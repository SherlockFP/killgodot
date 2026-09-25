#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STextBlock;

DECLARE_DELEGATE_OneParam(FKGOnFloatChanged, float);
DECLARE_DELEGATE_OneParam(FKGOnBoolChanged, bool);
DECLARE_DELEGATE_OneParam(FKGOnIndexChanged, int32);

/** Visual flavour of an SKGMenuButton. */
enum class EKGButtonKind : uint8
{
	Menu,      // big left-column title-screen entry: fills brass-gold on hover
	Accent,    // lantern-orange call to action (Host, Join, Apply)
	Secondary, // raised panel button
	Ghost,     // outlined, low emphasis (Back, Cancel)
	Danger,    // Impatient crimson (Quit, Leave)
	Tab,       // settings tab with a brass underline
	Card,      // selectable card with custom content (maps, languages)
	Row        // compact selectable list row with custom content (lobby browser)
};

/**
 * The one button used everywhere in the front end. Visuals are painted (no assets) and animated with springs:
 * hover/focus fills, press squash, a keyboard focus ring. Keyboard: Enter/Space (or gamepad A) click; the mouse
 * moves keyboard focus so arrows continue from what you hover.
 */
class KILLGODOT_API SKGMenuButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGMenuButton)
		: _Kind(EKGButtonKind::Secondary)
		, _Height(52.0f)
		, _MinWidth(0.0f)
		, _TextAlign(HAlign_Center)
		, _IsSelected(false)
	{}
		SLATE_ATTRIBUTE(FText, Text)
		/** Small tag at the right, e.g. "SOON". */
		SLATE_ATTRIBUTE(FText, Badge)
		SLATE_ARGUMENT(EKGButtonKind, Kind)
		SLATE_ARGUMENT(float, Height)
		SLATE_ARGUMENT(float, MinWidth)
		SLATE_ARGUMENT(EHorizontalAlignment, TextAlign)
		SLATE_ATTRIBUTE(bool, IsSelected)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
		/** Custom content (cards). When set, Text is ignored. */
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	/** Fires the click as if pressed (used by keyboard shortcuts). */
	void Click();

	/** Plays the entrance animation after Delay seconds (used for staggered menu reveals). */
	void PlayIntro(float Delay);

private:
	FSlateColor GetTextColor() const;
	FMargin GetTextPadding() const;
	TOptional<FSlateRenderTransform> GetRenderTransformValue() const;
	bool IsHighlighted() const;

	EKGButtonKind Kind = EKGButtonKind::Secondary;
	TAttribute<bool> IsSelected;
	TAttribute<FText> Badge;
	FSimpleDelegate OnClicked;

	float Hover = 0.0f;
	float Press = 0.0f;
	float Selected = 0.0f;
	float Focus = 0.0f;
	float Pop = 0.0f;      // click flash
	float Intro = 1.0f;    // 0 -> 1 entrance
	float IntroDelay = 0.0f;
	bool bPressed = false;
	bool bFocusVisible = false;
};

/** Brass slider with a filled track and a round knob. Left/Right keys step it; the mouse drags it. */
class KILLGODOT_API SKGSlider : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGSlider)
		: _MinValue(0.0f)
		, _MaxValue(1.0f)
		, _StepSize(0.0f)
	{}
		SLATE_ATTRIBUTE(float, Value)
		SLATE_ARGUMENT(float, MinValue)
		SLATE_ARGUMENT(float, MaxValue)
		/** Snapping step (0 = continuous). Keyboard steps use max(StepSize, 5% of the range). */
		SLATE_ARGUMENT(float, StepSize)
		SLATE_EVENT(FKGOnFloatChanged, OnValueChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(260.0, 36.0); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	float Normalized() const;
	void SetFromLocalX(const FGeometry& Geometry, float LocalX);
	void Commit(float NewValue);

	TAttribute<float> Value;
	float MinValue = 0.0f;
	float MaxValue = 1.0f;
	float StepSize = 0.0f;
	FKGOnFloatChanged OnValueChanged;
	float Hot = 0.0f;
	bool bFocusVisible = false;
};

/** Pill on/off switch. Click, Enter/Space or Left/Right toggle it. */
class KILLGODOT_API SKGToggle : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGToggle) {}
		SLATE_ATTRIBUTE(bool, IsChecked)
		SLATE_EVENT(FKGOnBoolChanged, OnToggled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.0, 32.0); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	void Set(bool bValue);

	TAttribute<bool> IsChecked;
	FKGOnBoolChanged OnToggled;
	float Knob = 0.0f;
	float Hot = 0.0f;
	bool bFocusVisible = false;
};

/**
 * "< High >" style selector (classic game settings control): chevrons on both ends, the value in the middle and
 * position pips underneath. Left/Right keys or clicking a side cycles it.
 */
class KILLGODOT_API SKGOptionSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGOptionSelector)
		: _bWrap(false)
	{}
		SLATE_ATTRIBUTE(TArray<FText>, Options)
		SLATE_ATTRIBUTE(int32, SelectedIndex)
		/** Shown instead of Options[SelectedIndex] when bound and non-empty (e.g. "Custom"). */
		SLATE_ATTRIBUTE(FText, OverrideText)
		SLATE_ARGUMENT(bool, bWrap)
		SLATE_EVENT(FKGOnIndexChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	FText GetValueText() const;
	void Step(int32 Direction);

	TAttribute<TArray<FText>> Options;
	TAttribute<int32> SelectedIndex;
	TAttribute<FText> OverrideText;
	FKGOnIndexChanged OnSelectionChanged;
	bool bWrap = false;
	float Hot = 0.0f;
	float Nudge = 0.0f;        // -1..1 text kick in the stepped direction
	int32 HoverSide = 0;       // -1 left chevron, +1 right chevron
	bool bFocusVisible = false;
};

/** One settings line: label (+ optional description) on the left, the control and an optional readout right. */
class KILLGODOT_API SKGSettingRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGSettingRow)
		: _ControlWidth(360.0f)
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Description)
		SLATE_ATTRIBUTE(FText, ValueText)
		SLATE_ARGUMENT(float, ControlWidth)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

private:
	float Hot = 0.0f;
};

/**
 * Centered confirmation card over a dimmed screen. Esc / clicking outside = cancel. Pops in with a spring.
 * Host it in an SOverlay and remove it from OnConfirm / OnCancel.
 */
class KILLGODOT_API SKGModal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGModal)
		: _ConfirmKind(EKGButtonKind::Accent)
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ATTRIBUTE(FText, Body)
		SLATE_ARGUMENT(FText, ConfirmText)
		/** Empty = no cancel button (information popup). */
		SLATE_ARGUMENT(FText, CancelText)
		SLATE_ARGUMENT(EKGButtonKind, ConfirmKind)
		SLATE_EVENT(FSimpleDelegate, OnConfirm)
		SLATE_EVENT(FSimpleDelegate, OnCancel)
		/** Optional widget between the body and the buttons (e.g. a password field). */
		SLATE_NAMED_SLOT(FArguments, ExtraContent)
		/** Focused when the dialog opens instead of the confirm button. */
		SLATE_ARGUMENT(TSharedPtr<SWidget>, InitialFocus)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** The widget that should receive focus when the modal opens. */
	TSharedPtr<SWidget> GetDefaultFocus() const;

private:
	FSimpleDelegate OnConfirm;
	FSimpleDelegate OnCancel;
	TSharedPtr<SKGMenuButton> ConfirmButton;
	TSharedPtr<SKGMenuButton> CancelButton;
	TSharedPtr<SWidget> InitialFocus;
	float Appear = 0.0f;
};

/** Small painted icons (no font glyph dependency: Roboto lacks most symbols). */
enum class EKGGlyph : uint8
{
	Lock,
	Check,
	Cross,
	ChevronUp,
	ChevronDown,
	Spinner,
	Refresh
};

class KILLGODOT_API SKGGlyph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGGlyph)
		: _Glyph(EKGGlyph::Check)
		, _Size(18.0f)
		, _Thickness(2.5f)
	{}
		SLATE_ARGUMENT(EKGGlyph, Glyph)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(float, Thickness)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

	/** Paints Glyph into the square (Pos, Extent) of Geometry's local space; Spin = spinner angle in radians. */
	static void Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, EKGGlyph Glyph, const FVector2f& Pos,
	                  float Extent, const FLinearColor& Color, float Thickness, float Spin = 0.0f);

private:
	EKGGlyph Glyph = EKGGlyph::Check;
	TAttribute<FLinearColor> Color;
	float Size = 18.0f;
	float Thickness = 2.5f;
	float Spin = 0.0f;
};

/** Square check box (ready flags). Click / Enter / Space toggles; bReadOnly shows the state only. */
class KILLGODOT_API SKGCheckBox : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGCheckBox)
		: _Size(28.0f)
		, _bReadOnly(false)
	{}
		SLATE_ATTRIBUTE(bool, IsChecked)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(bool, bReadOnly)
		SLATE_EVENT(FKGOnBoolChanged, OnToggled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return !bReadOnly; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	TAttribute<bool> IsChecked;
	FKGOnBoolChanged OnToggled;
	float Size = 28.0f;
	bool bReadOnly = false;
	float Fill = 0.0f;
	float Hot = 0.0f;
	bool bFocusVisible = false;
};

/** Clickable table column caption with a sort chevron (server browser). */
class KILLGODOT_API SKGSortHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGSortHeader)
		: _Justify(ETextJustify::Left)
	{}
		SLATE_ARGUMENT(FText, Text)
		SLATE_ARGUMENT(ETextJustify::Type, Justify)
		/** 0 = not sorted by this column, 1 = ascending, -1 = descending. */
		SLATE_ATTRIBUTE(int32, SortState)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	TAttribute<int32> SortState;
	FSimpleDelegate OnClicked;
};

/** Small helpers for consistent layout. */
namespace KGMenuUI
{
	/** "DISPLAY ---------" caption line used between setting groups. */
	KILLGODOT_API TSharedRef<SWidget> MakeSectionHeader(const FText& Text);

	/** Rounded key cap ("W", "Shift", "LMB"). */
	KILLGODOT_API TSharedRef<SWidget> MakeKeyCap(const FText& Text);

	/** Panel title with a lantern underline and an optional subtitle (bCompact: smaller, for dialogs). */
	KILLGODOT_API TSharedRef<SWidget> MakePanelTitle(const FText& Title, const FText& Subtitle = FText::GetEmpty(),
	                                                 bool bCompact = false);

	/** Keyboard/controller hint shown in footers: [Esc] Back. */
	KILLGODOT_API TSharedRef<SWidget> MakeKeyHint(const FText& Key, const FText& Action);
}
