#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGTabletopSubsystem.generated.h"

class AKGBoardTable;
class AKGCharacter;
class APlayerController;
class SKGTablePanel;
struct FKGTableView;

/**
 * Tabletop glue (SPRINT-036):
 *  - server: gives every AKGPlayerState a UKGTabletopRPCComponent, hands the tables their share of the bot budget
 *    (<= 0.5 ms per frame for all searching bots together);
 *  - local player: opens the 2D board panel while seated at a table, the spectator strip within 3 m of one;
 *  - dev: the -KGTableSmoke script (Tools/Unreal/kg_table_smoke.ps1), the kg.Table.* console verbs and kg.TableShot
 *    (KGTabletopDev.cpp).
 */
UCLASS()
class KILLGODOT_API UKGTabletopSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	static UKGTabletopSubsystem* Get(const UWorld* World);

	/** Authority: idempotent. */
	static void EnsurePlayerComponents(APlayerState* PlayerState);

	/** Total bot search time per frame across every table (seconds). */
	static double BotBudgetSeconds() { return 0.0005; }

	/** The table the local player currently sees in the panel (null when none). */
	AKGBoardTable* GetPanelTable() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TickServer(float DeltaTime);
	void TickUI(float DeltaTime);
	void TickSmoke(float DeltaTime);
	void OpenPanel(APlayerController* PC, bool bStrip);
	void ClosePanel();
	void FillView(AKGBoardTable* Table, const AKGCharacter* Me, APlayerController* PC);
	void HandlePanelMove(const FString& MoveText);
	void HandlePanelAction(uint8 Action);

	float SweepSeconds = 1.0f;
	TSharedPtr<FKGTableView> View;
	TSharedPtr<SKGTablePanel> Panel;
	TWeakObjectPtr<AKGBoardTable> PanelTable;
	TWeakObjectPtr<APlayerController> PanelPC;
	bool bPanelStrip = false;
	bool bPanelInputMode = false;

	// -KGTableSmoke
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	int32 SmokeSent = -1;
	int32 SmokeBadSent = 0;
	float SmokeNextSend = 0.0f;
	bool bSmokeDone = false;
	TWeakObjectPtr<AKGBoardTable> SmokeTable;
};
