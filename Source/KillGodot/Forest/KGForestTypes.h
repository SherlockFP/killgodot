#pragma once

#include "CoreMinimal.h"

/**
 * SPRINT-033/034: forest threats (wolves, the Mist) and forest chores / the Camp Vigil.
 * Design: Docs/Design/Forest_Threats_Chores.md (section 3 = the named constants below). Units: seconds, metres
 * (the rules are pure and unit-agnostic; actors convert to cm).
 */
namespace KGForest
{
	// ---- bands (section 2) ----------------------------------------------------------------------------------------
	constexpr float Edge = 12.0f;                 // KG_FOREST_EDGE
	constexpr float Deep = 30.0f;                 // KG_FOREST_DEEP
	constexpr float SafeMax = 45.0f;              // KG_FOREST_SAFE_MAX
	constexpr float GroupRadius = 8.0f;           // KG_GROUP_R
	constexpr float FireSafe = 10.0f;             // KG_FIRE_SAFE (6 m under 20 fuel)
	constexpr float FireSafeLow = 6.0f;
	constexpr float LanternSafe = 4.0f;           // KG_LANTERN_SAFE
	constexpr float AfkFreeze = 10.0f;            // KG_FOREST_AFK
	constexpr float AfkReturn = 20.0f;

	// ---- wolves (section 4) ---------------------------------------------------------------------------------------
	constexpr float HowlAt = 35.0f;
	constexpr float EyesAt = 65.0f;
	constexpr float AttackAt = 100.0f;
	constexpr float HowlExit = 20.0f;
	constexpr float EyesExit = 35.0f;
	constexpr float AttackExit = 65.0f;
	constexpr float HowlMin = 12.0f;              // KG_WOLF_HOWL_MIN
	constexpr float EyesMin = 8.0f;               // KG_WOLF_EYES_MIN
	constexpr float TelegraphMin = 20.0f;         // KG_WOLF_TELEGRAPH_MIN
	constexpr float BiteDamage = 20.0f;           // KG_WOLF_BITE
	constexpr float BiteCooldown = 4.0f;
	constexpr float BiteStagger = 0.4f;
	constexpr float BiteStamina = 15.0f;
	constexpr float WolfSneak = 2.2f;             // KG_WOLF_SPEED
	constexpr float WolfTrot = 4.0f;
	constexpr float WolfLunge = 6.4f;
	constexpr float WolfLungeMax = 4.0f;
	constexpr float RepelSecs = 15.0f;            // KG_WOLF_REPEL
	constexpr float RepelInterest = 40.0f;
	constexpr float HitAndRun = 3.0f;
	constexpr float EyesRingMin = 15.0f;
	constexpr float EyesRingMax = 22.0f;
	constexpr float LungeFrom = 6.0f;
	constexpr float HearHowl = 160.0f;            // KG_HEAR_HOWL
	constexpr float HowlSectorR = 60.0f;
	constexpr float HowlSectorCooldown = 45.0f;
	constexpr float SafeDecay = 8.0f;
	constexpr float IdleDecay = 3.0f;
	constexpr float PlayerWalk = 3.2f;
	constexpr float PlayerRun = 5.8f;
	constexpr float PlayerRunSecs = 5.5f;         // stamina 100, -18/s

	// ---- the Mist (section 5) -------------------------------------------------------------------------------------
	constexpr float MistNoticeDay = 40.0f;        // KG_MIST_NOTICE
	constexpr float MistNoticeNight = 20.0f;
	constexpr float MistFrostAt = 50.0f;          // frost (first visible sign) at half the notice meter
	constexpr float MistSpawnBehind = 40.0f;      // KG_MIST_SPAWN
	constexpr float MistSpeed0 = 2.4f;            // KG_MIST_SPEED
	constexpr float MistAccel = 0.08f;
	constexpr float MistSpeedMax = 4.2f;
	constexpr float MistCore = 3.0f;              // KG_MIST_CORE
	constexpr float MistCoreSecs = 3.0f;
	constexpr float MistTelegraphMin = 25.0f;     // KG_MIST_TELEGRAPH_MIN
	constexpr float MistFadeSecs = 2.5f;
	constexpr float WallReturnSecs = 3.0f;

	// ---- safety (P1b, F1) -----------------------------------------------------------------------------------------
	constexpr int32 LethalMinPlayers = 10;        // KG_PVE_LETHAL: N >= 10 ...
	constexpr float PvEShareCap = 0.05f;          // F1: PvE deaths <= 5% of all deaths
	constexpr int32 PvEShareWindow = 20;          // the share is judged over at least this many deaths

	// ---- camp fire + vigil (sections 7, 10) -----------------------------------------------------------------------
	constexpr float FireFuelMax = 100.0f;         // KG_FIRE_FUEL
	constexpr float FireBurn = 2.5f;
	constexpr float FireLog = 50.0f;
	constexpr float FireLowFuel = 20.0f;
	constexpr float VigilRing = 12.0f;            // KG_VIGIL
	constexpr float VigilArriveShare = 0.55f;
	constexpr float VigilArriveMin = 60.0f;
	constexpr float VigilOutsideMax = 15.0f;
	constexpr float VigilLitShare = 0.9f;
	constexpr float BotFeedBelow = 40.0f;
}

/** Band byte of the raster (bits 0-2) + flags. */
enum class EKGForestBand : uint8
{
	Village = 0,
	Edge = 1,
	Middle = 2,
	Deep = 3,
	Out = 4
};

namespace KGForestFlag
{
	constexpr uint8 BandMask = 0x07;
	constexpr uint8 NotWalkable = 0x40;
	constexpr uint8 Safe = 0x80;
}

/** What a player sees and hears of the wolves (the interest number itself never leaves the server). */
enum class EKGWolfStage : uint8
{
	Silent,
	Howl,
	Eyes,
	Attack
};

enum class EKGMistStage : uint8
{
	None,
	Frost,
	Tongue
};

KILLGODOT_API const TCHAR* KGWolfStageName(EKGWolfStage S);
KILLGODOT_API const TCHAR* KGForestBandName(EKGForestBand B);
