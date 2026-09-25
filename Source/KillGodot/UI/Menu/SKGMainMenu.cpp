#include "UI/Menu/SKGMainMenu.h"
#include "UI/Reveal/KGStreamerMode.h"

#include "Core/KGGameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformTime.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Menu/SKGMenuArt.h"
#include "UI/Menu/SKGMenuWidgets.h"
#include "UI/Menu/SKGSettingsMenu.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGMainMenu"

namespace
{
	/** Smaller text field for form rows and the browser toolbar (the shared style is sized for big fields). */
	const FEditableTextBoxStyle& MainMenuCompactTextBox()
	{
		static const FEditableTextBoxStyle Style = []()
		{
			FEditableTextBoxStyle Compact = FKGMenuStyle::Get().TextBox;
			Compact.SetFont(FKGMenuStyle::Font("Bold", 15, 20)).SetPadding(FMargin(12.0f, 8.0f));
			return Compact;
		}();
		return Style;
	}

	float MainMenuEaseOut(float T)
	{
		const float X = FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - X, 3.0f);
	}

	/** True when keyboard focus sits on the bare game viewport (map load, alt-tab) instead of any widget. */
	bool MainMenuFocusDrifted()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		static const FName ViewportType(TEXT("SViewport"));
		static const FName LayerManagerType(TEXT("SGameLayerManager"));
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
		return !Focused.IsValid() || Focused->GetType() == ViewportType || Focused->GetType() == LayerManagerType;
	}

	// Server browser column widths (logical units). Name takes the rest.
	constexpr float KGBrowserColLock = 34.0f;
	constexpr float KGBrowserColHost = 170.0f;
	constexpr float KGBrowserColMap = 170.0f;
	constexpr float KGBrowserColPlayers = 100.0f;
	constexpr float KGBrowserColPhase = 128.0f;
	constexpr float KGBrowserColPing = 84.0f;
	constexpr float KGBrowserColRegion = 96.0f;

	/** One table line (header or row): the same column boxes so everything lines up. */
	struct FKGBrowserCells
	{
		TSharedRef<SWidget> Lock = SNullWidget::NullWidget;
		TSharedRef<SWidget> Name = SNullWidget::NullWidget;
		TSharedRef<SWidget> Host = SNullWidget::NullWidget;
		TSharedRef<SWidget> Map = SNullWidget::NullWidget;
		TSharedRef<SWidget> Players = SNullWidget::NullWidget;
		TSharedRef<SWidget> Phase = SNullWidget::NullWidget;
		TSharedRef<SWidget> Ping = SNullWidget::NullWidget;
		TSharedRef<SWidget> Region = SNullWidget::NullWidget;
	};

	TSharedRef<SWidget> BrowserLine(const FKGBrowserCells& Cells, const TAttribute<EVisibility>& WideOnly)
	{
		auto Column = [](float Width, const TSharedRef<SWidget>& Content, EHorizontalAlignment Align)
		{
			return SNew(SBox).WidthOverride(Width).HAlign(Align).VAlign(VAlign_Center)[Content];
		};
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Column(KGBrowserColLock, Cells.Lock, HAlign_Left)]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)[Cells.Name]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).Visibility(WideOnly)[Column(KGBrowserColHost, Cells.Host, HAlign_Left)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Column(KGBrowserColMap, Cells.Map, HAlign_Left)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Column(KGBrowserColPlayers, Cells.Players, HAlign_Center)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Column(KGBrowserColPhase, Cells.Phase, HAlign_Center)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Column(KGBrowserColPing, Cells.Ping, HAlign_Right)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).Visibility(WideOnly)[Column(KGBrowserColRegion, Cells.Region, HAlign_Center)]
			];
	}

	TSharedRef<SWidget> BrowserCellText(const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color,
	                                    ETextJustify::Type Justify = ETextJustify::Left)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font)
			.ColorAndOpacity(Color)
			.Justification(Justify)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
	}

	/** Check box + label chip for the browser filters. */
	TSharedRef<SWidget> BrowserFilter(const FText& Label, TAttribute<bool> IsChecked, FKGOnBoolChanged OnToggled)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SKGCheckBox)
				.Size(24.0f)
				.IsChecked(IsChecked)
				.OnToggled(OnToggled)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(S.SmallFont)
				.ColorAndOpacity(S.CreamDim)
			];
	}

	int32 BrowserPingKey(const FKGSessionRow& Row)
	{
		return Row.PingMs < 0 ? 99999 : Row.PingMs;
	}

	/** Recommended order: joinable before full, lobbies before running games, fuller first, then lower ping. */
	bool BrowserRecommendedLess(const FKGSessionRow& A, const FKGSessionRow& B)
	{
		if (A.IsFull() != B.IsFull())
		{
			return !A.IsFull();
		}
		if (A.bInProgress != B.bInProgress)
		{
			return !A.bInProgress;
		}
		if (A.Players != B.Players)
		{
			return A.Players > B.Players;
		}
		return BrowserPingKey(A) < BrowserPingKey(B);
	}
}

SKGMainMenu::FCosmeticsPageFactory& SKGMainMenu::CosmeticsPageFactory()
{
	static FCosmeticsPageFactory Factory;
	return Factory;
}

void SKGMainMenu::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	OwningPlayer = InArgs._OwningPlayer;
	bOverlay = InArgs._bOverlay;
	OnCloseRequested = InArgs._OnCloseRequested;
	if (const UKGGameUserSettings* UserSettings = UKGGameUserSettings::Get())
	{
		HostMaxPlayers = FMath::Clamp(UserSettings->GetLastHostMaxPlayers(), KGMenu::MinPlayers, KGMenu::MaxPlayers);
	}
	LocalAddress = KGMenu::GetLocalAddress();
	HostRegion = FMath::Max(0, FKGSessions::GetRegions().IndexOfByKey(FKGSessions::GuessLocalRegion()));

	const FString Version = GetDefault<UGeneralProjectSettings>()->ProjectVersion;

	ChildSlot
	[
		SNew(SOverlay)
		// Backdrop
		+ SOverlay::Slot()
		[
			SNew(SKGMenuBackground)
			.DimLeft_Lambda([this]() { return Page == EKGMainMenuPage::Home ? FMath::Lerp(0.85f, 0.45f, SettingsAnim) : 0.35f; })
		]
		// Dim behind pages and the settings panel
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Visibility(EVisibility::HitTestInvisible)
			.Image(&S.WhiteBrush)
			.ColorAndOpacity_Lambda([this]()
			{
				const float PageDim = Page == EKGMainMenuPage::Home || Page == EKGMainMenuPage::Settings ? 0.0f : 0.35f;
				return FSlateColor(FKGMenuStyle::WithAlpha(FKGMenuStyle::Get().Ink, FMath::Max(PageDim, 0.55f * SettingsAnim)));
			})
		]
		// Home: logo, button list, footer (left column)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(TAttribute<FMargin>::CreateLambda([this]()
		{
			// 96 px margin on wide screens, less on narrow ones.
			return FMargin(FMath::Clamp(ViewSize.X * 0.05f, 40.0f, 96.0f), 56.0f, 0.0f, 36.0f);
		}))
		[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return FMath::Min(600.0f, ViewSize.X - 80.0f); })
			.Visibility_Lambda([this]()
			{
				return Page != EKGMainMenuPage::Home || SettingsAnim > 0.98f ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
			})
			.RenderTransform_Lambda([this]()
			{
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(-90.0f * SettingsAnim, 0.0f)));
			})
			[
				SNew(SBorder)
				.BorderImage(&S.NoBrush)
				.Padding(0.0f)
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f - SettingsAnim); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(&S.NoBrush)
						.Padding(0.0f)
						.ColorAndOpacity_Lambda([this]()
						{
							return FLinearColor(1.0f, 1.0f, 1.0f, MainMenuEaseOut((Intro - 0.15f) / 0.7f));
						})
						.RenderTransform_Lambda([this]()
						{
							const float E = MainMenuEaseOut((Intro - 0.15f) / 0.7f);
							return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.0f, -36.0f * (1.0f - E))));
						})
						[
							SNew(SKGLogo)
							.Size(92.0f)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SSpacer)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					[
						BuildHomeList()
					]
					+ SVerticalBox::Slot()
					.FillHeight(0.5f)
					[
						SNew(SSpacer)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Footer", "v{0}  \u00B7  Prototype  \u00B7  {1}"), FText::FromString(Version),
						                    FKGSessions::Get().GetBackendName(OwningPlayer.Get())))
						.Font(S.SmallFont)
						.ColorAndOpacity(S.Muted)
					]
				]
			]
		]
		// Pages (browser, host, cosmetics), centered and sized from the viewport
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(24.0f, 28.0f))
		[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return GetPageWidth(); })
			.MaxDesiredHeight_Lambda([this]() { return GetPageMaxHeight(); })
			.MinDesiredHeight_Lambda([this]() { return Page == EKGMainMenuPage::Play ? GetPageMaxHeight() : 0.0f; })
			.Visibility_Lambda([this]()
			{
				return Page == EKGMainMenuPage::Home || Page == EKGMainMenuPage::Settings || SettingsAnim > 0.02f
					? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
			})
			.RenderTransform_Lambda([this]()
			{
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.0f, 36.0f * (1.0f - MainMenuEaseOut(PanelAnim)))));
			})
			[
				SNew(SBorder)
				.BorderImage(&S.NoBrush)
				.Padding(0.0f)
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, MainMenuEaseOut(PanelAnim)); })
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex(this, &SKGMainMenu::GetPageIndex)
					+ SWidgetSwitcher::Slot()[SNew(SSpacer)]
					+ SWidgetSwitcher::Slot()[BuildBrowserPage()]
					+ SWidgetSwitcher::Slot()[BuildHostPanel()]
					+ SWidgetSwitcher::Slot()[BuildCosmeticsPanel()]
				]
			]
		]
		// Key hints (home only; pages have their own buttons)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 48.0f, 32.0f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]()
			{
				return Page == EKGMainMenuPage::Home && SettingsAnim < 0.02f && ViewSize.X > 1200.0f ? EVisibility::HitTestInvisible
				                                                                                      : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 20.0f, 0.0f)
			[
				KGMenuUI::MakeKeyHint(LOCTEXT("KeyArrows", "\u2191 \u2193"), LOCTEXT("HintNavigate", "Navigate"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 20.0f, 0.0f)
			[
				KGMenuUI::MakeKeyHint(LOCTEXT("KeyEnter", "Enter"), LOCTEXT("HintSelect", "Select"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				KGMenuUI::MakeKeyHint(LOCTEXT("KeyEsc", "Esc"), LOCTEXT("HintBack", "Back"))
			]
		]
		// Settings panel
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(24.0f, 40.0f))
		[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return FMath::Max(640.0f, FMath::Min(1180.0f, ViewSize.X - 48.0f)); })
			.Visibility_Lambda([this]() { return Settings.IsValid() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				SAssignNew(SettingsHost, SOverlay)
			]
		]
		// Toast
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 64.0f))
		[
			SNew(SBorder)
			.BorderImage(&S.ToastBrush)
			.Padding(FMargin(26.0f, 14.0f))
			.Visibility_Lambda([this]() { return ToastTime > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]()
			{
				return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(ToastTime / 0.35f, 0.0f, 1.0f));
			})
			.RenderTransform_Lambda([this]()
			{
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.0f, 16.0f * (1.0f - FMath::Clamp(ToastTime / 0.35f, 0.0f, 1.0f)))));
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(10.0f)
					.HeightOverride(10.0f)
					[
						SNew(SImage)
						.Image(&S.WhiteBrush)
						.ColorAndOpacity_Lambda([this]()
						{
							return FSlateColor(bToastError ? FKGMenuStyle::Get().Crimson : FKGMenuStyle::Get().Gold);
						})
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MaxDesiredWidth(720.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return ToastText; })
						.Font(S.BodyBoldFont)
						.ColorAndOpacity(S.Cream)
						.AutoWrapText(true)
					]
				]
			]
		]
		// Modal dialogs
		+ SOverlay::Slot()
		[
			SAssignNew(ModalHost, SOverlay)
		]
		// Fade in from black
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Visibility(EVisibility::HitTestInvisible)
			.Image(&S.WhiteBrush)
			.ColorAndOpacity_Lambda([this]()
			{
				return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f - MainMenuEaseOut(Intro / 0.9f)));
			})
		]
	];

	for (int32 Index = 0; Index < HomeButtons.Num(); ++Index)
	{
		HomeButtons[Index]->PlayIntro(0.45f + 0.07f * Index);
	}

	KGMenu::RegisterNetworkErrorHooks();
	const FText NetworkError = KGMenu::ConsumeLastNetworkError();
	if (!NetworkError.IsEmpty())
	{
		ShowToast(NetworkError, true);
	}
}

// --- Layout -----------------------------------------------------------------------------------------------------------

float SKGMainMenu::GetPageWidth() const
{
	const float Available = FMath::Max(560.0f, ViewSize.X - 48.0f);
	switch (Page)
	{
	case EKGMainMenuPage::Play:
		return FMath::Min(1560.0f, Available);
	case EKGMainMenuPage::Host:
		return FMath::Min(780.0f, Available);
	case EKGMainMenuPage::Cosmetics:
		return FMath::Min(900.0f, Available);
	default:
		return FMath::Min(780.0f, Available);
	}
}

float SKGMainMenu::GetPageMaxHeight() const
{
	const float Available = FMath::Max(420.0f, ViewSize.Y - 56.0f);
	return Page == EKGMainMenuPage::Play ? FMath::Min(1000.0f, Available) : Available;
}

bool SKGMainMenu::IsCompactBrowser() const
{
	return GetPageWidth() < 1240.0f;
}

// --- Home -------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGMainMenu::BuildHomeList()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [this, &Box](const FText& Text, FSimpleDelegate OnClick, TAttribute<bool> Selected)
	{
		TSharedPtr<SKGMenuButton> Button;
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SAssignNew(Button, SKGMenuButton)
			.Kind(EKGButtonKind::Menu)
			.Height(64.0f)
			.MinWidth(460.0f)
			.TextAlign(HAlign_Left)
			.Text(Text)
			.IsSelected(Selected)
			.OnClicked(OnClick)
		];
		HomeButtons.Add(Button);
	};

	Add(LOCTEXT("Play", "Play"), FSimpleDelegate::CreateSP(this, &SKGMainMenu::OpenPage, EKGMainMenuPage::Play), false);
	Add(LOCTEXT("Cosmetics", "Cosmetics"), FSimpleDelegate::CreateSP(this, &SKGMainMenu::OpenCosmeticsScreen), false);
	Add(LOCTEXT("Settings", "Settings"), FSimpleDelegate::CreateSP(this, &SKGMainMenu::OpenPage, EKGMainMenuPage::Settings),
	    false);
	if (bOverlay)
	{
		Add(LOCTEXT("Close", "Close menu"), FSimpleDelegate::CreateLambda([this]() { OnCloseRequested.ExecuteIfBound(); }), false);
	}
	Add(LOCTEXT("Quit", "Quit"), FSimpleDelegate::CreateSP(this, &SKGMainMenu::RequestQuit), false);
	// Every entry gets the same width (the longest label wins), like a column of signboards.
	return SNew(SBox).MinDesiredWidth(480.0f)[Box];
}

// --- Server browser ---------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGMainMenu::BuildBrowserPage()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const FText Backend = FKGSessions::Get().GetBackendName(OwningPlayer.Get());

	return SNew(SBorder)
		.BorderImage(&S.PanelBrush)
		.Padding(FMargin(32.0f, 24.0f, 32.0f, 22.0f))
		[
			SNew(SVerticalBox)
			// Header: back, title, primary actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 22.0f, 0.0f)
				[
					SNew(SKGMenuButton)
					.Kind(EKGButtonKind::Ghost)
					.MinWidth(110.0f)
					.Height(46.0f)
					.Text(LOCTEXT("Back", "Back"))
					.OnClicked(this, &SKGMainMenu::GoBack)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BrowserTitle", "Server browser"))
						.Font(S.HeadingFont)
						.ColorAndOpacity(S.Cream)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("BrowserSubtitle", "Player-hosted matches  \u00B7  {0}"), Backend))
						.Font(S.SmallFont)
						.ColorAndOpacity(S.Muted)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SKGMenuButton)
					.Kind(EKGButtonKind::Secondary)
					.MinWidth(170.0f)
					.Height(50.0f)
					.Text(LOCTEXT("QuickMatch", "Quick match"))
					.OnClicked(this, &SKGMainMenu::StartQuickMatch)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SKGMenuButton)
					.Kind(EKGButtonKind::Accent)
					.MinWidth(170.0f)
					.Height(50.0f)
					.Text(LOCTEXT("HostGame", "Host game"))
					.OnClicked(FSimpleDelegate::CreateSP(this, &SKGMainMenu::OpenPage, EKGMainMenuPage::Host))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 18.0f, 0.0f, 0.0f)
			[
				BuildBrowserToolbar()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				BuildBrowserTable()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				BuildBrowserFooter()
			]
		];
}

TSharedRef<SWidget> SKGMainMenu::BuildBrowserToolbar()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TArray<FText> RegionNames = {LOCTEXT("AnyRegion", "Any region")};
	for (const FString& Region : FKGSessions::GetRegions())
	{
		RegionNames.Add(FText::AsCultureInvariant(Region));
	}

	return SNew(SVerticalBox)
		// Search, region, refresh
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SearchBox, SEditableTextBox)
				.Style(&MainMenuCompactTextBox())
				.HintText(LOCTEXT("SearchHint", "Search by name, host, map or code"))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					SearchFilter = Text.ToString().TrimStartAndEnd();
					RebuildRows();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(220.0f)
				[
					SNew(SKGOptionSelector)
					.Options(RegionNames)
					.SelectedIndex_Lambda([this]() { return RegionFilter; })
					.OnSelectionChanged_Lambda([this](int32 Index)
					{
						RegionFilter = Index;
						RebuildRows();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(RefreshButton, SKGMenuButton)
				.Kind(EKGButtonKind::Secondary)
				.MinWidth(170.0f)
				.Height(46.0f)
				.IsEnabled_Lambda([this]() { return !bSearching; })
				.OnClicked(this, &SKGMainMenu::RefreshBrowser)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]() { return bSearching ? 1 : 0; })
						+ SWidgetSwitcher::Slot()[SNew(SKGGlyph).Glyph(EKGGlyph::Refresh).Size(18.0f).Color(S.Cream)]
						+ SWidgetSwitcher::Slot()[SNew(SKGGlyph).Glyph(EKGGlyph::Spinner).Size(18.0f).Color(S.Gold)]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(S.ButtonFont)
						.ColorAndOpacity(S.Cream)
						.Text_Lambda([this]() { return bSearching ? LOCTEXT("Searching", "Searching") : LOCTEXT("Refresh", "Refresh"); })
					]
				]
			]
		]
		// Filters + result count
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 24.0f, 0.0f)
			[
				BrowserFilter(LOCTEXT("HideFull", "Hide full"), TAttribute<bool>::CreateLambda([this]() { return bHideFull; }),
				              FKGOnBoolChanged::CreateLambda([this](bool bValue) { bHideFull = bValue; RebuildRows(); }))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 24.0f, 0.0f)
			[
				BrowserFilter(LOCTEXT("HideInProgress", "Hide in progress"),
				              TAttribute<bool>::CreateLambda([this]() { return bHideInProgress; }),
				              FKGOnBoolChanged::CreateLambda([this](bool bValue) { bHideInProgress = bValue; RebuildRows(); }))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				BrowserFilter(LOCTEXT("HideLocked", "Hide passworded"),
				              TAttribute<bool>::CreateLambda([this]() { return bHideLocked; }),
				              FKGOnBoolChanged::CreateLambda([this](bool bValue) { bHideLocked = bValue; RebuildRows(); }))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(S.BodyBoldFont)
				.ColorAndOpacity(S.Gold)
				.Text(this, &SKGMainMenu::GetBrowserStatus)
			]
		];
}

TSharedRef<SWidget> SKGMainMenu::BuildBrowserTable()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const TAttribute<EVisibility> WideOnly = TAttribute<EVisibility>::CreateLambda([this]()
	{
		return IsCompactBrowser() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
	});
	auto Header = [this](const FText& Text, EKGBrowserColumn Column, ETextJustify::Type Justify = ETextJustify::Left)
	{
		return SNew(SKGSortHeader)
			.Text(Text)
			.Justify(Justify)
			.SortState_Lambda([this, Column]() { return GetSortState(Column); })
			.OnClicked(FSimpleDelegate::CreateSP(this, &SKGMainMenu::SetSort, Column));
	};

	FKGBrowserCells Captions;
	Captions.Lock = SNew(SKGSortHeader)
		.SortState_Lambda([this]() { return GetSortState(EKGBrowserColumn::Lock); })
		.OnClicked(FSimpleDelegate::CreateSP(this, &SKGMainMenu::SetSort, EKGBrowserColumn::Lock))
		.Text(FText::GetEmpty());
	Captions.Name = Header(LOCTEXT("ColName", "Name"), EKGBrowserColumn::Name);
	Captions.Host = Header(LOCTEXT("ColHost", "Host"), EKGBrowserColumn::Host);
	Captions.Map = Header(LOCTEXT("ColMap", "Map"), EKGBrowserColumn::Map);
	Captions.Players = Header(LOCTEXT("ColPlayers", "Players"), EKGBrowserColumn::Players, ETextJustify::Center);
	Captions.Phase = Header(LOCTEXT("ColPhase", "Phase"), EKGBrowserColumn::Phase, ETextJustify::Center);
	Captions.Ping = Header(LOCTEXT("ColPing", "Ping"), EKGBrowserColumn::Ping, ETextJustify::Right);
	Captions.Region = Header(LOCTEXT("ColRegion", "Region"), EKGBrowserColumn::Region, ETextJustify::Center);

	return SNew(SBorder)
		.BorderImage(&S.InsetBrush)
		.Padding(FMargin(10.0f, 10.0f, 6.0f, 10.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(18.0f, 2.0f, 28.0f, 10.0f)
			[
				BrowserLine(Captions, WideOnly)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SScrollBox)
					.Style(&S.ScrollBox)
					.ScrollBarStyle(&S.ScrollBar)
					.ScrollBarThickness(FVector2D(6.0f, 6.0f))
					.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
					.NavigationDestination(EDescendantScrollDestination::IntoView)
					+ SScrollBox::Slot()
					[
						SAssignNew(RowsBox, SVerticalBox)
					]
				]
				// Searching (nothing listed yet)
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([this]()
					{
						return bSearching && CountVisibleRows() == 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 14.0f, 0.0f)
					[
						SNew(SKGGlyph).Glyph(EKGGlyph::Spinner).Size(30.0f).Thickness(3.5f).Color(S.Gold)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SearchingBody", "Looking for villages..."))
						.Font(S.SubheadingFont)
						.ColorAndOpacity(S.CreamDim)
					]
				]
				// Empty state
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([this]()
					{
						return !bSearching && bSearchedOnce && CountVisibleRows() == 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
					})
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return SessionRows.Num() > 0 ? LOCTEXT("EmptyFilteredTitle", "No game matches your filters.")
							                             : LOCTEXT("EmptyTitle", "Nobody came. Not even Godot.");
						})
						.Font(S.SubheadingFont)
						.ColorAndOpacity(S.CreamDim)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return SessionRows.Num() > 0 ? LOCTEXT("EmptyFiltered", "Clear the search or the filters to see every open game.")
							                             : LOCTEXT("EmptyHint", "Host one and your friends will find it here, or join by code.");
						})
						.Font(S.SmallFont)
						.ColorAndOpacity(S.Muted)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 20.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 14.0f, 0.0f)
						[
							SNew(SKGMenuButton)
							.Kind(EKGButtonKind::Accent)
							.MinWidth(200.0f)
							.Height(52.0f)
							.Text(LOCTEXT("EmptyHost", "Host a game"))
							.OnClicked(FSimpleDelegate::CreateSP(this, &SKGMainMenu::OpenPage, EKGMainMenuPage::Host))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SKGMenuButton)
							.Kind(EKGButtonKind::Ghost)
							.MinWidth(160.0f)
							.Height(52.0f)
							.Text(LOCTEXT("EmptyRefresh", "Search again"))
							.OnClicked(this, &SKGMainMenu::RefreshBrowser)
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SKGMainMenu::BuildBrowserFooter()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const UKGGameUserSettings* UserSettings = UKGGameUserSettings::Get();
	const FString LastAddress = UserSettings ? UserSettings->GetLastJoinAddress() : FString();

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("JoinByCaption", "Join by code or address"))
			.Font(S.CaptionFont)
			.ColorAndOpacity(S.Muted)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
			.Visibility_Lambda([this]() { return IsCompactBrowser() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			[
				SAssignNew(AddressBox, SEditableTextBox)
				.Style(&MainMenuCompactTextBox())
				.Text(FText::FromString(LastAddress))
				.IsPassword_Lambda([]() { return KGStreamer::IsEnabled(); })   // streamer mode: codes / IPs never on screen
				.HintText(LOCTEXT("AddressHint", "ABC123 or 192.168.1.20:7777"))
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(false)
				.RevertTextOnEscape(false)
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit)
				{
					if (Commit == ETextCommit::OnEnter)
					{
						JoinByCodeOrAddress();
					}
				})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Secondary)
			.MinWidth(110.0f)
			.Height(46.0f)
			.Text(LOCTEXT("JoinCode", "Connect"))
			.OnClicked(this, &SKGMainMenu::JoinByCodeOrAddress)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 16.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(S.SmallFont)
			.ColorAndOpacity(S.Muted)
			.Visibility_Lambda([this]() { return IsCompactBrowser() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.Text(LOCTEXT("DoubleClickHint", "Double-click a game to join"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Accent)
			.MinWidth(200.0f)
			.Height(54.0f)
			.Text(LOCTEXT("BrowserJoin", "Join"))
			.IsEnabled_Lambda([this]()
			{
				const FKGSessionRow* Row = FindRow(SelectedSearchIndex);
				return Row && IsRowVisible(*Row);
			})
			.OnClicked(this, &SKGMainMenu::JoinSelected)
		];
}

FText SKGMainMenu::GetBrowserStatus() const
{
	if (bSearching && SessionRows.Num() == 0)
	{
		const int32 Dots = 1 + (FMath::FloorToInt(SearchTime * 2.5f) % 3);
		return FText::Format(LOCTEXT("SearchingStatus", "Searching{0}"), FText::FromString(FString::ChrN(Dots, TEXT('.'))));
	}
	if (!bSearchedOnce)
	{
		return FText::GetEmpty();
	}
	const int32 Visible = CountVisibleRows();
	const int32 Hidden = SessionRows.Num() - Visible;
	const FText Found = FText::Format(LOCTEXT("ServersFound", "{0} {0}|plural(one=server,other=servers) found"), FText::AsNumber(Visible));
	return Hidden > 0 ? FText::Format(LOCTEXT("ServersHidden", "{0}  ({1} hidden by filters)"), Found, FText::AsNumber(Hidden)) : Found;
}

bool SKGMainMenu::IsRowVisible(const FKGSessionRow& Row) const
{
	if ((bHideFull && Row.IsFull()) || (bHideInProgress && Row.bInProgress) || (bHideLocked && Row.bPassword))
	{
		return false;
	}
	const TArray<FString>& Regions = FKGSessions::GetRegions();
	if (RegionFilter > 0 && Regions.IsValidIndex(RegionFilter - 1) && Row.Region != Regions[RegionFilter - 1])
	{
		return false;
	}
	if (!SearchFilter.IsEmpty())
	{
		return Row.Name.Contains(SearchFilter) || Row.HostName.Contains(SearchFilter) || Row.MapTitle.Contains(SearchFilter) ||
			(!Row.Code.IsEmpty() && Row.Code == FKGSessions::NormalizeJoinCode(SearchFilter));
	}
	return true;
}

int32 SKGMainMenu::CountVisibleRows() const
{
	int32 Count = 0;
	for (const FKGSessionRow& Row : SessionRows)
	{
		Count += IsRowVisible(Row) ? 1 : 0;
	}
	return Count;
}

const FKGSessionRow* SKGMainMenu::FindRow(int32 SearchIndex) const
{
	return SearchIndex == INDEX_NONE ? nullptr
		: SessionRows.FindByPredicate([SearchIndex](const FKGSessionRow& Row) { return Row.SearchIndex == SearchIndex; });
}

void SKGMainMenu::RefreshBrowser()
{
	APlayerController* PC = OwningPlayer.Get();
	if (!PC || bSearching)
	{
		return;
	}
	bSearching = true;
	SearchTime = 0.0f;
	// Old rows point into the previous search: clear them so a stale row can't be joined.
	SessionRows.Reset();
	SelectedSearchIndex = INDEX_NONE;
	RebuildRows();
	FKGSessions::Get().Find(PC, FKGOnSessionsFound::CreateSP(this, &SKGMainMenu::HandleSessionsFound));
}

void SKGMainMenu::HandleSessionsFound(bool bSuccess, const TArray<FKGSessionRow>& Rows)
{
	bSearching = false;
	bSearchedOnce = true;
	SessionRows = Rows;
	SortRows();
	SelectedSearchIndex = INDEX_NONE;
	for (const FKGSessionRow& Row : SessionRows)
	{
		if (IsRowVisible(Row) && !Row.IsFull())
		{
			SelectedSearchIndex = Row.SearchIndex;
			break;
		}
	}
	RebuildRows();
	if (!bSuccess)
	{
		ShowToast(LOCTEXT("SearchFailed", "The game search failed. Check your network and try Refresh."), true);
	}
}

void SKGMainMenu::DebugSetRows(const TArray<FKGSessionRow>& Rows)
{
	FKGSessions::Get().CancelFind(OwningPlayer.Get());
	HandleSessionsFound(true, Rows);
}

void SKGMainMenu::SortRows()
{
	const EKGBrowserColumn Column = SortColumn;
	const bool bAscending = bSortAscending;
	SessionRows.StableSort([Column, bAscending](const FKGSessionRow& A, const FKGSessionRow& B)
	{
		auto Order = [bAscending](int32 Compare) { return bAscending ? Compare < 0 : Compare > 0; };
		int32 Compare = 0;
		switch (Column)
		{
		case EKGBrowserColumn::Lock:
			Compare = static_cast<int32>(A.bPassword) - static_cast<int32>(B.bPassword);
			break;
		case EKGBrowserColumn::Name:
			Compare = A.Name.Compare(B.Name, ESearchCase::IgnoreCase);
			break;
		case EKGBrowserColumn::Host:
			Compare = A.HostName.Compare(B.HostName, ESearchCase::IgnoreCase);
			break;
		case EKGBrowserColumn::Map:
			Compare = A.MapTitle.Compare(B.MapTitle, ESearchCase::IgnoreCase);
			break;
		case EKGBrowserColumn::Players:
			Compare = A.Players != B.Players ? A.Players - B.Players : A.MaxPlayers - B.MaxPlayers;
			break;
		case EKGBrowserColumn::Phase:
			Compare = static_cast<int32>(A.bInProgress) - static_cast<int32>(B.bInProgress);
			break;
		case EKGBrowserColumn::Ping:
			Compare = BrowserPingKey(A) - BrowserPingKey(B);
			break;
		case EKGBrowserColumn::Region:
			Compare = A.Region.Compare(B.Region, ESearchCase::IgnoreCase);
			break;
		default:
			break;
		}
		return Compare != 0 ? Order(Compare) : BrowserRecommendedLess(A, B);
	});
}

void SKGMainMenu::SetSort(EKGBrowserColumn Column)
{
	if (SortColumn == Column)
	{
		bSortAscending = !bSortAscending;
	}
	else
	{
		SortColumn = Column;
		// Numbers read best high-to-low (players) or low-to-high (ping); text A-Z.
		bSortAscending = Column != EKGBrowserColumn::Players;
	}
	SortRows();
	RebuildRows();
}

int32 SKGMainMenu::GetSortState(EKGBrowserColumn Column) const
{
	return SortColumn == Column ? (bSortAscending ? 1 : -1) : 0;
}

void SKGMainMenu::RebuildRows()
{
	if (!RowsBox.IsValid())
	{
		return;
	}
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	RowsBox->ClearChildren();
	if (const FKGSessionRow* Selected = FindRow(SelectedSearchIndex); Selected && !IsRowVisible(*Selected))
	{
		SelectedSearchIndex = INDEX_NONE;
	}
	const TAttribute<EVisibility> WideOnly = TAttribute<EVisibility>::CreateLambda([this]()
	{
		return IsCompactBrowser() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
	});
	for (const FKGSessionRow& Row : SessionRows)
	{
		if (!IsRowVisible(Row))
		{
			continue;
		}
		const int32 SearchIndex = Row.SearchIndex;
		const FLinearColor PingColor = Row.PingMs < 0 ? S.Muted : (Row.PingMs < 80 ? S.Good : (Row.PingMs < 160 ? S.Gold : S.Crimson));

		FKGBrowserCells Cells;
		Cells.Lock = SNew(SBox)
			.Visibility(Row.bPassword ? EVisibility::HitTestInvisible : EVisibility::Hidden)
			[
				SNew(SKGGlyph).Glyph(EKGGlyph::Lock).Size(18.0f).Thickness(2.2f).Color(S.Gold)
			];
		Cells.Name = BrowserCellText(FText::FromString(Row.Name), S.BodyBoldFont, S.Cream);
		// Streamer mode: host names are other players' names too (a pseudonym per menu visit).
		const UWorld* MenuWorld = OwningPlayer.IsValid() ? OwningPlayer->GetWorld() : nullptr;
		Cells.Host = BrowserCellText(FText::FromString(KGStreamer::DisplayHostName(MenuWorld, Row.HostName)), S.SmallFont, S.CreamDim);
		Cells.Map = BrowserCellText(FText::FromString(Row.MapTitle), S.SmallFont, S.CreamDim);
		Cells.Players = BrowserCellText(FText::Format(LOCTEXT("PlayersOf", "{0} / {1}"), FText::AsNumber(Row.Players),
		                                              FText::AsNumber(Row.MaxPlayers)),
		                                S.BodyBoldFont, Row.IsFull() ? FMath::Lerp(S.Crimson, S.Cream, 0.3f) : S.Cream, ETextJustify::Center);
		Cells.Phase = BrowserCellText(Row.bInProgress ? LOCTEXT("StatusPlaying", "IN PROGRESS") : LOCTEXT("StatusLobby", "LOBBY"),
		                              S.CaptionFont, Row.bInProgress ? S.Lantern : S.Good, ETextJustify::Center);
		Cells.Ping = BrowserCellText(Row.PingMs < 0 ? FText::AsCultureInvariant(TEXT("-"))
		                                            : FText::Format(LOCTEXT("PingMs", "{0} ms"), FText::AsNumber(Row.PingMs)),
		                             S.BodyBoldFont, PingColor, ETextJustify::Right);
		Cells.Region = BrowserCellText(FText::AsCultureInvariant(Row.Region.IsEmpty() ? FString(TEXT("-")) : Row.Region),
		                               S.SmallFont, S.CreamDim, ETextJustify::Center);

		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 10.0f, 5.0f)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Row)
			.Height(46.0f)
			.IsSelected_Lambda([this, SearchIndex]() { return SelectedSearchIndex == SearchIndex; })
			.OnClicked_Lambda([this, SearchIndex]()
			{
				// Click selects; a second click on the same row within a moment joins (double click).
				const double Now = FPlatformTime::Seconds();
				const bool bDouble = LastClickedSearchIndex == SearchIndex && Now - LastRowClickTime < 0.45;
				LastClickedSearchIndex = SearchIndex;
				LastRowClickTime = Now;
				SelectedSearchIndex = SearchIndex;
				if (bDouble)
				{
					JoinRow(SearchIndex);
				}
			})
			[
				BrowserLine(Cells, WideOnly)
			]
		];
	}
}

void SKGMainMenu::JoinSelected()
{
	JoinRow(SelectedSearchIndex);
}

void SKGMainMenu::JoinRow(int32 SearchIndex)
{
	const FKGSessionRow* Found = FindRow(SearchIndex);
	if (!Found)
	{
		return;
	}
	const FKGSessionRow Row = *Found;
	if (Row.IsFull())
	{
		ShowToast(LOCTEXT("RowFull", "That game is full."), true);
		return;
	}
	if (!Row.bPassword)
	{
		JoinRowWithPassword(Row, FString());
		return;
	}

	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedPtr<SEditableTextBox> PasswordBox;
	TSharedRef<TFunction<void()>> Confirm = MakeShared<TFunction<void()>>();
	TSharedRef<SKGModal> Modal = SNew(SKGModal)
		.Title(LOCTEXT("PasswordTitle", "Password required"))
		.Body(FText::Format(LOCTEXT("PasswordBody", "\"{0}\" is locked. Type the password the host gave you."), FText::FromString(Row.Name)))
		.ExtraContent()
		[
			SAssignNew(PasswordBox, SEditableTextBox)
			.Style(&S.TextBox)
			.IsPassword(true)
			.HintText(LOCTEXT("PasswordHint", "Password"))
			.OnTextCommitted_Lambda([Confirm](const FText&, ETextCommit::Type Commit)
			{
				if (Commit == ETextCommit::OnEnter && *Confirm)
				{
					(*Confirm)();
				}
			})
		]
		.InitialFocus(PasswordBox)
		.ConfirmText(LOCTEXT("PasswordJoin", "Join"))
		.CancelText(LOCTEXT("PasswordCancel", "Cancel"))
		.OnConfirm_Lambda([Confirm]() { (*Confirm)(); })
		.OnCancel(this, &SKGMainMenu::CloseModal);
	TWeakPtr<SEditableTextBox> WeakBox = PasswordBox;
	*Confirm = [this, Row, WeakBox]()
	{
		const TSharedPtr<SEditableTextBox> Box = WeakBox.Pin();
		const FString Password = Box.IsValid() ? Box->GetText().ToString() : FString();
		CloseModal();
		JoinRowWithPassword(Row, Password);
	};
	ShowModal(Modal);
}

void SKGMainMenu::JoinRowWithPassword(const FKGSessionRow& Row, const FString& Password)
{
	APlayerController* PC = OwningPlayer.Get();
	if (!PC)
	{
		return;
	}
	ShowConnecting(FText::Format(LOCTEXT("JoiningRow", "Joining \"{0}\""), FText::FromString(Row.Name)), [this]()
	{
		KGMenu::CancelJoin(OwningPlayer.Get());
		FKGSessions::Get().EndSession(OwningPlayer.Get());
	});
	FKGSessions::Get().Join(PC, Row.SearchIndex, Password, FKGOnSessionResult::CreateSP(this, &SKGMainMenu::HandleJoinResult));
}

void SKGMainMenu::HandleJoinResult(bool bSuccess, const FText& Message)
{
	bQuickMatching = false;
	if (!bSuccess)
	{
		ConnectingTime = -1.0f;
		CloseModal();
		ShowToast(Message, true);
	}
}

void SKGMainMenu::JoinByCodeOrAddress()
{
	const FString Input = AddressBox.IsValid() ? AddressBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (Input.IsEmpty())
	{
		ShowToast(LOCTEXT("NeedCode", "Type a join code (6 letters) or the host's address."), true);
		KGMenu::FocusWidget(AddressBox);
		return;
	}
	if (FKGSessions::LooksLikeJoinCode(Input))
	{
		const FString Code = FKGSessions::NormalizeJoinCode(Input);
		if (const FKGSessionRow* Row = SessionRows.FindByPredicate([&Code](const FKGSessionRow& Candidate) { return Candidate.Code == Code; }))
		{
			JoinRow(Row->SearchIndex);
			return;
		}
		// Not in the current list: search, then join the match with that code.
		APlayerController* PC = OwningPlayer.Get();
		if (!PC)
		{
			return;
		}
		PendingJoinCode = Code;
		FKGSessions::Get().CancelFind(PC);
		bSearching = false;
		ShowConnecting(FText::Format(LOCTEXT("FindingCode", "Looking for game {0}"), FText::AsCultureInvariant(KGStreamer::MaskCode(Code))), [this]()
		{
			PendingJoinCode.Reset();
			FKGSessions::Get().CancelFind(OwningPlayer.Get());
			KGMenu::CancelJoin(OwningPlayer.Get());
			FKGSessions::Get().EndSession(OwningPlayer.Get());
		});
		bSearching = true;
		SearchTime = 0.0f;
		FKGSessions::Get().Find(PC, FKGOnSessionsFound::CreateSP(this, &SKGMainMenu::HandleCodeSearch));
		return;
	}
	FText Error;
	if (!KGMenu::JoinGame(OwningPlayer.Get(), Input, Error))
	{
		ShowToast(Error, true);
		KGMenu::FocusWidget(AddressBox);
		return;
	}
	ShowConnecting(FText::Format(LOCTEXT("JoiningAddress", "Connecting to {0}"), FText::FromString(KGStreamer::MaskAddress(KGMenu::NormalizeAddress(Input)))),
	               [this]() { KGMenu::CancelJoin(OwningPlayer.Get()); });
}

void SKGMainMenu::HandleCodeSearch(bool bSuccess, const TArray<FKGSessionRow>& Rows)
{
	const FString Code = PendingJoinCode;
	PendingJoinCode.Reset();
	HandleSessionsFound(bSuccess, Rows);
	if (Code.IsEmpty())
	{
		return; // cancelled
	}
	ConnectingTime = -1.0f;
	CloseModal();
	if (const FKGSessionRow* Row = SessionRows.FindByPredicate([&Code](const FKGSessionRow& Candidate) { return Candidate.Code == Code; }))
	{
		SelectedSearchIndex = Row->SearchIndex;
		JoinRow(Row->SearchIndex);
		return;
	}
	ShowToast(FText::Format(LOCTEXT("CodeNotFound", "No open game with code {0}. Private games join by address."),
	                        FText::AsCultureInvariant(KGStreamer::MaskCode(Code))), true);
}

void SKGMainMenu::StartQuickMatch()
{
	APlayerController* PC = OwningPlayer.Get();
	if (!PC)
	{
		return;
	}
	FKGSessions::Get().CancelFind(PC);
	bSearching = false;
	bQuickMatching = true;
	ShowConnecting(LOCTEXT("QuickFinding", "Looking for an open game"), [this]()
	{
		bQuickMatching = false;
		FKGSessions::Get().CancelFind(OwningPlayer.Get());
		KGMenu::CancelJoin(OwningPlayer.Get());
		FKGSessions::Get().EndSession(OwningPlayer.Get());
	});
	FKGSessions::Get().Find(PC, FKGOnSessionsFound::CreateSP(this, &SKGMainMenu::HandleQuickMatchSearch));
}

void SKGMainMenu::HandleQuickMatchSearch(bool bSuccess, const TArray<FKGSessionRow>& Rows)
{
	if (!bQuickMatching)
	{
		return;
	}
	HandleSessionsFound(bSuccess, Rows);
	APlayerController* PC = OwningPlayer.Get();
	const int32 Pick = FKGSessions::PickQuickMatch(Rows);
	if (PC && Pick != INDEX_NONE)
	{
		ConnectingWhat = FText::Format(LOCTEXT("JoiningRow", "Joining \"{0}\""), FText::FromString(Rows[Pick].Name));
		FKGSessions::Get().Join(PC, Rows[Pick].SearchIndex, FString(), FKGOnSessionResult::CreateSP(this, &SKGMainMenu::HandleJoinResult));
		return;
	}
	bQuickMatching = false;
	ConnectingTime = -1.0f;
	CloseModal();
	if (PC)
	{
		ShowToast(LOCTEXT("QuickHosting", "No open games found, so you are hosting one. Friends will see it in their list."));
		FKGSessions::Get().Host(PC, MakeHostOptions(false));
	}
}

FKGHostOptions SKGMainMenu::MakeHostOptions(bool bFromHostPanel) const
{
	FKGHostOptions Options;
	const FString PlayerName = FKGSessions::Get().GetLocalPlayerName(OwningPlayer.Get());
	const FString DefaultName = FText::Format(LOCTEXT("DefaultLobbyName", "{0}'s village"), FText::FromString(PlayerName)).ToString();
	const TArray<FKGMapEntry>& Maps = KGMenu::GetMaps();
	const int32 MapIndex = Maps.IsValidIndex(SelectedMap) && Maps[SelectedMap].bAvailable ? SelectedMap : 0;
	Options.MapPath = Maps[MapIndex].MapPath;
	Options.MapTitle = Maps[MapIndex].DisplayName.ToString();
	Options.MaxPlayers = HostMaxPlayers;
	const TArray<FString>& Regions = FKGSessions::GetRegions();
	Options.Region = Regions.IsValidIndex(HostRegion) ? Regions[HostRegion] : FString();
	FString Name;
	if (bFromHostPanel)
	{
		Name = HostNameBox.IsValid() ? HostNameBox->GetText().ToString().TrimStartAndEnd() : FString();
		Options.bPrivate = bHostPrivate;
		Options.Password = HostPasswordBox.IsValid() ? HostPasswordBox->GetText().ToString() : FString();
	}
	Options.Name = Name.IsEmpty() ? DefaultName : Name.Left(40);
	return Options;
}

void SKGMainMenu::ShowConnecting(const FText& What, TFunction<void()> OnCancel)
{
	ConnectingWhat = What;
	ConnectingTime = 0.0f;
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("ConnectingTitle", "Connecting"))
		.Body_Lambda([this]()
		{
			const int32 Dots = 1 + (FMath::FloorToInt(FMath::Max(ConnectingTime, 0.0f) * 2.5f) % 3);
			return FText::Format(LOCTEXT("ConnectingBody", "{0}{1}"), ConnectingWhat, FText::FromString(FString::ChrN(Dots, TEXT('.'))));
		})
		.ConfirmText(LOCTEXT("CancelJoin", "Cancel"))
		.ConfirmKind(EKGButtonKind::Ghost)
		.OnConfirm_Lambda([this, OnCancel]()
		{
			if (OnCancel)
			{
				OnCancel();
			}
			ConnectingTime = -1.0f;
			bSearching = false;
			CloseModal();
		}));
}

// --- Host -------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGMainMenu::BuildHostPanel()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedRef<SVerticalBox> Maps = SNew(SVerticalBox);
	const TArray<FKGMapEntry>& Entries = KGMenu::GetMaps();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FKGMapEntry& Entry = Entries[Index];
		Maps->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Row)
			.Height(46.0f)
			.IsEnabled(Entry.bAvailable)
			.IsSelected_Lambda([this, Index]() { return SelectedMap == Index; })
			.OnClicked_Lambda([this, Index]() { SelectedMap = Index; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Entry.DisplayName)
					.Font(FKGMenuStyle::Font("Black", 18, 30))
					.ColorAndOpacity(Entry.bAvailable ? S.Cream : S.Muted)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.0f, 2.0f, 14.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Entry.Mode)
					.Font(S.CaptionFont)
					.ColorAndOpacity(S.Lantern)
					.TransformPolicy(ETextTransformPolicy::ToUpper)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Entry.Description)
					.Font(S.SmallFont)
					.ColorAndOpacity(S.Muted)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(&S.BadgeBrush)
					.Padding(FMargin(10.0f, 3.0f))
					.Visibility(Entry.bAvailable ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MapSoon", "SOON"))
						.Font(S.CaptionFont)
						.ColorAndOpacity(S.Gold)
					]
				]
			]
		];
	}

	const FText AddressUnknown = LOCTEXT("AddressUnknown", "Your address could not be detected");
	const TAttribute<FText> AddressLine = TAttribute<FText>::CreateLambda([this, AddressUnknown]()
	{
		if (LocalAddress.IsEmpty())
		{
			return AddressUnknown;
		}
		// Streamer mode masks the address (digits become dots, the shape stays readable).
		return FText::FromString(KGStreamer::MaskAddress(FString::Printf(TEXT("%s  :  %d"), *LocalAddress, KGMenu::DefaultPort)));
	});
	TArray<FText> RegionNames;
	for (const FString& Region : FKGSessions::GetRegions())
	{
		RegionNames.Add(FText::AsCultureInvariant(Region));
	}
	constexpr float ControlWidth = 300.0f;

	// The form scrolls when a short screen cannot fit it; title and buttons stay put.
	TSharedRef<SVerticalBox> Form = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			KGMenuUI::MakeSectionHeader(LOCTEXT("SectionMap", "Map"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Maps
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 8.0f)
		[
			KGMenuUI::MakeSectionHeader(LOCTEXT("SectionLobby", "Lobby"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SKGSettingRow)
			.Label(LOCTEXT("LobbyName", "Lobby name"))
			.ControlWidth(ControlWidth)
			[
				SAssignNew(HostNameBox, SEditableTextBox)
				.Style(&MainMenuCompactTextBox())
				.HintText(FText::FromString(MakeHostOptions(false).Name))
				.SelectAllTextWhenFocused(true)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SKGSettingRow)
			.Label(LOCTEXT("MaxPlayers", "Max players"))
			.ControlWidth(ControlWidth - 40.0f)
			.ValueText_Lambda([this]() { return FText::AsNumber(HostMaxPlayers); })
			[
				SNew(SKGSlider)
				.MinValue(static_cast<float>(KGMenu::MinPlayers))
				.MaxValue(static_cast<float>(KGMenu::MaxPlayers))
				.StepSize(1.0f)
				.Value_Lambda([this]() { return static_cast<float>(HostMaxPlayers); })
				.OnValueChanged_Lambda([this](float NewValue) { HostMaxPlayers = FMath::RoundToInt(NewValue); })
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SKGSettingRow)
			.Label(LOCTEXT("Region", "Region"))
			.Description(LOCTEXT("RegionDesc", "Shown in the browser so nearby players find you."))
			.ControlWidth(ControlWidth)
			[
				SNew(SKGOptionSelector)
				.Options(RegionNames)
				.SelectedIndex_Lambda([this]() { return HostRegion; })
				.OnSelectionChanged_Lambda([this](int32 Index) { HostRegion = Index; })
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SKGSettingRow)
			.Label(LOCTEXT("PrivateLobby", "Private"))
			.Description(LOCTEXT("PrivateLobbyDesc", "Not listed; friends join by address."))
			.ControlWidth(ControlWidth)
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SKGToggle)
					.IsChecked_Lambda([this]() { return bHostPrivate; })
					.OnToggled_Lambda([this](bool bValue) { bHostPrivate = bValue; })
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SKGSettingRow)
			.Label(LOCTEXT("LobbyPassword", "Password"))
			.ControlWidth(ControlWidth)
			[
				SAssignNew(HostPasswordBox, SEditableTextBox)
				.Style(&MainMenuCompactTextBox())
				.IsPassword(true)
				.HintText(LOCTEXT("LobbyPasswordHint", "Optional"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(&S.InsetBrush)
			.Padding(FMargin(18.0f, 12.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 16.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("YourAddress", "Your LAN address"))
						.Font(S.CaptionFont)
						.ColorAndOpacity(S.Muted)
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(AddressLine)
						.Font(FKGMenuStyle::Font("Black", 18, 40))
						.ColorAndOpacity(S.Gold)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PortHelp", "LAN players see this game in their list and can join with its code. Internet friends need UDP port 7777 forwarded until online lobbies arrive."))
					.Font(S.SmallFont)
					.ColorAndOpacity(S.Muted)
					.AutoWrapText(true)
				]
			]
		];

	return SNew(SBorder)
		.BorderImage(&S.PanelBrush)
		.Padding(FMargin(36.0f, 28.0f, 36.0f, 24.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 14.0f)
			[
				KGMenuUI::MakePanelTitle(LOCTEXT("HostTitle", "Host a game"),
				                         LOCTEXT("HostSubtitle", "Your PC runs the match. You open a lobby first and start when the village is ready."))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				.Style(&S.ScrollBox)
				.ScrollBarStyle(&S.ScrollBar)
				.ScrollBarThickness(FVector2D(6.0f, 6.0f))
				.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
				.NavigationDestination(EDescendantScrollDestination::IntoView)
				+ SScrollBox::Slot()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					Form
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SKGMenuButton)
					.Kind(EKGButtonKind::Ghost)
					.MinWidth(140.0f)
					.Text(LOCTEXT("Back", "Back"))
					.OnClicked(this, &SKGMainMenu::GoBack)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(StartHostButton, SKGMenuButton)
					.Kind(EKGButtonKind::Accent)
					.MinWidth(240.0f)
					.Height(56.0f)
					.Text(LOCTEXT("StartHosting", "Open lobby"))
					.OnClicked(this, &SKGMainMenu::StartHosting)
				]
			]
		];
}

// --- Cosmetics --------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGMainMenu::BuildCosmeticsPanel()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	if (CosmeticsPageFactory())
	{
		CosmeticsContent = CosmeticsPageFactory()(OwningPlayer, FSimpleDelegate::CreateSP(this, &SKGMainMenu::GoBack));
		return CosmeticsContent.ToSharedRef();
	}

	TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
	constexpr int32 SlotCount = 4;
	const FText SlotNames[SlotCount] = {LOCTEXT("SlotHat", "Hats"), LOCTEXT("SlotLantern", "Lanterns"), LOCTEXT("SlotCape", "Capes"),
	                                    LOCTEXT("SlotBlade", "Blades")};
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		Slots->AddSlot()
		.FillWidth(1.0f)
		.Padding(Index == 0 ? 0.0f : 12.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.HeightOverride(128.0f)
			[
				SNew(SBorder)
				.BorderImage(&S.InsetBrush)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::AsCultureInvariant(TEXT("?")))
						.Font(FKGMenuStyle::Font("Black", 34))
						.ColorAndOpacity(FKGMenuStyle::WithAlpha(S.Cream, 0.25f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(SlotNames[Index])
						.Font(S.CaptionFont)
						.ColorAndOpacity(S.Muted)
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]
				]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(&S.PanelBrush)
		.Padding(FMargin(42.0f, 36.0f, 42.0f, 32.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShopSoonCaption", "Shop coming soon"))
				.Font(S.CaptionFont)
				.ColorAndOpacity(S.Lantern)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				KGMenuUI::MakePanelTitle(LOCTEXT("WardrobeTitle", "Wardrobe"),
				                         LOCTEXT("WardrobeBody", "Hats, lanterns, capes and blade skins, earned with copper from your matches. The shop opens in a future update."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 28.0f, 0.0f, 0.0f)
			[
				Slots
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 28.0f, 0.0f, 0.0f)
			[
				SAssignNew(CosmeticsBackButton, SKGMenuButton)
				.Kind(EKGButtonKind::Ghost)
				.MinWidth(160.0f)
				.Text(LOCTEXT("Back", "Back"))
				.OnClicked(this, &SKGMainMenu::GoBack)
			]
		];
}

// --- Navigation -------------------------------------------------------------------------------------------------------

int32 SKGMainMenu::GetPageIndex() const
{
	switch (Page)
	{
	case EKGMainMenuPage::Play:
	case EKGMainMenuPage::Join:
		return 1;
	case EKGMainMenuPage::Host:
		return 2;
	case EKGMainMenuPage::Cosmetics:
		return 3;
	default:
		return 0;
	}
}

void SKGMainMenu::OpenCosmeticsScreen()
{
	OpenPage(EKGMainMenuPage::Cosmetics);
}

void SKGMainMenu::OpenPage(EKGMainMenuPage NewPage)
{
	if (NewPage == EKGMainMenuPage::Join)
	{
		// Joining by address lives in the browser footer.
		NewPage = EKGMainMenuPage::Play;
		bFocusAddressPending = true;
	}
	if (NewPage == Page)
	{
		FocusPage(Page);
		return;
	}
	const EKGMainMenuPage Previous = Page;
	Page = NewPage;
	PanelAnim = 0.0f;
	if (Page == EKGMainMenuPage::Home)
	{
		for (int32 Index = 0; Index < HomeButtons.Num(); ++Index)
		{
			HomeButtons[Index]->PlayIntro(0.05f * Index);
		}
	}
	if (Page == EKGMainMenuPage::Settings && !Settings.IsValid())
	{
		SettingsHost->ClearChildren();
		SettingsHost->AddSlot()
		[
			SAssignNew(Settings, SKGSettingsMenu)
			.OnClosed_Lambda([this]() { bSettingsClosePending = true; })
		];
	}
	// Entering the browser from the title screen (or for the first time) looks for games right away.
	if (Page == EKGMainMenuPage::Play && (Previous == EKGMainMenuPage::Home || !bSearchedOnce))
	{
		RefreshBrowser();
	}
	FocusPage(Previous);
}

void SKGMainMenu::FocusPage(EKGMainMenuPage Previous)
{
	TSharedPtr<SWidget> Target;
	switch (Page)
	{
	case EKGMainMenuPage::Home:
	{
		const int32 Index = Previous == EKGMainMenuPage::Cosmetics ? 1 : (Previous == EKGMainMenuPage::Settings ? 2 : 0);
		Target = HomeButtons.IsValidIndex(Index) ? HomeButtons[Index] : nullptr;
		break;
	}
	case EKGMainMenuPage::Play:
	case EKGMainMenuPage::Join:
		if (bFocusAddressPending)
		{
			bFocusAddressPending = false;
			Target = AddressBox;
		}
		else
		{
			Target = RefreshButton;
		}
		break;
	case EKGMainMenuPage::Host:
		Target = StartHostButton;
		break;
	case EKGMainMenuPage::Cosmetics:
		Target = CosmeticsBackButton.IsValid() ? StaticCastSharedPtr<SWidget>(CosmeticsBackButton) : CosmeticsContent;
		break;
	case EKGMainMenuPage::Settings:
		Target = Settings.IsValid() ? Settings->GetInitialFocus() : nullptr;
		break;
	}
	KGMenu::FocusWidget(Target.IsValid() ? Target : TSharedPtr<SWidget>(SharedThis(this)));
}

TSharedPtr<SWidget> SKGMainMenu::GetInitialFocus() const
{
	return HomeButtons.Num() > 0 ? HomeButtons[0] : nullptr;
}

void SKGMainMenu::GoBack()
{
	switch (Page)
	{
	case EKGMainMenuPage::Host:
		OpenPage(EKGMainMenuPage::Play);
		break;
	case EKGMainMenuPage::Play:
	case EKGMainMenuPage::Join:
	case EKGMainMenuPage::Cosmetics:
		FKGSessions::Get().CancelFind(OwningPlayer.Get());
		bSearching = false;
		OpenPage(EKGMainMenuPage::Home);
		break;
	case EKGMainMenuPage::Settings:
		if (Settings.IsValid())
		{
			Settings->RequestClose();
		}
		break;
	case EKGMainMenuPage::Home:
		if (bOverlay)
		{
			OnCloseRequested.ExecuteIfBound();
		}
		else
		{
			RequestQuit();
		}
		break;
	}
}

void SKGMainMenu::RequestQuit()
{
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("QuitTitle", "Quit KillGo?"))
		.Body(LOCTEXT("QuitBody", "The village will have to wait for you."))
		.ConfirmText(LOCTEXT("QuitConfirm", "Quit"))
		.ConfirmKind(EKGButtonKind::Danger)
		.CancelText(LOCTEXT("QuitCancel", "Stay"))
		.OnConfirm_Lambda([this]() { KGMenu::QuitGame(OwningPlayer.Get()); })
		.OnCancel(this, &SKGMainMenu::CloseModal));
}

void SKGMainMenu::StartHosting()
{
	const TArray<FKGMapEntry>& Entries = KGMenu::GetMaps();
	if (!Entries.IsValidIndex(SelectedMap) || !Entries[SelectedMap].bAvailable)
	{
		ShowToast(LOCTEXT("PickMap", "Pick a map first."), true);
		return;
	}
	const FKGHostOptions Options = MakeHostOptions(true);
	ShowToast(FText::Format(LOCTEXT("Hosting", "Opening \"{0}\" on {1} for {2} players..."), FText::FromString(Options.Name),
	                        Entries[SelectedMap].DisplayName, FText::AsNumber(HostMaxPlayers)));
	FKGSessions::Get().Host(OwningPlayer.Get(), Options);
}

void SKGMainMenu::ShowModal(const TSharedRef<SKGModal>& Modal)
{
	if (!IsModalOpen())
	{
		FocusBeforeModal = FSlateApplication::Get().GetUserFocusedWidget(0);
	}
	ModalHost->ClearChildren();
	ModalHost->AddSlot()[Modal];
	KGMenu::FocusWidget(Modal->GetDefaultFocus());
}

void SKGMainMenu::CloseModal()
{
	ModalHost->ClearChildren();
	if (const TSharedPtr<SWidget> Previous = FocusBeforeModal.Pin())
	{
		KGMenu::FocusWidget(Previous);
	}
	else
	{
		FocusPage(Page);
	}
	FocusBeforeModal.Reset();
}

bool SKGMainMenu::IsModalOpen() const
{
	return ModalHost.IsValid() && ModalHost->GetNumWidgets() > 0;
}

void SKGMainMenu::ShowToast(const FText& Message, bool bError)
{
	ToastText = Message;
	bToastError = bError;
	ToastTime = bError ? 6.0f : 4.0f;
}

void SKGMainMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	Intro += InDeltaTime;
	PanelAnim = FMath::Min(1.0f, PanelAnim + InDeltaTime / 0.32f);
	SettingsAnim = FKGMenuStyle::Approach(SettingsAnim, Page == EKGMainMenuPage::Settings ? 1.0f : 0.0f, InDeltaTime, 12.0f);
	ToastTime = FMath::Max(0.0f, ToastTime - InDeltaTime);
	if (ConnectingTime >= 0.0f)
	{
		ConnectingTime += InDeltaTime;
	}
	if (bSearching)
	{
		SearchTime += InDeltaTime;
	}
	// The engine gives the game viewport focus after a map load; keep keyboard/Esc working inside the menu.
	if (Intro > 0.5f && MainMenuFocusDrifted())
	{
		if (IsModalOpen())
		{
			KGMenu::FocusWidget(StaticCastSharedRef<SKGModal>(ModalHost->GetChildren()->GetChildAt(0))->GetDefaultFocus());
		}
		else
		{
			FocusPage(Page);
		}
	}
	if (bSettingsClosePending)
	{
		// Deferred so the settings widget is never destroyed inside its own click handler.
		bSettingsClosePending = false;
		SettingsHost->ClearChildren();
		Settings.Reset();
		OpenPage(EKGMainMenuPage::Home);
	}
}

FReply SKGMainMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		if (IsModalOpen())
		{
			// Focus drifted outside the dialog: Esc still means the dialog's cancel, never "fall through".
			ModalHost->GetChildren()->GetChildAt(0)->OnKeyDown(MyGeometry, InKeyEvent);
			return FReply::Handled();
		}
		GoBack();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::F5 && (Page == EKGMainMenuPage::Play || Page == EKGMainMenuPage::Join))
	{
		RefreshBrowser();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGMainMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Clicking the backdrop keeps keyboard focus inside the menu so Esc and the arrows keep working.
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

#undef LOCTEXT_NAMESPACE
