#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Roles/KGRoleListGenerator.h"
#include "UObject/Package.h"

namespace KGDevTests
{
	/** A headless game world running AKGGameMode (no map, no player controllers): the host side of the dev verbs. */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("KGDevTestWorld"), GetTransientPackage());
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

		FKGDevResult Run(const FString& Line) const
		{
			// No requester: what automation / an agent on the host gets. Me.* verbs then need a [Player] argument.
			return FKGDev::Execute({World, nullptr}, Line);
		}

		AKGGameState* GS() const { return World->GetGameState<AKGGameState>(); }
		AKGGameMode* GM() const { return World->GetAuthGameMode<AKGGameMode>(); }
		int32 NumPlayers() const { return GS()->PlayerArray.Num(); }

		int32 NumBots() const
		{
			int32 Bots = 0;
			for (const APlayerState* PS : GS()->PlayerArray)
			{
				Bots += PS && PS->IsABot() ? 1 : 0;
			}
			return Bots;
		}

		AKGPlayerState* Player(int32 Index) const
		{
			return GS()->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS()->PlayerArray[Index]) : nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDevVerbTableTest, "KillGodot.Dev.VerbTable",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGDevVerbTableTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Bot.Add is a host verb"), FKGDev::IsServerVerb(TEXT("Bot.Add 5")));
	TestTrue(TEXT("Verb names are case-insensitive"), FKGDev::IsServerVerb(TEXT("match.phase night")));
	TestFalse(TEXT("World.Stat is local"), FKGDev::IsServerVerb(TEXT("World.Stat fps")));
	TestFalse(TEXT("Unknown verb"), FKGDev::IsServerVerb(TEXT("Nope.Nothing")));
	TestTrue(TEXT("Headless callers (no controller) may run verbs"), FKGDev::MayRun(nullptr));

	TArray<FString> Help;
	FKGDev::GetHelpLines(Help);
	TestTrue(TEXT("Help lists the verbs"), Help.Num() > 30);
	for (const TCHAR* Verb : {TEXT("kg.Bot.Add"), TEXT("kg.Bot.Remove"), TEXT("kg.Match.Phase"), TEXT("kg.Me.Role"), TEXT("kg.Chore.Done")})
	{
		TestTrue(FString::Printf(TEXT("%s is documented"), Verb),
		         Help.ContainsByPredicate([Verb](const FString& Line) { return Line.StartsWith(Verb); }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDevServerVerbsTest, "KillGodot.Dev.ServerVerbs",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGDevServerVerbsTest::RunTest(const FString& Parameters)
{
	KGDevTests::FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world with AKGGameMode"), Server.Create()))
	{
		return false;
	}

	// ---- bots: add / fill / remove (newest, by name) / remove all ----
	TestTrue(TEXT("Bot.Add 3 ok"), Server.Run(TEXT("Bot.Add 3")).bOk);
	TestEqual(TEXT("3 players after Bot.Add 3"), Server.NumPlayers(), 3);
	TestEqual(TEXT("...all of them bots"), Server.NumBots(), 3);
	TestTrue(TEXT("Bot.Fill 8 ok"), Server.Run(TEXT("Bot.Fill 8")).bOk);
	TestEqual(TEXT("Filled to 8"), Server.NumPlayers(), 8);
	TestTrue(TEXT("Bot.Fill below the count adds nobody"), Server.Run(TEXT("Bot.Fill 4")).bOk);
	TestEqual(TEXT("Still 8"), Server.NumPlayers(), 8);
	TestTrue(TEXT("Bot.Remove 2 ok"), Server.Run(TEXT("Bot.Remove 2")).bOk);
	TestEqual(TEXT("6 after removing 2"), Server.NumPlayers(), 6);
	const FString FirstName = Server.Player(0) ? Server.Player(0)->GetPlayerName() : FString();
	TestTrue(TEXT("Bot.Remove by (quoted) name ok"), Server.Run(FString::Printf(TEXT("Bot.Remove \"%s\""), *FirstName)).bOk);
	TestEqual(TEXT("5 after removing one by name"), Server.NumPlayers(), 5);
	bool bNameGone = true;
	for (const APlayerState* PS : Server.GS()->PlayerArray)
	{
		bNameGone &= PS && PS->GetPlayerName() != FirstName;
	}
	TestTrue(TEXT("The named bot is the one that left"), bNameGone);
	TestFalse(TEXT("Removing an unknown bot fails"), Server.Run(TEXT("Bot.Remove \"Nobody At All\"")).bOk);
	TestTrue(TEXT("Bot.RemoveAll ok"), Server.Run(TEXT("Bot.RemoveAll")).bOk);
	TestEqual(TEXT("Nobody left"), Server.NumPlayers(), 0);

	// ---- phase jumps (roles are dealt when a jump skips the reveal) ----
	Server.Run(TEXT("Bot.Fill 8"));
	TestFalse(TEXT("Unknown phase fails"), Server.Run(TEXT("Match.Phase Brunch")).bOk);
	TestTrue(TEXT("Match.Phase Night ok"), Server.Run(TEXT("Match.Phase Night")).bOk);
	TestTrue(TEXT("Phase is Night"), Server.GS()->GetPhase() == EKGPhase::Night);
	bool bAllHaveRoles = true;
	for (int32 i = 0; i < Server.NumPlayers(); ++i)
	{
		bAllHaveRoles &= Server.Player(i) && !Server.Player(i)->GetPrivateRoleId().IsNone();
	}
	TestTrue(TEXT("Jumping past the reveal dealt every role"), bAllHaveRoles);
	TestTrue(TEXT("The match got a seed"), Server.GM()->GetMatchSeed() != 0);

	Server.Run(TEXT("Match.Phase Day"));
	TestTrue(TEXT("Phase is Day"), Server.GS()->GetPhase() == EKGPhase::Day);
	TestTrue(TEXT("Day counter advanced"), Server.GS()->GetDayIndex() >= 1);

	Server.Run(TEXT("Match.Phase Trial"));
	TestTrue(TEXT("Phase is Trial"), Server.GS()->GetPhase() == EKGPhase::Trial);
	TestNotNull(TEXT("Somebody is on trial"), Server.GS()->OnTrial.Get());

	Server.Run(TEXT("Match.Phase Meeting"));
	TestTrue(TEXT("Phase is Meeting"), Server.GS()->GetPhase() == EKGPhase::Meeting);
	TestNull(TEXT("A jump clears the trial"), Server.GS()->OnTrial.Get());
	int32 Alive = 0;
	for (int32 i = 0; i < Server.NumPlayers(); ++i)
	{
		Alive += Server.Player(i) && Server.Player(i)->IsAlive() ? 1 : 0;
	}
	TestEqual(TEXT("Leaving a trial by jump hangs nobody"), Alive, Server.NumPlayers());

	// ---- timer ----
	TestTrue(TEXT("Match.Freeze 1 ok"), Server.Run(TEXT("Match.Freeze 1")).bOk);
	TestTrue(TEXT("Clock frozen"), Server.GS()->Clock.bPaused);
	Server.Run(TEXT("Match.Freeze"));
	TestFalse(TEXT("Match.Freeze toggles back"), Server.GS()->Clock.bPaused);
	Server.Run(TEXT("Match.Time 42"));
	TestEqual(TEXT("Match.Time sets the time left"), Server.GS()->Clock.RemainingSeconds, 42.0f);
	Server.Run(TEXT("Match.Speed 5"));
	TestEqual(TEXT("Clock speed x5"), Server.GM()->DevClockScale, 5.0f);
	Server.Run(TEXT("Match.Speed 1"));

	// ---- role set ----
	const TArray<FKGRoleInfo>& Catalog = FKGRoleListGenerator::GetDefaultCatalog();
	const FKGRoleInfo* Impatient = Catalog.FindByPredicate([](const FKGRoleInfo& R) { return R.GetAlignment() == EKGAlignment::Impatient; });
	const FKGRoleInfo* Town = Catalog.FindByPredicate([](const FKGRoleInfo& R) { return R.GetAlignment() == EKGAlignment::Town; });
	if (TestNotNull(TEXT("Catalog has an Impatient role"), Impatient) && TestNotNull(TEXT("Catalog has a Town role"), Town))
	{
		TestTrue(TEXT("Me.Role <Impatient> #2 ok"), Server.Run(FString::Printf(TEXT("Me.Role %s #2"), *Impatient->RoleId.ToString())).bOk);
		TestEqual(TEXT("Player #2 has the Impatient role"), Server.Player(2)->GetPrivateRoleId(), Impatient->RoleId);
		const FString LowerTown = Town->RoleId.ToString().ToLower();
		TestTrue(TEXT("Role ids are case-insensitive"), Server.Run(FString::Printf(TEXT("Me.Role %s #3"), *LowerTown)).bOk);
		TestEqual(TEXT("Player #3 has the Town role"), Server.Player(3)->GetPrivateRoleId(), Town->RoleId);
	}
	TestFalse(TEXT("Unknown role fails"), Server.Run(TEXT("Me.Role NotARole #2")).bOk);
	TestFalse(TEXT("Me.Role without a requester or player fails"), Server.Run(TEXT("Me.Role Sheriff")).bOk);

	// ---- win / reveal ----
	TestTrue(TEXT("Match.Win Impatient ok"), Server.Run(TEXT("Match.Win Impatient")).bOk);
	TestTrue(TEXT("Epilogue after a forced win"), Server.GS()->GetPhase() == EKGPhase::Epilogue);
	TestTrue(TEXT("Winner declared"), Server.GS()->bHasWinner);
	TestTrue(TEXT("Impatient won"), Server.GS()->Winner == EKGAlignment::Impatient);
	TestEqual(TEXT("Roles revealed in the epilogue"), Server.Player(0)->RevealedRoleId, Server.Player(0)->GetPrivateRoleId());
	Server.Run(TEXT("Match.Phase Day"));
	TestFalse(TEXT("Jumping back into the match clears the winner"), Server.GS()->bHasWinner);

	// ---- misc ----
	TestTrue(TEXT("Chore.Reset all works without stations"), Server.Run(TEXT("Chore.Reset all")).bOk);
	TestTrue(TEXT("Match.Seed ok"), Server.Run(TEXT("Match.Seed")).bOk);
	TestFalse(TEXT("Unknown verb fails"), Server.Run(TEXT("Bot.Dance")).bOk);
	TestTrue(TEXT("Match.Skip ok"), Server.Run(TEXT("Match.Skip")).bOk);
	TestTrue(TEXT("Skip leaves at most a tick"), Server.GS()->Clock.RemainingSeconds <= 0.01f);
	return true;
}

#endif
