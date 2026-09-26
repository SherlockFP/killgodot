#include "Tabletop/KGTableBot.h"

#include "Tabletop/KGTableRules.h"

namespace KGTableBotPrivate
{
	using namespace KGPiece;

	constexpr int32 ChessValue[7] = {0, 100, 320, 330, 500, 900, 0};

	/** Chebyshev distance from the board centre, doubled: 1 (d4/e5...) .. 7 (corners). */
	inline int32 CentreDist(int32 Sq)
	{
		return FMath::Max(FMath::Abs(2 * FileOf(Sq) - 7), FMath::Abs(2 * RankOf(Sq) - 7));
	}

	int32 ChessPiece(uint8 P, int32 Sq, bool bEndgame)
	{
		const uint8 Colour = ColourOf(P);
		const int32 Rank = Colour == 0 ? RankOf(Sq) : 7 - RankOf(Sq);   // 0 = own back rank
		const int32 File = FileOf(Sq);
		const int32 CD = CentreDist(Sq);
		switch (TypeOf(P))
		{
		case Pawn:
		{
			int32 V = 100 + (Rank - 1) * 4 + (bEndgame ? (Rank - 1) * 6 : 0);
			if ((File == 3 || File == 4) && Rank >= 2 && Rank <= 4)
			{
				V += 8;
			}
			return V;
		}
		case Knight: return 320 + (7 - CD) * 5 - (Rank == 0 ? 8 : 0);
		case Bishop: return 330 + (7 - CD) * 3 - (Rank == 0 ? 6 : 0);
		case Rook: return 500 + (Rank == 6 ? 20 : 0);
		case Queen: return 900 + (7 - CD) * 2;
		case King:
			if (bEndgame)
			{
				return (7 - CD) * 8;
			}
			return (Rank == 0 ? 20 : Rank == 1 ? 5 : -15) + ((File <= 2 || File >= 6) ? 10 : 0);
		default: return 0;
		}
	}

	int32 DraughtsPiece(uint8 P, int32 Sq)
	{
		const uint8 Colour = ColourOf(P);
		const int32 Rank = Colour == 0 ? RankOf(Sq) : 7 - RankOf(Sq);
		const int32 CD = CentreDist(Sq);
		if (TypeOf(P) == DKing)
		{
			return 160 + (7 - CD) * 2;
		}
		return 100 + Rank * 3 + (Rank == 0 ? 5 : 0) + (CD <= 3 ? 4 : 0);
	}
}

int32 FKGTableBot::Evaluate(const FKGBoardState& S)
{
	using namespace KGPiece;
	using namespace KGTableBotPrivate;
	int32 Score[2] = {0, 0};
	if (S.Game == EKGTableGame::Chess)
	{
		int32 Material = 0;
		bool bQueens = false;
		for (int32 Sq = 0; Sq < 64; ++Sq)
		{
			const uint8 P = S.Sq[Sq];
			if (P)
			{
				Material += ChessValue[TypeOf(P)];
				bQueens |= TypeOf(P) == Queen;
			}
		}
		const bool bEndgame = !bQueens || Material < 2600;
		for (int32 Sq = 0; Sq < 64; ++Sq)
		{
			const uint8 P = S.Sq[Sq];
			if (P)
			{
				Score[ColourOf(P)] += ChessPiece(P, Sq, bEndgame);
			}
		}
	}
	else
	{
		for (int32 Sq = 0; Sq < 64; ++Sq)
		{
			const uint8 P = S.Sq[Sq];
			if (P)
			{
				Score[ColourOf(P)] += DraughtsPiece(P, Sq);
			}
		}
	}
	return Score[S.Side] - Score[1 - S.Side];
}

bool FKGTableBot::IsCapture(const FKGBoardState& S, const FKGTableMove& M)
{
	if (S.Game == EKGTableGame::Chess)
	{
		return S.Sq[M.To] != KGPiece::Empty || (KGPiece::TypeOf(S.Sq[M.From]) == KGPiece::Pawn && M.To == S.Ep);
	}
	return M.NumHops > 0 && FMath::Abs(KGPiece::RankOf(M.Hops[0]) - KGPiece::RankOf(M.From)) == 2;
}

int32 FKGTableBot::CaptureValue(const FKGBoardState& S, const FKGTableMove& M)
{
	using namespace KGPiece;
	if (S.Game == EKGTableGame::Chess)
	{
		const int32 Victim = S.Sq[M.To] ? KGTableBotPrivate::ChessValue[TypeOf(S.Sq[M.To])] : (M.To == S.Ep && TypeOf(S.Sq[M.From]) == Pawn ? 100 : 0);
		const int32 Attacker = KGTableBotPrivate::ChessValue[TypeOf(S.Sq[M.From])];
		return Victim > 0 ? Victim * 10 - Attacker / 10 + (M.Promo == Queen ? 800 : 0) : (M.Promo == Queen ? 800 : 0);
	}
	int32 Jumps = 0;
	int32 At = M.From;
	for (int32 i = 0; i < M.NumHops; ++i)
	{
		Jumps += FMath::Abs(RankOf(M.Hops[i]) - RankOf(At)) == 2 ? 1 : 0;
		At = M.Hops[i];
	}
	return Jumps * 100;
}

void FKGTableBot::Start(const FKGBoardState& S, EKGTableBotLevel InLevel, uint64 Seed)
{
	Root = S;
	Level = InLevel;
	Rng.Reseed(Seed, 36u);
	Stack.Reset();
	Stack.Reserve(40);
	RootMoves.Reset();
	FKGTableRules::Generate(Root, RootMoves);
	RootMoves.Sort([this](const FKGTableMove& A, const FKGTableMove& B) { return CaptureValue(Root, A) > CaptureValue(Root, B); });
	LastRootScores.Reset();
	TotalNodes = 0;
	NodeBudgetLeft = NodeBudget(Root.Game);
	DepthReached = 0;
	bHaveMove = false;
	BestScore = 0;
	BestMove = FKGTableMove();
	const bool bChess = Root.Game == EKGTableGame::Chess;
	switch (Level)
	{
	case EKGTableBotLevel::Apprentice:
		TargetDepth = 1;
		QuiescenceMax = bChess ? 2 : 2;
		break;
	case EKGTableBotLevel::Journeyman:
		TargetDepth = bChess ? 2 : 6;
		QuiescenceMax = bChess ? 4 : 4;
		break;
	default:
		TargetDepth = bChess ? 4 : 8;
		QuiescenceMax = bChess ? 4 : 6;
		break;
	}
	if (RootMoves.Num() == 0)
	{
		bSearching = false;
		return;
	}
	if (RootMoves.Num() == 1)
	{
		BestMove = RootMoves[0];
		bHaveMove = true;
		bSearching = false;
		DepthReached = 0;
		return;
	}
	bSearching = true;
	CurrentDepth = 0;
	BeginIteration(Level == EKGTableBotLevel::Master ? 1 : TargetDepth);
}

void FKGTableBot::BeginIteration(int32 Depth)
{
	CurrentDepth = Depth;
	RootScores.Init(-Infinite, RootMoves.Num());
	Stack.Reset();
	FFrame& F = Stack.AddDefaulted_GetRef();
	F.S = Root;
	F.Moves = RootMoves;   // already ordered (best of the last iteration first, see FinishIteration)
	F.Index = 0;
	F.Depth = Depth;
	F.Ply = 0;
	++TotalNodes;
	--NodeBudgetLeft;
}

void FKGTableBot::ExpandFrame(FFrame& F)
{
	++TotalNodes;
	--NodeBudgetLeft;
	F.Index = 0;
	const bool bChess = F.S.Game == EKGTableGame::Chess;
	if (F.Depth <= 0)
	{
		// Capture search: chess stands pat and tries captures; draughts only continues while a capture is forced.
		if (F.Depth <= -QuiescenceMax)
		{
			F.Moves.Reset();
			F.Best = Evaluate(F.S);
			return;
		}
		TArray<FKGTableMove> All;
		FKGTableRules::Generate(F.S, All);
		F.Moves.Reset();
		if (bChess)
		{
			const int32 StandPat = Evaluate(F.S);
			F.Best = StandPat;
			if (StandPat >= F.Beta)
			{
				return;
			}
			F.Alpha = FMath::Max(F.Alpha, StandPat);
			for (const FKGTableMove& M : All)
			{
				if (IsCapture(F.S, M))
				{
					F.Moves.Add(M);
				}
			}
		}
		else
		{
			if (All.Num() == 0)
			{
				F.Best = -(Mate - F.Ply);
				return;
			}
			if (IsCapture(F.S, All[0]))
			{
				F.Moves = MoveTemp(All);
				F.Best = -Infinite;
			}
			else
			{
				F.Best = Evaluate(F.S);
				return;
			}
		}
	}
	else
	{
		FKGTableRules::Generate(F.S, F.Moves);
		if (F.Moves.Num() == 0)
		{
			if (bChess)
			{
				F.Best = FKGChessRules::InCheck(F.S) ? -(Mate - F.Ply) : 0;
			}
			else
			{
				F.Best = -(Mate - F.Ply);
			}
			return;
		}
		F.Best = -Infinite;
	}
	const FKGBoardState& S = F.S;
	F.Moves.Sort([&S](const FKGTableMove& A, const FKGTableMove& B) { return CaptureValue(S, A) > CaptureValue(S, B); });
}

bool FKGTableBot::ReturnValue(int32 Value)
{
	Stack.Pop(EAllowShrinking::No);
	if (Stack.Num() == 0)
	{
		return false;
	}
	FFrame& Parent = Stack.Last();
	const int32 Score = -Value;
	if (Stack.Num() == 1)
	{
		RootScores[Parent.Index] = Score;
	}
	++Parent.Index;
	if (Score > Parent.Best)
	{
		Parent.Best = Score;
	}
	if (Score > Parent.Alpha)
	{
		Parent.Alpha = Score;
	}
	if (Parent.Alpha >= Parent.Beta)
	{
		Parent.Index = Parent.Moves.Num();   // cut-off
	}
	return true;
}

bool FKGTableBot::Step(int32 MaxNodes, double MaxSeconds)
{
	if (!bSearching)
	{
		return true;
	}
	const double Deadline = FPlatformTime::Seconds() + MaxSeconds;
	int64 SliceNodes = 0;
	int32 Tick = 0;
	while (bSearching)
	{
		if (NodeBudgetLeft <= 0)
		{
			// Out of budget: keep the last completed iteration (or the partial root scores when there is none).
			if (LastRootScores.Num() == 0)
			{
				LastRootScores = RootScores;
			}
			bSearching = false;
			ChooseMove();
			return true;
		}
		if (SliceNodes >= MaxNodes || ((++Tick & 15) == 0 && FPlatformTime::Seconds() >= Deadline))
		{
			return false;
		}
		FFrame& F = Stack.Last();
		if (F.Index < 0)
		{
			ExpandFrame(F);
			++SliceNodes;
			continue;
		}
		if (F.Index < F.Moves.Num())
		{
			FFrame& Child = Stack.AddDefaulted_GetRef();
			FFrame& P = Stack[Stack.Num() - 2];   // AddDefaulted may not reallocate (reserved), re-fetch anyway
			Child.S = P.S;
			FKGTableRules::Make(Child.S, P.Moves[P.Index]);
			Child.Alpha = -P.Beta;
			Child.Beta = -P.Alpha;
			Child.Depth = P.Depth - 1;
			Child.Ply = P.Ply + 1;
			Child.Index = -1;
			continue;
		}
		const int32 Value = F.Best;
		if (!ReturnValue(Value))
		{
			FinishIteration();
		}
	}
	return true;
}

void FKGTableBot::FinishIteration()
{
	LastRootScores = RootScores;
	DepthReached = CurrentDepth;
	// Order the root moves by score for the next iteration (stable: keeps capture order among equals).
	TArray<int32> Order;
	Order.Reserve(RootMoves.Num());
	for (int32 i = 0; i < RootMoves.Num(); ++i)
	{
		Order.Add(i);
	}
	Order.StableSort([this](int32 A, int32 B) { return LastRootScores[A] > LastRootScores[B]; });
	TArray<FKGTableMove> NewMoves;
	TArray<int32> NewScores;
	for (const int32 i : Order)
	{
		NewMoves.Add(RootMoves[i]);
		NewScores.Add(LastRootScores[i]);
	}
	RootMoves = MoveTemp(NewMoves);
	LastRootScores = MoveTemp(NewScores);
	const bool bMateFound = LastRootScores.Num() > 0 && FMath::Abs(LastRootScores[0]) > Mate - 100;
	if (CurrentDepth >= TargetDepth || bMateFound || NodeBudgetLeft <= 0)
	{
		bSearching = false;
		ChooseMove();
		return;
	}
	BeginIteration(CurrentDepth + 1);
}

void FKGTableBot::ChooseMove()
{
	// Best searched move (the root list is ordered best-first after a completed iteration; a partial first
	// iteration falls back to the best of what was scored, else the first ordered move).
	int32 BestIndex = 0;
	int32 SecondIndex = INDEX_NONE;
	int32 Best = -Infinite - 1;
	int32 Second = -Infinite - 1;
	for (int32 i = 0; i < RootMoves.Num(); ++i)
	{
		const int32 Score = LastRootScores.IsValidIndex(i) ? LastRootScores[i] : -Infinite;
		if (Score > Best)
		{
			Second = Best;
			SecondIndex = BestIndex;
			Best = Score;
			BestIndex = i;
		}
		else if (Score > Second)
		{
			Second = Score;
			SecondIndex = i;
		}
	}
	BestMove = RootMoves[BestIndex];
	BestScore = Best;
	if (Level == EKGTableBotLevel::Apprentice && SecondIndex != INDEX_NONE && Second > -Infinite &&
	    Second > -(Mate - 100) && Rng.FRand() < 0.2f)
	{
		BestMove = RootMoves[SecondIndex];
		BestScore = Second;
	}
	bHaveMove = true;
}

FKGTableMove FKGTableBot::Think(const FKGBoardState& S, EKGTableBotLevel InLevel, uint64 Seed)
{
	Start(S, InLevel, Seed);
	while (!Step(1 << 20, 10.0))
	{
	}
	return BestMove;
}
