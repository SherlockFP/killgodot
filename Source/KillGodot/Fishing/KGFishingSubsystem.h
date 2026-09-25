#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGFishingSubsystem.generated.h"

class AKGCharacter;
class APlayerController;

/**
 * Fishing glue, zero-integration like UKGPlayerExtrasSubsystem:
 *  - server: gives every AKGCharacter a replicated UKGFishingComponent (runtime subobject),
 *  - server: puts Madam Brine's stall (AKGFishMarketStall) on the map's "fish_market" building once per world,
 *  - dev (-KGFishSmoke): scripted cast -> forced bite -> hook -> reel -> catch for the headless network smoke
 *    (Tools/Unreal/kg_fish_smoke.ps1), logging KG_FISH_SEEN / KG_FISH_POCKETS / KG_FISH_DONE on every machine.
 */
UCLASS()
class KILLGODOT_API UKGFishingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Authority: idempotent. */
	static void EnsureFishing(AKGCharacter* Character);

	/** Authority: spawns the market stall on the map's fish market (no-op when one exists or the map has none). */
	void EnsureMarket();

	/** Where the v2 map's stall goes (Madam Brine's fish table in the arcade), from the map region "fish_market". */
	static bool FindMarketSpot(UWorld* World, FVector& OutLocation, float& OutYaw);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void TickSmoke(float DeltaTime);

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<AKGCharacter>> Pending;
	float SweepSeconds = 0.0f;
	bool bMarketChecked = false;

	// -KGFishSmoke
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	uint8 SmokeCatchSerial = 0;
	bool bSmokeHooked = false;
	bool bSmokeForced = false;
	bool bSmokeDone = false;
	float SmokeLogAccum = 0.0f;
	float SmokeReportAt = -1.0f;
};
