#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Forest/KGForestMap.h"
#include "Forest/KGForestRules.h"

// SPRINT-033/034: Docs/Iterations/SPRINT-033-ForestThreats.md (033b/033c acceptance 1-2), SPRINT-034 (034a lint, 034b 1-2).
namespace KGForestTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FKGWolfInputs In(EKGForestBand Band, bool bNight, int32 Day = 2, int32 Group = 1)
	{
		FKGWolfInputs I;
		I.Band = Band;
		I.bSafe = false;
		I.bNight = bNight;
		I.DayIndex = Day;
		I.GroupSize = Group;
		return I;
	}

	/** Simulates one lone player standing still: returns stamp / eyes / first bite / 5th bite (death at 100 HP) times. */
	void Timeline(float Gain, float& Stamp, float& Eyes, float& Bite1, float& Bite5, bool bHowlLive = false)
	{
		FKGWolfTrack T;
		Stamp = Eyes = Bite1 = Bite5 = -1.0f;
		const float Dt = 0.01f;
		int32 Bites = 0;
		for (float t = Dt; t < 120.0f && Bites < 5; t += Dt)
		{
			const FKGWolfStep S = FKGForestRules::StepWolf(T, Gain, Dt, bHowlLive);
			if (S.bStamped && Stamp < 0.0f) { Stamp = t; }
			if (S.bEyes && Eyes < 0.0f) { Eyes = t; }
			if (S.bBiteReady)
			{
				FKGForestRules::OnBite(T);
				++Bites;
				if (Bites == 1) { Bite1 = t; }
				if (Bites == 5) { Bite5 = t; }
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestWolfGainTest, "KillGodot.Forest.Rules.WolfGains", KGForestTests::Flags)
bool FKGForestWolfGainTest::RunTest(const FString& Parameters)
{
	using namespace KGForestTests;
	TestEqual(TEXT("Middle, night, alone +5"), FKGForestRules::WolfGain(In(EKGForestBand::Middle, true)), 5.0f);
	TestEqual(TEXT("Middle, day 2, alone: no gain (decay -3)"), FKGForestRules::WolfGain(In(EKGForestBand::Middle, false)), -3.0f);
	TestEqual(TEXT("Middle, day 5: +2"), FKGForestRules::WolfGain(In(EKGForestBand::Middle, false, 5)), 2.0f);
	TestEqual(TEXT("Deep, day +3"), FKGForestRules::WolfGain(In(EKGForestBand::Deep, false)), 3.0f);
	TestEqual(TEXT("Deep, night +7"), FKGForestRules::WolfGain(In(EKGForestBand::Deep, true)), 7.0f);
	TestEqual(TEXT("Deep night, two together x0.5"), FKGForestRules::WolfGain(In(EKGForestBand::Deep, true, 2, 2)), 3.5f);
	TestEqual(TEXT("Deep day, two together x0"), FKGForestRules::WolfGain(In(EKGForestBand::Deep, false, 2, 2)), -3.0f);
	TestEqual(TEXT("Deep night, three together x0"), FKGForestRules::WolfGain(In(EKGForestBand::Deep, true, 2, 3)), -3.0f);
	FKGWolfInputs T = In(EKGForestBand::Deep, false);
	T.bTorch = true;
	TestEqual(TEXT("Torch by day x0.75"), FKGForestRules::WolfGain(T), 2.25f);
	T.bNight = true;
	TestEqual(TEXT("Torch by night x0.5"), FKGForestRules::WolfGain(T), 3.5f);
	FKGWolfInputs F = In(EKGForestBand::Deep, true);
	F.bRawFish = true;
	TestEqual(TEXT("Raw fish x1.5"), FKGForestRules::WolfGain(F), 10.5f);
	F.bRawFish = false;
	F.Health = 40.0f;
	TestEqual(TEXT("HP < 50 x1.25"), FKGForestRules::WolfGain(F), 8.75f);
	FKGWolfInputs S = In(EKGForestBand::Deep, true);
	S.bSafe = true;
	TestEqual(TEXT("A path / light: -8"), FKGForestRules::WolfGain(S), -8.0f);
	TestEqual(TEXT("Edge band: -8"), FKGForestRules::WolfGain(In(EKGForestBand::Edge, true)), -8.0f);
	TestEqual(TEXT("Village: -8"), FKGForestRules::WolfGain(In(EKGForestBand::Village, true)), -8.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestWolfTimingTest, "KillGodot.Forest.Rules.TelegraphTiming", KGForestTests::Flags)
bool FKGForestWolfTimingTest::RunTest(const FString& Parameters)
{
	using namespace KGForestTests;
	struct FRow { const TCHAR* Name; float Gain, Stamp, Eyes, Bite1, Death; };
	const FRow Rows[] = {{TEXT("Deep night"), 7.0f, 5.0f, 17.0f, 25.0f, 41.0f},
	                     {TEXT("Middle night"), 5.0f, 7.0f, 19.0f, 27.0f, 43.0f},
	                     {TEXT("Deep day"), 3.0f, 11.7f, 23.7f, 33.3f, 49.3f}};
	for (const FRow& R : Rows)
	{
		float Stamp, Eyes, Bite1, Bite5;
		Timeline(R.Gain, Stamp, Eyes, Bite1, Bite5);
		TestTrue(FString::Printf(TEXT("%s: stamp %.2f ~ %.1f"), R.Name, Stamp, R.Stamp), FMath::Abs(Stamp - R.Stamp) <= 0.5f);
		TestTrue(FString::Printf(TEXT("%s: eyes %.2f ~ %.1f"), R.Name, Eyes, R.Eyes), FMath::Abs(Eyes - R.Eyes) <= 0.5f);
		TestTrue(FString::Printf(TEXT("%s: first bite %.2f ~ %.1f"), R.Name, Bite1, R.Bite1), FMath::Abs(Bite1 - R.Bite1) <= 0.5f);
		TestTrue(FString::Printf(TEXT("%s: death (5th bite) %.2f ~ %.1f"), R.Name, Bite5, R.Death), FMath::Abs(Bite5 - R.Death) <= 0.6f);
		TestTrue(FString::Printf(TEXT("%s: >= 20 s from the howl stamp to the first bite"), R.Name), Bite1 - Stamp >= KGForest::TelegraphMin - 0.01f);
		TestTrue(FString::Printf(TEXT("%s: stage minima 12 / 8 s"), R.Name), Eyes - Stamp >= 11.99f && Bite1 - Eyes >= 7.99f);
	}
	// A second player walking into a live howl: no new howl, but their own stamp - and still >= 20 s to their first bite,
	// however fast their interest climbs.
	float Stamp, Eyes, Bite1, Bite5;
	Timeline(60.0f, Stamp, Eyes, Bite1, Bite5, true);
	TestTrue(TEXT("Per-player stamp: a fast climb still waits 20 s"), Bite1 - Stamp >= KGForest::TelegraphMin - 0.01f);
	FKGWolfTrack T;
	const FKGWolfStep S = FKGForestRules::StepWolf(T, 400.0f, 0.1f, true);
	TestTrue(TEXT("Live sector howl: stamped, no second howl"), S.bStamped && !S.bHowl);
	FKGWolfTrack U;
	TestTrue(TEXT("No live howl: the stamp comes with a howl"), FKGForestRules::StepWolf(U, 400.0f, 0.1f, false).bHowl);
	// hysteresis
	FKGWolfTrack H;
	FKGForestRules::StepWolf(H, 400.0f, 0.1f, false);
	FKGForestRules::StepWolf(H, -8.0f, 1.0f, false);
	TestTrue(TEXT("Howl holds above 20"), H.Stage == EKGWolfStage::Howl);
	FKGForestRules::StepWolf(H, -8.0f, 3.0f, false);
	TestTrue(TEXT("Back to silent below 20"), H.Stage == EKGWolfStage::Silent);
	TestTrue(TEXT("Silent clears the stamp"), H.StampAge < 0.0f);
	// repel
	FKGWolfTrack R;
	R.Interest = 90.0f;
	FKGForestRules::OnRepel(R);
	TestEqual(TEXT("Repel -40"), R.Interest, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestEscapeTest, "KillGodot.Forest.Rules.Escape", KGForestTests::Flags)
bool FKGForestEscapeTest::RunTest(const FString& Parameters)
{
	using namespace KGForest;
	// Wolves: running from the moment the eyes show. The first bite can come 8 s after the eyes at the earliest, bites
	// are 4 s apart; the player runs 5.5 s at 5.8 m/s (32 m), then walks at 3.2 m/s.
	auto TimeTo = [](float Dist)
	{
		const float RunDist = PlayerRun * PlayerRunSecs;
		return Dist <= RunDist ? Dist / PlayerRun : PlayerRunSecs + (Dist - RunDist) / PlayerWalk;
	};
	auto Bites = [&TimeTo](float Dist)
	{
		const float Safe = TimeTo(Dist);
		int32 N = 0;
		for (float t = EyesMin; t < Safe; t += BiteCooldown)
		{
			++N;
		}
		return N;
	};
	TestTrue(TEXT("Running from the eyes, 45 m out: at most 1 bite"), Bites(SafeMax) <= 1);
	TestEqual(TEXT("Running from the eyes, <= 32 m out: no bite"), Bites(32.0f), 0);
	// The Mist: spawned 40 m behind, the player walks (3.2 m/s: plain, limping, carrying - all the same walk) from 45 m.
	const float Walk = PlayerWalk;
	bool bCaught = false;
	for (float t = 0.0f; t <= SafeMax / Walk; t += 0.01f)
	{
		const float Gap = MistSpawnBehind + Walk * t - FKGForestRules::MistDistance(t);
		bCaught |= Gap <= MistCore;
	}
	TestFalse(TEXT("Walking from KG_FOREST_SAFE_MAX always escapes the Mist"), bCaught);
	TestTrue(TEXT("A walker is overtaken (speed-wise) after 10 s"), FMath::IsNearlyEqual(FKGForestRules::MistSpeed(10.0f), 3.2f, 0.01f));
	TestEqual(TEXT("Mist speed starts at 2.4"), FKGForestRules::MistSpeed(0.0f), 2.4f);
	TestEqual(TEXT("Mist speed tops out at 4.2"), FKGForestRules::MistSpeed(60.0f), 4.2f);
	TestTrue(TEXT("A runner (5.8) is never caught"), FKGForestRules::MistSpeed(1000.0f) < PlayerRun);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestMistTest, "KillGodot.Forest.Rules.Mist", KGForestTests::Flags)
bool FKGForestMistTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Notice fills in 40 s by day"), FMath::IsNearlyEqual(100.0f / FKGForestRules::MistNoticeRate(false, false), 40.0f));
	TestTrue(TEXT("Notice fills in 20 s by night"), FMath::IsNearlyEqual(100.0f / FKGForestRules::MistNoticeRate(true, false), 20.0f));
	TestTrue(TEXT("A torch doubles it"), FMath::IsNearlyEqual(100.0f / FKGForestRules::MistNoticeRate(true, true), 40.0f));
	TestTrue(TEXT("Deep, alone, day 2: active"), FKGForestRules::MistNoticeActive(EKGForestBand::Deep, 1, false, 2));
	TestFalse(TEXT("Middle band: no Mist"), FKGForestRules::MistNoticeActive(EKGForestBand::Middle, 1, false, 2));
	TestFalse(TEXT("Not alone: no Mist"), FKGForestRules::MistNoticeActive(EKGForestBand::Deep, 2, false, 2));
	TestFalse(TEXT("AFK-frozen: no Mist"), FKGForestRules::MistNoticeActive(EKGForestBand::Deep, 1, true, 2));
	TestFalse(TEXT("Day / night 1: no Mist"), FKGForestRules::MistNoticeActive(EKGForestBand::Deep, 1, false, 1));
	// night: frost at 10 s, tongue at 20 s, 40 m behind; a player who never moves dies >= 25 s after the frost
	FKGMistTrack T;
	float Frost = -1.0f, Tongue = -1.0f;
	const float Dt = 0.01f;
	float t = 0.0f;
	for (; t < 60.0f && Tongue < 0.0f; t += Dt)
	{
		const bool bSpawn = FKGForestRules::StepMistNotice(T, true, Dt, true, false);
		if (T.Stage == EKGMistStage::Frost && Frost < 0.0f) { Frost = t; }
		if (bSpawn) { Tongue = t; }
	}
	TestTrue(FString::Printf(TEXT("Frost at ~10 s (%.2f)"), Frost), FMath::Abs(Frost - 10.0f) < 0.1f);
	TestTrue(FString::Printf(TEXT("Tongue at ~20 s (%.2f)"), Tongue), FMath::Abs(Tongue - 20.0f) < 0.1f);
	float Age = 0.0f;
	bool bDead = false;
	for (; Age < 60.0f && !bDead; Age += Dt)
	{
		bDead = FKGForestRules::StepMistCore(T, KGForest::MistSpawnBehind - FKGForestRules::MistDistance(Age), Dt);
	}
	const float FrostToDeath = (Tongue + Age) - Frost;
	TestTrue(FString::Printf(TEXT("Frost -> death >= KG_MIST_TELEGRAPH_MIN (%.1f s)"), FrostToDeath), FrostToDeath >= KGForest::MistTelegraphMin);
	// the core resets when you step out
	FKGMistTrack C;
	FKGForestRules::StepMistCore(C, 1.0f, 2.0f);
	FKGForestRules::StepMistCore(C, 5.0f, 0.1f);
	TestFalse(TEXT("Leaving the core resets it"), FKGForestRules::StepMistCore(C, 1.0f, 2.0f));
	TestTrue(TEXT("3 s in the core"), FKGForestRules::StepMistCore(C, 1.0f, 1.01f));
	// spawn: 40 m behind, away from the nearest safe cell, outside the view cone
	const FVector2D P(0.0f, 0.0f), Safe(10.0f, 0.0f);
	const FVector2D A = FKGForestRules::MistSpawn(P, Safe, FVector2D(1.0f, 0.0f));
	TestTrue(TEXT("Spawn opposite the safe side"), A.Equals(FVector2D(-40.0f, 0.0f), 0.01f));
	const FVector2D B = FKGForestRules::MistSpawn(P, Safe, FVector2D(-1.0f, 0.0f));
	TestTrue(TEXT("Spawn 40 m away"), FMath::IsNearlyEqual(float(B.Size()), 40.0f, 0.01f));
	TestTrue(TEXT("Spawn outside a 60 deg view cone"), FVector2D::DotProduct(B.GetSafeNormal(), FVector2D(-1.0f, 0.0f)) <= 0.5f + 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestCapsSafetyTest, "KillGodot.Forest.Rules.CapsAndSafety", KGForestTests::Flags)
bool FKGForestCapsSafetyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Wolf cap N=6"), FKGForestRules::WolfCap(6), 2);
	TestEqual(TEXT("Wolf cap N=9"), FKGForestRules::WolfCap(9), 2);
	TestEqual(TEXT("Wolf cap N=12"), FKGForestRules::WolfCap(12), 3);
	TestEqual(TEXT("Wolf cap N=20"), FKGForestRules::WolfCap(20), 6);
	TestEqual(TEXT("Packs x size N=20"), FKGForestRules::PackCount(20) * FKGForestRules::PackSize(20), 6);
	TestTrue(TEXT("Packs never exceed the cap"), FKGForestRules::PackCount(12) * FKGForestRules::PackSize(12) <= FKGForestRules::WolfCap(12) &&
	                                              FKGForestRules::PackCount(8) * FKGForestRules::PackSize(8) <= FKGForestRules::WolfCap(8));
	TestEqual(TEXT("Mist cap N=6 / 12 / 20"), FKGForestRules::MistCap(6) * 100 + FKGForestRules::MistCap(12) * 10 + FKGForestRules::MistCap(20), 123);
	TestTrue(TEXT("West + North open at N=6"), FKGForestRules::IsSectorOpen(1, 6) && FKGForestRules::IsSectorOpen(3, 6));
	TestFalse(TEXT("Cave Ridge closed at N=9"), FKGForestRules::IsSectorOpen(2, 9));
	TestTrue(TEXT("East Ridge open at N=15"), FKGForestRules::IsSectorOpen(4, 15));
	// P1b: never lethal at N <= 9, in the endgame or on day / night 1
	TestFalse(TEXT("N=9: never lethal"), FKGForestRules::IsPvELethal(9, 9, 2, 3, 0, 0));
	TestTrue(TEXT("N=12, 10 alive, 2 threats, day 3: lethal"), FKGForestRules::IsPvELethal(12, 10, 2, 3, 0, 0));
	TestFalse(TEXT("Endgame: 5 alive"), FKGForestRules::IsPvELethal(12, 5, 1, 3, 0, 0));
	TestFalse(TEXT("Endgame: alive <= 2 x threats + 1"), FKGForestRules::IsPvELethal(20, 7, 3, 3, 0, 0));
	TestFalse(TEXT("Day / night 1: never lethal"), FKGForestRules::IsPvELethal(20, 20, 3, 1, 0, 0));
	TestEqual(TEXT("A non-lethal bite does 0 damage"), FKGForestRules::BiteDamage(false), 0.0f);
	TestEqual(TEXT("A lethal bite does 20"), FKGForestRules::BiteDamage(true), 20.0f);
	// F1: PvE deaths stay within 5 % of all deaths (judged over >= 20)
	TestTrue(TEXT("First PvE death of a session: 1/20 = 5 %"), FKGForestRules::IsPvELethal(12, 10, 2, 3, 0, 0));
	TestFalse(TEXT("Second PvE death after 10 deaths: > 5 %"), FKGForestRules::IsPvELethal(12, 10, 2, 3, 1, 10));
	TestTrue(TEXT("Second PvE death after 39 deaths: 2/40 = 5 %"), FKGForestRules::IsPvELethal(12, 10, 2, 3, 1, 39));
	for (int32 Deaths = 0; Deaths < 200; ++Deaths)
	{
		for (int32 PvE = 0; PvE <= Deaths; ++PvE)
		{
			if (FKGForestRules::IsPvELethal(12, 10, 2, 3, PvE, Deaths) && float(PvE + 1) / FMath::Max(Deaths + 1, 20) > 0.05f + 1e-4f)
			{
				AddError(FString::Printf(TEXT("share cap broken at %d/%d"), PvE + 1, Deaths + 1));
			}
		}
	}
	// the 2-player target rule
	TestEqual(TEXT("Farther from the light is taken"), FKGForestRules::PickPairTarget(20.0f, 50.0f, TEXT("B"), 10.0f, 90.0f, TEXT("A")), 0);
	TestEqual(TEXT("Tie -> higher interest"), FKGForestRules::PickPairTarget(10.0f, 50.0f, TEXT("A"), 10.0f, 90.0f, TEXT("B")), 1);
	TestEqual(TEXT("Tie -> player key"), FKGForestRules::PickPairTarget(10.0f, 50.0f, TEXT("B"), 10.0f, 50.0f, TEXT("A")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestVigilTest, "KillGodot.Forest.Vigil", KGForestTests::Flags)
bool FKGForestVigilTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Max 2 signers at N <= 7"), FKGForestRules::VigilMaxSigners(7), 2);
	TestEqual(TEXT("Max 4 signers at N = 12"), FKGForestRules::VigilMaxSigners(12), 4);
	TestEqual(TEXT("Arrive by 60 s (N=6, night 93 s)"), FKGForestRules::VigilArriveBy(93.0f), 60.0f);
	TestTrue(TEXT("Arrive by 0.55 x night (N=20, 135 s)"), FMath::IsNearlyEqual(FKGForestRules::VigilArriveBy(135.0f), 74.25f));
	TestFalse(TEXT("Night 1 closed"), FKGForestRules::VigilOpen(1));
	TestTrue(TEXT("Night 2 open"), FKGForestRules::VigilOpen(2));
	const FKGVigilCaps C6 = FKGForestRules::VigilCaps(6), C12 = FKGForestRules::VigilCaps(12), C20 = FKGForestRules::VigilCaps(20);
	TestTrue(TEXT("Caps 1/1/2 at N=6"), C6.PerSigner == 1 && C6.NightCap == 1 && C6.MatchCap == 2);
	TestTrue(TEXT("Caps 1/2/4 at N=12"), C12.PerSigner == 1 && C12.NightCap == 2 && C12.MatchCap == 4);
	TestTrue(TEXT("Caps 2/3/6 at N=20"), C20.PerSigner == 2 && C20.NightCap == 3 && C20.MatchCap == 6);
	FKGVigilNight Good;
	Good.Signers = 3;
	Good.bAllArrived = true;
	Good.MaxOutsideSecs = 10.0f;
	Good.LitShare = 0.95f;
	TestTrue(TEXT("A good night succeeds"), FKGForestRules::VigilSuccess(Good));
	FKGVigilNight Bad = Good;
	Bad.Signers = 1;
	TestFalse(TEXT("One signer is not a vigil"), FKGForestRules::VigilSuccess(Bad));
	Bad = Good;
	Bad.MaxOutsideSecs = 16.0f;
	TestFalse(TEXT("> 15 s outside the ring fails"), FKGForestRules::VigilSuccess(Bad));
	Bad = Good;
	Bad.LitShare = 0.85f;
	TestFalse(TEXT("Fire lit < 90 % fails"), FKGForestRules::VigilSuccess(Bad));
	Bad = Good;
	Bad.bAllArrived = false;
	TestFalse(TEXT("A late signer fails it"), FKGForestRules::VigilSuccess(Bad));
	TestEqual(TEXT("N=12: 3 signers -> night cap 2"), FKGForestRules::VigilUnits(Good, 12, 0, 20), 2);
	TestEqual(TEXT("N=12: match cap 4 (3 granted -> 1)"), FKGForestRules::VigilUnits(Good, 12, 3, 20), 1);
	TestEqual(TEXT("Never past the living Town players' open tasks"), FKGForestRules::VigilUnits(Good, 20, 0, 1), 1);
	TestEqual(TEXT("N=6 -> 1"), FKGForestRules::VigilUnits(Good, 6, 0, 20), 1);
	// side-blind: the rule has no role input at all - the same night gives the same units whoever signed
	TestEqual(TEXT("Side-blind"), FKGForestRules::VigilUnits(Good, 12, 0, 20), FKGForestRules::VigilUnits(Good, 12, 0, 20));
	// the fire
	float Fuel = KGForest::FireFuelMax;
	for (int32 i = 0; i < 400; ++i)
	{
		Fuel = FKGForestRules::FireStep(Fuel, 0.1f);
	}
	TestEqual(TEXT("A full fire burns 40 s"), Fuel, 0.0f);
	TestEqual(TEXT("A log +50"), FKGForestRules::FireAddLog(20.0f), 70.0f);
	TestEqual(TEXT("Capped at 100"), FKGForestRules::FireAddLog(80.0f), 100.0f);
	TestEqual(TEXT("Safe radius 10 m"), FKGForestRules::FireSafeRadius(50.0f), 10.0f);
	TestEqual(TEXT("Safe radius 6 m under 20 fuel"), FKGForestRules::FireSafeRadius(10.0f), 6.0f);
	TestEqual(TEXT("Cold fire: no safety"), FKGForestRules::FireSafeRadius(0.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGForestMapTest, "KillGodot.Forest.Map", KGForestTests::Flags)
bool FKGForestMapTest::RunTest(const FString& Parameters)
{
	const FKGForestMap& Map = FKGForestMap::Get();
	if (!TestTrue(TEXT("Forest raster loads"), Map.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("The square is village"), Map.BandAt(Map.Square) == EKGForestBand::Village);
	TestTrue(TEXT("The square is safe"), Map.IsStaticSafe(Map.Square));
	TestTrue(TEXT("4 dens"), Map.Dens.Num() == 4);
	TArray<int32> Sectors;
	for (const FKGForestDen& D : Map.Dens)
	{
		TestTrue(FString::Printf(TEXT("Den %s in the Deep band"), *D.Id.ToString()), Map.BandAt(D.At) == EKGForestBand::Deep);
		Sectors.AddUnique(D.Sector);
	}
	TestTrue(TEXT("Dens in >= 2 sectors"), Sectors.Num() >= 2);
	const EKGForestBand CampBand = Map.BandAt(Map.Camp);
	TestTrue(TEXT("The camp is not Deep"), CampBand != EKGForestBand::Deep && CampBand != EKGForestBand::Out);
	// F9: every walkable forest cell <= KG_FOREST_SAFE_MAX from a safe cell (BFS nearest: +1 cell slack)
	int32 Far = 0, Forest = 0;
	for (int32 i = 0; i < Map.Flags.Num(); ++i)
	{
		const uint8 B = Map.Flags[i] & KGForestFlag::BandMask;
		if (B >= 1 && B <= 3 && !(Map.Flags[i] & KGForestFlag::NotWalkable))
		{
			++Forest;
			const FVector2D C(Map.X0 + Map.Cell * ((i % Map.NX) + 0.5f), Map.Y0 + Map.Cell * ((i / Map.NX) + 0.5f));
			Far += Map.SafeDistance(C) > KGForest::SafeMax + Map.Cell ? 1 : 0;
		}
	}
	TestTrue(FString::Printf(TEXT("Walkable forest area (%d cells)"), Forest), Forest > 5000);
	TestEqual(TEXT("No walkable forest cell farther than KG_FOREST_SAFE_MAX"), Far, 0);
	// 034a: the forest chores exist and never send anyone into the Deep band
	const FKGWorldChoreCatalog* Cat = FKGWorldChoreCatalog::FindByMap(TEXT("L_Morrowmere_v2"));
	if (TestNotNull(TEXT("Morrowmere chore catalog"), Cat))
	{
		for (const TCHAR* Id : {TEXT("GatherDeadwood"), TEXT("PitchCamp"), TEXT("TrailLanterns"), TEXT("GatherHerbs")})
		{
			TestNotNull(FString::Printf(TEXT("Forest chore %s"), Id), Cat->FindChore(Id));
		}
		for (const FKGWorldAnchor& A : Cat->Anchors)
		{
			if (A.Id.ToString().StartsWith(TEXT("forest_")))
			{
				const EKGForestBand B = Map.BandAt(FVector2D(A.Location.X, A.Location.Y) / 100.0f);
				TestTrue(FString::Printf(TEXT("%s not in the Deep band"), *A.Id.ToString()), B != EKGForestBand::Deep);
				if (A.Kind == TEXT("Herbs"))
				{
					TestTrue(FString::Printf(TEXT("%s in Edge/Middle"), *A.Id.ToString()), B == EKGForestBand::Edge || B == EKGForestBand::Middle);
				}
			}
		}
	}
	return true;
}

#endif
