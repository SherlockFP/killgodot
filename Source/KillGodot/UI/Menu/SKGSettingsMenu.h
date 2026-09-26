#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SKGMenuButton;
class SKGModal;
class SOverlay;
class SVerticalBox;
class UKGGameUserSettings;

/** Every value the settings screen edits, used for "unsaved changes" detection and Discard. */
struct FKGSettingsSnapshot
{
	int32 WindowMode = 0;
	FIntPoint Resolution = FIntPoint::ZeroValue;
	bool bVSync = false;
	float FrameRateLimit = 0.0f;
	float ResolutionScale = 100.0f;
	int32 Quality[7] = {};
	float Sensitivity = 1.0f;
	bool bInvertY = false;
	float FieldOfView = 90.0f;
	int32 ViewmodelPreset = 1;
	bool bStreamerMode = false;
	FName StreamerPeekKey;
	bool bOpenMic = false;
	float Volumes[4] = {};

	static FKGSettingsSnapshot Capture(const UKGGameUserSettings& Settings);
	void Restore(UKGGameUserSettings& Settings) const;
	bool Matches(const FKGSettingsSnapshot& Other) const;
};

/**
 * Tabbed settings panel (Graphics, Gameplay, Audio, Controls, Language) used by both the main menu and the pause
 * menu. Gameplay and audio values preview live; graphics apply on Apply (with a keep/revert countdown after a
 * display mode change). Everything persists through UKGGameUserSettings.
 * Keys: Q / E switch tabs, Esc goes back (asks to apply or discard pending changes).
 */
class KILLGODOT_API SKGSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGSettingsMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Back: closes, asking first when there are unapplied changes. */
	void RequestClose();

	/** Widget to focus when the panel opens. */
	TSharedPtr<SWidget> GetInitialFocus() const;

private:
	enum ETab : int32
	{
		Tab_Graphics,
		Tab_Gameplay,
		Tab_Audio,
		Tab_Controls,
		Tab_Language,
		Tab_Count
	};

	TSharedRef<SWidget> BuildGraphicsTab();
	TSharedRef<SWidget> BuildGameplayTab();
	TSharedRef<SWidget> BuildAudioTab();
	TSharedRef<SWidget> BuildControlsTab();
	TSharedRef<SWidget> BuildLanguageTab();
	TSharedRef<SWidget> MakePage(const TSharedRef<SVerticalBox>& Content) const;
	TSharedRef<SWidget> MakeQualityRow(const FText& Label, const FText& Description, int32 QualityIndex);
	TSharedRef<SWidget> MakeVolumeRow(const FText& Label, const FText& Description, int32 Channel);
	TSharedRef<SWidget> MakeKeysRow(const FText& Label, const TArray<FText>& Keys, const FText& Description = FText::GetEmpty());

	void SelectTab(int32 Tab, bool bFocusTab);
	bool IsDirty() const;
	void Apply();
	void Discard();
	void Close();
	void AutoDetect();
	void ResetTab();
	void RefreshResolutions();
	void OnLiveValueChanged();

	void ShowModal(const TSharedRef<SKGModal>& Modal);
	void CloseModal();
	bool IsModalOpen() const;
	void KeepDisplayMode();
	void RevertDisplayMode();

	FSimpleDelegate OnClosed;
	FKGSettingsSnapshot Snapshot;
	int32 ActiveTab = Tab_Graphics;
	TArray<FIntPoint> Resolutions;
	TArray<FText> ResolutionLabels;
	FText LanguageNotice;

	TSharedPtr<SOverlay> ModalHost;
	TWeakPtr<SWidget> FocusBeforeModal;
	TSharedPtr<SKGMenuButton> TabButtons[Tab_Count];
	float Appear = 0.0f;
	float ConfirmCountdown = -1.0f;
};
