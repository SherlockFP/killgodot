#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Online/KGSessions.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SKGMenuButton;
class SKGModal;
class SKGSettingsMenu;
class SOverlay;
class SVerticalBox;

/** Pages of the title screen. */
enum class EKGMainMenuPage : uint8
{
	Home,
	Play,      // server browser
	Host,
	Join,      // alias: the server browser with the code / address field focused
	Cosmetics,
	Settings
};

/** Sortable columns of the server browser. */
enum class EKGBrowserColumn : uint8
{
	Recommended, // lobbies first, then fuller, then lower ping
	Lock,
	Name,
	Host,
	Map,
	Players,
	Phase,
	Ping,
	Region
};

/**
 * Title screen: painted dusk-harbour backdrop, the KILL GODOT wordmark and a column of big buttons on the home page
 * (Play / Cosmetics / Settings / Quit). Play opens a full-screen server browser (sortable table, search, filters,
 * join by code or address, Quick Match), Host and Cosmetics open centered panels. Every page sizes itself from the
 * DPI-scaled viewport, so it fits 720p to 1440p and ultrawide. Keyboard + mouse: arrows / Tab move, Enter selects,
 * Esc goes back.
 */
class KILLGODOT_API SKGMainMenu : public SCompoundWidget
{
public:
	/** Builds the Cosmetics page. Bind it from the shop code to replace the "coming soon" placeholder. */
	using FCosmeticsPageFactory = TFunction<TSharedRef<SWidget>(TWeakObjectPtr<APlayerController> /*Owner*/, FSimpleDelegate /*OnBack*/)>;
	static FCosmeticsPageFactory& CosmeticsPageFactory();

	SLATE_BEGIN_ARGS(SKGMainMenu)
		: _bOverlay(false)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, OwningPlayer)
		/** Shown over a running map (kg.MainMenu): adds "Close" and Esc on the home page closes it. */
		SLATE_ARGUMENT(bool, bOverlay)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows the Cosmetics page (the shop hook; placeholder until CosmeticsPageFactory is bound). */
	void OpenCosmeticsScreen();

	void OpenPage(EKGMainMenuPage Page);
	EKGMainMenuPage GetPage() const { return Page; }

	/** Small notification pill at the bottom of the screen. */
	void ShowToast(const FText& Message, bool bError = false);

	/** Widget that should get keyboard focus when the menu appears. */
	TSharedPtr<SWidget> GetInitialFocus() const;

	/** Test hooks (kg.UIShot): fill the browser with rows / open a modal without a network. */
	void DebugSetRows(const TArray<FKGSessionRow>& Rows);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedRef<SWidget> BuildHomeList();
	TSharedRef<SWidget> BuildBrowserPage();
	TSharedRef<SWidget> BuildBrowserToolbar();
	TSharedRef<SWidget> BuildBrowserTable();
	TSharedRef<SWidget> BuildBrowserFooter();
	TSharedRef<SWidget> BuildHostPanel();
	TSharedRef<SWidget> BuildCosmeticsPanel();

	void GoBack();
	void RequestQuit();
	void StartHosting();

	// Layout (logical = DPI-scaled Slate units)
	float GetPageWidth() const;
	float GetPageMaxHeight() const;
	bool IsCompactBrowser() const;

	// Server browser
	void RefreshBrowser();
	void HandleSessionsFound(bool bSuccess, const TArray<FKGSessionRow>& Rows);
	void SortRows();
	void SetSort(EKGBrowserColumn Column);
	int32 GetSortState(EKGBrowserColumn Column) const;
	void RebuildRows();
	bool IsRowVisible(const FKGSessionRow& Row) const;
	int32 CountVisibleRows() const;
	const FKGSessionRow* FindRow(int32 SearchIndex) const;
	void JoinSelected();
	void JoinRow(int32 SearchIndex);
	void JoinRowWithPassword(const FKGSessionRow& Row, const FString& Password);
	void HandleJoinResult(bool bSuccess, const FText& Message);
	void JoinByCodeOrAddress();
	void HandleCodeSearch(bool bSuccess, const TArray<FKGSessionRow>& Rows);
	void StartQuickMatch();
	void HandleQuickMatchSearch(bool bSuccess, const TArray<FKGSessionRow>& Rows);
	FKGHostOptions MakeHostOptions(bool bFromHostPanel) const;
	FText GetBrowserStatus() const;

	void ShowConnecting(const FText& What, TFunction<void()> OnCancel);
	void ShowModal(const TSharedRef<SKGModal>& Modal);
	void CloseModal();
	bool IsModalOpen() const;
	void FocusPage(EKGMainMenuPage Previous);
	int32 GetPageIndex() const;

	TWeakObjectPtr<APlayerController> OwningPlayer;
	bool bOverlay = false;
	FSimpleDelegate OnCloseRequested;

	EKGMainMenuPage Page = EKGMainMenuPage::Home;
	FVector2f ViewSize = FVector2f(1920.0f, 1080.0f);
	int32 SelectedMap = 0;
	int32 HostMaxPlayers = 12;
	int32 HostRegion = 0;
	FText ConnectingWhat;
	FString LocalAddress;
	bool bSettingsClosePending = false;
	bool bFocusAddressPending = false;

	TArray<TSharedPtr<SKGMenuButton>> HomeButtons;
	TSharedPtr<SKGMenuButton> StartHostButton;
	TSharedPtr<SKGMenuButton> RefreshButton;
	TSharedPtr<SKGMenuButton> CosmeticsBackButton;
	TSharedPtr<SEditableTextBox> AddressBox;
	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SEditableTextBox> HostNameBox;
	TSharedPtr<SEditableTextBox> HostPasswordBox;
	bool bHostPrivate = false;

	// Browser state
	TArray<FKGSessionRow> SessionRows;
	TSharedPtr<SVerticalBox> RowsBox;
	int32 SelectedSearchIndex = INDEX_NONE;
	int32 LastClickedSearchIndex = INDEX_NONE;
	double LastRowClickTime = 0.0;
	EKGBrowserColumn SortColumn = EKGBrowserColumn::Recommended;
	bool bSortAscending = true;
	FString SearchFilter;
	bool bHideFull = false;
	bool bHideInProgress = false;
	bool bHideLocked = false;
	int32 RegionFilter = 0; // 0 = any, else FKGSessions::GetRegions()[RegionFilter - 1]
	bool bSearching = false;
	bool bSearchedOnce = false;
	bool bQuickMatching = false;
	FString PendingJoinCode;
	float SearchTime = 0.0f;

	TSharedPtr<SWidget> CosmeticsContent;
	TSharedPtr<SOverlay> SettingsHost;
	TSharedPtr<SKGSettingsMenu> Settings;
	TSharedPtr<SOverlay> ModalHost;
	TWeakPtr<SWidget> FocusBeforeModal;

	FText ToastText;
	bool bToastError = false;
	float ToastTime = 0.0f;
	float Intro = 0.0f;     // seconds since construction
	float PanelAnim = 1.0f; // 0 -> 1 after each page change
	float SettingsAnim = 0.0f;
	float ConnectingTime = -1.0f;
};
