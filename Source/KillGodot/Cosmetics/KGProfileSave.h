#pragma once

#include "CoreMinimal.h"
#include "Cosmetics/KGCosmeticCatalog.h"
#include "GameFramework/SaveGame.h"
#include "KGProfileSave.generated.h"

class APlayerController;

UENUM(BlueprintType)
enum class EKGBuyResult : uint8
{
	Bought,
	AlreadyOwned,
	NotEnoughGold,
	Unknown
};

/**
 * Local player profile (Saved/SaveGames/KG_Profile.sav): persistent Gold, owned and equipped cosmetics.
 * Gold is EARNED ONLY (playing, banking match coins, achievements later) and never sold; there is no real-money
 * currency and no random box here (Docs/07_Economy_Cosmetics.md). Online persistence replaces this later
 * (Docs 07 section 8: nothing stored on the client is trusted), so the server accepts equipped ids as cosmetic-only.
 *
 * In-match gold is different: the Coin item in the pockets (UKGInventoryComponent). BankMatchCoins converts it 1:1.
 */
UCLASS()
class KILLGODOT_API UKGProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	static constexpr int32 StartingGold = 250;
	static const TCHAR* SlotName;

	/** The profile of this machine (loaded on first use, created with starter items if missing). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	static UKGProfileSave* GetProfile();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	static void ResetProfile();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	bool SaveProfile();

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	bool Owns(FName CosmeticId) const;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	EKGBuyResult Buy(FName CosmeticId);

	/** Equips an owned cosmetic in its slot (replacing whatever was there). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	bool Equip(FName CosmeticId);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	void Unequip(EKGCosmeticSlot Slot);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	FName GetEquipped(EKGCosmeticSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	bool IsEquipped(FName CosmeticId) const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	TArray<FName> GetEquippedIds() const;

	/** Reward path (Almanac tier, achievement, dev unlock): owns it without paying. Saves. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	bool GrantCosmetic(FName CosmeticId, FName Reason);

	/** Earned gold only (match rewards, banking coins, dev command). Saves. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	void AwardGold(int32 Amount, FName Reason);

	/** Local player: converts the Coin items currently in PC's pockets into profile Gold (1:1). Returns amount. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	static int32 BankMatchCoins(APlayerController* PC);

	/** SPRINT-027a: locked villager archetype (NAME_None = a random one each match). Saves and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Profile")
	void SetPreferredLook(FName Archetype);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Profile")
	FName GetPreferredLook() const { return PreferredLook; }

	/** Fires after any change (shop refresh, loadout push). */
	static FSimpleMulticastDelegate& OnProfileChanged();

protected:
	UPROPERTY(SaveGame)
	int32 Version = 0;

	UPROPERTY(SaveGame)
	int32 Gold = 0;

	UPROPERTY(SaveGame)
	int32 LifetimeGoldEarned = 0;

	UPROPERTY(SaveGame)
	TArray<FName> OwnedCosmetics;

	/** Indexed by EKGCosmeticSlot. */
	UPROPERTY(SaveGame)
	TArray<FName> EquippedBySlot;

	UPROPERTY(SaveGame)
	bool bStarterGranted = false;

	UPROPERTY(SaveGame)
	FName PreferredLook;

	/** Grants starter gold/items, drops unknown ids, sizes the slot array. */
	void Sanitize();
};
