#pragma once

#include "CoreMinimal.h"
#include "KGBoardTypes.generated.h"

/**
 * Tabletop games (SPRINT-036, Docs/Design/Tabletop_Games.md): one 8x8 board representation shared by chess and
 * English draughts (checkers). Plain structs, no UObjects: the rules (FKGChessRules / FKGDraughtsRules) and the bot
 * (FKGTableBot) work on copies, the table actor (AKGBoardTable) replicates a packed copy.
 *
 * Squares: index = file + 8 * rank, a1 = 0, h8 = 63. "White" always sits on ranks 1-2 and moves first (in draughts
 * the official first mover is Black; here the first sitter is White and moves first, the rules are symmetric).
 * Piece codes (KGPiece): 0 empty; chess White P N B R Q K = 1..6, Black = +8 (9..14); draughts man = 1, king = 2,
 * Black = +8.
 */
UENUM(BlueprintType)
enum class EKGTableGame : uint8
{
	Chess,
	Draughts
};

UENUM(BlueprintType)
enum class EKGTableStatus : uint8
{
	Waiting,      // seats not both ready
	Playing,
	Adjourned,    // paused: someone stood up / phase interruption; resumes with the same two players
	WhiteWins,
	BlackWins,
	Draw
};

UENUM(BlueprintType)
enum class EKGTableEndReason : uint8
{
	None,
	Checkmate,
	Stalemate,
	NoMoves,          // draughts: no legal move = loss
	Resign,
	Flag,             // clock ran out
	Repetition,
	FiftyMove,        // chess 100 plies / draughts 80 plies without progress
	Material,         // insufficient mating material
	Agreement,
	Abandoned
};

/** Fischer clock presets (Docs/Design/Tabletop_Games.md section 3.2). */
UENUM(BlueprintType)
enum class EKGTableClock : uint8
{
	Untimed,
	Bullet,   // 1 min + 1 s
	Blitz,    // 3 min + 2 s
	Rapid     // 5 min + 3 s
};

namespace KGPiece
{
	inline constexpr uint8 Empty = 0;
	inline constexpr uint8 Black = 8;
	inline constexpr uint8 TypeMask = 7;
	// chess types
	inline constexpr uint8 Pawn = 1, Knight = 2, Bishop = 3, Rook = 4, Queen = 5, King = 6;
	// draughts types
	inline constexpr uint8 Man = 1, DKing = 2;

	inline uint8 TypeOf(uint8 P) { return P & TypeMask; }
	inline bool IsBlack(uint8 P) { return (P & Black) != 0; }
	/** 0 = white, 1 = black (only meaningful for a non-empty piece). */
	inline uint8 ColourOf(uint8 P) { return IsBlack(P) ? 1 : 0; }
	inline uint8 Make(uint8 Type, uint8 Colour) { return Type | (Colour ? Black : 0); }
	inline int32 FileOf(int32 Sq) { return Sq & 7; }
	inline int32 RankOf(int32 Sq) { return Sq >> 3; }
	inline int32 SquareAt(int32 File, int32 Rank) { return File + Rank * 8; }
	inline bool OnBoard(int32 File, int32 Rank) { return File >= 0 && File < 8 && Rank >= 0 && Rank < 8; }
	KILLGODOT_API FString SquareName(int32 Sq);
	/** "e4" -> 28, -1 when malformed. */
	KILLGODOT_API int32 SquareFromName(const FString& Name);
}

/** One move. Chess: From/To (+ Promo type for pawns). Draughts: From, then every landing square in Hops (To = last). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGTableMove
{
	GENERATED_BODY()

	UPROPERTY() uint8 From = 0;
	UPROPERTY() uint8 To = 0;
	/** Chess: promotion piece type (Knight..Queen) or 0. */
	UPROPERTY() uint8 Promo = 0;
	/** Draughts: number of landing squares (1 = a simple step or a single jump). */
	UPROPERTY() uint8 NumHops = 0;
	uint8 Hops[12] = {};

	bool IsValid() const { return From != To || NumHops > 0; }
	bool operator==(const FKGTableMove& O) const;
	bool operator!=(const FKGTableMove& O) const { return !(*this == O); }

	/** "e2e4", "e7e8q" (chess) / "c3d4", "c3xe5xg7" (draughts jumps). */
	FString ToString() const;
	/** Parses ToString() output (also accepts "e2-e4", "c3-d4", "e7e8=Q"). */
	static bool Parse(const FString& Text, FKGTableMove& Out);
};

/** The position. Copyable by value (the rules use copy-make). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGBoardState
{
	GENERATED_BODY()

	uint8 Sq[64] = {};
	UPROPERTY(SaveGame) EKGTableGame Game = EKGTableGame::Chess;
	/** 0 = white to move, 1 = black. */
	UPROPERTY(SaveGame) uint8 Side = 0;
	/** Chess castling rights: 1 = white O-O, 2 = white O-O-O, 4 = black O-O, 8 = black O-O-O. */
	UPROPERTY(SaveGame) uint8 Castling = 0;
	/** Chess: en passant target square (the square behind the pawn that just double-stepped), -1 = none. */
	UPROPERTY(SaveGame) int8 Ep = -1;
	/** Plies since the last capture / pawn (draughts: man) move. */
	UPROPERTY(SaveGame) uint16 HalfClock = 0;
	/** Plies played from the start position. */
	UPROPERTY(SaveGame) uint16 Ply = 0;

	void Reset(EKGTableGame InGame);
	uint8 At(int32 File, int32 Rank) const { return Sq[KGPiece::SquareAt(File, Rank)]; }
	int32 FullMove() const { return 1 + Ply / 2; }

	/** Zobrist key over squares, side, castling rights and en passant file (repetition detection). */
	uint64 Hash() const;
	int32 CountPieces(uint8 Colour) const;

	/** Chess FEN; draughts: "<64 chars a8..h1 rows / separated> <w|b> <halfclock> <ply>" using . w W b B. */
	FString ToFen() const;
	bool FromFen(const FString& Fen, EKGTableGame InGame);

	/** 32-byte packed squares (two nibbles per byte) for replication. */
	void Pack(TArray<uint8>& Out) const;
	bool Unpack(const TArray<uint8>& In);
};

/** What the rules say about a position. */
struct FKGTableVerdict
{
	EKGTableStatus Status = EKGTableStatus::Playing;
	EKGTableEndReason Reason = EKGTableEndReason::None;
	bool IsOver() const { return Status != EKGTableStatus::Playing; }
};
