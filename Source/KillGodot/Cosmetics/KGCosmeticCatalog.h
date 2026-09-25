#pragma once

#include "CoreMinimal.h"
#include "Inventory/KGItemCatalog.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPath.h"
#include "KGCosmeticCatalog.generated.h"

/** One equipped cosmetic per slot. */
UENUM(BlueprintType)
enum class EKGCosmeticSlot : uint8
{
	Head,
	Face,
	Shoulders,
	Back,
	Belt,
	Count UMETA(Hidden)
};

/**
 * A wearable. Meshes are static meshes attached to a bone of the villager skeleton
 * (/Game/KillGodot/Characters/Villager/SK_KG_Villager_M: root, pelvis, spine_01..03, neck_01, Head, clavicle_l/r,
 * upperarm_l/r, ...; no sockets). Placement is authored in the villager's COMPONENT space (+Y forward, +Z up, cm,
 * feet at 0, Head bone at z=160) and converted to the bone's local space at runtime from the reference pose, so the
 * Quaternius outfit parts (baked in bind pose, see Tools/Unreal/kg_import_outfits.py) sit exactly where the artist
 * modelled them with an identity placement.
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGCosmeticDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	EKGCosmeticSlot Slot = EKGCosmeticSlot::Head;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	EKGRarity Rarity = EKGRarity::Common;

	/** Earned gold only (Docs/07_Economy_Cosmetics.md: no real money, no random boxes in this shop). */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	int32 Price = 0;

	/** Everyone owns it from the first launch. */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	bool bStarter = false;

	/** Shop card icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FString Glyph;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FSoftObjectPath Mesh;

	/** Used instead of Mesh on the female villager (body mesh name contains "_F"), when set. */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FSoftObjectPath FemaleMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FName AttachBone = FName(TEXT("Head"));

	/** When set, a mirrored copy (across the body's YZ plane) goes on this bone (pauldron pairs). */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FName MirrorBone;

	/** Mesh -> villager component space. Identity for bind-pose outfit parts. */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FTransform Placement = FTransform::Identity;

	/** Overrides the "Tint" parameter of M_KG_Character-based materials (colour variants of one mesh). */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	bool bTint = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FLinearColor Tint = FLinearColor::White;
};

UCLASS()
class KILLGODOT_API UKGCosmeticCatalog : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const TArray<FKGCosmeticDef>& GetAll();
	static const FKGCosmeticDef* Find(FName CosmeticId);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	static bool IsValidCosmetic(FName CosmeticId) { return Find(CosmeticId) != nullptr; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	static bool GetCosmeticDef(FName CosmeticId, FKGCosmeticDef& OutDef);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	static FText GetSlotName(EKGCosmeticSlot Slot);

	/** Case-insensitive id or display-name lookup for console commands. */
	static FName ResolveLoose(const FString& Text);
};
