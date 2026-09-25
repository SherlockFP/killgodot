#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPath.h"
#include "KGItemCatalog.generated.h"

/**
 * Shared rarity ladder for items and cosmetics (Docs/07_Economy_Cosmetics.md section 2): Siradan (grey), Kasaba (blue),
 * Nadir (purple), Efsane (pink). Mythic/Godot's tiers come with the crate system later.
 */
UENUM(BlueprintType)
enum class EKGRarity : uint8
{
	Common,
	Rare,
	Epic,
	Legendary
};

/** One kind of item. Pure data; the catalog below is the single source of truth (no editor assets). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGItemDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FText Description;

	/** Icon = a rounded tile in this colour with a short glyph on it (no texture assets needed). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FString Glyph;

	/** Max count per inventory slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 StackSize = 1;

	/** Worth in gold coins (selling/trading later; loot tables weigh by it). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 GoldValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EKGRarity Rarity = EKGRarity::Common;

	/** Free-form tags: Currency, Fish, Food, Valuable, Tool, Quest, Junk, Sea, Legendary... */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TArray<FName> Tags;

	/** World pickup look. Missing/None falls back to PickupFallbackShape tinted with IconColor. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FSoftObjectPath PickupMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FSoftObjectPath PickupFallbackShape;

	/** Largest dimension of the pickup in the world (cm); the mesh is scaled to fit. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	float PickupSize = 25.0f;

	/** Extra non-uniform scale/rotation of the pickup mesh (e.g. a coin standing on its edge, a bone lying down). */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FVector PickupShapeScale = FVector::OneVector;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FRotator PickupShapeRotation = FRotator::ZeroRotator;

	bool HasTag(FName Tag) const { return Tags.Contains(Tag); }
};

/** Well-known item ids (use these instead of string literals). */
struct KILLGODOT_API FKGItemIds
{
	static const FName Coin;
	static const FName Key;
	static const FName Bone;
	static const FName OldRing;
	static const FName Pearl;
	static const FName FishMackerel;
	static const FName FishCod;
	static const FName FishSalmon;
	static const FName FishGoldenCarp;
	static const FName Bread;
	static const FName Apple;
	static const FName Rope;
	static const FName Candle;
	static const FName TreasureMap;
	// Fishing (Source/KillGodot/Fishing): the rod, and the junk / treasure of the "Fishing" loot table.
	static const FName FishingRod;
	static const FName OldBoot;
	static const FName MessageBottle;
	static const FName CoinPouch;
};

/**
 * Static C++ item catalog. In-match gold is the Coin item (stack 999) so it can be looted, dropped, stashed in a
 * chest and stolen like anything else; the persistent out-of-match Gold balance lives in UKGProfileSave (Cosmetics/).
 */
UCLASS()
class KILLGODOT_API UKGItemCatalog : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const TArray<FKGItemDef>& GetAll();
	static const FKGItemDef* Find(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static bool IsValidItem(FName ItemId) { return Find(ItemId) != nullptr; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static bool GetItemDef(FName ItemId, FKGItemDef& OutDef);

	/** 0 for unknown items (so they can never be added). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static int32 GetStackSize(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static FText GetDisplayName(FName ItemId);

	/** Case-insensitive lookup by id or display name ("coin", "golden carp") for console commands. */
	static FName ResolveLoose(const FString& Text);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static FLinearColor GetRarityColor(EKGRarity Rarity);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Items")
	static FText GetRarityName(EKGRarity Rarity);
};
