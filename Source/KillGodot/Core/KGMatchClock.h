#pragma once

#include "CoreMinimal.h"
#include "KGMatchClock.generated.h"

/**
 * The only source of gameplay durations. Stores *remaining* seconds (never absolute server time) so a migrated
 * host can resume exactly where the old one stopped. TimerManager must not be used for gameplay-critical timing.
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGMatchClock
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Clock")
	float RemainingSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Clock")
	float PhaseDuration = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Clock")
	bool bPaused = false;

	void Start(float Duration)
	{
		PhaseDuration = FMath::Max(0.0f, Duration);
		RemainingSeconds = PhaseDuration;
	}

	/** Returns true on the tick the clock reaches zero. */
	bool Advance(float DeltaSeconds)
	{
		if (bPaused || RemainingSeconds <= 0.0f)
		{
			return false;
		}
		RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - DeltaSeconds);
		return RemainingSeconds <= 0.0f;
	}

	float GetAlpha() const
	{
		return PhaseDuration > 0.0f ? 1.0f - RemainingSeconds / PhaseDuration : 1.0f;
	}
};
