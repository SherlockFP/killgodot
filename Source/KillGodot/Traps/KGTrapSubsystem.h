#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Traps/KGTrapTypes.h"
#include "KGTrapSubsystem.generated.h"

class AKGCharacter;
class AKGTrap;

/** Arm policies by name (pure table; unit-tested). Defaults: None (nobody), Anyone, Impatient (killer alignment). */
struct KILLGODOT_API FKGTrapArmPolicies
{
	using FPolicy = TFunction<bool(const AKGCharacter*)>;

	FKGTrapArmPolicies();
	void Register(FName Name, FPolicy Policy) { Policies.Add(Name, MoveTemp(Policy)); }
	bool Has(FName Name) const { return Policies.Contains(Name); }
	/** Unknown or None policies deny. */
	bool CanArm(FName Policy, const AKGCharacter* Who) const;

	/** The Impatient alignment check (exactly UKGWorldChoreComponent::IsImpatient). */
	static bool IsImpatient(const AKGCharacter* Who);

private:
	TMap<FName, FPolicy> Policies;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FKGOnTrapEvent, const FKGTrapEvent&);

/**
 * SPRINT-040 trap glue: the registry of AKGTrap, the arm policies, the per-armer sabotage cooldowns (remaining
 * seconds, advanced in Tick) and the server event log (the last 64 FKGTrapEvent + OnTrapEvent for other systems +
 * "KG_TRAP <Type> <TrapId> kind=<Kind> room=<Room> by=<Name>" log lines). The game has no other event log yet.
 */
UCLASS()
class KILLGODOT_API UKGTrapSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UKGTrapSubsystem* Get(const UWorld* World);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterTrap(AKGTrap* Trap);
	void UnregisterTrap(AKGTrap* Trap);
	const TArray<TWeakObjectPtr<AKGTrap>>& GetTraps() const { return Traps; }
	AKGTrap* FindTrap(FName TrapId) const;

	void RegisterArmPolicy(FName Name, TFunction<bool(const AKGCharacter*)> Policy) { Policies.Register(Name, MoveTemp(Policy)); }
	bool CanArm(FName Policy, const AKGCharacter* Who) const { return Policies.CanArm(Policy, Who); }

	/** Server. Remaining sabotage cooldown of a player (0 = may arm). */
	float GetArmerCooldown(const FString& Puid) const { return Cooldowns.Remaining(Puid); }
	void StartArmerCooldown(const FString& Puid, float Secs) { Cooldowns.Start(Puid, Secs); }
	void ClearArmerCooldowns() { Cooldowns.Reset(); }

	/** Server: logs, keeps and broadcasts the event. */
	void Record(const FKGTrapEvent& Event);
	const FKGTrapEventLog& GetLog() const { return Log; }

	FKGOnTrapEvent OnTrapEvent;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<AKGTrap>> Traps;
	FKGTrapArmPolicies Policies;
	FKGTrapArmerCooldowns Cooldowns;
	FKGTrapEventLog Log;
};
