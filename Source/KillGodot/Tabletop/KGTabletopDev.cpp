// Development-only tabletop verbs (registered here, not in Dev/KGDevCommands.cpp: SPRINT-036a writes new files only).
//
//   kg.Table.Spawn [Chess|Draughts] [Bullet|Blitz|Rapid|Untimed]   a table 2.5 m in front of you (White seat nearest)
//   kg.Table.Start [Chess|Draughts] [Apprentice|Journeyman|Master|Human] [Clock]   spawn, sit you as White, bot as Black
//   kg.Table.Bot [White|Black] [Apprentice|Journeyman|Master|Off]    nearest table
//   kg.Table.Move <e2e4|e7e8q|c3xe5>   kg.Table.Resign   kg.Table.Draw   kg.Table.Rematch   (the table you sit at)
//   kg.Table.Fen <fen>   kg.Table.Reset [Game] [Clock]   kg.Table.List   kg.Table.BotDelay <scale>
//   kg.Table.Perft <depth> [chess|draughts] [fen]      kg.Table.Bench [chess|draughts] [level]
//   kg.TableShot [WxH ...]   off-screen PNGs of the panel (needs -RenderOffScreen): Saved/UIShots/table_*.png
// In PIE the verbs run in the server world's twin of the local player (FKGDev::AuthorityWorld); on a remote client
// only kg.Table.Move / Resign / Draw / Rematch work (they go through the RPC relay).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Character/KGCharacter.h"
#include "Dev/KGDevCommands.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "KillGodot.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "Tabletop/KGBoardTable.h"
#include "Tabletop/KGTableBot.h"
#include "Tabletop/KGTableRules.h"
#include "Tabletop/KGTabletopRPCComponent.h"
#include "Tabletop/SKGTablePanel.h"

namespace KGTabletopDev
{
	EKGTableGame ParseGame(const FString& S, EKGTableGame Default)
	{
		if (S.StartsWith(TEXT("chess"), ESearchCase::IgnoreCase) || S.StartsWith(TEXT("sat"), ESearchCase::IgnoreCase)) return EKGTableGame::Chess;
		if (S.StartsWith(TEXT("dra"), ESearchCase::IgnoreCase) || S.StartsWith(TEXT("check"), ESearchCase::IgnoreCase) || S.StartsWith(TEXT("dam"), ESearchCase::IgnoreCase)) return EKGTableGame::Draughts;
		return Default;
	}

	bool ParseClock(const FString& S, EKGTableClock& Out)
	{
		if (S.StartsWith(TEXT("bul"), ESearchCase::IgnoreCase)) { Out = EKGTableClock::Bullet; return true; }
		if (S.StartsWith(TEXT("bli"), ESearchCase::IgnoreCase)) { Out = EKGTableClock::Blitz; return true; }
		if (S.StartsWith(TEXT("rap"), ESearchCase::IgnoreCase)) { Out = EKGTableClock::Rapid; return true; }
		if (S.StartsWith(TEXT("unt"), ESearchCase::IgnoreCase)) { Out = EKGTableClock::Untimed; return true; }
		return false;
	}

	bool ParseLevel(const FString& S, EKGTableBotLevel& Out)
	{
		if (S.StartsWith(TEXT("app"), ESearchCase::IgnoreCase)) { Out = EKGTableBotLevel::Apprentice; return true; }
		if (S.StartsWith(TEXT("jou"), ESearchCase::IgnoreCase)) { Out = EKGTableBotLevel::Journeyman; return true; }
		if (S.StartsWith(TEXT("mas"), ESearchCase::IgnoreCase) || S.StartsWith(TEXT("ans"), ESearchCase::IgnoreCase)) { Out = EKGTableBotLevel::Master; return true; }
		return false;
	}

	/** The authority-world pawn of the local player (PIE: the server twin by PlayerId). */
	AKGCharacter* AuthorityMe(UWorld* World, UWorld*& OutAuthority)
	{
		OutAuthority = FKGDev::AuthorityWorld(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !OutAuthority)
		{
			return nullptr;
		}
		if (OutAuthority == World)
		{
			return Cast<AKGCharacter>(PC->GetPawn());
		}
		const int32 Id = PC->PlayerState ? PC->PlayerState->GetPlayerId() : INDEX_NONE;
		if (const AGameStateBase* GS = OutAuthority->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (PS && PS->GetPlayerId() == Id)
				{
					return Cast<AKGCharacter>(PS->GetPawn());
				}
			}
		}
		return nullptr;
	}

	AKGBoardTable* Spawn(UWorld* Authority, AKGCharacter* Me, EKGTableGame Game, EKGTableClock Clock)
	{
		if (!Authority || !Me)
		{
			return nullptr;
		}
		const float Yaw = Me->GetActorRotation().Yaw;
		const FVector Forward = FRotator(0, Yaw, 0).Vector();
		const FVector Foot = Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight());
		const FVector At = FKGDev::GroundAt(Authority, FVector2D(Foot + Forward * 250.0f), Foot.Z);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKGBoardTable* Table = Authority->SpawnActor<AKGBoardTable>(AKGBoardTable::StaticClass(), At, FRotator(0, Yaw, 0), Params);
		if (Table)
		{
			Table->ServerReset(Game, Clock);
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_DEV spawned %s at %s"), *Table->GetName(), *At.ToCompactString());
		}
		return Table;
	}

	AKGBoardTable* Nearest(UWorld* Authority, AKGCharacter* Me)
	{
		if (!Me)
		{
			return nullptr;
		}
		if (AKGBoardTable* Mine = AKGBoardTable::FindTableOf(Me))
		{
			return Mine;
		}
		return AKGBoardTable::FindNearest(Authority, Me->GetActorLocation(), 800.0f);
	}

	FAutoConsoleCommandWithWorldAndArgs GSpawn(TEXT("kg.Table.Spawn"), TEXT("kg.Table.Spawn [Chess|Draughts] [Bullet|Blitz|Rapid|Untimed]: a board table in front of you"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* AW;
			AKGCharacter* Me = AuthorityMe(World, AW);
			EKGTableClock Clock = EKGTableClock::Bullet;
			if (Args.Num() > 1) ParseClock(Args[1], Clock);
			Spawn(AW, Me, ParseGame(Args.Num() > 0 ? Args[0] : TEXT(""), EKGTableGame::Chess), Clock);
		}));

	FAutoConsoleCommandWithWorldAndArgs GStart(TEXT("kg.Table.Start"), TEXT("kg.Table.Start [Chess|Draughts] [Apprentice|Journeyman|Master|Human] [Clock]: table + you as White + a bot as Black"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* AW;
			AKGCharacter* Me = AuthorityMe(World, AW);
			EKGTableClock Clock = EKGTableClock::Bullet;
			if (Args.Num() > 2) ParseClock(Args[2], Clock);
			AKGBoardTable* Table = Spawn(AW, Me, ParseGame(Args.Num() > 0 ? Args[0] : TEXT(""), EKGTableGame::Chess), Clock);
			if (!Table)
			{
				return;
			}
			EKGTableBotLevel Level = EKGTableBotLevel::Journeyman;
			const bool bHuman = Args.Num() > 1 && Args[1].StartsWith(TEXT("hum"), ESearchCase::IgnoreCase);
			if (Args.Num() > 1) ParseLevel(Args[1], Level);
			if (!bHuman)
			{
				Table->ServerSetBot(1, true, Level);
			}
			Table->ServerSeat(Me, 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs GBot(TEXT("kg.Table.Bot"), TEXT("kg.Table.Bot [White|Black] [Apprentice|Journeyman|Master|Off]: a bot on the nearest table's seat"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* AW;
			AKGCharacter* Me = AuthorityMe(World, AW);
			AKGBoardTable* Table = Nearest(AW, Me);
			if (!Table)
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_TABLE_DEV no table near you"));
				return;
			}
			const uint8 Colour = (Args.Num() > 0 && Args[0].StartsWith(TEXT("w"), ESearchCase::IgnoreCase)) ? 0 : 1;
			EKGTableBotLevel Level = EKGTableBotLevel::Journeyman;
			const bool bOff = Args.Num() > 1 && Args[1].StartsWith(TEXT("off"), ESearchCase::IgnoreCase);
			if (Args.Num() > 1) ParseLevel(Args[1], Level);
			Table->ServerSetBot(Colour, !bOff, Level);
		}));

	void Relay(UWorld* World, TFunctionRef<void(UKGTabletopRPCComponent&, AKGBoardTable&)> Fn)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AKGCharacter* Me = PC ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr;
		AKGBoardTable* Table = AKGBoardTable::FindTableOf(Me);
		UKGTabletopRPCComponent* Comp = UKGTabletopRPCComponent::FindFor(PC);
		if (!Table || !Comp)
		{
			UE_LOG(LogKillGodot, Warning, TEXT("KG_TABLE_DEV you are not seated at a table (or the relay is missing)"));
			return;
		}
		Fn(*Comp, *Table);
	}

	FAutoConsoleCommandWithWorldAndArgs GMove(TEXT("kg.Table.Move"), TEXT("kg.Table.Move <e2e4|e7e8q|c3xe5xg7>: play a move at the table you sit at"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0) return;
			Relay(World, [&](UKGTabletopRPCComponent& C, AKGBoardTable& T) { C.RequestMove(&T, Args[0]); });
		}));
	FAutoConsoleCommandWithWorldAndArgs GResign(TEXT("kg.Table.Resign"), TEXT("Resign the game you sit at"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			Relay(World, [](UKGTabletopRPCComponent& C, AKGBoardTable& T) { C.RequestAction(&T, EKGTableAction::Resign); });
		}));
	FAutoConsoleCommandWithWorldAndArgs GDraw(TEXT("kg.Table.Draw"), TEXT("Offer / accept a draw at the table you sit at"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			Relay(World, [](UKGTabletopRPCComponent& C, AKGBoardTable& T) { C.RequestAction(&T, EKGTableAction::OfferDraw); });
		}));
	FAutoConsoleCommandWithWorldAndArgs GRematch(TEXT("kg.Table.Rematch"), TEXT("Another game at the table you sit at"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			Relay(World, [](UKGTabletopRPCComponent& C, AKGBoardTable& T) { C.RequestAction(&T, EKGTableAction::Rematch); });
		}));

	FAutoConsoleCommandWithWorldAndArgs GFen(TEXT("kg.Table.Fen"), TEXT("kg.Table.Fen <fen>: set the nearest table's position"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* AW;
			AKGCharacter* Me = AuthorityMe(World, AW);
			AKGBoardTable* Table = Nearest(AW, Me);
			if (Table && !Table->ServerSetPosition(FString::Join(Args, TEXT(" "))))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_TABLE_DEV bad FEN"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReset(TEXT("kg.Table.Reset"), TEXT("kg.Table.Reset [Chess|Draughts] [Clock]: new game on the nearest table"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWorld* AW;
			AKGCharacter* Me = AuthorityMe(World, AW);
			if (AKGBoardTable* Table = Nearest(AW, Me))
			{
				EKGTableClock Clock = Table->ClockPreset;
				if (Args.Num() > 1) ParseClock(Args[1], Clock);
				Table->ServerReset(ParseGame(Args.Num() > 0 ? Args[0] : TEXT(""), Table->Game), Clock);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GList(TEXT("kg.Table.List"), TEXT("Log every board table"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			TArray<AKGBoardTable*> Tables;
			AKGBoardTable::GetAll(FKGDev::AuthorityWorld(World), Tables);
			for (const AKGBoardTable* T : Tables)
			{
				UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_LIST %s at %s status=%s white=%s black=%s clocks=%.0f/%.0f fen=\"%s\""), *T->GetName(),
				       *T->GetActorLocation().ToCompactString(), FKGTableRules::StatusText(T->GetStatus()), *T->GetPlayerName(0), *T->GetPlayerName(1),
				       T->GetClockSeconds(0), T->GetClockSeconds(1), *T->GetFen());
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_LIST %d table(s)"), Tables.Num());
		}));

	FAutoConsoleCommandWithWorldAndArgs GBotDelay(TEXT("kg.Table.BotDelay"), TEXT("kg.Table.BotDelay <scale>: 0 = bots move as soon as they have thought"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			TArray<AKGBoardTable*> Tables;
			AKGBoardTable::GetAll(FKGDev::AuthorityWorld(World), Tables);
			const float Scale = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.0f;
			for (AKGBoardTable* T : Tables)
			{
				T->SetBotDelayScale(Scale);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPerft(TEXT("kg.Table.Perft"), TEXT("kg.Table.Perft <depth> [chess|draughts] [fen]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const int32 Depth = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 3;
			const EKGTableGame Game = ParseGame(Args.Num() > 1 ? Args[1] : TEXT(""), EKGTableGame::Chess);
			FKGBoardState S;
			S.Reset(Game);
			if (Args.Num() > 2)
			{
				TArray<FString> Rest(Args.GetData() + 2, Args.Num() - 2);
				S.FromFen(FString::Join(Rest, TEXT(" ")), Game);
			}
			const double T0 = FPlatformTime::Seconds();
			const int64 Nodes = FKGTableRules::Perft(S, Depth);
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_PERFT depth=%d nodes=%lld secs=%.2f fen=\"%s\""), Depth, Nodes, FPlatformTime::Seconds() - T0, *S.ToFen());
		}));

	FAutoConsoleCommandWithWorldAndArgs GBench(TEXT("kg.Table.Bench"), TEXT("kg.Table.Bench [chess|draughts] [level]: time-sliced search benchmark"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const EKGTableGame Game = ParseGame(Args.Num() > 0 ? Args[0] : TEXT(""), EKGTableGame::Chess);
			EKGTableBotLevel Level = EKGTableBotLevel::Master;
			if (Args.Num() > 1) ParseLevel(Args[1], Level);
			FKGBoardState S;
			S.Reset(Game);
			FKGTableBot Bot;
			Bot.Start(S, Level, 7);
			TArray<double> Slices;
			double Sum = 0.0, Max = 0.0;
			for (;;)
			{
				const double T0 = FPlatformTime::Seconds();
				const bool bDone = Bot.Step(1 << 20, 0.0005);
				const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
				Slices.Add(Ms);
				Sum += Ms;
				Max = FMath::Max(Max, Ms);
				if (bDone) break;
			}
			Slices.Sort();
			const double P99 = Slices[FMath::Clamp(static_cast<int32>(Slices.Num() * 0.99), 0, Slices.Num() - 1)];
			UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_BENCH game=%s level=%d move=%s score=%d nodes=%lld depth=%d slices=%d mean_ms=%.3f p99_ms=%.3f max_ms=%.3f"),
			       Game == EKGTableGame::Chess ? TEXT("chess") : TEXT("draughts"), static_cast<int32>(Level), *Bot.GetBest().ToString(), Bot.GetScore(),
			       Bot.GetNodes(), Bot.GetDepthReached(), Slices.Num(), Sum / Slices.Num(), P99, Max);
		}));

	// ---- kg.TableShot: the panel rendered off screen (kg.ChoreShot technique) ------------------------------------
	bool Shot(const FString& Name, const TSharedRef<FKGTableView>& View, int32 SelectSq, const FIntPoint& Size)
	{
		TSharedRef<SKGTablePanel> Panel = SNew(SKGTablePanel).View(View);
		if (SelectSq >= 0)
		{
			Panel->DebugSelect(SelectSq);
		}
		const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
		const FVector2D DrawSize(Size.X, Size.Y);
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false);
		Target->AddToRoot();
		{
			FWidgetRenderer Renderer(false, true);
			for (int32 Frame = 0; Frame < 3; ++Frame)
			{
				Renderer.DrawWidget(Target, Panel, Scale, DrawSize, 0.016f);
			}
		}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("UIShots");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / FString::Printf(TEXT("table_%s_%dx%d.png"), *Name, Size.X, Size.Y);
		bool bSaved = false;
		if (TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*File)))
		{
			bSaved = FImageUtils::ExportRenderTarget2DAsPNG(Target, *Ar);
		}
		Target->RemoveFromRoot();
		UE_LOG(LogKillGodot, Log, TEXT("KG_TABLESHOT %s %dx%d -> %s %s"), *Name, Size.X, Size.Y, *File, bSaved ? TEXT("ok") : TEXT("FAILED"));
		return bSaved;
	}

	FAutoConsoleCommandWithWorldAndArgs GShot(TEXT("kg.TableShot"), TEXT("kg.TableShot [WxH ...]: off-screen PNGs of the board panel (chess mid-game, mate, draughts, strip)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (!FApp::CanEverRender())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_TABLESHOT needs a renderer (run with -RenderOffScreen, not -nullrhi)"));
				return;
			}
			TArray<FIntPoint> Sizes;
			for (const FString& A : Args)
			{
				FString W, H;
				if (A.Split(TEXT("x"), &W, &H))
				{
					Sizes.Add(FIntPoint(FCString::Atoi(*W), FCString::Atoi(*H)));
				}
			}
			if (Sizes.Num() == 0)
			{
				Sizes.Add(FIntPoint(1280, 720));
			}
			for (const FIntPoint& Size : Sizes)
			{
				// 1 chess, my move, knight selected (legal dots), Blitz clocks
				{
					TSharedRef<FKGTableView> V = MakeShared<FKGTableView>();
					V->Board.FromFen(TEXT("r1bqk2r/1pppbppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1RK1 w kq - 2 6"), EKGTableGame::Chess);
					V->Status = EKGTableStatus::Playing;
					V->MyColour = 0;
					V->Clock[0] = 101.3f;
					V->Clock[1] = 88.0f;
					V->Names[0] = TEXT("Tom");
					V->Names[1] = TEXT("Anselm's pupil");
					V->Place = TEXT("The Latecomer");
					V->ClockName = TEXT("Blitz 3+2");
					V->LastFrom = 61;
					V->LastTo = 52;
					Shot(TEXT("chess_mid"), V, 21, Size);
				}
				// 2 chess, checkmate banner
				{
					TSharedRef<FKGTableView> V = MakeShared<FKGTableView>();
					V->Board.FromFen(TEXT("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4"), EKGTableGame::Chess);
					V->Status = EKGTableStatus::WhiteWins;
					V->Reason = EKGTableEndReason::Checkmate;
					V->MyColour = 1;
					V->Clock[0] = 165.0f;
					V->Clock[1] = 171.5f;
					V->Names[0] = TEXT("Tom");
					V->Names[1] = TEXT("Ada");
					V->Place = TEXT("Fountain Square");
					V->ClockName = TEXT("Blitz 3+2");
					V->LastFrom = 39;
					V->LastTo = 53;
					Shot(TEXT("chess_mate"), V, -1, Size);
				}
				// 3 draughts, a forced capture with two choices, b2 selected
				{
					TSharedRef<FKGTableView> V = MakeShared<FKGTableView>();
					V->Board.Reset(EKGTableGame::Draughts);
					for (const TCHAR* Move : {TEXT("c3d4"), TEXT("f6e5"), TEXT("d4xf6"), TEXT("g7xe5"), TEXT("e3d4"), TEXT("e5xc3")})
					{
						FKGTableMove M;
						if (FKGTableRules::FindLegal(V->Board, Move, M))
						{
							FKGTableRules::Make(V->Board, M);
							V->LastFrom = M.From;
							V->LastTo = M.To;
						}
					}
					V->Status = EKGTableStatus::Playing;
					V->MyColour = 0;
					V->Clock[0] = 52.0f;
					V->Clock[1] = 47.2f;
					V->Names[0] = TEXT("Tom");
					V->Names[1] = TEXT("Anselm's pupil");
					V->Place = TEXT("The Latecomer");
					V->ClockName = TEXT("Bullet 1+1");
					Shot(TEXT("draughts_mid"), V, 9, Size);
				}
				// 4 spectator strip
				{
					TSharedRef<FKGTableView> V = MakeShared<FKGTableView>();
					V->Board.Reset(EKGTableGame::Chess);
					V->bStrip = true;
					V->Status = EKGTableStatus::Playing;
					V->Clock[0] = 31.0f;
					V->Clock[1] = 28.4f;
					V->Names[0] = TEXT("Tom");
					V->Names[1] = TEXT("Ada");
					V->ClockName = TEXT("Bullet 1+1");
					Shot(TEXT("strip"), V, -1, Size);
				}
			}
		}));
}

#endif
