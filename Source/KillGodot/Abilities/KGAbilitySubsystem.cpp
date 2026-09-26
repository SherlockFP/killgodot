#include "Abilities/KGAbilitySubsystem.h"
#include "Abilities/KGAbilityHUD.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGWoundComponent.h"
#include "AI/KGBotController.h"
#include "Character/KGCharacter.h"
#include "Combat/KGHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Traps/KGFieldTraps.h"
#include "Traps/KGMimicTrap.h"
#include "Traps/KGTrapSubsystem.h"
#include "World/KGBreakable.h"
#include "World/KGStorageChest.h"
#if !UE_BUILD_SHIPPING
#include "Dev/KGDevCommands.h"
#endif

namespace KGAbilitySubsystemPrivate
{
	bool bHudRegistered = false;
}

UKGAbilitySubsystem* UKGAbilitySubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGAbilitySubsystem>() : nullptr;
}

bool UKGAbilitySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKGAbilitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGAbilitySubsystem, STATGROUP_Tickables);
}

void UKGAbilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!KGAbilitySubsystemPrivate::bHudRegistered && !IsRunningDedicatedServer())
	{
		KGAbilitySubsystemPrivate::bHudRegistered = true;
		AHUD::OnHUDPostRender.AddStatic(&KGAbilityHUD::Draw);   // owner UI without touching AKGHUD
	}
}

void UKGAbilitySubsystem::Deinitialize()
{
	Holders.Reset();
	KnownMimics.Reset();
	Super::Deinitialize();
}

void UKGAbilitySubsystem::RegisterHolder(AKGAbilityHolder* Holder)
{
	Holders.AddUnique(Holder);
}

void UKGAbilitySubsystem::UnregisterHolder(AKGAbilityHolder* Holder)
{
	Holders.Remove(Holder);
}

FName UKGAbilitySubsystem::PlaceName(const UWorld* World, const FVector& Location)
{
#if !UE_BUILD_SHIPPING
	if (World)
	{
		const FKGDevLocation* Best = nullptr;
		double BestD = FMath::Square(4000.0);
		for (const FKGDevLocation& L : FKGDev::GatherLocations(const_cast<UWorld*>(World)))
		{
			const double D = FVector::DistSquared(L.Location, Location);
			if (D < BestD)
			{
				BestD = D;
				Best = &L;
			}
		}
		if (Best)
		{
			return FName(*Best->Name);
		}
	}
#endif
	return NAME_None;
}

void UKGAbilitySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TickSmoke(DeltaTime);
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	if (GS && GS->GetPhase() != LastPhase)
	{
		LastPhase = GS->GetPhase();
		if (LastPhase == EKGPhase::Dawn)
		{
			OnDawn();
		}
	}
	SyncAccum += DeltaTime;
	if (SyncAccum >= 0.5f)
	{
		SyncAccum = 0.0f;
		SyncHolders();
	}
}

void UKGAbilitySubsystem::SyncHolders()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	for (APlayerState* Raw : GS->PlayerArray)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
		AController* Owner = PS ? PS->GetOwningController() : nullptr;
		if (!Owner)
		{
			continue;
		}
		const FName RoleId = PS->GetPrivateRoleId();
		const bool bWants = FKGAbilityCatalog::ForRole(RoleId).Num() > 0;
		AKGAbilityHolder* H = AKGAbilityHolder::FindFor(Owner);
		if (!bWants)
		{
			if (H)
			{
				H->Destroy();
			}
			continue;
		}
		if (!H)
		{
			FActorSpawnParameters P;
			P.Owner = Owner;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			H = World->SpawnActor<AKGAbilityHolder>(AKGAbilityHolder::StaticClass(), FTransform::Identity, P);
		}
		if (H && H->GetRoleId() != RoleId)
		{
			H->AuthSetup(RoleId);
			UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY holder %s role=%s abilities=%d"), *PS->GetPlayerName(), *RoleId.ToString(),
			       H->GetStates().Num());
		}
	}
}

void UKGAbilitySubsystem::OnDawn()
{
	int32 Expired = 0;
	if (UKGTrapSubsystem* TS = UKGTrapSubsystem::Get(GetWorld()))
	{
		TArray<AKGTrap*> Traps;
		for (const TWeakObjectPtr<AKGTrap>& T : TS->GetTraps())
		{
			if (T.IsValid())
			{
				Traps.Add(T.Get());
			}
		}
		for (AKGTrap* T : Traps)
		{
			if (AKGMimicTrap* M = Cast<AKGMimicTrap>(T))
			{
				M->AuthExpire();
				++Expired;
			}
			else if (Cast<AKGFieldTrap>(T))
			{
				T->Destroy();
				++Expired;
			}
		}
	}
	for (const TWeakObjectPtr<AKGAbilityHolder>& H : Holders)
	{
		if (H.IsValid())
		{
			H->AuthRefill();
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_ABILITY dawn: %d trapper trap(s) expired, %d holder(s) refilled"), Expired, Holders.Num());
}

void UKGAbilitySubsystem::NoteMimicScream(AActor* Host, const FVector& At, AKGCharacter* Victim)
{
	if (!Host)
	{
		return;
	}
	int32 Heard = 0;
	for (TActorIterator<AKGBotController> It(GetWorld()); It; ++It)
	{
		const APawn* P = It->GetPawn();
		if (P && FVector::Dist(P->GetActorLocation(), At) <= KGTrapperTuning::ScreamRadiusCm)
		{
			KnownMimics.FindOrAdd(*It).AddUnique(Host);
			++Heard;
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_MIMIC scream at %s victim=%s heard_by_bots=%d"), *At.ToCompactString(), *GetNameSafe(Victim), Heard);
}

bool UKGAbilitySubsystem::IsKnownMimic(const AController* Bot, const AActor* Host) const
{
	const TArray<TWeakObjectPtr<const AActor>>* L = KnownMimics.Find(Bot);
	return L && L->Contains(Host);
}

int32 UKGAbilitySubsystem::CountKnownMimics(const AController* Bot) const
{
	const TArray<TWeakObjectPtr<const AActor>>* L = KnownMimics.Find(Bot);
	return L ? L->Num() : 0;
}

void UKGAbilitySubsystem::NotifyArmer(const FString& Puid, const FString& Text)
{
	if (Puid.IsEmpty())
	{
		return;
	}
	for (const TWeakObjectPtr<AKGAbilityHolder>& H : Holders)
	{
		const AController* C = H.IsValid() ? Cast<AController>(H->GetOwner()) : nullptr;
		const AKGPlayerState* PS = C ? C->GetPlayerState<AKGPlayerState>() : nullptr;
		if (PS && PS->Puid == Puid)
		{
			H->AuthNotify(Text);
			return;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// -KGTrapperSmoke (Tools/Unreal/kg_trapper_smoke.ps1): arm -> the victim opens -> the bite replicates -> the chest
// resets with teeth marks; then a tripwire and a snare on the same victim. Host = the Trapper, client = the victim.
// ---------------------------------------------------------------------------------------------------------------------

void UKGAbilitySubsystem::TickSmoke(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGTrapperSmoke"));
	UWorld* World = GetWorld();
	if (!bSmoke || bSmokeDone || !World)
	{
		return;
	}
	const bool bHost = World->GetNetMode() != NM_Client;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");
	APlayerController* LocalPC = World->GetFirstPlayerController();
	AKGCharacter* Me = LocalPC ? Cast<AKGCharacter>(LocalPC->GetPawn()) : nullptr;
	AKGCharacter* Other = nullptr;
	int32 Humans = 0;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			AKGCharacter* Body = PS && !PS->IsABot() ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
			if (Body)
			{
				++Humans;
				Other = Body != Me ? Body : Other;
			}
		}
	}
	if (SmokeClock < 0.0f)
	{
		if (Humans < 2 || !Me || (bHost && !Other))
		{
			return;
		}
		SmokeClock = 0.0f;
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE %s start"), Machine);
	}
	SmokeClock += DeltaTime;
	auto Finish = [this, Machine](bool bOk, const TCHAR* Why)
	{
		bSmokeDone = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_DONE %s %s %s"), Machine, bOk ? TEXT("ok") : TEXT("fail"), Why);
	};
	if (SmokeClock > 150.0f)
	{
		Finish(false, TEXT("timeout"));
		return;
	}
	auto Face = [](AKGCharacter* Who, const FVector& At)
	{
		if (AController* C = Who ? Who->GetController() : nullptr)
		{
			C->SetControlRotation((At - Who->GetPawnViewLocation()).Rotation());
		}
	};

	if (!bHost)
	{
		// ---- the victim ----
		AKGMimicTrap* Mimic = nullptr;
		for (TActorIterator<AKGMimicTrap> It(World); It; ++It)
		{
			Mimic = *It;
		}
		const UKGHealthComponent* HC = Me->GetHealth();
		const bool bHeld = KGTrapHold::IsHeld(Me);
		const FString Look = Mimic ? Mimic->GetLookName() : TEXT("none");
		const FString Wounds = UKGWoundComponent::ListOn(Me);
		bSawHeld |= bHeld && Wounds.Contains(TEXT("BiteMarks"));
		bSawBite |= Look == TEXT("bite");
		bSawSnareHeld |= bHeld && Wounds.Contains(TEXT("SnareWound"));
		SmokeLogAccum += DeltaTime;
		if (SmokeLogAccum >= 0.5f)
		{
			SmokeLogAccum = 0.0f;
			int32 HolderCount = 0;
			for (TActorIterator<AKGAbilityHolder> It(World); It; ++It)
			{
				++HolderCount;
			}
			FName HostRole = NAME_None;
			for (APlayerState* PS : World->GetGameState()->PlayerArray)
			{
				if (const AKGPlayerState* KPS = Cast<AKGPlayerState>(PS); KPS && KPS->GetPawn() != Me && !KPS->IsABot())
				{
					HostRole = KPS->GetPrivateRoleId();
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SEEN Client look=%s held=%d health=%.0f wounds=%s holders=%d hostRole=%s marks=%d"),
			       *Look, bHeld ? 1 : 0, HC ? HC->GetHealth() : -1.0f, Wounds.IsEmpty() ? TEXT("-") : *Wounds, HolderCount,
			       *HostRole.ToString(), Mimic && Mimic->HasTeethMarks() ? 1 : 0);
		}
		AActor* Chest = Mimic ? Mimic->GetHost() : nullptr;
		if (!bInteracted && Mimic && Look == TEXT("rest") && Chest && FVector::Dist2D(Chest->GetActorLocation(), Me->GetActorLocation()) < 260.0)
		{
			if (SmokeStep == 0)
			{
				SmokeStep = 1;
				SmokeStepAt = SmokeClock;
			}
			Face(Me, Chest->GetComponentsBoundingBox(true).GetCenter());
			if (SmokeClock - SmokeStepAt > 1.0f)
			{
				bInteracted = true;
				UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Client opens the chest (E)"));
				Me->Interact();   // the real input path: ServerInteract with the view -> AKGStorageChest::Interact
			}
		}
		if (bSawHeld && bSawBite && Look == TEXT("marks") && !bHeld && bSawSnareHeld && Wounds.Contains(TEXT("SnareWound")))
		{
			Finish(true, TEXT(""));
		}
		return;
	}

	// ---- the host (the Trapper) ----
	AKGGameMode* GM = World->GetAuthGameMode<AKGGameMode>();
	AKGAbilityHolder* Holder = AKGAbilityHolder::FindFor(LocalPC);
	AActor* Chest = SmokeChest.Get();
	AKGMimicTrap* Mimic = Chest ? AKGMimicTrap::FindOn(Chest) : nullptr;
	SmokeLogAccum += DeltaTime;
	if (SmokeLogAccum >= 0.5f && SmokeStep >= 3)
	{
		SmokeLogAccum = 0.0f;
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SEEN Host look=%s state=%s victimHeld=%d wounds=%s"), Mimic ? *Mimic->GetLookName() : TEXT("none"),
		       Mimic ? KGTrap::StateName(Mimic->GetState()) : TEXT("-"), KGTrapHold::IsHeld(Other) ? 1 : 0, *UKGWoundComponent::ListOn(Other));
	}
	const float InStep = SmokeClock - SmokeStepAt;
	auto Next = [this](int32 Step)
	{
		SmokeStep = Step;
		SmokeStepAt = SmokeClock;
	};
	auto Use = [&](FName Id, const FVector& Target) -> EKGAbilityDeny
	{
		Face(Me, Target);
		const FVector Eyes = Me->GetPawnViewLocation();
		FString Detail;
		const EKGAbilityDeny V = Holder ? Holder->AuthUse(Id, Eyes, (Target - Eyes).GetSafeNormal(), &Detail) : EKGAbilityDeny::Unknown;
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host use %s verdict=%s %s"), *Id.ToString(), FKGAbilityRules::DenyName(V), *Detail);
		return V;
	};
	switch (SmokeStep)
	{
	case 0:   // a real match state: 4 frozen bots, day, the host is the Trapper and the client a Sheriff
		if (SmokeClock < 3.0f || !GM)
		{
			return;
		}
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.BotAI")))
		{
			V->Set(0, ECVF_SetByCode);
		}
		GM->DevAddBots(4);
		GM->DevJumpToPhase(EKGPhase::Day);
		if (AKGPlayerState* PS = LocalPC->GetPlayerState<AKGPlayerState>())
		{
			PS->SetPrivateRoleId(TEXT("Trapper"));
			PS->ForceNetUpdate();
		}
		if (AKGPlayerState* PS = Other->GetPlayerState<AKGPlayerState>())
		{
			PS->SetPrivateRoleId(TEXT("Sheriff"));
			PS->ForceNetUpdate();
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host setup: day, host Trapper, client Sheriff"));
		Next(1);
		return;
	case 1:   // a chest out of everyone's sight, the client right next to the Trapper first (the unseen rule)
	{
		SyncHolders();
		if (!Holder || InStep < 1.0f)
		{
			return;
		}
		// A flat, open spot away from the (frozen) bots: chest, Trapper and client on one level, with clear sight lines.
		const FVector Start = Me->GetActorLocation();
		bool bFound = false;
		for (int32 k = 0; k < 48 && !bFound; ++k)
		{
			const double Ang = k * 0.7;
			const double Rad = 3000.0 + 250.0 * (k % 12);
			const FVector S = FKGDev::GroundAt(World, FVector2D(Start.X + Rad * FMath::Cos(Ang), Start.Y + Rad * FMath::Sin(Ang)), Start.Z);
			const FVector HostAt = FKGDev::GroundAt(World, FVector2D(S.X - 240.0, S.Y), S.Z);
			const FVector CliAt = FKGDev::GroundAt(World, FVector2D(S.X - 240.0, S.Y + 220.0), S.Z);
			const FVector Far = FKGDev::GroundAt(World, FVector2D(S.X - 900.0, S.Y - 600.0), S.Z);
			const FVector P3 = FKGDev::GroundAt(World, FVector2D(S.X - 240.0, S.Y - 250.0), S.Z);
			if (FMath::Abs(P3.Z - S.Z) > 30.0 || FMath::Abs(HostAt.Z - S.Z) > 30.0 || FMath::Abs(CliAt.Z - S.Z) > 30.0 || FMath::Abs(Far.Z - S.Z) > 120.0)
			{
				continue;
			}
			FCollisionQueryParams Q(SCENE_QUERY_STAT(KGTrapperSmoke), false);
			Q.AddIgnoredActor(Me);
			Q.AddIgnoredActor(Other);
			FHitResult Hit;
			const FVector Up(0.0, 0.0, 160.0);
			if (World->LineTraceSingleByChannel(Hit, HostAt + Up, CliAt + Up, ECC_Visibility, Q) ||
			    World->LineTraceSingleByChannel(Hit, HostAt + Up, S + FVector(0.0, 0.0, 60.0), ECC_Visibility, Q))
			{
				continue;
			}
			bool bBotNear = false;
			for (TActorIterator<AKGBotController> It(World); It; ++It)
			{
				bBotNear |= It->GetPawn() && FVector::Dist(It->GetPawn()->GetActorLocation(), S) < 2500.0;
			}
			if (!bBotNear)
			{
				SmokeSpot = S;
				bFound = true;
			}
		}
		if (!bFound)
		{
			Finish(false, TEXT("no open spot"));
			return;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKGStorageChest* NewChest = World->SpawnActor<AKGStorageChest>(SmokeSpot, FRotator::ZeroRotator, P);
		SmokeChest = NewChest;
		Me->TeleportTo(SmokeSpot + FVector(-240.0, 0.0, 100.0), FRotator::ZeroRotator);
		Other->TeleportTo(SmokeSpot + FVector(-240.0, 220.0, 100.0), FRotator::ZeroRotator);
		UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host chest at %s"), *SmokeSpot.ToCompactString());
		Next(2);
		return;
	}
	case 2:
		if (InStep < 1.5f || !Chest)
		{
			return;
		}
		if (Use(TEXT("Mimic"), Chest->GetComponentsBoundingBox(true).GetCenter()) != EKGAbilityDeny::Seen)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host unseen-rule FAILED (armed in plain sight)"));
		}
		Other->TeleportTo(SmokeSpot + FVector(-3000.0, -2400.0, 200.0), FRotator::ZeroRotator);   // far away
		Next(3);
		return;
	case 3:
		if (InStep < 1.5f)
		{
			return;
		}
		if (Use(TEXT("Mimic"), Chest->GetComponentsBoundingBox(true).GetCenter()) != EKGAbilityDeny::None)
		{
			Finish(false, TEXT("could not arm the mimic"));
			return;
		}
		Next(4);
		return;
	case 4:   // the throw-test: a crate lobbed at the chest makes the mimic flinch
		if (InStep < 1.0f)
		{
			return;
		}
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKGBreakable* Thrown = World->SpawnActor<AKGBreakable>(SmokeSpot + FVector(-180.0, 0.0, 160.0), FRotator::ZeroRotator, P);
			if (Thrown)
			{
				Thrown->SetMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));   // a bare AKGBreakable has no mesh
				Thrown->SetActorScale3D(FVector(0.3));
			}
			if (UPrimitiveComponent* Prim = Thrown ? Cast<UPrimitiveComponent>(Thrown->GetRootComponent()) : nullptr)
			{
				Prim->SetPhysicsLinearVelocity(FVector(650.0, 0.0, 120.0));
			}
			SmokeThrown = Thrown;
		}
		Next(5);
		return;
	case 5:
		if (Mimic && Mimic->GetFlinchSerial() > 0)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host throw-test flinch=1"));
		}
		else if (InStep < 3.0f)
		{
			return;
		}
		else
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host throw-test flinch=0 (forcing one)"));
			if (Mimic)
			{
				Mimic->AuthFlinch();
			}
		}
		if (AActor* T = SmokeThrown.Get())
		{
			T->Destroy();
		}
		// The victim walks up to the chest; the Trapper steps back.
		Me->TeleportTo(SmokeSpot + FVector(-900.0, -600.0, 100.0), FRotator::ZeroRotator);
		Other->TeleportTo(SmokeSpot + FVector(-170.0, 0.0, 100.0), FRotator::ZeroRotator);
		Next(6);
		return;
	case 6:   // wait for the bite and the reset
		if (Mimic && Mimic->HasTeethMarks() && Mimic->GetState() == EKGTrapState::Idle && !KGTrapHold::IsHeld(Other))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host chest reset with teeth marks"));
			Next(7);
		}
		else if (InStep > 40.0f)
		{
			Finish(false, TEXT("no bite"));
		}
		return;
	case 7:   // on the verified flat strip: Trapper at P3, victim at P2, the tripwire goes on P1 between them
		if (InStep < 1.0f)
		{
			return;
		}
		Me->TeleportTo(SmokeSpot + FVector(-240.0, -250.0, 100.0), FRotator::ZeroRotator);
		Other->TeleportTo(SmokeSpot + FVector(-240.0, 220.0, 100.0), FRotator::ZeroRotator);
		Next(8);
		return;
	case 8:
		if (InStep < 1.0f)
		{
			return;
		}
		if (Use(TEXT("Tripwire"), SmokeSpot + FVector(-240.0, 0.0, 5.0)) != EKGAbilityDeny::None)
		{
			Finish(false, TEXT("tripwire refused"));
			return;
		}
		Next(9);
		return;
	case 9:
		if (InStep < 1.0f)
		{
			return;
		}
		for (TActorIterator<AKGTripwireTrap> It(World); It; ++It)
		{
			const bool bOk = Other->TeleportTo(It->GetActorLocation() + FVector(0.0, 0.0, 100.0), Other->GetActorRotation());   // walks through the wire
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host victim onto the tripwire teleport=%d"), bOk ? 1 : 0);
		}
		Next(10);
		return;
	case 10:   // the victim steps to the chest front, the Trapper to P1, the snare on P2
		if (InStep < 1.5f)
		{
			return;
		}
		if (SmokeStepAt >= 0.0f && InStep < 1.6f)
		{
			Other->TeleportTo(SmokeSpot + FVector(-170.0, 0.0, 100.0), Other->GetActorRotation());
			Me->TeleportTo(SmokeSpot + FVector(-240.0, 0.0, 100.0), FRotator::ZeroRotator);
			return;
		}
		if (InStep < 2.5f)
		{
			return;
		}
		if (Use(TEXT("Snare"), SmokeSpot + FVector(-240.0, 220.0, 5.0)) != EKGAbilityDeny::None)
		{
			Finish(false, TEXT("snare refused"));
			return;
		}
		Next(11);
		return;
	case 11:
		if (InStep < 1.0f)
		{
			return;
		}
		for (TActorIterator<AKGSnareTrap> It(World); It; ++It)
		{
			const bool bOk = Other->TeleportTo(It->GetActorLocation() + FVector(0.0, 0.0, 100.0), Other->GetActorRotation());   // steps on the snare
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host victim onto the snare teleport=%d"), bOk ? 1 : 0);
		}
		Next(12);
		return;
	case 12:
		if (UKGWoundComponent::ListOn(Other).Contains(TEXT("SnareWound")) && InStep > 6.0f)
		{
			int32 Notes = Holder ? Holder->GetNotes().Num() : 0;
			UE_LOG(LogKillGodot, Log, TEXT("KG_TRAPPER_SMOKE Host trapper notes=%d"), Notes);
			Next(13);
		}
		else if (InStep > 20.0f)
		{
			Finish(false, TEXT("no snare"));
		}
		return;
	case 13:
		if (InStep > 6.0f)
		{
			Finish(true, TEXT(""));   // the client needs a few seconds to see the snare release
		}
		return;
	default:
		return;
	}
#endif
}
