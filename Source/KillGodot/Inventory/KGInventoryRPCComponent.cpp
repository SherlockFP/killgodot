#include "Inventory/KGInventoryRPCComponent.h"
#include "Character/KGCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryUI.h"
#include "KillGodot.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "World/KGPickup.h"
#include "World/KGSeat.h"

namespace KGInvRelayPrivate
{
	int32 CountViewers(const AActor* Container)
	{
		const UWorld* World = Container ? Container->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS)
		{
			return 0;
		}
		int32 Num = 0;
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const UKGInventoryRPCComponent* Relay = UKGInventoryRPCComponent::FindForPlayer(PS);
			Num += Relay && Relay->GetViewedActor() == Container ? 1 : 0;
		}
		return Num;
	}
}

UKGInventoryRPCComponent::UKGInventoryRPCComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.2f;
	SetIsReplicatedByDefault(true);
}

void UKGInventoryRPCComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;   // only the viewer learns what is inside the container
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryRPCComponent, ViewedActor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryRPCComponent, ViewedItems, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryRPCComponent, ViewedCapacity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryRPCComponent, bViewedLocked, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryRPCComponent, bViewerCanToggleLock, Params);
}

UKGInventoryRPCComponent* UKGInventoryRPCComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGInventoryRPCComponent>() : nullptr;
}

UKGInventoryRPCComponent* UKGInventoryRPCComponent::FindForController(const APlayerController* Controller)
{
	return Controller ? FindForPlayer(Controller->PlayerState) : nullptr;
}

AKGCharacter* UKGInventoryRPCComponent::GetCharacter() const
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	return PS ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
}

UKGInventoryComponent* UKGInventoryRPCComponent::GetServerViewedInventory() const
{
	return ServerViewed.Get();
}

FText UKGInventoryRPCComponent::GetViewedTitle() const
{
	if (const IKGContainerOwner* Container = Cast<IKGContainerOwner>(ViewedActor.Get()))
	{
		return Container->GetContainerTitle();
	}
	return FText::GetEmpty();
}

bool UKGInventoryRPCComponent::OpenContainer(AActor* ContainerActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ContainerActor))
	{
		return false;
	}
	IKGContainerOwner* Container = Cast<IKGContainerOwner>(ContainerActor);
	UKGInventoryComponent* Inventory = Container ? Container->GetContainerInventory() : nullptr;
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (!Inventory || !Container->CanPlayerUseContainer(PS))
	{
		return false;
	}
	if (ViewedActor == ContainerActor)
	{
		RefreshMirror();
		BroadcastView();
		return true;
	}
	if (ViewedActor)
	{
		CloseContainer();
	}
	ViewedActor = ContainerActor;
	ServerViewed = Inventory;
	ServerViewedHandle = Inventory->OnChanged.AddUObject(this, &UKGInventoryRPCComponent::HandleContainerChanged);
	ViewedCapacity = Inventory->GetCapacity();
	bViewedLocked = Container->IsContainerLocked();
	bViewerCanToggleLock = Container->CanPlayerToggleLock(PS);
	ViewedItems.SyncFrom(Inventory->GetItemList());
	MarkViewDirty();
	SetComponentTickEnabled(true);
	Container->OnViewerCountChanged(KGInvRelayPrivate::CountViewers(ContainerActor));
	UE_LOG(LogKillGodot, Log, TEXT("%s opened %s"), *GetNameSafe(PS), *ContainerActor->GetName());
	BroadcastView();
	return true;
}

void UKGInventoryRPCComponent::CloseContainer()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ViewedActor)
	{
		return;
	}
	if (UKGInventoryComponent* Inventory = ServerViewed.Get())
	{
		Inventory->OnChanged.Remove(ServerViewedHandle);
	}
	ServerViewedHandle.Reset();
	ServerViewed.Reset();
	AActor* Old = ViewedActor;
	ViewedActor = nullptr;
	ViewedItems.Entries.Reset();
	ViewedItems.MarkArrayDirty();
	ViewedCapacity = 0;
	bViewedLocked = false;
	bViewerCanToggleLock = false;
	MarkViewDirty();
	SetComponentTickEnabled(false);
	if (IKGContainerOwner* Container = Cast<IKGContainerOwner>(Old))
	{
		Container->OnViewerCountChanged(KGInvRelayPrivate::CountViewers(Old));
	}
	BroadcastView();
}

bool UKGInventoryRPCComponent::IsViewStillValid() const
{
	const IKGContainerOwner* Container = Cast<IKGContainerOwner>(ViewedActor.Get());
	const AKGCharacter* Character = GetCharacter();
	if (!Container || !IsValid(ViewedActor) || !ServerViewed.IsValid() || !Character || Character->IsDead())
	{
		return false;
	}
	if (FVector::DistSquared(Character->GetActorLocation(), ViewedActor->GetActorLocation()) >
	    FMath::Square(MaxUseDistance))
	{
		return false;
	}
	return Container->CanPlayerUseContainer(Cast<APlayerState>(GetOwner()));
}

void UKGInventoryRPCComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority() || !ViewedActor)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (!IsViewStillValid())
	{
		CloseContainer();
		return;
	}
	const IKGContainerOwner* Container = Cast<IKGContainerOwner>(ViewedActor.Get());
	if (Container && Container->IsContainerLocked() != bViewedLocked)
	{
		bViewedLocked = Container->IsContainerLocked();
		MarkViewDirty();
		BroadcastView();
	}
}

void UKGInventoryRPCComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CloseContainer();
	}
	Super::EndPlay(EndPlayReason);
}

void UKGInventoryRPCComponent::RefreshMirror()
{
	if (const UKGInventoryComponent* Inventory = ServerViewed.Get())
	{
		const bool bChanged = ViewedItems.SyncFrom(Inventory->GetItemList());
		if (ViewedCapacity != Inventory->GetCapacity())
		{
			ViewedCapacity = Inventory->GetCapacity();
			MarkViewDirty();
		}
		else if (bChanged)
		{
			MarkViewDirty();
		}
	}
}

void UKGInventoryRPCComponent::HandleContainerChanged(UKGInventoryComponent* Inventory)
{
	RefreshMirror();
	BroadcastView();
}

void UKGInventoryRPCComponent::MarkViewDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryRPCComponent, ViewedActor, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryRPCComponent, ViewedItems, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryRPCComponent, ViewedCapacity, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryRPCComponent, bViewedLocked, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryRPCComponent, bViewerCanToggleLock, this);
	GetOwner()->ForceNetUpdate();
}

void UKGInventoryRPCComponent::BroadcastView()
{
	OnViewChanged.Broadcast(this);
	if (GetNetMode() != NM_DedicatedServer)
	{
		FKGInventoryUI::HandleViewChanged(this);   // no-op unless this relay belongs to a local player
	}
}

void UKGInventoryRPCComponent::OnRep_View()
{
	BroadcastView();
}

// ---- Requests ----

void UKGInventoryRPCComponent::RequestTransfer(bool bFromContainer, int32 Slot, int32 Num)
{
	if (GetOwner()->HasAuthority())
	{
		DoTransfer(bFromContainer, Slot, Num);
	}
	else
	{
		ServerTransfer(bFromContainer, Slot, Num);
	}
}

void UKGInventoryRPCComponent::RequestClose()
{
	if (GetOwner()->HasAuthority())
	{
		CloseContainer();
	}
	else
	{
		ServerCloseContainer();
	}
}

void UKGInventoryRPCComponent::RequestSetLocked(bool bLocked)
{
	if (GetOwner()->HasAuthority())
	{
		ServerSetLocked_Implementation(bLocked);
	}
	else
	{
		ServerSetLocked(bLocked);
	}
}

void UKGInventoryRPCComponent::RequestStandUp()
{
	if (GetOwner()->HasAuthority())
	{
		DoStandUp();
	}
	else
	{
		ServerStandUp();
	}
}

void UKGInventoryRPCComponent::RequestDrop(int32 Slot, int32 Num)
{
	if (GetOwner()->HasAuthority())
	{
		DoDrop(Slot, Num);
	}
	else
	{
		ServerDrop(Slot, Num);
	}
}

void UKGInventoryRPCComponent::ServerTransfer_Implementation(bool bFromContainer, int32 Slot, int32 Num)
{
	DoTransfer(bFromContainer, Slot, Num);
}

void UKGInventoryRPCComponent::ServerCloseContainer_Implementation()
{
	CloseContainer();
}

void UKGInventoryRPCComponent::ServerSetLocked_Implementation(bool bLocked)
{
	IKGContainerOwner* Container = Cast<IKGContainerOwner>(ViewedActor.Get());
	if (!Container || !IsViewStillValid() || !Container->CanPlayerToggleLock(Cast<APlayerState>(GetOwner())))
	{
		return;
	}
	Container->SetContainerLocked(bLocked);
	bViewedLocked = Container->IsContainerLocked();
	MarkViewDirty();
	BroadcastView();
}

void UKGInventoryRPCComponent::ServerStandUp_Implementation()
{
	DoStandUp();
}

void UKGInventoryRPCComponent::ServerDrop_Implementation(int32 Slot, int32 Num)
{
	DoDrop(Slot, Num);
}

void UKGInventoryRPCComponent::DoTransfer(bool bFromContainer, int32 Slot, int32 Num)
{
	if (!IsViewStillValid())
	{
		CloseContainer();
		return;
	}
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(Cast<APlayerState>(GetOwner()));
	UKGInventoryComponent* Box = ServerViewed.Get();
	if (!Pockets || !Box)
	{
		return;
	}
	UKGInventoryComponent* From = bFromContainer ? Box : Pockets;
	UKGInventoryComponent* To = bFromContainer ? Pockets : Box;
	const FKGItemEntry* Entry = From->FindSlot(Slot);
	if (!Entry)
	{
		RefreshMirror();   // stale click: resend the truth
		return;
	}
	const int32 Amount = Num < 0 ? Entry->Count : FMath::Clamp(Num, 1, Entry->Count);
	From->MoveSlotTo(To, Slot, Amount);
}

void UKGInventoryRPCComponent::DoStandUp()
{
	AKGSeat::StandUpCharacter(GetCharacter());
}

void UKGInventoryRPCComponent::DoDrop(int32 Slot, int32 Num)
{
	AKGCharacter* Character = GetCharacter();
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(Cast<APlayerState>(GetOwner()));
	if (!Character || Character->IsDead() || !Pockets)
	{
		return;
	}
	const FKGItemEntry* Entry = Pockets->FindSlot(Slot);
	if (!Entry)
	{
		return;
	}
	const int32 Amount = Num < 0 ? Entry->Count : FMath::Clamp(Num, 1, Entry->Count);
	FName ItemId;
	const int32 Removed = Pockets->RemoveFromSlot(Slot, Amount, ItemId);
	if (Removed > 0)
	{
		const FVector Forward = Character->GetActorForwardVector();
		AKGPickup::SpawnPickup(GetWorld(), ItemId, Removed, Character->GetActorLocation() + Forward * 70.0f);
	}
}
