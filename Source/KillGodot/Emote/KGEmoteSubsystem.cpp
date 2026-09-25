#include "Emote/KGEmoteSubsystem.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGChatUI.h"
#include "Core/KGPlayerController.h"
#include "Emote/KGEmoteCatalog.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/KGInventoryUI.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace KGEmoteSubsystemPrivate
{
	TAutoConsoleVariable<FString> CVarFavorites(TEXT("kg.Emote.Favorites"), TEXT("wave,point,dance,sit"),
		TEXT("Emotes on Shift+1..4 (comma separated emote ids, see kg.Emote list)."));

	/** Server side of the chat relay's emote gate: start the body animation (or refuse). */
	EKGChatReject Gate(APlayerState* Who, FName EmoteId)
	{
		UKGEmoteComponent* Emote = UKGEmoteComponent::FindForPlayer(Who);
		if (!Emote)
		{
			return EKGChatReject::None;   // no body to animate (lobby screen): the bubble + narration still go out
		}
		return FKGEmoteRules::ToChatReject(Emote->ServerTryStart(EmoteId));
	}
}

bool UKGEmoteSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGEmoteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Static and world-independent: bind once for the process (every world's chat relay uses the same gate).
	if (!UKGChatComponent::EmoteGate().IsBound())
	{
		UKGChatComponent::EmoteGate().BindStatic(&KGEmoteSubsystemPrivate::Gate);
	}
}

TStatId UKGEmoteSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGEmoteSubsystem, STATGROUP_Tickables);
}

TArray<FName> UKGEmoteSubsystem::GetFavorites()
{
	TArray<FString> Parts;
	KGEmoteSubsystemPrivate::CVarFavorites.GetValueOnGameThread().ParseIntoArray(Parts, TEXT(","));
	TArray<FName> Out;
	for (const FString& Part : Parts)
	{
		if (const FKGEmoteDef* Def = FKGEmoteCatalog::Find(Part))
		{
			Out.Add(Def->Id);
		}
		if (Out.Num() == 4)
		{
			break;
		}
	}
	return Out;
}

void UKGEmoteSubsystem::RequestEmote(APlayerController* PC, FName EmoteId)
{
	if (UKGChatComponent* Chat = UKGChatComponent::FindForController(PC))
	{
		Chat->RequestEmote(EmoteId);
	}
}

void UKGEmoteSubsystem::RequestStop(APlayerController* PC)
{
	if (UKGEmoteComponent* Emote = PC ? UKGEmoteComponent::FindForPlayer(PC->PlayerState) : nullptr)
	{
		Emote->RequestStop();
	}
}

void UKGEmoteSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->GetLocalPlayer())
		{
			TickLocalPlayer(PC);
			TickSmoke(PC, DeltaTime);
		}
	}
}

void UKGEmoteSubsystem::TickLocalPlayer(APlayerController* PC)
{
	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	if (FKGChatUI::IsTyping(PC) || FKGChatUI::IsWheelOpen(PC) || (KGPC && KGPC->IsPauseMenuOpen()) ||
	    FKGInventoryUI::IsOpen(PC))
	{
		return;
	}
	if (!PC->IsInputKeyDown(EKeys::LeftShift) && !PC->IsInputKeyDown(EKeys::RightShift))
	{
		return;
	}
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	const TArray<FName> Favorites = GetFavorites();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Digits) && i < Favorites.Num(); ++i)
	{
		if (!PC->WasInputKeyJustPressed(Digits[i]))
		{
			continue;
		}
		// The same favourite again while it plays = stop (sit down / stand up on one key).
		const UKGEmoteComponent* Emote = UKGEmoteComponent::FindForPlayer(PC->PlayerState);
		const FKGEmoteDef* Shown = Emote ? Emote->GetShownEmote() : nullptr;
		if (Shown && Shown->Id == Favorites[i])
		{
			RequestStop(PC);
		}
		else
		{
			RequestEmote(PC, Favorites[i]);
		}
		return;
	}
}

void UKGEmoteSubsystem::TickSmoke(APlayerController* PC, float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGEmoteSmoke"));
	if (!bSmoke || SmokeStep > 9)
	{
		return;
	}
	UWorld* World = PC->GetWorld();
	if (SmokeClock < 0.0f)
	{
		// Start once two humans are connected and both have bodies.
		int32 Bodies = 0;
		if (const AGameStateBase* GS = World->GetGameState())
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				Bodies += PS && !PS->IsABot() && Cast<AKGCharacter>(PS->GetPawn()) ? 1 : 0;
			}
		}
		if (Bodies < 2 || !UKGChatComponent::FindForController(PC))
		{
			return;
		}
		SmokeClock = 0.0f;
	}
	SmokeClock += DeltaTime;
	const FString Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("?");
	const TCHAR* Machine = World->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Host");
	auto Dump = [World, Machine, &Me](const TCHAR* Tag)
	{
		for (TActorIterator<AKGCharacter> It(World); It; ++It)
		{
			const UKGEmoteComponent* Emote = It->GetEmote();
			const APlayerState* PS = It->GetPlayerState();
			if (!Emote || !PS || PS->IsABot())
			{
				continue;
			}
			const FKGEmoteDef* Shown = Emote->GetShownEmote();
			UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_SEEN %s tag=%s viewer=%s body=%s local=%d replicated=%s shown=%s serial=%d cam=%.2f yawfree=%d"),
			       Machine, Tag, *Me, *PS->GetPlayerName(), It->IsLocallyControlled() ? 1 : 0,
			       *Emote->GetActiveEmoteId().ToString(), Shown ? *Shown->Id.ToString() : TEXT("None"), Emote->GetSerial(),
			       It->IsLocallyControlled() ? Emote->GetCameraAlpha() : 0.0f, It->bUseControllerRotationYaw ? 0 : 1);
		}
	};
	AKGCharacter* Body = Cast<AKGCharacter>(PC->GetPawn());
	struct FStep
	{
		float At;
		TFunction<void()> Run;
	};
	const TArray<FStep> Steps = {
		{4.0f, [&]() { RequestEmote(PC, TEXT("wave")); }},
		{6.0f, [&]() { Dump(TEXT("wave")); }},
		{8.0f, [&]() { RequestEmote(PC, TEXT("dance")); }},
		{11.0f, [&]() { Dump(TEXT("dance")); }},
		// 11.5 .. 13: walk forward (TickSmoke keeps pushing input below), which must cancel the dance.
		{14.0f, [&]() { Dump(TEXT("moved")); }},
		{15.0f, [&]() { RequestEmote(PC, TEXT("sit")); }},
		{17.5f, [&]() { if (Body) { Body->Attack(); } }},
		{19.0f, [&]() { Dump(TEXT("attacked")); }},
		{21.0f, [&]()
		{
			for (int32 i = 0; i < 6; ++i)
			{
				RequestEmote(PC, TEXT("clap"));   // rate limiting: only the first few may start
			}
		}},
		{24.0f, [&]() { UE_LOG(LogKillGodot, Log, TEXT("KG_EMOTE_DONE %s me=%s"), Machine, *Me); }},
	};
	if (Body && SmokeClock > 11.5f && SmokeClock < 13.0f)
	{
		Body->AddMovementInput(Body->GetActorForwardVector(), 1.0f);
	}
	while (Steps.IsValidIndex(SmokeStep) && SmokeClock >= Steps[SmokeStep].At)
	{
		Steps[SmokeStep].Run();
		++SmokeStep;
	}
#endif
}
