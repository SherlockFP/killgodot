#include "Cosmetics/KGCosmeticsComponent.h"
#include "AnimationRuntime.h"
#include "Character/KGCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Cosmetics/KGCosmeticCatalog.h"
#include "Cosmetics/KGProfileSave.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/ScaleMatrix.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

namespace KGCosmeticsPrivate
{
	bool IsLocalOwner(const UActorComponent* Component)
	{
		const APlayerState* PS = Cast<APlayerState>(Component->GetOwner());
		const APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
		return PC && PC->IsLocalController();
	}
}

UKGCosmeticsComponent::UKGCosmeticsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UKGCosmeticsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;   // public: everyone sees your hat
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGCosmeticsComponent, Equipped, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGCosmeticsComponent, Look, Params);   // public too: everyone renders your body
}

uint64 UKGCosmeticsComponent::LookSeed(const UWorld* World)
{
	const AKGGameMode* GM = World ? World->GetAuthGameMode<AKGGameMode>() : nullptr;
	if (GM && GM->GetMatchSeed() != 0)
	{
		return static_cast<uint64>(GM->GetMatchSeed());
	}
	// Pre-match lobby: one seed per server process, so a lobby's looks differ from the last lobby's.
	static const uint64 LobbySeed = (static_cast<uint64>(FMath::Rand()) << 32) ^ FPlatformTime::Cycles64();
	return LobbySeed;
}

void UKGCosmeticsComponent::AssignLook(FName PreferredArchetype)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (!PS || !GetOwner()->HasAuthority())
	{
		return;
	}
	TArray<FKGVillagerLook> Taken;
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const APlayerState* Other : GS->PlayerArray)
		{
			const UKGCosmeticsComponent* Theirs = Other && Other != PS ? FindForPlayer(Other) : nullptr;
			if (Theirs && Theirs->Look.IsAssigned())
			{
				Taken.Add(Theirs->Look);
			}
		}
	}
	const FKGVillagerLook New = FKGVillagerLookGen::Generate(LookSeed(GetWorld()), PS->GetPlayerId(), Taken, PreferredArchetype);
	PreferredLook = PreferredArchetype;
	if (New == Look)
	{
		return;
	}
	Look = New;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGCosmeticsComponent, Look, this);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogKillGodot, Log, TEXT("Look for %s: %s"), *PS->GetPlayerName(), *Look.ToString());
}

void UKGCosmeticsComponent::ServerSetPreferredLook_Implementation(FName Archetype)
{
	if (!FKGVillagerLookGen::FindArchetype(Archetype))
	{
		Archetype = NAME_None;
	}
	if (Archetype != PreferredLook || !Look.IsAssigned())
	{
		AssignLook(Archetype);
	}
}

UKGCosmeticsComponent* UKGCosmeticsComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGCosmeticsComponent>() : nullptr;
}

TMap<FName, FTransform>& UKGCosmeticsComponent::DevPlacementOverrides()
{
	static TMap<FName, FTransform> Overrides;
	return Overrides;
}

FTransform UKGCosmeticsComponent::ComputeBoneRelative(const FTransform& PlacementCS, const FTransform& BoneRefCS,
                                                      bool bMirrorX)
{
	// mesh -> component (placement) [-> mirrored across the body's YZ plane] -> bone local (inverse ref pose).
	FMatrix Matrix = PlacementCS.ToMatrixWithScale();
	if (bMirrorX)
	{
		Matrix = Matrix * FScaleMatrix(FVector(-1.0, 1.0, 1.0));
	}
	Matrix = Matrix * BoneRefCS.ToMatrixWithScale().Inverse();
	return FTransform(Matrix);
}

void UKGCosmeticsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		PS->OnPawnSet.AddDynamic(this, &UKGCosmeticsComponent::HandlePawnSet);
	}
	ProfileHandle = UKGProfileSave::OnProfileChanged().AddUObject(this, &UKGCosmeticsComponent::HandleProfileChanged);
}

void UKGCosmeticsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UKGProfileSave::OnProfileChanged().Remove(ProfileHandle);
	if (APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		PS->OnPawnSet.RemoveDynamic(this, &UKGCosmeticsComponent::HandlePawnSet);
	}
	ClearVisuals();
	Super::EndPlay(EndPlayReason);
}

void UKGCosmeticsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (!PS)
	{
		return;
	}
	if (GetOwner()->HasAuthority() && PS->IsABot() && Equipped.Num() == 0 && !PS->GetPlayerName().IsEmpty())
	{
		AssignBotLoadout();
	}
	if (GetOwner()->HasAuthority() && !Look.IsAssigned())
	{
		AssignLook(PreferredLook);   // SPRINT-027a: every player (bots too) gets a unique seeded villager look
	}
	if (!bPushedLocalLoadout && KGCosmeticsPrivate::IsLocalOwner(this))
	{
		PushLocalLoadout();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		const AKGCharacter* Character = Cast<AKGCharacter>(PS->GetPawn());
		if (Character && (Character != Decorated.Get() || AppliedIds != Equipped))
		{
			RefreshVisuals();
		}
	}
}

void UKGCosmeticsComponent::PushLocalLoadout()
{
	bPushedLocalLoadout = true;
	const TArray<FName> Ids = UKGProfileSave::GetProfile()->GetEquippedIds();
	const FName Preferred = UKGProfileSave::GetProfile()->GetPreferredLook();
	if (GetOwner()->HasAuthority())
	{
		SetLoadout(Ids);
		ServerSetPreferredLook_Implementation(Preferred);
	}
	else
	{
		ServerSetLoadout(Ids);
		ServerSetPreferredLook(Preferred);
	}
}

void UKGCosmeticsComponent::ServerSetLoadout_Implementation(const TArray<FName>& Ids)
{
	SetLoadout(Ids);
}

void UKGCosmeticsComponent::SetLoadout(const TArray<FName>& Ids)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	// Cosmetic only, so client-claimed ownership is accepted until the online inventory exists (Docs 07 section 8).
	TArray<FName> BySlot;
	BySlot.SetNum(static_cast<int32>(EKGCosmeticSlot::Count));
	for (int32 i = 0; i < FMath::Min(Ids.Num(), 16); ++i)
	{
		if (const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Ids[i]))
		{
			BySlot[static_cast<int32>(Def->Slot)] = Def->Id;
		}
	}
	TArray<FName> Clean;
	for (const FName Id : BySlot)
	{
		if (!Id.IsNone())
		{
			Clean.Add(Id);
		}
	}
	if (Clean == Equipped)
	{
		return;
	}
	Equipped = Clean;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGCosmeticsComponent, Equipped, this);
	GetOwner()->ForceNetUpdate();
	RefreshVisuals();
}

void UKGCosmeticsComponent::AssignBotLoadout()
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	// Visual variety only (not gameplay): a stable hash of the bot, no RNG stream consumed.
	uint32 Hash = HashCombine(GetTypeHash(PS->GetPlayerName()), GetTypeHash(PS->GetPlayerId()));
	TArray<FName> Ids;
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EKGCosmeticSlot::Count); ++SlotIndex)
	{
		TArray<const FKGCosmeticDef*> Options;
		for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
		{
			if (static_cast<int32>(Def.Slot) == SlotIndex)
			{
				Options.Add(&Def);
			}
		}
		const int32 ChancePercent = SlotIndex == 0 ? 75 : 30;
		if (Options.Num() > 0 && static_cast<int32>(Hash % 100u) < ChancePercent)
		{
			Ids.Add(Options[(Hash / 100u) % static_cast<uint32>(Options.Num())]->Id);
		}
		Hash = Hash * 2654435761u + 0x9E3779B9u;
	}
	if (Ids.Num() == 0)
	{
		Ids.Add(FName(TEXT("Hat_Bucket")));
	}
	SetLoadout(Ids);
}

void UKGCosmeticsComponent::OnRep_Equipped()
{
	RefreshVisuals();
}

void UKGCosmeticsComponent::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	RefreshVisuals();
}

void UKGCosmeticsComponent::HandleProfileChanged()
{
	if (KGCosmeticsPrivate::IsLocalOwner(this))
	{
		PushLocalLoadout();
	}
}

void UKGCosmeticsComponent::ClearVisuals()
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& Component : Spawned)
	{
		if (UStaticMeshComponent* Mesh = Component.Get())
		{
			Mesh->DestroyComponent();
		}
	}
	Spawned.Reset();
	AppliedIds.Reset();
}

void UKGCosmeticsComponent::RefreshVisuals()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	AKGCharacter* Character = PS ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
	if (!Character)
	{
		return;   // ghost/spectator: the corpse keeps its hat
	}
	ClearVisuals();
	Decorated = Character;
	for (const FName Id : Equipped)
	{
		if (const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id))
		{
			AttachOne(*Def, Character, false);
			if (!Def->MirrorBone.IsNone())
			{
				AttachOne(*Def, Character, true);
			}
		}
	}
	AppliedIds = Equipped;
}

void UKGCosmeticsComponent::AttachOne(const FKGCosmeticDef& Def, AKGCharacter* Character, bool bMirror)
{
	USkeletalMeshComponent* Body = Character->GetMesh();
	const USkeletalMesh* Skeletal = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Skeletal)
	{
		return;
	}
	const bool bFemale = Skeletal->GetName().EndsWith(TEXT("_F"));
	const FSoftObjectPath& Path = bFemale && Def.FemaleMesh.IsValid() ? Def.FemaleMesh : Def.Mesh;
	UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
	if (!Mesh)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Cosmetic %s: mesh %s missing (run Tools/Unreal/kg_import_outfits.py)"),
		       *Def.Id.ToString(), *Path.ToString());
		return;
	}
	const FName Bone = bMirror ? Def.MirrorBone : Def.AttachBone;
	const FReferenceSkeleton& RefSkeleton = Skeletal->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Cosmetic %s: bone %s not on %s"), *Def.Id.ToString(), *Bone.ToString(),
		       *Skeletal->GetName());
		return;
	}
	const FTransform BoneRefCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
	const FTransform* Override = DevPlacementOverrides().Find(Def.Id);
	const FTransform Relative = ComputeBoneRelative(Override ? *Override : Def.Placement, BoneRefCS, bMirror);

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character, NAME_None, RF_Transient);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	// Like the body: invisible to its own first-person camera, still casting its shadow, inked by the outline pass.
	Component->SetOwnerNoSee(true);
	Component->bCastHiddenShadow = true;
	Component->SetRenderCustomDepth(true);
	Component->SetupAttachment(Body, Bone);
	Component->SetRelativeTransform(Relative);
	Component->RegisterComponent();
	if (Def.bTint)
	{
		for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = Component->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Tint"), Def.Tint);
			}
		}
	}
	Spawned.Add(Component);
}
