#pragma once

#include "CoreMinimal.h"
#include "Core/KGTypes.h"

struct FKGRoleInfo;

/** Everything the role reveal card prints for one role, in one language. */
struct FKGRoleCardText
{
	FString Name;
	/** The one plain-words "what you do" line of the reveal moment (<= 6 words, SPRINT-037). */
	FString Do;
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
	/** The big banner of the reveal moment: "YOU ARE IMPATIENT", "YOU WAIT WITH THE TOWN", "YOU PLAY YOUR OWN GAME". */
	KILLGODOT_API FString AlignmentBanner(EKGAlignment Alignment);
	KILLGODOT_API FString AlignmentBannerIn(EKGAlignment Alignment, bool bTurkish);
	/** Upper case that knows Turkish (i -> İ, ı -> I) and the Latin-1 / Turkish letters FString::ToUpper leaves alone. */
	KILLGODOT_API FString ToDisplayUpper(const FString& Text, bool bTurkish);
	/** Words in a line (runs of letters/digits/apostrophes; punctuation and dashes do not count). */
	KILLGODOT_API int32 CountWords(const FString& Text);
	/** Text colour of an alignment on dark ink (Town green, Impatient crimson, Neutral violet). */
	KILLGODOT_API FLinearColor AlignmentColor(EKGAlignment Alignment);

	/** Impatient factions that know each other from the start (Clockbreakers; vampires once there are several). */
	KILLGODOT_API bool IsTeamFaction(EKGFaction Faction);
}
