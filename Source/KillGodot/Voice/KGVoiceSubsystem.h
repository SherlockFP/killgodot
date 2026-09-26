#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGVoiceSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UKGVoiceComponent;

/**
 * Voice glue, zero-integration like UKGChatSubsystem:
 *  - server: attaches a UKGVoiceComponent to every AKGPlayerState (humans and bots);
 *  - local players: push-to-talk (V) or open mic (Settings -> Audio), the Z/X/C voice-command radials, the overlay;
 *  - dev (-KGVoiceSmoke): the headless two-process smoke (Tools/Unreal/kg_voice_smoke.ps1): both machines send a
 *    synthetic tone, the host walks the client through near / mid / far / muted / meeting / ghost steps and every
 *    machine logs what it heard per second (KG_VOICE_HEARD) so the script can check gain, silence and ghost rules.
 */
UCLASS()
class KILLGODOT_API UKGVoiceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Local player: say a voice command through the relay (kg.Bark). */
	static void RequestBark(APlayerController* PC, FName CommandId);
	/** The smoke is running in this process (-KGVoiceSmoke): input does not drive the microphone. */
	static bool IsSmoke();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureComponents();
	static void EnsureVoiceComponent(APlayerState* PlayerState);
	void TickLocalPlayer(APlayerController* PC, UKGVoiceComponent* Voice);
	void TickSmoke(APlayerController* PC, UKGVoiceComponent* Voice, float DeltaTime);

	int32 LastPlayerCount = -1;
	float SmokeClock = -1.0f;
	float SmokeWindow = 0.0f;
	int32 SmokeStep = 0;
	bool bSmokeMuted = false;
	bool bSmokeDone = false;
};
