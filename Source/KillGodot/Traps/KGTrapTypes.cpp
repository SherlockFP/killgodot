#include "Traps/KGTrapTypes.h"

void FKGTrapMachine::Configure(const FKGTrapDef& Def)
{
	bPassive = Def.bPassive;
	TelegraphSecs = FMath::Max(0.0f, Def.TelegraphSecs);
	ActiveSecs = FMath::Max(0.0f, Def.ActiveSecs);
	CooldownSecs = FMath::Max(0.0f, Def.CooldownSecs);
	Reset();
}

bool FKGTrapMachine::TryArm()
{
	if (State != EKGTrapState::Idle)
	{
		return false;
	}
	State = EKGTrapState::Armed;
	Clock = FKGMatchClock();
	return true;
}

bool FKGTrapMachine::TryTrigger()
{
	if (State != EKGTrapState::Armed)
	{
		return false;
	}
	State = EKGTrapState::Telegraph;
	Clock.Start(TelegraphSecs);
	return true;
}

void FKGTrapMachine::ForceFire()
{
	State = EKGTrapState::Telegraph;
	Clock.Start(0.0f);
}

void FKGTrapMachine::Reset()
{
	State = bPassive ? EKGTrapState::Armed : EKGTrapState::Idle;
	Clock = FKGMatchClock();
}

void FKGTrapMachine::Enter(EKGTrapState Next, TArray<FKGTrapTransition>& Out)
{
	Out.Add({State, Next});
	State = Next;
	switch (Next)
	{
	case EKGTrapState::Telegraph: Clock.Start(TelegraphSecs); break;
	case EKGTrapState::Active: Clock.Start(ActiveSecs); break;
	case EKGTrapState::Cooldown: Clock.Start(CooldownSecs); break;
	default: Clock = FKGMatchClock(); break;
	}
}

TArray<FKGTrapTransition> FKGTrapMachine::Advance(float DeltaSeconds)
{
	TArray<FKGTrapTransition> Out;
	if (!IsBusy())
	{
		return Out;
	}
	Clock.Advance(FMath::Max(0.0f, DeltaSeconds));
	// Zero-length phases chain within one call; a phase with time left stops the loop.
	for (int32 Guard = 0; Guard < 4 && IsBusy() && Clock.RemainingSeconds <= 0.0f; ++Guard)
	{
		switch (State)
		{
		case EKGTrapState::Telegraph: Enter(EKGTrapState::Active, Out); break;
		case EKGTrapState::Active: Enter(EKGTrapState::Cooldown, Out); break;
		case EKGTrapState::Cooldown: Enter(bPassive ? EKGTrapState::Armed : EKGTrapState::Idle, Out); break;
		default: break;
		}
	}
	return Out;
}

void FKGTrapEventLog::Add(const FKGTrapEvent& E)
{
	if (Events.Num() < Capacity)
	{
		Events.Add(E);
		return;
	}
	Events[Head] = E;
	Head = (Head + 1) % Capacity;
}

const FKGTrapEvent& FKGTrapEventLog::Get(int32 Index) const
{
	return Events[(Head + Index) % Events.Num()];
}

TArray<FKGTrapEvent> FKGTrapEventLog::ToArray() const
{
	TArray<FKGTrapEvent> Out;
	Out.Reserve(Events.Num());
	for (int32 i = 0; i < Events.Num(); ++i)
	{
		Out.Add(Get(i));
	}
	return Out;
}

void FKGTrapArmerCooldowns::Start(const FString& Puid, float Secs)
{
	if (Puid.IsEmpty() || Secs <= 0.0f)
	{
		return;
	}
	Clocks.FindOrAdd(Puid).Start(Secs);
}

void FKGTrapArmerCooldowns::Advance(float DeltaSeconds)
{
	for (auto It = Clocks.CreateIterator(); It; ++It)
	{
		It->Value.Advance(DeltaSeconds);
		if (It->Value.RemainingSeconds <= 0.0f)
		{
			It.RemoveCurrent();
		}
	}
}

float FKGTrapArmerCooldowns::Remaining(const FString& Puid) const
{
	const FKGMatchClock* C = Clocks.Find(Puid);
	return C ? C->RemainingSeconds : 0.0f;
}

namespace KGTrap
{
	const TCHAR* StateName(EKGTrapState S)
	{
		switch (S)
		{
		case EKGTrapState::Idle: return TEXT("Idle");
		case EKGTrapState::Armed: return TEXT("Armed");
		case EKGTrapState::Telegraph: return TEXT("Telegraph");
		case EKGTrapState::Active: return TEXT("Active");
		case EKGTrapState::Cooldown: return TEXT("Cooldown");
		}
		return TEXT("?");
	}

	const TCHAR* EffectName(EKGTrapEffect E)
	{
		switch (E)
		{
		case EKGTrapEffect::Trapdoor: return TEXT("Trapdoor");
		case EKGTrapEffect::FallingObject: return TEXT("FallingObject");
		case EKGTrapEffect::LockDoors: return TEXT("LockDoors");
		case EKGTrapEffect::LightsOut: return TEXT("LightsOut");
		case EKGTrapEffect::Alarm: return TEXT("Alarm");
		case EKGTrapEffect::Witness: return TEXT("Witness");
		case EKGTrapEffect::Custom: return TEXT("Custom");
		}
		return TEXT("?");
	}

	bool ParseEffect(const FString& S, EKGTrapEffect& Out)
	{
		for (uint8 i = 0; i <= static_cast<uint8>(EKGTrapEffect::Custom); ++i)
		{
			if (S.Equals(EffectName(static_cast<EKGTrapEffect>(i)), ESearchCase::IgnoreCase))
			{
				Out = static_cast<EKGTrapEffect>(i);
				return true;
			}
		}
		return false;
	}
}
