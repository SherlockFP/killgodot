#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Core/KGPlayerController.h"
#include "Core/KGLobbyState.h"
#include "Character/KGCharacter.h"
#include "Roles/KGRoleListGenerator.h"
#include "KillGodot.h"
#include "Camera/CameraComponent.h"
#include "Character/KGViewmodelComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UI/KGHUD.h"
#include "World/KGDoor.h"
#include "World/KGInteractable.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "AI/KGBotController.h"
#include "GameFramework/DamageType.h"
#include "World/KGTaskStation.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "UI/Reveal/KGRevealTypes.h"

namespace
{
	// Playtest-friendly lengths; the GDD values (120 s warmup, 90 s epilogue) return with the lobby flow (M4).
	TAutoConsoleVariable<float> CVarWarmup(TEXT("kg.WarmupSeconds"), 25.0f, TEXT("Warmup length (s)."));
	TAutoConsoleVariable<float> CVarEpilogue(TEXT("kg.EpilogueSeconds"), 20.0f, TEXT("Epilogue length (s)."));
	TAutoConsoleVariable<int32> CVarBotFill(TEXT("kg.BotFill"), 6, TEXT("Fill the match with bots up to N players (0 = off)."));
	TAutoConsoleVariable<int32> CVarSkipPhase(TEXT("kg.SkipPhase"), 0, TEXT("Dev: end the current phase now (auto-resets)."));
	TAutoConsoleVariable<int32> CVarAutoStart(TEXT("kg.AutoStart"), 1, TEXT("Start the match flow automatically."));

	const TCHAR* BotNames[] = {TEXT("Fisher Riza"), TEXT("Baker Nuri"), TEXT("Widow Hatice"), TEXT("Old Kemal"),
	                           TEXT("Netmaker Sevgi"), TEXT("Smith Cemal"), TEXT("Priest Aurel"), TEXT("Innkeeper Mara"),
	                           TEXT("Shepherd Yusuf"), TEXT("Lamplighter Ivo"), TEXT("Herbalist Dunya"),
	                           TEXT("Harbourmaster Osman"), TEXT("Tanner Petra"), TEXT("Miller Salih"),
	                           TEXT("Candlemaker Lale"), TEXT("Ferryman Boris"), TEXT("Weaver Ayse"),
	                           TEXT("Gravedigger Emin"), TEXT("Cooper Vlad")};

	EKGAlignment AlignmentOf(const AKGPlayerState* PS)
	{
		const FKGRoleInfo* Role = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
		                                                                PS->GetPrivateRoleId())
		                             : nullptr;
		return Role ? Role->GetAlignment() : EKGAlignment::Town;
	}
}

AKGGameMode::AKGGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = AKGGameState::StaticClass();
	PlayerStateClass = AKGPlayerState::StaticClass();
	DefaultPawnClass = AKGCharacter::StaticClass();
	HUDClass = AKGHUD::StaticClass();
	PlayerControllerClass = AKGPlayerController::StaticClass(); // Esc pause menu + look/FOV settings
}

float AKGGameMode::GetPhaseDuration(EKGPhase Phase, int32 NumPlayers)
{
	const float N = static_cast<float>(FMath::Clamp(NumPlayers, FKGRoleListGenerator::MinPlayers,
	                                                FKGRoleListGenerator::MaxPlayers));
	switch (Phase)
	{
	case EKGPhase::Warmup:
		return CVarWarmup.GetValueOnGameThread();
	case EKGPhase::RoleReveal:
		return KGReveal::PhaseSeconds;   // the reveal ceremony (UI/Reveal), 8-12 s by contract (SPRINT-015)
	case EKGPhase::Dawn:
		return 12.0f;
	case EKGPhase::Day:
		return 150.0f + 6.0f * N;
	case EKGPhase::Meeting:
		return 45.0f + 3.0f * N;
	case EKGPhase::Trial:
		return 20.0f + 15.0f + 8.0f; // defense + judgement + last words
	case EKGPhase::Night:
		return 75.0f + 3.0f * N;
	case EKGPhase::Epilogue:
		return CVarEpilogue.GetValueOnGameThread();
	default:
		return 0.0f;
	}
}

AKGGameState* AKGGameMode::GetKGGameState() const
{
	return GetGameState<AKGGameState>();
}

void AKGGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Dev automation only (not gameplay): -KGAutoShot takes a screenshot after 4 s and quits after 7 s,
	// so agents can verify the first-person view without a human. TimerManager is fine here.
	if (FParse::Param(FCommandLine::Get(), TEXT("KGAutoShot")))
	{
		bDevPerf = true;
		FTimerHandle ShotHandle;
		FTimerHandle QuitHandle;
		GetWorldTimerManager().SetTimer(ShotHandle, []()
		{
			FScreenshotRequest::RequestScreenshot(TEXT("KG_AutoShot.png"), false, false);
			UE_LOG(LogKillGodot, Log, TEXT("KG_AUTOSHOT requested"));
		}, 4.0f, false);
		FTimerHandle PerfHandle;
		GetWorldTimerManager().SetTimer(PerfHandle, [this]()
		{
			bDevPerf = false;
			if (DevFrameTimesMs.Num() == 0)
			{
				return;
			}
			TArray<float> Sorted = DevFrameTimesMs;
			Sorted.Sort();
			float Sum = 0.0f;
			for (const float Ms : Sorted)
			{
				Sum += Ms;
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_PERF frames=%d avg_ms=%.2f p95_ms=%.2f max_ms=%.2f"), Sorted.Num(),
			       Sum / Sorted.Num(), Sorted[FMath::Min(Sorted.Num() - 1, FMath::FloorToInt(Sorted.Num() * 0.95f))],
			       Sorted.Last());
		}, 6.5f, false);
		GetWorldTimerManager().SetTimer(QuitHandle, [this]()
		{
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		}, 7.0f, false);
	}

	// Dev scenarios for automated visual checks: -KGDevScenario=backstab | inspect
	FString Scenario;
	const bool bScenario = FParse::Value(FCommandLine::Get(), TEXT("KGDevScenario="), Scenario);
	// Matches hosted from the front end open a pre-game lobby (?KGLobby); the host starts them from there.
	const bool bLobby = UGameplayStatics::HasOption(OptionsString, TEXT("KGLobby"));
	if (bLobby && !bScenario)
	{
		GetWorld()->SpawnActor<AKGLobbyState>();
	}
	else if (!bScenario && !bDevPerf && CVarAutoStart.GetValueOnGameThread() != 0)
	{
		// Give PIE clients a moment to join before bots take the remaining seats.
		AutoStartRemaining = 3.0f;
	}
	if (bScenario)
	{
		FTimerHandle ScenarioHandle;
		GetWorldTimerManager().SetTimer(ScenarioHandle, [this, Scenario]()
		{
			AKGCharacter* Player = Cast<AKGCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
			if (!Player)
			{
				return;
			}
			if (Scenario == TEXT("backstab"))
			{
				for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
				{
					if (*It != Player)
					{
						const FVector Fwd = It->GetActorForwardVector();
						Player->SetActorLocation(It->GetActorLocation() - Fwd * 110.0f, false, nullptr,
						                         ETeleportType::TeleportPhysics);
						if (AController* C = Player->GetController())
						{
							C->SetControlRotation(Fwd.Rotation());
						}
						Player->bHoldingAssassinBlade = true;
						break;
					}
				}
			}
			else if (Scenario == TEXT("inspect"))
			{
				Player->GetViewmodel()->StartInspect();
			}
			else if (Scenario == TEXT("dummy_front") || Scenario == TEXT("door"))
			{
				// Stand 2.5 m in front of the dummy (or the door) and face it.
				AActor* Target = nullptr;
				if (Scenario == TEXT("door"))
				{
					for (TActorIterator<AKGDoor> It(GetWorld()); It && !Target; ++It)
					{
						Target = *It;
					}
				}
				else
				{
					for (TActorIterator<AKGCharacter> It(GetWorld()); It && !Target; ++It)
					{
						Target = *It != Player ? *It : nullptr;
					}
				}
				if (Target)
				{
					const FVector Front = Scenario == TEXT("door") ? -Target->GetActorForwardVector() : Target->GetActorForwardVector();
					const FVector Stand = Target->GetActorLocation() + Front * 250.0f;
					Player->SetActorLocation(FVector(Stand.X, Stand.Y, Player->GetActorLocation().Z), false, nullptr,
					                         ETeleportType::TeleportPhysics);
					if (AController* C = Player->GetController())
					{
						const FVector LookAt = Target->GetActorLocation() + FVector(0.0f, 0.0f, Scenario == TEXT("door") ? 110.0f : 60.0f);
						C->SetControlRotation((LookAt - Player->GetFirstPersonCamera()->GetComponentLocation()).Rotation());
					}
					if (Scenario == TEXT("door"))
					{
						IKGInteractable::Execute_Interact(Target, Player);
					}
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_SCENARIO %s applied"), *Scenario);
			for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
			{
				const USkeletalMeshComponent* Body = It->GetMesh();
				UE_LOG(LogKillGodot, Log, TEXT("KG_DIAG %s actor=%s bounds=%s ext=%s head=%s anim=%s"), *It->GetName(),
				       *It->GetActorLocation().ToString(), *Body->Bounds.Origin.ToString(),
				       *Body->Bounds.BoxExtent.ToString(), *Body->GetBoneLocation(TEXT("Head")).ToString(),
				       Body->GetSingleNodeInstance() ? TEXT("single-node") : TEXT("none"));
			}
			// Where are the first-person hands relative to the camera (X fwd, Y right, Z up, cm)?
			if (const UCameraComponent* Cam = Player->GetFirstPersonCamera())
			{
				TArray<USkeletalMeshComponent*> Meshes;
				Player->GetComponents(Meshes);
				for (const USkeletalMeshComponent* M : Meshes)
				{
					if (M != Player->GetMesh())
					{
						const FTransform CamXf = Cam->GetComponentTransform();
						UE_LOG(LogKillGodot, Log, TEXT("KG_DIAG arms mesh=%s visible=%d hiddenInGame=%d rendered=%d boundsCam=%s ext=%s"),
						       M->GetSkeletalMeshAsset() ? *M->GetSkeletalMeshAsset()->GetName() : TEXT("none"),
						       M->IsVisible() ? 1 : 0, M->bHiddenInGame ? 1 : 0, M->WasRecentlyRendered(0.5f) ? 1 : 0,
						       *Cam->GetComponentTransform().InverseTransformPosition(M->Bounds.Origin).ToString(),
						       *M->Bounds.BoxExtent.ToString());
						UE_LOG(LogKillGodot, Log, TEXT("KG_DIAG arms handscale=%s comp=%s"),
						       *M->GetBoneTransform(M->GetBoneIndex(TEXT("hand_r"))).GetScale3D().ToString(),
						       *M->GetComponentScale().ToString());
						UE_LOG(LogKillGodot, Log, TEXT("KG_DIAG arms hand_r=%s hand_l=%s clavicle_r=%s"),
						       *CamXf.InverseTransformPosition(M->GetBoneLocation(TEXT("hand_r"))).ToString(),
						       *CamXf.InverseTransformPosition(M->GetBoneLocation(TEXT("hand_l"))).ToString(),
						       *CamXf.InverseTransformPosition(M->GetBoneLocation(TEXT("clavicle_r"))).ToString());
					}
				}
			}
		}, Scenario == TEXT("inspect") ? 3.3f : 1.0f, false);
	}
}

void AKGGameMode::StartFromLobby(int32 BotFillTarget)
{
	if (BotFillTarget > 0)
	{
		FillWithBots(BotFillTarget);
	}
	// The lobby was the warm-up: straight to the role reveal ceremony, the village only shows after the cards.
	const int64 Seed = FDateTime::UtcNow().GetTicks();
	Rng.Reseed(static_cast<uint64>(Seed));
	MatchSeed = Seed;
	EnterPhase(EKGPhase::RoleReveal);
}

void AKGGameMode::StartMatchFlow(int64 Seed)
{
	Rng.Reseed(static_cast<uint64>(Seed));
	MatchSeed = Seed;
	EnterPhase(EKGPhase::Warmup);
}

void AKGGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDevPerf)
	{
		DevElapsed += DeltaSeconds;
		if (DevElapsed > 1.5f)
		{
			DevFrameTimesMs.Add(FApp::GetDeltaTime() * 1000.0f);
		}
	}

	if (AutoStartRemaining > 0.0f)
	{
		AutoStartRemaining -= DeltaSeconds;
		if (AutoStartRemaining <= 0.0f)
		{
			FillWithBots(CVarBotFill.GetValueOnGameThread());
			StartMatchFlow(FDateTime::UtcNow().GetTicks());
		}
	}

	AKGGameState* GS = GetKGGameState();
	if (!GS || GS->GetPhase() == EKGPhase::Lobby || GS->GetPhase() == EKGPhase::Migrating)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
	if (CVarSkipPhase.GetValueOnGameThread() != 0)
	{
		CVarSkipPhase->Set(0, ECVF_SetByConsole);
		GS->Clock.RemainingSeconds = 0.01f;
	}
#endif
	if (GS->Clock.Advance(DeltaSeconds * DevClockScale))   // DevClockScale: dev panel fast-forward (1 = real time)
	{
		if (GS->GetPhase() == EKGPhase::Epilogue)
		{
			// Next round: reload the map with everyone (bots respawn through the fill again).
			UE_LOG(LogKillGodot, Log, TEXT("Epilogue over - restarting the round"));
			GetWorld()->ServerTravel(TEXT("?Restart"), false);
			return;
		}
		EnterPhase(GetNextPhase(GS->GetPhase(), GS->DayIndex));
	}
}

EKGPhase AKGGameMode::GetNextPhase(EKGPhase Current, int32 DayIndex) const
{
	switch (Current)
	{
	case EKGPhase::Warmup:
		return EKGPhase::RoleReveal;
	case EKGPhase::RoleReveal:
		return EKGPhase::Day;
	case EKGPhase::Dawn:
		return EKGPhase::Day;
	case EKGPhase::Day:
		// Day 1 is the "get to know each other" day: no meeting.
		return DayIndex <= 1 ? EKGPhase::Night : EKGPhase::Meeting;
	case EKGPhase::Meeting:
	case EKGPhase::Trial:
		// A trial is entered explicitly when a vote passes; timing out always leads to night.
		return EKGPhase::Night;
	case EKGPhase::Night:
		return EKGPhase::Dawn;
	default:
		return Current;
	}
}

void AKGGameMode::EnterPhase(EKGPhase NewPhase)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS)
	{
		return;
	}
	if (NewPhase == EKGPhase::RoleReveal)
	{
		AssignRoles();
	}
	if (NewPhase == EKGPhase::Day)
	{
		GS->DayIndex++;
	}
	if (NewPhase == EKGPhase::Epilogue)
	{
		RevealAllRoles();
	}
	// Leaving a trial for any reason resolves it (guilty -> the gallows).
	if (GS->GetPhase() == EKGPhase::Trial && NewPhase != EKGPhase::Trial)
	{
		ResolveTrial();
		if (GS->GetPhase() == EKGPhase::Epilogue)
		{
			return;   // the hanging decided the match
		}
	}
	if (NewPhase == EKGPhase::Night)
	{
		NightDeaths.Reset();
	}
	GS->SetPhase(NewPhase, GetPhaseDuration(NewPhase, GetNumPlayers()));
	if (NewPhase == EKGPhase::Meeting)
	{
		StartMeeting();
	}
	if (NewPhase == EKGPhase::Dawn)
	{
		AnnounceDawn();
	}
	UE_LOG(LogKillGodot, Log, TEXT("Phase -> %s (day %d, %.0fs)"), *UEnum::GetValueAsString(NewPhase), GS->DayIndex,
	       GS->Clock.RemainingSeconds);
}

void AKGGameMode::AssignRoles()
{
	TArray<AKGPlayerState*> Players;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AKGPlayerState* KGPS = Cast<AKGPlayerState>(PS))
		{
			Players.Add(KGPS);
		}
	}
	if (Players.Num() < FKGRoleListGenerator::MinPlayers)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("AssignRoles: %d players (min %d) - using the %d-player list"),
		       Players.Num(), FKGRoleListGenerator::MinPlayers, FKGRoleListGenerator::MinPlayers);
	}

	const FKGRoleListResult Result =
		FKGRoleListGenerator::Generate(Players.Num(), Rng, FKGRoleListGenerator::GetDefaultCatalog());
	TArray<FName> Roles = Result.Roles;
	Rng.Shuffle(Roles);
	for (int32 i = 0; i < Players.Num() && i < Roles.Num(); ++i)
	{
		Players[i]->SetPrivateRoleId(Roles[i]);
		// Warm-up knife play ends here: only the Impatient keep a blade.
		if (AKGCharacter* C = Cast<AKGCharacter>(Players[i]->GetPawn()); C && !C->CanDrawBlade())
		{
			C->bHoldingAssassinBlade = false;
		}
	}
	// Every villager owns a home (its storage chest locks to them): shuffled so homes differ between matches.
	TArray<int32> Homes;
	for (int32 i = 0; i < 16; ++i)
	{
		Homes.Add(i);
	}
	Rng.Shuffle(Homes);
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		Players[i]->HouseIndex = i < Homes.Num() ? Homes[i] : INDEX_NONE;
	}
	AssignTasks(Players);
	UE_LOG(LogKillGodot, Log, TEXT("Roles assigned: %d players, balance %d (in band: %s)"), Players.Num(),
	       Result.Balance, Result.bWithinBand ? TEXT("yes") : TEXT("no"));
}

void AKGGameMode::FillWithBots(int32 TargetPlayers)
{
	const int32 Missing = TargetPlayers - GameState->PlayerArray.Num();
	for (int32 i = 0; i < Missing; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKGBotController* Bot = GetWorld()->SpawnActor<AKGBotController>(Params);
		if (!Bot)
		{
			continue;
		}
		if (APlayerState* PS = Bot->PlayerState)
		{
			PS->SetPlayerName(BotNames[BotsSpawned % UE_ARRAY_COUNT(BotNames)]);
			PS->SetIsABot(true);
		}
		++BotsSpawned;
		RestartPlayer(Bot);
	}
	UE_LOG(LogKillGodot, Log, TEXT("Bot fill: %d bots, %d players total"), FMath::Max(Missing, 0),
	       GameState->PlayerArray.Num());
}

void AKGGameMode::OnCharacterDied(AKGCharacter* Victim, AActor* Killer)
{
	if (!Victim)
	{
		return;
	}
	if (AKGPlayerState* PS = Victim->GetPlayerState<AKGPlayerState>())
	{
		const AKGGameState* GS = GetKGGameState();
		if (GS && GS->GetPhase() == EKGPhase::Night)
		{
			NightDeaths.Add(FString::Printf(TEXT("%s (%s)"), *PS->GetPlayerName(), *RoleLabel(PS)));
		}
		else if (GS && GS->GetPhase() != EKGPhase::Trial && GS->GetPhase() != EKGPhase::Epilogue)
		{
			// A body in daylight: the bell rings for everyone.
			GetKGGameState()->Announce(FString::Printf(TEXT("The bell tolls! %s was found dead. They were the %s."),
			                                           *PS->GetPlayerName(), *RoleLabel(PS)), 8.0f);
		}
		PS->LifeState = EKGLifeState::Ghost;
		// Town of Salem rule: the dead's role is read out (the Cleaner will be able to hide it later).
		PS->RevealedRoleId = PS->GetPrivateRoleId();
		PS->ForceNetUpdate();
	}
	CheckWinCondition();
}

void AKGGameMode::CheckWinCondition()
{
	AKGGameState* GS = GetKGGameState();
	if (!GS || GS->bHasWinner)
	{
		return;
	}
	const EKGPhase Phase = GS->GetPhase();
	if (Phase == EKGPhase::Lobby || Phase == EKGPhase::Warmup || Phase == EKGPhase::RoleReveal ||
	    Phase == EKGPhase::Epilogue || Phase == EKGPhase::Migrating)
	{
		return;
	}
	int32 ImpatientAlive = 0;
	int32 OthersAlive = 0;
	for (APlayerState* Raw : GS->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		if (!PS || !PS->IsAlive())
		{
			continue;
		}
		(AlignmentOf(PS) == EKGAlignment::Impatient ? ImpatientAlive : OthersAlive)++;
	}
	if (ImpatientAlive == 0 || ImpatientAlive >= OthersAlive)
	{
		GS->bHasWinner = true;
		GS->Winner = ImpatientAlive == 0 ? EKGAlignment::Town : EKGAlignment::Impatient;
		UE_LOG(LogKillGodot, Log, TEXT("Match decided: %s win (impatient alive %d, others alive %d)"),
		       GS->Winner == EKGAlignment::Town ? TEXT("Town") : TEXT("Impatient"), ImpatientAlive, OthersAlive);
		EnterPhase(EKGPhase::Epilogue);
	}
}

void AKGGameMode::RevealAllRoles()
{
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
		{
			PS->RevealedRoleId = PS->GetPrivateRoleId();
			PS->ForceNetUpdate();
		}
	}
}

FString AKGGameMode::RoleLabel(const AKGPlayerState* PS)
{
	const FString Raw = PS ? PS->GetPrivateRoleId().ToString() : FString();
	FString Out;
	for (int32 i = 0; i < Raw.Len(); ++i)
	{
		if (i > 0 && FChar::IsUpper(Raw[i]) && !FChar::IsUpper(Raw[i - 1]))
		{
			Out.AppendChar(TEXT(' '));
		}
		Out.AppendChar(Raw[i]);
	}
	return Out;
}

int32 AKGGameMode::CountAlive() const
{
	int32 Alive = 0;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		Alive += PS && PS->IsAlive() ? 1 : 0;
	}
	return Alive;
}

FVector AKGGameMode::GetGallowsLocation() const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("KG_Gallows")))
		{
			return It->GetActorLocation();
		}
	}
	return FVector(0.0, -800.0, 500.0);
}

void AKGGameMode::StartMeeting()
{
	AKGGameState* GS = GetKGGameState();
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
		{
			PS->AccuseTarget = nullptr;
			PS->Verdict = 0;
		}
	}
	GS->OnTrial = nullptr;
	// Everyone alive is gathered in a ring in front of the gallows (the GDD's "late-comers fade in").
	const FVector G = GetGallowsLocation();
	TArray<AKGCharacter*> Alive;
	for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && It->GetPlayerState())
		{
			Alive.Add(*It);
		}
	}
	// Half ring on the square side of the gallows: toward the KG_BotHub marker (v2: the fountain), else +Y (v1 plaza).
	float FirstDeg = 20.0f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("KG_BotHub")))
		{
			const FVector ToHub = It->GetActorLocation() - G;
			FirstDeg = FMath::RadiansToDegrees(FMath::Atan2(ToHub.Y, ToHub.X)) - 70.0f;
			break;
		}
	}
	for (int32 i = 0; i < Alive.Num(); ++i)
	{
		// Everyone facing the platform.
		const float A = FMath::DegreesToRadians(FirstDeg + 140.0f * (Alive.Num() > 1 ? float(i) / (Alive.Num() - 1) : 0.5f));
		const FVector P(G.X + 650.0 * FMath::Cos(A), G.Y + 650.0 * FMath::Sin(A), G.Z + 150.0);
		const FRotator Face = (FVector(G.X, G.Y, P.Z) - P).Rotation();
		Alive[i]->TeleportTo(P, FRotator(0.0f, Face.Yaw, 0.0f), false, true);
		if (AController* C = Alive[i]->GetController())
		{
			C->SetControlRotation(FRotator(0.0f, Face.Yaw, 0.0f));
		}
	}
	GS->Announce(FString::Printf(TEXT("Town meeting! Look at a suspect and press [V] to accuse. %d votes send them to trial."),
	                             CountAlive() / 2 + 1), 10.0f);
}

void AKGGameMode::HandleAccuse(AKGPlayerState* Voter, AKGPlayerState* Target)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS || GS->GetPhase() != EKGPhase::Meeting || !Voter || !Voter->IsAlive() || Voter == Target ||
	    (Target && !Target->IsAlive()))
	{
		return;
	}
	Voter->AccuseTarget = (Voter->AccuseTarget == Target) ? nullptr : Target;   // pressing again withdraws
	Voter->ForceNetUpdate();
	if (!Target || Voter->AccuseTarget != Target)
	{
		return;
	}
	int32 Votes = 0;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		Votes += PS && PS->IsAlive() && PS->AccuseTarget == Target ? 1 : 0;
	}
	if (Votes >= CountAlive() / 2 + 1)
	{
		StartTrial(Target);
	}
}

void AKGGameMode::StartTrial(AKGPlayerState* Accused)
{
	AKGGameState* GS = GetKGGameState();
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
		{
			PS->Verdict = 0;
		}
	}
	GS->OnTrial = Accused;
	EnterPhase(EKGPhase::Trial);
	if (APawn* P = Accused->GetPawn())
	{
		const FVector G = GetGallowsLocation();
		P->TeleportTo(G + FVector(0.0, 0.0, 110.0), FRotator(0.0f, 90.0f, 0.0f), false, true);
		if (AController* C = P->GetController())
		{
			C->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));   // face the crowd
		}
	}
	GS->Announce(FString::Printf(TEXT("%s is on trial! Let them speak, then vote [Y] guilty or [N] innocent."),
	                             *Accused->GetPlayerName()), 10.0f);
}

void AKGGameMode::HandleVerdict(AKGPlayerState* Voter, bool bGuilty)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS || GS->GetPhase() != EKGPhase::Trial || !Voter || !Voter->IsAlive() || Voter == GS->OnTrial)
	{
		return;
	}
	Voter->Verdict = bGuilty ? 1 : 2;
	Voter->ForceNetUpdate();
}

void AKGGameMode::ResolveTrial()
{
	AKGGameState* GS = GetKGGameState();
	AKGPlayerState* Accused = GS ? GS->OnTrial.Get() : nullptr;
	if (!Accused)
	{
		return;
	}
	int32 Guilty = 0;
	int32 Innocent = 0;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		if (PS && PS->IsAlive() && PS != Accused)
		{
			Guilty += PS->Verdict == 1 ? 1 : 0;
			Innocent += PS->Verdict == 2 ? 1 : 0;
		}
	}
	GS->OnTrial = nullptr;
	if (Guilty > Innocent)
	{
		GS->Announce(FString::Printf(TEXT("%s was hanged (%d guilty, %d innocent). They were the %s."),
		                             *Accused->GetPlayerName(), Guilty, Innocent, *RoleLabel(Accused)), 10.0f);
		if (AKGCharacter* C = Cast<AKGCharacter>(Accused->GetPawn()))
		{
			UGameplayStatics::ApplyDamage(C, 10000.0f, nullptr, this, UDamageType::StaticClass());
		}
	}
	else
	{
		GS->Announce(FString::Printf(TEXT("%s was spared (%d guilty, %d innocent)."), *Accused->GetPlayerName(),
		                             Guilty, Innocent), 8.0f);
	}
}

void AKGGameMode::AnnounceDawn()
{
	AKGGameState* GS = GetKGGameState();
	if (NightDeaths.Num() == 0)
	{
		GS->Announce(TEXT("Dawn. Nobody died last night... Mr. Godot could not make it, but he will surely come tomorrow."), 9.0f);
	}
	else
	{
		GS->Announce(FString::Printf(TEXT("Dawn. Found dead this morning: %s."), *FString::Join(NightDeaths, TEXT(", "))),
		             10.0f);
	}
	NightDeaths.Reset();
}

void AKGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// Until EOS display names arrive (M3), humans get a village nickname instead of the machine name.
	static const TCHAR* Nicks[] = {TEXT("Wanderer"), TEXT("Stranger"), TEXT("Newcomer"), TEXT("Drifter"),
	                               TEXT("Pilgrim"), TEXT("Traveller")};
	if (NewPlayer && NewPlayer->PlayerState && !NewPlayer->PlayerState->IsABot())
	{
		NewPlayer->PlayerState->SetPlayerName(Nicks[HumansJoined++ % UE_ARRAY_COUNT(Nicks)]);
	}
}

void AKGGameMode::AssignTasks(const TArray<AKGPlayerState*>& Players)
{
	TArray<FName> Pool;
	for (TActorIterator<AKGTaskStation> It(GetWorld()); It; ++It)
	{
		Pool.AddUnique(It->TaskId);
	}
	TownTasksTotal = 0;
	TownTasksDone = 0;
	bIlluminated = false;
	if (AKGGameState* GS = GetKGGameState())
	{
		GS->Preparation = 0.0f;
	}
	if (Pool.Num() == 0)
	{
		return;
	}
	constexpr int32 TasksPerPlayer = 4;
	for (AKGPlayerState* PS : Players)
	{
		// SPRINT-016 hook: about 70 % world chores / 30 % panel chores where the level has world chores
		// (Chores/WorldChores); levels without them get exactly the old shuffle.
		TArray<FName> Mine = FKGWorldChoreRules::Deal(Pool, FKGWorldChoreCatalog::Get(), PS->IsABot(), TasksPerPlayer, Rng);
		PS->TaskIds = Mine;
		PS->TaskDone.Init(false, Mine.Num());
		const FKGRoleInfo* RoleInfo = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
		                                                         PS->GetPrivateRoleId());
		if (!RoleInfo || RoleInfo->GetAlignment() != EKGAlignment::Impatient)
		{
			TownTasksTotal += Mine.Num();
		}
		PS->ForceNetUpdate();
	}
}

void AKGGameMode::OnTaskCompleted(AKGPlayerState* Who, FName TaskId)
{
	AKGGameState* GS = GetKGGameState();
	if (!GS || !Who)
	{
		return;
	}
	const FKGRoleInfo* RoleInfo = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
	                                                         Who->GetPrivateRoleId());
	if (RoleInfo && RoleInfo->GetAlignment() == EKGAlignment::Impatient)
	{
		return;   // faked: looks the same to onlookers, fills nothing
	}
	++TownTasksDone;
	GS->Preparation = TownTasksTotal > 0 ? FMath::Clamp(float(TownTasksDone) / TownTasksTotal, 0.0f, 1.0f) : 0.0f;
	GS->ForceNetUpdate();
	if (GS->Preparation >= 0.999f && !bIlluminated)
	{
		LighthouseIllumination();
	}
}

void AKGGameMode::LighthouseIllumination()
{
	AKGGameState* GS = GetKGGameState();
	bIlluminated = true;
	TArray<AKGPlayerState*> Impatient;
	for (APlayerState* Raw : GameState->PlayerArray)
	{
		AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		const FKGRoleInfo* RoleInfo = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
		                                                                PS->GetPrivateRoleId()) : nullptr;
		if (PS && PS->IsAlive() && RoleInfo && RoleInfo->GetAlignment() == EKGAlignment::Impatient)
		{
			Impatient.Add(PS);
		}
	}
	if (Impatient.Num() == 0)
	{
		return;
	}
	GS->LighthouseRevealed = Impatient[Rng.RandRange(0, Impatient.Num() - 1)];
	GS->RevealUntil = GS->GetServerWorldTimeSeconds() + 10.0f;
	GS->Announce(TEXT("The village is ready - the lighthouse blazes! One of the Impatient glows red for 10 seconds..."), 10.0f);
}
