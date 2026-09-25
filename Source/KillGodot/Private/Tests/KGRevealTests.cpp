#include "Misc/AutomationTest.h"
#include "Core/KGGameMode.h"
#include "Core/KGPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "UI/Reveal/KGPseudonyms.h"
#include "UI/Reveal/KGRevealComponent.h"
#include "UI/Reveal/KGRevealTypes.h"
#include "UI/Reveal/KGRoleCardText.h"

#if WITH_DEV_AUTOMATION_TESTS

// SPRINT-015 acceptance 3: the pseudonym mapping is stable within a match, different across matches, never the real
// name (nor anyone else's real name), and never shared by two players.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGPseudonymsTest, "KillGodot.Reveal.Pseudonyms",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGPseudonymsTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Real = {TEXT("Wanderer"), TEXT("Stranger"), TEXT("Fisher Riza"), TEXT("Baker Nuri"), TEXT("Widow Hatice"),
	                              TEXT("Old Kemal"), TEXT("Netmaker Sevgi"), TEXT("Smith Cemal"), TEXT("Priest Aurel"),
	                              TEXT("Innkeeper Mara"), TEXT("Shepherd Yusuf"), TEXT("Lamplighter Ivo"), TEXT("Herbalist Dunya"),
	                              TEXT("Harbourmaster Osman"), TEXT("Tanner Petra"), TEXT("Miller Salih"), TEXT("Candlemaker Lale"),
	                              TEXT("Ferryman Boris"), TEXT("Weaver Ayse"), TEXT("Gravedigger Emin")};
	auto Key = [](int32 Index) { return FString::Printf(TEXT("puid:%08x"), 0x1000 + Index * 7919); };

	// Stable within a match + unique + never a real name.
	FKGPseudonyms Match(0xC0FFEEull);
	TMap<FString, FString> First;
	TSet<FString> Seen;
	for (int32 Index = 0; Index < Real.Num(); ++Index)
	{
		const FString Name = Match.Get(Key(Index), Real[Index]);
		First.Add(Key(Index), Name);
		TestFalse(*FString::Printf(TEXT("Pseudonym '%s' is not a real name"), *Name),
		          Real.ContainsByPredicate([&Name](const FString& R) { return R.Equals(Name, ESearchCase::IgnoreCase); }));
		TestFalse(*FString::Printf(TEXT("Pseudonym '%s' is unique"), *Name), Seen.Contains(Name.ToLower()));
		Seen.Add(Name.ToLower());
		TestFalse(TEXT("Pseudonym is not empty"), Name.IsEmpty());
	}
	for (int32 Round = 0; Round < 3; ++Round)
	{
		for (int32 Index = Real.Num() - 1; Index >= 0; --Index)
		{
			TestEqual(TEXT("Same player, same pseudonym for the whole match"), Match.Get(Key(Index), Real[Index]), First[Key(Index)]);
		}
	}
	// A late joiner does not shift anyone else's name.
	Match.Get(TEXT("puid:latecomer"), TEXT("Latecomer"));
	for (int32 Index = 0; Index < Real.Num(); ++Index)
	{
		TestEqual(TEXT("Stable after a join"), *Match.Find(Key(Index)), First[Key(Index)]);
	}
	TestEqual(TEXT("One entry per player"), Match.Num(), Real.Num() + 1);

	// Different across matches: a new salt re-deals the names (for 20 players almost all differ, never all equal).
	int32 WorstSame = 0;
	for (uint64 Salt = 1; Salt <= 40; ++Salt)
	{
		FKGPseudonyms A(Salt);
		FKGPseudonyms B(Salt + 1000);
		int32 Same = 0;
		for (int32 Index = 0; Index < Real.Num(); ++Index)
		{
			Same += A.Get(Key(Index), Real[Index]) == B.Get(Key(Index), Real[Index]) ? 1 : 0;
		}
		WorstSame = FMath::Max(WorstSame, Same);
	}
	TestTrue(*FString::Printf(TEXT("Different across matches (worst case %d of %d names repeated)"), WorstSame, Real.Num()),
	         WorstSame <= 3);
	FKGPseudonyms Reset(7);
	const FString Before = Reset.Get(Key(0), Real[0]);
	Reset.Reset(8);
	TestEqual(TEXT("Reset forgets the old match"), Reset.Num(), 0);
	(void)Before;

	// A player whose real name happens to be the pseudonym they would get never gets it.
	FKGPseudonyms Probe(99);
	const FString Would = Probe.Get(TEXT("puid:mirror"), TEXT("x"));
	FKGPseudonyms Mirror(99);
	const FString Got = Mirror.Get(TEXT("puid:mirror"), Would);
	TestNotEqual(TEXT("Never the player's own real name"), Got, Would);
	// ...nor another player's real name that is known to the table.
	FKGPseudonyms Other(99);
	Other.ReserveRealName(Would);
	TestNotEqual(TEXT("Never another player's real name"), Other.Get(TEXT("puid:mirror"), TEXT("x")), Would);

	// Deterministic for a given salt (the same match on the same machine re-derives the same table).
	FKGPseudonyms Again(0xC0FFEEull);
	for (int32 Index = 0; Index < Real.Num(); ++Index)
	{
		TestEqual(TEXT("Deterministic per salt"), Again.Get(Key(Index), Real[Index]), First[Key(Index)]);
	}

	// Pool exhausted (every name reserved): the fallback still gives unique, non-real names.
	FKGPseudonyms Full(3);
	for (int32 Index = 0; Index < FKGPseudonyms::PoolSize(); ++Index)
	{
		Full.ReserveRealName(FKGPseudonyms::PoolName(Index));
	}
	const FString F1 = Full.Get(TEXT("a"), TEXT("Villager 1"));
	const FString F2 = Full.Get(TEXT("b"), TEXT("B"));
	TestNotEqual(TEXT("Fallback avoids the real name"), F1, FString(TEXT("Villager 1")));
	TestNotEqual(TEXT("Fallback names are unique"), F1, F2);
	TestTrue(TEXT("Pool is big enough for 20 players many times over"), FKGPseudonyms::PoolSize() >= 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRoleCardTextTest, "KillGodot.Reveal.RoleCards",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRoleCardTextTest::RunTest(const FString& Parameters)
{
	for (const FKGRoleInfo& Role : FKGRoleListGenerator::GetDefaultCatalog())
	{
		const FString Id = Role.RoleId.ToString();
		TestTrue(*FString::Printf(TEXT("%s has card text"), *Id), KGRoleCard::HasEntry(Role.RoleId));
		for (const bool bTr : {false, true})
		{
			const FKGRoleCardText Card = KGRoleCard::GetIn(Role.RoleId, bTr);
			const TCHAR* Lang = bTr ? TEXT("tr") : TEXT("en");
			TestFalse(*FString::Printf(TEXT("%s/%s name"), *Id, Lang), Card.Name.IsEmpty());
			TestFalse(*FString::Printf(TEXT("%s/%s goal"), *Id, Lang), Card.Goal.IsEmpty());
			TestFalse(*FString::Printf(TEXT("%s/%s team line"), *Id, Lang), Card.Team.IsEmpty());
			TestFalse(*FString::Printf(TEXT("%s/%s flavour"), *Id, Lang), Card.Flavour.IsEmpty());
			TestTrue(*FString::Printf(TEXT("%s/%s has 1-2 ability lines"), *Id, Lang),
			         Card.Abilities.Num() >= 1 && Card.Abilities.Num() <= 2);
			TestTrue(*FString::Printf(TEXT("%s/%s flavour fits the card (<= 80 chars)"), *Id, Lang), Card.Flavour.Len() <= 80);
		}
	}
	// Teams that meet at the reveal: the Clockbreakers; solo killers never see anyone.
	TestTrue(TEXT("Clockbreakers are a team"), KGRoleCard::IsTeamFaction(EKGFaction::Clockbreakers));
	TestFalse(TEXT("Serial killers work alone"), KGRoleCard::IsTeamFaction(EKGFaction::SoloKiller));
	TestFalse(TEXT("Town is not an Impatient team"), KGRoleCard::IsTeamFaction(EKGFaction::Town));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRevealTimelineTest, "KillGodot.Reveal.Timeline",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRevealTimelineTest::RunTest(const FString& Parameters)
{
	using namespace KGReveal;
	TestTrue(TEXT("Reveal lasts 5-7 s (SPRINT-037 contract)"), PhaseSeconds >= 5.0f && PhaseSeconds <= 7.0f);
	for (int32 Players = 6; Players <= 20; ++Players)
	{
		TestEqual(TEXT("Phase duration comes from the reveal timeline"), AKGGameMode::GetPhaseDuration(EKGPhase::RoleReveal, Players),
		          PhaseSeconds);
	}
	TestTrue(TEXT("Beats in order"), 0.0f < SpinEnd && SpinEnd < TeaseEnd && TeaseEnd < BangAt && BangAt < FlipEnd);
	TestTrue(TEXT("Words in order"), BangAt <= NameAt && NameAt < BannerAt && BannerAt < LineAt && LineAt < MatesAt && MatesAt < ReadEnd);
	TestTrue(TEXT("The moment lands fast (face up within 1.5 s)"), BangAt <= 1.5f);
	TestTrue(TEXT("Everything is on screen within 2.5 s"), ReadEnd <= 2.5f);
	TestTrue(TEXT("At least 3 s to read before the phase can end"), ReadEnd + 3.0f <= PhaseSeconds);
	TestTrue(TEXT("Ready only once the name is up"), ReadyFromSeconds > NameAt && ReadyFromSeconds <= ReadEnd);
	TestTrue(TEXT("Skipping leaves a fade-out"), SkipToSeconds > 0.0f && SkipToSeconds >= OutroSeconds && SkipToSeconds < PhaseSeconds - FlipEnd);
	TestEqual(TEXT("Stage at 0"), StageAt(0.0f), EStage::Spin);
	TestEqual(TEXT("Stage mid tease"), StageAt((SpinEnd + TeaseEnd) * 0.5f), EStage::Tease);
	TestEqual(TEXT("Stage at the bang"), StageAt(BangAt), EStage::Flip);
	TestEqual(TEXT("Stage after flip"), StageAt(PhaseSeconds), EStage::Role);
	TestEqual(TEXT("Stage names"), FString(StageName(EStage::Role)), FString(TEXT("role")));
	return true;
}

// SPRINT-037 acceptance 1: the main reveal says at most ~12 words - the role name, the alignment banner and ONE plain
// "what you do" line (<= 6 words) - in both languages, for every role in the catalog.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRevealMomentTextTest, "KillGodot.Reveal.MomentText",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRevealMomentTextTest::RunTest(const FString& Parameters)
{
	int32 Worst = 0;
	FString WorstRole;
	for (const FKGRoleInfo& Role : FKGRoleListGenerator::GetDefaultCatalog())
	{
		for (const bool bTr : {false, true})
		{
			const FKGRoleCardText Card = KGRoleCard::GetIn(Role.RoleId, bTr);
			const FString Banner = KGRoleCard::AlignmentBannerIn(Role.GetAlignment(), bTr);
			const TCHAR* Lang = bTr ? TEXT("tr") : TEXT("en");
			const FString Id = Role.RoleId.ToString();
			TestFalse(*FString::Printf(TEXT("%s/%s has a 'what you do' line"), *Id, Lang), Card.Do.IsEmpty());
			const int32 DoWords = KGRoleCard::CountWords(Card.Do);
			TestTrue(*FString::Printf(TEXT("%s/%s line '%s' is <= 6 words (%d)"), *Id, Lang, *Card.Do, DoWords), DoWords >= 2 && DoWords <= 6);
			const int32 Total = KGRoleCard::CountWords(Card.Name) + KGRoleCard::CountWords(Banner) + DoWords;
			TestTrue(*FString::Printf(TEXT("%s/%s main reveal is <= 12 words (%d)"), *Id, Lang, Total), Total <= 12);
			if (Total > Worst)
			{
				Worst = Total;
				WorstRole = FString::Printf(TEXT("%s/%s"), *Id, Lang);
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Longest main reveal: %d words (%s)"), Worst, *WorstRole));
	for (const EKGAlignment Align : {EKGAlignment::Town, EKGAlignment::Impatient, EKGAlignment::Neutral})
	{
		for (const bool bTr : {false, true})
		{
			const FString Banner = KGRoleCard::AlignmentBannerIn(Align, bTr);
			TestTrue(*FString::Printf(TEXT("Banner '%s' is short"), *Banner), KGRoleCard::CountWords(Banner) >= 2 && KGRoleCard::CountWords(Banner) <= 5);
			TestEqual(TEXT("Banner is upper case"), KGRoleCard::ToDisplayUpper(Banner, bTr), Banner);
		}
	}
	// Turkish upper case (the huge role name is shown in capitals).
	TestEqual(TEXT("Turkish i gets its dot"), KGRoleCard::ToDisplayUpper(TEXT("Şerif"), true), FString(TEXT("ŞERİF")));
	TestEqual(TEXT("Turkish dotless i"), KGRoleCard::ToDisplayUpper(TEXT("Kapı"), true), FString(TEXT("KAPI")));
	TestEqual(TEXT("Circumflex and umlauts"), KGRoleCard::ToDisplayUpper(TEXT("Kâhin Gözcü Çoban Doğu"), true),
	          FString(TEXT("KÂHİN GÖZCÜ ÇOBAN DOĞU")));
	TestEqual(TEXT("English stays English"), KGRoleCard::ToDisplayUpper(TEXT("Serial Killer"), false), FString(TEXT("SERIAL KILLER")));
	TestEqual(TEXT("Word count ignores punctuation"), KGRoleCard::CountWords(TEXT("Kill at night. Don't get caught.")), 6);
	return true;
}

// Secret data never leaves its owner: the role (player state) and everything the ceremony adds (teammates, ready).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRevealOwnerOnlyTest, "KillGodot.Reveal.OwnerOnly",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRevealOwnerOnlyTest::RunTest(const FString& Parameters)
{
	auto Conditions = [](UClass* Class)
	{
		TMap<FName, ELifetimeCondition> Out;
		Class->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		for (const FLifetimeProperty& Prop : Props)
		{
			// Only the class's own fields (engine base classes replicate their usual public flags).
			if (Class->ClassReps.IsValidIndex(Prop.RepIndex) && Class->ClassReps[Prop.RepIndex].Property->GetOwnerClass() == Class)
			{
				Out.Add(Class->ClassReps[Prop.RepIndex].Property->GetFName(), Prop.Condition);
			}
		}
		return Out;
	};
	const TMap<FName, ELifetimeCondition> Player = Conditions(AKGPlayerState::StaticClass());
	TestEqual(TEXT("PrivateRoleId is owner-only"), Player.FindRef(TEXT("PrivateRoleId")), COND_OwnerOnly);
	const TMap<FName, ELifetimeCondition> Reveal = Conditions(UKGRevealComponent::StaticClass());
	TestTrue(TEXT("Reveal component replicates its fields"), Reveal.Num() >= 5);
	for (const TPair<FName, ELifetimeCondition>& Pair : Reveal)
	{
		TestEqual(*FString::Printf(TEXT("UKGRevealComponent::%s is owner-only"), *Pair.Key.ToString()), Pair.Value, COND_OwnerOnly);
	}
	return true;
}

#endif
