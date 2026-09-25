#include "World/KGDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Audio/KGAudio.h"


AKGDoor::AKGDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetDormancy(DORM_Initial);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Frame: a lintel above a 100 x 210 cm opening (greybox; art pass swaps the meshes).
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Frame->SetRelativeLocation(FVector(0.0f, 50.0f, 220.0f));
	Frame->SetRelativeScale3D(FVector(0.2f, 1.2f, 0.2f));

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(Hinge);
	Leaf->SetMobility(EComponentMobility::Movable);
	Leaf->SetRelativeLocation(FVector(0.0f, 50.0f, 105.0f));
	Leaf->SetRelativeScale3D(FVector(0.08f, 1.0f, 2.1f));

	// Village kit door (Quaternius, CC0): the hinge edge is the mesh origin and the width runs along its X, so a
	// 90 degree yaw lays it along the door's Y. The kit wall pieces already have the arch, so no frame mesh.
	// Non-static finder: Live Coding keeps stale statics.
	ConstructorHelpers::FObjectFinder<UStaticMesh> KitDoor(
		TEXT("/Game/KillGodot/Env/KG_Village/StaticMeshes/Door_1_Round.Door_1_Round"));
	if (KitDoor.Succeeded())
	{
		Leaf->SetStaticMesh(KitDoor.Object);
		Leaf->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f));
		Leaf->SetRelativeScale3D(FVector(1.15f, 1.0f, 1.03f)); // fill the kit arch (opening ~123 cm)
		Frame->SetVisibility(false);
		Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else if (Cube.Succeeded())
	{
		Frame->SetStaticMesh(Cube.Object);
		Leaf->SetStaticMesh(Cube.Object);
	}

	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
}

void AKGDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGDoor, bOpen);
	DOREPLIFETIME(AKGDoor, bLocked);
	DOREPLIFETIME(AKGDoor, bBroken);
}

void AKGDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBroken)
	{
		return;
	}
	const float Target = bOpen ? OpenAngle : 0.0f;
	static TMap<TWeakObjectPtr<const AKGDoor>, bool> WasMoving;   // no header change needed (Live Coding)
	bool& bMovingBefore = WasMoving.FindOrAdd(this);
	const bool bMoving = !FMath::IsNearlyEqual(CurrentAngle, Target);
	if (bMoving && !bMovingBefore)
	{
		KGAudio::At(this, bOpen ? TEXT("S_DoorOpen") : TEXT("S_DoorClose"), GetActorLocation() + FVector(0, 0, 120.0f));
	}
	bMovingBefore = bMoving;
	if (bMoving)
	{
		CurrentAngle = StepAngle(CurrentAngle, Target, OpenSpeedDegPerSec, DeltaSeconds);
		Hinge->SetRelativeRotation(FRotator(0.0f, CurrentAngle, 0.0f));
	}
}

void AKGDoor::Interact_Implementation(AKGCharacter* By)
{
	if (bBroken || !HasAuthority())
	{
		return;
	}
	if (bLocked)
	{
		// TODO(M8): rattle sound + keys/lockpicks from the inventory.
		UE_LOG(LogKillGodot, Log, TEXT("%s is locked"), *GetName());
		return;
	}
	FlushNetDormancy();
	bOpen = !bOpen;
}

FText AKGDoor::GetInteractPrompt_Implementation() const
{
	if (bBroken)
	{
		return FText::GetEmpty();
	}
	if (bLocked)
	{
		return NSLOCTEXT("KillGodot", "DoorLocked", "Locked");
	}
	return bOpen ? NSLOCTEXT("KillGodot", "DoorClose", "Close") : NSLOCTEXT("KillGodot", "DoorOpen", "Open");
}

float AKGDoor::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                          AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (HasAuthority() && !bBroken)
	{
		DoorHealth -= Applied;
		if (DoorHealth <= 0.0f)
		{
			FlushNetDormancy();
			bBroken = true;
			Break();
		}
	}
	return Applied;
}

void AKGDoor::OnRep_Broken()
{
	if (bBroken)
	{
		Break();
	}
}

void AKGDoor::Break()
{
	// TODO(M12): swap to a Geometry Collection burst (replicated break event, cosmetic fragments).
	Leaf->SetCollisionProfileName(TEXT("PhysicsActor"));
	Leaf->SetSimulatePhysics(true);
	Leaf->AddImpulse(Leaf->GetRightVector() * 300.0f, NAME_None, true);
}
