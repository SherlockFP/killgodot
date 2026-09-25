#include "Character/KGVillagerLook.h"

namespace KGLookPrivate
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor(FColor(R, G, B));   // sRGB -> linear
	}

	struct FBuilder
	{
		TArray<FKGVillagerArchetype> Items;

		FKGVillagerArchetype& Add(const TCHAR* Name, bool bFemale, EKGVillagerBody Body, EKGVillagerBuild Build,
		                          EKGVillagerHair Hair, const FLinearColor& OutfitA, const FLinearColor& OutfitB,
		                          const FLinearColor& OutfitC, const FLinearColor& Accent = FLinearColor::White)
		{
			FKGVillagerArchetype& A = Items.AddDefaulted_GetRef();
			A.Name = FName(Name);
			A.bFemale = bFemale;
			A.Body = Body;
			A.Build = Build;
			A.Hair = Hair;
			A.Outfit[0] = OutfitA;
			A.Outfit[1] = OutfitB;
			A.Outfit[2] = OutfitC;
			A.Accent = Accent;
			return A;
		}
	};

	TArray<FKGVillagerArchetype> Build()
	{
		using B = EKGVillagerBody;
		using S = EKGVillagerBuild;
		using H = EKGVillagerHair;
		using T = EKGVillagerHat;
		FBuilder Bd;
		// Outfit tints multiply the Quaternius base colour (peasant = undyed cloth, ranger = green leather), so
		// saturated tints read as dyed cloth; keep the three variants of one trade in the same family.
		const FLinearColor Navy = Hex(60, 90, 150), Teal = Hex(70, 150, 150), Sea = Hex(90, 170, 130);
		const FLinearColor Cream = Hex(245, 235, 210), Wheat = Hex(230, 200, 140), Rose = Hex(235, 170, 170);
		const FLinearColor Olive = Hex(150, 160, 90), Straw = Hex(220, 190, 110), Moss = Hex(110, 140, 80);
		const FLinearColor Rust = Hex(170, 90, 50), Soot = Hex(80, 75, 70), Brown = Hex(140, 100, 70);
		const FLinearColor Crimson = Hex(190, 40, 50), Plum = Hex(120, 50, 120), Royal = Hex(50, 70, 170);
		const FLinearColor Gold = Hex(230, 180, 60), Amber = Hex(230, 140, 50), Mustard = Hex(200, 170, 60);
		const FLinearColor Steel = Hex(150, 160, 175), Iron = Hex(110, 115, 125), DarkRed = Hex(130, 40, 40);
		const FLinearColor Black = Hex(45, 42, 48), Grey = Hex(140, 140, 145), White = Hex(240, 240, 240);
		const FLinearColor Indigo = Hex(70, 60, 150), Midnight = Hex(40, 45, 90), Violet = Hex(130, 80, 180);
		const FLinearColor Forest = Hex(60, 120, 70), Sage = Hex(150, 180, 140), Beige = Hex(215, 200, 170);
		const FLinearColor Sky = Hex(140, 190, 240), Burgundy = Hex(120, 40, 60), Pink = Hex(240, 150, 190);
		const FLinearColor HeroBlue = Hex(40, 90, 220), HeroRed = Hex(230, 50, 50), HeroYellow = Hex(250, 210, 60);

		// ---- village men ----
		Bd.Add(TEXT("Fisher"), false, B::MaleMixed, S::Stocky, H::Buzzed, Navy, Teal, Sea).bBeard = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Smith"), false, B::MaleMixed, S::Stocky, H::Bald, Rust, Soot, Brown, Soot);
			A.bBeard = true; A.bApron = true;
		}
		Bd.Add(TEXT("Farmer"), false, B::MalePeasantBald, S::Regular, H::SimpleParted, Olive, Straw, Moss, Straw).Hat = T::Straw;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Baker"), false, B::MalePeasantBald, S::Stocky, H::Buzzed, Cream, Wheat, White, Cream);
			A.bApron = true; A.Hat = T::Toque;
		}
		Bd.Add(TEXT("Miller"), false, B::MaleMixed, S::Tall, H::SimpleParted, Grey, Beige, Wheat).bBeard = true;
		Bd.Add(TEXT("Shepherd"), false, B::MalePeasantBald, S::Short, H::Buzzed, Beige, Brown, Grey, Beige).bHood = true;
		Bd.Add(TEXT("Merchant"), false, B::MaleRanger, S::Regular, H::SimpleParted, Gold, Mustard, Amber);
		Bd.Add(TEXT("Noble"), false, B::MaleRanger, S::Tall, H::SimpleParted, Crimson, Plum, Royal, Crimson).bCape = true;
		Bd.Add(TEXT("Guard"), false, B::MaleRanger, S::Tall, H::Buzzed, Steel, DarkRed, Navy, Iron).bPauldrons = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Knight"), false, B::MaleRanger, S::Stocky, H::Bald, Steel, Iron, White, Steel);
			A.bHood = true; A.bPauldrons = true; A.bCape = true;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Priest"), false, B::MalePeasantBald, S::Regular, H::Bald, Black, Plum, White, Black);
			A.bOld = true; A.bHood = true;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Wizard"), false, B::MalePeasantBald, S::Tall, H::Long, Indigo, Midnight, Violet, Midnight);
			A.bOld = true; A.bBeard = true; A.bCape = true; A.Hat = T::Witch;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Pirate"), false, B::MaleRanger, S::Stocky, H::Buzzed, DarkRed, Black, Navy, Crimson);
			A.bBeard = true; A.bHood = true;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Innkeeper"), false, B::MalePeasantBald, S::Stocky, H::Buzzed, Burgundy, Forest, Brown, Brown);
			A.bBeard = true; A.bApron = true;
		}
		Bd.Add(TEXT("Gravedigger"), false, B::MaleMixed, S::Tall, H::Bald, Black, Soot, Grey, Black).bHood = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("King"), false, B::MaleRanger, S::Regular, H::Long, Plum, Gold, Crimson, Gold);
			A.bBeard = true; A.bCape = true; A.Hat = T::Crown;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("OldSalt"), false, B::MalePeasantBald, S::Short, H::Bald, Navy, Grey, Teal, Navy);
			A.bOld = true; A.bBeard = true; A.bHood = true;
		}
		Bd.Add(TEXT("Hero"), false, B::MaleMixed, S::Tall, H::SimpleParted, HeroBlue, HeroRed, HeroYellow, HeroRed).bCape = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Chef"), false, B::MalePeasantBald, S::Stocky, H::Bald, White, Grey, Cream, White);
			A.bApron = true; A.Hat = T::Toque;
		}
		Bd.Add(TEXT("Scholar"), false, B::MalePeasantBald, S::Slim, H::SimpleParted, Midnight, Indigo, Soot, Indigo).bOld = true;
		Bd.Add(TEXT("Ranger"), false, B::MaleRanger, S::Slim, H::Buzzed, Forest, Moss, Olive, Forest).bHood = true;

		// ---- village women ----
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("BakerWoman"), true, B::FemalePeasantBald, S::Regular, H::Buns, Cream, Rose, Wheat, Cream);
			A.bApron = true; A.Hat = T::Toque;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Herbalist"), true, B::FemalePeasantBald, S::Regular, H::Buns, Sage, Moss, Olive, Sage);
			A.bOld = true; A.bHood = true;
		}
		Bd.Add(TEXT("Innkeeper's Wife"), true, B::FemalePeasantBald, S::Stocky, H::Buns, Burgundy, Brown, Forest, Brown).bApron = true;
		Bd.Add(TEXT("Weaver"), true, B::FemalePeasantBald, S::Short, H::Long, Indigo, Teal, Violet);
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Huntress"), true, B::FemaleRanger, S::Regular, H::BuzzedFemale, Forest, Olive, Moss, Forest);
			A.bHood = true; A.bPauldrons = true;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Witch"), true, B::FemalePeasantBald, S::Slim, H::Long, Black, Plum, Midnight, Plum);
			A.bCape = true; A.Hat = T::Witch;
		}
		Bd.Add(TEXT("Nun"), true, B::FemalePeasantBald, S::Regular, H::Bald, Black, White, Grey, Black).bHood = true;
		Bd.Add(TEXT("Sailor"), true, B::FemaleMixed, S::Regular, H::BuzzedFemale, Navy, White, Sky);
		Bd.Add(TEXT("MerchantWoman"), true, B::FemaleRanger, S::Regular, H::Long, Gold, Amber, Rose);
		Bd.Add(TEXT("Lady"), true, B::FemaleRanger, S::Tall, H::Buns, Crimson, Royal, Pink, Crimson).bCape = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Queen"), true, B::FemaleRanger, S::Tall, H::Long, Plum, Gold, Royal, Gold);
			A.bCape = true; A.Hat = T::Crown;
		}
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("KnightWoman"), true, B::FemaleRanger, S::Tall, H::BuzzedFemale, Steel, Iron, White, Steel);
			A.bHood = true; A.bPauldrons = true; A.bCape = true;
		}
		Bd.Add(TEXT("PirateWoman"), true, B::FemaleRanger, S::Regular, H::Long, DarkRed, Black, Navy, Crimson).bHood = true;
		Bd.Add(TEXT("Heroine"), true, B::FemaleMixed, S::Tall, H::Long, HeroBlue, HeroRed, HeroYellow, HeroRed).bCape = true;
		Bd.Add(TEXT("NetMender"), true, B::FemaleMixed, S::Stocky, H::BuzzedFemale, Navy, Teal, Sea, Straw).Hat = T::Straw;
		Bd.Add(TEXT("FarmerWoman"), true, B::FemalePeasantBald, S::Regular, H::Buns, Olive, Straw, Moss, Straw).Hat = T::Straw;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("Widow"), true, B::FemalePeasantBald, S::Short, H::Long, Black, Grey, Soot, Black);
			A.bOld = true; A.bHood = true;
		}
		Bd.Add(TEXT("GuardWoman"), true, B::FemaleRanger, S::Tall, H::Buns, Steel, DarkRed, Navy, Iron).bPauldrons = true;
		{
			FKGVillagerArchetype& A = Bd.Add(TEXT("ScholarWoman"), true, B::FemalePeasantBald, S::Slim, H::Buns, Indigo, Midnight, Violet, Indigo);
			A.bOld = true; A.bCape = true;
		}
		return Bd.Items;
	}
}

FString FKGVillagerLook::ToString() const
{
	if (!IsAssigned())
	{
		return TEXT("(unassigned)");
	}
	return FString::Printf(TEXT("%s skin=%d hair=%d outfit=%d"), *FKGVillagerLookGen::ArchetypeOf(*this).Name.ToString(),
	                       SkinTone, HairColour, OutfitVariant);
}

const TArray<FKGVillagerArchetype>& FKGVillagerLookGen::Archetypes()
{
	static const TArray<FKGVillagerArchetype> Table = KGLookPrivate::Build();
	return Table;
}

const FKGVillagerArchetype* FKGVillagerLookGen::FindArchetype(FName Name)
{
	const int32 Index = ArchetypeIndex(Name);
	return Index == INDEX_NONE ? nullptr : &Archetypes()[Index];
}

int32 FKGVillagerLookGen::ArchetypeIndex(FName Name)
{
	if (Name.IsNone())
	{
		return INDEX_NONE;
	}
	return Archetypes().IndexOfByPredicate([Name](const FKGVillagerArchetype& A) { return A.Name == Name; });
}

const FKGVillagerArchetype& FKGVillagerLookGen::ArchetypeOf(const FKGVillagerLook& Look)
{
	const TArray<FKGVillagerArchetype>& Table = Archetypes();
	return Table[Table.IsValidIndex(Look.Archetype) ? Look.Archetype : 0];
}

FLinearColor FKGVillagerLookGen::SkinTint(uint8 Tone)
{
	// Multiplies the light Quaternius skin texture.
	static const FLinearColor Tones[SkinToneCount] = {
		FLinearColor(1.00f, 1.00f, 1.00f), FLinearColor(0.95f, 0.82f, 0.68f), FLinearColor(0.80f, 0.62f, 0.45f),
		FLinearColor(0.55f, 0.38f, 0.25f), FLinearColor(0.34f, 0.23f, 0.16f)};
	return Tones[Tone % SkinToneCount];
}

FLinearColor FKGVillagerLookGen::HairTint(uint8 Colour)
{
	static const FLinearColor Colours[HairColourCount] = {
		FLinearColor(0.06f, 0.05f, 0.05f),   // black
		FLinearColor(0.30f, 0.15f, 0.07f),   // brown
		FLinearColor(0.55f, 0.26f, 0.12f),   // chestnut
		FLinearColor(0.95f, 0.75f, 0.40f),   // blonde
		FLinearColor(0.95f, 0.38f, 0.12f),   // ginger
		FLinearColor(0.16f, 0.10f, 0.12f),   // dark auburn
		FLinearColor(0.70f, 0.70f, 0.74f),   // grey (old)
		FLinearColor(0.92f, 0.90f, 0.88f)};  // white (old)
	return Colours[Colour % HairColourCount];
}

FKGVillagerLook FKGVillagerLookGen::Generate(uint64 Seed, int32 PlayerId, const TArray<FKGVillagerLook>& Taken,
                                             FName Preferred)
{
	const TArray<FKGVillagerArchetype>& Table = Archetypes();
	const int32 Count = Table.Num();
	TSet<uint32> TakenKeys;
	TSet<uint8> TakenArchetypes;
	for (const FKGVillagerLook& L : Taken)
	{
		if (L.IsAssigned())
		{
			TakenKeys.Add(L.Key());
			TakenArchetypes.Add(L.Archetype);
		}
	}
	// One stream per (match, player): the same player gets the same look after a host migration re-deal.
	const uint32 Lo = static_cast<uint32>(Seed), Hi = static_cast<uint32>(Seed >> 32);
	FRandomStream Rng(static_cast<int32>(HashCombine(HashCombine(Lo, Hi), GetTypeHash(PlayerId))));

	auto Roll = [&](int32 ArchetypeIndex)
	{
		const FKGVillagerArchetype& A = Table[ArchetypeIndex];
		FKGVillagerLook L;
		L.Archetype = static_cast<uint8>(ArchetypeIndex);
		L.SkinTone = static_cast<uint8>(Rng.RandRange(0, SkinToneCount - 1));
		L.HairColour = static_cast<uint8>(A.bOld ? Rng.RandRange(6, HairColourCount - 1) : Rng.RandRange(0, 5));
		L.OutfitVariant = static_cast<uint8>(Rng.RandRange(0, OutfitVariantCount - 1));
		return L;
	};

	// 1. A locked archetype: any free palette of it.
	const int32 PreferredIndex = ArchetypeIndex(Preferred);
	if (PreferredIndex != INDEX_NONE)
	{
		for (int32 Attempt = 0; Attempt < 48; ++Attempt)
		{
			const FKGVillagerLook L = Roll(PreferredIndex);
			if (!TakenKeys.Contains(L.Key()))
			{
				return L;
			}
		}
	}
	// 2. Spread: an archetype nobody wears yet, in a seeded order.
	TArray<int32> Order;
	for (int32 i = 0; i < Count; ++i)
	{
		Order.Add(i);
	}
	for (int32 i = Count - 1; i > 0; --i)
	{
		Order.Swap(i, Rng.RandRange(0, i));
	}
	for (const int32 Index : Order)
	{
		if (!TakenArchetypes.Contains(static_cast<uint8>(Index)))
		{
			return Roll(Index);
		}
	}
	// 3. Every archetype is worn (lobby > archetype count): any free palette.
	FKGVillagerLook Last;
	for (int32 Attempt = 0; Attempt < 256; ++Attempt)
	{
		Last = Roll(Order[Attempt % Count]);
		if (!TakenKeys.Contains(Last.Key()))
		{
			return Last;
		}
	}
	return Last;
}
