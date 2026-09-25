#include "Chores/UI/KGMinigame.h"
#include "UI/KGUITokens.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/Menu/KGMenuStyle.h"

// ---- palette / math ------------------------------------------------------------------------------------------------

namespace KGMg
{
	FLinearColor C(uint32 RGB, float Alpha)
	{
		return FKGMenuStyle::Hex(RGB, Alpha);
	}

	FLinearColor A(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	FLinearColor Mix(const FLinearColor& X, const FLinearColor& Y, float T)
	{
		return X + (Y - X) * T;
	}

	float Saturate(float X)
	{
		return FMath::Clamp(X, 0.0f, 1.0f);
	}

	float EaseOutBack(float T)
	{
		return FKGMenuStyle::EaseOutBack(T);
	}

	float EaseOutCubic(float T)
	{
		const float X = 1.0f - Saturate(T);
		return 1.0f - X * X * X;
	}

	float Ping(float Time, float Period)
	{
		const float P = FMath::Fmod(Time, Period) / Period;
		return P < 0.5f ? P * 2.0f : 2.0f - P * 2.0f;
	}

	float Approach(float Current, float Target, float Dt, float Speed)
	{
		return FMath::Lerp(Current, Target, 1.0f - FMath::Exp(-Speed * FMath::Max(Dt, 0.0f)));
	}

	bool InRect(const FVector2f& P, const FVector2f& Pos, const FVector2f& Size)
	{
		return P.X >= Pos.X && P.Y >= Pos.Y && P.X <= Pos.X + Size.X && P.Y <= Pos.Y + Size.Y;
	}

	float DistToSegment(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float Len2 = AB.SizeSquared();
		const float T = Len2 > 1e-4f ? FMath::Clamp(FVector2f::DotProduct(P - A, AB) / Len2, 0.0f, 1.0f) : 0.0f;
		return FVector2f::Distance(P, A + AB * T);
	}

	// SPRINT-025: colour roles from UI/KGUITokens.h (shared with the HUD and the front end).
	const FLinearColor Ink = C(KGUI::Ink);
	const FLinearColor Night = C(KGUI::Night);
	const FLinearColor Panel = C(KGUI::Panel);
	const FLinearColor PanelHi = C(KGUI::PanelHi);
	const FLinearColor Cream = C(KGUI::Cream);
	const FLinearColor CreamDim = C(KGUI::CreamDim);
	const FLinearColor Muted = C(KGUI::Muted);
	const FLinearColor Gold = C(KGUI::Gold);
	const FLinearColor Lantern = C(KGUI::Lantern);
	const FLinearColor Crimson = C(KGUI::Crimson);
	const FLinearColor Ghost = C(KGUI::Ghost);
	const FLinearColor Good = C(KGUI::Good);
	const FLinearColor Wood = C(0x8A5A3B);
	const FLinearColor WoodDark = C(0x5C3A26);
	const FLinearColor WoodLight = C(0xB98256);
	const FLinearColor Stone = C(0x8C8A94);
	const FLinearColor StoneDark = C(0x55535E);
	const FLinearColor Iron = C(0x4A4E57);
	const FLinearColor Water = C(0x3FA7D6);
	const FLinearColor WaterDeep = C(0x1D5C8C);
	const FLinearColor Grass = C(0x6DB33F);
	const FLinearColor Soil = C(0x6B4428);
	const FLinearColor Sky = C(0x8FD3F4);
	const FLinearColor SkyNight = C(0x1B2447);
	const FLinearColor Fire = C(0xFF7A1A);
	const FLinearColor Paper = C(0xF3E6C4);
}

// ---- painter -------------------------------------------------------------------------------------------------------

FKGMgPainter::FKGMgPainter(FSlateWindowElementList& InOut, const FGeometry& InLogical, int32 InLayer, float InOpacity)
	: Out(InOut)
	, G(InLogical)
	, Layer(InLayer)
	, Opacity(InOpacity)
{
}

FLinearColor FKGMgPainter::Fade(const FLinearColor& Color) const
{
	return FLinearColor(Color.R, Color.G, Color.B, Color.A * Opacity);
}

void FKGMgPainter::Rect(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Color)
{
	if (Size.X <= 0.0f || Size.Y <= 0.0f || Color.A <= 0.001f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), &FKGMenuStyle::Get().WhiteBrush,
	                           ESlateDrawEffect::None, Fade(Color));
}

void FKGMgPainter::RectV(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Top, const FLinearColor& Bottom)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.RectV(Pos, Size, Top, Bottom);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::RectH(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Left, const FLinearColor& Right)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.RectH(Pos, Size, Left, Right);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::RoundRect(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Fill, float Radius,
                             const FLinearColor& Outline, float OutlineWidth)
{
	FKGMenuStyle::DrawRoundedBox(Out, Layer++, G, Pos, Size, Fade(Fill), Radius, Fade(Outline), OutlineWidth);
}

void FKGMgPainter::Circle(const FVector2f& Centre, float Radius, const FLinearColor& Fill, const FLinearColor& Outline,
                          float OutlineWidth)
{
	RoundRect(Centre - FVector2f(Radius, Radius), FVector2f(Radius * 2.0f, Radius * 2.0f), Fill, -1.0f, Outline, OutlineWidth);
}

void FKGMgPainter::Disc(const FVector2f& Centre, const FVector2f& Radii, const FLinearColor& Inner, const FLinearColor& Outer,
                        int32 Segments)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.Ellipse(Centre, Radii, Inner, Outer, Segments);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::Glow(const FVector2f& Centre, float Radius, const FLinearColor& Color)
{
	Disc(Centre, FVector2f(Radius, Radius), Color, KGMg::A(Color, 0.0f), 36);
}

void FKGMgPainter::Line(const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness)
{
	if (Points.Num() < 2)
	{
		return;
	}
	FSlateDrawElement::MakeLines(Out, Layer++, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Fade(Color), true, Thickness);
}

void FKGMgPainter::Segment(const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness)
{
	Line({A, B}, Color, Thickness);
}

void FKGMgPainter::Bar(const FVector2f& A, const FVector2f& B, float Width, const FLinearColor& Color)
{
	const FVector2f D = B - A;
	const float Len = D.Size();
	if (Len < 1e-3f)
	{
		return;
	}
	const FVector2f N = FVector2f(-D.Y, D.X) / Len * (Width * 0.5f);
	Quad(A + N, B + N, B - N, A - N, Color);
}

void FKGMgPainter::RotRect(const FVector2f& Centre, const FVector2f& Size, float AngleRad, const FLinearColor& Color)
{
	const FVector2f AX(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
	const FVector2f AY(-AX.Y, AX.X);
	const FVector2f HX = AX * (Size.X * 0.5f);
	const FVector2f HY = AY * (Size.Y * 0.5f);
	Quad(Centre - HX - HY, Centre + HX - HY, Centre + HX + HY, Centre - HX + HY, Color);
}

void FKGMgPainter::Tri(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FLinearColor& Color)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.Tri(A, B, C, Color, Color, Color);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::Quad(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& D, const FLinearColor& Color)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.Quad(A, B, C, D, Color, Color, Color, Color);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::Poly(const TArray<FVector2f>& Points, const FLinearColor& Color)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	Canvas.Convex(Points, Color);
	Canvas.Flush(Layer++);
}

void FKGMgPainter::Arc(const FVector2f& Centre, float Radius, float A0, float A1, const FLinearColor& Color, float Thickness,
                       int32 Segments)
{
	TArray<FVector2f> Points;
	const int32 N = FMath::Max(2, Segments);
	Points.Reserve(N + 1);
	for (int32 i = 0; i <= N; ++i)
	{
		const float Ang = FMath::Lerp(A0, A1, float(i) / N);
		Points.Add(Centre + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Radius);
	}
	Line(Points, Color, Thickness);
}

void FKGMgPainter::ArcBand(const FVector2f& Centre, float R0, float R1, float A0, float A1, const FLinearColor& Color, int32 Segments)
{
	FKGVertexCanvas Canvas(Out, G, Opacity);
	const int32 N = FMath::Max(1, Segments);
	for (int32 i = 0; i < N; ++i)
	{
		const float T0 = FMath::Lerp(A0, A1, float(i) / N);
		const float T1 = FMath::Lerp(A0, A1, float(i + 1) / N);
		const FVector2f D0(FMath::Cos(T0), FMath::Sin(T0));
		const FVector2f D1(FMath::Cos(T1), FMath::Sin(T1));
		Canvas.Quad(Centre + D0 * R0, Centre + D0 * R1, Centre + D1 * R1, Centre + D1 * R0, Color, Color, Color, Color);
	}
	Canvas.Flush(Layer++);
}

FVector2f FKGMgPainter::MeasureText(const FString& Text, float Size, FName Typeface, int32 Spacing) const
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return FVector2f(Text.Len() * Size * 0.55f, Size * 1.2f);
	}
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, FMath::RoundToInt(Size));
	Font.LetterSpacing = Spacing;
	return UE::Slate::CastToVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
}

void FKGMgPainter::Text(const FVector2f& Pos, const FString& Text, float Size, const FLinearColor& Color, float Align,
                        FName Typeface, int32 Spacing)
{
	if (Text.IsEmpty())
	{
		return;
	}
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, FMath::RoundToInt(Size));
	Font.LetterSpacing = Spacing;
	const FVector2f Measured = MeasureText(Text, Size, Typeface, Spacing);
	const FVector2f At(Pos.X - Measured.X * Align, Pos.Y);
	FSlateDrawElement::MakeText(Out, Layer++, G.ToPaintGeometry(Measured + FVector2f(4.0f, 2.0f), FSlateLayoutTransform(At)), Text,
	                            Font, ESlateDrawEffect::None, Fade(Color));
}

void FKGMgPainter::Shadow(const FVector2f& Pos, const FVector2f& Size, float Radius, float Alpha, float Offset)
{
	for (int32 i = 3; i >= 1; --i)
	{
		const float Grow = 3.0f * i;
		RoundRect(Pos + FVector2f(-Grow, -Grow + Offset), Size + FVector2f(Grow * 2.0f, Grow * 2.0f),
		          FLinearColor(0.0f, 0.0f, 0.0f, Alpha / (i + 1.5f)), Radius + Grow);
	}
}

void FKGMgPainter::Gauge(const FVector2f& Pos, const FVector2f& Size, float Fill, const FLinearColor& FillColor, float Z0, float Z1)
{
	const float R = Size.Y * 0.5f;
	RoundRect(Pos, Size, KGMg::A(KGMg::Ink, 0.85f), R, KGMg::A(KGMg::Cream, 0.25f), 1.5f);
	if (Z0 >= 0.0f && Z1 > Z0)
	{
		RoundRect(Pos + FVector2f(Size.X * Z0, 2.0f), FVector2f(Size.X * (Z1 - Z0), Size.Y - 4.0f), KGMg::A(KGMg::Good, 0.35f), 3.0f,
		          KGMg::A(KGMg::Good, 0.9f), 1.5f);
	}
	const float F = KGMg::Saturate(Fill);
	if (F > 0.002f)
	{
		RoundRect(Pos + FVector2f(3.0f, 3.0f), FVector2f(FMath::Max(Size.Y - 6.0f, (Size.X - 6.0f) * F), Size.Y - 6.0f), FillColor, R - 3.0f);
	}
}

void FKGMgPainter::GaugeV(const FVector2f& Pos, const FVector2f& Size, float Fill, const FLinearColor& FillColor, float Z0, float Z1)
{
	const float R = Size.X * 0.5f;
	RoundRect(Pos, Size, KGMg::A(KGMg::Ink, 0.85f), R, KGMg::A(KGMg::Cream, 0.25f), 1.5f);
	if (Z0 >= 0.0f && Z1 > Z0)
	{
		const float Y0 = Pos.Y + Size.Y * (1.0f - Z1);
		RoundRect(FVector2f(Pos.X + 2.0f, Y0), FVector2f(Size.X - 4.0f, Size.Y * (Z1 - Z0)), KGMg::A(KGMg::Good, 0.35f), 3.0f,
		          KGMg::A(KGMg::Good, 0.9f), 1.5f);
	}
	const float F = KGMg::Saturate(Fill);
	if (F > 0.002f)
	{
		const float Hgt = FMath::Max(Size.X - 6.0f, (Size.Y - 6.0f) * F);
		RoundRect(FVector2f(Pos.X + 3.0f, Pos.Y + Size.Y - 3.0f - Hgt), FVector2f(Size.X - 6.0f, Hgt), FillColor, R - 3.0f);
	}
}

void FKGMgPainter::Tag(const FVector2f& Centre, const FString& InText, const FLinearColor& Fill, const FLinearColor& TextColor, float Size)
{
	const FVector2f M = MeasureText(InText, Size);
	const FVector2f Box(M.X + Size * 1.2f, M.Y + Size * 0.35f);
	RoundRect(Centre - Box * 0.5f, Box, Fill, -1.0f);
	Text(FVector2f(Centre.X, Centre.Y - M.Y * 0.5f), InText, Size, TextColor, 0.5f);
}

void FKGMgPainter::HintArrow(const FVector2f& From, const FVector2f& To, float Time, const FLinearColor& Color)
{
	const FVector2f D = To - From;
	const float Len = D.Size();
	if (Len < 4.0f)
	{
		return;
	}
	const FVector2f Dir = D / Len;
	const FVector2f N(-Dir.Y, Dir.X);
	// Dashes flowing toward the target + a chevron at the tip.
	const float Dash = 14.0f;
	const float Offset = FMath::Fmod(Time * 60.0f, Dash * 2.0f);
	for (float S = Offset - Dash * 2.0f; S < Len - 18.0f; S += Dash * 2.0f)
	{
		const float S0 = FMath::Max(0.0f, S);
		const float S1 = FMath::Min(Len - 18.0f, S + Dash);
		if (S1 > S0)
		{
			Bar(From + Dir * S0, From + Dir * S1, 4.0f, KGMg::A(Color, 0.75f));
		}
	}
	const FVector2f Tip = To;
	Tri(Tip, Tip - Dir * 18.0f + N * 10.0f, Tip - Dir * 18.0f - N * 10.0f, Color);
}

void FKGMgPainter::HintRing(const FVector2f& Centre, float Radius, float Time, const FLinearColor& Color)
{
	const float P = FMath::Fmod(Time, 1.1f) / 1.1f;
	Arc(Centre, Radius * (0.8f + 0.5f * P), 0.0f, UE_TWO_PI, KGMg::A(Color, (1.0f - P) * 0.9f), 3.0f, 36);
	Arc(Centre, Radius * 0.8f, 0.0f, UE_TWO_PI, KGMg::A(Color, 0.5f), 2.0f, 36);
}

// ---- minigame base ---------------------------------------------------------------------------------------------------

void FKGMinigame::Start(uint32 Seed, int32 FirstStage)
{
	BaseSeed = Seed == 0 ? 1u : Seed;
	Stage = FMath::Clamp(FirstStage, 0, FMath::Max(0, NumStages() - 1));
	Rng.Reseed(uint64(BaseSeed) * 2654435761ull + uint64(Stage) * 7919ull);
	StageTime = 0.0f;
	bSolved = false;
	BeginStage();
}

void FKGMinigame::HostTick(float Dt, bool bAuto)
{
	Time += Dt;
	if (!bSolved)
	{
		StageTime += Dt;
		if (bAuto)
		{
			AutoPlay(Dt);
		}
	}
	Tick(Dt);
}

void FKGMinigame::HostPress(const FVector2f& Pos)
{
	Mouse = Pos;
	bMouseDown = true;
	if (!bSolved)
	{
		OnPress(Pos);
	}
}

void FKGMinigame::HostRelease(const FVector2f& Pos)
{
	Mouse = Pos;
	bMouseDown = false;
	if (!bSolved)
	{
		OnRelease(Pos);
	}
}

void FKGMinigame::HostMove(const FVector2f& Pos, const FVector2f& Delta)
{
	Mouse = Pos;
	if (!bSolved)
	{
		OnMove(Pos, Delta);
	}
}

void FKGMinigame::HostKey(const FKey& Key)
{
	if (!bSolved)
	{
		OnKey(Key);
	}
}

void FKGMinigame::AdvanceStage()
{
	if (Stage + 1 >= NumStages())
	{
		return;
	}
	++Stage;
	Rng.Reseed(uint64(BaseSeed) * 2654435761ull + uint64(Stage) * 7919ull);
	StageTime = 0.0f;
	bSolved = false;
	BeginStage();
}

void FKGMinigame::RetryStage()
{
	Rng.Reseed(uint64(BaseSeed) * 2654435761ull + uint64(Stage) * 7919ull + uint64(Time * 1000.0f));
	StageTime = 0.0f;
	bSolved = false;
	BeginStage();
}

void FKGMinigame::RestartAt(int32 InStage)
{
	Stage = FMath::Clamp(InStage, 0, FMath::Max(0, NumStages() - 1));
	RetryStage();
}

TArray<FKGMgFeedback> FKGMinigame::DrainFeedback()
{
	TArray<FKGMgFeedback> Result = MoveTemp(Feedback);
	Feedback.Reset();
	return Result;
}

void FKGMinigame::Solve()
{
	bSolved = true;
}

void FKGMinigame::Nice(const FVector2f& At, const FString& Pop, FName InSound, float Pitch)
{
	FKGMgFeedback& F = Feedback.AddDefaulted_GetRef();
	F.Sound = InSound;
	F.Pitch = Pitch;
	F.Pop = Pop;
	F.At = At;
	F.Color = KGMg::Good;
}

void FKGMinigame::Oops(const FVector2f& At, const FString& Pop, FName InSound, float Shake)
{
	FKGMgFeedback& F = Feedback.AddDefaulted_GetRef();
	F.Sound = InSound;
	F.Pop = Pop;
	F.At = At;
	F.Color = KGMg::C(0xFF5A4E);
	F.Shake = Shake;
}

void FKGMinigame::Sound(FName Name, float Volume, float Pitch)
{
	FKGMgFeedback& F = Feedback.AddDefaulted_GetRef();
	F.Sound = Name;
	F.Volume = Volume;
	F.Pitch = Pitch;
}

void FKGMinigame::PopText(const FVector2f& At, const FString& Text, const FLinearColor& Color)
{
	FKGMgFeedback& F = Feedback.AddDefaulted_GetRef();
	F.Pop = Text;
	F.At = At;
	F.Color = Color;
}

// ---- factory -----------------------------------------------------------------------------------------------------------

TUniquePtr<FKGMinigame> FKGMinigameFactory::Create(FName ChoreId)
{
#define KG_MINIGAME_CREATE(Id)                  \
	if (ChoreId == FName(TEXT(#Id)))            \
	{                                           \
		return KGMakeMinigame_##Id();           \
	}
	KG_MINIGAME_LIST(KG_MINIGAME_CREATE)
#undef KG_MINIGAME_CREATE
	return nullptr;
}

bool FKGMinigameFactory::Has(FName ChoreId)
{
	return GetIds().Contains(ChoreId);
}

TArray<FName> FKGMinigameFactory::GetIds()
{
	TArray<FName> Ids;
#define KG_MINIGAME_ID(Id) Ids.Add(FName(TEXT(#Id)));
	KG_MINIGAME_LIST(KG_MINIGAME_ID)
#undef KG_MINIGAME_ID
	return Ids;
}
