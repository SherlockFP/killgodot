#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UKGSnapshotComponent;

/**
 * A small bobbing, spinning item in the world. E puts it into the player's pockets (server authoritative); whatever
 * does not fit stays on the ground. Spawn with AKGPickup::SpawnPickup or FKGLoot::SpawnLoot, or place in the level
 * and set ItemId/Count.
 */
UCLASS()
class KILLGODOT_API AKGPickup : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/**
	 * Authority: spawns a pickup of Count x ItemId at Location (snapped onto the ground below when bSnapToGround).
	 * Returns nullptr on clients or for unknown items.
	 */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Items", meta = (WorldContext = "WorldContextObject"))
	static AKGPickup* SpawnPickup(UObject* WorldContextObject, FName InItemId, int32 InCount, FVector Location,
	                              bool bSnapToGround = true);

	/** Authority. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Items")
	void SetItem(FName InItemId, int32 InCount);

	FName GetItemId() const { return ItemId; }
	int32 GetCount() const { return Count; }

	/** Height the item floats above the ground. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float HoverHeight = 22.0f;

protected:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Item, SaveGame, BlueprintReadOnly, Category = "Pickup")
	FName ItemId = FName(TEXT("Coin"));

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Item, SaveGame, BlueprintReadOnly, Category = "Pickup",
		meta = (ClampMin = "1"))
	int32 Count = 1;

	UFUNCTION()
	void OnRep_Item();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPickedUp();

	void ApplyLook();

	/** Generous E-trace target (blocks Visibility only); the mesh itself has no collision. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Spinner;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

private:
	FName AppliedItem;
	float Phase = 0.0f;
	bool bTaken = false;
};
