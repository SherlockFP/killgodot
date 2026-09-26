#include "Traps/KGFieldTraps.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilitySubsystem.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGWoundComponent.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Combat/KGHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"

namespace KGFieldTrapPrivate
{
	int32 NextId = 0;

	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f);
	}

	UStaticMesh* LoadMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KillGodot/Env/Dress/KG_Mimic_Clean/StaticMeshes/SM_KG_%s.SM_KG_%s"), Name, Name),
		                               nullptr, LOAD_Quiet | LOAD_NoWarn);
	}

	FString NameOf(const AKGCharacter* C)
	{
		const APlayerState* PS = C ? C->GetPlayerState() : nullptr;
		return PS ? PS->GetPlayerName() : GetNameSafe(C);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// AKGFieldTrap
// ---------------------------------------------------------------------------------------------------------------------

AKGFieldTrap::AKGFieldTrap()
{
	Effect = EKGTrapEffect::Custom;
	TrapDef.ArmPolicy = TEXT("None");
	TrapDef.ArmerCooldownSecs = 0.0f;
	TrapDef.bIgnoreArmer = true;
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->CastShadow = false;
	Visual->SetVisibility(false);
	SetNetCullDistanceSquared(FMath::Square(9000.0f));
}

void AKGFieldTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGFieldTrap, ArmedAt);
	DOREPLIFETIME(AKGFieldTrap, bSprung);
	DOREPLIFETIME(AKGFieldTrap, Victim);
}

void AKGFieldTrap::EndPlay(const EEndPlayReason::Type Reason)
{
	ApplyHold(false);
	Super::EndPlay(Reason);
}

void AKGFieldTrap::AuthSetup(AKGCharacter* By)
{
	TrapId = FName(*FString::Printf(TEXT("%s_%d"), *TrapDef.Kind.ToString(), ++KGFieldTrapPrivate::NextId));
	Room = UKGAbilitySubsystem::PlaceName(GetWorld(), GetActorLocation());
	ArmedAt = KGFieldTrapPrivate::ServerNow(GetWorld());
	if (!TrapDef.bPassive)
	{
		AuthArm(By, true);
	}
	if (AKGAbilityHolder* H = By ? AKGAbilityHolder::FindFor(By->GetController()) : nullptr)
	{
		H->AuthAddTrap(this);
	}
	ForceNetUpdate();
}

bool AKGFieldTrap::IsShownLocally() const
{
	if (bSprung || bShowToLocalOwner)
	{
		return true;
	}
	return KGFieldTrapPrivate::ServerNow(GetWorld()) - ArmedAt < KGTrapperTuning::RevealSecs;
}

void AKGFieldTrap::ApplyHold(bool bWant)
{
	AKGCharacter* Want = bWant && Victim && !Victim->IsDead() ? Victim.Get() : nullptr;
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

void AKGFieldTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UStaticMesh* Want = PickMesh();
	if (Want != LastMesh)
	{
		LastMesh = Want;
		if (Want)
		{
			Visual->SetStaticMesh(Want);
			Visual->SetRelativeScale3D(FVector::OneVector);
		}
		else if (!Visual->GetStaticMesh())
		{
			// Before the Blender pack is imported: a flat disc stands in.
			Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Visual->SetRelativeScale3D(FVector(0.5, 0.5, 0.06));
		}
	}
	const bool bShow = IsShownLocally();
	if (Visual->IsVisible() != bShow)
	{
		Visual->SetVisibility(bShow);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Snare
// ---------------------------------------------------------------------------------------------------------------------

AKGSnareTrap::AKGSnareTrap()
{
	TrapDef.Kind = TEXT("Snare");
	TrapDef.TelegraphSecs = 0.0f;
	TrapDef.ActiveSecs = KGTrapperTuning::SnareHoldSecs;
	TrapDef.CooldownSecs = 1.0f;
	TrapDef.Damage = KGTrapperTuning::SnareDamage;
	ZoneExtent = FVector(38.0, 38.0, 75.0);   // the capsule centre stands ~90 cm above the ground
	ZoneOffset = FVector(0.0, 0.0, 85.0);
}

UStaticMesh* AKGSnareTrap::PickMesh() const
{
	return KGFieldTrapPrivate::LoadMesh(bSprung ? TEXT("SnareShut") : TEXT("SnareOpen"));
}

void AKGSnareTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyHold(State == EKGTrapState::Active);
}

void AKGSnareTrap::OnFire()
{
	AKGCharacter* V = LastTrigger.Get();
	FlushNetDormancy();
	Victim = V;
	bSprung = true;
	if (V && !V->IsDead())
	{
		if (UKGHealthComponent* H = V->GetHealth())
		{
			const float Amount = FMath::Min(TrapDef.Damage, H->GetHealth() - KGTrapperTuning::SnareMinHealthLeft);
			if (Amount > 0.0f)
			{
				H->ApplyDamage(Amount, this, TEXT("Snare"));
			}
		}
		UKGWoundComponent::AuthAdd(V, UKGWoundComponent::SnareWound);
	}
	MulticastCue(static_cast<uint8>(ECue::Crash));   // the jaws slam shut
	Record(TEXT("Snared"), V);
	if (UKGAbilitySubsystem* A = UKGAbilitySubsystem::Get(GetWorld()))
	{
		A->NotifyArmer(ArmedByPuid, FString::Printf(TEXT("%s: %s"), *FKGAbilityCatalog::DisplayName(TEXT("Snare"), false),
		                                            *KGFieldTrapPrivate::NameOf(V)));
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_SNARE caught %s victim=%s health=%.0f wounds=%s"), *TrapId.ToString(), *GetNameSafe(V),
	       V && V->GetHealth() ? V->GetHealth()->GetHealth() : -1.0f, *UKGWoundComponent::ListOn(V));
	ForceNetUpdate();
}

void AKGSnareTrap::OnEnd()
{
	Record(TEXT("Ended"), nullptr);
}

void AKGSnareTrap::OnReady()
{
	Super::OnReady();   // forgets the armer
	Victim = nullptr;   // stays sprung (visible evidence) until dawn clears it
}

// ---------------------------------------------------------------------------------------------------------------------
// Tripwire
// ---------------------------------------------------------------------------------------------------------------------

AKGTripwireTrap::AKGTripwireTrap()
{
	TrapDef.Kind = TEXT("Tripwire");
	TrapDef.bPassive = true;   // always armed; re-arms after each trip
	TrapDef.TelegraphSecs = 0.0f;
	TrapDef.ActiveSecs = 0.1f;
	TrapDef.CooldownSecs = KGTrapperTuning::TripwireRearmSecs;
	ZoneExtent = FVector(18.0, 150.0, 75.0);   // the wire runs along local Y (3 m); the capsule centre is ~90 cm up
	ZoneOffset = FVector(0.0, 0.0, 85.0);
}

UStaticMesh* AKGTripwireTrap::PickMesh() const
{
	return KGFieldTrapPrivate::LoadMesh(TEXT("Tripwire"));
}

void AKGTripwireTrap::AuthSetup(AKGCharacter* By)
{
	Super::AuthSetup(By);
	const AKGPlayerState* PS = By ? By->GetPlayerState<AKGPlayerState>() : nullptr;
	ArmedByPuid = PS ? PS->Puid : FString();
	Record(TEXT("Armed"), By);
}

void AKGTripwireTrap::OnFire()
{
	AKGCharacter* V = LastTrigger.Get();
	++Trips;
	const FString Where = Room.IsNone() ? GetActorLocation().ToCompactString() : Room.ToString();
	Record(TEXT("Tripped"), V);
	if (UKGAbilitySubsystem* A = UKGAbilitySubsystem::Get(GetWorld()))
	{
		A->NotifyArmer(ArmedByPuid, FString::Printf(TEXT("%s: %s passed %s"), *FKGAbilityCatalog::DisplayName(TEXT("Tripwire"), false),
		                                            *KGFieldTrapPrivate::NameOf(V), *Where));
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_TRIPWIRE %s crossed by %s at %s (trips %d)"), *TrapId.ToString(), *GetNameSafe(V), *Where, Trips);
}
