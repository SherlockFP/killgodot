#include "Voice/KGVoiceCommands.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "KGVoiceCommands"

namespace KGVoiceCommandsPrivate
{
	struct FRow
	{
		const TCHAR* Id;
		FText Line;
		const TCHAR* Gesture;
		uint8 Syllables;
		const TCHAR* Emoji;
	};

	TArray<FKGVoiceCommandDef> Build()
	{
		// Menu Z "Calls", menu X "Deduction", menu C "Social": 8 rows each, top slot first, clockwise.
		const FRow Rows[] = {
			// ---- Z: calls ----
			{TEXT("overhere"), LOCTEXT("OverHere", "Over here!"), TEXT("wave"), 3, TEXT("wave")},
			{TEXT("help"), LOCTEXT("Help", "Help!"), TEXT("cheer"), 1, TEXT("exclamation")},
			{TEXT("followme"), LOCTEXT("FollowMe", "Follow me"), TEXT("point"), 3, TEXT("eyes")},
			{TEXT("meeting"), LOCTEXT("Meeting", "Meeting at the fountain!"), TEXT("accuse"), 6, TEXT("bell")},
			{TEXT("whosthere"), LOCTEXT("WhosThere", "Who's there?"), TEXT("shrug"), 2, TEXT("question")},
			{TEXT("wait"), LOCTEXT("Wait", "Wait!"), TEXT("point"), 1, TEXT("clock")},
			{TEXT("gogogo"), LOCTEXT("GoGoGo", "Go, go, go!"), TEXT("cheer"), 3, TEXT("fire")},
			{TEXT("run"), LOCTEXT("Run", "Run!"), TEXT("cheer"), 1, TEXT("shock")},
			// ---- X: deduction ----
			{TEXT("sawsomething"), LOCTEXT("SawSomething", "I saw something!"), TEXT("point"), 4, TEXT("eyes")},
			{TEXT("suspicious"), LOCTEXT("Suspicious", "Suspicious..."), TEXT("sus"), 3, TEXT("sus")},
			{TEXT("iwasat"), LOCTEXT("IWasAt", "I was at the {place}"), TEXT("shrug"), 4, TEXT("question")},
			{TEXT("trustme"), LOCTEXT("TrustMe", "Trust me"), TEXT("salute"), 2, TEXT("heart")},
			{TEXT("notme"), LOCTEXT("NotMe", "It wasn't me!"), TEXT("shrug"), 4, TEXT("sweat")},
			{TEXT("votethem"), LOCTEXT("VoteThem", "Vote them!"), TEXT("accuse"), 2, TEXT("knife")},
			{TEXT("mychore"), LOCTEXT("MyChore", "I'm doing my chore"), TEXT("salute"), 5, TEXT("thumbsup")},
			{TEXT("watchthem"), LOCTEXT("WatchThem", "Watch them"), TEXT("point"), 2, TEXT("eyes")},
			// ---- C: social ----
			{TEXT("yes"), LOCTEXT("Yes", "Yes"), TEXT("salute"), 1, TEXT("thumbsup")},
			{TEXT("no"), LOCTEXT("No", "No"), TEXT("facepalm"), 1, TEXT("thumbsdown")},
			{TEXT("thanks"), LOCTEXT("Thanks", "Thanks!"), TEXT("bow"), 1, TEXT("heart")},
			{TEXT("sorry"), LOCTEXT("Sorry", "Sorry"), TEXT("bow"), 2, TEXT("sweat")},
			{TEXT("goodgame"), LOCTEXT("GoodGame", "Good game"), TEXT("clap"), 2, TEXT("clap")},
			{TEXT("nice"), LOCTEXT("Nice", "Nice!"), TEXT("cheer"), 1, TEXT("smile")},
			{TEXT("hello"), LOCTEXT("Hello", "Hello!"), TEXT("wave"), 2, TEXT("wave")},
			{TEXT("bye"), LOCTEXT("Bye", "Bye!"), TEXT("wave"), 1, TEXT("wave")},
		};
		TArray<FKGVoiceCommandDef> Out;
		for (const FRow& Row : Rows)
		{
			FKGVoiceCommandDef& Def = Out.AddDefaulted_GetRef();
			Def.Id = FName(Row.Id);
			Def.Line = Row.Line;
			Def.Gesture = Row.Gesture && *Row.Gesture ? FName(Row.Gesture) : NAME_None;
			Def.Syllables = Row.Syllables;
			Def.Emoji = Row.Emoji;
		}
		return Out;
	}

	TArray<FKGVoiceMenuDef> BuildMenus()
	{
		TArray<FKGVoiceMenuDef> Out;
		const FText Titles[] = {LOCTEXT("MenuCalls", "Calls"), LOCTEXT("MenuDeduction", "Deduction"), LOCTEXT("MenuSocial", "Social")};
		const FKey Keys[] = {EKeys::Z, EKeys::X, EKeys::C};
		for (int32 m = 0; m < FKGVoiceCommandCatalog::MenuCount; ++m)
		{
			FKGVoiceMenuDef& Menu = Out.AddDefaulted_GetRef();
			Menu.Title = Titles[m];
			Menu.Key = Keys[m];
			for (int32 s = 0; s < FKGVoiceCommandCatalog::SlotsPerMenu; ++s)
			{
				Menu.Commands.Add(m * FKGVoiceCommandCatalog::SlotsPerMenu + s);
			}
		}
		return Out;
	}

	uint32 Hash(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}
}

bool FKGVoiceCommandDef::UsesPlace() const
{
	return Line.ToString().Contains(TEXT("{place}"));
}

const TArray<FKGVoiceCommandDef>& FKGVoiceCommandCatalog::GetAll()
{
	static const TArray<FKGVoiceCommandDef> All = KGVoiceCommandsPrivate::Build();
	return All;
}

const TArray<FKGVoiceMenuDef>& FKGVoiceCommandCatalog::GetMenus()
{
	static const TArray<FKGVoiceMenuDef> Menus = KGVoiceCommandsPrivate::BuildMenus();
	return Menus;
}

int32 FKGVoiceCommandCatalog::IndexOf(FName Id)
{
	if (Id.IsNone())
	{
		return INDEX_NONE;
	}
	const TArray<FKGVoiceCommandDef>& All = GetAll();
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].Id == Id)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

const FKGVoiceCommandDef* FKGVoiceCommandCatalog::Find(FName Id)
{
	return Get(IndexOf(Id));
}

const FKGVoiceCommandDef* FKGVoiceCommandCatalog::Get(int32 Index)
{
	const TArray<FKGVoiceCommandDef>& All = GetAll();
	return All.IsValidIndex(Index) ? &All[Index] : nullptr;
}

int32 FKGVoiceCommandCatalog::MenuForKey(const FKey& Key)
{
	const TArray<FKGVoiceMenuDef>& Menus = GetMenus();
	for (int32 i = 0; i < Menus.Num(); ++i)
	{
		if (Menus[i].Key == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

float FKGVoiceCommandCatalog::MouthEnvelope(float T, int32 Syllables, uint32 Seed)
{
	const int32 N = FMath::Max(1, Syllables);
	const float Duration = BarkDuration(N);
	if (T < 0.0f || T >= Duration)
	{
		return 0.0f;
	}
	// Syllables share the duration equally; each opens for 60% of its slot with a sine hump. Accents (peak 0.55..1)
	// come from a hash so the same line always moves the same way on every machine.
	const float Slot = Duration / N;
	const int32 K = FMath::Min(static_cast<int32>(T / Slot), N - 1);
	const float Local = (T - K * Slot) / Slot;
	if (Local > 0.6f)
	{
		return 0.0f;
	}
	const float Peak = 0.55f + 0.45f * (static_cast<float>(KGVoiceCommandsPrivate::Hash(Seed * 31u + K) & 0xFFFF) / 65535.0f);
	return Peak * FMath::Sin(Local / 0.6f * PI);
}

const TCHAR* FKGVoiceCommandCatalog::VoiceName(EKGBarkVoice Voice)
{
	switch (Voice)
	{
	case EKGBarkVoice::MaleLow: return TEXT("MaleLow");
	case EKGBarkVoice::MaleYoung: return TEXT("MaleYoung");
	case EKGBarkVoice::Female: return TEXT("Female");
	case EKGBarkVoice::Old: return TEXT("Old");
	default: return TEXT("MaleLow");
	}
}

FString FKGVoiceCommandCatalog::WavePath(EKGBarkVoice Voice, FName Id)
{
	const FString Name = FString::Printf(TEXT("S_Bark_%s_%s"), VoiceName(Voice), *Id.ToString());
	return FString::Printf(TEXT("/Game/KillGodot/Audio/Barks/%s.%s"), *Name, *Name);
}

EKGBarkVoice FKGVoiceCommandCatalog::VoiceFor(bool bFemale, bool bOld, int32 PlayerId)
{
	if (bFemale)
	{
		return EKGBarkVoice::Female;
	}
	if (bOld)
	{
		return EKGBarkVoice::Old;
	}
	return (PlayerId & 1) ? EKGBarkVoice::MaleYoung : EKGBarkVoice::MaleLow;
}

FKGChatRateLimiter FKGVoiceCommandCatalog::MakeLimiter()
{
	FKGChatRateLimiter L;
	L.Burst = 3.0f;
	L.RefillPerSecond = 0.5f;
	L.DuplicateWindow = 0.0f;
	return L;
}

FString FKGVoiceCommandCatalog::FormatLine(const FKGVoiceCommandDef& Def, const FText& Place)
{
	FString Text = Def.Line.ToString();
	if (Def.UsesPlace())
	{
		const FString Where = Place.IsEmpty() ? LOCTEXT("PlaceUnknown", "square").ToString() : Place.ToString();
		Text = Text.Replace(TEXT("{place}"), *Where);
	}
	return Text;
}

#undef LOCTEXT_NAMESPACE
