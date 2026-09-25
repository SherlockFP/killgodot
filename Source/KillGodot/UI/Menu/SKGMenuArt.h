#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/**
 * Painted, animated title backdrop (Docs/08_UI_UX.md section 3 "dusk harbour"): gradient sky with twinkling stars and a
 * moon, drifting clouds and fog, the village headland with lit windows, a lighthouse whose twin beams sweep the
 * sky, a rippling sea with glints, rising embers and a vignette. No assets; everything is vertex-coloured geometry.
 */
class KILLGODOT_API SKGMenuBackground : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGMenuBackground)
		: _DimLeft(0.85f)
	{}
		/** Opacity of the dark gradient behind the left menu column (0 = none). */
		SLATE_ATTRIBUTE(float, DimLeft)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.0, 64.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

private:
	TAttribute<float> DimLeft;
	double StartTime = 0.0;
};

/**
 * The "O" of GODOT drawn as a cracked clock face whose minute hand ticks (Docs/08_UI_UX.md section 3 logo). Sized from
 * the given font's metrics so it sits on the same baseline as the neighbouring letters.
 */
class KILLGODOT_API SKGClockGlyph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGClockGlyph) {}
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_ARGUMENT(FVector2D, ShadowOffset)
		SLATE_ARGUMENT(FLinearColor, ShadowColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

private:
	float GetEm() const;

	FSlateFontInfo Font;
	FVector2D ShadowOffset = FVector2D::ZeroVector;
	FLinearColor ShadowColor = FLinearColor::Black;
	double StartTime = 0.0;
};

/** "KILLGO" wordmark with the clock "O", a warm poster shadow and the tagline. */
class KILLGODOT_API SKGLogo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGLogo)
		: _Size(108.0f)
		, _ShowTagline(true)
	{}
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(bool, ShowTagline)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
