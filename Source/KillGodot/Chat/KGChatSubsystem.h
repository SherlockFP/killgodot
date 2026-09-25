#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGChatSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UKGChatComponent;
struct FKGChatMirror;

/**
 * Text chat glue, zero-integration like UKGPlayerExtrasSubsystem (no edits to the player controller, character or HUD):
 *  - server: gives every AKGPlayerState a UKGChatComponent (replicated dynamic subobject),
 *  - local players: creates the chat overlay (FKGChatUI), polls Enter / T / "/" (open), G (reaction wheel),
 *  - local players: mirrors public, already-replicated events into crier lines (announcements, phase changes,
 *    accusations, "X has voted", your own death) so they need no extra network traffic,
 *  - dev console commands kg.Chat.* (not in Shipping).
 */
UCLASS()
class KILLGODOT_API UKGChatSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Authority: idempotent. */
	static void EnsureChatComponent(APlayerState* PlayerState);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void TickLocalPlayer(APlayerController* PC, UKGChatComponent* Chat);
	void MirrorPublicEvents(APlayerController* PC, UKGChatComponent* Chat);

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<APlayerState>> Pending;
	float SweepSeconds = 0.0f;
	TMap<TWeakObjectPtr<APlayerController>, TSharedPtr<FKGChatMirror>> Mirrors;

	/** Dev (-KGChatSmoke): scripted chat traffic for headless network checks (Tools/Unreal/kg_chat_smoke.ps1). */
	void TickSmoke(APlayerController* PC, UKGChatComponent* Chat, float DeltaTime);
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	FDelegateHandle SmokeReactionHandle;
};
