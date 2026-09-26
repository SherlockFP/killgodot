#include "Abilities/KGWoundComponent.h"
#include "Character/KGCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

const FName UKGWoundComponent::BiteMarks(TEXT("BiteMarks"));
const FName UKGWoundComponent::SnareWound(TEXT("SnareWound"));

UKGWoundComponent::UKGWoundComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UKGWoundComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKGWoundComponent, Wounds);
}

void UKGWoundComponent::AuthAdd(AKGCharacter* Body, FName Kind)
{
	if (!Body || !Body->HasAuthority() || Kind.IsNone())
	{
		return;
	}
	UKGWoundComponent* W = Body->FindComponentByClass<UKGWoundComponent>();
	if (!W)
	{
		W = NewObject<UKGWoundComponent>(Body, TEXT("KGWounds"));
		W->SetIsReplicated(true);
		W->RegisterComponent();
	}
	FKGWound& New = W->Wounds.AddDefaulted_GetRef();
	New.Kind = Kind;
	const AGameStateBase* GS = Body->GetWorld() ? Body->GetWorld()->GetGameState() : nullptr;
	New.At = GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : 0.0f;
	Body->FlushNetDormancy();
	Body->ForceNetUpdate();
}

const UKGWoundComponent* UKGWoundComponent::FindOn(const AActor* Body)
{
	return Body ? Body->FindComponentByClass<UKGWoundComponent>() : nullptr;
}

FString UKGWoundComponent::ListOn(const AActor* Body)
{
	FString Out;
	if (const UKGWoundComponent* W = FindOn(Body))
	{
		for (const FKGWound& X : W->Wounds)
		{
			Out += (Out.IsEmpty() ? TEXT("") : TEXT(",")) + X.Kind.ToString();
		}
	}
	return Out;
}

FString UKGWoundComponent::Describe(FName Kind, bool bTurkish)
{
	if (Kind == BiteMarks)
	{
		return bTurkish ? TEXT("Isırık izleri: sıra sıra küçük diş, ahşap kıymığı") : TEXT("Bite marks: rows of small teeth, wood splinters");
	}
	if (Kind == SnareWound)
	{
		return bTurkish ? TEXT("Kapan yarası: bilekte demir diş izi") : TEXT("Snare wound: iron jaw marks round the ankle");
	}
	return Kind.ToString();
}

namespace KGTrapHold
{
	void Apply(AKGCharacter* Body, bool bHold)
	{
		if (!Body || !(Body->HasAuthority() || Body->IsLocallyControlled()))
		{
			return;
		}
		UCharacterMovementComponent* Move = Body->GetCharacterMovement();
		if (!Move)
		{
			return;
		}
		if (bHold)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		else if (!Body->IsDead() && Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	bool IsHeld(const AKGCharacter* Body)
	{
		const UCharacterMovementComponent* Move = Body ? Body->GetCharacterMovement() : nullptr;
		return Move && Move->MovementMode == MOVE_None;
	}
}
