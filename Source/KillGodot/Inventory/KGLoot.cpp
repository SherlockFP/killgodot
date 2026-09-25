#include "Inventory/KGLoot.h"
#include "GameFramework/Actor.h"
#include "Inventory/KGItemCatalog.h"
#include "Misc/Crc.h"
#include "Online/KGSnapshotComponent.h"
#include "World/KGPickup.h"

namespace KGLootPrivate
{
	FKGLootEntry E(FName Id, int32 Min, int32 Max, int32 Weight)
	{
		FKGLootEntry Entry;
		Entry.ItemId = Id;
		Entry.MinCount = Min;
		Entry.MaxCount = Max;
		Entry.Weight = Weight;
		return Entry;
	}

	FKGLootTable T(int32 MinRolls, int32 MaxRolls, int32 Nothing, std::initializer_list<FKGLootEntry> Entries)
	{
		FKGLootTable Table;
		Table.MinRolls = MinRolls;
		Table.MaxRolls = MaxRolls;
		Table.NothingWeight = Nothing;
		Table.Entries = Entries;
		return Table;
	}

	TMap<FName, FKGLootTable> Build()
	{
		using Ids = FKGItemIds;
		TMap<FName, FKGLootTable> Tables;
		Tables.Add(TEXT("Crate"), T(1, 2, 20, {E(Ids::Coin, 1, 5, 40), E(Ids::Apple, 1, 2, 15), E(Ids::Bread, 1, 1, 10),
		                                      E(Ids::Rope, 1, 1, 10), E(Ids::Candle, 1, 2, 10), E(Ids::Key, 1, 1, 5),
		                                      E(Ids::Bone, 1, 1, 5), E(Ids::OldRing, 1, 1, 2),
		                                      E(Ids::TreasureMap, 1, 1, 1)}));
		Tables.Add(TEXT("Barrel"), T(1, 1, 30, {E(Ids::Apple, 1, 3, 30), E(Ids::FishMackerel, 1, 2, 20),
		                                       E(Ids::FishCod, 1, 1, 10), E(Ids::Coin, 1, 3, 20), E(Ids::Rope, 1, 1, 5),
		                                       E(Ids::FishingRod, 1, 1, 3)}));
		Tables.Add(TEXT("Pot"), T(1, 1, 35, {E(Ids::Coin, 1, 4, 45), E(Ids::Pearl, 1, 1, 5), E(Ids::OldRing, 1, 1, 5),
		                                    E(Ids::Candle, 1, 1, 10)}));
		Tables.Add(TEXT("Chest"), T(2, 4, 0, {E(Ids::Coin, 5, 20, 40), E(Ids::Pearl, 1, 2, 15), E(Ids::OldRing, 1, 1, 10),
		                                     E(Ids::Key, 1, 1, 10), E(Ids::TreasureMap, 1, 1, 5), E(Ids::Candle, 1, 3, 10),
		                                     E(Ids::Rope, 1, 1, 10)}));
		Tables.Add(TEXT("Grave"), T(1, 2, 20, {E(Ids::Bone, 1, 3, 40), E(Ids::OldRing, 1, 1, 15), E(Ids::Coin, 1, 6, 20),
		                                      E(Ids::Key, 1, 1, 5), E(Ids::TreasureMap, 1, 1, 2)}));
		// Junk and treasure a bite can be instead of a fish (FKGFishingRules::RollBite; fish come from the species table).
		Tables.Add(TEXT("Fishing"), T(1, 1, 0, {E(Ids::OldBoot, 1, 1, 40), E(Ids::MessageBottle, 1, 1, 20),
		                                       E(Ids::Key, 1, 1, 14), E(Ids::CoinPouch, 1, 1, 12), E(Ids::Bone, 1, 1, 6),
		                                       E(Ids::Pearl, 1, 1, 6), E(Ids::TreasureMap, 1, 1, 2)}));
		// KG_DIG: dig spots, graves, buried chests, the catacombs and the well cellar (Source/KillGodot/Dig).
		// Intermediate dig stages roll *Shallow, the last stage the spot's own table (FKGDigRules::LootTable).
		Tables.Add(TEXT("DigShallow"), T(1, 1, 65, {E(Ids::Coin, 1, 2, 40), E(Ids::Bone, 1, 1, 20), E(Ids::OldBoot, 1, 1, 12),
		                                           E(Ids::MapScrap, 1, 1, 6), E(Ids::Skull, 1, 1, 4)}));
		Tables.Add(TEXT("DigMound"), T(1, 2, 10, {E(Ids::Coin, 2, 8, 40), E(Ids::Bone, 1, 2, 18), E(Ids::OldRing, 1, 1, 8),
		                                         E(Ids::Key, 1, 1, 6), E(Ids::MapScrap, 1, 1, 10), E(Ids::CoinPouch, 1, 1, 6),
		                                         E(Ids::OldBoot, 1, 1, 10), E(Ids::MournerToken, 1, 1, 1)}));
		Tables.Add(TEXT("DigX"), T(2, 3, 0, {E(Ids::Coin, 5, 15, 35), E(Ids::CoinPouch, 1, 1, 15), E(Ids::OldRing, 1, 1, 12),
		                                    E(Ids::Pearl, 1, 2, 10), E(Ids::MapScrap, 1, 1, 12), E(Ids::CryptKey, 1, 1, 6),
		                                    E(Ids::MournerToken, 1, 1, 3), E(Ids::TreasureMap, 1, 1, 2)}));
		Tables.Add(TEXT("DigGlint"), T(1, 1, 0, {E(Ids::Coin, 3, 10, 50), E(Ids::OldRing, 1, 1, 20), E(Ids::Pearl, 1, 1, 15),
		                                        E(Ids::CoinPouch, 1, 1, 10), E(Ids::MournerToken, 1, 1, 2)}));
		Tables.Add(TEXT("DigGraveShallow"), T(1, 1, 40, {E(Ids::Bone, 1, 2, 50), E(Ids::Skull, 1, 1, 15), E(Ids::Coin, 1, 3, 20),
		                                                E(Ids::Candle, 1, 1, 8)}));
		Tables.Add(TEXT("DigGrave"), T(2, 3, 5, {E(Ids::Bone, 1, 3, 30), E(Ids::Skull, 1, 1, 15), E(Ids::OldRing, 1, 1, 15),
		                                        E(Ids::Coin, 2, 10, 15), E(Ids::CryptKey, 1, 1, 8), E(Ids::MapScrap, 1, 1, 7),
		                                        E(Ids::MournerToken, 1, 1, 3), E(Ids::TreasureMap, 1, 1, 2)}));
		Tables.Add(TEXT("BuriedChest"), T(3, 5, 0, {E(Ids::Coin, 15, 40, 40), E(Ids::CoinPouch, 1, 2, 15), E(Ids::Pearl, 1, 3, 15),
		                                           E(Ids::OldRing, 1, 2, 12), E(Ids::MournerToken, 1, 1, 8), E(Ids::CryptKey, 1, 1, 4)}));
		Tables.Add(TEXT("CryptUrn"), T(1, 1, 35, {E(Ids::Bone, 1, 2, 30), E(Ids::Skull, 1, 1, 15), E(Ids::Coin, 1, 4, 25),
		                                         E(Ids::OldRing, 1, 1, 8), E(Ids::Candle, 1, 1, 10), E(Ids::MapScrap, 1, 1, 6)}));
		Tables.Add(TEXT("CryptVault"), T(4, 6, 0, {E(Ids::Coin, 20, 50, 35), E(Ids::Pearl, 1, 3, 15), E(Ids::OldRing, 1, 2, 12),
		                                          E(Ids::MournerToken, 1, 1, 10), E(Ids::CoinPouch, 1, 2, 15), E(Ids::TreasureMap, 1, 1, 8)}));
		Tables.Add(TEXT("Cellar"), T(2, 4, 0, {E(Ids::Apple, 1, 3, 20), E(Ids::Bread, 1, 1, 12), E(Ids::Rope, 1, 1, 10),
		                                      E(Ids::Candle, 1, 2, 12), E(Ids::Coin, 3, 10, 25), E(Ids::Key, 1, 1, 6),
		                                      E(Ids::MapScrap, 1, 1, 8), E(Ids::Shovel, 1, 1, 4)}));
		return Tables;
	}
}

void FKGLoot::Roll(FKGRng& Rng, const FKGLootTable& Table, TArray<FKGItemStack>& OutItems)
{
	int32 Total = FMath::Max(0, Table.NothingWeight);
	for (const FKGLootEntry& Entry : Table.Entries)
	{
		Total += UKGItemCatalog::Find(Entry.ItemId) ? FMath::Max(0, Entry.Weight) : 0;
	}
	if (Total <= 0)
	{
		return;
	}
	const int32 MinRolls = FMath::Max(0, Table.MinRolls);
	const int32 Rolls = Rng.RandRange(MinRolls, FMath::Max(MinRolls, Table.MaxRolls));
	for (int32 RollIndex = 0; RollIndex < Rolls; ++RollIndex)
	{
		int32 Pick = static_cast<int32>(Rng.NextBounded(static_cast<uint32>(Total)));
		if (Pick < Table.NothingWeight)
		{
			continue;
		}
		Pick -= FMath::Max(0, Table.NothingWeight);
		for (const FKGLootEntry& Entry : Table.Entries)
		{
			const int32 Weight = UKGItemCatalog::Find(Entry.ItemId) ? FMath::Max(0, Entry.Weight) : 0;
			if (Pick >= Weight)
			{
				Pick -= Weight;
				continue;
			}
			const int32 MinCount = FMath::Max(1, Entry.MinCount);
			const int32 Num = Rng.RandRange(MinCount, FMath::Max(MinCount, Entry.MaxCount));
			if (FKGItemStack* Existing = OutItems.FindByPredicate([&Entry](const FKGItemStack& S) { return S.ItemId == Entry.ItemId; }))
			{
				Existing->Count += Num;
			}
			else
			{
				OutItems.Emplace(Entry.ItemId, Num);
			}
			break;
		}
	}
}

const FKGLootTable* FKGLoot::FindTable(FName TableName)
{
	static const TMap<FName, FKGLootTable> Tables = KGLootPrivate::Build();
	return Tables.Find(TableName);
}

FKGRng FKGLoot::MakeRng(uint64 MatchSeed, const AActor* Source)
{
	uint32 Hash = 0x9E3779B9u;
	if (Source)
	{
		const UKGSnapshotComponent* Snapshot = Source->FindComponentByClass<UKGSnapshotComponent>();
		Hash = Snapshot && Snapshot->GetPersistentId().IsValid()
			       ? GetTypeHash(Snapshot->GetPersistentId())
			       : FCrc::StrCrc32(*Source->GetFName().ToString());
	}
	return FKGRng(MatchSeed ^ (static_cast<uint64>(Hash) << 21), static_cast<uint64>(Hash) | 1u);
}

TArray<AKGPickup*> FKGLoot::SpawnLoot(UObject* WorldContextObject, const TArray<FKGItemStack>& Items,
                                      const FVector& Origin, FKGRng& Rng, float Scatter)
{
	TArray<AKGPickup*> Spawned;
	const int32 Num = Items.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		const float Angle = (static_cast<float>(i) / FMath::Max(1, Num)) * UE_TWO_PI + Rng.FRand() * 0.6f;
		const float Radius = Num > 1 ? Scatter * (0.6f + 0.4f * Rng.FRand()) : 0.0f;
		const FVector Location = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
		if (AKGPickup* Pickup = AKGPickup::SpawnPickup(WorldContextObject, Items[i].ItemId, Items[i].Count, Location))
		{
			Spawned.Add(Pickup);
		}
	}
	return Spawned;
}

TArray<AKGPickup*> FKGLoot::RollAndSpawn(UObject* WorldContextObject, FName TableName, FKGRng& Rng,
                                         const FVector& Origin)
{
	TArray<FKGItemStack> Items;
	if (const FKGLootTable* Table = FindTable(TableName))
	{
		Roll(Rng, *Table, Items);
	}
	return SpawnLoot(WorldContextObject, Items, Origin, Rng);
}
