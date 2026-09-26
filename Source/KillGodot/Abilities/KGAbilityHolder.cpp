#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilitySubsystem.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "Traps/KGFieldTraps.h"
#include "Traps/KGMimicTrap.h"
#include "UI/Reveal/KGRoleCardText.h"
#include "World/KGBreakable.h"
#include "World/KGStorageChest.h"

namespace KGAbilityHolderPrivate
{
	/** The container the view ray points at (bounds-based: independent of the container's collision setup). */
	AActor* FindContainer(UWorld* World, const FVector& Eyes, const FVector& Dir, float Range, float& OutDist)
	{
		AActor* Best = nullptr;
		double BestT = TNumericLimits<double>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!AKGMimicTrap::IsContainer(A))
			{
				continue;
			}
			const FBox B = A->GetComponentsBoundingBox(true);
			if (!B.IsValid)
			{
				continue;
			}
			const FVector C = B.GetCenter();
			const double T = FVector::DotProduct(C - Eyes, Dir);
			if (T <= 0.0 || T > Range + 150.0)
			{
				continue;
			}
			const double Off = FVector::Dist(Eyes + Dir * T, C);
			const double Allow = FMath::Max(55.0, B.GetExtent().Size2D() * 0.9);
			if (Off <= Allow && T < BestT)
			{
				BestT = T;
				Best = A;
			}
		}
		if (Best)
		{
			const FBox B = Best->GetComponentsBoundingBox(true);
			OutDist = FMath::Sqrt(B.ComputeSquaredDistanceToPoint(Eyes));
		}
		return Best;
	}

	/** The standing spot the view ray points at (ground-projected). */
	bool FindGround(UWorld* World, const AActor* Ignore, const FVector& Eyes, const FVector& Dir, float Range, FVector& OutPoint)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGAbilityGround), false, Ignore);
		FHitResult Hit;
		FVector Probe = Eyes + Dir * Range;
		if (World->LineTraceSingleByChannel(Hit, Eyes, Probe, ECC_Visibility, Params))
		{
			if (Hit.ImpactNormal.Z > 0.5)
			{
				OutPoint = Hit.ImpactPoint;
				return true;
			}
			Probe = Hit.ImpactPoint - Dir * 30.0;   // a wall: drop in front of it
		}
		if (World->LineTraceSingleByChannel(Hit, Probe + FVector(0.0, 0.0, 60.0), Probe - FVector(0.0, 0.0, 400.0), ECC_Visibility, Params) &&
		    Hit.ImpactNormal.Z > 0.5)
		{
			OutPoint = Hit.ImpactPoint;
			return true;
		}
		return false;
	}
}

AKGAbilityHolder::AKGAbilityHolder()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;   // nobody else ever receives it: its existence would already hint at the role
	bAlwaysRelevant = false;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(4.0f);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AKGAbilityHolder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AKGAbilityHolder, RoleId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AKGAbilityHolder, States, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AKGAbilityHolder, MyTraps, COND_OwnerOnly);
}

void AKGAbilityHolder::BeginPlay()
{
	Super::BeginPlay();
	if (UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(GetWorld()))
	{
		S->RegisterHolder(this);
	}
}

void AKGAbilityHolder::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(GetWorld()))
	{
		S->UnregisterHolder(this);
	}
	Super::EndPlay(Reason);
}

AActor* AKGAbilityHolder::PickContainer(UWorld* World, const FVector& Eyes, const FVector& Dir, float RangeCm, float& OutDistCm)
{
	return World ? KGAbilityHolderPrivate::FindContainer(World, Eyes, Dir.GetSafeNormal(), RangeCm, OutDistCm) : nullptr;
}

bool AKGAbilityHolder::PickGround(UWorld* World, const AActor* Ignore, const FVector& Eyes, const FVector& Dir, float RangeCm, FVector& OutPoint)
{
	return World && KGAbilityHolderPrivate::FindGround(World, Ignore, Eyes, Dir.GetSafeNormal(), RangeCm, OutPoint);
}

AKGAbilityHolder* AKGAbilityHolder::FindFor(const AController* Controller)
{
	const UKGAbilitySubsystem* S = Controller ? UKGAbilitySubsystem::Get(Controller->GetWorld()) : nullptr;
	if (!S)
	{
		return nullptr;
	}
	for (const TWeakObjectPtr<AKGAbilityHolder>& H : S->GetHolders())
	{
		if (H.IsValid() && H->GetOwner() == Controller && !H->IsActorBeingDestroyed())
		{
			return H.Get();
		}
	}
	return nullptr;
}

AKGAbilityHolder* AKGAbilityHolder::FindLocal(const UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? FindFor(PC) : nullptr;
}

AKGCharacter* AKGAbilityHolder::GetBody() const
{
	const AController* C = Cast<AController>(GetOwner());
	return C ? Cast<AKGCharacter>(C->GetPawn()) : nullptr;
}

const FKGAbilityState* AKGAbilityHolder::GetState(FName AbilityId) const
{
	return States.FindByPredicate([AbilityId](const FKGAbilityState& S) { return S.AbilityId == AbilityId; });
}

FString AKGAbilityHolder::Describe() const
{
	FString Out = FString::Printf(TEXT("role=%s"), *RoleId.ToString());
	for (const FKGAbilityState& S : States)
	{
		const FKGAbilityDef* D = FKGAbilityCatalog::Find(S.AbilityId);
		Out += FString::Printf(TEXT(" %s %d/%d cd=%.1f uses=%d"), *S.AbilityId.ToString(), S.Charges, D ? D->ChargesPerCycle : 0,
		                       S.Cooldown.RemainingSeconds, S.Uses);
	}
	return Out + FString::Printf(TEXT(" traps=%d"), MyTraps.Num());
}

// ---------------------------------------------------------------------------------------------------------------------
// Authority
// ---------------------------------------------------------------------------------------------------------------------

void AKGAbilityHolder::AuthSetup(FName InRoleId)
{
	if (!HasAuthority())
	{
		return;
	}
	RoleId = InRoleId;
	// Traps remember their armer by PUID (never pointers). Without an EOS login (PIE, -game smokes, bots) the PUID is
	// empty: give a dev one, like KGPlayerExtrasSubsystem does for chest ownership.
	if (const AController* C = Cast<AController>(GetOwner()))
	{
		if (AKGPlayerState* PS = C->GetPlayerState<AKGPlayerState>(); PS && PS->Puid.IsEmpty())
		{
			PS->Puid = FString::Printf(TEXT("DEV-%d"), PS->GetPlayerId());
			PS->ForceNetUpdate();
		}
	}
	States.Reset();
	for (const FKGAbilityDef* D : FKGAbilityCatalog::ForRole(RoleId))
	{
		FKGAbilityState& S = States.AddDefaulted_GetRef();
		FKGAbilityRules::Refill(*D, S);
	}
	MyTraps.Reset();
	ForceNetUpdate();
}

void AKGAbilityHolder::AuthRefill()
{
	if (!HasAuthority())
	{
		return;
	}
	for (FKGAbilityState& S : States)
	{
		if (const FKGAbilityDef* D = FKGAbilityCatalog::Find(S.AbilityId))
		{
			FKGAbilityRules::Refill(*D, S);
		}
	}
	MyTraps.RemoveAll([](const TObjectPtr<AKGTrap>& T) { return !IsValid(T) || T->IsActorBeingDestroyed(); });
	ForceNetUpdate();
}

void AKGAbilityHolder::AuthAddTrap(AKGTrap* Trap)
{
	if (HasAuthority() && Trap)
	{
		MyTraps.RemoveAll([](const TObjectPtr<AKGTrap>& T) { return !IsValid(T) || T->IsActorBeingDestroyed(); });
		MyTraps.AddUnique(Trap);
		ForceNetUpdate();
	}
}

bool AKGAbilityHolder::IsSeenByOthers(const AKGCharacter* Who, float RadiusCm)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	const FVector MyEyes = Who->GetPawnViewLocation();
	for (TActorIterator<AKGCharacter> It(World); It; ++It)
	{
		const AKGCharacter* O = *It;
		if (O == Who || O->IsDead())
		{
			continue;
		}
		const FVector Eyes = O->GetPawnViewLocation();
		if (FVector::DistSquared(Eyes, MyEyes) > FMath::Square(RadiusCm))
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGAbilitySeen), false, O);
		Params.AddIgnoredActor(Who);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Eyes, MyEyes, ECC_Visibility, Params))
		{
			return true;   // a clear line of sight: they could turn their head any moment
		}
	}
	return false;
}

EKGAbilityDeny AKGAbilityHolder::AuthUse(FName AbilityId, const FVector& InViewStart, const FVector& InViewDir, FString* OutDetail)
{
	if (!HasAuthority())
	{
		return EKGAbilityDeny::Unknown;
	}
	UWorld* World = GetWorld();
	const FKGAbilityDef* Def = FKGAbilityCatalog::Find(AbilityId);
	FKGAbilityState* State = States.FindByPredicate([AbilityId](const FKGAbilityState& S) { return S.AbilityId == AbilityId; });
	AKGCharacter* Body = GetBody();
	const AKGPlayerState* PS = Body ? Body->GetPlayerState<AKGPlayerState>() : nullptr;
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	FString Detail;
	EKGAbilityDeny Verdict = EKGAbilityDeny::Unknown;
	AActor* TargetActor = nullptr;
	FVector Point = FVector::ZeroVector;
	FVector ViewDir = InViewDir.GetSafeNormal();
	if (Def && State && Body && GS)
	{
		// Never trust the client's eyes further than a head's width (the server re-measures everything else).
		FVector Eyes = InViewStart;
		if (FVector::Dist(Eyes, Body->GetPawnViewLocation()) > 200.0)
		{
			Eyes = Body->GetPawnViewLocation();
		}
		if (ViewDir.IsNearlyZero())
		{
			ViewDir = Body->GetControlRotation().Vector();
		}
		FKGAbilityQuery Q;
		Q.Phase = GS->GetPhase();
		Q.bAlive = !Body->IsDead() && (!PS || PS->IsAlive());
		switch (Def->Target)
		{
		case EKGAbilityTarget::Container:
		{
			float Dist = 0.0f;
			TargetActor = KGAbilityHolderPrivate::FindContainer(World, Eyes, ViewDir, Def->RangeCm, Dist);
			if (TargetActor)
			{
				const AKGMimicTrap* Existing = AKGMimicTrap::FindOn(TargetActor);
				Q.bHasTarget = !Existing || Existing->GetState() == EKGTrapState::Idle;
				Q.TargetDistanceCm = Dist;
			}
			break;
		}
		case EKGAbilityTarget::Ground:
			Q.bHasTarget = KGAbilityHolderPrivate::FindGround(World, Body, Eyes, ViewDir, Def->RangeCm, Point);
			Q.TargetDistanceCm = Q.bHasTarget ? FVector::Dist2D(Point, Body->GetActorLocation()) : 0.0f;
			break;
		default:
			Q.bHasTarget = true;
			break;
		}
		Q.bSeen = Def->bRequiresUnseen && IsSeenByOthers(Body, Def->UnseenRadiusCm);
		Verdict = FKGAbilityRules::Validate(*Def, *State, Q);
		if (Verdict == EKGAbilityDeny::None)
		{
			AKGTrap* Made = nullptr;
			if (AbilityId == TEXT("Mimic"))
			{
				Made = AKGMimicTrap::AuthArmOn(TargetActor, Body);
			}
			else if (AbilityId == TEXT("Snare"))
			{
				Made = AKGFieldTrap::AuthPlace<AKGSnareTrap>(World, Point, ViewDir.Rotation().Yaw, Body);
			}
			else if (AbilityId == TEXT("Tripwire"))
			{
				Made = AKGFieldTrap::AuthPlace<AKGTripwireTrap>(World, Point, ViewDir.Rotation().Yaw, Body);
			}
			if (!Made)
			{
				Verdict = EKGAbilityDeny::NoTarget;
			}
			else
			{
				AuthAddTrap(Made);
				FKGAbilityRules::Commit(*Def, *State);
				Detail = FString::Printf(TEXT("trap=%s at=%s"), *Made->TrapId.ToString(), *Made->GetActorLocation().ToCompactString());
				ForceNetUpdate();
			}
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY use %s by %s -> %s %s"), *AbilityId.ToString(), PS ? *PS->GetPlayerName() : *GetNameSafe(Body),
	       FKGAbilityRules::DenyName(Verdict), *Detail);
	if (OutDetail)
	{
		*OutDetail = Detail;
	}
	if (Cast<APlayerController>(GetOwner()))
	{
		ClientFeedback(AbilityId, Verdict, Detail);
	}
	return Verdict;
}

void AKGAbilityHolder::AuthNotify(const FString& Text)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Cast<APlayerController>(GetOwner()))
	{
		ClientNotify(Text);
	}
	else
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY_NOTE (bot %s) %s"), *GetNameSafe(GetBody()), *Text);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Owner
// ---------------------------------------------------------------------------------------------------------------------

void AKGAbilityHolder::RequestUse(FName AbilityId)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	FVector Loc;
	FRotator Rot;
	PC->GetPlayerViewPoint(Loc, Rot);
	ServerUse(AbilityId, Loc, Rot.Vector());
}

void AKGAbilityHolder::ServerUse_Implementation(FName AbilityId, FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir)
{
	AuthUse(AbilityId, ViewStart, ViewDir);
}

void AKGAbilityHolder::ClientNotify_Implementation(const FString& Text)
{
	FKGAbilityNote& N = Notes.AddDefaulted_GetRef();
	N.Text = Text;
	N.ShownAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Notes.Num() > 5)
	{
		Notes.RemoveAt(0);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY_NOTE %s"), *Text);
}

void AKGAbilityHolder::ClientFeedback_Implementation(FName AbilityId, EKGAbilityDeny Verdict, const FString& Detail)
{
	const bool bTr = KGRoleCard::IsTurkish();
	LastFeedback = Verdict == EKGAbilityDeny::None
		? FString::Printf(TEXT("%s %s"), *FKGAbilityCatalog::DisplayName(AbilityId, bTr), bTr ? TEXT("kuruldu.") : TEXT("set."))
		: FString::Printf(TEXT("%s: %s"), *FKGAbilityCatalog::DisplayName(AbilityId, bTr), *FKGAbilityRules::DenyText(Verdict, bTr));
	LastFeedbackAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Verdict == EKGAbilityDeny::None)
	{
		AimingAbility = NAME_None;
	}
}

void AKGAbilityHolder::OnRep_MyTraps()
{
	for (AKGTrap* T : MyTraps)
	{
		if (AKGFieldTrap* F = Cast<AKGFieldTrap>(T))
		{
			F->bShowToLocalOwner = true;
		}
	}
}

void AKGAbilityHolder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		for (FKGAbilityState& S : States)
		{
			S.Cooldown.Advance(DeltaSeconds);
		}
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->IsLocalController())
	{
		OnRep_MyTraps();   // the listen host gets no OnRep for its own holder
		TickInput(DeltaSeconds);
	}
}

void AKGAbilityHolder::TickInput(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three};
	const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	for (int32 i = 0; i < States.Num() && i < UE_ARRAY_COUNT(Digits); ++i)
	{
		if (bAlt && PC->WasInputKeyJustPressed(Digits[i]))
		{
			const FName Id = States[i].AbilityId;
			if (AimingAbility == Id)
			{
				RequestUse(Id);   // second press: use it
			}
			else
			{
				AimingAbility = Id;
			}
		}
	}
	if (!AimingAbility.IsNone() && PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		AimingAbility = NAME_None;
	}
}
