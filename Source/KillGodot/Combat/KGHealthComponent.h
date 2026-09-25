#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FKGOnHealthChanged, float, NewHealth, float, Delta, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKGOnDeath, AActor*, Killer, FName, DamageType);

/** 100 HP, no natural regen (Docs/01_GDD_Core.md §4). Authority applies damage; health replicates. */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns the damage actually applied. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Health")
	float ApplyDamage(float Amount, AActor* DamageInstigator, FName DamageType);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Health")
	float Heal(float Amount);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Health")
	bool IsDead() const { return Health <= 0.0f; }

	/** New health after taking Amount (clamped to [0, Max]); pure, unit-tested. */
	static float ComputeHealth(float Current, float Amount, float Max)
	{
		return FMath::Clamp(Current - Amount, 0.0f, Max);
	}

	UPROPERTY(BlueprintAssignable, Category = "KillGodot|Health")
	FKGOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "KillGodot|Health")
	FKGOnDeath OnDeath;

	UPROPERTY(EditAnywhere, Category = "Health")
	float MaxHealth = 100.0f;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Health, SaveGame)
	float Health = 100.0f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);
};
