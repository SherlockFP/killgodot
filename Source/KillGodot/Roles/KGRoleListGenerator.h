#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "Core/KGTypes.h"
#include "Roles/KGRoleDefinition.h"

/** Faction head-count for a lobby size (Docs/01_GDD_Core.md §3). */
struct KILLGODOT_API FKGFactionCounts
{
	int32 Town = 0;
	int32 Impatient = 0;
	int32 Neutral = 0;
	/** How many of the Impatient slots are solo killers (the rest are Clockbreakers). */
	int32 SoloKillers = 0;
};

struct KILLGODOT_API FKGRoleListResult
{
	TArray<FName> Roles;
	TArray<EKGSlotKind> Slots;
	int32 Balance = 0;
	bool bWithinBand = false;
};

/**
 * Builds a balanced role list for 6..20 players. Pure logic, deterministic for a given seed, no UObjects needed,
 * so it is unit-tested directly (Private/Tests/KGRoleListGeneratorTests.cpp).
 */
class KILLGODOT_API FKGRoleListGenerator
{
public:
	static constexpr int32 MinPlayers = 6;
	static constexpr int32 MaxPlayers = 20;
	static constexpr int32 MaxRerolls = 20;

	/** The default 51-role catalogue mirroring Docs/02_Roles.md. */
	static const TArray<FKGRoleInfo>& GetDefaultCatalog();

	static FKGFactionCounts GetFactionCounts(int32 NumPlayers);
	static TArray<EKGSlotKind> BuildSlots(int32 NumPlayers);

	/** Town power - Impatient power - Neutral threat. */
	static int32 ComputeBalance(const TArray<FName>& Roles, const TArray<FKGRoleInfo>& Catalog);
	static void GetBalanceBand(int32 NumPlayers, int32& OutMin, int32& OutMax);

	static FKGRoleListResult Generate(int32 NumPlayers, FKGRng& Rng, const TArray<FKGRoleInfo>& Catalog);

	static const FKGRoleInfo* FindRole(const TArray<FKGRoleInfo>& Catalog, FName RoleId);

private:
	static bool SlotAccepts(EKGSlotKind Slot, const FKGRoleInfo& Role);
	static bool TryFill(int32 NumPlayers, const TArray<EKGSlotKind>& Slots, FKGRng& Rng,
	                    const TArray<FKGRoleInfo>& Catalog, TArray<FName>& OutRoles);
};
