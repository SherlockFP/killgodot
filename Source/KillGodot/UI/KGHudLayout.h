#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "UI/KGUITokens.h"

/**
 * SPRINT-025: where the fixed HUD cards sit, computed from the screen size alone (no world, no canvas) so a unit test
 * can prove nothing overlaps or leaves the screen from 720p to 1440p (KGHudTests.cpp). UI/KGHUD.cpp draws through
 * these rectangles; anything drawn elsewhere is either centred and transient (announcements, toasts) or anchored to
 * the crosshair.
 *
 * All values in screen pixels for the given size; S = H / 1080 (the HUD is designed at 1080p).
 */
struct FKGHudLayout
{
	float W = 0.0f;
	float H = 0.0f;
	float S = 1.0f;

	FBox2D Tracker;      // top left: chore tracker (Rows rows)
	FBox2D Phase;        // top centre: phase card (+ preparation strip)
	FBox2D Compass;      // under the phase card: world chore compass strip + step line
	FBox2D Minimap;      // top right: disc + rim + location pill
	FBox2D Vitals;       // bottom left: health + stamina
	FBox2D Purse;        // above the vitals: gold + [I]
	FBox2D RoleChip;     // bottom right: own role (+ blade hint above it for the Impatient)
	FBox2D Prompt;       // under the crosshair: E prompt / work ring label

	static FKGHudLayout Compute(float InW, float InH, int32 TrackerRows, bool bPrepStrip)
	{
		FKGHudLayout L;
		L.W = InW;
		L.H = InH;
		const float S = InH / 1080.0f;
		L.S = S;
		const float M = KGUI::Margin * S;

		// Chore tracker (top left).
		{
			const float W = 372.0f * S;
			const float HeadH = 58.0f * S;
			const float H = TrackerRows > 0 ? HeadH + KGUI::RowH * S * TrackerRows + 10.0f * S : 0.0f;
			L.Tracker = FBox2D(FVector2D(M, M), FVector2D(M + W, M + H));
		}
		// Phase card (top centre).
		{
			const float W = 440.0f * S;
			const float H = (72.0f + (bPrepStrip ? 40.0f : 0.0f)) * S;
			L.Phase = FBox2D(FVector2D(InW * 0.5f - W * 0.5f, 18.0f * S), FVector2D(InW * 0.5f + W * 0.5f, 18.0f * S + H));
		}
		// Compass strip + step line under the phase card.
		{
			const float W = 560.0f * S;
			const float Top = L.Phase.Max.Y + 12.0f * S;
			L.Compass = FBox2D(FVector2D(InW * 0.5f - W * 0.5f, Top), FVector2D(InW * 0.5f + W * 0.5f, Top + 84.0f * S));
		}
		// Minimap (top right): disc radius 124 + rim 7, the location pill under it (32 high, 12 gap).
		{
			const float R = 124.0f * S;
			const float Rim = 7.0f * S;
			const float Right = InW - M;
			const float Top = M;
			L.Minimap = FBox2D(FVector2D(Right - 2.0f * (R + Rim), Top), FVector2D(Right, Top + 2.0f * (R + Rim) + 44.0f * S));
		}
		// Vitals (bottom left).
		{
			const float W = 440.0f * S;
			const float H = 100.0f * S;
			L.Vitals = FBox2D(FVector2D(M, InH - M - H), FVector2D(M + W, InH - M));
		}
		// Purse pill above the vitals.
		{
			const float H = 38.0f * S;
			const float Bottom = L.Vitals.Min.Y - 12.0f * S;
			L.Purse = FBox2D(FVector2D(M, Bottom - H), FVector2D(M + 300.0f * S, Bottom));
		}
		// Role chip (bottom right), widest role name budgeted, plus the blade hint above it.
		{
			const float W = 330.0f * S;
			const float H = 64.0f * S;
			L.RoleChip = FBox2D(FVector2D(InW - M - W, InH - M - H - 48.0f * S), FVector2D(InW - M, InH - M));
		}
		// Prompt under the crosshair.
		{
			const float W = 420.0f * S;
			L.Prompt = FBox2D(FVector2D(InW * 0.5f - W * 0.5f, InH * 0.5f + 40.0f * S), FVector2D(InW * 0.5f + W * 0.5f, InH * 0.5f + 96.0f * S));
		}
		return L;
	}

	/** Every card, for the overlap / overflow test. */
	TArray<TPair<FString, FBox2D>> All() const
	{
		return {{TEXT("Tracker"), Tracker}, {TEXT("Phase"), Phase},   {TEXT("Compass"), Compass},   {TEXT("Minimap"), Minimap},
		        {TEXT("Vitals"), Vitals},   {TEXT("Purse"), Purse},   {TEXT("RoleChip"), RoleChip}, {TEXT("Prompt"), Prompt}};
	}
};
