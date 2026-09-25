#include "Inventory/KGItemCatalog.h"

#define LOCTEXT_NAMESPACE "KGItems"

const FName FKGItemIds::Coin(TEXT("Coin"));
const FName FKGItemIds::Key(TEXT("Key"));
const FName FKGItemIds::Bone(TEXT("Bone"));
const FName FKGItemIds::OldRing(TEXT("OldRing"));
const FName FKGItemIds::Pearl(TEXT("Pearl"));
const FName FKGItemIds::FishMackerel(TEXT("Fish_Mackerel"));
const FName FKGItemIds::FishCod(TEXT("Fish_Cod"));
const FName FKGItemIds::FishSalmon(TEXT("Fish_Salmon"));
const FName FKGItemIds::FishGoldenCarp(TEXT("Fish_GoldenCarp"));
const FName FKGItemIds::Bread(TEXT("Bread"));
const FName FKGItemIds::Apple(TEXT("Apple"));
const FName FKGItemIds::Rope(TEXT("Rope"));
const FName FKGItemIds::Candle(TEXT("Candle"));
const FName FKGItemIds::TreasureMap(TEXT("TreasureMap"));
const FName FKGItemIds::FishingRod(TEXT("FishingRod"));
const FName FKGItemIds::OldBoot(TEXT("OldBoot"));
const FName FKGItemIds::MessageBottle(TEXT("MessageBottle"));
const FName FKGItemIds::CoinPouch(TEXT("CoinPouch"));
// KG_DIG
const FName FKGItemIds::Shovel(TEXT("Shovel"));
const FName FKGItemIds::CryptKey(TEXT("CryptKey"));
const FName FKGItemIds::MapScrap(TEXT("MapScrap"));
const FName FKGItemIds::Skull(TEXT("Skull"));
const FName FKGItemIds::MournerToken(TEXT("MournerToken"));

namespace KGItemCatalogPrivate
{
	const TCHAR* PropsDir = TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/");
	const TCHAR* FishDir = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/");

	FSoftObjectPath Mesh(const TCHAR* Dir, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), Dir, Name, Name));
	}

	FLinearColor Hex(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
	}

	FKGItemDef Make(FName Id, const FText& Name, const FText& Desc, const FLinearColor& Color, const TCHAR* Glyph,
	                int32 Stack, int32 Value, EKGRarity Rarity, std::initializer_list<const TCHAR*> Tags,
	                const FSoftObjectPath& PickupMesh, float PickupSize, const TCHAR* Fallback = TEXT("Sphere"),
	                const FVector& ShapeScale = FVector::OneVector, const FRotator& ShapeRotation = FRotator::ZeroRotator)
	{
		FKGItemDef Def;
		Def.Id = Id;
		Def.DisplayName = Name;
		Def.Description = Desc;
		Def.IconColor = Color;
		Def.Glyph = Glyph;
		Def.StackSize = Stack;
		Def.GoldValue = Value;
		Def.Rarity = Rarity;
		for (const TCHAR* Tag : Tags)
		{
			Def.Tags.Add(FName(Tag));
		}
		Def.PickupMesh = PickupMesh;
		Def.PickupFallbackShape = FSoftObjectPath(FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Fallback, Fallback));
		Def.PickupSize = PickupSize;
		Def.PickupShapeScale = ShapeScale;
		Def.PickupShapeRotation = ShapeRotation;
		return Def;
	}

	TArray<FKGItemDef> Build()
	{
		const FSoftObjectPath None;
		TArray<FKGItemDef> Items;
		Items.Add(Make(FKGItemIds::Coin, LOCTEXT("Coin", "Gold Coin"),
		               LOCTEXT("CoinDesc", "Morrowmere's only honest currency. Stacks to 999."),
		               Hex(242, 194, 48), TEXT("G"), 999, 1, EKGRarity::Common, {TEXT("Currency"), TEXT("Valuable")},
		               // The kit coin already stands on its edge (thin along Y), so the spin flashes its face.
		               Mesh(PropsDir, TEXT("Coin")), 14.0f, TEXT("Cylinder")));
		Items.Add(Make(FKGItemIds::Key, LOCTEXT("Key", "Rusty Key"),
		               LOCTEXT("KeyDesc", "Opens something, somewhere. Nobody remembers what."),
		               Hex(201, 166, 107), TEXT("K"), 5, 5, EKGRarity::Common, {TEXT("Tool"), TEXT("Key")},
		               Mesh(PropsDir, TEXT("Key_Metal")), 18.0f));
		Items.Add(Make(FKGItemIds::Bone, LOCTEXT("Bone", "Old Bone"),
		               LOCTEXT("BoneDesc", "Dug up near the graveyard. The dogs want it back."),
		               Hex(237, 230, 214), TEXT("B"), 10, 1, EKGRarity::Common, {TEXT("Junk")},
		               None, 24.0f, TEXT("Cylinder"), FVector(0.28f, 0.28f, 1.0f), FRotator(90.0f, 0.0f, 0.0f)));
		Items.Add(Make(FKGItemIds::OldRing, LOCTEXT("OldRing", "Old Ring"),
		               LOCTEXT("OldRingDesc", "A tarnished wedding band with somebody's initials."),
		               Hex(232, 192, 74), TEXT("R"), 5, 40, EKGRarity::Rare, {TEXT("Valuable")},
		               None, 8.0f, TEXT("Cylinder"), FVector(1.0f, 1.0f, 0.22f), FRotator(0.0f, 0.0f, 90.0f)));
		Items.Add(Make(FKGItemIds::Pearl, LOCTEXT("Pearl", "Pearl"),
		               LOCTEXT("PearlDesc", "Pried from an oyster below the harbour steps."),
		               Hex(244, 241, 255), TEXT("P"), 10, 25, EKGRarity::Rare, {TEXT("Valuable"), TEXT("Sea")},
		               None, 7.0f));
		Items.Add(Make(FKGItemIds::FishMackerel, LOCTEXT("Mackerel", "Mackerel"),
		               LOCTEXT("MackerelDesc", "The village's daily bread, if bread had scales."),
		               Hex(92, 155, 209), TEXT("Mk"), 5, 4, EKGRarity::Common, {TEXT("Fish"), TEXT("Food"), TEXT("Sea")},
		               Mesh(FishDir, TEXT("SM_KG_Fish_Mackerel")), 32.0f));
		Items.Add(Make(FKGItemIds::FishCod, LOCTEXT("Cod", "Cod"),
		               LOCTEXT("CodDesc", "Heavy, grey and honest."),
		               Hex(159, 174, 122), TEXT("Cd"), 5, 6, EKGRarity::Common, {TEXT("Fish"), TEXT("Food"), TEXT("Sea")},
		               Mesh(FishDir, TEXT("SM_KG_Fish_Cod")), 38.0f));
		Items.Add(Make(FKGItemIds::FishSalmon, LOCTEXT("Salmon", "Salmon"),
		               LOCTEXT("SalmonDesc", "Swam all the way upriver just to end up here."),
		               Hex(240, 138, 93), TEXT("Sa"), 5, 10, EKGRarity::Rare, {TEXT("Fish"), TEXT("Food"), TEXT("Sea")},
		               Mesh(FishDir, TEXT("SM_KG_Fish_Salmon")), 42.0f));
		Items.Add(Make(FKGItemIds::FishGoldenCarp, LOCTEXT("GoldenCarp", "Golden Carp"),
		               LOCTEXT("GoldenCarpDesc", "Legend says it grants one wish. It mostly flops."),
		               Hex(255, 201, 60), TEXT("GC"), 1, 150, EKGRarity::Legendary,
		               {TEXT("Fish"), TEXT("Sea"), TEXT("Valuable"), TEXT("Legendary")},
		               Mesh(FishDir, TEXT("SM_KG_Fish_GoldenCarp")), 45.0f));
		Items.Add(Make(FKGItemIds::Bread, LOCTEXT("Bread", "Bread Loaf"),
		               LOCTEXT("BreadDesc", "Still warm from the bakery. Also a decent club."),
		               Hex(217, 164, 91), TEXT("Br"), 5, 2, EKGRarity::Common, {TEXT("Food")},
		               FSoftObjectPath(TEXT("/Game/KillGodot/Items/Melee/Baguette_HPvkMpqqTg/StaticMeshes/SM_KG_Baguette.SM_KG_Baguette")),
		               40.0f));
		Items.Add(Make(FKGItemIds::Apple, LOCTEXT("Apple", "Apple"),
		               LOCTEXT("AppleDesc", "From the orchard behind the chapel."),
		               Hex(224, 65, 58), TEXT("Ap"), 10, 1, EKGRarity::Common, {TEXT("Food")},
		               None, 9.0f));
		Items.Add(Make(FKGItemIds::Rope, LOCTEXT("Rope", "Rope"),
		               LOCTEXT("RopeDesc", "Good for boats, wells and worse."),
		               Hex(176, 141, 87), TEXT("Ro"), 3, 3, EKGRarity::Common, {TEXT("Tool")},
		               Mesh(PropsDir, TEXT("Rope_1")), 30.0f));
		Items.Add(Make(FKGItemIds::Candle, LOCTEXT("Candle", "Candle"),
		               LOCTEXT("CandleDesc", "Keeps the dark (and the Impatient) at arm's length."),
		               Hex(255, 241, 201), TEXT("Ca"), 5, 2, EKGRarity::Common, {TEXT("Tool"), TEXT("Light")},
		               Mesh(PropsDir, TEXT("Candle_1")), 16.0f));
		Items.Add(Make(FKGItemIds::TreasureMap, LOCTEXT("TreasureMap", "Treasure Map"),
		               LOCTEXT("TreasureMapDesc", "An X, a cliff and a date. Tomorrow's date."),
		               Hex(230, 211, 163), TEXT("Map"), 1, 60, EKGRarity::Epic, {TEXT("Quest"), TEXT("Valuable")},
		               Mesh(PropsDir, TEXT("Scroll_1")), 30.0f));
		// Fishing (Docs/01_GDD_Core.md §15). Rod: H takes it out; Madam Brine at the Fish Market lends one.
		Items.Add(Make(FKGItemIds::FishingRod, LOCTEXT("FishingRod", "Fishing Rod"),
		               LOCTEXT("FishingRodDesc", "Bamboo, cork and a brass reel. H to take it out, hold the mouse to cast."),
		               Hex(214, 176, 96), TEXT("Rod"), 1, 8, EKGRarity::Common, {TEXT("Tool"), TEXT("Fishing")},
		               Mesh(FishDir, TEXT("SM_KG_FishingRod")), 110.0f));
		Items.Add(Make(FKGItemIds::OldBoot, LOCTEXT("OldBoot", "Old Boot"),
		               LOCTEXT("OldBootDesc", "Size 44, left foot, full of the harbour. Somebody walked home barefoot."),
		               Hex(122, 92, 64), TEXT("Bt"), 3, 1, EKGRarity::Common, {TEXT("Junk"), TEXT("Sea")},
		               None, 26.0f, TEXT("Cube"), FVector(1.0f, 0.45f, 0.55f)));
		Items.Add(Make(FKGItemIds::MessageBottle, LOCTEXT("MessageBottle", "Bottle with a Message"),
		               LOCTEXT("MessageBottleDesc", "Corked, salty, and somebody's secret is inside."),
		               Hex(122, 196, 170), TEXT("Msg"), 3, 6, EKGRarity::Rare, {TEXT("Sea"), TEXT("Quest")},
		               Mesh(PropsDir, TEXT("Bottle_1")), 30.0f, TEXT("Cylinder"), FVector(0.35f, 0.35f, 1.0f)));
		Items.Add(Make(FKGItemIds::CoinPouch, LOCTEXT("CoinPouch", "Coin Pouch"),
		               LOCTEXT("CoinPouchDesc", "Soggy leather, heavy with somebody's savings."),
		               Hex(186, 132, 72), TEXT("$"), 5, 15, EKGRarity::Rare, {TEXT("Valuable"), TEXT("Sea")},
		               Mesh(PropsDir, TEXT("Pouch_Large")), 18.0f));
		// KG_DIG: digging + the underground (Source/KillGodot/Dig, Docs/01_GDD_Core.md section 16).
		Items.Add(Make(FKGItemIds::Shovel, LOCTEXT("Shovel", "Shovel"),
		               LOCTEXT("ShovelDesc", "Q to take it out, hold the mouse to dig. Graves are loud."),
		               Hex(168, 132, 92), TEXT("Sh"), 1, 6, EKGRarity::Common, {TEXT("Tool"), TEXT("Dig")},
		               Mesh(FishDir, TEXT("SM_KG_Shovel")), 110.0f));
		Items.Add(Make(FKGItemIds::CryptKey, LOCTEXT("CryptKey", "Crypt Key"),
		               LOCTEXT("CryptKeyDesc", "Black iron, cold as a tomb. The gate in the catacombs still remembers it."),
		               Hex(96, 104, 128), TEXT("CK"), 3, 20, EKGRarity::Rare, {TEXT("Tool"), TEXT("Key")},
		               Mesh(PropsDir, TEXT("Key_Metal")), 20.0f));
		Items.Add(Make(FKGItemIds::MapScrap, LOCTEXT("MapScrap", "Map Scrap"),
		               LOCTEXT("MapScrapDesc", "A torn corner of a treasure map. Three of them make a whole one."),
		               Hex(214, 190, 140), TEXT("Sc"), 9, 15, EKGRarity::Rare, {TEXT("Quest")},
		               Mesh(PropsDir, TEXT("Scroll_1")), 18.0f));
		Items.Add(Make(FKGItemIds::Skull, LOCTEXT("Skull", "Skull"),
		               LOCTEXT("SkullDesc", "Alas. Somebody knew him well."),
		               Hex(233, 223, 198), TEXT("Sk"), 5, 2, EKGRarity::Common, {TEXT("Junk")},
		               None, 18.0f));
		Items.Add(Make(FKGItemIds::MournerToken, LOCTEXT("MournerToken", "Mourner's Token"),
		               LOCTEXT("MournerTokenDesc", "A stamped brass token from the old crypt. The tailor pays well for these."),
		               Hex(196, 120, 232), TEXT("Tk"), 5, 80, EKGRarity::Epic, {TEXT("Valuable"), TEXT("Cosmetic")},
		               None, 10.0f, TEXT("Cylinder"), FVector(1.0f, 1.0f, 0.2f)));
		return Items;
	}
}

const TArray<FKGItemDef>& UKGItemCatalog::GetAll()
{
	static const TArray<FKGItemDef> Items = KGItemCatalogPrivate::Build();
	return Items;
}

const FKGItemDef* UKGItemCatalog::Find(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	return GetAll().FindByPredicate([ItemId](const FKGItemDef& Def) { return Def.Id == ItemId; });
}

bool UKGItemCatalog::GetItemDef(FName ItemId, FKGItemDef& OutDef)
{
	if (const FKGItemDef* Def = Find(ItemId))
	{
		OutDef = *Def;
		return true;
	}
	return false;
}

int32 UKGItemCatalog::GetStackSize(FName ItemId)
{
	const FKGItemDef* Def = Find(ItemId);
	return Def ? FMath::Max(1, Def->StackSize) : 0;
}

FText UKGItemCatalog::GetDisplayName(FName ItemId)
{
	const FKGItemDef* Def = Find(ItemId);
	return Def ? Def->DisplayName : FText::FromName(ItemId);
}

FName UKGItemCatalog::ResolveLoose(const FString& Text)
{
	const FString Wanted = Text.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
	for (const FKGItemDef& Def : GetAll())
	{
		const FString Id = Def.Id.ToString().Replace(TEXT("_"), TEXT(""));
		const FString Name = Def.DisplayName.ToString().Replace(TEXT(" "), TEXT(""));
		if (Id.Equals(Wanted, ESearchCase::IgnoreCase) || Name.Equals(Wanted, ESearchCase::IgnoreCase) ||
		    Id.Equals(TEXT("Fish") + Wanted, ESearchCase::IgnoreCase))
		{
			return Def.Id;
		}
	}
	return NAME_None;
}

FLinearColor UKGItemCatalog::GetRarityColor(EKGRarity Rarity)
{
	switch (Rarity)
	{
	case EKGRarity::Rare:
		return FLinearColor::FromSRGBColor(FColor(75, 139, 245));    // Kasaba blue
	case EKGRarity::Epic:
		return FLinearColor::FromSRGBColor(FColor(155, 89, 230));    // Nadir purple
	case EKGRarity::Legendary:
		return FLinearColor::FromSRGBColor(FColor(240, 80, 154));    // Efsane pink
	default:
		return FLinearColor::FromSRGBColor(FColor(157, 163, 170));   // Siradan grey
	}
}

FText UKGItemCatalog::GetRarityName(EKGRarity Rarity)
{
	switch (Rarity)
	{
	case EKGRarity::Rare:
		return LOCTEXT("RarityRare", "Rare");
	case EKGRarity::Epic:
		return LOCTEXT("RarityEpic", "Epic");
	case EKGRarity::Legendary:
		return LOCTEXT("RarityLegendary", "Legendary");
	default:
		return LOCTEXT("RarityCommon", "Common");
	}
}

#undef LOCTEXT_NAMESPACE
