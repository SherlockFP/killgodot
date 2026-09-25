#include "World/KGMapInfo.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "World/KGWaves.h"

bool FKGMapRegion::Contains(const FVector2D& P) const
{
	const int32 N = Polygon.Num();
	if (N >= 3)
	{
		bool bInside = false;
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[j];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}
	return Radius > 0.0f && FVector2D::DistSquared(P, Center) <= FMath::Square(Radius);
}

double FKGMapRegion::Area() const
{
	const int32 N = Polygon.Num();
	if (N >= 3)
	{
		double A = 0.0;
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			A += Polygon[j].X * Polygon[i].Y - Polygon[i].X * Polygon[j].Y;
		}
		return FMath::Abs(A) * 0.5;
	}
	return UE_DOUBLE_PI * Radius * Radius;
}

AKGMapInfo::AKGMapInfo()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = true;
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;   // World Partition must never stream the map data out
#endif
}

AKGMapInfo* AKGMapInfo::Find(const UWorld* World)
{
	static TMap<TObjectKey<UWorld>, TWeakObjectPtr<AKGMapInfo>> Cache;
	if (!World)
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<AKGMapInfo>* Hit = Cache.Find(World); Hit && Hit->IsValid())
	{
		return Hit->Get();
	}
	for (TActorIterator<AKGMapInfo> It(const_cast<UWorld*>(World)); It; ++It)
	{
		Cache.Add(World, *It);
		return *It;
	}
	return nullptr;
}

int32 AKGMapInfo::FindRegionAt(const FVector2D& P) const
{
	int32 Best = INDEX_NONE;
	int32 BestLayer = -1;
	double BestArea = 0.0;
	for (int32 i = 0; i < Regions.Num(); ++i)
	{
		const FKGMapRegion& R = Regions[i];
		if (R.Layer < 0 || R.Layer < BestLayer || !R.Contains(P))
		{
			continue;
		}
		const double A = R.Area();
		if (R.Layer > BestLayer || A < BestArea)
		{
			Best = i;
			BestLayer = R.Layer;
			BestArea = A;
		}
	}
	return Best;
}

void AKGMapInfo::ApplyWater()
{
	FKGWaves::FCalmZone& Zone = FKGWaves::Calm();
	Zone = FKGWaves::FCalmZone();
	if (bCalmWater)
	{
		Zone.CentreX = CalmCentre.X;
		Zone.CentreY = CalmCentre.Y;
		Zone.Radius = CalmRadius;
		Zone.Fade = CalmFade;
		Zone.Scale = CalmWaveScale;
		Zone.RippleAmp = CalmRipple;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!WaterParameters)
	{
		// Non-static lookup (Live Coding keeps static locals); the ocean material references the MPC, so it is cooked.
		WaterParameters = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/KillGodot/Materials/MPC_KG_Water.MPC_KG_Water"),
		                                                           nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	UMaterialParameterCollectionInstance* Inst = WaterParameters ? World->GetParameterCollectionInstance(WaterParameters) : nullptr;
	if (!Inst)
	{
		return;
	}
	// Same layout as FKGWaves::FCalmZone (see Tools/Unreal/kg_make_ocean.py).
	Inst->SetVectorParameterValue(TEXT("CalmZone"), FLinearColor(Zone.CentreX, Zone.CentreY, Zone.Radius, Zone.Fade));
	Inst->SetVectorParameterValue(TEXT("CalmParams"), FLinearColor(Zone.Scale, Zone.RippleAmp, bCalmWater ? CalmTintAmount : 0.0f, 0.0f));
	Inst->SetVectorParameterValue(TEXT("CalmTint"), CalmTint);
}

void AKGMapInfo::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	ApplyWater();   // editor worlds too, so the viewport shows the calm basin
}

void AKGMapInfo::BeginPlay()
{
	Super::BeginPlay();
	ApplyWater();
}

void AKGMapInfo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (bCalmWater)
	{
		FKGWaves::Calm() = FKGWaves::FCalmZone();   // the next map starts with the open sea
	}
}

#if WITH_EDITOR
void AKGMapInfo::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	ApplyWater();
}
#endif
