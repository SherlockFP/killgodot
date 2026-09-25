#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Chores/KGChoreTypes.h"
#include "Chores/UI/KGMinigame.h"
#include "KillGodot.h"

// ---------------------------------------------------------------------------------------------------------------------
// SPRINT-025 item 3: every panel minigame, played by the scripted player (AutoPlay at 60 Hz, no ForceSolve), finishes
// all its stages in a median of at most MaxPlaySeconds over several seeds. With the walking allowance that is the
// "20-40 s including walking" target. Logs one KG_CHORE_TIMING line per chore (seconds per stage, floor per stage).
// Also proves the plausibility floors stay below the scripted time (a perfect player is never rejected as TooFast).
// ---------------------------------------------------------------------------------------------------------------------

namespace KGChoreTiming
{
	constexpr float MaxPlaySeconds = 28.0f;    // 40 s target minus ~12 s of walking (Tools/Level routes: 30-75 s world chores)
	constexpr float StepSeconds = 1.0f / 60.0f;
	constexpr float GiveUpSeconds = 120.0f;
	constexpr uint32 Seeds[] = {11, 4242, 90210};

	/** Seconds the scripted player needs per stage (GiveUpSeconds when a stage never solves). */
	TArray<float> Play(FKGMinigame& Game, uint32 Seed)
	{
		TArray<float> Out;
		Game.Start(Seed, 0);
		for (int32 Stage = 0; Stage < Game.NumStages(); ++Stage)
		{
			float T = 0.0f;
			while (!Game.IsStageSolved() && T < GiveUpSeconds)
			{
				Game.HostTick(StepSeconds, true);
				T += StepSeconds;
			}
			Game.DrainFeedback();
			Out.Add(T);
			if (!Game.IsStageSolved())
			{
				break;
			}
			if (Stage + 1 < Game.NumStages())
			{
				Game.AdvanceStage();
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreTimingTest, "KillGodot.Chores.Timing",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreTimingTest::RunTest(const FString& Parameters)
{
	using namespace KGChoreTiming;
	for (const FName Id : FKGMinigameFactory::GetIds())
	{
		const FString Name = Id.ToString();
		const FKGChoreDef* Def = FKGChoreCatalog::Find(Id);
		TArray<float> Totals;
		TArray<TArray<float>> PerSeed;
		bool bSolved = true;
		for (const uint32 Seed : Seeds)
		{
			TUniquePtr<FKGMinigame> Game = FKGMinigameFactory::Create(Id);
			if (!TestTrue(Name + TEXT(": created"), Game.IsValid()))
			{
				break;
			}
			const TArray<float> Stages = Play(*Game, Seed);
			bSolved &= Stages.Num() == Game->NumStages() && Stages.Last() < GiveUpSeconds;
			float Total = 0.0f;
			for (const float T : Stages)
			{
				Total += T;
			}
			Totals.Add(Total);
			PerSeed.Add(Stages);
		}
		Totals.Sort();
		const float Median = Totals.Num() > 0 ? Totals[Totals.Num() / 2] : GiveUpSeconds;
		FString Detail;
		for (const TArray<float>& Stages : PerSeed)
		{
			Detail += TEXT("[");
			for (int32 i = 0; i < Stages.Num(); ++i)
			{
				Detail += FString::Printf(TEXT("%s%.1f"), i ? TEXT(" ") : TEXT(""), Stages[i]);
			}
			Detail += TEXT("] ");
		}
		FString Floors;
		if (Def)
		{
			for (const float Fl : Def->StageMinSeconds)
			{
				Floors += FString::Printf(TEXT("%.1f "), Fl);
			}
		}
		UE_LOG(LogKillGodot, Display, TEXT("KG_CHORE_TIMING %s median=%.1fs stages=%s floors=[%s] %s"), *Name, Median, *Detail,
		       *Floors, bSolved ? TEXT("ok") : TEXT("UNSOLVED"));
		TestTrue(Name + TEXT(": the scripted player solves every stage"), bSolved);
		TestTrue(FString::Printf(TEXT("%s: median %.1f s <= %.0f s (40 s with walking)"), *Name, Median, MaxPlaySeconds),
		         Median <= MaxPlaySeconds);
		// Floors: a perfect run must never trip the server's TooFast check (StageMinSeconds * TimingSlack).
		if (Def && bSolved)
		{
			for (int32 Stage = 0; Stage < Def->NumStages(); ++Stage)
			{
				float Fastest = GiveUpSeconds;
				for (const TArray<float>& Stages : PerSeed)
				{
					if (Stages.IsValidIndex(Stage))
					{
						Fastest = FMath::Min(Fastest, Stages[Stage]);
					}
				}
				TestTrue(FString::Printf(TEXT("%s stage %d: floor %.1f * slack <= fastest scripted %.1f"), *Name, Stage,
				                         Def->StageMinSeconds[Stage], Fastest),
				         Def->StageMinSeconds[Stage] * FKGChoreRules::TimingSlack <= Fastest + 0.05f);
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// SPRINT-025 item 3: failing never resets more than the current stage. Garbage input for 10 s on a later stage, plus a
// server retry, leaves the minigame on that stage (only the host's RestartAt, a cheat path, can go back).
// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChoreNoRegressTest, "KillGodot.Chores.FailKeepsStage",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChoreNoRegressTest::RunTest(const FString& Parameters)
{
	FRandomStream Rand(7);
	for (const FName Id : FKGMinigameFactory::GetIds())
	{
		const FString Name = Id.ToString();
		TUniquePtr<FKGMinigame> Game = FKGMinigameFactory::Create(Id);
		if (!TestTrue(Name + TEXT(": created"), Game.IsValid()) || Game->NumStages() < 2)
		{
			continue;
		}
		const int32 Stage = Game->NumStages() - 1;
		Game->Start(99, Stage);
		int32 Solves = 0;
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const FVector2f P(Rand.FRandRange(0.0f, KGMg::W), Rand.FRandRange(0.0f, KGMg::H));
			switch (Frame % 7)
			{
			case 0: Game->HostPress(P); break;
			case 2: Game->HostMove(P, FVector2f(Rand.FRandRange(-80.0f, 80.0f), Rand.FRandRange(-80.0f, 80.0f))); break;
			case 4: Game->HostRelease(P); break;
			case 5: Game->HostKey(Frame % 2 ? EKeys::SpaceBar : EKeys::LeftShift); break;
			default: break;
			}
			Game->HostTick(1.0f / 60.0f, false);
			if (Game->IsStageSolved())
			{
				++Solves;
				Game->RetryStage();   // the server said no: play the stage again, never an earlier one
			}
			Game->DrainFeedback();
			if (!TestEqual(Name + TEXT(": garbage input never drops a stage"), Game->GetStage(), Stage))
			{
				break;
			}
		}
		Game->RetryStage();
		TestEqual(Name + TEXT(": retry keeps the stage"), Game->GetStage(), Stage);
		TestFalse(Name + TEXT(": retry clears the solve"), Game->IsStageSolved());
	}
	return true;
}

#endif
