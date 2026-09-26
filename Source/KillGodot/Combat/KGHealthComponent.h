#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FKGOnHealthChanged, float, NewHealth, float, Delta, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKGOnDeath, AActor*, Killer, FName, DamageType);

/**
 * 100 HP. Authority applies damage; health replicates.
 * Second wind: after RegenDelay seconds without damage, health creeps back at RegenPerSecond, but only up to RegenCap
 * (60). Anything above that needs a bandage (SPRINT-035b), so wounds stay worth treating.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	/** Second-wind step: health after Dt seconds, SinceDamage seconds after the last hit; pure, unit-tested. */
	static float RegenStep(float Current, float SinceDamage, float Dt, float Delay, float Rate, float Cap)
	{
		if (Current <= 0.0f || Current >= Cap || SinceDamage < Delay)
		{
			return Current;
		}
		return FMath::Min(Cap, Current + Rate * Dt);
	}

	UPROPERTY(BlueprintAssignable, Category = "KillGodot|Health")
	FKGOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "KillGodot|Health")
	FKGOnDeath OnDeath;

	UPROPERTY(EditAnywhere, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Health")
	float RegenDelay = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Health")
	float RegenPerSecond = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Health")
	float RegenCap = 60.0f;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Health, SaveGame)
	float Health = 100.0f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

private:
	float SinceDamage = 0.0f;
	/** Regen accumulates here and lands in whole points, so Health replicates once per HP, not every frame. */
	float RegenCarry = 0.0f;
};
