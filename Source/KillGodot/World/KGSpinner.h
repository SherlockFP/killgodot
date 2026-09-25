#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGSpinner.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * Cosmetic motion for set dressing: windmill sails, weather vanes, swinging shop signs, hanging lanterns,
 * bobbing buoys. Purely time driven (world time), so every client sees the same pose without replication, and it
 * stops ticking when it has not been rendered recently. Placed by the dressing modules (Tools/Unreal/dressing).
 */
UCLASS()
class KILLGODOT_API AKGSpinner : public AActor
{
	GENERATED_BODY()

public:
	AKGSpinner();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Spinner")
	void SetMesh(UStaticMesh* NewMesh);

	/** Constant spin, degrees per second around the local axes (e.g. Roll = sails around their hub). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner")
	FRotator SpinRate = FRotator::ZeroRotator;

	/** Pendulum sway amplitude in degrees around local X (signs, lanterns). 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner")
	float SwayDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner")
	float SwayHz = 0.35f;

	/** Vertical bob amplitude in cm (buoys, floating crates). 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner")
	float BobCm = 0.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	FRotator BaseRotation = FRotator::ZeroRotator;
	FVector BaseLocation = FVector::ZeroVector;
	float Phase = 0.0f;
};
