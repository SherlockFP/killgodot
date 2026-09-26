#pragma once

// Dev verbs for the SPRINT-040 manor secrets (kg.Manor.*), registered from Dev/KGDevCommands.cpp's verb table
// through one hook line (KGManorDev::AddVerbs(Add)), like KGDigDev. Header-only. Development builds only.

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Character/KGCharacter.h"
#include "Dev/KGDevCommands.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Manor/KGHiddenCompartment.h"
#include "Manor/KGManorSubsystem.h"
#include "Manor/KGSecretPassage.h"

namespace KGManorDev
{
	inline AKGCharacter* Body(const FKGDevContext& C)
	{
		return C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
	}

	template <typename FAdd>
	void AddVerbs(FAdd&& Add)
	{
		constexpr EKGDevScope Server = EKGDevScope::Server;
		Add(TEXT("Manor.Secrets"), TEXT(""), TEXT("Log every secret passage end and hidden compartment with its state."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    int32 Secrets = 0, Found = 0, Comps = 0, Open = 0;
			    for (TActorIterator<AKGSecretPassage> It(C.World); It; ++It, ++Secrets)
			    {
				    Found += It->IsDiscovered() ? 1 : 0;
				    UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR_SECRET %s (%s) end=%s -> %s discovered=%d at %s"), *It->SecretId.ToString(),
				           *It->SecretKind.ToString(), *It->PassageId.ToString(), *It->TargetId.ToString(), It->IsDiscovered() ? 1 : 0,
				           *It->GetActorLocation().ToCompactString());
			    }
			    for (TActorIterator<AKGHiddenCompartment> It(C.World); It; ++It, ++Comps)
			    {
				    Open += It->IsOpen() ? 1 : 0;
				    UE_LOG(LogKillGodot, Log, TEXT("KG_MANOR_COMPARTMENT %s (%s) room=%s open=%d loot=%s clue=%s at %s"), *It->CompartmentId.ToString(),
				           *It->Kind.ToString(), *It->Room.ToString(), It->IsOpen() ? 1 : 0, *It->LootTable.ToString(),
				           It->ClueText.IsEmpty() ? TEXT("-") : *It->ClueText, *It->GetActorLocation().ToCompactString());
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d secret end(s), %d discovered; %d compartment(s), %d open (see log)"), Secrets,
			                                            Found, Comps, Open));
		    });
		Add(TEXT("Manor.Discover"), TEXT("<SecretId|all>"), TEXT("Discover a secret (every end) for everyone, as you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGManorSubsystem* M = UKGManorSubsystem::Get(C.World);
			    if (!M || !A.IsValidIndex(0))
			    {
				    return FKGDevResult::Fail(TEXT("Usage: Manor.Discover <SecretId|all>"));
			    }
			    TSet<FName> Ids;
			    if (A[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
			    {
				    for (TActorIterator<AKGSecretPassage> It(C.World); It; ++It)
				    {
					    Ids.Add(It->SecretId);
				    }
			    }
			    else
			    {
				    Ids.Add(FName(*A[0]));
			    }
			    int32 Ends = 0;
			    for (const FName Id : Ids)
			    {
				    Ends += M->DiscoverSecret(Id, Body(C));
			    }
			    return Ends > 0 ? FKGDevResult::Ok(FString::Printf(TEXT("%d secret(s), %d end(s) discovered"), Ids.Num(), Ends))
			                    : FKGDevResult::Fail(TEXT("No such secret (Manor.Secrets)"));
		    });
		Add(TEXT("Manor.Open"), TEXT("<CompartmentId|all>"), TEXT("Open a hidden compartment (loot rolls once) for everyone."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGManorSubsystem* M = UKGManorSubsystem::Get(C.World);
			    if (!M || !A.IsValidIndex(0))
			    {
				    return FKGDevResult::Fail(TEXT("Usage: Manor.Open <CompartmentId|all>"));
			    }
			    int32 N = 0;
			    if (A[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
			    {
				    for (TActorIterator<AKGHiddenCompartment> It(C.World); It; ++It)
				    {
					    N += It->AuthOpen(Body(C)) ? 1 : 0;
				    }
			    }
			    else
			    {
				    N += M->OpenCompartment(FName(*A[0]), Body(C)) ? 1 : 0;
			    }
			    return N > 0 ? FKGDevResult::Ok(FString::Printf(TEXT("%d compartment(s) opened"), N)) : FKGDevResult::Fail(TEXT("Nothing opened (unknown id or already open)"));
		    });
		Add(TEXT("Manor.Gates"), TEXT(""), TEXT("Re-evaluate the wing gates against the player count now (even mid-match)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGManorSubsystem* M = UKGManorSubsystem::Get(C.World);
			    if (!M)
			    {
				    return FKGDevResult::Fail(TEXT("No manor subsystem"));
			    }
			    const int32 Locked = M->RefreshGates(true);
			    return FKGDevResult::Ok(FString::Printf(TEXT("players=%d locked gates=%d"), M->PlayerCount(), Locked));
		    });
	}
}

#endif
