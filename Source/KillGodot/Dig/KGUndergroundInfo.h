#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "World/KGMapInfo.h"
#include "KGUndergroundInfo.generated.h"

class AKGMapInfo;
class UAudioComponent;
class UPostProcessComponent;
class UTexture2D;

/**
 * The underground of a village level (the well cellar, the tunnel, the catacombs: built below the terrain in the same
 * persistent level by Tools/Unreal/kg_build_underground.py). Placed once in the level, not replicated.
 *  - "below ground" = inside one of Volumes (cm boxes): the HUD map switches to this plan (UI/KGHUDMap.inl hook),
 *    the location toast uses Regions, the dev ground finder skips it,
 *  - its own look while the local camera is below: an unbound post-process blended in (darker exposure, vignette,
 *    cooler shadows; the candles are lights of their own) and a 2D ambience loop (drips, distant echoes).
 * The HUD map code reads a transient AKGMapInfo built from this plan (GetPlan), spawned locally on first use.
 */
UCLASS(hidecategories = (Input, Movement, Collision, Rendering, HLOD, Physics, Replication, LOD, Cooking))
class KILLGODOT_API AKGUndergroundInfo : public AInfo
{
	GENERATED_BODY()

public:
	AKGUndergroundInfo();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	static AKGUndergroundInfo* Find(const UWorld* World);
	/** P (cm) is inside the underground of World's level. */
	static bool IsBelowGround(const UWorld* World, const FVector& P);
	bool Contains(const FVector& P) const;
	/** Name of the underground region at P ("Well Cellar", "Catacombs"...), empty if none. */
	FString RegionNameAt(const FVector& P) const;

	/** A map info carrying this plan for the HUD (spawned locally, transient, never saved). */
	AKGMapInfo* GetPlan();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMin = FVector2D(-10000.0, -10000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMax = FVector2D(10000.0, 10000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText MapTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TArray<FKGMapRegion> Regions;

	/** World boxes (cm) that are "below ground". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Underground")
	TArray<FBox> Volumes;

	/** Exposure bias while below (the village look uses +0.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Underground")
	float ExposureBias = -0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Underground")
	float Vignette = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Underground")
	FName AmbienceSound = TEXT("S_Cave_Amb_Loop");

private:
	UPROPERTY() TObjectPtr<UPostProcessComponent> Look;
	UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
	UPROPERTY() TObjectPtr<AKGMapInfo> Plan;
	float Below = 0.0f;
	float DripIn = 2.0f;
	uint32 DripState = 0x2545F491u;
};
