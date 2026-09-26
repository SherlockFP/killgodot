// SPRINT-040 manor marks for AKGHUD: a keyhole glyph at every DISCOVERED secret passage end and a pulsing ring at
// every trap whose alarm is pinging (minimap + full map). Undiscovered secrets are never drawn.
// Included at the end of UI/KGHUDMap.inl like Dig/KGDigHud.inl (leaves and re-opens the anonymous namespace for the
// includes; reuses FKGPainter, FKGHudFrame and the palette).
}   // namespace (anonymous, KGHUD.cpp)

#include "EngineUtils.h"
#include "Manor/KGSecretPassage.h"
#include "Traps/KGTrap.h"

namespace
{
/** A small door keyhole: a ring with a wedge below it. */
void ManorKeyhole(const FKGPainter& P, const FVector2D& C, float Size, float Opacity)
{
	const float R = Size * 0.22f;
	const float T = FMath::Max(1.5f, Size * 0.12f);
	P.Circle(C, R + T + 1.5f, WithAlpha(InkBottom, 0.85f * Opacity));
	P.Circle(C + FVector2D(0.0f, R * 0.9f), R * 0.9f + T + 1.5f, WithAlpha(InkBottom, 0.85f * Opacity));
	P.Circle(C, R, WithAlpha(Gold, Opacity));
	P.Line(C + FVector2D(-R * 0.55f, R * 1.7f), C + FVector2D(R * 0.55f, R * 1.7f), T + 1.0f, WithAlpha(Gold, Opacity), true);
	P.Line(C, C + FVector2D(0.0f, R * 1.7f), T + 1.0f, WithAlpha(Gold, Opacity), true);
}

void ManorMapMarks(const FKGHudFrame& F, TFunctionRef<FVector2D(const FVector2D&)> ToScreen, float Size, float Opacity,
                   const FVector2D* ClipCentre, float ClipRadius)
{
	UWorld* World = F.PC ? F.PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	auto Visible = [&](const FVector2D& Sc) { return !ClipCentre || FVector2D::Distance(Sc, *ClipCentre) <= ClipRadius - Size; };
	for (TActorIterator<AKGSecretPassage> It(World); It; ++It)
	{
		if (!It->IsDiscovered())
		{
			continue;   // a secret is only drawn after someone finds it
		}
		const FVector L = It->GetActorLocation();
		const FVector2D Sc = ToScreen(FVector2D(L.X, L.Y));
		if (Visible(Sc))
		{
			ManorKeyhole(F.P, Sc, Size, Opacity);
		}
	}
	const double Now = F.Fx ? F.Fx->Now : 0.0;
	for (TActorIterator<AKGTrap> It(World); It; ++It)
	{
		if (!It->IsPinging())
		{
			continue;
		}
		const FVector L = It->GetActorLocation();
		const FVector2D Sc = ToScreen(FVector2D(L.X, L.Y));
		if (!Visible(Sc))
		{
			continue;
		}
		const float Pulse = FMath::Frac(static_cast<float>(Now) * 1.4f);   // 0..1 every ~0.7 s
		const float R = Size * (0.4f + 1.1f * Pulse);
		F.P.Arc(Sc, R, FMath::Max(2.0f, Size * 0.14f), 0.0f, UE_TWO_PI, WithAlpha(ImpatientColor, Opacity * (1.0f - Pulse)));
		F.P.Circle(Sc, Size * 0.22f, WithAlpha(ImpatientColor, Opacity));
	}
}
