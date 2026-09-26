#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "Tabletop/KGBoardTypes.h"
#include "KGTableBot.generated.h"

/** Docs/Design/Tabletop_Games.md section 6: Apprentice / Journeyman / Master ("Anselm" is only a difficulty label). */
UENUM(BlueprintType)
enum class EKGTableBotLevel : uint8
{
	Apprentice,   // depth 1 (+ capture search), plays the second-best move 20% of the time (seeded)
	Journeyman,   // chess: 2 plies + captures up to 4 more; draughts: 6 plies
	Master        // iterative deepening to 4 (chess) / 8 (draughts) plies inside the node budget
};

/**
 * Resumable alpha-beta searcher for both games (G6.1: it reads only the board). The search runs on an explicit
 * stack so it can be sliced: Step() advances by at most MaxNodes nodes or MaxSeconds of wall time and returns
 * whether the move is ready. Results depend only on the position, level and seed (the node budget, never the wall
 * time, bounds the search), so bots are deterministic under FKGRng. Budgets: chess 20k nodes, draughts 50k.
 */
class KILLGODOT_API FKGTableBot
{
public:
	static constexpr int32 Mate = 30000;
	static constexpr int32 Infinite = 32000;

	/** Begins thinking about S. */
	void Start(const FKGBoardState& S, EKGTableBotLevel Level, uint64 Seed);
	/** One slice. Returns true when the best move is ready (also true when there is nothing to search). */
	bool Step(int32 MaxNodes, double MaxSeconds);
	/** Start + Step until done (tests, headless benchmarks). */
	FKGTableMove Think(const FKGBoardState& S, EKGTableBotLevel Level, uint64 Seed);

	bool IsSearching() const { return bSearching; }
	bool HasMove() const { return bHaveMove; }
	const FKGTableMove& GetBest() const { return BestMove; }
	/** Score of the best move from the mover's point of view (centipawns; +-Mate-ish near mate). */
	int32 GetScore() const { return BestScore; }
	int64 GetNodes() const { return TotalNodes; }
	int32 GetDepthReached() const { return DepthReached; }

	/** Static evaluation from the side to move's point of view. */
	static int32 Evaluate(const FKGBoardState& S);
	static int32 NodeBudget(EKGTableGame Game) { return Game == EKGTableGame::Chess ? 20000 : 50000; }

private:
	struct FFrame
	{
		FKGBoardState S;
		TArray<FKGTableMove> Moves;
		int32 Index = -1;   // -1 = not expanded yet
		int32 Alpha = -Infinite;
		int32 Beta = Infinite;
		int32 Best = -Infinite;
		int32 Depth = 0;    // plies left; <= 0 = capture search
		int32 Ply = 0;
	};

	void BeginIteration(int32 Depth);
	void ExpandFrame(FFrame& F);
	/** Pops the top frame, hands its value to the parent. Returns false when the root finished. */
	bool ReturnValue(int32 Value);
	void FinishIteration();
	void ChooseMove();
	static int32 CaptureValue(const FKGBoardState& S, const FKGTableMove& M);
	static bool IsCapture(const FKGBoardState& S, const FKGTableMove& M);

	FKGBoardState Root;
	EKGTableBotLevel Level = EKGTableBotLevel::Journeyman;
	FKGRng Rng{1};
	TArray<FFrame> Stack;
	TArray<FKGTableMove> RootMoves;
	TArray<int32> RootScores;     // score of each root move in the current iteration (-Infinite = not searched)
	TArray<int32> LastRootScores; // from the last completed iteration
	int32 TargetDepth = 1;
	int32 CurrentDepth = 0;
	int32 DepthReached = 0;
	int32 QuiescenceMax = 0;
	int64 TotalNodes = 0;
	int64 NodeBudgetLeft = 0;
	bool bSearching = false;
	bool bHaveMove = false;
	FKGTableMove BestMove;
	int32 BestScore = 0;
};
