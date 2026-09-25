#pragma once

#include "CoreMinimal.h"
#include "KGChatTypes.generated.h"

/**
 * Text chat channels (Docs/08_UI_UX.md §6.1). Which channel is open for whom in which phase is decided on the
 * server by FKGChatRules; the client only mirrors the rules to grey out the channel pill.
 */
UENUM(BlueprintType)
enum class EKGChatChannel : uint8
{
	/** Town square: everyone who may talk in this phase; the dead read it too. */
	All,
	/** Proximity (20 m): whoever is close to the sender; ghosts read it too. */
	Nearby,
	/** The killers' secret channel (Clockbreakers, later Vampires/Plague): same faction, alive only. */
	Team,
	/** Ghosts only; the living never receive it. */
	Dead,
	/** Town crier: phase changes, deaths, votes. Server/local only, never sendable by players. */
	System,
	MAX UMETA(Hidden)
};

/** Why the server (or the client pre-check) refused a line. */
UENUM(BlueprintType)
enum class EKGChatReject : uint8
{
	None,
	/** The channel is closed in this phase (e.g. town chat at night). */
	Closed,
	/** Dead channel for the living, or public channels for ghosts. */
	WrongLifeState,
	/** Blackmailed (Docs/02_Roles.md): no written chat, emotes only. */
	Silenced,
	/** Revenants cannot speak (Docs/01_GDD_Core.md §11.1). */
	Revenant,
	/** Trial defence / last words: only the accused may use town chat. */
	AccusedSpeaking,
	/** Team channel without a team (Town, Neutral, solo killers). */
	NoTeam,
	RateLimited,
	Duplicate,
	Empty,
	/** Local-only (never sent to the server): the local player muted the sender. */
	Muted,
	/** Emote refused: busy (swimming, climbing, seated, carrying), dead, or too many in a row. */
	EmoteBlocked,
	/** Full-body emote while moving. */
	EmoteMoving
};

/** FKGChatMessage::Flags */
namespace EKGChatFlags
{
	enum Type : uint8
	{
		None = 0,
		/** "* Wanderer waves *" emote narration. */
		Action = 1 << 0,
		/** A ghost's quick reaction (ghosts have no body to float a bubble over). */
		Reaction = 1 << 1,
		/** Generated on this machine from replicated public state (announcements, votes); never networked. */
		Local = 1 << 2,
		/** A hint for the local player only (rejections, channel tips): drawn muted. */
		Hint = 1 << 3
	};
}

/** One chat line on the wire. The text is already sanitised; emojis are private-use characters (FKGEmoji). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	EKGChatChannel Channel = EKGChatChannel::All;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderName;

	/** APlayerState::GetPlayerId() of the sender (local mute list), INDEX_NONE for system lines. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	int32 SenderId = INDEX_NONE;

	/** Stable per-player seed for the name colour (FKGChatRules::NameColor). Never derived from the role. */
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	int32 ColorSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	uint8 Flags = 0;

	bool HasFlag(EKGChatFlags::Type Flag) const { return (Flags & Flag) != 0; }
};
