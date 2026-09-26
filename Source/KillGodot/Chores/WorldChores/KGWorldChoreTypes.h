#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"

/**
 * SPRINT-016 world chores ("physical chores"): multi-step jobs done with real, carried objects in the village.
 * Data: Tools/Level/morrowmere_world_chores.json -> Tools/Level/gen_world_chores.py -> KGWorldChoreData.gen.inl
 * (dev builds prefer Tools/Level/morrowmere_world_chores.resolved.json when it exists, so data edits need no rebuild).
 * Storm Manor (SPRINT-018): Tools/Level/gen_stormmanor_chores.py -> KGWorldChoreData_StormManor.gen.inl (+ its
 * stormmanor_world_chores.resolved.json). Every map has its own catalog; the world picks it at begin play.
 *
 *   take  : E at the spot -> the chore's item pops out there (then you carry it with hold-E)
 *   work  : E at the spot -> stand there Secs (x Repeat)
 *   bring : your item inside the spot's radius for Secs -> done (automatic; Fill / MinFill for liquids)
 *   panel : E at the spot -> a SPRINT-014 minigame from Stage (bots: BotSecs of standing)
 * A step may target several spots ("all of": doors, lamps, letterboxes), in any order.
 */
enum class EKGWorldVerb : uint8
{
	Take,
	Work,
	Bring,
	Panel
};

/** What a completed step leaves in the world (on the target spot). Several can combine ("stack+fx:ChimneySmoke"). */
namespace KGWorldEffect
{
	constexpr uint8 None = 0;
	constexpr uint8 Water = 1 << 0;   // spot water level += the bucket's fill
	constexpr uint8 Stack = 1 << 1;   // spot count += 1 (crates, loaves on doorsteps, letters, hung nets...)
	constexpr uint8 Light = 1 << 2;   // spot lit
	constexpr uint8 Chop = 1 << 3;    // spot count += 1 per work repeat (split logs on the block)
	constexpr uint8 Fx = 1 << 4;      // an AKGChoreFx visual (bell, chime, lighthouse, chimney smoke)
}

/** Cue sounds played for everyone near a spot (MulticastCue). */
enum class EKGWorldCue : uint8
{
	None,
	Thud,
	Crank,
	Pour,
	Knock,
	Flame,
	Whoosh,
	Knot,
	Chop,
	Paper,
	Grind,
	Splash,
	Poison,
	Snuff,
	StepDone
};

struct KILLGODOT_API FKGWorldItemDef
{
	FName Kind;
	FString Label;
	FString MeshPath;
	FVector MeshScale = FVector::OneVector;
	/** Collision box half extents (cm); the visual mesh sits MeshZ below the box centre. */
	FVector BoxExtent = FVector(15.0f);
	float MeshZ = 0.0f;
	float MassKg = 5.0f;
	/** Walk speed factor while carrying alone (1 = no slowdown; sprinting is capped for heavy items). */
	float Speed = 1.0f;
	bool bTwoPerson = false;
	bool bLiquid = false;
	bool bFlame = false;
	/** Pieces carried (loaves, letters): each "all of" delivery uses one. */
	int32 Count = 1;
	FLinearColor Tint = FLinearColor::White;
	bool bTinted = false;

	bool IsHeavy() const { return Speed < 0.99f; }
};

struct KILLGODOT_API FKGWorldAnchor
{
	FName Id;
	FName Kind;
	FString Label;
	/** Letterbox name plate. */
	FString Name;
	/** World position (cm) of the spot; Z is a hint (the ground is traced at spawn). */
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.0f;
	float RadiusCm = 160.0f;
	/** Where a walker stands (navmesh); defaults to Location. For tower tops: the tower door. */
	FVector Stand = FVector::ZeroVector;
	/** Ladder climb (cm) from Stand to the spot (tower tops): navmesh can't climb, the route check adds it. */
	float ClimbCm = 0.0f;
	/** "poison" (water) / "snuff" (lamps): the Impatient may spoil it once it has been done. */
	FName Sabotage;
};

struct KILLGODOT_API FKGWorldStepDef
{
	EKGWorldVerb Verb = EKGWorldVerb::Work;
	/** Anchor ids, possibly "$var" placeholders resolved per variant. Several = every one of them (any order). */
	TArray<FName> At;
	FName Item;
	float Secs = 1.0f;
	int32 Repeat = 1;
	/** Bring: sets the item's fill (the well). MinFill: the item must hold at least this much. */
	float Fill = -1.0f;
	float MinFill = 0.0f;
	bool bConsume = false;
	/** Take / Work / Panel: the item that pops out when the step completes. */
	FName Spawn;
	uint8 Effects = KGWorldEffect::None;
	FName FxName;
	FName Panel;
	int32 PanelStage = 0;
	float BotSecs = 8.0f;
	EKGWorldCue Cue = EKGWorldCue::None;
	FString Label;
};

struct KILLGODOT_API FKGWorldVariant
{
	FName Name;
	TMap<FName, FName> Vars;
};

struct KILLGODOT_API FKGWorldChoreDef
{
	FName Id;
	FString Title;
	FString Blurb;
	TArray<FName> Replaces;
	bool bBots = true;
	TArray<FKGWorldVariant> Variants;
	TArray<FKGWorldStepDef> Steps;
	// SPRINT-040 hook: manor secrets. A chore with SecretId is never dealt; it is given when that secret is found.
	// RewardSecret / RewardCompartment: discovered / opened for everyone when the chore completes (counted).
	// MinPlayers: not dealt below this many players (UKGManorSubsystem installs FKGWorldChoreRules::DealFilter).
	FName SecretId;
	FName RewardSecret;
	FName RewardCompartment;
	int32 MinPlayers = 0;

	int32 NumSteps() const { return Steps.Num(); }
	int32 NumVariants() const { return FMath::Max(1, Variants.Num()); }
	/** Anchor ids of Step for Variant (placeholders resolved). */
	TArray<FName> Targets(int32 Step, int32 Variant) const;
	FName Resolve(FName AnchorOrVar, int32 Variant) const;
	/** The step's label with $placeholders replaced by the anchors' labels. */
	FString StepLabel(int32 Step, int32 Variant) const;
	/** Any step with a ladder climb (bots can't) - used by the deal. */
	bool NeedsClimb() const;
};

/** The world chore data for one map (parsed once; dev builds may reload it from disk). */
class KILLGODOT_API FKGWorldChoreCatalog
{
public:
	/** The active map's catalog (empty when the data is broken). One catalog per map (SPRINT-018: Morrowmere v2 and
	 *  Storm Manor); the first one is active until SelectForWorld picks another. */
	static const FKGWorldChoreCatalog& Get();
	/** Make the catalog whose MapName matches the world's map the active one (no change when none matches). */
	static bool SelectForWorld(const UWorld* World);
	/** Tests / tools: the catalog of a map by level name (nullptr when there is none). */
	static const FKGWorldChoreCatalog* FindByMap(const FString& InMapName);
	/** Tests / tools: parse a JSON text (returns false + error). */
	bool Parse(const FString& Json, FString& OutError);
	/** Dev: re-read every map's resolved JSON from disk (kg.WorldChore.Reload). */
	static bool Reload(FString& OutMessage);

	FString MapName;
	float WalkSpeed = 320.0f;   // cm/s
	float ClimbSpeed = 260.0f;  // cm/s
	FVector Hub = FVector::ZeroVector;
	TArray<FKGWorldItemDef> Items;
	TArray<FKGWorldAnchor> Anchors;
	TArray<FKGWorldChoreDef> Chores;

	const FKGWorldChoreDef* FindChore(FName Id) const;
	const FKGWorldItemDef* FindItem(FName Kind) const;
	int32 AnchorIndex(FName Id) const;
	const FKGWorldAnchor* FindAnchor(FName Id) const;
	bool IsWorldChore(FName Id) const { return FindChore(Id) != nullptr; }
	bool ForMap(const UWorld* World) const;
	/** True when it has chores and every step target resolves (tests). */
	bool Validate(FString& OutError) const;

private:
	TMap<FName, int32> AnchorLookup;
};

class UKGWorldChoreComponent;
/** SPRINT-040 hook: a world chore was completed and counted (not faked by the Impatient). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FKGOnWorldChoreDone, UKGWorldChoreComponent*, FName);

/** Pure rules (unit-tested in KillGodot.WorldChores.*). */
struct KILLGODOT_API FKGWorldChoreRules
{
	/** SPRINT-040 hook: optional deal filter (false = never deal this chore); no filter = the SPRINT-016 deal. */
	static TFunction<bool(const FKGWorldChoreDef&)> DealFilter;
	/** SPRINT-040 hook: broadcast by UKGWorldChoreComponent::CompleteChore when the chore counted. */
	static FKGOnWorldChoreDone OnChoreDone;

	/** Target share of world chores in a dealt list (acceptance 4: about 70 / 30). */
	static constexpr float WorldShare = 0.7f;
	/** Sprinting with water spills this much per second; a fall/jump more. */
	static constexpr float SprintSpillPerSec = 0.16f;
	static constexpr float FallSpillPerSec = 0.35f;
	/** A tipped bucket (not carried) empties fast. */
	static constexpr float TippedSpillPerSec = 0.9f;
	/** Carrier speed above WalkSpeed * this counts as running. */
	static constexpr float RunThreshold = 1.15f;
	/** Server check: a work/panel/take walker must stand within the spot radius + this. */
	static constexpr float ReachSlackCm = 90.0f;
	/** Owner may be this far from their item when a helper (or gravity) delivers it. */
	static constexpr float OwnerNearCm = 600.0f;
	/** Impatient bots fake a world chore for this long, then drop the item and get back to hunting. */
	static constexpr float BotFakeSecs = 25.0f;
	/** A delivery that lands while its owner stands at least this far from the spot was thrown in ("Nice throw!"). */
	static constexpr float ThrowInCm = 300.0f;
	/** Sabotage takes this long (standing at the spot). */
	static constexpr float SabotageSecs = 3.0f;
	/** A poisoned trough: dumping it first. A snuffed lamp: cleaning the soot adds this. */
	static constexpr float DumpSecs = 3.0f;
	static constexpr float SootExtraSecs = 2.0f;

	/**
	 * Deal Count chores from Pool (station ids of the level) with about WorldShare world chores. bBot: bots never get
	 * chores with a ladder climb. Panel chores a dealt world chore "replaces" are avoided. Deterministic (Rng).
	 */
	static TArray<FName> Deal(const TArray<FName>& Pool, const FKGWorldChoreCatalog& Catalog, bool bBot, int32 Count, FKGRng& Rng);

	/** Carry speed factor for an item with Carriers people holding it. */
	static float CarrySpeedFactor(const FKGWorldItemDef& Item, int32 Carriers);

	/** New fill after Dt: carried (Speed = fastest carrier's horizontal speed, bFalling) or lying (UpZ = up vector Z). */
	static float SpillFill(float Fill, float Dt, bool bCarried, float CarrierSpeed, float WalkSpeed, bool bFalling, float UpZ);

	/** Time (s) a step's dwell takes at a spot in its current state (poisoned water, sooty lamp). */
	static float DwellSecs(const FKGWorldStepDef& Step, bool bSpoiled);

	/** Variant for a player (stable per chore and player). */
	static int32 PickVariant(const FKGWorldChoreDef& Def, uint32 PlayerHash);
};

namespace KGWorldChores
{
	/** Cue name for sounds. */
	KILLGODOT_API const TCHAR* CueSound(EKGWorldCue Cue);
	KILLGODOT_API FString VerbName(EKGWorldVerb Verb);
}
