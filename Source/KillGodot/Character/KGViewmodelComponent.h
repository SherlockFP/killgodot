#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGViewmodelComponent.generated.h"

class UCameraComponent;

/** CS2-style viewmodel preferences (Docs/05_Tech_Architecture.md §6, Docs/08_UI_UX.md §7.1). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGViewmodelSettings
{
	GENERATED_BODY()

	/** 1 = Desktop, 2 = Couch, 3 = Classic (like viewmodel_presetpos). 0 = custom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel")
	int32 Preset = 1;

	/** CS semantics: X = right, Y = forward, Z = up, in CS units (converted to cm internally). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel", meta = (ClampMin = "-2.5", ClampMax = "2.5"))
	FVector Offset = FVector(1.0, 1.0, -1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel", meta = (ClampMin = "54", ClampMax = "68"))
	float ViewmodelFOV = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel")
	bool bLeftHanded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel", meta = (ClampMin = "0", ClampMax = "1"))
	float BobScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewmodel", meta = (ClampMin = "0", ClampMax = "1"))
	float SwayScale = 1.0f;

	static FKGViewmodelSettings FromPreset(int32 InPreset);
};

/** Critically damped spring parameterised by half-life (D. Holden, "Spring-It-On"). */
struct FKGSpring
{
	FVector X = FVector::ZeroVector;
	FVector V = FVector::ZeroVector;

	void Update(const FVector& Goal, float HalfLife, float Dt)
	{
		const float Y = (4.0f * 0.69314718f) / (HalfLife + 1e-5f) * 0.5f;
		const FVector J0 = X - Goal;
		const FVector J1 = V + J0 * Y;
		const float EyDt = FMath::Exp(-Y * Dt);
		X = EyDt * (J0 + J1 * Dt) + Goal;
		V = EyDt * (V - J1 * Y * Dt);
	}
};

/**
 * Procedural first-person layer on top of the arms animation: sway, bob, recoil and the TF2-style
 * "backstab ready" raise. Runs on the locally controlled client only and moves the viewmodel pivot.
 * Springs are sub-stepped at 240 Hz so the feel is identical at 30 and 300 fps.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGViewmodelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGViewmodelComponent();

	void Initialize(USceneComponent* InPivot, UCameraComponent* InCamera);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Viewmodel")
	void ApplySettings(const FKGViewmodelSettings& InSettings);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Viewmodel")
	const FKGViewmodelSettings& GetSettings() const { return Settings; }

	/** Mouse/stick look delta this frame (degrees). */
	void AddLookInput(float YawDelta, float PitchDelta);

	/** Kick the weapon: location impulse (cm/s, X fwd) and rotation impulse (deg/s, pitch/yaw/roll in X/Y/Z). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Viewmodel")
	void AddRecoil(FVector LocationImpulse, FVector RotationImpulse);

	/** TF2-spy style: blade rises into the stab pose while a valid back is in range. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Viewmodel")
	void SetBackstabReady(bool bReady) { bBackstabReady = bReady; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Viewmodel")
	bool IsBackstabReady() const { return bBackstabReady; }

	/** CS2-style inspect: the item is turned towards the camera and back. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Viewmodel")
	void StartInspect() { InspectElapsed = 0.0f; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Viewmodel")
	bool IsInspecting() const { return InspectElapsed >= 0.0f && InspectElapsed < InspectDuration; }

	/** Degrees the held item is twirled around its own long axis right now (0 outside the twirl beat). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Viewmodel")
	float GetInspectItemSpin() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	FKGViewmodelSettings Settings;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float SwayHalfLife = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float SwayDegreesPerDegPerSec = 0.012f;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float MaxSwayDegrees = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float BobHalfLife = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	FVector BobAmplitudeCm = FVector(0.0, 0.35, 0.25);

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float RecoilHalfLife = 0.09f;

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float BackstabHalfLife = 0.06f;

	/** Raised "about to stab" pose: location (cm, X fwd/Y right/Z up) and rotation (pitch, yaw, roll). */
	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	FVector BackstabLocation = FVector(3.0, -1.0, 4.0);

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	FVector BackstabRotation = FVector(3.0, 1.0, -8.0);

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	float InspectDuration = 3.2f;

	/** Peak inspect pose: location (cm) and rotation (pitch, yaw, roll). */
	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	FVector InspectLocation = FVector(3.0, -5.0, 3.0);

	UPROPERTY(EditAnywhere, Category = "Viewmodel|Tuning")
	FVector InspectRotation = FVector(4.0, -8.0, 16.0);

private:
	void SampleInspect(float T, FVector& OutLoc, FVector& OutRot) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	FVector BaseLocation = FVector::ZeroVector;
	FVector2D PendingLook = FVector2D::ZeroVector;
	float BobPhase = 0.0f;
	bool bBackstabReady = false;
	float InspectElapsed = -1.0f;

	FKGSpring SwaySpring;
	FKGSpring BobSpring;
	FKGSpring RecoilLocSpring;
	FKGSpring RecoilRotSpring;
	FKGSpring BackstabLocSpring;
	FKGSpring BackstabRotSpring;
};
