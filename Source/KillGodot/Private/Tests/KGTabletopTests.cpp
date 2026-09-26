#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Tabletop/KGBoardTable.h"
#include "Tabletop/KGTableBot.h"
#include "Tabletop/KGTableRules.h"
#include "UObject/Package.h"
#include "World/KGSeat.h"

// SPRINT-036a acceptance: perft (independent reference numbers), >= 20 rule cases, bot budget / determinism, and
// the server-authoritative table (seats, turn order, refusals, clocks, adjourn / resume).

namespace KGTabletopTests
{
	FKGBoardState Chess(const TCHAR* Fen)
	{
		FKGBoardState S;
		S.FromFen(Fen, EKGTableGame::Chess);
		return S;
	}

	FKGBoardState EmptyDraughts()
	{
		FKGBoardState S;
		S.Reset(EKGTableGame::Draughts);
		FMemory::Memzero(S.Sq, sizeof(S.Sq));
		return S;
	}

	int32 Sq(const TCHAR* Name) { return KGPiece::SquareFromName(Name); }

	bool HasMove(const FKGBoardState& S, const TCHAR* Text)
	{
		FKGTableMove M;
		return FKGTableRules::FindLegal(S, Text, M);
	}

	bool Play(FKGBoardState& S, const TCHAR* Text)
	{
		FKGTableMove M;
		if (!FKGTableRules::FindLegal(S, Text, M))
		{
			return false;
		}
		FKGTableRules::Make(S, M);
		return true;
	}

	/** Headless server world running AKGGameMode (the KillGodot.Dev.* pattern). */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGTabletopTestWorld"));
			World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
			if (!World)
			{
				return false;
			}
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GetTransientPackage()));
			World->GetWorldSettings()->DefaultGameMode = AKGGameMode::StaticClass();
			const FURL URL;
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			return World->GetAuthGameMode<AKGGameMode>() != nullptr && World->GetGameState<AKGGameState>() != nullptr;
		}

		~FServerWorld()
		{
			if (World && GEngine)
			{
				World->EndPlay(EEndPlayReason::Quit);
				World->BeginTearingDown();
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					It->Destroy();
				}
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		AKGGameState* GS() const { return World->GetGameState<AKGGameState>(); }

		AKGCharacter* BotBody(int32 Index) const
		{
			AKGPlayerState* PS = GS()->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS()->PlayerArray[Index]) : nullptr;
			AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
			if (!Controller)
			{
				return nullptr;
			}
			AKGCharacter* Body = Cast<AKGCharacter>(Controller->GetPawn());
			if (!Body)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Body = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(FVector(400.0 * Index, 0.0, 200.0)), Params);
				if (Body)
				{
					Controller->Possess(Body);
				}
			}
			if (Body)
			{
				UCharacterMovementComponent* Move = Body->GetCharacterMovement();
				Move->SetComponentTickEnabled(false);
				Move->Velocity = FVector::ZeroVector;
			}
			return Body;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTabletopPerftTest, "KillGodot.Tabletop.Perft",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTabletopPerftTest::RunTest(const FString& Parameters)
{
	using namespace KGTabletopTests;
	struct FCase { const TCHAR* Name; const TCHAR* Fen; int64 Nodes[4]; int32 Depth; };
	// Reference counts: chessprogramming.org "Perft Results" (start, Kiwipete, positions 3 and 5).
	const FCase Cases[] = {
		{TEXT("start"), TEXT("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), {20, 400, 8902, 197281}, 4},
		{TEXT("kiwipete"), TEXT("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"), {48, 2039, 97862, 0}, 3},
		{TEXT("pos3"), TEXT("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"), {14, 191, 2812, 43238}, 4},
		{TEXT("pos5"), TEXT("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"), {44, 1486, 62379, 0}, 3},
	};
	for (const FCase& C : Cases)
	{
		const FKGBoardState S = Chess(C.Fen);
		for (int32 D = 1; D <= C.Depth; ++D)
		{
			const int64 N = FKGChessRules::Perft(S, D);
			TestEqual(FString::Printf(TEXT("chess %s perft %d"), C.Name, D), N, C.Nodes[D - 1]);
		}
	}
	// English draughts from the start (multi-jump = one move): 7, 49, 302, 1469, 7361 (checkers perft, e.g. Martin Fierz).
	FKGBoardState Dr;
	Dr.Reset(EKGTableGame::Draughts);
	const int64 DraughtsRef[5] = {7, 49, 302, 1469, 7361};
	for (int32 D = 1; D <= 5; ++D)
	{
		TestEqual(FString::Printf(TEXT("draughts perft %d"), D), FKGDraughtsRules::Perft(Dr, D), DraughtsRef[D - 1]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTabletopRulesTest, "KillGodot.Tabletop.Rules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTabletopRulesTest::RunTest(const FString& Parameters)
{
	using namespace KGTabletopTests;
	using namespace KGPiece;
	TArray<uint64> NoHistory;

	// 1-4 castling
	TestFalse(TEXT("castling through an attacked square is refused"), HasMove(Chess(TEXT("4k3/8/8/8/8/8/5r2/4K2R w K - 0 1")), TEXT("e1g1")));
	TestFalse(TEXT("castling out of check is refused"), HasMove(Chess(TEXT("4k3/8/8/8/8/8/4r3/4K2R w K - 0 1")), TEXT("e1g1")));
	{
		FKGBoardState S = Chess(TEXT("4k3/8/8/8/8/8/8/4K2R w K - 0 1"));
		TestTrue(TEXT("castling with a clear path is legal"), Play(S, TEXT("e1g1")));
		TestEqual(TEXT("castling moves the rook to f1"), static_cast<int32>(S.Sq[Sq(TEXT("f1"))]), static_cast<int32>(Make(Rook, 0)));
		TestEqual(TEXT("castling moves the king to g1"), static_cast<int32>(S.Sq[Sq(TEXT("g1"))]), static_cast<int32>(Make(King, 0)));
		TestEqual(TEXT("castling clears white's rights"), static_cast<int32>(S.Castling & 3), 0);
	}
	TestTrue(TEXT("queenside castling only needs c1/d1 safe (b1 may be attacked)"), HasMove(Chess(TEXT("4k3/8/8/8/8/8/1r6/R3K3 w Q - 0 1")), TEXT("e1c1")));
	TestFalse(TEXT("no castling without the right"), HasMove(Chess(TEXT("4k3/8/8/8/8/8/8/4K2R w - - 0 1")), TEXT("e1g1")));

	// 5-6 en passant
	{
		FKGBoardState S;
		S.Reset(EKGTableGame::Chess);
		Play(S, TEXT("e2e4")); Play(S, TEXT("a7a6")); Play(S, TEXT("e4e5")); Play(S, TEXT("d7d5"));
		TestEqual(TEXT("double step sets the ep square"), static_cast<int32>(S.Ep), Sq(TEXT("d6")));
		TestTrue(TEXT("en passant right after the double step"), HasMove(S, TEXT("e5d6")));
		FKGBoardState T = S;
		Play(T, TEXT("e5d6"));
		TestEqual(TEXT("en passant removes the passed pawn"), static_cast<int32>(T.Sq[Sq(TEXT("d5"))]), 0);
		Play(S, TEXT("a2a3")); Play(S, TEXT("a6a5"));
		TestFalse(TEXT("en passant only on the very next move"), HasMove(S, TEXT("e5d6")));
	}
	// 7-8 promotion
	{
		FKGBoardState S = Chess(TEXT("8/P6k/8/8/8/8/8/K7 w - - 0 1"));
		TArray<FKGTableMove> Moves;
		FKGChessRules::Generate(S, Moves);
		int32 Promotions = 0;
		for (const FKGTableMove& M : Moves) { Promotions += (M.From == Sq(TEXT("a7")) && M.Promo != 0) ? 1 : 0; }
		TestEqual(TEXT("a pawn on the seventh promotes to four pieces"), Promotions, 4);
		FKGBoardState T = S;
		TestTrue(TEXT("knight underpromotion parses"), Play(T, TEXT("a7a8n")));
		TestEqual(TEXT("knight underpromotion lands a knight"), static_cast<int32>(T.Sq[Sq(TEXT("a8"))]), static_cast<int32>(Make(Knight, 0)));
		FKGTableMove Q;
		TestTrue(TEXT("a7a8 without a piece is a queen"), FKGTableRules::FindLegal(S, TEXT("a7a8"), Q) && Q.Promo == Queen);
	}
	// 9-10 stalemate / checkmate
	{
		const FKGTableVerdict V = FKGTableRules::Evaluate(Chess(TEXT("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")), NoHistory);
		TestTrue(TEXT("stalemate is a draw"), V.Status == EKGTableStatus::Draw && V.Reason == EKGTableEndReason::Stalemate);
		const FKGTableVerdict M = FKGTableRules::Evaluate(Chess(TEXT("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4")), NoHistory);
		TestTrue(TEXT("scholar's mate is checkmate for White"), M.Status == EKGTableStatus::WhiteWins && M.Reason == EKGTableEndReason::Checkmate);
	}
	// 11 threefold repetition (Zobrist)
	{
		FKGBoardState S;
		S.Reset(EKGTableGame::Chess);
		TArray<uint64> History;
		History.Add(S.Hash());
		const TCHAR* Shuffle[8] = {TEXT("g1f3"), TEXT("g8f6"), TEXT("f3g1"), TEXT("f6g8"), TEXT("g1f3"), TEXT("g8f6"), TEXT("f3g1"), TEXT("f6g8")};
		FKGTableVerdict V;
		for (int32 i = 0; i < 8; ++i)
		{
			Play(S, Shuffle[i]);
			History.Add(S.Hash());
			V = FKGTableRules::Evaluate(S, History);
			if (i < 7)
			{
				TestTrue(TEXT("no draw before the third occurrence"), !V.IsOver());
			}
		}
		TestTrue(TEXT("third repetition is a draw"), V.Status == EKGTableStatus::Draw && V.Reason == EKGTableEndReason::Repetition);
		FKGBoardState A = Chess(TEXT("4k3/8/8/8/8/8/8/4K2R w K - 0 1"));
		FKGBoardState B = Chess(TEXT("4k3/8/8/8/8/8/8/4K2R w - - 0 1"));
		TestTrue(TEXT("castling rights change the hash"), A.Hash() != B.Hash());
	}
	// 12 fifty-move rule
	{
		FKGBoardState S = Chess(TEXT("4k3/8/8/8/8/8/8/R3K3 w - - 99 80"));
		Play(S, TEXT("a1a2"));
		const FKGTableVerdict V = FKGTableRules::Evaluate(S, NoHistory);
		TestTrue(TEXT("100 quiet plies is a draw"), V.Status == EKGTableStatus::Draw && V.Reason == EKGTableEndReason::FiftyMove);
	}
	// 13-14 material
	TestTrue(TEXT("K vs K is insufficient"), FKGChessRules::InsufficientMaterial(Chess(TEXT("4k3/8/8/8/8/8/8/4K3 w - - 0 1"))));
	TestTrue(TEXT("K+B vs K is insufficient"), FKGChessRules::InsufficientMaterial(Chess(TEXT("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1"))));
	TestTrue(TEXT("K+N vs K is insufficient"), FKGChessRules::InsufficientMaterial(Chess(TEXT("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1"))));
	// c1 and d8 are both dark squares; c8 is light (a1 dark: file+rank even = dark).
	TestTrue(TEXT("same-coloured bishops are insufficient"), FKGChessRules::InsufficientMaterial(Chess(TEXT("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1"))));
	TestFalse(TEXT("opposite-coloured bishops are not"), FKGChessRules::InsufficientMaterial(Chess(TEXT("2b1k3/8/8/8/8/8/8/2B1K3 w - - 0 1"))));
	TestFalse(TEXT("a rook is enough"), FKGChessRules::InsufficientMaterial(Chess(TEXT("4k3/8/8/8/8/8/8/R3K3 w - - 0 1"))));
	{
		const FKGBoardState S = Chess(TEXT("4k3/8/8/8/8/8/P7/1N2K3 w - - 0 1"));
		TestTrue(TEXT("a pawn can mate on time"), FKGChessRules::HasMatingMaterial(S, 0));
		TestFalse(TEXT("a lone king cannot"), FKGChessRules::HasMatingMaterial(S, 1));
		TestTrue(TEXT("evaluate: insufficient material ends the game"), FKGTableRules::Evaluate(Chess(TEXT("4k3/8/8/8/8/8/8/4K3 w - - 0 1")), NoHistory).Reason == EKGTableEndReason::Material);
	}
	// 15-16 draughts: forced capture, only captures
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("c3"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("a1"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("d4"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("h8"))] = Make(Man, 1);
		TArray<FKGTableMove> Moves;
		FKGDraughtsRules::Generate(S, Moves);
		TestEqual(TEXT("capture is mandatory: exactly one move"), Moves.Num(), 1);
		TestTrue(TEXT("the one move is c3xe5"), Moves.Num() == 1 && Moves[0].ToString() == TEXT("c3xe5"));
		TestFalse(TEXT("a quiet step is refused while a capture exists"), HasMove(S, TEXT("a1b2")));
	}
	// 17 multi-jump continuation
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("c3"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("d4"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("f6"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("h8"))] = Make(Man, 1);
		TArray<FKGTableMove> Moves;
		FKGDraughtsRules::Generate(S, Moves);
		TestEqual(TEXT("multi-jump: one sequence"), Moves.Num(), 1);
		TestTrue(TEXT("the sequence is c3xe5xg7"), Moves.Num() == 1 && Moves[0].ToString() == TEXT("c3xe5xg7"));
		FKGTableMove Partial;
		FKGTableMove::Parse(TEXT("c3xe5"), Partial);
		TestFalse(TEXT("stopping halfway is illegal"), FKGTableRules::IsLegal(S, Partial));
		Play(S, TEXT("c3xe5xg7"));
		TestEqual(TEXT("both jumped men are gone"), static_cast<int32>(S.Sq[Sq(TEXT("d4"))]) + S.Sq[Sq(TEXT("f6"))], 0);
	}
	// 18 crowning ends the move
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("b6"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("c7"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("e7"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("h2"))] = Make(Man, 1);
		TArray<FKGTableMove> Moves;
		FKGDraughtsRules::Generate(S, Moves);
		TestTrue(TEXT("crowning jump stops on the king row"), Moves.Num() == 1 && Moves[0].NumHops == 1 && Moves[0].To == Sq(TEXT("d8")));
		Play(S, TEXT("b6xd8"));
		TestEqual(TEXT("the man is crowned"), static_cast<int32>(S.Sq[Sq(TEXT("d8"))]), static_cast<int32>(Make(DKing, 0)));
		TestEqual(TEXT("the second man survives"), static_cast<int32>(S.Sq[Sq(TEXT("e7"))]), static_cast<int32>(Make(Man, 1)));
	}
	// 19-20 kings move four ways, men only forward
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("d4"))] = Make(DKing, 0);
		S.Sq[Sq(TEXT("h8"))] = Make(Man, 1);
		TArray<FKGTableMove> Moves;
		FKGDraughtsRules::Generate(S, Moves);
		TestEqual(TEXT("a king steps in four directions"), Moves.Num(), 4);
		S.Sq[Sq(TEXT("d4"))] = Make(Man, 0);
		FKGDraughtsRules::Generate(S, Moves);
		TestEqual(TEXT("a man steps forward only"), Moves.Num(), 2);
		TestFalse(TEXT("no flying kings"), HasMove(S, TEXT("d4f6")));
	}
	// 21 no moves = loss
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("a1"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("b2"))] = Make(Man, 1);
		S.Sq[Sq(TEXT("c3"))] = Make(Man, 1);
		const FKGTableVerdict V = FKGTableRules::Evaluate(S, NoHistory);
		TestTrue(TEXT("a blocked side loses"), V.Status == EKGTableStatus::BlackWins && V.Reason == EKGTableEndReason::NoMoves);
	}
	// 22 eighty-ply no-progress draw
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("a1"))] = Make(DKing, 0);
		S.Sq[Sq(TEXT("h8"))] = Make(DKing, 1);
		S.HalfClock = 79;
		Play(S, TEXT("a1b2"));
		const FKGTableVerdict V = FKGTableRules::Evaluate(S, NoHistory);
		TestTrue(TEXT("80 plies without a capture or man move is a draw"), V.Status == EKGTableStatus::Draw && V.Reason == EKGTableEndReason::FiftyMove);
	}
	// 23 a man move resets the counter
	{
		FKGBoardState S = EmptyDraughts();
		S.Sq[Sq(TEXT("a1"))] = Make(Man, 0);
		S.Sq[Sq(TEXT("h8"))] = Make(DKing, 1);
		S.HalfClock = 50;
		Play(S, TEXT("a1b2"));
		TestEqual(TEXT("man move resets the no-progress counter"), static_cast<int32>(S.HalfClock), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTabletopSerializationTest, "KillGodot.Tabletop.Serialization",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTabletopSerializationTest::RunTest(const FString& Parameters)
{
	using namespace KGTabletopTests;
	const FString Kiwi = TEXT("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
	TestEqual(TEXT("chess FEN round trip"), Chess(*Kiwi).ToFen(), Kiwi);
	FKGBoardState Dr;
	Dr.Reset(EKGTableGame::Draughts);
	Play(Dr, TEXT("c3d4"));
	FKGBoardState Dr2;
	TestTrue(TEXT("draughts FEN parses"), Dr2.FromFen(Dr.ToFen(), EKGTableGame::Draughts));
	TestEqual(TEXT("draughts FEN round trip"), Dr2.ToFen(), Dr.ToFen());
	TestTrue(TEXT("draughts hash survives the round trip"), Dr2.Hash() == Dr.Hash());
	TArray<uint8> Packed;
	Dr.Pack(Packed);
	TestEqual(TEXT("packed board is 32 bytes"), Packed.Num(), 32);
	FKGBoardState Un;
	Un.Reset(EKGTableGame::Draughts);
	Un.Unpack(Packed);
	Un.Side = Dr.Side;
	Un.Ply = Dr.Ply;
	TestEqual(TEXT("pack / unpack round trip"), Un.ToFen(), Dr.ToFen());
	FKGTableMove M;
	TestTrue(TEXT("jump text parses"), FKGTableMove::Parse(TEXT("c3xe5xg7"), M) && M.NumHops == 2);
	TestEqual(TEXT("jump text round trip"), M.ToString(), FString(TEXT("c3xe5xg7")));
	TestTrue(TEXT("promotion text parses"), FKGTableMove::Parse(TEXT("e7e8=Q"), M) && M.Promo == KGPiece::Queen);
	TestEqual(TEXT("promotion text round trip"), M.ToString(), FString(TEXT("e7e8q")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTabletopBotTest, "KillGodot.Tabletop.Bot",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTabletopBotTest::RunTest(const FString& Parameters)
{
	using namespace KGTabletopTests;
	// Mate in one is found by every level.
	const FKGBoardState MateIn1 = Chess(TEXT("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4"));
	for (int32 L = 0; L < 3; ++L)
	{
		FKGTableBot Bot;
		const FKGTableMove M = Bot.Think(MateIn1, static_cast<EKGTableBotLevel>(L), 3);
		TestEqual(FString::Printf(TEXT("level %d plays Qxf7#"), L), M.ToString(), FString(TEXT("h5f7")));
		TestTrue(FString::Printf(TEXT("level %d stays inside the node budget"), L), Bot.GetNodes() <= FKGTableBot::NodeBudget(EKGTableGame::Chess) + 64);
	}
	// Deterministic under a seed (Apprentice rolls its 20% second-best from FKGRng).
	FKGBoardState Start;
	Start.Reset(EKGTableGame::Chess);
	for (int32 Seed = 1; Seed <= 6; ++Seed)
	{
		FKGTableBot A, B;
		TestEqual(FString::Printf(TEXT("same seed, same move (%d)"), Seed), A.Think(Start, EKGTableBotLevel::Apprentice, Seed).ToString(),
		          B.Think(Start, EKGTableBotLevel::Apprentice, Seed).ToString());
	}
	{
		FKGTableBot A, B;
		TestEqual(TEXT("master is deterministic"), A.Think(Start, EKGTableBotLevel::Master, 1).ToString(), B.Think(Start, EKGTableBotLevel::Master, 9).ToString());
	}
	// Time slicing: <= 0.5 ms asked per slice; the search only ever overruns by one node's worth.
	for (int32 G = 0; G < 2; ++G)
	{
		FKGBoardState S;
		S.Reset(G == 0 ? EKGTableGame::Chess : EKGTableGame::Draughts);
		FKGTableBot Bot;
		Bot.Start(S, EKGTableBotLevel::Master, 5);
		double Sum = 0.0, Max = 0.0;
		int32 Slices = 0;
		for (;;)
		{
			const double T0 = FPlatformTime::Seconds();
			const bool bDone = Bot.Step(1 << 20, 0.0005);
			const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
			Sum += Ms;
			Max = FMath::Max(Max, Ms);
			++Slices;
			if (bDone) break;
		}
		AddInfo(FString::Printf(TEXT("%s master: nodes=%lld depth=%d slices=%d mean=%.3f ms max=%.3f ms move=%s"), G == 0 ? TEXT("chess") : TEXT("draughts"),
		                        Bot.GetNodes(), Bot.GetDepthReached(), Slices, Sum / Slices, Max, *Bot.GetBest().ToString()));
		TestTrue(TEXT("node budget respected"), Bot.GetNodes() <= FKGTableBot::NodeBudget(S.Game) + 64);
		TestTrue(TEXT("mean slice <= 0.6 ms"), Sum / Slices <= 0.6);
		TestTrue(TEXT("no slice above 2 ms (0.5 ms asked)"), Max <= 2.0);
		TestTrue(TEXT("the bot has a move"), Bot.HasMove());
	}
	// Draughts: the Journeyman takes a free capture (the only legal move is forced anyway) and never picks an illegal one.
	{
		FKGBoardState S;
		S.Reset(EKGTableGame::Draughts);
		FKGTableBot Bot;
		for (int32 i = 0; i < 20; ++i)
		{
			const FKGTableMove M = Bot.Think(S, EKGTableBotLevel::Journeyman, i);
			if (!M.IsValid())
			{
				break;
			}
			TestTrue(TEXT("bot move is legal"), FKGTableRules::IsLegal(S, M));
			FKGTableRules::Make(S, M);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGTabletopTableTest, "KillGodot.Tabletop.Table",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGTabletopTableTest::RunTest(const FString& Parameters)
{
	using namespace KGTabletopTests;
	FServerWorld Server;
	if (!Server.Create())
	{
		AddError(TEXT("could not create the server world"));
		return false;
	}
	UWorld* World = Server.World;
	FKGDev::Execute({World, nullptr}, TEXT("Bot.Add 3"));
	AKGCharacter* A = Server.BotBody(0);
	AKGCharacter* B = Server.BotBody(1);
	AKGCharacter* C = Server.BotBody(2);
	if (!A || !B || !C)
	{
		AddError(TEXT("no bot bodies"));
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKGBoardTable* Table = World->SpawnActor<AKGBoardTable>(AKGBoardTable::StaticClass(), FTransform(FVector(0, 0, 100)), Params);
	if (!Table)
	{
		AddError(TEXT("no table"));
		return false;
	}
	Table->ServerReset(EKGTableGame::Chess, EKGTableClock::Blitz);
	TestNotNull(TEXT("the table spawned its white seat"), Table->GetSeat(0));
	TestNotNull(TEXT("the table spawned its black seat"), Table->GetSeat(1));
	Table->ServerTick(0.0f);
	TestEqual(TEXT("empty table waits"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Waiting));

	FString Err;
	TestFalse(TEXT("an unseated player cannot move"), Table->ServerTryMove(A->GetPlayerState(), TEXT("e2e4"), Err));
	TestEqual(TEXT("...refused as not seated"), Err, FString(TEXT("not seated")));

	TestTrue(TEXT("A sits as White"), Table->ServerSeat(A, 0));
	Table->ServerTick(0.0f);
	TestEqual(TEXT("one player: still waiting"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Waiting));
	TestFalse(TEXT("no game yet: move refused"), Table->ServerTryMove(A->GetPlayerState(), TEXT("e2e4"), Err));
	TestTrue(TEXT("B sits as Black"), Table->ServerSeat(B, 1));
	Table->ServerTick(0.0f);
	TestEqual(TEXT("both seated: playing"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Playing));
	TestEqual(TEXT("A is White"), Table->ColourOf(A->GetPlayerState()), 0);
	TestEqual(TEXT("B is Black"), Table->ColourOf(B->GetPlayerState()), 1);
	TestEqual(TEXT("Blitz starts at 180 s"), Table->GetClockSeconds(0), 180.0f);

	TestFalse(TEXT("Black cannot move first"), Table->ServerTryMove(B->GetPlayerState(), TEXT("e7e5"), Err));
	TestEqual(TEXT("...refused as not your turn"), Err, FString(TEXT("not your turn")));
	TestFalse(TEXT("an illegal move is refused"), Table->ServerTryMove(A->GetPlayerState(), TEXT("e2e5"), Err));
	TestEqual(TEXT("...refused as illegal"), Err, FString(TEXT("illegal")));
	TestFalse(TEXT("a spectator cannot move"), Table->ServerTryMove(C->GetPlayerState(), TEXT("e2e4"), Err));
	Table->ServerTick(1.0f);
	TestTrue(TEXT("White plays e4"), Table->ServerTryMove(A->GetPlayerState(), TEXT("e2e4"), Err));
	TestTrue(TEXT("Fischer increment credited to the mover"), FMath::IsNearlyEqual(Table->GetClockSeconds(0), 181.0f, 0.01f));
	TestEqual(TEXT("last move replicated"), Table->GetLastTo(), KGPiece::SquareFromName(TEXT("e4")));
	TestEqual(TEXT("FEN after e4"), Table->GetFen(), FString(TEXT("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")));

	// Standing up adjourns; sitting back down resumes; a stranger starts over.
	AKGSeat::StandUpCharacter(B);
	Table->ServerTick(0.0f);
	TestEqual(TEXT("standing up adjourns"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Adjourned));
	TestFalse(TEXT("no moves while adjourned"), Table->ServerTryMove(A->GetPlayerState(), TEXT("d2d4"), Err));
	World->TimeSeconds += 5.0f;   // past AKGSeat's re-sit cooldown (the headless world's clock does not run)
	Table->ServerSeat(B, 1);
	Table->ServerTick(0.0f);
	TestEqual(TEXT("the same player resumes"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Playing));
	TestEqual(TEXT("the position survived the adjournment"), static_cast<int32>(Table->GetBoard().Ply), 1);

	// Scholar's mate to the end.
	const TCHAR* Moves[7] = {TEXT("e7e5"), TEXT("f1c4"), TEXT("b8c6"), TEXT("d1h5"), TEXT("g8f6"), TEXT("h5f7"), nullptr};
	for (int32 i = 0; Moves[i]; ++i)
	{
		APlayerState* Mover = (i % 2 == 0) ? B->GetPlayerState() : A->GetPlayerState();
		TestTrue(FString::Printf(TEXT("move %s accepted"), Moves[i]), Table->ServerTryMove(Mover, Moves[i], Err));
	}
	TestEqual(TEXT("checkmate: White wins"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::WhiteWins));
	TestEqual(TEXT("...by checkmate"), static_cast<int32>(Table->GetEndReason()), static_cast<int32>(EKGTableEndReason::Checkmate));
	TestEqual(TEXT("final FEN"), Table->GetFen(), FString(TEXT("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4")));
	TestFalse(TEXT("no moves after the result"), Table->ServerTryMove(B->GetPlayerState(), TEXT("e8f7"), Err));

	// A stranger sits: new game.
	AKGSeat::StandUpCharacter(B);
	Table->ServerTick(0.0f);
	Table->ServerSeat(C, 1);
	Table->ServerTick(0.0f);
	Table->ServerTick(0.0f);
	TestEqual(TEXT("a new opponent starts a new game"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Playing));
	TestEqual(TEXT("...from the start position"), static_cast<int32>(Table->GetBoard().Ply), 0);
	TestTrue(TEXT("resignation"), Table->ServerResign(C->GetPlayerState()));
	TestEqual(TEXT("resigning loses"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::WhiteWins));

	// Flag: the clock of the side to move runs out.
	AKGSeat::StandUpCharacter(C);
	Table->ServerTick(0.0f);
	World->TimeSeconds += 5.0f;
	Table->ServerSeat(B, 1);
	Table->ServerTick(0.0f);
	Table->ServerTick(0.0f);
	TestEqual(TEXT("playing again"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Playing));
	Table->ServerTick(200.0f);
	TestEqual(TEXT("White's flag falls: Black wins"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::BlackWins));
	TestEqual(TEXT("...on time"), static_cast<int32>(Table->GetEndReason()), static_cast<int32>(EKGTableEndReason::Flag));

	// Bot vs bot draughts to a result (Apprentice both sides, no pause).
	AKGSeat::StandUpCharacter(A);
	AKGSeat::StandUpCharacter(B);
	Table->ServerTick(0.0f);
	Table->ServerReset(EKGTableGame::Draughts, EKGTableClock::Untimed);
	Table->SetBotDelayScale(0.0f);
	Table->ServerSetBot(0, true, EKGTableBotLevel::Apprentice);
	Table->ServerSetBot(1, true, EKGTableBotLevel::Journeyman);
	Table->ServerTick(0.0f);
	TestEqual(TEXT("two bots start at once"), static_cast<int32>(Table->GetStatus()), static_cast<int32>(EKGTableStatus::Playing));
	int32 Iterations = 0;
	while (Table->GetStatus() == EKGTableStatus::Playing && Iterations++ < 200000)
	{
		Table->ServerTickBot(0.002);
		Table->ServerTick(0.016f);
	}
	AddInfo(FString::Printf(TEXT("draughts bot game: %d moves, status=%s reason=%s in %d ticks"), Table->GetMoveCount(),
	                        FKGTableRules::StatusText(Table->GetStatus()), FKGTableRules::ReasonText(Table->GetEndReason()), Iterations));
	TestTrue(TEXT("the bot game reached a result"), Table->GetStatus() != EKGTableStatus::Playing && Table->GetStatus() != EKGTableStatus::Waiting);
	TestTrue(TEXT("the bots played"), Table->GetMoveCount() >= 10);
	return true;
}

#endif
