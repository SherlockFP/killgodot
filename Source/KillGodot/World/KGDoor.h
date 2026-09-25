#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGDoor.generated.h"

class UKGSnapshotComponent;

/**
 * House door (Docs/01_GDD_Core.md §7): open/close with E, can be locked, breaks after enough damage
 * (~8 s of hitting). A broken leaf becomes a physics body. State replicates and snapshots.
 */
UCLASS()
class KILLGODOT_API AKGDoor : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Door")
	void SetLocked(bool bInLocked) { bLocked = bInLocked; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Door")
	bool IsOpen() const { return bOpen; }

	/** Moves Current toward Target by at most Speed*Dt (pure; unit-tested). */
	static float StepAngle(float Current, float Target, float SpeedDegPerSec, float Dt)
	{
		const float MaxStep = SpeedDegPerSec * Dt;
		return Current + FMath::Clamp(Target - Current, -MaxStep, MaxStep);
	}

protected:
	void Break();

	UFUNCTION()
	void OnRep_Broken();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Leaf;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY(Replicated, SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bOpen = false;

	UPROPERTY(Replicated, SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bLocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_Broken, SaveGame, BlueprintReadOnly, Category = "Door")
	bool bBroken = false;

	UPROPERTY(SaveGame, EditAnywhere, Category = "Door")
	float DoorHealth = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Door")
	float OpenAngle = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Door")
	float OpenSpeedDegPerSec = 260.0f;

private:
	float CurrentAngle = 0.0f;
};
