#include "Misc/AutomationTest.h"
#include "Character/KGCharacter.h"
#include "Voice/KGMouthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGBackstabGeometryTest, "KillGodot.Combat.BackstabGeometry",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGBackstabGeometryTest::RunTest(const FString& Parameters)
{
	const FVector Target(0, 0, 0);
	const FVector TargetFacingX(1, 0, 0); // target looks down +X
	const float Range = 150.0f;

	// Directly behind, both facing +X.
	TestTrue(TEXT("Behind -> stab"),
	         AKGCharacter::IsBackstabGeometry(FVector(-100, 0, 0), FVector(1, 0, 0), Target, TargetFacingX, Range));
	// Face to face.
	TestFalse(TEXT("Face to face -> no stab"),
	          AKGCharacter::IsBackstabGeometry(FVector(100, 0, 0), FVector(-1, 0, 0), Target, TargetFacingX, Range));
	// Standing at the target's side, looking at it.
	TestFalse(TEXT("From the side -> no stab"),
	          AKGCharacter::IsBackstabGeometry(FVector(0, -100, 0), FVector(0, 1, 0), Target, TargetFacingX, Range));
	// Behind but looking away.
	TestFalse(TEXT("Behind, looking away -> no stab"),
	          AKGCharacter::IsBackstabGeometry(FVector(-100, 0, 0), FVector(-1, 0, 0), Target, TargetFacingX, Range));
	// Behind but out of range.
	TestFalse(TEXT("Too far -> no stab"),
	          AKGCharacter::IsBackstabGeometry(FVector(-200, 0, 0), FVector(1, 0, 0), Target, TargetFacingX, Range));
	// Behind at a 30 degree offset: still valid.
	const FVector Offset = FVector(-1, 0.5, 0).GetSafeNormal() * 100.0;
	TestTrue(TEXT("Behind at an angle -> stab"),
	         AKGCharacter::IsBackstabGeometry(Offset, (Target - Offset).GetSafeNormal(), Target, TargetFacingX, Range));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGMouthCurveTest, "KillGodot.Voice.MouthCurve",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGMouthCurveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Silence -> closed"), UKGMouthComponent::AmplitudeToJaw(0.0f, -45.0f, -6.0f, 0.6f), 0.0f);
	TestEqual(TEXT("Below gate -> closed"), UKGMouthComponent::AmplitudeToJaw(0.001f, -45.0f, -6.0f, 0.6f), 0.0f);
	TestEqual(TEXT("Loud -> fully open"), UKGMouthComponent::AmplitudeToJaw(1.0f, -45.0f, -6.0f, 0.6f), 1.0f);
	const float Quiet = UKGMouthComponent::AmplitudeToJaw(0.02f, -45.0f, -6.0f, 0.6f);
	const float Normal = UKGMouthComponent::AmplitudeToJaw(0.1f, -45.0f, -6.0f, 0.6f);
	TestTrue(TEXT("Monotonic"), Quiet > 0.0f && Normal > Quiet && Normal < 1.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
