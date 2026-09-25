#pragma once

#include "CoreMinimal.h"
#include "Core/KGTypes.h"

/**
 * Visible ("visual") chore effects: completing one of these chores changes the world for everybody, so onlookers can
 * vouch for whoever did it (Among Us visual tasks). The Impatient can fake the chore, but the effect never plays for
 * a fake. Played by AKGChoreFx on every machine.
 */
enum class EKGChoreFx : uint8
{
	None,
	BellRing,        // RingBell: the church bell tolls village-wide (3 strikes)
	LighthouseGlow,  // FuelLighthouse: the lighthouse lamp blazes brighter for a while
	ChimneySmoke,    // BakeBread: smoke rises from the bakery chimney
	NoticePosted,    // PostNotice: a fresh notice appears on the board
	CandlesLit,      // LightCandles: the church candles glow
	LampLit,         // LightHarbourLamp: the harbour lamp is lit
	ClockChime,      // WindClock: the town clock chimes
	WoodPile         // ChopWood: split logs stack up at the camp
};

/** One chore: its minigame, stages and the server's plausibility floor per stage. */
struct KILLGODOT_API FKGChoreDef
{
	FName Id;
	/** Panel title, e.g. "Draw water". */
	FString Title;
	/** Short stage names shown as pips ("Crank", "Carry"). Size = number of stages. */
	TArray<FString> Stages;
	/**
	 * Server anti-cheat floor (seconds) per stage: a stage reported faster than this (measured by the server from the
	 * previous accepted stage) is rejected. Roughly 40% of a typical player's time; a perfect human stays above it.
	 */
	TArray<float> StageMinSeconds;
	EKGChoreFx Fx = EKGChoreFx::None;
	/** Optional side chores (not dealt by default). */
	bool bOptional = false;

	int32 NumStages() const { return Stages.Num(); }
	bool IsVisual() const { return Fx != EKGChoreFx::None; }
	float MinTotalSeconds() const;
};

/** Every chore that has a minigame (a station without an entry here falls back to hold-E). */
class KILLGODOT_API FKGChoreCatalog
{
public:
	static const TArray<FKGChoreDef>& GetAll();
	static const FKGChoreDef* Find(FName Id);
	static FString FxName(EKGChoreFx Fx);
};

/** Why the server refused a stage report (or accepted it). */
enum class EKGChoreVerdict : uint8
{
	Accepted,
	NoSession,    // nothing open (stale RPC, or the session was interrupted)
	WrongChore,
	WrongToken,   // report from an older session
	WrongStage,   // skipped or repeated a stage
	TooFast,      // below the stage's plausibility floor
	Interrupted   // phase / death / moved away (checked every tick, reported here for tests)
};

/** Why an open minigame closed. Sent to the owner (ClientClose) and logged. */
enum class EKGChoreClose : uint8
{
	Completed,
	Left,       // the player closed it (Esc / X / moved) - progress saved
	Moved,      // pushed away from the station (shove, knock-back)
	Hit,        // took damage
	Phase,      // a meeting / trial / curfew began
	Died,
	Replaced    // another minigame opened
};

/** Pure server rules (unit-tested in KillGodot.Chores.*). */
struct KILLGODOT_API FKGChoreRules
{
	/** Fraction of StageMinSeconds the server accepts (network jitter can only make the server time longer). */
	static constexpr float TimingSlack = 0.9f;
	/** Pushed further than this from where the minigame opened = interrupted. */
	static constexpr float MaxDriftCm = 110.0f;

	/** Can chores be opened / worked on in this phase? */
	static bool PhaseAllowsChores(EKGPhase Phase);

	/**
	 * Validates one stage report. ServerStageSeconds = time the server measured since the stage began (open, or the
	 * previous stage's acceptance).
	 */
	static EKGChoreVerdict CheckStage(const FKGChoreDef& Def, FName ReportedChore, int32 ReportedStage, int32 ReportedToken,
	                                  FName SessionChore, int32 SessionStage, int32 SessionToken, float ServerStageSeconds);

	static const TCHAR* VerdictName(EKGChoreVerdict Verdict);
	static const TCHAR* CloseName(EKGChoreClose Reason);
};
