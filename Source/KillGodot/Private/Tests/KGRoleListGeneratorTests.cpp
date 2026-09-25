#include "Misc/AutomationTest.h"
#include "Core/KGRng.h"
#include "Roles/KGRoleListGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRoleCatalogTest, "KillGodot.Roles.Catalog",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRoleCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FKGRoleInfo>& Catalog = FKGRoleListGenerator::GetDefaultCatalog();
	TestEqual(TEXT("Catalog has 51 roles"), Catalog.Num(), 51);

	int32 Town = 0, Impatient = 0, Neutral = 0;
	TSet<FName> Ids;
	for (const FKGRoleInfo& Role : Catalog)
	{
		TestFalse(*FString::Printf(TEXT("Duplicate id %s"), *Role.RoleId.ToString()), Ids.Contains(Role.RoleId));
		Ids.Add(Role.RoleId);
		switch (Role.GetAlignment())
		{
		case EKGAlignment::Town: ++Town; break;
		case EKGAlignment::Impatient: ++Impatient; break;
		case EKGAlignment::Neutral: ++Neutral; break;
		}
	}
	TestEqual(TEXT("22 town roles"), Town, 22);
	TestEqual(TEXT("21 impatient roles"), Impatient, 21);
	TestEqual(TEXT("8 neutral roles"), Neutral, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRoleListGeneratorTest, "KillGodot.Roles.Generator",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRoleListGeneratorTest::RunTest(const FString& Parameters)
{
	const TArray<FKGRoleInfo>& Catalog = FKGRoleListGenerator::GetDefaultCatalog();
	int32 Generated = 0;
	int32 OutOfBand = 0;

	for (int32 N = FKGRoleListGenerator::MinPlayers; N <= FKGRoleListGenerator::MaxPlayers; ++N)
	{
		const FKGFactionCounts Expected = FKGRoleListGenerator::GetFactionCounts(N);
		TestEqual(*FString::Printf(TEXT("Counts sum to N=%d"), N),
		          Expected.Town + Expected.Impatient + Expected.Neutral, N);

		for (uint64 Seed = 1; Seed <= 40; ++Seed)
		{
			FKGRng Rng(Seed * 7919ULL + N);
			const FKGRoleListResult Result = FKGRoleListGenerator::Generate(N, Rng, Catalog);
			++Generated;
			if (!TestEqual(*FString::Printf(TEXT("N=%d seed=%llu role count"), N, Seed), Result.Roles.Num(), N))
			{
				continue;
			}

			int32 Town = 0, Impatient = 0, Neutral = 0, Solo = 0;
			TMap<FName, int32> Seen;
			for (const FName& Id : Result.Roles)
			{
				const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(Catalog, Id);
				if (!TestNotNull(TEXT("Role exists in catalog"), Role))
				{
					continue;
				}
				Seen.FindOrAdd(Id)++;
				TestTrue(*FString::Printf(TEXT("%s min players"), *Id.ToString()), Role->MinPlayers <= N);
				Solo += Role->Category == EKGRoleCategory::SoloKilling ? 1 : 0;
				switch (Role->GetAlignment())
				{
				case EKGAlignment::Town: ++Town; break;
				case EKGAlignment::Impatient: ++Impatient; break;
				case EKGAlignment::Neutral: ++Neutral; break;
				}
			}
			TestEqual(*FString::Printf(TEXT("N=%d town"), N), Town, Expected.Town);
			TestEqual(*FString::Printf(TEXT("N=%d impatient"), N), Impatient, Expected.Impatient);
			TestEqual(*FString::Printf(TEXT("N=%d neutral"), N), Neutral, Expected.Neutral);
			TestTrue(TEXT("At most 2 solo killers"), Solo <= 2);
			TestTrue(TEXT("Exactly one Clockmaster"), Seen.FindRef(FName(TEXT("Clockmaster"))) == 1);

			for (const TPair<FName, int32>& Pair : Seen)
			{
				const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(Catalog, Pair.Key);
				if (Role && Role->bUnique)
				{
					TestTrue(*FString::Printf(TEXT("Unique %s appears once"), *Pair.Key.ToString()), Pair.Value == 1);
				}
				if (Role)
				{
					for (const FName& Other : Role->ExclusiveWith)
					{
						TestFalse(*FString::Printf(TEXT("%s excludes %s"), *Pair.Key.ToString(), *Other.ToString()),
						          Seen.Contains(Other));
					}
				}
			}
			OutOfBand += Result.bWithinBand ? 0 : 1;
		}
	}

	// Balance is a soft goal; flag it if the band is unreachable too often.
	const float OutOfBandRatio = static_cast<float>(OutOfBand) / FMath::Max(1, Generated);
	AddInfo(FString::Printf(TEXT("Out-of-band role lists: %d / %d"), OutOfBand, Generated));
	TestTrue(TEXT("Balance band reachable in >= 90% of lists"), OutOfBandRatio <= 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGRngDeterminismTest, "KillGodot.Core.RngDeterminism",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGRngDeterminismTest::RunTest(const FString& Parameters)
{
	FKGRng A(12345);
	FKGRng B(12345);
	for (int32 i = 0; i < 1000; ++i)
	{
		if (!TestEqual(TEXT("Same seed, same stream"), static_cast<int64>(A.NextUInt32()), static_cast<int64>(B.NextUInt32())))
		{
			break;
		}
	}
	// A copied RNG (as restored from a snapshot) continues the same sequence.
	FKGRng Restored = A;
	TestEqual(TEXT("Restored stream continues"), Restored.RandRange(0, 1000000), A.RandRange(0, 1000000));

	FKGRng C(99);
	for (int32 i = 0; i < 10000; ++i)
	{
		const int32 V = C.RandRange(3, 7);
		if (!TestTrue(TEXT("RandRange inclusive bounds"), V >= 3 && V <= 7))
		{
			break;
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
