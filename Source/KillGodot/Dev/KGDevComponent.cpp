#include "Dev/KGDevComponent.h"
#include "Character/KGCharacter.h"
#include "Character/KGCharacterMovement.h"
#include "Character/KGStamina.h"
#include "Dev/KGDevCommands.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "KillGodot.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

UKGDevComponent::UKGDevComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UKGDevComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKGDevComponent, bMayRun);
	DOREPLIFETIME(UKGDevComponent, bGod);
	DOREPLIFETIME(UKGDevComponent, bFly);
	DOREPLIFETIME(UKGDevComponent, SpeedScale);
}

UKGDevComponent* UKGDevComponent::FindFor(const APlayerController* PC)
{
	return PC ? PC->FindComponentByClass<UKGDevComponent>() : nullptr;
}

APlayerController* UKGDevComponent::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UKGDevComponent::RefillStamina(AKGCharacter* Character)
{
	// AKGCharacter::Stamina is a protected UPROPERTY: reach it through reflection instead of widening its API.
	const FStructProperty* Prop = Character ? CastField<FStructProperty>(
		                                          AKGCharacter::StaticClass()->FindPropertyByName(TEXT("Stamina")))
	                                        : nullptr;
	if (!Prop || Prop->Struct != FKGStamina::StaticStruct())
	{
		return false;
	}
	FKGStamina* Stamina = Prop->ContainerPtrToValuePtr<FKGStamina>(Character);
	Stamina->Current = Stamina->Max;
	Stamina->bExhausted = false;
	Stamina->RegenCooldown = 0.0f;
	return true;
}

void UKGDevComponent::ServerRun_Implementation(const FString& Line)
{
#if !UE_BUILD_SHIPPING
	APlayerController* PC = GetPC();
	if (!FKGDev::MayRun(PC))
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Dev: refused '%s' from %s (host only; kg.Dev.AllowClients 1 allows clients)"),
		       *Line, PC && PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("?"));
		ClientReport(false, TEXT("Refused: only the host can run this (kg.Dev.AllowClients 1)"));
		return;
	}
	if (!FKGDev::IsServerVerb(Line))
	{
		ClientReport(false, FString::Printf(TEXT("'%s' is not a server verb"), *Line));
		return;
	}
	const FKGDevResult Result = FKGDev::Execute({GetWorld(), PC}, Line);
	UE_LOG(LogKillGodot, Log, TEXT("Dev[%s] %s -> %s"), PC && PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("?"),
	       *Line, *Result.Message);
	ClientReport(Result.bOk, Result.Message);
#else
	ClientReport(false, TEXT("Dev commands are not available in Shipping"));
#endif
}

void UKGDevComponent::ClientReport_Implementation(bool bOk, const FString& Message)
{
#if !UE_BUILD_SHIPPING
	FKGDev::Report(GetWorld(), bOk ? FKGDevResult::Ok(Message) : FKGDevResult::Fail(Message));
#endif
}

void UKGDevComponent::ClientRefillStamina_Implementation()
{
	const APlayerController* PC = GetPC();
	RefillStamina(PC ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr);
}

void UKGDevComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if !UE_BUILD_SHIPPING
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	const bool bAuthority = GetOwner()->HasAuthority();
	if (bAuthority)
	{
		const bool bNow = FKGDev::MayRun(PC);
		if (bMayRun != bNow)
		{
			bMayRun = bNow;
		}
	}

	AKGCharacter* Character = Cast<AKGCharacter>(PC->GetPawn());
	if (!Character)
	{
		AppliedPawn = nullptr;
		return;
	}
	if (AppliedPawn.Get() != Character)
	{
		// New body (respawn, revive): everything is applied again.
		AppliedPawn = Character;
		bAppliedGod = false;
		bAppliedFly = false;
	}
	UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(Character->GetCharacterMovement());

	// God mode: APawn::ShouldTakeDamage refuses everything while CanBeDamaged is off (server decides damage).
	if (bAuthority && bAppliedGod != bGod)
	{
		Character->SetCanBeDamaged(!bGod);
		bAppliedGod = bGod;
	}
	if (Move)
	{
		Move->DevSpeedScale = FMath::Clamp(SpeedScale, 0.1f, 10.0f);
	}

	const bool bWantFly = bFly && !Character->IsDead() && Move;
	if (bWantFly)
	{
		if (!bAppliedFly)
		{
			SavedMaxFlySpeed = Move->MaxFlySpeed;
			SavedFlyBraking = Move->BrakingDecelerationFlying;
			Move->MaxFlySpeed = 1400.0f;
			Move->BrakingDecelerationFlying = 4000.0f;
			Character->SetActorEnableCollision(false);
			bAppliedFly = true;
		}
		if (Move->MovementMode != MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Flying);
		}
		if (PC->IsLocalController())
		{
			// The pawn's move input is yaw-only: add the vertical axis here.
			const float Up = (PC->IsInputKeyDown(EKeys::SpaceBar) ? 1.0f : 0.0f) -
			                 (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::C) ? 1.0f : 0.0f);
			if (Up != 0.0f)
			{
				Character->AddMovementInput(FVector::UpVector, Up);
			}
		}
	}
	else if (bAppliedFly)
	{
		if (Move)
		{
			Move->MaxFlySpeed = SavedMaxFlySpeed;
			Move->BrakingDecelerationFlying = SavedFlyBraking;
			if (Move->MovementMode == MOVE_Flying)
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
		if (!Character->IsDead())
		{
			Character->SetActorEnableCollision(true);
		}
		bAppliedFly = false;
	}
#endif
}
