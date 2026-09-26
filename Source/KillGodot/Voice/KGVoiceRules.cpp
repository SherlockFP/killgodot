#include "Voice/KGVoiceRules.h"

float FKGVoiceRules::Attenuation(float DistanceCm)
{
	if (DistanceCm <= FullRadius)
	{
		return 1.0f;
	}
	if (DistanceCm >= SilentRadius)
	{
		return 0.0f;
	}
	const float T = (DistanceCm - FullRadius) / (SilentRadius - FullRadius);
	return 1.0f - T;   // linear in distance: 16.5 m = half volume, easy to reason about in the smoke
}

bool FKGVoiceRules::CanSpeak(const FKGChatParticipant& Speaker, EKGPhase Phase)
{
	if (Phase == EKGPhase::Migrating || Speaker.Life == EKGLifeState::Revenant)
	{
		return false;
	}
	if (Speaker.Life == EKGLifeState::Ghost)
	{
		return true;
	}
	return Phase != EKGPhase::RoleReveal;
}

EKGVoiceChannel FKGVoiceRules::ChannelOf(const FKGChatParticipant& Speaker, EKGPhase Phase)
{
	if (!CanSpeak(Speaker, Phase))
	{
		return EKGVoiceChannel::Closed;
	}
	if (Speaker.Life == EKGLifeState::Ghost)
	{
		return EKGVoiceChannel::Ghost;
	}
	return IsMeeting(Phase) ? EKGVoiceChannel::Square : EKGVoiceChannel::Nearby;
}

float FKGVoiceRules::HearGain(const FKGChatParticipant& Speaker, const FKGChatParticipant& Listener, EKGPhase Phase,
                              bool bSelf)
{
	if (bSelf || !CanSpeak(Speaker, Phase))
	{
		return 0.0f;
	}
	if (Speaker.Life == EKGLifeState::Ghost)
	{
		return Listener.Life == EKGLifeState::Ghost ? 1.0f : 0.0f;
	}
	if (Listener.Life == EKGLifeState::Ghost)
	{
		return 1.0f;
	}
	if (IsMeeting(Phase))
	{
		return 1.0f;
	}
	if (!Speaker.bHasLocation || !Listener.bHasLocation)
	{
		return 0.0f;
	}
	return Attenuation(static_cast<float>(FVector::Dist(Speaker.Location, Listener.Location)));
}

float FKGVoiceRules::Amplitude(const int16* Samples, int32 Num)
{
	if (!Samples || Num <= 0)
	{
		return 0.0f;
	}
	double Sum = 0.0;
	for (int32 i = 0; i < Num; ++i)
	{
		const double S = static_cast<double>(Samples[i]) / 32768.0;
		Sum += S * S;
	}
	return static_cast<float>(FMath::Sqrt(Sum / Num));
}
