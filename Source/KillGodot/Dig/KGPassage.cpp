#include "Dig/KGPassage.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"

#define LOCTEXT_NAMESPACE "KGPassage"

namespace KGPassagePrivate
{
	constexpr double Cooldown = 1.5;

	TMap<TWeakObjectPtr<const AActor>, double>& LastTravel()
	{
		static TMap<TWeakObjectPtr<const AActor>, double> Map;
		return Map;
	}
}

AKGPassage::AKGPassage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(GetRootComponent());
	// The E trace (Visibility) finds it; nothing walks into it or is blocked by it.
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetCanEverAffectNavigation(false);
	Hitbox->SetBoxExtent(HitboxExtent);
	Hitbox->SetRelativeLocation(HitboxOffset);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGPassage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHitbox();
}

void AKGPassage::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyHitbox();
}

void AKGPassage::ApplyHitbox()
{
	Hitbox->SetBoxExtent(HitboxExtent);
	Hitbox->SetRelativeLocation(HitboxOffset);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	if (bAutoExit)
	{
		// An auto exit is a trigger for bodies; the E trace must not stop on it.
		Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Hitbox->SetGenerateOverlapEvents(true);
	}
	else
	{
		Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Hitbox->SetGenerateOverlapEvents(false);
	}
}

AKGPassage* AKGPassage::FindById(const UWorld* World, FName Id)
{
	if (!World || Id.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AKGPassage> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->PassageId == Id)
		{
			return *It;
		}
	}
	return nullptr;
}

FTransform AKGPassage::GetArrival() const
{
	const FRotator Rot(0.0f, GetActorRotation().Yaw + ArrivalYaw, 0.0f);
	return FTransform(Rot, GetActorTransform().TransformPosition(ArrivalLocal));
}

void AKGPassage::Interact_Implementation(AKGCharacter* By)
{
	if (HasAuthority() && !bAutoExit)
	{
		TravelThrough(By);
	}
}

FText AKGPassage::GetInteractPrompt_Implementation() const
{
	return Prompt.IsEmpty() ? LOCTEXT("Go", "Go through") : Prompt;
}

bool AKGPassage::TravelThrough(AKGCharacter* Who)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !Who || Who->IsDead())
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (const double* Last = KGPassagePrivate::LastTravel().Find(Who); Last && Now - *Last < KGPassagePrivate::Cooldown && *Last <= Now)
	{
		return false;
	}
	const AKGPassage* Target = FindById(World, TargetId);
	if (!Target)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("KG_PASSAGE %s: no passage '%s'"), *PassageId.ToString(), *TargetId.ToString());
		return false;
	}
	const FTransform Arrive = Target->GetArrival();
	const FVector From = Who->GetActorLocation();
	UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	Move->StopMovementImmediately();
	if (!Who->TeleportTo(Arrive.GetLocation(), Arrive.Rotator(), false, true))
	{
		Who->SetActorLocation(Arrive.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	Move->SetMovementMode(Target->bArriveFalling ? MOVE_Falling : MOVE_Walking);
	if (AController* Controller = Who->GetController())
	{
		const FRotator View(Target->bArriveFalling ? 0.0f : -5.0f, Arrive.Rotator().Yaw, 0.0f);
		Controller->SetControlRotation(View);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ClientSetRotation(View, true);
			PC->ClientSetCameraFade(true, FColor::Black, FVector2D(1.0f, 0.0f), 0.8f, false, false);
		}
	}
	KGPassagePrivate::LastTravel().Add(Who, Now);
	MulticastTravelled(From, Arrive.GetLocation());
	UE_LOG(LogKillGodot, Log, TEXT("KG_PASSAGE %s -> %s: %s at %s"), *PassageId.ToString(), *TargetId.ToString(), *Who->GetName(),
	       *Arrive.GetLocation().ToCompactString());
	return true;
}

void AKGPassage::MulticastTravelled_Implementation(FVector_NetQuantize From, FVector_NetQuantize To)
{
	KGAudio::At(this, *SoundName.ToString(), From, 0.9f);
	KGAudio::At(this, *SoundName.ToString(), To, 0.9f);
}

void AKGPassage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAutoExit || !HasAuthority())
	{
		return;
	}
	// Whoever climbs up into the top of the shaft goes up to the surface (never someone who just arrived and is
	// climbing down: they move down).
	TArray<AActor*> Inside;
	Hitbox->GetOverlappingActors(Inside, AKGCharacter::StaticClass());
	for (AActor* A : Inside)
	{
		AKGCharacter* C = Cast<AKGCharacter>(A);
		if (C && C->GetVelocity().Z > 30.0f)
		{
			TravelThrough(C);
		}
	}
}

#undef LOCTEXT_NAMESPACE
