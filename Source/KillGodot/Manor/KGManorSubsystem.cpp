#include "Manor/KGManorSubsystem.h"
#include "Character/KGCharacter.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Core/KGTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KillGodot.h"
#include "Manor/KGHiddenCompartment.h"
#include "Manor/KGSecretPassage.h"
#include "World/KGDoor.h"
#include "World/KGInteractable.h"

namespace KGManorPrivate
{
	constexpr float GateRefreshSecs = 0.5f;
	const FName WingGateTag(TEXT("KG_WingGate"));
	const TCHAR* MinPrefix = TEXT("KG_MinN_");

	/** The subsystem that owns FKGWorldChoreRules::DealFilter right now (several PIE worlds share the static). */
	UKGManorSubsystem* GFilterOwner = nullptr;
}

UKGManorSubsystem* UKGManorSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGManorSubsystem>() : nullptr;
}

bool UKGManorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKGManorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGManorSubsystem, STATGROUP_Tickables);
}

bool UKGManorSubsystem::IsAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

int32 UKGManorSubsystem::PlayerCount() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->PlayerArray.Num() : 0;
}

void UKGManorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ChoreDoneHandle = FKGWorldChoreRules::OnChoreDone.AddUObject(this, &UKGManorSubsystem::HandleChoreDone);
	// SPRINT-040 deal filter: MinPlayers against the game state of this world.
	TWeakObjectPtr<UKGManorSubsystem> Self(this);
	KGManorPrivate::GFilterOwner = this;
	FKGWorldChoreRules::DealFilter = [Self](const FKGWorldChoreDef& Def)
	{
		return !Self.IsValid() || PassesDeal(Def.MinPlayers, Self->PlayerCount());
	};
}

void UKGManorSubsystem::Deinitialize()
{
	FKGWorldChoreRules::OnChoreDone.Remove(ChoreDoneHandle);
	if (KGManorPrivate::GFilterOwner == this)
	{
		KGManorPrivate::GFilterOwner = nullptr;
		FKGWorldChoreRules::DealFilter = nullptr;
	}
	Super::Deinitialize();
}

int32 UKGManorSubsystem::MinPlayersOf(const TArray<FName>& Tags)
{
	const int32 PrefixLen = FCString::Strlen(KGManorPrivate::MinPrefix);
	for (const FName& Tag : Tags)
	{
		const FString S = Tag.ToString();
		if (S.StartsWith(KGManorPrivate::MinPrefix))
		{
			return FMath::Max(0, FCString::Atoi(*S.Mid(PrefixLen)));
		}
	}
	return 0;
}

void UKGManorSubsystem::SetGate(AKGDoor* Door, bool bLocked) const
{
	if (!Door)
	{
		return;
	}
	Door->FlushNetDormancy();
	if (bLocked)
	{
		Door->SetLocked(false);
		if (Door->IsOpen())
		{
			IKGInteractable::Execute_Interact(Door, nullptr);   // shut it first (a locked door will not move)
		}
		Door->SetLocked(true);
	}
	else
	{
		Door->SetLocked(false);
	}
	Door->ForceNetUpdate();
}

int32 UKGManorSubsystem::RefreshGates(bool bForce)
{
	if (!IsAuthority() || (bGatesFrozen && !bForce))
	{
		return 0;
	}
	const int32 Players = PlayerCount();
	int32 Locked = 0;
	int32 Gates = 0;
	for (TActorIterator<AKGDoor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(KGManorPrivate::WingGateTag))
		{
			continue;
		}
		++Gates;
		const int32 Need = MinPlayersOf(It->Tags);
		const bool bLock = Need > 0 && Players < Need;
		SetGate(*It, bLock);
		Locked += bLock ? 1 : 0;
	}
	if (Gates > 0 && Players != LastGatePlayers)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR gates players=%d locked=%d"), Players, Locked);
	}
	LastGatePlayers = Players;
	return Locked;
}

void UKGManorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAuthority() || bGatesFrozen)
	{
		return;
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (!GS)
	{
		return;
	}
	const EKGPhase Phase = GS->GetPhase();
	if (Phase != EKGPhase::Lobby && Phase != EKGPhase::Warmup)
	{
		bGatesFrozen = true;   // the match started: whatever is open stays open
		return;
	}
	GateAccum += DeltaTime;
	if (GateAccum >= KGManorPrivate::GateRefreshSecs)
	{
		GateAccum = 0.0f;
		if (PlayerCount() != LastGatePlayers)
		{
			RefreshGates(false);
		}
	}
}

void UKGManorSubsystem::OnSecretDiscovered(FName SecretId, AKGCharacter* By)
{
	AKGPlayerState* PS = By ? By->GetPlayerState<AKGPlayerState>() : nullptr;
	UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR secret %s discovered by %s"), *SecretId.ToString(), PS ? *PS->GetPlayerName() : TEXT("-"));
	UKGWorldChoreComponent* Chores = By ? UKGWorldChoreComponent::FindFor(By) : nullptr;
	if (!Chores || !PS)
	{
		return;
	}
	for (const FKGWorldChoreDef& Def : FKGWorldChoreCatalog::Get().Chores)
	{
		if (Def.SecretId != SecretId || PS->TaskIds.Contains(Def.Id))
		{
			continue;
		}
		if (Chores->AuthGive(Def.Id))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR secret chore %s given to %s"), *Def.Id.ToString(), *PS->GetPlayerName());
		}
	}
}

void UKGManorSubsystem::OnCompartmentOpened(FName CompartmentId, AKGCharacter* By)
{
	const AKGPlayerState* PS = By ? By->GetPlayerState<AKGPlayerState>() : nullptr;
	UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR compartment %s opened by %s"), *CompartmentId.ToString(), PS ? *PS->GetPlayerName() : TEXT("-"));
}

int32 UKGManorSubsystem::DiscoverSecret(FName SecretId, AKGCharacter* By)
{
	return IsAuthority() ? AKGSecretPassage::DiscoverAll(GetWorld(), SecretId, By) : 0;
}

bool UKGManorSubsystem::OpenCompartment(FName CompartmentId, AKGCharacter* By)
{
	AKGHiddenCompartment* C = IsAuthority() ? AKGHiddenCompartment::FindById(GetWorld(), CompartmentId) : nullptr;
	return C && C->AuthOpen(By);
}

void UKGManorSubsystem::HandleChoreDone(UKGWorldChoreComponent* Comp, FName Chore)
{
	if (!Comp || Comp->GetWorld() != GetWorld() || !IsAuthority())
	{
		return;
	}
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(Chore);
	if (!Def)
	{
		return;
	}
	AKGCharacter* By = Cast<AKGCharacter>(Comp->GetOwner());
	if (!Def->RewardSecret.IsNone())
	{
		const int32 Ends = DiscoverSecret(Def->RewardSecret, By);
		UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR reward chore=%s secret=%s ends=%d"), *Chore.ToString(), *Def->RewardSecret.ToString(), Ends);
	}
	if (!Def->RewardCompartment.IsNone())
	{
		const bool bOpened = OpenCompartment(Def->RewardCompartment, By);
		UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR reward chore=%s compartment=%s opened=%d"), *Chore.ToString(),
		       *Def->RewardCompartment.ToString(), bOpened ? 1 : 0);
	}
}
