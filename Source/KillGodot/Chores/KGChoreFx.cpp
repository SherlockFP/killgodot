#include "Chores/KGChoreFx.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace KGChoreFxPrivate
{
	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UStaticMesh* Sphere() { return Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
	UStaticMesh* Cube() { return Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Cylinder() { return Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }

	/** Unlit emissive ("Color", "Intensity"); falls back to the chore marker glow, then the engine shape material. */
	UMaterialInterface* GlowMaterial()
	{
		if (UMaterialInterface* M = Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreGlow.M_KG_ChoreGlow")))
		{
			return M;
		}
		if (UMaterialInterface* M = Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_TaskMarker.M_KG_TaskMarker")))
		{
			return M;
		}
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	/** Translucent soft puff ("Color", "Opacity"). */
	UMaterialInterface* SmokeMaterial()
	{
		if (UMaterialInterface* M = Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreSmoke.M_KG_ChoreSmoke")))
		{
			return M;
		}
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	/** Lit solid colour ("Color"). */
	UMaterialInterface* SolidMaterial()
	{
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	FLinearColor Srgb(uint32 RGB)
	{
		return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
	}

	constexpr int32 SmokePuffs = 12;
	constexpr float SmokeCycle = 6.5f;
}

AKGChoreFx::AKGChoreFx()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(10.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
}

void AKGChoreFx::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGChoreFx, Events);
}

AKGChoreFx* AKGChoreFx::Get(UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKGChoreFx> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	if (!bCreate || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Name = TEXT("KGChoreFx");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	return World->SpawnActor<AKGChoreFx>(AKGChoreFx::StaticClass(), FTransform::Identity, Params);
}

float AKGChoreFx::Duration(EKGChoreFx Fx)
{
	switch (Fx)
	{
	case EKGChoreFx::BellRing: return 8.0f;
	case EKGChoreFx::ClockChime: return 5.0f;
	case EKGChoreFx::LighthouseGlow: return 150.0f;
	case EKGChoreFx::ChimneySmoke: return 80.0f;
	case EKGChoreFx::NoticePosted: return 600.0f;
	case EKGChoreFx::CandlesLit: return 180.0f;
	case EKGChoreFx::LampLit: return 240.0f;
	case EKGChoreFx::WoodPile: return 900.0f;
	default: return 1.0f;
	}
}

float AKGChoreFx::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.0f);
}

void AKGChoreFx::AuthTrigger(FName ChoreId, EKGChoreFx Fx, const FVector& Where, const FRotator& Facing)
{
	if (!HasAuthority() || Fx == EKGChoreFx::None)
	{
		return;
	}
	const float Now = ServerNow();
	Events.RemoveAll([Now](const FKGChoreFxEvent& E) { return Now - E.ServerTime > Duration(static_cast<EKGChoreFx>(E.Fx)); });
	while (Events.Num() >= 40)
	{
		Events.RemoveAt(0);
	}
	FKGChoreFxEvent& E = Events.AddDefaulted_GetRef();
	E.ChoreId = ChoreId;
	E.Fx = static_cast<uint8>(Fx);
	E.Location = Where;
	E.Yaw = Facing.Yaw;
	E.ServerTime = Now;
	E.Serial = NextSerial++;
	ForceNetUpdate();
	UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_FX %s serial=%d at %s: %s"), *ChoreId.ToString(), E.Serial, *Where.ToCompactString(),
	       *FKGChoreCatalog::FxName(Fx));
	if (GetNetMode() != NM_DedicatedServer)
	{
		SyncLive();
	}
}

void AKGChoreFx::OnRep_Events()
{
	SyncLive();
}

void AKGChoreFx::SyncLive()
{
	const float Now = ServerNow();
	// Drop presentations whose event is gone or expired.
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		const FLive& L = Live[i];
		const bool bKnown = Events.ContainsByPredicate([&L](const FKGChoreFxEvent& E) { return E.Serial == L.Serial; });
		if (!bKnown || Now - L.ServerTime > Duration(L.Fx))
		{
			DestroyLive(Live[i]);
			Live.RemoveAt(i);
		}
	}
	for (const FKGChoreFxEvent& E : Events)
	{
		const EKGChoreFx Fx = static_cast<EKGChoreFx>(E.Fx);
		if (Now - E.ServerTime > Duration(Fx) || Live.ContainsByPredicate([&E](const FLive& L) { return L.Serial == E.Serial; }))
		{
			continue;
		}
		FLive& L = Live.AddDefaulted_GetRef();
		L.Serial = E.Serial;
		L.Fx = Fx;
		L.Location = E.Location;
		L.Yaw = E.Yaw;
		L.ServerTime = E.ServerTime;
		Build(L);
		UE_LOG(LogKillGodot, Log, TEXT("KG_CHORE_FX_SEEN %s serial=%d fx=%s age=%.1fs authority=%d"), *E.ChoreId.ToString(), E.Serial,
		       *FKGChoreCatalog::FxName(Fx), Now - E.ServerTime, HasAuthority() ? 1 : 0);
	}
}

UStaticMeshComponent* AKGChoreFx::AddMesh(FLive& L, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& Color, float Glow)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	C->SetupAttachment(GetRootComponent());
	C->RegisterComponent();
	if (Material)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Intensity"), Glow);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		C->SetMaterial(0, MID);
	}
	L.Meshes.Add(C);
	Owned.Add(C);
	return C;
}

UPointLightComponent* AKGChoreFx::AddLight(FLive& L, const FLinearColor& Color, float Intensity, float Radius)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetLightColor(Color);
	Light->SetIntensity(0.0f);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	Light->SetupAttachment(GetRootComponent());
	Light->RegisterComponent();
	L.Lights.Add(Light);
	Owned.Add(Light);
	return Light;
}

void AKGChoreFx::Build(FLive& L)
{
	using namespace KGChoreFxPrivate;
	const FRotator Facing(0.0f, L.Yaw, 0.0f);
	const FVector Fwd = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
	switch (L.Fx)
	{
	case EKGChoreFx::BellRing:
	case EKGChoreFx::ClockChime:
	{
		L.Base = L.Location + FVector(0.0f, 0.0f, 250.0f);
		AddLight(L, Srgb(0xFFD08A), 0.0f, 1600.0f)->SetWorldLocation(L.Base);
		break;
	}
	case EKGChoreFx::LighthouseGlow:
	{
		L.Base = L.Location + FVector(0.0f, 0.0f, 380.0f);
		if (UStaticMeshComponent* Orb = AddMesh(L, Sphere(), GlowMaterial(), Srgb(0xFFC45A), 0.0f))
		{
			Orb->SetWorldLocation(L.Base);
			Orb->SetWorldScale3D(FVector(1.5f));
		}
		AddLight(L, Srgb(0xFFC87A), 0.0f, 9000.0f)->SetWorldLocation(L.Base + FVector(0.0f, 0.0f, 20.0f));
		break;
	}
	case EKGChoreFx::LampLit:
	{
		L.Base = L.Location + FVector(0.0f, 0.0f, 250.0f);
		if (UStaticMeshComponent* Orb = AddMesh(L, Sphere(), GlowMaterial(), Srgb(0xFFB347), 0.0f))
		{
			Orb->SetWorldLocation(L.Base);
			Orb->SetWorldScale3D(FVector(0.4f));
		}
		AddLight(L, Srgb(0xFFB866), 0.0f, 2600.0f)->SetWorldLocation(L.Base);
		break;
	}
	case EKGChoreFx::CandlesLit:
	{
		L.Base = L.Location + FVector(0.0f, 0.0f, 100.0f);
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector At = L.Base + Fwd * 40.0f + Right * ((i - 2) * 22.0f) + FVector(0.0f, 0.0f, (i % 2) * 6.0f);
			if (UStaticMeshComponent* Candle = AddMesh(L, Cylinder(), SolidMaterial(), Srgb(0xF4EBD6), 0.0f))
			{
				Candle->SetWorldLocation(At);
				Candle->SetWorldScale3D(FVector(0.05f, 0.05f, 0.22f));
			}
			if (UStaticMeshComponent* Flame = AddMesh(L, Sphere(), GlowMaterial(), Srgb(0xFFB030), 0.0f))
			{
				Flame->SetWorldLocation(At + FVector(0.0f, 0.0f, 16.0f));
				Flame->SetWorldScale3D(FVector(0.045f, 0.045f, 0.08f));
			}
		}
		AddLight(L, Srgb(0xFFB45C), 0.0f, 1100.0f)->SetWorldLocation(L.Base + Fwd * 40.0f + FVector(0.0f, 0.0f, 30.0f));
		break;
	}
	case EKGChoreFx::ChimneySmoke:
	{
		// The roof above the station: smoke leaves the highest surface there.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGChoreSmoke), false);
		const FVector Top = L.Location + FVector(0.0f, 0.0f, 4000.0f);
		L.Base = GetWorld()->LineTraceSingleByChannel(Hit, Top, L.Location + FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, Params)
			         ? Hit.ImpactPoint + FVector(0.0f, 0.0f, 40.0f)
			         : L.Location + FVector(0.0f, 0.0f, 650.0f);
		for (int32 i = 0; i < SmokePuffs; ++i)
		{
			AddMesh(L, Sphere(), SmokeMaterial(), Srgb(0xD9D4CF), 0.0f);
		}
		break;
	}
	case EKGChoreFx::NoticePosted:
	{
		int32 Index = 0;
		for (const FLive& Other : Live)
		{
			Index += (&Other != &L && Other.Fx == EKGChoreFx::NoticePosted && Other.Serial < L.Serial) ? 1 : 0;
		}
		const FVector At = L.Location + FVector(0.0f, 0.0f, 150.0f + (Index / 3) * 26.0f) + Right * ((Index % 3 - 1) * 48.0f) + Fwd * 8.0f;
		if (UStaticMeshComponent* Sheet = AddMesh(L, Cube(), SolidMaterial(), Srgb(0xF3E6C4), 0.0f))
		{
			Sheet->SetWorldLocationAndRotation(At, FRotator(0.0f, L.Yaw + ((Index % 2) ? 3.0f : -2.0f), 0.0f));
			Sheet->SetWorldScale3D(FVector(0.012f, 0.40f, 0.52f));
		}
		if (UStaticMeshComponent* Seal = AddMesh(L, Sphere(), SolidMaterial(), Srgb(0xC8102E), 0.0f))
		{
			Seal->SetWorldLocation(At + Fwd * 1.5f + FVector(0.0f, 0.0f, -16.0f));
			Seal->SetWorldScale3D(FVector(0.03f, 0.07f, 0.07f));
		}
		break;
	}
	case EKGChoreFx::WoodPile:
	{
		int32 Index = 0;
		for (const FLive& Other : Live)
		{
			Index += (&Other != &L && Other.Fx == EKGChoreFx::WoodPile && Other.Serial < L.Serial) ? 1 : 0;
		}
		const FVector Base = L.Location + Right * 170.0f;
		for (int32 j = 0; j < 3; ++j)
		{
			const int32 N = (Index * 3 + j) % 15;
			const int32 Row = N < 5 ? 0 : N < 9 ? 1 : N < 12 ? 2 : N < 14 ? 3 : 4;
			const int32 RowStart = Row == 0 ? 0 : Row == 1 ? 5 : Row == 2 ? 9 : Row == 3 ? 12 : 14;
			const int32 Col = N - RowStart;
			const float Offset = (Col - (4 - Row) * 0.5f) * 24.0f;
			const FVector At = Base + Right * Offset + FVector(0.0f, 0.0f, 12.0f + Row * 21.0f);
			if (UStaticMeshComponent* Log = AddMesh(L, Cylinder(), SolidMaterial(), Srgb(0x9A6B45), 0.0f))
			{
				Log->SetWorldLocationAndRotation(At, FRotator(90.0f, L.Yaw, 0.0f));
				Log->SetWorldScale3D(FVector(0.22f, 0.22f, 0.75f));
			}
		}
		break;
	}
	default:
		break;
	}
}

void AKGChoreFx::Strike(const FVector& Where, float Pitch, float Volume)
{
	USoundBase* Bell = LoadObject<USoundBase>(nullptr, TEXT("/Game/KillGodot/Audio/S_ChurchBell.S_ChurchBell"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Bell)
	{
		return;
	}
	if (!BellAttenuation)
	{
		// Heard across the whole cove: loud near the tower, still clear at the harbour.
		BellAttenuation = NewObject<USoundAttenuation>(this);
		BellAttenuation->Attenuation.AttenuationShapeExtents = FVector(3500.0f, 0.0f, 0.0f);
		BellAttenuation->Attenuation.FalloffDistance = 26000.0f;
	}
	UGameplayStatics::PlaySoundAtLocation(this, Bell, Where, Volume, Pitch, 0.0f, BellAttenuation);
}

void AKGChoreFx::Animate(FLive& L, float Age, float Dt)
{
	using namespace KGChoreFxPrivate;
	const float Dur = Duration(L.Fx);
	const float In = FMath::Clamp(Age / 1.5f, 0.0f, 1.0f);
	const float Out = FMath::Clamp((Dur - Age) / FMath::Min(20.0f, Dur * 0.25f), 0.0f, 1.0f);
	const float Alpha = In * Out;
	auto SetGlow = [](UStaticMeshComponent* C, float Intensity)
	{
		if (UMaterialInstanceDynamic* MID = C ? Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)) : nullptr)
		{
			MID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		}
	};
	switch (L.Fx)
	{
	case EKGChoreFx::BellRing:
	case EKGChoreFx::ClockChime:
	{
		const bool bBell = L.Fx == EKGChoreFx::BellRing;
		const int32 Count = bBell ? 3 : 2;
		const float Gap = bBell ? 1.9f : 1.3f;
		float Flash = 0.0f;
		for (int32 s = 0; s < Count; ++s)
		{
			const float At = s * Gap;
			if (s >= L.Strikes && Age >= At)
			{
				L.Strikes = s + 1;
				// A late joiner does not hear a bell that rang long ago.
				if (Age - At < 1.0f)
				{
					Strike(L.Base, bBell ? 1.0f : 1.55f, bBell ? 1.0f : 0.7f);
				}
			}
			if (Age >= At)
			{
				Flash = FMath::Max(Flash, FMath::Exp(-(Age - At) * 2.5f));
			}
		}
		for (UPointLightComponent* Light : L.Lights)
		{
			Light->SetIntensity(300.0f * Flash * Out);
		}
		break;
	}
	case EKGChoreFx::LighthouseGlow:
	{
		const float Breathe = 0.85f + 0.15f * FMath::Sin(Age * 1.6f);
		for (UStaticMeshComponent* C : L.Meshes)
		{
			SetGlow(C, 60.0f * Alpha * Breathe);
			C->SetWorldScale3D(FVector(1.3f + 0.25f * Breathe * Alpha));
		}
		for (UPointLightComponent* Light : L.Lights)
		{
			Light->SetIntensity(6000.0f * Alpha * Breathe);
		}
		break;
	}
	case EKGChoreFx::LampLit:
	case EKGChoreFx::CandlesLit:
	{
		const float Flicker = 0.85f + 0.1f * FMath::Sin(Age * 13.0f) + 0.05f * FMath::Sin(Age * 29.0f + 1.3f);
		for (UStaticMeshComponent* C : L.Meshes)
		{
			SetGlow(C, 30.0f * Alpha * Flicker);
		}
		for (UPointLightComponent* Light : L.Lights)
		{
			Light->SetIntensity((L.Fx == EKGChoreFx::LampLit ? 400.0f : 120.0f) * Alpha * Flicker);
		}
		break;
	}
	case EKGChoreFx::ChimneySmoke:
	{
		for (int32 i = 0; i < L.Meshes.Num(); ++i)
		{
			UStaticMeshComponent* Puff = L.Meshes[i];
			const float T = FMath::Fmod(Age / SmokeCycle + i / float(SmokePuffs), 1.0f);
			const float Rise = 1100.0f * T;
			const FVector Drift(260.0f * T * T, 90.0f * FMath::Sin(T * 5.0f + i), 0.0f);
			Puff->SetWorldLocation(L.Base + Drift + FVector(0.0f, 0.0f, Rise));
			Puff->SetWorldScale3D(FVector(0.5f + 2.4f * T));
			const float PuffAlpha = FMath::Clamp(T / 0.12f, 0.0f, 1.0f) * (1.0f - T) * 0.75f * Alpha;
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Puff->GetMaterial(0)))
			{
				MID->SetScalarParameterValue(TEXT("Opacity"), PuffAlpha);
			}
			Puff->SetVisibility(PuffAlpha > 0.01f);
		}
		break;
	}
	default:
		break;
	}
}

void AKGChoreFx::DestroyLive(FLive& L)
{
	for (UStaticMeshComponent* C : L.Meshes)
	{
		if (C)
		{
			Owned.Remove(C);
			C->DestroyComponent();
		}
	}
	for (UPointLightComponent* C : L.Lights)
	{
		if (C)
		{
			Owned.Remove(C);
			C->DestroyComponent();
		}
	}
	L.Meshes.Reset();
	L.Lights.Reset();
}

void AKGChoreFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (HasAuthority())
	{
		const int32 Before = Events.Num();
		Events.RemoveAll([Now](const FKGChoreFxEvent& E) { return Now - E.ServerTime > Duration(static_cast<EKGChoreFx>(E.Fx)) + 1.0f; });
		if (Events.Num() != Before)
		{
			ForceNetUpdate();
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SyncLive();
	for (FLive& L : Live)
	{
		const float Age = Now - L.ServerTime;
		if (L.bFresh)
		{
			L.bFresh = false;
			// Joined late: strikes that are long over count as played.
			const float Gap = L.Fx == EKGChoreFx::BellRing ? 1.9f : 1.3f;
			while ((L.Fx == EKGChoreFx::BellRing || L.Fx == EKGChoreFx::ClockChime) && L.Strikes < 3 && Age - L.Strikes * Gap > 1.0f)
			{
				++L.Strikes;
			}
		}
		Animate(L, Age, DeltaSeconds);
	}
}
