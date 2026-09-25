#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "KGMapInfo.generated.h"

class APlayerController;
class UMaterialParameterCollection;
class UTexture2D;
class UWorld;

/**
 * One named location on the village map (Lockdown-Protocol-style room names). Generated from the layout JSON by
 * Tools/Level/render_minimap.py -> KG_MapRegions_<Key>.json -> Tools/Unreal/kg_make_minimap.py. Units: cm, UE XY.
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGMapRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText Name;

	/** district / street / place / building / poi (informational; Layer drives behaviour). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName Kind;

	/** 0 district, 1 street, 2 place, 3 building, -1 map-only point of interest. The player's location is the highest
	 *  layer that contains them (ties: the smallest region). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	int32 Layer = 2;

	/** Entering it shows the top-centre location toast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	bool bToast = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D Center = FVector2D::ZeroVector;

	/** Circle radius (cm), used when Polygon is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TArray<FVector2D> Polygon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D LabelPos = FVector2D::ZeroVector;

	/** HUD vector icon id (church, bell, well, gallows, anchor, ...); None = no icon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName Icon;

	/** Label importance (higher wins when labels collide); < 0 = never labelled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	int32 Priority = 50;

	bool Contains(const FVector2D& P) const;
	double Area() const;
};

/**
 * Per-level map data for the HUD minimap, the full-screen map (M) and the location toast. Not replicated: it is
 * placed in the level (loaded identically on every machine) by Tools/Unreal/kg_make_minimap.py, which the village
 * builders call. The texture covers WorldMin..WorldMax exactly, north (-Y) up, +X to the right.
 */
UCLASS(hidecategories = (Input, Movement, Collision, Rendering, HLOD, Physics, Replication, LOD, Cooking))
class KILLGODOT_API AKGMapInfo : public AInfo
{
	GENERATED_BODY()

public:
	AKGMapInfo();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMin = FVector2D(-10000.0, -10000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMax = FVector2D(10000.0, 10000.0);

	/** Shown as the full map's title ("Morrowmere"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText MapTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TArray<FKGMapRegion> Regions;

	/** Sheltered harbour basin: inside CalmRadius the ocean swell fades to CalmWaveScale plus small ripples and a tint.
	 *  Pushed into FKGWaves (swimming, buoyancy) and MPC_KG_Water (M_KG_Ocean WPO) so both agree. Off on v1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water")
	bool bCalmWater = false;

	/** Basin centre, world XY (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater"))
	FVector2D CalmCentre = FVector2D::ZeroVector;

	/** Outer radius (cm): full swell beyond it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater"))
	float CalmRadius = 3400.0f;

	/** Ramp width (cm) inside the radius; full calm within CalmRadius - CalmFade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater"))
	float CalmFade = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater", ClampMin = "0", ClampMax = "1"))
	float CalmWaveScale = 0.25f;

	/** Ripple amplitude (cm) at full calm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater"))
	float CalmRipple = 2.5f;

	/** Sheltered-water colour and how much of it is mixed in at full calm (material only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater"))
	FLinearColor CalmTint = FLinearColor(0.02f, 0.30f, 0.27f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water", meta = (EditCondition = "bCalmWater", ClampMin = "0", ClampMax = "1"))
	float CalmTintAmount = 0.45f;

	/** MPC_KG_Water (loaded by path when empty). */
	UPROPERTY(EditAnywhere, Category = "Water")
	TObjectPtr<UMaterialParameterCollection> WaterParameters;

	/** Push the calm-water numbers into FKGWaves and this world's MPC_KG_Water instance. */
	void ApplyWater();

	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	/** First map info in the world (cached per world). */
	static AKGMapInfo* Find(const UWorld* World);

	/** Index of the region the point is in (highest layer, then smallest), or INDEX_NONE. Layer < 0 never matches. */
	int32 FindRegionAt(const FVector2D& P) const;

	/** World XY (cm) -> texture UV (0..1). */
	FVector2D ToUV(const FVector2D& P) const
	{
		const FVector2D Span = (WorldMax - WorldMin).ComponentMax(FVector2D(1.0, 1.0));
		return (P - WorldMin) / Span;
	}
};

namespace KGMinimap
{
	/** Full-screen map (owned by AKGHUD). Returns true when it was open and has been closed (Esc handling). */
	KILLGODOT_API bool CloseFullMap(const APlayerController* PC);
	KILLGODOT_API bool IsFullMapOpen(const APlayerController* PC);
}
