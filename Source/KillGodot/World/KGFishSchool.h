#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGFishSchool.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * A school of fish circling under the surface (cosmetic, deterministic from world time so every machine sees
 * roughly the same school). Fish scatter from nearby pawns. Tail wiggle is done by M_KG_Fish (vertex alpha).
 * Later the fishing system samples schools to decide bites (more fish nearby = faster bites).
 */
UCLASS()
class KILLGODOT_API AKGFishSchool : public AActor
{
	GENERATED_BODY()

public:
	AKGFishSchool();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	TObjectPtr<UStaticMesh> FishMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	int32 Count = 18;

	/** Orbit radius (cm) and depth band below the sea surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	float Radius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	float MinDepth = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	float MaxDepth = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	float Speed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	float FishScale = 1.0f;

	/** Species id for fishing (Mackerel, Cod, Salmon, GoldenCarp). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish")
	FName Species = TEXT("Mackerel");

	UFUNCTION(BlueprintPure, Category = "Fish")
	int32 GetFishCount() const { return Count; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Fish;

	/** Panic 0..1 (someone swimming close): wider, faster circles. */
	float Panic = 0.0f;
};
