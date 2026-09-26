#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/**
 * SPRINT-023 voice overlay for a local player (Slate, code-built, one leaf widget per local player at a low Z-order):
 *  - speaking markers over the heads of bodies you can hear (packets in the last 0.35 s or a bark), pulsing with
 *    the mouth, plus the "E  High five?" prompt over a body offering a partner emote near you;
 *  - your own mic pill at the bottom (push-to-talk held / open mic / voice closed) with a level bar, and your own
 *    partner-emote status (waiting, result);
 *  - the three TF2-style voice-command radials: hold Z / X / C, flick the mouse towards a line (or press 1-8),
 *    release to say it (UKGVoiceSubsystem drives the input, this only draws and remembers the selection).
 */
class KILLGODOT_API FKGVoiceUI
{
public:
	/** Idempotent: creates / re-attaches the overlay for PC's local player. */
	static void Ensure(APlayerController* PC);
	static void Remove(APlayerController* PC);
	static void RemoveForWorld(const UWorld* World);

	static bool IsWheelOpen(const APlayerController* PC);
	/** Menu 0..2 (Z, X, C). */
	static void OpenWheel(APlayerController* PC, int32 Menu);
	static int32 GetOpenMenu(const APlayerController* PC);
	static void UpdateWheel(APlayerController* PC, const FVector2D& MouseDelta);
	static void SelectWheelSlot(APlayerController* PC, int32 Slot);
	/** Closes; returns the catalog index of the chosen command when bSend and something was picked, else INDEX_NONE. */
	static int32 CloseWheel(APlayerController* PC, bool bSend);
};
