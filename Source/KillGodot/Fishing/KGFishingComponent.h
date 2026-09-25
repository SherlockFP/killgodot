#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/KGMatchClock.h"
#include "Core/KGRng.h"
#include "Fishing/KGFishingTypes.h"
#include "KGFishingComponent.generated.h"

class AKGCharacter;
class APlayerController;
class APlayerState;
class UAnimSequence;
class UAudioComponent;
class UInputAction;
class UInputMappingContext;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FMinimalViewInfo;

/** Owner-only messages the HUD shows as a toast. */
UENUM()
enum class EKGFishNotice : uint8
{
	None,
	NoRod,          // H without a rod in the pockets
	NotNow,         // meeting / trial / swimming / blade out
	TooEarly,       // hooked during a nibble: spooked it
	Missed,         // hook window passed
	LineTooFar,     // walked away from the bobber
	LentRod,        // Madam Brine lent you a rod
	Sold,           // A = items sold, B = coins
	NothingToSell,
	PocketsFull,    // the catch landed at your feet
	Busy            // hands full / carrying
};

/** Replicated to everyone: the rod, the line and the bobber (others see your fishing). */
USTRUCT()
struct KILLGODOT_API FKGFishLine
{
	GENERATED_BODY()

	UPROPERTY() EKGFishPhase Phase = EKGFishPhase::Stowed;
	UPROPERTY() uint8 CastSerial = 0;
	UPROPERTY() uint8 NibbleSerial = 0;
	UPROPERTY() uint8 BiteSerial = 0;
	UPROPERTY() EKGFishWater Water = EKGFishWater::None;
	/** Cast arc (server-validated) and where the bobber rests. */
	UPROPERTY() FVector_NetQuantize10 Origin;
	UPROPERTY() FVector_NetQuantize10 Velocity;
	UPROPERTY() FVector_NetQuantize10 Landing;
	UPROPERTY() float FlightTime = 0.0f;
	/** Fight visuals for everyone: tension 0..1.4 (x 180), fish lateral -1..1 (x 127), line out (cm). */
	UPROPERTY() uint8 TensionQ = 0;
	UPROPERTY() int8 FishXQ = 0;
	UPROPERTY() uint16 DistanceCm = 0;
	/** Wind-up while charging (0..255), so others see the rod go back. */
	UPROPERTY() uint8 ChargeQ = 0;
};

/** Owner only: the fight the server is simulating (the owning client predicts it and reconciles). */
USTRUCT()
struct KILLGODOT_API FKGFishFight
{
	GENERATED_BODY()

	UPROPERTY() uint8 Serial = 0;
	/** Bumped on every publish (15 Hz) so the owner knows a fresh server state arrived. */
	UPROPERTY() uint8 NetTick = 0;
	/** FKGFishingRules::Species() index, -1 = junk. */
	UPROPERTY() int8 Species = -1;
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Grams = 0;
	UPROPERTY() FKGReelSim Sim;
};

/** Replicated to everyone: the last result (a catch, a snapped line, the koi tax). */
USTRUCT()
struct KILLGODOT_API FKGFishCatch
{
	GENERATED_BODY()

	UPROPERTY() uint8 Serial = 0;
	UPROPERTY() EKGFishPhase Result = EKGFishPhase::Stowed;
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Grams = 0;
	/** Coin pouch contents / koi tax taken. */
	UPROPERTY() int32 Coins = 0;
	/** Message-in-a-bottle index, -1 = none. */
	UPROPERTY() int8 Message = -1;
	/** Pockets were full: it landed at the angler's feet. */
	UPROPERTY() bool bDropped = false;
};

/**
 * Fishing on a villager body (added at runtime by UKGFishingSubsystem, replicated). Docs/01_GDD_Core.md §15.
 *
 * Server authoritative: rod in/out (needs a Fishing Rod item, not during meetings/trials), the cast (view origin
 * validated, arc traced against the world, water classified), nibbles and the bite (FKGRng), the hook window, the reel
 * fight (FKGReelSim with the owner's inputs), the catch (inventory with its weight, or at your feet) and the koi tax.
 * Everyone: rod, line, bobber, splashes, trophy and sounds from the replicated FKGFishLine / FKGFishCatch. The owning
 * client predicts the cast arc, the charge and the fight, polls its keys (H, LMB, RMB, A/D), blocks attack/shove and
 * walking with a higher-priority input context, narrows its view while reeling and draws the HUD (KGHUDFishing.inl).
 */
UCLASS(ClassGroup = (KillGodot))
class KILLGODOT_API UKGFishingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGFishingComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UKGFishingComponent* FindFor(const AActor* Pawn);
	static UKGFishingComponent* FindForPlayer(const APlayerState* PlayerState);

	// ---- Queries (HUD, character, tests) ----

	/** What this machine shows (the owner's predictions included). */
	EKGFishPhase GetShownPhase() const;
	EKGFishPhase GetServerPhase() const { return Line.Phase; }
	bool IsRodShown() const;
	/** The character should put the rod in the first-person hand. */
	bool WantsRodInHand() const { return IsRodShown(); }
	UStaticMesh* GetRodMesh() const { return RodMesh; }
	/** First-person arms loop for the current phase; one-shots are handed out once. */
	UAnimSequence* GetArmsLoop() const;
	UAnimSequence* ConsumeArmsOneShot();
	float GetChargePower() const;
	/** Owner: the predicted fight (HUD); others: a coarse copy from FKGFishLine. */
	const FKGReelSim& GetShownSim() const { return LocalSim; }
	bool HasShownSim() const { return bLocalSim; }
	const FKGFishFight& GetFight() const { return Fight; }
	const FKGFishLine& GetLine() const { return Line; }
	const FKGFishCatch& GetLastCatch() const { return Catch; }
	/** Real time (s) the last catch result arrived here, < 0 = none yet. */
	double GetCatchShownAt() const { return CatchShownAt; }
	/** Personal best (grams) of the caught species before this catch, and whether it was beaten. */
	int32 GetPreviousBest() const { return PreviousBest; }
	bool IsNewBest() const { return bNewBest; }
	EKGFishNotice GetNotice(int32& OutA, int32& OutB, double& OutAt) const;
	/** Real time of the last bite (bobber pulled under), for the "!" cue. */
	double GetBiteShownAt() const { return BiteShownAt; }
	/** 0..1 how much the reel fight owns the player's attention (view narrowing, vignette). */
	float GetFocusAlpha() const { return Focus; }
	/** Seconds since the bobber settled on the water (owner HUD hint), -1 = not waiting. */
	float GetWaitSeconds() const;

	/** AKGCharacter::CalcCamera: narrower view while reeling + the bite nudge. */
	void ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView);

	// ---- Owner actions (input, automation, the network smoke) ----

	void RequestToggleRod();
	void RequestSetRod(bool bOut);
	void BeginCharge();
	void CancelCharge();
	/** Cast with the current charge. */
	void ReleaseCast();
	/** Cast along the view with an explicit power (automation). */
	void CastWithPower(float Power);
	void RequestHook();
	void RequestReelIn();
	/** Scripted reel input (automation / smoke) instead of the keys, for Seconds. */
	void SetScriptedReelInput(const FKGReelInput& Input, float Seconds);

	// ---- Server ----

	/** Authority: take the rod out / put it away. Returns false (with a reason) when refused. */
	bool ServerTrySetRod(bool bOut, EKGFishNotice* OutWhy = nullptr);
	/** Authority: validated cast from a claimed view. */
	bool ServerTryCast(const FVector& ViewStart, const FVector& ViewDir, float Power);
	/** Authority (dev / smoke): the next bite happens now (optionally a given species id or item id). */
	void ServerForceBite(FName Species = NAME_None);
	/** Authority (dev): strike for the owner when something bites. */
	void ServerForceHook();
	/** Authority (dev): land the current fight at once. */
	bool ServerForceLand();
	/** Authority: end fishing (meeting, death, blade drawn...). */
	void ServerStow();
	/** Authority: owner toast. */
	void ServerNotify(EKGFishNotice InNotice, int32 A = 0, int32 B = 0);

	/** Where a cast starts for a view (both machines use it, so the owner can predict the arc). */
	static FVector CastOrigin(const FVector& ViewStart, const FVector& ViewDir);

	/** kg.Fish.Tension overlay. */
	static bool IsDebugOverlayOn();

protected:
	UFUNCTION(Server, Reliable) void ServerSetRod(bool bOut);
	UFUNCTION(Server, Unreliable) void ServerSetCharge(uint8 ChargeQ);
	UFUNCTION(Server, Reliable) void ServerCast(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir, uint8 PowerQ);
	UFUNCTION(Server, Reliable) void ServerHook();
	UFUNCTION(Server, Reliable) void ServerReelIn();
	UFUNCTION(Server, Unreliable) void ServerReelInput(bool bReel, int8 SteerQ);
	UFUNCTION(Client, Reliable) void ClientNotice(EKGFishNotice InNotice, int32 A, int32 B);

	/** Server: getting hit while reeling makes you flinch - the fish gets away (the killer's window). */
	UFUNCTION() void HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor);

	UPROPERTY(Replicated)
	bool bRodOut = false;

	UPROPERTY(Replicated)
	FKGFishLine Line;

	UPROPERTY(Replicated)
	FKGFishFight Fight;

	UPROPERTY(Replicated)
	FKGFishCatch Catch;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RodMesh;

private:
	AKGCharacter* GetCharacter() const;
	bool IsOwnerView() const;
	APlayerController* GetLocalPC() const;
	bool HasRodItem() const;

	// Server
	void TickServer(float DeltaTime);
	void ServerSetPhase(EKGFishPhase Phase, float HoldSeconds = 0.0f);
	void ServerLand();
	void ServerStartWaiting(float ExtraDelay);
	void ServerStartBite();
	void ServerStartFight();
	void ServerEndFight(EKGReelResult Result);
	void ServerResult(EKGFishPhase Result, FName ItemId, int32 Grams, int32 Coins, int8 Message, bool bDropped);
	void ServerPublishFight();
	void EnsureRng();
	FKGMatchClock PhaseClock;
	FKGBiteSchedule Schedule;
	int32 NextNibble = 0;
	float WaitElapsed = 0.0f;
	FKGBiteRoll PendingBite;
	FKGReelSim ServerSim;
	FKGReelInput ServerInput;
	float PublishAccum = 0.0f;
	UPROPERTY(SaveGame) FKGRng Rng;
	bool bRngSeeded = false;
	bool bForceBite = false;
	FName ForcedSpecies;
	float CastCooldown = 0.0f;

	// Owner
	void TickOwner(float DeltaTime);
	void UpdateInputContexts(bool bRod, bool bFight);
	void SendReelInput(const FKGReelInput& Input, bool bForce);
	FKGReelInput ReadKeys(APlayerController* PC) const;
	void OnCatchArrived();
	UPROPERTY() TObjectPtr<UInputMappingContext> RodContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> FightContext;
	UPROPERTY() TObjectPtr<UInputAction> ConsumeAction;
	bool bRodContextOn = false;
	bool bFightContextOn = false;
	bool bCharging = false;
	float ChargeSeconds = 0.0f;
	uint8 LastChargeQSent = 0;
	TOptional<bool> PredictedRod;
	double PredictedRodUntil = 0.0;
	// cast prediction: shown until the server's CastSerial moves (or it refuses)
	bool bPredictCast = false;
	uint8 PredictBaseSerial = 0;
	double PredictCastUntil = 0.0;
	FVector PredOrigin = FVector::ZeroVector;
	FVector PredVelocity = FVector::ZeroVector;
	FKGCastResult PredResult;
	double PredCastAt = 0.0;
	// hook prediction
	double PredictHookUntil = 0.0;
	double PredictReelInUntil = 0.0;
	// fight prediction
	FKGReelSim LocalSim;
	bool bLocalSim = false;
	uint8 LocalFightSerial = 0;
	uint8 LocalNetTick = 0;
	FKGReelInput LastSentInput;
	float InputSendAccum = 0.0f;
	FKGReelInput ScriptedInput;
	float ScriptedSeconds = 0.0f;
	// HUD
	EKGFishNotice Notice = EKGFishNotice::None;
	int32 NoticeA = 0;
	int32 NoticeB = 0;
	double NoticeAt = -100.0;
	double CatchShownAt = -1.0;
	uint8 ShownCatchSerial = 0;
	bool bCatchPrimed = false;
	int32 PreviousBest = 0;
	bool bNewBest = false;
	double BiteShownAt = -100.0;
	float Focus = 0.0f;
	float NudgeTime = 10.0f;
	UAnimSequence* PendingOneShot = nullptr;

	// Visuals (every machine)
	void CreateVisuals();
	void TickVisuals(float DeltaTime);
	void UpdateThirdPersonRod(float DeltaTime, EKGFishPhase Phase);
	void UpdateBodyPose(EKGFishPhase Phase);
	void DrawLine(const FVector& A, const FVector& B, float Tension, bool bVisible);
	void Splash(const FVector& At, float Size);
	void TickSplash(float DeltaTime);
	FVector RodTipWorld() const;
	FVector BobberRestPoint(float Time) const;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bobber;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ThirdPersonRod;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Trophy;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LineSegments;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Droplets;
	UPROPERTY() TObjectPtr<UAudioComponent> ReelSound;
	bool bVisualsReady = false;
	bool bVisualsPrimed = false;
	uint8 ShownCast = 0;
	uint8 ShownNibble = 0;
	uint8 ShownBite = 0;
	EKGFishPhase ShownPhaseLast = EKGFishPhase::Stowed;
	double CastSeenAt = -100.0;
	double NibbleSeenAt = -100.0;
	double PhaseSeenAt = 0.0;
	bool bLandedSplash = false;
	FVector LastBobber = FVector::ZeroVector;
	FVector LastFishPoint = FVector::ZeroVector;
	FRotator TPRodRot = FRotator::ZeroRotator;
	float TPWindup = 0.0f;
	UAnimSequence* BodyClip = nullptr;
	bool bHealthBound = false;
	FKGRng CosmeticRng;
	struct FDrop
	{
		FVector P;
		FVector V;
		float Age;
		float Life;
		float Size;
	};
	TArray<FDrop> Drops;
};
