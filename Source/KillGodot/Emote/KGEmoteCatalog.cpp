#include "Emote/KGEmoteCatalog.h"
#include "Core/KGPlayerState.h"
#include "Core/KGRng.h"

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

// ---------------------------------------------------------------------------------------------------------------
// Partner emotes
// ---------------------------------------------------------------------------------------------------------------

const TArray<FKGPartnerEmoteDef>& FKGPartnerCatalog::GetAll()
{
	static const TArray<FKGPartnerEmoteDef> All = []()
	{
		TArray<FKGPartnerEmoteDef> Out;
		auto Add = [&Out](EKGPartnerKind Kind, const TCHAR* Id, FText Name, const TCHAR* A, const TCHAR* B, float Dist, float Play,
		                  float Result, const TCHAR* Emoji)
		{
			FKGPartnerEmoteDef& D = Out.AddDefaulted_GetRef();
			D.Kind = Kind;
			D.Id = FName(Id);
			D.DisplayName = Name;
			D.EmoteA = FName(A);
			D.EmoteB = FName(B);
			D.Distance = Dist;
			D.PlaySeconds = Play;
			D.ResultSeconds = Result;
			D.Emoji = Emoji;
		};
		// Placeholder clips from the shipped emote set (dedicated partner clips are a proposal, see the sprint report).
		Add(EKGPartnerKind::HighFive, TEXT("highfive"), LOCTEXT("HighFive", "High five"), TEXT("cheer"), TEXT("cheer"), 95.0f, 2.2f, 0.0f, TEXT("clap"));
		Add(EKGPartnerKind::Handshake, TEXT("handshake"), LOCTEXT("Handshake", "Handshake"), TEXT("point"), TEXT("point"), 85.0f, 2.2f, 0.0f, TEXT("heart"));
		Add(EKGPartnerKind::RockPaperScissors, TEXT("rps"), LOCTEXT("Rps", "Rock, paper, scissors"), TEXT("clap"), TEXT("clap"), 110.0f, 2.4f, 3.0f, TEXT("question"));
		Add(EKGPartnerKind::DanceOff, TEXT("danceoff"), LOCTEXT("DanceOff", "Dance-off"), TEXT("dance"), TEXT("dance"), 160.0f, 6.0f, 3.0f, TEXT("fire"));
		return Out;
	}();
	return All;
}

const FKGPartnerEmoteDef* FKGPartnerCatalog::Find(FName IdOrAlias)
{
	const FString Word = IdOrAlias.ToString().ToLower().Replace(TEXT("-"), TEXT("")).Replace(TEXT("_"), TEXT(""));
	for (const FKGPartnerEmoteDef& D : GetAll())
	{
		if (D.Id.ToString() == Word)
		{
			return &D;
		}
	}
	if (Word == TEXT("rockpaperscissors") || Word == TEXT("roshambo"))
	{
		return Get(EKGPartnerKind::RockPaperScissors);
	}
	if (Word == TEXT("shake"))
	{
		return Get(EKGPartnerKind::Handshake);
	}
	if (Word == TEXT("five") || Word == TEXT("hi5"))
	{
		return Get(EKGPartnerKind::HighFive);
	}
	if (Word == TEXT("dancebattle"))
	{
		return Get(EKGPartnerKind::DanceOff);
	}
	return nullptr;
}

const FKGPartnerEmoteDef* FKGPartnerCatalog::Get(EKGPartnerKind Kind)
{
	for (const FKGPartnerEmoteDef& D : GetAll())
	{
		if (D.Kind == Kind)
		{
			return &D;
		}
	}
	return nullptr;
}

int32 FKGPartnerRules::RpsWinner(uint8 A, uint8 B)
{
	if (A == B)
	{
		return 0;
	}
	// rock(0) beats scissors(2), paper(1) beats rock(0), scissors(2) beats paper(1)
	return ((A + 1) % 3 == B) ? 2 : 1;
}

const TCHAR* FKGPartnerRules::RpsName(uint8 Pick)
{
	switch (Pick % 3)
	{
	case 0: return TEXT("rock");
	case 1: return TEXT("paper");
	default: return TEXT("scissors");
	}
}

uint8 FKGPartnerRules::PackRps(uint8 A, uint8 B)
{
	return static_cast<uint8>((A % 3) | ((B % 3) << 2) | (static_cast<uint8>(RpsWinner(A % 3, B % 3)) << 4));
}

void FKGPartnerRules::UnpackRps(uint8 Packed, uint8& A, uint8& B, uint8& Winner)
{
	A = Packed & 3;
	B = (Packed >> 2) & 3;
	Winner = (Packed >> 4) & 3;
}

uint8 FKGPartnerRules::RollRps(uint64 Seed, uint8 Serial)
{
	FKGRng Rng(Seed ^ (static_cast<uint64>(Serial) * 0x9E3779B97F4A7C15ull), 23u);
	const uint8 A = static_cast<uint8>(Rng.RandRange(0, 2));
	const uint8 B = static_cast<uint8>(Rng.RandRange(0, 2));
	return PackRps(A, B);
}

uint8 FKGPartnerRules::RollDanceOff(uint64 Seed, uint8 Serial)
{
	FKGRng Rng(Seed ^ (static_cast<uint64>(Serial) * 0xC2B2AE3D27D4EB4Full), 29u);
	return static_cast<uint8>(Rng.RandRange(1, 2));
}

bool FKGPartnerRules::WithinAcceptRadius(const FVector& Offerer, const FVector& Acceptor)
{
	return FVector::DistSquared2D(Offerer, Acceptor) <= FMath::Square(AcceptRadius) && FMath::Abs(Offerer.Z - Acceptor.Z) < 150.0f;
}

void FKGPartnerRules::Align(const FVector& OffererLoc, const FVector& AcceptorLoc, float Distance, FVector& OutAcceptorLoc,
                            float& OutOffererYaw, float& OutAcceptorYaw)
{
	FVector Dir = (AcceptorLoc - OffererLoc);
	Dir.Z = 0.0;
	if (!Dir.Normalize())
	{
		Dir = FVector::ForwardVector;
	}
	OutAcceptorLoc = FVector(OffererLoc.X + Dir.X * Distance, OffererLoc.Y + Dir.Y * Distance, AcceptorLoc.Z);
	OutOffererYaw = static_cast<float>(Dir.Rotation().Yaw);
	OutAcceptorYaw = static_cast<float>((-Dir).Rotation().Yaw);
}

bool FKGPartnerRules::StopEndsPartner(EKGEmoteStop Reason)
{
	switch (Reason)
	{
	case EKGEmoteStop::Moved:
	case EKGEmoteStop::Attacked:
	case EKGEmoteStop::Damaged:
	case EKGEmoteStop::Died:
	case EKGEmoteStop::Phase:
	case EKGEmoteStop::Busy:
	case EKGEmoteStop::Requested:
		return true;
	default:
		return false;   // Finished / Replaced: the clip ran out or the partner emote swapped it, the pairing goes on
	}
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
