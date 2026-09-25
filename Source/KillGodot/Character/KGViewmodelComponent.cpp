#include "Character/KGViewmodelComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Core/KGGameUserSettings.h"

namespace
{
	constexpr float CsUnitToCm = 2.54f;
	constexpr float SubstepHz = 240.0f;
	constexpr int32 MaxSubsteps = 8;
}

FKGViewmodelSettings FKGViewmodelSettings::FromPreset(int32 InPreset)
{
	FKGViewmodelSettings S;
	S.Preset = InPreset;
	// FPArms2 is posed in camera space for a 72 deg viewmodel FOV. Playtest: "still very big" -> every preset pushes the
	// rig forward (smaller) and down/right; Desktop (default) is the most compact. Numbers from
	// Tools/Blender/kg_fp2_metrics.py (knife idle, 16:9): Desktop fills 6.8% of the frame (was 10.5% at offset 0 / FOV 72),
	// knife tip at v -0.43 (was -0.27; 0 = crosshair, -1 = bottom edge), palm in the lower-right third (+0.58, -0.87).
	// Offsets are CS units (right, forward, up), 1 unit = 2.54 cm.
	switch (InPreset)
	{
	case 2: // Couch: a little bigger for a TV (7.3% of the frame at knife idle); slash follow-through stays off the near plane.
		S.Offset = FVector(0.0, 2.5, -1.5);
		S.ViewmodelFOV = 68.0f;
		break;
	case 3: // Classic: further right, mid size (7.2% at knife idle).
		S.Offset = FVector(1.0, 2.0, -1.5);
		S.ViewmodelFOV = 72.0f;
		break;
	default: // 1 = Desktop: smallest and lowest.
		S.Preset = 1;
		S.Offset = FVector(0.5, 2.5, -2.0);
		S.ViewmodelFOV = 74.0f;
		break;
	}
	return S;
}

void UKGViewmodelComponent::SampleInspect(float T, FVector& OutLoc, FVector& OutRot) const
{
	// FPArms2: the inspect is a keyframed arms clip (A_FP2_knife_inspect, 3.2 s) that already raises, turns and
	// flips the item. The pivot stays put so the clip is not rotated twice; only the timing (IsInspecting) is kept.
	OutLoc = OutRot = FVector::ZeroVector;
}

float UKGViewmodelComponent::GetInspectItemSpin() const
{
	// The finger flip lives in the clip's weapon_r track now; no procedural twirl on top.
	return 0.0f;
}

UKGViewmodelComponent::UKGViewmodelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

void UKGViewmodelComponent::Initialize(USceneComponent* InPivot, UCameraComponent* InCamera)
{
	Pivot = InPivot;
	Camera = InCamera;
	// The player's saved preset (Settings menu) wins unless a custom setup (Preset 0) was authored/applied.
	const UKGGameUserSettings* User = UKGGameUserSettings::Get();
	ApplySettings(Settings.Preset != 0 ? FKGViewmodelSettings::FromPreset(User ? User->GetViewmodelPreset() : 1) : Settings);
}

void UKGViewmodelComponent::ApplySettings(const FKGViewmodelSettings& InSettings)
{
	Settings = InSettings;
	// CS offsets are (right, forward, up); the pivot is in camera space (forward, right, up).
	BaseLocation = FVector(Settings.Offset.Y, Settings.Offset.X, Settings.Offset.Z) * CsUnitToCm;
	if (Pivot)
	{
		Pivot->SetRelativeScale3D(FVector(1.0, Settings.bLeftHanded ? -1.0 : 1.0, 1.0));
	}
	if (Camera)
	{
		Camera->SetEnableFirstPersonFieldOfView(true);
		Camera->SetFirstPersonFieldOfView(Settings.ViewmodelFOV);
	}
}

void UKGViewmodelComponent::AddLookInput(float YawDelta, float PitchDelta)
{
	PendingLook += FVector2D(YawDelta, PitchDelta);
}

void UKGViewmodelComponent::AddRecoil(FVector LocationImpulse, FVector RotationImpulse)
{
	RecoilLocSpring.V += LocationImpulse;
	RecoilRotSpring.V += RotationImpulse;
}

void UKGViewmodelComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Pivot || DeltaTime <= 0.0f)
	{
		return;
	}
	// Follow a preset change made in the Settings menu while playing.
	if (Settings.Preset != 0)
	{
		const UKGGameUserSettings* User = UKGGameUserSettings::Get();
		if (User && User->GetViewmodelPreset() != Settings.Preset)
		{
			ApplySettings(FKGViewmodelSettings::FromPreset(User->GetViewmodelPreset()));
		}
	}

	// Look velocity (deg/s) drives sway, so it is frame-rate independent.
	const float YawVel = static_cast<float>(PendingLook.X) / DeltaTime;
	const float PitchVel = static_cast<float>(PendingLook.Y) / DeltaTime;
	PendingLook = FVector2D::ZeroVector;
	auto Sway = [this](float Value) { return FMath::Clamp(Value, -MaxSwayDegrees, MaxSwayDegrees) * Settings.SwayScale; };
	const FVector SwayGoal(Sway(PitchVel * SwayDegreesPerDegPerSec), Sway(-YawVel * SwayDegreesPerDegPerSec),
	                       Sway(-YawVel * SwayDegreesPerDegPerSec * 0.5f));

	// Figure-8 bob from horizontal speed.
	const AActor* Owner = GetOwner();
	const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.0f;
	BobPhase = FMath::Fmod(BobPhase + DeltaTime * Speed * 0.035f, 2.0f * PI);
	const float BobAlpha = FMath::Clamp(Speed / 580.0f, 0.0f, 1.0f) * Settings.BobScale;
	const FVector BobGoal(0.0, FMath::Sin(BobPhase) * BobAmplitudeCm.Y, FMath::Sin(2.0f * BobPhase) * BobAmplitudeCm.Z);

	// Inspect and backstab share the "pose" springs; backstab readiness wins.
	FVector InspectLoc = FVector::ZeroVector;
	FVector InspectRot = FVector::ZeroVector;
	if (InspectElapsed >= 0.0f)
	{
		InspectElapsed += DeltaTime;
		SampleInspect(InspectElapsed, InspectLoc, InspectRot);
		if (InspectElapsed >= InspectDuration || bBackstabReady)
		{
			InspectElapsed = -1.0f;
			InspectLoc = InspectRot = FVector::ZeroVector;
		}
	}
	const FVector StabLocGoal = bBackstabReady ? BackstabLocation : InspectLoc;
	const FVector StabRotGoal = bBackstabReady ? BackstabRotation : InspectRot;

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime * SubstepHz), 1, MaxSubsteps);
	const float Dt = DeltaTime / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		SwaySpring.Update(SwayGoal, SwayHalfLife, Dt);
		BobSpring.Update(BobGoal * BobAlpha, BobHalfLife, Dt);
		RecoilLocSpring.Update(FVector::ZeroVector, RecoilHalfLife, Dt);
		RecoilRotSpring.Update(FVector::ZeroVector, RecoilHalfLife, Dt);
		BackstabLocSpring.Update(StabLocGoal, BackstabHalfLife, Dt);
		BackstabRotSpring.Update(StabRotGoal, BackstabHalfLife, Dt);
	}

	const FVector Location = BaseLocation + BobSpring.X + RecoilLocSpring.X + BackstabLocSpring.X;
	const FVector Rot = SwaySpring.X + RecoilRotSpring.X + BackstabRotSpring.X; // pitch, yaw, roll
	Pivot->SetRelativeLocationAndRotation(Location, FRotator(Rot.X, Rot.Y, Rot.Z));
}
