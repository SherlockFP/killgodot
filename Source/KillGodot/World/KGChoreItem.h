#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGChoreItem.generated.h"

class APawn;
class APlayerState;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
struct FKGWorldItemDef;

/**
 * SPRINT-016: a real, replicated, physics-simulated chore object (bucket, fish crate, bread basket, taper, net,
 * firewood, letters, grain / flour sack, oil can). Players carry it with the normal hold-E physics carry
 * (AKGCharacter::TryGrab); anyone can pick it up, snatch it or knock it out of your hands with a shove. Bots carry it
 * attached. It belongs to one player's chore (OwnerPlayer + Chore): only the owner's chore advances when it arrives,
 * though anyone may help carry it (the heavy crate walks at full speed with two carriers).
 * Liquids (the bucket) have a Fill that spills when the carrier runs, falls or the bucket tips over.
 * The public never learns whose chore it is for a fake (the Impatient's items look and behave identically).
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGChoreItem : public AActor
{
	GENERATED_BODY()

public:
	AKGChoreItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Authority: spawns an item of Kind for Owner's Chore at Where (deferred so Kind replicates with the spawn). */
	static AKGChoreItem* AuthSpawn(UWorld* World, FName Kind, const FVector& Where, float Yaw, APlayerState* Owner, FName Chore);

	/** Item the pawn carries (hold-E or bot attach), or null. Any machine (Carriers replicate). */
	static AKGChoreItem* CarriedBy(const APawn* Pawn);
	/** Speed factor for a pawn's movement (1 = not carrying anything that slows). Any machine. */
	static float SpeedFactorFor(const APawn* Pawn);

	// ---- carry bookkeeping (authority; hooks in AKGCharacter::TryGrab / Release / ServerShove) ----
	/** A pawn grabbed it. Returns the pawn whose hands it was snatched from (light items have one carrier). */
	APawn* AuthAddCarrier(APawn* Pawn);
	void AuthRemoveCarrier(APawn* Pawn);
	/** Bots: carried attached to the body (no physics handle). */
	void AuthAttachTo(APawn* Bot);
	/** Drops from a bot (or anyone), with an optional push. */
	void AuthDetach(const FVector& Impulse);
	bool IsAttached() const { return bAttachedCarry; }

	const TArray<TObjectPtr<APawn>>& GetCarriers() const { return Carriers; }
	bool IsCarried() const { return Carriers.Num() > 0; }
	bool IsCarriedBy(const APawn* Pawn) const;
	FName GetKind() const { return Kind; }
	FName GetChore() const { return Chore; }
	APlayerState* GetOwnerPlayer() const { return OwnerPlayer; }
	float GetFill() const { return Fill; }
	int32 GetPieces() const { return Pieces; }
	const FKGWorldItemDef* GetDef() const;

	/** Authority. */
	void AuthSetFill(float NewFill);
	void AuthSetPieces(int32 NewPieces);
	/** Authority: nobody's chore needs it any more; it vanishes after lying idle for a while. */
	void AuthOrphan() { bOrphan = true; }
	/** Authority: poof (consumed at a delivery, replaced by a fresh one). */
	void AuthConsume();

	UBoxComponent* GetBox() const { return Box; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Kind)
	FName Kind;

	UPROPERTY(Replicated)
	FName Chore;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerPlayer;

	UPROPERTY(ReplicatedUsing = OnRep_Fill, SaveGame)
	float Fill = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Fill, SaveGame)
	int32 Pieces = 1;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<APawn>> Carriers;

	UPROPERTY(ReplicatedUsing = OnRep_Attached)
	bool bAttachedCarry = false;

	UFUNCTION()
	void OnRep_Kind();

	UFUNCTION()
	void OnRep_Fill();

	UFUNCTION()
	void OnRep_Attached();

private:
	void BuildVisual();
	void UpdateVisual(float DeltaSeconds);
	void TickServer(float DeltaSeconds);
	UStaticMeshComponent* AddPart(class UStaticMesh* PartMesh, const FVector& Rel, const FVector& Scale, const FLinearColor& Color, bool bGlow);

	bool bBuilt = false;
	bool bOrphan = false;
	float IdleSeconds = 0.0f;
	float SpillCueCooldown = 0.0f;
	float LastFill = -1.0f;
	float FlameTime = 0.0f;
	float CarryLogClock = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WaterDisc;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PieceMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlameMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlameLight;

	// Playful touches (every machine, visual only): the water sloshes when the carrier hurries, a fish flops on the
	// crate now and then, the fresh bread steams.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FishMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SteamPuffs;

	TArray<FVector> FishBase;
	float SloshAmp = 0.0f;
	float SloshTime = 0.0f;
	float FlopClock = 0.0f;
	int32 FlopIndex = 0;
};
