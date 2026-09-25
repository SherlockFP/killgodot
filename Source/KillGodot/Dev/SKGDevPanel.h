#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class AKGGameState;
class AKGPlayerState;
class APlayerController;
class SVerticalBox;
class SWrapBox;
class UKGDevComponent;

/**
 * The dev / test panel (F1, kg.Dev): a compact tabbed sheet docked to the right edge, in the HUD/menu palette
 * (FKGMenuStyle), scrolling so it fits 720p..1440p (Slate DPI scaling does the rest). Tabs: Match, Bots, Me, World,
 * Chores, Debug. Every button just submits a dev verb line (FKGDev::Submit), exactly what the kg.* console commands
 * run, so the panel never has logic of its own. On a client without permission the server buttons are disabled
 * (read-only view); local view toggles (stats, perf groups, viewmodel, HUD demo) always work.
 */
class SKGDevPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGDevPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	/** Clicks outside the panel never reach the game (no accidental swings while the cursor is up). */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// --- data access ------------------------------------------------------------------------------------------
	UWorld* LocalWorld() const;
	/** Authority world when this process has one (host, PIE server twin), else the local world. */
	UWorld* ReadWorld() const;
	AKGGameState* ReadGameState() const;
	/** This player's state in ReadWorld (the PIE server twin on a PIE client). */
	AKGPlayerState* ReadMe() const;
	UKGDevComponent* DevLink() const;
	bool CanRunServer() const;

	// --- actions ------------------------------------------------------------------------------------------------
	void Run(const FString& Line) const;
	/** Local verbs without a footer message (sliders fire every frame while dragged). */
	void RunQuiet(const FString& Line) const;

	// --- building -----------------------------------------------------------------------------------------------
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildStatus();
	TSharedRef<SWidget> BuildTabs();
	TSharedRef<SWidget> BuildMatchTab();
	TSharedRef<SWidget> BuildBotsTab();
	TSharedRef<SWidget> BuildMeTab();
	TSharedRef<SWidget> BuildWorldTab();
	TSharedRef<SWidget> BuildChoresTab();
	TSharedRef<SWidget> BuildDebugTab();
	TSharedRef<SWidget> BuildFooter();

	/** A button that submits Line (disabled while server verbs are not allowed). */
	TSharedRef<SWidget> ActionButton(const FText& Label, const FString& Line, uint8 Kind = 0,
	                                 TAttribute<bool> Selected = false, const FLinearColor& Tint = FLinearColor::Transparent,
	                                 const FText& ToolTip = FText::GetEmpty());
	TSharedRef<SWidget> SliderRow(const FText& Label, float Min, float Max, float Step, TAttribute<float> Value,
	                              TFunction<void(float)> OnChanged, int32 Decimals = 2);

	void RebuildPlayers();
	void RebuildChores();
	void RebuildRoles();
	uint32 PlayersSignature() const;
	uint32 ChoresSignature() const;

	TWeakObjectPtr<APlayerController> PC;
	int32 ActiveTab = 0;
	int32 FillTarget = 12;
	FString RoleFilter;
	float SmoothedFps = 60.0f;
	float SignatureTimer = 0.0f;
	uint32 PlayersSig = 0;
	uint32 ChoresSig = 0;

	TSharedPtr<SVerticalBox> PlayersBox;
	TSharedPtr<SVerticalBox> ChoresBox;
	TSharedPtr<SWrapBox> RolesBox;
};

#endif
