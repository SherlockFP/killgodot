#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGLadder.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Climbable ladder (towers, lighthouse, lofts, the well shaft). While a character overlaps the climb volume and
 * pushes towards the ladder (W), it moves up in flying mode; S climbs down; walking away or jumping lets go.
 * Visual rails/rungs are built from kit posts by the level builder; Height sets the climb volume.
 */
UCLASS()
class KILLGODOT_API AKGLadder : public AActor
{
	GENERATED_BODY()

public:
	AKGLadder();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Climb height in cm (bottom at the actor origin). The ladder faces +X (climber stands at -X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float Height = 600.0f;

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetHeight(float NewHeight);

	float GetTopZ() const { return GetActorLocation().Z + Height; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> ClimbVolume;
};
