// Chore minigames (Craft): ForgeNails, SharpenTools, BakeBread, PourAle, StockStall, FeedAnimals.
// Smithy, bakery, inn, market and farm chores. See KGMinigame.h for the contract, KGMinigames_Core.cpp for the reference set.
// Anti-cheat: every stage has a built-in minimum duration (fixed animations, speed caps, locks) above the server floor * 0.9.
#include "Chores/UI/KGMinigame.h"

namespace KGMg
{
namespace KGMgCraft
{
	FVector2f Rot(const FVector2f& P, float Ang)
	{
		const float Cs = FMath::Cos(Ang);
		const float Sn = FMath::Sin(Ang);
		return FVector2f(P.X * Cs - P.Y * Sn, P.X * Sn + P.Y * Cs);
	}

	/** Frame-rate independent exponential approach for points. */
	FVector2f ApproachV(const FVector2f& Current, const FVector2f& Target, float Dt, float Speed)
	{
		return FMath::Lerp(Current, Target, 1.0f - FMath::Exp(-Speed * FMath::Max(Dt, 0.0f)));
	}

	/** Keeps the part of a convex polygon below the horizontal line y = SurfaceY (screen y grows downwards). */
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

	float PolyArea(const TArray<FVector2f>& Poly)
	{
		float Sum = 0.0f;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2f& P0 = Poly[i];
			const FVector2f& P1 = Poly[(i + 1) % Poly.Num()];
			Sum += P0.X * P1.Y - P1.X * P0.Y;
		}
		return FMath::Abs(Sum) * 0.5f;
	}

	/** World y of a horizontal liquid surface such that the part of the convex Poly below it has area Target. */
	float LevelForArea(const TArray<FVector2f>& Poly, float Target)
	{
		float Top = 1.0e6f;
		float Bottom = -1.0e6f;
		for (const FVector2f& V : Poly)
		{
			Top = FMath::Min(Top, V.Y);
			Bottom = FMath::Max(Bottom, V.Y);
		}
		float Full = Top;
		float Empty = Bottom;
		for (int32 i = 0; i < 22; ++i)
		{
			const float Mid = (Full + Empty) * 0.5f;
			if (PolyArea(ClipBelow(Poly, Mid)) > Target)
			{
				Full = Mid;
			}
			else
			{
				Empty = Mid;
			}
		}
		return (Full + Empty) * 0.5f;
	}

	/** Lowest point (largest y) where the vertical line x = X crosses the convex polygon, or Fallback. */
	float VerticalHit(const TArray<FVector2f>& Poly, float X, float Fallback)
	{
		float Best = -1.0e6f;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2f& P0 = Poly[i];
			const FVector2f& P1 = Poly[(i + 1) % Poly.Num()];
			if ((P0.X - X) * (P1.X - X) <= 0.0f && FMath::Abs(P1.X - P0.X) > 1e-3f)
			{
				Best = FMath::Max(Best, FMath::Lerp(P0.Y, P1.Y, (X - P0.X) / (P1.X - P0.X)));
			}
		}
		return Best > -1.0e5f ? Best : Fallback;
	}

	/** Points on an ellipse between parametric angles A0..A1 (a half-ellipse is a valid convex polygon). */
	TArray<FVector2f> EllipsePts(const FVector2f& Ctr, const FVector2f& R, int32 N, float A0 = 0.0f, float A1 = UE_TWO_PI)
	{
		TArray<FVector2f> Pts;
		Pts.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i)
		{
			const float T = FMath::Lerp(A0, A1, float(i) / float(N));
			Pts.Add(Ctr + FVector2f(FMath::Cos(T) * R.X, FMath::Sin(T) * R.Y));
		}
		return Pts;
	}

	/** Soft sky + distant hills backdrop used by outdoor chores. */
	void Outdoor(FKGMgPainter& P, float GroundY, const FLinearColor& SkyTop = C(0x7EC8F0), const FLinearColor& SkyLow = C(0xE9F4F2))
	{
		P.RectV(FVector2f(0, 0), FVector2f(W, GroundY), SkyTop, SkyLow);
		P.Circle(FVector2f(90.0f, 48.0f), 22.0f, A(C(0xFFF4C8), 0.9f));
		P.Glow(FVector2f(90.0f, 48.0f), 70.0f, A(C(0xFFF0B0), 0.45f));
		TArray<FVector2f> Hills;
		for (int32 i = 0; i <= 16; ++i)
		{
			const float X = W * float(i) / 16.0f;
			Hills.Add(FVector2f(X, GroundY - 34.0f - 18.0f * FMath::Sin(float(i) * 0.9f) - 10.0f * FMath::Sin(float(i) * 2.3f)));
		}
		for (int32 i = 0; i + 1 < Hills.Num(); ++i)
		{
			P.Quad(Hills[i], Hills[i + 1], FVector2f(Hills[i + 1].X, GroundY), FVector2f(Hills[i].X, GroundY), C(0x8CC66A));
		}
	}

	/** Wooden boards with seams, knots and grain (vertical or horizontal planks). */
	void Planks(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, float Pitch, const FLinearColor& Top,
	            const FLinearColor& Bottom, bool bVertical)
	{
		P.RectV(Pos, Size, Top, Bottom);
		const FLinearColor Seam = A(Ink, 0.35f);
		const FLinearColor Grain = A(Ink, 0.12f);
		int32 k = 0;
		if (bVertical)
		{
			for (float X = Pos.X + Pitch; X < Pos.X + Size.X - 1.0f; X += Pitch, ++k)
			{
				P.Rect(FVector2f(X, Pos.Y), FVector2f(2.0f, Size.Y), Seam);
				P.Rect(FVector2f(X + 2.0f, Pos.Y), FVector2f(1.0f, Size.Y), A(Cream, 0.07f));
				const float GX = X - Pitch * 0.5f;
				const float GY = Pos.Y + 20.0f + FMath::Fmod(float(k) * 71.0f + 30.0f, FMath::Max(Size.Y - 40.0f, 1.0f));
				P.Disc(FVector2f(GX, GY), FVector2f(3.5f, 7.0f), A(Ink, 0.25f), A(Ink, 0.0f), 12);
				P.Segment(FVector2f(GX - Pitch * 0.22f, Pos.Y + 4.0f), FVector2f(GX - Pitch * 0.18f, Pos.Y + Size.Y - 4.0f), Grain, 1.0f);
				P.Segment(FVector2f(GX + Pitch * 0.2f, Pos.Y + 4.0f), FVector2f(GX + Pitch * 0.24f, Pos.Y + Size.Y - 4.0f), Grain, 1.0f);
			}
		}
		else
		{
			for (float Y = Pos.Y + Pitch; Y < Pos.Y + Size.Y + Pitch - 1.0f; Y += Pitch, ++k)
			{
				if (Y < Pos.Y + Size.Y - 1.0f)
				{
					P.Rect(FVector2f(Pos.X, Y), FVector2f(Size.X, 2.0f), Seam);
					P.Rect(FVector2f(Pos.X, Y + 2.0f), FVector2f(Size.X, 1.0f), A(Cream, 0.07f));
				}
				const float GY = Y - Pitch * 0.5f;
				const float GX = Pos.X + 40.0f + FMath::Fmod(float(k) * 173.0f + 60.0f, FMath::Max(Size.X - 80.0f, 1.0f));
				P.Disc(FVector2f(GX, GY), FVector2f(8.0f, 3.5f), A(Ink, 0.22f), A(Ink, 0.0f), 12);
				P.Segment(FVector2f(Pos.X + 6.0f, GY - Pitch * 0.22f), FVector2f(Pos.X + Size.X - 6.0f, GY - Pitch * 0.18f), Grain, 1.0f);
				P.Segment(FVector2f(Pos.X + 6.0f, GY + Pitch * 0.2f), FVector2f(Pos.X + Size.X - 6.0f, GY + Pitch * 0.24f), Grain, 1.0f);
			}
		}
	}

	/** A painted particle: embers, sparks, flour, grain, smoke, drops. */
	struct FBit
	{
		FVector2f P = FVector2f::ZeroVector;
		FVector2f V = FVector2f::ZeroVector;
		float Age = 0.0f;
		float Life = 1.0f;
		float Size = 3.0f;
		float Grow = 0.0f;   // > 0: radius grows over life (smoke puffs)
		FLinearColor Col = FLinearColor::White;
		bool bStreak = false;   // sparks: short line along the velocity
	};

	void Burst(TArray<FBit>& Bits, FKGRng& Rand, const FVector2f& At, int32 Count, float Ang0, float Ang1, float Speed0, float Speed1,
	           const FLinearColor& Col0, const FLinearColor& Col1, float Size, float Life, bool bStreak)
	{
		for (int32 i = 0; i < Count && Bits.Num() < 400; ++i)
		{
			const float Ang = FMath::Lerp(Ang0, Ang1, Rand.FRand());
			const float Spd = FMath::Lerp(Speed0, Speed1, Rand.FRand());
			FBit& B = Bits.AddDefaulted_GetRef();
			B.P = At;
			B.V = FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Spd;
			B.Life = Life * (0.6f + 0.4f * Rand.FRand());
			B.Size = Size * (0.7f + 0.6f * Rand.FRand());
			B.Col = Mix(Col0, Col1, Rand.FRand());
			B.bStreak = bStreak;
		}
	}

	void TickBits(TArray<FBit>& Bits, float Dt, float Gravity, float FloorY = 1.0e6f)
	{
		for (int32 i = Bits.Num() - 1; i >= 0; --i)
		{
			FBit& B = Bits[i];
			B.Age += Dt;
			B.V.Y += Gravity * Dt;
			B.P += B.V * Dt;
			if (B.P.Y > FloorY)
			{
				B.P.Y = FloorY;
				B.V = FVector2f::ZeroVector;
			}
			if (B.Age >= B.Life)
			{
				Bits.RemoveAtSwap(i);
			}
		}
	}

	void PaintBits(FKGMgPainter& P, const TArray<FBit>& Bits)
	{
		for (const FBit& B : Bits)
		{
			const float T = Saturate(B.Age / FMath::Max(B.Life, 0.01f));
			const float K = 1.0f - T;
			if (B.bStreak)
			{
				P.Segment(B.P - B.V * 0.035f, B.P, A(B.Col, K), B.Size);
			}
			else if (B.Grow > 0.0f)
			{
				P.Circle(B.P, B.Size * (1.0f + B.Grow * T), A(B.Col, K));
			}
			else
			{
				P.Circle(B.P, B.Size * (0.5f + 0.5f * K), A(B.Col, FMath::Min(1.0f, K * 1.6f)));
			}
		}
	}

	/** Four-point glint star. */
	void Sparkle(FKGMgPainter& P, const FVector2f& At, float R, const FLinearColor& Col)
	{
		P.Glow(At, R * 1.4f, A(Col, 0.35f));
		P.Quad(At + FVector2f(0.0f, -R), At + FVector2f(R * 0.2f, 0.0f), At + FVector2f(0.0f, R), At + FVector2f(-R * 0.2f, 0.0f), Col);
		P.Quad(At + FVector2f(-R, 0.0f), At + FVector2f(0.0f, -R * 0.2f), At + FVector2f(R, 0.0f), At + FVector2f(0.0f, R * 0.2f), Col);
	}

	void Heart(FKGMgPainter& P, const FVector2f& At, float R, const FLinearColor& Col)
	{
		P.Circle(At + FVector2f(-R * 0.5f, -R * 0.2f), R * 0.56f, Col);
		P.Circle(At + FVector2f(R * 0.5f, -R * 0.2f), R * 0.56f, Col);
		P.Tri(At + FVector2f(-R * 1.03f, -R * 0.08f), At + FVector2f(R * 1.03f, -R * 0.08f), At + FVector2f(0.0f, R * 1.1f), Col);
	}

	/** Row of progress pips (fractional fill allowed). */
	void Pips(FKGMgPainter& P, const FVector2f& Left, int32 Total, float Done, float Radius, const FLinearColor& On)
	{
		for (int32 i = 0; i < Total; ++i)
		{
			const FVector2f At = Left + FVector2f(float(i) * Radius * 2.6f, 0.0f);
			const float Fill = Saturate(Done - float(i));
			P.Circle(At, Radius, A(Ink, 0.6f), A(Cream, 0.35f), 1.5f);
			if (Fill > 0.01f)
			{
				P.Circle(At, (Radius - 1.5f) * (0.35f + 0.65f * Fill), On);
			}
		}
	}

	void Coin(FKGMgPainter& P, const FVector2f& At, float R)
	{
		P.Circle(At, R, C(0xF2C230), C(0xA8781A), FMath::Max(1.5f, R * 0.16f));
		P.Arc(At, R * 0.6f, 0.0f, UE_TWO_PI, A(C(0xA8781A), 0.7f), 1.2f, 16);
		P.Arc(At, R * 0.72f, -2.6f, -1.4f, A(FLinearColor::White, 0.7f), 1.5f, 8);
	}

	/** Market goods: 0 fish, 1 apple, 2 bread, 3 cheese, 4 honey jar, 5 carrot (about 52 x 44 units at S = 1). */
	void Ware(FKGMgPainter& P, int32 Kind, const FVector2f& Ctr, float S)
	{
		auto At = [&Ctr, S](float X, float Y) { return Ctr + FVector2f(X, Y) * S; };
		switch (Kind % 6)
		{
		case 0:
			P.Tri(At(12.0f, 0.0f), At(27.0f, -13.0f), At(27.0f, 13.0f), C(0x3F78A8));
			P.Tri(At(-6.0f, -8.0f), At(8.0f, -8.0f), At(2.0f, -17.0f), C(0x4F86B8));
			P.Disc(At(-4.0f, 0.0f), FVector2f(21.0f, 11.5f) * S, C(0xB4DCF2), C(0x5B93C2), 20);
			P.Bar(At(-18.0f, 4.0f), At(8.0f, 4.0f), 3.0f * S, A(Cream, 0.55f));
			P.Arc(At(-9.0f, 0.0f), 7.0f * S, -0.9f, 0.9f, A(C(0x2E5E88), 0.8f), 1.5f * S, 8);
			P.Circle(At(-15.0f, -2.0f), 3.2f * S, Cream);
			P.Circle(At(-15.5f, -2.0f), 1.7f * S, Ink);
			break;
		case 1:
			P.Disc(At(0.0f, 3.0f), FVector2f(17.0f, 15.5f) * S, C(0xF2604A), C(0xB02020), 20);
			P.Circle(At(-7.0f, -3.0f), 4.5f * S, A(C(0xFFD8C8), 0.75f));
			P.Bar(At(0.0f, -10.0f), At(2.5f, -18.0f), 3.0f * S, WoodDark);
			P.RotRect(At(8.0f, -15.0f), FVector2f(11.0f, 5.5f) * S, -0.5f, C(0x6DB33F));
			break;
		case 2:
			P.RoundRect(At(-23.0f, -11.0f), FVector2f(46.0f, 24.0f) * S, C(0xD9923E), -1.0f, C(0x9A5A22), 2.0f * S);
			P.RoundRect(At(-17.0f, -9.0f), FVector2f(34.0f, 7.0f) * S, A(C(0xF5C57A), 0.85f), -1.0f);
			for (int32 i = 0; i < 3; ++i)
			{
				const float X = -10.0f + float(i) * 10.0f;
				P.Segment(At(X - 3.0f, 4.0f), At(X + 4.0f, -5.0f), A(C(0x8A4A1A), 0.85f), 2.0f * S);
			}
			break;
		case 3:
			P.Rect(At(-22.0f, -2.0f), FVector2f(44.0f, 14.0f) * S, C(0xF2C24A));
			P.Tri(At(-22.0f, -2.0f), At(22.0f, -15.0f), At(22.0f, -2.0f), C(0xFFE27A));
			P.Rect(At(-22.0f, 10.0f), FVector2f(44.0f, 2.0f) * S, C(0xC9962A));
			P.Circle(At(-10.0f, 5.0f), 3.0f * S, C(0xD9A530));
			P.Circle(At(5.0f, 3.0f), 3.8f * S, C(0xD9A530));
			P.Circle(At(15.0f, 8.0f), 2.2f * S, C(0xD9A530));
			P.Circle(At(12.0f, -6.0f), 2.0f * S, C(0xE8B840));
			break;
		case 4:
			P.RoundRect(At(-15.0f, -10.0f), FVector2f(30.0f, 29.0f) * S, C(0xE8A020), 8.0f * S, C(0xB06A10), 2.0f * S);
			P.Bar(At(-9.0f, -4.0f), At(-9.0f, 12.0f), 3.0f * S, A(FLinearColor::White, 0.45f));
			P.RoundRect(At(-18.0f, -19.0f), FVector2f(36.0f, 11.0f) * S, C(0xC8403A), 3.0f * S);
			P.Bar(At(-15.0f, -9.5f), At(15.0f, -9.5f), 2.0f * S, Cream);
			P.RoundRect(At(-7.0f, -1.0f), FVector2f(16.0f, 11.0f) * S, Paper, 2.0f * S);
			break;
		default:
			P.Tri(At(-2.0f, -9.0f), At(2.0f, -9.0f), At(-9.0f, -23.0f), C(0x5AA33A));
			P.Tri(At(-2.0f, -9.0f), At(2.0f, -9.0f), At(0.0f, -25.0f), C(0x6DB33F));
			P.Tri(At(-2.0f, -9.0f), At(2.0f, -9.0f), At(9.0f, -22.0f), C(0x5AA33A));
			P.Tri(At(-10.0f, -9.0f), At(10.0f, -9.0f), At(0.0f, 22.0f), C(0xF28C28));
			P.Tri(At(-10.0f, -9.0f), At(-3.0f, -9.0f), At(0.0f, 20.0f), A(C(0xFFB060), 0.7f));
			P.Segment(At(-6.0f, -1.0f), At(0.0f, 0.0f), A(C(0xB85A18), 0.9f), 1.5f * S);
			P.Segment(At(2.0f, 7.0f), At(5.0f, 6.0f), A(C(0xB85A18), 0.9f), 1.5f * S);
			break;
		}
	}

	void Caption(FKGMgPainter& P, const FVector2f& Pos, const FString& Str, const FLinearColor& Col, float Align = 0.0f)
	{
		P.Text(Pos, Str, 10.0f, Col, Align, TEXT("Bold"), 100);
	}

	void HudPanel(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size)
	{
		// Near-opaque so tool handles and props behind never read through the numbers.
		P.RoundRect(Pos, Size, A(Night, 0.9f), 10.0f, A(Cream, 0.18f), 1.5f);
	}

	/** Dark stone smithy wall with a warm forge light and a packed-earth floor. */
	void SmithyWall(FKGMgPainter& P, float Warm)
	{
		P.RectV(FVector2f(0, 0), FVector2f(W, H), C(0x46343A), C(0x221A1E));
		for (int32 Row = 0; Row < 12; ++Row)
		{
			for (int32 Col = 0; Col < 11; ++Col)
			{
				const float X = float(Col) * 64.0f - float(Row % 2) * 32.0f;
				P.RoundRect(FVector2f(X + 2.0f, float(Row) * 30.0f + 2.0f), FVector2f(60.0f, 26.0f),
				            A(C(0x5E4A52), 0.3f + 0.1f * float((Row * 5 + Col * 3) % 3)), 5.0f);
			}
		}
		P.Glow(FVector2f(220.0f, 250.0f), 440.0f, A(C(0xFF8A3A), 0.1f + 0.22f * Warm));
		P.RectV(FVector2f(0, 350.0f), FVector2f(W, 50.0f), C(0x3E3029), C(0x1F1714));
		P.Rect(FVector2f(0, 350.0f), FVector2f(W, 3.0f), A(C(0x7A5A48), 0.8f));
	}
}

// =====================================================================================================================
// ForgeNails: pump the bellows to hold the forge heat in a drifting band, then strike the glowing nail on the mark.
// =====================================================================================================================
class FKGMgForgeNails final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Click the bellows to pump air: keep the heat in the green band until the iron glows")
		                  : TEXT("Click when the marker is in the green: 4 clean blows shape the nail");
	}

	virtual void BeginStage() override
	{
		Bits.Reset();
		Heat = 0.22f;
		CoalGlow = 0.22f;
		Progress = 0.0f;
		Open = 1.0f;
		PumpT = -1.0f;
		PumpFrom = 0.0f;
		Pumps = 0;
		bTooHot = false;
		EmberAcc = 0.0f;
		BandPhase = Rng.FRand() * 6.28f;
		Blows = 0;
		StrikeT = -1.0f;
		ShapeT = -1.0f;
		ReheatT = 0.0f;
		Temp = 1.0f;
		bPendingGood = false;
		bImpactDone = true;
		NewZone();
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			if (Pos.X > 372.0f)
			{
				Pump();
			}
		}
		else
		{
			Strike();
		}
	}

	virtual void OnKey(const FKey& Key) override
	{
		if (Key == EKeys::SpaceBar)
		{
			if (Stage == 0)
			{
				Pump();
			}
			else
			{
				Strike();
			}
		}
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickHeat(Dt);
			KGMgCraft::TickBits(Bits, Dt, -40.0f);
		}
		else
		{
			TickHammer(Dt);
			KGMgCraft::TickBits(Bits, Dt, 900.0f, 346.0f);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			if (PumpT < 0.0f && Open > 0.85f && Heat < BandCentre() - 0.02f)
			{
				Pump();
			}
		}
		else
		{
			const float Mid = (Z0 + Z1) * 0.5f;
			if (FMath::Abs(Marker - Mid) < (Z1 - Z0) * 0.25f)
			{
				Strike();
			}
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			StageTime = 4.0f;
			Heat = BandCentre() + 0.03f;
			CoalGlow = Heat;
			Progress = 0.55f;
			Open = 0.5f;
			Pumps = 4;
			PumpT = -1.0f;
			for (int32 i = 0; i < 40; ++i)
			{
				SpawnEmber();
				KGMgCraft::TickBits(Bits, 0.04f, -40.0f);
			}
		}
		else
		{
			Blows = 2;
			Marker = Z0 + (Z1 - Z0) * 0.4f;
			MarkerDir = 1.0f;
			Temp = 0.85f;
			StrikeT = -1.0f;
			ShapeT = -1.0f;
			KGMgCraft::Burst(Bits, Rng, NailTop(), 16, -PI, 0.0f, 180.0f, 420.0f, C(0xFFF1B0), Fire, 2.5f, 0.6f, true);
			KGMgCraft::TickBits(Bits, 0.08f, 900.0f, 346.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintHeat(P);
		}
		else
		{
			PaintHammer(P);
		}
	}

private:
	static constexpr float HotLine = 0.9f;
	static constexpr float BandHalf = 0.1f;
	static constexpr float InBandSeconds = 7.5f;   // min stage time: 7.5 s (floor 5.0)
	static constexpr float PumpTime = 0.2f;
	static constexpr float RefillTime = 0.5f;
	static constexpr float PumpGain = 0.14f;
	static constexpr int32 BlowsNeeded = 4;
	static constexpr float ImpactAt = 0.1f;
	static constexpr float StrikeTime = 0.45f;
	static constexpr float ShapeLock = 0.6f;   // min stage time: ~3.6 s (floor 3.0)
	static constexpr float ColdLine = 0.32f;
	static constexpr float ReheatTime = 1.4f;

	TArray<KGMgCraft::FBit> Bits;
	// Heat
	float Heat = 0.0f;
	float CoalGlow = 0.0f;
	float Progress = 0.0f;
	float Open = 1.0f;
	float PumpT = -1.0f;
	float PumpFrom = 0.0f;
	float EmberAcc = 0.0f;
	float BandPhase = 0.0f;
	int32 Pumps = 0;
	bool bTooHot = false;
	// Hammer
	int32 Blows = 0;
	float Marker = 0.0f;
	float MarkerDir = 1.0f;
	float Z0 = 0.4f;
	float Z1 = 0.55f;
	float StrikeT = -1.0f;
	float ShapeT = -1.0f;
	float ReheatT = 0.0f;
	float Temp = 1.0f;
	bool bPendingGood = false;
	bool bImpactDone = true;

	// ---- heat ---------------------------------------------------------------------------------------------------------

	float BandCentre() const
	{
		return 0.6f + 0.12f * FMath::Sin(0.55f * StageTime + BandPhase) + 0.04f * FMath::Sin(1.7f * StageTime + BandPhase * 2.0f);
	}

	bool InBand() const
	{
		const float Ctr = BandCentre();
		return Heat >= Ctr - BandHalf && Heat <= Ctr + BandHalf;
	}

	FVector2f Hinge() const { return FVector2f(404.0f, 262.0f); }
	float TopAngle() const { return -(0.08f + 0.3f * Open); }
	FVector2f TopEnd() const { return Hinge() + KGMgCraft::Rot(FVector2f(180.0f, 0.0f), TopAngle()); }
	FVector2f HandleEnd() const { return TopEnd() + KGMgCraft::Rot(FVector2f(28.0f, 0.0f), TopAngle()); }

	void Pump()
	{
		if (Stage != 0 || bSolved || PumpT >= 0.0f)
		{
			return;
		}
		PumpFrom = Open;
		PumpT = 0.0f;
		++Pumps;
		Sound(TEXT("S_Chore_Bellows"), 0.45f + 0.55f * Open, 0.85f + 0.3f * Open);
		if (Open < 0.4f)
		{
			PopText(HandleEnd() + FVector2f(-40.0f, -46.0f), TEXT("Let it fill!"), Cream);
		}
		KGMgCraft::Burst(Bits, Rng, FVector2f(300.0f, 240.0f), 4 + FMath::RoundToInt(8.0f * Open), -2.4f, -0.7f, 60.0f, 160.0f, Gold, Fire,
		                 3.0f, 1.0f, false);
	}

	void SpawnEmber()
	{
		if (Bits.Num() >= 200)
		{
			return;
		}
		KGMgCraft::FBit& B = Bits.AddDefaulted_GetRef();
		B.P = FVector2f(120.0f + Rng.FRand() * 200.0f, 236.0f);
		B.V = FVector2f(-20.0f + Rng.FRand() * 40.0f, -50.0f - Rng.FRand() * 70.0f);
		B.Life = 0.9f + Rng.FRand() * 0.8f;
		B.Size = 1.8f + Rng.FRand() * 1.6f;
		B.Col = Mix(Fire, Gold, Rng.FRand());
	}

	void TickHeat(float Dt)
	{
		// A press squeezes the bellows shut over PumpTime (air = how open it was), then it breathes open again.
		if (PumpT >= 0.0f)
		{
			const float K0 = Saturate(PumpT / PumpTime);
			PumpT += Dt;
			const float K1 = Saturate(PumpT / PumpTime);
			Open = PumpFrom * (1.0f - K1);
			if (!bSolved)
			{
				Heat = Saturate(Heat + PumpGain * PumpFrom * (K1 - K0));
			}
			if (PumpT >= PumpTime)
			{
				PumpT = -1.0f;
			}
		}
		else
		{
			Open = FMath::Min(1.0f, Open + Dt / RefillTime);
		}
		if (!bSolved)
		{
			Heat = Saturate(Heat - Dt * (0.07f + 0.06f * Heat));
			if (Heat > HotLine)
			{
				Progress = FMath::Max(0.0f, Progress - Dt * 0.22f);
				if (!bTooHot)
				{
					bTooHot = true;
					Oops(FVector2f(170.0f, 110.0f), TEXT("Too hot! Ease off"), TEXT("S_UI_Bad"), 4.0f);
				}
			}
			else if (InBand())
			{
				const float Before = Progress;
				Progress = FMath::Min(1.0f, Progress + Dt / InBandSeconds);
				if (FMath::FloorToInt(Before * 4.0f) != FMath::FloorToInt(Progress * 4.0f) && Progress < 1.0f)
				{
					Sound(TEXT("S_Chore_Flame"), 0.5f, 0.9f + 0.2f * Progress);
				}
			}
			if (Heat < HotLine - 0.08f)
			{
				bTooHot = false;
			}
			if (Progress >= 1.0f)
			{
				Nice(FVector2f(230.0f, 150.0f), TEXT("Glowing hot!"), TEXT("S_Chore_Flame"), 1.1f);
				Solve();
			}
		}
		CoalGlow = Approach(CoalGlow, Heat, Dt, 5.0f);
		EmberAcc += Dt * (2.0f + 8.0f * CoalGlow);
		while (EmberAcc >= 1.0f)
		{
			EmberAcc -= 1.0f;
			SpawnEmber();
		}
	}

	void PaintHeat(FKGMgPainter& P) const
	{
		KGMgCraft::SmithyWall(P, CoalGlow);

		// Chimney hood.
		P.Quad(FVector2f(92.0f, 150.0f), FVector2f(348.0f, 150.0f), FVector2f(292.0f, 0.0f), FVector2f(148.0f, 0.0f), C(0x4D3E40));
		for (int32 Row = 1; Row < 5; ++Row)
		{
			const float Y = float(Row) * 30.0f;
			const float Inset = 56.0f * (1.0f - Y / 150.0f);
			P.Segment(FVector2f(92.0f + Inset, Y), FVector2f(348.0f - Inset, Y), A(Ink, 0.35f), 2.0f);
		}
		P.Quad(FVector2f(92.0f, 150.0f), FVector2f(104.0f, 150.0f), FVector2f(160.0f, 0.0f), FVector2f(148.0f, 0.0f), A(Cream, 0.08f));
		P.Glow(FVector2f(220.0f, 150.0f), 140.0f, A(Fire, 0.12f + 0.25f * CoalGlow));
		P.RoundRect(FVector2f(82.0f, 142.0f), FVector2f(276.0f, 16.0f), C(0x33282A), 4.0f);
		P.Rect(FVector2f(84.0f, 154.0f), FVector2f(272.0f, 3.0f), A(Fire, 0.3f + 0.4f * CoalGlow));

		// Brick hearth.
		P.RectV(FVector2f(70.0f, 252.0f), FVector2f(300.0f, 148.0f), C(0xA5563A), C(0x6E3424));
		for (int32 Row = 0; Row < 7; ++Row)
		{
			const float Y = 252.0f + float(Row) * 22.0f;
			P.Rect(FVector2f(70.0f, Y), FVector2f(300.0f, 2.0f), A(C(0x3A1A12), 0.6f));
			for (int32 k = 0; k < 7; ++k)
			{
				const float X = 70.0f + float(k) * 48.0f + float(Row % 2) * 24.0f;
				if (X > 72.0f && X < 368.0f)
				{
					P.Rect(FVector2f(X, Y), FVector2f(2.0f, 22.0f), A(C(0x3A1A12), 0.6f));
				}
			}
		}
		P.RectV(FVector2f(70.0f, 252.0f), FVector2f(300.0f, 50.0f), A(C(0xFFB060), 0.3f * CoalGlow), A(C(0xFFB060), 0.0f));

		// Fire bed: glow, coals, flames, then the iron rods heating in it.
		P.RoundRect(FVector2f(88.0f, 206.0f), FVector2f(264.0f, 50.0f), C(0x1A1010), 12.0f);
		P.Glow(FVector2f(220.0f, 232.0f), 120.0f + 120.0f * CoalGlow, A(C(0xFF7A1A), 0.2f + 0.45f * CoalGlow));
		for (int32 i = 0; i < 7; ++i)
		{
			const float X = 118.0f + float(i) * 34.0f;
			const float Hgt = (14.0f + 56.0f * CoalGlow) * (0.7f + 0.3f * FMath::Sin(Time * 9.0f + float(i) * 1.7f));
			const float Sway = 5.0f * FMath::Sin(Time * 7.0f + float(i));
			P.Tri(FVector2f(X - 16.0f, 240.0f), FVector2f(X + 16.0f, 240.0f), FVector2f(X + Sway, 240.0f - Hgt), A(Fire, 0.8f));
			P.Tri(FVector2f(X - 8.0f, 240.0f), FVector2f(X + 8.0f, 240.0f), FVector2f(X + Sway * 0.6f, 240.0f - Hgt * 0.6f), A(C(0xFFE08A), 0.85f));
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector2f R0(142.0f + float(k) * 16.0f, 230.0f + float(k) * 5.0f);
			const FVector2f R1 = R0 + FVector2f(150.0f, -8.0f);
			const float Hot = Saturate(Progress * 1.3f - float(k) * 0.08f);
			const FLinearColor Rod = Mix(C(0x5A5F6A), Mix(C(0xD8431C), C(0xFFD27A), Saturate(Hot * 2.0f - 1.0f)), Saturate(Hot * 1.5f));
			P.Glow((R0 + R1) * 0.5f, 60.0f, A(Fire, 0.45f * Hot));
			P.Bar(R0, R1, 7.0f, Rod);
			P.Bar(R0 + FVector2f(0.0f, -2.0f), R1 + FVector2f(0.0f, -2.0f), 2.0f, A(C(0xFFF6D0), 0.2f + 0.5f * Hot));
		}
		for (int32 i = 0; i < 18; ++i)
		{
			const float X = 106.0f + float(i) * 13.5f + float((i * 37) % 7);
			const float Y = 246.0f + float((i * 53) % 3) * 4.0f;
			const float R = 9.0f + float(i % 3) * 3.0f;
			const float Flick = 0.55f + 0.45f * FMath::Sin(Time * (3.0f + float(i % 4)) + float(i));
			const FLinearColor Hot = Mix(C(0xE0401A), C(0xFFD060), CoalGlow * Flick);
			P.Circle(FVector2f(X, Y), R, Mix(C(0x3A1A14), Hot, Saturate(0.25f + CoalGlow * 0.9f)));
			P.Circle(FVector2f(X - R * 0.3f, Y - R * 0.3f), R * 0.35f, A(C(0xFFE9A0), 0.55f * CoalGlow * Flick));
		}
		P.Rect(FVector2f(62.0f, 250.0f), FVector2f(316.0f, 10.0f), Stone);
		P.Rect(FVector2f(62.0f, 250.0f), FVector2f(316.0f, 3.0f), C(0xB4B2BA));
		KGMgCraft::PaintBits(P, Bits);

		// Pipe, air streaks, bellows.
		P.Bar(FVector2f(360.0f, 262.0f), Hinge(), 14.0f, Iron);
		P.Bar(FVector2f(360.0f, 257.0f), Hinge() + FVector2f(0.0f, -5.0f), 3.0f, A(Cream, 0.25f));
		if (PumpT >= 0.0f)
		{
			const float K = Saturate(PumpT / PumpTime);
			for (int32 k = 0; k < 3; ++k)
			{
				const float Y = 250.0f + float(k) * 8.0f;
				P.Segment(FVector2f(356.0f - 60.0f * K, Y), FVector2f(356.0f - 60.0f * K - 36.0f, Y - 4.0f), A(Cream, 0.6f * (1.0f - K)), 3.0f);
			}
		}
		PaintBellows(P);

		// Heat gauge (left).
		const FVector2f GP(18.0f, 64.0f);
		const FVector2f GS(38.0f, 270.0f);
		auto GY = [&GP, &GS](float V) { return GP.Y + GS.Y * (1.0f - V); };
		P.Text(FVector2f(GP.X + GS.X * 0.5f, GP.Y - 24.0f), TEXT("HEAT"), 12.0f, Cream, 0.5f, TEXT("Black"), 80);
		P.RoundRect(GP - FVector2f(4.0f, 4.0f), GS + FVector2f(8.0f, 8.0f), A(Ink, 0.85f), 10.0f, A(Cream, 0.3f), 1.5f);
		P.Rect(FVector2f(GP.X, GY(1.0f)), FVector2f(GS.X, GY(HotLine) - GY(1.0f)), A(Crimson, 0.65f));
		const float Ctr = BandCentre();
		const bool bIn = InBand();
		P.RoundRect(FVector2f(GP.X - 3.0f, GY(Ctr + BandHalf)), FVector2f(GS.X + 6.0f, GY(Ctr - BandHalf) - GY(Ctr + BandHalf)),
		            A(Good, bIn ? 0.5f : 0.28f), 4.0f, Good, 2.0f);
		const float HeatY = GY(Heat);
		P.RectV(FVector2f(GP.X + 10.0f, HeatY), FVector2f(GS.X - 20.0f, GY(0.0f) - HeatY), Mix(Lantern, C(0xFFF0A0), Heat), C(0xA0301A));
		P.Bar(FVector2f(GP.X, HeatY), FVector2f(GP.X + GS.X, HeatY), 3.0f, Cream);
		P.Tri(FVector2f(GP.X + GS.X + 3.0f, HeatY), FVector2f(GP.X + GS.X + 15.0f, HeatY - 8.0f), FVector2f(GP.X + GS.X + 15.0f, HeatY + 8.0f),
		      Heat > HotLine ? Crimson : Cream);

		// Iron progress (top right).
		KGMgCraft::HudPanel(P, FVector2f(396.0f, 12.0f), FVector2f(230.0f, 62.0f));
		KGMgCraft::Caption(P, FVector2f(410.0f, 20.0f), TEXT("IRON GLOW"), CreamDim);
		P.Text(FVector2f(612.0f, 16.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Progress * 100.0f)), 14.0f, Cream, 1.0f, TEXT("Black"));
		P.Gauge(FVector2f(408.0f, 42.0f), FVector2f(206.0f, 20.0f), Progress, Mix(Fire, Gold, Progress));

		// State + hints.
		const FVector2f Hd = HandleEnd();
		if (Heat > HotLine)
		{
			P.Tag(FVector2f(220.0f, 186.0f), TEXT("TOO HOT - let it cool"), A(Crimson, 0.92f), Cream, 13.0f);
		}
		else if (Heat < Ctr - BandHalf && !bSolved)
		{
			P.Tag(FVector2f(500.0f, 122.0f), TEXT("PUMP!"), A(Ink, 0.75f), Mix(Cream, Gold, Ping(Time, 0.6f)), 15.0f);
		}
		else if (bIn && !bSolved)
		{
			P.Tag(FVector2f(220.0f, 186.0f), TEXT("Hold it there..."), A(Good, 0.9f), Ink, 13.0f);
		}
		if (Pumps == 0)
		{
			P.HintRing(Hd, 26.0f, Time, Gold);
			P.HintArrow(Hd + FVector2f(0.0f, -84.0f), Hd + FVector2f(0.0f, -26.0f), Time, Cream);
		}
	}

	void PaintBellows(FKGMgPainter& P) const
	{
		const FVector2f Hg = Hinge();
		const FVector2f Top = TopEnd();
		const FVector2f Bot(588.0f, 282.0f);
		const FVector2f Hd = HandleEnd();
		// Stand.
		P.Bar(FVector2f(440.0f, 280.0f), FVector2f(432.0f, 392.0f), 12.0f, WoodDark);
		P.Bar(FVector2f(566.0f, 284.0f), FVector2f(574.0f, 392.0f), 12.0f, WoodDark);
		P.Bar(FVector2f(436.0f, 356.0f), FVector2f(570.0f, 356.0f), 8.0f, WoodDark);
		// Leather with accordion folds.
		P.Tri(Hg, Top, Bot, C(0x8A5234));
		P.Tri(Hg, Top, FMath::Lerp(Top, Bot, 0.5f), A(C(0xB07048), 0.5f));
		for (const float T : {0.35f, 0.55f, 0.75f, 0.92f})
		{
			P.Segment(FMath::Lerp(Hg, Top, T), FMath::Lerp(Hg, Bot, T), A(Ink, 0.35f), 2.0f);
		}
		// Boards: fixed bottom, lifting top (with brass studs), handle.
		P.Bar(Hg + FVector2f(0.0f, 6.0f), Bot, 12.0f, WoodDark);
		P.Bar(Hg, Top, 13.0f, Wood);
		const FVector2f Along = (Top - Hg).GetSafeNormal();
		const FVector2f Up(Along.Y, -Along.X);
		P.Bar(Hg + Up * 4.0f, Top + Up * 4.0f, 3.0f, A(WoodLight, 0.9f));
		for (int32 k = 1; k < 4; ++k)
		{
			P.Circle(FMath::Lerp(Hg, Top, float(k) * 0.25f), 2.5f, Gold);
		}
		P.Bar(Top, Hd, 9.0f, WoodLight);
		P.Circle(Hd, 14.0f, Open > 0.9f ? Gold : WoodLight, WoodDark, 3.0f);
		// Nozzle.
		P.Quad(Hg + FVector2f(6.0f, -10.0f), Hg + FVector2f(6.0f, 10.0f), Hg + FVector2f(-34.0f, 5.0f), Hg + FVector2f(-34.0f, -5.0f), Iron);
		P.Circle(Hg, 8.0f, C(0x2E3138));
		// Air readiness under the bellows.
		KGMgCraft::Caption(P, FVector2f(506.0f, 300.0f), TEXT("AIR"), CreamDim, 0.5f);
		P.Gauge(FVector2f(456.0f, 316.0f), FVector2f(100.0f, 12.0f), Open, Open > 0.9f ? Good : Lantern);
	}

	// ---- hammer -------------------------------------------------------------------------------------------------------

	void NewZone()
	{
		const float Width = 0.17f - 0.015f * float(Blows);
		Z0 = 0.42f + Rng.FRand() * (0.9f - 0.42f - Width);
		Z1 = Z0 + Width;
		Marker = 0.0f;
		MarkerDir = 1.0f;
	}

	float ShapeK() const
	{
		float S = float(Blows);
		if (ShapeT >= 0.0f)
		{
			S = float(Blows - 1) + EaseOutBack(Saturate(ShapeT / 0.3f));
		}
		return FMath::Clamp(S / float(BlowsNeeded), 0.0f, 1.05f);
	}

	static constexpr float NailHeadX = 388.0f;
	static constexpr float FaceY = 212.0f;
	float NailLen() const { return FMath::Lerp(78.0f, 150.0f, ShapeK()); }
	float NailThick() const { return FMath::Lerp(18.0f, 9.0f, ShapeK()); }
	FVector2f NailTop() const { return FVector2f(NailHeadX - NailLen() * 0.5f, FaceY - NailThick()); }

	void Strike()
	{
		if (Stage != 1 || bSolved || StrikeT >= 0.0f || ShapeT >= 0.0f || ReheatT > 0.0f || Blows >= BlowsNeeded)
		{
			return;
		}
		StrikeT = 0.0f;
		bImpactDone = false;
		bPendingGood = Marker >= Z0 && Marker <= Z1;
		Sound(TEXT("S_Chore_Whoosh"), 0.45f, 1.15f);
	}

	void Impact()
	{
		const FVector2f Hit = NailTop();
		if (bPendingGood)
		{
			++Blows;
			ShapeT = 0.0f;
			KGMgCraft::Burst(Bits, Rng, Hit, 22, -PI, 0.0f, 180.0f, 460.0f, C(0xFFF1B0), Fire, 2.5f, 0.55f, true);
			Nice(Hit + FVector2f(0.0f, -80.0f), Blows >= BlowsNeeded ? TEXT("Sharp nail!") : TEXT("Clang!"), TEXT("S_Chore_Anvil"),
			     0.9f + 0.07f * float(Blows));
		}
		else
		{
			Temp = FMath::Max(0.0f, Temp - 0.22f);
			KGMgCraft::Burst(Bits, Rng, Hit, 6, -PI, 0.0f, 90.0f, 200.0f, Fire, C(0x8A3A1A), 2.0f, 0.35f, true);
			Sound(TEXT("S_Chore_Anvil"), 0.6f, 0.7f);
			Oops(Hit + FVector2f(0.0f, -80.0f), TEXT("Glancing blow - it cools"), TEXT("S_UI_Bad"), 4.0f);
			if (Temp < ColdLine)
			{
				StartReheat();
			}
		}
	}

	void StartReheat()
	{
		ReheatT = ReheatTime;
		PopText(FVector2f(320.0f, 110.0f), TEXT("Too cold - back in the fire"), C(0xFFB070));
		Sound(TEXT("S_Chore_Bellows"), 0.8f, 0.9f);
	}

	void TickHammer(float Dt)
	{
		if (StrikeT >= 0.0f)
		{
			StrikeT += Dt;
			if (!bImpactDone && StrikeT >= ImpactAt)
			{
				bImpactDone = true;
				Impact();
			}
			if (StrikeT >= StrikeTime)
			{
				StrikeT = -1.0f;
			}
		}
		if (ShapeT >= 0.0f)
		{
			ShapeT += Dt;
			if (ShapeT >= ShapeLock)
			{
				ShapeT = -1.0f;
				if (Blows >= BlowsNeeded)
				{
					if (!bSolved)
					{
						Solve();
					}
				}
				else
				{
					NewZone();
				}
			}
		}
		if (ReheatT > 0.0f)
		{
			ReheatT -= Dt;
			Temp = FMath::Lerp(1.0f, ColdLine, Saturate(ReheatT / ReheatTime));
			if (ReheatT <= 0.0f)
			{
				ReheatT = 0.0f;
				Temp = 1.0f;
				NewZone();
				Sound(TEXT("S_Chore_Flame"), 0.6f, 1.1f);
			}
		}
		else if (!bSolved && Blows < BlowsNeeded)
		{
			Temp = FMath::Max(0.0f, Temp - Dt * 0.02f);
			if (Temp < ColdLine && StrikeT < 0.0f)
			{
				StartReheat();
			}
		}
		const bool bFree = StrikeT < 0.0f && ShapeT < 0.0f && ReheatT <= 0.0f && !bSolved && Blows < BlowsNeeded;
		if (bFree)
		{
			Marker += MarkerDir * Dt * (0.95f + 0.1f * float(Blows));
			if (Marker > 1.0f)
			{
				Marker = 2.0f - Marker;
				MarkerDir = -1.0f;
			}
			else if (Marker < 0.0f)
			{
				Marker = -Marker;
				MarkerDir = 1.0f;
			}
		}
	}

	float HammerAngle() const
	{
		const float Rest = 0.62f;
		if (StrikeT < 0.0f)
		{
			return Rest + 0.03f * FMath::Sin(Time * 2.0f);
		}
		if (StrikeT < ImpactAt)
		{
			const float K = StrikeT / ImpactAt;
			return FMath::Lerp(Rest, 0.0f, K * K);
		}
		if (StrikeT < 0.18f)
		{
			return -0.04f * FMath::Sin((StrikeT - ImpactAt) / 0.08f * PI);
		}
		return FMath::Lerp(0.0f, Rest, EaseOutCubic((StrikeT - 0.18f) / (StrikeTime - 0.18f)));
	}

	void PaintNail(FKGMgPainter& P) const
	{
		const float L = NailLen();
		const float T = NailThick();
		const float PL = FMath::Lerp(5.0f, 34.0f, FMath::Min(ShapeK(), 1.0f));
		const FLinearColor Body = Mix(C(0x5A5F6A), Mix(C(0xD8431C), C(0xFFD27A), Temp * 0.8f), Saturate(Temp * 1.2f));
		const float X0 = NailHeadX - L;
		P.Glow(FVector2f(NailHeadX - L * 0.5f, FaceY - T * 0.5f), 70.0f + 60.0f * Temp, A(Fire, 0.5f * Temp));
		P.Rect(FVector2f(X0 + PL, FaceY - T), FVector2f(L - PL, T), Body);
		P.Tri(FVector2f(X0 + PL, FaceY - T), FVector2f(X0 + PL, FaceY), FVector2f(X0, FaceY - T * 0.5f), Body);
		P.Rect(FVector2f(NailHeadX, FaceY - T - 6.0f), FVector2f(9.0f, T + 6.0f), Mix(Body, Ink, 0.18f));
		P.Rect(FVector2f(X0 + PL, FaceY - T), FVector2f(L - PL, 3.0f), A(C(0xFFF6D0), 0.25f + 0.5f * Temp));
		for (int32 i = 0; i < Blows; ++i)
		{
			const float X = NailHeadX - 14.0f - float(i) * (L - PL - 16.0f) / float(BlowsNeeded);
			P.Rect(FVector2f(X, FaceY - T), FVector2f(2.0f, T), A(Ink, 0.18f));
		}
	}

	void PaintHammer(FKGMgPainter& P) const
	{
		KGMgCraft::SmithyWall(P, 0.3f + 0.4f * Temp);

		// Forge mouth glowing at the left edge.
		P.RoundRect(FVector2f(-20.0f, 206.0f), FVector2f(112.0f, 110.0f), C(0x1A1010), 18.0f);
		P.Glow(FVector2f(34.0f, 282.0f), 110.0f, A(Fire, 0.55f));
		for (int32 i = 0; i < 6; ++i)
		{
			const float Flick = 0.6f + 0.4f * FMath::Sin(Time * 4.0f + float(i) * 1.3f);
			P.Circle(FVector2f(8.0f + float(i) * 14.0f, 300.0f - float(i % 2) * 6.0f), 9.0f, Mix(C(0xC0301A), C(0xFFD060), Flick));
		}
		// Horseshoes and tongs on the wall.
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2f Ctr(66.0f + float(i) * 46.0f, 118.0f);
			P.Circle(Ctr + FVector2f(0.0f, -20.0f), 3.0f, C(0x2E3138));
			P.Arc(Ctr, 14.0f, 0.62f * PI, 2.38f * PI, Iron, 6.0f, 20);
			P.Arc(Ctr, 14.0f, 1.2f * PI, 1.6f * PI, A(Cream, 0.25f), 2.0f, 8);
		}
		P.Bar(FVector2f(250.0f, 70.0f), FVector2f(300.0f, 160.0f), 5.0f, Iron);
		P.Bar(FVector2f(262.0f, 70.0f), FVector2f(246.0f, 160.0f), 5.0f, Iron);
		P.Circle(FVector2f(256.0f, 92.0f), 4.0f, C(0x2E3138));

		// Stump + anvil.
		P.RectV(FVector2f(226.0f, 300.0f), FVector2f(200.0f, 100.0f), C(0x7A5234), C(0x4A2F1C));
		for (int32 k = 0; k < 5; ++k)
		{
			P.Segment(FVector2f(240.0f + float(k) * 40.0f, 312.0f), FVector2f(236.0f + float(k) * 40.0f, 398.0f), A(Ink, 0.25f), 2.0f);
		}
		P.Disc(FVector2f(326.0f, 300.0f), FVector2f(100.0f, 14.0f), C(0xC99A62), C(0x9C6B3F));
		const FLinearColor AnvilTop = C(0x7D838F);
		const FLinearColor AnvilMid = C(0x5A5F6A);
		const FLinearColor AnvilDark = C(0x34373F);
		P.Quad(FVector2f(246.0f, 306.0f), FVector2f(406.0f, 306.0f), FVector2f(384.0f, 284.0f), FVector2f(268.0f, 284.0f), AnvilDark);
		P.Quad(FVector2f(280.0f, 288.0f), FVector2f(372.0f, 288.0f), FVector2f(356.0f, 246.0f), FVector2f(296.0f, 246.0f), AnvilMid);
		P.RectV(FVector2f(236.0f, 222.0f), FVector2f(196.0f, 30.0f), AnvilMid, AnvilDark);
		P.Tri(FVector2f(236.0f, 212.0f), FVector2f(236.0f, 246.0f), FVector2f(154.0f, 216.0f), AnvilMid);
		P.Tri(FVector2f(236.0f, 212.0f), FVector2f(236.0f, 222.0f), FVector2f(158.0f, 214.0f), AnvilTop);
		P.Rect(FVector2f(236.0f, 212.0f), FVector2f(196.0f, 12.0f), AnvilTop);
		P.Rect(FVector2f(236.0f, 212.0f), FVector2f(196.0f, 3.0f), C(0xB4BAC4));
		P.Rect(FVector2f(432.0f, 212.0f), FVector2f(24.0f, 24.0f), AnvilMid);
		P.Rect(FVector2f(432.0f, 212.0f), FVector2f(24.0f, 3.0f), C(0xB4BAC4));
		P.Rect(FVector2f(414.0f, 215.0f), FVector2f(8.0f, 6.0f), AnvilDark);

		PaintNail(P);
		KGMgCraft::PaintBits(P, Bits);

		// Hammer (swings around the gloved grip).
		const float Ang = HammerAngle();
		const FVector2f Grip(596.0f, 172.0f);
		const FVector2f HeadC = Grip + KGMgCraft::Rot(FVector2f(-244.0f, 0.0f), Ang);
		P.Bar(Grip + KGMgCraft::Rot(FVector2f(10.0f, 0.0f), Ang), HeadC, 11.0f, C(0xB98256));
		P.Bar(Grip + KGMgCraft::Rot(FVector2f(0.0f, -3.0f), Ang), HeadC + KGMgCraft::Rot(FVector2f(0.0f, -3.0f), Ang), 2.0f, A(C(0xE0B07A), 0.8f));
		P.Bar(Grip + KGMgCraft::Rot(FVector2f(6.0f, 0.0f), Ang), Grip + KGMgCraft::Rot(FVector2f(-54.0f, 0.0f), Ang), 15.0f, C(0x6E3B22));
		P.Circle(Grip, 17.0f, C(0x8A5A3B), C(0x5C3A26), 3.0f);
		P.RotRect(HeadC, FVector2f(30.0f, 64.0f), Ang, C(0x4A4E57));
		P.RotRect(HeadC + KGMgCraft::Rot(FVector2f(0.0f, 28.0f), Ang), FVector2f(32.0f, 8.0f), Ang, C(0x8B929C));
		P.RotRect(HeadC + KGMgCraft::Rot(FVector2f(0.0f, -29.0f), Ang), FVector2f(24.0f, 7.0f), Ang, C(0x6C737E));
		P.RotRect(HeadC + KGMgCraft::Rot(FVector2f(-9.0f, 0.0f), Ang), FVector2f(4.0f, 54.0f), Ang, A(Cream, 0.2f));

		// Metal heat (top left).
		KGMgCraft::HudPanel(P, FVector2f(14.0f, 12.0f), FVector2f(200.0f, 58.0f));
		KGMgCraft::Caption(P, FVector2f(26.0f, 20.0f), TEXT("METAL HEAT"), CreamDim);
		P.Gauge(FVector2f(24.0f, 40.0f), FVector2f(180.0f, 18.0f), Temp, Temp < 0.5f ? Lantern : Mix(Fire, Gold, Temp), ColdLine, 1.0f);

		// Timing bar.
		const FVector2f M0(120.0f, 356.0f);
		const float MW = 400.0f;
		const bool bLocked = StrikeT >= 0.0f || ShapeT >= 0.0f || ReheatT > 0.0f || bSolved;
		P.RoundRect(M0 - FVector2f(8.0f, 8.0f), FVector2f(MW + 16.0f, 36.0f), A(Ink, 0.82f), 12.0f);
		P.RoundRect(M0, FVector2f(MW, 20.0f), C(0x8C2A2A), 6.0f);
		P.Rect(M0 + FVector2f(MW * Z0, 0.0f), FVector2f(MW * (Z1 - Z0), 20.0f), Good);
		P.Rect(M0 + FVector2f(MW * Z0, 0.0f), FVector2f(MW * (Z1 - Z0), 4.0f), A(Cream, 0.4f));
		if (bLocked)
		{
			P.RoundRect(M0, FVector2f(MW, 20.0f), A(Ink, 0.5f), 6.0f);
		}
		const float NX = M0.X + MW * Marker;
		const bool bInZone = Marker >= Z0 && Marker <= Z1;
		P.Tri(FVector2f(NX, M0.Y + 4.0f), FVector2f(NX - 9.0f, M0.Y - 12.0f), FVector2f(NX + 9.0f, M0.Y - 12.0f), bInZone ? Good : Cream);
		P.Bar(FVector2f(NX, M0.Y - 2.0f), FVector2f(NX, M0.Y + 24.0f), 4.0f, Cream);
		P.Text(FVector2f(M0.X + MW + 24.0f, M0.Y - 6.0f), FString::Printf(TEXT("%d / %d"), Blows, BlowsNeeded), 20.0f, Cream, 0.0f, TEXT("Black"));
		KGMgCraft::Caption(P, FVector2f(M0.X + MW + 24.0f, M0.Y + 20.0f), TEXT("BLOWS"), CreamDim);
		if (Blows == 0 && !bLocked)
		{
			P.HintRing(FVector2f(M0.X + MW * (Z0 + Z1) * 0.5f, M0.Y + 10.0f), 26.0f, Time, Gold);
			if (bInZone)
			{
				P.Tag(FVector2f(M0.X + MW * (Z0 + Z1) * 0.5f, M0.Y - 28.0f), TEXT("NOW!"), Good, Ink, 13.0f);
			}
		}
		if (ReheatT > 0.0f)
		{
			P.Tag(FVector2f(320.0f, 140.0f), TEXT("Reheating..."), A(Crimson, 0.9f), Cream, 14.0f);
		}
	}
};

// =====================================================================================================================
// SharpenTools: the mouse height sets the blade angle; hold and stroke it end to end across the whetstone.
// =====================================================================================================================
class FKGMgSharpenTools final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Mouse height tilts the axe: hold and stroke end to end with the angle in the green")
		                  : TEXT("Now the scythe, at a steeper angle: hold and stroke end to end in the green");
	}

	virtual void BeginStage() override
	{
		Bits.Reset();
		Sharp = 0.0f;
		BladeX = ContactMin + 30.0f;
		Lift = 1.0f;
		AngleDeg = 6.0f;
		StrokeSide = 0;
		StrokeIn = 0.0f;
		StrokeOut = 0.0f;
		GlintT = -1.0f;
		WrongT = 0.0f;
		ScrapeAcc = 0.0f;
		DebrisAcc = 0.0f;
		bAutoDown = false;
	}

	virtual void OnPress(const FVector2f& Pos) override { Sound(TEXT("S_UI_Click"), 0.35f, 0.8f); }

	virtual void Tick(float Dt) override
	{
		const bool bHeld = Held();
		AngleDeg = Approach(AngleDeg, TargetAngle(Mouse.Y), Dt, 14.0f);
		const float Want = FMath::Clamp(Mouse.X, ContactMin, ContactMax);
		const float MaxStep = MaxSpeed * Dt;
		const float Step = FMath::Clamp(Want - BladeX, -MaxStep, MaxStep);
		BladeX += Step;
		Lift = Approach(Lift, bHeld ? 0.0f : 1.0f, Dt, 18.0f);
		const bool bGrind = bHeld && Lift < 0.35f;
		if (bGrind)
		{
			const bool bIn = InBand(AngleDeg);
			const float Moved = FMath::Abs(Step);
			if (bIn)
			{
				StrokeIn += Moved;
			}
			else
			{
				StrokeOut += Moved;
			}
			if (Moved > 0.2f)
			{
				const FVector2f Contact = ContactPoint();
				const bool bRight = Step > 0.0f;
				DebrisAcc += Moved * (bIn ? 0.08f : 0.22f);
				while (DebrisAcc >= 1.0f)
				{
					DebrisAcc -= 1.0f;
					const float A0 = bRight ? -PI + 0.2f : -0.9f;
					const float A1 = bRight ? -PI * 0.62f : -0.2f;
					if (bIn)
					{
						KGMgCraft::Burst(Bits, Rng, Contact, 1, A0, A1, 40.0f, 130.0f, C(0xD0DAE2), C(0x8FA0AE), 2.6f, 0.5f, false);
					}
					else
					{
						KGMgCraft::Burst(Bits, Rng, Contact, 2, A0, A1, 180.0f, 380.0f, C(0xFFF1B0), Fire, 2.0f, 0.35f, true);
					}
				}
				ScrapeAcc += Moved;
				if (ScrapeAcc > 70.0f)
				{
					ScrapeAcc = 0.0f;
					Sound(TEXT("S_Chore_Scrape"), 0.45f, bIn ? 1.0f + 0.03f * Sharp : 0.75f);
				}
				if (!bIn)
				{
					WrongT = 0.3f;
				}
			}
			const int32 Side = BladeX <= ContactMin + EndZone ? -1 : (BladeX >= ContactMax - EndZone ? 1 : 0);
			if (Side != 0 && Side != StrokeSide)
			{
				if (StrokeSide == -Side)
				{
					FinishStroke();
				}
				StrokeSide = Side;
				StrokeIn = 0.0f;
				StrokeOut = 0.0f;
			}
		}
		else
		{
			StrokeSide = 0;
			StrokeIn = 0.0f;
			StrokeOut = 0.0f;
		}
		WrongT = FMath::Max(0.0f, WrongT - Dt);
		if (GlintT >= 0.0f)
		{
			GlintT += Dt;
			if (GlintT > 0.7f)
			{
				GlintT = -1.0f;
			}
		}
		KGMgCraft::TickBits(Bits, Dt, 600.0f, 318.0f);
	}

	virtual void AutoPlay(float Dt) override
	{
		bAutoDown = true;
		Mouse.Y = YForAngle((BandLo() + BandHi()) * 0.5f);
		Mouse.X = StrokeSide == -1 ? ContactMax + 20.0f : ContactMin - 20.0f;
	}

	virtual void DebugPose() override
	{
		Sharp = 3.0f;
		AngleDeg = (BandLo() + BandHi()) * 0.5f;
		Mouse = FVector2f(300.0f, YForAngle(AngleDeg));
		BladeX = 300.0f;
		Lift = 0.0f;
		bAutoDown = true;
		StrokeSide = -1;
		GlintT = 0.25f;
		for (int32 i = 0; i < 10; ++i)
		{
			KGMgCraft::Burst(Bits, Rng, ContactPoint(), 1, -PI + 0.2f, -PI * 0.62f, 40.0f, 130.0f, C(0xD0DAE2), C(0x8FA0AE), 2.6f, 0.5f, false);
			KGMgCraft::TickBits(Bits, 0.04f, 600.0f, 318.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintWorkshop(P);
		PaintStone(P);
		PaintTool(P);
		KGMgCraft::PaintBits(P, Bits);
		PaintAngleArc(P);
		PaintHud(P);
	}

private:
	static constexpr float StoneL = 110.0f;
	static constexpr float StoneR = 420.0f;
	static constexpr float StoneTop = 262.0f;
	static constexpr float ContactMin = StoneL + 16.0f;
	static constexpr float ContactMax = StoneR - 16.0f;
	static constexpr float EndZone = 40.0f;
	static constexpr float MaxSpeed = 380.0f;   // ~0.52 s per stroke, 10 strokes >= 5.2 s (floor 4.0)
	static constexpr float AngleZeroY = 320.0f;
	static constexpr float DegPerPx = 0.2f;
	static constexpr float MaxDeg = 46.0f;
	static constexpr int32 Needed = 10;

	TArray<KGMgCraft::FBit> Bits;
	float Sharp = 0.0f;
	float BladeX = 0.0f;
	float Lift = 1.0f;
	float AngleDeg = 0.0f;
	int32 StrokeSide = 0;
	float StrokeIn = 0.0f;
	float StrokeOut = 0.0f;
	float GlintT = -1.0f;
	float WrongT = 0.0f;
	float ScrapeAcc = 0.0f;
	float DebrisAcc = 0.0f;
	bool bAutoDown = false;

	bool Held() const { return (bMouseDown || bAutoDown) && !bSolved; }
	float BandLo() const { return Stage == 0 ? 15.0f : 30.0f; }
	float BandHi() const { return Stage == 0 ? 22.0f : 38.0f; }
	bool InBand(float Deg) const { return Deg >= BandLo() && Deg <= BandHi(); }
	float TargetAngle(float Y) const { return FMath::Clamp((AngleZeroY - Y) * DegPerPx, 0.0f, MaxDeg); }
	float YForAngle(float Deg) const { return AngleZeroY - Deg / DegPerPx; }
	FVector2f ContactPoint() const { return FVector2f(BladeX, StoneTop - Lift * 22.0f); }

	void FinishStroke()
	{
		const float Total = StrokeIn + StrokeOut;
		if (Total < 60.0f)
		{
			return;
		}
		const FVector2f At = ContactPoint() + FVector2f(0.0f, -100.0f);
		if (StrokeOut <= Total * 0.3f)
		{
			Sharp = FMath::Min(float(Needed), Sharp + 1.0f);
			GlintT = 0.0f;
			const bool bDone = Sharp >= float(Needed);
			Nice(At, bDone ? TEXT("Razor sharp!") : TEXT("Good stroke"), TEXT("S_Chore_Grind"), 0.9f + 0.05f * Sharp);
			if (bDone)
			{
				Solve();
			}
		}
		else
		{
			Sharp = FMath::Max(0.0f, Sharp - 0.5f);
			KGMgCraft::Burst(Bits, Rng, ContactPoint(), 12, -PI, 0.0f, 200.0f, 420.0f, C(0xFFF1B0), Fire, 2.2f, 0.4f, true);
			Oops(At, TEXT("Wrong angle!"), TEXT("S_UI_Bad"), 4.0f);
		}
	}

	void PaintWorkshop(FKGMgPainter& P) const
	{
		KGMgCraft::Planks(P, FVector2f(0, 0), FVector2f(W, 318.0f), 64.0f, C(0x7A5238), C(0x4E3322), true);
		// Window with daylight.
		P.RoundRect(FVector2f(22.0f, 24.0f), FVector2f(124.0f, 114.0f), WoodDark, 6.0f);
		P.RectV(FVector2f(30.0f, 32.0f), FVector2f(108.0f, 98.0f), C(0x9ED7F2), C(0xE9F4F2));
		P.Quad(FVector2f(30.0f, 110.0f), FVector2f(138.0f, 96.0f), FVector2f(138.0f, 130.0f), FVector2f(30.0f, 130.0f), C(0x8CC66A));
		P.Rect(FVector2f(81.0f, 32.0f), FVector2f(6.0f, 98.0f), WoodDark);
		P.Rect(FVector2f(30.0f, 78.0f), FVector2f(108.0f, 6.0f), WoodDark);
		P.Glow(FVector2f(84.0f, 190.0f), 180.0f, A(C(0xFFF0C0), 0.16f));
		// Saw and spare handles on a rail.
		P.Rect(FVector2f(170.0f, 40.0f), FVector2f(300.0f, 8.0f), WoodDark);
		P.Quad(FVector2f(190.0f, 56.0f), FVector2f(330.0f, 56.0f), FVector2f(330.0f, 80.0f), FVector2f(196.0f, 70.0f), C(0x9AA2AC));
		for (int32 i = 0; i < 12; ++i)
		{
			const float X = 198.0f + float(i) * 11.0f;
			P.Tri(FVector2f(X, 70.0f + float(i) * 0.8f), FVector2f(X + 11.0f, 71.0f + float(i) * 0.8f), FVector2f(X + 5.0f, 77.0f + float(i) * 0.8f),
			      C(0x7A828C));
		}
		P.RoundRect(FVector2f(330.0f, 50.0f), FVector2f(38.0f, 36.0f), C(0x9C6B3F), 10.0f, WoodDark, 2.0f);
		P.Bar(FVector2f(400.0f, 48.0f), FVector2f(392.0f, 140.0f), 8.0f, C(0xB98256));
		P.Bar(FVector2f(430.0f, 48.0f), FVector2f(436.0f, 130.0f), 8.0f, C(0xA8744A));
		// Bench.
		P.RectV(FVector2f(0, 318.0f), FVector2f(W, 82.0f), C(0xA8744A), C(0x6E4A2E));
		P.Rect(FVector2f(0, 318.0f), FVector2f(W, 5.0f), C(0xC99A6A));
		for (int32 i = 0; i < 4; ++i)
		{
			P.Segment(FVector2f(0.0f, 336.0f + float(i) * 16.0f), FVector2f(W, 332.0f + float(i) * 17.0f), A(Ink, 0.15f), 1.0f);
		}
		// Water pot for wetting the stone.
		P.Quad(FVector2f(22.0f, 282.0f), FVector2f(90.0f, 282.0f), FVector2f(84.0f, 320.0f), FVector2f(28.0f, 320.0f), Wood);
		P.Disc(FVector2f(56.0f, 283.0f), FVector2f(34.0f, 6.0f), Water, WaterDeep, 20);
		P.Bar(FVector2f(24.0f, 292.0f), FVector2f(88.0f, 292.0f), 4.0f, Iron);
		P.Bar(FVector2f(27.0f, 312.0f), FVector2f(85.0f, 312.0f), 4.0f, Iron);
	}

	void PaintStone(FKGMgPainter& P) const
	{
		P.Shadow(FVector2f(StoneL - 22.0f, StoneTop + 26.0f), FVector2f(StoneR - StoneL + 44.0f, 30.0f), 6.0f, 0.3f, 4.0f);
		P.RoundRect(FVector2f(StoneL - 22.0f, StoneTop + 26.0f), FVector2f(StoneR - StoneL + 44.0f, 30.0f), Wood, 5.0f, WoodDark, 2.0f);
		P.Rect(FVector2f(StoneL - 22.0f, StoneTop + 26.0f), FVector2f(StoneR - StoneL + 44.0f, 4.0f), WoodLight);
		P.RectV(FVector2f(StoneL, StoneTop), FVector2f(StoneR - StoneL, 30.0f), C(0x9AA0A8), C(0x676C75));
		P.Rect(FVector2f(StoneL, StoneTop), FVector2f(StoneR - StoneL, 5.0f), C(0xC4CAD2));
		P.Disc(FVector2f((StoneL + StoneR) * 0.5f, StoneTop + 3.0f), FVector2f((StoneR - StoneL) * 0.4f, 3.0f), A(C(0xE6F4FF), 0.6f),
		       A(C(0xE6F4FF), 0.0f), 20);
		for (int32 i = 0; i < 24; ++i)
		{
			const float X = StoneL + 8.0f + float((i * 53) % 290);
			const float Y = StoneTop + 9.0f + float((i * 29) % 18);
			P.Circle(FVector2f(X, Y), 1.3f, A(Ink, 0.25f));
		}
		// End markers: where the next stroke must reach.
		const float Pulse = 0.45f + 0.55f * Ping(Time, 0.8f);
		const bool bHeld = Held();
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const bool bTarget = bHeld && (StrokeSide == 0 || StrokeSide == -Side);
			const float X = Side < 0 ? StoneL + 16.0f : StoneR - 16.0f;
			const FLinearColor Col = bTarget ? A(Gold, Pulse) : A(Cream, 0.3f);
			P.Tri(FVector2f(X + float(Side) * 10.0f, StoneTop + 17.0f), FVector2f(X - float(Side) * 4.0f, StoneTop + 9.0f),
			      FVector2f(X - float(Side) * 4.0f, StoneTop + 25.0f), Col);
		}
	}

	void PaintTool(FKGMgPainter& P) const
	{
		const FVector2f O = ContactPoint();
		const float Ang = FMath::DegreesToRadians(AngleDeg);
		auto Pt = [&O, Ang](float X, float Y) { return O + KGMgCraft::Rot(FVector2f(X, Y), Ang); };
		const float Sh = Saturate(Sharp / float(Needed));
		const FLinearColor Keen = C(0xF4FBFF);
		const FLinearColor Dull = C(0x565B64);
		P.Disc(FVector2f(O.X - 40.0f, StoneTop + 2.0f), FVector2f(70.0f, 4.0f), A(Ink, 0.25f * (1.0f - Lift)), A(Ink, 0.0f), 16);
		if (Stage == 0)
		{
			// Axe: edge on the stone from the contact point leftwards, handle up and to the right.
			P.Bar(Pt(-54.0f, -66.0f), Pt(-92.0f, -250.0f), 16.0f, C(0xB98256));
			P.Bar(Pt(-49.0f, -66.0f), Pt(-86.0f, -248.0f), 4.0f, A(C(0xE0B07A), 0.8f));
			P.Quad(Pt(-32.0f, -56.0f), Pt(-76.0f, -56.0f), Pt(-80.0f, -86.0f), Pt(-28.0f, -86.0f), C(0x3E424A));
			P.Quad(Pt(0.0f, 0.0f), Pt(-110.0f, 0.0f), Pt(-76.0f, -58.0f), Pt(-32.0f, -58.0f), C(0x6C737E));
			P.Quad(Pt(-20.0f, -30.0f), Pt(-90.0f, -30.0f), Pt(-76.0f, -54.0f), Pt(-34.0f, -54.0f), A(Ink, 0.15f));
			P.Quad(Pt(0.0f, 0.0f), Pt(-110.0f, 0.0f), Pt(-100.0f, -14.0f), Pt(-8.0f, -14.0f), C(0x9AA3AE));
			P.Bar(Pt(0.0f, -1.0f), Pt(-110.0f, -1.0f), 3.0f, Dull);
			if (Sh > 0.01f)
			{
				P.Bar(Pt(0.0f, -1.5f), Pt(-110.0f * Sh, -1.5f), 3.5f, Keen);
				P.Bar(Pt(-2.0f, -6.0f), Pt(-108.0f * Sh, -6.0f), 4.0f, A(Keen, 0.35f));
			}
			for (int32 k = 0; k < 6; ++k)
			{
				const float F = (float(k) + 0.5f) / 6.0f;
				if (F > Sh)
				{
					const float X = -110.0f * F;
					P.Tri(Pt(X - 3.0f, 0.0f), Pt(X + 3.0f, 0.0f), Pt(X, -4.0f), C(0x2E3138));
				}
			}
			if (GlintT >= 0.0f && Sh > 0.01f)
			{
				const float K = Saturate(GlintT / 0.7f);
				KGMgCraft::Sparkle(P, Pt(-110.0f * Sh * K, -3.0f), 5.0f + 11.0f * FMath::Sin(K * PI), Keen);
			}
		}
		else
		{
			// Scythe: long curved blade from the heel (contact) leftwards, snath up and to the right.
			P.Bar(Pt(8.0f, -14.0f), Pt(84.0f, -236.0f), 13.0f, C(0x9C6B3F));
			P.Bar(Pt(12.0f, -14.0f), Pt(88.0f, -234.0f), 3.0f, A(C(0xD9A56A), 0.7f));
			P.Bar(Pt(52.0f, -150.0f), Pt(92.0f, -142.0f), 10.0f, C(0x7A4A2A));
			const int32 N = 12;
			TArray<FVector2f> EdgeLine;
			for (int32 i = 0; i <= N; ++i)
			{
				const float T = float(i) / float(N);
				EdgeLine.Add(FVector2f(-236.0f * T, -38.0f * T * T));
			}
			for (int32 i = 0; i < N; ++i)
			{
				const float T0 = float(i) / float(N);
				const float T1 = float(i + 1) / float(N);
				const FVector2f E0 = EdgeLine[i];
				const FVector2f E1 = EdgeLine[i + 1];
				const FVector2f B0 = E0 + FVector2f(6.0f * T0, -(28.0f * (1.0f - T0) + 5.0f));
				const FVector2f B1 = E1 + FVector2f(6.0f * T1, -(28.0f * (1.0f - T1) + 5.0f));
				P.Quad(Pt(E0.X, E0.Y), Pt(E1.X, E1.Y), Pt(B1.X, B1.Y), Pt(B0.X, B0.Y), C(0x7E8690));
				P.Quad(Pt(E0.X, E0.Y), Pt(E1.X, E1.Y), Pt(E1.X, E1.Y - 7.0f), Pt(E0.X, E0.Y - 7.0f), C(0xAAB3BD));
				P.Segment(Pt(B0.X, B0.Y + 2.0f), Pt(B1.X, B1.Y + 2.0f), A(Ink, 0.35f), 2.0f);
			}
			P.Quad(Pt(-6.0f, 2.0f), Pt(16.0f, 2.0f), Pt(18.0f, -30.0f), Pt(-4.0f, -34.0f), C(0x3E424A));
			TArray<FVector2f> DullPts;
			TArray<FVector2f> KeenPts;
			for (int32 i = 0; i <= N; ++i)
			{
				const float T = float(i) / float(N);
				const FVector2f E = Pt(EdgeLine[i].X, EdgeLine[i].Y - 1.0f);
				DullPts.Add(E);
				if (T <= Sh + 0.001f)
				{
					KeenPts.Add(E);
				}
			}
			P.Line(DullPts, Dull, 3.0f);
			if (KeenPts.Num() >= 2)
			{
				P.Line(KeenPts, Keen, 3.5f);
			}
			if (GlintT >= 0.0f && Sh > 0.01f)
			{
				const float K = Saturate(GlintT / 0.7f);
				const float F = Sh * K * float(N);
				const int32 I0 = FMath::Clamp(FMath::FloorToInt(F), 0, N - 1);
				const FVector2f E = FMath::Lerp(EdgeLine[I0], EdgeLine[I0 + 1], F - float(I0));
				KGMgCraft::Sparkle(P, Pt(E.X, E.Y - 3.0f), 5.0f + 11.0f * FMath::Sin(K * PI), Keen);
			}
		}
		if (WrongT > 0.0f)
		{
			P.Glow(O, 36.0f, A(Fire, WrongT * 1.6f));
		}
	}

	void PaintAngleArc(FKGMgPainter& P) const
	{
		const FVector2f O = ContactPoint();
		const bool bIn = InBand(AngleDeg);
		const float R0 = Stage == 0 ? 128.0f : 150.0f;
		const float R1 = R0 + 14.0f;
		const float Base = -PI;
		P.ArcBand(O, R0, R1, Base, Base + FMath::DegreesToRadians(MaxDeg), A(Ink, 0.45f), 24);
		P.ArcBand(O, R0 - 3.0f, R1 + 3.0f, Base + FMath::DegreesToRadians(BandLo()), Base + FMath::DegreesToRadians(BandHi()),
		          A(Good, 0.85f), 8);
		const float N = Base + FMath::DegreesToRadians(AngleDeg);
		const FVector2f D(FMath::Cos(N), FMath::Sin(N));
		P.Bar(O + D * (R0 - 12.0f), O + D * (R1 + 14.0f), 4.0f, bIn ? Good : C(0xFF6A5A));
		P.Circle(O + D * (R1 + 14.0f), 4.0f, bIn ? Good : C(0xFF6A5A));
	}

	void PaintHud(FKGMgPainter& P) const
	{
		const bool bIn = InBand(AngleDeg);
		// Angle readout.
		KGMgCraft::HudPanel(P, FVector2f(486.0f, 12.0f), FVector2f(140.0f, 78.0f));
		KGMgCraft::Caption(P, FVector2f(556.0f, 19.0f), TEXT("ANGLE"), CreamDim, 0.5f);
		P.Text(FVector2f(556.0f, 32.0f), FString::Printf(TEXT("%d°"), FMath::RoundToInt(AngleDeg)), 26.0f, bIn ? Good : C(0xFF8A80), 0.5f,
		       TEXT("Black"));
		KGMgCraft::Caption(P, FVector2f(556.0f, 72.0f), FString::Printf(TEXT("AIM %d-%d°"), FMath::RoundToInt(BandLo()), FMath::RoundToInt(BandHi())),
		                   Cream, 0.5f);

		// Edge progress.
		KGMgCraft::HudPanel(P, FVector2f(166.0f, 12.0f), FVector2f(304.0f, 46.0f));
		KGMgCraft::Caption(P, FVector2f(180.0f, 28.0f), TEXT("EDGE"), CreamDim);
		KGMgCraft::Pips(P, FVector2f(232.0f, 35.0f), Needed, Sharp, 7.5f, C(0xDDEBF5));
		P.Text(FVector2f(458.0f, 24.0f), FString::Printf(TEXT("%d/%d"), FMath::FloorToInt(Sharp), Needed), 14.0f, Cream, 1.0f, TEXT("Black"));

		// Hand-height strip: keep the mouse in the green.
		const float SX = 604.0f;
		const float YTop = 100.0f;
		const float YBot = AngleZeroY;
		P.RoundRect(FVector2f(SX, YTop), FVector2f(24.0f, YBot - YTop), A(Ink, 0.55f), 12.0f, A(Cream, 0.25f), 1.5f);
		const float BY0 = YForAngle(BandHi());
		const float BY1 = YForAngle(BandLo());
		P.RoundRect(FVector2f(SX - 2.0f, BY0), FVector2f(28.0f, BY1 - BY0), A(Good, 0.5f), 6.0f, Good, 2.0f);
		const float MY = FMath::Clamp(Mouse.Y, YTop, YBot);
		P.Bar(FVector2f(SX, MY), FVector2f(SX + 24.0f, MY), 3.0f, Cream);
		P.Tri(FVector2f(SX - 3.0f, MY), FVector2f(SX - 15.0f, MY - 8.0f), FVector2f(SX - 15.0f, MY + 8.0f), bIn ? Good : Cream);
		KGMgCraft::Caption(P, FVector2f(SX + 12.0f, YBot + 6.0f), TEXT("HAND"), Cream, 0.5f);
		const float BandMid = (BY0 + BY1) * 0.5f;
		if (!bIn && FMath::Abs(MY - BandMid) > 26.0f && !bSolved)
		{
			P.HintArrow(FVector2f(SX - 34.0f, MY), FVector2f(SX - 34.0f, BandMid), Time, Cream);
		}

		// First-time hint: hold on the stone and stroke it end to end.
		if (Sharp <= 0.0f && StrokeSide == 0 && !Held())
		{
			P.HintRing(ContactPoint(), 26.0f, Time, Gold);
			P.HintArrow(FVector2f(StoneL + 30.0f, StoneTop + 72.0f), FVector2f(StoneR - 30.0f, StoneTop + 72.0f), Time, Cream);
			P.Tag(FVector2f((StoneL + StoneR) * 0.5f, StoneTop + 100.0f), TEXT("HOLD + STROKE"), A(Ink, 0.75f), Gold, 13.0f);
		}
	}
};

// =====================================================================================================================
// BakeBread: knead the glowing spots, trace the loaf outline, then pull it from the oven when it turns golden.
// =====================================================================================================================
class FKGMgBakeBread final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 3; }

	virtual FString GetInstruction() const override
	{
		switch (Stage)
		{
		case 0: return TEXT("Press the dough where the ring glows: knead it smooth");
		case 1: return TEXT("Hold and trace the dashed outline all the way round to shape the loaf");
		default: return TEXT("Toss logs to keep the fire up, then TAKE OUT the loaf when it turns golden");
		}
	}

	virtual void BeginStage() override
	{
		Bits.Reset();
		Dents.Reset();
		Trail.Reset();
		Kneads = 0;
		SquashT = -1.0f;
		StickT = 0.0f;
		SpotLocal = FVector2f(0.0f, -0.5f);
		NewSpot();
		AutoT = 0.0f;
		Covered.Init(false, Segs);
		NumCovered = 0;
		Pinch = Mouse;
		bPinch = false;
		bLost = false;
		Brown = 0.0f;
		FireLvl = 1.0f;
		Flare = 0.0f;
		LogsFlying.Reset();
		BurntT = -1.0f;
		TakeT = -1.0f;
		SmokeAcc = 0.0f;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			PressDough(Pos);
		}
		else if (Stage == 1)
		{
			bPinch = true;
			bLost = false;
			Pinch = Pos;
			Trail.Reset();
			Sound(TEXT("S_UI_Click"), 0.4f, 1.1f);
		}
		else
		{
			if (InRect(Pos, FVector2f(0.0f, 286.0f), FVector2f(140.0f, 114.0f)))
			{
				TossLog();
			}
			else if (InRect(Pos, ButtonPos - FVector2f(6.0f, 6.0f), ButtonSize + FVector2f(12.0f, 12.0f)) ||
			         FVector2f::Distance(Pos, LoafPos()) < 70.0f)
			{
				TakeOut();
			}
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		bPinch = false;
		bLost = false;
	}

	virtual void OnKey(const FKey& Key) override
	{
		if (Stage == 2 && Key == EKeys::SpaceBar)
		{
			TakeOut();
		}
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickKnead(Dt);
			KGMgCraft::TickBits(Bits, Dt, 120.0f);
		}
		else if (Stage == 1)
		{
			TickShape(Dt);
			KGMgCraft::TickBits(Bits, Dt, 120.0f);
		}
		else
		{
			TickBake(Dt);
			KGMgCraft::TickBits(Bits, Dt, -15.0f);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			AutoT += Dt;
			if (AutoT > 0.35f && SquashT < 0.0f && StickT <= 0.0f)
			{
				AutoT = 0.0f;
				Mouse = SpotWorld();
				PressDough(Mouse);
			}
		}
		else if (Stage == 1)
		{
			if (!bPinch)
			{
				bPinch = true;
				bLost = false;
				Pinch = OnLoaf(-0.5f * PI);
			}
			Mouse = OnLoaf(LoafParam(Pinch) + 0.3f);
		}
		else
		{
			if (FireLvl < 0.5f && LogsFlying.Num() == 0)
			{
				TossLog();
			}
			if (Brown >= 0.78f)
			{
				TakeOut();
			}
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			Kneads = 6;
			SquashT = -1.0f;
			for (int32 i = 0; i < 4; ++i)
			{
				Dents.Add({DoughC + FVector2f(-60.0f + float(i) * 40.0f, -10.0f + float(i % 2) * 24.0f), 0.3f * float(i)});
			}
			Mouse = SpotWorld() + FVector2f(40.0f, 30.0f);
		}
		else if (Stage == 1)
		{
			const int32 Upto = Segs * 3 / 5;
			for (int32 i = 0; i < Segs; ++i)
			{
				const int32 Idx = (7 + i) % Segs;
				Covered[Idx] = i < Upto;
			}
			NumCovered = Upto;
			bPinch = true;
			const float Ang = -PI + (float((7 + Upto) % Segs) + 0.2f) * UE_TWO_PI / float(Segs);
			Pinch = OnLoaf(Ang);
			Mouse = Pinch;
			for (int32 i = 0; i < 12; ++i)
			{
				Trail.Add(OnLoaf(Ang - 0.04f * float(12 - i)));
			}
		}
		else
		{
			Brown = 0.62f;
			FireLvl = 0.55f;
			LogsFlying = {0.25f};
			for (int32 i = 0; i < 30; ++i)
			{
				SpawnSmoke();
				KGMgCraft::TickBits(Bits, 0.08f, -15.0f);
			}
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintKnead(P);
		}
		else if (Stage == 1)
		{
			PaintShape(P);
		}
		else
		{
			PaintBake(P);
		}
	}

private:
	struct FDent
	{
		FVector2f P;
		float Age;
	};

	// Knead
	static constexpr int32 KneadsNeeded = 16;
	static constexpr float SquashTime = 0.26f;   // lock per knead: 16 x 0.26 >= 4.1 s (floor 3.0)
	const FVector2f DoughC = FVector2f(320.0f, 246.0f);
	const FVector2f DoughR = FVector2f(130.0f, 84.0f);
	// Shape
	static constexpr int32 Segs = 28;
	static constexpr float PinchSpeed = 360.0f;   // loop >= 2.0 s (floor 2.0, + 0.8 s celebration before)
	const FVector2f LoafC = FVector2f(320.0f, 240.0f);
	const FVector2f LoafR = FVector2f(184.0f, 94.0f);
	// Bake
	static constexpr float GoldenLo = 0.7f;
	static constexpr float GoldenHi = 0.9f;
	static constexpr float MaxBakeRate = 0.7f / 6.2f;   // golden no sooner than 6.2 s (floor 6.0)
	const FVector2f ButtonPos = FVector2f(462.0f, 324.0f);
	const FVector2f ButtonSize = FVector2f(164.0f, 58.0f);

	TArray<KGMgCraft::FBit> Bits;
	TArray<FDent> Dents;
	int32 Kneads = 0;
	float SquashT = -1.0f;
	float StickT = 0.0f;
	FVector2f SpotLocal = FVector2f::ZeroVector;
	float AutoT = 0.0f;
	TArray<bool> Covered;
	int32 NumCovered = 0;
	FVector2f Pinch = FVector2f::ZeroVector;
	bool bPinch = false;
	bool bLost = false;
	TArray<FVector2f> Trail;
	float Brown = 0.0f;
	float FireLvl = 1.0f;
	float Flare = 0.0f;
	TArray<float> LogsFlying;
	float BurntT = -1.0f;
	float TakeT = -1.0f;
	float SmokeAcc = 0.0f;

	// ---- knead ------------------------------------------------------------------------------------------------------

	FVector2f SpotWorld() const { return DoughC + FVector2f(SpotLocal.X * DoughR.X, SpotLocal.Y * DoughR.Y) * 0.62f; }

	void NewSpot()
	{
		const FVector2f Prev = SpotLocal;
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const float Ang = Rng.FRand() * UE_TWO_PI;
			const float Rad = 0.35f + 0.65f * FMath::Sqrt(Rng.FRand());
			SpotLocal = FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Rad;
			if (FVector2f::Distance(SpotLocal, Prev) > 0.7f)
			{
				break;
			}
		}
	}

	void PressDough(const FVector2f& Pos)
	{
		if (SquashT >= 0.0f || StickT > 0.0f || bSolved)
		{
			return;
		}
		if (FVector2f::Distance(Pos, SpotWorld()) <= 36.0f)
		{
			++Kneads;
			SquashT = 0.0f;
			Dents.Add({Pos, 0.0f});
			KGMgCraft::Burst(Bits, Rng, Pos, 10, 0.0f, UE_TWO_PI, 30.0f, 110.0f, FLinearColor::White, C(0xF6EEDC), 3.5f, 0.6f, false);
			Sound(TEXT("S_Chore_Squish"), 0.8f, 0.85f + 0.03f * float(Kneads));
			if (Kneads == KneadsNeeded / 2)
			{
				PopText(Pos + FVector2f(0.0f, -60.0f), TEXT("Halfway!"), Gold);
			}
		}
		else
		{
			const FVector2f D = Pos - DoughC;
			if ((D.X * D.X) / (DoughR.X * DoughR.X) + (D.Y * D.Y) / (DoughR.Y * DoughR.Y) <= 1.1f)
			{
				StickT = 0.45f;
				Oops(Pos + FVector2f(0.0f, -50.0f), TEXT("Sticky! Press the ring"), TEXT("S_Chore_Squish"), 3.0f);
			}
		}
	}

	void TickKnead(float Dt)
	{
		StickT = FMath::Max(0.0f, StickT - Dt);
		if (SquashT >= 0.0f)
		{
			SquashT += Dt;
			if (SquashT >= SquashTime)
			{
				SquashT = -1.0f;
				if (Kneads >= KneadsNeeded)
				{
					if (!bSolved)
					{
						Nice(DoughC + FVector2f(0.0f, -110.0f), TEXT("Smooth dough!"), TEXT("S_UI_Good"), 1.1f);
						Solve();
					}
				}
				else
				{
					NewSpot();
				}
			}
		}
		for (int32 i = Dents.Num() - 1; i >= 0; --i)
		{
			Dents[i].Age += Dt;
			if (Dents[i].Age > 1.5f)
			{
				Dents.RemoveAt(i);
			}
		}
	}

	void PaintKitchen(FKGMgPainter& P) const
	{
		// Plaster wall with timber frame, copper pan, shelf of jars.
		P.RectV(FVector2f(0, 0), FVector2f(W, 142.0f), C(0xEAD7B4), C(0xD5BC92));
		P.Rect(FVector2f(0, 0), FVector2f(W, 14.0f), WoodDark);
		P.Rect(FVector2f(150.0f, 0), FVector2f(14.0f, 142.0f), WoodDark);
		P.Rect(FVector2f(470.0f, 0), FVector2f(14.0f, 142.0f), WoodDark);
		P.Quad(FVector2f(164.0f, 14.0f), FVector2f(176.0f, 14.0f), FVector2f(470.0f, 130.0f), FVector2f(458.0f, 130.0f), A(WoodDark, 0.85f));
		P.Bar(FVector2f(60.0f, 26.0f), FVector2f(60.0f, 46.0f), 3.0f, Iron);
		P.Circle(FVector2f(60.0f, 74.0f), 28.0f, C(0xC87533), C(0x8A4A1E), 4.0f);
		P.Circle(FVector2f(52.0f, 66.0f), 9.0f, A(C(0xFFD0A0), 0.45f));
		P.Rect(FVector2f(500.0f, 80.0f), FVector2f(130.0f, 8.0f), Wood);
		const uint32 JarCols[] = {0xE8A020, 0xC8403A, 0x6DB33F};
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = 512.0f + float(i) * 40.0f;
			P.RoundRect(FVector2f(X, 50.0f), FVector2f(28.0f, 30.0f), C(JarCols[i]), 6.0f, A(Ink, 0.4f), 1.5f);
			P.Rect(FVector2f(X - 2.0f, 46.0f), FVector2f(32.0f, 7.0f), C(0xE8DCC0));
		}
		// Table.
		KGMgCraft::Planks(P, FVector2f(0, 142.0f), FVector2f(W, 258.0f), 52.0f, C(0xCC9862), C(0x94643C), false);
		P.Rect(FVector2f(0, 142.0f), FVector2f(W, 4.0f), C(0xE0B684));
		P.Disc(FVector2f(320.0f, 250.0f), FVector2f(250.0f, 120.0f), A(FLinearColor::White, 0.42f), A(FLinearColor::White, 0.0f), 40);
		P.Disc(FVector2f(120.0f, 340.0f), FVector2f(70.0f, 26.0f), A(FLinearColor::White, 0.3f), A(FLinearColor::White, 0.0f), 24);
		// Rolling pin + flour sack.
		P.Bar(FVector2f(28.0f, 372.0f), FVector2f(60.0f, 368.0f), 8.0f, WoodDark);
		P.RoundRect(FVector2f(58.0f, 355.0f), FVector2f(130.0f, 26.0f), C(0xD9B07A), -1.0f, C(0xA8744A), 2.0f);
		P.Bar(FVector2f(186.0f, 366.0f), FVector2f(218.0f, 362.0f), 8.0f, WoodDark);
		P.RoundRect(FVector2f(540.0f, 280.0f), FVector2f(86.0f, 104.0f), C(0xE8DCC0), 24.0f, C(0xB8A888), 2.0f);
		P.RoundRect(FVector2f(548.0f, 268.0f), FVector2f(70.0f, 24.0f), C(0xD8CBAE), 10.0f);
		P.Text(FVector2f(583.0f, 320.0f), TEXT("FLOUR"), 12.0f, C(0x8A7A5A), 0.5f, TEXT("Black"));
	}

	void PaintDough(FKGMgPainter& P, const FVector2f& Ctr, const FVector2f& R, float Smooth, float Squash) const
	{
		const FVector2f Rs(R.X * (1.0f + 0.2f * Squash), R.Y * (1.0f - 0.24f * Squash));
		const FVector2f C0 = Ctr + FVector2f(0.0f, R.Y - Rs.Y);
		P.Disc(C0 + FVector2f(0.0f, Rs.Y * 0.82f), FVector2f(Rs.X * 1.06f, Rs.Y * 0.32f), A(Ink, 0.28f), A(Ink, 0.0f), 32);
		P.Disc(C0, Rs, C(0xFCEDCD), C(0xE0BA82), 40);
		for (int32 i = 0; i < 7; ++i)
		{
			const float Ang = float(i) * 0.9f + 0.4f;
			const FVector2f L = C0 + FVector2f(FMath::Cos(Ang) * Rs.X * 0.55f, FMath::Sin(Ang) * Rs.Y * 0.5f);
			const float Sz = 0.8f + 0.12f * float(i % 3);
			P.Disc(L, FVector2f(20.0f, 13.0f) * Sz, A(C(0xE6C48E), 0.95f * (1.0f - Smooth)), A(C(0xE6C48E), 0.0f), 16);
			P.Arc(L + FVector2f(0.0f, 5.0f), 11.0f * Sz, 0.2f * PI, 0.8f * PI, A(C(0xB8925A), 0.7f * (1.0f - Smooth)), 2.0f, 8);
		}
		P.Disc(C0 + FVector2f(-Rs.X * 0.3f, -Rs.Y * 0.42f), FVector2f(Rs.X * 0.4f, Rs.Y * 0.22f), A(FLinearColor::White, 0.6f),
		       A(FLinearColor::White, 0.0f), 24);
		for (int32 i = 0; i < 9; ++i)
		{
			const FVector2f D(FMath::Cos(float(i) * 2.4f) * 0.7f, FMath::Sin(float(i) * 2.4f) * 0.6f);
			P.Circle(C0 + FVector2f(D.X * Rs.X, D.Y * Rs.Y), 2.0f, A(FLinearColor::White, 0.7f));
		}
	}

	void PaintKnead(FKGMgPainter& P) const
	{
		PaintKitchen(P);
		const float Squash = SquashT >= 0.0f ? FMath::Sin(PI * Saturate(SquashT / SquashTime)) : 0.0f;
		const float Smooth = Saturate(float(Kneads) / float(KneadsNeeded));
		PaintDough(P, DoughC, DoughR, Smooth, Squash);
		for (const FDent& D : Dents)
		{
			const float K = 1.0f - Saturate(D.Age / 1.5f);
			P.Disc(D.P, FVector2f(16.0f, 10.0f), A(C(0xC99A62), 0.6f * K), A(C(0xC99A62), 0.0f), 16);
			P.Arc(D.P + FVector2f(0.0f, 2.0f), 12.0f, 0.15f * PI, 0.85f * PI, A(C(0xA87844), 0.6f * K), 2.0f, 8);
		}
		KGMgCraft::PaintBits(P, Bits);
		if (SquashT < 0.0f && !bSolved)
		{
			const FVector2f S = SpotWorld();
			const FLinearColor Col = StickT > 0.0f ? A(Cream, 0.5f) : Gold;
			P.Circle(S, 30.0f, A(Gold, 0.22f), Col, 3.0f);
			P.Circle(S, 8.0f, A(Col, 0.9f));
			P.HintRing(S, 36.0f, Time, Cream);
			if (Kneads == 0)
			{
				P.Tag(S + FVector2f(0.0f, -56.0f), TEXT("PRESS"), A(Ink, 0.75f), Gold, 13.0f);
			}
		}
		if (StickT > 0.0f)
		{
			P.Tag(DoughC + FVector2f(0.0f, 120.0f), TEXT("Sticky..."), A(Crimson, 0.9f), Cream, 13.0f);
		}
		// Progress.
		KGMgCraft::HudPanel(P, FVector2f(196.0f, 18.0f), FVector2f(248.0f, 56.0f));
		KGMgCraft::Caption(P, FVector2f(210.0f, 26.0f), TEXT("KNEADED"), CreamDim);
		P.Text(FVector2f(430.0f, 22.0f), FString::Printf(TEXT("%d / %d"), Kneads, KneadsNeeded), 14.0f, Cream, 1.0f, TEXT("Black"));
		P.Gauge(FVector2f(208.0f, 46.0f), FVector2f(224.0f, 18.0f), Smooth, C(0xF3D9A0));
	}

	// ---- shape ------------------------------------------------------------------------------------------------------

	float LoafParam(const FVector2f& Q) const { return FMath::Atan2((Q.Y - LoafC.Y) / LoafR.Y, (Q.X - LoafC.X) / LoafR.X); }
	FVector2f OnLoaf(float T) const { return LoafC + FVector2f(FMath::Cos(T) * LoafR.X, FMath::Sin(T) * LoafR.Y); }

	void TickShape(float Dt)
	{
		if (bPinch && !bLost && !bSolved)
		{
			const FVector2f D = Mouse - Pinch;
			const float Len = D.Size();
			const float MaxStep = PinchSpeed * Dt;
			Pinch = Len > MaxStep ? Pinch + D / Len * MaxStep : Mouse;
			const float T = LoafParam(Pinch);
			const float Dist = FVector2f::Distance(Pinch, OnLoaf(T));
			if (Dist <= 28.0f)
			{
				const int32 Idx = FMath::Clamp(FMath::FloorToInt((T + PI) / UE_TWO_PI * float(Segs)), 0, Segs - 1);
				if (!Covered[Idx])
				{
					Covered[Idx] = true;
					++NumCovered;
					Sound(TEXT("S_Chore_Squish"), 0.3f, 0.8f + 0.6f * float(NumCovered) / float(Segs));
					if (NumCovered >= Segs)
					{
						Nice(LoafC + FVector2f(0.0f, -130.0f), TEXT("Lovely loaf!"), TEXT("S_UI_Good"), 1.1f);
						Solve();
						bPinch = false;
					}
				}
			}
			else if (Dist > 50.0f)
			{
				bLost = true;
				Oops(Pinch + FVector2f(0.0f, -40.0f), TEXT("Stay on the line!"), TEXT("S_UI_Bad"), 3.0f);
			}
			Trail.Add(Pinch);
			if (Trail.Num() > 24)
			{
				Trail.RemoveAt(0);
			}
		}
		else
		{
			Pinch = Mouse;
			if (Trail.Num() > 0)
			{
				Trail.RemoveAt(0);
			}
		}
	}

	void PaintShape(FKGMgPainter& P) const
	{
		PaintKitchen(P);
		const float Frac = float(NumCovered) / float(Segs);
		const FVector2f R = FMath::Lerp(FVector2f(112.0f, 76.0f), FVector2f(172.0f, 84.0f), EaseOutCubic(Frac));
		PaintDough(P, LoafC + FVector2f(0.0f, 94.0f - R.Y - 4.0f), R, 0.85f + 0.15f * Frac, 0.0f);
		if (bSolved)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				const float X = LoafC.X - 60.0f + float(i) * 60.0f;
				P.Bar(FVector2f(X - 16.0f, LoafC.Y + 40.0f), FVector2f(X + 16.0f, LoafC.Y - 8.0f), 5.0f, A(C(0xC99A62), 0.8f));
			}
		}
		// Dashed outline; traced segments glow gold.
		const float Step = UE_TWO_PI / float(Segs);
		for (int32 i = 0; i < Segs; ++i)
		{
			const float A0 = -PI + float(i) * Step;
			const FVector2f S0 = OnLoaf(A0 + Step * 0.14f);
			const FVector2f S1 = OnLoaf(A0 + Step * 0.86f);
			if (Covered[i])
			{
				P.Bar(S0, S1, 11.0f, A(Gold, 0.35f));
				P.Bar(S0, S1, 6.0f, Gold);
			}
			else
			{
				P.Bar(S0, S1, 4.0f, A(C(0x7A4A2A), 0.75f));
			}
		}
		// Trail and fingertip.
		if (Trail.Num() >= 2)
		{
			P.Line(Trail, A(bLost ? Crimson : FLinearColor::White, 0.6f), 5.0f);
		}
		if (!bSolved)
		{
			const bool bTracing = bPinch && !bLost;
			P.Circle(Pinch, 13.0f, A(C(0xF6E7C8), 0.95f), bLost ? Crimson : (bTracing ? Gold : A(Ink, 0.5f)), 3.0f);
			P.Circle(Pinch + FVector2f(-3.0f, -3.0f), 4.0f, A(FLinearColor::White, 0.8f));
		}
		if (NumCovered == 0 && !bPinch)
		{
			const FVector2f Start = OnLoaf(-0.5f * PI);
			P.HintRing(Start, 26.0f, Time, Gold);
			P.HintArrow(OnLoaf(-0.5f * PI + 0.12f), OnLoaf(-0.5f * PI + 0.75f), Time, C(0x7A4A2A));
			P.Tag(Start + FVector2f(0.0f, -38.0f), TEXT("HOLD + TRACE"), A(Ink, 0.75f), Gold, 13.0f);
		}
		if (bLost)
		{
			P.Tag(FVector2f(320.0f, 376.0f), TEXT("Release, then grab the line again"), A(Crimson, 0.9f), Cream, 13.0f);
		}
		KGMgCraft::HudPanel(P, FVector2f(226.0f, 18.0f), FVector2f(188.0f, 56.0f));
		KGMgCraft::Caption(P, FVector2f(240.0f, 26.0f), TEXT("SHAPED"), CreamDim);
		P.Text(FVector2f(400.0f, 22.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Frac * 100.0f)), 14.0f, Cream, 1.0f, TEXT("Black"));
		P.Gauge(FVector2f(238.0f, 46.0f), FVector2f(164.0f, 18.0f), Frac, Gold);
	}

	// ---- bake -------------------------------------------------------------------------------------------------------

	FVector2f LoafPos() const
	{
		FVector2f Base(282.0f, 288.0f);
		if (TakeT >= 0.0f)
		{
			const float K = EaseOutCubic(TakeT / 0.6f);
			Base += FVector2f(130.0f * K, 36.0f * K);
		}
		return Base;
	}

	static FLinearColor LoafColor(float B)
	{
		if (B < GoldenLo)
		{
			return Mix(C(0xF3E0B5), C(0xE3A552), B / GoldenLo);
		}
		if (B < GoldenHi)
		{
			return Mix(C(0xE3A552), C(0xB86A2A), (B - GoldenLo) / (GoldenHi - GoldenLo));
		}
		return Mix(C(0xB86A2A), C(0x2A1A12), Saturate((B - GoldenHi) / 0.12f));
	}

	void TossLog()
	{
		if (bSolved || LogsFlying.Num() >= 2 || FireLvl > 0.92f)
		{
			if (FireLvl > 0.92f && !bSolved)
			{
				PopText(FVector2f(70.0f, 250.0f), TEXT("Fire's roaring"), Cream);
			}
			return;
		}
		LogsFlying.Add(0.0f);
		Sound(TEXT("S_Chore_Whoosh"), 0.6f, 0.9f);
	}

	void TakeOut()
	{
		if (bSolved || BurntT >= 0.0f || TakeT >= 0.0f)
		{
			return;
		}
		if (Brown < GoldenLo)
		{
			PopText(LoafPos() + FVector2f(0.0f, -70.0f), TEXT("Still pale - wait for golden"), Cream);
			Sound(TEXT("S_UI_Click"), 0.6f, 0.8f);
		}
		else if (Brown <= GoldenHi)
		{
			TakeT = 0.0f;
			Nice(LoafPos() + FVector2f(0.0f, -80.0f), TEXT("Golden crust!"), TEXT("S_Chore_Whoosh"), 1.0f);
			Solve();
		}
		else
		{
			Burn();
		}
	}

	void Burn()
	{
		BurntT = 0.0f;
		Oops(LoafPos() + FVector2f(0.0f, -80.0f), TEXT("Burnt! Fresh loaf..."), TEXT("S_UI_Bad"), 6.0f);
		for (int32 i = 0; i < 12; ++i)
		{
			KGMgCraft::FBit& B = Bits.AddDefaulted_GetRef();
			B.P = LoafPos() + FVector2f(-50.0f + Rng.FRand() * 100.0f, -10.0f);
			B.V = FVector2f(-10.0f + Rng.FRand() * 20.0f, -40.0f - Rng.FRand() * 40.0f);
			B.Life = 1.4f + Rng.FRand() * 0.6f;
			B.Size = 8.0f + Rng.FRand() * 6.0f;
			B.Grow = 1.5f;
			B.Col = A(C(0x3A3438), 0.8f);
		}
	}

	void SpawnSmoke()
	{
		if (Bits.Num() >= 120)
		{
			return;
		}
		KGMgCraft::FBit& B = Bits.AddDefaulted_GetRef();
		B.P = FVector2f(403.0f + Rng.FRand() * 10.0f, 2.0f + Rng.FRand() * 6.0f);
		B.V = FVector2f(12.0f + Rng.FRand() * 20.0f, -8.0f - Rng.FRand() * 12.0f);
		B.Life = 1.6f + Rng.FRand() * 0.8f;
		B.Size = 7.0f + Rng.FRand() * 5.0f;
		B.Grow = 1.2f;
		B.Col = A(C(0xE8E2DA), 0.75f);
	}

	void TickBake(float Dt)
	{
		Flare = FMath::Max(0.0f, Flare - Dt * 2.0f);
		if (TakeT >= 0.0f)
		{
			TakeT += Dt;
		}
		for (int32 i = LogsFlying.Num() - 1; i >= 0; --i)
		{
			LogsFlying[i] += Dt;
			if (LogsFlying[i] >= 0.5f)
			{
				LogsFlying.RemoveAt(i);
				FireLvl = FMath::Min(1.0f, FireLvl + 0.45f);
				Flare = 1.0f;
				Sound(TEXT("S_Chore_Flame"), 0.8f, 1.0f);
				KGMgCraft::Burst(Bits, Rng, FVector2f(220.0f, 300.0f), 10, -PI * 0.85f, -PI * 0.15f, 60.0f, 160.0f, Gold, Fire, 3.0f, 0.8f, false);
			}
		}
		if (BurntT >= 0.0f)
		{
			BurntT += Dt;
			if (BurntT >= 1.4f)
			{
				BurntT = -1.0f;
				Brown = 0.0f;
				PopText(LoafPos() + FVector2f(0.0f, -70.0f), TEXT("New loaf in"), Cream);
			}
		}
		else if (!bSolved)
		{
			FireLvl = FMath::Max(0.1f, FireLvl - Dt * 0.15f);
			Brown += MaxBakeRate * (0.25f + 0.75f * FireLvl) * Dt;
			if (Brown >= 1.0f)
			{
				Brown = 1.0f;
				Burn();
			}
		}
		SmokeAcc += Dt * (1.5f + 4.0f * FireLvl);
		while (SmokeAcc >= 1.0f)
		{
			SmokeAcc -= 1.0f;
			SpawnSmoke();
		}
	}

	void PaintBake(FKGMgPainter& P) const
	{
		// Bakery wall, tiled floor.
		P.RectV(FVector2f(0, 0), FVector2f(W, 340.0f), C(0xE6CFA4), C(0xCBAE80));
		P.RectV(FVector2f(0, 340.0f), FVector2f(W, 60.0f), C(0x8C6A4A), C(0x5E4430));
		for (int32 i = 0; i < 11; ++i)
		{
			P.Segment(FVector2f(float(i) * 64.0f, 340.0f), FVector2f(float(i) * 64.0f - 30.0f, 400.0f), A(Ink, 0.25f), 2.0f);
		}
		P.Rect(FVector2f(0, 368.0f), FVector2f(W, 2.0f), A(Ink, 0.25f));
		// Chimney and smoke (smoke drawn on top of it).
		P.RectV(FVector2f(384.0f, 0.0f), FVector2f(44.0f, 80.0f), C(0xA84E36), C(0x7A3624));
		for (int32 Row = 0; Row < 4; ++Row)
		{
			P.Rect(FVector2f(384.0f, float(Row) * 20.0f), FVector2f(44.0f, 2.0f), A(Ink, 0.3f));
		}
		KGMgCraft::PaintBits(P, Bits);

		// Oven dome + body with brick courses.
		const FLinearColor Brick = C(0xB65A3C);
		const FVector2f DomeC(290.0f, 180.0f);
		P.Poly(KGMgCraft::EllipsePts(DomeC, FVector2f(214.0f, 150.0f), 32, PI, UE_TWO_PI), Brick);
		P.RectV(FVector2f(76.0f, 180.0f), FVector2f(428.0f, 160.0f), Brick, C(0x8E4028));
		for (int32 k = 1; k < 4; ++k)
		{
			const float S = 1.0f - float(k) * 0.14f;
			P.Line(KGMgCraft::EllipsePts(DomeC, FVector2f(214.0f * S, 150.0f * S), 24, PI, UE_TWO_PI), A(Ink, 0.22f), 2.0f);
		}
		for (int32 Row = 0; Row < 7; ++Row)
		{
			const float Y = 190.0f + float(Row) * 22.0f;
			P.Rect(FVector2f(76.0f, Y), FVector2f(428.0f, 2.0f), A(Ink, 0.22f));
			for (int32 k = 0; k < 9; ++k)
			{
				const float X = 76.0f + float(k) * 50.0f + float(Row % 2) * 25.0f;
				if (X > 78.0f && X < 502.0f)
				{
					P.Rect(FVector2f(X, Y), FVector2f(2.0f, 22.0f), A(Ink, 0.22f));
				}
			}
		}
		P.Poly(KGMgCraft::EllipsePts(DomeC + FVector2f(-60.0f, -40.0f), FVector2f(70.0f, 40.0f), 16, PI, UE_TWO_PI), A(FLinearColor::White, 0.1f));

		// Mouth: dark arch with the fire at the back.
		const FVector2f MouthC(282.0f, 246.0f);
		const FLinearColor Dark = C(0x1A0E0A);
		P.Poly(KGMgCraft::EllipsePts(MouthC, FVector2f(124.0f, 84.0f), 24, PI, UE_TWO_PI), C(0x7A3624));
		P.Poly(KGMgCraft::EllipsePts(MouthC, FVector2f(112.0f, 74.0f), 24, PI, UE_TWO_PI), Dark);
		P.Rect(FVector2f(170.0f, 246.0f), FVector2f(224.0f, 78.0f), Dark);
		const float Fire01 = Saturate(FireLvl + Flare * 0.3f);
		P.Glow(FVector2f(250.0f, 300.0f), 120.0f + 60.0f * Fire01, A(Fire, 0.3f + 0.4f * Fire01));
		for (int32 i = 0; i < 6; ++i)
		{
			const float X = 186.0f + float(i) * 18.0f;
			const float Hgt = (16.0f + 58.0f * Fire01) * (0.7f + 0.3f * FMath::Sin(Time * 9.0f + float(i) * 1.9f));
			const float Sway = 4.0f * FMath::Sin(Time * 6.0f + float(i));
			P.Tri(FVector2f(X - 12.0f, 318.0f), FVector2f(X + 12.0f, 318.0f), FVector2f(X + Sway, 318.0f - Hgt), A(Fire, 0.85f));
			P.Tri(FVector2f(X - 6.0f, 318.0f), FVector2f(X + 6.0f, 318.0f), FVector2f(X + Sway, 318.0f - Hgt * 0.55f), A(C(0xFFE08A), 0.9f));
		}

		// Loaf on the peel.
		const FVector2f L = LoafPos();
		const FLinearColor Crust = BurntT >= 0.0f ? C(0x241612) : LoafColor(Brown);
		P.Bar(L + FVector2f(70.0f, 26.0f), FVector2f(470.0f + (L.X - 282.0f), 356.0f + (L.Y - 288.0f)), 10.0f, C(0xB98256));
		P.RoundRect(L + FVector2f(-78.0f, 20.0f), FVector2f(156.0f, 9.0f), C(0xC9A26B), 4.0f);
		P.Disc(L, FVector2f(66.0f, 28.0f), Mix(Crust, FLinearColor::White, 0.12f), Mix(Crust, Ink, 0.25f), 32);
		P.Disc(L + FVector2f(-12.0f, -12.0f), FVector2f(34.0f, 8.0f), A(FLinearColor::White, 0.25f), A(FLinearColor::White, 0.0f), 16);
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = L.X - 30.0f + float(i) * 30.0f;
			P.Bar(FVector2f(X - 10.0f, L.Y + 8.0f), FVector2f(X + 10.0f, L.Y - 16.0f), 4.0f, Mix(Crust, C(0xF6E7C8), 0.35f));
		}
		if (BurntT >= 0.0f)
		{
			P.Tag(L + FVector2f(0.0f, -56.0f), TEXT("BURNT!"), A(Crimson, 0.92f), Cream, 14.0f);
		}
		// Oven sill.
		P.Rect(FVector2f(150.0f, 322.0f), FVector2f(270.0f, 14.0f), Stone);
		P.Rect(FVector2f(150.0f, 322.0f), FVector2f(270.0f, 3.0f), C(0xB4B2BA));

		// Log pile + fire gauge.
		const int32 Rows[] = {3, 2, 1};
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 k = 0; k < Rows[Row]; ++k)
			{
				const FVector2f Ctr(34.0f + float(k) * 36.0f + float(Row) * 18.0f, 372.0f - float(Row) * 30.0f);
				P.Circle(Ctr, 17.0f, C(0x6E4A2E), C(0x4A2F1C), 3.0f);
				P.Circle(Ctr, 11.0f, C(0xD9B07A));
				P.Arc(Ctr, 6.0f, 0.0f, UE_TWO_PI, A(C(0x9C6B3F), 0.8f), 1.5f, 12);
			}
		}
		for (const float T : LogsFlying)
		{
			const float K = Saturate(T / 0.5f);
			const FVector2f From(70.0f, 330.0f);
			const FVector2f To(220.0f, 300.0f);
			const FVector2f At = FMath::Lerp(From, To, K) + FVector2f(0.0f, -130.0f * FMath::Sin(K * PI));
			P.RotRect(At, FVector2f(48.0f, 16.0f), K * 5.0f, C(0x7A5234));
			P.Circle(At + KGMgCraft::Rot(FVector2f(24.0f, 0.0f), K * 5.0f), 8.0f, C(0xD9B07A));
		}
		KGMgCraft::HudPanel(P, FVector2f(12.0f, 232.0f), FVector2f(124.0f, 46.0f));
		KGMgCraft::Caption(P, FVector2f(24.0f, 238.0f), TEXT("FIRE"), CreamDim);
		P.Gauge(FVector2f(22.0f, 254.0f), FVector2f(104.0f, 16.0f), FireLvl, FireLvl < 0.4f ? Lantern : Fire, 0.5f, 1.0f);
		if (FireLvl < 0.45f && LogsFlying.Num() == 0 && !bSolved)
		{
			P.HintRing(FVector2f(70.0f, 340.0f), 40.0f, Time, Gold);
			P.Tag(FVector2f(74.0f, 214.0f), TEXT("ADD A LOG"), A(Ink, 0.75f), Gold, 12.0f);
		}

		// Crust meter (right).
		const FVector2f MP(560.0f, 40.0f);
		const FVector2f MS(34.0f, 250.0f);
		auto MY = [&MP, &MS](float V) { return MP.Y + MS.Y * (1.0f - V); };
		P.Text(FVector2f(MP.X + MS.X * 0.5f, MP.Y - 26.0f), TEXT("CRUST"), 12.0f, Ink, 0.5f, TEXT("Black"), 80);
		P.RoundRect(MP - FVector2f(4.0f, 4.0f), MS + FVector2f(8.0f, 8.0f), A(Ink, 0.85f), 8.0f);
		P.RectV(FVector2f(MP.X, MY(GoldenLo)), FVector2f(MS.X, MY(0.0f) - MY(GoldenLo)), C(0xE3A552), C(0xF3E0B5));
		P.RectV(FVector2f(MP.X, MY(GoldenHi)), FVector2f(MS.X, MY(GoldenLo) - MY(GoldenHi)), C(0xB86A2A), C(0xE3A552));
		P.RectV(FVector2f(MP.X, MY(1.0f)), FVector2f(MS.X, MY(GoldenHi) - MY(1.0f)), C(0x1A100C), C(0x6A3A1A));
		P.RoundRect(FVector2f(MP.X - 3.0f, MY(GoldenHi)), FVector2f(MS.X + 6.0f, MY(GoldenLo) - MY(GoldenHi)), A(Good, 0.0f), 3.0f, Good, 3.0f);
		const float BY = MY(Saturate(Brown));
		P.Bar(FVector2f(MP.X - 6.0f, BY), FVector2f(MP.X + MS.X + 6.0f, BY), 4.0f, Cream);
		P.Tri(FVector2f(MP.X - 6.0f, BY), FVector2f(MP.X - 18.0f, BY - 9.0f), FVector2f(MP.X - 18.0f, BY + 9.0f), Cream);
		KGMgCraft::Caption(P, FVector2f(MP.X - 22.0f, MY(0.95f) - 6.0f), TEXT("BURNT"), C(0x6A2A1A), 1.0f);
		KGMgCraft::Caption(P, FVector2f(MP.X - 22.0f, MY(0.8f) - 6.0f), TEXT("GOLDEN"), C(0x3E7A20), 1.0f);
		KGMgCraft::Caption(P, FVector2f(MP.X - 22.0f, MY(0.25f) - 6.0f), TEXT("PALE"), C(0x8A6A40), 1.0f);

		// Take-out button.
		const bool bReady = Brown >= GoldenLo && Brown <= GoldenHi && BurntT < 0.0f;
		P.Shadow(ButtonPos, ButtonSize, 14.0f, 0.35f, 5.0f);
		P.RoundRect(ButtonPos, ButtonSize, bReady ? Mix(Gold, Good, Ping(Time, 0.5f)) : C(0xE8D8B8), 14.0f, WoodDark, 3.0f);
		P.Text(ButtonPos + FVector2f(ButtonSize.X * 0.5f, 16.0f), TEXT("TAKE OUT"), 20.0f, Ink, 0.5f, TEXT("Black"));
		if (bReady && !bSolved)
		{
			P.HintRing(ButtonPos + ButtonSize * 0.5f, 60.0f, Time, Good);
		}
		if (StageTime < 3.0f && !bSolved)
		{
			P.Tag(FVector2f(282.0f, 110.0f), TEXT("Wait for golden..."), A(Ink, 0.75f), Gold, 13.0f);
		}
	}
};

// =====================================================================================================================
// PourAle: hold to pour; the mouse height tilts the mug (tilted = less foam). Fill the ale to the line, no overflow.
// =====================================================================================================================
class FKGMgPourAle final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return TEXT("Hold to pour, raise the mouse to tilt the mug (less foam): stop with the ale on the line");
	}

	virtual void BeginStage() override
	{
		Bits.Reset();
		LineLevel = Stage == 0 ? 0.56f + Rng.FRand() * 0.06f : 0.66f + Rng.FRand() * 0.04f;
		TiltDeg = 0.0f;
		ServeT = -1.0f;
		bAutoDown = false;
		NewMug();
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (DumpT < 0.0f && ServeT < 0.0f && SlideT >= SlideTime && !bSolved)
		{
			bArmed = true;
			PourSoundT = 0.0f;
			Sound(TEXT("S_Chore_Pour"), 0.7f, 1.0f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (bArmed)
		{
			bArmed = false;
			Serve();
		}
	}

	virtual void Tick(float Dt) override
	{
		TiltDeg = Approach(TiltDeg, TargetTilt(), Dt, 8.0f);
		SlideT += Dt;
		if (ServeT >= 0.0f)
		{
			ServeT += Dt;
		}
		const bool bPour = bArmed && DumpT < 0.0f && !bSolved;
		Tap = Approach(Tap, bPour ? 1.0f : 0.0f, Dt, 18.0f);
		if (DumpT >= 0.0f)
		{
			DumpT += Dt;
			if (DumpT >= DumpTime)
			{
				NewMug();
			}
		}
		else if (!bSolved)
		{
			if (bPour)
			{
				const float Flow = FlowRate * Dt;
				const float Share = FMath::Lerp(0.62f, 0.08f, Saturate(TiltDeg / MaxTilt));
				Ale += Flow * (1.0f - Share);
				Foam += Flow * Share;
				PourSoundT += Dt;
				if (PourSoundT > 1.1f)
				{
					PourSoundT = 0.0f;
					Sound(TEXT("S_Chore_Pour"), 0.5f, 0.95f + 0.2f * Ale);
				}
				if (Rng.FRand() < 0.5f)
				{
					KGMgCraft::Burst(Bits, Rng, FVector2f(TapX, StreamEndY()), 1, -PI * 0.9f, -PI * 0.1f, 30.0f, 90.0f, C(0xFFF4DC),
					                 FLinearColor::White, 3.0f, 0.3f, false);
				}
			}
			const float Settle = Foam * 0.12f * Dt;
			Foam -= Settle;
			Ale += Settle * 0.25f;
			if (Ale + Foam > Capacity() + 0.004f)
			{
				Spill(TEXT("Overflow!"));
			}
			else if (Ale > LineLevel + OverTol)
			{
				Spill(TEXT("Over the line!"));
			}
		}
		KGMgCraft::TickBits(Bits, Dt, 900.0f, 372.0f);
	}

	virtual void AutoPlay(float Dt) override
	{
		if (DumpT >= 0.0f || ServeT >= 0.0f || SlideT < SlideTime)
		{
			return;
		}
		bArmed = true;
		bAutoDown = true;
		Mouse.Y = (Ale > LineLevel - 0.15f || Ale + Foam > Capacity() - 0.12f) ? 230.0f : 120.0f;
		if (Ale >= LineLevel)
		{
			bArmed = false;
			bAutoDown = false;
			Serve();
		}
	}

	virtual void DebugPose() override
	{
		SlideT = SlideTime;
		Ale = LineLevel - 0.14f;
		Foam = 0.07f;
		TiltDeg = 24.0f;
		Mouse = FVector2f(420.0f, TiltY(TiltDeg));
		bArmed = true;
		bAutoDown = true;
		Tap = 1.0f;
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintInn(P);
		PaintBarrel(P);
		PaintStream(P);
		PaintMug(P);
		KGMgCraft::PaintBits(P, Bits);
		PaintHud(P);
	}

private:
	static constexpr float MaxTilt = 38.0f;
	static constexpr float FlowRate = 0.15f;   // mug fractions per second: line >= 0.52 needs >= 3.5 s (floor 3.0)
	static constexpr float UnderTol = 0.035f;
	static constexpr float OverTol = 0.045f;
	static constexpr float SlideTime = 0.35f;
	static constexpr float DumpTime = 0.45f;
	static constexpr float TapX = 328.0f;
	static constexpr float InnerHalfW = 52.0f;
	static constexpr float InnerH = 134.0f;
	const FVector2f MouthAt = FVector2f(340.0f, 222.0f);

	TArray<KGMgCraft::FBit> Bits;
	float LineLevel = 0.6f;
	float Ale = 0.0f;
	float Foam = 0.0f;
	float TiltDeg = 0.0f;
	float Tap = 0.0f;
	float SlideT = 0.0f;
	float DumpT = -1.0f;
	float ServeT = -1.0f;
	float PourSoundT = 0.0f;
	bool bArmed = false;
	bool bAutoDown = false;

	float TargetTilt() const { return Saturate((340.0f - Mouse.Y) / 220.0f) * MaxTilt; }
	float TiltY(float Deg) const { return 340.0f - Deg / MaxTilt * 220.0f; }

	FVector2f MugOffset() const
	{
		float X = 0.0f;
		if (SlideT < SlideTime)
		{
			X += (1.0f - EaseOutCubic(SlideT / SlideTime)) * 320.0f;
		}
		if (DumpT >= 0.0f)
		{
			const float K = Saturate(DumpT / DumpTime);
			X += K * K * 340.0f;
		}
		if (ServeT >= 0.0f)
		{
			const float K = Saturate(ServeT / 0.6f);
			X -= K * K * 420.0f;
		}
		return FVector2f(X, 0.0f);
	}

	/** Mug-local (origin at the mouth centre, y down) to world, around the mouth so it stays under the tap. */
	FVector2f ToWorld(float X, float Y) const
	{
		return MouthAt + MugOffset() + KGMgCraft::Rot(FVector2f(X, Y), -FMath::DegreesToRadians(TiltDeg));
	}

	TArray<FVector2f> InnerPoly() const
	{
		return {ToWorld(-InnerHalfW, 0.0f), ToWorld(InnerHalfW, 0.0f), ToWorld(InnerHalfW, InnerH), ToWorld(-InnerHalfW, InnerH)};
	}

	float InnerArea() const { return InnerHalfW * 2.0f * InnerH; }

	/** Fraction of the mug that holds liquid at the current tilt (the lower lip spills first). */
	float Capacity() const
	{
		const TArray<FVector2f> Inner = InnerPoly();
		const float Lip = FMath::Max(Inner[0].Y, Inner[1].Y);
		return KGMgCraft::PolyArea(KGMgCraft::ClipBelow(Inner, Lip)) / InnerArea();
	}

	float StreamEndY() const
	{
		const TArray<FVector2f> Inner = InnerPoly();
		const float Hit = KGMgCraft::VerticalHit(Inner, TapX, 372.0f);
		if (Ale + Foam > 0.005f)
		{
			return FMath::Min(Hit, KGMgCraft::LevelForArea(Inner, FMath::Min(1.0f, Ale + Foam) * InnerArea()));
		}
		return Hit;
	}

	void NewMug()
	{
		Ale = 0.0f;
		Foam = 0.0f;
		DumpT = -1.0f;
		SlideT = 0.0f;
		bArmed = false;
		Tap = 0.0f;
	}

	void Spill(const TCHAR* Why)
	{
		const TArray<FVector2f> Inner = InnerPoly();
		const FVector2f Lip = Inner[0].Y > Inner[1].Y ? Inner[0] : Inner[1];
		KGMgCraft::Burst(Bits, Rng, Lip, 14, PI * 0.5f, PI * 0.95f, 40.0f, 160.0f, C(0xE89A2C), C(0xFFF4DC), 3.5f, 0.8f, false);
		Oops(MouthAt + FVector2f(0.0f, -60.0f), FString::Printf(TEXT("%s New mug"), Why), TEXT("S_Chore_Splash"), 5.0f);
		DumpT = 0.0f;
		bArmed = false;
	}

	void Serve()
	{
		if (bSolved || DumpT >= 0.0f || ServeT >= 0.0f || Ale < 0.03f)
		{
			return;
		}
		if (Ale < LineLevel - UnderTol)
		{
			PopText(MouthAt + FVector2f(0.0f, -50.0f), TEXT("A bit more - up to the line"), Cream);
			Sound(TEXT("S_UI_Click"), 0.6f, 0.9f);
			return;
		}
		ServeT = 0.0f;
		Nice(MouthAt + FVector2f(0.0f, -60.0f), Stage == 0 ? TEXT("Perfect pint!") : TEXT("Two fine pints!"), TEXT("S_UI_Good"),
		     1.0f + 0.1f * float(Stage));
		Solve();
	}

	void PaintInn(FKGMgPainter& P) const
	{
		KGMgCraft::Planks(P, FVector2f(0, 0), FVector2f(W, 372.0f), 58.0f, C(0x70482F), C(0x4A2E1E), true);
		// Lantern.
		P.Glow(FVector2f(80.0f, 170.0f), 190.0f, A(Lantern, 0.3f));
		P.Bar(FVector2f(80.0f, 100.0f), FVector2f(80.0f, 128.0f), 3.0f, Iron);
		P.Bar(FVector2f(60.0f, 100.0f), FVector2f(100.0f, 100.0f), 5.0f, Iron);
		P.RoundRect(FVector2f(64.0f, 128.0f), FVector2f(32.0f, 44.0f), A(C(0xFFE08A), 0.9f), 6.0f, Iron, 3.0f);
		P.Glow(FVector2f(80.0f, 150.0f), 26.0f, A(FLinearColor::White, 0.7f));
		P.Rect(FVector2f(60.0f, 170.0f), FVector2f(40.0f, 6.0f), Iron);
		// Shelf with bottles and mugs.
		P.Rect(FVector2f(470.0f, 92.0f), FVector2f(170.0f, 8.0f), WoodLight);
		P.Rect(FVector2f(470.0f, 100.0f), FVector2f(170.0f, 4.0f), WoodDark);
		const uint32 Bottles[] = {0x3E7A3A, 0x7A2A3A, 0x2E5E88, 0x9A6A20};
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = 484.0f + float(i) * 38.0f;
			P.RoundRect(FVector2f(X, 50.0f), FVector2f(22.0f, 42.0f), C(Bottles[i]), 6.0f);
			P.Rect(FVector2f(X + 7.0f, 34.0f), FVector2f(8.0f, 18.0f), C(Bottles[i]));
			P.Rect(FVector2f(X + 6.0f, 30.0f), FVector2f(10.0f, 5.0f), WoodLight);
			P.Rect(FVector2f(X + 4.0f, 56.0f), FVector2f(4.0f, 28.0f), A(FLinearColor::White, 0.3f));
		}
		// Counter.
		P.RectV(FVector2f(0, 372.0f), FVector2f(W, 28.0f), C(0x9A6440), C(0x5C3A26));
		P.Rect(FVector2f(0, 372.0f), FVector2f(W, 4.0f), C(0xC99A6A));
		P.Disc(FVector2f(200.0f, 374.0f), FVector2f(40.0f, 3.0f), A(C(0xE89A2C), 0.3f), A(C(0xE89A2C), 0.0f), 12);
	}

	void PaintBarrel(FKGMgPainter& P) const
	{
		const FVector2f Ctr(TapX + 26.0f, 8.0f);
		const float R = 116.0f;
		P.Circle(Ctr + FVector2f(0.0f, 8.0f), R + 6.0f, A(Ink, 0.35f));
		P.Circle(Ctr, R, C(0x9A6440), C(0x4A2F1C), 5.0f);
		for (int32 k = -3; k <= 3; ++k)
		{
			const float X = float(k) * 30.0f;
			const float Hh = FMath::Sqrt(FMath::Max(0.0f, R * R - X * X)) - 4.0f;
			P.Segment(Ctr + FVector2f(X, -Hh), Ctr + FVector2f(X, Hh), A(Ink, 0.35f), 2.0f);
		}
		P.Arc(Ctr, R - 12.0f, 0.0f, UE_TWO_PI, Iron, 8.0f, 48);
		P.Arc(Ctr, R - 12.0f, 0.15f * PI, 0.45f * PI, A(Cream, 0.2f), 2.0f, 12);
		P.Text(Ctr + FVector2f(0.0f, 24.0f), TEXT("ALE"), 22.0f, A(C(0xF6E7C8), 0.8f), 0.5f, TEXT("Black"), 200);
		// Rack chocks.
		P.Quad(FVector2f(Ctr.X - 118.0f, 96.0f), FVector2f(Ctr.X - 70.0f, 118.0f), FVector2f(Ctr.X - 70.0f, 132.0f), FVector2f(Ctr.X - 128.0f, 132.0f),
		       WoodDark);
		P.Quad(FVector2f(Ctr.X + 118.0f, 96.0f), FVector2f(Ctr.X + 70.0f, 118.0f), FVector2f(Ctr.X + 70.0f, 132.0f), FVector2f(Ctr.X + 128.0f, 132.0f),
		       WoodDark);
		// Brass tap with lever (tips forward while pouring).
		P.RoundRect(FVector2f(TapX - 14.0f, 90.0f), FVector2f(28.0f, 26.0f), C(0xD9A23A), 6.0f, C(0x8A6A20), 2.0f);
		P.Rect(FVector2f(TapX - 5.0f, 112.0f), FVector2f(10.0f, 20.0f), C(0xB8862B));
		P.Rect(FVector2f(TapX - 7.0f, 128.0f), FVector2f(14.0f, 5.0f), C(0x8A6A20));
		const FVector2f LeverBase(TapX, 94.0f);
		const FVector2f LeverEnd = LeverBase + KGMgCraft::Rot(FVector2f(0.0f, -40.0f), -0.9f * Tap);
		P.Bar(LeverBase, LeverEnd, 7.0f, C(0x3A2418));
		P.Circle(LeverEnd, 8.0f, C(0x5C3A26), C(0x3A2418), 2.0f);
		P.Circle(LeverBase, 5.0f, C(0xF2D27A));
	}

	void PaintStream(FKGMgPainter& P) const
	{
		if (Tap < 0.05f || DumpT >= 0.0f)
		{
			return;
		}
		const float EndY = StreamEndY();
		const float Wd = 2.0f + 6.0f * Tap;
		const float Wob = 1.2f * FMath::Sin(Time * 30.0f);
		P.Bar(FVector2f(TapX, 132.0f), FVector2f(TapX + Wob, EndY), Wd, C(0xE8A032));
		P.Bar(FVector2f(TapX - Wd * 0.2f, 132.0f), FVector2f(TapX - Wd * 0.2f + Wob, EndY), Wd * 0.3f, A(C(0xFFE3A0), 0.8f));
		for (int32 k = 0; k < 4; ++k)
		{
			const float Ph = FMath::Frac(Time * 2.5f + float(k) * 0.25f);
			P.Circle(FVector2f(TapX + (float(k % 2) * 2.0f - 1.0f) * 8.0f * Ph, EndY - 4.0f - 6.0f * FMath::Sin(Ph * PI)), 3.0f * (1.0f - Ph),
			         A(C(0xFFF4DC), 0.9f));
		}
	}

	void PaintMug(FKGMgPainter& P) const
	{
		auto Wd = [this](float X, float Y) { return ToWorld(X, Y); };
		const FLinearColor Glass = C(0xCFE8F0);
		// Shadow on the counter when upright-ish.
		const FVector2f Base = Wd(0.0f, 150.0f);
		P.Disc(FVector2f(Base.X, 374.0f), FVector2f(70.0f, 6.0f), A(Ink, 0.35f * Saturate(1.0f - (374.0f - Base.Y) / 60.0f)), A(Ink, 0.0f), 16);
		// Handle (behind the body edge).
		TArray<FVector2f> Handle;
		for (int32 i = 0; i <= 12; ++i)
		{
			const float T = -0.5f * PI + PI * float(i) / 12.0f;
			Handle.Add(Wd(62.0f + FMath::Cos(T) * 36.0f, 74.0f + FMath::Sin(T) * 44.0f));
		}
		P.Line(Handle, A(Glass, 0.55f), 12.0f);
		P.Line(Handle, A(FLinearColor::White, 0.35f), 3.0f);
		// Glass body.
		P.Quad(Wd(-64.0f, -8.0f), Wd(64.0f, -8.0f), Wd(62.0f, 150.0f), Wd(-62.0f, 150.0f), A(Glass, 0.22f));
		// Liquids: horizontal surfaces in the world.
		const TArray<FVector2f> Inner = InnerPoly();
		const float Area = InnerArea();
		const float Total = FMath::Min(1.0f, Ale + Foam);
		float Bottom = -1.0e6f;
		for (const FVector2f& V : Inner)
		{
			Bottom = FMath::Max(Bottom, V.Y);
		}
		if (Total > 0.003f)
		{
			const float TotY = KGMgCraft::LevelForArea(Inner, Total * Area);
			const TArray<FVector2f> FoamPoly = KGMgCraft::ClipBelow(Inner, TotY);
			if (FoamPoly.Num() >= 3)
			{
				P.Poly(FoamPoly, C(0xFFF4DC));
			}
			if (Ale > 0.003f)
			{
				const float AleY = KGMgCraft::LevelForArea(Inner, Ale * Area);
				const TArray<FVector2f> AlePoly = KGMgCraft::ClipBelow(Inner, AleY);
				if (AlePoly.Num() >= 3)
				{
					P.Poly(AlePoly, C(0xEE9F2E));
					const TArray<FVector2f> Deep = KGMgCraft::ClipBelow(Inner, FMath::Lerp(AleY, Bottom, 0.45f));
					if (Deep.Num() >= 3)
					{
						P.Poly(Deep, A(C(0xC9731A), 0.8f));
					}
				}
				// Rising bubbles inside the ale.
				for (int32 k = 0; k < 9; ++k)
				{
					const float Ph = FMath::Frac(Time * 0.45f + float(k) * 0.137f);
					const FVector2f B = Wd(-40.0f + float((k * 29) % 80), InnerH - 6.0f - Ph * (InnerH - 10.0f));
					if (B.Y > AleY + 4.0f)
					{
						P.Circle(B, 2.0f, A(C(0xFFE3A0), 0.7f));
					}
				}
			}
			// Foam head bubbles along the top surface.
			float X0 = 1.0e6f;
			float X1 = -1.0e6f;
			for (const FVector2f& V : FoamPoly)
			{
				if (FMath::Abs(V.Y - TotY) < 0.5f)
				{
					X0 = FMath::Min(X0, V.X);
					X1 = FMath::Max(X1, V.X);
				}
			}
			if (X1 > X0 && Foam > 0.01f)
			{
				const int32 N = FMath::Max(2, FMath::RoundToInt((X1 - X0) / 12.0f));
				for (int32 k = 0; k <= N; ++k)
				{
					const float X = FMath::Lerp(X0 + 5.0f, X1 - 5.0f, float(k) / float(N));
					P.Circle(FVector2f(X, TotY + 1.0f), 5.0f + float((k * 7) % 3), C(0xFFF8E8));
				}
			}
			else if (X1 > X0)
			{
				P.Bar(FVector2f(X0, TotY + 1.5f), FVector2f(X1, TotY + 1.5f), 3.0f, C(0xFFD27A));
			}
		}
		// Line mark on the glass.
		const float LineY = InnerH * (1.0f - LineLevel);
		const bool bOver = Ale > LineLevel + OverTol * 0.6f;
		const bool bNear = Ale >= LineLevel - UnderTol;
		const FLinearColor LineCol = bOver ? Crimson : (bNear ? Good : Gold);
		P.Bar(Wd(-InnerHalfW, LineY), Wd(InnerHalfW, LineY), 3.0f, LineCol);
		for (int32 k = 0; k < 5; ++k)
		{
			P.Bar(Wd(-InnerHalfW + 4.0f + float(k) * 24.0f, LineY - 5.0f), Wd(-InnerHalfW + 4.0f + float(k) * 24.0f, LineY + 5.0f), 2.0f, LineCol);
		}
		P.Tag(Wd(-InnerHalfW - 42.0f, LineY), TEXT("LINE"), A(Ink, 0.75f), LineCol, 11.0f);
		// Glass outline, thick base, rim, shine.
		const TArray<FVector2f> Outline = {Wd(-64.0f, -8.0f), Wd(-62.0f, 150.0f), Wd(62.0f, 150.0f), Wd(64.0f, -8.0f)};
		P.Line(Outline, A(FLinearColor::White, 0.55f), 3.0f);
		P.Quad(Wd(-62.0f, InnerH), Wd(62.0f, InnerH), Wd(62.0f, 150.0f), Wd(-62.0f, 150.0f), A(Glass, 0.45f));
		P.Bar(Wd(-64.0f, -8.0f), Wd(64.0f, -8.0f), 4.0f, A(FLinearColor::White, 0.7f));
		P.Bar(Wd(-46.0f, 12.0f), Wd(-46.0f, 120.0f), 6.0f, A(FLinearColor::White, 0.3f));
		P.Bar(Wd(-36.0f, 16.0f), Wd(-36.0f, 60.0f), 3.0f, A(FLinearColor::White, 0.25f));
	}

	void PaintHud(FKGMgPainter& P) const
	{
		// Mug counter.
		KGMgCraft::HudPanel(P, FVector2f(14.0f, 12.0f), FVector2f(128.0f, 44.0f));
		P.Text(FVector2f(78.0f, 22.0f), FString::Printf(TEXT("MUG %d / 2"), Stage + 1), 16.0f, Cream, 0.5f, TEXT("Black"));

		// Level gauge (upright-equivalent): ale + foam stacked, the line window, the rim.
		const FVector2f GP(528.0f, 150.0f);
		const FVector2f GS(30.0f, 200.0f);
		auto GY = [&GP, &GS](float V) { return GP.Y + GS.Y * (1.0f - V); };
		KGMgCraft::Caption(P, FVector2f(GP.X + GS.X * 0.5f, GP.Y - 36.0f), TEXT("LEVEL"), Cream, 0.5f);
		P.RoundRect(GP - FVector2f(4.0f, 4.0f), GS + FVector2f(8.0f, 8.0f), A(Ink, 0.85f), 8.0f, A(Cream, 0.3f), 1.5f);
		const float AleTop = GY(Saturate(Ale));
		const float TotTop = GY(Saturate(Ale + Foam));
		P.Rect(FVector2f(GP.X + 4.0f, TotTop), FVector2f(GS.X - 8.0f, AleTop - TotTop), C(0xFFF4DC));
		P.RectV(FVector2f(GP.X + 4.0f, AleTop), FVector2f(GS.X - 8.0f, GY(0.0f) - AleTop), C(0xF2B040), C(0xC9731A));
		P.RoundRect(FVector2f(GP.X - 3.0f, GY(LineLevel + OverTol)), FVector2f(GS.X + 6.0f, GY(LineLevel - UnderTol) - GY(LineLevel + OverTol)),
		            A(Good, 0.25f), 3.0f, Good, 2.0f);
		P.Bar(FVector2f(GP.X - 6.0f, GY(LineLevel)), FVector2f(GP.X + GS.X + 6.0f, GY(LineLevel)), 2.0f, Gold);
		const float Cap = Capacity();
		P.Bar(FVector2f(GP.X - 6.0f, GY(Cap)), FVector2f(GP.X + GS.X + 6.0f, GY(Cap)), 3.0f, Crimson);
		KGMgCraft::Caption(P, FVector2f(GP.X + GS.X * 0.5f, GY(Cap) - 18.0f), Cap < 0.99f ? TEXT("SPILLS") : TEXT("RIM"), C(0xFF8A80), 0.5f);
		KGMgCraft::Caption(P, FVector2f(GP.X + GS.X * 0.5f, GP.Y + GS.Y + 8.0f), FString::Printf(TEXT("FOAM %d%%"), FMath::RoundToInt(Foam * 100.0f)),
		                   Cream, 0.5f);

		// Tilt slider (right): up = tilted, less foam; down = upright, foamy.
		const float SX = 598.0f;
		const float Y0 = 120.0f;
		const float Y1 = 340.0f;
		P.RoundRect(FVector2f(SX, Y0), FVector2f(22.0f, Y1 - Y0), A(Ink, 0.55f), 11.0f, A(Cream, 0.25f), 1.5f);
		P.RectV(FVector2f(SX + 8.0f, Y0 + 10.0f), FVector2f(6.0f, Y1 - Y0 - 20.0f), A(Good, 0.8f), A(C(0xFFF4DC), 0.8f));
		const float KY = TiltY(TiltDeg);
		P.Circle(FVector2f(SX + 11.0f, KY), 12.0f, Gold, WoodDark, 3.0f);
		KGMgCraft::Caption(P, FVector2f(SX + 11.0f, Y0 - 30.0f), TEXT("TILT"), Cream, 0.5f);
		KGMgCraft::Caption(P, FVector2f(SX + 11.0f, Y0 - 16.0f), TEXT("low foam"), CreamDim, 0.5f);
		KGMgCraft::Caption(P, FVector2f(SX + 11.0f, Y1 + 6.0f), TEXT("FOAMY"), CreamDim, 0.5f);

		// Hints.
		if (Ale < 0.01f && !bArmed && DumpT < 0.0f && SlideT >= SlideTime)
		{
			P.HintRing(MouthAt + FVector2f(0.0f, 60.0f), 40.0f, Time, Gold);
			P.Tag(MouthAt + FVector2f(-150.0f, 60.0f), TEXT("HOLD TO POUR"), A(Ink, 0.75f), Gold, 13.0f);
			if (Stage == 0)
			{
				P.HintArrow(FVector2f(SX - 20.0f, 300.0f), FVector2f(SX - 20.0f, 170.0f), Time, Cream);
			}
		}
	}
};

// =====================================================================================================================
// StockStall: drag each good from the crate to the shelf slot whose tag matches it (pictures, then prices).
// =====================================================================================================================
class FKGMgStockStall final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Drag each good from the crate to the shelf spot whose tag shows it")
		                  : TEXT("Now the prices: drag each good to the tag with the same price as its sticker");
	}

	virtual void BeginStage() override
	{
		Items.Reset();
		SlotKind.Init(0, Count);
		SlotPrice.Init(0, Count);
		SlotItem.Init(-1, Count);
		Held = -1;
		LockT = 0.0f;
		Placed = 0;
		bFinish = false;
		AutoTarget = -1;
		TArray<int32> Spots;
		for (int32 i = 0; i < Count; ++i)
		{
			Spots.Add(i);
		}
		Rng.Shuffle(Spots);
		if (Stage == 0)
		{
			TArray<int32> Kinds;
			for (int32 i = 0; i < Count; ++i)
			{
				Kinds.Add(i);
			}
			Rng.Shuffle(Kinds);
			for (int32 s = 0; s < Count; ++s)
			{
				SlotKind[s] = Kinds[s];
			}
			for (int32 i = 0; i < Count; ++i)
			{
				AddItem(i, 0, CrateSpot(Spots[i]));
			}
		}
		else
		{
			TArray<int32> Pool = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12};
			Rng.Shuffle(Pool);
			TArray<int32> Prices;
			for (int32 i = 0; i < Count; ++i)
			{
				Prices.Add(Pool[i]);
			}
			for (int32 i = 0; i < Count; ++i)
			{
				AddItem(Rng.RandRange(0, 5), Prices[i], CrateSpot(Spots[i]));
			}
			Rng.Shuffle(Prices);
			for (int32 s = 0; s < Count; ++s)
			{
				SlotPrice[s] = Prices[s];
			}
		}
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (LockT > 0.0f || Held >= 0 || bFinish)
		{
			return;
		}
		int32 Best = -1;
		float BestD = 36.0f;
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const float D = FVector2f::Distance(Pos, Items[i].Pos);
			if (Items[i].Slot < 0 && D < BestD)
			{
				Best = i;
				BestD = D;
			}
		}
		if (Best >= 0)
		{
			Held = Best;
			Sound(TEXT("S_UI_Click"), 0.6f, 1.1f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { Drop(); }

	virtual void Tick(float Dt) override
	{
		LockT = FMath::Max(0.0f, LockT - Dt);
		if (bFinish && LockT <= 0.0f && !bSolved)
		{
			Nice(FVector2f(320.0f, 110.0f), Stage == 0 ? TEXT("Shelf stocked!") : TEXT("All priced!"), TEXT("S_Chore_Coin"), 1.2f);
			Solve();
		}
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			FItem& It = Items[i];
			if (i == Held)
			{
				const FVector2f D = Mouse - It.Pos;
				const float Len = D.Size();
				const float MaxStep = CarrySpeed * Dt;
				It.Pos = Len > MaxStep ? It.Pos + D / Len * MaxStep : Mouse;
			}
			else if (It.Slot >= 0)
			{
				It.Pos = KGMgCraft::ApproachV(It.Pos, SlotPos(It.Slot), Dt, 16.0f);
			}
			else
			{
				It.Pos = KGMgCraft::ApproachV(It.Pos, It.Home, Dt, 10.0f);
			}
			if (It.Hop >= 0.0f)
			{
				It.Hop += Dt;
				if (It.Hop > 0.6f)
				{
					It.Hop = -1.0f;
				}
			}
			It.Shake = FMath::Max(0.0f, It.Shake - Dt * 2.5f);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (LockT > 0.0f || bFinish)
		{
			return;
		}
		if (Held < 0)
		{
			for (int32 i = 0; i < Items.Num(); ++i)
			{
				if (Items[i].Slot < 0)
				{
					AutoTarget = TargetSlot(i);
					if (AutoTarget >= 0)
					{
						Held = i;
						Mouse = Items[i].Pos;
					}
					break;
				}
			}
			return;
		}
		if (AutoTarget < 0)
		{
			AutoTarget = TargetSlot(Held);
			if (AutoTarget < 0)
			{
				return;
			}
		}
		const FVector2f To = SlotPos(AutoTarget);
		const FVector2f D = To - Mouse;
		const float Len = D.Size();
		const float Step = 480.0f * Dt;
		Mouse = Len > Step ? Mouse + D / Len * Step : To;
		if (FVector2f::Distance(Items[Held].Pos, To) < 6.0f)
		{
			Drop();
			AutoTarget = -1;
		}
	}

	virtual void DebugPose() override
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const int32 S = TargetSlot(i);
			if (S >= 0)
			{
				Items[i].Slot = S;
				Items[i].Pos = SlotPos(S);
				SlotItem[S] = i;
				++Placed;
			}
		}
		const int32 T = TargetSlot(2);
		if (T >= 0)
		{
			Held = 2;
			Items[2].Pos = FMath::Lerp(Items[2].Home, SlotPos(T), 0.6f);
			Mouse = Items[2].Pos + FVector2f(6.0f, -4.0f);
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintStall(P);
		// Items resting in the crate go behind its front board; placed and held ones on top.
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].Slot < 0 && i != Held && Items[i].Pos.Y > 300.0f)
			{
				PaintItem(P, i, 1.0f);
			}
		}
		PaintCrateFront(P);
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].Slot >= 0 || (i != Held && Items[i].Pos.Y <= 300.0f))
			{
				PaintItem(P, i, 1.0f);
			}
		}
		if (Held >= 0)
		{
			const FVector2f Hp = Items[Held].Pos;
			P.Disc(Hp + FVector2f(6.0f, 30.0f), FVector2f(30.0f, 8.0f), A(Ink, 0.35f), A(Ink, 0.0f), 16);
			PaintItem(P, Held, 1.12f);
		}
		PaintHud(P);
	}

private:
	struct FItem
	{
		int32 Kind = 0;
		int32 Price = 0;
		FVector2f Home = FVector2f::ZeroVector;
		FVector2f Pos = FVector2f::ZeroVector;
		int32 Slot = -1;
		float Hop = -1.0f;
		float Shake = 0.0f;
	};

	static constexpr int32 Count = 6;
	static constexpr float CarrySpeed = 450.0f;   // 6 carries of >= 160 units + 5 x 0.4 s shelving >= 4.1 s (floor 4.0)
	static constexpr float PlaceLock = 0.4f;
	static constexpr float ShelfY = 184.0f;

	TArray<FItem> Items;
	TArray<int32> SlotKind;
	TArray<int32> SlotPrice;
	TArray<int32> SlotItem;
	int32 Held = -1;
	float LockT = 0.0f;
	int32 Placed = 0;
	bool bFinish = false;
	int32 AutoTarget = -1;

	static FVector2f SlotPos(int32 S) { return FVector2f(90.0f + float(S) * 92.0f, ShelfY - 24.0f); }
	static FVector2f CrateSpot(int32 K) { return FVector2f(152.0f + float(K) * 67.0f, 330.0f); }

	void AddItem(int32 Kind, int32 Price, const FVector2f& Home)
	{
		FItem& It = Items.AddDefaulted_GetRef();
		It.Kind = Kind;
		It.Price = Price;
		It.Home = Home;
		It.Pos = Home;
	}

	bool Matches(int32 Item, int32 S) const
	{
		return Stage == 0 ? SlotKind[S] == Items[Item].Kind : SlotPrice[S] == Items[Item].Price;
	}

	int32 TargetSlot(int32 Item) const
	{
		for (int32 S = 0; S < Count; ++S)
		{
			if (SlotItem[S] < 0 && Matches(Item, S))
			{
				return S;
			}
		}
		return -1;
	}

	void Drop()
	{
		if (Held < 0)
		{
			return;
		}
		const int32 I = Held;
		Held = -1;
		FItem& It = Items[I];
		int32 Best = -1;
		float BestD = 50.0f;
		for (int32 S = 0; S < Count; ++S)
		{
			const float D = FVector2f::Distance(It.Pos, SlotPos(S));
			if (D < BestD)
			{
				Best = S;
				BestD = D;
			}
		}
		if (Best < 0)
		{
			Sound(TEXT("S_UI_Click"), 0.4f, 0.8f);
			return;
		}
		if (SlotItem[Best] < 0 && Matches(I, Best))
		{
			It.Slot = Best;
			It.Hop = 0.0f;
			SlotItem[Best] = I;
			++Placed;
			LockT = PlaceLock;
			Nice(SlotPos(Best) + FVector2f(0.0f, -56.0f), Stage == 0 ? FString() : FString::Printf(TEXT("%dc"), It.Price),
			     Stage == 0 ? TEXT("S_Chore_Thud") : TEXT("S_Chore_Coin"), 0.9f + 0.06f * float(Placed));
			if (Placed >= Count)
			{
				bFinish = true;
			}
		}
		else
		{
			It.Shake = 1.0f;
			Oops(SlotPos(Best) + FVector2f(0.0f, -56.0f), Stage == 0 ? TEXT("Wrong spot!") : TEXT("Wrong price!"), TEXT("S_UI_Bad"), 4.0f);
		}
	}

	void PaintStall(FKGMgPainter& P) const
	{
		KGMgCraft::Outdoor(P, 300.0f);
		// Rooftops of the square in the distance.
		for (int32 i = 0; i < 6; ++i)
		{
			const float X = float(i) * 120.0f - 20.0f;
			P.Rect(FVector2f(X, 210.0f), FVector2f(96.0f, 90.0f), i % 2 ? C(0xE8D8B8) : C(0xD9C29A));
			P.Tri(FVector2f(X - 8.0f, 210.0f), FVector2f(X + 104.0f, 210.0f), FVector2f(X + 48.0f, 170.0f), i % 3 ? C(0xB5483A) : C(0x3F6E9E));
		}
		// Posts and back boards.
		P.RectV(FVector2f(14.0f, 40.0f), FVector2f(24.0f, 360.0f), WoodLight, WoodDark);
		P.RectV(FVector2f(602.0f, 40.0f), FVector2f(24.0f, 360.0f), WoodLight, WoodDark);
		KGMgCraft::Planks(P, FVector2f(38.0f, 60.0f), FVector2f(564.0f, 240.0f), 62.0f, C(0x8E5E3E), C(0x684028), true);
		// Awning: stripes, scalloped edge, shade.
		for (int32 i = 0; i < 16; ++i)
		{
			const FLinearColor Col = i % 2 ? Cream : C(0xC8423A);
			P.Rect(FVector2f(float(i) * 40.0f, 0.0f), FVector2f(40.0f, 58.0f), Col);
			P.Circle(FVector2f(float(i) * 40.0f + 20.0f, 58.0f), 20.0f, Col);
		}
		P.RectV(FVector2f(0, 0), FVector2f(W, 58.0f), A(FLinearColor::White, 0.18f), A(Ink, 0.12f));
		P.Bar(FVector2f(0.0f, 3.0f), FVector2f(W, 3.0f), 7.0f, WoodDark);
		P.RectV(FVector2f(38.0f, 78.0f), FVector2f(564.0f, 24.0f), A(Ink, 0.35f), A(Ink, 0.0f));
		// Shelf, slots, tags.
		for (int32 S = 0; S < Count; ++S)
		{
			const FVector2f Sp = SlotPos(S);
			const bool bNear = Held >= 0 && SlotItem[S] < 0 && FVector2f::Distance(Items[Held].Pos, Sp) < 50.0f;
			P.RoundRect(Sp - FVector2f(38.0f, 36.0f), FVector2f(76.0f, 62.0f), A(Ink, bNear ? 0.3f : 0.16f), 10.0f,
			            bNear ? Cream : A(Cream, 0.15f), bNear ? 2.5f : 1.0f);
		}
		P.Rect(FVector2f(30.0f, ShelfY), FVector2f(580.0f, 10.0f), WoodLight);
		P.Rect(FVector2f(30.0f, ShelfY + 10.0f), FVector2f(580.0f, 8.0f), WoodDark);
		P.Tri(FVector2f(50.0f, ShelfY + 18.0f), FVector2f(74.0f, ShelfY + 18.0f), FVector2f(50.0f, ShelfY + 44.0f), WoodDark);
		P.Tri(FVector2f(590.0f, ShelfY + 18.0f), FVector2f(566.0f, ShelfY + 18.0f), FVector2f(590.0f, ShelfY + 44.0f), WoodDark);
		for (int32 S = 0; S < Count; ++S)
		{
			const FVector2f T(SlotPos(S).X, ShelfY + 48.0f);
			P.Segment(FVector2f(T.X - 10.0f, ShelfY + 16.0f), FVector2f(T.X, T.Y - 18.0f), A(Cream, 0.8f), 1.5f);
			P.Segment(FVector2f(T.X + 10.0f, ShelfY + 16.0f), FVector2f(T.X, T.Y - 18.0f), A(Cream, 0.8f), 1.5f);
			const bool bDone = SlotItem[S] >= 0;
			P.RoundRect(T - FVector2f(32.0f, 19.0f), FVector2f(64.0f, 40.0f), bDone ? Mix(Paper, Good, 0.35f) : Paper, 6.0f, A(Ink, 0.45f), 1.5f);
			P.Circle(T + FVector2f(0.0f, -13.0f), 2.5f, A(Ink, 0.6f));
			if (Stage == 0)
			{
				KGMgCraft::Ware(P, SlotKind[S], T + FVector2f(0.0f, 4.0f), 0.58f);
			}
			else
			{
				KGMgCraft::Coin(P, T + FVector2f(-15.0f, 3.0f), 9.0f);
				P.Text(FVector2f(T.X + 9.0f, T.Y - 9.0f), FString::Printf(TEXT("%d"), SlotPrice[S]), 18.0f, Ink, 0.5f, TEXT("Black"));
			}
		}
		// Counter + crate back.
		KGMgCraft::Planks(P, FVector2f(0, 290.0f), FVector2f(W, 110.0f), 36.0f, C(0xB07A4C), C(0x6E4A2E), false);
		P.Rect(FVector2f(0, 290.0f), FVector2f(W, 4.0f), C(0xD9A876));
		P.RoundRect(FVector2f(104.0f, 302.0f), FVector2f(432.0f, 50.0f), C(0x4A2F1C), 4.0f);
	}

	void PaintCrateFront(FKGMgPainter& P) const
	{
		P.RectV(FVector2f(98.0f, 344.0f), FVector2f(444.0f, 50.0f), C(0xC99A62), C(0x9C6B3F));
		P.Rect(FVector2f(98.0f, 344.0f), FVector2f(444.0f, 4.0f), C(0xE0B684));
		P.Rect(FVector2f(98.0f, 368.0f), FVector2f(444.0f, 2.0f), A(Ink, 0.3f));
		P.Rect(FVector2f(98.0f, 344.0f), FVector2f(14.0f, 50.0f), C(0x8A5A3B));
		P.Rect(FVector2f(528.0f, 344.0f), FVector2f(14.0f, 50.0f), C(0x8A5A3B));
		P.Text(FVector2f(320.0f, 373.0f), TEXT("MORROWMERE GOODS"), 12.0f, A(C(0x5C3A26), 0.8f), 0.5f, TEXT("Black"), 150);
	}

	void PaintItem(FKGMgPainter& P, int32 I, float Scale) const
	{
		const FItem& It = Items[I];
		float S = Scale;
		FVector2f At = It.Pos;
		if (It.Hop >= 0.0f)
		{
			const float K = Saturate(It.Hop / 0.4f);
			S *= 1.0f + 0.18f * FMath::Sin(K * PI);
			At.Y -= 10.0f * FMath::Sin(K * PI);
		}
		At.X += 8.0f * It.Shake * FMath::Sin(Time * 45.0f);
		KGMgCraft::Ware(P, It.Kind, At, S);
		if (Stage == 1)
		{
			const FVector2f St = At + FVector2f(18.0f, -16.0f) * S;
			P.Circle(St, 12.0f, Paper, A(Ink, 0.55f), 1.5f);
			P.Text(FVector2f(St.X, St.Y - 8.0f), FString::Printf(TEXT("%d"), It.Price), 12.0f, Ink, 0.5f, TEXT("Black"));
		}
		if (It.Hop >= 0.0f)
		{
			KGMgCraft::Sparkle(P, At + FVector2f(24.0f, -20.0f), 8.0f * (1.0f - Saturate(It.Hop / 0.6f)), FLinearColor::White);
		}
	}

	void PaintHud(FKGMgPainter& P) const
	{
		P.Tag(FVector2f(320.0f, 110.0f), FString::Printf(TEXT("STOCKED %d / %d"), Placed, Count), A(Ink, 0.7f), Cream, 14.0f);
		if (Placed == 0 && Held < 0 && Items.Num() > 0)
		{
			const int32 T = TargetSlot(0);
			P.HintRing(Items[0].Pos, 30.0f, Time, Gold);
			if (T >= 0)
			{
				P.HintArrow(Items[0].Pos + FVector2f(0.0f, -30.0f), SlotPos(T) + FVector2f(0.0f, 30.0f), Time, Gold);
			}
		}
	}
};

// =====================================================================================================================
// FeedAnimals: hold on the sack to fill the scoop, carry it gently to the trough and let go to pour.
// =====================================================================================================================
class FKGMgFeedAnimals final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Hold on the sack to scoop, drag gently to the trough and let go to pour - too fast spills");
	}

	virtual void BeginStage() override
	{
		Bits.Reset();
		Hearts.Reset();
		State = EScoop::Empty;
		StateT = 0.0f;
		ScoopPos = Mouse;
		LastMouse = Mouse;
		ScoopAmt = 0.0f;
		PourAmt = 0.0f;
		Trough = 0.0f;
		SpeedS = 0.0f;
		ScoopTilt = 0.0f;
		bWarned = false;
		Scoops = 0;
		for (float& Hp : Hop)
		{
			Hp = 0.0f;
		}
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (State == EScoop::Empty && InRect(Pos, SackPos - FVector2f(14.0f, 30.0f), SackSize + FVector2f(28.0f, 40.0f)))
		{
			State = EScoop::Filling;
			StateT = 0.0f;
			Sound(TEXT("S_Chore_Scrape"), 0.7f, 0.9f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (State == EScoop::Filling)
		{
			State = EScoop::Empty;
			ScoopAmt = 0.0f;
			PopText(SackMouth() + FVector2f(0.0f, -50.0f), TEXT("Hold to fill the scoop"), Cream);
		}
		else if (State == EScoop::Carrying)
		{
			if (OverTrough(ScoopPos))
			{
				State = EScoop::Pouring;
				StateT = 0.0f;
				PourAmt = ScoopAmt;
				PourAt = FVector2f(FMath::Clamp(ScoopPos.X, 400.0f, 570.0f), 262.0f);
				Sound(TEXT("S_Chore_Pour"), 0.8f, 1.0f);
			}
			else
			{
				SpillGrain(FMath::RoundToInt(ScoopAmt * 30.0f), 60.0f);
				ScoopAmt = 0.0f;
				State = EScoop::Empty;
				Oops(ScoopPos + FVector2f(0.0f, -46.0f), TEXT("Dropped it! Pour over the trough"), TEXT("S_UI_Bad"), 4.0f);
			}
		}
	}

	virtual void Tick(float Dt) override
	{
		const float FrameDist = FVector2f::Distance(Mouse, LastMouse);
		const float PrevX = LastMouse.X;
		LastMouse = Mouse;
		const float Inst = Dt > 0.0f ? FrameDist / Dt : 0.0f;
		SpeedS = Approach(SpeedS, Inst, Dt, 12.0f);
		const float SwayTarget = Dt > 0.0f && State == EScoop::Carrying ? FMath::Clamp((Mouse.X - PrevX) / Dt * 0.0006f, -0.35f, 0.35f) : 0.0f;
		ScoopTilt = Approach(ScoopTilt, SwayTarget, Dt, 10.0f);
		switch (State)
		{
		case EScoop::Empty:
			ScoopPos = KGMgCraft::ApproachV(ScoopPos, Mouse, Dt, 30.0f);
			break;
		case EScoop::Filling:
		{
			StateT += Dt;
			const float K = Saturate(StateT / FillTime);
			ScoopPos = KGMgCraft::ApproachV(ScoopPos, SackMouth() + FVector2f(10.0f, 14.0f - 18.0f * FMath::Sin(K * PI)), Dt, 20.0f);
			ScoopAmt = K;
			if (StateT >= FillTime)
			{
				State = EScoop::Carrying;
				ScoopAmt = 1.0f;
				bWarned = false;
				Sound(TEXT("S_Chore_Scrape"), 0.5f, 1.25f);
			}
			break;
		}
		case EScoop::Carrying:
		{
			ScoopPos = Mouse;
			const float Excess = Saturate((SpeedS - SafeSpeed) / 600.0f);
			if (Excess > 0.0f && !bSolved)
			{
				const float Lost = FMath::Min(ScoopAmt, FrameDist * 0.0035f * Excess);
				ScoopAmt -= Lost;
				SpillGrain(FMath::Max(1, FMath::RoundToInt(Lost * 60.0f)), 30.0f);
				if (!bWarned && Lost > 0.004f)
				{
					bWarned = true;
					Oops(ScoopPos + FVector2f(0.0f, -46.0f), TEXT("Too fast - spilling!"), TEXT("S_UI_Bad"), 2.0f);
				}
				if (ScoopAmt <= 0.03f)
				{
					ScoopAmt = 0.0f;
					State = EScoop::Empty;
					PopText(ScoopPos + FVector2f(0.0f, -70.0f), TEXT("Empty - back to the sack"), Cream);
				}
			}
			break;
		}
		case EScoop::Pouring:
		{
			const float K0 = Saturate(StateT / PourTime);
			StateT += Dt;
			const float K1 = Saturate(StateT / PourTime);
			ScoopPos = KGMgCraft::ApproachV(ScoopPos, PourAt, Dt, 16.0f);
			const float D = PourAmt * (K1 - K0);
			ScoopAmt = FMath::Max(0.0f, ScoopAmt - D);
			Trough = FMath::Min(1.0f, Trough + D * PerScoop);
			if (Bits.Num() < 300)
			{
				KGMgCraft::FBit& B = Bits.AddDefaulted_GetRef();
				B.P = ScoopPos + FVector2f(-26.0f + Rng.FRand() * 10.0f, 8.0f);
				B.V = FVector2f(-30.0f + Rng.FRand() * 40.0f, 20.0f);
				B.Life = 0.3f;
				B.Size = 2.6f;
				B.Col = Mix(C(0xF2D27A), C(0xD9A83A), Rng.FRand());
			}
			if (Rng.FRand() < Dt * 4.0f)
			{
				SpawnHeart();
			}
			if (StateT >= PourTime)
			{
				++Scoops;
				State = EScoop::Empty;
				ScoopAmt = 0.0f;
				for (float& Hp : Hop)
				{
					Hp = 1.0f;
				}
				const bool bFull = Trough >= 0.999f;
				Nice(PourAt + FVector2f(0.0f, -70.0f), bFull ? TEXT("All fed!") : TEXT("Yum!"), TEXT("S_Chore_Pour"), 0.9f + 0.08f * float(Scoops));
				Sound(TEXT("S_UI_Good"), 0.6f, 1.0f + 0.06f * float(Scoops));
				if (bFull && !bSolved)
				{
					Solve();
				}
			}
			break;
		}
		}
		for (float& Hp : Hop)
		{
			Hp = FMath::Max(0.0f, Hp - Dt * 2.2f);
		}
		for (int32 i = Hearts.Num() - 1; i >= 0; --i)
		{
			Hearts[i].Age += Dt;
			Hearts[i].P.Y -= 36.0f * Dt;
			if (Hearts[i].Age > 1.3f)
			{
				Hearts.RemoveAt(i);
			}
		}
		KGMgCraft::TickBits(Bits, Dt, 900.0f, 376.0f);
	}

	virtual void AutoPlay(float Dt) override
	{
		auto MoveTo = [this, Dt](const FVector2f& To, float Spd)
		{
			const FVector2f D = To - Mouse;
			const float Len = D.Size();
			const float Step = Spd * Dt;
			Mouse = Len > Step ? Mouse + D / Len * Step : To;
			return Len <= Step;
		};
		if (State == EScoop::Empty)
		{
			if (MoveTo(SackMouth() + FVector2f(0.0f, 40.0f), 600.0f))
			{
				OnPress(Mouse);
			}
		}
		else if (State == EScoop::Carrying)
		{
			if (MoveTo(FVector2f(485.0f, 262.0f), 300.0f))
			{
				OnRelease(Mouse);
			}
		}
	}

	virtual void DebugPose() override
	{
		Trough = 0.4f;
		Scoops = 2;
		State = EScoop::Carrying;
		ScoopAmt = 0.85f;
		Mouse = FVector2f(300.0f, 226.0f);
		ScoopPos = Mouse;
		LastMouse = Mouse;
		SpeedS = 260.0f;
		for (int32 i = 0; i < 3; ++i)
		{
			SpawnHeart();
		}
		for (float& Hp : Hop)
		{
			Hp = 0.4f;
		}
		SpillGrain(8, 30.0f);
		KGMgCraft::TickBits(Bits, 0.12f, 900.0f, 376.0f);
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintFarm(P);
		PaintSack(P);
		// Animals behind the trough.
		const bool bHappy = Trough > 0.05f;
		const float Peck0 = bHappy ? Saturate(FMath::Sin(Time * 6.0f) * 1.4f) : 0.0f;
		const float Peck1 = bHappy ? Saturate(FMath::Sin(Time * 6.0f + 2.0f) * 1.4f) : 0.0f;
		Chicken(P, FVector2f(380.0f, 300.0f), Hop[0] * 12.0f, Peck0);
		Pig(P, FVector2f(450.0f, 314.0f), Hop[1] * 14.0f, Hop[1] > 0.1f || Trough > 0.6f, bHappy ? FMath::Sin(Time * 10.0f) : 0.0f);
		Pig(P, FVector2f(548.0f, 314.0f), Hop[2] * 14.0f, Hop[2] > 0.1f || Trough > 0.6f, bHappy ? FMath::Sin(Time * 10.0f + 1.0f) : 0.0f);
		Chicken(P, FVector2f(606.0f, 300.0f), Hop[3] * 12.0f, Peck1);
		PaintTrough(P);
		KGMgCraft::PaintBits(P, Bits);
		for (const FHeart& Ht : Hearts)
		{
			const float K = Saturate(Ht.Age / 1.3f);
			KGMgCraft::Heart(P, Ht.P, 9.0f * (0.6f + 0.4f * EaseOutBack(Saturate(Ht.Age / 0.3f))), A(C(0xF2506A), 1.0f - K * K));
		}
		PaintScoop(P);
		PaintHud(P);
	}

private:
	enum class EScoop : uint8
	{
		Empty,
		Filling,
		Carrying,
		Pouring
	};

	struct FHeart
	{
		FVector2f P;
		float Age;
	};

	static constexpr float FillTime = 0.45f;
	static constexpr float PourTime = 0.6f;
	static constexpr float PerScoop = 1.0f / 6.0f;   // 6 full scoops; >= ~9 s even when rushing (floor 6.0)
	static constexpr float SafeSpeed = 480.0f;
	const FVector2f SackPos = FVector2f(30.0f, 214.0f);
	const FVector2f SackSize = FVector2f(140.0f, 156.0f);

	TArray<KGMgCraft::FBit> Bits;
	TArray<FHeart> Hearts;
	EScoop State = EScoop::Empty;
	float StateT = 0.0f;
	FVector2f ScoopPos = FVector2f::ZeroVector;
	FVector2f LastMouse = FVector2f::ZeroVector;
	FVector2f PourAt = FVector2f::ZeroVector;
	float ScoopAmt = 0.0f;
	float PourAmt = 0.0f;
	float Trough = 0.0f;
	float SpeedS = 0.0f;
	float ScoopTilt = 0.0f;
	bool bWarned = false;
	int32 Scoops = 0;
	float Hop[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	FVector2f SackMouth() const { return FVector2f(100.0f, 212.0f); }
	static bool OverTrough(const FVector2f& Q) { return InRect(Q, FVector2f(340.0f, 190.0f), FVector2f(290.0f, 180.0f)); }

	void SpillGrain(int32 N, float Spread)
	{
		for (int32 i = 0; i < N && Bits.Num() < 300; ++i)
		{
			KGMgCraft::FBit& B = Bits.AddDefaulted_GetRef();
			B.P = ScoopPos + FVector2f(-20.0f + Rng.FRand() * 40.0f, 4.0f);
			B.V = FVector2f((Rng.FRand() - 0.5f) * Spread * 2.0f, -20.0f - Rng.FRand() * 40.0f);
			B.Life = 1.2f + Rng.FRand() * 0.6f;
			B.Size = 2.4f + Rng.FRand() * 1.2f;
			B.Col = Mix(C(0xF2D27A), C(0xD9A83A), Rng.FRand());
		}
	}

	void SpawnHeart()
	{
		const float Xs[] = {380.0f, 450.0f, 548.0f, 606.0f};
		const int32 I = Rng.RandRange(0, 3);
		Hearts.Add({FVector2f(Xs[I] - 10.0f + Rng.FRand() * 20.0f, 232.0f), 0.0f});
	}

	void PaintFarm(FKGMgPainter& P) const
	{
		KGMgCraft::Outdoor(P, 238.0f, C(0x86CDEF), C(0xF1EED8));
		// Barn.
		P.Rect(FVector2f(430.0f, 120.0f), FVector2f(190.0f, 118.0f), C(0xB5483A));
		P.Tri(FVector2f(418.0f, 122.0f), FVector2f(632.0f, 122.0f), FVector2f(525.0f, 60.0f), C(0x7A2A22));
		P.Rect(FVector2f(492.0f, 160.0f), FVector2f(66.0f, 78.0f), C(0x8C3028));
		P.Bar(FVector2f(492.0f, 160.0f), FVector2f(558.0f, 238.0f), 4.0f, Cream);
		P.Bar(FVector2f(558.0f, 160.0f), FVector2f(492.0f, 238.0f), 4.0f, Cream);
		P.RoundRect(FVector2f(492.0f, 160.0f), FVector2f(66.0f, 78.0f), FLinearColor::Transparent, 0.0f, Cream, 4.0f);
		// Fence.
		for (int32 i = 0; i < 12; ++i)
		{
			const float X = 10.0f + float(i) * 56.0f;
			P.Rect(FVector2f(X, 196.0f), FVector2f(8.0f, 46.0f), C(0xC9A26B));
		}
		P.Rect(FVector2f(0.0f, 206.0f), FVector2f(W, 6.0f), C(0xB98E58));
		P.Rect(FVector2f(0.0f, 224.0f), FVector2f(W, 6.0f), C(0xB98E58));
		// Grass and a muddy patch around the trough.
		P.RectV(FVector2f(0, 238.0f), FVector2f(W, 162.0f), C(0x8CC663), C(0x5E9A40));
		P.Disc(FVector2f(488.0f, 346.0f), FVector2f(200.0f, 50.0f), A(C(0x9C7650), 0.7f), A(C(0x9C7650), 0.0f), 32);
		for (int32 i = 0; i < 16; ++i)
		{
			const float X = 14.0f + float((i * 83) % 610);
			const float Y = 250.0f + float((i * 47) % 140);
			P.Tri(FVector2f(X - 5.0f, Y), FVector2f(X + 5.0f, Y), FVector2f(X - 2.0f, Y - 10.0f), C(0x4E8A34));
			P.Tri(FVector2f(X, Y), FVector2f(X + 8.0f, Y), FVector2f(X + 6.0f, Y - 8.0f), C(0x5E9A40));
		}
	}

	void PaintSack(FKGMgPainter& P) const
	{
		P.Disc(FVector2f(100.0f, 370.0f), FVector2f(80.0f, 10.0f), A(Ink, 0.3f), A(Ink, 0.0f), 16);
		P.RoundRect(SackPos + FVector2f(0.0f, 10.0f), SackSize - FVector2f(0.0f, 10.0f), C(0xC9A26B), 34.0f, C(0x9C7A48), 3.0f);
		P.RectV(SackPos + FVector2f(10.0f, 30.0f), FVector2f(30.0f, 110.0f), A(FLinearColor::White, 0.14f), A(FLinearColor::White, 0.0f));
		for (int32 i = 0; i < 5; ++i)
		{
			const float Y = SackPos.Y + 44.0f + float(i) * 24.0f;
			P.Segment(FVector2f(SackPos.X + 12.0f, Y), FVector2f(SackPos.X + SackSize.X - 12.0f, Y + 3.0f), A(C(0x8A6A3A), 0.35f), 1.5f);
		}
		P.RoundRect(SackPos + FVector2f(34.0f, 82.0f), FVector2f(72.0f, 34.0f), C(0xF3E6C4), 6.0f, C(0x9C7A48), 2.0f);
		P.Text(SackPos + FVector2f(70.0f, 89.0f), TEXT("FEED"), 14.0f, C(0x7A4A2A), 0.5f, TEXT("Black"), 100);
		// Rolled rim and the grain inside.
		P.RoundRect(SackPos + FVector2f(-6.0f, -6.0f), FVector2f(SackSize.X + 12.0f, 26.0f), C(0xB08850), 12.0f, C(0x8A6A3A), 2.0f);
		P.Disc(SackMouth(), FVector2f(62.0f, 10.0f), C(0xF6DC8A), C(0xD9B050), 20);
		for (int32 i = 0; i < 10; ++i)
		{
			P.Circle(SackMouth() + FVector2f(-48.0f + float(i) * 10.5f, float((i * 7) % 5) - 2.0f), 2.0f, C(0xC9962A));
		}
		if (State == EScoop::Empty && Scoops == 0)
		{
			P.HintRing(SackMouth() + FVector2f(0.0f, 30.0f), 36.0f, Time, Gold);
			P.Tag(SackMouth() + FVector2f(0.0f, -36.0f), TEXT("HOLD HERE"), A(Ink, 0.75f), Gold, 13.0f);
		}
	}

	static void Pig(FKGMgPainter& P, const FVector2f& Base, float Lift, bool bHappy, float Chew)
	{
		const FVector2f B = Base + FVector2f(0.0f, -Lift);
		const FLinearColor Skin = C(0xF7B5C0);
		const FLinearColor SkinDark = C(0xE58A9C);
		P.Arc(B + FVector2f(44.0f, -36.0f), 6.0f, -PI, PI * 0.6f, SkinDark, 3.0f, 12);
		P.Disc(B + FVector2f(8.0f, -28.0f), FVector2f(40.0f, 27.0f), Skin, SkinDark, 32);
		P.Tri(B + FVector2f(-44.0f, -54.0f), B + FVector2f(-28.0f, -52.0f), B + FVector2f(-40.0f, -70.0f), SkinDark);
		P.Tri(B + FVector2f(-22.0f, -54.0f), B + FVector2f(-8.0f, -50.0f), B + FVector2f(-12.0f, -70.0f), SkinDark);
		P.Circle(B + FVector2f(-26.0f, -40.0f), 21.0f, Skin);
		P.Circle(B + FVector2f(-36.0f, -32.0f), 5.0f, A(C(0xFF8FA0), 0.6f));
		const FVector2f Snout = B + FVector2f(-46.0f, -35.0f + Chew * 2.0f);
		P.Disc(Snout, FVector2f(9.5f, 8.0f), C(0xF59AAC), SkinDark, 16);
		P.Circle(Snout + FVector2f(-3.0f, 0.0f), 1.9f, C(0x8A3A4A));
		P.Circle(Snout + FVector2f(3.0f, 0.0f), 1.9f, C(0x8A3A4A));
		if (bHappy)
		{
			P.Arc(B + FVector2f(-32.0f, -44.0f), 4.0f, PI, UE_TWO_PI, Ink, 2.0f, 8);
			P.Arc(B + FVector2f(-17.0f, -45.0f), 4.0f, PI, UE_TWO_PI, Ink, 2.0f, 8);
		}
		else
		{
			P.Circle(B + FVector2f(-32.0f, -46.0f), 3.0f, Ink);
			P.Circle(B + FVector2f(-17.0f, -47.0f), 3.0f, Ink);
			P.Circle(B + FVector2f(-31.0f, -47.0f), 1.0f, FLinearColor::White);
			P.Circle(B + FVector2f(-16.0f, -48.0f), 1.0f, FLinearColor::White);
		}
	}

	static void Chicken(FKGMgPainter& P, const FVector2f& Base, float Lift, float Peck)
	{
		const FVector2f B = Base + FVector2f(0.0f, -Lift);
		P.Tri(B + FVector2f(14.0f, -28.0f), B + FVector2f(32.0f, -54.0f), B + FVector2f(24.0f, -22.0f), C(0xE8E0D0));
		P.Tri(B + FVector2f(12.0f, -30.0f), B + FVector2f(24.0f, -56.0f), B + FVector2f(20.0f, -24.0f), C(0xF4EFE4));
		P.Disc(B + FVector2f(2.0f, -24.0f), FVector2f(23.0f, 18.0f), FLinearColor::White, C(0xE6DDCB), 24);
		P.Disc(B + FVector2f(7.0f, -24.0f), FVector2f(12.0f, 7.5f), C(0xF1EADB), C(0xD9CDB5), 16);
		const FVector2f Hd = B + FVector2f(-17.0f, -42.0f + Peck * 12.0f);
		P.Circle(Hd + FVector2f(-3.0f, -10.0f), 4.0f, Crimson);
		P.Circle(Hd + FVector2f(2.0f, -11.0f), 4.5f, Crimson);
		P.Circle(Hd + FVector2f(6.0f, -9.0f), 3.5f, Crimson);
		P.Circle(Hd, 10.5f, FLinearColor::White);
		P.Tri(Hd + FVector2f(-9.0f, -3.0f), Hd + FVector2f(-9.0f, 4.0f), Hd + FVector2f(-18.0f, 1.0f), C(0xF2A93B));
		P.Circle(Hd + FVector2f(-7.0f, 8.0f), 3.2f, Crimson);
		P.Circle(Hd + FVector2f(-3.0f, -2.0f), 2.2f, Ink);
	}

	void PaintTrough(FKGMgPainter& P) const
	{
		// Legs.
		P.Bar(FVector2f(372.0f, 350.0f), FVector2f(362.0f, 378.0f), 8.0f, WoodDark);
		P.Bar(FVector2f(598.0f, 350.0f), FVector2f(608.0f, 378.0f), 8.0f, WoodDark);
		// Back rim, feed heap, front.
		P.Rect(FVector2f(352.0f, 294.0f), FVector2f(266.0f, 10.0f), WoodDark);
		if (Trough > 0.01f)
		{
			P.Poly(KGMgCraft::EllipsePts(FVector2f(485.0f, 306.0f), FVector2f(126.0f, 3.0f + 18.0f * Trough), 18, PI, UE_TWO_PI), C(0xF0CC6A));
			for (int32 i = 0; i < 14; ++i)
			{
				const float X = 374.0f + float(i) * 16.0f;
				const float Top = 306.0f - (3.0f + 18.0f * Trough) * FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square((X - 485.0f) / 126.0f)));
				P.Circle(FVector2f(X, FMath::Lerp(Top, 304.0f, 0.4f + 0.1f * float(i % 3))), 2.0f, C(0xC9962A));
			}
		}
		P.Quad(FVector2f(346.0f, 302.0f), FVector2f(624.0f, 302.0f), FVector2f(608.0f, 356.0f), FVector2f(362.0f, 356.0f), Wood);
		P.Rect(FVector2f(346.0f, 302.0f), FVector2f(278.0f, 5.0f), WoodLight);
		P.Segment(FVector2f(352.0f, 322.0f), FVector2f(618.0f, 322.0f), A(Ink, 0.35f), 2.0f);
		P.Segment(FVector2f(357.0f, 340.0f), FVector2f(613.0f, 340.0f), A(Ink, 0.35f), 2.0f);
		for (int32 k = 0; k < 4; ++k)
		{
			P.Circle(FVector2f(372.0f + float(k) * 76.0f, 312.0f), 2.5f, Iron);
		}
	}

	void PaintScoop(FKGMgPainter& P) const
	{
		float Ang = 0.0f;
		if (State == EScoop::Pouring)
		{
			Ang = -1.1f * EaseOutCubic(Saturate(StateT / 0.2f));
		}
		else if (State == EScoop::Carrying)
		{
			Ang = ScoopTilt;
		}
		const FVector2f O = ScoopPos;
		auto Pt = [&O, Ang](float X, float Y) { return O + KGMgCraft::Rot(FVector2f(X, Y), Ang); };
		const float Danger = State == EScoop::Carrying ? Saturate((SpeedS - SafeSpeed * 0.7f) / (SafeSpeed * 0.5f)) : 0.0f;
		P.Bar(Pt(26.0f, -4.0f), Pt(66.0f, -28.0f), 10.0f, C(0x9C6B3F));
		P.Bar(Pt(26.0f, -6.0f), Pt(64.0f, -29.0f), 3.0f, A(C(0xD9A56A), 0.7f));
		P.Bar(Pt(20.0f, -2.0f), Pt(32.0f, -9.0f), 8.0f, Iron);
		TArray<FVector2f> Bowl;
		for (int32 i = 0; i <= 12; ++i)
		{
			const float T = PI * float(i) / 12.0f;
			Bowl.Add(Pt(FMath::Cos(T) * 30.0f, FMath::Sin(T) * 21.0f));
		}
		P.Poly(Bowl, Mix(C(0xB8C0C8), Crimson, Danger * 0.5f));
		TArray<FVector2f> Shade;
		for (int32 i = 0; i <= 8; ++i)
		{
			const float T = PI * 0.5f + PI * 0.5f * float(i) / 8.0f;
			Shade.Add(Pt(FMath::Cos(T) * 28.0f, FMath::Sin(T) * 19.0f));
		}
		Shade.Add(Pt(0.0f, 0.0f));
		P.Poly(Shade, A(Ink, 0.15f));
		if (ScoopAmt > 0.01f)
		{
			TArray<FVector2f> Heap;
			for (int32 i = 0; i <= 12; ++i)
			{
				const float T = PI + PI * float(i) / 12.0f;
				Heap.Add(Pt(FMath::Cos(T) * 28.0f, FMath::Sin(T) * (3.0f + 15.0f * ScoopAmt)));
			}
			P.Poly(Heap, C(0xF0CC6A));
			for (int32 i = 0; i < 6; ++i)
			{
				P.Circle(Pt(-18.0f + float(i) * 7.0f, -2.0f - (3.0f + 10.0f * ScoopAmt) * 0.5f * float(1 + i % 2)), 1.8f, C(0xC9962A));
			}
		}
		P.Bar(Pt(-31.0f, 0.0f), Pt(31.0f, 0.0f), 4.0f, Danger > 0.3f ? Mix(Cream, Crimson, Danger) : C(0xDDE3EA));
		if (State == EScoop::Carrying && Scoops == 0)
		{
			P.HintArrow(O + FVector2f(40.0f, 10.0f), FVector2f(470.0f, 260.0f), Time, Gold);
		}
	}

	void PaintHud(FKGMgPainter& P) const
	{
		KGMgCraft::HudPanel(P, FVector2f(196.0f, 12.0f), FVector2f(196.0f, 52.0f));
		KGMgCraft::Caption(P, FVector2f(208.0f, 19.0f), TEXT("STEADY HAND"), CreamDim);
		const float Fill = Saturate(SpeedS / (SafeSpeed * 1.6f));
		P.Gauge(FVector2f(206.0f, 38.0f), FVector2f(176.0f, 16.0f), State == EScoop::Carrying ? Fill : 0.0f,
		        SpeedS > SafeSpeed ? Crimson : Good, 0.0f, 1.0f / 1.6f);
		KGMgCraft::HudPanel(P, FVector2f(400.0f, 12.0f), FVector2f(226.0f, 52.0f));
		KGMgCraft::Caption(P, FVector2f(412.0f, 19.0f), TEXT("TROUGH"), CreamDim);
		P.Text(FVector2f(614.0f, 16.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Trough * 100.0f)), 13.0f, Cream, 1.0f, TEXT("Black"));
		KGMgCraft::Pips(P, FVector2f(422.0f, 46.0f), 6, Trough * 6.0f, 9.0f, C(0xF0CC6A));
	}
};

} // namespace KGMg


TUniquePtr<FKGMinigame> KGMakeMinigame_ForgeNails() { return MakeUnique<KGMg::FKGMgForgeNails>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_SharpenTools() { return MakeUnique<KGMg::FKGMgSharpenTools>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_BakeBread() { return MakeUnique<KGMg::FKGMgBakeBread>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_PourAle() { return MakeUnique<KGMg::FKGMgPourAle>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_StockStall() { return MakeUnique<KGMg::FKGMgStockStall>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_FeedAnimals() { return MakeUnique<KGMg::FKGMgFeedAnimals>(); }
