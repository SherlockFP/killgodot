#include "Character/KGCharacter.h"
#include "Camera/CameraComponent.h"
#include "Character/KGViewmodelComponent.h"
#include "Character/KGAppearanceComponent.h"
#include "Character/KGBodyAnimInstance.h"
#include "Camera/CameraTypes.h"
#include "Emote/KGEmoteComponent.h"
#include "Chores/KGChoreComponent.h"
#include "Combat/KGHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "KillGodot.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGGameUserSettings.h"
#include "Core/KGPlayerState.h"
#include "Roles/KGRoleListGenerator.h"
#include "World/KGTaskStation.h"
#include "Character/KGCharacterMovement.h"
#include "Components/PostProcessComponent.h"
#include "World/KGLadder.h"
#include "World/KGChoreItem.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "World/KGWaves.h"
#include "GameFramework/PhysicsVolume.h"
#include "Materials/MaterialInterface.h"
#include "Online/KGSnapshotComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Voice/KGMouthComponent.h"
#include "World/KGInteractable.h"
#include "Audio/KGAudio.h"
#include "Fishing/KGFishingComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace KGFP2
{
	// First-person arms v2 (Tools/Blender/kg_make_fp_arms2.py, Tools/Unreal/kg_import_fp_arms2.py).
	const TCHAR* MeshPath = TEXT("/Game/KillGodot/Characters/FPArms2/SK_KG_FPArms2.SK_KG_FPArms2");
	// weapon_r: X = item tip direction, Z = item spine/top edge, origin = handle grip point.
	const FName WeaponBone(TEXT("weapon_r"));

	FString AnimPath(const TCHAR* Clip)
	{
		return FString::Printf(TEXT("/Game/KillGodot/Characters/FPArms2/Anims/A_FP2_%s.A_FP2_%s"), Clip, Clip);
	}

	/** Runtime clip lookup for clips without a UPROPERTY slot (rooted so GC keeps them). */
	UAnimSequence* Anim(const TCHAR* Clip)
	{
		static TMap<FString, TStrongObjectPtr<UAnimSequence>> Cache;
		if (const TStrongObjectPtr<UAnimSequence>* Found = Cache.Find(Clip))
		{
			return Found->Get();
		}
		UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *AnimPath(Clip));
		if (Seq)
		{
			Cache.Add(Clip, TStrongObjectPtr<UAnimSequence>(Seq));
		}
		return Seq;
	}

	/**
	 * Per-item grip on weapon_r, in the item's own mesh space: which mesh axis is the business end (-> socket +X),
	 * which is its top edge/spine (-> socket +Z), where the hand closes (mesh units) and the first-person scale.
	 */
	struct FGripDef
	{
		const TCHAR* Mesh;
		FVector Tip;
		FVector Spine;
		FVector Grip;
		float Scale;
	};
	const FGripDef Grips[] = {
		// Rig author's contract: loc (2.1,0,0), rot P90/Y0/R-90, scale 0.6 (blade is mesh -Y, spine +X).
		{TEXT("SM_KG_Hunters_Knife"), FVector(0, -1, 0), FVector(1, 0, 0), FVector(0.0, 3.5, 0.0), 0.6f},
		// Handle z -14..1, pan disc z 1..22 (face in the XZ plane): hold mid-handle, head forward, face to the view.
		{TEXT("SM_KG_Frying_Pan"), FVector(0, 0, 1), FVector(1, 0, 0), FVector(0.0, 0.4, -8.0), 0.7f},
		// Ferrule tip at z 0, canopy up to z 55, crook handle z 61..73 bending to +X: hold the crook, tip forward.
		{TEXT("SM_KG_Closed_Umbrella"), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0.0, 0.0, 70.0), 0.38f},
		// Fat 124 x 37 cm loaf along Y: shrink to ~19 x 5.5 cm so a fist closes on one end.
		{TEXT("SM_KG_Baguette"), FVector(0, 1, 0), FVector(0, 0, 1), FVector(0.0, -40.0, 13.0), 0.15f},
		// Foot z 0..2, stem z 2..9, drip tray z 9..11, candle up to z 27: grip the stem, candle forward.
		{TEXT("SM_KG_Candlestick"), FVector(0, 0, 1), FVector(1, 0, 0), FVector(0.0, 0.0, 5.5), 1.1f},
		// Imported ~17x too large (7.6 m): body |y| < 254, handles |y| 254..382; hold the middle of one handle.
		{TEXT("SM_KG_Rolling_Pin"), FVector(0, 1, 0), FVector(0, 0, 1), FVector(0.0, -318.0, 0.0), 0.06f},
		// 1.9 m along +X, butt at X=0, cork 3..26 cm, reel hanging to -Z at 31 cm: hold the cork, tip forward, reel down.
		// KEEP IN SYNC with ROD_SCALE / ROD_GRIP in Tools/Blender/kg_make_fp_arms2.py (the rod_* clips).
		{TEXT("SM_KG_FishingRod"), FVector(1, 0, 0), FVector(0, 0, 1), FVector(20.0, 0.0, 0.0), 0.55f},
	};

	FTransform MakeGrip(const FVector& Tip, const FVector& Spine, const FVector& Grip, float Scale)
	{
		// Rotation that takes the item's tip axis to +X and its spine to +Z; then pull the grip onto the origin.
		const FQuat Rot = FRotationMatrix::MakeFromXZ(Tip, Spine).ToQuat().Inverse();
		return FTransform(Rot, -Rot.RotateVector(Grip * Scale), FVector(Scale));
	}

	FTransform GripFor(const UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			return FTransform::Identity;
		}
		const FString Name = Mesh->GetName();
		for (const FGripDef& G : Grips)
		{
			if (Name == G.Mesh)
			{
				return MakeGrip(G.Tip, G.Spine, G.Grip, G.Scale);
			}
		}
		// Unknown item: longest bounds axis is its length, hold it 80% toward the min end, ~30 cm long on screen.
		const FBoxSphereBounds B = Mesh->GetBounds();
		const FVector E = B.BoxExtent;
		const int32 Axis = (E.X >= E.Y && E.X >= E.Z) ? 0 : (E.Y >= E.Z ? 1 : 2);
		FVector Tip = FVector::ZeroVector;
		Tip[Axis] = 1.0;
		const FVector Spine = Axis == 2 ? FVector::ForwardVector : FVector::UpVector;
		const FVector Grip = B.Origin - Tip * (E[Axis] * 0.8);
		const float Scale = FMath::Clamp(30.0f / FMath::Max(2.0f * static_cast<float>(E[Axis]), 1.0f), 0.02f, 1.5f);
		return MakeGrip(Tip, Spine, Grip, Scale);
	}

	// FPArms2's unarmed poses (fists_idle, hands_idle, punches) and carry_idle are authored low, mostly below a 72 deg
	// frame; lift the whole rig (camera-space cm, up) in those states so the fists/open hands read like CS2 fists.
	TAutoConsoleVariable<float> CVarFistLift(TEXT("kg.VM.FistLift"), 7.0f,
		TEXT("FPArms2: cm the arms rig rises while empty-handed (fists, open hands, punches)."));
	TAutoConsoleVariable<float> CVarCarryLift(TEXT("kg.VM.CarryLift"), 4.0f,
		TEXT("FPArms2: cm the arms rig rises while carrying an object."));

	/** Per-pawn viewmodel state kept out of the header so this stays Live Coding friendly. */
	struct FState
	{
		TWeakObjectPtr<UStaticMesh> PlacedMesh;
		float FistsLinger = 0.0f;
		float Lift = 0.0f;
		bool bInspectClip = false;
		/** An emote gesture (A_FP2_emote_*) owns the arms: held item hidden, no fist lift. */
		bool bGesture = false;
	};
	FState& StateOf(const AActor* Owner)
	{
		static TMap<TWeakObjectPtr<const AActor>, FState> States;
		if (States.Num() > 64)
		{
			for (auto It = States.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
		return States.FindOrAdd(Owner);
	}
}


AKGCharacter::AKGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UKGCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f)); // puppet eye height ~1.54 m
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 70.0f;
	FirstPersonCamera->FirstPersonScale = 0.6f;

	ViewmodelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewmodelPivot"));
	ViewmodelPivot->SetupAttachment(FirstPersonCamera);

	// The arms mesh is authored directly in camera space (Tools/Blender/kg_export_characters.py).
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(ViewmodelPivot);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	ArmsMesh->SetCollisionProfileName(FName("NoCollision"));
	ArmsMesh->CastShadow = false;

	// Full puppet body: hidden from its owner but still casting a shadow (VSM is off, see tech doc §6).
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	// Layered body animation (base clip + emote layer, upper-body blend) instead of single-node PlayAnimation.
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->AnimClass = UKGBodyAnimInstance::StaticClass();

	// Quaternius villager (CC0) for both the body and the first-person arms (Docs/06_Art_Direction.md).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> VillagerAsset(
		TEXT("/Game/KillGodot/Characters/Villager/SK_KG_Villager_M.SK_KG_Villager_M"));
	// Dedicated first-person arms v2 (SK_KG_FPArms2, Tools/Blender/kg_make_fp_arms2.py). The old Drillimpact rig
	// (/Game/KillGodot/Characters/FPArms) stays in the project but is no longer referenced here.
	// Non-static on purpose: Live Coding keeps old statics, which pinned a stale failed lookup here.
	ConstructorHelpers::FObjectFinder<USkeletalMesh> FPArmsAsset(KGFP2::MeshPath);
	if (VillagerAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(VillagerAsset.Object);
		// Ink outline post-process (M_KG_PP_Outline) keys off custom depth; the viewmodel stays un-inked.
		GetMesh()->SetRenderCustomDepth(true);
	}
	if (FPArmsAsset.Succeeded())
	{
		ArmsMesh->SetSkeletalMeshAsset(FPArmsAsset.Object);
	}
	// FPArms2 is authored in camera space (rig root = eye, X forward, Z up, posed for a 72 deg viewmodel FOV):
	// identity under the pivot; CS2-style framing comes from the poses plus the viewmodel preset offset.
	ArmsMesh->SetRelativeTransform(FTransform::Identity);

	HeldItem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItem"));
	HeldItem->SetupAttachment(ArmsMesh, KGFP2::WeaponBone);
	HeldItem->SetOnlyOwnerSee(true);
	HeldItem->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	HeldItem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItem->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PanAsset(
		TEXT("/Game/KillGodot/Items/Melee/Frying_Pan_u1SOUzLBQc/StaticMeshes/SM_KG_Frying_Pan.SM_KG_Frying_Pan"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BladeAsset(
		TEXT("/Game/KillGodot/Items/Melee/Hunters_Knife_a2avVUVeYD/StaticMeshes/SM_KG_Hunters_Knife.SM_KG_Hunters_Knife"));
	if (PanAsset.Succeeded())
	{
		ToolMesh = PanAsset.Object;
		HeldItem->SetStaticMesh(ToolMesh);
	}
	if (BladeAsset.Succeeded())
	{
		BladeMesh = BladeAsset.Object;
	}
	// Real grip per item is applied in PlaceHeldItemInHand (KGFP2::GripFor); FPArms2 bones are unit scale.
	HeldItem->SetRelativeTransform(KGFP2::GripFor(ToolMesh));

	auto FindAnim = [](const TCHAR* Name) -> UAnimSequence*
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(
			*FString::Printf(TEXT("/Game/KillGodot/Characters/Villager/Anims/%s.%s"), Name, Name));
		return Finder.Succeeded() ? Finder.Object : nullptr;
	};
	AnimIdle = FindAnim(TEXT("A_KG_Idle_Loop"));
	AnimWalk = FindAnim(TEXT("A_KG_Walk_Loop"));
	AnimSprint = FindAnim(TEXT("A_KG_Sprint_Loop"));
	AnimCrouchIdle = FindAnim(TEXT("A_KG_Crouch_Idle_Loop"));
	AnimCrouchMove = FindAnim(TEXT("A_KG_Crouch_Fwd_Loop"));
	AnimFalling = FindAnim(TEXT("A_KG_Jump_Loop"));
	AnimAttack = FindAnim(TEXT("A_KG_Sword_Attack"));
	AnimDeath = FindAnim(TEXT("A_KG_Death01"));
	auto FindArmsAnim = [](const TCHAR* Clip) -> UAnimSequence*
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*KGFP2::AnimPath(Clip));
		return Finder.Succeeded() ? Finder.Object : nullptr;
	};
	AnimArmsIdle = FindArmsAnim(TEXT("knife_idle"));
	AnimArmsAttack = FindArmsAnim(TEXT("knife_slash_a"));
	AnimArmsAttack2 = FindArmsAnim(TEXT("knife_slash_b"));
	AnimArmsDraw = FindArmsAnim(TEXT("knife_draw"));
	AnimArmsShove = FindArmsAnim(TEXT("punch_l"));
	AnimArmsGrab = FindArmsAnim(TEXT("reach_grab"));
	AnimArmsEmptyIdle = FindArmsAnim(TEXT("fists_idle"));
	AnimArmsPunch = FindArmsAnim(TEXT("punch_r"));
	// knife_stab / knife_inspect / carry_idle / hands_idle have no UPROPERTY slot yet (header change pending):
	// they load through KGFP2::Anim at runtime.

	Viewmodel = CreateDefaultSubobject<UKGViewmodelComponent>(TEXT("Viewmodel"));
	Appearance = CreateDefaultSubobject<UKGAppearanceComponent>(TEXT("Appearance"));   // SPRINT-027a
	Mouth = CreateDefaultSubobject<UKGMouthComponent>(TEXT("Mouth"));
	Health = CreateDefaultSubobject<UKGHealthComponent>(TEXT("Health"));
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
	Emote = CreateDefaultSubobject<UKGEmoteComponent>(TEXT("Emote"));
	Chores = CreateDefaultSubobject<UKGChoreComponent>(TEXT("Chores"));

	// Underwater look: blue-green tint, darker, soft vignette. Enabled only while the camera is below the waves.
	UnderwaterPP = CreateDefaultSubobject<UPostProcessComponent>(TEXT("UnderwaterPP"));
	UnderwaterPP->SetupAttachment(FirstPersonCamera);
	UnderwaterPP->bUnbound = true;
	UnderwaterPP->bEnabled = false;
	UnderwaterPP->Priority = 10.0f;
	UnderwaterPP->Settings.bOverride_SceneColorTint = true;
	UnderwaterPP->Settings.SceneColorTint = FLinearColor(0.32f, 0.72f, 0.82f);
	UnderwaterPP->Settings.bOverride_VignetteIntensity = true;
	UnderwaterPP->Settings.VignetteIntensity = 0.85f;
	UnderwaterPP->Settings.bOverride_AutoExposureBias = true;
	UnderwaterPP->Settings.AutoExposureBias = -0.6f;
	UnderwaterPP->Settings.bOverride_ColorSaturation = true;
	UnderwaterPP->Settings.ColorSaturation = FVector4(0.8f, 0.9f, 1.1f, 1.0f);
	// Murky distance fog + wobble (M_KG_Underwater, Tools/Unreal/kg_make_underwater.py).
	ConstructorHelpers::FObjectFinder<UMaterialInterface> UnderwaterMat(
		TEXT("/Game/KillGodot/Materials/M_KG_Underwater.M_KG_Underwater"));
	if (UnderwaterMat.Succeeded())
	{
		UnderwaterPP->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, UnderwaterMat.Object));
	}
	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxWalkSpeed = WalkSpeed;
	MoveComp->MaxWalkSpeedCrouched = 180.0f;
	MoveComp->NavAgentProps.bCanCrouch = true;
	MoveComp->SetCrouchedHalfHeight(62.0f);
	MoveComp->BrakingDecelerationFalling = 1500.0f;
	MoveComp->AirControl = 0.4f;
}

#if !UE_BUILD_SHIPPING
namespace KGDevActions
{
	// Actions requested from the console, run on the next tick of that world's local player pawn. Needed because
	// editor Python runs under GAllowActorScriptExecutionInEditor, which makes every RPC execute locally.
	static TArray<TPair<TWeakObjectPtr<UWorld>, FString>> Pending;
	static FAutoConsoleCommandWithWorldAndArgs ActCommand(
		TEXT("kg.Act"), TEXT("Queue a player action on this world's local pawn: attack | shove | interact | release | blade | inspect | accuse | guilty | innocent"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World && Args.Num() > 0)
			{
				Pending.Emplace(World, Args[0].ToLower());
			}
		}));
}
#endif

void AKGCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health->OnDeath.AddDynamic(this, &AKGCharacter::HandleDeath);

	// Live Coding does not always rebuild the CDO: make sure a pawn spawned from a stale default still gets the
	// FPArms2 rig, its camera-space (identity) transform and the weapon_r attachment.
	if (USkeletalMesh* Arms2 = LoadObject<USkeletalMesh>(nullptr, KGFP2::MeshPath);
	    Arms2 && ArmsMesh->GetSkeletalMeshAsset() != Arms2)
	{
		ArmsMesh->SetSkeletalMeshAsset(Arms2);
		auto Reload = [](TObjectPtr<UAnimSequence>& Slot, const TCHAR* Clip) { Slot = KGFP2::Anim(Clip); };
		Reload(AnimArmsIdle, TEXT("knife_idle"));
		Reload(AnimArmsAttack, TEXT("knife_slash_a"));
		Reload(AnimArmsAttack2, TEXT("knife_slash_b"));
		Reload(AnimArmsDraw, TEXT("knife_draw"));
		Reload(AnimArmsShove, TEXT("punch_l"));
		Reload(AnimArmsGrab, TEXT("reach_grab"));
		Reload(AnimArmsEmptyIdle, TEXT("fists_idle"));
		Reload(AnimArmsPunch, TEXT("punch_r"));
	}
	ArmsMesh->SetRelativeTransform(FTransform::Identity);
	HeldItem->SetUsingAbsoluteScale(false);

	if (UAnimSequence* Idle = CurrentArmsIdle())
	{
		ArmsMesh->PlayAnimation(Idle, true);
	}
	// The sea's physics volume is managed by TickWaterAndLadders (see there).
	GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);
	// Villagers start empty-handed; the blade (Impatient) appears only when drawn.
	HeldItem->SetVisibility(false);
	PlayBodyAnim(AnimIdle, true);
}

void AKGCharacter::PlaceHeldItemInHand()
{
	// FPArms2 carries a dedicated weapon_r bone (X = tip, Z = spine, origin = grip); every item rides it with a
	// per-item grip (KGFP2::Grips), so the clips (draw, slash, stab, inspect) move the item with the fingers.
	RightHandBone = KGFP2::WeaponBone;
	UStaticMesh* ItemMesh = HeldItem ? HeldItem->GetStaticMesh() : nullptr;
	KGFP2::StateOf(this).PlacedMesh = ItemMesh;
	if (!ItemMesh || ArmsMesh->GetBoneIndex(KGFP2::WeaponBone) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("KG_DIAG HeldItem placement skipped on %s (arms %s, %d bones, item %s)"),
			*GetName(), *GetNameSafe(ArmsMesh->GetSkeletalMeshAsset()), ArmsMesh->GetNumBones(), *GetNameSafe(ItemMesh));
		return;
	}
	HeldItem->SetUsingAbsoluteScale(false);
	HeldItem->AttachToComponent(ArmsMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, KGFP2::WeaponBone);
	const FTransform Grip = KGFP2::GripFor(ItemMesh);
	HeldItem->SetRelativeTransform(Grip);
	HeldItemBaseRel = Grip.GetRotation();
	UE_LOG(LogTemp, Display, TEXT("KG_DIAG HeldItem %s on %s grip loc=%s rot=%s scale=%.3f"), *ItemMesh->GetName(),
		*KGFP2::WeaponBone.ToString(), *Grip.GetLocation().ToCompactString(), *Grip.Rotator().ToCompactString(),
		Grip.GetScale3D().X);
}

void AKGCharacter::PlayArmsOneShot(UAnimSequence* Anim)
{
	if (Anim)
	{
		KGFP2::StateOf(this).bGesture = false;
		ArmsMesh->PlayAnimation(Anim, false);
		ArmsOneShotRemaining = Anim->GetPlayLength();
	}
}

void AKGCharacter::PlayBodyAnim(UAnimSequence* Anim, bool bLoop)
{
	const UKGBodyAnimInstance* Body = GetBodyAnim();
	// Also re-plays when the mesh re-created its anim instance (outfit / body swap).
	if (Anim && (Anim != CurrentBodyAnim || (Body && !Body->GetBase())))
	{
		CurrentBodyAnim = Anim;
		PlayBodyClip(Anim, bLoop);
	}
}

UKGBodyAnimInstance* AKGCharacter::GetBodyAnim() const
{
	return GetMesh() ? Cast<UKGBodyAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
}

void AKGCharacter::PlayBodyClip(UAnimSequence* Anim, bool bLoop, float BlendTime)
{
	if (!Anim)
	{
		return;
	}
	if (UKGBodyAnimInstance* Body = GetBodyAnim())
	{
		Body->PlayBase(Anim, bLoop, BlendTime);
	}
	else
	{
		GetMesh()->PlayAnimation(Anim, bLoop);   // something forced single-node mode: still show the clip
	}
}

UAnimSequence* AKGCharacter::GetBodyClip() const
{
	if (const UKGBodyAnimInstance* Body = GetBodyAnim())
	{
		return Body->GetBase();
	}
	const UAnimSingleNodeInstance* Single = GetMesh() ? GetMesh()->GetSingleNodeInstance() : nullptr;
	return Single ? Cast<UAnimSequence>(Single->GetAnimationAsset()) : nullptr;
}

void AKGCharacter::PlayArmsGesture(UAnimSequence* Clip)
{
	if (!Clip || !IsLocallyControlled())
	{
		return;
	}
	KGFP2::StateOf(this).bInspectClip = false;
	PlayArmsOneShot(Clip);
	KGFP2::StateOf(this).bGesture = true;
}

void AKGCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (Emote)
	{
		Emote->ApplyCamera(DeltaTime, OutResult);
	}
	if (UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(this))
	{
		Fishing->ApplyCamera(DeltaTime, OutResult);   // narrower view while reeling, bite nudge
	}
	// SPRINT-026: optional CS-style FOV widen at high ground speed (sprint through bhop range).
	if (IsLocallyControlled() && UKGGameUserSettings::Get()->GetFOVKickOnSpeed())
	{
		const float Speed = GetVelocity().Size2D();
		const float Kick = FMath::GetMappedRangeValueClamped(FVector2D(SprintSpeed, SprintSpeed * 1.35f),
		                                                     FVector2D(0.0f, 6.0f), Speed);
		OutResult.FOV += Kick;
	}
}

void AKGCharacter::UpdateBodyAnimation(float DeltaSeconds)
{
	if (ArmsOneShotRemaining > 0.0f)
	{
		// When it runs out, Tick's arms state machine blends back into the right loop (knife/fists/carry/hands).
		ArmsOneShotRemaining -= DeltaSeconds;
	}
	if (BodyOneShotRemaining > 0.0f)
	{
		BodyOneShotRemaining -= DeltaSeconds;
		return;
	}
	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Wanted = AnimIdle;
	if (GetCharacterMovement()->IsFalling())
	{
		Wanted = AnimFalling;
	}
	else if (bIsCrouched)
	{
		Wanted = Speed > 20.0f ? AnimCrouchMove : AnimCrouchIdle;
	}
	else if (Speed > 400.0f)
	{
		Wanted = AnimSprint;
	}
	else if (Speed > 20.0f)
	{
		Wanted = AnimWalk;
	}
	PlayBodyAnim(Wanted, true);
}

bool AKGCharacter::IsDead() const
{
	return Health && Health->IsDead();
}

void AKGCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (!DefaultMappingContext)
	{
		CreateDefaultInput();
	}
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	Viewmodel->Initialize(ViewmodelPivot, FirstPersonCamera);
}

void AKGCharacter::CreateDefaultInput()
{
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	AttackAction = MakeAction(TEXT("IA_Attack"), EInputActionValueType::Boolean);
	ShoveAction = MakeAction(TEXT("IA_Shove"), EInputActionValueType::Boolean);
	InspectAction = MakeAction(TEXT("IA_Inspect"), EInputActionValueType::Boolean);
	DevBladeAction = MakeAction(TEXT("IA_DevBlade"), EInputActionValueType::Boolean);
	AccuseAction = MakeAction(TEXT("IA_Accuse"), EInputActionValueType::Boolean);
	RotateHeldAction = MakeAction(TEXT("IA_RotateHeld"), EInputActionValueType::Boolean);
	GuiltyAction = MakeAction(TEXT("IA_Guilty"), EInputActionValueType::Boolean);
	InnocentAction = MakeAction(TEXT("IA_Innocent"), EInputActionValueType::Boolean);

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KG_Default"));
	auto Map = [this](UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegateX = false,
	                  bool bNegateY = false)
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(Action, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(DefaultMappingContext));
		}
		if (bNegateX || bNegateY)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(DefaultMappingContext);
			Negate->bX = bNegateX;
			Negate->bY = bNegateY;
			Negate->bZ = false;
			Mapping.Modifiers.Add(Negate);
		}
	};
	// Move: W/S drive Y (swizzled), A/D drive X.
	Map(MoveAction, EKeys::W, true);
	Map(MoveAction, EKeys::S, true, true);
	Map(MoveAction, EKeys::A, false, true);
	Map(MoveAction, EKeys::D);
	Map(LookAction, EKeys::Mouse2D, false, false, true);
	Map(JumpAction, EKeys::SpaceBar);
	Map(SprintAction, EKeys::LeftShift);
	Map(CrouchAction, EKeys::LeftControl);
	Map(CrouchAction, EKeys::C);
	Map(InteractAction, EKeys::E);
	Map(AttackAction, EKeys::LeftMouseButton);
	Map(ShoveAction, EKeys::RightMouseButton);
	Map(InspectAction, EKeys::F);
	Map(DevBladeAction, EKeys::B);
	Map(AccuseAction, EKeys::V);
	Map(RotateHeldAction, EKeys::R);
	Map(GuiltyAction, EKeys::Y);
	Map(InnocentAction, EKeys::N);
}

void AKGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	if (!DefaultMappingContext)
	{
		CreateDefaultInput();
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AKGCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AKGCharacter::Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AKGCharacter::JumpStart);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AKGCharacter::JumpEnd);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AKGCharacter::StartSprint);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AKGCharacter::StopSprint);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AKGCharacter::ToggleCrouch);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AKGCharacter::Interact);
	Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &AKGCharacter::InteractReleased);
	Input->BindAction(RotateHeldAction, ETriggerEvent::Started, this, &AKGCharacter::RotateHeld);
	Input->BindAction(AttackAction, ETriggerEvent::Started, this, &AKGCharacter::Attack);
	Input->BindAction(ShoveAction, ETriggerEvent::Started, this, &AKGCharacter::Shove);
	Input->BindAction(InspectAction, ETriggerEvent::Started, this, &AKGCharacter::Inspect);
	Input->BindAction(DevBladeAction, ETriggerEvent::Started, this, &AKGCharacter::ToggleDevBlade);
	Input->BindAction(AccuseAction, ETriggerEvent::Started, this, &AKGCharacter::Accuse);
	Input->BindAction(GuiltyAction, ETriggerEvent::Started, this, &AKGCharacter::VoteGuilty);
	Input->BindAction(InnocentAction, ETriggerEvent::Started, this, &AKGCharacter::VoteInnocent);
}

void AKGCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Ladder.IsValid() && GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		// On a ladder W/S climb, A/D shuffle sideways a little.
		AddMovementInput(FVector::UpVector, Axis.Y);
		AddMovementInput(GetActorRightVector(), Axis.X * 0.3f);
		return;
	}
	if (GetCharacterMovement()->IsSwimming())
	{
		// Swim where you look (dive by looking down).
		AddMovementInput(GetControlRotation().Vector(), Axis.Y);
		AddMovementInput(GetActorRightVector(), Axis.X);
		return;
	}
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AKGCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
	Viewmodel->AddLookInput(Axis.X, Axis.Y);
}

void AKGCharacter::JumpStart()
{
	if (Ladder.IsValid() && GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		// Let go of the ladder with a small push away from it.
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		LaunchCharacter(-Ladder->GetActorForwardVector() * 250.0f + FVector(0, 0, 120.0f), true, true);
		Ladder = nullptr;
		return;
	}
	if (GetCharacterMovement()->IsSwimming())
	{
		bSwimUp = true;
		return;
	}
	Jump();
}

void AKGCharacter::JumpEnd()
{
	bSwimUp = false;
	StopJumping();
}

void AKGCharacter::OnJumped_Implementation()
{
	// Fires once per successful grounded DoJump (both the predicting client and the server run this identically -
	// same input, same deterministic gate in UKGCharacterMovement::CanAttemptJump/ACharacter::CanJump). The landing-
	// buffer chain hop does NOT go through here (it bypasses CheckJumpInput entirely - see OnMovementModeChanged in
	// KGCharacterMovement.cpp); that one is charged and felt from the Tick() HopCounter poll above instead.
	Super::OnJumped_Implementation();
	if (IsDead())
	{
		return;
	}
	Stamina.TrySpend(JumpStaminaCost);
	if (IsLocallyControlled())
	{
		Viewmodel->AddRecoil(FVector(4.0, 0.0, 6.0), FVector(-4.0, 0.0, 0.0));   // small upward dip on takeoff
	}
}

void AKGCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (IsDead())
	{
		return;
	}
	// The landing-buffer chain hop (if any) has already re-launched us by the time Landed() runs (it happens inside
	// SetPostLandedPhysics -> OnMovementModeChanged, before ACharacter::ProcessLanded calls Landed()); if we are
	// still falling, the streak continues, otherwise this landing ended the chain.
	const UKGCharacterMovement* KGMove = Cast<UKGCharacterMovement>(GetCharacterMovement());
	if (!KGMove || !KGMove->IsFalling())
	{
		ChainHopStreak = 0;
	}
	if (!IsLocallyControlled())
	{
		return;
	}
	Viewmodel->AddRecoil(FVector(-6.0, 0.0, -10.0), FVector(6.0, 0.0, 0.0));   // landing dip, heavier than a footstep
	const UPrimitiveComponent* Floor = Hit.GetComponent();
	const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Floor);
	const FString MeshName = SM && SM->GetStaticMesh() ? SM->GetStaticMesh()->GetName() : FString();
	const TCHAR* Kind = MeshName.Contains(TEXT("Wood")) || MeshName.Contains(TEXT("Stair")) ? TEXT("Wood")
	                  : MeshName.Contains(TEXT("Brick")) || MeshName.Contains(TEXT("Platform")) ? TEXT("Stone")
	                                                                                             : TEXT("Grass");
	// Reuses the footstep set (variant 0) at a heavier volume rather than a new, not-yet-authored landing cue.
	KGAudio::At(this, *FString::Printf(TEXT("S_Step_%s_0"), Kind), GetActorLocation() - FVector(0, 0, 80.0f), 1.0f);
}

void AKGCharacter::StartSprint()
{
	bWantsSprint = true;
	if (!HasAuthority())
	{
		ServerSetSprint(true);
	}
}

void AKGCharacter::StopSprint()
{
	bWantsSprint = false;
	if (!HasAuthority())
	{
		ServerSetSprint(false);
	}
}

void AKGCharacter::ServerSetSprint_Implementation(bool bSprint)
{
	bWantsSprint = bSprint;
}

void AKGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGCharacter, bHoldingAssassinBlade);
	DOREPLIFETIME_CONDITION(AKGCharacter, bHoldingObject, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AKGCharacter, ActiveTask, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AKGCharacter, TaskProgress, COND_OwnerOnly);
}

FVector AKGCharacter::ValidatedViewStart(const FVector& Claimed) const
{
	// Generous for crouch/latency, tight enough to stop hitting from across the room.
	const FVector Head = FirstPersonCamera->GetComponentLocation();
	return FVector::DistSquared(Claimed, Head) <= FMath::Square(120.0) ? Claimed : Head;
}

void AKGCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AKGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDead())
	{
		return;
	}

	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	if (HasAuthority())
	{
		TickTask(DeltaSeconds);
	}
	TickWaterAndLadders(DeltaSeconds);
	UpdateRevealGlow();
	ServerAttackCooldown = FMath::Max(0.0f, ServerAttackCooldown - DeltaSeconds);
	UpdateBodyAnimation(DeltaSeconds);
	const bool bMoving = GetVelocity().SizeSquared2D() > 100.0f;
	const bool bSprinting = Stamina.Tick(DeltaSeconds, bWantsSprint && !bIsCrouched, bMoving);
	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
	// SPRINT-016 hook (Chores/WorldChores): a heavy chore item (fish crate, sacks, firewood) slows its carrier and
	// forbids sprinting; a second carrier on the crate restores full walking speed.
	const float CarryFactor = AKGChoreItem::SpeedFactorFor(this);
	if (CarryFactor < 0.999f)
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CarryFactor;
	}

	// SPRINT-026: feed context into the CMC's air-strafe / skill bhop (Docs/01_GDD_Core.md movement section).
	// Bots never press Jump, so UKGCharacterMovement::HopCounter never advances for them regardless of these
	// settings - this block only ever matters for a human-controlled pawn.
	if (UKGCharacterMovement* KGMove = Cast<UKGCharacterMovement>(GetCharacterMovement()))
	{
		const bool bChoreActive = ActiveTask != nullptr;
		const UKGFishingComponent* FishingCtx = UKGFishingComponent::FindFor(this);
		const bool bFishingOut = FishingCtx && FishingCtx->WantsRodInHand();
		const bool bCarrying = CarryFactor < 0.999f || bHoldingObject;
		KGMove->SprintSpeedForCap = SprintSpeed;
		// Carrying, rod out or a chore in progress: no bhop advantage at all (hopping still moves you, it just
		// never beats sprint-speed-capped air control). The blade out or a tired sprinter still gets a hop, just
		// capped at plain sprint speed instead of the full 1.35x soft cap.
		KGMove->bAirStrafeDisabled = bChoreActive || bFishingOut || bCarrying;
		KGMove->HopGainScale = (bHoldingAssassinBlade || Stamina.bExhausted) ? 0.0f : 1.0f;
		// Stamina exhaustion stops CHAINING (the landing-buffer re-hop); a plain grounded jump is never blocked.
		KGMove->bHopChainBlocked = Stamina.bExhausted;

		if (KGMove->HopCounter != LastSeenHopCounter)
		{
			LastSeenHopCounter = KGMove->HopCounter;
			++ChainHopStreak;
			const float Cost = ChainHopBaseCost + ChainHopStepCost * FMath::Min(ChainHopStreak, ChainHopStreakCostCap);
			Stamina.TrySpend(Cost);   // accounting only: the hop already happened in the CMC this move.
			if (IsLocallyControlled())
			{
				Viewmodel->AddRecoil(FVector(5.0, 0.0, 8.0), FVector(-5.0, 0.0, 0.0));   // chained hop: a touch stronger than a plain jump
			}
		}
	}

	if (HeldComponent)
	{
		const FVector Target = FirstPersonCamera->GetComponentLocation() +
		                       GetControlRotation().Vector() * HoldDistance;
		if (!IsValid(HeldComponent) ||
		    FVector::DistSquared(HeldComponent->GetComponentLocation(), Target) > FMath::Square(HoldDistance * 2.0f))
		{
			Release(false);
		}
		else
		{
			const FRotator Held(0.0f, GetControlRotation().Yaw + HeldYaw, 0.0f);
			PhysicsHandle->SetTargetLocationAndRotation(Target, Held);
		}
	}

	KGFP2::FState& FP2 = KGFP2::StateOf(this);
	if (IsLocallyControlled() && IsPlayerControlled() && HeldItemPlacementFrames > 0 && --HeldItemPlacementFrames == 0)
	{
		PlaceHeldItemInHand();
	}
	// Whatever swapped the held mesh (blade skin, tool, automation), re-seat it with that item's grip.
	if (IsLocallyControlled() && HeldItem->GetStaticMesh() && FP2.PlacedMesh.Get() != HeldItem->GetStaticMesh())
	{
		PlaceHeldItemInHand();
	}
	if (IsLocallyControlled() && bHoldingAssassinBlade != bShowingBlade)
	{
		bShowingBlade = bHoldingAssassinBlade;
		UStaticMesh* Wanted = bShowingBlade && BladeMesh ? BladeMesh.Get() : ToolMesh.Get();
		if (Wanted && HeldItem->GetStaticMesh() != Wanted)
		{
			HeldItem->SetStaticMesh(Wanted);
			PlaceHeldItemInHand();
		}
		HeldItem->SetVisibility(bShowingBlade);
		FP2.bInspectClip = false;
		AttackAlternate = 0;   // first swing after a draw/sheathe is always the right hand / slash A
		ArmsOneShotRemaining = 0.0f;
		if (bShowingBlade)
		{
			PlayArmsOneShot(AnimArmsDraw);
		}
	}
	// Fishing rod (Fishing/KGFishingComponent): owns weapon_r while it is out and the blade is not.
	UKGFishingComponent* Fishing = IsLocallyControlled() ? UKGFishingComponent::FindFor(this) : nullptr;
	const bool bRodInHand = Fishing && Fishing->WantsRodInHand() && !bShowingBlade && !bHoldingObject && !IsDead();
	if (Fishing)
	{
		UStaticMesh* RodMesh = Fishing->GetRodMesh();
		if (bRodInHand && RodMesh && HeldItem->GetStaticMesh() != RodMesh)
		{
			HeldItem->SetStaticMesh(RodMesh);
			PlaceHeldItemInHand();
			ArmsOneShotRemaining = 0.0f;
		}
		else if (!bRodInHand && RodMesh && HeldItem->GetStaticMesh() == RodMesh)
		{
			HeldItem->SetStaticMesh(bShowingBlade && BladeMesh ? BladeMesh.Get() : ToolMesh.Get());
			PlaceHeldItemInHand();
		}
		if (bRodInHand)
		{
			if (UAnimSequence* Shot = Fishing->ConsumeArmsOneShot())
			{
				PlayArmsOneShot(Shot);   // cast / hook-set
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		for (int32 i = KGDevActions::Pending.Num() - 1; i >= 0; --i)
		{
			if (KGDevActions::Pending[i].Key.Get() != GetWorld())
			{
				continue;
			}
			const FString Action = KGDevActions::Pending[i].Value;
			KGDevActions::Pending.RemoveAt(i);
			if (Action == TEXT("attack")) { Attack(); }
			else if (Action == TEXT("shove")) { Shove(); }
			else if (Action == TEXT("interact")) { Interact(); }
			else if (Action == TEXT("blade")) { ToggleDevBlade(); }
			else if (Action == TEXT("inspect")) { Inspect(); }
			else if (Action == TEXT("release")) { InteractReleased(); }
			else if (Action == TEXT("accuse")) { Accuse(); }
			else if (Action == TEXT("guilty")) { VoteGuilty(); }
			else if (Action == TEXT("innocent")) { VoteInnocent(); }
			UE_LOG(LogKillGodot, Log, TEXT("kg.Act %s on %s"), *Action, *GetName());
		}
	}
#endif
	// Clean view when empty-handed (CS without a weapon): the arms only show for an action (punch, grab, draw)
	// and while carrying the blade or an object.
	if (IsLocallyControlled() && !IsDead())
	{
		// The fists linger counts from the end of the jab, not from the click.
		if (ArmsOneShotRemaining <= 0.0f)
		{
			FP2.FistsLinger = FMath::Max(0.0f, FP2.FistsLinger - DeltaSeconds);
		}
		if (FP2.bInspectClip && (ArmsOneShotRemaining <= 0.0f || !bShowingBlade || Viewmodel->IsBackstabReady()))
		{
			// A back in reach cancels the flourish (CS2 cancels inspect on combat) instead of stacking the procedural
			// stab-ready raise on top of the clip.
			if (ArmsOneShotRemaining > 0.0f)
			{
				ArmsOneShotRemaining = 0.0f;
			}
			FP2.bInspectClip = false;
		}
		const bool bEmptyInspect = !bShowingBlade && !bHoldingObject && Viewmodel->IsInspecting();
		if (FP2.bGesture && ArmsOneShotRemaining <= 0.0f)
		{
			FP2.bGesture = false;
		}
		// Third-person emote camera: no viewmodel at all (the body is what you see).
		const bool bEmoteView = Emote->WantsViewmodelHidden();
		// Fists stay up for a beat after a punch so the jab does not pop in and out of an empty view.
		const bool bShowArms = !bEmoteView && (bShowingBlade || bHoldingObject || ArmsOneShotRemaining > 0.05f ||
		                                       ActiveTask != nullptr || bEmptyInspect || FP2.FistsLinger > 0.0f || bRodInHand);
		const bool bShowItem = (bShowingBlade || bRodInHand) && !bEmoteView && !FP2.bGesture;
		if (HeldItem->IsVisible() != bShowItem)
		{
			HeldItem->SetVisibility(bShowItem);
		}
		// Arms loop state machine (FPArms2): one-shots own the rig while they run, then the right loop resumes.
		// The knife inspect is a keyframed clip (knife_inspect) that moves weapon_r itself: no procedural twirl.
		UAnimSequence* Loop = bHoldingObject                         ? KGFP2::Anim(TEXT("carry_idle"))
		                    : bShowingBlade                          ? AnimArmsIdle.Get()
		                    : bRodInHand                             ? Fishing->GetArmsLoop()
		                    : (bEmptyInspect || ActiveTask != nullptr) ? KGFP2::Anim(TEXT("hands_idle"))
		                                                             : AnimArmsEmptyIdle.Get();
		const UAnimSingleNodeInstance* Single = ArmsMesh->GetSingleNodeInstance();
		const UAnimationAsset* Playing = Single ? Single->GetAnimationAsset() : nullptr;
		if (Loop && ArmsOneShotRemaining <= 0.0f && Playing != Loop)
		{
			ArmsMesh->PlayAnimation(Loop, true);
		}
		// Knife clips are framed as authored (identity); unarmed/carry poses get lifted into view, blended so a
		// draw or sheathe does not pop.
		const float LiftGoal = bHoldingObject                  ? KGFP2::CVarCarryLift.GetValueOnGameThread()
		                     : (bShowingBlade || FP2.bGesture || bRodInHand) ? 0.0f
		                                                       : KGFP2::CVarFistLift.GetValueOnGameThread();
		FP2.Lift = bShowArms ? FMath::FInterpTo(FP2.Lift, LiftGoal, DeltaSeconds, 12.0f) : LiftGoal;
		ArmsMesh->SetRelativeLocation(FVector(0.0, 0.0, FP2.Lift));
		if (ArmsMesh->IsVisible() != bShowArms)
		{
			ArmsMesh->SetVisibility(bShowArms);
		}
	}
	if (IsLocallyControlled())
	{
		Viewmodel->SetBackstabReady(bHoldingAssassinBlade && !bHoldingObject && FindBackstabTarget() != nullptr);
	}
	TickMoveSmoke(DeltaSeconds);
}

bool AKGCharacter::TraceView(float Distance, FHitResult& OutHit) const
{
	return TraceFrom(FirstPersonCamera->GetComponentLocation(), GetControlRotation().Vector(), Distance, OutHit);
}

bool AKGCharacter::TraceFrom(const FVector& Start, const FVector& Dir, float Distance, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGTraceView), false, this);
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Dir * Distance, ECC_Visibility, Params);
}

namespace KGInteractTrace
{
	// A chore item (the cranked bucket, the fish crate) lying behind a chore spot's or panel station's E box wins: the
	// item is what the player came for, and those boxes are invisible. Only those boxes are looked through, never walls.
	static void PreferChoreItem(const AKGCharacter* Self, const FVector& Start, const FVector& Dir, float Distance, FHitResult& Hit)
	{
		if (!Hit.GetActor() || !(Hit.GetActor()->IsA<AKGTaskStation>() || Hit.GetActor()->IsA<AKGChoreSpot>()))
		{
			return;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGTraceViewItem), false, Self);
		Params.AddIgnoredActor(Hit.GetActor());
		FHitResult Behind;
		if (Self->GetWorld()->LineTraceSingleByChannel(Behind, Start, Start + Dir * Distance, ECC_Visibility, Params) &&
		    Cast<AKGChoreItem>(Behind.GetActor()))
		{
			Hit = Behind;
		}
	}
}

void AKGCharacter::Interact()
{
	if (IsDead())
	{
		return;
	}
	Emote->NotifyLocalAction();
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector Dir = GetControlRotation().Vector();
	if (!bHoldingObject)
	{
		// Predict the reach locally: doors/stations and anything the server would let us pick up (hold-E carry).
		FHitResult Hit;
		if (TraceFrom(Start, Dir, GrabDistance, Hit))
		{
			KGInteractTrace::PreferChoreItem(this, Start, Dir, GrabDistance, Hit);
			const UPrimitiveComponent* Comp = Hit.GetComponent();
			const bool bInteractable = Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UKGInteractable::StaticClass());
			const bool bCarryable = Comp && Comp->IsSimulatingPhysics() && Comp->GetMass() <= MaxGrabMassKg;
			if (bInteractable || bCarryable)
			{
				KGFP2::StateOf(this).bInspectClip = false;
				PlayArmsOneShot(AnimArmsGrab);
			}
		}
	}
	ServerInteract(Start, Dir);
}

void AKGCharacter::ServerInteract_Implementation(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir)
{
	if (IsDead())
	{
		return;
	}
	if (HeldComponent)
	{
		return;   // already carrying: releasing E drops it
	}
	Emote->ServerStop(EKGEmoteStop::Requested);   // using something (door, seat, chore) ends an emote
	FHitResult Hit;
	if (!TraceFrom(ValidatedViewStart(ViewStart), ViewDir.GetSafeNormal(), GrabDistance, Hit))
	{
		return;
	}
	KGInteractTrace::PreferChoreItem(this, ValidatedViewStart(ViewStart), ViewDir.GetSafeNormal(), GrabDistance, Hit);
	AActor* HitActor = Hit.GetActor();
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UKGInteractable::StaticClass()))
	{
		IKGInteractable::Execute_Interact(HitActor, this);
		return;
	}
	if (UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		TryGrab(Comp, Hit.ImpactPoint);
	}
}

bool AKGCharacter::TryGrab(UPrimitiveComponent* Component, const FVector& GrabPoint)
{
	if (!Component || !Component->IsSimulatingPhysics() || Component->GetMass() > MaxGrabMassKg)
	{
		return false;
	}
	// Soft, heavy-feeling handle (HL2 use/GMod carry): objects lag a little and swing on collisions.
	PhysicsHandle->SetInterpolationSpeed(12.0f);
	PhysicsHandle->bSoftAngularConstraint = true;
	PhysicsHandle->bSoftLinearConstraint = true;
	HeldYaw = Component->GetComponentRotation().Yaw - GetControlRotation().Yaw;
	// SPRINT-016 hook: chore items know who carries them (speed, spills, credit). A light one is snatched out of the
	// previous carrier's hands; the heavy crate takes a second pair of hands.
	if (AKGChoreItem* ChoreItem = Cast<AKGChoreItem>(Component->GetOwner()))
	{
		if (AKGCharacter* Robbed = Cast<AKGCharacter>(ChoreItem->AuthAddCarrier(this)); Robbed && Robbed != this)
		{
			Robbed->Release(false);
		}
	}
	PhysicsHandle->GrabComponentAtLocationWithRotation(Component, NAME_None, Component->Bounds.Origin,
	                                                   Component->GetComponentRotation());
	HeldComponent = Component;
	bHoldingObject = true;
	return true;
}

void AKGCharacter::Release(bool bThrow)
{
	UPrimitiveComponent* Held = HeldComponent;
	PhysicsHandle->ReleaseComponent();
	HeldComponent = nullptr;
	bHoldingObject = false;
	if (AKGChoreItem* ChoreItem = IsValid(Held) ? Cast<AKGChoreItem>(Held->GetOwner()) : nullptr)
	{
		ChoreItem->AuthRemoveCarrier(this);   // SPRINT-016 hook
	}
	if (bThrow && IsValid(Held))
	{
		Held->AddImpulse(GetControlRotation().Vector() * ThrowSpeed, NAME_None, true);
	}
}

void AKGCharacter::Attack()
{
	if (IsDead())
	{
		return;
	}
	Emote->NotifyLocalAction();
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector Forward = GetControlRotation().Vector();
	if (bHoldingObject)
	{
		Viewmodel->AddRecoil(FVector(-40.0, 0.0, 20.0), FVector(-120.0, 0.0, 0.0)); // throw
		ServerAttack(Start, Forward);
		return;
	}
	if (AttackCooldownRemaining > 0.0f)
	{
		return;
	}
	AttackCooldownRemaining = MeleeCooldown;
	// Local prediction of the feel only; the server decides what actually got hit.
	PlaySwingCosmetics(bHoldingAssassinBlade && FindBackstabTarget(Forward) != nullptr);
	ServerAttack(Start, Forward);
}

void AKGCharacter::PlaySwingCosmetics(bool bBackstab)
{
	if (IsLocallyControlled())
	{
		// FPArms2 clips carry the motion; the viewmodel springs only add a little weight on top.
		KGFP2::StateOf(this).bInspectClip = false;
		if (bBackstab)
		{
			// TF2 backstab: the knife_stab thrust.
			Viewmodel->AddRecoil(FVector(90.0, 0.0, -30.0), FVector(-90.0, 0.0, 0.0));
			PlayArmsOneShot(KGFP2::Anim(TEXT("knife_stab")));
		}
		else if (bShowingBlade)
		{
			// CS2-style slashes, alternating right-to-left (a) and left-to-right (b).
			const bool bFirst = AttackAlternate++ % 2 == 0;
			const float Side = bFirst ? 1.0f : -1.0f;
			Viewmodel->AddRecoil(FVector(50.0, 0.0, 0.0), FVector(-40.0, 60.0 * Side, 0.0));
			PlayArmsOneShot(bFirst ? AnimArmsAttack.Get() : AnimArmsAttack2.Get());
		}
		else
		{
			// Unarmed: right/left jab from the fists guard, fists stay up for a moment afterwards.
			const bool bRight = AttackAlternate++ % 2 == 0;
			Viewmodel->AddRecoil(FVector(60.0, 0.0, 0.0), FVector(-40.0, 0.0, 0.0));
			PlayArmsOneShot(bRight ? AnimArmsPunch.Get() : AnimArmsShove.Get());
			KGFP2::StateOf(this).FistsLinger = 1.6f;
		}
	}
	if (AnimAttack)
	{
		CurrentBodyAnim = AnimAttack;
		PlayBodyClip(AnimAttack, false, 0.08f);
		BodyOneShotRemaining = AnimAttack->GetPlayLength();
	}
	KGAudio::At(this, bBackstab || bShowingBlade ? TEXT("S_Stab") : TEXT("S_Punch"),
	            FirstPersonCamera->GetComponentLocation() + GetControlRotation().Vector() * 80.0f,
	            IsLocallyControlled() ? 0.6f : 1.0f);
}

void AKGCharacter::MulticastSwing_Implementation(bool bBackstab)
{
	// The owner already played it when the button went down.
	if (!IsLocallyControlled())
	{
		PlaySwingCosmetics(bBackstab);
	}
}

void AKGCharacter::ServerAttack_Implementation(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir)
{
	if (IsDead())
	{
		return;
	}
	Emote->ServerStop(EKGEmoteStop::Attacked);
	const FVector Forward = ViewDir.GetSafeNormal();
	if (HeldComponent)
	{
		UPrimitiveComponent* Held = HeldComponent;
		Release(false);
		if (IsValid(Held))
		{
			Held->AddImpulse(Forward * ThrowSpeed, NAME_None, true);
		}
		return;
	}
	// Small grace so a client whose cooldown ended a packet earlier is not rejected.
	if (ServerAttackCooldown > 0.15f)
	{
		return;
	}
	ServerAttackCooldown = MeleeCooldown;
	const FVector Start = ValidatedViewStart(ViewStart);

	// TF2-style: a valid back in range turns the swing into an instant backstab.
	if (bHoldingAssassinBlade)
	{
		if (AKGCharacter* Victim = FindBackstabTarget(Forward))
		{
			MulticastSwing(true);
			UGameplayStatics::ApplyDamage(Victim, 1000.0f, GetController(), this, UDamageType::StaticClass());
			UE_LOG(LogKillGodot, Log, TEXT("%s backstabbed %s"), *GetName(), *Victim->GetName());
			return;
		}
	}
	MulticastSwing(false);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGMelee), false, this);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Forward * MeleeRange, FQuat::Identity, Objects,
	                                   FCollisionShape::MakeSphere(25.0f), Params);
	TSet<AActor*> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == this || Damaged.Contains(HitActor))
		{
			continue;
		}
		Damaged.Add(HitActor);
		UGameplayStatics::ApplyDamage(HitActor, MeleeDamage, GetController(), this, UDamageType::StaticClass());
		if (UPrimitiveComponent* Comp = Hit.GetComponent(); Comp && Comp->IsSimulatingPhysics())
		{
			Comp->AddImpulseAtLocation(Forward * 400.0f * Comp->GetMass(), Hit.ImpactPoint);
		}
	}
}

void AKGCharacter::Shove()
{
	if (IsDead() || bHoldingObject)
	{
		return;
	}
	Emote->NotifyLocalAction();
	// The listen-server host spends in ServerShove; a remote client predicts its own stamina here.
	if (!HasAuthority() && !Stamina.TrySpend(ShoveStaminaCost))
	{
		return;
	}
	if (IsLocallyControlled())
	{
		// Shove: a left-hand push (punch_l) so a drawn knife stays in the right hand.
		Viewmodel->AddRecoil(FVector(120.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0));
		KGFP2::StateOf(this).bInspectClip = false;
		PlayArmsOneShot(AnimArmsShove);
		KGFP2::StateOf(this).FistsLinger = bShowingBlade ? 0.0f : 1.6f;
	}
	ServerShove(GetControlRotation().Vector());
}

void AKGCharacter::ServerShove_Implementation(FVector_NetQuantizeNormal ViewDir)
{
	if (IsDead() || HeldComponent || !Stamina.TrySpend(ShoveStaminaCost))
	{
		return;
	}
	Emote->ServerStop(EKGEmoteStop::Attacked);
	const FVector Forward = ViewDir.GetSafeNormal();
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGShove), false, this);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Forward * 140.0f, FQuat::Identity, Objects,
	                                   FCollisionShape::MakeSphere(40.0f), Params);
	TSet<AActor*> Pushed;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == this || Pushed.Contains(HitActor))
		{
			continue;
		}
		Pushed.Add(HitActor);
		if (ACharacter* Other = Cast<ACharacter>(HitActor))
		{
			// SPRINT-016 hook: a shove knocks whatever they carry out of their hands (players and bots).
			if (AKGCharacter* Carrier = Cast<AKGCharacter>(Other); Carrier && Carrier->HeldComponent)
			{
				UPrimitiveComponent* Knocked = Carrier->HeldComponent;
				Carrier->Release(false);
				if (IsValid(Knocked) && Knocked->IsSimulatingPhysics())
				{
					Knocked->AddImpulse(Forward * 350.0f + FVector(0.0f, 0.0f, 150.0f), NAME_None, true);
				}
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_KNOCK %s knocked %s out of %s's hands"), *GetName(), *GetNameSafe(Knocked ? Knocked->GetOwner() : nullptr),
				       *Carrier->GetName());
			}
			if (AKGChoreItem* Attached = AKGChoreItem::CarriedBy(Other); Attached && Attached->IsAttached())
			{
				Attached->AuthDetach(Forward * 350.0f + FVector(0.0f, 0.0f, 150.0f));
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_KNOCK %s knocked %s out of %s's hands"), *GetName(), *Attached->GetName(), *Other->GetName());
			}
			Other->LaunchCharacter(FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal() * ShoveImpulse +
			                       FVector(0.0, 0.0, 180.0), true, true);
		}
		else if (UPrimitiveComponent* Comp = Hit.GetComponent(); Comp && Comp->IsSimulatingPhysics())
		{
			Comp->AddImpulse(Forward * 600.0f, NAME_None, true);
		}
	}
}

void AKGCharacter::Inspect()
{
	if (IsDead() || bHoldingObject)
	{
		return;
	}
	KGFP2::FState& FP2 = KGFP2::StateOf(this);
	if (bShowingBlade)
	{
		// CS2 inspect: the keyframed knife_inspect clip (3.2 s) turns the knife in the fingers; restart only once it
		// has finished, like CS2 (spamming F does not restart the flourish).
		if (FP2.bInspectClip && ArmsOneShotRemaining > 0.0f)
		{
			return;
		}
		if (UAnimSequence* Clip = KGFP2::Anim(TEXT("knife_inspect")))
		{
			PlayArmsOneShot(Clip);
			FP2.bInspectClip = true;
		}
	}
	// Empty-handed this shows the open hands (hands_idle) for the inspect duration.
	Viewmodel->StartInspect();
}

bool AKGCharacter::CanDrawBlade() const
{
	// Before roles are dealt (lobby/warmup sandbox) anyone may play with the knife; afterwards only the Impatient.
	const AKGGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKGGameState>() : nullptr;
	const AKGPlayerState* PS = GetPlayerState<AKGPlayerState>();
	if (!GS || GS->GetPhase() == EKGPhase::Lobby || GS->GetPhase() == EKGPhase::Warmup || !PS ||
	    PS->GetPrivateRoleId().IsNone())
	{
		return true;
	}
	const FKGRoleInfo* RoleInfo =
		FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId());
	return RoleInfo && RoleInfo->GetAlignment() == EKGAlignment::Impatient;
}

void AKGCharacter::ToggleDevBlade()
{
	if (IsDead() || (!bHoldingAssassinBlade && !CanDrawBlade()))
	{
		return;
	}
	bHoldingAssassinBlade = !bHoldingAssassinBlade; // predicted; the server's value replicates back
	ServerSetAssassinBlade(bHoldingAssassinBlade);
}

void AKGCharacter::ServerSetAssassinBlade_Implementation(bool bHolding)
{
	// Server check: a Townie cannot conjure the knife by sending the RPC.
	bHoldingAssassinBlade = bHolding && !IsDead() && CanDrawBlade();
}

float AKGCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                               AActor* DamageCauser)
{
	const float Incoming = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || !Health)
	{
		return Incoming;
	}
	const FName Type = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetFName() : NAME_None;
	if (Incoming > 0.0f)
	{
		Emote->ServerStop(EKGEmoteStop::Damaged);   // being hit ends any emote
	}
	return Health->ApplyDamage(Incoming, DamageCauser, Type);
}

void AKGCharacter::HandleDeath(AActor* Killer, FName DamageType)
{
	Emote->ServerStop(EKGEmoteStop::Died);
	Release(false);
	bHoldingAssassinBlade = false;
	HeldItem->SetVisibility(false);
	// Death cam: no arms, eyes drop to the floor next to the body until ghosts arrive (M5).
	ArmsMesh->SetVisibility(false);
	FirstPersonCamera->SetRelativeLocation(FVector(-40.0f, 0.0f, -55.0f));
	if (HasAuthority())
	{
		if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
		{
			GM->OnCharacterDied(this, Killer);
		}
		// Humans become ghosts after a beat on the floor (bots just stay dead).
		if (IsPlayerControlled())
		{
			FTimerHandle GhostHandle;
			GetWorldTimerManager().SetTimer(GhostHandle, this, &AKGCharacter::BecomeGhost, 4.0f, false);
		}
	}
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (AnimDeath)
	{
		CurrentBodyAnim = AnimDeath;
		PlayBodyClip(AnimDeath, false, 0.1f);
	}
	else
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	UE_LOG(LogKillGodot, Log, TEXT("%s died (killer: %s)"), *GetName(), Killer ? *Killer->GetName() : TEXT("none"));
	// TODO(M5): ghost form, Limbo duel queue, body becomes evidence.
}

bool AKGCharacter::IsBackstabGeometry(const FVector& AttackerLocation, const FVector& AttackerForward,
                                      const FVector& TargetLocation, const FVector& TargetForward, float MaxDistance)
{
	FVector ToTarget = TargetLocation - AttackerLocation;
	ToTarget.Z = 0.0;
	const double Distance = ToTarget.Size();
	if (Distance > MaxDistance || Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	ToTarget /= Distance;
	const FVector AttackerFwd = FVector(AttackerForward.X, AttackerForward.Y, 0.0).GetSafeNormal();
	const FVector TargetFwd = FVector(TargetForward.X, TargetForward.Y, 0.0).GetSafeNormal();

	const bool bTargetInFront = FVector::DotProduct(AttackerFwd, ToTarget) > 0.5;   // within ~60 deg of our aim
	const bool bBehindTarget = FVector::DotProduct(TargetFwd, ToTarget) > 0.0;      // target faces away from us
	const bool bSameHeading = FVector::DotProduct(AttackerFwd, TargetFwd) > -0.3;   // no sideways face-stabs
	return bTargetInFront && bBehindTarget && bSameHeading;
}

AKGCharacter* AKGCharacter::FindBackstabTarget() const
{
	return FindBackstabTarget(GetControlRotation().Vector());
}

AKGCharacter* AKGCharacter::FindBackstabTarget(const FVector& ViewForward) const
{
	const FVector MyLocation = GetActorLocation();
	const FVector MyForward = ViewForward;
	AKGCharacter* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();

	for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
	{
		AKGCharacter* Other = *It;
		if (Other == this || Other->IsDead())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(MyLocation, Other->GetActorLocation());
		if (DistSq < BestDistSq &&
		    IsBackstabGeometry(MyLocation, MyForward, Other->GetActorLocation(), Other->GetActorForwardVector(),
		                       BackstabRange))
		{
			Best = Other;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

void AKGCharacter::Accuse()
{
	if (IsDead())
	{
		return;
	}
	// Generous sphere sweep so accusing someone across the ring is easy.
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + GetControlRotation().Vector() * 2500.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGAccuse), false, this);
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.0f),
	                                     Params))
	{
		if (const AKGCharacter* Other = Cast<AKGCharacter>(Hit.GetActor()))
		{
			AccusePlayer(Other->GetPlayerState());
		}
	}
}

void AKGCharacter::AccusePlayer(APlayerState* Target)
{
	if (Target)
	{
		ServerAccuse(Target);
	}
}

void AKGCharacter::VoteGuilty()
{
	ServerVerdict(true);
}

void AKGCharacter::VoteInnocent()
{
	ServerVerdict(false);
}

void AKGCharacter::ServerAccuse_Implementation(APlayerState* Target)
{
	if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
	{
		GM->HandleAccuse(GetPlayerState<AKGPlayerState>(), Cast<AKGPlayerState>(Target));
	}
}

void AKGCharacter::ServerVerdict_Implementation(bool bGuilty)
{
	if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
	{
		GM->HandleVerdict(GetPlayerState<AKGPlayerState>(), bGuilty);
	}
}

void AKGCharacter::BecomeGhost()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	// Free-flying spectator from where the body lies; the body stays in the world as evidence.
	PC->UnPossess();
	PC->ChangeState(NAME_Spectating);
	PC->ClientGotoState(NAME_Spectating);
}

void AKGCharacter::BeginTask(AKGTaskStation* Station)
{
	AKGPlayerState* PS = GetPlayerState<AKGPlayerState>();
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (!Station || !PS || IsDead() || !PS->HasOpenTask(Station->TaskId) || !GS)
	{
		return;
	}
	const EKGPhase Phase = GS->GetPhase();
	if (Phase == EKGPhase::Meeting || Phase == EKGPhase::Trial || Phase == EKGPhase::Epilogue ||
	    Phase == EKGPhase::Lobby || Phase == EKGPhase::Warmup)
	{
		return;
	}
	ActiveTask = Station;
	TaskProgress = 0.0f;
}

void AKGCharacter::TickMoveSmoke(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGMoveSmoke"));
	if (!bSmoke || !IsLocallyControlled() || IsDead())
	{
		return;
	}
	// Per-instance clock/step (no header change needed, Live Coding - mirrors the footstep StepAccum pattern above).
	static TMap<TWeakObjectPtr<const AKGCharacter>, float> Clock;
	static TMap<TWeakObjectPtr<const AKGCharacter>, int32> Step;
	float& T = Clock.FindOrAdd(this);
	int32& S = Step.FindOrAdd(this);
	T += DeltaSeconds;

	// T<1: settle. [1,6): "bad" strafe - straight line, one jump, never pressed again (no chaining possible without
	// a fresh press - proves holding/forgetting does not keep gaining speed). [6,11): "good" strafe - alternating
	// A/D + a matching yaw turn every ~0.18s with Jump re-pressed every tick (a human mashing space roughly on
	// beat), which reliably lands within the ~80ms buffer and keeps re-aiming the wish direction into the turn.
	const bool bGoodPhase = T >= 6.0f && T < 11.0f;
	const bool bBadPhase = T >= 1.0f && T < 6.0f;
	if (bBadPhase)
	{
		AddMovementInput(GetActorForwardVector(), 1.0f);
	}
	else if (bGoodPhase)
	{
		const float Beat = FMath::Fmod(T, 0.36f);
		const float Sign = Beat < 0.18f ? 1.0f : -1.0f;
		AddMovementInput(GetActorForwardVector(), 0.6f);
		AddMovementInput(GetActorRightVector(), Sign);
		AddControllerYawInput(Sign * 90.0f * DeltaSeconds);
		JumpStart();   // re-pressed every tick this phase: lands inside the buffer on (almost) every landing
	}

	struct FStep
	{
		float At;
		TFunction<void()> Run;
	};
	const TArray<FStep> Steps = {
		{1.0f, [&]() { JumpStart(); }},         // the one and only press of the bad phase
		{1.15f, [&]() { JumpEnd(); }},          // release: bad phase never presses again, so it cannot auto-chain
		{11.0f, [&]() { JumpEnd(); }},
		{11.2f, [&]()
		{
			const UKGCharacterMovement* KGMove = Cast<UKGCharacterMovement>(GetCharacterMovement());
			UE_LOG(LogKillGodot, Log, TEXT("KG_MOVE_DONE speed=%.1f corrections=%d"), GetHorizontalSpeed(),
			       KGMove ? KGMove->CorrectionCount : -1);
		}},
	};
	while (Steps.IsValidIndex(S) && T >= Steps[S].At)
	{
		Steps[S].Run();
		++S;
	}

	if (T >= 1.0f && T < 11.0f)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_MOVE_CSV,%.3f,%s,%.1f"), T, bGoodPhase ? TEXT("good_strafe") : bBadPhase ? TEXT("bad_strafe") : TEXT("settle"),
		       GetHorizontalSpeed());
	}
#endif
}

void AKGCharacter::TickTask(float DeltaSeconds)
{
	if (!ActiveTask)
	{
		return;
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	const bool bPhaseOk = GS && GS->GetPhase() != EKGPhase::Meeting && GS->GetPhase() != EKGPhase::Trial &&
	                      GS->GetPhase() != EKGPhase::Epilogue;
	if (IsDead() || !bPhaseOk ||
	    FVector::DistSquared2D(GetActorLocation(), ActiveTask->GetActorLocation()) > FMath::Square(ActiveTask->WorkRadius))
	{
		ActiveTask = nullptr;   // walked away or interrupted: progress is lost (Among Us rules)
		TaskProgress = 0.0f;
		return;
	}
	TaskProgress += DeltaSeconds / FMath::Max(0.5f, ActiveTask->WorkSeconds);
	if (TaskProgress >= 1.0f)
	{
		AKGPlayerState* PS = GetPlayerState<AKGPlayerState>();
		const FName Done = ActiveTask->TaskId;
		ActiveTask = nullptr;
		TaskProgress = 0.0f;
		if (PS && PS->CompleteTask(Done))
		{
			if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
			{
				GM->OnTaskCompleted(PS, Done);
			}
		}
	}
}

void AKGCharacter::UpdateRevealGlow()
{
	// Lighthouse Illumination: the revealed player's body glows through a red overlay for everyone.
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	const bool bGlow = GS && GS->LighthouseRevealed && GS->LighthouseRevealed == GetPlayerState() &&
	                   GS->GetServerWorldTimeSeconds() < GS->RevealUntil;
	static TWeakObjectPtr<UMaterialInterface> RevealMaterial;
	if (!RevealMaterial.IsValid())
	{
		RevealMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KillGodot/Materials/M_KG_RevealOverlay.M_KG_RevealOverlay"));
	}
	UMaterialInterface* Wanted = bGlow ? RevealMaterial.Get() : nullptr;
	if (GetMesh()->GetOverlayMaterial() != Wanted)
	{
		GetMesh()->SetOverlayMaterial(Wanted);
	}
}

APhysicsVolume* AKGCharacter::GetSeaVolume() const
{
	// One brushless water volume per world, found by tag or spawned on demand (every machine spawns its own).
	for (TActorIterator<APhysicsVolume> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("KG_Sea")))
		{
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APhysicsVolume* Sea = GetWorld()->SpawnActor<APhysicsVolume>(Params);
	if (Sea)
	{
		Sea->Tags.Add(TEXT("KG_Sea"));
		Sea->bWaterVolume = true;
		Sea->FluidFriction = 0.6f;
		Sea->SetReplicates(false);
	}
	return Sea;
}

void AKGCharacter::TickWaterAndLadders(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (IsDead())
	{
		return;
	}
	// Unreal swims only inside a water PhysicsVolume. The sea is a wave function, so the capsule's physics volume
	// is switched by hand between a brushless "sea" volume and the world default; the CMC then does the rest
	// (enter swimming, buoyancy, climb out). Automatic volume updates are off for the capsule (BeginPlay).
	if (Move->MovementMode != MOVE_Flying)
	{
		APhysicsVolume* Sea = GetSeaVolume();
		const bool bSubmerged = Move->IsInWater();
		UPrimitiveComponent* Cap = GetCapsuleComponent();
		if (bSubmerged && Cap->GetPhysicsVolume() != Sea)
		{
			Cap->SetPhysicsVolume(Sea, true);
			KGAudio::At(this, GetVelocity().Z < -500.0f ? TEXT("S_Splash_Big") : TEXT("S_Splash"), GetActorLocation());
		}
		else if (!bSubmerged && Cap->GetPhysicsVolume() == Sea)
		{
			Cap->SetPhysicsVolume(GetWorld()->GetDefaultPhysicsVolume(), true);
		}
	}
	if (Move->IsSwimming() && bSwimUp)
	{
		AddMovementInput(FVector::UpVector, 1.0f);
	}

	// Ladders: step into the climb volume facing the ladder -> climb (flying); leave it -> fall/walk.
	TSet<AActor*> Overlaps;
	GetCapsuleComponent()->GetOverlappingActors(Overlaps, AKGLadder::StaticClass());
	AKGLadder* Near = Overlaps.Num() > 0 ? Cast<AKGLadder>(*Overlaps.CreateConstIterator()) : nullptr;
	if (Near && Move->MovementMode != MOVE_Flying)
	{
		const float Facing = FVector::DotProduct(GetControlRotation().Vector().GetSafeNormal2D(), Near->GetActorForwardVector());
		const FVector Acc = Move->GetCurrentAcceleration();
		const bool bPushing = FVector::DotProduct(Acc.GetSafeNormal2D(), Near->GetActorForwardVector()) > 0.4f;
		if (Facing > 0.35f && (bPushing || Move->IsFalling()))
		{
			Ladder = Near;
			Move->SetMovementMode(MOVE_Flying);
			Move->Velocity = FVector::ZeroVector;
		}
	}
	if (Ladder.IsValid() && Move->MovementMode == MOVE_Flying)
	{
		Move->MaxFlySpeed = 260.0f;
		const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (!Near || Feet > Ladder->GetTopZ() - 10.0f)
		{
			// Off the top: hop forward onto the platform. Off the side: just fall.
			const bool bTop = Feet > Ladder->GetTopZ() - 10.0f;
			Move->SetMovementMode(MOVE_Falling);
			if (bTop)
			{
				LaunchCharacter(Ladder->GetActorForwardVector() * 320.0f + FVector(0, 0, 150.0f), true, true);
			}
			Ladder = nullptr;
		}
	}

	static TMap<TWeakObjectPtr<const AKGCharacter>, float> StepAccum;   // no header change needed (Live Coding)
	float& StepDistance = StepAccum.FindOrAdd(this);
	// Footsteps: one every ~1.7 m on the ground, sound by what we stand on (wood decks, stone, else grass).
	if (Move->IsMovingOnGround())
	{
		const float GroundSpeed = GetVelocity().Size2D();
		StepDistance += GroundSpeed * DeltaSeconds;
		// SPRINT-026: a third, longer stride for bhop-range landing speed (above plain sprint) so footsteps thin
		// out rather than machine-gunning once strafe-jumping pushes well past SprintSpeed.
		const float Stride = GroundSpeed > SprintSpeed ? 260.0f : GroundSpeed > 450.0f ? 210.0f : 165.0f;
		if (StepDistance > Stride)
		{
			StepDistance = 0.0f;
			const UPrimitiveComponent* Floor = Move->CurrentFloor.HitResult.GetComponent();
			const FString FloorName = Floor && Floor->GetOwner() ? Floor->GetOwner()->GetActorNameOrLabel() + Floor->GetName() : FString();
			const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Floor);
			const FString MeshName = SM && SM->GetStaticMesh() ? SM->GetStaticMesh()->GetName() : FString();
			const TCHAR* Kind = MeshName.Contains(TEXT("Wood")) || MeshName.Contains(TEXT("Stair")) ? TEXT("Wood")
			                  : MeshName.Contains(TEXT("Brick")) || MeshName.Contains(TEXT("Platform")) ? TEXT("Stone")
			                                                                                           : TEXT("Grass");
			const FString Name = FString::Printf(TEXT("S_Step_%s_%d"), Kind, FMath::RandRange(0, 2));
			KGAudio::At(this, *Name, GetActorLocation() - FVector(0, 0, 80.0f), IsLocallyControlled() ? 0.35f : 0.8f);
		}
	}

	// Underwater camera look (local only).
	if (IsLocallyControlled())
	{
		const FVector Eye = FirstPersonCamera->GetComponentLocation();
		UnderwaterPP->bEnabled = FKGWaves::IsUnderwater(Eye, GetWorld()->GetTimeSeconds());
	}
}

void AKGCharacter::InteractReleased()
{
	// Letting go of E drops whatever you were carrying.
	if (bHoldingObject)
	{
		ServerReleaseHeld();
	}
}

void AKGCharacter::ServerReleaseHeld_Implementation()
{
	if (HeldComponent)
	{
		Release(false);
	}
}

void AKGCharacter::RotateHeld()
{
	if (bHoldingObject)
	{
		ServerRotateHeld();
	}
}

void AKGCharacter::ServerRotateHeld_Implementation()
{
	HeldYaw += 45.0f;
}
