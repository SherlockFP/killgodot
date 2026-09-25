#include "World/KGSpinner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Core/KGGameState.h"

namespace KGSpinnerClock
{
	// The village clock shows the match: hours of the in-world day from the phase and its progress (FKGMatchClock).
	static bool Hours(const UWorld* World, float& OutHours)
	{
		const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
		if (!GS)
		{
			return false;
		}
		const float A = GS->Clock.GetAlpha();
		switch (GS->GetPhase())
		{
		case EKGPhase::Dawn:    OutHours = 5.0f + A; return true;
		case EKGPhase::Day:     OutHours = 6.0f + 12.0f * A; return true;
		case EKGPhase::Meeting: OutHours = 18.0f + 1.5f * A; return true;
		case EKGPhase::Trial:   OutHours = 19.5f + 0.5f * A; return true;
		case EKGPhase::Night:   OutHours = 20.0f + 9.0f * A; return true;
		default:                return false;   // lobby, reveal, epilogue: the hands just turn
		}
	}
}

AKGSpinner::AKGSpinner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCanEverAffectNavigation(false);
}

void AKGSpinner::SetMesh(UStaticMesh* NewMesh)
{
	Mesh->SetStaticMesh(NewMesh);
}

void AKGSpinner::BeginPlay()
{
	Super::BeginPlay();
	BaseRotation = GetActorRotation();
	BaseLocation = GetActorLocation();
	// Neighbouring signs must not swing in lockstep: phase from the placement.
	Phase = FMath::Frac((BaseLocation.X * 0.0137f) + (BaseLocation.Y * 0.0071f)) * 2.0f * PI;
	const FString MeshName = Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetName() : FString();
	ClockHand = MeshName.Contains(TEXT("ClockHand_Minute")) ? 2 : (MeshName.Contains(TEXT("ClockHand_Hour")) ? 1 : 0);
	const bool bMoves = !SpinRate.IsNearlyZero() || SwayDegrees != 0.0f || BobCm != 0.0f || ClockHand != 0;
	SetActorTickEnabled(bMoves);
}

void AKGSpinner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Mesh->WasRecentlyRendered(0.5f))
	{
		return;
	}
	const float T = GetWorld()->GetTimeSeconds();
	FRotator Local = SpinRate * T;
	float Hours = 0.0f;
	if (ClockHand != 0 && KGSpinnerClock::Hours(GetWorld(), Hours))
	{
		// Clockwise on the dial is negative pitch (the builder's SpinRate); 12 o'clock is the placed pose.
		Local = FRotator(ClockHand == 2 ? -360.0f * FMath::Frac(Hours) : -30.0f * FMath::Fmod(Hours, 12.0f), 0.0f, 0.0f);
	}
	if (SwayDegrees != 0.0f)
	{
		Local.Roll += SwayDegrees * FMath::Sin(2.0f * PI * SwayHz * T + Phase);
	}
	const FQuat Q = BaseRotation.Quaternion() * Local.Quaternion();
	if (BobCm != 0.0f)
	{
		const FVector Offset(0.0f, 0.0f, BobCm * FMath::Sin(1.3f * T + Phase));
		SetActorLocationAndRotation(BaseLocation + Offset, Q);
	}
	else
	{
		SetActorRotation(Q);
	}
}
