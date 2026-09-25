#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "KGFishingJournal.generated.h"

/**
 * The local angler's journal (slot "KGFishing"): personal best weight and catch count per species. Cosmetic and
 * client-side only (the catch card's "personal best" line); the server never trusts it.
 */
UCLASS()
class KILLGODOT_API UKGFishingJournal : public USaveGame
{
	GENERATED_BODY()

public:
	/** Loaded once per process (created empty when there is no save yet). */
	static UKGFishingJournal* Get();

	/** Records a catch; returns the previous best (grams, 0 = first of its kind). */
	int32 Record(FName Species, int32 Grams);

	int32 GetBest(FName Species) const { const int32* B = BestGrams.Find(Species); return B ? *B : 0; }
	int32 GetCaught(FName Species) const { const int32* C = Caught.Find(Species); return C ? *C : 0; }

	UPROPERTY(SaveGame)
	TMap<FName, int32> BestGrams;

	UPROPERTY(SaveGame)
	TMap<FName, int32> Caught;
};
