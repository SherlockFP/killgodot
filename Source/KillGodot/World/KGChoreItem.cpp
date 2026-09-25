#include "World/KGChoreItem.h"

#include "Engine/World.h"   // before KGAudio.h (it uses UWorld)
#include "Audio/KGAudio.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"

namespace KGChoreItemPrivate
{
	template <typename T>
	T* Load(const FString& Path)
	{
		return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UStaticMesh* Shape(const TCHAR* Name)
	{
		return Load<UStaticMesh>(FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	UMaterialInterface* SolidMaterial()
	{
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	UMaterialInterface* GlowMaterial()
	{
		if (UMaterialInterface* M = Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreGlow.M_KG_ChoreGlow")))
		{
			return M;
		}
		return SolidMaterial();
	}

	const FLinearColor Water = FLinearColor(0.10f, 0.42f, 0.75f);
}

AKGChoreItem::AKGChoreItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.0f);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(15.0f));
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetCollisionProfileName(TEXT("PhysicsActor"));
	Box->SetSimulatePhysics(true);
	Box->SetCanEverAffectNavigation(false);
	Box->SetLinearDamping(0.4f);
	Box->SetAngularDamping(0.8f);
	Box->SetHiddenInGame(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetMobility(EComponentMobility::Movable);

	CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
}

void AKGChoreItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AKGChoreItem, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGChoreItem, Chore, COND_InitialOnly);
	DOREPLIFETIME(AKGChoreItem, OwnerPlayer);
	DOREPLIFETIME(AKGChoreItem, Fill);
	DOREPLIFETIME(AKGChoreItem, Pieces);
	DOREPLIFETIME(AKGChoreItem, Carriers);
	DOREPLIFETIME(AKGChoreItem, bAttachedCarry);
}

AKGChoreItem* AKGChoreItem::AuthSpawn(UWorld* World, FName InKind, const FVector& Where, float Yaw, APlayerState* InOwner, FName InChore)
{
	const FKGWorldItemDef* Def = FKGWorldChoreCatalog::Get().FindItem(InKind);
	if (!World || !Def || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform Xf(FRotator(0.0f, Yaw, 0.0f), Where + FVector(0.0f, 0.0f, Def->BoxExtent.Z + 4.0f));
	AKGChoreItem* Item = World->SpawnActorDeferred<AKGChoreItem>(AKGChoreItem::StaticClass(), Xf, nullptr, nullptr,
	                                                             ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Item)
	{
		return nullptr;
	}
	Item->Kind = InKind;
	Item->Chore = InChore;
	Item->OwnerPlayer = InOwner;
	Item->Pieces = FMath::Max(1, Def->Count);
	Item->Fill = 0.0f;
	Item->FinishSpawning(Xf);
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_ITEM spawn %s for %s (%s) at %s"), *InKind.ToString(),
	       InOwner ? *InOwner->GetPlayerName() : TEXT("-"), *InChore.ToString(), *Where.ToCompactString());
	return Item;
}

const FKGWorldItemDef* AKGChoreItem::GetDef() const
{
	return FKGWorldChoreCatalog::Get().FindItem(Kind);
}

void AKGChoreItem::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!Kind.IsNone())
	{
		BuildVisual();
	}
}

void AKGChoreItem::BeginPlay()
{
	Super::BeginPlay();
	BuildVisual();
	if (!HasAuthority())
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_ITEM_SEEN %s chore=%s authority=0"), *Kind.ToString(), *Chore.ToString());
	}
}

void AKGChoreItem::OnRep_Kind()
{
	BuildVisual();
}

void AKGChoreItem::OnRep_Attached()
{
	Box->SetSimulatePhysics(!bAttachedCarry);
	Box->SetCollisionEnabled(bAttachedCarry ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void AKGChoreItem::OnRep_Fill()
{
	// Clients: a visible drop in the water level splashes (the carrier ran, fell or the bucket tipped).
	if (LastFill >= 0.0f && Fill < LastFill - 0.04f && SpillCueCooldown <= 0.0f)
	{
		KGAudio::At(this, TEXT("S_Chore_Splash"), GetActorLocation(), 0.45f);
		SpillCueCooldown = 0.6f;
	}
	LastFill = Fill;
}

UStaticMeshComponent* AKGChoreItem::AddPart(UStaticMesh* PartMesh, const FVector& Rel, const FVector& Scale, const FLinearColor& Color, bool bGlow)
{
	if (!PartMesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(PartMesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	C->SetupAttachment(Box);
	C->SetRelativeLocation(Rel);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	const bool bShape = PartMesh->GetPathName().StartsWith(TEXT("/Engine/"));
	if (bShape)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(bGlow ? KGChoreItemPrivate::GlowMaterial() : KGChoreItemPrivate::SolidMaterial(), C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Intensity"), bGlow ? 6.0f : 0.0f);
		C->SetMaterial(0, MID);
	}
	KGWorldChoreLook::FlattenVertexColour(C);
	return C;
}

void AKGChoreItem::BuildVisual()
{
	using namespace KGChoreItemPrivate;
	const FKGWorldItemDef* Def = GetDef();
	if (bBuilt || !Def)
	{
		return;
	}
	bBuilt = true;
	Box->SetBoxExtent(Def->BoxExtent, true);
	if (HasAuthority())
	{
		Box->SetMassOverrideInKg(NAME_None, Def->MassKg, true);
	}
	if (UStaticMesh* M = Load<UStaticMesh>(Def->MeshPath))
	{
		Mesh->SetStaticMesh(M);
		Mesh->SetRelativeScale3D(Def->MeshScale);
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, Def->MeshZ));
		if (Def->bTinted)
		{
			Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(Def->Tint));
			Mesh->SetVectorParameterValueOnMaterials(TEXT("BaseColorFactor"), FVector(Def->Tint));
			Mesh->SetVectorParameterValueOnMaterials(TEXT("Tint"), FVector(Def->Tint));
		}
		KGWorldChoreLook::FlattenVertexColour(Mesh);
	}
	const FVector E = Def->BoxExtent;
	if (Def->bLiquid)
	{
		WaterDisc = AddPart(Shape(TEXT("Cylinder")), FVector(0.0f, 0.0f, -E.Z + 2.0f), FVector(0.33f, 0.30f, 0.02f), Water, false);
	}
	if (Def->bFlame)
	{
		FlameMesh = AddPart(Shape(TEXT("Sphere")), FVector(0.0f, 0.0f, E.Z + 4.0f), FVector(0.05f, 0.05f, 0.08f), FLinearColor(1.0f, 0.62f, 0.2f), true);
		FlameLight = NewObject<UPointLightComponent>(this);
		FlameLight->SetMobility(EComponentMobility::Movable);
		FlameLight->SetIntensityUnits(ELightUnits::Candelas);
		FlameLight->SetIntensity(18.0f);
		FlameLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.25f));
		FlameLight->SetAttenuationRadius(420.0f);
		FlameLight->SetCastShadows(false);
		FlameLight->SetupAttachment(Box);
		FlameLight->SetRelativeLocation(FVector(0.0f, 0.0f, E.Z + 8.0f));
		FlameLight->RegisterComponent();
	}
	// Pieces you hand out one by one (loaves on the basket, letters on the bundle); fish on the crate.
	if (Kind == TEXT("Loaves"))
	{
		UStaticMesh* Bread = Load<UStaticMesh>(TEXT("/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_Bread.SM_KG_Bread"));
		for (int32 i = 0; i < Def->Count; ++i)
		{
			PieceMeshes.Add(AddPart(Bread, FVector(-16.0f + 16.0f * i, 0.0f, E.Z - 4.0f), FVector(0.85f), FLinearColor::White, false));
		}
	}
	else if (Kind == TEXT("Letters"))
	{
		UStaticMesh* Scroll = Load<UStaticMesh>(TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Scroll_1.Scroll_1"));
		for (int32 i = 0; i < Def->Count; ++i)
		{
			UStaticMeshComponent* S = AddPart(Scroll, FVector(-6.0f + 6.0f * i, 0.0f, E.Z + 2.0f + i * 1.5f), FVector(0.9f), FLinearColor::White, false);
			if (S)
			{
				S->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
			}
			PieceMeshes.Add(S);
		}
	}
	else if (Kind == TEXT("Crate"))
	{
		UStaticMesh* Fish = Load<UStaticMesh>(TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Fish_Cod.SM_KG_Fish_Cod"));
		FishMeshes.Add(AddPart(Fish, FVector(-6.0f, -8.0f, E.Z + 3.0f), FVector(0.9f), FLinearColor::White, false));
		FishMeshes.Add(AddPart(Fish, FVector(5.0f, 8.0f, E.Z + 4.0f), FVector(0.85f, 0.85f, 0.85f), FLinearColor::White, false));
		FishMeshes.RemoveAll([](const TObjectPtr<UStaticMeshComponent>& C) { return C == nullptr; });
		for (const UStaticMeshComponent* F : FishMeshes)
		{
			FishBase.Add(F->GetRelativeLocation());
		}
	}
	if (Kind == TEXT("Loaves"))
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (UStaticMeshComponent* Puff = AddPart(Shape(TEXT("Sphere")), FVector(-14.0f + 9.0f * i, 0.0f, E.Z + 6.0f), FVector(0.04f),
			                                         FLinearColor(0.92f, 0.92f, 0.9f), false))
			{
				SteamPuffs.Add(Puff);
			}
		}
	}
	OnRep_Attached();
	LastFill = Fill;
	UpdateVisual(0.0f);
}

void AKGChoreItem::UpdateVisual(float DeltaSeconds)
{
	const FKGWorldItemDef* Def = GetDef();
	if (!Def)
	{
		return;
	}
	if (WaterDisc)
	{
		WaterDisc->SetVisibility(Fill > 0.02f);
		WaterDisc->SetRelativeLocation(FVector(0.0f, 0.0f, -Def->BoxExtent.Z + 3.0f + (2.0f * Def->BoxExtent.Z - 7.0f) * Fill));
		// Slosh: the surface rocks harder the faster the carrier goes (a warning before it spills).
		const APawn* By = Carriers.Num() > 0 ? Carriers[0].Get() : nullptr;
		const float Speed = By ? static_cast<float>(By->GetVelocity().Size2D()) : static_cast<float>(GetVelocity().Size());
		SloshAmp = FMath::FInterpTo(SloshAmp, FMath::Clamp((Speed - 120.0f) / 260.0f, 0.0f, 1.0f), DeltaSeconds, 5.0f);
		SloshTime += DeltaSeconds * (5.0f + 9.0f * SloshAmp);
		WaterDisc->SetRelativeRotation(FRotator(16.0f * SloshAmp * FMath::Sin(SloshTime), 0.0f, 16.0f * SloshAmp * FMath::Cos(SloshTime * 1.3f)));
	}
	// A fish flops on the crate every couple of seconds: a quick hop and a twist.
	if (FishMeshes.Num() > 0)
	{
		FlopClock += DeltaSeconds;
		const float Period = 2.3f + 0.7f * (FlopIndex % 3);
		if (FlopClock > Period)
		{
			FlopClock = 0.0f;
			++FlopIndex;
		}
		for (int32 i = 0; i < FishMeshes.Num() && i < FishBase.Num(); ++i)
		{
			UStaticMeshComponent* F = FishMeshes[i];
			const bool bFlop = F && (FlopIndex % FishMeshes.Num()) == i && FlopClock < 0.5f;
			const float T = bFlop ? FlopClock / 0.5f : 0.0f;
			if (F)
			{
				F->SetRelativeLocation(FishBase[i] + FVector(0.0f, 0.0f, 9.0f * FMath::Sin(T * UE_PI)));
				F->SetRelativeRotation(FRotator(0.0f, 0.0f, 40.0f * FMath::Sin(T * UE_PI * 3.0f) * (1.0f - T)));
			}
		}
	}
	// Steam curls up off the fresh bread while there is bread in the basket.
	for (int32 i = 0; i < SteamPuffs.Num(); ++i)
	{
		if (UStaticMeshComponent* Puff = SteamPuffs[i])
		{
			const float T = FMath::Frac(FlameTime * 0.55f + i * 0.27f);
			Puff->SetVisibility(Pieces > 0);
			Puff->SetRelativeLocation(FVector(-14.0f + 9.0f * i + 4.0f * FMath::Sin(T * 6.0f + i), 0.0f, Def->BoxExtent.Z + 6.0f + 38.0f * T));
			Puff->SetRelativeScale3D(FVector(0.03f + 0.07f * FMath::Sin(T * UE_PI)));
		}
	}
	for (int32 i = 0; i < PieceMeshes.Num(); ++i)
	{
		if (PieceMeshes[i])
		{
			PieceMeshes[i]->SetVisibility(i < Pieces);
		}
	}
	FlameTime += DeltaSeconds;   // also the steam clock
	if (FlameMesh)
	{
		const float Flicker = 1.0f + 0.18f * FMath::Sin(FlameTime * 23.0f) + 0.1f * FMath::Sin(FlameTime * 37.0f);
		FlameMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.08f) * Flicker);
		if (FlameLight)
		{
			FlameLight->SetIntensity(18.0f * Flicker);
		}
	}
}

void AKGChoreItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SpillCueCooldown = FMath::Max(0.0f, SpillCueCooldown - DeltaSeconds);
	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateVisual(DeltaSeconds);
	}
}

void AKGChoreItem::TickServer(float DeltaSeconds)
{
	Carriers.RemoveAll([](const TObjectPtr<APawn>& P) { return !IsValid(P); });
	const FKGWorldItemDef* Def = GetDef();
	if (Def && Def->bLiquid && Fill > 0.0f)
	{
		float Fastest = 0.0f;
		bool bFalling = false;
		for (const APawn* P : Carriers)
		{
			Fastest = FMath::Max(Fastest, static_cast<float>(P->GetVelocity().Size2D()));
			const ACharacter* C = Cast<ACharacter>(P);
			bFalling |= C && C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling() && FMath::Abs(P->GetVelocity().Z) > 250.0f;
		}
		const float UpZ = bAttachedCarry ? 1.0f : static_cast<float>(GetActorUpVector().Z);
		const float Walk = FKGWorldChoreCatalog::Get().WalkSpeed;
		const float NewFill = FKGWorldChoreRules::SpillFill(Fill, DeltaSeconds, IsCarried(), Fastest, Walk, bFalling, UpZ);
		if (NewFill < Fill)
		{
			const bool bCrossed = FMath::FloorToInt(Fill * 10.0f) != FMath::FloorToInt(NewFill * 10.0f);
			Fill = NewFill;
			if (bCrossed)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SPILL %s fill=%.2f carried=%d speed=%.0f falling=%d upz=%.2f"), *Kind.ToString(), Fill,
				       IsCarried() ? 1 : 0, Fastest, bFalling ? 1 : 0, UpZ);
			}
			if (GetNetMode() != NM_DedicatedServer)
			{
				OnRep_Fill();
			}
		}
	}
	if (IsCarried())
	{
		IdleSeconds = 0.0f;
		CarryLogClock += DeltaSeconds;
		if (CarryLogClock >= 3.0f)
		{
			CarryLogClock = 0.0f;
			const APawn* By = Carriers[0];
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_CARRYPOS %s at %s by %s at %s vel=%.0f sim=%d fill=%.2f"), *Kind.ToString(),
			       *GetActorLocation().ToCompactString(), *GetNameSafe(By), By ? *By->GetActorLocation().ToCompactString() : TEXT("-"),
			       By ? By->GetVelocity().Size2D() : 0.0f, Box->IsSimulatingPhysics() ? 1 : 0, Fill);
		}
	}
	else if (bOrphan || !IsValid(OwnerPlayer))
	{
		IdleSeconds += DeltaSeconds;
		if (IdleSeconds > 30.0f)
		{
			AuthConsume();
		}
	}
}

bool AKGChoreItem::IsCarriedBy(const APawn* Pawn) const
{
	return Pawn && Carriers.Contains(Pawn);
}

AKGChoreItem* AKGChoreItem::CarriedBy(const APawn* Pawn)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AKGChoreItem> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->IsCarriedBy(Pawn))
		{
			return *It;
		}
	}
	return nullptr;
}

float AKGChoreItem::SpeedFactorFor(const APawn* Pawn)
{
	const AKGChoreItem* Item = CarriedBy(Pawn);
	const FKGWorldItemDef* Def = Item ? Item->GetDef() : nullptr;
	return Def ? FKGWorldChoreRules::CarrySpeedFactor(*Def, Item->Carriers.Num()) : 1.0f;
}

APawn* AKGChoreItem::AuthAddCarrier(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn || Carriers.Contains(Pawn))
	{
		return nullptr;
	}
	const FKGWorldItemDef* Def = GetDef();
	const int32 MaxHands = Def && Def->bTwoPerson ? 2 : 1;
	APawn* Snatched = nullptr;
	if (Carriers.Num() >= MaxHands)
	{
		Snatched = Carriers[0];
		Carriers.RemoveAt(0);
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SNATCH %s taken from %s by %s"), *Kind.ToString(), *GetNameSafe(Snatched), *Pawn->GetName());
	}
	Carriers.Add(Pawn);
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_CARRY %s by %s carriers=%d speed=%.2f"), *Kind.ToString(), *Pawn->GetName(), Carriers.Num(),
	       Def ? FKGWorldChoreRules::CarrySpeedFactor(*Def, Carriers.Num()) : 1.0f);
	ForceNetUpdate();
	return Snatched;
}

void AKGChoreItem::AuthRemoveCarrier(APawn* Pawn)
{
	if (HasAuthority() && Carriers.Remove(Pawn) > 0)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DROP %s by %s carriers=%d fill=%.2f"), *Kind.ToString(), *GetNameSafe(Pawn), Carriers.Num(), Fill);
		ForceNetUpdate();
	}
}

void AKGChoreItem::AuthAttachTo(APawn* Bot)
{
	if (!HasAuthority() || !Bot)
	{
		return;
	}
	// One hug at a time: whatever else this bot carried goes down first.
	while (AKGChoreItem* Other = CarriedBy(Bot))
	{
		if (Other == this || !Other->IsAttached())
		{
			break;
		}
		Other->AuthDetach(FVector::ZeroVector);
	}
	Carriers.Reset();
	Carriers.Add(Bot);
	bAttachedCarry = true;
	OnRep_Attached();
	AttachToComponent(Bot->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(48.0f, 0.0f, 12.0f));
	SetActorRelativeRotation(FRotator::ZeroRotator);
	ForceNetUpdate();
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_CARRY %s by %s (attached)"), *Kind.ToString(), *Bot->GetName());
}

void AKGChoreItem::AuthDetach(const FVector& Impulse)
{
	if (!HasAuthority() || !bAttachedCarry)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bAttachedCarry = false;
	Carriers.Reset();
	OnRep_Attached();
	if (!Impulse.IsNearlyZero())
	{
		Box->AddImpulse(Impulse, NAME_None, true);
	}
	ForceNetUpdate();
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DROP %s (detached) fill=%.2f"), *Kind.ToString(), Fill);
}

void AKGChoreItem::AuthSetFill(float NewFill)
{
	Fill = FMath::Clamp(NewFill, 0.0f, 1.0f);
	LastFill = Fill;
	ForceNetUpdate();
}

void AKGChoreItem::AuthSetPieces(int32 NewPieces)
{
	Pieces = FMath::Max(0, NewPieces);
	ForceNetUpdate();
}

void AKGChoreItem::AuthConsume()
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	if (bAttachedCarry)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	Carriers.Reset();
	Destroy();
}
