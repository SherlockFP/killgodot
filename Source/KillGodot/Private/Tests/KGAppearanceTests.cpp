#include "Misc/AutomationTest.h"
#include "Character/KGAppearanceComponent.h"
#include "Character/KGVillagerLook.h"
#include "Core/KGPlayerState.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// SPRINT-027a acceptance 1: at least 12 visually distinct archetypes, men and women, several builds, silhouettes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGLookArchetypesTest, "KillGodot.Appearance.Archetypes",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGLookArchetypesTest::RunTest(const FString& Parameters)
{
	const TArray<FKGVillagerArchetype>& Table = FKGVillagerLookGen::Archetypes();
	TestTrue(TEXT("at least 30 archetypes (contract asks for 12)"), Table.Num() >= 30);
	TSet<FName> Names;
	TSet<uint64> Silhouettes;
	int32 Women = 0, Men = 0, Old = 0;
	TSet<EKGVillagerBuild> Builds;
	TSet<EKGVillagerBody> Bodies;
	TSet<EKGVillagerHat> Hats;
	for (const FKGVillagerArchetype& A : Table)
	{
		TestFalse(*FString::Printf(TEXT("archetype name %s is unique"), *A.Name.ToString()), Names.Contains(A.Name));
		Names.Add(A.Name);
		(A.bFemale ? Women : Men)++;
		Old += A.bOld ? 1 : 0;
		Builds.Add(A.Build);
		Bodies.Add(A.Body);
		Hats.Add(A.Hat);
		// The silhouette: everything except colours. No two archetypes may share one.
		const uint64 Sil = (static_cast<uint64>(A.Body) << 40) | (static_cast<uint64>(A.Build) << 32) |
		                   (static_cast<uint64>(A.Hair) << 24) | (static_cast<uint64>(A.Hat) << 16) |
		                   (A.bBeard ? 1 : 0) | (A.bHood ? 2 : 0) | (A.bPauldrons ? 4 : 0) | (A.bCape ? 8 : 0) |
		                   (A.bApron ? 16 : 0) | (A.bOld ? 32 : 0);
		TestFalse(*FString::Printf(TEXT("%s: silhouette differs from every other archetype"), *A.Name.ToString()), Silhouettes.Contains(Sil));
		Silhouettes.Add(Sil);
		TestFalse(*FString::Printf(TEXT("%s: body asset path"), *A.Name.ToString()), UKGAppearanceComponent::BodyPath(A.Body).IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s: body matches sex"), *A.Name.ToString()),
		         A.bFemale == UKGAppearanceComponent::BodyPath(A.Body).Contains(TEXT("_F")));
	}
	TestTrue(TEXT("at least 12 women"), Women >= 12);
	TestTrue(TEXT("at least 12 men"), Men >= 12);
	TestTrue(TEXT("elderly archetypes exist"), Old >= 4);
	TestTrue(TEXT("five builds used"), Builds.Num() == static_cast<int32>(EKGVillagerBuild::Count));
	TestTrue(TEXT("six body meshes used"), Bodies.Num() >= 6);
	TestTrue(TEXT("hats: none + 4 kinds"), Hats.Num() == static_cast<int32>(EKGVillagerHat::Count));
	return true;
}

// Acceptance 1: seeded per match, no two players identical in a 20-player lobby (and none in a 40-player one).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGLookUniquenessTest, "KillGodot.Appearance.UniqueLobby",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGLookUniquenessTest::RunTest(const FString& Parameters)
{
	for (uint64 Seed = 1; Seed <= 40; ++Seed)
	{
		TArray<FKGVillagerLook> Lobby;
		TSet<uint32> Keys;
		TSet<uint8> Archetypes;
		for (int32 Player = 0; Player < 40; ++Player)
		{
			const FKGVillagerLook L = FKGVillagerLookGen::Generate(Seed * 7919u, 100 + Player, Lobby);
			TestTrue(TEXT("assigned"), L.IsAssigned());
			TestFalse(*FString::Printf(TEXT("seed %llu player %d: look %s not taken"), Seed, Player, *L.ToString()), Keys.Contains(L.Key()));
			Keys.Add(L.Key());
			Lobby.Add(L);
			if (Player < 20)
			{
				Archetypes.Add(L.Archetype);
			}
			const FKGVillagerArchetype& A = FKGVillagerLookGen::ArchetypeOf(L);
			TestTrue(TEXT("old archetypes wear grey/white hair"), !A.bOld || L.HairColour >= 6);
			TestTrue(TEXT("young archetypes do not"), A.bOld || L.HairColour < 6);
		}
		TestEqual(*FString::Printf(TEXT("seed %llu: 20 players wear 20 different archetypes"), Seed), Archetypes.Num(), 20);
	}
	// Deterministic per (seed, player, taken); different seeds give different lobbies.
	const TArray<FKGVillagerLook> None;
	TestTrue(TEXT("deterministic"), FKGVillagerLookGen::Generate(42, 7, None) == FKGVillagerLookGen::Generate(42, 7, None));
	int32 Differs = 0;
	for (int32 P = 0; P < 20; ++P)
	{
		Differs += FKGVillagerLookGen::Generate(42, P, None) != FKGVillagerLookGen::Generate(43, P, None) ? 1 : 0;
	}
	TestTrue(TEXT("a new seed re-deals most looks"), Differs >= 15);
	return true;
}

// Acceptance 1: the profile's locked archetype wins while a palette of it is free; uniqueness still holds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGLookLockedTest, "KillGodot.Appearance.LockedLook",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGLookLockedTest::RunTest(const FString& Parameters)
{
	const FName Witch(TEXT("Witch"));
	const int32 WitchIndex = FKGVillagerLookGen::ArchetypeIndex(Witch);
	TestTrue(TEXT("Witch exists"), WitchIndex != INDEX_NONE);
	TArray<FKGVillagerLook> Lobby;
	TSet<uint32> Keys;
	int32 Witches = 0;
	for (int32 Player = 0; Player < 20; ++Player)
	{
		const FKGVillagerLook L = FKGVillagerLookGen::Generate(5, Player, Lobby, Witch);
		TestFalse(TEXT("unique"), Keys.Contains(L.Key()));
		Keys.Add(L.Key());
		Lobby.Add(L);
		Witches += L.Archetype == WitchIndex ? 1 : 0;
	}
	TestEqual(TEXT("twenty players all locked to Witch get twenty different witches"), Witches, 20);
	TestTrue(TEXT("unknown archetype falls back to random"), FKGVillagerLookGen::Generate(5, 1, Lobby, TEXT("Dragon")).IsAssigned());
	return true;
}

// Acceptance 2: other clients never receive role-dependent appearance data.
//  - The look struct has no role field and the generator takes no role.
//  - UKGAppearanceComponent (cuff, sash) has no replicated property and no RPC at all.
//  - The role inputs: PrivateRoleId stays COND_OwnerOnly (cuff), RevealedRoleId is the deliberately public one (sash).
//  - The cosmetics component's Look replicates with no condition (public cosmetic data, like the hat).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGLookRoleSafetyTest, "KillGodot.Appearance.RoleDataNeverPublic",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGLookRoleSafetyTest::RunTest(const FString& Parameters)
{
	for (TFieldIterator<FProperty> It(FKGVillagerLook::StaticStruct()); It; ++It)
	{
		const FString Name = It->GetName();
		TestFalse(*FString::Printf(TEXT("FKGVillagerLook::%s is not role data"), *Name),
		          Name.Contains(TEXT("Role")) || Name.Contains(TEXT("Align")) || Name.Contains(TEXT("Team")));
	}
	int32 ReplicatedProps = 0, NetFunctions = 0;
	// Own fields only: UActorComponent itself carries bReplicates/bIsActive as engine-replicated flags.
	for (TFieldIterator<FProperty> It(UKGAppearanceComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		ReplicatedProps += It->HasAnyPropertyFlags(CPF_Net) ? 1 : 0;
	}
	for (TFieldIterator<UFunction> It(UKGAppearanceComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		NetFunctions += It->HasAnyFunctionFlags(FUNC_Net) ? 1 : 0;
	}
	TestEqual(TEXT("UKGAppearanceComponent replicates nothing"), ReplicatedProps, 0);
	TestEqual(TEXT("UKGAppearanceComponent has no RPC"), NetFunctions, 0);
	TestFalse(TEXT("UKGAppearanceComponent is not a replicated component"),
	          GetDefault<UKGAppearanceComponent>()->GetIsReplicated());

	auto Conditions = [](UClass* Class)
	{
		TMap<FName, ELifetimeCondition> Out;
		Class->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		for (const FLifetimeProperty& Prop : Props)
		{
			if (Class->ClassReps.IsValidIndex(Prop.RepIndex) && Class->ClassReps[Prop.RepIndex].Property->GetOwnerClass() == Class)
			{
				Out.Add(Class->ClassReps[Prop.RepIndex].Property->GetFName(), Prop.Condition);
			}
		}
		return Out;
	};
	const TMap<FName, ELifetimeCondition> Player = Conditions(AKGPlayerState::StaticClass());
	TestEqual(TEXT("PrivateRoleId (cuff input) is owner-only"), Player.FindRef(TEXT("PrivateRoleId")), COND_OwnerOnly);
	TestTrue(TEXT("RevealedRoleId (sash input) is the public reveal"), Player.Contains(TEXT("RevealedRoleId")));
	TestEqual(TEXT("RevealedRoleId has no owner condition"), Player.FindRef(TEXT("RevealedRoleId")), COND_None);
	const TMap<FName, ELifetimeCondition> Cosmetics = Conditions(UKGCosmeticsComponent::StaticClass());
	TestTrue(TEXT("Look replicates"), Cosmetics.Contains(TEXT("Look")));
	TestEqual(TEXT("Look is public cosmetic data"), Cosmetics.FindRef(TEXT("Look")), COND_None);
	TestFalse(TEXT("PreferredLook never replicates"), Cosmetics.Contains(TEXT("PreferredLook")));

	// The three alignments get three distinct band colours, and an unknown/none role reads as neutral.
	const FLinearColor Town = UKGAppearanceComponent::AlignmentColour(EKGAlignment::Town);
	const FLinearColor Imp = UKGAppearanceComponent::AlignmentColour(EKGAlignment::Impatient);
	const FLinearColor Neu = UKGAppearanceComponent::AlignmentColour(EKGAlignment::Neutral);
	TestTrue(TEXT("alignment colours differ"), !Town.Equals(Imp) && !Town.Equals(Neu) && !Imp.Equals(Neu));
	TestEqual(TEXT("no role -> neutral"), UKGAppearanceComponent::AlignmentOfRole(NAME_None), EKGAlignment::Neutral);
	const TArray<FKGRoleInfo>& Roles = FKGRoleListGenerator::GetDefaultCatalog();
	if (Roles.Num() > 0)
	{
		TestEqual(TEXT("role alignment lookup"), UKGAppearanceComponent::AlignmentOfRole(Roles[0].RoleId), Roles[0].GetAlignment());
	}
	return true;
}

#endif
