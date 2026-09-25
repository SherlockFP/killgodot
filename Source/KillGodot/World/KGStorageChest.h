#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/KGInventoryRPCComponent.h"
#include "Inventory/KGInventoryTypes.h"
#include "World/KGInteractable.h"
#include "KGStorageChest.generated.h"

class AKGPlayerState;
class UKGInventoryComponent;
class UKGSnapshotComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Minecraft-style storage chest. E opens a two-panel window (pockets <-> chest); clicking an item moves the stack
 * (right click: one). Contents are server-only and reach just the players who have it open (see
 * UKGInventoryRPCComponent). Optionally owned by a house: when locked, only the owner (OwnerPuid, or whoever got
 * HouseIndex from the game mode) can open it; the owner toggles the lock from the window.
 */
UCLASS()
class KILLGODOT_API AKGStorageChest : public AActor, public IKGInteractable, public IKGContainerOwner
{
	GENERATED_BODY()

public:
	AKGStorageChest();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IKGInteractable
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	// IKGContainerOwner
	virtual UKGInventoryComponent* GetContainerInventory() const override { return Inventory; }
	virtual FText GetContainerTitle() const override { return DisplayName; }
	virtual bool CanPlayerUseContainer(const APlayerState* Player) const override;
	virtual bool CanPlayerToggleLock(const APlayerState* Player) const override;
	virtual void SetContainerLocked(bool bInLocked) override;
	virtual bool IsContainerLocked() const override { return bLocked; }
	virtual void OnViewerCountChanged(int32 InNumViewers) override;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Chest")
	void SetChestMesh(UStaticMesh* NewMesh);

	/** Authority: binds the chest to a house owner (EOS PUID). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Chest")
	void SetHouseOwner(const FString& InOwnerPuid);

	bool IsOwnedBy(const APlayerState* Player) const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Chest")
	UKGInventoryComponent* GetInventory() const { return Inventory; }

	/** Level builder: which mesh to show. Defaults to the pirate/Quaternius chest when imported, else a box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	TObjectPtr<UStaticMesh> ChestMesh;

	/** Uniform scale for real chest meshes (the Quaternius chest is 127 cm long; 0.75 gives a ~95 cm chest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest", meta = (ClampMin = "0.1"))
	float MeshScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Slots = 16;

	/** House owner's EOS PUID (empty = public chest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Chest")
	FString OwnerPuid;

	/** Alternative ownership: the player whose AKGPlayerState::HouseIndex matches owns it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Chest")
	int32 HouseIndex = INDEX_NONE;

	/** Given once on the server at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	TArray<FKGItemStack> StartingItems;

	/** Optional FKGLoot table rolled once on the server at BeginPlay ("Chest", "Crate"...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	FName StartingLootTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	int32 LootSeed = 0;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Chest")
	bool bLocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_NumViewers)
	uint8 NumViewers = 0;

	UFUNCTION()
	void OnRep_NumViewers(uint8 OldNumViewers);

	UPROPERTY(SaveGame)
	bool bStartingItemsGiven = false;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	/** True when the fallback cube is shown (so it gets chest-like proportions). */
	bool bUsingFallbackMesh = false;
};
