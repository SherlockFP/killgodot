#include "Chores/KGChoreTypes.h"

float FKGChoreDef::MinTotalSeconds() const
{
	float Sum = 0.0f;
	for (const float S : StageMinSeconds)
	{
		Sum += S;
	}
	return Sum;
}

const TArray<FKGChoreDef>& FKGChoreCatalog::GetAll()
{
	// Mirrors Tools/Level/morrowmere_layout_v2.json "tasks" (18 + 4 optional). Stage floors are the server's
	// anti-cheat minimums (about 40% of a typical run); tune them together with the minigame, never above what a
	// skilled player can do.
	static const TArray<FKGChoreDef> All = []
	{
		TArray<FKGChoreDef> Out;
		auto Add = [&Out](const TCHAR* Id, const TCHAR* Title, TArray<FString> Stages, TArray<float> Mins,
		                  EKGChoreFx Fx = EKGChoreFx::None, bool bOptional = false)
		{
			check(Stages.Num() == Mins.Num());
			FKGChoreDef& D = Out.AddDefaulted_GetRef();
			D.Id = FName(Id);
			D.Title = Title;
			D.Stages = MoveTemp(Stages);
			D.StageMinSeconds = MoveTemp(Mins);
			D.Fx = Fx;
			D.bOptional = bOptional;
		};
		Add(TEXT("DrawWater"), TEXT("Draw water from the well"), {TEXT("Crank"), TEXT("Carry")}, {5.0f, 8.0f});
		Add(TEXT("PostNotice"), TEXT("Post the notice"), {TEXT("Place"), TEXT("Pin"), TEXT("Smooth")}, {1.2f, 2.5f, 2.0f},
		    EKGChoreFx::NoticePosted);
		Add(TEXT("FileReports"), TEXT("File the reports"), {TEXT("Sort"), TEXT("Sign")}, {7.0f, 2.5f});
		Add(TEXT("RingBell"), TEXT("Ring the church bell"), {TEXT("Ring")}, {3.0f}, EKGChoreFx::BellRing);
		Add(TEXT("LightCandles"), TEXT("Light the church candles"), {TEXT("Row one"), TEXT("Row two")}, {4.0f, 5.0f},
		    EKGChoreFx::CandlesLit);
		Add(TEXT("TendGraves"), TEXT("Tend the graves"), {TEXT("Weed"), TEXT("Flowers")}, {6.0f, 3.0f});
		Add(TEXT("ForgeNails"), TEXT("Forge nails"), {TEXT("Heat"), TEXT("Hammer")}, {6.5f, 3.0f});
		Add(TEXT("SharpenTools"), TEXT("Sharpen the tools"), {TEXT("Axe"), TEXT("Scythe")}, {4.5f, 4.5f});
		Add(TEXT("BakeBread"), TEXT("Bake bread"), {TEXT("Knead"), TEXT("Shape"), TEXT("Bake")}, {3.5f, 2.0f, 6.0f},
		    EKGChoreFx::ChimneySmoke);
		Add(TEXT("PourAle"), TEXT("Pour ale"), {TEXT("First mug"), TEXT("Second mug")}, {3.0f, 3.5f});
		Add(TEXT("StockStall"), TEXT("Stock the market stall"), {TEXT("Goods"), TEXT("Prices")}, {4.0f, 4.0f});
		Add(TEXT("MendNets"), TEXT("Mend the fishing nets"), {TEXT("First tear"), TEXT("Second tear")}, {2.0f, 2.4f});
		Add(TEXT("UnloadFish"), TEXT("Unload the fish"), {TEXT("Sort")}, {11.0f});
		Add(TEXT("FuelLighthouse"), TEXT("Refuel the lighthouse"), {TEXT("Oil"), TEXT("Wick")}, {4.0f, 3.5f},
		    EKGChoreFx::LighthouseGlow);
		Add(TEXT("HarvestCarrots"), TEXT("Harvest carrots"), {TEXT("Harvest")}, {8.0f});
		Add(TEXT("FeedAnimals"), TEXT("Fill the feed trough"), {TEXT("Scoop")}, {7.5f});
		Add(TEXT("ChopWood"), TEXT("Chop firewood"), {TEXT("Split")}, {5.0f}, EKGChoreFx::WoodPile);
		Add(TEXT("FixBoat"), TEXT("Tar the boat"), {TEXT("Port side"), TEXT("Starboard")}, {4.0f, 5.0f});
		Add(TEXT("WindClock"), TEXT("Wind the town clock"), {TEXT("Wind"), TEXT("Set")}, {4.0f, 2.0f}, EKGChoreFx::ClockChime, true);
		Add(TEXT("FeedKoi"), TEXT("Feed the koi"), {TEXT("Feed")}, {6.0f}, EKGChoreFx::None, true);
		Add(TEXT("LightHarbourLamp"), TEXT("Light the harbour lamp"), {TEXT("Strike"), TEXT("Light")}, {1.0f, 3.0f},
		    EKGChoreFx::LampLit, true);
		Add(TEXT("GrindFlour"), TEXT("Grind flour"), {TEXT("Fill"), TEXT("Grind")}, {2.4f, 7.0f}, EKGChoreFx::None, true);
		return Out;
	}();
	return All;
}

const FKGChoreDef* FKGChoreCatalog::Find(FName Id)
{
	for (const FKGChoreDef& D : GetAll())
	{
		if (D.Id == Id)
		{
			return &D;
		}
	}
	return nullptr;
}

FString FKGChoreCatalog::FxName(EKGChoreFx Fx)
{
	switch (Fx)
	{
	case EKGChoreFx::BellRing: return TEXT("bell rings village-wide");
	case EKGChoreFx::LighthouseGlow: return TEXT("lighthouse lamp blazes");
	case EKGChoreFx::ChimneySmoke: return TEXT("bakery chimney smokes");
	case EKGChoreFx::NoticePosted: return TEXT("notice appears on the board");
	case EKGChoreFx::CandlesLit: return TEXT("church candles glow");
	case EKGChoreFx::LampLit: return TEXT("harbour lamp lights up");
	case EKGChoreFx::ClockChime: return TEXT("town clock chimes");
	case EKGChoreFx::WoodPile: return TEXT("log pile grows");
	default: return TEXT("-");
	}
}

bool FKGChoreRules::PhaseAllowsChores(EKGPhase Phase)
{
	switch (Phase)
	{
	case EKGPhase::Meeting:
	case EKGPhase::Trial:
	case EKGPhase::Epilogue:
	case EKGPhase::Lobby:
	case EKGPhase::Warmup:
	case EKGPhase::Migrating:
		return false;
	default:
		return true;
	}
}

EKGChoreVerdict FKGChoreRules::CheckStage(const FKGChoreDef& Def, FName ReportedChore, int32 ReportedStage, int32 ReportedToken,
                                          FName SessionChore, int32 SessionStage, int32 SessionToken, float ServerStageSeconds)
{
	if (SessionChore.IsNone())
	{
		return EKGChoreVerdict::NoSession;
	}
	if (ReportedChore != SessionChore || Def.Id != SessionChore)
	{
		return EKGChoreVerdict::WrongChore;
	}
	if (ReportedToken != SessionToken)
	{
		return EKGChoreVerdict::WrongToken;
	}
	if (ReportedStage != SessionStage || !Def.StageMinSeconds.IsValidIndex(ReportedStage))
	{
		return EKGChoreVerdict::WrongStage;
	}
	if (ServerStageSeconds < Def.StageMinSeconds[ReportedStage] * TimingSlack)
	{
		return EKGChoreVerdict::TooFast;
	}
	return EKGChoreVerdict::Accepted;
}

const TCHAR* FKGChoreRules::VerdictName(EKGChoreVerdict Verdict)
{
	switch (Verdict)
	{
	case EKGChoreVerdict::Accepted: return TEXT("Accepted");
	case EKGChoreVerdict::NoSession: return TEXT("NoSession");
	case EKGChoreVerdict::WrongChore: return TEXT("WrongChore");
	case EKGChoreVerdict::WrongToken: return TEXT("WrongToken");
	case EKGChoreVerdict::WrongStage: return TEXT("WrongStage");
	case EKGChoreVerdict::TooFast: return TEXT("TooFast");
	case EKGChoreVerdict::Interrupted: return TEXT("Interrupted");
	default: return TEXT("?");
	}
}

const TCHAR* FKGChoreRules::CloseName(EKGChoreClose Reason)
{
	switch (Reason)
	{
	case EKGChoreClose::Completed: return TEXT("Completed");
	case EKGChoreClose::Left: return TEXT("Left");
	case EKGChoreClose::Moved: return TEXT("Moved");
	case EKGChoreClose::Hit: return TEXT("Hit");
	case EKGChoreClose::Phase: return TEXT("Phase");
	case EKGChoreClose::Died: return TEXT("Died");
	case EKGChoreClose::Replaced: return TEXT("Replaced");
	default: return TEXT("?");
	}
}
