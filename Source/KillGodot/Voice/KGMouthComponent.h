#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGMouthComponent.generated.h"

class UStaticMeshComponent;
class USkeletalMesh;

/**
 * Talking mouth (SPRINT-023). The Quaternius villager has no jaw bone and no morph targets (Head / neck_01 /
 * spine_* only), so the mouth is a small dark oval mesh on the Head bone whose height follows JawOpen: a closed slit
 * at rest, an open "o" while talking. Readable from the 20 m the voice carries, costs one static mesh component.
 *
 * Drivers:
 *  - live voice: UKGVoiceComponent pushes the speaker's packet amplitude (PushAmplitude) -> dB gate, normalise,
 *    gamma, attack/release (AmplitudeToJaw, unit-tested);
 *  - barks: StartBark plays the deterministic syllable envelope of a voice command (FKGVoiceCommandCatalog::
 *    MouthEnvelope) so bots and remote players chew in time with the placeholder audio;
 *  - kg.Mouth.Force <0..1> pins every mouth for screenshots (-1 = off).
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGMouthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGMouthComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** Latest linear amplitude (0..1) from the voice pipeline. Game thread. Decays to silence after 0.25 s. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Voice")
	void PushAmplitude(float LinearAmplitude);

	/** Chew a bark: Syllables over FKGVoiceCommandCatalog::BarkDuration seconds, Seed picks the accents. */
	void StartBark(int32 Syllables, uint32 Seed);
	void StopBark() { BarkSyllables = 0; }
	bool IsBarking() const { return BarkSyllables > 0; }

	/** Smoothed jaw opening, 0 (closed) .. 1 (fully open). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Voice")
	float GetJawOpen() const { return JawOpen; }

	/** True while voice or a bark drives the mouth (HUD speaking indicator for bodies you can see). */
	bool IsTalking() const { return JawOpen > 0.05f || IsBarking(); }

	/** Maps amplitude to a readable jaw value: dB gate, normalise to [Gate, Max] dB, gamma. Pure; unit-tested. */
	static float AmplitudeToJaw(float LinearAmplitude, float GateDb, float MaxDb, float Gamma);

	/** The mouth mesh (null on dedicated servers / before the body exists). Tests and shots read its scale. */
	UStaticMeshComponent* GetMouthMesh() const { return MouthMesh; }

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

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MouthMesh;

private:
	void EnsureMouthMesh();
	void PlaceMouthMesh();
	void ApplyMouthMesh();

	float TargetAmplitude = 0.0f;
	double LastPushTime = -1000.0;
	float JawOpen = 0.0f;

	int32 BarkSyllables = 0;
	uint32 BarkSeed = 0;
	float BarkTime = 0.0f;

	TWeakObjectPtr<USkeletalMesh> PlacedFor;
	FTransform HeadRefCS;
	bool bHeadValid = false;
};
