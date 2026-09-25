#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KGMenuGameMode.generated.h"

/**
 * Front-end game mode (L_MainMenu, or any map opened with ?game=/Script/KillGodot.KGMenuGameMode): no pawn, the
 * AKGMenuPlayerController shows the title screen with the cursor and UI-only input.
 */
UCLASS()
class KILLGODOT_API AKGMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKGMenuGameMode();

	/** The front end never spawns a pawn. */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override { return false; }
};
