#include "Forest/KGForestRules.h"

const TCHAR* KGWolfStageName(EKGWolfStage S)
{
	switch (S)
	{
	case EKGWolfStage::Howl: return TEXT("howl");
	case EKGWolfStage::Eyes: return TEXT("eyes");
	case EKGWolfStage::Attack: return TEXT("bite");
	default: return TEXT("silent");
	}
}

const TCHAR* KGForestBandName(EKGForestBand B)
{
	switch (B)
	{
	case EKGForestBand::Village: return TEXT("village");
	case EKGForestBand::Edge: return TEXT("edge");
	case EKGForestBand::Middle: return TEXT("middle");
	case EKGForestBand::Deep: return TEXT("deep");
	default: return TEXT("out");
	}
}

// ================================================================================================ wolves
float FKGForestRules::WolfGain(const FKGWolfInputs& In)
{
	using namespace KGForest;
	if (In.bSafe || In.Band == EKGForestBand::Village || In.Band == EKGForestBand::Edge || In.Band == EKGForestBand::Out)
	{
		return -SafeDecay;
	}
	float Gain = 0.0f;
	if (In.Band == EKGForestBand::Middle)
	{
		Gain = In.bNight ? 5.0f : (In.DayIndex > 4 ? 2.0f : 0.0f);
	}
	else if (In.Band == EKGForestBand::Deep)
	{
		Gain = In.bNight ? 7.0f : 3.0f;
	}
	if (In.bRunningAstray && Gain > 0.0f)
	{
		Gain += 3.0f;
	}
	if (In.GroupSize >= 3)
	{
		Gain = 0.0f;
	}
	else if (In.GroupSize == 2)
	{
		Gain *= In.bNight ? 0.5f : 0.0f;
	}
	if (In.bTorch)
	{
		Gain *= In.bNight ? 0.5f : 0.75f;
	}
	if (In.bRawFish)
	{
		Gain *= 1.5f;
	}
	if (In.Health < 50.0f)
	{
		Gain *= 1.25f;
	}
	return Gain > 0.0f ? Gain : -IdleDecay;
}

FKGWolfStep FKGForestRules::StepWolf(FKGWolfTrack& T, float Gain, float Dt, bool bSectorHowlLive)
{
	using namespace KGForest;
	FKGWolfStep R;
	T.Interest = FMath::Clamp(T.Interest + Gain * Dt, 0.0f, AttackAt);
	T.StageAge += Dt;
	T.SinceBite += Dt;
	if (T.StampAge >= 0.0f)
	{
		T.StampAge += Dt;
	}
	auto Enter = [&T](EKGWolfStage S)
	{
		T.Stage = S;
		T.StageAge = 0.0f;
	};
	switch (T.Stage)
	{
	case EKGWolfStage::Silent:
		if (T.Interest >= HowlAt)
		{
			Enter(EKGWolfStage::Howl);
			T.StampAge = 0.0f;
			R.bStamped = true;
			R.bHowl = !bSectorHowlLive;
		}
		break;
	case EKGWolfStage::Howl:
		if (T.Interest < HowlExit)
		{
			Enter(EKGWolfStage::Silent);
			T.StampAge = -1.0f;
			R.bCalmed = true;
		}
		else if (T.Interest >= EyesAt && T.StampAge >= HowlMin)
		{
			Enter(EKGWolfStage::Eyes);
			R.bEyes = true;
		}
		break;
	case EKGWolfStage::Eyes:
		if (T.Interest < EyesExit)
		{
			Enter(EKGWolfStage::Howl);
		}
		else if (T.Interest >= AttackAt - KINDA_SMALL_NUMBER && T.StageAge >= EyesMin && T.StampAge >= TelegraphMin)
		{
			Enter(EKGWolfStage::Attack);
			R.bAttack = true;
		}
		break;
	case EKGWolfStage::Attack:
		if (T.Interest < AttackExit)
		{
			Enter(EKGWolfStage::Eyes);
		}
		break;
	}
	R.bBiteReady = T.Stage == EKGWolfStage::Attack && T.SinceBite >= BiteCooldown && T.StampAge >= TelegraphMin;
	return R;
}

void FKGForestRules::OnBite(FKGWolfTrack& T)
{
	T.SinceBite = 0.0f;
	++T.Bites;
}

void FKGForestRules::OnRepel(FKGWolfTrack& T)
{
	T.Interest = FMath::Max(0.0f, T.Interest - KGForest::RepelInterest);
}

int32 FKGForestRules::WolfCap(int32 NumPlayers)
{
	return NumPlayers >= 15 ? 6 : (NumPlayers >= 10 ? 3 : 2);
}

bool FKGForestRules::IsSectorOpen(int32 Sector, int32 NumPlayers)
{
	switch (Sector)
	{
	case 1:
	case 3: return true;                      // West + North
	case 2: return NumPlayers >= 10;          // + Cave Ridge
	case 4: return NumPlayers >= 15;          // + East Ridge
	default: return false;
	}
}

int32 FKGForestRules::PickPairTarget(float SafeDistA, float InterestA, const FString& KeyA, float SafeDistB, float InterestB,
                                     const FString& KeyB)
{
	if (!FMath::IsNearlyEqual(SafeDistA, SafeDistB, 0.5f))
	{
		return SafeDistA > SafeDistB ? 0 : 1;
	}
	if (!FMath::IsNearlyEqual(InterestA, InterestB, 0.01f))
	{
		return InterestA > InterestB ? 0 : 1;
	}
	return KeyA <= KeyB ? 0 : 1;
}

// ================================================================================================ the Mist
float FKGForestRules::MistNoticeRate(bool bNight, bool bTorch)
{
	const float Secs = (bNight ? KGForest::MistNoticeNight : KGForest::MistNoticeDay) * (bTorch ? 2.0f : 1.0f);
	return 100.0f / Secs;
}

bool FKGForestRules::MistNoticeActive(EKGForestBand Band, int32 GroupSize, bool bAfkFrozen, int32 DayIndex)
{
	return Band == EKGForestBand::Deep && GroupSize <= 1 && !bAfkFrozen && DayIndex >= 2;
}

bool FKGForestRules::StepMistNotice(FKGMistTrack& T, bool bActive, float Dt, bool bNight, bool bTorch)
{
	if (T.Stage == EKGMistStage::Tongue)
	{
		return false;
	}
	if (T.FrostAge >= 0.0f)
	{
		T.FrostAge += Dt;
	}
	if (bActive)
	{
		T.Notice = FMath::Min(100.0f, T.Notice + MistNoticeRate(bNight, bTorch) * Dt);
	}
	else
	{
		T.Notice = FMath::Max(0.0f, T.Notice - 5.0f * Dt);   // leaving the Deep band empties the meter in 20 s
	}
	if (T.Stage == EKGMistStage::None && T.Notice >= KGForest::MistFrostAt)
	{
		T.Stage = EKGMistStage::Frost;
		T.FrostAge = 0.0f;
	}
	else if (T.Stage == EKGMistStage::Frost && T.Notice <= 0.0f)
	{
		T.Stage = EKGMistStage::None;
		T.FrostAge = -1.0f;
	}
	if (T.Stage == EKGMistStage::Frost && T.Notice >= 100.0f)
	{
		T.Stage = EKGMistStage::Tongue;
		T.CoreSecs = 0.0f;
		return true;
	}
	return false;
}

float FKGForestRules::MistSpeed(float Age)
{
	return FMath::Min(KGForest::MistSpeedMax, KGForest::MistSpeed0 + KGForest::MistAccel * FMath::Max(0.0f, Age));
}

float FKGForestRules::MistDistance(float Age)
{
	using namespace KGForest;
	const float TMax = (MistSpeedMax - MistSpeed0) / MistAccel;   // 22.5 s to top speed
	const float A = FMath::Max(0.0f, Age);
	if (A <= TMax)
	{
		return MistSpeed0 * A + 0.5f * MistAccel * A * A;
	}
	return MistSpeed0 * TMax + 0.5f * MistAccel * TMax * TMax + MistSpeedMax * (A - TMax);
}

FVector2D FKGForestRules::MistSpawn(const FVector2D& Player, const FVector2D& NearestSafe, const FVector2D& ViewDir)
{
	FVector2D Away = Player - NearestSafe;
	if (!Away.Normalize())
	{
		Away = -ViewDir.GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = FVector2D(1.0f, 0.0f);
		}
	}
	const FVector2D View = ViewDir.GetSafeNormal();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(60.0f));
	FVector2D Dir = Away;
	for (int32 i = 0; i < 18 && !View.IsNearlyZero() && FVector2D::DotProduct(Dir, View) > CosCone; ++i)
	{
		// rotate away from the view, alternating sides, in 20 deg steps
		const float Deg = 20.0f * ((i / 2) + 1) * ((i % 2) ? -1.0f : 1.0f);
		Dir = Away.GetRotated(Deg);
	}
	return Player + Dir * KGForest::MistSpawnBehind;
}

bool FKGForestRules::StepMistCore(FKGMistTrack& T, float Dist, float Dt)
{
	if (Dist <= KGForest::MistCore)
	{
		T.CoreSecs += Dt;
	}
	else
	{
		T.CoreSecs = 0.0f;
	}
	return T.CoreSecs >= KGForest::MistCoreSecs;
}

int32 FKGForestRules::MistCap(int32 NumPlayers)
{
	return NumPlayers >= 15 ? 3 : (NumPlayers >= 10 ? 2 : 1);
}

// ================================================================================================ safety
bool FKGForestRules::IsPvELethal(int32 NumPlayers, int32 Alive, int32 Threats, int32 DayIndex, int32 SessionPvEDeaths,
                                 int32 SessionDeaths)
{
	if (NumPlayers < KGForest::LethalMinPlayers || IsEndgame(Alive, Threats) || DayIndex <= 1)
	{
		return false;
	}
	// F1: the next PvE death must keep the PvE share within the cap, judged over at least PvEShareWindow deaths.
	const float Denominator = static_cast<float>(FMath::Max(SessionDeaths + 1, KGForest::PvEShareWindow));
	return static_cast<float>(SessionPvEDeaths + 1) <= KGForest::PvEShareCap * Denominator + KINDA_SMALL_NUMBER;
}

// ================================================================================================ camp fire + vigil
float FKGForestRules::FireSafeRadius(float Fuel)
{
	if (Fuel <= 0.0f)
	{
		return 0.0f;
	}
	return Fuel < KGForest::FireLowFuel ? KGForest::FireSafeLow : KGForest::FireSafe;
}

float FKGForestRules::VigilArriveBy(float NightSecs)
{
	return FMath::Max(KGForest::VigilArriveShare * NightSecs, KGForest::VigilArriveMin);
}

FKGVigilCaps FKGForestRules::VigilCaps(int32 NumPlayers)
{
	FKGVigilCaps C;
	if (NumPlayers >= 16)
	{
		C.PerSigner = 2;
		C.NightCap = 3;
		C.MatchCap = 6;
	}
	else if (NumPlayers >= 10)
	{
		C.PerSigner = 1;
		C.NightCap = 2;
		C.MatchCap = 4;
	}
	return C;
}

bool FKGForestRules::VigilSuccess(const FKGVigilNight& N)
{
	return N.Signers >= VigilMinSigners && N.bAllArrived && N.MaxOutsideSecs <= KGForest::VigilOutsideMax &&
	       N.LitShare >= KGForest::VigilLitShare;
}

int32 FKGForestRules::VigilUnits(const FKGVigilNight& Night, int32 NumPlayers, int32 MatchGranted, int32 OpenLivingTownTasks)
{
	if (!VigilSuccess(Night))
	{
		return 0;
	}
	const FKGVigilCaps C = VigilCaps(NumPlayers);
	const int32 Signers = FMath::Min(Night.Signers, VigilMaxSigners(NumPlayers));
	int32 Units = FMath::Min(Signers * C.PerSigner, C.NightCap);
	Units = FMath::Min(Units, FMath::Max(0, C.MatchCap - MatchGranted));
	return FMath::Clamp(Units, 0, FMath::Max(0, OpenLivingTownTasks));
}
