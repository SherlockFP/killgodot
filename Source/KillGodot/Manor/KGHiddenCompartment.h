#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGHiddenCompartment.generated.h"

class AKGCharacter;
class UBoxComponent;
class UKGSnapshotComponent;
class UStaticMeshComponent;

/**
 * SPRINT-040 hidden compartment (loose brick, false drawer, hollow book, floor safe ...). E while closed opens it for
 * everyone (the Lid slides / swings by LidOpenOffset / LidOpenRotation on every client), the loot table rolls once
 * (FKGLoot, match-seeded) and the manor subsystem logs the event. Once open a ClueText shows to anyone who looks:
 * "Clue: <text>" (computed on the viewer's machine from the replicated state).
 */
UCLASS()
class KILLGODOT_API AKGHiddenCompartment : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGHiddenCompartment();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	// ---- Python-facing ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Compartment")
	FName CompartmentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Compartment")
	FName Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Compartment")
	FName Room;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FText SearchPrompt;

	/** Evidence shown once open (may be empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Compartment")
	FString ClueText;

	/** FKGLoot table rolled once on opening (None = nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FName LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FVector LootOffset = FVector(60.0, 0.0, 40.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FVector HitboxExtent = FVector(40.0, 40.0, 40.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FVector LidOpenOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FRotator LidOpenRotation = FRotator::ZeroRotator;

	UPROPERTY(ReplicatedUsing = OnRep_Open, SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	bool bOpen = false;

	bool IsOpen() const { return bOpen; }

	/** Authority: open (loot once, event). By may be null (a chore reward). */
	bool AuthOpen(AKGCharacter* By);

	static AKGHiddenCompartment* FindById(const UWorld* World, FName Id);

protected:
	UFUNCTION()
	void OnRep_Open();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOpened();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY(SaveGame)
	bool bLooted = false;

private:
	void ApplyHitbox();

	FVector LidRestLoc = FVector::ZeroVector;
	FRotator LidRestRot = FRotator::ZeroRotator;
	bool bRestCaptured = false;
	float LidAlpha = 0.0f;
};
