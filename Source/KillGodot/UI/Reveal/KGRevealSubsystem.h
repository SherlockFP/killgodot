#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Reveal/KGRevealTypes.h"
#include "KGRevealSubsystem.generated.h"

class APawn;
class APlayerController;
class SKGRoleReveal;
class SWidget;

/**
 * Role reveal glue, zero-integration like UKGChatSubsystem (no edits to the player controller or character):
 *  - server: gives every human player controller a UKGRevealComponent, deals the teammates list when the RoleReveal
 *    phase starts, keeps the ready tally (and cuts the phase short when everyone is ready),
 *  - local player: shows SKGRoleReveal full screen from the moment the lobby starts until the RoleReveal phase is
 *    over (the village is never seen before the card), freezes the pawn's input meanwhile, turns Space / A / left
 *    click into the "ready" press, and logs every beat (KG_REVEAL lines, also under -nullrhi for smokes),
 *  - dev: kg.Reveal.Ready, -KGRevealAutoReady (headless smokes), kg.UIShot reveal:<Role>:<stage> via MakeShotWidget.
 */
UCLASS()
class KILLGODOT_API UKGRevealSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** True while the ceremony covers the screen of World's local player (the HUD skips its own role card). */
	static bool IsOverlayShown(const UWorld* World);

	/** Local "I have seen my card". Returns false when it is too early or already sent. */
	bool PressReady();

	/** Builds the live view for the local player (also used by the -nullrhi logger). */
	FKGRevealView BuildView() const;

	/**
	 * Screenshot widget: Spec = "<RoleId>:<stage>[:ready][:streamer]", stage = table | shuffle | deal | flip | role. Sample
	 * data (12 seats, two accomplices for a Clockbreaker). Null when the spec is not understood.
	 */
	static TSharedPtr<SWidget> MakeShotWidget(const FString& Spec, bool bStreamer);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void TickServer(float DeltaTime);
	void TickLocal(float DeltaTime);
	void ShowOverlay(APlayerController* PC);
	void HideOverlay();
	void LogBeats(const FKGRevealView& View);

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<APlayerController>> Pending;
	float SweepSeconds = 0.0f;

	// Server phase tracking.
	bool bServerInReveal = false;
	float ReadySweep = 0.0f;

	// Local ceremony.
	TWeakObjectPtr<APlayerController> LocalPC;
	TSharedPtr<SKGRoleReveal> Widget;
	FKGViewportWidget Entry;
	TWeakObjectPtr<APawn> FrozenPawn;
	bool bActive = false;
	bool bEnding = false;
	float ActiveSeconds = 0.0f;
	float EndingSeconds = 0.0f;
	float LocalClock = 0.0f;
	int32 LoggedStage = -1;
	bool bLoggedRole = false;
	bool bAutoReadySent = false;
};
