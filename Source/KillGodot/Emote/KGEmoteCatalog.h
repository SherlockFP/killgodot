#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Core/KGTypes.h"
#include "KGEmoteCatalog.generated.h"

/** Which part of the body an emote drives. Upper-body emotes blend from spine_01 up over the locomotion. */
UENUM(BlueprintType)
enum class EKGEmoteLayer : uint8
{
	/** Everything; moving cancels it; the owner sees it from a third-person camera. */
	FullBody,
	/** Torso, arms and head only: you can walk while waving. */
	UpperBody
};

/** Why the server refused an emote (mapped to a chat hint for the player). */
UENUM(BlueprintType)
enum class EKGEmoteReject : uint8
{
	None,
	Unknown,
	/** Ghosts (and dead bodies) cannot emote. */
	Dead,
	/** Same phases as reactions (FKGChatRules::CanReact): no emotes at night, at the role reveal, during migration. */
	Phase,
	/** Swimming, climbing, seated, carrying something, mid-air. */
	Busy,
	/** Full-body emotes need you to stand still. */
	Moving,
	RateLimited,
	/** No AKGCharacter to animate (lobby screen, spectator). */
	NoBody
};

/** Why a running emote ended (logs, smoke tests). */
UENUM(BlueprintType)
enum class EKGEmoteStop : uint8
{
	Finished,
	Replaced,
	Requested,
	Moved,
	Attacked,
	Damaged,
	Phase,
	Died,
	Busy
};

/**
 * One emote. Pure data; FKGEmoteCatalog below is the single source of truth (no editor assets), like the item and
 * cosmetic catalogs. Adding an emote = one row. Chat commands (/wave), the G wheel ring, the Shift+1..4 favourites,
 * kg.Emote and the dev panel all read this table.
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGEmoteDef
{
	GENERATED_BODY()

	/** Canonical id = the chat command (/wave) and the kg.Emote argument. */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FText DisplayName;

	/** FKGEmoji id popped over the head as a bubble when it starts. */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FString Emoji;

	/** Chat narration: "* Wanderer <Verb> *". */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FString Verb;

	/** Other chat words for the same emote ("cheers", "o7"), comma separated. */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FString Aliases;

	/** Third-person body clip (villager skeleton). */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FSoftObjectPath Clip;

	/** Optional one-shot played before Clip (sit down -> sitting loop). */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FSoftObjectPath IntroClip;

	/** Optional first-person arms gesture (FPArms2 rig): the owner stays in first person and sees this instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	FSoftObjectPath FirstPersonClip;

	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	EKGEmoteLayer Layer = EKGEmoteLayer::UpperBody;

	/** Loops until cancelled (full body: by moving) or until MaxSeconds. One-shots end with their clip. */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	bool bLoop = false;

	/** Loops: automatic stop after this long (0 = never; upper-body loops always get one). */
	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	float MaxSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	float BlendIn = 0.25f;

	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	float BlendOut = 0.3f;

	UPROPERTY(BlueprintReadOnly, Category = "Emote")
	float PlayRate = 1.0f;

	bool IsFullBody() const { return Layer == EKGEmoteLayer::FullBody; }
	/** The owner watches it from the third-person camera (full body, or no first-person gesture for it). */
	bool UsesThirdPersonCamera() const { return IsFullBody() || FirstPersonClip.IsNull(); }
	bool MatchesWord(const FString& Word) const;
};

/** The emote table, wheel order (clockwise from the top of the G wheel's emote ring). */
struct KILLGODOT_API FKGEmoteCatalog
{
	static const TArray<FKGEmoteDef>& GetAll();
	/** Id or alias, case-insensitive ("Wave", "o7", "cheers"). Null if unknown. */
	static const FKGEmoteDef* Find(FName IdOrAlias);
	static const FKGEmoteDef* Find(const FString& IdOrAlias);
	/** Index into GetAll() (the replicated emote number is Index + 1), INDEX_NONE if unknown. */
	static int32 IndexOf(FName IdOrAlias);
	static const FKGEmoteDef* Get(int32 Index);
	/** Asset path of the owner's-view gesture / third-person clips, for tests and preloading. */
	static TArray<FSoftObjectPath> GetAllClipPaths();
};

/** What the server knows about the body that wants to emote (built from the character; tests fill it by hand). */
struct KILLGODOT_API FKGEmoteBodyState
{
	bool bHasBody = true;
	bool bDead = false;
	bool bFalling = false;
	bool bSwimming = false;
	bool bClimbing = false;
	bool bSeated = false;
	bool bCarrying = false;
	/** Full-body emotes need you standing. */
	bool bCrouched = false;
	/** Horizontal speed (cm/s). */
	float Speed = 0.0f;
};

/** Pure emote rules (automation tests KillGodot.Emote.*). No world access. */
struct KILLGODOT_API FKGEmoteRules
{
	/** A full-body emote ends once the body moves faster than this (cm/s). Upper-body emotes ignore movement. */
	static constexpr float MoveCancelSpeed = 60.0f;

	/**
	 * May Who start Emote now? Same phases as reactions (FKGChatRules::CanReact), but ghosts and dead bodies cannot
	 * emote at all (their reactions become Dead-chat lines instead), and the body must be free for it.
	 */
	static EKGEmoteReject CanStart(const FKGEmoteDef& Emote, const FKGChatParticipant& Who, const FKGEmoteBodyState& Body,
	                               EKGPhase Phase, float PhaseRemaining);

	/** Must a running emote stop now? True with the reason (clip length / MaxSeconds are the component's job). */
	static bool ShouldStop(const FKGEmoteDef& Emote, const FKGChatParticipant& Who, const FKGEmoteBodyState& Body,
	                       EKGPhase Phase, float PhaseRemaining, EKGEmoteStop& OutReason);

	/** A meeting / trial starting clears every emote (everyone gets moved to the square), as does curfew. */
	static bool PhaseChangeStops(EKGPhase OldPhase, EKGPhase NewPhase);

	/** Per-player server limiter: 3 in a burst, then one every 2 s. */
	static FKGChatRateLimiter MakeLimiter();

	/** Chat hint for a refusal (EKGChatReject the chat relay understands). */
	static EKGChatReject ToChatReject(EKGEmoteReject Reject);
};
