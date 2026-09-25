#include "UI/KGHUD.h"
#include "Audio/KGAudio.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Character/KGCharacter.h"
#include "Character/KGViewmodelComponent.h"
#include "Combat/KGHealthComponent.h"
#include "Emote/KGEmoteComponent.h"
#include "Fishing/KGFishingComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryUI.h"
#include "Inventory/KGItemCatalog.h"
#include "Rendering/SlateRenderer.h"
#include "Roles/KGRoleListGenerator.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"
#include "UI/Reveal/KGRevealSubsystem.h"
#include "UI/Reveal/KGStreamerMode.h"
#include "World/KGInteractable.h"
#include "World/KGMapInfo.h"
#include "World/KGTaskStation.h"

/*
 * Canvas HUD, "vibrant stylised" pass. Every shape is built from triangles with a 1 px feathered skirt, so panels get
 * real anti-aliased rounded corners, soft drop shadows and glows without any texture assets. Geometry is in screen
 * pixels (callers multiply design units by S = ClipY / 1080); text sizes are design pixels at 1080p and are turned
 * into Slate font points, so text stays crisp at every resolution instead of being bitmap-scaled.
 */

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarHudDemo(
		TEXT("kg.HUDDemo"), 0,
		TEXT("Dev HUD preview: 1 = animated loop (hits, stamina, chore, prompt), 2 = frozen showcase (hurt, exhausted, ")
		TEXT("working a chore), 3 = frozen showcase (healthy, interact prompt, attack ring), 4 = epilogue card, ")
		TEXT("5 = backstab-ready crosshair."));

	int32 HudDemo() { return CVarHudDemo.GetValueOnGameThread(); }
#else
	int32 HudDemo() { return 0; }
#endif

	// ---------------------------------------------------------------------------------------------------------------
	// Palette
	// ---------------------------------------------------------------------------------------------------------------
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); }
	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * A); }

	const FLinearColor Cream = Srgb(255, 244, 222);          // #FFF4DE body text
	const FLinearColor Gold = Srgb(255, 184, 77);            // #FFB84D accent
	const FLinearColor GoldLight = Srgb(255, 224, 150);
	const FLinearColor TownColor = Srgb(110, 205, 120);      // village green
	const FLinearColor TownLight = Srgb(170, 236, 160);
	const FLinearColor Crimson = Srgb(200, 16, 46);          // #C8102E impatient crimson (fills, vignettes)
	const FLinearColor ImpatientColor = Srgb(255, 72, 94);   // crimson lifted so it reads as text on dark ink
	const FLinearColor NeutralColor = Srgb(182, 150, 242);   // neutral violet
	const FLinearColor NightColor = Srgb(140, 172, 255);
	const FLinearColor DawnColor = Srgb(255, 146, 92);
	const FLinearColor GhostColor = Srgb(110, 225, 235);
	const FLinearColor InkTop = Srgb(40, 31, 58);            // panels: warm-dark ink gradient
	const FLinearColor InkBottom = Srgb(19, 14, 29);
	const FLinearColor InkText = Srgb(40, 28, 44);           // text printed on cream keycaps / bright chips

	float Saturate(float X) { return FMath::Clamp(X, 0.0f, 1.0f); }

	/** Brighter/darker without losing saturation (scales linear RGB, keeps alpha). */
	FLinearColor Shade(const FLinearColor& C, float K)
	{
		return FLinearColor(FMath::Min(C.R * K, 1.0f), FMath::Min(C.G * K, 1.0f), FMath::Min(C.B * K, 1.0f), C.A);
	}

	float EaseOutCubic(float T)
	{
		const float U = 1.0f - Saturate(T);
		return 1.0f - U * U * U;
	}

	float EaseOutBack(float T)
	{
		const float U = Saturate(T) - 1.0f;
		return 1.0f + 2.70158f * U * U * U + 1.70158f * U * U;
	}

	/** Lub-dub heartbeat envelope in [0,1]; Rate in beats per second. */
	float Heartbeat(double Time, float Rate)
	{
		const float Ph = static_cast<float>(FMath::Frac(Time * Rate));
		auto Pulse = [](float X) { return FMath::Exp(-X * X / 0.0036f); };
		return FMath::Min(1.0f, Pulse(Ph - 0.08f) + 0.6f * Pulse(Ph - 0.3f));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Geometry
	// ---------------------------------------------------------------------------------------------------------------
	using FTris = TArray<FCanvasUVTri>;
	using FPts = TArray<FVector2D, TInlineAllocator<64>>;

	void AddTri(FTris& T, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA,
	            const FLinearColor& CB, const FLinearColor& CC)
	{
		FCanvasUVTri& Tri = T.AddDefaulted_GetRef();
		Tri.V0_Pos = A;
		Tri.V1_Pos = B;
		Tri.V2_Pos = C;
		Tri.V0_Color = CA;
		Tri.V1_Color = CB;
		Tri.V2_Color = CC;
	}

	void AddQuad(FTris& T, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
	             const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD)
	{
		AddTri(T, A, B, C, CA, CB, CC);
		AddTri(T, A, C, D, CA, CC, CD);
	}

	/** Per-vertex outward offsets (mitred, clamped so spikes stay tame) for skirts and strokes. Either winding. */
	void OutwardOffsets(TConstArrayView<FVector2D> P, FPts& Out)
	{
		const int32 N = P.Num();
		double Area = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = P[i];
			const FVector2D& B = P[(i + 1) % N];
			Area += A.X * B.Y - B.X * A.Y;
		}
		const double Sign = Area >= 0.0 ? 1.0 : -1.0;
		FPts Edge;
		Edge.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D E = P[(i + 1) % N] - P[i];
			Edge[i] = (FVector2D(E.Y, -E.X) * Sign).GetSafeNormal();
		}
		Out.SetNumUninitialized(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& Prev = Edge[(i + N - 1) % N];
			const FVector2D& Next = Edge[i];
			FVector2D M = (Prev + Next).GetSafeNormal();
			if (M.IsNearlyZero())
			{
				M = Next.IsNearlyZero() ? Prev : Next;
			}
			const double Cos = FMath::Max3(FVector2D::DotProduct(M, Prev), FVector2D::DotProduct(M, Next), 0.5);
			Out[i] = M / Cos;
		}
	}

	/** Sutherland-Hodgman against one axis-aligned line: keeps V[Axis] >= Limit (bKeepGreater) or <= Limit. */
	void ClipAxis(FPts& P, int32 Axis, double Limit, bool bKeepGreater)
	{
		FPts Out;
		const int32 N = P.Num();
		auto Inside = [&](const FVector2D& V) { return bKeepGreater ? V[Axis] >= Limit : V[Axis] <= Limit; };
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = P[i];
			const FVector2D& B = P[(i + 1) % N];
			const bool bA = Inside(A);
			if (bA)
			{
				Out.Add(A);
			}
			if (bA != Inside(B))
			{
				const double T = (Limit - A[Axis]) / (B[Axis] - A[Axis]);
				Out.Add(A + (B - A) * T);
			}
		}
		P = MoveTemp(Out);
	}

	void RoundRectPoints(float X, float Y, float W, float H, float R, FPts& Out)
	{
		Out.Reset();
		R = FMath::Clamp(R, 0.0f, FMath::Min(W, H) * 0.5f);
		if (R < 0.75f)
		{
			Out.Add(FVector2D(X, Y));
			Out.Add(FVector2D(X + W, Y));
			Out.Add(FVector2D(X + W, Y + H));
			Out.Add(FVector2D(X, Y + H));
			return;
		}
		const int32 Seg = FMath::Clamp(FMath::CeilToInt(R * 0.45f), 3, 12);
		const FVector2D Centres[4] = {
			FVector2D(X + R, Y + R), FVector2D(X + W - R, Y + R), FVector2D(X + W - R, Y + H - R), FVector2D(X + R, Y + H - R)};
		for (int32 c = 0; c < 4; ++c)
		{
			const float Start = UE_PI * (1.0f + 0.5f * c);   // TL 180..270, TR 270..360, BR 0..90, BL 90..180
			for (int32 k = 0; k <= Seg; ++k)
			{
				const float A = Start + UE_HALF_PI * k / Seg;
				const FVector2D V = Centres[c] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
				if (Out.Num() == 0 || !Out.Last().Equals(V, 0.05))
				{
					Out.Add(V);
				}
			}
		}
		if (Out.Num() > 2 && Out.Last().Equals(Out[0], 0.05))
		{
			Out.Pop();
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Painter: anti-aliased shapes, crisp text, icons
	// ---------------------------------------------------------------------------------------------------------------
	class FKGPainter
	{
	public:
		FKGPainter(UCanvas* InCanvas, float InS) : C(InCanvas), S(InS) {}

		UCanvas* C = nullptr;
		float S = 1.0f;

		void Draw(const FTris& T) const
		{
			// The canvas' white square (not GWhiteTexture: that would need a RenderCore link dependency).
			const FTexture* White = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
			if (T.Num() > 0 && White)
			{
				FCanvasTriangleItem Item(T, White);
				Item.BlendMode = SE_BLEND_Translucent;
				C->DrawItem(Item);
			}
		}

		void Rect(float X, float Y, float W, float H, const FLinearColor& Col) const
		{
			FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Col);
			Item.BlendMode = SE_BLEND_Translucent;
			C->DrawItem(Item);
		}

		/**
		 * Filled shape, star-shaped around Centre (default: vertex average), vertical gradient Top -> Bottom over
		 * [GradY0, GradY1] (default: the shape's bounds), Feather px anti-aliasing / blur skirt. bFill = false draws
		 * only the skirt (glows).
		 */
		void PolyEx(TConstArrayView<FVector2D> B, const FLinearColor& Top, const FLinearColor& Bottom, float Feather,
		            const FVector2D* Centre, bool bFill, double GradY0, double GradY1) const
		{
			const int32 N = B.Num();
			if (N < 3)
			{
				return;
			}
			FVector2D Avg = FVector2D::ZeroVector;
			double MinY = B[0].Y;
			double MaxY = B[0].Y;
			for (const FVector2D& V : B)
			{
				Avg += V;
				MinY = FMath::Min(MinY, V.Y);
				MaxY = FMath::Max(MaxY, V.Y);
			}
			if (GradY1 > GradY0)
			{
				MinY = GradY0;
				MaxY = GradY1;
			}
			const FVector2D Ctr = Centre ? *Centre : Avg / static_cast<double>(N);
			const double Span = FMath::Max(MaxY - MinY, 0.001);
			auto ColourAt = [&](const FVector2D& V)
			{
				return FMath::Lerp(Top, Bottom, Saturate(static_cast<float>((V.Y - MinY) / Span)));
			};
			FTris T;
			T.Reserve(N * 3);
			if (bFill)
			{
				const FLinearColor CC = ColourAt(Ctr);
				for (int32 i = 0; i < N; ++i)
				{
					const int32 j = (i + 1) % N;
					AddTri(T, Ctr, B[i], B[j], CC, ColourAt(B[i]), ColourAt(B[j]));
				}
			}
			if (Feather > 0.0f)
			{
				FPts Off;
				OutwardOffsets(B, Off);
				for (int32 i = 0; i < N; ++i)
				{
					const int32 j = (i + 1) % N;
					const FLinearColor Ci = ColourAt(B[i]);
					const FLinearColor Cj = ColourAt(B[j]);
					AddQuad(T, B[i], B[j], B[j] + Off[j] * Feather, B[i] + Off[i] * Feather, Ci, Cj, WithAlpha(Cj, 0.0f),
					        WithAlpha(Ci, 0.0f));
				}
			}
			Draw(T);
		}

		void Poly(TConstArrayView<FVector2D> B, const FLinearColor& Top, const FLinearColor& Bottom, float Feather = 1.0f) const
		{
			PolyEx(B, Top, Bottom, Feather, nullptr, true, 0.0, 0.0);
		}

		/** Two boundary chains sharing their end points (crescents, arcs): stitched as a strip, skirt around it. */
		void Ribbon(TConstArrayView<FVector2D> A, TConstArrayView<FVector2D> B, const FLinearColor& Col) const
		{
			const int32 K = FMath::Min(A.Num(), B.Num());
			if (K < 2)
			{
				return;
			}
			FTris T;
			for (int32 k = 0; k + 1 < K; ++k)
			{
				AddQuad(T, A[k], A[k + 1], B[k + 1], B[k], Col, Col, Col, Col);
			}
			Draw(T);
			FPts Boundary;
			for (int32 k = 0; k < K; ++k)
			{
				Boundary.Add(A[k]);
			}
			for (int32 k = K - 2; k >= 1; --k)
			{
				Boundary.Add(B[k]);
			}
			PolyEx(Boundary, Col, Col, 1.0f, nullptr, false, 0.0, 0.0);
		}

		void RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& Top, const FLinearColor& Bottom,
		               float Feather = 1.0f) const
		{
			if (W <= 0.0f || H <= 0.0f)
			{
				return;
			}
			FPts P;
			RoundRectPoints(X, Y, W, H, R, P);
			PolyEx(P, Top, Bottom, Feather, nullptr, true, Y, Y + H);
		}

		void RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& Col) const
		{
			RoundRect(X, Y, W, H, R, Col, Col);
		}

		/** Inner stroke along a rounded rectangle, anti-aliased on both sides. */
		void Outline(float X, float Y, float W, float H, float R, float Thick, const FLinearColor& Col) const
		{
			FPts B;
			RoundRectPoints(X, Y, W, H, R, B);
			const int32 N = B.Num();
			FPts Off;
			OutwardOffsets(B, Off);
			const FLinearColor Clear = WithAlpha(Col, 0.0f);
			constexpr float Aa = 0.8f;
			FTris T;
			T.Reserve(N * 6);
			for (int32 i = 0; i < N; ++i)
			{
				const int32 j = (i + 1) % N;
				const FVector2D Ii = B[i] - Off[i] * Thick;
				const FVector2D Ij = B[j] - Off[j] * Thick;
				AddQuad(T, B[i], B[j], Ij, Ii, Col, Col, Col, Col);
				AddQuad(T, B[i] + Off[i] * Aa, B[j] + Off[j] * Aa, B[j], B[i], Clear, Clear, Col, Col);
				AddQuad(T, Ii, Ij, Ij - Off[j] * Aa, Ii - Off[i] * Aa, Col, Col, Clear, Clear);
			}
			Draw(T);
		}

		void Shadow(float X, float Y, float W, float H, float R, float Blur, float Opacity) const
		{
			const FLinearColor Col(0.0f, 0.0f, 0.0f, Opacity);
			RoundRect(X, Y, W, H, R, Col, Col, Blur);
		}

		/** Soft coloured halo outside a rounded rectangle (no fill). */
		void Glow(float X, float Y, float W, float H, float R, float Blur, const FLinearColor& Col) const
		{
			FPts P;
			RoundRectPoints(X, Y, W, H, R, P);
			PolyEx(P, Col, Col, Blur, nullptr, false, 0.0, 0.0);
		}

		/** House panel: soft drop shadow, warm-dark ink gradient, faint rim light. bSolid for modal cards. */
		void Card(float X, float Y, float W, float H, float Opacity = 1.0f, float Radius = -1.0f, bool bSolid = false) const
		{
			const float R = Radius >= 0.0f ? Radius : 16.0f * S;
			Shadow(X + 2.0f * S, Y + 6.0f * S, W - 4.0f * S, H - 4.0f * S, R, 16.0f * S, 0.32f * Opacity);
			RoundRect(X, Y, W, H, R, WithAlpha(InkTop, (bSolid ? 0.94f : 0.80f) * Opacity),
			          WithAlpha(InkBottom, (bSolid ? 0.97f : 0.88f) * Opacity));
			Outline(X, Y, W, H, R, FMath::Max(1.0f, 1.5f * S), FLinearColor(1.0f, 0.93f, 0.8f, 0.09f * Opacity));
		}

		/** Coloured cap along the top edge of a card, following its rounded corners. */
		void TopBand(float X, float Y, float W, float R, float Thick, const FLinearColor& Col) const
		{
			FPts P;
			RoundRectPoints(X, Y, W, FMath::Max(2.0f * R, Thick) + 2.0f, R, P);
			ClipAxis(P, 1, Y + Thick, false);
			Poly(P, Col, Col);
		}

		/** Portion F0..F1 of a pill-shaped bar, clipped to the pill outline (so short fills keep the round end). */
		void PillSpan(float X, float Y, float W, float H, float F0, float F1, const FLinearColor& Top,
		              const FLinearColor& Bottom) const
		{
			F0 = Saturate(F0);
			F1 = Saturate(F1);
			if (F1 - F0 < 0.001f || W <= 0.0f)
			{
				return;
			}
			FPts P;
			RoundRectPoints(X, Y, W, H, H * 0.5f, P);
			if (F0 > 0.0f)
			{
				ClipAxis(P, 0, X + W * F0, true);
			}
			if (F1 < 1.0f)
			{
				ClipAxis(P, 0, X + W * F1, false);
			}
			PolyEx(P, Top, Bottom, 1.0f, nullptr, true, Y, Y + H);
		}

		/** Track + gradient fill + gloss. */
		void PillBar(float X, float Y, float W, float H, float Frac, const FLinearColor& Top, const FLinearColor& Bottom,
		             float Opacity = 1.0f) const
		{
			RoundRect(X, Y, W, H, H * 0.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f * Opacity));
			PillSpan(X, Y, W, H, 0.0f, Frac, WithAlpha(Top, Opacity), WithAlpha(Bottom, Opacity));
			const float In = FMath::Min(H * 0.25f, 4.0f * S);
			if (Frac * W > 2.0f * In && H >= 8.0f)
			{
				PillSpan(X + In, Y + H * 0.14f, W - 2.0f * In, H * 0.34f, 0.0f, (Frac * W - In) / (W - 2.0f * In),
				         FLinearColor(1.0f, 1.0f, 1.0f, 0.32f * Opacity), FLinearColor(1.0f, 1.0f, 1.0f, 0.06f * Opacity));
			}
		}

		void Circle(const FVector2D& Ctr, float R, const FLinearColor& Col, float Feather = 1.0f) const
		{
			if (R <= 0.0f)
			{
				return;
			}
			FPts P;
			const int32 Seg = FMath::Clamp(FMath::CeilToInt(R * 1.1f), 12, 72);
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A = UE_TWO_PI * k / Seg;
				P.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			PolyEx(P, Col, Col, Feather, &Ctr, true, 0.0, 0.0);
		}

		/** Ring segment A0..A1 (radians, 0 = right, clockwise on screen), anti-aliased on both edges. */
		void Arc(const FVector2D& Ctr, float Radius, float Thick, float A0, float A1, const FLinearColor& Col,
		         float Feather = 1.0f) const
		{
			const float Ri = FMath::Max(0.0f, Radius - Thick * 0.5f);
			const float Ro = Radius + Thick * 0.5f;
			const int32 Seg = FMath::Clamp(FMath::CeilToInt(FMath::Abs(A1 - A0) * Ro / 3.0f), 6, 180);
			const FLinearColor Clear = WithAlpha(Col, 0.0f);
			FTris T;
			T.Reserve(Seg * 6);
			for (int32 k = 0; k < Seg; ++k)
			{
				const float Ta = A0 + (A1 - A0) * k / Seg;
				const float Tb = A0 + (A1 - A0) * (k + 1) / Seg;
				const FVector2D Da(FMath::Cos(Ta), FMath::Sin(Ta));
				const FVector2D Db(FMath::Cos(Tb), FMath::Sin(Tb));
				AddQuad(T, Ctr + Da * Ri, Ctr + Db * Ri, Ctr + Db * Ro, Ctr + Da * Ro, Col, Col, Col, Col);
				AddQuad(T, Ctr + Da * Ro, Ctr + Db * Ro, Ctr + Db * (Ro + Feather), Ctr + Da * (Ro + Feather), Col, Col,
				        Clear, Clear);
				if (Ri > 0.0f)
				{
					const float Rf = FMath::Max(0.0f, Ri - Feather);
					AddQuad(T, Ctr + Da * Rf, Ctr + Db * Rf, Ctr + Db * Ri, Ctr + Da * Ri, Clear, Clear, Col, Col);
				}
			}
			Draw(T);
		}

		void Line(const FVector2D& A, const FVector2D& B, float Thick, const FLinearColor& Col, bool bRoundCaps = false) const
		{
			const FVector2D D = (B - A).GetSafeNormal();
			const FVector2D N = FVector2D(-D.Y, D.X) * (Thick * 0.5f);
			const FVector2D Q[4] = {A + N, B + N, B - N, A - N};
			Poly(MakeArrayView(Q), Col, Col);
			if (bRoundCaps)
			{
				Circle(A, Thick * 0.5f, Col);
				Circle(B, Thick * 0.5f, Col);
			}
		}

		/** Screen-edge glow: clear inside an ellipse (Inner x half-screen), EdgeAlpha(angle) at the screen edge. */
		template <typename FEdgeAlpha>
		void Vignette(const FLinearColor& Col, float Inner, FEdgeAlpha&& EdgeAlpha) const
		{
			const FVector2D Ctr(C->ClipX * 0.5f, C->ClipY * 0.5f);
			constexpr int32 Seg = 72;
			const float Rings[4] = {Inner, FMath::Lerp(Inner, 1.0f, 0.55f), 1.0f, 1.6f};
			const float Ramp[4] = {0.0f, 0.28f, 1.0f, 1.0f};
			FTris T;
			T.Reserve(Seg * 6);
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A0 = UE_TWO_PI * k / Seg;
				const float A1 = UE_TWO_PI * (k + 1) / Seg;
				const FVector2D D0(FMath::Cos(A0) * Ctr.X, FMath::Sin(A0) * Ctr.Y);
				const FVector2D D1(FMath::Cos(A1) * Ctr.X, FMath::Sin(A1) * Ctr.Y);
				const float E0 = EdgeAlpha(A0);
				const float E1 = EdgeAlpha(A1);
				for (int32 r = 0; r < 3; ++r)
				{
					AddQuad(T, Ctr + D0 * Rings[r], Ctr + D1 * Rings[r], Ctr + D1 * Rings[r + 1], Ctr + D0 * Rings[r + 1],
					        WithAlpha(Col, E0 * Ramp[r]), WithAlpha(Col, E1 * Ramp[r]), WithAlpha(Col, E1 * Ramp[r + 1]),
					        WithAlpha(Col, E0 * Ramp[r + 1]));
				}
			}
			Draw(T);
		}

		// ---- Text ----------------------------------------------------------------------------------------------

		static const UFont* HudFont()
		{
			static TWeakObjectPtr<const UFont> Cached;
			if (!Cached.IsValid())
			{
				const UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
				Cached = Roboto ? Roboto : GEngine->GetLargeFont();
			}
			return Cached.Get();
		}

		/** Design px at 1080p -> Slate points (1 pt = 4/3 px at 96 DPI), rounded so the glyph cache stays small. */
		FSlateFontInfo Font(float Px, bool bBold) const
		{
			const int32 Pt = FMath::Max(6, FMath::RoundToInt(Px * S * 0.75f));
			return FSlateFontInfo(HudFont(), static_cast<float>(Pt), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
		}

		FVector2D Measure(const FString& Str, float Px, bool bBold = true, float Tracking = 0.0f) const
		{
			if (Str.IsEmpty() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
			{
				return FVector2D::ZeroVector;
			}
			const FVector2f Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Font(Px, bBold));
			return FVector2D(Size.X + Tracking * S * FMath::Max(0, Str.Len() - 1), Size.Y);
		}

		/** Y is the top of the line box. AlignX: 0 left, 0.5 centre, 1 right. Returns the measured size. */
		FVector2D Text(const FString& Str, float X, float Y, float Px, const FLinearColor& Col, float AlignX = 0.0f,
		               bool bBold = true, float Tracking = 0.0f, float ShadowAlpha = 0.6f) const
		{
			if (Str.IsEmpty() || Col.A <= 0.004f)
			{
				return FVector2D::ZeroVector;
			}
			const FSlateFontInfo Info = Font(Px, bBold);
			const FVector2D Size = Measure(Str, Px, bBold, Tracking);
			const FVector2D Pos(FMath::RoundToFloat(X - static_cast<float>(Size.X) * AlignX), FMath::RoundToFloat(Y));
			const float Spacing = Tracking * S;
			if (ShadowAlpha > 0.0f)
			{
				const float Drop = FMath::Max(1.0f, FMath::RoundToFloat(2.0f * S));
				FCanvasTextStringViewItem Back(Pos + FVector2D(Drop * 0.5f, Drop), Str, Info,
				                               FLinearColor(0.0f, 0.0f, 0.0f, Col.A * ShadowAlpha));
				Back.HorizSpacingAdjust = Spacing;
				C->DrawItem(Back);
			}
			FCanvasTextStringViewItem Item(Pos, Str, Info, Col);
			Item.HorizSpacingAdjust = Spacing;
			C->DrawItem(Item);
			return Size;
		}

		/** Same as Text, vertically centred on MidY. */
		FVector2D TextMid(const FString& Str, float X, float MidY, float Px, const FLinearColor& Col, float AlignX = 0.0f,
		                  bool bBold = true, float Tracking = 0.0f, float ShadowAlpha = 0.6f) const
		{
			const float Height = static_cast<float>(Measure(Str, Px, bBold).Y);
			return Text(Str, X, MidY - Height * 0.5f, Px, Col, AlignX, bBold, Tracking, ShadowAlpha);
		}

		// ---- Keycaps -----------------------------------------------------------------------------------------

		float KeycapHeight(float Px) const { return FMath::RoundToFloat(Px * 1.55f * S); }

		float KeycapWidth(const FString& Key, float Px) const
		{
			return FMath::Max(KeycapHeight(Px), static_cast<float>(Measure(Key, Px).X) + Px * 0.9f * S);
		}

		/** Cream keyboard key with a darker lip; X/Y top-left. Returns its width. */
		float Keycap(const FString& Key, float X, float Y, float Px, float Opacity = 1.0f) const
		{
			const float H = KeycapHeight(Px);
			const float W = KeycapWidth(Key, Px);
			const float R = FMath::Min(6.0f * S, H * 0.3f);
			const float Lip = FMath::Max(2.0f, FMath::RoundToFloat(3.0f * S));
			Shadow(X, Y + Lip, W, H, R, 4.0f * S, 0.35f * Opacity);
			RoundRect(X, Y + Lip, W, H, R, WithAlpha(Srgb(150, 112, 80), Opacity));
			RoundRect(X, Y, W, H, R, WithAlpha(Srgb(255, 251, 240), Opacity), WithAlpha(Srgb(240, 224, 196), Opacity));
			TextMid(Key, X + W * 0.5f, Y + H * 0.5f, Px, WithAlpha(InkText, Opacity), 0.5f, true, 0.0f, 0.0f);
			return W;
		}

		float KeyHintWidth(const FString& Key, const FString& Label, float Px) const
		{
			return KeycapWidth(Key, Px * 0.85f) + 10.0f * S + static_cast<float>(Measure(Label, Px).X);
		}

		/** [Key] Label, vertically centred on MidY. */
		void KeyHint(const FString& Key, const FString& Label, float X, float MidY, float Px, const FLinearColor& Col,
		             float AlignX = 0.0f, float Opacity = 1.0f) const
		{
			const float KeyPx = Px * 0.85f;
			const float X0 = X - KeyHintWidth(Key, Label, Px) * AlignX;
			const float KW = Keycap(Key, X0, MidY - KeycapHeight(KeyPx) * 0.5f - 1.0f * S, KeyPx, Opacity);
			TextMid(Label, X0 + KW + 10.0f * S, MidY, Px, WithAlpha(Col, Opacity), 0.0f, true);
		}

		// ---- Icons (Size = box edge in screen px) ---------------------------------------------------------------

		void Heart(const FVector2D& Ctr, float Size, const FLinearColor& Top, const FLinearColor& Bottom) const
		{
			// Classic parametric heart: x in [-16,16], y in [-11.7,17] (screen-down), re-centred on its bounds.
			const float K = Size / 32.0f;
			FPts P;
			constexpr int32 Seg = 56;
			for (int32 i = 0; i < Seg; ++i)
			{
				const float T = UE_TWO_PI * i / Seg;
				const float Sn = FMath::Sin(T);
				const float Hx = 16.0f * Sn * Sn * Sn;
				const float Hy = -(13.0f * FMath::Cos(T) - 5.0f * FMath::Cos(2.0f * T) - 2.0f * FMath::Cos(3.0f * T) -
				                   FMath::Cos(4.0f * T));
				P.Add(Ctr + FVector2D(Hx, Hy - 2.65f) * K);
			}
			const FVector2D Kernel = Ctr + FVector2D(0.0f, -0.65f) * K;
			PolyEx(P, Top, Bottom, 1.0f, &Kernel, true, Ctr.Y - 14.35f * K, Ctr.Y + 14.35f * K);
			// Toon shine on the left lobe.
			Circle(Ctr + FVector2D(-7.5f, -6.5f) * K, 2.6f * K, FLinearColor(1.0f, 1.0f, 1.0f, 0.55f * Top.A));
		}

		void Bolt(const FVector2D& Ctr, float Size, const FLinearColor& Top, const FLinearColor& Bottom) const
		{
			const FVector2D Unit[6] = {FVector2D(0.62, 0.0),  FVector2D(0.16, 0.56), FVector2D(0.46, 0.56),
			                           FVector2D(0.36, 1.0),  FVector2D(0.86, 0.40), FVector2D(0.55, 0.40)};
			FPts P;
			for (const FVector2D& U : Unit)
			{
				P.Add(Ctr + (U - FVector2D(0.5, 0.5)) * Size);
			}
			const FVector2D Kernel = Ctr + FVector2D(0.01, -0.02) * Size;   // (0.51, 0.48) sees every vertex
			PolyEx(P, Top, Bottom, 1.0f, &Kernel, true, Ctr.Y - Size * 0.5f, Ctr.Y + Size * 0.5f);
		}

		void Sun(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			Circle(Ctr, Size * 0.24f, Col);
			for (int32 i = 0; i < 8; ++i)
			{
				const float A = UE_PI * 0.25f * i;
				const FVector2D D(FMath::Cos(A), FMath::Sin(A));
				Line(Ctr + D * (Size * 0.36f), Ctr + D * (Size * 0.48f), Size * 0.09f, Col, true);
			}
		}

		void Moon(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			// Disc minus a shifted disc: both arcs sampled tip to tip and stitched as a ribbon.
			const double R = Size * 0.46;
			const double Rc = 0.82 * R;
			const FVector2D Cut = Ctr + FVector2D(0.55, -0.3) * R;
			const FVector2D Dv = Cut - Ctr;
			const double D = Dv.Size();
			const double Along = (R * R - Rc * Rc + D * D) / (2.0 * D);
			const double Half = FMath::Sqrt(FMath::Max(0.0, R * R - Along * Along));
			const FVector2D U = Dv / D;
			const FVector2D Perp(-U.Y, U.X);
			const FVector2D Tip1 = Ctr + U * Along + Perp * Half;
			const FVector2D Tip2 = Ctr + U * Along - Perp * Half;
			auto Angle = [](const FVector2D& V) { return FMath::Atan2(V.Y, V.X); };
			auto Sweep = [](double A0, double A1, double Via)
			{
				const double Full = FMath::Fmod(A1 - A0 + 4.0 * UE_DOUBLE_PI, 2.0 * UE_DOUBLE_PI);
				const double ToVia = FMath::Fmod(Via - A0 + 4.0 * UE_DOUBLE_PI, 2.0 * UE_DOUBLE_PI);
				return ToVia <= Full ? Full : Full - 2.0 * UE_DOUBLE_PI;
			};
			const double Away = Angle(-U);
			const double O0 = Angle(Tip1 - Ctr);
			const double OS = Sweep(O0, Angle(Tip2 - Ctr), Away);
			const double I0 = Angle(Tip1 - Cut);
			const double IS = Sweep(I0, Angle(Tip2 - Cut), Away);
			constexpr int32 Seg = 22;
			FPts Outer;
			FPts Inner;
			for (int32 k = 0; k <= Seg; ++k)
			{
				const double T = static_cast<double>(k) / Seg;
				Outer.Add(Ctr + FVector2D(FMath::Cos(O0 + OS * T), FMath::Sin(O0 + OS * T)) * R);
				Inner.Add(Cut + FVector2D(FMath::Cos(I0 + IS * T), FMath::Sin(I0 + IS * T)) * Rc);
			}
			Ribbon(Outer, Inner, Col);
		}

		void Dawn(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			const FVector2D Base = Ctr + FVector2D(0.0f, Size * 0.16f);
			FPts P;
			for (int32 k = 0; k <= 24; ++k)
			{
				const float A = UE_PI + UE_PI * k / 24.0f;
				P.Add(Base + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Size * 0.26f));
			}
			Poly(P, Col, Col);
			Line(Base + FVector2D(-Size * 0.46f, Size * 0.07f), Base + FVector2D(Size * 0.46f, Size * 0.07f), Size * 0.08f,
			     Col, true);
			for (int32 i = 0; i < 5; ++i)
			{
				const float A = UE_PI + UE_PI * (i + 1) / 6.0f;
				const FVector2D D(FMath::Cos(A), FMath::Sin(A));
				Line(Base + D * (Size * 0.36f), Base + D * (Size * 0.47f), Size * 0.08f, Col, true);
			}
		}

		void Bubble(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			RoundRect(Ctr.X - Size * 0.46f, Ctr.Y - Size * 0.4f, Size * 0.92f, Size * 0.64f, Size * 0.2f, Col);
			const FVector2D Tail[3] = {Ctr + FVector2D(-Size * 0.26f, Size * 0.18f), Ctr + FVector2D(-Size * 0.02f, Size * 0.18f),
			                           Ctr + FVector2D(-Size * 0.32f, Size * 0.46f)};
			Poly(MakeArrayView(Tail), Col, Col);
			for (int32 i = -1; i <= 1; ++i)
			{
				Circle(Ctr + FVector2D(i * Size * 0.2f, -Size * 0.08f), Size * 0.065f, InkBottom);
			}
		}

		void Exclaim(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			Line(Ctr + FVector2D(0.0f, -Size * 0.36f), Ctr + FVector2D(0.0f, Size * 0.1f), Size * 0.17f, Col, true);
			Circle(Ctr + FVector2D(0.0f, Size * 0.36f), Size * 0.1f, Col);
		}

		void Hourglass(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			const float Hw = Size * 0.27f;
			const float Hh = Size * 0.4f;
			const float Th = Size * 0.08f;
			Line(Ctr + FVector2D(-Hw - Th, -Hh), Ctr + FVector2D(Hw + Th, -Hh), Th, Col, true);
			Line(Ctr + FVector2D(-Hw - Th, Hh), Ctr + FVector2D(Hw + Th, Hh), Th, Col, true);
			const FVector2D Top[3] = {Ctr + FVector2D(-Hw, -Hh), Ctr + FVector2D(Hw, -Hh), Ctr + FVector2D(0.0f, -Size * 0.02f)};
			const FVector2D Bottom[3] = {Ctr + FVector2D(0.0f, Size * 0.02f), Ctr + FVector2D(Hw, Hh), Ctr + FVector2D(-Hw, Hh)};
			Poly(MakeArrayView(Top), Col, Col);
			Poly(MakeArrayView(Bottom), Col, Col);
		}

		void Star(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			FPts P;
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = -UE_HALF_PI + UE_PI * i / 5.0f;
				const float R = (i % 2 == 0 ? 0.5f : 0.22f) * Size;
				P.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			PolyEx(P, Col, Col, 1.0f, &Ctr, true, 0.0, 0.0);
		}

		void Diamond(const FVector2D& Ctr, float R, const FLinearColor& Col) const
		{
			const FVector2D Q[4] = {Ctr + FVector2D(0.0f, -R), Ctr + FVector2D(R, 0.0f), Ctr + FVector2D(0.0f, R),
			                        Ctr + FVector2D(-R, 0.0f)};
			Poly(MakeArrayView(Q), Col, Col);
		}

		void CardIcon(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			FPts P;
			RoundRectPoints(Ctr.X - Size * 0.27f, Ctr.Y - Size * 0.38f, Size * 0.54f, Size * 0.76f, Size * 0.08f, P);
			const double Sn = FMath::Sin(-0.21);
			const double Cs = FMath::Cos(-0.21);
			for (FVector2D& V : P)
			{
				const FVector2D L = V - Ctr;
				V = Ctr + FVector2D(L.X * Cs - L.Y * Sn, L.X * Sn + L.Y * Cs);
			}
			Poly(P, Col, Col);
			Diamond(Ctr, Size * 0.14f, InkBottom);
		}

		void Person(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			Circle(Ctr + FVector2D(0.0f, -Size * 0.24f), Size * 0.2f, Col);
			FPts P;
			RoundRectPoints(Ctr.X - Size * 0.36f, Ctr.Y + Size * 0.04f, Size * 0.72f, Size * 0.72f, Size * 0.36f, P);
			ClipAxis(P, 1, Ctr.Y + Size * 0.44f, false);
			Poly(P, Col, Col);
		}

		void House(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			const FVector2D Roof[3] = {Ctr + FVector2D(0.0f, -Size * 0.44f), Ctr + FVector2D(Size * 0.44f, -Size * 0.02f),
			                           Ctr + FVector2D(-Size * 0.44f, -Size * 0.02f)};
			Poly(MakeArrayView(Roof), Col, Col);
			RoundRect(Ctr.X - Size * 0.29f, Ctr.Y - Size * 0.08f, Size * 0.58f, Size * 0.48f, Size * 0.04f, Col);
			RoundRect(Ctr.X - Size * 0.08f, Ctr.Y + Size * 0.12f, Size * 0.16f, Size * 0.28f, Size * 0.03f, InkBottom);
		}

		void Blade(const FVector2D& Ctr, float Size, const FLinearColor& Col) const
		{
			const FVector2D Dir = FVector2D(1.0, -1.0).GetSafeNormal();
			const FVector2D N(-Dir.Y, Dir.X);
			const FVector2D Guard = Ctr - Dir * (Size * 0.12f);
			const FVector2D Tri[3] = {Ctr + Dir * (Size * 0.5f), Guard + N * (Size * 0.12f), Guard - N * (Size * 0.12f)};
			Poly(MakeArrayView(Tri), Col, Col);
			Line(Guard + N * (Size * 0.2f), Guard - N * (Size * 0.2f), Size * 0.08f, Col, true);
			Line(Guard, Guard - Dir * (Size * 0.3f), Size * 0.11f, Col, true);
		}

		void Check(const FVector2D& Ctr, float Size, float Thick, const FLinearColor& Col) const
		{
			const FVector2D A = Ctr + FVector2D(-0.26, 0.02) * Size;
			const FVector2D B = Ctr + FVector2D(-0.06, 0.22) * Size;
			const FVector2D E = Ctr + FVector2D(0.28, -0.2) * Size;
			Line(A, B, Thick, Col, true);
			Line(B, E, Thick, Col, true);
		}
	};

	// ---------------------------------------------------------------------------------------------------------------
	// Per-HUD animation state
	// ---------------------------------------------------------------------------------------------------------------
	struct FKGHudFx
	{
		double Now = 0.0;
		float Dt = 0.0f;
		bool bStarted = false;

		// Vitals
		TWeakObjectPtr<const AKGCharacter> Pawn;
		float Health = 1.0f;
		float ShownHealth = 1.0f;
		float GhostHealth = 1.0f;
		float GhostHold = 0.0f;
		float HitFlash = 0.0f;
		float HitShake = 0.0f;
		float HitDirTime = 0.0f;
		FVector HitFrom = FVector::ZeroVector;
		float Stamina = 1.0f;
		float ShownStamina = 1.0f;
		float StaminaFull = 0.0f;
		bool bExhausted = false;

		// Crosshair
		float AttackKick = 0.0f;
		bool bAttackHeld = false;
		float ShownTask = 0.0f;
		float PromptAlpha = 0.0f;
		FString Prompt;

		// Panels
		int32 LastDone = -1;
		TArray<double> ChoreDoneAt;
		FString Announcement;
		double AnnouncedAt = 0.0;
		EKGPhase Phase = EKGPhase::Lobby;
		double PhaseAt = 0.0;
		double DeadAt = -1.0;
		TMap<FName, FString> TaskNames;

		// Purse
		int32 LastGold = -1;
		int32 GoldGain = 0;
		double GoldAt = -100.0;

		void Advance(double RealTime)
		{
			Dt = bStarted ? FMath::Clamp(static_cast<float>(RealTime - Now), 0.0f, 0.1f) : 0.0f;
			Now = RealTime;
			bStarted = true;
		}
	};

	FKGHudFx& HudFx(const AKGHUD* Hud)
	{
		// Kept out of KGHUD.h so the class layout stays stable and the HUD can be iterated with Live Coding.
		// Live++ keeps old statics across patches: bump the suffix whenever FKGHudFx changes shape.
		static TMap<TObjectKey<AKGHUD>, TUniquePtr<FKGHudFx>> StoreV2;
		const TObjectKey<AKGHUD> Key(Hud);
		if (TUniquePtr<FKGHudFx>* Found = StoreV2.Find(Key))
		{
			return **Found;
		}
		for (auto It = StoreV2.CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
		return *StoreV2.Add(Key, MakeUnique<FKGHudFx>());
	}

	/** Everything one draw pass needs. */
	struct FKGHudFrame
	{
		AKGHUD* Hud = nullptr;
		FKGPainter P{nullptr, 1.0f};
		FKGHudFx* Fx = nullptr;
		float S = 1.0f;
		float W = 0.0f;
		float H = 0.0f;
		float CX = 0.0f;
		float CY = 0.0f;
		const AKGGameState* GS = nullptr;
		APlayerController* PC = nullptr;
		const AKGPlayerState* Me = nullptr;
		const AKGCharacter* Pawn = nullptr;   // the living local villager, else null
		int32 Demo = 0;
		FLinearColor CrossCol = FLinearColor::White;
		FLinearColor StabCol = Crimson;
		FLinearColor HealthCol = Srgb(224, 65, 58);
		FLinearColor StaminaCol = Srgb(242, 194, 48);
	};

	FKGHudFrame MakeFrame(AKGHUD* Hud, UCanvas* Canvas, float S)
	{
		FKGHudFrame F;
		F.Hud = Hud;
		F.P = FKGPainter(Canvas, S);
		F.Fx = &HudFx(Hud);
		F.S = S;
		F.W = Canvas->ClipX;
		F.H = Canvas->ClipY;
		F.CX = F.W * 0.5f;
		F.CY = F.H * 0.5f;
		F.GS = Hud->GetWorld()->GetGameState<AKGGameState>();
		F.PC = Hud->GetOwningPlayerController();
		F.Me = F.PC ? F.PC->GetPlayerState<AKGPlayerState>() : nullptr;
		const AKGCharacter* Character = Cast<AKGCharacter>(Hud->GetOwningPawn());
		F.Pawn = Character && !Character->IsDead() ? Character : nullptr;
		F.Demo = HudDemo();
		return F;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Match vocabulary
	// ---------------------------------------------------------------------------------------------------------------
	const FKGRoleInfo* RoleOf(FName RoleId)
	{
		return RoleId.IsNone() ? nullptr
		                       : FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId);
	}

	FLinearColor AlignmentColor(EKGAlignment A)
	{
		return A == EKGAlignment::Impatient ? ImpatientColor : A == EKGAlignment::Neutral ? NeutralColor : TownColor;
	}

	FString AlignmentName(EKGAlignment A)
	{
		return A == EKGAlignment::Impatient ? TEXT("Impatient") : A == EKGAlignment::Neutral ? TEXT("Neutral")
		                                                                                     : TEXT("Village");
	}

	/** "ClockMaster" -> "Clock Master". */
	FString PrettyRole(FName RoleId)
	{
		const FString Raw = RoleId.ToString();
		FString Out;
		for (int32 i = 0; i < Raw.Len(); ++i)
		{
			if (i > 0 && FChar::IsUpper(Raw[i]) && !FChar::IsUpper(Raw[i - 1]))
			{
				Out.AppendChar(TEXT(' '));
			}
			Out.AppendChar(Raw[i]);
		}
		return Out;
	}

	FString PhaseName(EKGPhase P, int32 Day)
	{
		switch (P)
		{
		case EKGPhase::Warmup: return TEXT("Warm-up");
		case EKGPhase::RoleReveal: return TEXT("Roles are dealt");
		case EKGPhase::Dawn: return FString::Printf(TEXT("Dawn %d"), Day + 1);
		case EKGPhase::Day: return FString::Printf(TEXT("Day %d"), Day);
		case EKGPhase::Meeting: return FString::Printf(TEXT("Day %d - Town meeting"), Day);
		case EKGPhase::Trial: return FString::Printf(TEXT("Day %d - Trial"), Day);
		case EKGPhase::Night: return FString::Printf(TEXT("Night %d"), Day);
		case EKGPhase::Epilogue: return TEXT("Epilogue");
		case EKGPhase::Migrating: return TEXT("Host migrating...");
		default: return TEXT("Lobby");
		}
	}

	FLinearColor PhaseColor(EKGPhase P)
	{
		switch (P)
		{
		case EKGPhase::Night: return NightColor;
		case EKGPhase::Dawn: return DawnColor;
		case EKGPhase::Meeting: return TownColor;
		case EKGPhase::Trial: return ImpatientColor;
		case EKGPhase::RoleReveal:
		case EKGPhase::Migrating: return NeutralColor;
		default: return Gold;
		}
	}

	void PhaseIcon(const FKGPainter& P, EKGPhase Phase, const FVector2D& Ctr, float Size, const FLinearColor& Col)
	{
		switch (Phase)
		{
		case EKGPhase::Day: P.Sun(Ctr, Size, Col); break;
		case EKGPhase::Night: P.Moon(Ctr, Size, Col); break;
		case EKGPhase::Dawn: P.Dawn(Ctr, Size, Col); break;
		case EKGPhase::Meeting: P.Bubble(Ctr, Size, Col); break;
		case EKGPhase::Trial: P.Exclaim(Ctr, Size, Col); break;
		case EKGPhase::RoleReveal: P.CardIcon(Ctr, Size, Col); break;
		case EKGPhase::Epilogue: P.Star(Ctr, Size, Col); break;
		default: P.Hourglass(Ctr, Size, Col); break;
		}
	}

	void AlignmentEmblem(const FKGPainter& P, EKGAlignment A, const FVector2D& Ctr, float Size, float Opacity = 1.0f)
	{
		const FLinearColor Col = WithAlpha(AlignmentColor(A), Opacity);
		P.Circle(Ctr, Size * 0.62f, WithAlpha(Col, 0.2f));
		switch (A)
		{
		case EKGAlignment::Impatient: P.Blade(Ctr, Size * 0.86f, Col); break;
		case EKGAlignment::Neutral: P.Diamond(Ctr, Size * 0.3f, Col); break;
		default: P.House(Ctr, Size * 0.72f, Col); break;
		}
	}

	FString ChoreName(const FKGHudFrame& F, FName Id)
	{
		TMap<FName, FString>& Cache = F.Fx->TaskNames;
		if (const FString* Found = Cache.Find(Id))
		{
			return *Found;
		}
		for (TActorIterator<AKGTaskStation> It(F.Hud->GetWorld()); It; ++It)
		{
			if (!It->TaskName.IsEmpty())
			{
				Cache.Add(It->TaskId, It->TaskName);
			}
		}
		return Cache.FindOrAdd(Id, Id.ToString());
	}

	/** Screen angle (0 = right, +90 deg = down/behind) of the last hit's source relative to where we look. */
	float HitScreenAngle(const FKGHudFrame& F)
	{
		const FRotator View = F.PC && F.PC->PlayerCameraManager ? F.PC->PlayerCameraManager->GetCameraRotation()
		                                                        : F.Pawn->GetControlRotation();
		const FVector Local = FRotator(0.0f, View.Yaw, 0.0f).UnrotateVector(F.Fx->HitFrom - F.Pawn->GetActorLocation());
		return static_cast<float>(FMath::Atan2(-Local.X, Local.Y));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Vitals state (health ghost trail, hit flash, stamina exhaustion, attack kick)
	// ---------------------------------------------------------------------------------------------------------------
	void UpdateVitals(const FKGHudFrame& F)
	{
		FKGHudFx& Fx = *F.Fx;
		const float Dt = Fx.Dt;
		Fx.AttackKick = FMath::Max(0.0f, Fx.AttackKick - Dt * 3.5f);
		Fx.HitFlash = FMath::Max(0.0f, Fx.HitFlash - Dt * 1.5f);
		Fx.HitShake = FMath::Max(0.0f, Fx.HitShake - Dt * 3.0f);
		Fx.HitDirTime = FMath::Max(0.0f, Fx.HitDirTime - Dt);
		const AKGCharacter* Ch = F.Pawn;
		if (!Ch)
		{
			Fx.Pawn = nullptr;
			return;
		}

		const UKGHealthComponent* HealthComp = Ch->GetHealth();
		float Hp = HealthComp ? HealthComp->GetHealth() / FMath::Max(1.0f, HealthComp->MaxHealth) : 0.0f;
		float St = Ch->GetStaminaAlpha();
		if (F.Demo == 1)
		{
			const float Steps[4] = {1.0f, 0.72f, 0.46f, 0.21f};
			Hp = Steps[static_cast<int32>(FMath::Fmod(Fx.Now, 8.0) / 2.0) & 3];
			const float Ph = static_cast<float>(FMath::Fmod(Fx.Now, 7.0) / 7.0);
			St = Ph < 0.45f ? 1.0f - Ph / 0.45f : Ph < 0.6f ? 0.0f : (Ph - 0.6f) / 0.4f;
		}
		Hp = Saturate(Hp);
		St = Saturate(St);

		if (Fx.Pawn.Get() != Ch)
		{
			Fx.Pawn = Ch;
			Fx.Health = Fx.ShownHealth = Fx.GhostHealth = Hp;
			Fx.Stamina = Fx.ShownStamina = St;
			Fx.GhostHold = Fx.HitFlash = Fx.HitShake = Fx.HitDirTime = 0.0f;
			Fx.bExhausted = false;
		}
		if (Hp < Fx.Health - 0.001f)
		{
			const float Damage = Fx.Health - Hp;
			Fx.GhostHealth = FMath::Max(Fx.GhostHealth, Fx.Health);
			Fx.GhostHold = 0.55f;
			Fx.HitFlash = FMath::Clamp(FMath::Max(Fx.HitFlash, 0.5f + Damage * 2.0f), 0.0f, 1.0f);
			Fx.HitShake = 1.0f;
			// Melee game: whoever just hit us is almost always the closest villager.
			const FVector Here = Ch->GetActorLocation();
			float Best = FMath::Square(600.0f);
			Fx.HitDirTime = 0.0f;
			for (TActorIterator<AKGCharacter> It(Ch->GetWorld()); It; ++It)
			{
				const float D2 = FVector::DistSquared(It->GetActorLocation(), Here);
				if (*It != Ch && !It->IsDead() && D2 < Best)
				{
					Best = D2;
					Fx.HitFrom = It->GetActorLocation();
					Fx.HitDirTime = 1.6f;
				}
			}
			if (F.Demo == 1 && Fx.HitDirTime <= 0.0f)
			{
				Fx.HitFrom = Here + FRotator(0.0f, Ch->GetControlRotation().Yaw + 135.0f, 0.0f).Vector() * 200.0f;
				Fx.HitDirTime = 1.6f;
			}
		}
		Fx.Health = Hp;
		Fx.ShownHealth = FMath::FInterpTo(Fx.ShownHealth, Hp, Dt, Hp < Fx.ShownHealth ? 16.0f : 5.0f);
		if (Fx.GhostHold > 0.0f)
		{
			Fx.GhostHold -= Dt;
		}
		else
		{
			Fx.GhostHealth = FMath::FInterpConstantTo(Fx.GhostHealth, Fx.ShownHealth, Dt, 0.6f);
		}
		Fx.GhostHealth = FMath::Max(Fx.GhostHealth, Fx.ShownHealth);

		// FKGStamina: hitting zero exhausts until RecoverThreshold (25 of 100) is back.
		if (St <= 0.005f)
		{
			Fx.bExhausted = true;
		}
		else if (St >= 0.25f)
		{
			Fx.bExhausted = false;
		}
		Fx.Stamina = St;
		Fx.ShownStamina = FMath::FInterpTo(Fx.ShownStamina, St, Dt, 12.0f);
		Fx.StaminaFull = St >= 0.999f ? Fx.StaminaFull + Dt : 0.0f;

		if (F.PC)
		{
			const bool bHeld = F.PC->IsInputKeyDown(EKeys::LeftMouseButton) || F.PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger);
			if (bHeld && !Fx.bAttackHeld)
			{
				Fx.AttackKick = 1.0f;
			}
			Fx.bAttackHeld = bHeld;
		}

		if (F.Demo == 2)
		{
			Fx.Health = 0.24f;
			Fx.ShownHealth = 0.24f;
			Fx.GhostHealth = 0.55f;
			Fx.HitFlash = 0.6f;
			Fx.HitShake = 0.0f;
			Fx.HitDirTime = 1.2f;
			Fx.HitFrom = Ch->GetActorLocation() +
			             FRotator(0.0f, Ch->GetControlRotation().Yaw - 120.0f, 0.0f).Vector() * 200.0f;
			Fx.Stamina = Fx.ShownStamina = 0.08f;
			Fx.bExhausted = true;
			Fx.StaminaFull = 0.0f;
		}
		else if (F.Demo == 3)
		{
			Fx.Health = Fx.ShownHealth = Fx.GhostHealth = 0.85f;
			Fx.HitFlash = Fx.HitShake = Fx.HitDirTime = 0.0f;
			Fx.Stamina = Fx.ShownStamina = 0.62f;
			Fx.bExhausted = false;
			Fx.StaminaFull = 0.0f;
			Fx.AttackKick = 0.6f;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Screen effects: damage flash (directional when we can guess the attacker), low-health heartbeat
	// ---------------------------------------------------------------------------------------------------------------
	void DrawScreenEffects(const FKGHudFrame& F)
	{
		const FKGHudFx& Fx = *F.Fx;
		if (!F.Pawn)
		{
			return;
		}
		const bool bLow = Fx.Health < 0.3f;
		const float Crit = bLow ? Saturate((0.3f - Fx.Health) / 0.25f) : 0.0f;
		const float Beat = bLow ? Heartbeat(Fx.Now, 1.1f + Crit) : 0.0f;
		const float LowAlpha = bLow ? 0.18f + 0.18f * Crit + 0.26f * Beat : 0.0f;
		const float HitAlpha = Fx.HitFlash * Fx.HitFlash * 0.6f;
		if (LowAlpha + HitAlpha < 0.01f)
		{
			return;
		}
		const bool bDir = Fx.HitDirTime > 0.0f;
		const float Theta = bDir ? HitScreenAngle(F) : 0.0f;
		F.P.Vignette(Crimson, 0.5f, [&](float A)
		{
			float Hit = HitAlpha;
			if (bDir)
			{
				const float Facing = FMath::Max(0.0f, FMath::Cos(A - Theta));
				Hit *= 0.18f + 1.5f * Facing * Facing;
			}
			return FMath::Min(0.9f, LowAlpha + Hit);
		});
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Crosshair: dot + ring (kicks on attack, gold when something is usable, crimson on a backstab), chore progress
	// ring, keycap interaction prompt, damage direction arc
	// ---------------------------------------------------------------------------------------------------------------
	void DrawLabelChip(const FKGHudFrame& F, float MidY, float Width, float Height, float Opacity)
	{
		F.P.RoundRect(F.CX - Width * 0.5f, MidY - Height * 0.5f, Width, Height, Height * 0.5f,
		              WithAlpha(InkBottom, 0.62f * Opacity), WithAlpha(InkBottom, 0.72f * Opacity));
	}

	void DrawCrosshair(const FKGHudFrame& F)
	{
		FKGHudFx& Fx = *F.Fx;
		if (!F.Pawn)
		{
			Fx.PromptAlpha = 0.0f;
			Fx.ShownTask = 0.0f;
			return;
		}
		const FKGPainter& P = F.P;
		const float S = F.S;
		const FVector2D Ctr(F.CX, F.CY);
		const bool bBackstab = (F.Pawn->GetViewmodel() && F.Pawn->GetViewmodel()->IsBackstabReady()) || F.Demo == 5;

		// Damage direction: a crimson arc on the side the hit came from.
		if (Fx.HitDirTime > 0.0f)
		{
			const float A = Saturate(Fx.HitDirTime / 0.6f);
			const float Theta = HitScreenAngle(F);
			const float R = 130.0f * S;
			const FVector2D D(FMath::Cos(Theta), FMath::Sin(Theta));
			const FVector2D Nn(-D.Y, D.X);
			P.Arc(Ctr, R, 18.0f * S, Theta - 0.5f, Theta + 0.5f, WithAlpha(Crimson, 0.22f * A), 10.0f * S);
			P.Arc(Ctr, R, FMath::Max(3.0f, 6.0f * S), Theta - 0.42f, Theta + 0.42f, WithAlpha(ImpatientColor, 0.95f * A));
			const FVector2D Arrow[3] = {Ctr + D * (R + 18.0f * S), Ctr + D * (R + 6.0f * S) + Nn * (9.0f * S),
			                            Ctr + D * (R + 6.0f * S) - Nn * (9.0f * S)};
			P.Poly(MakeArrayView(Arrow), WithAlpha(ImpatientColor, 0.95f * A), WithAlpha(ImpatientColor, 0.95f * A));
		}

		// Chore in progress: radial ring + label.
		const AKGTaskStation* Task = F.Pawn->GetActiveTask();
		FString TaskName = Task ? Task->TaskName : FString();
		float TaskProgress = F.Pawn->GetTaskProgress();
		if (!Task && (F.Demo == 2 || (F.Demo == 1 && FMath::Fmod(Fx.Now, 8.0) >= 4.0)))
		{
			TaskName = TEXT("Mend the nets");
			TaskProgress = F.Demo == 2 ? 0.62f : static_cast<float>(FMath::Fmod(Fx.Now, 4.0) / 4.0);
		}
		const bool bWorking = !TaskName.IsEmpty();
		Fx.ShownTask = bWorking ? FMath::FInterpTo(Fx.ShownTask, TaskProgress, Fx.Dt, 14.0f) : 0.0f;
		if (bWorking)
		{
			const float R = 28.0f * S;
			const float Th = FMath::Max(3.0f, 6.0f * S);
			P.Arc(Ctr, R, Th + 8.0f * S, 0.0f, UE_TWO_PI, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 2.0f * S);
			P.Arc(Ctr, R, Th, 0.0f, UE_TWO_PI, FLinearColor(1.0f, 1.0f, 1.0f, 0.14f));
			if (Fx.ShownTask > 0.003f)
			{
				const float A0 = -UE_HALF_PI;
				const float A1 = A0 + UE_TWO_PI * Saturate(Fx.ShownTask);
				P.Arc(Ctr, R, Th + 10.0f * S, A0, A1, WithAlpha(Gold, 0.18f), 6.0f * S);
				P.Arc(Ctr, R, Th, A0, A1, Gold);
				P.Circle(Ctr + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, Th * 0.5f, Gold);
				P.Circle(Ctr + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, Th * 0.5f, GoldLight);
			}
			const FString Label = TaskName.ToUpper();
			const FString Pct = FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Saturate(Fx.ShownTask) * 100.0f));
			const float LW = static_cast<float>(P.Measure(Label, 16.0f, true, 1.5f).X);
			const float PW = static_cast<float>(P.Measure(Pct, 16.0f, true).X);
			const float Gap = 12.0f * S;
			const float MidY = F.CY + 62.0f * S;
			DrawLabelChip(F, MidY, LW + Gap + PW + 32.0f * S, 34.0f * S, 1.0f);
			const float X0 = F.CX - (LW + Gap + PW) * 0.5f;
			P.TextMid(Label, X0, MidY, 16.0f, Gold, 0.0f, true, 1.5f);
			P.TextMid(Pct, X0 + LW + Gap, MidY, 16.0f, Cream, 0.0f, true);
		}

		// Interaction prompt: look-at trace (same reach as the old HUD prompt).
		FString PromptNow;
		if (!bWorking && F.Pawn->GetFirstPersonCamera())
		{
			const FVector Start = F.Pawn->GetFirstPersonCamera()->GetComponentLocation();
			const FVector End = Start + F.Pawn->GetControlRotation().Vector() * 260.0f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KGHudPrompt), false, F.Pawn);
			if (F.Hud->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() &&
			    Hit.GetActor()->GetClass()->ImplementsInterface(UKGInteractable::StaticClass()))
			{
				PromptNow = IKGInteractable::Execute_GetInteractPrompt(Hit.GetActor()).ToString();
			}
		}
		if (PromptNow.IsEmpty() && !bWorking && (F.Demo == 3 || (F.Demo == 1 && FMath::Fmod(Fx.Now, 8.0) < 4.0)))
		{
			PromptNow = TEXT("Open");
		}
		if (!PromptNow.IsEmpty())
		{
			Fx.Prompt = PromptNow;
		}
		Fx.PromptAlpha = FMath::FInterpConstantTo(Fx.PromptAlpha, PromptNow.IsEmpty() ? 0.0f : 1.0f, Fx.Dt, 8.0f);
		if (Fx.PromptAlpha > 0.01f && !Fx.Prompt.IsEmpty())
		{
			const float A = EaseOutCubic(Fx.PromptAlpha);
			const float MidY = F.CY + (62.0f + 8.0f * (1.0f - A)) * S;
			const float Px = 19.0f;
			const float GroupW = P.KeyHintWidth(TEXT("E"), Fx.Prompt, Px);
			DrawLabelChip(F, MidY, GroupW + 28.0f * S, 40.0f * S, A);
			P.KeyHint(TEXT("E"), Fx.Prompt, F.CX, MidY, Px, Cream, 0.5f, A);
		}

		// Ring: a subtle circle that kicks outward on every swing.
		const float Kick = Fx.AttackKick * Fx.AttackKick;
		if (!bWorking)
		{
			const float Pulse = bBackstab ? 0.5f + 0.5f * static_cast<float>(FMath::Sin(Fx.Now * 14.0)) : 0.0f;
			const float R = FMath::Max(8.0f, 12.0f * S) + 11.0f * S * Kick + 2.0f * S * Pulse;
			const float Th = FMath::Max(1.25f, 1.7f * S) + 1.5f * S * Kick;
			const FLinearColor RingCol = bBackstab ? F.StabCol : FMath::Lerp(FLinearColor::White, Gold, Fx.PromptAlpha);
			const float RingA = bBackstab ? 0.95f : FMath::Min(1.0f, 0.3f + 0.55f * Kick + 0.35f * Fx.PromptAlpha);
			P.Arc(Ctr, R, Th + 2.0f, 0.0f, UE_TWO_PI, FLinearColor(0.0f, 0.0f, 0.0f, 0.18f * RingA), 1.0f);
			P.Arc(Ctr, R, Th, 0.0f, UE_TWO_PI, WithAlpha(RingCol, RingA), 0.8f);
		}

		// Backstab: four crimson ticks, an unmistakable "stab now" read.
		if (bBackstab)
		{
			const float Pulse = static_cast<float>(FMath::Sin(Fx.Now * 14.0));
			const float Gap = 17.0f * FMath::Max(S, 0.8f) + 2.0f * S * Pulse;
			const float Len = 9.0f * FMath::Max(S, 0.8f);
			const float Th = FMath::Max(3.0f * S, 2.5f);
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = UE_HALF_PI * i + UE_PI * 0.25f;
				const FVector2D D(FMath::Cos(A), FMath::Sin(A));
				P.Line(Ctr + D * Gap, Ctr + D * (Gap + Len), Th + 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), true);
				P.Line(Ctr + D * Gap, Ctr + D * (Gap + Len), Th, F.StabCol, true);
			}
		}

		// Dot, with a dark halo so it reads on bright sand and sky alike (pixel floor: QA-002-01).
		const float Dot = bBackstab ? FMath::Max(3.5f, 4.5f * S) : FMath::Max(2.0f, 2.8f * S);
		P.Circle(Ctr, Dot + FMath::Max(1.0f, 1.3f * S), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		P.Circle(Ctr, Dot, bBackstab ? F.StabCol : F.CrossCol);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Vitals: heart + health (ghost trail, quarter notches, number), bolt + stamina (flashes when exhausted)
	// ---------------------------------------------------------------------------------------------------------------
	void DrawVitals(const FKGHudFrame& F)
	{
		if (!F.Pawn)
		{
			return;
		}
		const FKGHudFx& Fx = *F.Fx;
		const FKGPainter& P = F.P;
		const float S = F.S;
		const float Shake = Fx.HitShake * Fx.HitShake * static_cast<float>(FMath::Sin(Fx.Now * 80.0)) * 7.0f * S;
		const float PW = 440.0f * S;
		const float PH = 100.0f * S;
		const float PX = 28.0f * S + Shake;
		const float PY = F.H - 28.0f * S - PH;
		P.Card(PX, PY, PW, PH);
		if (Fx.HitFlash > 0.01f)
		{
			P.Glow(PX, PY, PW, PH, 16.0f * S, 16.0f * S, WithAlpha(Crimson, 0.6f * Fx.HitFlash));
		}

		const float BarX = PX + 84.0f * S;
		const float BarW = 262.0f * S;

		// Health.
		const bool bLow = Fx.Health < 0.3f;
		const float Crit = bLow ? Saturate((0.3f - Fx.Health) / 0.25f) : 0.0f;
		const float Beat = bLow ? Heartbeat(Fx.Now, 1.1f + Crit) : 0.0f;
		const float Row1 = PY + 36.0f * S;
		const FVector2D HeartC(PX + 44.0f * S, Row1);
		P.Circle(HeartC, 27.0f * S, WithAlpha(Crimson, 0.14f + 0.35f * Beat));
		const float HeartSize = 38.0f * S * (1.0f + 0.2f * Beat);
		P.Heart(HeartC, HeartSize, FMath::Lerp(Srgb(255, 118, 128), FLinearColor::White, 0.35f * Beat + 0.4f * Fx.HitFlash),
		        Srgb(210, 26, 58));

		const float BarH = 26.0f * S;
		const float BarY = Row1 - BarH * 0.5f;
		P.RoundRect(BarX, BarY, BarW, BarH, BarH * 0.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		P.PillSpan(BarX, BarY, BarW, BarH, Fx.ShownHealth, Fx.GhostHealth, FLinearColor(1.0f, 1.0f, 1.0f, 0.95f),
		           FLinearColor(1.0f, 0.86f, 0.82f, 0.85f));
		FLinearColor FillTop = Shade(FMath::Lerp(F.HealthCol, Srgb(255, 92, 84), 0.5f), 1.2f);
		FLinearColor FillBottom = Shade(F.HealthCol, 0.72f);
		FillTop = FMath::Lerp(FillTop, FLinearColor::White, 0.55f * Fx.HitFlash * Fx.HitFlash + 0.2f * Beat);
		FillBottom = FMath::Lerp(FillBottom, Crimson, 0.4f * Crit);
		FillTop.A = FillBottom.A = 1.0f;
		P.PillSpan(BarX, BarY, BarW, BarH, 0.0f, Fx.ShownHealth, FillTop, FillBottom);
		const float In = 5.0f * S;
		if (Fx.ShownHealth * BarW > 2.0f * In)
		{
			P.PillSpan(BarX + In, BarY + 3.0f * S, BarW - 2.0f * In, BarH * 0.32f, 0.0f,
			           (Fx.ShownHealth * BarW - In) / (BarW - 2.0f * In), FLinearColor(1.0f, 1.0f, 1.0f, 0.3f),
			           FLinearColor(1.0f, 1.0f, 1.0f, 0.04f));
		}
		for (int32 q = 1; q <= 3; ++q)
		{
			if (q * 0.25f < Fx.ShownHealth - 0.02f)
			{
				P.Rect(FMath::RoundToFloat(BarX + BarW * q * 0.25f - S), BarY + 7.0f * S, FMath::Max(1.0f, 2.0f * S),
				       BarH - 14.0f * S, FLinearColor(0.0f, 0.0f, 0.0f, 0.22f));
			}
		}
		// Rolls down with the bar instead of snapping (fighting-game counter).
		const int32 HpNumber = FMath::Max(0, FMath::CeilToInt(FMath::Max(Fx.Health, Fx.ShownHealth) * 100.0f - 0.01f));
		FLinearColor NumCol = bLow ? FMath::Lerp(ImpatientColor, FLinearColor::White, 0.45f * Beat) : Cream;
		NumCol = FMath::Lerp(NumCol, FLinearColor::White, Fx.HitFlash);
		P.TextMid(FString::FromInt(HpNumber), PX + PW - 22.0f * S, Row1, 34.0f, NumCol, 1.0f, true);

		// Stamina.
		const float Row2 = PY + 76.0f * S;
		const float Blink = Fx.bExhausted ? 0.5f + 0.5f * static_cast<float>(FMath::Sin(Fx.Now * 16.0)) : 0.0f;
		const float RowA = FMath::Lerp(1.0f, 0.72f, Saturate((Fx.StaminaFull - 1.5f) / 0.8f));
		const FLinearColor Orange = Srgb(255, 128, 48);
		const FLinearColor BoltTop = Fx.bExhausted ? FMath::Lerp(ImpatientColor, Orange, Blink) : Shade(F.StaminaCol, 1.25f);
		const FLinearColor BoltBottom = Fx.bExhausted ? FMath::Lerp(Crimson, Orange, Blink)
		                                              : FMath::Lerp(F.StaminaCol, Orange, 0.45f);
		P.Circle(FVector2D(PX + 44.0f * S, Row2), 18.0f * S, WithAlpha(F.StaminaCol, 0.12f * RowA));
		P.Bolt(FVector2D(PX + 44.0f * S, Row2), 30.0f * S, WithAlpha(BoltTop, RowA), WithAlpha(BoltBottom, RowA));
		const float SBarH = 14.0f * S;
		const float SBarY = Row2 - SBarH * 0.5f;
		P.RoundRect(BarX, SBarY, BarW, SBarH, SBarH * 0.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		if (Fx.bExhausted)
		{
			P.RoundRect(BarX, SBarY, BarW, SBarH, SBarH * 0.5f, WithAlpha(Crimson, 0.25f + 0.35f * Blink));
		}
		const FLinearColor STop = Fx.bExhausted ? FMath::Lerp(Orange, FLinearColor::White, 0.3f * Blink)
		                                        : Shade(FMath::Lerp(F.StaminaCol, Srgb(255, 226, 90), 0.4f), 1.1f);
		const FLinearColor SBottom = Fx.bExhausted ? FMath::Lerp(Crimson, Orange, Blink)
		                                           : FMath::Lerp(F.StaminaCol, Orange, 0.5f);
		P.PillSpan(BarX, SBarY, BarW, SBarH, 0.0f, Fx.ShownStamina, STop, SBottom);
		if (Fx.ShownStamina * BarW > 8.0f * S)
		{
			P.PillSpan(BarX + 3.0f * S, SBarY + 2.0f * S, BarW - 6.0f * S, SBarH * 0.3f, 0.0f,
			           (Fx.ShownStamina * BarW - 3.0f * S) / (BarW - 6.0f * S), FLinearColor(1.0f, 1.0f, 1.0f, 0.3f),
			           FLinearColor(1.0f, 1.0f, 1.0f, 0.04f));
		}
		if (Fx.bExhausted)
		{
			// Where sprinting comes back (FKGStamina::RecoverThreshold).
			P.Rect(FMath::RoundToFloat(BarX + BarW * 0.25f - S), SBarY - 3.0f * S, FMath::Max(1.0f, 2.0f * S),
			       SBarH + 6.0f * S, FLinearColor(1.0f, 1.0f, 1.0f, 0.8f));
			P.TextMid(TEXT("TIRED"), PX + PW - 22.0f * S, Row2, 15.0f, FMath::Lerp(ImpatientColor, Orange, Blink), 1.0f, true,
			          1.5f);
		}
	}

	/** Above the vitals: in-match gold (Coin items) that pops when it grows, plus the pockets key. */
	void DrawPurse(const FKGHudFrame& F)
	{
		FKGHudFx& Fx = *F.Fx;
		const UKGInventoryComponent* Pockets = F.Pawn && F.Me ? UKGInventoryComponent::FindForPlayer(F.Me) : nullptr;
		if (!Pockets)
		{
			Fx.LastGold = -1;
			return;
		}
		const int32 Coins = Pockets->GetGold();
		if (Fx.LastGold >= 0 && Coins > Fx.LastGold)
		{
			Fx.GoldGain = Coins - Fx.LastGold;
			Fx.GoldAt = Fx.Now;
		}
		Fx.LastGold = Coins;

		const FKGPainter& P = F.P;
		const float S = F.S;
		const float Pop = 1.0f - EaseOutCubic(static_cast<float>(Fx.Now - Fx.GoldAt) / 0.7f);   // 1 right after a gain
		const FString Amount = FString::FromInt(Coins);
		const FString Label = TEXT("Pockets");
		const bool bHint = !(F.PC && FKGInventoryUI::IsOpen(F.PC));
		const float AmountW = static_cast<float>(P.Measure(Amount, 20.0f, true).X);
		const float HintW = bHint ? P.KeyHintWidth(TEXT("I"), Label, 16.0f) + 22.0f * S : 0.0f;
		const float H = 38.0f * S;
		const float W = 50.0f * S + AmountW + 16.0f * S + HintW;
		const float X = 28.0f * S;
		const float MidY = F.H - 128.0f * S - 12.0f * S - H * 0.5f;
		P.RoundRect(X, MidY - H * 0.5f, W, H, H * 0.5f, WithAlpha(InkBottom, 0.62f), WithAlpha(InkBottom, 0.74f));

		const FVector2D Coin(X + 24.0f * S, MidY);
		const float R = 11.0f * S * (1.0f + 0.35f * Pop);
		P.Circle(Coin, R + 1.5f * S, Srgb(150, 88, 24));
		P.Circle(Coin, R, FMath::Lerp(Gold, FLinearColor::White, 0.4f * Pop));
		P.Circle(Coin, R * 0.62f, Srgb(236, 150, 40));
		P.Circle(Coin + FVector2D(-0.38f, -0.38f) * R, R * 0.2f, FLinearColor(1.0f, 1.0f, 1.0f, 0.65f));
		P.TextMid(Amount, X + 46.0f * S, MidY, 20.0f, FMath::Lerp(Gold, FLinearColor::White, Pop), 0.0f, true);
		if (bHint)
		{
			const float DivX = X + 50.0f * S + AmountW + 6.0f * S;
			P.Rect(DivX, MidY - 11.0f * S, FMath::Max(1.0f, S), 22.0f * S, FLinearColor(1.0f, 0.93f, 0.8f, 0.15f));
			P.KeyHint(TEXT("I"), Label, DivX + 12.0f * S, MidY, 16.0f, WithAlpha(Cream, 0.85f));
		}
		const float FloatT = static_cast<float>(Fx.Now - Fx.GoldAt) / 1.3f;   // "+N" drifts up and fades
		if (FloatT < 1.0f && Fx.GoldGain > 0)
		{
			P.TextMid(FString::Printf(TEXT("+%d"), Fx.GoldGain), Coin.X, MidY - 30.0f * S - EaseOutCubic(FloatT) * 26.0f * S,
			          22.0f, WithAlpha(GoldLight, 1.0f - FloatT * FloatT), 0.5f, true);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Match panels
	// ---------------------------------------------------------------------------------------------------------------

	/** Top centre: phase icon + name, alive count, clock; village preparation underneath. Returns the bottom edge. */
	float DrawPhasePanel(const FKGHudFrame& F)
	{
		const AKGGameState* GS = F.GS;
		const FKGPainter& P = F.P;
		const float S = F.S;
		const EKGPhase Phase = GS->GetPhase();
		int32 Alive = 0;
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			Alive += PS && PS->IsAlive() ? 1 : 0;
		}
		const bool bPrep = Phase != EKGPhase::Epilogue && Phase != EKGPhase::Lobby &&
		                   ((F.Me && F.Me->TaskIds.Num() > 0) || F.Demo != 0);
		const float PW = 440.0f * S;
		const float RowH = 72.0f * S;
		const float PH = RowH + (bPrep ? 40.0f * S : 0.0f);
		const float PX = F.CX - PW * 0.5f;
		const float PY = 18.0f * S;
		P.Card(PX, PY, PW, PH);
		const FLinearColor Accent = PhaseColor(Phase);
		const FVector2D IconC(PX + 40.0f * S, PY + RowH * 0.5f);
		P.Circle(IconC, 25.0f * S, WithAlpha(Accent, 0.16f));
		PhaseIcon(P, Phase, IconC, 32.0f * S, Accent);

		const bool bClock = Phase != EKGPhase::Lobby;
		const FString Name = PhaseName(Phase, GS->GetDayIndex()).ToUpper();
		P.TextMid(Name, PX + 76.0f * S, PY + (bClock ? 26.0f : 36.0f) * S, 24.0f, Cream, 0.0f, true, 1.0f);
		if (bClock)
		{
			const FLinearColor Dim = WithAlpha(Cream, 0.7f);
			P.Person(FVector2D(PX + 85.0f * S, PY + 51.0f * S), 16.0f * S, Dim);
			P.TextMid(FString::Printf(TEXT("%d ALIVE"), Alive), PX + 99.0f * S, PY + 51.0f * S, 15.0f, Dim, 0.0f, true, 1.5f);

			const float Remaining = GS->GetPhaseRemaining();
			const int32 Secs = FMath::Max(0, FMath::CeilToInt(Remaining));
			const bool bUrgent = Secs <= 10 && Remaining > 0.0f && Phase != EKGPhase::Epilogue;
			const float Tick = bUrgent ? 1.0f - static_cast<float>(FMath::Frac(Remaining)) : 0.0f;
			const FLinearColor TimeCol = bUrgent ? FMath::Lerp(ImpatientColor, FLinearColor::White, 0.5f * Tick * Tick) : Gold;
			P.TextMid(FString::Printf(TEXT("%d:%02d"), Secs / 60, Secs % 60), PX + PW - 24.0f * S, PY + RowH * 0.5f, 38.0f,
			          TimeCol, 1.0f, true, 1.0f);
		}

		if (bPrep)
		{
			const float Prep = F.Demo != 0 && !(F.Me && F.Me->TaskIds.Num() > 0) ? 0.42f : Saturate(GS->Preparation);
			const bool bFull = Prep >= 0.999f;
			const float InX = PX + 20.0f * S;
			const float InW = PW - 40.0f * S;
			P.Rect(InX, PY + RowH, InW, FMath::Max(1.0f, S), FLinearColor(1.0f, 0.93f, 0.8f, 0.08f));
			const float LabelY = PY + RowH + 13.0f * S;
			P.TextMid(bFull ? TEXT("LIGHTHOUSE READY") : TEXT("VILLAGE PREPARATION"), InX, LabelY, 13.0f,
			          bFull ? Gold : WithAlpha(Cream, 0.75f), 0.0f, true, 2.0f);
			P.TextMid(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Prep * 100.0f)), InX + InW, LabelY, 14.0f, Gold, 1.0f,
			          true, 1.0f);
			const float BarY = PY + RowH + 23.0f * S;
			const float BarH = 8.0f * S;
			if (bFull)
			{
				const float Glow = 0.5f + 0.5f * static_cast<float>(FMath::Sin(F.Fx->Now * 4.0));
				P.Glow(InX, BarY, InW, BarH, BarH * 0.5f, 8.0f * S, WithAlpha(Gold, 0.45f * Glow));
			}
			P.PillBar(InX, BarY, InW, BarH, Prep, GoldLight, Gold);
		}
		const float Bottom = PY + PH;
		return Bottom;
	}

	void DrawAnnouncement(const FKGHudFrame& F, float Top)
	{
		const AKGGameState* GS = F.GS;
		FKGHudFx& Fx = *F.Fx;
		const double ServerNow = GS->GetServerWorldTimeSeconds();
		if (GS->Announcement.IsEmpty() || ServerNow >= GS->AnnouncementUntil)
		{
			Fx.Announcement.Reset();
			return;
		}
		if (Fx.Announcement != GS->Announcement)
		{
			Fx.Announcement = GS->Announcement;
			Fx.AnnouncedAt = Fx.Now;
		}
		const FKGPainter& P = F.P;
		const float S = F.S;
		const float Age = static_cast<float>(Fx.Now - Fx.AnnouncedAt);
		const float In = EaseOutBack(Age / 0.4f);
		const float A = Saturate(Age / 0.2f) * Saturate(static_cast<float>(GS->AnnouncementUntil - ServerNow) / 0.4f);
		const FString Line = KGStreamer::MaskText(F.Hud->GetWorld(), GS->Announcement);   // streamer mode: pseudonyms
		const float TextW = static_cast<float>(P.Measure(Line, 21.0f, true).X);
		const float W = FMath::Min(TextW + 110.0f * S, F.W - 40.0f * S);
		const float H = 52.0f * S;
		const float X = F.CX - W * 0.5f;
		const float Y = Top - (1.0f - In) * 18.0f * S;
		P.Card(X, Y, W, H, A);
		P.RoundRect(F.CX - 34.0f * S, Y - 2.0f * S, 68.0f * S, 5.0f * S, 2.5f * S, WithAlpha(Gold, A));
		P.Diamond(FVector2D(X + 26.0f * S, Y + H * 0.5f), 6.0f * S, WithAlpha(Gold, A));
		P.Diamond(FVector2D(X + W - 26.0f * S, Y + H * 0.5f), 6.0f * S, WithAlpha(Gold, A));
		P.TextMid(Line, F.CX, Y + H * 0.5f, 21.0f, WithAlpha(Cream, A), 0.5f, true);
	}

	/** Top left: own chores with checkboxes. Returns the bottom edge (or the top margin when nothing is shown). */
	float DrawChores(const FKGHudFrame& F)
	{
		FKGHudFx& Fx = *F.Fx;
		const FKGPainter& P = F.P;
		const float S = F.S;
		struct FRow
		{
			FString Name;
			bool bDone = false;
			bool bActive = false;
		};
		TArray<FRow> Rows;
		const AKGTaskStation* Active = F.Pawn ? F.Pawn->GetActiveTask() : nullptr;
		if (F.Me && F.GS->GetPhase() != EKGPhase::Epilogue)
		{
			for (int32 i = 0; i < F.Me->TaskIds.Num(); ++i)
			{
				FRow& Row = Rows.AddDefaulted_GetRef();
				Row.Name = ChoreName(F, F.Me->TaskIds[i]);
				Row.bDone = F.Me->TaskDone.IsValidIndex(i) && F.Me->TaskDone[i];
				Row.bActive = Active && Active->TaskId == F.Me->TaskIds[i];
			}
		}
		if (Rows.Num() == 0 && F.Demo != 0)
		{
			const TCHAR* Names[] = {TEXT("Mend the nets"), TEXT("Feed the gulls"), TEXT("Ring the harbour bell"), TEXT("Stack firewood")};
			for (int32 i = 0; i < 4; ++i)
			{
				FRow& Row = Rows.AddDefaulted_GetRef();
				Row.Name = Names[i];
				Row.bDone = i == 1 || (F.Demo == 1 && i == 3 && FMath::Fmod(Fx.Now, 8.0) > 6.0);
				Row.bActive = i == 0 && F.Demo != 3;
			}
		}
		if (Rows.Num() == 0)
		{
			Fx.LastDone = -1;
			Fx.ChoreDoneAt.Reset();
			return 0.0f;
		}

		int32 Done = 0;
		for (const FRow& Row : Rows)
		{
			Done += Row.bDone ? 1 : 0;
		}
		if (Fx.LastDone >= 0 && Done > Fx.LastDone)
		{
			KGAudio::UI(F.Hud, TEXT("S_TaskDone"), 0.7f);   // chime when a chore completes
		}
		Fx.LastDone = Done;
		if (Fx.ChoreDoneAt.Num() != Rows.Num())
		{
			Fx.ChoreDoneAt.Init(0.0, Rows.Num());
			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				Fx.ChoreDoneAt[i] = Rows[i].bDone ? -100.0 : 0.0;   // already done when first seen: no pop
			}
		}

		const float X = 28.0f * S;
		const float Y = 24.0f * S;
		const float W = 350.0f * S;
		const float RowH = 36.0f * S;
		const float HeadH = 62.0f * S;
		const float H = HeadH + RowH * Rows.Num() + 10.0f * S;
		P.Card(X, Y, W, H);
		P.TextMid(TEXT("CHORES"), X + 20.0f * S, Y + 22.0f * S, 15.0f, Gold, 0.0f, true, 2.5f);
		P.TextMid(FString::Printf(TEXT("%d / %d"), Done, Rows.Num()), X + W - 20.0f * S, Y + 22.0f * S, 15.0f, Cream, 1.0f,
		          true, 1.0f);
		P.PillBar(X + 20.0f * S, Y + 40.0f * S, W - 40.0f * S, 7.0f * S, static_cast<float>(Done) / Rows.Num(), TownLight,
		          TownColor);

		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			const FRow& Row = Rows[i];
			if (Row.bDone && Fx.ChoreDoneAt[i] == 0.0)
			{
				Fx.ChoreDoneAt[i] = Fx.Now;
			}
			else if (!Row.bDone)
			{
				Fx.ChoreDoneAt[i] = 0.0;
			}
			const float MidY = Y + HeadH + RowH * (i + 0.5f);
			const float Box = 20.0f * S;
			const FVector2D BoxC(X + 20.0f * S + Box * 0.5f, MidY);
			if (Row.bDone)
			{
				const float Pop = Fx.ChoreDoneAt[i] < 0.0 ? 1.0f : EaseOutBack(static_cast<float>(Fx.Now - Fx.ChoreDoneAt[i]) / 0.35f);
				const float B = Box * FMath::Max(0.05f, Pop);
				P.RoundRect(BoxC.X - B * 0.5f, BoxC.Y - B * 0.5f, B, B, 6.0f * S * Pop, TownLight, TownColor);
				P.Check(BoxC, B, FMath::Max(1.5f, 3.0f * S * Pop), InkText);
			}
			else if (Row.bActive)
			{
				const float Glow = 0.5f + 0.5f * static_cast<float>(FMath::Sin(Fx.Now * 6.0));
				P.RoundRect(BoxC.X - Box * 0.5f, BoxC.Y - Box * 0.5f, Box, Box, 6.0f * S, WithAlpha(Gold, 0.2f + 0.25f * Glow));
				P.Outline(BoxC.X - Box * 0.5f, BoxC.Y - Box * 0.5f, Box, Box, 6.0f * S, FMath::Max(1.5f, 2.0f * S), Gold);
			}
			else
			{
				P.Outline(BoxC.X - Box * 0.5f, BoxC.Y - Box * 0.5f, Box, Box, 6.0f * S, FMath::Max(1.5f, 2.0f * S),
				          WithAlpha(Cream, 0.55f));
			}
			const FLinearColor TextCol = Row.bDone ? WithAlpha(Cream, 0.42f) : Row.bActive ? Gold : Cream;
			const FVector2D Size = P.TextMid(Row.Name, X + 54.0f * S, MidY, 19.0f, TextCol, 0.0f, !Row.bDone);
			if (Row.bDone)
			{
				P.Rect(X + 52.0f * S, FMath::RoundToFloat(MidY), static_cast<float>(Size.X) + 4.0f * S, FMath::Max(1.0f, 2.0f * S),
				       WithAlpha(Cream, 0.45f));
			}
		}
		return Y + H;
	}

	/** Bottom right: own (secret) role, plus the blade key for the Impatient. */
	void DrawRoleChip(const FKGHudFrame& F, const FKGRoleInfo& Role)
	{
		const FKGPainter& P = F.P;
		const float S = F.S;
		const EKGAlignment A = Role.GetAlignment();
		const FLinearColor AC = AlignmentColor(A);
		const FString Name = PrettyRole(Role.RoleId);
		const FString Side = AlignmentName(A).ToUpper();
		const float TextW = FMath::Max(static_cast<float>(P.Measure(Name, 22.0f, true).X),
		                               static_cast<float>(P.Measure(Side, 13.0f, true, 2.0f).X));
		const float H = 64.0f * S;
		const float W = TextW + 90.0f * S;
		const float X = F.W - 28.0f * S - W;
		const float Y = F.H - 28.0f * S - H;
		P.Card(X, Y, W, H);
		AlignmentEmblem(P, A, FVector2D(X + 36.0f * S, Y + H * 0.5f), 36.0f * S);
		P.TextMid(Name, X + 68.0f * S, Y + 25.0f * S, 22.0f, AC, 0.0f, true);
		P.TextMid(Side, X + 68.0f * S, Y + 46.0f * S, 13.0f, WithAlpha(Cream, 0.62f), 0.0f, true, 2.0f);
		if (A == EKGAlignment::Impatient && F.GS->GetPhase() != EKGPhase::Epilogue)
		{
			const float MidY = Y - 30.0f * S;
			const FString Label = TEXT("Draw / hide blade");
			const float HW = P.KeyHintWidth(TEXT("B"), Label, 17.0f) + 26.0f * S;
			P.RoundRect(F.W - 28.0f * S - HW, MidY - 19.0f * S, HW, 38.0f * S, 19.0f * S, WithAlpha(InkBottom, 0.6f));
			P.KeyHint(TEXT("B"), Label, F.W - 28.0f * S - 13.0f * S, MidY, 17.0f, Cream, 1.0f);
		}
	}

	/** Streamer mode (SPRINT-015): the role chip without the role - hold the peek key to see it. */
	void DrawHiddenRoleChip(const FKGHudFrame& F)
	{
		const FKGPainter& P = F.P;
		const float S = F.S;
		const FString Key = KGStreamer::GetPeekKeyLabel().ToString();
		const FString Label = TEXT("Hold to peek");
		const FString Caption = TEXT("ROLE HIDDEN");
		const float TextW = FMath::Max(P.KeyHintWidth(Key, Label, 16.0f), static_cast<float>(P.Measure(Caption, 12.0f, true, 2.0f).X));
		const float H = 64.0f * S;
		const float W = TextW + 48.0f * S;
		const float X = F.W - 28.0f * S - W;
		const float Y = F.H - 28.0f * S - H;
		P.Card(X, Y, W, H);
		P.TextMid(Caption, X + 24.0f * S, Y + 20.0f * S, 12.0f, WithAlpha(Cream, 0.55f), 0.0f, true, 2.0f);
		P.KeyHint(Key, Label, X + 24.0f * S, Y + 43.0f * S, 16.0f, Cream);
	}

	/** Centre: the role card while roles are dealt (slides up, glows in the alignment colour). */
	void DrawRoleCard(const FKGHudFrame& F, const FKGRoleInfo& Role)
	{
		const FKGPainter& P = F.P;
		const float S = F.S;
		const EKGAlignment A = Role.GetAlignment();
		const FLinearColor AC = AlignmentColor(A);
		const float Age = static_cast<float>(F.Fx->Now - F.Fx->PhaseAt);
		const float T = EaseOutBack(Age / 0.5f);
		const float Al = Saturate(Age / 0.25f);
		const float W = 1000.0f * S;
		const float H = 340.0f * S;
		const float X = F.CX - W * 0.5f;
		const float Y = F.H * 0.25f + (1.0f - T) * 50.0f * S;
		const float R = 20.0f * S;
		P.Glow(X, Y, W, H, R, 40.0f * S, WithAlpha(AC, 0.45f * Al));
		P.Card(X, Y, W, H, Al, R, true);
		P.TopBand(X, Y, W, R, 10.0f * S, WithAlpha(AC, Al));
		AlignmentEmblem(P, A, FVector2D(F.CX, Y + 58.0f * S), 46.0f * S, Al);
		P.TextMid(TEXT("YOUR ROLE"), F.CX, Y + 104.0f * S, 15.0f, WithAlpha(Cream, 0.65f * Al), 0.5f, true, 4.0f);
		P.TextMid(PrettyRole(Role.RoleId), F.CX, Y + 158.0f * S, 64.0f, WithAlpha(AC, Al), 0.5f, true, 1.5f);
		const FString Side = AlignmentName(A).ToUpper();
		const float SW = static_cast<float>(P.Measure(Side, 15.0f, true, 2.0f).X) + 40.0f * S;
		P.RoundRect(F.CX - SW * 0.5f, Y + 204.0f * S, SW, 32.0f * S, 16.0f * S, WithAlpha(AC, Al));
		P.TextMid(Side, F.CX, Y + 220.0f * S, 15.0f, WithAlpha(InkText, Al), 0.5f, true, 2.0f, 0.0f);
		const FString Goal = A == EKGAlignment::Impatient
			                     ? TEXT("Kill the villagers before they find you. [B] draws your blade - stab them in the back.")
			                     : A == EKGAlignment::Neutral
			                     ? TEXT("Survive, and follow your own goal.")
			                     : TEXT("Find the Impatient among you. Never turn your back on anyone.");
		P.TextMid(Goal, F.CX, Y + 288.0f * S, 21.0f, WithAlpha(Cream, Al), 0.5f, false);
	}

	void DrawDeathBanner(const FKGHudFrame& F)
	{
		const FKGPainter& P = F.P;
		const float S = F.S;
		const float A = Saturate(static_cast<float>(F.Fx->Now - F.Fx->DeadAt) / 0.6f);
		P.Vignette(GhostColor, 0.55f, [A](float) { return 0.28f * A; });
		const float Y = F.H * 0.34f;
		const float BH = 220.0f * S;
		const FLinearColor Band = Srgb(60, 4, 16);
		const FVector2D Upper[4] = {FVector2D(0.0f, Y), FVector2D(F.W, Y), FVector2D(F.W, Y + BH * 0.5f), FVector2D(0.0f, Y + BH * 0.5f)};
		const FVector2D Lower[4] = {FVector2D(0.0f, Y + BH * 0.5f), FVector2D(F.W, Y + BH * 0.5f), FVector2D(F.W, Y + BH),
		                            FVector2D(0.0f, Y + BH)};
		P.Poly(MakeArrayView(Upper), WithAlpha(Band, 0.0f), WithAlpha(Band, 0.8f * A), 0.0f);
		P.Poly(MakeArrayView(Lower), WithAlpha(Band, 0.8f * A), WithAlpha(Band, 0.0f), 0.0f);
		P.TextMid(TEXT("YOU DIED"), F.CX, Y + BH * 0.42f, 80.0f, WithAlpha(ImpatientColor, A), 0.5f, true, 8.0f);
		P.TextMid(TEXT("Your secrets die with you... for now. Wait for the round to end."), F.CX, Y + BH * 0.8f, 20.0f,
		          WithAlpha(Cream, 0.85f * A), 0.5f, false);
	}

	void DrawEpilogue(const FKGHudFrame& F)
	{
		const AKGGameState* GS = F.GS;
		const FKGPainter& P = F.P;
		const float S = F.S;
		const bool bTown = GS->Winner != EKGAlignment::Impatient;
		const FLinearColor AC = bTown ? TownColor : ImpatientColor;
		const float Age = F.Demo == 4 ? 10.0f : static_cast<float>(F.Fx->Now - F.Fx->PhaseAt);
		const float T = EaseOutBack(Age / 0.5f);
		const float Al = Saturate(Age / 0.3f);
		TArray<const AKGPlayerState*> Players;
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			if (const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
			{
				Players.Add(PS);
			}
		}
		const int32 Cols = Players.Num() > 8 ? 2 : 1;
		const int32 Rows = (Players.Num() + Cols - 1) / Cols;
		const float W = (Cols == 2 ? 1160.0f : 880.0f) * S;
		const float RowH = 42.0f * S;
		const float H = 150.0f * S + RowH * Rows + 58.0f * S;
		const float X = F.CX - W * 0.5f;
		const float Y = FMath::Max(F.H * 0.12f, (F.H - H) * 0.42f) + (1.0f - T) * 30.0f * S;
		const float R = 20.0f * S;
		P.Glow(X, Y, W, H, R, 36.0f * S, WithAlpha(AC, 0.4f * Al));
		P.Card(X, Y, W, H, Al, R, true);
		P.TopBand(X, Y, W, R, 10.0f * S, WithAlpha(AC, Al));
		P.TextMid(bTown ? TEXT("THE VILLAGE WINS") : TEXT("THE IMPATIENT WIN"), F.CX, Y + 62.0f * S, 52.0f, WithAlpha(AC, Al),
		          0.5f, true, 3.0f);
		P.TextMid(bTown ? TEXT("The Impatient were found out. Mr. Godot is still coming tomorrow.")
		                : TEXT("The village fell before Mr. Godot came."),
		          F.CX, Y + 108.0f * S, 18.0f, WithAlpha(Cream, 0.7f * Al), 0.5f, false);
		const AKGPlayerState* Me = F.Me;
		const float ColW = (W - 48.0f * S - (Cols - 1) * 16.0f * S) / Cols;
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			const AKGPlayerState* PS = Players[i];
			const int32 Col = i / Rows;
			const int32 Row = i % Rows;
			const float RX = X + 24.0f * S + Col * (ColW + 16.0f * S);
			const float RowY = Y + 140.0f * S + Row * RowH;
			const FKGRoleInfo* Role = RoleOf(PS->RevealedRoleId);
			const FLinearColor RC = Role ? AlignmentColor(Role->GetAlignment()) : Cream;
			const float Dim = PS->IsAlive() ? 1.0f : 0.5f;
			const float MidY = RowY + RowH * 0.5f;
			if (PS == Me)
			{
				P.RoundRect(RX, RowY + 3.0f * S, ColW, RowH - 6.0f * S, 10.0f * S, WithAlpha(Gold, 0.16f * Al));
			}
			else if (Row % 2 == 0)
			{
				P.RoundRect(RX, RowY + 3.0f * S, ColW, RowH - 6.0f * S, 10.0f * S, FLinearColor(1.0f, 1.0f, 1.0f, 0.04f * Al));
			}
			P.Circle(FVector2D(RX + 24.0f * S, MidY), 7.0f * S, WithAlpha(RC, Dim * Al));
			const FString Name = PS == Me ? PS->GetPlayerName() + TEXT("  (you)") : KGStreamer::DisplayName(PS);
			P.TextMid(Name, RX + 44.0f * S, MidY, 20.0f, WithAlpha(PS == Me ? Gold : Cream, Dim * Al), 0.0f, true);
			const FVector2D RoleSize = P.TextMid(Role ? PrettyRole(Role->RoleId) : TEXT("?"), RX + ColW - 20.0f * S, MidY, 20.0f,
			                                     WithAlpha(RC, Dim * Al), 1.0f, true);
			if (!PS->IsAlive())
			{
				P.TextMid(TEXT("DEAD"), RX + ColW - 32.0f * S - static_cast<float>(RoleSize.X), MidY, 12.0f,
				          WithAlpha(ImpatientColor, 0.8f * Al), 1.0f, true, 2.0f);
			}
		}
		const int32 Next = FMath::Max(0, FMath::CeilToInt(GS->GetPhaseRemaining()));
		P.TextMid(FString::Printf(TEXT("NEXT ROUND IN %d"), Next), F.CX, Y + H - 30.0f * S, 14.0f, WithAlpha(Cream, 0.6f * Al),
		          0.5f, true, 2.5f);
	}

	void DrawMeeting(const FKGHudFrame& F, float Top)
	{
		const AKGGameState* GS = F.GS;
		const FKGPainter& P = F.P;
		const float S = F.S;
		TMap<const AKGPlayerState*, int32> Counts;
		int32 AliveNow = 0;
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (PS && PS->IsAlive())
			{
				++AliveNow;
				if (PS->AccuseTarget)
				{
					Counts.FindOrAdd(PS->AccuseTarget)++;
				}
			}
		}
		const int32 Needed = AliveNow / 2 + 1;
		TArray<TPair<const AKGPlayerState*, int32>> Rows;
		for (const TPair<const AKGPlayerState*, int32>& Pair : Counts)
		{
			Rows.Add(Pair);
		}
		Rows.Sort([](const TPair<const AKGPlayerState*, int32>& A, const TPair<const AKGPlayerState*, int32>& B)
		{
			return A.Value > B.Value;
		});
		const float X = 28.0f * S;
		const float W = 440.0f * S;
		const float RowH = 40.0f * S;
		const float Y = FMath::Max(Top + 16.0f * S, F.H * 0.3f);
		const float H = 100.0f * S + RowH * FMath::Max(1, Rows.Num()) + 10.0f * S;
		P.Card(X, Y, W, H);
		P.TopBand(X, Y, W, 16.0f * S, 5.0f * S, TownColor);
		P.TextMid(TEXT("TOWN MEETING"), X + 20.0f * S, Y + 30.0f * S, 15.0f, TownColor, 0.0f, true, 2.5f);
		P.TextMid(FString::Printf(TEXT("%d VOTES = TRIAL"), Needed), X + W - 20.0f * S, Y + 30.0f * S, 14.0f, Gold, 1.0f,
		          true, 1.5f);
		P.KeyHint(TEXT("V"), TEXT("Accuse who you look at"), X + 20.0f * S, Y + 70.0f * S, 18.0f, Cream);
		float RowY = Y + 100.0f * S;
		if (Rows.Num() == 0)
		{
			P.TextMid(TEXT("No accusations yet."), X + 20.0f * S, RowY + RowH * 0.5f, 17.0f, WithAlpha(Cream, 0.5f), 0.0f, false);
		}
		for (const TPair<const AKGPlayerState*, int32>& Row : Rows)
		{
			const bool bMine = F.Me && F.Me->AccuseTarget == Row.Key;
			const bool bHot = Row.Value + 1 >= Needed;
			const FLinearColor Col = bHot ? ImpatientColor : Gold;
			const float MidY = RowY + RowH * 0.5f;
			if (bMine)
			{
				P.RoundRect(X + 10.0f * S, RowY + 3.0f * S, W - 20.0f * S, RowH - 6.0f * S, 10.0f * S, WithAlpha(Gold, 0.14f));
			}
			P.TextMid(KGStreamer::DisplayName(Row.Key), X + 20.0f * S, MidY, 18.0f, bMine ? Gold : Cream, 0.0f, true);
			P.PillBar(X + 200.0f * S, MidY - 5.0f * S, W - 290.0f * S, 10.0f * S, static_cast<float>(Row.Value) / Needed,
			          FMath::Lerp(Col, FLinearColor::White, 0.3f), Col);
			P.TextMid(FString::Printf(TEXT("%d/%d"), Row.Value, Needed), X + W - 20.0f * S, MidY, 17.0f, Col, 1.0f, true);
			RowY += RowH;
		}
	}

	void DrawVoteChip(const FKGHudFrame& F, const FString& Key, const FString& Label, float X, float MidY,
	                  const FLinearColor& Col, bool bChosen, bool bVoted, float AlignX)
	{
		const FKGPainter& P = F.P;
		const float S = F.S;
		constexpr float Px = 19.0f;
		const float W = P.KeyHintWidth(Key, Label, Px) + 32.0f * S;
		const float H = 46.0f * S;
		const float X0 = X - W * AlignX;
		P.RoundRect(X0, MidY - H * 0.5f, W, H, H * 0.5f, WithAlpha(Col, bChosen ? 0.38f : 0.12f));
		if (bChosen)
		{
			P.Outline(X0, MidY - H * 0.5f, W, H, H * 0.5f, FMath::Max(1.5f, 2.0f * S), Col);
		}
		P.KeyHint(Key, Label, X0 + 16.0f * S, MidY, Px, bChosen ? FLinearColor::White : Cream, 0.0f,
		          bVoted && !bChosen ? 0.45f : 1.0f);
	}

	void DrawTrial(const FKGHudFrame& F)
	{
		const AKGGameState* GS = F.GS;
		const FKGPainter& P = F.P;
		const float S = F.S;
		int32 Guilty = 0;
		int32 Innocent = 0;
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			if (PS && PS->IsAlive() && PS != GS->OnTrial)
			{
				Guilty += PS->Verdict == 1 ? 1 : 0;
				Innocent += PS->Verdict == 2 ? 1 : 0;
			}
		}
		const float W = 700.0f * S;
		const float H = 172.0f * S;
		const float X = F.CX - W * 0.5f;
		const float Y = F.H * 0.66f;
		const float R = 18.0f * S;
		P.Glow(X, Y, W, H, R, 26.0f * S, WithAlpha(Crimson, 0.4f));
		P.Card(X, Y, W, H, 1.0f, R, true);
		P.TopBand(X, Y, W, R, 7.0f * S, Crimson);
		P.TextMid(TEXT("ON TRIAL"), F.CX, Y + 32.0f * S, 15.0f, ImpatientColor, 0.5f, true, 3.0f);
		P.TextMid(KGStreamer::DisplayName(GS->OnTrial), F.CX, Y + 70.0f * S, 40.0f, Cream, 0.5f, true, 1.0f);
		if (F.Me == GS->OnTrial)
		{
			P.TextMid(TEXT("Defend yourself - they are listening."), F.CX, Y + 130.0f * S, 20.0f, WithAlpha(Cream, 0.85f), 0.5f,
			          false);
		}
		else
		{
			const uint8 MyVote = F.Me ? F.Me->Verdict : 0;
			DrawVoteChip(F, TEXT("Y"), FString::Printf(TEXT("GUILTY  %d"), Guilty), F.CX - 12.0f * S, Y + 130.0f * S,
			             ImpatientColor, MyVote == 1, MyVote != 0, 1.0f);
			DrawVoteChip(F, TEXT("N"), FString::Printf(TEXT("INNOCENT  %d"), Innocent), F.CX + 12.0f * S, Y + 130.0f * S,
			             TownColor, MyVote == 2, MyVote != 0, 0.0f);
		}
	}

	// Third-person emote camera: no crosshair; full-body emotes get a small "move to stop" hint low in the centre.
	void DrawEmoteHint(const FKGHudFrame& F, const UKGEmoteComponent& Emote)
	{
		const FKGEmoteDef* Def = Emote.GetShownEmote();
		const float A = FMath::Clamp((Emote.GetCameraAlpha() - 0.3f) / 0.7f, 0.0f, 1.0f);
		if (!Def || !Def->IsFullBody() || A <= 0.01f)
		{
			return;
		}
		const float S = F.S;
		const float Px = 17.0f;
		const float MidY = F.CY + 300.0f * S;
		const FString Label = TEXT("Move to stop");
		const float W = F.P.KeyHintWidth(TEXT("WASD"), Label, Px);
		DrawLabelChip(F, MidY, W + 28.0f * S, 38.0f * S, A);
		F.P.KeyHint(TEXT("WASD"), Label, F.CX, MidY, Px, Cream, 0.5f, A);
	}

	// Minimap, full-screen map (M), location toast.
#include "UI/KGHUDMap.inl"

	// Fishing: charge, bite, reel minigame, catch card, toasts.
#include "UI/KGHUDFishing.inl"
}

bool KGMinimap::CloseFullMap(const APlayerController* PC)
{
	FKGMapFx* M = MapFxFor(PC);
	if (!M || !M->bFullOpen)
	{
		return false;
	}
	M->bFullOpen = false;
	return true;
}

bool KGMinimap::IsFullMapOpen(const APlayerController* PC)
{
	const FKGMapFx* M = MapFxFor(PC);
	return M && M->bFullOpen;
}

void AKGHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld())
	{
		return;
	}
	// Scale with resolution (designed at 1080p).
	const float S = Canvas->ClipY / 1080.0f;
	HudFx(this).Advance(GetWorld()->GetRealTimeSeconds());
	FKGHudFrame F = MakeFrame(this, Canvas, S);
	F.CrossCol = CrosshairColor;
	F.StabCol = BackstabColor;
	F.HealthCol = HealthColor;
	F.StaminaCol = StaminaColor;

	UpdateVitals(F);
	DrawScreenEffects(F);   // under everything
	DrawFishingVignette(F);
	DrawMatchInfo(S);
	const AKGCharacter* EmoteBody = Cast<AKGCharacter>(GetOwningPawn());
	const UKGEmoteComponent* Emote = EmoteBody ? EmoteBody->GetEmote() : nullptr;
	if (Emote && Emote->GetCameraAlpha() > 0.05f)
	{
		DrawEmoteHint(F, *Emote);
	}
	else
	{
		DrawCrosshair(F);
	}
	DrawVitals(F);          // always visible while alive (user request; supersedes "only when not full")
	DrawPurse(F);
	DrawFishing(F);
	DrawMapLayer(F, CardStartTime < 0.0f || GetWorld()->GetRealTimeSeconds() - CardStartTime < 4.0f);
	DrawLoadingCard(S);     // on top of everything
}

void AKGHUD::DrawBar(float X, float Y, float Width, float Height, float Alpha, const FLinearColor& Fill)
{
	FKGPainter(Canvas, Canvas->ClipY / 1080.0f)
		.PillBar(X, Y, Width, Height, Alpha, FMath::Lerp(Fill, FLinearColor::White, 0.3f), Fill);
}

void AKGHUD::Panel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	FKGPainter P(Canvas, Canvas->ClipY / 1080.0f);
	P.Card(X, Y, W, H, Color.A);
}

void AKGHUD::DrawTextAt(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, bool bCentre,
                        bool bLarge)
{
	// Scale 1 = 20 px body text at 1080p (resolution scaling is applied inside); bLarge = bold.
	const float S = Canvas->ClipY / 1080.0f;
	FKGPainter(Canvas, S).Text(Text, X, Y, 20.0f * Scale, Color, bCentre ? 0.5f : 0.0f, bLarge);
}

void AKGHUD::DrawMatchInfo(float S)
{
	FKGHudFrame F = MakeFrame(this, Canvas, S);
	if (!F.GS)
	{
		return;
	}
	FKGHudFx& Fx = *F.Fx;
	const EKGPhase Phase = F.GS->GetPhase();
	if (Phase != Fx.Phase)
	{
		Fx.Phase = Phase;
		Fx.PhaseAt = Fx.Now;
	}
	const bool bDead = F.Me && !F.Me->IsAlive() && Phase != EKGPhase::Epilogue;
	if (bDead && Fx.DeadAt < 0.0)
	{
		Fx.DeadAt = Fx.Now;
	}
	else if (!bDead)
	{
		Fx.DeadAt = -1.0;
	}

	const float PhaseBottom = DrawPhasePanel(F);
	if (Phase != EKGPhase::Epilogue && F.Demo != 4)
	{
		DrawAnnouncement(F, PhaseBottom + 16.0f * S);
	}
	const float ChoresBottom = DrawChores(F);
	const FKGRoleInfo* MyRole = F.Me ? RoleOf(F.Me->GetPrivateRoleId()) : nullptr;   // secret: only the owner has it
	// Streamer mode: after the reveal the role stays off screen unless the peek key is held.
	const bool bRoleHidden = MyRole && KGStreamer::IsRoleHidden(F.PC);
	if (MyRole && bRoleHidden)
	{
		DrawHiddenRoleChip(F);
	}
	else if (MyRole)
	{
		DrawRoleChip(F, *MyRole);
	}
	if (Phase == EKGPhase::Meeting)
	{
		DrawMeeting(F, ChoresBottom);
	}
	if (Phase == EKGPhase::Trial && F.GS->OnTrial)
	{
		DrawTrial(F);
	}
	// The full-screen ceremony (UI/Reveal) replaces this card whenever it runs; this stays as the fallback.
	if (Phase == EKGPhase::RoleReveal && MyRole && !bRoleHidden && !UKGRevealSubsystem::IsOverlayShown(GetWorld()))
	{
		DrawRoleCard(F, *MyRole);
	}
	if (bDead)
	{
		DrawDeathBanner(F);
	}
	if ((Phase == EKGPhase::Epilogue && F.GS->bHasWinner) || F.Demo == 4)
	{
		DrawEpilogue(F);
	}
}

void AKGHUD::DrawLoadingCard(float S)
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (CardStartTime < 0.0f)
	{
		CardStartTime = Now;
		TipIndex = FMath::RandRange(0, 5);   // cosmetic, not gameplay randomness
	}
	const float Age = Now - CardStartTime;
	constexpr float Hold = 3.2f;
	constexpr float Fade = 0.8f;
	if (Age > Hold + Fade)
	{
		return;
	}
	const float A = Age < Hold ? 1.0f : 1.0f - (Age - Hold) / Fade;
	// Map display name from the level ("UEDPIE_0_L_Morrowmere" -> "MORROWMERE").
	FString Map = GetWorld()->GetMapName();
	Map.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
	Map.RemoveFromStart(TEXT("L_"));
	if (const int32 V = Map.Find(TEXT("_v"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	    V != INDEX_NONE && V + 2 < Map.Len() && Map.Mid(V + 2).IsNumeric())
	{
		Map.LeftInline(V);   // "Morrowmere_v2" -> "Morrowmere" (layout revisions share the village name)
	}
	Map = Map.ToUpper();
	const TCHAR* Tips[] = {
		TEXT("Tip: never turn your back on anyone. The Impatient stab from behind."),
		TEXT("Tip: chores fill the village's preparation - when it is full the lighthouse exposes a killer."),
		TEXT("Tip: [V] at a meeting accuses whoever you are looking at."),
		TEXT("Tip: lock your door at night. Or don't, and see who knocks."),
		TEXT("Tip: Mr. Godot is coming tomorrow. He is always coming tomorrow."),
		TEXT("Tip: punch crates open - some villagers hide things in them."),
	};
	const FKGPainter P(Canvas, S);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const FVector2D Screen[4] = {FVector2D(0.0f, 0.0f), FVector2D(W, 0.0f), FVector2D(W, H), FVector2D(0.0f, H)};
	P.Poly(MakeArrayView(Screen), WithAlpha(Srgb(22, 26, 44), A), WithAlpha(Srgb(10, 8, 18), A), 0.0f);
	P.Vignette(Srgb(255, 150, 60), 0.2f, [A](float Angle)
	{
		// Warm lantern light pooling at the bottom of the card.
		return 0.12f * A * FMath::Max(0.0f, FMath::Sin(Angle));
	});
	const float Rule = H * 0.62f;
	P.Text(Map, 88.0f * S, Rule - 160.0f * S, 120.0f, WithAlpha(Cream, A), 0.0f, true, 6.0f, 0.5f);
	P.RoundRect(94.0f * S, Rule, 560.0f * S, 5.0f * S, 2.5f * S, WithAlpha(Gold, A));
	P.Text(TEXT("CLASSIC  ·  6-20 PLAYERS  ·  A FISHING VILLAGE WHERE MR. GODOT NEVER ARRIVES"), 94.0f * S,
	       Rule + 22.0f * S, 17.0f, WithAlpha(Gold, A), 0.0f, true, 2.0f);
	P.Text(Tips[TipIndex], 94.0f * S, Rule + 64.0f * S, 20.0f, WithAlpha(Cream, 0.78f * A), 0.0f, false);
	const float Progress = FMath::Clamp(Age / Hold, 0.0f, 1.0f);
	P.Text(FString::Printf(TEXT("LOADING  %d%%"), FMath::RoundToInt(Progress * 100.0f)), W - 380.0f * S, H - 118.0f * S,
	       14.0f, WithAlpha(Cream, 0.8f * A), 0.0f, true, 2.5f);
	P.PillBar(W - 380.0f * S, H - 84.0f * S, 300.0f * S, 9.0f * S, Progress, GoldLight, Gold, A);
}
