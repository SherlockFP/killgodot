#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/Menu/KGMenuActions.h"
#include "KGMenuPlayerController.generated.h"

class SKGMainMenu;

/** Front-end controller: puts the title screen (SKGMainMenu) on the viewport with the cursor and UI-only input. */
UCLASS()
class KILLGODOT_API AKGMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKGMenuPlayerController();

	/** Shop hook: jumps straight to the Cosmetics page of the title screen. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Menu")
	void OpenCosmeticsScreen();

	/** (Re)creates the title screen if it is not on screen. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Menu")
	void ShowMainMenu();

	virtual void ReceivedPlayer() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SKGMainMenu> Menu;
	FKGViewportWidget MenuEntry;
};
