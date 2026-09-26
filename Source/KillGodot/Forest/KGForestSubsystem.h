#pragma once

#include "CoreMinimal.h"
#include "Forest/KGForestRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGForestSubsystem.generated.h"

class AKGBotController;
class AKGCharacter;
class AKGCampfire;
class AKGForestPlayerInfo;
class AKGMistTongue;
class AKGPlayerState;
class AKGVigilBook;
class AKGWolf;

/**
 * SPRINT-033/034 forest director (server authority; clients only get the replicated actors). Morrowmere v2 only.
 * - Per player: band, wolf interest/stage (telegraph stamp), Mist notice/frost/tongue, AFK, the owner-only info actor.
 * - Wolves (AKGWolf) woken per pack at the den, capped by lobby size, never on a village or lit-light cell.
 * - Mist tongues (AKGMistTongue) behind lone players in the Deep band; walking to a path or a light always escapes.
 * - Safety: FKGForestRules::IsPvELethal (N <= 9, endgame, first day/night, the 5% PvE share of the session).
 * - Camp fire + vigil book + the Camp Vigil (from night 2), trail-head lanterns, the Mist Wall boundary return.
 * Console: kg.Forest.* (Forest/KGForestCommands.cpp). Logs: KG_FOREST ... (TODO(020b): the Event Ledger).
 */
UCLASS()
class KILLGODOT_API UKGForestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UKGForestSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bActive; }

	static UKGForestSubsystem* Get(const UWorld* World);
	/** kg.Forest.Enabled (default 1): the whole forest threat layer on/off (chores and the camp fire stay). */
	static bool IsEnabled();

	struct FTrack
	{
		TWeakObjectPtr<AKGPlayerState> PS;
		TWeakObjectPtr<AKGForestPlayerInfo> Info;
		FKGWolfTrack Wolf;
		FKGMistTrack Mist;
		EKGForestBand Band = EKGForestBand::Village;
		int32 Sector = 0;
		float SafeDist = 0.0f;
		int32 Group = 1;
		bool bSafe = true;
		float Idle = 0.0f;
		FVector LastPos = FVector::ZeroVector;
		FRotator LastRot = FRotator::ZeroRotator;
		TWeakObjectPtr<AKGMistTongue> Tongue;
		float OutsideSecs = 0.0f;
		/** Dev: forced stage (kg.Forest.Test) keeps the gain up regardless of band / day. */
		float ForceWolfGain = 0.0f;
		bool bForceMist = false;
		float LastBiteTime = -100.0f;
		float FirstStampTime = -1.0f;
		float FirstBiteTime = -1.0f;
		bool bWallReturning = false;
	};

	// ---- queries (server) ----
	FTrack* FindTrack(const AKGPlayerState* PS);
	const TArray<FTrack>& GetTracks() const { return Tracks; }
	bool IsLitLightAt(const FVector2D& M) const;
	/** Where a player should walk to be safe (metres -> world cm); false when already safe / unknown. */
	bool SafeGoal(const FVector& From, FVector& OutGoal) const;
	int32 NumPlayers() const;
	bool IsLethalNow() const;
	void CountAlive(int32& OutAlive, int32& OutThreats) const;
	int32 ActiveWolves() const;
	int32 ActiveTongues() const;

	// ---- dev / tests ----
	/** Forces a wolf approach (Gain/s) or a Mist tongue on Who; teleports them to the deep forest first when bTeleport. */
	FString DevTest(const FString& What, AKGPlayerState* Who, bool bTeleport);
	FString DevStatus() const;
	/** A deep-forest spot of a sector (world cm), for teleports and shots. */
	FVector DeepSpot(int32 Sector, float Along = 0.0f) const;
	FVector CampLocation() const;
	void DevResetSession();
	/** kg.Forest.Smoke: the two-process smoke's host script (wolf approach + bite on the remote player, then a Mist
	 *  tongue it must walk away from). Logs KG_FOREST_SMOKE ... and KG_FOREST_SMOKE_DONE. */
	static bool bSmoke;

	// ---- camp vigil (server) ----
	bool SignVigil(AKGPlayerState* PS, FString& OutWhy);
	bool IsVigilSigner(const AKGPlayerState* PS) const { return VigilSigners.Contains(PS); }
	bool IsVigilNight() const { return bVigilNight; }
	/** Takes one log off the camp woodpile (the Gather deadwood chore stacks them); false when it is empty. */
	bool TakeCampLog();
	int32 CampLogs() const;
	const TArray<TWeakObjectPtr<AKGPlayerState>>& GetVigilSigners() const { return VigilSigners; }

	/** Bot brain hook (AKGBotController::Tick): flee wolves / the Mist, walk to the vigil, feed the fire. True = handled. */
	static bool UpdateBot(AKGBotController* Bot, AKGCharacter* Me, float DeltaSeconds);

	/** A PvE kill we caused (counted for the session's PvE death share). */
	void NotePvEDeath(AKGPlayerState* Victim, FName Cause);

	/** Session tallies across matches on this host (F1: PvE deaths <= 5 %). */
	static int32 SessionDeaths;
	static int32 SessionPvEDeaths;

	/** Server messages to one player's HUD line (owner-only). */
	void Tell(AKGPlayerState* PS, const FString& Text, float Seconds = 4.0f);

private:
	void ServerTick(float Dt);
	void TickSmoke(float Dt);
	int32 SmokeState = 0;
	float SmokeTimer = 0.0f;
	bool bSmokeSawTongue = false;
	TWeakObjectPtr<AKGPlayerState> SmokePS;
	FString SmokeMist;
	void UpdateTracks(float Dt);
	void UpdateWolves(float Dt);
	void UpdateMist(float Dt);
	void UpdateCampAndVigil(float Dt);
	void UpdateDeaths();
	void SpawnWorld();
	AKGWolf* WakeWolf(int32 Pack, int32 Slot, const FVector& Den);
	void Bite(AKGWolf* Wolf, AKGCharacter* Victim, FTrack& T);
	void MistCatch(FTrack& T, AKGCharacter* Body);
	void WallReturn(FTrack& T, AKGCharacter* Body, const TCHAR* Why);
	bool WolfCellOk(const FVector& WorldCm) const;
	FVector ClampWolfGoal(const FVector& From, const FVector& Goal) const;

	bool bActive = false;
	bool bServer = false;
	bool bSpawned = false;
	TArray<FTrack> Tracks;
	TArray<TWeakObjectPtr<AKGWolf>> Wolves;
	TMap<int32, float> SectorHowlUntil;
	/** Pack -> the player it hunts (weak). */
	TMap<int32, TWeakObjectPtr<AKGPlayerState>> PackTarget;
	TWeakObjectPtr<AKGCampfire> Campfire;
	TWeakObjectPtr<AKGVigilBook> Book;
	float Accum = 0.0f;
	uint8 LastPhase = 255;
	int32 LastDay = -1;
	TSet<TWeakObjectPtr<AKGPlayerState>> KnownDead;
	TSet<TWeakObjectPtr<AKGPlayerState>> PvEVictims;
	// vigil
	TArray<TWeakObjectPtr<AKGPlayerState>> VigilSigners;
	bool bVigilNight = false;
	float VigilNightSecs = 0.0f;
	float VigilElapsed = 0.0f;
	float VigilLitSecs = 0.0f;
	float VigilCountedSecs = 0.0f;
	TMap<TWeakObjectPtr<AKGPlayerState>, float> VigilOutside;
	TSet<TWeakObjectPtr<AKGPlayerState>> VigilArrived;
	int32 VigilMatchGranted = 0;
	int32 CampWoodpileSpot = INDEX_NONE;
	TArray<int32> ForestLampSpots;
	float StatAccumMs = 0.0f;
	int32 StatFrames = 0;
};
