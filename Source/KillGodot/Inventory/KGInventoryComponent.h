#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/KGInventoryTypes.h"
#include "KGInventoryComponent.generated.h"

class APawn;
class APlayerState;
class UKGInventoryComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FKGOnInventoryChanged, UKGInventoryComponent* /*Inventory*/);

/**
 * Slot-based item container, server authoritative. Used for a player's pockets (on the player state, replicated
 * COND_OwnerOnly so nobody else can read what you carry) and for storage chests (server-only; viewers receive a
 * per-player mirror through UKGInventoryRPCComponent).
 *
 * Host migration: the item list is UPROPERTY(SaveGame). UKGSnapshotComponent serializes only its owner actor, so
 * owners either override Serialize (AKGStorageChest does) or call WriteSaveData/ReadSaveData explicitly.
 *
 * A slot holds up to UKGItemCatalog::GetStackSize(ItemId) of one item; Capacity is the number of slots.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Player pockets on the player state (created at runtime by UKGInventorySubsystem unless added in C++). */
	static UKGInventoryComponent* FindForPlayer(const APlayerState* PlayerState);
	static UKGInventoryComponent* FindForPawn(const APawn* Pawn);

	// ---- Queries (valid on the server and on the owning client) ----

	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	int32 Count(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	bool Has(FName ItemId, int32 Num = 1) const { return Count(ItemId) >= FMath::Max(1, Num); }

	/** In-match gold = Coin items. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	int32 GetGold() const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	int32 GetUsedSlots() const { return Items.Entries.Num(); }

	/** How many more of ItemId would fit (existing stacks + free slots). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Inventory")
	int32 RoomFor(FName ItemId) const;

	const TArray<FKGItemEntry>& GetEntries() const { return Items.Entries; }
	const FKGItemList& GetItemList() const { return Items; }
	const FKGItemEntry* FindSlot(int32 Slot) const { return Items.FindSlot(Slot); }

	// ---- Mutations (authority only; return how many items were actually affected) ----

	/** Grams = total weight of the Num items (fish); spread over the stacks they land in. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Inventory")
	int32 AddItem(FName ItemId, int32 Num = 1, int32 Grams = 0);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Inventory")
	int32 RemoveItem(FName ItemId, int32 Num = 1);

	/** RemoveItem that also reports the weight that left with the items. */
	int32 RemoveItemWeighed(FName ItemId, int32 Num, int32& OutGrams);

	/** Removes up to Num from one slot (OutGrams: the weight that left with them). */
	int32 RemoveFromSlot(int32 Slot, int32 Num, FName& OutItemId, int32* OutGrams = nullptr);

	/** Total recorded weight (grams) of every ItemId in here. */
	int32 GramsOf(FName ItemId) const;

	/** Moves up to Num of ItemId into Other (limited by Other's room). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Inventory")
	int32 MoveTo(UKGInventoryComponent* Other, FName ItemId, int32 Num = 1);

	/** Moves up to Num from one of our slots into Other (the click-to-transfer path of the chest window). */
	int32 MoveSlotTo(UKGInventoryComponent* Other, int32 Slot, int32 Num);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Inventory")
	void Clear();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Inventory")
	void SetCapacity(int32 NewCapacity);

	/** Fires on the server after every mutation and on the owning client after replication. */
	FKGOnInventoryChanged OnChanged;

	/** Host-migration blob of the SaveGame state (items), for owners that snapshot components explicitly. */
	void WriteSaveData(TArray<uint8>& OutBytes);
	void ReadSaveData(const TArray<uint8>& Bytes);
	/** Re-dirty everything after the list was restored through Serialize (snapshot, save game). */
	void NotifyRestored();

	bool CanMutate() const;

protected:
	/** Number of slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Capacity = 12;

	UPROPERTY(ReplicatedUsing = OnRep_Items, SaveGame)
	FKGItemList Items;

	UFUNCTION()
	void OnRep_Items();

	int32 FindFreeSlot() const;
	void MarkItemsDirty(FKGItemEntry* Changed);
	void Broadcast();
};
