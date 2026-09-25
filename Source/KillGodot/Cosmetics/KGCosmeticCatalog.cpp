#include "Cosmetics/KGCosmeticCatalog.h"

#define LOCTEXT_NAMESPACE "KGCosmetics"

namespace KGCosmeticCatalogPrivate
{
	const TCHAR* OutfitDir = TEXT("/Game/KillGodot/Cosmetics/Outfits/");
	const TCHAR* PropsDir = TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/");
	const TCHAR* FishDir = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/");

	/** Interchange names the mesh after the glTF mesh, inside the folder named after our asset key. */
	FSoftObjectPath Outfit(const TCHAR* Folder, const TCHAR* Mesh)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s/%s.%s"), OutfitDir, Folder, Mesh, Mesh));
	}

	FSoftObjectPath Prop(const TCHAR* Dir, const TCHAR* Mesh)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), Dir, Mesh, Mesh));
	}

	FLinearColor Hex(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
	}

	struct FBuilder
	{
		TArray<FKGCosmeticDef> Items;

		FKGCosmeticDef& Add(const TCHAR* Id, const FText& Name, const FText& Desc, EKGCosmeticSlot Slot,
		                    EKGRarity Rarity, int32 Price, const TCHAR* Glyph, const FLinearColor& Icon,
		                    const FSoftObjectPath& Mesh, const TCHAR* Bone, const FTransform& Placement = FTransform::Identity)
		{
			FKGCosmeticDef& Def = Items.AddDefaulted_GetRef();
			Def.Id = FName(Id);
			Def.DisplayName = Name;
			Def.Description = Desc;
			Def.Slot = Slot;
			Def.Rarity = Rarity;
			Def.Price = Price;
			Def.Glyph = Glyph;
			Def.IconColor = Icon;
			Def.Mesh = Mesh;
			Def.AttachBone = FName(Bone);
			Def.Placement = Placement;
			return Def;
		}
	};

	FTransform Place(const FVector& Location, const FRotator& Rotation, float Scale)
	{
		return FTransform(Rotation, Location, FVector(Scale));
	}

	TArray<FKGCosmeticDef> Build()
	{
		using S = EKGCosmeticSlot;
		using R = EKGRarity;
		FBuilder B;
		const FSoftObjectPath HoodM = Outfit(TEXT("SM_KG_Cos_Hood_M"), TEXT("Male_Ranger_Head_Hood"));
		const FSoftObjectPath HoodF = Outfit(TEXT("SM_KG_Cos_Hood_F"), TEXT("Female_Ranger_Head_Hood"));
		const FSoftObjectPath PauldronM = Outfit(TEXT("SM_KG_Cos_Pauldron_M"), TEXT("Male_Ranger_Acc_Pauldron"));
		const FSoftObjectPath PauldronF = Outfit(TEXT("SM_KG_Cos_Pauldrons_F"), TEXT("Female_Ranger_Acc_Pauldrons"));
		const FSoftObjectPath Beard = Outfit(TEXT("SM_KG_Cos_Beard"), TEXT("Hair_Beard"));

		// ---- Head: novelty hats made from village props (placements from the prop bounds; head top ~z 184) ----
		B.Add(TEXT("Hat_Bucket"), LOCTEXT("HatBucket", "Bucket Helm"),
		      LOCTEXT("HatBucketDesc", "Every villager's first armour. Smells faintly of well water."),
		      S::Head, R::Common, 0, TEXT("Bk"), Hex(166, 124, 82), Prop(PropsDir, TEXT("Bucket_Wooden_1")), TEXT("Head"),
		      Place(FVector(0.0f, -1.0f, 187.5f), FRotator(0.0f, 0.0f, 180.0f), 0.6f)).bStarter = true;
		B.Add(TEXT("Hat_StewPot"), LOCTEXT("HatPot", "Stew Pot Helm"),
		      LOCTEXT("HatPotDesc", "Borrowed from the inn. The innkeeper has questions."),
		      S::Head, R::Common, 120, TEXT("Pt"), Hex(120, 120, 128), Prop(PropsDir, TEXT("Pot_1")), TEXT("Head"),
		      Place(FVector(0.0f, -1.0f, 186.0f), FRotator(0.0f, 0.0f, 180.0f), 0.5f));
		B.Add(TEXT("Hat_Candle"), LOCTEXT("HatCandle", "Candle Crown"),
		      LOCTEXT("HatCandleDesc", "For the villager who is always the last one awake."),
		      S::Head, R::Rare, 250, TEXT("Cn"), Hex(255, 226, 150), Prop(PropsDir, TEXT("Candle_1")), TEXT("Head"),
		      Place(FVector(0.0f, -1.0f, 181.0f), FRotator::ZeroRotator, 1.0f));
		B.Add(TEXT("Hat_Mackerel"), LOCTEXT("HatMackerel", "Mackerel Cap"),
		      LOCTEXT("HatMackerelDesc", "Fresh this morning. Less fresh by the trial."),
		      S::Head, R::Rare, 200, TEXT("Mk"), Hex(92, 155, 209), Prop(FishDir, TEXT("SM_KG_Fish_Mackerel")), TEXT("Head"),
		      Place(FVector(0.0f, -1.0f, 187.0f), FRotator(0.0f, 90.0f, 0.0f), 0.9f));
		B.Add(TEXT("Hat_GoldenCarp"), LOCTEXT("HatCarp", "Golden Carp Crown"),
		      LOCTEXT("HatCarpDesc", "The harbour's legend, worn as a crown. Wishes not included."),
		      S::Head, R::Legendary, 1500, TEXT("GC"), Hex(255, 201, 60), Prop(FishDir, TEXT("SM_KG_Fish_GoldenCarp")),
		      TEXT("Head"), Place(FVector(0.0f, -1.0f, 189.5f), FRotator(0.0f, 90.0f, 0.0f), 0.65f));

		// ---- Head: Quaternius ranger hood (bind pose) + colour variants on M_KG_Character's Tint ----
		B.Add(TEXT("Hood_Ranger"), LOCTEXT("HoodRanger", "Ranger Hood"),
		      LOCTEXT("HoodRangerDesc", "Keeps the sea spray off. And your face out of the gossip."),
		      S::Head, R::Rare, 300, TEXT("Hd"), Hex(92, 120, 70), HoodM, TEXT("Head")).FemaleMesh = HoodF;
		{
			FKGCosmeticDef& Def = B.Add(TEXT("Hood_Crimson"), LOCTEXT("HoodCrimson", "Crimson Hood"),
			                            LOCTEXT("HoodCrimsonDesc", "Bold choice in a village that hangs people."),
			                            S::Head, R::Epic, 600, TEXT("Hd"), Hex(200, 40, 46), HoodM, TEXT("Head"));
			Def.FemaleMesh = HoodF;
			Def.bTint = true;
			Def.Tint = FLinearColor(1.0f, 0.28f, 0.24f, 1.0f);
		}
		{
			FKGCosmeticDef& Def = B.Add(TEXT("Hood_Nightwatch"), LOCTEXT("HoodNight", "Nightwatch Hood"),
			                            LOCTEXT("HoodNightDesc", "Dyed with harbour ink for the long night shifts."),
			                            S::Head, R::Epic, 650, TEXT("Hd"), Hex(60, 72, 140), HoodM, TEXT("Head"));
			Def.FemaleMesh = HoodF;
			Def.bTint = true;
			Def.Tint = FLinearColor(0.34f, 0.42f, 0.85f, 1.0f);
		}

		// ---- Face: UBC beard (bind pose), tinted ----
		B.Add(TEXT("Beard_Fisher"), LOCTEXT("BeardFisher", "Fisherman's Beard"),
		      LOCTEXT("BeardFisherDesc", "Grown over three winters and one very long day at sea."),
		      S::Face, R::Common, 100, TEXT("Bd"), Hex(140, 66, 30), Beard, TEXT("Head"));
		{
			FKGCosmeticDef& Def = B.Add(TEXT("Beard_Ginger"), LOCTEXT("BeardGinger", "Ginger Beard"),
			                            LOCTEXT("BeardGingerDesc", "Visible from the lighthouse."),
			                            S::Face, R::Rare, 220, TEXT("Bd"), Hex(230, 110, 40), Beard, TEXT("Head"));
			Def.bTint = true;
			Def.Tint = FLinearColor(1.0f, 0.42f, 0.12f, 1.0f);
		}
		{
			FKGCosmeticDef& Def = B.Add(TEXT("Beard_OldSalt"), LOCTEXT("BeardSalt", "Old Salt Beard"),
			                            LOCTEXT("BeardSaltDesc", "White as a gull and twice as loud."),
			                            S::Face, R::Rare, 240, TEXT("Bd"), Hex(235, 235, 240), Beard, TEXT("Head"));
			Def.bTint = true;
			Def.Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		}

		// ---- Shoulders: Quaternius ranger pauldron (left clavicle; the pair mirrors onto the right) ----
		B.Add(TEXT("Pauldron_Ranger"), LOCTEXT("PauldronRanger", "Ranger Pauldron"),
		      LOCTEXT("PauldronRangerDesc", "One shoulder, because the other one carries the nets."),
		      S::Shoulders, R::Rare, 350, TEXT("Pa"), Hex(122, 96, 66), PauldronM, TEXT("clavicle_l")).FemaleMesh = PauldronF;
		{
			FKGCosmeticDef& Def = B.Add(TEXT("Pauldron_Twin"), LOCTEXT("PauldronTwin", "Twin Pauldrons"),
			                            LOCTEXT("PauldronTwinDesc", "Symmetry, at last. The Mayor approves."),
			                            S::Shoulders, R::Epic, 700, TEXT("PP"), Hex(150, 118, 80), PauldronM,
			                            TEXT("clavicle_l"));
			Def.FemaleMesh = PauldronF;
			Def.MirrorBone = FName(TEXT("clavicle_r"));
		}

		// ---- Back / Belt: props (spine_03 at z 131, back surface ~y -12) ----
		B.Add(TEXT("Back_Satchel"), LOCTEXT("BackSatchel", "Traveller's Satchel"),
		      LOCTEXT("BackSatchelDesc", "Room for bread, rope and one suspicious letter."),
		      S::Back, R::Common, 150, TEXT("Sa"), Hex(176, 141, 87), Prop(PropsDir, TEXT("Bag")), TEXT("spine_03"),
		      Place(FVector(0.0f, -23.0f, 107.0f), FRotator::ZeroRotator, 0.45f));
		B.Add(TEXT("Back_Shield"), LOCTEXT("BackShield", "Driftwood Shield"),
		      LOCTEXT("BackShieldDesc", "Washed ashore with a name carved on it. Not yours."),
		      S::Back, R::Rare, 320, TEXT("Sh"), Hex(139, 101, 66), Prop(PropsDir, TEXT("Shield_Wooden")), TEXT("spine_03"),
		      Place(FVector(0.0f, -13.0f, 128.0f), FRotator(0.0f, 180.0f, 0.0f), 0.7f));
		B.Add(TEXT("Belt_CoinPouch"), LOCTEXT("BeltPouch", "Coin Pouch"),
		      LOCTEXT("BeltPouchDesc", "Jingles when you walk. Terrible for sneaking."),
		      S::Belt, R::Common, 90, TEXT("Po"), Hex(150, 104, 60), Prop(PropsDir, TEXT("Pouch_Large")), TEXT("pelvis"),
		      Place(FVector(-17.0f, -2.0f, 88.0f), FRotator(0.0f, 90.0f, 0.0f), 0.8f));
		return B.Items;
	}
}

const TArray<FKGCosmeticDef>& UKGCosmeticCatalog::GetAll()
{
	static const TArray<FKGCosmeticDef> Items = KGCosmeticCatalogPrivate::Build();
	return Items;
}

const FKGCosmeticDef* UKGCosmeticCatalog::Find(FName CosmeticId)
{
	if (CosmeticId.IsNone())
	{
		return nullptr;
	}
	return GetAll().FindByPredicate([CosmeticId](const FKGCosmeticDef& Def) { return Def.Id == CosmeticId; });
}

bool UKGCosmeticCatalog::GetCosmeticDef(FName CosmeticId, FKGCosmeticDef& OutDef)
{
	if (const FKGCosmeticDef* Def = Find(CosmeticId))
	{
		OutDef = *Def;
		return true;
	}
	return false;
}

FText UKGCosmeticCatalog::GetSlotName(EKGCosmeticSlot Slot)
{
	switch (Slot)
	{
	case EKGCosmeticSlot::Head:
		return LOCTEXT("SlotHead", "Head");
	case EKGCosmeticSlot::Face:
		return LOCTEXT("SlotFace", "Face");
	case EKGCosmeticSlot::Shoulders:
		return LOCTEXT("SlotShoulders", "Shoulders");
	case EKGCosmeticSlot::Back:
		return LOCTEXT("SlotBack", "Back");
	case EKGCosmeticSlot::Belt:
		return LOCTEXT("SlotBelt", "Belt");
	default:
		return FText::GetEmpty();
	}
}

FName UKGCosmeticCatalog::ResolveLoose(const FString& Text)
{
	const FString Wanted = Text.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
	for (const FKGCosmeticDef& Def : GetAll())
	{
		const FString Id = Def.Id.ToString().Replace(TEXT("_"), TEXT(""));
		const FString Name = Def.DisplayName.ToString().Replace(TEXT(" "), TEXT("")).Replace(TEXT("'"), TEXT(""));
		if (Id.Equals(Wanted, ESearchCase::IgnoreCase) || Name.Equals(Wanted, ESearchCase::IgnoreCase))
		{
			return Def.Id;
		}
	}
	return NAME_None;
}

#undef LOCTEXT_NAMESPACE
