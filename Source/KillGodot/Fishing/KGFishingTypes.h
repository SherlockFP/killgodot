#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "Core/KGTypes.h"
#include "KGFishingTypes.generated.h"

class AActor;
class UWorld;

/**
 * Fishing rules (Docs/01b_Village_Life_Fun.md §2 "Balık Tutma", GDD fishing section): pure data + math shared by the
 * server (authority), the owning client (prediction) and the automation tests. Nothing here touches the world except
 * FKGFishingWater / FKGCastSim, which only read it (traces, water planes, fish schools, the wave function).
 */

/** Where a bobber can land. KoiPond is the Sakura Garden pond: fishing there is sacrilege (the "koi tax"). */
UENUM()
enum class EKGFishWater : uint8
{
	None,      // still flying / nothing
	Land,      // hit ground, a roof, a wall: snagged
	Sea,       // open sea beyond the mole
	Basin,     // the sheltered harbour basin (FKGWaves calm zone)
	Brook,     // Morrow Brook + mill pond (M_KG_PondWater planes)
	KoiPond,   // forbidden
	Count UMETA(Hidden)
};

/** Replicated fishing phase of one villager (everyone sees it: rod, line, bobber, the fight). */
UENUM()
enum class EKGFishPhase : uint8
{
	Stowed,    // rod put away
	Ready,     // rod in hand, line wound in
	Charging,  // holding LMB: power builds (cosmetic for others: wind-up)
	Flight,    // bobber in the air along the cast arc
	Waiting,   // bobber on the water: nibbles, then a bite
	Bite,      // bobber pulled under: hook-set window
	Fight,     // reel minigame
	Landed,    // caught something (trophy on the line)
	Snapped,   // line snapped (tension too high too long)
	Escaped,   // fish got away (slack / flinch / missed hook)
	Snagged,   // the cast hit land / a roof: reel back
	KoiTax,    // cast into the koi pond: fined, reel back
};

/** Why a fight ended (FKGReelSim). */
enum class EKGReelResult : uint8
{
	None,
	Landed,
	Snapped,
	Escaped
};

/** Coarse time of day for bite tables. */
enum class EKGFishTime : uint8
{
	Day,
	Dawn,
	Night
};

/** One catchable species (the table in FKGFishingRules::Species, documented in Docs/01_GDD_Core.md §15). */
struct KILLGODOT_API FKGFishSpecies
{
	FName Id;
	/** Inventory item (UKGItemCatalog); None for the sacred koi. */
	FName ItemId;
	/** Relative abundance before water/time multipliers. */
	float Base = 1.0f;
	/** Multiplier per EKGFishWater (index = enum value). */
	float Water[static_cast<int32>(EKGFishWater::Count)] = {};
	float Day = 1.0f;
	float Dawn = 1.0f;
	float Night = 1.0f;
	float MinKg = 0.5f;
	float MaxKg = 1.0f;
	/** Fight: base pull 0..1, seconds of good-tension reeling to tire it, direction changes per second, lateral speed. */
	float Pull = 0.3f;
	float StaminaSec = 5.0f;
	float Agility = 0.5f;
	float Lateral = 1.0f;
	/** Hook-set window after the real bite (seconds). */
	float HookWindow = 0.8f;
	/** AKGFishSchool::Species that attracts this fish (schools near the bobber multiply its chance). */
	FName School;
	/** How much a nearby school of its own kind multiplies its chance (golden carp: a lot). */
	float SchoolAffinity = 3.0f;
	/** Koi: never catchable, the pond is forbidden. */
	bool bSacred = false;

	float TimeWeight(EKGFishTime Time) const { return Time == EKGFishTime::Night ? Night : Time == EKGFishTime::Dawn ? Dawn : Day; }
	float WaterWeight(EKGFishWater W) const { return Water[FMath::Clamp(static_cast<int32>(W), 0, static_cast<int32>(EKGFishWater::Count) - 1)]; }
};

/** A school near the bobber, as the bite roll sees it. */
struct KILLGODOT_API FKGSchoolSample
{
	FName Species;
	int32 Count = 0;
	float Distance = 0.0f;   // cm, bobber to school centre (2D)
};

/** Nibbles then the bite, in seconds after the bobber settled. */
struct KILLGODOT_API FKGBiteSchedule
{
	float BiteAt = 5.0f;
	TArray<float, TInlineAllocator<4>> Nibbles;   // ascending, all < BiteAt
};

/** What the server decided the bite is (species index or junk item) and its weight. */
struct KILLGODOT_API FKGBiteRoll
{
	int32 Species = INDEX_NONE;   // FKGFishingRules::Species() index, INDEX_NONE = junk/treasure
	FName ItemId;                 // item that ends up in the pockets (fish item or junk)
	int32 Grams = 0;              // fish weight (0 for junk)
	bool IsJunk() const { return Species == INDEX_NONE; }
};

/** Static tables + formulas. */
struct KILLGODOT_API FKGFishingRules
{
	static const TArray<FKGFishSpecies>& Species();
	static const FKGFishSpecies* Find(FName Id);
	static int32 IndexOf(FName Id);
	/** Species whose inventory item is ItemId, or null. */
	static const FKGFishSpecies* FindByItem(FName ItemId);

	/** Rod out / casting allowed in this phase (not in meetings, trials, the role reveal or a migration). */
	static bool CanFishInPhase(EKGPhase Phase);
	static EKGFishTime TimeFor(EKGPhase Phase);

	/** Chance (0..1) that a bite is junk/treasure (Fishing loot table) instead of a fish. */
	static float JunkChance(EKGFishWater Water);

	/** Extra bite-rate factor from schools near the bobber (0 = none, up to 1.5). */
	static float SchoolBonus(const TArray<FKGSchoolSample>& Schools);

	/** Bites per second (exponential waiting time). */
	static float BiteRate(EKGFishWater Water, EKGFishTime Time, float SchoolBonus);

	/** Unnormalised chance of one species for this spot. */
	static float SpeciesWeight(const FKGFishSpecies& Species, EKGFishWater Water, EKGFishTime Time,
	                           const TArray<FKGSchoolSample>& Schools);

	/** Nibbles + bite time. */
	static FKGBiteSchedule RollSchedule(FKGRng& Rng, float BiteRate);

	/** Species (or junk via the "Fishing" loot table) + weight. Sacred water yields nothing (ItemId None). */
	static FKGBiteRoll RollBite(FKGRng& Rng, EKGFishWater Water, EKGFishTime Time, const TArray<FKGSchoolSample>& Schools);

	/** Weight in grams, skewed towards the light end (a trophy fish is rare). */
	static int32 RollGrams(FKGRng& Rng, const FKGFishSpecies& Species);

	/** 0 (lightest) .. 1 (heaviest) for a weight of this species. */
	static float WeightAlpha(const FKGFishSpecies& Species, int32 Grams);

	/** Rarity tier of a catch (the item's rarity). */
	static int32 SellPrice(FName ItemId, int32 Count, int32 TotalGrams);

	/** Items the Fish Market buys (fish + sea junk/treasure). */
	static bool IsSellable(FName ItemId);

	/** Seconds the hook window stays open on the server (species window + latency allowance). */
	static float ServerHookWindow(float SpeciesWindow, float PingSeconds);

	/** Messages in bottles (index replicated with the catch). */
	static int32 NumBottleMessages();
	static FText BottleMessage(int32 Index);

	// ---- cast ----
	/** Launch velocity (cm/s) for a view direction and a charge 0..1. */
	static FVector CastVelocity(const FVector& ViewDir, float Power);
	static constexpr float CastGravity = 980.0f;
	static constexpr float MaxLineLength = 3500.0f;   // cm: walk further from the bobber and the line breaks off
	static constexpr float ChargePeriod = 2.4f;       // s: full power at half the period, then it falls again

	/** Power 0..1 after holding the charge for Seconds (ping-pong, so over-holding loses power). */
	static float ChargePower(float Seconds);
};

/** One step of player input for the reel minigame. */
struct KILLGODOT_API FKGReelInput
{
	bool bReel = false;
	/** -1 = steer left (A), +1 = steer right (D). */
	float Steer = 0.0f;
};

/**
 * The reel minigame, deterministic: fixed 60 Hz substeps, its own FKGRng for the fish's direction changes (which only
 * depend on elapsed fight time), so the server and the owning client's prediction agree for the same inputs.
 *
 * Tension rises while you hold the reel (more with a strong, fresh fish, less while you steer against its pull) and
 * falls when you let go. Tension >= 1 for SnapSeconds snaps the line, tension <= SlackTension for SlackSeconds lets
 * the fish throw the hook. Good tension tires the fish (Stamina -> 0) and winds it in (Distance -> LandDistance).
 */
USTRUCT()
struct KILLGODOT_API FKGReelSim
{
	GENERATED_BODY()

	static constexpr float StepSeconds = 1.0f / 60.0f;
	static constexpr float SnapSeconds = 0.8f;
	static constexpr float SlackTension = 0.12f;
	static constexpr float SlackSeconds = 2.0f;
	static constexpr float GoodMin = 0.25f;
	static constexpr float LandDistance = 1.2f;   // m
	static constexpr float MaxSeconds = 75.0f;
	static constexpr float ReelSpeed = 2.2f;      // m/s with a tired fish

	// Parameters
	UPROPERTY() float Pull = 0.3f;
	UPROPERTY() float StaminaSec = 5.0f;
	UPROPERTY() float Agility = 0.5f;
	UPROPERTY() float Lateral = 1.0f;
	UPROPERTY() float WeightMul = 1.0f;
	UPROPERTY() bool bJunk = false;

	// State (replicated to the owner for reconciliation)
	UPROPERTY() float Tension = 0.3f;
	UPROPERTY() float Stamina = 1.0f;
	UPROPERTY() float Distance = 10.0f;   // m
	UPROPERTY() float FishX = 0.0f;       // -1 (left) .. 1 (right), for visuals
	UPROPERTY() float Elapsed = 0.0f;
	UPROPERTY() float OverTime = 0.0f;
	UPROPERTY() float SlackTime = 0.0f;
	UPROPERTY() float NextSwitch = 1.0f;
	/** When the fish last changed direction: it surges right after (release or snap). */
	UPROPERTY() float LastSwitch = -10.0f;
	UPROPERTY() int8 PullDir = 1;         // -1 fish pulls left, +1 right
	UPROPERTY() FKGRng Rng;
	float Accum = 0.0f;
	EKGReelResult Result = EKGReelResult::None;

	/** Species null = junk (a boot does not fight). WeightAlpha 0..1, StartDistance in metres. */
	void Init(const FKGFishSpecies* Species, float WeightAlpha, float StartDistance, uint64 Seed);

	/** Advances by DeltaSeconds in fixed substeps with a constant input. Stops at a result. */
	void Advance(float DeltaSeconds, const FKGReelInput& Input);

	/** One fixed step. */
	void Step(float H, const FKGReelInput& Input);

	/** Current pull strength (with stamina and surges). */
	float CurrentPull() const;

	bool IsOver() const { return Result != EKGReelResult::None; }

	/** Blend towards an authoritative state (owner prediction), adopting the discrete fish schedule exactly. */
	void Reconcile(const FKGReelSim& Server, float Alpha);

	/** A "perfect" player for tests and bots: reel while tension is comfortable, steer against the pull. */
	static FKGReelInput ExpertInput(const FKGReelSim& Sim);
};

/** Where and on what a cast lands. */
struct KILLGODOT_API FKGCastResult
{
	FVector Landing = FVector::ZeroVector;   // bobber rest point (Z = water surface at landing time)
	float FlightTime = 0.0f;
	EKGFishWater Water = EKGFishWater::None;
	/** Fresh-water plane height (brook / ponds); sea uses FKGWaves. */
	float SurfaceZ = 0.0f;
};

/** Water queries (every machine; cached per world). */
struct KILLGODOT_API FKGFishingWater
{
	/**
	 * What is under XY at height Z: a fresh-water plane (M_KG_PondWater, the brook ribbons, the mill pond, the koi pond:
	 * the plane near a Koi fish school), else the sea (FKGWaves) when the ground is below the moving surface.
	 * OutSurfaceZ = water height. Returns EKGFishWater::Land when there is ground above the water.
	 */
	static EKGFishWater Classify(UWorld* World, const FVector& Point, float Time, float& OutSurfaceZ,
	                             const AActor* Ignore = nullptr);

	/** Fresh-water surface height at XY if a pond/brook plane covers it (below MaxZ). */
	static bool FreshSurfaceAt(UWorld* World, const FVector2D& XY, float MaxZ, float& OutZ, bool& bOutKoi);

	/** Schools within Radius (cm) of Point (2D). */
	static void GatherSchools(UWorld* World, const FVector& Point, float Radius, TArray<FKGSchoolSample>& Out);

	/** Drops the per-world caches (tests, level changes). */
	static void ResetCache();
};

/** Ballistic cast arc against the world (same on server and owner, so the owner can predict the landing). */
struct KILLGODOT_API FKGCastSim
{
	static constexpr float Dt = 1.0f / 30.0f;
	static constexpr float MaxFlight = 3.5f;

	/** Position along the arc t seconds after launch (no collision). */
	static FVector PointAt(const FVector& Origin, const FVector& Velocity, float T);

	/** Traces the arc; stops at the first blocking hit or water crossing. */
	static FKGCastResult Simulate(UWorld* World, const FVector& Origin, const FVector& Velocity, float Time,
	                              const AActor* Ignore);
};
