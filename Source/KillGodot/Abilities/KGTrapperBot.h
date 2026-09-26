#pragma once

#include "CoreMinimal.h"

class AKGBotController;
class AKGCharacter;

/**
 * SPRINT-041 bot hooks (one call from AKGBotController::Tick, so the brain file stays untouched otherwise):
 * - a Trapper bot spends its ability charges by day along the chore routes: a mimic in a container near a task
 *   station (only when unseen), a tripwire and a snare on the approach to a station;
 * - villager bots now and then rummage a container near them (the way players loot), which is how bots meet mimics;
 *   a bot that heard a mimic scream (UKGAbilitySubsystem::NoteMimicScream) never opens that container again.
 * Returns true while it drives the body (the caller skips the rest of the brain for this tick).
 */
namespace KGTrapperBot
{
	KILLGODOT_API bool Update(AKGBotController* Bot, AKGCharacter* Me, float DeltaSeconds);

	/** Counters since the match started (kg.Trapper.Stats, the balance report). */
	struct FStats
	{
		int32 Arms = 0;
		int32 Rummages = 0;
		int32 Bitten = 0;
		int32 Avoided = 0;
	};
	KILLGODOT_API FStats& Stats();
}
