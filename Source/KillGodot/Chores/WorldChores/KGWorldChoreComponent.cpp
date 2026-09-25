#include "Chores/WorldChores/KGWorldChoreComponent.h"

#include "AIController.h"
#include "Character/KGCharacter.h"
#include "Chores/KGChoreComponent.h"
#include "Chores/KGChoreFx.h"
#include "Chores/KGChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "Components/BoxComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleListGenerator.h"
#include "World/KGChoreItem.h"

namespace KGWorldChoreComp
{
	TAutoConsoleVariable<int32> CVarAnyPhase(
		TEXT("kg.WorldChore.AnyPhase"), 0,
		TEXT("Dev/smoke: world chores work in any phase (lobby, warm-up) instead of only when chores are allowed."));

	EKGChoreFx FxFromName(FName Name)
	{
		if (Name == TEXT("BellRing")) { return EKGChoreFx::BellRing; }
		if (Name == TEXT("ClockChime")) { return EKGChoreFx::ClockChime; }
		if (Name == TEXT("LighthouseGlow")) { return EKGChoreFx::LighthouseGlow; }
		if (Name == TEXT("ChimneySmoke")) { return EKGChoreFx::ChimneySmoke; }
		if (Name == TEXT("LampLit")) { return EKGChoreFx::LampLit; }
		if (Name == TEXT("WoodPile")) { return EKGChoreFx::WoodPile; }
		return EKGChoreFx::None;
	}

	constexpr float MaxDz = 260.0f;
}

UKGWorldChoreComponent::UKGWorldChoreComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UKGWorldChoreComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UKGWorldChoreComponent, Progress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UKGWorldChoreComponent, Dwell, COND_OwnerOnly);
}

UKGWorldChoreComponent* UKGWorldChoreComponent::FindFor(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UKGWorldChoreComponent>() : nullptr;
}

AKGCharacter* UKGWorldChoreComponent::GetCharacter() const
{
	return Cast<AKGCharacter>(GetOwner());
}

AKGPlayerState* UKGWorldChoreComponent::GetPlayerState() const
{
	const AKGCharacter* Char = GetCharacter();
	return Char ? Char->GetPlayerState<AKGPlayerState>() : nullptr;
}

bool UKGWorldChoreComponent::IsImpatient() const
{
	const AKGPlayerState* PS = GetPlayerState();
	const FKGRoleInfo* Role = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId())
	                             : nullptr;
	return Role && Role->GetAlignment() == EKGAlignment::Impatient;
}

bool UKGWorldChoreComponent::PhaseAllows() const
{
	if (KGWorldChoreComp::CVarAnyPhase.GetValueOnGameThread() != 0)
	{
		return true;
	}
	const AKGGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKGGameState>() : nullptr;
	return GS && FKGChoreRules::PhaseAllowsChores(GS->GetPhase());
}

float UKGWorldChoreComponent::ServerNow() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

const FKGWorldProgress* UKGWorldChoreComponent::FindProgress(FName Chore) const
{
	return Progress.FindByPredicate([Chore](const FKGWorldProgress& P) { return P.Chore == Chore; });
}

FKGWorldProgress* UKGWorldChoreComponent::MutableProgress(FName Chore)
{
	return Progress.FindByPredicate([Chore](const FKGWorldProgress& P) { return P.Chore == Chore; });
}

FKGWorldProgress& UKGWorldChoreComponent::EnsureProgress(FName Chore)
{
	if (FKGWorldProgress* Found = MutableProgress(Chore))
	{
		return *Found;
	}
	FKGWorldProgress& P = Progress.AddDefaulted_GetRef();
	P.Chore = Chore;
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(Chore);
	const AKGPlayerState* PS = GetPlayerState();
	P.Variant = Def ? static_cast<uint8>(FKGWorldChoreRules::PickVariant(*Def, GetTypeHash(PS ? PS->GetPlayerName() : GetOwner()->GetName()))) : 0;
	return P;
}

int32 UKGWorldChoreComponent::TargetSlot(const FKGWorldChoreDef& Def, const FKGWorldProgress& P, int32 AnchorIndex) const
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const TArray<FName> Targets = Def.Targets(P.Step, P.Variant);
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		if (Cat.AnchorIndex(Targets[i]) == AnchorIndex && (P.DoneMask & (1 << i)) == 0)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UKGWorldChoreComponent::Say(const FString& Text)
{
	if (NoticeCooldown > 0.0f)
	{
		return;
	}
	NoticeCooldown = 2.5f;
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_NOTICE %s: %s"), *GetNameSafe(GetOwner()), *Text);
	ClientNotice(Text);
}

// ---- tick --------------------------------------------------------------------------------------------------------------

void UKGWorldChoreComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (AutoPath.Num() > 0)
	{
		TickAutopilot(DeltaTime);
	}
}

void UKGWorldChoreComponent::DebugTick(float Seconds, float Step)
{
	for (float T = 0.0f; T < Seconds - KINDA_SMALL_NUMBER; T += Step)
	{
		TickServer(Step);
	}
}

void UKGWorldChoreComponent::TickServer(float DeltaTime)
{
	NoticeCooldown = FMath::Max(0.0f, NoticeCooldown - DeltaTime);
	AKGCharacter* Char = GetCharacter();
	if (!Char || !GetPlayerState())
	{
		return;
	}
	SyncList();
	if (Char->IsDead())
	{
		for (FKGWorldProgress& P : Progress)
		{
			if (IsValid(P.Item) && P.Item->IsAttached() && P.Item->IsCarriedBy(Char))
			{
				P.Item->AuthDetach(FVector::ZeroVector);   // a dead bot drops what it carried
			}
		}
		Dwell = FKGWorldDwell();
		return;
	}
	for (FKGWorldProgress& P : Progress)
	{
		ValidateItems(P);
	}
	if (!PhaseAllows())
	{
		if (Dwell.Kind != EKGDwell::None)
		{
			Dwell = FKGWorldDwell();
		}
		return;
	}
	if (Dwell.Kind == EKGDwell::Work || Dwell.Kind == EKGDwell::Sabotage || Dwell.Kind == EKGDwell::Dump)
	{
		TickWorkDwell(DeltaTime);
	}
	else
	{
		TickBring(DeltaTime);
	}
}

void UKGWorldChoreComponent::SyncList()
{
	AKGPlayerState* PS = GetPlayerState();
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	for (const FName Id : PS->TaskIds)
	{
		if (Cat.IsWorldChore(Id) && PS->HasOpenTask(Id) && !FindProgress(Id))
		{
			EnsureProgress(Id);
		}
	}
	for (int32 i = Progress.Num() - 1; i >= 0; --i)
	{
		if (!PS->HasOpenTask(Progress[i].Chore))
		{
			if (IsValid(Progress[i].Item))
			{
				Progress[i].Item->AuthOrphan();
				if (Progress[i].Item->IsAttached())
				{
					Progress[i].Item->AuthDetach(FVector::ZeroVector);
				}
			}
			if (Dwell.Chore == Progress[i].Chore)
			{
				Dwell = FKGWorldDwell();
			}
			Progress.RemoveAt(i);
		}
	}
}

void UKGWorldChoreComponent::Revert(FKGWorldProgress& P, int32 ToStep, const FString& Why)
{
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_REVERT %s by %s step %d -> %d (%s)"), *P.Chore.ToString(), *GetNameSafe(GetOwner()),
	       P.Step + 1, ToStep + 1, *Why);
	P.Step = static_cast<uint8>(ToStep);
	P.Reps = 0;
	if (Dwell.Chore == P.Chore)
	{
		Dwell = FKGWorldDwell();
	}
	Say(Why);
}

void UKGWorldChoreComponent::ValidateItems(FKGWorldProgress& P)
{
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(P.Chore);
	if (!Def || !Def->Steps.IsValidIndex(P.Step))
	{
		return;
	}
	const FKGWorldStepDef& S = Def->Steps[P.Step];
	if (S.Verb != EKGWorldVerb::Bring)
	{
		return;
	}
	if (!IsValid(P.Item) || P.Item->IsActorBeingDestroyed() || P.Item->GetKind() != S.Item)
	{
		// Lost it (stolen and thrown in the sea, despawned...): back to the step that hands one out.
		for (int32 i = P.Step; i >= 0; --i)
		{
			if (Def->Steps[i].Spawn == S.Item)
			{
				P.Item = nullptr;
				Revert(P, i, FString::Printf(TEXT("Your %s is gone - get another one"), *FKGWorldChoreCatalog::Get().FindItem(S.Item)->Label));
				return;
			}
		}
		return;
	}
	if (S.MinFill > 0.0f && P.Item->GetFill() < S.MinFill)
	{
		for (int32 i = P.Step - 1; i >= 0; --i)
		{
			if (Def->Steps[i].Fill >= 0.0f)
			{
				Revert(P, i, TEXT("The bucket is empty - fill it again at the well (walk, don't run!)"));
				return;
			}
		}
	}
}

bool UKGWorldChoreComponent::InReach(int32 AnchorIndex, float SlackCm) const
{
	const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const AKGCharacter* Char = GetCharacter();
	if (!Sub || !Char || !Cat.Anchors.IsValidIndex(AnchorIndex))
	{
		return false;
	}
	const FVector Spot = Sub->SpotLocation(AnchorIndex);
	const FVector Me = Char->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f);   // feet-ish
	return FVector::DistSquared2D(Me, Spot) <= FMath::Square(Cat.Anchors[AnchorIndex].RadiusCm + SlackCm) &&
	       FMath::Abs(Me.Z - Spot.Z) <= KGWorldChoreComp::MaxDz;
}

void UKGWorldChoreComponent::StartDwell(EKGDwell Kind, FName Chore, int32 Anchor, float Needed)
{
	Dwell.Kind = Kind;
	Dwell.Chore = Chore;
	Dwell.Anchor = static_cast<int16>(Anchor);
	Dwell.Seconds = 0.0f;
	Dwell.Needed = FMath::Max(0.2f, Needed);
}

void UKGWorldChoreComponent::TickWorkDwell(float DeltaTime)
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	if (!InReach(Dwell.Anchor, FKGWorldChoreRules::ReachSlackCm))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DWELL_BROKEN %s by %s at %s (walked away)"), *Dwell.Chore.ToString(),
		       *GetNameSafe(GetOwner()), Cat.Anchors.IsValidIndex(Dwell.Anchor) ? *Cat.Anchors[Dwell.Anchor].Id.ToString() : TEXT("?"));
		Dwell = FKGWorldDwell();
		return;
	}
	const float Before = Dwell.Seconds;
	Dwell.Seconds += DeltaTime;
	AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld());
	const FKGWorldAnchor& A = Cat.Anchors[Dwell.Anchor];
	UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	if (Dir && Sub && Dwell.Kind == EKGDwell::Work && FMath::FloorToInt(Before) != FMath::FloorToInt(Dwell.Seconds))
	{
		// A working rhythm everyone nearby hears (crank / chop / wind) once a second.
		if (const FKGWorldChoreDef* Def = Cat.FindChore(Dwell.Chore))
		{
			if (const FKGWorldProgress* P = FindProgress(Dwell.Chore); P && Def->Steps.IsValidIndex(P->Step))
			{
				Dir->AuthCue(Def->Steps[P->Step].Cue, Dwell.Anchor, Sub->SpotLocation(Dwell.Anchor), 0.8f);
			}
		}
	}
	if (Dwell.Seconds < Dwell.Needed)
	{
		return;
	}
	const FKGWorldDwell Done = Dwell;
	Dwell = FKGWorldDwell();
	FKGSpotState* Spot = Dir ? Dir->AuthMutableSpot(Done.Anchor) : nullptr;
	if (Done.Kind == EKGDwell::Sabotage)
	{
		if (Spot)
		{
			Spot->bSpoiled = true;
			if (A.Sabotage == TEXT("snuff"))
			{
				Spot->bLit = false;
			}
			Dir->AuthDirty();
			Dir->AuthCue(A.Sabotage == TEXT("snuff") ? EKGWorldCue::Snuff : EKGWorldCue::Poison, Done.Anchor, Sub->SpotLocation(Done.Anchor), 0.7f);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SABOTAGE %s at %s by %s"), *A.Sabotage.ToString(), *A.Id.ToString(), *GetNameSafe(GetOwner()));
		return;
	}
	if (Done.Kind == EKGDwell::Dump)
	{
		if (Spot)
		{
			Spot->bSpoiled = false;
			Spot->Level = 0.0f;
			Dir->AuthDirty();
			Dir->AuthCue(EKGWorldCue::Splash, Done.Anchor, Sub->SpotLocation(Done.Anchor), 0.9f);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_CLEANED %s by %s"), *A.Id.ToString(), *GetNameSafe(GetOwner()));
		Say(FString::Printf(TEXT("You emptied the poisoned water out of %s"), *A.Label));
		return;
	}
	if (FKGWorldProgress* P = MutableProgress(Done.Chore))
	{
		CompleteStep(*P, Done.Anchor);
	}
}

void UKGWorldChoreComponent::TickBring(float DeltaTime)
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const AKGCharacter* Char = GetCharacter();
	const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	const AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld());
	if (!Sub)
	{
		return;
	}
	// The most recently touched chore first (the one you are carrying for).
	TArray<FKGWorldProgress*> Order;
	for (FKGWorldProgress& P : Progress)
	{
		Order.Add(&P);
	}
	Order.Sort([](const FKGWorldProgress& A, const FKGWorldProgress& B) { return A.Touch > B.Touch; });
	for (FKGWorldProgress* P : Order)
	{
		const FKGWorldChoreDef* Def = Cat.FindChore(P->Chore);
		if (!Def || !Def->Steps.IsValidIndex(P->Step) || Def->Steps[P->Step].Verb != EKGWorldVerb::Bring || !IsValid(P->Item))
		{
			continue;
		}
		const FKGWorldStepDef& S = Def->Steps[P->Step];
		const AKGChoreItem* Item = P->Item;
		// Yours in your hands, or delivered by a helper / gravity while you stand close.
		const bool bMine = Item->IsCarriedBy(Char) ||
		                   FVector::DistSquared(Item->GetActorLocation(), Char->GetActorLocation()) <= FMath::Square(FKGWorldChoreRules::OwnerNearCm);
		if (!bMine)
		{
			continue;
		}
		const TArray<FName> Targets = Def->Targets(P->Step, P->Variant);
		for (int32 i = 0; i < Targets.Num(); ++i)
		{
			if ((P->DoneMask & (1 << i)) != 0)
			{
				continue;
			}
			const int32 AnchorIndex = Cat.AnchorIndex(Targets[i]);
			const FKGWorldAnchor& A = Cat.Anchors[AnchorIndex];
			const FVector Spot = Sub->SpotLocation(AnchorIndex);
			const FVector ItemAt = Item->GetActorLocation();
			if (FVector::DistSquared2D(ItemAt, Spot) > FMath::Square(A.RadiusCm) || FMath::Abs(ItemAt.Z - Spot.Z) > KGWorldChoreComp::MaxDz)
			{
				continue;
			}
			const FKGSpotState* State = Dir ? Dir->GetSpot(AnchorIndex) : nullptr;
			const bool bSpoiled = State && State->bSpoiled;
			if (bSpoiled && (S.Effects & KGWorldEffect::Water))
			{
				Say(FString::Printf(TEXT("%s has been poisoned! Press E on it to dump it first"), *A.Label));
				if (Dwell.Kind == EKGDwell::Bring)
				{
					Dwell = FKGWorldDwell();
				}
				return;
			}
			if (Dwell.Kind != EKGDwell::Bring || Dwell.Chore != P->Chore || Dwell.Anchor != AnchorIndex)
			{
				StartDwell(EKGDwell::Bring, P->Chore, AnchorIndex, FKGWorldChoreRules::DwellSecs(S, bSpoiled));
				P->Touch = NextTouch++;
				if (bSpoiled)
				{
					Say(FString::Printf(TEXT("Soot on %s - cleaning it takes a moment longer"), *A.Label));
				}
			}
			const float Before = Dwell.Seconds;
			Dwell.Seconds += DeltaTime;
			if (Before == 0.0f || FMath::FloorToInt(Before * 0.8f) != FMath::FloorToInt(Dwell.Seconds * 0.8f))
			{
				if (AKGWorldChoreDirector* D = AKGWorldChoreDirector::Get(GetWorld()))
				{
					D->AuthCue(S.Cue, AnchorIndex, Spot, 0.8f);
				}
			}
			if (Dwell.Seconds >= Dwell.Needed)
			{
				Dwell = FKGWorldDwell();
				CompleteStep(*P, AnchorIndex);
			}
			return;
		}
	}
	if (Dwell.Kind == EKGDwell::Bring)
	{
		Dwell = FKGWorldDwell();   // the item left the spot
	}
}

// ---- actions -----------------------------------------------------------------------------------------------------------

bool UKGWorldChoreComponent::AuthInteract(int32 AnchorIndex)
{
	AKGCharacter* Char = GetCharacter();
	AKGPlayerState* PS = GetPlayerState();
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	if (!Char || !PS || Char->IsDead() || !Cat.Anchors.IsValidIndex(AnchorIndex) || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (!PhaseAllows())
	{
		Say(TEXT("Chores wait until the meeting is over"));
		return false;
	}
	if (!InReach(AnchorIndex, FKGWorldChoreRules::ReachSlackCm))
	{
		return false;
	}
	SyncList();
	const FKGWorldAnchor& A = Cat.Anchors[AnchorIndex];
	if (Dwell.Kind != EKGDwell::None && Dwell.Kind != EKGDwell::Bring && Dwell.Anchor == AnchorIndex)
	{
		return true;   // already at it
	}
	// 1. One of your chores wants E here right now.
	TArray<FKGWorldProgress*> Order;
	for (FKGWorldProgress& P : Progress)
	{
		Order.Add(&P);
	}
	Order.Sort([](const FKGWorldProgress& X, const FKGWorldProgress& Y) { return X.Touch > Y.Touch; });
	for (FKGWorldProgress* P : Order)
	{
		const FKGWorldChoreDef* Def = Cat.FindChore(P->Chore);
		if (!Def || !Def->Steps.IsValidIndex(P->Step) || TargetSlot(*Def, *P, AnchorIndex) == INDEX_NONE)
		{
			continue;
		}
		const FKGWorldStepDef& S = Def->Steps[P->Step];
		if (S.Verb == EKGWorldVerb::Bring)
		{
			continue;
		}
		P->Touch = NextTouch++;
		if (P->StartedAt < 0.0f)
		{
			P->StartedAt = ServerNow();
		}
		if (S.Verb == EKGWorldVerb::Panel)
		{
			UKGChoreComponent* Panels = Char->GetChores();
			if (Char->IsPlayerControlled() && Panels && Panels->AuthOpenWorldStep(S.Panel, S.PanelStage, P->Chore, A.Location))
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_PANEL %s by %s: %s from stage %d"), *P->Chore.ToString(), *PS->GetPlayerName(),
				       *S.Panel.ToString(), S.PanelStage);
				return true;
			}
			StartDwell(EKGDwell::Work, P->Chore, AnchorIndex, S.BotSecs);   // bots: stand and work it out
			return true;
		}
		StartDwell(EKGDwell::Work, P->Chore, AnchorIndex, S.Secs);
		if (AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld()))
		{
			Dir->AuthCue(S.Cue, AnchorIndex, UKGWorldChoreSubsystem::Get(GetWorld())->SpotLocation(AnchorIndex), 0.8f);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_WORK %s by %s at %s (%s %.1fs)"), *P->Chore.ToString(), *PS->GetPlayerName(),
		       *A.Id.ToString(), *KGWorldChores::VerbName(S.Verb), S.Secs);
		return true;
	}
	const AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld());
	const FKGSpotState* State = Dir ? Dir->GetSpot(AnchorIndex) : nullptr;
	// 2. A poisoned trough in the way of your water run: dump it.
	if (State && State->bSpoiled && A.Sabotage == TEXT("poison"))
	{
		for (FKGWorldProgress& P : Progress)
		{
			const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
			if (Def && TargetSlot(*Def, P, AnchorIndex) != INDEX_NONE)
			{
				StartDwell(EKGDwell::Dump, P.Chore, AnchorIndex, FKGWorldChoreRules::DumpSecs);
				return true;
			}
		}
	}
	// 3. The Impatient spoil what someone did (poison a filled trough, snuff a lit lamp).
	if (IsImpatient() && State && !State->bSpoiled && !A.Sabotage.IsNone())
	{
		const bool bDone = A.Sabotage == TEXT("poison") ? State->Level > 0.15f : State->bLit;
		if (bDone)
		{
			StartDwell(EKGDwell::Sabotage, NAME_None, AnchorIndex, FKGWorldChoreRules::SabotageSecs);
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SABOTAGE_START %s at %s by %s"), *A.Sabotage.ToString(), *A.Id.ToString(), *PS->GetPlayerName());
			return true;
		}
	}
	return false;
}

void UKGWorldChoreComponent::AuthPanelStepDone(FName Chore)
{
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(Chore);
	FKGWorldProgress* P = MutableProgress(Chore);
	if (!Def || !P || !Def->Steps.IsValidIndex(P->Step) || Def->Steps[P->Step].Verb != EKGWorldVerb::Panel)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_REJECT panel %s by %s: not at a panel step"), *Chore.ToString(), *GetNameSafe(GetOwner()));
		return;
	}
	const TArray<FName> Targets = Def->Targets(P->Step, P->Variant);
	CompleteStep(*P, FKGWorldChoreCatalog::Get().AnchorIndex(Targets[0]));
}

void UKGWorldChoreComponent::CompleteStep(FKGWorldProgress& P, int32 AnchorIndex)
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
	if (!Def || !Def->Steps.IsValidIndex(P.Step))
	{
		return;
	}
	const FKGWorldStepDef& S = Def->Steps[P.Step];
	const int32 Slot = TargetSlot(*Def, P, AnchorIndex);
	if (Slot == INDEX_NONE)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World);
	AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World);
	const FKGWorldAnchor& A = Cat.Anchors[AnchorIndex];
	const FVector SpotAt = Sub ? Sub->SpotLocation(AnchorIndex) : A.Location;
	const AKGPlayerState* PS = GetPlayerState();
	P.Touch = NextTouch++;
	if (P.StartedAt < 0.0f)
	{
		P.StartedAt = ServerNow();
	}

	// World effects: the same for real and faked chores (the Impatient's fake looks identical).
	if (FKGSpotState* Spot = Dir ? Dir->AuthMutableSpot(AnchorIndex) : nullptr)
	{
		if ((S.Effects & KGWorldEffect::Water) && IsValid(P.Item))
		{
			Spot->Level = FMath::Min(1.0f, Spot->Level + FMath::Max(0.25f, P.Item->GetFill()));
		}
		if (S.Effects & (KGWorldEffect::Stack | KGWorldEffect::Chop))
		{
			Spot->Count = static_cast<uint8>(FMath::Min(250, Spot->Count + 1));
		}
		if (S.Effects & KGWorldEffect::Light)
		{
			Spot->bLit = true;
			Spot->bSpoiled = false;
		}
		Dir->AuthDirty();
	}
	if ((S.Effects & KGWorldEffect::Fx) && World)
	{
		if (AKGChoreFx* Fx = AKGChoreFx::Get(World, true))
		{
			Fx->AuthTrigger(P.Chore, KGWorldChoreComp::FxFromName(S.FxName), SpotAt, FRotator(0.0f, A.Yaw, 0.0f));
		}
	}
	// The item.
	if (IsValid(P.Item))
	{
		if (S.Fill >= 0.0f)
		{
			P.Item->AuthSetFill(S.Fill);
		}
		if (S.Effects & KGWorldEffect::Water)
		{
			P.Item->AuthSetFill(0.0f);
		}
	}
	const TArray<FName> Targets = Def->Targets(P.Step, P.Variant);
	bool bAdvance = true;
	if (S.Repeat > 1 && ++P.Reps < S.Repeat)
	{
		bAdvance = false;
	}
	if (Targets.Num() > 1)
	{
		P.DoneMask |= static_cast<uint16>(1 << Slot);
		const int32 DoneCount = FMath::CountBits(P.DoneMask);
		bAdvance = DoneCount >= Targets.Num();
		if (IsValid(P.Item) && S.bConsume)
		{
			P.Item->AuthSetPieces(Targets.Num() - DoneCount);
		}
	}
	if (bAdvance && S.bConsume && IsValid(P.Item))
	{
		P.Item->AuthConsume();
		P.Item = nullptr;
	}
	if (Dir)
	{
		Dir->AuthCue(EKGWorldCue::StepDone, AnchorIndex, SpotAt, 1.0f);
	}
	const float T = P.StartedAt >= 0.0f ? ServerNow() - P.StartedAt : 0.0f;
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_STEP %s by %s step=%d/%d target=%s slot=%d reps=%d t=%.1fs fake=%d"), *P.Chore.ToString(),
	       PS ? *PS->GetPlayerName() : TEXT("?"), P.Step + 1, Def->NumSteps(), *A.Id.ToString(), Slot, P.Reps, T, IsImpatient() ? 1 : 0);
	if (!bAdvance)
	{
		ClientStepDone(P.Chore, P.Step, static_cast<uint8>(Def->NumSteps()), false,
		               Targets.Num() > 1 ? FString::Printf(TEXT("%s  %d/%d"), *A.Label, FMath::CountBits(P.DoneMask), Targets.Num())
		                                 : FString::Printf(TEXT("%d/%d"), P.Reps, S.Repeat));
		return;
	}
	// Something pops out (a bucket, a crate, the firewood bundle, the flour): next to the spot, ready to carry.
	if (!S.Spawn.IsNone() && World)
	{
		if (IsValid(P.Item))
		{
			P.Item->AuthConsume();   // a fresh one replaces the old
		}
		// Between the spot and whoever did the work, so it lands in reach and not inside the prop.
		const AKGCharacter* Worker = GetCharacter();
		const FVector From = Worker ? Worker->GetActorLocation() : (Sub ? Sub->StandLocation(AnchorIndex) : A.Stand);
		FVector Toward = (From - SpotAt).GetSafeNormal2D();
		if (Toward.IsNearlyZero())
		{
			Toward = FRotator(0.0f, A.Yaw, 0.0f).Vector();
		}
		const float Along = FMath::Min(FMath::Max(60.0f, static_cast<float>(FVector::Dist2D(From, SpotAt)) * 0.55f), A.RadiusCm * 0.8f);
		const FVector Where = SpotAt + Toward * Along + FVector(0.0f, 0.0f, 5.0f);
		P.Item = AKGChoreItem::AuthSpawn(World, S.Spawn, Where, A.Yaw, GetPlayerState(), P.Chore);
		if (IsValid(P.Item) && Def->Steps.IsValidIndex(P.Step + 1) && Def->Steps[P.Step + 1].Verb == EKGWorldVerb::Bring)
		{
			const int32 NextTargets = Def->Targets(P.Step + 1, P.Variant).Num();
			if (NextTargets > 1)
			{
				P.Item->AuthSetPieces(NextTargets - FMath::CountBits(P.DoneMask));
			}
		}
		const AKGCharacter* Char = GetCharacter();
		if (IsValid(P.Item) && Char && !Char->IsPlayerControlled())
		{
			P.Item->AuthAttachTo(GetCharacter());   // bots pick it straight up
		}
	}
	if (Targets.Num() <= 1 || bAdvance)
	{
		if (Targets.Num() > 1)
		{
			P.DoneMask = 0;
		}
	}
	P.Reps = 0;
	++P.Step;
	const bool bDone = P.Step >= Def->NumSteps();
	ClientStepDone(P.Chore, static_cast<uint8>(P.Step - 1), static_cast<uint8>(Def->NumSteps()), bDone,
	               bDone ? Def->Title : Def->StepLabel(P.Step, P.Variant));
	if (bDone)
	{
		CompleteChore(P.Chore);
	}
}

void UKGWorldChoreComponent::CompleteChore(FName Chore)
{
	AKGPlayerState* PS = GetPlayerState();
	const FKGWorldProgress* P = FindProgress(Chore);
	const float Secs = P && P->StartedAt >= 0.0f ? ServerNow() - P->StartedAt : 0.0f;
	bool bCounted = false;
	const bool bFake = IsImpatient();
	if (PS && PS->CompleteTask(Chore))
	{
		if (AKGGameMode* GM = GetWorld()->GetAuthGameMode<AKGGameMode>())
		{
			GM->OnTaskCompleted(PS, Chore);   // counts nothing for the Impatient (faked)
		}
		bCounted = !bFake;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DONE %s by %s fake=%d counted=%d secs=%.1f"), *Chore.ToString(),
	       PS ? *PS->GetPlayerName() : TEXT("?"), bFake ? 1 : 0, bCounted ? 1 : 0, Secs);
	const int32 Index = Progress.IndexOfByPredicate([Chore](const FKGWorldProgress& X) { return X.Chore == Chore; });
	if (Index != INDEX_NONE)
	{
		if (AKGChoreItem* Item = Progress[Index].Item; IsValid(Item))
		{
			Item->AuthOrphan();
			if (Item->IsAttached())
			{
				Item->AuthDetach(FVector::ZeroVector);   // a bot puts the empty bucket down
			}
		}
		Progress.RemoveAt(Index);
	}
}

bool UKGWorldChoreComponent::AuthGive(FName Chore, int32 Variant)
{
	AKGPlayerState* PS = GetPlayerState();
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(Chore);
	if (!PS || !Def)
	{
		return false;
	}
	const int32 i = PS->TaskIds.IndexOfByKey(Chore);
	if (i == INDEX_NONE)
	{
		PS->TaskIds.Add(Chore);
		PS->TaskDone.Add(false);
	}
	else if (PS->TaskDone.IsValidIndex(i))
	{
		PS->TaskDone[i] = false;
	}
	PS->ForceNetUpdate();
	Progress.RemoveAll([Chore](const FKGWorldProgress& X) { return X.Chore == Chore; });
	FKGWorldProgress& P = EnsureProgress(Chore);
	if (Variant >= 0)
	{
		P.Variant = static_cast<uint8>(FMath::Clamp(Variant, 0, Def->NumVariants() - 1));
	}
	P.Touch = NextTouch++;
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_GIVE %s to %s variant=%d"), *Chore.ToString(), *PS->GetPlayerName(), P.Variant);
	return true;
}

// ---- owner HUD ---------------------------------------------------------------------------------------------------------

void UKGWorldChoreComponent::GetWaypoints(TArray<FKGWorldWaypoint>& Out) const
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	const AKGCharacter* Char = GetCharacter();
	uint16 Best = 0;
	for (const FKGWorldProgress& P : Progress)
	{
		Best = FMath::Max(Best, P.Touch);
	}
	for (const FKGWorldProgress& P : Progress)
	{
		const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
		if (!Def || !Def->Steps.IsValidIndex(P.Step))
		{
			continue;
		}
		const bool bActive = Best > 0 && P.Touch == Best;
		const FKGWorldStepDef& S = Def->Steps[P.Step];
		if (S.Verb == EKGWorldVerb::Bring && IsValid(P.Item) && !P.Item->IsCarriedBy(Char) &&
		    FVector::DistSquared(P.Item->GetActorLocation(), Char ? Char->GetActorLocation() : FVector::ZeroVector) > FMath::Square(400.0f))
		{
			Out.Add({P.Item->GetActorLocation(), P.Chore, bActive, true});   // go and pick your item up
			continue;
		}
		const TArray<FName> Targets = Def->Targets(P.Step, P.Variant);
		for (int32 i = 0; i < Targets.Num(); ++i)
		{
			if ((P.DoneMask & (1 << i)) == 0)
			{
				const int32 Index = Cat.AnchorIndex(Targets[i]);
				Out.Add({Sub ? Sub->SpotLocation(Index) : Cat.Anchors[Index].Location, P.Chore, bActive, false});
			}
		}
	}
	Out.StableSort([](const FKGWorldWaypoint& A, const FKGWorldWaypoint& B) { return A.bActive && !B.bActive; });
}

FString UKGWorldChoreComponent::GetActiveLabel(FName* OutChore) const
{
	const FKGWorldProgress* Best = nullptr;
	for (const FKGWorldProgress& P : Progress)
	{
		if (!Best || P.Touch > Best->Touch)
		{
			Best = &P;
		}
	}
	const FKGWorldChoreDef* Def = Best ? FKGWorldChoreCatalog::Get().FindChore(Best->Chore) : nullptr;
	if (!Def || !Def->Steps.IsValidIndex(Best->Step))
	{
		return FString();
	}
	if (OutChore)
	{
		*OutChore = Best->Chore;
	}
	FString Label = Def->StepLabel(Best->Step, Best->Variant);
	const FKGWorldStepDef& S = Def->Steps[Best->Step];
	if (S.Repeat > 1)
	{
		Label += FString::Printf(TEXT("  %d/%d"), Best->Reps, S.Repeat);
	}
	const int32 N = Def->Targets(Best->Step, Best->Variant).Num();
	if (N > 1)
	{
		Label += FString::Printf(TEXT("  %d/%d"), FMath::CountBits(Best->DoneMask), N);
	}
	if (S.Verb == EKGWorldVerb::Bring && IsValid(Best->Item) && !Best->Item->IsCarriedBy(GetCharacter()))
	{
		Label = FString::Printf(TEXT("Pick up your %s (hold E) - %s"), *FKGWorldChoreCatalog::Get().FindItem(S.Item)->Label, *Label);
	}
	return FString::Printf(TEXT("%s: %s"), *Def->Title, *Label);
}

FText UKGWorldChoreComponent::PromptFor(int32 AnchorIndex) const
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	if (!Cat.Anchors.IsValidIndex(AnchorIndex))
	{
		return FText::GetEmpty();
	}
	for (const FKGWorldProgress& P : Progress)
	{
		const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
		if (!Def || !Def->Steps.IsValidIndex(P.Step) || TargetSlot(*Def, P, AnchorIndex) == INDEX_NONE)
		{
			continue;
		}
		const FKGWorldStepDef& S = Def->Steps[P.Step];
		if (S.Verb != EKGWorldVerb::Bring)
		{
			FString Label = Def->StepLabel(P.Step, P.Variant);
			if (S.Repeat > 1)
			{
				Label += FString::Printf(TEXT("  (%d/%d)"), P.Reps + 1, S.Repeat);
			}
			return FText::FromString(Label);
		}
	}
	const AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld());
	const FKGSpotState* State = Dir ? Dir->GetSpot(AnchorIndex) : nullptr;
	const FKGWorldAnchor& A = Cat.Anchors[AnchorIndex];
	if (State && State->bSpoiled && A.Sabotage == TEXT("poison"))
	{
		for (const FKGWorldProgress& P : Progress)
		{
			const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
			if (Def && TargetSlot(*Def, P, AnchorIndex) != INDEX_NONE)
			{
				return FText::FromString(TEXT("Dump the poisoned water"));
			}
		}
	}
	if (IsImpatient() && State && !State->bSpoiled && !A.Sabotage.IsNone() &&
	    (A.Sabotage == TEXT("poison") ? State->Level > 0.15f : State->bLit))
	{
		return FText::FromString(A.Sabotage == TEXT("poison") ? TEXT("Poison the water (sabotage)") : TEXT("Snuff the lamp (sabotage)"));
	}
	return FText::GetEmpty();
}

void UKGWorldChoreComponent::ClientStepDone_Implementation(FName Chore, uint8 Step, uint8 NumSteps, bool bChoreDone, const FString& Text)
{
	StepDoneText = bChoreDone ? FString::Printf(TEXT("%s - done!"), *Text) : Text;
	StepDoneAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_CLIENT_STEP %s step=%d/%d done=%d authority=%d: %s"), *Chore.ToString(), Step + 1, NumSteps,
	       bChoreDone ? 1 : 0, GetOwner() && GetOwner()->HasAuthority() ? 1 : 0, *Text);
}

void UKGWorldChoreComponent::ClientNotice_Implementation(const FString& Text)
{
	Notice = Text;
	NoticeAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

// ---- bots --------------------------------------------------------------------------------------------------------------

UKGWorldChoreComponent::EBot UKGWorldChoreComponent::BotDrive(AAIController* AI, FName Chore, float DeltaSeconds)
{
	AKGCharacter* Me = GetCharacter();
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	const FKGWorldChoreDef* Def = Cat.FindChore(Chore);
	UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	if (!AI || !Me || !Def || !Sub || !GetOwner()->HasAuthority())
	{
		return EBot::Failed;
	}
	FKGWorldProgress* P = MutableProgress(Chore);
	if (!P)
	{
		SyncList();
		P = MutableProgress(Chore);
		if (!P)
		{
			return EBot::Idle;
		}
	}
	if (BotChore != Chore)
	{
		BotChore = Chore;
		BotElapsed = 0.0f;
		BotFails = 0;
		BotRepath = 0.0f;
		BotProgressClock = 0.0f;
		BotProgressFrom = Me->GetActorLocation();
	}
	BotElapsed += DeltaSeconds;
	if (BotElapsed > 240.0f)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_BOT %s gives up %s at step %d (too long)"), *Me->GetName(), *Chore.ToString(), P->Step + 1);
		BotChore = NAME_None;
		return EBot::Failed;
	}
	if (Dwell.Kind != EKGDwell::None)
	{
		AI->StopMovement();
		return EBot::Busy;   // working / pouring
	}
	if (!Def->Steps.IsValidIndex(P->Step))
	{
		return EBot::Idle;
	}
	const FKGWorldStepDef& S = Def->Steps[P->Step];
	// Where to: your dropped item first, else the nearest target not done yet.
	int32 GoalAnchor = INDEX_NONE;
	FVector Goal;
	if (S.Verb == EKGWorldVerb::Bring && IsValid(P->Item) && !P->Item->IsCarriedBy(Me))
	{
		Goal = P->Item->GetActorLocation();
		if (FVector::DistSquared2D(Goal, Me->GetActorLocation()) < FMath::Square(160.0f))
		{
			P->Item->AuthAttachTo(Me);
			return EBot::Busy;
		}
	}
	else
	{
		double Best = TNumericLimits<double>::Max();
		const TArray<FName> Targets = Def->Targets(P->Step, P->Variant);
		for (int32 i = 0; i < Targets.Num(); ++i)
		{
			const int32 Index = Cat.AnchorIndex(Targets[i]);
			if ((P->DoneMask & (1 << i)) != 0 || Index == INDEX_NONE)
			{
				continue;
			}
			const double D = FVector::DistSquared(Sub->SpotLocation(Index), Me->GetActorLocation());
			if (D < Best)
			{
				Best = D;
				GoalAnchor = Index;
			}
		}
		if (GoalAnchor == INDEX_NONE)
		{
			return EBot::Idle;
		}
		if (Cat.Anchors[GoalAnchor].ClimbCm > 1.0f)
		{
			return EBot::Failed;   // bots don't climb ladders (never dealt; dev-given only)
		}
		Goal = Sub->SpotLocation(GoalAnchor);
		if (S.Verb != EKGWorldVerb::Bring && InReach(GoalAnchor, 20.0f))
		{
			AI->StopMovement();
			if (!AuthInteract(GoalAnchor))
			{
				++BotFails;
				return BotFails > 20 ? EBot::Failed : EBot::Busy;
			}
			return EBot::Busy;
		}
		if (S.Verb == EKGWorldVerb::Bring && InReach(GoalAnchor, -40.0f))
		{
			AI->StopMovement();   // standing there: the bring dwell ticks on its own
			return EBot::Busy;
		}
	}
	BotRepath -= DeltaSeconds;
	if (BotRepath <= 0.0f || !Goal.Equals(BotGoal, 50.0f))
	{
		BotRepath = 2.5f;
		BotGoal = Goal;
		if (AI->MoveToLocation(Goal, 40.0f, true, true, true, false) == EPathFollowingRequestResult::Failed)
		{
			if (++BotFails > 4)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_BOT %s cannot reach %s for %s"), *Me->GetName(), *Goal.ToCompactString(), *Chore.ToString());
				BotChore = NAME_None;
				return EBot::Failed;
			}
		}
	}
	// Progress watchdog: 8 s without getting 1 m closer = stuck.
	BotProgressClock += DeltaSeconds;
	if (BotProgressClock > 8.0f)
	{
		const bool bMoved = FVector::Dist(Me->GetActorLocation(), BotProgressFrom) > 100.0f;
		BotProgressClock = 0.0f;
		BotProgressFrom = Me->GetActorLocation();
		if (!bMoved && ++BotFails > 4)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_BOT %s stuck on %s step %d"), *Me->GetName(), *Chore.ToString(), P->Step + 1);
			BotChore = NAME_None;
			return EBot::Failed;
		}
	}
	return EBot::Busy;
}

// ---- dev ---------------------------------------------------------------------------------------------------------------

void UKGWorldChoreComponent::ServerDev_Implementation(const FString& Line)
{
#if !UE_BUILD_SHIPPING
	const AKGCharacter* Char = GetCharacter();
	const APlayerController* PC = Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
	if (!FKGDev::MayRun(PC))
	{
		UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE_DEV refused '%s' (kg.Dev.AllowClients 1 on the host)"), *Line);
		return;
	}
	const FString Result = AuthDev(Line);
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DEV %s -> %s"), *Line, *Result);
#endif
}

FString UKGWorldChoreComponent::AuthDev(const FString& Line)
{
#if !UE_BUILD_SHIPPING
	TArray<FString> A;
	Line.ParseIntoArrayWS(A);
	if (A.Num() == 0)
	{
		return TEXT("empty");
	}
	AKGCharacter* Char = GetCharacter();
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld());
	if (!Char || !Sub)
	{
		return TEXT("no body / subsystem");
	}
	// Target anchor: an id, or "next" (the highlighted chore's first pending target).
	auto ResolveAnchor = [&](const FString& What) -> int32
	{
		if (!What.Equals(TEXT("next"), ESearchCase::IgnoreCase))
		{
			return Cat.AnchorIndex(FName(*What));
		}
		TArray<FKGWorldWaypoint> Points;
		GetWaypoints(Points);
		for (const FKGWorldWaypoint& W : Points)
		{
			if (!W.bItem)
			{
				for (int32 i = 0; i < Cat.Anchors.Num(); ++i)
				{
					if (FVector::DistSquared2D(Sub->SpotLocation(i), W.Location) < 1.0)
					{
						return i;
					}
				}
			}
		}
		return INDEX_NONE;
	};
	const FString Verb = A[0];
	if (Verb.Equals(TEXT("Give"), ESearchCase::IgnoreCase) && A.Num() >= 2)
	{
		return AuthGive(FName(*A[1]), A.Num() >= 3 ? FCString::Atoi(*A[2]) : -1) ? TEXT("ok") : TEXT("unknown chore");
	}
	if ((Verb.Equals(TEXT("Path"), ESearchCase::IgnoreCase) || Verb.Equals(TEXT("Goto"), ESearchCase::IgnoreCase)) && A.Num() >= 2)
	{
		const int32 Index = ResolveAnchor(A[1]);
		if (Index == INDEX_NONE)
		{
			return TEXT("no such anchor");
		}
		const FVector Stand = Sub->StandLocation(Index);
		const FVector Spot = Sub->SpotLocation(Index);
		// Stop short of the spot so the carried item (held ~1.7 m ahead) lands inside it.
		const FVector Dir = (Spot - Stand).GetSafeNormal2D();
		const FVector Goal = Stand.Equals(Spot, 10.0f) ? Spot - (Spot - Char->GetActorLocation()).GetSafeNormal2D() * 120.0f : Stand;
		if (Verb.Equals(TEXT("Goto"), ESearchCase::IgnoreCase))
		{
			AKGChoreItem* Held = AKGChoreItem::CarriedBy(Char);
			const FVector Offset = Held ? Held->GetActorLocation() - Char->GetActorLocation() : FVector::ZeroVector;
			Char->TeleportTo(Goal + FVector(0.0f, 0.0f, 100.0f), (Spot - Goal).GetSafeNormal2D().Rotation());
			if (AController* C = Char->GetController())
			{
				C->SetControlRotation((Spot - Goal).GetSafeNormal2D().Rotation());
			}
			if (Held && !Held->IsAttached())
			{
				Held->SetActorLocation(Char->GetActorLocation() + Offset, false, nullptr, ETeleportType::TeleportPhysics);
			}
			return FString::Printf(TEXT("at %s"), *Cat.Anchors[Index].Id.ToString());
		}
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		UNavigationPath* Path = Nav ? Nav->FindPathToLocationSynchronously(GetWorld(), Char->GetActorLocation(), Goal, Char) : nullptr;
		TArray<FVector_NetQuantize> Points;
		if (Path && Path->IsValid())
		{
			for (const FVector& V : Path->PathPoints)
			{
				Points.Add(V);
			}
		}
		if (Points.Num() == 0)
		{
			Points.Add(Goal);
		}
		Points.Add(Goal + Dir * 30.0f);
		ClientDevPath(Points);
		return FString::Printf(TEXT("path to %s: %d points, %.1f m"), *Cat.Anchors[Index].Id.ToString(), Points.Num(),
		                       Path ? Path->GetPathLength() / 100.0f : -1.0f);
	}
	if (Verb.Equals(TEXT("Use"), ESearchCase::IgnoreCase) && A.Num() >= 2)
	{
		const int32 Index = ResolveAnchor(A[1]);
		return AuthInteract(Index) ? TEXT("started") : TEXT("nothing to do there");
	}
	if (Verb.Equals(TEXT("Skip"), ESearchCase::IgnoreCase))
	{
		for (FKGWorldProgress& P : Progress)
		{
			const FKGWorldChoreDef* Def = Cat.FindChore(P.Chore);
			if (Def && Def->Steps.IsValidIndex(P.Step) && (A.Num() < 2 || P.Chore == FName(*A[1])))
			{
				const TArray<FName> Targets = Def->Targets(P.Step, P.Variant);
				for (int32 i = 0; i < Targets.Num(); ++i)
				{
					if ((P.DoneMask & (1 << i)) == 0)
					{
						CompleteStep(P, Cat.AnchorIndex(Targets[i]));
						return TEXT("skipped one");
					}
				}
			}
		}
		return TEXT("nothing to skip");
	}
	return TEXT("unknown (Give <Chore> [Variant] | Path <anchor|next> | Goto <anchor|next> | Use <anchor|next> | Skip [Chore])");
#else
	return FString();
#endif
}

void UKGWorldChoreComponent::ClientDevPath_Implementation(const TArray<FVector_NetQuantize>& Points)
{
	AutoPath.Reset();
	for (const FVector_NetQuantize& P : Points)
	{
		AutoPath.Add(P);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_AUTOPILOT %d points"), AutoPath.Num());
}

void UKGWorldChoreComponent::TickAutopilot(float DeltaTime)
{
	AKGCharacter* Char = GetCharacter();
	if (!Char || !Char->IsLocallyControlled() || Char->IsDead())
	{
		AutoPath.Reset();
		return;
	}
	FVector Delta = AutoPath[0] - Char->GetActorLocation();
	Delta.Z = 0.0f;
	if (Delta.Size() < 55.0f)
	{
		AutoPath.RemoveAt(0);
		return;
	}
	const FVector Dir = Delta.GetSafeNormal();
	Char->AddMovementInput(Dir, 1.0f);
	if (AController* C = Char->GetController())
	{
		// Look where you walk, slightly down so the carried thing stays low and in front.
		FRotator R = C->GetControlRotation();
		R.Yaw = FMath::FixedTurn(R.Yaw, Dir.Rotation().Yaw, 360.0f * DeltaTime);
		R.Pitch = -12.0f;
		C->SetControlRotation(R);
	}
}
