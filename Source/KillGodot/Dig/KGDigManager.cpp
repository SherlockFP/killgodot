#include "Dig/KGDigManager.h"
#include "Audio/KGAudio.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "World/KGPickup.h"

namespace KGDigManagerPrivate
{
	const TCHAR* PackDir = TEXT("/Game/KillGodot/Env/Dress/KG_DressUnder_Clean/StaticMeshes/");
	const TCHAR* WaterDir = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/");

	UStaticMesh* LoadMesh(const TCHAR* Dir, const TCHAR* Name)
	{
		static TMap<FString, TStrongObjectPtr<UStaticMesh>> Cache;
		const FString Path = FString::Printf(TEXT("%s%s.%s"), Dir, Name, Name);
		if (const TStrongObjectPtr<UStaticMesh>* Hit = Cache.Find(Path))
		{
			return Hit->Get();
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		Cache.Add(Path, TStrongObjectPtr<UStaticMesh>(Mesh));
		return Mesh;
	}

	/** SM_KG_<Name> from the underground pack (Tools/Blender/kg_make_dress_underground.py), else a fallback. */
	UStaticMesh* PackMesh(const TCHAR* Name, const TCHAR* WaterFallback = nullptr)
	{
		const FString Full = FString(TEXT("SM_KG_")) + Name;
		if (UStaticMesh* Mesh = LoadMesh(PackDir, *Full))
		{
			return Mesh;
		}
		if (WaterFallback)
		{
			const FString Water = FString(TEXT("SM_KG_")) + WaterFallback;
			return LoadMesh(WaterDir, *Water);
		}
		return nullptr;
	}

	USoundAttenuation* FarAttenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("KG_DigFarAttenuation"));
			A->AddToRoot();
			A->Attenuation.FalloffDistance = FKGDigRules::GraveNoiseRadius;
			A->Attenuation.AttenuationShapeExtents = FVector(600.0f, 0.0f, 0.0f);
			Att = A;
		}
		return Att.Get();
	}

	void PlayFar(const UObject* Ctx, const TCHAR* Name, const FVector& At, float Volume)
	{
		USoundBase* S = KGAudio::Get(Name);
		if (S && Ctx && Ctx->GetWorld() && Ctx->GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(Ctx, S, At, Volume, 1.0f, 0.0f, FarAttenuation());
		}
	}

	constexpr int32 MaxClods = 48;

	TMap<TObjectKey<UWorld>, TWeakObjectPtr<AKGDigManager>>& Cache()
	{
		static TMap<TObjectKey<UWorld>, TWeakObjectPtr<AKGDigManager>> Map;
		return Map;
	}
}

AKGDigManager::AKGDigManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGDigManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGDigManager, Spots);
	DOREPLIFETIME(AKGDigManager, GeneratedSeed);
}

AKGDigManager* AKGDigManager::Get(const UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	auto& Map = KGDigManagerPrivate::Cache();
	if (const TWeakObjectPtr<AKGDigManager>* Hit = Map.Find(World); Hit && Hit->IsValid() && !(*Hit)->IsActorBeingDestroyed())
	{
		return Hit->Get();
	}
	for (TActorIterator<AKGDigManager> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (!It->IsActorBeingDestroyed())
		{
			Map.Add(World, *It);
			return *It;
		}
	}
	if (!bCreate || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Name = TEXT("KG_DigManager");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKGDigManager* Manager = const_cast<UWorld*>(World)->SpawnActor<AKGDigManager>(AKGDigManager::StaticClass(), FTransform::Identity, Params);
	if (Manager)
	{
		Map.Add(World, Manager);
	}
	return Manager;
}

void AKGDigManager::BeginPlay()
{
	Super::BeginPlay();
	CosmeticRng = FKGRng(FCrc::StrCrc32(*GetName()) ^ 0xC10Du, 7u);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Clods = NewObject<UInstancedStaticMeshComponent>(this, TEXT("DigClods"));
		Clods->SetupAttachment(GetRootComponent());
		Clods->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Clods->SetCastShadow(false);
		Clods->SetMobility(EComponentMobility::Movable);
		Clods->SetUsingAbsoluteLocation(true);
		Clods->SetUsingAbsoluteRotation(true);
		Clods->SetUsingAbsoluteScale(true);
		if (UStaticMesh* Sphere = KGDigManagerPrivate::LoadMesh(TEXT("/Engine/BasicShapes/"), TEXT("Sphere")))
		{
			Clods->SetStaticMesh(Sphere);
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
				Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.07f, 0.035f));
				Clods->SetMaterial(0, Mid);
			}
		}
		Clods->RegisterComponent();
	}
}

// =================================================================================================================
// Queries
// =================================================================================================================
const FKGDigSpot* AKGDigManager::FindSpot(uint16 Id) const
{
	return Spots.Items.FindByPredicate([Id](const FKGDigSpot& S) { return S.Id == Id; });
}

int32 AKGDigManager::IndexOfSpot(uint16 Id) const
{
	return Spots.Items.IndexOfByPredicate([Id](const FKGDigSpot& S) { return S.Id == Id; });
}

int32 AKGDigManager::FindSpotNear(const FVector& P, float Slack) const
{
	int32 Best = INDEX_NONE;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Spots.Items.Num(); ++i)
	{
		const FKGDigSpot& S = Spots.Items[i];
		const float R = FKGDigRules::AimRadius(S.Kind) + Slack;
		const float D = FVector::Dist2D(P, FVector(S.Location));
		if (D <= R && FMath::Abs(P.Z - S.Location.Z) < 150.0f && D < BestD)
		{
			Best = i;
			BestD = D;
		}
	}
	return Best;
}

int32 AKGDigManager::FindNearestSpot(const FVector& P, float MaxDist, TOptional<EKGDigKind> Kind) const
{
	int32 Best = INDEX_NONE;
	float BestD = MaxDist;
	for (int32 i = 0; i < Spots.Items.Num(); ++i)
	{
		const FKGDigSpot& S = Spots.Items[i];
		if (Kind.IsSet() && S.Kind != Kind.GetValue())
		{
			continue;
		}
		const float D = FVector::Dist(P, FVector(S.Location));
		if (D <= BestD)
		{
			Best = i;
			BestD = D;
		}
	}
	return Best;
}

// =================================================================================================================
// Server
// =================================================================================================================
bool AKGDigManager::ProbeGround(UWorld* World, const FVector2D& XY, FVector& OutGround)
{
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDigProbe), true);
	if (!World->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, 8000.0), FVector(XY.X, XY.Y, -1500.0), ECC_Visibility, Params))
	{
		return false;
	}
	// The terrain itself: not a roof, a prop, a tree, a bridge or a path deck.
	const UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
	if (!Mesh || !Mesh->GetName().Contains(TEXT("Terrain")) || Hit.ImpactNormal.Z < 0.86f || Hit.ImpactPoint.Z < 40.0f)
	{
		return false;
	}
	// Room to stand and swing: nothing solid at knee-to-chest height around the spot (walls, trunks, fences).
	FCollisionQueryParams Overlap(SCENE_QUERY_STAT(KGDigProbeRoom), false);
	if (World->OverlapBlockingTestByChannel(Hit.ImpactPoint + FVector(0.0, 0.0, 100.0), FQuat::Identity, ECC_Pawn,
	                                        FCollisionShape::MakeSphere(75.0f), Overlap))
	{
		return false;
	}
	OutGround = Hit.ImpactPoint;
	return true;
}

FVector AKGDigManager::GraveFromStone(const FVector& StoneLocation, float StoneYaw, float& OutYaw)
{
	// The builder's rows face the lych gate on the graveyard's local -Y; the grave lies in front of its stone.
	const FVector Front = FRotator(0.0f, StoneYaw, 0.0f).RotateVector(FVector(0.0, -1.0, 0.0));
	OutYaw = StoneYaw + 90.0f;   // GraveOpen_* run along their local X
	return StoneLocation + Front * 125.0f;
}

void AKGDigManager::Regenerate(uint64 Seed)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	Spots.Items.Reset();
	Buried.Reset();
	NextId = 1;
	GeneratedSeed = Seed;
	bGenerated = true;

	// Graves: one per gravestone of the level, sorted so every host numbers them the same way.
	struct FStone
	{
		FVector Location;
		float Yaw;
	};
	TArray<FStone> Stones;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		const UStaticMeshComponent* Comp = It->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (Mesh && Mesh->GetName().Contains(TEXT("Gravestone")))
		{
			Stones.Add({It->GetActorLocation(), It->GetActorRotation().Yaw});
		}
	}
	Stones.Sort([](const FStone& A, const FStone& B)
	{
		return !FMath::IsNearlyEqual(A.Location.X, B.Location.X, 1.0) ? A.Location.X < B.Location.X : A.Location.Y < B.Location.Y;
	});
	TArray<FVector> GravePoints;
	for (const FStone& Stone : Stones)
	{
		float Yaw = 0.0f;
		const FVector Want = GraveFromStone(Stone.Location, Stone.Yaw, Yaw);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDigGrave), true);
		FVector Ground = Want;
		if (World->LineTraceSingleByChannel(Hit, Want + FVector(0.0, 0.0, 300.0), Want - FVector(0.0, 0.0, 400.0), ECC_Visibility, Params))
		{
			Ground = Hit.ImpactPoint;
		}
		Spots.Items.Add(FKGDigRules::MakeSpot(NextId++, EKGDigKind::Grave, EKGDigZone::Graveyard, Ground, Yaw));
		GravePoints.Add(Ground);
	}

	// Zones (Morrowmere v2 only: the numbers are that map's).
	FKGRng Rng(Seed ^ 0xD16F1E1Dull, 0xD16u);
	TArray<FKGDigSpot> Public;
	if (World->GetMapName().Contains(TEXT("Morrowmere_v2")))
	{
		FKGDigRules::Generate(Rng, FKGDigRules::MorrowmereV2Zones(),
		                      [World](const FVector2D& XY, FVector& Out) { return ProbeGround(World, XY, Out); },
		                      GravePoints, Public, Buried, NextId);
		SpawnShovels(Rng);
	}
	Spots.Items.Append(Public);
	Spots.MarkArrayDirty();
	int32 Counts[5] = {};
	for (const FKGDigSpot& S : Spots.Items)
	{
		++Counts[static_cast<int32>(S.Kind)];
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_GEN seed=%llu spots=%d mounds=%d x=%d glints=%d graves=%d buried=%d"),
	       Seed, Spots.Items.Num(), Counts[0], Counts[1], Counts[2], Counts[3], Buried.Num());
}

void AKGDigManager::SpawnShovels(FKGRng& Rng)
{
	for (const TWeakObjectPtr<AKGPickup>& Old : Shovels)
	{
		if (Old.IsValid())
		{
			Old->Destroy();
		}
	}
	Shovels.Reset();
	// Where a villager leaves a shovel (metres, v2 layout): the lych gate, the farmyard, the beach, the woodcutter.
	const FVector2D Sites[] = {{-27.6, -51.4}, {55.0, -66.5}, {-58.0, 62.0}, {-98.0, -1.5}};
	for (const FVector2D& Site : Sites)
	{
		const FVector2D XY = Site * 100.0 + FVector2D(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f) * 120.0;
		if (AKGPickup* Pickup = AKGPickup::SpawnPickup(this, FKGItemIds::Shovel, 1, FVector(XY.X, XY.Y, 3000.0), true))
		{
			Shovels.Add(Pickup);
		}
	}
}

void AKGDigManager::MarkChanged(int32 Index)
{
	if (Spots.Items.IsValidIndex(Index))
	{
		Spots.MarkItemDirty(Spots.Items[Index]);
	}
	ForceNetUpdate();
}

int32 AKGDigManager::RevealBuriedNear(const FVector& P, float Radius)
{
	int32 Best = INDEX_NONE;
	float BestD = Radius;
	for (int32 i = 0; i < Buried.Num(); ++i)
	{
		const float D = FVector::Dist2D(P, FVector(Buried[i].Location));
		if (D <= BestD)
		{
			Best = i;
			BestD = D;
		}
	}
	if (Best == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	const int32 Index = Spots.Items.Add(Buried[Best]);
	Buried.RemoveAt(Best);
	MarkChanged(Index);
	UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_REVEAL buried chest #%d at %s"), Spots.Items[Index].Id,
	       *FVector(Spots.Items[Index].Location).ToCompactString());
	return Index;
}

bool AKGDigManager::ApplyStage(int32 Index, TArray<FKGItemStack>& OutItems)
{
	if (!HasAuthority() || !Spots.Items.IsValidIndex(Index) || Spots.Items[Index].IsDugOut())
	{
		return false;
	}
	FKGDigSpot& S = Spots.Items[Index];
	++S.Stage;
	const FName Table = FKGDigRules::LootTable(S.Kind, S.Stage, S.MaxStage);
	if (const FKGLootTable* LootTable = Table.IsNone() ? nullptr : FKGLoot::FindTable(Table))
	{
		FKGRng Rng = FKGDigRules::StageRng(GeneratedSeed, S.Id, S.Stage);
		FKGLoot::Roll(Rng, *LootTable, OutItems);
	}
	MarkChanged(Index);
	return true;
}

void AKGDigManager::DebugSetStage(int32 Index, uint8 Stage)
{
	if (HasAuthority() && Spots.Items.IsValidIndex(Index))
	{
		Spots.Items[Index].Stage = FMath::Min(Stage, Spots.Items[Index].MaxStage);
		MarkChanged(Index);
	}
}

int32 AKGDigManager::AddSpot(EKGDigKind Kind, const FVector& Ground, float YawDeg, EKGDigZone Zone)
{
	if (!HasAuthority())
	{
		return INDEX_NONE;
	}
	bGenerated = true;
	const int32 Index = Spots.Items.Add(FKGDigRules::MakeSpot(NextId++, Kind, Zone, Ground, YawDeg));
	MarkChanged(Index);
	return Index;
}

void AKGDigManager::AddBuried(const FVector& Ground)
{
	if (HasAuthority())
	{
		bGenerated = true;
		Buried.Add(FKGDigRules::MakeSpot(NextId++, EKGDigKind::Treasure, EKGDigZone::None, Ground, 0.0f));
	}
}

// =================================================================================================================
// Visuals (every machine)
// =================================================================================================================
void AKGDigManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SyncVisuals();
	TickFx(DeltaSeconds);
}

UStaticMeshComponent* AKGDigManager::MakeMeshComp(const FName& Name)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Name));
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetUsingAbsoluteLocation(true);
	Comp->SetUsingAbsoluteRotation(true);
	Comp->SetUsingAbsoluteScale(true);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetCullDistance(12000.0f);
	Comp->bCastDynamicShadow = false;
	Comp->RegisterComponent();
	return Comp;
}

void AKGDigManager::SyncVisuals()
{
	if (LastSeedSeen != GeneratedSeed)
	{
		// A fresh set (new match): drop every old mesh, no dust for the new spots.
		for (TPair<uint16, FSpotVisual>& Pair : Visuals)
		{
			for (const TWeakObjectPtr<UStaticMeshComponent>& C : {Pair.Value.Main, Pair.Value.Pile, Pair.Value.Extra})
			{
				if (C.IsValid())
				{
					C->DestroyComponent();
				}
			}
		}
		Visuals.Reset();
		LastSeedSeen = GeneratedSeed;
		bVisualsPrimed = false;
	}
	for (const FKGDigSpot& Spot : Spots.Items)
	{
		FSpotVisual& V = Visuals.FindOrAdd(Spot.Id);
		if (V.ShownStage == Spot.Stage && V.Kind == Spot.Kind)
		{
			continue;
		}
		const bool bDeeper = bVisualsPrimed && V.ShownStage != 255 && Spot.Stage > V.ShownStage;
		V.Kind = Spot.Kind;
		ApplyVisual(Spot, V);
		if (bDeeper)
		{
			PlayStageFx(Spot, Spot.Stage);
		}
		V.ShownStage = Spot.Stage;
	}
	bVisualsPrimed = true;
	// Glints twinkle: a slow scale pulse (cheap, only a handful).
	GlintTime += GetWorld()->GetDeltaSeconds();
	for (const FKGDigSpot& Spot : Spots.Items)
	{
		if (Spot.Kind == EKGDigKind::Glint && Spot.Stage == 0)
		{
			if (const FSpotVisual* V = Visuals.Find(Spot.Id); V && V->Main.IsValid())
			{
				const float Pulse = 1.0f + 0.07f * FMath::Sin(GlintTime * 3.1f + Spot.Id);
				V->Main->SetWorldScale3D(FVector(1.0f, 1.0f, Pulse));
			}
		}
	}
}

void AKGDigManager::ApplyVisual(const FKGDigSpot& Spot, FSpotVisual& V)
{
	using namespace KGDigManagerPrivate;
	auto Ensure = [this](TWeakObjectPtr<UStaticMeshComponent>& C, const TCHAR* Name) -> UStaticMeshComponent*
	{
		if (!C.IsValid())
		{
			C = MakeMeshComp(Name);
		}
		return C.Get();
	};
	auto Hide = [](TWeakObjectPtr<UStaticMeshComponent>& C)
	{
		if (C.IsValid())
		{
			C->SetVisibility(false);
		}
	};
	const FVector At = Spot.Location;
	const FRotator Rot(0.0f, Spot.GetYaw(), 0.0f);
	auto Show = [&](TWeakObjectPtr<UStaticMeshComponent>& C, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Local,
	                const FVector& Scale, float YawOff = 0.0f)
	{
		if (!Mesh)
		{
			Hide(C);
			return;
		}
		UStaticMeshComponent* Comp = Ensure(C, Name);
		Comp->SetStaticMesh(Mesh);
		Comp->SetWorldLocationAndRotation(At + Rot.RotateVector(Local), Rot + FRotator(0.0f, YawOff, 0.0f));
		Comp->SetWorldScale3D(Scale);
		Comp->SetVisibility(true);
	};
	const uint8 S = Spot.Stage;
	const float Depth = Spot.MaxStage > 0 ? static_cast<float>(S) / Spot.MaxStage : 1.0f;
	// Holes sit a hair above the ground (they are rims + a dark pit; the terrain cannot be cut).
	const FVector Up(0.0, 0.0, 1.5);
	switch (Spot.Kind)
	{
	case EKGDigKind::Grave:
		if (S == 0)
		{
			Hide(V.Main);
			Hide(V.Pile);
		}
		else
		{
			const bool bOpen = S >= Spot.MaxStage - 1;
			Show(V.Main, TEXT("DigGrave"), PackMesh(bOpen ? TEXT("GraveOpen_2") : TEXT("GraveOpen_1"), TEXT("DugHole")), Up,
			     bOpen ? FVector(1.0) : FVector(1.0, 1.0, 0.6f + 0.4f * S));
			Show(V.Pile, TEXT("DigGravePile"), PackMesh(TEXT("DirtPile"), TEXT("DigMound")), FVector(0.0, 95.0, 0.0),
			     FVector(0.9f + 0.5f * Depth, 1.2f + 0.8f * Depth, 0.5f + 0.7f * Depth));
		}
		Hide(V.Extra);
		break;
	case EKGDigKind::Treasure:
		if (S == 0)
		{
			Show(V.Main, TEXT("DigTreasure"), PackMesh(TEXT("DigMound"), TEXT("DigMound")), FVector::ZeroVector, FVector(0.8f));
			Hide(V.Pile);
		}
		else if (Spot.IsDugOut())
		{
			Show(V.Main, TEXT("DigTreasure"), PackMesh(TEXT("BuriedChest"), TEXT("DugHole")), Up, FVector(1.0));
			Show(V.Pile, TEXT("DigTreasurePile"), PackMesh(TEXT("DirtPile"), TEXT("DigMound")), FVector(115.0, 0.0, 0.0), FVector(1.5f));
		}
		else
		{
			const TCHAR* Hole = S == 1 ? TEXT("DigHole_1") : S == 2 ? TEXT("DigHole_2") : TEXT("DigHole_3");
			Show(V.Main, TEXT("DigTreasure"), PackMesh(Hole, TEXT("DugHole")), Up, FVector(1.0));
			Show(V.Pile, TEXT("DigTreasurePile"), PackMesh(TEXT("DirtPile"), TEXT("DigMound")), FVector(110.0, 0.0, 0.0),
			     FVector(0.6f + 0.35f * S));
		}
		Hide(V.Extra);
		break;
	default:
		if (S == 0)
		{
			const TCHAR* Look = Spot.Kind == EKGDigKind::XMark ? TEXT("DigX") : Spot.Kind == EKGDigKind::Glint ? TEXT("DigGlint") : TEXT("DigMound");
			Show(V.Main, TEXT("DigSpot"), PackMesh(Look, TEXT("DigMound")), FVector::ZeroVector, FVector(1.0));
			Hide(V.Pile);
		}
		else
		{
			// Deeper with every stage; a glint (one stage) leaves a small hole.
			const int32 Level = Spot.MaxStage <= 1 ? 1 : FMath::Clamp<int32>(S, 1, 3);
			const TCHAR* Hole = Level == 1 ? TEXT("DigHole_1") : Level == 2 ? TEXT("DigHole_2") : TEXT("DigHole_3");
			Show(V.Main, TEXT("DigSpot"), PackMesh(Hole, TEXT("DugHole")), Up, FVector(Spot.MaxStage <= 1 ? 0.8f : 1.0f));
			Show(V.Pile, TEXT("DigPile"), PackMesh(TEXT("DirtPile"), TEXT("DigMound")), FVector(100.0, 20.0, 0.0),
			     FVector(0.45f + 0.35f * Level));
		}
		Hide(V.Extra);
		break;
	}
}

void AKGDigManager::PlayStageFx(const FKGDigSpot& Spot, uint8 NewStage)
{
	const FVector At = FVector(Spot.Location) + FVector(0.0, 0.0, 20.0);
	Puff(At, Spot.Kind == EKGDigKind::Grave ? 1.4f : 1.0f);
	const bool bFinal = NewStage >= Spot.MaxStage;
	if (FKGDigRules::IsLoud(Spot.Kind))
	{
		// Graves are loud: a spade on coffin wood carries across Crown Hill (the social-deduction hook).
		KGDigManagerPrivate::PlayFar(this, bFinal ? TEXT("S_Dig_Clank") : TEXT("S_Dig_Stab"), At, bFinal ? 1.4f : 1.1f);
		LastNoiseAt = At;
		LastNoiseTime = GetWorld()->GetRealTimeSeconds();
	}
	KGAudio::At(this, TEXT("S_Dig_Toss"), At, 0.9f);
	if (bFinal && Spot.Kind == EKGDigKind::Treasure)
	{
		KGAudio::At(this, TEXT("S_Dig_Chest"), At, 1.0f);
	}
	else if (bFinal && Spot.Kind == EKGDigKind::Glint)
	{
		KGAudio::At(this, TEXT("S_Fish_Coins"), At, 0.8f);
	}
}

void AKGDigManager::Puff(const FVector& At, float Size)
{
	if (!Clods)
	{
		return;
	}
	const int32 N = FMath::RoundToInt(6.0f + 8.0f * Size);
	for (int32 i = 0; i < N && ClodList.Num() < KGDigManagerPrivate::MaxClods; ++i)
	{
		const float A = CosmeticRng.FRand() * UE_TWO_PI;
		const float Out = 90.0f + 160.0f * CosmeticRng.FRand();
		FClod C;
		C.P = At + FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * 20.0f;
		C.V = FVector(FMath::Cos(A) * Out, FMath::Sin(A) * Out, 260.0f + 300.0f * CosmeticRng.FRand()) * Size;
		C.Age = 0.0f;
		C.Life = 0.55f + 0.4f * CosmeticRng.FRand();
		C.Size = (0.05f + 0.07f * CosmeticRng.FRand()) * FMath::Sqrt(Size);
		ClodList.Add(C);
	}
}

void AKGDigManager::TickFx(float DeltaSeconds)
{
	if (!Clods)
	{
		return;
	}
	if (ClodList.Num() == 0 && Clods->GetInstanceCount() == 0)
	{
		return;
	}
	for (int32 i = ClodList.Num() - 1; i >= 0; --i)
	{
		FClod& C = ClodList[i];
		C.Age += DeltaSeconds;
		C.V.Z -= 980.0f * DeltaSeconds;
		C.P += C.V * DeltaSeconds;
		if (C.Age >= C.Life)
		{
			ClodList.RemoveAtSwap(i);
		}
	}
	TArray<FTransform> Xf;
	Xf.Reserve(ClodList.Num());
	for (const FClod& C : ClodList)
	{
		const float Shrink = 1.0f - FMath::Clamp((C.Age - C.Life * 0.7f) / (C.Life * 0.3f), 0.0f, 1.0f);
		Xf.Add(FTransform(FQuat::Identity, C.P, FVector(C.Size * Shrink)));
	}
	Clods->ClearInstances();
	if (Xf.Num() > 0)
	{
		Clods->AddInstances(Xf, false, true);
	}
}
