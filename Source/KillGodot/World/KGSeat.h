#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGSeat.generated.h"

class AKGCharacter;
class UAnimSequence;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Stool / chair / bench seat. E to sit: the character snaps onto the seat, movement is switched off (MOVE_None on
 * the server and the owning client, so prediction never fights it), the first-person camera drops to seated eye
 * height and the body plays the sitting loop. E or Space stands up (routed through UKGInventoryRPCComponent, so no
 * change to AKGCharacter is needed). The occupant replicates; every machine applies/undoes the pose in OnRep.
 *
 * Seat frame: actor origin on the floor, +X is the direction the sitter faces, SeatHeight is the sitting surface.
 */
UCLASS()
class KILLGODOT_API AKGSeat : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGSeat();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Authority. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Seat")
	bool Sit(AKGCharacter* Who);

	/** Authority. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Seat")
	void Stand();

	/** Authority: stands Who up from whatever seat holds them. Returns false if they were not seated. */
	static bool StandUpCharacter(AKGCharacter* Who);

	static AKGSeat* FindSeatOf(const AKGCharacter* Who);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Seat")
	AKGCharacter* GetOccupant() const { return Occupant; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Seat")
	void SetSeatMesh(UStaticMesh* NewMesh);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	TObjectPtr<UStaticMesh> SeatMesh;

	/** Rotates the mesh so its front faces +X (Quaternius chairs may need 90/180/-90). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FRotator MeshRotation = FRotator::ZeroRotator;

	/** Height of the sitting surface above the actor origin (Stool 58, Bench 52, Chair_1 ~46). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float SeatHeight = 57.0f;

	/** Nudges the sitter forward (+) / back (-) along +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float SitForwardOffset = 0.0f;

	/** Seated eye: A_KG_Sitting_Idle_Loop puts the head ~34 cm lower and ~18 cm further back than standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float CameraDrop = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float CameraBack = 18.0f;

	/** Where the character is put when standing up (in front of the seat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	float StandDistance = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	TObjectPtr<UAnimSequence> SitAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	TObjectPtr<UAnimSequence> IdleAnim;

	/** Capsule-centre transform of a seated character (world space). */
	FTransform GetSitTransform() const;
	FVector GetStandLocation() const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	TObjectPtr<AKGCharacter> Occupant;

	UFUNCTION()
	void OnRep_Occupant(AKGCharacter* OldOccupant);

	void ApplySit(AKGCharacter* Who);
	void ApplyStand(AKGCharacter* Who);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	TWeakObjectPtr<AKGCharacter> Applied;
	FVector SavedCameraLocation = FVector::ZeroVector;
	bool bSavedUseControllerYaw = true;
	float LocalSeatedTime = 0.0f;
	TWeakObjectPtr<AKGCharacter> LastStood;
	double LastStandTime = -10.0;
};
