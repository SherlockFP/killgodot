#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Core/KGTypes.h"

/** Placeholder bark voices (Tools/Audio/kg_synth_barks.py writes one WAV per voice per line). */
enum class EKGBarkVoice : uint8
{
	MaleLow,
	MaleYoung,
	Female,
	Old,
	Count
};

/** One TF2-style voice command: a spoken bark, a mouth pattern, a gesture and a NEAR chat line. */
struct KILLGODOT_API FKGVoiceCommandDef
{
	/** Stable id = wave name suffix (S_Bark_<Voice>_<Id>) and the kg.Bark argument. */
	FName Id;
	/** Spoken line, may contain {place} (filled with the speaker's minimap region). */
	FText Line;
	/** Emote id from FKGEmoteCatalog played as the gesture (NAME_None = the talking idle only). */
	FName Gesture;
	/** Mouth / bark rhythm: syllables of the placeholder voice (the synth uses the same table). */
	uint8 Syllables = 2;
	/** Reaction emoji id popped over the head (FKGEmoji), empty = none. */
	FString Emoji;

	bool UsesPlace() const;
};

/** A radial: key + 8 commands (slot 0 at the top, clockwise). */
struct KILLGODOT_API FKGVoiceMenuDef
{
	FText Title;
	/** Z, X or C. */
	FKey Key;
	TArray<int32> Commands;   // indices into FKGVoiceCommandCatalog::GetAll()
};

/** The three menus and their 24 lines. Pure data, no assets (like FKGEmoteCatalog). */
struct KILLGODOT_API FKGVoiceCommandCatalog
{
	static constexpr int32 MenuCount = 3;
	static constexpr int32 SlotsPerMenu = 8;

	static const TArray<FKGVoiceCommandDef>& GetAll();
	static const TArray<FKGVoiceMenuDef>& GetMenus();
	static const FKGVoiceCommandDef* Find(FName Id);
	static int32 IndexOf(FName Id);
	static const FKGVoiceCommandDef* Get(int32 Index);
	/** Menu index (0..2) for a key, INDEX_NONE if the key opens no radial. */
	static int32 MenuForKey(const FKey& Key);

	/** Length of a bark with N syllables (seconds); the synth and the mouth agree on it. */
	static float BarkDuration(int32 Syllables) { return 0.16f * Syllables + 0.25f; }
	/** Mouth envelope 0..1 at time T into a bark of N syllables (deterministic, Seed varies the accents). */
	static float MouthEnvelope(float T, int32 Syllables, uint32 Seed);

	/** Wave asset path for a line in a voice: /Game/KillGodot/Audio/Barks/S_Bark_<Voice>_<Id>. */
	static FString WavePath(EKGBarkVoice Voice, FName Id);
	static const TCHAR* VoiceName(EKGBarkVoice Voice);
	/** Placeholder voice for a body: from the villager look (female / old archetypes), else by player id. */
	static EKGBarkVoice VoiceFor(bool bFemale, bool bOld, int32 PlayerId);

	/** Per-player server limiter: 3 barks in a burst, then one every 2 s (same shape as the emote limiter). */
	static FKGChatRateLimiter MakeLimiter();

	/** The spoken line with {place} filled in ("I was at the Harbour"). */
	static FString FormatLine(const FKGVoiceCommandDef& Def, const FText& Place);
};
