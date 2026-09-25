#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Dig/KGDigComponent.h"
#include "Dig/KGDigManager.h"
#include "Dig/KGDigSubsystem.h"
#include "Dig/KGDigTypes.h"
#include "Dig/KGKeyGate.h"
#include "Dig/KGPassage.h"
#include "Dig/KGUndergroundInfo.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "UObject/Package.h"

namespace KGDigTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Flat stub ground (no world): z 500 everywhere except a "pond" we must never dig in. */
	bool FlatProbe(const FVector2D& XY, FVector& Out)
	{
		if (FVector2D::Distance(XY, FVector2D(-11800.0, -4800.0)) < 400.0)
		{
			return false;
		}
		Out = FVector(XY.X, XY.Y, 150.0);   // inside every zone's height window (the beach wants 0.4..2.6 m)
		return true;
	}

	struct FGen
	{
		TArray<FKGDigSpot> Spots;
		TArray<FKGDigSpot> Buried;
	};

	FGen Generate(uint64 Seed, const TArray<FVector>& Avoid = {})
	{
		FGen G;
		FKGRng Rng(Seed, 0xD16u);
		uint16 NextId = 1;
		FKGDigRules::Generate(Rng, FKGDigRules::MorrowmereV2Zones(), [](const FVector2D& XY, FVector& Out) { return FlatProbe(XY, Out); },
		                      Avoid, G.Spots, G.Buried, NextId);
		return G;
	}

	bool SameSpots(const TArray<FKGDigSpot>& A, const TArray<FKGDigSpot>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (A[i].Id != B[i].Id || A[i].Kind != B[i].Kind || A[i].YawQ != B[i].YawQ ||
			    !FVector(A[i].Location).Equals(FVector(B[i].Location), 0.1))
			{
				return false;
			}
		}
		return true;
	}

	/** Headless server world running AKGGameMode (like KillGodot.Chores.ServerValidation). */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGDigTestWorld"));
			World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
			if (!World)
			{
				return false;
			}
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GetTransientPackage()));
			World->GetWorldSettings()->DefaultGameMode = AKGGameMode::StaticClass();
			const FURL URL;
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			return World->GetAuthGameMode<AKGGameMode>() != nullptr;
		}

		~FServerWorld()
		{
			if (World && GEngine)
			{
				World->EndPlay(EEndPlayReason::Quit);
				World->BeginTearingDown();
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					It->Destroy();
				}
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		FKGDevResult Run(const FString& Line) const { return FKGDev::Execute({World, nullptr}, Line); }

		AKGPlayerState* Player(int32 Index) const
		{
			const AKGGameState* GS = World->GetGameState<AKGGameState>();
			return GS && GS->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS->PlayerArray[Index]) : nullptr;
		}

		AKGCharacter* BotBody(int32 Index, const FVector& At) const
		{
			AKGPlayerState* PS = Player(Index);
			AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
			if (!Controller)
			{
				return nullptr;
			}
			AKGCharacter* Body = Cast<AKGCharacter>(Controller->GetPawn());
			if (!Body)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Body = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(At), Params);
				if (Body)
				{
					Controller->Possess(Body);
				}
			}
			if (Body)
			{
				Body->SetActorLocationAndRotation(At, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
				UCharacterMovementComponent* Move = Body->GetCharacterMovement();
				Move->SetComponentTickEnabled(false);
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
				UKGPlayerExtrasSubsystem::EnsurePlayerComponents(PS);
				UKGDigSubsystem::EnsureDig(Body);
			}
			return Body;
		}
	};

	FVector Feet(const AKGCharacter* C)
	{
		return C->GetActorLocation() - FVector(0.0, 0.0, C->GetSimpleCollisionHalfHeight());
	}

	FVector Eye(const AKGCharacter* C)
	{
		return C->GetFirstPersonCamera()->GetComponentLocation();
	}

	int32 CountAll(const UKGInventoryComponent* Pockets)
	{
		int32 N = 0;
		for (const FKGItemEntry& E : Pockets->GetEntries())
		{
			N += E.Count;
		}
		return N;
	}
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDigRulesTest, "KillGodot.Dig.Rules", KGDigTests::Flags)

bool FKGDigRulesTest::RunTest(const FString& Parameters)
{
	for (const EKGDigKind Kind : {EKGDigKind::Mound, EKGDigKind::XMark, EKGDigKind::Glint, EKGDigKind::Grave, EKGDigKind::Treasure})
	{
		const uint8 Max = FKGDigRules::StagesFor(Kind);
		TestTrue(*FString::Printf(TEXT("%s: 1..4 stages"), FKGDigRules::KindName(Kind)), Max >= 1 && Max <= 4);
		TestTrue(*FString::Printf(TEXT("%s: a short hold"), FKGDigRules::KindName(Kind)),
		         FKGDigRules::HoldSeconds(Kind) >= 0.5f && FKGDigRules::HoldSeconds(Kind) <= 2.0f);
		const FName Final = FKGDigRules::LootTable(Kind, Max, Max);
		TestNotNull(*FString::Printf(TEXT("%s: final table %s exists"), FKGDigRules::KindName(Kind), *Final.ToString()), FKGLoot::FindTable(Final));
		for (uint8 S = 1; S < Max; ++S)
		{
			const FName Mid = FKGDigRules::LootTable(Kind, S, Max);
			TestTrue(*FString::Printf(TEXT("%s: stage %d table"), FKGDigRules::KindName(Kind), S), Mid.IsNone() || FKGLoot::FindTable(Mid) != nullptr);
		}
	}
	TestTrue(TEXT("Graves are the loud ones"), FKGDigRules::IsLoud(EKGDigKind::Grave) && !FKGDigRules::IsLoud(EKGDigKind::Mound));
	TestFalse(TEXT("No digging in a meeting"), FKGDigRules::CanDigInPhase(EKGPhase::Meeting));
	TestFalse(TEXT("No digging in a trial"), FKGDigRules::CanDigInPhase(EKGPhase::Trial));
	TestTrue(TEXT("Digging at night"), FKGDigRules::CanDigInPhase(EKGPhase::Night));
	// Every item the dig/underground tables can give exists.
	for (const TCHAR* Table : {TEXT("DigShallow"), TEXT("DigMound"), TEXT("DigX"), TEXT("DigGlint"), TEXT("DigGraveShallow"),
	                           TEXT("DigGrave"), TEXT("BuriedChest"), TEXT("CryptUrn"), TEXT("CryptVault"), TEXT("Cellar")})
	{
		const FKGLootTable* T = FKGLoot::FindTable(Table);
		if (TestNotNull(*FString::Printf(TEXT("Table %s"), Table), T))
		{
			for (const FKGLootEntry& E : T->Entries)
			{
				TestNotNull(*FString::Printf(TEXT("%s: item %s exists"), Table, *E.ItemId.ToString()), UKGItemCatalog::Find(E.ItemId));
			}
		}
	}
	const FKGItemDef* Shovel = UKGItemCatalog::Find(FKGItemIds::Shovel);
	TestTrue(TEXT("The shovel is a tool, one per slot"), Shovel && Shovel->HasTag(TEXT("Tool")) && Shovel->StackSize == 1);
	TestTrue(TEXT("Map scraps stack to at least 3"), UKGItemCatalog::GetStackSize(FKGItemIds::MapScrap) >= FKGDigRules::ScrapsPerMap);
	const FKGItemDef* Token = UKGItemCatalog::Find(FKGItemIds::MournerToken);
	TestTrue(TEXT("Mourner's token is a rare cosmetic token"), Token && Token->Rarity >= EKGRarity::Epic && Token->HasTag(TEXT("Cosmetic")));
	int32 Left = 0;
	TestEqual(TEXT("7 scraps = 2 maps"), FKGDigRules::MapsFromScraps(7, Left), 2);
	TestEqual(TEXT("...1 scrap left"), Left, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDigGenerationTest, "KillGodot.Dig.SpotGeneration", KGDigTests::Flags)

bool FKGDigGenerationTest::RunTest(const FString& Parameters)
{
	using namespace KGDigTests;
	const FGen A = Generate(1234);
	const FGen B = Generate(1234);
	const FGen C = Generate(98765);
	TestTrue(TEXT("Same seed -> the same spots (host migration / replays)"), SameSpots(A.Spots, B.Spots) && SameSpots(A.Buried, B.Buried));
	TestFalse(TEXT("Another seed -> other spots"), SameSpots(A.Spots, C.Spots));
	int32 Want[5] = {};
	for (const FKGDigZoneDef& Z : FKGDigRules::MorrowmereV2Zones())
	{
		Want[static_cast<int32>(EKGDigKind::Mound)] += Z.Mounds;
		Want[static_cast<int32>(EKGDigKind::XMark)] += Z.XMarks;
		Want[static_cast<int32>(EKGDigKind::Glint)] += Z.Glints;
		Want[static_cast<int32>(EKGDigKind::Treasure)] += Z.Treasures;
	}
	int32 Got[5] = {};
	for (const FKGDigSpot& S : A.Spots)
	{
		++Got[static_cast<int32>(S.Kind)];
	}
	for (const FKGDigSpot& S : A.Buried)
	{
		++Got[static_cast<int32>(S.Kind)];
	}
	TestEqual(TEXT("Mounds placed"), Got[0], Want[0]);
	TestEqual(TEXT("X marks placed"), Got[1], Want[1]);
	TestEqual(TEXT("Glints placed"), Got[2], Want[2]);
	TestEqual(TEXT("Buried chests placed"), Got[4], Want[4]);
	TestEqual(TEXT("Buried chests are hidden (not in the public list)"),
	          A.Spots.FilterByPredicate([](const FKGDigSpot& S) { return S.Kind == EKGDigKind::Treasure; }).Num(), 0);
	TestTrue(TEXT("The zones cover the graveyard, beach, farm, forest and orchard"),
	         A.Spots.ContainsByPredicate([](const FKGDigSpot& S) { return S.Zone == EKGDigZone::Graveyard; }) &&
	         A.Spots.ContainsByPredicate([](const FKGDigSpot& S) { return S.Zone == EKGDigZone::Beach; }) &&
	         A.Spots.ContainsByPredicate([](const FKGDigSpot& S) { return S.Zone == EKGDigZone::Farm; }) &&
	         A.Spots.ContainsByPredicate([](const FKGDigSpot& S) { return S.Zone == EKGDigZone::Forest; }) &&
	         A.Spots.ContainsByPredicate([](const FKGDigSpot& S) { return S.Zone == EKGDigZone::Orchard; }));
	TArray<FKGDigSpot> All = A.Spots;
	All.Append(A.Buried);
	TSet<uint16> Ids;
	bool bSpacing = true;
	bool bInZone = true;
	bool bPond = true;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		Ids.Add(All[i].Id);
		bPond &= FVector2D::Distance(FVector2D(All[i].Location), FVector2D(-11800.0, -4800.0)) >= 400.0;
		bInZone &= FKGDigRules::MorrowmereV2Zones().ContainsByPredicate([&](const FKGDigZoneDef& Z) { return Z.Contains(FVector2D(All[i].Location)); });
		bInZone &= All[i].MaxStage == FKGDigRules::StagesFor(All[i].Kind) && All[i].Stage == 0;
		for (int32 j = i + 1; j < All.Num(); ++j)
		{
			bSpacing &= FVector::Dist2D(FVector(All[i].Location), FVector(All[j].Location)) >= FKGDigRules::MinSpacing - 0.5f;
		}
	}
	TestEqual(TEXT("Unique ids"), Ids.Num(), All.Num());
	TestFalse(TEXT("Id 0 is never used"), Ids.Contains(0));
	TestTrue(TEXT("Spots keep their spacing"), bSpacing);
	TestTrue(TEXT("Every spot lies in its zone, untouched"), bInZone);
	TestTrue(TEXT("Nothing where the probe says no (water)"), bPond);
	// Graves keep clear of the generated spots.
	TArray<FVector> Graves = {FVector(-3000.0, -6000.0, 1400.0), FVector(-2800.0, -5800.0, 1400.0)};
	const FGen D = Generate(1234, Graves);
	bool bClear = true;
	for (const FKGDigSpot& S : D.Spots)
	{
		for (const FVector& G : Graves)
		{
			bClear &= FVector::Dist2D(FVector(S.Location), G) >= FKGDigRules::GraveClearance - 0.5f;
		}
	}
	TestTrue(TEXT("Graves keep their clearance"), bClear);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDigLootTest, "KillGodot.Dig.LootDeterminism", KGDigTests::Flags)

bool FKGDigLootTest::RunTest(const FString& Parameters)
{
	auto RollStage = [](uint64 Seed, uint16 Id, EKGDigKind Kind, uint8 Stage)
	{
		TArray<FKGItemStack> Items;
		const FName Table = FKGDigRules::LootTable(Kind, Stage, FKGDigRules::StagesFor(Kind));
		if (const FKGLootTable* T = Table.IsNone() ? nullptr : FKGLoot::FindTable(Table))
		{
			FKGRng Rng = FKGDigRules::StageRng(Seed, Id, Stage);
			FKGLoot::Roll(Rng, *T, Items);
		}
		return Items;
	};
	auto Same = [](const TArray<FKGItemStack>& A, const TArray<FKGItemStack>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (A[i].ItemId != B[i].ItemId || A[i].Count != B[i].Count)
			{
				return false;
			}
		}
		return true;
	};
	bool bSame = true;
	bool bVaries = false;
	int32 EmptyFinalX = 0;
	int32 FinalChestItems = 0;
	for (uint16 Id = 1; Id < 60; ++Id)
	{
		for (const EKGDigKind Kind : {EKGDigKind::Mound, EKGDigKind::XMark, EKGDigKind::Grave, EKGDigKind::Treasure})
		{
			const uint8 Max = FKGDigRules::StagesFor(Kind);
			for (uint8 S = 1; S <= Max; ++S)
			{
				bSame &= Same(RollStage(777, Id, Kind, S), RollStage(777, Id, Kind, S));
				bVaries |= !Same(RollStage(777, Id, Kind, S), RollStage(778, Id, Kind, S));
			}
		}
		EmptyFinalX += RollStage(777, Id, EKGDigKind::XMark, 3).Num() == 0 ? 1 : 0;
		for (const FKGItemStack& S : RollStage(777, Id, EKGDigKind::Treasure, 4))
		{
			FinalChestItems += S.Count;
		}
	}
	TestTrue(TEXT("Same seed + spot + stage -> the same loot"), bSame);
	TestTrue(TEXT("Another match seed -> other loot"), bVaries);
	TestEqual(TEXT("An X mark always pays out"), EmptyFinalX, 0);
	TestTrue(TEXT("Buried chests are rich (> 15 items per chest on average)"), FinalChestItems / 59 > 15);
	TestEqual(TEXT("A buried chest gives nothing until the lid shows"), RollStage(777, 5, EKGDigKind::Treasure, 2).Num(), 0);
	// Treasure-map targets: in range, stable, different maps of one player point at different chests.
	TestEqual(TEXT("No chests left -> no target"), FKGDigRules::TreasureFor(123u, 0, 0), static_cast<int32>(INDEX_NONE));
	bool bRange = true;
	for (uint32 H = 0; H < 500; H += 7)
	{
		const int32 T = FKGDigRules::TreasureFor(H, 0, 4);
		bRange &= T >= 0 && T < 4 && T == FKGDigRules::TreasureFor(H, 0, 4);
	}
	TestTrue(TEXT("Targets in range and stable"), bRange);
	TestNotEqual(TEXT("A second map shows a second chest"), FKGDigRules::TreasureFor(10u, 0, 4), FKGDigRules::TreasureFor(10u, 1, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDigServerTest, "KillGodot.Dig.ServerValidation", KGDigTests::Flags)

bool FKGDigServerTest::RunTest(const FString& Parameters)
{
	using namespace KGDigTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world"), Server.Create()))
	{
		return false;
	}
	Server.Run(TEXT("Bot.Fill 2"));
	AKGCharacter* Body = Server.BotBody(0, FVector(0.0, 0.0, 200.0));
	AKGCharacter* Other = Server.BotBody(1, FVector(3000.0, 0.0, 200.0));
	UKGDigComponent* Dig = UKGDigComponent::FindFor(Body);
	UKGDigComponent* OtherDig = UKGDigComponent::FindFor(Other);
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Body);
	AKGDigManager* Manager = AKGDigManager::Get(Server.World, true);
	if (!TestNotNull(TEXT("Dig component"), Dig) || !TestNotNull(TEXT("Pockets"), Pockets) || !TestNotNull(TEXT("Manager"), Manager) ||
	    !TestNotNull(TEXT("Second digger"), OtherDig))
	{
		return false;
	}
	const FVector Ground = Feet(Body);
	const int32 XIndex = Manager->AddSpot(EKGDigKind::XMark, Ground + FVector(150.0, 0.0, 0.0), 0.0f);
	const uint16 XId = Manager->GetSpots()[XIndex].Id;
	auto Aim = [&](const FVector& At) { return (At - Eye(Body)).GetSafeNormal(); };

	// Shovel rules.
	EKGDigNotice Why = EKGDigNotice::None;
	TestFalse(TEXT("No shovel item: refused"), Dig->ServerTrySetShovel(true, &Why));
	TestTrue(TEXT("...NoShovel"), Why == EKGDigNotice::NoShovel);
	Pockets->AddItem(FKGItemIds::Shovel, 1);
	TestTrue(TEXT("With a shovel it comes out"), Dig->ServerTrySetShovel(true));
	TestTrue(TEXT("Shovel out (replicated flag)"), Dig->IsShovelOutOnServer());

	// Aim / reach validation.
	TestFalse(TEXT("Looking at the sky: refused"), Dig->ServerTryBeginDig(XId, Eye(Body), FVector::UpVector, &Why));
	const int32 FarIndex = Manager->AddSpot(EKGDigKind::Mound, Ground + FVector(1200.0, 0.0, 0.0), 0.0f);
	TestFalse(TEXT("A spot 12 m away: refused"), Dig->ServerTryBeginDig(Manager->GetSpots()[FarIndex].Id, Eye(Body),
	                                                                         Aim(FVector(Manager->GetSpots()[FarIndex].Location)), &Why));
	TestTrue(TEXT("...TooFar"), Why == EKGDigNotice::TooFar);

	// Dig the X out: 3 stages, held.
	const int32 Before = CountAll(Pockets);
	TestTrue(TEXT("Digging the X starts"), Dig->ServerTryBeginDig(XId, Eye(Body), Aim(FVector(Manager->GetSpots()[XIndex].Location)), &Why));
	TestTrue(TEXT("Digging is public"), Dig->GetAction().bDigging && Dig->GetAction().SpotId == XId);
	Dig->DebugTickServer(0.5f);
	TestEqual(TEXT("Too early: still stage 0"), static_cast<int32>(Manager->GetSpots()[XIndex].Stage), 0);
	Dig->DebugTickServer(0.7f);
	TestEqual(TEXT("One hold = one stage"), static_cast<int32>(Manager->GetSpots()[XIndex].Stage), 1);
	TestEqual(TEXT("A second digger cannot share the hole"),
	          OtherDig->ServerTryBeginDig(XId, Eye(Other), Aim(FVector(Manager->GetSpots()[XIndex].Location)), &Why), false);
	Dig->DebugTickServer(2.4f);
	TestTrue(TEXT("Held on: dug out"), Manager->GetSpots()[XIndex].IsDugOut());
	TestFalse(TEXT("Digging stops when it is empty"), Dig->GetAction().bDigging);
	TestTrue(TEXT("The X paid out into the pockets"), CountAll(Pockets) > Before);
	TestEqual(TEXT("Result card: last stage"), static_cast<int32>(Dig->GetLastResult().Stage), 3);
	TestFalse(TEXT("A dug-out hole refuses"), Dig->ServerTryBeginDig(XId, Eye(Body), Aim(FVector(Manager->GetSpots()[XIndex].Location)), &Why));
	TestTrue(TEXT("...DugOut"), Why == EKGDigNotice::DugOut);

	// Interrupts: walking off.
	const int32 MoundIndex = Manager->AddSpot(EKGDigKind::Mound, Ground + FVector(0.0, 150.0, 0.0), 0.0f);
	const uint16 MoundId = Manager->GetSpots()[MoundIndex].Id;
	TestTrue(TEXT("Mound starts"), Dig->ServerTryBeginDig(MoundId, Eye(Body), Aim(FVector(Manager->GetSpots()[MoundIndex].Location))));
	Body->SetActorLocation(Body->GetActorLocation() + FVector(0.0, -700.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	Dig->DebugTickServer(0.1f);
	TestFalse(TEXT("Walking away stops the dig"), Dig->GetAction().bDigging);
	Body->SetActorLocation(Body->GetActorLocation() + FVector(0.0, 700.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);

	// Meetings put the shovel away.
	Server.Run(TEXT("Match.Phase Meeting"));
	Dig->DebugTickServer(0.1f);
	TestFalse(TEXT("A meeting stows the shovel"), Dig->IsShovelOutOnServer());
	TestFalse(TEXT("...and it will not come out"), Dig->ServerTrySetShovel(true, &Why));
	Server.Run(TEXT("Match.Phase Day"));
	// The meeting gathered everyone at the gallows: back to the dig site.
	Body->SetActorLocationAndRotation(FVector(0.0, 0.0, 200.0), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	TestTrue(TEXT("Day: out again"), Dig->ServerTrySetShovel(true));

	// A grave: 4 loud stages, the open grave stays.
	const int32 GraveIndex = Manager->AddSpot(EKGDigKind::Grave, Ground + FVector(-150.0, 0.0, 0.0), 90.0f, EKGDigZone::Graveyard);
	const uint16 GraveId = Manager->GetSpots()[GraveIndex].Id;
	TestTrue(TEXT("Grave digging starts"), Dig->ServerTryBeginDig(GraveId, Eye(Body), Aim(FVector(Manager->GetSpots()[GraveIndex].Location))));
	Dig->DebugTickServer(FKGDigRules::HoldSeconds(EKGDigKind::Grave) * 4.0f + 0.3f);
	TestTrue(TEXT("The grave is dug open (and stays open for everyone)"), Manager->GetSpots()[GraveIndex].IsDugOut());

	// Treasure: 3 scraps -> a map -> the X points at the buried chest -> digging there reveals it.
	const FVector ChestAt = Ground + FVector(0.0, -160.0, 0.0);
	Manager->AddBuried(ChestAt);
	Pockets->AddItem(FKGItemIds::MapScrap, 3);
	Dig->ServerRefreshTreasure();
	TestEqual(TEXT("Three scraps became a map"), Pockets->Count(FKGItemIds::TreasureMap), 1);
	TestEqual(TEXT("...the scraps are used"), Pockets->Count(FKGItemIds::MapScrap), 0);
	TestEqual(TEXT("The map shows one X"), Dig->GetTreasureMarks().Num(), 1);
	TestTrue(TEXT("...at the buried chest"), Dig->GetTreasureMarks().Num() == 1 &&
	                                         FVector2D::Distance(Dig->GetTreasureMarks()[0], FVector2D(ChestAt)) < 1.0);
	const int32 PublicBefore = Manager->GetSpots().Num();
	TestTrue(TEXT("Digging bare ground at the X"), Dig->ServerTryBeginDig(0, Eye(Body), Aim(ChestAt), &Why));
	TestEqual(TEXT("...reveals the chest as a public hole"), Manager->GetSpots().Num(), PublicBefore + 1);
	Dig->DebugTickServer(FKGDigRules::HoldSeconds(EKGDigKind::Treasure) * 4.0f + 0.3f);
	TestTrue(TEXT("The chest is dug up"), Manager->GetSpots().Last().IsDugOut());
	TestEqual(TEXT("The map is used up"), Pockets->Count(FKGItemIds::TreasureMap), 0);
	TestEqual(TEXT("No more buried chests"), Manager->GetBuried().Num(), 0);
	TestFalse(TEXT("Bare ground without a chest: nothing here"), Dig->ServerTryBeginDig(0, Eye(Body), Aim(Ground + FVector(100.0, 100.0, 0.0)), &Why));

	// Loot determinism on the server path: the same seed and spot give the same stage loot.
	TArray<FKGItemStack> A1;
	TArray<FKGItemStack> A2;
	const int32 T1 = Manager->AddSpot(EKGDigKind::Glint, Ground + FVector(600.0, 600.0, 0.0), 0.0f);
	Manager->DebugSetStage(T1, 0);
	Manager->ApplyStage(T1, A1);
	Manager->DebugSetStage(T1, 0);
	Manager->ApplyStage(T1, A2);
	TestTrue(TEXT("ApplyStage rolls from the match seed + spot + stage"), A1.Num() > 0 && A1.Num() == A2.Num() && A1[0].ItemId == A2[0].ItemId && A1[0].Count == A2[0].Count);

	// Dead villagers put the shovel away.
	Server.Run(TEXT("Me.Kill #0"));
	Dig->DebugTickServer(0.1f);
	TestFalse(TEXT("The dead drop their shovel"), Dig->IsShovelOutOnServer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDigUndergroundTest, "KillGodot.Dig.Underground", KGDigTests::Flags)

bool FKGDigUndergroundTest::RunTest(const FString& Parameters)
{
	using namespace KGDigTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world"), Server.Create()))
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKGUndergroundInfo* Under = Server.World->SpawnActor<AKGUndergroundInfo>(AKGUndergroundInfo::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("Underground info"), Under))
	{
		return false;
	}
	Under->Volumes.Add(FBox(FVector(-4400.0, -2600.0, 100.0), FVector(-2400.0, -900.0, 600.0)));
	FKGMapRegion Cellar;
	Cellar.Id = TEXT("well_cellar");
	Cellar.Name = FText::FromString(TEXT("Well Cellar"));
	Cellar.Layer = 2;
	Cellar.Polygon = {FVector2D(-4400.0, -2600.0), FVector2D(-2400.0, -2600.0), FVector2D(-2400.0, -900.0), FVector2D(-4400.0, -900.0)};
	Under->Regions.Add(Cellar);
	TestTrue(TEXT("Inside the cellar box = below ground"), AKGUndergroundInfo::IsBelowGround(Server.World, FVector(-3300.0, -1500.0, 300.0)));
	TestFalse(TEXT("The well court above is not"), AKGUndergroundInfo::IsBelowGround(Server.World, FVector(-3300.0, -1500.0, 900.0)));
	TestEqual(TEXT("Region name for the toast"), Under->RegionNameAt(FVector(-3300.0, -1500.0, 300.0)), FString(TEXT("Well Cellar")));
	TestTrue(TEXT("Surface: no region"), Under->RegionNameAt(FVector(0.0, 0.0, 300.0)).IsEmpty());

	// A two-way passage: the well top <-> the cellar shaft.
	AKGPassage* Top = Server.World->SpawnActor<AKGPassage>(AKGPassage::StaticClass(), FTransform(FVector(-3313.0, -1484.0, 800.0)), Params);
	AKGPassage* Shaft = Server.World->SpawnActor<AKGPassage>(AKGPassage::StaticClass(), FTransform(FVector(-3313.0, -1484.0, 180.0)), Params);
	Top->PassageId = TEXT("WellTop");
	Top->TargetId = TEXT("WellShaft");
	Top->ArrivalLocal = FVector(0.0, 140.0, 100.0);
	Shaft->PassageId = TEXT("WellShaft");
	Shaft->TargetId = TEXT("WellTop");
	Shaft->ArrivalLocal = FVector(0.0, 0.0, 250.0);
	Shaft->bArriveFalling = true;
	Server.Run(TEXT("Bot.Fill 1"));
	AKGCharacter* Body = Server.BotBody(0, FVector(-3313.0, -1344.0, 900.0));
	if (!TestNotNull(TEXT("A villager"), Body))
	{
		return false;
	}
	TestTrue(TEXT("Down the well"), Top->TravelThrough(Body));
	TestTrue(TEXT("...arrives in the shaft, below ground"), AKGUndergroundInfo::IsBelowGround(Server.World, Body->GetActorLocation()));
	TestTrue(TEXT("...on its way down the ladder"), Body->GetCharacterMovement()->MovementMode == MOVE_Falling);
	TestFalse(TEXT("No instant bounce back (cooldown)"), Shaft->TravelThrough(Body));
	TestTrue(TEXT("FindById"), AKGPassage::FindById(Server.World, TEXT("WellShaft")) == Shaft);

	// The catacomb gate wants a crypt key, keeps it, then opens and closes for anyone.
	AKGKeyGate* Gate = Server.World->SpawnActor<AKGKeyGate>(AKGKeyGate::StaticClass(), FTransform(FVector(-3000.0, -6000.0, 180.0)), Params);
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Body);
	if (TestNotNull(TEXT("Gate"), Gate) && TestNotNull(TEXT("Pockets"), Pockets))
	{
		IKGInteractable::Execute_Interact(Gate, Body);
		TestFalse(TEXT("No key: still locked"), Gate->IsUnlocked() || Gate->IsOpen());
		Pockets->AddItem(FKGItemIds::CryptKey, 1);
		IKGInteractable::Execute_Interact(Gate, Body);
		TestTrue(TEXT("The key opens it"), Gate->IsUnlocked() && Gate->IsOpen());
		TestEqual(TEXT("...and stays in the lock"), Pockets->Count(FKGItemIds::CryptKey), 0);
		IKGInteractable::Execute_Interact(Gate, Body);
		TestFalse(TEXT("Unlocked gates close again"), Gate->IsOpen());
	}
	return true;
}

#endif
