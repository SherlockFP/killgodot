#include "UI/Menu/SKGMenuArt.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Types/PaintArgs.h"
#include "UI/Menu/KGMenuStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGMenu"

namespace
{
	/** Deterministic 0..1 hash (PCG) so the painted scene is identical every frame. */
	float ArtHash01(int32 N)
	{
		uint32 X = static_cast<uint32>(N) * 747796405u + 2891336453u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return static_cast<float>(X) / 4294967295.0f;
	}

	FLinearColor ArtHex(uint32 RGB, float Alpha = 1.0f)
	{
		return FKGMenuStyle::Hex(RGB, Alpha);
	}

	FLinearColor ArtFade(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	/** Sky colour at height Y (matches the three gradient bands painted below). */
	FLinearColor ArtSkyAt(float Y, float Horizon)
	{
		const float T = FMath::Clamp(Y / Horizon, 0.0f, 1.0f);
		if (T < 0.5f)
		{
			return FMath::Lerp(ArtHex(0x0B0816), ArtHex(0x1E1330), T / 0.5f);
		}
		if (T < 0.82f)
		{
			return FMath::Lerp(ArtHex(0x1E1330), ArtHex(0x4A2240), (T - 0.5f) / 0.32f);
		}
		return FMath::Lerp(ArtHex(0x4A2240), ArtHex(0xB9553A), (T - 0.82f) / 0.18f);
	}

	void ArtLines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points,
	           const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(Out, static_cast<uint32>(Layer), Geometry.ToPaintGeometry(), MoveTemp(Points),
		                             ESlateDrawEffect::None, Color, true, Thickness);
	}
}

// ==================================================================================================================
// SKGMenuBackground
// ==================================================================================================================

void SKGMenuBackground::Construct(const FArguments& InArgs)
{
	DimLeft = InArgs._DimLeft;
	StartTime = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : 0.0;
	SetCanTick(false);
	// The scene animates every frame; keep Slate painting even when nothing else changes.
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([](double, float)
	{
		return EActiveTimerReturnType::Continue;
	}));
}

int32 SKGMenuBackground::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                                 FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                 bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float W = Size.X;
	const float H = Size.Y;
	if (W < 2.0f || H < 2.0f)
	{
		return LayerId;
	}
	const float U = H / 1080.0f;
	const float T = static_cast<float>(Args.GetCurrentTime() - StartTime);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const float Horizon = H * 0.64f;
	FKGVertexCanvas C(OutDrawElements, AllottedGeometry, Opacity);
	int32 Layer = LayerId;

	// --- Sky, sun glow -----------------------------------------------------------------------------------------
	C.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, Horizon * 0.5f), ArtHex(0x0B0816), ArtHex(0x1E1330));
	C.RectV(FVector2f(0.0f, Horizon * 0.5f), FVector2f(W, Horizon * 0.32f), ArtHex(0x1E1330), ArtHex(0x4A2240));
	C.RectV(FVector2f(0.0f, Horizon * 0.82f), FVector2f(W, Horizon * 0.18f + 2.0f), ArtHex(0x4A2240), ArtHex(0xB9553A));
	C.Ellipse(FVector2f(W * 0.60f, Horizon), FVector2f(H * 0.95f, H * 0.40f), ArtHex(0xF28C28, 0.34f), ArtHex(0xF28C28, 0.0f), 48);
	C.Ellipse(FVector2f(W * 0.60f, Horizon), FVector2f(H * 0.34f, H * 0.10f), ArtHex(0xFFC870, 0.38f), ArtHex(0xFFC870, 0.0f), 40);

	// Stars, denser near the zenith, each twinkling at its own rate.
	for (int32 Index = 0; Index < 110; ++Index)
	{
		const float Rx = ArtHash01(Index * 3 + 1);
		const float Ry = ArtHash01(Index * 3 + 2);
		const float Rp = ArtHash01(Index * 3 + 3);
		const FVector2f P(Rx * W, Ry * Ry * Horizon * 0.66f);
		const float Twinkle = 0.35f + 0.65f * (0.5f + 0.5f * FMath::Sin(T * (0.7f + 2.4f * Rp) + Rp * 40.0f));
		const float Alpha = Twinkle * (1.0f - Ry * 0.85f) * 0.95f;
		const float R = (0.9f + 1.7f * Rp * Rp) * U;
		C.Ellipse(P, FVector2f(R * 2.6f, R * 2.6f), ArtHex(0xFFF2D6, Alpha), ArtHex(0xFFF2D6, 0.0f), 8);
	}

	// Moon with a crescent bite and a soft halo.
	{
		const FVector2f Moon(W * 0.745f, H * 0.17f);
		C.Ellipse(Moon, FVector2f(36.0f * U, 36.0f * U), ArtHex(0xFFF3DA), ArtHex(0xF5DDB0), 40);
		const FVector2f Bite = Moon + FVector2f(15.0f * U, -9.0f * U);
		const FLinearColor Sky = ArtSkyAt(Bite.Y, Horizon);
		C.Ellipse(Bite, FVector2f(32.0f * U, 32.0f * U), Sky, Sky, 40);
		// Halo last so it glows over the bite too (otherwise the bite reads as a dark disc).
		C.Ellipse(Moon, FVector2f(150.0f * U, 150.0f * U), ArtHex(0xFFE8C0, 0.10f), ArtHex(0xFFE8C0, 0.0f), 40);
	}
	C.Flush(Layer++);

	// --- Clouds (slow drift, wrap around) ------------------------------------------------------------------------
	for (int32 Index = 0; Index < 7; ++Index)
	{
		const float Span = W + 1000.0f * U;
		const float X = FMath::Fmod(ArtHash01(Index + 40) * Span + T * (5.0f + 3.0f * Index) * U, Span) - 500.0f * U;
		const float Y = Horizon * (0.18f + 0.105f * Index);
		const float Rx = (240.0f + 220.0f * ArtHash01(Index + 50)) * U;
		const float Ry = (22.0f + 16.0f * ArtHash01(Index + 60)) * U;
		const FLinearColor Tone = Index < 3 ? ArtHex(0x3B2346, 0.55f) : ArtHex(0x7A3A48, 0.40f);
		C.Ellipse(FVector2f(X, Y), FVector2f(Rx, Ry), Tone, ArtFade(Tone, 0.0f), 32);
		C.Ellipse(FVector2f(X + Rx * 0.35f, Y - Ry * 0.5f), FVector2f(Rx * 0.55f, Ry * 1.1f), ArtFade(Tone, 0.8f), ArtFade(Tone, 0.0f), 28);
		C.Ellipse(FVector2f(X - Rx * 0.4f, Y + Ry * 0.2f), FVector2f(Rx * 0.5f, Ry * 0.8f), ArtFade(Tone, 0.7f), ArtFade(Tone, 0.0f), 28);
	}
	C.Flush(Layer++);

	// --- Lighthouse beams (behind the headlands and the tower) -----------------------------------------------
	const float LighthouseX = W * 0.875f;
	auto CliffY = [&](float X)
	{
		const float Tn = FMath::Clamp((X - W * 0.76f) / (W * 0.24f), 0.0f, 1.0f);
		const float Rise = FMath::SmoothStep(0.0f, 0.45f, Tn);
		return Horizon - (18.0f + 120.0f * Rise + 9.0f * FMath::Sin(X * 0.021f / U) + 5.0f * FMath::Sin(X * 0.057f / U)) * U;
	};
	const float TowerBase = CliffY(LighthouseX);
	const float TowerHeight = 190.0f * U;
	const FVector2f Lamp(LighthouseX, TowerBase - TowerHeight - 12.0f * U);
	const float Spin = T * 0.62f;
	float Flare = 0.0f;
	for (int32 Beam = 0; Beam < 2; ++Beam)
	{
		const float Theta = Spin + PI * static_cast<float>(Beam);
		const float Cos = FMath::Cos(Theta);
		const float Toward = FMath::Max(0.0f, FMath::Sin(Theta));
		Flare = FMath::Max(Flare, FMath::Pow(Toward, 10.0f));
		const float Dir = Cos >= 0.0f ? 1.0f : -1.0f;
		const float Reach = W * 1.05f * FMath::Pow(FMath::Abs(Cos), 0.6f) + 40.0f * U;
		const float Spread = (50.0f + 170.0f * (1.0f - FMath::Abs(Cos))) * U;
		const float Alpha = 0.09f + 0.20f * Toward;
		const FVector2f Far = Lamp + FVector2f(Dir * Reach, -Reach * 0.05f);
		const FLinearColor Warm = ArtHex(0xFFD58A, Alpha);
		C.Tri(Lamp + FVector2f(0.0f, -4.0f * U), Far + FVector2f(0.0f, -Spread), Far + FVector2f(0.0f, Spread), Warm,
		      ArtFade(Warm, 0.0f), ArtFade(Warm, 0.0f));
		const FLinearColor Core = ArtHex(0xFFE7B0, Alpha * 1.2f);
		C.Tri(Lamp, Far + FVector2f(0.0f, -Spread * 0.35f), Far + FVector2f(0.0f, Spread * 0.35f), Core, ArtFade(Core, 0.0f),
		      ArtFade(Core, 0.0f));
	}
	C.Flush(Layer++);

	// --- Distant coast, sea ------------------------------------------------------------------------------------
	{
		TArray<FVector2f> Coast;
		for (int32 Step = 0; Step <= 40; ++Step)
		{
			const float X = W * 0.58f * static_cast<float>(Step) / 40.0f;
			const float Taper = 1.0f - FMath::SmoothStep(0.55f, 1.0f, X / (W * 0.58f));
			const float Lift = (26.0f + 16.0f * FMath::Sin(X * 0.004f / U + 1.0f) + 9.0f * FMath::Sin(X * 0.013f / U + 2.0f)) * U;
			Coast.Add(FVector2f(X, Horizon - Lift * Taper));
		}
		C.Ridge(Coast, Horizon + 1.0f, ArtHex(0x2B1834), ArtHex(0x2B1834));
	}
	C.RectV(FVector2f(0.0f, Horizon), FVector2f(W, H - Horizon), ArtHex(0x3A2140), ArtHex(0x090A13));
	C.RectV(FVector2f(0.0f, Horizon - 1.0f * U), FVector2f(W, 5.0f * U), ArtHex(0xF2A060, 0.35f), ArtHex(0xF2A060, 0.0f));
	C.Flush(Layer++);

	// --- Village headland with houses, church and lit windows ---------------------------------------------------
	auto HillY = [&](float X)
	{
		const float Tn = FMath::Clamp((X - W * 0.30f) / (W * 0.46f), 0.0f, 1.0f);
		const float Bump = FMath::Pow(FMath::Sin(PI * Tn), 0.55f);
		return Horizon + 4.0f * U - (110.0f * Bump + 8.0f * FMath::Sin(X * 0.03f / U) * Bump) * U;
	};
	{
		TArray<FVector2f> Hill;
		for (int32 Step = 0; Step <= 48; ++Step)
		{
			const float X = FMath::Lerp(W * 0.30f, W * 0.76f, static_cast<float>(Step) / 48.0f);
			Hill.Add(FVector2f(X, HillY(X)));
		}
		C.Ridge(Hill, Horizon + 6.0f * U, ArtHex(0x170F20), ArtHex(0x120B19));

		TArray<FVector2f> Cliff;
		for (int32 Step = 0; Step <= 30; ++Step)
		{
			const float X = FMath::Lerp(W * 0.74f, W, static_cast<float>(Step) / 30.0f);
			Cliff.Add(FVector2f(X, CliffY(X)));
		}
		C.Ridge(Cliff, Horizon + 6.0f * U, ArtHex(0x150E1D), ArtHex(0x100A16));
	}

	struct FWindow
	{
		FVector2f Pos;
		float Seed;
	};
	TArray<FWindow> Windows;
	const FLinearColor HouseTone = ArtHex(0x1B1226);
	for (int32 Index = 0; Index < 11; ++Index)
	{
		const float X = FMath::Lerp(W * 0.37f, W * 0.69f, static_cast<float>(Index) / 10.0f) + (ArtHash01(Index + 70) - 0.5f) * 24.0f * U;
		const float Ground = HillY(X) + 6.0f * U;
		const float HW = (16.0f + 12.0f * ArtHash01(Index + 80)) * U;
		const float HH = (26.0f + 20.0f * ArtHash01(Index + 90)) * U;
		const float Roof = (16.0f + 14.0f * ArtHash01(Index + 100)) * U;
		const bool bChurch = Index == 5;
		const float BodyH = bChurch ? HH * 1.6f : HH;
		C.Quad(FVector2f(X - HW, Ground - BodyH), FVector2f(X + HW, Ground - BodyH), FVector2f(X + HW, Ground + 8.0f * U),
		       FVector2f(X - HW, Ground + 8.0f * U), HouseTone, HouseTone, HouseTone, HouseTone);
		C.Tri(FVector2f(X - HW - 5.0f * U, Ground - BodyH), FVector2f(X, Ground - BodyH - Roof),
		      FVector2f(X + HW + 5.0f * U, Ground - BodyH), HouseTone, HouseTone, HouseTone);
		if (bChurch)
		{
			const float SW = 7.0f * U;
			const float Top = Ground - BodyH - Roof * 0.6f;
			C.Quad(FVector2f(X - SW, Top - 36.0f * U), FVector2f(X + SW, Top - 36.0f * U), FVector2f(X + SW, Top),
			       FVector2f(X - SW, Top), HouseTone, HouseTone, HouseTone, HouseTone);
			C.Tri(FVector2f(X - SW - 2.0f * U, Top - 36.0f * U), FVector2f(X, Top - 78.0f * U),
			      FVector2f(X + SW + 2.0f * U, Top - 36.0f * U), HouseTone, HouseTone, HouseTone);
		}
		else if (ArtHash01(Index + 110) > 0.45f)
		{
			const float CX = X + HW * 0.45f;
			C.Quad(FVector2f(CX - 3.0f * U, Ground - BodyH - Roof * 0.9f), FVector2f(CX + 3.0f * U, Ground - BodyH - Roof * 0.9f),
			       FVector2f(CX + 3.0f * U, Ground - BodyH), FVector2f(CX - 3.0f * U, Ground - BodyH), HouseTone, HouseTone,
			       HouseTone, HouseTone);
		}
		const int32 WindowCount = bChurch ? 1 : 1 + (ArtHash01(Index + 120) > 0.5f ? 1 : 0);
		for (int32 Pane = 0; Pane < WindowCount; ++Pane)
		{
			if (ArtHash01(Index * 7 + Pane + 130) < 0.25f)
			{
				continue;
			}
			const float WX = WindowCount == 1 ? X : X + (Pane == 0 ? -HW * 0.45f : HW * 0.45f);
			Windows.Add({FVector2f(WX, Ground - BodyH * (bChurch ? 0.55f : 0.5f)), ArtHash01(Index * 11 + Pane + 140)});
		}
	}
	C.Flush(Layer++);

	for (const FWindow& Window : Windows)
	{
		const float Flicker = 0.78f + 0.22f * FMath::Sin(T * (2.0f + 3.0f * Window.Seed) + Window.Seed * 30.0f);
		C.Ellipse(Window.Pos, FVector2f(14.0f * U, 14.0f * U), ArtHex(0xFFB24A, 0.22f * Flicker), ArtHex(0xFFB24A, 0.0f), 12);
		C.RectV(Window.Pos - FVector2f(3.5f * U, 4.5f * U), FVector2f(7.0f * U, 9.0f * U), ArtHex(0xFFD27A, Flicker),
		        ArtHex(0xF29A3A, Flicker));
	}

	// --- Lighthouse tower -------------------------------------------------------------------------------------
	{
		const FLinearColor Tower = ArtHex(0x21172C);
		const float BaseHalf = 24.0f * U;
		const float TopHalf = 15.0f * U;
		const float TopY = TowerBase - TowerHeight;
		C.Quad(FVector2f(LighthouseX - TopHalf, TopY), FVector2f(LighthouseX + TopHalf, TopY),
		       FVector2f(LighthouseX + BaseHalf, TowerBase + 6.0f * U), FVector2f(LighthouseX - BaseHalf, TowerBase + 6.0f * U),
		       Tower, Tower, Tower, Tower);
		for (int32 Band = 0; Band < 3; ++Band)
		{
			const float Y0 = TopY + TowerHeight * (0.18f + 0.28f * Band);
			const float Y1 = Y0 + TowerHeight * 0.1f;
			const float Half0 = FMath::Lerp(TopHalf, BaseHalf, (Y0 - TopY) / TowerHeight);
			const float Half1 = FMath::Lerp(TopHalf, BaseHalf, (Y1 - TopY) / TowerHeight);
			const FLinearColor Stripe = ArtHex(0xC8102E, 0.28f);
			C.Quad(FVector2f(LighthouseX - Half0, Y0), FVector2f(LighthouseX + Half0, Y0), FVector2f(LighthouseX + Half1, Y1),
			       FVector2f(LighthouseX - Half1, Y1), Stripe, Stripe, Stripe, Stripe);
		}
		C.RectV(FVector2f(LighthouseX - 23.0f * U, TopY - 5.0f * U), FVector2f(46.0f * U, 6.0f * U), Tower, Tower);
		C.RectV(FVector2f(LighthouseX - 13.0f * U, TopY - 27.0f * U), FVector2f(26.0f * U, 22.0f * U), ArtHex(0xFFE3A0),
		        ArtHex(0xFFB84D));
		C.Tri(FVector2f(LighthouseX - 18.0f * U, TopY - 27.0f * U), FVector2f(LighthouseX, TopY - 47.0f * U),
		      FVector2f(LighthouseX + 18.0f * U, TopY - 27.0f * U), Tower, Tower, Tower);
		C.RectV(FVector2f(LighthouseX - 1.5f * U, TopY - 56.0f * U), FVector2f(3.0f * U, 10.0f * U), Tower, Tower);
	}
	C.Flush(Layer++);

	// Lamp glow and the flare when a beam faces us.
	C.Ellipse(Lamp, FVector2f(95.0f * U, 95.0f * U), ArtHex(0xFFD58A, 0.50f), ArtHex(0xFFD58A, 0.0f), 32);
	C.Ellipse(Lamp, FVector2f(270.0f * U, 270.0f * U), ArtHex(0xFFB84D, 0.10f + 0.25f * Flare), ArtHex(0xFFB84D, 0.0f), 40);
	if (Flare > 0.01f)
	{
		C.Ellipse(Lamp, FVector2f(420.0f * U, 36.0f * U), ArtHex(0xFFF0C8, 0.45f * Flare), ArtHex(0xFFF0C8, 0.0f), 32);
	}

	// --- Reflections on the water --------------------------------------------------------------------------------
	for (int32 Index = 0; Index < 34; ++Index)
	{
		const float Y = Horizon + 6.0f * U + FMath::Pow(static_cast<float>(Index), 1.35f) * 6.0f * U;
		if (Y > H)
		{
			break;
		}
		const float Fall = 1.0f - static_cast<float>(Index) / 34.0f;
		const float Half = (50.0f + 90.0f * ArtHash01(Index + 200)) * U * (1.0f + Index * 0.03f) *
			(0.7f + 0.3f * FMath::Sin(T * 1.3f + Index));
		const float X = W * 0.60f + FMath::Sin(T * 0.8f + Index * 1.7f) * 12.0f * U;
		const float Thick = 2.4f * U * (1.0f + Index * 0.04f);
		const FLinearColor Glint = ArtHex(0xF7A45A, 0.26f * Fall);
		C.RectH(FVector2f(X - Half, Y), FVector2f(Half, Thick), ArtFade(Glint, 0.0f), Glint);
		C.RectH(FVector2f(X, Y), FVector2f(Half, Thick), Glint, ArtFade(Glint, 0.0f));

		const float LX = LighthouseX + FMath::Sin(T * 1.1f + Index * 2.3f) * 6.0f * U;
		const float LHalf = (10.0f + 26.0f * ArtHash01(Index + 300)) * U * (1.0f + Index * 0.02f);
		const FLinearColor Lamplight = ArtHex(0xFFD58A, (0.14f + 0.3f * Flare) * Fall);
		C.RectH(FVector2f(LX - LHalf, Y), FVector2f(LHalf, Thick), ArtFade(Lamplight, 0.0f), Lamplight);
		C.RectH(FVector2f(LX, Y), FVector2f(LHalf, Thick), Lamplight, ArtFade(Lamplight, 0.0f));
	}
	C.Flush(Layer++);

	// --- Waves ---------------------------------------------------------------------------------------------------
	for (int32 Row = 0; Row < 9; ++Row)
	{
		const float BaseY = Horizon + FMath::Pow(static_cast<float>(Row + 1), 1.6f) * 8.0f * U;
		if (BaseY > H)
		{
			break;
		}
		const float Amplitude = 1.4f * U * (1.0f + Row * 0.4f);
		const float Frequency = 0.012f / U / (1.0f + Row * 0.25f);
		TArray<FVector2f> Points;
		Points.Reserve(49);
		for (int32 Step = 0; Step <= 48; ++Step)
		{
			const float X = W * static_cast<float>(Step) / 48.0f;
			const float Y = BaseY + Amplitude * FMath::Sin(X * Frequency + T * (0.7f + 0.12f * Row) + Row * 1.9f) +
				Amplitude * 0.5f * FMath::Sin(X * Frequency * 2.3f - T * 0.9f + Row);
			Points.Add(FVector2f(X, Y));
		}
		ArtLines(OutDrawElements, Layer, AllottedGeometry, MoveTemp(Points),
		      ArtFade(S.Cream, (0.04f + 0.012f * Row) * Opacity), 1.1f + 0.3f * Row);
	}
	++Layer;

	// --- Fog banks drifting over the water line -------------------------------------------------------------------
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float Span = W + 1100.0f * U;
		const float X = FMath::Fmod(ArtHash01(Index + 400) * Span + T * (9.0f + 4.0f * Index) * U, Span) - 550.0f * U;
		const float Y = Horizon - 26.0f * U + Index * 16.0f * U;
		const FLinearColor Mist = ArtHex(0xC4A6C0, 0.075f);
		C.Ellipse(FVector2f(X, Y), FVector2f(560.0f * U, 40.0f * U), Mist, ArtFade(Mist, 0.0f), 32);
	}

	// --- Embers rising --------------------------------------------------------------------------------------------
	for (int32 Index = 0; Index < 30; ++Index)
	{
		const float Speed = (12.0f + 24.0f * ArtHash01(Index + 500)) * U;
		const float Travel = H * 0.8f;
		const float Period = Travel / Speed;
		const float Phase = FMath::Fmod(T + ArtHash01(Index + 600) * Period, Period) / Period;
		const float X = ArtHash01(Index + 700) * W + FMath::Sin(T * 0.7f + Index) * 28.0f * U;
		const float Y = H - Phase * Travel;
		const float Alpha = FMath::Sin(Phase * PI) * (0.30f + 0.5f * ArtHash01(Index + 800));
		const float R = (1.4f + 1.8f * ArtHash01(Index + 900)) * U;
		const FLinearColor Ember = FMath::Lerp(S.Gold, S.Lantern, ArtHash01(Index + 1000));
		C.Ellipse(FVector2f(X, Y), FVector2f(R * 5.0f, R * 5.0f), ArtFade(Ember, 0.25f * Alpha), ArtFade(Ember, 0.0f), 12);
		C.Ellipse(FVector2f(X, Y), FVector2f(R, R), ArtFade(Ember, Alpha), ArtFade(Ember, Alpha * 0.6f), 8);
	}
	C.Flush(Layer++);

	// --- Vignette + menu column shade -----------------------------------------------------------------------------
	const float Dim = FMath::Clamp(DimLeft.Get(), 0.0f, 1.0f);
	if (Dim > 0.0f)
	{
		C.RectH(FVector2f(0.0f, 0.0f), FVector2f(W * 0.26f, H), ArtFade(S.Ink, Dim), ArtFade(S.Ink, Dim * 0.8f));
		C.RectH(FVector2f(W * 0.26f, 0.0f), FVector2f(W * 0.34f, H), ArtFade(S.Ink, Dim * 0.8f), ArtFade(S.Ink, 0.0f));
	}
	C.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, H * 0.2f), ArtFade(S.Ink, 0.55f), ArtFade(S.Ink, 0.0f));
	C.RectV(FVector2f(0.0f, H * 0.78f), FVector2f(W, H * 0.22f + 1.0f), ArtFade(S.Ink, 0.0f), ArtFade(S.Ink, 0.8f));
	C.RectH(FVector2f(W * 0.9f, 0.0f), FVector2f(W * 0.1f + 1.0f, H), ArtFade(S.Ink, 0.0f), ArtFade(S.Ink, 0.35f));
	C.Flush(Layer);

	return Layer;
}

// ==================================================================================================================
// SKGClockGlyph
// ==================================================================================================================

void SKGClockGlyph::Construct(const FArguments& InArgs)
{
	Font = InArgs._Font;
	ShadowOffset = InArgs._ShadowOffset;
	ShadowColor = InArgs._ShadowColor;
	StartTime = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : 0.0;
	SetCanTick(false);
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([](double, float)
	{
		return EActiveTimerReturnType::Continue;
	}));
}

float SKGClockGlyph::GetEm() const
{
	// Slate font sizes are points at 96 DPI.
	return Font.Size * 96.0f / 72.0f;
}

FVector2D SKGClockGlyph::ComputeDesiredSize(float) const
{
	const float Em = GetEm();
	float LineHeight = Em * 1.17f;
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		LineHeight = static_cast<float>(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(Font));
	}
	return FVector2D(Em * 0.80f, LineHeight);
}

int32 SKGClockGlyph::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	const float Em = GetEm();

	float Baseline = Size.Y * 0.79f;
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		Baseline = static_cast<float>(Measure->GetMaxCharacterHeight(Font)) + static_cast<float>(Measure->GetBaseline(Font));
	}
	const float Radius = Em * 0.37f;
	const float Ring = Em * 0.155f;
	const FVector2f Centre(Size.X * 0.5f, Baseline - Em * 0.36f);
	const float T = static_cast<float>(Args.GetCurrentTime() - StartTime);
	auto ClockDir = [](float Degrees)
	{
		const float Rad = FMath::DegreesToRadians(Degrees);
		return FVector2f(FMath::Sin(Rad), -FMath::Cos(Rad));
	};

	FKGVertexCanvas C(OutDrawElements, AllottedGeometry, Opacity);
	const FVector2f Shadow = UE::Slate::CastToVector2f(ShadowOffset);
	C.Ellipse(Centre + Shadow, FVector2f(Radius, Radius), ShadowColor, ShadowColor, 64);
	C.Ellipse(Centre, FVector2f(Radius, Radius), S.Cream, S.Cream, 64);
	C.Ellipse(Centre, FVector2f(Radius - Ring, Radius - Ring), ArtHex(0x2E1E38), ArtHex(0x1A1120), 64);
	C.Flush(LayerId);

	const float Face = Radius - Ring;
	for (int32 Mark = 0; Mark < 12; ++Mark)
	{
		const bool bMajor = Mark % 3 == 0;
		const FVector2f Dir = ClockDir(Mark * 30.0f);
		TArray<FVector2f> Points = {Centre + Dir * (Face - Em * (bMajor ? 0.075f : 0.045f)), Centre + Dir * (Face - Em * 0.018f)};
		ArtLines(OutDrawElements, LayerId + 1, AllottedGeometry, MoveTemp(Points), ArtFade(S.Cream, 0.85f * Opacity),
		      FMath::Max(1.0f, Em * (bMajor ? 0.026f : 0.016f)));
	}

	// Minute hand ticks once a second with a small spring overshoot; the hour hand creeps.
	const float TickPeriod = 1.0f;
	const float Steps = FMath::FloorToFloat(T / TickPeriod);
	const float Frac = FMath::Fmod(T, TickPeriod) / TickPeriod;
	const float Minute = 150.0f + 6.0f * (Steps + FKGMenuStyle::EaseOutBack(FMath::Min(Frac * 5.0f, 1.0f)));
	const float Hour = 300.0f + Minute / 12.0f;
	{
		TArray<FVector2f> HourHand = {Centre - ClockDir(Hour) * Em * 0.03f, Centre + ClockDir(Hour) * Face * 0.52f};
		ArtLines(OutDrawElements, LayerId + 2, AllottedGeometry, MoveTemp(HourHand), ArtFade(S.Cream, Opacity),
		      FMath::Max(1.5f, Em * 0.05f));
		TArray<FVector2f> MinuteHand = {Centre - ClockDir(Minute) * Em * 0.04f, Centre + ClockDir(Minute) * Face * 0.84f};
		ArtLines(OutDrawElements, LayerId + 2, AllottedGeometry, MoveTemp(MinuteHand), ArtFade(S.Lantern, Opacity),
		      FMath::Max(1.2f, Em * 0.034f));
	}

	// Crack through the ring (top-right), with a short branch.
	{
		const FLinearColor Crack = ArtFade(S.Ink, Opacity);
		TArray<FVector2f> Main = {
			Centre + ClockDir(38.0f) * Radius * 1.02f,
			Centre + ClockDir(46.0f) * Radius * 0.80f,
			Centre + ClockDir(33.0f) * Radius * 0.63f,
			Centre + ClockDir(44.0f) * Radius * 0.44f,
		};
		ArtLines(OutDrawElements, LayerId + 3, AllottedGeometry, MoveTemp(Main), Crack, FMath::Max(1.2f, Em * 0.028f));
		TArray<FVector2f> Branch = {Centre + ClockDir(46.0f) * Radius * 0.80f, Centre + ClockDir(62.0f) * Radius * 0.93f};
		ArtLines(OutDrawElements, LayerId + 3, AllottedGeometry, MoveTemp(Branch), Crack, FMath::Max(1.0f, Em * 0.018f));
	}

	C.Ellipse(Centre, FVector2f(Em * 0.045f, Em * 0.045f), S.Gold, S.Gold, 20);
	C.Flush(LayerId + 4);
	return LayerId + 4;
}

// ==================================================================================================================
// SKGLogo
// ==================================================================================================================

void SKGLogo::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const float Size = InArgs._Size;
	const FSlateFontInfo Font = FKGMenuStyle::Font("Black", Size, 10);
	const FVector2D Shadow(Size * 0.03f, Size * 0.055f);
	const FLinearColor ShadowColor = ArtHex(0x7A2A12, 0.95f);

	auto Word = [&](const TCHAR* Text) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::AsCultureInvariant(Text))
			.Font(Font)
			.ColorAndOpacity(S.Cream)
			.ShadowOffset(Shadow)
			.ShadowColorAndOpacity(ShadowColor);
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		.Visibility(EVisibility::HitTestInvisible)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				Word(TEXT("KILLG"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SKGClockGlyph)
				.Font(Font)
				.ShadowOffset(Shadow)
				.ShadowColor(ShadowColor)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Size * 0.04f, -Size * 0.16f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility(InArgs._ShowTagline ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.Text(LOCTEXT("Tagline", "Everyone's waiting. Someone's impatient."))
			.Font(FKGMenuStyle::Font("Bold", FMath::Max(12.0f, Size * 0.15f), 170))
			.ColorAndOpacity(S.Lantern)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		]
	];
}

#undef LOCTEXT_NAMESPACE
