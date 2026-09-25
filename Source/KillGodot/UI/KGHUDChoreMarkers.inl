// SPRINT-025 (item 1): chore visibility for AKGHUD.
//  - In-world markers: a soft glow on the station / item plus a floating lantern-orange pin that scales with distance,
//    shows the metres, and clamps to the screen edge as an arrow when the step is off screen or behind you.
//  - The chore tracker (top left): every chore on one short line (name, next step, distance); the active one lit.
// Included at the end of UI/KGHUDMap.inl, inside KGHUD.cpp's anonymous namespace, after the world chore HUD (so it
// sees FKGPainter, FKGHudFrame, FKGChorePin, the palette, DrawActionPrompt and the world chore step lines).
// Pins come from GatherChores (one gather per frame, shared with the minimap and the full map).
// Social deduction: only your own chores. The Impatient's fake chores come through the same lists, so their markers
// and tracker rows are indistinguishable from a villager's.

/** Screen position of a world point; false when it is behind the camera (Out then holds the direction to it). */
bool ProjectMarker(const FKGHudFrame& F, const FVector& CamLoc, const FRotator& CamRot, float Focal, const FVector& World,
                   FVector2D& Out)
{
	const FVector Local = CamRot.UnrotateVector(World - CamLoc);   // X forward, Y right, Z up
	if (Local.X < 1.0)
	{
		FVector2D Dir(Local.Y, -Local.Z);
		if (Dir.IsNearlyZero())
		{
			Dir = FVector2D(0.0, 1.0);
		}
		Out = FVector2D(F.CX, F.CY) + Dir.GetSafeNormal() * 100000.0;
		return false;
	}
	Out = FVector2D(F.CX + Local.Y / Local.X * Focal, F.CY - Local.Z / Local.X * Focal);
	return true;
}

/** The marker glyph: a teardrop pin with a dot; Size = pin height. */
void MarkerPin(const FKGPainter& P, const FVector2D& Tip, float Size, const FLinearColor& Col, float Opacity)
{
	const float R = Size * 0.36f;
	const FVector2D C = Tip - FVector2D(0.0f, Size * 0.66f);
	const FVector2D L = C + FVector2D(-R * 0.94f, R * 0.34f);
	const FVector2D Rt = C + FVector2D(R * 0.94f, R * 0.34f);
	const FVector2D Tri[3] = {Tip + FVector2D(0.0f, 2.5f), L + FVector2D(-2.0f, 0.0f), Rt + FVector2D(2.0f, 0.0f)};
	P.Poly(MakeArrayView(Tri), WithAlpha(InkBottom, 0.85f * Opacity), WithAlpha(InkBottom, 0.85f * Opacity));
	P.Circle(C, R + 2.0f, WithAlpha(InkBottom, 0.85f * Opacity));
	const FVector2D Tri2[3] = {Tip, L, Rt};
	P.Poly(MakeArrayView(Tri2), WithAlpha(Col, Opacity), WithAlpha(Shade(Col, 0.8f), Opacity));
	P.Circle(C, R, WithAlpha(Col, Opacity));
	P.Circle(C, R * 0.42f, WithAlpha(InkBottom, 0.9f * Opacity));
}

/** Off-screen chevron pointing along Dir. */
void MarkerArrow(const FKGPainter& P, const FVector2D& At, const FVector2D& Dir, float Size, const FLinearColor& Col, float Opacity)
{
	const FVector2D N(-Dir.Y, Dir.X);
	const FVector2D Tip = At + Dir * Size * 0.5f;
	const FVector2D Base = At - Dir * Size * 0.35f;
	const FVector2D Tri[3] = {Tip, Base + N * Size * 0.5f, Base - N * Size * 0.5f};
	const FVector2D TriO[3] = {Tip + Dir * 3.0f, Base + N * (Size * 0.5f + 3.0f) - Dir * 2.0f, Base - N * (Size * 0.5f + 3.0f) - Dir * 2.0f};
	P.Poly(MakeArrayView(TriO), WithAlpha(InkBottom, 0.9f * Opacity), WithAlpha(InkBottom, 0.9f * Opacity));
	P.Poly(MakeArrayView(Tri), WithAlpha(Col, Opacity), WithAlpha(Shade(Col, 0.8f), Opacity));
}

void DrawChoreMarkers(const FKGHudFrame& F, const TArray<FKGChorePin>& Pins, float Opacity)
{
	if (!F.Pawn || !F.PC || Opacity <= 0.01f || Pins.Num() == 0)
	{
		return;
	}
	// Not while a panel is open (it blurs the world) and never at a meeting / trial / the epilogue.
	const UKGChoreComponent* Chores = UKGChoreComponent::FindFor(F.Pawn);
	if (Chores && Chores->HasSession())
	{
		return;
	}
	if (F.GS && (F.GS->GetPhase() == EKGPhase::Meeting || F.GS->GetPhase() == EKGPhase::Trial || F.GS->GetPhase() == EKGPhase::Epilogue))
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	FVector CamLoc;
	FRotator CamRot;
	F.PC->GetPlayerViewPoint(CamLoc, CamRot);
	const float FOV = F.PC->PlayerCameraManager ? F.PC->PlayerCameraManager->GetFOVAngle() : 90.0f;
	const float Focal = (F.W * 0.5f) / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV, 30.0f, 150.0f)) * 0.5f);
	// Clamp rectangle: inside the screen, clear of the top cards and the vitals.
	const FBox2D Bounds(FVector2D(KGUI::MarkerEdgeInset * S, 150.0f * S), FVector2D(F.W - KGUI::MarkerEdgeInset * S, F.H - 150.0f * S));
	const FVector2D Centre(F.CX, F.CY);
	const double Now = F.Fx->Now;

	// Nearest open step is "the" chore when nothing is being worked.
	int32 Nearest = INDEX_NONE;
	bool bAnyActive = false;
	for (int32 i = 0; i < Pins.Num(); ++i)
	{
		const FKGChorePin& Pin = Pins[i];
		if (Pin.bDone || Pin.bRowOnly)
		{
			continue;
		}
		bAnyActive |= Pin.bActive;
		if (Nearest == INDEX_NONE || Pin.Metres < Pins[Nearest].Metres)
		{
			Nearest = i;
		}
	}

	for (int32 i = 0; i < Pins.Num(); ++i)
	{
		const FKGChorePin& Pin = Pins[i];
		if (Pin.bDone || Pin.bRowOnly)
		{
			continue;
		}
		const bool bLit = Pin.bActive || (!bAnyActive && i == Nearest);
		const FLinearColor Col = Pin.bItem ? WaterColor : bLit ? Lantern : WithAlpha(Cream, 0.9f);
		// Distance scale: full size up close, MarkerScaleMin far away.
		const float T = Saturate((Pin.Metres - KGUI::MarkerNearMetres) / (KGUI::MarkerFarMetres - KGUI::MarkerNearMetres));
		const float Scale = FMath::Lerp(1.0f, KGUI::MarkerScaleMin, T);
		// Fade right on top of it: the E prompt takes over there.
		const float A = Opacity * FMath::Lerp(0.35f, 1.0f, Saturate((Pin.Metres - 1.2f) / 1.6f)) * (bLit ? 1.0f : 0.8f);

		FVector2D Sc;
		const bool bFront = ProjectMarker(F, CamLoc, CamRot, Focal, Pin.Loc + FVector(0.0, 0.0, 110.0), Sc);
		const bool bInside = bFront && Bounds.IsInside(Sc);
		if (bInside)
		{
			// Soft glow on the station / item itself (the outline substitute: no post-process, no assets).
			FVector2D Foot;
			ProjectMarker(F, CamLoc, CamRot, Focal, Pin.Loc + FVector(0.0, 0.0, 40.0), Foot);
			const float GlowR = FMath::Clamp(9000.0f / FMath::Max(Pin.Metres * 100.0f, 250.0f), 26.0f, 120.0f) * S;
			const float Pulse = bLit ? 0.85f + 0.15f * static_cast<float>(FMath::Sin(Now * 3.0)) : 0.7f;
			P.Circle(Foot, GlowR, WithAlpha(Col, 0.05f * A * Pulse), GlowR * 0.9f);
			P.Circle(Foot, GlowR * 0.55f, WithAlpha(Col, 0.08f * A * Pulse), GlowR * 0.5f);

			const float Size = 40.0f * S * Scale;
			const float Bob = bLit ? 3.0f * S * static_cast<float>(FMath::Sin(Now * 2.4)) : 0.0f;
			const FVector2D Tip = Sc + FVector2D(0.0f, Bob);
			MarkerPin(P, Tip, Size, Col, A);
			// Metres under the pin; the step name above the lit one.
			const FString Dist = FString::Printf(TEXT("%d m"), FMath::RoundToInt(Pin.Metres));
			const float Px = FMath::Lerp(KGUI::TypeCaption, KGUI::TypeMicro, T);
			const float DW = static_cast<float>(P.Measure(Dist, Px, true, 1.0f).X);
			const float DH = 20.0f * S * FMath::Lerp(1.0f, 0.85f, T);
			P.RoundRect(Tip.X - DW * 0.5f - 8.0f * S, Tip.Y + 6.0f * S, DW + 16.0f * S, DH, DH * 0.5f, WithAlpha(InkBottom, 0.7f * A));
			P.TextMid(Dist, Tip.X, Tip.Y + 6.0f * S + DH * 0.5f, Px, WithAlpha(Cream, A), 0.5f, true, 1.0f);
			if (bLit && Pin.Metres > 2.0f)
			{
				const FString Line = Pin.bItem ? FString::Printf(TEXT("Your %s"), *Pin.Name) : Pin.Step;
				const float LW = static_cast<float>(P.Measure(Line, KGUI::TypeBody, true).X);
				const float LH = 26.0f * S;
				const float LY = Tip.Y - Size - 8.0f * S - LH;
				P.RoundRect(Tip.X - LW * 0.5f - 12.0f * S, LY, LW + 24.0f * S, LH, LH * 0.5f, WithAlpha(InkBottom, 0.72f * A));
				P.TextMid(Line, Tip.X, LY + LH * 0.5f, KGUI::TypeBody, WithAlpha(Col, A), 0.5f, true);
			}
		}
		else
		{
			// Clamp along the ray from the centre to the bounds; an arrow shows the way, the distance beside it.
			FVector2D Dir = (Sc - Centre).GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector2D(0.0, 1.0);
			}
			double Tmax = 1e9;
			if (Dir.X > 1e-6)
			{
				Tmax = FMath::Min(Tmax, (Bounds.Max.X - Centre.X) / Dir.X);
			}
			else if (Dir.X < -1e-6)
			{
				Tmax = FMath::Min(Tmax, (Bounds.Min.X - Centre.X) / Dir.X);
			}
			if (Dir.Y > 1e-6)
			{
				Tmax = FMath::Min(Tmax, (Bounds.Max.Y - Centre.Y) / Dir.Y);
			}
			else if (Dir.Y < -1e-6)
			{
				Tmax = FMath::Min(Tmax, (Bounds.Min.Y - Centre.Y) / Dir.Y);
			}
			const FVector2D At = Centre + Dir * Tmax;
			const float Size = 26.0f * S * FMath::Lerp(1.0f, 0.8f, T);
			MarkerArrow(P, At, Dir, Size, Col, A);
			const FString Dist = FString::Printf(TEXT("%d m"), FMath::RoundToInt(Pin.Metres));
			const float DW = static_cast<float>(P.Measure(Dist, KGUI::TypeCaption, true, 1.0f).X);
			const float DH = 20.0f * S;
			const FVector2D LabelC = At - Dir * (Size * 0.9f + 14.0f * S + DW * 0.5f * FMath::Abs(Dir.X));
			P.RoundRect(LabelC.X - DW * 0.5f - 8.0f * S, LabelC.Y - DH * 0.5f, DW + 16.0f * S, DH, DH * 0.5f, WithAlpha(InkBottom, 0.7f * A));
			P.TextMid(Dist, LabelC.X, LabelC.Y, KGUI::TypeCaption, WithAlpha(Cream, A), 0.5f, true, 1.0f);
			if (bLit)
			{
				const FString Line = Pin.bItem ? FString::Printf(TEXT("Your %s"), *Pin.Name) : Pin.Name;
				const float LW = static_cast<float>(P.Measure(Line, KGUI::TypeCaption, true).X);
				const FVector2D NameC = LabelC - Dir * (DH + 8.0f * S) * (FMath::Abs(Dir.Y) > 0.5f ? 1.0f : 0.0f) -
				                        FVector2D(0.0f, FMath::Abs(Dir.Y) > 0.5f ? 0.0f : DH + 4.0f * S);
				P.RoundRect(NameC.X - LW * 0.5f - 8.0f * S, NameC.Y - DH * 0.5f, LW + 16.0f * S, DH, DH * 0.5f, WithAlpha(InkBottom, 0.7f * A));
				P.TextMid(Line, NameC.X, NameC.Y, KGUI::TypeCaption, WithAlpha(Col, A), 0.5f, true);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------
// Chore tracker (top left): "CHORES 1 / 4", then one line per chore: [state] Name · next step ........ 12 m
// -------------------------------------------------------------------------------------------------------------------
float DrawChoreTracker(const FKGHudFrame& F)
{
	FKGHudFx& Fx = *F.Fx;
	const FKGPainter& P = F.P;
	const float S = F.S;
	if (!F.GS)
	{
		return 0.0f;
	}
	FKGMapFx& M = MapFx(F.Hud);
	TArray<FKGChorePin> Pins;
	GatherChores(F, M, Pins);

	// One row per chore: panel chores and world chore rows (their step pins are skipped, the row carries the step).
	struct FRow
	{
		FString Name;
		FString Step;
		float Metres = 0.0f;
		bool bDone = false;
		bool bActive = false;
		bool bHasPlace = false;
	};
	TArray<FRow> Rows;
	TSet<FName> Seen;
	for (const FKGChorePin& Pin : Pins)
	{
		if (Pin.bItem || (Seen.Contains(Pin.Chore) && !Pin.Chore.IsNone()))
		{
			continue;
		}
		Seen.Add(Pin.Chore);
		FRow& Row = Rows.AddDefaulted_GetRef();
		Row.Name = Pin.Name;
		Row.Step = Pin.Step;
		Row.Metres = Pin.Metres;
		Row.bDone = Pin.bDone;
		Row.bActive = Pin.bActive;
		Row.bHasPlace = !Pin.Loc.IsNearlyZero();
	}
	// World chore rows: the step pin of that chore carries "active" (the one you touched last).
	for (const FKGChorePin& Pin : Pins)
	{
		if (Pin.bActive)
		{
			for (FRow& Row : Rows)
			{
				if (Row.Name == Pin.Name && !Row.bDone)
				{
					Row.bActive = true;
				}
			}
		}
	}
	if (Rows.Num() == 0)
	{
		Fx.LastDone = -1;
		Fx.ChoreDoneAt.Reset();
		return 0.0f;
	}
	// Nothing being worked: the nearest open chore is the one to go for.
	bool bAnyActive = false;
	for (const FRow& Row : Rows)
	{
		bAnyActive |= Row.bActive && !Row.bDone;
	}
	if (!bAnyActive)
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			if (!Rows[i].bDone && Rows[i].bHasPlace && (Best == INDEX_NONE || Rows[i].Metres < Rows[Best].Metres))
			{
				Best = i;
			}
		}
		if (Best != INDEX_NONE)
		{
			Rows[Best].bActive = true;
		}
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

	const bool bPrep = F.GS->GetPhase() != EKGPhase::Lobby && ((F.Me && F.Me->TaskIds.Num() > 0) || F.Demo != 0);
	const FBox2D Box = FKGHudLayout::Compute(F.W, F.H, Rows.Num(), bPrep).Tracker;
	const float X = static_cast<float>(Box.Min.X);
	const float Y = static_cast<float>(Box.Min.Y);
	const float W = static_cast<float>(Box.GetSize().X);
	const float H = static_cast<float>(Box.GetSize().Y);
	const float RowH = KGUI::RowH * S;
	const float HeadH = 58.0f * S;
	const float Pad = KGUI::PadCard * S;
	P.Card(X, Y, W, H);
	P.TopBand(X, Y, W, KGUI::RadiusCard * S, 4.0f * S, WithAlpha(Lantern, 0.9f));
	P.TextMid(TEXT("CHORES"), X + Pad, Y + 24.0f * S, KGUI::TypeCaption, Lantern, 0.0f, true, KGUI::CaptionTracking);
	P.TextMid(FString::Printf(TEXT("%d / %d"), Done, Rows.Num()), X + W - Pad, Y + 24.0f * S, KGUI::TypeCaption, Cream, 1.0f, true, 1.0f);
	P.PillBar(X + Pad, Y + 42.0f * S, W - 2.0f * Pad, 6.0f * S, static_cast<float>(Done) / Rows.Num(), TownLight, TownColor);

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
		const float Top = Y + HeadH + RowH * i;
		const float MidY = Top + RowH * 0.5f;
		if (Row.bActive && !Row.bDone)
		{
			// Lit row: a soft lantern band with a bright left edge.
			P.RoundRect(X + 8.0f * S, Top + 2.0f * S, W - 16.0f * S, RowH - 4.0f * S, 8.0f * S, WithAlpha(Lantern, 0.14f));
			P.RoundRect(X + 8.0f * S, Top + 6.0f * S, 3.0f * S, RowH - 12.0f * S, 1.5f * S, Lantern);
		}
		const float Bx = 20.0f * S;
		const FVector2D BoxC(X + Pad + Bx * 0.5f, MidY);
		if (Row.bDone)
		{
			const float Pop = Fx.ChoreDoneAt[i] < 0.0 ? 1.0f : EaseOutBack(static_cast<float>(Fx.Now - Fx.ChoreDoneAt[i]) / 0.35f);
			const float B = Bx * FMath::Max(0.05f, Pop);
			P.RoundRect(BoxC.X - B * 0.5f, BoxC.Y - B * 0.5f, B, B, 6.0f * S * Pop, TownLight, TownColor);
			P.Check(BoxC, B, FMath::Max(1.5f, 3.0f * S * Pop), InkText);
		}
		else if (Row.bActive)
		{
			MarkerPin(P, BoxC + FVector2D(0.0f, Bx * 0.55f), Bx * 1.15f, Lantern, 1.0f);
		}
		else
		{
			P.Outline(BoxC.X - Bx * 0.5f, BoxC.Y - Bx * 0.5f, Bx, Bx, 6.0f * S, FMath::Max(1.5f, 2.0f * S), WithAlpha(Cream, 0.55f));
		}
		// Distance on the right, then name + step on one line, the step trimmed to the room left.
		float Right = X + W - Pad;
		if (!Row.bDone && Row.bHasPlace)
		{
			const FString Dist = FString::Printf(TEXT("%d m"), FMath::RoundToInt(Row.Metres));
			const FVector2D DS = P.TextMid(Dist, Right, MidY, KGUI::TypeCaption, Row.bActive ? Lantern : CreamDim, 1.0f, true, 1.0f);
			Right -= static_cast<float>(DS.X) + 10.0f * S;
		}
		const float TextX = X + Pad + Bx + 12.0f * S;
		const FLinearColor NameCol = Row.bDone ? WithAlpha(Cream, 0.42f) : Cream;
		const FVector2D NS = P.TextMid(Row.Name, TextX, MidY, KGUI::TypeBody + 1.0f, NameCol, 0.0f, !Row.bDone);
		if (Row.bDone)
		{
			P.Rect(TextX - 2.0f * S, FMath::RoundToFloat(MidY), static_cast<float>(NS.X) + 4.0f * S, FMath::Max(1.0f, 2.0f * S), WithAlpha(Cream, 0.45f));
		}
		else if (!Row.Step.IsEmpty())
		{
			const float StepX = TextX + static_cast<float>(NS.X) + 8.0f * S;
			const float Room = Right - StepX;
			if (Room > 30.0f * S)
			{
				FString Step = TEXT("·  ") + Row.Step;
				while (Step.Len() > 5 && P.Measure(Step, KGUI::TypeBody - 1.0f, false).X > Room)
				{
					Step.LeftInline(Step.Len() - 2);
					Step += TEXT("...");
				}
				P.TextMid(Step, StepX, MidY + 0.5f * S, KGUI::TypeBody - 1.0f, Row.bActive ? Lantern : CreamDim, 0.0f, false);
			}
		}
	}
	return Y + H;
}
