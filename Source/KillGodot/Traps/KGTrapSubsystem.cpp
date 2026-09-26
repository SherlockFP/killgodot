#include "Traps/KGTrapSubsystem.h"
#include "Character/KGCharacter.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "KillGodot.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "Traps/KGTrap.h"

FKGTrapArmPolicies::FKGTrapArmPolicies()
{
	Register(TEXT("None"), [](const AKGCharacter*) { return false; });
	Register(TEXT("Anyone"), [](const AKGCharacter* Who) { return Who != nullptr; });
	Register(TEXT("Impatient"), [](const AKGCharacter* Who) { return IsImpatient(Who); });
}

bool FKGTrapArmPolicies::CanArm(FName Policy, const AKGCharacter* Who) const
{
	if (Policy.IsNone())
	{
		return false;
	}
	const FPolicy* P = Policies.Find(Policy);
	return P && (*P) && (*P)(Who);
}

bool FKGTrapArmPolicies::IsImpatient(const AKGCharacter* Who)
{
	const AKGPlayerState* PS = Who ? Who->GetPlayerState<AKGPlayerState>() : nullptr;
	const FKGRoleInfo* Role = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId())
	                             : nullptr;
	return Role && Role->GetAlignment() == EKGAlignment::Impatient;
}

UKGTrapSubsystem* UKGTrapSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGTrapSubsystem>() : nullptr;
}

bool UKGTrapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKGTrapSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGTrapSubsystem, STATGROUP_Tickables);
}

void UKGTrapSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		Cooldowns.Advance(DeltaTime);
	}
}

void UKGTrapSubsystem::RegisterTrap(AKGTrap* Trap)
{
	if (Trap)
	{
		Traps.AddUnique(Trap);
	}
}

void UKGTrapSubsystem::UnregisterTrap(AKGTrap* Trap)
{
	Traps.RemoveAll([Trap](const TWeakObjectPtr<AKGTrap>& T) { return !T.IsValid() || T.Get() == Trap; });
}

AKGTrap* UKGTrapSubsystem::FindTrap(FName TrapId) const
{
	for (const TWeakObjectPtr<AKGTrap>& T : Traps)
	{
		if (T.IsValid() && T->TrapId == TrapId)
		{
			return T.Get();
		}
	}
	return nullptr;
}

void UKGTrapSubsystem::Record(const FKGTrapEvent& Event)
{
	Log.Add(Event);
	UE_LOG(LogKillGodot, Log, TEXT("KG_TRAP %s %s kind=%s room=%s by=%s"), *Event.Type.ToString(), *Event.TrapId.ToString(),
	       *Event.Kind.ToString(), *Event.Room.ToString(), Event.ByName.IsEmpty() ? TEXT("-") : *Event.ByName);
	OnTrapEvent.Broadcast(Event);
}
