#include "World/KGFishSchool.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "World/KGWaves.h"

namespace
{
	float FishHash(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return static_cast<float>(X & 0xFFFF) / 65535.0f;
	}
}

AKGFishSchool::AKGFishSchool()
{
	PrimaryActorTick.bCanEverTick = true;
	Fish = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fish"));
	SetRootComponent(Fish);
	Fish->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fish->SetCanEverAffectNavigation(false);
	Fish->SetCastShadow(false);
	Fish->SetMobility(EComponentMobility::Movable);
}

void AKGFishSchool::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Fish->SetStaticMesh(FishMesh);
	Fish->ClearInstances();
	for (int32 i = 0; i < Count; ++i)
	{
		Fish->AddInstance(FTransform::Identity, false);
	}
}

void AKGFishSchool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || Fish->GetInstanceCount() == 0)
	{
		return;
	}
	const FVector Home = GetActorLocation();
	const float T = GetWorld()->GetTimeSeconds();

	// Scatter when a pawn is within the school.
	bool bThreat = false;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Home) < FMath::Square(Radius * 1.4f))
		{
			bThreat = true;
			break;
		}
	}
	Panic = FMath::FInterpTo(Panic, bThreat ? 1.0f : 0.0f, DeltaSeconds, bThreat ? 4.0f : 0.5f);

	const int32 N = Fish->GetInstanceCount();
	TArray<FTransform> Xf;
	Xf.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const float Ri = Radius * (0.45f + 0.55f * FishHash(i * 97u + 1)) * (1.0f + 0.8f * Panic);
		const float Dir = FishHash(i * 13u + 7) < 0.15f ? -1.0f : 1.0f;   // a few swim against the stream
		const float W = Dir * Speed * (1.0f + 1.5f * Panic) / Ri;
		const float A = FishHash(i * 31u + 3) * 2.0f * PI + T * W;
		const float Depth = FMath::Lerp(MinDepth, MaxDepth, FishHash(i * 53u + 5)) + 25.0f * FMath::Sin(T * 0.7f + i);
		const FVector P(Home.X + Ri * FMath::Cos(A), Home.Y + Ri * 0.75f * FMath::Sin(A),
		                Home.Z - Depth);   // surface = the school actor height (sea: 0, garden ponds: pond level)
		// Tangent of the (squashed) circle = heading; the mesh's +X is the head.
		const FVector Tangent(-FMath::Sin(A) * Dir, 0.75f * FMath::Cos(A) * Dir, 0.0f);
		const float S = FishScale * (0.85f + 0.3f * FishHash(i * 71u + 9));
		Xf[i] = FTransform(Tangent.Rotation(), P - Home, FVector(S));
	}
	Fish->BatchUpdateInstancesTransforms(0, Xf, false, true);
}
