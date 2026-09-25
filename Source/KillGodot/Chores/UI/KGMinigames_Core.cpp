// Chore minigames: DrawWater, RingBell, ChopWood, MendNets (the reference set - see KGMinigame.h for the contract).
#include "Chores/UI/KGMinigame.h"

namespace KGMg
{
namespace KGMgCore
{
	/** Keeps the part of a polygon below the horizontal line y = SurfaceY (screen y grows downwards). */
	TArray<FVector2f> ClipBelow(const TArray<FVector2f>& Poly, float SurfaceY)
	{
		TArray<FVector2f> Out;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2f& A = Poly[i];
			const FVector2f& B = Poly[(i + 1) % Poly.Num()];
			const bool bAIn = A.Y >= SurfaceY;
			const bool bBIn = B.Y >= SurfaceY;
			if (bAIn)
			{
				Out.Add(A);
			}
			if (bAIn != bBIn)
			{
				const float T = (SurfaceY - A.Y) / (B.Y - A.Y);
				Out.Add(A + (B - A) * T);
			}
		}
		return Out;
	}

	FVector2f Rot(const FVector2f& P, float Ang)
	{
		const float C = FMath::Cos(Ang);
		const float S = FMath::Sin(Ang);
		return FVector2f(P.X * C - P.Y * S, P.X * S + P.Y * C);
	}

	float WrapPi(float A)
	{
		while (A > PI)
		{
			A -= 2.0f * PI;
		}
		while (A < -PI)
		{
			A += 2.0f * PI;
		}
		return A;
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
			const float X = W * i / 16.0f;
			Hills.Add(FVector2f(X, GroundY - 34.0f - 18.0f * FMath::Sin(i * 0.9f) - 10.0f * FMath::Sin(i * 2.3f)));
		}
		for (int32 i = 0; i + 1 < Hills.Num(); ++i)
		{
			P.Quad(Hills[i], Hills[i + 1], FVector2f(Hills[i + 1].X, GroundY), FVector2f(Hills[i].X, GroundY), C(0x8CC66A));
		}
	}

	/** A shape marker used for colour-blind friendly matching (0 circle, 1 square, 2 triangle, 3 diamond, 4 bar, 5 cross). */
	void Symbol(FKGMgPainter& P, int32 Kind, const FVector2f& C, float R, const FLinearColor& Color)
	{
		switch (Kind % 6)
		{
		case 0: P.Circle(C, R, Color); break;
		case 1: P.Rect(C - FVector2f(R, R) * 0.85f, FVector2f(R, R) * 1.7f, Color); break;
		case 2: P.Tri(C + FVector2f(0, -R), C + FVector2f(R, R * 0.8f), C + FVector2f(-R, R * 0.8f), Color); break;
		case 3: P.Quad(C + FVector2f(0, -R), C + FVector2f(R, 0), C + FVector2f(0, R), C + FVector2f(-R, 0), Color); break;
		case 4: P.Rect(C - FVector2f(R, R * 0.4f), FVector2f(R * 2.0f, R * 0.8f), Color); break;
		default:
			P.Rect(C - FVector2f(R, R * 0.32f), FVector2f(R * 2.0f, R * 0.64f), Color);
			P.Rect(C - FVector2f(R * 0.32f, R), FVector2f(R * 0.64f, R * 2.0f), Color);
			break;
		}
	}
}

// =====================================================================================================================
// DrawWater: crank the windlass by circling the handle, then carry the bucket to the trough without spilling.
// =====================================================================================================================
class FKGMgDrawWater final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Grab the crank and circle it clockwise - not too fast or the rope slips")
		                  : TEXT("Carry it to the trough: move the mouse to keep the bucket level");
	}

	virtual void BeginStage() override
	{
		Wound = 0.0f;
		Angle = -0.5f * PI;
		Speed = 0.0f;
		FrameTurn = 0.0f;
		SlipT = 0.0f;
		bGrab = false;
		ClickAcc = 0.0f;
		Walk = 0.0f;
		Carried = 1.0f;
		Tilt = 0.0f;
		Drops.Reset();
		for (float& Ph : Phase)
		{
			Ph = Rng.FRand() * 6.28f;
		}
		JoltT = 0.6f;
		Jolt = 0.0f;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0 && FVector2f::Distance(Pos, Hub) < CrankR + 50.0f)
		{
			bGrab = true;
			PrevAng = FMath::Atan2(Pos.Y - Hub.Y, Pos.X - Hub.X);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { bGrab = false; }

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage != 0 || !bGrab || FVector2f::Distance(Pos, Hub) < 18.0f)
		{
			return;
		}
		const float Ang = FMath::Atan2(Pos.Y - Hub.Y, Pos.X - Hub.X);
		Turn(KGMgCore::WrapPi(Ang - PrevAng));
		PrevAng = Ang;
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickCrank(Dt);
		}
		else
		{
			TickCarry(Dt);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			bGrab = true;
			Turn(1.0f * 2.0f * PI * Dt);
			Mouse = Hub + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * CrankR;
		}
		else
		{
			Mouse.X = 320.0f + (Drift() + Jolt) / -0.2f * 0.9f;
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			Wound = 0.46f;
			Angle = 0.35f;
			Speed = 0.8f;
			bGrab = true;
			Mouse = Hub + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * CrankR;
		}
		else
		{
			Walk = 0.56f;
			Carried = 0.74f;
			Mouse.X = 360.0f;
			Tilt = 21.0f;
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintCrank(P);
		}
		else
		{
			PaintCarry(P);
		}
	}

private:
	static constexpr float Turns = 6.0f;
	static constexpr float SlipSpeed = 1.3f;   // turns per second
	const FVector2f Hub = FVector2f(420.0f, 96.0f);
	const float CrankR = 64.0f;

	// Crank
	float Wound = 0.0f;
	float Angle = 0.0f;
	float PrevAng = 0.0f;
	float Speed = 0.0f;
	float FrameTurn = 0.0f;
	float SlipT = 0.0f;
	float ClickAcc = 0.0f;
	bool bGrab = false;
	// Carry
	float Walk = 0.0f;
	float Carried = 1.0f;
	float Tilt = 0.0f;
	float Phase[3] = {0, 0, 0};
	float JoltT = 0.0f;
	float Jolt = 0.0f;
	struct FDrop
	{
		FVector2f P;
		FVector2f V;
		float Age;
	};
	TArray<FDrop> Drops;

	void Turn(float D)
	{
		if (SlipT > 0.0f)
		{
			return;
		}
		FrameTurn += D;
		Angle += D;
		Wound = Saturate(Wound + D / (2.0f * PI * Turns));
		ClickAcc += FMath::Abs(D);
		if (ClickAcc > PI * 0.5f)
		{
			ClickAcc = 0.0f;
			Sound(TEXT("S_Chore_Crank"), 0.55f, 0.9f + 0.3f * Wound);
		}
	}

	void TickCrank(float Dt)
	{
		const float Inst = Dt > 0.0f ? FrameTurn / (2.0f * PI * Dt) : 0.0f;
		FrameTurn = 0.0f;
		Speed = Approach(Speed, Inst, Dt, 6.0f);
		if (SlipT > 0.0f)
		{
			SlipT -= Dt;
			Wound = FMath::Max(0.0f, Wound - Dt * 0.12f);   // the bucket drops back while the rope slips
		}
		else if (Speed > SlipSpeed)
		{
			SlipT = 0.7f;
			Speed = 0.0f;
			Oops(FVector2f(220.0f, 140.0f), TEXT("Too fast - it slips!"));
		}
		if (Wound >= 1.0f && !bSolved)
		{
			Nice(FVector2f(220.0f, 150.0f), TEXT("Full bucket!"), TEXT("S_Chore_Splash"));
			Solve();
		}
	}

	float Drift() const
	{
		const float T = StageTime;
		return 26.0f * FMath::Sin(0.95f * T + Phase[0]) + 12.0f * FMath::Sin(2.3f * T + Phase[1]) + 5.0f * FMath::Sin(4.1f * T + Phase[2]);
	}

	void TickCarry(float Dt)
	{
		if (!bSolved)
		{
			Walk = Saturate(Walk + Dt / 10.0f);
			JoltT -= Dt;
			if (JoltT <= 0.0f)
			{
				JoltT = 0.5f + Rng.FRand() * 0.25f;
				Jolt += (Rng.FRand() < 0.5f ? -1.0f : 1.0f) * (5.0f + Rng.FRand() * 7.0f);   // a step on the cobbles
			}
			Jolt = Approach(Jolt, 0.0f, Dt, 3.0f);
			const float Target = Drift() + Jolt + (FMath::Clamp(Mouse.X, 0.0f, W) - 320.0f) * 0.2f;
			Tilt = Approach(Tilt, FMath::Clamp(Target, -70.0f, 70.0f), Dt, 10.0f);
			const float Over = FMath::Abs(Tilt) - 22.0f;
			if (Over > 0.0f && Carried > 0.0f)
			{
				Carried = FMath::Max(0.0f, Carried - Dt * (0.05f + Over * 0.012f));
				if (Rng.FRand() < 0.6f)
				{
					const float Side = Tilt > 0.0f ? 1.0f : -1.0f;
					const FVector2f Lip = Pivot() + KGMgCore::Rot(FVector2f(Side * 58.0f, 58.0f), FMath::DegreesToRadians(Tilt));
					Drops.Add({Lip, FVector2f(Side * (40.0f + Rng.FRand() * 60.0f), -20.0f), 0.0f});
				}
			}
			if (Carried < 0.35f)
			{
				Oops(FVector2f(320.0f, 200.0f), TEXT("Spilled it! Back to the well"));
				RetryStage();
				return;
			}
			if (Walk >= 1.0f)
			{
				Nice(FVector2f(320.0f, 160.0f), FString::Printf(TEXT("%d%% delivered"), FMath::RoundToInt(Carried * 100.0f)), TEXT("S_Chore_Splash"));
				Solve();
			}
		}
		for (int32 i = Drops.Num() - 1; i >= 0; --i)
		{
			Drops[i].Age += Dt;
			Drops[i].V.Y += 900.0f * Dt;
			Drops[i].P += Drops[i].V * Dt;
			if (Drops[i].Age > 1.0f)
			{
				Drops.RemoveAt(i);
			}
		}
	}

	FVector2f Pivot() const { return FVector2f(320.0f, 150.0f); }

	static void Bucket(FKGMgPainter& P, const FVector2f& Top, float Wd, float Ht, float Ang, float Fill)
	{
		// Wooden bucket around its handle pivot Top (rotated by Ang), water surface level in world space.
		auto Pt = [&](float X, float Y) { return Top + KGMgCore::Rot(FVector2f(X, Y), Ang); };
		const float Rim = 30.0f;
		const TArray<FVector2f> Body = {Pt(-Wd * 0.5f, Rim), Pt(Wd * 0.5f, Rim), Pt(Wd * 0.4f, Rim + Ht), Pt(-Wd * 0.4f, Rim + Ht)};
		// Handle
		TArray<FVector2f> Handle;
		for (int32 i = 0; i <= 12; ++i)
		{
			const float A0 = PI + PI * i / 12.0f;
			Handle.Add(Pt(FMath::Cos(A0) * Wd * 0.5f, Rim + FMath::Sin(A0) * (Rim - 2.0f)));
		}
		P.Line(Handle, Iron, 3.0f);
		P.Poly(Body, WoodDark);
		const TArray<FVector2f> Inner = {Pt(-Wd * 0.44f, Rim + 3.0f), Pt(Wd * 0.44f, Rim + 3.0f), Pt(Wd * 0.36f, Rim + Ht - 5.0f),
		                                 Pt(-Wd * 0.36f, Rim + Ht - 5.0f)};
		P.Poly(Inner, C(0x3B2416));
		if (Fill > 0.01f)
		{
			// Water surface: horizontal in the world, height from how full it is.
			const float Bottom = FMath::Max(Inner[2].Y, Inner[3].Y);
			const float TopY = FMath::Min(Inner[0].Y, Inner[1].Y);
			const float Surface = FMath::Lerp(Bottom, TopY - 6.0f, Fill);
			const TArray<FVector2f> Wat = KGMgCore::ClipBelow(Inner, Surface);
			if (Wat.Num() >= 3)
			{
				P.Poly(Wat, Water);
				float X0 = 1e9f, X1 = -1e9f;
				for (const FVector2f& V : Wat)
				{
					if (FMath::Abs(V.Y - Surface) < 0.5f)
					{
						X0 = FMath::Min(X0, V.X);
						X1 = FMath::Max(X1, V.X);
					}
				}
				if (X1 > X0)
				{
					P.Bar(FVector2f(X0, Surface + 1.5f), FVector2f(X1, Surface + 1.5f), 3.0f, C(0xA8E4FF));
				}
			}
		}
		// Planks + iron hoops.
		for (int32 i = -2; i <= 2; ++i)
		{
			P.Segment(Pt(i * Wd * 0.17f, Rim), Pt(i * Wd * 0.14f, Rim + Ht), A(Ink, 0.35f), 1.5f);
		}
		P.Bar(Pt(-Wd * 0.49f, Rim + Ht * 0.18f), Pt(Wd * 0.49f, Rim + Ht * 0.18f), 5.0f, Iron);
		P.Bar(Pt(-Wd * 0.42f, Rim + Ht * 0.82f), Pt(Wd * 0.42f, Rim + Ht * 0.82f), 5.0f, Iron);
	}

	void PaintCrank(FKGMgPainter& P) const
	{
		const float GroundY = 210.0f;
		KGMgCore::Outdoor(P, GroundY);
		// Soil cut-away with the shaft.
		P.RectV(FVector2f(0, GroundY), FVector2f(W, H - GroundY), C(0x7A5234), C(0x4A2F1C));
		P.Rect(FVector2f(0, GroundY), FVector2f(W, 10.0f), Grass);
		for (int32 i = 0; i < 14; ++i)
		{
			const float X = 30.0f + (i * 97) % 600;
			const float Y = GroundY + 30.0f + (i * 53) % 150;
			P.Circle(FVector2f(X, Y), 6.0f + (i % 3) * 3.0f, A(C(0x8E6A4A), 0.7f));
		}
		const float ShaftL = 160.0f, ShaftR = 280.0f;
		P.RectV(FVector2f(ShaftL, GroundY), FVector2f(ShaftR - ShaftL, H - GroundY), C(0x2A2230), C(0x120D17));
		for (int32 Row = 0; Row < 8; ++Row)
		{
			const float Y = GroundY + Row * 24.0f;
			P.Rect(FVector2f(ShaftL - 14.0f, Y + 2.0f), FVector2f(12.0f, 20.0f), Row % 2 ? Stone : StoneDark);
			P.Rect(FVector2f(ShaftR + 2.0f, Y + 2.0f), FVector2f(12.0f, 20.0f), Row % 2 ? StoneDark : Stone);
		}
		P.RectV(FVector2f(ShaftL, H - 34.0f), FVector2f(ShaftR - ShaftL, 34.0f), WaterDeep, C(0x0E3050));
		P.Rect(FVector2f(ShaftL, H - 34.0f), FVector2f(ShaftR - ShaftL, 3.0f), A(Water, 0.9f));

		// Rim + posts + roof.
		for (int32 i = 0; i < 6; ++i)
		{
			P.RoundRect(FVector2f(138.0f + i * 28.0f, GroundY - 36.0f), FVector2f(27.0f, 18.0f), i % 2 ? Stone : C(0x9A98A2), 3.0f);
			P.RoundRect(FVector2f(124.0f + i * 28.0f + 14.0f, GroundY - 18.0f), FVector2f(27.0f, 18.0f), i % 2 ? C(0x9A98A2) : Stone, 3.0f);
		}
		P.Rect(FVector2f(146.0f, 44.0f), FVector2f(12.0f, GroundY - 80.0f), WoodDark);
		P.Rect(FVector2f(282.0f, 44.0f), FVector2f(12.0f, GroundY - 80.0f), WoodDark);
		P.Quad(FVector2f(116.0f, 50.0f), FVector2f(324.0f, 50.0f), FVector2f(270.0f, 14.0f), FVector2f(170.0f, 14.0f), C(0x9C3B2E));
		P.Rect(FVector2f(116.0f, 48.0f), FVector2f(208.0f, 6.0f), C(0x6E2A21));

		// Drum with rope coils (more coils = more wound).
		const float DrumY = Hub.Y;
		P.RoundRect(FVector2f(158.0f, DrumY - 16.0f), FVector2f(124.0f, 32.0f), Wood, 6.0f);
		const int32 Coils = 2 + FMath::FloorToInt(Wound * 12.0f);
		for (int32 i = 0; i < Coils; ++i)
		{
			const float X = 166.0f + i * 8.8f;
			P.Bar(FVector2f(X, DrumY - 15.0f), FVector2f(X + 5.0f, DrumY + 15.0f), 5.0f, C(0xD8B98A));
		}
		// Axle to the crank.
		P.Bar(FVector2f(282.0f, DrumY), Hub, 9.0f, Iron);

		// Rope + bucket.
		const float BucketTop = FMath::Lerp(H - 110.0f, GroundY - 70.0f, Wound);
		P.Bar(FVector2f(220.0f, DrumY + 14.0f), FVector2f(220.0f, BucketTop + 4.0f), 4.0f, C(0xD8B98A));
		Bucket(P, FVector2f(220.0f, BucketTop), 70.0f, 56.0f, SlipT > 0.0f ? FMath::Sin(Time * 40.0f) * 0.08f : 0.0f, 0.85f);

		// Crank wheel + handle.
		const float Glow = bGrab ? 1.0f : 0.0f;
		P.Circle(Hub, CrankR + 16.0f, A(Ink, 0.25f));
		if (!bGrab)
		{
			// Direction guide: chevrons running clockwise.
			for (int32 i = 0; i < 8; ++i)
			{
				const float Ang = Time * 1.6f + i * PI / 4.0f;
				const FVector2f D(FMath::Cos(Ang), FMath::Sin(Ang));
				const FVector2f T(-D.Y, D.X);
				const FVector2f At = Hub + D * (CrankR + 24.0f);
				P.Tri(At + T * 8.0f, At - T * 6.0f + D * 6.0f, At - T * 6.0f - D * 6.0f, A(Cream, 0.75f));
			}
		}
		P.Arc(Hub, CrankR, 0.0f, 2.0f * PI, A(Cream, 0.18f + 0.25f * Glow), 2.0f, 48);
		P.Circle(Hub, 30.0f, Wood, WoodDark, 4.0f);
		for (int32 i = 0; i < 4; ++i)
		{
			const float Ang = Angle + i * PI * 0.5f;
			P.Bar(Hub, Hub + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * 28.0f, 5.0f, WoodDark);
		}
		const FVector2f Knob = Hub + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * CrankR;
		P.Bar(Hub, Knob, 12.0f, Iron);
		P.Circle(Hub, 9.0f, C(0x2E3138));
		P.Circle(Knob, 15.0f, bGrab ? Gold : WoodLight, WoodDark, 3.0f);
		if (!bGrab && Wound < 0.02f)
		{
			P.HintRing(Knob, 22.0f, Time, Gold);
		}

		// Speed gauge.
		const FVector2f G(560.0f, 30.0f);
		P.Text(G + FVector2f(14.0f, -2.0f), TEXT("SPEED"), 10.0f, Cream, 0.5f, TEXT("Bold"), 100);
		const float SpeedFill = Saturate(Speed / (SlipSpeed * 1.25f));
		P.GaugeV(G + FVector2f(0.0f, 16.0f), FVector2f(28.0f, 150.0f), SpeedFill, SpeedFill > 0.8f ? Crimson : Gold, 0.0f, 0.8f);
		P.Text(G + FVector2f(14.0f, 172.0f), SlipT > 0.0f ? TEXT("SLIP!") : TEXT(""), 12.0f, C(0xFF8A80), 0.5f, TEXT("Black"));
		// Depth meter next to the shaft.
		P.Text(FVector2f(80.0f, GroundY + 20.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Wound * 100.0f)), 22.0f, Cream, 0.5f,
		       TEXT("Black"));
		P.Text(FVector2f(80.0f, GroundY + 48.0f), TEXT("WOUND UP"), 10.0f, CreamDim, 0.5f, TEXT("Bold"), 100);
	}

	void PaintCarry(FKGMgPainter& P) const
	{
		const float GroundY = 318.0f;
		KGMgCore::Outdoor(P, GroundY, C(0x88CBEF), C(0xF4EBD8));
		// Houses scrolling past (parallax).
		const float Scroll = Walk * 520.0f;
		for (int32 i = 0; i < 7; ++i)
		{
			const float X = FMath::Fmod(i * 150.0f - Scroll + 1050.0f, 1050.0f) - 150.0f;
			const float Hgt = 90.0f + (i * 37) % 50;
			P.Rect(FVector2f(X, GroundY - Hgt), FVector2f(110.0f, Hgt), i % 2 ? C(0xE8D8B8) : C(0xD9C29A));
			P.Tri(FVector2f(X - 10.0f, GroundY - Hgt), FVector2f(X + 120.0f, GroundY - Hgt), FVector2f(X + 55.0f, GroundY - Hgt - 50.0f),
			      i % 3 ? C(0xB5483A) : C(0x3F6E9E));
			P.Rect(FVector2f(X + 40.0f, GroundY - 46.0f), FVector2f(28.0f, 46.0f), WoodDark);
		}
		// Cobbles.
		P.Rect(FVector2f(0, GroundY), FVector2f(W, H - GroundY), C(0x9C9486));
		for (int32 i = 0; i < 18; ++i)
		{
			const float X = FMath::Fmod(i * 40.0f - Scroll * 2.0f + 1440.0f, 720.0f) - 40.0f;
			P.RoundRect(FVector2f(X, GroundY + 10.0f + (i % 2) * 22.0f), FVector2f(34.0f, 18.0f), C(0x857D70), 6.0f);
		}

		// Arm (sleeve + fist gripping the handle) + bucket.
		const FVector2f Piv = Pivot();
		const float Sway = FMath::Sin(Walk * 60.0f) * 3.0f;
		P.Bar(FVector2f(300.0f + Sway, -10.0f), Piv + FVector2f(-2.0f, -36.0f), 40.0f, C(0x3F6E9E));
		P.Bar(FVector2f(284.0f + Sway, -10.0f), Piv + FVector2f(-18.0f, -36.0f), 6.0f, A(Ink, 0.25f));
		Bucket(P, Piv + FVector2f(0.0f, -28.0f), 116.0f, 100.0f, FMath::DegreesToRadians(Tilt), Carried);
		P.RoundRect(Piv + FVector2f(-24.0f, -44.0f), FVector2f(48.0f, 14.0f), C(0x2E5378), 5.0f);
		P.RoundRect(Piv + FVector2f(-19.0f, -32.0f), FVector2f(38.0f, 34.0f), C(0xE8B08A), 12.0f);
		for (int32 k = 0; k < 3; ++k)
		{
			P.Segment(Piv + FVector2f(-10.0f + k * 10.0f, -12.0f), Piv + FVector2f(-10.0f + k * 10.0f, 0.0f), A(C(0x9A5E3E), 0.7f), 2.0f);
		}
		for (const FDrop& D : Drops)
		{
			P.Circle(D.P, 4.0f * (1.0f - D.Age * 0.6f), A(Water, 1.0f - D.Age));
		}

		// Tilt gauge (level) under the bucket.
		const FVector2f G(320.0f, 385.0f);
		P.ArcBand(G, 44.0f, 54.0f, -PI, 0.0f, A(Ink, 0.7f), 30);
		const float Z = FMath::DegreesToRadians(22.0f) * 1.2f;
		P.ArcBand(G, 44.0f, 54.0f, -0.5f * PI - Z, -0.5f * PI + Z, A(Good, 0.85f), 12);
		const float N = -0.5f * PI + FMath::Clamp(FMath::DegreesToRadians(Tilt) * 1.2f, -0.5f * PI, 0.5f * PI);
		P.Bar(G, G + FVector2f(FMath::Cos(N), FMath::Sin(N)) * 58.0f, 4.0f, FMath::Abs(Tilt) > 22.0f ? Crimson : Cream);
		P.Circle(G, 6.0f, Cream);

		// Route (well -> trough) and water left.
		const FVector2f R0(40.0f, 22.0f), R1(600.0f, 22.0f);
		P.Bar(R0, R1, 6.0f, A(Ink, 0.45f));
		P.Bar(R0, FMath::Lerp(R0, R1, Walk), 6.0f, Gold);
		P.Circle(R0, 10.0f, Stone, StoneDark, 2.0f);
		P.RoundRect(R1 - FVector2f(16.0f, 8.0f), FVector2f(32.0f, 16.0f), Wood, 3.0f);
		P.Circle(FMath::Lerp(R0, R1, Walk), 9.0f, Cream, WoodDark, 2.0f);
		P.Text(FVector2f(40.0f, 38.0f), TEXT("WELL"), 10.0f, Ink, 0.5f, TEXT("Bold"), 80);
		P.Text(FVector2f(600.0f, 38.0f), TEXT("TROUGH"), 10.0f, Ink, 0.5f, TEXT("Bold"), 80);
		P.Text(FVector2f(40.0f, 66.0f), TEXT("WATER"), 10.0f, Ink, 0.0f, TEXT("Bold"), 100);
		P.Gauge(FVector2f(40.0f, 82.0f), FVector2f(150.0f, 16.0f), Carried, Carried < 0.5f ? Crimson : KGMg::Water, 0.35f, 1.0f);
	}
};

// =====================================================================================================================
// RingBell: pull the rope when the approach ring closes (the bell's swing apex), three good strokes.
// =====================================================================================================================
class FKGMgRingBell final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Pull the rope down when the ring closes on the grip - 3 strokes on the beat");
	}

	virtual void BeginStage() override
	{
		Rung = 0;
		Pull = 0.0f;
		bGrab = false;
		bFired = false;
		Amp = 0.18f;
		Waves.Reset();
		LastBeat = -1;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (FVector2f::Distance(Pos, Sally()) < 70.0f)
		{
			bGrab = true;
			GrabY = Pos.Y;
			bFired = false;
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { bGrab = false; }

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (bGrab)
		{
			Pull = FMath::Clamp(Pos.Y - GrabY, 0.0f, 80.0f);
			if (Pull > 48.0f && !bFired)
			{
				bFired = true;
				Stroke();
			}
		}
	}

	virtual void OnKey(const FKey& Key) override
	{
		if (Key == EKeys::SpaceBar)
		{
			Pull = 60.0f;
			Stroke();
		}
	}

	virtual void Tick(float Dt) override
	{
		if (!bGrab)
		{
			Pull = Approach(Pull, 0.0f, Dt, 9.0f);
		}
		for (int32 i = Waves.Num() - 1; i >= 0; --i)
		{
			Waves[i] += Dt;
			if (Waves[i] > 1.4f)
			{
				Waves.RemoveAt(i);
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		const int32 Beat = FMath::RoundToInt(StageTime / Period);
		if (Beat >= 1 && FMath::Abs(StageTime - Beat * Period) < 0.04f && Beat != LastBeat)
		{
			LastBeat = Beat;
			Pull = 60.0f;
			Stroke();
		}
	}

	virtual void DebugPose() override
	{
		Rung = 1;
		Amp = 0.3f;
		StageTime = Period * 2.0f - 0.5f;
		Pull = 22.0f;
		Waves = {0.5f};
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		// Belfry: stone walls, a window slit, the headstock beam.
		P.RectV(FVector2f(0, 0), FVector2f(W, H), C(0x4A4452), C(0x2A2530));
		for (int32 Row = 0; Row < 14; ++Row)
		{
			for (int32 Col = 0; Col < 11; ++Col)
			{
				const float X = Col * 62.0f - (Row % 2) * 31.0f;
				P.RoundRect(FVector2f(X + 2.0f, Row * 30.0f + 2.0f), FVector2f(58.0f, 26.0f), A(C(0x5E5868), 0.55f + 0.1f * ((Row * 7 + Col) % 3)), 4.0f);
			}
		}
		P.RoundRect(FVector2f(66.0f, 90.0f), FVector2f(54.0f, 150.0f), C(0x9FD6F5), -1.0f);
		P.RectV(FVector2f(66.0f, 180.0f), FVector2f(54.0f, 60.0f), A(C(0xF6C58A), 0.0f), A(C(0xF6C58A), 0.8f));
		P.Rect(FVector2f(0, 30.0f), FVector2f(W, 26.0f), WoodDark);
		P.Rect(FVector2f(0, 30.0f), FVector2f(W, 5.0f), WoodLight);

		// Bell (slices so the flared profile stays convex per piece).
		const float Swing = Amp * FMath::Cos(PI * StageTime / Period);
		const FVector2f Piv(300.0f, 56.0f);
		auto Pt = [&](float X, float Y) { return Piv + KGMgCore::Rot(FVector2f(X, Y), Swing); };
		P.Circle(Piv, 58.0f, A(Ink, 0.25f));
		P.Arc(Piv, 50.0f, 0.0f, 2.0f * PI, Wood, 8.0f, 40);
		for (int32 i = 0; i < 6; ++i)
		{
			const float Ang = Swing * 3.0f + i * PI / 3.0f;
			P.Bar(Piv, Piv + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * 48.0f, 4.0f, WoodDark);
		}
		const float Prof[][2] = {{0.0f, 34.0f}, {20.0f, 46.0f}, {60.0f, 52.0f}, {100.0f, 60.0f}, {130.0f, 78.0f}, {150.0f, 100.0f}, {160.0f, 108.0f}};
		const FLinearColor Bronze = C(0xC8943A);
		const FLinearColor BronzeDark = C(0x8C5E22);
		// Clapper swings behind the lip, lagging the bell.
		const FVector2f ClapTop = Pt(0.0f, 60.0f);
		const FVector2f ClapEnd = ClapTop + KGMgCore::Rot(FVector2f(0.0f, 100.0f), -Swing * 0.6f);
		for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(Prof); ++i)
		{
			const float Y0 = Prof[i][0] + 10.0f, Y1 = Prof[i + 1][0] + 10.0f;
			const float W0 = Prof[i][1], W1 = Prof[i + 1][1];
			P.Quad(Pt(-W0, Y0), Pt(W0, Y0), Pt(W1, Y1), Pt(-W1, Y1), i % 2 ? Bronze : C(0xD49F45));
			P.Quad(Pt(-W0 * 0.55f, Y0), Pt(-W0 * 0.25f, Y0), Pt(-W1 * 0.25f, Y1), Pt(-W1 * 0.55f, Y1), A(C(0xFFE3A0), 0.35f));
		}
		P.Bar(Pt(-108.0f, 170.0f), Pt(108.0f, 170.0f), 8.0f, BronzeDark);
		P.Bar(ClapTop, ClapEnd, 7.0f, Iron);
		P.Circle(ClapEnd, 12.0f, Iron);
		// Sound waves from the mouth.
		for (const float Age : Waves)
		{
			const float R = 60.0f + Age * 220.0f;
			P.Arc(Pt(0.0f, 150.0f), R, 0.15f * PI, 0.85f * PI, A(Gold, 0.9f * (1.0f - Age / 1.4f)), 4.0f, 32);
		}

		// Rope: from the wheel over a pulley down to the grip.
		const FVector2f S = Sally();
		const FVector2f WheelOut = Piv + FVector2f(FMath::Cos(Swing * 3.0f), FMath::Sin(Swing * 3.0f)) * 50.0f;
		P.Circle(FVector2f(470.0f, 64.0f), 12.0f, Iron);
		P.Bar(WheelOut, FVector2f(470.0f, 52.0f), 5.0f, C(0xD8B98A));
		P.Bar(FVector2f(478.0f, 64.0f), S + FVector2f(0.0f, -40.0f), 6.0f, C(0xD8B98A));
		// Sally: the striped woollen grip.
		for (int32 i = 0; i < 6; ++i)
		{
			const FLinearColor Stripe = i % 3 == 0 ? Crimson : i % 3 == 1 ? Cream : C(0x2F5DA8);
			P.RoundRect(S + FVector2f(-15.0f, -40.0f + i * 13.0f), FVector2f(30.0f, 14.0f), Stripe, 6.0f);
		}
		P.Bar(S + FVector2f(0.0f, 38.0f), S + FVector2f(0.0f, 80.0f), 6.0f, C(0xD8B98A));

		// Approach ring: closes on the grip at every beat.
		const float Next = FMath::CeilToFloat(StageTime / Period + 0.0001f) * Period;
		const float Prev = Next - Period;
		const float ToNext = Next - StageTime;
		const float SincePrev = StageTime - Prev;
		const bool bWindow = ToNext < Window || (SincePrev < Window && Prev > 0.0f);
		const float Ring = 44.0f + 110.0f * Saturate(ToNext / Period);
		P.Arc(S, 44.0f, 0.0f, 2.0f * PI, A(bWindow ? Good : Cream, 0.9f), 4.0f, 40);
		P.Arc(S, Ring, 0.0f, 2.0f * PI, A(bWindow ? Good : Gold, 0.35f + 0.6f * (1.0f - Saturate(ToNext / Period))), 5.0f, 48);
		if (bWindow)
		{
			P.Glow(S, 80.0f, A(Good, 0.35f));
		}
		if (Rung == 0 && !bGrab)
		{
			P.HintArrow(S + FVector2f(56.0f, -30.0f), S + FVector2f(56.0f, 40.0f), Time, Cream);
		}

		// Strokes so far.
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector2f B(40.0f + i * 44.0f, 350.0f);
			const bool bOn = i < Rung;
			P.Circle(B, 18.0f, bOn ? Gold : A(Ink, 0.6f), A(Cream, bOn ? 0.0f : 0.4f), 2.0f);
			P.Tri(B + FVector2f(0.0f, -10.0f), B + FVector2f(10.0f, 8.0f), B + FVector2f(-10.0f, 8.0f), bOn ? BronzeDark : A(Cream, 0.3f));
		}
		P.Text(FVector2f(40.0f, 374.0f), TEXT("STROKES"), 10.0f, CreamDim, 0.0f, TEXT("Bold"), 100);
	}

private:
	static constexpr float Period = 1.5f;
	static constexpr float Window = 0.22f;
	int32 Rung = 0;
	float Pull = 0.0f;
	float GrabY = 0.0f;
	bool bGrab = false;
	bool bFired = false;
	float Amp = 0.18f;
	TArray<float> Waves;
	int32 LastBeat = -1;

	FVector2f Sally() const { return FVector2f(478.0f, 268.0f + Pull); }

	void Stroke()
	{
		const int32 Beat = FMath::RoundToInt(StageTime / Period);
		const float Off = FMath::Abs(StageTime - Beat * Period);
		if (Beat >= 1 && Off <= Window)
		{
			++Rung;
			Amp = FMath::Min(0.42f, Amp + 0.08f);
			Waves.Add(0.0f);
			Nice(FVector2f(300.0f, 250.0f), Rung == 3 ? TEXT("DONG!!!") : TEXT("DONG!"), TEXT("S_ChurchBell"), 1.0f + 0.06f * Rung);
			if (Rung >= 3)
			{
				Solve();
			}
		}
		else
		{
			Oops(FVector2f(300.0f, 250.0f), StageTime - Beat * Period < 0.0f ? TEXT("Too early") : TEXT("Too late"), TEXT("S_UI_Bad"), 4.0f);
		}
	}
};

// =====================================================================================================================
// ChopWood: stop the swinging needle in the green to split each log clean (5 logs).
// =====================================================================================================================
class FKGMgChopWood final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Click when the needle is in the green: a clean swing splits the log");
	}

	virtual void BeginStage() override
	{
		Logs = 0;
		NewLog();
	}

	virtual void OnPress(const FVector2f& Pos) override { Swing(); }

	virtual void OnKey(const FKey& Key) override
	{
		if (Key == EKeys::SpaceBar)
		{
			Swing();
		}
	}

	virtual void Tick(float Dt) override
	{
		if (StuckT > 0.0f)
		{
			StuckT -= Dt;
		}
		else if (SplitT < 0.0f && AxeT < 0.0f)
		{
			Needle += Dir * Dt * NeedleSpeed;
			if (Needle > 1.0f || Needle < 0.0f)
			{
				Dir = -Dir;
				Needle = Saturate(Needle);
			}
		}
		if (AxeT >= 0.0f)
		{
			AxeT += Dt;
			if (AxeT > 0.35f)
			{
				AxeT = -1.0f;
			}
		}
		if (SplitT >= 0.0f)
		{
			SplitT += Dt;
			if (SplitT > 0.75f)
			{
				SplitT = -1.0f;
				if (Logs >= 5)
				{
					Solve();
				}
				else
				{
					NewLog();
				}
			}
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (FMath::Abs(Needle - (Zone0 + Zone1) * 0.5f) < 0.02f)
		{
			Swing();
		}
	}

	virtual void DebugPose() override
	{
		Logs = 2;
		Needle = Zone0 - 0.06f;
		Cracked = true;
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		const float GroundY = 280.0f;
		KGMgCore::Outdoor(P, GroundY, C(0x9ED7F2), C(0xF7EFD9));
		// Pines.
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = 20.0f + i * 78.0f;
			const float Hgt = 110.0f + (i * 31) % 60;
			P.Rect(FVector2f(X - 4.0f, GroundY - 30.0f), FVector2f(8.0f, 30.0f), WoodDark);
			P.Tri(FVector2f(X - 34.0f, GroundY - 24.0f), FVector2f(X + 34.0f, GroundY - 24.0f), FVector2f(X, GroundY - Hgt), C(0x2F6B3E));
			P.Tri(FVector2f(X - 26.0f, GroundY - 60.0f), FVector2f(X + 26.0f, GroundY - 60.0f), FVector2f(X, GroundY - Hgt - 20.0f), C(0x3C7F4A));
		}
		P.RectV(FVector2f(0, GroundY), FVector2f(W, H - GroundY), C(0x7DB257), C(0x5A8A3E));

		// Split pile.
		for (int32 i = 0; i < Logs * 2; ++i)
		{
			const int32 Row = i < 5 ? 0 : 1;
			const float X = 40.0f + (i % 5) * 22.0f + Row * 11.0f;
			const float Y = GroundY + 8.0f - Row * 18.0f;
			P.Tri(FVector2f(X, Y), FVector2f(X + 20.0f, Y), FVector2f(X + 10.0f, Y - 18.0f), C(0xE2B77F));
			P.Arc(FVector2f(X + 10.0f, Y - 4.0f), 6.0f, PI, 2.0f * PI, C(0xB98256), 1.5f, 8);
		}

		// Stump + log.
		const FVector2f Block(300.0f, GroundY + 20.0f);
		P.RoundRect(Block + FVector2f(-70.0f, -20.0f), FVector2f(140.0f, 70.0f), C(0x6E4A30), 8.0f);
		P.Disc(Block + FVector2f(0.0f, -20.0f), FVector2f(70.0f, 16.0f), C(0xD9B07A), C(0xB98256));
		const float Split = SplitT >= 0.0f ? EaseOutCubic(SplitT / 0.4f) : 0.0f;
		const float LogH = 110.0f;
		const FVector2f LogBase = Block + FVector2f(0.0f, -26.0f);
		auto Half = [&](float Side)
		{
			const float Off = Side * (90.0f * Split);
			const float Rot = Side * 0.9f * Split;
			const FVector2f Base = LogBase + FVector2f(Off, -30.0f * FMath::Sin(Split * PI));
			TArray<FVector2f> Pts;
			Pts.Add(Base + KGMgCore::Rot(FVector2f(0.0f, 0.0f), Rot));
			Pts.Add(Base + KGMgCore::Rot(FVector2f(Side * 42.0f, 0.0f), Rot));
			Pts.Add(Base + KGMgCore::Rot(FVector2f(Side * 42.0f, -LogH), Rot));
			Pts.Add(Base + KGMgCore::Rot(FVector2f(0.0f, -LogH), Rot));
			P.Poly(Pts, C(0x7A5234));
			// Bark highlight on the outer edge; the pale heartwood face only shows once split.
			P.Bar(Base + KGMgCore::Rot(FVector2f(Side * 34.0f, 0.0f), Rot), Base + KGMgCore::Rot(FVector2f(Side * 34.0f, -LogH), Rot), 6.0f,
			      A(C(0x9C6B45), 0.8f));
			if (Split > 0.0f)
			{
				P.Bar(Base + KGMgCore::Rot(FVector2f(Side * 3.0f, 0.0f), Rot), Base + KGMgCore::Rot(FVector2f(Side * 3.0f, -LogH), Rot), 6.0f,
				      C(0xE2B77F));
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const float Y = -18.0f - k * 26.0f;
				P.Segment(Base + KGMgCore::Rot(FVector2f(Side * 12.0f, Y), Rot), Base + KGMgCore::Rot(FVector2f(Side * 36.0f, Y - 8.0f), Rot),
				          A(Ink, 0.35f), 2.0f);
			}
		};
		Half(-1.0f);
		Half(1.0f);
		if (Split <= 0.001f)
		{
			P.Disc(LogBase + FVector2f(0.0f, -LogH), FVector2f(42.0f, 12.0f), C(0xE8C48E), C(0xC99A62));
			P.Arc(LogBase + FVector2f(0.0f, -LogH), 22.0f, 0.0f, 2.0f * PI, A(C(0x9C6B3F), 0.8f), 1.5f, 24);
			if (Cracked)
			{
				P.Line({LogBase + FVector2f(0, -LogH), LogBase + FVector2f(6, -LogH + 30), LogBase + FVector2f(-4, -LogH + 60)}, Ink, 3.0f);
			}
		}

		// Axe: held from the bottom right, raised over the log at rest, sweeps down onto the log top on a swing,
		// wobbles when stuck.
		constexpr float Rest = -1.85f;    // raised, leaning towards the log
		constexpr float Strike = -2.62f;  // down-left onto the log
		float AxeAng = Rest;
		if (AxeT >= 0.0f)
		{
			const float Down = EaseOutCubic(FMath::Min(1.0f, AxeT / 0.12f));
			const float Up = AxeT > 0.2f ? EaseOutCubic((AxeT - 0.2f) / 0.15f) : 0.0f;
			AxeAng = FMath::Lerp(FMath::Lerp(Rest, Strike, Down), Rest, Up);
		}
		if (StuckT > 0.0f)
		{
			AxeAng = Strike + 0.05f * FMath::Sin(Time * 40.0f);
		}
		const FVector2f Grip(600.0f, 372.0f);
		const FVector2f Dir2(FMath::Cos(AxeAng), FMath::Sin(AxeAng));
		const FVector2f Head = Grip + Dir2 * 300.0f;
		P.Bar(Grip, Head, 13.0f, C(0xC9A26B));
		P.Bar(Grip, Grip + Dir2 * 60.0f, 15.0f, C(0x8A5A3B));   // leather wrap
		// Blade on the side facing the swing (towards the log).
		// The swing turns the axe towards smaller angles: the blade (-N side) leads.
		const FVector2f N(-Dir2.Y, Dir2.X);
		const FVector2f B0 = Head - Dir2 * 6.0f;
		P.Quad(B0 + N * 8.0f, B0 + N * 8.0f - Dir2 * 30.0f, B0 - N * 40.0f - Dir2 * 38.0f, B0 - N * 44.0f + Dir2 * 8.0f, C(0x8B929C));
		P.Bar(B0 - N * 42.0f - Dir2 * 36.0f, B0 - N * 46.0f + Dir2 * 6.0f, 5.0f, C(0xE6EBF0));
		P.Quad(B0 + N * 8.0f, B0 + N * 20.0f - Dir2 * 4.0f, B0 + N * 20.0f - Dir2 * 22.0f, B0 + N * 8.0f - Dir2 * 30.0f, C(0x5E646E));

		// Timing meter.
		const FVector2f M0(120.0f, 356.0f);
		const float MW = 400.0f;
		P.RoundRect(M0 - FVector2f(8.0f, 8.0f), FVector2f(MW + 16.0f, 36.0f), A(Ink, 0.75f), 12.0f);
		P.RoundRect(M0, FVector2f(MW, 20.0f), C(0x8C2A2A), 6.0f);
		P.Rect(M0 + FVector2f(MW * (Zone0 - Near), 0.0f), FVector2f(MW * (Zone1 - Zone0 + Near * 2.0f), 20.0f), C(0xE0B03A));
		P.Rect(M0 + FVector2f(MW * Zone0, 0.0f), FVector2f(MW * (Zone1 - Zone0), 20.0f), Good);
		const float NX = M0.X + MW * Needle;
		P.Tri(FVector2f(NX, M0.Y + 4.0f), FVector2f(NX - 9.0f, M0.Y - 12.0f), FVector2f(NX + 9.0f, M0.Y - 12.0f), Cream);
		P.Bar(FVector2f(NX, M0.Y - 2.0f), FVector2f(NX, M0.Y + 24.0f), 4.0f, Cream);
		P.Text(FVector2f(M0.X + MW + 30.0f, M0.Y - 2.0f), FString::Printf(TEXT("%d / 5"), Logs), 18.0f, Cream, 0.0f, TEXT("Black"));
		if (StuckT > 0.0f)
		{
			P.Tag(FVector2f(300.0f, 60.0f), TEXT("STUCK - wrench it free..."), A(Crimson, 0.9f), Cream, 14.0f);
		}
	}

private:
	static constexpr float Near = 0.09f;
	int32 Logs = 0;
	float Needle = 0.0f;
	float Dir = 1.0f;
	float NeedleSpeed = 1.1f;
	float Zone0 = 0.45f;
	float Zone1 = 0.57f;
	float AxeT = -1.0f;
	float SplitT = -1.0f;
	float StuckT = 0.0f;
	bool Cracked = false;

	void NewLog()
	{
		const float Width = FMath::Lerp(0.14f, 0.08f, Logs / 4.0f);
		Zone0 = 0.15f + Rng.FRand() * (0.7f - Width);
		Zone1 = Zone0 + Width;
		NeedleSpeed = 1.0f + 0.12f * Logs;
		Needle = 0.0f;
		Dir = 1.0f;
		Cracked = false;
	}

	void Swing()
	{
		if (StuckT > 0.0f || SplitT >= 0.0f || AxeT >= 0.0f || bSolved)
		{
			return;
		}
		AxeT = 0.0f;
		Sound(TEXT("S_Chore_Whoosh"), 0.6f, 1.0f);
		if (Needle >= Zone0 && Needle <= Zone1)
		{
			++Logs;
			SplitT = 0.0f;
			Nice(FVector2f(300.0f, 120.0f), TEXT("Clean split!"), TEXT("S_Chore_Chop"), 1.0f);
		}
		else if (Needle >= Zone0 - Near && Needle <= Zone1 + Near)
		{
			if (Cracked)
			{
				++Logs;
				SplitT = 0.0f;
				Nice(FVector2f(300.0f, 120.0f), TEXT("Split!"), TEXT("S_Chore_Chop"), 0.9f);
			}
			else
			{
				Cracked = true;
				PopText(FVector2f(300.0f, 120.0f), TEXT("Cracked - again!"), Gold);
				Sound(TEXT("S_Chore_Chop"), 0.7f, 0.75f);
			}
		}
		else
		{
			StuckT = 0.9f;
			Oops(FVector2f(300.0f, 120.0f), TEXT("Stuck!"), TEXT("S_Chore_Chop"), 7.0f);
		}
	}
};

// =====================================================================================================================
// MendNets: drag each frayed rope end to the knot of the same colour (and shape).
// =====================================================================================================================
class FKGMgMendNets final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return TEXT("Drag each frayed rope to the knot with the same colour and mark");
	}

	virtual void BeginStage() override
	{
		const int32 Count = Stage == 0 ? 5 : 6;
		Right.Reset();
		Linked.Init(-1, Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Right.Add(i);
		}
		Rng.Shuffle(Right);
		Dragging = -1;
		Snap.Init(0.0f, Count);
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		for (int32 i = 0; i < Linked.Num(); ++i)
		{
			if (Linked[i] < 0 && FVector2f::Distance(Pos, LeftEnd(i)) < 30.0f)
			{
				Dragging = i;
				Sound(TEXT("S_UI_Click"), 0.5f, 1.2f);
				return;
			}
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (Dragging < 0)
		{
			return;
		}
		for (int32 j = 0; j < Right.Num(); ++j)
		{
			if (FVector2f::Distance(Pos, RightEnd(j)) < 36.0f)
			{
				if (Right[j] == Dragging)
				{
					Linked[Dragging] = j;
					Nice(RightEnd(j) + FVector2f(-40.0f, -10.0f), TEXT(""), TEXT("S_Chore_Knot"), 0.9f + 0.05f * Dragging);
					bool bAll = true;
					for (const int32 L : Linked)
					{
						bAll &= L >= 0;
					}
					if (bAll)
					{
						Solve();
					}
				}
				else
				{
					Snap[Dragging] = 1.0f;
					Oops(RightEnd(j) + FVector2f(-40.0f, -10.0f), TEXT("Wrong knot"));
				}
				break;
			}
		}
		Dragging = -1;
	}

	virtual void Tick(float Dt) override
	{
		for (float& S : Snap)
		{
			S = FMath::Max(0.0f, S - Dt * 3.0f);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		for (int32 i = 0; i < Linked.Num(); ++i)
		{
			if (Linked[i] < 0 && StageTime > 0.7f * (i + 1))
			{
				Linked[i] = Right.IndexOfByKey(i);
				Sound(TEXT("S_Chore_Knot"), 0.8f, 1.0f);
				break;
			}
		}
		// Same completion rule as OnRelease: the stage is done once every rope is tied. (Stabilisation 2026-09-26:
		// the scripted player tied every rope but never solved the stage - KillGodot.Chores.Timing hit the give-up.)
		if (!bSolved && !Linked.Contains(-1))
		{
			Solve();
		}
	}

	virtual void DebugPose() override
	{
		for (int32 i = 0; i < 2; ++i)
		{
			Linked[i] = Right.IndexOfByKey(i);
		}
		Dragging = 2;
		const int32 Target = Right.IndexOfByKey(2);
		Mouse = FMath::Lerp(LeftEnd(2), RightEnd(Target), 0.62f) + FVector2f(0.0f, 12.0f);
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		// Sandy dock planks under a hanging net with a torn middle.
		P.RectV(FVector2f(0, 0), FVector2f(W, H), C(0xC9A574), C(0xA9804F));
		for (int32 i = 0; i < 9; ++i)
		{
			P.Rect(FVector2f(0.0f, i * 46.0f), FVector2f(W, 2.0f), A(C(0x6E4A2A), 0.5f));
		}
		const FLinearColor NetC = A(C(0x3E5A5C), 0.8f);
		const float Cell = 34.0f;
		for (float X = -H; X < W + H; X += Cell)
		{
			// Two diagonal families; skip the torn hole in the middle.
			for (int32 Fam = 0; Fam < 2; ++Fam)
			{
				const float S = Fam == 0 ? 1.0f : -1.0f;
				FVector2f A0(X, 0.0f), A1(X + S * H, H);
				const int32 Steps = 14;
				for (int32 k = 0; k < Steps; ++k)
				{
					const FVector2f P0 = FMath::Lerp(A0, A1, k / float(Steps));
					const FVector2f P1 = FMath::Lerp(A0, A1, (k + 1) / float(Steps));
					const FVector2f Mid = (P0 + P1) * 0.5f;
					const float Hole = ((Mid.X - 320.0f) / 170.0f) * ((Mid.X - 320.0f) / 170.0f) + ((Mid.Y - 200.0f) / 150.0f) * ((Mid.Y - 200.0f) / 150.0f);
					if (Hole > 1.0f)
					{
						P.Segment(P0, P1, NetC, 2.0f);
					}
				}
			}
		}
		// Posts holding the rope ends.
		P.Rect(FVector2f(22.0f, 0.0f), FVector2f(28.0f, H), WoodDark);
		P.Rect(FVector2f(590.0f, 0.0f), FVector2f(28.0f, H), WoodDark);

		// Connected ropes, the dragged one, then the ends on top.
		for (int32 i = 0; i < Linked.Num(); ++i)
		{
			if (Linked[i] >= 0)
			{
				RopeCurve(P, LeftEnd(i), RightEnd(Linked[i]), Color(i), 26.0f);
			}
		}
		if (Dragging >= 0)
		{
			RopeCurve(P, LeftEnd(Dragging), Mouse, Color(Dragging), 18.0f);
			P.Circle(Mouse, 10.0f, Color(Dragging), Cream, 2.0f);
		}
		for (int32 i = 0; i < Linked.Num(); ++i)
		{
			const FVector2f E = LeftEnd(i) + FVector2f(-Snap[i] * 20.0f * FMath::Sin(Time * 50.0f), 0.0f);
			P.Bar(FVector2f(36.0f, E.Y), E, 12.0f, Color(i));
			if (Linked[i] < 0)
			{
				// Frayed end.
				for (int32 f = -2; f <= 2; ++f)
				{
					P.Segment(E, E + FVector2f(14.0f, f * 5.0f), Color(i), 2.5f);
				}
				if (Dragging < 0)
				{
					P.HintRing(E + FVector2f(6.0f, 0.0f), 20.0f, Time + i * 0.2f, A(Cream, 0.5f));
				}
			}
			KGMgCore::Symbol(P, i, FVector2f(36.0f, E.Y), 7.0f, Cream);
		}
		for (int32 j = 0; j < Right.Num(); ++j)
		{
			const FVector2f E = RightEnd(j);
			const int32 Id = Right[j];
			const bool bDone = Linked.IsValidIndex(Id) && Linked[Id] == j;
			P.Bar(E, FVector2f(604.0f, E.Y), 12.0f, Color(Id));
			P.Circle(E, 16.0f, bDone ? Color(Id) : A(Ink, 0.55f), Color(Id), 4.0f);
			KGMgCore::Symbol(P, Id, E, 7.0f, bDone ? Ink : Cream);
		}
		P.Text(FVector2f(320.0f, 12.0f), FString::Printf(TEXT("%d / %d mended"), Linked.FilterByPredicate([](int32 L) { return L >= 0; }).Num(),
		                                                  Linked.Num()),
		       14.0f, Ink, 0.5f, TEXT("Black"));
	}

private:
	TArray<int32> Right;    // right slot -> rope id
	TArray<int32> Linked;   // rope id -> right slot (or -1)
	TArray<float> Snap;
	int32 Dragging = -1;

	float SlotY(int32 i) const
	{
		const int32 N = FMath::Max(1, Linked.Num());
		return 62.0f + (H - 110.0f) * i / float(FMath::Max(1, N - 1));
	}

	FVector2f LeftEnd(int32 i) const { return FVector2f(88.0f, SlotY(i)); }
	FVector2f RightEnd(int32 j) const { return FVector2f(560.0f, SlotY(j)); }

	static FLinearColor Color(int32 i)
	{
		static const uint32 Colors[] = {0xE0413A, 0xF2C230, 0x3FA7D6, 0x8BD160, 0xB06AD9, 0xF28C28};
		return C(Colors[i % UE_ARRAY_COUNT(Colors)]);
	}

	static void RopeCurve(FKGMgPainter& P, const FVector2f& From, const FVector2f& To, const FLinearColor& Col, float Sag)
	{
		TArray<FVector2f> Pts;
		const FVector2f Mid = (From + To) * 0.5f + FVector2f(0.0f, Sag);
		for (int32 k = 0; k <= 20; ++k)
		{
			const float T = k / 20.0f;
			Pts.Add(FMath::Lerp(FMath::Lerp(From, Mid, T), FMath::Lerp(Mid, To, T), T));
		}
		P.Line(Pts, A(Ink, 0.5f), 11.0f);
		P.Line(Pts, Col, 8.0f);
		// Twist marks.
		for (int32 k = 1; k < Pts.Num() - 1; k += 2)
		{
			const FVector2f D = (Pts[k + 1] - Pts[k - 1]).GetSafeNormal();
			const FVector2f N(-D.Y, D.X);
			P.Segment(Pts[k] - N * 3.5f - D * 2.0f, Pts[k] + N * 3.5f + D * 2.0f, A(Ink, 0.3f), 1.5f);
		}
	}
};

} // namespace KGMg


TUniquePtr<FKGMinigame> KGMakeMinigame_DrawWater() { return MakeUnique<KGMg::FKGMgDrawWater>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_RingBell() { return MakeUnique<KGMg::FKGMgRingBell>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_ChopWood() { return MakeUnique<KGMg::FKGMgChopWood>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_MendNets() { return MakeUnique<KGMg::FKGMgMendNets>(); }
