#include "UI/Menu/KGMenuActions.h"

#include "Core/KGGameUserSettings.h"
#include "Core/KGMenuPlayerController.h"
#include "Core/KGPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "GameMapsSettings.h"
#include "HAL/IConsoleManager.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KillGodot.h"
#include "Misc/PackageName.h"
#include "Online/KGSessions.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "SocketSubsystem.h"
#include "UI/Menu/SKGMainMenu.h"

#define LOCTEXT_NAMESPACE "KGMenuActions"

namespace
{
	FText GKGLastNetworkError;
	/** Set by SetPendingMenuMessage: keeps its message when the connection drop that follows reports a failure. */
	bool bGKGMenuMessageSticky = false;

	/** State of the main menu opened over a running map with kg.MainMenu. */
	struct FKGMenuOverlayState
	{
		TWeakObjectPtr<APlayerController> Owner;
		TWeakObjectPtr<UWorld> World;
		FKGViewportWidget Entry;
	};
	FKGMenuOverlayState GKGMenuOverlay;

	void CloseMenuOverlay(bool bRestoreGameInput)
	{
		UGameViewportClient* Viewport = GKGMenuOverlay.Entry.Viewport.Get();
		KGMenu::RemoveFromViewport(GKGMenuOverlay.Entry);
		if (APlayerController* Owner = GKGMenuOverlay.Owner.Get(); Owner && bRestoreGameInput)
		{
			KGMenu::SetGameInput(Owner);
			Owner->FlushPressedKeys();
		}
		else
		{
			KGMenu::ResetViewportInput(Viewport);
		}
		GKGMenuOverlay = FKGMenuOverlayState();
	}

	void OpenMenuOverlay(APlayerController* PC)
	{
		if (!PC || GKGMenuOverlay.Entry.IsShown())
		{
			return;
		}
		static bool bCleanupHooked = false;
		if (!bCleanupHooked)
		{
			bCleanupHooked = true;
			FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool, bool)
			{
				if (GKGMenuOverlay.World.Get() == World)
				{
					CloseMenuOverlay(false);
				}
			});
		}

		TSharedRef<SKGMainMenu> Menu = SNew(SKGMainMenu)
			.OwningPlayer(PC)
			.bOverlay(true)
			.OnCloseRequested_Lambda([]() { CloseMenuOverlay(true); });
		GKGMenuOverlay.Owner = PC;
		GKGMenuOverlay.World = PC->GetWorld();
		GKGMenuOverlay.Entry = KGMenu::AddToViewport(PC, Menu, 50);
		KGMenu::SetMenuInput(PC, Menu->GetInitialFocus());
	}

	void HandleMainMenuCommand(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = KGMenu::GetLocalPlayerController(World);
		if (!PC)
		{
			UE_LOG(LogKillGodot, Warning, TEXT("kg.MainMenu: no local player controller in this world"));
			return;
		}
		if (Args.Num() > 0 && Args[0].Equals(TEXT("travel"), ESearchCase::IgnoreCase))
		{
			KGMenu::LeaveToMainMenu(PC);
			return;
		}
		// kg.MainMenu <page>: open the overlay straight on a page (handy for screenshots and UI checks).
		static const TMap<FString, EKGMainMenuPage> Pages = {
			{TEXT("home"), EKGMainMenuPage::Home}, {TEXT("play"), EKGMainMenuPage::Play},
			{TEXT("host"), EKGMainMenuPage::Host}, {TEXT("join"), EKGMainMenuPage::Join},
			{TEXT("cosmetics"), EKGMainMenuPage::Cosmetics}, {TEXT("settings"), EKGMainMenuPage::Settings}};
		if (const EKGMainMenuPage* Page = Args.Num() > 0 ? Pages.Find(Args[0].ToLower()) : nullptr)
		{
			if (!GKGMenuOverlay.Entry.IsShown())
			{
				OpenMenuOverlay(PC);
			}
			if (GKGMenuOverlay.Entry.IsShown())
			{
				StaticCastSharedPtr<SKGMainMenu>(GKGMenuOverlay.Entry.Widget)->OpenPage(*Page);
			}
			return;
		}
		if (GKGMenuOverlay.Entry.IsShown())
		{
			CloseMenuOverlay(true);
		}
		else if (AKGMenuPlayerController* MenuPC = Cast<AKGMenuPlayerController>(PC))
		{
			MenuPC->ShowMainMenu();
		}
		else
		{
			OpenMenuOverlay(PC);
		}
	}

	void HandlePauseMenuCommand(const TArray<FString>& Args, UWorld* World)
	{
		AKGPlayerController* PC = Cast<AKGPlayerController>(KGMenu::GetLocalPlayerController(World));
		if (!PC)
		{
			UE_LOG(LogKillGodot, Warning,
			       TEXT("kg.PauseMenu: the local controller is not an AKGPlayerController (set AKGGameMode::PlayerControllerClass)"));
			return;
		}
		PC->TogglePauseMenu();
	}

	/** Dev/QA: drives FKGSessions exactly like the lobby UI does, from the console (and from editor automation). */
	void HandleSessionCommand(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = KGMenu::GetLocalPlayerController(World);
		const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString();
		if (!PC || Verb.IsEmpty())
		{
			UE_LOG(LogKillGodot, Warning, TEXT("kg.Session host [name] [password] | find | findjoin [password] | join <index> [password] | end | probe"));
			return;
		}
		if (Verb == TEXT("host"))
		{
			const FKGMapEntry& Map = KGMenu::GetMaps()[0];
			FKGHostOptions Options;
			Options.Name = Args.Num() > 1 ? Args[1] : FString(TEXT("Test village"));
			Options.Password = Args.Num() > 2 ? Args[2] : FString();
			Options.MapPath = Map.MapPath;
			Options.MapTitle = Map.DisplayName.ToString();
			Options.MaxPlayers = 12;
			FKGSessions::Get().Host(PC, Options);
		}
		else if (Verb == TEXT("find"))
		{
			FKGSessions::Get().Find(PC, FKGOnSessionsFound::CreateLambda([](bool bSuccess, const TArray<FKGSessionRow>& Rows)
			{
				for (const FKGSessionRow& Row : Rows)
				{
					UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS row %d: '%s' host=%s map=%s %d/%d ping=%d %s%s"), Row.SearchIndex,
					       *Row.Name, *Row.HostName, *Row.MapTitle, Row.Players, Row.MaxPlayers, Row.PingMs,
					       Row.bInProgress ? TEXT("in-progress") : TEXT("lobby"), Row.bPassword ? TEXT(" locked") : TEXT(""));
				}
			}));
		}
		else if (Verb == TEXT("join") && Args.Num() > 1)
		{
			FKGSessions::Get().Join(PC, FCString::Atoi(*Args[1]), Args.Num() > 2 ? Args[2] : FString(),
			                        FKGOnSessionResult::CreateLambda([](bool bSuccess, const FText& Message)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS join result: %s %s"), bSuccess ? TEXT("ok") : TEXT("failed"), *Message.ToString());
			}));
		}
		else if (Verb == TEXT("end"))
		{
			FKGSessions::Get().EndSession(PC);
		}
		else if (Verb == TEXT("findjoin"))
		{
			// QA: find, then join the first listed game (with an optional password).
			const FString Password = Args.Num() > 1 ? Args[1] : FString();
			TWeakObjectPtr<APlayerController> WeakPC = PC;
			FKGSessions::Get().Find(PC, FKGOnSessionsFound::CreateLambda([WeakPC, Password](bool, const TArray<FKGSessionRow>& Rows)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS findjoin: %d game(s)"), Rows.Num());
				if (Rows.Num() > 0 && WeakPC.IsValid())
				{
					UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS findjoin -> '%s' %d/%d%s"), *Rows[0].Name, Rows[0].Players,
					       Rows[0].MaxPlayers, Rows[0].bPassword ? TEXT(" locked") : TEXT(""));
					FKGSessions::Get().Join(WeakPC.Get(), Rows[0].SearchIndex, Password,
					                        FKGOnSessionResult::CreateLambda([](bool bSuccess, const FText& Message)
					{
						UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS join result: %s %s"), bSuccess ? TEXT("ok") : TEXT("failed"),
						       *Message.ToString());
					}));
				}
			}));
		}
#if !UE_BUILD_SHIPPING
		else if (Verb == TEXT("probe"))
		{
			// QA: search the LAN from a second, independent Null subsystem instance in this process, so a single
			// editor can check what its own hosted match advertises (a host cannot search: that stops its beacon).
			IOnlineSubsystem* Probe = IOnlineSubsystem::Get(FName(TEXT("NULL:KGProbe")));
			const IOnlineSessionPtr ProbeSessions = Probe ? Probe->GetSessionInterface() : nullptr;
			if (!ProbeSessions.IsValid())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_SESSIONS probe: no Null subsystem instance"));
				return;
			}
			TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
			Search->bIsLanQuery = true;
			Search->MaxSearchResults = 50;
			TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
			TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> WeakSessions = ProbeSessions;
			*Handle = ProbeSessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateLambda(
				[Search, Handle, WeakSessions](bool bSuccess)
				{
					for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
					{
						const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
						FString Name, Host, Map;
						int32 Players = -1;
						bool bInProgress = false, bPassword = false;
						Settings.Get(FName(TEXT("KG_NAME")), Name);
						Settings.Get(FName(TEXT("KG_HOST")), Host);
						Settings.Get(FName(TEXT("KG_MAPTITLE")), Map);
						Settings.Get(FName(TEXT("KG_PLAYERS")), Players);
						Settings.Get(FName(TEXT("KG_INPROGRESS")), bInProgress);
						Settings.Get(FName(TEXT("KG_PASSWORD")), bPassword);
						FString Address;
						if (const IOnlineSessionPtr Pinned = WeakSessions.Pin())
						{
							Pinned->GetResolvedConnectString(Result, NAME_GamePort, Address);
						}
						UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS probe found '%s' host=%s map=%s %d/%d ping=%d %s%s at %s"), *Name,
						       *Host, *Map, Players, Settings.NumPublicConnections, Result.PingInMs,
						       bInProgress ? TEXT("in-progress") : TEXT("lobby"), bPassword ? TEXT(" locked") : TEXT(""), *Address);
					}
					UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS probe done: %s, %d result(s)"), bSuccess ? TEXT("ok") : TEXT("failed"),
					       Search->SearchResults.Num());
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Handle, WeakSessions](float)
					{
						if (const IOnlineSessionPtr Pinned = WeakSessions.Pin())
						{
							Pinned->ClearOnFindSessionsCompleteDelegate_Handle(*Handle);
						}
						return false;
					}));
				}));
			ProbeSessions->FindSessions(0, Search);
		}
#endif
	}

#if !UE_BUILD_SHIPPING
	/** QA: "kg.After 8 shot showui" runs a console command later in whatever game world is current then. */
	void HandleAfterCommand(const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num() < 2)
		{
			return;
		}
		const float Delay = FCString::Atof(*Args[0]);
		const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
		{
			UWorld* Target = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
				{
					Target = Context.World();
					break;
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_AFTER %s"), *Command);
			// Through the local player so viewport commands (shot, HighResShot...) work too.
			if (APlayerController* PC = KGMenu::GetLocalPlayerController(Target))
			{
				PC->ConsoleCommand(Command, true);
			}
			else
			{
				GEngine->Exec(Target, *Command);
			}
			return false;
		}), Delay);
	}

	FAutoConsoleCommandWithWorldAndArgs GKGAfterCommand(
		TEXT("kg.After"), TEXT("QA: kg.After <seconds> <console command> runs the command later."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleAfterCommand));
#endif

	FAutoConsoleCommandWithWorldAndArgs GKGSessionCommand(
		TEXT("kg.Session"),
		TEXT("Lobby sessions from the console: host [name] [password] | find | join <index> [password] | end"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleSessionCommand));

	FAutoConsoleCommandWithWorldAndArgs GKGMainMenuCommand(
		TEXT("kg.MainMenu"),
		TEXT("Toggle the KillGo main menu over the current map. 'kg.MainMenu travel' loads the front-end map instead; ")
		TEXT("'kg.MainMenu home|play|host|join|cosmetics|settings' opens the overlay on that page."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleMainMenuCommand));

	FAutoConsoleCommandWithWorldAndArgs GKGPauseMenuCommand(
		TEXT("kg.PauseMenu"),
		TEXT("Toggle the in-game pause menu (Esc stops PIE in the editor, so use this there)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandlePauseMenuCommand));
}

namespace KGMenu
{
	const TArray<FKGMapEntry>& GetMaps()
	{
		static TArray<FKGMapEntry> Maps;
		if (Maps.Num() == 0)
		{
			// Morrowmere v2 "The Amphitheatre Cove" (Docs/Level/Morrowmere_v2_Plan.md, Tools/Unreal/kg_build_v2_all.ps1) is the
			// default map: index 0 is what quick-play, kg.Session host and the lobby fall back to.
			FKGMapEntry Morrowmere;
			Morrowmere.DisplayName = LOCTEXT("MapMorrowmereV2", "Morrowmere");
			Morrowmere.Mode = LOCTEXT("ModeClassic", "Classic");
			Morrowmere.Description = LOCTEXT("MapMorrowmereV2Desc", "Terraced harbour town around a round basin \u00B7 6-20 players");
			Morrowmere.MapPath = TEXT("/Game/KillGodot/Maps/L_Morrowmere_v2");
			Morrowmere.bAvailable = true;
			Maps.Add(Morrowmere);

			// Map 2 "Storm Manor" (SPRINT-018: Docs/Level/StormManor_Plan.md, Tools/Unreal/kg_build_stormmanor_all.ps1).
			FKGMapEntry Manor;
			Manor.DisplayName = LOCTEXT("MapStormManor", "Storm Manor");
			Manor.Mode = LOCTEXT("ModeClassic", "Classic");
			Manor.Description = LOCTEXT("MapStormManorDesc", "A storm-locked manor on a rock off the coast \u00B7 6-20 players");
			Manor.MapPath = TEXT("/Game/KillGodot/Maps/L_StormManor");
			Manor.bAvailable = true;
			Maps.Add(Manor);

			// The first village (Tools/Unreal/kg_build_village.py), kept selectable.
			FKGMapEntry Classic;
			Classic.DisplayName = LOCTEXT("MapMorrowmereClassic", "Morrowmere (Classic)");
			Classic.Mode = LOCTEXT("ModeClassic", "Classic");
			Classic.Description = LOCTEXT("MapMorrowmereDesc", "Foggy coastal fishing village \u00B7 6-20 players");
			Classic.MapPath = TEXT("/Game/KillGodot/Maps/L_Morrowmere");
			Classic.bAvailable = true;
			Maps.Add(Classic);

			FKGMapEntry More;
			More.DisplayName = LOCTEXT("MapMore", "More maps");
			More.Mode = FText::GetEmpty();
			More.Description = LOCTEXT("MapMoreDesc", "New villages are on their way.");
			More.bAvailable = false;
			Maps.Add(More);
		}
		return Maps;
	}

	void HostGame(APlayerController* PC, const FString& MapPath, int32 InMaxPlayers)
	{
		if (!PC || MapPath.IsEmpty())
		{
			return;
		}
		const int32 Players = FMath::Clamp(InMaxPlayers, MinPlayers, MaxPlayers);
		if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
		{
			Settings->SetLastHostMaxPlayers(Players);
			Settings->PersistFrontEndMemory();
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_MENU host %s (listen, max %d players)"), *MapPath, Players);
		UGameplayStatics::OpenLevel(PC, FName(*MapPath), true, FString::Printf(TEXT("listen?KGLobby?MaxPlayers=%d"), Players));
	}

	FString NormalizeAddress(const FString& Input)
	{
		FString Address = Input.TrimStartAndEnd();
		if (Address.IsEmpty() || Address.Len() > 253 || Address.Contains(TEXT("://")))
		{
			return FString();
		}
		for (const TCHAR Char : Address)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('-') && Char != TEXT(':'))
			{
				return FString();
			}
		}
		FString Host = Address;
		FString Port;
		if (Address.Split(TEXT(":"), &Host, &Port, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			if (Host.IsEmpty() || Host.Contains(TEXT(":")) || !Port.IsNumeric())
			{
				return FString();
			}
			const int32 PortNumber = FCString::Atoi(*Port);
			if (PortNumber < 1 || PortNumber > 65535)
			{
				return FString();
			}
		}
		return Address;
	}

	bool JoinGame(APlayerController* PC, const FString& Address, FText& OutError)
	{
		const FString Target = NormalizeAddress(Address);
		if (Target.IsEmpty())
		{
			OutError = LOCTEXT("BadAddress", "That does not look like an address. Use an IP like 192.168.1.20, optionally with :port.");
			return false;
		}
		if (!PC)
		{
			OutError = LOCTEXT("NoPlayer", "No local player to travel with.");
			return false;
		}
		if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
		{
			Settings->SetLastJoinAddress(Target);
			Settings->PersistFrontEndMemory();
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_MENU join %s"), *Target);
		PC->ClientTravel(Target, TRAVEL_Absolute);
		return true;
	}

	void CancelJoin(APlayerController* PC)
	{
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		if (!World || !GEngine)
		{
			return;
		}
		if (FWorldContext* Context = GEngine->GetWorldContextFromWorld(World))
		{
			Context->TravelURL.Empty();
		}
		GEngine->CancelPending(World);
		UE_LOG(LogKillGodot, Log, TEXT("KG_MENU join cancelled"));
	}

	void LeaveToMainMenu(APlayerController* PC)
	{
		if (!PC)
		{
			return;
		}
		const FString Map = FPackageName::DoesPackageExist(MainMenuMap) ? FString(MainMenuMap) : UGameMapsSettings::GetGameDefaultMap();
		UE_LOG(LogKillGodot, Log, TEXT("KG_MENU leave to %s"), *Map);
		FKGSessions::Get().EndSession(PC);
		UGameplayStatics::OpenLevel(PC, FName(*Map), true, FString(TEXT("game=")) + MenuGameModeClass);
	}

	void QuitGame(APlayerController* PC)
	{
		FKGSessions::Get().EndSession(PC);
		UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
	}

	FString GetLocalAddress()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return FString();
		}
		bool bCanBindAll = false;
		const TSharedRef<FInternetAddr> Address = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
		return Address->IsValid() ? Address->ToString(false) : FString();
	}

	void RegisterNetworkErrorHooks()
	{
		static bool bRegistered = false;
		if (bRegistered || !GEngine)
		{
			return;
		}
		bRegistered = true;
		GEngine->OnNetworkFailure().AddLambda([](UWorld* World, UNetDriver*, ENetworkFailure::Type Type, const FString& Message)
		{
			const FString Reason = Message.IsEmpty() ? FString(ENetworkFailure::ToString(Type)) : Message;
			if (!bGKGMenuMessageSticky)
			{
				GKGLastNetworkError = Type == ENetworkFailure::PendingConnectionFailure
					? FText::Format(LOCTEXT("JoinFailure", "Could not join: {0}"), FText::FromString(Reason))
					: FText::Format(LOCTEXT("NetworkFailure", "Connection lost: {0}"), FText::FromString(Reason));
			}
			FKGSessions::Get().EndSession(World);
			UE_LOG(LogKillGodot, Warning, TEXT("KG_MENU network failure: %s"), *Reason);
		});
		GEngine->OnTravelFailure().AddLambda([](UWorld* World, ETravelFailure::Type Type, const FString& Message)
		{
			const FString Reason = Message.IsEmpty() ? FString(ETravelFailure::ToString(Type)) : Message;
			FKGSessions::Get().EndSession(World);
			GKGLastNetworkError = FText::Format(LOCTEXT("TravelFailure", "Could not join: {0}"), FText::FromString(Reason));
			UE_LOG(LogKillGodot, Warning, TEXT("KG_MENU travel failure: %s"), *Reason);
		});
	}

	void SetPendingMenuMessage(const FText& Message)
	{
		GKGLastNetworkError = Message;
		bGKGMenuMessageSticky = true;
	}

	FText ConsumeLastNetworkError()
	{
		FText Error = GKGLastNetworkError;
		GKGLastNetworkError = FText::GetEmpty();
		bGKGMenuMessageSticky = false;
		return Error;
	}

	FKGViewportWidget AddToViewport(APlayerController* PC, const TSharedRef<SWidget>& Widget, int32 ZOrder)
	{
		FKGViewportWidget Entry;
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		if (!Viewport)
		{
			return Entry;
		}
		Viewport->AddViewportWidgetContent(Widget, ZOrder);
		Entry.Viewport = Viewport;
		Entry.Widget = Widget;
		return Entry;
	}

	void RemoveFromViewport(FKGViewportWidget& Entry)
	{
		if (UGameViewportClient* Viewport = Entry.Viewport.Get(); Viewport && Entry.Widget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(Entry.Widget.ToSharedRef());
		}
		Entry = FKGViewportWidget();
	}

	void SetMenuInput(APlayerController* PC, const TSharedPtr<SWidget>& Focus)
	{
		if (!PC)
		{
			return;
		}
		FInputModeUIOnly Mode;
		if (Focus.IsValid())
		{
			Mode.SetWidgetToFocus(Focus);
		}
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}

	void SetGameInput(APlayerController* PC)
	{
		if (!PC)
		{
			return;
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}

	void ResetViewportInput(UGameViewportClient* Viewport)
	{
		if (!Viewport)
		{
			return;
		}
		const UInputSettings* InputSettings = GetDefault<UInputSettings>();
		Viewport->SetIgnoreInput(false);
		Viewport->SetMouseCaptureMode(InputSettings->DefaultViewportMouseCaptureMode);
		Viewport->SetMouseLockMode(InputSettings->DefaultViewportMouseLockMode);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}

	void FocusWidget(const TSharedPtr<SWidget>& Widget)
	{
		if (Widget.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
		}
	}

	APlayerController* GetLocalPlayerController(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController())
			{
				return PC;
			}
		}
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
