#include "UI/Reveal/SKGRoleReveal.h"

#include "Engine/Texture2D.h"
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
	float EaseIn(float T) { const float X = Sat(T); return X * X; }
	float EaseInOut(float T) { const float X = Sat(T); return X * X * (3.0f - 2.0f * X); }
	float Ramp(float Time, float Start, float Length) { return Sat((Time - Start) / FMath::Max(Length, 0.001f)); }
	/** 0 before Start, then an exponential decay with time constant Tau. */
	float Decay(float Time, float Start, float Tau) { return Time < Start ? 0.0f : FMath::Exp(-(Time - Start) / Tau); }
	FLinearColor A(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, C.A * Alpha); }
	FLinearColor Hex(uint32 RGB, float Alpha = 1.0f) { return FKGMenuStyle::Hex(RGB, Alpha); }
	FLinearColor Mul(const FLinearColor& C, float K) { return FLinearColor(C.R * K, C.G * K, C.B * K, C.A); }
	FLinearColor Mix(const FLinearColor& X, const FLinearColor& Y, float T) { return X + (Y - X) * T; }
	/** Stable pseudo random 0..1 for particle I, channel K (no RNG state: shots and live frames agree). */
	float Hash(int32 I, int32 K) { return FMath::Frac(FMath::Sin(I * 12.9898f + K * 78.233f) * 43758.5453f); }

	// Palette (Docs/08_UI_UX.md section 1).
	const FLinearColor Ink = Hex(0x1A1320);
	const FLinearColor Cream = Hex(0xFFF6E6);
	const FLinearColor BackRim = Hex(0xD9AE4E);
	const FLinearColor BackFill = Hex(0x5B1A2E);
	const FLinearColor BackDeep = Hex(0x43121F);

	/** Rotation + scale around a centre: local units -> widget space (painted fallbacks). */
	struct FXf
	{
		FV C;
		float Cos = 1.0f;
		float Sin = 0.0f;
		float SX = 1.0f;
		float SY = 1.0f;

		FXf(const FV& Centre, float AngleDeg = 0.0f, float ScaleX = 1.0f, float ScaleY = 1.0f)
			: C(Centre), Cos(FMath::Cos(FMath::DegreesToRadians(AngleDeg))), Sin(FMath::Sin(FMath::DegreesToRadians(AngleDeg))),
			  SX(ScaleX), SY(ScaleY)
		{
		}

		FV Map(const FV& L) const
		{
			const FV S(L.X * SX, L.Y * SY);
			return C + FV(S.X * Cos - S.Y * Sin, S.X * Sin + S.Y * Cos);
		}
	};

	void FillLocal(FKGVertexCanvas& C, const FXf& X, std::initializer_list<FV> Points, const FLinearColor& Col)
	{
		TArray<FV> P;
		for (const FV& L : Points)
		{
			P.Add(X.Map(L));
		}
		C.Convex(P, Col);
	}

	/** Painted alignment emblem (fallback when the illustration texture is missing): house, blade, diamond. */
	void Emblem(FKGVertexCanvas& C, const FXf& X, EKGAlignment Align, float S, const FLinearColor& Col, const FLinearColor& Cut)
	{
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

	// --- Textures (Tools/UI/kg_make_reveal_art.py -> /Game/KillGodot/UI/Reveal) ----------------------------------------

	/** Brush for a reveal texture, or null (not imported yet: the painted fallback draws instead). Game thread only. */
	const FSlateBrush* Art(const TCHAR* Name)
	{
		struct FEntry
		{
			TSharedPtr<FSlateBrush> Brush;
			uint64 TriedFrame = 0;
			bool bTried = false;
		};
		static TMap<FName, FEntry> Cache;
		if (!IsInGameThread())
		{
			return nullptr;
		}
		FEntry& Entry = Cache.FindOrAdd(FName(Name));
		if (!Entry.Brush.IsValid() && (!Entry.bTried || GFrameCounter - Entry.TriedFrame > 600))
		{
			Entry.bTried = true;
			Entry.TriedFrame = GFrameCounter;
			const FString Path = FString::Printf(TEXT("/Game/KillGodot/UI/Reveal/%s.%s"), Name, Name);
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Texture->AddToRoot();   // the brush holds a raw pointer for the session
				Texture->WaitForPendingInitOrStreaming();   // async creation would leave the first frames blank
				Entry.Brush = MakeShared<FSlateBrush>();
				Entry.Brush->SetResourceObject(Texture);
				Entry.Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Entry.Brush->DrawAs = ESlateBrushDrawType::Image;
				Entry.Brush->Tiling = ESlateBrushTileType::NoTile;
			}
		}
		return Entry.Brush.Get();
	}

	/** Loads every reveal texture up front (the ceremony starts in the lobby, long before the flip needs them). */
	void PreloadArt()
	{
		for (const TCHAR* Name : {TEXT("T_KG_Reveal_Town"), TEXT("T_KG_Reveal_Impatient"), TEXT("T_KG_Reveal_Neutral"),
		                          TEXT("T_KG_Reveal_Mate0"), TEXT("T_KG_Reveal_Mate1"), TEXT("T_KG_Reveal_Mate2")})
		{
			Art(Name);
		}
	}

	const TCHAR* IllustrationName(EKGAlignment Align)
	{
		return Align == EKGAlignment::Impatient ? TEXT("T_KG_Reveal_Impatient")
			: Align == EKGAlignment::Neutral ? TEXT("T_KG_Reveal_Neutral")
			: TEXT("T_KG_Reveal_Town");
	}

	void Image(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FSlateBrush* Brush, const FV& Pos, const FV& Size,
	           const FLinearColor& Tint)
	{
		if (Brush && Tint.A > 0.004f)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Tint);
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
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(P)), S, Font, ESlateDrawEffect::None, Col);
		return Size;
	}

	/** Big display text: soft drop shadow + dark outline, so it reads on any flood. */
	FVector2f Display(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& S, FSlateFontInfo Font, const FV& Pos,
	                  const FLinearColor& Col, float Outline, float Shadow, float AlignX = 0.5f, float AlignY = 0.5f)
	{
		Font.OutlineSettings.OutlineSize = FMath::Max(0, FMath::RoundToInt(Outline));
		Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.01f, 0.03f, 0.92f * Col.A);
		if (Shadow > 0.0f)
		{
			Text(Out, Layer, Geo, S, Font, Pos + FV(0.0f, Shadow), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Col.A), AlignX, AlignY);
		}
		return Text(Out, Layer + 1, Geo, S, Font, Pos, Col, AlignX, AlignY);
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
		for (int32 Try = 0; Try < 16 && Measure(S, Font).X > MaxWidth; ++Try)
		{
			Font.Size *= 0.93f;
		}
		return Font;
	}

	/** Child geometry scaled by S around Centre (text slams, the camera punch). Fonts render sharp at the new size. */
	FGeometry ScaledAbout(const FGeometry& Geo, const FV& Size, const FV& Centre, float S, const FV& Offset = FV(0.0f, 0.0f))
	{
		return Geo.MakeChild(Size, FSlateLayoutTransform(S, Centre * (1.0f - S) + Offset));
	}

	/** Child geometry for a card: local box (0,0)-(W,H) centred on Centre, scaled (flip) and rotated about its middle. */
	FGeometry CardSpace(const FGeometry& Geo, const FV& Centre, const FV& CardSize, float ScaleX, float ScaleY, float AngleDeg)
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const float Co = FMath::Cos(Rad);
		const float Si = FMath::Sin(Rad);
		// Row-vector convention (Slate): p' = p * Scale * Rotation.
		const FMatrix2x2f M(ScaleX * Co, ScaleX * Si, -ScaleY * Si, ScaleY * Co);
		return Geo.MakeChild(CardSize, FSlateLayoutTransform(Centre - CardSize * 0.5f), FSlateRenderTransform(M, FVector2f::ZeroVector),
		                     FVector2f(0.5f, 0.5f));
	}

	/** The back of a Morrowmere card (card-local space): brass rim, crimson field, the cracked clock. */
	void CardBack(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Card, const FV& Size, float U, float Glow)
	{
		const float R = Size.X * 0.09f;
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Card, FV(0.0f, 0.0f), Size, BackRim, R);
		FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Card, FV(10.0f, 10.0f) * U, Size - FV(20.0f, 20.0f) * U, BackFill, R * 0.8f);
		FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Card, FV(24.0f, 24.0f) * U, Size - FV(48.0f, 48.0f) * U, BackDeep, R * 0.6f,
		                             A(BackRim, 0.6f), 3.0f * U);
		FKGVertexCanvas C(Out, Card);
		const FV Mid = Size * 0.5f;
		const float Ring = Size.X * 0.3f;
		C.Ellipse(Mid, FV(Ring * 1.9f, Ring * 1.9f), A(Hex(0xFFD27A), 0.35f * Glow), A(Hex(0xFFD27A), 0.0f), 40);
		C.Ellipse(Mid, FV(Ring, Ring), BackRim, BackRim, 40);
		C.Ellipse(Mid, FV(Ring * 0.84f, Ring * 0.84f), BackFill, BackDeep, 40);
		const FXf X(Mid);
		const float W = Size.X * 0.025f;
		FillLocal(C, X, {FV(-W, 0.0f), FV(W, 0.0f), FV(W * 0.4f, -Ring * 0.68f), FV(-W * 0.4f, -Ring * 0.68f)}, BackRim);
		FillLocal(C, X, {FV(0.0f, -W), FV(0.0f, W), FV(Ring * 0.5f, Ring * 0.22f + W * 0.3f), FV(Ring * 0.5f, Ring * 0.22f - W * 0.3f)}, BackRim);
		FillLocal(C, X, {FV(-Ring * 0.2f, -Ring * 0.8f), FV(-Ring * 0.12f, -Ring * 0.8f), FV(Ring * 0.08f, -Ring * 0.2f), FV(0.0f, -Ring * 0.2f)},
		          BackDeep);
		for (const float Y : {-Size.Y * 0.37f, Size.Y * 0.37f})
		{
			FillLocal(C, X, {FV(0.0f, Y - Size.X * 0.05f), FV(Size.X * 0.04f, Y), FV(0.0f, Y + Size.X * 0.05f), FV(-Size.X * 0.04f, Y)}, BackRim);
		}
		C.Flush(Layer + 2);
	}

	/** Your card, face up (card-local space): alignment rim, a glowing well, the big illustration. */
	void CardFace(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Card, const FV& Size, float U, EKGAlignment Align,
	              const FLinearColor& AC, float Glow)
	{
		const float R = Size.X * 0.09f;
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Card, FV(0.0f, 0.0f), Size, Mix(AC, FLinearColor::White, 0.25f), R);
		FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Card, FV(9.0f, 9.0f) * U, Size - FV(18.0f, 18.0f) * U, Mul(AC, 0.2f), R * 0.8f,
		                             A(FLinearColor::White, 0.25f), 2.0f * U);
		FKGVertexCanvas C(Out, Card);
		const FV Mid(Size.X * 0.5f, Size.Y * 0.46f);
		C.Ellipse(Mid, FV(Size.X * 0.44f, Size.Y * 0.4f), A(Mix(AC, FLinearColor::White, 0.2f), 0.85f), A(AC, 0.0f), 40);
		C.Ellipse(Mid, FV(Size.X * 0.25f, Size.X * 0.25f), A(FLinearColor::White, 0.35f * Glow), A(FLinearColor::White, 0.0f), 32);
		// Corner pips.
		for (const FV& Pip : {FV(Size.X * 0.13f, Size.Y * 0.08f), FV(Size.X * 0.87f, Size.Y * 0.92f)})
		{
			const float P = Size.X * 0.035f;
			C.Convex({Pip + FV(0.0f, -P * 1.3f), Pip + FV(P, 0.0f), Pip + FV(0.0f, P * 1.3f), Pip + FV(-P, 0.0f)}, Mix(AC, FLinearColor::White, 0.4f));
		}
		const FSlateBrush* Brush = Art(IllustrationName(Align));
		if (!Brush)
		{
			Emblem(C, FXf(Mid), Align, Size.X * 0.62f, Cream, Mul(AC, 0.25f));
		}
		C.Flush(Layer + 2);
		const float Side = Size.X * 0.96f;
		Image(Out, Layer + 3, Card, Brush, FV(Mid.X - Side * 0.5f, Mid.Y - Side * 0.5f), FV(Side, Side), FLinearColor::White);
	}

	/** An accomplice bust: texture tinted in team colours, or a painted head-and-shoulders when it is missing. */
	void Bust(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, int32 Variant, const FV& Pos, float Side,
	          const FLinearColor& Tint)
	{
		static const TCHAR* Names[3] = {TEXT("T_KG_Reveal_Mate0"), TEXT("T_KG_Reveal_Mate1"), TEXT("T_KG_Reveal_Mate2")};
		if (const FSlateBrush* Brush = Art(Names[Variant % 3]))
		{
			// Sticker border: the same bust a little larger in cream underneath, then the tinted one.
			const float Grow = Side * 0.045f;
			Image(Out, Layer, Geo, Brush, Pos - FV(Grow, Grow * 1.6f), FV(Side + Grow * 2.0f, Side + Grow * 2.0f),
			      FLinearColor(1.0f, 0.95f, 0.88f, Tint.A));
			Image(Out, Layer + 1, Geo, Brush, Pos, FV(Side, Side), Tint);
			return;
		}
		FKGVertexCanvas C(Out, Geo);
		C.Ellipse(Pos + FV(Side * 0.5f, Side * 0.42f), FV(Side * 0.17f, Side * 0.19f), Tint, Tint, 32);
		C.Convex({Pos + FV(Side * 0.16f, Side), Pos + FV(Side * 0.22f, Side * 0.66f), Pos + FV(Side * 0.78f, Side * 0.66f),
		          Pos + FV(Side * 0.84f, Side)}, Mul(Tint, 0.85f));
		C.Flush(Layer);
	}

	// --- Particles ----------------------------------------------------------------------------------------------------

	/**
	 * The burst on the flip, deterministic in Time: Town throws confetti, the Impatient spray embers that rise, Neutrals
	 * scatter glitter. Everything flies out of Origin with drag and falls (or rises) with its own gravity.
	 */
	void Burst(FKGVertexCanvas& C, EKGAlignment Align, const FLinearColor& AC, const FV& Origin, float Since, float U)
	{
		if (Since < 0.0f)
		{
			return;
		}
		constexpr int32 Count = 190;
		const FLinearColor Gold = Hex(0xFFD23F);
		const FLinearColor Palette[4] = {AC, Gold, Cream, Mix(AC, FLinearColor::White, 0.5f)};
		for (int32 I = 0; I < Count; ++I)
		{
			const float Life = 1.3f + 1.6f * Hash(I, 3);
			if (Since > Life)
			{
				continue;
			}
			const float Ang = 2.0f * PI * Hash(I, 1);
			const float Speed = (700.0f + 1500.0f * Hash(I, 2) * Hash(I, 2)) * U;
			constexpr float Drag = 2.6f;
			const float Gravity = (Align == EKGAlignment::Impatient ? -140.0f : Align == EKGAlignment::Neutral ? 60.0f : 420.0f) * U;
			const FV Dir(FMath::Cos(Ang), FMath::Sin(Ang) * 0.8f);
			const float Travel = (1.0f - FMath::Exp(-Drag * Since)) / Drag;
			const FV P = Origin + Dir * Speed * Travel + FV(0.0f, 0.5f * Gravity * Since * Since);
			const float Vel = Speed * FMath::Exp(-Drag * Since);
			const float Fade = 1.0f - FMath::Square(Since / Life);
			const float Size = (7.0f + 13.0f * Hash(I, 4)) * U;
			const FLinearColor Col = Palette[static_cast<int32>(Hash(I, 5) * 3.99f)];
			switch (Align)
			{
			case EKGAlignment::Impatient:
			{
				// Embers: hot streaks along the velocity, cooling from white-gold to crimson.
				const FV V = (Dir * Vel + FV(0.0f, Gravity * Since)).GetSafeNormal();
				const float Len = Size * (1.2f + Vel / (220.0f * U));
				const FV N(-V.Y, V.X);
				const float Heat = Sat(1.0f - Since / (Life * 0.6f));
				const FLinearColor Hot = Mix(Mix(Hex(0xFF3B2F), Gold, Heat), FLinearColor::White, Heat * Heat * 0.6f);
				const float Wd = Size * 0.3f;
				C.Quad(P - V * Len + N * Wd * 0.2f, P + N * Wd, P - N * Wd, P - V * Len - N * Wd * 0.2f, A(Hot, 0.0f), A(Hot, Fade),
				       A(Hot, Fade), A(Hot, 0.0f));
				break;
			}
			case EKGAlignment::Neutral:
			{
				// Glitter: twinkling diamonds.
				const float Tw = 0.45f + 0.55f * FMath::Abs(FMath::Sin(Since * (9.0f + 8.0f * Hash(I, 6)) + Hash(I, 7) * 6.0f));
				const float S = Size * 0.8f;
				C.Convex({P + FV(0.0f, -S * 1.4f), P + FV(S * 0.7f, 0.0f), P + FV(0.0f, S * 1.4f), P + FV(-S * 0.7f, 0.0f)}, A(Col, Fade * Tw));
				break;
			}
			default:
			{
				// Confetti: spinning, fluttering paper.
				const float Rot = Hash(I, 6) * 6.28f + Since * (4.0f + 8.0f * Hash(I, 7));
				const float Flutter = FMath::Cos(Since * (7.0f + 6.0f * Hash(I, 8)) + Hash(I, 9) * 6.0f);
				const FV Ax(FMath::Cos(Rot) * Size * Flutter, FMath::Sin(Rot) * Size * Flutter);
				const FV Ay(-FMath::Sin(Rot) * Size * 0.55f, FMath::Cos(Rot) * Size * 0.55f);
				const FLinearColor Paper = A(Col, Fade);
				C.Quad(P - Ax - Ay, P + Ax - Ay, P + Ax + Ay, P - Ax + Ay, Paper, Paper, Paper, Paper);
				break;
			}
			}
		}
	}

	/** Slow motes / embers drifting up through the flood while you read (Anim = free-running clock). */
	void Motes(FKGVertexCanvas& C, const FLinearColor& Col, float W, float H, float U, float Anim, float Alpha)
	{
		for (int32 I = 0; I < 46; ++I)
		{
			const float Speed = 0.05f + 0.08f * Hash(I, 11);
			const float Y = 1.0f - FMath::Frac(Hash(I, 12) + Anim * Speed);
			const float X = Hash(I, 13) + 0.02f * FMath::Sin(Anim * (0.8f + Hash(I, 14)) + I);
			const float R = (2.0f + 4.0f * Hash(I, 15)) * U;
			const float Edge = Sat(FMath::Min(Y, 1.0f - Y) * 6.0f);
			C.Ellipse(FV(W * X, H * Y), FV(R, R), A(Col, Alpha * Edge * (0.35f + 0.5f * Hash(I, 16))), A(Col, 0.0f), 8);
		}
	}
}


void SKGRoleReveal::Construct(const FArguments& InArgs)
{
	Provider = InArgs._ViewProvider;
	View = InArgs._StaticView;
	FixedTime = InArgs._FixedTime;
	Time = FixedTime >= 0.0f ? FixedTime : 0.0f;
	AnimTime = Time;
	DetailsAlpha = View.bDetails ? 1.0f : 0.0f;
	KGRevealPaint::PreloadArt();
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
		AnimTime = FixedTime;
		ReadyAge = View.bReady ? 1.0f : 0.0f;
		DetailsAlpha = View.bDetails ? 1.0f : 0.0f;
		return;
	}
	AnimTime += InDeltaTime;
	DetailsAlpha = FKGMenuStyle::Approach(DetailsAlpha, View.bDetails && Time >= KGReveal::FlipEnd ? 1.0f : 0.0f, InDeltaTime, 18.0f);
	if (View.bEnding)
	{
		OutroTime += InDeltaTime;
	}
	if (View.ServerElapsed < 0.0f)
	{
		// Still the lobby's last frames (the phase has not replicated yet): the dark room waits.
		Time = FMath::Min(Time + InDeltaTime, 0.1f);
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
		Time = FMath::Min(Time, KGReveal::TeaseEnd - 0.02f);   // the card trembles face down until the owner-only role arrives
	}
}

int32 SKGRoleReveal::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	using namespace KGRevealPaint;   // function scope only (unity builds must not see these names)
	using namespace KGReveal;
	const FKGMenuStyle& Style = FKGMenuStyle::Get();
	const FV Size = FV(Geo.GetLocalSize());
	const float W = Size.X;
	const float H = Size.Y;
	const float U = FMath::Min(H / 1080.0f, W / 1560.0f);
	const float Alpha = Sat(1.0f - OutroTime / OutroSeconds) * InWidgetStyle.GetColorAndOpacityTint().A;
	if (Alpha <= 0.002f)
	{
		return LayerId;
	}
	const float T = Time;
	const float Anim = AnimTime;
	const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), View.RoleId);
	const EKGAlignment Align = Role ? Role->GetAlignment() : EKGAlignment::Town;
	const FLinearColor AC = KGRoleCard::AlignmentColor(Align);
	const FLinearColor Bright = Mix(AC, FLinearColor::White, 0.35f);
	const FKGRoleCardText Card = KGRoleCard::Get(View.RoleId);
	const bool bTr = KGRoleCard::IsTurkish();
	const bool bRevealed = T >= BangAt && !View.RoleId.IsNone();
	const float Since = T - BangAt;
	auto Fade = [Alpha](const FLinearColor& C) { return A(C, Alpha); };

	int32 Layer = LayerId;

	// --- Camera: punch on the flip and on the name landing, shake that dies out fast -----------------------------------
	const float Punch = 0.075f * Decay(T, BangAt, 0.16f) + 0.025f * Decay(T, NameAt + 0.2f, 0.1f);
	const float ShakeAmp = (16.0f * Decay(T, BangAt, 0.18f) + 6.0f * Decay(T, NameAt + 0.2f, 0.08f)) * U;
	const float Tremble = T < BangAt ? Ramp(T, SpinEnd - 0.1f, TeaseEnd - SpinEnd + 0.1f) : 0.0f;
	const FV Shake(FMath::Sin(Anim * 83.0f) * ShakeAmp, FMath::Cos(Anim * 67.0f) * ShakeAmp);
	const FGeometry Stage = ScaledAbout(Geo, Size, Size * 0.5f, 1.0f + Punch, Shake);

	// --- Background: the dark room before, the colour flood after ----------------------------------------------------
	{
		FKGVertexCanvas C(Out, Geo, Alpha);
		if (!bRevealed)
		{
			C.RectV(FV(0.0f, 0.0f), Size, Hex(0x1F1627), Hex(0x09070D));
			const float Spot = EaseOut(Ramp(T, 0.1f, 0.6f)) * (0.7f + 0.3f * Tremble);
			C.Ellipse(FV(W * 0.5f, H * 0.42f), FV(W * 0.42f, H * 0.62f), A(Hex(0xFFC34D), 0.22f * Spot), A(Hex(0xFFC34D), 0.0f), 48);
			Motes(C, Hex(0xFFD27A), W, H, U, Anim, 0.6f * Spot);
		}
		else
		{
			C.RectV(FV(0.0f, 0.0f), Size, Mul(AC, 0.34f), Mul(AC, 0.1f));
			const FV Heart(W * 0.5f, H * 0.4f);
			// Sun rays turning slowly behind the card.
			const float RayIn = EaseOut(Ramp(T, BangAt, 0.45f));
			const float Reach = FMath::Max(W, H) * 1.1f * RayIn;
			constexpr int32 Rays = 18;
			for (int32 Ray = 0; Ray < Rays; ++Ray)
			{
				const float A0 = 2.0f * PI * Ray / Rays + Anim * 0.12f;
				const float Half = PI / Rays * 0.45f;
				const FV P1 = Heart + FV(FMath::Cos(A0 - Half), FMath::Sin(A0 - Half)) * Reach;
				const FV P2 = Heart + FV(FMath::Cos(A0 + Half), FMath::Sin(A0 + Half)) * Reach;
				C.Tri(Heart, P1, P2, A(Bright, 0.26f), A(Bright, 0.0f), A(Bright, 0.0f));
			}
			C.Ellipse(Heart, FV(560.0f, 560.0f) * U * (0.8f + 0.2f * RayIn), A(Bright, 0.6f), A(AC, 0.0f), 56);
			Motes(C, Bright, W, H, U, Anim, 0.8f);
			// A darker floor under the name and the line, so white text always sits on a calm field.
			C.RectV(FV(0.0f, H * 0.6f), FV(W, H * 0.4f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		}
		// Vignette, harder on the punch.
		const float Vig = 0.6f + 0.35f * Decay(T, BangAt, 0.3f);
		const FLinearColor Clear(0.0f, 0.0f, 0.0f, 0.0f);
		const FLinearColor Dark(0.0f, 0.0f, 0.0f, Vig);
		C.RectH(FV(0.0f, 0.0f), FV(W * 0.2f, H), Dark, Clear);
		C.RectH(FV(W * 0.8f, 0.0f), FV(W * 0.2f, H), Clear, Dark);
		C.RectV(FV(0.0f, 0.0f), FV(W, H * 0.18f), A(Dark, 0.8f), Clear);
		C.RectV(FV(0.0f, H * 0.84f), FV(W, H * 0.16f), Clear, A(Dark, 0.9f));
		C.Flush(Layer);
		Layer += 2;
	}

	// --- Your card -------------------------------------------------------------------------------------------------------
	const FV CardSize = FV(290.0f, 400.0f) * U;
	const FV CardC(W * 0.5f, H * 0.405f);
	{
		FV Pos = CardC;
		float SX = 1.0f;
		float SY = 1.0f;
		float Angle = 0.0f;
		bool bFace = false;
		if (T < SpinEnd)
		{
			// Rises out of the dark, spinning, and lands with a little overshoot.
			const float P = Ramp(T, 0.05f, SpinEnd - 0.05f);
			const float E = FKGMenuStyle::EaseOutBack(P);
			Pos = FMath::Lerp(CardC + FV(0.0f, H * 0.75f), CardC, E);
			Angle = (1.0f - EaseOut(P)) * -200.0f;
			SX = SY = FMath::Lerp(0.5f, 1.0f, EaseOut(P));
		}
		else if (T < TeaseEnd || View.RoleId.IsNone())
		{
			// "Who are you?": hovering, trembling harder and harder.
			const float Amp = (1.5f + 7.0f * Tremble * Tremble) * U;
			Pos = CardC + FV(FMath::Sin(Anim * 57.0f) * Amp, FMath::Cos(Anim * 71.0f) * Amp - 10.0f * U * Tremble);
			Angle = FMath::Sin(Anim * 43.0f) * 2.5f * Tremble;
			SX = SY = 1.0f + 0.05f * Tremble;
		}
		else
		{
			// The flip, then an elastic settle.
			const float F = Ramp(T, TeaseEnd, FlipEnd - TeaseEnd);
			const float Turn = FMath::Cos(PI * F);
			bFace = F >= 0.5f;
			const float Settle = T >= FlipEnd ? 0.16f * Decay(T, FlipEnd, 0.12f) * FMath::Cos((T - FlipEnd) * 26.0f) : 0.0f;
			const float Lift = 1.0f + 0.12f * FMath::Sin(PI * F) + Settle;
			SX = FMath::Max(FMath::Abs(Turn), 0.02f) * Lift;
			SY = Lift;
			Angle = T >= FlipEnd ? -3.0f * Decay(T, FlipEnd, 0.3f) : 0.0f;
		}
		const FGeometry CardGeo = CardSpace(Stage, Pos, CardSize, SX, SY, Angle);
		// Drop shadow under the card (not rotated: it is a soft blob).
		{
			FKGVertexCanvas C(Out, Stage, Alpha);
			C.Ellipse(Pos + FV(0.0f, CardSize.Y * 0.56f * SY), FV(CardSize.X * 0.62f * SX, 26.0f * U), FLinearColor(0.0f, 0.0f, 0.0f, 0.45f),
			          FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 32);
			C.Flush(Layer);
		}
		if (!bFace && T >= SpinEnd * 0.5f)
		{
			FKGVertexCanvas C(Out, Stage, Alpha);
			const float Build = 0.25f + 0.75f * Tremble;
			const float Pulse = 1.0f + 0.06f * FMath::Sin(Anim * 14.0f);
			C.Ellipse(Pos, FV(CardSize.X * 1.5f, CardSize.Y * 1.1f) * Pulse * (0.8f + 0.5f * Tremble), A(Hex(0xFFC34D), 0.55f * Build),
			          A(Hex(0xFF8A2A), 0.0f), 48);
			C.Flush(Layer + 1);
		}
		// Mates stand behind the card's layer, so the card overlaps the nearest bust a little on narrow screens.
		const int32 CardLayer = Layer + 8;
		if (bFace)
		{
			CardFace(Out, CardLayer, CardGeo, CardSize, U, Align, AC, Decay(T, BangAt, 0.4f));
		}
		else
		{
			CardBack(Out, CardLayer, CardGeo, CardSize, U, Tremble);
		}
	}

	// --- Accomplices: a lineup beside your card (Impatient teams, owner-only data) ---------------------------------------
	if (bRevealed && Align == EKGAlignment::Impatient && View.Mates.Num() > 0)
	{
		const float Side = 330.0f * U;
		const float Base = CardC.Y + CardSize.Y * 0.5f;   // feet on the card's bottom line
		const FSlateFontInfo NameFont = FKGMenuStyle::Font("Black", 40.0f * U, 40);
		const FSlateFontInfo RoleFont = FKGMenuStyle::Font("Bold", 24.0f * U, 160);
		for (int32 Index = 0; Index < View.Mates.Num(); ++Index)
		{
			const FKGRevealMateView& Mate = View.Mates[Index];
			const float Dir = Index % 2 == 0 ? 1.0f : -1.0f;
			const int32 Rank = Index / 2;
			const float Scale = Rank == 0 ? 1.0f : 0.84f;
			const float X = W * 0.5f + Dir * (CardSize.X * 0.5f + 40.0f * U + Side * 0.5f + Rank * Side * 0.82f);
			const float In = Ramp(T, MatesAt + Index * 0.14f, 0.4f);
			if (In <= 0.0f)
			{
				continue;
			}
			const float Pop = FKGMenuStyle::EaseOutBack(In);
			const float S = Side * Scale * FMath::Lerp(0.6f, 1.0f, Pop);
			const FV Foot(X, Base + (1.0f - Pop) * 90.0f * U);
			const float MA = Alpha * Sat(In * 2.5f);
			{
				FKGVertexCanvas C(Out, Stage, MA);
				C.Ellipse(Foot - FV(0.0f, S * 0.45f), FV(S * 0.66f, S * 0.66f), A(Bright, 0.7f), A(AC, 0.0f), 40);
				C.Ellipse(Foot, FV(S * 0.42f, 18.0f * U), FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 24);
				C.Flush(Layer + 2);
			}
			const FLinearColor Tints[3] = {Hex(0xFF3B4F), Hex(0xE0283A), Hex(0xFF5A3C)};
			Bust(Out, Layer + 3, Stage, Index, FV(X - S * 0.5f, Foot.Y - S), S, A(Tints[Index % 3], MA));
			// Name over the head, Among Us style; the role under it, small.
			const float NameY = Foot.Y - S - 6.0f * U;
			const FString MateRole = KGRoleCard::ToDisplayUpper(KGRoleCard::Get(Mate.RoleId).Name, bTr);
			const FVector2f RoleSize = Display(Out, Layer + 5, Stage, MateRole, RoleFont, FV(X, NameY), A(Bright, MA), 2.0f * U, 0.0f, 0.5f, 1.0f);
			const FSlateFontInfo Fit = FitFont(Mate.Name, TEXT("Black"), 40.0f * U * Scale, Side * 1.25f, 40);
			Display(Out, Layer + 5, Stage, Mate.Name, Fit, FV(X, NameY - RoleSize.Y - 2.0f * U), A(Cream, MA), 3.0f * U, 3.0f * U, 0.5f, 1.0f);
			(void)NameFont;
		}
	}
	Layer += 12;

	// --- Burst ---------------------------------------------------------------------------------------------------------
	if (bRevealed)
	{
		FKGVertexCanvas C(Out, Stage, Alpha);
		Burst(C, Align, AC, CardC, Since, U);
		C.Flush(Layer);
		++Layer;
	}

	// --- Words ---------------------------------------------------------------------------------------------------------
	const float BannerY = H * 0.105f;
	if (!bRevealed)
	{
		// The tease: "WHO ARE YOU?" pulsing over the trembling card.
		const float TA = Ramp(T, 0.35f, 0.3f) * (0.75f + 0.25f * FMath::Sin(Anim * 9.0f));
		Display(Out, Layer, Stage, KGRoleCard::Tr(TEXT("WHO ARE YOU?"), TEXT("SEN KİMSİN?")), FKGMenuStyle::Font("Black", 60.0f * U, 200),
		        FV(W * 0.5f, BannerY), Fade(A(Cream, TA)), 3.0f * U, 4.0f * U);
	}
	else
	{
		// Banner: a band sweeping out from the middle, then the words.
		const float Sweep = EaseOut(Ramp(T, BannerAt, 0.28f));
		const float BandH = 92.0f * U;
		const float Half = W * 0.5f * Sweep;
		if (Sweep > 0.0f)
		{
			FKGVertexCanvas C(Out, Stage, Alpha);
			const float Slant = 26.0f * U * Sweep;
			const FLinearColor Band = Mix(AC, FLinearColor::White, 0.12f);
			C.Convex({FV(W * 0.5f - Half, BannerY - BandH * 0.5f), FV(W * 0.5f + Half + Slant, BannerY - BandH * 0.5f),
			          FV(W * 0.5f + Half, BannerY + BandH * 0.5f), FV(W * 0.5f - Half - Slant, BannerY + BandH * 0.5f)}, Band);
			C.RectH(FV(W * 0.5f - Half, BannerY + BandH * 0.5f), FV(Half * 2.0f, 6.0f * U), A(Mul(AC, 0.4f), 0.9f), A(Mul(AC, 0.4f), 0.9f));
			C.RectH(FV(W * 0.5f - Half, BannerY - BandH * 0.5f - 4.0f * U), FV(Half * 2.0f, 3.0f * U), A(FLinearColor::White, 0.7f),
			        A(FLinearColor::White, 0.7f));
			C.Flush(Layer);
		}
		const FString BannerText = KGRoleCard::AlignmentBanner(Align);
		const float WordsIn = Ramp(T, BannerAt + 0.1f, 0.2f);
		const FSlateFontInfo BannerFont = FitFont(BannerText, TEXT("Black"), 58.0f * U, W * 0.86f, 180);
		const float BS = FMath::Lerp(1.35f, 1.0f, EaseOut(WordsIn));
		Text(Out, Layer + 1, ScaledAbout(Stage, Size, FV(W * 0.5f, BannerY), BS), BannerText, BannerFont, FV(W * 0.5f, BannerY + 2.0f * U),
		     Fade(A(Ink, WordsIn)), 0.5f, 0.5f);

		// The role name: HUGE, slammed down from above the screen.
		const float NameY = CardC.Y + CardSize.Y * 0.5f + 104.0f * U;
		const float NP = Ramp(T, NameAt, 0.2f);
		if (NP > 0.0f)
		{
			const FString Name = KGRoleCard::ToDisplayUpper(Card.Name, bTr);
			const FSlateFontInfo NameFont = FitFont(Name, TEXT("Black"), 150.0f * U, W * 0.9f, 30);
			const float Land = T - (NameAt + 0.2f);
			const float NS = NP < 1.0f ? FMath::Lerp(2.8f, 1.0f, EaseIn(NP)) : 1.0f - 0.07f * Decay(Land, 0.0f, 0.07f) * FMath::Cos(Land * 40.0f);
			const FGeometry NameGeo = ScaledAbout(Stage, Size, FV(W * 0.5f, NameY), NS);
			Display(Out, Layer + 2, NameGeo, Name, NameFont, FV(W * 0.5f, NameY), Fade(A(Cream, Sat(NP * 3.0f))), 7.0f * U, 8.0f * U);
			// A shock ring where it lands.
			if (Land > 0.0f && Land < 0.5f)
			{
				FKGVertexCanvas C(Out, Stage, Alpha * (1.0f - Land / 0.5f));
				const float R = (200.0f + 900.0f * EaseOut(Land / 0.5f)) * U;
				C.Ellipse(FV(W * 0.5f, NameY), FV(R, R * 0.28f), A(Bright, 0.0f), A(Bright, 0.35f), 48);
				C.Flush(Layer + 1);
			}
		}

		// One line, plain words.
		const float LA = EaseOut(Ramp(T, LineAt, 0.3f));
		if (LA > 0.0f)
		{
			const FSlateFontInfo LineFont = FitFont(Card.Do, TEXT("Bold"), 48.0f * U, W * 0.86f, 10);
			Display(Out, Layer + 2, Stage, Card.Do, LineFont, FV(W * 0.5f, NameY + 134.0f * U + (1.0f - LA) * 24.0f * U),
			        Fade(A(FLinearColor::White, LA)), 4.0f * U, 4.0f * U);
		}
	}
	Layer += 5;

	// --- Flash (over everything but the prompts) --------------------------------------------------------------------------
	{
		const float Pre = T < BangAt ? Ramp(T, BangAt - 0.08f, 0.08f) : 0.0f;
		const float Flash = !View.RoleId.IsNone() ? FMath::Max(0.85f * Pre, 0.92f * Decay(T, BangAt, 0.11f)) : 0.0f;
		if (Flash > 0.004f)
		{
			FKGVertexCanvas C(Out, Geo, Alpha);
			const FLinearColor F = Mix(FLinearColor::White, Bright, 0.3f);
			C.RectV(FV(0.0f, 0.0f), Size, A(F, Flash), A(F, Flash));
			C.Flush(Layer);
		}
		++Layer;
	}

	// --- Prompts: ready + details, a thin bar for the time left --------------------------------------------------------
	const float PromptA = Alpha * Ramp(T, ReadyFromSeconds, 0.3f) * (1.0f - DetailsAlpha);
	if (PromptA > 0.004f)
	{
		const float Y = H - 62.0f * U;
		const FSlateFontInfo KeyFont = FKGMenuStyle::Font("Black", 22.0f * U, 60);
		const FSlateFontInfo LabelFont = FKGMenuStyle::Font("Black", 26.0f * U, 120);
		auto KeyCap = [&](const FString& Key, const FString& Label, float X0, const FLinearColor& Rim, const FLinearColor& Col, float LA)
		{
			const FVector2f KS = Measure(Key, KeyFont) + FVector2f(30.0f * U, 16.0f * U);
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(X0, Y - KS.Y * 0.5f), FV(KS), A(Style.Ink, 0.85f * LA), 8.0f * U, A(Rim, LA), 2.0f * U);
			Text(Out, Layer + 1, Geo, Key, KeyFont, FV(X0 + KS.X * 0.5f, Y), A(Rim, LA), 0.5f, 0.5f);
			const FVector2f LS = Display(Out, Layer + 1, Geo, Label, LabelFont, FV(X0 + KS.X + 12.0f * U, Y), A(Col, LA), 2.0f * U, 0.0f, 0.0f, 0.5f);
			return KS.X + 12.0f * U + LS.X;
		};
		auto Width = [&](const FString& Key, const FString& Label)
		{
			return Measure(Key, KeyFont).X + 30.0f * U + 12.0f * U + Measure(Label, LabelFont).X;
		};
		const FString DetKey = TEXT("TAB");
		const FString DetLabel = KGRoleCard::Tr(TEXT("DETAILS"), TEXT("DETAY"));
		const float Gap = 48.0f * U;
		if (!View.bReady)
		{
			const FString Key = KGRoleCard::Tr(TEXT("SPACE"), TEXT("BOŞLUK"));
			const FString Label = KGRoleCard::Tr(TEXT("READY"), TEXT("HAZIRIM"));
			const float Total = Width(Key, Label) + Gap + Width(DetKey, DetLabel);
			float X0 = W * 0.5f - Total * 0.5f;
			const float Pulse = 0.75f + 0.25f * FMath::Sin(Anim * 5.0f);
			X0 += KeyCap(Key, Label, X0, A(Style.Gold, Pulse), Cream, PromptA) + Gap;
			KeyCap(DetKey, DetLabel, X0, Cream, Mix(Cream, AC, 0.3f), PromptA * 0.8f);
		}
		else
		{
			const float Pop = FKGMenuStyle::EaseOutBack(Sat(ReadyAge / 0.3f));
			const FString Label = View.HumanCount > 1 ? FString::Printf(TEXT("%s  %d / %d"), KGRoleCard::Tr(TEXT("READY"), TEXT("HAZIR")),
			                                                            View.ReadyCount, View.HumanCount)
			                                          : FString(KGRoleCard::Tr(TEXT("READY"), TEXT("HAZIR")));
			const FVector2f LS = Measure(Label, LabelFont);
			const float DW = Width(DetKey, DetLabel);
			const FV Box(LS.X + 56.0f * U, 50.0f * U);
			const float Total = Box.X + Gap + DW;
			const float X0 = W * 0.5f - Total * 0.5f;
			FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FV(X0, Y - Box.Y * 0.5f * Pop), FV(Box.X, Box.Y * Pop), A(Style.Good, 0.9f * PromptA), -1.0f);
			Text(Out, Layer + 1, Geo, Label, LabelFont, FV(X0 + Box.X * 0.5f, Y), A(Ink, PromptA), 0.5f, 0.5f);
			KeyCap(DetKey, DetLabel, X0 + Box.X + Gap, Cream, Mix(Cream, AC, 0.3f), PromptA * 0.8f);
		}
		if (View.bStreamer)
		{
			Display(Out, Layer + 1, Geo, KGRoleCard::Tr(TEXT("STREAMER MODE  ·  ROLE HIDDEN AFTER THIS"), TEXT("YAYINCI MODU  ·  ROL BUNDAN SONRA GİZLİ")),
			        FKGMenuStyle::Font("Bold", 20.0f * U, 160), FV(W * 0.5f, Y - 52.0f * U), A(Style.Ghost, PromptA), 2.0f * U, 0.0f);
		}
		if (View.ServerRemaining >= 0.0f)
		{
			const float Left = Sat(View.ServerRemaining / PhaseSeconds);
			FKGVertexCanvas C(Out, Geo, PromptA);
			C.RectH(FV(0.0f, H - 6.0f * U), FV(W * Left, 6.0f * U), A(Bright, 0.9f), A(Bright, 0.9f));
			C.Flush(Layer);
		}
		Layer += 3;
	}

	// --- Details (hold Tab): goal, abilities, flavour ---------------------------------------------------------------------
	if (DetailsAlpha > 0.004f && bRevealed)
	{
		const float DA = Alpha * DetailsAlpha;
		const float PW = FMath::Min(1100.0f * U, W * 0.86f);
		const float Pad = 48.0f * U;
		const float InnerW = PW - Pad * 2.0f;
		const FSlateFontInfo HeadFont = FitFont(KGRoleCard::ToDisplayUpper(Card.Name, bTr), TEXT("Black"), 64.0f * U, InnerW, 20);
		const FSlateFontInfo CapFont = FKGMenuStyle::Font("Black", 22.0f * U, 260);
		const FSlateFontInfo GoalFont = FKGMenuStyle::Font("Bold", 34.0f * U);
		const FSlateFontInfo BodyFont = FKGMenuStyle::Font("Medium", 30.0f * U);
		const FSlateFontInfo FlavourFont = FKGMenuStyle::Font("Italic", 26.0f * U);
		const float CapH = Measure(TEXT("A"), CapFont).Y;
		const float GoalH = Measure(TEXT("Ag"), GoalFont).Y * 1.05f;
		const float BodyH = Measure(TEXT("Ag"), BodyFont).Y * 1.08f;
		const float FlavH = Measure(TEXT("Ag"), FlavourFont).Y * 1.05f;
		const TArray<FString> Goal = Wrap(Card.Goal, GoalFont, InnerW);
		TArray<TArray<FString>> Abilities;
		for (const FString& Line : Card.Abilities)
		{
			Abilities.Add(Wrap(Line, BodyFont, InnerW - 30.0f * U));
		}
		const TArray<FString> Flavour = Wrap(FString::Printf(TEXT("“%s”"), *Card.Flavour), FlavourFont, InnerW);
		float Need = Pad + Measure(TEXT("A"), HeadFont).Y + 8.0f * U + Measure(TEXT("A"), CapFont).Y + 30.0f * U;
		Need += CapH + 10.0f * U + Goal.Num() * GoalH + 26.0f * U + CapH + 10.0f * U;
		for (const TArray<FString>& Lines : Abilities)
		{
			Need += Lines.Num() * BodyH + 10.0f * U;
		}
		Need += 20.0f * U + Flavour.Num() * FlavH + (View.bStreamer ? 50.0f * U : 0.0f) + Pad;
		const float PH = FMath::Min(Need, H * 0.9f);
		const FV P0(W * 0.5f - PW * 0.5f, H * 0.5f - PH * 0.5f + (1.0f - DetailsAlpha) * 30.0f * U);
		{
			FKGVertexCanvas C(Out, Geo, DA);
			C.RectV(FV(0.0f, 0.0f), Size, FLinearColor(0.02f, 0.01f, 0.03f, 0.78f), FLinearColor(0.02f, 0.01f, 0.03f, 0.86f));
			C.Flush(Layer);
		}
		FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Geo, P0, FV(PW, PH), A(Style.Night, 0.97f * DA), 24.0f * U, A(AC, 0.9f * DA), 3.0f * U);
		const int32 TL = Layer + 2;
		float Y = P0.Y + Pad;
		const float X = P0.X + Pad;
		Y += Text(Out, TL, Geo, KGRoleCard::ToDisplayUpper(Card.Name, bTr), HeadFont, FV(X, Y), A(Cream, DA)).Y + 8.0f * U;
		Y += Text(Out, TL, Geo, KGRoleCard::ToDisplayUpper(Card.Team, bTr), CapFont, FV(X, Y), A(AC, DA)).Y + 30.0f * U;
		Text(Out, TL, Geo, KGRoleCard::Tr(TEXT("GOAL"), TEXT("HEDEF")), CapFont, FV(X, Y), A(Style.Gold, DA));
		Y += CapH + 10.0f * U;
		for (const FString& Line : Goal)
		{
			Text(Out, TL, Geo, Line, GoalFont, FV(X, Y), A(Cream, DA));
			Y += GoalH;
		}
		Y += 26.0f * U;
		Text(Out, TL, Geo, KGRoleCard::Tr(TEXT("ABILITIES"), TEXT("YETENEKLER")), CapFont, FV(X, Y), A(Style.Gold, DA));
		Y += CapH + 10.0f * U;
		for (const TArray<FString>& Lines : Abilities)
		{
			const float D = 7.0f * U;
			FKGMenuStyle::DrawRoundedBox(Out, TL, Geo, FV(X + 4.0f * U, Y + BodyH * 0.5f - D), FV(D * 2.0f, D * 2.0f), A(AC, DA), -1.0f);
			for (const FString& Line : Lines)
			{
				Text(Out, TL, Geo, Line, BodyFont, FV(X + 30.0f * U, Y), A(Style.CreamDim, DA));
				Y += BodyH;
			}
			Y += 10.0f * U;
		}
		Y += 10.0f * U;
		for (const FString& Line : Flavour)
		{
			Text(Out, TL, Geo, Line, FlavourFont, FV(X, Y), A(Style.Muted, DA));
			Y += FlavH;
		}
		if (View.bStreamer)
		{
			Y += 14.0f * U;
			Text(Out, TL, Geo, FString::Format(KGRoleCard::Tr(TEXT("Streamer mode: hold [{0}] to peek at your role later."),
			                                                  TEXT("Yayıncı modu: rolünü görmek için [{0}] basılı tut.")), {View.PeekKey}),
			     FKGMenuStyle::Font("Bold", 22.0f * U), FV(X, Y), A(Style.Ghost, DA));
		}
		Layer += 4;
	}
	(void)Args;
	return Layer;
}
