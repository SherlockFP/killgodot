#include "Cosmetics/KGProfileSave.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGInventoryComponent.h"
#include "KillGodot.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

const TCHAR* UKGProfileSave::SlotName = TEXT("KG_Profile");

namespace KGProfilePrivate
{
	TStrongObjectPtr<UKGProfileSave>& Cached()
	{
		static TStrongObjectPtr<UKGProfileSave> Profile;
		return Profile;
	}
}

FSimpleMulticastDelegate& UKGProfileSave::OnProfileChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

UKGProfileSave* UKGProfileSave::GetProfile()
{
	TStrongObjectPtr<UKGProfileSave>& Cached = KGProfilePrivate::Cached();
	if (!Cached.IsValid())
	{
		UKGProfileSave* Loaded = nullptr;
		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			Loaded = Cast<UKGProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		}
		if (!Loaded)
		{
			Loaded = NewObject<UKGProfileSave>(GetTransientPackage());
		}
		Cached.Reset(Loaded);
		const bool bFresh = !Loaded->bStarterGranted;
		Loaded->Sanitize();
		if (bFresh)
		{
			Loaded->SaveProfile();
		}
	}
	return Cached.Get();
}

void UKGProfileSave::ResetProfile()
{
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	KGProfilePrivate::Cached().Reset();
	GetProfile();
	OnProfileChanged().Broadcast();
}

bool UKGProfileSave::SaveProfile()
{
	Version = CurrentVersion;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
	if (!bSaved)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Profile: could not write save slot %s"), SlotName);
	}
	return bSaved;
}

void UKGProfileSave::Sanitize()
{
	EquippedBySlot.SetNum(static_cast<int32>(EKGCosmeticSlot::Count));
	OwnedCosmetics.RemoveAll([](FName Id) { return !UKGCosmeticCatalog::IsValidCosmetic(Id); });
	if (!bStarterGranted)
	{
		bStarterGranted = true;
		Gold += StartingGold;
		for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
		{
			if (Def.bStarter)
			{
				OwnedCosmetics.AddUnique(Def.Id);
			}
		}
	}
	for (int32 i = 0; i < EquippedBySlot.Num(); ++i)
	{
		const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(EquippedBySlot[i]);
		if (!Def || static_cast<int32>(Def->Slot) != i || !Owns(Def->Id))
		{
			EquippedBySlot[i] = NAME_None;
		}
	}
	Gold = FMath::Max(0, Gold);
}

bool UKGProfileSave::Owns(FName CosmeticId) const
{
	return OwnedCosmetics.Contains(CosmeticId);
}

void UKGProfileSave::SetPreferredLook(FName Archetype)
{
	if (Archetype == PreferredLook)
	{
		return;
	}
	PreferredLook = Archetype;
	SaveProfile();
	OnProfileChanged().Broadcast();   // the cosmetics component pushes it to the server like the loadout
}

EKGBuyResult UKGProfileSave::Buy(FName CosmeticId)
{
	const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(CosmeticId);
	if (!Def)
	{
		return EKGBuyResult::Unknown;
	}
	if (Owns(CosmeticId))
	{
		return EKGBuyResult::AlreadyOwned;
	}
	if (Gold < Def->Price)
	{
		return EKGBuyResult::NotEnoughGold;
	}
	Gold -= Def->Price;
	OwnedCosmetics.Add(CosmeticId);
	SaveProfile();
	UE_LOG(LogKillGodot, Log, TEXT("Profile: bought %s for %d gold (%d left)"), *CosmeticId.ToString(), Def->Price, Gold);
	OnProfileChanged().Broadcast();
	return EKGBuyResult::Bought;
}

bool UKGProfileSave::Equip(FName CosmeticId)
{
	const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(CosmeticId);
	if (!Def || !Owns(CosmeticId))
	{
		return false;
	}
	EquippedBySlot.SetNum(static_cast<int32>(EKGCosmeticSlot::Count));
	EquippedBySlot[static_cast<int32>(Def->Slot)] = CosmeticId;
	SaveProfile();
	OnProfileChanged().Broadcast();
	return true;
}

void UKGProfileSave::Unequip(EKGCosmeticSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (EquippedBySlot.IsValidIndex(Index) && !EquippedBySlot[Index].IsNone())
	{
		EquippedBySlot[Index] = NAME_None;
		SaveProfile();
		OnProfileChanged().Broadcast();
	}
}

FName UKGProfileSave::GetEquipped(EKGCosmeticSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return EquippedBySlot.IsValidIndex(Index) ? EquippedBySlot[Index] : NAME_None;
}

bool UKGProfileSave::IsEquipped(FName CosmeticId) const
{
	return !CosmeticId.IsNone() && EquippedBySlot.Contains(CosmeticId);
}

TArray<FName> UKGProfileSave::GetEquippedIds() const
{
	TArray<FName> Ids;
	for (const FName Id : EquippedBySlot)
	{
		if (!Id.IsNone())
		{
			Ids.Add(Id);
		}
	}
	return Ids;
}

bool UKGProfileSave::GrantCosmetic(FName CosmeticId, FName Reason)
{
	if (!UKGCosmeticCatalog::IsValidCosmetic(CosmeticId) || Owns(CosmeticId))
	{
		return false;
	}
	OwnedCosmetics.Add(CosmeticId);
	SaveProfile();
	UE_LOG(LogKillGodot, Log, TEXT("Profile: granted %s (%s)"), *CosmeticId.ToString(), *Reason.ToString());
	OnProfileChanged().Broadcast();
	return true;
}

void UKGProfileSave::AwardGold(int32 Amount, FName Reason)
{
	if (Amount <= 0)
	{
		return;
	}
	Gold += Amount;
	LifetimeGoldEarned += Amount;
	SaveProfile();
	UE_LOG(LogKillGodot, Log, TEXT("Profile: +%d gold (%s), now %d"), Amount, *Reason.ToString(), Gold);
	OnProfileChanged().Broadcast();
}

int32 UKGProfileSave::BankMatchCoins(APlayerController* PC)
{
	const UKGInventoryComponent* Pockets = PC ? UKGInventoryComponent::FindForPlayer(PC->PlayerState) : nullptr;
	const int32 Coins = Pockets ? Pockets->GetGold() : 0;
	if (Coins > 0)
	{
		GetProfile()->AwardGold(Coins, TEXT("MatchCoins"));
	}
	return Coins;
}
