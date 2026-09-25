#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGMouthComponent.generated.h"

/**
 * Voice-driven jaw (R.E.P.O.-style). A voice source (EOS unmixed PCM -> envelope follower, see
 * Docs/05_Tech_Architecture.md §5) pushes linear amplitude; the AnimBP reads GetJawOpen() into the "JawOpen" curve,
 * which rotates the puppet's hinged jaw bone.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGMouthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGMouthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** Latest linear amplitude (0..1) from the voice pipeline. Game thread. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Voice")
	void PushAmplitude(float LinearAmplitude) { TargetAmplitude = LinearAmplitude; }

	/** Smoothed jaw opening, 0 (closed) .. 1 (fully open). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Voice")
	float GetJawOpen() const { return JawOpen; }

	/** Maps amplitude to a readable jaw value: dB gate, normalise to [Gate, Max] dB, gamma. Pure; unit-tested. */
	static float AmplitudeToJaw(float LinearAmplitude, float GateDb, float MaxDb, float Gamma);

protected:
	UPROPERTY(EditAnywhere, Category = "Voice")
	float GateDb = -45.0f;

	UPROPERTY(EditAnywhere, Category = "Voice")
	float MaxDb = -6.0f;

	UPROPERTY(EditAnywhere, Category = "Voice")
	float Gamma = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Voice")
	float AttackSeconds = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Voice")
	float ReleaseSeconds = 0.12f;

private:
	float TargetAmplitude = 0.0f;
	float JawOpen = 0.0f;
};
