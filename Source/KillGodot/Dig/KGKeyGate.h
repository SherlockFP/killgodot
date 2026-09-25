#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGKeyGate.generated.h"

class UKGSnapshotComponent;
class UStaticMeshComponent;

/**
 * The iron gate of the catacombs' treasure vault. Locked until someone uses a Crypt Key on it (the key stays in the
 * lock); after that E swings it open or shut for everyone (a killer can close it behind you). Replicated, snapshotted.
 * The leaf mesh is SM_KG_CryptGate (hinge at local X=0, 1.9 m wide along +X).
 */
UCLASS()
class KILLGODOT_API AKGKeyGate : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGKeyGate();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	bool IsUnlocked() const { return bUnlocked; }
	bool IsOpen() const { return bOpen; }

	/** Authority (dev / tests): unlock without a key. */
	void DevUnlock(bool bAlsoOpen);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	float OpenAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FName KeyItem = TEXT("CryptKey");

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(bool bLockedRattle);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Leaf;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY(Replicated, SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Gate")
	bool bUnlocked = false;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "Gate")
	bool bOpen = false;

private:
	float Angle = 0.0f;
};
