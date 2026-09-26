#pragma once

#include "CoreMinimal.h"
#include "Tabletop/KGBoardTypes.h"
#include "Widgets/SLeafWidget.h"

struct FSlateFontInfo;

/** What the panel draws. UKGTabletopSubsystem refreshes it every frame from the replicated table. */
struct FKGTableView
{
	FKGBoardState Board;
	EKGTableStatus Status = EKGTableStatus::Waiting;
	EKGTableEndReason Reason = EKGTableEndReason::None;
	/** 0 white, 1 black, -1 spectator (white at the bottom). */
	int32 MyColour = -1;
	float Clock[2] = {0.0f, 0.0f};
	bool bTimed = true;
	FString Names[2];
	FString Place;
	FString ClockName;
	int32 LastFrom = -1;
	int32 LastTo = -1;
	uint8 DrawOffers = 0;
	bool bOpen = true;
	bool bBot[2] = {false, false};
	FString Notice;
	double NoticeTime = -100.0;
	/** Spectator strip only (clocks and names at the top of the screen). */
	bool bStrip = false;
};

DECLARE_DELEGATE_OneParam(FKGOnTableMove, const FString& /*MoveText*/);
DECLARE_DELEGATE_OneParam(FKGOnTableAction, uint8 /*EKGTableAction*/);

/**
 * The 2D board panel (Docs/Design/Tabletop_Games.md section 7 "Panel"): the MVP play surface until the 3D board
 * view (036c) lands, and the spectator clock strip. Everything is painted in code in the front-end style
 * (FKGMenuStyle): cream / ink pieces told apart by fill AND letter (colour-blind safe), last move in brass, the
 * checked king in a crimson ring, legal-move dots on the selected piece, a 4-way promotion chooser, Draw / Resign
 * buttons (resign needs a second click within 3 s). Mouse only; keys stay with the game (E / Space stand up).
 */
class KILLGODOT_API SKGTablePanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGTablePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FKGTableView>, View)
		SLATE_EVENT(FKGOnTableMove, OnMove)
		SLATE_EVENT(FKGOnTableAction, OnAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(0.0, 0.0); }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }

	/** Screenshots: select a square as if clicked (legal-move dots appear). */
	void DebugSelect(int32 Sq);

private:
	struct FLayout
	{
		float S = 1.0f;          // design-pixel scale
		FVector2f Card;          // top-left of the card (local px)
		FVector2f CardSize;
		FVector2f BoardPos;      // top-left of the board
		float Cell = 52.0f;      // px per square
		FVector2f DrawButton, ResignButton, RematchButton, ButtonSize;
		bool bBottomWhite = true;
	};
	FLayout Layout(const FGeometry& G) const;
	int32 SquareAt(const FLayout& L, const FVector2f& P) const;
	FVector2f SquareOrigin(const FLayout& L, int32 Sq) const;
	void Select(int32 Sq);
	void ClickSquare(int32 Sq);
	bool CanPlay() const;
	void PaintStrip(const FGeometry& G, FSlateWindowElementList& Out, int32& Layer) const;

	TSharedPtr<FKGTableView> View;
	FKGOnTableMove OnMove;
	FKGOnTableAction OnAction;

	int32 SelectedSq = -1;
	TArray<FKGTableMove> LegalFromSelected;
	int32 PromoFrom = -1;
	int32 PromoTo = -1;
	double ResignArmedAt = -100.0;
	mutable uint16 SeenPly = 65535;
};
