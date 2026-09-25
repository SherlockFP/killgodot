#include "Inventory/KGInventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGItemCatalog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

bool FKGItemList::SyncFrom(const FKGItemList& Source)
{
	bool bChanged = false;
	bool bRemoved = false;
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		if (!Source.FindSlot(Entries[i].Slot))
		{
			Entries.RemoveAtSwap(i);
			bRemoved = true;
		}
	}
	if (bRemoved)
	{
		MarkArrayDirty();
		bChanged = true;
	}
	for (const FKGItemEntry& Src : Source.Entries)
	{
		if (FKGItemEntry* Mine = FindSlot(Src.Slot))
		{
			if (Mine->ItemId != Src.ItemId || Mine->Count != Src.Count || Mine->Grams != Src.Grams)
			{
				Mine->ItemId = Src.ItemId;
				Mine->Count = Src.Count;
				Mine->Grams = Src.Grams;
				MarkItemDirty(*Mine);
				bChanged = true;
			}
		}
		else
		{
			MarkItemDirty(Entries.Emplace_GetRef(Src.Slot, Src.ItemId, Src.Count, Src.Grams));
			bChanged = true;
		}
	}
	return bChanged;
}

UKGInventoryComponent::UKGInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKGInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;   // nobody may read another player's pockets
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryComponent, Items, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGInventoryComponent, Capacity, Params);
}

UKGInventoryComponent* UKGInventoryComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGInventoryComponent>() : nullptr;
}

UKGInventoryComponent* UKGInventoryComponent::FindForPawn(const APawn* Pawn)
{
	return Pawn ? FindForPlayer(Pawn->GetPlayerState()) : nullptr;
}

bool UKGInventoryComponent::CanMutate() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

int32 UKGInventoryComponent::Count(FName ItemId) const
{
	int32 Total = 0;
	for (const FKGItemEntry& Entry : Items.Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			Total += Entry.Count;
		}
	}
	return Total;
}

int32 UKGInventoryComponent::GetGold() const
{
	return Count(FKGItemIds::Coin);
}

int32 UKGInventoryComponent::FindFreeSlot() const
{
	for (int32 Slot = 0; Slot < Capacity; ++Slot)
	{
		if (!Items.FindSlot(Slot))
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

int32 UKGInventoryComponent::RoomFor(FName ItemId) const
{
	const int32 Stack = UKGItemCatalog::GetStackSize(ItemId);
	if (Stack <= 0)
	{
		return 0;
	}
	int32 Room = 0;
	int32 Used = 0;
	for (const FKGItemEntry& Entry : Items.Entries)
	{
		if (Entry.Slot < Capacity)
		{
			++Used;
		}
		if (Entry.ItemId == ItemId)
		{
			Room += FMath::Max(0, Stack - Entry.Count);
		}
	}
	return Room + FMath::Max(0, Capacity - Used) * Stack;
}

int32 UKGInventoryComponent::AddItem(FName ItemId, int32 Num, int32 Grams)
{
	const int32 Stack = UKGItemCatalog::GetStackSize(ItemId);
	if (!CanMutate() || Num <= 0 || Stack <= 0)
	{
		return 0;
	}
	int32 Left = Num;
	Grams = FMath::Max(0, Grams);
	int32 GramsLeft = Grams;
	// Weight that goes with Take of the items (the last chunk takes the rounding remainder).
	auto GramsFor = [&](int32 Take)
	{
		const int32 G = Take >= Left ? GramsLeft : static_cast<int32>(static_cast<int64>(Grams) * Take / Num);
		GramsLeft -= G;
		return G;
	};

	// Top up existing stacks first, lowest slot first (deterministic for host migration and tests).
	TArray<int32> Slots;
	for (const FKGItemEntry& Entry : Items.Entries)
	{
		if (Entry.ItemId == ItemId && Entry.Count < Stack)
		{
			Slots.Add(Entry.Slot);
		}
	}
	Slots.Sort();
	for (const int32 Slot : Slots)
	{
		FKGItemEntry* Entry = Items.FindSlot(Slot);
		const int32 Take = FMath::Min(Left, Stack - Entry->Count);
		Entry->Grams += GramsFor(Take);
		Entry->Count += Take;
		Left -= Take;
		Items.MarkItemDirty(*Entry);
		if (Left <= 0)
		{
			break;
		}
	}
	while (Left > 0)
	{
		const int32 Slot = FindFreeSlot();
		if (Slot == INDEX_NONE)
		{
			break;
		}
		const int32 Take = FMath::Min(Left, Stack);
		const int32 G = GramsFor(Take);
		Items.MarkItemDirty(Items.Entries.Emplace_GetRef(Slot, ItemId, Take, G));
		Left -= Take;
	}

	const int32 Added = Num - Left;
	if (Added > 0)
	{
		MarkItemsDirty(nullptr);
		Broadcast();
	}
	return Added;
}

int32 UKGInventoryComponent::RemoveItem(FName ItemId, int32 Num)
{
	int32 Grams = 0;
	return RemoveItemWeighed(ItemId, Num, Grams);
}

int32 UKGInventoryComponent::GramsOf(FName ItemId) const
{
	int32 Total = 0;
	for (const FKGItemEntry& Entry : Items.Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			Total += Entry.Grams;
		}
	}
	return Total;
}

int32 UKGInventoryComponent::RemoveItemWeighed(FName ItemId, int32 Num, int32& OutGrams)
{
	OutGrams = 0;
	if (!CanMutate() || Num <= 0)
	{
		return 0;
	}
	// Take from the highest slot first so the front of the pockets stays stable.
	TArray<int32> Slots;
	for (const FKGItemEntry& Entry : Items.Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			Slots.Add(Entry.Slot);
		}
	}
	Slots.Sort(TGreater<int32>());
	int32 Removed = 0;
	for (const int32 Slot : Slots)
	{
		if (Removed >= Num)
		{
			break;
		}
		FName Unused;
		int32 G = 0;
		Removed += RemoveFromSlot(Slot, Num - Removed, Unused, &G);
		OutGrams += G;
	}
	return Removed;
}

int32 UKGInventoryComponent::RemoveFromSlot(int32 Slot, int32 Num, FName& OutItemId, int32* OutGrams)
{
	OutItemId = NAME_None;
	if (OutGrams)
	{
		*OutGrams = 0;
	}
	if (!CanMutate() || Num <= 0)
	{
		return 0;
	}
	const int32 Index = Items.Entries.IndexOfByPredicate([Slot](const FKGItemEntry& E) { return E.Slot == Slot; });
	if (Index == INDEX_NONE)
	{
		return 0;
	}
	FKGItemEntry& Entry = Items.Entries[Index];
	OutItemId = Entry.ItemId;
	const int32 Take = FMath::Min(Num, Entry.Count);
	const int32 TakeGrams = Take >= Entry.Count ? Entry.Grams
	                                            : static_cast<int32>(static_cast<int64>(Entry.Grams) * Take / FMath::Max(1, Entry.Count));
	Entry.Grams -= TakeGrams;
	if (OutGrams)
	{
		*OutGrams = TakeGrams;
	}
	Entry.Count -= Take;
	if (Entry.Count <= 0)
	{
		Items.Entries.RemoveAtSwap(Index);
		Items.MarkArrayDirty();
	}
	else
	{
		Items.MarkItemDirty(Entry);
	}
	if (Take > 0)
	{
		MarkItemsDirty(nullptr);
		Broadcast();
	}
	return Take;
}

int32 UKGInventoryComponent::MoveTo(UKGInventoryComponent* Other, FName ItemId, int32 Num)
{
	if (!Other || Other == this || !CanMutate() || !Other->CanMutate())
	{
		return 0;
	}
	const int32 Wanted = FMath::Min3(Num, Count(ItemId), Other->RoomFor(ItemId));
	if (Wanted <= 0)
	{
		return 0;
	}
	int32 Grams = 0;
	const int32 Removed = RemoveItemWeighed(ItemId, Wanted, Grams);
	const int32 Added = Other->AddItem(ItemId, Removed, Grams);
	if (Added < Removed)
	{
		// never destroy items on a partial move (the weight that did not fit comes back with them)
		const int32 Moved = static_cast<int32>(static_cast<int64>(Grams) * Added / FMath::Max(1, Removed));
		AddItem(ItemId, Removed - Added, Grams - Moved);
	}
	return Added;
}

int32 UKGInventoryComponent::MoveSlotTo(UKGInventoryComponent* Other, int32 Slot, int32 Num)
{
	if (!Other || Other == this || !CanMutate() || !Other->CanMutate())
	{
		return 0;
	}
	const FKGItemEntry* Entry = Items.FindSlot(Slot);
	if (!Entry)
	{
		return 0;
	}
	const FName ItemId = Entry->ItemId;
	const int32 Wanted = FMath::Min3(Num, Entry->Count, Other->RoomFor(ItemId));
	if (Wanted <= 0)
	{
		return 0;
	}
	FName Removed;
	int32 Grams = 0;
	const int32 Taken = RemoveFromSlot(Slot, Wanted, Removed, &Grams);
	const int32 Added = Other->AddItem(ItemId, Taken, Grams);
	if (Added < Taken)
	{
		const int32 Moved = static_cast<int32>(static_cast<int64>(Grams) * Added / FMath::Max(1, Taken));
		AddItem(ItemId, Taken - Added, Grams - Moved);
	}
	return Added;
}

void UKGInventoryComponent::Clear()
{
	if (!CanMutate() || Items.Entries.Num() == 0)
	{
		return;
	}
	Items.Entries.Reset();
	Items.MarkArrayDirty();
	MarkItemsDirty(nullptr);
	Broadcast();
}

void UKGInventoryComponent::SetCapacity(int32 NewCapacity)
{
	if (!CanMutate())
	{
		return;
	}
	Capacity = FMath::Clamp(NewCapacity, 1, 64);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryComponent, Capacity, this);
	Broadcast();
}

void UKGInventoryComponent::MarkItemsDirty(FKGItemEntry* Changed)
{
	if (Changed)
	{
		Items.MarkItemDirty(*Changed);
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGInventoryComponent, Items, this);
	if (AActor* Owner = GetOwner())
	{
		// The player state only updates twice a second; pockets should feel instant.
		Owner->ForceNetUpdate();
	}
}

void UKGInventoryComponent::Broadcast()
{
	OnChanged.Broadcast(this);
}

void UKGInventoryComponent::OnRep_Items()
{
	Broadcast();
}

void UKGInventoryComponent::WriteSaveData(TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	FMemoryWriter Writer(OutBytes, true);
	FObjectAndNameAsStringProxyArchive Ar(Writer, true);
	Ar.ArIsSaveGame = true;
	Serialize(Ar);
}

void UKGInventoryComponent::ReadSaveData(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() == 0)
	{
		return;
	}
	FMemoryReader Reader(Bytes, true);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);
	Ar.ArIsSaveGame = true;
	Serialize(Ar);
	NotifyRestored();
}

void UKGInventoryComponent::NotifyRestored()
{
	for (FKGItemEntry& Entry : Items.Entries)
	{
		Items.MarkItemDirty(Entry);
	}
	Items.MarkArrayDirty();
	MarkItemsDirty(nullptr);
	Broadcast();
}
