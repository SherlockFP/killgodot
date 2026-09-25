#include "World/KGTaskStation.h"
#include "Character/KGCharacter.h"
#include "Chores/KGChoreComponent.h"
#include "Chores/KGChoreTypes.h"
#include "Chores/UI/KGMinigame.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AKGTaskStation::AKGTaskStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = false;   // stations are static level furniture; progress lives on the players

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(
		TEXT("/Game/KillGodot/Materials/M_KG_TaskMarker.M_KG_TaskMarker"));

	Hitbox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	Hitbox->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.8f));
	Hitbox->SetHiddenInGame(true);
	Hitbox->SetCollisionProfileName(TEXT("NoCollision"));
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetCanEverAffectNavigation(false);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	Marker->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));   // cone pointing down at the chore
	Marker->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.45f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	Marker->SetVisibility(false);
	Marker->SetCanEverAffectNavigation(false);

	if (Cylinder.Succeeded())
	{
		Hitbox->SetStaticMesh(Cylinder.Object);
	}
	if (Cone.Succeeded())
	{
		Marker->SetStaticMesh(Cone.Object);
	}
	if (Glow.Succeeded())
	{
		Marker->SetMaterial(0, Glow.Object);
	}
}

bool AKGTaskStation::IsWantedByLocalPlayer() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AKGPlayerState* PS = PC ? PC->GetPlayerState<AKGPlayerState>() : nullptr;
	return PS && PS->HasOpenTask(TaskId);
}

void AKGTaskStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const bool bWanted = IsWantedByLocalPlayer();
	Marker->SetVisibility(bWanted);
	if (bWanted)
	{
		// Gentle bob + spin so it reads from across the square.
		MarkerTime += DeltaSeconds;
		Marker->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f + 18.0f * FMath::Sin(MarkerTime * 2.4f)));
		Marker->SetRelativeRotation(FRotator(180.0f, MarkerTime * 60.0f, 0.0f));
	}
}

void AKGTaskStation::Interact_Implementation(AKGCharacter* By)
{
	if (!By || !HasAuthority())
	{
		return;
	}
	// Players get the chore's minigame (Chores/); bots and chores without one keep the hold-E fallback.
	UKGChoreComponent* Chores = UKGChoreComponent::FindFor(By);
	if (Chores && By->IsPlayerControlled() && FKGMinigameFactory::Has(TaskId) && Chores->AuthOpen(TaskId, this))
	{
		return;
	}
	By->BeginTask(this);
}

FText AKGTaskStation::GetInteractPrompt_Implementation() const
{
	if (!IsWantedByLocalPlayer())
	{
		return FText::GetEmpty();
	}
	if (FKGMinigameFactory::Has(TaskId))
	{
		const FKGChoreDef* Def = FKGChoreCatalog::Find(TaskId);
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const UKGChoreComponent* Chores = PC ? UKGChoreComponent::FindFor(PC->GetPawn()) : nullptr;
		const int32 Saved = Chores ? Chores->GetSavedStage(TaskId) : 0;
		if (Def && Saved > 0 && Def->NumStages() > 1)
		{
			return FText::FromString(FString::Printf(TEXT("%s  (continue %d/%d)"), *TaskName, Saved + 1, Def->NumStages()));
		}
		return FText::FromString(TaskName);
	}
	return FText::FromString(FString::Printf(TEXT("%s  (%.0fs)"), *TaskName, WorkSeconds));
}
