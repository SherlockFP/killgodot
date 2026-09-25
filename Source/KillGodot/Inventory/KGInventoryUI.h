#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UKGInventoryRPCComponent;

/**
 * Local-player inventory window (Slate, no assets): pockets alone, or pockets <-> an opened container side by side.
 *  - Container open: left click moves the whole stack across, right click moves one.
 *  - Ctrl + click drops the stack on the ground (Ctrl + right click: one).
 *  - E / Esc / Tab / I closes. The window follows the server: it opens when a chest is opened for this player and
 *    closes when the server closes the view (walked away, died, chest locked).
 */
class KILLGODOT_API FKGInventoryUI
{
public:
	static void Open(APlayerController* PC);
	static void Close(APlayerController* PC, bool bTellServer = true);
	static void Toggle(APlayerController* PC);
	static bool IsOpen(const APlayerController* PC);

	/** UKGInventoryRPCComponent -> UI: the viewed container changed for its (possibly local) player. */
	static void HandleViewChanged(UKGInventoryRPCComponent* Relay);
};
