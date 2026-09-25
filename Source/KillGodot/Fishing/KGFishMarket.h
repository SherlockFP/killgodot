#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGFishMarket.generated.h"

class APlayerState;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Madam Brine's table at the Fish Market (Docs/01b_Village_Life_Fun.md NPC list): press E to sell every fish and sea
 * find in your pockets for gold coins (price by species value and catch weight, FKGFishingRules::SellPrice). Anyone
 * without a rod is lent one. Spawned at runtime by UKGFishingSubsystem on the map's "fish_market" building (the
 * arcade's fish table), or in front of you with kg.Fish.Market. Replicated, static.
 */
UCLASS()
class KILLGODOT_API AKGFishMarketStall : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGFishMarketStall();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Authority: sells every sellable stack of Seller's pockets. Returns coins paid; OutItems = items sold. */
	static int32 SellAll(APlayerState* Seller, int32& OutItems);

	/** Authority: a stall at Location facing Yaw (the counter's long side along local X, customers at local -Y). */
	static AKGFishMarketStall* SpawnStall(UWorld* World, const FVector& Location, float Yaw);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Interaction volume (hidden) around the fish table so the E trace hits it. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Sign;

	/** A golden carp turning slowly above the table: the market reads from across the quay. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Icon;

	float IconSpin = 0.0f;
};
