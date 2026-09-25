#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KGCharacterMovement.generated.h"

/**
 * Swimming without water volumes: "in water" means the capsule centre is below the wave surface (FKGWaves), and
 * immersion (which drives buoyancy) is measured against the same moving surface, so players bob with the waves.
 * Wading in shallows stays walking; from ~1.2 m depth the character swims.
 */
UCLASS()
class KILLGODOT_API UKGCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UKGCharacterMovement();

	virtual bool IsInWater() const override;
	virtual float ImmersionDepth() const override;
	virtual float GetMaxSpeed() const override;

	/** Dev panel speed multiplier (kg.Me.Speed). Set on the server and the owning client alike (UKGDevComponent). */
	float DevSpeedScale = 1.0f;

	/** Water surface height above the capsule centre at which swimming starts (negative = centre below). */
	UPROPERTY(EditAnywhere, Category = "Swimming")
	float SwimStartOffset = -25.0f;
};
