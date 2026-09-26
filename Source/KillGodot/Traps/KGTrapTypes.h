#pragma once

#include "CoreMinimal.h"
#include "Core/KGMatchClock.h"
#include "KGTrapTypes.generated.h"

/**
 * SPRINT-040 generic trap framework (role-agnostic; SPRINT-041's Trapper reuses it for mimic chests).
 * A trap is a small state machine: Idle -> (arm) Armed -> (a body in the zone) Telegraph -> Active -> Cooldown ->
 * Idle (passive traps re-arm: -> Armed). Every phase is timed by FKGMatchClock (remaining seconds, migration safe).
 */
UENUM(BlueprintType)
enum class EKGTrapState : uint8
{
	Idle,
	Armed,
	Telegraph,
	Active,
	Cooldown
};

UENUM(BlueprintType)
enum class EKGTrapEffect : uint8
{
	Trapdoor,
	FallingObject,
	LockDoors,
	LightsOut,
	Alarm,
	Witness,
	Custom
};

/** Data of one trap (the level builder fills it; a subclass may override the defaults). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGTrapDef
{
	GENERATED_BODY()

	/** Flavour id (Trapdoor_Cellar, Chandelier, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	FName Kind;

	/** Who may arm it: "None" (nobody), "Anyone", "Impatient" (the killer alignment), or a registered policy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	FName ArmPolicy = TEXT("Impatient");

	/** Always armed, re-arms itself after the cooldown (alarms, witness portraits). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	bool bPassive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float TelegraphSecs = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float ActiveSecs = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float CooldownSecs = 45.0f;

	/** Per-player sabotage cooldown shared by every trap that player arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float ArmerCooldownSecs = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	float EffectRadiusCm = 600.0f;

	/** The player who armed it neither triggers it nor takes its damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Trap")
	bool bIgnoreArmer = true;
};

/** One state change reported by FKGTrapMachine::Advance. */
struct KILLGODOT_API FKGTrapTransition
{
	EKGTrapState From = EKGTrapState::Idle;
	EKGTrapState To = EKGTrapState::Idle;
};

/** Pure trap logic (no UObject; unit-tested in KillGodot.Traps.*). Durations only from FKGMatchClock. */
USTRUCT()
struct KILLGODOT_API FKGTrapMachine
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EKGTrapState State = EKGTrapState::Idle;

	UPROPERTY(SaveGame)
	FKGMatchClock Clock;

	UPROPERTY(SaveGame)
	bool bPassive = false;

	UPROPERTY(SaveGame)
	float TelegraphSecs = 2.0f;

	UPROPERTY(SaveGame)
	float ActiveSecs = 1.0f;

	UPROPERTY(SaveGame)
	float CooldownSecs = 45.0f;

	/** Takes the durations; passive machines start Armed. */
	void Configure(const FKGTrapDef& Def);

	/** Idle -> Armed. */
	bool TryArm();

	/** Armed -> Telegraph (clock = TelegraphSecs). */
	bool TryTrigger();

	/** Skips the wait: Telegraph with a zero clock, so the next Advance fires it (dev / tests). */
	void ForceFire();

	/** Idle (Armed when passive), clock cleared. */
	void Reset();

	/**
	 * Runs the timed phases: Telegraph -> Active -> Cooldown -> Idle | Armed (passive). Zero-length phases chain within
	 * one call. Returns the transitions in order.
	 */
	TArray<FKGTrapTransition> Advance(float DeltaSeconds);

	bool IsArmed() const { return State == EKGTrapState::Armed; }
	bool IsBusy() const { return State == EKGTrapState::Telegraph || State == EKGTrapState::Active || State == EKGTrapState::Cooldown; }
	float Remaining() const { return Clock.RemainingSeconds; }

private:
	void Enter(EKGTrapState Next, TArray<FKGTrapTransition>& Out);
};

/** One line of the trap event log (server). Type: Armed / Telegraph / Fired / Ended / Ready / Witnessed / Denied. */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGTrapEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trap") FName TrapId;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FName Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FName Room;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FName Type;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FString ByPuid;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FString ByName;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") float WorldSeconds = 0.0f;
};

/** Fixed-size ring of the last Capacity events (oldest first when read). Pure; unit-tested. */
struct KILLGODOT_API FKGTrapEventLog
{
	static constexpr int32 Capacity = 64;

	void Add(const FKGTrapEvent& E);
	int32 Num() const { return Events.Num(); }
	/** Index 0 = the oldest kept event. */
	const FKGTrapEvent& Get(int32 Index) const;
	TArray<FKGTrapEvent> ToArray() const;
	void Reset() { Events.Reset(); Head = 0; }

private:
	TArray<FKGTrapEvent> Events;
	int32 Head = 0;
};

/** Per-armer sabotage cooldowns keyed by PUID (remaining seconds). Pure; unit-tested. */
struct KILLGODOT_API FKGTrapArmerCooldowns
{
	void Start(const FString& Puid, float Secs);
	void Advance(float DeltaSeconds);
	float Remaining(const FString& Puid) const;
	bool IsReady(const FString& Puid) const { return Remaining(Puid) <= 0.0f; }
	void Reset() { Clocks.Reset(); }

private:
	TMap<FString, FKGMatchClock> Clocks;
};

namespace KGTrap
{
	KILLGODOT_API const TCHAR* StateName(EKGTrapState S);
	KILLGODOT_API const TCHAR* EffectName(EKGTrapEffect E);
	KILLGODOT_API bool ParseEffect(const FString& S, EKGTrapEffect& Out);
}
