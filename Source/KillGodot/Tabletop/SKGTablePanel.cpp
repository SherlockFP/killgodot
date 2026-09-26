#include "Tabletop/SKGTablePanel.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Tabletop/KGTableRules.h"
#include "UI/KGUITokens.h"
#include "UI/Menu/KGMenuStyle.h"

#define LOCTEXT_NAMESPACE "KGTable"

namespace KGTablePanelPrivate
{
	constexpr float Pad = 24.0f;
	constexpr float HeaderH = 88.0f;
	constexpr float FooterH = 64.0f;
	constexpr float CellPx = 52.0f;
	constexpr float PromoPause = 3.0f;

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(Text.Len() * Font.Size * 0.55, Font.Size * 1.3);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	/** Align 0 left, 0.5 centre, 1 right; Pos.Y is the top of the line. */
	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& Pos, const FString& S,
	          const FSlateFontInfo& Font, const FLinearColor& Colour, float Align = 0.0f)
	{
		const FVector2D Size = Measure(S, Font);
		const FVector2f P(Pos.X - static_cast<float>(Size.X) * Align, Pos.Y);
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(P)), S, Font,
		                            ESlateDrawEffect::None, Colour);
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& Pos, const FVector2f& Size,
	         const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float Width = 0.0f)
	{
		FKGMenuStyle::DrawRoundedBox(Out, Layer, G, Pos, Size, Fill, Radius, Outline, Width);
	}

	void Circle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& Centre, float R,
	            const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float Width = 0.0f)
	{
		Box(Out, Layer, G, Centre - FVector2f(R, R), FVector2f(2.0f * R, 2.0f * R), Fill, -1.0f, Outline, Width);
	}

	FString ClockText(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
		if (Seconds < 10.0f)
		{
			return FString::Printf(TEXT("%.1f"), FMath::Max(0.0f, Seconds));
		}
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	const TCHAR* PieceLetter(EKGTableGame Game, uint8 Type)
	{
		if (Game == EKGTableGame::Draughts)
		{
			return Type == KGPiece::DKing ? TEXT("K") : TEXT("");
		}
		static const TCHAR* Letters[7] = {TEXT(""), TEXT("P"), TEXT("N"), TEXT("B"), TEXT("R"), TEXT("Q"), TEXT("K")};
		return Letters[Type & 7];
	}

	FText ResultText(const FKGTableView& V)
	{
		const FText Winner = V.Status == EKGTableStatus::WhiteWins ? FText::FromString(V.Names[0]) : FText::FromString(V.Names[1]);
		switch (V.Reason)
		{
		case EKGTableEndReason::Checkmate: return FText::Format(LOCTEXT("Mate", "Checkmate! {0} wins"), Winner);
		case EKGTableEndReason::Stalemate: return LOCTEXT("Stalemate", "Stalemate: a draw");
		case EKGTableEndReason::NoMoves: return FText::Format(LOCTEXT("NoMoves", "No moves left: {0} wins"), Winner);
		case EKGTableEndReason::Resign: return FText::Format(LOCTEXT("Resign", "Resigned: {0} wins"), Winner);
		case EKGTableEndReason::Flag: return V.Status == EKGTableStatus::Draw ? LOCTEXT("FlagDraw", "Time ran out: a draw")
		                                                                    : FText::Format(LOCTEXT("Flag", "Time ran out: {0} wins"), Winner);
		case EKGTableEndReason::Repetition: return LOCTEXT("Repetition", "Threefold repetition: a draw");
		case EKGTableEndReason::FiftyMove: return LOCTEXT("FiftyMove", "No progress: a draw");
		case EKGTableEndReason::Material: return LOCTEXT("Material", "Nothing left to mate with: a draw");
		case EKGTableEndReason::Agreement: return LOCTEXT("Agreement", "Draw agreed");
		default: return LOCTEXT("Over", "Game over");
		}
	}
}

void SKGTablePanel::Construct(const FArguments& InArgs)
{
	View = InArgs._View.IsValid() ? InArgs._View : MakeShared<FKGTableView>();
	OnMove = InArgs._OnMove;
	OnAction = InArgs._OnAction;
	SetCanTick(false);
}

SKGTablePanel::FLayout SKGTablePanel::Layout(const FGeometry& G) const
{
	using namespace KGTablePanelPrivate;
	FLayout L;
	const FVector2f Size = G.GetLocalSize();
	L.S = FMath::Clamp(FMath::Min(Size.Y / 1080.0f, Size.X / 1920.0f) * 1.25f, 0.55f, 1.6f);
	L.Cell = CellPx * L.S;
	const float Board = 8.0f * L.Cell;
	L.CardSize = FVector2f(Board + 2.0f * Pad * L.S, (Pad + HeaderH) * L.S + Board + (FooterH + Pad) * L.S);
	L.Card = FVector2f((Size.X - L.CardSize.X) * 0.5f, (Size.Y - L.CardSize.Y) * 0.5f);
	L.BoardPos = L.Card + FVector2f(Pad * L.S, (Pad + HeaderH) * L.S);
	L.ButtonSize = FVector2f(96.0f * L.S, 34.0f * L.S);
	const float FooterY = L.BoardPos.Y + Board + 16.0f * L.S;
	L.ResignButton = FVector2f(L.Card.X + L.CardSize.X - Pad * L.S - L.ButtonSize.X, FooterY);
	L.DrawButton = L.ResignButton - FVector2f(L.ButtonSize.X + 8.0f * L.S, 0.0f);
	L.RematchButton = L.ResignButton;
	L.bBottomWhite = View->MyColour != 1;
	return L;
}

FVector2f SKGTablePanel::SquareOrigin(const FLayout& L, int32 Sq) const
{
	const int32 File = KGPiece::FileOf(Sq);
	const int32 Rank = KGPiece::RankOf(Sq);
	const int32 Col = L.bBottomWhite ? File : 7 - File;
	const int32 Row = L.bBottomWhite ? 7 - Rank : Rank;
	return L.BoardPos + FVector2f(Col * L.Cell, Row * L.Cell);
}

int32 SKGTablePanel::SquareAt(const FLayout& L, const FVector2f& P) const
{
	const FVector2f Rel = P - L.BoardPos;
	if (Rel.X < 0.0f || Rel.Y < 0.0f || Rel.X >= 8.0f * L.Cell || Rel.Y >= 8.0f * L.Cell)
	{
		return -1;
	}
	const int32 Col = static_cast<int32>(Rel.X / L.Cell);
	const int32 Row = static_cast<int32>(Rel.Y / L.Cell);
	const int32 File = L.bBottomWhite ? Col : 7 - Col;
	const int32 Rank = L.bBottomWhite ? 7 - Row : Row;
	return KGPiece::SquareAt(File, Rank);
}

bool SKGTablePanel::CanPlay() const
{
	return View->MyColour >= 0 && View->Status == EKGTableStatus::Playing && View->Board.Side == View->MyColour && View->bOpen;
}

void SKGTablePanel::Select(int32 Sq)
{
	SelectedSq = Sq;
	LegalFromSelected.Reset();
	PromoFrom = PromoTo = -1;
	if (Sq < 0)
	{
		return;
	}
	TArray<FKGTableMove> All;
	FKGTableRules::Generate(View->Board, All);
	for (const FKGTableMove& M : All)
	{
		if (M.From == Sq)
		{
			LegalFromSelected.Add(M);
		}
	}
	if (LegalFromSelected.Num() == 0)
	{
		SelectedSq = -1;
	}
}

void SKGTablePanel::DebugSelect(int32 Sq)
{
	Select(Sq);
}

void SKGTablePanel::ClickSquare(int32 Sq)
{
	const uint8 P = View->Board.Sq[Sq];
	if (SelectedSq >= 0)
	{
		const FKGTableMove* Found = nullptr;
		int32 Matches = 0;
		for (const FKGTableMove& M : LegalFromSelected)
		{
			if (M.To == Sq)
			{
				Found = Found ? Found : &M;
				++Matches;
			}
		}
		if (Found)
		{
			if (View->Board.Game == EKGTableGame::Chess && Matches > 1)
			{
				PromoFrom = SelectedSq;   // four promotions end on this square: ask
				PromoTo = Sq;
				return;
			}
			OnMove.ExecuteIfBound(Found->ToString());
			Select(-1);
			return;
		}
	}
	if (P && KGPiece::ColourOf(P) == View->MyColour)
	{
		Select(Sq == SelectedSq ? -1 : Sq);
	}
	else
	{
		Select(-1);
	}
}

FReply SKGTablePanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace KGTablePanelPrivate;
	if (View->bStrip)
	{
		return FReply::Unhandled();
	}
	const FLayout L = Layout(MyGeometry);
	const FVector2f P = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	const bool bInside = P.X >= L.Card.X && P.Y >= L.Card.Y && P.X <= L.Card.X + L.CardSize.X && P.Y <= L.Card.Y + L.CardSize.Y;
	if (!bInside)
	{
		return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		Select(-1);
		return FReply::Handled();
	}
	auto Hit = [&](const FVector2f& Pos, const FVector2f& Size)
	{
		return P.X >= Pos.X && P.Y >= Pos.Y && P.X <= Pos.X + Size.X && P.Y <= Pos.Y + Size.Y;
	};
	const bool bOver = View->Status != EKGTableStatus::Playing && View->Status != EKGTableStatus::Adjourned && View->Status != EKGTableStatus::Waiting;
	if (View->MyColour >= 0)
	{
		if (bOver && Hit(L.RematchButton, L.ButtonSize))
		{
			OnAction.ExecuteIfBound(2);
			return FReply::Handled();
		}
		if (!bOver && Hit(L.ResignButton, L.ButtonSize))
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - ResignArmedAt < PromoPause)
			{
				ResignArmedAt = -100.0;
				OnAction.ExecuteIfBound(0);
			}
			else
			{
				ResignArmedAt = Now;
			}
			return FReply::Handled();
		}
		if (!bOver && Hit(L.DrawButton, L.ButtonSize))
		{
			OnAction.ExecuteIfBound(1);
			return FReply::Handled();
		}
	}
	if (PromoFrom >= 0)
	{
		// The chooser: four discs centred on the board.
		const FVector2f Centre = L.BoardPos + FVector2f(4.0f * L.Cell, 4.0f * L.Cell);
		static const uint8 Promos[4] = {KGPiece::Queen, KGPiece::Rook, KGPiece::Bishop, KGPiece::Knight};
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2f C = Centre + FVector2f((i - 1.5f) * L.Cell * 1.2f, 0.0f);
			if (FVector2f::Distance(P, C) <= L.Cell * 0.5f)
			{
				FKGTableMove M;
				M.From = static_cast<uint8>(PromoFrom);
				M.To = static_cast<uint8>(PromoTo);
				M.Promo = Promos[i];
				OnMove.ExecuteIfBound(M.ToString());
				Select(-1);
				return FReply::Handled();
			}
		}
		Select(-1);
		return FReply::Handled();
	}
	const int32 Sq = SquareAt(L, P);
	if (Sq >= 0 && CanPlay())
	{
		ClickSquare(Sq);
	}
	return FReply::Handled();
}

void SKGTablePanel::PaintStrip(const FGeometry& G, FSlateWindowElementList& Out, int32& Layer) const
{
	using namespace KGTablePanelPrivate;
	const FKGMenuStyle& St = FKGMenuStyle::Get();
	const FVector2f Size = G.GetLocalSize();
	const float S = FMath::Clamp(Size.Y / 1080.0f, 0.6f, 1.5f);
	const FVector2f Pill(380.0f * S, 40.0f * S);
	const FVector2f Pos((Size.X - Pill.X) * 0.5f, 22.0f * S);
	Box(Out, Layer++, G, Pos, Pill, KGUI::Color(KGUI::Night, KGUI::ChipAlpha), -1.0f, KGUI::Color(KGUI::Gold, 0.35f), 1.0f);
	const FSlateFontInfo Font = FKGMenuStyle::Font(TEXT("Bold"), 15.0f * S);
	const FSlateFontInfo Small = FKGMenuStyle::Font(TEXT("Medium"), 12.0f * S);
	const bool bPlaying = View->Status == EKGTableStatus::Playing;
	const float Y = Pos.Y + 10.0f * S;
	const FLinearColor White = (bPlaying && View->Board.Side == 0) ? St.Gold : St.CreamDim;
	const FLinearColor Black = (bPlaying && View->Board.Side == 1) ? St.Gold : St.CreamDim;
	const FString Left = FString::Printf(TEXT("%s %s"), *View->Names[0].Left(12), View->bTimed ? *ClockText(View->Clock[0]) : TEXT(""));
	const FString Right = FString::Printf(TEXT("%s %s"), View->bTimed ? *ClockText(View->Clock[1]) : TEXT(""), *View->Names[1].Left(12));
	Text(Out, Layer, G, FVector2f(Pos.X + 18.0f * S, Y), Left, Font, White, 0.0f);
	Text(Out, Layer, G, FVector2f(Pos.X + Pill.X - 18.0f * S, Y), Right, Font, Black, 1.0f);
	Circle(Out, Layer, G, FVector2f(Pos.X + Pill.X * 0.5f, Pos.Y + Pill.Y * 0.5f), 4.0f * S, bPlaying ? St.Gold : St.Muted);
	const FString Status = bPlaying ? (View->Board.Game == EKGTableGame::Chess ? TEXT("chess") : TEXT("draughts"))
	                                : FString(FKGTableRules::StatusText(View->Status)).ToLower();
	Text(Out, Layer, G, FVector2f(Pos.X + Pill.X * 0.5f, Pos.Y + Pill.Y + 2.0f * S), Status, Small, St.Muted, 0.5f);
	++Layer;
}

int32 SKGTablePanel::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace KGTablePanelPrivate;
	int32 Layer = LayerId + 1;
	if (View->bStrip)
	{
		PaintStrip(G, Out, Layer);
		return Layer;
	}
	// The position moved under us (opponent's move, new game): drop a stale selection.
	if (SeenPly != View->Board.Ply)
	{
		SeenPly = View->Board.Ply;
		const_cast<SKGTablePanel*>(this)->Select(-1);
	}
	const FKGMenuStyle& St = FKGMenuStyle::Get();
	const FLayout L = Layout(G);
	const float S = L.S;
	const double Now = FPlatformTime::Seconds();
	const FKGTableView& V = *View;
	const bool bChess = V.Board.Game == EKGTableGame::Chess;

	// Card
	Box(Out, Layer++, G, L.Card + FVector2f(0.0f, 6.0f * S), L.CardSize, FLinearColor(0, 0, 0, 0.35f), KGUI::RadiusCard * S);
	Box(Out, Layer++, G, L.Card, L.CardSize, KGUI::Color(KGUI::Night, 0.94f), KGUI::RadiusCard * S, KGUI::Color(KGUI::Gold, 0.25f), 1.0f);

	// Header: place / game / clock preset, then the two clock chips (bottom player left, top player right).
	const FSlateFontInfo Caption = FKGMenuStyle::Font(TEXT("Bold"), 12.0f * S, 2);
	const FSlateFontInfo Title = FKGMenuStyle::Font(TEXT("Black"), 22.0f * S);
	const FSlateFontInfo Body = FKGMenuStyle::Font(TEXT("Bold"), 16.0f * S);
	const FSlateFontInfo Small = FKGMenuStyle::Font(TEXT("Medium"), 13.0f * S);
	const FSlateFontInfo Clock = FKGMenuStyle::Font(TEXT("Black"), 20.0f * S);
	const FVector2f H = L.Card + FVector2f(Pad * S, Pad * S * 0.8f);
	const FString Head = FString::Printf(TEXT("%s  ·  %s"), *V.Place.ToUpper(), V.bTimed ? *V.ClockName.ToUpper() : TEXT("UNTIMED"));
	Text(Out, Layer, G, H, Head, Caption, St.Muted);
	Text(Out, Layer, G, H + FVector2f(0.0f, 16.0f * S), bChess ? LOCTEXT("Chess", "Chess").ToString() : LOCTEXT("Draughts", "Draughts").ToString(), Title, St.Cream);
	const int32 Bottom = L.bBottomWhite ? 0 : 1;
	const int32 Top = 1 - Bottom;
	const FVector2f ChipSize(150.0f * S, 30.0f * S);
	const float ChipY = H.Y + 50.0f * S;
	const bool bPlaying = V.Status == EKGTableStatus::Playing;
	auto Chip = [&](int32 Colour, const FVector2f& Pos, float Align)
	{
		const bool bToMove = bPlaying && V.Board.Side == Colour;
		Box(Out, Layer, G, Pos, ChipSize, bToMove ? St.Gold : KGUI::Color(KGUI::Panel), KGUI::RadiusKey * S);
		const FLinearColor TextColour = bToMove ? St.Ink : St.CreamDim;
		FString Name = V.Names[Colour].Left(14);
		if (Colour == V.MyColour)
		{
			Name = LOCTEXT("You", "You").ToString();
		}
		const FString ClockStr = V.bTimed ? ClockText(V.Clock[Colour]) : FString();
		if (Align < 0.5f)
		{
			Text(Out, Layer + 1, G, Pos + FVector2f(12.0f * S, 4.0f * S), Name, Body, TextColour, 0.0f);
			Text(Out, Layer + 1, G, Pos + FVector2f(ChipSize.X - 12.0f * S, 2.0f * S), ClockStr, Clock, TextColour, 1.0f);
		}
		else
		{
			Text(Out, Layer + 1, G, Pos + FVector2f(12.0f * S, 2.0f * S), ClockStr, Clock, TextColour, 0.0f);
			Text(Out, Layer + 1, G, Pos + FVector2f(ChipSize.X - 12.0f * S, 4.0f * S), Name, Body, TextColour, 1.0f);
		}
	};
	Chip(Bottom, FVector2f(L.BoardPos.X, ChipY), 0.0f);
	Chip(Top, FVector2f(L.BoardPos.X + 8.0f * L.Cell - ChipSize.X, ChipY), 1.0f);
	Circle(Out, Layer, G, FVector2f(L.BoardPos.X + 4.0f * L.Cell, ChipY + ChipSize.Y * 0.5f), 4.0f * S, bPlaying ? St.Gold : St.Muted);
	Layer += 2;

	// Board
	const FLinearColor Light = KGUI::Color(0xE8D8B4);
	const FLinearColor Dark = KGUI::Color(0x8B6B4A);
	const int32 CheckSq = (bChess && bPlaying && FKGChessRules::InCheck(V.Board)) ? FKGChessRules::KingSquare(V.Board, V.Board.Side) : -1;
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		const FVector2f O = SquareOrigin(L, Sq);
		const bool bDark = ((KGPiece::FileOf(Sq) + KGPiece::RankOf(Sq)) % 2) == 0;
		Box(Out, Layer, G, O, FVector2f(L.Cell, L.Cell), bDark ? Dark : Light, 0.0f);
		if (Sq == V.LastFrom || Sq == V.LastTo)
		{
			Box(Out, Layer + 1, G, O, FVector2f(L.Cell, L.Cell), KGUI::Color(KGUI::Gold, 0.38f), 0.0f);
		}
		if (Sq == SelectedSq)
		{
			Box(Out, Layer + 1, G, O, FVector2f(L.Cell, L.Cell), FLinearColor::Transparent, 0.0f, St.Gold, 3.0f * S);
		}
	}
	Layer += 2;
	// Coordinates
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 File = L.bBottomWhite ? i : 7 - i;
		const int32 Rank = L.bBottomWhite ? 7 - i : i;
		Text(Out, Layer, G, L.BoardPos + FVector2f((i + 1) * L.Cell - 3.0f * S, 7.0f * L.Cell + L.Cell - 15.0f * S),
		     FString::Chr(TCHAR('a' + File)), Small, KGUI::Color(KGUI::Ink, 0.55f), 1.0f);
		Text(Out, Layer, G, L.BoardPos + FVector2f(3.0f * S, i * L.Cell + 1.0f * S), FString::FromInt(Rank + 1), Small,
		     KGUI::Color(KGUI::Ink, 0.55f), 0.0f);
	}
	++Layer;
	// Pieces
	const FSlateFontInfo PieceFont = FKGMenuStyle::Font(TEXT("Black"), 19.0f * S);
	const float R = L.Cell * 0.37f;
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		const uint8 P = V.Board.Sq[Sq];
		if (!P)
		{
			continue;
		}
		const FVector2f C = SquareOrigin(L, Sq) + FVector2f(L.Cell * 0.5f, L.Cell * 0.5f);
		const bool bBlack = KGPiece::IsBlack(P);
		Circle(Out, Layer, G, C + FVector2f(0.0f, 2.0f * S), R, FLinearColor(0, 0, 0, 0.35f));
		Circle(Out, Layer + 1, G, C, R, bBlack ? St.Ink : St.Cream, bBlack ? St.Cream : St.Ink, 1.5f * S);
		if (!bChess && KGPiece::TypeOf(P) == KGPiece::DKing)
		{
			Circle(Out, Layer + 2, G, C, R * 0.72f, FLinearColor::Transparent, St.Gold, 2.0f * S);
		}
		const FString Letter = PieceLetter(V.Board.Game, KGPiece::TypeOf(P));
		if (!Letter.IsEmpty())
		{
			const FVector2D Sz = Measure(Letter, PieceFont);
			Text(Out, Layer + 3, G, C - FVector2f(0.0f, static_cast<float>(Sz.Y) * 0.5f), Letter, PieceFont, bBlack ? St.Cream : St.Ink, 0.5f);
		}
		if (Sq == CheckSq)
		{
			Circle(Out, Layer + 4, G, C, R + 4.0f * S, FLinearColor::Transparent, St.Crimson, 3.0f * S);
		}
	}
	Layer += 5;
	// Legal-move dots for the selection (a ring on a capture)
	for (const FKGTableMove& M : LegalFromSelected)
	{
		const FVector2f C = SquareOrigin(L, M.To) + FVector2f(L.Cell * 0.5f, L.Cell * 0.5f);
		const bool bCapture = V.Board.Sq[M.To] != 0 || (bChess && M.To == V.Board.Ep && KGPiece::TypeOf(V.Board.Sq[M.From]) == KGPiece::Pawn) ||
		                      (!bChess && M.NumHops > 0 && FMath::Abs(KGPiece::RankOf(M.Hops[0]) - KGPiece::RankOf(M.From)) == 2);
		if (bCapture)
		{
			Circle(Out, Layer, G, C, R + 2.0f * S, FLinearColor::Transparent, KGUI::Color(KGUI::Gold, 0.9f), 3.0f * S);
		}
		else
		{
			Circle(Out, Layer, G, C, 7.0f * S, KGUI::Color(KGUI::Gold, 0.9f));
		}
	}
	++Layer;
	// Promotion chooser
	if (PromoFrom >= 0)
	{
		const FVector2f Centre = L.BoardPos + FVector2f(4.0f * L.Cell, 4.0f * L.Cell);
		const FVector2f PillSize(L.Cell * 5.2f, L.Cell * 1.4f);
		Box(Out, Layer, G, Centre - PillSize * 0.5f, PillSize, KGUI::Color(KGUI::Night, 0.96f), -1.0f, St.Gold, 1.5f * S);
		static const uint8 Promos[4] = {KGPiece::Queen, KGPiece::Rook, KGPiece::Bishop, KGPiece::Knight};
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2f C = Centre + FVector2f((i - 1.5f) * L.Cell * 1.2f, 0.0f);
			const bool bBlack = V.MyColour == 1;
			Circle(Out, Layer + 1, G, C, R, bBlack ? St.Ink : St.Cream, St.Gold, 2.0f * S);
			const FString Letter = PieceLetter(EKGTableGame::Chess, Promos[i]);
			const FVector2D Sz = Measure(Letter, PieceFont);
			Text(Out, Layer + 2, G, C - FVector2f(0.0f, static_cast<float>(Sz.Y) * 0.5f), Letter, PieceFont, bBlack ? St.Cream : St.Ink, 0.5f);
		}
		Layer += 3;
	}

	// Footer: hint on the left, buttons on the right.
	const float FooterY = L.BoardPos.Y + 8.0f * L.Cell + 16.0f * S;
	const bool bOver = !bPlaying && V.Status != EKGTableStatus::Adjourned && V.Status != EKGTableStatus::Waiting;
	FString Hint;
	if (V.MyColour < 0)
	{
		Hint = LOCTEXT("HintWatch", "Watching").ToString();
	}
	else if (bOver)
	{
		Hint = LOCTEXT("HintAgain", "Stand up (E) or ask for another game").ToString();
	}
	else if (PromoFrom >= 0)
	{
		Hint = LOCTEXT("HintPromo", "Choose the new piece").ToString();
	}
	else if (V.Status == EKGTableStatus::Playing && V.Board.Side == V.MyColour)
	{
		Hint = LOCTEXT("HintYourMove", "Your move · click a piece, then a square · E: stand up").ToString();
	}
	else
	{
		Hint = LOCTEXT("HintWait", "Opponent is thinking · E: stand up").ToString();
	}
	Text(Out, Layer, G, FVector2f(L.BoardPos.X, FooterY + 8.0f * S), Hint, Small, St.CreamDim);
	if (V.MyColour >= 0)
	{
		auto Button = [&](const FVector2f& Pos, const FString& Label, const FLinearColor& Fill, const FLinearColor& TextColour)
		{
			Box(Out, Layer, G, Pos, L.ButtonSize, Fill, KGUI::RadiusKey * S, KGUI::Color(KGUI::Gold, 0.35f), 1.0f);
			Text(Out, Layer + 1, G, Pos + FVector2f(L.ButtonSize.X * 0.5f, 8.0f * S), Label, Body, TextColour, 0.5f);
		};
		if (bOver)
		{
			Button(L.RematchButton, LOCTEXT("Rematch", "Again").ToString(), St.Gold, St.Ink);
		}
		else
		{
			const bool bOffered = (V.DrawOffers & (1 << V.MyColour)) != 0;
			const bool bTheirs = (V.DrawOffers & (1 << (1 - V.MyColour))) != 0;
			Button(L.DrawButton, bTheirs ? LOCTEXT("AcceptDraw", "Accept ½").ToString() : LOCTEXT("Draw", "Draw").ToString(),
			       bTheirs ? St.Gold : KGUI::Color(KGUI::Panel), bTheirs ? St.Ink : (bOffered ? St.Muted : St.Cream));
			const bool bArmed = Now - ResignArmedAt < PromoPause;
			Button(L.ResignButton, bArmed ? LOCTEXT("ResignSure", "Sure?").ToString() : LOCTEXT("Resign", "Resign").ToString(),
			       bArmed ? St.Crimson : KGUI::Color(KGUI::Panel), St.Cream);
		}
	}
	Layer += 2;

	// Banners: result / adjourned / waiting, and the notice toast.
	FString Banner;
	FLinearColor BannerColour = St.Gold;
	if (bOver)
	{
		Banner = ResultText(V).ToString();
	}
	else if (V.Status == EKGTableStatus::Adjourned)
	{
		Banner = (V.bOpen ? LOCTEXT("Adjourned", "Adjourned: waiting for the other player") : LOCTEXT("Closed", "Tables are closed: the village calls")).ToString();
		BannerColour = St.CreamDim;
	}
	else if (V.Status == EKGTableStatus::Waiting)
	{
		Banner = LOCTEXT("Waiting", "Waiting for an opponent to sit down").ToString();
		BannerColour = St.CreamDim;
	}
	if (!Banner.IsEmpty())
	{
		const FVector2D Sz = Measure(Banner, Body);
		const FVector2f BSize(static_cast<float>(Sz.X) + 40.0f * S, 40.0f * S);
		const FVector2f BPos(L.BoardPos.X + 4.0f * L.Cell - BSize.X * 0.5f, L.BoardPos.Y + 4.0f * L.Cell - BSize.Y * 0.5f);
		Box(Out, Layer, G, BPos, BSize, KGUI::Color(KGUI::Night, 0.94f), -1.0f, BannerColour, 1.5f * S);
		Text(Out, Layer + 1, G, BPos + FVector2f(BSize.X * 0.5f, 10.0f * S), Banner, Body, BannerColour, 0.5f);
		Layer += 2;
	}
	if (!V.Notice.IsEmpty() && Now - V.NoticeTime < KGUI::ToastHold)
	{
		const float A = FMath::Clamp(static_cast<float>(KGUI::ToastHold - (Now - V.NoticeTime)) / 0.5f, 0.0f, 1.0f);
		const FVector2D Sz = Measure(V.Notice, Small);
		const FVector2f TSize(static_cast<float>(Sz.X) + 28.0f * S, 28.0f * S);
		const FVector2f TPos(L.BoardPos.X + 4.0f * L.Cell - TSize.X * 0.5f, L.BoardPos.Y - 34.0f * S);
		Box(Out, Layer, G, TPos, TSize, KGUI::Color(KGUI::Crimson, 0.9f * A), -1.0f);
		Text(Out, Layer + 1, G, TPos + FVector2f(TSize.X * 0.5f, 6.0f * S), V.Notice, Small, FLinearColor(1, 1, 1, A), 0.5f);
		Layer += 2;
	}
	return Layer;
}

#undef LOCTEXT_NAMESPACE
