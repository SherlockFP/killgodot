// Digging + underground HUD for AKGHUD (Docs/01_GDD_Core.md section 16): the dig prompt and stage ring, key hints, the
// "dug up" card, toasts, the "you hear digging" cue, treasure-map X marks (world, minimap, full map) and the map
// switch to the underground plan below ground.
// Included at the end of UI/KGHUDMap.inl, i.e. inside KGHUD.cpp's anonymous namespace (reuses FKGPainter,
// FKGHudFrame, FKGMapFx and the palette). KGHUD.cpp is not edited: the Dig headers are pulled in at global scope by
// leaving the anonymous namespace for the includes and re-opening it (the same unique namespace).
}   // namespace (anonymous, KGHUD.cpp)

#include "Dig/KGDigComponent.h"
#include "Dig/KGDigManager.h"
#include "Dig/KGUndergroundInfo.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

namespace
{
const FLinearColor DigEarth = Srgb(214, 160, 104);
const FLinearColor DigRed = Srgb(240, 70, 56);

/** [Key] Label pairs centred on CX (like the fishing hints; that file comes after this one). */
void DigHints(const FKGHudFrame& F, float MidY, std::initializer_list<TPair<const TCHAR*, FString>> Hints, float Opacity)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float Px = 17.0f;
	const float Gap = 26.0f * S;
	float Total = 0.0f;
	for (const TPair<const TCHAR*, FString>& H : Hints)
	{
		Total += P.KeyHintWidth(H.Key, H.Value, Px);
	}
	Total += Gap * FMath::Max(0, static_cast<int32>(Hints.size()) - 1);
	DrawLabelChip(F, MidY, Total + 34.0f * S, 40.0f * S, Opacity);
	float X = F.CX - Total * 0.5f;
	for (const TPair<const TCHAR*, FString>& H : Hints)
	{
		P.KeyHint(H.Key, H.Value, X, MidY, Px, Cream, 0.0f, Opacity);
		X += P.KeyHintWidth(H.Key, H.Value, Px) + Gap;
	}
}

/** A bold painted X (treasure maps). */
void DigX(const FKGPainter& P, const FVector2D& C, float Size, float Opacity)
{
	const float T = FMath::Max(2.0f, Size * 0.22f);
	const float H = Size * 0.5f;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const float Tk = Pass == 0 ? T + FMath::Max(2.0f, Size * 0.12f) : T;
		const FLinearColor Col = Pass == 0 ? WithAlpha(InkBottom, 0.9f * Opacity) : WithAlpha(DigRed, Opacity);
		P.Line(C + FVector2D(-H, -H), C + FVector2D(H, H), Tk, Col, true);
		P.Line(C + FVector2D(-H, H), C + FVector2D(H, -H), Tk, Col, true);
	}
}

FString DigKindLabel(EKGDigKind Kind)
{
	switch (Kind)
	{
	case EKGDigKind::XMark: return TEXT("X MARKS THE SPOT");
	case EKGDigKind::Glint: return TEXT("SOMETHING SHINY");
	case EKGDigKind::Grave: return TEXT("A GRAVE");
	case EKGDigKind::Treasure: return TEXT("BURIED CHEST");
	default: return TEXT("FRESH EARTH");
	}
}

void DrawDigRing(const FKGHudFrame& F, float Progress, int32 Done, int32 Max, bool bLoud)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const FVector2D Ctr(F.CX, F.CY);
	const float R = 30.0f * S;
	const float Th = FMath::Max(3.0f, 6.0f * S);
	P.Arc(Ctr, R, Th + 8.0f * S, 0.0f, UE_TWO_PI, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 2.0f * S);
	P.Arc(Ctr, R, Th, 0.0f, UE_TWO_PI, FLinearColor(1.0f, 1.0f, 1.0f, 0.14f));
	if (Progress > 0.003f)
	{
		const float A0 = -UE_HALF_PI;
		P.Arc(Ctr, R, Th, A0, A0 + UE_TWO_PI * Saturate(Progress), bLoud ? ImpatientColor : DigEarth);
	}
	// Stage pips under the ring: filled = dug.
	const float Pip = 7.0f * S;
	const float Gap = 6.0f * S;
	const float W = Max * Pip * 2.0f + (Max - 1) * Gap;
	for (int32 i = 0; i < Max; ++i)
	{
		const FVector2D C(F.CX - W * 0.5f + Pip + i * (Pip * 2.0f + Gap), F.CY + R + 22.0f * S);
		P.Circle(C, Pip + 1.5f * S, WithAlpha(InkBottom, 0.8f));
		P.Circle(C, Pip, i < Done ? DigEarth : FLinearColor(1.0f, 1.0f, 1.0f, 0.18f));
	}
}

void DrawDigCard(const FKGHudFrame& F, const UKGDigComponent& Dig)
{
	const FKGDigResult& R = Dig.GetLastResult();
	const float Age = static_cast<float>(F.Fx->Now - Dig.GetResultShownAt());
	const bool bFinal = R.Stage >= R.MaxStage;
	if (Age < 0.0f || Age > 3.4f || (R.Items.Num() == 0 && !bFinal))
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float A = Saturate(Age / 0.2f) * Saturate((3.4f - Age) / 0.5f);
	const float Pop = EaseOutBack(Saturate(Age / 0.3f));
	const float W = 360.0f * S;
	const float LineH = 26.0f * S;
	const int32 Rows = FMath::Max(1, R.Items.Num());
	const float H = (70.0f + 8.0f) * S + LineH * Rows + (R.bDropped ? LineH : 0.0f);
	const float X = F.W - W - 40.0f * S + (1.0f - Pop) * 40.0f * S;
	const float Y = F.CY - H * 0.5f + 40.0f * S;
	P.Card(X, Y, W, H, A, 16.0f * S, true);
	P.TopBand(X, Y, W, 16.0f * S, 6.0f * S, WithAlpha(R.Kind == EKGDigKind::Grave ? ImpatientColor : DigEarth, A));
	const FString Title = bFinal ? (R.Kind == EKGDigKind::Treasure ? TEXT("TREASURE!") : TEXT("DUG OUT"))
	                             : FString::Printf(TEXT("DIGGING %d/%d"), R.Stage, R.MaxStage);
	P.Text(Title, X + 22.0f * S, Y + 18.0f * S, 14.0f, WithAlpha(R.Kind == EKGDigKind::Grave ? ImpatientColor : Gold, A), 0.0f, true, 3.0f);
	P.Text(DigKindLabel(R.Kind), X + 22.0f * S, Y + 38.0f * S, 20.0f, WithAlpha(Cream, A), 0.0f, true);
	float LY = Y + 72.0f * S;
	if (R.Items.Num() == 0)
	{
		P.Text(TEXT("Nothing but dirt and worms."), X + 22.0f * S, LY, 16.0f, WithAlpha(Cream, 0.75f * A), 0.0f, false);
	}
	for (const FKGItemStack& Stack : R.Items)
	{
		const FKGItemDef* Def = UKGItemCatalog::Find(Stack.ItemId);
		const FLinearColor Col = Def ? UKGItemCatalog::GetRarityColor(Def->Rarity) : Cream;
		P.Circle(FVector2D(X + 30.0f * S, LY + 10.0f * S), 6.0f * S, WithAlpha(Def ? Def->IconColor : Cream, A));
		P.Text(FString::Printf(TEXT("%s  x%d"), *UKGItemCatalog::GetDisplayName(Stack.ItemId).ToString(), Stack.Count),
		       X + 46.0f * S, LY, 17.0f, WithAlpha(Def && Def->Rarity != EKGRarity::Common ? Col : Cream, A), 0.0f, true);
		LY += LineH;
	}
	if (R.bDropped)
	{
		P.Text(TEXT("Pockets full - the rest lies by the hole"), X + 22.0f * S, LY, 14.0f, WithAlpha(ImpatientColor, A), 0.0f, false);
	}
}

void DrawDigNotice(const FKGHudFrame& F, const UKGDigComponent& Dig)
{
	int32 NA = 0;
	int32 NB = 0;
	double At = 0.0;
	const EKGDigNotice Notice = Dig.GetNotice(NA, NB, At);
	const float Age = static_cast<float>(F.Fx->Now - At);
	FString Text;
	FLinearColor Col = Cream;
	switch (Notice)
	{
	case EKGDigNotice::NoShovel: Text = TEXT("No shovel - the gravedigger leaves one by the lych gate"); break;
	case EKGDigNotice::NotNow: Text = TEXT("Not now - the shovel goes away"); break;
	case EKGDigNotice::Busy: Text = NA == 1 ? TEXT("You flinched - the rhythm is lost") : TEXT("Someone else is digging here"); break;
	case EKGDigNotice::NothingHere: Text = TEXT("Just dirt. Look for mounds, X marks and glints"); break;
	case EKGDigNotice::TooFar: Text = TEXT("Get closer to the hole"); break;
	case EKGDigNotice::DugOut: Text = TEXT("This hole is empty"); break;
	case EKGDigNotice::PocketsFull: Text = TEXT("Pockets full!"); Col = ImpatientColor; break;
	case EKGDigNotice::MapAssembled: Text = TEXT("The scraps fit together: a treasure map! (M)"); Col = GoldLight; break;
	case EKGDigNotice::GraveWarning: Text = TEXT("Disturbing a grave... everyone nearby will hear it"); Col = ImpatientColor; break;
	case EKGDigNotice::GateLocked: Text = TEXT("Locked. An old crypt key would fit"); break;
	case EKGDigNotice::GateOpened: Text = TEXT("The crypt key turns. The gate groans open"); Col = GoldLight; break;
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

/** Somebody's spade on a grave (4 s after each loud stage): the social-deduction cue, never who. */
void DrawDigNoise(const FKGHudFrame& F, const AKGDigManager* Manager, const UKGDigComponent* MyDig)
{
	FVector Where;
	double At = 0.0;
	if (!Manager || !F.Pawn || !Manager->GetLastNoise(Where, At))
	{
		return;
	}
	const float Age = static_cast<float>(F.Fx->Now - At);
	if (Age < 0.0f || Age > 4.0f || FVector::Dist(Where, F.Pawn->GetActorLocation()) > FKGDigRules::GraveNoiseRadius)
	{
		return;
	}
	if (MyDig && MyDig->IsDiggingShown())
	{
		return;   // your own spade
	}
	const float A = Saturate(Age / 0.25f) * Saturate((4.0f - Age) / 0.6f);
	const float S = F.S;
	const FString Text = TEXT("You hear a spade on stone... someone is digging a grave");
	const float MidY = 172.0f * S;
	const float TW = static_cast<float>(F.P.Measure(Text, 17.0f, true).X);
	DrawLabelChip(F, MidY, TW + 40.0f * S, 38.0f * S, A);
	F.P.TextMid(Text, F.CX, MidY, 17.0f, WithAlpha(ImpatientColor, A), 0.5f, true);
}

/** Treasure-map X marks in the world (projected), within 30 m. */
void DrawDigWorldMarks(const FKGHudFrame& F, const UKGDigComponent& Dig)
{
	if (!F.PC || Dig.GetTreasureMarks().Num() == 0)
	{
		return;
	}
	const float S = F.S;
	for (const FVector2D& M : Dig.GetTreasureMarks())
	{
		const FVector World(M.X, M.Y, F.Pawn->GetActorLocation().Z - 60.0);
		const float Dist = FVector::Dist2D(World, F.Pawn->GetActorLocation());
		if (Dist > 3000.0f)
		{
			continue;
		}
		FVector2D Screen;
		if (!UGameplayStatics::ProjectWorldToScreen(F.PC, World, Screen, true))
		{
			continue;
		}
		const float A = Saturate((3000.0f - Dist) / 800.0f);
		DigX(F.P, Screen, 20.0f * S, 0.85f * A);
		F.P.TextMid(FString::Printf(TEXT("%.0f m"), Dist / 100.0f), Screen.X, Screen.Y + 22.0f * S, 13.0f, WithAlpha(Cream, A), 0.5f, true);
	}
}

void DrawDigLayer(const FKGHudFrame& F)
{
	const UWorld* World = F.Hud ? F.Hud->GetWorld() : nullptr;
	const AKGDigManager* Manager = AKGDigManager::Get(World);
	const UKGDigComponent* Dig = F.Pawn ? UKGDigComponent::FindFor(F.Pawn) : nullptr;
	DrawDigNoise(F, Manager, Dig);
	if (!Dig)
	{
		return;
	}
	const float S = F.S;
	const float HintY = F.H - 150.0f * S;
	DrawDigWorldMarks(F, *Dig);
	if (Dig->IsShovelShown())
	{
		const int32 Aim = Dig->GetAimSpotIndex();
		const FKGDigSpot* Spot = Manager && Manager->GetSpots().IsValidIndex(Aim) ? &Manager->GetSpots()[Aim] : nullptr;
		if (Spot)
		{
			const bool bLoud = FKGDigRules::IsLoud(Spot->Kind);
			if (Dig->IsDiggingShown())
			{
				DrawDigRing(F, Dig->GetStageProgress(), Spot->Stage, Spot->MaxStage, bLoud);
			}
			const float LabelY = F.CY + 78.0f * S;
			const FString Label = Spot->IsDugOut() ? FString(TEXT("DUG OUT")) : DigKindLabel(Spot->Kind);
			F.P.TextMid(Label, F.CX, LabelY, 15.0f, WithAlpha(bLoud ? ImpatientColor : GoldLight, 0.95f), 0.5f, true, 2.5f);
			if (!Spot->IsDugOut())
			{
				DigHints(F, HintY, {{TEXT("LMB"), bLoud ? FString(TEXT("Hold to dig (loud!)")) : FString(TEXT("Hold to dig"))},
				                    {TEXT("Q"), FString(TEXT("Put the shovel away"))}}, 1.0f);
			}
			else
			{
				DigHints(F, HintY, {{TEXT("Q"), FString(TEXT("Put the shovel away"))}}, 1.0f);
			}
		}
		else if (Dig->IsAimOnTreasureMark())
		{
			if (Dig->IsDiggingShown())
			{
				DrawDigRing(F, Dig->GetStageProgress(), 0, FKGDigRules::StagesFor(EKGDigKind::Treasure), false);
			}
			F.P.TextMid(TEXT("THE X ON YOUR MAP"), F.CX, F.CY + 78.0f * S, 15.0f, WithAlpha(DigRed, 0.95f), 0.5f, true, 2.5f);
			DigHints(F, HintY, {{TEXT("LMB"), FString(TEXT("Dig here"))}, {TEXT("Q"), FString(TEXT("Put the shovel away"))}}, 1.0f);
		}
		else
		{
			DigHints(F, HintY, {{TEXT("Q"), FString(TEXT("Put the shovel away"))}}, 0.85f);
		}
	}
	DrawDigCard(F, *Dig);
	DrawDigNotice(F, *Dig);
}

/** Below ground the map shows the underground plan; the location tracking restarts on every switch. */
const AKGMapInfo* DigResolvePlan(const FKGHudFrame& F, FKGMapFx& M, const AKGMapInfo* Surface)
{
	static TMap<TObjectKey<AKGHUD>, TWeakObjectPtr<const AKGMapInfo>> Shown;
	UWorld* World = F.Hud ? F.Hud->GetWorld() : nullptr;
	AKGUndergroundInfo* Under = AKGUndergroundInfo::Find(World);
	const AKGMapInfo* Want = Surface;
	if (Under)
	{
		FVector Where = FVector::ZeroVector;
		if (F.Pawn)
		{
			Where = F.Pawn->GetActorLocation();
		}
		else if (F.PC && F.PC->PlayerCameraManager)
		{
			Where = F.PC->PlayerCameraManager->GetCameraLocation();
		}
		if (Under->Contains(Where))
		{
			if (const AKGMapInfo* Plan = Under->GetPlan())
			{
				Want = Plan;
			}
		}
	}
	TWeakObjectPtr<const AKGMapInfo>& Last = Shown.FindOrAdd(F.Hud);
	if (Last.Get() != Want)
	{
		const bool bFirst = !Last.IsValid();
		Last = Want;
		M.Here = M.District = M.CandHere = M.CandDistrict = INDEX_NONE;
		M.ToastedAt.Reset();
		M.bPrimed = !bFirst;   // a switch toasts the new place ("WELL CELLAR"); the first frame stays quiet
	}
	return Want;
}

/** Treasure X marks on a map view (ToScreen: world XY -> screen). Clip: only inside the minimap disc. */
void DigMapMarks(const FKGHudFrame& F, TFunctionRef<FVector2D(const FVector2D&)> ToScreen, float Size, float Opacity,
                 const FVector2D* ClipCentre, float ClipRadius)
{
	const UKGDigComponent* Dig = F.Pawn ? UKGDigComponent::FindFor(F.Pawn) : nullptr;
	if (!Dig)
	{
		return;
	}
	for (const FVector2D& Mark : Dig->GetTreasureMarks())
	{
		const FVector2D Sc = ToScreen(Mark);
		if (ClipCentre && FVector2D::Distance(Sc, *ClipCentre) > ClipRadius - Size)
		{
			continue;
		}
		DigX(F.P, Sc, Size, Opacity);
	}
}
