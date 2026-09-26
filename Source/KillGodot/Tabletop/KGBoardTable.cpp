#include "Tabletop/KGBoardTable.h"

#include "Character/KGCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "Tabletop/KGTableRules.h"
#include "UObject/ConstructorHelpers.h"
#include "World/KGSeat.h"

namespace KGTablePrivate
{
	const TCHAR* StoolPath = TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Stool.Stool");
	const TCHAR* TablePath = TEXT("/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_Square_Table.Kitchen_Square_Table");
	constexpr float NetCullMetres = 40.0f;
	constexpr float ClockPushSeconds = 0.5f;   // clock-only replication rate (clients extrapolate)

	FLinearColor Colour(uint32 RGB)
	{
		return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
	}

	/** Placeholder piece: cylinder radius / height in cm (Docs/Design section 7: real pieces come with 036c). */
	void PieceShape(EKGTableGame Game, uint8 Type, float& OutRadius, float& OutHeight)
	{
		if (Game == EKGTableGame::Draughts)
		{
			OutRadius = 1.9f;
			OutHeight = Type == KGPiece::DKing ? 1.8f : 0.8f;
			return;
		}
		switch (Type)
		{
		case KGPiece::Pawn: OutRadius = 1.4f; OutHeight = 3.0f; break;
		case KGPiece::Knight: OutRadius = 1.6f; OutHeight = 4.0f; break;
		case KGPiece::Bishop: OutRadius = 1.5f; OutHeight = 4.6f; break;
		case KGPiece::Rook: OutRadius = 1.8f; OutHeight = 3.8f; break;
		case KGPiece::Queen: OutRadius = 1.8f; OutHeight = 5.6f; break;
		default: OutRadius = 1.9f; OutHeight = 6.4f; break;
		}
	}
}

AKGBoardTable::AKGBoardTable()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = false;
	SetNetCullDistanceSquared(FMath::Square(KGTablePrivate::NetCullMetres * 100.0f));
	SetNetUpdateFrequency(10.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(Root);
	TableMesh->SetCollisionProfileName(TEXT("BlockAll"));
	TableMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));   // furniture faces its local +Y

	ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto MakeIsm = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Ism = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Ism->SetupAttachment(Root);
		Ism->SetStaticMesh(Mesh);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCastShadow(false);
		Ism->SetCullDistances(0, 2500);   // pieces vanish at 25 m (section 8)
		return Ism;
	};
	LightSquares = MakeIsm(TEXT("LightSquares"), Cube.Object);
	DarkSquares = MakeIsm(TEXT("DarkSquares"), Cube.Object);
	WhitePieces = MakeIsm(TEXT("WhitePieces"), Cylinder.Object);
	BlackPieces = MakeIsm(TEXT("BlackPieces"), Cylinder.Object);
	Snapshot = CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
	PlaceName = NSLOCTEXT("KGTable", "DefaultPlace", "Morrowmere");
}

void AKGBoardTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(AKGBoardTable, Net, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AKGBoardTable, PlayerWhite);
	DOREPLIFETIME(AKGBoardTable, PlayerBlack);
	DOREPLIFETIME(AKGBoardTable, SeatWhite);
	DOREPLIFETIME(AKGBoardTable, SeatBlack);
}

void AKGBoardTable::BeginPlay()
{
	Super::BeginPlay();
	TableSeed = FCrc::StrCrc32(*GetName());
	if (bSpawnTableMesh && !TableMesh->GetStaticMesh())
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, KGTablePrivate::TablePath))
		{
			TableMesh->SetStaticMesh(Mesh);
		}
	}
	if (HasAuthority())
	{
		SpawnFurniture();
		if (!Fen.IsEmpty() && Board.FromFen(Fen, Game))
		{
			// Restored from a snapshot: the game goes on from the saved position.
		}
		else
		{
			Board.Reset(Game);
		}
		Net.Game = Game;
		Net.Clock = ClockPreset;
		PushNet();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		// Board squares (once) and the material set: cream / ink palette, brass for the pieces' rims.
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		const uint32 Colours[4] = {0xE8D8B4, 0x3A2B2A, 0xF6E7C8, 0x1D1524};
		UInstancedStaticMeshComponent* Targets[4] = {LightSquares, DarkSquares, WhitePieces, BlackPieces};
		for (int32 i = 0; i < 4; ++i)
		{
			if (Base)
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
				Mid->SetVectorParameterValue(TEXT("Color"), KGTablePrivate::Colour(Colours[i]));
				Materials.Add(Mid);
				Targets[i]->SetMaterial(0, Mid);
			}
		}
		const FVector Scale(SquareSize / 100.0f, SquareSize / 100.0f, 0.01f);
		for (int32 Sq = 0; Sq < 64; ++Sq)
		{
			const FVector Local = SquareCentre(Sq) - FVector(0.0f, 0.0f, 0.5f);
			UInstancedStaticMeshComponent* Ism = ((KGPiece::FileOf(Sq) + KGPiece::RankOf(Sq)) % 2 == 0) ? DarkSquares : LightSquares;
			Ism->AddInstance(FTransform(FRotator::ZeroRotator, Local, Scale), false);
		}
		RebuildPieces();
	}
}

void AKGBoardTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AKGBoardTable::SpawnFurniture()
{
	if (!bSpawnSeats || SeatWhite || SeatBlack)
	{
		return;
	}
	UStaticMesh* Stool = LoadObject<UStaticMesh>(nullptr, KGTablePrivate::StoolPath);
	const FTransform T = GetActorTransform();
	for (int32 Colour = 0; Colour < 2; ++Colour)
	{
		const FVector Local(Colour == 0 ? -SeatDistance : SeatDistance, 0.0f, 0.0f);
		const FRotator Rot(0.0f, T.Rotator().Yaw + (Colour == 0 ? 0.0f : 180.0f), 0.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		AKGSeat* Seat = GetWorld()->SpawnActor<AKGSeat>(AKGSeat::StaticClass(), T.TransformPosition(Local), Rot, Params);
		if (!Seat)
		{
			continue;
		}
		Seat->SeatHeight = 58.0f;
		Seat->MeshRotation = FRotator(0.0f, -90.0f, 0.0f);
		Seat->StandDistance = 60.0f;
		if (Stool)
		{
			Seat->SetSeatMesh(Stool);
		}
		(Colour == 0 ? SeatWhite : SeatBlack) = Seat;
	}
}

FVector AKGBoardTable::SquareCentre(int32 Sq) const
{
	// White sits at -X looking +X: ranks run along +X, files along +Y (a-file on White's left = -Y).
	return FVector((KGPiece::RankOf(Sq) - 3.5f) * SquareSize, (KGPiece::FileOf(Sq) - 3.5f) * SquareSize, BoardHeight);
}

void AKGBoardTable::RebuildPieces()
{
	if (!WhitePieces || !BlackPieces)
	{
		return;
	}
	WhitePieces->ClearInstances();
	BlackPieces->ClearInstances();
	for (int32 Sq = 0; Sq < 64; ++Sq)
	{
		const uint8 P = Board.Sq[Sq];
		if (!P)
		{
			continue;
		}
		float Radius, Height;
		KGTablePrivate::PieceShape(Board.Game, KGPiece::TypeOf(P), Radius, Height);
		const FVector Scale(Radius * 2.0f / 100.0f, Radius * 2.0f / 100.0f, Height / 100.0f);
		const FVector Local = SquareCentre(Sq) + FVector(0.0f, 0.0f, Height * 0.5f);
		(KGPiece::IsBlack(P) ? BlackPieces : WhitePieces)->AddInstance(FTransform(FRotator::ZeroRotator, Local, Scale), false);
	}
	LastDrawnSerial = Net.Serial;
}

void AKGBoardTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	else
	{
		ClockAccum += DeltaSeconds;
	}
	if (GetNetMode() != NM_DedicatedServer && LastDrawnSerial != Net.Serial)
	{
		RebuildPieces();
	}
}

float AKGBoardTable::GetClockSeconds(uint8 Colour) const
{
	const float Base = ClockSeconds[Colour & 1];
	if (HasAuthority() || Net.Status != EKGTableStatus::Playing || Board.Side != (Colour & 1) || Net.Clock == EKGTableClock::Untimed)
	{
		return Base;
	}
	return FMath::Max(0.0f, Base - ClockAccum);
}

// ---- replication --------------------------------------------------------------------------------------------------

void AKGBoardTable::PushNet()
{
	Board.Pack(Net.Packed);
	Net.Game = Board.Game;
	Net.Side = Board.Side;
	Net.Castling = Board.Castling;
	Net.Ep = Board.Ep;
	Net.HalfClock = Board.HalfClock;
	Net.Ply = Board.Ply;
	Net.Clock = ClockPreset;
	Net.ClockW = static_cast<int16>(FMath::Clamp(FMath::CeilToInt(ClockSeconds[0] * 10.0f), 0, 32000));
	Net.ClockB = static_cast<int16>(FMath::Clamp(FMath::CeilToInt(ClockSeconds[1] * 10.0f), 0, 32000));
	++Net.Serial;
	Fen = Board.ToFen();
	ClockAccum = 0.0f;
	ForceNetUpdate();
}

void AKGBoardTable::OnRep_Net()
{
	Board.Game = Net.Game;
	Board.Unpack(Net.Packed);
	Board.Side = Net.Side;
	Board.Castling = Net.Castling;
	Board.Ep = Net.Ep;
	Board.HalfClock = Net.HalfClock;
	Board.Ply = Net.Ply;
	ClockSeconds[0] = Net.ClockW * 0.1f;
	ClockSeconds[1] = Net.ClockB * 0.1f;
	ClockAccum = 0.0f;
}

// ---- server -------------------------------------------------------------------------------------------------------

void AKGBoardTable::Log(const TCHAR* Act, const FString& Detail) const
{
	UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE table=%s game=%s act=%s %s"), *GetName(),
	       Board.Game == EKGTableGame::Chess ? TEXT("Chess") : TEXT("Draughts"), Act, *Detail);
}

bool AKGBoardTable::IsOpenPhase() const
{
	const AKGGameState* GS = GetWorld() ? GetWorld()->GetGameState<AKGGameState>() : nullptr;
	if (!GS)
	{
		return true;
	}
	switch (GS->GetPhase())
	{
	case EKGPhase::Lobby:
	case EKGPhase::Warmup:
	case EKGPhase::Day:
	case EKGPhase::Epilogue:
		return true;
	default:
		return false;
	}
}

APlayerState* AKGBoardTable::PlayerOnSeat(uint8 Colour) const
{
	const AKGSeat* Seat = GetSeat(Colour);
	const AKGCharacter* Who = Seat ? Seat->GetOccupant() : nullptr;
	return Who ? Who->GetPlayerState() : nullptr;
}

int32 AKGBoardTable::ColourOf(const APlayerState* Who) const
{
	if (!Who)
	{
		return -1;
	}
	return PlayerWhite == Who ? 0 : PlayerBlack == Who ? 1 : -1;
}

int32 AKGBoardTable::ColourOfCharacter(const AKGCharacter* Who) const
{
	if (!Who)
	{
		return -1;
	}
	if (SeatWhite && SeatWhite->GetOccupant() == Who)
	{
		return 0;
	}
	if (SeatBlack && SeatBlack->GetOccupant() == Who)
	{
		return 1;
	}
	return -1;
}

FString AKGBoardTable::GetPlayerName(uint8 Colour) const
{
	if (IsBot(Colour))
	{
		return TEXT("Anselm's pupil");
	}
	const APlayerState* PS = GetPlayer(Colour);
	return PS ? PS->GetPlayerName() : FString(TEXT("-"));
}

bool AKGBoardTable::ServerSeat(AKGCharacter* Who, uint8 Colour)
{
	AKGSeat* Seat = GetSeat(Colour);
	if (!HasAuthority() || !Seat || !Who)
	{
		return false;
	}
	if (Seat->GetOccupant() == Who)
	{
		return true;
	}
	AKGSeat::StandUpCharacter(Who);
	return Seat->Sit(Who);
}

void AKGBoardTable::SetStatus(EKGTableStatus Status, EKGTableEndReason Reason, const TCHAR* Why)
{
	Net.Status = Status;
	Net.Reason = Reason;
	if (Status != EKGTableStatus::Playing && Status != EKGTableStatus::Adjourned)
	{
		Bot = FKGTableBot();
		BotWait = -1.0f;
	}
	Log(TEXT("status"), FString::Printf(TEXT("status=%s reason=%s why=%s moves=%d fen=\"%s\""), FKGTableRules::StatusText(Status),
	                                    FKGTableRules::ReasonText(Reason), Why, MovesPlayed, *Board.ToFen()));
	PushNet();
}

void AKGBoardTable::StartGame()
{
	Board.Reset(Game);
	History.Reset();
	History.Add(Board.Hash());
	float Base, Inc;
	FKGTableRules::ClockPreset(ClockPreset, Base, Inc);
	ClockSeconds[0] = ClockSeconds[1] = Base;
	MovesPlayed = 0;
	Net.DrawOffers = 0;
	Net.LastFrom = Net.LastTo = 255;
	BotBadStreak = 0;
	Bot = FKGTableBot();
	BotWait = -1.0f;
	auto Puid = [this](uint8 Colour)
	{
		if (IsBot(Colour))
		{
			return FString(TEXT("BOT"));
		}
		const AKGPlayerState* PS = Cast<AKGPlayerState>(GetPlayer(Colour));
		return PS ? (PS->Puid.IsEmpty() ? PS->GetPlayerName() : PS->Puid) : FString();
	};
	PuidWhite = Puid(0);
	PuidBlack = Puid(1);
	SetStatus(EKGTableStatus::Playing, EKGTableEndReason::None,
	          *FString::Printf(TEXT("start white=%s black=%s clock=%s"), *GetPlayerName(0), *GetPlayerName(1), FKGTableRules::ClockText(ClockPreset)));
}

void AKGBoardTable::UpdateSeats()
{
	APlayerState* Cur[2] = {PlayerOnSeat(0), PlayerOnSeat(1)};
	bool bChanged[2] = {false, false};
	for (int32 C = 0; C < 2; ++C)
	{
		TObjectPtr<APlayerState>& Slot = C == 0 ? PlayerWhite : PlayerBlack;
		if (Cur[C] && IsBot(C))
		{
			Net.Bots &= ~(1 << C);   // a human took the bot's seat
			bChanged[C] = true;
		}
		if (Slot != Cur[C])
		{
			Log(Cur[C] ? TEXT("sit") : TEXT("stand"), FString::Printf(TEXT("colour=%s who=%s"), C == 0 ? TEXT("white") : TEXT("black"),
			                                                           Cur[C] ? *Cur[C]->GetPlayerName() : (Slot ? *Slot->GetPlayerName() : TEXT("?"))));
			Slot = Cur[C];
			bChanged[C] = true;
		}
	}
	const bool bPresent[2] = {IsBot(0) || Cur[0] != nullptr, IsBot(1) || Cur[1] != nullptr};
	const bool bOpen = IsOpenPhase();
	auto SamePlayers = [&]()
	{
		auto Matches = [&](uint8 C, const FString& Recorded)
		{
			if (IsBot(C))
			{
				return Recorded == TEXT("BOT");
			}
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Cur[C]);
			return PS && (PS->Puid.IsEmpty() ? PS->GetPlayerName() : PS->Puid) == Recorded;
		};
		return Matches(0, PuidWhite) && Matches(1, PuidBlack);
	};

	switch (Net.Status)
	{
	case EKGTableStatus::Waiting:
		if (bPresent[0] && bPresent[1] && bOpen)
		{
			StartGame();
		}
		break;
	case EKGTableStatus::Playing:
		if (!bPresent[0] || !bPresent[1])
		{
			SetStatus(EKGTableStatus::Adjourned, EKGTableEndReason::None, TEXT("stood up"));
		}
		else if (!bOpen)
		{
			SetStatus(EKGTableStatus::Adjourned, EKGTableEndReason::None, TEXT("phase"));
		}
		break;
	case EKGTableStatus::Adjourned:
		if ((bChanged[0] && Cur[0]) || (bChanged[1] && Cur[1]))
		{
			if (!SamePlayers() && bPresent[0] && bPresent[1])
			{
				SetStatus(EKGTableStatus::Waiting, EKGTableEndReason::Abandoned, TEXT("new player"));
				break;
			}
		}
		if (bPresent[0] && bPresent[1] && bOpen && SamePlayers())
		{
			SetStatus(EKGTableStatus::Playing, EKGTableEndReason::None, TEXT("resumed"));
		}
		break;
	default:
		// A result is on the board: any seat change clears it for the next game.
		if (bChanged[0] || bChanged[1])
		{
			SetStatus(EKGTableStatus::Waiting, EKGTableEndReason::None, TEXT("seat change after result"));
		}
		break;
	}
	if (bChanged[0] || bChanged[1])
	{
		ForceNetUpdate();
	}
}

void AKGBoardTable::ServerTick(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	UpdateSeats();
	if (Net.Status != EKGTableStatus::Playing)
	{
		return;
	}
	// Clock of the side to move (a paused phase never reaches here: UpdateSeats adjourns it).
	if (ClockPreset != EKGTableClock::Untimed)
	{
		const uint8 Side = Board.Side;
		ClockSeconds[Side] = FMath::Max(0.0f, ClockSeconds[Side] - DeltaSeconds);
		if (ClockSeconds[Side] <= 0.0f)
		{
			const uint8 Other = 1 - Side;
			if (Board.Game == EKGTableGame::Chess && !FKGChessRules::HasMatingMaterial(Board, Other))
			{
				SetStatus(EKGTableStatus::Draw, EKGTableEndReason::Flag, TEXT("flag, no mating material"));
			}
			else
			{
				SetStatus(Other == 0 ? EKGTableStatus::WhiteWins : EKGTableStatus::BlackWins, EKGTableEndReason::Flag, TEXT("flag"));
			}
			return;
		}
		ClockAccum += DeltaSeconds;
		if (ClockAccum >= KGTablePrivate::ClockPushSeconds)
		{
			PushNet();
		}
	}
	// The bot's move lands after a human-like pause (the search itself ran in ServerTickBot slices).
	if (IsBot(Board.Side) && Bot.HasMove())
	{
		BotWait -= DeltaSeconds;
		if (BotWait <= 0.0f)
		{
			const bool bBad = Board.Game == EKGTableGame::Chess && Bot.GetScore() <= -900;
			BotBadStreak = bBad ? BotBadStreak + 1 : 0;
			if (BotBadStreak >= 3)
			{
				Log(TEXT("resign"), FString::Printf(TEXT("by=bot colour=%d score=%d"), Board.Side, Bot.GetScore()));
				SetStatus(Board.Side == 0 ? EKGTableStatus::BlackWins : EKGTableStatus::WhiteWins, EKGTableEndReason::Resign, TEXT("bot resigns"));
				return;
			}
			const FKGTableMove M = Bot.GetBest();
			Bot = FKGTableBot();
			ApplyMove(M, TEXT("bot"));
		}
	}
}

void AKGBoardTable::ServerTickBot(double SliceSeconds)
{
	if (!HasAuthority() || Net.Status != EKGTableStatus::Playing || !IsBot(Board.Side) || Bot.HasMove())
	{
		return;
	}
	if (!Bot.IsSearching())
	{
		// A standing draw offer from the opponent: accept when the position is not clearly better for us.
		if ((Net.DrawOffers & (1 << (1 - Board.Side))) && FKGTableBot::Evaluate(Board) <= 50)
		{
			Log(TEXT("draw"), TEXT("by=bot accept"));
			SetStatus(EKGTableStatus::Draw, EKGTableEndReason::Agreement, TEXT("bot accepted the draw"));
			return;
		}
		Bot.Start(Board, BotLevel[Board.Side], (static_cast<uint64>(TableSeed) << 32) ^ Board.Hash() ^ Board.Ply);
	}
	if (Bot.Step(1 << 20, SliceSeconds))
	{
		const float Remaining = ClockSeconds[Board.Side];
		const float Pause = ClockPreset == EKGTableClock::Untimed ? 1.2f : FMath::Clamp(Remaining * 0.05f, 0.4f, 1.5f);
		BotWait = Pause * BotDelayScale;
		Log(TEXT("think"), FString::Printf(TEXT("colour=%d move=%s score=%d nodes=%lld depth=%d pause=%.2f"), Board.Side,
		                                   *Bot.GetBest().ToString(), Bot.GetScore(), Bot.GetNodes(), Bot.GetDepthReached(), BotWait));
	}
}

void AKGBoardTable::ApplyMove(const FKGTableMove& M, const TCHAR* By)
{
	const uint8 Mover = Board.Side;
	FKGTableRules::Make(Board, M);
	if (Board.HalfClock == 0)
	{
		History.Reset();   // irreversible: earlier positions can never repeat
	}
	History.Add(Board.Hash());
	if (History.Num() > 150)
	{
		History.RemoveAt(0, History.Num() - 150, EAllowShrinking::No);
	}
	++MovesPlayed;
	float Base, Inc;
	FKGTableRules::ClockPreset(ClockPreset, Base, Inc);
	if (ClockPreset != EKGTableClock::Untimed)
	{
		ClockSeconds[Mover] += Inc;
	}
	Net.LastFrom = M.From;
	Net.LastTo = M.To;
	Net.DrawOffers &= static_cast<uint8>(1 << Mover);   // the opponent's offer lapses with our move
	Log(TEXT("move"), FString::Printf(TEXT("by=%s colour=%d move=%s ply=%d fen=\"%s\""), By, Mover, *M.ToString(), Board.Ply, *Board.ToFen()));
	CheckVerdict();
}

void AKGBoardTable::CheckVerdict()
{
	const FKGTableVerdict V = FKGTableRules::Evaluate(Board, History);
	if (V.IsOver())
	{
		SetStatus(V.Status, V.Reason, TEXT("rules"));
	}
	else
	{
		PushNet();
	}
}

bool AKGBoardTable::ServerTryMove(APlayerState* Who, const FString& MoveText, FString& OutError)
{
	auto Refuse = [&](const TCHAR* Why)
	{
		OutError = Why;
		Log(TEXT("reject"), FString::Printf(TEXT("by=%s move=%s why=%s"), Who ? *Who->GetPlayerName() : TEXT("?"), *MoveText, Why));
		return false;
	};
	if (!HasAuthority())
	{
		return Refuse(TEXT("not authority"));
	}
	UpdateSeats();
	const int32 Colour = ColourOf(Who);
	if (Colour < 0)
	{
		return Refuse(TEXT("not seated"));
	}
	if (Net.Status != EKGTableStatus::Playing)
	{
		return Refuse(Net.Status == EKGTableStatus::Adjourned ? TEXT("adjourned") : TEXT("no game"));
	}
	if (!IsOpenPhase())
	{
		return Refuse(TEXT("tables closed"));
	}
	if (Colour != Board.Side)
	{
		return Refuse(TEXT("not your turn"));
	}
	FKGTableMove M;
	if (!FKGTableRules::FindLegal(Board, MoveText, M))
	{
		return Refuse(TEXT("illegal"));
	}
	ApplyMove(M, *Who->GetPlayerName());
	return true;
}

bool AKGBoardTable::ServerResign(APlayerState* Who)
{
	const int32 Colour = ColourOf(Who);
	if (!HasAuthority() || Colour < 0 || (Net.Status != EKGTableStatus::Playing && Net.Status != EKGTableStatus::Adjourned))
	{
		return false;
	}
	Log(TEXT("resign"), FString::Printf(TEXT("by=%s colour=%d"), *Who->GetPlayerName(), Colour));
	SetStatus(Colour == 0 ? EKGTableStatus::BlackWins : EKGTableStatus::WhiteWins, EKGTableEndReason::Resign, TEXT("resigned"));
	return true;
}

bool AKGBoardTable::ServerOfferDraw(APlayerState* Who)
{
	const int32 Colour = ColourOf(Who);
	if (!HasAuthority() || Colour < 0 || Net.Status != EKGTableStatus::Playing)
	{
		return false;
	}
	if (Net.DrawOffers & (1 << (1 - Colour)))
	{
		Log(TEXT("draw"), FString::Printf(TEXT("by=%s accept"), *Who->GetPlayerName()));
		SetStatus(EKGTableStatus::Draw, EKGTableEndReason::Agreement, TEXT("agreed"));
		return true;
	}
	Net.DrawOffers |= static_cast<uint8>(1 << Colour);
	Log(TEXT("draw"), FString::Printf(TEXT("by=%s offer"), *Who->GetPlayerName()));
	PushNet();
	return true;
}

bool AKGBoardTable::ServerRematch(APlayerState* Who)
{
	if (!HasAuthority() || ColourOf(Who) < 0 || Net.Status == EKGTableStatus::Playing || Net.Status == EKGTableStatus::Adjourned)
	{
		return false;
	}
	SetStatus(EKGTableStatus::Waiting, EKGTableEndReason::None, TEXT("rematch"));
	UpdateSeats();
	return true;
}

void AKGBoardTable::ServerSetBot(uint8 Colour, bool bBot, EKGTableBotLevel Level)
{
	if (!HasAuthority() || Colour > 1)
	{
		return;
	}
	if (bBot)
	{
		Net.Bots |= static_cast<uint8>(1 << Colour);
		BotLevel[Colour] = Level;
	}
	else
	{
		Net.Bots &= ~static_cast<uint8>(1 << Colour);
	}
	Log(TEXT("bot"), FString::Printf(TEXT("colour=%d on=%d level=%d"), Colour, bBot ? 1 : 0, static_cast<int32>(Level)));
	if (Net.Status == EKGTableStatus::Playing || Net.Status == EKGTableStatus::Adjourned)
	{
		SetStatus(EKGTableStatus::Waiting, EKGTableEndReason::Abandoned, TEXT("bot changed"));
	}
	else
	{
		PushNet();
	}
	UpdateSeats();
}

void AKGBoardTable::ServerReset(EKGTableGame NewGame, EKGTableClock Clock)
{
	if (!HasAuthority())
	{
		return;
	}
	Game = NewGame;
	ClockPreset = Clock;
	Board.Reset(Game);
	History.Reset();
	SetStatus(EKGTableStatus::Waiting, EKGTableEndReason::None, TEXT("reset"));
	UpdateSeats();
}

bool AKGBoardTable::ServerSetPosition(const FString& InFen)
{
	FKGBoardState Test;
	if (!HasAuthority() || !Test.FromFen(InFen, Game))
	{
		return false;
	}
	Board = Test;
	History.Reset();
	History.Add(Board.Hash());
	Net.LastFrom = Net.LastTo = 255;
	Log(TEXT("position"), FString::Printf(TEXT("fen=\"%s\""), *Board.ToFen()));
	if (Net.Status == EKGTableStatus::Playing)
	{
		CheckVerdict();
	}
	else
	{
		PushNet();
	}
	return true;
}

// ---- statics --------------------------------------------------------------------------------------------------------

AKGBoardTable* AKGBoardTable::FindTableOf(const AKGCharacter* Who)
{
	const AKGSeat* Seat = Who ? AKGSeat::FindSeatOf(Who) : nullptr;
	if (!Seat)
	{
		return nullptr;
	}
	for (TActorIterator<AKGBoardTable> It(Who->GetWorld()); It; ++It)
	{
		if (It->SeatWhite == Seat || It->SeatBlack == Seat)
		{
			return *It;
		}
	}
	return nullptr;
}

AKGBoardTable* AKGBoardTable::FindNearest(UWorld* World, const FVector& From, float MaxDistance)
{
	AKGBoardTable* Best = nullptr;
	float BestDist = MaxDistance;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKGBoardTable> It(World); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), From);
		if (D <= BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void AKGBoardTable::GetAll(UWorld* World, TArray<AKGBoardTable*>& Out)
{
	Out.Reset();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AKGBoardTable> It(World); It; ++It)
	{
		Out.Add(*It);
	}
}
