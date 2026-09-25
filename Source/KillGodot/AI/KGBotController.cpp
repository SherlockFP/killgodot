#include "AI/KGBotController.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Roles/KGRoleListGenerator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "World/KGDoor.h"
#include "World/KGInteractable.h"
#include "World/KGMapInfo.h"
#include "World/KGTaskStation.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBotAI(TEXT("kg.BotAI"), 1,
	                                      TEXT("Bot brains on (1) or frozen (0). Dev panel: Bots > AI."));
	TAutoConsoleVariable<int32> CVarBotGather(TEXT("kg.BotGather"), 0,
	                                          TEXT("1 = every bot walks to the meeting hub now (test the meeting run-up); ")
	                                          TEXT("0 = automatic (last seconds of a day that ends in a meeting)."));

	// Legacy wander for levels without a navmesh: walkable open ground of L_Morrowmere (plaza ring + harbour street).
	const FVector2D PlazaCentre(0.0, -800.0);
	constexpr double PlazaRadius = 1700.0;

	constexpr float GatherLeadSeconds = 30.0f;   // walk to the square this long before a meeting starts
	constexpr double GatherArriveRadius = 1000.0; // "at the meeting" = within 10 m of the hub
	constexpr float StuckSeconds = 4.0f;          // no 0.8 m of progress in this long while moving = stuck
	constexpr double StuckDistance = 80.0;
	constexpr double NearAnchorRadius = 4500.0;   // most strolls stay in the neighbourhood

	// Cheap deterministic hash so bots differ without touching the gameplay RNG (bots are dev tooling).
	float Hash01(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return static_cast<float>(X & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
	}

	/** What bots know about the level, built once per world from the level's own data. */
	struct FBotLevel
	{
		TWeakObjectPtr<const UWorld> World;
		bool bBuilt = false;
		bool bNav = false;
		FVector Hub = FVector::ZeroVector;
		TArray<FVector> Anchors;
	};

	UNavigationSystemV1* NavOf(const UWorld* World)
	{
		return World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(World)) : nullptr;
	}

	FVector FindHub(const UWorld* World)
	{
		FVector Gallows = FVector::ZeroVector;
		bool bGallows = false;
		for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (It->ActorHasTag(TEXT("KG_BotHub")))
			{
				return It->GetActorLocation();
			}
			if (!bGallows && It->ActorHasTag(TEXT("KG_Gallows")))
			{
				Gallows = It->GetActorLocation();
				bGallows = true;
			}
		}
		if (bGallows)
		{
			return Gallows;
		}
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (TActorIterator<APlayerStart> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Sum += It->GetActorLocation();
			++N;
		}
		return N > 0 ? Sum / N : FVector(PlazaCentre, 0.0);
	}

	/** Points to sample inside a map region: its centre / label when inside, else the mean of its polygon. */
	void RegionPoints(const FKGMapRegion& R, TArray<FVector2D>& Out)
	{
		if (R.Contains(R.Center))
		{
			Out.Add(R.Center);
		}
		if (R.Contains(R.LabelPos) && FVector2D::Distance(R.LabelPos, R.Center) > 500.0)
		{
			Out.Add(R.LabelPos);
		}
		if (Out.Num() == 0 && R.Polygon.Num() >= 3)
		{
			// Long streets: points a third of the way between the centre and each polygon vertex that land inside.
			for (int32 i = 0; i < R.Polygon.Num() && Out.Num() < 3; i += FMath::Max(1, R.Polygon.Num() / 4))
			{
				const FVector2D P = FMath::Lerp(R.Polygon[i], R.Center, 0.3);
				if (R.Contains(P))
				{
					Out.Add(P);
				}
			}
		}
	}

	FBotLevel& LevelOf(const UWorld* World)
	{
		static FBotLevel Level;
		if (Level.bBuilt && Level.World.Get() == World)
		{
			return Level;
		}
		Level = FBotLevel();
		Level.World = World;
		Level.bBuilt = true;
		if (!World)
		{
			return Level;
		}
		Level.Hub = FindHub(World);
		UNavigationSystemV1* Nav = NavOf(World);
		FNavLocation HubNav;
		if (!Nav || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) ||
		    !Nav->ProjectPointToNavigation(Level.Hub, HubNav, FVector(600.0, 600.0, 1000.0)))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_BOT level: no navmesh at the hub, legacy plaza wander"));
			return Level;
		}
		Level.bNav = true;
		int32 Tried = 0;
		int32 Dropped = 0;
		// Keep only anchors the navmesh connects to the hub (stairs through their ramps, jetties, bridges).
		auto TryAdd = [&](const FVector& P, const FVector& Extent, int32 Copies)
		{
			++Tried;
			FNavLocation L;
			if (!Nav->ProjectPointToNavigation(P, L, Extent))
			{
				++Dropped;
				return;
			}
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(const_cast<UWorld*>(World),
			                                                                                  HubNav.Location, L.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				++Dropped;
				return;
			}
			for (int32 i = 0; i < Copies; ++i)
			{
				Level.Anchors.Add(L.Location);
			}
		};
		TryAdd(Level.Hub, FVector(600.0, 600.0, 1000.0), 3);
		int32 Starts = 0;
		for (TActorIterator<APlayerStart> It(const_cast<UWorld*>(World)); It && Starts < 2; ++It, ++Starts)
		{
			TryAdd(It->GetActorLocation(), FVector(200.0, 200.0, 300.0), 1);
		}
		for (TActorIterator<AKGTaskStation> It(const_cast<UWorld*>(World)); It; ++It)
		{
			TryAdd(It->GetActorLocation(), FVector(300.0, 300.0, 400.0), 1);
		}
		for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (It->ActorHasTag(TEXT("KG_BotSpot")))
			{
				TryAdd(It->GetActorLocation(), FVector(200.0, 200.0, 400.0), 1);
			}
		}
		if (const AKGMapInfo* Map = AKGMapInfo::Find(World))
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KGBotAnchor), false);
			for (const FKGMapRegion& R : Map->Regions)
			{
				if (R.Layer < 0 || R.Layer > 2)
				{
					continue;   // buildings are interiors, POIs are map-only
				}
				TArray<FVector2D> Points;
				RegionPoints(R, Points);
				for (const FVector2D& P : Points)
				{
					// Regions are 2D: find the walkable surface under the point (the first hit from above).
					FHitResult Hit;
					const FVector From(P, 6000.0);
					if (World->LineTraceSingleByChannel(Hit, From, FVector(P, -1000.0), ECC_Visibility, Params))
					{
						TryAdd(Hit.ImpactPoint, FVector(200.0, 200.0, 250.0), 1);
					}
				}
			}
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_BOT level: hub %s, %d anchors (%d tried, %d unreachable/off-mesh)"),
		       *Level.Hub.ToCompactString(), Level.Anchors.Num(), Tried, Dropped);
		return Level;
	}

	void HandleBotStats(const TArray<FString>& Args, UWorld* World)
	{
		AKGBotController::FStats& S = AKGBotController::Stats();
		int32 Bots = 0;
		TMap<FString, int32> ByStatus;
		for (TActorIterator<AKGBotController> It(World); It; ++It)
		{
			++Bots;
			FString Status = It->GetDevStatus();
			int32 Space = INDEX_NONE;
			if (Status.FindChar(TEXT(' '), Space) && !Status.StartsWith(TEXT("AI")) && !Status.StartsWith(TEXT("no ")))
			{
				Status.LeftInline(Space);   // "working DrawWater" -> "working"
			}
			ByStatus.FindOrAdd(Status)++;
		}
		FString Mix;
		for (const TPair<FString, int32>& Pair : ByStatus)
		{
			Mix += FString::Printf(TEXT("%s=%d "), *Pair.Key, Pair.Value);
		}
		UE_LOG(LogKillGodot, Display,
		       TEXT("KG_BOTSTATS bots=%d stuck=%d pathfail=%d arrivals=%d gathered=%d gather_avg=%.1fs gather_max=%.1fs ")
		       TEXT("chores=%d chore_avg=%.1fs rescues=%d | %s"),
		       Bots, S.StuckEvents, S.PathFailures, S.WanderArrivals, S.GatherArrivals,
		       S.GatherArrivals > 0 ? S.GatherSecondsSum / S.GatherArrivals : 0.0f, S.GatherSecondsMax, S.ChoresDone,
		       S.ChoresDone > 0 ? S.ChoreSecondsSum / S.ChoresDone : 0.0f, S.Rescues, *Mix);
		if (Args.Num() > 0 && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
		{
			S = AKGBotController::FStats();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GKGBotStatsCommand(
		TEXT("kg.BotStats"), TEXT("Log bot soak counters (stuck events, path failures, meeting arrivals). 'reset' clears them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleBotStats));
}

AKGBotController::FStats& AKGBotController::Stats()
{
	static FStats S;
	return S;
}

FVector AKGBotController::GetHubLocation(const UWorld* World)
{
	return LevelOf(World).Hub;
}

AKGBotController::AKGBotController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AKGBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	BotSeed = GetUniqueID();
	LastLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	bNavMove = false;
	bGathering = false;
	PauseRemaining = 0.5f + 2.0f * Hash01(BotSeed * 97u);   // don't all set off on the same frame
	if (!LevelOf(GetWorld()).bNav)
	{
		PickTarget();
	}
}

bool AKGBotController::IsBrainEnabled()
{
	return CVarBotAI.GetValueOnGameThread() != 0;
}

FString AKGBotController::GetDevStatus() const
{
	const AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	if (!Me)
	{
		return TEXT("no body");
	}
	if (Me->IsDead())
	{
		return TEXT("dead");
	}
	if (!IsBrainEnabled())
	{
		return TEXT("AI off");
	}
	if (const AKGTaskStation* Task = Me->GetActiveTask())
	{
		return FString::Printf(TEXT("working %s"), *Task->TaskId.ToString());
	}
	if (Prey.IsValid())
	{
		const APlayerState* PreyPS = Prey->GetPlayerState();
		return FString::Printf(TEXT("hunting %s"), PreyPS ? *PreyPS->GetPlayerName() : *Prey->GetName());
	}
	if (bGathering)
	{
		return bGatherArrived ? TEXT("at-meeting") : FString::Printf(TEXT("to-meeting %.0fs"), GatherElapsed);
	}
	if (ChoreTarget.IsValid())
	{
		return FString::Printf(TEXT("to-chore %s"), *ChoreTarget->TaskId.ToString());
	}
	if (VoteDelay >= 0.0f)
	{
		return TEXT("voting");
	}
	return PauseRemaining > 0.0f ? TEXT("idle") : TEXT("wandering");
}

bool AKGBotController::IsImpatient() const
{
	const AKGPlayerState* PS = GetPlayerState<AKGPlayerState>();
	if (!PS || PS->GetPrivateRoleId().IsNone())
	{
		return false;
	}
	const FKGRoleInfo* RoleInfo =
		FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId());
	return RoleInfo && RoleInfo->GetAlignment() == EKGAlignment::Impatient;
}

void AKGBotController::PickTarget()
{
	++Step;
	StuckTime = 0.0f;
	const FBotLevel& Level = LevelOf(GetWorld());
	const APawn* Me = GetPawn();
	if (Level.bNav && Level.Anchors.Num() > 0 && Me)
	{
		// Mostly the neighbourhood (anchors within 45 m), sometimes anywhere in town: bots spread over every district.
		const FVector From = Me->GetActorLocation();
		TArray<int32> Near;
		for (int32 i = 0; i < Level.Anchors.Num(); ++i)
		{
			const double D = FVector::Dist2D(From, Level.Anchors[i]);
			if (D > 500.0 && D < NearAnchorRadius)
			{
				Near.Add(i);
			}
		}
		const bool bLocal = Near.Num() > 0 && Hash01(BotSeed * 7u + Step * 131u) < 0.65f;
		const int32 Pick = bLocal ? Near[static_cast<int32>(Hash01(BotSeed * 7919u + Step * 104729u) * Near.Num()) % Near.Num()]
		                          : static_cast<int32>(Hash01(BotSeed * 31u + Step * 613u) * Level.Anchors.Num()) % Level.Anchors.Num();
		Target = Level.Anchors[Pick];
		FNavLocation Around;
		if (UNavigationSystemV1* Nav = NavOf(GetWorld()); Nav && Nav->GetRandomReachablePointInRadius(Target, 400.0f, Around))
		{
			Target = Around.Location;
		}
		const EPathFollowingRequestResult::Type R = MoveToLocation(Target, 100.0f, true, true, false, false);
		bNavMove = R == EPathFollowingRequestResult::RequestSuccessful;
		if (R == EPathFollowingRequestResult::Failed)
		{
			NotePathFailure();
		}
		else
		{
			PathFailStreak = 0;
		}
		ProgressTimer = 0.0f;
		ProgressFrom = From;
		PauseRemaining = 0.0f;
		return;
	}
	const float A = Hash01(BotSeed * 7919u + Step * 104729u) * 2.0f * PI;
	const float R = FMath::Sqrt(Hash01(BotSeed * 31u + Step * 613u)) * PlazaRadius;
	if (Hash01(BotSeed + Step * 17u) < 0.3f)
	{
		// Stroll down the harbour street instead.
		Target = FVector(Hash01(Step * 3u + BotSeed) * 400.0 - 200.0, 600.0 + Hash01(Step * 5u + BotSeed) * 4500.0, 0.0);
	}
	else
	{
		Target = FVector(PlazaCentre.X + R * FMath::Cos(A), PlazaCentre.Y + R * FMath::Sin(A), 0.0);
	}
	PauseRemaining = Hash01(BotSeed * 13u + Step) < 0.35f ? 1.5f + 4.0f * Hash01(Step * 29u + BotSeed) : 0.0f;
}

bool AKGBotController::CheckStuck(float DeltaSeconds)
{
	const APawn* Me = GetPawn();
	if (!Me)
	{
		return false;
	}
	ProgressTimer += DeltaSeconds;
	if (ProgressTimer < StuckSeconds)
	{
		return false;
	}
	const double Moved = FVector::Dist2D(Me->GetActorLocation(), ProgressFrom);
	ProgressFrom = Me->GetActorLocation();
	ProgressTimer = 0.0f;
	if (Moved >= StuckDistance)
	{
		return false;
	}
	++Stats().StuckEvents;
	const APlayerState* PS = GetPlayerState<APlayerState>();
	UE_LOG(LogKillGodot, Log, TEXT("KG_BOT stuck %s at %s (%s)"), PS ? *PS->GetPlayerName() : *GetName(),
	       *Me->GetActorLocation().ToCompactString(), *GetDevStatus());
	StuckStreak = FVector::Dist(Me->GetActorLocation(), LastStuckAt) < 150.0 ? StuckStreak + 1 : 1;
	LastStuckAt = Me->GetActorLocation();
	if (StuckStreak >= 4)
	{
		// Wedged for 16 s despite doors, side-steps and hops (a navmesh pocket between props): dev bots are put back on
		// the nearest level anchor instead of blocking a soak test forever. Counted as a rescue (kg.BotStats).
		const FBotLevel& Level = LevelOf(GetWorld());
		double Best = TNumericLimits<double>::Max();
		const FVector* Pick = nullptr;
		for (const FVector& A : Level.Anchors)
		{
			const double D = FVector::DistSquared(A, LastStuckAt);
			if (D > FMath::Square(300.0) && D < Best)
			{
				Best = D;
				Pick = &A;
			}
		}
		if (Pick && GetPawn()->TeleportTo(*Pick + FVector(0.0, 0.0, 100.0), Me->GetActorRotation()))
		{
			++Stats().Rescues;
			UE_LOG(LogKillGodot, Log, TEXT("KG_BOT rescue %s -> %s"), PS ? *PS->GetPlayerName() : *GetName(), *Pick->ToCompactString());
		}
		StuckStreak = 0;
		StopMovement();
		return true;
	}
	BeginUnstick();
	return true;
}

bool AKGBotController::OpenDoorAhead(double Radius, float MinDot)
{
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	if (!Me)
	{
		return false;
	}
	FVector Fwd = Me->GetVelocity().GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = Me->GetActorForwardVector().GetSafeNormal2D();
	}
	for (TActorIterator<AKGDoor> It(GetWorld()); It; ++It)
	{
		if (It->IsOpen())
		{
			continue;
		}
		FVector To = It->GetActorLocation() - Me->GetActorLocation();
		if (FMath::Abs(To.Z) > 200.0)
		{
			continue;
		}
		To.Z = 0.0;
		const double D = To.Size();
		// The door's pivot is its hinge: accept it right beside us, or ahead within the radius.
		if (D > Radius || (D > 100.0 && FVector::DotProduct(To / D, Fwd) < MinDot))
		{
			continue;
		}
		IKGInteractable::Execute_Interact(*It, Me);
		return true;
	}
	return false;
}

void AKGBotController::BeginUnstick()
{
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	if (!Me || OpenDoorAhead(220.0, 0.2f))
	{
		return;
	}
	// Back off sideways (and a little backwards) with a hop, then the caller repaths from a new spot.
	FVector Fwd = Me->GetActorForwardVector().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(Fwd, FVector::UpVector) * (Hash01(BotSeed + ++Step * 7u) < 0.5f ? 1.0 : -1.0);
	UnstickDir = (Side - Fwd * 0.5).GetSafeNormal();
	UnstickRemaining = 0.9f;
	StopMovement();
	Me->Jump();
}

void AKGBotController::NotePathFailure()
{
	++Stats().PathFailures;
	++PathFailStreak;
	PauseRemaining = FMath::Max(PauseRemaining, 0.5f);   // never hammer the pathfinder every tick
	const APawn* Me = GetPawn();
	if (Me && PathFailLogCooldown <= 0.0f)
	{
		PathFailLogCooldown = 10.0f;
		const APlayerState* PS = GetPlayerState<APlayerState>();
		UE_LOG(LogKillGodot, Log, TEXT("KG_BOT pathfail %s at %s (%s, streak %d)"), PS ? *PS->GetPlayerName() : *GetName(),
		       *Me->GetActorLocation().ToCompactString(), *GetDevStatus(), PathFailStreak);
	}
	if (PathFailStreak >= 3 && Me)
	{
		// Off the navmesh (knocked into the basin, up on a crate...): head for the nearest walkable ground. In the water
		// that is the nearest low navmesh (the quay steps, the slipway, a beach), not the quay top 2 m up the wall.
		const FVector At = Me->GetActorLocation();
		FVector Goal = LevelOf(GetWorld()).Hub;
		FNavLocation Near;
		UNavigationSystemV1* Nav = NavOf(GetWorld());
		const ACharacter* Body = Cast<ACharacter>(Me);
		const bool bSwimming = Body && Body->GetCharacterMovement() && Body->GetCharacterMovement()->IsSwimming();
		if (Nav && bSwimming && Nav->ProjectPointToNavigation(FVector(At.X, At.Y, 0.0), Near, FVector(3000.0, 3000.0, 150.0)))
		{
			Goal = Near.Location;
		}
		else if (Nav && Nav->ProjectPointToNavigation(At, Near, FVector(800.0, 800.0, 400.0)))
		{
			Goal = Near.Location;
		}
		UnstickDir = (Goal - At).GetSafeNormal2D();
		UnstickRemaining = bSwimming ? 3.0f : 1.2f;
		PathFailStreak = 0;
		if (Body)
		{
			const_cast<ACharacter*>(Body)->Jump();
		}
	}
}

bool AKGBotController::UpdateNavWander(float DeltaSeconds)
{
	if (!LevelOf(GetWorld()).bNav)
	{
		return false;
	}
	if (PauseRemaining > 0.0f)
	{
		PauseRemaining -= DeltaSeconds;
		return true;
	}
	if (!bNavMove)
	{
		PickTarget();
		return true;
	}
	if (GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		bNavMove = false;
		const APawn* Me = GetPawn();
		if (Me && FVector::Dist2D(Me->GetActorLocation(), Target) < 300.0)
		{
			++Stats().WanderArrivals;
		}
		// Linger a little: chat at the stall, look at the sea.
		PauseRemaining = Hash01(BotSeed * 13u + Step) < 0.6f ? 1.5f + 5.0f * Hash01(Step * 29u + BotSeed) : 0.0f;
		return true;
	}
	if (CheckStuck(DeltaSeconds))
	{
		StopMovement();
		bNavMove = false;
	}
	return true;
}

bool AKGBotController::UpdateGather(float DeltaSeconds, const AKGGameState* GS)
{
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	const FBotLevel& Level = LevelOf(GetWorld());
	if (!Me)
	{
		return false;
	}
	const double ToHub = FVector::Dist2D(Me->GetActorLocation(), Level.Hub);
	bool bWant = CVarBotGather.GetValueOnGameThread() == 1;
	if (GS && !bWant)
	{
		const EKGPhase Phase = GS->GetPhase();
		// Mirrors AKGGameMode::GetNextPhase: from day 2 on, the day ends in a town meeting.
		bWant = Phase == EKGPhase::Day && GS->GetDayIndex() > 1 && GS->GetPhaseRemaining() < GatherLeadSeconds;
		// The meeting itself gathers everyone at the gallows; only a straggler far from the square walks there.
		bWant |= (Phase == EKGPhase::Meeting || Phase == EKGPhase::Trial) && ToHub > 2500.0;
	}
	if (!bWant)
	{
		if (bGathering)
		{
			bGathering = false;
			bGatherArrived = false;
			bNavMove = false;
			StopMovement();
		}
		return false;
	}
	if (!bGathering)
	{
		bGathering = true;
		bGatherArrived = false;
		GatherElapsed = 0.0f;
		ChoreTarget = nullptr;
		++Step;
		// A loose ring round the fountain (4-9 m), each bot its own spot.
		const float A = Hash01(BotSeed * 211u + Step) * 2.0f * PI;
		const float R = 400.0f + 500.0f * Hash01(BotSeed * 17u + Step * 3u);
		GatherSpot = Level.Hub + FVector(R * FMath::Cos(A), R * FMath::Sin(A), 0.0);
		FNavLocation L;
		UNavigationSystemV1* Nav = NavOf(GetWorld());
		if (Nav && Nav->ProjectPointToNavigation(GatherSpot, L, FVector(300.0, 300.0, 600.0)))
		{
			GatherSpot = L.Location;
		}
		NavRepath = 0.0f;
		ProgressTimer = 0.0f;
		ProgressFrom = Me->GetActorLocation();
		if (Me->GetActiveTask())
		{
			UE_LOG(LogKillGodot, Verbose, TEXT("KG_BOT %s leaves its chore for the meeting"), *GetName());
		}
	}
	GatherElapsed += DeltaSeconds;
	if (!bGatherArrived && ToHub < GatherArriveRadius)
	{
		bGatherArrived = true;
		FStats& S = Stats();
		++S.GatherArrivals;
		S.GatherSecondsSum += GatherElapsed;
		S.GatherSecondsMax = FMath::Max(S.GatherSecondsMax, GatherElapsed);
		const APlayerState* PS = GetPlayerState<APlayerState>();
		UE_LOG(LogKillGodot, Log, TEXT("KG_BOT gather %s arrived in %.1fs"), PS ? *PS->GetPlayerName() : *GetName(), GatherElapsed);
	}
	if (!Level.bNav)
	{
		// No navmesh: steer straight at the hub.
		if (!bGatherArrived)
		{
			const FVector Dir = (Level.Hub - Me->GetActorLocation()).GetSafeNormal2D();
			SetControlRotation(FRotator(0.0, Dir.Rotation().Yaw, 0.0));
			Me->AddMovementInput(Dir, 1.0f);
		}
		return true;
	}
	const double ToSpot = FVector::Dist2D(Me->GetActorLocation(), GatherSpot);
	if (ToSpot < 150.0 || (bGatherArrived && (GetMoveStatus() == EPathFollowingStatus::Idle || ToSpot < 450.0)))
	{
		// Waiting for the meeting (the crowd jostles; close enough is fine): face the square's centre.
		if (GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			StopMovement();
		}
		const FVector Look = Level.Hub - Me->GetActorLocation();
		SetControlRotation(FRotator(0.0, Look.Rotation().Yaw, 0.0));
		return true;
	}
	NavRepath -= DeltaSeconds;
	if (NavRepath <= 0.0f || GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		NavRepath = 3.0f;
		if (MoveToLocation(GatherSpot, 80.0f, true, true, false, false) == EPathFollowingRequestResult::Failed)
		{
			NotePathFailure();
			GatherSpot = Level.Hub;   // the ring spot is off the mesh: head for the centre instead
		}
	}
	if (!bGatherArrived && CheckStuck(DeltaSeconds))
	{
		// Something in the way: pick another spot of the ring.
		const float A = Hash01(BotSeed * 211u + ++Step) * 2.0f * PI;
		GatherSpot = Level.Hub + FVector(500.0f * FMath::Cos(A), 500.0f * FMath::Sin(A), 0.0);
		NavRepath = 0.0f;
	}
	return true;
}

void AKGBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	if (!Me || Me->IsDead() || !IsBrainEnabled())
	{
		return;
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (GS && GS->GetPhase() == EKGPhase::Epilogue)
	{
		return;
	}
	PathFailLogCooldown -= DeltaSeconds;
	if (UnstickRemaining > 0.0f)
	{
		UnstickRemaining -= DeltaSeconds;
		Me->AddMovementInput(UnstickDir, 1.0f);
		ProgressTimer = 0.0f;
		ProgressFrom = Me->GetActorLocation();
		if (UnstickRemaining <= 0.0f)
		{
			bNavMove = false;   // repath from here
			ChoreRepath = 0.0f;
			NavRepath = 0.0f;
		}
		return;
	}
	// Doors on the way (house interiors, civic buildings): open them like a player would.
	DoorCheck -= DeltaSeconds;
	if (DoorCheck <= 0.0f)
	{
		DoorCheck = 0.5f;
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			// Only a door we are walking straight into: passers-by must not swing leaves open across the lane.
			OpenDoorAhead(130.0, 0.7f);
		}
	}

	// Impatient: keep the blade hidden until a back is exposed, then draw and stab.
	DecisionCooldown -= DeltaSeconds;
	if (IsImpatient() && DecisionCooldown <= 0.0f)
	{
		if (AKGCharacter* Victim = Me->FindBackstabTarget())
		{
			if (!Me->bHoldingAssassinBlade)
			{
				Me->ToggleDevBlade();
				DecisionCooldown = 0.5f;   // draw first, stab next decision
			}
			else
			{
				SetControlRotation((Victim->GetActorLocation() - Me->GetActorLocation()).Rotation());
				Me->Attack();
				DecisionCooldown = 2.0f;
			}
			return;
		}
		if (Me->bHoldingAssassinBlade)
		{
			Me->ToggleDevBlade();   // sheathe: walking around with a knife out is a confession
		}
		DecisionCooldown = 0.4f;
		// A hunt that goes nowhere (prey off the navmesh, or unreachable) is dropped after PreyGiveUpSeconds and that
		// prey skipped on the next pick, so the Impatient moves on instead of stalling the match.
		if (Prey.IsValid() && !Prey->IsDead())
		{
			PreyHuntSeconds += 0.4f;   // one decision step (DecisionCooldown cadence above)
			if (PreyHuntSeconds >= PreyGiveUpSeconds)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_BOT hunt stale %s -> %s after %.0fs, re-picking"), *Me->GetName(),
				       *Prey->GetName(), PreyHuntSeconds);
				StalePrey = Prey;
				Prey = nullptr;
				PreyHuntSeconds = 0.0f;
			}
		}
		// At night the Impatient hunt: lock onto the nearest villager and slip in behind them.
		if (GS && GS->GetPhase() == EKGPhase::Night && (!Prey.IsValid() || Prey->IsDead()))
		{
			Prey = nullptr;
			PreyHuntSeconds = 0.0f;
			double Best = TNumericLimits<double>::Max();
			for (int32 Pass = 0; Pass < 2 && !Prey.IsValid(); ++Pass)
			for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
			{
				const AKGPlayerState* PS = It->GetPlayerState<AKGPlayerState>();
				const FKGRoleInfo* R = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
				                                                             PS->GetPrivateRoleId()) : nullptr;
				if (*It == Me || It->IsDead() || !PS || (R && R->GetAlignment() == EKGAlignment::Impatient))
				{
					continue;
				}
				if (Pass == 0 && StalePrey.IsValid() && *It == StalePrey.Get())
				{
					continue;   // second pass only: when nobody else is left, the stale one is tried again
				}
				const double D = FVector::DistSquared2D(It->GetActorLocation(), Me->GetActorLocation());
				if (D < Best)
				{
					Best = D;
					Prey = *It;
				}
			}
		}
		if (!GS || GS->GetPhase() != EKGPhase::Night)
		{
			Prey = nullptr;
		}
	}
	const bool bNav = LevelOf(GetWorld()).bNav;
	if (Prey.IsValid() && !Prey->IsDead())
	{
		Target = Prey->GetActorLocation() - Prey->GetActorForwardVector() * 100.0;
		PauseRemaining = 0.0f;
		if (bNav && FVector::Dist2D(Target, Me->GetActorLocation()) > 600.0)
		{
			// Far away: follow the streets and stairs; the last metres are steered directly (below).
			NavRepath -= DeltaSeconds;
			if (NavRepath <= 0.0f || GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				NavRepath = 1.0f;
				MoveToLocation(Target, 60.0f, false, true, true, false);
				bNavMove = true;
			}
			return;
		}
		if (bNavMove)
		{
			StopMovement();
			bNavMove = false;
		}
	}
	UpdateVotes(DeltaSeconds, GS);
	if (!Prey.IsValid() && UpdateGather(DeltaSeconds, GS))
	{
		bNavMove = false;
		return;
	}
	if (!Prey.IsValid() && UpdateChores(DeltaSeconds, GS))
	{
		bNavMove = false;
		return;
	}
	if (GS && (GS->GetPhase() == EKGPhase::Meeting || GS->GetPhase() == EKGPhase::Trial))
	{
		if (bNavMove)
		{
			StopMovement();
			bNavMove = false;
		}
		return;   // stand in the crowd and listen
	}
	if (!Prey.IsValid() && UpdateNavWander(DeltaSeconds))
	{
		return;
	}

	// Direct steering: the last metres of a hunt, or the whole wander on levels without a navmesh.
	if (PauseRemaining > 0.0f)
	{
		PauseRemaining -= DeltaSeconds;
		return;
	}
	FVector To = Target - Me->GetActorLocation();
	To.Z = 0.0;
	if (To.SizeSquared() < FMath::Square(Prey.IsValid() ? 60.0 : 150.0))
	{
		if (!Prey.IsValid())
		{
			PickTarget();
		}
		return;
	}
	FVector Dir = To.GetSafeNormal();
	if (SidestepRemaining > 0.0f)
	{
		SidestepRemaining -= DeltaSeconds;
		Dir = (Dir + SidestepDir * 1.5).GetSafeNormal();
	}
	SetControlRotation(FRotator(0.0, Dir.Rotation().Yaw, 0.0));
	Me->AddMovementInput(Dir, 1.0f);

	// Walls and props: if we barely moved for a while, choose somewhere else.
	const double Moved = FVector::Dist2D(Me->GetActorLocation(), LastLocation);
	LastLocation = Me->GetActorLocation();
	StuckTime = Moved < 20.0 * DeltaSeconds ? StuckTime + DeltaSeconds : 0.0f;
	if (StuckTime > 1.2f)
	{
		if (Prey.IsValid())
		{
			// Walk around whatever is in the way instead of giving up the hunt.
			SidestepDir = FVector::CrossProduct(Dir, FVector::UpVector) * (Hash01(Step++ + BotSeed) < 0.5f ? 1.0 : -1.0);
			SidestepRemaining = 0.8f;
			StuckTime = 0.0f;
		}
		else
		{
			PickTarget();
		}
	}
}

void AKGBotController::UpdateVotes(float DeltaSeconds, const AKGGameState* GS)
{
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	AKGPlayerState* MyPS = GetPlayerState<AKGPlayerState>();
	if (!GS || !Me || !MyPS)
	{
		return;
	}
	const EKGPhase Phase = GS->GetPhase();
	if (Phase != EKGPhase::Meeting && Phase != EKGPhase::Trial)
	{
		VoteDelay = -1.0f;
		return;
	}
	if (VoteDelay < 0.0f)
	{
		VoteDelay = 4.0f + 10.0f * Hash01(BotSeed * 53u + GS->GetDayIndex() * 7u + static_cast<uint32>(Phase));
	}
	VoteDelay -= DeltaSeconds;
	if (VoteDelay > 0.0f)
	{
		return;
	}
	VoteDelay = 1000.0f;   // one decision per phase
	const bool bImpatient = IsImpatient();
	if (Phase == EKGPhase::Meeting && !MyPS->AccuseTarget)
	{
		// Villages bandwagon: join the most accused, otherwise pick someone. The Impatient never accuse each other.
		TMap<AKGPlayerState*, int32> Counts;
		TArray<AKGPlayerState*> Candidates;
		for (APlayerState* Raw : GS->PlayerArray)
		{
			AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (!PS || !PS->IsAlive() || PS == MyPS)
			{
				continue;
			}
			const FKGRoleInfo* R = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
			                                                      PS->GetPrivateRoleId());
			if (bImpatient && R && R->GetAlignment() == EKGAlignment::Impatient)
			{
				continue;
			}
			Candidates.Add(PS);
			if (PS->AccuseTarget)
			{
				Counts.FindOrAdd(PS->AccuseTarget)++;
			}
		}
		AKGPlayerState* Pick = nullptr;
		int32 Top = 0;
		for (const TPair<AKGPlayerState*, int32>& Pair : Counts)
		{
			if (Pair.Value > Top && Pair.Key != MyPS && Candidates.Contains(Pair.Key))
			{
				Top = Pair.Value;
				Pick = Pair.Key;
			}
		}
		if ((!Pick || Hash01(BotSeed * 3u + GS->GetDayIndex()) < 0.25f) && Candidates.Num() > 0)
		{
			Pick = Candidates[FMath::Min(Candidates.Num() - 1,
			                             static_cast<int32>(Hash01(BotSeed * 11u + GS->GetDayIndex() * 31u) * Candidates.Num()))];
		}
		if (Pick)
		{
			Me->AccusePlayer(Pick);
		}
	}
	else if (Phase == EKGPhase::Trial && MyPS != GS->OnTrial && MyPS->Verdict == 0)
	{
		const AKGPlayerState* Accused = GS->OnTrial.Get();
		const FKGRoleInfo* R = Accused ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
		                                                                Accused->GetPrivateRoleId()) : nullptr;
		const bool bAccusedImpatient = R && R->GetAlignment() == EKGAlignment::Impatient;
		// Impatient protect their own and happily hang villagers; villagers lean guilty.
		const bool bGuilty = bImpatient ? !bAccusedImpatient : Hash01(BotSeed * 17u + GS->GetDayIndex()) < 0.6f;
		bGuilty ? Me->VoteGuilty() : Me->VoteInnocent();
	}
}

bool AKGBotController::UpdateChores(float DeltaSeconds, const AKGGameState* GS)
{
	AKGCharacter* Me = Cast<AKGCharacter>(GetPawn());
	AKGPlayerState* PS = GetPlayerState<AKGPlayerState>();
	if (!Me || !PS || !GS || (GS->GetPhase() != EKGPhase::Day && GS->GetPhase() != EKGPhase::Dawn))
	{
		ChoreTarget = nullptr;
		FailedChores.Reset();
		return false;
	}
	if (Me->GetActiveTask())
	{
		StopMovement();
		return true;   // working: stand still until done
	}
	ChoreElapsed += DeltaSeconds;
	if (!ChoreTarget.IsValid() || !PS->HasOpenTask(ChoreTarget->TaskId))
	{
		if (ChoreTarget.IsValid())
		{
			FStats& S = Stats();
			++S.ChoresDone;
			S.ChoreSecondsSum += ChoreElapsed;
			UE_LOG(LogKillGodot, Log, TEXT("KG_BOT chore %s done %s in %.1fs"), *PS->GetPlayerName(),
			       *ChoreTarget->TaskId.ToString(), ChoreElapsed);
		}
		ChoreTarget = nullptr;
		// The nearest open chore this bot has not failed to reach lately (all failed: forget and retry).
		TArray<AKGTaskStation*> Open;
		for (TActorIterator<AKGTaskStation> It(GetWorld()); It; ++It)
		{
			if (PS->HasOpenTask(It->TaskId))
			{
				Open.Add(*It);
			}
		}
		FailedChores.RemoveAll([](const TWeakObjectPtr<AKGTaskStation>& W) { return !W.IsValid(); });
		if (Open.Num() > 0 && Open.Num() <= FailedChores.Num())
		{
			FailedChores.Reset();
		}
		double Best = TNumericLimits<double>::Max();
		for (AKGTaskStation* Station : Open)
		{
			if (FailedChores.Contains(Station) ||
			    (bFakedWorldChore && FKGWorldChoreCatalog::Get().IsWorldChore(Station->TaskId)))
			{
				continue;
			}
			// A little per-bot jitter so twenty bots don't queue at the same well.
			const double D = FVector::Dist(Me->GetActorLocation(), Station->GetActorLocation()) *
			                 (0.75 + 0.5 * Hash01(BotSeed * 41u + GetTypeHash(Station->TaskId)));
			if (D < Best)
			{
				Best = D;
				ChoreTarget = Station;
			}
		}
		if (!ChoreTarget.IsValid())
		{
			return false;   // all done: wander and chat
		}
		ChoreRepath = 0.0f;
		ChoreElapsed = 0.0f;
		ProgressTimer = 0.0f;
		ProgressFrom = Me->GetActorLocation();
	}
	// SPRINT-016 hook: world chores (Chores/WorldChores) walk their real steps - fetch, carry, work, deliver.
	if (UKGWorldChoreComponent* WorldChores = UKGWorldChoreComponent::FindFor(Me);
	    WorldChores && FKGWorldChoreCatalog::Get().IsWorldChore(ChoreTarget->TaskId))
	{
		if (WorldChores->BotDrive(this, ChoreTarget->TaskId, DeltaSeconds) == UKGWorldChoreComponent::EBot::Failed)
		{
			// A world chore is a long walk with an item in your arms: the Impatient fake one, then get back to hunting.
			bFakedWorldChore = bFakedWorldChore || IsImpatient();
			FailedChores.AddUnique(ChoreTarget);
			ChoreTarget = nullptr;
			StopMovement();
		}
		return true;
	}
	const FVector Goal = ChoreTarget->GetActorLocation();
	if (FVector::DistSquared2D(Me->GetActorLocation(), Goal) < FMath::Square(150.0))
	{
		StopMovement();
		Me->BeginTask(ChoreTarget.Get());
		return true;
	}
	ChoreRepath -= DeltaSeconds;
	if (ChoreRepath <= 0.0f)
	{
		ChoreRepath = 3.0f;
		if (MoveToLocation(Goal, 90.0f, true, true, true, true) == EPathFollowingRequestResult::Failed)
		{
			NotePathFailure();
			FailedChores.AddUnique(ChoreTarget);
			ChoreTarget = nullptr;   // unreachable: try another chore next tick
			return true;
		}
	}
	else if (GetMoveStatus() == EPathFollowingStatus::Idle && FVector::DistSquared2D(Me->GetActorLocation(), Goal) <
	                                                               FMath::Square(ChoreTarget->WorkRadius - 20.0f))
	{
		// Path ended as close as the navmesh allows and we are in range: start working.
		Me->BeginTask(ChoreTarget.Get());
		return true;
	}
	if (CheckStuck(DeltaSeconds))
	{
		FailedChores.AddUnique(ChoreTarget);
		ChoreTarget = nullptr;
		StopMovement();
	}
	return true;
}
