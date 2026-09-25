#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/KGMatchClock.h"
#include "Dig/KGDigTypes.h"
#include "KGDigComponent.generated.h"

class AKGCharacter;
class AKGDigManager;
class APlayerController;
class UAnimSequence;
class UInputAction;
class UInputMappingContext;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Replicated to everyone: the shovel stroke others see (third-person swing, dust, sounds). */
USTRUCT()
struct KILLGODOT_API FKGDigAction
{
	GENERATED_BODY()

	UPROPERTY() bool bDigging = false;
	UPROPERTY() uint16 SpotId = 0;
	/** Seconds per stroke x 20 (so others time their swing to the digger's). */
	UPROPERTY() uint8 HoldQ = 22;
	UPROPERTY() uint8 Serial = 0;
};

/**
 * Digging on a villager body (added at runtime by UKGDigSubsystem, replicated). Docs/01_GDD_Core.md section 16.
 *
 * Server authoritative: shovel out/in (needs a Shovel item, not in meetings/trials), which spot is dug (view origin
 * validated, the aimed ground point must be on the spot, the digger must stand next to it), the hold time per stage
 * (FKGMatchClock, no timers), the loot (AKGDigManager::ApplyStage -> pockets, the rest at your feet), interrupts (hit,
 * walked off, meeting), the treasure-map marks (owner only) and the map-scrap merge.
 * Owner: Q / LMB keys (polled; LMB and RMB are consumed while the shovel is out so nobody punches by accident), the
 * aim and the predicted stage progress for the HUD (Dig/KGDigHud.inl), a first-person arms rig of its own (the
 * FPArms2 shovel_* clips) so AKGCharacter needs no changes. Everyone: the shovel in the body's hand and the stroke.
 */
UCLASS(ClassGroup = (KillGodot))
class KILLGODOT_API UKGDigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGDigComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UKGDigComponent* FindFor(const AActor* Pawn);

	// ---- Queries (HUD, tests, smoke) ----

	/** What this machine shows (the owner's prediction included). */
	bool IsShovelShown() const;
	bool IsShovelOutOnServer() const { return bShovelOut; }
	/** Owner: digging right now (predicted), others: the replicated stroke. */
	bool IsDiggingShown() const;
	const FKGDigAction& GetAction() const { return Action; }
	/** Owner: the spot under the crosshair (index into the manager's list) and the aimed ground point. */
	int32 GetAimSpotIndex() const { return AimSpot; }
	bool HasAimPoint() const { return bAimValid; }
	FVector GetAimPoint() const { return AimPoint; }
	/** Owner: the aim point is on one of your treasure-map X marks (you can dig there). */
	bool IsAimOnTreasureMark() const { return bAimOnMark; }
	/** Owner: 0..1 of the current stage (predicted). */
	float GetStageProgress() const { return LocalProgress; }
	const FKGDigResult& GetLastResult() const { return Result; }
	double GetResultShownAt() const { return ResultShownAt; }
	EKGDigNotice GetNotice(int32& OutA, int32& OutB, double& OutAt) const;
	/** Owner: where your treasure maps point (world XY, cm). */
	const TArray<FVector2D>& GetTreasureMarks() const { return TreasureMarks; }

	// ---- Owner actions (input, automation, the network smoke) ----

	void RequestToggleShovel();
	void RequestSetShovel(bool bOut);
	/** Start digging the spot with this id along the current view (automation: aim first). */
	bool StartDigging(uint16 SpotId);
	void StopDigging();
	/** Automation: hold the dig on SpotId for Seconds without the mouse (the view must look at the spot). */
	void SetScriptedDig(uint16 SpotId, float Seconds);

	// ---- Server ----

	bool ServerTrySetShovel(bool bOut, EKGDigNotice* OutWhy = nullptr);
	/** Authority: begin (or continue) digging. SpotId 0 = the aimed ground (reveals a buried chest there). */
	bool ServerTryBeginDig(uint16 SpotId, const FVector& ViewStart, const FVector& ViewDir, EKGDigNotice* OutWhy = nullptr);
	void ServerStopDig();
	/** Authority (dev Dig.Finish / tests): dig every remaining stage of the spot at Index now, loot to the pockets. */
	int32 ServerFinishSpot(int32 Index);
	void ServerNotify(EKGDigNotice InNotice, int32 A = 0, int32 B = 0);
	/** Authority: recompute the treasure marks from the pockets (also merges map scraps). */
	void ServerRefreshTreasure();
	/** Tests: advance the server dig clock. */
	void DebugTickServer(float Seconds, float Step = 0.05f);

protected:
	UFUNCTION(Server, Reliable) void ServerSetShovel(bool bOut);
	UFUNCTION(Server, Reliable) void ServerBeginDig(uint16 SpotId, FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir);
	UFUNCTION(Server, Reliable) void ServerEndDig();
	UFUNCTION(Client, Reliable) void ClientNotice(EKGDigNotice InNotice, int32 A, int32 B);

	UFUNCTION() void HandleHealthChanged(float NewHealth, float Delta, AActor* InstigatorActor);

	UPROPERTY(Replicated)
	bool bShovelOut = false;

	UPROPERTY(Replicated)
	FKGDigAction Action;

	UPROPERTY(Replicated)
	FKGDigResult Result;

	UPROPERTY(Replicated)
	TArray<FVector2D> TreasureMarks;

private:
	AKGCharacter* GetCharacter() const;
	bool IsOwnerView() const;
	APlayerController* GetLocalPC() const;
	bool HasShovelItem() const;
	AKGDigManager* GetManager() const;

	// Server
	void TickServer(float DeltaTime);
	void ServerCompleteStage();
	bool ServerCheckStillDigging(EKGDigNotice& OutWhy) const;
	FKGMatchClock HoldClock;
	int32 ServerSpot = INDEX_NONE;
	float TreasureAccum = 0.0f;
	bool bHealthBound = false;
	float NothingCooldown = 0.0f;

	// Owner
	void TickOwner(float DeltaTime);
	void UpdateAim();
	void UpdateInputContext(bool bOn);
	void OnResultArrived();
	UPROPERTY() TObjectPtr<UInputMappingContext> ShovelContext;
	UPROPERTY() TObjectPtr<UInputAction> ConsumeAction;
	bool bContextOn = false;
	TOptional<bool> PredictedShovel;
	double PredictedShovelUntil = 0.0;
	bool bLocalDigging = false;
	uint16 LocalSpotId = 0;
	uint8 LocalStageSeen = 0;
	float LocalProgress = 0.0f;
	float LocalHold = 1.1f;
	double LocalStartAt = 0.0;
	float AimLostFor = 0.0f;
	uint16 ScriptedSpot = 0;
	float ScriptedSeconds = 0.0f;
	int32 AimSpot = INDEX_NONE;
	FVector AimPoint = FVector::ZeroVector;
	bool bAimValid = false;
	bool bAimOnMark = false;
	uint8 ShownResultSerial = 0;
	bool bResultPrimed = false;
	double ResultShownAt = -100.0;
	EKGDigNotice Notice = EKGDigNotice::None;
	int32 NoticeA = 0;
	int32 NoticeB = 0;
	double NoticeAt = -100.0;
	float ProbeCooldown = 0.0f;

	// Visuals (every machine)
	void CreateVisuals();
	void TickVisuals(float DeltaTime);
	void UpdateFirstPerson(float DeltaTime);
	void UpdateThirdPerson(float DeltaTime);
	void UpdateBodyPose();
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> DigArms;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FPShovel;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TPShovel;
	UPROPERTY() TObjectPtr<UStaticMesh> ShovelMesh;
	bool bVisualsReady = false;
	UAnimSequence* ArmsPlaying = nullptr;
	float ArmsRate = 1.0f;
	bool bDrawPlayed = false;
	float StrokeClock = 0.0f;
	bool bStrokeHit = false;
	UAnimSequence* BodyClip = nullptr;
	FRotator TPRot = FRotator::ZeroRotator;
};
