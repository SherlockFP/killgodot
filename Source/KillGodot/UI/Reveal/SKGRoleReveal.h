#pragma once

#include "CoreMinimal.h"
#include "UI/Reveal/KGRevealTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

struct FSlateFontInfo;

/**
 * The role reveal MOMENT (SPRINT-037, Docs/08_UI_UX.md "Rol töreni"; replaces the SPRINT-015 dealer's table): a dark
 * room, your card spins up and trembles ("WHO ARE YOU?"), flips - white flash, the screen floods in your alignment's
 * colour, a camera punch, sparks / confetti / glitter burst out, the sting plays (UKGRevealSubsystem) - and three
 * things slam in: a HUGE role name, the alignment banner ("YOU ARE IMPATIENT") and one plain "what you do" line.
 * Impatient teams see their accomplices step up beside the card as silhouettes with names (a lineup, not a table).
 * Goal, abilities and flavour live in the optional details view (hold Tab) and in the HUD role card afterwards.
 *
 * Timing follows the server's RoleReveal clock (KGReveal beats, KGRevealTypes.h); FixedTime pins a moment for
 * off-screen screenshots (kg.UIShot reveal:<Role>:<stage>). Art: /Game/KillGodot/UI/Reveal textures
 * (Tools/UI/kg_make_reveal_art.py), with painted fallbacks when they are missing.
 */
class KILLGODOT_API SKGRoleReveal : public SLeafWidget
{
public:
	using FViewProvider = TFunction<FKGRevealView()>;

	SLATE_BEGIN_ARGS(SKGRoleReveal)
		: _FixedTime(-1.0f)
	{}
		/** Live data (UKGRevealSubsystem); called every tick. */
		SLATE_ARGUMENT(FViewProvider, ViewProvider)
		/** Used when there is no provider (screenshots). */
		SLATE_ARGUMENT(FKGRevealView, StaticView)
		/** >= 0: draw exactly this moment of the timeline (screenshots). */
		SLATE_ARGUMENT(float, FixedTime)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Seconds into the ceremony as drawn. */
	float GetTime() const { return Time; }
	/** The fade-out after the phase ended has finished: the owner may remove the widget. */
	bool IsFinished() const { return OutroTime >= KGReveal::OutroSeconds; }
	const FKGRevealView& GetView() const { return View; }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0, 1080.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

private:
	FViewProvider Provider;
	FKGRevealView View;
	float FixedTime = -1.0f;
	float Time = 0.0f;
	float OutroTime = 0.0f;
	/** Seconds since the face-up card was first drawn with ready pressed (for the "ready" pop). */
	float ReadyAge = 0.0f;
	/** Free-running clock for idle loops (tremble, rays, motes) that must keep moving while Time waits for the role. */
	float AnimTime = 0.0f;
	/** 0..1 fade of the details view (Tab held). */
	float DetailsAlpha = 0.0f;
};
