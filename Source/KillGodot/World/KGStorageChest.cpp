#include "World/KGStorageChest.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "KillGodot.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"

#define LOCTEXT_NAMESPACE "KGChest"

namespace KGChestPrivate
{
	/** First chest mesh that exists: pirate kit, Quaternius prop kit, our own import (kg_import_storage.py). */
	UStaticMesh* FindDefaultChestMesh()
	{
		static const TCHAR* Candidates[] = {
			TEXT("/Game/KillGodot/Env/KG_Pirate/StaticMeshes/Chest"),
			TEXT("/Game/KillGodot/Env/KG_Pirate/StaticMeshes/Chest_Pirate"),
			TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Chest_Wood"),
			TEXT("/Game/KillGodot/Items/Storage/SM_KG_Chest_Wood"),
		};
		for (const TCHAR* Package : Candidates)
		{
			if (FPackageName::DoesPackageExist(Package))
			{
				const FString Name = FPackageName::GetShortName(Package);
				if (UStaticMesh* Found = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *Name),
				                                                 nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}
}

AKGStorageChest::AKGStorageChest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetDormancy(DORM_Initial);
	DisplayName = LOCTEXT("ChestName", "Chest");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);   // blocks pawns and the E trace
	Mesh->SetRenderCustomDepth(true);

	Inventory = CreateDefaultSubobject<UKGInventoryComponent>(TEXT("Inventory"));
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));

	ChestMesh = KGChestPrivate::FindDefaultChestMesh();
	if (!ChestMesh)
	{
		// Non-static finder on purpose (Live Coding keeps stale statics).
		ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		ChestMesh = Cube.Object;
	}
	SetChestMesh(ChestMesh);

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGStorageChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGStorageChest, OwnerPuid);
	DOREPLIFETIME(AKGStorageChest, bLocked);
	DOREPLIFETIME(AKGStorageChest, NumViewers);
}

void AKGStorageChest::SetChestMesh(UStaticMesh* NewMesh)
{
	ChestMesh = NewMesh;
	Mesh->SetStaticMesh(NewMesh);
	bUsingFallbackMesh = NewMesh && NewMesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));
	if (bUsingFallbackMesh)
	{
		// 90 x 55 x 55 cm box sitting on the floor.
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 27.5f));
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.55f, 0.55f));
	}
	else
	{
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		Mesh->SetRelativeScale3D(FVector(MeshScale));
	}
}

void AKGStorageChest::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetChestMesh(ChestMesh);
}

void AKGStorageChest::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Contents never replicate directly; viewers get a private mirror (UKGInventoryRPCComponent).
	Inventory->SetIsReplicated(false);
}

void AKGStorageChest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	Inventory->SetCapacity(Slots);
	if (!bStartingItemsGiven)
	{
		bStartingItemsGiven = true;
		for (const FKGItemStack& Stack : StartingItems)
		{
			Inventory->AddItem(Stack.ItemId, Stack.Count);
		}
		if (const FKGLootTable* Table = FKGLoot::FindTable(StartingLootTable))
		{
			FKGRng Rng = FKGLoot::MakeRng(static_cast<uint64>(LootSeed), this);
			TArray<FKGItemStack> Rolled;
			FKGLoot::Roll(Rng, *Table, Rolled);
			for (const FKGItemStack& Stack : Rolled)
			{
				Inventory->AddItem(Stack.ItemId, Stack.Count);
			}
		}
	}
}

void AKGStorageChest::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// Host-migration snapshots serialize only the actor's SaveGame fields; carry the contents along.
	if (Ar.IsSaveGame() && Inventory)
	{
		Inventory->Serialize(Ar);
		if (Ar.IsLoading())
		{
			Inventory->NotifyRestored();
		}
	}
}

bool AKGStorageChest::IsOwnedBy(const APlayerState* Player) const
{
	const AKGPlayerState* PS = Cast<AKGPlayerState>(Player);
	if (!PS)
	{
		return false;
	}
	if (!OwnerPuid.IsEmpty() && PS->Puid == OwnerPuid)
	{
		return true;
	}
	return HouseIndex != INDEX_NONE && PS->HouseIndex == HouseIndex;
}

bool AKGStorageChest::CanPlayerUseContainer(const APlayerState* Player) const
{
	return Player && (!bLocked || IsOwnedBy(Player));
}

bool AKGStorageChest::CanPlayerToggleLock(const APlayerState* Player) const
{
	return IsOwnedBy(Player);
}

void AKGStorageChest::SetContainerLocked(bool bInLocked)
{
	if (HasAuthority() && bLocked != bInLocked)
	{
		FlushNetDormancy();
		bLocked = bInLocked;
	}
}

void AKGStorageChest::SetHouseOwner(const FString& InOwnerPuid)
{
	if (HasAuthority())
	{
		FlushNetDormancy();
		OwnerPuid = InOwnerPuid;
	}
}

void AKGStorageChest::OnViewerCountChanged(int32 InNumViewers)
{
	const uint8 Old = NumViewers;
	const uint8 New = static_cast<uint8>(FMath::Clamp(InNumViewers, 0, 255));
	if (Old != New)
	{
		FlushNetDormancy();
		NumViewers = New;
		OnRep_NumViewers(Old);   // listen-server host hears it too
	}
}

void AKGStorageChest::OnRep_NumViewers(uint8 OldNumViewers)
{
	const FVector Where = Mesh->Bounds.Origin;
	if (OldNumViewers == 0 && NumViewers > 0)
	{
		KGAudio::At(this, TEXT("S_DoorOpen"), Where, 0.6f);
	}
	else if (OldNumViewers > 0 && NumViewers == 0)
	{
		KGAudio::At(this, TEXT("S_DoorClose"), Where, 0.6f);
	}
}

void AKGStorageChest::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By || By->IsDead())
	{
		return;
	}
	AKGPlayerState* PS = By->GetPlayerState<AKGPlayerState>();
	UKGInventoryRPCComponent* Relay = UKGInventoryRPCComponent::FindForPlayer(PS);
	if (!Relay)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("%s: %s has no inventory relay yet"), *GetName(), *GetNameSafe(PS));
		return;
	}
	if (!CanPlayerUseContainer(PS))
	{
		UE_LOG(LogKillGodot, Log, TEXT("%s is locked for %s"), *GetName(), *GetNameSafe(PS));
		return;
	}
	if (Relay->GetViewedActor() == this)
	{
		Relay->CloseContainer();   // E toggles
		return;
	}
	Relay->OpenContainer(this);
}

FText AKGStorageChest::GetInteractPrompt_Implementation() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APlayerState* Local = PC ? PC->PlayerState.Get() : nullptr;
	if (bLocked && !IsOwnedBy(Local))
	{
		return FText::Format(LOCTEXT("ChestLocked", "{0} (locked)"), DisplayName);
	}
	if (IsOwnedBy(Local))
	{
		return FText::Format(LOCTEXT("ChestOpenOwn", "Open your {0}"), DisplayName);
	}
	return FText::Format(LOCTEXT("ChestOpen", "Open {0}"), DisplayName);
}

#undef LOCTEXT_NAMESPACE
