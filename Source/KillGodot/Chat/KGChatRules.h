#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatTypes.h"
#include "Core/KGPlayerState.h"
#include "Core/KGTypes.h"

/** Trial sub-stages derived from the remaining trial time (Docs/01_GDD_Core.md §1: 20 s defence, 15 s judgement,
 *  8 s last words). Counting from the end keeps it right even if the trial length changes. */
enum class EKGTrialStage : uint8
{
	Defense,
	Judgement,
	LastWords
};

/** Everything the channel rules need to know about one player (built by the server from the player state). */
struct KILLGODOT_API FKGChatParticipant
{
	EKGLifeState Life = EKGLifeState::Alive;
	/** Only meaningful when bHasRole (roles are dealt at RoleReveal). */
	EKGFaction Faction = EKGFaction::Town;
	bool bHasRole = false;
	/** Blackmailed: no written chat (reactions/emotes still allowed). */
	bool bSilenced = false;
	/** On the gallows (AKGGameState::OnTrial). */
	bool bOnTrial = false;
	/** World position of the body; ghosts have none (they spectate). */
	FVector Location = FVector::ZeroVector;
	bool bHasLocation = false;
};

/** Token bucket + duplicate filter, one per sender, server side. */
struct KILLGODOT_API FKGChatRateLimiter
{
	float Burst = 4.0f;
	/** Tokens regained per second (1 message every 1.5 s sustained). */
	float RefillPerSecond = 1.0f / 1.5f;
	/** The same text again within this window is dropped. */
	float DuplicateWindow = 8.0f;

	float Tokens = -1.0f;
	double LastTime = 0.0;
	FString LastText;
	double LastTextTime = -1000.0;

	/** None if the message may pass (and consumes a token), RateLimited / Duplicate otherwise. */
	EKGChatReject TryConsume(double Now, const FString& Text);
};

/**
 * Pure chat rules: who may write where, who receives what, the sanitiser, colours. No world access, so the
 * automation tests (KillGodot.Chat.*) cover every branch. The per-phase table lives in Docs/08_UI_UX.md §6.1.
 */
struct KILLGODOT_API FKGChatRules
{
	static constexpr int32 MaxChars = 140;
	static constexpr int32 MaxEmojis = 12;
	/** A character repeated more than this in a row is cut ("!!!!!!!!!!!" -> "!!!!!!"). */
	static constexpr int32 MaxRepeat = 6;
	/** Nearby channel radius (20 m, Docs/01_GDD_Core.md §12). */
	static constexpr float NearbyRadius = 2000.0f;
	/** Reaction bubbles reach this far (they are seen, not heard). */
	static constexpr float ReactionRadius = 3500.0f;

	static EKGTrialStage GetTrialStage(float PhaseRemaining);

	/** Factions that share a secret channel. Solo killers and neutrals have none. */
	static bool IsTeamFaction(EKGFaction Faction);

	/** May Sender write on Channel right now? */
	static EKGChatReject CanSend(const FKGChatParticipant& Sender, EKGChatChannel Channel, EKGPhase Phase,
	                             float PhaseRemaining);

	/** Does Receiver get a line Sender wrote on Channel? (bSelf: the sender's own echo, always delivered.) */
	static bool CanReceive(const FKGChatParticipant& Sender, const FKGChatParticipant& Receiver, EKGChatChannel Channel,
	                       bool bSelf);

	/** Channels Sender may write on now, in Tab order (All, Nearby, Team, Dead). */
	static TArray<EKGChatChannel> GetSendableChannels(const FKGChatParticipant& Sender, EKGPhase Phase,
	                                                  float PhaseRemaining);

	/** Quick reactions / emotes: allowed wherever you could talk (blackmailed players may still emote). */
	static bool CanReact(const FKGChatParticipant& Sender, EKGPhase Phase, float PhaseRemaining);

	/** Does Receiver see Sender's reaction bubble? Living senders: bodies within ReactionRadius, plus ghosts.
	 *  Ghost senders: ghosts only (delivered as a Dead-chat line). */
	static bool CanSeeReaction(const FKGChatParticipant& Sender, const FKGChatParticipant& Receiver, bool bSelf);

	/**
	 * Never trust the client: emoji conversion, strips control / bidi / zero-width / unknown private-use characters
	 * and stray surrogates, collapses whitespace, trims, caps repeated characters, emojis and length.
	 * Returns an empty string when nothing printable is left.
	 */
	static FString Sanitize(const FString& Raw);

	/** Stable colour seed: EOS PUID when known, otherwise the display name (PIE / LAN). Never the role. */
	static int32 MakeColorSeed(const FString& Puid, const FString& PlayerName);
	static FLinearColor NameColor(int32 ColorSeed);

	static FLinearColor ChannelColor(EKGChatChannel Channel);
	static FText ChannelLabel(EKGChatChannel Channel);
	static FText RejectReason(EKGChatReject Reason);
	/** "/all" "/a" "/t" "/team" ... -> channel; false if Word is not a channel switch. */
	static bool ParseChannelCommand(const FString& Word, EKGChatChannel& OutChannel);
};
