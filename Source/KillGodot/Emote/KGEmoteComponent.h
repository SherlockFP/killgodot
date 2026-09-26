#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Components/ActorComponent.h"
#include "Emote/KGEmoteCatalog.h"
#include "KGEmoteComponent.generated.h"

class AKGCharacter;
class APlayerState;
class UAnimSequence;
class UKGBodyAnimInstance;
class UPrimitiveComponent;
struct FMinimalViewInfo;

/** The replicated emote state of one body. */
USTRUCT()
struct KILLGODOT_API FKGEmotePlayback
{
	GENERATED_BODY()

	/** FKGEmoteCatalog index + 1; 0 = not emoting. */
	UPROPERTY()
	uint8 Emote = 0;

	/** Bumped on every start, so the same emote twice in a row replays everywhere. */
	UPROPERTY()
	uint8 Serial = 0;
};

/** SPRINT-023: the replicated partner-emote state of one body (TF2 partner taunts). */
USTRUCT()
struct KILLGODOT_API FKGPartnerState
{
	GENERATED_BODY()

	/** EKGPartnerKind */
	UPROPERTY()
	uint8 Kind = 0;

	/** EKGPartnerStage */
	UPROPERTY()
	uint8 Stage = 0;

	/** The other body once accepted (null while offering). */
	UPROPERTY()
	TObjectPtr<APlayerState> Partner;

	/** Bumped on every offer so repeats replay everywhere. */
	UPROPERTY()
	uint8 Serial = 0;

	/** RPS: FKGPartnerRules::PackRps; dance-off: 1 = initiator wins, 2 = acceptor wins. */
	UPROPERTY()
	uint8 Result = 0;

	/** This body made the offer (its component runs the pairing's clock). */
	UPROPERTY()
	uint8 bInitiator = 0;

	EKGPartnerKind GetKind() const { return static_cast<EKGPartnerKind>(Kind); }
	EKGPartnerStage GetStage() const { return static_cast<EKGPartnerStage>(Stage); }
	bool IsActive() const { return Stage != 0; }
};

/**
 * Emotes on a villager body (default subobject of AKGCharacter).
 *
 * Server authoritative: requests arrive through the chat relay (/wave, the G wheel, Shift+1..4, kg.Emote all call
 * UKGChatComponent::RequestEmote -> server validates the reaction, then the emote gate calls ServerTryStart here),
 * which checks FKGEmoteRules (reaction phases, no ghosts, body free) and a per-player rate limit, then sets the
 * replicated Playback. The server also ends it: clip finished, moving (full body), attacking, being hit, a meeting /
 * trial / curfew starting, death.
 *
 * Every machine: Playback drives the body's UKGBodyAnimInstance emote layer (full body, or upper body over the
 * locomotion). The owning player: third-person camera pulled out behind/around the body for full-body emotes (and
 * upper-body ones without a first-person gesture), viewmodel hidden meanwhile; wave/point/clap/salute instead play a
 * gesture on the first-person arms. Moving / attacking cancels locally at once (then tells the server).
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGEmoteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGEmoteComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The emote component of a player's current body, or null (ghost, lobby, spectator). */
	static UKGEmoteComponent* FindForPlayer(const APlayerState* PlayerState);

	// ---- Server ----

	/** Authority: validate and start. bIgnoreRateLimit for dev verbs / bots. */
	EKGEmoteReject ServerTryStart(FName IdOrAlias, bool bIgnoreRateLimit = false);
	/** Authority: end the running emote (no-op when idle). */
	void ServerStop(EKGEmoteStop Reason);

	// ---- Owning client ----

	/** Stop now (predicted locally) and tell the server. */
	void RequestStop(EKGEmoteStop Reason = EKGEmoteStop::Requested);
	/** The owner attacked / shoved / used something: stop the local presentation at once (the server stops too). */
	void NotifyLocalAction();

	// ---- Queries ----

	/** Replicated (server) state. */
	const FKGEmoteDef* GetActiveEmote() const { return FKGEmoteCatalog::Get(int32(Playback.Emote) - 1); }
	FName GetActiveEmoteId() const;
	bool IsEmoting() const { return Playback.Emote != 0; }
	uint8 GetSerial() const { return Playback.Serial; }
	EKGEmoteStop GetLastStopReason() const { return LastStop; }
	/** What this machine currently shows (may lag the replicated state by a frame, or lead it for the owner). */
	const FKGEmoteDef* GetShownEmote() const;

	/** Owner: 0 = first person, 1 = fully in the third-person emote camera. */
	float GetCameraAlpha() const { return CamAlpha; }
	/** Owner: the first-person arms / held item must hide (third-person emote camera in or blending). */
	bool WantsViewmodelHidden() const { return bWantThirdPerson || CamAlpha > 0.001f; }

	/** AKGCharacter::CalcCamera: the owner's view during third-person emotes. */
	void ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView);

	/** Loaded clip for a catalog path (cached, null if the asset is missing). */
	static UAnimSequence* LoadClip(const FSoftObjectPath& Path);
	/** Server duration of a one-shot (intro + clip, seconds); loops: MaxSeconds, or -1 = until cancelled. */
	static float ComputeDuration(const FKGEmoteDef& Def);

	/** Body state the rules need (server; tests read it). */
	FKGEmoteBodyState MakeBodyState() const;

	// ---- Partner emotes (SPRINT-023) ----

	/** Owning client: offer a partner emote (/highfive, kg.Partner highfive). */
	void RequestPartnerOffer(FName IdOrAlias);
	/** Owning client: withdraw an offer / leave a pairing. */
	void RequestPartnerCancel();
	/** Owning client (E): accept the nearest open offer within FKGPartnerRules::AcceptRadius. True if one was sent. */
	bool TryAcceptNearbyOffer();
	/** This machine: a body offering a partner emote within AcceptRadius of Me (prompt + E), or null. */
	static UKGEmoteComponent* FindNearbyOffer(const AKGCharacter* Me);

	/** Authority: start offering. */
	EKGEmoteReject ServerPartnerOffer(FName IdOrAlias, bool bIgnoreRateLimit = false);
	/** Authority, on the ACCEPTOR's component: pair with Offerer (validates distance, state, phase), aligns both. */
	EKGEmoteReject ServerPartnerAccept(UKGEmoteComponent* Offerer);
	/** Authority: end the pairing for both bodies. */
	void ServerPartnerEnd(EKGEmoteStop Reason);

	const FKGPartnerState& GetPartner() const { return Partner; }
	const FKGPartnerEmoteDef* GetPartnerDef() const;
	/** "Waiting for a partner..." (owner) / "E  High five?" (nearby) / result line. Empty when idle. */
	FText GetPartnerPrompt(bool bForOwner) const;
	/** Result narration once Stage == Result ("rock beats scissors: Wanderer wins"). */
	FString GetPartnerResultText() const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestPartnerOffer(FName IdOrAlias);

	UFUNCTION(Server, Reliable)
	void ServerRequestPartnerAccept(APlayerState* Offerer);

	UFUNCTION(Server, Reliable)
	void ServerRequestPartnerCancel();

	UFUNCTION()
	void OnRep_Partner();

	UPROPERTY(ReplicatedUsing = OnRep_Partner)
	FKGPartnerState Partner;

	void TickPartnerServer(float DeltaTime);
	void SetPartnerState(const FKGPartnerState& NewState);
	void PartnerRevealResult();
	float PartnerElapsed = 0.0f;
	FKGChatRateLimiter PartnerLimiter;
	uint8 PartnerLoggedSerial = 0;
	uint8 PartnerLoggedStage = 0;
	UFUNCTION(Server, Reliable)
	void ServerStopEmote(EKGEmoteStop Reason);

	UFUNCTION()
	void OnRep_Playback();

	UPROPERTY(ReplicatedUsing = OnRep_Playback)
	FKGEmotePlayback Playback;

private:
	AKGCharacter* GetCharacter() const;
	UKGBodyAnimInstance* GetBodyAnim() const;
	bool IsOwnerView() const;
	void TickServer(float DeltaTime);
	void SyncPresentation();
	void TickPresentation(float DeltaTime);
	void TickOwnerView(float DeltaTime);
	void StartPresentation(const FKGEmoteDef& Def);
	void StopPresentation(float BlendOut);
	void SetBodyYawFree(bool bFree);
	void SetOwnerSeesBody(bool bSee);

	// Server
	FKGChatRateLimiter Limiter;
	float ServerElapsed = 0.0f;
	float ServerDuration = -1.0f;
	EKGPhase LastPhase = EKGPhase::Lobby;
	bool bHasLastPhase = false;
	EKGEmoteStop LastStop = EKGEmoteStop::Finished;

	// Presentation (every machine)
	uint8 ShownEmote = 0;
	uint8 ShownSerial = 0;
	bool bLocallyCancelled = false;
	float ShownElapsed = 0.0f;
	bool bYawFreed = false;
	bool bSavedUseControllerYaw = true;

	// Owner view
	bool bWantThirdPerson = false;
	bool bWantFullBodyCam = false;
	float CamAlpha = 0.0f;
	float CamOrbit = 0.0f;
	bool bOwnerSeesBody = false;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> UnhiddenForOwner;
};
