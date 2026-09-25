#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/KGVillagerLook.h"
#include "KGCosmeticsComponent.generated.h"

class AKGCharacter;
class APlayerState;
class UStaticMeshComponent;
struct FKGCosmeticDef;

/**
 * Public, replicated loadout of one player (lives on the player state: everyone sees your hat) plus the local
 * visuals: on every non-dedicated machine it spawns/attaches the cosmetic static meshes onto that player's current
 * AKGCharacter whenever the ids or the pawn change. The owning client pushes its UKGProfileSave loadout once
 * (ServerSetLoadout); bots get a deterministic random-looking loadout so a bot-filled match shows the system.
 * Equipped is UPROPERTY(SaveGame) for host migration.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGCosmeticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGCosmeticsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UKGCosmeticsComponent* FindForPlayer(const APlayerState* PlayerState);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	const TArray<FName>& GetEquipped() const { return Equipped; }

	/**
	 * SPRINT-027a: this player's villager look. Public (everyone renders your body), seeded per match, unique in the
	 * lobby, assigned by the server on the first tick and re-rolled when the owner locks a preferred archetype.
	 * A pure function of (seed, player id, other looks, preference): no role goes in (KGVillagerLook.h).
	 */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Cosmetics")
	const FKGVillagerLook& GetLook() const { return Look; }

	/** Authority: (re)generates the look, unique against every other player state in the game state. */
	void AssignLook(FName PreferredArchetype);

	/** Seed the looks derive from: the running match's seed, else a per-process lobby seed. */
	static uint64 LookSeed(const UWorld* World);

	/** Owning client (or host): sends the local profile's equipped ids to the server. */
	void PushLocalLoadout();

	/** Authority: validates (known ids, one per slot) and replicates. */
	void SetLoadout(const TArray<FName>& Ids);

	/** Rebuilds the attached meshes (also used by kg.CosmeticNudge). */
	void RefreshVisuals();

	/** Dev tuning: component-space placement overrides per cosmetic id (kg.CosmeticNudge). */
	static TMap<FName, FTransform>& DevPlacementOverrides();

	/** Pure math (unit-tested): mesh -> bone-local transform from a component-space placement. */
	static FTransform ComputeBoneRelative(const FTransform& PlacementCS, const FTransform& BoneRefCS, bool bMirrorX);

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetLoadout(const TArray<FName>& Ids);

	/** Owning client -> server: the profile's locked archetype (NAME_None = random). */
	UFUNCTION(Server, Reliable)
	void ServerSetPreferredLook(FName Archetype);

	UFUNCTION()
	void OnRep_Equipped();

	UPROPERTY(Replicated, SaveGame)
	FKGVillagerLook Look;

	/** Server-side memory of the owner's preference (not replicated; the look itself is). */
	FName PreferredLook;

	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	UPROPERTY(ReplicatedUsing = OnRep_Equipped, SaveGame)
	TArray<FName> Equipped;

private:
	void ClearVisuals();
	void AttachOne(const FKGCosmeticDef& Def, AKGCharacter* Character, bool bMirror);
	void AssignBotLoadout();
	void HandleProfileChanged();

	TWeakObjectPtr<AKGCharacter> Decorated;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Spawned;
	TArray<FName> AppliedIds;
	bool bPushedLocalLoadout = false;
	FDelegateHandle ProfileHandle;
};
