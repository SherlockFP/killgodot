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
			TickPartnerSmoke(PC, DeltaTime);
		}
	}
}

void UKGEmoteSubsystem::TickPartnerSmoke(APlayerController* PC, float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGPartnerSmoke"));
	if (!bSmoke || PartnerStep > 20)
	{
		return;
	}
	UWorld* World = PC->GetWorld();
	const bool bHost = World->GetNetMode() != NM_Client;
	APlayerState* Other = nullptr;
	int32 Bodies = 0;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS && !PS->IsABot() && Cast<AKGCharacter>(PS->GetPawn()))
			{
				++Bodies;
				if (PS != PC->PlayerState)
				{
					Other = PS;
				}
			}
		}
	}
	if (PartnerClock < 0.0f)
	{
		if (Bodies < 2 || !Other || !UKGChatComponent::FindForController(PC))
		{
			return;
		}
		PartnerClock = 0.0f;
	}
	PartnerClock += DeltaTime;
	const TCHAR* Machine = bHost ? TEXT("Host") : TEXT("Client");
	const FString Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("?");
	AKGCharacter* MyBody = Cast<AKGCharacter>(PC->GetPawn());
	AKGCharacter* TheirBody = Cast<AKGCharacter>(Other->GetPawn());
	UKGEmoteComponent* Mine = MyBody ? MyBody->GetEmote() : nullptr;
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
			const FKGPartnerState& P = Emote->GetPartner();
			const FKGPartnerEmoteDef* Def = Emote->GetPartnerDef();
			const FKGEmoteDef* Shown = Emote->GetShownEmote();
			UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_SEEN %s tag=%s viewer=%s body=%s local=%d stage=%d kind=%s partner=%s result=%d shown=%s yaw=%.0f loc=%s"),
			       Machine, Tag, *Me, *PS->GetPlayerName(), It->IsLocallyControlled() ? 1 : 0, P.Stage, Def ? *Def->Id.ToString() : TEXT("none"),
			       P.Partner ? *P.Partner->GetPlayerName() : TEXT("-"), P.Result, Shown ? *Shown->Id.ToString() : TEXT("None"),
			       It->GetActorRotation().Yaw, *It->GetActorLocation().ToCompactString());
		}
	};
	auto HostOffer = [&](const TCHAR* Id)
	{
		if (bHost && Mine)
		{
			Mine->ServerPartnerOffer(Id, true);
		}
	};
	auto ClientAccept = [&]()
	{
		if (!bHost && Mine)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_SMOKE Client accept -> %d"), Mine->TryAcceptNearbyOffer() ? 1 : 0);
		}
	};
	struct FStep
	{
		float At;
		TFunction<void()> Run;
	};
	const TArray<FStep> Steps = {
		{2.0f, [&]()
		{
			if (bHost && MyBody && TheirBody)
			{
				TheirBody->TeleportTo(MyBody->GetActorLocation() + MyBody->GetActorForwardVector() * 150.0f, MyBody->GetActorRotation(), false, true);
			}
		}},
		{4.0f, [&]() { HostOffer(TEXT("highfive")); }},
		{6.0f, [&]() { ClientAccept(); }},
		{7.5f, [&]() { Dump(TEXT("highfive")); }},
		{11.0f, [&]() { HostOffer(TEXT("rps")); }},
		{13.0f, [&]() { ClientAccept(); }},
		{17.0f, [&]() { Dump(TEXT("rps")); }},
		{20.0f, [&]() { HostOffer(TEXT("handshake")); }},
		{22.0f, [&]() { ClientAccept(); }},
		{23.2f, [&]() { if (bHost && MyBody) { MyBody->Attack(); } }},
		{24.5f, [&]() { Dump(TEXT("cancelled")); }},
		{27.0f, [&]() { HostOffer(TEXT("danceoff")); }},
		{29.0f, [&]() { ClientAccept(); }},
		{31.5f, [&]() { Dump(TEXT("danceoff")); }},
		{34.0f, [&]() { UE_LOG(LogKillGodot, Log, TEXT("KG_PARTNER_DONE %s me=%s"), Machine, *Me); }},
	};
	while (Steps.IsValidIndex(PartnerStep) && PartnerClock >= Steps[PartnerStep].At)
	{
		Steps[PartnerStep].Run();
		++PartnerStep;
	}
	if (PartnerStep >= Steps.Num())
	{
		PartnerStep = 99;
	}
#endif
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
