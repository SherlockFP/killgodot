#include "Core/KGLobbyState.h"

#include "Core/KGGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSessions.h"
#include "UI/Menu/KGMenuActions.h"

#define LOCTEXT_NAMESPACE "KGLobby"

namespace
{
	/** Wall clock in ms since 1970 (logs only: lets a local multi-process smoke line up events across processes). */
	long long LobbyUnixMs()
	{
		return static_cast<long long>((FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTotalMilliseconds());
	}
}

AKGLobbyState::AKGLobbyState()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
}

AKGLobbyState* AKGLobbyState::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKGLobbyState> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FLinearColor AKGLobbyState::GetPlayerColor(int32 ColorIndex)
{
	// 20 readable, distinct villager colours (one per seat of a full lobby).
	static const uint32 Palette[] = {0xE0413A, 0xF28C28, 0xF2C230, 0x8BD160, 0x2BB3A3, 0x4FA3E0, 0x6C6FE0, 0xB072E0,
	                                 0xE06CB4, 0xC8102E, 0x7A4A2A, 0xF6E7C8, 0x3F8F4A, 0x1E5A64, 0x9A9A9A, 0xE8A0A0,
	                                 0xA0D8E8, 0xD8C080, 0x505080, 0x303030};
	constexpr int32 Count = UE_ARRAY_COUNT(Palette);
	const uint32 RGB = Palette[((ColorIndex % Count) + Count) % Count];
	return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
}

void AKGLobbyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGLobbyState, Entries);
	DOREPLIFETIME(AKGLobbyState, Settings);
	DOREPLIFETIME(AKGLobbyState, Countdown);
	DOREPLIFETIME(AKGLobbyState, bCountingDown);
	DOREPLIFETIME(AKGLobbyState, bStarted);
}

void AKGLobbyState::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	// Seed the settings from what the host chose on the Host screen (name, code, privacy); fall back to defaults.
	if (const FKGHostOptions* Hosted = FKGSessions::Get().GetHostOptions(this))
	{
		Settings.LobbyName = Hosted->Name;
		Settings.Code = Hosted->Code;
		Settings.MapPath = Hosted->MapPath;
		Settings.MaxPlayers = Hosted->MaxPlayers;
		Settings.bPrivate = Hosted->bPrivate;
		Settings.bPassword = !Hosted->Password.IsEmpty();
	}
	if (Settings.MapPath.IsEmpty())
	{
		Settings.MapPath = GetWorld()->GetOutermost()->GetName();
	}
	if (const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode(); GameMode && GameMode->GameSession)
	{
		Settings.MaxPlayers = GameMode->GameSession->MaxPlayers > 0 ? GameMode->GameSession->MaxPlayers : Settings.MaxPlayers;
	}
	if (Settings.LobbyName.IsEmpty())
	{
		Settings.LobbyName = LOCTEXT("DefaultName", "Morrowmere lobby").ToString();
	}
	AuthSyncSeats();
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &AKGLobbyState::HandlePostLogin);
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY open '%s' code %s, max %d"), *Settings.LobbyName, *Settings.Code, Settings.MaxPlayers);
}

void AKGLobbyState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	Super::EndPlay(EndPlayReason);
}

void AKGLobbyState::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld() || !NewPlayer)
	{
		return;
	}
	// Wall clock (ms since 1970, same machine clock for every process of a local smoke) for the "names within 1 s"
	// acceptance check (Tools/Unreal/kg_lobby_smoke.ps1).
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY join t=%lld"), LobbyUnixMs());
	bSyncNextTick = true;   // AKGGameMode::PostLogin names the player right after this event
}

FString AKGLobbyState::GetSeatName(const FKGLobbyEntry& Entry)
{
	return !Entry.Name.IsEmpty() ? Entry.Name : (Entry.Player ? Entry.Player->GetPlayerName() : FString());
}

void AKGLobbyState::LogVisibleNames()
{
	TArray<FString> Names;
	for (const FKGLobbyEntry& Entry : Entries.Items)
	{
		const FString Name = GetSeatName(Entry);
		if (!Name.IsEmpty())
		{
			Names.Add(Name);
		}
	}
	const FString Joined = FString::Join(Names, TEXT(", "));
	if (Joined == LastLoggedNames)
	{
		return;
	}
	LastLoggedNames = Joined;
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY_NAMES t=%lld %s n=%d [%s]"), LobbyUnixMs(), HasAuthority() ? TEXT("host") : TEXT("client"),
	       Names.Num(), *Joined);
}

const FKGLobbyEntry* AKGLobbyState::FindEntry(const APlayerState* Player) const
{
	return Entries.Items.FindByPredicate([Player](const FKGLobbyEntry& Entry) { return Entry.Player == Player; });
}

int32 AKGLobbyState::CountHumans() const
{
	int32 Count = 0;
	for (const FKGLobbyEntry& Entry : Entries.Items)
	{
		Count += Entry.Player && !Entry.Player->IsABot() ? 1 : 0;
	}
	return Count;
}

int32 AKGLobbyState::CountReady() const
{
	int32 Count = 0;
	for (const FKGLobbyEntry& Entry : Entries.Items)
	{
		Count += Entry.Player && Entry.bReady ? 1 : 0;
	}
	return Count;
}

bool AKGLobbyState::CanStart(FText& OutReason) const
{
	if (bStarted)
	{
		OutReason = LOCTEXT("AlreadyStarted", "The match has started.");
		return false;
	}
	const int32 Humans = CountHumans();
	if (!Settings.bFillWithBots && Humans < MinPlayersToStart)
	{
		OutReason = FText::Format(LOCTEXT("NeedPlayers", "Need {0} players (or turn on bots)."), FText::AsNumber(MinPlayersToStart));
		return false;
	}
	OutReason = FText::GetEmpty();
	return true;
}

void AKGLobbyState::Touch()
{
	++Revision;
	Entries.MarkArrayDirty();
	ForceNetUpdate();
}

void AKGLobbyState::OnRep_Lobby()
{
	++Revision;
}

void AKGLobbyState::AuthSyncSeats()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState || bStarted)
	{
		return;
	}
	bool bChanged = false;
	// Drop seats of players who left.
	for (int32 Index = Entries.Items.Num() - 1; Index >= 0; --Index)
	{
		if (!Entries.Items[Index].Player || !GameState->PlayerArray.Contains(Entries.Items[Index].Player))
		{
			Entries.Items.RemoveAt(Index);
			bChanged = true;
		}
	}
	// Seat newcomers (humans only: bots are added when the match starts).
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState || PlayerState->IsABot() || FindEntry(PlayerState))
		{
			continue;
		}
		FKGLobbyEntry& Entry = Entries.Items.AddDefaulted_GetRef();
		Entry.Player = PlayerState;
		Entry.Name = PlayerState->GetPlayerName();
		Entry.ColorIndex = NextColor++;
		const APlayerController* OwnerPC = Cast<APlayerController>(PlayerState->GetOwner());
		Entry.bHost = OwnerPC && OwnerPC->IsLocalController();
		Entries.MarkItemDirty(Entry);
		bChanged = true;
		UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY seat %s%s"), *PlayerState->GetPlayerName(), Entry.bHost ? TEXT(" (host)") : TEXT(""));
	}
	// Renames (the game mode names humans right after login) reach every seat list with the lobby, not later with the
	// player state (which replicates at a low rate).
	for (FKGLobbyEntry& Entry : Entries.Items)
	{
		if (Entry.Player && Entry.Name != Entry.Player->GetPlayerName())
		{
			Entry.Name = Entry.Player->GetPlayerName();
			Entries.MarkItemDirty(Entry);
			++Revision;
			ForceNetUpdate();
		}
	}
	if (bChanged)
	{
		if (bCountingDown)
		{
			// Someone joined or left mid-countdown: let the host decide again.
			bCountingDown = false;
			Countdown.RemainingSeconds = 0.0f;
		}
		Touch();
	}
}

void AKGLobbyState::AuthSetReady(APlayerState* Player, bool bReady)
{
	for (FKGLobbyEntry& Entry : Entries.Items)
	{
		if (Entry.Player == Player && Entry.bReady != bReady)
		{
			Entry.bReady = bReady;
			Entries.MarkItemDirty(Entry);
			Touch();
			UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY %s is %s (%d/%d ready)"), *Player->GetPlayerName(),
			       bReady ? TEXT("ready") : TEXT("not ready"), CountReady(), CountHumans());
		}
	}
}

void AKGLobbyState::AuthApplySettings(const FKGLobbySettings& NewSettings)
{
	if (bStarted)
	{
		return;
	}
	FKGLobbySettings Clean = Settings;
	Clean.MaxPlayers = FMath::Clamp(NewSettings.MaxPlayers, MinPlayersToStart, 20);
	Clean.RolePreset = NewSettings.RolePreset;
	Clean.bFillWithBots = NewSettings.bFillWithBots;
	Clean.MapPath = NewSettings.MapPath.IsEmpty() ? Settings.MapPath : NewSettings.MapPath;
	const bool bMaxChanged = Clean.MaxPlayers != Settings.MaxPlayers;
	const bool bMapChanged = Clean.MapPath != Settings.MapPath;
	Settings = Clean;
	Touch();
	if (bMaxChanged)
	{
		if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode(); GameMode && GameMode->GameSession)
		{
			GameMode->GameSession->MaxPlayers = Settings.MaxPlayers;
		}
		FKGSessions::Get().UpdateHostedMaxPlayers(this, Settings.MaxPlayers);
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY settings: max %d, preset %d, bots %s, map %s"), Settings.MaxPlayers,
	       Settings.RolePreset, Settings.bFillWithBots ? TEXT("on") : TEXT("off"), *Settings.MapPath);
	if (bMapChanged && !GetWorld()->GetOutermost()->GetName().EndsWith(FPackageName::GetShortName(Settings.MapPath)))
	{
		// Everyone follows the host to the new map; the lobby opens again there.
		FString MapTitle = FPackageName::GetShortName(Settings.MapPath);
		for (const FKGMapEntry& Map : KGMenu::GetMaps())
		{
			MapTitle = Map.MapPath == Settings.MapPath ? Map.DisplayName.ToString() : MapTitle;
		}
		FKGSessions::Get().UpdateHostedMap(this, Settings.MapPath, MapTitle);
		GetWorld()->ServerTravel(FString::Printf(TEXT("%s?listen?KGLobby?MaxPlayers=%d"), *Settings.MapPath, Settings.MaxPlayers));
	}
}

void AKGLobbyState::AuthSetCountdown(bool bStart)
{
	FText Reason;
	if (bStart && !CanStart(Reason))
	{
		return;
	}
	bCountingDown = bStart;
	Countdown.Start(bStart ? CountdownSeconds : 0.0f);
	LastCountdownSecond = -1;
	Touch();
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY countdown %s"), bStart ? TEXT("started") : TEXT("cancelled"));
}

void AKGLobbyState::AuthKick(APlayerState* Target)
{
	APlayerController* Victim = Target ? Cast<APlayerController>(Target->GetOwner()) : nullptr;
	AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (!Victim || Victim->IsLocalController() || !GameMode || !GameMode->GameSession)
	{
		return; // never kick the host
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY kick %s"), *Target->GetPlayerName());
	GameMode->GameSession->KickPlayer(Victim, LOCTEXT("Kicked", "The host removed you from the lobby."));
}

void AKGLobbyState::AuthBeginMatch()
{
	bStarted = true;
	bCountingDown = false;
	Touch();
	const int32 BotTarget = Settings.bFillWithBots ? MinPlayersToStart : 0;
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY start: %d human(s), bots up to %d"), CountHumans(), BotTarget);
	if (AKGGameMode* GameMode = GetWorld()->GetAuthGameMode<AKGGameMode>())
	{
		GameMode->StartFromLobby(BotTarget);
	}
}

void AKGLobbyState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted)
	{
		LogVisibleNames();
	}
	if (!HasAuthority() || bStarted)
	{
		return;
	}
	SyncAccumulator += DeltaSeconds;
	if (SyncAccumulator >= 0.25f || bSyncNextTick)
	{
		SyncAccumulator = 0.0f;
		bSyncNextTick = false;
		AuthSyncSeats();
		LogVisibleNames();
	}
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("KGLobbySmoke"));
	if (bSmoke && !bCountingDown)
	{
		// Headless two-process smoke: once two named humans sit here for a few seconds, the host starts the match.
		SmokeSeatedSeconds = CountHumans() >= 2 ? SmokeSeatedSeconds + DeltaSeconds : 0.0f;
		if (SmokeSeatedSeconds > 4.0f)
		{
			SmokeSeatedSeconds = -1000.0f;
			AuthSetCountdown(true);
		}
	}
	if (bCountingDown)
	{
		const bool bDone = Countdown.Advance(DeltaSeconds);
		const int32 Second = FMath::CeilToInt(Countdown.RemainingSeconds);
		if (Second != LastCountdownSecond && Second > 0)
		{
			LastCountdownSecond = Second;
			Touch();
			UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY starting in %d"), Second);
		}
		if (bDone)
		{
			AuthBeginMatch();
		}
	}
}

#undef LOCTEXT_NAMESPACE
