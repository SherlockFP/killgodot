#include "Chat/KGChatSubsystem.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGChatRules.h"
#include "Chat/KGChatUI.h"
#include "Chat/KGEmoji.h"
#include "Core/KGGameState.h"
#include "Core/KGLobbyState.h"
#include "Core/KGPlayerController.h"
#include "Core/KGPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/KGInventoryUI.h"
#include "UI/Menu/SKGLobbyRoom.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "KGChat"

namespace KGChatSubsystemPrivate
{
	TAutoConsoleVariable<float> CVarWheelSensitivity(TEXT("kg.Chat.WheelSensitivity"), 10.0f,
	                                                 TEXT("Reaction wheel: mouse delta multiplier."));
}

/** Last seen public state per local player (to turn replicated changes into crier lines). */
struct FKGChatMirror
{
	bool bInit = false;
	EKGPhase Phase = EKGPhase::Migrating;
	EKGTrialStage Stage = EKGTrialStage::Defense;
	FString Announcement;
	EKGLifeState Life = EKGLifeState::Alive;
	TMap<TWeakObjectPtr<AKGPlayerState>, TWeakObjectPtr<AKGPlayerState>> Accuse;
	TMap<TWeakObjectPtr<AKGPlayerState>, uint8> Verdict;
};

bool UKGChatSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The pre-game lobby screen (UI/Menu/SKGLobbyRoom) shows the chat through this hook.
	SKGLobbyRoom::ChatPanelFactory() = [](TWeakObjectPtr<APlayerController> Owner) -> TSharedRef<SWidget>
	{
		return FKGChatUI::MakeLobbyPanel(Owner.Get());
	};
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKGChatSubsystem::HandleActorSpawned));
}

void UKGChatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
		FKGChatUI::RemoveForWorld(World);
		UKGChatComponent::OnReaction().Remove(SmokeReactionHandle);
	}
	Pending.Reset();
	Mirrors.Reset();
	Super::Deinitialize();
}

TStatId UKGChatSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGChatSubsystem, STATGROUP_Tickables);
}

void UKGChatSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (APlayerState* PS = Cast<AKGPlayerState>(Actor))
	{
		Pending.Add(PS);   // next tick: the player state may still be mid-construction here
	}
}

void UKGChatSubsystem::EnsureChatComponent(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || PlayerState->IsActorBeingDestroyed() ||
	    !PlayerState->IsA<AKGPlayerState>() || PlayerState->FindComponentByClass<UKGChatComponent>())
	{
		return;
	}
	UKGChatComponent* Chat = NewObject<UKGChatComponent>(PlayerState, TEXT("KGChat"));
	Chat->SetIsReplicated(true);
	PlayerState->AddInstanceComponent(Chat);
	Chat->RegisterComponent();
}

void UKGChatSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		for (const TWeakObjectPtr<APlayerState>& PS : Pending)
		{
			EnsureChatComponent(PS.Get());
		}
		SweepSeconds += DeltaTime;
		if (SweepSeconds >= 1.0f)
		{
			SweepSeconds = 0.0f;
			if (const AGameStateBase* GS = World->GetGameState())
			{
				for (APlayerState* PS : GS->PlayerArray)
				{
					EnsureChatComponent(PS);
				}
			}
		}
	}
	Pending.Reset();

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (auto It = Mirrors.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
		{
			continue;
		}
		if (UKGChatComponent* Chat = UKGChatComponent::FindForController(PC))
		{
			FKGChatUI::Ensure(PC, Chat);
			const AKGLobbyState* Lobby = AKGLobbyState::Get(World);
			const bool bLobbyOpen = Lobby && !Lobby->HasStarted();
			FKGChatUI::SetSuppressed(PC, bLobbyOpen);
			MirrorPublicEvents(PC, Chat);
			if (!bLobbyOpen)
			{
				TickLocalPlayer(PC, Chat);
			}
			TickSmoke(PC, Chat, DeltaTime);
		}
	}
}

void UKGChatSubsystem::TickLocalPlayer(APlayerController* PC, UKGChatComponent* Chat)
{
	const AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
	const bool bTyping = FKGChatUI::IsTyping(PC);
	const bool bBlocked = bTyping || (KGPC && KGPC->IsPauseMenuOpen()) || FKGInventoryUI::IsOpen(PC);

	if (FKGChatUI::IsWheelOpen(PC))
	{
		float DX = 0.0f;
		float DY = 0.0f;
		PC->GetInputMouseDelta(DX, DY);
		const float K = KGChatSubsystemPrivate::CVarWheelSensitivity.GetValueOnGameThread();
		FKGChatUI::UpdateWheel(PC, FVector2D(DX, -DY) * K);
		static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		                              EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
		const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
		for (int32 i = 0; i < UE_ARRAY_COUNT(Digits) && !bShift; ++i)   // Shift+1..4 are the emote favourites
		{
			if (PC->WasInputKeyJustPressed(Digits[i]))
			{
				FKGChatUI::SelectWheelSlot(PC, i);
				FKGChatUI::CloseWheel(PC, true);
				return;
			}
		}
		if (bBlocked || !PC->IsInputKeyDown(EKeys::G))
		{
			FKGChatUI::CloseWheel(PC, !bBlocked);
		}
		return;
	}
	if (bBlocked)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::T))
	{
		FKGChatUI::OpenInput(PC);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Slash))
	{
		FKGChatUI::OpenInput(PC, TEXT("/"));
	}
	else if (PC->WasInputKeyJustPressed(EKeys::G))
	{
		const AKGGameState* GS = PC->GetWorld()->GetGameState<AKGGameState>();
		const bool bCan = FKGChatRules::CanReact(Chat->MakeLocalParticipant(), GS ? GS->GetPhase() : EKGPhase::Lobby,
		                                         GS ? GS->GetPhaseRemaining() : 0.0f);
		if (bCan)
		{
			FKGChatUI::OpenWheel(PC);
		}
		else
		{
			Chat->AddHint(LOCTEXT("NoReactNow", "No reactions now - the village is under curfew."));
		}
	}
}

void UKGChatSubsystem::TickSmoke(APlayerController* PC, UKGChatComponent* Chat, float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGChatSmoke"));
	if (!bSmoke || SmokeStep > 8)
	{
		return;
	}
	if (SmokeClock < 0.0f)
	{
		// Start once two humans are connected so both machines' traffic overlaps.
		int32 Humans = 0;
		if (const AGameStateBase* GS = PC->GetWorld()->GetGameState())
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				Humans += PS && !PS->IsABot() ? 1 : 0;
			}
		}
		if (Humans < 2)
		{
			return;
		}
		SmokeClock = 0.0f;
		SmokeReactionHandle = UKGChatComponent::OnReaction().AddLambda(
			[](APlayerController* Viewer, APlayerState* Sender, int32 Emoji)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_CHAT_REACT viewer=%s sender=%s emoji=%s"),
				       Viewer && Viewer->PlayerState ? *Viewer->PlayerState->GetPlayerName() : TEXT("?"),
				       Sender ? *Sender->GetPlayerName() : TEXT("?"), FKGEmoji::GetId(Emoji));
			});
	}
	SmokeClock += DeltaTime;
	const FString Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("?");
	struct FStep
	{
		float At;
		TFunction<void()> Run;
	};
	const TArray<FStep> Steps = {
		{6.0f, [&]() { Chat->SubmitInput(EKGChatChannel::All, FString::Printf(TEXT("hello from %s :skull: :) <3"), *Me)); }},
		{7.0f, [&]() { Chat->SubmitInput(EKGChatChannel::Nearby, TEXT("psst, nearby :eyes:")); }},
		{8.0f, [&]() { Chat->SubmitInput(EKGChatChannel::Team, TEXT("team line (must be refused: no team yet)")); }},
		{9.0f, [&]() { Chat->SubmitInput(EKGChatChannel::Dead, TEXT("dead line (must be refused: alive)")); }},
		{10.0f, [&]() { Chat->RequestReaction(FKGEmoji::Find(TEXT("laugh"))); }},
		{11.0f, [&]() { Chat->SubmitInput(EKGChatChannel::All, TEXT("/wave")); }},
		{12.0f, [&]()
		{
			for (int32 i = 0; i < 6; ++i)
			{
				Chat->SubmitInput(EKGChatChannel::All, FString::Printf(TEXT("spam %d"), i));   // expect rate limiting
			}
		}},
		{19.0f, [&]() { Chat->SubmitInput(EKGChatChannel::All, FString::Printf(TEXT("  %cevil   %s  "), static_cast<TCHAR>(0x202E), *Me)); }},
		{34.0f, [&]()
		{
			for (const FKGChatLine& Line : Chat->GetHistory())
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_CHAT me=%s [%s] %s: %s"), *Me,
				       *FKGChatRules::ChannelLabel(Line.Message.Channel).ToString(), *Line.Message.SenderName,
				       *FKGEmoji::ToPlainText(Line.Message.Text));
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_CHAT_DONE me=%s lines=%d"), *Me, Chat->GetHistory().Num());
		}},
	};
	while (Steps.IsValidIndex(SmokeStep) && SmokeClock >= Steps[SmokeStep].At)
	{
		Steps[SmokeStep].Run();
		++SmokeStep;
	}
#endif
}

void UKGChatSubsystem::MirrorPublicEvents(APlayerController* PC, UKGChatComponent* Chat)
{
	UWorld* World = PC->GetWorld();
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	const AKGPlayerState* Me = PC->GetPlayerState<AKGPlayerState>();
	if (!GS || !Me)
	{
		return;
	}
	TSharedPtr<FKGChatMirror>& Ptr = Mirrors.FindOrAdd(PC);
	if (!Ptr.IsValid())
	{
		Ptr = MakeShared<FKGChatMirror>();
	}
	FKGChatMirror& M = *Ptr;

	auto Crier = [Chat](const FText& Text)
	{
		FKGChatMessage Message;
		Message.Channel = EKGChatChannel::System;
		Message.Text = Text.ToString();
		Message.Flags = EKGChatFlags::Local;
		Chat->AddLocalLine(Message);
	};
	auto SnapshotVotes = [&M, GS]()
	{
		M.Accuse.Reset();
		M.Verdict.Reset();
		for (APlayerState* Raw : GS->PlayerArray)
		{
			if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
			{
				M.Accuse.Add(PS, PS->AccuseTarget.Get());
				M.Verdict.Add(PS, PS->Verdict);
			}
		}
	};

	const EKGPhase Phase = GS->GetPhase();
	const float Remaining = GS->GetPhaseRemaining();
	const FKGChatParticipant Self = Chat->MakeLocalParticipant();
	const bool bTeam = Self.bHasRole && FKGChatRules::IsTeamFaction(Self.Faction) && Self.Life == EKGLifeState::Alive;

	if (!M.bInit)
	{
		M.bInit = true;
		M.Phase = Phase;
		M.Life = Me->LifeState;
		M.Announcement = GS->Announcement;
		M.Stage = FKGChatRules::GetTrialStage(Remaining);
		SnapshotVotes();
		const AKGLobbyState* Lobby = AKGLobbyState::Get(World);
		Chat->AddHint(Lobby && !Lobby->HasStarted()
			              ? LOCTEXT("WelcomeLobby", "Lobby chat: say hello! Emojis work too - :) <3 :skull: or the smiley button.")
			              : LOCTEXT("Welcome", "Chat: Enter or T to talk, hold G to react, /help for commands."));
		return;
	}

	if (Phase != M.Phase)
	{
		M.Phase = Phase;
		M.Stage = FKGChatRules::GetTrialStage(Remaining);
		SnapshotVotes();
		const bool bGhost = Me->LifeState == EKGLifeState::Ghost;
		switch (Phase)
		{
		case EKGPhase::RoleReveal:
			if (bTeam)
			{
				Crier(LOCTEXT("PhaseRoleTeam", "Your fellow Impatient can read you on the IMPATIENT channel (Tab)."));
			}
			break;
		case EKGPhase::Dawn:
			if (!bGhost)
			{
				Crier(LOCTEXT("PhaseDawn", "Dawn. Listen to the crier - only nearby chat for now."));
			}
			break;
		case EKGPhase::Day:
			if (!bGhost)
			{
				Crier(FText::Format(LOCTEXT("PhaseDay", "Day {0}. Town chat is open."), FText::AsNumber(GS->GetDayIndex())));
			}
			break;
		case EKGPhase::Trial:
			if (!bGhost)
			{
				Crier(LOCTEXT("PhaseTrial", "Silence! The accused speaks in their defence."));
			}
			break;
		case EKGPhase::Night:
			if (!bGhost)
			{
				Crier(bTeam ? LOCTEXT("PhaseNightTeam", "Night falls. Town chat is closed - the IMPATIENT channel is open.")
				            : LOCTEXT("PhaseNight", "Night falls. The village goes quiet."));
			}
			break;
		case EKGPhase::Epilogue:
			Crier(LOCTEXT("PhaseEpilogue", "The match is over. Everyone can talk, the dead too."));
			break;
		default:
			break;
		}
	}
	else if (Phase == EKGPhase::Trial)
	{
		const EKGTrialStage Stage = FKGChatRules::GetTrialStage(Remaining);
		if (Stage != M.Stage)
		{
			M.Stage = Stage;
			if (Stage == EKGTrialStage::Judgement)
			{
				Crier(LOCTEXT("StageJudgement", "Judgement: everyone may speak. Vote [Y] guilty or [N] innocent."));
			}
			else if (Stage == EKGTrialStage::LastWords)
			{
				Crier(LOCTEXT("StageLastWords", "Last words of the accused."));
			}
		}
	}

	if (GS->Announcement != M.Announcement)
	{
		M.Announcement = GS->Announcement;
		if (!M.Announcement.IsEmpty() && GS->GetServerWorldTimeSeconds() < GS->AnnouncementUntil)
		{
			Crier(FText::FromString(M.Announcement));
		}
	}

	if (Me->LifeState != M.Life)
	{
		M.Life = Me->LifeState;
		if (M.Life == EKGLifeState::Ghost)
		{
			Chat->AddHint(LOCTEXT("YouDied", "You are a ghost now. Only other ghosts can read you (GHOSTS channel)."));
		}
		else if (M.Life == EKGLifeState::Revenant)
		{
			Chat->AddHint(LOCTEXT("Revenant", "You rose as a Revenant. Revenants cannot speak."));
		}
	}

	if (Phase == EKGPhase::Meeting || Phase == EKGPhase::Trial)
	{
		for (APlayerState* Raw : GS->PlayerArray)
		{
			AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (!PS)
			{
				continue;
			}
			AKGPlayerState* Target = PS->AccuseTarget.Get();
			TWeakObjectPtr<AKGPlayerState>* Prev = M.Accuse.Find(PS);
			if (Phase == EKGPhase::Meeting && (!Prev || Prev->Get() != Target))
			{
				if (Target)
				{
					Crier(FText::Format(LOCTEXT("Accuses", "{0} accuses {1}!"), FText::FromString(PS->GetPlayerName()),
					                    FText::FromString(Target->GetPlayerName())));
				}
				else if (Prev && Prev->IsValid())
				{
					Crier(FText::Format(LOCTEXT("Withdraws", "{0} withdraws the accusation."),
					                    FText::FromString(PS->GetPlayerName())));
				}
			}
			M.Accuse.Add(PS, Target);
			const uint8* PrevVerdict = M.Verdict.Find(PS);
			if (Phase == EKGPhase::Trial && PS->Verdict != 0 && (!PrevVerdict || *PrevVerdict != PS->Verdict))
			{
				Crier(FText::Format(LOCTEXT("Voted", "{0} has voted."), FText::FromString(PS->GetPlayerName())));
			}
			M.Verdict.Add(PS, PS->Verdict);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Dev console commands (kg.Chat.*). They run in the window's own world, so in a PIE client window they go through
// the real Server RPCs (unlike editor Python, which executes RPCs locally).
// ---------------------------------------------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
namespace KGChatConsole
{
	using FWorldArgs = FConsoleCommandWithWorldAndArgsDelegate;

	APlayerController* LocalPC(UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	UKGChatComponent* LocalChat(UWorld* World)
	{
		UKGChatComponent* Chat = UKGChatComponent::FindForController(LocalPC(World));
		if (!Chat)
		{
			UE_LOG(LogKillGodot, Warning, TEXT("kg.Chat.*: no chat component on the local player yet"));
		}
		return Chat;
	}

	/** The server world in this process (the window's own world if it is the host). */
	UWorld* AuthorityWorld(UWorld* World)
	{
		if (World && World->GetNetMode() != NM_Client)
		{
			return World;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Other = Context.World();
			if (Other && Other->GetNetMode() != NM_Client && Context.WorldType == EWorldType::PIE)
			{
				return Other;
			}
		}
		UE_LOG(LogKillGodot, Warning, TEXT("kg.Chat.*: no server world in this process"));
		return nullptr;
	}

	FString JoinFrom(const TArray<FString>& Args, int32 Start)
	{
		FString Out;
		for (int32 i = Start; i < Args.Num(); ++i)
		{
			Out += (i > Start ? TEXT(" ") : TEXT("")) + Args[i];
		}
		return Out;
	}

	FAutoConsoleCommandWithWorldAndArgs Say(
		TEXT("kg.Chat.Say"),
		TEXT("kg.Chat.Say <all|near|team|dead> <text...> : send a chat line through the server (\"/wave\" etc. work too)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGChatComponent* Chat = LocalChat(World);
			if (!Chat || Args.Num() < 2)
			{
				return;
			}
			EKGChatChannel Channel = EKGChatChannel::All;
			FKGChatRules::ParseChannelCommand(TEXT("/") + Args[0], Channel);
			Chat->SubmitInput(Channel, JoinFrom(Args, 1));
		}));

	FAutoConsoleCommandWithWorldAndArgs React(
		TEXT("kg.Chat.React"), TEXT("kg.Chat.React <emoji> : pop a reaction bubble over your head (server validated)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGChatComponent* Chat = LocalChat(World);
			const int32 Index = Args.Num() > 0 ? FKGEmoji::Find(Args[0].Replace(TEXT(":"), TEXT(""))) : INDEX_NONE;
			if (Chat && Index != INDEX_NONE)
			{
				Chat->RequestReaction(Index);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Emote(
		TEXT("kg.Chat.Emote"), TEXT("kg.Chat.Emote <wave|clap|point|...> : chat emote (bubble + narration + emote hook)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UKGChatComponent* Chat = LocalChat(World); Chat && Args.Num() > 0)
			{
				Chat->RequestEmote(FName(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Open(
		TEXT("kg.Chat.Open"), TEXT("kg.Chat.Open [prefill...] : open the chat input"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FKGChatUI::OpenInput(LocalPC(World), JoinFrom(Args, 0));
		}));

	FAutoConsoleCommandWithWorldAndArgs Close(
		TEXT("kg.Chat.Close"), TEXT("Close the chat input"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FKGChatUI::CloseInput(LocalPC(World));
		}));

	FAutoConsoleCommandWithWorldAndArgs Silence(
		TEXT("kg.Chat.Silence"), TEXT("kg.Chat.Silence <name> [1|0] : Blackmailer test - no written chat for that player"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* Server = AuthorityWorld(World);
			const AGameStateBase* GS = Server ? Server->GetGameState() : nullptr;
			if (!GS || Args.Num() < 1)
			{
				return;
			}
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (PS && PS->GetPlayerName().StartsWith(Args[0], ESearchCase::IgnoreCase))
				{
					if (UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(PS))
					{
						Chat->SetSilenced(Args.Num() < 2 || FCString::Atoi(*Args[1]) != 0);
						UE_LOG(LogKillGodot, Log, TEXT("kg.Chat.Silence %s -> %d"), *PS->GetPlayerName(),
						       Chat->IsSilenced() ? 1 : 0);
					}
					return;
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs System(
		TEXT("kg.Chat.System"), TEXT("kg.Chat.System <text...> : crier line to everyone (server)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGChatComponent::BroadcastSystem(AuthorityWorld(World), JoinFrom(Args, 0));
		}));

	FAutoConsoleCommandWithWorldAndArgs Dump(
		TEXT("kg.Chat.Dump"), TEXT("Log this window's chat history (KG_CHAT lines) for log-based checks"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const UKGChatComponent* Chat = LocalChat(World))
			{
				for (const FKGChatLine& Line : Chat->GetHistory())
				{
					UE_LOG(LogKillGodot, Log, TEXT("KG_CHAT %s [%s] %s: %s"), *GetNameSafe(LocalPC(World)),
					       *FKGChatRules::ChannelLabel(Line.Message.Channel).ToString(), *Line.Message.SenderName,
					       *FKGEmoji::ToPlainText(Line.Message.Text));
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Demo(
		TEXT("kg.Chat.Demo"), TEXT("Local visual check: sample lines on every channel + bubbles over you and the nearest player"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGChatComponent* Chat = LocalChat(World);
			APlayerController* PC = LocalPC(World);
			if (!Chat || !PC)
			{
				return;
			}
			auto Line = [Chat](EKGChatChannel Channel, const TCHAR* Name, const TCHAR* Text, uint8 Flags = 0)
			{
				FKGChatMessage M;
				M.Channel = Channel;
				M.SenderName = Name;
				M.ColorSeed = FKGChatRules::MakeColorSeed(FString(), Name);
				M.Text = FKGChatRules::Sanitize(Text);
				M.Flags = Flags | EKGChatFlags::Local;
				Chat->AddLocalLine(M);
			};
			Line(EKGChatChannel::System, TEXT(""), TEXT("Day 2. Town chat is open."));
			Line(EKGChatChannel::All, TEXT("Baker Nuri"), TEXT("good morning :) who rang the bell? :bell:"));
			Line(EKGChatChannel::All, TEXT("Fisher Riza"), TEXT("Kemal was near the harbour all night :sus: :eyes:"));
			Line(EKGChatChannel::Nearby, TEXT("Widow Hatice"), TEXT("psst... I saw a knife :knife: behind the tavern"));
			Line(EKGChatChannel::All, TEXT("Old Kemal"), TEXT("I was FISHING :fish: :anchor: ask anyone <3"));
			Line(EKGChatChannel::Team, TEXT("Smith Cemal"), TEXT("tonight: the lighthouse keeper :mask: :candle:"));
			Line(EKGChatChannel::Dead, TEXT("Priest Aurel"), TEXT("it was the smith!! :skull: :ghost: nobody can hear us"));
			Line(EKGChatChannel::Nearby, TEXT("Wanderer"), TEXT("waves"), EKGChatFlags::Action);
			Line(EKGChatChannel::All, TEXT("Innkeeper Mara"), TEXT("drinks are on me :mug: :D :clap: :fire:"));
			FKGChatUI::ShowBubble(PC, PC->PlayerState, FKGEmoji::Find(TEXT("laugh")));
			// Bubble over the nearest other pawn.
			APawn* Mine = PC->GetPawn();
			APawn* Best = nullptr;
			double BestDist = TNumericLimits<double>::Max();
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				if (*It != Mine && It->GetPlayerState() && Mine)
				{
					const double D = FVector::DistSquared(It->GetActorLocation(), Mine->GetActorLocation());
					if (D < BestDist)
					{
						BestDist = D;
						Best = *It;
					}
				}
			}
			if (Best)
			{
				FKGChatUI::ShowBubble(PC, Best->GetPlayerState(), FKGEmoji::Find(TEXT("sus")));
			}
		}));
}
#endif

#undef LOCTEXT_NAMESPACE
