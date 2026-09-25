#include "Fishing/KGFishingTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "Materials/MaterialInterface.h"
#include "World/KGFishSchool.h"
#include "World/KGWaves.h"

#define LOCTEXT_NAMESPACE "KGFishing"

// ---------------------------------------------------------------------------------------------------------------
// Species table (Docs/01_GDD_Core.md §15 mirrors it; KGFishingTests checks the invariants)
// ---------------------------------------------------------------------------------------------------------------
namespace KGFishingPrivate
{
	//                                    None Land Sea  Basin Brook Koi
	FKGFishSpecies Make(FName Id, FName Item, float Base, std::initializer_list<float> Water, float Day, float Dawn,
	                    float Night, float MinKg, float MaxKg, float Pull, float StaminaSec, float Agility, float Lateral,
	                    float HookWindow, FName School, float Affinity, bool bSacred = false)
	{
		FKGFishSpecies S;
		S.Id = Id;
		S.ItemId = Item;
		S.Base = Base;
		int32 i = 0;
		for (const float W : Water)
		{
			if (i < static_cast<int32>(EKGFishWater::Count))
			{
				S.Water[i++] = W;
			}
		}
		S.Day = Day;
		S.Dawn = Dawn;
		S.Night = Night;
		S.MinKg = MinKg;
		S.MaxKg = MaxKg;
		S.Pull = Pull;
		S.StaminaSec = StaminaSec;
		S.Agility = Agility;
		S.Lateral = Lateral;
		S.HookWindow = HookWindow;
		S.School = School;
		S.SchoolAffinity = Affinity;
		S.bSacred = bSacred;
		return S;
	}

	TArray<FKGFishSpecies> Build()
	{
		using Ids = FKGItemIds;
		TArray<FKGFishSpecies> Out;
		//                 id                       item                 base   None Land  Sea  Basin Brook Koi    day  dawn night  kg min/max     pull stam agil lat  hook  school                 affinity
		Out.Add(Make(TEXT("Mackerel"), Ids::FishMackerel, 50.0f, {0, 0, 1.0f, 1.3f, 0, 0}, 1.3f, 1.0f, 0.45f, 0.25f, 1.1f, 0.30f, 4.5f, 0.50f, 0.9f, 0.90f, TEXT("Mackerel"), 3.0f));
		Out.Add(Make(TEXT("Cod"), Ids::FishCod, 30.0f, {0, 0, 1.0f, 0.35f, 0, 0}, 0.6f, 1.0f, 1.9f, 1.2f, 8.5f, 0.55f, 10.0f, 0.30f, 0.5f, 0.75f, TEXT("Cod"), 3.0f));
		Out.Add(Make(TEXT("Salmon"), Ids::FishSalmon, 20.0f, {0, 0, 0.25f, 0.12f, 1.0f, 0}, 0.8f, 2.2f, 0.9f, 1.8f, 6.5f, 0.70f, 12.0f, 1.00f, 1.3f, 0.60f, TEXT("Salmon"), 3.0f));
		Out.Add(Make(TEXT("GoldenCarp"), Ids::FishGoldenCarp, 0.9f, {0, 0, 0.35f, 0.0f, 0.6f, 0}, 0.5f, 1.2f, 1.8f, 2.5f, 11.0f, 0.88f, 20.0f, 1.20f, 1.5f, 0.45f, TEXT("GoldenCarp"), 8.0f));
		Out.Add(Make(TEXT("Koi"), NAME_None, 1.0f, {0, 0, 0, 0, 0, 1.0f}, 1.0f, 1.0f, 1.0f, 1.0f, 3.0f, 0.4f, 5.0f, 0.6f, 1.0f, 0.8f, TEXT("Koi"), 0.0f, true));
		return Out;
	}

	float Falloff(float Distance, float Radius)
	{
		return FMath::Clamp(1.0f - Distance / FMath::Max(Radius, 1.0f), 0.0f, 1.0f);
	}

	constexpr float SchoolRadius = 1500.0f;   // cm: schools further than 15 m from the bobber do not matter
}

const TArray<FKGFishSpecies>& FKGFishingRules::Species()
{
	static const TArray<FKGFishSpecies> Table = KGFishingPrivate::Build();
	return Table;
}

const FKGFishSpecies* FKGFishingRules::Find(FName Id)
{
	return Species().FindByPredicate([Id](const FKGFishSpecies& S) { return S.Id == Id; });
}

int32 FKGFishingRules::IndexOf(FName Id)
{
	return Species().IndexOfByPredicate([Id](const FKGFishSpecies& S) { return S.Id == Id; });
}

const FKGFishSpecies* FKGFishingRules::FindByItem(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	return Species().FindByPredicate([ItemId](const FKGFishSpecies& S) { return S.ItemId == ItemId; });
}

bool FKGFishingRules::CanFishInPhase(EKGPhase Phase)
{
	switch (Phase)
	{
	case EKGPhase::Meeting:
	case EKGPhase::Trial:
	case EKGPhase::RoleReveal:
	case EKGPhase::Migrating:
		return false;
	default:
		return true;
	}
}

EKGFishTime FKGFishingRules::TimeFor(EKGPhase Phase)
{
	switch (Phase)
	{
	case EKGPhase::Night:
		return EKGFishTime::Night;
	case EKGPhase::Dawn:
		return EKGFishTime::Dawn;
	default:
		return EKGFishTime::Day;
	}
}

float FKGFishingRules::JunkChance(EKGFishWater Water)
{
	switch (Water)
	{
	case EKGFishWater::Sea:
		return 0.07f;
	case EKGFishWater::Basin:
		return 0.14f;   // harbour trash
	case EKGFishWater::Brook:
		return 0.06f;
	default:
		return 0.0f;
	}
}

float FKGFishingRules::SchoolBonus(const TArray<FKGSchoolSample>& Schools)
{
	float Bonus = 0.0f;
	for (const FKGSchoolSample& S : Schools)
	{
		if (S.Species == TEXT("Gull") || S.Count <= 0)
		{
			continue;
		}
		Bonus += (S.Count / 12.0f) * KGFishingPrivate::Falloff(S.Distance, KGFishingPrivate::SchoolRadius);
	}
	return FMath::Clamp(Bonus, 0.0f, 1.5f);
}

float FKGFishingRules::BiteRate(EKGFishWater Water, EKGFishTime Time, float SchoolBonus)
{
	float WaterMod = 0.0f;
	switch (Water)
	{
	case EKGFishWater::Sea:
		WaterMod = 1.0f;
		break;
	case EKGFishWater::Basin:
		WaterMod = 1.1f;
		break;
	case EKGFishWater::Brook:
		WaterMod = 0.95f;
		break;
	default:
		return 0.0f;
	}
	const float TimeMod = Time == EKGFishTime::Dawn ? 1.25f : Time == EKGFishTime::Night ? 0.9f : 1.0f;
	return (1.0f / 14.0f) * WaterMod * TimeMod * (1.0f + FMath::Max(0.0f, SchoolBonus));
}

float FKGFishingRules::SpeciesWeight(const FKGFishSpecies& S, EKGFishWater Water, EKGFishTime Time,
                                     const TArray<FKGSchoolSample>& Schools)
{
	if (S.bSacred)
	{
		return 0.0f;
	}
	float Affinity = 0.0f;
	for (const FKGSchoolSample& School : Schools)
	{
		if (School.Species == S.School)
		{
			Affinity += KGFishingPrivate::Falloff(School.Distance, KGFishingPrivate::SchoolRadius) *
			            FMath::Min(1.0f, School.Count / 8.0f);
		}
	}
	return S.Base * S.WaterWeight(Water) * S.TimeWeight(Time) * (1.0f + S.SchoolAffinity * FMath::Min(Affinity, 1.0f));
}

FKGBiteSchedule FKGFishingRules::RollSchedule(FKGRng& Rng, float Rate)
{
	FKGBiteSchedule Out;
	const float U = FMath::Min(Rng.FRand(), 0.999f);
	Out.BiteAt = FMath::Clamp(-FMath::Loge(1.0f - U) / FMath::Max(Rate, 0.001f), 2.5f, 40.0f);
	const int32 Nibbles = Rng.RandRange(0, 3);
	float T = Out.BiteAt;
	for (int32 k = 0; k < Nibbles; ++k)
	{
		T -= 0.6f + Rng.FRand() * 1.1f;
		if (T < 1.0f)
		{
			break;
		}
		Out.Nibbles.Insert(T, 0);
	}
	return Out;
}

int32 FKGFishingRules::RollGrams(FKGRng& Rng, const FKGFishSpecies& S)
{
	const float U = FMath::Pow(Rng.FRand(), 1.8f);
	return FMath::RoundToInt((S.MinKg + (S.MaxKg - S.MinKg) * U) * 1000.0f);
}

float FKGFishingRules::WeightAlpha(const FKGFishSpecies& S, int32 Grams)
{
	const float Kg = Grams / 1000.0f;
	return FMath::Clamp((Kg - S.MinKg) / FMath::Max(S.MaxKg - S.MinKg, 0.01f), 0.0f, 1.0f);
}

FKGBiteRoll FKGFishingRules::RollBite(FKGRng& Rng, EKGFishWater Water, EKGFishTime Time,
                                      const TArray<FKGSchoolSample>& Schools)
{
	FKGBiteRoll Out;
	if (Water == EKGFishWater::KoiPond || Water == EKGFishWater::None || Water == EKGFishWater::Land)
	{
		return Out;   // sacred / nothing: ItemId stays None
	}
	if (Rng.FRand() < JunkChance(Water))
	{
		TArray<FKGItemStack> Stacks;
		if (const FKGLootTable* Table = FKGLoot::FindTable(TEXT("Fishing")))
		{
			FKGLoot::Roll(Rng, *Table, Stacks);
		}
		Out.ItemId = Stacks.Num() > 0 ? Stacks[0].ItemId : FKGItemIds::OldBoot;
		return Out;
	}
	const TArray<FKGFishSpecies>& All = Species();
	float Total = 0.0f;
	TArray<float, TInlineAllocator<8>> Weights;
	for (const FKGFishSpecies& S : All)
	{
		Weights.Add(SpeciesWeight(S, Water, Time, Schools));
		Total += Weights.Last();
	}
	if (Total <= 0.0f)
	{
		Out.ItemId = FKGItemIds::OldBoot;
		return Out;
	}
	float Pick = Rng.FRand() * Total;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (Weights[i] <= 0.0f)
		{
			continue;
		}
		if (Pick < Weights[i] || i == All.Num() - 1)
		{
			Out.Species = i;
			break;
		}
		Pick -= Weights[i];
	}
	if (Out.Species == INDEX_NONE)   // float edge: the last positive weight
	{
		for (int32 i = All.Num() - 1; i >= 0; --i)
		{
			if (Weights[i] > 0.0f)
			{
				Out.Species = i;
				break;
			}
		}
	}
	Out.ItemId = All[Out.Species].ItemId;
	Out.Grams = RollGrams(Rng, All[Out.Species]);
	return Out;
}

int32 FKGFishingRules::SellPrice(FName ItemId, int32 Count, int32 TotalGrams)
{
	const FKGItemDef* Def = UKGItemCatalog::Find(ItemId);
	if (!Def || Count <= 0)
	{
		return 0;
	}
	const FKGFishSpecies* S = FindByItem(ItemId);
	if (!S || TotalGrams <= 0)
	{
		return Def->GoldValue * Count;
	}
	const float Alpha = WeightAlpha(*S, TotalGrams / Count);
	const int32 Each = FMath::Max(1, FMath::RoundToInt(Def->GoldValue * (0.6f + 0.9f * Alpha)));
	return Each * Count;
}

bool FKGFishingRules::IsSellable(FName ItemId)
{
	const FKGItemDef* Def = UKGItemCatalog::Find(ItemId);
	return Def && !Def->HasTag(TEXT("Currency")) && (Def->HasTag(TEXT("Fish")) || Def->HasTag(TEXT("Sea")));
}

float FKGFishingRules::ServerHookWindow(float SpeciesWindow, float PingSeconds)
{
	return SpeciesWindow + 0.2f + FMath::Clamp(PingSeconds, 0.0f, 0.3f);
}

int32 FKGFishingRules::NumBottleMessages()
{
	return 8;
}

FText FKGFishingRules::BottleMessage(int32 Index)
{
	switch (((Index % 8) + 8) % 8)
	{
	case 0: return LOCTEXT("Bottle0", "\"Godot is coming. Tomorrow. Probably.\"");
	case 1: return LOCTEXT("Bottle1", "\"Whoever reads this: the baker waters the ale.\"");
	case 2: return LOCTEXT("Bottle2", "\"Day 40 on the island. The pagoda monk still won't talk.\"");
	case 3: return LOCTEXT("Bottle3", "\"The bell rang twice last night. Nobody pulled the rope.\"");
	case 4: return LOCTEXT("Bottle4", "\"If found, return to the lighthouse keeper. He owes me a boot.\"");
	case 5: return LOCTEXT("Bottle5", "\"Do not trust anyone who fishes at night.\"");
	case 6: return LOCTEXT("Bottle6", "\"The golden carp grants one wish. I wished for a bigger bottle.\"");
	default: return LOCTEXT("Bottle7", "\"Madam Brine's prices are a crime. Tell the Sheriff.\"");
	}
}

FVector FKGFishingRules::CastVelocity(const FVector& ViewDir, float Power)
{
	const FVector Dir = ViewDir.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const float Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
	const float LaunchPitch = FMath::DegreesToRadians(FMath::Clamp(Pitch + 14.0f, -5.0f, 55.0f));
	FVector Flat = FVector(Dir.X, Dir.Y, 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Launch = Flat * FMath::Cos(LaunchPitch) + FVector::UpVector * FMath::Sin(LaunchPitch);
	return Launch * (450.0f + 1150.0f * FMath::Clamp(Power, 0.0f, 1.0f));
}

float FKGFishingRules::ChargePower(float Seconds)
{
	const float T = FMath::Fmod(FMath::Max(Seconds, 0.0f), ChargePeriod) / ChargePeriod;
	return FMath::Max(0.08f, T < 0.5f ? T * 2.0f : 2.0f - 2.0f * T);
}

// ---------------------------------------------------------------------------------------------------------------
// Reel minigame
// ---------------------------------------------------------------------------------------------------------------
void FKGReelSim::Init(const FKGFishSpecies* Species, float WeightAlpha, float StartDistance, uint64 Seed)
{
	*this = FKGReelSim();
	if (Species && !Species->bSacred)
	{
		Pull = Species->Pull;
		StaminaSec = Species->StaminaSec;
		Agility = Species->Agility;
		Lateral = Species->Lateral;
		WeightMul = 0.85f + 0.35f * FMath::Clamp(WeightAlpha, 0.0f, 1.0f);
		bJunk = false;
	}
	else
	{
		Pull = 0.22f;
		StaminaSec = 2.5f;
		Agility = 0.0f;
		Lateral = 0.0f;
		WeightMul = 1.0f;
		bJunk = true;
	}
	Tension = 0.3f;
	Stamina = 1.0f;
	Distance = FMath::Clamp(StartDistance, 3.0f, 34.0f);
	Rng = FKGRng(Seed, 77u);
	PullDir = Rng.FRand() < 0.5f ? -1 : 1;
	NextSwitch = bJunk ? 1.0e9f : (0.5f + Rng.FRand()) / FMath::Max(Agility, 0.05f);
}

float FKGReelSim::CurrentPull() const
{
	if (bJunk)
	{
		return Pull;
	}
	// Slow surges plus a burst right after every change of direction (the moment to let go).
	const float Burst = FMath::Exp(-3.0f * FMath::Max(0.0f, Elapsed - LastSwitch));
	return Pull * (0.45f + 0.55f * Stamina) * WeightMul * (1.0f + 0.2f * FMath::Sin(Elapsed * 2.1f) + 0.5f * Burst);
}

void FKGReelSim::Step(float H, const FKGReelInput& In)
{
	if (Result != EKGReelResult::None)
	{
		return;
	}
	Elapsed += H;
	if (!bJunk && Elapsed >= NextSwitch)
	{
		PullDir = -PullDir;
		LastSwitch = Elapsed;
		NextSwitch = Elapsed + (0.5f + Rng.FRand()) / FMath::Max(Agility, 0.05f);
	}
	if (!bJunk)
	{
		FishX += PullDir * Lateral * (0.4f + 0.6f * Stamina) * H;
		if (FMath::Abs(FishX) >= 1.0f)
		{
			FishX = FMath::Clamp(FishX, -1.0f, 1.0f);
			const int8 Turn = FishX > 0.0f ? -1 : 1;   // turns at the edge of its run
			if (Turn != PullDir)
			{
				PullDir = Turn;
				LastSwitch = Elapsed;
			}
		}
	}
	const float P = CurrentPull();
	const float Align = bJunk ? 0.0f : FMath::Clamp(In.Steer, -1.0f, 1.0f) * static_cast<float>(-PullDir);
	const float Counter = FMath::Max(0.0f, Align);
	const float Wrong = FMath::Max(0.0f, -Align);
	const float Target = In.bReel ? 0.30f + 0.80f * P * (1.0f - 0.55f * Counter) + 0.35f * Wrong * P
	                              : 0.02f + 0.28f * P * (1.0f - 0.5f * Counter) + 0.20f * Wrong * P;
	const float Rate = In.bReel ? (Target > Tension ? 2.6f : 1.8f) : 2.2f;
	Tension = FMath::Clamp(Tension + (Target - Tension) * (1.0f - FMath::Exp(-Rate * H)), 0.0f, 1.4f);

	const bool bGood = In.bReel && Tension >= GoodMin && Tension < 1.0f;
	Stamina = FMath::Max(0.0f, Stamina - H / FMath::Max(StaminaSec, 0.1f) * (bGood ? 0.5f + 0.5f * Counter : 0.1f));

	if (In.bReel && Tension < 1.0f)
	{
		Distance -= ReelSpeed * (0.2f + 0.8f * (1.0f - Stamina)) * H;   // a fresh fish barely comes in
	}
	else if (In.bReel)
	{
		Distance += 0.35f * H;   // the drag slips
	}
	else
	{
		Distance += 0.9f * P * Stamina * H;   // the fish runs
	}

	OverTime = Tension >= 1.0f ? OverTime + H : FMath::Max(0.0f, OverTime - 2.0f * H);
	SlackTime = Tension <= SlackTension ? SlackTime + H : FMath::Max(0.0f, SlackTime - 2.0f * H);
	if (OverTime >= SnapSeconds)
	{
		Result = EKGReelResult::Snapped;
	}
	else if (SlackTime >= SlackSeconds)
	{
		Result = EKGReelResult::Escaped;
	}
	else if (Distance <= LandDistance)
	{
		Distance = LandDistance;
		Result = EKGReelResult::Landed;
	}
	else if (Distance > 36.0f)
	{
		Result = EKGReelResult::Snapped;   // ran out of line
	}
	else if (Elapsed >= MaxSeconds)
	{
		Result = EKGReelResult::Escaped;
	}
}

void FKGReelSim::Advance(float DeltaSeconds, const FKGReelInput& Input)
{
	Accum += FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
	while (Accum >= StepSeconds && Result == EKGReelResult::None)
	{
		Accum -= StepSeconds;
		Step(StepSeconds, Input);
	}
}

void FKGReelSim::Reconcile(const FKGReelSim& Server, float Alpha)
{
	Pull = Server.Pull;
	StaminaSec = Server.StaminaSec;
	Agility = Server.Agility;
	Lateral = Server.Lateral;
	WeightMul = Server.WeightMul;
	bJunk = Server.bJunk;
	Tension = FMath::Lerp(Tension, Server.Tension, Alpha);
	Stamina = FMath::Lerp(Stamina, Server.Stamina, Alpha);
	Distance = FMath::Lerp(Distance, Server.Distance, Alpha);
	FishX = FMath::Lerp(FishX, Server.FishX, Alpha);
	Elapsed = Server.Elapsed;
	NextSwitch = Server.NextSwitch;
	LastSwitch = Server.LastSwitch;
	PullDir = Server.PullDir;
	Rng = Server.Rng;
	OverTime = Server.OverTime;
	SlackTime = Server.SlackTime;
}

FKGReelInput FKGReelSim::ExpertInput(const FKGReelSim& Sim)
{
	FKGReelInput In;
	In.bReel = Sim.Tension < 0.78f;
	In.Steer = Sim.bJunk ? 0.0f : static_cast<float>(-Sim.PullDir);
	return In;
}

// ---------------------------------------------------------------------------------------------------------------
// Water
// ---------------------------------------------------------------------------------------------------------------
namespace KGFishingPrivate
{
	/** One M_KG_PondWater plane (brook ribbon, mill pond, koi pond). */
	struct FFreshPlane
	{
		FTransform Xf;
		FVector Centre = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FBox2D Bounds;
		bool bKoi = false;
	};

	struct FWaterCache
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FFreshPlane> Planes;
	};

	FWaterCache& Cache()
	{
		static FWaterCache C;
		return C;
	}

	const TArray<FFreshPlane>& PlanesFor(UWorld* World)
	{
		FWaterCache& C = Cache();
		if (C.World.Get() == World)
		{
			return C.Planes;
		}
		C.World = World;
		C.Planes.Reset();
		if (!World)
		{
			return C.Planes;
		}
		TArray<FVector> KoiSchools;
		for (TActorIterator<AKGFishSchool> It(World); It; ++It)
		{
			if (It->Species == TEXT("Koi"))
			{
				KoiSchools.Add(It->GetActorLocation());
			}
		}
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			const UStaticMeshComponent* Comp = *It;
			if (!Comp || Comp->GetWorld() != World || !Comp->GetStaticMesh() || Comp->GetStaticMesh()->GetName() != TEXT("Plane"))
			{
				continue;
			}
			const UMaterialInterface* Mat = Comp->GetMaterial(0);
			if (!Mat || !Mat->GetPathName().Contains(TEXT("M_KG_PondWater")))
			{
				continue;
			}
			FFreshPlane P;
			P.Xf = Comp->GetComponentTransform();
			P.Centre = P.Xf.GetLocation();
			P.Normal = P.Xf.GetUnitAxis(EAxis::Z);
			if (FMath::Abs(P.Normal.Z) < 0.7f)
			{
				continue;   // the koi spill curtain and other vertical sheets
			}
			const FBox Box = Comp->Bounds.GetBox();
			P.Bounds = FBox2D(FVector2D(Box.Min), FVector2D(Box.Max));
			for (const FVector& Koi : KoiSchools)
			{
				if (FVector::Dist2D(Koi, P.Centre) < 800.0f && FMath::Abs(Koi.Z - P.Centre.Z) < 300.0f)
				{
					P.bKoi = true;
				}
			}
			C.Planes.Add(P);
		}
		return C.Planes;
	}
}

void FKGFishingWater::ResetCache()
{
	KGFishingPrivate::Cache() = KGFishingPrivate::FWaterCache();
}

bool FKGFishingWater::FreshSurfaceAt(UWorld* World, const FVector2D& XY, float MaxZ, float& OutZ, bool& bOutKoi)
{
	bool bFound = false;
	OutZ = -TNumericLimits<float>::Max();
	bOutKoi = false;
	for (const KGFishingPrivate::FFreshPlane& P : KGFishingPrivate::PlanesFor(World))
	{
		if (!P.Bounds.IsInside(XY))
		{
			continue;
		}
		// Height of the (slightly tilted) plane straight above/below XY, then the inside test in mesh space.
		const float Z = P.Centre.Z - (P.Normal.X * (XY.X - P.Centre.X) + P.Normal.Y * (XY.Y - P.Centre.Y)) / P.Normal.Z;
		const FVector Local = P.Xf.InverseTransformPosition(FVector(XY.X, XY.Y, Z));
		if (FMath::Abs(Local.X) > 50.5f || FMath::Abs(Local.Y) > 50.5f || Z > MaxZ || Z < OutZ)
		{
			continue;
		}
		OutZ = Z;
		bOutKoi = P.bKoi;
		bFound = true;
	}
	return bFound;
}

EKGFishWater FKGFishingWater::Classify(UWorld* World, const FVector& Point, float Time, float& OutSurfaceZ,
                                        const AActor* Ignore)
{
	OutSurfaceZ = FKGWaves::SeaLevel;
	if (!World)
	{
		return EKGFishWater::None;
	}
	float FreshZ = 0.0f;
	bool bKoi = false;
	if (FreshSurfaceAt(World, FVector2D(Point), Point.Z + 60.0f, FreshZ, bKoi) && Point.Z > FreshZ - 60.0f)
	{
		OutSurfaceZ = FreshZ;
		return bKoi ? EKGFishWater::KoiPond : EKGFishWater::Brook;
	}
	const float SeaZ = FKGWaves::HeightAt(Point.X, Point.Y, Time);
	OutSurfaceZ = SeaZ;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGFishWater), false, Ignore);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	const FVector Top(Point.X, Point.Y, FMath::Max(Point.Z, SeaZ) + 20.0f);
	const FVector Bottom(Point.X, Point.Y, SeaZ - 400.0f);
	if (World->LineTraceSingleByObjectType(Hit, Top, Bottom, Objects, Params) && Hit.ImpactPoint.Z > SeaZ - 15.0f)
	{
		OutSurfaceZ = Hit.ImpactPoint.Z;
		return EKGFishWater::Land;
	}
	return FKGWaves::CalmWeight(Point.X, Point.Y) > 0.5f ? EKGFishWater::Basin : EKGFishWater::Sea;
}

void FKGFishingWater::GatherSchools(UWorld* World, const FVector& Point, float Radius, TArray<FKGSchoolSample>& Out)
{
	Out.Reset();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AKGFishSchool> It(World); It; ++It)
	{
		if (It->Species == TEXT("Gull"))
		{
			continue;
		}
		const float D = FVector::Dist2D(It->GetActorLocation(), Point);
		if (D <= Radius && FMath::Abs(It->GetActorLocation().Z - Point.Z) < 800.0f)
		{
			FKGSchoolSample S;
			S.Species = It->Species;
			S.Count = It->GetFishCount();
			S.Distance = D;
			Out.Add(S);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Cast arc
// ---------------------------------------------------------------------------------------------------------------
FVector FKGCastSim::PointAt(const FVector& Origin, const FVector& Velocity, float T)
{
	return Origin + Velocity * T - FVector(0.0f, 0.0f, 0.5f * FKGFishingRules::CastGravity * T * T);
}

FKGCastResult FKGCastSim::Simulate(UWorld* World, const FVector& Origin, const FVector& Velocity, float Time,
                                    const AActor* Ignore)
{
	FKGCastResult Out;
	if (!World)
	{
		return Out;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGFishCast), false, Ignore);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FVector Prev = Origin;
	for (float T = Dt; T <= MaxFlight + 0.5f * Dt; T += Dt)
	{
		const FVector P = PointAt(Origin, Velocity, T);
		// Water crossing in this segment (fresh planes first, then the moving sea surface).
		float Cross = 2.0f;   // fraction along the segment, > 1 = none
		EKGFishWater CrossWater = EKGFishWater::None;
		float CrossZ = 0.0f;
		float FreshZ = 0.0f;
		bool bKoi = false;
		if (FKGFishingWater::FreshSurfaceAt(World, FVector2D(P), Prev.Z + 1.0f, FreshZ, bKoi) && P.Z <= FreshZ && Prev.Z >= FreshZ)
		{
			Cross = (Prev.Z - FreshZ) / FMath::Max(Prev.Z - P.Z, 0.01f);
			CrossWater = bKoi ? EKGFishWater::KoiPond : EKGFishWater::Brook;
			CrossZ = FreshZ;
		}
		else
		{
			const float SeaZ = FKGWaves::HeightAt(P.X, P.Y, Time + T);
			if (P.Z <= SeaZ)
			{
				const float SeaPrev = FKGWaves::HeightAt(Prev.X, Prev.Y, Time + T - Dt);
				Cross = FMath::Clamp((Prev.Z - SeaPrev) / FMath::Max((Prev.Z - SeaPrev) - (P.Z - SeaZ), 0.01f), 0.0f, 1.0f);
				const FVector C = FMath::Lerp(Prev, P, Cross);
				CrossWater = FKGWaves::CalmWeight(C.X, C.Y) > 0.5f ? EKGFishWater::Basin : EKGFishWater::Sea;
				CrossZ = SeaZ;
			}
		}
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByObjectType(Hit, Prev, P, Objects, Params);
		if (bHit && Hit.Time <= Cross)
		{
			Out.Landing = Hit.ImpactPoint;
			Out.FlightTime = T - Dt + Dt * Hit.Time;
			Out.Water = EKGFishWater::Land;
			Out.SurfaceZ = Hit.ImpactPoint.Z;
			return Out;
		}
		if (Cross <= 1.0f)
		{
			Out.Landing = FMath::Lerp(Prev, P, Cross);
			Out.Landing.Z = CrossZ;
			Out.FlightTime = T - Dt + Dt * Cross;
			Out.Water = CrossWater;
			Out.SurfaceZ = CrossZ;
			return Out;
		}
		Prev = P;
	}
	// Still in the air after MaxFlight (cast off a cliff): land straight below the last point.
	Out.Landing = Prev;
	Out.FlightTime = MaxFlight;
	Out.Water = FKGFishingWater::Classify(World, Prev, Time + MaxFlight, Out.SurfaceZ, Ignore);
	Out.Landing.Z = Out.SurfaceZ;
	return Out;
}

#undef LOCTEXT_NAMESPACE
