#include "Character/KGAppearanceComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Character/KGCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGPlayerState.h"
#include "Cosmetics/KGCosmeticCatalog.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Cosmetics/KGProfileSave.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"

namespace KGAppearancePrivate
{
	const TCHAR* VarietyRoot = TEXT("/Game/KillGodot/Characters/VillagerVariety");

	template <typename T>
	T* Load(const FString& Path)
	{
		return LoadObject<T>(nullptr, *Path);
	}

	FString Piece(const TCHAR* Folder, const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s/%s/StaticMeshes/%s.%s"), VarietyRoot, Folder, Name, Name);
	}

	bool SlotIs(const FName& Slot, const TCHAR* Keyword)
	{
		return Slot.ToString().Contains(Keyword);
	}
}

UKGAppearanceComponent::UKGAppearanceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(false);   // nothing here ever replicates (see the class comment)
}

void UKGAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UKGAppearanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAttachments();
	if (UStaticMeshComponent* S = Sash.Get())
	{
		S->DestroyComponent();
	}
	if (UStaticMeshComponent* C = Cuff.Get())
	{
		C->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void UKGAppearanceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFromPlayerState();
}

AKGCharacter* UKGAppearanceComponent::Character() const
{
	return Cast<AKGCharacter>(GetOwner());
}

// ---- static tables ------------------------------------------------------------------------------------------

FLinearColor UKGAppearanceComponent::AlignmentColour(EKGAlignment Alignment)
{
	switch (Alignment)
	{
	case EKGAlignment::Town: return FLinearColor(0.20f, 0.50f, 1.00f);
	case EKGAlignment::Impatient: return FLinearColor(1.00f, 0.12f, 0.10f);
	default: return FLinearColor(1.00f, 0.70f, 0.10f);
	}
}

EKGAlignment UKGAppearanceComponent::AlignmentOfRole(FName RoleId)
{
	const FKGRoleInfo* Info = RoleId.IsNone() ? nullptr : FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId);
	return Info ? Info->GetAlignment() : EKGAlignment::Neutral;
}

FString UKGAppearanceComponent::BodyPath(EKGVillagerBody Body)
{
	const TCHAR* Name = nullptr;
	switch (Body)
	{
	case EKGVillagerBody::MalePeasant: return TEXT("/Game/KillGodot/Characters/Villager/SK_KG_Villager_M.SK_KG_Villager_M");
	case EKGVillagerBody::FemalePeasant: return TEXT("/Game/KillGodot/Characters/Villager/SK_KG_Villager_F.SK_KG_Villager_F");
	case EKGVillagerBody::MalePeasantBald: Name = TEXT("SK_KG_Villager_MB"); break;
	case EKGVillagerBody::FemalePeasantBald: Name = TEXT("SK_KG_Villager_FB"); break;
	case EKGVillagerBody::MaleRanger: Name = TEXT("SK_KG_Villager_MR"); break;
	case EKGVillagerBody::FemaleRanger: Name = TEXT("SK_KG_Villager_FR"); break;
	case EKGVillagerBody::MaleMixed: Name = TEXT("SK_KG_Villager_MX"); break;
	case EKGVillagerBody::FemaleMixed: Name = TEXT("SK_KG_Villager_FX"); break;
	default: Name = TEXT("SK_KG_Villager_MB"); break;
	}
	return FString::Printf(TEXT("%s/Bodies/%s.%s"), KGAppearancePrivate::VarietyRoot, Name, Name);
}

FString UKGAppearanceComponent::HairPath(EKGVillagerHair Hair)
{
	const TCHAR* Name = nullptr;
	switch (Hair)
	{
	case EKGVillagerHair::Buns: Name = TEXT("Hair_Buns"); break;
	case EKGVillagerHair::Buzzed: Name = TEXT("Hair_Buzzed"); break;
	case EKGVillagerHair::BuzzedFemale: Name = TEXT("Hair_BuzzedFemale"); break;
	case EKGVillagerHair::Long: Name = TEXT("Hair_Long"); break;
	case EKGVillagerHair::SimpleParted: Name = TEXT("Hair_SimpleParted"); break;
	default: return FString();
	}
	return FString::Printf(TEXT("%s/SM_KG_%s/%s.%s"), KGAppearancePrivate::VarietyRoot, Name, Name, Name);
}

FString UKGAppearanceComponent::HatPath(EKGVillagerHat Hat)
{
	const TCHAR* Name = nullptr;
	switch (Hat)
	{
	case EKGVillagerHat::Witch: Name = TEXT("SM_KG_Gear_WitchHat"); break;
	case EKGVillagerHat::Straw: Name = TEXT("SM_KG_Gear_StrawHat"); break;
	case EKGVillagerHat::Toque: Name = TEXT("SM_KG_Gear_Toque"); break;
	case EKGVillagerHat::Crown: Name = TEXT("SM_KG_Gear_Crown"); break;
	default: return FString();
	}
	return KGAppearancePrivate::Piece(TEXT("Gear/KG_VillagerGear"), Name);
}

const TCHAR* UKGAppearanceComponent::CapePath() { return TEXT("/Game/KillGodot/Characters/VillagerVariety/Gear/KG_VillagerGear/StaticMeshes/SM_KG_Gear_Cape.SM_KG_Gear_Cape"); }
const TCHAR* UKGAppearanceComponent::ApronPath() { return TEXT("/Game/KillGodot/Characters/VillagerVariety/Gear/KG_VillagerGear/StaticMeshes/SM_KG_Gear_Apron.SM_KG_Gear_Apron"); }
const TCHAR* UKGAppearanceComponent::SashPath() { return TEXT("/Game/KillGodot/Characters/VillagerVariety/Bands/KG_RoleBands/StaticMeshes/SM_KG_RoleSash.SM_KG_RoleSash"); }
const TCHAR* UKGAppearanceComponent::CuffPath() { return TEXT("/Game/KillGodot/Characters/VillagerVariety/Bands/KG_RoleBands/StaticMeshes/SM_KG_RoleCuff.SM_KG_RoleCuff"); }

FVector UKGAppearanceComponent::BuildScale(EKGVillagerBuild Build)
{
	switch (Build)
	{
	case EKGVillagerBuild::Tall: return FVector(1.02f, 1.02f, 1.07f);
	case EKGVillagerBuild::Stocky: return FVector(1.11f, 1.11f, 0.98f);
	case EKGVillagerBuild::Short: return FVector(0.96f, 0.96f, 0.92f);
	case EKGVillagerBuild::Slim: return FVector(0.93f, 0.93f, 1.04f);
	default: return FVector(1.0f);
	}
}

// ---- building the look ---------------------------------------------------------------------------------------

void UKGAppearanceComponent::ClearAttachments()
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& A : Attachments)
	{
		if (UStaticMeshComponent* C = A.Get())
		{
			C->DestroyComponent();
		}
	}
	Attachments.Reset();
}

UStaticMeshComponent* UKGAppearanceComponent::Attach(const TCHAR* Path, FName Bone, const FLinearColor& Tint, bool bMirror, float Glow)
{
	AKGCharacter* Char = Character();
	USkeletalMeshComponent* Body = Char ? Char->GetMesh() : nullptr;
	const USkeletalMesh* Skeletal = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	UStaticMesh* Mesh = KGAppearancePrivate::Load<UStaticMesh>(Path);
	if (!Skeletal || !Mesh)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Appearance: mesh %s missing (run Tools/Unreal/kg_import_villager_variety.py)"), Path);
		return nullptr;
	}
	const FReferenceSkeleton& RefSkeleton = Skeletal->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Appearance: bone %s not on %s"), *Bone.ToString(), *Skeletal->GetName());
		return nullptr;
	}
	// Bind-pose pieces: identity placement in the villager's component space -> bone local (like the cosmetics).
	const FTransform BoneRefCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
	const FTransform Relative = UKGCosmeticsComponent::ComputeBoneRelative(FTransform::Identity, BoneRefCS, bMirror);

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Char, NAME_None, RF_Transient);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetOwnerNoSee(true);
	Component->bCastHiddenShadow = true;
	Component->SetRenderCustomDepth(true);
	Component->SetupAttachment(Body, Bone);
	Component->SetRelativeTransform(Relative);
	Component->RegisterComponent();
	for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = Component->CreateAndSetMaterialInstanceDynamic(i))
		{
			MID->SetVectorParameterValue(TEXT("Tint"), Tint);
			MID->SetScalarParameterValue(TEXT("Glow"), Glow);
		}
	}
	Attachments.Add(Component);
	return Component;
}

void UKGAppearanceComponent::TintSlots(const FKGVillagerLook& Look, const FKGVillagerArchetype& A)
{
	USkeletalMeshComponent* Body = Character()->GetMesh();
	const TArray<FName> Slots = Body->GetMaterialSlotNames();
	int32 OutfitSlot = 0;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		using namespace KGAppearancePrivate;
		const FName& Slot = Slots[i];
		FLinearColor Tint = FLinearColor::White;
		float Desaturation = -0.3f;   // M_KG_Character default: vibrant
		if (SlotIs(Slot, TEXT("Hair")))
		{
			Tint = FKGVillagerLookGen::HairTint(Look.HairColour);   // hair + eyebrows
		}
		else if (SlotIs(Slot, TEXT("Eyes")))
		{
			continue;
		}
		else if (SlotIs(Slot, TEXT("Superhero")) || SlotIs(Slot, TEXT("Regular")))
		{
			Tint = FKGVillagerLookGen::SkinTint(Look.SkinTone);
			Desaturation = A.bOld ? 0.05f : -0.2f;
		}
		else if (SlotIs(Slot, TEXT("Peasant")) || SlotIs(Slot, TEXT("Ranger")))
		{
			// First outfit slot: the archetype's dyed colour; the rest of the garment keeps a quieter shade of it.
			// The base textures are dark (undyed cloth, green leather), so the dye is pushed above 1 to read in the
			// vibrant look (look round 1: everything came out muddy at 1.0).
			const FLinearColor Dye = A.Outfit[Look.OutfitVariant % FKGVillagerLookGen::OutfitVariantCount];
			const float Boost = SlotIs(Slot, TEXT("Ranger")) ? 2.4f : 1.7f;
			Tint = (OutfitSlot++ == 0 ? Dye : FMath::Lerp(Dye, FLinearColor(0.85f, 0.80f, 0.74f), 0.5f)) * Boost;
			Tint.A = 1.0f;
		}
		if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(i))
		{
			MID->SetVectorParameterValue(TEXT("Tint"), Tint);
			MID->SetScalarParameterValue(TEXT("Desaturation"), Desaturation);
		}
	}
}

void UKGAppearanceComponent::ApplyLook(const FKGVillagerLook& Look, const TArray<FName>& EquippedCosmetics)
{
	AKGCharacter* Char = Character();
	if (!Char || GetNetMode() == NM_DedicatedServer || !Look.IsAssigned())
	{
		return;
	}
	const FKGVillagerArchetype& A = FKGVillagerLookGen::ArchetypeOf(Look);
	USkeletalMeshComponent* Body = Char->GetMesh();

	// Which slots the shop loadout owns (it overrides the archetype's own part there).
	bool bHeadTaken = false, bFaceTaken = false, bShouldersTaken = false;
	for (const FName Id : EquippedCosmetics)
	{
		if (const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id))
		{
			bHeadTaken |= Def->Slot == EKGCosmeticSlot::Head;
			bFaceTaken |= Def->Slot == EKGCosmeticSlot::Face;
			bShouldersTaken |= Def->Slot == EKGCosmeticSlot::Shoulders;
		}
	}

	ClearAttachments();
	bool bMeshChanged = false;
	if (USkeletalMesh* Wanted = KGAppearancePrivate::Load<USkeletalMesh>(BodyPath(A.Body)))
	{
		if (Body->GetSkeletalMeshAsset() != Wanted)
		{
			Body->SetSkeletalMeshAsset(Wanted);   // same skeleton: the anim instance and every clip carry on
			bMeshChanged = true;
		}
	}
	else
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Appearance: body %s missing"), *BodyPath(A.Body));
	}
	Body->SetRelativeScale3D(BuildScale(A.Build));
	TintSlots(Look, A);

	const FLinearColor Hair = FKGVillagerLookGen::HairTint(Look.HairColour);
	const bool bFemaleMesh = Body->GetSkeletalMeshAsset() && Body->GetSkeletalMeshAsset()->GetName().Contains(TEXT("_F"));
	if (!HairPath(A.Hair).IsEmpty())
	{
		Attach(*HairPath(A.Hair), TEXT("Head"), Hair);
	}
	if (A.bBeard && !bFaceTaken)
	{
		Attach(TEXT("/Game/KillGodot/Cosmetics/Outfits/SM_KG_Cos_Beard/Hair_Beard.Hair_Beard"), TEXT("Head"), Hair);
	}
	if (A.bHood && !bHeadTaken)
	{
		Attach(bFemaleMesh ? TEXT("/Game/KillGodot/Cosmetics/Outfits/SM_KG_Cos_Hood_F/Female_Ranger_Head_Hood.Female_Ranger_Head_Hood")
		                   : TEXT("/Game/KillGodot/Cosmetics/Outfits/SM_KG_Cos_Hood_M/Male_Ranger_Head_Hood.Male_Ranger_Head_Hood"),
		       TEXT("Head"), A.Accent);
	}
	if (A.Hat != EKGVillagerHat::None && !bHeadTaken)
	{
		Attach(*HatPath(A.Hat), TEXT("Head"), A.Accent);
	}
	if (A.bPauldrons && !bShouldersTaken)
	{
		const TCHAR* Pauldron = bFemaleMesh
			? TEXT("/Game/KillGodot/Cosmetics/Outfits/SM_KG_Cos_Pauldrons_F/Female_Ranger_Acc_Pauldrons.Female_Ranger_Acc_Pauldrons")
			: TEXT("/Game/KillGodot/Cosmetics/Outfits/SM_KG_Cos_Pauldron_M/Male_Ranger_Acc_Pauldron.Male_Ranger_Acc_Pauldron");
		Attach(Pauldron, TEXT("clavicle_l"), A.Accent);
		Attach(Pauldron, TEXT("clavicle_r"), A.Accent, true);
	}
	if (A.bCape)
	{
		Attach(CapePath(), TEXT("spine_03"), A.Accent);
	}
	if (A.bApron)
	{
		Attach(ApronPath(), TEXT("spine_02"), A.Accent);
	}
	Applied = Look;
	AppliedCosmetics = EquippedCosmetics;
	bApplied = true;

	// The sash rides the body: rebuild it on the new mesh. The shop pieces pick M/F meshes from the body name.
	const FName RoleForSash = SashRole;
	SashRole = NAME_None;
	if (UStaticMeshComponent* S = Sash.Get())
	{
		S->DestroyComponent();
		Sash.Reset();
	}
	SetSash(RoleForSash);
	if (bMeshChanged)
	{
		if (UKGCosmeticsComponent* Cosmetics = UKGCosmeticsComponent::FindForPlayer(LastPlayerState.Get()))
		{
			Cosmetics->RefreshVisuals();
		}
	}
}

void UKGAppearanceComponent::SetSash(FName RoleId)
{
	if (RoleId == SashRole && (RoleId.IsNone() || Sash.IsValid()))
	{
		return;
	}
	SashRole = RoleId;
	if (UStaticMeshComponent* S = Sash.Get())
	{
		S->DestroyComponent();
		Sash.Reset();
	}
	if (RoleId.IsNone())
	{
		return;
	}
	if (UStaticMeshComponent* S = Attach(SashPath(), TEXT("spine_02"), AlignmentColour(AlignmentOfRole(RoleId)), false, 0.25f))
	{
		Attachments.RemoveAll([S](const TWeakObjectPtr<UStaticMeshComponent>& P) { return P.Get() == S; });
		S->SetOwnerNoSee(false);   // the revealed player may see their own sash in the death cam
		Sash = S;
	}
}

void UKGAppearanceComponent::SetCuff(EKGAlignment Alignment, bool bShow)
{
	AKGCharacter* Char = Character();
	if (!Char)
	{
		return;
	}
	if (!bShow)
	{
		if (UStaticMeshComponent* C = Cuff.Get())
		{
			C->DestroyComponent();
			Cuff.Reset();
		}
		bCuffShown = false;
		return;
	}
	if (bCuffShown && Cuff.IsValid() && CuffAlignment == Alignment)
	{
		return;
	}
	if (UStaticMeshComponent* C = Cuff.Get())
	{
		C->DestroyComponent();
		Cuff.Reset();
	}
	USkeletalMeshComponent* Arms = Char->GetArmsMesh();
	UStaticMesh* Mesh = KGAppearancePrivate::Load<UStaticMesh>(CuffPath());
	if (!Arms || !Mesh || !Arms->GetSkeletalMeshAsset() ||
	    Arms->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(TEXT("hand_r")) == INDEX_NONE)
	{
		return;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Char, NAME_None, RF_Transient);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	// Exactly like the arms: only the owner renders it, in the first-person pass, no shadow, no ink outline.
	Component->SetOnlyOwnerSee(true);
	Component->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	Component->CastShadow = false;
	Component->SetupAttachment(Arms, TEXT("hand_r"));
	// FPArms2 hand_r: origin = wrist, Y = distal; the ring's axis is Z, so roll it onto the forearm.
	Component->SetRelativeTransform(FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector(0.0f, -1.5f, 0.0f)));
	Component->RegisterComponent();
	for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = Component->CreateAndSetMaterialInstanceDynamic(i))
		{
			MID->SetVectorParameterValue(TEXT("Tint"), AlignmentColour(Alignment));
			MID->SetScalarParameterValue(TEXT("Glow"), 0.6f);
		}
	}
	Cuff = Component;
	CuffAlignment = Alignment;
	bCuffShown = true;
}

void UKGAppearanceComponent::RefreshFromPlayerState()
{
	AKGCharacter* Char = Character();
	if (!Char)
	{
		return;
	}
	// The corpse keeps its look and its sash after the player state unpossesses it.
	if (AKGPlayerState* PS = Char->GetPlayerState<AKGPlayerState>())
	{
		LastPlayerState = PS;
	}
	AKGPlayerState* PS = LastPlayerState.Get();
	if (!PS)
	{
		return;
	}
	if (const UKGCosmeticsComponent* Cosmetics = UKGCosmeticsComponent::FindForPlayer(PS))
	{
		const FKGVillagerLook& Look = Cosmetics->GetLook();
		if (Look.IsAssigned() && (!bApplied || Look != Applied || Cosmetics->GetEquipped() != AppliedCosmetics))
		{
			ApplyLook(Look, Cosmetics->GetEquipped());
		}
	}
	// Public: the server filled RevealedRoleId on purpose (death without a Cleaner, trial, epilogue).
	SetSash(PS->RevealedRoleId);
	// Owner-only: PrivateRoleId only ever arrives on the owning client, so this branch cannot run for anyone else.
	const bool bOwner = Char->IsLocallyControlled();
	const FName Private = bOwner ? PS->GetPrivateRoleId() : NAME_None;
	SetCuff(AlignmentOfRole(Private), bOwner && !Private.IsNone() && !Char->IsDead());
}

// ---- dev verbs: contact sheet lineup and the locked look ----------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace KGAppearanceDev
{
	static UAnimSequence* Clip(const TCHAR* Name)
	{
		return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/KillGodot/Characters/Villager/Anims/%s.%s"), Name, Name));
	}

	/** kg.Look.Lineup [Count=20] [Seed=1] [Pose=idle|sit|dance|wave] [X Y Z Yaw]: dummies in two rows facing -forward. */
	static void Lineup(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Count = FMath::Clamp(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 20, 0, 64);   // 0 = clear
		const uint64 Seed = Args.IsValidIndex(1) ? static_cast<uint64>(FCString::Atoi64(*Args[1])) : 1u;
		const FString Pose = Args.IsValidIndex(2) ? Args[2].ToLower() : TEXT("idle");
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		APawn* Me = PC ? PC->GetPawn() : nullptr;
		FVector Origin = Me ? Me->GetActorLocation() + Me->GetActorForwardVector() * 600.0f : FVector::ZeroVector;
		float Yaw = Me ? Me->GetActorRotation().Yaw + 180.0f : 180.0f;
		if (Args.Num() >= 7)
		{
			Origin = FVector(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[5]));
			Yaw = FCString::Atof(*Args[6]);
		}
		// Clear a previous lineup.
		for (TActorIterator<AKGCharacter> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("KG_Lineup")))
			{
				It->Destroy();
			}
		}
		if (Count == 0)
		{
			UE_LOG(LogKillGodot, Display, TEXT("KG_LOOK_LINEUP cleared"));
			return;
		}
		const FRotator Facing(0.0f, Yaw, 0.0f);
		const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
		const FVector Back = -FRotationMatrix(Facing).GetUnitAxis(EAxis::X);
		const int32 PerRow = FMath::Max(1, (Count + 1) / 2);
		UAnimSequence* Anim = Pose == TEXT("sit") ? Clip(TEXT("A_KG_Sitting_Idle_Loop"))
		                    : Pose == TEXT("dance") ? Clip(TEXT("A_KG_Dance_Loop"))
		                    : Pose == TEXT("wave") ? Clip(TEXT("A_KG_Idle_Talking_Loop"))
		                    : Pose == TEXT("walk") ? Clip(TEXT("A_KG_Jog_Fwd_Loop")) : Clip(TEXT("A_KG_Idle_Loop"));
		TArray<FKGVillagerLook> Taken;
		TSet<uint32> Keys;
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 Row = i / PerRow, Col = i % PerRow;
			const FVector At = Origin + Right * ((Col - (PerRow - 1) * 0.5f) * 110.0f) + Back * (Row * 170.0f) + FVector(0, 0, 100.0f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKGCharacter* Dummy = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), At, Facing, Params);
			if (!Dummy)
			{
				continue;
			}
			Dummy->Tags.Add(TEXT("KG_Lineup"));
			Dummy->SetActorTickEnabled(false);   // no locomotion state machine: the pose clip stays
			Dummy->SetActorEnableCollision(false);
			if (UCharacterMovementComponent* Move = Dummy->GetCharacterMovement())
			{
				Move->DisableMovement();          // floats where placed (no gravity): any backdrop works
				Move->SetComponentTickEnabled(false);
			}
			FKGVillagerLook Look;
			if (Seed == 0)
			{
				// Catalogue mode: archetype First+i with a rolling palette (every archetype on the sheets).
				const int32 First = Args.IsValidIndex(7) ? FCString::Atoi(*Args[7]) : 0;
				const int32 Count2 = FKGVillagerLookGen::Archetypes().Num();
				Look.Archetype = static_cast<uint8>((First + i) % Count2);
				Look.SkinTone = static_cast<uint8>(i % FKGVillagerLookGen::SkinToneCount);
				Look.HairColour = static_cast<uint8>(FKGVillagerLookGen::ArchetypeOf(Look).bOld ? 6 + i % 2 : i % 6);
				Look.OutfitVariant = static_cast<uint8>(i % FKGVillagerLookGen::OutfitVariantCount);
			}
			else
			{
				Look = FKGVillagerLookGen::Generate(Seed, 1000 + i, Taken);
			}
			Taken.Add(Look);
			const bool bUnique = !Keys.Contains(Look.Key());
			Keys.Add(Look.Key());
			if (UKGAppearanceComponent* Appearance = Dummy->GetAppearance())
			{
				Appearance->ApplyLook(Look, {});
				// Two of them wear the public sash so the sheet shows it (Town blue / Impatient red).
				if (i == 6 || i == 13)
				{
					const TArray<FKGRoleInfo>& Roles = FKGRoleListGenerator::GetDefaultCatalog();
					const EKGAlignment Want = i == 6 ? EKGAlignment::Town : EKGAlignment::Impatient;
					const FKGRoleInfo* Role = Roles.FindByPredicate([Want](const FKGRoleInfo& R) { return R.GetAlignment() == Want; });
					if (Role)
					{
						Appearance->SetSash(Role->RoleId);
					}
				}
			}
			if (Anim)
			{
				Dummy->PlayBodyClip(Anim, true, 0.0f);
			}
			++Spawned;
			UE_LOG(LogKillGodot, Display, TEXT("KG_LOOK %d %s key=%08x unique=%d"), i, *Look.ToString(), Look.Key(), bUnique ? 1 : 0);
		}
		UE_LOG(LogKillGodot, Display, TEXT("KG_LOOK_LINEUP spawned=%d unique=%d/%d archetypes=%d pose=%s"), Spawned, Keys.Num(), Count,
		       FKGVillagerLookGen::Archetypes().Num(), *Pose);
	}

	static FAutoConsoleCommandWithWorldAndArgs GLineup(
		TEXT("kg.Look.Lineup"), TEXT("[Count=20] [Seed=1 | 0=catalogue] [Pose=idle|sit|dance|wave|walk] [X Y Z Yaw] [FirstArchetype]: spawn a villager lineup (contact sheet)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Lineup));

	static FAutoConsoleCommandWithWorldAndArgs GLock(
		TEXT("kg.Look.Lock"), TEXT("<Archetype|none>: lock your preferred villager look in the local profile (the server keeps it unique)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName Name = Args.IsValidIndex(0) && !Args[0].Equals(TEXT("none"), ESearchCase::IgnoreCase) ? FName(*Args[0]) : NAME_None;
			if (!Name.IsNone() && !FKGVillagerLookGen::FindArchetype(Name))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("kg.Look.Lock: unknown archetype %s (kg.Look.List)"), *Name.ToString());
				return;
			}
			UKGProfileSave::GetProfile()->SetPreferredLook(Name);
			UE_LOG(LogKillGodot, Display, TEXT("KG_LOOK_LOCK %s"), Name.IsNone() ? TEXT("none") : *Name.ToString());
		}));

	static FAutoConsoleCommand GList(
		TEXT("kg.Look.List"), TEXT("List the villager archetypes."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			for (const FKGVillagerArchetype& A : FKGVillagerLookGen::Archetypes())
			{
				UE_LOG(LogKillGodot, Display, TEXT("KG_LOOK_ARCHETYPE %s %s"), *A.Name.ToString(), A.bFemale ? TEXT("F") : TEXT("M"));
			}
		}));
}
#endif
