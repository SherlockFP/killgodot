#include "Combat/KGHealthComponent.h"
#include "Net/UnrealNetwork.h"

UKGHealthComponent::UKGHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKGHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKGHealthComponent, Health);
}

float UKGHealthComponent::ApplyDamage(float Amount, AActor* DamageInstigator, FName DamageType)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}
	const float Old = Health;
	Health = ComputeHealth(Health, Amount, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - Old, DamageInstigator);
	if (IsDead())
	{
		OnDeath.Broadcast(DamageInstigator, DamageType);
	}
	return Old - Health;
}

float UKGHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}
	const float Old = Health;
	Health = ComputeHealth(Health, -Amount, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - Old, nullptr);
	return Health - Old;
}

void UKGHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
	// Clients learn about deaths through replication; the killer stays server-side (secret in a deduction game).
	if (IsDead() && OldHealth > 0.0f)
	{
		OnDeath.Broadcast(nullptr, NAME_None);
	}
}
