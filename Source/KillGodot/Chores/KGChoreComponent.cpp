#include "Chores/KGChoreComponent.h"

#include "Character/KGCharacter.h"
#include "Chores/KGChoreFx.h"
#include "Chores/UI/KGMinigame.h"
#include "Chores/UI/SKGChorePanel.h"
#include "Combat/KGHealthComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerController.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleListGenerator.h"
#include "World/KGTaskStation.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"

namespace KGChoreComp
{
	bool GAutoWin = false;
}

UKGChoreComponent::UKGChoreComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UKGChoreComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKGChoreComponent, bWorking);
	DOREPLIFETIME_CONDITION(UKGChoreComponent, Saved, COND_OwnerOnly);
}

UKGChoreComponent* UKGChoreComponent::FindFor(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UKGChoreComponent>() : nullptr;
}

AKGCharacter* UKGChoreComponent::GetCharacter() const
{
	return Cast<AKGCharacter>(GetOwner());
}

AKGPlayerState* UKGChoreComponent::GetPlayerState() const
{
	const AKGCharacter* Char = GetCharacter();
	return Char ? Char->GetPlayerState<AKGPlayerState>() : nullptr;
}

bool UKGChoreComponent::IsImpatient() const
{
	const AKGPlayerState* PS = GetPlayerState();
	const FKGRoleInfo* Role = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId())
	                             : nullptr;
	return Role && Role->GetAlignment() == EKGAlignment::Impatient;
}

void UKGChoreComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// Session tokens only need to differ between sessions (anti-replay), not to be secret or reproducible.
		Rng.Reseed(uint64(GetUniqueID()) * 0x9E3779B97F4A7C15ull ^ FPlatformTime::Cycles64());
		if (AKGCharacter* Char = GetCharacter(); Char && Char->GetHealth())
		{
			Char->GetHealth()->OnHealthChanged.AddDynamic(this, &UKGChoreComponent::HandleHealthChanged);
		}
	}
}

void UKGChoreComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HidePanel();
	Super::EndPlay(EndPlayReason);
}

void UKGChoreComponent::HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (Delta < 0.0f)
	{
		bHitThisTick = true;
	}
}

int32 UKGChoreComponent::GetSavedStage(FName ChoreId) const
{
	const FKGChoreProgress* Found = Saved.FindByPredicate([ChoreId](const FKGChoreProgress& P) { return P.ChoreId == ChoreId; });
	return Found ? Found->Stage : 0;
}

void UKGChoreComponent::SetSaved(FName ChoreId, int32 Stage)
{
	const int32 Index = Saved.IndexOfByPredicate([ChoreId](const FKGChoreProgress& P) { return P.ChoreId == ChoreId; });
	if (Stage <= 0)
	{
		if (Index != INDEX_NONE)
		{
			Saved.RemoveAt(Index);
		}
		return;
	}
	if (Index == INDEX_NONE)
	{
		Saved.Add({ChoreId, Stage});
	}
	else
	{
		Saved[Index].Stage = Stage;
	}
}

// ---- server ------------------------------------------------------------------------------------------------------------

bool UKGChoreComponent::AuthOpen(FName ChoreId, AKGTaskStation* Station, bool bDev)
{
	AKGCharacter* Char = GetCharacter();
	AKGPlayerState* PS = GetPlayerState();
	const AKGGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKGGameState>() : nullptr;
	const FKGChoreDef* Def = FKGChoreCatalog::Find(ChoreId);
	if (!Char || !Char->HasAuthority() || !PS || Char->IsDead() || !Def || !FKGMinigameFactory::Has(ChoreId))
	{
		return false;
	}
	if (!bDev && (!GS || !FKGChoreRules::PhaseAllowsChores(GS->GetPhase())))
	{
		return false;
	}
	const bool bOnList = PS->HasOpenTask(ChoreId);
	if (!bOnList && !bDev)
	{
		return false;
	}
	if (HasSession())
	{
		if (Session == ChoreId)
		{
			// Pressed E again on the same chore (or a lost open): resend, same session.
			ClientOpen(Session, SessionStage, SessionToken, bSessionFake, bSessionPractice);
			return true;
		}
		AuthClose(EKGChoreClose::Replaced, true);
	}

	Session = ChoreId;
	SessionWorldChore = NAME_None;
	SessionStage = FMath::Clamp(GetSavedStage(ChoreId), 0, Def->NumStages() - 1);
	SessionToken = Rng.RandRange(1, 0x3FFFFFFF);
	bSessionFake = IsImpatient();
	bSessionPractice = !bOnList;
	StageSeconds = 0.0f;
	bHitThisTick = false;
	Anchor = Char->GetActorLocation();
	SessionStation = Station;
	FxLocation = Station ? Station->GetActorLocation() : Anchor;
	if (!Station)
	{
		for (TActorIterator<AKGTaskStation> It(GetWorld()); It; ++It)
		{
			if (It->TaskId == ChoreId)
			{
				FxLocation = It->GetActorLocation();   // dev play anywhere: the effect still shows at the real station
				break;
			}
		}
	}
	bWorking = true;
	if (UKGEmoteComponent* Emote = Char->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_OPEN %s by %s stage=%d/%d token=%d fake=%d practice=%d visual=%d"), *ChoreId.ToString(),
	       *PS->GetPlayerName(), SessionStage, Def->NumStages(), SessionToken, bSessionFake ? 1 : 0, bSessionPractice ? 1 : 0,
	       Def->IsVisual() ? 1 : 0);
	ClientOpen(Session, SessionStage, SessionToken, bSessionFake, bSessionPractice);
	return true;
}

bool UKGChoreComponent::AuthOpenWorldStep(FName InPanelChore, int32 StartStage, FName WorldChore, const FVector& At)
{
	AKGCharacter* Char = GetCharacter();
	const AKGPlayerState* PS = GetPlayerState();
	const FKGChoreDef* Def = FKGChoreCatalog::Find(InPanelChore);
	if (!Char || !Char->HasAuthority() || !PS || Char->IsDead() || !Def || !FKGMinigameFactory::Has(InPanelChore))
	{
		return false;
	}
	if (HasSession())
	{
		if (Session == InPanelChore && SessionWorldChore == WorldChore)
		{
			ClientOpen(Session, SessionStage, SessionToken, bSessionFake, bSessionPractice);
			return true;
		}
		AuthClose(EKGChoreClose::Replaced, true);
	}
	Session = InPanelChore;
	SessionWorldChore = WorldChore;
	SessionStage = FMath::Clamp(FMath::Max(StartStage, GetSavedStage(InPanelChore)), 0, Def->NumStages() - 1);
	SessionToken = Rng.RandRange(1, 0x3FFFFFFF);
	bSessionFake = IsImpatient();
	bSessionPractice = false;
	StageSeconds = 0.0f;
	bHitThisTick = false;
	Anchor = Char->GetActorLocation();
	SessionStation.Reset();
	FxLocation = At;
	bWorking = true;
	if (UKGEmoteComponent* Emote = Char->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_OPEN %s by %s stage=%d/%d token=%d fake=%d practice=0 visual=0 world=%s"), *InPanelChore.ToString(),
	       *PS->GetPlayerName(), SessionStage, Def->NumStages(), SessionToken, bSessionFake ? 1 : 0, *WorldChore.ToString());
	ClientOpen(Session, SessionStage, SessionToken, bSessionFake, bSessionPractice);
	return true;
}

EKGChoreVerdict UKGChoreComponent::AuthStageDone(FName ChoreId, int32 Stage, int32 Token)
{
	const FKGChoreDef* Def = FKGChoreCatalog::Find(Session);
	const EKGChoreVerdict Verdict = Def ? FKGChoreRules::CheckStage(*Def, ChoreId, Stage, Token, Session, SessionStage, SessionToken, StageSeconds)
	                                    : EKGChoreVerdict::NoSession;
	const AKGPlayerState* PS = GetPlayerState();
	if (Verdict != EKGChoreVerdict::Accepted)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_REJECT %s by %s stage=%d (session %s stage=%d) t=%.2fs floor=%.2fs verdict=%s"),
		       *ChoreId.ToString(), PS ? *PS->GetPlayerName() : TEXT("?"), Stage, *Session.ToString(), SessionStage, StageSeconds,
		       Def && Def->StageMinSeconds.IsValidIndex(SessionStage) ? Def->StageMinSeconds[SessionStage] : 0.0f,
		       FKGChoreRules::VerdictName(Verdict));
		if (Verdict == EKGChoreVerdict::TooFast)
		{
			StageSeconds = 0.0f;   // the client replays the stage from scratch
		}
		if (Verdict != EKGChoreVerdict::NoSession)
		{
			ClientReject(ChoreId, SessionStage, static_cast<uint8>(Verdict));
		}
		return Verdict;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_STAGE %s by %s stage=%d/%d t=%.2fs"), *ChoreId.ToString(), PS ? *PS->GetPlayerName() : TEXT("?"),
	       Stage + 1, Def->NumStages(), StageSeconds);
	++SessionStage;
	StageSeconds = 0.0f;
	if (SessionStage >= Def->NumStages())
	{
		CompleteSession();
	}
	else
	{
		SetSaved(ChoreId, SessionStage);
	}
	return Verdict;
}

void UKGChoreComponent::CompleteSession()
{
	const FName Id = Session;
	const FKGChoreDef* Def = FKGChoreCatalog::Find(Id);
	AKGPlayerState* PS = GetPlayerState();
	SetSaved(Id, 0);
	if (!SessionWorldChore.IsNone())
	{
		// SPRINT-016: a step of a world chore - the world chore advances, nothing is credited here.
		const FName World = SessionWorldChore;
		++CompletedCount;
		UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_DONE %s by %s fake=%d practice=0 counted=0 visual=0 world=%s"), *Id.ToString(),
		       PS ? *PS->GetPlayerName() : TEXT("?"), bSessionFake ? 1 : 0, *World.ToString());
		Session = NAME_None;
		SessionWorldChore = NAME_None;
		SessionStation.Reset();
		bWorking = false;
		if (UKGWorldChoreComponent* WorldChores = UKGWorldChoreComponent::FindFor(GetOwner()))
		{
			WorldChores->AuthPanelStepDone(World);
		}
		return;
	}
	bool bCounted = false;
	if (!bSessionPractice && PS && PS->CompleteTask(Id))
	{
		if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
		{
			GM->OnTaskCompleted(PS, Id);   // counts nothing for the Impatient (faked)
		}
		bCounted = !bSessionFake;
	}
	// Visual chores change the world for everyone - the proof of innocence. A fake never plays it.
	const bool bVisual = Def && Def->IsVisual() && !bSessionFake;
	if (bVisual)
	{
		if (AKGChoreFx* Fx = AKGChoreFx::Get(GetWorld(), true))
		{
			Fx->AuthTrigger(Id, Def->Fx, FxLocation, SessionStation.IsValid() ? SessionStation->GetActorRotation() : FRotator::ZeroRotator);
		}
	}
	++CompletedCount;
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_DONE %s by %s fake=%d practice=%d counted=%d visual=%d"), *Id.ToString(),
	       PS ? *PS->GetPlayerName() : TEXT("?"), bSessionFake ? 1 : 0, bSessionPractice ? 1 : 0, bCounted ? 1 : 0, bVisual ? 1 : 0);
	Session = NAME_None;
	SessionStation.Reset();
	bWorking = false;
}

void UKGChoreComponent::AuthClose(EKGChoreClose Reason, bool bTellOwner)
{
	if (!HasSession())
	{
		return;
	}
	const AKGPlayerState* PS = GetPlayerState();
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_CLOSE %s by %s reason=%s saved_stage=%d"), *Session.ToString(),
	       PS ? *PS->GetPlayerName() : TEXT("?"), FKGChoreRules::CloseName(Reason), GetSavedStage(Session));
	if (bTellOwner)
	{
		ClientClose(Session, static_cast<uint8>(Reason));
	}
	Session = NAME_None;
	SessionWorldChore = NAME_None;
	SessionStation.Reset();
	bWorking = false;
}

void UKGChoreComponent::TickServer(float DeltaTime)
{
	if (!HasSession())
	{
		bHitThisTick = false;
		return;
	}
	StageSeconds += DeltaTime;
	const AKGCharacter* Char = GetCharacter();
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	EKGChoreClose Reason = EKGChoreClose::Completed;
	if (!Char || Char->IsDead())
	{
		Reason = EKGChoreClose::Died;
	}
	else if (GS && !FKGChoreRules::PhaseAllowsChores(GS->GetPhase()) && !bSessionPractice)
	{
		Reason = EKGChoreClose::Phase;
	}
	else if (GS && (GS->GetPhase() == EKGPhase::Meeting || GS->GetPhase() == EKGPhase::Trial))
	{
		Reason = EKGChoreClose::Phase;   // even dev practice closes for the meeting
	}
	else if (bHitThisTick)
	{
		Reason = EKGChoreClose::Hit;
	}
	else if (FVector::DistSquared2D(Char->GetActorLocation(), Anchor) > FMath::Square(FKGChoreRules::MaxDriftCm) ||
	         FMath::Abs(Char->GetActorLocation().Z - Anchor.Z) > 200.0f || Char->GetVelocity().Size2D() > 450.0f)
	{
		Reason = EKGChoreClose::Moved;
	}
	bHitThisTick = false;
	if (Reason != EKGChoreClose::Completed)
	{
		AuthClose(Reason, true);
	}
}

void UKGChoreComponent::ServerStageDone_Implementation(FName ChoreId, int32 Stage, int32 Token)
{
	AuthStageDone(ChoreId, Stage, Token);
}

void UKGChoreComponent::ServerLeave_Implementation(FName ChoreId, int32 Token)
{
	if (Session == ChoreId && Token == SessionToken)
	{
		AuthClose(EKGChoreClose::Left, false);
	}
}

// ---- owning client -----------------------------------------------------------------------------------------------------

void UKGChoreComponent::SetAutoWin(bool bOn)
{
	KGChoreComp::GAutoWin = bOn;
}

bool UKGChoreComponent::IsAutoWin()
{
	return KGChoreComp::GAutoWin;
}

bool UKGChoreComponent::IsPanelOpen() const
{
	return Panel.IsValid() && !Panel->IsClosing();
}

void UKGChoreComponent::ClientOpen_Implementation(FName ChoreId, int32 Stage, int32 Token, bool bFake, bool bPractice)
{
	ShowPanel(ChoreId, Stage, Token, bFake, bPractice);
}

void UKGChoreComponent::ShowPanel(FName ChoreId, int32 Stage, int32 Token, bool bFake, bool bPractice)
{
	AKGCharacter* Char = GetCharacter();
	APlayerController* PC = Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || !FSlateApplication::IsInitialized())
	{
		return;   // bots / the server copy of a remote player / headless tests
	}
	if (Panel.IsValid())
	{
		if (PanelChore == ChoreId && PanelToken == Token && !Panel->IsClosing())
		{
			return;   // the server resent the same session
		}
		HidePanel();
	}
	PanelChore = ChoreId;
	PanelToken = Token;
	Panel = SNew(SKGChorePanel)
		.ChoreId(ChoreId)
		.Seed(Token)
		.StartStage(Stage)
		.bFake(bFake)
		.bPractice(bPractice)
		.SoundContext(this)
		.OnStageDone_UObject(this, &UKGChoreComponent::HandlePanelStage)
		.OnClosed_UObject(this, &UKGChoreComponent::HandlePanelClosed);
	Panel->SetAutoPlay(IsAutoWin());
	PanelEntry = KGMenu::AddToViewport(PC, Panel.ToSharedRef(), 35);
	if (!PanelEntry.IsShown())
	{
		Panel.Reset();
		return;
	}
	KGMenu::SetMenuInput(PC, Panel);
	bFlushKeys = true;
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_PANEL open %s stage=%d fake=%d practice=%d auto=%d"), *ChoreId.ToString(), Stage,
	       bFake ? 1 : 0, bPractice ? 1 : 0, IsAutoWin() ? 1 : 0);
}

void UKGChoreComponent::HidePanel()
{
	if (!Panel.IsValid() && !PanelEntry.IsShown())
	{
		return;
	}
	KGMenu::RemoveFromViewport(PanelEntry);
	Panel.Reset();
	PanelChore = NAME_None;
	const AKGCharacter* Char = GetCharacter();
	APlayerController* PC = Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	if (PC && PC->IsLocalController() && !(KGPC && (KGPC->IsPauseMenuOpen() || KGPC->IsLobbyScreenShown())))
	{
		KGMenu::SetGameInput(PC);
	}
}

void UKGChoreComponent::HandlePanelStage(int32 Stage)
{
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_PANEL stage %s %d solved"), *PanelChore.ToString(), Stage);
	ServerStageDone(PanelChore, Stage, PanelToken);
}

void UKGChoreComponent::HandlePanelClosed(EKGChoreClose Reason)
{
	// Called from the panel's own tick: tell the server now, drop the widget next component tick.
	if (Reason == EKGChoreClose::Left)
	{
		ServerLeave(PanelChore, PanelToken);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_PANEL closed %s reason=%s"), *PanelChore.ToString(), FKGChoreRules::CloseName(Reason));
	bPendingHide = true;
}

void UKGChoreComponent::ClientReject_Implementation(FName ChoreId, int32 Stage, uint8 Verdict)
{
	if (!Panel.IsValid() || PanelChore != ChoreId)
	{
		return;
	}
	const EKGChoreVerdict V = static_cast<EKGChoreVerdict>(Verdict);
	Panel->ServerRejected(Stage, V == EKGChoreVerdict::TooFast ? FString(TEXT("Too quick to be real - once more!"))
	                                                           : FString(TEXT("Lost track - try that step again")));
}

void UKGChoreComponent::ClientClose_Implementation(FName ChoreId, uint8 Reason)
{
	if (Panel.IsValid() && PanelChore == ChoreId)
	{
		Panel->ForceClose(static_cast<EKGChoreClose>(Reason));
	}
}

void UKGChoreComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (Panel.IsValid() || bPendingHide)
	{
		const AKGCharacter* Char = GetCharacter();
		APlayerController* PC = Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
		if (bFlushKeys && PC)
		{
			bFlushKeys = false;
			PC->FlushPressedKeys();   // E (and any held movement key) must not stay "down" under the panel
		}
		if (bPendingHide || !PC || (Char && Char->IsDead() && Panel.IsValid() && !Panel->IsClosing()))
		{
			if (!bPendingHide && Panel.IsValid() && Char && Char->IsDead())
			{
				Panel->ForceClose(EKGChoreClose::Died);
			}
			else
			{
				bPendingHide = false;
				HidePanel();
			}
		}
		else if (Panel.IsValid())
		{
			if (Panel->IsAutoPlay() != IsAutoWin())
			{
				Panel->SetAutoPlay(IsAutoWin());
			}
			if (!FApp::CanEverRender())
			{
				Panel->ManualTick(DeltaTime);   // -nullrhi smoke tests: Slate does not tick the viewport there
			}
		}
	}
}
