#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Core/KGTypes.h"

/** Which voice room a speaker is in right now (HUD label; the routing itself is HearGain). */
enum class EKGVoiceChannel : uint8
{
	/** Living voice carried by distance (full <= 8 m, silent at 25 m). */
	Nearby,
	/** Meeting / trial: the whole square hears everyone alive. */
	Square,
	/** Ghosts talk to ghosts anywhere; the living never receive it. */
	Ghost,
	/** Cannot speak (revenant, migration, role reveal curfew). */
	Closed
};

/**
 * SPRINT-023 proximity voice rules. Pure (automation tests KillGodot.Voice.*), applied by the listen server per
 * packet per receiver: a receiver that may not hear a packet never gets it on the wire (ghost voice never reaches a
 * living machine, so it cannot be un-muted client side). The same rules run under EOS RTC later as per-participant
 * volumes (Docs/05_Tech_Architecture.md §5): only the transport changes, see UKGVoiceComponent.
 */
struct KILLGODOT_API FKGVoiceRules
{
	/** Full volume up to here (8 m). */
	static constexpr float FullRadius = 800.0f;
	/** Silent from here (25 m). */
	static constexpr float SilentRadius = 2500.0f;

	/** 1 inside FullRadius, smooth fall-off to 0 at SilentRadius. */
	static float Attenuation(float DistanceCm);

	static bool IsMeeting(EKGPhase Phase) { return Phase == EKGPhase::Meeting || Phase == EKGPhase::Trial; }

	/** May Speaker transmit at all right now? Ghosts always may (ghost room); revenants never; nobody mid-migration
	 *  or during the role reveal curfew (a whisper would give the deal away). */
	static bool CanSpeak(const FKGChatParticipant& Speaker, EKGPhase Phase);

	static EKGVoiceChannel ChannelOf(const FKGChatParticipant& Speaker, EKGPhase Phase);

	/**
	 * Gain (0..1) at which Listener hears one packet from Speaker; 0 = not delivered. Never yourself.
	 *  - ghost speaker: ghosts only, full volume anywhere;
	 *  - living speaker, ghost listener: full volume (spectators hear the living, like Nearby chat);
	 *  - living <-> living: full during meetings/trials (everyone at the square), else Attenuation(distance);
	 *    without a known position on either side: silent.
	 */
	static float HearGain(const FKGChatParticipant& Speaker, const FKGChatParticipant& Listener, EKGPhase Phase,
	                      bool bSelf);

	/** Gain quantised for the wire (uint8) and back. */
	static uint8 QuantizeGain(float Gain) { return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Gain, 0.0f, 1.0f) * 255.0f)); }
	static float DequantizeGain(uint8 Q) { return static_cast<float>(Q) / 255.0f; }

	/** Linear RMS of 16-bit PCM (0..1). */
	static float Amplitude(const int16* Samples, int32 Num);
};
