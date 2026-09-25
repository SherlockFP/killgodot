// Fishing HUD for AKGHUD (Docs/08_UI_UX.md §6.3): charge meter, bite cue, reel minigame (tension bar with slack / good /
// snap zones, fish stamina, which way it pulls), the catch card (species, weight, rarity, personal best), toasts, the
// tunnel-vision vignette while reeling and the kg.Fish.Tension debug overlay.
// Included by KGHUD.cpp inside its anonymous namespace (reuses FKGPainter, FKGHudFrame and the palette). All state lives
// in UKGFishingComponent (owner side), so KGHUD.h keeps its layout.

const FLinearColor FishWater = Srgb(92, 190, 230);
const FLinearColor FishGood = Srgb(120, 214, 132);
const FLinearColor FishDanger = Srgb(255, 84, 72);

FString FishKg(int32 Grams)
{
	return FString::Printf(TEXT("%.2f kg"), Grams / 1000.0f);
}

/** Greedy word wrap to Width screen px. */
void FishWrap(const FKGPainter& P, const FString& Text, float Px, float Width, TArray<FString>& Out)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	FString Line;
	for (const FString& W : Words)
	{
		const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
		if (!Line.IsEmpty() && P.Measure(Try, Px, false).X > Width)
		{
			Out.Add(Line);
			Line = W;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		Out.Add(Line);
	}
}

struct FFishHint
{
	const TCHAR* Key;
	const TCHAR* Label;
};

/** Row of key hints centred on CX (e.g. [LMB] Reel   [A/D] Steer). */
void FishHints(const FKGHudFrame& F, float MidY, std::initializer_list<FFishHint> Hints, float Opacity)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float Px = 17.0f;
	const float Gap = 26.0f * S;
	float Total = 0.0f;
	for (const FFishHint& H : Hints)
	{
		Total += P.KeyHintWidth(H.Key, H.Label, Px);
	}
	Total += Gap * FMath::Max(0, static_cast<int32>(Hints.size()) - 1);
	DrawLabelChip(F, MidY, Total + 34.0f * S, 40.0f * S, Opacity);
	float X = F.CX - Total * 0.5f;
	for (const FFishHint& H : Hints)
	{
		P.KeyHint(H.Key, H.Label, X, MidY, Px, Cream, 0.0f, Opacity);
		X += P.KeyHintWidth(H.Key, H.Label, Px) + Gap;
	}
}

/** Under everything: darker edges while the fight has your attention (killers love an angler). */
void DrawFishingVignette(const FKGHudFrame& F)
{
	const UKGFishingComponent* Fishing = F.Pawn ? UKGFishingComponent::FindFor(F.Pawn) : nullptr;
	const float A = Fishing ? Fishing->GetFocusAlpha() : 0.0f;
	if (A < 0.02f)
	{
		return;
	}
	const FKGPainter& P = F.P;
	constexpr int32 Steps = 14;
	const float BandX = F.W * 0.2f;
	const float BandY = F.H * 0.16f;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float T = 1.0f - static_cast<float>(i) / Steps;
		const FLinearColor C(0.0f, 0.02f, 0.05f, 0.075f * A * T * T);
		const float X = BandX * i / Steps;
		const float Y = BandY * i / Steps;
		P.Rect(X, 0.0f, BandX / Steps + 1.0f, F.H, C);
		P.Rect(F.W - X - BandX / Steps, 0.0f, BandX / Steps + 1.0f, F.H, C);
		P.Rect(0.0f, Y, F.W, BandY / Steps + 1.0f, C);
		P.Rect(0.0f, F.H - Y - BandY / Steps, F.W, BandY / Steps + 1.0f, C);
	}
}

void DrawFishCharge(const FKGHudFrame& F, float Power)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float W = 14.0f * S;
	const float H = 120.0f * S;
	const float X = F.CX + 70.0f * S;
	const float Y = F.CY - H * 0.5f;
	P.Shadow(X - 4.0f * S, Y - 4.0f * S, W + 8.0f * S, H + 8.0f * S, W, 8.0f * S, 0.35f);
	P.RoundRect(X, Y, W, H, W * 0.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	const float Fill = H * Saturate(Power);
	const FLinearColor Top = Power > 0.85f ? GoldLight : FishWater;
	P.RoundRect(X, Y + H - Fill, W, Fill, W * 0.5f, Top, Shade(Top, 0.7f));
	// sweet spot tick at 85%+
	P.Rect(X - 5.0f * S, Y + H * 0.15f, W + 10.0f * S, FMath::Max(1.0f, 2.0f * S), WithAlpha(Gold, 0.9f));
	P.TextMid(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Power * 100.0f)), X + W + 10.0f * S, Y + H - Fill, 15.0f,
	          Power > 0.85f ? GoldLight : Cream, 0.0f, true);
	P.TextMid(TEXT("CAST"), X + W * 0.5f, Y - 16.0f * S, 13.0f, WithAlpha(Cream, 0.8f), 0.5f, true, 2.0f);
}

void DrawFishBite(const FKGHudFrame& F, double Age)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float Pop = EaseOutBack(static_cast<float>(Age) / 0.25f);
	const float Pulse = 0.5f + 0.5f * static_cast<float>(FMath::Sin(Age * 18.0));
	const FVector2D C(F.CX, F.CY - 78.0f * S);
	P.Circle(C, (34.0f + 6.0f * Pulse) * S * Pop, WithAlpha(Gold, 0.22f));
	P.Circle(C, 27.0f * S * Pop, Gold);
	P.TextMid(TEXT("!"), C.X, C.Y, 40.0f * Pop, InkText, 0.5f, true, 0.0f, 0.0f);
	P.TextMid(TEXT("STRIKE!"), F.CX, C.Y + 46.0f * S, 18.0f, GoldLight, 0.5f, true, 3.0f);
}

void DrawFishFight(const FKGHudFrame& F, const UKGFishingComponent& Fishing)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const bool bSim = Fishing.HasShownSim();
	const FKGReelSim& Sim = Fishing.GetShownSim();
	const float Tension = bSim ? Sim.Tension : 0.3f;
	const float W = 520.0f * S;
	const float H = 132.0f * S;
	const float X = F.CX - W * 0.5f;
	const float Y = F.H - 312.0f * S;
	const float Danger = bSim ? Saturate(Sim.OverTime / FKGReelSim::SnapSeconds) : 0.0f;
	const float Slack = bSim ? Saturate(Sim.SlackTime / FKGReelSim::SlackSeconds) : 0.0f;
	const float Shake = Danger * 3.0f * S * static_cast<float>(FMath::Sin(F.Fx->Now * 60.0));
	P.Card(X + Shake, Y, W, H, 0.95f);
	if (Danger > 0.01f)
	{
		P.Glow(X + Shake, Y, W, H, 16.0f * S, 14.0f * S, WithAlpha(FishDanger, 0.55f * Danger));
	}
	else if (Slack > 0.01f)
	{
		P.Glow(X, Y, W, H, 16.0f * S, 14.0f * S, WithAlpha(FishWater, 0.5f * Slack));
	}

	// Header: state + distance
	const FString Head = !bSim ? TEXT("HOOKED!") : Danger > 0.05f ? TEXT("LINE STRAINING - LET GO!")
	                   : Slack > 0.05f ? TEXT("SLACK LINE - REEL!") : Sim.bJunk ? TEXT("SOMETHING HEAVY...")
	                   : TEXT("FISH ON!");
	const FLinearColor HeadCol = Danger > 0.05f ? FishDanger : Slack > 0.05f ? FishWater : GoldLight;
	P.Text(Head, X + 22.0f * S + Shake, Y + 14.0f * S, 18.0f, HeadCol, 0.0f, true, 2.0f);
	if (bSim)
	{
		P.Text(FString::Printf(TEXT("%.1f m"), Sim.Distance), X + W - 22.0f * S, Y + 14.0f * S, 18.0f, Cream, 1.0f, true);
	}

	// Tension bar: 0 .. 1.3 with slack / good / snap zones.
	const float BX = X + 22.0f * S;
	const float BW = W - 44.0f * S;
	const float BY = Y + 50.0f * S;
	const float BH = 22.0f * S;
	constexpr float Span = 1.3f;
	auto At = [BX, BW](float T) { return BX + BW * Saturate(T / Span); };
	P.RoundRect(BX, BY, BW, BH, BH * 0.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	P.PillSpan(BX, BY, BW, BH, 0.0f, FKGReelSim::SlackTension / Span, WithAlpha(FishWater, 0.35f), WithAlpha(FishWater, 0.25f));
	P.PillSpan(BX, BY, BW, BH, FKGReelSim::GoodMin / Span, 0.85f / Span, WithAlpha(FishGood, 0.30f), WithAlpha(FishGood, 0.2f));
	P.PillSpan(BX, BY, BW, BH, 1.0f / Span, 1.0f, WithAlpha(FishDanger, 0.45f), WithAlpha(FishDanger, 0.3f));
	const FLinearColor FillCol = Tension >= 1.0f ? FishDanger : Tension < FKGReelSim::SlackTension ? FishWater
	                           : Tension < FKGReelSim::GoodMin ? Cream : Tension < 0.85f ? FishGood : Gold;
	P.PillSpan(BX, BY + BH * 0.28f, BW, BH * 0.44f, 0.0f, Saturate(Tension / Span), FillCol, Shade(FillCol, 0.75f));
	const float MX = At(Tension);
	P.RoundRect(MX - 3.0f * S, BY - 6.0f * S, 6.0f * S, BH + 12.0f * S, 3.0f * S, FLinearColor::White);
	P.Text(TEXT("TENSION"), BX, BY + BH + 6.0f * S, 12.0f, WithAlpha(Cream, 0.7f), 0.0f, true, 2.0f);

	// Fish stamina + which way it pulls.
	const float SY = BY + BH + 30.0f * S;
	const float SW = BW * 0.45f;
	P.Text(TEXT("FISH"), BX, SY - 2.0f * S, 12.0f, WithAlpha(Cream, 0.7f), 0.0f, true, 2.0f);
	P.PillBar(BX + 46.0f * S, SY, SW, 12.0f * S, bSim ? Saturate(Sim.Stamina) : 1.0f, DawnColor, Shade(DawnColor, 0.7f));
	if (bSim && !Sim.bJunk)
	{
		const bool bLeft = Sim.PullDir < 0;
		const float AX = X + W - 150.0f * S;
		const float Wob = 4.0f * S * static_cast<float>(FMath::Sin(F.Fx->Now * 12.0));
		const FVector2D Tip(AX + (bLeft ? -30.0f : 30.0f) * S + (bLeft ? -Wob : Wob), SY + 6.0f * S);
		const FVector2D Arrow[3] = {Tip, Tip + FVector2D(bLeft ? 16.0f : -16.0f, -10.0f) * S,
		                            Tip + FVector2D(bLeft ? 16.0f : -16.0f, 10.0f) * S};
		P.Poly(MakeArrayView(Arrow), DawnColor, DawnColor);
		P.TextMid(bLeft ? TEXT("pulls left") : TEXT("pulls right"), AX + (bLeft ? 10.0f : -10.0f) * S, SY + 6.0f * S, 14.0f,
		          Cream, bLeft ? 0.0f : 1.0f, true);
		P.Keycap(bLeft ? TEXT("D") : TEXT("A"), X + W - 52.0f * S, SY - 5.0f * S, 13.0f);
	}
}

void DrawFishCatchCard(const FKGHudFrame& F, const UKGFishingComponent& Fishing)
{
	const double Shown = Fishing.GetCatchShownAt();
	const float Age = static_cast<float>(F.Fx->Now - Shown);
	const FKGFishCatch& Catch = Fishing.GetLastCatch();
	if (Shown < 0.0 || Age > 5.5f || Catch.Result == EKGFishPhase::Snagged)
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float In = EaseOutBack(Age / 0.35f);
	const float Out = Saturate((5.5f - Age) / 0.5f);
	const float A = Saturate(Age / 0.12f) * Out;
	const float W = 400.0f * S;
	const float X = F.W - (W + 36.0f * S) * In;
	const float Y = F.CY - 150.0f * S;
	const FKGItemDef* Def = UKGItemCatalog::Find(Catch.ItemId);
	const FKGFishSpecies* Species = FKGFishingRules::FindByItem(Catch.ItemId);

	FString Title;
	FString Name;
	FLinearColor Band = Gold;
	TArray<FString> Lines;
	TArray<FLinearColor> Colours;
	auto Add = [&Lines, &Colours](const FString& L, const FLinearColor& C) { Lines.Add(L); Colours.Add(C); };
	switch (Catch.Result)
	{
	case EKGFishPhase::Landed:
		Title = TEXT("CAUGHT!");
		Name = Def ? Def->DisplayName.ToString() : Catch.ItemId.ToString();
		Band = Def ? UKGItemCatalog::GetRarityColor(Def->Rarity) : Gold;
		if (Species && Catch.Grams > 0)
		{
			Add(FString::Printf(TEXT("%s   -   %s"), *FishKg(Catch.Grams), *UKGItemCatalog::GetRarityName(Def ? Def->Rarity : EKGRarity::Common).ToString()), Band);
			if (Fishing.IsNewBest() && Fishing.GetPreviousBest() > 0)
			{
				Add(FString::Printf(TEXT("NEW PERSONAL BEST!  (was %s)"), *FishKg(Fishing.GetPreviousBest())), GoldLight);
			}
			else if (Fishing.GetPreviousBest() == 0)
			{
				Add(FString::Printf(TEXT("Your first %s!"), *Name), GoldLight);
			}
			else
			{
				Add(FString::Printf(TEXT("Personal best: %s"), *FishKg(Fishing.GetPreviousBest())), WithAlpha(Cream, 0.8f));
			}
			Add(FString::Printf(TEXT("Madam Brine pays ~%d gold"), FKGFishingRules::SellPrice(Catch.ItemId, 1, Catch.Grams)),
			    WithAlpha(Cream, 0.75f));
		}
		else if (Catch.ItemId == FKGItemIds::CoinPouch)
		{
			Add(FString::Printf(TEXT("+%d gold coins, still wet"), Catch.Coins), GoldLight);
		}
		else if (Catch.ItemId == FKGItemIds::MessageBottle && Catch.Message >= 0)
		{
			FishWrap(P, FKGFishingRules::BottleMessage(Catch.Message).ToString(), 16.0f, W - 44.0f * S, Lines);
			Colours.SetNum(Lines.Num());
			for (FLinearColor& C : Colours)
			{
				C = Cream;
			}
		}
		else if (Def)
		{
			FishWrap(P, Def->Description.ToString(), 16.0f, W - 44.0f * S, Lines);
			Colours.Init(WithAlpha(Cream, 0.85f), Lines.Num());
		}
		if (Catch.bDropped)
		{
			Add(TEXT("Pockets full - it flops at your feet"), FishDanger);
		}
		break;
	case EKGFishPhase::Snapped:
		Title = TEXT("LINE SNAPPED");
		Band = FishDanger;
		Name = TEXT("Too much tension");
		if (Species && Catch.Grams > 0)
		{
			Add(FString::Printf(TEXT("It felt like a %s %s..."), *FishKg(Catch.Grams), *UKGItemCatalog::GetDisplayName(Catch.ItemId).ToString()), Cream);
		}
		Add(TEXT("Let go (release) when the bar turns red"), WithAlpha(Cream, 0.75f));
		break;
	case EKGFishPhase::Escaped:
		Title = TEXT("IT GOT AWAY");
		Band = FishWater;
		Name = TEXT("The one that got away");
		Add(TEXT("Keep the line tight: reel and steer against the pull"), WithAlpha(Cream, 0.8f));
		break;
	case EKGFishPhase::KoiTax:
		Title = TEXT("SACRILEGE!");
		Band = Srgb(240, 80, 154);
		Name = TEXT("The koi are sacred");
		Add(Catch.Coins > 0 ? FString::Printf(TEXT("A koi slapped you. The garden keeps %d gold."), Catch.Coins)
		                    : FString(TEXT("A koi slapped you. Luckily your purse was empty.")), Cream);
		break;
	default:
		return;
	}
	const float LineH = 24.0f * S;
	const float H = 96.0f * S + LineH * Lines.Num();
	P.Card(X, Y, W, H, A, 16.0f * S, true);
	P.TopBand(X, Y, W, 16.0f * S, 6.0f * S, WithAlpha(Band, A));
	P.Text(Title, X + 22.0f * S, Y + 16.0f * S, 14.0f, WithAlpha(Band, A), 0.0f, true, 3.0f);
	P.Text(Name, X + 22.0f * S, Y + 38.0f * S, 30.0f, WithAlpha(Cream, A), 0.0f, true);
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		P.Text(Lines[i], X + 22.0f * S, Y + 84.0f * S + LineH * i, 16.0f, WithAlpha(Colours.IsValidIndex(i) ? Colours[i] : Cream, A),
		       0.0f, i == 0);
	}
}

void DrawFishNotice(const FKGHudFrame& F, const UKGFishingComponent& Fishing)
{
	int32 NA = 0;
	int32 NB = 0;
	double At = 0.0;
	const EKGFishNotice Notice = Fishing.GetNotice(NA, NB, At);
	const float Age = static_cast<float>(F.Fx->Now - At);
	FString Text;
	FLinearColor Col = Cream;
	switch (Notice)
	{
	case EKGFishNotice::NoRod: Text = TEXT("No fishing rod - Madam Brine at the Fish Market lends one"); break;
	case EKGFishNotice::NotNow: Text = TEXT("Not now - the rod goes away"); break;
	case EKGFishNotice::TooEarly: Text = TEXT("Too early! You spooked it"); Col = FishWater; break;
	case EKGFishNotice::Missed: Text = TEXT("Missed the bite"); Col = FishWater; break;
	case EKGFishNotice::LineTooFar: Text = TEXT("You walked away - line reeled in"); break;
	case EKGFishNotice::LentRod: Text = TEXT("Madam Brine lends you a fishing rod (H)"); Col = GoldLight; break;
	case EKGFishNotice::Sold: Text = FString::Printf(TEXT("Sold %d to Madam Brine for %d gold"), NA, NB); Col = GoldLight; break;
	case EKGFishNotice::NothingToSell: Text = TEXT("Nothing to sell - catch something first"); break;
	case EKGFishNotice::PocketsFull: Text = TEXT("Pockets full!"); Col = FishDanger; break;
	case EKGFishNotice::Busy: Text = NA == 1 ? TEXT("You flinched - the fish got away") : TEXT("Hands full"); Col = NA == 1 ? FishDanger : Cream; break;
	default: return;
	}
	if (Age > 3.0f || Age < 0.0f)
	{
		return;
	}
	const float A = Saturate(Age / 0.15f) * Saturate((3.0f - Age) / 0.4f);
	const float S = F.S;
	const float MidY = F.CY + 150.0f * S;
	const float TW = static_cast<float>(F.P.Measure(Text, 18.0f, true).X);
	DrawLabelChip(F, MidY, TW + 40.0f * S, 40.0f * S, A);
	F.P.TextMid(Text, F.CX, MidY, 18.0f, WithAlpha(Col, A), 0.5f, true);
}

void DrawFishDebug(const FKGHudFrame& F, const UKGFishingComponent& Fishing)
{
	const FKGReelSim& L = Fishing.GetShownSim();
	const FKGReelSim& Srv = Fishing.GetFight().Sim;
	const FKGFishLine& Line = Fishing.GetLine();
	const UEnum* Phases = StaticEnum<EKGFishPhase>();
	TArray<FString> Rows;
	Rows.Add(FString::Printf(TEXT("FISH  shown %s  server %s  water %s"),
	                         *Phases->GetNameStringByValue(static_cast<int64>(Fishing.GetShownPhase())),
	                         *Phases->GetNameStringByValue(static_cast<int64>(Line.Phase)),
	                         *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(Line.Water))));
	Rows.Add(FString::Printf(TEXT("fight #%d  %s  %d g  pull %.2f  stam %.1fs  agil %.2f"), Fishing.GetFight().Serial,
	                         *Fishing.GetFight().ItemId.ToString(), Fishing.GetFight().Grams, L.Pull, L.StaminaSec, L.Agility));
	Rows.Add(FString::Printf(TEXT("tension %.3f (srv %.3f)  stamina %.2f (%.2f)  dist %.2f (%.2f)"), L.Tension, Srv.Tension,
	                         L.Stamina, Srv.Stamina, L.Distance, Srv.Distance));
	Rows.Add(FString::Printf(TEXT("fishX %+.2f  pullDir %+d  next %.2f  t %.2f  over %.2f  slack %.2f  P %.2f"), L.FishX,
	                         L.PullDir, L.NextSwitch, L.Elapsed, L.OverTime, L.SlackTime, L.CurrentPull()));
	Rows.Add(FString::Printf(TEXT("cast #%d nibble #%d bite #%d  landing %s  charge %.2f  focus %.2f"), Line.CastSerial,
	                         Line.NibbleSerial, Line.BiteSerial, *FVector(Line.Landing).ToCompactString(), Fishing.GetChargePower(),
	                         Fishing.GetFocusAlpha()));
	const float S = F.S;
	const float X = 24.0f * S;
	float Y = F.CY - 40.0f * S;
	F.P.RoundRect(X - 10.0f * S, Y - 8.0f * S, 720.0f * S, (Rows.Num() * 20.0f + 14.0f) * S, 8.0f * S, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	for (const FString& Row : Rows)
	{
		F.P.Text(Row, X, Y, 14.0f, FishWater, 0.0f, false);
		Y += 20.0f * S;
	}
}

void DrawFishing(const FKGHudFrame& F)
{
	const UKGFishingComponent* Fishing = F.Pawn ? UKGFishingComponent::FindFor(F.Pawn) : nullptr;
	if (!Fishing)
	{
		return;
	}
	const float S = F.S;
	const EKGFishPhase Phase = Fishing->GetShownPhase();
	const float HintY = F.H - 150.0f * S;
	switch (Phase)
	{
	case EKGFishPhase::Ready:
		FishHints(F, HintY, {{TEXT("LMB"), TEXT("Hold to cast")}, {TEXT("H"), TEXT("Put the rod away")}}, 1.0f);
		break;
	case EKGFishPhase::Charging:
		DrawFishCharge(F, Fishing->GetChargePower());
		FishHints(F, HintY, {{TEXT("LMB"), TEXT("Release to cast")}, {TEXT("RMB"), TEXT("Cancel")}}, 1.0f);
		break;
	case EKGFishPhase::Flight:
	case EKGFishPhase::Waiting:
		FishHints(F, HintY, {{TEXT("LMB"), TEXT("Strike when it bites")}, {TEXT("RMB"), TEXT("Reel in")}}, 1.0f);
		break;
	case EKGFishPhase::Bite:
		DrawFishBite(F, F.Fx->Now - Fishing->GetBiteShownAt());
		FishHints(F, HintY, {{TEXT("LMB"), TEXT("STRIKE!")}}, 1.0f);
		break;
	case EKGFishPhase::Fight:
		DrawFishFight(F, *Fishing);
		FishHints(F, HintY, {{TEXT("LMB"), TEXT("Hold to reel")}, {TEXT("A / D"), TEXT("Steer against it")}, {TEXT("RMB"), TEXT("Let it go")}}, 1.0f);
		break;
	default:
		break;
	}
	DrawFishCatchCard(F, *Fishing);
	DrawFishNotice(F, *Fishing);
	if (UKGFishingComponent::IsDebugOverlayOn())
	{
		DrawFishDebug(F, *Fishing);
	}
}
