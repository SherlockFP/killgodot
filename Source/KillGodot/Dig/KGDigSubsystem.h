#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGDigSubsystem.generated.h"

class AKGCharacter;

/**
 * Digging glue, zero-integration like UKGFishingSubsystem:
 *  - server: gives every AKGCharacter a replicated UKGDigComponent (runtime subobject),
 *  - server: spawns the world's AKGDigManager and (re)generates the dig spots whenever the match seed changes
 *    (before a match: a fixed free-roam seed),
 *  - dev (-KGDigShots): pre-digs a few spots to every stage for the headless captures (Tools/Unreal/kg_capture_dig.ps1),
 *  - dev (-KGDigSmoke): the headless network smoke (Tools/Unreal/kg_dig_smoke.ps1): the client digs a mound out
 *    (stages + loot replicate), then climbs down the well into the cellar; KG_DIG_SEEN / KG_DIG_POCKETS /
 *    KG_DIG_REGION / KG_DIG_DONE lines on every machine.
 */
UCLASS()
class KILLGODOT_API UKGDigSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Authority: idempotent. */
	static void EnsureDig(AKGCharacter* Character);

	/** Free-roam seed (no match running yet). */
	static constexpr uint64 FreeRoamSeed = 0x5EED0D16ull;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void TickSmoke(float DeltaTime);
	void ApplyShotStages();

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<AKGCharacter>> Pending;
	float SweepSeconds = 0.0f;
	bool bShotsApplied = false;

	// -KGDigSmoke
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	uint16 SmokeSpot = 0;
	uint8 SmokeResultSerial = 0;
	float SmokeLogAccum = 0.0f;
	float SmokeStepAt = 0.0f;
	bool bSmokeDone = false;
	FString SmokeLastRegion;
};
