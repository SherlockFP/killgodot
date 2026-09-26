#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGManorSubsystem.generated.h"

class AKGCharacter;
class AKGDoor;
class UKGWorldChoreComponent;

/**
 * SPRINT-040 Storm Manor glue (server logic):
 *  - wing gates: while the phase is Lobby / Warmup, every AKGDoor tagged KG_WingGate + KG_MinN_<n> is shut and locked
 *    when fewer than n players are in the game state, unlocked otherwise; frozen once the match starts,
 *  - secret chores: a discovered secret gives its finder every catalog chore whose SecretId matches,
 *  - rewards: a counted chore with RewardSecret discovers that secret for everyone (a shortcut); RewardCompartment
 *    opens that compartment (a clue),
 *  - the deal filter: chores with MinPlayers above the player count are not dealt.
 */
UCLASS()
class KILLGODOT_API UKGManorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UKGManorSubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Called by AKGSecretPassage::DiscoverAll (authority). By may be null (a chore reward). */
	void OnSecretDiscovered(FName SecretId, AKGCharacter* By);
	/** Called by AKGHiddenCompartment::AuthOpen (authority). */
	void OnCompartmentOpened(FName CompartmentId, AKGCharacter* By);

	/** Authority helpers (rewards, dev verbs). */
	int32 DiscoverSecret(FName SecretId, AKGCharacter* By);
	bool OpenCompartment(FName CompartmentId, AKGCharacter* By);
	/** Re-evaluates the wing gates now (bForce: even after the match started). Returns the number of locked gates. */
	int32 RefreshGates(bool bForce);

	/** Pure: "KG_MinN_<n>" tag -> n (0 when the actor has none). Unit-tested. */
	static int32 MinPlayersOf(const TArray<FName>& Tags);
	/** Pure: chores with MinPlayers above Players are not dealt. */
	static bool PassesDeal(int32 ChoreMinPlayers, int32 Players) { return ChoreMinPlayers <= 0 || Players >= ChoreMinPlayers; }

	int32 PlayerCount() const;
	bool IsAuthority() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleChoreDone(UKGWorldChoreComponent* Comp, FName Chore);
	void SetGate(AKGDoor* Door, bool bLocked) const;

	FDelegateHandle ChoreDoneHandle;
	float GateAccum = 0.0f;
	bool bGatesFrozen = false;
	int32 LastGatePlayers = -1;
};
