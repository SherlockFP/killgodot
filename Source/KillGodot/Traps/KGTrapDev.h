#pragma once

// Dev verbs for the SPRINT-040 traps (kg.Trap.*), registered from Dev/KGDevCommands.cpp's verb table through one hook
// line (KGTrapDev::AddVerbs(Add)), like KGDigDev. Header-only. Development builds only.

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Character/KGCharacter.h"
#include "Dev/KGDevCommands.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Traps/KGTrap.h"
#include "Traps/KGTrapSubsystem.h"

namespace KGTrapDev
{
	inline TArray<AKGTrap*> All(UWorld* World)
	{
		TArray<AKGTrap*> Out;
		for (TActorIterator<AKGTrap> It(World); It; ++It)
		{
			Out.Add(*It);
		}
		Out.Sort([](const AKGTrap& A, const AKGTrap& B) { return A.TrapId.LexicalLess(B.TrapId); });
		return Out;
	}

	/** "<Id>" or "all" (bAllowAll). Empty = every trap when bAllowAll, else none. */
	inline TArray<AKGTrap*> Pick(UWorld* World, const TArray<FString>& A, bool bAllowAll)
	{
		const FString Arg = A.IsValidIndex(0) ? A[0] : (bAllowAll ? TEXT("all") : TEXT(""));
		if (bAllowAll && Arg.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			return All(World);
		}
		TArray<AKGTrap*> Out;
		if (AKGTrap* T = AKGTrap::FindById(World, FName(*Arg)))
		{
			Out.Add(T);
		}
		return Out;
	}

	template <typename FAdd>
	void AddVerbs(FAdd&& Add)
	{
		constexpr EKGDevScope Server = EKGDevScope::Server;
		Add(TEXT("Trap.List"), TEXT(""), TEXT("Log every trap: id, effect, room, state, clock."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const TArray<AKGTrap*> Traps = All(C.World);
			    for (const AKGTrap* T : Traps)
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("KG_TRAP_LIST %s"), *T->Describe());
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d trap(s) (see log)"), Traps.Num()));
		    });
		Add(TEXT("Trap.Arm"), TEXT("<Id|all>"), TEXT("Arm a trap (or all) as the host: ignores the arm policy and cooldowns."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Body = C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
			    int32 N = 0;
			    for (AKGTrap* T : Pick(C.World, A, true))
			    {
				    N += T->AuthArm(Body, true) ? 1 : 0;
			    }
			    return N > 0 ? FKGDevResult::Ok(FString::Printf(TEXT("%d trap(s) armed"), N)) : FKGDevResult::Fail(TEXT("Nothing armed (unknown id or not idle)"));
		    });
		Add(TEXT("Trap.Fire"), TEXT("<Id>"), TEXT("Fire a trap now (a zero telegraph; the effect happens on the next tick)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const TArray<AKGTrap*> Picked = Pick(C.World, A, false);
			    if (Picked.Num() == 0)
			    {
				    return FKGDevResult::Fail(TEXT("Usage: Trap.Fire <Id> (Trap.List)"));
			    }
			    Picked[0]->AuthForceFire();
			    return FKGDevResult::Ok(Picked[0]->Describe());
		    });
		Add(TEXT("Trap.Reset"), TEXT("[Id|all]"), TEXT("Reset a trap (default all): idle / armed when passive, doors unlocked, cooldowns cleared."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    int32 N = 0;
			    for (AKGTrap* T : Pick(C.World, A, true))
			    {
				    T->AuthReset();
				    ++N;
			    }
			    if (UKGTrapSubsystem* S = UKGTrapSubsystem::Get(C.World))
			    {
				    S->ClearArmerCooldowns();
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d trap(s) reset"), N));
		    });
		Add(TEXT("Trap.Log"), TEXT(""), TEXT("Print the trap event ring (the last 64 events)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const UKGTrapSubsystem* S = UKGTrapSubsystem::Get(C.World);
			    if (!S)
			    {
				    return FKGDevResult::Fail(TEXT("No trap subsystem"));
			    }
			    const TArray<FKGTrapEvent> Events = S->GetLog().ToArray();
			    for (const FKGTrapEvent& E : Events)
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("KG_TRAP_LOG t=%.1f %s %s kind=%s room=%s by=%s at=%s"), E.WorldSeconds, *E.Type.ToString(),
				           *E.TrapId.ToString(), *E.Kind.ToString(), *E.Room.ToString(), E.ByName.IsEmpty() ? TEXT("-") : *E.ByName,
				           *E.Location.ToCompactString());
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d event(s) (see log)"), Events.Num()));
		    });
	}
}

#endif
