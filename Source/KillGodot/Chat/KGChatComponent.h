#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Chat/KGChatTypes.h"
#include "Components/ActorComponent.h"
#include "KGChatComponent.generated.h"

class APlayerController;
class APlayerState;
class UWorld;

/** One line in a local player's history. */
struct FKGChatLine
{
	FKGChatMessage Message;
	/** FPlatformTime::Seconds() when it arrived (UI fade). */
	double ReceivedAt = 0.0;
	/** Monotonic id so the UI can tell new lines from old ones. */
	uint32 Serial = 0;
};

/** A chat emote (/wave ...): a view of FKGEmoteCatalog (Emote/KGEmoteCatalog.h, the single source of truth). It pops a
 *  reaction bubble and a "* Name waves *" line; the body animation starts through UKGChatComponent::EmoteGate(). */
struct KILLGODOT_API FKGChatEmote
{
	const TCHAR* Id;
	/** Emoji shown in the bubble. */
	const TCHAR* Emoji;
	/** Narration verb: "* Wanderer waves *". */
	const TCHAR* Verb;

	static const FKGChatEmote* Find(FName Id);
	static TArrayView<const FKGChatEmote> GetAll();
};

DECLARE_MULTICAST_DELEGATE_OneParam(FKGOnChatLine, const FKGChatLine& /*Line*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FKGOnChatReaction, APlayerController* /*LocalViewer*/, APlayerState* /*Sender*/,
                                       int32 /*EmojiIndex*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FKGOnEmoteRequested, APlayerState* /*Who*/, FName /*EmoteId*/);
/** Server: may Who start this emote on its body (and does it)? None = go ahead (bubble + narration follow). */
DECLARE_DELEGATE_RetVal_TwoParams(EKGChatReject, FKGEmoteGate, APlayerState* /*Who*/, FName /*EmoteId*/);

/**
 * Per-player text chat relay, attached at runtime to every AKGPlayerState by UKGChatSubsystem (humans and bots;
 * owned by the player's controller, so the owning client may call its Server RPCs).
 *
 * Server: ServerSendChat -> sanitise -> rate limit -> FKGChatRules::CanSend -> ClientReceiveChat on every eligible
 * receiver's own component (Client RPCs only reach that player's connection, so dead chat and team chat never touch
 * the wire of anyone who may not read them). Reactions / emotes work the same way with ClientReaction.
 * Client: keeps the last 50 lines, the local mute list, and fires OnLine for the UI.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGChatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UKGChatComponent* FindForPlayer(const APlayerState* PlayerState);
	static UKGChatComponent* FindForController(const APlayerController* Controller);

	static constexpr int32 HistorySize = 50;

	// ---- Client requests (owning client or listen host) ----

	/** Text as typed. Handles "/channel text", "/r emoji", "/wave" emotes, "/mute" "/unmute" "/report" "/help". */
	void SubmitInput(EKGChatChannel Channel, const FString& Typed);
	void RequestSend(EKGChatChannel Channel, const FString& Text);
	void RequestReaction(int32 EmojiIndex);
	void RequestEmote(FName EmoteId);

	// ---- Local (client) state ----

	const TArray<FKGChatLine>& GetHistory() const { return History; }
	/** Adds a line to this machine's history (system mirrors, hints). */
	void AddLocalLine(const FKGChatMessage& Message);
	void AddHint(const FText& Text);
	void ClearHistory();

	bool IsMuted(int32 PlayerId) const { return MutedIds.Contains(PlayerId); }
	void SetMuted(int32 PlayerId, bool bMuted);

	/** Local mirror of the server's rules for the UI (channel pill, greyed channels). */
	FKGChatParticipant MakeLocalParticipant() const;

	FKGOnChatLine OnLine;

	/** Fires on the viewing machine when a reaction bubble must pop over Sender's head. */
	static FKGOnChatReaction& OnReaction();
	/** Server: a validated emote request, after the body started it (listeners: logs, future NPC mimes). */
	static FKGOnEmoteRequested& OnEmoteRequested();
	/** Server: bound by UKGEmoteSubsystem; starts the body animation or refuses (the bubble is skipped then). */
	static FKGEmoteGate& EmoteGate();

	// ---- Server API ----

	/** Authority: a crier line to everyone (or only to ghosts / one faction via Channel = Dead / Team rules). */
	static void BroadcastSystem(UWorld* World, const FString& Text);
	/** Authority: Blackmailer hook - no written chat until cleared (reactions still allowed). */
	void SetSilenced(bool bInSilenced);
	bool IsSilenced() const { return bSilenced; }

	/** Authority: rules input for a player (life state, secret faction, trial, position). */
	static FKGChatParticipant MakeParticipant(const APlayerState* PlayerState);

protected:
	UFUNCTION(Server, Reliable)
	void ServerSendChat(EKGChatChannel Channel, const FString& Text);

	UFUNCTION(Server, Unreliable)
	void ServerReact(uint8 EmojiIndex);

	UFUNCTION(Server, Reliable)
	void ServerEmote(FName EmoteId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FKGChatMessage& Message);

	UFUNCTION(Client, Unreliable)
	void ClientReaction(APlayerState* Sender, uint8 EmojiIndex);

	UFUNCTION(Client, Reliable)
	void ClientChatRejected(EKGChatReject Reason);

	/** Blackmailed: replicated to the owner only so the input box can say why it is closed. */
	UPROPERTY(Replicated)
	bool bSilenced = false;

private:
	void HandleSend(EKGChatChannel Channel, const FString& Text);
	void HandleReact(int32 EmojiIndex, const FKGChatEmote* Emote);
	/** Server: Client RPC to every eligible human receiver. */
	void Deliver(const FKGChatMessage& Message, const FKGChatParticipant& Sender, bool bReaction, int32 EmojiIndex);
	APlayerController* GetOwningController() const;
	bool IsLocallyOwned() const;
	FKGChatMessage MakeMessage(EKGChatChannel Channel, const FString& Text, uint8 Flags) const;

	TArray<FKGChatLine> History;
	TSet<int32> MutedIds;
	uint32 NextSerial = 1;
	FKGChatRateLimiter ChatLimiter;
	FKGChatRateLimiter ReactLimiter;
};
