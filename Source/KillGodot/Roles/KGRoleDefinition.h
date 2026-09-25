#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/KGTypes.h"
#include "KGRoleDefinition.generated.h"

class UGameplayAbility;
class UTexture2D;

/** Lightweight, data-only view of a role used by the role list generator and tests. */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGRoleInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FName RoleId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	EKGRoleCategory Category = EKGRoleCategory::TownInvestigative;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	EKGFaction Faction = EKGFaction::Town;

	/** Magnitude of the role's influence (always positive; the alignment gives the sign). Docs/02_Roles.md "Güç". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	int32 Power = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	bool bUnique = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	int32 MinPlayers = 0;

	/** Roles that may never appear in the same match as this one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	TArray<FName> ExclusiveWith;

	EKGAlignment GetAlignment() const;
};

/** Designer-facing role asset. The generator only needs FKGRoleInfo; everything else drives gameplay and UI. */
UCLASS(BlueprintType)
class KILLGODOT_API UKGRoleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FKGRoleInfo Info;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	EKGUseMode UseMode = EKGUseMode::Ledger;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	EKGSheriffResult SheriffResult = EKGSheriffResult::NotSuspicious;

	/** Investigator group (1..17), Docs/02_Roles.md "Dedektif grupları". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	int32 InvestigatorGroup = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role|UI")
	TSoftObjectPtr<UTexture2D> CardArt;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("KGRole"), Info.RoleId);
	}
};
