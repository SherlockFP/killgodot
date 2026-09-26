#include "Abilities/KGAbilityTypes.h"
#include "Roles/KGRoleListGenerator.h"

uint8 KGAbilityPhase::BitOf(EKGPhase Phase)
{
	switch (Phase)
	{
	case EKGPhase::Day: return Day;
	case EKGPhase::Night: return Night;
	case EKGPhase::Dawn: return Dawn;
	case EKGPhase::Meeting:
	case EKGPhase::Trial: return Meeting;
	default: return 0;   // lobby, warmup, reveal, epilogue, migration: never
	}
}

EKGAbilityDeny FKGAbilityRules::Validate(const FKGAbilityDef& Def, const FKGAbilityState& State, const FKGAbilityQuery& Q)
{
	if (Def.AbilityId.IsNone() || State.AbilityId != Def.AbilityId)
	{
		return EKGAbilityDeny::Unknown;
	}
	if (!Q.bAlive)
	{
		return EKGAbilityDeny::Dead;
	}
	if ((KGAbilityPhase::BitOf(Q.Phase) & Def.PhaseMask) == 0)
	{
		return EKGAbilityDeny::WrongPhase;
	}
	if (State.Charges <= 0)
	{
		return EKGAbilityDeny::NoCharges;
	}
	if (State.Cooldown.RemainingSeconds > 0.0f)
	{
		return EKGAbilityDeny::Cooldown;
	}
	if (Def.Target != EKGAbilityTarget::Self)
	{
		if (!Q.bHasTarget)
		{
			return EKGAbilityDeny::NoTarget;
		}
		if (Q.TargetDistanceCm > Def.RangeCm)
		{
			return EKGAbilityDeny::OutOfRange;
		}
	}
	if (Def.bRequiresUnseen && Q.bSeen)
	{
		return EKGAbilityDeny::Seen;
	}
	return EKGAbilityDeny::None;
}

void FKGAbilityRules::Commit(const FKGAbilityDef& Def, FKGAbilityState& State)
{
	State.Charges = FMath::Max(0, State.Charges - 1);
	State.Cooldown.Start(Def.CooldownSecs);
	++State.Uses;
}

void FKGAbilityRules::Refill(const FKGAbilityDef& Def, FKGAbilityState& State)
{
	State.AbilityId = Def.AbilityId;
	State.Charges = Def.ChargesPerCycle;
	State.Cooldown = FKGMatchClock();
}

const TCHAR* FKGAbilityRules::DenyName(EKGAbilityDeny Deny)
{
	switch (Deny)
	{
	case EKGAbilityDeny::None: return TEXT("None");
	case EKGAbilityDeny::Dead: return TEXT("Dead");
	case EKGAbilityDeny::WrongPhase: return TEXT("WrongPhase");
	case EKGAbilityDeny::NoCharges: return TEXT("NoCharges");
	case EKGAbilityDeny::Cooldown: return TEXT("Cooldown");
	case EKGAbilityDeny::Seen: return TEXT("Seen");
	case EKGAbilityDeny::NoTarget: return TEXT("NoTarget");
	case EKGAbilityDeny::OutOfRange: return TEXT("OutOfRange");
	default: return TEXT("Unknown");
	}
}

FString FKGAbilityRules::DenyText(EKGAbilityDeny Deny, bool bTurkish)
{
	switch (Deny)
	{
	case EKGAbilityDeny::Dead: return bTurkish ? TEXT("Ölüler tuzak kuramaz.") : TEXT("The dead set no traps.");
	case EKGAbilityDeny::WrongPhase: return bTurkish ? TEXT("Şimdi olmaz.") : TEXT("Not now.");
	case EKGAbilityDeny::NoCharges: return bTurkish ? TEXT("Bu gece hakkın bitti.") : TEXT("No charges left until dawn.");
	case EKGAbilityDeny::Cooldown: return bTurkish ? TEXT("Biraz bekle.") : TEXT("Still recharging.");
	case EKGAbilityDeny::Seen: return bTurkish ? TEXT("Biri seni görüyor.") : TEXT("Someone can see you.");
	case EKGAbilityDeny::NoTarget: return bTurkish ? TEXT("Hedef yok.") : TEXT("Nothing to aim at.");
	case EKGAbilityDeny::OutOfRange: return bTurkish ? TEXT("Çok uzak.") : TEXT("Too far away.");
	case EKGAbilityDeny::None: return FString();
	default: return bTurkish ? TEXT("Bilinmeyen yetenek.") : TEXT("Unknown ability.");
	}
}

const TArray<FKGAbilityDef>& FKGAbilityCatalog::GetAll()
{
	auto Make = [](const TCHAR* Id, int32 Charges, float Cooldown, uint8 Phases, bool bUnseen, EKGAbilityTarget Target,
	               float Range, int32 Slot)
	{
		FKGAbilityDef D;
		D.AbilityId = FName(Id);
		D.ChargesPerCycle = Charges;
		D.CooldownSecs = Cooldown;
		D.PhaseMask = Phases;
		D.bRequiresUnseen = bUnseen;
		D.Target = Target;
		D.RangeCm = Range;
		D.Slot = Slot;
		return D;
	};
	// Docs/02_Roles.md "Tuzakçı" holds the same numbers (keep them in sync).
	static const TArray<FKGAbilityDef> All = {
		Make(TEXT("Mimic"), 2, 20.0f, KGAbilityPhase::DayAndNight, true, EKGAbilityTarget::Container, 320.0f, 1),
		Make(TEXT("Snare"), 2, 30.0f, KGAbilityPhase::DayAndNight, false, EKGAbilityTarget::Ground, 350.0f, 2),
		Make(TEXT("Tripwire"), 2, 10.0f, KGAbilityPhase::DayAndNight, false, EKGAbilityTarget::Ground, 350.0f, 3),
	};
	return All;
}

const FKGAbilityDef* FKGAbilityCatalog::Find(FName AbilityId)
{
	return GetAll().FindByPredicate([AbilityId](const FKGAbilityDef& D) { return D.AbilityId == AbilityId; });
}

TArray<const FKGAbilityDef*> FKGAbilityCatalog::ForRole(FName RoleId)
{
	TArray<const FKGAbilityDef*> Out;
	if (const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId))
	{
		for (const FName& Id : Role->AbilityIds)
		{
			if (const FKGAbilityDef* D = Find(Id))
			{
				Out.Add(D);
			}
		}
	}
	return Out;
}

FString FKGAbilityCatalog::DisplayName(FName AbilityId, bool bTurkish)
{
	if (AbilityId == TEXT("Mimic")) { return bTurkish ? TEXT("Mimik") : TEXT("Mimic"); }
	if (AbilityId == TEXT("Snare")) { return bTurkish ? TEXT("Kapan") : TEXT("Snare"); }
	if (AbilityId == TEXT("Tripwire")) { return bTurkish ? TEXT("Tel") : TEXT("Tripwire"); }
	return AbilityId.ToString();
}

FString FKGAbilityCatalog::TargetHint(FName AbilityId, bool bTurkish)
{
	if (AbilityId == TEXT("Mimic"))
	{
		return bTurkish ? TEXT("Bir sandığa, kasaya ya da fıçıya bak") : TEXT("Look at a chest, crate or barrel");
	}
	if (AbilityId == TEXT("Snare"))
	{
		return bTurkish ? TEXT("Kapanı kuracağın yere bak") : TEXT("Look at the ground where the snare goes");
	}
	if (AbilityId == TEXT("Tripwire"))
	{
		return bTurkish ? TEXT("Teli gereceğin yolu gözle") : TEXT("Look across the path to string the wire");
	}
	return FString();
}
