#pragma once

#include "CoreMinimal.h"
#include "Core/KGTypes.h"

struct FKGRoleInfo;

/** Everything the role reveal card prints for one role, in one language. */
struct FKGRoleCardText
{
	FString Name;
	/** Sub-line under the alignment ribbon ("Town protective", "Clockbreakers", "Solo killer"...). */
	FString Team;
	FString Goal;
	/** One or two short ability lines (Docs/02_Roles.md, shortened). */
	TArray<FString> Abilities;
	/** Italic flavour line (Docs/Lore/KillGo_Lore.md section 2.4). */
	FString Flavour;
};

/**
 * Role card copy for the reveal ceremony (UI/Reveal/SKGRoleReveal) in English and Turkish. The Turkish lines follow
 * the lore doc; the String Table (ST_Lore, keys Role.<RoleId>.Flavour) replaces this table when localisation lands.
 */
namespace KGRoleCard
{
	/** Card text in the active UI language (tr -> Turkish, anything else -> English). */
	KILLGODOT_API FKGRoleCardText Get(FName RoleId);
	KILLGODOT_API FKGRoleCardText GetIn(FName RoleId, bool bTurkish);
	/** True when RoleId has hand-written card text (every catalog role must: tested). */
	KILLGODOT_API bool HasEntry(FName RoleId);

	KILLGODOT_API bool IsTurkish();
	/** EN / TR pick for UI chrome strings. */
	inline const TCHAR* Tr(const TCHAR* En, const TCHAR* TrText) { return IsTurkish() ? TrText : En; }

	KILLGODOT_API FString AlignmentName(EKGAlignment Alignment);
	/** Text colour of an alignment on dark ink (Town green, Impatient crimson, Neutral violet). */
	KILLGODOT_API FLinearColor AlignmentColor(EKGAlignment Alignment);

	/** Impatient factions that know each other from the start (Clockbreakers; vampires once there are several). */
	KILLGODOT_API bool IsTeamFaction(EKGFaction Faction);
}
