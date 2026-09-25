#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGBuoyancyComponent.generated.h"

/**
 * Floats the owner's physics-simulating root on the FKGWaves sea: samples the bottom corners of the bounds, pushes
 * each submerged point up proportionally to its depth, and adds water drag. Crates, barrels, boats, bodies.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGBuoyancyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 1 = neutral when the sample depth reaches FullDepth; >1 floats higher. */
	UPROPERTY(EditAnywhere, Category = "Buoyancy")
	float Buoyancy = 1.6f;

	/** Depth (cm) at which a sample point gives full lift. */
	UPROPERTY(EditAnywhere, Category = "Buoyancy")
	float FullDepth = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Buoyancy")
	float LinearDrag = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Buoyancy")
	float AngularDrag = 1.5f;
};
