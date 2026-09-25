#include "Fishing/KGFishingSubsystem.h"
#include "Character/KGCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fishing/KGFishingComponent.h"
#include "Fishing/KGFishMarket.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "World/KGMapInfo.h"

bool UKGFishingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGFishingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKGFishingSubsystem::HandleActorSpawned));
	FKGFishingWater::ResetCache();
}

void UKGFishingSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Pending.Reset();
	FKGFishingWater::ResetCache();
	Super::Deinitialize();
}

TStatId UKGFishingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGFishingSubsystem, STATGROUP_Tickables);
}

void UKGFishingSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AKGCharacter* Character = Cast<AKGCharacter>(Actor))
	{
		Pending.Add(Character);   // next tick: the pawn may still be mid-construction here
	}
}

void UKGFishingSubsystem::EnsureFishing(AKGCharacter* Character)
{
	if (!IsValid(Character) || !Character->HasAuthority() || Character->IsActorBeingDestroyed() ||
	    Character->FindComponentByClass<UKGFishingComponent>())
	{
		return;
	}
	UKGFishingComponent* Fishing = NewObject<UKGFishingComponent>(Character, TEXT("KGFishing"));
	Fishing->SetIsReplicated(true);
	Character->AddInstanceComponent(Fishing);
	Fishing->RegisterComponent();
}

bool UKGFishingSubsystem::FindMarketSpot(UWorld* World, FVector& OutLocation, float& OutYaw)
{
	const AKGMapInfo* Info = AKGMapInfo::Find(World);
	if (!Info)
	{
		return false;
	}
	const FKGMapRegion* Hall = Info->Regions.FindByPredicate([](const FKGMapRegion& R)
	{
		return R.Id == TEXT("fish_market") && R.Layer == 3;
	});
	if (!Hall)
	{
		return false;
	}
	// Morrowmere v2 layout: the fish_market landmark faces yaw 209.53 (Tools/Level/morrowmere_layout_v2.json); the
	// builder's Table_Large with the day's catch sits at local (0, 150) of that frame (Tools/Unreal/dressing/v2/
	// dress_harbour.py fish_market()). Local -Y is the open front of the arcade.
	constexpr float HallYaw = 209.53f;
	const float C = FMath::Cos(FMath::DegreesToRadians(HallYaw));
	const float S = FMath::Sin(FMath::DegreesToRadians(HallYaw));
	const FVector2D Local(0.0f, 150.0f);
	const FVector2D XY(Hall->Center.X + Local.X * C - Local.Y * S, Hall->Center.Y + Local.X * S + Local.Y * C);
	// Floor under the arcade roof: start below the roof (top 8.5 m) and look down.
	FHitResult Hit;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const bool bHit = World->LineTraceSingleByObjectType(Hit, FVector(XY.X, XY.Y, 450.0f), FVector(XY.X, XY.Y, -300.0f), Objects);
	OutLocation = FVector(XY.X, XY.Y, bHit ? Hit.ImpactPoint.Z : 202.0f);
	OutYaw = HallYaw;
	return true;
}

void UKGFishingSubsystem::EnsureMarket()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<AKGFishMarketStall> It(World); It; ++It)
	{
		return;
	}
	FVector Location;
	float Yaw = 0.0f;
	if (FindMarketSpot(World, Location, Yaw) && AKGFishMarketStall::SpawnStall(World, Location, Yaw))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_MARKET stall at %s yaw %.1f"), *Location.ToCompactString(), Yaw);
	}
}

void UKGFishingSubsystem::Tick(float DeltaTime)
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
			EnsureFishing(Character.Get());
		}
		Pending.Reset();
		SweepSeconds += DeltaTime;
		if (SweepSeconds >= 1.0f)
		{
			SweepSeconds = 0.0f;
			for (TActorIterator<AKGCharacter> It(World); It; ++It)
			{
				EnsureFishing(*It);
			}
			if (!bMarketChecked && World->HasBegunPlay())
			{
				bMarketChecked = true;
				EnsureMarket();
			}
		}
	}
	else
	{
		Pending.Reset();
	}
	TickSmoke(DeltaTime);
}

// ---------------------------------------------------------------------------------------------------------------
// -KGFishSmoke: headless network smoke (Tools/Unreal/kg_fish_smoke.ps1)
// ---------------------------------------------------------------------------------------------------------------
void UKGFishingSubsystem::TickSmoke(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGFishSmoke"));
	UWorld* World = GetWorld();
	if (!bSmoke || bSmokeDone || !World)
	{
		return;
	}
	const bool bHost = World->GetNetMode() != NM_Client;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");
	APlayerController* LocalPC = World->GetFirstPlayerController();
	AKGCharacter* Me = LocalPC ? Cast<AKGCharacter>(LocalPC->GetPawn()) : nullptr;
	// The other human (the client, seen from the host).
	AKGCharacter* Angler = nullptr;
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
					Angler = Body;
				}
			}
		}
	}
	if (!bHost)
	{
		Angler = Me;   // the client fishes itself
	}
	UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(Angler);
	if (SmokeClock < 0.0f)
	{
		if (Humans < 2 || !Me || !Fishing)
		{
			return;
		}
		SmokeClock = 0.0f;
		SmokeCatchSerial = Fishing->GetLastCatch().Serial;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SMOKE %s start angler=%s"), Machine, *Angler->GetName());
	}
	SmokeClock += DeltaTime;
	auto Finish = [this, Machine](const TCHAR* Why)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_DONE %s %s"), Machine, Why);
		bSmokeDone = true;
	};
	if (!Fishing || !Angler)
	{
		Finish(TEXT("lost the angler"));
		return;
	}
	const FString Body = Angler->GetPlayerState() ? Angler->GetPlayerState()->GetPlayerName() : Angler->GetName();

	// 1 Hz what this machine sees of the angler's line.
	SmokeLogAccum += DeltaTime;
	if (SmokeLogAccum >= 1.0f)
	{
		SmokeLogAccum = 0.0f;
		const FKGFishLine& L = Fishing->GetLine();
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SEEN %s tag=line body=%s local=%d rod=%d phase=%s shown=%s water=%s landing=%s tension=%.2f t=%.0f"),
		       Machine, *Body, Angler->IsLocallyControlled() ? 1 : 0, Fishing->IsRodShown() ? 1 : 0,
		       *StaticEnum<EKGFishPhase>()->GetNameStringByValue(static_cast<int64>(L.Phase)),
		       *StaticEnum<EKGFishPhase>()->GetNameStringByValue(static_cast<int64>(Fishing->GetShownPhase())),
		       *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(L.Water)), *FVector(L.Landing).ToCompactString(),
		       Fishing->HasShownSim() ? Fishing->GetShownSim().Tension : L.TensionQ / 180.0f, SmokeClock);
	}
	// The catch replicated here?
	const FKGFishCatch& Catch = Fishing->GetLastCatch();
	if (Catch.Serial != SmokeCatchSerial)
	{
		SmokeCatchSerial = Catch.Serial;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SEEN %s tag=catch body=%s result=%s item=%s grams=%d coins=%d dropped=%d"),
		       Machine, *Body, *StaticEnum<EKGFishPhase>()->GetNameStringByValue(static_cast<int64>(Catch.Result)),
		       *Catch.ItemId.ToString(), Catch.Grams, Catch.Coins, Catch.bDropped ? 1 : 0);
		if (!bHost && Catch.Result == EKGFishPhase::Landed)
		{
			SmokeReportAt = SmokeClock + 1.5f;   // give the pockets a moment to replicate, then report them
		}
		if (bHost && Catch.Result == EKGFishPhase::Landed)
		{
			Finish(TEXT("ok"));
			return;
		}
	}

	if (bHost)
	{
		if (SmokeStep == 0 && SmokeClock >= 2.0f)
		{
			// Everyone onto the jetty head (Morrowmere v2, deck at 1.2 m), looking north over the basin.
			auto Place = [World](AKGCharacter* C, float X, float Y)
			{
				FHitResult Hit;
				FCollisionObjectQueryParams Objects(ECC_WorldStatic);
				const float Z = World->LineTraceSingleByObjectType(Hit, FVector(X, Y, 700.0f), FVector(X, Y, -400.0f), Objects)
					                ? Hit.ImpactPoint.Z + 100.0f : 230.0f;
				C->TeleportTo(FVector(X, Y, Z), FRotator(0.0f, 90.0f, 0.0f));
				if (APlayerController* PC = Cast<APlayerController>(C->GetController()))
				{
					PC->ClientSetRotation(FRotator(-6.0f, 90.0f, 0.0f));
					PC->SetControlRotation(FRotator(-6.0f, 90.0f, 0.0f));
				}
			};
			Place(Angler, 2900.0f, 7530.0f);
			if (Me)
			{
				Place(Me, 2620.0f, 7600.0f);
			}
			UKGPlayerExtrasSubsystem::EnsurePlayerComponents(Angler->GetPlayerState());
			if (UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Angler))
			{
				Pockets->AddItem(FKGItemIds::FishingRod, 1);
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SMOKE Host placed %s at %s, rod given"), *Body, *Angler->GetActorLocation().ToCompactString());
			SmokeStep = 1;
		}
		if (!bSmokeForced && Fishing->GetServerPhase() == EKGFishPhase::Waiting)
		{
			bSmokeForced = true;
			const FKGFishLine& L = Fishing->GetLine();
			UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SEEN %s tag=line body=%s local=0 rod=%d phase=Waiting water=%s landing=%s (bobber on the water)"),
			       Machine, *Body, Fishing->IsRodShown() ? 1 : 0,
			       *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(L.Water)), *FVector(L.Landing).ToCompactString());
			Fishing->ServerForceBite(TEXT("Mackerel"));
			UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SMOKE Host forced a bite for %s"), *Body);
		}
		if (SmokeClock > 150.0f)
		{
			Finish(TEXT("timeout"));
		}
		return;
	}

	// ---- client: the angler ----
	if (SmokeStep == 0 && SmokeClock >= 6.0f)
	{
		const UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Angler);
		if (Pockets && Pockets->Has(FKGItemIds::FishingRod))
		{
			Fishing->RequestSetRod(true);
			SmokeStep = 1;
			UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SMOKE Client rod out requested"));
		}
	}
	else if (SmokeStep == 1 && Fishing->GetServerPhase() == EKGFishPhase::Ready && Fishing->IsRodShown() && SmokeClock >= 9.0f)
	{
		if (LocalPC)
		{
			LocalPC->SetControlRotation(FRotator(-6.0f, 90.0f, 0.0f));
		}
		Fishing->CastWithPower(0.55f);
		SmokeStep = 2;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SMOKE Client cast from %s"), *Angler->GetActorLocation().ToCompactString());
	}
	else if (SmokeStep == 2 && Fishing->GetServerPhase() == EKGFishPhase::Ready && SmokeClock >= 14.0f)
	{
		SmokeStep = 1;   // the cast was refused or snagged: try again
	}
	if (!bSmokeHooked && Fishing->GetShownPhase() == EKGFishPhase::Bite)
	{
		bSmokeHooked = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SEEN Client tag=bite body=%s"), *Body);
		Fishing->RequestHook();
	}
	if (Fishing->GetServerPhase() == EKGFishPhase::Fight && Fishing->HasShownSim())
	{
		Fishing->SetScriptedReelInput(FKGReelSim::ExpertInput(Fishing->GetShownSim()), 0.25f);
	}
	if (SmokeReportAt > 0.0f && SmokeClock >= SmokeReportAt)
	{
		const UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(Angler);
		const FName Item = Catch.ItemId;
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_POCKETS Client item=%s count=%d grams=%d"), *Item.ToString(),
		       Pockets ? Pockets->Count(Item) : -1, Pockets ? Pockets->GramsOf(Item) : -1);
		Finish(TEXT("ok"));
		return;
	}
	if (SmokeClock > 150.0f)
	{
		Finish(TEXT("timeout"));
	}
#endif
}
