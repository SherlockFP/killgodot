#pragma once

#include "CoreMinimal.h"
#include "KGVillagerLook.generated.h"

/**
 * SPRINT-027a: per-PLAYER villager variety. A look is public cosmetic data (everyone sees your body) and is a pure
 * function of (match seed, player id, looks already taken in the lobby, an optional locked archetype). It never
 * takes a role as input and carries nothing role-dependent: in a social deduction game anything on the body that
 * followed the role would reveal it (Docs/Iterations/SPRINT-027-CharactersAndViewmodel.md, design note).
 */
UENUM(BlueprintType)
enum class EKGVillagerBody : uint8
{
	MalePeasant,        // shipped SK_KG_Villager_M (hair baked in)
	FemalePeasant,      // shipped SK_KG_Villager_F (hair baked in)
	MalePeasantBald,    // VillagerVariety/Bodies (Tools/Blender/kg_build_villager_variants.py)
	FemalePeasantBald,
	MaleRanger,
	FemaleRanger,
	MaleMixed,          // ranger torso, peasant legs
	FemaleMixed,        // peasant torso, ranger legs
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EKGVillagerBuild : uint8
{
	Regular,
	Tall,
	Stocky,
	Short,
	Slim,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EKGVillagerHair : uint8
{
	Baked,          // whatever the body mesh carries (shipped M/F only)
	Bald,
	Buns,
	Buzzed,
	BuzzedFemale,
	Long,
	SimpleParted,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EKGVillagerHat : uint8
{
	None,
	Witch,
	Straw,
	Toque,
	Crown,
	Count UMETA(Hidden)
};

/** One archetype: a body, a silhouette (hair, beard, hood, pauldrons, cape, apron, hat) and an outfit palette. */
struct KILLGODOT_API FKGVillagerArchetype
{
	FName Name;
	bool bFemale = false;
	bool bOld = false;
	EKGVillagerBody Body = EKGVillagerBody::MalePeasantBald;
	EKGVillagerBuild Build = EKGVillagerBuild::Regular;
	EKGVillagerHair Hair = EKGVillagerHair::Bald;
	bool bBeard = false;
	bool bHood = false;
	bool bPauldrons = false;
	bool bCape = false;
	bool bApron = false;
	EKGVillagerHat Hat = EKGVillagerHat::None;
	/** Outfit tints (M_KG_Character "Tint" on the Peasant/Ranger slots), one per OutfitVariant. */
	FLinearColor Outfit[3];
	/** Hood / cape / apron / hat colour. */
	FLinearColor Accent = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct KILLGODOT_API FKGVillagerLook
{
	GENERATED_BODY()

	static constexpr uint8 Unassigned = 255;

	/** Index into FKGVillagerLookGen::Archetypes(); 255 = not assigned yet (the default villager). */
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	uint8 Archetype = Unassigned;

	UPROPERTY(BlueprintReadOnly, Category = "Look")
	uint8 SkinTone = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Look")
	uint8 HairColour = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Look")
	uint8 OutfitVariant = 0;

	bool IsAssigned() const { return Archetype != Unassigned; }
	uint32 Key() const
	{
		return (static_cast<uint32>(Archetype) << 24) | (static_cast<uint32>(SkinTone) << 16) |
		       (static_cast<uint32>(HairColour) << 8) | OutfitVariant;
	}
	bool operator==(const FKGVillagerLook& Other) const { return Key() == Other.Key(); }
	bool operator!=(const FKGVillagerLook& Other) const { return !(*this == Other); }
	FString ToString() const;
};

class KILLGODOT_API FKGVillagerLookGen
{
public:
	static constexpr int32 SkinToneCount = 5;
	static constexpr int32 HairColourCount = 8;   // 0..5 young, 6..7 grey/white (old archetypes)
	static constexpr int32 OutfitVariantCount = 3;

	static const TArray<FKGVillagerArchetype>& Archetypes();
	static const FKGVillagerArchetype* FindArchetype(FName Name);
	static int32 ArchetypeIndex(FName Name);
	static const FKGVillagerArchetype& ArchetypeOf(const FKGVillagerLook& Look);

	static FLinearColor SkinTint(uint8 Tone);
	static FLinearColor HairTint(uint8 Colour);

	/**
	 * Deterministic per (Seed, PlayerId). Never equal to a look in Taken while any combination is free; spreads
	 * archetypes first (no archetype twice while an unused one exists), then palettes. Preferred (a locked
	 * archetype from the cosmetics profile) wins whenever one of its palettes is still free.
	 */
	static FKGVillagerLook Generate(uint64 Seed, int32 PlayerId, const TArray<FKGVillagerLook>& Taken,
	                                FName Preferred = NAME_None);
};
