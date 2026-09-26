#include "Forest/KGForestActors.h"

#include "Animation/AnimSequence.h"
#include "AIController.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Forest/KGForestMap.h"
#include "Forest/KGForestSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

namespace KGForestLook
{
	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path);
	}

	UStaticMesh* Shape(const TCHAR* Name)
	{
		return Load<UStaticMesh>(*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	UMaterialInterface* Glow()
	{
		return Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreGlow.M_KG_ChoreGlow"));
	}

	UMaterialInterface* Smoke()
	{
		return Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreSmoke.M_KG_ChoreSmoke"));
	}

	UMaterialInterface* Solid()
	{
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	/** A basic-shape part with a flat colour / glow / smoke material. Mode 0 solid, 1 glow, 2 smoke. */
	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Rel, const FRotator& Rot,
	                           const FVector& Scale, const FLinearColor& Color, int32 Mode, float Param = 1.0f)
	{
		if (!Mesh)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(Mode == 0);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Rel);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		UMaterialInterface* Base = Mode == 1 ? Glow() : Mode == 2 ? Smoke() : Solid();
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			if (Mode == 1)
			{
				MID->SetScalarParameterValue(TEXT("Intensity"), Param);
			}
			else if (Mode == 2)
			{
				MID->SetScalarParameterValue(TEXT("Opacity"), Param);
			}
			C->SetMaterial(0, MID);
		}
		C->RegisterComponent();
		return C;
	}

	UStaticMeshComponent* Mesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Path, const FVector& Rel, const FRotator& Rot, float Scale)
	{
		UStaticMesh* M = Load<UStaticMesh>(Path);
		if (!M)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Rel);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(FVector(Scale));
		C->RegisterComponent();
		return C;
	}

	FVector GroundAt(const UWorld* World, const FVector& Hint, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("KGForestGround"), false, Ignore);
		const FVector From(Hint.X, Hint.Y, Hint.Z + 3000.0f);
		const FVector To(Hint.X, Hint.Y, Hint.Z - 6000.0f);
		if (World && World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q))
		{
			return Hit.ImpactPoint;
		}
		return Hint;
	}

	float Now(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	}

	const FLinearColor FlameCol(1.0f, 0.55f, 0.15f);
	const FLinearColor MistCol(0.80f, 0.86f, 0.92f);
}

using namespace KGForestLook;

int32 GKGForestAutoWalk = 0;

// ================================================================================================ player info
AKGForestPlayerInfo::AKGForestPlayerInfo()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(8.0f);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKGForestPlayerInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGForestPlayerInfo, Band);
	DOREPLIFETIME(AKGForestPlayerInfo, WolfStage);
	DOREPLIFETIME(AKGForestPlayerInfo, MistStage);
	DOREPLIFETIME(AKGForestPlayerInfo, Frost);
	DOREPLIFETIME(AKGForestPlayerInfo, SafeDir);
	DOREPLIFETIME(AKGForestPlayerInfo, SafeDist);
	DOREPLIFETIME(AKGForestPlayerInfo, Line);
	DOREPLIFETIME(AKGForestPlayerInfo, LineUntil);
	DOREPLIFETIME(AKGForestPlayerInfo, BittenAt);
	DOREPLIFETIME(AKGForestPlayerInfo, Vigil);
}

void AKGForestPlayerInfo::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_INFO_SEEN authority=0 own=%d"), GetOwner() && GetOwner() == PC ? 1 : 0);
	}
}

void AKGForestPlayerInfo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// kg.Forest.AutoWalk (smokes, dev): the local player walks (never runs) along the arrow to the nearest path
	const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (GKGForestAutoWalk != 0 && LocalPC && GetOwner() == LocalPC && !FVector(SafeDir).IsNearlyZero() &&
	    (GKGForestAutoWalk == 1 || MistStage == uint8(EKGMistStage::Tongue)))
	{
		if (APawn* P = LocalPC->GetPawn())
		{
			P->AddMovementInput(FVector(SafeDir), 1.0f);
			if (!bAutoWalkLogged)
			{
				bAutoWalkLogged = true;
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST autowalk moving dir=%s authority=%d"), *FVector(SafeDir).ToCompactString(), HasAuthority() ? 1 : 0);
			}
		}
	}
	if (HasAuthority())
	{
		return;
	}
	// Client log for the two-process smoke: every stage change this client's own info shows.
	const FString S = FString::Printf(TEXT("wolf=%s mist=%d band=%s"), KGWolfStageName(static_cast<EKGWolfStage>(WolfStage)),
	                                  MistStage, KGForestBandName(static_cast<EKGForestBand>(Band)));
	if (S != LastLoggedStage)
	{
		LastLoggedStage = S;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_CLIENT %s t=%.1f authority=0"), *S, Now(GetWorld()));
	}
}

AKGForestPlayerInfo* AKGForestPlayerInfo::FindLocal(const UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return nullptr;
	}
	for (TActorIterator<AKGForestPlayerInfo> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

// ================================================================================================ director
AKGForestDirector::AKGForestDirector()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKGForestDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

AKGForestDirector* AKGForestDirector::Get(const UWorld* World)
{
	for (TActorIterator<AKGForestDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AKGForestDirector::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildLanterns();
		BuildWall();
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_DIRECTOR up authority=%d"), HasAuthority() ? 1 : 0);
}

void AKGForestDirector::BuildLanterns()
{
	const FKGForestMap& Map = FKGForestMap::Get();
	UStaticMesh* Cyl = Shape(TEXT("Cylinder"));
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	int32 i = 0;
	for (const FVector2D& L : Map.Lanterns)
	{
		const FVector At = GroundAt(GetWorld(), FVector(L.X * 100.0f, L.Y * 100.0f, 2000.0f));
		USceneComponent* Post = NewObject<USceneComponent>(this);
		Post->SetupAttachment(RootComponent);
		Post->RegisterComponent();
		Post->SetWorldLocation(At);
		// A trail-head lantern: a short post, a hook, the lantern with a warm flame, and a small light (KG_LANTERN_SAFE).
		Part(this, Post, Cyl, FVector(0.0f, 0.0f, 90.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 1.8f), FLinearColor(0.22f, 0.13f, 0.07f), 0);
		Part(this, Post, Cyl, FVector(20.0f, 0.0f, 176.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.05f, 0.05f, 0.45f), FLinearColor(0.2f, 0.2f, 0.22f), 0);
		Mesh(this, Post, TEXT("/Game/KillGodot/Env/Ext/KG_Ext_KGraveyard/StaticMeshes/SM_KG_KGraveyard_lantern_candle.SM_KG_KGraveyard_lantern_candle"),
		     FVector(38.0f, 0.0f, 138.0f), FRotator::ZeroRotator, 1.0f);
		Part(this, Post, Sphere, FVector(38.0f, 0.0f, 152.0f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 0.12f), FlameCol, 1, 6.0f);
		if (i % 2 == 0)
		{
			// the village end of a trail: a signpost so the way into (and out of) the wood reads from afar
			Mesh(this, Post, TEXT("/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_SignPost.SM_KG_SignPost"),
			     FVector(-60.0f, 40.0f, 0.0f), FRotator(0.0f, 30.0f, 0.0f), 0.8f);
		}
		UPointLightComponent* PL = NewObject<UPointLightComponent>(this);
		PL->SetMobility(EComponentMobility::Movable);
		PL->SetIntensityUnits(ELightUnits::Candelas);
		PL->SetIntensity(35.0f);
		PL->SetLightColor(FLinearColor(1.0f, 0.68f, 0.35f));
		PL->SetAttenuationRadius(700.0f);
		PL->SetCastShadows(false);
		PL->SetupAttachment(Post);
		PL->SetRelativeLocation(FVector(38.0f, 0.0f, 150.0f));
		PL->RegisterComponent();
		++i;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST lanterns %d"), i);
}

void AKGForestDirector::BuildWall()
{
	// The Mist Wall: a ring of slow, pale fog banks at the forest boundary (translucent smoke, works headless).
	const FKGForestMap& Map = FKGForestMap::Get();
	if (!Map.IsValid())
	{
		return;
	}
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	const int32 N = 90;
	for (int32 k = 0; k < N; ++k)
	{
		const float Deg = -40.0f + 260.0f * k / float(N - 1);   // bearings: the land side only (0 = east, 90 = north)
		const float Rad = FMath::DegreesToRadians(Deg);
		const float R = (Map.ROut + 4.0f) * 100.0f;
		const FVector2D P(R * FMath::Cos(Rad), -R * FMath::Sin(Rad));
		const FVector At = GroundAt(GetWorld(), FVector(P.X, P.Y, 6000.0f));
		const FLinearColor C = FMath::Lerp(MistCol, FLinearColor(0.72f, 0.80f, 0.88f), (k % 3) / 3.0f);
		UStaticMeshComponent* Bank = Part(this, RootComponent, Sphere, At + FVector(0.0f, 0.0f, 250.0f), FRotator(0.0f, -Deg + 90.0f, 0.0f),
		                                  FVector(22.0f, 7.0f, 7.5f), C, 2, 0.32f);
		if (Bank)
		{
			Bank->SetUsingAbsoluteLocation(true);
			Bank->SetWorldLocation(At + FVector(0.0f, 0.0f, 250.0f));
			WallParts.Add(Bank);
		}
	}
}

void AKGForestDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	for (int32 i = 0; i < WallParts.Num(); ++i)
	{
		if (UStaticMeshComponent* P = WallParts[i])
		{
			const float S = 1.0f + 0.06f * FMath::Sin(Clock * 0.3f + i * 1.7f);
			P->SetRelativeScale3D(FVector(22.0f * S, 7.0f, 7.5f / S));
		}
	}
}

void AKGForestDirector::MulticastHowl_Implementation(FVector_NetQuantize At, uint8 Sector)
{
	LastHowlAt = At;
	LastHowlSector = Sector;
	LastHowlTime = Now(GetWorld());
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* S = KGAudio::Get(TEXT("S_Forest_Howl")))
		{
			UGameplayStatics::PlaySoundAtLocation(this, S, At, 1.0f, 1.0f, 0.0f, nullptr);
		}
	}
	if (!HasAuthority())
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_CLIENT howl heard sector=%s authority=0"), *FKGForestMap::SectorName(Sector));
	}
}

void AKGForestDirector::MulticastCue_Implementation(FVector_NetQuantize At, uint8 Cue)
{
	static const TCHAR* Names[] = {TEXT("S_Forest_Growl"), TEXT("S_Forest_Snarl"), TEXT("S_Forest_Whisper"), TEXT("S_Chore_Flame")};
	if (Cue < UE_ARRAY_COUNT(Names))
	{
		KGAudio::At(this, Names[Cue], At, 1.0f);
	}
}

// ================================================================================================ wolf
AKGWolf::AKGWolf()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetNetCullDistanceSquared(FMath::Square(9000.0f));
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Spawned;
	AIControllerClass = AAIController::StaticClass();
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 40.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);   // wolves never block players
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
	GetMesh()->SetRelativeScale3D(FVector(0.14f));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = 400.0f;
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	M->bUseRVOAvoidance = false;
	M->NavAgentProps.AgentRadius = 34.0f;
	bUseControllerRotationYaw = false;
}

void AKGWolf::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGWolf, Gait);
	DOREPLIFETIME(AKGWolf, bEyesLit);
	DOREPLIFETIME(AKGWolf, bAwake);
}

void AKGWolf::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* SK = Load<USkeletalMesh>(TEXT("/Game/KillGodot/Env/Ext/Characters/Animals_Quaternius/Wolf/Wolf1.Wolf1")))
	{
		GetMesh()->SetSkeletalMesh(SK);
	}
	ClipIdle = Load<UAnimSequence>(TEXT("/Game/KillGodot/Env/Ext/Characters/Animals_Quaternius/Wolf/WolfWolfArmature_Idle.WolfWolfArmature_Idle"));
	ClipWalk = Load<UAnimSequence>(TEXT("/Game/KillGodot/Env/Ext/Characters/Animals_Quaternius/Wolf/WolfWolfArmature_Walking.WolfWolfArmature_Walking"));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// Eyes on the head bone: the highest non-IK bone of the rig in the reference pose (Quaternius names are Bone.0xx).
	FName Head = NAME_None;
	float BestZ = -1e9f;
	for (int32 i = 0; i < GetMesh()->GetNumBones(); ++i)
	{
		const FName B = GetMesh()->GetBoneName(i);
		if (B.ToString().StartsWith(TEXT("IK")))
		{
			continue;
		}
		const FVector P = GetMesh()->GetBoneLocation(B, EBoneSpaces::ComponentSpace);
		if (P.Z + 0.25f * P.X > BestZ)
		{
			BestZ = P.Z + 0.25f * P.X;
			Head = B;
		}
	}
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	const FLinearColor EyeCol(1.0f, 0.78f, 0.25f);
	EyeL = Part(this, GetMesh(), Sphere, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.06f), EyeCol, 1, 60.0f);
	EyeR = Part(this, GetMesh(), Sphere, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.06f), EyeCol, 1, 60.0f);
	for (UStaticMeshComponent* E : {EyeL.Get(), EyeR.Get()})
	{
		if (E)
		{
			E->SetUsingAbsoluteScale(true);
			E->SetWorldScale3D(FVector(0.06f));
			if (Head != NAME_None)
			{
				E->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, Head);
			}
		}
	}
	// the eye offsets in the head bone's space are set in unscaled mesh units (x0.14 in the world)
	if (EyeL) { EyeL->SetRelativeLocation(FVector(55.0f, 30.0f, 40.0f)); }
	if (EyeR) { EyeR->SetRelativeLocation(FVector(55.0f, -30.0f, 40.0f)); }
	EyeLight = NewObject<UPointLightComponent>(this);
	EyeLight->SetMobility(EComponentMobility::Movable);
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(0.0f);
	EyeLight->SetLightColor(EyeCol);
	EyeLight->SetAttenuationRadius(120.0f);
	EyeLight->SetCastShadows(false);
	EyeLight->SetupAttachment(GetMesh(), Head);
	EyeLight->RegisterComponent();
	OnRep_Awake();   // asleep wolves stay hidden in their den (clients get no OnRep for the default)
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_WOLF_RIG head=%s bones=%d mesh=%d clips=%d/%d authority=%d"), *Head.ToString(),
	       GetMesh()->GetNumBones(), GetMesh()->GetSkeletalMeshAsset() ? 1 : 0, ClipIdle ? 1 : 0, ClipWalk ? 1 : 0, HasAuthority() ? 1 : 0);
	ApplyLook();
}

void AKGWolf::SetAwake(bool bIn)
{
	if (bAwake != bIn)
	{
		bAwake = bIn;
		OnRep_Awake();
		ForceNetUpdate();
	}
}

void AKGWolf::OnRep_Awake()
{
	SetActorHiddenInGame(!bAwake);
	GetCharacterMovement()->SetComponentTickEnabled(bAwake);
}

void AKGWolf::MulticastSnarl_Implementation()
{
	KGAudio::At(this, TEXT("S_Forest_Snarl"), GetActorLocation(), 1.0f);
	if (!HasAuthority())
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_CLIENT wolf snarl %s authority=0"), *GetName());
	}
}

void AKGWolf::ApplyLook()
{
	const bool bLit = bEyesLit && bAwake;
	for (UStaticMeshComponent* E : {EyeL.Get(), EyeR.Get()})
	{
		if (E)
		{
			E->SetVisibility(bLit);
		}
	}
	if (EyeLight)
	{
		EyeLight->SetIntensity(bLit ? 4.0f : 0.0f);
	}
	if (ShownGait != Gait && GetMesh()->GetSkeletalMeshAsset())
	{
		ShownGait = Gait;
		UAnimSequence* Clip = Gait == 0 ? ClipIdle : ClipWalk;
		if (Clip)
		{
			GetMesh()->PlayAnimation(Clip, true);
			const float Rate[] = {1.0f, 0.8f, 1.6f, 2.6f, 2.2f};
			GetMesh()->SetPlayRate(Rate[FMath::Min<int32>(Gait, 4)]);
		}
	}
}

void AKGWolf::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyLook();
	if (!HasAuthority() && bAwake && !bSeenLogged)
	{
		bSeenLogged = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_WOLF_SEEN %s at %s authority=0"), *GetName(), *GetActorLocation().ToCompactString());
	}
}

// ================================================================================================ mist tongue
AKGMistTongue::AKGMistTongue()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKGMistTongue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGMistTongue, Density);
	DOREPLIFETIME(AKGMistTongue, TargetPS);
}

void AKGMistTongue::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// A rolling bank: a low wide core, a crown of puffs and a trailing tail (translucent, unlit, soft edges).
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	FRandomStream R(GetUniqueID());
	for (int32 i = 0; i < 22; ++i)
	{
		const float T = i / 21.0f;
		const FVector Rel(-900.0f * T + R.FRandRange(-120.0f, 120.0f), R.FRandRange(-260.0f, 260.0f) * (1.0f - 0.4f * T),
		                  60.0f + R.FRandRange(0.0f, 180.0f) * (1.0f - T));
		const float S = FMath::Lerp(6.5f, 3.0f, T) * R.FRandRange(0.8f, 1.25f);
		UStaticMeshComponent* P = Part(this, RootComponent, Sphere, Rel, FRotator::ZeroRotator, FVector(S, S, S * 0.55f),
		                               FMath::Lerp(MistCol, FLinearColor(0.62f, 0.70f, 0.78f), R.FRand() * 0.6f), 2, 0.0f);
		if (P)
		{
			Puffs.Add(P);
			PuffBase.Add(Rel);
		}
	}
}

void AKGMistTongue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		UStaticMeshComponent* P = Puffs[i];
		if (!P)
		{
			continue;
		}
		const FVector B = PuffBase[i];
		P->SetRelativeLocation(B + FVector(40.0f * FMath::Sin(Clock * 0.7f + i), 50.0f * FMath::Sin(Clock * 0.5f + i * 2.1f),
		                                    25.0f * FMath::Sin(Clock * 0.9f + i * 0.7f)));
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(P->GetMaterial(0)))
		{
			MID->SetScalarParameterValue(TEXT("Opacity"), Density * (0.34f - 0.12f * (i / float(Puffs.Num()))));
		}
	}
	if (!HasAuthority() && Density > 0.1f && !bSeenLogged)
	{
		bSeenLogged = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_MIST_SEEN %s target=%s authority=0"), *GetName(),
		       TargetPS ? *TargetPS->GetPlayerName() : TEXT("?"));
	}
}

// ================================================================================================ camp fire
AKGCampfire::AKGCampfire()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetBoxExtent(FVector(90.0f, 90.0f, 50.0f));
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Hitbox->SetCollisionProfileName(TEXT("NoCollision"));
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AKGCampfire::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGCampfire, Fuel);
	DOREPLIFETIME(AKGCampfire, Ash);
}

AKGCampfire* AKGCampfire::Get(const UWorld* World)
{
	for (TActorIterator<AKGCampfire> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AKGCampfire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (!Logs && GetNetMode() != NM_DedicatedServer)
	{
		Logs = Mesh(this, Root, TEXT("/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_Campfire.SM_KG_Campfire"),
		            FVector::ZeroVector, FRotator::ZeroRotator, 0.8f);
		UStaticMesh* Cone = Shape(TEXT("Cone"));
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = i * 72.0f;
			const FVector Rel(22.0f * FMath::Cos(FMath::DegreesToRadians(A)), 22.0f * FMath::Sin(FMath::DegreesToRadians(A)), 40.0f);
			if (UStaticMeshComponent* F = Part(this, Root, Cone, i == 0 ? FVector(0.0f, 0.0f, 55.0f) : Rel, FRotator::ZeroRotator,
			                                   i == 0 ? FVector(0.5f, 0.5f, 1.1f) : FVector(0.3f, 0.3f, 0.7f),
			                                   i == 0 ? FLinearColor(1.0f, 0.75f, 0.25f) : FlameCol, 1, 8.0f))
			{
				Flames.Add(F);
			}
		}
		Light = NewObject<UPointLightComponent>(this);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(1.0f, 0.6f, 0.28f));
		Light->SetAttenuationRadius(2400.0f);
		Light->SetCastShadows(false);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
		Light->RegisterComponent();
	}
	const bool bLit = IsLit();
	for (int32 i = 0; i < Flames.Num(); ++i)
	{
		if (UStaticMeshComponent* F = Flames[i])
		{
			F->SetVisibility(bLit);
			if (bLit)
			{
				const float Size = FMath::Clamp(0.45f + Fuel / 100.0f, 0.5f, 1.4f);
				const float Flick = 1.0f + 0.18f * FMath::Sin(Clock * (13.0f + i * 3.0f)) + 0.1f * FMath::Sin(Clock * 29.0f + i);
				const FVector Base = i == 0 ? FVector(0.5f, 0.5f, 1.1f) : FVector(0.3f, 0.3f, 0.7f);
				F->SetRelativeScale3D(Base * Size * FVector(1.0f, 1.0f, Flick));
			}
		}
	}
	if (Light)
	{
		Light->SetIntensity(bLit ? (150.0f + 1.5f * Fuel) * (1.0f + 0.12f * FMath::Sin(Clock * 11.0f)) : 0.0f);
	}
	if (HasAuthority() && bLit)
	{
		Fuel = FKGForestRules::FireStep(Fuel, DeltaSeconds);
		if (Fuel <= 0.0f)
		{
			Ash = 1;
			UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL fire out (cold ash)"));
		}
	}
}

FString AKGCampfire::AuthUse(AKGCharacter* By)
{
	FString What;
	if (Ash == 0 && Fuel <= 0.0f)
	{
		Fuel = KGForest::FireFuelMax;   // the laid fresh logs catch: a full fire (40 s)
		What = TEXT("The camp fire catches.");
	}
	else if (Fuel >= KGForest::FireFuelMax - 1.0f)
	{
		What = TEXT("The fire is burning high already.");
	}
	else if (UKGForestSubsystem* FS = UKGForestSubsystem::Get(GetWorld()); FS && FS->TakeCampLog())
	{
		AuthAddLog();
		What = IsLit() ? TEXT("You feed the fire a log.") : TEXT("You relight the fire with a log from the woodpile.");
	}
	else
	{
		What = TEXT("The camp woodpile is empty - gather deadwood in the forest.");
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL fire use by=%s fuel=%.0f ash=%d: %s"), By ? *By->GetName() : TEXT("?"), Fuel, Ash, *What);
	ForceNetUpdate();
	return What;
}

void AKGCampfire::AuthAddLog()
{
	Fuel = FKGForestRules::FireAddLog(Fuel);
	Ash = 0;
	ForceNetUpdate();
}

void AKGCampfire::AuthReset()
{
	Fuel = 0.0f;
	Ash = 0;
	ForceNetUpdate();
}

void AKGCampfire::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By)
	{
		return;
	}
	if (UKGForestSubsystem* FS = UKGForestSubsystem::Get(GetWorld()))
	{
		FS->Tell(By->GetPlayerState<AKGPlayerState>(), AuthUse(By));
	}
}

FText AKGCampfire::GetInteractPrompt_Implementation() const
{
	if (Ash == 0 && Fuel <= 0.0f)
	{
		return FText::FromString(TEXT("Light the camp fire"));
	}
	return FText::FromString(FString::Printf(TEXT("Feed the fire (%d%%)"), FMath::RoundToInt(Fuel)));
}

// ================================================================================================ vigil book
AKGVigilBook::AKGVigilBook()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetBoxExtent(FVector(35.0f, 35.0f, 60.0f));
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	Hitbox->SetCollisionProfileName(TEXT("NoCollision"));
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(14.0f);
	Label->SetTextRenderColor(FColor(255, 226, 170));
	Label->SetText(FText::FromString(TEXT("Camp Vigil")));
}

void AKGVigilBook::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGVigilBook, NumSigned);
	DOREPLIFETIME(AKGVigilBook, MaxSigners);
	DOREPLIFETIME(AKGVigilBook, bOpen);
}

AKGVigilBook* AKGVigilBook::Get(const UWorld* World)
{
	for (TActorIterator<AKGVigilBook> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AKGVigilBook::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By)
	{
		return;
	}
	UKGForestSubsystem* FS = UKGForestSubsystem::Get(GetWorld());
	AKGPlayerState* PS = By->GetPlayerState<AKGPlayerState>();
	FString Why;
	if (FS && PS)
	{
		FS->Tell(PS, FS->SignVigil(PS, Why) ? TEXT("You signed the Camp Vigil: keep the camp fire burning tonight.") : Why);
	}
}

FText AKGVigilBook::GetInteractPrompt_Implementation() const
{
	return FText::FromString(bOpen ? FString::Printf(TEXT("Sign the Camp Vigil (%d/%d)"), NumSigned, MaxSigners)
	                               : FString(TEXT("Camp Vigil book (closed)")));
}

// ================================================================================================ bite evidence
AKGBiteEvidence::AKGBiteEvidence()
{
	bReplicates = true;
	SetReplicatingMovement(true);   // the attachment to the body replicates with the movement data
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetBoxExtent(FVector(45.0f, 45.0f, 30.0f));
	Hitbox->SetCollisionProfileName(TEXT("NoCollision"));
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AKGBiteEvidence::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGBiteEvidence, Kind);
	DOREPLIFETIME(AKGBiteEvidence, Wounds);
	DOREPLIFETIME(AKGBiteEvidence, Sector);
}

void AKGBiteEvidence::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Look();
}

void AKGBiteEvidence::AuthAddWound()
{
	Wounds = static_cast<uint8>(FMath::Min(255, Wounds + 1));
	OnRep_Look();
	ForceNetUpdate();
}

void AKGBiteEvidence::OnRep_Look()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Torn, ragged marks: pairs of dark-red gashes per bite (a wolf), or a rime of white frost (the Mist).
	UStaticMesh* Cube = Shape(TEXT("Cube"));
	const bool bFrost = Kind == TEXT("frost");
	const int32 Want = bFrost ? 6 : FMath::Min<int32>(Wounds, 6) * 3;
	while (Marks.Num() < Want)
	{
		const int32 i = Marks.Num();
		const float A = i * 53.0f;
		const FVector Rel(18.0f * FMath::Cos(FMath::DegreesToRadians(A)), 18.0f * FMath::Sin(FMath::DegreesToRadians(A)), -10.0f + 9.0f * (i % 5));
		UStaticMeshComponent* M = Part(this, Root, Cube, Rel, FRotator(20.0f * (i % 3), A + 35.0f, 10.0f),
		                               bFrost ? FVector(0.22f, 0.1f, 0.02f) : FVector(0.16f, 0.018f, 0.018f),
		                               bFrost ? FLinearColor(0.85f, 0.93f, 1.0f) : FLinearColor(0.42f, 0.02f, 0.02f), bFrost ? 1 : 0, 1.5f);
		if (!M)
		{
			break;
		}
		Marks.Add(M);
	}
}

void AKGBiteEvidence::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By)
	{
		return;
	}
	const FString Text = Kind == TEXT("frost")
		? FString(TEXT("White rime on the skin and clothes, eyes glassy. The cold took them - the Mist."))
		: FString::Printf(TEXT("Torn, ragged bite wounds (%d), paw prints in the mud - a wolf, not a blade. Found in the %s wood."),
		                  Wounds, Sector.IsEmpty() ? TEXT("deep") : *Sector);
	if (UKGForestSubsystem* FS = UKGForestSubsystem::Get(GetWorld()))
	{
		FS->Tell(By->GetPlayerState<AKGPlayerState>(), Text, 7.0f);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST evidence read by=%s kind=%s wounds=%d"), *By->GetName(), *Kind.ToString(), Wounds);
}

FText AKGBiteEvidence::GetInteractPrompt_Implementation() const
{
	return FText::FromString(Kind == TEXT("frost") ? TEXT("Examine the frost") : TEXT("Examine the wounds"));
}
