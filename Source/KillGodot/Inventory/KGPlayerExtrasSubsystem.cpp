#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "Character/KGCharacter.h"
#include "Core/KGPlayerState.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryRPCComponent.h"
#include "Inventory/KGInventoryUI.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "KillGodot.h"
#include "World/KGPickup.h"
#include "World/KGSeat.h"
#include "World/KGStorageChest.h"

bool UKGPlayerExtrasSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGPlayerExtrasSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKGPlayerExtrasSubsystem::HandleActorSpawned));
}

void UKGPlayerExtrasSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Pending.Reset();
	Super::Deinitialize();
}

TStatId UKGPlayerExtrasSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGPlayerExtrasSubsystem, STATGROUP_Tickables);
}

void UKGPlayerExtrasSubsystem::HandleActorSpawned(AActor* Actor)
{
	// Deferred to the next tick: the player state may still be mid-construction here.
	if (APlayerState* PS = Cast<AKGPlayerState>(Actor))
	{
		Pending.Add(PS);
	}
}

void UKGPlayerExtrasSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// I opens the pockets window for local players (polled so AKGCharacter's input setup stays untouched).
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && Cast<AKGCharacter>(PC->GetPawn()) &&
			    PC->WasInputKeyJustPressed(EKeys::I) && !FKGInventoryUI::IsOpen(PC))
			{
				FKGInventoryUI::Open(PC);
			}
		}
	}
	if (World->GetNetMode() == NM_Client)
	{
		Pending.Reset();
		return;
	}
	for (const TWeakObjectPtr<APlayerState>& PS : Pending)
	{
		EnsurePlayerComponents(PS.Get());
	}
	Pending.Reset();

	// Safety sweep (seamless travel, anything spawned before this subsystem existed).
	SweepSeconds += DeltaTime;
	if (SweepSeconds >= 1.0f)
	{
		SweepSeconds = 0.0f;
		if (const AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				EnsurePlayerComponents(PS);
			}
		}
	}
}

void UKGPlayerExtrasSubsystem::EnsurePlayerComponents(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || PlayerState->IsActorBeingDestroyed() ||
	    !PlayerState->IsA<AKGPlayerState>())
	{
		return;
	}
	auto Ensure = [PlayerState](UClass* Class, const TCHAR* Name)
	{
		if (PlayerState->FindComponentByClass(Class))
		{
			return;
		}
		UActorComponent* Component = NewObject<UActorComponent>(PlayerState, Class, FName(Name));
		Component->SetIsReplicated(true);
		PlayerState->AddInstanceComponent(Component);
		Component->RegisterComponent();
	};
	Ensure(UKGInventoryComponent::StaticClass(), TEXT("KGInventory"));
	Ensure(UKGInventoryRPCComponent::StaticClass(), TEXT("KGInventoryRelay"));
	Ensure(UKGCosmeticsComponent::StaticClass(), TEXT("KGCosmetics"));
}

// ---------------------------------------------------------------------------------------------------------------
// Dev spawners (KGExtrasDev, shared with the dev panel) and the console commands on top of them. In PIE the
// commands work from any window: a client window forwards to the server world's twin of its player (same PlayerId,
// same process). They do not work on a real remote client (the dev panel's kg.* verbs do, through a server RPC).
// ---------------------------------------------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
namespace KGExtrasDev
{
	/** Ground point ~Distance in front of the authoritative pawn, plus the pawn's yaw. */
	static bool InFront(APlayerState* PS, float Distance, FVector& OutLocation, float& OutYaw)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn)
		{
			return false;
		}
		OutYaw = Pawn->GetActorRotation().Yaw;
		const FVector Forward = FRotator(0.0f, OutYaw, 0.0f).Vector();
		OutLocation = Pawn->GetActorLocation() + Forward * Distance;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGConsoleGround), false, Pawn);
		if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, OutLocation + FVector(0, 0, 100.0f),
		                                               OutLocation - FVector(0, 0, 400.0f), ECC_WorldStatic, Params))
		{
			OutLocation = Hit.ImpactPoint;
		}
		return true;
	}

	int32 GiveItem(APlayerState* For, FName ItemId, int32 Count)
	{
		UKGPlayerExtrasSubsystem::EnsurePlayerComponents(For);
		UKGInventoryComponent* Inventory = UKGInventoryComponent::FindForPlayer(For);
		if (ItemId.IsNone() || !Inventory)
		{
			return 0;
		}
		return Inventory->AddItem(ItemId, FMath::Max(1, Count));
	}

	AActor* SpawnPickup(APlayerState* For, FName ItemId, int32 Count)
	{
		FVector Where;
		float Yaw;
		if (ItemId.IsNone() || !InFront(For, 150.0f, Where, Yaw))
		{
			return nullptr;
		}
		return AKGPickup::SpawnPickup(For->GetWorld(), ItemId, FMath::Max(1, Count), Where);
	}

	int32 SpawnLoot(APlayerState* For, FName Table, int32 Seed)
	{
		FVector Where;
		float Yaw;
		if (!InFront(For, 180.0f, Where, Yaw))
		{
			return 0;
		}
		FKGRng Rng(static_cast<uint64>(Seed));
		return FKGLoot::RollAndSpawn(For->GetWorld(), Table.IsNone() ? FName(TEXT("Crate")) : Table, Rng, Where).Num();
	}

	AActor* SpawnChest(APlayerState* For, bool bLocked, bool bMine, FName LootTable)
	{
		FVector Where;
		float Yaw;
		if (!InFront(For, 160.0f, Where, Yaw))
		{
			return nullptr;
		}
		const FTransform Transform(FRotator(0.0f, Yaw + 180.0f, 0.0f), Where);
		AKGStorageChest* Chest = For->GetWorld()->SpawnActorDeferred<AKGStorageChest>(
			AKGStorageChest::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Chest)
		{
			return nullptr;
		}
		Chest->StartingLootTable = LootTable.IsNone() ? FName(TEXT("Chest")) : LootTable;
		Chest->LootSeed = static_cast<int32>(For->GetWorld()->GetTimeSeconds() * 1000.0);
		Chest->FinishSpawning(Transform);
		if (bMine)
		{
			if (AKGPlayerState* KGPS = Cast<AKGPlayerState>(For))
			{
				if (KGPS->Puid.IsEmpty())
				{
					// PIE has no EOS login: give this player a dev PUID so ownership can be tested.
					KGPS->Puid = FString::Printf(TEXT("DEV-%d"), For->GetPlayerId());
					KGPS->ForceNetUpdate();
				}
				Chest->SetHouseOwner(KGPS->Puid);
			}
		}
		Chest->SetContainerLocked(bLocked);
		UE_LOG(LogKillGodot, Log, TEXT("SpawnChest %s locked=%d owner='%s'"), *Chest->GetName(),
		       Chest->IsContainerLocked() ? 1 : 0, *Chest->OwnerPuid);
		return Chest;
	}

	AActor* SpawnSeat(APlayerState* For, const FString& Kind)
	{
		FVector Where;
		float Yaw;
		if (!InFront(For, 140.0f, Where, Yaw))
		{
			return nullptr;
		}
		const FTransform Transform(FRotator(0.0f, Yaw + 180.0f, 0.0f), Where);
		AKGSeat* Seat = For->GetWorld()->SpawnActorDeferred<AKGSeat>(AKGSeat::StaticClass(), Transform, nullptr, nullptr,
		                                                            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Seat)
		{
			return nullptr;
		}
		if (Kind.Equals(TEXT("Chair"), ESearchCase::IgnoreCase) || Kind.Equals(TEXT("Bench"), ESearchCase::IgnoreCase))
		{
			const bool bChair = Kind.Equals(TEXT("Chair"), ESearchCase::IgnoreCase);
			const TCHAR* Path = bChair ? TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Chair_1.Chair_1")
			                           : TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Bench.Bench");
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
			{
				Seat->SetSeatMesh(Mesh);
			}
			Seat->SeatHeight = bChair ? 46.0f : 51.0f;
		}
		Seat->FinishSpawning(Transform);
		return Seat;
	}
}

namespace KGExtrasConsole
{
	APlayerController* LocalPC(UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	/** The authoritative copy of the local player's state (the listen server's own, or its PIE server twin). */
	APlayerState* AuthorityPlayer(UWorld* World)
	{
		const APlayerController* PC = LocalPC(World);
		APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
		if (!PS)
		{
			return nullptr;
		}
		if (World->GetNetMode() != NM_Client)
		{
			return PS;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Other = Context.World();
			if (!Other || Other == World || Other->GetNetMode() == NM_Client || Context.WorldType != EWorldType::PIE)
			{
				continue;
			}
			if (const AGameStateBase* GS = Other->GetGameState())
			{
				for (APlayerState* Candidate : GS->PlayerArray)
				{
					if (Candidate && Candidate->GetPlayerId() == PS->GetPlayerId())
					{
						return Candidate;
					}
				}
			}
		}
		UE_LOG(LogKillGodot, Warning, TEXT("kg.*: no server world in this process (run the command on the host)"));
		return nullptr;
	}

	FName ItemArg(const TArray<FString>& Args, int32 Index)
	{
		if (!Args.IsValidIndex(Index))
		{
			return NAME_None;
		}
		const FName Id = UKGItemCatalog::ResolveLoose(Args[Index]);
		if (Id.IsNone())
		{
			UE_LOG(LogKillGodot, Warning, TEXT("Unknown item '%s' (kg.ListItems)"), *Args[Index]);
		}
		return Id;
	}

	int32 IntArg(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	using FWorldArgs = FConsoleCommandWithWorldAndArgsDelegate;

	FAutoConsoleCommandWithWorldAndArgs GiveItem(
		TEXT("kg.GiveItem"), TEXT("kg.GiveItem <Item> [Count=1] : put items in your pockets (server authoritative)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName Id = ItemArg(Args, 0);
			APlayerState* PS = AuthorityPlayer(World);
			const int32 Wanted = FMath::Max(1, IntArg(Args, 1, 1));
			const int32 Added = KGExtrasDev::GiveItem(PS, Id, Wanted);
			const UKGInventoryComponent* Inventory = UKGInventoryComponent::FindForPlayer(PS);
			UE_LOG(LogKillGodot, Log, TEXT("kg.GiveItem %s x%d -> added %d (now %d)"), *Id.ToString(), Wanted, Added,
			       Inventory ? Inventory->Count(Id) : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs TakeItem(
		TEXT("kg.TakeItem"), TEXT("kg.TakeItem <Item> [Count=1] : remove items from your pockets"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName Id = ItemArg(Args, 0);
			if (UKGInventoryComponent* Inventory = UKGInventoryComponent::FindForPlayer(AuthorityPlayer(World)); Inventory && !Id.IsNone())
			{
				const int32 Removed = Inventory->RemoveItem(Id, FMath::Max(1, IntArg(Args, 1, 1)));
				UE_LOG(LogKillGodot, Log, TEXT("kg.TakeItem %s -> removed %d"), *Id.ToString(), Removed);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ClearInventory(
		TEXT("kg.ClearInventory"), TEXT("Empty your pockets"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UKGInventoryComponent* Inventory = UKGInventoryComponent::FindForPlayer(AuthorityPlayer(World)))
			{
				Inventory->Clear();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ShowInventory(
		TEXT("kg.Inventory"), TEXT("Toggle the inventory window (pockets) for this window's player"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FKGInventoryUI::Toggle(LocalPC(World));
		}));

	FAutoConsoleCommandWithWorldAndArgs ListItems(
		TEXT("kg.ListItems"), TEXT("Log the item catalog"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (const FKGItemDef& Def : UKGItemCatalog::GetAll())
			{
				UE_LOG(LogKillGodot, Log, TEXT("  %-16s %-14s stack %4d  value %4d  %s"), *Def.Id.ToString(),
				       *Def.DisplayName.ToString(), Def.StackSize, Def.GoldValue,
				       *UKGItemCatalog::GetRarityName(Def.Rarity).ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SpawnPickup(
		TEXT("kg.SpawnPickup"), TEXT("kg.SpawnPickup <Item> [Count=1] : drop a pickup 1.5 m in front of you"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			KGExtrasDev::SpawnPickup(AuthorityPlayer(World), ItemArg(Args, 0), IntArg(Args, 1, 1));
		}));

	FAutoConsoleCommandWithWorldAndArgs SpawnLoot(
		TEXT("kg.SpawnLoot"), TEXT("kg.SpawnLoot <Crate|Barrel|Pot|Chest|Grave|Fishing> [Seed=1] : roll a loot table in front of you"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Spawned = KGExtrasDev::SpawnLoot(AuthorityPlayer(World),
			                                             FName(Args.IsValidIndex(0) ? *Args[0] : TEXT("Crate")),
			                                             IntArg(Args, 1, 1));
			UE_LOG(LogKillGodot, Log, TEXT("kg.SpawnLoot -> %d pickups"), Spawned);
		}));

	FAutoConsoleCommandWithWorldAndArgs SpawnChest(
		TEXT("kg.SpawnChest"),
		TEXT("kg.SpawnChest [Locked=0] [Mine=0] [LootTable=Chest] : storage chest in front of you (Mine=1 makes you the owner)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			KGExtrasDev::SpawnChest(AuthorityPlayer(World), IntArg(Args, 0, 0) != 0, IntArg(Args, 1, 0) != 0,
			                        FName(Args.IsValidIndex(2) ? *Args[2] : TEXT("Chest")));
		}));

	FAutoConsoleCommandWithWorldAndArgs SpawnSeat(
		TEXT("kg.SpawnSeat"), TEXT("kg.SpawnSeat [Stool|Chair|Bench] : a seat in front of you, facing you"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			KGExtrasDev::SpawnSeat(AuthorityPlayer(World), Args.IsValidIndex(0) ? Args[0] : TEXT("Stool"));
		}));

	FAutoConsoleCommandWithWorldAndArgs StandUp(
		TEXT("kg.StandUp"), TEXT("Stand up from the seat you are on"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UKGInventoryRPCComponent* Relay = UKGInventoryRPCComponent::FindForController(LocalPC(World)))
			{
				Relay->RequestStandUp();
			}
		}));
}
#endif
