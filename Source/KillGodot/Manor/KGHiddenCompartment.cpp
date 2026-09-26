#include "Manor/KGHiddenCompartment.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/KGLoot.h"
#include "KillGodot.h"
#include "Manor/KGManorSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"

#define LOCTEXT_NAMESPACE "KGHiddenCompartment"

AKGHiddenCompartment::AKGHiddenCompartment()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(GetRootComponent());
	Lid->SetMobility(EComponentMobility::Movable);
	Lid->SetCanEverAffectNavigation(false);
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(GetRootComponent());
	// The E trace (Visibility) finds it; nothing walks into it.
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->SetHiddenInGame(true);
	Hitbox->SetCanEverAffectNavigation(false);
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGHiddenCompartment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHitbox();
}

void AKGHiddenCompartment::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyHitbox();
}

void AKGHiddenCompartment::ApplyHitbox()
{
	Hitbox->SetBoxExtent(HitboxExtent);
}

void AKGHiddenCompartment::BeginPlay()
{
	Super::BeginPlay();
	LidRestLoc = Lid->GetRelativeLocation();
	LidRestRot = Lid->GetRelativeRotation();
	bRestCaptured = true;
	LidAlpha = bOpen ? 1.0f : 0.0f;
}

void AKGHiddenCompartment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AKGHiddenCompartment, CompartmentId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGHiddenCompartment, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGHiddenCompartment, Room, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGHiddenCompartment, ClueText, COND_InitialOnly);
	DOREPLIFETIME(AKGHiddenCompartment, bOpen);
}

AKGHiddenCompartment* AKGHiddenCompartment::FindById(const UWorld* World, FName Id)
{
	for (TActorIterator<AKGHiddenCompartment> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->CompartmentId == Id)
		{
			return *It;
		}
	}
	return nullptr;
}

void AKGHiddenCompartment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRestCaptured || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Target = bOpen ? 1.0f : 0.0f;
	if (FMath::IsNearlyEqual(LidAlpha, Target, 0.001f))
	{
		return;
	}
	LidAlpha = FMath::FInterpConstantTo(LidAlpha, Target, DeltaSeconds, 1.6f);
	const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, LidAlpha, 2.0f);
	Lid->SetRelativeLocation(LidRestLoc + LidOpenOffset * Ease);
	Lid->SetRelativeRotation(LidRestRot + LidOpenRotation * Ease);
}

void AKGHiddenCompartment::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || bOpen || !By || By->IsDead())
	{
		return;
	}
	AuthOpen(By);
}

FText AKGHiddenCompartment::GetInteractPrompt_Implementation() const
{
	if (!bOpen)
	{
		return SearchPrompt.IsEmpty() ? LOCTEXT("Search", "Search") : SearchPrompt;
	}
	if (!ClueText.IsEmpty())
	{
		return FText::Format(LOCTEXT("Clue", "Clue: {0}"), FText::FromString(ClueText));
	}
	return FText::GetEmpty();
}

bool AKGHiddenCompartment::AuthOpen(AKGCharacter* By)
{
	if (!HasAuthority() || bOpen)
	{
		return false;
	}
	FlushNetDormancy();
	bOpen = true;
	ForceNetUpdate();
	if (!bLooted && !LootTable.IsNone())
	{
		bLooted = true;
		const AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>();
		const uint64 Seed = GM ? static_cast<uint64>(GM->GetMatchSeed()) : 0x5EC4E7ull;
		FKGRng Rng = FKGLoot::MakeRng(Seed, this);
		const FVector Where = GetActorTransform().TransformPosition(LootOffset);
		FKGLoot::RollAndSpawn(this, LootTable, Rng, Where);
	}
	MulticastOpened();
	if (UKGManorSubsystem* M = UKGManorSubsystem::Get(GetWorld()))
	{
		M->OnCompartmentOpened(CompartmentId, By);
	}
	return true;
}

void AKGHiddenCompartment::OnRep_Open()
{
	// Tick eases the lid toward the replicated state.
}

void AKGHiddenCompartment::MulticastOpened_Implementation()
{
	KGAudio::At(this, TEXT("S_Chore_Crank"), GetActorLocation(), 0.8f);
}

#undef LOCTEXT_NAMESPACE
