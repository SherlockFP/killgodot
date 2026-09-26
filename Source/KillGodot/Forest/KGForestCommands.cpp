// SPRINT-033/034 console commands (dev + smokes + offscreen shots). Registered here, not in Dev/KGDevCommands.cpp.
//   kg.Forest.Enabled 0|1                         the forest threat layer (cvar, KGForestSubsystem.cpp)
//   kg.Forest.Status                              bands / stages / caps / lethality
//   kg.Forest.Test wolf|mist|stop|fire|logs|vigil [me|remote|bot|<name>] [stay]   [server] force a wolf approach (Deep-night
//                                                 gain, teleports to the North deep wood unless "stay"), a Mist tongue, ...
//   kg.Forest.Goto deep|camp|den [sector 1-4]     [host] teleports the local player
//   kg.Forest.AutoWalk 0|1                        [client] walk along the arrow to the nearest path (smoke: escaping by walking)
//   kg.Forest.Cam wolf|mist|camp|me|pov:wolf|pov:mist|off [dist m] [height m] [side deg]   local camera for shots
//   kg.Forest.Reload, kg.Forest.ResetSession
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Core/KGPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Forest/KGForestActors.h"
#include "Forest/KGForestMap.h"
#include "Forest/KGForestSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"

namespace KGForestCmd
{
	AKGPlayerState* Pick(UWorld* World, const FString& Who)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS)
		{
			return nullptr;
		}
		for (APlayerState* Raw : GS->PlayerArray)
		{
			AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (!PS)
			{
				continue;
			}
			const APlayerController* PC = Cast<APlayerController>(PS->GetOwningController());
			if ((Who.IsEmpty() || Who == TEXT("me")) && PC && PC->IsLocalController())
			{
				return PS;
			}
			if (Who == TEXT("remote") && PC && !PC->IsLocalController())
			{
				return PS;
			}
			if (Who == TEXT("bot") && PS->IsABot())
			{
				return PS;
			}
			if (PS->GetPlayerName().Equals(Who, ESearchCase::IgnoreCase))
			{
				return PS;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GStatus(TEXT("kg.Forest.Status"), TEXT("Forest bands, stages, caps and PvE lethality."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UKGForestSubsystem* FS = UKGForestSubsystem::Get(World))
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_STATUS %s"), *FS->DevStatus());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GTest(TEXT("kg.Forest.Test"),
		TEXT("[server] kg.Forest.Test wolf|mist|stop|fire|logs|vigil [me|remote|bot|<name>] [stay]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGForestSubsystem* FS = UKGForestSubsystem::Get(World);
			if (!FS || !World || World->GetNetMode() == NM_Client || Args.Num() < 1)
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_FOREST test: run on the host: kg.Forest.Test wolf|mist|stop [who] [stay]"));
				return;
			}
			AKGPlayerState* PS = Pick(World, Args.Num() > 1 ? Args[1] : FString());
			if (!PS)
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_FOREST test: no player '%s'"), Args.Num() > 1 ? *Args[1] : TEXT("me"));
				return;
			}
			const bool bStay = Args.Contains(TEXT("stay"));
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_TEST %s"), *FS->DevTest(Args[0].ToLower(), PS, !bStay));
		}));

	FAutoConsoleCommandWithWorldAndArgs GGoto(TEXT("kg.Forest.Goto"), TEXT("[host] kg.Forest.Goto deep|camp|den [sector 1-4] [along m]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGForestSubsystem* FS = UKGForestSubsystem::Get(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* P = PC ? PC->GetPawn() : nullptr;
			if (!FS || !P || World->GetNetMode() == NM_Client)
			{
				return;
			}
			const FString What = Args.Num() ? Args[0].ToLower() : TEXT("deep");
			const int32 Sector = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
			const float Along = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;
			FVector At = What == TEXT("camp") ? FS->CampLocation() + FVector(-500.0f, 300.0f, 100.0f) : FS->DeepSpot(Sector, Along);
			if (What == TEXT("den"))
			{
				for (const FKGForestDen& D : FKGForestMap::Get().Dens)
				{
					if (D.Sector == Sector)
					{
						At = FVector(D.At.X * 100.0f, D.At.Y * 100.0f, At.Z);
					}
				}
			}
			{ FRotator R = P->GetActorRotation(); World->FindTeleportSpot(P, At, R); P->TeleportTo(At, R, false, true); }
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST goto %s -> %s"), *What, *At.ToCompactString());
		}));

	FAutoConsoleCommandWithWorldAndArgs GAutoWalkCmd(TEXT("kg.Forest.AutoWalk"),
		TEXT("[client] kg.Forest.AutoWalk 1|2|0: walk (never run) along the arrow to the nearest path (2 = only from a Mist tongue)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			GKGForestAutoWalk = Args.Num() == 0 ? 1 : FCString::Atoi(*Args[0]);   // AKGForestPlayerInfo::Tick walks
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST autowalk=%d"), GKGForestAutoWalk);
		}));

	FAutoConsoleCommandWithWorldAndArgs GSmoke(TEXT("kg.Forest.Smoke"), TEXT("[host] run the two-process forest smoke script"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			UKGForestSubsystem::bSmoke = true;
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_SMOKE armed"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GReload(TEXT("kg.Forest.Reload"), TEXT("Re-read Tools/Level/morrowmere_forest_v2.json"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			FString Msg;
			const bool bOk = FKGForestMap::Reload(Msg);
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST reload ok=%d %s"), bOk ? 1 : 0, *Msg);
		}));

	FAutoConsoleCommandWithWorldAndArgs GReset(TEXT("kg.Forest.ResetSession"), TEXT("Zero the session's PvE death share tally"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UKGForestSubsystem* FS = UKGForestSubsystem::Get(World))
			{
				FS->DevResetSession();
			}
		}));

	TWeakObjectPtr<ACameraActor> GCam;

	FAutoConsoleCommandWithWorldAndArgs GCamCmd(TEXT("kg.Forest.Cam"),
		TEXT("kg.Forest.Cam wolf|mist|camp|me|pov:wolf|pov:mist|off [dist m] [height m] [side deg]: a local camera for shots"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!PC)
			{
				return;
			}
			const FString What = Args.Num() ? Args[0].ToLower() : TEXT("off");
			if (What == TEXT("off"))
			{
				PC->SetViewTargetWithBlend(PC->GetPawn(), 0.0f);
				return;
			}
			const float Dist = (Args.Num() > 1 ? FCString::Atof(*Args[1]) : 6.0f) * 100.0f;
			const float Height = (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 1.6f) * 100.0f;
			const float Side = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 35.0f;
			const FString Target = What.StartsWith(TEXT("pov:")) ? What.Mid(4) : What;
			AActor* T = nullptr;
			if (Target == TEXT("wolf"))
			{
				float Best = 1e12f;
				for (TActorIterator<AKGWolf> It(World); It; ++It)
				{
					const float D = PC->GetPawn() ? FVector::DistSquared(It->GetActorLocation(), PC->GetPawn()->GetActorLocation()) : 0.0f;
					if (It->bAwake && D < Best)
					{
						Best = D;
						T = *It;
					}
				}
			}
			else if (Target == TEXT("mist"))
			{
				for (TActorIterator<AKGMistTongue> It(World); It; ++It)
				{
					T = *It;
				}
			}
			else if (Target == TEXT("camp"))
			{
				T = AKGCampfire::Get(World);
			}
			else
			{
				T = PC->GetPawn();
			}
			if (!T)
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_FOREST cam: no %s"), *Target);
				return;
			}
			if (!GCam.IsValid())
			{
				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				GCam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
				if (GCam.IsValid())
				{
					GCam->GetCameraComponent()->SetFieldOfView(60.0f);
					GCam->GetCameraComponent()->bConstrainAspectRatio = false;
				}
			}
			if (!GCam.IsValid())
			{
				return;
			}
			const FVector Look = T->GetActorLocation() + FVector(0.0f, 0.0f, Target == TEXT("mist") ? 150.0f : 40.0f);
			FVector Eye;
			if (What.StartsWith(TEXT("pov:")) && PC->GetPawn())
			{
				Eye = PC->GetPawn()->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f);
			}
			else
			{
				const APawn* Me = PC->GetPawn();
				FVector From = Me && Me != T ? (Me->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D() : T->GetActorForwardVector();
				if (From.IsNearlyZero())
				{
					From = FVector(1, 0, 0);
				}
				From = From.RotateAngleAxis(Side, FVector::UpVector);
				Eye = T->GetActorLocation() + From * Dist + FVector(0.0f, 0.0f, Height);
			}
			GCam->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
			PC->SetViewTargetWithBlend(GCam.Get(), 0.0f);
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST cam %s eye=%s target=%s"), *What, *Eye.ToCompactString(), *T->GetName());
		}));
}
