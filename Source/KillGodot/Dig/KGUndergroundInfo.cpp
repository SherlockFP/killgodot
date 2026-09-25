#include "Dig/KGUndergroundInfo.h"
#include "Audio/KGAudio.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AKGUndergroundInfo::AKGUndergroundInfo()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = false;
	bNetLoadOnClient = true;
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;   // World Partition must never stream the plan out
#endif
}

AKGUndergroundInfo* AKGUndergroundInfo::Find(const UWorld* World)
{
	static TMap<TObjectKey<UWorld>, TWeakObjectPtr<AKGUndergroundInfo>> Cache;
	if (!World)
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<AKGUndergroundInfo>* Hit = Cache.Find(World); Hit && Hit->IsValid())
	{
		return Hit->Get();
	}
	for (TActorIterator<AKGUndergroundInfo> It(const_cast<UWorld*>(World)); It; ++It)
	{
		Cache.Add(World, *It);
		return *It;
	}
	return nullptr;
}

bool AKGUndergroundInfo::Contains(const FVector& P) const
{
	for (const FBox& Box : Volumes)
	{
		if (Box.IsInsideOrOn(P))
		{
			return true;
		}
	}
	return false;
}

bool AKGUndergroundInfo::IsBelowGround(const UWorld* World, const FVector& P)
{
	const AKGUndergroundInfo* Info = Find(World);
	return Info && Info->Contains(P);
}

FString AKGUndergroundInfo::RegionNameAt(const FVector& P) const
{
	if (!Contains(P))
	{
		return FString();
	}
	int32 Best = INDEX_NONE;
	int32 BestLayer = -1;
	double BestArea = 0.0;
	const FVector2D XY(P);
	for (int32 i = 0; i < Regions.Num(); ++i)
	{
		const FKGMapRegion& R = Regions[i];
		if (R.Layer < 0 || R.Layer < BestLayer || !R.Contains(XY))
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
	return Regions.IsValidIndex(Best) ? Regions[Best].Name.ToString() : FString(TEXT("Underground"));
}

AKGMapInfo* AKGUndergroundInfo::GetPlan()
{
	if (IsValid(Plan))
	{
		return Plan;
	}
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return nullptr;
	}
	AKGMapInfo* Surface = AKGMapInfo::Find(World);   // resolve (and cache) the level's own map info first
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AKGMapInfo* Info = World->SpawnActor<AKGMapInfo>(AKGMapInfo::StaticClass(), FTransform::Identity, Params);
	if (!Info)
	{
		return nullptr;
	}
	Info->MapTexture = MapTexture;
	Info->WorldMin = WorldMin;
	Info->WorldMax = WorldMax;
	Info->MapTitle = MapTitle.IsEmpty() ? FText::FromString(TEXT("Underground")) : MapTitle;
	Info->Regions = Regions;
	Info->FinishSpawning(FTransform::Identity);
	// Its BeginPlay pushed "no calm basin" into FKGWaves / MPC_KG_Water: the level's own map info has the last word.
	if (Surface && Surface != Info)
	{
		Surface->ApplyWater();
	}
	Plan = Info;
	return Plan;
}

void AKGUndergroundInfo::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	Look = NewObject<UPostProcessComponent>(this, TEXT("KGUnderLook"));
	Look->SetupAttachment(GetRootComponent());
	Look->bUnbound = true;
	Look->Priority = 10.0f;
	Look->BlendWeight = 0.0f;
	FPostProcessSettings& S = Look->Settings;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = Vignette;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.9, 0.9, 0.95, 1.0);
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FLinearColor(0.93f, 0.96f, 1.0f);
	Look->RegisterComponent();
	Ambience = NewObject<UAudioComponent>(this, TEXT("KGUnderAmbience"));
	Ambience->SetupAttachment(GetRootComponent());
	Ambience->bAutoActivate = false;
	Ambience->bIsUISound = true;   // 2D bed; the positional drips come on top
	Ambience->SetSound(KGAudio::Get(*AmbienceSound.ToString()));
	Ambience->RegisterComponent();
}

void AKGUndergroundInfo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Plan))
	{
		Plan->Destroy();
	}
	Plan = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AKGUndergroundInfo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager || !Look)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const float Target = Contains(Cam) ? 1.0f : 0.0f;
	Below = FMath::FInterpConstantTo(Below, Target, DeltaSeconds, 2.5f);
	Look->BlendWeight = Below;
	if (Ambience)
	{
		if (Below > 0.01f && !Ambience->IsPlaying())
		{
			Ambience->Play();
		}
		else if (Below <= 0.01f && Ambience->IsPlaying())
		{
			Ambience->Stop();
		}
		Ambience->SetVolumeMultiplier(FMath::Max(0.001f, Below * 0.8f));
	}
	if (Below > 0.5f)
	{
		DripIn -= DeltaSeconds;
		if (DripIn <= 0.0f)
		{
			// Cosmetic randomness (xorshift, not gameplay): a drip somewhere around the listener.
			auto Next = [this]()
			{
				DripState ^= DripState << 13;
				DripState ^= DripState >> 17;
				DripState ^= DripState << 5;
				return (DripState & 0xFFFF) / 65535.0f;
			};
			const float A = Next() * UE_TWO_PI;
			const FVector At = Cam + FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * (300.0f + 700.0f * Next()) + FVector(0, 0, 120.0f);
			KGAudio::At(this, TEXT("S_Cave_Drip"), At, 0.5f + 0.4f * Next());
			DripIn = 1.2f + 3.8f * Next();
		}
	}
}
