#include "Chat/KGChatRules.h"
#include "Chat/KGEmoji.h"
#include "Misc/Crc.h"

#define LOCTEXT_NAMESPACE "KGChat"

namespace KGChatRulesPrivate
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	bool IsInvisibleFormatChar(TCHAR C)
	{
		return (C >= 0x200B && C <= 0x200F)      // zero-width space/joiners, LRM/RLM
		    || (C >= 0x202A && C <= 0x202E)      // bidi embeddings / overrides
		    || (C >= 0x2060 && C <= 0x2069)      // word joiner, invisible operators, bidi isolates
		    || C == 0xFEFF || C == 0x00AD || C == 0x061C || C == 0x180E;
	}

	bool IsCombiningMark(TCHAR C)
	{
		return (C >= 0x0300 && C <= 0x036F) || (C >= 0x1AB0 && C <= 0x1AFF) || (C >= 0x1DC0 && C <= 0x1DFF) ||
		       (C >= 0x20D0 && C <= 0x20FF) || (C >= 0xFE20 && C <= 0xFE2F);
	}

	FString CollapseWhitespace(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		bool bPendingSpace = false;
		for (const TCHAR C : In)
		{
			if (FChar::IsWhitespace(C) || C == 0x00A0 || C == 0x3000)
			{
				bPendingSpace = !Out.IsEmpty();
				continue;
			}
			if (bPendingSpace)
			{
				Out.AppendChar(TEXT(' '));
				bPendingSpace = false;
			}
			Out.AppendChar(C);
		}
		return Out;
	}
}

EKGChatReject FKGChatRateLimiter::TryConsume(double Now, const FString& Text)
{
	if (Tokens < 0.0f)
	{
		Tokens = Burst;
		LastTime = Now;
	}
	Tokens = FMath::Min(Burst, Tokens + static_cast<float>(FMath::Max(0.0, Now - LastTime)) * RefillPerSecond);
	LastTime = Now;
	if (!Text.IsEmpty() && Text.Equals(LastText, ESearchCase::IgnoreCase) && Now - LastTextTime < DuplicateWindow)
	{
		return EKGChatReject::Duplicate;
	}
	if (Tokens < 1.0f)
	{
		return EKGChatReject::RateLimited;
	}
	Tokens -= 1.0f;
	LastText = Text;
	LastTextTime = Now;
	return EKGChatReject::None;
}

EKGTrialStage FKGChatRules::GetTrialStage(float PhaseRemaining)
{
	if (PhaseRemaining <= 8.0f)
	{
		return EKGTrialStage::LastWords;
	}
	if (PhaseRemaining <= 8.0f + 15.0f)
	{
		return EKGTrialStage::Judgement;
	}
	return EKGTrialStage::Defense;
}

bool FKGChatRules::IsTeamFaction(EKGFaction Faction)
{
	return Faction == EKGFaction::Clockbreakers || Faction == EKGFaction::Vampires || Faction == EKGFaction::Plague;
}

EKGChatReject FKGChatRules::CanSend(const FKGChatParticipant& Sender, EKGChatChannel Channel, EKGPhase Phase,
                                    float PhaseRemaining)
{
	if (Channel == EKGChatChannel::System || Channel >= EKGChatChannel::MAX || Phase == EKGPhase::Migrating)
	{
		return EKGChatReject::Closed;
	}
	const bool bEpilogue = Phase == EKGPhase::Epilogue;
	if (Sender.Life == EKGLifeState::Revenant)
	{
		return bEpilogue && Channel == EKGChatChannel::All ? EKGChatReject::None : EKGChatReject::Revenant;
	}
	if (Sender.Life == EKGLifeState::Ghost)
	{
		if (Channel == EKGChatChannel::Dead || (bEpilogue && Channel == EKGChatChannel::All))
		{
			return EKGChatReject::None;
		}
		return EKGChatReject::WrongLifeState;
	}
	// Alive from here on.
	if (Channel == EKGChatChannel::Dead)
	{
		return EKGChatReject::WrongLifeState;
	}
	if (Sender.bSilenced && !bEpilogue)
	{
		return EKGChatReject::Silenced;
	}
	switch (Channel)
	{
	case EKGChatChannel::All:
		switch (Phase)
		{
		case EKGPhase::Lobby:
		case EKGPhase::Warmup:
		case EKGPhase::Day:
		case EKGPhase::Meeting:
		case EKGPhase::Epilogue:
			return EKGChatReject::None;
		case EKGPhase::Trial:
			return GetTrialStage(PhaseRemaining) == EKGTrialStage::Judgement || Sender.bOnTrial
				       ? EKGChatReject::None
				       : EKGChatReject::AccusedSpeaking;
		default:
			return EKGChatReject::Closed;   // RoleReveal, Dawn (the crier speaks), Night (curfew)
		}
	case EKGChatChannel::Nearby:
		switch (Phase)
		{
		case EKGPhase::Lobby:
		case EKGPhase::Warmup:
		case EKGPhase::Dawn:
		case EKGPhase::Day:
		case EKGPhase::Epilogue:
			return EKGChatReject::None;
		default:
			return EKGChatReject::Closed;
		}
	case EKGChatChannel::Team:
		if (!Sender.bHasRole || !IsTeamFaction(Sender.Faction))
		{
			return EKGChatReject::NoTeam;
		}
		return Phase == EKGPhase::RoleReveal || Phase == EKGPhase::Night ? EKGChatReject::None : EKGChatReject::Closed;
	default:
		return EKGChatReject::Closed;
	}
}

bool FKGChatRules::CanReceive(const FKGChatParticipant& Sender, const FKGChatParticipant& Receiver,
                              EKGChatChannel Channel, bool bSelf)
{
	if (bSelf)
	{
		return true;
	}
	switch (Channel)
	{
	case EKGChatChannel::All:
	case EKGChatChannel::System:
		return true;   // the dead read the town square too (Docs/01_GDD_Core.md §11)
	case EKGChatChannel::Nearby:
		if (Receiver.Life == EKGLifeState::Ghost)
		{
			return true;   // spectators hear the living (never the other way round)
		}
		return Sender.bHasLocation && Receiver.bHasLocation &&
		       FVector::DistSquared(Sender.Location, Receiver.Location) <= FMath::Square(NearbyRadius);
	case EKGChatChannel::Team:
		return Receiver.Life == EKGLifeState::Alive && Receiver.bHasRole && Sender.bHasRole &&
		       Receiver.Faction == Sender.Faction && IsTeamFaction(Receiver.Faction);
	case EKGChatChannel::Dead:
		return Receiver.Life == EKGLifeState::Ghost;
	default:
		return false;
	}
}

TArray<EKGChatChannel> FKGChatRules::GetSendableChannels(const FKGChatParticipant& Sender, EKGPhase Phase,
                                                         float PhaseRemaining)
{
	TArray<EKGChatChannel> Out;
	for (const EKGChatChannel Channel : {EKGChatChannel::All, EKGChatChannel::Nearby, EKGChatChannel::Team,
	                                     EKGChatChannel::Dead})
	{
		if (CanSend(Sender, Channel, Phase, PhaseRemaining) == EKGChatReject::None)
		{
			Out.Add(Channel);
		}
	}
	return Out;
}

bool FKGChatRules::CanReact(const FKGChatParticipant& Sender, EKGPhase Phase, float PhaseRemaining)
{
	if (Phase == EKGPhase::Migrating || Sender.Life == EKGLifeState::Revenant)
	{
		return false;
	}
	if (Sender.Life == EKGLifeState::Ghost)
	{
		return true;   // becomes a Dead-chat line
	}
	switch (Phase)
	{
	case EKGPhase::RoleReveal:
	case EKGPhase::Night:
		return false;   // curfew: a bubble would give you away
	default:
		return true;    // incl. blackmailed players and the crowd during a defence
	}
}

bool FKGChatRules::CanSeeReaction(const FKGChatParticipant& Sender, const FKGChatParticipant& Receiver, bool bSelf)
{
	if (bSelf)
	{
		return true;
	}
	if (Sender.Life == EKGLifeState::Ghost)
	{
		return Receiver.Life == EKGLifeState::Ghost;
	}
	if (Receiver.Life == EKGLifeState::Ghost)
	{
		return true;
	}
	return Sender.bHasLocation && Receiver.bHasLocation &&
	       FVector::DistSquared(Sender.Location, Receiver.Location) <= FMath::Square(ReactionRadius);
}

FString FKGChatRules::Sanitize(const FString& Raw)
{
	using namespace KGChatRulesPrivate;
	// Hard cap before any work (a malicious client can send megabytes in one reliable RPC).
	const FString Clipped = Raw.Left(MaxChars * 8);
	const FString Mapped = FKGEmoji::ConvertUnicode(Clipped);

	FString Filtered;
	Filtered.Reserve(Mapped.Len());
	bool bPrevCombining = false;
	for (const TCHAR C : Mapped)
	{
		if (C == TEXT('\t') || C == TEXT('\n') || C == TEXT('\r'))
		{
			Filtered.AppendChar(TEXT(' '));
			bPrevCombining = false;
			continue;
		}
		if (C < 0x20 || (C >= 0x7F && C <= 0x9F) || IsInvisibleFormatChar(C) || (C >= 0xD800 && C <= 0xDFFF))
		{
			continue;
		}
		if (C >= 0xE000 && C <= 0xF8FF && !FKGEmoji::IsEmojiChar(C))
		{
			continue;   // private use outside our emoji range
		}
		const bool bCombining = IsCombiningMark(C);
		if (bCombining && (bPrevCombining || Filtered.IsEmpty()))
		{
			continue;   // "zalgo" stacks: at most one mark per letter
		}
		bPrevCombining = bCombining;
		Filtered.AppendChar(C);
	}

	FString Text = FKGEmoji::ConvertEmoticons(FKGEmoji::ConvertShortcodes(CollapseWhitespace(Filtered)));

	// Repeats and emoji cap.
	FString Capped;
	Capped.Reserve(Text.Len());
	int32 Run = 0;
	int32 Emojis = 0;
	TCHAR Prev = 0;
	for (const TCHAR C : Text)
	{
		Run = C == Prev ? Run + 1 : 1;
		Prev = C;
		if (Run > MaxRepeat)
		{
			continue;
		}
		if (FKGEmoji::IsEmojiChar(C) && ++Emojis > MaxEmojis)
		{
			continue;
		}
		Capped.AppendChar(C);
	}
	return CollapseWhitespace(Capped).Left(MaxChars).TrimStartAndEnd();
}

int32 FKGChatRules::MakeColorSeed(const FString& Puid, const FString& PlayerName)
{
	const FString& Key = Puid.IsEmpty() ? PlayerName : Puid;
	return static_cast<int32>(FCrc::StrCrc32(*Key) & 0x7FFFFFFF);
}

FLinearColor FKGChatRules::NameColor(int32 ColorSeed)
{
	using namespace KGChatRulesPrivate;
	// Bright, readable on the dark ink panel. Deliberately no crimson (Impatient UI) and no ghost cyan (Dead chat),
	// so a name colour never hints at a side.
	static const FLinearColor Palette[] = {
		Srgb(255, 184, 77),   // lantern gold
		Srgb(124, 217, 146),  // meadow green
		Srgb(110, 180, 255),  // harbour blue
		Srgb(255, 143, 177),  // dusk pink
		Srgb(195, 166, 255),  // lilac
		Srgb(255, 214, 102),  // sunflower
		Srgb(255, 159, 104),  // orange
		Srgb(184, 224, 74),   // lime
		Srgb(245, 163, 255),  // orchid
		Srgb(143, 184, 255),  // periwinkle
		Srgb(232, 201, 160),  // sand
		Srgb(255, 179, 138),  // peach
	};
	return Palette[static_cast<uint32>(ColorSeed) % UE_ARRAY_COUNT(Palette)];
}

FLinearColor FKGChatRules::ChannelColor(EKGChatChannel Channel)
{
	using namespace KGChatRulesPrivate;
	switch (Channel)
	{
	case EKGChatChannel::All: return Srgb(255, 184, 77);      // HUD gold
	case EKGChatChannel::Nearby: return Srgb(143, 208, 255);  // soft sky
	case EKGChatChannel::Team: return Srgb(255, 72, 94);      // Impatient crimson (text-safe)
	case EKGChatChannel::Dead: return Srgb(110, 225, 235);    // ghost cyan
	default: return Srgb(255, 217, 138);                      // crier parchment
	}
}

FText FKGChatRules::ChannelLabel(EKGChatChannel Channel)
{
	switch (Channel)
	{
	case EKGChatChannel::All: return LOCTEXT("ChanAll", "TOWN");
	case EKGChatChannel::Nearby: return LOCTEXT("ChanNearby", "NEAR");
	case EKGChatChannel::Team: return LOCTEXT("ChanTeam", "IMPATIENT");
	case EKGChatChannel::Dead: return LOCTEXT("ChanDead", "GHOSTS");
	default: return LOCTEXT("ChanSystem", "CRIER");
	}
}

FText FKGChatRules::RejectReason(EKGChatReject Reason)
{
	switch (Reason)
	{
	case EKGChatReject::Closed: return LOCTEXT("RejClosed", "That channel is closed right now.");
	case EKGChatReject::WrongLifeState: return LOCTEXT("RejLife", "The living and the dead cannot read each other.");
	case EKGChatReject::Silenced: return LOCTEXT("RejSilenced", "You have been silenced today. Only reactions work.");
	case EKGChatReject::Revenant: return LOCTEXT("RejRevenant", "Revenants cannot speak.");
	case EKGChatReject::AccusedSpeaking: return LOCTEXT("RejAccused", "Quiet! Only the accused may speak now.");
	case EKGChatReject::NoTeam: return LOCTEXT("RejNoTeam", "You have no secret channel.");
	case EKGChatReject::RateLimited: return LOCTEXT("RejRate", "Slow down, you are writing too fast.");
	case EKGChatReject::Duplicate: return LOCTEXT("RejDup", "You just said that.");
	case EKGChatReject::EmoteBlocked: return LOCTEXT("RejEmote", "You can't do that emote right now.");
	case EKGChatReject::EmoteMoving: return LOCTEXT("RejEmoteMoving", "Stand still for that emote.");
	default: return FText::GetEmpty();
	}
}

bool FKGChatRules::ParseChannelCommand(const FString& Word, EKGChatChannel& OutChannel)
{
	const FString W = Word.ToLower();
	if (W == TEXT("/all") || W == TEXT("/a") || W == TEXT("/town") || W == TEXT("/say") || W == TEXT("/s"))
	{
		OutChannel = EKGChatChannel::All;
		return true;
	}
	if (W == TEXT("/near") || W == TEXT("/n") || W == TEXT("/local") || W == TEXT("/l"))
	{
		OutChannel = EKGChatChannel::Nearby;
		return true;
	}
	if (W == TEXT("/team") || W == TEXT("/t") || W == TEXT("/imp"))
	{
		OutChannel = EKGChatChannel::Team;
		return true;
	}
	if (W == TEXT("/dead") || W == TEXT("/d") || W == TEXT("/ghost") || W == TEXT("/g"))
	{
		OutChannel = EKGChatChannel::Dead;
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
