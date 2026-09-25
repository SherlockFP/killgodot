#include "Misc/AutomationTest.h"
#include "Cosmetics/KGCosmeticCatalog.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGInventoryStackTest, "KillGodot.Inventory.Stacking",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGInventoryStackTest::RunTest(const FString& Parameters)
{
	UKGInventoryComponent* Inv = NewObject<UKGInventoryComponent>();
	Inv->SetCapacity(3);
	TestEqual(TEXT("Unknown items are refused"), Inv->AddItem(TEXT("NotAnItem"), 5), 0);
	TestEqual(TEXT("Coins stack to 999 in one slot"), Inv->AddItem(FKGItemIds::Coin, 500), 500);
	TestEqual(TEXT("Top up the same stack"), Inv->AddItem(FKGItemIds::Coin, 499), 499);
	TestEqual(TEXT("One slot used"), Inv->GetUsedSlots(), 1);
	TestEqual(TEXT("Overflow opens a new slot"), Inv->AddItem(FKGItemIds::Coin, 10), 10);
	TestEqual(TEXT("Gold = coins"), Inv->GetGold(), 1009);
	TestEqual(TEXT("Golden carp does not stack"), Inv->AddItem(FKGItemIds::FishGoldenCarp, 2), 1);
	TestEqual(TEXT("Full: nothing fits"), Inv->RoomFor(FKGItemIds::Rope), 0);
	TestEqual(TEXT("Remove across stacks"), Inv->RemoveItem(FKGItemIds::Coin, 1005), 1005);
	TestEqual(TEXT("Remaining coins"), Inv->Count(FKGItemIds::Coin), 4);
	TestTrue(TEXT("Has"), Inv->Has(FKGItemIds::FishGoldenCarp));
	TestFalse(TEXT("Has not"), Inv->Has(FKGItemIds::Pearl));
	for (const FKGItemEntry& Entry : Inv->GetEntries())
	{
		TestTrue(TEXT("Slots stay inside capacity"), Entry.Slot >= 0 && Entry.Slot < 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGInventoryMoveTest, "KillGodot.Inventory.MoveBetweenContainers",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGInventoryMoveTest::RunTest(const FString& Parameters)
{
	UKGInventoryComponent* Pockets = NewObject<UKGInventoryComponent>();
	UKGInventoryComponent* Chest = NewObject<UKGInventoryComponent>();
	Pockets->SetCapacity(2);
	Chest->SetCapacity(16);
	Chest->AddItem(FKGItemIds::Apple, 25);   // 10 + 10 + 5
	Chest->AddItem(FKGItemIds::Pearl, 3);
	TestEqual(TEXT("Chest apples"), Chest->Count(FKGItemIds::Apple), 25);

	const FKGItemEntry* First = Chest->FindSlot(0);
	TestNotNull(TEXT("Slot 0 filled"), First);
	TestEqual(TEXT("Move a whole stack"), Chest->MoveSlotTo(Pockets, 0, First ? First->Count : 0), 10);
	TestEqual(TEXT("Move by id, limited by room"), Chest->MoveTo(Pockets, FKGItemIds::Apple, 50), 10);
	TestEqual(TEXT("Pockets are full"), Pockets->RoomFor(FKGItemIds::Pearl), 0);
	TestEqual(TEXT("Nothing lost"), Chest->Count(FKGItemIds::Apple) + Pockets->Count(FKGItemIds::Apple), 25);
	TestEqual(TEXT("Move back one"), Pockets->MoveTo(Chest, FKGItemIds::Apple, 1), 1);
	TestEqual(TEXT("Still nothing lost"), Chest->Count(FKGItemIds::Apple) + Pockets->Count(FKGItemIds::Apple), 25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGLootTest, "KillGodot.Inventory.LootDeterminism",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGLootTest::RunTest(const FString& Parameters)
{
	const FKGLootTable* Chest = FKGLoot::FindTable(TEXT("Chest"));
	TestNotNull(TEXT("Chest table exists"), Chest);
	if (!Chest)
	{
		return false;
	}
	for (uint64 Seed = 1; Seed < 40; ++Seed)
	{
		FKGRng A(Seed);
		FKGRng B(Seed);
		TArray<FKGItemStack> RollA;
		TArray<FKGItemStack> RollB;
		FKGLoot::Roll(A, *Chest, RollA);
		FKGLoot::Roll(B, *Chest, RollB);
		TestEqual(TEXT("Same seed, same number of stacks"), RollA.Num(), RollB.Num());
		TestTrue(TEXT("Chest rolls at least one item"), RollA.Num() >= 1);
		for (int32 i = 0; i < FMath::Min(RollA.Num(), RollB.Num()); ++i)
		{
			TestEqual(TEXT("Same item"), RollA[i].ItemId, RollB[i].ItemId);
			TestEqual(TEXT("Same count"), RollA[i].Count, RollB[i].Count);
			TestTrue(TEXT("Known item"), UKGItemCatalog::IsValidItem(RollA[i].ItemId));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGCosmeticPlacementTest, "KillGodot.Cosmetics.Placement",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGCosmeticPlacementTest::RunTest(const FString& Parameters)
{
	// Villager Head bone reference pose (component space): the import carries a x100 root scale.
	const FTransform Head(FRotator(0.0f, 0.0f, 91.7f), FVector(0.0f, -1.7f, 160.0f), FVector(100.0f));
	const FVector P(3.0f, 8.0f, 175.0f);

	// Bind-pose part (identity placement): mesh vertex -> bone local -> back through the bone == the same point.
	const FTransform Rel = UKGCosmeticsComponent::ComputeBoneRelative(FTransform::Identity, Head, false);
	const FVector Round = Head.TransformPosition(Rel.TransformPosition(P));
	TestTrue(TEXT("Identity placement round-trips"), Round.Equals(P, 0.05f));

	// Mirrored copy lands on the other side of the body.
	const FTransform Mirror = UKGCosmeticsComponent::ComputeBoneRelative(FTransform::Identity, Head, true);
	const FVector Mirrored = Head.TransformPosition(Mirror.TransformPosition(P));
	TestTrue(TEXT("Mirror flips X only"), Mirrored.Equals(FVector(-P.X, P.Y, P.Z), 0.05f));

	// Every catalog entry is well formed.
	for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
	{
		TestTrue(*FString::Printf(TEXT("%s has a mesh"), *Def.Id.ToString()), Def.Mesh.IsValid());
		TestTrue(*FString::Printf(TEXT("%s has a bone"), *Def.Id.ToString()), !Def.AttachBone.IsNone());
		TestTrue(*FString::Printf(TEXT("%s price >= 0"), *Def.Id.ToString()), Def.Price >= 0);
	}
	return true;
}

#endif
