#pragma once

#include "CoreMinimal.h"
#include "Dig/KGPassage.h"
#include "KGSecretPassage.generated.h"

/**
 * SPRINT-040 Storm Manor secret (rotating bookcase, portrait door, fireplace passage, ...): a two-way AKGPassage whose
 * ends start hidden. E while undiscovered = discover: every end with the same SecretId opens for everyone (replicated,
 * stays open, a reveal sound at both ends, UKGManorSubsystem::OnSecretDiscovered gives the secret chores). E once
 * discovered = travel (AKGPassage). The minimap draws it only after discovery (Manor/KGManorHud.inl).
 */
UCLASS()
class KILLGODOT_API AKGSecretPassage : public AKGPassage
{
	GENERATED_BODY()

public:
	AKGSecretPassage();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret")
	FName SecretId;

	/** Bookcase, Portrait, Fireplace, Wardrobe, Dumbwaiter, Crypt, Crawlway, Observatory ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret")
	FName SecretKind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret")
	FText DiscoverPrompt;

	UPROPERTY(ReplicatedUsing = OnRep_Discovered, SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Secret")
	bool bDiscovered = false;

	bool IsDiscovered() const { return bDiscovered; }

	/** Authority: discover this end only (the subsystem / DiscoverAll handle the siblings). */
	void AuthSetDiscovered(bool bNow, bool bAnnounce);

	/** Authority: every end with SecretId discovered + the manor subsystem told. Returns the number of ends. */
	static int32 DiscoverAll(UWorld* World, FName SecretId, AKGCharacter* By);

protected:
	UFUNCTION()
	void OnRep_Discovered();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRevealed();
};
