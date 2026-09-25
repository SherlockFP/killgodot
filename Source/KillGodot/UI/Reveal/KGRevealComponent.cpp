#include "UI/Reveal/KGRevealComponent.h"

#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "UI/Reveal/KGRevealTypes.h"
#include "UI/Reveal/KGRoleCardText.h"

namespace KGRevealComponentPrivate
{
	const FKGRoleInfo* RoleOf(const APlayerState* PS)
	{
		const AKGPlayerState* KGPS = Cast<AKGPlayerState>(PS);
		return KGPS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), KGPS->GetPrivateRoleId())
		            : nullptr;
	}

	bool IsHuman(const APlayerController* PC)
	{
		return PC && PC->PlayerState && !PC->PlayerState->IsABot();
	}
}

UKGRevealComponent::UKGRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKGRevealComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;   // secret: teammates are never sent to anyone else
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGRevealComponent, Mates, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGRevealComponent, DealSerial, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGRevealComponent, bReady, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGRevealComponent, ReadyCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGRevealComponent, HumanCount, Params);
}

UKGRevealComponent* UKGRevealComponent::Find(const APlayerController* PC)
{
	return PC ? PC->FindComponentByClass<UKGRevealComponent>() : nullptr;
}

UKGRevealComponent* UKGRevealComponent::Ensure(APlayerController* PC)
{
	if (!IsValid(PC) || !PC->HasAuthority() || PC->IsActorBeingDestroyed() || !KGRevealComponentPrivate::IsHuman(PC))
	{
		return nullptr;
	}
	if (UKGRevealComponent* Existing = Find(PC))
	{
		return Existing;
	}
	UKGRevealComponent* Comp = NewObject<UKGRevealComponent>(PC, TEXT("KGReveal"));
	Comp->SetIsReplicated(true);
	PC->AddInstanceComponent(Comp);
	Comp->RegisterComponent();
	return Comp;
}

void UKGRevealComponent::AuthDeal(UWorld* World)
{
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (!GS || World->GetNetMode() == NM_Client)
	{
		return;
	}
	int32 Humans = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		Humans += KGRevealComponentPrivate::IsHuman(It->Get()) ? 1 : 0;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		UKGRevealComponent* Comp = Ensure(It->Get());
		if (!Comp)
		{
			continue;
		}
		APlayerState* Me = It->Get()->PlayerState;
		const FKGRoleInfo* Mine = KGRevealComponentPrivate::RoleOf(Me);
		Comp->Mates.Reset();
		if (Mine && Mine->GetAlignment() == EKGAlignment::Impatient && KGRoleCard::IsTeamFaction(Mine->Faction))
		{
			for (APlayerState* Other : GS->PlayerArray)
			{
				const FKGRoleInfo* Theirs = Other != Me ? KGRevealComponentPrivate::RoleOf(Other) : nullptr;
				if (Theirs && Theirs->Faction == Mine->Faction)
				{
					FKGRevealMate& Mate = Comp->Mates.AddDefaulted_GetRef();
					Mate.Player = Other;
					Mate.Name = Other->GetPlayerName();
					Mate.RoleId = Theirs->RoleId;
				}
			}
		}
		++Comp->DealSerial;
		Comp->bReady = false;
		Comp->ReadyRequestedAt = -1.0;
		Comp->ReadyCount = 0;
		Comp->HumanCount = static_cast<uint8>(FMath::Clamp(Humans, 0, 255));
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, Mates, Comp);
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, DealSerial, Comp);
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, bReady, Comp);
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, ReadyCount, Comp);
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, HumanCount, Comp);
		It->Get()->ForceNetUpdate();
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL deal %s: %s, %d mate(s)"), *Me->GetPlayerName(),
		       Mine ? *Mine->RoleId.ToString() : TEXT("?"), Comp->Mates.Num());
	}
}

void UKGRevealComponent::AuthUpdateReady(UWorld* World)
{
	AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (!GS || World->GetNetMode() == NM_Client)
	{
		return;
	}
	int32 Humans = 0;
	int32 Ready = 0;
	TArray<UKGRevealComponent*> Comps;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (UKGRevealComponent* Comp = KGRevealComponentPrivate::IsHuman(It->Get()) ? Find(It->Get()) : nullptr)
		{
			++Humans;
			Ready += Comp->bReady ? 1 : 0;
			Comps.Add(Comp);
		}
	}
	for (UKGRevealComponent* Comp : Comps)
	{
		if (Comp->ReadyCount != Ready || Comp->HumanCount != Humans)
		{
			Comp->ReadyCount = static_cast<uint8>(FMath::Clamp(Ready, 0, 255));
			Comp->HumanCount = static_cast<uint8>(FMath::Clamp(Humans, 0, 255));
			MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, ReadyCount, Comp);
			MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, HumanCount, Comp);
			Comp->GetOwner()->ForceNetUpdate();
		}
	}
	if (GS->GetPhase() == EKGPhase::RoleReveal && Humans > 0 && Ready >= Humans &&
	    GS->Clock.RemainingSeconds > KGReveal::SkipToSeconds)
	{
		// Everyone has read their card: the match clock jumps to the fade-out (server-timed, migration-safe).
		GS->Clock.RemainingSeconds = KGReveal::SkipToSeconds;
		GS->ForceNetUpdate();
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL everyone ready (%d/%d): phase cut to %.2fs"), Ready, Humans,
		       KGReveal::SkipToSeconds);
	}
}

bool UKGRevealComponent::RequestReady()
{
	const double Now = FPlatformTime::Seconds();
	if (bReady || (ReadyRequestedAt >= 0.0 && Now - ReadyRequestedAt < 1.0))
	{
		return false;
	}
	ReadyRequestedAt = Now;
	ServerSetReady();
	return true;
}

void UKGRevealComponent::ServerSetReady_Implementation()
{
	UWorld* World = GetWorld();
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (!GS || GS->GetPhase() != EKGPhase::RoleReveal || bReady)
	{
		return;
	}
	const float Elapsed = GS->Clock.PhaseDuration - GS->Clock.RemainingSeconds;
	if (Elapsed + KGReveal::ServerReadyTolerance < KGReveal::ReadyFromSeconds)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL ready refused (%.2fs < %.2fs)"), Elapsed, KGReveal::ReadyFromSeconds);
		return;
	}
	bReady = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGRevealComponent, bReady, this);
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	UE_LOG(LogKillGodot, Log, TEXT("KG_REVEAL ready %s at %.2fs"), PC && PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("?"),
	       Elapsed);
	AuthUpdateReady(World);
}
