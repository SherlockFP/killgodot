#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KGCosmeticShop.generated.h"

class APlayerController;

/**
 * "Village Market" cosmetics shop (Slate, no assets): grid of cosmetics with rarity colour, gold price, Buy / Equip,
 * category tabs and a preview panel. Earned gold only (UKGProfileSave): no real money, no loot boxes
 * (Docs/07_Economy_Cosmetics.md). Works from the main menu (no pawn needed) and in a match (equipping pushes the
 * loadout to the server so everyone sees it). Esc closes.
 */
UCLASS()
class KILLGODOT_API UKGCosmeticShop : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Main menu "Cosmetics" button: UKGCosmeticShop::OpenShop(GetOwningPlayer()). Null = first local player. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Cosmetics")
	static void OpenShop(APlayerController* PC);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Cosmetics")
	static void CloseShop(APlayerController* PC);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	static bool IsShopOpen(const APlayerController* PC);
};
