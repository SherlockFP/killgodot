#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SKGMenuButton;
class SKGModal;
class SKGSettingsMenu;
class SOverlay;

/**
 * In-game Esc menu over a blurred, dimmed view: Resume, Settings, Leave to main menu, Quit. The match keeps
 * running (online game), the owning controller switches input to UI while this is on screen.
 */
class KILLGODOT_API SKGPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGPauseMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, OwningPlayer)
		SLATE_EVENT(FSimpleDelegate, OnResume)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Restarts the open animation and returns to the button list (the widget is reused between openings). */
	void PlayOpen();

	/** Esc: closes settings / dialogs first, otherwise resumes. */
	void HandleBack();

	TSharedPtr<SWidget> GetInitialFocus() const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void Resume();
	void OpenSettings();
	void ConfirmLeave();
	void ConfirmQuit();
	void ShowModal(const TSharedRef<SKGModal>& Modal);
	void CloseModal();
	bool IsModalOpen() const;
	FText GetSessionText() const;

	TWeakObjectPtr<APlayerController> OwningPlayer;
	FSimpleDelegate OnResume;
	TArray<TSharedPtr<SKGMenuButton>> Buttons;
	TSharedPtr<SOverlay> SettingsHost;
	TSharedPtr<SKGSettingsMenu> Settings;
	TSharedPtr<SOverlay> ModalHost;
	TWeakPtr<SWidget> FocusBeforeModal;
	FVector2f ViewSize = FVector2f(1920.0f, 1080.0f);
	float Appear = 0.0f;
	float SettingsAnim = 0.0f;
	bool bSettingsClosePending = false;
};
