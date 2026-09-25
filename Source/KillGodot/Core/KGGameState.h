#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/KGMatchClock.h"
#include "Core/KGTypes.h"
#include "KGGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKGOnPhaseChanged, EKGPhase, NewPhase);

/** Public match state (everyone may know this). Secret role data lives on the owning player only. */
class AKGPlayerState;

UCLASS()
class KILLGODOT_API AKGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AKGGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Match")
	EKGPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Match")
	float GetPhaseRemaining() const { return Clock.RemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Match")
	int32 GetDayIndex() const { return DayIndex; }

	/** Called by the game mode (authority only). */
	void SetPhase(EKGPhase NewPhase, float Duration);

	UPROPERTY(BlueprintAssignable, Category = "KillGodot|Match")
	FKGOnPhaseChanged OnPhaseChanged;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Match")
	FKGMatchClock Clock;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Match")
	int32 DayIndex = 0;

	/** Monotonic host-migration epoch; clients follow the highest epoch (split-brain guard). */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Migration")
	int32 MigrationEpoch = 0;

	/** Ranked successor PUIDs, recomputed by the host every 10 s (redundant copy of the lobby attribute). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Migration")
	TArray<FString> SuccessorRanks;

	/** Set when the match is decided; the Epilogue shows it. */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Match")
	bool bHasWinner = false;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Match")
	EKGAlignment Winner = EKGAlignment::Town;

	/** The player on the gallows during a Trial. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Vote")
	TObjectPtr<AKGPlayerState> OnTrial;

	/** Town crier line shown to everyone for a few seconds (deaths at dawn, hangings). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Match")
	FString Announcement;

	/** Server world time until which the announcement is shown. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Match")
	float AnnouncementUntil = 0.0f;

	void Announce(const FString& Text, float Seconds);

	/** Lighthouse Illumination: this player glows red for everyone until RevealUntil (server time). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Tasks")
	TObjectPtr<AKGPlayerState> LighthouseRevealed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Tasks")
	float RevealUntil = 0.0f;

	/** Village preparation bar (0..1), fed by tasks. */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Tasks")
	float Preparation = 0.0f;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, SaveGame)
	EKGPhase Phase = EKGPhase::Lobby;

	UFUNCTION()
	void OnRep_Phase();

	/** Cosmetic, every machine: blends sun/sky towards the look of the current phase (day, dusk, night, dawn). */
	void UpdatePhaseLighting(float DeltaSeconds, bool bSnap);
	float LightBlend = 1.0f;
};
