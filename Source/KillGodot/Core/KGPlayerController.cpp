#include "Core/KGPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/KGGameUserSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "UI/Menu/SKGLobbyRoom.h"
#include "UI/Menu/SKGPauseMenu.h"

#define LOCTEXT_NAMESPACE "KGPlayerController"
#include "World/KGMapInfo.h"

AKGPlayerController::AKGPlayerController()
{
}

void AKGPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (!IsLocalController())
	{
		return;
	}
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get(); Settings && !SettingsAppliedHandle.IsValid())
	{
		SettingsAppliedHandle = Settings->OnSettingsApplied.AddUObject(this, &AKGPlayerController::HandleSettingsApplied);
		// Volumes are per audio device; make sure they are live in this world.
		Settings->ApplyAudioSettings();
	}
	ApplyUserSettings();
	AddMenuMappingContext();
	KGMenu::RegisterNetworkErrorHooks();
	// Coming from the front end the viewport may still be in UI mode.
	if (!bPauseMenuOpen)
	{
		KGMenu::SetGameInput(this);
	}
}

void AKGPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		ApplyUserSettings();
	}
}

void AKGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get(); Settings && SettingsAppliedHandle.IsValid())
	{
		Settings->OnSettingsApplied.Remove(SettingsAppliedHandle);
		SettingsAppliedHandle.Reset();
	}
	KGMenu::RemoveFromViewport(PauseMenuEntry);
	KGMenu::RemoveFromViewport(LobbyScreenEntry);
	PauseMenu.Reset();
	LobbyScreen.Reset();
	bPauseMenuOpen = false;
	Super::EndPlay(EndPlayReason);
}

void AKGPlayerController::CreateMenuInput()
{
	if (MenuAction && MenuMappingContext)
	{
		return;
	}
	MenuAction = NewObject<UInputAction>(this, TEXT("IA_KG_Menu"));
	MenuAction->ValueType = EInputActionValueType::Boolean;
	MenuMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KG_Menu"));
	MenuMappingContext->MapKey(MenuAction, EKeys::Escape);
	MenuMappingContext->MapKey(MenuAction, EKeys::Gamepad_Special_Right);
}

void AKGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateMenuInput();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(MenuAction, ETriggerEvent::Started, this, &AKGPlayerController::HandleMenuAction);
	}
	AddMenuMappingContext();
}

void AKGPlayerController::AddMenuMappingContext()
{
	CreateMenuInput();
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (!Subsystem->HasMappingContext(MenuMappingContext))
			{
				// High priority so gameplay contexts never shadow Esc.
				Subsystem->AddMappingContext(MenuMappingContext, 100);
			}
		}
	}
}

void AKGPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	if (InPawn && IsLocalController())
	{
		ApplyUserSettings();
		AddMenuMappingContext();
	}
}

void AKGPlayerController::HandleMenuAction()
{
	if (KGMinimap::CloseFullMap(this))
	{
		return;   // Esc closes the full-screen map first
	}
	if (!LobbyScreenEntry.IsShown())
	{
		OpenPauseMenu();
	}
}

void AKGPlayerController::OpenPauseMenu()
{
	if (bPauseMenuOpen || !IsLocalController() || LobbyScreenEntry.IsShown())
	{
		return;
	}
	if (!PauseMenu.IsValid())
	{
		PauseMenu = SNew(SKGPauseMenu)
			.OwningPlayer(this)
			.OnResume_UObject(this, &AKGPlayerController::ClosePauseMenu);
	}
	else
	{
		PauseMenu->PlayOpen();
	}
	PauseMenuEntry = KGMenu::AddToViewport(this, PauseMenu.ToSharedRef(), 40);
	if (!PauseMenuEntry.IsShown())
	{
		return;
	}
	bPauseMenuOpen = true;
	bFlushKeysPending = true;
	KGMenu::SetMenuInput(this, PauseMenu->GetInitialFocus());
}

void AKGPlayerController::PlayerTick(float DeltaTime)
{
	// Deferred: flushing inside the Esc input callback would re-enter the input stack.
	if (bFlushKeysPending)
	{
		bFlushKeysPending = false;
		FlushPressedKeys();
	}
	UpdateLobbyScreen();
	Super::PlayerTick(DeltaTime);
}

void AKGPlayerController::UpdateLobbyScreen()
{
	if (!IsLocalController())
	{
		return;
	}
	const AKGLobbyState* Lobby = AKGLobbyState::Get(this);
	const bool bWant = Lobby && !Lobby->HasStarted();
	if (bWant && !LobbyScreenEntry.IsShown())
	{
		if (bPauseMenuOpen)
		{
			ClosePauseMenu();
		}
		LobbyScreen = SNew(SKGLobbyRoom).OwningPlayer(this);
		LobbyScreenEntry = KGMenu::AddToViewport(this, LobbyScreen.ToSharedRef(), 30);
		if (LobbyScreenEntry.IsShown())
		{
			bFlushKeysPending = true;
			KGMenu::SetMenuInput(this, LobbyScreen->GetInitialFocus());
			UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY screen shown (%s)"), IsLobbyHost() ? TEXT("host") : TEXT("guest"));
		}
	}
	else if (!bWant && LobbyScreenEntry.IsShown())
	{
		KGMenu::RemoveFromViewport(LobbyScreenEntry);
		LobbyScreen.Reset();
		if (!bPauseMenuOpen)
		{
			KGMenu::SetGameInput(this);
		}
		UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY screen closed (match starting)"));
	}
}

AKGLobbyState* AKGPlayerController::AuthLobby(bool bHostOnly) const
{
	AKGLobbyState* Lobby = AKGLobbyState::Get(this);
	if (!Lobby || Lobby->HasStarted() || (bHostOnly && !IsLobbyHost()))
	{
		return nullptr;
	}
	return Lobby;
}

void AKGPlayerController::ServerLobbySetReady_Implementation(bool bReady)
{
	if (AKGLobbyState* Lobby = AuthLobby(false))
	{
		Lobby->AuthSetReady(PlayerState, bReady);
	}
}

void AKGPlayerController::ServerLobbyApplySettings_Implementation(const FKGLobbySettings& NewSettings)
{
	if (AKGLobbyState* Lobby = AuthLobby(true))
	{
		Lobby->AuthApplySettings(NewSettings);
	}
}

void AKGPlayerController::ServerLobbyStart_Implementation(bool bStart)
{
	if (AKGLobbyState* Lobby = AuthLobby(true))
	{
		Lobby->AuthSetCountdown(bStart);
	}
}

void AKGPlayerController::ServerLobbyKick_Implementation(APlayerState* Target)
{
	if (AKGLobbyState* Lobby = AuthLobby(true))
	{
		Lobby->AuthKick(Target);
	}
}

void AKGPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	Super::ClientWasKicked_Implementation(KickReason);
	UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY kicked by the host: %s"), *KickReason.ToString());
	// Shown by the title screen after the connection closes.
	KGMenu::SetPendingMenuMessage(KickReason.IsEmpty() ? LOCTEXT("Kicked", "You were removed from the lobby.") : KickReason);
}

void AKGPlayerController::ClosePauseMenu()
{
	if (!bPauseMenuOpen)
	{
		return;
	}
	bPauseMenuOpen = false;
	KGMenu::RemoveFromViewport(PauseMenuEntry);
	KGMenu::SetGameInput(this);
}

void AKGPlayerController::TogglePauseMenu()
{
	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void AKGPlayerController::HandleSettingsApplied(const UKGGameUserSettings& Settings)
{
	ApplyUserSettings();
}

void AKGPlayerController::ApplyUserSettings()
{
	const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	LookSensitivity = Settings ? Settings->GetMouseSensitivity() : 1.0f;
	bInvertLook = Settings && Settings->GetInvertY();
	const float FieldOfView = Settings ? Settings->GetFieldOfView() : UKGGameUserSettings::DefaultFieldOfView;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->DefaultFOV = FieldOfView;
	}
	if (const APawn* ControlledPawn = GetPawn())
	{
		if (UCameraComponent* Camera = ControlledPawn->FindComponentByClass<UCameraComponent>())
		{
			Camera->SetFieldOfView(FieldOfView);
		}
	}
}

void AKGPlayerController::AddYawInput(float Val)
{
	Super::AddYawInput(Val * LookSensitivity);
}

void AKGPlayerController::AddPitchInput(float Val)
{
	Super::AddPitchInput(Val * LookSensitivity * (bInvertLook ? -1.0f : 1.0f));
}

// --- Console ---------------------------------------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace
{
	AKGPlayerController* LobbyConsolePlayer(UWorld* World)
	{
		return Cast<AKGPlayerController>(KGMenu::GetLocalPlayerController(World));
	}

	/** kg.Lobby ready|unready|start|cancel|bots on/off|max N|preset N|kick <name>|status (dev / automation). */
	FAutoConsoleCommandWithWorldAndArgs GKGLobbyCommand(
		TEXT("kg.Lobby"),
		TEXT("Pre-game lobby: kg.Lobby ready|unready|start|cancel|bots <0/1>|max <N>|preset <N>|kick <name>|status"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AKGPlayerController* PC = LobbyConsolePlayer(World);
			const AKGLobbyState* Lobby = AKGLobbyState::Get(World);
			if (!PC || !Lobby)
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_LOBBY no lobby here"));
				return;
			}
			const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("status"));
			const FString Value = Args.Num() > 1 ? Args[1] : FString();
			FKGLobbySettings Settings = Lobby->GetSettings();
			if (Verb == TEXT("ready") || Verb == TEXT("unready"))
			{
				PC->ServerLobbySetReady(Verb == TEXT("ready"));
			}
			else if (Verb == TEXT("start") || Verb == TEXT("cancel"))
			{
				PC->ServerLobbyStart(Verb == TEXT("start"));
			}
			else if (Verb == TEXT("bots"))
			{
				Settings.bFillWithBots = Value != TEXT("0") && Value != TEXT("off");
				PC->ServerLobbyApplySettings(Settings);
			}
			else if (Verb == TEXT("max"))
			{
				Settings.MaxPlayers = FCString::Atoi(*Value);
				PC->ServerLobbyApplySettings(Settings);
			}
			else if (Verb == TEXT("preset"))
			{
				Settings.RolePreset = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Value), 0, 255));
				PC->ServerLobbyApplySettings(Settings);
			}
			else if (Verb == TEXT("kick"))
			{
				// "kick 2" = seat index, anything else = name substring.
				const TArray<FKGLobbyEntry>& Entries = Lobby->GetEntries();
				const int32 Seat = Value.IsNumeric() ? FCString::Atoi(*Value) : INDEX_NONE;
				for (int32 Index = 0; Index < Entries.Num(); ++Index)
				{
					const FKGLobbyEntry& Entry = Entries[Index];
					if (Entry.Player && (Seat != INDEX_NONE ? Index == Seat : Entry.Player->GetPlayerName().Contains(Value)))
					{
						PC->ServerLobbyKick(Entry.Player);
						break;
					}
				}
			}
			FString Seats;
			for (const FKGLobbyEntry& Entry : Lobby->GetEntries())
			{
				Seats += FString::Printf(TEXT(" [%s%s%s]"), Entry.Player ? *Entry.Player->GetPlayerName() : TEXT("?"),
				                         Entry.bHost ? TEXT(" host") : TEXT(""), Entry.bReady ? TEXT(" ready") : TEXT(""));
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_LOBBY status (%s): '%s' code %s max %d bots %s preset %d countdown %s %.1f, screen %s, seats:%s"),
			       PC->IsLobbyHost() ? TEXT("host") : TEXT("guest"), *Settings.LobbyName, *Settings.Code, Settings.MaxPlayers,
			       Settings.bFillWithBots ? TEXT("on") : TEXT("off"), Settings.RolePreset,
			       Lobby->IsCountingDown() ? TEXT("on") : TEXT("off"), Lobby->GetCountdownRemaining(),
			       PC->IsLobbyScreenShown() ? TEXT("shown") : TEXT("hidden"), *Seats);
		}));
}
#endif

#undef LOCTEXT_NAMESPACE
