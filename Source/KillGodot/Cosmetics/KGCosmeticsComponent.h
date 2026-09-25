#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	UFUNCTION()
	void OnRep_Equipped();

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
