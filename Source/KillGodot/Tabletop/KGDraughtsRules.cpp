#include "Tabletop/KGTableRules.h"

// English draughts / checkers (Docs/Design/Tabletop_Games.md section 3.5): dark squares only, men move one square
// diagonally forward, capture by jumping; capture is mandatory and a multi-jump must be completed (the player chooses
// among branches); a man reaching the far rank is crowned and the move ends there; kings move one square in all four
// diagonal directions (no flying kings). One FKGTableMove = the whole jump sequence (perft counts sequences).

namespace KGDraughtsPrivate
{
	using namespace KGPiece;

	constexpr int32 Dirs[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};   // first two: up the board (white forward)

	inline bool MayMove(uint8 Piece, int32 Dir)
	{
		if (TypeOf(Piece) == DKing)
		{
			return true;
		}
		return IsBlack(Piece) ? Dir >= 2 : Dir < 2;
	}

	inline bool Crowns(uint8 Piece, int32 Sq)
	{
		return TypeOf(Piece) == Man && RankOf(Sq) == (IsBlack(Piece) ? 0 : 7);
	}
}

void FKGDraughtsRules::Jumps(const FKGBoardState& S, int32 From, int32 Start, uint8 Piece, uint64 Captured,
                             FKGTableMove& Cur, TArray<FKGTableMove>& Out)
{
	using namespace KGPiece;
	using namespace KGDraughtsPrivate;
	const uint8 Them = 1 - ColourOf(Piece);
	const int32 F = FileOf(Start);
	const int32 R = RankOf(Start);
	bool bAny = false;
	for (int32 D = 0; D < 4; ++D)
	{
		if (!MayMove(Piece, D) || !OnBoard(F + 2 * Dirs[D][0], R + 2 * Dirs[D][1]))
		{
			continue;
		}
		const int32 Over = SquareAt(F + Dirs[D][0], R + Dirs[D][1]);
		const int32 Land = SquareAt(F + 2 * Dirs[D][0], R + 2 * Dirs[D][1]);
		const uint8 O = S.Sq[Over];
		if (!O || ColourOf(O) != Them || (Captured & (1ull << Over)))
		{
			continue;
		}
		if (S.Sq[Land] && Land != From)
		{
			continue;   // the origin is empty while the piece is in the air; everything else blocks
		}
		if (Cur.NumHops >= UE_ARRAY_COUNT(Cur.Hops))
		{
			continue;
		}
		bAny = true;
		Cur.Hops[Cur.NumHops++] = static_cast<uint8>(Land);
		if (Crowns(Piece, Land))
		{
			Cur.To = static_cast<uint8>(Land);
			Out.Add(Cur);   // crowning ends the move
		}
		else
		{
			Jumps(S, From, Land, Piece, Captured | (1ull << Over), Cur, Out);
		}
		--Cur.NumHops;
	}
	if (!bAny && Cur.NumHops > 0)
	{
		Cur.To = Cur.Hops[Cur.NumHops - 1];
		Out.Add(Cur);
	}
}

void FKGDraughtsRules::Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out)
{
	using namespace KGPiece;
	using namespace KGDraughtsPrivate;
	Out.Reset();
	const uint8 Us = S.Side;
	// Captures first: if any exist, only captures are legal.
	for (int32 From = 0; From < 64; ++From)
	{
		const uint8 P = S.Sq[From];
		if (!P || ColourOf(P) != Us)
		{
			continue;
		}
		FKGTableMove Cur;
		Cur.From = static_cast<uint8>(From);
		Jumps(S, From, From, P, 0, Cur, Out);
	}
	if (Out.Num() > 0)
	{
		return;
	}
	for (int32 From = 0; From < 64; ++From)
	{
		const uint8 P = S.Sq[From];
		if (!P || ColourOf(P) != Us)
		{
			continue;
		}
		const int32 F = FileOf(From);
		const int32 R = RankOf(From);
		for (int32 D = 0; D < 4; ++D)
		{
			if (!MayMove(P, D) || !OnBoard(F + Dirs[D][0], R + Dirs[D][1]))
			{
				continue;
			}
			const int32 To = SquareAt(F + Dirs[D][0], R + Dirs[D][1]);
			if (S.Sq[To])
			{
				continue;
			}
			FKGTableMove M;
			M.From = static_cast<uint8>(From);
			M.To = static_cast<uint8>(To);
			M.NumHops = 1;
			M.Hops[0] = static_cast<uint8>(To);
			Out.Add(M);
		}
	}
}

void FKGDraughtsRules::Make(FKGBoardState& S, const FKGTableMove& M)
{
	using namespace KGPiece;
	using namespace KGDraughtsPrivate;
	uint8 P = S.Sq[M.From];
	S.Sq[M.From] = Empty;
	int32 At = M.From;
	bool bCapture = false;
	for (int32 i = 0; i < M.NumHops; ++i)
	{
		const int32 Next = M.Hops[i];
		if (FMath::Abs(RankOf(Next) - RankOf(At)) == 2)
		{
			S.Sq[(At + Next) / 2] = Empty;
			bCapture = true;
		}
		At = Next;
	}
	if (M.NumHops == 0)
	{
		At = M.To;   // tolerate a bare From/To
	}
	const bool bManMove = TypeOf(P) == Man;
	if (Crowns(P, At))
	{
		P = KGPiece::Make(DKing, ColourOf(P));
	}
	S.Sq[At] = P;
	S.HalfClock = (bCapture || bManMove) ? 0 : static_cast<uint16>(S.HalfClock + 1);
	S.Side = 1 - S.Side;
	++S.Ply;
}

int64 FKGDraughtsRules::Perft(const FKGBoardState& S, int32 Depth)
{
	if (Depth <= 0)
	{
		return 1;
	}
	TArray<FKGTableMove> Moves;
	Generate(S, Moves);
	if (Depth == 1)
	{
		return Moves.Num();
	}
	int64 Nodes = 0;
	for (const FKGTableMove& M : Moves)
	{
		FKGBoardState Next = S;
		Make(Next, M);
		Nodes += Perft(Next, Depth - 1);
	}
	return Nodes;
}
