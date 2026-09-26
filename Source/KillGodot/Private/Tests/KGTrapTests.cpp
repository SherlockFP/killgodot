#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Core/KGGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Traps/KGTrap.h"
#include "Traps/KGTrapSubsystem.h"
#include "Traps/KGTrapTypes.h"
#include "UObject/Package.h"

namespace KGTrapTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FKGTrapDef Def(float Telegraph, float Active, float Cooldown, bool bPassive = false)
	{
		FKGTrapDef D;
		D.TelegraphSecs = Telegraph;
		D.ActiveSecs = Active;
		D.CooldownSecs = Cooldown;
		D.bPassive = bPassive;
		return D;
	}

	/** A transient game world with a KillGodot game mode (like the dig tests). */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGTrapTestWorld"));
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

// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapArmingTest, "KillGodot.Traps.Arming", KGTrapTests::Flags)
bool FKGTrapArmingTest::RunTest(const FString& Parameters)
{
	// Policies: None denies, Anyone allows a body, unknown denies; registered policies are consulted.
	FKGTrapArmPolicies Policies;
	TestFalse(TEXT("None denies"), Policies.CanArm(TEXT("None"), nullptr));
	TestFalse(TEXT("Anyone needs a body"), Policies.CanArm(TEXT("Anyone"), nullptr));
	TestFalse(TEXT("Unknown policy denies"), Policies.CanArm(TEXT("Trapper"), nullptr));
	TestFalse(TEXT("Impatient without a player state denies"), Policies.CanArm(TEXT("Impatient"), nullptr));
	TestTrue(TEXT("Defaults registered"), Policies.Has(TEXT("None")) && Policies.Has(TEXT("Anyone")) && Policies.Has(TEXT("Impatient")));
	Policies.Register(TEXT("TestAllow"), [](const AKGCharacter*) { return true; });
	Policies.Register(TEXT("TestDeny"), [](const AKGCharacter*) { return false; });
	TestTrue(TEXT("Registered allow"), Policies.CanArm(TEXT("TestAllow"), nullptr));
	TestFalse(TEXT("Registered deny"), Policies.CanArm(TEXT("TestDeny"), nullptr));
	TestFalse(TEXT("NAME_None denies"), Policies.CanArm(NAME_None, nullptr));

	// The machine: only Idle arms; passive machines start Armed.
	FKGTrapMachine M;
	M.Configure(KGTrapTests::Def(2.0f, 1.0f, 45.0f));
	TestEqual(TEXT("Starts Idle"), M.State, EKGTrapState::Idle);
	TestTrue(TEXT("Idle -> Armed"), M.TryArm());
	TestEqual(TEXT("Armed"), M.State, EKGTrapState::Armed);
	TestFalse(TEXT("Armed twice fails"), M.TryArm());
	FKGTrapMachine Passive;
	Passive.Configure(KGTrapTests::Def(0.3f, 4.0f, 10.0f, true));
	TestEqual(TEXT("Passive starts Armed"), Passive.State, EKGTrapState::Armed);
	TestFalse(TEXT("Passive needs no arming"), Passive.TryArm());

	// Armer cooldown: a second arm is blocked until it expires (remaining seconds, advanced).
	FKGTrapArmerCooldowns Cd;
	const FString Puid = TEXT("puid-A");
	TestTrue(TEXT("Fresh player may arm"), Cd.IsReady(Puid));
	Cd.Start(Puid, 60.0f);
	TestFalse(TEXT("Blocked right after arming"), Cd.IsReady(Puid));
	TestEqual(TEXT("60 s remaining"), Cd.Remaining(Puid), 60.0f);
	Cd.Advance(30.0f);
	TestFalse(TEXT("Still blocked at 30 s"), Cd.IsReady(Puid));
	TestEqual(TEXT("30 s remaining"), Cd.Remaining(Puid), 30.0f);
	TestTrue(TEXT("Another player is unaffected"), Cd.IsReady(TEXT("puid-B")));
	Cd.Advance(30.0f);
	TestTrue(TEXT("Expired after 60 s"), Cd.IsReady(Puid));
	TestEqual(TEXT("0 remaining"), Cd.Remaining(Puid), 0.0f);
	Cd.Start(TEXT(""), 10.0f);
	TestTrue(TEXT("An empty PUID is never tracked"), Cd.IsReady(TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapTriggerTest, "KillGodot.Traps.Trigger", KGTrapTests::Flags)
bool FKGTrapTriggerTest::RunTest(const FString& Parameters)
{
	FKGTrapMachine M;
	M.Configure(KGTrapTests::Def(2.0f, 1.0f, 45.0f));
	TestFalse(TEXT("Idle cannot trigger"), M.TryTrigger());
	TestTrue(TEXT("Advance while idle does nothing"), M.Advance(10.0f).Num() == 0);
	M.TryArm();
	TestTrue(TEXT("Armed -> Telegraph"), M.TryTrigger());
	TestEqual(TEXT("Telegraph"), M.State, EKGTrapState::Telegraph);
	TestEqual(TEXT("Telegraph clock = TelegraphSecs"), M.Remaining(), 2.0f);
	TestFalse(TEXT("No double trigger"), M.TryTrigger());

	TestEqual(TEXT("1.5 s: still telegraphing"), M.Advance(1.5f).Num(), 0);
	TestEqual(TEXT("0.5 s left"), M.Remaining(), 0.5f);
	TArray<FKGTrapTransition> T = M.Advance(0.5f);
	TestEqual(TEXT("One transition at 2 s"), T.Num(), 1);
	if (T.Num() == 1)
	{
		TestEqual(TEXT("Telegraph -> Active"), T[0].From, EKGTrapState::Telegraph);
		TestEqual(TEXT("-> Active"), T[0].To, EKGTrapState::Active);
	}
	TestEqual(TEXT("Active clock = ActiveSecs"), M.Remaining(), 1.0f);
	T = M.Advance(1.0f);
	TestEqual(TEXT("Active -> Cooldown"), T.Num() == 1 ? T[0].To : EKGTrapState::Idle, EKGTrapState::Cooldown);
	TestEqual(TEXT("Cooldown clock = CooldownSecs"), M.Remaining(), 45.0f);
	TestEqual(TEXT("44 s: still cooling"), M.Advance(44.0f).Num(), 0);
	T = M.Advance(1.0f);
	TestEqual(TEXT("Cooldown -> Idle"), T.Num() == 1 ? T[0].To : EKGTrapState::Armed, EKGTrapState::Idle);
	TestEqual(TEXT("Clock cleared"), M.Remaining(), 0.0f);

	// ForceFire: a zero telegraph fires on the next advance; zero-length phases chain.
	FKGTrapMachine F;
	F.Configure(KGTrapTests::Def(2.0f, 0.0f, 5.0f));
	F.ForceFire();
	TestEqual(TEXT("ForceFire -> Telegraph"), F.State, EKGTrapState::Telegraph);
	T = F.Advance(0.016f);
	TestEqual(TEXT("Telegraph -> Active -> Cooldown in one advance (ActiveSecs 0)"), T.Num(), 2);
	TestEqual(TEXT("Now cooling"), F.State, EKGTrapState::Cooldown);
	TestEqual(TEXT("Cooldown clock"), F.Remaining(), 5.0f);
	F.Reset();
	TestEqual(TEXT("Reset -> Idle"), F.State, EKGTrapState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapCooldownTest, "KillGodot.Traps.Cooldown", KGTrapTests::Flags)
bool FKGTrapCooldownTest::RunTest(const FString& Parameters)
{
	FKGTrapMachine M;
	M.Configure(KGTrapTests::Def(1.0f, 1.0f, 10.0f));
	M.TryArm();
	M.TryTrigger();
	M.Advance(1.0f);   // Active
	M.Advance(1.0f);   // Cooldown
	TestEqual(TEXT("Cooling"), M.State, EKGTrapState::Cooldown);
	TestFalse(TEXT("Not triggerable during cooldown"), M.TryTrigger());
	TestFalse(TEXT("Not armable during cooldown"), M.TryArm());
	M.Advance(9.0f);
	TestEqual(TEXT("Still cooling at 9 s"), M.State, EKGTrapState::Cooldown);
	TestFalse(TEXT("Still not triggerable"), M.TryTrigger());
	M.Advance(1.0f);
	TestEqual(TEXT("Idle after the cooldown"), M.State, EKGTrapState::Idle);
	TestFalse(TEXT("Idle still needs arming"), M.TryTrigger());
	TestTrue(TEXT("Armable again"), M.TryArm());

	FKGTrapMachine P;
	P.Configure(KGTrapTests::Def(0.3f, 4.0f, 10.0f, true));
	TestTrue(TEXT("Passive triggers at once"), P.TryTrigger());
	P.Advance(0.3f);
	P.Advance(4.0f);
	TestEqual(TEXT("Passive cools"), P.State, EKGTrapState::Cooldown);
	const TArray<FKGTrapTransition> T = P.Advance(10.0f);
	TestEqual(TEXT("Passive returns to Armed"), P.State, EKGTrapState::Armed);
	TestEqual(TEXT("... reported"), T.Num() == 1 ? T[0].To : EKGTrapState::Idle, EKGTrapState::Armed);
	TestTrue(TEXT("Passive re-triggers without arming"), P.TryTrigger());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapEventLogTest, "KillGodot.Traps.EventLog", KGTrapTests::Flags)
bool FKGTrapEventLogTest::RunTest(const FString& Parameters)
{
	FKGTrapEventLog Log;
	TestEqual(TEXT("Empty"), Log.Num(), 0);
	for (int32 i = 0; i < 10; ++i)
	{
		FKGTrapEvent E;
		E.TrapId = *FString::Printf(TEXT("T%d"), i);
		E.Type = TEXT("Fired");
		E.WorldSeconds = static_cast<float>(i);
		Log.Add(E);
	}
	TestEqual(TEXT("10 recorded"), Log.Num(), 10);
	TestEqual(TEXT("Oldest first"), Log.Get(0).TrapId, FName(TEXT("T0")));
	TestEqual(TEXT("Newest last"), Log.Get(9).TrapId, FName(TEXT("T9")));
	for (int32 i = 10; i < 150; ++i)
	{
		FKGTrapEvent E;
		E.TrapId = *FString::Printf(TEXT("T%d"), i);
		E.WorldSeconds = static_cast<float>(i);
		Log.Add(E);
	}
	TestEqual(TEXT("Capped at 64"), Log.Num(), FKGTrapEventLog::Capacity);
	TestEqual(TEXT("Oldest kept = 150 - 64"), Log.Get(0).TrapId, FName(TEXT("T86")));
	TestEqual(TEXT("Newest = 149"), Log.Get(63).TrapId, FName(TEXT("T149")));
	const TArray<FKGTrapEvent> All = Log.ToArray();
	TestEqual(TEXT("ToArray size"), All.Num(), 64);
	bool bOrdered = true;
	for (int32 i = 1; i < All.Num(); ++i)
	{
		bOrdered &= All[i].WorldSeconds > All[i - 1].WorldSeconds;
	}
	TestTrue(TEXT("ToArray is chronological"), bOrdered);
	Log.Reset();
	TestEqual(TEXT("Reset"), Log.Num(), 0);

	TestEqual(TEXT("State names"), FString(KGTrap::StateName(EKGTrapState::Telegraph)), FString(TEXT("Telegraph")));
	EKGTrapEffect Eff = EKGTrapEffect::Custom;
	TestTrue(TEXT("ParseEffect"), KGTrap::ParseEffect(TEXT("lightsout"), Eff) && Eff == EKGTrapEffect::LightsOut);
	TestFalse(TEXT("ParseEffect unknown"), KGTrap::ParseEffect(TEXT("Banana"), Eff));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapActorTest, "KillGodot.Traps.Actor", KGTrapTests::Flags)
bool FKGTrapActorTest::RunTest(const FString& Parameters)
{
	KGTrapTests::FServerWorld Server;
	if (!Server.Create())
	{
		AddWarning(TEXT("No engine world for the actor test; skipped"));
		return true;
	}
	UKGTrapSubsystem* Sub = UKGTrapSubsystem::Get(Server.World);
	if (!TestNotNull(TEXT("Trap subsystem in a game world"), Sub))
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKGTrap* Alarm = Server.World->SpawnActor<AKGTrap>(AKGTrap::StaticClass(), FTransform(FVector(1000.0, 0.0, 0.0)), Params);
	if (!TestNotNull(TEXT("Alarm spawned"), Alarm))
	{
		return false;
	}
	// Spawned: PostInitializeComponents configured the machine from the CDO defaults; reconfigure as a passive alarm.
	Alarm->TrapId = TEXT("T_Alarm");
	Alarm->Room = TEXT("Gallery");
	Alarm->Effect = EKGTrapEffect::Alarm;
	Alarm->TrapDef = KGTrapTests::Def(0.3f, 2.0f, 5.0f, true);
	Alarm->TrapDef.ArmPolicy = TEXT("None");
	Alarm->ZoneExtent = FVector(200.0, 200.0, 200.0);
	Alarm->ZoneOffset = FVector(0.0, 0.0, 100.0);
	Alarm->AuthReset();
	TestEqual(TEXT("Passive alarm is armed"), Alarm->GetState(), EKGTrapState::Armed);
	TestTrue(TEXT("Registered"), Sub->GetTraps().ContainsByPredicate([Alarm](const TWeakObjectPtr<AKGTrap>& T) { return T.Get() == Alarm; }));
	TestTrue(TEXT("FindById"), AKGTrap::FindById(Server.World, TEXT("T_Alarm")) == Alarm);
	TestTrue(TEXT("Point in zone"), Alarm->IsInZone(FVector(1100.0, 50.0, 90.0)));
	TestFalse(TEXT("Point outside zone"), Alarm->IsInZone(FVector(1400.0, 0.0, 90.0)));

	Alarm->Tick(0.1f);
	TestEqual(TEXT("Nobody inside: stays armed"), Alarm->GetState(), EKGTrapState::Armed);

	AKGCharacter* Body = Server.World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(FVector(1000.0, 0.0, 100.0)), Params);
	if (!TestNotNull(TEXT("Body spawned"), Body))
	{
		return false;
	}
	Body->SetActorLocation(FVector(1000.0, 0.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
	const int32 LogBefore = Sub->GetLog().Num();
	Alarm->Tick(0.1f);
	TestEqual(TEXT("A body inside triggers the telegraph"), Alarm->GetState(), EKGTrapState::Telegraph);
	TestFalse(TEXT("Not pinging yet"), Alarm->IsPinging());
	Alarm->Tick(0.3f);
	TestEqual(TEXT("Fired after the telegraph"), Alarm->GetState(), EKGTrapState::Active);
	TestTrue(TEXT("Alarm pings while active"), Alarm->IsPinging());
	Alarm->Tick(2.0f);
	TestEqual(TEXT("Cooling"), Alarm->GetState(), EKGTrapState::Cooldown);
	TestFalse(TEXT("Ping ends"), Alarm->IsPinging());
	// Step off the creaky board: a body still inside would re-trigger the moment the alarm re-arms.
	Body->SetActorLocation(FVector(3000.0, 0.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
	Alarm->Tick(5.0f);
	TestEqual(TEXT("Passive: armed again"), Alarm->GetState(), EKGTrapState::Armed);
	const TArray<FKGTrapEvent> Events = Sub->GetLog().ToArray();
	TestEqual(TEXT("Telegraph/Fired/Ended/Ready recorded"), Events.Num() - LogBefore, 4);
	if (Events.Num() - LogBefore == 4)
	{
		TestEqual(TEXT("1: Telegraph"), Events[LogBefore].Type, FName(TEXT("Telegraph")));
		TestEqual(TEXT("2: Fired"), Events[LogBefore + 1].Type, FName(TEXT("Fired")));
		TestEqual(TEXT("3: Ended"), Events[LogBefore + 2].Type, FName(TEXT("Ended")));
		TestEqual(TEXT("4: Ready"), Events[LogBefore + 3].Type, FName(TEXT("Ready")));
		TestEqual(TEXT("Room recorded"), Events[LogBefore].Room, FName(TEXT("Gallery")));
	}

	// An arming trap: the policy gate and the host override.
	AKGTrap* Door = Server.World->SpawnActor<AKGTrap>(AKGTrap::StaticClass(), FTransform(FVector(-1000.0, 0.0, 0.0)), Params);
	Door->TrapId = TEXT("T_Lock");
	Door->Effect = EKGTrapEffect::LockDoors;
	Door->TrapDef = KGTrapTests::Def(1.0f, 20.0f, 45.0f);
	Door->TrapDef.ArmPolicy = TEXT("Impatient");
	Door->AuthReset();
	TestEqual(TEXT("Arming trap starts idle"), Door->GetState(), EKGTrapState::Idle);
	TestFalse(TEXT("A roleless body may not arm an Impatient trap"), Door->MayArm(Body));
	TestFalse(TEXT("AuthArm honours the policy"), Door->AuthArm(Body, false));
	TestTrue(TEXT("Host override arms"), Door->AuthArm(Body, true));
	TestEqual(TEXT("Armed"), Door->GetState(), EKGTrapState::Armed);
	Door->AuthForceFire();
	Door->Tick(0.05f);
	TestEqual(TEXT("ForceFire -> Active (locks nothing here: no doors)"), Door->GetState(), EKGTrapState::Active);
	Door->AuthReset();
	TestEqual(TEXT("Reset -> Idle"), Door->GetState(), EKGTrapState::Idle);
	return true;
}

#endif
