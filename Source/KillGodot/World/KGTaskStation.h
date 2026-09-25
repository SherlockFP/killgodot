#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/KGInteractable.h"
#include "KGTaskStation.generated.h"

class UStaticMeshComponent;

/**
 * Among-Us-style village chore (Docs/01_GDD_Core.md §2 "Büyük Hazırlık", §8). Press E: players get the chore's
 * minigame (UKGChoreComponent + Chores/UI, server-validated); bots and chores without a minigame fall back to "stay
 * close for WorkSeconds". Town completions fill the village Preparation bar; the Impatient can fake them (no progress).
 * The floating marker is shown only to local players who still have this chore on their list.
 * Placed by Tools/Unreal/kg_build_village.py from Tools/Level/morrowmere_layout.json.
 */
UCLASS()
class KILLGODOT_API AKGTaskStation : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGTaskStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FName TaskId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FString TaskName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	float WorkSeconds = 4.0f;

	/** Players must stay within this distance while working. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	float WorkRadius = 260.0f;

	/** True if the first local player still has this chore to do (drives the marker and the prompt). */
	bool IsWantedByLocalPlayer() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Interaction volume so the E trace hits the station even when the prop has no collision. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hitbox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Marker;

	float MarkerTime = 0.0f;
};
