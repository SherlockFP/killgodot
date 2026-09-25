#include "UI/Reveal/KGRevealSubsystem.h"

#include "Core/KGGameState.h"
#include "Core/KGLobbyState.h"
#include "Core/KGPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "UI/Reveal/KGPseudonyms.h"
#include "UI/Reveal/KGRevealComponent.h"
#include "UI/Reveal/KGRoleCardText.h"
#include "UI/Reveal/KGStreamerMode.h"
#include "UI/Reveal/SKGRoleReveal.h"

namespace KGRevealSubsystemPrivate
{
	constexpr int32 RevealZOrder = 36;   // over the lobby room (30) and chores (35), under the pause menu (40)

	const FKGRoleInfo* FindRoleInfo(FName RoleId)
	{
		return FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId);
	}

	FString AlignmentTag(EKGAlignment Align)
	{
		return Align == EKGAlignment::Impatient ? TEXT("Impatient") : Align == EKGAlignment::Neutral ? TEXT("Neutral") : TEXT("Town");
	}
}

bool UKGRevealSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGRevealSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKGRevealSubsystem::HandleActorSpawned));
}

void UKGRevealSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	HideOverlay();
	Pending.Reset();
	Super::Deinitialize();
}

TStatId UKGRevealSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGRevealSubsystem, STATGROUP_Tickables);
}

void UKGRevealSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (APlayerController* PC = Cast<APlayerController>(Actor))
	{
		Pending.Add(PC);   // next tick: the player state is not there yet
	}
}

bool UKGRevealSubsystem::IsOverlayShown(const UWorld* World)
{
	const UKGRevealSubsystem* Self = World ? World->GetSubsystem<UKGRevealSubsystem>() : nullptr;
	return Self && Self->bActive;
}

void UKGRevealSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->GetGameState<AKGGameState>())
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		TickServer(DeltaTime);
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		TickLocal(DeltaTime);
	}
}

// --- Server -----------------------------------------------------------------------------------------------------------

void UKGRevealSubsystem::TickServer(float DeltaTime)
{
	UWorld* World = GetWorld();
	for (const TWeakObjectPtr<APlayerController>& PC : Pending)
	{
		UKGRevealComponent::Ensure(PC.Get());
	}
	Pending.RemoveAll([](const TWeakObjectPtr<APlayerController>& PC)
	{
		return !PC.IsValid() || UKGRevealComponent::Find(PC.Get()) != nullptr;
	});
	SweepSeconds += DeltaTime;
	if (SweepSeconds >= 1.0f)
	{
		SweepSeconds = 0.0f;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			UKGRevealComponent::Ensure(It->Get());
		}
	}
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	const bool bReveal = GS->GetPhase() == EKGPhase::RoleReveal;
	if (bReveal && !bServerInReveal)
	{
		// The game mode dealt the roles in EnterPhase(RoleReveal) this frame or the last one.
		UKGRevealComponent::AuthDeal(World);
		ReadySweep = 0.0f;
	}
	bServerInReveal = bReveal;
	if (bReveal)
	{
		ReadySweep += DeltaTime;
		if (ReadySweep >= 0.5f)
		{
			ReadySweep = 0.0f;
			UKGRevealComponent::AuthUpdateReady(World);   // someone left: the others may all be ready now
		}
	}
}

// --- Local ceremony ---------------------------------------------------------------------------------------------------

FKGRevealView UKGRevealSubsystem::BuildView() const
{
	FKGRevealView View;
	const UWorld* World = GetWorld();
	const APlayerController* PC = LocalPC.Get();
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (!PC || !GS)
	{
		return View;
	}
	const bool bReveal = GS->GetPhase() == EKGPhase::RoleReveal;
	const AKGPlayerState* Me = PC->GetPlayerState<AKGPlayerState>();
	View.RoleId = bReveal && Me ? Me->GetPrivateRoleId() : NAME_None;
	View.Players = GS->PlayerArray.Num();
	View.ServerElapsed = bReveal ? GS->Clock.PhaseDuration - GS->Clock.RemainingSeconds : -1.0f;
	View.ServerRemaining = bReveal ? GS->Clock.RemainingSeconds : -1.0f;
	View.bEnding = bEnding;
	View.bStreamer = KGStreamer::IsEnabled();
	View.PeekKey = KGStreamer::GetPeekKeyLabel().ToString();
	if (const FKGRoleInfo* Info = KGRevealSubsystemPrivate::FindRoleInfo(View.RoleId))
	{
		View.bHasTeam = KGRoleCard::IsTeamFaction(Info->Faction);
	}
	if (const UKGRevealComponent* Comp = UKGRevealComponent::Find(PC))
	{
		for (const FKGRevealMate& Mate : Comp->GetMates())
		{
			FKGRevealMateView& Out = View.Mates.AddDefaulted_GetRef();
			Out.Name = Mate.Player ? KGStreamer::DisplayName(Mate.Player) : KGStreamer::DisplayNameOf(World, Mate.Name);
			Out.RoleId = Mate.RoleId;
		}
		View.bReady = Comp->IsReady();
		View.ReadyCount = Comp->GetReadyCount();
		View.HumanCount = FMath::Max(1, static_cast<int32>(Comp->GetHumanCount()));
	}
	return View;
}

bool UKGRevealSubsystem::PressReady()
{
	UKGRevealComponent* Comp = UKGRevealComponent::Find(LocalPC.Get());
	if (!bActive || bEnding || !Comp || LocalClock < KGReveal::ReadyFromSeconds)
	{
		return false;
	}
	if (!Comp->RequestReady())
	{
		return false;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL ready pressed at %.2fs"), LocalClock);
	return true;
}

void UKGRevealSubsystem::ShowOverlay(APlayerController* PC)
{
	if (!PC || !FApp::CanEverRender())
	{
		return;   // -nullrhi: the ceremony still runs (and logs), there is just nothing to draw
	}
	TWeakObjectPtr<UKGRevealSubsystem> WeakThis = this;
	Widget = SNew(SKGRoleReveal)
		.ViewProvider([WeakThis]()
		{
			return WeakThis.IsValid() ? WeakThis->BuildView() : FKGRevealView();
		});
	Entry = KGMenu::AddToViewport(PC, Widget.ToSharedRef(), KGRevealSubsystemPrivate::RevealZOrder);
}

void UKGRevealSubsystem::HideOverlay()
{
	KGMenu::RemoveFromViewport(Entry);
	Widget.Reset();
	if (APawn* Pawn = FrozenPawn.Get())
	{
		if (APlayerController* PC = LocalPC.Get())
		{
			Pawn->EnableInput(PC);
		}
	}
	FrozenPawn.Reset();
}

void UKGRevealSubsystem::LogBeats(const FKGRevealView& View)
{
	const int32 Stage = static_cast<int32>(KGReveal::StageAt(LocalClock));
	if (Stage != LoggedStage && !bEnding)
	{
		LoggedStage = Stage;
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL stage %s at %.2fs (server %.2fs)"), KGReveal::StageName(static_cast<KGReveal::EStage>(Stage)),
		       LocalClock, View.ServerElapsed);
	}
	if (!bLoggedRole && !View.RoleId.IsNone())
	{
		bLoggedRole = true;
		const FKGRoleInfo* Info = KGRevealSubsystemPrivate::FindRoleInfo(View.RoleId);
		// Owner-only check: how many OTHER players' secret roles does this machine know? Must be 0 on a client.
		int32 OthersKnown = 0;
		const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
		const APlayerController* PC = LocalPC.Get();
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			OthersKnown += PS && PS != PC->PlayerState && !PS->GetPrivateRoleId().IsNone() ? 1 : 0;
		}
		FString Mates;
		for (const FKGRevealMateView& Mate : View.Mates)
		{
			Mates += FString::Printf(TEXT(" [%s %s]"), *Mate.Name, *Mate.RoleId.ToString());
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL role %s alignment %s card '%s' mates %d:%s others_known %d net %s"),
		       *View.RoleId.ToString(), Info ? *KGRevealSubsystemPrivate::AlignmentTag(Info->GetAlignment()) : TEXT("?"), *KGRoleCard::Get(View.RoleId).Name,
		       View.Mates.Num(), *Mates, OthersKnown, GetWorld()->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"));
		if (KGStreamer::IsEnabled())
		{
			GEngine->Exec(GetWorld(), TEXT("kg.Streamer dump"));
		}
	}
}

void UKGRevealSubsystem::TickLocal(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	if (!PC)
	{
		return;
	}
	LocalPC = PC;
	const AKGLobbyState* Lobby = AKGLobbyState::Get(World);
	const EKGPhase Phase = GS->GetPhase();
	const bool bWant = Phase == EKGPhase::RoleReveal || (Lobby && Lobby->HasStarted() && Phase == EKGPhase::Lobby);
	if (bWant && !bActive)
	{
		bActive = true;
		bEnding = false;
		ActiveSeconds = 0.0f;
		EndingSeconds = 0.0f;
		LocalClock = 0.0f;
		LoggedStage = -1;
		bLoggedRole = false;
		bAutoReadySent = false;
		ShowOverlay(PC);
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL begin (%s, phase %s, %d seats, overlay %s)"),
		       World->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"), *UEnum::GetValueAsString(Phase),
		       GS->PlayerArray.Num(), Widget.IsValid() ? TEXT("shown") : TEXT("headless"));
	}
	if (!bActive)
	{
		return;
	}
	ActiveSeconds += DeltaTime;
	if (!bWant && !bEnding)
	{
		bEnding = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL end: phase %s after %.2fs on screen (ceremony clock %.2fs)"),
		       *UEnum::GetValueAsString(Phase), ActiveSeconds, LocalClock);
		// Hand the body back right away: the village fades in under the last frames of the card.
		if (APawn* Pawn = FrozenPawn.Get())
		{
			Pawn->EnableInput(PC);
		}
		FrozenPawn.Reset();
	}
	const FKGRevealView View = BuildView();
	if (Widget.IsValid())
	{
		LocalClock = Widget->GetTime();
	}
	else if (View.ServerElapsed >= 0.0f)
	{
		LocalClock = FMath::Max(LocalClock + DeltaTime, View.ServerElapsed);
		LocalClock = View.RoleId.IsNone() ? FMath::Min(LocalClock, KGReveal::DealEnd - 0.02f) : LocalClock;
	}
	LogBeats(View);
	if (bEnding)
	{
		EndingSeconds += DeltaTime;
		if (!Widget.IsValid() || Widget->IsFinished() || EndingSeconds > KGReveal::OutroSeconds + 0.5f)
		{
			HideOverlay();
			bActive = false;
		}
		return;
	}
	// Freeze the body under the card (no walking, jumping or swinging behind it).
	if (APawn* Pawn = PC->GetPawn(); Pawn && FrozenPawn.Get() != Pawn)
	{
		if (APawn* Old = FrozenPawn.Get())
		{
			Old->EnableInput(PC);
		}
		Pawn->DisableInput(PC);
		FrozenPawn = Pawn;
	}
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) ||
	    PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		PressReady();
	}
	static const bool bAutoReady = FParse::Param(FCommandLine::Get(), TEXT("KGRevealAutoReady"));
	if (bAutoReady && !bAutoReadySent && LocalClock >= KGReveal::FlipEnd + 1.0f)
	{
		bAutoReadySent = PressReady();
	}
}

// --- Screenshots (kg.UIShot reveal:<Role>:<stage>) --------------------------------------------------------------------

TSharedPtr<SWidget> UKGRevealSubsystem::MakeShotWidget(const FString& Spec, bool bStreamer)
{
	TArray<FString> Parts;
	Spec.ParseIntoArray(Parts, TEXT(":"), true);
	if (Parts.Num() < 2)
	{
		return nullptr;
	}
	const FName RoleId(*Parts[0]);
	const FKGRoleInfo* Info = KGRevealSubsystemPrivate::FindRoleInfo(RoleId);
	if (!Info)
	{
		return nullptr;
	}
	const FString Stage = Parts[1].ToLower();
	bool bReady = false;
	for (int32 Index = 2; Index < Parts.Num(); ++Index)
	{
		bReady |= Parts[Index].ToLower() == TEXT("ready");
		bStreamer |= Parts[Index].ToLower() == TEXT("streamer");   // file name carries the variant
	}
	static const TMap<FString, float> Times = {
		{TEXT("table"), KGReveal::TableEnd * 0.8f},
		{TEXT("shuffle"), KGReveal::TableEnd + (KGReveal::ShuffleEnd - KGReveal::TableEnd) * 0.5f * 0.5f},
		{TEXT("deal"), KGReveal::ShuffleEnd + (KGReveal::DealEnd - KGReveal::ShuffleEnd) * 0.55f},
		{TEXT("flip"), KGReveal::DealEnd + (KGReveal::FlipEnd - KGReveal::DealEnd) * 0.66f},
		{TEXT("role"), 7.4f}};
	const float* Time = Times.Find(Stage);
	if (!Time)
	{
		return nullptr;
	}
	FKGRevealView View;
	View.RoleId = RoleId;
	View.Players = 12;
	View.bHasTeam = KGRoleCard::IsTeamFaction(Info->Faction);
	View.HumanCount = 4;
	View.ReadyCount = bReady ? 2 : 1;
	View.bReady = bReady;
	View.ServerElapsed = *Time;
	View.ServerRemaining = KGReveal::PhaseSeconds - *Time;
	View.bStreamer = bStreamer;
	View.PeekKey = TEXT("Tab");
	if (View.bHasTeam)
	{
		// Sample accomplices (a 12-player list has three Clockbreakers).
		struct FMate { const TCHAR* Name; const TCHAR* Role; };
		const FMate Mates[] = {{TEXT("Netmaker Sevgi"), TEXT("Enforcer")}, {TEXT("Old Kemal"), TEXT("Framer")}};
		FKGPseudonyms Masks(0x5EEDull);
		for (const FMate& Mate : Mates)
		{
			if (FName(Mate.Role) == RoleId)
			{
				continue;
			}
			FKGRevealMateView& Out = View.Mates.AddDefaulted_GetRef();
			Out.Name = bStreamer ? Masks.Get(Mate.Name, Mate.Name) : FString(Mate.Name);
			Out.RoleId = Mate.Role;
		}
	}
	return SNew(SKGRoleReveal).StaticView(View).FixedTime(*Time + (bReady ? 0.8f : 0.0f));
}

// --- Console (dev) ----------------------------------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace KGRevealSubsystemPrivate
{
	FAutoConsoleCommandWithWorldAndArgs GKGRevealReadyCommand(
		TEXT("kg.Reveal.Ready"),
		TEXT("Role reveal: press 'ready' for the local player (same as Space on the card)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UKGRevealSubsystem* Self = World ? World->GetSubsystem<UKGRevealSubsystem>() : nullptr;
			UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL kg.Reveal.Ready -> %s"), Self && Self->PressReady() ? TEXT("sent") : TEXT("ignored"));
		}));
}
#endif
