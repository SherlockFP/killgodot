#pragma once

#include "CoreMinimal.h"

/**
 * Role reveal ceremony timeline (SPRINT-015 acceptance 2, Docs/08_UI_UX.md "Role reveal"). The server owns the length
 * (the RoleReveal phase runs on AKGGameState::Clock, an FKGMatchClock); clients follow the replicated clock and play
 * these beats on top of it.
 */
namespace KGReveal
{
	/** RoleReveal phase length (AKGGameMode::GetPhaseDuration). Contract: 8-12 s. */
	inline constexpr float PhaseSeconds = 10.0f;

	// Beats, seconds since the phase began.
	inline constexpr float TableEnd = 1.0f;     // 0.0-1.0  the dealer's table fades in, the deck waits
	inline constexpr float ShuffleEnd = 3.2f;   // 1.0-3.2  two riffle shuffles
	inline constexpr float DealEnd = 4.2f;      // 3.2-4.2  one card to every seat, yours slides up to you
	inline constexpr float FlipEnd = 5.0f;      // 4.2-5.0  your card flips
	inline constexpr float FaceEnd = 5.8f;      // 5.0-5.8  role name, alignment, goal, abilities, flavour, mates

	/** "Ready" is accepted once the card is face up (server check allows a little clock skew). */
	inline constexpr float ReadyFromSeconds = FlipEnd;
	inline constexpr float ServerReadyTolerance = 0.6f;
	/** When every human is ready the server cuts the phase down to this (the fade-out). */
	inline constexpr float SkipToSeconds = 0.75f;
	/** Client fade-out after the phase ends (the village appears behind it). */
	inline constexpr float OutroSeconds = 0.7f;

	/** Named stages (screenshots, logs). */
	enum class EStage : uint8
	{
		Table,
		Shuffle,
		Deal,
		Flip,
		Role
	};

	inline EStage StageAt(float Seconds)
	{
		return Seconds < TableEnd ? EStage::Table
			: Seconds < ShuffleEnd ? EStage::Shuffle
			: Seconds < DealEnd ? EStage::Deal
			: Seconds < FlipEnd ? EStage::Flip
			: EStage::Role;
	}

	inline const TCHAR* StageName(EStage Stage)
	{
		switch (Stage)
		{
		case EStage::Table: return TEXT("table");
		case EStage::Shuffle: return TEXT("shuffle");
		case EStage::Deal: return TEXT("deal");
		case EStage::Flip: return TEXT("flip");
		default: return TEXT("role");
		}
	}
}

/** One fellow Impatient as the reveal shows it. */
struct FKGRevealMateView
{
	FString Name;   // already streamer-masked
	FName RoleId;
};

/** Everything the reveal widget draws; built each frame from the live match or from sample data (kg.UIShot). */
struct FKGRevealView
{
	/** Own role (None until the owner-only replication arrives: the deck keeps shuffling). */
	FName RoleId;
	/** Seats at the table (players in the match, bots included). */
	int32 Players = 6;
	/** Fellow Impatient (owner-only data). */
	TArray<FKGRevealMateView> Mates;
	bool bHasTeam = false;
	bool bReady = false;
	int32 ReadyCount = 0;
	int32 HumanCount = 1;
	/** Server clock: seconds since the phase began / seconds left (negative = unknown, e.g. still in the lobby). */
	float ServerElapsed = -1.0f;
	float ServerRemaining = -1.0f;
	/** The phase has moved on: play the fade-out. */
	bool bEnding = false;
	bool bStreamer = false;
	FString PeekKey;
};
