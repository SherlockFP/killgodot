#include "Emote/KGEmoteCatalog.h"
#include "Core/KGPlayerState.h"

#define LOCTEXT_NAMESPACE "KGEmote"

namespace KGEmoteCatalogPrivate
{
	const TCHAR* Emotes3P = TEXT("/Game/KillGodot/Characters/Villager/Anims/Emotes");
	const TCHAR* Anims3P = TEXT("/Game/KillGodot/Characters/Villager/Anims");
	const TCHAR* AnimsFP = TEXT("/Game/KillGodot/Characters/FPArms2/Anims");

	FSoftObjectPath Asset(const TCHAR* Folder, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name));
	}

	/** Authored clip (Tools/Blender/kg_make_emotes.py -> Tools/Unreal/kg_import_emotes.py). */
	FSoftObjectPath Emote(const TCHAR* Name)
	{
		return Asset(Emotes3P, *FString::Printf(TEXT("A_KG_Emote_%s"), Name));
	}

	/** First-person gesture (Tools/Blender/kg_make_fp_arms2.py emote_* clips). */
	FSoftObjectPath Gesture(const TCHAR* Name)
	{
		return Asset(AnimsFP, *FString::Printf(TEXT("A_FP2_emote_%s"), Name));
	}

	struct FRow
	{
		const TCHAR* Id;
		FText Name;
		const TCHAR* Emoji;
		const TCHAR* Verb;
		const TCHAR* Aliases;
		FSoftObjectPath Clip;
		FSoftObjectPath Intro;
		FSoftObjectPath FirstPerson;
		EKGEmoteLayer Layer;
		bool bLoop;
		float MaxSeconds;
	};

	TArray<FKGEmoteDef> Build()
	{
		constexpr EKGEmoteLayer Full = EKGEmoteLayer::FullBody;
		constexpr EKGEmoteLayer Upper = EKGEmoteLayer::UpperBody;
		const FSoftObjectPath None;
		// Wheel order: clockwise from the top of the emote ring. Emoji ids are FKGEmoji ids.
		const FRow Rows[] = {
			{TEXT("wave"), LOCTEXT("Wave", "Wave"), TEXT("wave"), TEXT("waves"), TEXT("hi,hello,bye,hey"),
			 Emote(TEXT("Wave")), None, Gesture(TEXT("wave")), Upper, false, 0.0f},
			{TEXT("point"), LOCTEXT("Point", "Point"), TEXT("eyes"), TEXT("points"), TEXT("look,there"),
			 Emote(TEXT("Point")), None, Gesture(TEXT("point")), Upper, false, 0.0f},
			{TEXT("accuse"), LOCTEXT("Accuse", "Accuse!"), TEXT("exclamation"), TEXT("points accusingly and shouts"),
			 TEXT("blame,jaccuse,itwasyou"), Emote(TEXT("Accuse")), None, Gesture(TEXT("point")), Upper, false, 0.0f},
			{TEXT("clap"), LOCTEXT("Clap", "Clap"), TEXT("clap"), TEXT("claps"), TEXT("applause,gg,bravo"),
			 Emote(TEXT("Clap")), None, Gesture(TEXT("clap")), Upper, false, 0.0f},
			{TEXT("cheer"), LOCTEXT("Cheer", "Cheer"), TEXT("mug"), TEXT("cheers"), TEXT("cheers,yay,woo,hooray"),
			 Emote(TEXT("Cheer")), None, None, Upper, false, 0.0f},
			{TEXT("laugh"), LOCTEXT("Laugh", "Laugh"), TEXT("laugh"), TEXT("laughs"), TEXT("lol,haha,lmao"),
			 Emote(TEXT("Laugh")), None, None, Full, false, 0.0f},
			{TEXT("dance"), LOCTEXT("Dance", "Dance"), TEXT("fire"), TEXT("dances"), TEXT("dance1,boogie,groove"),
			 Asset(Anims3P, TEXT("A_KG_Dance_Loop")), None, None, Full, true, 0.0f},
			{TEXT("reel"), LOCTEXT("Reel", "Reel It In"), TEXT("fish"), TEXT("reels in an invisible fish"),
			 TEXT("dance2,reelitin,fishdance"), Emote(TEXT("Reel")), None, None, Full, true, 0.0f},
			{TEXT("sit"), LOCTEXT("Sit", "Sit"), TEXT("anchor"), TEXT("sits down"), TEXT("sleep,rest,chill"),
			 Emote(TEXT("Sit_Loop")), Emote(TEXT("Sit_Enter")), None, Full, true, 0.0f},
			{TEXT("bow"), LOCTEXT("Bow", "Bow"), TEXT("crown"), TEXT("bows"), TEXT("pray,thanks,curtsy"),
			 Emote(TEXT("Bow")), None, None, Full, false, 0.0f},
			{TEXT("salute"), LOCTEXT("Salute", "Salute"), TEXT("thumbsup"), TEXT("salutes"), TEXT("o7,respect,aye"),
			 Emote(TEXT("Salute")), None, Gesture(TEXT("salute")), Upper, false, 0.0f},
			{TEXT("shrug"), LOCTEXT("Shrug", "Shrug"), TEXT("question"), TEXT("shrugs"), TEXT("idk,dunno,whatever"),
			 Emote(TEXT("Shrug")), None, None, Upper, false, 0.0f},
			{TEXT("facepalm"), LOCTEXT("Facepalm", "Facepalm"), TEXT("sweat"), TEXT("facepalms"), TEXT("fp,ugh,smh"),
			 Emote(TEXT("Facepalm")), None, None, Upper, false, 0.0f},
			{TEXT("sus"), LOCTEXT("Sus", "Sus"), TEXT("sus"), TEXT("folds their arms, unconvinced"),
			 TEXT("think,hmm,thinking,suspicious"), Emote(TEXT("Sus")), None, None, Upper, true, 8.0f},
			{TEXT("cry"), LOCTEXT("Cry", "Cry"), TEXT("cry"), TEXT("cries"), TEXT("sob,sad,weep"),
			 Emote(TEXT("Cry")), None, None, Full, false, 0.0f},
			// Impatient-flavoured but open to everyone, so using it reveals nothing.
			{TEXT("threaten"), LOCTEXT("Threaten", "Threaten"), TEXT("knife"), TEXT("draws a thumb across their throat"),
			 TEXT("angry,threat,yourenext"), Emote(TEXT("Threaten")), None, None, Upper, false, 0.0f},
		};
		TArray<FKGEmoteDef> Out;
		for (const FRow& Row : Rows)
		{
			FKGEmoteDef& Def = Out.AddDefaulted_GetRef();
			Def.Id = FName(Row.Id);
			Def.DisplayName = Row.Name;
			Def.Emoji = Row.Emoji;
			Def.Verb = Row.Verb;
			Def.Aliases = Row.Aliases;
			Def.Clip = Row.Clip;
			Def.IntroClip = Row.Intro;
			Def.FirstPersonClip = Row.FirstPerson;
			Def.Layer = Row.Layer;
			Def.bLoop = Row.bLoop;
			Def.MaxSeconds = Row.MaxSeconds;
			Def.BlendIn = Row.Layer == Full ? 0.3f : 0.2f;
			Def.BlendOut = Row.Layer == Full ? 0.35f : 0.3f;
		}
		return Out;
	}
}

bool FKGEmoteDef::MatchesWord(const FString& Word) const
{
	if (Word.Equals(Id.ToString(), ESearchCase::IgnoreCase))
	{
		return true;
	}
	TArray<FString> Parts;
	Aliases.ParseIntoArray(Parts, TEXT(","));
	for (const FString& Part : Parts)
	{
		if (Word.Equals(Part.TrimStartAndEnd(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

const TArray<FKGEmoteDef>& FKGEmoteCatalog::GetAll()
{
	static const TArray<FKGEmoteDef> All = KGEmoteCatalogPrivate::Build();
	return All;
}

int32 FKGEmoteCatalog::IndexOf(FName IdOrAlias)
{
	if (IdOrAlias.IsNone())
	{
		return INDEX_NONE;
	}
	const TArray<FKGEmoteDef>& All = GetAll();
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].Id == IdOrAlias)
		{
			return i;
		}
	}
	const FString Word = IdOrAlias.ToString();
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].MatchesWord(Word))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

const FKGEmoteDef* FKGEmoteCatalog::Find(FName IdOrAlias)
{
	return Get(IndexOf(IdOrAlias));
}

const FKGEmoteDef* FKGEmoteCatalog::Find(const FString& IdOrAlias)
{
	const FString Word = IdOrAlias.TrimStartAndEnd();
	return Word.IsEmpty() ? nullptr : Find(FName(*Word));
}

const FKGEmoteDef* FKGEmoteCatalog::Get(int32 Index)
{
	const TArray<FKGEmoteDef>& All = GetAll();
	return All.IsValidIndex(Index) ? &All[Index] : nullptr;
}

TArray<FSoftObjectPath> FKGEmoteCatalog::GetAllClipPaths()
{
	TArray<FSoftObjectPath> Out;
	for (const FKGEmoteDef& Def : GetAll())
	{
		for (const FSoftObjectPath& Path : {Def.Clip, Def.IntroClip, Def.FirstPersonClip})
		{
			if (!Path.IsNull())
			{
				Out.AddUnique(Path);
			}
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------------------------

EKGEmoteReject FKGEmoteRules::CanStart(const FKGEmoteDef& Emote, const FKGChatParticipant& Who,
                                       const FKGEmoteBodyState& Body, EKGPhase Phase, float PhaseRemaining)
{
	if (Who.Life == EKGLifeState::Ghost || Body.bDead)
	{
		return EKGEmoteReject::Dead;
	}
	if (!FKGChatRules::CanReact(Who, Phase, PhaseRemaining))
	{
		return EKGEmoteReject::Phase;
	}
	if (!Body.bHasBody)
	{
		return EKGEmoteReject::NoBody;
	}
	if (Body.bSwimming || Body.bClimbing || Body.bSeated || Body.bCarrying ||
	    (Emote.IsFullBody() && (Body.bFalling || Body.bCrouched)))
	{
		return EKGEmoteReject::Busy;
	}
	if (Emote.IsFullBody() && Body.Speed > MoveCancelSpeed)
	{
		return EKGEmoteReject::Moving;
	}
	return EKGEmoteReject::None;
}

bool FKGEmoteRules::ShouldStop(const FKGEmoteDef& Emote, const FKGChatParticipant& Who, const FKGEmoteBodyState& Body,
                               EKGPhase Phase, float PhaseRemaining, EKGEmoteStop& OutReason)
{
	if (Who.Life == EKGLifeState::Ghost || Body.bDead || !Body.bHasBody)
	{
		OutReason = EKGEmoteStop::Died;
		return true;
	}
	if (!FKGChatRules::CanReact(Who, Phase, PhaseRemaining))
	{
		OutReason = EKGEmoteStop::Phase;
		return true;
	}
	if (Body.bSwimming || Body.bClimbing || Body.bSeated || Body.bCarrying)
	{
		OutReason = EKGEmoteStop::Busy;
		return true;
	}
	if (Emote.IsFullBody() && (Body.Speed > MoveCancelSpeed || Body.bFalling || Body.bCrouched))
	{
		OutReason = EKGEmoteStop::Moved;
		return true;
	}
	return false;
}

bool FKGEmoteRules::PhaseChangeStops(EKGPhase OldPhase, EKGPhase NewPhase)
{
	if (OldPhase == NewPhase)
	{
		return false;
	}
	switch (NewPhase)
	{
	case EKGPhase::Meeting:
	case EKGPhase::Trial:
	case EKGPhase::Night:
	case EKGPhase::RoleReveal:
	case EKGPhase::Migrating:
		return true;
	default:
		return false;
	}
}

FKGChatRateLimiter FKGEmoteRules::MakeLimiter()
{
	FKGChatRateLimiter Limiter;
	Limiter.Burst = 3.0f;
	Limiter.RefillPerSecond = 0.5f;
	Limiter.DuplicateWindow = 0.0f;
	return Limiter;
}

EKGChatReject FKGEmoteRules::ToChatReject(EKGEmoteReject Reject)
{
	switch (Reject)
	{
	case EKGEmoteReject::None: return EKGChatReject::None;
	case EKGEmoteReject::Phase: return EKGChatReject::Closed;
	case EKGEmoteReject::Moving: return EKGChatReject::EmoteMoving;
	default: return EKGChatReject::EmoteBlocked;
	}
}

#undef LOCTEXT_NAMESPACE
