#pragma once

#include "CoreMinimal.h"
#include "Forest/KGForestTypes.h"

struct KILLGODOT_API FKGForestTrail
{
	FName Name;
	TArray<FVector2D> Points;   // metres
	float Width = 2.4f;
};

struct KILLGODOT_API FKGForestDen
{
	FName Id;
	int32 Sector = 0;
	FVector2D At = FVector2D::ZeroVector;
};

/**
 * The forest band raster of a map (SPRINT-033a): Tools/Level/gen_forest_bands.py -> Tools/Level/morrowmere_forest_v2.json
 * (dev builds read it from disk) / KGForestData.gen.inl (embedded). 2 m cells, metres, UE frame.
 * Band + flags per cell, sector per cell, and a nearest-safe-cell field computed at load (multi-source BFS).
 */
class KILLGODOT_API FKGForestMap
{
public:
	/** The Morrowmere v2 forest (empty / invalid when the data is missing or broken). */
	static const FKGForestMap& Get();
	/** Dev: re-read the JSON (kg.Forest.Reload). */
	static bool Reload(FString& OutMessage);
	/** Tests: parse a JSON text. */
	bool Parse(const FString& Json, FString& OutError);

	bool IsValid() const { return NX > 0 && NY > 0 && Flags.Num() == NX * NY; }

	/** Metres (UE cm / 100). Out-of-raster points read as Out. */
	uint8 FlagsAt(const FVector2D& M) const;
	EKGForestBand BandAt(const FVector2D& M) const { return static_cast<EKGForestBand>(FlagsAt(M) & KGForestFlag::BandMask); }
	bool IsStaticSafe(const FVector2D& M) const { return (FlagsAt(M) & KGForestFlag::Safe) != 0; }
	bool IsWalkable(const FVector2D& M) const { return (FlagsAt(M) & KGForestFlag::NotWalkable) == 0; }
	int32 SectorAt(const FVector2D& M) const;
	/** Centre of the nearest static safe cell (metres); false when the raster has none / M is outside. */
	bool NearestSafe(const FVector2D& M, FVector2D& Out) const;
	/** Distance (m) from M to the nearest static safe cell (0 on one, large when unknown). */
	float SafeDistance(const FVector2D& M) const;
	/** Inside the playable forest boundary (the Mist Wall radius around the map origin). */
	bool InsideBoundary(const FVector2D& M) const { return M.Size() <= ROut; }
	static FString SectorName(int32 Sector);

	float Cell = 2.0f;
	float X0 = 0.0f;
	float Y0 = 0.0f;
	int32 NX = 0;
	int32 NY = 0;
	float ROut = 178.0f;
	FVector2D Square = FVector2D::ZeroVector;
	FVector2D Camp = FVector2D::ZeroVector;
	float CampZ = 0.0f;
	TArray<FKGForestTrail> Trails;
	TArray<FKGForestDen> Dens;
	/** Fixed trail-head lanterns (always lit, KG_LANTERN_SAFE). */
	TArray<FVector2D> Lanterns;
	TArray<uint8> Flags;
	TArray<uint8> Sectors;
	/** Index of the nearest safe cell per cell (INDEX_NONE when none). */
	TArray<int32> NearestSafeIndex;

private:
	int32 IndexOf(const FVector2D& M) const;
	FVector2D CentreOf(int32 Index) const;
	void BuildNearestSafe();
	void RefineNearestSafe();
};
