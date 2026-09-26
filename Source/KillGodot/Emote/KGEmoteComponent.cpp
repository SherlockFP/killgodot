#include "Emote/KGEmoteComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraTypes.h"
#include "Character/KGBodyAnimInstance.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGEmoji.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"

namespace KGEmotePrivate
{
	TAutoConsoleVariable<float> CVarCamDistance(TEXT("kg.Emote.CamDistance"), 270.0f,
		TEXT("Emotes: third-person camera distance (cm) for full-body emotes."));
	TAutoConsoleVariable<float> CVarCamDistanceUpper(TEXT("kg.Emote.CamDistanceUpper"), 185.0f,
		TEXT("Emotes: over-the-shoulder camera distance (cm) for upper-body emotes without a first-person gesture."));
	TAutoConsoleVariable<float> CVarCamOrbit(TEXT("kg.Emote.CamOrbit"), 145.0f,
		TEXT("Emotes: degrees the camera swings around the body for full-body emotes (0 = stay behind, 180 = in front)."));
	TAutoConsoleVariable<float> CVarCamBlendIn(TEXT("kg.Emote.CamBlendIn"), 0.45f, TEXT("Emotes: camera pull-out time (s)."));
	TAutoConsoleVariable<float> CVarCamBlendOut(TEXT("kg.Emote.CamBlendOut"), 0.35f, TEXT("Emotes: camera return time (s)."));
	TAutoConsoleVariable<int32> CVarDebugCam(TEXT("kg.Emote.DebugCam"), 0,
		TEXT("Emotes: 1 keeps the local player in the third-person emote camera (look at your own body / clips)."));

	float SmoothStep(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	const TCHAR* StopName(EKGEmoteStop Reason)
	{
		const UEnum* Enum = StaticEnum<EKGEmoteStop>();
		static TMap<int64, FString> Names;
		FString& Name = Names.FindOrAdd(int64(Reason));
		if (Name.IsEmpty())
		{
			Name = Enum ? Enum->GetNameStringByValue(int64(Reason)) : TEXT("?");
		}
		return *Name;
	}

	const TCHAR* MachineName(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("?");
		}
		switch (World->GetNetMode())
		{
		case NM_Client: return TEXT("Client");
		case NM_ListenServer: return TEXT("Host");
		case NM_DedicatedServer: return TEXT("Server");
		default: return TEXT("Standalone");
		}
	}
}

UKGEmoteComponent::UKGEmoteComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
	Limiter = FKGEmoteRules::MakeLimiter();
	PartnerLimiter = FKGEmoteRules::MakeLimiter();
}

void UKGEmoteComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGEmoteComponent, Playback, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGEmoteComponent, Partner, Params);
}

UKGEmoteComponent* UKGEmoteComponent::FindForPlayer(const APlayerState* PlayerState)
{
	const AKGCharacter* Character = PlayerState ? Cast<AKGCharacter>(PlayerState->GetPawn()) : nullptr;
	return Character ? Character->GetEmote() : nullptr;
}

AKGCharacter* UKGEmoteComponent::GetCharacter() const
{
	return Cast<AKGCharacter>(GetOwner());
}

UKGBodyAnimInstance* UKGEmoteComponent::GetBodyAnim() const
{
	const AKGCharacter* Character = GetCharacter();
	return Character ? Character->GetBodyAnim() : nullptr;
}

bool UKGEmoteComponent::IsOwnerView() const
{
	const AKGCharacter* Character = GetCharacter();
	return Character && Character->IsLocallyControlled() && Character->IsPlayerControlled();
}

FName UKGEmoteComponent::GetActiveEmoteId() const
{
	const FKGEmoteDef* Def = GetActiveEmote();
	return Def ? Def->Id : NAME_None;
}

const FKGEmoteDef* UKGEmoteComponent::GetShownEmote() const
{
	return bLocallyCancelled ? nullptr : FKGEmoteCatalog::Get(int32(ShownEmote) - 1);
}

UAnimSequence* UKGEmoteComponent::LoadClip(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return nullptr;
	}
	static TMap<FSoftObjectPath, TStrongObjectPtr<UAnimSequence>> Cache;
	if (const TStrongObjectPtr<UAnimSequence>* Found = Cache.Find(Path))
	{
		return Found->Get();
	}
	UAnimSequence* Seq = Cast<UAnimSequence>(Path.TryLoad());
	if (!Seq)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("[Emote] clip missing: %s (run Tools/Unreal/kg_import_emotes.py)"), *Path.ToString());
	}
	Cache.Add(Path, TStrongObjectPtr<UAnimSequence>(Seq));
	return Seq;
}

float UKGEmoteComponent::ComputeDuration(const FKGEmoteDef& Def)
{
	if (Def.bLoop)
	{
		return Def.MaxSeconds > 0.0f ? Def.MaxSeconds : -1.0f;
	}
	const UAnimSequence* Main = LoadClip(Def.Clip);
	const UAnimSequence* Intro = LoadClip(Def.IntroClip);
	const float Length = (Main ? Main->GetPlayLength() : 2.0f) + (Intro ? Intro->GetPlayLength() : 0.0f);
	// End a little before the last frame: the replicated stop then arrives while the clip settles into idle.
	return FMath::Max(0.3f, Length / FMath::Max(Def.PlayRate, 0.05f) - Def.BlendOut * 0.5f);
}

FKGEmoteBodyState UKGEmoteComponent::MakeBodyState() const
{
	FKGEmoteBodyState Body;
	const AKGCharacter* Character = GetCharacter();
	if (!Character)
	{
		Body.bHasBody = false;
		return Body;
	}
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Body.bDead = Character->IsDead();
	Body.bFalling = Move && Move->IsFalling();
	Body.bSwimming = Move && Move->IsSwimming();
	// Ladders and seats: flying (ladder) or no movement at all (seated, AKGSeat disables the CMC).
	Body.bClimbing = Move && Move->MovementMode == MOVE_Flying;
	Body.bSeated = Move && Move->MovementMode == MOVE_None && !Body.bDead;
	Body.bCarrying = Character->bHoldingObject;
	Body.bCrouched = Character->bIsCrouched;
	Body.Speed = Character->GetVelocity().Size2D();
	return Body;
}

// ---- server -----------------------------------------------------------------------------------------------------

EKGEmoteReject UKGEmoteComponent::ServerTryStart(FName IdOrAlias, bool bIgnoreRateLimit)
{
	AKGCharacter* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !Character->HasAuthority())
	{
		return EKGEmoteReject::NoBody;
	}
	const int32 Index = FKGEmoteCatalog::IndexOf(IdOrAlias);
	const FKGEmoteDef* Def = FKGEmoteCatalog::Get(Index);
	if (!Def)
	{
		return EKGEmoteReject::Unknown;
	}
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	const EKGPhase Phase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	const float Remaining = GS ? GS->GetPhaseRemaining() : 0.0f;
	const FKGChatParticipant Who = UKGChatComponent::MakeParticipant(Character->GetPlayerState());
	EKGEmoteReject Reject = FKGEmoteRules::CanStart(*Def, Who, MakeBodyState(), Phase, Remaining);
	if (Reject == EKGEmoteReject::None && !bIgnoreRateLimit &&
	    Limiter.TryConsume(World->GetRealTimeSeconds(), FString()) != EKGChatReject::None)
	{
		Reject = EKGEmoteReject::RateLimited;
	}
	if (Reject != EKGEmoteReject::None)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_REJECT %s pawn=%s emote=%s reason=%s"), KGEmotePrivate::MachineName(World),
		       *Character->GetName(), *Def->Id.ToString(), *StaticEnum<EKGEmoteReject>()->GetNameStringByValue(int64(Reject)));
		return Reject;
	}
	if (Playback.Emote != 0)
	{
		LastStop = EKGEmoteStop::Replaced;
	}
	Playback.Emote = static_cast<uint8>(Index + 1);
	Playback.Serial = static_cast<uint8>(Playback.Serial + 1);
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGEmoteComponent, Playback, this);
	ServerElapsed = 0.0f;
	ServerDuration = ComputeDuration(*Def);
	LastPhase = Phase;
	bHasLastPhase = true;
	Character->ForceNetUpdate();
	UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_START %s pawn=%s emote=%s serial=%d duration=%.2f"), KGEmotePrivate::MachineName(World),
	       *Character->GetName(), *Def->Id.ToString(), Playback.Serial, ServerDuration);
	SyncPresentation();
	return EKGEmoteReject::None;
}

void UKGEmoteComponent::ServerStop(EKGEmoteStop Reason)
{
	AKGCharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority() || Playback.Emote == 0)
	{
		return;
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_STOP %s pawn=%s emote=%s reason=%s after=%.2f"),
	       KGEmotePrivate::MachineName(GetWorld()), *Character->GetName(), *GetActiveEmoteId().ToString(),
	       KGEmotePrivate::StopName(Reason), ServerElapsed);
	Playback.Emote = 0;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGEmoteComponent, Playback, this);
	LastStop = Reason;
	Character->ForceNetUpdate();
	SyncPresentation();
	if (Partner.IsActive() && FKGPartnerRules::StopEndsPartner(Reason))
	{
		ServerPartnerEnd(Reason);   // walking off, attacking, being hit or dying breaks the pairing for both
	}
}

void UKGEmoteComponent::ServerStopEmote_Implementation(EKGEmoteStop Reason)
{
	// The owner may only report what it saw locally; anything else is a plain request.
	const bool bValid = Reason == EKGEmoteStop::Moved || Reason == EKGEmoteStop::Attacked || Reason == EKGEmoteStop::Requested;
	ServerStop(bValid ? Reason : EKGEmoteStop::Requested);
}

void UKGEmoteComponent::TickServer(float DeltaTime)
{
	UWorld* World = GetWorld();
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	const EKGPhase Phase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	const bool bPhaseChanged = bHasLastPhase && FKGEmoteRules::PhaseChangeStops(LastPhase, Phase);
	LastPhase = Phase;
	bHasLastPhase = true;
	const FKGEmoteDef* Def = GetActiveEmote();
	if (!Def)
	{
		return;
	}
	ServerElapsed += DeltaTime;
	if (bPhaseChanged)
	{
		ServerStop(EKGEmoteStop::Phase);
		return;
	}
	EKGEmoteStop Reason;
	const FKGChatParticipant Who = UKGChatComponent::MakeParticipant(GetCharacter()->GetPlayerState());
	if (FKGEmoteRules::ShouldStop(*Def, Who, MakeBodyState(), Phase, GS ? GS->GetPhaseRemaining() : 0.0f, Reason))
	{
		ServerStop(Reason);
		return;
	}
	if (ServerDuration > 0.0f && ServerElapsed >= ServerDuration)
	{
		ServerStop(EKGEmoteStop::Finished);
	}
}

// ---- owning client -------------------------------------------------------------------------------------------------

void UKGEmoteComponent::RequestStop(EKGEmoteStop Reason)
{
	AKGCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	if (const FKGEmoteDef* Shown = GetShownEmote())
	{
		bLocallyCancelled = true;
		StopPresentation(Shown->BlendOut);
	}
	if (Character->HasAuthority())
	{
		ServerStop(Reason);
	}
	else if (Playback.Emote != 0 || ShownEmote != 0)
	{
		ServerStopEmote(Reason);
	}
}

void UKGEmoteComponent::NotifyLocalAction()
{
	if (GetShownEmote())
	{
		RequestStop(EKGEmoteStop::Attacked);
	}
}

// ---- presentation ---------------------------------------------------------------------------------------------------

void UKGEmoteComponent::OnRep_Playback()
{
	UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_REP %s pawn=%s emote=%s serial=%d local=%d"), KGEmotePrivate::MachineName(GetWorld()),
	       *GetNameSafe(GetOwner()), Playback.Emote ? *GetActiveEmoteId().ToString() : TEXT("none"), Playback.Serial,
	       IsOwnerView() ? 1 : 0);
	SyncPresentation();
}

void UKGEmoteComponent::SyncPresentation()
{
	if (Playback.Emote == ShownEmote && Playback.Serial == ShownSerial)
	{
		return;
	}
	const FKGEmoteDef* Old = FKGEmoteCatalog::Get(int32(ShownEmote) - 1);
	const FKGEmoteDef* New = GetActiveEmote();
	ShownSerial = Playback.Serial;
	if (!New)
	{
		if (Old && !bLocallyCancelled)
		{
			StopPresentation(Old->BlendOut);
		}
		ShownEmote = 0;
		bLocallyCancelled = false;
		return;
	}
	ShownEmote = Playback.Emote;
	StartPresentation(*New);
}

void UKGEmoteComponent::StartPresentation(const FKGEmoteDef& Def)
{
	AKGCharacter* Character = GetCharacter();
	bLocallyCancelled = false;
	ShownElapsed = 0.0f;
	if (!Character)
	{
		return;
	}
	if (UKGBodyAnimInstance* Body = GetBodyAnim())
	{
		if (UAnimSequence* Main = LoadClip(Def.Clip))
		{
			Body->PlayEmote(LoadClip(Def.IntroClip), Main, Def.bLoop, !Def.IsFullBody(), Def.BlendIn, Def.PlayRate);
		}
	}
	SetBodyYawFree(Def.IsFullBody());
	if (IsOwnerView() && !Def.UsesThirdPersonCamera())
	{
		Character->PlayArmsGesture(LoadClip(Def.FirstPersonClip));
	}
}

void UKGEmoteComponent::StopPresentation(float BlendOut)
{
	if (UKGBodyAnimInstance* Body = GetBodyAnim(); Body && Body->IsEmoteWanted())
	{
		Body->StopEmote(BlendOut);
	}
	SetBodyYawFree(false);
}

void UKGEmoteComponent::SetBodyYawFree(bool bFree)
{
	// Full-body emotes: the body keeps its facing while the owner orbits the camera. Server and owner agree on it
	// (the server's CMC applies the owner's control rotation to the body otherwise), simulated proxies follow.
	AKGCharacter* Character = GetCharacter();
	if (!Character || bFree == bYawFreed || !(Character->HasAuthority() || Character->IsLocallyControlled()))
	{
		return;
	}
	bYawFreed = bFree;
	if (bFree)
	{
		bSavedUseControllerYaw = Character->bUseControllerRotationYaw;
		Character->bUseControllerRotationYaw = false;
	}
	else
	{
		Character->bUseControllerRotationYaw = bSavedUseControllerYaw;
	}
}

void UKGEmoteComponent::SetOwnerSeesBody(bool bSee)
{
	AKGCharacter* Character = GetCharacter();
	if (!Character || bSee == bOwnerSeesBody)
	{
		return;
	}
	bOwnerSeesBody = bSee;
	if (bSee)
	{
		UnhiddenForOwner.Reset();
		TArray<USceneComponent*> Parts;
		Parts.Add(Character->GetMesh());
		Character->GetMesh()->GetChildrenComponents(true, Parts);
		for (USceneComponent* Part : Parts)
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Part);
			if (Prim && Prim->bOwnerNoSee)
			{
				Prim->SetOwnerNoSee(false);
				UnhiddenForOwner.Add(Prim);
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Prim : UnhiddenForOwner)
		{
			if (Prim.IsValid())
			{
				Prim->SetOwnerNoSee(true);
			}
		}
		UnhiddenForOwner.Reset();
	}
}

void UKGEmoteComponent::TickPresentation(float DeltaTime)
{
	SyncPresentation();
	AKGCharacter* Character = GetCharacter();
	const FKGEmoteDef* Shown = GetShownEmote();
	if (!Character || !Shown)
	{
		return;
	}
	ShownElapsed += DeltaTime;
	if (Character->IsDead())
	{
		bLocallyCancelled = true;
		StopPresentation(0.1f);
		return;
	}
	// The mesh re-created its anim instance (outfit swap): put a looping emote back on.
	if (UKGBodyAnimInstance* Body = GetBodyAnim(); Body && !Body->IsEmoteWanted() && Shown->bLoop)
	{
		if (UAnimSequence* Main = LoadClip(Shown->Clip))
		{
			Body->PlayEmote(nullptr, Main, true, !Shown->IsFullBody(), 0.1f, Shown->PlayRate);
		}
	}
	// The owner cancels a full-body emote the moment it moves, jumps or crouches (the server follows by velocity).
	if (IsOwnerView() && Shown->IsFullBody() && ShownElapsed > 0.15f)
	{
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		const bool bInput = (Move && Move->GetCurrentAcceleration().SizeSquared2D() > 1.0) || Character->bPressedJump ||
		                    Character->bIsCrouched;
		if (bInput)
		{
			RequestStop(EKGEmoteStop::Moved);
		}
	}
}

void UKGEmoteComponent::TickOwnerView(float DeltaTime)
{
	using namespace KGEmotePrivate;
	const AKGCharacter* Character = GetCharacter();
	const bool bOwner = IsOwnerView();
	const FKGEmoteDef* Shown = GetShownEmote();
	const bool bDead = !Character || Character->IsDead();
	bWantThirdPerson = bOwner && !bDead && ((Shown && Shown->UsesThirdPersonCamera()) || CVarDebugCam.GetValueOnGameThread() != 0);
	bWantFullBodyCam = bWantThirdPerson && (!Shown || Shown->IsFullBody());
	if (bDead || !bOwner)
	{
		CamAlpha = 0.0f;
		CamOrbit = 0.0f;
	}
	else
	{
		const float Time = bWantThirdPerson ? CVarCamBlendIn.GetValueOnGameThread() : CVarCamBlendOut.GetValueOnGameThread();
		CamAlpha = FMath::Clamp(CamAlpha + (bWantThirdPerson ? 1.0f : -1.0f) * DeltaTime / FMath::Max(Time, 0.01f), 0.0f, 1.0f);
		const float OrbitGoal = bWantThirdPerson && Shown && Shown->IsFullBody() ? CVarCamOrbit.GetValueOnGameThread() : 0.0f;
		CamOrbit = FMath::FInterpTo(CamOrbit, OrbitGoal, DeltaTime, bWantThirdPerson ? 3.0f : 7.0f);
	}
	// Show our own body (and its cosmetics) once the camera has left the head.
	SetOwnerSeesBody(bOwner && CamAlpha > 0.12f);
}

void UKGEmoteComponent::ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView)
{
	using namespace KGEmotePrivate;
	const AKGCharacter* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || CamAlpha <= 0.0f)
	{
		return;
	}
	const float A = SmoothStep(CamAlpha);
	const bool bFull = bWantFullBodyCam || (!bWantThirdPerson && CamOrbit > 1.0f);
	const FRotator ViewRot = InOutView.Rotation;
	FRotator Orbit = ViewRot;
	Orbit.Yaw += CamOrbit;
	// Look slightly down on the body; never from under the floor.
	Orbit.Pitch = FMath::Clamp(Orbit.Pitch - 10.0f, -60.0f, 40.0f);
	const FVector Pivot = Character->GetActorLocation() + FVector(0.0f, 0.0f, bFull ? 35.0f : 62.0f);
	const FRotationMatrix M(Orbit);
	const float Distance = bFull ? CVarCamDistance.GetValueOnGameThread() : CVarCamDistanceUpper.GetValueOnGameThread();
	const float Side = bFull ? 0.0f : 42.0f;
	FVector Desired = Pivot - M.GetUnitAxis(EAxis::X) * Distance + M.GetUnitAxis(EAxis::Y) * Side + FVector(0.0f, 0.0f, 12.0f);
	// Spring-arm style: never through walls.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGEmoteCamera), false, Character);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.0f), Params))
	{
		Desired = Hit.Location;
	}
	InOutView.Location = FMath::Lerp(InOutView.Location, Desired, A);
	InOutView.Rotation = FQuat::Slerp(ViewRot.Quaternion(), Orbit.Quaternion(), A).Rotator();
}

void UKGEmoteComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
		TickPartnerServer(DeltaTime);
	}
	TickPresentation(DeltaTime);
	TickOwnerView(DeltaTime);
}

// ---- partner emotes (SPRINT-023) ----------------------------------------------------------------------------------

namespace KGEmotePrivate
{
	uint64 MatchSeedOf(const UWorld* World)
	{
		const AKGGameMode* GM = World ? World->GetAuthGameMode<AKGGameMode>() : nullptr;
		const int64 Seed = GM ? GM->GetMatchSeed() : 0;
		return Seed != 0 ? static_cast<uint64>(Seed) : 0x4B47504152544E52ull;   // "KGPARTNR": deterministic pre-match too
	}

	const TCHAR* StageName(uint8 Stage)
	{
		switch (static_cast<EKGPartnerStage>(Stage))
		{
		case EKGPartnerStage::Offering: return TEXT("Offering");
		case EKGPartnerStage::Playing: return TEXT("Playing");
		case EKGPartnerStage::Result: return TEXT("Result");
		default: return TEXT("None");
		}
	}
}

const FKGPartnerEmoteDef* UKGEmoteComponent::GetPartnerDef() const
{
	return Partner.IsActive() ? FKGPartnerCatalog::Get(Partner.GetKind()) : nullptr;
}

void UKGEmoteComponent::SetPartnerState(const FKGPartnerState& NewState)
{
	Partner = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(UKGEmoteComponent, Partner, this);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnRep_Partner();
}

void UKGEmoteComponent::RequestPartnerOffer(FName IdOrAlias)
{
	if (IsOwnerView() || (GetOwner() && GetOwner()->HasAuthority() && GetCharacter() && GetCharacter()->IsLocallyControlled()))
	{
		ServerRequestPartnerOffer(IdOrAlias);
	}
}

void UKGEmoteComponent::RequestPartnerCancel()
{
	if (Partner.IsActive())
	{
		ServerRequestPartnerCancel();
	}
}

UKGEmoteComponent* UKGEmoteComponent::FindNearbyOffer(const AKGCharacter* Me)
{
	if (!Me || !Me->GetWorld())
	{
		return nullptr;
	}
	UKGEmoteComponent* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AKGCharacter> It(Me->GetWorld()); It; ++It)
	{
		UKGEmoteComponent* Other = *It != Me ? It->GetEmote() : nullptr;
		if (!Other || Other->Partner.GetStage() != EKGPartnerStage::Offering || Other->Partner.Partner || It->IsDead())
		{
			continue;
		}
		if (!FKGPartnerRules::WithinAcceptRadius(It->GetActorLocation(), Me->GetActorLocation()))
		{
			continue;
		}
		const float D = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), Me->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Other;
		}
	}
	return Best;
}

bool UKGEmoteComponent::TryAcceptNearbyOffer()
{
	AKGCharacter* Me = GetCharacter();
	if (!Me || !Me->IsLocallyControlled() || Partner.IsActive())
	{
		return false;
	}
	UKGEmoteComponent* Offer = FindNearbyOffer(Me);
	if (!Offer || !Offer->GetCharacter())
	{
		return false;
	}
	ServerRequestPartnerAccept(Offer->GetCharacter()->GetPlayerState());
	return true;
}

void UKGEmoteComponent::ServerRequestPartnerOffer_Implementation(FName IdOrAlias)
{
	ServerPartnerOffer(IdOrAlias);
}

void UKGEmoteComponent::ServerRequestPartnerAccept_Implementation(APlayerState* Offerer)
{
	ServerPartnerAccept(FindForPlayer(Offerer));
}

void UKGEmoteComponent::ServerRequestPartnerCancel_Implementation()
{
	ServerPartnerEnd(EKGEmoteStop::Requested);
}

EKGEmoteReject UKGEmoteComponent::ServerPartnerOffer(FName IdOrAlias, bool bIgnoreRateLimit)
{
	AKGCharacter* Character = GetCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !Character->HasAuthority())
	{
		return EKGEmoteReject::NoBody;
	}
	const FKGPartnerEmoteDef* Def = FKGPartnerCatalog::Find(IdOrAlias);
	const FKGEmoteDef* ClipA = Def ? FKGEmoteCatalog::Find(Def->EmoteA) : nullptr;
	if (!Def || !ClipA)
	{
		return EKGEmoteReject::Unknown;
	}
	if (Partner.IsActive())
	{
		ServerPartnerEnd(EKGEmoteStop::Replaced);
	}
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	const EKGPhase Phase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	const float Remaining = GS ? GS->GetPhaseRemaining() : 0.0f;
	const FKGChatParticipant Who = UKGChatComponent::MakeParticipant(Character->GetPlayerState());
	EKGEmoteReject Reject = FKGEmoteRules::CanStart(*ClipA, Who, MakeBodyState(), Phase, Remaining);
	if (Reject == EKGEmoteReject::None && !bIgnoreRateLimit &&
	    PartnerLimiter.TryConsume(World->GetRealTimeSeconds(), FString()) != EKGChatReject::None)
	{
		Reject = EKGEmoteReject::RateLimited;
	}
	if (Reject != EKGEmoteReject::None)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_REJECT %s pawn=%s kind=%s reason=%s"), KGEmotePrivate::MachineName(World),
		       *Character->GetName(), *Def->Id.ToString(), *StaticEnum<EKGEmoteReject>()->GetNameStringByValue(int64(Reject)));
		return Reject;
	}
	FKGPartnerState S;
	S.Kind = static_cast<uint8>(Def->Kind);
	S.Stage = static_cast<uint8>(EKGPartnerStage::Offering);
	S.Serial = static_cast<uint8>(Partner.Serial + 1);
	S.bInitiator = 1;
	PartnerElapsed = 0.0f;
	SetPartnerState(S);
	if (UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(Character->GetPlayerState()))
	{
		Chat->ServerSay(EKGChatChannel::Nearby, FString::Printf(TEXT("offers a %s (E to accept)"), *Def->DisplayName.ToString().ToLower()),
		                EKGChatFlags::Action, FKGEmoji::Find(Def->Emoji));
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_OFFER %s pawn=%s kind=%s serial=%d"), KGEmotePrivate::MachineName(World),
	       *Character->GetName(), *Def->Id.ToString(), S.Serial);
	return EKGEmoteReject::None;
}

EKGEmoteReject UKGEmoteComponent::ServerPartnerAccept(UKGEmoteComponent* Offerer)
{
	AKGCharacter* Me = GetCharacter();
	AKGCharacter* Them = Offerer ? Offerer->GetCharacter() : nullptr;
	UWorld* World = GetWorld();
	if (!Me || !World || !Me->HasAuthority())
	{
		return EKGEmoteReject::NoBody;
	}
	const FKGPartnerEmoteDef* Def = Offerer ? Offerer->GetPartnerDef() : nullptr;
	const FKGEmoteDef* ClipB = Def ? FKGEmoteCatalog::Find(Def->EmoteB) : nullptr;
	if (!Them || !Def || !ClipB || Offerer->Partner.GetStage() != EKGPartnerStage::Offering || Offerer->Partner.Partner ||
	    Offerer == this)
	{
		return EKGEmoteReject::Unknown;
	}
	if (!FKGPartnerRules::WithinAcceptRadius(Them->GetActorLocation(), Me->GetActorLocation()))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_REJECT %s pawn=%s kind=%s reason=TooFar"), KGEmotePrivate::MachineName(World),
		       *Me->GetName(), *Def->Id.ToString());
		return EKGEmoteReject::Busy;
	}
	const AKGGameState* GS = World->GetGameState<AKGGameState>();
	const EKGPhase Phase = GS ? GS->GetPhase() : EKGPhase::Lobby;
	const float Remaining = GS ? GS->GetPhaseRemaining() : 0.0f;
	const FKGChatParticipant Who = UKGChatComponent::MakeParticipant(Me->GetPlayerState());
	FKGEmoteBodyState Body = MakeBodyState();
	Body.Speed = 0.0f;   // we snap the acceptor into place below
	const EKGEmoteReject Reject = FKGEmoteRules::CanStart(*ClipB, Who, Body, Phase, Remaining);
	if (Reject != EKGEmoteReject::None)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_REJECT %s pawn=%s kind=%s reason=%s"), KGEmotePrivate::MachineName(World),
		       *Me->GetName(), *Def->Id.ToString(), *StaticEnum<EKGEmoteReject>()->GetNameStringByValue(int64(Reject)));
		return Reject;
	}
	if (Partner.IsActive())
	{
		ServerPartnerEnd(EKGEmoteStop::Replaced);
	}
	// Align: the acceptor steps to Distance in front of the offerer, both turn to face each other.
	FVector MyLoc;
	float TheirYaw;
	float MyYaw;
	FKGPartnerRules::Align(Them->GetActorLocation(), Me->GetActorLocation(), Def->Distance, MyLoc, TheirYaw, MyYaw);
	auto Face = [](AKGCharacter* Char, float Yaw)
	{
		const FRotator Rot(0.0f, Yaw, 0.0f);
		Char->SetActorRotation(Rot);
		if (AController* C = Char->GetController())
		{
			C->SetControlRotation(Rot);
			if (APlayerController* PC = Cast<APlayerController>(C))
			{
				PC->ClientSetRotation(Rot, false);
			}
		}
	};
	Me->TeleportTo(MyLoc, FRotator(0.0f, MyYaw, 0.0f), false, true);
	Face(Me, MyYaw);
	Face(Them, TheirYaw);

	const uint8 Serial = Offerer->Partner.Serial;
	uint8 Result = 0;
	if (Def->Kind == EKGPartnerKind::RockPaperScissors)
	{
		Result = FKGPartnerRules::RollRps(KGEmotePrivate::MatchSeedOf(World), Serial);
	}
	else if (Def->Kind == EKGPartnerKind::DanceOff)
	{
		Result = FKGPartnerRules::RollDanceOff(KGEmotePrivate::MatchSeedOf(World), Serial);
	}
	FKGPartnerState Mine;
	Mine.Kind = static_cast<uint8>(Def->Kind);
	Mine.Stage = static_cast<uint8>(EKGPartnerStage::Playing);
	Mine.Partner = Them->GetPlayerState();
	Mine.Serial = Serial;
	Mine.Result = Result;
	Mine.bInitiator = 0;
	FKGPartnerState Theirs = Mine;
	Theirs.Partner = Me->GetPlayerState();
	Theirs.bInitiator = 1;
	PartnerElapsed = 0.0f;
	Offerer->PartnerElapsed = 0.0f;
	SetPartnerState(Mine);
	Offerer->SetPartnerState(Theirs);
	Offerer->ServerTryStart(Def->EmoteA, true);
	ServerTryStart(Def->EmoteB, true);
	UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_START %s kind=%s a=%s b=%s serial=%d dist=%.0f result=%d"), KGEmotePrivate::MachineName(World),
	       *Def->Id.ToString(), *Them->GetName(), *Me->GetName(), Serial, FVector::Dist2D(Them->GetActorLocation(), Me->GetActorLocation()), Result);
	return EKGEmoteReject::None;
}

void UKGEmoteComponent::ServerPartnerEnd(EKGEmoteStop Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Partner.IsActive())
	{
		return;
	}
	UKGEmoteComponent* Other = Partner.Partner ? FindForPlayer(Partner.Partner) : nullptr;
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_STOP %s pawn=%s kind=%s stage=%s reason=%s"), KGEmotePrivate::MachineName(GetWorld()),
	       *GetNameSafe(GetOwner()), Def ? *Def->Id.ToString() : TEXT("?"), KGEmotePrivate::StageName(Partner.Stage),
	       KGEmotePrivate::StopName(Reason));
	const bool bWasPlaying = Partner.GetStage() == EKGPartnerStage::Playing;
	FKGPartnerState Cleared;
	Cleared.Serial = Partner.Serial;
	SetPartnerState(Cleared);
	if (bWasPlaying && Playback.Emote != 0)
	{
		ServerStop(EKGEmoteStop::Requested);   // our own clip; Requested does not recurse into the pairing (already cleared)
	}
	const APlayerState* MyPS = GetCharacter() ? GetCharacter()->GetPlayerState() : nullptr;
	if (Other && Other->Partner.IsActive() && Other->Partner.Partner == MyPS)
	{
		Other->ServerPartnerEnd(Reason);
	}
}

void UKGEmoteComponent::PartnerRevealResult()
{
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	AKGCharacter* Me = GetCharacter();
	if (!Def || !Me || !Partner.bInitiator)
	{
		return;
	}
	if (UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(Me->GetPlayerState()))
	{
		const TCHAR* Emoji = Def->Kind == EKGPartnerKind::RockPaperScissors ? TEXT("exclamation") : TEXT("crown");
		Chat->ServerSay(EKGChatChannel::Nearby, GetPartnerResultText(), EKGChatFlags::Action, FKGEmoji::Find(Emoji));
	}
}

void UKGEmoteComponent::TickPartnerServer(float DeltaTime)
{
	if (!Partner.IsActive())
	{
		return;
	}
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	AKGCharacter* Me = GetCharacter();
	if (!Def || !Me || Me->IsDead())
	{
		ServerPartnerEnd(EKGEmoteStop::Died);
		return;
	}
	PartnerElapsed += DeltaTime;
	switch (Partner.GetStage())
	{
	case EKGPartnerStage::Offering:
		if (PartnerElapsed >= FKGPartnerRules::OfferSeconds)
		{
			ServerPartnerEnd(EKGEmoteStop::Finished);
		}
		break;
	case EKGPartnerStage::Playing:
	{
		AKGCharacter* Them = Partner.Partner ? Cast<AKGCharacter>(Partner.Partner->GetPawn()) : nullptr;
		if (!Them || Them->IsDead())
		{
			ServerPartnerEnd(EKGEmoteStop::Died);
			return;
		}
		if (Partner.bInitiator && PartnerElapsed >= Def->PlaySeconds)
		{
			UKGEmoteComponent* Other = FindForPlayer(Partner.Partner);
			if (Def->IsResolved())
			{
				FKGPartnerState S = Partner;
				S.Stage = static_cast<uint8>(EKGPartnerStage::Result);
				PartnerElapsed = 0.0f;
				SetPartnerState(S);
				if (Other && Other->Partner.IsActive())
				{
					FKGPartnerState O = Other->Partner;
					O.Stage = S.Stage;
					Other->PartnerElapsed = 0.0f;
					Other->SetPartnerState(O);
				}
				PartnerRevealResult();
			}
			else
			{
				ServerPartnerEnd(EKGEmoteStop::Finished);
			}
		}
		break;
	}
	case EKGPartnerStage::Result:
		if (Partner.bInitiator && PartnerElapsed >= Def->ResultSeconds)
		{
			ServerPartnerEnd(EKGEmoteStop::Finished);
		}
		break;
	default:
		break;
	}
}

void UKGEmoteComponent::OnRep_Partner()
{
	if (Partner.Serial == PartnerLoggedSerial && Partner.Stage == PartnerLoggedStage)
	{
		return;
	}
	PartnerLoggedSerial = Partner.Serial;
	PartnerLoggedStage = Partner.Stage;
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_REP %s pawn=%s kind=%s stage=%s partner=%s serial=%d initiator=%d"),
	       KGEmotePrivate::MachineName(GetWorld()), *GetNameSafe(GetOwner()), Def ? *Def->Id.ToString() : TEXT("none"),
	       KGEmotePrivate::StageName(Partner.Stage), Partner.Partner ? *Partner.Partner->GetPlayerName() : TEXT("-"), Partner.Serial,
	       Partner.bInitiator);
	if (Def && Partner.GetStage() == EKGPartnerStage::Result && Partner.bInitiator)
	{
		// One line per machine per outcome: the network smoke compares them across host and client.
		UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_RESULT %s kind=%s serial=%d result=%d text=\"%s\""), KGEmotePrivate::MachineName(GetWorld()),
		       *Def->Id.ToString(), Partner.Serial, Partner.Result, *GetPartnerResultText());
	}
}

FString UKGEmoteComponent::GetPartnerResultText() const
{
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	const AKGCharacter* Me = GetCharacter();
	if (!Def || !Me || !Me->GetPlayerState())
	{
		return FString();
	}
	const FString A = Partner.bInitiator ? Me->GetPlayerState()->GetPlayerName() : (Partner.Partner ? Partner.Partner->GetPlayerName() : TEXT("?"));
	const FString B = Partner.bInitiator ? (Partner.Partner ? Partner.Partner->GetPlayerName() : TEXT("?")) : Me->GetPlayerState()->GetPlayerName();
	if (Def->Kind == EKGPartnerKind::RockPaperScissors)
	{
		uint8 PA, PB, Winner;
		FKGPartnerRules::UnpackRps(Partner.Result, PA, PB, Winner);
		if (Winner == 0)
		{
			return FString::Printf(TEXT("%s and %s both throw %s - a draw!"), *A, *B, FKGPartnerRules::RpsName(PA));
		}
		const FString& W = Winner == 1 ? A : B;
		return FString::Printf(TEXT("%s: %s, %s: %s - %s wins!"), *A, FKGPartnerRules::RpsName(PA), *B, FKGPartnerRules::RpsName(PB), *W);
	}
	if (Def->Kind == EKGPartnerKind::DanceOff)
	{
		return FString::Printf(TEXT("dance-off: the crowd goes with %s!"), Partner.Result == 2 ? *B : *A);
	}
	return FString();
}

FText UKGEmoteComponent::GetPartnerPrompt(bool bForOwner) const
{
	const FKGPartnerEmoteDef* Def = GetPartnerDef();
	if (!Def)
	{
		return FText::GetEmpty();
	}
	switch (Partner.GetStage())
	{
	case EKGPartnerStage::Offering:
		return bForOwner ? FText::Format(NSLOCTEXT("KGEmote", "PartnerWaiting", "{0}: waiting for a partner (they press E)"), Def->DisplayName)
		                 : FText::Format(NSLOCTEXT("KGEmote", "PartnerAccept", "E  {0}?"), Def->DisplayName);
	case EKGPartnerStage::Playing:
		return Def->DisplayName;
	case EKGPartnerStage::Result:
		return FText::FromString(GetPartnerResultText());
	default:
		return FText::GetEmpty();
	}
}

void UKGEmoteComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetOwnerSeesBody(false);
	SetBodyYawFree(false);
	Super::EndPlay(EndPlayReason);
}
