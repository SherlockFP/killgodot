#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KGInventoryTypes.generated.h"

struct FKGItemList;

/** Plain (non-replicated) "N of item X": loot results, chest starting contents, console commands. */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGItemStack
{
	GENERATED_BODY()

	FKGItemStack() = default;
	FKGItemStack(FName InItemId, int32 InCount) : ItemId(InItemId), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * One occupied inventory slot. Slot indices are stable (assigned by the server, lowest free first) so the UI keeps
 * items where they are even though FastArray order is not preserved on clients.
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGItemEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FKGItemEntry() = default;
	FKGItemEntry(int32 InSlot, FName InItemId, int32 InCount, int32 InGrams = 0)
		: Slot(InSlot), ItemId(InItemId), Count(InCount), Grams(InGrams) {}

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	int32 Slot = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	/** Total weight of the stack in grams (caught fish; 0 = not weighed). Splits proportionally when the stack does. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	int32 Grams = 0;
};

/** Replicated item list (FastArray: per-item deltas, Iris-compatible). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGItemList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	TArray<FKGItemEntry> Entries;

	FKGItemEntry* FindSlot(int32 Slot) { return Entries.FindByPredicate([Slot](const FKGItemEntry& E) { return E.Slot == Slot; }); }
	const FKGItemEntry* FindSlot(int32 Slot) const { return Entries.FindByPredicate([Slot](const FKGItemEntry& E) { return E.Slot == Slot; }); }

	/** Makes this list equal to Source slot by slot, dirtying only what changed. Returns true if anything did. */
	bool SyncFrom(const FKGItemList& Source);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FKGItemEntry, FKGItemList>(Entries, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FKGItemList> : public TStructOpsTypeTraitsBase2<FKGItemList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
