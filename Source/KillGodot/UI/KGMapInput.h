#pragma once

#include "CoreMinimal.h"

/**
 * SPRINT-025: the M key for the village map, as a pure state machine so it can be unit-tested (KGHudTests.cpp).
 *  - tap (press + release within HoldSeconds) toggles the full map, as before;
 *  - hold (press for HoldSeconds or longer) shows the big map while the key is down and hides it on release;
 *  - a press while the map is open (from a tap) closes it; the matching release does nothing.
 * The HUD feeds the raw key state every frame (KGHUDMap.inl); Close() serves Esc and the pause menu.
 */
struct FKGMapInput
{
	bool bOpen = false;
	bool bDown = false;
	/** This press opened the map (so a long press is a hold: release closes it). */
	bool bHoldOpened = false;
	double DownAt = 0.0;
};

namespace KGMapInput
{
	inline constexpr double HoldSeconds = 0.3;

	/** Feed the key state for this frame. Returns true when the open state changed. */
	inline bool Update(FKGMapInput& S, bool bKeyDown, double Now)
	{
		const bool bWasOpen = S.bOpen;
		if (bKeyDown && !S.bDown)
		{
			S.bDown = true;
			S.DownAt = Now;
			if (S.bOpen)
			{
				S.bOpen = false;
				S.bHoldOpened = false;
			}
			else
			{
				S.bOpen = true;
				S.bHoldOpened = true;
			}
		}
		else if (!bKeyDown && S.bDown)
		{
			S.bDown = false;
			if (S.bHoldOpened && Now - S.DownAt >= HoldSeconds)
			{
				S.bOpen = false;
			}
			S.bHoldOpened = false;
		}
		return S.bOpen != bWasOpen;
	}

	/** The map is up because the key is being held (release will close it). */
	inline bool IsHolding(const FKGMapInput& S, double Now)
	{
		return S.bDown && S.bHoldOpened && S.bOpen && Now - S.DownAt >= HoldSeconds;
	}

	inline void Close(FKGMapInput& S)
	{
		S.bOpen = false;
		S.bHoldOpened = false;
	}
}
