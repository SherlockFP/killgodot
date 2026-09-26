#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGEmoteSubsystem.generated.h"

class APlayerController;

/**
 * Emote glue, zero-integration like UKGChatSubsystem:
 *  - server: binds UKGChatComponent::EmoteGate() so every chat emote (/wave, G wheel, hotkeys, kg.Emote) starts the
 *    body animation through UKGEmoteComponent::ServerTryStart before the bubble pops,
 *  - local players: Shift+1..4 play the favourite emotes (kg.Emote.Favorites; the same key again stops it),
 *  - dev (-KGEmoteSmoke): scripted emote traffic for the headless network smoke (Tools/Unreal/kg_emote_smoke.ps1).
 */
UCLASS()
class KILLGODOT_API UKGEmoteSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Favourite emotes for Shift+1..4 (kg.Emote.Favorites, comma separated ids). */
	static TArray<FName> GetFavorites();

	/** Local player: ask for an emote the normal way (chat relay -> server validation -> bubble + body). */
	static void RequestEmote(APlayerController* PC, FName EmoteId);
	/** Local player: stop the running emote. */
	static void RequestStop(APlayerController* PC);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TickLocalPlayer(APlayerController* PC);
	void TickSmoke(APlayerController* PC, float DeltaTime);
	/** -KGPartnerSmoke: offer -> accept -> synced clips, RPS outcome, cancel by attack (Tools/Unreal/kg_partner_smoke.ps1). */
	void TickPartnerSmoke(APlayerController* PC, float DeltaTime);
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	float PartnerClock = -1.0f;
	int32 PartnerStep = 0;
};
