// SPRINT-016 world chore HUD for AKGHUD: waypoints on the minimap / full map (the current step of every open world
// chore, or your dropped item), a compass strip with those waypoints under the phase panel, the current step line, a
// water gauge while carrying a bucket, the work ring (crank, chop, pour...) and step / warning toasts.
// Included at the end of UI/KGHUDMap.inl, i.e. inside KGHUD.cpp's anonymous namespace (reuses FKGPainter,
// FKGHudFrame, FKGChorePin and the palette). Like Dig/KGDigHud.inl it leaves the anonymous namespace for its includes
// and re-opens it (the same unique namespace).
}   // namespace (anonymous, KGHUD.cpp)

#include "Chores/WorldChores/KGWorldChoreComponent.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "World/KGChoreItem.h"

namespace
{
bool IsWorldChoreId(FName Id)
{
	return FKGWorldChoreCatalog::Get().IsWorldChore(Id);
}

void GatherWorldChorePins(const FKGHudFrame& F, TArray<FKGChorePin>& Out)
{
	if (const UKGWorldChoreComponent* WC = F.Pawn ? UKGWorldChoreComponent::FindFor(F.Pawn) : nullptr)
	{
		TArray<FKGWorldWaypoint> Points;
		WC->GetWaypoints(Points);
		for (const FKGWorldWaypoint& W : Points)
		{
			Out.Add({FVector2D(W.Location), W.bActive});
		}
	}
}

void DrawWorldChoreHud(const FKGHudFrame& F)
{
	const UKGWorldChoreComponent* WC = F.Pawn ? UKGWorldChoreComponent::FindFor(F.Pawn) : nullptr;
	if (!WC || !F.GS || F.GS->GetPhase() == EKGPhase::Epilogue || F.GS->GetPhase() == EKGPhase::Meeting ||
	    F.GS->GetPhase() == EKGPhase::Trial)
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	const double Now = F.Hud->GetWorld()->GetTimeSeconds();
	TArray<FKGWorldWaypoint> Points;
	WC->GetWaypoints(Points);
	FName ActiveChore;
	const FString Label = WC->GetActiveLabel(&ActiveChore);

	// Compass strip under the phase panel: 180 degrees of view, cardinal letters, gold diamonds for waypoints.
	if (Points.Num() > 0)
	{
		const float CW = 560.0f * S;
		const float CH = 30.0f * S;
		const float Y0 = 142.0f * S;
		const float X0 = F.CX - CW * 0.5f;
		const float PxPerDeg = CW / 180.0f;
		const float Yaw = F.PC && F.PC->PlayerCameraManager ? F.PC->PlayerCameraManager->GetCameraRotation().Yaw : F.Pawn->GetControlRotation().Yaw;
		P.RoundRect(X0, Y0, CW, CH, CH * 0.5f, WithAlpha(InkBottom, 0.55f));
		const TCHAR* Cardinals[4] = {TEXT("E"), TEXT("S"), TEXT("W"), TEXT("N")};   // UE yaw 0 = +X (east); +Y = the sea (south)
		for (int32 Deg = 0; Deg < 360; Deg += 15)
		{
			const float D = FMath::FindDeltaAngleDegrees(Yaw, static_cast<float>(Deg));
			if (FMath::Abs(D) > 88.0f)
			{
				continue;
			}
			const float X = F.CX + D * PxPerDeg;
			const float Fade = 1.0f - FMath::Abs(D) / 90.0f;
			if (Deg % 90 == 0)
			{
				P.TextMid(Cardinals[Deg / 90], X, Y0 + CH * 0.5f, 15.0f, WithAlpha(Cream, 0.9f * Fade), 0.5f, true);
			}
			else
			{
				P.Line(FVector2D(X, Y0 + CH * 0.32f), FVector2D(X, Y0 + CH * 0.68f), FMath::Max(1.0f, 1.5f * S), WithAlpha(Cream, 0.35f * Fade));
			}
		}
		P.Line(FVector2D(F.CX, Y0 - 3.0f * S), FVector2D(F.CX, Y0 + 5.0f * S), FMath::Max(1.5f, 2.0f * S), WithAlpha(Cream, 0.8f));
		for (int32 i = Points.Num() - 1; i >= 0; --i)
		{
			const FKGWorldWaypoint& W = Points[i];
			const FVector Delta = W.Location - F.Pawn->GetActorLocation();
			const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const float D = FMath::FindDeltaAngleDegrees(Yaw, Bearing);
			const bool bEdge = FMath::Abs(D) > 88.0f;
			const float X = F.CX + FMath::Clamp(D, -88.0f, 88.0f) * PxPerDeg;
			const float R = (W.bActive ? 8.0f : 5.5f) * S;
			const FLinearColor Col = W.bItem ? Srgb(120, 200, 255) : (W.bActive ? Gold : WithAlpha(GoldLight, 0.7f));
			P.Diamond(FVector2D(X, Y0 + CH * 0.5f), R + 2.0f * S, WithAlpha(InkBottom, 0.9f));
			P.Diamond(FVector2D(X, Y0 + CH * 0.5f), R, Col);
			if (W.bActive && i == 0)
			{
				const int32 Metres = FMath::RoundToInt(static_cast<float>(Delta.Size2D()) / 100.0f);
				const FString Dist = bEdge ? FString::Printf(TEXT("%s %d m"), D < 0.0f ? TEXT("<") : TEXT(">"), Metres)
				                           : FString::Printf(TEXT("%d m"), Metres);
				P.TextMid(Dist, X, Y0 + CH + 11.0f * S, 13.0f, Gold, 0.5f, true);
			}
		}
		if (!Label.IsEmpty())
		{
			const float LW = static_cast<float>(P.Measure(Label, 16.0f, true).X);
			P.RoundRect(F.CX - LW * 0.5f - 16.0f * S, Y0 + CH + 22.0f * S, LW + 32.0f * S, 30.0f * S, 15.0f * S, WithAlpha(InkBottom, 0.55f));
			P.TextMid(Label, F.CX, Y0 + CH + 37.0f * S, 16.0f, Cream, 0.5f, true);
		}
		// Carrying water: how much is left in the bucket (the fill also shows on the bucket itself).
		if (const FKGWorldProgress* Prog = ActiveChore.IsNone() ? nullptr : WC->FindProgress(ActiveChore))
		{
			const AKGChoreItem* Item = Prog->Item;
			const FKGWorldItemDef* Def = Item ? Item->GetDef() : nullptr;
			if (Def && Def->bLiquid && Item->IsCarriedBy(F.Pawn))
			{
				const float BW = 180.0f * S;
				const float BY = Y0 + CH + 60.0f * S;
				P.RoundRect(F.CX - BW * 0.5f, BY, BW, 10.0f * S, 5.0f * S, WithAlpha(InkBottom, 0.6f));
				P.RoundRect(F.CX - BW * 0.5f, BY, FMath::Max(10.0f * S, BW * Item->GetFill()), 10.0f * S, 5.0f * S, Srgb(70, 160, 240));
				P.TextMid(FString::Printf(TEXT("Water %d%%"), FMath::RoundToInt(Item->GetFill() * 100.0f)), F.CX + BW * 0.5f + 10.0f * S,
				          BY + 5.0f * S, 13.0f, WithAlpha(Cream, 0.85f), 0.0f, true);
			}
		}
	}

	// Work ring around the crosshair (crank, chop, pour, knock, sabotage...).
	const FKGWorldDwell& Dwell = WC->GetDwell();
	if (Dwell.Kind != EKGDwell::None)
	{
		const FVector2D Ctr(F.CX, F.CY);
		const float R = 34.0f * S;
		const float Th = FMath::Max(3.0f, 6.0f * S);
		const FLinearColor Col = Dwell.Kind == EKGDwell::Sabotage ? ImpatientColor : Dwell.Kind == EKGDwell::Dump ? Srgb(140, 210, 90) : Gold;
		P.Arc(Ctr, R, Th + 6.0f * S, 0.0f, UE_TWO_PI, FLinearColor(0.0f, 0.0f, 0.0f, 0.3f), 2.0f * S);
		if (Dwell.Alpha() > 0.003f)
		{
			P.Arc(Ctr, R, Th, -UE_HALF_PI, -UE_HALF_PI + UE_TWO_PI * Dwell.Alpha(), Col);
		}
		const TCHAR* What = Dwell.Kind == EKGDwell::Sabotage ? TEXT("Sabotaging...")
		                  : Dwell.Kind == EKGDwell::Dump   ? TEXT("Dumping the poison...")
		                  : Dwell.Kind == EKGDwell::Bring  ? TEXT("Hold it there...")
		                                                   : TEXT("Working...");
		P.TextMid(What, F.CX, F.CY + 104.0f * S, 16.0f, Col, 0.5f, true);
	}

	// Toasts: a step done (green tick) or a warning (spilled, poisoned, lost).
	const double DoneAge = Now - WC->GetStepDoneAt();
	if (DoneAge >= 0.0 && DoneAge < 2.6 && !WC->GetStepDoneText().IsEmpty())
	{
		const float A = static_cast<float>(FMath::Clamp((2.6 - DoneAge) / 0.5, 0.0, 1.0));
		const float Pop = EaseOutCubic(static_cast<float>(FMath::Clamp(DoneAge / 0.25, 0.0, 1.0)));
		const FString& Text = WC->GetStepDoneText();
		const float TW = static_cast<float>(P.Measure(Text, 20.0f, true).X);
		const float Y = F.CY - 150.0f * S - 10.0f * S * Pop;
		P.RoundRect(F.CX - TW * 0.5f - 44.0f * S, Y - 20.0f * S, TW + 64.0f * S, 40.0f * S, 20.0f * S, WithAlpha(InkBottom, 0.7f * A));
		P.Check(FVector2D(F.CX - TW * 0.5f - 22.0f * S, Y), 18.0f * S * FMath::Max(0.05f, Pop), FMath::Max(2.0f, 3.0f * S), WithAlpha(TownColor, A));
		P.TextMid(Text, F.CX - TW * 0.5f, Y, 20.0f, WithAlpha(Cream, A), 0.0f, true);
	}
	const double NoticeAge = Now - WC->GetNoticeAt();
	if (NoticeAge >= 0.0 && NoticeAge < 3.5 && !WC->GetNotice().IsEmpty())
	{
		const float A = static_cast<float>(FMath::Clamp((3.5 - NoticeAge) / 0.5, 0.0, 1.0));
		const FString& Text = WC->GetNotice();
		const float TW = static_cast<float>(P.Measure(Text, 18.0f, true).X);
		const float Y = F.CY - 200.0f * S;
		P.RoundRect(F.CX - TW * 0.5f - 20.0f * S, Y - 18.0f * S, TW + 40.0f * S, 36.0f * S, 18.0f * S, WithAlpha(InkBottom, 0.72f * A));
		P.TextMid(Text, F.CX, Y, 18.0f, WithAlpha(ImpatientColor, A), 0.5f, true);
	}
}
