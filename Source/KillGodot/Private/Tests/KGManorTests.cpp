#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Core/KGGameMode.h"
#include "Core/KGRng.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Manor/KGHiddenCompartment.h"
#include "Manor/KGManorSubsystem.h"
#include "Manor/KGSecretPassage.h"
#include "UObject/Package.h"
#include "World/KGInteractable.h"

namespace KGManorTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const TCHAR* CatalogJson = TEXT(R"KGJSON({
	"map": "KGManorTest", "walk_speed": 3.2, "climb_speed": 2.6, "hub": [0, 0],
	"anchors": [ {"id": "a1", "kind": "clock", "label": "The clock", "at": [1, 2], "z": 0, "yaw": 0, "r": 1.6} ],
	"chores": [
		{"id": "c_plain", "title": "Wind the clock", "steps": [{"verb": "work", "at": "a1", "secs": 2, "label": "Wind"}]},
		{"id": "c_secret", "title": "Oil the hinge", "secret": "S_Bookcase", "reward_compartment": "HC_Brick",
		 "steps": [{"verb": "work", "at": "a1", "secs": 2, "label": "Oil"}]},
		{"id": "c_big", "title": "Ring the bell", "min_players": 6, "reward_secret": "S_Crypt",
		 "steps": [{"verb": "work", "at": "a1", "secs": 2, "label": "Ring"}]}
	]})KGJSON");

	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGManorTestWorld"));
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
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGManorChoreFieldsTest, "KillGodot.Manor.ChoreFields", KGManorTests::Flags)
bool FKGManorChoreFieldsTest::RunTest(const FString& Parameters)
{
	FKGWorldChoreCatalog Cat;
	FString Err;
	if (!TestTrue(FString::Printf(TEXT("Catalog parses (%s)"), *Err), Cat.Parse(KGManorTests::CatalogJson, Err)))
	{
		return false;
	}
	TestEqual(TEXT("3 chores"), Cat.Chores.Num(), 3);
	const FKGWorldChoreDef* Plain = Cat.FindChore(TEXT("c_plain"));
	const FKGWorldChoreDef* Secret = Cat.FindChore(TEXT("c_secret"));
	const FKGWorldChoreDef* Big = Cat.FindChore(TEXT("c_big"));
	if (!Plain || !Secret || !Big)
	{
		AddError(TEXT("Chores missing"));
		return false;
	}
	TestTrue(TEXT("Defaults: no secret"), Plain->SecretId.IsNone() && Plain->RewardSecret.IsNone() && Plain->RewardCompartment.IsNone());
	TestEqual(TEXT("Defaults: min_players 0"), Plain->MinPlayers, 0);
	TestEqual(TEXT("secret"), Secret->SecretId, FName(TEXT("S_Bookcase")));
	TestEqual(TEXT("reward_compartment"), Secret->RewardCompartment, FName(TEXT("HC_Brick")));
	TestEqual(TEXT("min_players"), Big->MinPlayers, 6);
	TestEqual(TEXT("reward_secret"), Big->RewardSecret, FName(TEXT("S_Crypt")));

	TArray<FName> Tags = {TEXT("KG_WingGate"), TEXT("KG_MinN_6"), TEXT("KG_Room_EastWing")};
	TestEqual(TEXT("KG_MinN_6 -> 6"), UKGManorSubsystem::MinPlayersOf(Tags), 6);
	TestEqual(TEXT("No MinN tag -> 0"), UKGManorSubsystem::MinPlayersOf({TEXT("KG_WingGate")}), 0);
	TestTrue(TEXT("PassesDeal: no minimum"), UKGManorSubsystem::PassesDeal(0, 1));
	TestTrue(TEXT("PassesDeal: enough players"), UKGManorSubsystem::PassesDeal(6, 6));
	TestFalse(TEXT("PassesDeal: too few"), UKGManorSubsystem::PassesDeal(6, 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGManorDealTest, "KillGodot.Manor.Deal", KGManorTests::Flags)
bool FKGManorDealTest::RunTest(const FString& Parameters)
{
	FKGWorldChoreCatalog Cat;
	FString Err;
	if (!TestTrue(TEXT("Catalog parses"), Cat.Parse(KGManorTests::CatalogJson, Err)))
	{
		return false;
	}
	const TArray<FName> Pool = {TEXT("c_plain"), TEXT("c_secret"), TEXT("c_big"), TEXT("panel_a"), TEXT("panel_b"), TEXT("panel_c")};
	TFunction<bool(const FKGWorldChoreDef&)> Saved = FKGWorldChoreRules::DealFilter;
	FKGWorldChoreRules::DealFilter = nullptr;

	auto Count = [&](FName Id, int32 Rounds)
	{
		FKGRng Rng(99);
		int32 N = 0;
		for (int32 i = 0; i < Rounds; ++i)
		{
			N += FKGWorldChoreRules::Deal(Pool, Cat, false, 4, Rng).Contains(Id) ? 1 : 0;
		}
		return N;
	};
	TestEqual(TEXT("Secret chores are never dealt"), Count(TEXT("c_secret"), 300), 0);
	TestTrue(TEXT("No filter: the min_players chore is dealt"), Count(TEXT("c_big"), 300) > 0);
	TestTrue(TEXT("No filter: the plain chore is dealt"), Count(TEXT("c_plain"), 300) > 0);

	int32 Players = 4;
	FKGWorldChoreRules::DealFilter = [&Players](const FKGWorldChoreDef& Def) { return UKGManorSubsystem::PassesDeal(Def.MinPlayers, Players); };
	TestEqual(TEXT("Filter (4 players): min_players 6 not dealt"), Count(TEXT("c_big"), 300), 0);
	TestTrue(TEXT("Filter (4 players): the plain chore still deals"), Count(TEXT("c_plain"), 300) > 0);
	Players = 6;
	TestTrue(TEXT("Filter (6 players): min_players 6 dealt again"), Count(TEXT("c_big"), 300) > 0);
	TestEqual(TEXT("Filter never deals secret chores"), Count(TEXT("c_secret"), 300), 0);

	// A deal with no world chores left (all filtered) is the legacy panel deal.
	FKGWorldChoreRules::DealFilter = [](const FKGWorldChoreDef&) { return false; };
	FKGRng A(7), B(7);
	TArray<FName> Panels = {TEXT("panel_a"), TEXT("panel_b"), TEXT("panel_c")};
	TArray<FName> Legacy = Panels;
	B.Shuffle(Legacy);
	TestEqual(TEXT("Everything filtered: the SPRINT-014 panel deal"), FKGWorldChoreRules::Deal(Pool, Cat, false, 4, A), Legacy);

	FKGWorldChoreRules::DealFilter = Saved;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGManorSecretsTest, "KillGodot.Manor.Secrets", KGManorTests::Flags)
bool FKGManorSecretsTest::RunTest(const FString& Parameters)
{
	KGManorTests::FServerWorld Server;
	if (!Server.Create())
	{
		AddWarning(TEXT("No engine world; skipped"));
		return true;
	}
	UKGManorSubsystem* Manor = UKGManorSubsystem::Get(Server.World);
	if (!TestNotNull(TEXT("Manor subsystem in a game world"), Manor))
	{
		return false;
	}
	TestTrue(TEXT("The subsystem installed the deal filter"), static_cast<bool>(FKGWorldChoreRules::DealFilter));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKGSecretPassage* A = Server.World->SpawnActor<AKGSecretPassage>(AKGSecretPassage::StaticClass(), FTransform(FVector(0.0, 0.0, 0.0)), Params);
	AKGSecretPassage* B = Server.World->SpawnActor<AKGSecretPassage>(AKGSecretPassage::StaticClass(), FTransform(FVector(2000.0, 0.0, 0.0)), Params);
	AKGSecretPassage* Other = Server.World->SpawnActor<AKGSecretPassage>(AKGSecretPassage::StaticClass(), FTransform(FVector(0.0, 2000.0, 0.0)), Params);
	if (!A || !B || !Other)
	{
		AddError(TEXT("Passages did not spawn"));
		return false;
	}
	A->SecretId = B->SecretId = TEXT("S_Bookcase");
	A->PassageId = TEXT("bk_a");
	A->TargetId = TEXT("bk_b");
	B->PassageId = TEXT("bk_b");
	B->TargetId = TEXT("bk_a");
	A->DiscoverPrompt = FText::FromString(TEXT("Pull the odd book"));
	Other->SecretId = TEXT("S_Crypt");
	TestFalse(TEXT("Starts hidden"), A->IsDiscovered());
	TestEqual(TEXT("Hidden prompt = DiscoverPrompt"), A->GetInteractPrompt_Implementation().ToString(), FString(TEXT("Pull the odd book")));

	AKGCharacter* Body = Server.World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(FVector(100.0, 0.0, 100.0)), Params);
	if (!TestNotNull(TEXT("Body"), Body))
	{
		return false;
	}
	IKGInteractable::Execute_Interact(A, Body);
	TestTrue(TEXT("E discovers this end"), A->IsDiscovered());
	TestTrue(TEXT("... and the other end of the same secret"), B->IsDiscovered());
	TestFalse(TEXT("... not another secret"), Other->IsDiscovered());
	TestEqual(TEXT("Discovered prompt = the passage prompt"), A->GetInteractPrompt_Implementation().ToString(),
	          A->AKGPassage::GetInteractPrompt_Implementation().ToString());
	TestEqual(TEXT("DiscoverSecret counts the ends"), Manor->DiscoverSecret(TEXT("S_Crypt"), nullptr), 1);
	TestTrue(TEXT("Reward path discovers"), Other->IsDiscovered());
	TestEqual(TEXT("Unknown secret: 0 ends"), Manor->DiscoverSecret(TEXT("S_Nope"), nullptr), 0);

	AKGHiddenCompartment* Brick = Server.World->SpawnActor<AKGHiddenCompartment>(AKGHiddenCompartment::StaticClass(), FTransform(FVector(500.0, 0.0, 0.0)), Params);
	if (!TestNotNull(TEXT("Compartment"), Brick))
	{
		return false;
	}
	Brick->CompartmentId = TEXT("HC_Brick");
	Brick->ClueText = TEXT("a torn glove");
	Brick->SearchPrompt = FText::FromString(TEXT("Wiggle the brick"));
	TestFalse(TEXT("Closed"), Brick->IsOpen());
	TestEqual(TEXT("Closed prompt"), Brick->GetInteractPrompt_Implementation().ToString(), FString(TEXT("Wiggle the brick")));
	TestTrue(TEXT("OpenCompartment by id"), Manor->OpenCompartment(TEXT("HC_Brick"), Body));
	TestTrue(TEXT("Open"), Brick->IsOpen());
	TestEqual(TEXT("Open prompt shows the clue"), Brick->GetInteractPrompt_Implementation().ToString(), FString(TEXT("Clue: a torn glove")));
	TestFalse(TEXT("Opening twice fails"), Manor->OpenCompartment(TEXT("HC_Brick"), Body));
	TestFalse(TEXT("Unknown compartment"), Manor->OpenCompartment(TEXT("HC_Nope"), Body));
	return true;
}

#endif
