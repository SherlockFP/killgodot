#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGPlayerExtrasSubsystem.generated.h"

class APlayerState;

/**
 * Zero-integration glue: on the server it gives every AKGPlayerState (humans and bots) the runtime components this
 * feature set needs — UKGInventoryComponent (pockets), UKGInventoryRPCComponent (client->server relay) and
 * UKGCosmeticsComponent (equipped hats etc.). The components replicate as dynamic subobjects. If AKGPlayerState
 * later creates them as default subobjects, FindComponentByClass finds those and nothing is added.
 * Also hosts the dev console commands (kg.GiveItem, kg.SpawnChest, ...; not in Shipping).
 */
UCLASS()
class KILLGODOT_API UKGPlayerExtrasSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Authority: idempotent. */
	static void EnsurePlayerComponents(APlayerState* PlayerState);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<APlayerState>> Pending;
	float SweepSeconds = 0.0f;
};

#if !UE_BUILD_SHIPPING
/**
 * Dev spawners shared by the kg.* console commands below and the dev panel (Dev/KGDevCommands.cpp). Authority world
 * only; "For" is the authoritative player state whose pawn the thing appears in front of.
 */
namespace KGExtrasDev
{
	/** Adds Count of ItemId to For's pockets. Returns how many fit. */
	KILLGODOT_API int32 GiveItem(APlayerState* For, FName ItemId, int32 Count);
	KILLGODOT_API AActor* SpawnPickup(APlayerState* For, FName ItemId, int32 Count);
	/** Rolls a loot table (Crate, Barrel, Pot, Chest, Grave, Fishing) in front of For. Returns pickups spawned. */
	KILLGODOT_API int32 SpawnLoot(APlayerState* For, FName Table, int32 Seed);
	/** Storage chest facing For; bMine makes For the house owner (a DEV-<id> PUID is minted in PIE). */
	KILLGODOT_API AActor* SpawnChest(APlayerState* For, bool bLocked, bool bMine, FName LootTable);
	/** Stool | Chair | Bench facing For. */
	KILLGODOT_API AActor* SpawnSeat(APlayerState* For, const FString& Kind);
}
#endif
