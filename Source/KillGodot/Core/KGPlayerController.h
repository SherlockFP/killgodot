#pragma once

#include "CoreMinimal.h"
#include "Core/KGLobbyState.h"
#include "GameFramework/PlayerController.h"
#include "UI/Menu/KGMenuActions.h"
#include "KGPlayerController.generated.h"

class SKGLobbyRoom;
class SKGPauseMenu;
class UInputAction;
class UInputMappingContext;
class UKGGameUserSettings;

/**
 * Gameplay player controller: owns the Esc pause menu (Resume / Settings / Leave / Quit), the pre-game lobby room
 * (shown while an AKGLobbyState is open, with its server RPCs) and applies the local player's settings (mouse
 * sensitivity, invert Y, field of view). Menus show the cursor and switch to UI-only input; the match keeps running.
 */
UCLASS()
class KILLGODOT_API AKGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKGPlayerController();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Menu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category = "KillGodot|Menu")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	/** Re-reads UKGGameUserSettings and applies look sensitivity / invert / FOV to this controller's view. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void ApplyUserSettings();

	virtual void ReceivedPlayer() override;
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

	// --- Pre-game lobby (AKGLobbyState). Clients ask, the server checks and applies. ----------------------------
	/** The listen-server host's own controller (the only one allowed to change lobby settings / kick / start). */
	bool IsLobbyHost() const { return IsLocalController() && GetNetMode() != NM_Client; }

	UFUNCTION(Server, Reliable)
	void ServerLobbySetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerLobbyApplySettings(const FKGLobbySettings& NewSettings);

	/** Host: start (true) or cancel (false) the start countdown. */
	UFUNCTION(Server, Reliable)
	void ServerLobbyStart(bool bStart);

	UFUNCTION(Server, Reliable)
	void ServerLobbyKick(APlayerState* Target);

	bool IsLobbyScreenShown() const { return LobbyScreenEntry.IsShown(); }

	// Look input from the pawn goes through these: sensitivity and invert Y are applied here.
	virtual void AddYawInput(float Val) override;
	virtual void AddPitchInput(float Val) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void CreateMenuInput();
	void AddMenuMappingContext();
	void HandleMenuAction();
	void HandleSettingsApplied(const UKGGameUserSettings& Settings);
	void UpdateLobbyScreen();
	/** Server side: the lobby, if this RPC may act on it. */
	AKGLobbyState* AuthLobby(bool bHostOnly) const;

	/** Esc (and gamepad Start): opens the pause menu. Created in code, no assets. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuMappingContext;

	TSharedPtr<SKGPauseMenu> PauseMenu;
	TSharedPtr<SKGLobbyRoom> LobbyScreen;
	FKGViewportWidget LobbyScreenEntry;
	FKGViewportWidget PauseMenuEntry;
	FDelegateHandle SettingsAppliedHandle;
	float LookSensitivity = 1.0f;
	bool bInvertLook = false;
	bool bPauseMenuOpen = false;
	/** Keys held when the menu opened (Esc itself, W...) must be released for the player input next tick. */
	bool bFlushKeysPending = false;
};
