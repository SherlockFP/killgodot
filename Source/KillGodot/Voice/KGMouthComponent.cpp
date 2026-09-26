#include "Voice/KGMouthComponent.h"
#include "AnimationRuntime.h"
#include "Character/KGCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Voice/KGVoiceCommands.h"

namespace KGMouthPrivate
{
	TAutoConsoleVariable<float> CVarForce(TEXT("kg.Mouth.Force"), -1.0f,
		TEXT("Pin every mouth to this opening (0 closed .. 1 open) for screenshots; -1 = voice/bark driven."));
	// Mouth placement relative to the Head bone in the villager's component space (mesh +Y = forward, +Z = up).
	TAutoConsoleVariable<float> CVarFwd(TEXT("kg.Mouth.Fwd"), 9.5f, TEXT("Mouth offset forward of the Head bone (cm)."));
	TAutoConsoleVariable<float> CVarUp(TEXT("kg.Mouth.Up"), 2.5f, TEXT("Mouth offset above the Head bone (cm)."));   // 4.0 sat under the nostrils (Voice shots 2026-09-26)
	TAutoConsoleVariable<float> CVarWidth(TEXT("kg.Mouth.Width"), 4.2f, TEXT("Mouth width (cm)."));
	TAutoConsoleVariable<float> CVarOpen(TEXT("kg.Mouth.OpenHeight"), 3.2f, TEXT("Mouth height when fully open (cm)."));

	constexpr float ClosedHeight = 0.7f;
	constexpr float Depth = 1.6f;
	constexpr double PushHoldSeconds = 0.25;
}

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

void UKGMouthComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureMouthMesh();
}

void UKGMouthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MouthMesh)
	{
		MouthMesh->DestroyComponent();
		MouthMesh = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UKGMouthComponent::PushAmplitude(float LinearAmplitude)
{
	TargetAmplitude = FMath::Max(TargetAmplitude * 0.5f, LinearAmplitude);
	LastPushTime = FPlatformTime::Seconds();
}

void UKGMouthComponent::StartBark(int32 Syllables, uint32 Seed)
{
	BarkSyllables = FMath::Clamp(Syllables, 1, 8);
	BarkSeed = Seed;
	BarkTime = 0.0f;
}

void UKGMouthComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (LastPushTime > 0.0 && FPlatformTime::Seconds() - LastPushTime > KGMouthPrivate::PushHoldSeconds)
	{
		TargetAmplitude = 0.0f;
	}
	float Target = AmplitudeToJaw(TargetAmplitude, GateDb, MaxDb, Gamma);
	if (BarkSyllables > 0)
	{
		BarkTime += DeltaTime;
		if (BarkTime >= FKGVoiceCommandCatalog::BarkDuration(BarkSyllables))
		{
			BarkSyllables = 0;
		}
		else
		{
			Target = FMath::Max(Target, FKGVoiceCommandCatalog::MouthEnvelope(BarkTime, BarkSyllables, BarkSeed));
		}
	}
	const float Force = KGMouthPrivate::CVarForce.GetValueOnGameThread();
	if (Force >= 0.0f)
	{
		Target = FMath::Clamp(Force, 0.0f, 1.0f);
	}
	const float TimeConstant = Target > JawOpen ? AttackSeconds : ReleaseSeconds;
	const float Alpha = 1.0f - FMath::Exp(-DeltaTime / FMath::Max(TimeConstant, 1e-4f));
	JawOpen = Force >= 0.0f ? Target : FMath::Lerp(JawOpen, Target, Alpha);

	EnsureMouthMesh();
	ApplyMouthMesh();
}

// ---- the mouth mesh ---------------------------------------------------------------------------------------------

void UKGMouthComponent::EnsureMouthMesh()
{
	UWorld* World = GetWorld();
	if (MouthMesh || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	AKGCharacter* Char = Cast<AKGCharacter>(GetOwner());
	USkeletalMeshComponent* Body = Char ? Char->GetMesh() : nullptr;
	if (!Body || !Body->GetSkeletalMeshAsset())
	{
		return;
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Sphere)
	{
		return;
	}
	MouthMesh = NewObject<UStaticMeshComponent>(Char, TEXT("KGMouth"), RF_Transient);
	MouthMesh->SetStaticMesh(Sphere);
	MouthMesh->SetMobility(EComponentMobility::Movable);
	MouthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MouthMesh->SetGenerateOverlapEvents(false);
	MouthMesh->SetCanEverAffectNavigation(false);
	MouthMesh->SetOwnerNoSee(true);
	MouthMesh->CastShadow = false;
	MouthMesh->SetRenderCustomDepth(true);
	MouthMesh->SetupAttachment(Body, TEXT("Head"));
	MouthMesh->RegisterComponent();
	// Dark mouth interior: the character material with a near-black tint (same parameters the outfit pieces use).
	UMaterialInterface* Base = Body->GetMaterial(0);
	if (UMaterialInstanceDynamic* MID = Base ? UMaterialInstanceDynamic::Create(Base, MouthMesh) : nullptr)
	{
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.06f, 0.015f, 0.02f));
		MID->SetScalarParameterValue(TEXT("Desaturation"), 0.0f);
		MID->SetScalarParameterValue(TEXT("Glow"), 0.0f);
		MouthMesh->SetMaterial(0, MID);
	}
	PlacedFor.Reset();
	PlaceMouthMesh();
}

void UKGMouthComponent::PlaceMouthMesh()
{
	AKGCharacter* Char = Cast<AKGCharacter>(GetOwner());
	USkeletalMeshComponent* Body = Char ? Char->GetMesh() : nullptr;
	USkeletalMesh* Skeletal = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!MouthMesh || !Skeletal)
	{
		return;
	}
	if (PlacedFor.Get() == Skeletal)
	{
		return;
	}
	PlacedFor = Skeletal;
	bHeadValid = false;
	const FReferenceSkeleton& Ref = Skeletal->GetRefSkeleton();
	const int32 HeadIndex = Ref.FindBoneIndex(TEXT("Head"));
	if (HeadIndex == INDEX_NONE)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Mouth: no Head bone on %s"), *Skeletal->GetName());
		MouthMesh->SetVisibility(false);
		return;
	}
	HeadRefCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, HeadIndex);
	bHeadValid = true;
	MouthMesh->SetVisibility(true);
	UE_LOG(LogKillGodot, Log, TEXT("Mouth: %s Head bone at %s scale %s"), *Skeletal->GetName(), *HeadRefCS.GetLocation().ToCompactString(),
	       *HeadRefCS.GetScale3D().ToCompactString());
}

void UKGMouthComponent::ApplyMouthMesh()
{
	if (!MouthMesh)
	{
		return;
	}
	PlaceMouthMesh();
	if (!bHeadValid)
	{
		return;
	}
	// Place and size the oval in the villager's component space (mesh +Y = forward, +Z = up; the engine sphere is
	// 100 cm across, so scale = size / 100), then express the whole transform bone-local like the hats do: the Head
	// bone carries a non-unit scale in the reference pose, so the size must go through the same conversion.
	const FVector Offset(0.0f, KGMouthPrivate::CVarFwd.GetValueOnGameThread(), KGMouthPrivate::CVarUp.GetValueOnGameThread());
	const float Width = KGMouthPrivate::CVarWidth.GetValueOnGameThread();
	const float Height = FMath::Lerp(KGMouthPrivate::ClosedHeight, KGMouthPrivate::CVarOpen.GetValueOnGameThread(), JawOpen);
	const FTransform PlacementCS(FRotator::ZeroRotator, HeadRefCS.GetLocation() + Offset,
	                             FVector(Width, KGMouthPrivate::Depth, Height) * 0.01f);
	MouthMesh->SetRelativeTransform(UKGCosmeticsComponent::ComputeBoneRelative(PlacementCS, HeadRefCS, false));
}
