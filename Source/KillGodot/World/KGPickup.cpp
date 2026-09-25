#include "World/KGPickup.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"

#define LOCTEXT_NAMESPACE "KGPickup"

AKGPickup::AKGPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	SetNetCullDistanceSquared(FMath::Square(6000.0f));

	Hitbox = CreateDefaultSubobject<USphereComponent>(TEXT("Hitbox"));
	Hitbox->InitSphereRadius(28.0f);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // the E trace and the HUD prompt
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->SetCanEverAffectNavigation(false);
	SetRootComponent(Hitbox);

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Hitbox);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Spinner);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetRenderCustomDepth(true);   // same ink outline as the characters and props

	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGPickup, ItemId);
	DOREPLIFETIME(AKGPickup, Count);
}

AKGPickup* AKGPickup::SpawnPickup(UObject* WorldContextObject, FName InItemId, int32 InCount, FVector Location,
                                  bool bSnapToGround)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
	                        : nullptr;
	if (!World || World->GetNetMode() == NM_Client || InCount <= 0 || !UKGItemCatalog::Find(InItemId))
	{
		return nullptr;
	}
	const AKGPickup* Defaults = GetDefault<AKGPickup>();
	if (bSnapToGround)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGPickupGround), false);
		const FVector Start = Location + FVector(0.0f, 0.0f, 60.0f);
		if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0f, 0.0f, 600.0f), ECC_WorldStatic, Params))
		{
			Location = Hit.ImpactPoint + FVector(0.0f, 0.0f, Defaults->HoverHeight);
		}
	}
	const FTransform Transform(FRotator::ZeroRotator, Location);
	AKGPickup* Pickup = World->SpawnActorDeferred<AKGPickup>(AKGPickup::StaticClass(), Transform, nullptr, nullptr,
	                                                         ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}
	Pickup->ItemId = InItemId;
	Pickup->Count = InCount;
	Pickup->FinishSpawning(Transform);
	return Pickup;
}

void AKGPickup::SetItem(FName InItemId, int32 InCount)
{
	if (!HasAuthority() || !UKGItemCatalog::Find(InItemId))
	{
		return;
	}
	ItemId = InItemId;
	Count = FMath::Max(1, InCount);
	ApplyLook();
	ForceNetUpdate();
}

void AKGPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AppliedItem = NAME_None;
	ApplyLook();
}

void AKGPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	// Desynchronise neighbouring coins without touching the gameplay RNG (purely cosmetic).
	const FVector L = GetActorLocation();
	Phase = FMath::Frac(L.X * 0.0137f + L.Y * 0.0071f) * UE_TWO_PI;
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}
}

void AKGPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Phase += DeltaSeconds;
	Spinner->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, 4.0f * FMath::Sin(Phase * 2.2f)),
	                                        FRotator(0.0f, FMath::Fmod(Phase * 90.0f, 360.0f), 0.0f));
}

void AKGPickup::OnRep_Item()
{
	ApplyLook();
}

void AKGPickup::ApplyLook()
{
	const FKGItemDef* Def = UKGItemCatalog::Find(ItemId);
	if (!Def)
	{
		Mesh->SetStaticMesh(nullptr);
		AppliedItem = NAME_None;
		return;
	}
	if (AppliedItem == ItemId)
	{
		return;
	}
	AppliedItem = ItemId;

	UStaticMesh* StaticMesh = Def->PickupMesh.IsValid() ? Cast<UStaticMesh>(Def->PickupMesh.TryLoad()) : nullptr;
	const bool bFallback = StaticMesh == nullptr;
	if (bFallback)
	{
		StaticMesh = Cast<UStaticMesh>(Def->PickupFallbackShape.TryLoad());
	}
	Mesh->SetStaticMesh(StaticMesh);
	if (!StaticMesh)
	{
		return;
	}
	// Fit the largest dimension to PickupSize and centre the visual on the spin axis.
	const FBox Box = StaticMesh->GetBoundingBox();
	const FVector Shaped = Box.GetSize() * Def->PickupShapeScale;
	const float Scale = Def->PickupSize / FMath::Max(Shaped.GetMax(), 1.0f);
	const FVector Scale3D = Def->PickupShapeScale * Scale;
	Mesh->SetRelativeScale3D(Scale3D);
	Mesh->SetRelativeRotation(Def->PickupShapeRotation);
	Mesh->SetRelativeLocation(-Def->PickupShapeRotation.RotateVector(Box.GetCenter() * Scale3D));

	if (bFallback && GetNetMode() != NM_DedicatedServer)
	{
		// Engine basic shapes: tint them with the item colour (BasicShapeMaterial exposes "Color").
		if (UMaterialInterface* Base = Mesh->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, Base))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Def->IconColor);
			}
		}
	}
}

void AKGPickup::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || bTaken || !By || By->IsDead())
	{
		return;
	}
	UKGInventoryComponent* Inventory = UKGInventoryComponent::FindForPawn(By);
	if (!Inventory)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("%s: %s has no inventory yet"), *GetName(), *By->GetName());
		return;
	}
	const int32 Added = Inventory->AddItem(ItemId, Count);
	if (Added <= 0)
	{
		UE_LOG(LogKillGodot, Log, TEXT("%s: %s's pockets are full"), *GetName(), *By->GetName());
		return;
	}
	MulticastPickedUp();
	Count -= Added;
	if (Count <= 0)
	{
		bTaken = true;
		Destroy();
	}
	else
	{
		ForceNetUpdate();
	}
}

void AKGPickup::MulticastPickedUp_Implementation()
{
	KGAudio::At(this, TEXT("S_ReelClick"), GetActorLocation(), 0.8f);
}

FText AKGPickup::GetInteractPrompt_Implementation() const
{
	const FKGItemDef* Def = UKGItemCatalog::Find(ItemId);
	if (!Def)
	{
		return FText::GetEmpty();
	}
	if (Count > 1)
	{
		return FText::Format(LOCTEXT("PickupMany", "Pick up {0} x{1}"), Def->DisplayName, FText::AsNumber(Count));
	}
	return FText::Format(LOCTEXT("PickupOne", "Pick up {0}"), Def->DisplayName);
}

#undef LOCTEXT_NAMESPACE
