#include "Online/KGSessions.h"

#include "Core/KGGameState.h"
#include "Core/KGTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "KillGodot.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "UI/Menu/KGMenuActions.h"

#define LOCTEXT_NAMESPACE "KGSessions"

namespace
{
	// Custom session attributes (advertised with the session / lobby, readable in search results).
	const FName KGKeyName(TEXT("KG_NAME"));
	const FName KGKeyHost(TEXT("KG_HOST"));
	const FName KGKeyMap(TEXT("KG_MAP"));
	const FName KGKeyMapTitle(TEXT("KG_MAPTITLE"));
	const FName KGKeyPlayers(TEXT("KG_PLAYERS"));
	const FName KGKeyInProgress(TEXT("KG_INPROGRESS"));
	const FName KGKeyPassword(TEXT("KG_PASSWORD"));
	const FName KGKeyVersion(TEXT("KG_VERSION"));
	const FName KGKeyCode(TEXT("KG_CODE"));
	const FName KGKeyRegion(TEXT("KG_REGION"));
	const TCHAR* KGCodeAlphabet = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	constexpr int32 KGCodeLength = 6;
	const TCHAR* KGKeyword = TEXT("KillGodot");

	constexpr float KGSessionTickSeconds = 0.5f;
	constexpr float KGUpdateMinSeconds = 2.0f;

	UWorld* SessionWorld(const UObject* WorldContext)
	{
		return GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	IOnlineSubsystem* SessionSubsystem(const UObject* WorldContext)
	{
		UWorld* World = SessionWorld(WorldContext);
		return World ? Online::GetSubsystem(World) : nullptr;
	}

	IOnlineSessionPtr SessionInterface(const UObject* WorldContext)
	{
		IOnlineSubsystem* Subsystem = SessionSubsystem(WorldContext);
		return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
	}

	bool SessionIsLan(const UObject* WorldContext)
	{
		const IOnlineSubsystem* Subsystem = SessionSubsystem(WorldContext);
		return !Subsystem || Subsystem->GetSubsystemName() == NULL_SUBSYSTEM;
	}

	int32 LocalUserNumOf(const APlayerController* PC)
	{
		const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
	}

	/**
	 * Runs Fn on the next core tick. Completion lambdas unregister themselves through this: removing a delegate
	 * while it is being broadcast destroys the lambda (and its captures) while it is still running.
	 */
	void SessionDeferOnce(TFunction<void()> Fn)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Fn = MoveTemp(Fn)](float)
		{
			Fn();
			return false;
		}));
	}

	/** Logs the local user into the online service first when it needs it (EOS); Null logs in instantly. */
	void SessionEnsureLogin(const APlayerController* PC, TFunction<void(bool)> Then)
	{
		IOnlineSubsystem* Subsystem = SessionSubsystem(PC);
		const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
		const int32 UserNum = LocalUserNumOf(PC);
		if (!Identity.IsValid() || Identity->GetLoginStatus(UserNum) == ELoginStatus::LoggedIn)
		{
			Then(true);
			return;
		}
		TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
		TWeakPtr<IOnlineIdentity, ESPMode::ThreadSafe> WeakIdentity = Identity;
		*Handle = Identity->AddOnLoginCompleteDelegate_Handle(UserNum, FOnLoginCompleteDelegate::CreateLambda(
			[WeakIdentity, Handle, Then, UserNum](int32, bool bWasSuccessful, const FUniqueNetId&, const FString& Error)
			{
				SessionDeferOnce([WeakIdentity, Handle, UserNum]()
				{
					if (const IOnlineIdentityPtr Pinned = WeakIdentity.Pin())
					{
						Pinned->ClearOnLoginCompleteDelegate_Handle(UserNum, *Handle);
					}
				});
				if (!bWasSuccessful)
				{
					UE_LOG(LogKillGodot, Warning, TEXT("KG_SESSIONS login failed: %s"), *Error);
				}
				Then(bWasSuccessful);
			}));
		if (!Identity->AutoLogin(UserNum))
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(UserNum, *Handle);
			Then(false);
		}
	}

	FText JoinResultText(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return LOCTEXT("JoinFull", "That game is full.");
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return LOCTEXT("JoinGone", "That game is no longer open.");
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return LOCTEXT("JoinAddress", "Could not reach the host.");
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return LOCTEXT("JoinAlready", "You are already in a game.");
		default:
			return LOCTEXT("JoinUnknown", "Joining failed.");
		}
	}

	template <typename T>
	T SessionSetting(const FOnlineSessionSettings& Settings, FName Key, T Default)
	{
		T Value = Default;
		Settings.Get(Key, Value);
		return Value;
	}
}

struct FKGSessions::FContext
{
	TSharedPtr<FOnlineSessionSearch> Search;
	bool bSearching = false;

	/** Set by Host(): advertise as soon as the listen server is running. */
	TOptional<FKGHostOptions> PendingHost;
	/** Advertised options of the running hosted match. */
	TOptional<FKGHostOptions> Hosted;
	bool bCreating = false;
	bool bAdvertised = false;
	FString PasswordHash;

	int32 LastPlayers = -1;
	bool bLastInProgress = false;
	double LastUpdateTime = 0.0;
};

FKGSessions& FKGSessions::Get()
{
	static FKGSessions Instance;
	return Instance;
}

FKGSessions::FKGSessions()
{
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FKGSessions::Tick));
	FGameModeEvents::OnGameModePreLoginEvent().AddRaw(this, &FKGSessions::HandlePreLogin);
	// Search results hold session infos owned by online subsystem modules: release them before modules unload.
	FCoreDelegates::OnPreExit.AddLambda([this]()
	{
		Contexts.Empty();
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	});
}

TSharedRef<FKGSessions::FContext> FKGSessions::ContextFor(const UObject* WorldContext)
{
	UWorld* World = SessionWorld(WorldContext);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (const TSharedRef<FContext>* Found = Contexts.Find(GameInstance))
	{
		return *Found;
	}
	TSharedRef<FContext> Context = MakeShared<FContext>();
	if (GameInstance)
	{
		Contexts.Add(GameInstance, Context);
	}
	return Context;
}

TSharedPtr<FKGSessions::FContext> FKGSessions::FindContext(const UObject* WorldContext) const
{
	UWorld* World = SessionWorld(WorldContext);
	const TSharedRef<FContext>* Found = World ? Contexts.Find(World->GetGameInstance()) : nullptr;
	return Found ? TSharedPtr<FContext>(*Found) : nullptr;
}

FText FKGSessions::GetBackendName(const UObject* WorldContext) const
{
	const IOnlineSubsystem* Subsystem = SessionSubsystem(WorldContext);
	if (!Subsystem || !Subsystem->GetSessionInterface().IsValid())
	{
		return LOCTEXT("BackendOffline", "Offline");
	}
	if (Subsystem->GetSubsystemName() == NULL_SUBSYSTEM)
	{
		return LOCTEXT("BackendLan", "LAN");
	}
	if (Subsystem->GetSubsystemName() == EOS_SUBSYSTEM)
	{
		return LOCTEXT("BackendEos", "Epic Online Services");
	}
	return FText::FromName(Subsystem->GetSubsystemName());
}

FString FKGSessions::GetLocalPlayerName(const APlayerController* PC) const
{
	// Null "nicknames" are raw ids: use the OS user name there; real services have real display names.
	FString Name;
	if (!SessionIsLan(PC))
	{
		IOnlineSubsystem* Subsystem = SessionSubsystem(PC);
		const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
		Name = Identity.IsValid() ? Identity->GetPlayerNickname(LocalUserNumOf(PC)) : FString();
	}
	if (Name.IsEmpty())
	{
		Name = FPlatformProcess::UserName();
	}
	if (Name.IsEmpty())
	{
		Name = TEXT("Villager");
	}
	// Several PIE players share one OS user: tell them apart.
	const UWorld* World = SessionWorld(PC);
	if (World && World->WorldType == EWorldType::PIE && GEngine)
	{
		if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
		{
			Name += FString::Printf(TEXT(" #%d"), WorldContext->PIEInstance);
		}
	}
	return Name.Left(28);
}

FString FKGSessions::HashPassword(const FString& Password)
{
	return Password.IsEmpty() ? FString() : FMD5::HashAnsiString(*(FString(TEXT("KillGodot:")) + Password));
}

void FKGSessions::Host(APlayerController* PC, const FKGHostOptions& Options, FKGOnSessionResult OnDone)
{
	if (!PC)
	{
		OnDone.ExecuteIfBound(false, LOCTEXT("NoPlayer", "No local player."));
		return;
	}
	TSharedRef<FContext> Context = ContextFor(PC);
	EndSession(PC);
	FKGHostOptions Resolved = Options;
	Resolved.Code = Resolved.Code.IsEmpty() ? MakeJoinCode() : NormalizeJoinCode(Resolved.Code);
	Resolved.Region = Resolved.Region.IsEmpty() ? GuessLocalRegion() : Resolved.Region;
	Context->PendingHost = Resolved;
	Context->Hosted.Reset();
	Context->bAdvertised = false;
	Context->bCreating = false;
	Context->PasswordHash = HashPassword(Options.Password);
	Context->LastPlayers = -1;
	UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS host '%s' code %s region %s on %s (max %d, %s%s) via %s"), *Options.Name,
	       *Resolved.Code, *Resolved.Region, *Options.MapPath, Options.MaxPlayers, Options.bPrivate ? TEXT("private") : TEXT("public"),
	       Options.Password.IsEmpty() ? TEXT("") : TEXT(", password"), *GetBackendName(PC).ToString());
	// The listen server comes first: the advertised address/port is read from its net driver.
	KGMenu::HostGame(PC, Options.MapPath, Options.MaxPlayers);
	OnDone.ExecuteIfBound(true, FText::GetEmpty());
}

void FKGSessions::CreateHostedSession(UGameInstance* GameInstance, const TSharedRef<FContext>& Context)
{
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const IOnlineSessionPtr Sessions = SessionInterface(World);
	if (!Sessions.IsValid() || !Context->PendingHost.IsSet())
	{
		Context->PendingHost.Reset();
		return;
	}
	const FKGHostOptions Options = Context->PendingHost.GetValue();
	Context->PendingHost.Reset();
	Context->Hosted = Options; // known while CreateSession runs (the lobby reads name/code from it)
	Context->bCreating = true;

	const bool bLan = SessionIsLan(World);
	APlayerController* HostPC = GameInstance->GetFirstLocalPlayerController(World);
	const FString HostName = GetLocalPlayerName(HostPC);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Clamp(Options.MaxPlayers, KGMenu::MinPlayers, KGMenu::MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bLan;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = !Options.bPrivate;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = !Options.bPrivate;
	Settings.bUseLobbiesIfAvailable = true; // EOS: lobbies carry the P2P host address
	const EOnlineDataAdvertisementType::Type Advert = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	Settings.Set(KGKeyName, Options.Name, Advert);
	Settings.Set(KGKeyHost, HostName, Advert);
	Settings.Set(KGKeyMap, Options.MapPath, Advert);
	Settings.Set(KGKeyMapTitle, Options.MapTitle, Advert);
	Settings.Set(KGKeyPlayers, 1, Advert);
	Settings.Set(KGKeyInProgress, false, Advert);
	Settings.Set(KGKeyPassword, !Options.Password.IsEmpty(), Advert);
	Settings.Set(KGKeyVersion, GetDefault<UGeneralProjectSettings>()->ProjectVersion, Advert);
	Settings.Set(KGKeyCode, Options.Code, Advert);
	Settings.Set(KGKeyRegion, Options.Region, Advert);
	Settings.Set(SETTING_MAPNAME, FPackageName::GetShortName(Options.MapPath), Advert);
	Settings.Set(SEARCH_KEYWORDS, FString(KGKeyword), Advert);

	TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> WeakSessions = Sessions;
	TWeakPtr<FContext> WeakContext = Context;
	*Handle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateLambda(
		[WeakSessions, WeakContext, Handle, Options](FName, bool bWasSuccessful)
		{
			SessionDeferOnce([WeakSessions, Handle]()
			{
				if (const IOnlineSessionPtr Pinned = WeakSessions.Pin())
				{
					Pinned->ClearOnCreateSessionCompleteDelegate_Handle(*Handle);
				}
			});
			if (const TSharedPtr<FContext> Ctx = WeakContext.Pin())
			{
				Ctx->bCreating = false;
				Ctx->bAdvertised = bWasSuccessful;
				Ctx->Hosted = Options;
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS advertise '%s': %s"), *Options.Name,
			       bWasSuccessful ? TEXT("ok") : TEXT("FAILED (direct IP only)"));
		}));

	const int32 UserNum = LocalUserNumOf(HostPC);
	SessionEnsureLogin(HostPC, [WeakSessions, WeakContext, Handle, Settings, UserNum](bool)
	{
		const IOnlineSessionPtr Pinned = WeakSessions.Pin();
		if (Pinned.IsValid() && !Pinned->CreateSession(UserNum, NAME_GameSession, Settings))
		{
			Pinned->ClearOnCreateSessionCompleteDelegate_Handle(*Handle);
			if (const TSharedPtr<FContext> Ctx = WeakContext.Pin())
			{
				Ctx->bCreating = false;
			}
			UE_LOG(LogKillGodot, Warning, TEXT("KG_SESSIONS CreateSession refused (direct IP only)"));
		}
	});
}

void FKGSessions::Find(APlayerController* PC, FKGOnSessionsFound OnDone)
{
	const IOnlineSessionPtr Sessions = SessionInterface(PC);
	TSharedRef<FContext> Context = ContextFor(PC);
	if (!Sessions.IsValid() || !PC)
	{
		OnDone.ExecuteIfBound(false, TArray<FKGSessionRow>());
		return;
	}
	if (Context->bSearching)
	{
		return;
	}
	const bool bLan = SessionIsLan(PC);
	TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 200;
	Search->bIsLanQuery = bLan;
	Search->PingBucketSize = 50;
	if (!bLan)
	{
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	Context->Search = Search;
	Context->bSearching = true;

	TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> WeakSessions = Sessions;
	TWeakPtr<FContext> WeakContext = Context;
	*Handle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateLambda(
		[WeakSessions, WeakContext, Handle, Search, OnDone](bool bWasSuccessful)
		{
			SessionDeferOnce([WeakSessions, Handle]()
			{
				if (const IOnlineSessionPtr Pinned = WeakSessions.Pin())
				{
					Pinned->ClearOnFindSessionsCompleteDelegate_Handle(*Handle);
				}
			});
			const TSharedPtr<FContext> Ctx = WeakContext.Pin();
			if (Ctx.IsValid() && Ctx->Search != Search)
			{
				return; // cancelled or superseded search
			}
			if (Ctx.IsValid())
			{
				Ctx->bSearching = false;
			}
			TArray<FKGSessionRow> Rows;
			for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
			{
				const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
				const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
				FKGSessionRow Row;
				if (!Result.IsValid() || !Settings.Get(KGKeyName, Row.Name))
				{
					continue; // not a Kill Godot session
				}
				Row.SearchIndex = Index;
				Row.HostName = SessionSetting(Settings, KGKeyHost, Result.Session.OwningUserName);
				Row.MapPath = SessionSetting(Settings, KGKeyMap, FString());
				Row.MapTitle = SessionSetting(Settings, KGKeyMapTitle, FPackageName::GetShortName(Row.MapPath));
				Row.MaxPlayers = Settings.NumPublicConnections;
				Row.Players = SessionSetting(Settings, KGKeyPlayers,
				                             FMath::Max(0, Settings.NumPublicConnections - Result.Session.NumOpenPublicConnections));
				Row.bInProgress = SessionSetting(Settings, KGKeyInProgress, false);
				Row.bPassword = SessionSetting(Settings, KGKeyPassword, false);
				Row.Code = SessionSetting(Settings, KGKeyCode, FString());
				Row.Region = SessionSetting(Settings, KGKeyRegion, FString());
				Row.PingMs = Result.PingInMs >= 0 && Result.PingInMs < 9999 ? Result.PingInMs : -1;
				Rows.Add(Row);
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS find: %s, %d result(s), %d Kill Godot game(s)"),
			       bWasSuccessful ? TEXT("ok") : TEXT("failed"), Search->SearchResults.Num(), Rows.Num());
			OnDone.ExecuteIfBound(bWasSuccessful, Rows);
		}));

	const int32 UserNum = LocalUserNumOf(PC);
	SessionEnsureLogin(PC, [WeakSessions, WeakContext, Handle, Search, UserNum, OnDone](bool bLoggedIn)
	{
		const IOnlineSessionPtr Pinned = WeakSessions.Pin();
		if (!Pinned.IsValid() || !bLoggedIn || !Pinned->FindSessions(UserNum, Search))
		{
			if (Pinned.IsValid())
			{
				Pinned->ClearOnFindSessionsCompleteDelegate_Handle(*Handle);
			}
			if (const TSharedPtr<FContext> Ctx = WeakContext.Pin())
			{
				Ctx->bSearching = false;
			}
			OnDone.ExecuteIfBound(false, TArray<FKGSessionRow>());
		}
	});
}

void FKGSessions::Join(APlayerController* PC, int32 SearchIndex, const FString& Password, FKGOnSessionResult OnDone)
{
	const IOnlineSessionPtr Sessions = SessionInterface(PC);
	const TSharedRef<FContext> Context = ContextFor(PC);
	if (!Sessions.IsValid() || !PC || !Context->Search.IsValid() || !Context->Search->SearchResults.IsValidIndex(SearchIndex))
	{
		OnDone.ExecuteIfBound(false, LOCTEXT("JoinStale", "That game is no longer in the list. Refresh and try again."));
		return;
	}
	const FOnlineSessionSearchResult Result = Context->Search->SearchResults[SearchIndex];
	const FString PasswordHash = HashPassword(Password);
	TWeakObjectPtr<APlayerController> WeakPC = PC;
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> WeakSessions = Sessions;
	const int32 UserNum = LocalUserNumOf(PC);

	auto DoJoin = [WeakPC, WeakSessions, Result, PasswordHash, UserNum, OnDone]()
	{
		const IOnlineSessionPtr Pinned = WeakSessions.Pin();
		if (!Pinned.IsValid())
		{
			OnDone.ExecuteIfBound(false, LOCTEXT("JoinNoService", "Online sessions are unavailable."));
			return;
		}
		TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
		*Handle = Pinned->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateLambda(
			[WeakPC, WeakSessions, Handle, PasswordHash, OnDone](FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult)
			{
				const IOnlineSessionPtr Inner = WeakSessions.Pin();
				SessionDeferOnce([WeakSessions, Handle]()
				{
					if (const IOnlineSessionPtr Pinned = WeakSessions.Pin())
					{
						Pinned->ClearOnJoinSessionCompleteDelegate_Handle(*Handle);
					}
				});
				FString Url;
				if (JoinResult != EOnJoinSessionCompleteResult::Success || !Inner.IsValid() ||
					!Inner->GetResolvedConnectString(SessionName, Url))
				{
					UE_LOG(LogKillGodot, Warning, TEXT("KG_SESSIONS join failed: %s"), LexToString(JoinResult));
					if (Inner.IsValid())
					{
						Inner->DestroySession(SessionName);
					}
					OnDone.ExecuteIfBound(false, JoinResultText(JoinResult));
					return;
				}
				if (!PasswordHash.IsEmpty())
				{
					Url += FString::Printf(TEXT("?%s=%s"), FKGSessions::PasswordOption(), *PasswordHash);
				}
				UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS join -> %s"), *Url);
				if (APlayerController* Traveller = WeakPC.Get())
				{
					Traveller->ClientTravel(Url, TRAVEL_Absolute);
				}
				OnDone.ExecuteIfBound(true, FText::FromString(Url));
			}));
		if (!Pinned->JoinSession(UserNum, NAME_GameSession, Result))
		{
			Pinned->ClearOnJoinSessionCompleteDelegate_Handle(*Handle);
			OnDone.ExecuteIfBound(false, LOCTEXT("JoinRefused", "Joining failed."));
		}
	};

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		// A stale session (e.g. after a disconnect) blocks joining: clear it first.
		Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateLambda(
			[WeakPC, DoJoin](FName, bool)
			{
				if (WeakPC.IsValid())
				{
					SessionEnsureLogin(WeakPC.Get(), [DoJoin](bool) { DoJoin(); });
				}
			}));
		return;
	}
	SessionEnsureLogin(PC, [DoJoin](bool) { DoJoin(); });
}

int32 FKGSessions::PickQuickMatch(const TArray<FKGSessionRow>& Rows)
{
	int32 Best = INDEX_NONE;
	int32 BestScore = TNumericLimits<int32>::Lowest();
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FKGSessionRow& Row = Rows[Index];
		if (Row.bPassword || Row.IsFull())
		{
			continue;
		}
		const int32 Score = (Row.bInProgress ? 0 : 100000) + Row.Players * 1000 -
			FMath::Clamp(Row.PingMs < 0 ? 500 : Row.PingMs, 0, 999);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	return Best;
}

void FKGSessions::CancelFind(const UObject* WorldContext)
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	const IOnlineSessionPtr Sessions = SessionInterface(WorldContext);
	if (Context.IsValid() && Context->bSearching)
	{
		Context->bSearching = false;
		Context->Search.Reset(); // the pending completion sees a different search and stays silent
		if (Sessions.IsValid())
		{
			Sessions->CancelFindSessions();
		}
	}
}

void FKGSessions::EndSession(const UObject* WorldContext)
{
	if (const TSharedPtr<FContext> Context = FindContext(WorldContext))
	{
		Context->Hosted.Reset();
		Context->PendingHost.Reset();
		Context->bAdvertised = false;
		Context->PasswordHash.Reset();
	}
	const IOnlineSessionPtr Sessions = SessionInterface(WorldContext);
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS destroy session"));
		Sessions->DestroySession(NAME_GameSession);
	}
}

const FKGHostOptions* FKGSessions::GetHostOptions(const UObject* WorldContext) const
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	if (!Context.IsValid())
	{
		return nullptr;
	}
	return Context->Hosted.IsSet() ? &Context->Hosted.GetValue() : Context->PendingHost.GetPtrOrNull();
}

void FKGSessions::UpdateHostedMaxPlayers(const UObject* WorldContext, int32 MaxPlayers)
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	if (!Context.IsValid() || !Context->Hosted.IsSet())
	{
		return;
	}
	Context->Hosted->MaxPlayers = FMath::Clamp(MaxPlayers, KGMenu::MinPlayers, KGMenu::MaxPlayers);
	const IOnlineSessionPtr Sessions = SessionInterface(WorldContext);
	FOnlineSessionSettings* Settings = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (Settings && Context->bAdvertised)
	{
		Settings->NumPublicConnections = Context->Hosted->MaxPlayers;
		Sessions->UpdateSession(NAME_GameSession, *Settings, true);
		UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS update: max %d"), Context->Hosted->MaxPlayers);
	}
}

void FKGSessions::UpdateHostedMap(const UObject* WorldContext, const FString& MapPath, const FString& MapTitle)
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	if (!Context.IsValid() || !Context->Hosted.IsSet())
	{
		return;
	}
	Context->Hosted->MapPath = MapPath;
	Context->Hosted->MapTitle = MapTitle;
	const IOnlineSessionPtr Sessions = SessionInterface(WorldContext);
	FOnlineSessionSettings* Settings = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (Settings && Context->bAdvertised)
	{
		const EOnlineDataAdvertisementType::Type Advert = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
		Settings->Set(KGKeyMap, MapPath, Advert);
		Settings->Set(KGKeyMapTitle, MapTitle, Advert);
		Settings->Set(SETTING_MAPNAME, FPackageName::GetShortName(MapPath), Advert);
		Sessions->UpdateSession(NAME_GameSession, *Settings, true);
	}
}

FString FKGSessions::MakeJoinCode()
{
	const FGuid Guid = FGuid::NewGuid();
	uint64 Bits = ((uint64(Guid.A) << 32) | Guid.B) ^ ((uint64(Guid.C) << 32) | Guid.D);
	const int32 AlphabetLen = FCString::Strlen(KGCodeAlphabet);
	FString Code;
	for (int32 Index = 0; Index < KGCodeLength; ++Index)
	{
		Code.AppendChar(KGCodeAlphabet[Bits % AlphabetLen]);
		Bits /= AlphabetLen;
	}
	return Code;
}

FString FKGSessions::NormalizeJoinCode(const FString& Text)
{
	FString Code = Text.TrimStartAndEnd().ToUpper();
	Code.ReplaceInline(TEXT("-"), TEXT(""));
	Code.ReplaceInline(TEXT(" "), TEXT(""));
	return Code;
}

bool FKGSessions::LooksLikeJoinCode(const FString& Text)
{
	const FString Code = NormalizeJoinCode(Text);
	if (Code.Len() != KGCodeLength)
	{
		return false;
	}
	bool bHasLetter = false;
	for (const TCHAR Char : Code)
	{
		if (!FChar::IsAlnum(Char))
		{
			return false;
		}
		bHasLetter |= FChar::IsAlpha(Char);
	}
	return bHasLetter; // six digits is more likely a mistyped address/port
}

const TArray<FString>& FKGSessions::GetRegions()
{
	static const TArray<FString> Regions = {TEXT("EU"), TEXT("NA"), TEXT("SA"), TEXT("ME"), TEXT("AS"), TEXT("OC")};
	return Regions;
}

FString FKGSessions::GuessLocalRegion()
{
	// Coarse guess from the local UTC offset; the host can override it (FKGHostOptions::Region).
	const FTimespan Offset = FDateTime::Now() - FDateTime::UtcNow();
	const double Hours = Offset.GetTotalHours();
	if (Hours <= -6.5)
	{
		return TEXT("NA");
	}
	if (Hours < -1.5)
	{
		return TEXT("SA");
	}
	if (Hours < 3.5)
	{
		return TEXT("EU");
	}
	if (Hours < 5.0)
	{
		return TEXT("ME");
	}
	if (Hours < 9.5)
	{
		return TEXT("AS");
	}
	return TEXT("OC");
}

bool FKGSessions::IsSearching(const UObject* WorldContext) const
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	return Context.IsValid() && Context->bSearching;
}

FText FKGSessions::DescribeHostedSession(const UObject* WorldContext) const
{
	const TSharedPtr<FContext> Context = FindContext(WorldContext);
	if (!Context.IsValid() || (!Context->Hosted.IsSet() && !Context->bCreating && !Context->PendingHost.IsSet()))
	{
		return FText::GetEmpty();
	}
	if (!Context->Hosted.IsSet() || Context->bCreating)
	{
		return LOCTEXT("Advertising", "Opening the lobby...");
	}
	const FKGHostOptions& Options = Context->Hosted.GetValue();
	if (!Context->bAdvertised)
	{
		return LOCTEXT("NotAdvertised", "Not listed: friends join by IP");
	}
	if (Options.bPrivate)
	{
		return FText::Format(LOCTEXT("ListedPrivate", "\"{0}\" (private)"), FText::FromString(Options.Name));
	}
	return FText::Format(Options.Password.IsEmpty() ? LOCTEXT("ListedPublic", "Listed as \"{0}\"")
	                                                : LOCTEXT("ListedLocked", "Listed as \"{0}\" (password)"),
	                     FText::FromString(Options.Name));
}

bool FKGSessions::Tick(float DeltaTime)
{
	TickAccumulator += DeltaTime;
	if (TickAccumulator < KGSessionTickSeconds)
	{
		return true;
	}
	TickAccumulator = 0.0f;
	for (auto It = Contexts.CreateIterator(); It; ++It)
	{
		UGameInstance* GameInstance = It.Key().Get();
		if (!GameInstance)
		{
			It.RemoveCurrent();
			continue;
		}
		TickHost(GameInstance, It.Value().Get());
		if (It.Value()->PendingHost.IsSet() && !It.Value()->bCreating)
		{
			UWorld* World = GameInstance->GetWorld();
			const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
			if (World && World->HasBegunPlay() && World->GetNetMode() == NM_ListenServer && Driver)
			{
				CreateHostedSession(GameInstance, It.Value());
			}
		}
	}
	return true;
}

void FKGSessions::TickHost(UGameInstance* GameInstance, FContext& Context)
{
	if (!Context.Hosted.IsSet() || !Context.bAdvertised)
	{
		return;
	}
	UWorld* World = GameInstance->GetWorld();
	const IOnlineSessionPtr Sessions = SessionInterface(World);
	FOnlineSessionSettings* Settings = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (!World || !Settings || World->GetNetMode() != NM_ListenServer)
	{
		return;
	}
	int32 Players = 0;
	bool bInProgress = false;
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			Players += PlayerState && !PlayerState->IsABot() ? 1 : 0;
		}
		if (const AKGGameState* KGState = Cast<AKGGameState>(GameState))
		{
			bInProgress = KGState->GetPhase() != EKGPhase::Lobby && KGState->GetPhase() != EKGPhase::Warmup;
		}
	}
	const double Now = FPlatformTime::Seconds();
	if ((Players == Context.LastPlayers && bInProgress == Context.bLastInProgress) || Now - Context.LastUpdateTime < KGUpdateMinSeconds)
	{
		return;
	}
	Context.LastPlayers = Players;
	Context.bLastInProgress = bInProgress;
	Context.LastUpdateTime = Now;
	const EOnlineDataAdvertisementType::Type Advert = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	Settings->Set(KGKeyPlayers, Players, Advert);
	Settings->Set(KGKeyInProgress, bInProgress, Advert);
	Sessions->UpdateSession(NAME_GameSession, *Settings, true);
	UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS update: %d player(s), %s"), Players, bInProgress ? TEXT("in progress") : TEXT("lobby"));
}

void FKGSessions::HandlePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& NewPlayer, FString& ErrorMessage)
{
	if (!GameMode || !ErrorMessage.IsEmpty())
	{
		return;
	}
	const TSharedPtr<FContext> Context = FindContext(GameMode);
	if (!Context.IsValid() || Context->PasswordHash.IsEmpty())
	{
		return;
	}
	// The joining connection is not logged in yet: match it by net id, newest first.
	const UNetDriver* Driver = GameMode->GetWorld() ? GameMode->GetWorld()->GetNetDriver() : nullptr;
	const UNetConnection* Joining = nullptr;
	if (Driver)
	{
		for (int32 Index = Driver->ClientConnections.Num() - 1; Index >= 0; --Index)
		{
			const UNetConnection* Connection = Driver->ClientConnections[Index];
			if (Connection && !Connection->PlayerController && Connection->PlayerId == NewPlayer)
			{
				Joining = Connection;
				break;
			}
		}
	}
	const FURL Url(nullptr, Joining ? *Joining->RequestURL : TEXT(""), TRAVEL_Absolute);
	const FString Given = Url.GetOption(*FString::Printf(TEXT("%s="), PasswordOption()), TEXT(""));
	if (Given != Context->PasswordHash)
	{
		ErrorMessage = LOCTEXT("WrongPassword", "Wrong password.").ToString();
		UE_LOG(LogKillGodot, Log, TEXT("KG_SESSIONS rejected a join: wrong or missing password"));
	}
}

#undef LOCTEXT_NAMESPACE
