#include "UI/Reveal/SKGRoleReveal.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Roles/KGRoleDefinition.h"
#include "Roles/KGRoleListGenerator.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Reveal/KGRoleCardText.h"

namespace KGRevealPaint
{
	using FV = FVector2f;

	float Sat(float X) { return FMath::Clamp(X, 0.0f, 1.0f); }
	float EaseOut(float T) { const float U = 1.0f - Sat(T); return 1.0f - U * U * U; }
	float EaseInOut(float T) { const float X = Sat(T); return X * X * (3.0f - 2.0f * X); }
	float Ramp(float Time, float Start, float Length) { return Sat((Time - Start) / FMath::Max(Length, 0.001f)); }
	FLinearColor A(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, C.A * Alpha); }
	FLinearColor Hex(uint32 RGB, float Alpha = 1.0f) { return FKGMenuStyle::Hex(RGB, Alpha); }

	// Palette (Docs/08_UI_UX.md section 1 + the HUD's alignment hues).
	const FLinearColor CardInk = Hex(0x2A1F31);
	const FLinearColor Parchment = Hex(0xF6E7C8);
	const FLinearColor ParchmentDeep = Hex(0xE9D3A6);
	const FLinearColor BackRim = Hex(0xD9AE4E);
	const FLinearColor BackFill = Hex(0x5B1A2E);
	const FLinearColor BackDeep = Hex(0x43121F);

	/** Rotation + (flip) scale around a centre: local card units -> widget space. */
	struct FXf
	{
		FV C;
		float Cos = 1.0f;
		float Sin = 0.0f;
		float SX = 1.0f;
		float SY = 1.0f;
		/** Card turning about its vertical axis: the near edge grows by Persp (fraction), measured over PerspRef. */
		float Persp = 0.0f;
		float PerspRef = 1.0f;

		FXf(const FV& Centre, float AngleDeg = 0.0f, float ScaleX = 1.0f, float ScaleY = 1.0f)
			: C(Centre), Cos(FMath::Cos(FMath::DegreesToRadians(AngleDeg))), Sin(FMath::Sin(FMath::DegreesToRadians(AngleDeg))),
			  SX(ScaleX), SY(ScaleY)
		{
		}

		FV Map(const FV& L) const
		{
			const FV S(L.X * SX, L.Y * SY * (1.0f + Persp * L.X / PerspRef));
			return C + FV(S.X * Cos - S.Y * Sin, S.X * Sin + S.Y * Cos);
		}
	};

	void RoundRect(const FXf& X, float HW, float HH, float R, TArray<FV>& Out, float Inset = 0.0f)
	{
		HW -= Inset;
		HH -= Inset;
		R = FMath::Clamp(R - Inset, 0.0f, FMath::Min(HW, HH));
		Out.Reset();
		const FV Corners[4] = {FV(HW - R, -HH + R), FV(HW - R, HH - R), FV(-HW + R, HH - R), FV(-HW + R, -HH + R)};
		const float StartDeg[4] = {-90.0f, 0.0f, 90.0f, 180.0f};
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			for (int32 Step = 0; Step <= 5; ++Step)
			{
				const float Rad = FMath::DegreesToRadians(StartDeg[Corner] + 90.0f * Step / 5.0f);
				Out.Add(X.Map(Corners[Corner] + FV(FMath::Cos(Rad), FMath::Sin(Rad)) * R));
			}
		}
	}

	void FillRound(FKGVertexCanvas& C, const FXf& X, float HW, float HH, float R, const FLinearColor& Col, float Inset = 0.0f)
	{
		TArray<FV> P;
		RoundRect(X, HW, HH, R, P, Inset);
		C.Convex(P, Col);
	}

	void FillLocal(FKGVertexCanvas& C, const FXf& X, std::initializer_list<FV> Points, const FLinearColor& Col)
	{
		TArray<FV> P;
		for (const FV& L : Points)
		{
			P.Add(X.Map(L));
		}
		C.Convex(P, Col);
	}

	/** Emblem of an alignment in local units (Size = height): house, blade, diamond. */
	void Emblem(FKGVertexCanvas& C, const FXf& X, EKGAlignment Align, float Size, const FLinearColor& Col, const FLinearColor& Cut)
	{
		const float S = Size;
		switch (Align)
		{
		case EKGAlignment::Impatient:
			FillLocal(C, X, {FV(0.0f, -0.55f * S), FV(0.15f * S, 0.18f * S), FV(-0.15f * S, 0.18f * S)}, Col);
			FillLocal(C, X, {FV(-0.32f * S, 0.18f * S), FV(0.32f * S, 0.18f * S), FV(0.32f * S, 0.28f * S), FV(-0.32f * S, 0.28f * S)}, Col);
			FillLocal(C, X, {FV(-0.07f * S, 0.28f * S), FV(0.07f * S, 0.28f * S), FV(0.07f * S, 0.52f * S), FV(-0.07f * S, 0.52f * S)}, Col);
			FillLocal(C, X, {FV(0.0f, -0.4f * S), FV(0.035f * S, 0.12f * S), FV(-0.035f * S, 0.12f * S)}, Cut);
			break;
		case EKGAlignment::Neutral:
			FillLocal(C, X, {FV(0.0f, -0.52f * S), FV(0.36f * S, 0.0f), FV(0.0f, 0.52f * S), FV(-0.36f * S, 0.0f)}, Col);
			FillLocal(C, X, {FV(0.0f, -0.24f * S), FV(0.16f * S, 0.0f), FV(0.0f, 0.24f * S), FV(-0.16f * S, 0.0f)}, Cut);
			break;
		default:
			FillLocal(C, X, {FV(-0.46f * S, 0.46f * S), FV(-0.46f * S, -0.06f * S), FV(0.0f, -0.5f * S), FV(0.46f * S, -0.06f * S),
			                 FV(0.46f * S, 0.46f * S)}, Col);
			FillLocal(C, X, {FV(-0.12f * S, 0.46f * S), FV(-0.12f * S, 0.12f * S), FV(0.12f * S, 0.12f * S), FV(0.12f * S, 0.46f * S)}, Cut);
			break;
		}
	}

	/** The back of a Morrowmere card: brass rim, crimson field, a cracked clock ("KILL GODOT"). */
	void CardBack(FKGVertexCanvas& C, const FXf& X, float HW, float HH, float U, float Alpha)
	{
		const float R = HW * 0.14f;
		FXf Shadow = X;
		Shadow.C += FV(0.0f, 5.0f * U);
		FillRound(C, Shadow, HW + 2.0f * U, HH + 2.0f * U, R, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Alpha));
		FillRound(C, X, HW, HH, R, A(BackRim, Alpha));
		FillRound(C, X, HW, HH, R, A(BackFill, Alpha), HW * 0.06f);
		FillRound(C, X, HW, HH, R, A(BackRim, 0.55f * Alpha), HW * 0.13f);
		FillRound(C, X, HW, HH, R, A(BackDeep, Alpha), HW * 0.16f);
		// Clock face.
		const FV Centre = X.Map(FV(0.0f, 0.0f));
		const float Ring = HW * 0.46f;
		C.Ellipse(Centre, FV(Ring * FMath::Abs(X.SX), Ring * X.SY), A(BackRim, Alpha), A(BackRim, Alpha), 28);
		C.Ellipse(Centre, FV(Ring * 0.82f * FMath::Abs(X.SX), Ring * 0.82f * X.SY), A(BackFill, Alpha), A(BackDeep, Alpha), 28);
		const float W = HW * 0.05f;
		FillLocal(C, X, {FV(-W, 0.0f), FV(W, 0.0f), FV(W * 0.4f, -Ring * 0.68f), FV(-W * 0.4f, -Ring * 0.68f)}, A(BackRim, Alpha));
		FillLocal(C, X, {FV(0.0f, -W), FV(0.0f, W), FV(Ring * 0.5f, Ring * 0.22f + W * 0.3f), FV(Ring * 0.5f, Ring * 0.22f - W * 0.3f)},
		          A(BackRim, Alpha));
		// The crack across the face.
		FillLocal(C, X, {FV(-Ring * 0.2f, -Ring * 0.8f), FV(-Ring * 0.12f, -Ring * 0.8f), FV(Ring * 0.08f, -Ring * 0.2f),
		                 FV(0.0f, -Ring * 0.2f)}, A(BackDeep, Alpha));
		// Corner pips.
		for (const float Y : {-HH * 0.74f, HH * 0.74f})
		{
			FillLocal(C, X, {FV(0.0f, Y - HW * 0.09f), FV(HW * 0.07f, Y), FV(0.0f, Y + HW * 0.09f), FV(-HW * 0.07f, Y)}, A(BackRim, Alpha));
		}
	}

	/** The face of your card without text (text is drawn on top once the card is square to the screen). */
	void CardFace(FKGVertexCanvas& C, const FXf& X, float HW, float HH, float U, EKGAlignment Align, float Alpha)
	{
		const FLinearColor AC = KGRoleCard::AlignmentColor(Align);
		const float R = HW * 0.08f;
		FXf Shadow = X;
		Shadow.C += FV(0.0f, 8.0f * U);
		FillRound(C, Shadow, HW + 4.0f * U, HH + 4.0f * U, R, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f * Alpha));
		FillRound(C, X, HW, HH, R, A(AC, Alpha));
		FillRound(C, X, HW, HH, R, A(Parchment, Alpha), 5.0f * U);
		// Warm bottom half (paper shading).
		const float Split = HH * 0.1f;
		FillLocal(C, X, {FV(-HW + 5.0f * U, Split), FV(HW - 5.0f * U, Split), FV(HW - 5.0f * U, HH - R), FV(-HW + 5.0f * U, HH - R)},
		          A(ParchmentDeep, 0.55f * Alpha));
		// Alignment band across the top, with a medallion hanging from it.
		const float Band = HH * 0.36f;
		const float Top = -HH + 5.0f * U;
		TArray<FV> BandPoly;
		const float BR = FMath::Max(R - 5.0f * U, 0.0f);
		for (int32 Step = 0; Step <= 5; ++Step)
		{
			const float Rad = FMath::DegreesToRadians(180.0f + 90.0f * Step / 5.0f);
			BandPoly.Add(X.Map(FV(-HW + 5.0f * U + BR, Top + BR) + FV(FMath::Cos(Rad), FMath::Sin(Rad)) * BR));
		}
		for (int32 Step = 0; Step <= 5; ++Step)
		{
			const float Rad = FMath::DegreesToRadians(270.0f + 90.0f * Step / 5.0f);
			BandPoly.Add(X.Map(FV(HW - 5.0f * U - BR, Top + BR) + FV(FMath::Cos(Rad), FMath::Sin(Rad)) * BR));
		}
		BandPoly.Add(X.Map(FV(HW - 5.0f * U, Top + Band)));
		BandPoly.Add(X.Map(FV(-HW + 5.0f * U, Top + Band)));
		C.Convex(BandPoly, A(AC, Alpha));
		FillLocal(C, X, {FV(-HW + 5.0f * U, Top + Band - 6.0f * U), FV(HW - 5.0f * U, Top + Band - 6.0f * U),
		                 FV(HW - 5.0f * U, Top + Band), FV(-HW + 5.0f * U, Top + Band)}, A(FLinearColor::Black, 0.12f * Alpha));
		const float Med = HW * 0.24f;
		const FV MedCentre = X.Map(FV(0.0f, Top + Band * 0.52f));
		C.Ellipse(MedCentre, FV(Med * 1.12f * FMath::Abs(X.SX), Med * 1.12f * X.SY), A(Parchment, Alpha), A(Parchment, Alpha), 32);
		C.Ellipse(MedCentre, FV(Med * FMath::Abs(X.SX), Med * X.SY), A(CardInk, Alpha), A(Hex(0x1A1320), Alpha), 32);
		FXf EmblemX = X;
		EmblemX.C = MedCentre;
		Emblem(C, EmblemX, Align, Med * 1.15f, A(AC, Alpha), A(CardInk, Alpha));
		// Large faded emblem in the open middle of the card (the "card art" until illustrated role art exists).
		FXf MarkX = X;
		MarkX.C = X.Map(FV(0.0f, HH * 0.36f));
		Emblem(C, MarkX, Align, HW * 0.62f, A(AC, 0.16f * Alpha), A(Parchment, 0.0f));
		// Corner pips in the alignment colour.
		for (const FV& Pip : {FV(-HW * 0.78f, HH * 0.86f), FV(HW * 0.78f, HH * 0.86f)})
		{
			const float P = HW * 0.045f;
			FillLocal(C, X, {Pip + FV(0.0f, -P), Pip + FV(P * 0.8f, 0.0f), Pip + FV(0.0f, P), Pip + FV(-P * 0.8f, 0.0f)}, A(AC, 0.8f * Alpha));
		}
	}

	// --- Text -----------------------------------------------------------------------------------------------------

	FVector2f Measure(const FString& S, const FSlateFontInfo& Font)
	{
		if (S.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return FVector2f::ZeroVector;
		}
		return FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Font));
	}

	/** Draws S with its box anchored at Pos (Align 0 = left/top, 0.5 = centre, 1 = right/bottom). Returns the size. */
	FVector2f Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& S, const FSlateFontInfo& Font,
	               const FV& Pos, const FLinearColor& Col, float AlignX = 0.0f, float AlignY = 0.0f)
	{
		const FVector2f Size = Measure(S, Font);
		if (Size.X <= 0.0f || Col.A <= 0.004f)
		{
			return Size;
		}
		const FV P(FMath::RoundToFloat(Pos.X - Size.X * AlignX), FMath::RoundToFloat(Pos.Y - Size.Y * AlignY));
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(P)), S, Font,
		                            ESlateDrawEffect::None, Col);
		return Size;
	}

	TArray<FString> Wrap(const FString& S, const FSlateFontInfo& Font, float MaxWidth)
	{
		TArray<FString> Words;
		S.ParseIntoArray(Words, TEXT(" "), true);
		TArray<FString> Lines;
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && Measure(Try, Font).X > MaxWidth)
			{
				Lines.Add(Line);
				Line = Word;
			}
			else
			{
				Line = Try;
			}
		}
		if (!Line.IsEmpty())
		{
			Lines.Add(Line);
		}
		return Lines;
	}

	/** Largest font of Typeface (<= Size) that fits S into MaxWidth on one line. */
	FSlateFontInfo FitFont(const FString& S, FName Typeface, float Size, float MaxWidth, int32 Spacing = 0)
	{
		FSlateFontInfo Font = FKGMenuStyle::Font(Typeface, Size, Spacing);
		for (int32 Try = 0; Try < 12 && Measure(S, Font).X > MaxWidth; ++Try)
		{
			Font.Size *= 0.92f;
		}
		return Font;
	}
}


void SKGRoleReveal::Construct(const FArguments& InArgs)
{
	Provider = InArgs._ViewProvider;
	View = InArgs._StaticView;
	FixedTime = InArgs._FixedTime;
	Time = FixedTime >= 0.0f ? FixedTime : 0.0f;
	SetCanTick(true);
}

void SKGRoleReveal::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (Provider)
	{
		View = Provider();
	}
	ReadyAge = View.bReady ? ReadyAge + InDeltaTime : 0.0f;
	if (FixedTime >= 0.0f)
	{
		Time = FixedTime;
		ReadyAge = View.bReady ? 1.0f : 0.0f;
		return;
	}
	if (View.bEnding)
	{
		OutroTime += InDeltaTime;
	}
	if (View.ServerElapsed < 0.0f)
	{
		// Still the lobby's last frames (the phase has not replicated yet): the table fades in, the deck waits.
		Time = FMath::Min(Time + InDeltaTime, KGReveal::TableEnd * 0.95f);
	}
	else
	{
		Time += InDeltaTime;
		// Follow the server clock: catch up at once, only step back on a real jump (host migration, clock cut).
		if (View.ServerElapsed > Time + 0.3f || View.ServerElapsed < Time - 1.0f)
		{
			Time = View.ServerElapsed;
		}
	}
	if (View.RoleId.IsNone())
	{
		Time = FMath::Min(Time, KGReveal::DealEnd - 0.02f);   // your card waits face down for the owner-only role
	}
}

int32 SKGRoleReveal::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	using namespace KGRevealPaint;   // function scope only (unity builds must not see these names)
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const FV Size = FV(Geo.GetLocalSize());
	const float W = Size.X;
	const float H = Size.Y;
	const float U = FMath::Min(W / 1500.0f, H / 1080.0f);
	const float Alpha = Sat(1.0f - OutroTime / KGReveal::OutroSeconds) * InWidgetStyle.GetColorAndOpacityTint().A;
	if (Alpha <= 0.002f)
	{
		return LayerId;
	}
	const float T = Time;
	const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), View.RoleId);
	const EKGAlignment Align = Role ? Role->GetAlignment() : EKGAlignment::Town;
	const FLinearColor AC = KGRoleCard::AlignmentColor(Align);
	const FKGRoleCardText Card = KGRoleCard::Get(View.RoleId);
	const bool bTr = KGRoleCard::IsTurkish();
	const int32 Seats = FMath::Clamp(View.Players, 1, 20);
	const float TableIn = EaseOut(Ramp(T, 0.0f, KGReveal::TableEnd));
	const float FaceIn = EaseOut(Ramp(T, KGReveal::FlipEnd, KGReveal::FaceEnd - KGReveal::FlipEnd));
	const double Real = FPlatformTime::Seconds();

	int32 Layer = LayerId;
	FKGVertexCanvas C(Out, Geo, Alpha);

	// --- Room: dusk-plum dark, lantern light pooling over the table, dust in the air ------------------------------
	C.RectV(FV(0.0f, 0.0f), Size, Hex(0x1F1627), Hex(0x09070D));
	C.Ellipse(FV(W * 0.5f, H * 0.5f), FV(W * 0.62f, H * 0.62f), A(S.Lantern, 0.2f * TableIn), A(S.Lantern, 0.0f), 48);
	C.Ellipse(FV(W * 0.5f, H * 0.02f), FV(W * 0.18f, H * 0.12f), A(S.Gold, 0.22f * TableIn), A(S.Gold, 0.0f), 32);
	C.RectH(FV(0.0f, 0.0f), FV(W * 0.22f, H), FLinearColor(0, 0, 0, 0.55f), FLinearColor(0, 0, 0, 0.0f));
	C.RectH(FV(W * 0.78f, 0.0f), FV(W * 0.22f, H), FLinearColor(0, 0, 0, 0.0f), FLinearColor(0, 0, 0, 0.55f));
	for (int32 Mote = 0; Mote < 34; ++Mote)
	{
		// Deterministic motes (hash of the index) drifting up through the light.
		const float Seed = FMath::Frac(FMath::Sin(Mote * 12.9898f) * 43758.5453f);
		const float Seed2 = FMath::Frac(FMath::Sin(Mote * 78.233f) * 12345.678f);
		const float Y = FMath::Frac(Seed2 - static_cast<float>(FMath::Fmod(Real * 0.018 * (0.5 + Seed), 1.0)));
		const FV P(W * (0.2f + 0.6f * Seed), H * (0.1f + 0.8f * Y));
		const float R = (1.2f + 2.2f * Seed2) * U;
		C.Ellipse(P, FV(R, R), A(S.Gold, 0.35f * TableIn * (0.4f + 0.6f * Seed)), A(S.Gold, 0.0f), 8);
	}
	C.Flush(Layer);
	++Layer;

	// --- The dealer's table ------------------------------------------------------------------------------------------
	const FV TableC(W * 0.5f, H * 0.6f);
	const float RX = FMath::Min(620.0f * U, W * 0.44f);
	const float RY = 250.0f * U;
	const float TableA = TableIn;
	C.Ellipse(TableC + FV(0.0f, 40.0f * U), FV(RX + 90.0f * U, RY + 70.0f * U), FLinearColor(0, 0, 0, 0.6f * TableA),
	          FLinearColor(0, 0, 0, 0.0f), 64);
	C.Ellipse(TableC + FV(0.0f, 14.0f * U), FV(RX + 30.0f * U, RY + 26.0f * U), A(Hex(0x2B170D), TableA), A(Hex(0x2B170D), TableA), 72);
	C.Ellipse(TableC, FV(RX + 30.0f * U, RY + 24.0f * U), A(Hex(0x7A4A2A), TableA), A(Hex(0x4A2A16), TableA), 72);
	C.Ellipse(TableC, FV(RX, RY), A(Hex(0x2E7A5C), TableA), A(Hex(0x123A2C), TableA), 72);
	C.Ellipse(TableC + FV(0.0f, -RY * 0.1f), FV(RX * 0.7f, RY * 0.62f), A(S.Lantern, 0.16f * TableA), A(S.Lantern, 0.0f), 48);
	for (int32 Dot = 0; Dot < 56; ++Dot)
	{
		// Brass inlay ring.
		const float Ang = 2.0f * PI * Dot / 56.0f;
		const FV P = TableC + FV(FMath::Cos(Ang) * RX * 0.86f, FMath::Sin(Ang) * RY * 0.86f);
		C.Ellipse(P, FV(2.2f * U, 1.6f * U), A(S.Gold, 0.45f * TableA), A(S.Gold, 0.3f * TableA), 6);
	}
	C.Flush(Layer);
	++Layer;

	// --- Cards: deck, shuffle, deal to the seats --------------------------------------------------------------------
	const float CHW = 44.0f * U;   // table card half extents
	const float CHH = 62.0f * U;
	auto SeatPos = [&](int32 Index, float& OutAngle)
	{
		const float Ang = FMath::DegreesToRadians(90.0f + 360.0f * Index / Seats);
		OutAngle = FMath::RadiansToDegrees(Ang) - 90.0f;
		return TableC + FV(FMath::Cos(Ang) * RX * 0.66f, FMath::Sin(Ang) * RY * 0.62f);
	};
	const float DealStart = KGReveal::ShuffleEnd;
	const float DealSpan = (KGReveal::DealEnd - KGReveal::ShuffleEnd) * 0.62f;
	const float Flight = 0.34f;
	auto DepartAt = [&](int32 Index) { return DealStart + DealSpan * Index / FMath::Max(Seats, 1); };
	const float CardsA = TableA * (1.0f - 0.55f * FaceIn);

	if (T < KGReveal::TableEnd || T >= KGReveal::ShuffleEnd)
	{
		// Deck in the middle: what has not been dealt yet.
		int32 Left = Seats;
		for (int32 Index = 0; Index < Seats; ++Index)
		{
			Left -= T >= DepartAt(Index) ? 1 : 0;
		}
		const int32 Layers = FMath::Min(Left, 6);
		for (int32 Index = 0; Index < Layers; ++Index)
		{
			CardBack(C, FXf(TableC + FV(0.0f, -Index * 1.6f * U), 0.0f), CHW, CHH, U, CardsA);
		}
	}
	else
	{
		// Riffle shuffle, twice: split, interleave back, square up.
		const float Riffle = (KGReveal::ShuffleEnd - KGReveal::TableEnd) * 0.5f;
		const float P = FMath::Fmod(T - KGReveal::TableEnd, Riffle) / Riffle;
		constexpr int32 Deck = 12;
		const float Split = EaseInOut(Sat(P / 0.28f)) * (1.0f - EaseInOut(Ramp(P, 0.86f, 0.14f)));
		TArray<TPair<float, FXf>> Order;   // draw order (landing time), transform
		for (int32 K = 0; K < Deck; ++K)
		{
			const int32 Half = K % 2;
			const int32 J = K / 2;
			const float Side = Half == 0 ? -1.0f : 1.0f;
			const FV HalfPos = TableC + FV(Side * 150.0f * U * Split, -J * 1.6f * U);
			const float LandAt = 0.3f + 0.5f * K / Deck;
			const float Land = EaseOut(Ramp(P, LandAt, 0.1f));
			const FV CentrePos = TableC + FV(0.0f, -K * 0.9f * U - 6.0f * U * FMath::Sin(PI * Land));
			const FV Pos = P < 0.3f ? HalfPos : FMath::Lerp(HalfPos, CentrePos, Land);
			const float Angle = (P < 0.3f ? Side * 9.0f * Split : Side * 9.0f * (1.0f - Land) * Split) + (K - Deck * 0.5f) * 0.4f;
			Order.Emplace(P < LandAt ? -1.0f + J * 0.01f : LandAt, FXf(Pos, Angle));
		}
		Order.StableSort([](const TPair<float, FXf>& L, const TPair<float, FXf>& R) { return L.Key < R.Key; });
		for (const TPair<float, FXf>& Entry : Order)
		{
			CardBack(C, Entry.Value, CHW, CHH, U, CardsA);
		}
	}

	// Seat cards (everyone else's), flying out from the deck.
	for (int32 Index = 1; Index < Seats; ++Index)
	{
		const float Depart = DepartAt(Index);
		if (T < Depart)
		{
			continue;
		}
		float SeatAngle = 0.0f;
		const FV Seat = SeatPos(Index, SeatAngle);
		const float F = EaseOut(Ramp(T, Depart, Flight));
		const FV Pos = FMath::Lerp(TableC, Seat, F);
		const float Angle = FMath::Lerp(0.0f, SeatAngle + 360.0f, F);
		CardBack(C, FXf(Pos, Angle, 0.86f, 0.86f), CHW, CHH, U, CardsA);
	}

	// --- Your card ---------------------------------------------------------------------------------------------------
	// Final layout: the card left of centre, the reading panel to its right.
	const float CardH = FMath::Min(520.0f * U, H * 0.54f);
	const float CardW = CardH * 0.7f;
	const float PanelW = FMath::Min(640.0f * U, W * 0.44f);
	const float Gap = 54.0f * U;
	const float LayoutX = (W - (CardW + Gap + PanelW)) * 0.5f;
	const FV FinalC(LayoutX + CardW * 0.5f, H * 0.5f);
	const FV HandC(W * 0.5f, TableC.Y + RY + 30.0f * U);
	const float HandHW = CHW * 1.5f;
	const float HandHH = CHH * 1.5f;

	C.Flush(Layer);
	++Layer;
	if (FaceIn > 0.0f)
	{
		// The table dims: the room is about you now.
		C.RectV(FV(0.0f, 0.0f), Size, FLinearColor(0.03f, 0.02f, 0.05f, 0.55f * FaceIn), FLinearColor(0.02f, 0.01f, 0.03f, 0.7f * FaceIn));
	}
	bool bFaceUp = false;
	FV MyC = TableC;
	float MyHW = CHW;
	float MyHH = CHH;
	if (T >= DepartAt(0))
	{
		if (T < KGReveal::DealEnd)
		{
			const float F = EaseOut(Ramp(T, DepartAt(0), 0.5f));
			MyC = FMath::Lerp(TableC, HandC, F);
			MyHW = FMath::Lerp(CHW, HandHW, F);
			MyHH = FMath::Lerp(CHH, HandHH, F);
			CardBack(C, FXf(MyC, (1.0f - F) * -14.0f), MyHW, MyHH, U, TableA);
		}
		else
		{
			const float F = Ramp(T, KGReveal::DealEnd, KGReveal::FlipEnd - KGReveal::DealEnd);
			const float Move = EaseInOut(F / 0.55f);
			MyC = FMath::Lerp(HandC, FinalC, Move);
			MyHW = FMath::Lerp(HandHW, CardW * 0.5f, Move);
			MyHH = FMath::Lerp(HandHH, CardH * 0.5f, Move);
			const float Lift = 1.0f + 0.06f * FMath::Sin(PI * F);
			const float Turn = FMath::Cos(PI * F);   // 1 -> -1: the card turns over
			bFaceUp = F >= 0.5f;
			// Perspective: the edge swinging towards you grows (right edge on the way up, then the face's left edge).
			FXf TurnX(MyC, 0.0f, FMath::Max(FMath::Abs(Turn), 0.02f) * Lift, Lift);
			TurnX.Persp = (bFaceUp ? -0.22f : 0.22f) * FMath::Sin(PI * F);
			TurnX.PerspRef = MyHW;
			if (bFaceUp)
			{
				// Burst of alignment light as the face comes round.
				const float Burst = Sat(1.0f - (F - 0.5f) * 1.4f) + 0.35f * FaceIn;
				C.Ellipse(MyC, FV(MyHW * 2.4f, MyHH * 1.8f), A(AC, 0.32f * Burst), A(AC, 0.0f), 40);
				CardFace(C, TurnX, MyHW, MyHH, U, Align, 1.0f);
			}
			else
			{
				CardBack(C, TurnX, MyHW, MyHH, U, 1.0f);
			}
		}
	}
	C.Flush(Layer);
	++Layer;

	const FLinearColor Cream = S.Cream;
	const FLinearColor CreamDim = S.CreamDim;

	// --- Title --------------------------------------------------------------------------------------------------------
	{
		const float TitleA = TableIn * (1.0f - 0.35f * FaceIn);
		const float Y = H * 0.07f;
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("MORROWMERE  ·  DUSK"), TEXT("MORROWMERE  ·  AKŞAM")),
		     FKGMenuStyle::Font("Bold", 12.0f * U, 420), FV(W * 0.5f, Y), A(S.Gold, TitleA), 0.5f);
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("THE CARDS ARE DEALT"), TEXT("KARTLAR DAĞITILIYOR")),
		     FKGMenuStyle::Font("Black", 38.0f * U, 120), FV(W * 0.5f, Y + 22.0f * U), A(Cream, TitleA), 0.5f);
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("Everyone's waiting. Someone's impatient."), TEXT("Herkes bekliyor. Biri sabırsız.")),
		     FKGMenuStyle::Font("Italic", 16.0f * U), FV(W * 0.5f, Y + 80.0f * U), A(CreamDim, 0.85f * TitleA), 0.5f);
	}

	// --- Bottom line: seats, then the ready prompt ---------------------------------------------------------------
	{
		// Seat count under the tagline (the bottom of the screen belongs to your card and the ready prompt).
		const FString Seated = FString::Format(KGRoleCard::Tr(TEXT("{0} villagers at the table"), TEXT("Masada {0} köylü")), {Seats});
		Text(Out, Layer, Geo, Seated.ToUpper(), FKGMenuStyle::Font("Bold", 12.0f * U, 300), FV(W * 0.5f, H * 0.07f + 128.0f * U),
		     A(CreamDim, 0.75f * TableIn * (1.0f - FaceIn)), 0.5f, 0.5f);
	}

	// --- Card text, once the card is square to the screen -------------------------------------------------------
	if (bFaceUp && FaceIn > 0.0f)
	{
		const float TA = FaceIn;
		const float Left = MyC.X - MyHW;
		const float Top = MyC.Y - MyHH;
		const float InnerW = MyHW * 2.0f - 56.0f * U;
		// Just under the medallion (CardFace: band = 36 % of the half height, medallion centred at 52 % of it).
		float Y = Top + 5.0f * U + MyHH * 0.36f * 0.52f + MyHW * 0.24f * 1.12f + 18.0f * U;
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("YOUR ROLE"), TEXT("ROLÜN")), FKGMenuStyle::Font("Bold", 11.0f * U, 380),
		     FV(MyC.X, Y), A(CardInk, 0.55f * TA), 0.5f);
		Y += 20.0f * U;
		const FSlateFontInfo NameFont = FitFont(Card.Name, TEXT("Black"), 40.0f * U, InnerW, 20);
		const FVector2f NameSize = Text(Out, Layer, Geo, Card.Name, NameFont, FV(MyC.X, Y), A(CardInk, TA), 0.5f);
		Y += NameSize.Y + 6.0f * U;
		// Alignment ribbon.
		const FString AlignText = KGRoleCard::AlignmentName(Align);
		const FSlateFontInfo RibbonFont = FKGMenuStyle::Font("Black", 13.0f * U, 320);
		const FVector2f RibbonText = Measure(AlignText, RibbonFont);
		const FV RibbonSize(RibbonText.X + 44.0f * U, 32.0f * U);
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(MyC.X - RibbonSize.X * 0.5f, Y), RibbonSize,
		                             A(Align == EKGAlignment::Impatient ? S.Crimson : AC, TA), -1.0f);
		Text(Out, Layer + 1, Geo, AlignText, RibbonFont, FV(MyC.X, Y + RibbonSize.Y * 0.5f),
		     A(Align == EKGAlignment::Impatient ? Cream : CardInk, TA), 0.5f, 0.5f);
		Y += RibbonSize.Y + 10.0f * U;
		Text(Out, Layer, Geo, Card.Team, FKGMenuStyle::Font("Bold", 13.0f * U, 60), FV(MyC.X, Y), A(CardInk, 0.62f * TA), 0.5f);
		// Flavour line, bottom of the card.
		const FSlateFontInfo FlavourFont = FKGMenuStyle::Font("Italic", 15.0f * U);
		const TArray<FString> Lines = Wrap(FString::Printf(TEXT("“%s”"), *Card.Flavour), FlavourFont, InnerW);
		const float LineH = Measure(TEXT("Ag"), FlavourFont).Y * 1.05f;
		float FY = Top + MyHH * 2.0f - 44.0f * U - LineH * Lines.Num();
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(MyC.X - 60.0f * U, FY - 16.0f * U), FV(120.0f * U, 2.0f * U), A(CardInk, 0.25f * TA), 1.0f);
		for (const FString& Line : Lines)
		{
			Text(Out, Layer, Geo, Line, FlavourFont, FV(MyC.X, FY), A(CardInk, 0.78f * TA), 0.5f);
			FY += LineH;
		}
		(void)Left;
	}
	Layer += 2;

	// --- Reading panel: goal, abilities, accomplices -----------------------------------------------------------------
	if (FaceIn > 0.0f)
	{
		const float Slide = EaseOut(Ramp(T, KGReveal::FlipEnd + 0.1f, 0.55f));
		const float PA = Slide;
		const float PX = LayoutX + CardW + Gap + (1.0f - Slide) * 40.0f * U;
		const float Pad = 34.0f * U;
		const float InnerW = PanelW - Pad * 2.0f;
		const FSlateFontInfo CaptionFont = FKGMenuStyle::Font("Bold", 12.0f * U, 320);
		const FSlateFontInfo GoalFont = FKGMenuStyle::Font("Bold", 21.0f * U);
		const FSlateFontInfo BodyFont = FKGMenuStyle::Font("Regular", 17.0f * U);
		const FSlateFontInfo MateFont = FKGMenuStyle::Font("Bold", 18.0f * U);
		const FSlateFontInfo MateRoleFont = FKGMenuStyle::Font("Medium", 15.0f * U);
		const float CapH = Measure(TEXT("A"), CaptionFont).Y;
		const float GoalLineH = Measure(TEXT("Ag"), GoalFont).Y * 1.08f;
		const float BodyLineH = Measure(TEXT("Ag"), BodyFont).Y * 1.12f;
		const TArray<FString> GoalLines = Wrap(Card.Goal, GoalFont, InnerW);
		TArray<TArray<FString>> AbilityLines;
		for (const FString& Ability : Card.Abilities)
		{
			AbilityLines.Add(Wrap(Ability, BodyFont, InnerW - 22.0f * U));
		}
		const bool bImpatient = Align == EKGAlignment::Impatient;
		const float MateRowH = 44.0f * U;
		const int32 MateRows = bImpatient ? FMath::Max(View.Mates.Num(), 1) : 1;
		// Measure the whole panel first so it can grow past the card on long Turkish lines.
		float Need = Pad + CapH + 10.0f * U + GoalLines.Num() * GoalLineH + 24.0f * U + CapH + 10.0f * U;
		for (const TArray<FString>& Lines : AbilityLines)
		{
			Need += Lines.Num() * BodyLineH + 8.0f * U;
		}
		Need += 20.0f * U + CapH + 10.0f * U + MateRows * MateRowH;
		Need += View.bStreamer ? 58.0f * U : 0.0f;
		Need += Pad;
		const float PH = FMath::Max(CardH * 0.62f, Need);
		const float PY = FinalC.Y - PH * 0.5f;
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(PX, PY), FV(PanelW, PH), A(S.Night, 0.92f * PA), 20.0f * U,
		                             A(Cream, 0.1f * PA), 1.0f);
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(PX, PY), FV(6.0f * U, PH), A(AC, 0.9f * PA), 3.0f * U);
		++Layer;
		float Y = PY + Pad;
		const float X = PX + Pad;
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("GOAL"), TEXT("HEDEF")), CaptionFont, FV(X, Y), A(AC, PA));
		Y += CapH + 10.0f * U;
		for (const FString& Line : GoalLines)
		{
			Text(Out, Layer, Geo, Line, GoalFont, FV(X, Y), A(Cream, PA));
			Y += GoalLineH;
		}
		Y += 24.0f * U;
		Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("ABILITIES"), TEXT("YETENEKLER")), CaptionFont, FV(X, Y), A(AC, PA));
		Y += CapH + 10.0f * U;
		for (const TArray<FString>& Lines : AbilityLines)
		{
			const float D = 5.0f * U;
			const FV Pip(X + 6.0f * U, Y + BodyLineH * 0.5f);
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, Pip - FV(D, D), FV(D * 2.0f, D * 2.0f), A(AC, PA), 1.5f * U);
			for (const FString& Line : Lines)
			{
				Text(Out, Layer, Geo, Line, BodyFont, FV(X + 22.0f * U, Y), A(CreamDim, PA));
				Y += BodyLineH;
			}
			Y += 8.0f * U;
		}
		Y += 12.0f * U;
		const float MateA = PA * EaseOut(Ramp(T, KGReveal::FlipEnd + 0.35f, 0.5f));
		if (bImpatient)
		{
			Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("YOUR ACCOMPLICES"), TEXT("SUÇ ORTAKLARIN")), CaptionFont, FV(X, Y), A(AC, MateA));
			Y += CapH + 10.0f * U;
			if (View.Mates.Num() == 0)
			{
				const FString Alone = View.bHasTeam ? KGRoleCard::Tr(TEXT("None tonight. You strike alone."), TEXT("Bu gece kimse yok. Tek başına vurursun."))
				                                    : KGRoleCard::Tr(TEXT("You work alone. Trust no one."), TEXT("Yalnız çalışırsın. Kimseye güvenme."));
				Text(Out, Layer, Geo, Alone, BodyFont, FV(X, Y + MateRowH * 0.5f), A(CreamDim, MateA), 0.0f, 0.5f);
			}
			for (const FKGRevealMateView& Mate : View.Mates)
			{
				FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(X - 8.0f * U, Y + 3.0f * U), FV(InnerW + 16.0f * U, MateRowH - 6.0f * U),
				                             A(S.Crimson, 0.16f * MateA), 10.0f * U, A(S.Crimson, 0.5f * MateA), 1.0f);
				// Mini card back.
				FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Geo, FV(X + 4.0f * U, Y + 8.0f * U), FV(20.0f * U, MateRowH - 16.0f * U),
				                             A(BackRim, MateA), 3.0f * U);
				FKGMenuStyle::DrawRoundedBox(Out, Layer + 2, Geo, FV(X + 6.5f * U, Y + 10.5f * U), FV(15.0f * U, MateRowH - 21.0f * U),
				                             A(S.Crimson, MateA), 2.0f * U);
				const FKGRoleCardText MateCard = KGRoleCard::Get(Mate.RoleId);
				Text(Out, Layer + 2, Geo, Mate.Name, MateFont, FV(X + 36.0f * U, Y + MateRowH * 0.5f), A(Cream, MateA), 0.0f, 0.5f);
				Text(Out, Layer + 2, Geo, MateCard.Name, MateRoleFont, FV(X + InnerW, Y + MateRowH * 0.5f), A(AC, MateA), 1.0f, 0.5f);
				Y += MateRowH;
			}
			if (View.Mates.Num() == 0)
			{
				Y += MateRowH;
			}
		}
		else
		{
			Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("KEEP IT SECRET"), TEXT("SIR OLARAK KALSIN")), CaptionFont, FV(X, Y), A(AC, MateA));
			Y += CapH + 10.0f * U;
			Text(Out, Layer, Geo, KGRoleCard::Tr(TEXT("Nobody else saw this card. Trust no one."), TEXT("Bu kartı başka kimse görmedi. Kimseye güvenme.")),
			     BodyFont, FV(X, Y + MateRowH * 0.5f), A(CreamDim, MateA), 0.0f, 0.5f);
			Y += MateRowH;
		}
		if (View.bStreamer)
		{
			Y += 14.0f * U;
			const FString Note = FString::Format(
				KGRoleCard::Tr(TEXT("Streamer mode: your role hides after this. Hold [{0}] to peek."),
				               TEXT("Yayıncı modu: rolün bundan sonra gizlenir. Görmek için [{0}] basılı tut.")),
				{View.PeekKey});
			for (const FString& Line : Wrap(Note, FKGMenuStyle::Font("Medium", 14.0f * U), InnerW))
			{
				Text(Out, Layer, Geo, Line, FKGMenuStyle::Font("Medium", 14.0f * U), FV(X, Y), A(S.Ghost, MateA));
				Y += 20.0f * U;
			}
		}
		Layer += 3;

		// Ready prompt under everything.
		const float ReadyA = PA;
		const FSlateFontInfo KeyFont = FKGMenuStyle::Font("Black", 14.0f * U, 120);
		const FSlateFontInfo LabelFont = FKGMenuStyle::Font("Bold", 19.0f * U);
		const float PromptY = FMath::Min(H - 58.0f * U, FinalC.Y + FMath::Max(CardH, PH) * 0.5f + 52.0f * U);
		if (!View.bReady)
		{
			const FString Key = KGRoleCard::Tr(TEXT("SPACE"), TEXT("BOŞLUK"));
			const FString Label = KGRoleCard::Tr(TEXT("I have seen my card - ready"), TEXT("Kartımı gördüm - hazırım"));
			const FVector2f KeySize = Measure(Key, KeyFont) + FVector2f(26.0f * U, 14.0f * U);
			const FVector2f LabelSize = Measure(Label, LabelFont);
			const float Total = KeySize.X + 14.0f * U + LabelSize.X;
			const float X0 = W * 0.5f - Total * 0.5f;
			const float Pulse = 0.75f + 0.25f * FMath::Sin(static_cast<float>(Real) * 4.0f);
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(X0, PromptY - KeySize.Y * 0.5f), FV(KeySize), A(S.Panel, ReadyA), 7.0f * U,
			                             A(S.Gold, Pulse * ReadyA), 1.5f);
			Text(Out, Layer + 1, Geo, Key, KeyFont, FV(X0 + KeySize.X * 0.5f, PromptY), A(S.Gold, ReadyA), 0.5f, 0.5f);
			Text(Out, Layer + 1, Geo, Label, LabelFont, FV(X0 + KeySize.X + 14.0f * U, PromptY), A(Cream, ReadyA), 0.0f, 0.5f);
		}
		else
		{
			const float Pop = FKGMenuStyle::EaseOutBack(Sat(ReadyAge / 0.3f));
			const FString Label = View.HumanCount > 1
				? FString::Format(KGRoleCard::Tr(TEXT("Ready  ·  waiting for the others ({0} / {1})"),
				                                 TEXT("Hazır  ·  diğerleri bekleniyor ({0} / {1})")), {View.ReadyCount, View.HumanCount})
				: FString(KGRoleCard::Tr(TEXT("Ready"), TEXT("Hazır")));
			const FVector2f LabelSize = Measure(Label, LabelFont);
			const FV Box(LabelSize.X + 64.0f * U, 44.0f * U);
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(W * 0.5f - Box.X * 0.5f, PromptY - Box.Y * 0.5f * Pop), FV(Box.X, Box.Y * Pop),
			                             A(S.Good, 0.16f * ReadyA), -1.0f, A(S.Good, 0.8f * ReadyA), 1.5f);
			Text(Out, Layer + 1, Geo, Label, LabelFont, FV(W * 0.5f, PromptY), A(S.Good, ReadyA), 0.5f, 0.5f);
		}
		if (View.ServerRemaining >= 0.0f)
		{
			const int32 Left = FMath::Max(0, FMath::CeilToInt(View.ServerRemaining));
			Text(Out, Layer + 1, Geo,
			     FString::Format(KGRoleCard::Tr(TEXT("THE VILLAGE WAKES IN {0}"), TEXT("KÖY {0} SN SONRA UYANIYOR")), {Left}),
			     FKGMenuStyle::Font("Bold", 11.0f * U, 320), FV(W * 0.5f, PromptY + 36.0f * U), A(CreamDim, 0.7f * ReadyA), 0.5f, 0.5f);
		}
		Layer += 2;
	}
	(void)bTr;
	return Layer;
}
