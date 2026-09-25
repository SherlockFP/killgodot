#pragma once

// Dev verbs for digging + the underground (kg.Dig.*), registered from Dev/KGDevCommands.cpp's verb table through one
// hook line (KGDigDev::AddVerbs(Add)). Header-only so that file only gains an include. Development builds only.

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Dig/KGDigComponent.h"
#include "Dig/KGDigManager.h"
#include "Dig/KGKeyGate.h"
#include "Dig/KGUndergroundInfo.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "KillGodot.h"

namespace KGDigDev
{
	inline AKGCharacter* Body(const FKGDevContext& C)
	{
		return C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
	}

	inline FVector Where(const FKGDevContext& C)
	{
		if (const AKGCharacter* B = Body(C))
		{
			return B->GetActorLocation();
		}
		APawn* P = C.Requester ? C.Requester->GetPawnOrSpectator() : nullptr;
		return P ? P->GetActorLocation() : FVector::ZeroVector;
	}

	inline bool ParseKind(const FString& S, EKGDigKind& Out)
	{
		const TPair<const TCHAR*, EKGDigKind> Kinds[] = {{TEXT("Mound"), EKGDigKind::Mound}, {TEXT("X"), EKGDigKind::XMark},
		                                                 {TEXT("XMark"), EKGDigKind::XMark}, {TEXT("Glint"), EKGDigKind::Glint},
		                                                 {TEXT("Grave"), EKGDigKind::Grave}, {TEXT("Treasure"), EKGDigKind::Treasure},
		                                                 {TEXT("Chest"), EKGDigKind::Treasure}};
		for (const auto& K : Kinds)
		{
			if (S.Equals(K.Key, ESearchCase::IgnoreCase))
			{
				Out = K.Value;
				return true;
			}
		}
		return false;
	}

	/** "near" (default) = the closest spot within 8 m, a number = spot id, a kind = the closest of that kind. */
	inline int32 PickSpot(const FKGDevContext& C, const AKGDigManager* M, const TArray<FString>& A, int32 ArgIndex, float NearDist = 800.0f)
	{
		if (!M)
		{
			return INDEX_NONE;
		}
		const FString Arg = A.IsValidIndex(ArgIndex) ? A[ArgIndex] : TEXT("near");
		if (Arg.IsNumeric())
		{
			return M->IndexOfSpot(static_cast<uint16>(FCString::Atoi(*Arg)));
		}
		EKGDigKind Kind;
		if (ParseKind(Arg, Kind))
		{
			return M->FindNearestSpot(Where(C), 1.0e7f, Kind);
		}
		return M->FindNearestSpot(Where(C), NearDist);
	}

	inline FString Describe(const FKGDigSpot& S)
	{
		return FString::Printf(TEXT("#%d %s (%s) stage %d/%d at %s"), S.Id, FKGDigRules::KindName(S.Kind), FKGDigRules::ZoneName(S.Zone),
		                       S.Stage, S.MaxStage, *FVector(S.Location).ToCompactString());
	}

	/** Adds the Dig.* verbs through the verb table's Add(Name, Usage, Help, Scope, Run). */
	template <typename FAdd>
	void AddVerbs(FAdd&& Add)
	{
		constexpr EKGDevScope Server = EKGDevScope::Server;
		Add(TEXT("Dig.Give"), TEXT("[Shovel|Map|Scrap|Key|Token] [Count=1]"),
		    TEXT("A shovel (taken out at once), a treasure map, map scraps, a crypt key or a mourner's token."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = C.Requester ? C.Requester->GetPlayerState<AKGPlayerState>() : nullptr;
			    if (!PS)
			    {
				    return FKGDevResult::Fail(TEXT("No player"));
			    }
			    UKGPlayerExtrasSubsystem::EnsurePlayerComponents(PS);
			    UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
			    if (!Pockets)
			    {
				    return FKGDevResult::Fail(TEXT("No pockets yet"));
			    }
			    const FString What = A.IsValidIndex(0) ? A[0] : TEXT("Shovel");
			    const int32 Count = A.IsValidIndex(1) ? FMath::Max(1, FCString::Atoi(*A[1])) : 1;
			    FName Item = What.Equals(TEXT("Map"), ESearchCase::IgnoreCase) ? FKGItemIds::TreasureMap
			               : What.Equals(TEXT("Scrap"), ESearchCase::IgnoreCase) ? FKGItemIds::MapScrap
			               : What.Equals(TEXT("Key"), ESearchCase::IgnoreCase) ? FKGItemIds::CryptKey
			               : What.Equals(TEXT("Token"), ESearchCase::IgnoreCase) ? FKGItemIds::MournerToken
			               : What.Equals(TEXT("Shovel"), ESearchCase::IgnoreCase) ? FKGItemIds::Shovel
			                                                                     : UKGItemCatalog::ResolveLoose(What);
			    if (Item.IsNone())
			    {
				    return FKGDevResult::Fail(FString::Printf(TEXT("Unknown item '%s'"), *What));
			    }
			    const int32 Added = Pockets->AddItem(Item, Count);
			    if (Item == FKGItemIds::Shovel)
			    {
				    if (UKGDigComponent* Dig = UKGDigComponent::FindFor(Body(C)))
				    {
					    Dig->ServerTrySetShovel(true);
				    }
			    }
			    if (UKGDigComponent* Dig = UKGDigComponent::FindFor(Body(C)))
			    {
				    Dig->ServerRefreshTreasure();
			    }
			    return FKGDevResult{Added > 0, FString::Printf(TEXT("+%d %s%s"), Added, *UKGItemCatalog::GetDisplayName(Item).ToString(),
			                                                   Item == FKGItemIds::Shovel ? TEXT(" (Q puts it away, hold LMB to dig)") : TEXT(""))};
		    });
		Add(TEXT("Dig.Spots"), TEXT("[list | near | <Id> | Mound|X|Glint|Grave|Chest]"),
		    TEXT("Log every dig spot (list), describe the closest (near), or teleport next to a spot by id / the closest of a kind."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGDigManager* M = AKGDigManager::Get(C.World);
			    if (!M)
			    {
				    return FKGDevResult::Fail(TEXT("No dig manager in this world"));
			    }
			    const FString Mode = A.IsValidIndex(0) ? A[0] : TEXT("list");
			    if (Mode.Equals(TEXT("list"), ESearchCase::IgnoreCase))
			    {
				    for (const FKGDigSpot& S : M->GetSpots())
				    {
					    UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SPOT %s"), *Describe(S));
				    }
				    for (const FKGDigSpot& S : M->GetBuried())
				    {
					    UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_SPOT (buried) %s"), *Describe(S));
				    }
				    return FKGDevResult::Ok(FString::Printf(TEXT("%d spots + %d buried chests (see log), seed %llu"), M->GetSpots().Num(),
				                                            M->GetBuried().Num(), M->GetGeneratedSeed()));
			    }
			    const int32 i = PickSpot(C, M, A, 0);
			    if (!M->GetSpots().IsValidIndex(i))
			    {
				    return FKGDevResult::Fail(TEXT("No such spot (Dig.Spots list)"));
			    }
			    const FKGDigSpot& S = M->GetSpots()[i];
			    if (Mode.Equals(TEXT("near"), ESearchCase::IgnoreCase))
			    {
				    return FKGDevResult::Ok(Describe(S));
			    }
			    // Stand 1.8 m off the spot, facing it.
			    const FVector Spot = S.Location;
			    const FVector Stand = Spot + FVector(-180.0, 0.0, 110.0);
			    APawn* P = C.Requester ? C.Requester->GetPawnOrSpectator() : nullptr;
			    if (!P)
			    {
				    return FKGDevResult::Fail(TEXT("No pawn to teleport"));
			    }
			    const FRotator Look = (Spot - (Stand + FVector(0.0, 0.0, 64.0))).Rotation();
			    P->TeleportTo(Stand, FRotator(0.0f, Look.Yaw, 0.0f), false, true);
			    C.Requester->SetControlRotation(Look);
			    C.Requester->ClientSetRotation(Look);
			    return FKGDevResult::Ok(FString::Printf(TEXT("At %s"), *Describe(S)));
		    });
		Add(TEXT("Dig.Finish"), TEXT("[near | <Id> | Kind]"), TEXT("Dig out a spot at once for you (the closest within 8 m by default): loot to your pockets."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGDigManager* M = AKGDigManager::Get(C.World);
			    UKGDigComponent* Dig = UKGDigComponent::FindFor(Body(C));
			    if (!M || !Dig)
			    {
				    return FKGDevResult::Fail(TEXT("No dig manager / villager body"));
			    }
			    const int32 i = PickSpot(C, M, A, 0);
			    if (!M->GetSpots().IsValidIndex(i))
			    {
				    return FKGDevResult::Fail(TEXT("No spot near you (Dig.Spots)"));
			    }
			    if (M->GetSpots()[i].IsDugOut())
			    {
				    return FKGDevResult::Fail(TEXT("Already dug out"));
			    }
			    const int32 Items = Dig->ServerFinishSpot(i);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Dug out %s: %d item(s)"), *Describe(M->GetSpots()[i]), Items));
		    });
		Add(TEXT("Dig.Stage"), TEXT("<near|Id|Kind> <Stage>"), TEXT("Set a spot's stage without loot (looks, captures)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGDigManager* M = AKGDigManager::Get(C.World);
			    const int32 i = PickSpot(C, M, A, 0);
			    if (!M || !M->GetSpots().IsValidIndex(i) || !A.IsValidIndex(1))
			    {
				    return FKGDevResult::Fail(TEXT("Usage: Dig.Stage <near|Id|Kind> <Stage>"));
			    }
			    M->DebugSetStage(i, static_cast<uint8>(FMath::Clamp(FCString::Atoi(*A[1]), 0, 8)));
			    return FKGDevResult::Ok(Describe(M->GetSpots()[i]));
		    });
		Add(TEXT("Dig.Regen"), TEXT("[Seed]"), TEXT("Scatter a fresh set of dig spots (new match seed, or the given one)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGDigManager* M = AKGDigManager::Get(C.World, true);
			    if (!M)
			    {
				    return FKGDevResult::Fail(TEXT("No dig manager"));
			    }
			    const uint64 Seed = A.IsValidIndex(0) ? static_cast<uint64>(FCString::Atoi64(*A[0])) : M->GetGeneratedSeed() * 6364136223846793005ull + 1442695040888963407ull;
			    M->Regenerate(Seed);
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d spots, %d buried chests (seed %llu)"), M->GetSpots().Num(), M->GetBuried().Num(), Seed));
		    });
		Add(TEXT("Dig.Gate"), TEXT("[open]"), TEXT("Unlock the catacomb vault gate (and open it) without a crypt key."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    int32 N = 0;
			    for (TActorIterator<AKGKeyGate> It(C.World); It; ++It, ++N)
			    {
				    It->DevUnlock(A.IsValidIndex(0) && A[0].Equals(TEXT("open"), ESearchCase::IgnoreCase));
			    }
			    return N > 0 ? FKGDevResult::Ok(FString::Printf(TEXT("%d gate(s) unlocked"), N)) : FKGDevResult::Fail(TEXT("No gate in this level"));
		    });
		Add(TEXT("Dig.Where"), TEXT(""), TEXT("Are you below ground, and in which underground region?"), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AKGUndergroundInfo* U = AKGUndergroundInfo::Find(C.World);
			    const FVector P = Where(C);
			    if (!U)
			    {
				    return FKGDevResult::Fail(TEXT("This level has no underground"));
			    }
			    return FKGDevResult::Ok(U->Contains(P) ? FString::Printf(TEXT("Below ground: %s"), *U->RegionNameAt(P)) : FString(TEXT("On the surface")));
		    });
	}
}

#endif
