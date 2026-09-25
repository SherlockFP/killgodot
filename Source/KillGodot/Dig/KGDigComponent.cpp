#include "Dig/KGDigComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Audio/KGAudio.h"
#include "Camera/CameraComponent.h"
#include "Character/KGBodyAnimInstance.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatUI.h"
#include "Combat/KGHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerController.h"
#include "Core/KGPlayerState.h"
#include "Dig/KGDigManager.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Fishing/KGFishingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryUI.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGLoot.h"
#include "KillGodot.h"
#include "Misc/Crc.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"

namespace KGDigComp
{
	const TCHAR* ShovelPath = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Shovel.SM_KG_Shovel");
	const TCHAR* ArmsPath = TEXT("/Game/KillGodot/Characters/FPArms2/SK_KG_FPArms2.SK_KG_FPArms2");
	const FName WeaponBone(TEXT("weapon_r"));

	// First-person grip on weapon_r, like the fishing rod (item +X = the blade end -> bone X, item up -> spine).
	// KEEP IN SYNC with SHOVEL_SCALE / SHOVEL_GRIP in Tools/Blender/kg_make_fp_arms2.py (the shovel_* clips).
	constexpr float ShovelScale = 0.70f;
	constexpr float ShovelGripCm = 85.0f;     // along the shovel's X, where the right hand closes
	constexpr float ShovelLength = 147.0f;    // SM_KG_Shovel: X 0 (handle) .. 147 (blade)

	template <typename T>
	T* LoadCached(const FString& Path)
	{
		static TMap<FString, TStrongObjectPtr<UObject>> Cache;
		if (const TStrongObjectPtr<UObject>* Hit = Cache.Find(Path))
		{
			return Cast<T>(Hit->Get());
		}
		T* Obj = LoadObject<T>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		Cache.Add(Path, TStrongObjectPtr<UObject>(Obj));
		return Obj;
	}

	/** First-person arms clip (A_FP2_shovel_*), falling back to the open-hands idle until the clips are imported. */
	UAnimSequence* FPClip(const TCHAR* Clip)
	{
		UAnimSequence* Seq = LoadCached<UAnimSequence>(
			FString::Printf(TEXT("/Game/KillGodot/Characters/FPArms2/Anims/A_FP2_%s.A_FP2_%s"), Clip, Clip));
		return Seq ? Seq : LoadCached<UAnimSequence>(TEXT("/Game/KillGodot/Characters/FPArms2/Anims/A_FP2_carry_idle.A_FP2_carry_idle"));
	}

	UAnimSequence* BodyClip(const TCHAR* Name)
	{
		return LoadCached<UAnimSequence>(FString::Printf(TEXT("/Game/KillGodot/Characters/Villager/Anims/%s.%s"), Name, Name));
	}

	FTransform Grip()
	{
		// Same construction as KGFP2::MakeGrip in AKGCharacter: tip axis -> socket +X, spine -> socket +Z.
		const FQuat Rot = FRotationMatrix::MakeFromXZ(FVector(1, 0, 0), FVector(0, 0, 1)).ToQuat().Inverse();
		return FTransform(Rot, -Rot.RotateVector(FVector(ShovelGripCm, 0.0, 0.0) * ShovelScale), FVector(ShovelScale));
	}

	uint32 PlayerHash(const AKGCharacter* C)
	{
		const AKGPlayerState* PS = C ? C->GetPlayerState<AKGPlayerState>() : nullptr;
		const FString Key = PS && !PS->Puid.IsEmpty() ? PS->Puid : (PS ? PS->GetPlayerName() : (C ? C->GetName() : FString()));
		return FCrc::StrCrc32(*Key);
	}
}

// =================================================================================================================
// Setup
// =================================================================================================================
UKGDigComponent::UKGDigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // after the character's own arms state machine
	SetIsReplicatedByDefault(true);
}

void UKGDigComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Everyone;
	Everyone.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGDigComponent, bShovelOut, Everyone);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGDigComponent, Action, Everyone);
	FDoRepLifetimeParams Owner;
	Owner.bIsPushBased = true;
	Owner.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGDigComponent, Result, Owner);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGDigComponent, TreasureMarks, Owner);
}

UKGDigComponent* UKGDigComponent::FindFor(const AActor* Pawn)
{
	return Pawn ? Pawn->FindComponentByClass<UKGDigComponent>() : nullptr;
}

void UKGDigComponent::BeginPlay()
{
	Super::BeginPlay();
	ShovelMesh = KGDigComp::LoadCached<UStaticMesh>(KGDigComp::ShovelPath);
	if (GetNetMode() != NM_DedicatedServer)
	{
		CreateVisuals();
	}
}

void UKGDigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetLocalPC())
	{
		UpdateInputContext(false);
	}
	Super::EndPlay(EndPlayReason);
}

AKGCharacter* UKGDigComponent::GetCharacter() const
{
	return Cast<AKGCharacter>(GetOwner());
}

bool UKGDigComponent::IsOwnerView() const
{
	const AKGCharacter* C = GetCharacter();
	return C && C->IsLocallyControlled() && C->IsPlayerControlled();
}

APlayerController* UKGDigComponent::GetLocalPC() const
{
	const AKGCharacter* C = GetCharacter();
	return IsOwnerView() ? Cast<APlayerController>(C->GetController()) : nullptr;
}

bool UKGDigComponent::HasShovelItem() const
{
	const UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(GetCharacter());
	return Pockets && Pockets->Has(FKGItemIds::Shovel);
}

AKGDigManager* UKGDigComponent::GetManager() const
{
	return AKGDigManager::Get(GetWorld());
}

// =================================================================================================================
// Queries
// =================================================================================================================
bool UKGDigComponent::IsShovelShown() const
{
	if (IsOwnerView() && PredictedShovel.IsSet() && GetWorld() && GetWorld()->GetRealTimeSeconds() < PredictedShovelUntil)
	{
		return PredictedShovel.GetValue();
	}
	return bShovelOut;
}

bool UKGDigComponent::IsDiggingShown() const
{
	return IsOwnerView() ? bLocalDigging : Action.bDigging;
}

EKGDigNotice UKGDigComponent::GetNotice(int32& OutA, int32& OutB, double& OutAt) const
{
	OutA = NoticeA;
	OutB = NoticeB;
	OutAt = NoticeAt;
	return Notice;
}

// =================================================================================================================
// Tick
// =================================================================================================================
void UKGDigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (IsOwnerView())
	{
		TickOwner(DeltaTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickVisuals(DeltaTime);
	}
}

// =================================================================================================================
// Server
// =================================================================================================================
bool UKGDigComponent::ServerTrySetShovel(bool bOut, EKGDigNotice* OutWhy)
{
	auto Refuse = [OutWhy](EKGDigNotice Why)
	{
		if (OutWhy)
		{
			*OutWhy = Why;
		}
		return false;
	};
	AKGCharacter* C = GetCharacter();
	if (!bOut)
	{
		ServerStopDig();
		if (bShovelOut)
		{
			bShovelOut = false;
			MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, bShovelOut, this);
		}
		return true;
	}
	if (!C || C->IsDead())
	{
		return Refuse(EKGDigNotice::NotNow);
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (GS && !FKGDigRules::CanDigInPhase(GS->GetPhase()))
	{
		return Refuse(EKGDigNotice::NotNow);
	}
	if (C->bHoldingAssassinBlade || C->bHoldingObject)
	{
		return Refuse(EKGDigNotice::Busy);
	}
	if (!HasShovelItem())
	{
		return Refuse(EKGDigNotice::NoShovel);
	}
	if (UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(C); Fishing && Fishing->GetServerPhase() != EKGFishPhase::Stowed)
	{
		Fishing->ServerStow();   // one tool at a time: the rod goes away
	}
	if (UKGEmoteComponent* Emote = C->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	if (!bShovelOut)
	{
		bShovelOut = true;
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, bShovelOut, this);
	}
	return true;
}

void UKGDigComponent::ServerSetShovel_Implementation(bool bOut)
{
	EKGDigNotice Why = EKGDigNotice::None;
	if (!ServerTrySetShovel(bOut, &Why) && Why != EKGDigNotice::None)
	{
		ServerNotify(Why);
	}
}

bool UKGDigComponent::ServerTryBeginDig(uint16 SpotId, const FVector& ViewStart, const FVector& ViewDir, EKGDigNotice* OutWhy)
{
	auto Refuse = [OutWhy](EKGDigNotice Why)
	{
		if (OutWhy)
		{
			*OutWhy = Why;
		}
		return false;
	};
	AKGCharacter* C = GetCharacter();
	AKGDigManager* Manager = GetManager();
	if (!C || C->IsDead() || !Manager)
	{
		return Refuse(EKGDigNotice::NotNow);
	}
	if (!bShovelOut || !HasShovelItem())
	{
		return Refuse(EKGDigNotice::NoShovel);
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if ((GS && !FKGDigRules::CanDigInPhase(GS->GetPhase())) || C->GetCharacterMovement()->IsSwimming())
	{
		return Refuse(EKGDigNotice::NotNow);
	}
	// The claimed eye must be where the server thinks the head is; the aim is the view ray meeting the ground.
	const FVector Eye = C->GetFirstPersonCamera() ? C->GetFirstPersonCamera()->GetComponentLocation() : C->GetActorLocation();
	const FVector Start = FVector::Dist(ViewStart, Eye) < 150.0f ? ViewStart : Eye;
	const FVector Dir = ViewDir.GetSafeNormal(UE_SMALL_NUMBER, C->GetActorForwardVector());
	const FVector Feet = C->GetActorLocation() - FVector(0.0, 0.0, C->GetSimpleCollisionHalfHeight());
	auto AimOnPlane = [&](float PlaneZ, FVector& Out)
	{
		if (Dir.Z > -0.05f)
		{
			return false;
		}
		const float T = (PlaneZ - Start.Z) / Dir.Z;
		if (T < 0.0f || T > FKGDigRules::Reach + 80.0f)
		{
			return false;
		}
		Out = Start + Dir * T;
		return true;
	};
	int32 Index = INDEX_NONE;
	if (SpotId != 0)
	{
		Index = Manager->IndexOfSpot(SpotId);
		if (Index == INDEX_NONE)
		{
			return Refuse(EKGDigNotice::NothingHere);
		}
		const FKGDigSpot& S = Manager->GetSpots()[Index];
		FVector Aim;
		if (!AimOnPlane(S.Location.Z, Aim) || FVector::Dist2D(Aim, FVector(S.Location)) > FKGDigRules::AimRadius(S.Kind) + 70.0f)
		{
			return Refuse(EKGDigNotice::TooFar);
		}
	}
	else
	{
		// Digging bare ground: only a buried chest (a treasure map's X) is there to find.
		if (NothingCooldown > 0.0f)
		{
			return Refuse(EKGDigNotice::NothingHere);
		}
		FVector Aim;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDigAim), false, C);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * (FKGDigRules::Reach + 60.0f), ECC_Visibility, Params))
		{
			Aim = Hit.ImpactPoint;
		}
		else if (!AimOnPlane(Feet.Z, Aim))
		{
			return Refuse(EKGDigNotice::TooFar);
		}
		Index = Manager->FindSpotNear(Aim, 40.0f);
		if (Index == INDEX_NONE)
		{
			Index = Manager->RevealBuriedNear(Aim, FKGDigRules::TreasureFindRadius);
		}
		if (Index == INDEX_NONE)
		{
			NothingCooldown = 0.6f;
			return Refuse(EKGDigNotice::NothingHere);
		}
	}
	const FKGDigSpot& Spot = Manager->GetSpots()[Index];
	if (Spot.IsDugOut())
	{
		return Refuse(EKGDigNotice::DugOut);
	}
	if (FVector::Dist2D(Feet, FVector(Spot.Location)) > FKGDigRules::StandRadius)
	{
		return Refuse(EKGDigNotice::TooFar);
	}
	// One shovel per hole.
	for (TActorIterator<AKGCharacter> It(GetWorld()); It; ++It)
	{
		const UKGDigComponent* Other = *It != C ? UKGDigComponent::FindFor(*It) : nullptr;
		if (Other && Other->Action.bDigging && Other->Action.SpotId == Spot.Id)
		{
			return Refuse(EKGDigNotice::Busy);
		}
	}
	const bool bContinue = Action.bDigging && Action.SpotId == Spot.Id;
	ServerSpot = Index;
	if (!bContinue)
	{
		HoldClock.Start(FKGDigRules::HoldSeconds(Spot.Kind));
	}
	Action.bDigging = true;
	Action.SpotId = Spot.Id;
	Action.HoldQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FKGDigRules::HoldSeconds(Spot.Kind) * 20.0f), 1, 255));
	++Action.Serial;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, Action, this);
	if (Spot.Kind == EKGDigKind::Grave && Spot.Stage == 0 && !bContinue)
	{
		ServerNotify(EKGDigNotice::GraveWarning);
	}
	if (UKGEmoteComponent* Emote = C->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	return true;
}

void UKGDigComponent::ServerBeginDig_Implementation(uint16 SpotId, FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir)
{
	EKGDigNotice Why = EKGDigNotice::None;
	if (!ServerTryBeginDig(SpotId, ViewStart, ViewDir, &Why) && Why != EKGDigNotice::None)
	{
		ServerNotify(Why);
	}
}

void UKGDigComponent::ServerEndDig_Implementation()
{
	ServerStopDig();
}

void UKGDigComponent::ServerStopDig()
{
	ServerSpot = INDEX_NONE;
	if (Action.bDigging)
	{
		Action.bDigging = false;
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, Action, this);
	}
}

bool UKGDigComponent::ServerCheckStillDigging(EKGDigNotice& OutWhy) const
{
	const AKGCharacter* C = GetCharacter();
	const AKGDigManager* Manager = GetManager();
	const int32 Index = Manager ? Manager->IndexOfSpot(Action.SpotId) : INDEX_NONE;
	if (!C || C->IsDead() || !bShovelOut || Index == INDEX_NONE)
	{
		OutWhy = EKGDigNotice::None;
		return false;
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (GS && !FKGDigRules::CanDigInPhase(GS->GetPhase()))
	{
		OutWhy = EKGDigNotice::NotNow;
		return false;
	}
	const FKGDigSpot& S = Manager->GetSpots()[Index];
	if (S.IsDugOut())
	{
		OutWhy = EKGDigNotice::None;
		return false;
	}
	const FVector Feet = C->GetActorLocation() - FVector(0.0, 0.0, C->GetSimpleCollisionHalfHeight());
	if (FVector::Dist2D(Feet, FVector(S.Location)) > FKGDigRules::StandRadius + 40.0f)
	{
		OutWhy = EKGDigNotice::TooFar;
		return false;
	}
	return true;
}

void UKGDigComponent::TickServer(float DeltaTime)
{
	AKGCharacter* C = GetCharacter();
	if (!C)
	{
		return;
	}
	if (!bHealthBound && C->GetHealth())
	{
		C->GetHealth()->OnHealthChanged.AddDynamic(this, &UKGDigComponent::HandleHealthChanged);
		bHealthBound = true;
	}
	NothingCooldown = FMath::Max(0.0f, NothingCooldown - DeltaTime);
	if (bShovelOut)
	{
		const UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(C);
		const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
		const bool bPhaseOk = !GS || FKGDigRules::CanDigInPhase(GS->GetPhase());
		if (C->IsDead() || C->bHoldingAssassinBlade || C->bHoldingObject || !bPhaseOk || !HasShovelItem() ||
		    (Fishing && Fishing->GetServerPhase() != EKGFishPhase::Stowed))
		{
			ServerTrySetShovel(false);
			if (!bPhaseOk)
			{
				ServerNotify(EKGDigNotice::NotNow);
			}
		}
	}
	TreasureAccum += DeltaTime;
	if (TreasureAccum >= 1.0f)
	{
		TreasureAccum = 0.0f;
		ServerRefreshTreasure();
	}
	if (!Action.bDigging)
	{
		return;
	}
	EKGDigNotice Why = EKGDigNotice::None;
	if (!ServerCheckStillDigging(Why))
	{
		ServerStopDig();
		if (Why != EKGDigNotice::None)
		{
			ServerNotify(Why);
		}
		return;
	}
	if (HoldClock.Advance(DeltaTime))
	{
		ServerCompleteStage();
	}
}

void UKGDigComponent::DebugTickServer(float Seconds, float Step)
{
	for (float T = 0.0f; T < Seconds - KINDA_SMALL_NUMBER; T += Step)
	{
		TickServer(Step);
	}
}

void UKGDigComponent::ServerCompleteStage()
{
	AKGCharacter* C = GetCharacter();
	AKGDigManager* Manager = GetManager();
	const int32 Index = Manager ? Manager->IndexOfSpot(Action.SpotId) : INDEX_NONE;
	if (!C || Index == INDEX_NONE)
	{
		ServerStopDig();
		return;
	}
	TArray<FKGItemStack> Items;
	if (!Manager->ApplyStage(Index, Items))
	{
		ServerStopDig();
		return;
	}
	const FKGDigSpot& Spot = Manager->GetSpots()[Index];
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(C);
	TArray<FKGItemStack> Left;
	for (const FKGItemStack& Stack : Items)
	{
		const int32 Added = Pockets ? Pockets->AddItem(Stack.ItemId, Stack.Count) : 0;
		if (Added < Stack.Count)
		{
			Left.Emplace(Stack.ItemId, Stack.Count - Added);
		}
	}
	if (Left.Num() > 0)
	{
		FKGRng DropRng = FKGDigRules::StageRng(Manager->GetGeneratedSeed() ^ 0xD20Bull, Spot.Id, Spot.Stage);
		FKGLoot::SpawnLoot(C, Left, FVector(Spot.Location) + FVector(0.0, 0.0, 40.0), DropRng, 70.0f);
	}
	if (Spot.Kind == EKGDigKind::Treasure && Spot.IsDugOut() && Pockets && Pockets->Has(FKGItemIds::TreasureMap))
	{
		Pockets->RemoveItem(FKGItemIds::TreasureMap, 1);   // the map led here: it is used up
	}
	++Result.Serial;
	Result.SpotId = Spot.Id;
	Result.Kind = Spot.Kind;
	Result.Stage = Spot.Stage;
	Result.MaxStage = Spot.MaxStage;
	Result.Items = Items;
	Result.bDropped = Left.Num() > 0;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, Result, this);
	FString What;
	for (const FKGItemStack& S : Items)
	{
		What += FString::Printf(TEXT("%s%s x%d"), What.IsEmpty() ? TEXT("") : TEXT(", "), *S.ItemId.ToString(), S.Count);
	}
	const AKGPlayerState* PS = C->GetPlayerState<AKGPlayerState>();
	UE_LOG(LogKillGodot, Log, TEXT("KG_DIG_STAGE spot=%d kind=%s stage=%d/%d by=%s loot=[%s] dropped=%d"), Spot.Id,
	       FKGDigRules::KindName(Spot.Kind), Spot.Stage, Spot.MaxStage, PS ? *PS->GetPlayerName() : *C->GetName(), *What,
	       Left.Num() > 0 ? 1 : 0);
	if (Spot.IsDugOut())
	{
		ServerStopDig();
	}
	else
	{
		HoldClock.Start(FKGDigRules::HoldSeconds(Spot.Kind));   // still holding: the next stage starts
		++Action.Serial;
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, Action, this);
	}
}

int32 UKGDigComponent::ServerFinishSpot(int32 Index)
{
	AKGDigManager* Manager = GetManager();
	if (!Manager || !Manager->GetSpots().IsValidIndex(Index))
	{
		return 0;
	}
	int32 Total = 0;
	const uint16 Id = Manager->GetSpots()[Index].Id;
	for (int32 Guard = 0; Guard < 8 && !Manager->GetSpots()[Index].IsDugOut(); ++Guard)
	{
		Action.SpotId = Id;
		ServerCompleteStage();
		for (const FKGItemStack& S : Result.Items)
		{
			Total += S.Count;
		}
	}
	ServerStopDig();
	return Total;
}

void UKGDigComponent::ServerRefreshTreasure()
{
	AKGCharacter* C = GetCharacter();
	UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPawn(C);
	AKGDigManager* Manager = GetManager();
	if (!C || !Pockets)
	{
		return;
	}
	// Three scraps make a map.
	int32 Left = 0;
	const int32 Maps = FKGDigRules::MapsFromScraps(Pockets->Count(FKGItemIds::MapScrap), Left);
	if (Maps > 0 && Pockets->RoomFor(FKGItemIds::TreasureMap) >= 1)
	{
		const int32 Made = FMath::Min(Maps, Pockets->RoomFor(FKGItemIds::TreasureMap));
		Pockets->RemoveItem(FKGItemIds::MapScrap, Made * FKGDigRules::ScrapsPerMap);
		Pockets->AddItem(FKGItemIds::TreasureMap, Made);
		ServerNotify(EKGDigNotice::MapAssembled, Made);
	}
	TArray<FVector2D> Marks;
	if (Manager)
	{
		const TArray<FKGDigSpot>& Buried = Manager->GetBuried();
		const int32 Held = FMath::Min(Pockets->Count(FKGItemIds::TreasureMap), 3);
		const uint32 Hash = KGDigComp::PlayerHash(C);
		for (int32 k = 0; k < Held; ++k)
		{
			const int32 i = FKGDigRules::TreasureFor(Hash, k, Buried.Num());
			if (Buried.IsValidIndex(i))
			{
				Marks.AddUnique(FVector2D(Buried[i].Location));
			}
		}
	}
	if (Marks != TreasureMarks)
	{
		TreasureMarks = Marks;
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGDigComponent, TreasureMarks, this);
	}
}

void UKGDigComponent::ServerNotify(EKGDigNotice InNotice, int32 A, int32 B)
{
	ClientNotice(InNotice, A, B);
}

void UKGDigComponent::ClientNotice_Implementation(EKGDigNotice InNotice, int32 A, int32 B)
{
	Notice = InNotice;
	NoticeA = A;
	NoticeB = B;
	NoticeAt = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (InNotice == EKGDigNotice::NothingHere || InNotice == EKGDigNotice::DugOut || InNotice == EKGDigNotice::TooFar)
	{
		bLocalDigging = false;
		LocalProgress = 0.0f;
	}
}

void UKGDigComponent::HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (Delta < 0.0f && Action.bDigging)
	{
		ServerStopDig();   // a hit knocks the shovel out of the rhythm
		ServerNotify(EKGDigNotice::Busy, 1);
	}
}

// =================================================================================================================
// Owner
// =================================================================================================================
void UKGDigComponent::RequestToggleShovel()
{
	RequestSetShovel(!IsShovelShown());
}

void UKGDigComponent::RequestSetShovel(bool bOut)
{
	if (bOut && !HasShovelItem())
	{
		ClientNotice_Implementation(EKGDigNotice::NoShovel, 0, 0);
		return;
	}
	if (!bOut)
	{
		StopDigging();
	}
	PredictedShovel = bOut;
	PredictedShovelUntil = GetWorld()->GetRealTimeSeconds() + 1.0;
	bDrawPlayed = !bOut;
	ServerSetShovel(bOut);
}

bool UKGDigComponent::StartDigging(uint16 SpotId)
{
	if (!IsShovelShown() || !GetWorld())
	{
		return false;
	}
	const AKGCharacter* C = GetCharacter();
	const AKGDigManager* Manager = GetManager();
	const FKGDigSpot* Spot = Manager && SpotId != 0 ? Manager->FindSpot(SpotId) : nullptr;
	bLocalDigging = true;
	LocalSpotId = SpotId;
	LocalStageSeen = Spot ? Spot->Stage : 0;
	LocalHold = FKGDigRules::HoldSeconds(Spot ? Spot->Kind : EKGDigKind::Treasure);
	LocalProgress = 0.0f;
	LocalStartAt = GetWorld()->GetRealTimeSeconds();
	AimLostFor = 0.0f;
	const FVector Eye = C && C->GetFirstPersonCamera() ? C->GetFirstPersonCamera()->GetComponentLocation() : FVector::ZeroVector;
	const FVector Dir = C ? C->GetControlRotation().Vector() : FVector::ForwardVector;
	ServerBeginDig(SpotId, Eye, Dir);
	return true;
}

void UKGDigComponent::StopDigging()
{
	if (bLocalDigging)
	{
		bLocalDigging = false;
		ServerEndDig();
	}
	LocalProgress = 0.0f;
	ScriptedSeconds = 0.0f;
}

void UKGDigComponent::SetScriptedDig(uint16 SpotId, float Seconds)
{
	ScriptedSpot = SpotId;
	ScriptedSeconds = Seconds;
}

void UKGDigComponent::UpdateAim()
{
	const AKGCharacter* C = GetCharacter();
	const AKGDigManager* Manager = GetManager();
	bAimValid = false;
	bAimOnMark = false;
	AimSpot = INDEX_NONE;
	if (!C || !C->GetFirstPersonCamera())
	{
		return;
	}
	const FVector Start = C->GetFirstPersonCamera()->GetComponentLocation();
	const FVector Dir = C->GetControlRotation().Vector();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDigAimOwner), false, C);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * FKGDigRules::Reach, ECC_Visibility, Params))
	{
		return;
	}
	bAimValid = true;
	AimPoint = Hit.ImpactPoint;
	AimSpot = Manager ? Manager->FindSpotNear(AimPoint) : INDEX_NONE;
	for (const FVector2D& Mark : TreasureMarks)
	{
		if (FVector2D::Distance(Mark, FVector2D(AimPoint)) <= FKGDigRules::TreasureFindRadius)
		{
			bAimOnMark = true;
		}
	}
}

void UKGDigComponent::UpdateInputContext(bool bOn)
{
	APlayerController* PC = GetLocalPC();
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	if (!Input)
	{
		return;
	}
	if (!ConsumeAction)
	{
		// Mapped above IMC_KG_Default: the mouse digs instead of punching / shoving while the shovel is out.
		ConsumeAction = NewObject<UInputAction>(this, TEXT("IA_KG_DigConsume"));
		ConsumeAction->ValueType = EInputActionValueType::Boolean;
		ConsumeAction->bConsumeInput = true;
		ShovelContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KG_Shovel"));
		ShovelContext->MapKey(ConsumeAction, EKeys::LeftMouseButton);
		ShovelContext->MapKey(ConsumeAction, EKeys::RightMouseButton);
	}
	if (bOn != bContextOn)
	{
		bContextOn = bOn;
		if (bOn)
		{
			Input->AddMappingContext(ShovelContext, 10);
		}
		else
		{
			Input->RemoveMappingContext(ShovelContext);
		}
	}
}

void UKGDigComponent::OnResultArrived()
{
	ResultShownAt = GetWorld()->GetRealTimeSeconds();
	const bool bFinal = Result.Stage >= Result.MaxStage;
	if (Result.Items.Num() > 0)
	{
		KGAudio::UI(this, bFinal ? TEXT("S_Fish_Coins") : TEXT("S_UI_Good"), bFinal ? 0.7f : 0.4f);
	}
}

void UKGDigComponent::TickOwner(float DeltaTime)
{
	APlayerController* PC = GetLocalPC();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	if (PredictedShovel.IsSet() && (Now >= PredictedShovelUntil || PredictedShovel.GetValue() == bShovelOut))
	{
		PredictedShovel.Reset();
	}
	if (Result.Serial != ShownResultSerial)
	{
		if (bResultPrimed)
		{
			OnResultArrived();
		}
		ShownResultSerial = Result.Serial;
	}
	bResultPrimed = true;

	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	const bool bBlocked = FKGChatUI::IsTyping(PC) || FKGChatUI::IsWheelOpen(PC) || FKGInventoryUI::IsOpen(PC) ||
	                      (KGPC && KGPC->IsPauseMenuOpen()) || PC->bShowMouseCursor;
	if (!bBlocked && PC->WasInputKeyJustPressed(EKeys::Q))
	{
		RequestToggleShovel();
	}
	const bool bShown = IsShovelShown();
	// One tool at a time: the rod coming out puts the shovel away.
	if (bShown && !PredictedShovel.IsSet())
	{
		if (const UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(GetOwner()); Fishing && Fishing->IsRodShown())
		{
			RequestSetShovel(false);
		}
	}
	ProbeCooldown = FMath::Max(0.0f, ProbeCooldown - DeltaTime);
	if (bShown)
	{
		UpdateAim();
	}
	else
	{
		bAimValid = false;
		AimSpot = INDEX_NONE;
	}
	const bool bScripted = ScriptedSeconds > 0.0f;
	if (bScripted)
	{
		ScriptedSeconds -= DeltaTime;
	}
	const bool bMouse = !bBlocked && (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger));
	const bool bHeld = bShown && (bMouse || bScripted);
	const AKGDigManager* Manager = GetManager();
	if (bHeld && !bLocalDigging)
	{
		const bool bPress = bScripted || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
		                    PC->WasInputKeyJustPressed(EKeys::Gamepad_RightTrigger);
		const uint16 Target = bScripted ? ScriptedSpot
		                    : (Manager && Manager->GetSpots().IsValidIndex(AimSpot) ? Manager->GetSpots()[AimSpot].Id : 0);
		if (Target != 0 && Manager && Manager->FindSpot(Target) && !Manager->FindSpot(Target)->IsDugOut() && bPress)
		{
			StartDigging(Target);
		}
		else if (Target != 0 && bPress)
		{
			ClientNotice_Implementation(EKGDigNotice::DugOut, 0, 0);
		}
		else if (bAimOnMark && bPress)
		{
			StartDigging(0);   // dig at the X: the server looks for the buried chest
		}
		else if (bPress && ProbeCooldown <= 0.0f)
		{
			ProbeCooldown = 0.6f;
			ClientNotice_Implementation(EKGDigNotice::NothingHere, 0, 0);
		}
	}
	if (bLocalDigging)
	{
		if (!bHeld)
		{
			StopDigging();
		}
		else
		{
			if (LocalSpotId == 0 && Action.bDigging && Action.SpotId != 0)
			{
				LocalSpotId = Action.SpotId;   // the buried chest the server revealed
				const FKGDigSpot* Revealed = Manager ? Manager->FindSpot(LocalSpotId) : nullptr;
				LocalStageSeen = Revealed ? Revealed->Stage : 0;
			}
			const FKGDigSpot* Spot = Manager && LocalSpotId != 0 ? Manager->FindSpot(LocalSpotId) : nullptr;
			if (Spot && Spot->Stage != LocalStageSeen)
			{
				LocalStageSeen = Spot->Stage;
				LocalProgress = 0.0f;
				LocalStartAt = Now;
				if (Spot->IsDugOut())
				{
					bLocalDigging = false;
					ScriptedSeconds = 0.0f;
				}
			}
			LocalProgress = FMath::Min(0.98f, LocalProgress + DeltaTime / FMath::Max(LocalHold, 0.1f));
			// Looked away from the hole: stop (scripted digs keep their target).
			const int32 Want = Manager && LocalSpotId != 0 ? Manager->IndexOfSpot(LocalSpotId) : INDEX_NONE;
			AimLostFor = (!bScripted && Want != INDEX_NONE && AimSpot != Want) ? AimLostFor + DeltaTime : 0.0f;
			if (AimLostFor > 0.35f)
			{
				StopDigging();
			}
			// The server never started (refused, with a notice): give up the prediction.
			if (bLocalDigging && Now - LocalStartAt > 0.9 && !Action.bDigging)
			{
				bLocalDigging = false;
				LocalProgress = 0.0f;
			}
		}
	}
	UpdateInputContext(bShown && !bBlocked);
}

// =================================================================================================================
// Visuals
// =================================================================================================================
void UKGDigComponent::CreateVisuals()
{
	AActor* Owner = GetOwner();
	if (!Owner || bVisualsReady || !Owner->GetRootComponent())
	{
		return;
	}
	TPShovel = NewObject<UStaticMeshComponent>(Owner, TEXT("KGDigShovelTP"), RF_Transient);
	TPShovel->SetupAttachment(Owner->GetRootComponent());
	TPShovel->SetUsingAbsoluteLocation(true);
	TPShovel->SetUsingAbsoluteRotation(true);
	TPShovel->SetUsingAbsoluteScale(true);
	TPShovel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TPShovel->SetGenerateOverlapEvents(false);
	TPShovel->SetCanEverAffectNavigation(false);
	TPShovel->SetOwnerNoSee(true);
	TPShovel->SetStaticMesh(ShovelMesh);
	TPShovel->SetVisibility(false);
	TPShovel->RegisterComponent();
	bVisualsReady = true;
}

void UKGDigComponent::TickVisuals(float DeltaTime)
{
	if (!bVisualsReady)
	{
		return;
	}
	const bool bDigging = IsDiggingShown() && IsShovelShown();
	const float Hold = IsOwnerView() ? LocalHold : Action.HoldQ / 20.0f;
	if (bDigging)
	{
		const float Before = StrokeClock;
		StrokeClock += DeltaTime / FMath::Max(Hold, 0.2f);
		// Contact at ~40% of a stroke: the spade bites (everyone hears it; the stage end plays the toss).
		if (Before < 0.4f && StrokeClock >= 0.4f)
		{
			const AKGDigManager* Manager = GetManager();
			const FKGDigSpot* Spot = Manager ? Manager->FindSpot(IsOwnerView() ? LocalSpotId : Action.SpotId) : nullptr;
			const FVector At = Spot ? FVector(Spot->Location) : GetOwner()->GetActorLocation();
			KGAudio::At(this, TEXT("S_Dig_Stab"), At, Spot && Spot->Kind == EKGDigKind::Grave ? 1.0f : 0.7f);
			if (AKGDigManager* M = GetManager())
			{
				M->Puff(At + FVector(0.0, 0.0, 10.0), 0.35f);
			}
		}
		if (StrokeClock >= 1.0f)
		{
			StrokeClock -= 1.0f;
		}
	}
	else
	{
		StrokeClock = 0.0f;
	}
	UpdateThirdPerson(DeltaTime);
	UpdateBodyPose();
	if (IsOwnerView())
	{
		UpdateFirstPerson(DeltaTime);
	}
}

void UKGDigComponent::UpdateFirstPerson(float DeltaTime)
{
	AKGCharacter* C = GetCharacter();
	if (!C)
	{
		return;
	}
	USkeletalMeshComponent* CharArms = nullptr;
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(C);
		for (USkeletalMeshComponent* M : Meshes)
		{
			if (M && M->GetFName() == TEXT("ArmsMesh"))
			{
				CharArms = M;
				break;
			}
		}
	}
	if (!DigArms && CharArms && CharArms->GetAttachParent())
	{
		// A second FPArms2 rig of our own, on the same viewmodel pivot: the character's arms state machine is left alone.
		DigArms = NewObject<USkeletalMeshComponent>(C, TEXT("KGDigArms"), RF_Transient);
		DigArms->SetupAttachment(CharArms->GetAttachParent());
		DigArms->SetSkeletalMeshAsset(KGDigComp::LoadCached<USkeletalMesh>(KGDigComp::ArmsPath));
		DigArms->SetOnlyOwnerSee(true);
		DigArms->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		DigArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DigArms->CastShadow = false;
		DigArms->SetRelativeTransform(CharArms->GetRelativeTransform());
		DigArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		DigArms->SetVisibility(false);
		DigArms->RegisterComponent();
		FPShovel = NewObject<UStaticMeshComponent>(C, TEXT("KGDigShovelFP"), RF_Transient);
		FPShovel->SetupAttachment(DigArms, KGDigComp::WeaponBone);
		FPShovel->SetOnlyOwnerSee(true);
		FPShovel->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		FPShovel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FPShovel->CastShadow = false;
		FPShovel->SetStaticMesh(ShovelMesh);
		FPShovel->SetRelativeTransform(KGDigComp::Grip());
		FPShovel->SetVisibility(false);
		FPShovel->RegisterComponent();
	}
	if (!DigArms)
	{
		return;
	}
	const UKGEmoteComponent* Emote = C->GetEmote();
	const bool bShow = IsShovelShown() && !C->IsDead() && !(Emote && Emote->WantsViewmodelHidden()) &&
	                   !(CharArms && CharArms->IsVisible());
	if (DigArms->IsVisible() != bShow)
	{
		DigArms->SetVisibility(bShow);
		FPShovel->SetVisibility(bShow);
		if (!bShow)
		{
			ArmsPlaying = nullptr;
		}
	}
	if (!bShow)
	{
		return;
	}
	UAnimSequence* Want = nullptr;
	float Rate = 1.0f;
	bool bLoop = true;
	if (!bDrawPlayed)
	{
		Want = KGDigComp::FPClip(TEXT("shovel_draw"));
		bLoop = false;
		bDrawPlayed = true;
	}
	else if (IsDiggingShown())
	{
		Want = KGDigComp::FPClip(TEXT("shovel_dig"));
		Rate = Want ? Want->GetPlayLength() / FMath::Max(LocalHold, 0.2f) : 1.0f;   // one stroke per stage
	}
	else
	{
		Want = KGDigComp::FPClip(TEXT("shovel_idle"));
	}
	const UAnimSingleNodeInstance* Single = DigArms->GetSingleNodeInstance();
	const bool bOneShotRunning = ArmsPlaying && Single && !Single->IsLooping() && Single->IsPlaying();
	if (Want && Want != ArmsPlaying && (!bOneShotRunning || !bLoop))
	{
		DigArms->PlayAnimation(Want, bLoop);
		ArmsPlaying = Want;
		ArmsRate = 1.0f;
	}
	if (Want && !FMath::IsNearlyEqual(ArmsRate, Rate) && ArmsPlaying == Want)
	{
		DigArms->SetPlayRate(Rate);
		ArmsRate = Rate;
	}
}

void UKGDigComponent::UpdateThirdPerson(float DeltaTime)
{
	AKGCharacter* C = GetCharacter();
	if (!C || !TPShovel)
	{
		return;
	}
	const bool bShow = IsShovelShown() && !C->IsDead();
	if (TPShovel->IsVisible() != bShow)
	{
		TPShovel->SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}
	// Blade down in front of the body; while digging it follows the stroke: lift, plunge, lever, toss.
	float Pitch = -58.0f;
	float Yaw = 8.0f;
	if (IsDiggingShown())
	{
		const float T = StrokeClock;
		if (T < 0.25f)
		{
			Pitch = FMath::Lerp(-58.0f, -25.0f, T / 0.25f);
		}
		else if (T < 0.42f)
		{
			Pitch = FMath::Lerp(-25.0f, -82.0f, (T - 0.25f) / 0.17f);
		}
		else if (T < 0.62f)
		{
			Pitch = FMath::Lerp(-82.0f, -48.0f, (T - 0.42f) / 0.2f);
		}
		else if (T < 0.82f)
		{
			Pitch = FMath::Lerp(-48.0f, -20.0f, (T - 0.62f) / 0.2f);
			Yaw = FMath::Lerp(8.0f, 55.0f, (T - 0.62f) / 0.2f);
		}
		else
		{
			Pitch = FMath::Lerp(-20.0f, -58.0f, (T - 0.82f) / 0.18f);
			Yaw = FMath::Lerp(55.0f, 8.0f, (T - 0.82f) / 0.18f);
		}
	}
	const FRotator Want(Pitch, C->GetActorRotation().Yaw + Yaw, 0.0f);
	TPRot = TPShovel->IsVisible() ? FMath::RInterpTo(TPRot, Want, DeltaTime, 18.0f) : Want;
	const USkeletalMeshComponent* Body = C->GetMesh();
	const FVector Hand = Body && Body->DoesSocketExist(TEXT("hand_r")) ? Body->GetSocketLocation(TEXT("hand_r"))
	                                                                   : C->GetActorLocation() + C->GetActorForwardVector() * 30.0f;
	const FVector Dir = TPRot.Vector();
	TPShovel->SetWorldLocationAndRotation(Hand - Dir * KGDigComp::ShovelGripCm, FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).Rotator());
}

void UKGDigComponent::UpdateBodyPose()
{
	AKGCharacter* C = GetCharacter();
	UKGBodyAnimInstance* Body = C ? C->GetBodyAnim() : nullptr;
	if (!Body)
	{
		return;
	}
	const UKGEmoteComponent* Emote = C->GetEmote();
	if (Emote && Emote->GetShownEmote())
	{
		BodyClip = nullptr;   // a social emote owns the layer
		return;
	}
	if (const UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(C); Fishing && Fishing->IsRodShown())
	{
		BodyClip = nullptr;   // the angler pose owns it
		return;
	}
	UAnimSequence* Want = nullptr;
	if (IsShovelShown() && !C->IsDead())
	{
		Want = IsDiggingShown() ? KGDigComp::BodyClip(TEXT("A_KG_Sword_Attack")) : KGDigComp::BodyClip(TEXT("A_KG_Idle_Torch_Loop"));
	}
	const bool bPlaying = Want && Body->IsEmoteWanted() && Body->GetEmoteMain() == Want;
	if (Want == BodyClip && (bPlaying || !Want))
	{
		return;
	}
	if (Want)
	{
		const float Hold = IsOwnerView() ? LocalHold : Action.HoldQ / 20.0f;
		const float Rate = IsDiggingShown() ? Want->GetPlayLength() / FMath::Max(Hold, 0.2f) : 1.0f;
		Body->PlayEmote(nullptr, Want, true, true, 0.2f, Rate);
	}
	else if (BodyClip && Body->GetEmoteMain() == BodyClip)
	{
		Body->StopEmote(0.25f);
	}
	BodyClip = Want;
}
