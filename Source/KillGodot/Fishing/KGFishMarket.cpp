#include "Fishing/KGFishMarket.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Fishing/KGFishingComponent.h"
#include "Fishing/KGFishingTypes.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "KillGodot.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "KGFishMarket"

AKGFishMarketStall::AKGFishMarketStall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Carp(
		TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Fish_GoldenCarp.SM_KG_Fish_GoldenCarp"));

	// The builder's Table_Large is 285 x 110 x 81 cm: a box a little larger than it.
	Hitbox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	Hitbox->SetRelativeScale3D(FVector(3.1f, 1.4f, 1.5f));
	Hitbox->SetHiddenInGame(true);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetCanEverAffectNavigation(false);
	if (Cube.Succeeded())
	{
		Hitbox->SetStaticMesh(Cube.Object);
	}

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
	Sign->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));   // readable from the customers' side (local -Y)
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(26.0f);
	Sign->SetTextRenderColor(FColor(255, 214, 120));
	Sign->SetText(LOCTEXT("Sign", "MADAM BRINE BUYS FISH\n[E] sell your catch"));
	Sign->SetCastShadow(false);

	Icon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Icon"));
	Icon->SetupAttachment(Root);
	Icon->SetRelativeLocation(FVector(0.0f, 0.0f, 320.0f));
	Icon->SetRelativeScale3D(FVector(1.6f));
	Icon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Icon->SetCanEverAffectNavigation(false);
	if (Carp.Succeeded())
	{
		Icon->SetStaticMesh(Carp.Object);
	}
}

void AKGFishMarketStall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	IconSpin += DeltaSeconds;
	Icon->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, 320.0f + 10.0f * FMath::Sin(IconSpin * 1.6f)),
	                                     FRotator(12.0f * FMath::Sin(IconSpin * 2.3f), IconSpin * 40.0f, 0.0f));
}

int32 AKGFishMarketStall::SellAll(APlayerState* Seller, int32& OutItems)
{
	OutItems = 0;
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(Seller);
	if (!Pockets || !Pockets->CanMutate())
	{
		return 0;
	}
	// Price every stack with its own recorded weight, then take them (slots change while removing).
	struct FSale
	{
		int32 Slot;
		FName ItemId;
		int32 Count;
		int32 Price;
	};
	TArray<FSale> Sales;
	for (const FKGItemEntry& Entry : Pockets->GetEntries())
	{
		if (FKGFishingRules::IsSellable(Entry.ItemId))
		{
			Sales.Add({Entry.Slot, Entry.ItemId, Entry.Count, FKGFishingRules::SellPrice(Entry.ItemId, Entry.Count, Entry.Grams)});
		}
	}
	int32 Coins = 0;
	for (const FSale& Sale : Sales)
	{
		FName Removed;
		const int32 Taken = Pockets->RemoveFromSlot(Sale.Slot, Sale.Count, Removed);
		if (Taken > 0 && Removed == Sale.ItemId)
		{
			OutItems += Taken;
			Coins += Sale.Price * Taken / FMath::Max(1, Sale.Count);
		}
	}
	if (Coins > 0)
	{
		const int32 Paid = Pockets->AddItem(FKGItemIds::Coin, Coins);
		if (Paid < Coins)
		{
			UE_LOG(LogKillGodot, Warning, TEXT("KG_FISH_SELL purse full: %d of %d coins"), Paid, Coins);
		}
	}
	return Coins;
}

void AKGFishMarketStall::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By || By->IsDead())
	{
		return;
	}
	APlayerState* PS = By->GetPlayerState();
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
	UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(By);
	if (!Pockets)
	{
		return;
	}
	bool bLent = false;
	if (!Pockets->Has(FKGItemIds::FishingRod) && Pockets->AddItem(FKGItemIds::FishingRod, 1) > 0)
	{
		bLent = true;
		if (Fishing)
		{
			Fishing->ServerNotify(EKGFishNotice::LentRod);
		}
	}
	int32 Items = 0;
	const int32 Coins = SellAll(PS, Items);
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SELL %s items=%d coins=%d lent_rod=%d"), *GetNameSafe(PS), Items, Coins, bLent ? 1 : 0);
	if (Fishing && Items > 0)
	{
		Fishing->ServerNotify(EKGFishNotice::Sold, Items, Coins);
	}
	else if (Fishing && !bLent)
	{
		Fishing->ServerNotify(EKGFishNotice::NothingToSell);
	}
}

FText AKGFishMarketStall::GetInteractPrompt_Implementation() const
{
	return LOCTEXT("Prompt", "Sell your catch to Madam Brine");
}

AKGFishMarketStall* AKGFishMarketStall::SpawnStall(UWorld* World, const FVector& Location, float Yaw)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AKGFishMarketStall>(Location, FRotator(0.0f, Yaw, 0.0f), Params);
}

#undef LOCTEXT_NAMESPACE
