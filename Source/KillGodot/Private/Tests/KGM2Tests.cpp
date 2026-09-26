#include "Misc/AutomationTest.h"
#include "Character/KGStamina.h"
#include "Combat/KGHealthComponent.h"
#include "World/KGDoor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGStaminaTest, "KillGodot.Character.Stamina",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGStaminaTest::RunTest(const FString& Parameters)
{
	FKGStamina S;
	TestTrue(TEXT("Sprinting while moving"), S.Tick(1.0f, true, true));
	TestEqual(TEXT("Drains 18/s"), S.Current, 82.0f);
	TestFalse(TEXT("No sprint when standing still"), S.Tick(0.1f, true, false));

	// Sprint to exhaustion.
	for (int32 i = 0; i < 100 && !S.bExhausted; ++i)
	{
		S.Tick(0.1f, true, true);
	}
	TestTrue(TEXT("Exhausted at zero"), S.bExhausted);
	TestFalse(TEXT("Cannot sprint while exhausted"), S.Tick(0.1f, true, true));
	TestFalse(TEXT("Cannot shove while exhausted"), S.TrySpend(10.0f));

	// Regen waits for the delay, then recovers past the threshold.
	S.Tick(0.5f, false, false);
	TestTrue(TEXT("Still in regen delay"), S.Current < 1.0f);
	for (int32 i = 0; i < 30; ++i)
	{
		S.Tick(0.1f, false, false);
	}
	TestFalse(TEXT("Recovered from exhaustion"), S.bExhausted);
	TestTrue(TEXT("Shove spends stamina"), S.TrySpend(10.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGHealthTest, "KillGodot.Combat.Health",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGHealthTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Damage subtracts"), UKGHealthComponent::ComputeHealth(100.0f, 35.0f, 100.0f), 65.0f);
	TestEqual(TEXT("Never below zero"), UKGHealthComponent::ComputeHealth(20.0f, 1000.0f, 100.0f), 0.0f);
	TestEqual(TEXT("Heal is capped"), UKGHealthComponent::ComputeHealth(90.0f, -40.0f, 100.0f), 100.0f);

	// Second wind: nothing before the delay, Rate x Dt after it, never above the cap, never for the dead.
	TestEqual(TEXT("No regen right after a hit"), UKGHealthComponent::RegenStep(40.0f, 5.0f, 1.0f, 12.0f, 1.5f, 60.0f), 40.0f);
	TestEqual(TEXT("Regen after the delay"), UKGHealthComponent::RegenStep(40.0f, 12.0f, 2.0f, 12.0f, 1.5f, 60.0f), 43.0f);
	TestEqual(TEXT("Regen stops at the cap"), UKGHealthComponent::RegenStep(59.5f, 30.0f, 1.0f, 12.0f, 1.5f, 60.0f), 60.0f);
	TestEqual(TEXT("No regen above the cap"), UKGHealthComponent::RegenStep(80.0f, 30.0f, 1.0f, 12.0f, 1.5f, 60.0f), 80.0f);
	TestEqual(TEXT("The dead stay dead"), UKGHealthComponent::RegenStep(0.0f, 30.0f, 1.0f, 12.0f, 1.5f, 60.0f), 0.0f);

	UKGHealthComponent* Health = NewObject<UKGHealthComponent>();
	TestEqual(TEXT("Applied damage returned"), Health->ApplyDamage(35.0f, nullptr, NAME_None), 35.0f);
	Health->ApplyDamage(35.0f, nullptr, NAME_None);
	TestEqual(TEXT("Two frontal hits leave 30"), Health->GetHealth(), 30.0f);
	Health->ApplyDamage(1000.0f, nullptr, NAME_None);
	TestTrue(TEXT("Dead"), Health->IsDead());
	TestEqual(TEXT("No damage after death"), Health->ApplyDamage(10.0f, nullptr, NAME_None), 0.0f);
	TestEqual(TEXT("No healing the dead"), Health->Heal(50.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGDoorTest, "KillGodot.World.DoorSwing",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGDoorTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Limited step"), AKGDoor::StepAngle(0.0f, 100.0f, 260.0f, 0.1f), 26.0f);
	TestEqual(TEXT("Reaches target without overshoot"), AKGDoor::StepAngle(95.0f, 100.0f, 260.0f, 0.1f), 100.0f);
	TestEqual(TEXT("Closes"), AKGDoor::StepAngle(100.0f, 0.0f, 260.0f, 0.1f), 74.0f);
	float Angle = 0.0f;
	for (int32 i = 0; i < 60; ++i)
	{
		Angle = AKGDoor::StepAngle(Angle, 100.0f, 260.0f, 1.0f / 60.0f);
	}
	TestEqual(TEXT("Fully open within a second"), Angle, 100.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
