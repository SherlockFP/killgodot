#include "Misc/AutomationTest.h"
#include "Character/KGCharacterMovement.h"
#include "Character/KGStamina.h"

#if WITH_DEV_AUTOMATION_TESTS

// SPRINT-026: Source-style air strafing / skill bunny-hop. These exercise the pure math directly (no world needed),
// mirrored by the headless speed-curve run (Tools/Unreal/kg_move_smoke.ps1 + Tools/Unreal/kg_move_curve.py) and the
// two-process net smoke for the CMC-integrated behaviour (saved-move replay, corrections).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGAirStrafeTest, "KillGodot.Character.AirStrafe",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGAirStrafeTest::RunTest(const FString& Parameters)
{
	const float Dt = 1.0f / 60.0f;
	const float WishSpeed = 320.0f;
	const float Accelerate = 8.0f;
	const float SoftCap = 783.0f;   // 1.35x a 580 sprint speed

	// A "bad" strafe: no wish direction at all never adds speed.
	{
		FVector2D V(300.0f, 0.0f);
		const FVector2D Out = UKGCharacterMovement::ComputeAirStrafeVelocity2D(V, FVector2D::ZeroVector, WishSpeed,
		                                                                      Accelerate, Dt, SoftCap);
		TestTrue(TEXT("No wish input: velocity unchanged"), Out.Equals(V));
	}

	// A "bad" strafe: wishing straight backward against a fast forward velocity adds nothing (current speed along
	// wish is already very negative, so AddSpeed = WishSpeed - CurrentSpeedAlongWish is > WishSpeed but capped by
	// AddSpeed>=0 gate only blocks when CurrentSpeedAlongWish >= WishSpeed; test the true "no gain" case instead:
	// wishing the same direction you're already moving in, above wish speed, adds nothing.
	{
		FVector2D V(500.0f, 0.0f);   // already faster than WishSpeed, same direction as the wish
		const FVector2D Out = UKGCharacterMovement::ComputeAirStrafeVelocity2D(V, FVector2D(1.0f, 0.0f), WishSpeed,
		                                                                      Accelerate, Dt, SoftCap);
		TestTrue(TEXT("Already faster than wish speed along wish dir: no gain"), Out.Equals(V));
	}

	// A "good" strafe: wishing sideways (perpendicular to current velocity) always has CurrentSpeedAlongWish ~ 0, so
	// it keeps adding speed every tick - this is the classic strafe-jump / bhop gain.
	{
		FVector2D V(400.0f, 0.0f);
		const FVector2D Out = UKGCharacterMovement::ComputeAirStrafeVelocity2D(V, FVector2D(0.0f, 1.0f), WishSpeed,
		                                                                      Accelerate, Dt, SoftCap);
		TestTrue(TEXT("Perpendicular strafe increases total speed"), Out.Size() > V.Size());
		TestTrue(TEXT("Perpendicular strafe adds the expected sideways component"), Out.Y > 0.0f);
	}

	// Repeated good strafing (a wish direction kept ~45 degrees off the current velocity, re-aimed every tick - what
	// a player's mouse turn + opposite strafe key achieves) climbs past a single wish-speed step and keeps growing
	// until CurrentSpeedAlongWish (= |V|*cos(45 deg) by construction) catches up to WishSpeed; this is what "speed
	// gain only from correctly synced strafes" means in practice. The 45-degree case has an equilibrium around
	// WishSpeed/cos(45 deg) =~ 1.41x, so 1.1x after 240 ticks is a safe, non-flaky bound to check growth happened.
	{
		FVector2D V(WishSpeed, 0.0f);
		for (int32 i = 0; i < 240; ++i)
		{
			const FVector2D WishDir = V.GetSafeNormal().GetRotated(45.0f);
			V = UKGCharacterMovement::ComputeAirStrafeVelocity2D(V, WishDir, WishSpeed, Accelerate, Dt, SoftCap);
		}
		TestTrue(TEXT("Sustained good strafing climbs above plain wish speed"), V.Size() > WishSpeed * 1.1f);
	}

	// The soft cap tapers, it does not hard-clamp: pushed well above it, a tick still adds a little, but far less
	// than the same tick would add below the cap.
	{
		const FVector2D VAtCap(SoftCap, 0.0f);
		const FVector2D BelowCap(WishSpeed * 0.5f, 0.0f);
		const FVector2D GainAtCap = UKGCharacterMovement::ComputeAirStrafeVelocity2D(VAtCap, FVector2D(0.0f, 1.0f), WishSpeed, Accelerate, Dt, SoftCap) - VAtCap;
		const FVector2D GainBelowCap = UKGCharacterMovement::ComputeAirStrafeVelocity2D(BelowCap, FVector2D(0.0f, 1.0f), WishSpeed, Accelerate, Dt, SoftCap) - BelowCap;
		TestTrue(TEXT("Gain still positive right at the soft cap"), GainAtCap.Size() > 0.0f);
		TestTrue(TEXT("Gain at the soft cap is smaller than well below it"), GainAtCap.Size() < GainBelowCap.Size());
	}

	// Jump buffer: a press right before landing (small positive elapsed time) counts; a stale one (held/way earlier)
	// does not. This is what makes each hop need "a fresh jump press timed on landing" rather than auto-repeating.
	TestTrue(TEXT("A press 0ms before landing is buffered"), UKGCharacterMovement::IsWithinJumpBuffer(0.0f, 0.08f));
	TestTrue(TEXT("A press 60ms before landing is buffered"), UKGCharacterMovement::IsWithinJumpBuffer(0.06f, 0.08f));
	TestFalse(TEXT("A press 200ms before landing is stale (not a fresh press near landing)"), UKGCharacterMovement::IsWithinJumpBuffer(0.2f, 0.08f));
	TestFalse(TEXT("Never pressed (sentinel) is not buffered"), UKGCharacterMovement::IsWithinJumpBuffer(1000.0f, 0.08f));
	return true;
}

// SPRINT-026 stamina costs: each jump costs stamina, chaining costs more, and exhaustion actually stops further
// chaining (TrySpend fails once bExhausted, matching UKGCharacterMovement::bHopChainBlocked's gate).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGHopStaminaTest, "KillGodot.Character.HopStamina",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGHopStaminaTest::RunTest(const FString& Parameters)
{
	FKGStamina S;
	const float JumpCost = 6.0f;
	const float ChainBase = 8.0f;
	const float ChainStep = 3.0f;

	TestTrue(TEXT("A plain jump is affordable from full stamina"), S.TrySpend(JumpCost));
	TestEqual(TEXT("Jump cost subtracted"), S.Current, 94.0f);

	// Chain a few hops; cost rises with streak (streak 1..5), matching ChainHopStepCost * min(streak, cap).
	float Total = 94.0f;
	for (int32 Streak = 1; Streak <= 5; ++Streak)
	{
		const float Cost = ChainBase + ChainStep * FMath::Min(Streak, 5);
		TestTrue(FString::Printf(TEXT("Chain hop %d affordable"), Streak), S.TrySpend(Cost));
		Total -= Cost;
	}
	TestEqual(TEXT("Stamina matches the escalating chain cost"), S.Current, Total);

	// Drain to exhaustion, then confirm TrySpend (what the chain-hop accounting uses) refuses to spend further -
	// this is the mechanism behind bHopChainBlocked stopping the chain, without ever touching whether a plain jump
	// is allowed (CanAttemptJump does not consult stamina at all).
	while (!S.bExhausted)
	{
		S.Tick(0.1f, true, true);
	}
	TestTrue(TEXT("Exhausted"), S.bExhausted);
	TestFalse(TEXT("Chain hop cost refused while exhausted"), S.TrySpend(ChainBase));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
