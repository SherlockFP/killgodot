#include "Character/KGCharacterMovement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "World/KGWaves.h"

UKGCharacterMovement::UKGCharacterMovement()
{
	// Float with the head out of the water; swim a bit slower than walking.
	Buoyancy = 1.35f;   // head and shoulders above the waves
	MaxSwimSpeed = 330.0f;
	GetNavAgentPropertiesRef().bCanSwim = true;
}

bool UKGCharacterMovement::IsInWater() const
{
	if (!UpdatedComponent || !GetWorld())
	{
		return Super::IsInWater();
	}
	const FVector P = UpdatedComponent->GetComponentLocation();
	const float Surface = FKGWaves::HeightAt(P.X, P.Y, GetWorld()->GetTimeSeconds());
	// Wave-based only (the character switches its physics volume from this answer; asking the volume back would
	// latch "in water" forever).
	return P.Z < Surface + SwimStartOffset;
}

float UKGCharacterMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * DevSpeedScale;
}

float UKGCharacterMovement::ImmersionDepth() const
{
	if (!UpdatedComponent || !CharacterOwner || !GetWorld())
	{
		return Super::ImmersionDepth();
	}
	const FVector P = UpdatedComponent->GetComponentLocation();
	const float Half = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Surface = FKGWaves::HeightAt(P.X, P.Y, GetWorld()->GetTimeSeconds());
	return FMath::Clamp((Surface - (P.Z - Half)) / (2.0f * Half), 0.0f, 1.0f);
}
