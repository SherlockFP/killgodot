#include "World/KGSpinner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

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
	const bool bMoves = !SpinRate.IsNearlyZero() || SwayDegrees != 0.0f || BobCm != 0.0f;
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
