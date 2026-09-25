#include "Fishing/KGFishingComponent.h"
#include "Animation/AnimSequence.h"
#include "Audio/KGAudio.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/KGBodyAnimInstance.h"
#include "Character/KGCharacter.h"
#include "Character/KGViewmodelComponent.h"
#include "Chat/KGChatUI.h"
#include "Combat/KGHealthComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerController.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Fishing/KGFishingJournal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryUI.h"
#include "Inventory/KGItemCatalog.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"
#include "World/KGPickup.h"
#include "World/KGWaves.h"

namespace KGFishingComp
{
	const TCHAR* RodPath = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_FishingRod.SM_KG_FishingRod");
	const TCHAR* BobberPath = TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Bobber.SM_KG_Bobber");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* ShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	constexpr float RodLength = 190.0f;   // SM_KG_FishingRod: butt at X=0, tip at X=190 (kg_make_water_props.py)
	constexpr float RodGrip = 20.0f;      // where the hand closes on the cork
	constexpr int32 LineSegments = 16;
	constexpr int32 MaxDrops = 24;
	constexpr float BobberScale = 2.0f;   // readable at 20 m (toon scale)

	TAutoConsoleVariable<int32> CVarDebug(TEXT("kg.Fish.Debug"), 0,
		TEXT("Fishing debug overlay: tension/stamina/distance, server vs predicted (kg.Fish.Tension toggles it)."));
	TAutoConsoleVariable<float> CVarFocusFov(TEXT("kg.Fish.FocusFOV"), 0.12f,
		TEXT("How much the view narrows while reeling (fraction of the FOV at full focus)."));

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

	/** First-person arms clip (Tools/Blender/kg_make_fp_arms2.py rod_*), falling back to the knife hold. */
	UAnimSequence* FPClip(const TCHAR* Clip)
	{
		UAnimSequence* Seq = LoadCached<UAnimSequence>(
			FString::Printf(TEXT("/Game/KillGodot/Characters/FPArms2/Anims/A_FP2_%s.A_FP2_%s"), Clip, Clip));
		return Seq ? Seq : LoadCached<UAnimSequence>(TEXT("/Game/KillGodot/Characters/FPArms2/Anims/A_FP2_knife_idle.A_FP2_knife_idle"));
	}

	/** Third-person upper-body clip (Tools/Blender/kg_make_emotes.py Fish_*), falling back to a villager clip. */
	UAnimSequence* TPClip(const TCHAR* Clip, const TCHAR* Fallback)
	{
		UAnimSequence* Seq = LoadCached<UAnimSequence>(FString::Printf(
			TEXT("/Game/KillGodot/Characters/Villager/Anims/Emotes/A_KG_Emote_%s.A_KG_Emote_%s"), Clip, Clip));
		return Seq ? Seq : LoadCached<UAnimSequence>(FString::Printf(
			TEXT("/Game/KillGodot/Characters/Villager/Anims/%s.%s"), Fallback, Fallback));
	}

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - (1.0f - T) * (1.0f - T) * (1.0f - T);
	}

	bool LineIsOut(EKGFishPhase P)
	{
		return P == EKGFishPhase::Flight || P == EKGFishPhase::Waiting || P == EKGFishPhase::Bite || P == EKGFishPhase::Fight;
	}

	FVector Bezier(const FVector& A, const FVector& C, const FVector& B, float T)
	{
		const float U = 1.0f - T;
		return A * (U * U) + C * (2.0f * U * T) + B * (T * T);
	}
}

// =================================================================================================================
// Setup
// =================================================================================================================
UKGFishingComponent::UKGFishingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the camera update: the first-person rod tip and the view are final for this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

void UKGFishingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Everyone;
	Everyone.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGFishingComponent, bRodOut, Everyone);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGFishingComponent, Line, Everyone);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGFishingComponent, Catch, Everyone);
	FDoRepLifetimeParams Owner;
	Owner.bIsPushBased = true;
	Owner.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGFishingComponent, Fight, Owner);
}

UKGFishingComponent* UKGFishingComponent::FindFor(const AActor* Pawn)
{
	return Pawn ? Pawn->FindComponentByClass<UKGFishingComponent>() : nullptr;
}

UKGFishingComponent* UKGFishingComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? FindFor(PlayerState->GetPawn()) : nullptr;
}

void UKGFishingComponent::BeginPlay()
{
	Super::BeginPlay();
	RodMesh = KGFishingComp::LoadCached<UStaticMesh>(KGFishingComp::RodPath);
	CosmeticRng = FKGRng(FCrc::StrCrc32(*GetOwner()->GetName()), 13u);
	if (GetNetMode() != NM_DedicatedServer)
	{
		CreateVisuals();
	}
}

void UKGFishingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetLocalPC())
	{
		UpdateInputContexts(false, false);
	}
	Super::EndPlay(EndPlayReason);
}

AKGCharacter* UKGFishingComponent::GetCharacter() const
{
	return Cast<AKGCharacter>(GetOwner());
}

bool UKGFishingComponent::IsOwnerView() const
{
	const AKGCharacter* C = GetCharacter();
	return C && C->IsLocallyControlled() && C->IsPlayerControlled();
}

APlayerController* UKGFishingComponent::GetLocalPC() const
{
	const AKGCharacter* C = GetCharacter();
	return IsOwnerView() ? Cast<APlayerController>(C->GetController()) : nullptr;
}

bool UKGFishingComponent::HasRodItem() const
{
	const AKGCharacter* C = GetCharacter();
	const UKGInventoryComponent* Pockets = C ? UKGInventoryComponent::FindForPawn(C) : nullptr;
	return Pockets && Pockets->Has(FKGItemIds::FishingRod);
}

FVector UKGFishingComponent::CastOrigin(const FVector& ViewStart, const FVector& ViewDir)
{
	const FVector Flat = FVector(ViewDir.X, ViewDir.Y, 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	return ViewStart + Flat * 45.0f + FVector(0.0f, 0.0f, 12.0f);
}

bool UKGFishingComponent::IsDebugOverlayOn()
{
	return KGFishingComp::CVarDebug.GetValueOnGameThread() != 0;
}

// =================================================================================================================
// Queries
// =================================================================================================================
bool UKGFishingComponent::IsRodShown() const
{
	if (IsOwnerView() && PredictedRod.IsSet() && GetWorld() && GetWorld()->GetRealTimeSeconds() < PredictedRodUntil)
	{
		return PredictedRod.GetValue();
	}
	return bRodOut;
}

EKGFishPhase UKGFishingComponent::GetShownPhase() const
{
	if (!IsRodShown())
	{
		return EKGFishPhase::Stowed;
	}
	if (IsOwnerView() && GetWorld())
	{
		const double Now = GetWorld()->GetRealTimeSeconds();
		if (bCharging)
		{
			return EKGFishPhase::Charging;
		}
		if (bPredictCast)
		{
			return EKGFishPhase::Flight;
		}
		if (Now < PredictReelInUntil)
		{
			return EKGFishPhase::Ready;
		}
		if (Now < PredictHookUntil && Line.Phase == EKGFishPhase::Bite)
		{
			return EKGFishPhase::Fight;
		}
		if (Line.Phase == EKGFishPhase::Stowed || Line.Phase == EKGFishPhase::Charging)
		{
			return EKGFishPhase::Ready;   // rod predicted out / the server still shows our last wind-up
		}
	}
	return Line.Phase == EKGFishPhase::Stowed ? EKGFishPhase::Ready : Line.Phase;
}

UAnimSequence* UKGFishingComponent::GetArmsLoop() const
{
	switch (GetShownPhase())
	{
	case EKGFishPhase::Charging:
		return KGFishingComp::FPClip(TEXT("rod_windup"));
	case EKGFishPhase::Fight:
		return KGFishingComp::FPClip(TEXT("rod_reel"));
	default:
		return KGFishingComp::FPClip(TEXT("rod_idle"));
	}
}

UAnimSequence* UKGFishingComponent::ConsumeArmsOneShot()
{
	UAnimSequence* Shot = PendingOneShot;
	PendingOneShot = nullptr;
	return Shot;
}

float UKGFishingComponent::GetChargePower() const
{
	return bCharging ? FKGFishingRules::ChargePower(ChargeSeconds) : 0.0f;
}

EKGFishNotice UKGFishingComponent::GetNotice(int32& OutA, int32& OutB, double& OutAt) const
{
	OutA = NoticeA;
	OutB = NoticeB;
	OutAt = NoticeAt;
	return Notice;
}

float UKGFishingComponent::GetWaitSeconds() const
{
	if (GetShownPhase() != EKGFishPhase::Waiting || !GetWorld())
	{
		return -1.0f;
	}
	return FMath::Max(0.0f, static_cast<float>(GetWorld()->GetRealTimeSeconds() - CastSeenAt) - Line.FlightTime);
}

// =================================================================================================================
// Tick
// =================================================================================================================
void UKGFishingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
void UKGFishingComponent::EnsureRng()
{
	if (bRngSeeded)
	{
		return;
	}
	const AKGGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AKGGameMode>() : nullptr;
	const uint64 MatchSeed = GM ? static_cast<uint64>(GM->GetMatchSeed()) : 0x5EEDu;
	const AKGCharacter* C = GetCharacter();
	const AKGPlayerState* PS = C ? C->GetPlayerState<AKGPlayerState>() : nullptr;
	const FString Key = PS && !PS->Puid.IsEmpty() ? PS->Puid : (PS ? PS->GetPlayerName() : GetOwner()->GetName());
	const uint32 Hash = FCrc::StrCrc32(*Key);
	Rng = FKGRng(MatchSeed ^ (static_cast<uint64>(Hash) << 17), 0xF15Au ^ Hash);
	bRngSeeded = true;
}

void UKGFishingComponent::ServerSetPhase(EKGFishPhase Phase, float HoldSeconds)
{
	Line.Phase = Phase;
	if (Phase != EKGFishPhase::Charging)
	{
		Line.ChargeQ = 0;
	}
	if (HoldSeconds > 0.0f)
	{
		PhaseClock.Start(HoldSeconds);
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, Line, this);
}

void UKGFishingComponent::ServerNotify(EKGFishNotice InNotice, int32 A, int32 B)
{
	ClientNotice(InNotice, A, B);
}

void UKGFishingComponent::ClientNotice_Implementation(EKGFishNotice InNotice, int32 A, int32 B)
{
	Notice = InNotice;
	NoticeA = A;
	NoticeB = B;
	NoticeAt = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

void UKGFishingComponent::TickServer(float DeltaTime)
{
	AKGCharacter* C = GetCharacter();
	if (!C)
	{
		return;
	}
	if (!bHealthBound && C->GetHealth())
	{
		C->GetHealth()->OnHealthChanged.AddDynamic(this, &UKGFishingComponent::HandleHealthChanged);
		bHealthBound = true;
	}
	CastCooldown = FMath::Max(0.0f, CastCooldown - DeltaTime);
	if (!bRodOut)
	{
		return;
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	const EKGPhase MatchPhase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	if (C->IsDead() || !FKGFishingRules::CanFishInPhase(MatchPhase) || C->bHoldingAssassinBlade || C->bHoldingObject ||
	    !HasRodItem())
	{
		const bool bTell = !C->IsDead();
		ServerStow();
		if (bTell)
		{
			ServerNotify(EKGFishNotice::NotNow);
		}
		return;
	}
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (Move && Move->IsSwimming() && KGFishingComp::LineIsOut(Line.Phase))
	{
		ServerSetPhase(EKGFishPhase::Ready);
		ServerNotify(EKGFishNotice::NotNow);
	}

	switch (Line.Phase)
	{
	case EKGFishPhase::Flight:
		if (PhaseClock.Advance(DeltaTime))
		{
			ServerLand();
		}
		break;
	case EKGFishPhase::Waiting:
	{
		WaitElapsed += DeltaTime;
		while (Schedule.Nibbles.IsValidIndex(NextNibble) && WaitElapsed >= Schedule.Nibbles[NextNibble])
		{
			++NextNibble;
			++Line.NibbleSerial;
			MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, Line, this);
		}
		if ((bForceBite && WaitElapsed >= 0.4f) || WaitElapsed >= Schedule.BiteAt)
		{
			ServerStartBite();
		}
		else if (FVector::Dist2D(C->GetActorLocation(), Line.Landing) > FKGFishingRules::MaxLineLength)
		{
			ServerSetPhase(EKGFishPhase::Ready);
			ServerNotify(EKGFishNotice::LineTooFar);
		}
		break;
	}
	case EKGFishPhase::Bite:
		if (PhaseClock.Advance(DeltaTime))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_MISSED %s"), *GetOwner()->GetName());
			ServerNotify(EKGFishNotice::Missed);
			ServerStartWaiting(1.5f);
		}
		break;
	case EKGFishPhase::Fight:
		ServerSim.Advance(DeltaTime, ServerInput);
		PublishAccum += DeltaTime;
		if (ServerSim.IsOver())
		{
			ServerPublishFight();
			ServerEndFight(ServerSim.Result);
		}
		else if (PublishAccum >= 1.0f / 15.0f)
		{
			ServerPublishFight();
		}
		break;
	case EKGFishPhase::Landed:
	case EKGFishPhase::Snapped:
	case EKGFishPhase::Escaped:
	case EKGFishPhase::Snagged:
	case EKGFishPhase::KoiTax:
		if (PhaseClock.Advance(DeltaTime))
		{
			ServerSetPhase(EKGFishPhase::Ready);
		}
		break;
	default:
		break;
	}
}

bool UKGFishingComponent::ServerTrySetRod(bool bOut, EKGFishNotice* OutWhy)
{
	AKGCharacter* C = GetCharacter();
	if (!bOut)
	{
		ServerStow();
		return true;
	}
	auto Refuse = [OutWhy](EKGFishNotice Why)
	{
		if (OutWhy)
		{
			*OutWhy = Why;
		}
		return false;
	};
	if (!C || C->IsDead())
	{
		return Refuse(EKGFishNotice::NotNow);
	}
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (GS && !FKGFishingRules::CanFishInPhase(GS->GetPhase()))
	{
		return Refuse(EKGFishNotice::NotNow);
	}
	if (C->bHoldingAssassinBlade || C->bHoldingObject)
	{
		return Refuse(EKGFishNotice::Busy);
	}
	if (!HasRodItem())
	{
		return Refuse(EKGFishNotice::NoRod);
	}
	if (bRodOut)
	{
		return true;
	}
	bRodOut = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, bRodOut, this);
	ServerSetPhase(EKGFishPhase::Ready);
	if (UKGEmoteComponent* Emote = C->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	EnsureRng();
	return true;
}

void UKGFishingComponent::ServerStow()
{
	bRodOut = false;
	bForceBite = false;
	ForcedSpecies = NAME_None;
	ServerInput = FKGReelInput();
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, bRodOut, this);
	ServerSetPhase(EKGFishPhase::Stowed);
}

void UKGFishingComponent::ServerSetRod_Implementation(bool bOut)
{
	EKGFishNotice Why = EKGFishNotice::None;
	if (!ServerTrySetRod(bOut, &Why) && Why != EKGFishNotice::None)
	{
		ServerNotify(Why);
	}
}

void UKGFishingComponent::ServerSetCharge_Implementation(uint8 ChargeQ)
{
	if (!bRodOut || (Line.Phase != EKGFishPhase::Ready && Line.Phase != EKGFishPhase::Charging))
	{
		return;
	}
	ServerSetPhase(ChargeQ > 0 ? EKGFishPhase::Charging : EKGFishPhase::Ready);
	Line.ChargeQ = ChargeQ;
}

void UKGFishingComponent::ServerCast_Implementation(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir, uint8 PowerQ)
{
	ServerTryCast(ViewStart, ViewDir, PowerQ / 255.0f);
}

bool UKGFishingComponent::ServerTryCast(const FVector& ViewStart, const FVector& ViewDir, float Power)
{
	AKGCharacter* C = GetCharacter();
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	if (!C || C->IsDead() || !bRodOut || CastCooldown > 0.0f ||
	    (Line.Phase != EKGFishPhase::Ready && Line.Phase != EKGFishPhase::Charging) ||
	    (GS && !FKGFishingRules::CanFishInPhase(GS->GetPhase())) ||
	    (C->GetCharacterMovement() && C->GetCharacterMovement()->IsSwimming()))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_CAST_REFUSED %s phase=%d rod=%d"), *GetOwner()->GetName(),
		       static_cast<int32>(Line.Phase), bRodOut ? 1 : 0);
		return false;
	}
	// The claimed view must be near where we think the head is (same rule as melee).
	const FVector Head = C->GetFirstPersonCamera()->GetComponentLocation();
	const FVector Start = FVector::DistSquared(ViewStart, Head) <= FMath::Square(150.0f) ? ViewStart : Head;
	const FVector Dir = ViewDir.GetSafeNormal(UE_SMALL_NUMBER, C->GetActorForwardVector());
	const float Clamped = FMath::Clamp(Power, 0.0f, 1.0f);
	const FVector Origin = CastOrigin(Start, Dir);
	const FVector Velocity = FKGFishingRules::CastVelocity(Dir, Clamped);
	const FKGCastResult Result = FKGCastSim::Simulate(GetWorld(), Origin, Velocity, GetWorld()->GetTimeSeconds(), C);
	Line.Origin = Origin;
	Line.Velocity = Velocity;
	Line.Landing = Result.Landing;
	Line.FlightTime = FMath::Max(0.1f, Result.FlightTime);
	Line.Water = Result.Water;
	++Line.CastSerial;
	ServerSetPhase(EKGFishPhase::Flight, Line.FlightTime);
	CastCooldown = 0.5f;
	if (UKGEmoteComponent* Emote = C->GetEmote())
	{
		Emote->ServerStop(EKGEmoteStop::Requested);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_CAST %s power=%.2f landing=%s water=%s flight=%.2fs dist=%.0fcm"),
	       *GetOwner()->GetName(), Clamped, *Result.Landing.ToCompactString(),
	       *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(Result.Water)), Line.FlightTime,
	       FVector::Dist2D(Origin, Result.Landing));
	return true;
}

void UKGFishingComponent::ServerLand()
{
	AKGCharacter* C = GetCharacter();
	switch (Line.Water)
	{
	case EKGFishWater::Sea:
	case EKGFishWater::Basin:
	case EKGFishWater::Brook:
		ServerStartWaiting(0.0f);
		return;
	case EKGFishWater::KoiPond:
	{
		// Sacrilege: a koi slaps you and the garden keeps a fine.
		UKGInventoryComponent* Pockets = C ? UKGInventoryComponent::FindForPawn(C) : nullptr;
		const int32 Taken = Pockets ? Pockets->RemoveItem(FKGItemIds::Coin, 3) : 0;
		if (C)
		{
			C->LaunchCharacter(FVector(0.0f, 0.0f, 260.0f) - C->GetActorForwardVector() * 160.0f, true, true);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_KOITAX %s coins=%d"), *GetOwner()->GetName(), Taken);
		ServerResult(EKGFishPhase::KoiTax, NAME_None, 0, Taken, -1, false);
		return;
	}
	default:
		ServerResult(EKGFishPhase::Snagged, NAME_None, 0, 0, -1, false);
		return;
	}
}

void UKGFishingComponent::ServerStartWaiting(float ExtraDelay)
{
	EnsureRng();
	TArray<FKGSchoolSample> Schools;
	FKGFishingWater::GatherSchools(GetWorld(), Line.Landing, 1500.0f, Schools);
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	const EKGFishTime Time = FKGFishingRules::TimeFor(GS ? GS->GetPhase() : EKGPhase::Day);
	const float Rate = FKGFishingRules::BiteRate(Line.Water, Time, FKGFishingRules::SchoolBonus(Schools));
	Schedule = FKGFishingRules::RollSchedule(Rng, Rate);
	if (ExtraDelay > 0.0f)
	{
		Schedule.BiteAt += ExtraDelay;
		for (float& N : Schedule.Nibbles)
		{
			N += ExtraDelay;
		}
	}
	WaitElapsed = 0.0f;
	NextNibble = 0;
	ServerSetPhase(EKGFishPhase::Waiting);
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_WAIT %s water=%s schools=%d rate=%.3f bite_at=%.1fs nibbles=%d"),
	       *GetOwner()->GetName(), *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(Line.Water)),
	       Schools.Num(), Rate, Schedule.BiteAt, Schedule.Nibbles.Num());
}

void UKGFishingComponent::ServerForceBite(FName Species)
{
	ForcedSpecies = Species;
	bForceBite = true;
	if (Line.Phase == EKGFishPhase::Waiting)
	{
		WaitElapsed = FMath::Max(WaitElapsed, 0.4f);
	}
	else if (Line.Phase == EKGFishPhase::Ready && bRodOut)
	{
		const AKGCharacter* C = GetCharacter();
		if (C)
		{
			ServerTryCast(C->GetFirstPersonCamera()->GetComponentLocation(), C->GetControlRotation().Vector(), 0.65f);
		}
	}
}

void UKGFishingComponent::ServerStartBite()
{
	EnsureRng();
	TArray<FKGSchoolSample> Schools;
	FKGFishingWater::GatherSchools(GetWorld(), Line.Landing, 1500.0f, Schools);
	const AKGGameState* GS = GetWorld()->GetGameState<AKGGameState>();
	PendingBite = FKGFishingRules::RollBite(Rng, Line.Water, FKGFishingRules::TimeFor(GS ? GS->GetPhase() : EKGPhase::Day), Schools);
	if (!ForcedSpecies.IsNone())
	{
		const int32 Index = FKGFishingRules::IndexOf(ForcedSpecies);
		const FKGFishSpecies* ByItem = FKGFishingRules::FindByItem(ForcedSpecies);
		const int32 Pick = Index != INDEX_NONE ? Index : (ByItem ? FKGFishingRules::IndexOf(ByItem->Id) : INDEX_NONE);
		if (Pick != INDEX_NONE && !FKGFishingRules::Species()[Pick].bSacred)
		{
			PendingBite.Species = Pick;
			PendingBite.ItemId = FKGFishingRules::Species()[Pick].ItemId;
			PendingBite.Grams = FKGFishingRules::RollGrams(Rng, FKGFishingRules::Species()[Pick]);
		}
		else if (UKGItemCatalog::IsValidItem(ForcedSpecies))
		{
			PendingBite = FKGBiteRoll();
			PendingBite.ItemId = ForcedSpecies;   // junk / treasure on demand
		}
	}
	bForceBite = false;
	ForcedSpecies = NAME_None;
	if (PendingBite.ItemId.IsNone())
	{
		ServerStartWaiting(2.0f);
		return;
	}
	const FKGFishSpecies* S = PendingBite.IsJunk() ? nullptr : &FKGFishingRules::Species()[PendingBite.Species];
	const APlayerState* PS = GetCharacter() ? GetCharacter()->GetPlayerState() : nullptr;
	const float Ping = PS ? PS->GetPingInMilliseconds() / 1000.0f : 0.0f;
	++Line.BiteSerial;
	ServerSetPhase(EKGFishPhase::Bite, FKGFishingRules::ServerHookWindow(S ? S->HookWindow : 0.9f, Ping));
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_BITE %s item=%s grams=%d window=%.2fs"), *GetOwner()->GetName(),
	       *PendingBite.ItemId.ToString(), PendingBite.Grams, PhaseClock.RemainingSeconds);
}

void UKGFishingComponent::ServerHook_Implementation()
{
	if (Line.Phase == EKGFishPhase::Bite)
	{
		ServerStartFight();
	}
	else if (Line.Phase == EKGFishPhase::Waiting)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_TOOEARLY %s"), *GetOwner()->GetName());
		ServerNotify(EKGFishNotice::TooEarly);
		ServerStartWaiting(2.5f);   // spooked: it swims off and comes back later
	}
}

void UKGFishingComponent::ServerForceHook()
{
	ServerHook_Implementation();
}

void UKGFishingComponent::ServerStartFight()
{
	EnsureRng();
	const AKGCharacter* C = GetCharacter();
	const FKGFishSpecies* S = PendingBite.IsJunk() ? nullptr : &FKGFishingRules::Species()[PendingBite.Species];
	const float Alpha = S ? FKGFishingRules::WeightAlpha(*S, PendingBite.Grams) : 0.0f;
	const float Distance = C ? FVector::Dist2D(C->GetActorLocation(), Line.Landing) / 100.0f : 10.0f;
	const uint64 Seed = (static_cast<uint64>(Rng.NextUInt32()) << 32) | Rng.NextUInt32();
	ServerSim.Init(S, Alpha, Distance, Seed);
	ServerInput = FKGReelInput();
	++Fight.Serial;
	Fight.Species = static_cast<int8>(PendingBite.Species);
	Fight.ItemId = PendingBite.ItemId;
	Fight.Grams = PendingBite.Grams;
	ServerSetPhase(EKGFishPhase::Fight);
	ServerPublishFight();
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_HOOK %s item=%s grams=%d distance=%.1fm pull=%.2f"), *GetOwner()->GetName(),
	       *Fight.ItemId.ToString(), Fight.Grams, ServerSim.Distance, ServerSim.Pull);
}

void UKGFishingComponent::ServerPublishFight()
{
	PublishAccum = 0.0f;
	Fight.Sim = ServerSim;
	++Fight.NetTick;
	Line.TensionQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(ServerSim.Tension * 180.0f), 0, 255));
	Line.FishXQ = static_cast<int8>(FMath::Clamp(FMath::RoundToInt(ServerSim.FishX * 127.0f), -127, 127));
	Line.DistanceCm = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(ServerSim.Distance * 100.0f), 0, 65535));
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, Fight, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, Line, this);
}

void UKGFishingComponent::ServerReelInput_Implementation(bool bReel, int8 SteerQ)
{
	ServerInput.bReel = bReel;
	ServerInput.Steer = FMath::Clamp(SteerQ / 127.0f, -1.0f, 1.0f);
}

void UKGFishingComponent::ServerReelIn_Implementation()
{
	if (Line.Phase == EKGFishPhase::Fight)
	{
		ServerSim.Result = EKGReelResult::Escaped;
		ServerPublishFight();
		ServerEndFight(EKGReelResult::Escaped);
	}
	else if (KGFishingComp::LineIsOut(Line.Phase) || Line.Phase == EKGFishPhase::Snagged)
	{
		ServerSetPhase(EKGFishPhase::Ready);
	}
}

bool UKGFishingComponent::ServerForceLand()
{
	if (Line.Phase == EKGFishPhase::Bite)
	{
		ServerStartFight();
	}
	if (Line.Phase != EKGFishPhase::Fight)
	{
		return false;
	}
	ServerSim.Result = EKGReelResult::Landed;
	ServerSim.Distance = FKGReelSim::LandDistance;
	ServerPublishFight();
	ServerEndFight(EKGReelResult::Landed);
	return true;
}

void UKGFishingComponent::HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (GetOwner() && GetOwner()->HasAuthority() && Delta < 0.0f && Line.Phase == EKGFishPhase::Fight)
	{
		// A hit while reeling: you flinch and the fish is gone (the vulnerability window).
		ServerSim.Result = EKGReelResult::Escaped;
		ServerPublishFight();
		ServerEndFight(EKGReelResult::Escaped);
		ServerNotify(EKGFishNotice::Busy, 1, 0);
	}
}

void UKGFishingComponent::ServerEndFight(EKGReelResult Result)
{
	AKGCharacter* C = GetCharacter();
	if (Result != EKGReelResult::Landed)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_RESULT %s result=%s item=%s grams=%d t=%.1fs"), *GetOwner()->GetName(),
		       Result == EKGReelResult::Snapped ? TEXT("Snapped") : TEXT("Escaped"), *Fight.ItemId.ToString(), Fight.Grams,
		       ServerSim.Elapsed);
		ServerResult(Result == EKGReelResult::Snapped ? EKGFishPhase::Snapped : EKGFishPhase::Escaped, Fight.ItemId, Fight.Grams,
		             0, -1, false);
		return;
	}
	EnsureRng();
	UKGInventoryComponent* Pockets = C ? UKGInventoryComponent::FindForPawn(C) : nullptr;
	const FVector Feet = C ? C->GetActorLocation() + C->GetActorForwardVector() * 70.0f : FVector::ZeroVector;
	int32 Coins = 0;
	int8 Message = -1;
	bool bDropped = false;
	if (Fight.ItemId == FKGItemIds::CoinPouch)
	{
		// A soggy pouch: straight into the purse.
		Coins = Rng.RandRange(8, 22);
		const int32 Added = Pockets ? Pockets->AddItem(FKGItemIds::Coin, Coins) : 0;
		if (Added < Coins)
		{
			AKGPickup::SpawnPickup(this, FKGItemIds::Coin, Coins - Added, Feet);
			bDropped = true;
		}
	}
	else
	{
		if (Fight.ItemId == FKGItemIds::MessageBottle)
		{
			Message = static_cast<int8>(Rng.RandRange(0, FKGFishingRules::NumBottleMessages() - 1));
		}
		const int32 Added = Pockets ? Pockets->AddItem(Fight.ItemId, 1, Fight.Grams) : 0;
		if (Added <= 0)
		{
			AKGPickup::SpawnPickup(this, Fight.ItemId, 1, Feet);
			bDropped = true;
			ServerNotify(EKGFishNotice::PocketsFull);
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_RESULT %s result=Landed item=%s grams=%d coins=%d message=%d dropped=%d t=%.1fs"),
	       *GetOwner()->GetName(), *Fight.ItemId.ToString(), Fight.Grams, Coins, Message, bDropped ? 1 : 0, ServerSim.Elapsed);
	ServerResult(EKGFishPhase::Landed, Fight.ItemId, Fight.Grams, Coins, Message, bDropped);
}

void UKGFishingComponent::ServerResult(EKGFishPhase Result, FName ItemId, int32 Grams, int32 Coins, int8 Message, bool bDropped)
{
	++Catch.Serial;
	Catch.Result = Result;
	Catch.ItemId = ItemId;
	Catch.Grams = Grams;
	Catch.Coins = Coins;
	Catch.Message = Message;
	Catch.bDropped = bDropped;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGFishingComponent, Catch, this);
	const float Hold = Result == EKGFishPhase::Landed ? 3.0f : Result == EKGFishPhase::KoiTax ? 2.5f
	                 : Result == EKGFishPhase::Snagged ? 1.4f : 2.0f;
	ServerSetPhase(Result, Hold);
}

// =================================================================================================================
// Owner (input, prediction, HUD state)
// =================================================================================================================
void UKGFishingComponent::RequestToggleRod()
{
	RequestSetRod(!IsRodShown());
}

void UKGFishingComponent::RequestSetRod(bool bOut)
{
	if (bOut && !HasRodItem())
	{
		ClientNotice_Implementation(EKGFishNotice::NoRod, 0, 0);
		return;
	}
	if (!bOut)
	{
		CancelCharge();
		bPredictCast = false;
	}
	PredictedRod = bOut;
	PredictedRodUntil = GetWorld()->GetRealTimeSeconds() + 1.0;
	ServerSetRod(bOut);
}

void UKGFishingComponent::BeginCharge()
{
	if (GetShownPhase() != EKGFishPhase::Ready)
	{
		return;
	}
	bCharging = true;
	ChargeSeconds = 0.0f;
	LastChargeQSent = 0;
	PendingOneShot = KGFishingComp::FPClip(TEXT("rod_windup_in"));   // then the rod_windup loop holds it back
}

void UKGFishingComponent::CancelCharge()
{
	if (bCharging)
	{
		bCharging = false;
		ServerSetCharge(0);
	}
}

void UKGFishingComponent::ReleaseCast()
{
	if (!bCharging)
	{
		return;
	}
	const float Power = FKGFishingRules::ChargePower(ChargeSeconds);
	bCharging = false;
	CastWithPower(Power);
}

void UKGFishingComponent::CastWithPower(float Power)
{
	AKGCharacter* C = GetCharacter();
	UWorld* World = GetWorld();
	if (!C || !World || !IsRodShown())
	{
		return;
	}
	const uint8 PowerQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Power * 255.0f), 0, 255));
	const FVector ViewStart = C->GetFirstPersonCamera()->GetComponentLocation();
	const FVector ViewDir = C->GetControlRotation().Vector();
	// Predict the arc exactly like the server will (same origin rule, same quantised power).
	PredOrigin = CastOrigin(ViewStart, ViewDir);
	PredVelocity = FKGFishingRules::CastVelocity(ViewDir, PowerQ / 255.0f);
	PredResult = FKGCastSim::Simulate(World, PredOrigin, PredVelocity, World->GetTimeSeconds(), C);
	bPredictCast = true;
	PredictBaseSerial = Line.CastSerial;
	PredCastAt = World->GetRealTimeSeconds();
	PredictCastUntil = PredCastAt + FMath::Max(PredResult.FlightTime, 0.1f) + 1.0;
	PendingOneShot = KGFishingComp::FPClip(TEXT("rod_cast"));
	KGAudio::At(this, TEXT("S_Fish_Cast"), ViewStart, 0.8f);
	ServerCast(ViewStart, ViewDir, PowerQ);
}

void UKGFishingComponent::RequestHook()
{
	const EKGFishPhase Phase = GetShownPhase();
	if (Phase != EKGFishPhase::Waiting && Phase != EKGFishPhase::Bite)
	{
		return;
	}
	PendingOneShot = KGFishingComp::FPClip(TEXT("rod_hookset"));
	if (Line.Phase == EKGFishPhase::Bite)
	{
		PredictHookUntil = GetWorld()->GetRealTimeSeconds() + 0.6;
		if (const AKGCharacter* C = GetCharacter())
		{
			KGAudio::At(this, TEXT("S_Fish_Hook"), C->GetActorLocation(), 0.9f);
		}
	}
	ServerHook();
}

void UKGFishingComponent::RequestReelIn()
{
	const EKGFishPhase Phase = GetShownPhase();
	if (!KGFishingComp::LineIsOut(Phase) && Phase != EKGFishPhase::Snagged)
	{
		return;
	}
	PredictReelInUntil = GetWorld()->GetRealTimeSeconds() + 0.5;
	bPredictCast = false;
	ServerReelIn();
}

void UKGFishingComponent::SetScriptedReelInput(const FKGReelInput& Input, float Seconds)
{
	ScriptedInput = Input;
	ScriptedSeconds = Seconds;
}

FKGReelInput UKGFishingComponent::ReadKeys(APlayerController* PC) const
{
	FKGReelInput In;
	In.bReel = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger);
	In.Steer = (PC->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
	return In;
}

void UKGFishingComponent::SendReelInput(const FKGReelInput& Input, bool bForce)
{
	const bool bChanged = Input.bReel != LastSentInput.bReel || !FMath::IsNearlyEqual(Input.Steer, LastSentInput.Steer, 0.05f);
	if (bForce || bChanged || InputSendAccum >= 0.05f)
	{
		ServerReelInput(Input.bReel, static_cast<int8>(FMath::Clamp(FMath::RoundToInt(Input.Steer * 127.0f), -127, 127)));
		LastSentInput = Input;
		InputSendAccum = 0.0f;
	}
}

void UKGFishingComponent::UpdateInputContexts(bool bRod, bool bFight)
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
		// Keys mapped here (higher priority than IMC_KG_Default) are consumed: no punch/shove while the rod is out,
		// no walking while a fish fights. The fishing code polls the keys itself.
		ConsumeAction = NewObject<UInputAction>(this, TEXT("IA_KG_FishConsume"));
		ConsumeAction->ValueType = EInputActionValueType::Boolean;
		ConsumeAction->bConsumeInput = true;
		RodContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KG_FishRod"));
		RodContext->MapKey(ConsumeAction, EKeys::LeftMouseButton);
		RodContext->MapKey(ConsumeAction, EKeys::RightMouseButton);
		FightContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KG_FishFight"));
		for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::SpaceBar, EKeys::LeftShift, EKeys::C,
		                        EKeys::LeftControl})
		{
			FightContext->MapKey(ConsumeAction, Key);
		}
	}
	if (bRod != bRodContextOn)
	{
		bRodContextOn = bRod;
		if (bRod)
		{
			Input->AddMappingContext(RodContext, 10);
		}
		else
		{
			Input->RemoveMappingContext(RodContext);
		}
	}
	if (bFight != bFightContextOn)
	{
		bFightContextOn = bFight;
		if (bFight)
		{
			Input->AddMappingContext(FightContext, 11);
		}
		else
		{
			Input->RemoveMappingContext(FightContext);
		}
	}
}

void UKGFishingComponent::OnCatchArrived()
{
	CatchShownAt = GetWorld()->GetRealTimeSeconds();
	bNewBest = false;
	PreviousBest = 0;
	const FKGFishSpecies* S = FKGFishingRules::FindByItem(Catch.ItemId);
	const AKGCharacter* C = GetCharacter();
	const FVector At = C ? C->GetActorLocation() : FVector::ZeroVector;
	switch (Catch.Result)
	{
	case EKGFishPhase::Landed:
		if (S && Catch.Grams > 0)
		{
			if (UKGFishingJournal* Journal = UKGFishingJournal::Get())
			{
				PreviousBest = Journal->Record(S->Id, Catch.Grams);
				bNewBest = Catch.Grams > PreviousBest;
			}
		}
		KGAudio::UI(this, TEXT("S_Fish_Fanfare"), 0.8f);
		break;
	case EKGFishPhase::KoiTax:
		KGAudio::UI(this, TEXT("S_Fish_Snap"), 0.5f);
		break;
	default:
		break;
	}
}

void UKGFishingComponent::TickOwner(float DeltaTime)
{
	APlayerController* PC = GetLocalPC();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	if (PredictedRod.IsSet() && (Now >= PredictedRodUntil || PredictedRod.GetValue() == bRodOut))
	{
		PredictedRod.Reset();
	}
	if (bPredictCast && (Line.CastSerial != PredictBaseSerial || Now > PredictCastUntil))
	{
		bPredictCast = false;
	}
	// The catch result (polled: works the same on the listen host and on clients).
	if (Catch.Serial != ShownCatchSerial)
	{
		if (bCatchPrimed)
		{
			OnCatchArrived();
		}
		ShownCatchSerial = Catch.Serial;
	}
	bCatchPrimed = true;

	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	const bool bBlocked = FKGChatUI::IsTyping(PC) || FKGChatUI::IsWheelOpen(PC) || FKGInventoryUI::IsOpen(PC) ||
	                      (KGPC && KGPC->IsPauseMenuOpen()) || PC->bShowMouseCursor;
	const bool bRod = IsRodShown();
	EKGFishPhase Phase = GetShownPhase();
	if (!bBlocked)
	{
		if (PC->WasInputKeyJustPressed(EKeys::H))
		{
			RequestToggleRod();
		}
		else if (bRod)
		{
			if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				if (Phase == EKGFishPhase::Ready)
				{
					BeginCharge();
				}
				else if (Phase == EKGFishPhase::Waiting || Phase == EKGFishPhase::Bite)
				{
					RequestHook();
				}
			}
			if (bCharging && PC->WasInputKeyJustReleased(EKeys::LeftMouseButton))
			{
				ReleaseCast();
			}
			if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
			{
				if (bCharging)
				{
					CancelCharge();
				}
				else
				{
					RequestReelIn();
				}
			}
		}
	}
	else if (bCharging)
	{
		CancelCharge();
	}
	if (bCharging)
	{
		ChargeSeconds += DeltaTime;
		const uint8 Q = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(GetChargePower() * 255.0f), 1, 255));
		if (FMath::Abs(static_cast<int32>(Q) - static_cast<int32>(LastChargeQSent)) > 20)
		{
			ServerSetCharge(Q);
			LastChargeQSent = Q;
		}
	}
	Phase = GetShownPhase();

	// Fight prediction: start from the server's state, run it locally with our keys, reconcile on every update.
	if (Line.Phase == EKGFishPhase::Fight && Fight.Serial != LocalFightSerial)
	{
		LocalSim = Fight.Sim;
		bLocalSim = true;
		LocalFightSerial = Fight.Serial;
		LocalNetTick = Fight.NetTick;
	}
	else if (bLocalSim && Fight.NetTick != LocalNetTick)
	{
		LocalSim.Reconcile(Fight.Sim, 0.35f);
		LocalNetTick = Fight.NetTick;
	}
	if (Line.Phase != EKGFishPhase::Fight && Phase != EKGFishPhase::Fight)
	{
		bLocalSim = false;
	}
	FKGReelInput In = bBlocked ? FKGReelInput() : ReadKeys(PC);
	if (ScriptedSeconds > 0.0f)
	{
		In = ScriptedInput;
		ScriptedSeconds -= DeltaTime;
	}
	InputSendAccum += DeltaTime;
	if (Line.Phase == EKGFishPhase::Fight && bLocalSim)
	{
		if (!LocalSim.IsOver())
		{
			LocalSim.Advance(DeltaTime, In);
		}
		SendReelInput(In, false);
	}

	UpdateInputContexts(bRod, Phase == EKGFishPhase::Fight);

	// Attention: the fight narrows the view (and the HUD darkens the edges) - fishing is a vulnerable moment.
	const float FocusGoal = Phase == EKGFishPhase::Fight ? 1.0f : Phase == EKGFishPhase::Bite ? 0.5f
	                      : Phase == EKGFishPhase::Waiting ? 0.2f : 0.0f;
	Focus = FMath::FInterpTo(Focus, FocusGoal, DeltaTime, FocusGoal > Focus ? 3.0f : 5.0f);
}

void UKGFishingComponent::ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView)
{
	if (!IsOwnerView())
	{
		return;
	}
	InOutView.FOV *= 1.0f - FMath::Clamp(KGFishingComp::CVarFocusFov.GetValueOnGameThread(), 0.0f, 0.4f) * Focus;
	NudgeTime += DeltaTime;
	if (NudgeTime < 0.7f)
	{
		// Controller-rumble style kick when the fish takes the bait.
		const float Amp = FMath::Exp(-NudgeTime * 7.0f);
		InOutView.Rotation.Pitch += -1.4f * Amp * FMath::Sin(NudgeTime * 38.0f);
		InOutView.Rotation.Yaw += 0.7f * Amp * FMath::Sin(NudgeTime * 31.0f);
	}
	if (bLocalSim && LocalSim.Tension > 0.9f)
	{
		const float Shake = FMath::Clamp((LocalSim.Tension - 0.9f) / 0.3f, 0.0f, 1.0f) * 0.22f;
		const float T = static_cast<float>(GetWorld()->GetRealTimeSeconds());
		InOutView.Rotation.Pitch += Shake * FMath::Sin(T * 47.0f);
		InOutView.Rotation.Yaw += Shake * 0.6f * FMath::Sin(T * 41.0f);
	}
}

// =================================================================================================================
// Visuals (every machine)
// =================================================================================================================
void UKGFishingComponent::CreateVisuals()
{
	AActor* Owner = GetOwner();
	if (!Owner || bVisualsReady || !Owner->GetRootComponent())
	{
		return;
	}
	auto MakeMesh = [Owner](const TCHAR* Name, UStaticMesh* Mesh, bool bShadow)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(Owner, Name, RF_Transient);
		M->SetupAttachment(Owner->GetRootComponent());
		M->SetUsingAbsoluteLocation(true);
		M->SetUsingAbsoluteRotation(true);
		M->SetUsingAbsoluteScale(true);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->SetCanEverAffectNavigation(false);
		M->SetCastShadow(bShadow);
		M->SetStaticMesh(Mesh);
		M->SetVisibility(false);
		M->RegisterComponent();
		return M;
	};
	UMaterialInterface* ShapeMat = KGFishingComp::LoadCached<UMaterialInterface>(KGFishingComp::ShapeMaterialPath);
	Bobber = MakeMesh(TEXT("KGFishBobber"), KGFishingComp::LoadCached<UStaticMesh>(KGFishingComp::BobberPath), false);
	Bobber->SetWorldScale3D(FVector(KGFishingComp::BobberScale));
	ThirdPersonRod = MakeMesh(TEXT("KGFishRodTP"), RodMesh, true);
	ThirdPersonRod->SetOwnerNoSee(true);
	Trophy = MakeMesh(TEXT("KGFishTrophy"), nullptr, true);

	auto MakeInstanced = [Owner, ShapeMat](const TCHAR* Name, const TCHAR* MeshPath, const FLinearColor& Colour, int32 Count)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(Owner, Name, RF_Transient);
		I->SetupAttachment(Owner->GetRootComponent());
		I->SetUsingAbsoluteLocation(true);
		I->SetUsingAbsoluteRotation(true);
		I->SetUsingAbsoluteScale(true);
		I->SetWorldTransform(FTransform::Identity);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCanEverAffectNavigation(false);
		I->SetCastShadow(false);
		I->SetStaticMesh(KGFishingComp::LoadCached<UStaticMesh>(MeshPath));
		if (ShapeMat)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMat, I);
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			I->SetMaterial(0, Mid);
		}
		I->RegisterComponent();
		for (int32 k = 0; k < Count; ++k)
		{
			I->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.0001f)), true);
		}
		I->SetVisibility(false);
		return I;
	};
	LineSegments = MakeInstanced(TEXT("KGFishLine"), KGFishingComp::CylinderPath, FLinearColor(0.92f, 0.9f, 0.82f),
	                             KGFishingComp::LineSegments);
	Droplets = MakeInstanced(TEXT("KGFishSplash"), KGFishingComp::SpherePath, FLinearColor(0.78f, 0.9f, 0.96f),
	                         KGFishingComp::MaxDrops);
	Drops.SetNum(0);

	ReelSound = NewObject<UAudioComponent>(Owner, TEXT("KGFishReel"), RF_Transient);
	ReelSound->SetupAttachment(Owner->GetRootComponent());
	ReelSound->bAutoActivate = false;
	ReelSound->SetSound(KGAudio::Get(TEXT("S_FishReel_Loop")));
	ReelSound->AttenuationSettings = KGAudio::Near();
	ReelSound->RegisterComponent();
	bVisualsReady = true;
}

FVector UKGFishingComponent::RodTipWorld() const
{
	const AKGCharacter* C = GetCharacter();
	if (!C)
	{
		return FVector::ZeroVector;
	}
	if (IsOwnerView())
	{
		// The first-person rod on the arms' weapon_r; FP primitives are drawn through the first-person transform, so
		// the line must start where the tip APPEARS, not where the component is.
		TArray<UStaticMeshComponent*> Meshes;
		C->GetComponents<UStaticMeshComponent>(Meshes);
		for (const UStaticMeshComponent* M : Meshes)
		{
			if (M->GetFName() == TEXT("HeldItem") && M->GetStaticMesh() == RodMesh && M->IsVisible())
			{
				FVector Tip = M->GetComponentTransform().TransformPosition(FVector(KGFishingComp::RodLength, 0.0f, 0.0f));
				const APlayerController* PC = Cast<APlayerController>(C->GetController());
				if (PC && PC->PlayerCameraManager)
				{
					const FMinimalViewInfo& View = PC->PlayerCameraManager->GetCameraCacheView();
					if (View.bUseFirstPersonParameters)
					{
						Tip = View.TransformWorldToFirstPerson(Tip, false);
					}
				}
				return Tip;
			}
		}
		// No FP rod yet (first frames): a point in front of the eye.
		return C->GetFirstPersonCamera()->GetComponentLocation() + C->GetControlRotation().Vector() * 90.0f +
		       FVector(0.0f, 0.0f, 25.0f);
	}
	return ThirdPersonRod ? ThirdPersonRod->GetComponentTransform().TransformPosition(FVector(KGFishingComp::RodLength, 0.0f, 0.0f))
	                      : C->GetActorLocation();
}

FVector UKGFishingComponent::BobberRestPoint(float Time) const
{
	const bool bPred = IsOwnerView() && bPredictCast;
	const FVector L = bPred ? PredResult.Landing : FVector(Line.Landing);
	const EKGFishWater W = bPred ? PredResult.Water : Line.Water;
	if (W == EKGFishWater::Sea || W == EKGFishWater::Basin)
	{
		return FVector(L.X, L.Y, FKGWaves::HeightAt(L.X, L.Y, Time));
	}
	return L;
}

void UKGFishingComponent::DrawLine(const FVector& A, const FVector& B, float Tension, bool bVisible)
{
	if (!LineSegments)
	{
		return;
	}
	if (!bVisible)
	{
		if (LineSegments->IsVisible())
		{
			LineSegments->SetVisibility(false);
		}
		return;
	}
	const float L = FVector::Dist(A, B);
	const float Sag = (1.0f - FMath::Clamp(Tension, 0.0f, 1.0f)) * 0.07f * L;
	const FVector Ctrl = (A + B) * 0.5f - FVector(0.0f, 0.0f, Sag);
	FVector Eye = A;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
	}
	TArray<FTransform> Xf;
	Xf.SetNum(KGFishingComp::LineSegments);
	FVector P0 = A;
	for (int32 i = 0; i < KGFishingComp::LineSegments; ++i)
	{
		const FVector P1 = KGFishingComp::Bezier(A, Ctrl, B, (i + 1.0f) / KGFishingComp::LineSegments);
		const FVector D = P1 - P0;
		const float Len = D.Size();
		const FVector Mid = (P0 + P1) * 0.5f;
		// About one pixel wide at any distance (a real line would vanish), never thicker than a cord up close.
		const float Radius = FMath::Clamp(FVector::Dist(Eye, Mid) * 0.0007f, 0.18f, 2.5f);
		const FQuat Rot = Len > KINDA_SMALL_NUMBER ? FRotationMatrix::MakeFromZ(D / Len).ToQuat() : FQuat::Identity;
		Xf[i] = FTransform(Rot, Mid, FVector(Radius * 0.02f, Radius * 0.02f, FMath::Max(Len, 0.01f) / 100.0f + 0.002f));
		P0 = P1;
	}
	LineSegments->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
	if (!LineSegments->IsVisible())
	{
		LineSegments->SetVisibility(true);
	}
}

void UKGFishingComponent::Splash(const FVector& At, float Size)
{
	for (int32 k = 0; k < 10; ++k)
	{
		if (Drops.Num() >= KGFishingComp::MaxDrops)
		{
			Drops.RemoveAt(0);
		}
		const float A = CosmeticRng.FRand() * UE_TWO_PI;
		const float R = (60.0f + 120.0f * CosmeticRng.FRand()) * Size;
		FDrop D;
		D.P = At + FVector(0.0f, 0.0f, 2.0f);
		D.V = FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, (260.0f + 260.0f * CosmeticRng.FRand()) * Size);
		D.Age = 0.0f;
		D.Life = 0.45f + 0.35f * CosmeticRng.FRand();
		D.Size = (1.6f + 2.2f * CosmeticRng.FRand()) * FMath::Max(Size, 0.5f);
		Drops.Add(D);
	}
}

void UKGFishingComponent::TickSplash(float DeltaTime)
{
	if (!Droplets)
	{
		return;
	}
	for (int32 i = Drops.Num() - 1; i >= 0; --i)
	{
		FDrop& D = Drops[i];
		D.Age += DeltaTime;
		D.V.Z -= 980.0f * DeltaTime;
		D.P += D.V * DeltaTime;
		if (D.Age >= D.Life)
		{
			Drops.RemoveAt(i);
		}
	}
	if (Drops.Num() == 0)
	{
		if (Droplets->IsVisible())
		{
			Droplets->SetVisibility(false);
		}
		return;
	}
	TArray<FTransform> Xf;
	Xf.SetNum(KGFishingComp::MaxDrops);
	for (int32 i = 0; i < KGFishingComp::MaxDrops; ++i)
	{
		if (Drops.IsValidIndex(i))
		{
			const FDrop& D = Drops[i];
			const float S = D.Size * (1.0f - D.Age / D.Life) / 100.0f;
			Xf[i] = FTransform(FQuat::Identity, D.P, FVector(S, S, S * 1.3f));
		}
		else
		{
			Xf[i] = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.0001f));
		}
	}
	Droplets->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
	if (!Droplets->IsVisible())
	{
		Droplets->SetVisibility(true);
	}
}

void UKGFishingComponent::UpdateThirdPersonRod(float DeltaTime, EKGFishPhase Phase)
{
	const AKGCharacter* C = GetCharacter();
	if (!C || !ThirdPersonRod)
	{
		return;
	}
	const float Now = static_cast<float>(GetWorld()->GetRealTimeSeconds());
	TPWindup = FMath::FInterpTo(TPWindup, Phase == EKGFishPhase::Charging ? Line.ChargeQ / 255.0f : 0.0f, DeltaTime, 8.0f);
	const float Tension = Line.TensionQ / 180.0f;
	const float FishX = Line.FishXQ / 127.0f;
	float Pitch = 28.0f;
	float YawOff = 10.0f;
	float Rate = 9.0f;
	switch (Phase)
	{
	case EKGFishPhase::Charging:
		Pitch = 28.0f + 92.0f * FMath::Max(TPWindup, 0.35f);   // back over the shoulder
		break;
	case EKGFishPhase::Flight:
		Pitch = Now - CastSeenAt < 0.18f ? 5.0f : 14.0f;
		Rate = 30.0f;
		break;
	case EKGFishPhase::Waiting:
		Pitch = 16.0f;
		break;
	case EKGFishPhase::Bite:
		Pitch = 38.0f;
		Rate = 25.0f;
		break;
	case EKGFishPhase::Fight:
		Pitch = 46.0f + 7.0f * Tension * FMath::Sin(Now * 11.0f);
		YawOff += FishX * 16.0f;
		break;
	case EKGFishPhase::Landed:
		Pitch = 66.0f;
		break;
	default:
		break;
	}
	const FRotator Aim(0.0f, C->GetActorRotation().Yaw + YawOff, 0.0f);
	const FVector Fwd = Aim.Vector();
	const float P = FMath::DegreesToRadians(Pitch);
	const FVector Want = (Fwd * FMath::Cos(P) + FVector::UpVector * FMath::Sin(P)).GetSafeNormal();
	FVector Dir = ThirdPersonRod->GetComponentRotation().Vector();
	if (!ThirdPersonRod->IsVisible() || Dir.IsNearlyZero())
	{
		Dir = Want;
	}
	Dir = FMath::VInterpNormalRotationTo(Dir, Want, DeltaTime, Rate * 60.0f);
	const USkeletalMeshComponent* Body = C->GetMesh();
	const FVector Hand = Body && Body->DoesSocketExist(TEXT("hand_r")) ? Body->GetSocketLocation(TEXT("hand_r"))
	                                                                   : C->GetActorLocation() + Fwd * 30.0f + FVector(0.0f, 0.0f, 20.0f);
	const FRotator Rot = FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).Rotator();   // reel hangs under the rod
	ThirdPersonRod->SetWorldLocationAndRotation(Hand - Dir * KGFishingComp::RodGrip, Rot);
}

void UKGFishingComponent::UpdateBodyPose(EKGFishPhase Phase)
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
	UAnimSequence* Want = nullptr;
	if (Phase != EKGFishPhase::Stowed)
	{
		Want = Phase == EKGFishPhase::Fight ? KGFishingComp::TPClip(TEXT("Fish_Reel"), TEXT("A_KG_Idle_Torch_Loop"))
		                                    : KGFishingComp::TPClip(TEXT("Fish_Hold"), TEXT("A_KG_Idle_Torch_Loop"));
	}
	const bool bPlaying = Want && Body->IsEmoteWanted() && Body->GetEmoteMain() == Want;
	if (Want == BodyClip && (bPlaying || !Want))
	{
		return;
	}
	if (Want)
	{
		Body->PlayEmote(nullptr, Want, true, true, 0.2f);
	}
	else if (BodyClip && Body->GetEmoteMain() == BodyClip)
	{
		Body->StopEmote(0.25f);
	}
	BodyClip = Want;
}

void UKGFishingComponent::TickVisuals(float DeltaTime)
{
	AKGCharacter* C = GetCharacter();
	UWorld* World = GetWorld();
	if (!C || !World || !bVisualsReady)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	const float WaveTime = World->GetTimeSeconds();
	const bool bOwner = IsOwnerView();
	const bool bRod = IsRodShown() && !C->IsDead();
	const EKGFishPhase Phase = bRod ? GetShownPhase() : EKGFishPhase::Stowed;

	// ---- one-shot events from replication ----
	if (!bVisualsPrimed)
	{
		ShownCast = Line.CastSerial;
		ShownNibble = Line.NibbleSerial;
		ShownBite = Line.BiteSerial;
		ShownPhaseLast = Phase;
		bVisualsPrimed = true;
	}
	if (Line.CastSerial != ShownCast)
	{
		ShownCast = Line.CastSerial;
		// The owner keeps its predicted launch time so the arc does not restart when the server confirms it.
		CastSeenAt = bOwner && Now - PredCastAt < 1.5 ? PredCastAt : Now;
		bLandedSplash = false;
		if (!bOwner)
		{
			KGAudio::At(this, TEXT("S_Fish_Cast"), C->GetActorLocation(), 0.8f);
		}
		if (UKGBodyAnimInstance* Body = C->GetBodyAnim(); Body && !(C->GetEmote() && C->GetEmote()->GetShownEmote()))
		{
			UAnimSequence* Hold = KGFishingComp::TPClip(TEXT("Fish_Hold"), TEXT("A_KG_Idle_Torch_Loop"));
			Body->PlayEmote(KGFishingComp::TPClip(TEXT("Fish_Cast"), TEXT("A_KG_Sword_Attack")), Hold, true, true, 0.08f);
			BodyClip = Hold;
		}
	}
	if (bOwner && bPredictCast && CastSeenAt < PredCastAt)
	{
		CastSeenAt = PredCastAt;
		bLandedSplash = false;
	}
	if (Line.NibbleSerial != ShownNibble)
	{
		ShownNibble = Line.NibbleSerial;
		NibbleSeenAt = Now;
		KGAudio::At(this, TEXT("S_Fish_Nibble"), BobberRestPoint(WaveTime), bOwner ? 0.6f : 0.35f);
		if (bOwner && C->GetViewmodel())
		{
			C->GetViewmodel()->AddRecoil(FVector(-8.0, 0.0, -6.0), FVector(-35.0, 0.0, 0.0));   // rod tip twitch
		}
	}
	if (Line.BiteSerial != ShownBite)
	{
		ShownBite = Line.BiteSerial;
		BiteShownAt = Now;
		const FVector At = BobberRestPoint(WaveTime);
		Splash(At, 1.0f);
		KGAudio::At(this, TEXT("S_Splash"), At, 0.9f);
		KGAudio::At(this, TEXT("S_Fish_Bite"), At, 1.0f);
		if (bOwner)
		{
			NudgeTime = 0.0f;
			if (C->GetViewmodel())
			{
				C->GetViewmodel()->AddRecoil(FVector(-30.0, 0.0, -24.0), FVector(-160.0, 20.0, 0.0));
			}
		}
	}
	if (Phase != ShownPhaseLast)
	{
		PhaseSeenAt = Now;
		switch (Phase)
		{
		case EKGFishPhase::Fight:
			if (!bOwner)
			{
				KGAudio::At(this, TEXT("S_Fish_Hook"), C->GetActorLocation(), 0.8f);
			}
			break;
		case EKGFishPhase::Landed:
			Splash(LastBobber, 1.3f);
			KGAudio::At(this, TEXT("S_Splash_Big"), LastBobber, 0.8f);
			if (!bOwner)
			{
				KGAudio::At(this, TEXT("S_Fish_Fanfare"), C->GetActorLocation(), 0.6f);
			}
			if (bOwner)
			{
				PendingOneShot = KGFishingComp::FPClip(TEXT("rod_hookset"));
			}
			break;
		case EKGFishPhase::Snapped:
			KGAudio::At(this, TEXT("S_Fish_Snap"), C->GetActorLocation(), 1.0f);
			break;
		case EKGFishPhase::Escaped:
			KGAudio::At(this, TEXT("S_Fish_Escape"), LastBobber, 0.8f);
			Splash(LastBobber, 0.6f);
			break;
		case EKGFishPhase::KoiTax:
			Splash(BobberRestPoint(WaveTime), 1.6f);
			KGAudio::At(this, TEXT("S_Splash_Big"), BobberRestPoint(WaveTime), 1.0f);
			break;
		default:
			break;
		}
		ShownPhaseLast = Phase;
	}

	TickSplash(DeltaTime);
	UpdateBodyPose(Phase);

	if (!bRod)
	{
		ThirdPersonRod->SetVisibility(false);
		Bobber->SetVisibility(false);
		Trophy->SetVisibility(false);
		DrawLine(FVector::ZeroVector, FVector::ZeroVector, 1.0f, false);
		if (ReelSound->IsPlaying())
		{
			ReelSound->Stop();
		}
		return;
	}
	UpdateThirdPersonRod(DeltaTime, Phase);
	ThirdPersonRod->SetVisibility(true);

	const FVector Tip = RodTipWorld();
	const FVector Hang = Tip - FVector(0.0f, 0.0f, bOwner ? 17.0f : 28.0f);   // owner: first-person scale
	FVector End = Hang;
	FVector BobberPos = Hang;
	float Tension = 1.0f;
	bool bBobber = true;
	bool bTrophy = false;
	const bool bPred = bOwner && bPredictCast;
	switch (Phase)
	{
	case EKGFishPhase::Flight:
	{
		const FVector Origin = bPred ? PredOrigin : FVector(Line.Origin);
		const FVector Vel = bPred ? PredVelocity : FVector(Line.Velocity);
		const float Flight = bPred ? PredResult.FlightTime : Line.FlightTime;
		const float T = FMath::Clamp(static_cast<float>(Now - CastSeenAt), 0.0f, Flight);
		BobberPos = T < Flight ? FKGCastSim::PointAt(Origin, Vel, T) : BobberRestPoint(WaveTime);
		End = BobberPos;
		Tension = 0.75f;
		break;
	}
	case EKGFishPhase::Waiting:
	case EKGFishPhase::Bite:
	{
		const FVector Rest = BobberRestPoint(WaveTime);
		if (!bLandedSplash)
		{
			bLandedSplash = true;
			Splash(Rest, 0.45f);
			KGAudio::At(this, TEXT("S_FishBite"), Rest, 0.7f);   // the plop of the bobber landing
		}
		float Dip = 0.0f;
		const float SinceNibble = static_cast<float>(Now - NibbleSeenAt);
		if (SinceNibble < 0.35f)
		{
			Dip = 4.0f * FMath::Sin(PI * SinceNibble / 0.35f);
		}
		if (Phase == EKGFishPhase::Bite)
		{
			const float Since = static_cast<float>(Now - BiteShownAt);
			Dip = FMath::Min(1.0f, Since / 0.12f) * 22.0f + 3.0f * FMath::Sin(Since * 34.0f);
		}
		BobberPos = Rest - FVector(0.0f, 0.0f, Dip);
		End = BobberPos;
		Tension = Phase == EKGFishPhase::Bite ? 0.55f : 0.12f;
		break;
	}
	case EKGFishPhase::Fight:
	{
		const bool bSim = bOwner && bLocalSim;
		const float Dist = (bSim ? LocalSim.Distance : Line.DistanceCm / 100.0f) * 100.0f;
		const float FishX = bSim ? LocalSim.FishX : Line.FishXQ / 127.0f;
		Tension = bSim ? LocalSim.Tension : Line.TensionQ / 180.0f;
		const FVector From = C->GetActorLocation();
		const FVector Dir = (FVector(Line.Landing) - From).GetSafeNormal2D(UE_SMALL_NUMBER, C->GetActorForwardVector());
		const FVector Right(-Dir.Y, Dir.X, 0.0f);
		FVector Fish = From + Dir * Dist + Right * FishX * FMath::Min(350.0f, Dist * 0.35f);
		const float Surface = (Line.Water == EKGFishWater::Sea || Line.Water == EKGFishWater::Basin)
			                      ? FKGWaves::HeightAt(Fish.X, Fish.Y, WaveTime) : Line.Landing.Z;
		Fish.Z = Surface - 35.0f;
		LastFishPoint = LastFishPoint.IsNearlyZero() || FVector::DistSquared(LastFishPoint, Fish) > FMath::Square(600.0f)
			                ? Fish : FMath::VInterpTo(LastFishPoint, Fish, DeltaTime, 6.0f);
		BobberPos = FVector(LastFishPoint.X, LastFishPoint.Y, Surface - 6.0f - 5.0f * FMath::Sin(static_cast<float>(Now) * 13.0f));
		End = BobberPos;
		if (CosmeticRng.FRand() < DeltaTime * (0.6f + 1.4f * Tension))
		{
			Splash(BobberPos, 0.35f + 0.4f * Tension);
		}
		break;
	}
	case EKGFishPhase::Landed:
	{
		const FKGItemDef* Def = UKGItemCatalog::Find(Catch.ItemId);
		UStaticMesh* Mesh = Def ? KGFishingComp::LoadCached<UStaticMesh>(Def->PickupMesh.ToString()) : nullptr;
		if (!Mesh && Def)
		{
			Mesh = KGFishingComp::LoadCached<UStaticMesh>(Def->PickupFallbackShape.ToString());
		}
		if (Mesh)
		{
			if (Trophy->GetStaticMesh() != Mesh)
			{
				Trophy->SetStaticMesh(Mesh);
			}
			const FKGFishSpecies* S = FKGFishingRules::FindByItem(Catch.ItemId);
			const float Extent = FMath::Max(Mesh->GetBounds().BoxExtent.GetMax() * 2.0f, 1.0f);
			float Scale = S ? 0.85f + 0.5f * FKGFishingRules::WeightAlpha(*S, Catch.Grams) : (Def ? Def->PickupSize : 25.0f) / Extent;
			Scale *= bOwner ? 0.6f : 1.0f;   // the owner's tip is in first-person space (FirstPersonScale)
			const float Since = static_cast<float>(Now - PhaseSeenAt);
			const float Wiggle = 28.0f * FMath::Exp(-Since * 1.2f) * FMath::Sin(Since * 16.0f);
			const FVector Mouth = Tip - FVector(0.0f, 0.0f, 22.0f * (bOwner ? 0.6f : 1.0f));
			// Fish meshes point +X at the nose: hang nose up under the line.
			const FRotator Rot(90.0f, C->GetActorRotation().Yaw + 90.0f + Wiggle, 0.0f);
			const FVector Down = -Rot.Vector() * Extent * 0.5f * Scale;
			Trophy->SetWorldLocationAndRotation(Mouth + Down, Rot);
			Trophy->SetWorldScale3D(FVector(Scale));
			bTrophy = true;
			End = Mouth;
		}
		bBobber = false;
		Tension = 1.0f;
		break;
	}
	case EKGFishPhase::Snapped:
	{
		const float Since = static_cast<float>(Now - PhaseSeenAt);
		End = Tip - FVector(0.0f, 0.0f, 40.0f) + FVector(12.0f * FMath::Sin(Since * 9.0f), 0.0f, 0.0f);
		BobberPos = LastBobber + FVector(0.0f, 0.0f, FMath::Sin(Since * 3.0f) * 2.0f);   // left drifting
		Tension = 1.0f;
		bBobber = Since < 1.8f;
		DrawLine(Tip, End, Tension, true);
		Bobber->SetWorldLocation(BobberPos);
		Bobber->SetVisibility(bBobber);
		Trophy->SetVisibility(false);
		if (ReelSound->IsPlaying())
		{
			ReelSound->Stop();
		}
		return;
	}
	case EKGFishPhase::Escaped:
	case EKGFishPhase::KoiTax:
	case EKGFishPhase::Snagged:
	{
		const float Since = static_cast<float>(Now - PhaseSeenAt);
		const float Hold = Phase == EKGFishPhase::Snagged ? 0.5f : Phase == EKGFishPhase::KoiTax ? 0.4f : 0.0f;
		const FVector From = Phase == EKGFishPhase::Escaped ? LastBobber : BobberRestPoint(WaveTime);
		BobberPos = FMath::Lerp(From, Hang, KGFishingComp::EaseOut((Since - Hold) / 0.7f));
		End = BobberPos;
		Tension = 0.6f;
		break;
	}
	default:
		// Ready / Charging: the bobber dangles under the tip.
		BobberPos = Hang + FVector(3.0f * FMath::Sin(static_cast<float>(Now) * 1.7f), 0.0f, 0.0f);
		End = BobberPos;
		Tension = 1.0f;
		break;
	}
	if (KGFishingComp::LineIsOut(Phase))
	{
		LastBobber = BobberPos;
	}
	DrawLine(Tip, End, Tension, true);
	const bool bDangle = Phase == EKGFishPhase::Ready || Phase == EKGFishPhase::Charging;
	// The dangling bobber is in first-person space for the owner: shrink it with the FP scale.
	Bobber->SetWorldScale3D(FVector(KGFishingComp::BobberScale * (bOwner && bDangle ? 0.6f : 1.0f)));
	Bobber->SetWorldLocationAndRotation(BobberPos, FRotator(Phase == EKGFishPhase::Fight ? 35.0f : 0.0f, 0.0f, 0.0f));
	Bobber->SetVisibility(bBobber);
	Trophy->SetVisibility(bTrophy);

	// Reel click: the whole jetty hears you reeling (fishing makes noise).
	if (Phase == EKGFishPhase::Fight)
	{
		if (!ReelSound->IsPlaying() && ReelSound->Sound)
		{
			ReelSound->Play();
		}
		const bool bReeling = bOwner ? LastSentInput.bReel : Tension > 0.3f;
		ReelSound->SetVolumeMultiplier(bReeling ? 0.9f : 0.25f);
		ReelSound->SetPitchMultiplier(0.85f + 0.45f * FMath::Clamp(Tension, 0.0f, 1.2f));
	}
	else if (ReelSound->IsPlaying())
	{
		ReelSound->Stop();
	}
}
