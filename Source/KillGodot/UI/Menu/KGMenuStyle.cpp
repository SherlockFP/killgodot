#include "UI/Menu/KGMenuStyle.h"

#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

const FKGMenuStyle& FKGMenuStyle::Get()
{
	static const FKGMenuStyle Instance;
	return Instance;
}

FLinearColor FKGMenuStyle::Hex(uint32 RGB, float Alpha)
{
	FLinearColor Color = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
	Color.A = Alpha;
	return Color;
}

FLinearColor FKGMenuStyle::WithAlpha(const FLinearColor& Color, float Alpha)
{
	return FLinearColor(Color.R, Color.G, Color.B, Alpha);
}

FSlateFontInfo FKGMenuStyle::Font(FName Typeface, float Size, int32 LetterSpacing)
{
	FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Info.LetterSpacing = LetterSpacing;
	return Info;
}

float FKGMenuStyle::Approach(float Current, float Target, float DeltaTime, float Speed)
{
	const float Blend = 1.0f - FMath::Exp(-Speed * FMath::Max(DeltaTime, 0.0f));
	const float Next = FMath::Lerp(Current, Target, Blend);
	return FMath::IsNearlyEqual(Next, Target, 0.001f) ? Target : Next;
}

float FKGMenuStyle::EaseOutBack(float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	constexpr float C1 = 1.4f;
	constexpr float C3 = C1 + 1.0f;
	return 1.0f + C3 * FMath::Pow(X - 1.0f, 3.0f) + C1 * FMath::Pow(X - 1.0f, 2.0f);
}

void FKGMenuStyle::DrawRoundedBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
                                  const FVector2f& Position, const FVector2f& Size, const FLinearColor& Fill, float Radius,
                                  const FLinearColor& Outline, float OutlineWidth)
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}
	const bool bOutline = OutlineWidth > 0.0f && Outline.A > 0.001f;
	FLinearColor FillColor = Fill;
	if (FillColor.A < 0.002f)
	{
		if (!bOutline)
		{
			return;
		}
		// Slate culls fully transparent boxes, which would also drop the outline: keep an invisible fill.
		FillColor = WithAlpha(Outline, 0.002f);
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Brush.OutlineSettings.RoundingType =
		Radius < 0.0f ? ESlateBrushRoundingType::HalfHeightRadius : ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Color = FSlateColor(bOutline ? Outline : FLinearColor::Transparent);
	Brush.OutlineSettings.Width = bOutline ? OutlineWidth : 0.0f;

	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &Brush,
	                           ESlateDrawEffect::None, FillColor);
}

FKGMenuStyle::FKGMenuStyle()
{
	Ink = Hex(0x120D17);
	Night = Hex(0x1D1524);
	Panel = Hex(0x2A1F31);
	PanelHi = Hex(0x3A2B42);
	Cream = Hex(0xF6E7C8);
	CreamDim = Hex(0xCDBB98);
	Muted = Hex(0x8E7D6E);
	Gold = Hex(0xF2C230);
	Lantern = Hex(0xF28C28);
	Crimson = Hex(0xC8102E);
	Ghost = Hex(0x4FD1C5);
	Good = Hex(0x8BD160);

	TitleFont = Font("Black", 108, 20);
	HeadingFont = Font("Black", 34, 40);
	SubheadingFont = Font("Bold", 20, 20);
	MenuButtonFont = Font("Black", 24, 70);
	ButtonFont = Font("Bold", 17, 60);
	BodyFont = Font("Regular", 17);
	BodyBoldFont = Font("Bold", 17);
	SmallFont = Font("Medium", 14);
	CaptionFont = Font("Bold", 12, 220);
	KeyFont = Font("Black", 15, 20);

	NoBrush = FSlateNoResource();
	PanelBrush = FSlateRoundedBoxBrush(WithAlpha(Night, 0.93f), 20.0f, WithAlpha(Cream, 0.08f), 1.0f);
	PanelSolidBrush = FSlateRoundedBoxBrush(Night, 20.0f, WithAlpha(Cream, 0.12f), 1.0f);
	BadgeBrush = FSlateRoundedBoxBrush(WithAlpha(Gold, 0.14f), 6.0f, WithAlpha(Gold, 0.75f), 1.0f);
	KeyCapBrush = FSlateRoundedBoxBrush(Panel, 7.0f, WithAlpha(Cream, 0.28f), 1.5f);
	ToastBrush = FSlateRoundedBoxBrush(WithAlpha(Night, 0.97f), 14.0f, WithAlpha(Lantern, 0.85f), 1.5f);
	InsetBrush = FSlateRoundedBoxBrush(WithAlpha(Ink, 0.75f), 12.0f, WithAlpha(Cream, 0.06f), 1.0f);

	BareButton = FButtonStyle()
		.SetNormal(NoBrush)
		.SetHovered(NoBrush)
		.SetPressed(NoBrush)
		.SetDisabled(NoBrush)
		.SetNormalPadding(FMargin(0.0f))
		.SetPressedPadding(FMargin(0.0f))
		.SetNormalForeground(FSlateColor(Cream))
		.SetHoveredForeground(FSlateColor(Cream))
		.SetPressedForeground(FSlateColor(Cream))
		.SetDisabledForeground(FSlateColor(Muted));

	// Scroll bar: thin brass thumb on a faint track.
	ScrollBar = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	const FSlateRoundedBoxBrush Track(WithAlpha(Cream, 0.06f), 3.0f);
	const FSlateRoundedBoxBrush Thumb(WithAlpha(Cream, 0.28f), 3.0f);
	const FSlateRoundedBoxBrush ThumbHot(WithAlpha(Gold, 0.85f), 3.0f);
	ScrollBar.SetVerticalBackgroundImage(Track)
		.SetHorizontalBackgroundImage(Track)
		.SetVerticalTopSlotImage(NoBrush)
		.SetVerticalBottomSlotImage(NoBrush)
		.SetHorizontalTopSlotImage(NoBrush)
		.SetHorizontalBottomSlotImage(NoBrush)
		.SetNormalThumbImage(Thumb)
		.SetHoveredThumbImage(ThumbHot)
		.SetDraggedThumbImage(ThumbHot)
		.SetThickness(6.0f);

	ScrollBox = FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox");
	ScrollBox.SetTopShadowBrush(NoBrush)
		.SetBottomShadowBrush(NoBrush)
		.SetLeftShadowBrush(NoBrush)
		.SetRightShadowBrush(NoBrush);

	TextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	TextBox.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Ink, 10.0f, WithAlpha(Cream, 0.18f), 1.5f))
		.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Ink, 10.0f, WithAlpha(Cream, 0.40f), 1.5f))
		.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Ink, 10.0f, Gold, 2.0f))
		.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Night, 10.0f, WithAlpha(Cream, 0.10f), 1.0f))
		.SetPadding(FMargin(18.0f, 14.0f))
		.SetFont(Font("Bold", 22, 40))
		.SetForegroundColor(FSlateColor(Cream))
		.SetFocusedForegroundColor(FSlateColor(Cream))
		.SetReadOnlyForegroundColor(FSlateColor(Muted))
		.SetBackgroundColor(FSlateColor(FLinearColor::White))
		.SetScrollBarStyle(ScrollBar);
}

// --- FKGVertexCanvas ------------------------------------------------------------------------------------------------

FKGVertexCanvas::FKGVertexCanvas(FSlateWindowElementList& InOut, const FGeometry& Geometry, float InOpacity)
	: Out(InOut)
	, Transform(Geometry.GetAccumulatedRenderTransform())
	, Opacity(InOpacity)
{
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(FKGMenuStyle::Get().WhiteBrush);
	}
}

SlateIndex FKGVertexCanvas::AddVertex(const FVector2f& Position, const FLinearColor& Color)
{
	FLinearColor Final = Color;
	Final.A = FMath::Clamp(Final.A * Opacity, 0.0f, 1.0f);
	const SlateIndex Index = static_cast<SlateIndex>(Vertices.Num());
	Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Position, FVector2f(0.5f, 0.5f),
	                                                                 Final.ToFColorSRGB()));
	return Index;
}

void FKGVertexCanvas::Tri(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FLinearColor& CA,
                          const FLinearColor& CB, const FLinearColor& CC)
{
	const SlateIndex IA = AddVertex(A, CA);
	const SlateIndex IB = AddVertex(B, CB);
	const SlateIndex IC = AddVertex(C, CC);
	Indices.Append({IA, IB, IC});
}

void FKGVertexCanvas::Quad(const FVector2f& TL, const FVector2f& TR, const FVector2f& BR, const FVector2f& BL,
                           const FLinearColor& CTL, const FLinearColor& CTR, const FLinearColor& CBR,
                           const FLinearColor& CBL)
{
	const SlateIndex I0 = AddVertex(TL, CTL);
	const SlateIndex I1 = AddVertex(TR, CTR);
	const SlateIndex I2 = AddVertex(BR, CBR);
	const SlateIndex I3 = AddVertex(BL, CBL);
	Indices.Append({I0, I1, I2, I0, I2, I3});
}

void FKGVertexCanvas::RectV(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Top,
                            const FLinearColor& Bottom)
{
	Quad(Pos, Pos + FVector2f(Size.X, 0.0f), Pos + Size, Pos + FVector2f(0.0f, Size.Y), Top, Top, Bottom, Bottom);
}

void FKGVertexCanvas::RectH(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Left,
                            const FLinearColor& Right)
{
	Quad(Pos, Pos + FVector2f(Size.X, 0.0f), Pos + Size, Pos + FVector2f(0.0f, Size.Y), Left, Right, Right, Left);
}

void FKGVertexCanvas::Ellipse(const FVector2f& Centre, const FVector2f& Radii, const FLinearColor& Inner,
                              const FLinearColor& Outer, int32 Segments)
{
	const int32 Count = FMath::Max(Segments, 6);
	const SlateIndex CentreIndex = AddVertex(Centre, Inner);
	const SlateIndex First = static_cast<SlateIndex>(Vertices.Num());
	for (int32 Step = 0; Step < Count; ++Step)
	{
		const float Angle = 2.0f * PI * static_cast<float>(Step) / static_cast<float>(Count);
		AddVertex(Centre + FVector2f(FMath::Cos(Angle) * Radii.X, FMath::Sin(Angle) * Radii.Y), Outer);
	}
	for (int32 Step = 0; Step < Count; ++Step)
	{
		const SlateIndex A = First + static_cast<SlateIndex>(Step);
		const SlateIndex B = First + static_cast<SlateIndex>((Step + 1) % Count);
		Indices.Append({CentreIndex, A, B});
	}
}

void FKGVertexCanvas::Ridge(const TArray<FVector2f>& Top, float BottomY, const FLinearColor& TopColor,
                            const FLinearColor& BottomColor)
{
	for (int32 Index = 0; Index + 1 < Top.Num(); ++Index)
	{
		const FVector2f& A = Top[Index];
		const FVector2f& B = Top[Index + 1];
		Quad(A, B, FVector2f(B.X, BottomY), FVector2f(A.X, BottomY), TopColor, TopColor, BottomColor, BottomColor);
	}
}

void FKGVertexCanvas::Convex(const TArray<FVector2f>& Points, const FLinearColor& Color)
{
	if (Points.Num() < 3)
	{
		return;
	}
	const SlateIndex First = AddVertex(Points[0], Color);
	SlateIndex Previous = AddVertex(Points[1], Color);
	for (int32 Index = 2; Index < Points.Num(); ++Index)
	{
		const SlateIndex Current = AddVertex(Points[Index], Color);
		Indices.Append({First, Previous, Current});
		Previous = Current;
	}
}

void FKGVertexCanvas::Flush(int32 Layer)
{
	if (Indices.Num() > 0 && Vertices.Num() > 0)
	{
		FSlateDrawElement::MakeCustomVerts(Out, static_cast<uint32>(Layer), Handle, Vertices, Indices, nullptr, 0, 0);
	}
	Vertices.Reset();
	Indices.Reset();
}
