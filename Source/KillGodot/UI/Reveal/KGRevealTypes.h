#pragma once

#include "CoreMinimal.h"

/**
 * Role reveal "moment" timeline (SPRINT-037, Docs/08_UI_UX.md "Rol töreni"): Among Us splash energy, readable in two
 * seconds. The server owns the length (the RoleReveal phase runs on AKGGameState::Clock, an FKGMatchClock); clients
 * follow the replicated clock and play these beats on top of it.
 */
namespace KGReveal
{
	/** RoleReveal phase length (AKGGameMode::GetPhaseDuration). Contract: 5-7 s. */
	inline constexpr float PhaseSeconds = 6.0f;

	// Beats, seconds since the phase began.
	inline constexpr float SpinEnd = 0.7f;     // 0.0-0.7  dark room, your card spins up out of the dark (riser)
	inline constexpr float TeaseEnd = 1.1f;    // 0.7-1.1  "WHO ARE YOU?" - the card trembles, light builds
	inline constexpr float BangAt = 1.2f;      //          the face comes round: flash, colour flood, punch, sparks, sting
	inline constexpr float FlipEnd = 1.3f;     // 1.1-1.3  the card flips (overshoots and settles until ~1.7)
	inline constexpr float NameAt = 1.25f;     // 1.25     the role name slams down
	inline constexpr float BannerAt = 1.4f;    // 1.4      the alignment banner sweeps across
	inline constexpr float LineAt = 1.7f;      // 1.7      the one "what you do" line
	inline constexpr float MatesAt = 1.9f;     // 1.9      accomplices step up beside you (Impatient teams)
	inline constexpr float ReadEnd = 2.4f;     //          everything is on screen

	/** "Ready" is accepted once the name is readable (server check allows a little clock skew). */
	inline constexpr float ReadyFromSeconds = 1.6f;
	inline constexpr float ServerReadyTolerance = 0.6f;
	/** When every human is ready the server cuts the phase down to this (the fade-out). */
	inline constexpr float SkipToSeconds = 0.6f;
	/** Client fade-out after the phase ends (the village appears behind it). */
	inline constexpr float OutroSeconds = 0.5f;

	/** Named stages (screenshots, logs). */
	enum class EStage : uint8
	{
		Spin,
		Tease,
		Flip,
		Role
	};

	inline EStage StageAt(float Seconds)
	{
		return Seconds < SpinEnd ? EStage::Spin
			: Seconds < TeaseEnd ? EStage::Tease
			: Seconds < FlipEnd ? EStage::Flip
			: EStage::Role;
	}

	inline const TCHAR* StageName(EStage Stage)
	{
		switch (Stage)
		{
		case EStage::Spin: return TEXT("spin");
		case EStage::Tease: return TEXT("tease");
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
	/** Tab held: the secondary details view (goal, abilities, flavour) over the moment. */
	bool bDetails = false;
};
