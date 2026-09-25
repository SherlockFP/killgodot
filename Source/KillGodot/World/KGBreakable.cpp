#include "World/KGBreakable.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "World/KGBuoyancyComponent.h"
#include "Audio/KGAudio.h"
#include "Inventory/KGLoot.h"


AKGBreakable::AKGBreakable()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCanEverAffectNavigation(false);

	Buoyancy = CreateDefaultSubobject<UKGBuoyancyComponent>(TEXT("Buoyancy"));

	// Planks from the village kit make convincing splinters.
	ConstructorHelpers::FObjectFinder<UStaticMesh> Plank(
		TEXT("/Game/KillGodot/Env/KG_Village/StaticMeshes/Floor_WoodDark.Floor_WoodDark"));
	if (Plank.Succeeded())
	{
		DebrisMesh = Plank.Object;
	}
}

void AKGBreakable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGBreakable, bBroken);
}

void AKGBreakable::SetMesh(UStaticMesh* NewMesh)
{
	Mesh->SetStaticMesh(NewMesh);
}

float AKGBreakable::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                               AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || bBroken)
	{
		return Applied;
	}
	if (DamageCauser)
	{
		LastHitDir = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	}
	Health -= DamageAmount;
	if (Health <= 0.0f)
	{
		bBroken = true;
		OnRep_Broken();
		SetLifeSpan(15.0f);
		if (!LootTable.IsNone())
		{
			FKGRng Rng = FKGLoot::MakeRng(static_cast<uint64>(LootSeed), this);
			FKGLoot::RollAndSpawn(this, LootTable, Rng, Mesh->Bounds.Origin);
		}
	}
	return Applied;
}

void AKGBreakable::OnRep_Broken()
{
	if (bBroken)
	{
		Burst();
	}
}

void AKGBreakable::Burst()
{
	KGAudio::At(this, TEXT("S_CrateBreak"), Mesh->Bounds.Origin);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetVisibility(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetNetMode() == NM_DedicatedServer || !DebrisMesh)
	{
		return;
	}
	// Local, short-lived splinters: small scaled planks flung away from the hit.
	const FBoxSphereBounds B = Mesh->Bounds;
	const float Size = FMath::Max(B.BoxExtent.GetMax() / 100.0f, 0.2f);
	for (int32 i = 0; i < DebrisCount; ++i)
	{
		const FVector Offset = FMath::VRand() * B.BoxExtent.GetMin() * 0.6f;
		AStaticMeshActor* Piece = GetWorld()->SpawnActor<AStaticMeshActor>(B.Origin + Offset, FMath::VRand().Rotation());
		if (!Piece)
		{
			continue;
		}
		Piece->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* C = Piece->GetStaticMeshComponent();
		C->SetStaticMesh(DebrisMesh);
		C->SetWorldScale3D(FVector(0.18f * Size, 0.05f * Size * FMath::FRandRange(0.6f, 1.4f), 1.0f));
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetSimulatePhysics(true);
		C->AddImpulse((LastHitDir * 250.0f + FMath::VRand() * 220.0f + FVector(0, 0, 260.0f)), NAME_None, true);
		Piece->SetLifeSpan(9.0f + FMath::FRand() * 3.0f);
	}
	UE_LOG(LogKillGodot, Verbose, TEXT("%s burst"), *GetName());
}
