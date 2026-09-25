#pragma once

#include "CoreMinimal.h"
#include "KGTypes.generated.h"

/** Match flow, see Docs/01_GDD_Core.md §1. */
UENUM(BlueprintType)
enum class EKGPhase : uint8
{
	Lobby,
	Warmup,
	RoleReveal,
	Dawn,
	Day,
	Meeting,
	Trial,
	Night,
	Epilogue,
	Migrating
};

UENUM(BlueprintType)
enum class EKGAlignment : uint8
{
	Town,
	Impatient,
	Neutral
};

/** Who a role wins with. */
UENUM(BlueprintType)
enum class EKGFaction : uint8
{
	Town,
	Clockbreakers,
	SoloKiller,
	Vampires,
	Plague,
	Neutral
};

UENUM(BlueprintType)
enum class EKGRoleCategory : uint8
{
	TownInvestigative,
	TownSpiritual,
	TownProtective,
	TownKilling,
	TownSupport,
	ClockbreakerLeader,
	ClockbreakerKilling,
	ClockbreakerSupport,
	SoloKilling,
	NeutralEvil,
	NeutralBenign,
	NeutralChaos
};

UENUM(BlueprintType)
enum class EKGSheriffResult : uint8
{
	NotSuspicious,
	Suspicious
};

/** How an ability is used: from the night ledger (resolved at dawn), physically in the world, or passive. */
UENUM(BlueprintType)
enum class EKGUseMode : uint8
{
	Ledger,
	Physical,
	Passive,
	LedgerAndPhysical
};

/** Role list slots, see Docs/02_Roles.md "Rol listesi şablonları". */
UENUM(BlueprintType)
enum class EKGSlotKind : uint8
{
	TownInvestigative,
	TownProtective,
	TownKilling,
	TownSupport,
	RandomTown,
	Jailor,
	Clockmaster,
	Enforcer,
	RandomClockbreaker,
	SoloKiller,
	NeutralEvil,
	NeutralBenign,
	NeutralChaos,
	NeutralBenignOrChaos
};
