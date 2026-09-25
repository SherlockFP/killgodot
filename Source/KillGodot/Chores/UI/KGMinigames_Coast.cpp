// Chore minigames (Coast): UnloadFish, FuelLighthouse, FixBoat, LightHarbourLamp, FeedKoi, HarvestCarrots.
// Contract: see KGMinigame.h. Anti-cheat: every stage paces itself (timed arrivals, speed caps, fixed animations) so a
// perfect run still stays above its server floor (KGChoreTypes.cpp); a MinSolve guard per stage backs that up.
#include "Chores/UI/KGMinigame.h"

namespace KGMg
{
namespace KGMgCoast
{
	FVector2f Rot(const FVector2f& V, float Ang)
	{
		const float Co = FMath::Cos(Ang);
		const float Si = FMath::Sin(Ang);
		return FVector2f(V.X * Co - V.Y * Si, V.X * Si + V.Y * Co);
	}

	float WrapPi(float Ang)
	{
		while (Ang > PI)
		{
			Ang -= 2.0f * PI;
		}
		while (Ang < -PI)
		{
			Ang += 2.0f * PI;
		}
		return Ang;
	}

	/** Turns Heading toward Desired by at most MaxStep radians. */
	float TurnToward(float Heading, float Desired, float MaxStep)
	{
		return WrapPi(Heading + FMath::Clamp(WrapPi(Desired - Heading), -MaxStep, MaxStep));
	}

	FVector2f Unit(float Ang)
	{
		return FVector2f(FMath::Cos(Ang), FMath::Sin(Ang));
	}

	/** Deterministic 0..1 hash for decoration (Paint is const: no Rng there). */
	float Hash01(int32 Seed)
	{
		uint32 X = static_cast<uint32>(Seed) * 747796405u + 2891336453u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return static_cast<float>(X & 0xFFFFFFu) / 16777216.0f;
	}

	TArray<FVector2f> Ellipse(const FVector2f& Centre, const FVector2f& Radii, float Ang = 0.0f, int32 Segments = 24)
	{
		TArray<FVector2f> Pts;
		Pts.Reserve(Segments);
		for (int32 i = 0; i < Segments; ++i)
		{
			const float T = 2.0f * PI * float(i) / float(Segments);
			Pts.Add(Centre + Rot(FVector2f(FMath::Cos(T) * Radii.X, FMath::Sin(T) * Radii.Y), Ang));
		}
		return Pts;
	}

	/** Convex cap of an ellipse between angles T0..T1 (a span <= PI stays convex). */
	TArray<FVector2f> EllipseCap(const FVector2f& Centre, const FVector2f& Radii, float T0, float T1, float Ang = 0.0f, int32 Segments = 12)
	{
		TArray<FVector2f> Pts;
		Pts.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float T = FMath::Lerp(T0, T1, float(i) / float(Segments));
			Pts.Add(Centre + Rot(FVector2f(FMath::Cos(T) * Radii.X, FMath::Sin(T) * Radii.Y), Ang));
		}
		return Pts;
	}

	/** Ellipse outline. */
	void Ring(FKGMgPainter& P, const FVector2f& Centre, const FVector2f& Radii, const FLinearColor& Color, float Thickness, int32 Segments = 32)
	{
		TArray<FVector2f> Pts = Ellipse(Centre, Radii, 0.0f, Segments);
		const FVector2f First = Pts[0];   // copy: Add(Pts[0]) would alias the array's own storage on growth
		Pts.Add(First);
		P.Line(Pts, Color, Thickness);
	}

	/** Keeps the part of a convex polygon above the horizontal line y = LineY (screen y grows downwards). */
	TArray<FVector2f> ClipAbove(const TArray<FVector2f>& Shape, float LineY)
	{
		TArray<FVector2f> Out;
		for (int32 i = 0; i < Shape.Num(); ++i)
		{
			const FVector2f& P0 = Shape[i];
			const FVector2f& P1 = Shape[(i + 1) % Shape.Num()];
			const bool bIn0 = P0.Y <= LineY;
			const bool bIn1 = P1.Y <= LineY;
			if (bIn0)
			{
				Out.Add(P0);
			}
			if (bIn0 != bIn1)
			{
				const float T = (LineY - P0.Y) / (P1.Y - P0.Y);
				Out.Add(P0 + (P1 - P0) * T);
			}
		}
		return Out;
	}

	FVector2f Bezier(const FVector2f& P0, const FVector2f& P1, const FVector2f& P2, float T)
	{
		return FMath::Lerp(FMath::Lerp(P0, P1, T), FMath::Lerp(P1, P2, T), T);
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
			Hills.Add(FVector2f(X, GroundY - 34.0f - 18.0f * FMath::Sin(i * 0.9f) - 10.0f * FMath::Sin(i * 2.3f)));
		}
		for (int32 i = 0; i + 1 < Hills.Num(); ++i)
		{
			P.Quad(Hills[i], Hills[i + 1], FVector2f(Hills[i + 1].X, GroundY), FVector2f(Hills[i].X, GroundY), C(0x8CC66A));
		}
	}

	/** Twinkling stars scattered over a rectangle. */
	void Stars(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, float Time, int32 Count, int32 Seed)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2f At = Pos + FVector2f(Hash01(Seed + i * 3) * Size.X, Hash01(Seed + i * 3 + 1) * Size.Y);
			const float Tw = 0.5f + 0.5f * FMath::Sin(Time * (1.3f + Hash01(Seed + i * 3 + 2) * 2.2f) + float(i));
			P.Circle(At, 0.8f + Hash01(Seed + i * 7) * 1.4f, A(C(0xFFF4D8), 0.35f + 0.6f * Tw));
		}
	}

	/** Painted sea: vertical gradient + drifting wave glints (sparser far away, longer up close). */
	void Sea(FKGMgPainter& P, const FVector2f& Pos, const FVector2f& Size, float Time, const FLinearColor& Top, const FLinearColor& Bottom,
	         const FLinearColor& Glint)
	{
		P.RectV(Pos, Size, Top, Bottom);
		const int32 Rows = FMath::Max(1, FMath::FloorToInt(Size.Y / 20.0f));
		for (int32 r = 0; r < Rows; ++r)
		{
			const float Y = Pos.Y + 6.0f + r * 20.0f;
			const float Depth = float(r + 1) / float(Rows);
			const float Drift = (r % 2 ? 1.0f : -1.0f) * Time * (8.0f + 10.0f * Depth);
			for (int32 k = 0; k < 6; ++k)
			{
				const float Len = 10.0f + 18.0f * Depth + Hash01(r * 17 + k * 5) * 10.0f;
				float X = FMath::Fmod(Hash01(r * 31 + k) * Size.X + Drift, Size.X);
				if (X < 0.0f)
				{
					X += Size.X;
				}
				X += Pos.X;
				if (X + Len < Pos.X + Size.X)
				{
					P.Bar(FVector2f(X, Y), FVector2f(X + Len, Y), 1.5f + Depth, A(Glint, 0.16f + 0.28f * (1.0f - Depth * 0.5f)));
				}
			}
		}
	}

	/** Warm flame: glow, outer tongue, inner core. Lean -1..1 bends the tip sideways. */
	void Flame(FKGMgPainter& P, const FVector2f& Base, float Size, float Lean, float Time, float GlowScale = 1.0f)
	{
		const float Flick = FMath::Sin(Time * 23.0f) * 0.08f + FMath::Sin(Time * 37.0f) * 0.05f;
		P.Glow(Base + FVector2f(0.0f, -Size * 0.6f), Size * 4.5f * GlowScale, A(C(0xFFB050), 0.42f));
		const FVector2f Tip = Base + FVector2f(Lean * Size * 0.9f, -Size * (1.9f + Flick));
		P.Circle(Base, Size * 0.55f, Fire);
		P.Tri(Base + FVector2f(-Size * 0.55f, 0.0f), Base + FVector2f(Size * 0.55f, 0.0f), Tip, Fire);
		const FVector2f Tip2 = Base + FVector2f(Lean * Size * 0.5f, -Size * (1.15f + Flick));
		P.Circle(Base + FVector2f(0.0f, Size * 0.08f), Size * 0.34f, Gold);
		P.Tri(Base + FVector2f(-Size * 0.34f, Size * 0.08f), Base + FVector2f(Size * 0.34f, Size * 0.08f), Tip2, Gold);
		P.Circle(Base + FVector2f(0.0f, Size * 0.22f), Size * 0.17f, A(C(0x8FC0FF), 0.85f));
	}

	// ---- fish (UnloadFish) --------------------------------------------------------------------------------------------

	FLinearColor FishColor(int32 Kind)
	{
		switch (Kind)
		{
		case 0: return C(0x7F95AE);   // cod: grey-blue
		case 1: return C(0x2F9A78);   // mackerel: striped green
		case 2: return C(0xE0503C);   // red snapper
		default: return C(0x6E7A2E);  // eel: olive
		}
	}

	const TCHAR* FishName(int32 Kind)
	{
		switch (Kind)
		{
		case 0: return TEXT("COD");
		case 1: return TEXT("MACKEREL");
		case 2: return TEXT("SNAPPER");
		default: return TEXT("EEL");
		}
	}

	/** A fish seen from the side, head towards +X of angle Ang. */
	void Fish(FKGMgPainter& P, int32 Kind, const FVector2f& At, float Ang, float Scale, float Wiggle)
	{
		auto L = [&](float X, float Y) { return At + Rot(FVector2f(X, Y) * Scale, Ang); };
		const FLinearColor Body = FishColor(Kind);
		const FLinearColor Dark = Mix(Body, Ink, 0.45f);
		if (Kind == 3)
		{
			// Eel: a long sinuous body built from overlapping round bars.
			auto Spine = [&](int32 i, float Off) { return L(24.0f - i * 6.5f, FMath::Sin(i * 0.75f + Wiggle) * (0.5f + i * 0.5f) + Off); };
			FVector2f Prev = Spine(0, 0.0f);
			for (int32 i = 1; i <= 11; ++i)
			{
				const FVector2f Cur = Spine(i, 0.0f);
				const float Thick = (10.0f - i * 0.55f) * Scale;
				P.Bar(Prev, Cur, Thick, Body);
				P.Circle(Cur, Thick * 0.5f, Body);
				Prev = Cur;
			}
			TArray<FVector2f> Fin;
			TArray<FVector2f> Belly;
			for (int32 i = 1; i <= 11; ++i)
			{
				Fin.Add(Spine(i, -4.2f + i * 0.15f));
				Belly.Add(Spine(i, 2.8f - i * 0.12f));
			}
			P.Line(Fin, Dark, 2.2f * Scale);
			P.Line(Belly, A(C(0xD8D27A), 0.8f), 1.6f * Scale);
			P.Poly(Ellipse(L(25.0f, 0.0f), FVector2f(8.5f, 5.5f) * Scale, Ang, 14), Body);
			P.Circle(L(28.0f, -1.6f), 2.0f * Scale, Cream);
			P.Circle(L(28.6f, -1.6f), 1.1f * Scale, Ink);
			P.Segment(L(32.0f, 1.6f), L(27.0f, 2.6f), Dark, 1.2f * Scale);
			return;
		}
		const float Len = Kind == 1 ? 31.0f : Kind == 2 ? 27.0f : 29.0f;
		const float Dep = Kind == 1 ? 8.5f : Kind == 2 ? 13.5f : 11.0f;
		const float Tail = FMath::Sin(Wiggle) * 3.5f;
		const FLinearColor FinC = Mix(Body, Ink, 0.25f);
		// Forked tail.
		P.Tri(L(-Len + 5.0f, 0.0f), L(-Len - 11.0f, -Dep * 0.95f + Tail), L(-Len - 5.0f, Tail * 0.6f), FinC);
		P.Tri(L(-Len + 5.0f, 0.0f), L(-Len - 5.0f, Tail * 0.6f), L(-Len - 11.0f, Dep * 0.95f + Tail), FinC);
		// Fins.
		if (Kind == 2)
		{
			P.Tri(L(-15.0f, -Dep + 2.0f), L(10.0f, -Dep + 2.0f), L(-6.0f, -Dep - 8.0f), FinC);
			P.Tri(L(-2.0f, -Dep + 2.0f), L(12.0f, -Dep + 3.0f), L(4.0f, -Dep - 6.0f), FinC);
		}
		else
		{
			P.Tri(L(-8.0f, -Dep + 2.0f), L(8.0f, -Dep + 2.0f), L(-4.0f, -Dep - 6.0f), FinC);
			P.Tri(L(-22.0f, -Dep + 3.0f), L(-11.0f, -Dep + 2.0f), L(-18.0f, -Dep - 4.0f), FinC);
		}
		P.Tri(L(-13.0f, Dep - 2.0f), L(0.0f, Dep - 2.0f), L(-10.0f, Dep + 5.0f), FinC);
		// Body + pale belly.
		P.Poly(Ellipse(At, FVector2f(Len, Dep) * Scale, Ang, 22), Body);
		TArray<FVector2f> BellyPts;
		for (int32 i = 0; i <= 10; ++i)
		{
			const float T = PI * (0.08f + 0.84f * float(i) / 10.0f);
			BellyPts.Add(L(FMath::Cos(T) * Len * 0.9f, FMath::Sin(T) * Dep * 0.78f));
		}
		P.Poly(BellyPts, Kind == 2 ? C(0xF6A48C) : C(0xE4E8EA));
		if (Kind == 1)
		{
			for (int32 s = 0; s < 6; ++s)
			{
				const float X = -Len * 0.62f + s * 8.0f;
				P.Line({L(X, -Dep * 0.85f), L(X + 3.0f, -Dep * 0.45f), L(X, -Dep * 0.1f)}, Dark, 2.0f * Scale);
			}
		}
		else if (Kind == 0)
		{
			P.Line({L(-Len * 0.8f, -1.0f), L(0.0f, -2.5f), L(Len * 0.55f, -1.5f)}, A(Cream, 0.75f), 1.5f * Scale);
			for (int32 s = 0; s < 5; ++s)
			{
				P.Circle(L(-16.0f + s * 7.0f, -Dep * 0.55f + (s % 2) * 2.0f), 1.3f * Scale, A(Dark, 0.8f));
			}
			P.Segment(L(Len * 0.8f, Dep * 0.45f), L(Len * 0.74f, Dep + 4.0f), Dark, 1.5f * Scale);
		}
		else
		{
			P.Line({L(-Len * 0.7f, -Dep * 0.35f), L(0.0f, -Dep * 0.55f), L(Len * 0.5f, -Dep * 0.4f)}, A(C(0xFFC0A8), 0.7f), 2.0f * Scale);
		}
		// Gill + eye.
		P.Arc(L(Len * 0.62f, 0.0f), Dep * 0.72f * Scale, Ang + PI * 0.65f, Ang + PI * 1.35f, A(Ink, 0.35f), 1.5f, 10);
		P.Circle(L(Len * 0.66f, -Dep * 0.22f), 3.4f * Scale, Cream);
		P.Circle(L(Len * 0.69f, -Dep * 0.22f), 1.8f * Scale, Ink);
	}

	// ---- carrots (HarvestCarrots) -------------------------------------------------------------------------------------

	/** Carrot body outline in local space: shoulder dome at y = 0, tip at y = 64. */
	TArray<FVector2f> CarrotBody(const FVector2f& Shoulder, float Scale, float Ang)
	{
		TArray<FVector2f> Pts;
		for (int32 k = 0; k <= 6; ++k)
		{
			const float T = PI + PI * float(k) / 6.0f;
			Pts.Add(FVector2f(FMath::Cos(T) * 13.0f, FMath::Sin(T) * 6.0f));
		}
		Pts.Add(FVector2f(10.0f, 22.0f));
		Pts.Add(FVector2f(5.0f, 48.0f));
		Pts.Add(FVector2f(0.0f, 64.0f));
		Pts.Add(FVector2f(-5.0f, 48.0f));
		Pts.Add(FVector2f(-10.0f, 22.0f));
		for (FVector2f& V : Pts)
		{
			V = Shoulder + Rot(V * Scale, Ang);
		}
		return Pts;
	}

	/** Leafy (or wilted) carrot top growing from Top. */
	void CarrotLeaves(FKGMgPainter& P, const FVector2f& Top, float Scale, float Ang, bool bRotten, float Time, int32 Seed)
	{
		for (int32 k = 0; k < 5; ++k)
		{
			const float Sway = FMath::Sin(Time * 3.0f + k * 1.3f + Seed) * 0.06f;
			if (!bRotten)
			{
				const float La = Ang - 0.5f * PI + (k - 2) * 0.32f + Sway;
				const float Len = (28.0f + (k % 2) * 7.0f) * Scale;
				const FVector2f D = Unit(La);
				const FVector2f N(-D.Y, D.X);
				const FVector2f Tip = Top + D * Len;
				P.Bar(Top, Tip, 3.0f * Scale, C(0x3E8E3A));
				for (int32 j = 1; j <= 3; ++j)
				{
					const FVector2f At = FMath::Lerp(Top, Tip, float(j) / 3.4f);
					P.Poly(Ellipse(At + N * 4.0f * Scale, FVector2f(5.5f, 2.8f) * Scale, La + 0.6f, 10), j % 2 ? Good : Grass);
					P.Poly(Ellipse(At - N * 4.0f * Scale, FVector2f(5.5f, 2.8f) * Scale, La - 0.6f, 10), j % 2 ? Grass : Good);
				}
				P.Circle(Tip, 3.6f * Scale, Good);
			}
			else
			{
				const float Dx = float(k - 2) * 9.0f * Scale;
				const FVector2f S0 = Top;
				const FVector2f S1 = Top + FVector2f(Dx * 0.5f, -14.0f * Scale);
				const FVector2f S2 = Top + FVector2f(Dx * 1.1f, -9.0f * Scale + Sway * 20.0f);
				const FVector2f S3 = Top + FVector2f(Dx * 1.4f, 5.0f * Scale);
				// Sickly grey-violet so a rotten top never blends into the brown soil.
				P.Line({S0, S1, S2, S3}, k % 2 ? C(0xA898B8) : C(0x8A7AA0), 3.5f * Scale);
				P.Poly(Ellipse(S3, FVector2f(5.0f, 3.0f) * Scale, 1.2f, 8), C(0x6E5E88));
			}
		}
	}

	/** A whole carrot (flying or in the basket). */
	void Carrot(FKGMgPainter& P, const FVector2f& Shoulder, float Scale, float Ang, float Time, int32 Seed)
	{
		P.Poly(CarrotBody(Shoulder, Scale, Ang), C(0xF28C28));
		P.Bar(Shoulder + Rot(FVector2f(-6.0f, 2.0f) * Scale, Ang), Shoulder + Rot(FVector2f(-2.0f, 44.0f) * Scale, Ang), 3.0f * Scale,
		      A(C(0xFFC680), 0.7f));
		for (int32 r = 0; r < 4; ++r)
		{
			const float Y = 10.0f + r * 11.0f;
			const float Hw = 10.0f - r * 2.2f;
			P.Segment(Shoulder + Rot(FVector2f(Hw * 0.2f, Y) * Scale, Ang), Shoulder + Rot(FVector2f(Hw, Y + 1.5f) * Scale, Ang), A(C(0xB85A16), 0.8f),
			          1.5f);
		}
		CarrotLeaves(P, Shoulder + Rot(FVector2f(0.0f, -5.0f) * Scale, Ang), Scale, Ang, false, Time, Seed);
	}
}

// =====================================================================================================================
// UnloadFish: fish slide down the gangplank one by one; drag each into the crate with its picture before it splashes.
// =====================================================================================================================
class FKGMgUnloadFish final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Drag each fish into the crate with its picture before it slides into the sea");
	}

	virtual void BeginStage() override
	{
		Fishes.Reset();
		Queue.Reset();
		for (int32 i = 0; i < Total; ++i)
		{
			Queue.Add(i % 4);
		}
		Rng.Shuffle(Queue);
		CrateKind = {0, 1, 2, 3};
		Rng.Shuffle(CrateKind);
		for (int32 s = 0; s < 4; ++s)
		{
			CrateCount[s] = 0;
			CrateBump[s] = 0.0f;
		}
		Splashes.Reset();
		SpawnT = 0.5f;
		Sorted = 0;
		HeldId = -1;
		NextId = 0;
		AutoT = 0.8f;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		for (int32 i = Fishes.Num() - 1; i >= 0; --i)
		{
			FFish& F = Fishes[i];
			if ((F.State == EFish::Slide || F.State == EFish::Back) && FVector2f::Distance(Pos, F.Pos) < 38.0f)
			{
				F.State = EFish::Held;
				HeldId = F.Id;
				GrabOff = F.Pos - Pos;
				Sound(TEXT("S_Chore_Squish"), 0.5f, 1.2f);
				return;
			}
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		FFish* F = Find(HeldId);
		HeldId = -1;
		if (!F)
		{
			return;
		}
		const int32 Slot = SlotAt(Pos);
		F->From = F->Pos;
		F->Age = 0.0f;
		if (Slot >= 0 && CrateKind[Slot] == F->Kind)
		{
			Deliver(*F, Slot);
		}
		else
		{
			F->State = EFish::Back;
			if (Slot >= 0)
			{
				Oops(CrateTop(Slot) + FVector2f(0.0f, -50.0f), TEXT("Wrong crate!"), TEXT("S_UI_Bad"), 4.0f);
			}
		}
	}

	virtual void Tick(float Dt) override
	{
		for (float& B : CrateBump)
		{
			B = FMath::Max(0.0f, B - Dt * 3.5f);
		}
		for (int32 i = Splashes.Num() - 1; i >= 0; --i)
		{
			Splashes[i].Age += Dt;
			if (Splashes[i].Age > 1.0f)
			{
				Splashes.RemoveAt(i);
			}
		}
		if (!bSolved)
		{
			SpawnT -= Dt;
			if (SpawnT <= 0.0f && Queue.Num() > 0 && !PlankBusy())
			{
				FFish& F = Fishes.AddDefaulted_GetRef();
				F.Id = NextId++;
				F.Kind = Queue[0];
				Queue.RemoveAt(0);
				F.Pos = OnPlank(0.0f);
				F.Phase = Rng.FRand() * 6.28f;
				SpawnT = Interval;
				Sound(TEXT("S_Chore_Squish"), 0.3f, 0.8f + Rng.FRand() * 0.3f);
			}
		}
		const FVector2f D = PlankDir();
		for (int32 i = Fishes.Num() - 1; i >= 0; --i)
		{
			FFish& F = Fishes[i];
			F.Age += Dt;
			bool bRemove = false;
			if (F.State == EFish::Slide)
			{
				if (!bSolved)
				{
					F.T += Dt / SlideTime * (0.8f + 0.4f * F.T);   // picks up speed down the slope
				}
				F.Pos = OnPlank(F.T);
				if (F.T >= 1.0f)
				{
					F.State = EFish::Fall;
					F.Vel = D * 30.0f + FVector2f(0.0f, -50.0f);
					F.Age = 0.0f;
					F.Phase = 0.0f;
				}
			}
			else if (F.State == EFish::Held)
			{
				F.Pos = Mouse + GrabOff;
				GrabOff = GrabOff * FMath::Max(0.0f, 1.0f - Dt * 6.0f);   // ease the fish under the cursor
			}
			else if (F.State == EFish::Back)
			{
				const float K = Saturate(F.Age / 0.28f);
				F.Pos = FMath::Lerp(F.From, OnPlank(F.T), EaseOutCubic(K));
				if (K >= 1.0f)
				{
					F.State = EFish::Slide;
				}
			}
			else if (F.State == EFish::ToCrate)
			{
				const float K = Saturate(F.Age / 0.3f);
				const FVector2f To = CrateTop(F.Slot) + FVector2f(0.0f, -6.0f);
				F.Pos = FMath::Lerp(F.From, To, EaseOutCubic(K)) + FVector2f(0.0f, -40.0f * FMath::Sin(K * PI));
				if (K >= 1.0f)
				{
					++CrateCount[F.Slot];
					CrateBump[F.Slot] = 1.0f;
					Sound(TEXT("S_Chore_Thud"), 0.6f, 0.9f + 0.1f * CrateCount[F.Slot]);
					bRemove = true;
				}
			}
			else
			{
				F.Vel.Y += 900.0f * Dt;
				F.Pos += F.Vel * Dt;
				F.Phase += Dt * 7.0f;
				if (F.Pos.Y > SeaSplashY)
				{
					Splashes.Add({FVector2f(FMath::Min(F.Pos.X, 596.0f), SeaSplashY), 0.0f});
					Queue.Add(F.Kind);
					Oops(FVector2f(468.0f, 222.0f), TEXT("Splash! It'll come round again"), TEXT("S_Splash"), 5.0f);
					bRemove = true;
				}
			}
			if (bRemove)
			{
				Fishes.RemoveAt(i);
			}
		}
		if (!bSolved && Sorted >= Total && Fishes.Num() == 0 && StageTime >= MinSolve)
		{
			Solve();
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		AutoT -= Dt;
		if (AutoT > 0.0f)
		{
			return;
		}
		AutoT = 0.7f;
		FFish* Best = nullptr;
		for (FFish& F : Fishes)
		{
			if (F.State == EFish::Slide && F.T > 0.2f && (!Best || F.T > Best->T))
			{
				Best = &F;
			}
		}
		if (Best)
		{
			Best->From = Best->Pos;
			Best->Age = 0.0f;
			Deliver(*Best, CrateKind.IndexOfByKey(Best->Kind));
		}
	}

	virtual void DebugPose() override
	{
		Fishes.Reset();
		Splashes.Reset();
		Sorted = 7;
		const int32 Counts[4] = {2, 2, 2, 1};
		for (int32 s = 0; s < 4; ++s)
		{
			CrateCount[s] = Counts[s];
		}
		auto Add = [this](int32 Kind, float T, EFish State)
		{
			FFish& F = Fishes.AddDefaulted_GetRef();
			F.Id = NextId++;
			F.Kind = Kind;
			F.T = T;
			F.State = State;
			F.Pos = OnPlank(T);
			F.Phase = T * 9.0f;
			return F.Id;
		};
		Add(CrateKind[3], 0.14f, EFish::Slide);
		Add(CrateKind[0], 0.62f, EFish::Slide);
		HeldId = Add(CrateKind[1], 0.4f, EFish::Held);
		Mouse = CrateTop(1) + FVector2f(34.0f, -64.0f);
		GrabOff = FVector2f::ZeroVector;
		if (FFish* F = Find(HeldId))
		{
			F->Pos = Mouse;
		}
		Splashes.Add({FVector2f(596.0f, SeaSplashY), 0.35f});
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintBackdrop(P);
		PaintPlank(P);
		for (int32 s = 0; s < 4; ++s)
		{
			PaintCrate(P, s);
		}

		// Fish: on the plank, flying, falling - the held one last, on top.
		const FVector2f D = PlankDir();
		const float PlankAng = FMath::Atan2(D.Y, D.X);
		const FFish* Held = nullptr;
		for (const FFish& F : Fishes)
		{
			if (F.State == EFish::Held)
			{
				Held = &F;
				continue;
			}
			if (F.State == EFish::Slide || F.State == EFish::Back)
			{
				P.Poly(KGMgCoast::Ellipse(F.Pos + FVector2f(0.0f, 10.0f), FVector2f(30.0f, 6.0f), PlankAng, 16), A(Ink, 0.25f));
				KGMgCoast::Fish(P, F.Kind, F.Pos, PlankAng + FMath::Sin(Time * 9.0f + F.Phase) * 0.05f, 1.0f, Time * 7.0f + F.Phase);
			}
			else if (F.State == EFish::ToCrate)
			{
				KGMgCoast::Fish(P, F.Kind, F.Pos, FMath::Lerp(PlankAng, -0.5f * PI, Saturate(F.Age / 0.3f)), FMath::Lerp(1.0f, 0.55f, Saturate(F.Age / 0.3f)),
				                Time * 12.0f);
			}
			else
			{
				KGMgCoast::Fish(P, F.Kind, F.Pos, PlankAng + F.Phase, 1.0f, Time * 14.0f);
			}
		}
		for (const FSplash& S : Splashes)
		{
			const float K = S.Age;
			KGMgCoast::Ring(P, S.At, FVector2f(12.0f + 28.0f * K, 4.0f + 10.0f * K), A(C(0xE6FAFF), 0.9f * (1.0f - K)), 3.0f, 28);
			for (int32 d = 0; d < 6; ++d)
			{
				const float Vx = (float(d) - 2.5f) * 24.0f;
				const FVector2f Drop = S.At + FVector2f(Vx * K, -150.0f * K + 330.0f * K * K);
				if (Drop.Y < S.At.Y + 2.0f)
				{
					P.Circle(Drop, 3.5f * (1.0f - K * 0.5f), A(C(0xE6FAFF), 1.0f - K));
				}
			}
		}
		if (Held)
		{
			P.Poly(KGMgCoast::Ellipse(Held->Pos + FVector2f(8.0f, 18.0f), FVector2f(32.0f, 8.0f), 0.0f, 16), A(Ink, 0.3f));
			KGMgCoast::Fish(P, Held->Kind, Held->Pos, FMath::Sin(Time * 10.0f) * 0.12f, 1.12f, Time * 16.0f);
		}

		// Hints: the first fish shows where it goes.
		if (Sorted == 0 && !Held)
		{
			for (const FFish& F : Fishes)
			{
				if (F.State == EFish::Slide)
				{
					P.HintRing(F.Pos, 34.0f, Time, Gold);
					const int32 Slot = CrateKind.IndexOfByKey(F.Kind);
					P.HintArrow(F.Pos + FVector2f(0.0f, 28.0f), CrateTop(Slot) + FVector2f(0.0f, -30.0f), Time, A(Gold, 0.9f));
					break;
				}
			}
		}

		// Counter.
		P.RoundRect(FVector2f(506.0f, 8.0f), FVector2f(124.0f, 52.0f), A(Ink, 0.72f), 10.0f);
		P.Text(FVector2f(568.0f, 12.0f), TEXT("SORTED"), 10.0f, CreamDim, 0.5f, TEXT("Bold"), 100);
		P.Text(FVector2f(568.0f, 25.0f), FString::Printf(TEXT("%d / %d"), Sorted, Total), 22.0f, Sorted >= Total ? Good : Cream, 0.5f,
		       TEXT("Black"));
	}

private:
	enum class EFish : uint8
	{
		Slide,
		Held,
		Back,
		ToCrate,
		Fall
	};

	struct FFish
	{
		int32 Id = 0;
		int32 Kind = 0;
		EFish State = EFish::Slide;
		float T = 0.0f;
		FVector2f Pos = FVector2f::ZeroVector;
		FVector2f From = FVector2f::ZeroVector;
		FVector2f Vel = FVector2f::ZeroVector;
		float Age = 0.0f;
		float Phase = 0.0f;
		int32 Slot = -1;
	};

	struct FSplash
	{
		FVector2f At;
		float Age;
	};

	static constexpr int32 Total = 12;
	static constexpr float SlideTime = 3.4f;   // seconds from the boat to the plank's end
	static constexpr float Interval = 1.25f;   // one fish every Interval: 12 fish can't be sorted in under ~14 s
	static constexpr float MinSolve = 8.2f;
	static constexpr float SeaSplashY = 262.0f;
	const FVector2f PlankA = FVector2f(156.0f, 94.0f);
	const FVector2f PlankB = FVector2f(590.0f, 196.0f);

	TArray<FFish> Fishes;
	TArray<int32> Queue;
	TArray<int32> CrateKind;
	int32 CrateCount[4] = {0, 0, 0, 0};
	float CrateBump[4] = {0, 0, 0, 0};
	TArray<FSplash> Splashes;
	float SpawnT = 0.0f;
	int32 Sorted = 0;
	int32 HeldId = -1;
	int32 NextId = 0;
	FVector2f GrabOff = FVector2f::ZeroVector;
	float AutoT = 0.0f;

	FVector2f PlankDir() const { return (PlankB - PlankA).GetSafeNormal(); }

	FVector2f OnPlank(float T) const
	{
		const FVector2f D = PlankDir();
		return FMath::Lerp(PlankA, PlankB, FMath::Min(T, 1.0f)) + FVector2f(D.Y, -D.X) * 10.0f;
	}

	bool PlankBusy() const
	{
		for (const FFish& F : Fishes)
		{
			if ((F.State == EFish::Slide || F.State == EFish::Back) && F.T < 0.2f)
			{
				return true;
			}
		}
		return false;
	}

	FFish* Find(int32 Id)
	{
		if (Id < 0)
		{
			return nullptr;
		}
		return Fishes.FindByPredicate([Id](const FFish& F) { return F.Id == Id; });
	}

	FVector2f CrateTop(int32 Slot) const { return FVector2f(84.0f + Slot * 124.0f, 292.0f); }

	int32 SlotAt(const FVector2f& Pos) const
	{
		for (int32 s = 0; s < 4; ++s)
		{
			if (InRect(Pos, CrateTop(s) + FVector2f(-62.0f, -60.0f), FVector2f(124.0f, 150.0f)))
			{
				return s;
			}
		}
		return -1;
	}

	void Deliver(FFish& F, int32 Slot)
	{
		F.State = EFish::ToCrate;
		F.Slot = Slot;
		F.From = F.Pos;
		F.Age = 0.0f;
		++Sorted;
		Nice(CrateTop(Slot) + FVector2f(0.0f, -54.0f), KGMgCoast::FishName(F.Kind), TEXT("S_Chore_Squish"), 0.85f + 0.04f * Sorted);
	}

	void PaintBackdrop(FKGMgPainter& P) const
	{
		// Sky, the far headland with its lighthouse, gulls.
		P.RectV(FVector2f(0, 0), FVector2f(W, 80.0f), C(0x86CDEF), C(0xF3E9CF));
		P.Glow(FVector2f(250.0f, 36.0f), 90.0f, A(C(0xFFF0B0), 0.5f));
		P.Circle(FVector2f(250.0f, 36.0f), 17.0f, C(0xFFF4C8));
		P.Quad(FVector2f(340.0f, 80.0f), FVector2f(600.0f, 80.0f), FVector2f(560.0f, 60.0f), FVector2f(410.0f, 54.0f), C(0x7FAE96));
		P.Quad(FVector2f(380.0f, 80.0f), FVector2f(520.0f, 80.0f), FVector2f(500.0f, 66.0f), FVector2f(430.0f, 64.0f), C(0x6E9C8C));
		P.Quad(FVector2f(462.0f, 58.0f), FVector2f(478.0f, 58.0f), FVector2f(475.0f, 24.0f), FVector2f(465.0f, 24.0f), Cream);
		P.Rect(FVector2f(464.0f, 38.0f), FVector2f(12.0f, 6.0f), Crimson);
		P.Rect(FVector2f(464.0f, 16.0f), FVector2f(12.0f, 8.0f), C(0x2A2230));
		P.Glow(FVector2f(470.0f, 20.0f), 18.0f, A(Gold, 0.8f));
		P.Tri(FVector2f(462.0f, 16.0f), FVector2f(478.0f, 16.0f), FVector2f(470.0f, 8.0f), Crimson);
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = FMath::Fmod(100.0f + i * 170.0f + Time * (16.0f + i * 3.0f), 700.0f) - 30.0f;
			const float Y = 22.0f + i * 10.0f + FMath::Sin(Time * 1.7f + i) * 3.0f;
			const float Flap = FMath::Sin(Time * 6.0f + i * 2.0f) * 3.0f;
			P.Line({FVector2f(X - 10.0f, Y - 3.0f - Flap), FVector2f(X - 5.0f, Y), FVector2f(X, Y - 2.0f), FVector2f(X + 5.0f, Y),
			        FVector2f(X + 10.0f, Y - 3.0f - Flap)},
			       A(Ink, 0.7f), 2.0f);
		}
		KGMgCoast::Sea(P, FVector2f(0.0f, 80.0f), FVector2f(W, H - 80.0f), Time, C(0x5CC0E0), C(0x1D5C8C), C(0xE6FAFF));

		// The fishing boat moored on the left.
		const float Bob = FMath::Sin(Time * 1.3f) * 1.5f;
		P.Disc(FVector2f(80.0f, 206.0f), FVector2f(130.0f, 14.0f), A(Ink, 0.3f), A(Ink, 0.0f));
		P.Bar(FVector2f(62.0f, 100.0f + Bob), FVector2f(62.0f, -10.0f), 7.0f, WoodDark);
		P.Bar(FVector2f(62.0f, 30.0f + Bob), FVector2f(146.0f, 44.0f + Bob), 5.0f, Wood);
		P.RoundRect(FVector2f(64.0f, 24.0f + Bob), FVector2f(80.0f, 14.0f), C(0xE8DCC0), 6.0f);
		P.Quad(FVector2f(-20.0f, 100.0f + Bob), FVector2f(184.0f, 94.0f + Bob), FVector2f(160.0f, 196.0f + Bob), FVector2f(-20.0f, 200.0f + Bob),
		       C(0x2F6DA8));
		P.Quad(FVector2f(-20.0f, 112.0f + Bob), FVector2f(180.0f, 107.0f + Bob), FVector2f(178.0f, 117.0f + Bob), FVector2f(-20.0f, 122.0f + Bob),
		       C(0xEFE6D2));
		P.Quad(FVector2f(-20.0f, 182.0f + Bob), FVector2f(164.0f, 178.0f + Bob), FVector2f(160.0f, 196.0f + Bob), FVector2f(-20.0f, 200.0f + Bob),
		       C(0xB5483A));
		P.Bar(FVector2f(-20.0f, 98.0f + Bob), FVector2f(186.0f, 92.0f + Bob), 8.0f, WoodDark);
		P.Text(FVector2f(80.0f, 136.0f + Bob), TEXT("MORROW GULL"), 11.0f, A(Cream, 0.9f), 0.5f, TEXT("Black"), 60);
		// Baskets of catch on deck.
		for (int32 b = 0; b < 2; ++b)
		{
			const FVector2f Bk(8.0f + b * 64.0f, 76.0f + Bob);
			KGMgCoast::Fish(P, b * 2, Bk + FVector2f(24.0f, 2.0f), -0.4f, 0.55f, 0.0f);
			KGMgCoast::Fish(P, b * 2 + 1, Bk + FVector2f(34.0f, 4.0f), -2.6f, 0.55f, 1.0f);
			P.RoundRect(Bk, FVector2f(56.0f, 20.0f), C(0xC8964E), 5.0f);
			P.Rect(Bk + FVector2f(0.0f, 7.0f), FVector2f(56.0f, 2.0f), A(Ink, 0.3f));
		}

		// Trestle holding the plank (in the water, behind the dock).
		const FVector2f Mid = FMath::Lerp(PlankA, PlankB, 0.52f);
		P.Bar(Mid, FVector2f(Mid.X - 22.0f, 250.0f), 8.0f, C(0x4A2F1C));
		P.Bar(Mid, FVector2f(Mid.X + 22.0f, 250.0f), 8.0f, C(0x4A2F1C));
		P.Bar(FVector2f(Mid.X - 14.0f, 200.0f), FVector2f(Mid.X + 14.0f, 200.0f), 5.0f, C(0x4A2F1C));

		// The dock the crates stand on; open water on its right.
		const float DockY = 238.0f;
		const float DockR = 540.0f;
		for (int32 i = 0; i < 3; ++i)
		{
			P.Rect(FVector2f(DockR + 10.0f, DockY + 20.0f + i * 60.0f), FVector2f(14.0f, 40.0f), C(0x4A2F1C));
			KGMgCoast::Ring(P, FVector2f(DockR + 17.0f, DockY + 60.0f + i * 60.0f), FVector2f(14.0f, 4.0f), A(C(0xE6FAFF), 0.5f), 1.5f, 16);
		}
		P.RectV(FVector2f(0.0f, DockY), FVector2f(DockR, H - DockY), C(0xC49466), C(0x9A6B44));
		for (int32 i = 0; i < 9; ++i)
		{
			const float Y = DockY + 4.0f + i * 18.0f;
			P.Rect(FVector2f(0.0f, Y), FVector2f(DockR, 2.0f), A(C(0x5C3A26), 0.45f));
			const float Jx = FMath::Fmod(i * 97.0f + 40.0f, 400.0f) + 40.0f;
			P.Rect(FVector2f(Jx, Y + 2.0f), FVector2f(2.0f, 16.0f), A(C(0x5C3A26), 0.4f));
		}
		P.Rect(FVector2f(0.0f, DockY - 6.0f), FVector2f(DockR, 8.0f), WoodDark);
		P.Rect(FVector2f(DockR, DockY - 6.0f), FVector2f(12.0f, H - DockY + 6.0f), WoodDark);
		P.Rect(FVector2f(0.0f, DockY - 6.0f), FVector2f(DockR + 12.0f, 2.0f), A(WoodLight, 0.6f));
		// Bollard with a rope.
		P.Circle(FVector2f(514.0f, 258.0f), 11.0f, Iron, C(0x2E3138), 2.0f);
		P.Arc(FVector2f(514.0f, 258.0f), 15.0f, 0.2f, 2.9f, C(0xD8B98A), 3.0f, 12);
	}

	void PaintPlank(FKGMgPainter& P) const
	{
		const FVector2f D = PlankDir();
		const FVector2f N(-D.Y, D.X);   // points down-left, across the plank
		P.Bar(PlankA + FVector2f(0.0f, 34.0f), PlankB + FVector2f(0.0f, 34.0f), 24.0f, A(Ink, 0.16f));
		P.Bar(PlankA, PlankB, 38.0f, WoodDark);
		P.Bar(PlankA - N * 2.0f, PlankB - N * 2.0f, 30.0f, C(0xC9975F));
		for (int32 c = 1; c < 9; ++c)
		{
			const FVector2f Q = FMath::Lerp(PlankA, PlankB, c / 9.0f) - N * 2.0f;
			P.Bar(Q - N * 14.0f, Q + N * 14.0f, 4.0f, A(C(0x8A5A3B), 0.8f));
		}
		P.Bar(PlankA - N * 12.0f, PlankB - N * 12.0f, 2.5f, A(Cream, 0.4f));
		// Hazard stripes at the drop.
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector2f Q = FMath::Lerp(PlankA, PlankB, 0.93f + s * 0.025f) - N * 2.0f;
			P.Bar(Q - N * 14.0f, Q + N * 14.0f, 6.0f, s % 2 ? Cream : Crimson);
		}
		bool bDanger = false;
		for (const FFish& F : Fishes)
		{
			bDanger |= F.State == EFish::Slide && F.T > 0.72f;
		}
		if (bDanger)
		{
			P.Glow(PlankB + FVector2f(10.0f, 40.0f), 50.0f, A(Crimson, 0.35f + 0.25f * Ping(Time, 0.5f)));
		}
	}

	void PaintCrate(FKGMgPainter& P, int32 Slot) const
	{
		const FVector2f Top = CrateTop(Slot) + FVector2f(0.0f, -CrateBump[Slot] * 6.0f);
		const int32 Kind = CrateKind.IsValidIndex(Slot) ? CrateKind[Slot] : 0;
		const FLinearColor Col = KGMgCoast::FishColor(Kind);
		const bool bHover = HeldId >= 0 && SlotAt(Mouse) == Slot;
		P.Shadow(Top + FVector2f(-54.0f, -8.0f), FVector2f(108.0f, 84.0f), 6.0f, 0.4f, 6.0f);
		if (bHover)
		{
			P.Glow(Top + FVector2f(0.0f, 30.0f), 100.0f, A(Cream, 0.45f));
		}
		// Open top + the catch already inside.
		P.Quad(Top + FVector2f(-54.0f, 0.0f), Top + FVector2f(54.0f, 0.0f), Top + FVector2f(48.0f, -12.0f), Top + FVector2f(-48.0f, -12.0f), C(0x3B2416));
		for (int32 n = 0; n < CrateCount[Slot]; ++n)
		{
			const float Hx = KGMgCoast::Hash01(Slot * 97 + n * 13);
			const float Row = float(n / 4);
			const FVector2f At = Top + FVector2f(-33.0f + (n % 4) * 22.0f + Hx * 6.0f, -2.0f - Row * 7.0f);
			KGMgCoast::Fish(P, Kind, At, -0.5f * PI + (Hx - 0.5f) * 0.9f, 0.55f, Hx * 6.0f);
		}
		// Front boards.
		P.RectV(Top + FVector2f(-54.0f, 0.0f), FVector2f(108.0f, 76.0f), C(0xB98256), C(0x8A5A3B));
		for (int32 k = 1; k <= 2; ++k)
		{
			P.Rect(Top + FVector2f(-54.0f, k * 25.0f - 1.0f), FVector2f(108.0f, 2.0f), A(Ink, 0.35f));
		}
		P.Rect(Top + FVector2f(-54.0f, 0.0f), FVector2f(8.0f, 76.0f), WoodDark);
		P.Rect(Top + FVector2f(46.0f, 0.0f), FVector2f(8.0f, 76.0f), WoodDark);
		P.Rect(Top + FVector2f(-54.0f, 0.0f), FVector2f(108.0f, 3.0f), A(Cream, 0.35f));
		// Label: species picture + name.
		P.RoundRect(Top + FVector2f(-40.0f, 10.0f), FVector2f(80.0f, 38.0f), Paper, 6.0f, Col, 3.0f);
		KGMgCoast::Fish(P, Kind, Top + FVector2f(Kind == 3 ? 6.0f : 3.0f, 29.0f), 0.0f, 0.6f, 0.0f);
		P.Text(Top + FVector2f(0.0f, 53.0f), KGMgCoast::FishName(Kind), 11.0f, Cream, 0.5f, TEXT("Black"), 60);
		if (bHover)
		{
			P.RoundRect(Top + FVector2f(-60.0f, -18.0f), FVector2f(120.0f, 100.0f), FLinearColor::Transparent, 10.0f, A(Cream, 0.9f), 3.0f);
		}
	}
};

// =====================================================================================================================
// FuelLighthouse: pour oil into the swaying funnel up to the line, then trim both charred wicks with a steady hand.
// =====================================================================================================================
class FKGMgFuelLighthouse final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Hold to pour - follow the swaying funnel and let go at the green line")
		                  : TEXT("Press the scissors on START and drag slowly along the guide");
	}

	virtual void BeginStage() override
	{
		CanX = 200.0f;
		Tilt = 0.0f;
		FlowNow = 0.0f;
		OilLevel = 0.0f;
		CanOil = CanCap;
		RefillT = -1.0f;
		MissT = 0.0f;
		MissCD = 0.0f;
		SettleT = 0.0f;
		PourSndT = 0.0f;
		bLineSaid = false;
		bAutoPour = false;
		Puddles.Reset();
		Ph0 = Rng.FRand() * 6.28f;
		Ph1 = Rng.FRand() * 6.28f;
		Wick = 0;
		LitT = -1.0f;
		NewWick();
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 1 && FallT < 0.0f && Wick < 2 && FVector2f::Distance(Pos, CutPoint(CutProg)) < 36.0f)
		{
			bCutting = true;
			bActive = false;
			LastPos = Pos;
			FrameDist = 0.0f;
			SpeedSm = 0.0f;
			Sound(TEXT("S_UI_Click"), 0.5f, 1.3f);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { bCutting = false; }

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage == 1 && bCutting)
		{
			FrameDist += FVector2f::Distance(Pos, LastPos);
			LastPos = Pos;
		}
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickOil(Dt);
		}
		else
		{
			TickWick(Dt);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			Mouse.X = FunnelX();
			bAutoPour = OilLevel < Band0 + 0.04f;
		}
		else if (FallT < 0.0f && Wick < 2)
		{
			bCutting = true;
			const float Len = FVector2f::Distance(StartPt(), EndPt());
			Mouse = CutPoint(FMath::Min(1.0f, CutProg + Cap * 0.8f * Dt / Len));
			LastPos = Mouse;
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			OilLevel = 0.58f;
			Tilt = 1.0f;
			FlowNow = PourRate;
			Mouse.X = FunnelX();
			CanX = Mouse.X - StreamDx;
			CanOil = 0.72f;
			RefillT = -1.0f;
			Puddles = {FVector2f(170.0f, 0.1f)};
		}
		else
		{
			Wick = 0;
			FallT = -1.0f;
			CutProg = 0.46f;
			bCutting = true;
			bActive = true;
			Mouse = CutPoint(CutProg);
			SpeedSm = Cap * 0.62f;
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintOil(P);
		}
		else
		{
			PaintWick(P);
		}
	}

private:
	// Oil
	static constexpr float PourRate = 0.24f;   // tank fraction per second at full tilt: >= 3.4 s of pouring to the line
	static constexpr float Band0 = 0.82f;
	static constexpr float Band1 = 0.95f;
	static constexpr float CanCap = 1.3f;
	static constexpr float FunnelY = 232.0f;
	static constexpr float FunnelHalf = 30.0f;
	static constexpr float SpoutY = 120.0f;
	static constexpr float StreamDx = 8.0f;
	static constexpr float MinOil = 4.0f;
	// Wick
	static constexpr float Cap = 130.0f;   // px/s speed limit of a clean cut (240 px -> ~1.8 s a wick, hard floor 1.5 s)
	static constexpr float Tol = 16.0f;
	static constexpr float MinWick = 2.6f;

	float CanX = 200.0f;
	float Tilt = 0.0f;
	float FlowNow = 0.0f;
	float OilLevel = 0.0f;
	float CanOil = CanCap;
	float RefillT = -1.0f;
	float MissT = 0.0f;
	float MissCD = 0.0f;
	float SettleT = 0.0f;
	float PourSndT = 0.0f;
	bool bLineSaid = false;
	bool bAutoPour = false;
	float Ph0 = 0.0f;
	float Ph1 = 0.0f;
	TArray<FVector2f> Puddles;   // X = floor position, Y = amount

	int32 Wick = 0;
	float CutProg = 0.0f;
	bool bCutting = false;
	bool bActive = false;
	float SpeedSm = 0.0f;
	float FrameDist = 0.0f;
	FVector2f LastPos = FVector2f::ZeroVector;
	float FallT = -1.0f;
	float BreakT = 0.0f;
	FVector2f BreakAt = FVector2f::ZeroVector;
	float SnipAcc = 0.0f;
	float Slope = 0.0f;
	float LitT = -1.0f;

	float FunnelX() const
	{
		return 320.0f + 72.0f * FMath::Sin(StageTime * 0.85f + Ph0) + 22.0f * FMath::Sin(StageTime * 2.05f + Ph1);
	}

	float StreamX() const { return CanX + StreamDx; }
	bool StreamHits() const { return FMath::Abs(StreamX() - FunnelX()) <= FunnelHalf; }

	void AddPuddle(float X, float Amount)
	{
		for (FVector2f& Pd : Puddles)
		{
			if (FMath::Abs(Pd.X - X) < 36.0f)
			{
				Pd.Y += Amount;
				return;
			}
		}
		if (Puddles.Num() < 10)
		{
			Puddles.Add(FVector2f(X, Amount));
		}
	}

	void TickOil(float Dt)
	{
		const bool bWant = (bMouseDown || bAutoPour) && !bSolved && RefillT < 0.0f;
		bAutoPour = false;
		Tilt = bWant ? FMath::Min(1.0f, Tilt + Dt * 3.5f) : FMath::Max(0.0f, Tilt - Dt * 5.0f);
		if (RefillT >= 0.0f)
		{
			RefillT += Dt;
			if (RefillT >= 1.4f)
			{
				RefillT = -1.0f;
				CanOil = CanCap;
				Sound(TEXT("S_Chore_Pour"), 0.5f, 0.8f);
			}
		}
		CanX = Approach(CanX, FMath::Clamp(Mouse.X, 110.0f, 540.0f) - StreamDx, Dt, 14.0f);
		FlowNow = CanOil > 0.0f && RefillT < 0.0f ? Saturate((Tilt - 0.3f) / 0.7f) * PourRate : 0.0f;
		MissCD = FMath::Max(0.0f, MissCD - Dt);
		if (FlowNow > 0.0f && !bSolved)
		{
			const float Amt = FlowNow * Dt;
			CanOil = FMath::Max(0.0f, CanOil - Amt);
			PourSndT -= Dt;
			if (PourSndT <= 0.0f)
			{
				PourSndT = 0.45f;
				Sound(TEXT("S_Chore_Pour"), 0.45f, 0.85f + 0.4f * OilLevel);
			}
			if (StreamHits())
			{
				OilLevel += Amt;
				MissT = 0.0f;
			}
			else
			{
				MissT += Dt;
				AddPuddle(StreamX(), Amt);
				if (MissT > 0.25f && MissCD <= 0.0f)
				{
					MissCD = 1.8f;
					Oops(FVector2f(StreamX(), 320.0f), TEXT("Missed the funnel!"), TEXT("S_Chore_Splash"), 3.0f);
				}
			}
			if (OilLevel >= Band0 && !bLineSaid)
			{
				bLineSaid = true;
				PopText(FVector2f(320.0f, 250.0f), TEXT("Let go!"), Gold);
			}
			if (OilLevel > Band1)
			{
				OilLevel = 0.5f;
				bLineSaid = false;
				Tilt = 0.0f;
				AddPuddle(FunnelX(), 0.35f);
				Oops(FVector2f(320.0f, 262.0f), TEXT("Overflow! Some drained off"), TEXT("S_Chore_Splash"), 6.0f);
			}
			if (CanOil <= 0.0f && RefillT < 0.0f)
			{
				RefillT = 0.0f;
				Oops(FVector2f(CanX - 40.0f, 90.0f), TEXT("Can's empty - refilling"), TEXT("S_UI_Bad"), 3.0f);
			}
		}
		if (!bSolved && OilLevel >= Band0 && FlowNow <= 0.0f)
		{
			SettleT += Dt;
			if (SettleT >= 0.5f && StageTime >= MinOil)
			{
				Nice(FVector2f(320.0f, 250.0f), TEXT("Filled to the line!"), TEXT("S_Chore_Pour"), 1.2f);
				Solve();
			}
		}
		else
		{
			SettleT = 0.0f;
		}
	}

	// ---- wick ------------------------------------------------------------------------------------------------------

	void NewWick()
	{
		CutProg = 0.0f;
		bCutting = false;
		bActive = false;
		SpeedSm = 0.0f;
		FrameDist = 0.0f;
		FallT = -1.0f;
		BreakT = 0.0f;
		SnipAcc = 0.0f;
		Slope = (Rng.FRand() - 0.5f) * 0.3f;
	}

	float CutY(float X) const { return 200.0f + Slope * (X - 320.0f); }
	FVector2f StartPt() const { return FVector2f(200.0f, CutY(200.0f)); }
	FVector2f EndPt() const { return FVector2f(440.0f, CutY(440.0f)); }
	FVector2f CutPoint(float T) const { return FMath::Lerp(StartPt(), EndPt(), T); }

	void Break(const TCHAR* Why)
	{
		BreakAt = CutPoint(CutProg);
		BreakT = 0.8f;
		Oops(BreakAt + FVector2f(0.0f, -40.0f), Why, TEXT("S_UI_Bad"), 5.0f);
		CutProg = 0.0f;
		bCutting = false;
		bActive = false;
		SpeedSm = 0.0f;
	}

	void TickWick(float Dt)
	{
		BreakT = FMath::Max(0.0f, BreakT - Dt);
		if (FallT >= 0.0f)
		{
			FallT += Dt;
			if (FallT >= 0.9f)
			{
				++Wick;
				if (Wick < 2)
				{
					NewWick();
				}
				else
				{
					FallT = -1.0f;
				}
			}
		}
		if (bCutting && !bSolved && FallT < 0.0f && Wick < 2)
		{
			const float Inst = Dt > 0.0f ? FrameDist / Dt : 0.0f;
			FrameDist = 0.0f;
			SpeedSm = Approach(SpeedSm, Inst, Dt, 7.0f);
			const FVector2f S = StartPt();
			const FVector2f E = EndPt();
			const float Len = FVector2f::Distance(S, E);
			const FVector2f D = (E - S) / Len;
			const FVector2f Rel = Mouse - S;
			const float Proj = FVector2f::DotProduct(Rel, D) / Len;
			const float Perp = FMath::Abs(D.X * Rel.Y - D.Y * Rel.X);
			if (!bActive)
			{
				// Armed: becomes a cut once the blades sit inside the band (a sloppy press is forgiven).
				bActive = Perp <= Tol;
				SpeedSm = 0.0f;
			}
			else if (Perp > Tol)
			{
				Break(TEXT("Off the line - start again"));
			}
			else if (SpeedSm > Cap)
			{
				Break(TEXT("Too fast - it tore!"));
			}
			else
			{
				const float NewProg = FMath::Min(FMath::Max(CutProg, Proj), CutProg + Cap * 1.25f * Dt / Len);
				SnipAcc += (NewProg - CutProg) * Len;
				CutProg = NewProg;
				if (SnipAcc > 22.0f)
				{
					SnipAcc = 0.0f;
					Sound(TEXT("S_Chore_Scrape"), 0.35f, 1.4f + 0.3f * CutProg);
				}
				// The cut is done within a quarter pixel of the end: Proj is a float projection of the cursor, so a
				// cursor sitting exactly on the end point can project to 0.9999999 (slope dependent) and never reach
				// 1.0 (stabilisation 2026-09-26: the scripted player stalled on two of three seeds).
				if (CutProg >= 1.0f - 0.25f / Len)
				{
					CutProg = 1.0f;
					bCutting = false;
					bActive = false;
					FallT = 0.0f;
					Nice(FVector2f(320.0f, 120.0f), Wick == 0 ? TEXT("Snip! One more") : TEXT("Both trimmed!"), TEXT("S_Chore_Pluck"), 1.0f + 0.15f * Wick);
				}
			}
		}
		else
		{
			FrameDist = 0.0f;
			SpeedSm = Approach(SpeedSm, 0.0f, Dt, 6.0f);
		}
		if (Wick >= 2 && !bSolved && StageTime >= MinWick)
		{
			Solve();
			Sound(TEXT("S_Chore_Flame"), 0.8f, 1.0f);
		}
		if (bSolved)
		{
			LitT = LitT < 0.0f ? 0.0f : LitT + Dt;
		}
	}

	// ---- painting --------------------------------------------------------------------------------------------------

	void PaintRoom(FKGMgPainter& P, float Warm) const
	{
		// Night outside the lantern-room windows.
		P.RectV(FVector2f(0, 0), FVector2f(W, 240.0f), SkyNight, C(0x3B3F7A));
		KGMgCoast::Stars(P, FVector2f(0.0f, 0.0f), FVector2f(W, 160.0f), Time, 50, 11);
		P.Glow(FVector2f(118.0f, 62.0f), 80.0f, A(C(0xDDE6FF), 0.35f));
		P.Circle(FVector2f(118.0f, 62.0f), 20.0f, C(0xF4F1DE));
		P.Circle(FVector2f(111.0f, 57.0f), 4.0f, A(C(0xC9C4A8), 0.7f));
		P.Circle(FVector2f(124.0f, 68.0f), 3.0f, A(C(0xC9C4A8), 0.7f));
		KGMgCoast::Sea(P, FVector2f(0.0f, 176.0f), FVector2f(W, 64.0f), Time, C(0x1E2A5A), C(0x0F1638), C(0xBFD0FF));
		for (int32 i = 0; i < 4; ++i)
		{
			const float Wd = 30.0f - i * 5.0f + 4.0f * FMath::Sin(Time * 2.0f + i);
			P.Bar(FVector2f(118.0f - Wd, 184.0f + i * 12.0f), FVector2f(118.0f + Wd, 184.0f + i * 12.0f), 2.5f, A(C(0xF4F1DE), 0.5f));
		}
		// Window frames + the lower wall.
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = i * 213.0f - 8.0f;
			P.Rect(FVector2f(X, 0.0f), FVector2f(18.0f, 240.0f), C(0x2A2230));
			P.Rect(FVector2f(X + 13.0f, 0.0f), FVector2f(3.0f, 240.0f), A(C(0xC8943A), 0.35f));
		}
		P.Rect(FVector2f(0.0f, 0.0f), FVector2f(W, 10.0f), C(0x2A2230));
		P.RectV(FVector2f(0.0f, 240.0f), FVector2f(W, 160.0f), C(0x4A3444), C(0x2A1D2A));
		P.Rect(FVector2f(0.0f, 228.0f), FVector2f(W, 14.0f), C(0x3A2B3A));
		P.Rect(FVector2f(0.0f, 228.0f), FVector2f(W, 2.0f), A(C(0xC8943A), 0.6f));
		for (int32 i = 0; i < 4; ++i)
		{
			P.RoundRect(FVector2f(14.0f + i * 160.0f, 254.0f), FVector2f(132.0f, 104.0f), A(Ink, 0.12f), 6.0f, A(C(0xC8943A), 0.25f), 1.5f);
		}
		P.Rect(FVector2f(0.0f, 376.0f), FVector2f(W, 24.0f), Iron);
		for (int32 i = 0; i < 32; ++i)
		{
			P.Rect(FVector2f(6.0f + i * 20.0f, 381.0f), FVector2f(12.0f, 3.0f), A(Ink, 0.5f));
			P.Rect(FVector2f(6.0f + i * 20.0f, 390.0f), FVector2f(12.0f, 3.0f), A(Ink, 0.5f));
		}
		if (Warm > 0.0f)
		{
			P.Glow(FVector2f(320.0f, 210.0f), 440.0f, A(Lantern, 0.4f * Warm));
		}
	}

	void PaintOil(FKGMgPainter& P) const
	{
		PaintRoom(P, 0.0f);
		const float FX = FunnelX();
		const float RailY = FunnelY + 38.0f;

		// Puddles of spilt oil on the floor.
		for (const FVector2f& Pd : Puddles)
		{
			const float Rx = 12.0f + 70.0f * FMath::Sqrt(Pd.Y);
			P.Poly(KGMgCoast::Ellipse(FVector2f(Pd.X, 384.0f), FVector2f(Rx, Rx * 0.22f), 0.0f, 20), A(C(0xA86E18), 0.8f));
			P.Poly(KGMgCoast::Ellipse(FVector2f(Pd.X - Rx * 0.3f, 382.0f), FVector2f(Rx * 0.3f, 2.0f), 0.0f, 12), A(C(0xFFE0A0), 0.5f));
		}

		// The lamp's oil tank (fixed) - it is the level gauge.
		const FVector2f T0(245.0f, 288.0f);
		const FVector2f TS(150.0f, 84.0f);
		P.Shadow(T0, TS, 8.0f, 0.4f, 4.0f);
		P.RoundRect(T0, TS, A(C(0xBFD8E8), 0.18f), 8.0f, A(Cream, 0.5f), 2.0f);
		const float LevelY = T0.Y + TS.Y - 4.0f - (TS.Y - 8.0f) * Saturate(OilLevel);
		if (OilLevel > 0.005f)
		{
			P.RectV(FVector2f(T0.X + 4.0f, LevelY), FVector2f(TS.X - 8.0f, T0.Y + TS.Y - 4.0f - LevelY), C(0xF2C048), C(0xC98A20));
			P.Rect(FVector2f(T0.X + 4.0f, LevelY), FVector2f(TS.X - 8.0f, 2.0f), A(C(0xFFF0C0), 0.8f));
		}
		auto LevelAt = [&](float L) { return T0.Y + TS.Y - 4.0f - (TS.Y - 8.0f) * L; };
		P.Rect(FVector2f(T0.X + 2.0f, LevelAt(Band1)), FVector2f(TS.X - 4.0f, LevelAt(Band0) - LevelAt(Band1)), A(Good, 0.28f));
		P.Rect(FVector2f(T0.X - 6.0f, LevelAt(Band0) - 1.5f), FVector2f(TS.X + 12.0f, 3.0f), Good);
		P.Rect(FVector2f(T0.X - 6.0f, LevelAt(Band1) - 1.0f), FVector2f(TS.X + 12.0f, 2.0f), A(Crimson, 0.9f));
		P.Text(FVector2f(T0.X + TS.X + 10.0f, LevelAt(Band0) - 8.0f), TEXT("FULL"), 11.0f, Good, 0.0f, TEXT("Black"), 60);
		P.Text(FVector2f(T0.X + TS.X + 10.0f, LevelAt(Band1) - 20.0f), TEXT("TOO MUCH"), 10.0f, C(0xFF8A80), 0.0f, TEXT("Bold"), 60);
		P.Rect(FVector2f(T0.X + 12.0f, T0.Y + 6.0f), FVector2f(6.0f, TS.Y - 12.0f), A(Cream, 0.25f));
		P.RoundRect(FVector2f(T0.X - 8.0f, T0.Y - 8.0f), FVector2f(TS.X + 16.0f, 12.0f), C(0xC8943A), 4.0f);
		P.RoundRect(FVector2f(T0.X - 8.0f, T0.Y + TS.Y - 4.0f), FVector2f(TS.X + 16.0f, 12.0f), C(0x8C5E22), 4.0f);
		P.Text(FVector2f(320.0f, T0.Y + 30.0f), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(OilLevel / Band0 * 100.0f)), 18.0f,
		       A(Ink, 0.75f), 0.5f, TEXT("Black"));

		// Hose from the funnel down to the tank, and the rail the funnel carriage runs on.
		TArray<FVector2f> Hose;
		for (int32 k = 0; k <= 16; ++k)
		{
			Hose.Add(KGMgCoast::Bezier(FVector2f(FX, FunnelY + 48.0f), FVector2f(FX, T0.Y + 10.0f), FVector2f(320.0f, T0.Y - 8.0f), k / 16.0f));
		}
		P.Line(Hose, C(0x3A2B2A), 12.0f);
		P.Line(Hose, C(0x5A4040), 7.0f);
		P.Bar(FVector2f(160.0f, RailY), FVector2f(480.0f, RailY), 6.0f, C(0x8C5E22));
		P.Bar(FVector2f(160.0f, RailY - 2.0f), FVector2f(480.0f, RailY - 2.0f), 1.5f, A(C(0xFFE0A0), 0.5f));
		P.Circle(FVector2f(160.0f, RailY), 7.0f, C(0xC8943A));
		P.Circle(FVector2f(480.0f, RailY), 7.0f, C(0xC8943A));

		// Funnel.
		const bool bPouring = FlowNow > 0.001f;
		const bool bHit = bPouring && StreamHits();
		if (bPouring)
		{
			P.Glow(FVector2f(FX, FunnelY + 6.0f), 60.0f, A(bHit ? Good : Crimson, 0.45f));
		}
		P.Quad(FVector2f(FX - 34.0f, FunnelY), FVector2f(FX + 34.0f, FunnelY), FVector2f(FX + 9.0f, FunnelY + 32.0f), FVector2f(FX - 9.0f, FunnelY + 32.0f),
		       C(0xC8943A));
		P.Quad(FVector2f(FX - 26.0f, FunnelY + 2.0f), FVector2f(FX - 16.0f, FunnelY + 2.0f), FVector2f(FX - 4.0f, FunnelY + 30.0f),
		       FVector2f(FX - 7.0f, FunnelY + 30.0f), A(C(0xFFE3A0), 0.5f));
		P.Rect(FVector2f(FX - 6.0f, FunnelY + 32.0f), FVector2f(12.0f, 18.0f), C(0x8C5E22));
		P.Poly(KGMgCoast::Ellipse(FVector2f(FX, FunnelY), FVector2f(34.0f, 6.0f), 0.0f, 20), C(0x3A2416));
		KGMgCoast::Ring(P, FVector2f(FX, FunnelY), FVector2f(34.0f, 6.0f), C(0xE0B060), 2.5f, 24);
		P.Circle(FVector2f(FX, RailY), 9.0f, C(0x8C5E22), C(0xC8943A), 2.0f);

		// The can: rotates about its spout tip, which follows the mouse.
		const float Lift = RefillT >= 0.0f ? 230.0f * FMath::Sin(PI * Saturate(RefillT / 1.4f)) : 0.0f;
		const FVector2f Tip(CanX, SpoutY - Lift);
		const float Ang = Tilt * 0.95f;
		auto Cp = [&](float X, float Y) { return Tip + KGMgCoast::Rot(FVector2f(X - 88.0f, Y + 52.0f), Ang); };
		// Stream first (behind the can).
		if (bPouring && RefillT < 0.0f)
		{
			const float EndY = bHit ? FunnelY + 4.0f : 380.0f;
			TArray<FVector2f> S;
			for (int32 k = 0; k <= 12; ++k)
			{
				const float T = k / 12.0f;
				S.Add(FVector2f(Tip.X + StreamDx * FMath::Sqrt(T) + FMath::Sin(Time * 30.0f + k) * 0.6f, FMath::Lerp(Tip.Y + 2.0f, EndY, T)));
			}
			const float Thick = 2.5f + 4.0f * FlowNow / PourRate;
			P.Line(S, C(0xB07818), Thick + 2.0f);
			P.Line(S, C(0xF2C048), Thick);
			if (!bHit)
			{
				for (int32 d = 0; d < 4; ++d)
				{
					const float Ph = FMath::Fmod(Time * 3.0f + d * 0.25f, 1.0f);
					P.Circle(FVector2f(StreamX() + (d - 1.5f) * 10.0f * Ph, EndY - 18.0f * FMath::Sin(Ph * PI)), 2.5f, A(C(0xF2C048), 1.0f - Ph));
				}
			}
		}
		P.Poly({Cp(0.0f, -22.0f), Cp(62.0f, -24.0f), Cp(62.0f, 28.0f), Cp(0.0f, 30.0f)}, C(0xAEB8C4));
		P.Poly({Cp(0.0f, -6.0f), Cp(62.0f, -8.0f), Cp(62.0f, 10.0f), Cp(0.0f, 12.0f)}, C(0xB5483A));
		P.Circle(Cp(31.0f, 2.0f), 8.0f, Cream);
		P.Text(Cp(31.0f, -4.0f), TEXT("OIL"), 10.0f, C(0xB5483A), 0.5f, TEXT("Black"));
		P.Bar(Cp(10.0f, -20.0f), Cp(10.0f, 28.0f), 4.0f, A(Cream, 0.4f));
		P.Bar(Cp(44.0f, -18.0f), Tip, 6.0f, C(0x8C96A2));
		P.Bar(Cp(22.0f, -25.0f), Cp(40.0f, -25.0f), 8.0f, C(0x8C96A2));
		P.Line({Cp(0.0f, -12.0f), Cp(-18.0f, -10.0f), Cp(-20.0f, 12.0f), Cp(0.0f, 18.0f)}, C(0x6A7480), 5.0f);
		P.Circle(Tip, 3.0f, C(0x6A7480));

		// HUD: the can's contents, hints.
		P.RoundRect(FVector2f(16.0f, 16.0f), FVector2f(180.0f, 44.0f), A(Ink, 0.7f), 8.0f);
		P.Text(FVector2f(28.0f, 20.0f), TEXT("OIL IN THE CAN"), 10.0f, CreamDim, 0.0f, TEXT("Bold"), 80);
		P.Gauge(FVector2f(28.0f, 36.0f), FVector2f(156.0f, 14.0f), CanOil / CanCap, CanOil < 0.3f ? Crimson : C(0xF2C048));
		if (RefillT >= 0.0f)
		{
			P.Tag(FVector2f(320.0f, 110.0f), TEXT("Refilling the can..."), A(Ink, 0.8f), Cream, 14.0f);
		}
		else if (OilLevel < 0.03f && Tilt < 0.05f)
		{
			P.HintRing(Tip + FVector2f(-50.0f, 40.0f), 40.0f, Time, Gold);
			P.Tag(Tip + FVector2f(-50.0f, -30.0f), TEXT("Hold to pour"), A(Ink, 0.8f), Gold, 13.0f);
			P.HintArrow(FVector2f(FX, 170.0f), FVector2f(FX, FunnelY - 10.0f), Time, A(Gold, 0.9f));
		}
		else if (OilLevel >= Band0 && bPouring)
		{
			P.Tag(FVector2f(320.0f, 270.0f), TEXT("LET GO!"), A(Good, 0.9f + 0.1f * Ping(Time, 0.3f)), Ink, 16.0f);
		}
	}

	void PaintWick(FKGMgPainter& P) const
	{
		const float Lit = LitT >= 0.0f ? Saturate(LitT / 0.6f) : 0.0f;
		PaintRoom(P, Lit);

		// Fresnel lens around the burner: nested glass rings.
		const FVector2f LensC(320.0f, 200.0f);
		for (int32 r = 0; r < 7; ++r)
		{
			const float R = 196.0f - r * 24.0f;
			P.Circle(LensC, R, A(Mix(C(0xE8C07A), C(0xFFF0C0), r / 6.0f), 0.08f + 0.025f * r + 0.2f * Lit), A(C(0xFFE8B0), 0.3f + 0.3f * Lit), 2.0f);
		}
		P.Rect(FVector2f(118.0f, 196.0f), FVector2f(404.0f, 6.0f), A(C(0xC8943A), 0.7f));

		// Wick ribbon (fresh cotton), then the charred crust above the cut line.
		const float WickL = 194.0f;
		const float WickR = 446.0f;
		P.Quad(FVector2f(WickL, CutY(WickL) - 4.0f), FVector2f(WickR, CutY(WickR) - 4.0f), FVector2f(WickR, 258.0f), FVector2f(WickL, 258.0f),
		       C(0xEEE3C8));
		P.Rect(FVector2f(WickL, 226.0f), FVector2f(WickR - WickL, 32.0f), A(C(0xC8B890), 0.5f));
		for (int32 i = 0; i < 31; ++i)
		{
			const float X = WickL + 4.0f + i * 8.0f;
			P.Segment(FVector2f(X, CutY(X) - 2.0f), FVector2f(X, 258.0f), A(Ink, 0.08f), 1.5f);
		}
		if (Wick < 2)
		{
			const float Fall = FallT >= 0.0f ? FallT : 0.0f;
			const FVector2f Pivot(320.0f, 160.0f);
			const float FallAng = Fall * 1.3f;
			const FVector2f FallOff(Fall * 70.0f, Fall * Fall * 520.0f);
			const float Fade = 1.0f - Saturate((Fall - 0.5f) / 0.4f);
			auto Xf = [&](const FVector2f& Q) { return Pivot + KGMgCoast::Rot(Q - Pivot, FallAng) + FallOff; };
			const FLinearColor Char = A(C(0x2A201C), Fade);
			P.Quad(Xf(FVector2f(WickL, 122.0f)), Xf(FVector2f(WickR, 122.0f)), Xf(FVector2f(WickR, CutY(WickR) - 3.0f)), Xf(FVector2f(WickL, CutY(WickL) - 3.0f)),
			       Char);
			for (int32 i = 0; i < 18; ++i)
			{
				const float X = WickL + i * 14.0f;
				const float Spike = 8.0f + KGMgCoast::Hash01(i + Wick * 50) * 14.0f;
				P.Tri(Xf(FVector2f(X, 123.0f)), Xf(FVector2f(FMath::Min(X + 14.0f, WickR), 123.0f)), Xf(FVector2f(X + 7.0f, 122.0f - Spike)), Char);
			}
			for (int32 i = 0; i < 9; ++i)
			{
				const float X = WickL + 20.0f + i * 26.0f + KGMgCoast::Hash01(i * 3 + Wick) * 10.0f;
				const float Y = 132.0f + KGMgCoast::Hash01(i * 5 + 1 + Wick) * (CutY(X) - 150.0f);
				const float Emb = 0.5f + 0.5f * FMath::Sin(Time * 3.0f + i);
				P.Circle(Xf(FVector2f(X, Y)), 2.5f, A(Mix(C(0xFF6A1A), Gold, Emb), Fade * (0.6f + 0.4f * Emb)));
			}
			// Crusty lower edge.
			for (int32 i = 0; i < 25; ++i)
			{
				const float X = WickL + i * 10.0f;
				P.Tri(Xf(FVector2f(X, CutY(X) - 4.0f)), Xf(FVector2f(FMath::Min(X + 10.0f, WickR), CutY(X + 10.0f) - 4.0f)),
				      Xf(FVector2f(X + 5.0f, CutY(X + 5.0f) + 1.0f + KGMgCoast::Hash01(i + 7) * 3.0f)), Char);
			}
		}

		// Burner in front of the wick's foot.
		P.Shadow(FVector2f(180.0f, 262.0f), FVector2f(280.0f, 104.0f), 10.0f);
		P.RoundRect(FVector2f(180.0f, 262.0f), FVector2f(280.0f, 104.0f), C(0xC8943A), 12.0f);
		P.Rect(FVector2f(192.0f, 272.0f), FVector2f(256.0f, 8.0f), A(C(0xFFE3A0), 0.45f));
		P.Rect(FVector2f(180.0f, 320.0f), FVector2f(280.0f, 10.0f), C(0x8C5E22));
		P.RoundRect(FVector2f(172.0f, 246.0f), FVector2f(296.0f, 22.0f), C(0x8C5E22), 6.0f);
		P.Rect(FVector2f(176.0f, 248.0f), FVector2f(288.0f, 3.0f), A(C(0xFFE3A0), 0.4f));
		for (int32 i = 0; i < 5; ++i)
		{
			P.Circle(FVector2f(206.0f + i * 57.0f, 346.0f), 4.0f, C(0x8C5E22), A(C(0xFFE3A0), 0.5f), 1.0f);
		}

		if (Wick < 2 && FallT < 0.0f)
		{
			// Guide band + dashed line; the part already cut is a clean bright slit.
			const FVector2f S = StartPt();
			const FVector2f E = EndPt();
			const FVector2f D = (E - S).GetSafeNormal();
			const FVector2f N(-D.Y, D.X);
			P.Quad(S + N * Tol, E + N * Tol, E - N * Tol, S - N * Tol, A(Good, 0.16f));
			P.Segment(S + N * Tol, E + N * Tol, A(Good, 0.55f), 1.5f);
			P.Segment(S - N * Tol, E - N * Tol, A(Good, 0.55f), 1.5f);
			const FVector2f Cut = CutPoint(CutProg);
			const float Len = FVector2f::Distance(S, E);
			for (float Dd = CutProg * Len; Dd < Len; Dd += 14.0f)
			{
				P.Segment(S + D * Dd, S + D * FMath::Min(Len, Dd + 7.0f), A(Cream, 0.9f), 2.0f);
			}
			if (CutProg > 0.0f)
			{
				P.Segment(S, Cut, Cream, 3.0f);
			}
			P.Circle(S, 9.0f, Good, Ink, 2.0f);
			P.Text(S + FVector2f(0.0f, 16.0f), TEXT("START"), 10.0f, Good, 0.5f, TEXT("Black"), 80);
			P.Circle(E, 7.0f, A(Cream, 0.9f), Ink, 2.0f);
			if (!bCutting)
			{
				P.HintRing(Cut, 26.0f, Time, Gold);
				if (CutProg <= 0.0f)
				{
					P.HintArrow(S + N * 34.0f, E + N * 34.0f, Time, A(Gold, 0.8f));
				}
			}
		}
		if (BreakT > 0.0f)
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const float Ang = k * 1.26f + 0.3f;
				P.Segment(BreakAt, BreakAt + KGMgCoast::Unit(Ang) * (10.0f + 14.0f * BreakT), A(Crimson, BreakT), 3.0f);
			}
		}

		// Flame(s) once both wicks are trimmed and the stage is solved.
		if (Lit > 0.0f)
		{
			for (int32 i = 0; i < 6; ++i)
			{
				const float X = WickL + 22.0f + i * 41.0f;
				KGMgCoast::Flame(P, FVector2f(X, CutY(X) + 2.0f), (10.0f + 5.0f * (i % 2)) * EaseOutBack(Lit), FMath::Sin(Time * 2.0f + i) * 0.2f,
				                 Time + i * 0.37f, 0.7f);
			}
		}

		// Scissors follow the mouse (blades along the guide).
		if (Wick < 2 && FallT < 0.0f)
		{
			const FVector2f D = (EndPt() - StartPt()).GetSafeNormal();
			const float Ang = FMath::Atan2(D.Y, D.X);
			const float Open = bCutting ? 0.12f + 0.16f * FMath::Abs(FMath::Sin(CutProg * 40.0f)) : 0.3f;
			PaintScissors(P, Mouse - D * 22.0f, Ang, Open);
		}

		// Steady-hand speed gauge + counter.
		const float Frac = 1.0f / 1.5f;
		P.RoundRect(FVector2f(566.0f, 84.0f), FVector2f(64.0f, 232.0f), A(Ink, 0.7f), 10.0f);
		P.Text(FVector2f(598.0f, 92.0f), TEXT("SPEED"), 10.0f, CreamDim, 0.5f, TEXT("Bold"), 80);
		const float SpeedFill = Saturate(SpeedSm / (Cap * 1.5f));
		P.GaugeV(FVector2f(585.0f, 110.0f), FVector2f(26.0f, 180.0f), SpeedFill, SpeedFill > Frac * 0.85f ? Crimson : Good, 0.0f, Frac);
		P.Text(FVector2f(598.0f, 294.0f), TEXT("SLOW"), 10.0f, Good, 0.5f, TEXT("Bold"), 60);
		P.Tag(FVector2f(80.0f, 30.0f), FString::Printf(TEXT("WICK %d / 2"), FMath::Min(Wick + 1, 2)), A(Ink, 0.8f), Cream, 13.0f);
	}

	static void PaintScissors(FKGMgPainter& P, const FVector2f& At, float Ang, float Open)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float Ba = Ang + s * Open;
			const FVector2f D = KGMgCoast::Unit(Ba);
			const FVector2f N(-D.Y, D.X);
			const float Ha = Ang + PI - s * Open * 1.4f;
			const FVector2f Hd = KGMgCoast::Unit(Ha);
			P.Bar(At, At + Hd * 26.0f, 6.0f, C(0x8C2A26));
			P.Arc(At + Hd * 38.0f, 11.0f, 0.0f, 2.0f * PI, C(0xC8403A), 5.0f, 20);
			P.Tri(At + N * 4.5f, At - N * 4.5f, At + D * 60.0f, C(0xC9D2DC));
			P.Segment(At + N * 2.0f * float(s), At + D * 56.0f, A(Cream, 0.9f), 1.2f);
		}
		P.Circle(At, 4.5f, Iron, Cream, 1.0f);
	}
};

// =====================================================================================================================
// FixBoat: dip the brush in the hot tar and paint over every crack in the hull (dots turn green when sealed).
// =====================================================================================================================
class FKGMgFixBoat final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return TEXT("Dip the brush in the tar, then hold and paint slowly over each crack until all dots are green");
	}

	virtual void BeginStage() override
	{
		Cracks.Reset();
		Dabs.Reset();
		Load = 0.0f;
		DipT = -1.0f;
		BrushSndT = 0.0f;
		WasteCD = 0.0f;
		WasteAcc = 0.0f;
		bDryWarned = false;
		Sealed = 0;
		AutoCrack = 0;
		AutoSample = 0;
		const int32 Count = Stage == 0 ? 3 : 4;
		TArray<float> Rows = {0.24f, 0.4f, 0.57f, 0.73f};
		Rng.Shuffle(Rows);
		for (int32 c = 0; c < Count; ++c)
		{
			FCrack& K = Cracks.AddDefaulted_GetRef();
			const float LenU = 0.24f + Rng.FRand() * 0.07f;
			const float U0 = 0.32f + Rng.FRand() * (0.9f - 0.32f - LenU);
			const float V0 = Rows[c];
			const float Slant = (Rng.FRand() - 0.5f) * 0.1f;
			for (int32 j = 0; j <= 7; ++j)
			{
				const float T = j / 7.0f;
				const float Jag = (j % 2 ? 1.0f : -1.0f) * (0.012f + Rng.FRand() * 0.02f) * (j == 0 || j == 7 ? 0.3f : 1.0f);
				K.Pts.Add(HullPt(U0 + LenU * T, V0 + Slant * T + Jag));
			}
			for (int32 j = 0; j + 1 < K.Pts.Num(); ++j)
			{
				const float SegLen = FVector2f::Distance(K.Pts[j], K.Pts[j + 1]);
				const int32 Steps = FMath::Max(1, FMath::RoundToInt(SegLen / 9.0f));
				for (int32 s = 0; s < Steps; ++s)
				{
					K.Samples.Add(FMath::Lerp(K.Pts[j], K.Pts[j + 1], float(s) / float(Steps)));
				}
			}
			K.Samples.Add(K.Pts.Last());
			K.Cov.Init(0.0f, K.Samples.Num());
		}
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (DipT >= 0.0f)
		{
			return;
		}
		if (FVector2f::Distance(Pos, BucketC) < 52.0f)
		{
			Dip();
		}
		else if (Load <= 0.0f)
		{
			bDryWarned = true;
			Oops(Pos + FVector2f(0.0f, -34.0f), TEXT("Dip the brush in the tar first"), TEXT("S_UI_Bad"), 3.0f);
		}
		else
		{
			bDryWarned = false;
		}
	}

	virtual void Tick(float Dt) override
	{
		WasteCD = FMath::Max(0.0f, WasteCD - Dt);
		if (DipT >= 0.0f)
		{
			DipT += Dt;
			if (DipT >= DipTime * 0.55f && Load < 1.0f)
			{
				Load = 1.0f;
				Sound(TEXT("S_Chore_Squish"), 0.7f, 0.8f);
			}
			if (DipT >= DipTime)
			{
				DipT = -1.0f;
			}
		}
		else if (bMouseDown && !bSolved && Load > 0.0f && FVector2f::Distance(Mouse, BucketC) >= 52.0f)
		{
			PaintTar(Mouse, Dt);
		}
		for (FCrack& K : Cracks)
		{
			if (K.bDone)
			{
				continue;
			}
			bool bAll = true;
			for (const float Cv : K.Cov)
			{
				bAll &= Cv >= 1.0f;
			}
			if (bAll)
			{
				K.bDone = true;
				++Sealed;
				Nice(K.Pts[K.Pts.Num() / 2] + FVector2f(0.0f, -30.0f), Sealed == Cracks.Num() ? TEXT("Watertight!") : TEXT("Sealed!"),
				     TEXT("S_Chore_Brush"), 0.9f + 0.1f * Sealed);
			}
		}
		if (!bSolved && Sealed >= Cracks.Num() && Cracks.Num() > 0 && StageTime >= MinSolve)
		{
			Solve();
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (DipT >= 0.0f)
		{
			return;
		}
		if (Load <= 0.02f)
		{
			Dip();
			return;
		}
		while (Cracks.IsValidIndex(AutoCrack) && Cracks[AutoCrack].bDone)
		{
			++AutoCrack;
			AutoSample = 0;
		}
		if (!Cracks.IsValidIndex(AutoCrack))
		{
			return;
		}
		FCrack& K = Cracks[AutoCrack];
		while (K.Cov.IsValidIndex(AutoSample) && K.Cov[AutoSample] >= 1.0f)
		{
			++AutoSample;
		}
		const int32 Idx = FMath::Min(AutoSample + 1, K.Samples.Num() - 1);
		Mouse = FMath::Lerp(Mouse, K.Samples[Idx], 1.0f - FMath::Exp(-10.0f * Dt));
		PaintTar(Mouse, Dt);
	}

	virtual void DebugPose() override
	{
		for (int32 c = 0; c < Cracks.Num(); ++c)
		{
			FCrack& K = Cracks[c];
			const int32 Upto = c == 0 ? K.Cov.Num() : c == 1 ? K.Cov.Num() / 2 : 0;
			for (int32 s = 0; s < K.Cov.Num(); ++s)
			{
				K.Cov[s] = s < Upto ? 1.0f : 0.0f;
				if (s < Upto)
				{
					Dabs.Add(K.Samples[s] + FVector2f(0.0f, (s % 3) - 1.0f));
				}
			}
			K.bDone = c == 0;
		}
		Sealed = 1;
		Load = 0.55f;
		DipT = -1.0f;
		if (Cracks.Num() > 1)
		{
			const FCrack& K = Cracks[1];
			Mouse = K.Samples[FMath::Min(K.Samples.Num() - 1, K.Samples.Num() / 2 + 1)];
		}
		Dabs.Add(Mouse + FVector2f(40.0f, 30.0f));
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintShed(P);
		PaintHull(P);

		// Cracks: glowing seams to find, then the tar laid down, then the progress dots on top.
		for (const FCrack& K : Cracks)
		{
			if (!K.bDone)
			{
				P.Line(K.Pts, A(Gold, 0.18f + 0.18f * Ping(Time, 1.2f)), 12.0f);
			}
			P.Line(K.Pts, A(C(0x1A100A), 0.95f), 3.5f);
			for (int32 j = 1; j + 1 < K.Pts.Num(); ++j)
			{
				const FVector2f D = (K.Pts[j + 1] - K.Pts[j - 1]).GetSafeNormal();
				const FVector2f N(-D.Y, D.X);
				const float Side = j % 2 ? 1.0f : -1.0f;
				P.Segment(K.Pts[j], K.Pts[j] + N * Side * 7.0f + D * 4.0f, A(C(0x1A100A), 0.85f), 2.0f);
			}
		}
		for (const FVector2f& Db : Dabs)
		{
			P.Circle(Db, 10.0f, A(C(0x2B1B12), 0.92f));
		}
		for (const FVector2f& Db : Dabs)
		{
			P.Circle(Db + FVector2f(-3.0f, -3.0f), 2.5f, A(C(0xFFE3A0), 0.18f));
		}
		for (const FCrack& K : Cracks)
		{
			for (int32 s = 0; s < K.Samples.Num(); ++s)
			{
				const bool bOn = K.Cov[s] >= 1.0f;
				if (bOn)
				{
					P.Circle(K.Samples[s], 3.0f, Good);
				}
				else
				{
					P.Circle(K.Samples[s], 3.2f, Mix(C(0xFF8A5A), Good, K.Cov[s]), A(Ink, 0.6f), 1.0f);
				}
			}
			if (K.bDone)
			{
				const FVector2f Ck = K.Pts.Last() + FVector2f(16.0f, -12.0f);
				P.Circle(Ck, 10.0f, Good);
				P.Line({Ck + FVector2f(-5.0f, 0.0f), Ck + FVector2f(-1.0f, 4.0f), Ck + FVector2f(5.0f, -4.0f)}, Ink, 2.5f);
			}
		}

		PaintBucket(P);
		PaintBrush(P);

		// HUD.
		P.Tag(FVector2f(320.0f, 22.0f), FString::Printf(TEXT("%s  -  cracks sealed %d / %d"), Stage == 0 ? TEXT("PORT SIDE") : TEXT("STARBOARD"), Sealed,
		                                                Cracks.Num()),
		      A(Ink, 0.78f), Cream, 13.0f);
		P.Text(FVector2f(18.0f, 358.0f), TEXT("TAR ON THE BRUSH"), 10.0f, Cream, 0.0f, TEXT("Bold"), 80);
		P.Gauge(FVector2f(18.0f, 374.0f), FVector2f(150.0f, 14.0f), Load, Load < 0.25f ? Crimson : C(0x6A4A30));
		if (Load <= 0.0f && DipT < 0.0f)
		{
			P.HintRing(BucketC + FVector2f(0.0f, -10.0f), 46.0f, Time, Gold);
			P.Tag(BucketC + FVector2f(-94.0f, -30.0f), TEXT("Dip!"), A(Ink, 0.8f), Gold, 13.0f);
		}
		else if (Sealed == 0 && !bMouseDown && Cracks.Num() > 0)
		{
			const FCrack& K = Cracks[0];
			P.HintRing(K.Samples[0], 22.0f, Time, Gold);
			P.HintArrow(K.Samples[0] + FVector2f(0.0f, -24.0f), K.Samples.Last() + FVector2f(0.0f, -24.0f), Time, A(Gold, 0.85f));
		}
	}

private:
	struct FCrack
	{
		TArray<FVector2f> Pts;
		TArray<FVector2f> Samples;
		TArray<float> Cov;
		bool bDone = false;
	};

	static constexpr float BrushR = 20.0f;
	static constexpr float Deposit = 4.0f;   // coverage per second under the brush: the brush can't go faster than ~160 px/s
	static constexpr float DipTime = 0.75f;
	static constexpr float MinSolve = 4.0f;
	const FVector2f BucketC = FVector2f(566.0f, 352.0f);

	TArray<FCrack> Cracks;
	TArray<FVector2f> Dabs;
	FVector2f LastDab = FVector2f::ZeroVector;
	float Load = 0.0f;
	float DipT = -1.0f;
	float BrushSndT = 0.0f;
	float WasteCD = 0.0f;
	float WasteAcc = 0.0f;
	bool bDryWarned = false;
	int32 Sealed = 0;
	int32 AutoCrack = 0;
	int32 AutoSample = 0;

	/** Hull surface: U 0 = bow .. 1 = stern, V 0 = gunwale .. 1 = keel. Port side has the bow on the left. */
	FVector2f HullPt(float U, float V) const
	{
		const float Top = 104.0f - 36.0f * FMath::Pow(1.0f - U, 3.0f) - 10.0f * FMath::Pow(U, 4.0f);
		const float Bow = 1.0f - Saturate(U / 0.32f);
		const float Stern = Saturate((U - 0.82f) / 0.18f);
		const float Bot = 292.0f - 182.0f * Bow * Bow - 16.0f * Stern * Stern;
		const float X = Stage == 0 ? FMath::Lerp(64.0f, 584.0f, U) : FMath::Lerp(584.0f, 64.0f, U);
		return FVector2f(X, FMath::Lerp(Top, Bot, V));
	}

	void Dip()
	{
		if (DipT >= 0.0f)
		{
			return;
		}
		DipT = 0.0f;
		bDryWarned = false;
		Sound(TEXT("S_Chore_Pour"), 0.6f, 0.7f);
	}

	void PaintTar(const FVector2f& At, float Dt)
	{
		bool bNear = false;
		for (FCrack& K : Cracks)
		{
			for (int32 s = 0; s < K.Samples.Num(); ++s)
			{
				if (FVector2f::DistSquared(At, K.Samples[s]) < BrushR * BrushR)
				{
					bNear = true;
					K.Cov[s] = FMath::Min(1.0f, K.Cov[s] + Deposit * Dt);
				}
			}
		}
		Load = FMath::Max(0.0f, Load - Dt * (bNear ? 0.42f : 0.8f));
		if (FVector2f::Distance(At, LastDab) > 6.0f && Dabs.Num() < 900)
		{
			Dabs.Add(At);
			LastDab = At;
		}
		BrushSndT -= Dt;
		if (BrushSndT <= 0.0f)
		{
			BrushSndT = 0.35f;
			Sound(TEXT("S_Chore_Brush"), 0.45f, bNear ? 1.0f : 0.8f);
		}
		if (!bNear)
		{
			WasteAcc += Dt;
			if (WasteAcc > 0.5f && WasteCD <= 0.0f)
			{
				WasteCD = 2.5f;
				WasteAcc = 0.0f;
				PopText(At + FVector2f(0.0f, -34.0f), TEXT("Wasting tar!"), C(0xFF8A80));
			}
		}
		else
		{
			WasteAcc = 0.0f;
		}
		if (Load <= 0.0f && !bDryWarned)
		{
			bDryWarned = true;
			Oops(At + FVector2f(0.0f, -34.0f), TEXT("Brush is dry - dip it!"), TEXT("S_UI_Bad"), 3.0f);
		}
	}

	void PaintShed(FKGMgPainter& P) const
	{
		// Boathouse: plank wall with an open doorway onto the harbour.
		P.RectV(FVector2f(0, 0), FVector2f(W, 330.0f), C(0x7A5234), C(0x5C3A26));
		for (int32 i = 0; i < 17; ++i)
		{
			const float X = i * 40.0f;
			P.Rect(FVector2f(X, 0.0f), FVector2f(2.0f, 330.0f), A(Ink, 0.35f));
			P.Rect(FVector2f(X + 2.0f, 0.0f), FVector2f(2.0f, 330.0f), A(WoodLight, 0.18f));
		}
		const FVector2f Door0(236.0f, 30.0f);
		const FVector2f DoorS(170.0f, 300.0f);
		P.RectV(Door0, FVector2f(DoorS.X, 140.0f), C(0x9ED7F2), C(0xF4E6C8));
		P.Circle(Door0 + FVector2f(120.0f, 50.0f), 14.0f, C(0xFFF4C8));
		P.Glow(Door0 + FVector2f(120.0f, 50.0f), 50.0f, A(C(0xFFF0B0), 0.5f));
		KGMgCoast::Sea(P, Door0 + FVector2f(0.0f, 140.0f), FVector2f(DoorS.X, DoorS.Y - 140.0f), Time, C(0x5CC0E0), C(0x2A7AAE), C(0xE6FAFF));
		P.Rect(Door0 + FVector2f(-10.0f, -10.0f), FVector2f(DoorS.X + 20.0f, 10.0f), WoodDark);
		P.Rect(Door0 + FVector2f(-10.0f, 0.0f), FVector2f(10.0f, DoorS.Y), WoodDark);
		P.Rect(Door0 + FVector2f(DoorS.X, 0.0f), FVector2f(10.0f, DoorS.Y), WoodDark);
		// Oars and a rope coil on the wall; lantern.
		P.Bar(FVector2f(40.0f, 30.0f), FVector2f(80.0f, 250.0f), 6.0f, WoodLight);
		P.RotRect(FVector2f(84.0f, 250.0f), FVector2f(14.0f, 44.0f), -0.18f, WoodLight);
		P.Bar(FVector2f(470.0f, 40.0f), FVector2f(440.0f, 240.0f), 6.0f, WoodLight);
		P.RotRect(FVector2f(438.0f, 244.0f), FVector2f(14.0f, 44.0f), 0.15f, WoodLight);
		for (int32 r = 0; r < 4; ++r)
		{
			P.Arc(FVector2f(560.0f, 70.0f), 22.0f - r * 4.0f, 0.0f, 2.0f * PI, C(0xD8B98A), 3.5f, 24);
		}
		P.Glow(FVector2f(150.0f, 60.0f), 120.0f, A(Lantern, 0.35f));
		P.Rect(FVector2f(148.0f, 20.0f), FVector2f(4.0f, 24.0f), Iron);
		P.RoundRect(FVector2f(138.0f, 44.0f), FVector2f(24.0f, 30.0f), A(C(0xFFD27A), 0.9f), 5.0f, Iron, 3.0f);
		// Floor.
		P.RectV(FVector2f(0.0f, 330.0f), FVector2f(W, 70.0f), C(0x9A7A58), C(0x7A5A3E));
		for (int32 i = 0; i < 4; ++i)
		{
			P.Rect(FVector2f(0.0f, 340.0f + i * 16.0f), FVector2f(W, 2.0f), A(Ink, 0.25f));
		}
		P.Quad(Door0 + FVector2f(0.0f, DoorS.Y), Door0 + DoorS, FVector2f(460.0f, 400.0f), FVector2f(180.0f, 400.0f), A(C(0xFFF0C8), 0.12f));
	}

	void PaintHull(FKGMgPainter& P) const
	{
		static const float Bands[] = {0.0f, 0.15f, 0.31f, 0.48f, 0.65f, 0.82f, 1.0f};
		const FLinearColor BandCol[] = {C(0x2F6DA8), C(0xEFE6D2), C(0xB77A4A), C(0xA96E42), C(0xB77A4A), C(0xA8412F)};
		const int32 Cols = 30;
		P.Disc(FVector2f(324.0f, 368.0f), FVector2f(290.0f, 18.0f), A(Ink, 0.45f), A(Ink, 0.0f));
		// Trestles.
		for (const float Ut : {0.36f, 0.8f})
		{
			const FVector2f Kp = HullPt(Ut, 1.0f);
			P.Bar(Kp, FVector2f(Kp.X - 34.0f, 372.0f), 10.0f, WoodDark);
			P.Bar(Kp, FVector2f(Kp.X + 34.0f, 372.0f), 10.0f, WoodDark);
			P.Bar(FVector2f(Kp.X - 22.0f, 340.0f), FVector2f(Kp.X + 22.0f, 340.0f), 6.0f, Wood);
			P.RoundRect(FVector2f(Kp.X - 40.0f, Kp.Y - 4.0f), FVector2f(80.0f, 12.0f), Wood, 3.0f);
		}
		for (int32 k = 0; k < 6; ++k)
		{
			for (int32 i = 0; i < Cols; ++i)
			{
				const float U0 = float(i) / Cols;
				const float U1 = float(i + 1) / Cols;
				const FLinearColor Col = Mix(BandCol[k], Ink, 0.08f * KGMgCoast::Hash01(i * 7 + k * 31));
				P.Quad(HullPt(U0, Bands[k]), HullPt(U1, Bands[k]), HullPt(U1, Bands[k + 1]), HullPt(U0, Bands[k + 1]), Col);
			}
		}
		// Clinker seams, gunwale, stem and transom.
		for (int32 k = 1; k < 6; ++k)
		{
			TArray<FVector2f> Seam;
			TArray<FVector2f> Lap;
			for (int32 i = 0; i <= Cols; ++i)
			{
				const FVector2f Q = HullPt(float(i) / Cols, Bands[k]);
				Seam.Add(Q);
				Lap.Add(Q + FVector2f(0.0f, 2.5f));
			}
			P.Line(Seam, A(Ink, 0.5f), 2.0f);
			P.Line(Lap, A(Cream, 0.2f), 1.5f);
		}
		TArray<FVector2f> Gun;
		for (int32 i = 0; i <= Cols; ++i)
		{
			Gun.Add(HullPt(float(i) / Cols, 0.0f));
		}
		P.Line(Gun, WoodDark, 9.0f);
		P.Line(Gun, WoodLight, 2.5f);
		P.Bar(HullPt(0.0f, 0.0f) + FVector2f(0.0f, -6.0f), HullPt(0.0f, 1.0f), 9.0f, WoodDark);
		P.Bar(HullPt(1.0f, 0.0f), HullPt(1.0f, 1.0f), 11.0f, WoodDark);
		P.Text(HullPt(0.2f, 0.075f) + FVector2f(0.0f, -7.0f), TEXT("MORROW GULL"), 11.0f, Cream, 0.5f, TEXT("Black"), 60);
	}

	void PaintBucket(FKGMgPainter& P) const
	{
		const FVector2f B = BucketC;
		P.Disc(B + FVector2f(0.0f, 40.0f), FVector2f(56.0f, 10.0f), A(Ink, 0.45f), A(Ink, 0.0f));
		P.Arc(B + FVector2f(0.0f, -30.0f), 46.0f, PI * 1.05f, PI * 1.95f, Iron, 3.0f, 16);
		P.Quad(B + FVector2f(-44.0f, -30.0f), B + FVector2f(44.0f, -30.0f), B + FVector2f(36.0f, 38.0f), B + FVector2f(-36.0f, 38.0f), C(0x3A3D45));
		P.Bar(B + FVector2f(-42.0f, -14.0f), B + FVector2f(42.0f, -14.0f), 5.0f, C(0x5A5F6A));
		P.Bar(B + FVector2f(-37.0f, 26.0f), B + FVector2f(37.0f, 26.0f), 5.0f, C(0x5A5F6A));
		P.Poly(KGMgCoast::Ellipse(B + FVector2f(0.0f, -30.0f), FVector2f(44.0f, 11.0f), 0.0f, 24), C(0x5A5F6A));
		P.Poly(KGMgCoast::Ellipse(B + FVector2f(0.0f, -29.0f), FVector2f(38.0f, 8.0f), 0.0f, 24), C(0x1A120C));
		P.Poly(KGMgCoast::Ellipse(B + FVector2f(-10.0f, -31.0f), FVector2f(12.0f, 2.5f), 0.0f, 12), A(Cream, 0.3f));
		P.Text(B + FVector2f(0.0f, -6.0f), TEXT("TAR"), 13.0f, A(Cream, 0.85f), 0.5f, TEXT("Black"), 80);
		for (int32 k = 0; k < 3; ++k)
		{
			TArray<FVector2f> Wisp;
			for (int32 j = 0; j <= 8; ++j)
			{
				const float Y = -38.0f - j * 7.0f;
				Wisp.Add(B + FVector2f(-16.0f + k * 16.0f + FMath::Sin(Time * 2.0f + Y * 0.08f + k) * 5.0f, Y));
			}
			P.Line(Wisp, A(Cream, 0.18f), 3.0f);
		}
	}

	void PaintBrush(FKGMgPainter& P) const
	{
		FVector2f Tip = Mouse;
		if (DipT >= 0.0f)
		{
			Tip = BucketC + FVector2f(0.0f, -32.0f + 26.0f * FMath::Sin(PI * Saturate(DipT / DipTime)));
		}
		const FVector2f D = KGMgCoast::Unit(-1.0f);
		const FVector2f N(-D.Y, D.X);
		const FLinearColor Bristle = Mix(C(0xD8B98A), C(0x2A1A10), Saturate(Load * 1.5f));
		P.Bar(Tip + D * 28.0f, Tip + D * 84.0f, 9.0f, Wood);
		P.Bar(Tip + D * 30.0f + N * 2.0f, Tip + D * 82.0f + N * 2.0f, 2.0f, A(Cream, 0.3f));
		P.Bar(Tip + D * 16.0f, Tip + D * 30.0f, 14.0f, Iron);
		P.Quad(Tip + N * 10.0f, Tip - N * 10.0f, Tip + D * 17.0f - N * 7.0f, Tip + D * 17.0f + N * 7.0f, Bristle);
		for (int32 b = -2; b <= 2; ++b)
		{
			P.Segment(Tip + N * (b * 3.5f) + D * 2.0f, Tip + N * (b * 2.5f) + D * 15.0f, A(Ink, 0.25f), 1.0f);
		}
		if (Load > 0.3f && DipT < 0.0f && !bMouseDown)
		{
			P.Circle(Tip + FVector2f(0.0f, 5.0f + Ping(Time, 1.2f) * 7.0f), 3.0f, C(0x2A1A10));
		}
	}
};

// =====================================================================================================================
// LightHarbourLamp: swipe the match fast across the striker, then carry the flame up through the hatch to the wick.
// =====================================================================================================================
class FKGMgLightHarbourLamp final : public FKGMinigame
{
public:
	virtual int32 NumStages() const override { return 2; }

	virtual FString GetInstruction() const override
	{
		return Stage == 0 ? TEXT("Grab the match and swipe it FAST all the way across the striker")
		                  : TEXT("Bring the flame up through the hatch to the wick and hold it there - mind the wind");
	}

	virtual void BeginStage() override
	{
		bHeld = false;
		bInStrip = false;
		Heat = 0.0f;
		FlareT = -1.0f;
		LastSwipe = 0.0f;
		LastSwipeT = -10.0f;
		Sparks.Reset();
		ScrapeAcc = 0.0f;
		HintCD = 0.0f;
		AutoT = 0.0f;
		HeadOff = FVector2f::ZeroVector;
		Burn = 0.0f;
		Catch = 0.0f;
		LitT = -1.0f;
		NewMatchT = -1.0f;
		GustAge = -1.0f;
		GustCD = 1.0f + Rng.FRand() * 0.6f;
		GustDir = 1.0f;
		GustLen = 1.2f;
		WindOff = FVector2f::ZeroVector;
		BumpCD = 0.0f;
		CrackleT = 0.0f;
		FlamePos = Mouse;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (Stage == 0 && FlareT < 0.0f)
		{
			const FVector2f Head = MatchHead();
			if (FVector2f::Distance(Pos, Head) < 40.0f || DistToSegment(Pos, Head, Head + StickDir * 140.0f) < 22.0f)
			{
				bHeld = true;
				HeadOff = Head - Pos;
				Sound(TEXT("S_UI_Click"), 0.5f, 1.2f);
			}
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override
	{
		if (Stage == 0)
		{
			if (bInStrip)
			{
				EndSwipe();
			}
			if (FlareT < 0.0f)
			{
				bHeld = false;
			}
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (Stage != 0 || !bHeld || FlareT >= 0.0f)
		{
			return;
		}
		const FVector2f Head = Pos + HeadOff;
		const bool bIn = InRect(Head, StripPos - FVector2f(10.0f, 12.0f), StripSize + FVector2f(20.0f, 24.0f));
		if (bIn && (!bInStrip || StageTime - SwipeLastT > 0.15f))
		{
			// Entering the strip, or moving again after a pause on it: a new swipe starts here.
			bInStrip = true;
			SwipeT0 = StageTime;
			SwipeMin = Head.X;
			SwipeMax = Head.X;
		}
		if (bIn)
		{
			if (FMath::Abs(Delta.X) > 0.5f)
			{
				SwipeLastT = StageTime;
			}
			SwipeMin = FMath::Min(SwipeMin, Head.X);
			SwipeMax = FMath::Max(SwipeMax, Head.X);
			ScrapeAcc += FMath::Abs(Delta.X);
			if (ScrapeAcc > 40.0f)
			{
				ScrapeAcc = 0.0f;
				Sound(TEXT("S_Chore_Scrape"), 0.3f, 1.1f);
			}
		}
		else if (bInStrip)
		{
			EndSwipe();
		}
	}

	virtual void Tick(float Dt) override
	{
		if (Stage == 0)
		{
			TickStrike(Dt);
		}
		else
		{
			TickLight(Dt);
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (Stage == 0)
		{
			if (FlareT >= 0.0f)
			{
				return;
			}
			AutoT += Dt;
			const float Cyc = FMath::Fmod(AutoT, 0.9f);
			const FVector2f S0 = StripPos + FVector2f(-24.0f, StripSize.Y * 0.5f);
			const FVector2f S1 = StripPos + FVector2f(StripSize.X + 24.0f, StripSize.Y * 0.5f);
			const FVector2f Target = Cyc < 0.6f ? FMath::Lerp(Rest, S0, Saturate(Cyc / 0.5f)) : FMath::Lerp(S0, S1, Saturate((Cyc - 0.6f) / 0.18f));
			bHeld = true;
			HeadOff = FVector2f::ZeroVector;
			const FVector2f Old = Mouse;
			Mouse = Target;
			OnMove(Target, Target - Old);
		}
		else if (NewMatchT < 0.0f && LitT < 0.0f)
		{
			// Line up under the hatch first, then rise straight up to the wick.
			const bool bLined = FMath::Abs(FlamePos.X - WickTop.X) < 14.0f && FlamePos.Y > WickTop.Y;
			const FVector2f Aim = InHatch(FlamePos) || bLined ? WickTop + FVector2f(0.0f, 12.0f) : FVector2f(WickTop.X, 320.0f);
			Mouse = FMath::Lerp(Mouse, Aim - WindOff, 1.0f - FMath::Exp(-5.0f * Dt));
		}
	}

	virtual void DebugPose() override
	{
		if (Stage == 0)
		{
			bHeld = true;
			HeadOff = FVector2f::ZeroVector;
			Mouse = StripPos + FVector2f(StripSize.X * 0.55f, StripSize.Y * 0.5f);
			Heat = 0.55f;
			LastSwipe = 560.0f;
			LastSwipeT = StageTime;
			Sparks.Reset();
			for (int32 i = 0; i < 8; ++i)
			{
				Sparks.Add({Mouse + FVector2f(-10.0f - i * 6.0f, -4.0f - (i % 3) * 6.0f), FVector2f(-60.0f, -80.0f), 0.1f + i * 0.04f});
			}
		}
		else
		{
			Burn = 0.35f;
			Catch = 0.55f;
			GustDir = -1.0f;
			GustLen = 1.4f;
			GustAge = 0.7f;
			WindOff = FVector2f(-18.0f, -3.0f);
			FlamePos = WickTop + FVector2f(3.0f, 12.0f);
			Mouse = FlamePos - WindOff;
			NewMatchT = -1.0f;
			LitT = -1.0f;
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		if (Stage == 0)
		{
			PaintStrike(P);
		}
		else
		{
			PaintLight(P);
		}
	}

private:
	struct FSpark
	{
		FVector2f Pos;
		FVector2f Vel;
		float Age;
	};

	static constexpr float StrikeSpeed = 650.0f;   // px/s across the strip
	static constexpr float MinStrike = 1.0f;
	static constexpr float MinLight = 3.0f;
	static constexpr float CatchTime = 2.5f;   // seconds the flame must hold at the wick
	static constexpr float MatchLife = 10.0f;
	const FVector2f StripPos = FVector2f(160.0f, 228.0f);
	const FVector2f StripSize = FVector2f(320.0f, 36.0f);
	const FVector2f Rest = FVector2f(118.0f, 246.0f);
	const FVector2f StickDir = FVector2f(-0.62f, 0.78f).GetSafeNormal();
	// Lantern (Light stage).
	const FVector2f LB = FVector2f(338.0f, 96.0f);
	const FVector2f LBSize = FVector2f(124.0f, 166.0f);
	const FVector2f Hatch = FVector2f(372.0f, 168.0f);
	const FVector2f HatchSize = FVector2f(56.0f, 104.0f);
	const FVector2f WickTop = FVector2f(400.0f, 190.0f);

	// Strike
	bool bHeld = false;
	FVector2f HeadOff = FVector2f::ZeroVector;
	bool bInStrip = false;
	float SwipeT0 = 0.0f;
	float SwipeMin = 0.0f;
	float SwipeMax = 0.0f;
	float SwipeLastT = -10.0f;
	float Heat = 0.0f;
	float FlareT = -1.0f;
	float LastSwipe = 0.0f;
	float LastSwipeT = -10.0f;
	TArray<FSpark> Sparks;
	float ScrapeAcc = 0.0f;
	float HintCD = 0.0f;
	float AutoT = 0.0f;
	// Light
	float Burn = 0.0f;
	float Catch = 0.0f;
	float LitT = -1.0f;
	float NewMatchT = -1.0f;
	float GustAge = -1.0f;
	float GustCD = 1.0f;
	float GustDir = 1.0f;
	float GustLen = 1.2f;
	FVector2f WindOff = FVector2f::ZeroVector;
	float BumpCD = 0.0f;
	float CrackleT = 0.0f;
	FVector2f FlamePos = FVector2f::ZeroVector;

	FVector2f MatchHead() const { return bHeld ? Mouse + HeadOff : Rest; }

	void EndSwipe()
	{
		bInStrip = false;
		const FVector2f Head = MatchHead();
		const float Dist = SwipeMax - SwipeMin;
		if (Dist < StripSize.X * 0.55f)
		{
			if (HintCD <= 0.0f)
			{
				HintCD = 1.2f;
				PopText(Head + FVector2f(0.0f, -44.0f), TEXT("All the way across!"), Gold);
			}
			return;
		}
		const float Dur = FMath::Max(SwipeLastT - SwipeT0, 1.0f / 60.0f);   // a stop at the end doesn't count
		const float Speed = Dist / Dur;
		LastSwipe = Speed;
		LastSwipeT = StageTime;
		if (Speed >= StrikeSpeed)
		{
			Heat += 0.6f + 0.4f * Saturate((Speed - StrikeSpeed) / 900.0f);
			for (int32 i = 0; i < 12; ++i)
			{
				Sparks.Add({Head, FVector2f((Rng.FRand() - 0.5f) * 260.0f, -60.0f - Rng.FRand() * 200.0f), 0.0f});
			}
			Sound(TEXT("S_Chore_Scrape"), 0.9f, 1.4f);
			if (Heat >= 1.0f)
			{
				FlareT = 0.0f;
				Heat = 1.0f;
				Sound(TEXT("S_Chore_Match"), 1.0f, 1.0f);
				Nice(Head + FVector2f(0.0f, -56.0f), TEXT("It's lit!"), TEXT("S_Chore_Flame"), 1.0f);
			}
			else
			{
				PopText(Head + FVector2f(0.0f, -44.0f), TEXT("Sparks! Again!"), Gold);
			}
		}
		else
		{
			Heat = FMath::Min(0.5f, Heat + 0.08f);
			Sound(TEXT("S_Chore_Scrape"), 0.6f, 0.75f);
			if (HintCD <= 0.0f)
			{
				HintCD = 0.8f;
				Oops(Head + FVector2f(0.0f, -44.0f), TEXT("Faster!"), TEXT("S_UI_Click"), 2.0f);
			}
		}
	}

	void TickSparks(float Dt)
	{
		for (int32 i = Sparks.Num() - 1; i >= 0; --i)
		{
			FSpark& S = Sparks[i];
			S.Age += Dt;
			S.Vel.Y += 700.0f * Dt;
			S.Pos += S.Vel * Dt;
			if (S.Age > 0.6f)
			{
				Sparks.RemoveAt(i);
			}
		}
	}

	void TickStrike(float Dt)
	{
		HintCD = FMath::Max(0.0f, HintCD - Dt);
		TickSparks(Dt);
		if (bInStrip && FlareT < 0.0f && StageTime - SwipeLastT > 0.12f && SwipeMax - SwipeMin >= StripSize.X * 0.55f)
		{
			EndSwipe();   // swiped across and stopped on the strip: judge it now
		}
		if (FlareT < 0.0f)
		{
			Heat = FMath::Max(0.0f, Heat - Dt * 0.3f);
		}
		else
		{
			FlareT += Dt;
			if (FlareT >= 0.9f && StageTime >= MinStrike && !bSolved)
			{
				Solve();
			}
		}
	}

	bool InHatch(const FVector2f& Q) const { return InRect(Q, Hatch, HatchSize); }

	void TickLight(float Dt)
	{
		TickSparks(Dt);
		BumpCD = FMath::Max(0.0f, BumpCD - Dt);
		// Wind gusts.
		if (GustAge < 0.0f)
		{
			GustCD -= Dt;
			if (GustCD <= 0.0f && !bSolved)
			{
				GustAge = 0.0f;
				GustDir = Rng.FRand() < 0.5f ? -1.0f : 1.0f;
				GustLen = 1.1f + Rng.FRand() * 0.5f;
				Sound(TEXT("S_Chore_Whoosh"), 0.5f, 0.7f + Rng.FRand() * 0.2f);
			}
		}
		else
		{
			GustAge += Dt;
			if (GustAge >= GustLen)
			{
				GustAge = -1.0f;
				GustCD = 1.4f + Rng.FRand() * 1.2f;
			}
		}
		const float Env = GustAge >= 0.0f ? FMath::Sin(PI * GustAge / GustLen) : 0.0f;
		const float Shield = InHatch(FlamePos) ? 0.35f : 1.0f;
		WindOff.X = Approach(WindOff.X, GustDir * 75.0f * Env * Shield, Dt, 4.0f);
		WindOff.Y = Approach(WindOff.Y, -8.0f * Env * Shield, Dt, 4.0f);
		FlamePos = Mouse + WindOff;

		if (NewMatchT >= 0.0f)
		{
			NewMatchT += Dt;
			if (NewMatchT >= 1.1f && Burn > 0.0f)
			{
				Burn = 0.0f;
				Sound(TEXT("S_Chore_Match"), 0.9f, 1.05f);
			}
			if (NewMatchT >= 1.5f)
			{
				NewMatchT = -1.0f;
			}
			return;
		}
		if (bSolved || LitT >= 0.0f)
		{
			if (LitT >= 0.0f)
			{
				LitT += Dt;
				if (LitT >= 0.5f && StageTime >= MinLight && !bSolved)
				{
					Solve();
				}
			}
			return;
		}
		Burn += Dt / MatchLife;
		if (InRect(FlamePos, LB, LBSize) && !InHatch(FlamePos))
		{
			if (BumpCD <= 0.0f)
			{
				BumpCD = 0.8f;
				Burn += 0.06f;
				Catch *= 0.5f;
				Oops(FlamePos + FVector2f(0.0f, -30.0f), TEXT("Mind the glass - use the hatch"), TEXT("S_UI_Bad"), 3.0f);
			}
		}
		const bool bNear = InHatch(FlamePos) && FVector2f::Distance(FlamePos, WickTop) < 28.0f;
		if (bNear)
		{
			Catch += Dt / CatchTime;
			CrackleT -= Dt;
			if (CrackleT <= 0.0f)
			{
				CrackleT = 0.4f;
				Sound(TEXT("S_Chore_Flame"), 0.35f, 0.8f + 0.5f * Catch);
			}
		}
		else
		{
			Catch = FMath::Max(0.0f, Catch - Dt * 0.5f);
		}
		if (Catch >= 1.0f)
		{
			Catch = 1.0f;
			LitT = 0.0f;
			Nice(WickTop + FVector2f(0.0f, -60.0f), TEXT("It caught!"), TEXT("S_Chore_Flame"), 1.1f);
		}
		else if (Burn >= 1.0f)
		{
			NewMatchT = 0.0f;
			Catch = 0.0f;
			Oops(FlamePos + FVector2f(0.0f, -40.0f), TEXT("Burnt out! Striking a new one..."), TEXT("S_UI_Bad"), 4.0f);
		}
	}

	// ---- painting --------------------------------------------------------------------------------------------------

	void PaintDusk(FKGMgPainter& P, float HorizonY) const
	{
		const float MidY = HorizonY * 0.55f;
		P.RectV(FVector2f(0, 0), FVector2f(W, MidY), C(0x3B2C66), C(0xC0607A));
		P.RectV(FVector2f(0, MidY), FVector2f(W, HorizonY - MidY), C(0xC0607A), C(0xFFB36B));
		KGMgCoast::Stars(P, FVector2f(0.0f, 0.0f), FVector2f(W, MidY * 0.7f), Time, 22, 5);
		P.Glow(FVector2f(140.0f, HorizonY), 150.0f, A(C(0xFFD27A), 0.6f));
		P.Circle(FVector2f(140.0f, HorizonY + 4.0f), 30.0f, C(0xFFD890));
		// Far quay houses on the right.
		for (int32 i = 0; i < 7; ++i)
		{
			const float X = 380.0f + i * 40.0f;
			const float Hh = 24.0f + KGMgCoast::Hash01(i + 40) * 26.0f;
			P.Rect(FVector2f(X, HorizonY - Hh), FVector2f(36.0f, Hh), C(0x3A2A52));
			P.Tri(FVector2f(X - 3.0f, HorizonY - Hh), FVector2f(X + 39.0f, HorizonY - Hh), FVector2f(X + 18.0f, HorizonY - Hh - 14.0f), C(0x3A2A52));
			if (KGMgCoast::Hash01(i + 60) > 0.35f)
			{
				P.Rect(FVector2f(X + 12.0f, HorizonY - Hh + 8.0f), FVector2f(7.0f, 7.0f), A(Lantern, 0.9f));
			}
		}
		KGMgCoast::Sea(P, FVector2f(0.0f, HorizonY), FVector2f(W, H - HorizonY), Time, C(0x8A5A7A), C(0x2A2146), C(0xFFC98B));
		for (int32 i = 0; i < 5; ++i)
		{
			const float Wd = 34.0f - i * 5.0f + 5.0f * FMath::Sin(Time * 1.8f + i);
			P.Bar(FVector2f(140.0f - Wd, HorizonY + 8.0f + i * 12.0f), FVector2f(140.0f + Wd, HorizonY + 8.0f + i * 12.0f), 3.0f, A(C(0xFFD27A), 0.55f));
		}
	}

	void PaintMatch(FKGMgPainter& P, const FVector2f& Head, float Charred, float Alpha) const
	{
		const FVector2f Tail = Head + StickDir * 140.0f;
		P.Bar(Head + FVector2f(4.0f, 6.0f), Tail + FVector2f(4.0f, 6.0f), 8.0f, A(Ink, 0.25f * Alpha));
		P.Bar(Head, Tail, 7.0f, A(C(0xE8C890), Alpha));
		P.Bar(Head + FVector2f(1.5f, 1.0f), Tail + FVector2f(1.5f, 1.0f), 2.0f, A(C(0xB8955A), Alpha));
		if (Charred > 0.0f)
		{
			P.Bar(Head, Head + StickDir * (8.0f + 110.0f * Charred), 7.0f, A(C(0x2A201C), Alpha));
			P.Bar(Head + StickDir * (8.0f + 110.0f * Charred) - StickDir * 3.0f, Head + StickDir * (8.0f + 110.0f * Charred), 7.0f,
			      A(C(0xFF7A1A), 0.8f * Alpha));
		}
		const float StickAng = FMath::Atan2(StickDir.Y, StickDir.X);
		const FLinearColor HeadC = Charred > 0.0f ? C(0x3A2A24) : Mix(C(0xC8102E), C(0xFF8A3A), Heat);
		P.Poly(KGMgCoast::Ellipse(Head, FVector2f(11.0f, 8.0f), StickAng, 16), A(HeadC, Alpha));
		P.Poly(KGMgCoast::Ellipse(Head - StickDir * 4.0f, FVector2f(4.0f, 3.0f), StickAng, 10), A(C(0xFFE0C0), 0.5f * Alpha));
	}

	void PaintSparks(FKGMgPainter& P) const
	{
		for (const FSpark& S : Sparks)
		{
			const float K = 1.0f - S.Age / 0.6f;
			P.Segment(S.Pos, S.Pos - S.Vel * 0.02f, A(Gold, K), 2.5f);
			P.Circle(S.Pos, 2.0f, A(C(0xFFF0B0), K));
		}
	}

	void PaintStrike(FKGMgPainter& P) const
	{
		PaintDusk(P, 200.0f);
		// Harbour wall top the matchbox lies on.
		P.RectV(FVector2f(0.0f, 290.0f), FVector2f(W, 110.0f), C(0x8A8096), C(0x4E4658));
		for (int32 i = 0; i < 9; ++i)
		{
			P.Rect(FVector2f(i * 76.0f + (i % 2) * 20.0f, 290.0f), FVector2f(2.0f, 110.0f), A(Ink, 0.3f));
		}
		P.Rect(FVector2f(0.0f, 290.0f), FVector2f(W, 3.0f), A(Cream, 0.35f));

		// Matchbox.
		P.Shadow(FVector2f(140.0f, 212.0f), FVector2f(360.0f, 160.0f), 12.0f, 0.5f, 8.0f);
		P.RoundRect(FVector2f(140.0f, 212.0f), FVector2f(360.0f, 160.0f), C(0xE9D8B0), 12.0f, C(0xB8A27A), 2.0f);
		P.RoundRect(FVector2f(160.0f, 276.0f), FVector2f(320.0f, 84.0f), C(0xB5483A), 8.0f, C(0x7A2A22), 2.0f);
		P.Text(FVector2f(348.0f, 284.0f), TEXT("HARBOUR"), 22.0f, Cream, 0.5f, TEXT("Black"), 80);
		P.Text(FVector2f(348.0f, 316.0f), TEXT("SAFETY MATCHES"), 11.0f, C(0xF6D8B0), 0.5f, TEXT("Bold"), 120);
		P.Tri(FVector2f(192.0f, 336.0f), FVector2f(236.0f, 336.0f), FVector2f(214.0f, 346.0f), Cream);
		P.Tri(FVector2f(214.0f, 334.0f), FVector2f(214.0f, 296.0f), FVector2f(236.0f, 330.0f), Cream);
		P.Tri(FVector2f(212.0f, 334.0f), FVector2f(212.0f, 304.0f), FVector2f(196.0f, 330.0f), A(Cream, 0.8f));
		// Striker strip.
		const float Hot = Saturate(Heat);
		P.RoundRect(StripPos, StripSize, Mix(C(0x5A3322), C(0x8A4020), Hot * 0.5f), 6.0f, C(0x3A2016), 2.0f);
		for (int32 i = 0; i < 70; ++i)
		{
			const FVector2f G = StripPos + FVector2f(6.0f + KGMgCoast::Hash01(i * 2) * (StripSize.X - 12.0f), 5.0f + KGMgCoast::Hash01(i * 2 + 1) * (StripSize.Y - 10.0f));
			P.Circle(G, 1.3f, A(C(0x9A6A4A), 0.8f));
		}
		if (!bHeld && FlareT < 0.0f)
		{
			P.HintArrow(StripPos + FVector2f(10.0f, -16.0f), StripPos + FVector2f(StripSize.X - 10.0f, -16.0f), Time, A(Gold, 0.95f));
			P.HintRing(Rest, 26.0f, Time, Gold);
		}

		// Match (+ flame once lit).
		const FVector2f Head = MatchHead();
		if (Heat > 0.05f && FlareT < 0.0f)
		{
			P.Glow(Head, 30.0f * Heat, A(Lantern, 0.7f * Heat));
		}
		PaintMatch(P, Head, FlareT >= 0.0f ? 0.05f + 0.05f * Saturate(FlareT) : 0.0f, 1.0f);
		if (FlareT >= 0.0f)
		{
			const float Burst = 1.0f - Saturate(FlareT / 0.5f);
			KGMgCoast::Flame(P, Head + FVector2f(0.0f, -4.0f), 12.0f + 16.0f * Burst * Burst, 0.1f, Time);
			if (FlareT < 0.8f)
			{
				P.Circle(Head + FVector2f(6.0f, -30.0f - FlareT * 50.0f), 8.0f + FlareT * 16.0f, A(C(0xBFB8C8), 0.5f * (1.0f - FlareT / 0.8f)));
			}
		}
		PaintSparks(P);

		// Swipe speed gauge (last swipe).
		P.RoundRect(FVector2f(16.0f, 14.0f), FVector2f(230.0f, 48.0f), A(Ink, 0.7f), 8.0f);
		P.Text(FVector2f(28.0f, 18.0f), TEXT("SWIPE SPEED"), 10.0f, CreamDim, 0.0f, TEXT("Bold"), 80);
		const float Fade = Saturate(1.6f - (StageTime - LastSwipeT));
		const float Fill = Saturate(LastSwipe / (StrikeSpeed * 2.0f)) * Fade;
		P.Gauge(FVector2f(28.0f, 34.0f), FVector2f(206.0f, 16.0f), Fill, LastSwipe >= StrikeSpeed ? Good : Gold, 0.5f, 1.0f);
	}

	void PaintLight(FKGMgPainter& P) const
	{
		const float Lit = LitT >= 0.0f ? Saturate(LitT / 0.5f) : 0.0f;
		PaintDusk(P, 300.0f);
		// Quay edge.
		P.RectV(FVector2f(0.0f, 346.0f), FVector2f(W, 54.0f), C(0x8A8096), C(0x4E4658));
		P.Rect(FVector2f(0.0f, 346.0f), FVector2f(W, 3.0f), A(Cream, 0.35f));
		for (int32 i = 0; i < 9; ++i)
		{
			P.Rect(FVector2f(i * 76.0f + (i % 2) * 20.0f, 346.0f), FVector2f(2.0f, 54.0f), A(Ink, 0.3f));
		}

		// Lamp post with a curled arm; the lantern hangs from it.
		const FLinearColor PostC = C(0x2A2430);
		P.Bar(FVector2f(214.0f, 400.0f), FVector2f(214.0f, 60.0f), 18.0f, PostC);
		P.RoundRect(FVector2f(198.0f, 330.0f), FVector2f(32.0f, 20.0f), PostC, 4.0f);
		P.Bar(FVector2f(214.0f, 64.0f), FVector2f(410.0f, 64.0f), 9.0f, PostC);
		P.Arc(FVector2f(250.0f, 84.0f), 20.0f, PI * 1.0f, PI * 2.0f, PostC, 5.0f, 16);
		P.Arc(FVector2f(292.0f, 80.0f), 14.0f, PI * 1.0f, PI * 2.2f, PostC, 4.0f, 16);
		P.Circle(FVector2f(214.0f, 56.0f), 10.0f, PostC);
		P.Bar(FVector2f(400.0f, 64.0f), FVector2f(400.0f, 78.0f), 4.0f, PostC);
		// Roof.
		P.Quad(FVector2f(LB.X - 10.0f, LB.Y), FVector2f(LB.X + LBSize.X + 10.0f, LB.Y), FVector2f(428.0f, 78.0f), FVector2f(372.0f, 78.0f), PostC);
		P.Circle(FVector2f(400.0f, 76.0f), 6.0f, PostC);
		// Glass (warm once lit) + inside.
		if (Lit > 0.0f)
		{
			P.Glow(WickTop, 260.0f * Lit, A(C(0xFFC060), 0.55f * Lit));
		}
		P.RectV(LB, LBSize, A(C(0xBFD8E8), 0.18f + 0.35f * Lit), A(C(0xFFE0A0), 0.12f + 0.45f * Lit));
		P.Quad(LB + FVector2f(14.0f, 8.0f), LB + FVector2f(34.0f, 8.0f), LB + FVector2f(20.0f, LBSize.Y - 8.0f), LB + FVector2f(4.0f, LBSize.Y - 8.0f),
		       A(Cream, 0.18f));
		P.Bar(FVector2f(LB.X, WickTop.Y + 16.0f), FVector2f(390.0f, WickTop.Y + 16.0f), 4.0f, C(0xC8943A));
		P.RoundRect(FVector2f(386.0f, WickTop.Y + 8.0f), FVector2f(28.0f, 16.0f), C(0xC8943A), 4.0f);
		P.Rect(FVector2f(396.0f, WickTop.Y), FVector2f(8.0f, 10.0f), Cream);
		P.Rect(FVector2f(396.0f, WickTop.Y), FVector2f(8.0f, 3.0f), C(0x3A2A24));
		// Frame: posts, top band, bottom plate with the hatch gap.
		P.Rect(FVector2f(LB.X - 4.0f, LB.Y), FVector2f(8.0f, LBSize.Y), PostC);
		P.Rect(FVector2f(LB.X + LBSize.X - 4.0f, LB.Y), FVector2f(8.0f, LBSize.Y), PostC);
		P.Rect(FVector2f(LB.X - 4.0f, LB.Y), FVector2f(LBSize.X + 8.0f, 8.0f), PostC);
		P.Rect(FVector2f(LB.X - 8.0f, LB.Y + LBSize.Y), FVector2f(Hatch.X - LB.X + 8.0f, 10.0f), PostC);
		P.Rect(FVector2f(Hatch.X + HatchSize.X, LB.Y + LBSize.Y), FVector2f(LB.X + LBSize.X - Hatch.X - HatchSize.X + 8.0f, 10.0f), PostC);
		P.Rect(FVector2f(Hatch.X - 2.0f, Hatch.Y), FVector2f(3.0f, LB.Y + LBSize.Y - Hatch.Y), A(C(0xC8943A), 0.9f));
		P.Rect(FVector2f(Hatch.X + HatchSize.X - 1.0f, Hatch.Y), FVector2f(3.0f, LB.Y + LBSize.Y - Hatch.Y), A(C(0xC8943A), 0.9f));
		P.Rect(FVector2f(Hatch.X - 2.0f, Hatch.Y - 3.0f), FVector2f(HatchSize.X + 4.0f, 3.0f), A(C(0xC8943A), 0.9f));
		// Hatch door hanging open below.
		const float Bottom = LB.Y + LBSize.Y + 10.0f;
		P.Quad(FVector2f(Hatch.X, Bottom), FVector2f(Hatch.X + HatchSize.X, Bottom), FVector2f(Hatch.X + HatchSize.X + 6.0f, Bottom + 30.0f),
		       FVector2f(Hatch.X - 6.0f, Bottom + 30.0f), A(C(0xBFD8E8), 0.3f));
		P.Line({FVector2f(Hatch.X, Bottom), FVector2f(Hatch.X - 6.0f, Bottom + 30.0f), FVector2f(Hatch.X + HatchSize.X + 6.0f, Bottom + 30.0f),
		        FVector2f(Hatch.X + HatchSize.X, Bottom)},
		       C(0xC8943A), 3.0f);

		// Catch progress ring around the wick.
		if (LitT < 0.0f)
		{
			P.Arc(WickTop, 32.0f, 0.0f, 2.0f * PI, A(Cream, 0.25f), 3.0f, 32);
			if (Catch > 0.0f)
			{
				P.ArcBand(WickTop, 29.0f, 36.0f, -0.5f * PI, -0.5f * PI + 2.0f * PI * Catch, Gold, 32);
			}
		}
		else
		{
			KGMgCoast::Flame(P, WickTop + FVector2f(0.0f, 2.0f), 14.0f * EaseOutBack(Lit), 0.0f, Time, 1.6f);
		}

		// Wind streaks.
		const float Env = GustAge >= 0.0f ? FMath::Sin(PI * GustAge / GustLen) : 0.0f;
		if (Env > 0.02f)
		{
			for (int32 k = 0; k < 9; ++k)
			{
				const float Y = 40.0f + k * 36.0f + KGMgCoast::Hash01(k + 90) * 16.0f;
				const float X = FMath::Fmod(KGMgCoast::Hash01(k + 70) * 760.0f + GustDir * Time * 420.0f + 7600.0f, 760.0f) - 60.0f;
				const FVector2f S0(X, Y);
				const FVector2f S1(X + GustDir * 46.0f, Y);
				P.Bar(S0, S1, 2.5f, A(Cream, 0.55f * Env));
				P.Tri(S1 + FVector2f(GustDir * 10.0f, 0.0f), S1 + FVector2f(0.0f, -6.0f), S1 + FVector2f(0.0f, 6.0f), A(Cream, 0.55f * Env));
			}
			P.Tag(FVector2f(320.0f, 30.0f), GustDir > 0.0f ? TEXT("GUST  >>>") : TEXT("<<<  GUST"), A(Ink, 0.6f * Env + 0.1f), A(Cream, Env), 13.0f);
		}

		// The match: burning, burnt out, or a fresh one being struck.
		if (NewMatchT >= 0.0f)
		{
			const float Drop = Saturate(NewMatchT / 0.7f);
			PaintMatch(P, FlamePos + FVector2f(Drop * 30.0f, 380.0f * Drop * Drop), 1.0f, 1.0f - Drop);
			if (NewMatchT > 0.9f)
			{
				const float In = Saturate((NewMatchT - 0.9f) / 0.3f);
				PaintMatch(P, FlamePos + FVector2f(0.0f, 60.0f * (1.0f - In)), 0.0f, In);
				if (NewMatchT > 1.1f)
				{
					KGMgCoast::Flame(P, FlamePos + FVector2f(0.0f, -4.0f), 12.0f + 10.0f * (1.0f - Saturate((NewMatchT - 1.1f) / 0.4f)), 0.0f, Time);
				}
			}
			P.Tag(FVector2f(320.0f, 380.0f), TEXT("New match..."), A(Ink, 0.8f), Cream, 13.0f);
		}
		else if (LitT < 0.0f)
		{
			PaintMatch(P, FlamePos, 0.05f + 0.95f * Saturate(Burn), 1.0f);
			const float Lean = FMath::Clamp(WindOff.X / 60.0f, -1.0f, 1.0f) + FMath::Sin(Time * 9.0f) * 0.08f;
			KGMgCoast::Flame(P, FlamePos + FVector2f(0.0f, -4.0f), 12.0f * (Burn > 0.85f ? 0.7f + 0.3f * Ping(Time, 0.15f) : 1.0f), Lean, Time);
		}
		else
		{
			PaintMatch(P, FlamePos + FVector2f(0.0f, 30.0f), 0.05f + 0.95f * Saturate(Burn), 1.0f - Lit * 0.5f);
		}
		PaintSparks(P);

		// Hints + HUD.
		if (LitT < 0.0f && NewMatchT < 0.0f && Catch < 0.05f && !(InHatch(FlamePos) && FVector2f::Distance(FlamePos, WickTop) < 28.0f))
		{
			P.HintArrow(FVector2f(WickTop.X, 336.0f), FVector2f(WickTop.X, WickTop.Y + 30.0f), Time, A(Gold, 0.9f));
			P.HintRing(WickTop, 22.0f, Time, Gold);
		}
		P.RoundRect(FVector2f(16.0f, 14.0f), FVector2f(180.0f, 46.0f), A(Ink, 0.7f), 8.0f);
		P.Text(FVector2f(28.0f, 18.0f), TEXT("MATCH"), 10.0f, CreamDim, 0.0f, TEXT("Bold"), 80);
		const float Left = NewMatchT >= 0.0f ? Saturate((NewMatchT - 1.1f) / 0.4f) : 1.0f - Saturate(Burn);
		P.Gauge(FVector2f(28.0f, 34.0f), FVector2f(156.0f, 14.0f), Left, Left < 0.25f ? Crimson : Lantern);
		if (LitT < 0.0f && Catch > 0.02f)
		{
			P.Tag(WickTop + FVector2f(0.0f, -52.0f), TEXT("Hold still..."), A(Ink, 0.8f), Gold, 12.0f);
		}
	}
};

// =====================================================================================================================
// FeedKoi: toss pellets just ahead of the hungry koi (two bites each) - uneaten pellets cloud the pond.
// =====================================================================================================================
class FKGMgFeedKoi final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Click the water just ahead of a hungry koi to toss a pellet - two bites each");
	}

	virtual void BeginStage() override
	{
		Koi.Reset();
		Pellets.Reset();
		Ripples.Reset();
		Bubbles.Reset();
		Cloud = 0.0f;
		ThrowCD = 0.3f;
		NextId = 0;
		for (int32 i = 0; i < 5; ++i)
		{
			FKoi& K = Koi.AddDefaulted_GetRef();
			const float Ang = 2.0f * PI * i / 5.0f + Rng.FRand() * 0.5f;
			K.Pos = PondC + FVector2f(FMath::Cos(Ang) * PondR.X * 0.5f, FMath::Sin(Ang) * PondR.Y * 0.5f);
			K.Heading = Ang + 0.5f * PI;
			K.Speed = 50.0f;
			K.Pattern = i;
			K.Phase = Rng.FRand() * 6.28f;
			K.Scale = 0.9f + Rng.FRand() * 0.25f;
		}
		Pads.Reset();
		for (int32 i = 0; i < 4; ++i)
		{
			const float Ang = 2.0f * PI * (i + 0.3f) / 4.0f + Rng.FRand() * 0.6f;
			Pads.Add(FVector3f(PondC.X + FMath::Cos(Ang) * PondR.X * 0.72f, PondC.Y + FMath::Sin(Ang) * PondR.Y * 0.68f, Rng.FRand() * 6.28f));
		}
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		if (ThrowCD <= 0.0f && InPond(Pos, 0.93f))
		{
			Throw(Pos);
		}
		else if (!InPond(Pos, 0.93f))
		{
			PopText(Pos + FVector2f(0.0f, -24.0f), TEXT("Into the pond!"), CreamDim);
		}
	}

	virtual void Tick(float Dt) override
	{
		ThrowCD = FMath::Max(0.0f, ThrowCD - Dt);
		Cloud = FMath::Max(0.0f, Cloud - Dt * 0.035f);
		for (int32 i = Ripples.Num() - 1; i >= 0; --i)
		{
			Ripples[i].Z += Dt;
			if (Ripples[i].Z > 1.0f)
			{
				Ripples.RemoveAt(i);
			}
		}
		for (int32 i = Bubbles.Num() - 1; i >= 0; --i)
		{
			Bubbles[i].Z += Dt;
			if (Bubbles[i].Z > 1.2f)
			{
				Bubbles.RemoveAt(i);
			}
		}
		// Pellets: fly, land, float, sink.
		for (int32 i = Pellets.Num() - 1; i >= 0; --i)
		{
			FPellet& Pe = Pellets[i];
			Pe.Age += Dt;
			if (!Pe.bLanded)
			{
				if (Pe.Age >= FlightTime)
				{
					Pe.bLanded = true;
					Pe.Age = 0.0f;
					Ripples.Add(FVector3f(Pe.At.X, Pe.At.Y, 0.0f));
					Sound(TEXT("S_Chore_Splash"), 0.3f, 1.8f);
				}
				continue;
			}
			Pe.At += FVector2f(FMath::Sin(Time + Pe.Id) * 4.0f, 3.0f) * Dt;
			if (Pe.Age >= FloatTime && !bSolved)
			{
				Cloud += 0.26f;
				PopText(Pe.At + FVector2f(0.0f, -20.0f), TEXT("Sank..."), CreamDim);
				Sound(TEXT("S_UI_Bad"), 0.35f, 1.1f);
				RemovePellet(i);
			}
		}
		AssignTargets();
		TickKoi(Dt);
		if (Cloud >= 1.0f && !bSolved)
		{
			int32 Lost = 0;
			for (FKoi& K : Koi)
			{
				if (K.Bites > 0 && Lost < 2)
				{
					--K.Bites;
					++Lost;
				}
			}
			Cloud = 0.45f;
			Oops(PondC + FVector2f(0.0f, -40.0f), TEXT("Too cloudy - the koi lose interest"), TEXT("S_UI_Bad"), 5.0f);
		}
		if (!bSolved && FedCount() >= Koi.Num() && StageTime >= MinSolve)
		{
			Solve();
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		if (ThrowCD > 0.0f)
		{
			return;
		}
		for (const FKoi& K : Koi)
		{
			if (K.Bites < 2 && K.Target < 0)
			{
				FVector2f Aim = K.Pos + KGMgCoast::Unit(K.Heading) * 45.0f;
				if (!InPond(Aim, 0.85f))
				{
					Aim = FMath::Lerp(K.Pos, PondC, 0.3f);
				}
				Throw(Aim);
				return;
			}
		}
	}

	virtual void DebugPose() override
	{
		const int32 BitesPose[5] = {2, 2, 1, 0, 0};
		for (int32 i = 0; i < Koi.Num(); ++i)
		{
			Koi[i].Bites = BitesPose[i];
			Koi[i].Target = -1;
		}
		Pellets.Reset();
		Cloud = 0.38f;
		if (Koi.Num() >= 4)
		{
			FPellet& Pe = Pellets.AddDefaulted_GetRef();
			Pe.Id = NextId++;
			Pe.At = Koi[3].Pos + KGMgCoast::Unit(Koi[3].Heading) * 40.0f;
			Pe.bLanded = true;
			Pe.Age = 0.4f;
			Koi[3].Target = Pe.Id;
			Ripples = {FVector3f(Pe.At.X, Pe.At.Y, 0.4f)};
			FPellet& Fl = Pellets.AddDefaulted_GetRef();
			Fl.Id = NextId++;
			Fl.From = BagPos;
			Fl.At = Koi[4].Pos + KGMgCoast::Unit(Koi[4].Heading) * 50.0f;
			Fl.Age = FlightTime * 0.55f;
			Bubbles = {FVector3f(Koi[0].Pos.X, Koi[0].Pos.Y - 10.0f, 0.3f)};
		}
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		PaintGarden(P);
		// Water.
		P.Poly(KGMgCoast::Ellipse(PondC, PondR, 0.0f, 56), C(0x2E8A94));
		P.Disc(PondC + FVector2f(0.0f, 10.0f), PondR * 0.92f, C(0x14485A), A(C(0x2E8A94), 0.0f), 48);
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector2f At = PondC + FVector2f(FMath::Sin(Time * 0.3f + i * 1.7f) * PondR.X * 0.55f, FMath::Cos(Time * 0.23f + i * 2.3f) * PondR.Y * 0.5f);
			KGMgCoast::Ring(P, At, FVector2f(40.0f + i * 6.0f, 12.0f + i * 2.0f), A(Cream, 0.06f), 2.0f, 24);
		}
		// Koi (shadow first), under the cloudy veil and the pads.
		for (const FKoi& K : Koi)
		{
			PaintKoi(P, K, true);
		}
		for (const FKoi& K : Koi)
		{
			PaintKoi(P, K, false);
		}
		if (Cloud > 0.01f)
		{
			P.Poly(KGMgCoast::Ellipse(PondC, PondR * 0.99f, 0.0f, 56), A(C(0x8C8F5A), 0.5f * Saturate(Cloud)));
		}
		for (int32 i = 0; i < Pads.Num(); ++i)
		{
			PaintPad(P, FVector2f(Pads[i].X, Pads[i].Y), Pads[i].Z, 24.0f + (i % 2) * 6.0f, i == 1);
		}
		// Pellets, ripples, bubbles.
		for (const FVector3f& R : Ripples)
		{
			KGMgCoast::Ring(P, FVector2f(R.X, R.Y), FVector2f(8.0f + 36.0f * R.Z, 3.0f + 12.0f * R.Z), A(Cream, 0.8f * (1.0f - R.Z)), 2.0f, 24);
		}
		for (const FPellet& Pe : Pellets)
		{
			if (Pe.bLanded)
			{
				const float Sink = Saturate((Pe.Age - FloatTime + 0.5f) / 0.5f);
				P.Circle(Pe.At, 4.5f, A(C(0x8A5A2A), 1.0f - Sink * 0.7f), A(Ink, 0.5f), 1.0f);
				P.Circle(Pe.At + FVector2f(-1.2f, -1.2f), 1.4f, A(C(0xE8C890), 1.0f - Sink));
			}
			else
			{
				const float K = Saturate(Pe.Age / FlightTime);
				const FVector2f At = FMath::Lerp(Pe.From, Pe.At, K) + FVector2f(0.0f, -90.0f * FMath::Sin(K * PI));
				P.Circle(Pe.At, 3.0f + 3.0f * K, A(Ink, 0.2f * K));
				P.Circle(At, 5.0f, C(0x8A5A2A), A(Ink, 0.5f), 1.0f);
			}
		}
		for (const FVector3f& B : Bubbles)
		{
			const float K = B.Z / 1.2f;
			P.Circle(FVector2f(B.X + FMath::Sin(K * 9.0f) * 4.0f, B.Y - 30.0f * K), 3.0f + 3.0f * K, A(Cream, 0.0f), A(Cream, 0.8f * (1.0f - K)), 1.5f);
			P.Circle(FVector2f(B.X + 10.0f, B.Y - 10.0f - 36.0f * K), 3.5f, A(C(0xFF8FA8), 0.9f * (1.0f - K)));
		}
		// Drifting sakura petals.
		for (int32 i = 0; i < 12; ++i)
		{
			const float T = Time * 0.05f + KGMgCoast::Hash01(i + 300);
			const FVector2f At = PondC + FVector2f(FMath::Sin(T * 2.0f * PI + i) * PondR.X * 0.8f, FMath::Cos(T * 1.3f * PI + i * 2.0f) * PondR.Y * 0.75f);
			P.Poly(KGMgCoast::Ellipse(At, FVector2f(4.5f, 2.6f), Time * 0.4f + i, 10), A(C(0xF8C8D8), 0.95f));
		}
		PaintTree(P);

		// Bag of pellets with the re-arm ring.
		P.Shadow(BagPos + FVector2f(-24.0f, -20.0f), FVector2f(48.0f, 46.0f), 12.0f, 0.3f, 4.0f);
		P.Poly({BagPos + FVector2f(-22.0f, 24.0f), BagPos + FVector2f(-26.0f, -4.0f), BagPos + FVector2f(-12.0f, -22.0f), BagPos + FVector2f(12.0f, -22.0f),
		        BagPos + FVector2f(26.0f, -4.0f), BagPos + FVector2f(22.0f, 24.0f)},
		       C(0xC9A574));
		P.Poly(KGMgCoast::Ellipse(BagPos + FVector2f(0.0f, -20.0f), FVector2f(13.0f, 4.0f), 0.0f, 14), C(0x6A4428));
		P.Bar(BagPos + FVector2f(-14.0f, -14.0f), BagPos + FVector2f(14.0f, -14.0f), 3.0f, C(0x8A6A3A));
		P.Text(BagPos + FVector2f(0.0f, -2.0f), TEXT("FOOD"), 10.0f, C(0x6A4428), 0.5f, TEXT("Black"));
		if (ThrowCD > 0.0f)
		{
			P.ArcBand(BagPos, 32.0f, 36.0f, -0.5f * PI, -0.5f * PI + 2.0f * PI * (1.0f - ThrowCD / ThrowGap), A(Cream, 0.8f), 24);
		}

		// HUD: fed counter + cloudiness.
		P.RoundRect(FVector2f(14.0f, 10.0f), FVector2f(120.0f, 52.0f), A(Ink, 0.72f), 10.0f);
		P.Text(FVector2f(74.0f, 14.0f), TEXT("KOI FED"), 10.0f, CreamDim, 0.5f, TEXT("Bold"), 100);
		P.Text(FVector2f(74.0f, 27.0f), FString::Printf(TEXT("%d / %d"), FedCount(), Koi.Num()), 22.0f, Cream, 0.5f, TEXT("Black"));
		P.RoundRect(FVector2f(454.0f, 10.0f), FVector2f(172.0f, 46.0f), A(Ink, 0.72f), 10.0f);
		P.Text(FVector2f(466.0f, 14.0f), TEXT("CLOUDY WATER"), 10.0f, CreamDim, 0.0f, TEXT("Bold"), 80);
		P.Gauge(FVector2f(466.0f, 30.0f), FVector2f(148.0f, 16.0f), Cloud, Cloud > 0.7f ? Crimson : C(0xB8B070));

		if (FedCount() == 0 && Pellets.Num() == 0 && Koi.Num() > 0)
		{
			const FKoi& K = Koi[0];
			P.HintRing(K.Pos + KGMgCoast::Unit(K.Heading) * 45.0f, 22.0f, Time, Gold);
		}
	}

private:
	struct FKoi
	{
		FVector2f Pos = FVector2f::ZeroVector;   // head
		float Heading = 0.0f;
		float Speed = 50.0f;
		int32 Pattern = 0;
		int32 Bites = 0;
		int32 Target = -1;
		float Phase = 0.0f;
		float Scale = 1.0f;
		float HappyT = 0.0f;
	};

	struct FPellet
	{
		int32 Id = 0;
		FVector2f From = FVector2f::ZeroVector;
		FVector2f At = FVector2f::ZeroVector;
		float Age = 0.0f;
		bool bLanded = false;
	};

	static constexpr float FlightTime = 0.35f;
	static constexpr float FloatTime = 1.8f;
	static constexpr float ThrowGap = 0.6f;   // one pellet every 0.6 s: 10 bites need >= ~6 s
	static constexpr float MinSolve = 6.0f;
	const FVector2f PondC = FVector2f(316.0f, 214.0f);
	const FVector2f PondR = FVector2f(262.0f, 148.0f);
	const FVector2f BagPos = FVector2f(598.0f, 360.0f);

	TArray<FKoi> Koi;
	TArray<FPellet> Pellets;
	TArray<FVector3f> Ripples;   // X, Y, age
	TArray<FVector3f> Bubbles;   // X, Y, age
	TArray<FVector3f> Pads;      // X, Y, notch angle
	float Cloud = 0.0f;
	float ThrowCD = 0.0f;
	int32 NextId = 0;

	bool InPond(const FVector2f& Q, float Frac) const
	{
		const FVector2f Rel = Q - PondC;
		return FMath::Square(Rel.X / (PondR.X * Frac)) + FMath::Square(Rel.Y / (PondR.Y * Frac)) < 1.0f;
	}

	int32 FedCount() const
	{
		int32 N = 0;
		for (const FKoi& K : Koi)
		{
			N += K.Bites >= 2 ? 1 : 0;
		}
		return N;
	}

	void Throw(const FVector2f& To)
	{
		FPellet& Pe = Pellets.AddDefaulted_GetRef();
		Pe.Id = NextId++;
		Pe.From = BagPos + FVector2f(0.0f, -20.0f);
		Pe.At = To;
		ThrowCD = ThrowGap;
		Sound(TEXT("S_Chore_Whoosh"), 0.35f, 1.5f);
	}

	void RemovePellet(int32 Index)
	{
		const int32 Id = Pellets[Index].Id;
		Pellets.RemoveAt(Index);
		for (FKoi& K : Koi)
		{
			if (K.Target == Id)
			{
				K.Target = -1;
			}
		}
	}

	FPellet* FindPellet(int32 Id)
	{
		return Pellets.FindByPredicate([Id](const FPellet& Pe) { return Pe.Id == Id; });
	}

	void AssignTargets()
	{
		for (const FPellet& Pe : Pellets)
		{
			if (!Pe.bLanded)
			{
				continue;
			}
			bool bClaimed = false;
			for (const FKoi& K : Koi)
			{
				bClaimed |= K.Target == Pe.Id;
			}
			if (bClaimed)
			{
				continue;
			}
			FKoi* Best = nullptr;
			float BestD = 210.0f;
			for (FKoi& K : Koi)
			{
				const float D = FVector2f::Distance(K.Pos, Pe.At);
				if (K.Bites < 2 && K.Target < 0 && D < BestD)
				{
					Best = &K;
					BestD = D;
				}
			}
			if (Best)
			{
				Best->Target = Pe.Id;
			}
		}
	}

	void TickKoi(float Dt)
	{
		for (int32 i = 0; i < Koi.Num(); ++i)
		{
			FKoi& K = Koi[i];
			K.HappyT = FMath::Max(0.0f, K.HappyT - Dt);
			const FPellet* Goal = K.Target >= 0 ? FindPellet(K.Target) : nullptr;
			if (K.Target >= 0 && !Goal)
			{
				K.Target = -1;
			}
			float WantSpeed = K.Bites >= 2 ? 34.0f : 58.0f;
			if (Goal)
			{
				// Arrive: slow down and turn tighter when close, so a koi can't circle a pellet forever.
				const FVector2f To = Goal->At - K.Pos;
				const float Near = Saturate((To.Size() - 20.0f) / 80.0f);
				K.Heading = KGMgCoast::TurnToward(K.Heading, FMath::Atan2(To.Y, To.X), FMath::Lerp(7.0f, 4.5f, Near) * Dt);
				WantSpeed = FMath::Lerp(70.0f, 150.0f, Near);
			}
			else
			{
				K.Heading += (0.6f * FMath::Sin(Time * 0.5f + K.Phase) + 0.3f * FMath::Sin(Time * 1.3f + K.Phase * 2.0f)) * Dt;
			}
			// Keep inside the pond.
			const FVector2f Rel = K.Pos - PondC;
			const float Edge = FMath::Square(Rel.X / (PondR.X - 45.0f)) + FMath::Square(Rel.Y / (PondR.Y - 38.0f));
			if (Edge > 0.7f && !Goal)
			{
				K.Heading = KGMgCoast::TurnToward(K.Heading, FMath::Atan2(-Rel.Y, -Rel.X), (Edge - 0.7f) * 6.0f * Dt + 0.4f * Dt);
			}
			// Separation.
			for (int32 j = 0; j < Koi.Num(); ++j)
			{
				if (j != i)
				{
					const FVector2f Away = K.Pos - Koi[j].Pos;
					const float D = Away.Size();
					if (D < 34.0f && D > 0.01f)
					{
						K.Pos += Away / D * (34.0f - D) * Dt * 2.0f;
					}
				}
			}
			K.Speed = Approach(K.Speed, WantSpeed, Dt, 3.0f);
			K.Pos += KGMgCoast::Unit(K.Heading) * K.Speed * Dt;
			if (!InPond(K.Pos, 0.9f))
			{
				K.Pos = PondC + (K.Pos - PondC) * 0.99f;
			}
			// Eat the target pellet (or any unclaimed one right under the nose).
			for (int32 p = Pellets.Num() - 1; p >= 0; --p)
			{
				const FPellet& Pe = Pellets[p];
				if (!Pe.bLanded || K.Bites >= 2 || FVector2f::Distance(K.Pos, Pe.At) > 18.0f)
				{
					continue;
				}
				bool bOther = false;
				for (int32 j = 0; j < Koi.Num(); ++j)
				{
					bOther |= j != i && Koi[j].Target == Pe.Id;
				}
				if (bOther && K.Target != Pe.Id)
				{
					continue;
				}
				const FVector2f At = Pe.At;
				RemovePellet(p);
				K.Target = -1;
				++K.Bites;
				K.HappyT = 1.0f;
				Ripples.Add(FVector3f(At.X, At.Y, 0.2f));
				Bubbles.Add(FVector3f(At.X, At.Y - 6.0f, 0.0f));
				Nice(At + FVector2f(0.0f, -24.0f), K.Bites >= 2 ? TEXT("Fed!") : TEXT("Yum!"), TEXT("S_Chore_Splash"), 1.3f + 0.08f * FedCount());
				break;
			}
		}
	}

	static void KoiColors(int32 Pattern, int32 Seg, FLinearColor& Base, FLinearColor& Patch, bool& bPatch)
	{
		const FLinearColor White = C(0xF8F4EC);
		const FLinearColor Red = C(0xE8502A);
		const FLinearColor Black = C(0x221C22);
		bPatch = false;
		switch (Pattern % 5)
		{
		case 0:   // Kohaku
			Base = White;
			Patch = Red;
			bPatch = Seg == 1 || Seg == 2 || Seg == 4;
			break;
		case 1:   // Showa
			Base = Black;
			Patch = Seg == 5 ? White : C(0xF07A28);
			bPatch = Seg == 0 || Seg == 2 || Seg == 3 || Seg == 5;
			break;
		case 2:   // Ogon
			Base = C(0xF2B830);
			Patch = C(0xFFE08A);
			bPatch = Seg == 1 || Seg == 3;
			break;
		case 3:   // Tancho
			Base = White;
			Patch = Red;
			bPatch = Seg == 0;
			break;
		default:   // Sanke
			Base = White;
			Patch = Seg == 2 || Seg == 5 ? Black : Red;
			bPatch = Seg == 1 || Seg == 2 || Seg == 3 || Seg == 5;
			break;
		}
	}

	void PaintKoi(FKGMgPainter& P, const FKoi& K, bool bShadow) const
	{
		static const float Radii[] = {7.0f, 9.0f, 9.5f, 9.0f, 7.5f, 6.0f, 4.5f};
		const FVector2f D = KGMgCoast::Unit(K.Heading);
		const FVector2f N(-D.Y, D.X);
		const float Sp = 7.0f * K.Scale;
		const float Swim = Time * (4.0f + K.Speed * 0.05f) + K.Phase;
		const FVector2f Off = bShadow ? FVector2f(6.0f, 9.0f) : FVector2f::ZeroVector;
		FVector2f Seg[7];
		for (int32 i = 0; i < 7; ++i)
		{
			Seg[i] = K.Pos + Off - D * (Sp * i) + N * (FMath::Sin(Swim - i * 0.7f) * i * 0.9f * K.Scale);
		}
		const FVector2f TailD = (Seg[6] - Seg[5]).GetSafeNormal();
		const FVector2f TailN(-TailD.Y, TailD.X);
		if (bShadow)
		{
			for (int32 i = 0; i < 7; ++i)
			{
				P.Circle(Seg[i], Radii[i] * K.Scale, A(Ink, 0.18f));
			}
			return;
		}
		FLinearColor Base, Patch;
		bool bPatch = false;
		KoiColors(K.Pattern, 0, Base, Patch, bPatch);
		// Fins (translucent) then the tail fan.
		const float Flap = FMath::Sin(Swim * 1.3f) * 0.3f;
		const float HeadAng = K.Heading;
		P.Poly(KGMgCoast::Ellipse(Seg[1] + N * 11.0f * K.Scale, FVector2f(8.0f, 4.0f) * K.Scale, HeadAng + 2.2f + Flap, 12), A(Base, 0.7f));
		P.Poly(KGMgCoast::Ellipse(Seg[1] - N * 11.0f * K.Scale, FVector2f(8.0f, 4.0f) * K.Scale, HeadAng - 2.2f - Flap, 12), A(Base, 0.7f));
		P.Tri(Seg[6], Seg[6] + TailD * 16.0f * K.Scale + TailN * 10.0f * K.Scale, Seg[6] + TailD * 12.0f * K.Scale, A(Base, 0.75f));
		P.Tri(Seg[6], Seg[6] + TailD * 12.0f * K.Scale, Seg[6] + TailD * 16.0f * K.Scale - TailN * 10.0f * K.Scale, A(Base, 0.75f));
		for (int32 i = 6; i >= 0; --i)
		{
			P.Circle(Seg[i], Radii[i] * K.Scale, Base);
		}
		for (int32 i = 6; i >= 0; --i)
		{
			KoiColors(K.Pattern, i, Base, Patch, bPatch);
			if (bPatch)
			{
				P.Circle(Seg[i] + N * FMath::Sin(i * 2.1f + K.Pattern) * 2.0f, Radii[i] * K.Scale * 0.72f, Patch);
			}
		}
		P.Line({Seg[0], Seg[2], Seg[4]}, A(Cream, 0.3f), 2.0f);
		P.Circle(Seg[0] + D * 3.0f * K.Scale + N * 4.0f * K.Scale, 1.6f, Ink);
		P.Circle(Seg[0] + D * 3.0f * K.Scale - N * 4.0f * K.Scale, 1.6f, Ink);
		// Appetite pips.
		const FVector2f Pip = K.Pos + FVector2f(0.0f, -24.0f);
		for (int32 b = 0; b < 2; ++b)
		{
			const FVector2f At = Pip + FVector2f(b == 0 ? -7.0f : 7.0f, 0.0f);
			const bool bOn = b < K.Bites;
			P.Circle(At, 5.5f, bOn ? Gold : A(Ink, 0.55f), bOn ? A(Ink, 0.6f) : A(Cream, 0.8f), 1.5f);
		}
		if (K.Bites >= 2)
		{
			P.Glow(Pip, 18.0f, A(Gold, 0.4f + 0.2f * K.HappyT));
		}
		else if (K.Target >= 0)
		{
			P.Arc(K.Pos + D * 4.0f, 10.0f + 3.0f * Ping(Time, 0.4f), 0.0f, 2.0f * PI, A(Cream, 0.6f), 1.5f, 16);
		}
	}

	static void PaintPad(FKGMgPainter& P, const FVector2f& At, float Notch, float R, bool bFlower)
	{
		P.Poly(KGMgCoast::Ellipse(At + FVector2f(3.0f, 4.0f), FVector2f(R, R * 0.8f), 0.0f, 20), A(Ink, 0.18f));
		const FLinearColor PadC = C(0x4E9A3E);
		TArray<FVector2f> Half0 = KGMgCoast::EllipseCap(At, FVector2f(R, R * 0.8f), Notch + 0.35f, Notch + PI, 0.0f, 10);
		Half0.Add(At);
		TArray<FVector2f> Half1 = KGMgCoast::EllipseCap(At, FVector2f(R, R * 0.8f), Notch + PI, Notch + 2.0f * PI - 0.35f, 0.0f, 10);
		Half1.Add(At);
		P.Poly(Half0, PadC);
		P.Poly(Half1, PadC);
		for (int32 v = 0; v < 5; ++v)
		{
			const float Va = Notch + 0.9f + v * 1.1f;
			P.Segment(At, At + FVector2f(FMath::Cos(Va) * R * 0.85f, FMath::Sin(Va) * R * 0.68f), A(C(0x3A7A2E), 0.8f), 1.2f);
		}
		if (bFlower)
		{
			for (int32 k = 0; k < 6; ++k)
			{
				const float Pa = k * PI / 3.0f;
				P.Poly(KGMgCoast::Ellipse(At + KGMgCoast::Unit(Pa) * 7.0f, FVector2f(7.0f, 3.5f), Pa, 10), C(0xF6B8C8));
			}
			P.Circle(At, 4.0f, Gold);
		}
	}

	void PaintGarden(FKGMgPainter& P) const
	{
		P.RectV(FVector2f(0, 0), FVector2f(W, H), C(0x86B85A), C(0x5E8E3E));
		for (int32 i = 0; i < 40; ++i)
		{
			const FVector2f At(KGMgCoast::Hash01(i * 2 + 500) * W, KGMgCoast::Hash01(i * 2 + 501) * H);
			P.Circle(At, 3.0f + KGMgCoast::Hash01(i + 600) * 4.0f, A(C(0x9ACB6A), 0.5f));
		}
		// Raked gravel corner + a stone lantern.
		P.Poly(KGMgCoast::Ellipse(FVector2f(40.0f, 380.0f), FVector2f(140.0f, 70.0f), 0.0f, 28), C(0xE0D6C0));
		for (int32 r = 1; r < 5; ++r)
		{
			P.Arc(FVector2f(40.0f, 380.0f), 26.0f * r, PI * 1.35f, PI * 2.0f, A(C(0xB8AC90), 0.8f), 1.5f, 20);
		}
		const FVector2f L0(44.0f, 70.0f);
		P.Rect(L0 + FVector2f(-6.0f, 10.0f), FVector2f(12.0f, 36.0f), Stone);
		P.RoundRect(L0 + FVector2f(-18.0f, -8.0f), FVector2f(36.0f, 20.0f), C(0x9A98A2), 3.0f);
		P.Rect(L0 + FVector2f(-6.0f, -4.0f), FVector2f(12.0f, 10.0f), A(Lantern, 0.8f));
		P.Tri(L0 + FVector2f(-26.0f, -8.0f), L0 + FVector2f(26.0f, -8.0f), L0 + FVector2f(0.0f, -26.0f), StoneDark);
		P.RoundRect(L0 + FVector2f(-16.0f, 44.0f), FVector2f(32.0f, 8.0f), StoneDark, 3.0f);
		// Rim stones.
		for (int32 i = 0; i < 36; ++i)
		{
			const float Ang = 2.0f * PI * i / 36.0f;
			const FVector2f At = PondC + FVector2f(FMath::Cos(Ang) * (PondR.X + 8.0f), FMath::Sin(Ang) * (PondR.Y + 8.0f));
			const float R = 13.0f + KGMgCoast::Hash01(i + 100) * 9.0f;
			const float Tone = KGMgCoast::Hash01(i + 200);
			P.Circle(At + FVector2f(2.0f, 3.0f), R, A(Ink, 0.25f));
			P.Circle(At, R, Mix(StoneDark, C(0xAEACB6), Tone));
			P.Circle(At + FVector2f(-R * 0.3f, -R * 0.3f), R * 0.35f, A(Cream, 0.18f));
		}
	}

	void PaintTree(FKGMgPainter& P) const
	{
		// Sakura branch reaching over the pond's corner.
		const FLinearColor Bark = C(0x4A2F2A);
		P.Bar(FVector2f(660.0f, -10.0f), FVector2f(560.0f, 56.0f), 14.0f, Bark);
		P.Bar(FVector2f(560.0f, 56.0f), FVector2f(500.0f, 70.0f), 8.0f, Bark);
		P.Bar(FVector2f(600.0f, 30.0f), FVector2f(590.0f, 96.0f), 6.0f, Bark);
		static const float Blobs[][3] = {{500.0f, 64.0f, 18.0f}, {528.0f, 48.0f, 22.0f}, {560.0f, 40.0f, 24.0f}, {596.0f, 20.0f, 26.0f},
		                                 {630.0f, 44.0f, 24.0f}, {590.0f, 92.0f, 18.0f}, {612.0f, 72.0f, 20.0f}, {548.0f, 70.0f, 16.0f}};
		for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(Blobs)); ++i)
		{
			const FVector2f At(Blobs[i][0], Blobs[i][1]);
			P.Circle(At + FVector2f(3.0f, 4.0f), Blobs[i][2], A(Ink, 0.12f));
			P.Circle(At, Blobs[i][2], i % 2 ? C(0xF6B8C8) : C(0xF09AB2));
			P.Circle(At + FVector2f(-Blobs[i][2] * 0.3f, -Blobs[i][2] * 0.3f), Blobs[i][2] * 0.45f, A(C(0xFFE4EC), 0.7f));
		}
	}
};

// =====================================================================================================================
// HarvestCarrots: carrot tops pop up - drag the leafy ones UP to pull them, leave the rotten ones alone (8 carrots).
// =====================================================================================================================
class FKGMgHarvestCarrots final : public FKGMinigame
{
public:
	virtual FString GetInstruction() const override
	{
		return TEXT("Grab a green carrot top and drag it UP - leave the grey rotten ones alone");
	}

	virtual void BeginStage() override
	{
		Holes.Reset();
		for (int32 r = 0; r < 3; ++r)
		{
			const float Y = RowY[r];
			const float Sc = RowScale[r];
			for (int32 c = 0; c < 4; ++c)
			{
				FHole& Hl = Holes.AddDefaulted_GetRef();
				Hl.At = FVector2f(290.0f + (c - 1.5f) * 112.0f * Sc, Y);
				Hl.Scale = Sc;
			}
		}
		// Spawn plan: every block of four has exactly one rotten carrot (so 8 good ones need >= 11 pop-ups).
		Plan.Reset();
		for (int32 b = 0; b < 12; ++b)
		{
			const int32 Bad = Rng.RandRange(0, 3);
			for (int32 k = 0; k < 4; ++k)
			{
				Plan.Add(k == Bad);
			}
		}
		PlanIdx = 0;
		SpawnT = 0.8f;
		Harvested = 0;
		GrabHole = -1;
		GrabY = 0.0f;
		Flyers.Reset();
		LastHole = -1;
		AutoT = 0.0f;
	}

	virtual void OnPress(const FVector2f& Pos) override
	{
		for (int32 i = Holes.Num() - 1; i >= 0; --i)
		{
			FHole& Hl = Holes[i];
			if ((Hl.State != EHole::Rise && Hl.State != EHole::Up) || FVector2f::Distance(Pos, TopPoint(i)) > 36.0f)
			{
				continue;
			}
			if (Hl.bRotten)
			{
				const bool bHad = Harvested > 0;
				Harvested = FMath::Max(0, Harvested - 1);
				Hl.State = EHole::Sink;
				Hl.T = 0.0f;
				Hl.Splat = 1.0f;
				Oops(TopPoint(i) + FVector2f(0.0f, -30.0f), bHad ? TEXT("Rotten! -1 carrot") : TEXT("Rotten! Yuck"), TEXT("S_Chore_Squish"), 6.0f);
			}
			else
			{
				GrabHole = i;
				GrabY = Pos.Y;
				Sound(TEXT("S_UI_Click"), 0.5f, 1.2f);
			}
			return;
		}
	}

	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) override
	{
		if (!Holes.IsValidIndex(GrabHole))
		{
			return;
		}
		Holes[GrabHole].Pull = FMath::Clamp(GrabY - Pos.Y, 0.0f, 60.0f);
		if (Holes[GrabHole].Pull >= PullNeeded)
		{
			Harvest(GrabHole);
		}
	}

	virtual void OnRelease(const FVector2f& Pos) override { GrabHole = -1; }

	virtual void Tick(float Dt) override
	{
		// Pop-ups.
		if (!bSolved)
		{
			SpawnT -= Dt;
			if (SpawnT <= 0.0f)
			{
				int32 Up = 0;
				TArray<int32> Free;
				for (int32 i = 0; i < Holes.Num(); ++i)
				{
					if (Holes[i].State == EHole::Empty)
					{
						if (i != LastHole)
						{
							Free.Add(i);
						}
					}
					else
					{
						++Up;
					}
				}
				if (Up < 3 && Free.Num() > 0)
				{
					const int32 Pick = Free[Rng.RandRange(0, Free.Num() - 1)];
					FHole& Hl = Holes[Pick];
					Hl.State = EHole::Rise;
					Hl.T = 0.0f;
					Hl.Pull = 0.0f;
					Hl.Splat = 0.0f;
					Hl.bRotten = Plan.IsValidIndex(PlanIdx) ? Plan[PlanIdx] : Rng.FRand() < 0.25f;
					Hl.Seed = PlanIdx;
					++PlanIdx;
					LastHole = Pick;
					SpawnT = SpawnGap;
					Sound(TEXT("S_Chore_Pluck"), 0.25f, 0.6f + Rng.FRand() * 0.2f);
				}
				else
				{
					SpawnT = 0.1f;
				}
			}
		}
		for (int32 i = 0; i < Holes.Num(); ++i)
		{
			FHole& Hl = Holes[i];
			Hl.Splat = FMath::Max(0.0f, Hl.Splat - Dt * 1.5f);
			if (i != GrabHole)
			{
				Hl.Pull = Approach(Hl.Pull, 0.0f, Dt, 12.0f);
			}
			if (Hl.State == EHole::Empty)
			{
				continue;
			}
			if (i == GrabHole && Hl.State == EHole::Up)
			{
				continue;   // held: it doesn't duck back while you pull
			}
			Hl.T += Dt;
			if (Hl.State == EHole::Rise && Hl.T >= RiseTime)
			{
				Hl.State = EHole::Up;
				Hl.T = 0.0f;
			}
			else if (Hl.State == EHole::Up && Hl.T >= UpTime)
			{
				Hl.State = EHole::Sink;
				Hl.T = 0.0f;
			}
			else if (Hl.State == EHole::Sink && Hl.T >= SinkTime)
			{
				Hl.State = EHole::Empty;
				Hl.T = 0.0f;
			}
		}
		for (int32 i = Flyers.Num() - 1; i >= 0; --i)
		{
			Flyers[i].Age += Dt;
			if (Flyers[i].Age >= 0.55f)
			{
				Sound(TEXT("S_Chore_Thud"), 0.4f, 1.2f);
				Flyers.RemoveAt(i);
			}
		}
		if (!bSolved && Harvested >= Goal && Flyers.Num() == 0 && StageTime >= MinSolve)
		{
			Solve();
		}
	}

	virtual void AutoPlay(float Dt) override
	{
		AutoT -= Dt;
		if (AutoT > 0.0f)
		{
			return;
		}
		for (int32 i = 0; i < Holes.Num(); ++i)
		{
			if (Holes[i].State == EHole::Up && !Holes[i].bRotten && Holes[i].T > 0.25f)
			{
				Mouse = TopPoint(i);
				Harvest(i);
				AutoT = 0.3f;
				return;
			}
		}
	}

	virtual void DebugPose() override
	{
		for (FHole& Hl : Holes)
		{
			Hl.State = EHole::Empty;
			Hl.Pull = 0.0f;
		}
		Harvested = 5;
		auto Pop = [this](int32 Index, bool bRot, float Pull)
		{
			FHole& Hl = Holes[Index];
			Hl.State = EHole::Up;
			Hl.T = 0.4f;
			Hl.bRotten = bRot;
			Hl.Pull = Pull;
			Hl.Seed = Index;
		};
		Pop(1, false, 0.0f);
		Pop(6, true, 0.0f);
		Pop(9, false, 20.0f);
		GrabHole = 9;
		Mouse = TopPoint(9);
		Flyers = {{TopPoint(4), 0.3f, 1.0f}};
	}

	virtual void Paint(FKGMgPainter& P) const override
	{
		KGMgCoast::Outdoor(P, 150.0f, C(0x8ED0F2), C(0xF4EBD2));
		// Barn + fence.
		P.Rect(FVector2f(40.0f, 70.0f), FVector2f(90.0f, 80.0f), C(0xB5483A));
		P.Quad(FVector2f(30.0f, 72.0f), FVector2f(140.0f, 72.0f), FVector2f(110.0f, 40.0f), FVector2f(60.0f, 40.0f), C(0x6E2A21));
		P.Rect(FVector2f(66.0f, 104.0f), FVector2f(38.0f, 46.0f), C(0x7A2A22));
		P.Line({FVector2f(66.0f, 104.0f), FVector2f(104.0f, 150.0f)}, Cream, 3.0f);
		P.Line({FVector2f(104.0f, 104.0f), FVector2f(66.0f, 150.0f)}, Cream, 3.0f);
		P.RectV(FVector2f(0.0f, 150.0f), FVector2f(W, H - 150.0f), C(0x86C35A), C(0x6DA848));
		for (int32 i = 0; i < 17; ++i)
		{
			const float X = 8.0f + i * 40.0f;
			P.Rect(FVector2f(X, 138.0f), FVector2f(8.0f, 36.0f), WoodLight);
			P.Tri(FVector2f(X, 138.0f), FVector2f(X + 8.0f, 138.0f), FVector2f(X + 4.0f, 132.0f), WoodLight);
		}
		P.Rect(FVector2f(0.0f, 146.0f), FVector2f(W, 5.0f), Wood);
		P.Rect(FVector2f(0.0f, 162.0f), FVector2f(W, 5.0f), Wood);

		// The bed.
		P.Quad(FVector2f(84.0f, 170.0f), FVector2f(496.0f, 170.0f), FVector2f(556.0f, 396.0f), FVector2f(24.0f, 396.0f), C(0x7A5234));
		for (int32 r = 0; r < 3; ++r)
		{
			const float Y = RowY[r];
			const float Sc = RowScale[r];
			P.RoundRect(FVector2f(290.0f - 230.0f * Sc, Y - 14.0f * Sc), FVector2f(460.0f * Sc, 26.0f * Sc), C(0x8A6040), -1.0f);
		}
		for (int32 i = 0; i < 26; ++i)
		{
			const FVector2f At(60.0f + KGMgCoast::Hash01(i + 700) * 460.0f, 180.0f + KGMgCoast::Hash01(i + 800) * 210.0f);
			P.Circle(At, 2.0f + KGMgCoast::Hash01(i + 900) * 3.0f, A(C(0x5A3A22), 0.7f));
		}

		// Holes + carrots, back rows first.
		for (int32 i = 0; i < Holes.Num(); ++i)
		{
			PaintHole(P, i);
		}

		// Basket.
		PaintBasket(P);
		for (const FFlyer& F : Flyers)
		{
			const float K = Saturate(F.Age / 0.55f);
			const FVector2f At = FMath::Lerp(F.From, BasketTop, K) + FVector2f(0.0f, -150.0f * FMath::Sin(K * PI));
			KGMgCoast::Carrot(P, At, 0.9f, F.Spin * K * 5.0f, Time, 3);
		}

		// Hints.
		if (Harvested == 0 && GrabHole < 0)
		{
			for (int32 i = 0; i < Holes.Num(); ++i)
			{
				if (Holes[i].State == EHole::Up && !Holes[i].bRotten)
				{
					const FVector2f Tp = TopPoint(i);
					P.HintRing(Tp, 30.0f, Time, Gold);
					P.HintArrow(Tp + FVector2f(40.0f, 10.0f), Tp + FVector2f(40.0f, -50.0f), Time, A(Gold, 0.95f));
					break;
				}
			}
		}
		if (Holes.IsValidIndex(GrabHole))
		{
			const FVector2f Tp = TopPoint(GrabHole);
			P.GaugeV(Tp + FVector2f(34.0f, -20.0f), FVector2f(14.0f, 56.0f), Holes[GrabHole].Pull / PullNeeded, Good);
		}

		// Counter.
		P.RoundRect(FVector2f(506.0f, 10.0f), FVector2f(124.0f, 52.0f), A(Ink, 0.72f), 10.0f);
		P.Text(FVector2f(568.0f, 14.0f), TEXT("CARROTS"), 10.0f, CreamDim, 0.5f, TEXT("Bold"), 100);
		P.Text(FVector2f(568.0f, 27.0f), FString::Printf(TEXT("%d / %d"), Harvested, Goal), 22.0f, Harvested >= Goal ? Good : Cream, 0.5f,
		       TEXT("Black"));
	}

private:
	enum class EHole : uint8
	{
		Empty,
		Rise,
		Up,
		Sink
	};

	struct FHole
	{
		FVector2f At = FVector2f::ZeroVector;
		float Scale = 1.0f;
		EHole State = EHole::Empty;
		float T = 0.0f;
		bool bRotten = false;
		float Pull = 0.0f;
		float Splat = 0.0f;
		int32 Seed = 0;
	};

	struct FFlyer
	{
		FVector2f From;
		float Age;
		float Spin;
	};

	static constexpr int32 Goal = 8;
	static constexpr float RiseTime = 0.18f;
	static constexpr float UpTime = 1.6f;
	static constexpr float SinkTime = 0.25f;
	static constexpr float SpawnGap = 0.8f;   // >= 10 pop-ups needed for 8 good ones -> >= 8 s even for a perfect player
	static constexpr float PullNeeded = 34.0f;
	static constexpr float MinSolve = 8.0f;
	static constexpr float RowY[3] = {210.0f, 278.0f, 352.0f};
	static constexpr float RowScale[3] = {0.8f, 0.9f, 1.0f};
	const FVector2f BasketTop = FVector2f(590.0f, 258.0f);

	TArray<FHole> Holes;
	TArray<bool> Plan;
	int32 PlanIdx = 0;
	float SpawnT = 0.0f;
	int32 Harvested = 0;
	int32 GrabHole = -1;
	float GrabY = 0.0f;
	TArray<FFlyer> Flyers;
	int32 LastHole = -1;
	float AutoT = 0.0f;

	float RiseOf(const FHole& Hl) const
	{
		switch (Hl.State)
		{
		case EHole::Rise: return EaseOutBack(Saturate(Hl.T / RiseTime));
		case EHole::Up: return 1.0f;
		case EHole::Sink: return 1.0f - Saturate(Hl.T / SinkTime);
		default: return 0.0f;
		}
	}

	FVector2f TopPoint(int32 Index) const
	{
		const FHole& Hl = Holes[Index];
		return Hl.At + FVector2f(0.0f, -(RiseOf(Hl) * 30.0f + 16.0f) * Hl.Scale - Hl.Pull);
	}

	void Harvest(int32 Index)
	{
		FHole& Hl = Holes[Index];
		Flyers.Add({TopPoint(Index) + FVector2f(0.0f, 16.0f), 0.0f, Rng.FRand() < 0.5f ? -1.0f : 1.0f});
		Hl.State = EHole::Empty;
		Hl.T = 0.0f;
		Hl.Pull = 0.0f;
		if (GrabHole == Index)
		{
			GrabHole = -1;
		}
		++Harvested;
		Nice(Hl.At + FVector2f(0.0f, -70.0f), Harvested >= Goal ? TEXT("Basket full!") : TEXT("+1"), TEXT("S_Chore_Pluck"), 0.9f + 0.06f * Harvested);
	}

	void PaintHole(FKGMgPainter& P, int32 Index) const
	{
		const FHole& Hl = Holes[Index];
		const float Sc = Hl.Scale;
		P.Poly(KGMgCoast::Ellipse(Hl.At, FVector2f(26.0f, 10.0f) * Sc, 0.0f, 20), C(0x3A2416));
		const float Rise = RiseOf(Hl);
		if (Rise > 0.01f)
		{
			float Wob = 0.0f;
			if (Hl.State == EHole::Up && Hl.T > UpTime - 0.4f && Index != GrabHole)
			{
				Wob = FMath::Sin(Time * 40.0f) * 0.08f;   // about to duck back down
			}
			const FVector2f Shoulder = Hl.At + FVector2f(0.0f, -Rise * 30.0f * Sc - Hl.Pull);
			const TArray<FVector2f> Body = KGMgCoast::ClipAbove(KGMgCoast::CarrotBody(Shoulder, Sc, Wob), Hl.At.Y + 1.0f);
			if (Body.Num() >= 3)
			{
				P.Poly(Body, Hl.bRotten ? C(0x7A5230) : C(0xF28C28));
			}
			if (Hl.bRotten)
			{
				P.Circle(Shoulder + FVector2f(-5.0f, 3.0f) * Sc, 3.0f * Sc, C(0x3A2416));
				P.Circle(Shoulder + FVector2f(5.0f, 7.0f) * Sc, 2.5f * Sc, A(C(0x6A8A4A), 0.9f));
			}
			else
			{
				P.Bar(Shoulder + FVector2f(-6.0f, 2.0f) * Sc, Shoulder + FVector2f(-5.0f, FMath::Min(24.0f, (Hl.At.Y - Shoulder.Y) / Sc - 2.0f)) * Sc, 3.0f * Sc,
				      A(C(0xFFC680), 0.7f));
			}
			const FVector2f LeafBase = Shoulder + FVector2f(0.0f, -5.0f * Sc);
			KGMgCoast::CarrotLeaves(P, LeafBase, Sc, Wob, Hl.bRotten, Time, Hl.Seed);
			if (Hl.bRotten)
			{
				for (int32 f = 0; f < 3; ++f)
				{
					const FVector2f Fly = LeafBase + FVector2f(FMath::Cos(Time * 7.0f + f * 2.1f) * 16.0f, -22.0f + FMath::Sin(Time * 9.0f + f) * 8.0f) * Sc;
					P.Poly(KGMgCoast::Ellipse(Fly + FVector2f(-2.0f, -2.5f), FVector2f(2.5f, 1.5f), -0.5f, 8), A(Cream, 0.7f));
					P.Poly(KGMgCoast::Ellipse(Fly + FVector2f(2.0f, -2.5f), FVector2f(2.5f, 1.5f), 0.5f, 8), A(Cream, 0.7f));
					P.Circle(Fly, 2.3f, Ink);
				}
				for (int32 s = 0; s < 2; ++s)
				{
					TArray<FVector2f> Stink;
					for (int32 j = 0; j <= 5; ++j)
					{
						Stink.Add(LeafBase + FVector2f(-10.0f + s * 20.0f + FMath::Sin(Time * 4.0f + j + s) * 3.0f, -10.0f - j * 5.0f) * Sc);
					}
					P.Line(Stink, A(C(0x9AA060), 0.5f), 2.0f);
				}
			}
		}
		if (Hl.Splat > 0.0f)
		{
			for (int32 k = 0; k < 7; ++k)
			{
				const FVector2f Dd = KGMgCoast::Unit(k * 0.9f - PI);
				P.Circle(Hl.At + FVector2f(0.0f, -24.0f) + Dd * (1.0f - Hl.Splat) * 40.0f, 4.0f * Hl.Splat, A(C(0x5C4A2A), Hl.Splat));
			}
		}
		// Front lip of soil hides the buried part.
		P.Poly(KGMgCoast::EllipseCap(Hl.At, FVector2f(30.0f, 13.0f) * Sc, 0.0f, PI, 0.0f, 14), C(0x8A6040));
		P.Arc(Hl.At, 26.0f * Sc, 0.15f, PI - 0.15f, A(C(0xA87A50), 0.8f), 2.0f, 12);
	}

	void PaintBasket(FKGMgPainter& P) const
	{
		const FVector2f B = BasketTop;
		P.Disc(B + FVector2f(0.0f, 84.0f), FVector2f(54.0f, 9.0f), A(Ink, 0.35f), A(Ink, 0.0f));
		P.Arc(B + FVector2f(0.0f, 4.0f), 40.0f, PI * 1.1f, PI * 1.9f, C(0xA8773A), 5.0f, 16);
		P.Poly(KGMgCoast::Ellipse(B, FVector2f(46.0f, 10.0f), 0.0f, 20), C(0x5A3A1E));
		for (int32 n = 0; n < FMath::Min(Harvested, 12); ++n)
		{
			const float Hx = KGMgCoast::Hash01(n + 1000);
			const FVector2f At = B + FVector2f(-32.0f + (n % 5) * 16.0f + Hx * 4.0f, 2.0f - (n / 5) * 7.0f);
			KGMgCoast::Carrot(P, At, 0.55f, (Hx - 0.5f) * 0.5f, Time, n);
		}
		P.Quad(B + FVector2f(-46.0f, 0.0f), B + FVector2f(46.0f, 0.0f), B + FVector2f(36.0f, 78.0f), B + FVector2f(-36.0f, 78.0f), C(0xC8964E));
		for (int32 r = 1; r < 5; ++r)
		{
			const float Y = r * 15.6f;
			const float Hw = 46.0f - 10.0f * (Y / 78.0f);
			P.Bar(B + FVector2f(-Hw, Y), B + FVector2f(Hw, Y), 2.0f, A(C(0x8A5A2A), 0.8f));
		}
		for (int32 c = -3; c <= 3; ++c)
		{
			P.Segment(B + FVector2f(c * 13.0f, 2.0f), B + FVector2f(c * 10.0f, 76.0f), A(C(0x8A5A2A), 0.6f), 2.0f);
		}
		P.Bar(B + FVector2f(-48.0f, 0.0f), B + FVector2f(48.0f, 0.0f), 7.0f, C(0xA8773A));
	}
};

} // namespace KGMg


TUniquePtr<FKGMinigame> KGMakeMinigame_UnloadFish() { return MakeUnique<KGMg::FKGMgUnloadFish>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_FuelLighthouse() { return MakeUnique<KGMg::FKGMgFuelLighthouse>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_FixBoat() { return MakeUnique<KGMg::FKGMgFixBoat>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_LightHarbourLamp() { return MakeUnique<KGMg::FKGMgLightHarbourLamp>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_FeedKoi() { return MakeUnique<KGMg::FKGMgFeedKoi>(); }
TUniquePtr<FKGMinigame> KGMakeMinigame_HarvestCarrots() { return MakeUnique<KGMg::FKGMgHarvestCarrots>(); }
