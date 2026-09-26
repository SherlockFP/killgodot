#pragma once

// Dev verbs for the SPRINT-041 role abilities and the Trapper (kg.Ability.*, kg.Trapper.*). Header-only, registered
// through the SPRINT-040 trap hook (KGTrapDev::AddVerbs calls KGAbilityDev::AddVerbs), so Dev/ stays untouched.

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilitySubsystem.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGTrapperBot.h"
#include "Abilities/KGWoundComponent.h"
#include "AI/KGBotController.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameMode.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Traps/KGFieldTraps.h"
#include "Traps/KGMimicTrap.h"
#include "World/KGStorageChest.h"

namespace KGAbilityDev
{
	inline AKGCharacter* BodyOf(const FKGDevContext& C)
	{
		return C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
	}

	/** A point on the ground Dist cm in front of Body, SideCm to its right. */
	inline FVector InFront(UWorld* World, const AKGCharacter* Body, float Dist, float SideCm = 0.0f)
	{
		const FRotator Yaw(0.0, Body->GetControlRotation().Yaw, 0.0);
		const FVector P = Body->GetActorLocation() + Yaw.Vector() * Dist + FRotationMatrix(Yaw).GetScaledAxis(EAxis::Y) * SideCm;
		// A short trace down from chest height (GroundAt may pick a roof for a spot under an awning).
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KGStageGround), false, Body);
		if (World->LineTraceSingleByChannel(Hit, P + FVector(0.0, 0.0, 60.0), P - FVector(0.0, 0.0, 400.0), ECC_Visibility, Q))
		{
			return Hit.ImpactPoint;
		}
		return P - FVector(0.0, 0.0, 90.0);
	}

	/** A frozen bot standing at Where (for staged shots). */
	inline AKGCharacter* StageBot(UWorld* World, const FVector& Where, const FVector& LookAt)
	{
		AKGGameMode* GM = World->GetAuthGameMode<AKGGameMode>();
		if (!GM || GM->DevAddBots(1) <= 0)
		{
			return nullptr;
		}
		AKGCharacter* Newest = nullptr;
		for (TActorIterator<AKGBotController> It(World); It; ++It)
		{
			if (AKGCharacter* B = Cast<AKGCharacter>(It->GetPawn()))
			{
				Newest = B;   // the iterator returns spawn order: the last one is the new bot
			}
		}
		if (Newest)
		{
			Newest->TeleportTo(Where + FVector(0.0, 0.0, 95.0), (LookAt - Where).GetSafeNormal2D().Rotation());
			if (AController* C = Newest->GetController())
			{
				C->SetControlRotation((LookAt - Where).GetSafeNormal2D().Rotation());
			}
		}
		return Newest;
	}

	template <typename FAdd>
	void AddVerbs(FAdd&& Add)
	{
		constexpr EKGDevScope Server = EKGDevScope::Server;
		constexpr EKGDevScope Local = EKGDevScope::Local;
		Add(TEXT("Ability.List"), TEXT(""), TEXT("Log every ability holder (role, charges, cooldowns, traps)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    int32 N = 0;
			    if (UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(C.World))
			    {
				    S->SyncHolders();
				    for (const TWeakObjectPtr<AKGAbilityHolder>& H : S->GetHolders())
				    {
					    if (H.IsValid())
					    {
						    UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY_LIST %s %s"), *GetNameSafe(H->GetBody()), *H->Describe());
						    ++N;
					    }
				    }
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d holder(s) (see log)"), N));
		    });
		Add(TEXT("Ability.Use"), TEXT("<Mimic|Snare|Tripwire>"), TEXT("Use one of your role's abilities where you look (server-validated)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Body = BodyOf(C);
			    if (UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(C.World))
			    {
				    S->SyncHolders();
			    }
			    AKGAbilityHolder* H = AKGAbilityHolder::FindFor(C.Requester);
			    if (!Body || !H || A.Num() == 0)
			    {
				    return FKGDevResult::Fail(TEXT("Usage: Ability.Use <Id> (your role needs abilities: Me.Role Trapper)"));
			    }
			    FString Detail;
			    const EKGAbilityDeny V = H->AuthUse(FName(*A[0]), Body->GetPawnViewLocation(), Body->GetControlRotation().Vector(), &Detail);
			    return V == EKGAbilityDeny::None ? FKGDevResult::Ok(Detail) : FKGDevResult::Fail(FKGAbilityRules::DenyName(V));
		    });
		Add(TEXT("Ability.Aim"), TEXT("<Id|off>"), TEXT("Open (or close) the targeting prompt for one of your abilities."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGAbilityHolder* H = AKGAbilityHolder::FindLocal(C.World);
			    if (!H)
			    {
				    return FKGDevResult::Fail(TEXT("No abilities (Me.Role Trapper)"));
			    }
			    H->AimingAbility = A.Num() == 0 || A[0].Equals(TEXT("off"), ESearchCase::IgnoreCase) ? NAME_None : FName(*A[0]);
			    return FKGDevResult::Ok(H->AimingAbility.ToString());
		    });
		Add(TEXT("Ability.Refill"), TEXT(""), TEXT("Dawn sweep now: charges back, Trapper traps expire."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(C.World);
			    if (!S)
			    {
				    return FKGDevResult::Fail(TEXT("No ability subsystem"));
			    }
			    S->OnDawn();
			    return FKGDevResult::Ok(TEXT("Refilled"));
		    });
		Add(TEXT("Trapper.Stats"), TEXT(""), TEXT("Trapper bot counters: arms, rummages, bites, avoided mimics."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const KGTrapperBot::FStats& S = KGTrapperBot::Stats();
			    const FString Line = FString::Printf(TEXT("arms=%d rummages=%d bitten=%d avoided=%d"), S.Arms, S.Rummages, S.Bitten, S.Avoided);
			    UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_STATS %s"), *Line);
			    return FKGDevResult::Ok(Line);
		    });
		Add(TEXT("Trapper.Stage"), TEXT("<rest|bite|marks|flinch|snare|ui>"), TEXT("Stage a Trapper scene in front of you (shots)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Body = BodyOf(C);
			    const FString What = A.Num() > 0 ? A[0].ToLower() : TEXT("rest");
			    if (!Body)
			    {
				    return FKGDevResult::Fail(TEXT("No body"));
			    }
			    UWorld* W = C.World;
			    FActorSpawnParameters P;
			    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			    const float Yaw = Body->GetControlRotation().Yaw;
			    if (What == TEXT("snare"))
			    {
				    const FVector Open = InFront(W, Body, 170.0f, -45.0f);
				    AKGSnareTrap* A1 = AKGFieldTrap::AuthPlace<AKGSnareTrap>(W, Open, Yaw + 20.0f, Body);
				    if (A1)
				    {
					    A1->bShowToLocalOwner = true;
				    }
				    const FVector Caught = InFront(W, Body, 300.0f, 110.0f);
				    AKGSnareTrap* A2 = AKGFieldTrap::AuthPlace<AKGSnareTrap>(W, Caught, Yaw - 30.0f, nullptr);
				    StageBot(W, Caught, Body->GetActorLocation());   // steps right into it
				    return FKGDevResult::Ok(FString::Printf(TEXT("snares %s %s"), A1 ? *A1->TrapId.ToString() : TEXT("-"), A2 ? *A2->TrapId.ToString() : TEXT("-")));
			    }
			    // Container scenes: a chest 2.4 m ahead, facing the camera.
			    const FVector Spot = InFront(W, Body, 240.0f);
			    AKGStorageChest* Chest = W->SpawnActor<AKGStorageChest>(Spot, FRotator(0.0f, Yaw + 180.0f, 0.0f), P);
			    if (!Chest)
			    {
				    return FKGDevResult::Fail(TEXT("No chest"));
			    }
			    if (What == TEXT("ui"))
			    {
				    if (AKGPlayerState* PS = Body->GetPlayerState<AKGPlayerState>())
				    {
					    PS->SetPrivateRoleId(TEXT("Trapper"));
				    }
				    if (UKGAbilitySubsystem* S = UKGAbilitySubsystem::Get(W))
				    {
					    S->SyncHolders();
				    }
				    if (AKGAbilityHolder* H = AKGAbilityHolder::FindFor(C.Requester))
				    {
					    H->AimingAbility = TEXT("Mimic");
				    }
				    return FKGDevResult::Ok(TEXT("ui staged (Trapper, aiming Mimic at a chest)"));
			    }
			    AKGMimicTrap* M = AKGMimicTrap::AuthArmOn(Chest, Body);
			    if (!M)
			    {
				    return FKGDevResult::Fail(TEXT("Could not arm"));
			    }
			    if (What == TEXT("bite") || What == TEXT("marks"))
			    {
				    // Beside the chest (not between it and the camera), facing it.
				    const FVector Side = FRotationMatrix(FRotator(0.0f, Yaw, 0.0f)).GetScaledAxis(EAxis::Y);
				    AKGCharacter* Bot = StageBot(W, Spot + Side * 115.0f - FRotator(0.0f, Yaw, 0.0f).Vector() * 30.0f, Chest->GetActorLocation());
				    if (Bot)
				    {
					    M->AuthBite(Bot);
				    }
			    }
			    else if (What == TEXT("flinch"))
			    {
				    M->AuthFlinch();
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s staged (%s)"), *What, *M->TrapId.ToString()));
		    });
	}
}

#endif
