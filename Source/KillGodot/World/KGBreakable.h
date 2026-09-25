#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGBreakable.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * Crates, barrels, pots: punch or hit them until they burst into planks (replicated break, local debris).
 * Physics-simulated while intact so they can be pushed, thrown and float (KGBuoyancyComponent).
 */
UCLASS()
class KILLGODOT_API AKGBreakable : public AActor
{
	GENERATED_BODY()

public:
	AKGBreakable();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Breakable")
	void SetMesh(UStaticMesh* NewMesh);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	float Health = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	TObjectPtr<UStaticMesh> DebrisMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	int32 DebrisCount = 7;

	/** FKGLoot table rolled when it bursts (Crate, Barrel, Pot...); None = nothing drops (boats, props). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	FName LootTable = TEXT("Crate");

	/** Mixed with the actor's stable name, so every crate drops the same thing in every replay of a seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Breakable")
	int32 LootSeed = 0;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<class UKGBuoyancyComponent> Buoyancy;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	UFUNCTION()
	void OnRep_Broken();

	void Burst();
	FVector LastHitDir = FVector::ZeroVector;
};
