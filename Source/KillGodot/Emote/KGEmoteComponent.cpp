#include "Emote/KGEmoteComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraTypes.h"
#include "Character/KGBodyAnimInstance.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/KGGameState.h"
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
}

void UKGEmoteComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGEmoteComponent, Playback, Params);
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
	}
	TickPresentation(DeltaTime);
	TickOwnerView(DeltaTime);
}

void UKGEmoteComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetOwnerSeesBody(false);
	SetBodyYawFree(false);
	Super::EndPlay(EndPlayReason);
}
