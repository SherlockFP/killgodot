#include "Misc/AutomationTest.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGWoundComponent.h"
#include "Core/KGPlayerState.h"
#include "Core/KGRng.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleListGenerator.h"
#include "Traps/KGFieldTraps.h"
#include "Traps/KGMimicTrap.h"
#include "UI/Reveal/KGRoleCardText.h"

#if WITH_DEV_AUTOMATION_TESTS

// SPRINT-041 acceptance 1: the validation rules of the role-ability framework.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGAbilityRulesTest, "KillGodot.Abilities.Rules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGAbilityRulesTest::RunTest(const FString& Parameters)
{
	const FKGAbilityDef* Mimic = FKGAbilityCatalog::Find(TEXT("Mimic"));
	if (!TestNotNull(TEXT("Mimic in the catalog"), Mimic))
	{
		return false;
	}
	FKGAbilityState S;
	FKGAbilityRules::Refill(*Mimic, S);
	TestEqual(TEXT("Refill gives the per-night charges"), S.Charges, 2);

	FKGAbilityQuery Q;
	Q.Phase = EKGPhase::Night;
	Q.bHasTarget = true;
	Q.TargetDistanceCm = 150.0f;
	TestEqual(TEXT("Valid use"), FKGAbilityRules::Validate(*Mimic, S, Q), EKGAbilityDeny::None);

	FKGAbilityQuery Dead = Q;
	Dead.bAlive = false;
	TestEqual(TEXT("The dead are refused first"), FKGAbilityRules::Validate(*Mimic, S, Dead), EKGAbilityDeny::Dead);
	for (const EKGPhase P : {EKGPhase::Lobby, EKGPhase::Warmup, EKGPhase::RoleReveal, EKGPhase::Meeting, EKGPhase::Trial, EKGPhase::Epilogue})
	{
		FKGAbilityQuery X = Q;
		X.Phase = P;
		TestEqual(*FString::Printf(TEXT("Phase %s refused"), *UEnum::GetValueAsString(P)), FKGAbilityRules::Validate(*Mimic, S, X), EKGAbilityDeny::WrongPhase);
	}
	FKGAbilityQuery Day = Q;
	Day.Phase = EKGPhase::Day;
	TestEqual(TEXT("Day is allowed"), FKGAbilityRules::Validate(*Mimic, S, Day), EKGAbilityDeny::None);

	FKGAbilityQuery NoTarget = Q;
	NoTarget.bHasTarget = false;
	TestEqual(TEXT("No container"), FKGAbilityRules::Validate(*Mimic, S, NoTarget), EKGAbilityDeny::NoTarget);
	FKGAbilityQuery Far = Q;
	Far.TargetDistanceCm = Mimic->RangeCm + 1.0f;
	TestEqual(TEXT("Out of range"), FKGAbilityRules::Validate(*Mimic, S, Far), EKGAbilityDeny::OutOfRange);
	FKGAbilityQuery Seen = Q;
	Seen.bSeen = true;
	TestEqual(TEXT("Mimic needs to be unseen"), FKGAbilityRules::Validate(*Mimic, S, Seen), EKGAbilityDeny::Seen);
	const FKGAbilityDef* Snare = FKGAbilityCatalog::Find(TEXT("Snare"));
	FKGAbilityState SS;
	FKGAbilityRules::Refill(*Snare, SS);
	TestEqual(TEXT("Snare may be set in sight"), FKGAbilityRules::Validate(*Snare, SS, Seen), EKGAbilityDeny::None);
	TestEqual(TEXT("A state of another ability is refused"), FKGAbilityRules::Validate(*Snare, S, Q), EKGAbilityDeny::Unknown);

	FKGAbilityRules::Commit(*Mimic, S);
	TestEqual(TEXT("A use spends a charge"), S.Charges, 1);
	TestEqual(TEXT("A use starts the cooldown"), FKGAbilityRules::Validate(*Mimic, S, Q), EKGAbilityDeny::Cooldown);
	S.Cooldown.Advance(Mimic->CooldownSecs + 0.1f);
	TestEqual(TEXT("Cooldown over"), FKGAbilityRules::Validate(*Mimic, S, Q), EKGAbilityDeny::None);
	FKGAbilityRules::Commit(*Mimic, S);
	S.Cooldown.Advance(Mimic->CooldownSecs + 0.1f);
	TestEqual(TEXT("2 charges per night"), FKGAbilityRules::Validate(*Mimic, S, Q), EKGAbilityDeny::NoCharges);
	FKGAbilityRules::Refill(*Mimic, S);
	TestEqual(TEXT("Dawn refills"), FKGAbilityRules::Validate(*Mimic, S, Q), EKGAbilityDeny::None);
	TestEqual(TEXT("Uses are counted"), S.Uses, 2);
	for (uint8 D = 0; D <= static_cast<uint8>(EKGAbilityDeny::Unknown); ++D)
	{
		const EKGAbilityDeny Deny = static_cast<EKGAbilityDeny>(D);
		TestTrue(TEXT("Every denial has a player line in both languages"),
		         Deny == EKGAbilityDeny::None || (!FKGAbilityRules::DenyText(Deny, false).IsEmpty() && !FKGAbilityRules::DenyText(Deny, true).IsEmpty()));
	}
	return true;
}

// Acceptance 1 + 2: data-driven from the role catalog; the Trapper and its card.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGAbilityCatalogTest, "KillGodot.Abilities.Catalog",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGAbilityCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FKGRoleInfo>& Roles = FKGRoleListGenerator::GetDefaultCatalog();
	for (const FKGRoleInfo& R : Roles)
	{
		for (const FName& Id : R.AbilityIds)
		{
			TestNotNull(*FString::Printf(TEXT("%s ability %s exists"), *R.RoleId.ToString(), *Id.ToString()), FKGAbilityCatalog::Find(Id));
		}
	}
	const FKGRoleInfo* Trapper = FKGRoleListGenerator::FindRole(Roles, TEXT("Trapper"));
	if (!TestNotNull(TEXT("Trapper in the role catalog"), Trapper))
	{
		return false;
	}
	TestEqual(TEXT("Trapper is Impatient"), Trapper->GetAlignment(), EKGAlignment::Impatient);
	TestTrue(TEXT("Trapper is unique"), Trapper->bUnique);
	const TArray<const FKGAbilityDef*> Defs = FKGAbilityCatalog::ForRole(TEXT("Trapper"));
	if (TestEqual(TEXT("Trapper has 3 abilities"), Defs.Num(), 3))
	{
		TestEqual(TEXT("Bar order 1"), Defs[0]->AbilityId, FName(TEXT("Mimic")));
		TestEqual(TEXT("Bar order 2"), Defs[1]->AbilityId, FName(TEXT("Snare")));
		TestEqual(TEXT("Bar order 3"), Defs[2]->AbilityId, FName(TEXT("Tripwire")));
		TestTrue(TEXT("Mimic targets containers, unseen"), Defs[0]->Target == EKGAbilityTarget::Container && Defs[0]->bRequiresUnseen);
		TestTrue(TEXT("Snare/Tripwire target the ground"), Defs[1]->Target == EKGAbilityTarget::Ground && Defs[2]->Target == EKGAbilityTarget::Ground);
	}
	TestEqual(TEXT("A role without abilities gets none"), FKGAbilityCatalog::ForRole(TEXT("Sheriff")).Num(), 0);
	TestTrue(TEXT("Trapper card text exists"), KGRoleCard::HasEntry(TEXT("Trapper")));
	for (const bool bTr : {false, true})
	{
		const FKGRoleCardText Card = KGRoleCard::GetIn(TEXT("Trapper"), bTr);
		TestTrue(TEXT("Trapper do-line <= 6 words"), KGRoleCard::CountWords(Card.Do) >= 2 && KGRoleCard::CountWords(Card.Do) <= 6);
		for (const FString& L : Card.Abilities)
		{
			TestTrue(*FString::Printf(TEXT("Ability line '%s' <= 12 words"), *L), KGRoleCard::CountWords(L) <= 12);
		}
		TestFalse(TEXT("Trapper flavour line"), Card.Flavour.IsEmpty());
		TestFalse(TEXT("Wound lines differ"), UKGWoundComponent::Describe(UKGWoundComponent::BiteMarks, bTr) ==
		                                          UKGWoundComponent::Describe(UKGWoundComponent::SnareWound, bTr));
	}
	return true;
}

// Acceptance 1: no role data leaks to other clients (the replication setup, checked through reflection).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGAbilityNoLeakTest, "KillGodot.Abilities.NoLeak",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGAbilityNoLeakTest::RunTest(const FString& Parameters)
{
	auto CheckOwnerOnly = [this](UClass* Class, const TCHAR* Declared, TSet<FName> MustBeOwnerOnly)
	{
		Class->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject<AActor>()->GetLifetimeReplicatedProps(Props);
		for (const FLifetimeProperty& P : Props)
		{
			if (!Class->ClassReps.IsValidIndex(P.RepIndex))
			{
				continue;
			}
			const FProperty* Prop = Class->ClassReps[P.RepIndex].Property;
			if (Prop && MustBeOwnerOnly.Contains(Prop->GetFName()))
			{
				TestEqual(*FString::Printf(TEXT("%s::%s is owner-only"), Declared, *Prop->GetName()), static_cast<int32>(P.Condition),
				          static_cast<int32>(COND_OwnerOnly));
				MustBeOwnerOnly.Remove(Prop->GetFName());
			}
		}
		TestEqual(*FString::Printf(TEXT("%s: every secret property is replicated with a condition"), Declared), MustBeOwnerOnly.Num(), 0);
	};
	CheckOwnerOnly(AKGAbilityHolder::StaticClass(), TEXT("AKGAbilityHolder"), {TEXT("RoleId"), TEXT("States"), TEXT("MyTraps")});
	TestTrue(TEXT("The holder is only relevant to its owner"), GetDefault<AKGAbilityHolder>()->bOnlyRelevantToOwner);
	TestFalse(TEXT("The holder is never always-relevant"), GetDefault<AKGAbilityHolder>()->bAlwaysRelevant);

	// Who armed a trap is a server secret on every trap class (not replicated at all).
	const FProperty* Armer = FindFProperty<FProperty>(AKGTrap::StaticClass(), TEXT("ArmedByPuid"));
	if (TestNotNull(TEXT("AKGTrap::ArmedByPuid"), Armer))
	{
		TestFalse(TEXT("ArmedByPuid is not replicated"), Armer->HasAnyPropertyFlags(CPF_Net));
	}
	for (UClass* C : {AKGMimicTrap::StaticClass(), AKGSnareTrap::StaticClass(), AKGTripwireTrap::StaticClass()})
	{
		for (TFieldIterator<FProperty> It(C); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Net) && It->GetOwnerClass()->IsChildOf(AKGTrap::StaticClass()))   // AActor::Role is engine net role
			{
				const FString N = It->GetName();
				TestFalse(*FString::Printf(TEXT("%s replicates no armer/role field (%s)"), *C->GetName(), *N),
				          (N.Contains(TEXT("Armed")) && N.Contains(TEXT("By"))) || N.Contains(TEXT("Role")) || N.Contains(TEXT("Puid")));
			}
		}
		TestEqual(*FString::Printf(TEXT("%s is armed only through the ability"), *C->GetName()), C->GetDefaultObject<AKGTrap>()->TrapDef.ArmPolicy, FName(TEXT("None")));
	}
	// The role itself (unchanged, re-checked because the Trapper's whole design leans on it).
	CheckOwnerOnly(AKGPlayerState::StaticClass(), TEXT("AKGPlayerState"), {TEXT("PrivateRoleId")});
	return true;
}

// Acceptance 4: the Trapper shows up in about one match in three from 8 players; a forced role always lands.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapperFrequencyTest, "KillGodot.Abilities.TrapperFrequency",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTrapperFrequencyTest::RunTest(const FString& Parameters)
{
	const TArray<FKGRoleInfo>& Catalog = FKGRoleListGenerator::GetDefaultCatalog();
	const FName Trapper(TEXT("Trapper"));
	int32 AllLists = 0;
	int32 AllHits = 0;
	for (int32 N = 6; N <= FKGRoleListGenerator::MaxPlayers; ++N)
	{
		int32 Hits = 0;
		constexpr int32 Seeds = 300;
		for (uint64 Seed = 1; Seed <= Seeds; ++Seed)
		{
			FKGRng Rng(Seed * 104729ULL + N);
			const FKGRoleListResult R = FKGRoleListGenerator::Generate(N, Rng, Catalog, NAME_None);
			Hits += R.Roles.Contains(Trapper) ? 1 : 0;
		}
		const float Rate = static_cast<float>(Hits) / Seeds;
		AddInfo(FString::Printf(TEXT("N=%d Trapper in %.0f%% of lists"), N, Rate * 100.0f));
		if (N < 8)
		{
			TestEqual(*FString::Printf(TEXT("N=%d: no Trapper below 8 players"), N), Hits, 0);
			continue;
		}
		TestTrue(*FString::Printf(TEXT("N=%d: Trapper in ~1/3 of lists (%.2f)"), N, Rate), Rate >= 0.25f && Rate <= 0.45f);
		AllLists += Seeds;
		AllHits += Hits;
	}
	const float Overall = static_cast<float>(AllHits) / FMath::Max(1, AllLists);
	AddInfo(FString::Printf(TEXT("N>=8 overall: %.1f%%"), Overall * 100.0f));
	TestTrue(TEXT("Overall ~1 in 3"), Overall >= 0.28f && Overall <= 0.40f);

	for (int32 N = 8; N <= FKGRoleListGenerator::MaxPlayers; ++N)   // below 8 there is no killing slot (Trapper MinPlayers)
	{
		for (uint64 Seed = 1; Seed <= 10; ++Seed)
		{
			FKGRng Rng(Seed * 31ULL + N);
			const FKGRoleListResult R = FKGRoleListGenerator::Generate(N, Rng, Catalog, Trapper);
			int32 Count = 0;
			int32 Impatient = 0;
			for (const FName& Id : R.Roles)
			{
				Count += Id == Trapper ? 1 : 0;
				const FKGRoleInfo* Info = FKGRoleListGenerator::FindRole(Catalog, Id);
				Impatient += Info && Info->GetAlignment() == EKGAlignment::Impatient ? 1 : 0;
			}
			TestEqual(*FString::Printf(TEXT("N=%d seed=%llu forced Trapper exactly once"), N, Seed), Count, 1);
			TestEqual(*FString::Printf(TEXT("N=%d forced list keeps the Impatient count"), N), Impatient,
			          FKGRoleListGenerator::GetFactionCounts(N).Impatient);
		}
	}
	return true;
}

// The Trapper traps run on the SPRINT-040 machine: bite / snare timings and the tripwire's re-arm.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTrapperMachinesTest, "KillGodot.Abilities.TrapperMachines",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTrapperMachinesTest::RunTest(const FString& Parameters)
{
	auto Walk = [](FKGTrapMachine& M, float Secs)
	{
		for (float T = 0.0f; T < Secs; T += 0.05f)
		{
			M.Advance(0.05f);
		}
	};
	{
		FKGTrapMachine M;
		M.Configure(GetDefault<AKGMimicTrap>()->TrapDef);
		TestEqual(TEXT("Mimic starts asleep"), M.State, EKGTrapState::Idle);
		TestTrue(TEXT("Mimic arms"), M.TryArm());
		TestTrue(TEXT("Opening triggers"), M.TryTrigger());
		Walk(M, 0.4f);
		TestEqual(TEXT("The snap, then the bite"), M.State, EKGTrapState::Active);
		Walk(M, KGTrapperTuning::BiteHoldSecs + 0.1f);
		TestEqual(TEXT("Held ~3 s, then it lets go"), M.State, EKGTrapState::Cooldown);
		Walk(M, 2.5f);
		TestEqual(TEXT("Back to a normal chest (no self re-arm)"), M.State, EKGTrapState::Idle);
	}
	{
		FKGTrapMachine M;
		M.Configure(GetDefault<AKGSnareTrap>()->TrapDef);
		TestTrue(TEXT("Snare arms"), M.TryArm());
		TestTrue(TEXT("A step triggers"), M.TryTrigger());
		M.Advance(0.01f);
		TestEqual(TEXT("Snare bites at once"), M.State, EKGTrapState::Active);
		Walk(M, KGTrapperTuning::SnareHoldSecs + 1.5f);
		TestEqual(TEXT("Snare is single-use"), M.State, EKGTrapState::Idle);
	}
	{
		FKGTrapMachine M;
		M.Configure(GetDefault<AKGTripwireTrap>()->TrapDef);
		TestEqual(TEXT("Tripwire is always armed"), M.State, EKGTrapState::Armed);
		TestTrue(TEXT("A crossing trips it"), M.TryTrigger());
		Walk(M, 0.3f);
		TestEqual(TEXT("Silent pause"), M.State, EKGTrapState::Cooldown);
		Walk(M, KGTrapperTuning::TripwireRearmSecs + 0.2f);
		TestEqual(TEXT("It re-arms"), M.State, EKGTrapState::Armed);
	}
	TestTrue(TEXT("A bite is heavy but never kills"), KGTrapperTuning::BiteDamage >= 40.0f && KGTrapperTuning::BiteMinHealthLeft > 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
