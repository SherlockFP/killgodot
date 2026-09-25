#include "Dig/KGKeyGate.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Dig/KGDigComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGInventoryComponent.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "World/KGDoor.h"

#define LOCTEXT_NAMESPACE "KGKeyGate"

AKGKeyGate::AKGKeyGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(GetRootComponent());
	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(Hinge);
	Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Leaf->SetMobility(EComponentMobility::Movable);
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGKeyGate::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!Leaf->GetStaticMesh())
	{
		// The level builder sets the leaf; spawned gates (tests, dev) load the pack mesh quietly.
		Leaf->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
			TEXT("/Game/KillGodot/Env/Dress/KG_DressUnder_Clean/StaticMeshes/SM_KG_CryptGate.SM_KG_CryptGate"), nullptr,
			LOAD_Quiet | LOAD_NoWarn));
	}
}

void AKGKeyGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGKeyGate, bUnlocked);
	DOREPLIFETIME(AKGKeyGate, bOpen);
}

void AKGKeyGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? OpenAngle : 0.0f;
	if (!FMath::IsNearlyEqual(Angle, Target, 0.01f))
	{
		Angle = AKGDoor::StepAngle(Angle, Target, 110.0f, DeltaSeconds);
		Hinge->SetRelativeRotation(FRotator(0.0f, Angle, 0.0f));
	}
}

void AKGKeyGate::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By || By->IsDead())
	{
		return;
	}
	if (!bUnlocked)
	{
		UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(By);
		if (!Pockets || !Pockets->Has(KeyItem))
		{
			MulticastSound(true);
			if (UKGDigComponent* Dig = UKGDigComponent::FindFor(By))
			{
				Dig->ServerNotify(EKGDigNotice::GateLocked);
			}
			return;
		}
		Pockets->RemoveItem(KeyItem, 1);   // the key stays in the lock
		bUnlocked = true;
		if (UKGDigComponent* Dig = UKGDigComponent::FindFor(By))
		{
			Dig->ServerNotify(EKGDigNotice::GateOpened);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_GATE %s unlocked by %s"), *GetName(), *By->GetName());
	}
	bOpen = !bOpen;
	MulticastSound(false);
	ForceNetUpdate();
}

void AKGKeyGate::DevUnlock(bool bAlsoOpen)
{
	if (HasAuthority())
	{
		bUnlocked = true;
		bOpen = bAlsoOpen || bOpen;
		ForceNetUpdate();
	}
}

void AKGKeyGate::MulticastSound_Implementation(bool bLockedRattle)
{
	KGAudio::At(this, bLockedRattle ? TEXT("S_Gate_Locked") : TEXT("S_Gate_Open"), GetActorLocation() + FVector(90.0, 0.0, 120.0), 1.0f);
}

FText AKGKeyGate::GetInteractPrompt_Implementation() const
{
	if (bUnlocked)
	{
		return bOpen ? LOCTEXT("Close", "Close the gate") : LOCTEXT("Open", "Open the gate");
	}
	// Client side: the local player's own pockets decide the hint.
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const UKGInventoryComponent* Pockets = PC ? UKGInventoryComponent::FindForPawn(PC->GetPawn()) : nullptr;
	return Pockets && Pockets->Has(KeyItem) ? LOCTEXT("Unlock", "Unlock with the Crypt Key")
	                                        : LOCTEXT("Locked", "Locked - it needs a Crypt Key");
}

#undef LOCTEXT_NAMESPACE
