#pragma once

#include "CoreMinimal.h"
#include "UI/Reveal/KGRevealTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

struct FSlateFontInfo;

/**
 * The role reveal ceremony (SPRINT-015, Docs/08_UI_UX.md "Role reveal"): a full-screen, painted dealer's table in the
 * Town of Salem tradition. The deck is shuffled, one card goes to every seat, yours slides up and flips: role name,
 * alignment colour, goal, one or two ability lines, the flavour line from the lore, and (Impatient teams) your
 * accomplices. The match world only shows once the phase ends and this fades out.
 *
 * Timing follows the server's RoleReveal clock (KGReveal beats, KGRevealTypes.h); FixedTime pins a moment for
 * off-screen screenshots (kg.UIShot reveal:<Role>:<stage>). Everything is vertex-painted: no assets.
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
};
