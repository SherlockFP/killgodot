#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "Inventory/KGInventoryTypes.h"
#include "KGLoot.generated.h"

class AActor;
class AKGPickup;

USTRUCT(BlueprintType)
struct KILLGODOT_API FKGLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	/** Relative chance among the entries of one roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 Weight = 10;
};

USTRUCT(BlueprintType)
struct KILLGODOT_API FKGLootTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TArray<FKGLootEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxRolls = 1;

	/** Weight of "nothing" in every roll (0 = always something). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 NothingWeight = 0;
};

/**
 * Deterministic loot (FKGRng only, so a host migration replays the same drops). Built-in tables: "Crate",
 * "Barrel", "Pot", "Chest", "Grave", "Fishing". Typical breakable use (server):
 *
 *   FKGRng Rng = FKGLoot::MakeRng(MatchSeed, this);
 *   FKGLoot::RollAndSpawn(this, TEXT("Crate"), Rng, Mesh->Bounds.Origin);
 */
struct KILLGODOT_API FKGLoot
{
	/** Rolls Table and appends merged stacks to OutItems. */
	static void Roll(FKGRng& Rng, const FKGLootTable& Table, TArray<FKGItemStack>& OutItems);

	/** Built-in table by name, or nullptr. */
	static const FKGLootTable* FindTable(FName TableName);

	/** A per-actor stream derived from the match seed and the actor's stable name (no FMath::Rand). */
	static FKGRng MakeRng(uint64 MatchSeed, const AActor* Source);

	/** Authority: one pickup per stack, scattered in a small ring around Origin. */
	static TArray<AKGPickup*> SpawnLoot(UObject* WorldContextObject, const TArray<FKGItemStack>& Items,
	                                    const FVector& Origin, FKGRng& Rng, float Scatter = 45.0f);

	/** Authority: Roll + SpawnLoot. */
	static TArray<AKGPickup*> RollAndSpawn(UObject* WorldContextObject, FName TableName, FKGRng& Rng,
	                                       const FVector& Origin);
};
