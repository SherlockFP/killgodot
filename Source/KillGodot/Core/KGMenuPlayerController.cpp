#include "Core/KGMenuPlayerController.h"

#include "Core/KGGameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "Online/KGSessions.h"
#include "UI/Menu/SKGMainMenu.h"

AKGMenuPlayerController::AKGMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AKGMenuPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ShowMainMenu();
	}
}

void AKGMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		// Arriving at the front end always means "not in a match": drop any leftover session (disconnect, crash).
		FKGSessions::Get().EndSession(this);
		if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
		{
			Settings->ApplyAudioSettings();
		}
		ShowMainMenu();
	}
}

void AKGMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameViewportClient* Viewport = MenuEntry.Viewport.Get();
	KGMenu::RemoveFromViewport(MenuEntry);
	Menu.Reset();
	// The viewport survives the travel: never leave the next map with ignored input and a free cursor.
	KGMenu::ResetViewportInput(Viewport);
	Super::EndPlay(EndPlayReason);
}

void AKGMenuPlayerController::ShowMainMenu()
{
	if (MenuEntry.IsShown() || !GetLocalPlayer())
	{
		return;
	}
	Menu = SNew(SKGMainMenu).OwningPlayer(this);
	MenuEntry = KGMenu::AddToViewport(this, Menu.ToSharedRef(), 10);
	if (MenuEntry.IsShown())
	{
		KGMenu::SetMenuInput(this, Menu->GetInitialFocus());
	}
}

void AKGMenuPlayerController::OpenCosmeticsScreen()
{
	ShowMainMenu();
	if (Menu.IsValid())
	{
		Menu->OpenCosmeticsScreen();
	}
}
