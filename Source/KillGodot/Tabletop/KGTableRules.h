#pragma once

#include "CoreMinimal.h"
#include "Tabletop/KGBoardTypes.h"

/**
 * Pure rules (no UObjects, deterministic, copy-make). Every function is static and re-entrant.
 * Chess: FIDE moves, castling, en passant, promotion, check / checkmate / stalemate, threefold repetition (Zobrist),
 * 50-move rule, insufficient material. Draughts: English checkers (8x8, dark squares, men move forward, mandatory
 * capture with mandatory multi-jump continuation, crowning ends the move, kings step one square in four directions),
 * no-move loss, 80-ply no-progress draw, threefold repetition.
 */
class KILLGODOT_API FKGChessRules
{
public:
	static void Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out);
	static void Make(FKGBoardState& S, const FKGTableMove& M);
	static bool IsAttacked(const FKGBoardState& S, int32 Sq, uint8 ByColour);
	static int32 KingSquare(const FKGBoardState& S, uint8 Colour);
	static bool InCheck(const FKGBoardState& S) { return IsAttacked(S, KingSquare(S, S.Side), 1 - S.Side); }
	static bool InsufficientMaterial(const FKGBoardState& S);
	/** Mating material for one colour alone (a lone king, K+B, K+N cannot win on time). */
	static bool HasMatingMaterial(const FKGBoardState& S, uint8 Colour);
	static int64 Perft(const FKGBoardState& S, int32 Depth);

private:
	static void GeneratePseudo(const FKGBoardState& S, TArray<FKGTableMove>& Out);
};

class KILLGODOT_API FKGDraughtsRules
{
public:
	static void Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out);
	static void Make(FKGBoardState& S, const FKGTableMove& M);
	static int64 Perft(const FKGBoardState& S, int32 Depth);
	static bool IsDark(int32 Sq) { return ((Sq & 7) + (Sq >> 3)) % 2 == 0; }

private:
	static void Jumps(const FKGBoardState& S, int32 From, int32 Start, uint8 Piece, uint64 Captured, FKGTableMove& Cur,
	                  TArray<FKGTableMove>& Out);
};

/** Dispatcher on FKGBoardState::Game. */
class KILLGODOT_API FKGTableRules
{
public:
	static void Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out);
	static void Make(FKGBoardState& S, const FKGTableMove& M);
	/** Finds the legal move matching text ("e2e4", "e7e8q", "c3xe5"), false if illegal. */
	static bool FindLegal(const FKGBoardState& S, const FString& Text, FKGTableMove& Out);
	static bool IsLegal(const FKGBoardState& S, const FKGTableMove& M);
	/**
	 * Result of the position, History = Zobrist keys of the positions since the last irreversible move (the current
	 * position's key included) for repetition.
	 */
	static FKGTableVerdict Evaluate(const FKGBoardState& S, const TArray<uint64>& History);
	static int64 Perft(const FKGBoardState& S, int32 Depth);
	static const TCHAR* StatusText(EKGTableStatus Status);
	static const TCHAR* ReasonText(EKGTableEndReason Reason);
	static const TCHAR* ClockText(EKGTableClock Clock);
	/** Base seconds and Fischer increment of a preset. */
	static void ClockPreset(EKGTableClock Clock, float& OutBase, float& OutIncrement);
};
