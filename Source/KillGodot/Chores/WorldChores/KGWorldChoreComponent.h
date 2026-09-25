#pragma once

#include "CoreMinimal.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Components/ActorComponent.h"
#include "KGWorldChoreComponent.generated.h"

class AAIController;
class AKGCharacter;
class AKGChoreItem;
class AKGPlayerState;

/** One world chore in progress (server; owner copy for the HUD). */
USTRUCT()
struct FKGWorldProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName Chore;

	UPROPERTY(SaveGame)
	uint8 Step = 0;

	UPROPERTY(SaveGame)
	uint8 Variant = 0;

	/** "All of" steps: which targets are done (bit per target, in data order). */
	UPROPERTY(SaveGame)
	uint16 DoneMask = 0;

	/** Work steps with Repeat: repeats done. */
	UPROPERTY(SaveGame)
	uint8 Reps = 0;

	/** Activity order: the highest is the chore the HUD highlights. */
	UPROPERTY(SaveGame)
	uint16 Touch = 0;

	UPROPERTY()
	TObjectPtr<AKGChoreItem> Item;

	/** Server time the chore started (logs). */
	float StartedAt = -1.0f;
};

/** What the body is doing at a spot right now (owner HUD ring). */
UENUM()
enum class EKGDwell : uint8
{
	None,
	Work,
	Bring,
	Sabotage,
	Dump
};

USTRUCT()
struct FKGWorldDwell
{
	GENERATED_BODY()

	UPROPERTY()
	FName Chore;

	UPROPERTY()
	int16 Anchor = -1;

	UPROPERTY()
	EKGDwell Kind = EKGDwell::None;

	UPROPERTY()
	float Seconds = 0.0f;

	UPROPERTY()
	float Needed = 1.0f;

	float Alpha() const { return Needed > 0.0f ? FMath::Clamp(Seconds / Needed, 0.0f, 1.0f) : 0.0f; }
};

/** A HUD / marker waypoint (owner). */
struct FKGWorldWaypoint
{
	FVector Location = FVector::ZeroVector;
	FName Chore;
	bool bActive = false;
	/** Points at your own item lying somewhere (pick it up again). */
	bool bItem = false;
};

/**
 * SPRINT-016 world chores on a villager body (runtime subobject added by UKGWorldChoreSubsystem on the server).
 * Server authoritative: every step is validated here (step order, standing at the spot, the item being yours and
 * inside the spot, the fill level, dwell time measured on the server). The Impatient runs the exact same path; at
 * the end nothing is counted (AKGGameMode::OnTaskCompleted ignores them) and the world looks the same to everybody.
 */
UCLASS(ClassGroup = (KillGodot))
class KILLGODOT_API UKGWorldChoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGWorldChoreComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UKGWorldChoreComponent* FindFor(const AActor* Actor);

	// ---- server ----
	/** E on a spot (players via AKGChoreSpot, bots directly). True when it started something. */
	bool AuthInteract(int32 AnchorIndex);
	/** The panel minigame of a Panel step was solved (UKGChoreComponent world-step session). */
	void AuthPanelStepDone(FName Chore);
	/** Dev: puts Chore on this player's list (reopened if done) with a variant (-1 = the player's own). */
	bool AuthGive(FName Chore, int32 Variant = -1);
	/** Tests: advance the server clock. */
	void DebugTick(float Seconds, float Step = 0.05f);

	// ---- queries (server, and the owner through replication) ----
	const TArray<FKGWorldProgress>& GetProgress() const { return Progress; }
	const FKGWorldProgress* FindProgress(FName Chore) const;
	const FKGWorldDwell& GetDwell() const { return Dwell; }
	/** Current targets of every open world chore (+ your dropped items), the active one first. */
	void GetWaypoints(TArray<FKGWorldWaypoint>& Out) const;
	/** The highlighted chore's step text ("Carry the water to the fountain trough - walk, don't run!  (2/3)"). */
	FString GetActiveLabel(FName* OutChore = nullptr) const;
	/** Local player: the E prompt for a spot (empty = nothing to do there). */
	FText PromptFor(int32 AnchorIndex) const;
	/** Owner: last notice ("The trough is poisoned!") and when it arrived (world seconds). */
	const FString& GetNotice() const { return Notice; }
	float GetNoticeAt() const { return NoticeAt; }
	float GetStepDoneAt() const { return StepDoneAt; }
	const FString& GetStepDoneText() const { return StepDoneText; }

	// ---- bots (AI/KGBotController hook) ----
	enum class EBot : uint8 { Idle, Busy, Failed };
	/** Walks and works Chore for a bot. Busy while it has something to do; Failed = give up on this chore. */
	EBot BotDrive(AAIController* AI, FName Chore, float DeltaSeconds);
	/** Bots: put down whatever chore item is hugged (night, meeting, hunting, giving a chore up). */
	void BotDropCarried(const TCHAR* Why);

	// ---- dev (kg.WorldChore.*; clients need kg.Dev.AllowClients 1 on the host) ----
	UFUNCTION(Server, Reliable)
	void ServerDev(const FString& Line);
	/** Authority: runs a dev line for this body ("Give WaterRun 0", "Path well", "Goto next", "Skip"). */
	FString AuthDev(const FString& Line);
	/** Client autopilot (the smoke walks like a player): follows Points with movement input. */
	bool IsAutopilotActive() const { return AutoPath.Num() > 0; }
	void StopAutopilot() { AutoPath.Reset(); }

protected:
	UFUNCTION(Client, Reliable)
	void ClientStepDone(FName Chore, uint8 Step, uint8 NumSteps, bool bChoreDone, const FString& Text);

	UFUNCTION(Client, Unreliable)
	void ClientNotice(const FString& Text);

	UFUNCTION(Client, Reliable)
	void ClientDevPath(const TArray<FVector_NetQuantize>& Points);

	UPROPERTY(Replicated, SaveGame)
	TArray<FKGWorldProgress> Progress;

	UPROPERTY(Replicated)
	FKGWorldDwell Dwell;

private:
	AKGCharacter* GetCharacter() const;
	AKGPlayerState* GetPlayerState() const;
	bool IsImpatient() const;
	bool PhaseAllows() const;
	float ServerNow() const;

	void TickServer(float DeltaTime);
	void SyncList();
	void ValidateItems(FKGWorldProgress& P);
	void TickBring(float DeltaTime);
	void TickWorkDwell(float DeltaTime);
	bool InReach(int32 AnchorIndex, float SlackCm) const;
	FKGWorldProgress* MutableProgress(FName Chore);
	FKGWorldProgress& EnsureProgress(FName Chore);
	void StartDwell(EKGDwell Kind, FName Chore, int32 Anchor, float Needed);
	void CompleteStep(FKGWorldProgress& P, int32 AnchorIndex);
	void CompleteChore(FName Chore);
	void Revert(FKGWorldProgress& P, int32 ToStep, const FString& Why);
	void Say(const FString& Text);
	int32 TargetSlot(const FKGWorldChoreDef& Def, const FKGWorldProgress& P, int32 AnchorIndex) const;
	void TickAutopilot(float DeltaTime);
	/** Bots: where to walk for a spot (its stand point, or the navmesh next to it on our side). */
	FVector BotWalkGoal(int32 AnchorIndex) const;

	uint16 NextTouch = 1;
	float NoticeCooldown = 0.0f;

	// Owner HUD
	FString Notice;
	float NoticeAt = -100.0f;
	FString StepDoneText;
	float StepDoneAt = -100.0f;

	// Bot
	FName BotChore;
	float BotRepath = 0.0f;
	float BotProgressClock = 0.0f;
	FVector BotProgressFrom = FVector::ZeroVector;
	int32 BotFails = 0;
	int32 BotSnags = 0;
	int32 BotGoalAnchor = INDEX_NONE;
	FVector BotGoalCached = FVector::ZeroVector;
	float BotElapsed = 0.0f;
	FVector BotGoal = FVector::ZeroVector;

	// Client autopilot (dev): follows the server's navmesh path; steps sideways around things the navmesh can't see
	// (a wall lantern at head height is above the nav agent but inside the 180 cm capsule).
	TArray<FVector> AutoPath;
	FVector AutoFace = FVector::ZeroVector;
	float AutoBestDist = TNumericLimits<float>::Max();
	float AutoStuckClock = 0.0f;
	float AutoSideClock = 0.0f;
	float AutoSideSign = 1.0f;
	int32 AutoUnstucks = 0;
};
