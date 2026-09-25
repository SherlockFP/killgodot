#include "UI/Reveal/KGPseudonyms.h"

#include "Hash/CityHash.h"

namespace KGPseudonymsPrivate
{
	// Village-themed halves (Docs/Lore/KillGo_Lore.md: a fishing village that waits). 24 x 24 = 576 names.
	const TCHAR* const Adjectives[] = {
		TEXT("Foggy"), TEXT("Salty"), TEXT("Quiet"), TEXT("Brass"), TEXT("Patient"), TEXT("Sleepy"), TEXT("Lantern"),
		TEXT("Tidal"), TEXT("Rusty"), TEXT("Gentle"), TEXT("Lucky"), TEXT("Dusky"), TEXT("Mossy"), TEXT("Harbour"),
		TEXT("Crooked"), TEXT("Humble"), TEXT("Wandering"), TEXT("Misty"), TEXT("Stormy"), TEXT("Candle"),
		TEXT("Amber"), TEXT("Pebble"), TEXT("Driftwood"), TEXT("Chapel")};
	const TCHAR* const Nouns[] = {
		TEXT("Herring"), TEXT("Gull"), TEXT("Anchor"), TEXT("Lantern"), TEXT("Kettle"), TEXT("Oyster"), TEXT("Heron"),
		TEXT("Net"), TEXT("Barrel"), TEXT("Bell"), TEXT("Oar"), TEXT("Cockle"), TEXT("Crab"), TEXT("Buoy"),
		TEXT("Otter"), TEXT("Pilgrim"), TEXT("Ferry"), TEXT("Wick"), TEXT("Mackerel"), TEXT("Clam"), TEXT("Tern"),
		TEXT("Rope"), TEXT("Puffin"), TEXT("Skiff")};
	constexpr int32 NumAdjectives = UE_ARRAY_COUNT(Adjectives);
	constexpr int32 NumNouns = UE_ARRAY_COUNT(Nouns);

	uint64 HashKey(const FString& Key, uint64 Salt)
	{
		const FTCHARToUTF8 Utf8(*Key);
		return CityHash64WithSeed(Utf8.Get(), static_cast<uint32>(Utf8.Length()), Salt ^ 0x9E3779B97F4A7C15ull);
	}
}

int32 FKGPseudonyms::PoolSize()
{
	return KGPseudonymsPrivate::NumAdjectives * KGPseudonymsPrivate::NumNouns;
}

FString FKGPseudonyms::PoolName(int32 Index)
{
	const int32 Wrapped = ((Index % PoolSize()) + PoolSize()) % PoolSize();
	const TCHAR* Adjective = KGPseudonymsPrivate::Adjectives[Wrapped % KGPseudonymsPrivate::NumAdjectives];
	const TCHAR* Noun = KGPseudonymsPrivate::Nouns[Wrapped / KGPseudonymsPrivate::NumAdjectives];
	if (FCString::Stricmp(Adjective, Noun) == 0)
	{
		return FString::Printf(TEXT("Old %s"), Noun);   // "Lantern Lantern" reads badly
	}
	return FString::Printf(TEXT("%s %s"), Adjective, Noun);
}

void FKGPseudonyms::Reset(uint64 NewSalt)
{
	Salt = NewSalt;
	ByKey.Reset();
	Used.Reset();
	Reserved.Reset();
}

void FKGPseudonyms::ReserveRealName(const FString& RealName)
{
	const FString Clean = RealName.TrimStartAndEnd();
	if (!Clean.IsEmpty())
	{
		Reserved.Add(Clean.ToLower());
	}
}

bool FKGPseudonyms::IsTaken(const FString& Candidate) const
{
	const FString Lower = Candidate.ToLower();
	return Used.Contains(Lower) || Reserved.Contains(Lower);
}

const FString& FKGPseudonyms::Get(const FString& Key, const FString& RealName)
{
	ReserveRealName(RealName);
	if (const FString* Found = ByKey.Find(Key))
	{
		return *Found;
	}
	// Open addressing over the pool: the start and the stride both come from (salt, key), so the same key lands on a
	// different name every match. A stride may share a factor with the pool size and skip slots; the second pass
	// (stride 1) covers whatever the first one skipped.
	const uint64 Hash = KGPseudonymsPrivate::HashKey(Key, Salt);
	const int32 Pool = PoolSize();
	const int32 Start = static_cast<int32>(Hash % static_cast<uint64>(Pool));
	const int32 Stride = static_cast<int32>((Hash >> 32) % static_cast<uint64>(Pool / 2)) * 2 + 1;
	FString Name;
	for (int32 Step = 0; Step < Pool && Name.IsEmpty(); ++Step)
	{
		const FString Candidate = PoolName(Start + Step * Stride);
		Name = IsTaken(Candidate) ? FString() : Candidate;
	}
	for (int32 Step = 0; Step < Pool && Name.IsEmpty(); ++Step)
	{
		const FString Candidate = PoolName(Start + Step);
		Name = IsTaken(Candidate) ? FString() : Candidate;
	}
	for (int32 Number = ByKey.Num() + 1; Name.IsEmpty(); ++Number)
	{
		const FString Candidate = FString::Printf(TEXT("Villager %d"), Number);
		Name = IsTaken(Candidate) ? FString() : Candidate;
	}
	Used.Add(Name.ToLower());
	return ByKey.Add(Key, MoveTemp(Name));
}
