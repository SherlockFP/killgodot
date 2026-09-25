#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/KGRng.h"
#include "Core/KGTypes.h"
#include "KGGameMode.generated.h"

class AKGCharacter;
class AKGPlayerState;
class AKGGameState;

/** Host-only match director: phase machine, role assignment, (later) night resolution and snapshots. */
UCLASS()
class KILLGODOT_API AKGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKGGameMode();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Authority: Who finished TaskId (validated by the character). Town chores fill the Preparation bar. */
	void OnTaskCompleted(AKGPlayerState* Who, FName TaskId);

	/** Warmup -> boat cinematic -> role cards -> day 1. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Match")
	void StartMatchFlow(int64 Seed);

	/** Called by AKGLobbyState when the host's start countdown ends: bots fill up to BotFillTarget, then the flow. */
	void StartFromLobby(int32 BotFillTarget);

	/** Meeting: Voter accuses Target (nullptr clears). A majority of the living sends Target to trial. */
	void HandleAccuse(AKGPlayerState* Voter, AKGPlayerState* Target);

	/** Trial: guilty / innocent from a living, non-accused player. */
	void HandleVerdict(AKGPlayerState* Voter, bool bGuilty);

	/** Authority: a character died (player or bot). Marks the ghost, reveals the role, checks the win. */
	void OnCharacterDied(AKGCharacter* Victim, AActor* Killer);

	/** Phase lengths scale with lobby size (Docs/01_GDD_Core.md §1). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Match")
	static float GetPhaseDuration(EKGPhase Phase, int32 NumPlayers);

	// --- Dev hooks (dev panel, kg.* commands, automation). Authority only; defined in Core/KGGameModeDev.cpp. -------
	/** Adds Count villager bots right now (any phase). Returns how many joined. */
	int32 DevAddBots(int32 Count);
	/** Tops the match up to TargetPlayers with bots. Returns how many joined. */
	int32 DevFillBots(int32 TargetPlayers);
	/** Removes up to Count bots (newest first), or exactly Specific when given. Returns how many left. */
	int32 DevRemoveBots(int32 Count, AController* Specific = nullptr);
	/** Starts the match flow now with Seed (a pre-game lobby gets its start countdown instead). */
	void DevStartMatch(int64 Seed);
	/** Jumps straight into Phase: deals roles first when the jump skips the reveal, picks an accused for a Trial. */
	void DevJumpToPhase(EKGPhase Phase);
	/** Declares Winner and goes to the epilogue. */
	void DevForceWin(EKGAlignment InWinner);
	void DevRevealAllRoles() { RevealAllRoles(); }
	/** Re-opens chores: one player's (Only), or a fresh deal for everyone (nullptr). Recounts the Preparation bar. */
	void DevResetTasks(AKGPlayerState* Only);
	/** Seed of the running match flow (0 = not started). */
	int64 GetMatchSeed() const { return MatchSeed; }
	/** Phase clock speed ("fast forward"): 1 = real time. */
	float DevClockScale = 1.0f;

protected:
	void EnterPhase(EKGPhase NewPhase);
	EKGPhase GetNextPhase(EKGPhase Current, int32 DayIndex) const;
	void AssignRoles();
	/** Dev/playtest: top the lobby up with villager bots so one human can play a full match. */
	void FillWithBots(int32 TargetPlayers);
	void CheckWinCondition();
	void RevealAllRoles();
	void StartMeeting();
	void StartTrial(AKGPlayerState* Accused);
	void ResolveTrial();
	void AnnounceDawn();
	void AssignTasks(const TArray<AKGPlayerState*>& Players);
	void LighthouseIllumination();
	int32 CountAlive() const;
	/** Gallows platform top (actor tagged KG_Gallows in the level), or the plaza if missing. */
	FVector GetGallowsLocation() const;
	static FString RoleLabel(const AKGPlayerState* PS);

	AKGGameState* GetKGGameState() const;

	UPROPERTY(SaveGame)
	FKGRng Rng;

private:
	/** Dev playtest perf sampling (-KGAutoShot): frame times after warm-up. */
	TArray<float> DevFrameTimesMs;
	float DevElapsed = 0.0f;
	bool bDevPerf = false;
	/** Seconds until the match auto-starts (bots fill first); negative = not armed. */
	float AutoStartRemaining = -1.0f;
	int32 BotsSpawned = 0;
	/** Names (and roles) of whoever died since dusk, read out at dawn. */
	TArray<FString> NightDeaths;
	/** Town chores dealt this match (the Preparation bar's denominator). */
	int32 TownTasksTotal = 0;
	int32 TownTasksDone = 0;
	bool bIlluminated = false;
	int32 HumansJoined = 0;
	int64 MatchSeed = 0;
};
