#include "Tabletop/KGTabletopSubsystem.h"

#include "Character/KGCharacter.h"
#include "Core/KGPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Tabletop/KGBoardTable.h"
#include "Tabletop/KGTableRules.h"
#include "EngineUtils.h"
#include "Tabletop/KGTabletopRPCComponent.h"
#include "Tabletop/SKGTablePanel.h"
#include "World/KGSeat.h"

namespace KGTabletopSubsystemPrivate
{
	constexpr float SpectateMetres = 3.0f;
	constexpr int32 PanelZOrder = 20;
}

UKGTabletopSubsystem* UKGTabletopSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGTabletopSubsystem>() : nullptr;
}

bool UKGTabletopSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGTabletopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	View = MakeShared<FKGTableView>();
}

void UKGTabletopSubsystem::Deinitialize()
{
	ClosePanel();
	Super::Deinitialize();
}

TStatId UKGTabletopSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGTabletopSubsystem, STATGROUP_Tickables);
}

void UKGTabletopSubsystem::EnsurePlayerComponents(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || PlayerState->IsActorBeingDestroyed() || !PlayerState->IsA<AKGPlayerState>())
	{
		return;
	}
	if (PlayerState->FindComponentByClass<UKGTabletopRPCComponent>())
	{
		return;
	}
	UActorComponent* Component = NewObject<UActorComponent>(PlayerState, UKGTabletopRPCComponent::StaticClass(), TEXT("KGTabletopRelay"));
	Component->SetIsReplicated(true);
	PlayerState->AddInstanceComponent(Component);
	Component->RegisterComponent();
}

void UKGTabletopSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		TickServer(DeltaTime);
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		TickUI(DeltaTime);
	}
	TickSmoke(DeltaTime);
}

void UKGTabletopSubsystem::TickServer(float DeltaTime)
{
	UWorld* World = GetWorld();
	SweepSeconds += DeltaTime;
	if (SweepSeconds >= 0.5f)
	{
		SweepSeconds = 0.0f;
		if (const AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				EnsurePlayerComponents(PS);
			}
		}
	}
	// Bot budget: shared by every table whose bot is to move.
	TArray<AKGBoardTable*> Tables;
	AKGBoardTable::GetAll(World, Tables);
	TArray<AKGBoardTable*> Thinking;
	for (AKGBoardTable* T : Tables)
	{
		if (T->IsPlaying() && T->IsBot(T->GetBoard().Side))
		{
			Thinking.Add(T);
		}
	}
	if (Thinking.Num() > 0)
	{
		const double Slice = BotBudgetSeconds() / Thinking.Num();
		for (AKGBoardTable* T : Thinking)
		{
			T->ServerTickBot(Slice);
		}
	}
}

// ---- local UI -----------------------------------------------------------------------------------------------------

AKGBoardTable* UKGTabletopSubsystem::GetPanelTable() const
{
	return PanelTable.Get();
}

void UKGTabletopSubsystem::OpenPanel(APlayerController* PC, bool bStrip)
{
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (!Viewport)
	{
		return;
	}
	ClosePanel();
	View->bStrip = bStrip;
	Panel = SNew(SKGTablePanel)
		.View(View)
		.OnMove(FKGOnTableMove::CreateUObject(this, &UKGTabletopSubsystem::HandlePanelMove))
		.OnAction(FKGOnTableAction::CreateUObject(this, &UKGTabletopSubsystem::HandlePanelAction));
	Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), KGTabletopSubsystemPrivate::PanelZOrder);
	PanelPC = PC;
	bPanelStrip = bStrip;
	if (!bStrip)
	{
		// Mouse on the board, keys with the character (E / Space stand up, T chat, V push to talk).
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->FlushPressedKeys();
		bPanelInputMode = true;
	}
}

void UKGTabletopSubsystem::ClosePanel()
{
	if (Panel.IsValid())
	{
		if (APlayerController* PC = PanelPC.Get())
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UGameViewportClient* Viewport = LocalPlayer->ViewportClient.Get())
				{
					Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
				}
			}
			if (bPanelInputMode)
			{
				PC->SetInputMode(FInputModeGameOnly());
				PC->SetShowMouseCursor(false);
			}
		}
	}
	Panel.Reset();
	PanelTable = nullptr;
	PanelPC = nullptr;
	bPanelInputMode = false;
}

void UKGTabletopSubsystem::FillView(AKGBoardTable* Table, const AKGCharacter* Me, APlayerController* PC)
{
	FKGTableView& V = *View;
	V.Board = Table->GetBoard();
	V.Status = Table->GetStatus();
	V.Reason = Table->GetEndReason();
	V.MyColour = Table->ColourOfCharacter(Me);
	V.Clock[0] = Table->GetClockSeconds(0);
	V.Clock[1] = Table->GetClockSeconds(1);
	V.bTimed = Table->GetNet().Clock != EKGTableClock::Untimed;
	V.Names[0] = Table->GetPlayerName(0);
	V.Names[1] = Table->GetPlayerName(1);
	V.Place = Table->PlaceName.ToString();
	V.ClockName = FKGTableRules::ClockText(Table->GetNet().Clock);
	V.LastFrom = Table->GetLastFrom();
	V.LastTo = Table->GetLastTo();
	V.DrawOffers = Table->GetNet().DrawOffers;
	V.bOpen = Table->IsOpenPhase();
	V.bBot[0] = Table->IsBot(0);
	V.bBot[1] = Table->IsBot(1);
	if (const UKGTabletopRPCComponent* Relay = UKGTabletopRPCComponent::FindFor(PC))
	{
		V.Notice = Relay->GetLastNotice();
		V.NoticeTime = Relay->GetLastNoticeTime();
	}
}

void UKGTabletopSubsystem::TickUI(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
	{
		return;
	}
	const AKGCharacter* Me = Cast<AKGCharacter>(PC->GetPawn());
	AKGBoardTable* Table = Me ? AKGBoardTable::FindTableOf(Me) : nullptr;
	bool bStrip = false;
	if (!Table && Me)
	{
		AKGBoardTable* Near = AKGBoardTable::FindNearest(World, Me->GetActorLocation(), KGTabletopSubsystemPrivate::SpectateMetres * 100.0f);
		if (Near && Near->GetStatus() != EKGTableStatus::Waiting)
		{
			Table = Near;
			bStrip = true;
		}
	}
	if (!Table)
	{
		if (Panel.IsValid())
		{
			ClosePanel();
		}
		return;
	}
	if (!Panel.IsValid() || PanelTable.Get() != Table || bPanelStrip != bStrip)
	{
		OpenPanel(PC, bStrip);
		PanelTable = Table;
	}
	FillView(Table, Me, PC);
}

void UKGTabletopSubsystem::HandlePanelMove(const FString& MoveText)
{
	AKGBoardTable* Table = PanelTable.Get();
	UKGTabletopRPCComponent* Relay = UKGTabletopRPCComponent::FindFor(PanelPC.Get());
	if (Table && Relay)
	{
		Relay->RequestMove(Table, MoveText);
	}
}

void UKGTabletopSubsystem::HandlePanelAction(uint8 Action)
{
	AKGBoardTable* Table = PanelTable.Get();
	UKGTabletopRPCComponent* Relay = UKGTabletopRPCComponent::FindFor(PanelPC.Get());
	if (Table && Relay)
	{
		Relay->RequestAction(Table, static_cast<EKGTableAction>(Action));
	}
}

// ---- -KGTableSmoke: two-process scripted game (Tools/Unreal/kg_table_smoke.ps1) --------------------------------------
// Host: spawns a table in front of itself, seats itself White and the client Black, plays Scholar's mate for White,
// and tries one out-of-turn move and one move by an unseated player. Client: plays Black over the RPC relay after
// first sending an out-of-turn move and an illegal move (both must be refused). Both log the final position.

void UKGTabletopSubsystem::TickSmoke(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGTableSmoke"));
	UWorld* World = GetWorld();
	if (!bSmoke || bSmokeDone || !World)
	{
		return;
	}
	const bool bHost = World->GetNetMode() != NM_Client;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");
	APlayerController* LocalPC = World->GetFirstPlayerController();
	AKGCharacter* Me = LocalPC ? Cast<AKGCharacter>(LocalPC->GetPawn()) : nullptr;
	AKGCharacter* Other = nullptr;
	int32 Humans = 0;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			AKGCharacter* Body = PS && !PS->IsABot() ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
			if (Body)
			{
				++Humans;
				if (Body != Me)
				{
					Other = Body;
				}
			}
		}
	}
	auto Finish = [this, Machine](const TCHAR* Why)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_DONE %s %s"), Machine, Why);
		bSmokeDone = true;
	};
	if (SmokeClock < 0.0f)
	{
		if (Humans < 2 || !Me)
		{
			return;
		}
		SmokeClock = 0.0f;
		UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE %s start"), Machine);
	}
	SmokeClock += DeltaTime;
	if (SmokeClock > 180.0f)
	{
		Finish(TEXT("timeout"));
		return;
	}
	static const TCHAR* WhiteMoves[4] = {TEXT("e2e4"), TEXT("f1c4"), TEXT("d1h5"), TEXT("h5f7")};
	static const TCHAR* BlackMoves[3] = {TEXT("e7e5"), TEXT("b8c6"), TEXT("g8f6")};

	if (bHost)
	{
		if (SmokeStep == 0 && SmokeClock >= 2.0f && Other)
		{
			const FVector At = Me->GetActorLocation() + Me->GetActorForwardVector() * 250.0f - FVector(0, 0, 90.0f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKGBoardTable* Table = World->SpawnActor<AKGBoardTable>(AKGBoardTable::StaticClass(), At, FRotator(0, Me->GetActorRotation().Yaw, 0), Params);
			if (!Table)
			{
				Finish(TEXT("no table"));
				return;
			}
			Table->ServerReset(EKGTableGame::Chess, EKGTableClock::Blitz);
			SmokeTable = Table;
			FString Err;
			const bool bUnseated = !Table->ServerTryMove(Other->GetPlayerState(), TEXT("e2e4"), Err);
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Host unseated=%s why=%s"), bUnseated ? TEXT("rejected") : TEXT("ACCEPTED"), *Err);
			const bool bA = Table->ServerSeat(Me, 0);
			const bool bB = Table->ServerSeat(Other, 1);
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Host table=%s seated white=%d black=%d"), *Table->GetName(), bA ? 1 : 0, bB ? 1 : 0);
			// Hold White's first move so the client's out-of-turn probe (sent as soon as it sees Playing at ply 0) reaches
			// an untouched board; otherwise e7e5 can race e2e4 and be accepted as Black's real reply.
			SmokeNextSend = SmokeClock + 4.0f;
			SmokeStep = 1;
			return;
		}
		AKGBoardTable* Table = SmokeTable.Get();
		if (!Table || SmokeStep == 0)
		{
			return;
		}
		if (Table->GetStatus() == EKGTableStatus::Playing)
		{
			const FKGBoardState& B = Table->GetBoard();
			if (B.Side == 1 && SmokeBadSent == 0)
			{
				FString Err;
				const bool bRejected = !Table->ServerTryMove(Me->GetPlayerState(), TEXT("a2a3"), Err);
				UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Host outofturn=%s why=%s"), bRejected ? TEXT("rejected") : TEXT("ACCEPTED"), *Err);
				SmokeBadSent = 1;
			}
			const int32 Index = B.Ply / 2;
			if (B.Side == 0 && Index < 4 && SmokeSent < Index && SmokeClock >= SmokeNextSend)
			{
				FString Err;
				const bool bOk = Table->ServerTryMove(Me->GetPlayerState(), WhiteMoves[Index], Err);
				UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Host move=%s ok=%d why=%s"), WhiteMoves[Index], bOk ? 1 : 0, *Err);
				SmokeSent = Index;
				SmokeNextSend = SmokeClock + 0.5f;
			}
		}
		else if (Table->GetStatus() != EKGTableStatus::Waiting && Table->GetStatus() != EKGTableStatus::Adjourned)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_FINAL %s status=%s reason=%s moves=%d fen=\"%s\""), Machine,
			       FKGTableRules::StatusText(Table->GetStatus()), FKGTableRules::ReasonText(Table->GetEndReason()), Table->GetMoveCount(), *Table->GetFen());
			Finish(TEXT("ok"));
		}
		return;
	}

	// ---- client ----
	AKGBoardTable* Table = AKGBoardTable::FindTableOf(Me);
	UKGTabletopRPCComponent* Relay = UKGTabletopRPCComponent::FindFor(LocalPC);
	if (SmokeClock >= SmokeNextDiag)
	{
		// Progress line while waiting (diagnoses a stuck client without a debugger): which lookup is still missing.
		SmokeNextDiag = SmokeClock + 5.0f;
		int32 NumTables = 0;
		for (TActorIterator<AKGBoardTable> It(World); It; ++It)
		{
			++NumTables;
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Client wait t=%.0f tables=%d table=%s relay=%d seat=%s colour=%d status=%s"),
		       SmokeClock, NumTables, Table ? *Table->GetName() : TEXT("none"), Relay ? 1 : 0,
		       AKGSeat::FindSeatOf(Me) ? *AKGSeat::FindSeatOf(Me)->GetName() : TEXT("none"),
		       Table ? Table->ColourOfCharacter(Me) : -1, Table ? FKGTableRules::StatusText(Table->GetStatus()) : TEXT("-"));
	}
	if (!Table || !Relay)
	{
		return;
	}
	if (Table->ColourOfCharacter(Me) != 1)
	{
		return;
	}
	if (Table->GetStatus() == EKGTableStatus::Playing)
	{
		const FKGBoardState& B = Table->GetBoard();
		if (SmokeBadSent == 0 && B.Side == 0 && B.Ply == 0)
		{
			Relay->RequestMove(Table, TEXT("e7e5"));   // out of turn
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Client sent out-of-turn e7e5"));
			SmokeBadSent = 1;
		}
		if (SmokeBadSent == 1 && B.Side == 1)
		{
			Relay->RequestMove(Table, TEXT("e7e4"));   // illegal
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Client sent illegal e7e4"));
			SmokeBadSent = 2;
			SmokeNextSend = SmokeClock + 1.0f;
		}
		const int32 Index = (B.Ply - 1) / 2;
		if (B.Side == 1 && SmokeBadSent == 2 && Index >= 0 && Index < 3 && SmokeSent < Index && SmokeClock >= SmokeNextSend)
		{
			Relay->RequestMove(Table, BlackMoves[Index]);
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_SMOKE Client move=%s ply=%d"), BlackMoves[Index], B.Ply);
			SmokeSent = Index;
			SmokeNextSend = SmokeClock + 0.5f;
		}
	}
	else if (Table->GetStatus() != EKGTableStatus::Waiting && Table->GetStatus() != EKGTableStatus::Adjourned)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_FINAL %s status=%s reason=%s moves=%d fen=\"%s\" notice=\"%s\""), Machine,
		       FKGTableRules::StatusText(Table->GetStatus()), FKGTableRules::ReasonText(Table->GetEndReason()), Table->GetMoveCount(),
		       *Table->GetFen(), *Relay->GetLastNotice());
		Finish(TEXT("ok"));
	}
#endif
}
