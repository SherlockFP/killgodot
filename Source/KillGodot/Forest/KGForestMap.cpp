#include "Forest/KGForestMap.h"

#include "Dom/JsonObject.h"
#include "KillGodot.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace KGForestData
{
#include "Forest/KGForestData.gen.inl"

	FString Embedded()
	{
		FString S;
		for (const TCHAR* Chunk : GKGForestJsonChunks)
		{
			S += Chunk;
		}
		return S;
	}

	FString DiskPath()
	{
		return FPaths::ProjectDir() / TEXT("Tools/Level/morrowmere_forest_v2.json");
	}

	FKGForestMap& Mutable()
	{
		static FKGForestMap Map;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Json, Error;
			// Dev: the JSON next to the tools wins (tuning the forest needs no C++ rebuild).
			if (!FFileHelper::LoadFileToString(Json, *DiskPath()) || !Map.Parse(Json, Error))
			{
				Map = FKGForestMap();
				if (!Map.Parse(Embedded(), Error))
				{
					UE_LOG(LogKillGodot, Warning, TEXT("KG_FOREST data broken: %s"), *Error);
				}
			}
		}
		return Map;
	}
}

const FKGForestMap& FKGForestMap::Get()
{
	return KGForestData::Mutable();
}

bool FKGForestMap::Reload(FString& OutMessage)
{
	FString Json, Error;
	if (!FFileHelper::LoadFileToString(Json, *KGForestData::DiskPath()))
	{
		Json = KGForestData::Embedded();
	}
	FKGForestMap Fresh;
	if (!Fresh.Parse(Json, Error))
	{
		OutMessage = Error;
		return false;
	}
	KGForestData::Mutable() = MoveTemp(Fresh);
	OutMessage = FString::Printf(TEXT("%dx%d cells, %d trails, %d dens"), Get().NX, Get().NY, Get().Trails.Num(), Get().Dens.Num());
	return true;
}

namespace
{
	FVector2D Vec2(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector;
	}
}

bool FKGForestMap::Parse(const FString& Json, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("not JSON");
		return false;
	}
	Cell = Root->GetNumberField(TEXT("cell"));
	X0 = Root->GetNumberField(TEXT("x0"));
	Y0 = Root->GetNumberField(TEXT("y0"));
	NX = Root->GetIntegerField(TEXT("nx"));
	NY = Root->GetIntegerField(TEXT("ny"));
	const TSharedPtr<FJsonObject>* Consts = nullptr;
	if (Root->TryGetObjectField(TEXT("consts"), Consts))
	{
		ROut = (*Consts)->GetNumberField(TEXT("r_out"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("square"), Arr))
	{
		Square = Vec2(*Arr);
	}
	const TSharedPtr<FJsonObject>* CampObj = nullptr;
	if (Root->TryGetObjectField(TEXT("camp"), CampObj))
	{
		Camp = Vec2((*CampObj)->GetArrayField(TEXT("at")));
		CampZ = (*CampObj)->GetNumberField(TEXT("z"));
	}
	Trails.Reset();
	if (Root->TryGetArrayField(TEXT("trails"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FKGForestTrail T;
			T.Name = FName(*O->GetStringField(TEXT("name")));
			T.Width = O->GetNumberField(TEXT("width"));
			for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("points")))
			{
				T.Points.Add(Vec2(P->AsArray()));
			}
			Trails.Add(MoveTemp(T));
		}
	}
	Dens.Reset();
	if (Root->TryGetArrayField(TEXT("dens"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FKGForestDen D;
			D.Id = FName(*O->GetStringField(TEXT("id")));
			const FString S = O->GetStringField(TEXT("sector"));
			for (int32 i = 1; i <= 4; ++i)
			{
				if (SectorName(i) == S)
				{
					D.Sector = i;
				}
			}
			D.At = Vec2(O->GetArrayField(TEXT("at")));
			Dens.Add(D);
		}
	}
	Lanterns.Reset();
	if (Root->TryGetArrayField(TEXT("lanterns"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			Lanterns.Add(Vec2(V->AsObject()->GetArrayField(TEXT("at"))));
		}
	}
	if (!FBase64::Decode(Root->GetStringField(TEXT("raster")), Flags) ||
	    !FBase64::Decode(Root->GetStringField(TEXT("sectors")), Sectors) || Flags.Num() != NX * NY ||
	    Sectors.Num() != NX * NY)
	{
		OutError = FString::Printf(TEXT("raster size %d / %d != %d x %d"), Flags.Num(), Sectors.Num(), NX, NY);
		NX = NY = 0;
		return false;
	}
	BuildNearestSafe();
	RefineNearestSafe();
	return true;
}

int32 FKGForestMap::IndexOf(const FVector2D& M) const
{
	const int32 I = FMath::FloorToInt((M.X - X0) / Cell);
	const int32 J = FMath::FloorToInt((M.Y - Y0) / Cell);
	return (I >= 0 && I < NX && J >= 0 && J < NY) ? J * NX + I : INDEX_NONE;
}

FVector2D FKGForestMap::CentreOf(int32 Index) const
{
	return FVector2D(X0 + Cell * ((Index % NX) + 0.5f), Y0 + Cell * ((Index / NX) + 0.5f));
}

uint8 FKGForestMap::FlagsAt(const FVector2D& M) const
{
	const int32 I = IndexOf(M);
	return I == INDEX_NONE ? uint8(uint8(EKGForestBand::Out) | KGForestFlag::NotWalkable) : Flags[I];
}

int32 FKGForestMap::SectorAt(const FVector2D& M) const
{
	const int32 I = IndexOf(M);
	return I == INDEX_NONE ? 0 : Sectors[I];
}

void FKGForestMap::BuildNearestSafe()
{
	// Multi-source BFS from every walkable safe cell; each cell remembers the source that reached it first.
	NearestSafeIndex.Init(INDEX_NONE, NX * NY);
	TArray<int32> Queue;
	Queue.Reserve(NX * NY);
	for (int32 i = 0; i < Flags.Num(); ++i)
	{
		if ((Flags[i] & KGForestFlag::Safe) && !(Flags[i] & KGForestFlag::NotWalkable))
		{
			NearestSafeIndex[i] = i;
			Queue.Add(i);
		}
	}
	for (int32 h = 0; h < Queue.Num(); ++h)
	{
		const int32 C = Queue[h];
		const int32 CI = C % NX, CJ = C / NX;
		for (int32 dj = -1; dj <= 1; ++dj)
		{
			for (int32 di = -1; di <= 1; ++di)
			{
				const int32 NI = CI + di, NJ = CJ + dj;
				if ((di || dj) && NI >= 0 && NI < NX && NJ >= 0 && NJ < NY)
				{
					const int32 N = NJ * NX + NI;
					if (NearestSafeIndex[N] == INDEX_NONE)
					{
						NearestSafeIndex[N] = NearestSafeIndex[C];
						Queue.Add(N);
					}
				}
			}
		}
	}
}

void FKGForestMap::RefineNearestSafe()
{
	// BFS order is not Euclidean: sweep the grid (forward, backward, twice) letting each cell adopt a neighbour's source
	// when that source is closer - a sequential Voronoi propagation, close to the exact nearest safe cell.
	auto D2 = [this](int32 Cell, int32 Src)
	{
		const int32 DX = (Cell % NX) - (Src % NX), DY = (Cell / NX) - (Src / NX);
		return DX * DX + DY * DY;
	};
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		const bool bFwd = Pass % 2 == 0;
		for (int32 k = 0; k < NX * NY; ++k)
		{
			const int32 C = bFwd ? k : NX * NY - 1 - k;
			const int32 CI = C % NX, CJ = C / NX;
			int32 Best = NearestSafeIndex[C];
			int32 BestD = Best == INDEX_NONE ? MAX_int32 : D2(C, Best);
			for (int32 dj = -1; dj <= 1; ++dj)
			{
				for (int32 di = -1; di <= 1; ++di)
				{
					const int32 NI = CI + di, NJ = CJ + dj;
					if (NI < 0 || NI >= NX || NJ < 0 || NJ >= NY)
					{
						continue;
					}
					const int32 Src = NearestSafeIndex[NJ * NX + NI];
					if (Src != INDEX_NONE && D2(C, Src) < BestD)
					{
						Best = Src;
						BestD = D2(C, Src);
					}
				}
			}
			NearestSafeIndex[C] = Best;
		}
	}
}

bool FKGForestMap::NearestSafe(const FVector2D& M, FVector2D& Out) const
{
	const int32 I = IndexOf(M);
	if (I == INDEX_NONE || !NearestSafeIndex.IsValidIndex(I) || NearestSafeIndex[I] == INDEX_NONE)
	{
		return false;
	}
	Out = CentreOf(NearestSafeIndex[I]);
	return true;
}

float FKGForestMap::SafeDistance(const FVector2D& M) const
{
	if (IsStaticSafe(M))
	{
		return 0.0f;
	}
	FVector2D S;
	return NearestSafe(M, S) ? FVector2D::Distance(M, S) : 1000.0f;
}

FString FKGForestMap::SectorName(int32 Sector)
{
	static const TCHAR* Names[] = {TEXT(""), TEXT("West"), TEXT("CaveRidge"), TEXT("North"), TEXT("EastRidge")};
	return (Sector >= 0 && Sector <= 4) ? Names[Sector] : TEXT("");
}
