#include "Core/KGMenuGameMode.h"

#include "Core/KGMenuPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerState.h"

AKGMenuGameMode::AKGMenuGameMode()
{
	PlayerControllerClass = AKGMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = AHUD::StaticClass();
	GameStateClass = AGameStateBase::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
	bStartPlayersAsSpectators = false;
}
