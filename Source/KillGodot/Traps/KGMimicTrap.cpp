#include "Traps/KGMimicTrap.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilitySubsystem.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGWoundComponent.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Combat/KGHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Traps/KGTrapSubsystem.h"
#include "World/KGBreakable.h"
#include "World/KGStorageChest.h"

namespace KGMimicPrivate
{
	enum ECue : uint8 { Snap = 0, Scream = 1, Flinch = 2 };

	/** Blender pack KG_Mimic (Tools/Blender/kg_make_mimic.py -> kg_import_dress_pack.py KG_Mimic_Clean). */
	UStaticMesh* LoadMesh(const TCHAR* Name, const TCHAR* Fallback)
	{
		// Non-static finders on purpose (Live++ keeps static locals; CLAUDE.md).
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KillGodot/Env/Dress/KG_Mimic_Clean/StaticMeshes/SM_KG_%s.SM_KG_%s"), Name, Name),
		                                         nullptr, LOAD_Quiet | LOAD_NoWarn);
		return M ? M : LoadObject<UStaticMesh>(nullptr, Fallback, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}

	UStaticMeshComponent* MakeOverlay(AActor* Owner, const TCHAR* Name)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetGenerateOverlapEvents(false);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetVisibility(false);
		C->CastShadow = false;
		return C;
	}

	/** 30 m falloff for the scream (KGAudio::Near is 18 m). */
	USoundAttenuation* Far()
	{
		static TWeakObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("KG_ScreamAttenuation"));
			A->AddToRoot();
			A->Attenuation.FalloffDistance = KGTrapperTuning::ScreamRadiusCm;
			A->Attenuation.AttenuationShapeExtents = FVector(400.0f, 0.0f, 0.0f);
			Att = A;
		}
		return Att.Get();
	}

	void Play(const UObject* Ctx, const TCHAR* Name, const FVector& At, float Volume, float Pitch, USoundAttenuation* Att)
	{
		if (USoundBase* S = KGAudio::Get(Name); S && Ctx && Ctx->GetWorld() && Ctx->GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(Ctx, S, At, Volume, Pitch, 0.0f, Att ? Att : KGAudio::Near());
		}
	}

	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : 0.0f;
	}

	int32 NextId = 0;
}

AKGMimicTrap::AKGMimicTrap()
{
	Effect = EKGTrapEffect::Custom;
	TrapDef.Kind = TEXT("Mimic");
	TrapDef.ArmPolicy = TEXT("None");   // armed only through the Trapper's ability, never with E
	TrapDef.bPassive = false;
	TrapDef.TelegraphSecs = 0.35f;      // the lid snaps
	TrapDef.ActiveSecs = KGTrapperTuning::BiteHoldSecs;
	TrapDef.CooldownSecs = 2.0f;
	TrapDef.ArmerCooldownSecs = 0.0f;   // the ability framework owns the Trapper's cooldowns
	TrapDef.Damage = KGTrapperTuning::BiteDamage;
	TrapDef.bIgnoreArmer = true;
	ZoneExtent = FVector(10.0);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetVisibility(false);
	Teeth = KGMimicPrivate::MakeOverlay(this, TEXT("Teeth"));
	Tongue = KGMimicPrivate::MakeOverlay(this, TEXT("Tongue"));
	Drool = KGMimicPrivate::MakeOverlay(this, TEXT("Drool"));
	Marks = KGMimicPrivate::MakeOverlay(this, TEXT("Marks"));
	SetNetCullDistanceSquared(FMath::Square(9000.0f));
}

void AKGMimicTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGMimicTrap, Host);
	DOREPLIFETIME(AKGMimicTrap, Victim);
	DOREPLIFETIME(AKGMimicTrap, bTeethMarks);
	DOREPLIFETIME(AKGMimicTrap, FlinchSerial);
	DOREPLIFETIME(AKGMimicTrap, FrontDir);
}

void AKGMimicTrap::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HeldBody.IsValid())
	{
		KGTrapHold::Apply(HeldBody.Get(), false);
		HeldBody = nullptr;
	}
	if (UStaticMeshComponent* M = TouchedHostMesh.Get())
	{
		M->SetRelativeScale3D(HostMeshRestScale);
	}
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Lookup / arming
// ---------------------------------------------------------------------------------------------------------------------

bool AKGMimicTrap::IsContainer(const AActor* Actor)
{
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetLifeSpan() > 0.0f)
	{
		return false;   // a burst crate gets a life span (AKGBreakable::TakeDamage)
	}
	return Actor->IsA<AKGStorageChest>() || Actor->IsA<AKGBreakable>();
}

AKGMimicTrap* AKGMimicTrap::FindOn(const AActor* Host)
{
	if (!Host)
	{
		return nullptr;
	}
	if (const UKGTrapSubsystem* S = UKGTrapSubsystem::Get(Host->GetWorld()))
	{
		for (const TWeakObjectPtr<AKGTrap>& T : S->GetTraps())
		{
			AKGMimicTrap* M = Cast<AKGMimicTrap>(T.Get());
			if (M && M->Host == Host && !M->IsActorBeingDestroyed())
			{
				return M;
			}
		}
	}
	return nullptr;
}

AKGMimicTrap* AKGMimicTrap::AuthArmOn(AActor* Host, AKGCharacter* By)
{
	if (!IsContainer(Host) || !Host->HasAuthority())
	{
		return nullptr;
	}
	AKGMimicTrap* M = FindOn(Host);
	if (M && M->State != EKGTrapState::Idle)
	{
		return nullptr;   // already armed, biting or cooling down
	}
	if (!M)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M = Host->GetWorld()->SpawnActor<AKGMimicTrap>(Host->GetActorLocation(), FRotator::ZeroRotator, P);
		if (!M)
		{
			return nullptr;
		}
		M->TrapId = FName(*FString::Printf(TEXT("Mimic_%d"), ++KGMimicPrivate::NextId));
		M->Room = UKGAbilitySubsystem::PlaceName(Host->GetWorld(), Host->GetActorLocation());
		M->Host = Host;
	}
	M->SetActorLocation(Host->GetActorLocation());
	M->HostRestLocation = Host->GetActorLocation();
	const FVector Towards = By ? (By->GetActorLocation() - Host->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	M->FrontDir = Towards.IsNearlyZero() ? Host->GetActorForwardVector().GetSafeNormal2D() : Towards;
	if (!M->AuthArm(By, true))
	{
		return nullptr;
	}
	M->FlushNetDormancy();
	M->ForceNetUpdate();
	return M;
}

bool AKGMimicTrap::TryBite(AActor* Host, AKGCharacter* By)
{
	AKGMimicTrap* M = Host && Host->HasAuthority() ? FindOn(Host) : nullptr;
	if (!M || !M->IsMimicArmed() || !By || By->IsDead() || (M->TrapDef.bIgnoreArmer && M->IsArmer(By)))
	{
		return false;
	}
	return M->AuthBite(By);
}

bool AKGMimicTrap::AuthBite(AKGCharacter* Who)
{
	if (!HasAuthority() || !Who || Who->IsDead() || !Host)
	{
		return false;
	}
	if (State == EKGTrapState::Idle)
	{
		Machine.TryArm();   // dev / shots: a sleeping mimic wakes up for the bite
	}
	if (!Machine.TryTrigger())
	{
		return false;
	}
	LastTrigger = Who;
	Victim = Who;
	FlushNetDormancy();
	SetState(Machine.State);
	OnTelegraph();
	return true;
}

void AKGMimicTrap::AuthFlinch()
{
	if (!HasAuthority() || !Host)
	{
		return;
	}
	FlushNetDormancy();
	++FlinchSerial;
	FlinchCooldown = 1.0f;
	ForceNetUpdate();
	MulticastMimicCue(KGMimicPrivate::Flinch, Host->GetActorLocation());
	Record(TEXT("Flinch"), nullptr);
}

void AKGMimicTrap::AuthExpire()
{
	if (!HasAuthority())
	{
		return;
	}
	if (IsBiting())
	{
		return;   // finishes its bite; it is spent afterwards anyway
	}
	if (!bTeethMarks)
	{
		Record(TEXT("Expired"), nullptr);
		Destroy();
		return;
	}
	if (State == EKGTrapState::Armed)
	{
		Record(TEXT("Expired"), nullptr);
		Machine.Reset();
		ArmedByPuid.Reset();
		SetState(Machine.State);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The bite (server)
// ---------------------------------------------------------------------------------------------------------------------

void AKGMimicTrap::OnTelegraph()
{
	MulticastMimicCue(KGMimicPrivate::Snap, Host ? Host->GetActorLocation() : GetActorLocation());
	Record(TEXT("Telegraph"), Victim);
}

void AKGMimicTrap::OnFire()
{
	AKGCharacter* V = Victim;
	if (V && !V->IsDead())
	{
		if (UKGHealthComponent* H = V->GetHealth())
		{
			// Heavy, but never an instant kill (SPRINT-041 acceptance 2).
			const float Amount = FMath::Min(TrapDef.Damage, H->GetHealth() - KGTrapperTuning::BiteMinHealthLeft);
			if (Amount > 0.0f)
			{
				H->ApplyDamage(Amount, this, TEXT("MimicBite"));
			}
		}
		UKGWoundComponent::AuthAdd(V, UKGWoundComponent::BiteMarks);
	}
	const FVector At = V ? V->GetActorLocation() : GetActorLocation();
	MulticastMimicCue(KGMimicPrivate::Scream, At);
	Record(TEXT("Fired"), V);
	Record(TEXT("Scream"), V);
	if (UKGAbilitySubsystem* A = UKGAbilitySubsystem::Get(GetWorld()))
	{
		A->NoteMimicScream(Host, At, V);
		A->NotifyArmer(ArmedByPuid, FString::Printf(TEXT("%s: %s"), *FKGAbilityCatalog::DisplayName(TEXT("Mimic"), false),
		                                            V && V->GetPlayerState() ? *V->GetPlayerState()->GetPlayerName() : TEXT("someone")));
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC bite %s victim=%s health=%.0f wounds=%s"), *TrapId.ToString(), *GetNameSafe(V),
	       V && V->GetHealth() ? V->GetHealth()->GetHealth() : -1.0f, *UKGWoundComponent::ListOn(V));
}

void AKGMimicTrap::OnEnd()
{
	FlushNetDormancy();
	bTeethMarks = true;
	Record(TEXT("Ended"), nullptr);
	UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC reset %s teethmarks=1"), *TrapId.ToString());
}

void AKGMimicTrap::OnReady()
{
	Super::OnReady();
	Victim = nullptr;
}

void AKGMimicTrap::ServerWatch(float DeltaSeconds)
{
	if (!IsValid(Host) || Host->IsActorBeingDestroyed() || Host->GetLifeSpan() > 0.0f)
	{
		if (!IsBiting())
		{
			Destroy();   // the crate burst or the chest is gone
		}
		return;
	}
	SetActorLocation(Host->GetActorLocation());
	if (State != EKGTrapState::Armed)
	{
		return;
	}
	FlinchCooldown -= DeltaSeconds;
	UWorld* World = GetWorld();
	const FVector Center = Host->GetComponentsBoundingBox(true).GetCenter();
	// Throw-test: a thrown object that lands near a mimic makes it flinch (it stays armed).
	if (FlinchCooldown <= 0.0f)
	{
		TArray<FOverlapResult> Hits;
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_PhysicsBody);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGMimicFlinch), false, Host);
		Params.AddIgnoredActor(this);
		if (World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj, FCollisionShape::MakeSphere(KGTrapperTuning::FlinchRadiusCm), Params))
		{
			for (const FOverlapResult& R : Hits)
			{
				const UPrimitiveComponent* C = R.GetComponent();
				if (C && C->IsSimulatingPhysics() && !Cast<AKGCharacter>(R.GetActor()) &&
				    C->GetPhysicsLinearVelocity().Size() >= KGTrapperTuning::FlinchSpeedCmS)
				{
					UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC flinch %s thrown=%s speed=%.0f"), *TrapId.ToString(), *GetNameSafe(R.GetActor()),
					       C->GetPhysicsLinearVelocity().Size());
					AuthFlinch();
					break;
				}
			}
		}
	}
	// Lifting / dragging a mimic crate counts as opening it.
	if (Host->IsA<AKGBreakable>() && FVector::Dist(Host->GetActorLocation(), HostRestLocation) > 35.0)
	{
		AKGCharacter* Best = nullptr;
		double BestD = FMath::Square(300.0);
		for (TActorIterator<AKGCharacter> It(World); It; ++It)
		{
			const double D = FVector::DistSquared(It->GetActorLocation(), Host->GetActorLocation());
			if (!It->IsDead() && D < BestD && !(TrapDef.bIgnoreArmer && IsArmer(*It)))
			{
				BestD = D;
				Best = *It;
			}
		}
		HostRestLocation = Host->GetActorLocation();
		if (Best)
		{
			AuthBite(Best);
		}
	}
}

void AKGMimicTrap::ApplyHold()
{
	AKGCharacter* Want = State == EKGTrapState::Active && Victim && !Victim->IsDead() ? Victim.Get() : nullptr;
	if (HeldBody.Get() == Want)
	{
		return;
	}
	if (HeldBody.IsValid())
	{
		KGTrapHold::Apply(HeldBody.Get(), false);
	}
	HeldBody = Want;
	if (Want)
	{
		KGTrapHold::Apply(Want, true);
	}
}

void AKGMimicTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerWatch(DeltaSeconds);
	}
	ApplyHold();
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLook(DeltaSeconds);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Cosmetics (every machine but a dedicated server)
// ---------------------------------------------------------------------------------------------------------------------

void AKGMimicTrap::MulticastMimicCue_Implementation(uint8 Cue, FVector_NetQuantize At)
{
	using namespace KGMimicPrivate;
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const float Dist = Me ? FVector::Dist(Me->GetActorLocation(), At) : -1.0f;
	switch (Cue)
	{
	case Snap:
		Play(this, TEXT("S_Fish_Snap"), At, 1.0f, 0.7f, nullptr);
		Play(this, TEXT("S_Forest_Snarl"), At, 1.0f, 1.25f, nullptr);
		break;
	case Scream:
		Play(this, TEXT("S_Forest_Howl"), At, 1.6f, 1.6f, Far());   // TODO(audio): a real scream (Backlog proposal)
		break;
	case Flinch:
		Play(this, TEXT("S_Chore_Thud"), At, 0.7f, 1.3f, nullptr);
		break;
	default: break;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC_CUE %s cue=%s dist=%.0f audible=%d"), GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Host"),
	       Cue == Snap ? TEXT("snap") : Cue == Scream ? TEXT("scream") : TEXT("flinch"), Dist,
	       Dist >= 0.0f && Dist <= (Cue == Scream ? KGTrapperTuning::ScreamRadiusCm : 1800.0f) ? 1 : 0);
}

UStaticMeshComponent* AKGMimicTrap::HostMesh() const
{
	if (!Host)
	{
		return nullptr;
	}
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Host.Get());
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M && M->GetStaticMesh() && M->IsVisible())
		{
			return M;
		}
	}
	return nullptr;
}

FString AKGMimicTrap::GetLookName() const
{
	if (!Host)
	{
		return TEXT("hidden");
	}
	if (IsBiting())
	{
		return TEXT("bite");
	}
	if (FlinchLook > 0.0f)
	{
		return TEXT("flinch");
	}
	if (State == EKGTrapState::Armed)
	{
		return TEXT("rest");
	}
	return bTeethMarks ? TEXT("marks") : TEXT("hidden");
}

void AKGMimicTrap::TickLook(float DeltaSeconds)
{
	LookTime += DeltaSeconds;
	if (!Teeth->GetStaticMesh())
	{
		using KGMimicPrivate::LoadMesh;
		Teeth->SetStaticMesh(LoadMesh(TEXT("MimicTeeth"), TEXT("/Engine/BasicShapes/Cone.Cone")));
		Tongue->SetStaticMesh(LoadMesh(TEXT("MimicTongue"), TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Drool->SetStaticMesh(LoadMesh(TEXT("MimicDrool"), TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Marks->SetStaticMesh(LoadMesh(TEXT("MimicMarks"), TEXT("/Engine/BasicShapes/Cube.Cube")));
	}
	if (FlinchSerial != SeenFlinchSerial)
	{
		SeenFlinchSerial = FlinchSerial;
		FlinchLook = 0.6f;
		UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC_SEEN %s flinch serial=%d"), GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Host"), FlinchSerial);
	}
	FlinchLook = FMath::Max(0.0f, FlinchLook - DeltaSeconds);
	FitOverlays();

	// Telegraph sounds for whoever stands close (local only).
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (State == EKGTrapState::Armed && Host && Me &&
	    FVector::Dist(Me->GetActorLocation(), Host->GetActorLocation()) <= KGTrapperTuning::BreathRadiusCm)
	{
		BreathTimer -= DeltaSeconds;
		DripTimer -= DeltaSeconds;
		if (BreathTimer <= 0.0f)
		{
			BreathTimer = 3.2f;
			KGMimicPrivate::Play(this, TEXT("S_Forest_Whisper"), Host->GetActorLocation(), 0.25f, 0.55f, nullptr);
		}
		if (DripTimer <= 0.0f)
		{
			DripTimer = 1.7f;
			KGMimicPrivate::Play(this, TEXT("S_Cave_Drip"), Host->GetActorLocation(), 0.3f, 1.2f, nullptr);
		}
	}
}

void AKGMimicTrap::FitOverlays()
{
	UStaticMeshComponent* HM = HostMesh();
	const bool bArmed = State == EKGTrapState::Armed;
	const bool bBite = IsBiting();
	const bool bShowMarks = bTeethMarks && !bBite;
	if (!HM)
	{
		Teeth->SetVisibility(false);
		Tongue->SetVisibility(false);
		Drool->SetVisibility(false);
		Marks->SetVisibility(false);
		return;
	}
	// The host's own mesh breathes / chomps (chests only: a physics crate must not be rescaled under the solver).
	const bool bScaleHost = !HM->IsSimulatingPhysics();
	if (bScaleHost && TouchedHostMesh.Get() != HM)
	{
		TouchedHostMesh = HM;
		HostMeshRestScale = HM->GetRelativeScale3D();
	}
	if (bScaleHost)
	{
		float Z = 1.0f;
		float XY = 1.0f;
		if (bBite)
		{
			Z = 1.0f + 0.10f * FMath::Abs(FMath::Sin(LookTime * 16.0f));
			XY = 1.0f + 0.03f * FMath::Sin(LookTime * 16.0f);
		}
		else if (FlinchLook > 0.0f)
		{
			Z = 1.0f + 0.12f * FlinchLook;
		}
		else if (bArmed)
		{
			Z = 1.0f + 0.015f * FMath::Sin(LookTime * 2.1f);   // breathing
		}
		HM->SetRelativeScale3D(HostMeshRestScale * FVector(XY, XY, Z));
	}

	const FBox Local = HM->GetStaticMesh()->GetBoundingBox();
	const FTransform T = HM->GetComponentTransform();
	const FVector Size = Local.GetSize();
	const FVector C = Local.GetCenter();
	const float SeamZ = Local.Min.Z + Size.Z * 0.62f;
	const FVector Scale3 = T.GetScale3D();
	const FVector SeamWorld = T.TransformPosition(FVector(C.X, C.Y, SeamZ));

	// Teeth ring: unit 100 x 100 cm footprint, teeth +-10 cm around the seam (kg_make_mimic.py).
	float TeethZ = 0.0f;
	if (bBite)
	{
		TeethZ = 1.2f + 0.5f * FMath::Abs(FMath::Sin(LookTime * 16.0f));
	}
	else if (FlinchLook > 0.0f)
	{
		TeethZ = 1.1f;
	}
	else if (bArmed)
	{
		TeethZ = 0.38f + 0.04f * FMath::Sin(LookTime * 2.1f);   // tips only: the lid sits ajar
	}
	const FVector RingScale(Size.X / 100.0f * 1.04f, Size.Y / 100.0f * 1.04f, FMath::Max(0.01f, Size.Z / 60.0f));
	Teeth->SetVisibility(TeethZ > 0.0f);
	if (TeethZ > 0.0f)
	{
		Teeth->SetWorldTransform(FTransform(T.GetRotation(), SeamWorld, Scale3 * RingScale * FVector(1.0f, 1.0f, TeethZ)));
	}
	Marks->SetVisibility(bShowMarks);
	if (bShowMarks)
	{
		Marks->SetWorldTransform(FTransform(T.GetRotation(), SeamWorld, Scale3 * RingScale * FVector(1.005f, 1.005f, 1.0f)));
	}

	// Mouth side: the box face that points most along FrontDir.
	const FVector LocalFront = T.InverseTransformVectorNoScale(FVector(FrontDir));
	FVector Face = FMath::Abs(LocalFront.X) >= FMath::Abs(LocalFront.Y) ? FVector(FMath::Sign(LocalFront.X), 0.0, 0.0)
	                                                                     : FVector(0.0, FMath::Sign(LocalFront.Y), 0.0);
	if (Face.IsNearlyZero())
	{
		Face = FVector(1.0, 0.0, 0.0);
	}
	const FVector Lip = FVector(C.X, C.Y, SeamZ) + Face * (Face.X != 0.0 ? Size.X * 0.5 : Size.Y * 0.5);
	const FVector LipWorld = T.TransformPosition(Lip);
	const FVector OutWorld = T.TransformVectorNoScale(Face).GetSafeNormal();
	const FRotator OutRot = OutWorld.Rotation();
	const float Mouth = FMath::Clamp(Size.Z * Scale3.Z / 60.0f, 0.4f, 2.0f);

	float TongueLen = 0.0f;
	if (bBite)
	{
		TongueLen = 1.0f + 0.25f * FMath::Sin(LookTime * 9.0f);
	}
	else if (bArmed)
	{
		TongueLen = 0.12f;   // the tip, just over the lip
	}
	Tongue->SetVisibility(TongueLen > 0.0f);
	if (TongueLen > 0.0f)
	{
		const float Droop = bBite ? -25.0f + 12.0f * FMath::Sin(LookTime * 7.0f) : -12.0f;
		const float Thick = bBite ? Mouth : Mouth * 0.45f;   // at rest only a thin tip pokes out of the seam
		Tongue->SetWorldTransform(FTransform(FRotator(Droop, OutRot.Yaw, 0.0f), LipWorld - FVector(0.0, 0.0, 3.0), FVector(TongueLen * Mouth, Thick, Thick)));
	}
	const bool bDrip = bArmed && FlinchLook <= 0.0f;
	Drool->SetVisibility(bDrip);
	if (bDrip)
	{
		const float Phase = FMath::Fmod(LookTime, 1.7f) / 1.7f;
		Drool->SetWorldTransform(FTransform(FRotator(0.0f, OutRot.Yaw, 0.0f), LipWorld + OutWorld * 4.0f - FVector(0.0, 0.0, 2.0),
		                                    FVector(Mouth, Mouth, Mouth * (0.4f + 1.1f * Phase))));
	}
}
