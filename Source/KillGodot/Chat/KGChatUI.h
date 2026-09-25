#pragma once

#include "CoreMinimal.h"

class APlayerController;
class APlayerState;
class UKGChatComponent;
class UWorld;

/**
 * Local-player chat UI (Slate, code-built, matches the HUD's warm-dark ink panels and Roboto):
 *  - bottom-left chat log above the vitals card; idle lines fade after ~12 s, the last 8 stay while fresh,
 *  - Enter / T opens the input (UI-only input: the character stops, the cursor shows), Esc cancels, Tab cycles the
 *    channels the rules allow right now, Up/Down recall sent lines, PageUp/PageDown scroll,
 *  - emoji picker (button right of the input): click inserts, right-click reacts, Ctrl+1..8 quick-react,
 *  - reaction bubbles over players' heads (projected, occlusion-tested, distance-scaled),
 *  - reaction wheel: hold G, flick the mouse towards an emoji (or press 1-8), release to send; a longer flick reaches
 *    the outer emote ring (FKGEmoteCatalog, living bodies only) which sends the emote through the same relay.
 * One overlay per local player, added to its game viewport at a low Z-order (under the pause menu and modals).
 */
class KILLGODOT_API FKGChatUI
{
public:
	/** Idempotent: creates / re-attaches the overlay for PC's local player and binds it to Chat. */
	static void Ensure(APlayerController* PC, UKGChatComponent* Chat);
	static void Remove(APlayerController* PC);
	static void RemoveAll();
	/** World teardown: drops every overlay of that world's local players (and any stale entry). */
	static void RemoveForWorld(const UWorld* World);

	static bool IsTyping(const APlayerController* PC);
	static void OpenInput(APlayerController* PC, const FString& Prefill = FString());
	static void CloseInput(APlayerController* PC);

	static bool IsWheelOpen(const APlayerController* PC);
	static void OpenWheel(APlayerController* PC);
	/** Mouse delta this frame (look input is ignored while the wheel is open). */
	static void UpdateWheel(APlayerController* PC, const FVector2D& MouseDelta);
	/** Number key 0..7 picks a slot directly. */
	static void SelectWheelSlot(APlayerController* PC, int32 Slot);
	static void CloseWheel(APlayerController* PC, bool bSend);

	/** Local preview (dev): pop a bubble over Sender without the network. */
	static void ShowBubble(APlayerController* Viewer, APlayerState* Sender, int32 EmojiIndex);

	/** While the pre-game lobby screen is up it shows its own panel (MakeLobbyPanel); the HUD overlay hides. */
	static void SetSuppressed(APlayerController* PC, bool bSuppressed);

	/** Chat panel for SKGLobbyRoom::ChatPanelFactory: log + always-visible input + emoji picker, fills its slot. */
	static TSharedRef<class SWidget> MakeLobbyPanel(APlayerController* PC);

	/** Emoji shown on the wheel, clockwise from the top. */
	static const TArray<int32>& GetWheelEmojis();
};
