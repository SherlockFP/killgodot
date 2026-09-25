#include "Misc/AutomationTest.h"
#include "Core/KGRng.h"
#include "Fishing/KGFishingTypes.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace KGFishingTestsPrivate
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Plays a whole fight with a policy; returns the result (and the elapsed fight time). */
	template <typename FPolicy>
	EKGReelResult PlayFight(const FKGFishSpecies* Species, float Alpha, uint64 Seed, float Dt, FPolicy Policy, float* OutSeconds = nullptr)
	{
		FKGReelSim Sim;
		Sim.Init(Species, Alpha, 14.0f, Seed);
		for (int32 Frame = 0; Frame < 200000 && !Sim.IsOver(); ++Frame)
		{
			Sim.Advance(Dt, Policy(Sim));
		}
		if (OutSeconds)
		{
			*OutSeconds = Sim.Elapsed;
		}
		return Sim.Result;
	}

	int32 CountItem(const TMap<FName, int32>& Counts, FName Item)
	{
		const int32* C = Counts.Find(Item);
		return C ? *C : 0;
	}
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGFishingSpeciesTableTest, "KillGodot.Fishing.SpeciesTable", KGFishingTestsPrivate::Flags)

bool FKGFishingSpeciesTableTest::RunTest(const FString& Parameters)
{
	const TArray<FKGFishSpecies>& All = FKGFishingRules::Species();
	TestTrue(TEXT("Five species (mackerel, cod, salmon, golden carp, koi)"), All.Num() == 5);
	int32 Sacred = 0;
	float LowestBase = TNumericLimits<float>::Max();
	FName Rarest;
	for (const FKGFishSpecies& S : All)
	{
		TestTrue(*FString::Printf(TEXT("%s weight range"), *S.Id.ToString()), S.MinKg > 0.0f && S.MinKg < S.MaxKg);
		TestTrue(*FString::Printf(TEXT("%s hook window 0.3..1.2 s"), *S.Id.ToString()), S.HookWindow >= 0.3f && S.HookWindow <= 1.2f);
		TestTrue(*FString::Printf(TEXT("%s pull 0..1"), *S.Id.ToString()), S.Pull > 0.0f && S.Pull <= 1.0f);
		TestEqual(*FString::Printf(TEXT("%s never on land"), *S.Id.ToString()), S.WaterWeight(EKGFishWater::Land), 0.0f);
		if (S.bSacred)
		{
			++Sacred;
			TestTrue(TEXT("Koi live only in the koi pond"), S.WaterWeight(EKGFishWater::KoiPond) > 0.0f &&
			         S.WaterWeight(EKGFishWater::Sea) == 0.0f && S.WaterWeight(EKGFishWater::Brook) == 0.0f);
			TestTrue(TEXT("Koi are not an item"), S.ItemId.IsNone());
			continue;
		}
		const FKGItemDef* Def = UKGItemCatalog::Find(S.ItemId);
		TestNotNull(*FString::Printf(TEXT("%s has an item"), *S.Id.ToString()), Def);
		TestTrue(*FString::Printf(TEXT("%s item is a Fish"), *S.Id.ToString()), Def && Def->HasTag(TEXT("Fish")));
		TestEqual(*FString::Printf(TEXT("%s not in the koi pond"), *S.Id.ToString()), S.WaterWeight(EKGFishWater::KoiPond), 0.0f);
		TestTrue(*FString::Printf(TEXT("%s: item lookup round-trips"), *S.Id.ToString()), FKGFishingRules::FindByItem(S.ItemId) == &S);
		if (S.Base < LowestBase)
		{
			LowestBase = S.Base;
			Rarest = S.Id;
		}
	}
	TestEqual(TEXT("Exactly one sacred species"), Sacred, 1);
	TestEqual(TEXT("The golden carp is the rarest"), Rarest, FName(TEXT("GoldenCarp")));
	const FKGItemDef* Carp = UKGItemCatalog::Find(FKGItemIds::FishGoldenCarp);
	TestTrue(TEXT("Golden carp is Legendary"), Carp && Carp->Rarity == EKGRarity::Legendary);
	// Every fishable water has something to catch.
	for (const EKGFishWater W : {EKGFishWater::Sea, EKGFishWater::Basin, EKGFishWater::Brook})
	{
		float Total = 0.0f;
		for (const FKGFishSpecies& S : All)
		{
			Total += FKGFishingRules::SpeciesWeight(S, W, EKGFishTime::Day, {});
		}
		TestTrue(*FString::Printf(TEXT("Water %d has fish"), static_cast<int32>(W)), Total > 0.0f);
	}
	// Weights stay inside the species range; fish items + rod + junk are in the catalog.
	FKGRng Rng(7);
	for (const FKGFishSpecies& S : All)
	{
		for (int32 i = 0; i < 200; ++i)
		{
			const int32 G = FKGFishingRules::RollGrams(Rng, S);
			if (G < FMath::RoundToInt(S.MinKg * 1000.0f) || G > FMath::RoundToInt(S.MaxKg * 1000.0f))
			{
				AddError(FString::Printf(TEXT("%s rolled %d g outside %.2f..%.2f kg"), *S.Id.ToString(), G, S.MinKg, S.MaxKg));
				break;
			}
		}
	}
	for (const FName Id : {FKGItemIds::FishingRod, FKGItemIds::OldBoot, FKGItemIds::MessageBottle, FKGItemIds::CoinPouch})
	{
		TestTrue(*FString::Printf(TEXT("%s is an item"), *Id.ToString()), UKGItemCatalog::IsValidItem(Id));
	}
	const FKGLootTable* Junk = FKGLoot::FindTable(TEXT("Fishing"));
	TestNotNull(TEXT("Fishing loot table"), Junk);
	if (Junk)
	{
		for (const FKGLootEntry& E : Junk->Entries)
		{
			TestTrue(*FString::Printf(TEXT("Junk %s is sellable at the market"), *E.ItemId.ToString()),
			         FKGFishingRules::IsSellable(E.ItemId) || E.ItemId == FKGItemIds::Key || E.ItemId == FKGItemIds::Bone ||
			         E.ItemId == FKGItemIds::TreasureMap);
		}
	}
	TestFalse(TEXT("The rod is not sold"), FKGFishingRules::IsSellable(FKGItemIds::FishingRod));
	TestFalse(TEXT("Coins are not sold"), FKGFishingRules::IsSellable(FKGItemIds::Coin));
	TestTrue(TEXT("Fish are sold"), FKGFishingRules::IsSellable(FKGItemIds::FishCod));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGFishingBiteRollTest, "KillGodot.Fishing.BiteRolls", KGFishingTestsPrivate::Flags)

bool FKGFishingBiteRollTest::RunTest(const FString& Parameters)
{
	using namespace KGFishingTestsPrivate;
	// Determinism: same seed, same bites.
	for (uint64 Seed = 1; Seed < 30; ++Seed)
	{
		FKGRng A(Seed);
		FKGRng B(Seed);
		const FKGBiteRoll RA = FKGFishingRules::RollBite(A, EKGFishWater::Sea, EKGFishTime::Day, {});
		const FKGBiteRoll RB = FKGFishingRules::RollBite(B, EKGFishWater::Sea, EKGFishTime::Day, {});
		TestTrue(TEXT("Same seed, same bite"), RA.ItemId == RB.ItemId && RA.Grams == RB.Grams && RA.Species == RB.Species);
	}
	// The koi pond (and land) never yields anything.
	FKGRng Rng(11);
	for (int32 i = 0; i < 500; ++i)
	{
		if (!FKGFishingRules::RollBite(Rng, EKGFishWater::KoiPond, EKGFishTime::Day, {}).ItemId.IsNone() ||
		    !FKGFishingRules::RollBite(Rng, EKGFishWater::Land, EKGFishTime::Day, {}).ItemId.IsNone())
		{
			AddError(TEXT("Something was caught in the koi pond / on land"));
			break;
		}
	}
	auto Tally = [](EKGFishWater Water, EKGFishTime Time, const TArray<FKGSchoolSample>& Schools, int32 N, uint64 Seed)
	{
		TMap<FName, int32> Counts;
		FKGRng R(Seed);
		for (int32 i = 0; i < N; ++i)
		{
			Counts.FindOrAdd(FKGFishingRules::RollBite(R, Water, Time, Schools).ItemId)++;
		}
		return Counts;
	};
	constexpr int32 N = 20000;
	const TMap<FName, int32> Sea = Tally(EKGFishWater::Sea, EKGFishTime::Day, {}, N, 99);
	const int32 CarpOpen = CountItem(Sea, FKGItemIds::FishGoldenCarp);
	TestTrue(FString::Printf(TEXT("Golden carp is rare in the open sea (%d / %d)"), CarpOpen, N), CarpOpen > 0 && CarpOpen < N / 50);
	int32 Junk = 0;
	for (const TPair<FName, int32>& P : Sea)
	{
		Junk += FKGFishingRules::FindByItem(P.Key) ? 0 : P.Value;
	}
	const float JunkShare = static_cast<float>(Junk) / N;
	TestTrue(FString::Printf(TEXT("Junk share %.3f near %.3f"), JunkShare, FKGFishingRules::JunkChance(EKGFishWater::Sea)),
	         FMath::Abs(JunkShare - FKGFishingRules::JunkChance(EKGFishWater::Sea)) < 0.015f);
	TestTrue(TEXT("Mackerel is the everyday catch by day"),
	         CountItem(Sea, FKGItemIds::FishMackerel) > CountItem(Sea, FKGItemIds::FishCod));
	// Near a golden carp school the legend shows up much more often.
	FKGSchoolSample CarpSchool;
	CarpSchool.Species = TEXT("GoldenCarp");
	CarpSchool.Count = 3;
	CarpSchool.Distance = 200.0f;
	const TMap<FName, int32> NearCarp = Tally(EKGFishWater::Sea, EKGFishTime::Day, {CarpSchool}, N, 99);
	const int32 CarpNear = CountItem(NearCarp, FKGItemIds::FishGoldenCarp);
	TestTrue(FString::Printf(TEXT("A golden carp school raises its chance (%d -> %d)"), CarpOpen, CarpNear), CarpNear > CarpOpen * 2);
	// Time of day: cod bite at night.
	const TMap<FName, int32> Night = Tally(EKGFishWater::Sea, EKGFishTime::Night, {}, N, 5);
	TestTrue(TEXT("Cod outnumber mackerel at night"), CountItem(Night, FKGItemIds::FishCod) > CountItem(Night, FKGItemIds::FishMackerel));
	// The brook has salmon (and the odd carp) but no sea fish.
	const TMap<FName, int32> Brook = Tally(EKGFishWater::Brook, EKGFishTime::Dawn, {}, 5000, 3);
	TestEqual(TEXT("No mackerel in the brook"), CountItem(Brook, FKGItemIds::FishMackerel), 0);
	TestEqual(TEXT("No cod in the brook"), CountItem(Brook, FKGItemIds::FishCod), 0);
	TestTrue(TEXT("Salmon run in the brook"), CountItem(Brook, FKGItemIds::FishSalmon) > 4000);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGFishingBiteScheduleTest, "KillGodot.Fishing.BiteSchedule", KGFishingTestsPrivate::Flags)

bool FKGFishingBiteScheduleTest::RunTest(const FString& Parameters)
{
	// Schools near the bobber raise the bite rate; far schools and gulls do not.
	FKGSchoolSample Near;
	Near.Species = TEXT("Mackerel");
	Near.Count = 24;
	Near.Distance = 150.0f;
	FKGSchoolSample Far = Near;
	Far.Distance = 5000.0f;
	FKGSchoolSample Gulls = Near;
	Gulls.Species = TEXT("Gull");
	TestTrue(TEXT("A school nearby adds a bonus"), FKGFishingRules::SchoolBonus({Near}) > 0.5f);
	TestEqual(TEXT("A far school adds nothing"), FKGFishingRules::SchoolBonus({Far}), 0.0f);
	TestEqual(TEXT("Gulls are not fish"), FKGFishingRules::SchoolBonus({Gulls}), 0.0f);
	TestTrue(TEXT("Bonus is capped"), FKGFishingRules::SchoolBonus({Near, Near, Near, Near}) <= 1.5f);
	const float Base = FKGFishingRules::BiteRate(EKGFishWater::Sea, EKGFishTime::Day, 0.0f);
	const float Boosted = FKGFishingRules::BiteRate(EKGFishWater::Sea, EKGFishTime::Day, FKGFishingRules::SchoolBonus({Near}));
	TestTrue(TEXT("Schools make bites faster"), Boosted > Base * 1.5f);
	TestEqual(TEXT("No bites in the koi pond"), FKGFishingRules::BiteRate(EKGFishWater::KoiPond, EKGFishTime::Day, 1.0f), 0.0f);
	TestTrue(TEXT("Dawn is the best time"), FKGFishingRules::BiteRate(EKGFishWater::Sea, EKGFishTime::Dawn, 0.0f) > Base);

	double SumBase = 0.0;
	double SumBoost = 0.0;
	FKGRng Rng(21);
	constexpr int32 N = 4000;
	for (int32 i = 0; i < N; ++i)
	{
		const FKGBiteSchedule S = FKGFishingRules::RollSchedule(Rng, Base);
		SumBase += S.BiteAt;
		SumBoost += FKGFishingRules::RollSchedule(Rng, Boosted).BiteAt;
		if (S.BiteAt < 2.5f || S.BiteAt > 40.0f)
		{
			AddError(FString::Printf(TEXT("Bite at %.2f s outside 2.5..40"), S.BiteAt));
			break;
		}
		float Prev = 0.0f;
		for (const float T : S.Nibbles)
		{
			if (T < 1.0f || T <= Prev || T >= S.BiteAt)
			{
				AddError(FString::Printf(TEXT("Nibble at %.2f (prev %.2f, bite %.2f) out of order"), T, Prev, S.BiteAt));
				break;
			}
			Prev = T;
		}
	}
	TestTrue(FString::Printf(TEXT("Mean wait drops near a school (%.1f -> %.1f s)"), SumBase / N, SumBoost / N), SumBoost < SumBase * 0.75);
	TestTrue(TEXT("Mean open-sea wait is a pleasant 8..20 s"), SumBase / N > 8.0 && SumBase / N < 20.0);
	// Hook window: species window plus a capped latency allowance.
	TestEqual(TEXT("Hook window without ping"), FKGFishingRules::ServerHookWindow(0.6f, 0.0f), 0.8f);
	TestEqual(TEXT("Latency allowance is capped"), FKGFishingRules::ServerHookWindow(0.6f, 2.0f), 1.1f);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGFishingReelTest, "KillGodot.Fishing.ReelMinigame", KGFishingTestsPrivate::Flags)

bool FKGFishingReelTest::RunTest(const FString& Parameters)
{
	using namespace KGFishingTestsPrivate;
	const auto Expert = [](const FKGReelSim& Sim) { return FKGReelSim::ExpertInput(Sim); };
	const auto Idle = [](const FKGReelSim&) { return FKGReelInput(); };
	// A skilled angler lands every catchable species (and junk) for many seeds, and bigger fish take longer.
	for (const FKGFishSpecies& S : FKGFishingRules::Species())
	{
		if (S.bSacred)
		{
			continue;
		}
		float Longest = 0.0f;
		for (uint64 Seed = 1; Seed <= 12; ++Seed)
		{
			float Seconds = 0.0f;
			const EKGReelResult R = PlayFight(&S, (Seed % 4) / 3.0f, Seed, 1.0f / 60.0f, Expert, &Seconds);
			if (R != EKGReelResult::Landed)
			{
				AddError(FString::Printf(TEXT("Expert lost a %s (seed %llu, result %d after %.1f s)"), *S.Id.ToString(), Seed,
				                         static_cast<int32>(R), Seconds));
			}
			Longest = FMath::Max(Longest, Seconds);
		}
		TestTrue(FString::Printf(TEXT("%s fight stays under a minute (%.1f s)"), *S.Id.ToString(), Longest), Longest < 60.0f);
	}
	TestEqual(TEXT("Junk comes in with steady reeling"), PlayFight(nullptr, 0.0f, 3, 1.0f / 60.0f, Expert), EKGReelResult::Landed);

	// Doing nothing lets a light fish throw the hook (slack) and a strong one run the line out.
	const FKGFishSpecies* Mackerel = FKGFishingRules::Find(TEXT("Mackerel"));
	const FKGFishSpecies* Carp = FKGFishingRules::Find(TEXT("GoldenCarp"));
	TestEqual(TEXT("Idle angler loses a mackerel (slack)"), PlayFight(Mackerel, 0.2f, 5, 1.0f / 60.0f, Idle), EKGReelResult::Escaped);
	const EKGReelResult IdleCarp = PlayFight(Carp, 0.8f, 5, 1.0f / 60.0f, Idle);
	TestTrue(TEXT("Idle angler loses a golden carp"), IdleCarp == EKGReelResult::Escaped || IdleCarp == EKGReelResult::Snapped);

	// Cranking non-stop while steering WITH the pull snaps the line on a golden carp.
	const auto Brute = [](const FKGReelSim& Sim)
	{
		FKGReelInput In;
		In.bReel = true;
		In.Steer = static_cast<float>(Sim.PullDir);
		return In;
	};
	for (uint64 Seed = 1; Seed <= 6; ++Seed)
	{
		TestEqual(TEXT("Brute force snaps a golden carp"), PlayFight(Carp, 0.9f, Seed, 1.0f / 60.0f, Brute), EKGReelResult::Snapped);
	}
	// Species differ: steady reeling without steering lands a mackerel but snaps a golden carp.
	const auto SteadyNoSteer = [](const FKGReelSim&)
	{
		FKGReelInput In;
		In.bReel = true;
		return In;
	};
	for (uint64 Seed = 1; Seed <= 8; ++Seed)
	{
		TestEqual(TEXT("Steady reeling lands a mackerel"), PlayFight(Mackerel, 0.5f, Seed, 1.0f / 60.0f, SteadyNoSteer), EKGReelResult::Landed);
		TestEqual(TEXT("Steady reeling snaps a golden carp"), PlayFight(Carp, 0.5f, Seed, 1.0f / 60.0f, SteadyNoSteer), EKGReelResult::Snapped);
	}

	// Steering against the pull keeps the tension lower than not steering.
	FKGReelSim A;
	FKGReelSim B;
	A.Init(Carp, 0.5f, 14.0f, 42);
	B.Init(Carp, 0.5f, 14.0f, 42);
	float SumA = 0.0f;
	float SumB = 0.0f;
	for (int32 i = 0; i < 90; ++i)
	{
		FKGReelInput Counter;
		Counter.bReel = true;
		Counter.Steer = static_cast<float>(-A.PullDir);
		FKGReelInput Straight;
		Straight.bReel = true;
		A.Step(FKGReelSim::StepSeconds, Counter);
		B.Step(FKGReelSim::StepSeconds, Straight);
		SumA += A.Tension;
		SumB += B.Tension;
	}
	TestTrue(FString::Printf(TEXT("Counter-steering lowers tension (%.2f < %.2f)"), SumA / 90.0f, SumB / 90.0f), SumA < SumB);

	// Frame-rate independence: the fixed 60 Hz substep gives the same fight at 30 and 144 fps.
	for (uint64 Seed = 1; Seed <= 5; ++Seed)
	{
		float T30 = 0.0f;
		float T144 = 0.0f;
		const auto Steady = [](const FKGReelSim&)
		{
			FKGReelInput In;
			In.bReel = true;
			return In;
		};
		const EKGReelResult R30 = PlayFight(Mackerel, 0.5f, Seed, 1.0f / 30.0f, Steady, &T30);
		const EKGReelResult R144 = PlayFight(Mackerel, 0.5f, Seed, 1.0f / 144.0f, Steady, &T144);
		TestEqual(TEXT("Same result at 30 and 144 fps"), R30, R144);
		TestTrue(FString::Printf(TEXT("Same fight length (%.4f vs %.4f)"), T30, T144), FMath::IsNearlyEqual(T30, T144, 1e-3f));
	}

	// Reconcile adopts the fish's discrete schedule exactly and blends the continuous values.
	FKGReelSim Client;
	FKGReelSim Server;
	Client.Init(Mackerel, 0.5f, 10.0f, 1);
	Server.Init(Mackerel, 0.5f, 10.0f, 2);
	Server.Tension = 0.9f;
	Client.Tension = 0.3f;
	Client.Reconcile(Server, 0.5f);
	TestEqual(TEXT("PullDir adopted"), Client.PullDir, Server.PullDir);
	TestEqual(TEXT("Rng adopted"), Client.Rng.State, Server.Rng.State);
	TestTrue(TEXT("Tension blended"), FMath::IsNearlyEqual(Client.Tension, 0.6f, 1e-4f));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGFishingEconomyTest, "KillGodot.Fishing.WeightsAndPrices", KGFishingTestsPrivate::Flags)

bool FKGFishingEconomyTest::RunTest(const FString& Parameters)
{
	// Heavier fish sell for more; unweighed fish sell at the catalog price.
	const int32 Light = FKGFishingRules::SellPrice(FKGItemIds::FishSalmon, 1, 1900);
	const int32 Heavy = FKGFishingRules::SellPrice(FKGItemIds::FishSalmon, 1, 6400);
	TestTrue(FString::Printf(TEXT("Heavy salmon pays more (%d > %d)"), Heavy, Light), Heavy > Light);
	TestEqual(TEXT("Unweighed cod = catalog value"), FKGFishingRules::SellPrice(FKGItemIds::FishCod, 2, 0),
	          UKGItemCatalog::Find(FKGItemIds::FishCod)->GoldValue * 2);
	TestTrue(TEXT("A trophy golden carp is a fortune"), FKGFishingRules::SellPrice(FKGItemIds::FishGoldenCarp, 1, 11000) >= 200);

	// Pockets keep the weight: stacks sum it, splits share it, moves carry it.
	UKGInventoryComponent* Pockets = NewObject<UKGInventoryComponent>();
	UKGInventoryComponent* Chest = NewObject<UKGInventoryComponent>();
	Pockets->SetCapacity(6);
	Chest->SetCapacity(6);
	TestEqual(TEXT("Add a weighed mackerel"), Pockets->AddItem(FKGItemIds::FishMackerel, 1, 800), 1);
	TestEqual(TEXT("Add another"), Pockets->AddItem(FKGItemIds::FishMackerel, 1, 400), 1);
	TestEqual(TEXT("Stacked in one slot"), Pockets->GetUsedSlots(), 1);
	TestEqual(TEXT("Stack weight is the sum"), Pockets->GramsOf(FKGItemIds::FishMackerel), 1200);
	TestEqual(TEXT("Move one to the chest"), Pockets->MoveTo(Chest, FKGItemIds::FishMackerel, 1), 1);
	TestEqual(TEXT("Half the weight went with it"), Chest->GramsOf(FKGItemIds::FishMackerel), 600);
	TestEqual(TEXT("Half stayed"), Pockets->GramsOf(FKGItemIds::FishMackerel), 600);
	FName Removed;
	int32 Grams = 0;
	TestEqual(TEXT("Take the last one out of its slot"), Pockets->RemoveFromSlot(0, 1, Removed, &Grams), 1);
	TestEqual(TEXT("Its weight left with it"), Grams, 600);
	TestEqual(TEXT("Nothing left"), Pockets->GramsOf(FKGItemIds::FishMackerel), 0);
	TestEqual(TEXT("Golden carp keeps its own slot and weight"), Pockets->AddItem(FKGItemIds::FishGoldenCarp, 1, 7300), 1);
	TestEqual(TEXT("Carp weight"), Pockets->GramsOf(FKGItemIds::FishGoldenCarp), 7300);

	// Charge: ping-pong, full at half the period, never zero.
	TestTrue(TEXT("Charge starts low"), FKGFishingRules::ChargePower(0.05f) < 0.2f);
	TestTrue(TEXT("Full charge at half the period"), FMath::IsNearlyEqual(FKGFishingRules::ChargePower(FKGFishingRules::ChargePeriod * 0.5f), 1.0f, 0.01f));
	TestTrue(TEXT("Over-holding loses power"), FKGFishingRules::ChargePower(FKGFishingRules::ChargePeriod * 0.9f) < 0.3f);
	TestTrue(TEXT("Never zero"), FKGFishingRules::ChargePower(0.0f) > 0.0f);
	// Cast: more power, faster; the launch pitch is clamped (no casting straight up or into your feet).
	const FVector Weak = FKGFishingRules::CastVelocity(FVector::ForwardVector, 0.1f);
	const FVector Strong = FKGFishingRules::CastVelocity(FVector::ForwardVector, 1.0f);
	TestTrue(TEXT("Power adds speed"), Strong.Size() > Weak.Size() * 1.8f);
	TestTrue(TEXT("Looking level still lobs it up"), Strong.Z > 0.0f);
	const FVector Up = FKGFishingRules::CastVelocity(FVector::UpVector, 1.0f);
	TestTrue(TEXT("Looking straight up is clamped to 55 deg"), Up.Z / Up.Size() < FMath::Sin(FMath::DegreesToRadians(56.0f)));
	const float Range = 2.0f * Strong.X * Strong.Z / FKGFishingRules::CastGravity / 100.0f;
	TestTrue(FString::Printf(TEXT("A full level cast reaches 10..30 m (%.1f m)"), Range), Range > 10.0f && Range < 30.0f);
	// Phases.
	TestFalse(TEXT("No fishing in meetings"), FKGFishingRules::CanFishInPhase(EKGPhase::Meeting));
	TestFalse(TEXT("No fishing in trials"), FKGFishingRules::CanFishInPhase(EKGPhase::Trial));
	TestTrue(TEXT("Fishing by day"), FKGFishingRules::CanFishInPhase(EKGPhase::Day));
	TestTrue(TEXT("Fishing at night"), FKGFishingRules::CanFishInPhase(EKGPhase::Night));
	TestTrue(TEXT("Night is night"), FKGFishingRules::TimeFor(EKGPhase::Night) == EKGFishTime::Night);
	// Bottles have something to say.
	for (int32 i = 0; i < FKGFishingRules::NumBottleMessages(); ++i)
	{
		TestFalse(TEXT("Bottle message"), FKGFishingRules::BottleMessage(i).IsEmpty());
	}
	return true;
}

#endif
