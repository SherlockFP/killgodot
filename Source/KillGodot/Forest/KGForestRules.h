#pragma once

#include "CoreMinimal.h"
#include "Forest/KGForestTypes.h"

/** Everything the wolf interest meter reads for one player (sampled 4 Hz on the server). */
struct KILLGODOT_API FKGWolfInputs
{
	EKGForestBand Band = EKGForestBand::Village;
	/** On a static safe cell (village, trail) or inside a lit fixed light (lantern, camp fire). */
	bool bSafe = true;
	bool bNight = false;
	int32 DayIndex = 1;
	/** Living players within KG_GROUP_R including this one (1 = alone). */
	int32 GroupSize = 1;
	bool bTorch = false;
	bool bRawFish = false;
	float Health = 100.0f;
	/** Running in the forest, not toward the nearest path (+3/s). */
	bool bRunningAstray = false;
};

/** One player's wolf state on the server. */
struct KILLGODOT_API FKGWolfTrack
{
	float Interest = 0.0f;
	EKGWolfStage Stage = EKGWolfStage::Silent;
	float StageAge = 0.0f;
	/** Seconds since this player's telegraph stamp (< 0 = none). The first bite waits for KG_WOLF_TELEGRAPH_MIN. */
	float StampAge = -1.0f;
	float SinceBite = 99.0f;
	int32 Bites = 0;
};

struct KILLGODOT_API FKGWolfStep
{
	bool bStamped = false;      // the telegraph stamp was set this step
	bool bHowl = false;         // an audible howl starts (no live howl in the sector)
	bool bEyes = false;         // entered the eyes stage
	bool bAttack = false;       // entered the attack stage
	bool bCalmed = false;       // dropped back to silent
	bool bBiteReady = false;    // attack stage and the per-target bite cooldown is over
};

/** One player's Mist state on the server. */
struct KILLGODOT_API FKGMistTrack
{
	float Notice = 0.0f;
	EKGMistStage Stage = EKGMistStage::None;
	/** Seconds since the frost (first visible sign), < 0 = none. */
	float FrostAge = -1.0f;
	/** Seconds spent inside the tongue's core in a row. */
	float CoreSecs = 0.0f;
};

/** Camp Vigil reward sizes (design 10: B / night cap / match cap). */
struct KILLGODOT_API FKGVigilCaps
{
	int32 PerSigner = 1;
	int32 NightCap = 1;
	int32 MatchCap = 2;
};

/** One vigil night, as the server measured it. */
struct KILLGODOT_API FKGVigilNight
{
	int32 Signers = 0;
	/** Every signer inside the ring before the arrival deadline. */
	bool bAllArrived = false;
	/** The largest total time any signer spent outside the ring after arriving. */
	float MaxOutsideSecs = 0.0f;
	/** Fire lit share of the night after the arrival window (0..1). */
	float LitShare = 0.0f;
};

/**
 * Pure forest rules (unit-tested in KillGodot.Forest.*). No world access; seconds and metres.
 * Timing contract: a player gets at least KG_WOLF_TELEGRAPH_MIN (20 s) between their own howl stamp and the first bite,
 * and at least KG_MIST_TELEGRAPH_MIN (25 s) between the frost and a Mist death.
 */
struct KILLGODOT_API FKGForestRules
{
	// ---- wolves ---------------------------------------------------------------------------------------------------
	/** Interest change per second (negative = decay). */
	static float WolfGain(const FKGWolfInputs& In);
	/** Advances one track by Dt with the given gain. bSectorHowlLive: a howl is already sounding in this sector. */
	static FKGWolfStep StepWolf(FKGWolfTrack& T, float Gain, float Dt, bool bSectorHowlLive);
	/** A bite happened: the cooldown starts and the pack backs off (hit and run). */
	static void OnBite(FKGWolfTrack& T);
	/** Repel (shove / torch holder bitten): interest -40. */
	static void OnRepel(FKGWolfTrack& T);
	/** Active wolves at most (2 / 3 / 6 by lobby size). */
	static int32 WolfCap(int32 NumPlayers);
	/** Packs and wolves per pack. */
	static int32 PackCount(int32 NumPlayers) { return NumPlayers >= 15 ? 2 : 1; }
	static int32 PackSize(int32 NumPlayers) { return NumPlayers >= 10 ? 3 : 2; }
	/** Sectors with a live den (1 West, 2 Cave Ridge, 3 North, 4 East Ridge). */
	static bool IsSectorOpen(int32 Sector, int32 NumPlayers);
	/** Bite damage (0 when PvE is not lethal: the bite only staggers). */
	static float BiteDamage(bool bLethal) { return bLethal ? KGForest::BiteDamage : 0.0f; }
	/**
	 * Two players in a group at night: who the pack goes for. Farther from the nearest light / safe cell first, then
	 * higher interest, then the lower player key. Returns 0 or 1.
	 */
	static int32 PickPairTarget(float SafeDistA, float InterestA, const FString& KeyA, float SafeDistB, float InterestB,
	                            const FString& KeyB);

	// ---- the Mist -------------------------------------------------------------------------------------------------
	/** Notice meter fill per second (100 = a tongue): 40 s by day, 20 s by night, twice as long with a torch. */
	static float MistNoticeRate(bool bNight, bool bTorch);
	/** Whether the notice meter fills: alone in the Deep band, not AFK-frozen, not in the first day/night. */
	static bool MistNoticeActive(EKGForestBand Band, int32 GroupSize, bool bAfkFrozen, int32 DayIndex);
	/** Advances the notice meter. Returns true on the step the tongue should spawn. */
	static bool StepMistNotice(FKGMistTrack& T, bool bActive, float Dt, bool bNight, bool bTorch);
	/** Tongue speed (m/s) after Age seconds: 2.4 +0.08/s, at most 4.2. */
	static float MistSpeed(float Age);
	/** Distance the tongue has covered after Age seconds. */
	static float MistDistance(float Age);
	/**
	 * Spawn point: 40 m from the player, on the side away from the nearest safe cell, outside the view cone (60 deg
	 * half-angle; rotated in 20 deg steps until it is). 2D, metres.
	 */
	static FVector2D MistSpawn(const FVector2D& Player, const FVector2D& NearestSafe, const FVector2D& ViewDir);
	/** Core contact: returns true when the player has been inside the core for KG_MIST_CORE seconds in a row. */
	static bool StepMistCore(FKGMistTrack& T, float Dist, float Dt);
	/** Tongues at most (1 / 2 / 3 by lobby size). */
	static int32 MistCap(int32 NumPlayers);

	// ---- safety (P1b + F1) ----------------------------------------------------------------------------------------
	/**
	 * KG_PVE_LETHAL: a wolf bite / Mist catch may kill only with N >= 10, more than max(5, 2 * threats + 1) alive,
	 * after the first day/night, and while the session's PvE death share stays within 5% (judged over >= 20 deaths).
	 */
	static bool IsPvELethal(int32 NumPlayers, int32 Alive, int32 Threats, int32 DayIndex, int32 SessionPvEDeaths,
	                        int32 SessionDeaths);
	/** Endgame guard alone (true = the game is near its end, the forest never kills). */
	static bool IsEndgame(int32 Alive, int32 Threats) { return Alive <= FMath::Max(5, 2 * Threats + 1); }

	// ---- camp fire + vigil ----------------------------------------------------------------------------------------
	static float FireStep(float Fuel, float Dt) { return FMath::Max(0.0f, Fuel - KGForest::FireBurn * Dt); }
	static float FireAddLog(float Fuel) { return FMath::Min(KGForest::FireFuelMax, Fuel + KGForest::FireLog); }
	static float FireSafeRadius(float Fuel);
	static int32 VigilMaxSigners(int32 NumPlayers) { return NumPlayers <= 7 ? 2 : 4; }
	static constexpr int32 VigilMinSigners = 2;
	/** Seconds after dusk by which every signer must stand in the ring. */
	static float VigilArriveBy(float NightSecs);
	static bool VigilOpen(int32 DayIndex) { return DayIndex >= 2; }
	static FKGVigilCaps VigilCaps(int32 NumPlayers);
	static bool VigilSuccess(const FKGVigilNight& Night);
	/**
	 * Task-equivalents a successful vigil adds to the Preparation bar. Side-blind (the signers' roles never enter),
	 * capped per night and per match, and never more than the living Town players' open tasks.
	 */
	static int32 VigilUnits(const FKGVigilNight& Night, int32 NumPlayers, int32 MatchGranted, int32 OpenLivingTownTasks);
};
