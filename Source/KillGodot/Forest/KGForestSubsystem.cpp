#include "Forest/KGForestSubsystem.h"

#include "AI/KGBotController.h"
#include "AIController.h"
#include "Character/KGCharacter.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "Combat/KGHealthComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Forest/KGForestActors.h"
#include "Forest/KGForestMap.h"
#include "World/KGFoliageField.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "KillGodot.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "Roles/KGRoleListGenerator.h"

static TAutoConsoleVariable<int32> CVarForestEnabled(TEXT("kg.Forest.Enabled"), 1,
	TEXT("SPRINT-033: forest threats (wolves, the Mist) on Morrowmere v2. 0 = off (chores and the camp fire stay)."));

static TAutoConsoleVariable<int32> CVarForestNight(TEXT("kg.Forest.Night"), 0,
	TEXT("Dev / shots: 1 = the forest plays by its night rules (gains, glowing eyes) whatever the phase."));

int32 UKGForestSubsystem::SessionDeaths = 0;
bool UKGForestSubsystem::bSmoke = false;
int32 UKGForestSubsystem::SessionPvEDeaths = 0;

namespace
{
	FVector2D M2(const FVector& Cm) { return FVector2D(Cm.X, Cm.Y) / 100.0f; }
	FVector Cm3(const FVector2D& M, float Z) { return FVector(M.X * 100.0f, M.Y * 100.0f, Z); }

	/** The ground under a point: terrain and props, never the tree canopies (AKGFoliageField) or pawns. */
	FVector Ground(const UWorld* World, const FVector& Hint, float Up = 3000.0f)
	{
		static TWeakObjectPtr<const UWorld> CachedFor;
		static TArray<TWeakObjectPtr<AActor>> Foliage;
		if (World && CachedFor.Get() != World)
		{
			CachedFor = World;
			Foliage.Reset();
			for (TActorIterator<AKGFoliageField> It(const_cast<UWorld*>(World)); It; ++It)
			{
				Foliage.Add(*It);
			}
		}
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("KGForestGround"), false);
		for (const TWeakObjectPtr<AActor>& F : Foliage)
		{
			if (F.IsValid())
			{
				Q.AddIgnoredActor(F.Get());
			}
		}
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		if (World && World->LineTraceSingleByObjectType(Hit, Hint + FVector(0, 0, Up), Hint - FVector(0, 0, 6000), Obj, Q))
		{
			return Hit.ImpactPoint;
		}
		return Hint;
	}

	bool IsThreat(const AKGPlayerState* PS)
	{
		const FKGRoleInfo* R = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId()) : nullptr;
		return R && (R->GetAlignment() == EKGAlignment::Impatient || R->Category == EKGRoleCategory::SoloKilling);
	}

	bool IsImpatient(const AKGPlayerState* PS)
	{
		const FKGRoleInfo* R = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId()) : nullptr;
		return R && R->GetAlignment() == EKGAlignment::Impatient;
	}

	AKGCharacter* BodyOf(const AKGPlayerState* PS)
	{
		AKGCharacter* C = PS ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
		return C && !C->IsDead() ? C : nullptr;
	}

	struct FBotMem
	{
		FString LoggedStage;
		int32 SignDecidedDay = -1;
		float Repath = 0.0f;
		bool bLeaveVigil = false;
		bool bVigilLogged = false;
	};
	TMap<TWeakObjectPtr<AKGBotController>, FBotMem>& BotMem()
	{
		static TMap<TWeakObjectPtr<AKGBotController>, FBotMem> M;
		return M;
	}
}

// ================================================================================================ lifecycle
bool UKGForestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

UKGForestSubsystem* UKGForestSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGForestSubsystem>() : nullptr;
}

bool UKGForestSubsystem::IsEnabled()
{
	return CVarForestEnabled.GetValueOnGameThread() != 0;
}

void UKGForestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FKGForestMap& Map = FKGForestMap::Get();
	bActive = Map.IsValid() && InWorld.GetMapName().Contains(TEXT("L_Morrowmere_v2"));
	bServer = InWorld.GetNetMode() != NM_Client;
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST setup map=%s active=%d server=%d raster=%dx%d dens=%d trails=%d enabled=%d"),
	       *InWorld.GetMapName(), bActive ? 1 : 0, bServer ? 1 : 0, Map.NX, Map.NY, Map.Dens.Num(), Map.Trails.Num(), IsEnabled() ? 1 : 0);
}

void UKGForestSubsystem::SpawnWorld()
{
	UWorld* W = GetWorld();
	bSpawned = true;
	if (!bServer)
	{
		return;
	}
	const FKGForestMap& Map = FKGForestMap::Get();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	W->SpawnActor<AKGForestDirector>(AKGForestDirector::StaticClass(), FTransform::Identity, P);
	const FVector Camp = Ground(W, Cm3(Map.Camp, Map.CampZ * 100.0f + 200.0f));
	Campfire = W->SpawnActor<AKGCampfire>(AKGCampfire::StaticClass(), FTransform(Camp), P);
	// The vigil book stands by the notice board on the Fountain Square (landmark notice_board, layout v2: 4, -7 m).
	const FVector BookAt = Ground(W, FVector(530.0f, -860.0f, 800.0f));
	Book = W->SpawnActor<AKGVigilBook>(AKGVigilBook::StaticClass(), FTransform(FRotator(0.0f, 90.0f, 0.0f), BookAt), P);
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	CampWoodpileSpot = Cat.AnchorIndex(TEXT("forest_camp_woodpile"));
	for (int32 i = 0; i < Cat.Anchors.Num(); ++i)
	{
		if (Cat.Anchors[i].Id.ToString().StartsWith(TEXT("forest_lantern_")))
		{
			ForestLampSpots.Add(i);
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST world camp=%s book=%s woodpile_spot=%d lamp_spots=%d"), *Camp.ToCompactString(),
	       *BookAt.ToCompactString(), CampWoodpileSpot, ForestLampSpots.Num());
}

void UKGForestSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W || !W->HasBegunPlay())
	{
		return;
	}
	if (!bSpawned)
	{
		SpawnWorld();
	}
	if (bServer)
	{
		const double T0 = FPlatformTime::Seconds();
		ServerTick(DeltaTime);
		StatAccumMs += float((FPlatformTime::Seconds() - T0) * 1000.0);
		if (++StatFrames >= 600)
		{
			UE_LOG(LogKillGodot, Verbose, TEXT("KG_FOREST_STAT %.3f ms/frame avg, wolves %d, tongues %d"), StatAccumMs / StatFrames,
			       ActiveWolves(), ActiveTongues());
			StatAccumMs = 0.0f;
			StatFrames = 0;
		}
	}
}

// ================================================================================================ queries
UKGForestSubsystem::FTrack* UKGForestSubsystem::FindTrack(const AKGPlayerState* PS)
{
	return Tracks.FindByPredicate([PS](const FTrack& T) { return T.PS.Get() == PS; });
}

int32 UKGForestSubsystem::NumPlayers() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->PlayerArray.Num() : 0;
}

void UKGForestSubsystem::CountAlive(int32& OutAlive, int32& OutThreats) const
{
	OutAlive = OutThreats = 0;
	const AGameStateBase* GS = GetWorld()->GetGameState();
	for (APlayerState* Raw : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		if (PS && PS->IsAlive())
		{
			++OutAlive;
			OutThreats += IsThreat(PS) ? 1 : 0;
		}
	}
}

bool UKGForestSubsystem::IsLethalNow() const
{
	int32 Alive, Threats;
	CountAlive(Alive, Threats);
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	const bool bEnded = GS && (GS->bHasWinner || GS->GetPhase() == EKGPhase::Epilogue);
	return !bEnded && FKGForestRules::IsPvELethal(NumPlayers(), Alive, Threats, GS ? GS->DayIndex : 0, SessionPvEDeaths, SessionDeaths);
}

bool UKGForestSubsystem::IsLitLightAt(const FVector2D& M) const
{
	const FKGForestMap& Map = FKGForestMap::Get();
	for (const FVector2D& L : Map.Lanterns)
	{
		if (FVector2D::DistSquared(L, M) <= FMath::Square(KGForest::LanternSafe))
		{
			return true;
		}
	}
	if (const AKGCampfire* F = Campfire.Get())
	{
		const float R = FKGForestRules::FireSafeRadius(F->Fuel);
		if (R > 0.0f && FVector2D::DistSquared(M2(F->GetActorLocation()), M) <= R * R)
		{
			return true;
		}
	}
	if (const AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(GetWorld()))
	{
		const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
		for (const int32 i : ForestLampSpots)
		{
			const FKGSpotState* S = D->GetSpot(i);
			if (S && S->bLit && Cat.Anchors.IsValidIndex(i) &&
			    FVector2D::DistSquared(M2(Cat.Anchors[i].Location), M) <= FMath::Square(KGForest::LanternSafe))
			{
				return true;
			}
		}
	}
	return false;
}

bool UKGForestSubsystem::SafeGoal(const FVector& From, FVector& OutGoal) const
{
	const FKGForestMap& Map = FKGForestMap::Get();
	FVector2D S;
	if (!Map.IsValid() || Map.IsStaticSafe(M2(From)) || !Map.NearestSafe(M2(From), S))
	{
		return false;
	}
	// a step onto the path, not its edge
	const FVector2D Dir = (S - M2(From)).GetSafeNormal();
	OutGoal = Ground(GetWorld(), Cm3(S + Dir * 1.5f, From.Z + 200.0f)) + FVector(0, 0, 50.0f);
	return true;
}

int32 UKGForestSubsystem::ActiveWolves() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AKGWolf>& W : Wolves)
	{
		N += W.IsValid() && W->bAwake ? 1 : 0;
	}
	return N;
}

int32 UKGForestSubsystem::ActiveTongues() const
{
	int32 N = 0;
	for (const FTrack& T : Tracks)
	{
		N += T.Tongue.IsValid() ? 1 : 0;
	}
	return N;
}

int32 UKGForestSubsystem::CampLogs() const
{
	const AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(GetWorld());
	const FKGSpotState* S = D ? D->GetSpot(CampWoodpileSpot) : nullptr;
	return S ? S->Count : 0;
}

bool UKGForestSubsystem::TakeCampLog()
{
	AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(GetWorld());
	FKGSpotState* S = D ? D->AuthMutableSpot(CampWoodpileSpot) : nullptr;
	if (!S || S->Count == 0)
	{
		return false;
	}
	--S->Count;
	D->AuthDirty();
	return true;
}

void UKGForestSubsystem::Tell(AKGPlayerState* PS, const FString& Text, float Seconds)
{
	if (FTrack* T = FindTrack(PS))
	{
		if (AKGForestPlayerInfo* I = T->Info.Get())
		{
			I->Line = Text;
			I->LineUntil = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + Seconds;
			I->ForceNetUpdate();
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST tell %s: %s"), PS ? *PS->GetPlayerName() : TEXT("?"), *Text);
}

void UKGForestSubsystem::NotePvEDeath(AKGPlayerState* Victim, FName Cause)
{
	++SessionPvEDeaths;
	PvEVictims.Add(Victim);
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST pve_death victim=%s cause=%s session_pve=%d session_deaths=%d"),
	       Victim ? *Victim->GetPlayerName() : TEXT("?"), *Cause.ToString(), SessionPvEDeaths, SessionDeaths + 1);
	// TODO(020b): Event Ledger entry (forest death, sector, witnesses) instead of the log line.
}

// ================================================================================================ server tick
void UKGForestSubsystem::ServerTick(float Dt)
{
	UpdateDeaths();
	UpdateCampAndVigil(Dt);
	Accum += Dt;
	if (Accum >= 0.25f)
	{
		UpdateTracks(Accum);
		Accum = 0.0f;
	}
	UpdateWolves(Dt);
	UpdateMist(Dt);
	if (bSmoke)
	{
		TickSmoke(Dt);
	}
}

void UKGForestSubsystem::TickSmoke(float Dt)
{
	SmokeTimer += Dt;
	FTrack* T = SmokePS.IsValid() ? FindTrack(SmokePS.Get()) : nullptr;
	switch (SmokeState)
	{
	case 0:
		for (FTrack& C : Tracks)
		{
			const APlayerController* PC = C.PS.IsValid() ? Cast<APlayerController>(C.PS->GetOwningController()) : nullptr;
			if (PC && !PC->IsLocalController() && BodyOf(C.PS.Get()) && C.Info.IsValid() && SmokeTimer > 3.0f)
			{
				SmokePS = C.PS;
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_SMOKE wolf start remote=%s: %s"), *C.PS->GetPlayerName(),
				       *DevTest(TEXT("wolf"), C.PS.Get(), true));
				SmokeState = 1;
				SmokeTimer = 0.0f;
				break;
			}
		}
		break;
	case 1:
		if (!T || T->Wolf.Bites >= 2 || SmokeTimer > 70.0f)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_SMOKE wolf end bites=%d first_stamp=%.1f first_bite=%.1f after=%.1f s"),
			       T ? T->Wolf.Bites : -1, T ? T->FirstStampTime : -1.0f, T ? T->FirstBiteTime : -1.0f, SmokeTimer);
			if (T)
			{
				DevTest(TEXT("stop"), SmokePS.Get(), false);
				T->Wolf = FKGWolfTrack();
			}
			SmokeState = 2;
			SmokeTimer = 0.0f;
		}
		break;
	case 2:
		if (SmokeTimer > 4.0f && T)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_SMOKE mist start: %s"), *DevTest(TEXT("mist"), SmokePS.Get(), true));
			SmokeState = 3;
			SmokeTimer = 0.0f;
			bSmokeSawTongue = false;
		}
		break;
	case 3:
		if (T && T->Tongue.IsValid())
		{
			bSmokeSawTongue = true;
			if (T->Tongue->bFading && SmokeMist.IsEmpty())
			{
				SmokeMist = T->Mist.CoreSecs > 0.0f ? TEXT("caught") : TEXT("escaped");
			}
		}
		if ((bSmokeSawTongue && T && !T->Tongue.IsValid()) || SmokeTimer > 90.0f || !T)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST_SMOKE_DONE mist=%s tongue=%d safe_dist=%.1f after=%.1f s"),
			       SmokeMist.IsEmpty() ? TEXT("timeout") : *SmokeMist, bSmokeSawTongue ? 1 : 0, T ? T->SafeDist : -1.0f, SmokeTimer);
			SmokeState = 4;
			bSmoke = false;
		}
		break;
	default:
		break;
	}
}

void UKGForestSubsystem::UpdateDeaths()
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS)
	{
		return;
	}
	for (APlayerState* Raw : GS->PlayerArray)
	{
		AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		if (PS && !PS->IsAlive() && !KnownDead.Contains(PS))
		{
			KnownDead.Add(PS);
			++SessionDeaths;
		}
		else if (PS && PS->IsAlive() && KnownDead.Contains(PS))
		{
			KnownDead.Remove(PS);   // revived (dev) or a new match
		}
	}
}

void UKGForestSubsystem::UpdateTracks(float Dt)
{
	UWorld* W = GetWorld();
	const AKGGameState* GS = W->GetGameState<AKGGameState>();
	if (!GS)
	{
		return;
	}
	const FKGForestMap& Map = FKGForestMap::Get();
	const EKGPhase Phase = GS->GetPhase();
	const bool bPhaseActive = Phase == EKGPhase::Day || Phase == EKGPhase::Night;
	const bool bNight = Phase == EKGPhase::Night || CVarForestNight.GetValueOnGameThread() != 0;
	const bool bEnabled = IsEnabled();
	const float Now = GS->GetServerWorldTimeSeconds();
	// tracks for every player (bots too); info actors for humans only
	for (APlayerState* Raw : GS->PlayerArray)
	{
		AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		if (PS && !FindTrack(PS))
		{
			FTrack& T = Tracks.AddDefaulted_GetRef();
			T.PS = PS;
		}
	}
	Tracks.RemoveAll([](const FTrack& T) { return !T.PS.IsValid(); });
	TArray<AKGCharacter*> Bodies;
	for (const FTrack& T : Tracks)
	{
		if (AKGCharacter* B = BodyOf(T.PS.Get()))
		{
			Bodies.Add(B);
		}
	}
	for (FTrack& T : Tracks)
	{
		AKGPlayerState* PS = T.PS.Get();
		if (!T.Info.IsValid())
		{
			if (APlayerController* PC = Cast<APlayerController>(PS->GetOwningController()))
			{
				FActorSpawnParameters P;
				P.Owner = PC;
				T.Info = W->SpawnActor<AKGForestPlayerInfo>(AKGForestPlayerInfo::StaticClass(), FTransform::Identity, P);
			}
		}
		AKGCharacter* Body = BodyOf(PS);
		AKGForestPlayerInfo* Info = T.Info.Get();
		if (!Body)
		{
			T.Wolf = FKGWolfTrack();
			T.Mist = FKGMistTrack();
			T.ForceWolfGain = 0.0f;
			T.bForceMist = false;
			if (Info)
			{
				Info->WolfStage = Info->MistStage = 0;
				Info->Frost = 0.0f;
			}
			continue;
		}
		const FVector Pos = Body->GetActorLocation();
		const FVector2D M = M2(Pos);
		T.Band = Map.BandAt(M);
		T.Sector = Map.SectorAt(M);
		T.SafeDist = Map.SafeDistance(M);
		T.bSafe = Map.IsStaticSafe(M) || IsLitLightAt(M);
		T.Group = 0;
		for (const AKGCharacter* O : Bodies)
		{
			T.Group += FVector::DistSquared2D(O->GetActorLocation(), Pos) <= FMath::Square(KGForest::GroupRadius * 100.0f) ? 1 : 0;
		}
		// AFK: no movement and no turning in the Middle / Deep band
		const FRotator Rot = Body->GetBaseAimRotation();
		const bool bIdle = FVector::DistSquared(Pos, T.LastPos) < 400.0f && Rot.Equals(T.LastRot, 0.5f);
		T.Idle = bIdle ? T.Idle + Dt : 0.0f;
		T.LastPos = Pos;
		T.LastRot = Rot;
		const bool bForced = T.ForceWolfGain > 0.0f || T.bForceMist;
		const bool bDeepish = T.Band == EKGForestBand::Middle || T.Band == EKGForestBand::Deep;
		const bool bAfkFrozen = !bForced && bDeepish && T.Idle >= KGForest::AfkFreeze;
		const bool bLive = bEnabled && (bPhaseActive || bForced);
		if (bLive && !bForced && bDeepish && T.Idle >= KGForest::AfkReturn)
		{
			WallReturn(T, Body, TEXT("afk"));
			continue;
		}
		// the Mist Wall stands on the land side only (north of the coast line), never out at sea
		if (bEnabled && !Map.InsideBoundary(M) && M.Y < 40.0f && Map.IsValid() && !T.bWallReturning)
		{
			WallReturn(T, Body, TEXT("wall"));
			continue;
		}
		// ---- wolves ----
		FKGWolfInputs In;
		In.Band = T.Band;
		In.bSafe = T.bSafe;
		In.bNight = bNight;
		In.DayIndex = GS->DayIndex;
		In.GroupSize = FMath::Max(1, T.Group);
		In.Health = Body->GetHealth() ? Body->GetHealth()->GetHealth() : 100.0f;
		const bool bLiveWolf = bEnabled && (bPhaseActive || T.ForceWolfGain > 0.0f);   // a forced Mist test keeps the wolves out
		float Gain = bLiveWolf && !bAfkFrozen ? FKGForestRules::WolfGain(In) : -KGForest::SafeDecay;
		if (T.ForceWolfGain > 0.0f && bEnabled)
		{
			Gain = T.bSafe ? -KGForest::SafeDecay : T.ForceWolfGain;   // dev: the path still saves you
		}
		const bool bHowlLive = SectorHowlUntil.FindRef(T.Sector) > Now;
		const EKGWolfStage Before = T.Wolf.Stage;
		const FKGWolfStep Step = FKGForestRules::StepWolf(T.Wolf, Gain, Dt, bHowlLive);
		if (Step.bStamped)
		{
			T.FirstStampTime = Now;
			if (Step.bHowl && T.Sector != 0)
			{
				SectorHowlUntil.Add(T.Sector, Now + KGForest::HowlSectorCooldown);
			}
			if (AKGForestDirector* D = AKGForestDirector::Get(W))
			{
				// the howl comes from the pack's side of the forest: 60 m further out from the player
				const FVector From = Pos + FVector(M.GetSafeNormal().X, M.GetSafeNormal().Y, 0.0f) * 6000.0f;
				if (Step.bHowl)
				{
					D->MulticastHowl(From, static_cast<uint8>(T.Sector));
				}
				else
				{
					D->MulticastCue(Pos + FVector(800.0f, 0.0f, 0.0f), 0);   // a close growl instead of a second howl
				}
			}
		}
		if (T.Wolf.Stage != Before)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST player=%s stage=%s interest=%.0f band=%s sector=%s t=%.1f stamp_age=%.1f bot=%d"),
			       *PS->GetPlayerName(), KGWolfStageName(T.Wolf.Stage), T.Wolf.Interest, KGForestBandName(T.Band),
			       *FKGForestMap::SectorName(T.Sector), Now, T.Wolf.StampAge, PS->IsABot() ? 1 : 0);
			if (Step.bEyes)
			{
				Tell(PS, TEXT("Eyes in the dark. Walk back to the path - now."), 3.0f);
			}
		}
		// ---- the Mist (notice) ----
		const bool bMistOn = bLive && (T.bForceMist ||
			FKGForestRules::MistNoticeActive(T.Band, FMath::Max(1, T.Group), bAfkFrozen, GS->DayIndex));
		const EKGMistStage MistBefore = T.Mist.Stage;
		if (FKGForestRules::StepMistNotice(T.Mist, bMistOn && !T.bSafe, Dt, bNight || T.bForceMist, false))
		{
			if (ActiveTongues() < FKGForestRules::MistCap(NumPlayers()) || T.bForceMist)
			{
				FVector2D Safe = M;
				Map.NearestSafe(M, Safe);
				const FVector View = Rot.Vector();
				const FVector2D At = FKGForestRules::MistSpawn(M, Safe, FVector2D(View.X, View.Y));
				const FVector Spawn = Ground(W, Cm3(At, Pos.Z + 500.0f));
				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AKGMistTongue* Tg = W->SpawnActor<AKGMistTongue>(AKGMistTongue::StaticClass(), FTransform(Spawn), P))
				{
					Tg->TargetPS = PS;
					T.Tongue = Tg;
					UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST mist spawn player=%s dist=%.1f m safe_dist=%.1f m t=%.1f frost_age=%.1f"),
					       *PS->GetPlayerName(), FVector::Dist2D(Spawn, Pos) / 100.0f, T.SafeDist, Now, T.Mist.FrostAge);
					if (AKGForestDirector* D = AKGForestDirector::Get(W))
					{
						D->MulticastCue(Spawn, 2);
					}
				}
			}
			else
			{
				T.Mist.Stage = EKGMistStage::Frost;   // cap reached: hold at the frost
				T.Mist.Notice = 99.0f;
			}
		}
		if (T.Mist.Stage != MistBefore)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST player=%s mist=%d notice=%.0f band=%s t=%.1f bot=%d"), *PS->GetPlayerName(),
			       int32(T.Mist.Stage), T.Mist.Notice, KGForestBandName(T.Band), Now, PS->IsABot() ? 1 : 0);
			if (T.Mist.Stage == EKGMistStage::Frost)
			{
				Tell(PS, TEXT("Your breath fogs. Something cold is watching you."), 4.0f);
			}
		}
		// ---- owner-only info ----
		if (Info)
		{
			Info->Band = static_cast<uint8>(T.Band);
			Info->WolfStage = static_cast<uint8>(T.Wolf.Stage);
			Info->MistStage = static_cast<uint8>(T.Mist.Stage);
			float Frost = 0.0f;
			if (T.Mist.Stage == EKGMistStage::Frost)
			{
				Frost = FMath::Clamp((T.Mist.Notice - KGForest::MistFrostAt) / (100.0f - KGForest::MistFrostAt), 0.0f, 1.0f) * 0.55f;
			}
			else if (const AKGMistTongue* Tg = T.Tongue.Get())
			{
				const float D = FVector::Dist2D(Tg->GetActorLocation(), Pos) / 100.0f;
				Frost = 0.55f + 0.45f * FMath::Clamp(1.0f - D / KGForest::MistSpawnBehind, 0.0f, 1.0f);
			}
			Info->Frost = Frost;
			FVector2D S;
			const bool bHas = !T.bSafe && Map.NearestSafe(M, S);
			const FVector2D Dir = bHas ? (S - M).GetSafeNormal() : FVector2D::ZeroVector;
			Info->SafeDir = FVector(Dir.X, Dir.Y, 0.0f);
			Info->SafeDist = bHas ? FVector2D::Distance(S, M) : 0.0f;
			Info->Vigil = IsVigilSigner(PS) ? (bVigilNight && Campfire.IsValid() &&
				FVector::Dist2D(Campfire->GetActorLocation(), Pos) <= KGForest::VigilRing * 100.0f ? 2 : 1) : 0;
		}
	}
}

// ================================================================================================ wolves
bool UKGForestSubsystem::WolfCellOk(const FVector& WorldCm) const
{
	const FKGForestMap& Map = FKGForestMap::Get();
	const FVector2D M = M2(WorldCm);
	const EKGForestBand B = Map.BandAt(M);
	return (B == EKGForestBand::Edge || B == EKGForestBand::Middle || B == EKGForestBand::Deep) && !IsLitLightAt(M);
}

FVector UKGForestSubsystem::ClampWolfGoal(const FVector& From, const FVector& Goal) const
{
	const float Len = FVector::Dist2D(From, Goal);
	FVector Last = From;
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / 100.0f));
	for (int32 i = 1; i <= Steps; ++i)
	{
		const FVector P = FMath::Lerp(From, Goal, float(i) / Steps);
		if (!WolfCellOk(P))
		{
			break;
		}
		Last = P;
	}
	return Last;
}

AKGWolf* UKGForestSubsystem::WakeWolf(int32 Pack, int32 Slot, const FVector& Den)
{
	for (const TWeakObjectPtr<AKGWolf>& Wk : Wolves)
	{
		if (AKGWolf* Wf = Wk.Get(); Wf && Wf->Pack == Pack && Wf->Slot == Slot)
		{
			if (!Wf->bAwake)
			{
				Wf->SetActorLocation(Den + FVector(150.0f * Slot, 80.0f * Slot, 60.0f));
				Wf->SetAwake(true);
			}
			return Wf;
		}
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKGWolf* Wf = GetWorld()->SpawnActor<AKGWolf>(AKGWolf::StaticClass(), FTransform(Den + FVector(150.0f * Slot, 80.0f * Slot, 60.0f)), P);
	if (Wf)
	{
		Wf->Pack = Pack;
		Wf->Slot = Slot;
		Wf->Den = Den;
		Wf->OrbitPhase = Slot * 2.1f;
		Wf->SetAwake(true);
		Wolves.Add(Wf);
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST wolf spawn pack=%d slot=%d den=%s"), Pack, Slot, *Den.ToCompactString());
	}
	return Wf;
}

void UKGForestSubsystem::UpdateWolves(float Dt)
{
	UWorld* W = GetWorld();
	const AKGGameState* GS = W->GetGameState<AKGGameState>();
	const FKGForestMap& Map = FKGForestMap::Get();
	if (!GS || Map.Dens.Num() == 0)
	{
		return;
	}
	const int32 N = FMath::Max(1, NumPlayers());
	const bool bNight = GS->GetPhase() == EKGPhase::Night || GS->GetPhase() == EKGPhase::Dawn || CVarForestNight.GetValueOnGameThread() != 0;
	// packs: one den per pack in an open sector (the forced dev test may use any den)
	TArray<const FKGForestDen*> Open;
	for (const FKGForestDen& D : Map.Dens)
	{
		if (FKGForestRules::IsSectorOpen(D.Sector, N))
		{
			Open.Add(&D);
		}
	}
	const int32 NumPacks = FMath::Min(FKGForestRules::PackCount(N), FMath::Max(1, Open.Num()));
	const int32 PackSize = FKGForestRules::PackSize(N);
	const int32 Cap = FKGForestRules::WolfCap(N);
	// each pack takes the most interested player with a live telegraph (nearest den wins the player)
	TSet<const FTrack*> Taken;
	for (int32 p = 0; p < NumPacks; ++p)
	{
		FTrack* Best = nullptr;
		for (FTrack& T : Tracks)
		{
			if (T.Wolf.Stage >= EKGWolfStage::Howl && !Taken.Contains(&T) && BodyOf(T.PS.Get()) &&
			    (!Best || T.Wolf.Interest > Best->Wolf.Interest))
			{
				Best = &T;
			}
		}
		if (Best)
		{
			Taken.Add(Best);
			PackTarget.Add(p, Best->PS);
		}
		else
		{
			PackTarget.Remove(p);
		}
	}
	int32 Awake = ActiveWolves();
	for (int32 p = 0; p < NumPacks; ++p)
	{
		const TWeakObjectPtr<AKGPlayerState>* TargetPS = PackTarget.Find(p);
		FTrack* T = TargetPS ? FindTrack(TargetPS->Get()) : nullptr;
		AKGCharacter* Prey = T ? BodyOf(T->PS.Get()) : nullptr;
		// the pack's den: the open den nearest to its prey (else the p-th open den)
		const FKGForestDen* Den = Open.IsValidIndex(p) ? Open[p] : &Map.Dens[0];
		if (Prey)
		{
			float BestD = 1e12f;
			for (const FKGForestDen* D : (Open.Num() ? Open : TArray<const FKGForestDen*>{&Map.Dens[0]}))
			{
				const float Dd = FVector2D::DistSquared(D->At, M2(Prey->GetActorLocation()));
				if (Dd < BestD)
				{
					BestD = Dd;
					Den = D;
				}
			}
		}
		const FVector DenAt = Ground(W, Cm3(Den->At, 3000.0f));
		for (int32 s = 0; s < PackSize; ++s)
		{
			AKGWolf* Wf = nullptr;
			for (const TWeakObjectPtr<AKGWolf>& Wk : Wolves)
			{
				if (Wk.IsValid() && Wk->Pack == p && Wk->Slot == s)
				{
					Wf = Wk.Get();
				}
			}
			if (Prey && (!Wf || !Wf->bAwake))
			{
				if (Awake >= Cap)
				{
					continue;
				}
				Wf = WakeWolf(p, s, DenAt);
				++Awake;
				if (Wf)
				{
					Wf->Den = DenAt;
				}
			}
			if (!Wf || !Wf->bAwake)
			{
				continue;
			}
			AAIController* AI = Cast<AAIController>(Wf->GetController());
			UCharacterMovementComponent* Move = Wf->GetCharacterMovement();
			Wf->bEyesLit = bNight;
			Wf->Repath -= Dt;
			Wf->BackOff = FMath::Max(0.0f, Wf->BackOff - Dt);
			Wf->RepelLeft = FMath::Max(0.0f, Wf->RepelLeft - Dt);
			const FVector WPos = Wf->GetActorLocation();
			FVector Goal = Wf->Den;
			uint8 Gait = 2;
			if (!WolfCellOk(WPos) && Wf->RepelLeft <= 0.0f)
			{
				Wf->RepelLeft = 5.0f;   // strayed onto a village / lit cell: back to the trees
			}
			if (!Prey || Wf->RepelLeft > 0.0f)
			{
				Goal = Wf->Den;
				Gait = Prey ? 4 : 2;
				if (!Prey && FVector::Dist2D(WPos, Wf->Den) < 400.0f)
				{
					Wf->SetAwake(false);   // back in the den: asleep, cost 0
					if (AI)
					{
						AI->StopMovement();
					}
					continue;
				}
			}
			else
			{
				const FVector PPos = Prey->GetActorLocation();
				const float Dist = FVector::Dist2D(WPos, PPos) / 100.0f;
				FVector Away = (WPos - PPos).GetSafeNormal2D();
				if (Away.IsNearlyZero())
				{
					Away = FVector(1, 0, 0);
				}
				const int32 Lunger = T->Wolf.Bites % PackSize;
				if (Wf->BackOff > 0.0f)
				{
					Goal = PPos + Away * 1000.0f;
					Gait = 2;
				}
				else if (T->Wolf.Stage == EKGWolfStage::Howl)
				{
					const FVector ToDen = (Wf->Den - PPos).GetSafeNormal2D();
					Goal = PPos + ToDen * (3200.0f + 400.0f * s);
					Gait = 2;
				}
				else if (T->Wolf.Stage == EKGWolfStage::Eyes || s != Lunger)
				{
					Wf->OrbitPhase += Dt * 0.35f;
					const float R = (T->Wolf.Stage == EKGWolfStage::Attack ? 1000.0f : 1500.0f + 350.0f * s);
					const float A = Wf->OrbitPhase + s * 2.1f;
					Goal = PPos + FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * R;
					Gait = 1;
				}
				else   // attack: this wolf's turn
				{
					const bool bBiteReady = T->Wolf.SinceBite >= KGForest::BiteCooldown && T->Wolf.StampAge >= KGForest::TelegraphMin;
					if (Wf->LungeLeft <= 0.0f && Dist <= KGForest::LungeFrom && bBiteReady)
					{
						Wf->LungeLeft = KGForest::WolfLungeMax;
						Wf->MulticastSnarl();
					}
					if (Wf->LungeLeft > 0.0f)
					{
						Wf->LungeLeft -= Dt;
						Goal = PPos;
						Gait = 3;
						if (Dist <= 1.8f && bBiteReady)
						{
							Bite(Wf, Prey, *T);
							Wf->LungeLeft = 0.0f;
							Wf->BackOff = KGForest::HitAndRun;
						}
					}
					else
					{
						Goal = PPos + Away * 500.0f;
						Gait = 2;
					}
				}
			}
			const FVector Clamped = ClampWolfGoal(WPos, Goal);
			Wf->DebugLog -= Dt;
			if (Wf->DebugLog <= 0.0f && Prey)
			{
				Wf->DebugLog = 2.0f;
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST wolf %s gait=%d dist=%.1f m goal_dist=%.1f m clamp_cut=%.1f m speed=%.0f status=%d move=%d"),
				       *Wf->GetName(), Gait, FVector::Dist2D(WPos, Prey->GetActorLocation()) / 100.0f, FVector::Dist2D(WPos, Goal) / 100.0f,
				       FVector::Dist2D(Goal, Clamped) / 100.0f, Wf->GetVelocity().Size2D(), AI ? int32(AI->GetMoveStatus()) : -1, Wf->MoveKind);
			}
			const float Speeds[] = {0.0f, KGForest::WolfSneak, KGForest::WolfTrot, KGForest::WolfLunge, KGForest::WolfTrot};
			Move->MaxWalkSpeed = Speeds[Gait] * 100.0f;
			if (Wf->Gait != Gait)
			{
				Wf->Gait = Gait;
			}
			if (AI && (Wf->Repath <= 0.0f || Gait == 3))
			{
				Wf->Repath = 0.4f;
				// navmesh first (around the trunks); a path that fails or crosses a village / lit cell -> straight line
				int32 Kind = 1;
				{
					if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(W, WPos, Clamped, Wf))
					{
						if (Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 0)
						{
							Kind = 0;
							for (const FVector& Pt : Path->PathPoints)
							{
								if (!WolfCellOk(Pt))
								{
									Kind = 2;
									break;
								}
							}
						}
					}
				}
				Wf->MoveKind = Kind;
				if (Kind == 0)
				{
					if (Move->MovementMode != MOVE_Walking)
					{
						Move->SetMovementMode(MOVE_Walking);
					}
					if (FVector::Dist2D(WPos, Clamped) < 60.0f)
					{
						AI->StopMovement();
					}
					else
					{
						AI->MoveToLocation(Clamped, 40.0f, false, true, false, false, nullptr, true);
					}
				}
				else
				{
					AI->StopMovement();
					Move->SetMovementMode(MOVE_None);   // kinematic below: the server slides it along the ground
				}
			}
			if (Wf->MoveKind != 0)
			{
				const float Speed = Speeds[Gait] * 100.0f;
				const float Left = FVector::Dist2D(WPos, Clamped);
				const FVector Dir = (Clamped - WPos).GetSafeNormal2D();
				if (Left > 30.0f && !Dir.IsNearlyZero())
				{
					FVector Next = WPos + Dir * FMath::Min(Speed * Dt, Left - 20.0f);
					Next = Ground(W, Next, 150.0f) + FVector(0.0f, 0.0f, Wf->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
					Wf->SetActorLocationAndRotation(Next, FMath::RInterpTo(Wf->GetActorRotation(), Dir.Rotation(), Dt, 8.0f));
					Move->Velocity = Dir * Speed;
				}
				else
				{
					Move->Velocity = FVector::ZeroVector;
					if (Prey)
					{
						Wf->SetActorRotation(FMath::RInterpTo(Wf->GetActorRotation(), FRotator(0.0f, (Prey->GetActorLocation() - WPos).Rotation().Yaw, 0.0f), Dt, 6.0f));
					}
				}
			}
		}
	}
}

void UKGForestSubsystem::Bite(AKGWolf* Wolf, AKGCharacter* Victim, FTrack& T)
{
	AKGPlayerState* PS = T.PS.Get();
	const bool bLethal = IsLethalNow();
	const float Damage = FKGForestRules::BiteDamage(bLethal);
	const float Now = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	UKGHealthComponent* H = Victim->GetHealth();
	if (bLethal && H && H->GetHealth() - Damage <= 0.0f)
	{
		NotePvEDeath(PS, TEXT("wolf"));
	}
	FKGForestRules::OnBite(T.Wolf);
	if (T.FirstBiteTime < 0.0f)
	{
		T.FirstBiteTime = Now;
	}
	T.LastBiteTime = Now;
	// Evidence first (the body may die from this bite): torn bite wounds, distinct from a knife.
	AKGBiteEvidence* Ev = nullptr;
	for (AActor* A : Victim->Children)
	{
		if (AKGBiteEvidence* E = Cast<AKGBiteEvidence>(A); E && E->Kind == TEXT("bite"))
		{
			Ev = E;
		}
	}
	if (!Ev)
	{
		FActorSpawnParameters P;
		P.Owner = Victim;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Ev = GetWorld()->SpawnActor<AKGBiteEvidence>(AKGBiteEvidence::StaticClass(), Victim->GetActorTransform(), P);
		if (Ev)
		{
			Ev->Kind = TEXT("bite");
			Ev->Sector = FKGForestMap::SectorName(T.Sector);
			Ev->AttachToActor(Victim, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}
	if (Ev)
	{
		Ev->AuthAddWound();
	}
	if (Damage > 0.0f)
	{
		UGameplayStatics::ApplyDamage(Victim, Damage, nullptr, Wolf, UKGDamageType_WolfBite::StaticClass());
	}
	// stagger (0.4 s knock) - also the whole bite when PvE is not lethal
	const FVector Push = (Victim->GetActorLocation() - Wolf->GetActorLocation()).GetSafeNormal2D() * 320.0f + FVector(0, 0, 140.0f);
	Victim->LaunchCharacter(Push, true, false);
	if (AKGForestPlayerInfo* I = T.Info.Get())
	{
		I->BittenAt = Now;
		I->ForceNetUpdate();
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST bite victim=%s wolf=%s damage=%.0f lethal=%d bites=%d stamp_to_bite=%.1f t=%.1f hp=%.0f bot=%d"),
	       *PS->GetPlayerName(), *Wolf->GetName(), Damage, bLethal ? 1 : 0, T.Wolf.Bites, T.Wolf.StampAge, Now,
	       H ? H->GetHealth() : -1.0f, PS->IsABot() ? 1 : 0);
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST evidence victim=%s kind=bite wounds=%d sector=%s"), *PS->GetPlayerName(),
	       Ev ? Ev->Wounds : 0, *FKGForestMap::SectorName(T.Sector));
	// TODO(020b): howl / eyes / bite / evidence as Event Ledger entries (witness filter) instead of log lines.
}

// ================================================================================================ the Mist
void UKGForestSubsystem::UpdateMist(float Dt)
{
	UWorld* W = GetWorld();
	const AKGGameState* GS = W->GetGameState<AKGGameState>();
	const EKGPhase Phase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	const bool bFrozen = Phase == EKGPhase::Meeting || Phase == EKGPhase::Trial || Phase == EKGPhase::Epilogue;
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : 0.0f;
	for (FTrack& T : Tracks)
	{
		AKGMistTongue* Tg = T.Tongue.Get();
		if (!Tg)
		{
			continue;
		}
		AKGCharacter* Body = BodyOf(T.PS.Get());
		if (!Tg->bFading)
		{
			const FVector2D M = Body ? M2(Body->GetActorLocation()) : FVector2D::ZeroVector;
			const bool bEscaped = Body && (FKGForestMap::Get().IsStaticSafe(M) || IsLitLightAt(M));
			if (!Body || bEscaped || (bFrozen && !T.bForceMist) || !IsEnabled())
			{
				Tg->bFading = true;
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST mist %s player=%s age=%.1f t=%.1f"), bEscaped ? TEXT("escaped") : TEXT("gone"),
				       T.PS.IsValid() ? *T.PS->GetPlayerName() : TEXT("?"), Tg->Age, Now);
			}
		}
		if (Tg->bFading)
		{
			Tg->Density = FMath::Max(0.0f, Tg->Density - Dt / KGForest::MistFadeSecs);
			if (Tg->Density <= 0.0f)
			{
				Tg->Destroy();
				T.Tongue = nullptr;
				T.Mist = FKGMistTrack();
				T.bForceMist = false;
			}
			continue;
		}
		Tg->Age += Dt;
		Tg->Density = FMath::Min(1.0f, Tg->Density + Dt / 2.0f);
		const FVector Pos = Tg->GetActorLocation();
		const FVector Target = Body->GetActorLocation();
		const FVector To = (Target - Pos).GetSafeNormal2D();
		const float Step = FKGForestRules::MistSpeed(Tg->Age) * 100.0f * Dt;
		const float Dist2D = FVector::Dist2D(Pos, Target);
		FVector Next = Pos + To * FMath::Min(Step, FMath::Max(0.0f, Dist2D - 50.0f));
		Next = Ground(W, Next + FVector(0, 0, 300.0f));
		Tg->SetActorLocationAndRotation(Next, FRotator(0.0f, To.Rotation().Yaw, 0.0f));
		if (FKGForestRules::StepMistCore(T.Mist, FVector::Dist2D(Next, Target) / 100.0f, Dt))
		{
			MistCatch(T, Body);
		}
	}
}

void UKGForestSubsystem::MistCatch(FTrack& T, AKGCharacter* Body)
{
	AKGPlayerState* PS = T.PS.Get();
	const bool bLethal = IsLethalNow();
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST mist catch player=%s lethal=%d frost_to_catch=%.1f"), *PS->GetPlayerName(), bLethal ? 1 : 0,
	       T.Mist.FrostAge);
	if (AKGMistTongue* Tg = T.Tongue.Get())
	{
		Tg->bFading = true;
	}
	if (bLethal)
	{
		NotePvEDeath(PS, TEXT("mist"));
		FActorSpawnParameters P;
		P.Owner = Body;
		if (AKGBiteEvidence* Ev = GetWorld()->SpawnActor<AKGBiteEvidence>(AKGBiteEvidence::StaticClass(), Body->GetActorTransform(), P))
		{
			Ev->Kind = TEXT("frost");
			Ev->Sector = FKGForestMap::SectorName(T.Sector);
			Ev->AttachToActor(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Ev->AuthAddWound();
		}
		UGameplayStatics::ApplyDamage(Body, 1000.0f, nullptr, T.Tongue.Get(), UKGDamageType_Mist::StaticClass());
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST evidence victim=%s kind=frost sector=%s"), *PS->GetPlayerName(),
		       *FKGForestMap::SectorName(T.Sector));
	}
	else
	{
		WallReturn(T, Body, TEXT("mist"));
	}
}

void UKGForestSubsystem::WallReturn(FTrack& T, AKGCharacter* Body, const TCHAR* Why)
{
	FVector Goal;
	if (!SafeGoal(Body->GetActorLocation(), Goal))
	{
		const FKGForestMap& Map = FKGForestMap::Get();
		FVector2D M = M2(Body->GetActorLocation());
		// outside the raster (past the wall): walk the line back toward the village until a safe cell
		for (int32 i = 0; i < 200 && !Map.IsStaticSafe(M); ++i)
		{
			M -= M.GetSafeNormal() * 2.0f;
		}
		Goal = Ground(GetWorld(), Cm3(M, Body->GetActorLocation().Z + 500.0f)) + FVector(0, 0, 100.0f);
	}
	{ FRotator R = Body->GetActorRotation(); GetWorld()->FindTeleportSpot(Body, Goal, R); Body->TeleportTo(Goal, R, false, true); }
	T.Idle = 0.0f;
	T.Wolf.Interest = 0.0f;
	T.Mist.Notice = 0.0f;
	T.Mist.Stage = EKGMistStage::None;
	T.Mist.FrostAge = -1.0f;
	T.Mist.CoreSecs = 0.0f;
	Tell(T.PS.Get(), FString(Why) == TEXT("afk") ? TEXT("You wake up on the path, cold and stiff.")
	                                          : TEXT("The Mist turns you around - you stumble out on the path, soaked and shivering."), 4.0f);
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST wall_return player=%s why=%s to=%s"), *T.PS->GetPlayerName(), Why, *Goal.ToCompactString());
	// TODO(035b): Wet 60 s + stamina 0 through UKGStatusComponent once it exists.
}

// ================================================================================================ camp + vigil
bool UKGForestSubsystem::SignVigil(AKGPlayerState* PS, FString& OutWhy)
{
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (!GS || GS->GetPhase() != EKGPhase::Day || !FKGForestRules::VigilOpen(GS->DayIndex))
	{
		OutWhy = TEXT("The vigil book opens in the day, from the second day on.");
		return false;
	}
	if (!PS || !PS->IsAlive())
	{
		return false;
	}
	if (VigilSigners.Contains(PS))
	{
		OutWhy = TEXT("You already signed tonight's vigil.");
		return false;
	}
	if (VigilSigners.Num() >= FKGForestRules::VigilMaxSigners(NumPlayers()))
	{
		OutWhy = TEXT("The vigil is full tonight.");
		return false;
	}
	VigilSigners.Add(PS);
	if (AKGVigilBook* B = Book.Get())
	{
		B->NumSigned = static_cast<uint8>(VigilSigners.Num());
		B->ForceNetUpdate();
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL sign player=%s signers=%d day=%d bot=%d"), *PS->GetPlayerName(), VigilSigners.Num(),
	       GS->DayIndex, PS->IsABot() ? 1 : 0);
	// never mirrored to the Town Board (design 10)
	return true;
}

void UKGForestSubsystem::UpdateCampAndVigil(float Dt)
{
	UWorld* W = GetWorld();
	AKGGameState* GS = W->GetGameState<AKGGameState>();
	if (!GS)
	{
		return;
	}
	const EKGPhase Phase = GS->GetPhase();
	const int32 N = NumPlayers();
	if (AKGVigilBook* B = Book.Get())
	{
		const bool bOpen = Phase == EKGPhase::Day && FKGForestRules::VigilOpen(GS->DayIndex);
		const uint8 Max = static_cast<uint8>(FKGForestRules::VigilMaxSigners(N));
		if (B->bOpen != bOpen || B->MaxSigners != Max)
		{
			B->bOpen = bOpen;
			B->MaxSigners = Max;
			B->ForceNetUpdate();
		}
	}
	VigilSigners.RemoveAll([](const TWeakObjectPtr<AKGPlayerState>& P) { return !P.IsValid(); });
	const uint8 PhaseByte = static_cast<uint8>(Phase);
	if (PhaseByte != LastPhase)
	{
		const EKGPhase Old = static_cast<EKGPhase>(LastPhase);
		LastPhase = PhaseByte;
		if (Phase == EKGPhase::Night && FKGForestRules::VigilOpen(GS->DayIndex))
		{
			if (VigilSigners.Num() >= FKGForestRules::VigilMinSigners)
			{
				bVigilNight = true;
				VigilNightSecs = GS->Clock.PhaseDuration;
				VigilElapsed = VigilLitSecs = VigilCountedSecs = 0.0f;
				VigilOutside.Reset();
				VigilArrived.Reset();
				for (const TWeakObjectPtr<AKGPlayerState>& S : VigilSigners)
				{
					Tell(S.Get(), TEXT("Camp Vigil: walk to the camp fire in the north wood and keep it burning till dawn."), 6.0f);
				}
				UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL start signers=%d night=%.0f s arrive_by=%.0f s camp_logs=%d"), VigilSigners.Num(),
				       VigilNightSecs, FKGForestRules::VigilArriveBy(VigilNightSecs), CampLogs());
			}
			else if (VigilSigners.Num() > 0)
			{
				for (const TWeakObjectPtr<AKGPlayerState>& S : VigilSigners)
				{
					Tell(S.Get(), TEXT("Too few signed the vigil - no watch tonight."), 4.0f);
				}
				UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL cancelled signers=%d"), VigilSigners.Num());
				VigilSigners.Reset();
			}
		}
		if (Old == EKGPhase::Night && bVigilNight)
		{
			// dawn: judge the night (side-blind: roles never enter)
			FKGVigilNight Night;
			int32 AliveSigners = 0;
			float MaxOut = 0.0f;
			bool bAll = true;
			for (const TWeakObjectPtr<AKGPlayerState>& S : VigilSigners)
			{
				if (!S.IsValid() || !S->IsAlive())
				{
					continue;
				}
				++AliveSigners;
				bAll &= VigilArrived.Contains(S);
				MaxOut = FMath::Max(MaxOut, VigilOutside.FindRef(S));
			}
			Night.Signers = AliveSigners;
			Night.bAllArrived = AliveSigners > 0 && bAll;
			Night.MaxOutsideSecs = MaxOut;
			Night.LitShare = VigilCountedSecs > 0.0f ? VigilLitSecs / VigilCountedSecs : 0.0f;
			int32 Units = 0;
			if (AKGGameMode* GM = W->GetAuthGameMode<AKGGameMode>())
			{
				Units = FKGForestRules::VigilUnits(Night, N, VigilMatchGranted, GM->CountOpenLivingTownTasks());
				Units = Units > 0 ? GM->AddVigilReward(Units) : 0;
			}
			VigilMatchGranted += Units;
			UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL result success=%d units=%d signers=%d arrived=%d max_outside=%.1f lit=%.2f match_units=%d"),
			       FKGForestRules::VigilSuccess(Night) ? 1 : 0, Units, Night.Signers, Night.bAllArrived ? 1 : 0, Night.MaxOutsideSecs,
			       Night.LitShare, VigilMatchGranted);
			for (const TWeakObjectPtr<AKGPlayerState>& S : VigilSigners)
			{
				Tell(S.Get(), Units > 0 ? TEXT("The fire burned all night. The village is a little more ready.")
				                        : TEXT("The vigil failed - the fire died or the watch broke."), 5.0f);
			}
			bVigilNight = false;
			VigilSigners.Reset();
			if (AKGVigilBook* B = Book.Get())
			{
				B->NumSigned = 0;
				B->ForceNetUpdate();
			}
		}
		if (Phase == EKGPhase::Dawn || (Phase == EKGPhase::Day && Old == EKGPhase::Night))
		{
			// world state resets at dawn: the fire is laid fresh, the trail lanterns burn out (the woodpile keeps its logs)
			if (AKGCampfire* F = Campfire.Get())
			{
				F->AuthReset();
			}
			if (AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(W))
			{
				for (const int32 i : ForestLampSpots)
				{
					if (FKGSpotState* S = D->AuthMutableSpot(i))
					{
						S->bLit = false;
					}
				}
				D->AuthDirty();
			}
		}
		if (Phase == EKGPhase::Warmup || Phase == EKGPhase::Lobby)
		{
			VigilMatchGranted = 0;
			VigilSigners.Reset();
			bVigilNight = false;
		}
	}
	if (bVigilNight && Phase == EKGPhase::Night)
	{
		VigilElapsed += Dt;
		const AKGCampfire* F = Campfire.Get();
		const float ArriveBy = FKGForestRules::VigilArriveBy(VigilNightSecs);
		for (const TWeakObjectPtr<AKGPlayerState>& S : VigilSigners)
		{
			const AKGCharacter* B = BodyOf(S.Get());
			if (!B || !F)
			{
				continue;
			}
			const bool bIn = FVector::Dist2D(B->GetActorLocation(), F->GetActorLocation()) <= KGForest::VigilRing * 100.0f;
			if (bIn && VigilElapsed <= ArriveBy && !VigilArrived.Contains(S))
			{
				VigilArrived.Add(S);
				UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL arrive player=%s at=%.0f s"), *S->GetPlayerName(), VigilElapsed);
			}
			if (VigilElapsed > ArriveBy && !bIn)
			{
				VigilOutside.FindOrAdd(S) += Dt;
			}
		}
		if (VigilElapsed > ArriveBy && F)
		{
			VigilCountedSecs += Dt;
			VigilLitSecs += F->IsLit() ? Dt : 0.0f;
		}
	}
}

// ================================================================================================ bots
bool UKGForestSubsystem::UpdateBot(AKGBotController* Bot, AKGCharacter* Me, float DeltaSeconds)
{
	UKGForestSubsystem* FS = Get(Me->GetWorld());
	if (!FS || !FS->bActive || !FS->bServer)
	{
		return false;
	}
	AKGPlayerState* PS = Me->GetPlayerState<AKGPlayerState>();
	FTrack* T = FS->FindTrack(PS);
	if (!T)
	{
		return false;
	}
	FBotMem& Mem = BotMem().FindOrAdd(Bot);
	Mem.Repath -= DeltaSeconds;
	const AKGGameState* GS = Me->GetWorld()->GetGameState<AKGGameState>();
	// 1) wolves / the Mist: walk straight back to the nearest path (the escape the rules guarantee)
	const bool bThreat = IsEnabled() && (T->Wolf.Stage >= EKGWolfStage::Howl || T->Mist.Stage != EKGMistStage::None || T->Tongue.IsValid());
	if (bThreat)
	{
		const FString Stage = T->Tongue.IsValid() || T->Mist.Stage != EKGMistStage::None ? TEXT("mist") : KGWolfStageName(T->Wolf.Stage);
		FVector Goal;
		if (FS->SafeGoal(Me->GetActorLocation(), Goal))
		{
			if (Mem.Repath <= 0.0f || Bot->GetMoveStatus() != EPathFollowingStatus::Moving)
			{
				Mem.Repath = 1.0f;
				if (Bot->MoveToLocation(Goal, 60.0f, false, true, true, false, nullptr, true) == EPathFollowingRequestResult::Failed)
				{
					Bot->MoveToLocation(Goal, 60.0f, false, false, false, false, nullptr, true);
				}
			}
			if (Stage != Mem.LoggedStage)
			{
				Mem.LoggedStage = Stage;
				UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST bot=%s stage=%s action=walk safe_dist=%.1f"), *PS->GetPlayerName(), *Stage, T->SafeDist);
			}
			return true;
		}
	}
	else
	{
		Mem.LoggedStage.Reset();
	}
	if (!GS)
	{
		return false;
	}
	// 2) the vigil book: in the day (from day 2), Town and Impatient bots sign with the same chance (side-blind)
	if (GS->GetPhase() == EKGPhase::Day && FKGForestRules::VigilOpen(GS->DayIndex) && Mem.SignDecidedDay != GS->DayIndex &&
	    GS->GetPhaseRemaining() < GS->Clock.PhaseDuration * 0.8f)
	{
		Mem.SignDecidedDay = GS->DayIndex;
		FKGRng R(GetTypeHash(PS->GetPlayerName()) ^ uint32(GS->DayIndex * 7919), 33u);
		if (R.FRand() < 0.4f)
		{
			FString Why;
			FS->SignVigil(PS, Why);
		}
		Mem.bLeaveVigil = IsImpatient(PS) && R.FRand() < 0.5f;   // an Impatient signer may slip away to hunt
		Mem.bVigilLogged = false;
	}
	// 3) vigil night: walk to the camp fire, stay in the ring, feed the fire below 40 fuel
	if (FS->bVigilNight && FS->IsVigilSigner(PS) && GS->GetPhase() == EKGPhase::Night && FS->Campfire.IsValid())
	{
		AKGCampfire* F = FS->Campfire.Get();
		const float D = FVector::Dist2D(Me->GetActorLocation(), F->GetActorLocation());
		if (Mem.bLeaveVigil && FS->VigilArrived.Contains(PS))
		{
			return false;
		}
		if (D > 600.0f)
		{
			if (Mem.Repath <= 0.0f || Bot->GetMoveStatus() != EPathFollowingStatus::Moving)
			{
				Mem.Repath = 1.5f;
				const FVector Spot = F->GetActorLocation() + FVector(FMath::Cos(GetTypeHash(PS->GetPlayerName()) * 0.1f),
				                                                     FMath::Sin(GetTypeHash(PS->GetPlayerName()) * 0.1f), 0.0f) * 350.0f;
				Bot->MoveToLocation(Spot, 80.0f, false, true, true, false, nullptr, true);
			}
			return true;
		}
		if (!Mem.bVigilLogged)
		{
			Mem.bVigilLogged = true;
			UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL bot=%s at_fire"), *PS->GetPlayerName());
		}
		if (F->Fuel < KGForest::BotFeedBelow && (F->Ash == 0 && F->Fuel <= 0.0f ? true : FS->CampLogs() > 0) && Mem.Repath <= 0.0f)
		{
			Mem.Repath = 2.0f;
			F->AuthUse(Me);
			UE_LOG(LogKillGodot, Log, TEXT("KG_VIGIL bot=%s feed fuel=%.0f logs=%d"), *PS->GetPlayerName(), F->Fuel, FS->CampLogs());
		}
		Bot->StopMovement();
		return true;
	}
	return false;
}

// ================================================================================================ dev
FVector UKGForestSubsystem::DeepSpot(int32 Sector, float Along) const
{
	const FKGForestMap& Map = FKGForestMap::Get();
	for (const FKGForestDen& D : Map.Dens)
	{
		if (Sector == 0 || D.Sector == Sector)
		{
			// halfway between the den and the ring trail: Deep band, walkable
			const FVector2D Dir = D.At.GetSafeNormal();
			const FVector2D P = D.At - Dir * 14.0f + FVector2D(-Dir.Y, Dir.X) * Along;
			return Ground(GetWorld(), Cm3(P, 6000.0f)) + FVector(0, 0, 100.0f);
		}
	}
	return FVector::ZeroVector;
}

FVector UKGForestSubsystem::CampLocation() const
{
	return Campfire.IsValid() ? Campfire->GetActorLocation() : FVector::ZeroVector;
}

void UKGForestSubsystem::DevResetSession()
{
	SessionDeaths = SessionPvEDeaths = 0;
}

FString UKGForestSubsystem::DevTest(const FString& What, AKGPlayerState* Who, bool bTeleport)
{
	FTrack* T = FindTrack(Who);
	AKGCharacter* Body = BodyOf(Who);
	if (!T || !Body)
	{
		return TEXT("no such living player (tracks start 0.25 s after spawn)");
	}
	if (bTeleport && (What == TEXT("wolf") || What == TEXT("mist") || What == TEXT("deep")))
	{
		const FVector At = DeepSpot(3);
		FVector To = At;
		FRotator R(0.0f, (FVector(-At.X, -At.Y, 0.0f)).Rotation().Yaw, 0.0f);
		GetWorld()->FindTeleportSpot(Body, To, R);
		const bool bOk = Body->TeleportTo(To, R, false, true);
		UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST teleport %s -> %s ok=%d"), *Who->GetPlayerName(), *To.ToCompactString(), bOk ? 1 : 0);
	}
	if (What == TEXT("wolf"))
	{
		T->ForceWolfGain = 7.0f;   // = Deep band at night: stamp 5 s, eyes 17 s, first bite 25 s
		T->Wolf = FKGWolfTrack();
	}
	else if (What == TEXT("mist"))
	{
		T->bForceMist = true;
		T->Mist = FKGMistTrack();
	}
	else if (What == TEXT("stop"))
	{
		T->ForceWolfGain = 0.0f;
		T->bForceMist = false;
		T->Wolf.Interest = 0.0f;
	}
	else if (What == TEXT("fire"))
	{
		if (AKGCampfire* F = Campfire.Get())
		{
			return F->AuthUse(Body);
		}
	}
	else if (What == TEXT("logs"))
	{
		AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(GetWorld());
		if (FKGSpotState* S = D ? D->AuthMutableSpot(CampWoodpileSpot) : nullptr)
		{
			S->Count = 6;
			D->AuthDirty();
		}
	}
	else if (What == TEXT("vigil"))
	{
		FString Why;
		return SignVigil(Who, Why) ? TEXT("signed") : Why;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FOREST test %s player=%s teleport=%d at=%s"), *What, *Who->GetPlayerName(), bTeleport ? 1 : 0,
	       *Body->GetActorLocation().ToCompactString());
	return FString::Printf(TEXT("%s on %s"), *What, *Who->GetPlayerName());
}

FString UKGForestSubsystem::DevStatus() const
{
	int32 Alive, Threats;
	CountAlive(Alive, Threats);
	FString S = FString::Printf(TEXT("forest enabled=%d active=%d N=%d alive=%d threats=%d lethal=%d wolves=%d/%d tongues=%d session %d/%d pve"),
	                            IsEnabled() ? 1 : 0, bActive ? 1 : 0, NumPlayers(), Alive, Threats, IsLethalNow() ? 1 : 0, ActiveWolves(),
	                            FKGForestRules::WolfCap(NumPlayers()), ActiveTongues(), SessionPvEDeaths, SessionDeaths);
	for (const FTrack& T : Tracks)
	{
		if (T.PS.IsValid() && (T.Band != EKGForestBand::Village || T.Wolf.Stage != EKGWolfStage::Silent))
		{
			S += FString::Printf(TEXT("\n  %s band=%s wolf=%s(%.0f) mist=%d(%.0f) safe=%.0fm group=%d"), *T.PS->GetPlayerName(),
			                     KGForestBandName(T.Band), KGWolfStageName(T.Wolf.Stage), T.Wolf.Interest, int32(T.Mist.Stage),
			                     T.Mist.Notice, T.SafeDist, T.Group);
		}
	}
	return S;
}
