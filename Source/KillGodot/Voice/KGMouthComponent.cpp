#include "Voice/KGMouthComponent.h"

UKGMouthComponent::UKGMouthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

float UKGMouthComponent::AmplitudeToJaw(float LinearAmplitude, float InGateDb, float InMaxDb, float InGamma)
{
	if (LinearAmplitude <= 0.0f || InMaxDb <= InGateDb)
	{
		return 0.0f;
	}
	const float Db = 20.0f * FMath::LogX(10.0f, LinearAmplitude);
	if (Db <= InGateDb)
	{
		return 0.0f;
	}
	const float Normalized = FMath::Clamp((Db - InGateDb) / (InMaxDb - InGateDb), 0.0f, 1.0f);
	return FMath::Pow(Normalized, InGamma);
}

void UKGMouthComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Target = AmplitudeToJaw(TargetAmplitude, GateDb, MaxDb, Gamma);
	const float TimeConstant = Target > JawOpen ? AttackSeconds : ReleaseSeconds;
	const float Alpha = 1.0f - FMath::Exp(-DeltaTime / FMath::Max(TimeConstant, 1e-4f));
	JawOpen = FMath::Lerp(JawOpen, Target, Alpha);
}
