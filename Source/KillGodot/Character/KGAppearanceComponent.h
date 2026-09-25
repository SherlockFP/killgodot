#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/KGTypes.h"
#include "Character/KGVillagerLook.h"
#include "KGAppearanceComponent.generated.h"

class AKGCharacter;
class AKGPlayerState;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * SPRINT-027a: the villager's body appearance, applied locally on every machine that renders it.
 *
 *  - Villager variety: reads the public FKGVillagerLook from the player's UKGCosmeticsComponent (seeded per match,
 *    unique in the lobby) and builds it: body mesh (VillagerVariety/Bodies, one shared skeleton so every clip,
 *    emote and the sitting pose keep working), build scale, tinted skin/hair/outfit MIDs, hair, beard, hood,
 *    pauldrons, cape, apron, hat. The cosmetics loadout overrides the archetype's own part in that slot
 *    (a shop hat replaces the archetype's hat/hood, a shop beard the archetype's beard, shop pauldrons its pauldrons).
 *  - Safe role visuals. Nothing here replicates: this component has NO replicated property and NO RPC (asserted by
 *    KillGodot.Appearance.RoleDataNeverPublic).
 *      cuff: a ring on the owner-only first-person arms tinted by the alignment of AKGPlayerState::PrivateRoleId,
 *            which only the owning client receives (COND_OwnerOnly); nobody else can even compute it.
 *      sash: a band across the body tinted by the alignment of AKGPlayerState::RevealedRoleId, which the server
 *            makes public on purpose (death, trial, epilogue) - the corpse keeps it.
 *  Dedicated servers do nothing here.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGAppearanceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Builds the look on the body now (also used by the kg.Look.Lineup contact sheet with no player state). */
	void ApplyLook(const FKGVillagerLook& Look, const TArray<FName>& EquippedCosmetics);
	const FKGVillagerLook& GetAppliedLook() const { return Applied; }

	/** Public sash by alignment (None/hidden when RoleId is none). */
	void SetSash(FName RoleId);
	/** Owner-only cuff on the first-person arms. */
	void SetCuff(EKGAlignment Alignment, bool bShow);

	/** Alignment colours shared by the cuff, the sash and the lineup (Town blue, Impatient red, Neutral amber). */
	static FLinearColor AlignmentColour(EKGAlignment Alignment);
	static EKGAlignment AlignmentOfRole(FName RoleId);

	/** Asset paths (new assets from Tools/Unreal/kg_import_villager_variety.py). */
	static FString BodyPath(EKGVillagerBody Body);
	static FString HairPath(EKGVillagerHair Hair);
	static FString HatPath(EKGVillagerHat Hat);
	static const TCHAR* CapePath();
	static const TCHAR* ApronPath();
	static const TCHAR* SashPath();
	static const TCHAR* CuffPath();
	static FVector BuildScale(EKGVillagerBuild Build);

private:
	void RefreshFromPlayerState();
	void ClearAttachments();
	UStaticMeshComponent* Attach(const TCHAR* Path, FName Bone, const FLinearColor& Tint, bool bMirror = false,
	                             float Glow = 0.0f);
	void TintSlots(const FKGVillagerLook& Look, const FKGVillagerArchetype& A);

	AKGCharacter* Character() const;

	TWeakObjectPtr<AKGPlayerState> LastPlayerState;
	FKGVillagerLook Applied;
	TArray<FName> AppliedCosmetics;
	bool bApplied = false;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Attachments;
	TWeakObjectPtr<UStaticMeshComponent> Sash;
	TWeakObjectPtr<UStaticMeshComponent> Cuff;
	FName SashRole;
	EKGAlignment CuffAlignment = EKGAlignment::Neutral;
	bool bCuffShown = false;
};
