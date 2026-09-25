#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGDevSubsystem.generated.h"

class APlayerController;
class SWidget;
class UGameViewportClient;

/**
 * Dev panel host (development builds only; never created in Shipping). Zero-integration glue like
 * UKGPlayerExtrasSubsystem:
 *  - server: gives every human player controller a UKGDevComponent (client -> host RPC + cheat state),
 *  - local players: F1 toggles the dev panel (SKGDevPanel, docked right, cursor on, game keeps running),
 *  - holds the panel's message log and this machine's view toggles (chore markers, hidden perf groups).
 * Verbs, console commands and the permission rule live in Dev/KGDevCommands.h.
 */
UCLASS()
class KILLGODOT_API UKGDevSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UKGDevSubsystem* Get(const UWorld* World);

	/** Server: idempotent. */
	static void EnsureDevComponent(APlayerController* PC);

	void TogglePanel(APlayerController* PC);
	void OpenPanel(APlayerController* PC);
	void ClosePanel();
	bool IsPanelOpen() const { return Panel.IsValid(); }

	struct FMessage
	{
		bool bOk = true;
		FString Text;
		double Time = 0.0;
	};
	void PushMessage(bool bOk, const FString& Text);
	const TArray<FMessage>& GetMessages() const { return Messages; }

	/** Local view toggles (World.ChoreMarkers, World.Hide). */
	bool bShowChoreMarkers = false;
	TSet<FName> HiddenGroups;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void DrawChoreMarkers() const;

	TArray<FMessage> Messages;
	float SweepSeconds = 0.0f;
	uint64 LastToggleFrame = 0;

	TSharedPtr<SWidget> Panel;
	TWeakObjectPtr<UGameViewportClient> PanelViewport;
	TWeakObjectPtr<APlayerController> PanelPC;
	bool bPanelRestoreCursor = false;
};
