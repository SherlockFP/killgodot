#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGPassage.generated.h"

class AKGCharacter;
class UBoxComponent;

/**
 * One end of a two-way passage between the village and the underground (the same persistent level: the well cellar
 * and the catacombs are built below the terrain, Tools/Unreal/kg_build_underground.py). E on the hitbox (the well rim,
 * the mausoleum door, the crypt door at the top of the stair) moves you to the linked end's arrival point behind a
 * short fade; bAutoExit ends (the top of the well shaft) take whoever climbs up into them.
 * Server authoritative (teleport + control rotation); the owner gets a camera fade, everyone hears the door.
 */
UCLASS()
class KILLGODOT_API AKGPassage : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGPassage();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Authority: send Who to the linked passage's arrival. Returns false on cooldown / missing link. */
	bool TravelThrough(AKGCharacter* Who);

	/** Where people arrive when they come from the other end (world). */
	FTransform GetArrival() const;

	static AKGPassage* FindById(const UWorld* World, FName Id);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FName PassageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FText Prompt;

	/** Actor-local arrival point and facing (yaw relative to the actor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FVector ArrivalLocal = FVector(150.0, 0.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	float ArrivalYaw = 0.0f;

	/** Arrivals keep falling (a ladder below grabs them) instead of standing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	bool bArriveFalling = false;

	/** Hitbox half size (cm) and actor-local centre: the E target, or the trigger of an auto exit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FVector HitboxExtent = FVector(60.0, 60.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FVector HitboxOffset = FVector(0.0, 0.0, 100.0);

	/** Climbing up into the hitbox (the top of a ladder shaft) travels without E. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	bool bAutoExit = false;

	/** Door sound at both ends: S_Passage_Door (stone door) or S_Passage_Ladder (the well). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage")
	FName SoundName = TEXT("S_Passage_Door");

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTravelled(FVector_NetQuantize From, FVector_NetQuantize To);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Hitbox;

private:
	void ApplyHitbox();
};
