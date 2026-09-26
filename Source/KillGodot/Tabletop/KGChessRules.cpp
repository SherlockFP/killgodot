#include "Tabletop/KGTableRules.h"

// FIDE chess on the shared 8x8 board (Docs/Design/Tabletop_Games.md section 3.4). Copy-make, pseudo-legal
// generation filtered by a king-safety check; perft-verified in Private/Tests/KGTabletopTests.cpp.

namespace KGChessPrivate
{
	using namespace KGPiece;

	constexpr int32 KnightSteps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
	constexpr int32 KingSteps[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
	constexpr int32 BishopDirs[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
	constexpr int32 RookDirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

	// Castling: {right bit, king from, king to, rook from, rook to, squares that must be empty..., squares that must be safe}
	struct FCastle
	{
		uint8 Bit;
		int32 KingFrom, KingTo, RookFrom, RookTo;
		int32 Empty[3];
		int32 NumEmpty;
		int32 Safe[2];   // squares the king passes / lands on (the start square is checked separately)
	};
	constexpr FCastle Castles[4] = {
		{1, 4, 6, 7, 5, {5, 6, -1}, 2, {5, 6}},
		{2, 4, 2, 0, 3, {3, 2, 1}, 3, {3, 2}},
		{4, 60, 62, 63, 61, {61, 62, -1}, 2, {61, 62}},
		{8, 60, 58, 56, 59, {59, 58, 57}, 3, {59, 58}},
	};

	inline void Add(TArray<FKGTableMove>& Out, int32 From, int32 To, uint8 Promo = 0)
	{
		FKGTableMove M;
		M.From = static_cast<uint8>(From);
		M.To = static_cast<uint8>(To);
		M.Promo = Promo;
		Out.Add(M);
	}

	inline void AddPawn(TArray<FKGTableMove>& Out, int32 From, int32 To)
	{
		const int32 R = RankOf(To);
		if (R == 7 || R == 0)
		{
			Add(Out, From, To, Queen);
			Add(Out, From, To, Rook);
			Add(Out, From, To, Bishop);
			Add(Out, From, To, Knight);
		}
		else
		{
			Add(Out, From, To);
		}
	}

	/** Castling-rights mask cleared when a piece leaves or a capture lands on Sq. */
	inline uint8 RightsLostAt(int32 Sq)
	{
		switch (Sq)
		{
		case 0: return 2;
		case 7: return 1;
		case 4: return 3;
		case 56: return 8;
		case 63: return 4;
		case 60: return 12;
		default: return 0;
		}
	}
}

void FKGChessRules::GeneratePseudo(const FKGBoardState& S, TArray<FKGTableMove>& Out)
{
	using namespace KGPiece;
	using namespace KGChessPrivate;
	const uint8 Us = S.Side;
	const uint8 Them = 1 - Us;
	const int32 Forward = Us == 0 ? 1 : -1;
	const int32 StartRank = Us == 0 ? 1 : 6;

	for (int32 From = 0; From < 64; ++From)
	{
		const uint8 P = S.Sq[From];
		if (!P || ColourOf(P) != Us)
		{
			continue;
		}
		const int32 F = FileOf(From);
		const int32 R = RankOf(From);
		switch (TypeOf(P))
		{
		case Pawn:
		{
			const int32 R1 = R + Forward;
			if (R1 >= 0 && R1 < 8)
			{
				const int32 Push = SquareAt(F, R1);
				if (!S.Sq[Push])
				{
					AddPawn(Out, From, Push);
					if (R == StartRank)
					{
						const int32 Push2 = SquareAt(F, R1 + Forward);
						if (!S.Sq[Push2])
						{
							Add(Out, From, Push2);
						}
					}
				}
				for (int32 DF = -1; DF <= 1; DF += 2)
				{
					if (!OnBoard(F + DF, R1))
					{
						continue;
					}
					const int32 Target = SquareAt(F + DF, R1);
					const uint8 T = S.Sq[Target];
					if ((T && ColourOf(T) == Them) || Target == S.Ep)
					{
						AddPawn(Out, From, Target);
					}
				}
			}
			break;
		}
		case Knight:
			for (const auto& D : KnightSteps)
			{
				if (OnBoard(F + D[0], R + D[1]))
				{
					const int32 To = SquareAt(F + D[0], R + D[1]);
					if (!S.Sq[To] || ColourOf(S.Sq[To]) == Them)
					{
						Add(Out, From, To);
					}
				}
			}
			break;
		case King:
			for (const auto& D : KingSteps)
			{
				if (OnBoard(F + D[0], R + D[1]))
				{
					const int32 To = SquareAt(F + D[0], R + D[1]);
					if (!S.Sq[To] || ColourOf(S.Sq[To]) == Them)
					{
						Add(Out, From, To);
					}
				}
			}
			for (const FCastle& C : Castles)
			{
				if (!(S.Castling & C.Bit) || From != C.KingFrom)
				{
					continue;
				}
				if (S.Sq[C.RookFrom] != KGPiece::Make(Rook, Us))
				{
					continue;
				}
				bool bOk = true;
				for (int32 i = 0; i < C.NumEmpty && bOk; ++i)
				{
					bOk = S.Sq[C.Empty[i]] == Empty;
				}
				if (!bOk || IsAttacked(S, C.KingFrom, Them) || IsAttacked(S, C.Safe[0], Them) || IsAttacked(S, C.Safe[1], Them))
				{
					continue;
				}
				Add(Out, From, C.KingTo);
			}
			break;
		default:
		{
			const bool bDiag = TypeOf(P) == Bishop || TypeOf(P) == Queen;
			const bool bOrtho = TypeOf(P) == Rook || TypeOf(P) == Queen;
			auto Slide = [&](const int32 (*Dirs)[2])
			{
				for (int32 D = 0; D < 4; ++D)
				{
					int32 TF = F + Dirs[D][0];
					int32 TR = R + Dirs[D][1];
					while (OnBoard(TF, TR))
					{
						const int32 To = SquareAt(TF, TR);
						if (S.Sq[To])
						{
							if (ColourOf(S.Sq[To]) == Them)
							{
								Add(Out, From, To);
							}
							break;
						}
						Add(Out, From, To);
						TF += Dirs[D][0];
						TR += Dirs[D][1];
					}
				}
			};
			if (bDiag)
			{
				Slide(BishopDirs);
			}
			if (bOrtho)
			{
				Slide(RookDirs);
			}
			break;
		}
		}
	}
}

bool FKGChessRules::IsAttacked(const FKGBoardState& S, int32 Sq, uint8 ByColour)
{
	using namespace KGPiece;
	using namespace KGChessPrivate;
	if (Sq < 0 || Sq > 63)
	{
		return false;
	}
	const int32 F = FileOf(Sq);
	const int32 R = RankOf(Sq);
	// Pawns: an attacking pawn of ByColour stands one rank behind (from its point of view) on an adjacent file.
	const int32 PawnRank = R - (ByColour == 0 ? 1 : -1);
	for (int32 DF = -1; DF <= 1; DF += 2)
	{
		if (OnBoard(F + DF, PawnRank) && S.Sq[SquareAt(F + DF, PawnRank)] == KGPiece::Make(Pawn, ByColour))
		{
			return true;
		}
	}
	for (const auto& D : KnightSteps)
	{
		if (OnBoard(F + D[0], R + D[1]) && S.Sq[SquareAt(F + D[0], R + D[1])] == KGPiece::Make(Knight, ByColour))
		{
			return true;
		}
	}
	for (const auto& D : KingSteps)
	{
		if (OnBoard(F + D[0], R + D[1]) && S.Sq[SquareAt(F + D[0], R + D[1])] == KGPiece::Make(King, ByColour))
		{
			return true;
		}
	}
	auto Ray = [&](const int32 (*Dirs)[2], uint8 SliderA, uint8 SliderB)
	{
		for (int32 D = 0; D < 4; ++D)
		{
			int32 TF = F + Dirs[D][0];
			int32 TR = R + Dirs[D][1];
			while (OnBoard(TF, TR))
			{
				const uint8 P = S.Sq[SquareAt(TF, TR)];
				if (P)
				{
					if (ColourOf(P) == ByColour && (TypeOf(P) == SliderA || TypeOf(P) == SliderB))
					{
						return true;
					}
					break;
				}
				TF += Dirs[D][0];
				TR += Dirs[D][1];
			}
		}
		return false;
	};
	return Ray(BishopDirs, Bishop, Queen) || Ray(RookDirs, Rook, Queen);
}

int32 FKGChessRules::KingSquare(const FKGBoardState& S, uint8 Colour)
{
	const uint8 K = KGPiece::Make(KGPiece::King, Colour);
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		if (S.Sq[Sq] == K)
		{
			return Sq;
		}
	}
	return -1;
}

void FKGChessRules::Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out)
{
	Out.Reset();
	TArray<FKGTableMove> Pseudo;
	Pseudo.Reserve(64);
	GeneratePseudo(S, Pseudo);
	const uint8 Us = S.Side;
	for (const FKGTableMove& M : Pseudo)
	{
		FKGBoardState Next = S;
		Make(Next, M);
		if (!IsAttacked(Next, KingSquare(Next, Us), 1 - Us))
		{
			Out.Add(M);
		}
	}
}

void FKGChessRules::Make(FKGBoardState& S, const FKGTableMove& M)
{
	using namespace KGPiece;
	using namespace KGChessPrivate;
	const uint8 P = S.Sq[M.From];
	const uint8 Us = ColourOf(P);
	const uint8 Type = TypeOf(P);
	const int32 OldEp = S.Ep;
	bool bIrreversible = S.Sq[M.To] != Empty || Type == Pawn;
	S.Ep = -1;

	if (Type == Pawn)
	{
		if (M.To == OldEp && FileOf(M.From) != FileOf(M.To))
		{
			// en passant: the captured pawn stands beside the origin, on the destination file
			S.Sq[SquareAt(FileOf(M.To), RankOf(M.From))] = Empty;
		}
		else if (FMath::Abs(RankOf(M.To) - RankOf(M.From)) == 2)
		{
			S.Ep = static_cast<int8>((M.From + M.To) / 2);
		}
	}
	else if (Type == King && FMath::Abs(FileOf(M.To) - FileOf(M.From)) == 2)
	{
		for (const FCastle& C : Castles)
		{
			if (C.KingFrom == M.From && C.KingTo == M.To)
			{
				S.Sq[C.RookTo] = S.Sq[C.RookFrom];
				S.Sq[C.RookFrom] = Empty;
			}
		}
	}

	S.Castling &= ~(RightsLostAt(M.From) | RightsLostAt(M.To));
	S.Sq[M.To] = (Type == Pawn && M.Promo) ? KGPiece::Make(M.Promo, Us) : P;
	S.Sq[M.From] = Empty;
	S.HalfClock = bIrreversible ? 0 : static_cast<uint16>(S.HalfClock + 1);
	S.Side = 1 - Us;
	++S.Ply;
}

bool FKGChessRules::InsufficientMaterial(const FKGBoardState& S)
{
	using namespace KGPiece;
	int32 Minors[2] = {0, 0};
	int32 Knights = 0;
	int32 BishopColours = 0;   // bit 1 light squares, bit 2 dark squares
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		const uint8 P = S.Sq[Sq];
		switch (TypeOf(P))
		{
		case Empty:
		case King:
			break;
		case Knight:
			++Knights;
			++Minors[ColourOf(P)];
			break;
		case Bishop:
			BishopColours |= ((FileOf(Sq) + RankOf(Sq)) & 1) ? 1 : 2;
			++Minors[ColourOf(P)];
			break;
		default:
			return false;   // pawn, rook or queen: mate is possible
		}
	}
	const int32 Total = Minors[0] + Minors[1];
	if (Total <= 1)
	{
		return true;   // K vs K, K+B vs K, K+N vs K
	}
	// Bishops only, all on the same square colour (any number, both sides)
	return Knights == 0 && (BishopColours == 1 || BishopColours == 2);
}

bool FKGChessRules::HasMatingMaterial(const FKGBoardState& S, uint8 Colour)
{
	using namespace KGPiece;
	int32 Minors = 0;
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		const uint8 P = S.Sq[Sq];
		if (!P || ColourOf(P) != Colour)
		{
			continue;
		}
		switch (TypeOf(P))
		{
		case Pawn:
		case Rook:
		case Queen:
			return true;
		case Knight:
		case Bishop:
			++Minors;
			break;
		default:
			break;
		}
	}
	return Minors >= 2;
}

int64 FKGChessRules::Perft(const FKGBoardState& S, int32 Depth)
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
