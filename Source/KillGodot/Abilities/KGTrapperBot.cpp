#include "Abilities/KGTrapperBot.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilitySubsystem.h"
#include "Abilities/KGAbilityTypes.h"
#include "AI/KGBotController.h"
#include "Character/KGCharacter.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KillGodot.h"
#include "Navigation/PathFollowingComponent.h"
#include "Roles/KGRoleListGenerator.h"
#include "Traps/KGMimicTrap.h"
#include "World/KGTaskStation.h"

namespace KGTrapperBotPrivate
{
	struct FGoal
	{
		FName AbilityId;                       // None = rummage
		TWeakObjectPtr<AActor> Container;
		FVector Stand = FVector::ZeroVector;
		FVector AimAt = FVector::ZeroVector;
		float Elapsed = 0.0f;
		bool bActive = false;
	};

	struct FBrain
	{
		FGoal Goal;
		float Think = 0.0f;
		float NextRummage = 0.0f;
		TArray<TWeakObjectPtr<AActor>> Rummaged;
		TSet<TWeakObjectPtr<const AActor>> Avoided;
		TMap<FName, float> RetryAt;            // an ability denied (seen / no target) rests so the others get a turn
	};

	TMap<TWeakObjectPtr<AKGBotController>, FBrain>& Brains()
	{
		static TMap<TWeakObjectPtr<AKGBotController>, FBrain> B;
		return B;
	}

	/** Deterministic 0..1 (no FMath::Rand in gameplay code). */
	float Hash01(uint32 A, uint32 B)
	{
		return static_cast<float>(HashCombine(A * 2654435761u, B) % 10007u) / 10007.0f;
	}

	bool IsImpatient(const AKGCharacter* Me)
	{
		const AKGPlayerState* PS = Me ? Me->GetPlayerState<AKGPlayerState>() : nullptr;
		const FKGRoleInfo* R = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId()) : nullptr;
		return R && R->GetAlignment() == EKGAlignment::Impatient;
	}

	AKGTaskStation* NearestStation(UWorld* World, const FVector& From, double MaxCm)
	{
		AKGTaskStation* Best = nullptr;
		double BestD = FMath::Square(MaxCm);
		for (TActorIterator<AKGTaskStation> It(World); It; ++It)
		{
			const double D = FVector::DistSquared(It->GetActorLocation(), From);
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
		return Best;
	}

	AActor* NearestContainer(UWorld* World, const FVector& From, double MaxCm, TFunctionRef<bool(AActor*)> Accept)
	{
		AActor* Best = nullptr;
		double BestD = FMath::Square(MaxCm);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!AKGMimicTrap::IsContainer(A))
			{
				continue;
			}
			const double D = FVector::DistSquared(A->GetActorLocation(), From);
			if (D < BestD && FMath::Abs(A->GetActorLocation().Z - From.Z) < 300.0 && Accept(A))
			{
				BestD = D;
				Best = A;
			}
		}
		return Best;
	}

	bool PlanTrapper(AKGBotController* Bot, AKGCharacter* Me, AKGAbilityHolder* H, FBrain& Brain, float Now)
	{
		UWorld* World = Me->GetWorld();
		const FVector Here = Me->GetActorLocation();
		for (const FKGAbilityState& S : H->GetStates())
		{
			const float* Retry = Brain.RetryAt.Find(S.AbilityId);
			if (S.Charges <= 0 || S.Cooldown.RemainingSeconds > 0.0f || (Retry && Now < *Retry))
			{
				continue;
			}
			FGoal G;
			G.AbilityId = S.AbilityId;
			if (S.AbilityId == TEXT("Mimic"))
			{
				// A container on a chore route: near a task station, not a mimic already.
				AActor* C = NearestContainer(World, Here, 2500.0, [World](AActor* A)
				{
					const AKGMimicTrap* M = AKGMimicTrap::FindOn(A);
					return (!M || M->GetState() == EKGTrapState::Idle) && NearestStation(World, A->GetActorLocation(), 1500.0) != nullptr;
				});
				if (!C)
				{
					continue;
				}
				const FVector Center = C->GetComponentsBoundingBox(true).GetCenter();
				const FVector Away = (Here - Center).GetSafeNormal2D();
				G.Container = C;
				G.Stand = Center + (Away.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Away) * 170.0;
				G.AimAt = Center;
			}
			else
			{
				// Snare / tripwire: on the approach to the nearest station, 3 m out, facing it.
				AKGTaskStation* St = NearestStation(World, Here, 3000.0);
				if (!St)
				{
					continue;
				}
				const FVector To = St->GetActorLocation();
				FVector Dir = (Here - To).GetSafeNormal2D();
				if (Dir.IsNearlyZero())
				{
					Dir = FVector(1.0, 0.0, 0.0);
				}
				G.Stand = To + Dir * 520.0;
				G.AimAt = To + Dir * 300.0 + FVector(0.0, 0.0, -80.0);
			}
			G.bActive = true;
			Brain.Goal = G;
			Bot->MoveToLocation(G.Stand, 60.0f, false, true, true, false);
			return true;
		}
		return false;
	}

	bool PlanRummage(AKGBotController* Bot, AKGCharacter* Me, FBrain& Brain, float Now)
	{
		if (Now < Brain.NextRummage)
		{
			return false;
		}
		Brain.NextRummage = Now + 35.0f + 40.0f * Hash01(GetTypeHash(Bot->GetName()), static_cast<uint32>(Now));
		const UKGAbilitySubsystem* Sub = UKGAbilitySubsystem::Get(Me->GetWorld());
		AActor* C = NearestContainer(Me->GetWorld(), Me->GetActorLocation(), 900.0, [&](AActor* A)
		{
			if (Brain.Rummaged.Contains(A))
			{
				return false;
			}
			if (Sub && Sub->IsKnownMimic(Bot, A))
			{
				if (!Brain.Avoided.Contains(A))
				{
					Brain.Avoided.Add(A);
					++KGTrapperBot::Stats().Avoided;
					UE_LOG(LogKillGodot, Log, TEXT("KG_BOT %s avoids a known mimic (%s)"), *Me->GetName(), *A->GetName());
				}
				return false;
			}
			return true;
		});
		if (!C)
		{
			return false;
		}
		FGoal G;
		G.Container = C;
		const FVector Center = C->GetComponentsBoundingBox(true).GetCenter();
		const FVector Away = (Me->GetActorLocation() - Center).GetSafeNormal2D();
		G.Stand = Center + (Away.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Away) * 150.0;
		G.AimAt = Center;
		G.bActive = true;
		Brain.Goal = G;
		Bot->MoveToLocation(G.Stand, 50.0f, false, true, true, false);
		return true;
	}
}

KGTrapperBot::FStats& KGTrapperBot::Stats()
{
	static FStats S;
	return S;
}

bool KGTrapperBot::Update(AKGBotController* Bot, AKGCharacter* Me, float DeltaSeconds)
{
	using namespace KGTrapperBotPrivate;
	UWorld* World = Me ? Me->GetWorld() : nullptr;
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (!Bot || !GS || (GS->GetPhase() != EKGPhase::Day && GS->GetPhase() != EKGPhase::Night))
	{
		return false;
	}
	FBrain& Brain = Brains().FindOrAdd(Bot);
	const float Now = World->GetTimeSeconds();
	AKGAbilityHolder* H = AKGAbilityHolder::FindFor(Bot);
	FGoal& G = Brain.Goal;

	if (G.bActive)
	{
		G.Elapsed += DeltaSeconds;
		AActor* C = G.Container.Get();
		if (G.Elapsed > 18.0f || (!G.AbilityId.IsNone() && !H) || (G.AbilityId == TEXT("Mimic") && !C) || (G.AbilityId.IsNone() && !AKGMimicTrap::IsContainer(C)))
		{
			G.bActive = false;   // give up quietly
			Bot->StopMovement();
			return false;
		}
		if (FVector::Dist2D(Me->GetActorLocation(), G.Stand) > 110.0)
		{
			if (Bot->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				Bot->MoveToLocation(G.Stand, 50.0f, false, true, true, false);
			}
			return true;
		}
		Bot->StopMovement();
		const FVector Eyes = Me->GetPawnViewLocation();
		const FVector Dir = (G.AimAt - Eyes).GetSafeNormal();
		Bot->SetControlRotation(Dir.Rotation());
		G.bActive = false;
		if (G.AbilityId.IsNone())
		{
			Brain.Rummaged.Add(C);
			++Stats().Rummages;
			const bool bBit = AKGMimicTrap::TryBite(C, Me);
			Stats().Bitten += bBit ? 1 : 0;
			UE_LOG(LogKillGodot, Log, TEXT("KG_BOT %s rummages %s%s"), *Me->GetName(), *C->GetName(), bBit ? TEXT(" -> BITTEN") : TEXT(""));
			return true;
		}
		const EKGAbilityDeny V = H->AuthUse(G.AbilityId, Eyes, Dir);
		if (V == EKGAbilityDeny::None)
		{
			++Stats().Arms;
		}
		else
		{
			Brain.RetryAt.Add(G.AbilityId, Now + 25.0f);   // seen / no target: the next ability gets a turn first
		}
		Brain.Think = V == EKGAbilityDeny::None ? 3.0f : 4.0f;
		return true;
	}

	Brain.Think -= DeltaSeconds;
	if (Brain.Think > 0.0f)
	{
		return false;
	}
	Brain.Think = 2.0f;
	if (H && H->GetStates().Num() > 0 && GS->GetPhase() == EKGPhase::Day)
	{
		return PlanTrapper(Bot, Me, H, Brain, Now);
	}
	if (!IsImpatient(Me) && GS->GetPhase() == EKGPhase::Day)
	{
		return PlanRummage(Bot, Me, Brain, Now);
	}
	return false;
}
