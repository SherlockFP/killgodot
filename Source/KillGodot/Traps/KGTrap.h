#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Traps/KGTrapTypes.h"
#include "World/KGInteractable.h"
#include "KGTrap.generated.h"

class AKGCharacter;
class AKGDoor;
class UBoxComponent;
class UKGSnapshotComponent;
class UKGTrapSubsystem;
class ULightComponent;
class UStaticMeshComponent;

/** One person the portrait saw (replicated; At = server world seconds). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGTrapWitness
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trap") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") FString Puid;
	UPROPERTY(BlueprintReadOnly, Category = "Trap") float At = 0.0f;
};

/**
 * SPRINT-040 manor trap (Docs/Iterations/SPRINT-040 acceptance 5). Data-driven through TrapDef + Effect; the level
 * builder (Tools/Unreal) places them and sets Visual's mesh. Server authoritative: Tick advances the FKGTrapMachine,
 * an alive body inside Zone triggers an armed trap, the effect happens on Fire. Every effect is telegraphed first
 * (a creak, a sway, a flicker, a clunk) and never kills outright. State / bPing / Witnesses replicate for cosmetics,
 * the prompt and the minimap; ArmedByPuid stays a server secret (SaveGame, not replicated).
 * Subclasses (SPRINT-041 mimic chests) override WantsTrigger / OnTelegraph / OnFire / OnEnd; Effect Custom does nothing.
 */
UCLASS()
class KILLGODOT_API AKGTrap : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGTrap();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	// ---- Python-facing (Tools/Unreal builder sets these) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category = "Trap")
	FName TrapId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Trap")
	FName Room;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Trap")
	EKGTrapEffect Effect = EKGTrapEffect::Trapdoor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category = "Trap")
	FKGTrapDef TrapDef;

	/** Trigger / effect box: half extent (cm) and actor-local centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	FVector ZoneExtent = FVector(120.0, 120.0, 120.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	FVector ZoneOffset = FVector(0.0, 0.0, 100.0);

	/** Trapdoor: where the victims land (world). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap")
	FVector TeleportTarget = FVector::ZeroVector;

	// ---- Read API ----
	EKGTrapState GetState() const { return State; }
	bool IsPinging() const { return bPing; }
	const TArray<FKGTrapWitness>& GetWitnesses() const { return Witnesses; }
	const FKGTrapMachine& GetMachine() const { return Machine; }
	/** Policy + effect only (no cooldown): may Who arm this kind of trap? Works on clients for the local player. */
	bool MayArm(const AKGCharacter* Who) const;
	bool IsArmer(const AKGCharacter* Who) const;
	/** World-space test against the zone box. */
	bool IsInZone(const FVector& WorldLocation) const;
	FString Describe() const;

	// ---- Authority API (dev verbs, tests, subclasses) ----
	/** Arms it (bIgnorePolicy: host verbs). Records Armed and starts the armer's cooldown when By is given. */
	bool AuthArm(AKGCharacter* By, bool bIgnorePolicy);
	/** Telegraph with no wait: fires on the next tick. */
	void AuthForceFire();
	void AuthReset();

	static AKGTrap* FindById(const UWorld* World, FName Id);

protected:
	/** Armed + these alive bodies in the zone (the armer already removed): trigger? Default: any. */
	virtual bool WantsTrigger(const TArray<AKGCharacter*>& InZone) const;
	virtual void OnTelegraph();
	virtual void OnFire();
	virtual void OnEnd();
	virtual void OnReady();

	UFUNCTION()
	void OnRep_State();

	enum class ECue : uint8 { Creak, Crash, Clunk, Trapdoor, Flicker, Alarm };

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCue(uint8 Cue);

	void PlayCue(ECue Cue) const;
	void Record(FName Type, const AKGCharacter* By) const;
	void GatherInZone(TArray<AKGCharacter*>& Out, bool bSkipArmer) const;
	void SetState(EKGTrapState New);
	void ApplyDamageTo(AKGCharacter* C) const;
	UKGTrapSubsystem* Subsystem() const;
	void SampleWitnesses(float DeltaSeconds);

	// Cosmetics (every machine but a dedicated server; driven by the replicated State).
	void TickCosmetics(float DeltaSeconds);
	void OnCosmeticState(EKGTrapState Old, EKGTrapState New);
	void CollectLights();
	void RestoreLights();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Zone;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame, BlueprintReadOnly, Category = "Trap")
	EKGTrapState State = EKGTrapState::Idle;

	/** Alarm: loud for ActiveSecs; the minimap pulses. */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "Trap")
	bool bPing = false;

	/** Witness portraits: the last 5 distinct people seen, newest first. */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "Trap")
	TArray<FKGTrapWitness> Witnesses;

	/** Secret (server only): who armed it. */
	UPROPERTY(SaveGame)
	FString ArmedByPuid;

	UPROPERTY(SaveGame)
	FKGTrapMachine Machine;

	/** LockDoors: the doors this trap locked (by actor name for migration; the pointers are rebuilt on fire). */
	UPROPERTY(SaveGame)
	TArray<FString> LockedDoorNames;

	TArray<TWeakObjectPtr<AKGDoor>> LockedDoors;
	TWeakObjectPtr<AKGCharacter> LastTrigger;

private:
	void ApplyZone();

	// Cosmetic scratch.
	EKGTrapState CosmeticState = EKGTrapState::Idle;
	FVector VisualRestLoc = FVector::ZeroVector;
	FRotator VisualRestRot = FRotator::ZeroRotator;
	bool bRestCaptured = false;
	float CosmeticTime = 0.0f;
	float DropCm = -1.0f;
	TArray<TWeakObjectPtr<ULightComponent>> DimmedLights;
	float WitnessAccum = 0.0f;
};
