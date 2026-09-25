#include "UI/Menu/SKGMenuWidgets.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "UI/Menu/KGMenuStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Tint for painted elements: parent fade * own intro fade * disabled dimming. */
	FLinearColor WidgetTint(const FLinearColor& Color, float Opacity)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Opacity);
	}

	bool IsTextEditorWidget(const TSharedPtr<SWidget>& Widget)
	{
		static const FName EditableText(TEXT("SEditableText"));
		static const FName MultiLineText(TEXT("SMultiLineEditableText"));
		return Widget.IsValid() && (Widget->GetType() == EditableText || Widget->GetType() == MultiLineText);
	}

	/** Scales RGB only (keeps alpha). */
	FLinearColor WidgetShade(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, Color.A);
	}

	bool IsVisibleFocusCause(EFocusCause Cause)
	{
		return Cause == EFocusCause::Navigation || Cause == EFocusCause::SetDirectly;
	}
}

// ==================================================================================================================
// SKGMenuButton
// ==================================================================================================================

void SKGMenuButton::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	Kind = InArgs._Kind;
	IsSelected = InArgs._IsSelected;
	Badge = InArgs._Badge;
	OnClicked = InArgs._OnClicked;
	Selected = IsSelected.Get() ? 1.0f : 0.0f;

	const bool bCustom = InArgs._Content.Widget != SNullWidget::NullWidget;
	const FSlateFontInfo& Font = Kind == EKGButtonKind::Menu ? S.MenuButtonFont : S.ButtonFont;

	TSharedRef<SWidget> Inner = InArgs._Content.Widget;
	if (!bCustom)
	{
		Inner = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(InArgs._TextAlign)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._Text)
				.Font(Font)
				.ColorAndOpacity(this, &SKGMenuButton::GetTextColor)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(&S.BadgeBrush)
				.Padding(FMargin(8.0f, 3.0f))
				.Visibility_Lambda([this]()
				{
					return Badge.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
				})
				[
					SNew(STextBlock)
					.Text(Badge)
					.Font(S.CaptionFont)
					.ColorAndOpacity(S.Gold)
				]
			];
	}

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(InArgs._Height)
		.MinDesiredWidth(InArgs._MinWidth)
		.Padding(this, &SKGMenuButton::GetTextPadding)
		.VAlign(VAlign_Center)
		[
			Inner
		]
	];

	SetColorAndOpacity(TAttribute<FLinearColor>::CreateLambda([this]()
	{
		return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Intro, 0.0f, 1.0f));
	}));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateSP(
		this, &SKGMenuButton::GetRenderTransformValue));
}

bool SKGMenuButton::IsHighlighted() const
{
	return IsHovered() || (bFocusVisible && HasKeyboardFocus());
}

void SKGMenuButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const bool bEnabled = IsEnabled();
	Hover = FKGMenuStyle::Approach(Hover, bEnabled && IsHighlighted() ? 1.0f : 0.0f, InDeltaTime, 16.0f);
	Press = FKGMenuStyle::Approach(Press, bPressed ? 1.0f : 0.0f, InDeltaTime, 30.0f);
	Selected = FKGMenuStyle::Approach(Selected, IsSelected.Get() ? 1.0f : 0.0f, InDeltaTime, 14.0f);
	Focus = FKGMenuStyle::Approach(Focus, bFocusVisible && HasKeyboardFocus() ? 1.0f : 0.0f, InDeltaTime, 18.0f);
	Pop = FKGMenuStyle::Approach(Pop, 0.0f, InDeltaTime, 5.0f);
	if (IntroDelay > 0.0f)
	{
		IntroDelay -= InDeltaTime;
	}
	else if (Intro < 1.0f)
	{
		Intro = FMath::Min(1.0f, Intro + InDeltaTime / 0.32f);
	}
}

TOptional<FSlateRenderTransform> SKGMenuButton::GetRenderTransformValue() const
{
	const float IntroEase = FKGMenuStyle::EaseOutBack(Intro);
	FVector2f Offset = FVector2f::ZeroVector;
	float Scale = 1.0f;
	switch (Kind)
	{
	case EKGButtonKind::Menu:
		Offset.X = -48.0f * (1.0f - IntroEase);
		break;
	case EKGButtonKind::Tab:
		Offset.Y = 10.0f * (1.0f - IntroEase);
		break;
	case EKGButtonKind::Card:
		Scale = 1.0f + 0.012f * Hover - 0.02f * Press;
		Offset.Y = 14.0f * (1.0f - IntroEase);
		break;
	case EKGButtonKind::Row:
		Offset.X = 4.0f * Hover;
		break;
	default:
		Scale = 1.0f + 0.03f * Hover - 0.04f * Press;
		Offset.Y = 14.0f * (1.0f - IntroEase);
		break;
	}
	if (FMath::IsNearlyEqual(Scale, 1.0f) && Offset.IsNearlyZero())
	{
		return TOptional<FSlateRenderTransform>();
	}
	return FSlateRenderTransform(FScale2f(Scale), Offset);
}

FMargin SKGMenuButton::GetTextPadding() const
{
	switch (Kind)
	{
	case EKGButtonKind::Menu:
		return FMargin(28.0f + 18.0f * Hover, 0.0f, 20.0f, 0.0f);
	case EKGButtonKind::Tab:
		return FMargin(18.0f, 0.0f, 18.0f, 4.0f);
	case EKGButtonKind::Card:
		return FMargin(22.0f, 16.0f);
	case EKGButtonKind::Row:
		return FMargin(18.0f, 9.0f);
	default:
		return FMargin(28.0f, 0.0f);
	}
}

FSlateColor SKGMenuButton::GetTextColor() const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	FLinearColor Color;
	switch (Kind)
	{
	case EKGButtonKind::Menu:
		Color = FMath::Lerp(S.Cream, S.Ink, Hover);
		break;
	case EKGButtonKind::Accent:
		Color = S.Ink;
		break;
	case EKGButtonKind::Ghost:
		Color = FMath::Lerp(S.CreamDim, S.Cream, Hover);
		break;
	case EKGButtonKind::Tab:
		Color = FMath::Lerp(S.Muted, S.Cream, FMath::Max(Selected, Hover));
		break;
	default:
		Color = S.Cream;
		break;
	}
	if (!IsEnabled())
	{
		Color.A *= 0.45f;
	}
	return FSlateColor(Color);
}

int32 SKGMenuButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * FMath::Clamp(Intro, 0.0f, 1.0f) * (bEnabled ? 1.0f : 0.45f);
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float Active = FMath::Max(Hover, Selected);
	float Radius = 12.0f;

	switch (Kind)
	{
	case EKGButtonKind::Menu:
	{
		Radius = 10.0f;
		const FLinearColor FillColor = FMath::Lerp(S.Gold, S.Lantern, Press);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(S.Cream, 0.07f * Selected * (1.0f - Hover) * Opacity), Radius);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(FillColor, Hover * Opacity), Radius);
		const float BarHeight = Size.Y * FMath::Lerp(0.34f, 0.72f, Active);
		const FLinearColor BarColor = FMath::Lerp(S.Lantern, S.Ink, Hover);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry,
		                             FVector2f(10.0f, (Size.Y - BarHeight) * 0.5f), FVector2f(5.0f + 2.0f * Active, BarHeight),
		                             WidgetTint(BarColor, FMath::Lerp(0.55f, 1.0f, Active) * Opacity), 3.0f);
		break;
	}
	case EKGButtonKind::Accent:
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(WidgetShade(FMath::Lerp(S.Lantern, S.Gold, Hover), 1.0f - 0.12f * Press), Opacity),
		                             Radius);
		break;
	case EKGButtonKind::Danger:
	{
		const FLinearColor Base = WidgetShade(S.Crimson, 0.9f);
		const FLinearColor Hot = FMath::Lerp(S.Crimson, FLinearColor::White, 0.14f);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(FMath::Lerp(Base, Hot, Hover), Opacity), Radius);
		break;
	}
	case EKGButtonKind::Secondary:
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(FMath::Lerp(S.Panel, S.PanelHi, Hover), Opacity), Radius,
		                             WidgetTint(FMath::Lerp(FKGMenuStyle::WithAlpha(S.Cream, 0.12f), S.Gold, Hover), Opacity),
		                             1.5f);
		break;
	case EKGButtonKind::Ghost:
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(S.Cream, 0.07f * Hover * Opacity), Radius,
		                             WidgetTint(S.Cream, FMath::Lerp(0.22f, 0.55f, Hover) * Opacity), 1.5f);
		break;
	case EKGButtonKind::Tab:
	{
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(S.Cream, 0.05f * Hover * Opacity), 10.0f);
		const float LineWidth = (Size.X - 28.0f) * FMath::Max(Selected, 0.35f * Hover);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry,
		                             FVector2f((Size.X - LineWidth) * 0.5f, Size.Y - 4.0f), FVector2f(LineWidth, 4.0f),
		                             WidgetTint(S.Gold, Opacity), -1.0f);
		break;
	}
	case EKGButtonKind::Row:
	{
		Radius = 10.0f;
		const FLinearColor Outline = FMath::Lerp(FKGMenuStyle::WithAlpha(S.Cream, 0.05f + 0.25f * Hover), S.Gold, Selected);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(FMath::Lerp(S.Panel, S.PanelHi, FMath::Max(Hover, 0.6f * Selected)), 0.85f * Opacity),
		                             Radius, WidgetTint(Outline, Opacity), FMath::Lerp(1.0f, 2.0f, Selected));
		break;
	}
	case EKGButtonKind::Card:
	{
		Radius = 14.0f;
		const FLinearColor Outline = FMath::Lerp(FKGMenuStyle::WithAlpha(S.Cream, 0.10f + 0.30f * Hover), S.Gold, Selected);
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
		                             WidgetTint(FMath::Lerp(S.Panel, S.PanelHi, 0.7f * Hover), 0.92f * Opacity), Radius,
		                             WidgetTint(Outline, Opacity), FMath::Lerp(1.5f, 2.5f, Selected));
		break;
	}
	}

	// Keyboard focus ring (the Menu and Tab kinds show focus as their hover state instead).
	if (Focus > 0.01f && Kind != EKGButtonKind::Menu && Kind != EKGButtonKind::Tab)
	{
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(-4.0f, -4.0f),
		                             Size + FVector2f(8.0f, 8.0f), FLinearColor::Transparent, Radius + 4.0f,
		                             WidgetTint(S.Gold, 0.9f * Focus * Opacity), 2.0f);
	}
	// Click pop: an outline that expands and fades.
	if (Pop > 0.01f)
	{
		const float Grow = (1.0f - Pop) * 12.0f;
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(-Grow, -Grow),
		                             Size + FVector2f(2.0f * Grow, 2.0f * Grow), FLinearColor::Transparent, Radius + Grow,
		                             WidgetTint(S.Cream, 0.55f * Pop * Opacity), 2.0f);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle,
	                                bEnabled);
}

FReply SKGMenuButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
	{
		return FReply::Unhandled();
	}
	bPressed = true;
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGMenuButton::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SKGMenuButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
	{
		return FReply::Unhandled();
	}
	bPressed = false;
	const bool bInside = MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	FReply Reply = FReply::Handled().ReleaseMouseCapture();
	if (bInside && IsEnabled())
	{
		Click();
	}
	return Reply;
}

void SKGMenuButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	if (!IsEnabled() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	// Focus follows the mouse so keyboard navigation continues from here (but never steal from a text field).
	const uint32 UserIndex = MouseEvent.GetUserIndex();
	if (!IsTextEditorWidget(FSlateApplication::Get().GetUserFocusedWidget(UserIndex)))
	{
		FSlateApplication::Get().SetUserFocus(UserIndex, SharedThis(this), EFocusCause::Mouse);
	}
}

FReply SKGMenuButton::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsEnabled() && !InKeyEvent.IsRepeat() &&
		FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		Press = 1.0f;
		Click();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGMenuButton::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	bFocusVisible = IsVisibleFocusCause(InFocusEvent.GetCause());
	return FReply::Handled();
}

void SKGMenuButton::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusLost(InFocusEvent);
	bFocusVisible = false;
	bPressed = false;
}

FCursorReply SKGMenuButton::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

void SKGMenuButton::Click()
{
	Pop = 1.0f;
	OnClicked.ExecuteIfBound();
}

void SKGMenuButton::PlayIntro(float Delay)
{
	Intro = 0.0f;
	IntroDelay = FMath::Max(Delay, 0.0f);
}

// ==================================================================================================================
// SKGSlider
// ==================================================================================================================

namespace
{
	constexpr float KGSliderPad = 13.0f;
	constexpr float KGKnobRadius = 11.0f;
}

void SKGSlider::Construct(const FArguments& InArgs)
{
	Value = InArgs._Value;
	MinValue = InArgs._MinValue;
	MaxValue = FMath::Max(InArgs._MaxValue, InArgs._MinValue + UE_KINDA_SMALL_NUMBER);
	StepSize = FMath::Max(InArgs._StepSize, 0.0f);
	OnValueChanged = InArgs._OnValueChanged;
}

float SKGSlider::Normalized() const
{
	return FMath::Clamp((Value.Get() - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
}

void SKGSlider::Commit(float NewValue)
{
	float Snapped = NewValue;
	if (StepSize > 0.0f)
	{
		Snapped = MinValue + FMath::RoundToFloat((NewValue - MinValue) / StepSize) * StepSize;
	}
	Snapped = FMath::Clamp(Snapped, MinValue, MaxValue);
	if (!FMath::IsNearlyEqual(Snapped, Value.Get(), UE_KINDA_SMALL_NUMBER))
	{
		OnValueChanged.ExecuteIfBound(Snapped);
	}
}

void SKGSlider::SetFromLocalX(const FGeometry& Geometry, float LocalX)
{
	const float Track = FMath::Max(static_cast<float>(Geometry.GetLocalSize().X) - 2.0f * KGSliderPad, 1.0f);
	const float T = FMath::Clamp((LocalX - KGSliderPad) / Track, 0.0f, 1.0f);
	Commit(MinValue + T * (MaxValue - MinValue));
}

void SKGSlider::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const bool bHot = IsEnabled() && (IsHovered() || HasMouseCapture() || (bFocusVisible && HasKeyboardFocus()));
	Hot = FKGMenuStyle::Approach(Hot, bHot ? 1.0f : 0.0f, InDeltaTime, 16.0f);
}

int32 SKGSlider::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                         FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * (bEnabled ? 1.0f : 0.45f);
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float CentreY = Size.Y * 0.5f;
	const float Track = FMath::Max(Size.X - 2.0f * KGSliderPad, 1.0f);
	const float KnobX = KGSliderPad + Normalized() * Track;

	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f(KGSliderPad, CentreY - 4.0f),
	                             FVector2f(Track, 8.0f), WidgetTint(S.Cream, 0.12f * Opacity), -1.0f);
	if (KnobX - KGSliderPad > 1.0f)
	{
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(KGSliderPad, CentreY - 4.0f),
		                             FVector2f(KnobX - KGSliderPad, 8.0f),
		                             WidgetTint(FMath::Lerp(S.Lantern, S.Gold, Hot), Opacity), -1.0f);
	}
	if (Hot > 0.01f)
	{
		const float Glow = 17.0f + 3.0f * Hot;
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(KnobX - Glow, CentreY - Glow),
		                             FVector2f(2.0f * Glow, 2.0f * Glow), WidgetTint(S.Gold, 0.20f * Hot * Opacity), -1.0f);
	}
	const float Knob = KGKnobRadius + 1.5f * Hot;
	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2f(KnobX - Knob, CentreY - Knob),
	                             FVector2f(2.0f * Knob, 2.0f * Knob), WidgetTint(S.Cream, Opacity), -1.0f,
	                             WidgetTint(S.Gold, Hot * Opacity), 3.0f);
	return LayerId + 2;
}

FReply SKGSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
	{
		return FReply::Unhandled();
	}
	SetFromLocalX(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGSlider::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	SetFromLocalX(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
	return FReply::Handled();
}

FReply SKGSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SKGSlider::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const EUINavigation Direction = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
	if (IsEnabled() && (Direction == EUINavigation::Left || Direction == EUINavigation::Right))
	{
		const float KeyStep = FMath::Max(StepSize, (MaxValue - MinValue) * 0.05f);
		Commit(Value.Get() + (Direction == EUINavigation::Right ? KeyStep : -KeyStep));
		return FReply::Handled();
	}
	return SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGSlider::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	bFocusVisible = IsVisibleFocusCause(InFocusEvent.GetCause());
	return FReply::Handled();
}

void SKGSlider::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SLeafWidget::OnFocusLost(InFocusEvent);
	bFocusVisible = false;
}

FCursorReply SKGSlider::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

// ==================================================================================================================
// SKGToggle
// ==================================================================================================================

void SKGToggle::Construct(const FArguments& InArgs)
{
	IsChecked = InArgs._IsChecked;
	OnToggled = InArgs._OnToggled;
	Knob = IsChecked.Get() ? 1.0f : 0.0f;
}

void SKGToggle::Set(bool bValue)
{
	if (IsEnabled() && bValue != IsChecked.Get())
	{
		OnToggled.ExecuteIfBound(bValue);
	}
}

void SKGToggle::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Knob = FKGMenuStyle::Approach(Knob, IsChecked.Get() ? 1.0f : 0.0f, InDeltaTime, 18.0f);
	const bool bHot = IsEnabled() && (IsHovered() || (bFocusVisible && HasKeyboardFocus()));
	Hot = FKGMenuStyle::Approach(Hot, bHot ? 1.0f : 0.0f, InDeltaTime, 16.0f);
}

int32 SKGToggle::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                         FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * (bEnabled ? 1.0f : 0.45f);
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const FVector2f TrackSize(60.0f, 30.0f);
	const FVector2f TrackPos(0.0f, (Size.Y - TrackSize.Y) * 0.5f);

	const FLinearColor Off = FKGMenuStyle::WithAlpha(S.Cream, 0.14f);
	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, TrackPos, TrackSize,
	                             WidgetTint(FMath::Lerp(Off, S.Lantern, Knob), Opacity), -1.0f,
	                             WidgetTint(S.Gold, Hot * Opacity), 2.0f);
	const float KnobSize = 22.0f;
	const float KnobX = FMath::Lerp(TrackPos.X + 4.0f, TrackPos.X + TrackSize.X - KnobSize - 4.0f, Knob);
	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry,
	                             FVector2f(KnobX, TrackPos.Y + (TrackSize.Y - KnobSize) * 0.5f), FVector2f(KnobSize, KnobSize),
	                             WidgetTint(FMath::Lerp(S.CreamDim, S.Cream, Knob), Opacity), -1.0f);
	return LayerId + 1;
}

FReply SKGToggle::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
	{
		return FReply::Unhandled();
	}
	Set(!IsChecked.Get());
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGToggle::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!IsEnabled())
	{
		return FReply::Unhandled();
	}
	if (!InKeyEvent.IsRepeat() &&
		FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		Set(!IsChecked.Get());
		return FReply::Handled();
	}
	const EUINavigation Direction = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		Set(Direction == EUINavigation::Right);
		return FReply::Handled();
	}
	return SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGToggle::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	bFocusVisible = IsVisibleFocusCause(InFocusEvent.GetCause());
	return FReply::Handled();
}

void SKGToggle::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SLeafWidget::OnFocusLost(InFocusEvent);
	bFocusVisible = false;
}

FCursorReply SKGToggle::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

// ==================================================================================================================
// SKGOptionSelector
// ==================================================================================================================

namespace
{
	constexpr int32 KGMaxPips = 8;
}

void SKGOptionSelector::Construct(const FArguments& InArgs)
{
	Options = InArgs._Options;
	SelectedIndex = InArgs._SelectedIndex;
	OverrideText = InArgs._OverrideText;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	bWrap = InArgs._bWrap;

	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(46.0f)
		.MinDesiredWidth(240.0f)
		.Padding_Lambda([this]()
		{
			const int32 Count = Options.Get().Num();
			return FMargin(46.0f, 0.0f, 46.0f, Count > 1 && Count <= KGMaxPips ? 7.0f : 0.0f);
		})
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SKGOptionSelector::GetValueText)
			.Font(FKGMenuStyle::Get().BodyBoldFont)
			.ColorAndOpacity(FKGMenuStyle::Get().Cream)
			.RenderTransform_Lambda([this]()
			{
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(14.0f * Nudge, 0.0f)));
			})
		]
	];
}

FText SKGOptionSelector::GetValueText() const
{
	const FText Override = OverrideText.Get();
	if (!Override.IsEmpty())
	{
		return Override;
	}
	const TArray<FText> Values = Options.Get();
	const int32 Index = SelectedIndex.Get();
	return Values.IsValidIndex(Index) ? Values[Index] : FText::GetEmpty();
}

void SKGOptionSelector::Step(int32 Direction)
{
	const int32 Count = Options.Get().Num();
	if (Count == 0 || !IsEnabled())
	{
		return;
	}
	int32 Current = SelectedIndex.Get();
	if (!FMath::IsWithin(Current, 0, Count))
	{
		Current = Direction > 0 ? -1 : Count;
	}
	int32 Next = Current + Direction;
	Next = bWrap ? ((Next % Count) + Count) % Count : FMath::Clamp(Next, 0, Count - 1);
	if (Next != SelectedIndex.Get())
	{
		Nudge = static_cast<float>(Direction);
		OnSelectionChanged.ExecuteIfBound(Next);
	}
}

void SKGOptionSelector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const bool bHot = IsEnabled() && (IsHovered() || (bFocusVisible && HasKeyboardFocus()));
	Hot = FKGMenuStyle::Approach(Hot, bHot ? 1.0f : 0.0f, InDeltaTime, 16.0f);
	Nudge = FKGMenuStyle::Approach(Nudge, 0.0f, InDeltaTime, 14.0f);
}

int32 SKGOptionSelector::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                                 FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                 bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * (bEnabled ? 1.0f : 0.45f);
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const bool bFocused = bFocusVisible && HasKeyboardFocus();

	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
	                             WidgetTint(FMath::Lerp(S.Ink, S.Panel, Hot), 0.85f * Opacity), 12.0f,
	                             WidgetTint(bFocused ? S.Gold : FKGMenuStyle::WithAlpha(S.Cream, 0.12f + 0.25f * Hot), Opacity),
	                             bFocused ? 2.0f : 1.5f);

	// Chevrons (dimmed at the ends when the list does not wrap).
	const int32 Count = Options.Get().Num();
	const int32 Index = SelectedIndex.Get();
	const float CentreY = Size.Y * 0.5f - (Count > 1 && Count <= KGMaxPips ? 3.0f : 0.0f);
	auto DrawChevron = [&](float X, float Dir, bool bCanStep, bool bHover)
	{
		const float Alpha = bCanStep ? (bHover ? 1.0f : 0.75f) : 0.2f;
		const FLinearColor Color = bHover && bCanStep ? S.Gold : S.CreamDim;
		TArray<FVector2f> Points;
		Points.Add(FVector2f(X - 4.0f * Dir, CentreY - 7.0f));
		Points.Add(FVector2f(X + 3.0f * Dir, CentreY));
		Points.Add(FVector2f(X - 4.0f * Dir, CentreY + 7.0f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
		                             ESlateDrawEffect::None, WidgetTint(Color, Alpha * Opacity), true, 2.5f);
	};
	const bool bCanLeft = bWrap || Index != 0;
	const bool bCanRight = bWrap || Index < Count - 1;
	DrawChevron(22.0f, -1.0f, Count > 1 && bCanLeft, HoverSide < 0);
	DrawChevron(Size.X - 22.0f, 1.0f, Count > 1 && bCanRight, HoverSide > 0);

	// Position pips.
	if (Count > 1 && Count <= KGMaxPips)
	{
		constexpr float PipW = 10.0f;
		constexpr float SelW = 20.0f;
		constexpr float Gap = 5.0f;
		float Total = Gap * static_cast<float>(Count - 1);
		for (int32 Pip = 0; Pip < Count; ++Pip)
		{
			Total += Pip == Index ? SelW : PipW;
		}
		float X = (Size.X - Total) * 0.5f;
		for (int32 Pip = 0; Pip < Count; ++Pip)
		{
			const bool bSel = Pip == Index;
			const float W = bSel ? SelW : PipW;
			FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(X, Size.Y - 10.0f),
			                             FVector2f(W, 3.0f),
			                             WidgetTint(bSel ? S.Gold : FKGMenuStyle::WithAlpha(S.Cream, 0.22f), Opacity), -1.0f);
			X += W + Gap;
		}
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle,
	                                bEnabled);
}

FReply SKGOptionSelector::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
	{
		return FReply::Unhandled();
	}
	const float LocalX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	Step(LocalX < MyGeometry.GetLocalSize().X * 0.35f ? -1 : 1);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGOptionSelector::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SKGOptionSelector::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float LocalX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	const float Width = MyGeometry.GetLocalSize().X;
	HoverSide = LocalX < Width * 0.35f ? -1 : 1;
	return FReply::Unhandled();
}

void SKGOptionSelector::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	HoverSide = 0;
}

FReply SKGOptionSelector::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const EUINavigation Direction = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		Step(Direction == EUINavigation::Right ? 1 : -1);
		return FReply::Handled();
	}
	if (!InKeyEvent.IsRepeat() &&
		FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		const int32 Count = Options.Get().Num();
		const bool bAtEnd = !bWrap && SelectedIndex.Get() >= Count - 1;
		if (bAtEnd && Count > 0)
		{
			Nudge = -1.0f;
			OnSelectionChanged.ExecuteIfBound(0);
		}
		else
		{
			Step(1);
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGOptionSelector::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	bFocusVisible = IsVisibleFocusCause(InFocusEvent.GetCause());
	return FReply::Handled();
}

void SKGOptionSelector::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusLost(InFocusEvent);
	bFocusVisible = false;
}

FCursorReply SKGOptionSelector::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

// ==================================================================================================================
// SKGSettingRow
// ==================================================================================================================

void SKGSettingRow::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const bool bHasValue = InArgs._ValueText.IsSet();
	const bool bHasDescription = !InArgs._Description.IsEmpty();

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(64.0f)
		.Padding(FMargin(22.0f, 9.0f, 16.0f, 9.0f))
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 24.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(InArgs._Label)
					.Font(S.BodyBoldFont)
					.ColorAndOpacity(S.Cream)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility(bHasDescription ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					.Text(InArgs._Description)
					.Font(S.SmallFont)
					.ColorAndOpacity(S.Muted)
					.AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(InArgs._ControlWidth)
				.VAlign(VAlign_Center)
				[
					InArgs._Content.Widget
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(bHasValue ? 16.0f : 0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(bHasValue ? 70.0f : 0.0f)
				.Visibility(bHasValue ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				[
					SNew(STextBlock)
					.Text(InArgs._ValueText)
					.Font(S.BodyBoldFont)
					.ColorAndOpacity(S.Gold)
					.Justification(ETextJustify::Right)
				]
			]
		]
	];
}

void SKGSettingRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Hot = FKGMenuStyle::Approach(Hot, (IsHovered() || HasFocusedDescendants()) ? 1.0f : 0.0f, InDeltaTime, 14.0f);
}

int32 SKGSettingRow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size,
	                             WidgetTint(S.Panel, (0.32f + 0.48f * Hot) * Opacity), 12.0f);
	const float BarHeight = (Size.Y - 22.0f) * Hot;
	if (BarHeight > 1.0f)
	{
		FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId + 1, AllottedGeometry,
		                             FVector2f(8.0f, (Size.Y - BarHeight) * 0.5f), FVector2f(4.0f, BarHeight),
		                             WidgetTint(S.Gold, Hot * Opacity), 2.0f);
	}
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle,
	                                bParentEnabled);
}

// ==================================================================================================================
// SKGModal
// ==================================================================================================================

void SKGModal::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	OnConfirm = InArgs._OnConfirm;
	OnCancel = InArgs._OnCancel;
	InitialFocus = InArgs._InitialFocus;
	const bool bHasCancel = !InArgs._CancelText.IsEmpty();
	const bool bHasExtra = InArgs._ExtraContent.Widget != SNullWidget::NullWidget;

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bHasCancel)
	{
		Buttons->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			SAssignNew(CancelButton, SKGMenuButton)
			.Kind(EKGButtonKind::Ghost)
			.MinWidth(150.0f)
			.Text(InArgs._CancelText)
			.OnClicked_Lambda([this]() { OnCancel.ExecuteIfBound(); })
		];
	}
	Buttons->AddSlot()
	.AutoWidth()
	[
		SAssignNew(ConfirmButton, SKGMenuButton)
		.Kind(InArgs._ConfirmKind)
		.MinWidth(180.0f)
		.Text(InArgs._ConfirmText)
		.OnClicked_Lambda([this]() { OnConfirm.ExecuteIfBound(); })
	];

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&S.WhiteBrush)
			.ColorAndOpacity_Lambda([this]()
			{
				return FSlateColor(FKGMenuStyle::WithAlpha(FKGMenuStyle::Get().Ink, 0.72f * FMath::Clamp(Appear * 3.0f, 0.0f, 1.0f)));
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(580.0f)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			.RenderTransform_Lambda([this]()
			{
				const float Scale = FMath::Lerp(0.9f, 1.0f, FKGMenuStyle::EaseOutBack(Appear));
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2f(Scale)));
			})
			[
				SNew(SBorder)
				.BorderImage(&S.PanelSolidBrush)
				.Padding(FMargin(38.0f, 32.0f, 38.0f, 30.0f))
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Appear * 2.0f, 0.0f, 1.0f)); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						KGMenuUI::MakePanelTitle(InArgs._Title, FText::GetEmpty(), true)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 18.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(InArgs._Body)
						.Font(S.BodyFont)
						.ColorAndOpacity(S.CreamDim)
						.AutoWrapText(true)
						.LineHeightPercentage(1.15f)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, bHasExtra ? 18.0f : 0.0f, 0.0f, 0.0f)
					[
						InArgs._ExtraContent.Widget
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.Padding(0.0f, 30.0f, 0.0f, 0.0f)
					[
						Buttons
					]
				]
			]
		]
	];
}

TSharedPtr<SWidget> SKGModal::GetDefaultFocus() const
{
	return InitialFocus.IsValid() ? InitialFocus : StaticCastSharedPtr<SWidget>(ConfirmButton);
}

void SKGModal::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Appear = FMath::Min(1.0f, Appear + InDeltaTime / 0.28f);
}

FReply SKGModal::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		if (CancelButton.IsValid())
		{
			OnCancel.ExecuteIfBound();
		}
		else
		{
			OnConfirm.ExecuteIfBound();
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGModal::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Swallow clicks so nothing underneath reacts while the dialog is up.
	return FReply::Handled();
}

// ==================================================================================================================
// Helpers
// ==================================================================================================================

// ==================================================================================================================
// SKGGlyph / SKGCheckBox / SKGSortHeader
// ==================================================================================================================

void SKGGlyph::Construct(const FArguments& InArgs)
{
	Glyph = InArgs._Glyph;
	Color = InArgs._Color.IsSet() ? InArgs._Color : TAttribute<FLinearColor>(FKGMenuStyle::Get().Cream);
	Size = InArgs._Size;
	Thickness = InArgs._Thickness;
}

void SKGGlyph::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (Glyph == EKGGlyph::Spinner)
	{
		Spin = FMath::Fmod(Spin + InDeltaTime * 7.0f, 2.0f * UE_PI);
	}
}

int32 SKGGlyph::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                        bool bParentEnabled) const
{
	const FVector2f Local = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float Extent = FMath::Min(Local.X, Local.Y);
	const FVector2f Pos((Local.X - Extent) * 0.5f, (Local.Y - Extent) * 0.5f);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * (ShouldBeEnabled(bParentEnabled) ? 1.0f : 0.45f);
	Paint(OutDrawElements, LayerId, AllottedGeometry, Glyph, Pos, Extent, WidgetTint(Color.Get(), Opacity), Thickness, Spin);
	return LayerId + 1;
}

void SKGGlyph::Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, EKGGlyph Glyph, const FVector2f& Pos,
                     float Extent, const FLinearColor& Color, float Thickness, float Spin)
{
	auto P = [&Pos, Extent](float X, float Y) { return FVector2f(Pos.X + X * Extent, Pos.Y + Y * Extent); };
	auto Lines = [&Out, Layer, &Geometry, &Color, Thickness](TArray<FVector2f> Points)
	{
		FSlateDrawElement::MakeLines(Out, static_cast<uint32>(Layer), Geometry.ToPaintGeometry(), MoveTemp(Points),
		                             ESlateDrawEffect::None, Color, true, Thickness);
	};
	auto Arc = [&P](float CX, float CY, float R, float From, float To, int32 Segments)
	{
		TArray<FVector2f> Points;
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float A = FMath::Lerp(From, To, static_cast<float>(Index) / Segments);
			Points.Add(P(CX + R * FMath::Cos(A), CY + R * FMath::Sin(A)));
		}
		return Points;
	};
	switch (Glyph)
	{
	case EKGGlyph::Lock:
		{
			// Shackle (upper arc + legs) over a rounded body.
			TArray<FVector2f> Shackle = Arc(0.5f, 0.38f, 0.2f, UE_PI, 2.0f * UE_PI, 10);
			Shackle.Insert(P(0.3f, 0.5f), 0);
			Shackle.Add(P(0.7f, 0.5f));
			Lines(MoveTemp(Shackle));
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geometry, P(0.16f, 0.46f), FVector2f(0.68f * Extent, 0.48f * Extent), Color,
			                             0.1f * Extent);
		}
		break;
	case EKGGlyph::Check:
		Lines({P(0.18f, 0.52f), P(0.42f, 0.76f), P(0.84f, 0.26f)});
		break;
	case EKGGlyph::Cross:
		Lines({P(0.24f, 0.24f), P(0.76f, 0.76f)});
		Lines({P(0.76f, 0.24f), P(0.24f, 0.76f)});
		break;
	case EKGGlyph::ChevronUp:
		Lines({P(0.22f, 0.64f), P(0.5f, 0.36f), P(0.78f, 0.64f)});
		break;
	case EKGGlyph::ChevronDown:
		Lines({P(0.22f, 0.36f), P(0.5f, 0.64f), P(0.78f, 0.36f)});
		break;
	case EKGGlyph::Spinner:
		Lines(Arc(0.5f, 0.5f, 0.36f, Spin, Spin + 1.5f * UE_PI, 18));
		break;
	case EKGGlyph::Refresh:
		{
			const float End = 1.65f * UE_PI;
			Lines(Arc(0.5f, 0.5f, 0.34f, 0.0f, End, 18));
			const FVector2f Tip = P(0.5f + 0.34f * FMath::Cos(End), 0.5f + 0.34f * FMath::Sin(End));
			Lines({Tip + FVector2f(-0.2f, -0.12f) * Extent, Tip, Tip + FVector2f(0.02f, 0.22f) * Extent});
		}
		break;
	}
}

void SKGCheckBox::Construct(const FArguments& InArgs)
{
	IsChecked = InArgs._IsChecked;
	OnToggled = InArgs._OnToggled;
	Size = InArgs._Size;
	bReadOnly = InArgs._bReadOnly;
	Fill = IsChecked.Get() ? 1.0f : 0.0f;
}

void SKGCheckBox::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Fill = FKGMenuStyle::Approach(Fill, IsChecked.Get() ? 1.0f : 0.0f, InDeltaTime, 18.0f);
	const bool bHot = !bReadOnly && IsEnabled() && (IsHovered() || (bFocusVisible && HasKeyboardFocus()));
	Hot = FKGMenuStyle::Approach(Hot, bHot ? 1.0f : 0.0f, InDeltaTime, 16.0f);
}

int32 SKGCheckBox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                           FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                           bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A * (ShouldBeEnabled(bParentEnabled) ? 1.0f : 0.45f);
	const FVector2f Local = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float Extent = FMath::Min(Local.X, Local.Y);
	const FVector2f Pos((Local.X - Extent) * 0.5f, (Local.Y - Extent) * 0.5f);
	const FLinearColor Empty = FKGMenuStyle::WithAlpha(S.Cream, 0.08f);
	const FLinearColor Outline = FMath::Lerp(FKGMenuStyle::WithAlpha(S.Cream, 0.35f), S.Gold, Hot);
	FKGMenuStyle::DrawRoundedBox(OutDrawElements, LayerId, AllottedGeometry, Pos, FVector2f(Extent, Extent),
	                             WidgetTint(FMath::Lerp(Empty, S.Good, Fill), Opacity), Extent * 0.24f,
	                             WidgetTint(Outline, Opacity * (1.0f - 0.6f * Fill)), 2.0f);
	if (Fill > 0.05f)
	{
		SKGGlyph::Paint(OutDrawElements, LayerId + 1, AllottedGeometry, EKGGlyph::Check, Pos, Extent,
		                WidgetTint(S.Ink, Opacity * Fill), 3.0f);
	}
	return LayerId + 2;
}

FReply SKGCheckBox::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bReadOnly || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
	{
		return FReply::Unhandled();
	}
	OnToggled.ExecuteIfBound(!IsChecked.Get());
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGCheckBox::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bReadOnly && IsEnabled() && !InKeyEvent.IsRepeat() &&
		FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		OnToggled.ExecuteIfBound(!IsChecked.Get());
		return FReply::Handled();
	}
	return SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGCheckBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	bFocusVisible = IsVisibleFocusCause(InFocusEvent.GetCause());
	return FReply::Handled();
}

void SKGCheckBox::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SLeafWidget::OnFocusLost(InFocusEvent);
	bFocusVisible = false;
}

FCursorReply SKGCheckBox::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return !bReadOnly && IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

void SKGSortHeader::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	SortState = InArgs._SortState;
	OnClicked = InArgs._OnClicked;
	const bool bRight = InArgs._Justify == ETextJustify::Right;
	const bool bCenter = InArgs._Justify == ETextJustify::Center;
	ChildSlot
	.HAlign(bRight ? HAlign_Right : (bCenter ? HAlign_Center : HAlign_Left))
	.VAlign(VAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Text)
			.Font(S.CaptionFont)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
			.ColorAndOpacity_Lambda([this]()
			{
				const FKGMenuStyle& Style = FKGMenuStyle::Get();
				return FSlateColor(SortState.Get() != 0 ? Style.Gold : (IsHovered() ? Style.Cream : Style.Muted));
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(12.0f)
			.HeightOverride(12.0f)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return SortState.Get() > 0 ? 1 : (SortState.Get() < 0 ? 2 : 0); })
				+ SWidgetSwitcher::Slot()[SNew(SSpacer)]
				+ SWidgetSwitcher::Slot()[SNew(SKGGlyph).Glyph(EKGGlyph::ChevronUp).Size(12.0f).Thickness(2.0f).Color(S.Gold)]
				+ SWidgetSwitcher::Slot()[SNew(SKGGlyph).Glyph(EKGGlyph::ChevronDown).Size(12.0f).Thickness(2.0f).Color(S.Gold)]
			]
		]
	];
}

FReply SKGSortHeader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	OnClicked.ExecuteIfBound();
	return FReply::Handled();
}

FCursorReply SKGSortHeader::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::Hand);
}

namespace KGMenuUI
{
	TSharedRef<SWidget> MakeSectionHeader(const FText& Text)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SHorizontalBox)
			.Visibility(EVisibility::HitTestInvisible)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(S.CaptionFont)
				.ColorAndOpacity(S.Lantern)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.HeightOverride(1.0f)
				[
					SNew(SImage)
					.Image(&S.WhiteBrush)
					.ColorAndOpacity(FKGMenuStyle::WithAlpha(S.Cream, 0.10f))
				]
			];
	}

	TSharedRef<SWidget> MakeKeyCap(const FText& Text)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SBox)
			.MinDesiredWidth(40.0f)
			.HeightOverride(36.0f)
			[
				SNew(SBorder)
				.BorderImage(&S.KeyCapBrush)
				.Padding(FMargin(12.0f, 0.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Text)
					.Font(S.KeyFont)
					.ColorAndOpacity(S.Cream)
				]
			];
	}

	TSharedRef<SWidget> MakePanelTitle(const FText& Title, const FText& Subtitle, bool bCompact)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(bCompact ? FKGMenuStyle::Font("Black", 26, 30) : S.HeadingFont)
				.ColorAndOpacity(S.Cream)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(64.0f)
				.HeightOverride(4.0f)
				[
					SNew(SImage)
					.Image(&S.WhiteBrush)
					.ColorAndOpacity(S.Lantern)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Visibility(Subtitle.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				.Text(Subtitle)
				.Font(S.BodyFont)
				.ColorAndOpacity(S.CreamDim)
				.AutoWrapText(true)
			];
	}

	TSharedRef<SWidget> MakeKeyHint(const FText& Key, const FText& Action)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SHorizontalBox)
			.Visibility(EVisibility::HitTestInvisible)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(&S.KeyCapBrush)
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(Key)
					.Font(S.CaptionFont)
					.ColorAndOpacity(S.CreamDim)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Action)
				.Font(S.SmallFont)
				.ColorAndOpacity(S.Muted)
			];
	}
}
