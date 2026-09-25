#pragma once

#include "CoreMinimal.h"

/**
 * SPRINT-025: one visual language for every screen (Docs/08_UI_UX.md section 1.1). The in-match HUD (UI/KGHUD.cpp,
 * canvas), the chore panel (Chores/UI, Slate) and the front end (UI/Menu, Slate) all read their colour roles, type
 * scale, spacing grid, radii and motion from here, so a change lands everywhere at once.
 *
 * Sizes are in 1080p design pixels: the HUD multiplies by S = ClipY / 1080, Slate screens by their own scale.
 */
namespace KGUI
{
	// ---- Colour roles (sRGB) ------------------------------------------------------------------------------------
	inline constexpr uint32 Ink = 0x120D17;        // deepest background
	inline constexpr uint32 Night = 0x1D1524;      // panel base
	inline constexpr uint32 Panel = 0x2A1F31;      // raised panel / row
	inline constexpr uint32 PanelHi = 0x3A2B42;    // hovered row
	inline constexpr uint32 Cream = 0xF6E7C8;      // primary text
	inline constexpr uint32 CreamDim = 0xCDBB98;   // secondary text
	inline constexpr uint32 Muted = 0x8E7D6E;      // captions, disabled
	inline constexpr uint32 Gold = 0xF2C230;       // brass: focus, the active thing, keycaps
	inline constexpr uint32 GoldLight = 0xFFE096;  // brass highlight (bar tops)
	inline constexpr uint32 Lantern = 0xF28C28;    // lighthouse orange: primary accent, chore markers
	inline constexpr uint32 Crimson = 0xC8102E;    // Impatient / danger fills
	inline constexpr uint32 CrimsonText = 0xFF485E; // crimson lifted for text on dark ink
	inline constexpr uint32 Ghost = 0x4FD1C5;      // ghosts
	inline constexpr uint32 Good = 0x8BD160;       // done / confirmation green
	inline constexpr uint32 GoodLight = 0xAAECA0;
	inline constexpr uint32 Neutral = 0xB696F2;    // neutral roles (violet)
	inline constexpr uint32 Night2 = 0x8CACFF;     // night phase
	inline constexpr uint32 Dawn = 0xFF925C;       // dawn phase
	inline constexpr uint32 Water = 0x3FA7D6;      // liquids, item waypoints

	// ---- Type scale (px at 1080p, Roboto) -------------------------------------------------------------------------
	inline constexpr float TypeDisplay = 38.0f;   // clock, big numbers
	inline constexpr float TypeTitle = 24.0f;     // phase name, card titles
	inline constexpr float TypeHeading = 19.0f;   // list rows, prompts
	inline constexpr float TypeBody = 16.0f;      // step lines, hints
	inline constexpr float TypeCaption = 13.0f;   // uppercase captions (tracking 2.5)
	inline constexpr float TypeMicro = 11.0f;     // tags
	inline constexpr float CaptionTracking = 2.5f;

	// ---- Spacing grid (8 px) ---------------------------------------------------------------------------------------
	inline constexpr float Grid = 8.0f;
	inline constexpr float Margin = 24.0f;        // screen edge to any HUD card
	inline constexpr float Gutter = 16.0f;        // between cards
	inline constexpr float PadCard = 20.0f;       // inside a card
	inline constexpr float RowH = 36.0f;          // list row

	// ---- Shape -----------------------------------------------------------------------------------------------------
	inline constexpr float RadiusCard = 18.0f;
	inline constexpr float RadiusInset = 12.0f;
	inline constexpr float RadiusKey = 6.0f;
	inline constexpr float CardAlpha = 0.72f;     // translucent dusk cards over the world
	inline constexpr float ChipAlpha = 0.62f;     // pills (prompt, location, hints)

	// ---- Motion (seconds) ------------------------------------------------------------------------------------------
	inline constexpr float MotionIn = 0.25f;      // ease-out-back
	inline constexpr float MotionOut = 0.18f;     // ease-out-cubic
	inline constexpr float ToastHold = 2.5f;

	// ---- Chore markers (item 1) ------------------------------------------------------------------------------------
	inline constexpr float MarkerNearMetres = 6.0f;    // at/below: full size
	inline constexpr float MarkerFarMetres = 60.0f;    // at/above: minimum size
	inline constexpr float MarkerScaleMin = 0.55f;
	inline constexpr float MarkerEdgeInset = 56.0f;    // clamp rectangle inset from the screen edge

	inline FLinearColor Color(uint32 RGB, float Alpha = 1.0f)
	{
		return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, static_cast<uint8>(Alpha * 255.0f + 0.5f)));
	}
}
