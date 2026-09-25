#include "Core/KGPlayerState.h"
#include "Net/UnrealNetwork.h"

AKGPlayerState::AKGPlayerState()
{
	SetNetUpdateFrequency(2.0f);
}

void AKGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGPlayerState, AccuseTarget);
	DOREPLIFETIME(AKGPlayerState, Verdict);
	DOREPLIFETIME_CONDITION(AKGPlayerState, TaskIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AKGPlayerState, TaskDone, COND_OwnerOnly);
	DOREPLIFETIME(AKGPlayerState, Puid);
	DOREPLIFETIME(AKGPlayerState, FaceId);
	DOREPLIFETIME(AKGPlayerState, ProfessionId);
	DOREPLIFETIME(AKGPlayerState, HouseIndex);
	DOREPLIFETIME(AKGPlayerState, LifeState);
	DOREPLIFETIME(AKGPlayerState, RevealedRoleId);
	DOREPLIFETIME_CONDITION(AKGPlayerState, PrivateRoleId, COND_OwnerOnly);
}
