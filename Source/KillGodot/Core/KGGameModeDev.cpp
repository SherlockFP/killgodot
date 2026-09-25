// AKGGameMode dev hooks (dev panel, kg.* console commands, automation). Kept out of KGGameMode.cpp so the match flow
// stays readable; every function here is authority-only and uses the same internals as the real flow.
#include "Core/KGGameMode.h"
#include "AI/KGBotController.h"
#include "Core/KGGameState.h"
#include "Core/KGLobbyState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "KillGodot.h"
#include "Roles/KGRoleListGenerator.h"

int32 AKGGameMode::DevFillBots(int32 TargetPlayers)
{
	if (!GameState)
	{
		return 0;
	}
	const int32 Before = GameState->PlayerArray.Num();
	FillWithBots(TargetPlayers);
	return FMath::Max(0, GameState->PlayerArray.Num() - Before);
}

int32 AKGGameMode::DevAddBots(int32 Count)
{
	return GameState && Count > 0 ? DevFillBots(GameState->PlayerArray.Num() + Count) : 0;
}

int32 AKGGameMode::DevRemoveBots(int32 Count, AController* Specific)
{
	if (!GameState || (Count <= 0 && !Specific))
	{
		return 0;
	}
	// Newest first (PlayerArray is join order). Collect before destroying: destroying a controller removes its
	// player state from PlayerArray synchronously.
	TArray<AController*> Victims;
	for (int32 i = GameState->PlayerArray.Num() - 1; i >= 0; --i)
	{
		const APlayerState* PS = GameState->PlayerArray[i];
		AController* BotController = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
		if (!PS || !PS->IsABot() || !BotController || (Specific && BotController != Specific))
		{
			continue;
		}
		Victims.Add(BotController);
		if (!Specific && Victims.Num() >= Count)
		{
			break;
		}
	}
	AKGGameState* GS = GetKGGameState();
	for (AController* Bot : Victims)
	{
		if (GS && Bot->PlayerState && GS->OnTrial == Bot->PlayerState)
		{
			GS->OnTrial = nullptr;
		}
		if (APawn* Body = Bot->GetPawn())
		{
			Bot->UnPossess();
			Body->Destroy();
		}
		UE_LOG(LogKillGodot, Log, TEXT("Dev: removed bot %s"), Bot->PlayerState ? *Bot->PlayerState->GetPlayerName() : *Bot->GetName());
		Bot->Destroy();   // AController::Destroyed cleans up the player state (and its PlayerArray entry)
	}
	return Victims.Num();
}

void AKGGameMode::DevStartMatch(int64 Seed)
{
	AutoStartRemaining = -1.0f;
	if (AKGLobbyState* Lobby = AKGLobbyState::Get(this); Lobby && !Lobby->HasStarted())
	{
		// Hosted from the front end: go through the lobby so its UI and bot fill stay consistent.
		Lobby->AuthSetCountdown(true);
		return;
	}
	if (AKGGameState* GS = GetKGGameState())
	{
		GS->bHasWinner = false;
		GS->OnTrial = nullptr;
		GS->DayIndex = 0;
	}
	StartMatchFlow(Seed != 0 ? Seed : FDateTime::UtcNow().GetTicks());
}

void AKGGameMode::DevJumpToPhase(EKGPhase Phase)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS)
	{
		return;
	}
	AutoStartRemaining = -1.0f;
	if (MatchSeed == 0)
	{
		MatchSeed = FDateTime::UtcNow().GetTicks();
		Rng.Reseed(static_cast<uint64>(MatchSeed));
	}
	if (Phase == EKGPhase::Lobby || Phase == EKGPhase::Migrating)
	{
		GS->SetPhase(Phase, 0.0f);
		return;
	}
	if (Phase != EKGPhase::Epilogue)
	{
		GS->bHasWinner = false;
	}
	// A jump is clean: leaving a trial through the panel never hangs anyone.
	if (Phase != EKGPhase::Trial)
	{
		GS->OnTrial = nullptr;
	}

	bool bRolesDealt = false;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		bRolesDealt |= PS && !PS->GetPrivateRoleId().IsNone();
	}
	if (!bRolesDealt && Phase != EKGPhase::Warmup && Phase != EKGPhase::RoleReveal)
	{
		AssignRoles();   // RoleReveal deals them itself in EnterPhase
	}

	if (Phase == EKGPhase::Trial)
	{
		// Someone has to stand on the gallows: prefer a living bot, else anyone alive.
		AKGPlayerState* Accused = nullptr;
		for (APlayerState* Raw : GameState->PlayerArray)
		{
			AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (PS && PS->IsAlive() && (!Accused || (PS->IsABot() && !Accused->IsABot())))
			{
				Accused = PS;
			}
		}
		if (Accused)
		{
			StartTrial(Accused);
			return;
		}
		UE_LOG(LogKillGodot, Warning, TEXT("Dev: nobody alive to put on trial - going to the meeting instead"));
		Phase = EKGPhase::Meeting;
	}
	EnterPhase(Phase);
}

void AKGGameMode::DevForceWin(EKGAlignment InWinner)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS)
	{
		return;
	}
	AutoStartRemaining = -1.0f;
	GS->OnTrial = nullptr;
	GS->bHasWinner = true;
	GS->Winner = InWinner;
	EnterPhase(EKGPhase::Epilogue);
}

void AKGGameMode::DevResetTasks(AKGPlayerState* Only)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS)
	{
		return;
	}
	if (!Only)
	{
		TArray<AKGPlayerState*> Players;
		for (APlayerState* Raw : GameState->PlayerArray)
		{
			if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
			{
				Players.Add(PS);
			}
		}
		AssignTasks(Players);   // fresh deal, Preparation back to 0
		GS->ForceNetUpdate();
		return;
	}
	Only->TaskDone.Init(false, Only->TaskIds.Num());
	Only->ForceNetUpdate();
	// Recount the town's progress from the ledgers (the Impatient's chores never count).
	TownTasksDone = 0;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		const FKGRoleInfo* RoleInfo = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
		                                                                   PS->GetPrivateRoleId()) : nullptr;
		if (!PS || (RoleInfo && RoleInfo->GetAlignment() == EKGAlignment::Impatient))
		{
			continue;
		}
		for (const bool bDone : PS->TaskDone)
		{
			TownTasksDone += bDone ? 1 : 0;
		}
	}
	GS->Preparation = TownTasksTotal > 0 ? FMath::Clamp(float(TownTasksDone) / TownTasksTotal, 0.0f, 1.0f) : 0.0f;
	bIlluminated = bIlluminated && GS->Preparation >= 0.999f;
	GS->ForceNetUpdate();
}
