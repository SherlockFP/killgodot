#pragma once

#include "CoreMinimal.h"
#include "Chores/KGChoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FKGMinigame;
class FKGMgPainter;
struct FKGChoreDef;

DECLARE_DELEGATE_OneParam(FKGOnChoreStageDone, int32 /*Stage*/);
DECLARE_DELEGATE_OneParam(FKGOnChoreClosed, EKGChoreClose /*Reason*/);

/**
 * The chore minigame window (Among Us style): a rounded dusk panel in the middle of the screen over a slightly blurred,
 * dimmed first-person view. Header = chore title, the current stage's instruction and stage pips; body = the
 * minigame canvas; footer = key hints. Hosts one FKGMinigame: ticks it, forwards mouse/keys, plays its feedback
 * (sounds, pop text, shake), celebrates solved stages and reports them (OnStageDone -> UKGChoreComponent -> server).
 * Esc / X / W A S D / E leave (progress kept by the server). Styled like UI/Menu (FKGMenuStyle palette, Roboto).
 */
class KILLGODOT_API SKGChorePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGChorePanel)
		: _Seed(1)
		, _StartStage(0)
		, _bFake(false)
		, _bPractice(false)
		, _bShot(false)
	{}
		SLATE_ARGUMENT(FName, ChoreId)
		SLATE_ARGUMENT(int32, Seed)
		SLATE_ARGUMENT(int32, StartStage)
		/** The Impatient faking the chore: identical panel, a small "faking" tag only they can see. */
		SLATE_ARGUMENT(bool, bFake)
		/** Dev play without the chore on your list: nothing is credited. */
		SLATE_ARGUMENT(bool, bPractice)
		/** Off-screen screenshot: a painted village backdrop instead of the blurred game view. */
		SLATE_ARGUMENT(bool, bShot)
		/** World context for UI sounds (none in screenshots). */
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, SoundContext)
		SLATE_EVENT(FKGOnChoreStageDone, OnStageDone)
		SLATE_EVENT(FKGOnChoreClosed, OnClosed)
	SLATE_END_ARGS()

	SKGChorePanel();
	virtual ~SKGChorePanel() override;

	void Construct(const FArguments& InArgs);

	FName GetChoreId() const { return ChoreId; }
	FKGMinigame* GetGame() const { return Game.Get(); }
	bool IsClosing() const { return bClosing; }

	/** Server said no to Stage (e.g. too fast): banner + replay that stage. */
	void ServerRejected(int32 Stage, const FString& Why);
	/** Server closed it (meeting, hit, pushed away): short banner, then OnClosed(Reason). */
	void ForceClose(EKGChoreClose Reason);
	/** Dev auto-win: the stages solve themselves once the server's floor has passed. */
	void SetAutoPlay(bool bOn) { bAuto = bOn; }
	bool IsAutoPlay() const { return bAuto; }
	/** Screenshots/tests: run Seconds of simulated time (then optionally freeze the minigame's debug pose). */
	void DebugAdvance(float Seconds, bool bPose);
	/** Headless (-nullrhi) processes never tick viewport widgets: the owning component steps the panel instead. */
	void ManualTick(float Dt) { Step(FMath::Min(Dt, 0.1f)); }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	struct FLayout
	{
		FGeometry Window;   // logical window space (WinW x WinH)
		FGeometry Content;  // minigame space (KGMg::W x KGMg::H)
		float Scale = 1.0f;
	};
	FLayout ComputeLayout(const FGeometry& Geometry) const;
	void Step(float Dt);
	void BeginClose(EKGChoreClose Reason, float Delay);
	void PlaySound(FName Name, float Volume, float Pitch) const;
	void PaintBackdrop(FKGMgPainter& P, const FVector2f& ViewSize) const;
	void PaintHeader(FKGMgPainter& P) const;
	void PaintOverlays(FKGMgPainter& P) const;
	bool IsCloseHovered() const;

	struct FPop
	{
		FString Text;
		FVector2f At;
		FLinearColor Color;
		float Age = 0.0f;
	};

	FName ChoreId;
	const FKGChoreDef* Def = nullptr;
	TUniquePtr<FKGMinigame> Game;
	TWeakObjectPtr<UObject> SoundContext;
	FKGOnChoreStageDone OnStageDone;
	FKGOnChoreClosed OnClosed;
	bool bFake = false;
	bool bPractice = false;
	bool bShot = false;
	bool bAuto = false;
	/** Screenshot pose: time stands still. */
	bool bFrozen = false;

	TArray<FPop> Pops;
	float Appear = 0.0f;
	float Shake = 0.0f;
	float ShakeT = 0.0f;
	float OpenAge = 0.0f;
	/** Stage solved and reported; celebrating before the next stage. */
	bool bStageReported = false;
	float SolveAge = 0.0f;
	int32 SolvedStage = -1;
	/** Reject / interrupt banner. */
	FString Banner;
	float BannerAge = 100.0f;
	FLinearColor BannerColor = FLinearColor::White;
	bool bClosing = false;
	bool bClosedFired = false;
	float CloseDelay = 0.0f;
	float CloseAge = 0.0f;
	EKGChoreClose CloseReason = EKGChoreClose::Left;
	bool bDoneStamp = false;
	FVector2f WindowMouse = FVector2f(-1000.0f, -1000.0f);
	FVector2f ViewSize = FVector2f(1920.0f, 1080.0f);
};
