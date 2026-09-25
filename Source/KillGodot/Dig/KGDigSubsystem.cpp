#include "Dig/KGDigSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameMode.h"
#include "Dig/KGDigComponent.h"
#include "Dig/KGDigManager.h"
#include "Dig/KGPassage.h"
#include "Dig/KGUndergroundInfo.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UKGDigSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGDigSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKGDigSubsystem::HandleActorSpawned));
}

void UKGDigSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Pending.Reset();
	Super::Deinitialize();
}

TStatId UKGDigSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGDigSubsystem, STATGROUP_Tickables);
}

void UKGDigSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AKGCharacter* Character = Cast<AKGCharacter>(Actor))
	{
		Pending.Add(Character);
	}
}

void UKGDigSubsystem::EnsureDig(AKGCharacter* Character)
{
	if (!IsValid(Character) || !Character->HasAuthority() || Character->IsActorBeingDestroyed() ||
	    Character->FindComponentByClass<UKGDigComponent>())
	{
		return;
	}
	UKGDigComponent* Dig = NewObject<UKGDigComponent>(Character, TEXT("KGDig"));
	Dig->SetIsReplicated(true);
	Character->AddInstanceComponent(Dig);
	Dig->RegisterComponent();
}

void UKGDigSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		for (const TWeakObjectPtr<AKGCharacter>& Character : Pending)
		{
			EnsureDig(Character.Get());
		}
		Pending.Reset();
		SweepSeconds += DeltaTime;
		if (SweepSeconds >= 1.0f && World->HasBegunPlay())
		{
			SweepSeconds = 0.0f;
			for (TActorIterator<AKGCharacter> It(World); It; ++It)
			{
				EnsureDig(*It);
			}
			// One manager per world; a fresh set of spots for every match seed.
			if (AKGDigManager* Manager = AKGDigManager::Get(World, true))
			{
				const AKGGameMode* GM = World->GetAuthGameMode<AKGGameMode>();
				const uint64 MatchSeed = GM ? static_cast<uint64>(GM->GetMatchSeed()) : 0;
				const uint64 Want = MatchSeed != 0 ? MatchSeed : FreeRoamSeed;
				if (Manager->GetGeneratedSeed() != Want)
				{
					Manager->Regenerate(Want);
					bShotsApplied = false;
				}
				if (!bShotsApplied)
				{
					bShotsApplied = true;
					ApplyShotStages();
				}
			}
		}
	}
	else
	{
		Pending.Reset();
	}
	TickSmoke(DeltaTime);
}

void UKGDigSubsystem::ApplyShotStages()
{
#if !UE_BUILD_SHIPPING
	static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("KGDigShots"));
	AKGDigManager* Manager = bShots ? AKGDigManager::Get(GetWorld()) : nullptr;
	if (Manager)
	{
		AKGDigManager::DevPrepareShots(Manager);
	}
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// -KGDigSmoke: headless network smoke (Tools/Unreal/kg_dig_smoke.ps1)
// ---------------------------------------------------------------------------------------------------------------
void UKGDigSubsystem::TickSmoke(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGDigSmoke"));
	UWorld* World = GetWorld();
	if (!bSmoke || bSmokeDone || !World)
	{
		return;
	}
	const bool bHost = World->GetNetMode() != NM_Client;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");
	APlayerController* LocalPC = World->GetFirstPlayerController();
	AKGCharacter* Me = LocalPC ? Cast<AKGCharacter>(LocalPC->GetPawn()) : nullptr;
	AKGCharacter* Digger = nullptr;
	int32 Humans = 0;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			AKGCharacter* Body = PS && !PS->IsABot() ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
			if (Body)
			{
				++Humans;
				if (Body != Me)
				{
					Digger = Body;
				}
			}
		}
	}
	if (!bHost)
	{
		Digger = Me;   // the client digs itself
	}
	UKGDigComponent* Dig = UKGDigComponent::FindFor(Digger);
	AKGDigManager* Manager = AKGDigManager::Get(World);
	if (SmokeClock < 0.0f)
	{
		if (Humans < 2 || !Me || !Dig || !Manager || Manager->GetSpots().Num() == 0)
		{
			return;
		}
		SmokeClock = 0.0f;
		SmokeResultSerial = Dig->GetLastResult().Serial;
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE %s start digger=%s spots=%d underground=%d"), Machine, *Digger->GetName(),
		       Manager->GetSpots().Num(), AKGUndergroundInfo::Find(World) ? 1 : 0);
	}
	SmokeClock += DeltaTime;
	auto Finish = [this, Machine](const TCHAR* Why)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_DONE %s %s"), Machine, Why);
		bSmokeDone = true;
	};
	if (!Dig || !Digger || !Manager)
	{
		Finish(TEXT("lost the digger"));
		return;
	}
	const FString Body = Digger->GetPlayerState() ? Digger->GetPlayerState()->GetPlayerName() : Digger->GetName();
	const FVector Feet = Digger->GetActorLocation() - FVector(0.0, 0.0, 90.0);
	if (SmokeSpot == 0)
	{
		// The client learns the spot from replication: the fresh mound right in front of it.
		const int32 i = Manager->FindNearestSpot(Feet, 400.0f, EKGDigKind::Mound);
		if (i != INDEX_NONE && (bHost ? SmokeStep >= 1 : true))
		{
			SmokeSpot = Manager->GetSpots()[i].Id;
		}
	}
	const FKGDigSpot* Spot = SmokeSpot ? Manager->FindSpot(SmokeSpot) : nullptr;
	// 1 Hz: what this machine sees.
	SmokeLogAccum += DeltaTime;
	if (SmokeLogAccum >= 1.0f)
	{
		SmokeLogAccum = 0.0f;
		const FString Region = AKGUndergroundInfo::Find(World) ? AKGUndergroundInfo::Find(World)->RegionNameAt(Digger->GetActorLocation()) : FString();
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SEEN %s tag=state body=%s shovel=%d digging=%d spot=%d stage=%d/%d below=%d region=%s t=%.0f"),
		       Machine, *Body, Dig->IsShovelShown() ? 1 : 0, Dig->GetAction().bDigging ? 1 : 0, SmokeSpot, Spot ? Spot->Stage : -1,
		       Spot ? Spot->MaxStage : -1, AKGUndergroundInfo::IsBelowGround(World, Digger->GetActorLocation()) ? 1 : 0,
		       Region.IsEmpty() ? TEXT("-") : *Region, SmokeClock);
	}
	if (Dig->GetLastResult().Serial != SmokeResultSerial && !bHost)
	{
		SmokeResultSerial = Dig->GetLastResult().Serial;
		const FKGDigResult& R = Dig->GetLastResult();
		FString Items;
		for (const FKGItemStack& S : R.Items)
		{
			Items += FString::Printf(TEXT("%s%s x%d"), Items.IsEmpty() ? TEXT("") : TEXT(","), *S.ItemId.ToString(), S.Count);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SEEN Client tag=result spot=%d stage=%d/%d items=[%s] dropped=%d"), R.SpotId, R.Stage,
		       R.MaxStage, *Items, R.bDropped ? 1 : 0);
	}
	AKGPassage* WellTop = AKGPassage::FindById(World, TEXT("WellTop"));
	const bool bBelow = AKGUndergroundInfo::IsBelowGround(World, Digger->GetActorLocation());

	if (bHost)
	{
		if (SmokeStep == 0 && SmokeClock >= 2.0f)
		{
			UKGPlayerExtrasSubsystem::EnsurePlayerComponents(Digger->GetPlayerState());
			if (UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Digger))
			{
				Pockets->AddItem(FKGItemIds::Shovel, 1);
			}
			// A fresh mound 1.6 m in front of the digger, on the ground it stands on.
			const FVector Fwd = Digger->GetActorForwardVector().GetSafeNormal2D();
			FVector At = Feet + Fwd * 160.0f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDigSmoke), false, Digger);
			if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 200.0), At - FVector(0, 0, 300.0), ECC_Visibility, Params))
			{
				At = Hit.ImpactPoint;
			}
			const int32 Index = Manager->AddSpot(EKGDigKind::Mound, At, Digger->GetActorRotation().Yaw);
			SmokeSpot = Manager->GetSpots()[Index].Id;
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Host gave %s a shovel, mound #%d at %s"), *Body, SmokeSpot, *At.ToCompactString());
			SmokeStep = 1;
		}
		if (SmokeStep == 1 && Spot && Spot->IsDugOut())
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SEEN Host tag=dugout spot=%d stage=%d/%d"), Spot->Id, Spot->Stage, Spot->MaxStage);
			SmokeStep = 2;
			SmokeStepAt = SmokeClock;
		}
		if (SmokeStep == 2 && SmokeClock - SmokeStepAt > 2.0f)
		{
			if (!WellTop)
			{
				Finish(TEXT("no WellTop passage in the level"));
				return;
			}
			// Stand the digger in front of the well rim, looking at it: the client presses E.
			// Where people come up out of the well = a good place to stand and look at the rim.
			const FVector Target = WellTop->GetActorTransform().TransformPosition(WellTop->HitboxOffset);
			const FVector Stand = WellTop->GetArrival().GetLocation();
			const FRotator Look = (Target - (Stand + FVector(0, 0, 64.0))).Rotation();
			Digger->TeleportTo(Stand, FRotator(0.0f, Look.Yaw, 0.0f));
			if (APlayerController* PC = Cast<APlayerController>(Digger->GetController()))
			{
				PC->SetControlRotation(Look);
				PC->ClientSetRotation(Look, true);
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Host placed %s at the well %s"), *Body, *Stand.ToCompactString());
			SmokeStep = 3;
			SmokeStepAt = SmokeClock;
		}
		if (SmokeStep == 3 && !bBelow && SmokeClock - SmokeStepAt > 12.0f && WellTop)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Host fallback: sending %s down the well server-side"), *Body);
			WellTop->TravelThrough(Digger);
			SmokeStepAt = SmokeClock;
		}
		if (SmokeStep == 3 && bBelow)
		{
			const FString Region = AKGUndergroundInfo::Find(World)->RegionNameAt(Digger->GetActorLocation());
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_REGION Host below=1 body=%s region=%s at %s"), *Body, *Region,
			       *Digger->GetActorLocation().ToCompactString());
			SmokeStep = 4;
			SmokeStepAt = SmokeClock;
		}
		if (SmokeStep == 4 && SmokeClock - SmokeStepAt > 4.0f)
		{
			Finish(TEXT("ok"));
			return;
		}
		if (SmokeClock > 200.0f)
		{
			Finish(TEXT("timeout"));
		}
		return;
	}

	// ---- client: the digger ----
	const UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Digger);
	if (SmokeStep == 0 && Pockets && Pockets->Has(FKGItemIds::Shovel) && Spot)
	{
		Dig->RequestSetShovel(true);
		SmokeStep = 1;
		SmokeStepAt = SmokeClock;
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Client shovel out requested, spot #%d"), SmokeSpot);
	}
	else if (SmokeStep == 1 && Dig->IsShovelOutOnServer() && Spot && SmokeClock - SmokeStepAt > 1.0f)
	{
		// Look at the mound and hold the dig until it is dug out.
		const FVector Eye = Digger->GetFirstPersonCamera()->GetComponentLocation();
		const FRotator Look = (FVector(Spot->Location) - Eye).Rotation();
		LocalPC->SetControlRotation(Look);
		Dig->SetScriptedDig(SmokeSpot, 30.0f);
		SmokeStep = 2;
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Client digging #%d (pitch %.0f)"), SmokeSpot, Look.Pitch);
	}
	else if (SmokeStep == 2 && Spot)
	{
		const FVector Eye = Digger->GetFirstPersonCamera()->GetComponentLocation();
		LocalPC->SetControlRotation((FVector(Spot->Location) - Eye).Rotation());
		if (Spot->IsDugOut())
		{
			Dig->StopDigging();
			FString What;
			if (Pockets)
			{
				for (const FKGItemEntry& E : Pockets->GetEntries())
				{
					What += FString::Printf(TEXT("%s%s x%d"), What.IsEmpty() ? TEXT("") : TEXT(","), *E.ItemId.ToString(), E.Count);
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_POCKETS Client items=[%s]"), *What);
			Dig->RequestSetShovel(false);
			SmokeStep = 3;
			SmokeStepAt = SmokeClock;
		}
	}
	else if (SmokeStep == 3 && WellTop && !bBelow &&
	         FVector::Dist2D(Digger->GetActorLocation(), WellTop->GetActorLocation()) < 400.0f && SmokeClock - SmokeStepAt > 1.5f)
	{
		// E on the well rim (the real interaction path: trace -> AKGPassage::Interact -> travel).
		const FVector Eye = Digger->GetFirstPersonCamera()->GetComponentLocation();
		LocalPC->SetControlRotation((WellTop->GetActorTransform().TransformPosition(WellTop->HitboxOffset) - Eye).Rotation());
		Digger->Interact();
		SmokeStepAt = SmokeClock;
		UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SMOKE Client pressed E at the well"));
	}
	if (SmokeStep >= 3 && bBelow)
	{
		const FString Region = AKGUndergroundInfo::Find(World)->RegionNameAt(Digger->GetActorLocation());
		if (Region != SmokeLastRegion)
		{
			SmokeLastRegion = Region;
			UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_REGION Client below=1 region=%s at %s"), *Region, *Digger->GetActorLocation().ToCompactString());
		}
		if (SmokeStep == 3)
		{
			SmokeStep = 4;
			SmokeStepAt = SmokeClock;
		}
		if (SmokeStep == 4 && SmokeClock - SmokeStepAt > 3.0f)
		{
			Finish(TEXT("ok"));
			return;
		}
	}
	if (SmokeClock > 200.0f)
	{
		Finish(TEXT("timeout"));
	}
#endif
}
