#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tabletop/KGBoardTypes.h"
#include "Tabletop/KGTableBot.h"
#include "KGBoardTable.generated.h"

class AKGCharacter;
class AKGSeat;
class APlayerState;
class UInstancedStaticMeshComponent;
class UKGSnapshotComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Everything a client needs to draw the game: ~44 bytes, replicated as one block on every change. */
USTRUCT()
struct FKGTableNet
{
	GENERATED_BODY()

	UPROPERTY() TArray<uint8> Packed;   // 32 bytes, FKGBoardState::Pack
	UPROPERTY() EKGTableGame Game = EKGTableGame::Chess;
	UPROPERTY() uint8 Side = 0;
	UPROPERTY() uint8 Castling = 0;
	UPROPERTY() int8 Ep = -1;
	UPROPERTY() uint16 HalfClock = 0;
	UPROPERTY() uint16 Ply = 0;
	UPROPERTY() uint8 LastFrom = 255;
	UPROPERTY() uint8 LastTo = 255;
	UPROPERTY() EKGTableStatus Status = EKGTableStatus::Waiting;
	UPROPERTY() EKGTableEndReason Reason = EKGTableEndReason::None;
	UPROPERTY() EKGTableClock Clock = EKGTableClock::Bullet;
	/** Deciseconds left, white / black. */
	UPROPERTY() int16 ClockW = 0;
	UPROPERTY() int16 ClockB = 0;
	/** Bit 1 white offers a draw, bit 2 black. */
	UPROPERTY() uint8 DrawOffers = 0;
	/** Bit 1 white seat is a bot, bit 2 black. */
	UPROPERTY() uint8 Bots = 0;
	/** Serial bumped on every change (OnRep with REPNOTIFY_Always still needs a cheap "did it change"). */
	UPROPERTY() uint16 Serial = 0;
};

/**
 * SPRINT-036: a board table in the world (Docs/Design/Tabletop_Games.md). Two AKGSeat seats face each other over
 * the board; whoever sits on the white seat plays White. The server owns the rules (FKGTableRules), the clock
 * (Fischer presets, advanced from Tick deltas: no TimerManager) and the bot (FKGTableBot, time-sliced by
 * UKGTabletopSubsystem). Clients receive FKGTableNet and draw the placeholder pieces (instanced engine shapes) and
 * the 2D panel (SKGTablePanel). Moves arrive through UKGTabletopRPCComponent on the player's PlayerState.
 *
 * Game flow: Waiting (a seat is empty) -> Playing (both seats have a player or a bot) -> a result, or Adjourned
 * when a player stands up or the match phase closes the tables (Meeting, Trial, Night, the ceremonies). The same
 * player sitting down again resumes; a different player starts a new game. After a result any seat change starts
 * a new game.
 */
UCLASS()
class KILLGODOT_API AKGBoardTable : public AActor
{
	GENERATED_BODY()

public:
	AKGBoardTable();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- configuration ------------------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Table")
	EKGTableGame Game = EKGTableGame::Chess;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Table")
	EKGTableClock ClockPreset = EKGTableClock::Bullet;

	/** Shown in the panel header ("The Latecomer", "Fountain Square"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table")
	FText PlaceName;

	/** Assigned by the dressing; when null (and bSpawnSeats) the server spawns two stools at BeginPlay. */
	UPROPERTY(EditAnywhere, Replicated, Category = "Table")
	TObjectPtr<AKGSeat> SeatWhite;

	UPROPERTY(EditAnywhere, Replicated, Category = "Table")
	TObjectPtr<AKGSeat> SeatBlack;

	UPROPERTY(EditAnywhere, Category = "Table")
	bool bSpawnSeats = true;

	/** Also spawn the kitchen table mesh under the board (dev spawns; the dressing brings its own). */
	UPROPERTY(EditAnywhere, Category = "Table")
	bool bSpawnTableMesh = true;

	UPROPERTY(EditAnywhere, Category = "Table")
	float SeatDistance = 70.0f;

	/** Height of the board surface above the actor origin (Kitchen_Square_Table top ~67). */
	UPROPERTY(EditAnywhere, Category = "Table")
	float BoardHeight = 68.0f;

	UPROPERTY(EditAnywhere, Category = "Table")
	float SquareSize = 5.0f;

	// ---- server API (authority) ---------------------------------------------------------------------------------
	/** Validates seat, turn, phase and legality; applies the move. OutError names the refusal (logged as KG_TABLE). */
	bool ServerTryMove(APlayerState* Who, const FString& MoveText, FString& OutError);
	bool ServerResign(APlayerState* Who);
	bool ServerOfferDraw(APlayerState* Who);
	/** After a result: starts a new game with the same players (either player may ask). */
	bool ServerRematch(APlayerState* Who);
	/** A virtual player on the given colour (0 white, 1 black). */
	void ServerSetBot(uint8 Colour, bool bBot, EKGTableBotLevel Level = EKGTableBotLevel::Journeyman);
	/** New game (dev / smoke): game type, clock. Players keep their seats. */
	void ServerReset(EKGTableGame NewGame, EKGTableClock Clock);
	/** Dev: sets a position (FEN) and continues the game from it. */
	bool ServerSetPosition(const FString& Fen);
	/** Dev / tests: the bot moves instantly instead of the human-like pause. */
	void SetBotDelayScale(float Scale) { BotDelayScale = Scale; }
	/** Called by UKGTabletopSubsystem with this frame's share of the bot budget. */
	void ServerTickBot(double SliceSeconds);
	/** Authority: server-side per-frame work (Tick calls it; tests call it directly). */
	void ServerTick(float DeltaSeconds);
	/** Sits Who on the seat of Colour (authority; convenience for verbs and smokes). */
	bool ServerSeat(AKGCharacter* Who, uint8 Colour);

	// ---- queries (any machine) ----------------------------------------------------------------------------------
	const FKGBoardState& GetBoard() const { return Board; }
	const FKGTableNet& GetNet() const { return Net; }
	EKGTableStatus GetStatus() const { return Net.Status; }
	EKGTableEndReason GetEndReason() const { return Net.Reason; }
	bool IsPlaying() const { return Net.Status == EKGTableStatus::Playing; }
	/** 0 white, 1 black, -1 not seated here. */
	int32 ColourOf(const APlayerState* Who) const;
	int32 ColourOfCharacter(const AKGCharacter* Who) const;
	APlayerState* GetPlayer(uint8 Colour) const { return Colour == 0 ? PlayerWhite : PlayerBlack; }
	AKGSeat* GetSeat(uint8 Colour) const { return Colour == 0 ? SeatWhite : SeatBlack; }
	bool IsBot(uint8 Colour) const { return (Net.Bots & (1 << Colour)) != 0; }
	/** Seconds left; clients extrapolate the running clock between replication pushes. */
	float GetClockSeconds(uint8 Colour) const;
	FString GetPlayerName(uint8 Colour) const;
	/** Tables open in Lobby, Warmup, Day and Epilogue (Docs/Design/Tabletop_Games.md section 3.3). */
	bool IsOpenPhase() const;
	int32 GetLastFrom() const { return Net.LastFrom == 255 ? -1 : Net.LastFrom; }
	int32 GetLastTo() const { return Net.LastTo == 255 ? -1 : Net.LastTo; }
	uint16 GetSerial() const { return Net.Serial; }
	const TArray<uint64>& GetHistory() const { return History; }
	FString GetFen() const { return Board.ToFen(); }
	int32 GetMoveCount() const { return MovesPlayed; }

	static AKGBoardTable* FindTableOf(const AKGCharacter* Who);
	static AKGBoardTable* FindNearest(UWorld* World, const FVector& From, float MaxDistance);
	static void GetAll(UWorld* World, TArray<AKGBoardTable*>& Out);

protected:
	UFUNCTION()
	void OnRep_Net();

	void SpawnFurniture();
	void StartGame();
	void SetStatus(EKGTableStatus Status, EKGTableEndReason Reason, const TCHAR* Why);
	void ApplyMove(const FKGTableMove& M, const TCHAR* By);
	void CheckVerdict();
	void PushNet();
	void UpdateSeats();
	void RebuildPieces();
	FVector SquareCentre(int32 Sq) const;
	APlayerState* PlayerOnSeat(uint8 Colour) const;
	void Log(const TCHAR* Act, const FString& Detail) const;

	UPROPERTY(ReplicatedUsing = OnRep_Net)
	FKGTableNet Net;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> PlayerWhite;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> PlayerBlack;

	// Server-only, SaveGame (host migration)
	UPROPERTY(SaveGame) FString Fen;
	UPROPERTY(SaveGame) TArray<uint64> History;
	UPROPERTY(SaveGame) FString PuidWhite;
	UPROPERTY(SaveGame) FString PuidBlack;
	UPROPERTY(SaveGame) float ClockSeconds[2] = {0.0f, 0.0f};
	UPROPERTY(SaveGame) int32 MovesPlayed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> TableMesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> LightSquares;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> DarkSquares;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> WhitePieces;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> BlackPieces;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

private:
	FKGBoardState Board;
	FKGTableBot Bot;
	EKGTableBotLevel BotLevel[2] = {EKGTableBotLevel::Journeyman, EKGTableBotLevel::Journeyman};
	float BotDelayScale = 1.0f;
	float BotWait = -1.0f;
	int32 BotBadStreak = 0;
	uint16 LastDrawnSerial = 65535;
	float ClockAccum = 0.0f;
	uint32 TableSeed = 0;
	bool bWasOpen = true;
};
