#pragma once

#include "CoreMinimal.h"
#include "KGStamina.generated.h"

/**
 * Sprint/shove stamina (Docs/01_GDD_Core.md §4). Pure value type so it can be snapshotted and unit-tested.
 * Hitting zero exhausts the player until stamina recovers past RecoverThreshold (no sprint-tapping at 1%).
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGStamina
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stamina")
	float Current = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
	float Max = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float SprintDrainPerSec = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float RegenPerSec = 16.0f;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float RegenDelay = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Stamina")
	float RecoverThreshold = 25.0f;

	UPROPERTY(SaveGame)
	float RegenCooldown = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Stamina")
	bool bExhausted = false;

	/** Advances stamina; returns whether the character is actually sprinting this tick. */
	bool Tick(float Dt, bool bWantsSprint, bool bMoving)
	{
		const bool bSprinting = bWantsSprint && bMoving && !bExhausted && Current > 0.0f;
		if (bSprinting)
		{
			Current = FMath::Max(0.0f, Current - SprintDrainPerSec * Dt);
			RegenCooldown = RegenDelay;
			bExhausted = Current <= 0.0f;
		}
		else if (RegenCooldown > 0.0f)
		{
			RegenCooldown = FMath::Max(0.0f, RegenCooldown - Dt);
		}
		else
		{
			Current = FMath::Min(Max, Current + RegenPerSec * Dt);
			if (bExhausted && Current >= RecoverThreshold)
			{
				bExhausted = false;
			}
		}
		return bSprinting;
	}

	/** Spends a chunk (shove, struggle). Fails without spending if there is not enough. */
	bool TrySpend(float Amount)
	{
		if (bExhausted || Current < Amount)
		{
			return false;
		}
		Current -= Amount;
		RegenCooldown = RegenDelay;
		return true;
	}

	float GetAlpha() const { return Max > 0.0f ? Current / Max : 0.0f; }
};
