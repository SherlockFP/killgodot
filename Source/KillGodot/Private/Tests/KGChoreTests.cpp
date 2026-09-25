#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Chores/KGChoreComponent.h"
#include "Chores/KGChoreFx.h"
#include "Chores/KGChoreTypes.h"
#include "Chores/UI/KGMinigame.h"
#include "Combat/KGHealthComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Roles/KGRoleListGenerator.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "World/KGTaskStation.h"

namespace KGChoreTests
{
	/** Headless server world running AKGGameMode (like KillGodot.Dev.*), with chore stations and bot bodies. */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGChoreTestWorld"));
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
			return World->GetAuthGameMode<AKGGameMode>() != nullptr && World->GetGameState<AKGGameState>() != nullptr;
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

		AKGPlayerState* Player(int32 Index) const
		{
			return GS()->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS()->PlayerArray[Index]) : nullptr;
		}

		AKGTaskStation* AddStation(FName Id, const FVector& At) const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKGTaskStation* Station = World->SpawnActor<AKGTaskStation>(AKGTaskStation::StaticClass(), FTransform(At), Params);
			if (Station)
			{
				Station->TaskId = Id;
				Station->TaskName = Id.ToString();
			}
			return Station;
		}

		/** Bot #Index with a parked body (headless worlds have no player starts). */
		AKGCharacter* BotBody(int32 Index) const
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
				Body = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(FVector(500.0 * Index, 0.0, 200.0)), Params);
				if (Body)
				{
					Controller->Possess(Body);
				}
			}
			if (Body)
			{
				UCharacterMovementComponent* Move = Body->GetCharacterMovement();
				Move->SetComponentTickEnabled(false);
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			return Body;
		}
	};

	const FKGRoleInfo* FirstRole(EKGAlignment Alignment)
	{
		return FKGRoleListGenerator::GetDefaultCatalog().FindByPredicate([Alignment](const FKGRoleInfo& R) { return R.GetAlignment() == Alignment; });
	}

	/** Server ticks for the chore component (session clock + interrupt checks). */
	void Tick(UKGChoreComponent* Chores, float Seconds, float Step = 0.05f)
	{
		for (float T = 0.0f; T < Seconds - KINDA_SMALL_NUMBER; T += Step)
		{
			Chores->TickComponent(Step, LEVELTICK_All, nullptr);
		}
	}

	/** Plays every stage of the open session with plausible server time. Returns the last verdict. */
	EKGChoreVerdict PlayThrough(UKGChoreComponent* Chores)
	{
		EKGChoreVerdict Last = EKGChoreVerdict::NoSession;
		const FKGChoreDef* Def = FKGChoreCatalog::Find(Chores->GetSessionChore());
		while (Def && Chores->HasSession())
		{
			const int32 Stage = Chores->GetSessionStage();
			Chores->DebugAddServerTime(Def->StageMinSeconds[Stage] + 0.5f);
			Last = Chores->AuthStageDone(Def->Id, Stage, Chores->GetSessionToken());
			if (Last != EKGChoreVerdict::Accepted)
			{
				break;
			}
		}
		return Last;
	}

	int32 CountTaskDone(const AKGPlayerState* PS, FName Id)
	{
		const int32 i = PS ? PS->TaskIds.IndexOfByKey(Id) : INDEX_NONE;
		return i != INDEX_NONE && PS->TaskDone.IsValidIndex(i) && PS->TaskDone[i] ? 1 : 0;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreCatalogTest, "KillGodot.Chores.Catalog",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FKGChoreDef>& All = FKGChoreCatalog::GetAll();
	TestEqual(TEXT("18 chores + 4 optional"), All.Num(), 22);
	int32 Optional = 0;
	int32 Visual = 0;
	for (const FKGChoreDef& Def : All)
	{
		const FString Id = Def.Id.ToString();
		Optional += Def.bOptional ? 1 : 0;
		Visual += Def.IsVisual() ? 1 : 0;
		TestTrue(Id + TEXT(": has stages"), Def.NumStages() >= 1 && Def.NumStages() <= 4);
		TestEqual(Id + TEXT(": one floor per stage"), Def.StageMinSeconds.Num(), Def.NumStages());
		for (const float Floor : Def.StageMinSeconds)
		{
			TestTrue(Id + TEXT(": positive floor (no instant completion)"), Floor >= 0.9f);
		}
		TestTrue(Id + TEXT(": floor total is sane"), Def.MinTotalSeconds() >= 3.0f && Def.MinTotalSeconds() <= 20.0f);
		TSharedPtr<FKGMinigame> Game(FKGMinigameFactory::Create(Def.Id).Release());
		if (TestTrue(Id + TEXT(": has a minigame"), Game.IsValid()))
		{
			TestEqual(Id + TEXT(": minigame stages match the catalog"), Game->NumStages(), Def.NumStages());
		}
	}
	TestEqual(TEXT("4 optional chores"), Optional, 4);
	TestTrue(TEXT("At least 6 visual chores"), Visual >= 6);
	for (const TCHAR* Must : {TEXT("RingBell"), TEXT("FuelLighthouse"), TEXT("BakeBread"), TEXT("PostNotice")})
	{
		const FKGChoreDef* Def = FKGChoreCatalog::Find(FName(Must));
		TestTrue(FString(Must) + TEXT(" is a visual chore"), Def && Def->IsVisual());
	}
	TestNull(TEXT("Unknown chore has no minigame"), FKGMinigameFactory::Create(TEXT("NotAChore")).Get());

	// Every chore of the map layout has an entry (the station ids come from this file).
	const FString LayoutPath = FPaths::ProjectDir() / TEXT("Tools/Level/morrowmere_layout_v2.json");
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *LayoutPath))
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (TestTrue(TEXT("Layout parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
		{
			const TArray<TSharedPtr<FJsonValue>>* Tasks = nullptr;
			if (TestTrue(TEXT("Layout has tasks"), Root->TryGetArrayField(TEXT("tasks"), Tasks) && Tasks))
			{
				for (const TSharedPtr<FJsonValue>& V : *Tasks)
				{
					const FString Id = V->AsObject()->GetStringField(TEXT("id"));
					TestNotNull(FString::Printf(TEXT("Layout chore %s is in the catalog"), *Id), FKGChoreCatalog::Find(FName(*Id)));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreRulesTest, "KillGodot.Chores.Rules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreRulesTest::RunTest(const FString& Parameters)
{
	using V = EKGChoreVerdict;
	const FKGChoreDef* Def = FKGChoreCatalog::Find(TEXT("DrawWater"));
	if (!TestNotNull(TEXT("DrawWater exists"), Def))
	{
		return false;
	}
	const FName Id = Def->Id;
	const float Floor0 = Def->StageMinSeconds[0];
	auto Check = [&](FName Chore, int32 Stage, int32 Token, FName SChore, int32 SStage, int32 SToken, float Secs)
	{
		return FKGChoreRules::CheckStage(*Def, Chore, Stage, Token, SChore, SStage, SToken, Secs);
	};
	TestTrue(TEXT("Plausible stage accepted"), Check(Id, 0, 7, Id, 0, 7, Floor0 + 0.1f) == V::Accepted);
	TestTrue(TEXT("Just inside the jitter slack accepted"), Check(Id, 0, 7, Id, 0, 7, Floor0 * FKGChoreRules::TimingSlack + 0.01f) == V::Accepted);
	TestTrue(TEXT("Instant completion refused"), Check(Id, 0, 7, Id, 0, 7, 0.05f) == V::TooFast);
	TestTrue(TEXT("Below the slack refused"), Check(Id, 0, 7, Id, 0, 7, Floor0 * FKGChoreRules::TimingSlack - 0.05f) == V::TooFast);
	TestTrue(TEXT("No session"), Check(Id, 0, 7, NAME_None, 0, 7, 99.0f) == V::NoSession);
	TestTrue(TEXT("Other chore"), Check(TEXT("RingBell"), 0, 7, Id, 0, 7, 99.0f) == V::WrongChore);
	TestTrue(TEXT("Stale token (older session)"), Check(Id, 0, 6, Id, 0, 7, 99.0f) == V::WrongToken);
	TestTrue(TEXT("Skipping a stage refused"), Check(Id, 1, 7, Id, 0, 7, 99.0f) == V::WrongStage);
	TestTrue(TEXT("Repeating a stage refused"), Check(Id, 0, 7, Id, 1, 7, 99.0f) == V::WrongStage);
	TestTrue(TEXT("Stage past the end refused"), Check(Id, 2, 7, Id, 2, 7, 99.0f) == V::WrongStage);

	TestTrue(TEXT("Chores during the day"), FKGChoreRules::PhaseAllowsChores(EKGPhase::Day));
	TestTrue(TEXT("Chores at night"), FKGChoreRules::PhaseAllowsChores(EKGPhase::Night));
	TestFalse(TEXT("No chores in a meeting"), FKGChoreRules::PhaseAllowsChores(EKGPhase::Meeting));
	TestFalse(TEXT("No chores on trial"), FKGChoreRules::PhaseAllowsChores(EKGPhase::Trial));
	TestFalse(TEXT("No chores in the lobby"), FKGChoreRules::PhaseAllowsChores(EKGPhase::Lobby));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreMinigameLogicTest, "KillGodot.Chores.MinigameLogic",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreMinigameLogicTest::RunTest(const FString& Parameters)
{
	// Every minigame: starts at any stage, survives input and auto-play for its whole run, advances, poses.
	for (const FName Id : FKGMinigameFactory::GetIds())
	{
		const FString Name = Id.ToString();
		TUniquePtr<FKGMinigame> Game = FKGMinigameFactory::Create(Id);
		if (!TestTrue(Name + TEXT(": created"), Game.IsValid()))
		{
			continue;
		}
		Game->Start(4242, 0);
		for (int32 Stage = 0; Stage < Game->NumStages(); ++Stage)
		{
			TestEqual(Name + TEXT(": on the expected stage"), Game->GetStage(), Stage);
			TestFalse(Name + TEXT(": instruction text"), Game->GetInstruction().IsEmpty());
			TestFalse(Name + TEXT(": a fresh stage is not solved"), Game->IsStageSolved());
			// Poke it like a player would, then let it play itself for a while.
			Game->HostMove(FVector2f(320.0f, 200.0f), FVector2f(4.0f, 2.0f));
			Game->HostPress(FVector2f(320.0f, 200.0f));
			Game->HostMove(FVector2f(330.0f, 150.0f), FVector2f(10.0f, -50.0f));
			Game->HostRelease(FVector2f(330.0f, 150.0f));
			Game->HostKey(EKeys::SpaceBar);
			for (int32 Frame = 0; Frame < 120 && !Game->IsStageSolved(); ++Frame)
			{
				Game->HostTick(1.0f / 30.0f, true);
			}
			Game->DrainFeedback();
			Game->ForceSolve();
			TestTrue(Name + TEXT(": solvable"), Game->IsStageSolved());
			if (Stage + 1 < Game->NumStages())
			{
				Game->AdvanceStage();
			}
		}
		// Resume a later stage (saved progress) and pose it for screenshots.
		TUniquePtr<FKGMinigame> Resumed = FKGMinigameFactory::Create(Id);
		Resumed->Start(7, Resumed->NumStages() - 1);
		TestEqual(Name + TEXT(": resumes at the saved stage"), Resumed->GetStage(), Resumed->NumStages() - 1);
		Resumed->HostTick(0.5f, false);
		Resumed->DebugPose();
		Resumed->RetryStage();
		TestFalse(Name + TEXT(": retry clears the solve"), Resumed->IsStageSolved());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreServerValidationTest, "KillGodot.Chores.ServerValidation",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreServerValidationTest::RunTest(const FString& Parameters)
{
	using namespace KGChoreTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world"), Server.Create()))
	{
		return false;
	}
	// Four stations = everyone is dealt exactly these four chores.
	const FName DrawWater(TEXT("DrawWater")), RingBell(TEXT("RingBell")), ChopWood(TEXT("ChopWood")), MendNets(TEXT("MendNets"));
	Server.AddStation(DrawWater, FVector(0, 0, 100));
	AKGTaskStation* BellStation = Server.AddStation(RingBell, FVector(800, 0, 100));
	Server.AddStation(ChopWood, FVector(0, 800, 100));
	Server.AddStation(MendNets, FVector(800, 800, 100));
	Server.Run(TEXT("Bot.Fill 6"));
	Server.Run(TEXT("Match.Phase Day"));
	const FKGRoleInfo* Town = FirstRole(EKGAlignment::Town);
	if (!TestNotNull(TEXT("A town role"), Town))
	{
		return false;
	}
	for (int32 i = 0; i < 6; ++i)
	{
		Server.Run(FString::Printf(TEXT("Me.Role %s #%d"), *Town->RoleId.ToString(), i));
	}
	Server.Run(TEXT("Chore.Reset all"));
	AKGPlayerState* PS = Server.Player(0);
	AKGCharacter* Body = Server.BotBody(0);
	UKGChoreComponent* Chores = Body ? Body->GetChores() : nullptr;
	if (!TestNotNull(TEXT("Bot body with a chore component"), Chores) || !TestNotNull(TEXT("Player state"), PS))
	{
		return false;
	}
	TestEqual(TEXT("Four chores dealt"), PS->TaskIds.Num(), 4);
	TestTrue(TEXT("DrawWater is on the list"), PS->HasOpenTask(DrawWater));

	// Open + anti-cheat timing.
	TestTrue(TEXT("Opens a chore on the list"), Chores->AuthOpen(DrawWater, nullptr));
	TestTrue(TEXT("Session open"), Chores->HasSession() && Chores->GetSessionChore() == DrawWater);
	TestFalse(TEXT("Real chore, not fake"), Chores->IsSessionFake());
	TestTrue(TEXT("Working flag is public"), Chores->IsWorking());
	const int32 Token1 = Chores->GetSessionToken();
	TestTrue(TEXT("Instant stage report refused"), Chores->AuthStageDone(DrawWater, 0, Token1) == EKGChoreVerdict::TooFast);
	TestEqual(TEXT("Still on stage 0"), Chores->GetSessionStage(), 0);
	Tick(Chores, 2.0f);
	TestTrue(TEXT("Still too fast after 2 s"), Chores->AuthStageDone(DrawWater, 0, Token1) == EKGChoreVerdict::TooFast);
	Tick(Chores, 5.5f);
	TestTrue(TEXT("Plausible stage accepted"), Chores->AuthStageDone(DrawWater, 0, Token1) == EKGChoreVerdict::Accepted);
	TestEqual(TEXT("On stage 1"), Chores->GetSessionStage(), 1);
	TestEqual(TEXT("Progress saved"), Chores->GetSavedStage(DrawWater), 1);
	TestTrue(TEXT("Skipping ahead refused"), Chores->AuthStageDone(DrawWater, 2, Token1) == EKGChoreVerdict::WrongStage);
	TestEqual(TEXT("Not on the list yet"), CountTaskDone(PS, DrawWater), 0);

	// Leave and come back: resumes at stage 1 with a new token; the old session's reports are stale.
	Chores->AuthClose(EKGChoreClose::Left, false);
	TestFalse(TEXT("Closed"), Chores->HasSession());
	TestFalse(TEXT("Working flag cleared"), Chores->IsWorking());
	TestTrue(TEXT("No session: report refused"), Chores->AuthStageDone(DrawWater, 1, Token1) == EKGChoreVerdict::NoSession);
	TestTrue(TEXT("Reopens"), Chores->AuthOpen(DrawWater, nullptr));
	TestEqual(TEXT("Resumes at the saved stage"), Chores->GetSessionStage(), 1);
	if (Chores->GetSessionToken() != Token1)
	{
		TestTrue(TEXT("Old token refused"), Chores->AuthStageDone(DrawWater, 1, Token1) == EKGChoreVerdict::WrongToken);
	}
	const float PrepBefore = Server.GS()->Preparation;
	TestTrue(TEXT("Plays to the end"), PlayThrough(Chores) == EKGChoreVerdict::Accepted);
	TestFalse(TEXT("Session over"), Chores->HasSession());
	TestEqual(TEXT("Ticked off the list"), CountTaskDone(PS, DrawWater), 1);
	TestTrue(TEXT("Preparation grew"), Server.GS()->Preparation > PrepBefore);
	TestEqual(TEXT("Saved progress cleared"), Chores->GetSavedStage(DrawWater), 0);
	TestFalse(TEXT("A done chore cannot be reopened"), Chores->AuthOpen(DrawWater, nullptr));

	// Visual chore: the bell rings for everyone.
	TestTrue(TEXT("Opens RingBell at its station"), Chores->AuthOpen(RingBell, BellStation));
	PlayThrough(Chores);
	const AKGChoreFx* Fx = AKGChoreFx::Get(Server.World, false);
	if (TestNotNull(TEXT("Effect director spawned"), Fx))
	{
		TestTrue(TEXT("Bell effect replicated state"), Fx->GetEvents().ContainsByPredicate([](const FKGChoreFxEvent& E)
		{
			return E.Fx == uint8(EKGChoreFx::BellRing) && E.ChoreId == TEXT("RingBell");
		}));
		TestTrue(TEXT("Effect sits at the station"), Fx->GetEvents().Num() > 0 &&
		                                             FVector::Dist(Fx->GetEvents().Last().Location, BellStation->GetActorLocation()) < 1.0f);
	}

	// Interrupts: meeting, hit, pushed away. Progress of a multi-stage chore is kept.
	TestTrue(TEXT("Opens MendNets"), Chores->AuthOpen(MendNets, nullptr));
	Chores->DebugAddServerTime(3.0f);
	TestTrue(TEXT("Stage 1 of 2"), Chores->AuthStageDone(MendNets, 0, Chores->GetSessionToken()) == EKGChoreVerdict::Accepted);
	Server.Run(TEXT("Match.Phase Meeting"));
	Tick(Chores, 0.1f);
	TestFalse(TEXT("A meeting closes the minigame"), Chores->HasSession());
	TestEqual(TEXT("...keeping the stage"), Chores->GetSavedStage(MendNets), 1);
	TestFalse(TEXT("Cannot open during the meeting"), Chores->AuthOpen(MendNets, nullptr));
	Server.Run(TEXT("Match.Phase Day"));

	TestTrue(TEXT("Opens ChopWood"), Chores->AuthOpen(ChopWood, nullptr));
	Body->GetHealth()->ApplyDamage(5.0f, nullptr, TEXT("Test"));
	Tick(Chores, 0.1f);
	TestFalse(TEXT("Taking a hit closes it"), Chores->HasSession());

	TestTrue(TEXT("Opens ChopWood again"), Chores->AuthOpen(ChopWood, nullptr));
	Tick(Chores, 0.1f);
	TestTrue(TEXT("Standing still keeps it open"), Chores->HasSession());
	Body->SetActorLocation(Body->GetActorLocation() + FVector(400.0, 0.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	Tick(Chores, 0.1f);
	TestFalse(TEXT("Being pushed away closes it"), Chores->HasSession());

	// Not on the list: refused, unless dev play (practice: nothing credited).
	TestFalse(TEXT("A chore not on your list does not open"), Chores->AuthOpen(TEXT("BakeBread"), nullptr));
	TestTrue(TEXT("Dev play opens it anyway"), Chores->AuthOpen(TEXT("BakeBread"), nullptr, true));
	TestTrue(TEXT("...as practice"), Chores->IsSessionPractice());
	const float PrepPractice = Server.GS()->Preparation;
	PlayThrough(Chores);
	TestEqual(TEXT("Practice fills nothing"), Server.GS()->Preparation, PrepPractice);

	// Dead villagers do no chores.
	Body->GetHealth()->ApplyDamage(500.0f, nullptr, TEXT("Test"));
	TestFalse(TEXT("The dead cannot open chores"), Chores->AuthOpen(MendNets, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreFakeTaskTest, "KillGodot.Chores.FakeTask",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreFakeTaskTest::RunTest(const FString& Parameters)
{
	using namespace KGChoreTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world"), Server.Create()))
	{
		return false;
	}
	const FName RingBell(TEXT("RingBell")), BakeBread(TEXT("BakeBread")), PostNotice(TEXT("PostNotice")), FuelLighthouse(TEXT("FuelLighthouse"));
	Server.AddStation(RingBell, FVector(0, 0, 100));
	Server.AddStation(BakeBread, FVector(800, 0, 100));
	Server.AddStation(PostNotice, FVector(0, 800, 100));
	Server.AddStation(FuelLighthouse, FVector(800, 800, 100));
	Server.Run(TEXT("Bot.Fill 6"));
	Server.Run(TEXT("Match.Phase Day"));
	const FKGRoleInfo* Impatient = FirstRole(EKGAlignment::Impatient);
	const FKGRoleInfo* Town = FirstRole(EKGAlignment::Town);
	if (!TestNotNull(TEXT("An Impatient role"), Impatient) || !TestNotNull(TEXT("A town role"), Town))
	{
		return false;
	}
	Server.Run(FString::Printf(TEXT("Me.Role %s #0"), *Impatient->RoleId.ToString()));
	for (int32 i = 1; i < 6; ++i)
	{
		Server.Run(FString::Printf(TEXT("Me.Role %s #%d"), *Town->RoleId.ToString(), i));
	}
	Server.Run(TEXT("Chore.Reset all"));
	AKGPlayerState* Killer = Server.Player(0);
	AKGPlayerState* Villager = Server.Player(1);
	UKGChoreComponent* Fake = Server.BotBody(0) ? Server.BotBody(0)->GetChores() : nullptr;
	UKGChoreComponent* Real = Server.BotBody(1) ? Server.BotBody(1)->GetChores() : nullptr;
	if (!TestNotNull(TEXT("Impatient body"), Fake) || !TestNotNull(TEXT("Villager body"), Real))
	{
		return false;
	}

	// The Impatient opens the same panel: same session shape, same public "working" state as a villager.
	TestTrue(TEXT("The Impatient can open a chore"), Fake->AuthOpen(RingBell, nullptr));
	TestTrue(TEXT("...flagged fake on the server"), Fake->IsSessionFake());
	TestTrue(TEXT("Villager opens the same chore"), Real->AuthOpen(RingBell, nullptr));
	TestFalse(TEXT("...real"), Real->IsSessionFake());
	TestEqual(TEXT("Onlookers see the same working flag"), Fake->IsWorking(), Real->IsWorking());
	TestEqual(TEXT("Same stage layout"), Fake->GetSessionStage(), Real->GetSessionStage());

	// The fake still needs plausible timing (the panel is identical).
	TestTrue(TEXT("Fake instant completion refused too"), Fake->AuthStageDone(RingBell, 0, Fake->GetSessionToken()) == EKGChoreVerdict::TooFast);
	const float Prep0 = Server.GS()->Preparation;
	TestTrue(TEXT("Fake plays through"), PlayThrough(Fake) == EKGChoreVerdict::Accepted);
	TestEqual(TEXT("Fake ticks their own list (looks done to them)"), CountTaskDone(Killer, RingBell), 1);
	TestEqual(TEXT("Fake fills no preparation"), Server.GS()->Preparation, Prep0);
	const AKGChoreFx* Fx = AKGChoreFx::Get(Server.World, false);
	TestTrue(TEXT("A faked visual chore shows nothing to the village"), !Fx || Fx->GetEvents().Num() == 0);

	// A real completion of the same visual chore does ring the bell: that is what proves innocence.
	TestTrue(TEXT("Villager plays through"), PlayThrough(Real) == EKGChoreVerdict::Accepted);
	TestEqual(TEXT("Villager's list ticked"), CountTaskDone(Villager, RingBell), 1);
	TestTrue(TEXT("Real completion fills preparation"), Server.GS()->Preparation > Prep0);
	Fx = AKGChoreFx::Get(Server.World, false);
	TestTrue(TEXT("Real visual chore rings the bell for everyone"), Fx && Fx->GetEvents().Num() == 1);

	// Multi-stage fake: stage progress is kept exactly like for a villager.
	TestTrue(TEXT("Fake opens BakeBread"), Fake->AuthOpen(BakeBread, nullptr));
	Fake->DebugAddServerTime(10.0f);
	TestTrue(TEXT("Fake stage 1 accepted"), Fake->AuthStageDone(BakeBread, 0, Fake->GetSessionToken()) == EKGChoreVerdict::Accepted);
	Fake->AuthClose(EKGChoreClose::Left, false);
	TestEqual(TEXT("Fake progress saved like a real one"), Fake->GetSavedStage(BakeBread), 1);
	TestTrue(TEXT("Fake resumes"), Fake->AuthOpen(BakeBread, nullptr));
	TestEqual(TEXT("...at the saved stage"), Fake->GetSessionStage(), 1);
	const float PrepBeforeFake = Server.GS()->Preparation;
	TestTrue(TEXT("Fake finishes BakeBread"), PlayThrough(Fake) == EKGChoreVerdict::Accepted);
	TestEqual(TEXT("Still no preparation from fakes"), Server.GS()->Preparation, PrepBeforeFake);
	TestTrue(TEXT("No chimney smoke from a fake"), !Fx || !Fx->GetEvents().ContainsByPredicate([](const FKGChoreFxEvent& E)
	{
		return E.Fx == uint8(EKGChoreFx::ChimneySmoke);
	}));
	return true;
}

#endif
