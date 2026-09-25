#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class FSlateWindowElementList;
struct FGeometry;

/**
 * Palette, fonts, widget styles and paint helpers shared by every front-end screen (Docs/08_UI_UX.md section 1):
 * warm cream "parchment" text on a dark dusk-plum base, lantern-orange and brass-gold accents, Impatient crimson,
 * ghost cyan. Everything is code-built (no assets), Roboto from the engine covers Turkish and Cyrillic glyphs.
 */
class KILLGODOT_API FKGMenuStyle
{
public:
	static const FKGMenuStyle& Get();

	// --- Palette ------------------------------------------------------------------------------------------------
	FLinearColor Ink;       // #120D17 deepest background
	FLinearColor Night;     // #1D1524 panel base
	FLinearColor Panel;     // #2A1F31 raised panel / row
	FLinearColor PanelHi;   // #3A2B42 hovered row
	FLinearColor Cream;     // #F6E7C8 primary text
	FLinearColor CreamDim;  // #CDBB98 secondary text
	FLinearColor Muted;     // #8E7D6E captions, disabled
	FLinearColor Gold;      // #F2C230 brass highlight
	FLinearColor Lantern;   // #F28C28 lighthouse orange (primary accent)
	FLinearColor Crimson;   // #C8102E Impatient red (danger)
	FLinearColor Ghost;     // #4FD1C5 ghost cyan
	FLinearColor Good;      // #8BD160 confirmation green

	// --- Fonts (engine Roboto typefaces) -----------------------------------------------------------------------
	FSlateFontInfo TitleFont;       // Black 108
	FSlateFontInfo HeadingFont;     // Black 34
	FSlateFontInfo SubheadingFont;  // Bold 20
	FSlateFontInfo MenuButtonFont;  // Black 24
	FSlateFontInfo ButtonFont;      // Bold 18
	FSlateFontInfo BodyFont;        // Regular 17
	FSlateFontInfo BodyBoldFont;    // Bold 17
	FSlateFontInfo SmallFont;       // Medium 14
	FSlateFontInfo CaptionFont;     // Bold 12, wide tracking
	FSlateFontInfo KeyFont;         // Black 15

	// --- Widget styles ------------------------------------------------------------------------------------------
	FButtonStyle BareButton;             // invisible button (visuals are painted by the owning widget)
	FEditableTextBoxStyle TextBox;
	FScrollBarStyle ScrollBar;
	FScrollBoxStyle ScrollBox;
	FSlateColorBrush WhiteBrush = FSlateColorBrush(FLinearColor::White);
	FSlateBrush NoBrush;
	FSlateBrush PanelBrush;      // translucent dusk panel, rounded 20, hairline outline
	FSlateBrush PanelSolidBrush; // opaque variant (modals)
	FSlateBrush BadgeBrush;      // small brass tag ("SOON")
	FSlateBrush KeyCapBrush;     // keyboard key cap
	FSlateBrush ToastBrush;      // notification pill
	FSlateBrush InsetBrush;      // darker inset well (cards, notes)

	// --- Helpers ------------------------------------------------------------------------------------------------
	static FSlateFontInfo Font(FName Typeface, float Size, int32 LetterSpacing = 0);
	static FLinearColor Hex(uint32 RGB, float Alpha = 1.0f);
	static FLinearColor WithAlpha(const FLinearColor& Color, float Alpha);

	/** Frame-rate independent exponential approach (used by every hover/press animation). */
	static float Approach(float Current, float Target, float DeltaTime, float Speed = 14.0f);

	/** Ease-out-back curve (a small overshoot = the "spring" feel from the UX doc). */
	static float EaseOutBack(float T);

	/**
	 * Filled rounded rectangle in the widget's local space. Radius < 0 = pill / circle. The outline is drawn inside
	 * the box. Colors are final (multiply by the widget style tint yourself when fading).
	 */
	static void DrawRoundedBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
	                           const FVector2f& Size, const FLinearColor& Fill, float Radius,
	                           const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);

private:
	FKGMenuStyle();
};

/**
 * Tiny immediate-mode triangle batcher for painted art (backdrop, logo, chevrons): gradients, soft discs and
 * silhouettes with per-vertex color. Local-space coordinates; one draw element per Flush().
 */
class KILLGODOT_API FKGVertexCanvas
{
public:
	FKGVertexCanvas(FSlateWindowElementList& InOut, const FGeometry& Geometry, float InOpacity = 1.0f);

	void Tri(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FLinearColor& CA, const FLinearColor& CB,
	         const FLinearColor& CC);
	void Quad(const FVector2f& TL, const FVector2f& TR, const FVector2f& BR, const FVector2f& BL, const FLinearColor& CTL,
	          const FLinearColor& CTR, const FLinearColor& CBR, const FLinearColor& CBL);
	/** Axis-aligned rect with a top->bottom gradient. */
	void RectV(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Top, const FLinearColor& Bottom);
	/** Axis-aligned rect with a left->right gradient. */
	void RectH(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Left, const FLinearColor& Right);
	/** Radial gradient ellipse (Inner at the centre, Outer at the rim). */
	void Ellipse(const FVector2f& Centre, const FVector2f& Radii, const FLinearColor& Inner, const FLinearColor& Outer,
	             int32 Segments = 36);
	/** Filled area under a polyline down to BottomY (vertical gradient). Points must be sorted by X. */
	void Ridge(const TArray<FVector2f>& Top, float BottomY, const FLinearColor& TopColor, const FLinearColor& BottomColor);
	/** Filled convex polygon, single colour. */
	void Convex(const TArray<FVector2f>& Points, const FLinearColor& Color);

	/** Emits everything queued so far on Layer and clears the queue. */
	void Flush(int32 Layer);

private:
	SlateIndex AddVertex(const FVector2f& Position, const FLinearColor& Color);

	FSlateWindowElementList& Out;
	FSlateRenderTransform Transform;
	FSlateResourceHandle Handle;
	float Opacity;
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
};
