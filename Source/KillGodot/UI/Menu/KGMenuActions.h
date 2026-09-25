#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class APlayerController;
class SWidget;
class UGameViewportClient;
class UWorld;

/** One entry of the Host Game map list. */
struct FKGMapEntry
{
	FText DisplayName;
	FText Mode;
	FText Description;
	/** Long package name, e.g. /Game/KillGodot/Maps/L_Morrowmere. Empty = not available yet. */
	FString MapPath;
	bool bAvailable = false;
};

/** A widget added to a game viewport; remembers the viewport so it can be removed during world teardown. */
struct FKGViewportWidget
{
	TWeakObjectPtr<UGameViewportClient> Viewport;
	TSharedPtr<SWidget> Widget;

	bool IsShown() const { return Widget.IsValid() && Viewport.IsValid(); }
};

/**
 * Front-end actions shared by the main menu, the pause menu and the console commands:
 * host (listen server travel), join by IP (ClientTravel), leave to the main menu, quit, plus viewport/input helpers.
 *
 * Console: kg.MainMenu [travel]  - show the main menu over the current map (or travel to the front-end map)
 *          kg.PauseMenu          - toggle the in-game pause menu (Esc stops PIE by default in the editor)
 */
namespace KGMenu
{
	/** Front-end map, created by Tools/Unreal/kg_make_main_menu_map.py. */
	inline constexpr const TCHAR* MainMenuMap = TEXT("/Game/KillGodot/Maps/L_MainMenu");
	/** Game mode forced through the URL when travelling to the front end (works even without the map override). */
	inline constexpr const TCHAR* MenuGameModeClass = TEXT("/Script/KillGodot.KGMenuGameMode");

	inline constexpr int32 MinPlayers = 6;
	inline constexpr int32 MaxPlayers = 20;
	inline constexpr int32 DefaultPort = 7777;

	KILLGODOT_API const TArray<FKGMapEntry>& GetMaps();

	/** Opens MapPath as a listen server for up to MaxPlayers (clamped 6-20), starting in the pre-game lobby. */
	KILLGODOT_API void HostGame(APlayerController* PC, const FString& MapPath, int32 InMaxPlayers);

	/** Connects to "ip" or "ip:port". Returns false (with a reason) when the address is not usable. */
	KILLGODOT_API bool JoinGame(APlayerController* PC, const FString& Address, FText& OutError);

	/** Aborts a pending connection started by JoinGame. */
	KILLGODOT_API void CancelJoin(APlayerController* PC);

	/** Leaves the current match (host: ends it) and loads the front end. */
	KILLGODOT_API void LeaveToMainMenu(APlayerController* PC);

	KILLGODOT_API void QuitGame(APlayerController* PC);

	/** Trims and validates "host" / "host:port". Returns an empty string when invalid. */
	KILLGODOT_API FString NormalizeAddress(const FString& Input);

	/** This machine's LAN address (for the Host screen), empty if unknown. */
	KILLGODOT_API FString GetLocalAddress();

	/** Hooks engine network/travel failures so the next main menu can explain what happened. */
	KILLGODOT_API void RegisterNetworkErrorHooks();

	/** Message for the next title screen that network failures do not overwrite (e.g. "You were kicked"). */
	KILLGODOT_API void SetPendingMenuMessage(const FText& Message);

	/** Returns the last network/travel failure message once (then clears it). */
	KILLGODOT_API FText ConsumeLastNetworkError();

	/** Adds Widget over the whole game viewport of PC's world. */
	KILLGODOT_API FKGViewportWidget AddToViewport(APlayerController* PC, const TSharedRef<SWidget>& Widget, int32 ZOrder);
	KILLGODOT_API void RemoveFromViewport(FKGViewportWidget& Entry);

	/**
	 * Cursor on, UI-only input, focus Focus (menus). Keys held at that moment stay "down" for the player input:
	 * gameplay controllers should FlushPressedKeys() on their next tick (AKGPlayerController does).
	 */
	KILLGODOT_API void SetMenuInput(APlayerController* PC, const TSharedPtr<SWidget>& Focus);
	/** Cursor off, game-only input (gameplay). */
	KILLGODOT_API void SetGameInput(APlayerController* PC);

	/**
	 * Undoes SetMenuInput on the viewport itself (input no longer ignored, default mouse capture, focus to the game
	 * view). Called when a front-end world ends: the viewport outlives the map and the next map's controller may not
	 * set an input mode at all.
	 */
	KILLGODOT_API void ResetViewportInput(UGameViewportClient* Viewport);

	/** Gives keyboard focus to Widget for every Slate user (used after page switches). */
	KILLGODOT_API void FocusWidget(const TSharedPtr<SWidget>& Widget);

	KILLGODOT_API APlayerController* GetLocalPlayerController(UWorld* World);
}
