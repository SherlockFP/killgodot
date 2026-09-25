#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class AKGLobbyState;
class AKGPlayerController;
class APlayerController;
class APlayerState;
class SKGMenuButton;
class SKGModal;
class SOverlay;
class SVerticalBox;
struct FKGLobbySettings;

/**
 * Pre-game lobby room shown over the map while AKGLobbyState is open: the seat list (avatar colour, name, host
 * badge, ready check box, kick for the host), the host's match settings (map, player count, role list preset, fill
 * with bots), the lobby chat slot, Ready / Start / Leave and the start countdown. Everything it shows comes from the
 * replicated lobby state; every change goes through AKGPlayerController server RPCs (host authoritative).
 */
class KILLGODOT_API SKGLobbyRoom : public SCompoundWidget
{
public:
	/** Builds the lobby chat panel. Bind it from the chat code; a placeholder is shown until then. */
	using FChatPanelFactory = TFunction<TSharedRef<SWidget>(TWeakObjectPtr<APlayerController> /*Owner*/)>;
	static FChatPanelFactory& ChatPanelFactory();

	/** Display names of the role list presets (index = FKGLobbySettings::RolePreset). */
	static const TArray<FText>& GetRolePresetNames();

	SLATE_BEGIN_ARGS(SKGLobbyRoom) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AKGPlayerController>, OwningPlayer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<SWidget> GetInitialFocus() const;
	void ShowToast(const FText& Message, bool bError = false);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	AKGLobbyState* GetLobby() const;
	bool IsHost() const;
	bool IsLocalReady() const;
	float PanelWidth() const;
	float PanelHeight() const;

	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildSettingsCard();
	TSharedRef<SWidget> BuildChatArea();
	TSharedRef<SWidget> BuildFooter();
	void RebuildPlayers();

	void SendSettings(TFunctionRef<void(FKGLobbySettings&)> Change);
	void RequestMap(int32 MapIndex);
	void ToggleReady();
	void PressStart();
	void RequestLeave();
	void RequestKick(TWeakObjectPtr<APlayerState> Target);
	FText GetStatusText() const;

	void ShowModal(const TSharedRef<SKGModal>& Modal);
	void CloseModal();
	bool IsModalOpen() const;

	TWeakObjectPtr<AKGPlayerController> OwningPlayer;
	mutable TWeakObjectPtr<AKGLobbyState> CachedLobby;
	uint32 SeenRevision = 0;
	bool bHost = false;

	TSharedPtr<SVerticalBox> PlayersBox;
	TSharedPtr<SOverlay> ModalHost;
	TWeakPtr<SWidget> FocusBeforeModal;
	TSharedPtr<SKGMenuButton> ReadyButton;
	TArray<int32> MapIndices; // selector index -> KGMenu::GetMaps() index (available maps only)

	FVector2f ViewSize = FVector2f(1920.0f, 1080.0f);
	FText ToastText;
	bool bToastError = false;
	float ToastTime = 0.0f;
	float Appear = 0.0f;
	float Life = 0.0f;
	float CountdownPop = 0.0f;
	int32 LastCountdownSecond = -1;
};
