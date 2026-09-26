#include "Voice/KGVoiceSubsystem.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGChatRules.h"
#include "Chat/KGChatUI.h"
#include "Core/KGGameState.h"
#include "Core/KGGameUserSettings.h"
#include "Core/KGPlayerController.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Inventory/KGInventoryUI.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Voice/KGVoiceCommands.h"
#include "Voice/KGVoiceComponent.h"
#include "Voice/KGVoiceUI.h"

#define LOCTEXT_NAMESPACE "KGVoice"

namespace KGVoiceSubsystemPrivate
{
	TAutoConsoleVariable<float> CVarWheelSensitivity(TEXT("kg.Voice.WheelSensitivity"), 10.0f,
		TEXT("Voice-command radials: mouse delta to virtual cursor scale."));
}

bool UKGVoiceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UKGVoiceSubsystem::Deinitialize()
{
	FKGVoiceUI::RemoveForWorld(GetWorld());
	Super::Deinitialize();
}

TStatId UKGVoiceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGVoiceSubsystem, STATGROUP_Tickables);
}

bool UKGVoiceSubsystem::IsSmoke()
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGVoiceSmoke"));
	return bSmoke;
#else
	return false;
#endif
}

void UKGVoiceSubsystem::RequestBark(APlayerController* PC, FName CommandId)
{
	if (UKGVoiceComponent* Voice = UKGVoiceComponent::FindForController(PC))
	{
		Voice->RequestBark(CommandId);
	}
}

void UKGVoiceSubsystem::EnsureVoiceComponent(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || PlayerState->IsActorBeingDestroyed() ||
	    !PlayerState->IsA<AKGPlayerState>() || PlayerState->FindComponentByClass<UKGVoiceComponent>())
	{
		return;
	}
	UKGVoiceComponent* Voice = NewObject<UKGVoiceComponent>(PlayerState, TEXT("KGVoice"));
	Voice->SetIsReplicated(true);
	PlayerState->AddInstanceComponent(Voice);
	Voice->RegisterComponent();
}

void UKGVoiceSubsystem::EnsureComponents()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || !GS->HasAuthority())
	{
		return;
	}
	// Cheap: only walk the list when it changed (joins, bots) or every ~2 s as a safety net.
	const bool bChanged = GS->PlayerArray.Num() != LastPlayerCount || (GFrameCounter % 120) == 0;
	if (!bChanged)
	{
		return;
	}
	LastPlayerCount = GS->PlayerArray.Num();
	for (APlayerState* PS : GS->PlayerArray)
	{
		EnsureVoiceComponent(PS);
	}
}

void UKGVoiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	EnsureComponents();
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		UKGVoiceComponent* Voice = PC && PC->IsLocalController() && PC->GetLocalPlayer() ? UKGVoiceComponent::FindForController(PC) : nullptr;
		if (!Voice)
		{
			continue;
		}
		FKGVoiceUI::Ensure(PC);
		TickLocalPlayer(PC, Voice);
		TickSmoke(PC, Voice, DeltaTime);
	}
}

void UKGVoiceSubsystem::TickLocalPlayer(APlayerController* PC, UKGVoiceComponent* Voice)
{
	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	const bool bTyping = FKGChatUI::IsTyping(PC);
	const bool bBlocked = bTyping || (KGPC && KGPC->IsPauseMenuOpen()) || FKGInventoryUI::IsOpen(PC) || FKGChatUI::IsWheelOpen(PC);

	// ---- microphone: push-to-talk (V) or open mic (kg.Voice.Tone and the smoke drive it themselves) ----
	if (!IsSmoke() && !Voice->IsSyntheticToneOn())
	{
		const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
		const bool bOpenMic = Settings && Settings->GetOpenMic();
		Voice->SetTransmitting(bOpenMic || (!bBlocked && PC->IsInputKeyDown(EKeys::V)));
	}

	// ---- voice-command radials ----
	const TArray<FKGVoiceMenuDef>& Menus = FKGVoiceCommandCatalog::GetMenus();
	if (FKGVoiceUI::IsWheelOpen(PC))
	{
		float DX = 0.0f;
		float DY = 0.0f;
		PC->GetInputMouseDelta(DX, DY);
		FKGVoiceUI::UpdateWheel(PC, FVector2D(DX, -DY) * KGVoiceSubsystemPrivate::CVarWheelSensitivity.GetValueOnGameThread());
		static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
		for (int32 i = 0; i < UE_ARRAY_COUNT(Digits); ++i)
		{
			if (PC->WasInputKeyJustPressed(Digits[i]))
			{
				FKGVoiceUI::SelectWheelSlot(PC, i);
				const int32 Cmd = FKGVoiceUI::CloseWheel(PC, true);
				if (const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Get(Cmd))
				{
					Voice->RequestBark(Def->Id);
				}
				return;
			}
		}
		const int32 Menu = FKGVoiceUI::GetOpenMenu(PC);
		const bool bHeld = Menus.IsValidIndex(Menu) && PC->IsInputKeyDown(Menus[Menu].Key);
		if (bBlocked || !bHeld)
		{
			const int32 Cmd = FKGVoiceUI::CloseWheel(PC, !bBlocked);
			if (const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Get(Cmd))
			{
				Voice->RequestBark(Def->Id);
			}
		}
		return;
	}
	if (bBlocked)
	{
		return;
	}
	for (int32 m = 0; m < Menus.Num(); ++m)
	{
		if (!PC->WasInputKeyJustPressed(Menus[m].Key))
		{
			continue;
		}
		UKGChatComponent* Chat = UKGChatComponent::FindForController(PC);
		const AKGGameState* GS = PC->GetWorld()->GetGameState<AKGGameState>();
		const bool bCan = Chat && FKGChatRules::CanReact(Chat->MakeLocalParticipant(), GS ? GS->GetPhase() : EKGPhase::Lobby,
		                                                 GS ? GS->GetPhaseRemaining() : 0.0f);
		if (bCan)
		{
			FKGVoiceUI::OpenWheel(PC, m);
		}
		else if (Chat)
		{
			Chat->AddHint(LOCTEXT("NoBarkNow", "Not now - the village is under curfew."));
		}
		break;
	}
}

// ---- the headless smoke (-KGVoiceSmoke) ---------------------------------------------------------------------------

void UKGVoiceSubsystem::TickSmoke(APlayerController* PC, UKGVoiceComponent* Voice, float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	if (!IsSmoke() || bSmokeDone)
	{
		return;
	}
	UWorld* World = PC->GetWorld();
	const AGameStateBase* GS = World->GetGameState();
	const bool bHost = World->GetNetMode() != NM_Client;
	// The other human (the client from the host's point of view and vice versa); identity does not depend on a body
	// (ghosts keep their player state and their relay after the death steps).
	APlayerState* Other = nullptr;
	int32 Bodies = 0;
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS || PS->IsABot())
			{
				continue;
			}
			Bodies += Cast<AKGCharacter>(PS->GetPawn()) ? 1 : 0;
			if (PS != PC->PlayerState && !Other)
			{
				Other = PS;
			}
		}
	}
	UKGVoiceComponent* OtherVoice = UKGVoiceComponent::FindForPlayer(Other);
	if (SmokeClock < 0.0f)
	{
		if (Bodies < 2 || !OtherVoice || !UKGChatComponent::FindForController(PC))
		{
			return;
		}
		SmokeClock = 0.0f;
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_SMOKE %s start me=%s other=%s"), bHost ? TEXT("Host") : TEXT("Client"),
		       *PC->PlayerState->GetPlayerName(), *Other->GetPlayerName());
	}
	if (!Other || !OtherVoice)
	{
		return;   // the other machine left
	}
	SmokeClock += DeltaTime;
	SmokeWindow += DeltaTime;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");

	// Every machine: tone out, and a per-second summary of what it heard from the other human.
	Voice->SetSyntheticTone(true);
	Voice->SetTransmitting(true);
	if (SmokeWindow >= 1.0f)
	{
		SmokeWindow = 0.0f;
		FKGVoiceHeard& H = Voice->GetHeardMap().FindOrAdd(Other->GetPlayerId());
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_HEARD %s tag=%s from=%s packets=%d maxgain=%.2f played=%d dropped=%d total=%d tx=%d codec=%s t=%.0f"),
		       Machine, *Voice->GetSmokeTag().ToString(), *Other->GetPlayerName(), H.PacketsWindow, H.MaxGainWindow, H.PlayedWindow,
		       H.DroppedMuted, H.Packets, Voice->GetTxPackets(), Voice->GetCodecName(), SmokeClock);
		H.PacketsWindow = 0;
		H.MaxGainWindow = 0.0f;
		H.PlayedWindow = 0;
		H.DroppedMuted = 0;
	}
	// Client-side reactions to the replicated step tag.
	if (UKGChatComponent* Chat = UKGChatComponent::FindForController(PC))
	{
		const FName Tag = Voice->GetSmokeTag();
		if (Tag == TEXT("muted") && !bSmokeMuted)
		{
			bSmokeMuted = true;
			Chat->SetMuted(Other->GetPlayerId(), true);
			UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_SMOKE %s muted %s"), Machine, *Other->GetPlayerName());
		}
		else if (Tag != TEXT("muted") && !Tag.IsNone() && bSmokeMuted)
		{
			bSmokeMuted = false;
			Chat->SetMuted(Other->GetPlayerId(), false);
		}
	}
	if (!bHost)
	{
		if (SmokeClock > 34.0f)
		{
			bSmokeDone = true;
			UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_DONE Client"));
		}
		return;
	}

	// ---- host: drive the steps ----
	AKGCharacter* Me = Cast<AKGCharacter>(PC->GetPawn());
	AKGCharacter* Them = Cast<AKGCharacter>(Other->GetPawn());
	APlayerController* TheirPC = Cast<APlayerController>(Other->GetOwner());
	auto Tag = [&](const TCHAR* Name)
	{
		Voice->SetSmokeTag(Name);
		OtherVoice->SetSmokeTag(Name);
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_STEP tag=%s t=%.1f"), Name, SmokeClock);
	};
	auto Place = [&](float DistanceCm)
	{
		if (Me && Them)
		{
			const FVector Loc = Me->GetActorLocation() + Me->GetActorForwardVector() * DistanceCm;
			Them->TeleportTo(Loc, Them->GetActorRotation(), false, true);
			UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_STEP placed client at %.0f cm (actual %.0f)"), DistanceCm,
			       FVector::Dist(Me->GetActorLocation(), Them->GetActorLocation()));
		}
	};
	auto Dev = [&](APlayerController* Who, const TCHAR* Line)
	{
		const FKGDevResult R = FKGDev::Execute({World, Who}, Line);
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_STEP dev '%s' -> %s %s"), Line, R.bOk ? TEXT("ok") : TEXT("FAILED"), *R.Message);
	};
	struct FStep
	{
		float At;
		TFunction<void()> Run;
	};
	const TArray<FStep> Steps = {
		{2.0f, [&]() { Place(300.0f); Tag(TEXT("near")); }},
		{6.0f, [&]() { Place(1650.0f); Tag(TEXT("mid")); }},
		{10.0f, [&]() { Place(4000.0f); Tag(TEXT("far")); }},
		{14.0f, [&]() { Place(300.0f); Tag(TEXT("muted")); }},
		{18.0f, [&]() { Place(4000.0f); Dev(PC, TEXT("Match.Phase Meeting")); Tag(TEXT("meeting")); }},
		{22.0f, [&]() { Dev(PC, TEXT("Match.Phase Day")); Place(300.0f); Dev(PC, TEXT("Me.Kill")); Tag(TEXT("ghostspk")); }},
		{26.0f, [&]() { Dev(TheirPC, TEXT("Me.Kill")); Tag(TEXT("ghostboth")); }},
		{31.0f, [&]() { bSmokeDone = true; UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_DONE Host")); }},
	};
	while (Steps.IsValidIndex(SmokeStep) && SmokeClock >= Steps[SmokeStep].At)
	{
		Steps[SmokeStep].Run();
		++SmokeStep;
	}
#endif
}

#undef LOCTEXT_NAMESPACE
