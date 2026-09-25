#include "World/KGSeat.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGInventoryRPCComponent.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "KGSeat"

namespace KGSeatPrivate
{
	// Measured from A_KG_Sitting_Idle_Loop (Tools/Unreal/kg_import_outfits.py --bones): the pelvis sits 33 cm behind
	// the root and 54 cm above it; the pelvis bone is ~9 cm above the sitting surface.
	constexpr float PelvisBehindRoot = 33.0f;
	constexpr float PelvisAboveRoot = 54.0f;
	constexpr float PelvisAboveSurface = 9.0f;
	constexpr float CapsuleHalfHeight = 90.0f;   // AKGCharacter: InitCapsuleSize(34, 90), mesh at -90
	constexpr float MinSeatedSecondsBeforeStand = 0.35f;
	constexpr double ResitCooldownSeconds = 0.6;
}

AKGSeat::AKGSeat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetNetUpdateFrequency(2.0f);   // only the occupant changes; ForceNetUpdate on every change

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetRenderCustomDepth(true);

	// Non-static finders: Live Coding keeps stale statics.
	ConstructorHelpers::FObjectFinder<UStaticMesh> Stool(TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Stool.Stool"));
	ConstructorHelpers::FObjectFinder<UAnimSequence> Sitting(
		TEXT("/Game/KillGodot/Characters/Villager/Anims/A_KG_Sitting_Idle_Loop.A_KG_Sitting_Idle_Loop"));
	ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(
		TEXT("/Game/KillGodot/Characters/Villager/Anims/A_KG_Idle_Loop.A_KG_Idle_Loop"));
	SeatMesh = Stool.Object;
	SitAnim = Sitting.Object;
	IdleAnim = Idle.Object;
	SetSeatMesh(SeatMesh);
}

void AKGSeat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGSeat, Occupant);
}

void AKGSeat::SetSeatMesh(UStaticMesh* NewMesh)
{
	SeatMesh = NewMesh;
	Mesh->SetStaticMesh(NewMesh);
	Mesh->SetRelativeRotation(MeshRotation);
}

void AKGSeat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetSeatMesh(SeatMesh);
}

FTransform AKGSeat::GetSitTransform() const
{
	using namespace KGSeatPrivate;
	// Put the root where the clip wants it so the pelvis lands on the seat centre.
	const FVector Local(PelvisBehindRoot + SitForwardOffset, 0.0f,
	                    SeatHeight + PelvisAboveSurface - PelvisAboveRoot + CapsuleHalfHeight);
	const FRotator Facing(0.0f, GetActorRotation().Yaw, 0.0f);
	return FTransform(Facing, GetActorTransform().TransformPosition(Local));
}

FVector AKGSeat::GetStandLocation() const
{
	return GetActorTransform().TransformPosition(
		FVector(StandDistance, 0.0f, KGSeatPrivate::CapsuleHalfHeight + 2.0f));
}

AKGSeat* AKGSeat::FindSeatOf(const AKGCharacter* Who)
{
	if (!Who || !Who->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AKGSeat> It(Who->GetWorld()); It; ++It)
	{
		if (It->Occupant == Who)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AKGSeat::StandUpCharacter(AKGCharacter* Who)
{
	AKGSeat* Seat = FindSeatOf(Who);
	if (!Seat || !Seat->HasAuthority())
	{
		return false;
	}
	Seat->Stand();
	return true;
}

bool AKGSeat::Sit(AKGCharacter* Who)
{
	if (!HasAuthority() || !Who || Who->IsDead() || Occupant || FindSeatOf(Who))
	{
		return false;
	}
	// The E that stood us up can arrive after the relay's stand request: don't bounce straight back down.
	if (LastStood.Get() == Who && GetWorld()->GetTimeSeconds() - LastStandTime < KGSeatPrivate::ResitCooldownSeconds)
	{
		return false;
	}
	Occupant = Who;
	ApplySit(Who);
	ForceNetUpdate();
	return true;
}

void AKGSeat::Stand()
{
	if (!HasAuthority() || !Occupant)
	{
		return;
	}
	AKGCharacter* Old = Occupant;
	Occupant = nullptr;
	ApplyStand(Old);
	LastStood = Old;
	LastStandTime = GetWorld()->GetTimeSeconds();
	ForceNetUpdate();
}

void AKGSeat::OnRep_Occupant(AKGCharacter* OldOccupant)
{
	if (OldOccupant && OldOccupant != Occupant)
	{
		ApplyStand(OldOccupant);
	}
	if (Occupant)
	{
		ApplySit(Occupant);
	}
}

void AKGSeat::ApplySit(AKGCharacter* Who)
{
	if (!Who || Applied.Get() == Who)
	{
		return;
	}
	if (Applied.IsValid())
	{
		ApplyStand(Applied.Get());
	}
	UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();   // MOVE_None on server + owner: no prediction, no corrections

	const FTransform Sit = GetSitTransform();
	Who->SetActorLocationAndRotation(Sit.GetLocation(), Sit.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	Who->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

	// Free look while seated without swivelling the body on the stool.
	bSavedUseControllerYaw = Who->bUseControllerRotationYaw;
	Who->bUseControllerRotationYaw = false;
	if (UCameraComponent* Camera = Who->GetFirstPersonCamera())
	{
		SavedCameraLocation = Camera->GetRelativeLocation();
		Camera->SetRelativeLocation(SavedCameraLocation + FVector(-CameraBack, 0.0f, -CameraDrop));
	}
	if (SitAnim)
	{
		Who->PlayBodyClip(SitAnim, true, 0.25f);
	}
	if (Who->IsLocallyControlled())
	{
		if (AController* Controller = Who->GetController())
		{
			Controller->SetControlRotation(Sit.Rotator());
		}
	}
	Applied = Who;
	LocalSeatedTime = 0.0f;
	SetActorTickEnabled(true);
	UE_LOG(LogKillGodot, Log, TEXT("%s sat on %s"), *Who->GetName(), *GetName());
}

void AKGSeat::ApplyStand(AKGCharacter* Who)
{
	if (!IsValid(Who))
	{
		Applied.Reset();   // occupant destroyed (left the match): nothing to restore
		SetActorTickEnabled(false);
		return;
	}
	if (Applied.Get() != Who)
	{
		return;
	}
	Applied.Reset();
	SetActorTickEnabled(false);
	Who->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
	Who->bUseControllerRotationYaw = bSavedUseControllerYaw;
	if (Who->IsDead())
	{
		return;   // death owns the body and the camera now (AKGCharacter::HandleDeath death-cam, ghost)
	}
	if (UCameraComponent* Camera = Who->GetFirstPersonCamera())
	{
		Camera->SetRelativeLocation(SavedCameraLocation);
	}
	Who->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Who->TeleportTo(GetStandLocation(), Who->GetActorRotation(), false, false);
	if (IdleAnim)
	{
		// AKGCharacter resumes its own clip selection as soon as the movement state changes.
		Who->PlayBodyClip(IdleAnim, true, 0.25f);
	}
	UE_LOG(LogKillGodot, Log, TEXT("%s stood up from %s"), *Who->GetName(), *GetName());
}

void AKGSeat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AKGCharacter* Who = Occupant;
	if (Who && (!IsValid(Who) || Who->IsDead()))
	{
		if (HasAuthority())
		{
			Stand();
		}
		return;   // clients: wait for the occupant to replicate away, never fight the death animation
	}
	if (!Who)
	{
		SetActorTickEnabled(false);
		return;
	}
	if (Applied.Get() != Who)
	{
		return;
	}
	LocalSeatedTime += DeltaSeconds;

	// AKGCharacter picks its body clip from movement; keep the sitting loop on top of it.
	if (SitAnim && Who->GetBodyClip() != SitAnim)
	{
		Who->PlayBodyClip(SitAnim, true, 0.25f);
	}
	if (Who->GetCharacterMovement()->MovementMode != MOVE_None)
	{
		Who->GetCharacterMovement()->DisableMovement();
	}

	// Owning player: E or Space stands up, wherever the crosshair is.
	if (Who->IsLocallyControlled() && LocalSeatedTime > KGSeatPrivate::MinSeatedSecondsBeforeStand)
	{
		APlayerController* PC = Cast<APlayerController>(Who->GetController());
		if (PC && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::E)))
		{
			if (UKGInventoryRPCComponent* Relay = UKGInventoryRPCComponent::FindForController(PC))
			{
				Relay->RequestStandUp();
			}
			else if (HasAuthority())
			{
				Stand();
			}
		}
	}
}

void AKGSeat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AKGCharacter* Who = Applied.Get())
	{
		ApplyStand(Who);
	}
	Super::EndPlay(EndPlayReason);
}

void AKGSeat::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By)
	{
		return;
	}
	if (Occupant == By)
	{
		Stand();
	}
	else if (!Occupant)
	{
		Sit(By);
	}
}

FText AKGSeat::GetInteractPrompt_Implementation() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Local = PC ? PC->GetPawn() : nullptr;
	if (Occupant && Occupant == Local)
	{
		return LOCTEXT("SeatStand", "Stand up");
	}
	return Occupant ? FText::GetEmpty() : LOCTEXT("SeatSit", "Sit");
}

#undef LOCTEXT_NAMESPACE
