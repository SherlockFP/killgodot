#include "Dig/KGDigTypes.h"

namespace KGDigTypesPrivate
{
	constexpr float M = 100.0f;   // layout metres -> cm

	FKGDigZoneDef Poly(EKGDigZone Zone, std::initializer_list<FVector2D> Metres, int32 Mounds, int32 XMarks, int32 Glints,
	                   int32 Treasures, float MinZ = -100000.0f, float MaxZ = 100000.0f)
	{
		FKGDigZoneDef Z;
		Z.Zone = Zone;
		for (const FVector2D& P : Metres)
		{
			Z.Polygon.Add(P * M);
		}
		Z.Mounds = Mounds;
		Z.XMarks = XMarks;
		Z.Glints = Glints;
		Z.Treasures = Treasures;
		Z.MinZ = MinZ;
		Z.MaxZ = MaxZ;
		return Z;
	}

	FKGDigZoneDef Circle(EKGDigZone Zone, const FVector2D& CentreM, float RadiusM, int32 Mounds, int32 XMarks, int32 Glints,
	                     int32 Treasures)
	{
		FKGDigZoneDef Z;
		Z.Zone = Zone;
		Z.Centre = CentreM * M;
		Z.Radius = RadiusM * M;
		Z.Mounds = Mounds;
		Z.XMarks = XMarks;
		Z.Glints = Glints;
		Z.Treasures = Treasures;
		return Z;
	}

	bool PointInPolygon(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		bool bInside = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}
}

bool FKGDigZoneDef::Contains(const FVector2D& P) const
{
	if (Polygon.Num() >= 3)
	{
		return KGDigTypesPrivate::PointInPolygon(Polygon, P);
	}
	return Radius > 0.0f && FVector2D::DistSquared(P, Centre) <= FMath::Square(Radius);
}

FBox2D FKGDigZoneDef::Bounds() const
{
	if (Polygon.Num() >= 3)
	{
		return FBox2D(Polygon);
	}
	return FBox2D(Centre - FVector2D(Radius), Centre + FVector2D(Radius));
}

uint8 FKGDigRules::StagesFor(EKGDigKind Kind)
{
	switch (Kind)
	{
	case EKGDigKind::Glint: return 1;
	case EKGDigKind::Grave: return 4;
	case EKGDigKind::Treasure: return 4;
	default: return 3;
	}
}

float FKGDigRules::HoldSeconds(EKGDigKind Kind)
{
	switch (Kind)
	{
	case EKGDigKind::Glint: return 0.9f;
	case EKGDigKind::Grave: return 1.4f;
	case EKGDigKind::Treasure: return 1.3f;
	default: return 1.1f;
	}
}

FName FKGDigRules::LootTable(EKGDigKind Kind, uint8 StageDone, uint8 MaxStage)
{
	if (StageDone < MaxStage)
	{
		switch (Kind)
		{
		case EKGDigKind::Grave: return TEXT("DigGraveShallow");
		case EKGDigKind::Treasure: return NAME_None;   // nothing until the lid shows
		default: return TEXT("DigShallow");
		}
	}
	switch (Kind)
	{
	case EKGDigKind::XMark: return TEXT("DigX");
	case EKGDigKind::Glint: return TEXT("DigGlint");
	case EKGDigKind::Grave: return TEXT("DigGrave");
	case EKGDigKind::Treasure: return TEXT("BuriedChest");
	default: return TEXT("DigMound");
	}
}

FKGRng FKGDigRules::StageRng(uint64 MatchSeed, uint16 SpotId, uint8 Stage)
{
	const uint64 Mix = MatchSeed ^ (static_cast<uint64>(SpotId) << 32) ^ (static_cast<uint64>(Stage) << 52) ^ 0xD16D16D1ull;
	return FKGRng(Mix, 0xD160u + SpotId);
}

bool FKGDigRules::CanDigInPhase(EKGPhase Phase)
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

const TCHAR* FKGDigRules::KindName(EKGDigKind Kind)
{
	switch (Kind)
	{
	case EKGDigKind::XMark: return TEXT("X mark");
	case EKGDigKind::Glint: return TEXT("Glint");
	case EKGDigKind::Grave: return TEXT("Grave");
	case EKGDigKind::Treasure: return TEXT("Buried chest");
	default: return TEXT("Mound");
	}
}

const TCHAR* FKGDigRules::ZoneName(EKGDigZone Zone)
{
	switch (Zone)
	{
	case EKGDigZone::Graveyard: return TEXT("Graveyard");
	case EKGDigZone::Beach: return TEXT("Beach");
	case EKGDigZone::Farm: return TEXT("Farm");
	case EKGDigZone::Forest: return TEXT("Forest");
	case EKGDigZone::Orchard: return TEXT("Orchard");
	default: return TEXT("-");
	}
}

const TArray<FKGDigZoneDef>& FKGDigRules::MorrowmereV2Zones()
{
	using namespace KGDigTypesPrivate;
	static const TArray<FKGDigZoneDef> Zones = []
	{
		// Metres, UE frame, from Tools/Level/morrowmere_layout_v2.json (landmarks.graveyard, fields[], coast) and the
		// v2 terrain heights: the shingle beach runs y 58..68 west of the brook mouth at 0.4..2.6 m.
		TArray<FKGDigZoneDef> Z;
		Z.Add(Poly(EKGDigZone::Graveyard, {{-39.7, -49.13}, {-20.25, -53.78}, {-24.44, -71.29}, {-43.89, -66.63}}, 2, 0, 1, 0));
		Z.Add(Poly(EKGDigZone::Beach, {{-125.0, 58.0}, {-50.0, 58.0}, {-50.0, 68.0}, {-125.0, 68.0}}, 3, 1, 2, 1, 40.0f, 260.0f));
		Z.Add(Poly(EKGDigZone::Farm, {{36.0, -112.0}, {57.0, -112.0}, {57.5, -88.0}, {38.0, -86.0}}, 2, 0, 0, 0));
		Z.Add(Poly(EKGDigZone::Farm, {{64.0, -112.0}, {94.0, -110.0}, {92.0, -86.0}, {66.0, -88.0}}, 1, 0, 1, 0));
		Z.Add(Poly(EKGDigZone::Farm, {{98.0, -84.0}, {116.0, -84.0}, {116.0, -68.0}, {104.0, -70.0}}, 1, 0, 0, 0));
		Z.Add(Circle(EKGDigZone::Forest, {-118.0, -48.0}, 12.0f, 2, 1, 0, 1));
		Z.Add(Circle(EKGDigZone::Forest, {-20.0, -108.0}, 12.0f, 2, 0, 1, 1));
		Z.Add(Circle(EKGDigZone::Forest, {-78.0, -86.0}, 8.0f, 1, 0, 0, 0));
		Z.Add(Poly(EKGDigZone::Orchard, {{80.0, -36.0}, {112.0, -40.0}, {114.0, 8.0}, {88.0, 12.0}, {84.0, -8.0}}, 2, 1, 0, 1));
		return Z;
	}();
	return Zones;
}

FKGDigSpot FKGDigRules::MakeSpot(uint16 Id, EKGDigKind Kind, EKGDigZone Zone, const FVector& Ground, float YawDeg)
{
	FKGDigSpot S;
	S.Id = Id;
	S.Kind = Kind;
	S.Zone = Zone;
	S.Stage = 0;
	S.MaxStage = StagesFor(Kind);
	S.Location = Ground;
	S.YawQ = static_cast<uint8>(FMath::RoundToInt(FRotator::ClampAxis(YawDeg) / 360.0f * 256.0f) & 0xFF);
	return S;
}

void FKGDigRules::Generate(FKGRng& Rng, const TArray<FKGDigZoneDef>& Zones, FKGDigProbe Probe, const TArray<FVector>& Avoid,
                           TArray<FKGDigSpot>& OutSpots, TArray<FKGDigSpot>& OutBuried, uint16& NextId)
{
	constexpr int32 Attempts = 40;
	auto FarEnough = [&](const FVector& P, EKGDigKind Kind)
	{
		for (const FVector& A : Avoid)
		{
			if (FVector::DistSquared2D(P, A) < FMath::Square(GraveClearance))
			{
				return false;
			}
		}
		for (const TArray<FKGDigSpot>* List : {&OutSpots, &OutBuried})
		{
			for (const FKGDigSpot& S : *List)
			{
				const float Min = Kind == EKGDigKind::Treasure && S.Kind == EKGDigKind::Treasure ? TreasureSpacing : MinSpacing;
				if (FVector::DistSquared2D(P, FVector(S.Location)) < FMath::Square(Min))
				{
					return false;
				}
			}
		}
		return true;
	};
	for (const FKGDigZoneDef& Zone : Zones)
	{
		const FBox2D Box = Zone.Bounds();
		const TPair<EKGDigKind, int32> Wanted[] = {{EKGDigKind::Treasure, Zone.Treasures}, {EKGDigKind::XMark, Zone.XMarks},
		                                           {EKGDigKind::Mound, Zone.Mounds}, {EKGDigKind::Glint, Zone.Glints}};
		for (const TPair<EKGDigKind, int32>& W : Wanted)
		{
			for (int32 n = 0; n < W.Value; ++n)
			{
				for (int32 Try = 0; Try < Attempts; ++Try)
				{
					FVector2D XY;
					if (Zone.Polygon.Num() >= 3)
					{
						XY = FVector2D(FMath::Lerp(Box.Min.X, Box.Max.X, static_cast<double>(Rng.FRand())),
						               FMath::Lerp(Box.Min.Y, Box.Max.Y, static_cast<double>(Rng.FRand())));
					}
					else
					{
						const float R = Zone.Radius * FMath::Sqrt(Rng.FRand());
						const float A = Rng.FRand() * UE_TWO_PI;
						XY = Zone.Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
					}
					const float Yaw = Rng.FRand() * 360.0f;
					if (!Zone.Contains(XY))
					{
						continue;
					}
					FVector Ground;
					if (!Probe(XY, Ground) || Ground.Z < Zone.MinZ || Ground.Z > Zone.MaxZ || !FarEnough(Ground, W.Key))
					{
						continue;
					}
					if (NextId == 0)
					{
						NextId = 1;
					}
					const FKGDigSpot Spot = MakeSpot(NextId++, W.Key, Zone.Zone, Ground, Yaw);
					(W.Key == EKGDigKind::Treasure ? OutBuried : OutSpots).Add(Spot);
					break;
				}
			}
		}
	}
}

int32 FKGDigRules::TreasureFor(uint32 PlayerHash, int32 MapIndex, int32 NumUndug)
{
	if (NumUndug <= 0 || MapIndex < 0)
	{
		return INDEX_NONE;
	}
	return static_cast<int32>((PlayerHash + static_cast<uint32>(MapIndex)) % static_cast<uint32>(NumUndug));
}
