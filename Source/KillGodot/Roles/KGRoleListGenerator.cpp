#include "Roles/KGRoleListGenerator.h"

namespace KGRoles
{
	static FKGRoleInfo Make(const TCHAR* Id, EKGRoleCategory Category, EKGFaction Faction, int32 Power,
	                        bool bUnique = false, int32 MinPlayers = 0, TArray<FName> ExclusiveWith = {})
	{
		FKGRoleInfo Info;
		Info.RoleId = FName(Id);
		Info.Category = Category;
		Info.Faction = Faction;
		Info.Power = Power;
		Info.bUnique = bUnique;
		Info.MinPlayers = MinPlayers;
		Info.ExclusiveWith = MoveTemp(ExclusiveWith);
		return Info;
	}

	static const FName Jailor(TEXT("Jailor"));
	static const FName Enforcer(TEXT("Enforcer"));
}

const TArray<FKGRoleInfo>& FKGRoleListGenerator::GetDefaultCatalog()
{
	using C = EKGRoleCategory;
	using F = EKGFaction;
	using KGRoles::Make;

	static const TArray<FKGRoleInfo> Catalog = {
		// Town (22)
		Make(TEXT("Sheriff"), C::TownInvestigative, F::Town, 6),
		Make(TEXT("Investigator"), C::TownInvestigative, F::Town, 6),
		Make(TEXT("Lookout"), C::TownInvestigative, F::Town, 5),
		Make(TEXT("Tracker"), C::TownInvestigative, F::Town, 5),
		Make(TEXT("Coroner"), C::TownInvestigative, F::Town, 5),
		Make(TEXT("Eavesdropper"), C::TownInvestigative, F::Town, 5),
		Make(TEXT("Medium"), C::TownSpiritual, F::Town, 4),
		Make(TEXT("Seer"), C::TownSpiritual, F::Town, 6),
		Make(TEXT("Doctor"), C::TownProtective, F::Town, 6),
		Make(TEXT("Bodyguard"), C::TownProtective, F::Town, 6),
		Make(TEXT("Watchman"), C::TownProtective, F::Town, 7),
		Make(TEXT("Priest"), C::TownProtective, F::Town, 6),
		Make(TEXT("Locksmith"), C::TownProtective, F::Town, 5),
		Make(TEXT("Vigilante"), C::TownKilling, F::Town, 6),
		Make(TEXT("Veteran"), C::TownKilling, F::Town, 6),
		Make(TEXT("Hunter"), C::TownKilling, F::Town, 5),
		Make(TEXT("Mayor"), C::TownSupport, F::Town, 8, true),
		Make(TEXT("Jailor"), C::TownSupport, F::Town, 9, true),
		Make(TEXT("TavernKeeper"), C::TownSupport, F::Town, 5),
		Make(TEXT("BellRinger"), C::TownSupport, F::Town, 6, true),
		Make(TEXT("Shepherd"), C::TownSupport, F::Town, 6),
		Make(TEXT("PowderMaster"), C::TownSupport, F::Town, 5),
		// Clockbreakers (13)
		Make(TEXT("Clockmaster"), C::ClockbreakerLeader, F::Clockbreakers, 9, true),
		Make(TEXT("Enforcer"), C::ClockbreakerKilling, F::Clockbreakers, 7),
		Make(TEXT("Informant"), C::ClockbreakerSupport, F::Clockbreakers, 6),
		Make(TEXT("Framer"), C::ClockbreakerSupport, F::Clockbreakers, 5),
		Make(TEXT("Cleaner"), C::ClockbreakerSupport, F::Clockbreakers, 5),
		Make(TEXT("Blackmailer"), C::ClockbreakerSupport, F::Clockbreakers, 6),
		Make(TEXT("Forger"), C::ClockbreakerSupport, F::Clockbreakers, 4),
		Make(TEXT("Spy"), C::ClockbreakerKilling, F::Clockbreakers, 7, true, 0, {FName(TEXT("Doppelganger"))}),
		Make(TEXT("Charmer"), C::ClockbreakerSupport, F::Clockbreakers, 5),
		Make(TEXT("Saboteur"), C::ClockbreakerSupport, F::Clockbreakers, 5),
		Make(TEXT("Smuggler"), C::ClockbreakerSupport, F::Clockbreakers, 5),
		Make(TEXT("Ambusher"), C::ClockbreakerKilling, F::Clockbreakers, 6),
		Make(TEXT("Bomber"), C::ClockbreakerKilling, F::Clockbreakers, 6, true, 10),
		// Solo killers & converting factions (8)
		Make(TEXT("SerialKiller"), C::SoloKilling, F::SoloKiller, 8, true),
		Make(TEXT("Werewolf"), C::SoloKilling, F::SoloKiller, 8, true),
		Make(TEXT("Arsonist"), C::SoloKilling, F::SoloKiller, 7, true),
		Make(TEXT("Poisoner"), C::SoloKilling, F::SoloKiller, 6, true),
		Make(TEXT("Vampire"), C::SoloKilling, F::Vampires, 7, true, 12),
		Make(TEXT("Plaguebearer"), C::SoloKilling, F::Plague, 7, true, 14),
		Make(TEXT("Drowned"), C::SoloKilling, F::SoloKiller, 7, true),
		Make(TEXT("Doppelganger"), C::SoloKilling, F::SoloKiller, 8, true, 0, {FName(TEXT("Spy"))}),
		// Neutrals (8)
		Make(TEXT("Fool"), C::NeutralEvil, F::Neutral, 3, true),
		Make(TEXT("Executioner"), C::NeutralEvil, F::Neutral, 4, true),
		Make(TEXT("Witch"), C::NeutralEvil, F::Neutral, 5, true),
		Make(TEXT("Survivor"), C::NeutralBenign, F::Neutral, 0),
		Make(TEXT("Amnesiac"), C::NeutralBenign, F::Neutral, 2),
		Make(TEXT("Pozzo"), C::NeutralBenign, F::Neutral, 1, true),
		Make(TEXT("Collector"), C::NeutralChaos, F::Neutral, 2, true),
		Make(TEXT("Pirate"), C::NeutralChaos, F::Neutral, 4, true),
	};
	return Catalog;
}

FKGFactionCounts FKGRoleListGenerator::GetFactionCounts(int32 NumPlayers)
{
	// Index = players - 6. Columns: Town, Impatient, Neutral, of which solo killers.
	static const int32 Table[][4] = {
		{4, 1, 1, 0},  // 6
		{5, 1, 1, 0},  // 7
		{5, 2, 1, 0},  // 8
		{6, 2, 1, 0},  // 9
		{6, 3, 1, 1},  // 10
		{7, 3, 1, 1},  // 11
		{7, 3, 2, 1},  // 12
		{8, 3, 2, 1},  // 13
		{8, 4, 2, 1},  // 14
		{9, 4, 2, 1},  // 15
		{9, 4, 3, 1},  // 16
		{10, 4, 3, 1}, // 17
		{10, 5, 3, 2}, // 18
		{11, 5, 3, 2}, // 19
		{12, 5, 3, 2}, // 20
	};
	const int32 Index = FMath::Clamp(NumPlayers, MinPlayers, MaxPlayers) - MinPlayers;
	FKGFactionCounts Counts;
	Counts.Town = Table[Index][0];
	Counts.Impatient = Table[Index][1];
	Counts.Neutral = Table[Index][2];
	Counts.SoloKillers = Table[Index][3];
	return Counts;
}

TArray<EKGSlotKind> FKGRoleListGenerator::BuildSlots(int32 NumPlayers)
{
	using S = EKGSlotKind;
	const int32 N = FMath::Clamp(NumPlayers, MinPlayers, MaxPlayers);
	const FKGFactionCounts Counts = GetFactionCounts(N);
	TArray<EKGSlotKind> Slots;

	// Town: fixed categories first (Jailor from 12 players), then 2-3 random town slots.
	static const S LargeOrder[] = {S::Jailor, S::TownInvestigative, S::TownProtective, S::TownInvestigative,
	                               S::TownKilling, S::TownSupport, S::TownInvestigative, S::TownProtective,
	                               S::TownSupport, S::TownKilling, S::TownProtective, S::TownSupport};
	static const S SmallOrder[] = {S::TownInvestigative, S::TownProtective, S::TownSupport, S::TownInvestigative,
	                               S::TownKilling, S::TownProtective, S::TownSupport, S::TownInvestigative};
	const int32 RandomTown = Counts.Town < 9 ? 2 : 3;
	const int32 FixedTown = Counts.Town - RandomTown;
	const S* Order = N >= 12 ? LargeOrder : SmallOrder;
	const int32 OrderLen = N >= 12 ? UE_ARRAY_COUNT(LargeOrder) : UE_ARRAY_COUNT(SmallOrder);
	for (int32 i = 0; i < FixedTown; ++i)
	{
		Slots.Add(Order[i % OrderLen]);
	}
	for (int32 i = 0; i < RandomTown; ++i)
	{
		Slots.Add(S::RandomTown);
	}

	// Impatient: Clockmaster, Enforcer, then random Clockbreakers; solo killers on top.
	const int32 Clockbreakers = Counts.Impatient - Counts.SoloKillers;
	for (int32 i = 0; i < Clockbreakers; ++i)
	{
		Slots.Add(i == 0 ? S::Clockmaster : (i == 1 ? S::Enforcer : S::RandomClockbreaker));
	}
	for (int32 i = 0; i < Counts.SoloKillers; ++i)
	{
		Slots.Add(S::SoloKiller);
	}

	// Neutrals
	if (Counts.Neutral == 1)
	{
		Slots.Add(S::NeutralEvil);
	}
	else if (Counts.Neutral == 2)
	{
		Slots.Append({S::NeutralEvil, S::NeutralBenignOrChaos});
	}
	else if (Counts.Neutral >= 3)
	{
		Slots.Append({S::NeutralEvil, S::NeutralBenign, S::NeutralChaos});
	}
	return Slots;
}

bool FKGRoleListGenerator::SlotAccepts(EKGSlotKind Slot, const FKGRoleInfo& Role)
{
	using S = EKGSlotKind;
	using C = EKGRoleCategory;
	switch (Slot)
	{
	case S::TownInvestigative:
		return Role.Category == C::TownInvestigative || Role.Category == C::TownSpiritual;
	case S::TownProtective:
		return Role.Category == C::TownProtective;
	case S::TownKilling:
		return Role.Category == C::TownKilling;
	case S::TownSupport:
		return Role.Category == C::TownSupport && Role.RoleId != KGRoles::Jailor;
	case S::RandomTown:
		return Role.Faction == EKGFaction::Town && Role.RoleId != KGRoles::Jailor;
	case S::Jailor:
		return Role.RoleId == KGRoles::Jailor;
	case S::Clockmaster:
		return Role.Category == C::ClockbreakerLeader;
	case S::Enforcer:
		return Role.RoleId == KGRoles::Enforcer;
	case S::RandomClockbreaker:
		return Role.Category == C::ClockbreakerKilling || Role.Category == C::ClockbreakerSupport;
	case S::SoloKiller:
		return Role.Category == C::SoloKilling;
	case S::NeutralEvil:
		return Role.Category == C::NeutralEvil;
	case S::NeutralBenign:
		return Role.Category == C::NeutralBenign;
	case S::NeutralChaos:
		return Role.Category == C::NeutralChaos;
	case S::NeutralBenignOrChaos:
		return Role.Category == C::NeutralBenign || Role.Category == C::NeutralChaos;
	}
	return false;
}

const FKGRoleInfo* FKGRoleListGenerator::FindRole(const TArray<FKGRoleInfo>& Catalog, FName RoleId)
{
	return Catalog.FindByPredicate([RoleId](const FKGRoleInfo& R) { return R.RoleId == RoleId; });
}

int32 FKGRoleListGenerator::ComputeBalance(const TArray<FName>& Roles, const TArray<FKGRoleInfo>& Catalog)
{
	int32 Balance = 0;
	for (const FName& Id : Roles)
	{
		if (const FKGRoleInfo* Role = FindRole(Catalog, Id))
		{
			Balance += Role->GetAlignment() == EKGAlignment::Town ? Role->Power : -Role->Power;
		}
	}
	return Balance;
}

void FKGRoleListGenerator::GetBalanceBand(int32 NumPlayers, int32& OutMin, int32& OutMax)
{
	// First guess (Docs/02_Roles.md "Denge notları"); to be re-fit from match telemetry.
	const int32 N = FMath::Clamp(NumPlayers, MinPlayers, MaxPlayers);
	const int32 Center = FMath::RoundToInt(1.35f * N + 1.5f);
	const int32 HalfWidth = 5 + N / 5;
	OutMin = Center - HalfWidth;
	OutMax = Center + HalfWidth;
}

bool FKGRoleListGenerator::TryFill(int32 NumPlayers, const TArray<EKGSlotKind>& Slots, FKGRng& Rng,
                                   const TArray<FKGRoleInfo>& Catalog, TArray<FName>& OutRoles)
{
	OutRoles.Init(NAME_None, Slots.Num());

	// Most specific slots first, so random slots can't steal a unique role a fixed slot needs.
	auto Specificity = [](EKGSlotKind Slot)
	{
		switch (Slot)
		{
		case EKGSlotKind::Jailor:
		case EKGSlotKind::Clockmaster:
		case EKGSlotKind::Enforcer:
			return 0;
		case EKGSlotKind::RandomTown:
		case EKGSlotKind::RandomClockbreaker:
		case EKGSlotKind::NeutralBenignOrChaos:
			return 2;
		default:
			return 1;
		}
	};
	TArray<int32> FillOrder;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		FillOrder.Add(i);
	}
	FillOrder.StableSort([&](int32 A, int32 B) { return Specificity(Slots[A]) < Specificity(Slots[B]); });

	TArray<FName> Used;
	for (const int32 SlotIndex : FillOrder)
	{
		TArray<const FKGRoleInfo*> Fresh;
		TArray<const FKGRoleInfo*> Repeats;
		for (const FKGRoleInfo& Role : Catalog)
		{
			if (!SlotAccepts(Slots[SlotIndex], Role) || Role.MinPlayers > NumPlayers)
			{
				continue;
			}
			const bool bAlreadyUsed = Used.Contains(Role.RoleId);
			if (bAlreadyUsed && Role.bUnique)
			{
				continue;
			}
			const bool bConflicts = Role.ExclusiveWith.ContainsByPredicate(
				[&Used](const FName& Other) { return Used.Contains(Other); });
			if (bConflicts)
			{
				continue;
			}
			(bAlreadyUsed ? Repeats : Fresh).Add(&Role);
		}
		const TArray<const FKGRoleInfo*>& Pool = Fresh.Num() > 0 ? Fresh : Repeats;
		if (Pool.Num() == 0)
		{
			return false;
		}
		const FKGRoleInfo* Pick = Pool[Rng.RandRange(0, Pool.Num() - 1)];
		OutRoles[SlotIndex] = Pick->RoleId;
		Used.Add(Pick->RoleId);
	}
	return true;
}

FKGRoleListResult FKGRoleListGenerator::Generate(int32 NumPlayers, FKGRng& Rng, const TArray<FKGRoleInfo>& Catalog)
{
	const int32 N = FMath::Clamp(NumPlayers, MinPlayers, MaxPlayers);
	int32 BandMin = 0;
	int32 BandMax = 0;
	GetBalanceBand(N, BandMin, BandMax);
	const int32 Center = (BandMin + BandMax) / 2;

	FKGRoleListResult Best;
	Best.Slots = BuildSlots(N);
	int32 BestDistance = MAX_int32;

	for (int32 Attempt = 0; Attempt < MaxRerolls; ++Attempt)
	{
		TArray<FName> Roles;
		if (!TryFill(N, Best.Slots, Rng, Catalog, Roles))
		{
			continue;
		}
		const int32 Balance = ComputeBalance(Roles, Catalog);
		const bool bWithin = Balance >= BandMin && Balance <= BandMax;
		const int32 Distance = FMath::Abs(Balance - Center);
		if (bWithin || Distance < BestDistance)
		{
			Best.Roles = MoveTemp(Roles);
			Best.Balance = Balance;
			Best.bWithinBand = bWithin;
			BestDistance = Distance;
		}
		if (bWithin)
		{
			break;
		}
	}
	return Best;
}
