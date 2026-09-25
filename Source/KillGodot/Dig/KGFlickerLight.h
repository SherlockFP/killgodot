#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGFlickerLight.generated.h"

class UPointLightComponent;

/**
 * Candle / torch light for the underground: a small unshadowed point light whose intensity breathes like a flame.
 * Cosmetic only (never on a dedicated server), ticks at 20 Hz and only while its light is near a local camera.
 */
UCLASS()
class KILLGODOT_API AKGFlickerLight : public AActor
{
	GENERATED_BODY()

public:
	AKGFlickerLight();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Candela at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float BaseIntensity = 12.0f;

	/** 0 = steady, 0.3 = a draughty candle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker")
	float Flicker = 0.18f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

private:
	float Time = 0.0f;
	float Phase = 0.0f;
};
