#pragma once

#include "CoreMinimal.h"
#include "Chores/KGChoreTypes.h"
#include "Components/ActorComponent.h"
#include "Core/KGRng.h"
#include "UI/Menu/KGMenuActions.h"
#include "KGChoreComponent.generated.h"

class AKGCharacter;
class AKGPlayerState;
class AKGTaskStation;
class SKGChorePanel;

/** A chore's saved stage (multi-stage chores resume where they were left). */
USTRUCT()
struct FKGChoreProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName ChoreId;

	UPROPERTY(SaveGame)
	int32 Stage = 0;
};

/**
 * Chore minigames on a villager body (default subobject of AKGCharacter). Docs/01_GDD_Core.md section 8.
 *
 * Server authoritative:
 *  - AuthOpen: E on a station with a minigame (humans; bots keep hold-E) or kg.Chore.Play. Opens a session (chore, stage,
 *    random token), tells the owner to show the panel (ClientOpen).
 *  - ServerStageDone: the owner reports each solved stage; FKGChoreRules::CheckStage validates chore/token/stage order
 *    and the time the server measured for the stage against the chore's plausibility floor (no instant completion).
 *    The last stage completes the chore: the ledger ticks it off, the game mode counts it (never for the Impatient:
 *    they fake it - same panel, same body, nothing counted) and visual chores play their world effect (AKGChoreFx),
 *    again never for a fake.
 *  - Interrupts (every server tick): death, a meeting / trial phase, being pushed away from where it opened, taking
 *    damage -> ClientClose. The stage progress of multi-stage chores is kept (Saved) and resumes next time.
 * Timing uses the component's own accumulated DeltaTime (no TimerManager, rules in CLAUDE.md).
 *
 * Owning client: shows SKGChorePanel over the view (UI input: the body stays in place), relays solved stages,
 * closes on Esc / X / moving keys (ServerLeave).
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGChoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGChoreComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static UKGChoreComponent* FindFor(const AActor* Actor);

	// ---- Server ----
	/**
	 * Authority: opens ChoreId's minigame. Station = where it is done (null = dev "anywhere" play at the body).
	 * bDev: allowed without the chore on the list (then it is practice: nothing credited).
	 * Returns false when it cannot open (dead, phase, no minigame, not your chore...).
	 */
	bool AuthOpen(FName ChoreId, AKGTaskStation* Station, bool bDev = false);
	/**
	 * Authority (SPRINT-016 hook): opens PanelChore's minigame as one step of the world chore WorldChore (from
	 * StartStage). Completing it tells UKGWorldChoreComponent::AuthPanelStepDone instead of crediting a chore.
	 */
	bool AuthOpenWorldStep(FName InPanelChore, int32 StartStage, FName WorldChore, const FVector& At);
	/** Authority: a stage report (the RPC body; tests call it directly). */
	EKGChoreVerdict AuthStageDone(FName ChoreId, int32 Stage, int32 Token);
	/** Authority: end the session (progress kept). */
	void AuthClose(EKGChoreClose Reason, bool bTellOwner);

	bool HasSession() const { return !Session.IsNone(); }
	FName GetSessionChore() const { return Session; }
	int32 GetSessionStage() const { return SessionStage; }
	int32 GetSessionToken() const { return SessionToken; }
	bool IsSessionFake() const { return bSessionFake; }
	bool IsSessionPractice() const { return bSessionPractice; }
	/** Saved stage of a chore (0 = fresh). */
	int32 GetSavedStage(FName ChoreId) const;
	/** Tests: advance the server clock of the running stage. */
	void DebugAddServerTime(float Seconds) { StageSeconds += Seconds; }
	/** How many chores this body completed (tests / logs). */
	int32 GetCompletedCount() const { return CompletedCount; }

	// ---- Owning client ----
	/** Dev auto-win for the open panel and future ones on this machine (kg.Chore.AutoWin). */
	static void SetAutoWin(bool bOn);
	static bool IsAutoWin();
	bool IsPanelOpen() const;
	/** The locally shown minigame panel (null when closed). */
	TSharedPtr<SKGChorePanel> GetPanel() const { return Panel; }

	/** Every machine: true while this body works a minigame (identical for real and faked chores). */
	bool IsWorking() const { return bWorking; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerStageDone(FName ChoreId, int32 Stage, int32 Token);

	UFUNCTION(Server, Reliable)
	void ServerLeave(FName ChoreId, int32 Token);

	UFUNCTION(Client, Reliable)
	void ClientOpen(FName ChoreId, int32 Stage, int32 Token, bool bFake, bool bPractice);

	UFUNCTION(Client, Reliable)
	void ClientReject(FName ChoreId, int32 Stage, uint8 Verdict);

	UFUNCTION(Client, Reliable)
	void ClientClose(FName ChoreId, uint8 Reason);

	/** Public: someone is working a chore here (same for fakes). */
	UPROPERTY(Replicated)
	bool bWorking = false;

	/** Stage progress per chore (server; owner copy for the HUD). */
	UPROPERTY(Replicated, SaveGame)
	TArray<FKGChoreProgress> Saved;

private:
	AKGCharacter* GetCharacter() const;
	AKGPlayerState* GetPlayerState() const;
	bool IsImpatient() const;
	void TickServer(float DeltaTime);
	void SetSaved(FName ChoreId, int32 Stage);
	void CompleteSession();
	void ShowPanel(FName ChoreId, int32 Stage, int32 Token, bool bFake, bool bPractice);
	void HidePanel();
	void HandlePanelStage(int32 Stage);
	void HandlePanelClosed(EKGChoreClose Reason);

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor);

	// Server session
	FName Session;
	int32 SessionStage = 0;
	int32 SessionToken = 0;
	bool bSessionFake = false;
	bool bSessionPractice = false;
	/** SPRINT-016: the world chore this panel session is a step of (None = a normal chore). */
	FName SessionWorldChore;
	float StageSeconds = 0.0f;
	FVector Anchor = FVector::ZeroVector;
	TWeakObjectPtr<AKGTaskStation> SessionStation;
	FVector FxLocation = FVector::ZeroVector;
	bool bHitThisTick = false;
	int32 CompletedCount = 0;
	FKGRng Rng;

	// Owner UI
	TSharedPtr<SKGChorePanel> Panel;
	FKGViewportWidget PanelEntry;
	FName PanelChore;
	int32 PanelToken = 0;
	bool bFlushKeys = false;
	bool bPendingHide = false;
};
