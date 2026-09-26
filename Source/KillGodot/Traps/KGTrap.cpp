#include "Traps/KGTrap.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Combat/KGHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "Traps/KGTrapSubsystem.h"
#include "UObject/UnrealType.h"
#include "World/KGDoor.h"

#define LOCTEXT_NAMESPACE "KGTrap"

namespace KGTrapPrivate
{
	constexpr float WitnessSampleSecs = 0.5f;
	constexpr int32 MaxWitnesses = 5;
	constexpr float WitnessFacingDot = 0.3f;

	/** AKGDoor keeps bLocked protected without a getter; read it through reflection (the header is not ours). */
	bool IsDoorLocked(const AKGDoor* Door)
	{
		static const FBoolProperty* Prop = CastField<FBoolProperty>(AKGDoor::StaticClass()->FindPropertyByName(TEXT("bLocked")));
		return Door && Prop && Prop->GetPropertyValue_InContainer(Door);
	}

	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f);
	}

	const TCHAR* CueSound(uint8 Cue)
	{
		switch (Cue)
		{
		case 1: return TEXT("S_CrateBreak");
		case 2: return TEXT("S_Gate_Locked");
		case 3: return TEXT("S_Passage_Door");
		case 4: return TEXT("S_Chore_Match");
		case 5: return TEXT("S_Doorbell");
		default: return TEXT("S_Step_Wood_0");
		}
	}
}

AKGTrap::AKGTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	NetDormancy = DORM_Awake;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(GetRootComponent());
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCanEverAffectNavigation(false);
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(GetRootComponent());
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetGenerateOverlapEvents(false);
	Zone->SetHiddenInGame(true);
	Zone->SetCanEverAffectNavigation(false);
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGTrap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyZone();
}

void AKGTrap::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyZone();
	Machine.Configure(TrapDef);
	State = Machine.State;
	CosmeticState = State;
}

void AKGTrap::ApplyZone()
{
	Zone->SetBoxExtent(ZoneExtent);
	Zone->SetRelativeLocation(ZoneOffset);
}

void AKGTrap::BeginPlay()
{
	Super::BeginPlay();
	if (UKGTrapSubsystem* S = Subsystem())
	{
		S->RegisterTrap(this);
	}
	if (HasAuthority() && Effect == EKGTrapEffect::LockDoors && LockedDoorNames.Num() > 0)
	{
		// Restored from a snapshot mid-lock: find the doors again.
		for (TActorIterator<AKGDoor> It(GetWorld()); It; ++It)
		{
			if (LockedDoorNames.Contains(It->GetName()))
			{
				LockedDoors.Add(*It);
			}
		}
	}
}

void AKGTrap::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreLights();
	if (UKGTrapSubsystem* S = Subsystem())
	{
		S->UnregisterTrap(this);
	}
	Super::EndPlay(Reason);
}

void AKGTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AKGTrap, TrapId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGTrap, Room, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGTrap, Effect, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AKGTrap, TrapDef, COND_InitialOnly);
	DOREPLIFETIME(AKGTrap, State);
	DOREPLIFETIME(AKGTrap, bPing);
	DOREPLIFETIME(AKGTrap, Witnesses);
}

UKGTrapSubsystem* AKGTrap::Subsystem() const
{
	return UKGTrapSubsystem::Get(GetWorld());
}

AKGTrap* AKGTrap::FindById(const UWorld* World, FName Id)
{
	if (const UKGTrapSubsystem* S = UKGTrapSubsystem::Get(World))
	{
		if (AKGTrap* T = S->FindTrap(Id))
		{
			return T;
		}
	}
	for (TActorIterator<AKGTrap> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->TrapId == Id)
		{
			return *It;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------------------------------

bool AKGTrap::IsInZone(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation) - ZoneOffset;
	return FMath::Abs(Local.X) <= ZoneExtent.X && FMath::Abs(Local.Y) <= ZoneExtent.Y && FMath::Abs(Local.Z) <= ZoneExtent.Z;
}

bool AKGTrap::IsArmer(const AKGCharacter* Who) const
{
	const AKGPlayerState* PS = Who ? Who->GetPlayerState<AKGPlayerState>() : nullptr;
	return PS && !ArmedByPuid.IsEmpty() && PS->Puid == ArmedByPuid;
}

bool AKGTrap::MayArm(const AKGCharacter* Who) const
{
	if (!Who || TrapDef.bPassive || Effect == EKGTrapEffect::Witness)
	{
		return false;
	}
	const UKGTrapSubsystem* S = Subsystem();
	return S ? S->CanArm(TrapDef.ArmPolicy, Who) : FKGTrapArmPolicies().CanArm(TrapDef.ArmPolicy, Who);
}

void AKGTrap::GatherInZone(TArray<AKGCharacter*>& Out, bool bSkipArmer) const
{
	for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
	{
		AKGCharacter* C = *It;
		if (!IsValid(C) || C->IsDead() || !IsInZone(C->GetActorLocation()))
		{
			continue;
		}
		if (bSkipArmer && TrapDef.bIgnoreArmer && IsArmer(C))
		{
			continue;
		}
		Out.Add(C);
	}
}

FString AKGTrap::Describe() const
{
	return FString::Printf(TEXT("%s [%s] %s room=%s state=%s%s%s clock=%.1f policy=%s witnesses=%d"), *TrapId.ToString(),
	                       KGTrap::EffectName(Effect), *TrapDef.Kind.ToString(), *Room.ToString(), KGTrap::StateName(State),
	                       bPing ? TEXT(" PING") : TEXT(""), ArmedByPuid.IsEmpty() ? TEXT("") : TEXT(" (armed by a player)"),
	                       Machine.Remaining(), *TrapDef.ArmPolicy.ToString(), Witnesses.Num());
}

// ---------------------------------------------------------------------------------------------------------------------
// Authority
// ---------------------------------------------------------------------------------------------------------------------

void AKGTrap::SetState(EKGTrapState New)
{
	if (State != New)
	{
		FlushNetDormancy();
		State = New;
		ForceNetUpdate();
	}
}

void AKGTrap::Record(FName Type, const AKGCharacter* By) const
{
	UKGTrapSubsystem* S = Subsystem();
	if (!S)
	{
		return;
	}
	FKGTrapEvent E;
	E.TrapId = TrapId;
	E.Kind = TrapDef.Kind.IsNone() ? FName(KGTrap::EffectName(Effect)) : TrapDef.Kind;
	E.Room = Room;
	E.Type = Type;
	if (const AKGPlayerState* PS = By ? By->GetPlayerState<AKGPlayerState>() : nullptr)
	{
		E.ByPuid = PS->Puid;
		E.ByName = PS->GetPlayerName();
	}
	E.Location = GetActorLocation();
	E.WorldSeconds = KGTrapPrivate::ServerNow(GetWorld());
	S->Record(E);
}

bool AKGTrap::AuthArm(AKGCharacter* By, bool bIgnorePolicy)
{
	if (!HasAuthority() || Effect == EKGTrapEffect::Witness)
	{
		return false;
	}
	if (!bIgnorePolicy && !MayArm(By))
	{
		return false;
	}
	if (!Machine.TryArm())
	{
		return false;
	}
	const AKGPlayerState* PS = By ? By->GetPlayerState<AKGPlayerState>() : nullptr;
	ArmedByPuid = PS ? PS->Puid : FString();
	if (PS && !PS->Puid.IsEmpty())
	{
		if (UKGTrapSubsystem* S = Subsystem())
		{
			S->StartArmerCooldown(PS->Puid, TrapDef.ArmerCooldownSecs);
		}
	}
	SetState(Machine.State);
	Record(TEXT("Armed"), By);
	return true;
}

void AKGTrap::AuthForceFire()
{
	if (!HasAuthority())
	{
		return;
	}
	Machine.ForceFire();
	SetState(Machine.State);
	OnTelegraph();
}

void AKGTrap::AuthReset()
{
	if (!HasAuthority())
	{
		return;
	}
	if (State == EKGTrapState::Active || State == EKGTrapState::Telegraph)
	{
		OnEnd();
	}
	Machine.Configure(TrapDef);
	ArmedByPuid.Reset();
	bPing = false;
	Witnesses.Reset();
	SetState(Machine.State);
}

void AKGTrap::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority() || !By || By->IsDead() || Effect == EKGTrapEffect::Witness)
	{
		return;
	}
	if (!MayArm(By) || State != EKGTrapState::Idle)
	{
		return;
	}
	const AKGPlayerState* PS = By->GetPlayerState<AKGPlayerState>();
	const UKGTrapSubsystem* S = Subsystem();
	if (PS && S && S->GetArmerCooldown(PS->Puid) > 0.0f)
	{
		Record(TEXT("Denied"), By);
		return;
	}
	AuthArm(By, false);
}

FText AKGTrap::GetInteractPrompt_Implementation() const
{
	if (Effect == EKGTrapEffect::Witness)
	{
		if (Witnesses.Num() == 0)
		{
			return LOCTEXT("EyesNothing", "The eyes remember nothing");
		}
		const float Now = KGTrapPrivate::ServerNow(GetWorld());
		FString List;
		for (const FKGTrapWitness& W : Witnesses)
		{
			List += FString::Printf(TEXT("%s%s (%d s ago)"), List.IsEmpty() ? TEXT("") : TEXT(", "), *W.Name,
			                        FMath::Max(0, FMath::RoundToInt(Now - W.At)));
		}
		return FText::Format(LOCTEXT("EyesRemember", "The eyes remember: {0}"), FText::FromString(List));
	}
	// Computed on the viewer's machine: the local player's own (owner-only) role decides what they see.
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AKGCharacter* Local = PC ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr;
	if (!Local || !MayArm(Local))
	{
		return FText::GetEmpty();
	}
	switch (State)
	{
	case EKGTrapState::Idle:
		return FText::Format(LOCTEXT("Arm", "Arm the trap ({0})"), FText::FromName(TrapDef.Kind.IsNone() ? FName(KGTrap::EffectName(Effect)) : TrapDef.Kind));
	case EKGTrapState::Armed:
		return LOCTEXT("Armed", "Armed");
	default:
		return FText::GetEmpty();
	}
}

void AKGTrap::ApplyDamageTo(AKGCharacter* C) const
{
	if (!C || TrapDef.Damage <= 0.0f)
	{
		return;
	}
	if (UKGHealthComponent* H = C->GetHealth())
	{
		H->ApplyDamage(TrapDef.Damage, const_cast<AKGTrap*>(this), TEXT("Trap"));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The machine
// ---------------------------------------------------------------------------------------------------------------------

bool AKGTrap::WantsTrigger(const TArray<AKGCharacter*>& InZone) const
{
	return Effect != EKGTrapEffect::Witness && InZone.Num() > 0;
}

void AKGTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		for (const FKGTrapTransition& T : Machine.Advance(DeltaSeconds))
		{
			SetState(T.To);
			switch (T.To)
			{
			case EKGTrapState::Active: OnFire(); break;
			case EKGTrapState::Cooldown: OnEnd(); break;
			case EKGTrapState::Idle:
			case EKGTrapState::Armed: OnReady(); break;
			default: break;
			}
		}
		if (Machine.IsArmed())
		{
			if (Effect == EKGTrapEffect::Witness)
			{
				SampleWitnesses(DeltaSeconds);
			}
			else
			{
				TArray<AKGCharacter*> InZone;
				GatherInZone(InZone, true);
				if (WantsTrigger(InZone) && Machine.TryTrigger())
				{
					LastTrigger = InZone.Num() > 0 ? InZone[0] : nullptr;
					SetState(Machine.State);
					OnTelegraph();
				}
			}
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickCosmetics(DeltaSeconds);
	}
}

void AKGTrap::OnTelegraph()
{
	switch (Effect)
	{
	case EKGTrapEffect::LockDoors: MulticastCue(static_cast<uint8>(ECue::Clunk)); break;
	case EKGTrapEffect::LightsOut: MulticastCue(static_cast<uint8>(ECue::Flicker)); break;
	case EKGTrapEffect::Custom:
	case EKGTrapEffect::Witness: break;
	default: MulticastCue(static_cast<uint8>(ECue::Creak)); break;
	}
	Record(TEXT("Telegraph"), LastTrigger.Get());
}

void AKGTrap::OnFire()
{
	switch (Effect)
	{
	case EKGTrapEffect::Trapdoor:
	{
		TArray<AKGCharacter*> Victims;
		GatherInZone(Victims, true);
		for (AKGCharacter* C : Victims)
		{
			// Keeps the control rotation: only the body moves.
			C->TeleportTo(TeleportTarget, C->GetActorRotation(), false, true);
			ApplyDamageTo(C);
		}
		MulticastCue(static_cast<uint8>(ECue::Trapdoor));
		break;
	}
	case EKGTrapEffect::FallingObject:
	{
		TArray<AKGCharacter*> Victims;
		GatherInZone(Victims, true);
		for (AKGCharacter* C : Victims)
		{
			ApplyDamageTo(C);
		}
		MulticastCue(static_cast<uint8>(ECue::Crash));
		break;
	}
	case EKGTrapEffect::LockDoors:
	{
		const FName Tag(*FString::Printf(TEXT("KG_Room_%s"), *Room.ToString()));
		LockedDoors.Reset();
		LockedDoorNames.Reset();
		for (TActorIterator<AKGDoor> It(GetWorld()); It; ++It)
		{
			AKGDoor* Door = *It;
			if (!Door->ActorHasTag(Tag) || KGTrapPrivate::IsDoorLocked(Door))
			{
				continue;   // someone else's lock stays theirs
			}
			if (Door->IsOpen())
			{
				IKGInteractable::Execute_Interact(Door, nullptr);   // swings it shut
			}
			Door->FlushNetDormancy();
			Door->SetLocked(true);
			Door->ForceNetUpdate();
			LockedDoors.Add(Door);
			LockedDoorNames.Add(Door->GetName());
		}
		MulticastCue(static_cast<uint8>(ECue::Clunk));
		break;
	}
	case EKGTrapEffect::Alarm:
		FlushNetDormancy();
		bPing = true;
		MulticastCue(static_cast<uint8>(ECue::Alarm));
		break;
	case EKGTrapEffect::LightsOut:   // cosmetic on every machine (TickCosmetics)
	case EKGTrapEffect::Witness:
	case EKGTrapEffect::Custom:
	default: break;
	}
	Record(TEXT("Fired"), LastTrigger.Get());
}

void AKGTrap::OnEnd()
{
	switch (Effect)
	{
	case EKGTrapEffect::LockDoors:
		for (const TWeakObjectPtr<AKGDoor>& D : LockedDoors)
		{
			if (AKGDoor* Door = D.Get())
			{
				Door->FlushNetDormancy();
				Door->SetLocked(false);
				Door->ForceNetUpdate();
			}
		}
		LockedDoors.Reset();
		LockedDoorNames.Reset();
		break;
	case EKGTrapEffect::Alarm:
		FlushNetDormancy();
		bPing = false;
		break;
	default: break;
	}
	Record(TEXT("Ended"), nullptr);
}

void AKGTrap::OnReady()
{
	if (!TrapDef.bPassive)
	{
		ArmedByPuid.Reset();
	}
	LastTrigger = nullptr;
	Record(TEXT("Ready"), nullptr);
}

void AKGTrap::SampleWitnesses(float DeltaSeconds)
{
	WitnessAccum += DeltaSeconds;
	if (WitnessAccum < KGTrapPrivate::WitnessSampleSecs)
	{
		return;
	}
	WitnessAccum = 0.0f;
	UWorld* World = GetWorld();
	const FVector Eye = GetActorLocation() + FVector(0.0, 0.0, 150.0);
	const FVector Fwd = GetActorForwardVector();
	const float Now = KGTrapPrivate::ServerNow(World);
	bool bChanged = false;
	for (TActorIterator<AKGCharacter> It(World); It; ++It)
	{
		AKGCharacter* C = *It;
		if (!IsValid(C) || C->IsDead())
		{
			continue;
		}
		const FVector To = C->GetActorLocation() - GetActorLocation();
		if (To.SizeSquared() > FMath::Square(TrapDef.EffectRadiusCm) || FVector::DotProduct(To.GetSafeNormal(), Fwd) <= KGTrapPrivate::WitnessFacingDot)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGTrapWitness), false, this);
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, C->GetActorLocation() + FVector(0.0, 0.0, 60.0), ECC_Visibility, Params)
		                      && Hit.GetActor() != C;
		if (bBlocked)
		{
			continue;
		}
		const AKGPlayerState* PS = C->GetPlayerState<AKGPlayerState>();
		const FString Puid = PS ? PS->Puid : FString();
		const FString Name = PS ? PS->GetPlayerName() : C->GetName();
		FKGTrapWitness* W = Witnesses.FindByPredicate([&](const FKGTrapWitness& X) { return Puid.IsEmpty() ? X.Name == Name : X.Puid == Puid; });
		if (!W)
		{
			W = &Witnesses.AddDefaulted_GetRef();
			W->Puid = Puid;
			Record(TEXT("Witnessed"), C);
		}
		W->Name = Name;
		W->At = Now;
		bChanged = true;
	}
	if (bChanged)
	{
		Witnesses.Sort([](const FKGTrapWitness& A, const FKGTrapWitness& B) { return A.At > B.At; });
		if (Witnesses.Num() > KGTrapPrivate::MaxWitnesses)
		{
			Witnesses.SetNum(KGTrapPrivate::MaxWitnesses);
		}
		FlushNetDormancy();
		ForceNetUpdate();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Cosmetics (clients + listen server)
// ---------------------------------------------------------------------------------------------------------------------

void AKGTrap::OnRep_State()
{
	// TickCosmetics notices the change (the same path as the listen server).
}

void AKGTrap::MulticastCue_Implementation(uint8 Cue)
{
	PlayCue(static_cast<ECue>(Cue));
}

void AKGTrap::PlayCue(ECue Cue) const
{
	const float Volume = Cue == ECue::Alarm || Cue == ECue::Crash ? 1.0f : 0.8f;
	KGAudio::At(this, KGTrapPrivate::CueSound(static_cast<uint8>(Cue)), GetActorLocation(), Volume);
}

void AKGTrap::CollectLights()
{
	RestoreLights();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Prefer the builder's room tags (KG_Room_<Room> on every light actor); the radius is the fallback.
	const FName Tag(*FString::Printf(TEXT("KG_Room_%s"), *Room.ToString()));
	TArray<ULightComponent*> Found;
	auto Consider = [&](AActor* A)
	{
		TInlineComponentArray<ULightComponent*> Lights(A);
		for (ULightComponent* L : Lights)
		{
			if (L && L->Mobility == EComponentMobility::Movable && L->IsVisible())
			{
				Found.Add(L);
			}
		}
	};
	if (!Room.IsNone())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				Consider(*It);
			}
		}
	}
	if (Found.Num() == 0)
	{
		const FVector Here = GetActorLocation();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FVector D = It->GetActorLocation() - Here;
			if (FMath::Abs(D.Z) < 250.0f && D.SizeSquared2D() <= FMath::Square(TrapDef.EffectRadiusCm))
			{
				Consider(*It);
			}
		}
	}
	for (ULightComponent* L : Found)
	{
		DimmedLights.Add(L);
	}
}

void AKGTrap::RestoreLights()
{
	for (const TWeakObjectPtr<ULightComponent>& L : DimmedLights)
	{
		if (ULightComponent* Light = L.Get())
		{
			Light->SetVisibility(true);
		}
	}
	DimmedLights.Reset();
}

void AKGTrap::OnCosmeticState(EKGTrapState Old, EKGTrapState New)
{
	CosmeticTime = 0.0f;
	if (Effect == EKGTrapEffect::LightsOut)
	{
		if (New == EKGTrapState::Telegraph)
		{
			CollectLights();
		}
		else if (New == EKGTrapState::Active)
		{
			if (DimmedLights.Num() == 0)
			{
				CollectLights();
			}
			for (const TWeakObjectPtr<ULightComponent>& L : DimmedLights)
			{
				if (ULightComponent* Light = L.Get())
				{
					Light->SetVisibility(false);
				}
			}
		}
		else
		{
			RestoreLights();
		}
	}
}

void AKGTrap::TickCosmetics(float DeltaSeconds)
{
	if (!bRestCaptured && Visual)
	{
		VisualRestLoc = Visual->GetRelativeLocation();
		VisualRestRot = Visual->GetRelativeRotation();
		bRestCaptured = true;
	}
	if (CosmeticState != State)
	{
		const EKGTrapState Old = CosmeticState;
		CosmeticState = State;
		OnCosmeticState(Old, State);
	}
	CosmeticTime += DeltaSeconds;
	if (!Visual)
	{
		return;
	}
	switch (Effect)
	{
	case EKGTrapEffect::Trapdoor:
	{
		FRotator Target = VisualRestRot;
		if (State == EKGTrapState::Telegraph)
		{
			Target.Roll += FMath::Sin(CosmeticTime * 40.0f) * 2.5f;
			Visual->SetRelativeRotation(Target);
		}
		else
		{
			if (State == EKGTrapState::Active)
			{
				Target.Pitch -= 85.0f;
			}
			const FRotator Cur = Visual->GetRelativeRotation();
			Visual->SetRelativeRotation(FMath::RInterpConstantTo(Cur, Target, DeltaSeconds, State == EKGTrapState::Active ? 320.0f : 120.0f));
		}
		break;
	}
	case EKGTrapEffect::FallingObject:
	{
		if (DropCm < 0.0f)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KGTrapDrop), false, this);
			const FVector Start = Visual->GetComponentLocation();
			DropCm = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 2000.0), ECC_Visibility, Params)
			             ? FMath::Max(0.0f, static_cast<float>(Hit.Distance) - 40.0f)
			             : 300.0f;
		}
		FRotator Rot = VisualRestRot;
		FVector Loc = Visual->GetRelativeLocation();
		const FVector Dropped = VisualRestLoc - FVector(0.0, 0.0, DropCm);
		if (State == EKGTrapState::Telegraph)
		{
			Rot.Roll += FMath::Sin(CosmeticTime * 6.0f) * 8.0f;
			Loc = VisualRestLoc;
		}
		else if (State == EKGTrapState::Active)
		{
			Loc = FMath::VInterpConstantTo(Loc, Dropped, DeltaSeconds, 900.0f);
		}
		else if (State == EKGTrapState::Cooldown)
		{
			Loc = FMath::VInterpConstantTo(Loc, VisualRestLoc, DeltaSeconds, 60.0f);   // hauled back up
		}
		else
		{
			Loc = VisualRestLoc;
		}
		Visual->SetRelativeLocation(Loc);
		Visual->SetRelativeRotation(FMath::RInterpConstantTo(Visual->GetRelativeRotation(), Rot, DeltaSeconds, 90.0f));
		break;
	}
	case EKGTrapEffect::LightsOut:
		if (State == EKGTrapState::Telegraph)
		{
			int32 i = 0;
			for (const TWeakObjectPtr<ULightComponent>& L : DimmedLights)
			{
				if (ULightComponent* Light = L.Get())
				{
					Light->SetVisibility(FMath::Sin(CosmeticTime * 23.0f + i * 1.7f) > -0.35f);
				}
				++i;
			}
		}
		break;
	default: break;
	}
}

#undef LOCTEXT_NAMESPACE
