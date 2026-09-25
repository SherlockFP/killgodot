// Chore minigames (Village): PostNotice, FileReports, LightCandles, TendGraves, WindClock, GrindFlour.
// See KGMinigame.h for the contract and KGMinigames_Core.cpp for the reference set. Every stage paces itself so a perfect,
// fast player still lands above the server's plausibility floor (fixed animations, speed caps, paced arrivals).
#include "Chores/UI/KGMinigame.h"

namespace KGMg
{
namespace KGMgVillage
{
	constexpr float TwoPi = 2.0f * PI;

	FVector2f Rot(const FVector2f& V, float Ang)
	{
		const float Co = FMath::Cos(Ang);
		const float Si = FMath::Sin(Ang);
		return FVector2f(V.X * Co - V.Y * Si, V.X * Si + V.Y * Co);
	}

	FVector2f Polar(float Ang)
	{
		return FVector2f(FMath::Cos(Ang), FMath::Sin(Ang));
	}

	float WrapPi(float X)
	{
		while (X > PI)
		{
			X -= TwoPi;
		}
		while (X < -PI)
		{
			X += TwoPi;
		}
		return X;
	}

	/** Deterministic 0..1 noise for painting (Paint is const: no Rng there). */
	float Hash01(int32 N)
	{
		const float S = FMath::Sin(float(N) * 12.9898f + 4.1414f) * 43758.5453f;
		return S - FMath::FloorToFloat(S);
	}

	/** Keeps the part of a polygon below the horizontal line y = SurfaceY. */
	TArray<FVector2f> ClipBelow(const TArray<FVector2f>& Poly, float SurfaceY)
	{
		TArray<FVector2f> Out;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2f& P0 = Poly[i];
			const FVector2f& P1 = Poly[(i + 1) % Poly.Num()];
			const bool bIn0 = P0.Y >= SurfaceY;
			const bool bIn1 = P1.Y >= SurfaceY;
			if (bIn0)
			{
				Out.Add(P0);
			}
			if (bIn0 != bIn1)
			{
				const float T = (SurfaceY - P0.Y) / (P1.Y - P0.Y);
				Out.Add(P0 + (P1 - P0) * T);
			}
		}
		return Out;
	}

	/** Shape marker for colour-blind friendly matching (0 circle, 1 square, 2 triangle, 3 diamond, 4 bar, 5 cross). */
	void Symbol(FKGMgPainter& P, int32 Kind, const FVector2f& Ctr, float R, const FLinearColor& Color)
	{
		switch (Kind % 6)
		{
		case 0: P.Circle(Ctr, R, Color); break;
		case 1: P.Rect(Ctr - FVector2f(R, R) * 0.85f, FVector2f(R, R) * 1.7f, Color); break;
		case 2: P.Tri(Ctr + FVector2f(0.0f, -R), Ctr + FVector2f(R, R * 0.8f), Ctr + FVector2f(-R, R * 0.8f), Color); break;
		case 3: P.Quad(Ctr + FVector2f(0.0f, -R), Ctr + FVector2f(R, 0.0f), Ctr + FVector2f(0.0f, R), Ctr + FVector2f(-R, 0.0f), Color); break;
		case 4: P.Rect(Ctr - FVector2f(R, R * 0.4f), FVector2f(R * 2.0f, R * 0.8f), Color); break;
		default:
			P.Rect(Ctr - FVector2f(R, R * 0.32f), FVector2f(R * 2.0f, R * 0.64f), Color);
			P.Rect(Ctr - FVector2f(R * 0.32f, R), FVector2f(R * 0.64f, R * 2.0f), Color);
			break;
		}
	}

	/** Four-point twinkle. */
	void Sparkle(FKGMgPainter& P, const FVector2f& Ctr, float R, const FLinearColor& Color)
	{
		if (R < 0.5f)
		{
			return;
		}
		P.Quad(Ctr + FVector2f(0.0f, -R), Ctr + FVector2f(R * 0.22f, 0.0f), Ctr + FVector2f(0.0f, R), Ctr + FVector2f(-R * 0.22f, 0.0f), Color);
		P.Quad(Ctr + FVector2f(-R, 0.0f), Ctr + FVector2f(0.0f, -R * 0.22f), Ctr + FVector2f(R, 0.0f), Ctr + FVector2f(0.0f, R * 0.22f), Color);
	}

	/** A sheet of paper with a soft shadow (Lift raises it off the surface). */
	void Sheet(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Tint, float Lift = 0.0f)
	{
		P.Shadow(Pos, Size, 4.0f, 0.28f + 0.18f * Lift, 3.0f + 9.0f * Lift);
		P.RoundRect(Pos, Size, Tint, 3.0f);
		P.RectV(Pos + FVector2f(2.0f, Size.Y * 0.7f), FVector2f(Size.X - 4.0f, Size.Y * 0.3f - 2.0f), A(Ink, 0.0f), A(Ink, 0.07f));
		P.Rect(Pos + FVector2f(Size.X - 5.0f, 3.0f), FVector2f(3.0f, Size.Y - 6.0f), A(Ink, 0.06f));
		P.Rect(Pos + FVector2f(3.0f, 2.0f), FVector2f(Size.X - 6.0f, 2.0f), A(FLinearColor::White, 0.35f));
	}

	/** Rows of pseudo handwriting (ink "words"), optionally rotated around Pivot (Pos is then relative to Pivot). */
	void Scribble(FKGMgPainter& P, const FVector2f& Pos, float Width, int32 Rows, float Gap, int32 Seed, const FLinearColor& Color,
	              float Thick = 2.0f, const FVector2f& Pivot = FVector2f::ZeroVector, float Ang = 0.0f)
	{
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float Len = Width * (Row == Rows - 1 ? 0.45f : 0.72f + 0.28f * Hash01(Seed * 31 + Row));
			float X = 0.0f;
			int32 Word = 0;
			while (X < Len - 4.0f)
			{
				const float WordLen = FMath::Min(Len - X, 9.0f + 22.0f * Hash01(Seed * 97 + Row * 13 + Word));
				const FVector2f L0 = Pos + FVector2f(X, Row * Gap);
				const FVector2f L1 = Pos + FVector2f(X + WordLen, Row * Gap);
				P.Bar(Pivot + Rot(L0, Ang), Pivot + Rot(L1, Ang), Thick, Color);
				X += WordLen + 5.0f;
				++Word;
			}
		}
	}

	/** Blobby wax seal with an embossed symbol. */
	void WaxSeal(FKGMgPainter& P, const FVector2f& Ctr, float R, const FLinearColor& Color, int32 Kind)
	{
		if (R < 1.0f)
		{
			return;
		}
		const FLinearColor Dark = Mix(Color, Ink, 0.3f);
		for (int32 k = 0; k < 7; ++k)
		{
			P.Circle(Ctr + Polar(k * TwoPi / 7.0f + 0.3f) * R * 0.72f, R * 0.36f, Dark);
		}
		P.Circle(Ctr, R * 0.95f, Color);
		P.Circle(Ctr, R * 0.66f, Mix(Color, Ink, 0.12f), A(Mix(Color, Ink, 0.4f), 0.8f), 1.5f);
		Symbol(P, Kind, Ctr, R * 0.36f, A(Cream, 0.9f));
		P.Circle(Ctr + FVector2f(-R * 0.4f, -R * 0.42f), R * 0.16f, A(FLinearColor::White, 0.4f));
	}

	/** Brass drawing pin; Depth 0 = just set, 1 = hammered flush. */
	void BrassPin(FKGMgPainter& P, const FVector2f& Ctr, float R, float Depth)
	{
		const float Up = (1.0f - Saturate(Depth)) * 5.0f;
		P.Circle(Ctr + FVector2f(2.0f + Up, 3.0f + Up), R, A(Ink, 0.35f));
		if (Depth < 0.99f)
		{
			P.Bar(Ctr, Ctr + FVector2f(Up * 0.6f, Up * 1.2f + 2.0f), 2.5f, C(0x9A9CA4));
		}
		P.Circle(Ctr, R, C(0xD8A640), C(0x8C6A2A), 1.5f);
		P.Circle(Ctr + FVector2f(-R * 0.3f, -R * 0.3f), R * 0.35f, A(C(0xFFF0B0), 0.8f));
	}

	/** Candle-style flame standing on Base (Sway leans the tip sideways). */
	void Flame(FKGMgPainter& P, const FVector2f& Base, float Hgt, float Sway, float Alpha)
	{
		if (Alpha <= 0.01f || Hgt <= 0.5f)
		{
			return;
		}
		P.Glow(Base + FVector2f(Sway * 0.5f, -Hgt * 0.45f), Hgt * 2.4f, A(C(0xFFB347), 0.35f * Alpha));
		const float Rad = Hgt * 0.3f;
		const FVector2f Tip = Base + FVector2f(Sway, -Hgt);
		const FVector2f Bulb = Base + FVector2f(Sway * 0.15f, -Rad * 1.1f);
		P.Tri(Tip, Bulb + FVector2f(-Rad * 0.95f, 0.0f), Bulb + FVector2f(Rad * 0.95f, 0.0f), A(Lantern, Alpha));
		P.Circle(Bulb, Rad, A(Lantern, Alpha));
		const FVector2f Tip2 = Base + FVector2f(Sway * 0.75f, -Hgt * 0.62f);
		const FVector2f Bulb2 = Bulb + FVector2f(0.0f, Rad * 0.25f);
		P.Tri(Tip2, Bulb2 + FVector2f(-Rad * 0.55f, 0.0f), Bulb2 + FVector2f(Rad * 0.55f, 0.0f), A(C(0xFFE58A), Alpha));
		P.Circle(Bulb2, Rad * 0.58f, A(C(0xFFE58A), Alpha));
		P.Circle(Bulb2 + FVector2f(0.0f, Rad * 0.2f), Rad * 0.3f, A(C(0xFFFBE8), Alpha));
	}

	/** Wooden planks with seams and grain. */
	void Planks(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Base, int32 Count, bool bVertical)
	{
		const float Step = (bVertical ? Size.X : Size.Y) / float(Count);
		const float Len = bVertical ? Size.Y : Size.X;
		for (int32 i = 0; i < Count; ++i)
		{
			const FLinearColor Shade = Mix(Base, i % 2 ? WoodDark : WoodLight, 0.1f + 0.1f * Hash01(i * 7 + 3));
			const FVector2f P0 = bVertical ? Pos + FVector2f(i * Step, 0.0f) : Pos + FVector2f(0.0f, i * Step);
			P.Rect(P0, bVertical ? FVector2f(Step, Size.Y) : FVector2f(Size.X, Step), Shade);
			P.Rect(P0, bVertical ? FVector2f(2.0f, Size.Y) : FVector2f(Size.X, 2.0f), A(Ink, 0.35f));
			for (int32 g = 0; g < 2; ++g)
			{
				const float Off = Step * (0.3f + 0.4f * Hash01(i * 13 + g * 5));
				const float S0 = Len * 0.3f * Hash01(i * 17 + g);
				const float S1 = S0 + Len * (0.3f + 0.4f * Hash01(i * 19 + g * 3));
				TArray<FVector2f> Grain;
				for (int32 k = 0; k <= 4; ++k)
				{
					const float S = FMath::Lerp(S0, S1, k / 4.0f);
					const float Wob = Off + 2.0f * FMath::Sin(k * 1.7f + i);
					Grain.Add(bVertical ? P0 + FVector2f(Wob, S) : P0 + FVector2f(S, Wob));
				}
				P.Line(Grain, A(Ink, 0.13f), 1.5f);
			}
		}
	}

	/** Cork board surface with speckles. */
	void Cork(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, int32 Count)
	{
		P.RectV(Pos, Size, C(0xCB9A62), C(0xB9854F));
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2f At = Pos + FVector2f(Hash01(i * 3 + 1) * (Size.X - 4.0f), Hash01(i * 3 + 2) * (Size.Y - 4.0f));
			const float Sz = 1.6f + 2.4f * Hash01(i * 7 + 5);
			P.Rect(At, FVector2f(Sz, Sz), i % 3 == 0 ? A(C(0xE8C08A), 0.7f) : A(C(0x8A5A32), 0.5f));
		}
	}

	/** Toothed gear wheel. */
	void Gear(FKGMgPainter& P, const FVector2f& Ctr, float R, int32 Teeth, float Ang, const FLinearColor& Color)
	{
		const float ToothW = TwoPi * R / float(Teeth) * 0.5f;
		for (int32 t = 0; t < Teeth; ++t)
		{
			const float TA = Ang + t * TwoPi / float(Teeth);
			P.RotRect(Ctr + Polar(TA) * (R + 5.0f), FVector2f(12.0f, ToothW), TA, Color);
		}
		P.Circle(Ctr, R, Color);
		P.Circle(Ctr, R * 0.74f, Mix(Color, Ink, 0.35f));
		for (int32 s = 0; s < 5; ++s)
		{
			P.Bar(Ctr, Ctr + Polar(Ang + s * TwoPi / 5.0f) * R * 0.76f, R * 0.14f, Color);
		}
		P.Circle(Ctr, R * 0.2f, Mix(Color, FLinearColor::White, 0.15f), Mix(Color, Ink, 0.4f), 2.0f);
		P.Arc(Ctr, R - 2.0f, -2.5f, -1.3f, A(FLinearColor::White, 0.2f), 2.0f, 12);
	}

	/** Flat hand used for smoothing paper. */
	void Palm(FKGMgPainter& P, const FVector2f& At, float Press)
	{
		const FLinearColor Skin = C(0xF2C49A);
		const FLinearColor SkinDark = C(0xC98F66);
		const FVector2f O = At + FVector2f(0.0f, Press * 2.0f);
		P.RoundRect(O + FVector2f(-17.0f, -30.0f), FVector2f(40.0f, 58.0f), A(Ink, 0.2f), 14.0f);
		for (int32 k = 0; k < 4; ++k)
		{
			const float Len = (k == 0 || k == 3) ? 18.0f : 24.0f;
			P.RoundRect(O + FVector2f(-19.0f + k * 10.0f, -10.0f - Len), FVector2f(9.0f, Len + 10.0f), Skin, -1.0f, SkinDark, 1.5f);
		}
		P.RotRect(O + FVector2f(-22.0f, 8.0f), FVector2f(22.0f, 10.0f), -0.6f, Skin);
		P.RoundRect(O + FVector2f(-20.0f, -14.0f), FVector2f(40.0f, 32.0f), Skin, 12.0f, SkinDark, 1.5f);
		P.Rect(O + FVector2f(-12.0f, -8.0f), FVector2f(18.0f, 3.0f), A(FLinearColor::White, 0.3f));
	}

	/** Goose-feather quill with its nib at Nib. */
	void Quill(FKGMgPainter& P, const FVector2f& Nib)
	{
		const FVector2f D(0.5f, -0.866f);
		const FVector2f N(-D.Y, D.X);
		const FVector2f Base = Nib + D * 14.0f;
		const FVector2f Tip = Nib + D * 118.0f;
		P.Bar(Nib + FVector2f(10.0f, 6.0f), Tip + FVector2f(18.0f, 12.0f), 12.0f, A(Ink, 0.16f));
		auto Pt = [&Base, &D](float Along, const FVector2f& Side, float Off) { return Base + D * Along + Side * Off; };
		P.Poly({Pt(14.0f, N, 0.0f), Pt(40.0f, N, 13.0f), Pt(78.0f, N, 15.0f), Pt(100.0f, N, 5.0f), Pt(104.0f, N, 0.0f)}, C(0xF4EEE2));
		P.Poly({Pt(104.0f, N, 0.0f), Pt(100.0f, -N, 4.0f), Pt(78.0f, -N, 11.0f), Pt(40.0f, -N, 9.0f), Pt(14.0f, N, 0.0f)}, C(0xDCD3C4));
		for (int32 k = 0; k < 6; ++k)
		{
			const float Along = 30.0f + k * 12.0f;
			P.Segment(Pt(Along, N, 0.0f), Pt(Along + 8.0f, N, 12.0f), A(Ink, 0.12f), 1.0f);
		}
		P.Bar(Nib + D * 6.0f, Tip, 2.5f, C(0xB8A888));
		P.Tri(Nib, Nib + D * 14.0f + N * 3.5f, Nib + D * 14.0f - N * 3.5f, C(0x2A2230));
	}

	/** Marching-ants dashed rectangle. */
	void DashedRect(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Color, float Thick, float Tm)
	{
		const FVector2f Corners[5] = {Pos, Pos + FVector2f(Size.X, 0.0f), Pos + Size, Pos + FVector2f(0.0f, Size.Y), Pos};
		const float Dash = 10.0f;
		const float Gap = 7.0f;
		const float Offset = FMath::Fmod(Tm * 20.0f, Dash + Gap);
		for (int32 e = 0; e < 4; ++e)
		{
			const FVector2f From = Corners[e];
			const FVector2f To = Corners[e + 1];
			const float Len = FVector2f::Distance(From, To);
			const FVector2f D = (To - From) / Len;
			for (float S = Offset - Dash; S < Len; S += Dash + Gap)
			{
				const float S0 = FMath::Max(0.0f, S);
				const float S1 = FMath::Min(Len, S + Dash);
				if (S1 > S0)
				{
					P.Bar(From + D * S0, From + D * S1, Thick, Color);
				}
			}
		}
	}

	/** Chevrons running around a circle to show the turning direction. */
	void Chevrons(FKGMgPainter& P, const FVector2f& Ctr, float R, float Tm, bool bClockwise, const FLinearColor& Color)
	{
		for (int32 i = 0; i < 8; ++i)
		{
			const float Ang = (bClockwise ? Tm : -Tm) * 1.6f + i * PI / 4.0f;
			const FVector2f D = Polar(Ang);
			const FVector2f T = bClockwise ? FVector2f(-D.Y, D.X) : FVector2f(D.Y, -D.X);
			const FVector2f At = Ctr + D * R;
			P.Tri(At + T * 8.0f, At - T * 6.0f + D * 6.0f, At - T * 6.0f - D * 6.0f, Color);
		}
	}
}

// =====================================================================================================================
// PostNotice: drag the new notice onto the board, pin its corners between gusts, rub the creases out.
// =====================================================================================================================
class FKGMgPostNotice final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 3; }

	virtual FString GetInstruction() const override
	{
		if (Stage == 0)
		{
			return TEXT("Drag the notice onto the empty spot");
		}
		if (Stage == 1)
		{
			return TEXT("Pin each corner while it lies flat");
		}
		return TEXT("Hold the mouse and rub out the creases");
	}

	virtual void BeginStage() override
	{
		NoticePos = Home;
		GrabOff = FVector2f::ZeroVector;
		bHeld = false;
		bTaken = false;
		bReturning = false;
		bPlaced = false;
		PlaceT = 0.0f;
		LiftK = 0.0f;
		const float GustBase = Rng.FRand() * KGMgVillage::TwoPi;
		for (int32 i = 0; i < 4; ++i)
		{
			const bool bDone = Stage >= 2;
			Pinned[i] = bDone;
			PinDepth[i] = bDone ? 1.0f : 0.0f;
			CurlNow[i] = bDone ? 0.0f : 1.0f;
			GustPhase[i] = GustBase + i * 1.57f + Rng.FRand() * 0.5f;
		}
		HammerCorner = -1;
		HammerT = -1.0f;
		PinLock = 0.0f;
		Creases.Reset();
		CellSmooth.Init(0.0f, Cols * Rows);
		for (int32 c = 0; c < Cols * Rows; ++c)
		{
			const int32 Count = Rng.FRand() < 0.5f ? 1 : 2;
			for (int32 k = 0; k < Count; ++k)
			{
				const float Ang = Rng.FRand() * PI;
				const float Len = 34.0f + Rng.FRand() * 28.0f;
				const FVector2f D = KGMgVillage::Polar(Ang);
				const FVector2f N(-D.Y, D.X);
				const float JX = Rng.FRand() - 0.5f;
				const float JY = Rng.FRand() - 0.5f;
				const float Kink = (Rng.FRand() - 0.5f) * 14.0f;
				const FVector2f Mid = CellCentre(c) + FVector2f(JX, JY) * 24.0f;
				FCrease& Cr = Creases.AddDefaulted_GetRef();
				Cr.P0 = Mid - D * Len * 0.5f;
				Cr.P1 = Mid + N * Kink;
				Cr.P2 = Mid + D * Len * 0.5f;
				Cr.Cell = c;
			}
		}
		RubAcc = 0.0f;
		RubSoundAcc = 0.0f;
		IdleT = 0.0f;
		bAutoRub = false;
		AutoT = 0.0f;
		DoneT = -1.0f;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			const FVector2f At = DrawPos();
			if (!bPlaced && StageTime >= 0.5f &&
			    InRect(Pos, At - SmallSize * 0.5f - FVector2f(10.0f, 10.0f), SmallSize + FVector2f(20.0f, 20.0f)))
			{
				bHeld = true;
				bTaken = true;
				bReturning = false;
				NoticePos = At;
				GrabOff = At - Pos;
				Sound(TEXT("S_Chore_Paper"), 0.6f, 1.1f);
			}
		}
		else if (Stage == 1)
		{
			TryPin(Pos);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (Stage != 0 || !bHeld)
		{
			return;
		}
		bHeld = false;
		if (FVector2f::Distance(NoticePos, Slot) < SnapDist)
		{
			bPlaced = true;
			PlaceT = 0.0f;
			Nice(Slot + FVector2f(0.0f, -96.0f), TEXT("Posted!"), TEXT("S_Chore_Paper"), 1.0f);
		}
		else
		{
			bReturning = true;
			if (FVector2f::Distance(NoticePos, Home) > 80.0f)
			{
				Oops(NoticePos + FVector2f(0.0f, -84.0f), TEXT("Missed the spot"), TEXT("S_UI_Bad"), 3.0f);
			}
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage == 2)
		{
			RubAcc += Delta.Size();
		}
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickPlace(Dt);
		}
		else if (Stage == 1)
		{
			TickPin(Dt);
		}
		else
		{
			TickSmooth(Dt);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			if (bPlaced || StageTime < 0.55f)
			{
				return;
			}
			if (!bHeld)
			{
				bHeld = true;
				bTaken = true;
				bReturning = false;
				GrabOff = FVector2f::ZeroVector;
				Mouse = NoticePos;
			}
			const FVector2f Delta = Slot - Mouse;
			const float StepLen = 420.0f * Dt;
			Mouse = Delta.Size() <= StepLen ? Slot : Mouse + Delta.GetSafeNormal() * StepLen;
			if (FVector2f::Distance(NoticePos, Slot) < 4.0f)
			{
				OnRelease(Mouse);
			}
		}
		else if (Stage == 1)
		{
			if (HammerT < 0.0f && DoneT < 0.0f && StageTime > 0.6f)
			{
				for (int32 i = 0; i < 4; ++i)
				{
					if (!Pinned[i] && CurlNow[i] < 0.15f)
					{
						Mouse = PinPoint(i);
						StartPin(i);
						break;
					}
				}
			}
		}
		else
		{
			bAutoRub = true;
			AutoT += Dt;
			const FVector2f Inner = BigC - BigSize * 0.5f + FVector2f(34.0f, 34.0f);
			const FVector2f Span = BigSize - FVector2f(68.0f, 68.0f);
			const FVector2f NewMouse(Inner.X + Span.X * (0.5f + 0.5f * FMath::Sin(AutoT * 7.0f)), Inner.Y + FMath::Fmod(AutoT * 70.0f, Span.Y));
			RubAcc += FVector2f::Distance(NewMouse, Mouse);
			Mouse = NewMouse;
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			bTaken = true;
			bHeld = true;
			bPlaced = false;
			NoticePos = FMath::Lerp(Home, Slot, 0.62f) + FVector2f(0.0f, -24.0f);
			Mouse = NoticePos;
			LiftK = 1.0f;
		}
		else if (Stage == 1)
		{
			Pinned[0] = true;
			PinDepth[0] = 1.0f;
			Pinned[2] = true;
			PinDepth[2] = 1.0f;
			CurlNow[0] = 0.0f;
			CurlNow[2] = 0.0f;
			CurlNow[1] = 0.0f;
			CurlNow[3] = 0.85f;
			HammerCorner = 1;
			HammerT = 0.08f;
			PinDepth[1] = 0.0f;
			Mouse = PinPoint(1);
		}
		else
		{
			for (int32 c = 0; c < CellSmooth.Num(); ++c)
			{
				CellSmooth[c] = c < 5 ? 1.0f : c < 7 ? 0.7f : 0.12f + 0.1f * (c % 3);
			}
			bAutoRub = true;
			Mouse = CellCentre(6) + FVector2f(20.0f, 6.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintPlace(P);
		}
		else
		{
			PaintZoom(P);
		}
	}

private:
	static constexpr int32 Cols = 3;
	static constexpr int32 Rows = 4;
	static constexpr float SnapDist = 34.0f;
	static constexpr float MaxDragSpeed = 560.0f;
	static constexpr float StrikeA = 0.15f;
	static constexpr float StrikeB = 0.40f;
	static constexpr float HammerTime = 0.58f;
	static constexpr float RubCap = 800.0f;       // px of rubbing counted per second
	static constexpr float RubPerCell = 170.0f;   // weighted px of rubbing to flatten one cell
	static constexpr float BrushR = 64.0f;
	const FVector2f Home = FVector2f(110.0f, 232.0f);
	const FVector2f Slot = FVector2f(426.0f, 200.0f);
	const FVector2f SmallSize = FVector2f(112.0f, 140.0f);
	const FVector2f BigC = FVector2f(320.0f, 206.0f);
	const FVector2f BigSize = FVector2f(236.0f, 290.0f);

	// Place
	FVector2f NoticePos = FVector2f::ZeroVector;
	FVector2f GrabOff = FVector2f::ZeroVector;
	bool bHeld = false;
	bool bTaken = false;
	bool bReturning = false;
	bool bPlaced = false;
	float PlaceT = 0.0f;
	float LiftK = 0.0f;
	// Pin
	bool Pinned[4] = {false, false, false, false};
	float PinDepth[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float CurlNow[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float GustPhase[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int32 HammerCorner = -1;
	float HammerT = -1.0f;
	float PinLock = 0.0f;
	// Smooth
	struct FCrease
	{
		FVector2f P0;
		FVector2f P1;
		FVector2f P2;
		int32 Cell = 0;
	};
	TArray<FCrease> Creases;
	TArray<float> CellSmooth;
	float RubAcc = 0.0f;
	float RubSoundAcc = 0.0f;
	float IdleT = 0.0f;
	bool bAutoRub = false;
	float AutoT = 0.0f;
	float DoneT = -1.0f;

	FVector2f DrawPos() const
	{
		if (bTaken)
		{
			return NoticePos;
		}
		const float Rise = EaseOutBack(Saturate(StageTime / 0.5f));
		return NoticePos + FVector2f(0.0f, (1.0f - Rise) * 70.0f);
	}

	FVector2f Corner(int32 i) const
	{
		static const FVector2f Signs[4] = {FVector2f(-1.0f, -1.0f), FVector2f(1.0f, -1.0f), FVector2f(1.0f, 1.0f), FVector2f(-1.0f, 1.0f)};
		return BigC + BigSize * 0.5f * Signs[i];
	}

	static FVector2f NextDir(int32 i)
	{
		static const FVector2f D[4] = {FVector2f(1.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(-1.0f, 0.0f), FVector2f(0.0f, -1.0f)};
		return D[i];
	}

	static FVector2f PrevDir(int32 i)
	{
		static const FVector2f D[4] = {FVector2f(0.0f, 1.0f), FVector2f(-1.0f, 0.0f), FVector2f(0.0f, -1.0f), FVector2f(1.0f, 0.0f)};
		return D[i];
	}

	FVector2f PinPoint(int32 i) const { return Corner(i) + (NextDir(i) + PrevDir(i)) * 17.0f; }

	FVector2f CellCentre(int32 c) const
	{
		const FVector2f Inner = BigC - BigSize * 0.5f + FVector2f(12.0f, 12.0f);
		const FVector2f Cell((BigSize.X - 24.0f) / float(Cols), (BigSize.Y - 24.0f) / float(Rows));
		return Inner + FVector2f((float(c % Cols) + 0.5f) * Cell.X, (float(c / Cols) + 0.5f) * Cell.Y);
	}

	int32 NumPinned() const
	{
		int32 N = 0;
		for (int32 i = 0; i < 4; ++i)
		{
			N += Pinned[i] ? 1 : 0;
		}
		return N;
	}

	float Smoothness() const
	{
		if (CellSmooth.Num() == 0)
		{
			return 1.0f;
		}
		float Sum = 0.0f;
		for (const float S : CellSmooth)
		{
			Sum += S;
		}
		return Sum / float(CellSmooth.Num());
	}

	void TickDone(float Dt, float MinTime)
	{
		if (DoneT >= 0.0f)
		{
			DoneT += Dt;
			if (DoneT >= 0.4f && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
		}
	}

	void TickPlace(float Dt)
	{
		LiftK = Approach(LiftK, bHeld ? 1.0f : 0.0f, Dt, 12.0f);
		if (bHeld)
		{
			// The notice follows the hand but never faster than MaxDragSpeed (paper catching the air).
			const FVector2f Delta = Mouse + GrabOff - NoticePos;
			FVector2f Move = Delta * (1.0f - FMath::Exp(-20.0f * Dt));
			const float MaxMove = MaxDragSpeed * Dt;
			if (Move.Size() > MaxMove)
			{
				Move = Move.GetSafeNormal() * MaxMove;
			}
			NoticePos += Move;
		}
		else if (bPlaced)
		{
			PlaceT += Dt;
			NoticePos = FMath::Lerp(NoticePos, Slot, 1.0f - FMath::Exp(-18.0f * Dt));
			if (PlaceT >= 0.4f && StageTime >= 1.3f && !bSolved)
			{
				Solve();
			}
		}
		else if (bReturning)
		{
			NoticePos = FMath::Lerp(NoticePos, Home, 1.0f - FMath::Exp(-10.0f * Dt));
			if (FVector2f::Distance(NoticePos, Home) < 1.5f)
			{
				NoticePos = Home;
				bReturning = false;
			}
		}
	}

	void StartPin(int32 i)
	{
		HammerCorner = i;
		HammerT = 0.0f;
		Sound(TEXT("S_Chore_Whoosh"), 0.3f, 1.5f);
	}

	void TryPin(const FVector2f& Pos)
	{
		if (HammerT >= 0.0f || PinLock > 0.0f || DoneT >= 0.0f)
		{
			return;
		}
		int32 Best = -1;
		float BestD = 44.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Pinned[i])
			{
				const float D = FMath::Min(FVector2f::Distance(Pos, PinPoint(i)), FVector2f::Distance(Pos, Corner(i)));
				if (D < BestD)
				{
					BestD = D;
					Best = i;
				}
			}
		}
		if (Best < 0)
		{
			return;
		}
		if (CurlNow[Best] > 0.2f)
		{
			PinLock = 0.45f;
			Oops(PinPoint(Best) + FVector2f(0.0f, Best < 2 ? 50.0f : -50.0f), TEXT("Wait - it's flapping!"), TEXT("S_UI_Bad"), 3.0f);
			return;
		}
		StartPin(Best);
	}

	void TickPin(float Dt)
	{
		PinLock = FMath::Max(0.0f, PinLock - Dt);
		const float Intro = Saturate(1.0f - StageTime / 0.6f);   // a gust blows in as the stage opens
		for (int32 i = 0; i < 4; ++i)
		{
			if (Pinned[i] || HammerCorner == i)
			{
				CurlNow[i] = Approach(CurlNow[i], 0.0f, Dt, 20.0f);
			}
			else
			{
				const float Wave = Saturate(1.5f * FMath::Sin(2.4f * StageTime + GustPhase[i]) - 0.35f);
				CurlNow[i] = FMath::Max(Wave, Intro);
			}
		}
		if (HammerT >= 0.0f)
		{
			const float Prev = HammerT;
			HammerT += Dt;
			const int32 Cn = HammerCorner;
			if (Prev < StrikeA && HammerT >= StrikeA)
			{
				PinDepth[Cn] = 0.5f;
				Sound(TEXT("S_Chore_Pin"), 0.7f, 0.9f + 0.06f * NumPinned());
			}
			if (Prev < StrikeB && HammerT >= StrikeB)
			{
				PinDepth[Cn] = 1.0f;
				Pinned[Cn] = true;
				Nice(PinPoint(Cn) + FVector2f(0.0f, Cn < 2 ? 40.0f : -40.0f), FString::Printf(TEXT("%d / 4"), NumPinned()), TEXT("S_Chore_Pin"),
				     1.05f + 0.08f * NumPinned());
			}
			if (HammerT >= HammerTime)
			{
				HammerT = -1.0f;
				HammerCorner = -1;
				if (NumPinned() >= 4 && DoneT < 0.0f)
				{
					DoneT = 0.0f;
					Nice(BigC + FVector2f(0.0f, -20.0f), TEXT("Pinned tight!"), TEXT("S_UI_Good"), 1.2f);
				}
			}
		}
		TickDone(Dt, 2.4f);
	}

	void TickSmooth(float Dt)
	{
		const bool bRub = (bMouseDown || bAutoRub) &&
		                  InRect(Mouse, BigC - BigSize * 0.5f - FVector2f(8.0f, 8.0f), BigSize + FVector2f(16.0f, 16.0f));
		IdleT += Dt;
		if (bRub && DoneT < 0.0f)
		{
			const float Amount = FMath::Min(RubAcc, RubCap * Dt);
			if (Amount > 0.5f)
			{
				IdleT = 0.0f;
				for (int32 c = 0; c < CellSmooth.Num(); ++c)
				{
					const float Wt = Saturate(1.0f - FVector2f::Distance(Mouse, CellCentre(c)) / BrushR);
					CellSmooth[c] = FMath::Min(1.0f, CellSmooth[c] + Amount * Wt / RubPerCell);
				}
				RubSoundAcc += Amount;
				if (RubSoundAcc > 120.0f)
				{
					RubSoundAcc = 0.0f;
					Sound(TEXT("S_Chore_Brush"), 0.45f, 0.9f + 0.3f * Smoothness());
				}
			}
		}
		RubAcc = 0.0f;
		if (DoneT < 0.0f)
		{
			bool bAll = true;
			for (const float S : CellSmooth)
			{
				bAll &= S >= 0.98f;
			}
			if (bAll)
			{
				DoneT = 0.0f;
				Nice(BigC + FVector2f(0.0f, -20.0f), TEXT("Neat as new!"), TEXT("S_Chore_Paper"), 1.15f);
			}
		}
		TickDone(Dt, 2.1f);
	}

	static void OldNotice(FKGMgPainter& P, const FVector2f& Ctr, const FVector2f& Size, float Ang, const FLinearColor& Tint, int32 Seed, int32 Kind)
	{
		auto Pt = [&Ctr, Ang](float X, float Y) { return Ctr + KGMgVillage::Rot(FVector2f(X, Y), Ang); };
		P.RotRect(Ctr + FVector2f(3.0f, 5.0f), Size, Ang, A(Ink, 0.25f));
		P.RotRect(Ctr, Size, Ang, Tint);
		const FLinearColor InkC = A(C(0x3A2B42), 0.6f);
		P.Bar(Pt(-Size.X * 0.3f, -Size.Y * 0.32f), Pt(Size.X * 0.3f, -Size.Y * 0.32f), 5.0f, InkC);
		if (Kind == 1)
		{
			// "Missing cat" poster.
			const float R = Size.X * 0.17f;
			const float Y0 = -Size.Y * 0.06f;
			const FLinearColor Fur = C(0x8C8A94);
			P.Tri(Pt(-R * 0.95f, Y0 - R * 0.3f), Pt(-R * 0.15f, Y0 - R * 0.9f), Pt(-R * 1.0f, Y0 - R * 1.5f), Fur);
			P.Tri(Pt(R * 0.95f, Y0 - R * 0.3f), Pt(R * 0.15f, Y0 - R * 0.9f), Pt(R * 1.0f, Y0 - R * 1.5f), Fur);
			P.Circle(Pt(0.0f, Y0), R, Fur);
			P.Circle(Pt(-R * 0.38f, Y0 - R * 0.1f), R * 0.15f, Ink);
			P.Circle(Pt(R * 0.38f, Y0 - R * 0.1f), R * 0.15f, Ink);
			P.Tri(Pt(-R * 0.12f, Y0 + R * 0.2f), Pt(R * 0.12f, Y0 + R * 0.2f), Pt(0.0f, Y0 + R * 0.36f), C(0xE07A8A));
		}
		else if (Kind == 2)
		{
			// Fish prices: a little fish sketch.
			P.RotRect(Pt(-Size.X * 0.06f, -Size.Y * 0.08f), FVector2f(Size.X * 0.42f, Size.Y * 0.14f), Ang, Water);
			P.Tri(Pt(Size.X * 0.14f, -Size.Y * 0.08f), Pt(Size.X * 0.3f, -Size.Y * 0.17f), Pt(Size.X * 0.3f, 0.01f * Size.Y), Water);
			P.Circle(Pt(-Size.X * 0.2f, -Size.Y * 0.1f), 2.5f, Ink);
		}
		const float LinesY = Kind == 0 ? -Size.Y * 0.2f : Size.Y * 0.1f;
		KGMgVillage::Scribble(P, FVector2f(-Size.X * 0.36f, LinesY), Size.X * 0.72f, Kind == 0 ? 6 : 3, 11.0f, Seed, InkC, 2.0f, Ctr, Ang);
		KGMgVillage::BrassPin(P, Pt(0.0f, -Size.Y * 0.43f), 5.5f, 1.0f);
	}

	void PaintSmallNotice(FKGMgPainter& P, const FVector2f& Ctr, float LiftAmt) const
	{
		const FVector2f Pos = Ctr - SmallSize * 0.5f;
		KGMgVillage::Sheet(P, Pos, SmallSize, Paper, LiftAmt);
		const FLinearColor InkC = C(0x3A2B42);
		P.Text(FVector2f(Ctr.X, Pos.Y + 8.0f), TEXT("NOTICE"), 13.0f, InkC, 0.5f, TEXT("Black"), 40);
		P.Bar(FVector2f(Ctr.X - 34.0f, Pos.Y + 30.0f), FVector2f(Ctr.X + 34.0f, Pos.Y + 30.0f), 2.0f, A(Crimson, 0.7f));
		KGMgVillage::Scribble(P, Pos + FVector2f(14.0f, 42.0f), SmallSize.X - 28.0f, 5, 12.0f, 3, A(InkC, 0.55f), 2.0f);
		KGMgVillage::WaxSeal(P, Ctr + FVector2f(32.0f, 50.0f), 11.0f, Crimson, 0);
	}

	void PaintPlace(FKGMgPainter& P) const
	{
		const float GroundY = 352.0f;
		// Timber-framed plaster wall.
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0xEEDDBA), C(0xDCC49C));
		P.Rect(FVector2f(0.0f, 0.0f), FVector2f(W, 14.0f), WoodDark);
		P.Rect(FVector2f(14.0f, 0.0f), FVector2f(18.0f, GroundY), WoodDark);
		P.Rect(FVector2f(196.0f, 0.0f), FVector2f(18.0f, GroundY), WoodDark);
		P.Rect(FVector2f(32.0f, 156.0f), FVector2f(164.0f, 14.0f), WoodDark);
		// Window with a flower box.
		P.RoundRect(FVector2f(58.0f, 34.0f), FVector2f(104.0f, 90.0f), WoodDark, 4.0f);
		P.RectV(FVector2f(64.0f, 40.0f), FVector2f(92.0f, 78.0f), C(0x8FD3F4), C(0xE6F4F8));
		P.Circle(FVector2f(128.0f, 60.0f), 9.0f, A(FLinearColor::White, 0.9f));
		P.Circle(FVector2f(140.0f, 62.0f), 7.0f, A(FLinearColor::White, 0.9f));
		P.Rect(FVector2f(64.0f, 100.0f), FVector2f(92.0f, 18.0f), C(0x7FB6C8));
		P.Rect(FVector2f(108.0f, 40.0f), FVector2f(4.0f, 78.0f), WoodDark);
		P.Rect(FVector2f(64.0f, 76.0f), FVector2f(92.0f, 4.0f), WoodDark);
		P.RoundRect(FVector2f(52.0f, 122.0f), FVector2f(116.0f, 16.0f), Wood, 3.0f, WoodDark, 2.0f);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector2f Fl(62.0f + i * 19.0f, 116.0f - (i % 2) * 5.0f);
			P.Bar(Fl, Fl + FVector2f(0.0f, 8.0f), 2.0f, Grass);
			P.Circle(Fl, 6.0f, i % 2 ? C(0xE0413A) : Gold);
			P.Circle(Fl, 2.5f, Cream);
		}
		// Cobbles.
		P.RectV(FVector2f(0.0f, GroundY), FVector2f(W, H - GroundY), C(0xA59C8C), C(0x857C6E));
		for (int32 Row = 0; Row < 2; ++Row)
		{
			for (int32 i = 0; i < 17; ++i)
			{
				P.RoundRect(FVector2f(i * 42.0f - (Row % 2) * 21.0f, GroundY + 6.0f + Row * 22.0f), FVector2f(38.0f, 17.0f), C(0x958C7E), 6.0f);
			}
		}

		// Notice board: posts, little roof, frame and cork.
		P.Rect(FVector2f(238.0f, 40.0f), FVector2f(16.0f, GroundY - 34.0f), WoodDark);
		P.Rect(FVector2f(594.0f, 40.0f), FVector2f(16.0f, GroundY - 34.0f), WoodDark);
		P.Quad(FVector2f(212.0f, 46.0f), FVector2f(636.0f, 46.0f), FVector2f(606.0f, 4.0f), FVector2f(242.0f, 4.0f), C(0x9C3B2E));
		for (int32 r = 0; r < 3; ++r)
		{
			const float Y = 14.0f + r * 11.0f;
			const float T = (Y - 4.0f) / 42.0f;
			P.Segment(FVector2f(FMath::Lerp(242.0f, 212.0f, T), Y), FVector2f(FMath::Lerp(606.0f, 636.0f, T), Y), A(Ink, 0.22f), 2.0f);
		}
		P.Rect(FVector2f(208.0f, 42.0f), FVector2f(432.0f, 8.0f), C(0x6E2A21));
		P.Text(FVector2f(424.0f, 14.0f), TEXT("MORROWMERE NOTICES"), 11.0f, A(Cream, 0.9f), 0.5f, TEXT("Black"), 120);
		P.Shadow(FVector2f(228.0f, 52.0f), FVector2f(392.0f, 304.0f), 6.0f, 0.35f, 6.0f);
		P.RoundRect(FVector2f(228.0f, 52.0f), FVector2f(392.0f, 304.0f), Wood, 6.0f, WoodDark, 3.0f);
		P.Rect(FVector2f(232.0f, 55.0f), FVector2f(384.0f, 3.0f), A(WoodLight, 0.8f));
		KGMgVillage::Cork(P, FVector2f(244.0f, 66.0f), FVector2f(360.0f, 274.0f), 90);
		P.RectV(FVector2f(244.0f, 66.0f), FVector2f(360.0f, 12.0f), A(Ink, 0.3f), A(Ink, 0.0f));

		OldNotice(P, FVector2f(300.0f, 124.0f), FVector2f(88.0f, 104.0f), -0.07f, Paper, 1, 0);
		OldNotice(P, FVector2f(298.0f, 272.0f), FVector2f(90.0f, 108.0f), 0.05f, C(0xE7F0F7), 2, 1);
		OldNotice(P, FVector2f(548.0f, 126.0f), FVector2f(86.0f, 100.0f), 0.06f, C(0xF9E3C8), 3, 2);
		OldNotice(P, FVector2f(550.0f, 274.0f), FVector2f(90.0f, 106.0f), -0.04f, Paper, 4, 0);

		// The empty slot (paler cork where an old notice hung).
		const FVector2f SPos = Slot - SmallSize * 0.5f;
		const bool bNear = bHeld && FVector2f::Distance(NoticePos, Slot) < SnapDist;
		P.Rect(SPos, SmallSize, A(C(0xE2B784), 0.55f));
		if (bNear)
		{
			P.Glow(Slot, 110.0f, A(Good, 0.3f));
		}
		KGMgVillage::DashedRect(P, SPos, SmallSize, bNear ? Good : A(Cream, 0.9f), 3.0f, Time);
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2f Hole = SPos + FVector2f(i % 2 ? SmallSize.X - 12.0f : 12.0f, i < 2 ? 12.0f : SmallSize.Y - 12.0f);
			P.Circle(Hole, 2.5f, A(Ink, 0.5f));
		}
		if (!bPlaced)
		{
			P.Text(FVector2f(Slot.X, Slot.Y - 8.0f), TEXT("POST HERE"), 11.0f, A(C(0x5A3A20), 0.5f + 0.3f * KGMg::Ping(Time, 1.4f)), 0.5f,
			       TEXT("Black"), 60);
		}

		// Table + satchel (the notice pokes out of it).
		P.Rect(FVector2f(26.0f, 316.0f), FVector2f(10.0f, GroundY - 310.0f), WoodDark);
		P.Rect(FVector2f(186.0f, 316.0f), FVector2f(10.0f, GroundY - 310.0f), WoodDark);
		P.RoundRect(FVector2f(10.0f, 304.0f), FVector2f(200.0f, 14.0f), Wood, 3.0f, WoodDark, 2.0f);
		P.Arc(FVector2f(112.0f, 262.0f), 68.0f, PI, 2.0f * PI, C(0x5A3520), 6.0f, 24);
		P.RoundRect(FVector2f(44.0f, 250.0f), FVector2f(136.0f, 60.0f), C(0x6A3F22), 10.0f);
		if (!bHeld && !bPlaced)
		{
			PaintSmallNotice(P, DrawPos(), LiftK);
		}
		P.RoundRect(FVector2f(38.0f, 268.0f), FVector2f(148.0f, 40.0f), C(0x8E5A31), 10.0f, C(0x5A3520), 2.0f);
		for (int32 i = 0; i < 12; ++i)
		{
			P.Rect(FVector2f(46.0f + i * 11.5f, 274.0f), FVector2f(6.0f, 2.0f), A(Cream, 0.4f));
		}
		P.Rect(FVector2f(106.0f, 268.0f), FVector2f(12.0f, 40.0f), C(0x5A3520));
		P.RoundRect(FVector2f(102.0f, 280.0f), FVector2f(20.0f, 14.0f), C(0xD8A640), 3.0f, C(0x8C6A2A), 1.5f);

		if (bHeld || bPlaced)
		{
			PaintSmallNotice(P, NoticePos, LiftK);
		}
		if (bPlaced)
		{
			const float Pop = Saturate(PlaceT / 0.4f);
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector2f Corner0 = Slot + FVector2f(i % 2 ? 50.0f : -50.0f, i < 2 ? -64.0f : 64.0f);
				KGMgVillage::Sparkle(P, Corner0, 12.0f * FMath::Sin(Pop * PI), A(Gold, 0.9f));
			}
		}

		// Hints.
		if (!bHeld && !bPlaced && !bReturning && StageTime > 0.5f)
		{
			if (!bTaken)
			{
				P.HintRing(DrawPos() + FVector2f(0.0f, -20.0f), 44.0f, Time, Gold);
			}
			P.HintArrow(FVector2f(190.0f, 200.0f), FVector2f(362.0f, 200.0f), Time, Gold);
		}
	}

	void PaintBigNotice(FKGMgPainter& P) const
	{
		const FVector2f Pos = BigC - BigSize * 0.5f;
		const float Crumple = Stage >= 2 ? 1.0f - Smoothness() : 0.0f;
		float Cut[4];
		TArray<FVector2f> Outline;
		for (int32 i = 0; i < 4; ++i)
		{
			Cut[i] = CurlNow[i] * 46.0f;
			const FVector2f K = Corner(i);
			if (Cut[i] > 0.5f)
			{
				Outline.Add(K + PrevDir(i) * Cut[i]);
				Outline.Add(K + NextDir(i) * Cut[i]);
			}
			else
			{
				Outline.Add(K);
			}
		}
		TArray<FVector2f> ShadowPts;
		TArray<FVector2f> ShadowFar;
		for (const FVector2f& V : Outline)
		{
			ShadowPts.Add(V + FVector2f(4.0f, 7.0f));
			ShadowFar.Add(V + FVector2f(7.0f, 12.0f));
		}
		P.Poly(ShadowFar, A(Ink, 0.12f));
		P.Poly(ShadowPts, A(Ink, 0.25f));
		P.Poly(Outline, Mix(Paper, C(0xD9C49A), Crumple * 0.7f));
		P.RoundRect(Pos + FVector2f(9.0f, 9.0f), BigSize - FVector2f(18.0f, 18.0f), FLinearColor::Transparent, 2.0f, A(C(0xB89A6A), 0.55f), 1.5f);

		// Print.
		const FLinearColor InkC = C(0x3A2B42);
		P.Text(FVector2f(BigC.X, Pos.Y + 20.0f), TEXT("NOTICE"), 28.0f, InkC, 0.5f, TEXT("Black"), 60);
		P.Bar(FVector2f(BigC.X - 70.0f, Pos.Y + 64.0f), FVector2f(BigC.X + 70.0f, Pos.Y + 64.0f), 3.0f, A(Crimson, 0.8f));
		P.Text(FVector2f(BigC.X, Pos.Y + 74.0f), TEXT("Harvest Supper"), 17.0f, InkC, 0.5f, TEXT("Bold"));
		P.Text(FVector2f(BigC.X, Pos.Y + 100.0f), TEXT("at the Town Hall"), 12.0f, A(InkC, 0.85f), 0.5f, TEXT("Medium"));
		KGMgVillage::Scribble(P, Pos + FVector2f(30.0f, 136.0f), BigSize.X - 60.0f, 6, 16.0f, 21, A(InkC, 0.42f), 2.5f);
		P.Text(FVector2f(BigC.X - 30.0f, Pos.Y + 238.0f), TEXT("- the Mayor"), 12.0f, A(InkC, 0.8f), 0.5f, TEXT("Medium"));
		KGMgVillage::WaxSeal(P, FVector2f(BigC.X + 70.0f, Pos.Y + 246.0f), 18.0f, Crimson, 0);

		// Creases (Smooth stage): faceted shading + a dark fold line with a light edge.
		if (Stage >= 2)
		{
			for (const FCrease& Cr : Creases)
			{
				const float Rough = 1.0f - CellSmooth[Cr.Cell];
				if (Rough <= 0.02f)
				{
					continue;
				}
				P.Tri(Cr.P0, Cr.P1, Cr.P2, A(Ink, 0.06f * Rough));
				P.Line({Cr.P0, Cr.P1, Cr.P2}, A(Ink, 0.34f * Rough), 2.5f);
				const FVector2f Off(1.5f, 1.8f);
				P.Line({Cr.P0 + Off, Cr.P1 + Off, Cr.P2 + Off}, A(FLinearColor::White, 0.6f * Rough), 1.8f);
			}
		}

		// Curled corners: the flap folds back and flutters.
		for (int32 i = 0; i < 4; ++i)
		{
			if (Cut[i] <= 0.5f)
			{
				continue;
			}
			const FVector2f K = Corner(i);
			const FVector2f E1 = K + PrevDir(i) * Cut[i];
			const FVector2f E2 = K + NextDir(i) * Cut[i];
			const float Flut = 0.75f + 0.25f * FMath::Sin(Time * 17.0f + i * 1.3f);
			const FVector2f Tip = K + (PrevDir(i) + NextDir(i)) * Cut[i] * Flut;
			P.Tri(E1 + FVector2f(2.0f, 4.0f), E2 + FVector2f(2.0f, 4.0f), Tip + FVector2f(3.0f, 6.0f), A(Ink, 0.22f));
			P.Tri(E1, E2, Tip, C(0xE2CFA2));
			P.Segment(E1, E2, A(C(0x9C8058), 0.7f), 1.5f);
		}
	}

	void PaintZoom(FKGMgPainter& P) const
	{
		KGMgVillage::Cork(P, FVector2f(0.0f, 0.0f), FVector2f(W, H), 170);
		OldNotice(P, FVector2f(56.0f, 140.0f), FVector2f(170.0f, 210.0f), -0.06f, C(0xE7F0F7), 11, 1);
		OldNotice(P, FVector2f(592.0f, 250.0f), FVector2f(170.0f, 214.0f), 0.07f, C(0xF9E3C8), 12, 2);
		P.Rect(FVector2f(0.0f, 0.0f), FVector2f(W, 14.0f), Wood);
		P.Rect(FVector2f(0.0f, 12.0f), FVector2f(W, 4.0f), WoodDark);
		P.Rect(FVector2f(0.0f, H - 14.0f), FVector2f(W, 14.0f), Wood);
		P.Rect(FVector2f(0.0f, H - 16.0f), FVector2f(W, 3.0f), WoodLight);

		// Gust streaks while corners are flapping.
		float Gust = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Pinned[i])
			{
				Gust = FMath::Max(Gust, CurlNow[i]);
			}
		}
		if (Stage == 1 && Gust > 0.05f)
		{
			for (int32 k = 0; k < 6; ++k)
			{
				const float X = FMath::Fmod(Time * 420.0f + k * 137.0f, 760.0f) - 60.0f;
				const float Y = 40.0f + k * 60.0f + 8.0f * FMath::Sin(Time * 3.0f + k);
				P.Bar(FVector2f(X, Y), FVector2f(X + 56.0f, Y - 5.0f), 3.0f, A(FLinearColor::White, 0.35f * Gust));
			}
		}

		PaintBigNotice(P);
		for (int32 i = 0; i < 4; ++i)
		{
			if (PinDepth[i] > 0.0f)
			{
				KGMgVillage::BrassPin(P, PinPoint(i), 9.0f, PinDepth[i]);
			}
		}

		if (Stage == 1)
		{
			// Corner hints: gold = pin now, red = wait for the gust to pass.
			if (HammerT < 0.0f && DoneT < 0.0f)
			{
				for (int32 i = 0; i < 4; ++i)
				{
					if (Pinned[i])
					{
						continue;
					}
					if (CurlNow[i] < 0.2f)
					{
						P.HintRing(PinPoint(i), 22.0f, Time + i * 0.25f, Gold);
					}
					else
					{
						P.Arc(PinPoint(i), 18.0f, 0.0f, KGMgVillage::TwoPi, A(C(0xFF8A80), 0.55f), 2.0f, 24);
					}
				}
			}
			PaintHammer(P);
			P.Tag(FVector2f(560.0f, 40.0f), FString::Printf(TEXT("PINS  %d / 4"), NumPinned()), A(Ink, 0.8f), Cream, 14.0f);
		}
		else
		{
			const float Sm = Smoothness();
			if (DoneT >= 0.0f)
			{
				for (int32 i = 0; i < 5; ++i)
				{
					const float Ph = Saturate(DoneT * 2.5f - i * 0.2f);
					const FVector2f At = BigC + FVector2f(-80.0f + i * 40.0f, -100.0f + (i % 2) * 150.0f);
					KGMgVillage::Sparkle(P, At, 16.0f * FMath::Sin(Ph * PI), A(Gold, 0.9f));
				}
			}
			const bool bRub = bMouseDown || bAutoRub;
			if (Sm < 0.04f && !bRub)
			{
				P.HintArrow(FVector2f(236.0f, 196.0f), FVector2f(404.0f, 196.0f), Time, Gold);
				P.HintArrow(FVector2f(404.0f, 226.0f), FVector2f(236.0f, 226.0f), Time + 0.3f, Gold);
			}
			else if (IdleT > 1.2f && DoneT < 0.0f)
			{
				int32 Worst = 0;
				for (int32 c = 1; c < CellSmooth.Num(); ++c)
				{
					if (CellSmooth[c] < CellSmooth[Worst])
					{
						Worst = c;
					}
				}
				P.HintRing(CellCentre(Worst), 30.0f, Time, Gold);
			}
			if (InRect(Mouse, BigC - BigSize * 0.5f - FVector2f(40.0f, 40.0f), BigSize + FVector2f(80.0f, 80.0f)) || bRub)
			{
				if (bRub)
				{
					P.Arc(Mouse, 34.0f, Time * 9.0f, Time * 9.0f + 1.4f, A(FLinearColor::White, 0.5f), 3.0f, 12);
				}
				KGMgVillage::Palm(P, Mouse, bRub ? 1.0f : 0.0f);
			}
			P.Tag(FVector2f(560.0f, 40.0f), FString::Printf(TEXT("SMOOTH  %d%%"), FMath::RoundToInt(Sm * 100.0f)), A(Ink, 0.8f), Cream, 14.0f);
			P.Gauge(FVector2f(496.0f, 62.0f), FVector2f(128.0f, 16.0f), Sm, Good);
		}
	}

	void PaintHammer(FKGMgPainter& P) const
	{
		if (HammerCorner < 0 || HammerT < 0.0f)
		{
			return;
		}
		const FVector2f Pp = PinPoint(HammerCorner);
		const float T = HammerT;
		float Raise = 0.0f;
		if (T < StrikeA)
		{
			Raise = 1.0f - T / StrikeA;
		}
		else if (T < 0.27f)
		{
			Raise = (T - StrikeA) / (0.27f - StrikeA) * 0.7f;
		}
		else if (T < StrikeB)
		{
			Raise = 0.7f * (1.0f - (T - 0.27f) / (StrikeB - 0.27f));
		}
		else
		{
			Raise = (T - StrikeB) / (HammerTime - StrikeB) * 1.2f;
		}
		const FVector2f Grip = Pp + FVector2f(78.0f, HammerCorner < 2 ? 56.0f : 40.0f);
		const FVector2f HeadC = Grip + KGMgVillage::Rot(Pp - Grip, Raise * 0.8f);
		const FVector2f Dir = (HeadC - Grip).GetSafeNormal();
		const float HeadScale = 1.0f + 0.25f * Raise;
		const float Ang = FMath::Atan2(Dir.Y, Dir.X);
		P.Bar(Grip + FVector2f(6.0f, 10.0f) - Dir * 26.0f, HeadC + FVector2f(6.0f, 10.0f), 8.0f, A(Ink, 0.2f));
		P.Bar(Grip - Dir * 26.0f, HeadC, 8.0f, WoodLight);
		P.Bar(Grip - Dir * 26.0f + FVector2f(-Dir.Y, Dir.X) * 2.0f, HeadC + FVector2f(-Dir.Y, Dir.X) * 2.0f, 2.0f, A(FLinearColor::White, 0.3f));
		P.RotRect(HeadC, FVector2f(18.0f, 40.0f) * HeadScale, Ang, Iron);
		P.RotRect(HeadC + Dir * 5.0f * HeadScale, FVector2f(5.0f, 38.0f) * HeadScale, Ang, C(0x8B929C));
		const float Flash = FMath::Max(1.0f - FMath::Abs(T - StrikeA) / 0.06f, 1.0f - FMath::Abs(T - StrikeB) / 0.06f);
		if (Flash > 0.0f)
		{
			for (int32 k = 0; k < 6; ++k)
			{
				const FVector2f D = KGMgVillage::Polar(k * KGMgVillage::TwoPi / 6.0f + 0.3f);
				P.Bar(Pp + D * 14.0f, Pp + D * (14.0f + 12.0f * Flash), 3.0f, A(Gold, Flash));
			}
		}
	}
};

// =====================================================================================================================
// FileReports: file each sealed report into the drawer with the matching seal, then sign the town record.
// =====================================================================================================================
class FKGMgFileReports final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Drag each report to the drawer with its seal") : TEXT("Hold and trace the signature on the dots");
	}

	virtual void BeginStage() override
	{
		DoneT = -1.0f;
		AutoT = 0.0f;
		if (Stage == 0)
		{
			Docs = {0, 0, 1, 1, 2, 2, 3, 3};
			Rng.Shuffle(Docs);
			TArray<int32> Kinds = {0, 1, 2, 3};
			Rng.Shuffle(Kinds);
			for (int32 d = 0; d < 4; ++d)
			{
				DrawerKind[d] = Kinds[d];
			}
			Cur = -1;
			Filed = 0;
			DocPos = PileTop();
			GrabOff = FVector2f::ZeroVector;
			bHeld = false;
			bBack = false;
			ArriveT = 0.0f;
			SinceSpawn = 0.55f;
			FileT = -1.0f;
			FileDrawer = -1;
			RattleDrawer = -1;
			RattleT = 0.0f;
		}
		else
		{
			// Loopy cursive signature: a sine wave with enough swing to curl into loops.
			Path.Reset();
			PathLen.Reset();
			const float Ph = Rng.FRand() * 0.8f;
			const int32 N = 300;
			for (int32 k = 0; k <= N; ++k)
			{
				const float T = float(k) / float(N);
				const float Wv = KGMgVillage::TwoPi * 6.0f * T + Ph;
				Path.Add(FVector2f(150.0f + 320.0f * T + 20.0f * FMath::Sin(Wv), 262.0f - 26.0f * FMath::Sin(Wv + 1.3f) * (1.0f - 0.3f * T) + 6.0f * FMath::Sin(PI * T)));
				PathLen.Add(k == 0 ? 0.0f : PathLen.Last() + FVector2f::Distance(Path[k - 1], Path[k]));
			}
			InkS = 0.0f;
			InkTarget = 0.0f;
			bStroke = false;
			GhostS = 0.0f;
			GhostT = 0.0f;
			Blots.Reset();
			ScratchAcc = 0.0f;
		}
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			if (Cur >= 0 && FileT < 0.0f && ArriveT >= 0.15f &&
			    InRect(Pos, DocPos - DocSize * 0.5f - FVector2f(8.0f, 8.0f), DocSize + FVector2f(16.0f, 16.0f)))
			{
				bHeld = true;
				bBack = false;
				GrabOff = DocPos - Pos;
				Sound(TEXT("S_Chore_Paper"), 0.5f, 1.2f);
			}
		}
		else if (DoneT < 0.0f && Path.Num() > 1 && FVector2f::Distance(Pos, PathAt(InkS)) < Tolerance)
		{
			bStroke = true;
			InkTarget = InkS;
			Sound(TEXT("S_UI_Click"), 0.4f, 1.3f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (Stage == 1)
		{
			bStroke = false;
			return;
		}
		if (!bHeld)
		{
			return;
		}
		bHeld = false;
		const int32 D = DrawerAt(Pos);
		if (D < 0 || Cur < 0)
		{
			bBack = true;
			return;
		}
		if (DrawerKind[D] == Docs[Cur])
		{
			FileT = 0.0f;
			FileDrawer = D;
			FileFrom = DocPos;
			Nice(DrawerCentre(D) + FVector2f(-40.0f, -30.0f), TEXT("Filed!"), TEXT("S_Chore_Paper"), 0.9f + 0.05f * Filed);
		}
		else
		{
			RattleDrawer = D;
			RattleT = 0.4f;
			bBack = true;
			Oops(DrawerCentre(D) + FVector2f(-60.0f, -30.0f), TEXT("Wrong drawer!"), TEXT("S_UI_Bad"), 5.0f);
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage != 1 || !bStroke || DoneT >= 0.0f)
		{
			return;
		}
		float S = 0.0f;
		if (Nearest(Pos, InkS - 12.0f, InkS + Window, S) <= Tolerance)
		{
			InkTarget = FMath::Max(InkTarget, S);
			return;
		}
		float Ahead = 0.0f;
		const bool bTooFast = Nearest(Pos, InkS + Window, InkS + 420.0f, Ahead) <= Tolerance;
		BreakStroke(bTooFast);
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickSort(Dt);
		}
		else
		{
			TickSign(Dt);
		}
		if (DoneT >= 0.0f)
		{
			DoneT += Dt;
			const float MinTime = Stage == 0 ? 5.6f : 2.4f;
			if (DoneT >= 0.5f && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			if (Cur < 0 || FileT >= 0.0f || ArriveT < 0.35f)
			{
				return;
			}
			if (!bHeld)
			{
				bHeld = true;
				bBack = false;
				GrabOff = FVector2f::ZeroVector;
				Mouse = DocPos;
			}
			const FVector2f Goal = DrawerCentre(DrawerOfKind(Docs[Cur]));
			const FVector2f Delta = Goal - Mouse;
			const float StepLen = 700.0f * Dt;
			if (Delta.Size() <= StepLen)
			{
				Mouse = Goal;
				OnRelease(Mouse);
			}
			else
			{
				Mouse += Delta.GetSafeNormal() * StepLen;
			}
		}
		else if (DoneT < 0.0f && Path.Num() > 1)
		{
			bStroke = true;
			InkTarget = FMath::Min(PathLen.Last(), InkS + 50.0f);
			Mouse = PathAt(InkTarget);
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			Filed = 3;
			Cur = 3;
			ArriveT = 1.0f;
			FileT = -1.0f;
			bHeld = true;
			bBack = false;
			GrabOff = FVector2f::ZeroVector;
			DocPos = DrawerCentre(DrawerOfKind(Docs[Cur])) + FVector2f(-120.0f, 14.0f);
			Mouse = DocPos;
		}
		else if (Path.Num() > 1)
		{
			InkS = PathLen.Last() * 0.55f;
			InkTarget = InkS;
			bStroke = true;
			Mouse = PathAt(InkS + 12.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintSort(P);
		}
		else
		{
			PaintSign(P);
		}
	}

private:
	static constexpr int32 NumDocs = 8;
	static constexpr float SignSpeed = 340.0f;   // px of ink per second the quill can lay down
	static constexpr float Window = 110.0f;      // how far ahead of the ink the hand may lead
	static constexpr float Tolerance = 26.0f;
	const FVector2f DocSize = FVector2f(100.0f, 124.0f);
	const FVector2f TrayC = FVector2f(150.0f, 296.0f);
	const FVector2f DrawerSize = FVector2f(188.0f, 80.0f);

	// Sort
	TArray<int32> Docs;
	int32 DrawerKind[4] = {0, 1, 2, 3};
	int32 Cur = -1;
	int32 Filed = 0;
	FVector2f DocPos = FVector2f::ZeroVector;
	FVector2f GrabOff = FVector2f::ZeroVector;
	FVector2f FileFrom = FVector2f::ZeroVector;
	bool bHeld = false;
	bool bBack = false;
	float ArriveT = 0.0f;
	float SinceSpawn = 0.0f;
	float FileT = -1.0f;
	int32 FileDrawer = -1;
	int32 RattleDrawer = -1;
	float RattleT = 0.0f;
	// Sign
	TArray<FVector2f> Path;
	TArray<float> PathLen;
	float InkS = 0.0f;
	float InkTarget = 0.0f;
	bool bStroke = false;
	float GhostS = 0.0f;
	float GhostT = 0.0f;
	TArray<FVector2f> Blots;
	float ScratchAcc = 0.0f;
	float DoneT = -1.0f;
	float AutoT = 0.0f;

	static FLinearColor SealColor(int32 Kind)
	{
		static const uint32 Colors[] = {0xD6453D, 0x3F7FD6, 0x4FA84A, 0xE0A824};
		return C(Colors[Kind % 4]);
	}

	static const TCHAR* Label(int32 Kind)
	{
		static const TCHAR* Labels[] = {TEXT("TAXES"), TEXT("HARBOUR"), TEXT("MARKET"), TEXT("CHURCH")};
		return Labels[Kind % 4];
	}

	FVector2f PileTop() const
	{
		const int32 Left = FMath::Max(0, NumDocs - Filed - (Cur >= 0 ? 1 : 0));
		return FVector2f(42.0f, 336.0f - Left * 4.0f);
	}

	FVector2f DrawerPos(int32 d) const { return FVector2f(412.0f, 36.0f + d * 88.0f); }
	FVector2f DrawerCentre(int32 d) const { return DrawerPos(d) + DrawerSize * 0.5f; }

	int32 DrawerAt(const FVector2f& Pos) const
	{
		for (int32 d = 0; d < 4; ++d)
		{
			if (InRect(Pos, DrawerPos(d) - FVector2f(10.0f, 4.0f), DrawerSize + FVector2f(20.0f, 8.0f)))
			{
				return d;
			}
		}
		return -1;
	}

	int32 DrawerOfKind(int32 Kind) const
	{
		for (int32 d = 0; d < 4; ++d)
		{
			if (DrawerKind[d] == Kind)
			{
				return d;
			}
		}
		return 0;
	}

	void Spawn()
	{
		Cur = Filed;
		ArriveT = 0.0f;
		SinceSpawn = 0.0f;
		DocPos = PileTop();
		bHeld = false;
		bBack = false;
		Sound(TEXT("S_Chore_Paper"), 0.6f, 1.0f + 0.04f * Filed);
	}

	void TickSort(float Dt)
	{
		SinceSpawn += Dt;
		RattleT = FMath::Max(0.0f, RattleT - Dt);
		if (Cur >= 0)
		{
			ArriveT += Dt;
			if (FileT >= 0.0f)
			{
				FileT += Dt;
				if (FileT >= 0.45f)
				{
					FileT = -1.0f;
					Cur = -1;
					++Filed;
					if (Filed >= NumDocs && DoneT < 0.0f)
					{
						DoneT = 0.0f;
						Nice(FVector2f(250.0f, 150.0f), TEXT("All filed!"), TEXT("S_Chore_Stamp"), 1.0f);
					}
				}
			}
			else if (bHeld)
			{
				DocPos = Mouse + GrabOff;
			}
			else if (ArriveT < 0.35f)
			{
				DocPos = FMath::Lerp(FVector2f(42.0f, 330.0f), TrayC, EaseOutCubic(ArriveT / 0.35f));
			}
			else if (bBack)
			{
				DocPos = FMath::Lerp(DocPos, TrayC, 1.0f - FMath::Exp(-14.0f * Dt));
				if (FVector2f::Distance(DocPos, TrayC) < 1.0f)
				{
					DocPos = TrayC;
					bBack = false;
				}
			}
		}
		else if (Filed < NumDocs && SinceSpawn >= 0.9f && DoneT < 0.0f)
		{
			// Reports come in one at a time, at a clerk's pace.
			Spawn();
		}
	}

	FVector2f PathAt(float S) const
	{
		if (Path.Num() < 2)
		{
			return FVector2f::ZeroVector;
		}
		if (S <= 0.0f)
		{
			return Path[0];
		}
		for (int32 k = 0; k + 1 < Path.Num(); ++k)
		{
			if (PathLen[k + 1] >= S)
			{
				const float Seg = PathLen[k + 1] - PathLen[k];
				return FMath::Lerp(Path[k], Path[k + 1], Seg > 1e-4f ? (S - PathLen[k]) / Seg : 0.0f);
			}
		}
		return Path.Last();
	}

	float Nearest(const FVector2f& Pos, float S0, float S1, float& OutS) const
	{
		float Best = 1e9f;
		OutS = S0;
		for (int32 k = 0; k + 1 < Path.Num(); ++k)
		{
			if (PathLen[k + 1] < S0 || PathLen[k] > S1)
			{
				continue;
			}
			const FVector2f Seg0 = Path[k];
			const FVector2f AB = Path[k + 1] - Seg0;
			const float L2 = AB.SizeSquared();
			const float T = L2 > 1e-4f ? FMath::Clamp(FVector2f::DotProduct(Pos - Seg0, AB) / L2, 0.0f, 1.0f) : 0.0f;
			const float D = FVector2f::Distance(Pos, Seg0 + AB * T);
			if (D < Best)
			{
				Best = D;
				OutS = FMath::Lerp(PathLen[k], PathLen[k + 1], T);
			}
		}
		return Best;
	}

	void BreakStroke(bool bTooFast)
	{
		GhostS = InkS;
		GhostT = 0.8f;
		if (InkS > 4.0f)
		{
			Blots.Add(PathAt(InkS));
			if (Blots.Num() > 4)
			{
				Blots.RemoveAt(0);
			}
		}
		InkS = 0.0f;
		InkTarget = 0.0f;
		bStroke = false;
		const FVector2f At(FMath::Clamp(Mouse.X, 150.0f, 490.0f), FMath::Clamp(Mouse.Y - 50.0f, 40.0f, 360.0f));
		Oops(At, bTooFast ? TEXT("Too fast - the ink skipped!") : TEXT("Off the line - start again"), TEXT("S_UI_Bad"), 4.0f);
	}

	void TickSign(float Dt)
	{
		GhostT = FMath::Max(0.0f, GhostT - Dt);
		if (bStroke && DoneT < 0.0f && Path.Num() > 1)
		{
			const float Prev = InkS;
			InkS = FMath::Min(InkTarget, InkS + SignSpeed * Dt);
			ScratchAcc += InkS - Prev;
			if (ScratchAcc > 70.0f)
			{
				ScratchAcc = 0.0f;
				Sound(TEXT("S_Chore_Scrape"), 0.3f, 1.25f + 0.2f * Rng.FRand());
			}
			if (InkS >= PathLen.Last() - 1.0f)
			{
				InkS = PathLen.Last();
				bStroke = false;
				DoneT = 0.0f;
				Nice(PathAt(InkS) + FVector2f(-40.0f, -50.0f), TEXT("Signed!"), TEXT("S_Chore_Stamp"), 1.0f);
			}
		}
	}

	void PaintDoc(FKGMgPainter& P, const FVector2f& Ctr, float Scale, int32 Kind, float LiftAmt, int32 Seed) const
	{
		const FVector2f Sz = DocSize * Scale;
		const FVector2f Pos = Ctr - Sz * 0.5f;
		KGMgVillage::Sheet(P, Pos, Sz, Paper, LiftAmt);
		P.Tri(Pos + FVector2f(1.0f, 1.0f), Pos + FVector2f(28.0f * Scale, 1.0f), Pos + FVector2f(1.0f, 28.0f * Scale), SealColor(Kind));
		const FLinearColor InkC = C(0x3A2B42);
		if (Scale > 0.8f)
		{
			P.Text(FVector2f(Ctr.X + 8.0f * Scale, Pos.Y + 8.0f * Scale), TEXT("REPORT"), 11.0f, InkC, 0.5f, TEXT("Black"), 40);
		}
		KGMgVillage::Scribble(P, Pos + FVector2f(14.0f, 34.0f) * Scale, Sz.X - 28.0f * Scale, 5, 11.0f * Scale, Seed, A(InkC, 0.5f), 2.0f * Scale);
		KGMgVillage::WaxSeal(P, Ctr + FVector2f(22.0f, 36.0f) * Scale, 17.0f * Scale, SealColor(Kind), Kind);
	}

	void PaintDrawer(FKGMgPainter& P, int32 d) const
	{
		const FVector2f Pos = DrawerPos(d);
		const float Open = (FileDrawer == d && FileT >= 0.0f) ? FMath::Sin(PI * Saturate(FileT / 0.45f)) : 0.0f;
		const float Shake = RattleDrawer == d ? FMath::Sin(Time * 70.0f) * 4.0f * (RattleT / 0.4f) : 0.0f;
		const FVector2f O = Pos + FVector2f(Shake, Open * 5.0f);
		if (Open > 0.0f)
		{
			P.Rect(Pos + FVector2f(4.0f, 0.0f), FVector2f(DrawerSize.X - 8.0f, Open * 14.0f), C(0x2A1A10));
		}
		P.Shadow(O, DrawerSize, 5.0f, 0.25f * (1.0f + Open), 2.0f + Open * 6.0f);
		P.RoundRect(O, DrawerSize, Mix(Wood, WoodLight, 0.25f), 5.0f, WoodDark, 2.0f);
		P.RoundRect(O + FVector2f(8.0f, 8.0f), DrawerSize - FVector2f(16.0f, 16.0f), FLinearColor::Transparent, 4.0f, A(WoodDark, 0.6f), 1.5f);
		P.Rect(O + FVector2f(6.0f, 3.0f), FVector2f(DrawerSize.X - 12.0f, 2.0f), A(FLinearColor::White, 0.2f));
		// Brass tag: seal colour + symbol + name.
		const int32 K = DrawerKind[d];
		const FVector2f TagPos = O + FVector2f(34.0f, 12.0f);
		P.RoundRect(TagPos, FVector2f(120.0f, 28.0f), C(0xE2C074), 4.0f, C(0x8C6A2A), 1.5f);
		P.Circle(TagPos + FVector2f(16.0f, 14.0f), 10.0f, SealColor(K), Mix(SealColor(K), Ink, 0.35f), 1.5f);
		KGMgVillage::Symbol(P, K, TagPos + FVector2f(16.0f, 14.0f), 5.0f, Cream);
		P.Text(TagPos + FVector2f(72.0f, 5.0f), Label(K), 12.0f, C(0x3A2B42), 0.5f, TEXT("Black"), 30);
		// Handle.
		P.Bar(O + FVector2f(64.0f, 58.0f), O + FVector2f(124.0f, 58.0f), 8.0f, Iron);
		P.Bar(O + FVector2f(66.0f, 56.0f), O + FVector2f(122.0f, 56.0f), 2.0f, A(FLinearColor::White, 0.3f));
		P.Circle(O + FVector2f(64.0f, 58.0f), 6.0f, C(0x2E3138));
		P.Circle(O + FVector2f(124.0f, 58.0f), 6.0f, C(0x2E3138));
		if (bHeld && InRect(Mouse, Pos - FVector2f(10.0f, 4.0f), DrawerSize + FVector2f(20.0f, 8.0f)))
		{
			P.RoundRect(O - FVector2f(3.0f, 3.0f), DrawerSize + FVector2f(6.0f, 6.0f), FLinearColor::Transparent, 7.0f, A(Cream, 0.95f), 3.0f);
		}
	}

	void PaintSort(FKGMgPainter& P) const
	{
		// Town hall office: striped wallpaper, wainscot, window over the harbour, a painting.
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, 240.0f), C(0xD9BF94), C(0xC7A77A));
		for (int32 i = 0; i < 22; ++i)
		{
			P.Rect(FVector2f(i * 30.0f + 12.0f, 0.0f), FVector2f(8.0f, 172.0f), A(C(0xB8955F), 0.25f));
		}
		P.Rect(FVector2f(0.0f, 172.0f), FVector2f(W, 68.0f), C(0x7A4E30));
		P.Rect(FVector2f(0.0f, 172.0f), FVector2f(W, 6.0f), WoodLight);
		for (int32 i = 0; i < 7; ++i)
		{
			P.RoundRect(FVector2f(12.0f + i * 96.0f, 186.0f), FVector2f(80.0f, 40.0f), A(C(0x6A4028), 0.6f), 4.0f, A(C(0x4E2E1C), 0.8f), 2.0f);
		}
		P.RoundRect(FVector2f(34.0f, 22.0f), FVector2f(140.0f, 118.0f), WoodDark, 6.0f);
		P.RectV(FVector2f(42.0f, 30.0f), FVector2f(124.0f, 102.0f), C(0x8FD3F4), C(0xDFF1F6));
		P.RectV(FVector2f(42.0f, 98.0f), FVector2f(124.0f, 34.0f), Water, WaterDeep);
		P.Tri(FVector2f(120.0f, 96.0f), FVector2f(138.0f, 96.0f), FVector2f(128.0f, 70.0f), Cream);
		P.Quad(FVector2f(112.0f, 96.0f), FVector2f(146.0f, 96.0f), FVector2f(140.0f, 104.0f), FVector2f(118.0f, 104.0f), WoodDark);
		P.Circle(FVector2f(70.0f, 52.0f), 10.0f, A(FLinearColor::White, 0.9f));
		P.Circle(FVector2f(84.0f, 50.0f), 8.0f, A(FLinearColor::White, 0.9f));
		P.Rect(FVector2f(102.0f, 30.0f), FVector2f(4.0f, 102.0f), WoodDark);
		P.Rect(FVector2f(42.0f, 78.0f), FVector2f(124.0f, 4.0f), WoodDark);
		P.Quad(FVector2f(26.0f, 16.0f), FVector2f(62.0f, 16.0f), FVector2f(50.0f, 146.0f), FVector2f(22.0f, 146.0f), C(0xA3203A));
		P.Quad(FVector2f(146.0f, 16.0f), FVector2f(182.0f, 16.0f), FVector2f(186.0f, 146.0f), FVector2f(158.0f, 146.0f), C(0xA3203A));
		P.Rect(FVector2f(20.0f, 12.0f), FVector2f(168.0f, 6.0f), WoodDark);
		// Painting: the lighthouse.
		P.Shadow(FVector2f(228.0f, 30.0f), FVector2f(124.0f, 96.0f), 4.0f, 0.3f, 4.0f);
		P.RoundRect(FVector2f(228.0f, 30.0f), FVector2f(124.0f, 96.0f), C(0xC9A04A), 4.0f, C(0x8C6A2A), 2.0f);
		P.RectV(FVector2f(238.0f, 40.0f), FVector2f(104.0f, 76.0f), C(0xF2A66B), C(0xFFE0B0));
		P.Rect(FVector2f(238.0f, 96.0f), FVector2f(104.0f, 20.0f), WaterDeep);
		P.Quad(FVector2f(302.0f, 96.0f), FVector2f(318.0f, 96.0f), FVector2f(314.0f, 58.0f), FVector2f(306.0f, 58.0f), Cream);
		P.Rect(FVector2f(303.0f, 72.0f), FVector2f(14.0f, 6.0f), Crimson);
		P.Glow(FVector2f(310.0f, 54.0f), 22.0f, A(Gold, 0.8f));
		P.Tag(FVector2f(290.0f, 150.0f), FString::Printf(TEXT("FILED  %d / %d"), Filed, NumDocs), A(Ink, 0.8f), Cream, 14.0f);

		// Desk.
		KGMgVillage::Planks(P, FVector2f(0.0f, 240.0f), FVector2f(W, H - 240.0f), C(0x9A6640), 4, false);
		P.Rect(FVector2f(0.0f, 240.0f), FVector2f(W, 4.0f), WoodLight);
		// Ink pot + stamp on the desk.
		P.Disc(FVector2f(318.0f, 372.0f), FVector2f(26.0f, 7.0f), A(Ink, 0.3f), A(Ink, 0.0f));
		P.RoundRect(FVector2f(298.0f, 336.0f), FVector2f(40.0f, 36.0f), C(0x2A2A3A), 8.0f);
		P.Rect(FVector2f(310.0f, 328.0f), FVector2f(16.0f, 10.0f), C(0x2A2A3A));
		P.Rect(FVector2f(304.0f, 344.0f), FVector2f(4.0f, 20.0f), A(FLinearColor::White, 0.25f));
		P.RoundRect(FVector2f(350.0f, 330.0f), FVector2f(22.0f, 28.0f), WoodLight, 4.0f, WoodDark, 1.5f);
		P.RoundRect(FVector2f(344.0f, 356.0f), FVector2f(34.0f, 12.0f), Crimson, 3.0f);

		// Pile of incoming reports + the tray.
		const int32 Left = FMath::Max(0, NumDocs - Filed - (Cur >= 0 ? 1 : 0));
		for (int32 k = 0; k < Left; ++k)
		{
			const float Y = 340.0f - k * 4.0f;
			P.Rect(FVector2f(12.0f + (k % 2) * 2.0f, Y), FVector2f(60.0f, 6.0f), k % 2 ? Paper : C(0xE8D8B0));
			P.Rect(FVector2f(12.0f + (k % 2) * 2.0f, Y + 5.0f), FVector2f(60.0f, 1.0f), A(Ink, 0.3f));
		}
		P.Text(FVector2f(42.0f, 350.0f), FString::Printf(TEXT("%d LEFT"), Left), 10.0f, Cream, 0.5f, TEXT("Black"), 60);
		P.Shadow(FVector2f(86.0f, 222.0f), FVector2f(128.0f, 150.0f), 8.0f, 0.3f, 5.0f);
		P.RoundRect(FVector2f(86.0f, 222.0f), FVector2f(128.0f, 150.0f), C(0x6E4A30), 8.0f, WoodDark, 2.0f);
		P.RoundRect(FVector2f(94.0f, 230.0f), FVector2f(112.0f, 134.0f), C(0x8A5E3E), 5.0f);
		P.Text(FVector2f(150.0f, 374.0f), TEXT("IN"), 12.0f, A(Cream, 0.8f), 0.5f, TEXT("Black"), 100);

		// Cabinet.
		P.Shadow(FVector2f(400.0f, 20.0f), FVector2f(220.0f, 384.0f), 8.0f, 0.35f, 6.0f);
		P.RoundRect(FVector2f(400.0f, 20.0f), FVector2f(220.0f, 386.0f), C(0x6E4428), 6.0f, WoodDark, 3.0f);
		P.Rect(FVector2f(394.0f, 12.0f), FVector2f(232.0f, 14.0f), WoodDark);
		P.Rect(FVector2f(394.0f, 12.0f), FVector2f(232.0f, 3.0f), A(WoodLight, 0.7f));
		P.Rect(FVector2f(606.0f, 28.0f), FVector2f(12.0f, 372.0f), A(Ink, 0.2f));
		for (int32 d = 0; d < 4; ++d)
		{
			PaintDrawer(P, d);
		}

		// The report on the desk (or flying into a drawer).
		if (Cur >= 0 && Docs.IsValidIndex(Cur))
		{
			if (FileT >= 0.0f)
			{
				const float T = Saturate(FileT / 0.3f);
				if (T < 1.0f)
				{
					const FVector2f To = DrawerPos(FileDrawer) + FVector2f(DrawerSize.X * 0.5f, 6.0f);
					PaintDoc(P, FMath::Lerp(FileFrom, To, EaseOutCubic(T)), FMath::Lerp(1.0f, 0.25f, T), Docs[Cur], 0.5f, Cur + 5);
				}
			}
			else
			{
				PaintDoc(P, DocPos, bHeld ? 1.06f : 1.0f, Docs[Cur], bHeld ? 1.0f : 0.0f, Cur + 5);
				if (Filed == 0 && !bHeld && !bBack && ArriveT > 0.35f)
				{
					const int32 D = DrawerOfKind(Docs[Cur]);
					P.HintRing(DocPos, 52.0f, Time, Gold);
					P.HintArrow(DocPos + FVector2f(62.0f, -14.0f), DrawerPos(D) + FVector2f(-8.0f, 26.0f), Time, Gold);
				}
			}
		}
	}

	void PaintInk(FKGMgPainter& P, float UpTo, const FLinearColor& Color, float Thick) const
	{
		if (UpTo <= 0.5f || Path.Num() < 2)
		{
			return;
		}
		TArray<FVector2f> Pts;
		for (int32 k = 0; k < Path.Num() && PathLen[k] < UpTo; ++k)
		{
			Pts.Add(Path[k]);
		}
		Pts.Add(PathAt(UpTo));
		P.Line(Pts, Color, Thick);
	}

	void PaintSign(FKGMgPainter& P) const
	{
		KGMgVillage::Planks(P, FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0x9A6640), 6, false);
		// Props: ink pot, a candle stub, a blotter.
		P.Disc(FVector2f(578.0f, 118.0f), FVector2f(34.0f, 30.0f), A(Ink, 0.3f), A(Ink, 0.0f));
		P.Circle(FVector2f(574.0f, 110.0f), 26.0f, C(0x2A2A3A));
		P.Circle(FVector2f(574.0f, 110.0f), 13.0f, C(0x10101A));
		P.Circle(FVector2f(566.0f, 100.0f), 5.0f, A(FLinearColor::White, 0.3f));
		P.Glow(FVector2f(574.0f, 290.0f), 90.0f, A(C(0xFFD27A), 0.25f));
		P.Circle(FVector2f(574.0f, 300.0f), 26.0f, C(0xC9A04A), C(0x8C6A2A), 2.0f);
		P.Circle(FVector2f(574.0f, 300.0f), 13.0f, C(0xF4E9D0));
		KGMgVillage::Flame(P, FVector2f(574.0f, 298.0f), 18.0f, 1.5f * FMath::Sin(Time * 9.0f), 1.0f);
		P.RoundRect(FVector2f(20.0f, 60.0f), FVector2f(54.0f, 280.0f), C(0x3F6E5A), 6.0f, C(0x2B4A3C), 2.0f);

		// The town record.
		const FVector2f SPos(92.0f, 16.0f);
		const FVector2f SSize(420.0f, 368.0f);
		KGMgVillage::Sheet(P, SPos, SSize, Paper, 0.0f);
		P.RoundRect(SPos + FVector2f(10.0f, 10.0f), SSize - FVector2f(20.0f, 20.0f), FLinearColor::Transparent, 2.0f, A(C(0xB89A6A), 0.5f), 1.5f);
		const FLinearColor InkC = C(0x3A2B42);
		P.Text(FVector2f(302.0f, 28.0f), TEXT("MORROWMERE TOWN RECORD"), 15.0f, InkC, 0.5f, TEXT("Black"), 40);
		P.Bar(FVector2f(182.0f, 54.0f), FVector2f(422.0f, 54.0f), 2.0f, A(Crimson, 0.7f));
		P.Text(FVector2f(302.0f, 60.0f), TEXT("Reports filed today"), 12.0f, A(InkC, 0.8f), 0.5f, TEXT("Medium"));
		for (int32 r = 0; r < 4; ++r)
		{
			const float Y = 90.0f + r * 24.0f;
			P.Circle(FVector2f(146.0f, Y + 8.0f), 8.0f, SealColor(r));
			KGMgVillage::Symbol(P, r, FVector2f(146.0f, Y + 8.0f), 4.0f, Cream);
			const FString Name = FString(Label(r)).Left(1) + FString(Label(r)).RightChop(1).ToLower();
			P.Text(FVector2f(162.0f, Y), Name, 13.0f, InkC, 0.0f, TEXT("Bold"));
			for (float X = 250.0f; X < 430.0f; X += 9.0f)
			{
				P.Rect(FVector2f(X, Y + 12.0f), FVector2f(3.0f, 2.0f), A(InkC, 0.35f));
			}
			P.Text(FVector2f(468.0f, Y), TEXT("x 2"), 13.0f, InkC, 1.0f, TEXT("Bold"));
		}
		for (float X = 130.0f; X < 490.0f; X += 12.0f)
		{
			P.Rect(FVector2f(X, 304.0f), FVector2f(7.0f, 2.0f), A(InkC, 0.3f));
		}
		P.Text(FVector2f(130.0f, 310.0f), TEXT("Witnessed and signed, the town clerk"), 11.0f, A(InkC, 0.7f), 0.0f, TEXT("Medium"));

		if (Path.Num() > 1)
		{
			const float L = PathLen.Last();
			const FLinearColor InkBlue = C(0x1E2A4A);
			// Dotted guide ahead of the ink.
			for (float S = 0.0f; S < L; S += 11.0f)
			{
				if (S > InkS)
				{
					P.Circle(PathAt(S), 2.3f, A(InkC, 0.35f));
				}
			}
			P.Circle(Path[0], 6.0f, InkS > 0.5f ? A(InkBlue, 0.9f) : Good);
			P.Circle(Path.Last(), 7.0f, FLinearColor::Transparent, A(Crimson, 0.7f), 2.0f);
			for (const FVector2f& B : Blots)
			{
				P.Circle(B, 7.0f, A(InkBlue, 0.8f));
				P.Circle(B + FVector2f(8.0f, 4.0f), 3.0f, A(InkBlue, 0.8f));
				P.Circle(B + FVector2f(-6.0f, 6.0f), 2.0f, A(InkBlue, 0.8f));
			}
			if (GhostT > 0.0f)
			{
				PaintInk(P, GhostS, A(InkBlue, 0.4f * GhostT / 0.8f), 4.0f);
			}
			PaintInk(P, InkS, InkBlue, 4.0f);
			// Progress under the line.
			P.Gauge(FVector2f(130.0f, 338.0f), FVector2f(300.0f, 12.0f), InkS / FMath::Max(1.0f, L), C(0x3F5A9E));

			if (DoneT >= 0.0f)
			{
				const float S = EaseOutBack(Saturate(DoneT / 0.3f));
				KGMgVillage::WaxSeal(P, FVector2f(478.0f, 342.0f), 22.0f * FMath::Max(0.05f, S), Crimson, 3);
			}
			else if (!bStroke)
			{
				P.HintRing(PathAt(InkS), 22.0f, Time, Gold);
				if (InkS < 0.5f)
				{
					P.Text(Path[0] + FVector2f(-10.0f, -42.0f), TEXT("START"), 11.0f, C(0x3F7A2A), 0.5f, TEXT("Black"), 60);
					P.HintArrow(Path[0] + FVector2f(-60.0f, -30.0f), Path[0] + FVector2f(-12.0f, -6.0f), Time, Gold);
				}
			}
			// The quill: on the ink tip while writing, following the hand otherwise.
			KGMgVillage::Quill(P, bStroke ? PathAt(InkS) : Mouse + FVector2f(3.0f, -5.0f));
		}
	}
};

// =====================================================================================================================
// LightCandles: watch which candles flicker (and in what order), then light them in the same order with the taper.
// =====================================================================================================================
class FKGMgLightCandles final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		if (Phase == EPhase::Play)
		{
			return TEXT("Light the candles in the same order");
		}
		if (Phase == EPhase::Done)
		{
			return TEXT("Lovely - the altar glows");
		}
		return TEXT("Watch the order the candles flicker");
	}

	virtual void BeginStage() override
	{
		NumCandles = Stage == 0 ? 5 : 6;
		SeqLen = Stage == 0 ? 4 : 5;
		const float Spacing = Stage == 0 ? 82.0f : 70.0f;
		TArray<int32> Order;
		for (int32 i = 0; i < NumCandles; ++i)
		{
			CandleX[i] = 320.0f + (float(i) - float(NumCandles - 1) * 0.5f) * Spacing;
			CandleH[i] = 58.0f + Rng.FRand() * 52.0f;
			Lit[i] = false;
			WasLit[i] = false;
			LitAge[i] = 0.0f;
			Order.Add(i);
		}
		Rng.Shuffle(Order);
		Seq.Reset();
		for (int32 k = 0; k < SeqLen; ++k)
		{
			Seq.Add(Order[k]);
		}
		Phase = EPhase::Watch;
		PhaseT = -0.7f;
		Shown = -1;
		Input = 0;
		ClickLock = 0.0f;
		ExtraLit = 0;
		AutoT = 0.0f;
		Smoke.Reset();
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Phase != EPhase::Play)
		{
			return;
		}
		for (int32 i = 0; i < NumCandles; ++i)
		{
			const FVector2f Wick = WickTop(i);
			if (InRect(Pos, FVector2f(CandleX[i] - 30.0f, Wick.Y - 44.0f), FVector2f(60.0f, CandleH[i] + 64.0f)))
			{
				ClickCandle(i);
				return;
			}
		}
	}

	virtual void Tick(float Dt) override
	{
		ClickLock = FMath::Max(0.0f, ClickLock - Dt);
		for (int32 i = 0; i < NumCandles; ++i)
		{
			if (Lit[i])
			{
				LitAge[i] += Dt;
			}
		}
		for (int32 k = Smoke.Num() - 1; k >= 0; --k)
		{
			Smoke[k].Age += Dt;
			Smoke[k].Pos += FVector2f(10.0f * FMath::Sin(Smoke[k].Age * 5.0f + k), -34.0f) * Dt;
			if (Smoke[k].Age > 1.4f)
			{
				Smoke.RemoveAt(k);
			}
		}
		PhaseT += Dt;
		switch (Phase)
		{
		case EPhase::Watch:
		{
			const int32 Beat = PhaseT < 0.0f ? -1 : FMath::FloorToInt(PhaseT / StepTime);
			if (Beat >= 0 && Beat < SeqLen && Beat != Shown && FMath::Fmod(PhaseT, StepTime) < 0.45f)
			{
				Shown = Beat;
				Sound(TEXT("S_Chore_Flame"), 0.8f, NotePitch(Seq[Beat]));
			}
			if (PhaseT >= SeqLen * StepTime - 0.1f)
			{
				Phase = EPhase::Play;
				PhaseT = 0.0f;
				Input = 0;
				ClickLock = 0.0f;
			}
			break;
		}
		case EPhase::Gust:
			if (PhaseT >= 1.3f)
			{
				Phase = EPhase::Watch;
				PhaseT = -0.4f;
				Shown = -1;
				Input = 0;
			}
			break;
		case EPhase::Done:
		{
			// The remaining candles catch one after another.
			if (PhaseT >= 0.15f * (ExtraLit + 1))
			{
				for (int32 i = 0; i < NumCandles; ++i)
				{
					if (!Lit[i])
					{
						Lit[i] = true;
						LitAge[i] = 0.0f;
						++ExtraLit;
						Sound(TEXT("S_Chore_Flame"), 0.5f, NotePitch(i));
						break;
					}
				}
			}
			const float MinTime = Stage == 0 ? 3.7f : 4.6f;
			if (PhaseT >= 0.6f && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
			break;
		}
		default:
			break;
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Phase != EPhase::Play || Input >= SeqLen)
		{
			return;
		}
		AutoT += Dt;
		const int32 Next = Seq[Input];
		Mouse = FMath::Lerp(Mouse, WickTop(Next) + FVector2f(0.0f, -8.0f), Saturate(Dt * 10.0f));
		if (AutoT > 0.4f && ClickLock <= 0.0f)
		{
			AutoT = 0.0f;
			ClickCandle(Next);
		}
	}

	virtual void DebugPose() override
	{
		Phase = EPhase::Play;
		PhaseT = 1.0f;
		for (int32 i = 0; i < NumCandles; ++i)
		{
			Lit[i] = false;
		}
		Input = 2;
		Lit[Seq[0]] = true;
		Lit[Seq[1]] = true;
		LitAge[Seq[0]] = 2.0f;
		LitAge[Seq[1]] = 1.0f;
		Mouse = WickTop(Seq[2]) + FVector2f(8.0f, -10.0f);
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		// Nave: stone walls, stained glass, light shafts.
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0x5A4E66), C(0x2E2638));
		for (int32 Row = 0; Row < 12; ++Row)
		{
			for (int32 Col = 0; Col < 11; ++Col)
			{
				const float X = Col * 62.0f - (Row % 2) * 31.0f;
				P.RoundRect(FVector2f(X + 2.0f, Row * 30.0f + 2.0f), FVector2f(58.0f, 26.0f),
				            A(C(0x6A5E78), 0.45f + 0.1f * ((Row * 7 + Col) % 3)), 4.0f);
			}
		}
		Window(P, 320.0f, 22.0f, 140.0f, 196.0f, Time, 0);
		Window(P, 96.0f, 40.0f, 64.0f, 130.0f, Time, 3);
		Window(P, 544.0f, 40.0f, 64.0f, 130.0f, Time, 5);
		P.Quad(FVector2f(252.0f, 218.0f), FVector2f(388.0f, 218.0f), FVector2f(470.0f, AltarY), FVector2f(170.0f, AltarY), A(C(0xFFE9B0), 0.08f));
		P.Quad(FVector2f(66.0f, 170.0f), FVector2f(126.0f, 170.0f), FVector2f(190.0f, 330.0f), FVector2f(110.0f, 330.0f), A(C(0xFFE9B0), 0.06f));
		P.Quad(FVector2f(514.0f, 170.0f), FVector2f(574.0f, 170.0f), FVector2f(530.0f, 330.0f), FVector2f(450.0f, 330.0f), A(C(0xFFE9B0), 0.06f));

		// Floor + altar.
		P.RectV(FVector2f(0.0f, 330.0f), FVector2f(W, 70.0f), C(0x3A3244), C(0x221C2A));
		for (int32 i = 0; i < 12; ++i)
		{
			P.Segment(FVector2f(i * 60.0f, 330.0f), FVector2f(i * 60.0f - 30.0f, 400.0f), A(Ink, 0.3f), 2.0f);
		}
		P.Shadow(FVector2f(104.0f, AltarY), FVector2f(432.0f, 100.0f), 4.0f, 0.4f, 6.0f);
		P.Rect(FVector2f(104.0f, AltarY), FVector2f(432.0f, 14.0f), C(0xB8B0C0));
		P.Rect(FVector2f(104.0f, AltarY), FVector2f(432.0f, 3.0f), A(FLinearColor::White, 0.4f));
		P.RectV(FVector2f(118.0f, AltarY + 14.0f), FVector2f(404.0f, H - AltarY - 14.0f), Crimson, C(0x7A0A1E));
		P.Rect(FVector2f(118.0f, AltarY + 14.0f), FVector2f(404.0f, 8.0f), Gold);
		P.Rect(FVector2f(150.0f, AltarY + 22.0f), FVector2f(6.0f, 80.0f), A(Gold, 0.8f));
		P.Rect(FVector2f(484.0f, AltarY + 22.0f), FVector2f(6.0f, 80.0f), A(Gold, 0.8f));
		P.Circle(FVector2f(320.0f, 360.0f), 20.0f, FLinearColor::Transparent, Gold, 3.0f);
		P.Bar(FVector2f(320.0f, 346.0f), FVector2f(320.0f, 374.0f), 4.0f, Gold);
		P.Bar(FVector2f(310.0f, 355.0f), FVector2f(330.0f, 355.0f), 4.0f, Gold);

		for (int32 i = 0; i < NumCandles; ++i)
		{
			PaintCandle(P, i);
		}
		for (const FWisp& S : Smoke)
		{
			P.Circle(S.Pos, 4.0f + S.Age * 7.0f, A(C(0xC8C0D0), 0.5f * (1.0f - S.Age / 1.4f)));
		}
		if (Phase == EPhase::Gust && PhaseT < 0.8f)
		{
			for (int32 k = 0; k < 7; ++k)
			{
				const float X = -160.0f + PhaseT * 1000.0f + KGMgVillage::Hash01(k) * 120.0f;
				const float Y = 120.0f + k * 26.0f;
				P.Bar(FVector2f(X, Y), FVector2f(X + 90.0f, Y - 6.0f), 3.0f, A(FLinearColor::White, 0.4f * (1.0f - PhaseT / 0.8f)));
			}
		}

		// The priest's taper.
		const bool bActive = Phase == EPhase::Play || Phase == EPhase::Done;
		const FVector2f Tip = bActive ? FVector2f(FMath::Clamp(Mouse.X, 0.0f, W), FMath::Clamp(Mouse.Y, 0.0f, H)) : FVector2f(578.0f, 318.0f);
		const FVector2f Butt = Tip + FVector2f(70.0f, 120.0f);
		const FVector2f TD = (Tip - Butt).GetSafeNormal();
		P.Bar(Butt + FVector2f(5.0f, 7.0f), Tip + FVector2f(5.0f, 7.0f), 7.0f, A(Ink, 0.25f));
		P.Bar(Butt, Tip, 6.0f, C(0xF4E9D0));
		P.Bar(Butt, Butt + TD * 34.0f, 10.0f, C(0xC9A04A));
		KGMgVillage::Flame(P, Tip, 15.0f, 1.5f * FMath::Sin(Time * 11.0f), 1.0f);

		// Status + sequence pips.
		FString Label;
		FLinearColor Col = Gold;
		switch (Phase)
		{
		case EPhase::Watch: Label = TEXT("WATCH..."); break;
		case EPhase::Play: Label = FString::Printf(TEXT("YOUR TURN  %d / %d"), Input, SeqLen); Col = Good; break;
		case EPhase::Gust: Label = TEXT("BLOWN OUT!"); Col = C(0xFF8A80); break;
		default: Label = TEXT("ALL LIT"); Col = Good; break;
		}
		// On the altar cloth, clear of the candles.
		P.Tag(FVector2f(206.0f, 372.0f), Label, A(Ink, 0.85f), Col, 14.0f);
		const int32 Beat = (Phase == EPhase::Watch && PhaseT >= 0.0f) ? FMath::FloorToInt(PhaseT / StepTime) : -1;
		for (int32 k = 0; k < SeqLen; ++k)
		{
			const bool bOn = k < Input || Phase == EPhase::Done;
			const bool bNow = k == Beat;
			P.Circle(FVector2f(456.0f + (k - (SeqLen - 1) * 0.5f) * 22.0f, 372.0f), bNow ? 9.0f : 7.0f,
			         bOn ? Gold : bNow ? A(Cream, 0.9f) : A(Ink, 0.6f), A(Cream, 0.5f), 1.5f);
		}
	}

private:
	enum class EPhase : uint8
	{
		Watch,
		Play,
		Gust,
		Done
	};

	struct FWisp
	{
		FVector2f Pos;
		float Age = 0.0f;
	};

	static constexpr float StepTime = 0.6f;
	static constexpr float AltarY = 300.0f;
	EPhase Phase = EPhase::Watch;
	int32 NumCandles = 5;
	int32 SeqLen = 4;
	float CandleX[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float CandleH[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	bool Lit[6] = {false, false, false, false, false, false};
	bool WasLit[6] = {false, false, false, false, false, false};
	float LitAge[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	TArray<int32> Seq;
	float PhaseT = 0.0f;
	int32 Shown = -1;
	int32 Input = 0;
	int32 ExtraLit = 0;
	float ClickLock = 0.0f;
	float AutoT = 0.0f;
	TArray<FWisp> Smoke;

	static float NotePitch(int32 i) { return 0.8f + 0.09f * float(i); }

	FVector2f WickTop(int32 i) const { return FVector2f(CandleX[i], AltarY - 12.0f - CandleH[i] - 7.0f); }

	float FlashOf(int32 i) const
	{
		if (Phase != EPhase::Watch || PhaseT < 0.0f)
		{
			return 0.0f;
		}
		const int32 Beat = FMath::FloorToInt(PhaseT / StepTime);
		if (Beat >= SeqLen || Seq[Beat] != i)
		{
			return 0.0f;
		}
		const float F = FMath::Fmod(PhaseT, StepTime);
		return F < 0.45f ? FMath::Sin(PI * F / 0.45f) : 0.0f;
	}

	void ClickCandle(int32 i)
	{
		if (Phase != EPhase::Play || ClickLock > 0.0f || Lit[i] || Input >= SeqLen)
		{
			return;
		}
		if (Seq[Input] == i)
		{
			Lit[i] = true;
			LitAge[i] = 0.0f;
			++Input;
			ClickLock = 0.3f;   // the taper needs a moment on each wick
			Nice(WickTop(i) + FVector2f(0.0f, -44.0f), TEXT(""), TEXT("S_Chore_Flame"), NotePitch(i));
			if (Input >= SeqLen)
			{
				Phase = EPhase::Done;
				PhaseT = 0.0f;
				ExtraLit = 0;
				Nice(FVector2f(320.0f, 150.0f), TEXT("Beautiful!"), TEXT("S_UI_Good"), 1.2f);
			}
		}
		else
		{
			// A draught blows every flame out; the order is shown again.
			Phase = EPhase::Gust;
			PhaseT = 0.0f;
			for (int32 j = 0; j < NumCandles; ++j)
			{
				WasLit[j] = Lit[j];
				if (Lit[j])
				{
					Smoke.Add({WickTop(j), 0.0f});
					Smoke.Add({WickTop(j) + FVector2f(3.0f, -8.0f), 0.2f});
				}
				Lit[j] = false;
			}
			Oops(FVector2f(320.0f, 150.0f), TEXT("A draught! Watch again"), TEXT("S_Chore_Whoosh"), 6.0f);
		}
	}

	static void Window(FKGMgPainter& P, float Cx, float Top, float Wd, float Ht, float Tm, int32 Seed)
	{
		static const uint32 Glass[] = {0x2F5DA8, 0xC8102E, 0xF2C230, 0x3FA34D, 0x7B4FB0, 0x3FA7D6};
		const float Rad = Wd * 0.5f;
		const FVector2f ArcC(Cx, Top + Rad);
		const float BodyH = Ht - Rad;
		const FLinearColor Frame = C(0x8C8398);
		const FLinearColor Lead = C(0x241C2C);
		P.Circle(ArcC, Rad + 10.0f, Frame);
		P.Rect(FVector2f(Cx - Rad - 10.0f, ArcC.Y), FVector2f(Wd + 20.0f, BodyH + 10.0f), Frame);
		const int32 ColsN = 3;
		const int32 RowsN = 4;
		for (int32 r = 0; r < RowsN; ++r)
		{
			for (int32 c = 0; c < ColsN; ++c)
			{
				const float Shim = 0.8f + 0.2f * FMath::Sin(Tm * 1.3f + r * 1.1f + c * 0.7f + Seed);
				P.Rect(FVector2f(Cx - Rad + c * Wd / ColsN, ArcC.Y + r * BodyH / RowsN), FVector2f(Wd / ColsN, BodyH / RowsN),
				       Mix(C(0x14101A), C(Glass[(Seed + r * 2 + c * 3) % 6]), Shim));
			}
		}
		for (int32 k = 0; k < 5; ++k)
		{
			P.ArcBand(ArcC, 0.0f, Rad, PI + k * PI / 5.0f, PI + (k + 1) * PI / 5.0f, C(Glass[(Seed + k * 5 + 1) % 6]), 8);
		}
		P.Circle(ArcC, Rad * 0.3f, Gold, Lead, 2.0f);
		P.Arc(ArcC, Rad, PI, 2.0f * PI, Lead, 4.0f, 24);
		for (int32 k = 1; k < 5; ++k)
		{
			P.Segment(ArcC + KGMgVillage::Polar(PI + k * PI / 5.0f) * Rad * 0.3f, ArcC + KGMgVillage::Polar(PI + k * PI / 5.0f) * Rad, Lead, 2.5f);
		}
		for (int32 c = 1; c < ColsN; ++c)
		{
			P.Bar(FVector2f(Cx - Rad + c * Wd / ColsN, ArcC.Y), FVector2f(Cx - Rad + c * Wd / ColsN, ArcC.Y + BodyH), 3.0f, Lead);
		}
		for (int32 r = 0; r <= RowsN; ++r)
		{
			P.Bar(FVector2f(Cx - Rad, ArcC.Y + r * BodyH / RowsN), FVector2f(Cx + Rad, ArcC.Y + r * BodyH / RowsN), 3.0f, Lead);
		}
		P.Glow(FVector2f(Cx, ArcC.Y + BodyH * 0.3f), Wd * 1.1f, A(C(0xFFE3A0), 0.16f));
	}

	void PaintCandle(FKGMgPainter& P, int32 i) const
	{
		const float X = CandleX[i];
		const float Hc = CandleH[i];
		const float Base = AltarY - 12.0f;
		const float TopY = Base - Hc;
		const float Flash = FlashOf(i);
		const FVector2f Wick = WickTop(i);
		if (Lit[i])
		{
			P.Glow(Wick + FVector2f(0.0f, -14.0f), 80.0f, A(C(0xFFC870), 0.3f));
		}
		if (Flash > 0.01f)
		{
			P.Glow(Wick + FVector2f(0.0f, -14.0f), 100.0f, A(C(0xFFE3A0), 0.45f * Flash));
		}
		// Brass holder.
		P.Disc(FVector2f(X, AltarY - 3.0f), FVector2f(24.0f, 7.0f), C(0xE8C46A), C(0x9C7A2E));
		P.Rect(FVector2f(X - 6.0f, Base), FVector2f(12.0f, 8.0f), C(0xB8923E));
		P.Disc(FVector2f(X, Base), FVector2f(19.0f, 5.0f), C(0xF0D080), C(0xA8842E));
		// Wax.
		const FLinearColor Wax = Mix(C(0xF4E9D0), C(0xFFF4C8), Flash);
		P.RoundRect(FVector2f(X - 13.0f, TopY), FVector2f(26.0f, Hc), Wax, 5.0f);
		P.Rect(FVector2f(X + 5.0f, TopY + 4.0f), FVector2f(6.0f, Hc - 6.0f), A(Ink, 0.1f));
		P.Rect(FVector2f(X - 9.0f, TopY + 6.0f), FVector2f(3.0f, Hc - 12.0f), A(FLinearColor::White, 0.45f));
		P.RoundRect(FVector2f(X - 11.0f, TopY), FVector2f(6.0f, 10.0f + 10.0f * KGMgVillage::Hash01(i * 5 + 1)), Wax, -1.0f);
		P.RoundRect(FVector2f(X + 4.0f, TopY), FVector2f(5.0f, 6.0f + 8.0f * KGMgVillage::Hash01(i * 9 + 2)), Wax, -1.0f);
		P.Disc(FVector2f(X, TopY + 2.0f), FVector2f(13.0f, 4.0f), C(0xFFF8E6), C(0xE6D6B4));
		P.Bar(FVector2f(X, TopY + 2.0f), Wick, 2.5f, C(0x2A2230));
		// Flame.
		if (Lit[i])
		{
			const float Pop = EaseOutBack(Saturate(LitAge[i] / 0.25f));
			KGMgVillage::Flame(P, Wick, 26.0f * Pop + 2.0f * FMath::Sin(Time * 13.0f + i), 2.5f * FMath::Sin(Time * 9.0f + i * 1.7f), 1.0f);
		}
		else if (Phase == EPhase::Gust && WasLit[i] && PhaseT < 0.3f)
		{
			const float T = PhaseT / 0.3f;
			KGMgVillage::Flame(P, Wick, 26.0f * (1.0f - T), 18.0f * T, 1.0f - T);
		}
		if (Flash > 0.01f)
		{
			KGMgVillage::Flame(P, Wick, 24.0f * Flash, 0.0f, 0.85f * Flash);
			P.Arc(Wick + FVector2f(0.0f, -12.0f), 30.0f + 6.0f * Flash, 0.0f, KGMgVillage::TwoPi, A(Gold, Flash), 3.0f, 32);
		}
		// Hover ring during play.
		if (Phase == EPhase::Play && !Lit[i] &&
		    InRect(Mouse, FVector2f(X - 30.0f, Wick.Y - 44.0f), FVector2f(60.0f, Hc + 64.0f)))
		{
			P.Arc(Wick + FVector2f(0.0f, -10.0f), 24.0f, 0.0f, KGMgVillage::TwoPi, A(Cream, 0.7f), 2.0f, 28);
		}
	}
};

// =====================================================================================================================
// TendGraves: pull the weeds straight up, then put flowers matching each headstone's ribbon in its vase.
// =====================================================================================================================
class FKGMgTendGraves final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Grab a weed and pull it straight up") : TEXT("Give each grave the flower on its ribbon");
	}

	virtual void BeginStage() override
	{
		if (!bWantSet)
		{
			// Kept for the whole chore so the ribbons don't change between stages.
			TArray<int32> Kinds = {0, 1, 2, 3};
			Rng.Shuffle(Kinds);
			for (int32 g = 0; g < 3; ++g)
			{
				Want[g] = Kinds[g];
			}
			bWantSet = true;
		}
		DoneT = -1.0f;
		AutoT = 0.0f;
		Grab = -1;
		StretchGoal = 0.0f;
		Crumbs.Reset();
		Pulled = 0;
		Weeds.Reset();
		if (Stage == 0)
		{
			int32 Per[3] = {3, 3, 3};
			Per[Rng.RandRange(0, 2)] = 2;
			for (int32 g = 0; g < 3; ++g)
			{
				for (int32 n = 0; n < Per[g]; ++n)
				{
					FVector2f Root = FVector2f::ZeroVector;
					for (int32 Try = 0; Try < 24; ++Try)
					{
						Root = FVector2f(GraveX[g] + (Rng.FRand() * 2.0f - 1.0f) * 60.0f, 284.0f + Rng.FRand() * 40.0f);
						bool bFree = true;
						for (const FWeed& Other : Weeds)
						{
							bFree &= FVector2f::Distance(Other.Root, Root) >= 44.0f;
						}
						if (bFree)
						{
							break;
						}
					}
					FWeed& Wd = Weeds.AddDefaulted_GetRef();
					Wd.Root = Root;
					Wd.Kind = Rng.RandRange(0, 2);
				}
			}
		}
		for (int32 g = 0; g < 3; ++g)
		{
			InVase[g] = 0;
			StemAge[g][0] = 10.0f;
			StemAge[g][1] = 10.0f;
		}
		Carry = -1;
		Falls.Reset();
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			int32 Best = -1;
			float BestD = 38.0f;
			for (int32 i = 0; i < Weeds.Num(); ++i)
			{
				if (Weeds[i].bOut)
				{
					continue;
				}
				const float D = FMath::Min(FVector2f::Distance(Pos, Head(i)), FVector2f::Distance(Pos, Weeds[i].Root + FVector2f(0.0f, -14.0f)));
				if (D < BestD)
				{
					BestD = D;
					Best = i;
				}
			}
			if (Best >= 0)
			{
				Grab = Best;
				GrabAt = Pos;
				StretchGoal = 0.0f;
				Sound(TEXT("S_UI_Click"), 0.4f, 0.9f);
			}
		}
		else if (DoneT < 0.0f)
		{
			int32 Best = -1;
			float BestD = 26.0f;
			for (int32 k = 0; k < 4; ++k)
			{
				const float D = FVector2f::Distance(Pos, BunchPos(k));
				if (D < BestD)
				{
					BestD = D;
					Best = k;
				}
			}
			if (Best >= 0)
			{
				Carry = Best;
				Sound(TEXT("S_Chore_Brush"), 0.4f, 1.3f);
			}
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			Grab = -1;
			return;
		}
		if (Carry < 0)
		{
			return;
		}
		const int32 Kind = Carry;
		Carry = -1;
		int32 G = -1;
		for (int32 g = 0; g < 3; ++g)
		{
			if (InRect(Pos, FVector2f(GraveX[g] - 64.0f, 140.0f), FVector2f(128.0f, 200.0f)))
			{
				G = g;
			}
		}
		if (G < 0)
		{
			return;
		}
		const FVector2f Top(GraveX[G], 286.0f);
		if (Want[G] != Kind)
		{
			Falls.Add({Pos, Kind, 0.0f});
			Oops(Top + FVector2f(0.0f, -110.0f), TEXT("Wrong flower - check the ribbon"), TEXT("S_UI_Bad"), 3.0f);
			return;
		}
		if (InVase[G] >= 2)
		{
			PopText(Top + FVector2f(0.0f, -110.0f), TEXT("This vase is full"), Cream);
			return;
		}
		StemAge[G][InVase[G]] = 0.0f;
		++InVase[G];
		const int32 Total = InVase[0] + InVase[1] + InVase[2];
		Nice(Top + FVector2f(0.0f, -100.0f), TEXT(""), TEXT("S_UI_Good"), 0.95f + 0.07f * Total);
		if (Total >= 6 && DoneT < 0.0f)
		{
			DoneT = 0.0f;
			Nice(FVector2f(385.0f, 110.0f), TEXT("Lovely."), TEXT("S_Chore_Pluck"), 1.2f);
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage != 0 || Grab < 0)
		{
			return;
		}
		const float Up = GrabAt.Y - Pos.Y;
		const float Side = FMath::Abs(Pos.X - GrabAt.X);
		if (Side > 48.0f)
		{
			Oops(Weeds[Grab].Root + FVector2f(0.0f, -80.0f), TEXT("Pull straight up!"), TEXT("S_UI_Bad"), 2.0f);
			Grab = -1;
			return;
		}
		StretchGoal = FMath::Clamp(Up, 0.0f, 110.0f) * 0.5f;
	}

	virtual void Tick(float Dt) override
	{
		for (int32 k = Crumbs.Num() - 1; k >= 0; --k)
		{
			Crumbs[k].Age += Dt;
			Crumbs[k].Vel.Y += 600.0f * Dt;
			Crumbs[k].Pos += Crumbs[k].Vel * Dt;
			if (Crumbs[k].Age > 0.9f)
			{
				Crumbs.RemoveAt(k);
			}
		}
		for (int32 k = Falls.Num() - 1; k >= 0; --k)
		{
			Falls[k].Age += Dt;
			if (Falls[k].Age > 0.6f)
			{
				Falls.RemoveAt(k);
			}
		}
		for (int32 g = 0; g < 3; ++g)
		{
			StemAge[g][0] += Dt;
			StemAge[g][1] += Dt;
		}
		if (Stage == 0)
		{
			TickWeeds(Dt);
		}
		if (DoneT >= 0.0f)
		{
			DoneT += Dt;
			const float MinTime = Stage == 0 ? 4.8f : 2.9f;
			if (DoneT >= 0.6f && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			if (Grab < 0)
			{
				for (int32 i = 0; i < Weeds.Num(); ++i)
				{
					if (!Weeds[i].bOut)
					{
						Grab = i;
						GrabAt = Head(i);
						break;
					}
				}
			}
			if (Grab >= 0)
			{
				StretchGoal = 50.0f;
				Mouse = Head(Grab);
			}
		}
		else if (DoneT < 0.0f)
		{
			AutoT += Dt;
			if (AutoT >= 0.45f)
			{
				AutoT = 0.0f;
				for (int32 g = 0; g < 3; ++g)
				{
					if (InVase[g] < 2)
					{
						Carry = Want[g];
						Mouse = FVector2f(GraveX[g], 250.0f);
						OnRelease(Mouse);
						break;
					}
				}
			}
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			for (int32 i = 0; i < Weeds.Num(); ++i)
			{
				Weeds[i].bOut = i < 3;
				Weeds[i].FlyT = i < 3 ? 1.0f : -1.0f;
				Weeds[i].Pull = 0.0f;
				Weeds[i].Loosen = 0.0f;
			}
			Pulled = 3;
			if (Weeds.IsValidIndex(3))
			{
				Grab = 3;
				Weeds[3].Pull = 42.0f;
				Weeds[3].Loosen = 0.55f;
				StretchGoal = 42.0f;
				GrabAt = Head(3) + FVector2f(0.0f, 84.0f);
				Mouse = Head(3);
			}
		}
		else
		{
			InVase[0] = 2;
			InVase[1] = 1;
			InVase[2] = 0;
			Carry = Want[1];
			Mouse = FVector2f(GraveX[1] - 40.0f, 236.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintYard(P);
		for (int32 g = 0; g < 3; ++g)
		{
			PaintStone(P, g);
		}
		if (Stage == 0)
		{
			PaintWeedStage(P);
		}
		else
		{
			PaintFlowerStage(P);
		}
	}

private:
	static constexpr float GraveX[3] = {230.0f, 385.0f, 540.0f};
	static constexpr float StoneW[3] = {108.0f, 118.0f, 100.0f};
	static constexpr float StoneH[3] = {122.0f, 140.0f, 114.0f};
	static constexpr float StoneBase = 268.0f;
	static constexpr int32 NumWeeds = 8;
	const FVector2f Bucket = FVector2f(84.0f, 330.0f);

	struct FWeed
	{
		FVector2f Root = FVector2f::ZeroVector;
		int32 Kind = 0;
		float Pull = 0.0f;
		float Loosen = 0.0f;
		bool bOut = false;
		float FlyT = -1.0f;
		FVector2f FlyFrom = FVector2f::ZeroVector;
	};
	struct FCrumb
	{
		FVector2f Pos;
		FVector2f Vel;
		float Age = 0.0f;
	};
	struct FFall
	{
		FVector2f Pos;
		int32 Kind = 0;
		float Age = 0.0f;
	};

	int32 Want[3] = {0, 1, 2};
	bool bWantSet = false;
	TArray<FWeed> Weeds;
	TArray<FCrumb> Crumbs;
	int32 Grab = -1;
	FVector2f GrabAt = FVector2f::ZeroVector;
	float StretchGoal = 0.0f;
	int32 Pulled = 0;
	int32 InVase[3] = {0, 0, 0};
	float StemAge[3][2] = {{10.0f, 10.0f}, {10.0f, 10.0f}, {10.0f, 10.0f}};
	int32 Carry = -1;
	TArray<FFall> Falls;
	float DoneT = -1.0f;
	float AutoT = 0.0f;

	static FLinearColor FlowerColor(int32 Kind)
	{
		static const uint32 Colors[] = {0xE0413A, 0x3F7FD6, 0xF2C230, 0xF4F0FF};
		return C(Colors[Kind % 4]);
	}

	/** Colour-blind marker per flower kind: rose circle, cornflower triangle, daisy diamond, lily square. */
	static int32 SymbolOf(int32 Kind)
	{
		static const int32 Symbols[] = {0, 2, 3, 1};
		return Symbols[Kind % 4];
	}

	FVector2f Head(int32 i) const
	{
		const FWeed& Wd = Weeds[i];
		const float Shake = Wd.Loosen > 0.02f ? FMath::Sin(Time * 50.0f + i) * 2.0f * Wd.Loosen : 0.0f;
		return Wd.Root + FVector2f(Shake, -(30.0f + Wd.Pull));
	}

	FVector2f BunchPos(int32 k) const { return FVector2f(40.0f + k * 38.0f, 300.0f); }

	void Pop(int32 i)
	{
		FWeed& Wd = Weeds[i];
		Wd.FlyFrom = Head(i);
		Wd.bOut = true;
		Wd.FlyT = 0.0f;
		Wd.Pull = 0.0f;
		Grab = -1;
		++Pulled;
		for (int32 k = 0; k < 9; ++k)
		{
			Crumbs.Add({Wd.Root + FVector2f(0.0f, -2.0f), FVector2f((Rng.FRand() * 2.0f - 1.0f) * 120.0f, -160.0f - Rng.FRand() * 140.0f), 0.0f});
		}
		Nice(Wd.Root + FVector2f(0.0f, -70.0f), Pulled >= NumWeeds ? TEXT("All weeded!") : TEXT("Pop!"), TEXT("S_Chore_Pluck"),
		     0.85f + 0.05f * Pulled);
	}

	void TickWeeds(float Dt)
	{
		bool bAllLanded = Weeds.Num() > 0;
		for (int32 i = 0; i < Weeds.Num(); ++i)
		{
			FWeed& Wd = Weeds[i];
			if (Wd.bOut)
			{
				if (Wd.FlyT < 1.0f)
				{
					Wd.FlyT = FMath::Min(1.0f, Wd.FlyT + Dt / 0.45f);
					bAllLanded = false;
				}
				continue;
			}
			bAllLanded = false;
			const bool bHeld = Grab == i;
			Wd.Pull = Approach(Wd.Pull, bHeld ? StretchGoal : 0.0f, Dt, bHeld ? 16.0f : 12.0f);
			if (bHeld && Wd.Pull > 26.0f)
			{
				// Roots give way only under steady tension.
				Wd.Loosen += Dt / 0.5f * FMath::Clamp((Wd.Pull - 26.0f) / 14.0f, 0.35f, 1.0f);
			}
			else
			{
				Wd.Loosen = FMath::Max(0.0f, Wd.Loosen - Dt * 0.25f);
			}
			if (Wd.Loosen >= 1.0f)
			{
				Pop(i);
			}
		}
		if (bAllLanded && DoneT < 0.0f)
		{
			DoneT = 0.0f;
		}
	}

	void PaintYard(FKGMgPainter& P) const
	{
		const float GroundY = 226.0f;
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, GroundY + 4.0f), C(0x40508F), C(0xF2A66B));
		for (int32 i = 0; i < 14; ++i)
		{
			const FVector2f S(KGMgVillage::Hash01(i * 3 + 1) * W, 6.0f + KGMgVillage::Hash01(i * 3 + 2) * 90.0f);
			P.Rect(S, FVector2f(2.0f, 2.0f), A(Cream, 0.4f + 0.4f * FMath::Sin(Time * 2.0f + i * 1.3f)));
		}
		P.Glow(FVector2f(470.0f, 206.0f), 170.0f, A(C(0xFFC47A), 0.55f));
		P.Circle(FVector2f(470.0f, 198.0f), 24.0f, C(0xFFD89A));
		TArray<FVector2f> Hills;
		for (int32 i = 0; i <= 16; ++i)
		{
			Hills.Add(FVector2f(W * i / 16.0f, 196.0f - 14.0f * FMath::Sin(i * 0.8f) - 8.0f * FMath::Sin(i * 2.1f)));
		}
		for (int32 i = 0; i + 1 < Hills.Num(); ++i)
		{
			P.Quad(Hills[i], Hills[i + 1], FVector2f(Hills[i + 1].X, GroundY + 2.0f), FVector2f(Hills[i].X, GroundY + 2.0f), C(0x6A5A8A));
		}
		// Church silhouette with a lit window.
		const FLinearColor Sil = C(0x3A2F55);
		P.Rect(FVector2f(40.0f, 134.0f), FVector2f(84.0f, 96.0f), Sil);
		P.Tri(FVector2f(34.0f, 136.0f), FVector2f(130.0f, 136.0f), FVector2f(82.0f, 102.0f), Sil);
		P.Rect(FVector2f(60.0f, 70.0f), FVector2f(26.0f, 66.0f), Sil);
		P.Tri(FVector2f(56.0f, 72.0f), FVector2f(90.0f, 72.0f), FVector2f(73.0f, 26.0f), Sil);
		P.Bar(FVector2f(73.0f, 12.0f), FVector2f(73.0f, 28.0f), 3.0f, Sil);
		P.Bar(FVector2f(67.0f, 18.0f), FVector2f(79.0f, 18.0f), 3.0f, Sil);
		P.Glow(FVector2f(90.0f, 172.0f), 30.0f, A(Gold, 0.5f));
		P.RoundRect(FVector2f(84.0f, 160.0f), FVector2f(12.0f, 22.0f), A(C(0xFFD27A), 0.95f), 5.0f);
		P.RoundRect(FVector2f(66.0f, 90.0f), FVector2f(10.0f, 16.0f), A(C(0xFFD27A), 0.8f), 4.0f);
		// Cypresses.
		P.RoundRect(FVector2f(140.0f, 120.0f), FVector2f(26.0f, 108.0f), C(0x2B3A3A), -1.0f);
		P.RoundRect(FVector2f(598.0f, 104.0f), FVector2f(30.0f, 124.0f), C(0x2B3A3A), -1.0f);
		P.RoundRect(FVector2f(574.0f, 140.0f), FVector2f(22.0f, 88.0f), C(0x324444), -1.0f);
		// Ground + iron fence.
		P.RectV(FVector2f(0.0f, GroundY), FVector2f(W, H - GroundY), C(0x5E8F45), C(0x3C6A2E));
		const FLinearColor FenceC = C(0x2E2A3A);
		P.Bar(FVector2f(0.0f, GroundY - 18.0f), FVector2f(W, GroundY - 18.0f), 3.0f, FenceC);
		P.Bar(FVector2f(0.0f, GroundY + 2.0f), FVector2f(W, GroundY + 2.0f), 3.0f, FenceC);
		for (float X = 8.0f; X < W; X += 24.0f)
		{
			P.Bar(FVector2f(X, GroundY + 8.0f), FVector2f(X, GroundY - 26.0f), 3.0f, FenceC);
			P.Tri(FVector2f(X - 4.0f, GroundY - 25.0f), FVector2f(X + 4.0f, GroundY - 25.0f), FVector2f(X, GroundY - 34.0f), FenceC);
		}
		for (int32 i = 0; i < 26; ++i)
		{
			const FVector2f T(KGMgVillage::Hash01(i * 5 + 11) * W, GroundY + 24.0f + KGMgVillage::Hash01(i * 5 + 12) * 166.0f);
			const FLinearColor Tuft = C(0x4E7F38);
			P.Tri(T + FVector2f(-6.0f, 0.0f), T + FVector2f(-2.0f, 0.0f), T + FVector2f(-7.0f, -10.0f), Tuft);
			P.Tri(T + FVector2f(-2.0f, 0.0f), T + FVector2f(2.0f, 0.0f), T + FVector2f(0.0f, -13.0f), Tuft);
			P.Tri(T + FVector2f(2.0f, 0.0f), T + FVector2f(6.0f, 0.0f), T + FVector2f(7.0f, -9.0f), Tuft);
		}
		// Fireflies.
		for (int32 i = 0; i < 7; ++i)
		{
			const FVector2f F(60.0f + i * 88.0f + 20.0f * FMath::Sin(Time * 0.7f + i * 2.1f), 150.0f + (i % 3) * 30.0f + 12.0f * FMath::Sin(Time * 1.1f + i));
			const float Blink = 0.5f + 0.5f * FMath::Sin(Time * 3.0f + i * 1.9f);
			P.Glow(F, 12.0f, A(C(0xE8FF8A), 0.5f * Blink));
			P.Circle(F, 1.8f, A(C(0xF4FFB0), Blink));
		}
	}

	void PaintStone(FKGMgPainter& P, int32 g) const
	{
		static const TCHAR* Names[3] = {TEXT("OLD TOM"), TEXT("MAREN"), TEXT("BRAM")};
		const float X = GraveX[g];
		const float Wd = StoneW[g];
		const float R = Wd * 0.5f;
		const float Top = StoneBase - StoneH[g];
		const FLinearColor StoneC = C(0x9A98A8);
		P.RoundRect(FVector2f(X - R + 6.0f, Top + 8.0f), FVector2f(Wd, StoneH[g]), A(Ink, 0.22f), R);
		P.Circle(FVector2f(X, Top + R), R, StoneC);
		P.Rect(FVector2f(X - R, Top + R), FVector2f(Wd, StoneH[g] - R), StoneC);
		P.Rect(FVector2f(X + R - 14.0f, Top + R), FVector2f(14.0f, StoneH[g] - R), A(Ink, 0.14f));
		P.ArcBand(FVector2f(X, Top + R), R - 14.0f, R, -0.5f * PI, 0.0f, A(Ink, 0.14f), 12);
		P.Arc(FVector2f(X, Top + R), R - 5.0f, PI * 1.05f, PI * 1.45f, A(FLinearColor::White, 0.35f), 3.0f, 12);
		P.Rect(FVector2f(X - R + 4.0f, Top + R), FVector2f(4.0f, StoneH[g] - R - 10.0f), A(FLinearColor::White, 0.18f));
		P.RoundRect(FVector2f(X - R - 10.0f, StoneBase - 8.0f), FVector2f(Wd + 20.0f, 16.0f), C(0x6E6C7A), 3.0f);
		const FLinearColor Carve = C(0x5A5866);
		P.Bar(FVector2f(X, Top + 14.0f), FVector2f(X, Top + 38.0f), 4.0f, Carve);
		P.Bar(FVector2f(X - 8.0f, Top + 22.0f), FVector2f(X + 8.0f, Top + 22.0f), 4.0f, Carve);
		P.Text(FVector2f(X, Top + 46.0f), Names[g], 12.0f, Carve, 0.5f, TEXT("Black"), 40);
		P.Circle(FVector2f(X - R + 9.0f, StoneBase - 14.0f), 7.0f, A(C(0x6BAF4A), 0.7f));
		P.Circle(FVector2f(X - R + 18.0f, StoneBase - 11.0f), 5.0f, A(C(0x6BAF4A), 0.6f));
		// Ribbon: colour + symbol of the flower this grave wants.
		const FLinearColor Rc = FlowerColor(Want[g]);
		const FLinearColor RcDark = Mix(Rc, Ink, 0.35f);
		const float BandY = StoneBase - 40.0f;
		const FVector2f Bow(X - R * 0.35f, BandY);
		P.Bar(FVector2f(X - R, BandY + 1.0f), FVector2f(X + R, BandY + 1.0f), 10.0f, RcDark);
		P.Bar(FVector2f(X - R, BandY), FVector2f(X + R, BandY), 7.0f, Rc);
		P.Bar(Bow, Bow + FVector2f(-10.0f, 26.0f), 7.0f, RcDark);
		P.Bar(Bow, Bow + FVector2f(9.0f, 28.0f), 7.0f, RcDark);
		P.Bar(Bow, Bow + FVector2f(-10.0f, 25.0f), 5.0f, Rc);
		P.Bar(Bow, Bow + FVector2f(9.0f, 27.0f), 5.0f, Rc);
		P.Tri(Bow, Bow + FVector2f(-24.0f, -14.0f), Bow + FVector2f(-24.0f, 12.0f), RcDark);
		P.Tri(Bow, Bow + FVector2f(24.0f, -14.0f), Bow + FVector2f(24.0f, 12.0f), RcDark);
		P.Tri(Bow, Bow + FVector2f(-21.0f, -11.0f), Bow + FVector2f(-21.0f, 9.0f), Rc);
		P.Tri(Bow, Bow + FVector2f(21.0f, -11.0f), Bow + FVector2f(21.0f, 9.0f), Rc);
		P.Circle(Bow, 9.0f, Mix(Rc, Ink, 0.1f), RcDark, 1.5f);
		KGMgVillage::Symbol(P, SymbolOf(Want[g]), Bow, 4.5f, Want[g] == 3 ? Ink : Cream);
		// Mound.
		P.Disc(FVector2f(X, 294.0f), FVector2f(80.0f, 24.0f), C(0x6B4428), C(0x4E3020));
		P.Arc(FVector2f(X, 294.0f), 78.0f, PI * 1.15f, PI * 1.85f, A(C(0x8C6A4A), 0.5f), 2.0f, 16);
	}

	void PaintWeed(FKGMgPainter& P, const FVector2f& Root, int32 Kind, float Pull, float Loosen, float Shake) const
	{
		const FVector2f HeadP = Root + FVector2f(Shake, -(30.0f + Pull));
		if (Loosen > 0.02f)
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const FVector2f D = KGMgVillage::Polar(PI * (k / 4.0f)) * FVector2f(1.0f, 0.35f);
				P.Segment(Root, Root + D * (8.0f + 16.0f * Loosen), A(Ink, 0.5f), 1.5f);
			}
		}
		P.Disc(Root + FVector2f(0.0f, 2.0f), FVector2f(12.0f + 4.0f * Loosen, 4.0f), C(0x3A2416), A(C(0x3A2416), 0.0f));
		const FVector2f GreenBase = Root + FVector2f(Shake * 0.3f, -Pull * 0.55f);
		if (Pull > 3.0f)
		{
			P.Bar(Root, GreenBase, 3.5f, C(0xEADCB8));
			P.Segment(Root + FVector2f(-1.0f, -2.0f), Root + FVector2f(-6.0f, 3.0f), C(0xEADCB8), 1.5f);
			P.Segment(Root + FVector2f(1.0f, -2.0f), Root + FVector2f(6.0f, 4.0f), C(0xEADCB8), 1.5f);
		}
		P.Bar(GreenBase, HeadP, 3.5f, C(0x4E8A2E));
		static const float LeafAng[4] = {-0.5f * PI - 1.25f, -0.5f * PI - 0.55f, -0.5f * PI + 0.55f, -0.5f * PI + 1.25f};
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2f D = KGMgVillage::Polar(LeafAng[k] + 0.08f * Shake);
			const FVector2f N(-D.Y, D.X);
			const float Len = (k == 1 || k == 2) ? 20.0f : 16.0f;
			P.Tri(GreenBase + N * 4.0f, GreenBase - N * 4.0f, GreenBase + D * Len, k % 2 ? C(0x5FA83A) : C(0x4E8A2E));
			P.Segment(GreenBase, GreenBase + D * Len * 0.8f, A(C(0xB8E08A), 0.6f), 1.0f);
		}
		PaintWeedHead(P, HeadP, Kind);
	}

	static void PaintWeedHead(FKGMgPainter& P, const FVector2f& HeadP, int32 Kind)
	{
		if (Kind == 0)
		{
			for (int32 k = 0; k < 8; ++k)
			{
				P.Circle(HeadP + KGMgVillage::Polar(k * PI / 4.0f) * 6.0f, 4.5f, C(0xFFD84A));
			}
			P.Circle(HeadP, 5.5f, C(0xF2B01E));
		}
		else if (Kind == 1)
		{
			P.Circle(HeadP + FVector2f(0.0f, 4.0f), 6.0f, C(0x5FA83A));
			for (int32 k = 0; k < 5; ++k)
			{
				const float X = -6.0f + k * 3.0f;
				P.Tri(HeadP + FVector2f(X - 2.0f, 1.0f), HeadP + FVector2f(X + 2.0f, 1.0f), HeadP + FVector2f(X * 1.6f, -12.0f), C(0xB06AD9));
			}
		}
		else
		{
			for (int32 k = 0; k < 3; ++k)
			{
				P.Circle(HeadP + KGMgVillage::Polar(-0.5f * PI + k * KGMgVillage::TwoPi / 3.0f) * 5.0f, 5.0f, C(0x6BBF45));
			}
			P.Circle(HeadP + FVector2f(0.0f, -12.0f), 5.0f, C(0xF2D6E6));
			P.Bar(HeadP, HeadP + FVector2f(0.0f, -8.0f), 2.0f, C(0x4E8A2E));
		}
	}

	static void FlowerHead(FKGMgPainter& P, int32 Kind, const FVector2f& Ctr, float S)
	{
		const FLinearColor Col = FlowerColor(Kind);
		switch (Kind)
		{
		case 0:
			P.Circle(Ctr, 10.0f * S, Col);
			P.Circle(Ctr + FVector2f(1.0f, -1.0f) * S, 6.5f * S, C(0xB82E2A));
			P.Arc(Ctr, 4.0f * S, 0.0f, 5.0f, C(0xFF7A70), 2.0f, 10);
			break;
		case 1:
			for (int32 k = 0; k < 8; ++k)
			{
				const float Ang = k * PI / 4.0f;
				P.Tri(Ctr + KGMgVillage::Polar(Ang - 0.25f) * 3.0f * S, Ctr + KGMgVillage::Polar(Ang + 0.25f) * 3.0f * S,
				      Ctr + KGMgVillage::Polar(Ang) * 12.0f * S, Col);
			}
			P.Circle(Ctr, 4.0f * S, C(0x1D3F7A));
			break;
		case 2:
			for (int32 k = 0; k < 9; ++k)
			{
				P.Circle(Ctr + KGMgVillage::Polar(k * KGMgVillage::TwoPi / 9.0f) * 7.5f * S, 4.0f * S, Col);
			}
			P.Circle(Ctr, 4.5f * S, C(0xE07A1A));
			break;
		default:
			for (int32 k = 0; k < 5; ++k)
			{
				const float Ang = -0.5f * PI + k * KGMgVillage::TwoPi / 5.0f;
				P.Tri(Ctr + KGMgVillage::Polar(Ang - 0.45f) * 3.0f * S, Ctr + KGMgVillage::Polar(Ang + 0.45f) * 3.0f * S,
				      Ctr + KGMgVillage::Polar(Ang) * 13.0f * S, C(0xC8C0D8));
				P.Tri(Ctr + KGMgVillage::Polar(Ang - 0.35f) * 3.0f * S, Ctr + KGMgVillage::Polar(Ang + 0.35f) * 3.0f * S,
				      Ctr + KGMgVillage::Polar(Ang) * 11.5f * S, Col);
			}
			P.Circle(Ctr, 3.5f * S, Gold);
			break;
		}
	}

	static void Stem(FKGMgPainter& P, const FVector2f& From, const FVector2f& To, int32 Kind, float S)
	{
		P.Bar(From, To, 3.0f, C(0x4E8A2E));
		const FVector2f Mid = FMath::Lerp(From, To, 0.45f);
		P.Tri(Mid, Mid + FVector2f(-10.0f, -2.0f), Mid + FVector2f(-3.0f, -9.0f), C(0x5FA83A));
		FlowerHead(P, Kind, To, S);
	}

	void PaintWeedStage(FKGMgPainter& P) const
	{
		// Bucket for the pulled weeds.
		P.Disc(Bucket + FVector2f(0.0f, 36.0f), FVector2f(44.0f, 8.0f), A(Ink, 0.3f), A(Ink, 0.0f));
		P.Quad(Bucket + FVector2f(-36.0f, -30.0f), Bucket + FVector2f(36.0f, -30.0f), Bucket + FVector2f(28.0f, 34.0f), Bucket + FVector2f(-28.0f, 34.0f), C(0x8C8A94));
		P.Quad(Bucket + FVector2f(14.0f, -30.0f), Bucket + FVector2f(36.0f, -30.0f), Bucket + FVector2f(28.0f, 34.0f), Bucket + FVector2f(12.0f, 34.0f), A(Ink, 0.15f));
		int32 Landed = 0;
		for (const FWeed& Wd : Weeds)
		{
			if (Wd.bOut && Wd.FlyT >= 1.0f)
			{
				const FVector2f At = Bucket + FVector2f(-24.0f + (Landed % 4) * 16.0f, -34.0f - (Landed / 4) * 8.0f);
				P.Bar(At + FVector2f(0.0f, 10.0f), At, 3.0f, C(0x4E8A2E));
				PaintWeedHead(P, At, Wd.Kind);
				++Landed;
			}
		}
		P.Disc(Bucket + FVector2f(0.0f, -30.0f), FVector2f(36.0f, 7.0f), C(0x55535E), C(0x3A3844));
		P.Bar(Bucket + FVector2f(-34.0f, -12.0f), Bucket + FVector2f(34.0f, -12.0f), 4.0f, Iron);
		P.Bar(Bucket + FVector2f(-30.0f, 20.0f), Bucket + FVector2f(30.0f, 20.0f), 4.0f, Iron);
		P.Arc(Bucket + FVector2f(0.0f, -30.0f), 36.0f, PI * 1.1f, PI * 1.9f, Iron, 3.0f, 16);

		for (int32 i = 0; i < Weeds.Num(); ++i)
		{
			const FWeed& Wd = Weeds[i];
			if (Wd.bOut)
			{
				if (Wd.FlyT < 1.0f)
				{
					const float T = EaseOutCubic(Wd.FlyT);
					const FVector2f At = FMath::Lerp(Wd.FlyFrom, Bucket + FVector2f(0.0f, -40.0f), T) + FVector2f(0.0f, -90.0f * FMath::Sin(PI * Wd.FlyT));
					P.Bar(At + FVector2f(0.0f, 30.0f), At, 3.0f, C(0x4E8A2E));
					P.Circle(At + FVector2f(0.0f, 32.0f), 6.0f, C(0x6B4428));
					PaintWeedHead(P, At, Wd.Kind);
				}
				else
				{
					P.Disc(Wd.Root, FVector2f(9.0f, 4.0f), C(0x2E1C10), C(0x4E3020));
				}
				continue;
			}
			const FVector2f H0 = Head(i);
			PaintWeed(P, Wd.Root, Wd.Kind, Wd.Pull, Wd.Loosen, H0.X - Wd.Root.X);
			if (Grab == i)
			{
				P.Arc(H0, 20.0f, -0.5f * PI, -0.5f * PI + KGMgVillage::TwoPi * Saturate(Wd.Loosen), Good, 4.0f, 32);
			}
		}
		for (const FCrumb& Cr : Crumbs)
		{
			P.Circle(Cr.Pos, 3.0f, A(C(0x6B4428), 1.0f - Cr.Age / 0.9f));
		}
		if (Pulled == 0 && Grab < 0)
		{
			for (int32 i = 0; i < Weeds.Num(); ++i)
			{
				if (!Weeds[i].bOut)
				{
					const FVector2f H0 = Head(i);
					P.HintRing(H0, 24.0f, Time, Gold);
					P.HintArrow(H0 + FVector2f(32.0f, 20.0f), H0 + FVector2f(32.0f, -50.0f), Time, Gold);
					break;
				}
			}
		}
		P.Tag(FVector2f(560.0f, 34.0f), FString::Printf(TEXT("WEEDS  %d / %d"), Pulled, Weeds.Num()), A(Ink, 0.75f), Cream, 14.0f);
	}

	void PaintFlowerStage(FKGMgPainter& P) const
	{
		for (int32 g = 0; g < 3; ++g)
		{
			const float X = GraveX[g];
			// Flowers first so the vase lip covers the stems.
			for (int32 s = 0; s < InVase[g]; ++s)
			{
				const float Bloom = EaseOutBack(Saturate(StemAge[g][s] / 0.35f));
				const float Lean = (s == 0 ? -0.3f : 0.32f);
				const FVector2f From(X, 292.0f);
				const FVector2f To = From + KGMgVillage::Polar(-0.5f * PI + Lean) * (30.0f + 24.0f * Bloom);
				Stem(P, From, To, Want[g], FMath::Max(0.1f, Bloom) * 1.15f);
			}
			P.Disc(FVector2f(X, 324.0f), FVector2f(26.0f, 6.0f), A(Ink, 0.3f), A(Ink, 0.0f));
			P.Quad(FVector2f(X - 20.0f, 288.0f), FVector2f(X + 20.0f, 288.0f), FVector2f(X + 14.0f, 322.0f), FVector2f(X - 14.0f, 322.0f), C(0x9A98A8));
			P.Quad(FVector2f(X + 6.0f, 288.0f), FVector2f(X + 20.0f, 288.0f), FVector2f(X + 14.0f, 322.0f), FVector2f(X + 5.0f, 322.0f), A(Ink, 0.15f));
			P.RoundRect(FVector2f(X - 24.0f, 283.0f), FVector2f(48.0f, 8.0f), C(0xB8B6C4), 3.0f);
			P.RoundRect(FVector2f(X - 17.0f, 320.0f), FVector2f(34.0f, 6.0f), C(0x6E6C7A), 2.0f);
			for (int32 s = 0; s < 2; ++s)
			{
				P.Circle(FVector2f(X - 7.0f + s * 14.0f, 305.0f), 4.0f, s < InVase[g] ? Good : A(Ink, 0.35f));
			}
			if (Carry >= 0 && InRect(Mouse, FVector2f(X - 64.0f, 140.0f), FVector2f(128.0f, 200.0f)))
			{
				P.Arc(FVector2f(X, 290.0f), 40.0f, 0.0f, KGMgVillage::TwoPi, A(Cream, 0.8f), 3.0f, 36);
			}
		}

		// Basket of flowers with symbol tags.
		P.Arc(FVector2f(96.0f, 330.0f), 72.0f, PI * 1.05f, PI * 1.95f, C(0x7A5230), 6.0f, 24);
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2f B = BunchPos(k);
			for (int32 j = 0; j < 3; ++j)
			{
				const FVector2f HeadP = B + FVector2f(-7.0f + j * 7.0f, j == 1 ? -6.0f : 4.0f);
				Stem(P, FVector2f(B.X, 336.0f), HeadP, k, 0.8f);
			}
		}
		P.Shadow(FVector2f(14.0f, 322.0f), FVector2f(164.0f, 70.0f), 10.0f, 0.3f, 4.0f);
		P.RoundRect(FVector2f(14.0f, 322.0f), FVector2f(164.0f, 70.0f), C(0xB8864E), 10.0f, C(0x7A5230), 2.0f);
		for (int32 r = 0; r < 3; ++r)
		{
			P.Rect(FVector2f(20.0f, 334.0f + r * 18.0f), FVector2f(152.0f, 2.0f), A(C(0x7A5230), 0.6f));
		}
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2f TagC(BunchPos(k).X, 362.0f);
			P.RoundRect(TagC - FVector2f(12.0f, 11.0f), FVector2f(24.0f, 22.0f), C(0xF0E0B8), 4.0f, C(0x7A5230), 1.5f);
			KGMgVillage::Symbol(P, SymbolOf(k), TagC, 5.5f, Mix(FlowerColor(k), Ink, k == 3 ? 0.6f : 0.15f));
		}

		for (const FFall& F : Falls)
		{
			const FVector2f At = F.Pos + FVector2f(20.0f * F.Age, 140.0f * F.Age * F.Age);
			P.Bar(At, At + FVector2f(10.0f, 30.0f), 3.0f, A(C(0x4E8A2E), 1.0f - F.Age / 0.6f));
			FlowerHead(P, F.Kind, At, 1.0f - F.Age);
		}
		if (Carry >= 0)
		{
			Stem(P, Mouse + FVector2f(6.0f, 44.0f), Mouse, Carry, 1.2f);
		}

		const int32 Total = InVase[0] + InVase[1] + InVase[2];
		if (Total == 0 && Carry < 0)
		{
			const FVector2f B = BunchPos(Want[0]);
			P.HintRing(B, 24.0f, Time, Gold);
			P.HintArrow(B + FVector2f(16.0f, -26.0f), FVector2f(GraveX[0] - 16.0f, 258.0f), Time, Gold);
		}
		if (DoneT >= 0.0f)
		{
			for (int32 g = 0; g < 3; ++g)
			{
				KGMgVillage::Sparkle(P, FVector2f(GraveX[g] + 24.0f, 236.0f), 12.0f * FMath::Sin(Saturate(DoneT / 0.6f) * PI), A(Gold, 0.9f));
			}
		}
		P.Tag(FVector2f(560.0f, 34.0f), FString::Printf(TEXT("FLOWERS  %d / 6"), Total), A(Ink, 0.75f), Cream, 14.0f);
	}
};

// =====================================================================================================================
// WindClock: circle anticlockwise to wind the spring into the green, then drag the minute hand to the posted time.
// =====================================================================================================================
class FKGMgWindClock final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Circle anticlockwise to wind, let go in the green") : TEXT("Drag the minute hand to the time on the note");
	}

	virtual void BeginStage() override
	{
		DoneT = -1.0f;
		bGrab = false;
		Tension = 0.0f;
		KeyAng = -0.5f * PI;
		PrevAng = 0.0f;
		FrameTurn = 0.0f;
		SlipT = 0.0f;
		ClickAcc = 0.0f;
		WrongAcc = 0.0f;
		WarnT = 0.0f;
		StartMin = float(Rng.RandRange(1, 12) * 60 + Rng.RandRange(0, 11) * 5);
		TargetMin = StartMin + float(Rng.RandRange(27, 42) * 5);
		Minutes = StartMin;
		WantMin = StartMin;
		LastStep = FMath::FloorToInt(Minutes / 5.0f);
		bCheck = false;
		bTouched = false;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (DoneT >= 0.0f)
		{
			return;
		}
		if (Stage == 0)
		{
			if (FVector2f::Distance(Pos, KeyC) < 160.0f)
			{
				bGrab = true;
				PrevAng = FMath::Atan2(Pos.Y - KeyC.Y, Pos.X - KeyC.X);
			}
		}
		else if (FVector2f::Distance(Pos, DialC) < DialR + 24.0f)
		{
			bGrab = true;
			bCheck = false;
			bTouched = true;
			PrevAng = FMath::Atan2(Pos.Y - DialC.Y, Pos.X - DialC.X);
			Sound(TEXT("S_UI_Click"), 0.4f, 1.0f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (!bGrab)
		{
			return;
		}
		bGrab = false;
		if (Stage == 0)
		{
			TryFinish(false);
		}
		else
		{
			WantMin = FMath::RoundToFloat(Minutes / 5.0f) * 5.0f;
			bCheck = true;
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (!bGrab)
		{
			return;
		}
		const FVector2f Ctr = Stage == 0 ? KeyC : DialC;
		if (FVector2f::Distance(Pos, Ctr) < 18.0f)
		{
			return;
		}
		const float Ang = FMath::Atan2(Pos.Y - Ctr.Y, Pos.X - Ctr.X);
		const float D = KGMgVillage::WrapPi(Ang - PrevAng);
		PrevAng = Ang;
		if (Stage == 0)
		{
			FrameTurn += D;
		}
		else
		{
			WantMin += D / KGMgVillage::TwoPi * 60.0f;
		}
	}

	virtual void OnKey(const FKey& Key) override
	{
		if (Stage == 0 && Key == EKeys::SpaceBar)
		{
			TryFinish(true);
		}
	}

	virtual void Tick(float Dt) override
	{
		WarnT -= Dt;
		if (Stage == 0)
		{
			TickWind(Dt);
		}
		else
		{
			TickSet(Dt);
		}
		if (DoneT >= 0.0f)
		{
			DoneT += Dt;
			const float MinTime = Stage == 0 ? 3.8f : 1.9f;
			if (DoneT >= 0.55f && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (DoneT >= 0.0f)
		{
			return;
		}
		if (Stage == 0)
		{
			bGrab = true;
			FrameTurn -= 0.7f * KGMgVillage::TwoPi * Dt;
			Mouse = KeyC + KGMgVillage::Polar(KeyAng) * 62.0f;
			if (Tension >= 0.76f)
			{
				OnRelease(Mouse);
			}
		}
		else
		{
			bGrab = true;
			bTouched = true;
			WantMin = FMath::Min(WantMin + 55.0f * Dt, TargetMin);
			Mouse = DialC + KGMgVillage::Polar(MinuteAngle()) * 140.0f;
			if (FMath::Abs(Minutes - TargetMin) < 0.5f)
			{
				OnRelease(Mouse);
			}
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			Tension = 0.55f;
			KeyAng = -2.3f;
			bGrab = true;
			Mouse = KeyC + KGMgVillage::Polar(KeyAng) * 62.0f;
		}
		else
		{
			Minutes = FMath::RoundToFloat(FMath::Lerp(StartMin, TargetMin, 0.6f) / 5.0f) * 5.0f;
			WantMin = Minutes;
			bGrab = true;
			bTouched = true;
			Mouse = DialC + KGMgVillage::Polar(MinuteAngle()) * 140.0f;
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintWind(P);
		}
		else
		{
			PaintSet(P);
		}
	}

private:
	static constexpr float GreenLo = 0.68f;
	static constexpr float GreenHi = 0.86f;
	static constexpr float PerTurn = 0.24f;        // tension per full turn
	static constexpr float MaxTurnRate = 0.75f;    // turns per second the ratchet accepts
	static constexpr float MaxMinutesRate = 66.0f; // clock minutes per second the hands can be swept
	static constexpr float DialR = 168.0f;
	const FVector2f KeyC = FVector2f(290.0f, 210.0f);
	const FVector2f DialC = FVector2f(400.0f, 200.0f);

	// Wind
	float Tension = 0.0f;
	float KeyAng = 0.0f;
	float PrevAng = 0.0f;
	float FrameTurn = 0.0f;
	float SlipT = 0.0f;
	float ClickAcc = 0.0f;
	float WrongAcc = 0.0f;
	float WarnT = 0.0f;
	bool bGrab = false;
	// Set
	float StartMin = 0.0f;
	float TargetMin = 0.0f;
	float Minutes = 0.0f;
	float WantMin = 0.0f;
	int32 LastStep = 0;
	bool bCheck = false;
	bool bTouched = false;
	float DoneT = -1.0f;

	static FString Fmt(float M)
	{
		const int32 Total = ((FMath::RoundToInt(M) % 720) + 720) % 720;
		int32 Hr = Total / 60;
		const int32 Mn = Total % 60;
		if (Hr == 0)
		{
			Hr = 12;
		}
		return FString::Printf(TEXT("%d:%02d"), Hr, Mn);
	}

	float MinuteAngle() const { return Minutes / 60.0f * KGMgVillage::TwoPi - 0.5f * PI; }
	float HourAngle() const { return Minutes / 720.0f * KGMgVillage::TwoPi - 0.5f * PI; }

	void TryFinish(bool bFromKey)
	{
		if (DoneT >= 0.0f || SlipT > 0.0f)
		{
			return;
		}
		if (Tension >= GreenLo && Tension <= GreenHi)
		{
			DoneT = 0.0f;
			bGrab = false;
			Nice(KeyC + FVector2f(0.0f, -150.0f), TEXT("Wound tight!"), TEXT("S_Chore_Crank"), 1.3f);
		}
		else if ((bFromKey || Tension > 0.45f) && WarnT <= 0.0f)
		{
			WarnT = 2.0f;
			PopText(KeyC + FVector2f(0.0f, -150.0f), TEXT("Not yet - wind into the green"), Cream);
		}
	}

	void TickWind(float Dt)
	{
		const float Ccw = -FrameTurn;
		FrameTurn = 0.0f;
		if (DoneT >= 0.0f)
		{
			return;
		}
		if (SlipT > 0.0f)
		{
			SlipT -= Dt;
			return;
		}
		if (!bGrab)
		{
			return;
		}
		if (Ccw > 0.0f)
		{
			const float MaxStep = MaxTurnRate * KGMgVillage::TwoPi * Dt;
			const float Turned = FMath::Min(Ccw, MaxStep);
			if (Ccw > MaxStep * 1.8f && WarnT <= 0.0f)
			{
				WarnT = 2.5f;
				PopText(KeyC + FVector2f(0.0f, -150.0f), TEXT("Steady - the key resists"), Gold);
			}
			KeyAng -= Turned;
			Tension += Turned / KGMgVillage::TwoPi * PerTurn;
			ClickAcc += Turned;
			if (ClickAcc > 0.4f)
			{
				ClickAcc = 0.0f;
				Sound(TEXT("S_Chore_Tick"), 0.55f, 0.8f + 0.6f * Tension);
			}
			if (Tension > GreenHi)
			{
				// Overwound: the pawl jumps and the spring slips back.
				Tension = GreenHi - 0.22f;
				SlipT = 0.6f;
				Oops(KeyC + FVector2f(0.0f, -150.0f), TEXT("Overwound! It slipped back"), TEXT("S_Chore_Crank"), 7.0f);
			}
		}
		else if (Ccw < 0.0f)
		{
			WrongAcc += -Ccw;
			ClickAcc += -Ccw;
			if (ClickAcc > 0.6f)
			{
				ClickAcc = 0.0f;
				Sound(TEXT("S_Chore_Tick"), 0.3f, 0.6f);
			}
			if (WrongAcc > 2.5f && WarnT <= 0.0f)
			{
				WrongAcc = 0.0f;
				WarnT = 2.5f;
				PopText(KeyC + FVector2f(0.0f, -150.0f), TEXT("Other way - anticlockwise!"), C(0xFF8A80));
			}
		}
	}

	void TickSet(float Dt)
	{
		if (DoneT >= 0.0f)
		{
			return;
		}
		const float MaxStep = MaxMinutesRate * Dt;
		Minutes += FMath::Clamp(WantMin - Minutes, -MaxStep, MaxStep);
		const int32 StepNow = FMath::FloorToInt(Minutes / 5.0f);
		if (StepNow != LastStep)
		{
			LastStep = StepNow;
			Sound(TEXT("S_Chore_Tick"), 0.5f, 0.9f + 0.03f * float(((StepNow % 12) + 12) % 12));
		}
		if (bCheck && FMath::Abs(WantMin - Minutes) < 0.05f)
		{
			bCheck = false;
			const float Diff = FMath::Fmod(FMath::Abs(WantMin - TargetMin), 720.0f);
			if (FMath::Min(Diff, 720.0f - Diff) <= 5.5f)
			{
				DoneT = 0.0f;
				Nice(DialC + FVector2f(0.0f, -70.0f), TEXT("Right on time!"), TEXT("S_ChurchBell"), 1.5f);
			}
			else
			{
				PopText(FVector2f(107.0f, 214.0f), FString::Printf(TEXT("It reads %s"), *Fmt(WantMin)), Cream);
			}
		}
	}

	void PaintWind(FKGMgPainter& P) const
	{
		const float GearAng = KeyAng * 0.4f;
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0x3E2C22), C(0x1E1512));
		for (int32 i = 0; i < 9; ++i)
		{
			P.Rect(FVector2f(i * 80.0f, 0.0f), FVector2f(2.0f, H), A(Ink, 0.3f));
		}
		KGMgVillage::Gear(P, FVector2f(96.0f, 92.0f), 80.0f, 16, -GearAng * 1.4f + 0.2f, C(0x7A5A26));
		KGMgVillage::Gear(P, FVector2f(520.0f, 340.0f), 100.0f, 20, GearAng + 0.1f, C(0x6E5226));
		KGMgVillage::Gear(P, FVector2f(70.0f, 330.0f), 60.0f, 12, GearAng * 1.9f, C(0x5E4620));
		P.Rect(FVector2f(0.0f, 0.0f), FVector2f(W, H), A(Ink, 0.25f));

		// Spring barrel.
		P.Circle(KeyC + FVector2f(6.0f, 10.0f), 126.0f, A(Ink, 0.4f));
		P.Circle(KeyC, 126.0f, C(0x8C6A2E), C(0xD8B060), 4.0f);
		P.Circle(KeyC, 112.0f, C(0x241A14));
		// The mainspring coils tighter as tension grows.
		TArray<FVector2f> Coil;
		const float Turns = 3.0f + Tension * 5.0f;
		for (int32 k = 0; k <= 180; ++k)
		{
			const float T = k / 180.0f;
			const float R = 26.0f + 80.0f * FMath::Pow(T, 1.0f + Tension * 1.4f);
			Coil.Add(KeyC + KGMgVillage::Polar(KeyAng + T * Turns * KGMgVillage::TwoPi) * R);
		}
		P.Line(Coil, C(0x0E0A08), 6.0f);
		P.Line(Coil, Mix(C(0xC8CED8), C(0xFF9A6A), Saturate((Tension - GreenHi + 0.06f) / 0.06f)), 3.5f);
		P.Glow(KeyC, 120.0f, A(Tension >= GreenLo ? Good : Gold, 0.08f + 0.1f * Tension));

		// Ratchet wheel + pawl.
		const FVector2f RC(448.0f, 70.0f);
		KGMgVillage::Gear(P, RC, 28.0f, 12, KeyAng, C(0xB8923E));
		const float PawlLift = SlipT > 0.0f ? 8.0f + 3.0f * FMath::Sin(Time * 60.0f) : 0.0f;
		P.Bar(FVector2f(500.0f, 40.0f), FVector2f(474.0f, 58.0f - PawlLift), 7.0f, Iron);
		P.Circle(FVector2f(500.0f, 40.0f), 6.0f, C(0x2E3138));

		// The key (butterfly bow).
		const FVector2f Ax = KGMgVillage::Polar(KeyAng);
		const FVector2f Nx(-Ax.Y, Ax.X);
		const FLinearColor Brass = C(0xD8B060);
		const FLinearColor BrassDark = C(0x8C6A2E);
		P.Bar(KeyC - Ax * 62.0f + FVector2f(6.0f, 9.0f), KeyC + Ax * 62.0f + FVector2f(6.0f, 9.0f), 30.0f, A(Ink, 0.35f));
		P.Circle(KeyC + Ax * 62.0f + FVector2f(6.0f, 9.0f), 30.0f, A(Ink, 0.35f));
		P.Circle(KeyC - Ax * 62.0f + FVector2f(6.0f, 9.0f), 30.0f, A(Ink, 0.35f));
		P.Bar(KeyC - Ax * 62.0f, KeyC + Ax * 62.0f, 30.0f, C(0xC9A04A));
		P.Circle(KeyC + Ax * 62.0f, 30.0f, Brass, BrassDark, 3.0f);
		P.Circle(KeyC - Ax * 62.0f, 30.0f, Brass, BrassDark, 3.0f);
		P.Circle(KeyC + Ax * 66.0f, 11.0f, C(0x241A14));
		P.Circle(KeyC - Ax * 66.0f, 11.0f, C(0x241A14));
		P.Bar(KeyC - Ax * 58.0f - Nx * 9.0f, KeyC + Ax * 58.0f - Nx * 9.0f, 4.0f, A(C(0xFFF0B0), 0.5f));
		P.Circle(KeyC, 20.0f, C(0xB8923E), BrassDark, 2.0f);
		P.RotRect(KeyC, FVector2f(14.0f, 14.0f), KeyAng, C(0x2A2230));
		if (bGrab)
		{
			P.Glow(KeyC + Ax * 62.0f, 50.0f, A(Gold, 0.3f));
		}
		else if (DoneT < 0.0f)
		{
			KGMgVillage::Chevrons(P, KeyC, 146.0f, Time, false, A(Cream, 0.75f));
			if (Tension < 0.02f)
			{
				P.HintRing(KeyC + Ax * 62.0f, 30.0f, Time, Gold);
			}
		}

		// Tension gauge: green = let go, red = slips.
		const FVector2f G(560.0f, 64.0f);
		const FVector2f GS(34.0f, 264.0f);
		P.Text(G + FVector2f(17.0f, -24.0f), TEXT("TENSION"), 11.0f, Cream, 0.5f, TEXT("Bold"), 80);
		P.GaugeV(G, GS, Tension, Tension >= GreenLo ? Good : Gold, GreenLo, GreenHi);
		P.RoundRect(G + FVector2f(2.0f, 2.0f), FVector2f(GS.X - 4.0f, GS.Y * (1.0f - GreenHi) - 2.0f), A(Crimson, 0.45f), 3.0f);
		FString Status = TEXT("WIND");
		FLinearColor StatusC = CreamDim;
		if (DoneT >= 0.0f)
		{
			Status = TEXT("WOUND!");
			StatusC = Good;
		}
		else if (SlipT > 0.0f)
		{
			Status = TEXT("SLIPPED!");
			StatusC = C(0xFF8A80);
		}
		else if (Tension >= GreenLo)
		{
			Status = TEXT("LET GO!");
			StatusC = A(Good, 0.6f + 0.4f * KGMg::Ping(Time, 0.5f));
		}
		P.Text(G + FVector2f(17.0f, GS.Y + 8.0f), Status, 13.0f, StatusC, 0.5f, TEXT("Black"));
		P.Text(G + FVector2f(17.0f, GS.Y + 28.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Tension * 100.0f)), 11.0f, CreamDim, 0.5f, TEXT("Bold"));
	}

	void PaintSet(FKGMgPainter& P) const
	{
		// Clock-room wall.
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0x5A5060), C(0x3A3240));
		for (int32 Row = 0; Row < 14; ++Row)
		{
			for (int32 Col = 0; Col < 11; ++Col)
			{
				const float X = Col * 62.0f - (Row % 2) * 31.0f;
				P.RoundRect(FVector2f(X + 2.0f, Row * 30.0f + 2.0f), FVector2f(58.0f, 26.0f), A(C(0x6E6478), 0.4f + 0.1f * ((Row * 5 + Col) % 3)), 4.0f);
			}
		}

		// The mayor's note + the current reading.
		const FVector2f NPos(22.0f, 34.0f);
		const FVector2f NSize(170.0f, 150.0f);
		KGMgVillage::Sheet(P, NPos, NSize, Paper, 0.0f);
		KGMgVillage::BrassPin(P, NPos + FVector2f(85.0f, 10.0f), 6.0f, 1.0f);
		const FLinearColor InkC = C(0x3A2B42);
		P.Text(NPos + FVector2f(85.0f, 24.0f), TEXT("SET THE CLOCK TO"), 11.0f, InkC, 0.5f, TEXT("Black"), 40);
		P.Text(NPos + FVector2f(85.0f, 46.0f), Fmt(TargetMin), 42.0f, Crimson, 0.5f, TEXT("Black"));
		P.Text(NPos + FVector2f(85.0f, 118.0f), TEXT("- the Mayor"), 11.0f, A(InkC, 0.8f), 0.5f, TEXT("Medium"));
		const FVector2f RPos(32.0f, 212.0f);
		P.Shadow(RPos, FVector2f(150.0f, 76.0f), 8.0f, 0.3f, 4.0f);
		P.RoundRect(RPos, FVector2f(150.0f, 76.0f), C(0xC9A04A), 8.0f, C(0x6E5226), 2.0f);
		P.Text(RPos + FVector2f(75.0f, 8.0f), TEXT("NOW"), 10.0f, C(0x4A3818), 0.5f, TEXT("Black"), 120);
		P.Text(RPos + FVector2f(75.0f, 24.0f), Fmt(Minutes), 30.0f, C(0x2A2230), 0.5f, TEXT("Black"));

		// Dial.
		P.Circle(DialC + FVector2f(5.0f, 9.0f), DialR + 12.0f, A(Ink, 0.35f));
		P.Circle(DialC, DialR + 12.0f, C(0x6E5226));
		P.Circle(DialC, DialR + 6.0f, C(0xD8B060));
		P.Disc(DialC, FVector2f(DialR, DialR), C(0xFFF8E6), C(0xE8D8B0), 48);
		P.Arc(DialC, DialR - 26.0f, 0.0f, KGMgVillage::TwoPi, A(InkC, 0.25f), 1.5f, 64);
		for (int32 m = 0; m < 60; ++m)
		{
			const float Ang = m / 60.0f * KGMgVillage::TwoPi - 0.5f * PI;
			const bool bBig = m % 5 == 0;
			P.Bar(DialC + KGMgVillage::Polar(Ang) * (DialR - (bBig ? 20.0f : 10.0f)), DialC + KGMgVillage::Polar(Ang) * (DialR - 4.0f), bBig ? 4.5f : 2.0f, InkC);
		}
		static const TCHAR* Roman[12] = {TEXT("XII"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IIII"), TEXT("V"),
		                                 TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X"), TEXT("XI")};
		for (int32 h = 0; h < 12; ++h)
		{
			const FVector2f At = DialC + KGMgVillage::Polar(h / 12.0f * KGMgVillage::TwoPi - 0.5f * PI) * (DialR - 44.0f);
			P.Text(At - FVector2f(0.0f, 12.0f), Roman[h], 18.0f, InkC, 0.5f, TEXT("Black"));
		}
		P.Text(DialC + FVector2f(0.0f, 42.0f), TEXT("MORROWMERE"), 10.0f, A(InkC, 0.5f), 0.5f, TEXT("Bold"), 120);
		// Target minute: a gold notch on the rim.
		const float TA = TargetMin / 60.0f * KGMgVillage::TwoPi - 0.5f * PI;
		P.Tri(DialC + KGMgVillage::Polar(TA) * (DialR - 2.0f), DialC + KGMgVillage::Polar(TA - 0.06f) * (DialR + 13.0f),
		      DialC + KGMgVillage::Polar(TA + 0.06f) * (DialR + 13.0f), Gold);

		// Hands (shadow first).
		const float Ha = HourAngle();
		const float Ma = MinuteAngle();
		const FLinearColor HandC = C(0x2A2230);
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const FVector2f Off = Pass == 0 ? FVector2f(4.0f, 6.0f) : FVector2f::ZeroVector;
			const FLinearColor Col = Pass == 0 ? A(Ink, 0.25f) : HandC;
			const FVector2f Hd = KGMgVillage::Polar(Ha);
			const FVector2f Hn(-Hd.Y, Hd.X);
			P.Bar(DialC + Off - Hd * 18.0f, DialC + Off + Hd * 84.0f, 10.0f, Col);
			P.Quad(DialC + Off + Hd * 76.0f, DialC + Off + Hd * 92.0f + Hn * 12.0f, DialC + Off + Hd * 112.0f, DialC + Off + Hd * 92.0f - Hn * 12.0f, Col);
			const FVector2f Md = KGMgVillage::Polar(Ma);
			const FVector2f Mn(-Md.Y, Md.X);
			P.Bar(DialC + Off - Md * 26.0f, DialC + Off + Md * 138.0f, 6.0f, Col);
			P.Tri(DialC + Off + Md * 158.0f, DialC + Off + Md * 134.0f + Mn * 9.0f, DialC + Off + Md * 134.0f - Mn * 9.0f, Col);
			P.Circle(DialC + Off - Md * 26.0f, 8.0f, Col);
		}
		P.Circle(DialC, 12.0f, C(0xD8B060), C(0x6E5226), 2.0f);
		const FVector2f MinTip = DialC + KGMgVillage::Polar(Ma) * 146.0f;
		if (bGrab)
		{
			P.Glow(MinTip, 40.0f, A(Gold, 0.45f));
		}
		if (!bTouched)
		{
			P.HintRing(MinTip, 22.0f, Time, Gold);
			P.Arc(DialC, DialR - 64.0f, Ma + 0.2f, Ma + 1.0f, A(Gold, 0.85f), 4.0f, 16);
			const FVector2f HeadP = DialC + KGMgVillage::Polar(Ma + 1.0f) * (DialR - 64.0f);
			const FVector2f Tang = KGMgVillage::Polar(Ma + 1.0f + 0.5f * PI);
			const FVector2f Rad = KGMgVillage::Polar(Ma + 1.0f);
			P.Tri(HeadP + Tang * 12.0f, HeadP + Rad * 8.0f, HeadP - Rad * 8.0f, Gold);
		}
		if (DoneT >= 0.0f)
		{
			const float Ring = Saturate(DoneT / 0.55f);
			P.Arc(DialC, DialR + 14.0f + 40.0f * Ring, 0.0f, KGMgVillage::TwoPi, A(Gold, 1.0f - Ring), 4.0f, 64);
		}
	}
};

// =====================================================================================================================
// GrindFlour: pour three scoops of grain into the hopper, then turn the millstone at a steady speed until the sack fills.
// =====================================================================================================================
class FKGMgGrindFlour final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Click the sack to pour grain into the hopper") : TEXT("Circle to turn the stone - keep the needle green");
	}

	virtual void BeginStage() override
	{
		Pours = 0;
		PourT = -1.0f;
		AutoT = 0.0f;
		DoneT = -1.0f;
		StoneAng = 0.0f;
		PrevAng = 0.0f;
		FrameTurn = 0.0f;
		Speed = 0.0f;
		Progress = 0.0f;
		WarnT = 0.0f;
		GrindAcc = 0.0f;
		SparkT = 0.0f;
		FlowT = 0.0f;
		bGrab = false;
		bInGreen = false;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			if (PourT < 0.0f && Pours < 3 && InRect(Pos, FVector2f(66.0f, 40.0f), FVector2f(150.0f, 164.0f)))
			{
				StartPour();
			}
		}
		else if (DoneT < 0.0f && FVector2f::Distance(Pos, StoneC) < StoneR + 50.0f)
		{
			bGrab = true;
			PrevAng = FMath::Atan2(Pos.Y - StoneC.Y, Pos.X - StoneC.X);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { bGrab = false; }

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage != 1 || !bGrab || FVector2f::Distance(Pos, StoneC) < 20.0f)
		{
			return;
		}
		const float Ang = FMath::Atan2(Pos.Y - StoneC.Y, Pos.X - StoneC.X);
		const float D = KGMgVillage::WrapPi(Ang - PrevAng);
		PrevAng = Ang;
		FrameTurn += D;
		StoneAng += D;
		GrindAcc += FMath::Abs(D);
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickFill(Dt);
		}
		else
		{
			TickGrind(Dt);
		}
		if (DoneT >= 0.0f)
		{
			DoneT += Dt;
			const float MinTime = Stage == 0 ? 1.45f : 7.2f;
			if (DoneT >= (Stage == 0 ? 0.3f : 0.5f) && StageTime >= MinTime && !bSolved)
			{
				Solve();
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			Mouse = FVector2f(140.0f, 120.0f);
			if (PourT < 0.0f && Pours < 3)
			{
				AutoT += Dt;
				if (AutoT > 0.25f)
				{
					AutoT = 0.0f;
					StartPour();
				}
			}
		}
		else if (DoneT < 0.0f)
		{
			bGrab = true;
			const float D = 0.8f * KGMgVillage::TwoPi * Dt;
			StoneAng += D;
			FrameTurn += D;
			GrindAcc += D;
			Mouse = HandleP();
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			Pours = 1;
			PourT = 0.38f;
		}
		else
		{
			Progress = 0.45f;
			Speed = 0.85f;
			bGrab = true;
			bInGreen = true;
			FlowT = 1.0f;
			StoneAng = 0.8f;
			Mouse = HandleP();
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintFill(P);
		}
		else
		{
			PaintGrind(P);
		}
	}

private:
	static constexpr float PourTime = 0.8f;
	static constexpr float GreenLo = 0.55f;   // turns per second
	static constexpr float GreenHi = 1.15f;
	static constexpr float SpeedMax = 1.7f;
	static constexpr float GrindSeconds = 8.0f;
	static constexpr float StoneR = 124.0f;
	const FVector2f Pivot0 = FVector2f(196.0f, 196.0f);
	const FVector2f StoneC = FVector2f(262.0f, 212.0f);
	const FLinearColor Grain = C(0xE0B868);
	const FLinearColor Flour = C(0xFBF5E6);
	const FLinearColor Burlap = C(0xD8B47A);

	// Fill
	int32 Pours = 0;
	float PourT = -1.0f;
	float AutoT = 0.0f;
	// Grind
	float StoneAng = 0.0f;
	float PrevAng = 0.0f;
	float FrameTurn = 0.0f;
	float Speed = 0.0f;
	float Progress = 0.0f;
	float WarnT = 0.0f;
	float GrindAcc = 0.0f;
	float SparkT = 0.0f;
	float FlowT = 0.0f;
	bool bGrab = false;
	bool bInGreen = false;
	float DoneT = -1.0f;

	void StartPour()
	{
		PourT = 0.0f;
		Sound(TEXT("S_Chore_Pour"), 0.9f, 0.9f + 0.1f * Pours);
	}

	float Tilt() const
	{
		if (PourT < 0.0f)
		{
			return 0.0f;
		}
		if (PourT < 0.2f)
		{
			return EaseOutCubic(PourT / 0.2f);
		}
		if (PourT < 0.6f)
		{
			return 1.0f;
		}
		return 1.0f - EaseOutCubic((PourT - 0.6f) / 0.2f);
	}

	float StreamAlpha() const
	{
		if (PourT < 0.15f || PourT > 0.66f)
		{
			return 0.0f;
		}
		return Saturate((PourT - 0.15f) / 0.06f) * Saturate((0.66f - PourT) / 0.06f);
	}

	float Fill() const
	{
		const float Cur = PourT >= 0.0f ? Saturate((PourT - 0.15f) / 0.45f) : 0.0f;
		return Saturate((float(Pours) + Cur) / 3.0f);
	}

	FVector2f SackPt(float X, float Y) const
	{
		const float T = Tilt();
		return Pivot0 + FVector2f(38.0f, -12.0f) * T + KGMgVillage::Rot(FVector2f(X, Y), 1.2f * T);
	}

	FVector2f HandleP() const { return StoneC + KGMgVillage::Polar(StoneAng) * (StoneR - 24.0f); }

	void TickFill(float Dt)
	{
		if (PourT < 0.0f)
		{
			return;
		}
		PourT += Dt;
		if (PourT >= PourTime)
		{
			PourT = -1.0f;
			++Pours;
			Nice(FVector2f(426.0f, 118.0f), Pours >= 3 ? FString(TEXT("Hopper full!")) : FString::Printf(TEXT("%d / 3"), Pours), TEXT("S_UI_Good"),
			     0.9f + 0.1f * Pours);
			if (Pours >= 3 && DoneT < 0.0f)
			{
				DoneT = 0.0f;
			}
		}
	}

	void TickGrind(float Dt)
	{
		WarnT -= Dt;
		SparkT -= Dt;
		const float Inst = Dt > 0.0f ? FMath::Abs(FrameTurn) / (KGMgVillage::TwoPi * Dt) : 0.0f;
		FrameTurn = 0.0f;
		Speed = Approach(Speed, bGrab ? Inst : 0.0f, Dt, bGrab ? 5.0f : 2.5f);
		bInGreen = bGrab && Speed >= GreenLo && Speed <= GreenHi;
		FlowT = Approach(FlowT, bInGreen && DoneT < 0.0f ? 1.0f : 0.0f, Dt, 6.0f);
		if (GrindAcc > 1.4f)
		{
			GrindAcc = 0.0f;
			Sound(TEXT("S_Chore_Grind"), 0.5f, 0.8f + 0.35f * FMath::Min(Speed, SpeedMax));
		}
		if (DoneT >= 0.0f)
		{
			return;
		}
		if (bInGreen)
		{
			const float Before = Progress;
			Progress = FMath::Min(1.0f, Progress + Dt / GrindSeconds);
			if (Before < 0.5f && Progress >= 0.5f)
			{
				PopText(FVector2f(566.0f, 240.0f), TEXT("Halfway!"), Gold);
			}
		}
		else if (bGrab && Speed > GreenHi)
		{
			SparkT = 0.25f;
			if (WarnT <= 0.0f)
			{
				WarnT = 1.6f;
				Oops(StoneC + FVector2f(0.0f, -StoneR - 30.0f), TEXT("Too fast - sparks!"), TEXT("S_Chore_Scrape"), 3.0f);
			}
		}
		if (Progress >= 1.0f)
		{
			DoneT = 0.0f;
			bGrab = false;
			Nice(FVector2f(566.0f, 230.0f), TEXT("A full sack!"), TEXT("S_Chore_Pour"), 1.0f);
		}
	}

	void PaintMillRoom(FKGMgPainter& P) const
	{
		KGMgVillage::Planks(P, FVector2f(0.0f, 0.0f), FVector2f(W, 352.0f), C(0x7E5A3C), 10, true);
		P.RectV(FVector2f(0.0f, 0.0f), FVector2f(W, 352.0f), A(Ink, 0.15f), A(Ink, 0.0f));
		// Round window: a distant windmill turning on the hill.
		const FVector2f WinC(430.0f, 64.0f);
		P.Circle(WinC, 50.0f, WoodDark);
		P.Disc(WinC, FVector2f(42.0f, 42.0f), C(0xBFE6F8), C(0x8FD3F4));
		P.Quad(WinC + FVector2f(-42.0f, 18.0f), WinC + FVector2f(42.0f, 12.0f), WinC + FVector2f(34.0f, 26.0f), WinC + FVector2f(-34.0f, 26.0f), C(0x8CC66A));
		const FVector2f Hub = WinC + FVector2f(12.0f, -2.0f);
		P.Quad(Hub + FVector2f(-5.0f, 2.0f), Hub + FVector2f(5.0f, 2.0f), Hub + FVector2f(7.0f, 24.0f), Hub + FVector2f(-7.0f, 24.0f), Cream);
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2f D = KGMgVillage::Polar(Time * 1.2f + k * 0.5f * PI);
			P.Bar(Hub, Hub + D * 22.0f, 4.0f, C(0x6E4A2E));
			P.Bar(Hub + D * 8.0f + FVector2f(-D.Y, D.X) * 3.0f, Hub + D * 22.0f + FVector2f(-D.Y, D.X) * 3.0f, 6.0f, A(Cream, 0.9f));
		}
		P.Arc(WinC, 46.0f, 0.0f, KGMgVillage::TwoPi, WoodDark, 8.0f, 40);
		P.Bar(WinC + FVector2f(-46.0f, 0.0f), WinC + FVector2f(46.0f, 0.0f), 4.0f, WoodDark);
		P.Bar(WinC + FVector2f(0.0f, -46.0f), WinC + FVector2f(0.0f, 46.0f), 4.0f, WoodDark);
		// Main shaft + crown wheel.
		P.Rect(FVector2f(596.0f, 0.0f), FVector2f(28.0f, 352.0f), WoodDark);
		P.Rect(FVector2f(600.0f, 0.0f), FVector2f(5.0f, 352.0f), A(WoodLight, 0.5f));
		KGMgVillage::Gear(P, FVector2f(610.0f, 40.0f), 44.0f, 14, Time * 0.6f, C(0x8A6038));
		// Floor.
		KGMgVillage::Planks(P, FVector2f(0.0f, 352.0f), FVector2f(W, 48.0f), C(0x6E4A2E), 3, false);
		P.Rect(FVector2f(0.0f, 352.0f), FVector2f(W, 4.0f), WoodDark);
	}

	void PaintFill(FKGMgPainter& P) const
	{
		PaintMillRoom(P);
		// Loft shelf with brackets.
		P.Rect(FVector2f(10.0f, 196.0f), FVector2f(236.0f, 14.0f), WoodDark);
		P.Rect(FVector2f(10.0f, 196.0f), FVector2f(236.0f, 3.0f), A(WoodLight, 0.6f));
		P.Tri(FVector2f(30.0f, 210.0f), FVector2f(60.0f, 210.0f), FVector2f(30.0f, 250.0f), WoodDark);
		P.Tri(FVector2f(200.0f, 210.0f), FVector2f(230.0f, 210.0f), FVector2f(230.0f, 250.0f), WoodDark);

		// Hopper (cut-away) with its grain level.
		const float Level = FMath::Lerp(256.0f, 160.0f, Fill());
		const TArray<FVector2f> Inner = {FVector2f(334.0f, 152.0f), FVector2f(518.0f, 152.0f), FVector2f(452.0f, 260.0f), FVector2f(400.0f, 260.0f)};
		P.Poly(Inner, C(0x3A2616));
		const TArray<FVector2f> Heap = KGMgVillage::ClipBelow(Inner, Level);
		if (Heap.Num() >= 3 && Fill() > 0.01f)
		{
			P.Poly(Heap, Grain);
			float X0 = 1e9f;
			float X1 = -1e9f;
			for (const FVector2f& V : Heap)
			{
				if (FMath::Abs(V.Y - Level) < 0.5f)
				{
					X0 = FMath::Min(X0, V.X);
					X1 = FMath::Max(X1, V.X);
				}
			}
			if (X1 > X0)
			{
				P.Disc(FVector2f((X0 + X1) * 0.5f, Level), FVector2f((X1 - X0) * 0.42f, 7.0f), C(0xF0CE84), Grain);
			}
			for (int32 k = 0; k < 14; ++k)
			{
				const FVector2f Dot(410.0f + KGMgVillage::Hash01(k * 3) * 40.0f - 20.0f + (KGMgVillage::Hash01(k * 5) - 0.5f) * (X1 - X0) * 0.6f,
				                    Level + 6.0f + KGMgVillage::Hash01(k * 7) * (256.0f - Level));
				if (Dot.Y < 254.0f)
				{
					P.Circle(Dot, 2.0f, A(C(0xB8904A), 0.8f));
				}
			}
		}
		P.Bar(FVector2f(326.0f, 146.0f), FVector2f(398.0f, 266.0f), 12.0f, Wood);
		P.Bar(FVector2f(526.0f, 146.0f), FVector2f(454.0f, 266.0f), 12.0f, Wood);
		P.Bar(FVector2f(316.0f, 146.0f), FVector2f(536.0f, 146.0f), 10.0f, WoodDark);
		P.Rect(FVector2f(412.0f, 262.0f), FVector2f(28.0f, 24.0f), WoodDark);
		// Millstone tun + a waiting flour sack.
		P.RoundRect(FVector2f(300.0f, 284.0f), FVector2f(252.0f, 68.0f), Wood, 8.0f, WoodDark, 2.0f);
		for (int32 k = 1; k < 8; ++k)
		{
			P.Rect(FVector2f(300.0f + k * 31.5f, 286.0f), FVector2f(2.0f, 64.0f), A(Ink, 0.25f));
		}
		P.Rect(FVector2f(300.0f, 296.0f), FVector2f(252.0f, 5.0f), Iron);
		P.Rect(FVector2f(300.0f, 334.0f), FVector2f(252.0f, 5.0f), Iron);
		P.RoundRect(FVector2f(560.0f, 300.0f), FVector2f(60.0f, 56.0f), Burlap, 10.0f, C(0xA8804A), 1.5f);

		// The grain stream.
		const float Stream = StreamAlpha();
		const FVector2f Mouth = SackPt(-58.0f, -136.0f);
		if (Stream > 0.0f)
		{
			const FVector2f End(386.0f, Level);
			TArray<FVector2f> Line;
			for (int32 k = 0; k <= 10; ++k)
			{
				const float F = k / 10.0f;
				Line.Add(FVector2f(FMath::Lerp(Mouth.X, End.X, F), FMath::Lerp(Mouth.Y, End.Y, F * F)));
			}
			P.Line(Line, A(C(0xD8B060), 0.7f * Stream), 7.0f);
			for (int32 k = 0; k < 16; ++k)
			{
				const float F = FMath::Frac(Time * 2.6f + k / 16.0f);
				const FVector2f G(FMath::Lerp(Mouth.X, End.X, F) + (KGMgVillage::Hash01(k) - 0.5f) * 8.0f, FMath::Lerp(Mouth.Y, End.Y, F * F));
				P.Circle(G, 3.2f, A(Grain, Stream));
			}
			P.Glow(End, 30.0f, A(C(0xFFF0C0), 0.4f * Stream));
		}

		// The sack (tilts around its bottom corner to pour).
		if (Tilt() < 0.05f)
		{
			P.Disc(FVector2f(138.0f, 198.0f), FVector2f(64.0f, 6.0f), A(Ink, 0.35f), A(Ink, 0.0f));
		}
		const TArray<FVector2f> Body = {SackPt(-116.0f, -6.0f), SackPt(-110.0f, -104.0f), SackPt(-86.0f, -128.0f), SackPt(-30.0f, -128.0f),
		                                SackPt(-6.0f, -104.0f), SackPt(0.0f, -6.0f), SackPt(-8.0f, 0.0f), SackPt(-108.0f, 0.0f)};
		P.Poly(Body, Burlap);
		P.Poly({SackPt(-40.0f, -127.0f), SackPt(-30.0f, -128.0f), SackPt(-6.0f, -104.0f), SackPt(0.0f, -6.0f), SackPt(-8.0f, 0.0f), SackPt(-40.0f, 0.0f)},
		       A(C(0xA8804A), 0.55f));
		for (int32 k = 0; k < 9; ++k)
		{
			const float X = -106.0f + k * 12.0f;
			P.Segment(SackPt(X, -34.0f), SackPt(X + 6.0f, -34.0f), A(C(0x7A5230), 0.7f), 2.0f);
		}
		P.Bar(SackPt(-58.0f, -44.0f), SackPt(-58.0f, -94.0f), 3.0f, C(0x8C6A2A));
		for (int32 k = 0; k < 4; ++k)
		{
			const float Side = k % 2 ? 1.0f : -1.0f;
			P.Circle(SackPt(-58.0f + Side * 6.0f, -56.0f - k * 9.0f), 4.0f, C(0x8C6A2A));
		}
		P.Quad(SackPt(-80.0f, -126.0f), SackPt(-36.0f, -126.0f), SackPt(-44.0f, -146.0f), SackPt(-72.0f, -146.0f), C(0xC9A66B));
		if (Tilt() > 0.3f)
		{
			P.Circle(Mouth, 9.0f, Grain);
		}
		else
		{
			P.Bar(SackPt(-78.0f, -132.0f), SackPt(-38.0f, -132.0f), 5.0f, C(0x7A5230));
		}

		// Hints + counter.
		if (PourT < 0.0f && Pours < 3)
		{
			P.HintRing(SackPt(-58.0f, -70.0f), Pours == 0 ? 60.0f : 50.0f, Time, Pours == 0 ? Gold : A(Gold, 0.5f));
			if (Pours == 0)
			{
				P.HintArrow(FVector2f(214.0f, 96.0f), FVector2f(344.0f, 132.0f), Time, Gold);
			}
		}
		P.Tag(FVector2f(110.0f, 24.0f), FString::Printf(TEXT("POURS  %d / 3"), Pours), A(Ink, 0.75f), Cream, 14.0f);
		P.GaugeV(FVector2f(548.0f, 150.0f), FVector2f(24.0f, 110.0f), Fill(), Grain);
		P.Text(FVector2f(560.0f, 128.0f), TEXT("HOPPER"), 10.0f, Cream, 0.5f, TEXT("Bold"), 80);
	}

	void PaintGrind(FKGMgPainter& P) const
	{
		KGMgVillage::Planks(P, FVector2f(0.0f, 0.0f), FVector2f(W, H), C(0x9A6A42), 9, true);
		// Tun around the runner stone.
		P.Circle(StoneC + FVector2f(6.0f, 10.0f), StoneR + 30.0f, A(Ink, 0.35f));
		P.Circle(StoneC, StoneR + 30.0f, C(0x7A5234), WoodDark, 4.0f);
		for (int32 k = 0; k < 24; ++k)
		{
			const FVector2f D = KGMgVillage::Polar(k * KGMgVillage::TwoPi / 24.0f);
			P.Segment(StoneC + D * (StoneR + 8.0f), StoneC + D * (StoneR + 28.0f), A(Ink, 0.3f), 2.0f);
		}
		P.Arc(StoneC, StoneR + 26.0f, 0.0f, KGMgVillage::TwoPi, Iron, 4.0f, 64);
		P.Circle(StoneC, StoneR + 6.0f, C(0x2A1C12));
		P.ArcBand(StoneC, StoneR + 1.0f, StoneR + 20.0f, 0.0f, KGMgVillage::TwoPi, A(Flour, 0.15f + 0.6f * Progress), 48);
		// Chute to the sack.
		P.Bar(StoneC + FVector2f(StoneR + 20.0f, 34.0f), FVector2f(548.0f, 262.0f), 24.0f, WoodDark);
		P.Bar(StoneC + FVector2f(StoneR + 22.0f, 34.0f), FVector2f(548.0f, 262.0f), 14.0f, C(0x3A2616));

		// Runner stone with dressing furrows.
		P.Disc(StoneC, FVector2f(StoneR, StoneR), C(0xB4B0BA), C(0x7E7A86), 48);
		for (int32 k = 0; k < 10; ++k)
		{
			const float A0 = StoneAng + k * KGMgVillage::TwoPi / 10.0f;
			P.Segment(StoneC + KGMgVillage::Polar(A0) * 32.0f, StoneC + KGMgVillage::Polar(A0 + 0.42f) * (StoneR - 6.0f), A(Ink, 0.32f), 3.0f);
			P.Segment(StoneC + KGMgVillage::Polar(A0 + 0.2f) * 62.0f, StoneC + KGMgVillage::Polar(A0 + 0.5f) * (StoneR - 8.0f), A(Ink, 0.2f), 2.0f);
		}
		P.Arc(StoneC, StoneR - 3.0f, -2.6f, -1.0f, A(FLinearColor::White, 0.3f), 3.0f, 20);
		P.ArcBand(StoneC, StoneR * 0.4f, StoneR, 0.0f, KGMgVillage::TwoPi, A(Flour, 0.25f * Progress), 40);
		P.Circle(StoneC, 26.0f, C(0x3A2E26));
		for (int32 k = 0; k < 7; ++k)
		{
			P.Circle(StoneC + KGMgVillage::Polar(StoneAng * 0.5f + k * 0.9f) * (6.0f + 12.0f * KGMgVillage::Hash01(k)), 3.0f, Grain);
		}
		P.Bar(StoneC - KGMgVillage::Polar(StoneAng) * 24.0f, StoneC + KGMgVillage::Polar(StoneAng) * 24.0f, 8.0f, Iron);
		const FVector2f Hp = HandleP();
		P.Circle(Hp + FVector2f(4.0f, 7.0f), 17.0f, A(Ink, 0.35f));
		P.Circle(Hp, 17.0f, bGrab ? Gold : WoodLight, WoodDark, 3.0f);
		P.Circle(Hp + FVector2f(-5.0f, -5.0f), 6.0f, A(FLinearColor::White, 0.35f));

		// Sparks when turning too fast.
		if (SparkT > 0.0f)
		{
			const int32 Seed = FMath::FloorToInt(Time * 24.0f);
			for (int32 k = 0; k < 10; ++k)
			{
				const float Ang = KGMgVillage::Hash01(Seed * 13 + k) * KGMgVillage::TwoPi;
				const FVector2f Base = StoneC + KGMgVillage::Polar(Ang) * (StoneR + 2.0f);
				const FVector2f Tip = Base + KGMgVillage::Polar(Ang + 0.5f) * (10.0f + 18.0f * KGMgVillage::Hash01(Seed * 7 + k));
				P.Bar(Base, Tip, 2.5f, Gold);
				P.Circle(Tip, 2.0f, C(0xFFF4C8));
			}
			P.Glow(StoneC, StoneR + 40.0f, A(Lantern, 0.12f));
		}

		// Flour sack filling up.
		const float Bulge = 8.0f * Progress;
		P.Disc(FVector2f(566.0f, 392.0f), FVector2f(64.0f, 8.0f), A(Ink, 0.35f), A(Ink, 0.0f));
		P.RoundRect(FVector2f(514.0f - Bulge, 262.0f), FVector2f(104.0f + Bulge * 2.0f, 128.0f), Burlap, 18.0f, C(0xA8804A), 2.0f);
		P.Rect(FVector2f(574.0f, 270.0f), FVector2f(30.0f, 112.0f), A(C(0xA8804A), 0.4f));
		P.Text(FVector2f(566.0f, 322.0f), TEXT("FLOUR"), 13.0f, C(0x7A5230), 0.5f, TEXT("Black"), 60);
		P.Disc(FVector2f(566.0f, 264.0f), FVector2f(50.0f, 10.0f), C(0x5A3E24), C(0x3A2616));
		if (Progress > 0.02f)
		{
			P.Disc(FVector2f(566.0f, 264.0f - 12.0f * Progress), FVector2f(20.0f + 26.0f * Progress, 4.0f + 12.0f * Progress), C(0xFFFFFF), Flour);
		}
		if (FlowT > 0.02f)
		{
			for (int32 k = 0; k < 8; ++k)
			{
				const float F = FMath::Frac(Time * 3.0f + k / 8.0f);
				P.Circle(FVector2f(548.0f + 14.0f * F + (KGMgVillage::Hash01(k) - 0.5f) * 8.0f, 262.0f + 10.0f * F), 3.0f + 3.0f * F,
				         A(Flour, FlowT * (1.0f - F)));
			}
			P.Glow(FVector2f(560.0f, 262.0f), 30.0f, A(Flour, 0.35f * FlowT));
		}

		// Speed dial + progress.
		const FVector2f G(548.0f, 112.0f);
		const float R0 = 46.0f;
		const float R1 = 60.0f;
		P.RoundRect(G - FVector2f(76.0f, 76.0f), FVector2f(152.0f, 172.0f), A(Ink, 0.62f), 14.0f);
		P.ArcBand(G, R0, R1, -PI, 0.0f, A(Cream, 0.18f), 30);
		const float Ag0 = -PI + PI * GreenLo / SpeedMax;
		const float Ag1 = -PI + PI * GreenHi / SpeedMax;
		P.ArcBand(G, R0, R1, Ag0, Ag1, A(Good, 0.9f), 16);
		P.ArcBand(G, R0, R1, Ag1, 0.0f, A(Crimson, 0.85f), 12);
		const float Na = -PI + PI * Saturate(Speed / SpeedMax);
		P.Bar(G, G + KGMgVillage::Polar(Na) * (R1 + 4.0f), 4.0f, Cream);
		P.Circle(G, 7.0f, Cream);
		FString Status = TEXT("TURN IT");
		FLinearColor StatusC = CreamDim;
		if (DoneT >= 0.0f)
		{
			Status = TEXT("DONE!");
			StatusC = Good;
		}
		else if (bGrab && Speed > GreenHi)
		{
			Status = TEXT("TOO FAST");
			StatusC = C(0xFF8A80);
		}
		else if (bInGreen)
		{
			Status = TEXT("STEADY!");
			StatusC = Good;
		}
		else if (bGrab)
		{
			Status = TEXT("FASTER");
			StatusC = Gold;
		}
		P.Text(G + FVector2f(0.0f, 10.0f), Status, 14.0f, StatusC, 0.5f, TEXT("Black"));
		P.Text(G + FVector2f(0.0f, 34.0f), FString::Printf(TEXT("FLOUR  %d%%"), FMath::RoundToInt(Progress * 100.0f)), 11.0f, Cream, 0.5f,
		       TEXT("Bold"), 60);
		P.Gauge(G + FVector2f(-64.0f, 52.0f), FVector2f(128.0f, 16.0f), Progress, Flour);

		if (!bGrab && Progress < 0.02f && DoneT < 0.0f)
		{
			P.HintRing(Hp, 26.0f, Time, Gold);
			KGMgVillage::Chevrons(P, StoneC, StoneR + 44.0f, Time, true, A(Cream, 0.75f));
		}
	}
};

} // namespace KGMg


TUniquePtr<FKGMinigame> KGMakeMinigame_PostNotice() { return MakeUnique<KGMg::FKGMgPostNotice>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_FileReports() { return MakeUnique<KGMg::FKGMgFileReports>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_LightCandles() { return MakeUnique<KGMg::FKGMgLightCandles>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_TendGraves() { return MakeUnique<KGMg::FKGMgTendGraves>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_WindClock() { return MakeUnique<KGMg::FKGMgWindClock>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_GrindFlour() { return MakeUnique<KGMg::FKGMgGrindFlour>(); }
