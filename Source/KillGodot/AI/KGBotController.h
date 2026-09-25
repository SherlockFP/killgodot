#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "KGBotController.generated.h"

class AKGCharacter;

/**
 * Dev/playtest villager bot: fills the lobby to the minimum so a match can run with one human, and doubles as
 * the soak-test driver (Backlog M3 "20 bot soak"). It drives the exact player code path (AKGCharacter actions).
 *
 * Brain (deliberately simple until the StateTree NPCs of M5):
 * - Roaming reads the level: anchors are the actors tagged KG_BotHub, every AKGTaskStation, a few PlayerStarts and the
 *   street/place regions of the level's AKGMapInfo, kept only when the navmesh connects them to the hub. Bots walk
 *   between them on the navmesh (stairs through their hidden ramps), mostly to nearby anchors, sometimes across town.
 *   Levels without a navmesh fall back to the old plaza wander.
 * - Day: chores (navmesh). Before a town meeting (last GatherLeadSeconds of a day that ends in one, or kg.BotGather 1)
 *   everyone walks to the hub (the Fountain Square) and waits in a loose ring.
 * - An Impatient bot draws its blade and stabs when someone turns their back nearby; at night it hunts.
 * Stuck detection: no progress for StuckSeconds while moving = abandon the target (counted, kg.Bot.Stats).
 */
UCLASS()
class KILLGODOT_API AKGBotController : public AAIController
{
	GENERATED_BODY()

public:
	AKGBotController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Dev panel / kg.Bot.List: what the brain is doing right now ("wandering", "working DrawWater", ...). */
	FString GetDevStatus() const;

	/** kg.BotAI 0 freezes every bot brain (bodies stay, phases still run). */
	static bool IsBrainEnabled();

	/** Where bots gather for meetings: the KG_BotHub actor, else the KG_Gallows marker, else the PlayerStarts' centre. */
	static FVector GetHubLocation(const UWorld* World);

	/** Soak-test counters since the last kg.Bot.Stats reset. */
	struct FStats
	{
		int32 StuckEvents = 0;
		int32 PathFailures = 0;
		int32 WanderArrivals = 0;
		int32 GatherArrivals = 0;
		int32 ChoresDone = 0;
		int32 Rescues = 0;
		float ChoreSecondsSum = 0.0f;
		float GatherSecondsSum = 0.0f;
		float GatherSecondsMax = 0.0f;
	};
	static FStats& Stats();

protected:
	void PickTarget();
	bool IsImpatient() const;

	FVector Target = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float StuckTime = 0.0f;
	float PauseRemaining = 0.0f;
	float DecisionCooldown = 0.0f;
	int32 BotSeed = 0;
	/** Night hunt: stays locked on one villager until they die or dawn breaks. */
	TWeakObjectPtr<AKGCharacter> Prey;
	/** Seconds spent hunting the current Prey without a stab. Past PreyGiveUpSeconds the hunt re-picks (skipping the
	 *  stale one for a while): a villager stuck off the navmesh, or a spawn pawn nobody can path to, used to lock the
	 *  Impatient forever and stall the whole match (kg_match_smoke: "no winner within 600 s", 2026-09-25/26). */
	float PreyHuntSeconds = 0.0f;
	static constexpr float PreyGiveUpSeconds = 25.0f;
	TWeakObjectPtr<AKGCharacter> StalePrey;
	float SidestepRemaining = 0.0f;
	FVector SidestepDir = FVector::ZeroVector;
	float VoteDelay = -1.0f;
	void UpdateVotes(float DeltaSeconds, const class AKGGameState* GS);
	/** Day: walk (NavMesh) to the next open chore and work it. Returns true while busy with chores. */
	bool UpdateChores(float DeltaSeconds, const class AKGGameState* GS);
	TWeakObjectPtr<class AKGTaskStation> ChoreTarget;
	/** Chores this bot could not reach lately (skipped until the list runs dry). */
	TArray<TWeakObjectPtr<class AKGTaskStation>> FailedChores;
	// The Impatient put on one world-chore show per match (SPRINT-016); after that they hunt and fake panel chores only.
	bool bFakedWorldChore = false;
	float ChoreRepath = 0.0f;
	float ChoreElapsed = 0.0f;
	uint32 Step = 0;

	/** Navmesh roaming between level anchors. Returns false when the level has no navmesh (legacy wander). */
	bool UpdateNavWander(float DeltaSeconds);
	/** Meeting run-up / meeting: walk to the hub. Returns true while gathering (the rest of the brain waits). */
	bool UpdateGather(float DeltaSeconds, const class AKGGameState* GS);
	/** Progress watchdog for navmesh moves; true = stuck (the caller drops its goal). */
	bool CheckStuck(float DeltaSeconds);

	/** Opens a closed door in front of (or right next to) the body; true when one was opened. MinDot: how straight
	 *  ahead it must be (cosine to the walking direction). */
	bool OpenDoorAhead(double Radius, float MinDot);
	/** Stuck recovery: open a door if there is one, else back off sideways with a hop for a moment. */
	void BeginUnstick();
	void NotePathFailure();

	bool bNavMove = false;
	float UnstickRemaining = 0.0f;
	FVector UnstickDir = FVector::ZeroVector;
	float DoorCheck = 0.0f;
	int32 PathFailStreak = 0;
	int32 StuckStreak = 0;
	FVector LastStuckAt = FVector::ZeroVector;
	float PathFailLogCooldown = 0.0f;
	float ProgressTimer = 0.0f;
	FVector ProgressFrom = FVector::ZeroVector;
	float NavRepath = 0.0f;
	bool bGathering = false;
	bool bGatherArrived = false;
	float GatherElapsed = 0.0f;
	FVector GatherSpot = FVector::ZeroVector;
};
