#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Roles/KGRoleListGenerator.h"
#include "UObject/Package.h"
#include "World/KGChoreItem.h"

namespace KGWorldChoreTests
{
	/** Headless server world with AKGGameMode and the world chore spots (same harness as KillGodot.Chores.*). */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGWorldChoreTestWorld"));
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
			UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World);
			if (Sub)
			{
				Sub->SetupWorld(true);
			}
			return Sub && Sub->IsSetUp() && World->GetAuthGameMode<AKGGameMode>() != nullptr;
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
		AKGGameState* GS() const { return World->GetGameState<AKGGameState>(); }
		UKGWorldChoreSubsystem* Sub() const { return UKGWorldChoreSubsystem::Get(World); }

		AKGPlayerState* Player(int32 Index) const
		{
			return GS()->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS()->PlayerArray[Index]) : nullptr;
		}

		AKGCharacter* Body(int32 Index) const
		{
			AKGPlayerState* PS = Player(Index);
			AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
			if (!Controller)
			{
				return nullptr;
			}
			AKGCharacter* B = Cast<AKGCharacter>(Controller->GetPawn());
			if (!B)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				B = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(FVector(500.0 * Index, -3000.0, 200.0)), Params);
				if (B)
				{
					Controller->Possess(B);
				}
			}
			if (B)
			{
				UCharacterMovementComponent* Move = B->GetCharacterMovement();
				Move->SetComponentTickEnabled(false);
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
				UKGWorldChoreSubsystem::EnsureComponent(B);
			}
			return B;
		}

		int32 Anchor(const TCHAR* Id) const { return FKGWorldChoreCatalog::Get().AnchorIndex(FName(Id)); }

		/** Stands the body right at a spot (feet on it), facing it. */
		void StandAt(AKGCharacter* B, const TCHAR* Id, float Back = 60.0f) const
		{
			const FVector Spot = Sub()->SpotLocation(Anchor(Id));
			B->SetActorLocation(Spot + FVector(-Back, 0.0f, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
			B->SetActorRotation(FRotator::ZeroRotator);
		}
	};

	const FKGRoleInfo* FirstRole(EKGAlignment Alignment)
	{
		return FKGRoleListGenerator::GetDefaultCatalog().FindByPredicate([Alignment](const FKGRoleInfo& R) { return R.GetAlignment() == Alignment; });
	}

	bool Done(const AKGPlayerState* PS, FName Id)
	{
		const int32 i = PS ? PS->TaskIds.IndexOfByKey(Id) : INDEX_NONE;
		return i != INDEX_NONE && PS->TaskDone.IsValidIndex(i) && PS->TaskDone[i];
	}
}

// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreCatalogTest, "KillGodot.WorldChores.Catalog",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreCatalogTest::RunTest(const FString& Parameters)
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	FString Error;
	TestTrue(TEXT("Catalog validates: ") + Error, Cat.Validate(Error));
	TestTrue(TEXT("At least 10 original world chores (acceptance 2)"), Cat.Chores.Num() >= 10);
	TestEqual(TEXT("For Morrowmere v2"), Cat.MapName, FString(TEXT("L_Morrowmere_v2")));
	for (const TCHAR* Id : {TEXT("WaterRun"), TEXT("FishToMarket"), TEXT("BreadDelivery"), TEXT("Lamplighter"), TEXT("BellAndClock"),
	                        TEXT("Nets"), TEXT("Firewood"), TEXT("Letters"), TEXT("GrainToMill"), TEXT("LighthouseOil")})
	{
		const FKGWorldChoreDef* D = Cat.FindChore(FName(Id));
		if (TestNotNull(FString::Printf(TEXT("%s exists"), Id), D))
		{
			// Physical (a fetch and a use) but simple (user, 2026-09-25: "keep it simple but fun"): 2-3 steps.
			TestTrue(FString::Printf(TEXT("%s has 2-3 simple steps"), Id), D->NumSteps() >= 2 && D->NumSteps() <= 3);
			TestFalse(FString::Printf(TEXT("%s has a title"), Id), D->Title.IsEmpty());
			for (int32 v = 0; v < D->NumVariants(); ++v)
			{
				for (int32 s = 0; s < D->NumSteps(); ++s)
				{
					TestFalse(FString::Printf(TEXT("%s step %d label resolved"), Id, s), D->StepLabel(s, v).Contains(TEXT("$")));
				}
			}
		}
	}
	TestTrue(TEXT("Clock tower chore needs a climb"), Cat.FindChore(TEXT("BellAndClock"))->NeedsClimb());
	TestTrue(TEXT("Lighthouse chore needs a climb"), Cat.FindChore(TEXT("LighthouseOil"))->NeedsClimb());
	TestFalse(TEXT("Water run needs none"), Cat.FindChore(TEXT("WaterRun"))->NeedsClimb());
	TestEqual(TEXT("Water run: three destinations (fountain / bakery / inn)"), Cat.FindChore(TEXT("WaterRun"))->NumVariants(), 3);
	const FKGWorldItemDef* Crate = Cat.FindItem(TEXT("Crate"));
	const FKGWorldItemDef* Bucket = Cat.FindItem(TEXT("Bucket"));
	if (TestNotNull(TEXT("Crate item"), Crate) && TestNotNull(TEXT("Bucket item"), Bucket))
	{
		TestTrue(TEXT("The crate is a two-person carry"), Crate->bTwoPerson && Crate->IsHeavy());
		TestTrue(TEXT("The bucket holds water"), Bucket->bLiquid);
	}
	const FKGWorldAnchor* Box = Cat.FindAnchor(TEXT("box_H14"));
	TestTrue(TEXT("Letterboxes carry lore names"), Box && Box->Name == TEXT("MR THIMBLE"));
	TestTrue(TEXT("Troughs can be poisoned"), Cat.FindAnchor(TEXT("inn_trough"))->Sabotage == TEXT("poison"));
	TestTrue(TEXT("Lamps can be snuffed"), Cat.FindAnchor(TEXT("lamp_3"))->Sabotage == TEXT("snuff"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreRulesTest, "KillGodot.WorldChores.Rules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreRulesTest::RunTest(const FString& Parameters)
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const FKGWorldItemDef& Crate = *Cat.FindItem(TEXT("Crate"));
	const FKGWorldItemDef& Bucket = *Cat.FindItem(TEXT("Bucket"));
	TestEqual(TEXT("A crate alone: slow"), FKGWorldChoreRules::CarrySpeedFactor(Crate, 1), Crate.Speed);
	TestTrue(TEXT("...really slow"), Crate.Speed < 0.7f);
	TestEqual(TEXT("A crate with a helper: full walking speed"), FKGWorldChoreRules::CarrySpeedFactor(Crate, 2), 1.0f);
	TestEqual(TEXT("A bucket does not slow you"), FKGWorldChoreRules::CarrySpeedFactor(Bucket, 1), 1.0f);

	const float Walk = 320.0f;
	TestEqual(TEXT("Walking keeps the water"), FKGWorldChoreRules::SpillFill(1.0f, 1.0f, true, 310.0f, Walk, false, 1.0f), 1.0f);
	const float Sprint = FKGWorldChoreRules::SpillFill(1.0f, 2.0f, true, 580.0f, Walk, false, 1.0f);
	TestTrue(TEXT("Running spills"), Sprint < 0.75f && Sprint > 0.5f);
	TestTrue(TEXT("Jumping / falling spills more"), FKGWorldChoreRules::SpillFill(1.0f, 1.0f, true, 300.0f, Walk, true, 1.0f) < 0.7f);
	TestTrue(TEXT("A tipped bucket empties in about a second"), FKGWorldChoreRules::SpillFill(1.0f, 1.2f, false, 0.0f, Walk, false, 0.1f) < 0.01f);
	TestEqual(TEXT("A bucket standing on the ground keeps its water"), FKGWorldChoreRules::SpillFill(0.8f, 5.0f, false, 0.0f, Walk, false, 1.0f), 0.8f);

	// The deal: about 70 % world chores, bots never climb, replaced panel chores avoided, old levels unchanged.
	TArray<FName> Pool;
	for (const FKGWorldChoreDef& D : Cat.Chores)
	{
		Pool.Add(D.Id);
	}
	const TArray<FName> Panels = {TEXT("DrawWater"), TEXT("PostNotice"), TEXT("FileReports"), TEXT("BakeBread"), TEXT("PourAle"),
	                              TEXT("StockStall"), TEXT("TendGraves"), TEXT("ForgeNails"), TEXT("UnloadFish"), TEXT("ChopWood")};
	Pool.Append(Panels);
	FKGRng Rng(42);
	int32 World = 0, Total = 0, BotClimb = 0, Replaced = 0;
	for (int32 i = 0; i < 2000; ++i)
	{
		const bool bBot = (i % 2) == 1;
		const TArray<FName> Mine = FKGWorldChoreRules::Deal(Pool, Cat, bBot, 4, Rng);
		TestEqual(TEXT("Four chores each"), Mine.Num(), 4);
		TSet<FName> Replaces;
		for (const FName Id : Mine)
		{
			if (const FKGWorldChoreDef* D = Cat.FindChore(Id))
			{
				++World;
				BotClimb += bBot && D->NeedsClimb() ? 1 : 0;
				Replaces.Append(D->Replaces);
			}
			++Total;
		}
		for (const FName Id : Mine)
		{
			Replaced += Replaces.Contains(Id) ? 1 : 0;
		}
	}
	const float Share = static_cast<float>(World) / Total;
	AddInfo(FString::Printf(TEXT("World chore share over 2000 deals: %.3f"), Share));
	TestTrue(TEXT("About 70 % world chores (acceptance 4)"), Share > 0.66f && Share < 0.74f);
	TestEqual(TEXT("Bots are never dealt ladder chores"), BotClimb, 0);
	TestEqual(TEXT("No panel chore next to the world chore that replaces it"), Replaced, 0);

	FKGRng A(7), B(7);
	TArray<FName> Legacy = Panels;
	B.Shuffle(Legacy);
	Legacy.SetNum(4);
	TestEqual(TEXT("Levels without world chores: exactly the old deal"), FKGWorldChoreRules::Deal(Panels, Cat, false, 4, A), Legacy);

	const FKGWorldChoreDef& Water = *Cat.FindChore(TEXT("WaterRun"));
	TSet<int32> Seen;
	for (uint32 h = 0; h < 64; ++h)
	{
		Seen.Add(FKGWorldChoreRules::PickVariant(Water, h * 2654435761u));
	}
	TestEqual(TEXT("Players spread over the water run's destinations"), Seen.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreFlowTest, "KillGodot.WorldChores.WaterRunFlow",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreFlowTest::RunTest(const FString& Parameters)
{
	using namespace KGWorldChoreTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world with world chore spots"), Server.Create()))
	{
		return false;
	}
	TestNotNull(TEXT("Director spawned"), AKGWorldChoreDirector::Get(Server.World));
	int32 Stations = 0;
	for (TActorIterator<AKGWorldChoreStation> It(Server.World); It; ++It)
	{
		++Stations;
	}
	TestEqual(TEXT("One chore-list station per world chore"), Stations, FKGWorldChoreCatalog::Get().Chores.Num());
	Server.Run(TEXT("Bot.Fill 6"));
	Server.Run(TEXT("Match.Phase Day"));
	const FKGRoleInfo* Town = FirstRole(EKGAlignment::Town);
	for (int32 i = 0; i < 6; ++i)
	{
		Server.Run(FString::Printf(TEXT("Me.Role %s #%d"), *Town->RoleId.ToString(), i));
	}
	AKGPlayerState* PS = Server.Player(0);
	AKGCharacter* Body = Server.Body(0);
	UKGWorldChoreComponent* WC = UKGWorldChoreComponent::FindFor(Body);
	if (!TestNotNull(TEXT("World chore component on the body"), WC) || !TestNotNull(TEXT("Player"), PS))
	{
		return false;
	}
	const FName WaterRun(TEXT("WaterRun"));
	TestTrue(TEXT("Give the water run (fountain)"), WC->AuthGive(WaterRun, 0));
	const FKGWorldProgress* P = WC->FindProgress(WaterRun);
	TestTrue(TEXT("Progress starts at step 1"), P && P->Step == 0);

	// Validation: E from across the square does nothing; E at a spot of no current step does nothing.
	Server.StandAt(Body, TEXT("fountain_trough"), 2000.0f);
	TestFalse(TEXT("Too far away: refused"), WC->AuthInteract(Server.Anchor(TEXT("well"))));
	Server.StandAt(Body, TEXT("fountain_trough"));
	TestFalse(TEXT("The trough is not the first step"), WC->AuthInteract(Server.Anchor(TEXT("fountain_trough"))));

	// 1. Crank a full bucket up the well (a bot: the bucket goes straight into its arms).
	Server.StandAt(Body, TEXT("well"));
	TestTrue(TEXT("E at the well starts the crank"), WC->AuthInteract(Server.Anchor(TEXT("well"))));
	TestEqual(TEXT("The work ring shows it"), WC->GetDwell().Kind, EKGDwell::Work);
	WC->DebugTick(1.5f);
	TestEqual(TEXT("Cranking takes time: still step 1"), static_cast<int32>(WC->FindProgress(WaterRun)->Step), 0);
	WC->DebugTick(1.8f);
	P = WC->FindProgress(WaterRun);
	TestEqual(TEXT("Cranked: step 2"), static_cast<int32>(P->Step), 1);
	AKGChoreItem* Bucket = P->Item;
	if (!TestNotNull(TEXT("A real bucket item"), Bucket))
	{
		return false;
	}
	TestTrue(TEXT("...carried by the bot"), Bucket->IsCarriedBy(Body));
	TestEqual(TEXT("...full to the brim"), Bucket->GetFill(), 1.0f);

	// 2. Standing at the well with it does nothing: it goes into the fountain trough.
	WC->DebugTick(2.0f);
	TestEqual(TEXT("Still at the pour step"), static_cast<int32>(WC->FindProgress(WaterRun)->Step), 1);
	const float PrepBefore = Server.GS()->Preparation;
	Server.StandAt(Body, TEXT("fountain_trough"));
	WC->DebugTick(0.7f);
	TestEqual(TEXT("Pouring takes a moment"), WC->GetDwell().Kind, EKGDwell::Bring);
	WC->DebugTick(1.3f);
	TestTrue(TEXT("Water run done: ticked off the list"), Done(PS, WaterRun));
	TestNull(TEXT("Progress cleared"), WC->FindProgress(WaterRun));
	TestTrue(TEXT("Preparation grew"), Server.GS()->Preparation > PrepBefore);
	const FKGSpotState* Trough = AKGWorldChoreDirector::Get(Server.World)->GetSpot(Server.Anchor(TEXT("fountain_trough")));
	TestTrue(TEXT("The trough visibly holds water"), Trough && Trough->Level > 0.9f);
	TestEqual(TEXT("The bucket was emptied into it"), Bucket->GetFill(), 0.0f);

	// Spill: a new run; most of the water sloshes out on the way -> back to the well for a refill.
	TestTrue(TEXT("Again"), WC->AuthGive(WaterRun, 0));
	Server.StandAt(Body, TEXT("well"));
	WC->AuthInteract(Server.Anchor(TEXT("well")));
	WC->DebugTick(3.3f);
	TestEqual(TEXT("Cranked again"), static_cast<int32>(WC->FindProgress(WaterRun)->Step), 1);
	AKGChoreItem* Second = WC->FindProgress(WaterRun)->Item;
	Second->AuthSetFill(0.1f);   // spilled most of it (ran / tipped)
	WC->DebugTick(0.1f);
	TestEqual(TEXT("An empty bucket goes back to the well"), static_cast<int32>(WC->FindProgress(WaterRun)->Step), 0);
	TestTrue(TEXT("Refill at the well"), WC->AuthInteract(Server.Anchor(TEXT("well"))));
	WC->DebugTick(3.3f);
	AKGChoreItem* Third = WC->FindProgress(WaterRun)->Item;
	TestTrue(TEXT("...a fresh, full bucket"), IsValid(Third) && Third->GetFill() > 0.99f);
	Third->AuthConsume();   // lost (thrown in the sea)
	WC->DebugTick(0.1f);
	TestEqual(TEXT("A lost bucket goes back to the well too"), static_cast<int32>(WC->FindProgress(WaterRun)->Step), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreSocialTest, "KillGodot.WorldChores.FakeSabotageTwoCarry",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreSocialTest::RunTest(const FString& Parameters)
{
	using namespace KGWorldChoreTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world with world chore spots"), Server.Create()))
	{
		return false;
	}
	Server.Run(TEXT("Bot.Fill 6"));
	Server.Run(TEXT("Match.Phase Day"));
	const FKGRoleInfo* Impatient = FirstRole(EKGAlignment::Impatient);
	const FKGRoleInfo* Town = FirstRole(EKGAlignment::Town);
	Server.Run(FString::Printf(TEXT("Me.Role %s #0"), *Impatient->RoleId.ToString()));
	for (int32 i = 1; i < 6; ++i)
	{
		Server.Run(FString::Printf(TEXT("Me.Role %s #%d"), *Town->RoleId.ToString(), i));
	}
	AKGCharacter* KillerBody = Server.Body(0);
	AKGCharacter* VillagerBody = Server.Body(1);
	AKGCharacter* HelperBody = Server.Body(2);
	UKGWorldChoreComponent* Fake = UKGWorldChoreComponent::FindFor(KillerBody);
	UKGWorldChoreComponent* Real = UKGWorldChoreComponent::FindFor(VillagerBody);
	if (!TestNotNull(TEXT("Impatient body"), Fake) || !TestNotNull(TEXT("Villager body"), Real) || !TestNotNull(TEXT("Helper"), HelperBody))
	{
		return false;
	}
	const FName WaterRun(TEXT("WaterRun"));
	const int32 Trough = Server.Anchor(TEXT("inn_trough"));
	AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(Server.World);

	// Fake: the Impatient does the inn water run for real-looking results; nothing is counted.
	Fake->AuthGive(WaterRun, 2);
	Server.StandAt(KillerBody, TEXT("well"));
	TestTrue(TEXT("Fake cranks a bucket at the well"), Fake->AuthInteract(Server.Anchor(TEXT("well"))));
	Fake->DebugTick(3.3f);
	TestEqual(TEXT("Fake cranked it full like anyone"), static_cast<int32>(Fake->FindProgress(WaterRun)->Step), 1);
	const float Prep0 = Server.GS()->Preparation;
	Server.StandAt(KillerBody, TEXT("inn_trough"));
	Fake->DebugTick(2.5f);
	TestTrue(TEXT("Fake ticks their own list"), Done(Server.Player(0), WaterRun));
	TestEqual(TEXT("...but fills no preparation"), Server.GS()->Preparation, Prep0);
	TestTrue(TEXT("...and the trough looks filled to everyone"), Dir->GetSpot(Trough)->Level > 0.9f);

	// Sabotage: the Impatient poisons the filled trough (3 s at it).
	TestTrue(TEXT("The Impatient may poison a filled trough"), Fake->AuthInteract(Trough));
	TestEqual(TEXT("...standing there"), Fake->GetDwell().Kind, EKGDwell::Sabotage);
	Fake->DebugTick(3.2f);
	TestTrue(TEXT("Poisoned"), Dir->GetSpot(Trough)->bSpoiled);
	TestFalse(TEXT("A villager cannot poison anything"), Real->AuthInteract(Trough));

	// The next villager with water for that trough finds it poisoned: dump it first, then pour.
	Real->AuthGive(WaterRun, 2);
	Server.StandAt(VillagerBody, TEXT("well"));
	Real->AuthInteract(Server.Anchor(TEXT("well")));
	Real->DebugTick(3.3f);
	Server.StandAt(VillagerBody, TEXT("inn_trough"));
	Real->DebugTick(3.0f);
	TestFalse(TEXT("No pouring into poison"), Done(Server.Player(1), WaterRun));
	TestTrue(TEXT("E dumps the poisoned water"), Real->AuthInteract(Trough));
	TestEqual(TEXT("...a dump"), Real->GetDwell().Kind, EKGDwell::Dump);
	Real->DebugTick(3.2f);
	TestFalse(TEXT("Clean again"), Dir->GetSpot(Trough)->bSpoiled);
	Real->DebugTick(2.5f);
	TestTrue(TEXT("Then the pour counts"), Done(Server.Player(1), WaterRun));

	// Two-person carry: the fish crate is slow alone, full speed with a helper.
	const FName Fish(TEXT("FishToMarket"));
	Real->AuthGive(Fish);
	Server.StandAt(VillagerBody, TEXT("jetty_crates"));
	TestTrue(TEXT("Lift a crate"), Real->AuthInteract(Server.Anchor(TEXT("jetty_crates"))));
	Real->DebugTick(1.5f);
	AKGChoreItem* Crate = Real->FindProgress(Fish) ? Real->FindProgress(Fish)->Item.Get() : nullptr;
	if (TestNotNull(TEXT("A crate popped out"), Crate))
	{
		Crate->AuthDetach(FVector::ZeroVector);   // a player carries it with the physics handle instead of a bot hug
		Crate->AuthAddCarrier(VillagerBody);
		TestTrue(TEXT("Alone: slow"), AKGChoreItem::SpeedFactorFor(VillagerBody) < 0.7f);
		TestNull(TEXT("A second pair of hands does not snatch it"), Crate->AuthAddCarrier(HelperBody));
		TestEqual(TEXT("Together: full speed"), AKGChoreItem::SpeedFactorFor(VillagerBody), 1.0f);
		TestEqual(TEXT("...for both"), AKGChoreItem::SpeedFactorFor(HelperBody), 1.0f);
		// A light item is snatched instead: the bucket has one carrier.
		AKGChoreItem* Pail = AKGChoreItem::AuthSpawn(Server.World, TEXT("Bucket"), FVector(0, 0, 300), 0.0f, Server.Player(1), WaterRun);
		Pail->AuthAddCarrier(VillagerBody);
		TestTrue(TEXT("Grabbing someone's bucket snatches it"), Pail->AuthAddCarrier(KillerBody) == VillagerBody);
		TestTrue(TEXT("...the thief holds it now"), Pail->IsCarriedBy(KillerBody) && !Pail->IsCarriedBy(VillagerBody));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreThrowTest, "KillGodot.WorldChores.ChopAndThrowIn",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreThrowTest::RunTest(const FString& Parameters)
{
	using namespace KGWorldChoreTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world with world chore spots"), Server.Create()))
	{
		return false;
	}
	Server.Run(TEXT("Bot.Fill 4"));
	Server.Run(TEXT("Match.Phase Day"));
	const FKGRoleInfo* Town = FirstRole(EKGAlignment::Town);
	Server.Run(FString::Printf(TEXT("Me.Role %s #0"), *Town->RoleId.ToString()));
	AKGCharacter* Body = Server.Body(0);
	UKGWorldChoreComponent* WC = UKGWorldChoreComponent::FindFor(Body);
	if (!TestNotNull(TEXT("World chore component"), WC))
	{
		return false;
	}
	const FName Firewood(TEXT("Firewood"));
	WC->AuthGive(Firewood);
	const int32 Block = Server.Anchor(TEXT("chop_block"));
	Server.StandAt(Body, TEXT("chop_block"));
	// Three swings, one E each; the split logs pile up on the block.
	for (int32 Swing = 1; Swing <= 3; ++Swing)
	{
		TestTrue(FString::Printf(TEXT("Swing %d starts"), Swing), WC->AuthInteract(Block));
		WC->DebugTick(2.4f);
	}
	TestEqual(TEXT("Three logs on the block"), static_cast<int32>(AKGWorldChoreDirector::Get(Server.World)->GetSpot(Block)->Count), 3);
	const FKGWorldProgress* P = WC->FindProgress(Firewood);
	AKGChoreItem* Bundle = P ? P->Item.Get() : nullptr;
	if (!TestNotNull(TEXT("The bundle popped out after the third log"), Bundle))
	{
		return false;
	}
	TestEqual(TEXT("Now: carry it to the inn"), static_cast<int32>(P->Step), 1);
	// Thrown from four metres: it lands in the woodbox, the chore counts, and the thrower hears about it.
	Bundle->AuthDetach(FVector::ZeroVector);
	const FVector Box = Server.Sub()->SpotLocation(Server.Anchor(TEXT("forge_woodbox")));
	Server.StandAt(Body, TEXT("forge_woodbox"), 400.0f);
	Bundle->SetActorLocation(Box + FVector(0.0f, 0.0f, 30.0f), false, nullptr, ETeleportType::TeleportPhysics);
	WC->DebugTick(2.0f);
	TestTrue(TEXT("Firewood done: thrown into the forge woodbox"), Done(Server.Player(0), Firewood));
	TestTrue(TEXT("\"Nice throw!\""), WC->GetNotice().Contains(TEXT("Nice throw")));
	TestEqual(TEXT("The woodbox shows the bundle"), static_cast<int32>(AKGWorldChoreDirector::Get(Server.World)->GetSpot(Server.Anchor(TEXT("forge_woodbox")))->Count), 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGWorldChoreStormManorTest, "KillGodot.WorldChores.StormManor",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGWorldChoreStormManorTest::RunTest(const FString& Parameters)
{
	// SPRINT-018: map 2 has its own catalog (all 19 manor chores); Morrowmere stays the default one.
	const FKGWorldChoreCatalog* Manor = FKGWorldChoreCatalog::FindByMap(TEXT("L_StormManor"));
	if (!TestNotNull(TEXT("Storm Manor catalog"), Manor))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("Storm Manor catalog validates: ") + Error, Manor->Validate(Error));
	TestEqual(TEXT("All 19 manor chores"), Manor->Chores.Num(), 19);
	for (const FKGWorldChoreDef& D : Manor->Chores)
	{
		TestTrue(FString::Printf(TEXT("%s has 1-3 simple steps"), *D.Id.ToString()), D.NumSteps() >= 1 && D.NumSteps() <= 3);
		for (int32 v = 0; v < D.NumVariants(); ++v)
		{
			for (int32 st = 0; st < D.NumSteps(); ++st)
			{
				TestFalse(FString::Printf(TEXT("%s step %d label resolved"), *D.Id.ToString(), st), D.StepLabel(st, v).Contains(TEXT("$")));
			}
		}
	}
	TestEqual(TEXT("Morrowmere v2 is still the default catalog"), FKGWorldChoreCatalog::Get().MapName, FString(TEXT("L_Morrowmere_v2")));
	return true;
}

#endif
