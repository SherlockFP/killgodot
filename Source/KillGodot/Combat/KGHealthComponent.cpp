#include "Combat/KGHealthComponent.h"
#include "Net/UnrealNetwork.h"

UKGHealthComponent::UKGHealthComponent()
{
	// Ticks on the server only while wounded (second wind); see ApplyDamage / TickComponent.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.25f;
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
	SinceDamage = 0.0f;
	RegenCarry = 0.0f;
	SetComponentTickEnabled(!IsDead() && Health < RegenCap);
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

void UKGHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || IsDead() || Health >= RegenCap)
	{
		SetComponentTickEnabled(false);
		return;
	}
	SinceDamage += DeltaTime;
	const float Target = RegenStep(Health + RegenCarry, SinceDamage, DeltaTime, RegenDelay, RegenPerSecond, RegenCap);
	RegenCarry = Target - Health;
	if (RegenCarry >= 1.0f || Target >= RegenCap)
	{
		Heal(RegenCarry);
		RegenCarry = 0.0f;
	}
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
