#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "Core/KGTypes.h"
#include "Inventory/KGInventoryTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KGDigTypes.generated.h"

/**
 * Digging (Docs/01_GDD_Core.md section 16): what can be dug, the per-match spot generation (FKGRng only) and the
 * loot rules. Pure data + pure functions, unit-tested in Private/Tests/KGDigTests.cpp.
 */
UENUM()
enum class EKGDigKind : uint8
{
	Mound,      // fresh mound of earth: 3 stages
	XMark,      // painted X: 3 stages, rich loot
	Glint,      // something shiny in loose soil: 1 quick stage
	Grave,      // a real grave in the graveyard: 4 loud stages, leaves an open grave (suspicious!)
	Treasure    // buried chest from a treasure map: hidden until someone digs at its X, 4 stages
};

UENUM()
enum class EKGDigZone : uint8
{
	None,
	Graveyard,
	Beach,
	Farm,
	Forest,
	Orchard
};

/** Owner-only messages the HUD shows as a toast. */
UENUM()
enum class EKGDigNotice : uint8
{
	None,
	NoShovel,       // Q without a shovel in the pockets
	NotNow,         // meeting / trial / dead / swimming
	Busy,           // hands full, blade out, someone else is digging this spot
	NothingHere,    // no spot under the crosshair (and no buried treasure at the X)
	TooFar,
	DugOut,         // this hole is already empty
	PocketsFull,    // loot landed at your feet
	MapAssembled,   // three scraps became a treasure map
	GraveWarning,   // first stroke on a grave: people will hear this
	GateLocked,     // catacomb gate without a crypt key
	GateOpened
};

/** One diggable spot. Replicated to everyone (FastArray); holes deepen with Stage for all players. */
USTRUCT()
struct KILLGODOT_API FKGDigSpot : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Stable within a match (generation order), never 0. */
	UPROPERTY(SaveGame) uint16 Id = 0;
	UPROPERTY(SaveGame) EKGDigKind Kind = EKGDigKind::Mound;
	UPROPERTY(SaveGame) EKGDigZone Zone = EKGDigZone::None;
	/** Stages dug so far (0 = untouched, MaxStage = dug out, loot taken). */
	UPROPERTY(SaveGame) uint8 Stage = 0;
	UPROPERTY(SaveGame) uint8 MaxStage = 3;
	/** Ground point (cm). */
	UPROPERTY(SaveGame) FVector_NetQuantize10 Location = FVector::ZeroVector;
	/** Yaw of the hole / grave (degrees / 360 * 256). */
	UPROPERTY(SaveGame) uint8 YawQ = 0;

	bool IsDugOut() const { return Stage >= MaxStage; }
	float GetYaw() const { return YawQ * (360.0f / 256.0f); }
};

USTRUCT()
struct KILLGODOT_API FKGDigSpotList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FKGDigSpot> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FKGDigSpot, FKGDigSpotList>(Items, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FKGDigSpotList> : public TStructOpsTypeTraitsBase2<FKGDigSpotList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** Owner-only: what the last finished stage gave you (HUD card). */
USTRUCT()
struct KILLGODOT_API FKGDigResult
{
	GENERATED_BODY()

	UPROPERTY() uint8 Serial = 0;
	UPROPERTY() uint16 SpotId = 0;
	UPROPERTY() EKGDigKind Kind = EKGDigKind::Mound;
	UPROPERTY() uint8 Stage = 0;
	UPROPERTY() uint8 MaxStage = 0;
	UPROPERTY() TArray<FKGItemStack> Items;
	/** Some of it did not fit in the pockets: it lies next to the hole. */
	UPROPERTY() bool bDropped = false;
};

/** A region of the map where spots are scattered each match. Units: cm. */
struct KILLGODOT_API FKGDigZoneDef
{
	EKGDigZone Zone = EKGDigZone::None;
	/** Polygon (>= 3 points) or circle (Centre + Radius). */
	TArray<FVector2D> Polygon;
	FVector2D Centre = FVector2D::ZeroVector;
	float Radius = 0.0f;
	int32 Mounds = 0;
	int32 XMarks = 0;
	int32 Glints = 0;
	/** Hidden buried chests (treasure-map targets). */
	int32 Treasures = 0;
	/** Accepted ground height window (cm), e.g. the beach between the waterline and the dunes. */
	float MinZ = -100000.0f;
	float MaxZ = 100000.0f;

	bool Contains(const FVector2D& P) const;
	FBox2D Bounds() const;
};

/** Where a spot could go: the game traces the world, tests use a flat stub. Returns false if XY is not diggable. */
using FKGDigProbe = TFunctionRef<bool(const FVector2D& XY, FVector& OutGround)>;

struct KILLGODOT_API FKGDigRules
{
	/** Closest two spots may be (cm); graves and buried chests keep their own spacing. */
	static constexpr float MinSpacing = 650.0f;
	static constexpr float TreasureSpacing = 2000.0f;
	/** Grave spots stay this far from generated spots (cm). */
	static constexpr float GraveClearance = 260.0f;
	/** From the eye to the aimed ground point (cm). */
	static constexpr float Reach = 280.0f;
	/** A spot counts as aimed at within this distance of the aimed ground point (cm). */
	static float AimRadius(EKGDigKind Kind) { return Kind == EKGDigKind::Grave ? 130.0f : 95.0f; }
	/** Server: the digger's feet must stay within this of the spot (cm). */
	static constexpr float StandRadius = 330.0f;
	/** Digging at a treasure map's X reveals the buried chest within this (cm). */
	static constexpr float TreasureFindRadius = 260.0f;
	/** The server accepts a stage when this much of the hold time really passed (lag margin). */
	static constexpr float HoldTolerance = 0.8f;
	/** Loud stages (graves) are heard by everyone within this (cm). */
	static constexpr float GraveNoiseRadius = 4000.0f;
	/** Map scraps that make one treasure map. */
	static constexpr int32 ScrapsPerMap = 3;

	static uint8 StagesFor(EKGDigKind Kind);
	/** Seconds of holding the mouse per stage. */
	static float HoldSeconds(EKGDigKind Kind);
	/** Loot table of a finished stage: intermediate stages roll the shallow table (may give nothing). */
	static FName LootTable(EKGDigKind Kind, uint8 StageDone, uint8 MaxStage);
	/** Per spot + stage stream: the same seed digs up the same things in the same order (host migration safe). */
	static FKGRng StageRng(uint64 MatchSeed, uint16 SpotId, uint8 Stage);
	static bool CanDigInPhase(EKGPhase Phase);
	/** Graves and buried chests are loud: everyone near hears each finished stage. */
	static bool IsLoud(EKGDigKind Kind) { return Kind == EKGDigKind::Grave; }
	static const TCHAR* KindName(EKGDigKind Kind);
	static const TCHAR* ZoneName(EKGDigZone Zone);

	/** Morrowmere v2 zones (Tools/Level/morrowmere_layout_v2.json: graveyard, shingle beach, fields, woods, orchard). */
	static const TArray<FKGDigZoneDef>& MorrowmereV2Zones();

	/**
	 * Scatters the zones' spots with Rng (deterministic for a seed and a probe). Public spots (mounds, X marks, glints)
	 * go to OutSpots, hidden treasures to OutBuried. Avoid = existing points (graves) to keep GraveClearance from.
	 * Ids continue from NextId (updated).
	 */
	static void Generate(FKGRng& Rng, const TArray<FKGDigZoneDef>& Zones, FKGDigProbe Probe, const TArray<FVector>& Avoid,
	                     TArray<FKGDigSpot>& OutSpots, TArray<FKGDigSpot>& OutBuried, uint16& NextId);

	/** Which buried chest (index into the undug list) the k-th treasure map of a player points at. */
	static int32 TreasureFor(uint32 PlayerHash, int32 MapIndex, int32 NumUndug);

	/** How many whole treasure maps Scraps make (and how many scraps are left). */
	static int32 MapsFromScraps(int32 Scraps, int32& OutLeft)
	{
		OutLeft = Scraps % ScrapsPerMap;
		return Scraps / ScrapsPerMap;
	}

	/** A new spot (Stage 0) of Kind at a ground point. */
	static FKGDigSpot MakeSpot(uint16 Id, EKGDigKind Kind, EKGDigZone Zone, const FVector& Ground, float YawDeg);
};
