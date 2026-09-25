#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "UI/KGHudLayout.h"
#include "UI/KGMapInput.h"

// ---------------------------------------------------------------------------------------------------------------------
// SPRINT-025 item 2: the M key. Tap toggles the full map; hold shows it while down and hides it on release.
// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGMapInputTest, "KillGodot.HUD.MapInput",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGMapInputTest::RunTest(const FString& Parameters)
{
	using namespace KGMapInput;
	FKGMapInput S;
	double T = 10.0;

	// Tap: press, release after 0.1 s -> stays open (toggle).
	TestTrue(TEXT("press opens"), Update(S, true, T));
	TestTrue(TEXT("open on press"), S.bOpen);
	TestFalse(TEXT("not a hold yet"), IsHolding(S, T + 0.1));
	TestFalse(TEXT("quick release keeps it open"), Update(S, false, T + 0.1));
	TestTrue(TEXT("tap toggled open"), S.bOpen);
	// Idle frames change nothing.
	TestFalse(TEXT("idle"), Update(S, false, T + 1.0));
	TestTrue(TEXT("still open"), S.bOpen);
	// Second tap closes on the press; its release does nothing.
	TestTrue(TEXT("press closes"), Update(S, true, T + 2.0));
	TestFalse(TEXT("closed"), S.bOpen);
	TestFalse(TEXT("release after close: no change"), Update(S, false, T + 2.1));
	TestFalse(TEXT("still closed"), S.bOpen);

	// Hold: press, keep down past HoldSeconds -> open while down, closed on release.
	T = 20.0;
	TestTrue(TEXT("hold press opens"), Update(S, true, T));
	TestFalse(TEXT("held frames: no change"), Update(S, true, T + 0.2));
	TestTrue(TEXT("holding after the threshold"), IsHolding(S, T + HoldSeconds + 0.05));
	TestFalse(TEXT("held frames: no change (2)"), Update(S, true, T + 1.5));
	TestTrue(TEXT("open while held"), S.bOpen);
	TestTrue(TEXT("release closes a hold"), Update(S, false, T + 1.6));
	TestFalse(TEXT("closed after the hold"), S.bOpen);
	TestFalse(TEXT("not holding"), IsHolding(S, T + 1.7));

	// A hold on an already-open (tapped) map: the press closes it; holding after that does not reopen it.
	T = 30.0;
	Update(S, true, T);
	Update(S, false, T + 0.05);
	TestTrue(TEXT("tapped open"), S.bOpen);
	TestTrue(TEXT("press on open closes"), Update(S, true, T + 1.0));
	TestFalse(TEXT("long press on a closed-by-press map stays closed"), Update(S, true, T + 2.0));
	TestFalse(TEXT("release: still closed"), Update(S, false, T + 2.1));
	TestFalse(TEXT("closed"), S.bOpen);

	// Esc / pause menu.
	Update(S, true, 40.0);
	Update(S, false, 40.05);
	TestTrue(TEXT("open"), S.bOpen);
	Close(S);
	TestFalse(TEXT("Close() closes"), S.bOpen);
	TestFalse(TEXT("release after Close: nothing"), Update(S, false, 41.0));

	// Exactly at the threshold counts as a hold.
	T = 50.0;
	Update(S, true, T);
	TestTrue(TEXT("release exactly at HoldSeconds closes"), Update(S, false, T + HoldSeconds));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// SPRINT-025 item 4: the fixed HUD cards never overlap or leave the screen from 720p to 1440p (16:9 and 16:10).
// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGHudLayoutTest, "KillGodot.HUD.Layout",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGHudLayoutTest::RunTest(const FString& Parameters)
{
	const FIntPoint Sizes[] = {{1280, 720}, {1280, 800}, {1600, 900}, {1920, 1080}, {1920, 1200}, {2560, 1440}, {3440, 1440}};
	for (const FIntPoint& Size : Sizes)
	{
		for (const int32 Rows : {0, 4, 6})
		{
			for (const bool bPrep : {false, true})
			{
				const FKGHudLayout L = FKGHudLayout::Compute(static_cast<float>(Size.X), static_cast<float>(Size.Y), Rows, bPrep);
				const TArray<TPair<FString, FBox2D>> All = L.All();
				const FString Tag = FString::Printf(TEXT("%dx%d rows=%d prep=%d: "), Size.X, Size.Y, Rows, bPrep ? 1 : 0);
				for (const TPair<FString, FBox2D>& A : All)
				{
					const FBox2D& B = A.Value;
					if (B.GetSize().X <= 0.0f || B.GetSize().Y <= 0.0f)
					{
						continue;   // empty card (no chores)
					}
					TestTrue(Tag + A.Key + TEXT(" inside the screen"),
					         B.Min.X >= 0.0f && B.Min.Y >= 0.0f && B.Max.X <= Size.X + 0.01f && B.Max.Y <= Size.Y + 0.01f);
					for (const TPair<FString, FBox2D>& C : All)
					{
						if (&A == &C || C.Value.GetSize().X <= 0.0f || C.Value.GetSize().Y <= 0.0f)
						{
							continue;
						}
						TestFalse(Tag + A.Key + TEXT(" overlaps ") + C.Key, B.Intersect(C.Value));
					}
				}
				// The tracker with 6 chores still clears the vitals / purse column.
				if (Rows > 0)
				{
					TestTrue(Tag + TEXT("tracker above the purse"), L.Tracker.Max.Y < L.Purse.Min.Y);
				}
				// The markers' clamp band (150 px * S top / bottom) clears the top cards and the vitals.
				const float S = L.S;
				TestTrue(Tag + TEXT("marker band clears the phase card"), L.Phase.Max.Y <= 150.0f * S + 0.01f || L.Compass.Min.Y >= 0.0f);
				TestTrue(Tag + TEXT("marker band clears the vitals"), L.Vitals.Min.Y >= Size.Y - 150.0f * S - 0.01f);
			}
		}
	}
	return true;
}

#endif
