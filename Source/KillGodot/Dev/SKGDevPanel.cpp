#include "Dev/SKGDevPanel.h"

#if !UE_BUILD_SHIPPING

#include "AI/KGBotController.h"
#include "Character/KGCharacter.h"
#include "Character/KGViewmodelComponent.h"
#include "Chores/KGChoreComponent.h"
#include "Chores/KGChoreTypes.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Emote/KGEmoteCatalog.h"
#include "Dev/KGDevComponent.h"
#include "Dev/KGDevSubsystem.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGSlateWidgets.h"
#include "Roles/KGRoleListGenerator.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Menu/SKGMenuWidgets.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/KGTaskStation.h"

#define LOCTEXT_NAMESPACE "KGDevPanel"

namespace KGDevPanelPrivate
{
	int32 GLastTab = 0;   // the panel reopens on the tab you left

	enum class EKind : uint8
	{
		Normal,
		Accent,
		Danger,
		Chip,
		Tab
	};

	const FKGMenuStyle& S()
	{
		return FKGMenuStyle::Get();
	}

	FSlateFontInfo Font(const TCHAR* Face, float Size, int32 Spacing = 0)
	{
		return FKGMenuStyle::Font(Face, Size, Spacing);
	}

	FText Txt(const FString& String)
	{
		return FText::FromString(String);
	}

	FLinearColor AlignmentColor(EKGAlignment Alignment)
	{
		switch (Alignment)
		{
		case EKGAlignment::Impatient:
			return S().Crimson;
		case EKGAlignment::Neutral:
			return S().Gold;
		default:
			return S().Good;
		}
	}

	FLinearColor PhaseColor(EKGPhase Phase)
	{
		switch (Phase)
		{
		case EKGPhase::Day:
			return S().Gold;
		case EKGPhase::Dawn:
		case EKGPhase::RoleReveal:
			return S().Lantern;
		case EKGPhase::Meeting:
		case EKGPhase::Trial:
			return S().Crimson;
		case EKGPhase::Night:
			return S().Ghost;
		case EKGPhase::Epilogue:
			return S().Good;
		default:
			return S().CreamDim;
		}
	}

	FString PhaseLabel(EKGPhase Phase)
	{
		return StaticEnum<EKGPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	/** Compact painted button (HUD palette): fills on hover, gold when selected, greys out when disabled. */
	class SKGDevButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SKGDevButton)
			: _Kind(EKind::Normal), _FontSize(10), _MinWidth(0.0f), _Height(26.0f), _IsSelected(false),
			  _Tint(FLinearColor::Transparent) {}
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ARGUMENT(EKind, Kind)
			SLATE_ARGUMENT(int32, FontSize)
			SLATE_ARGUMENT(float, MinWidth)
			SLATE_ARGUMENT(float, Height)
			SLATE_ATTRIBUTE(bool, IsSelected)
			SLATE_ARGUMENT(FLinearColor, Tint)
			SLATE_EVENT(FSimpleDelegate, OnClicked)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Kind = InArgs._Kind;
			IsSelected = InArgs._IsSelected;
			Tint = InArgs._Tint;
			OnClicked = InArgs._OnClicked;
			ChildSlot
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinWidth)
				.HeightOverride(InArgs._Height)
				[
					SNew(SBorder)
					.BorderImage(KGSlate::Rounded(FLinearColor::White, Kind == EKind::Tab ? 5.0f : 6.0f))
					.BorderBackgroundColor(this, &SKGDevButton::GetFill)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(FMargin(Kind == EKind::Tab ? 4.0f : 9.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(InArgs._Text)
						.Font(Font(TEXT("Bold"), InArgs._FontSize, Kind == EKind::Tab ? 90 : 20))
						.ColorAndOpacity(this, &SKGDevButton::GetTextColor)
					]
				]
			];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsEnabled())
			{
				Flash = 1.0f;
				OnClicked.ExecuteIfBound();
			}
			return FReply::Handled();
		}

		virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
			bHovered = true;
		}

		virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
		{
			SCompoundWidget::OnMouseLeave(MouseEvent);
			bHovered = false;
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
		{
			return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			Flash = FKGMenuStyle::Approach(Flash, 0.0f, InDeltaTime, 10.0f);
		}

	private:
		FSlateColor GetFill() const
		{
			const bool bSelected = IsSelected.Get(false);
			const bool bEnabled = IsEnabled();
			FLinearColor Fill;
			switch (Kind)
			{
			case EKind::Accent:
				Fill = S().Lantern;
				break;
			case EKind::Danger:
				Fill = FMath::Lerp(S().Panel, S().Crimson, 0.75f);
				break;
			case EKind::Tab:
				Fill = bSelected ? S().PanelHi : FKGMenuStyle::WithAlpha(S().Panel, bHovered ? 1.0f : 0.0f);
				break;
			default:
				Fill = Tint.A > 0.0f ? FMath::Lerp(S().Panel, Tint, 0.22f) : S().Panel;
				break;
			}
			if (bSelected && Kind != EKind::Tab)
			{
				Fill = Tint.A > 0.0f ? Tint : S().Gold;
			}
			if (bHovered && bEnabled && Kind != EKind::Tab)
			{
				Fill = FMath::Lerp(Fill, FLinearColor::White, 0.12f);
			}
			Fill = FMath::Lerp(Fill, FLinearColor::White, 0.35f * Flash);
			if (!bEnabled)
			{
				Fill = FKGMenuStyle::WithAlpha(FMath::Lerp(Fill.Desaturate(0.7f), S().Ink, 0.45f), Fill.A);
			}
			return Fill;
		}

		FSlateColor GetTextColor() const
		{
			const bool bSelected = IsSelected.Get(false);
			if (!IsEnabled())
			{
				return S().Muted;
			}
			if (Kind == EKind::Tab)
			{
				return bSelected ? S().Gold : S().CreamDim;
			}
			if (bSelected || Kind == EKind::Accent)
			{
				return S().Ink;
			}
			return Tint.A > 0.0f ? FMath::Lerp(S().Cream, Tint, 0.45f) : S().Cream;
		}

		EKind Kind = EKind::Normal;
		TAttribute<bool> IsSelected;
		FLinearColor Tint = FLinearColor::Transparent;
		FSimpleDelegate OnClicked;
		bool bHovered = false;
		float Flash = 0.0f;
	};

	TSharedRef<SWrapBox> Wrap()
	{
		return SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(5.0, 5.0));
	}

	TSharedRef<SWidget> Caption(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(Font(TEXT("Bold"), 9, 160)).ColorAndOpacity(S().Muted);
	}

	TSharedRef<SWidget> Section(const FText& Title, const TSharedRef<SWidget>& Body)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 7.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Title).Font(Font(TEXT("Bold"), 10, 200)).ColorAndOpacity(S().Lantern)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).HeightOverride(1.0f)
					[
						SNew(SBorder).BorderImage(KGSlate::Solid(FKGMenuStyle::WithAlpha(S().Muted, 0.35f)))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Body
			];
	}

	/** Label on the left, Content filling the rest. */
	TSharedRef<SWidget> Row(const FText& Label, const TSharedRef<SWidget>& Content, float LabelWidth = 92.0f)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(LabelWidth)
				[
					SNew(STextBlock).Text(Label).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(S().CreamDim)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Content
			];
	}
}

// ---- data access ---------------------------------------------------------------------------------------------------

UWorld* SKGDevPanel::LocalWorld() const
{
	return PC.IsValid() ? PC->GetWorld() : nullptr;
}

UWorld* SKGDevPanel::ReadWorld() const
{
	return FKGDev::AuthorityWorld(LocalWorld());
}

AKGGameState* SKGDevPanel::ReadGameState() const
{
	const UWorld* World = ReadWorld();
	return World ? World->GetGameState<AKGGameState>() : nullptr;
}

AKGPlayerState* SKGDevPanel::ReadMe() const
{
	AKGPlayerState* Mine = PC.IsValid() ? PC->GetPlayerState<AKGPlayerState>() : nullptr;
	const AKGGameState* GS = ReadGameState();
	if (!Mine || !GS || ReadWorld() == LocalWorld())
	{
		return Mine;
	}
	for (APlayerState* Raw : GS->PlayerArray)
	{
		if (Raw && Raw->GetPlayerId() == Mine->GetPlayerId())
		{
			return Cast<AKGPlayerState>(Raw);
		}
	}
	return Mine;
}

UKGDevComponent* SKGDevPanel::DevLink() const
{
	return UKGDevComponent::FindFor(PC.Get());
}

bool SKGDevPanel::CanRunServer() const
{
	const UWorld* World = LocalWorld();
	if (!World)
	{
		return false;
	}
	if (World->GetNetMode() != NM_Client)
	{
		return true;
	}
	const UKGDevComponent* Dev = DevLink();
	return Dev && Dev->bMayRun;
}

void SKGDevPanel::Run(const FString& Line) const
{
	FKGDev::Submit(LocalWorld(), Line);
}

void SKGDevPanel::RunQuiet(const FString& Line) const
{
	FKGDev::Execute({LocalWorld(), PC.Get()}, Line);
}

// ---- widget ------------------------------------------------------------------------------------------------------

void SKGDevPanel::Construct(const FArguments& InArgs)
{
	using namespace KGDevPanelPrivate;
	PC = InArgs._PlayerController;
	ActiveTab = GLastTab;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(0.0f, 16.0f, 16.0f, 16.0f)
		[
			SNew(SBox)
			.WidthOverride(430.0f)
			[
				SNew(SBorder)
				.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Night, 0.95f), 14.0f,
				                              FKGMenuStyle::WithAlpha(S().Gold, 0.35f), 1.5f))
				.Padding(0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[BuildHeader()]
					+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f)[BuildStatus()]
					+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 10.0f, 12.0f, 0.0f)[BuildTabs()]
					+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 8.0f, 12.0f, 0.0f)
					[
						SNew(SBorder)
						.Visibility_Lambda([this]() { return CanRunServer() ? EVisibility::Collapsed : EVisibility::Visible; })
						.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Crimson, 0.25f), 8.0f))
						.Padding(FMargin(10.0f, 6.0f))
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(Font(TEXT("Regular"), 10))
							.ColorAndOpacity(S().Cream)
							.Text(LOCTEXT("ReadOnly", "Read-only: match, bot and cheat actions run on the host only (host: kg.Dev.AllowClients 1). View toggles still work."))
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12.0f, 4.0f, 6.0f, 0.0f)
					[
						SNew(SScrollBox)
						.Style(&S().ScrollBox)
						.ScrollBarStyle(&S().ScrollBar)
						.ScrollBarThickness(FVector2D(6.0, 6.0))
						+ SScrollBox::Slot().Padding(0.0f, 0.0f, 8.0f, 12.0f)
						[
							SNew(SWidgetSwitcher)
							.WidgetIndex_Lambda([this]() { return ActiveTab; })
							+ SWidgetSwitcher::Slot()[BuildMatchTab()]
							+ SWidgetSwitcher::Slot()[BuildBotsTab()]
							+ SWidgetSwitcher::Slot()[BuildMeTab()]
							+ SWidgetSwitcher::Slot()[BuildWorldTab()]
							+ SWidgetSwitcher::Slot()[BuildChoresTab()]
							+ SWidgetSwitcher::Slot()[BuildDebugTab()]
						]
					]
					+ SVerticalBox::Slot().AutoHeight()[BuildFooter()]
				]
			]
		]
	];
	RebuildPlayers();
	RebuildChores();
	RebuildRoles();
}

FReply SKGDevPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::F1 || Key == EKeys::Escape)
	{
		if (UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld()))
		{
			Dev->TogglePanel(PC.Get());
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();   // WASD etc. fall through to the game
}

FReply SKGDevPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

void SKGDevPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (InDeltaTime > 0.0f)
	{
		SmoothedFps = FMath::Lerp(SmoothedFps, 1.0f / InDeltaTime, FMath::Min(1.0f, InDeltaTime * 3.0f));
	}
	SignatureTimer -= InDeltaTime;
	if (SignatureTimer <= 0.0f)
	{
		SignatureTimer = 0.25f;
		if (PlayersSignature() != PlayersSig)
		{
			RebuildPlayers();
		}
		if (ChoresSignature() != ChoresSig)
		{
			RebuildChores();
		}
	}
}

// ---- building blocks -----------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGDevPanel::ActionButton(const FText& Label, const FString& Line, uint8 Kind, TAttribute<bool> Selected,
                                              const FLinearColor& Tint, const FText& ToolTip)
{
	using namespace KGDevPanelPrivate;
	const bool bServer = FKGDev::IsServerVerb(Line);
	return SNew(SKGDevButton)
		.Text(Label)
		.Kind(static_cast<EKind>(Kind))
		.IsSelected(Selected)
		.Tint(Tint)
		.ToolTipText(ToolTip.IsEmpty() ? FText::FromString(TEXT("kg.") + Line) : ToolTip)
		.IsEnabled_Lambda([this, bServer]() { return !bServer || CanRunServer(); })
		.OnClicked(FSimpleDelegate::CreateLambda([this, Line]() { Run(Line); }));
}

TSharedRef<SWidget> SKGDevPanel::SliderRow(const FText& Label, float Min, float Max, float Step, TAttribute<float> Value,
                                           TFunction<void(float)> OnChanged, int32 Decimals)
{
	using namespace KGDevPanelPrivate;
	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = Decimals;
	Format.MaximumFractionalDigits = Decimals;
	Format.UseGrouping = false;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(92.0f)
			[
				SNew(STextBlock).Text(Label).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(S().CreamDim)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SBox).HeightOverride(24.0f)
			[
				SNew(SKGSlider)
				.MinValue(Min)
				.MaxValue(Max)
				.StepSize(Step)
				.Value(Value)
				.OnValueChanged(FKGOnFloatChanged::CreateLambda([OnChanged](float V) { OnChanged(V); }))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(46.0f)
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Bold"), 10))
				.ColorAndOpacity(S().Cream)
				.Justification(ETextJustify::Right)
				.Text_Lambda([Value, Format]() { return FText::AsNumber(Value.Get(0.0f), &Format); })
			]
		];
}

TSharedRef<SWidget> SKGDevPanel::BuildHeader()
{
	using namespace KGDevPanelPrivate;
	return SNew(SBorder)
		.BorderImage(KGSlate::Solid(FLinearColor::Transparent))
		.Padding(FMargin(16.0f, 12.0f, 12.0f, 10.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "DEV PANEL")).Font(Font(TEXT("Black"), 17, 120)).ColorAndOpacity(S().Gold)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(KGSlate::Rounded(FLinearColor::White, 5.0f))
				.BorderBackgroundColor_Lambda([this]()
				{
					const UWorld* World = LocalWorld();
					if (World && World->GetNetMode() == NM_Client)
					{
						return FSlateColor(CanRunServer() ? S().Ghost : S().Crimson);
					}
					return FSlateColor(S().Lantern);
				})
				.Padding(FMargin(7.0f, 2.0f))
				[
					SNew(STextBlock)
					.Font(Font(TEXT("Bold"), 9, 120))
					.ColorAndOpacity(S().Ink)
					.Text_Lambda([this]()
					{
						const UWorld* World = LocalWorld();
						if (!World || World->GetNetMode() != NM_Client)
						{
							return LOCTEXT("Host", "HOST");
						}
						return CanRunServer() ? LOCTEXT("ClientAllowed", "CLIENT") : LOCTEXT("ClientRO", "READ-ONLY");
					})
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SKGDevButton)
				.Text(LOCTEXT("Close", "F1  CLOSE"))
				.Kind(EKind::Tab)
				.OnClicked(FSimpleDelegate::CreateLambda([this]()
				{
					if (UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld()))
					{
						Dev->ClosePanel();
					}
				}))
			]
		];
}

TSharedRef<SWidget> SKGDevPanel::BuildStatus()
{
	using namespace KGDevPanelPrivate;
	return SNew(SBorder)
		.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Ink, 0.85f), 10.0f))
		.Padding(FMargin(12.0f, 8.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(Font(TEXT("Black"), 15, 60))
					.ColorAndOpacity_Lambda([this]()
					{
						const AKGGameState* GS = ReadGameState();
						return FSlateColor(GS ? PhaseColor(GS->GetPhase()) : S().Muted);
					})
					.Text_Lambda([this]()
					{
						const AKGGameState* GS = ReadGameState();
						if (!GS)
						{
							return LOCTEXT("NoMatch", "NO MATCH");
						}
						return Txt(FString::Printf(TEXT("%s  -  DAY %d"), *PhaseLabel(GS->GetPhase()).ToUpper(), GS->GetDayIndex()));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(Font(TEXT("Black"), 15))
					.ColorAndOpacity_Lambda([this]()
					{
						const AKGGameState* GS = ReadGameState();
						return FSlateColor(GS && GS->Clock.bPaused ? S().Ghost : S().Cream);
					})
					.Text_Lambda([this]()
					{
						const AKGGameState* GS = ReadGameState();
						if (!GS)
						{
							return FText::GetEmpty();
						}
						const int32 Secs = FMath::CeilToInt(GS->Clock.RemainingSeconds);
						return Txt(FString::Printf(TEXT("%s%d:%02d"), GS->Clock.bPaused ? TEXT("FROZEN  ") : TEXT(""), Secs / 60, Secs % 60));
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Regular"), 10))
				.ColorAndOpacity(S().CreamDim)
				.Text_Lambda([this]()
				{
					const AKGGameState* GS = ReadGameState();
					if (!GS)
					{
						return FText::GetEmpty();
					}
					int32 Bots = 0;
					int32 Alive = 0;
					for (const APlayerState* Raw : GS->PlayerArray)
					{
						const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
						Bots += PS && PS->IsABot() ? 1 : 0;
						Alive += PS && PS->IsAlive() ? 1 : 0;
					}
					const AKGGameMode* GM = ReadWorld() ? ReadWorld()->GetAuthGameMode<AKGGameMode>() : nullptr;
					return Txt(FString::Printf(TEXT("%d players  -  %d bots  -  %d alive  -  prep %.0f%%%s"), GS->PlayerArray.Num(),
					                           Bots, Alive, GS->Preparation * 100.0f,
					                           GM && GM->DevClockScale != 1.0f ? *FString::Printf(TEXT("  -  clock x%g"), GM->DevClockScale) : TEXT("")));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Regular"), 10))
				.ColorAndOpacity(S().Muted)
				.Text_Lambda([this]()
				{
					const UWorld* World = LocalWorld();
					const AKGGameMode* GM = ReadWorld() ? ReadWorld()->GetAuthGameMode<AKGGameMode>() : nullptr;
					const TCHAR* Mode = !World ? TEXT("-") : World->GetNetMode() == NM_Client ? TEXT("client")
					                    : World->GetNetMode() == NM_ListenServer ? TEXT("listen server") : TEXT("standalone");
					const APlayerState* PS = PC.IsValid() ? PC->PlayerState.Get() : nullptr;
					return Txt(FString::Printf(TEXT("%s  -  %.0f fps  -  ping %.0f ms  -  seed %s"), Mode, SmoothedFps,
					                           PS ? PS->GetPingInMilliseconds() : 0.0f,
					                           GM && GM->GetMatchSeed() != 0 ? *FString::Printf(TEXT("%016llX"), static_cast<uint64>(GM->GetMatchSeed())) : TEXT("-")));
				})
			]
		];
}

TSharedRef<SWidget> SKGDevPanel::BuildTabs()
{
	using namespace KGDevPanelPrivate;
	static const FText Names[] = {LOCTEXT("TabMatch", "MATCH"), LOCTEXT("TabBots", "BOTS"), LOCTEXT("TabMe", "ME"),
	                              LOCTEXT("TabWorld", "WORLD"), LOCTEXT("TabChores", "CHORES"), LOCTEXT("TabDebug", "DEBUG")};
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f, 0.0f));
	for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i)
	{
		Grid->AddSlot(i, 0)
		[
			SNew(SKGDevButton)
			.Text(Names[i])
			.Kind(EKind::Tab)
			.Height(28.0f)
			.IsSelected_Lambda([this, i]() { return ActiveTab == i; })
			.OnClicked(FSimpleDelegate::CreateLambda([this, i]() { ActiveTab = i; GLastTab = i; }))
		];
	}
	return SNew(SBorder)
		.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Ink, 0.6f), 7.0f))
		.Padding(3.0f)
		[
			Grid
		];
}

TSharedRef<SWidget> SKGDevPanel::BuildFooter()
{
	using namespace KGDevPanelPrivate;
	auto Last = [this]() -> const UKGDevSubsystem::FMessage*
	{
		const UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld());
		return Dev && Dev->GetMessages().Num() > 0 ? &Dev->GetMessages().Last() : nullptr;
	};
	return SNew(SBorder)
		.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Ink, 0.7f), 10.0f))
		.Padding(FMargin(14.0f, 8.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Bold"), 10))
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([Last]()
				{
					const UKGDevSubsystem::FMessage* M = Last();
					const float Age = M ? static_cast<float>(FPlatformTime::Seconds() - M->Time) : 99.0f;
					const FLinearColor Base = !M ? S().Muted : M->bOk ? S().Good : S().Crimson;
					return FSlateColor(FMath::Lerp(Base, S().Muted, FMath::Clamp((Age - 4.0f) / 4.0f, 0.0f, 1.0f)));
				})
				.Text_Lambda([Last]()
				{
					const UKGDevSubsystem::FMessage* M = Last();
					return M ? Txt(M->Text) : LOCTEXT("Ready", "Ready. Every button is a console command too: hover for its kg.* line.");
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Regular"), 9))
				.ColorAndOpacity(S().Muted)
				.Text(LOCTEXT("Hint", "F1 / Esc close  -  WASD still walks  -  kg.Dev.Help lists every command"))
			]
		];
}

// ---- tabs ------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> SKGDevPanel::BuildMatchTab()
{
	using namespace KGDevPanelPrivate;
	TSharedRef<SWrapBox> Flow = Wrap();
	Flow->AddSlot()[ActionButton(LOCTEXT("Start", "Start match"), TEXT("Match.Start"), uint8(EKind::Accent))];
	Flow->AddSlot()[ActionButton(LOCTEXT("Restart", "Restart round"), TEXT("Match.Restart"))];
	Flow->AddSlot()[ActionButton(LOCTEXT("Reveal", "Reveal all roles"), TEXT("Match.Reveal"))];

	TSharedRef<SWrapBox> Phases = Wrap();
	for (const EKGPhase Phase : {EKGPhase::Warmup, EKGPhase::RoleReveal, EKGPhase::Dawn, EKGPhase::Day, EKGPhase::Meeting,
	                             EKGPhase::Trial, EKGPhase::Night, EKGPhase::Epilogue})
	{
		Phases->AddSlot()
		[
			ActionButton(Txt(PhaseLabel(Phase)), TEXT("Match.Phase ") + PhaseLabel(Phase), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([this, Phase]()
			             {
				             const AKGGameState* GS = ReadGameState();
				             return GS && GS->GetPhase() == Phase;
			             }),
			             PhaseColor(Phase))
		];
	}

	TSharedRef<SWrapBox> Timer = Wrap();
	Timer->AddSlot()
	[
		ActionButton(LOCTEXT("Freeze", "Freeze timer"), TEXT("Match.Freeze"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]() { const AKGGameState* GS = ReadGameState(); return GS && GS->Clock.bPaused; }))
	];
	Timer->AddSlot()[ActionButton(LOCTEXT("Skip", "Skip phase"), TEXT("Match.Skip"))];
	Timer->AddSlot()[ActionButton(LOCTEXT("T10", "10 s left"), TEXT("Match.Time 10"))];
	Timer->AddSlot()[ActionButton(LOCTEXT("T120", "2 min left"), TEXT("Match.Time 120"))];

	TSharedRef<SWrapBox> Speed = Wrap();
	for (const float Scale : {1.0f, 2.0f, 5.0f, 10.0f, 25.0f})
	{
		Speed->AddSlot()
		[
			ActionButton(Txt(FString::Printf(TEXT("x%g"), Scale)), FString::Printf(TEXT("Match.Speed %g"), Scale), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([this, Scale]()
			             {
				             const AKGGameMode* GM = ReadWorld() ? ReadWorld()->GetAuthGameMode<AKGGameMode>() : nullptr;
				             return GM && FMath::IsNearlyEqual(GM->DevClockScale, Scale);
			             }))
		];
	}

	TSharedRef<SWrapBox> Win = Wrap();
	Win->AddSlot()[ActionButton(LOCTEXT("WinTown", "Town wins"), TEXT("Match.Win Town"), uint8(EKind::Chip), false, S().Good)];
	Win->AddSlot()[ActionButton(LOCTEXT("WinImp", "Impatient win"), TEXT("Match.Win Impatient"), uint8(EKind::Chip), false, S().Crimson)];
	Win->AddSlot()[ActionButton(LOCTEXT("WinNeutral", "Neutral wins"), TEXT("Match.Win Neutral"), uint8(EKind::Chip), false, S().Gold)];

	TSharedRef<SWidget> Seed =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(Font(TEXT("Mono"), 11))
			.ColorAndOpacity(S().Cream)
			.Text_Lambda([this]()
			{
				const AKGGameMode* GM = ReadWorld() ? ReadWorld()->GetAuthGameMode<AKGGameMode>() : nullptr;
				return GM ? Txt(FString::Printf(TEXT("%lld"), GM->GetMatchSeed())) : LOCTEXT("SeedHidden", "(host only)");
			})
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SKGDevButton)
			.Text(LOCTEXT("CopySeed", "Copy"))
			.OnClicked(FSimpleDelegate::CreateLambda([this]()
			{
				if (const AKGGameMode* GM = ReadWorld() ? ReadWorld()->GetAuthGameMode<AKGGameMode>() : nullptr)
				{
					FPlatformApplicationMisc::ClipboardCopy(*FString::Printf(TEXT("%lld"), GM->GetMatchSeed()));
					FKGDev::Report(LocalWorld(), FKGDevResult::Ok(TEXT("Seed copied (replay: kg.Match.Start <seed>)")));
				}
			}))
		];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecFlow", "FLOW"), Flow)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecPhase", "JUMP TO PHASE"), Phases)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecTimer", "TIMER"), Timer)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Row(LOCTEXT("ClockSpeed", "Clock speed"), Speed)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecWin", "FORCE WIN"), Win)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecSeed", "MATCH SEED"), Seed)];
}

TSharedRef<SWidget> SKGDevPanel::BuildBotsTab()
{
	using namespace KGDevPanelPrivate;
	TSharedRef<SWrapBox> AddRemove = Wrap();
	AddRemove->AddSlot()[ActionButton(LOCTEXT("Add1", "+1 bot"), TEXT("Bot.Add 1"), uint8(EKind::Accent))];
	AddRemove->AddSlot()[ActionButton(LOCTEXT("Add5", "+5 bots"), TEXT("Bot.Add 5"), uint8(EKind::Accent))];
	AddRemove->AddSlot()[ActionButton(LOCTEXT("Rem1", "-1 bot"), TEXT("Bot.Remove 1"))];
	AddRemove->AddSlot()[ActionButton(LOCTEXT("RemAll", "Remove all"), TEXT("Bot.RemoveAll"), uint8(EKind::Danger))];
	AddRemove->AddSlot()
	[
		ActionButton(LOCTEXT("BotAI", "Bot AI"), TEXT("Bot.AI"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([]() { return AKGBotController::IsBrainEnabled(); }))
	];

	TSharedRef<SWidget> Fill =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SKGDevButton).Text(LOCTEXT("Minus", "-")).MinWidth(30.0f)
			.OnClicked(FSimpleDelegate::CreateLambda([this]() { FillTarget = FMath::Max(1, FillTarget - 1); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(28.0f)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(Font(TEXT("Black"), 13))
				.ColorAndOpacity(S().Cream)
				.Text_Lambda([this]() { return FText::AsNumber(FillTarget); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SKGDevButton).Text(LOCTEXT("Plus", "+")).MinWidth(30.0f)
			.OnClicked(FSimpleDelegate::CreateLambda([this]() { FillTarget = FMath::Min(32, FillTarget + 1); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SKGDevButton)
			.Text(LOCTEXT("FillTo", "Fill to N"))
			.Kind(EKind::Accent)
			.ToolTipText(LOCTEXT("FillTip", "kg.Bot.Fill <N>"))
			.IsEnabled_Lambda([this]() { return CanRunServer(); })
			.OnClicked(FSimpleDelegate::CreateLambda([this]() { Run(FString::Printf(TEXT("Bot.Fill %d"), FillTarget)); }))
		];

	SAssignNew(PlayersBox, SVerticalBox);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecBots", "ADD / REMOVE"), AddRemove)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Row(LOCTEXT("FillRow", "Fill to"), Fill)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecPlayers", "PLAYERS"), PlayersBox.ToSharedRef())];
}

uint32 SKGDevPanel::PlayersSignature() const
{
	uint32 Hash = 0x9E3779B9u;
	if (const AKGGameState* GS = ReadGameState())
	{
		for (const APlayerState* Raw : GS->PlayerArray)
		{
			const AKGPlayerState* PS = Cast<AKGPlayerState>(Raw);
			Hash = HashCombine(Hash, GetTypeHash(Raw));
			Hash = HashCombine(Hash, PS ? GetTypeHash(PS->GetPlayerName()) : 0u);
		}
	}
	return Hash;
}

void SKGDevPanel::RebuildPlayers()
{
	using namespace KGDevPanelPrivate;
	PlayersSig = PlayersSignature();
	if (!PlayersBox.IsValid())
	{
		return;
	}
	PlayersBox->ClearChildren();
	const AKGGameState* GS = ReadGameState();
	if (!GS)
	{
		return;
	}
	const AKGPlayerState* Me = ReadMe();
	// Bots first (they are what this tab is about), humans after.
	TArray<AKGPlayerState*> Players;
	for (APlayerState* Raw : GS->PlayerArray)
	{
		if (AKGPlayerState* PS = Cast<AKGPlayerState>(Raw))
		{
			Players.Add(PS);
		}
	}
	Players.StableSort([](const AKGPlayerState& A, const AKGPlayerState& B) { return A.IsABot() && !B.IsABot(); });
	for (AKGPlayerState* Player : Players)
	{
		const TWeakObjectPtr<AKGPlayerState> Weak = Player;
		const FString Name = Player->GetPlayerName();
		const FString Quoted = FString::Printf(TEXT("\"%s\""), *Name);
		const bool bBot = Player->IsABot();
		const bool bMe = Player == Me;

		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		Buttons->AddSlot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
		[
			ActionButton(LOCTEXT("Goto", "Go"), TEXT("Bot.Goto ") + Quoted, uint8(EKind::Normal), false, FLinearColor::Transparent,
			             LOCTEXT("GotoTip", "Teleport behind them"))
		];
		Buttons->AddSlot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([Weak]() { return Weak.IsValid() && !Weak->IsAlive() ? 1 : 0; })
			+ SWidgetSwitcher::Slot()[ActionButton(LOCTEXT("Kill", "Kill"), TEXT("Me.Kill ") + Quoted)]
			+ SWidgetSwitcher::Slot()[ActionButton(LOCTEXT("Revive", "Revive"), TEXT("Me.Revive ") + Quoted, uint8(EKind::Accent))]
		];
		if (bBot)
		{
			Buttons->AddSlot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				ActionButton(LOCTEXT("RemoveOne", "X"), TEXT("Bot.Remove ") + Quoted, uint8(EKind::Danger), false,
				             FLinearColor::Transparent, LOCTEXT("RemoveTip", "Remove this bot"))
			];
		}

		PlayersBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Panel, 0.8f), 8.0f))
			.Padding(FMargin(10.0f, 5.0f, 6.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(Font(TEXT("Bold"), 11))
						.ColorAndOpacity_Lambda([Weak]() { return FSlateColor(Weak.IsValid() && Weak->IsAlive() ? S().Cream : S().Muted); })
						.Text(Txt(FString::Printf(TEXT("%s%s"), *Name, bMe ? TEXT("  (you)") : bBot ? TEXT("") : TEXT("  (human)"))))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(Font(TEXT("Regular"), 9))
						.ColorAndOpacity_Lambda([Weak]()
						{
							const FKGRoleInfo* Role = Weak.IsValid() ? FKGRoleListGenerator::FindRole(
								FKGRoleListGenerator::GetDefaultCatalog(), Weak->GetPrivateRoleId()) : nullptr;
							return FSlateColor(Role ? AlignmentColor(Role->GetAlignment()) : S().Muted);
						})
						.Text_Lambda([Weak]()
						{
							if (!Weak.IsValid())
							{
								return FText::GetEmpty();
							}
							const FName Role = Weak->GetPrivateRoleId();
							const AKGBotController* Bot = Cast<AKGBotController>(Weak->GetOwner());
							const FString Life = StaticEnum<EKGLifeState>()->GetNameStringByValue(int64(Weak->LifeState));
							return Txt(FString::Printf(TEXT("%s  -  %s%s%s"), Role.IsNone() ? TEXT("no role") : *Role.ToString(), *Life,
							                           Bot ? TEXT("  -  ") : TEXT(""), Bot ? *Bot->GetDevStatus() : TEXT("")));
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Buttons
				]
			]
		];
	}
}

TSharedRef<SWidget> SKGDevPanel::BuildMeTab()
{
	using namespace KGDevPanelPrivate;
	TSharedRef<SWrapBox> Cheats = Wrap();
	Cheats->AddSlot()
	[
		ActionButton(LOCTEXT("God", "God mode"), TEXT("Me.God"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]() { const UKGDevComponent* D = DevLink(); return D && D->bGod; }))
	];
	Cheats->AddSlot()
	[
		ActionButton(LOCTEXT("Fly", "Fly / noclip"), TEXT("Me.Fly"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]() { const UKGDevComponent* D = DevLink(); return D && D->bFly; }),
		             FLinearColor::Transparent, LOCTEXT("FlyTip", "kg.Me.Fly  (Space up, Ctrl/C down)"))
	];
	Cheats->AddSlot()
	[
		ActionButton(LOCTEXT("Blade", "Blade"), TEXT("Me.Blade"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]()
		             {
			             const AKGCharacter* C = PC.IsValid() ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr;
			             return C && C->bHoldingAssassinBlade;
		             }))
	];
	Cheats->AddSlot()[ActionButton(LOCTEXT("Heal", "Heal"), TEXT("Me.Heal"))];
	Cheats->AddSlot()[ActionButton(LOCTEXT("Stamina", "Stamina"), TEXT("Me.Stamina"))];
	Cheats->AddSlot()[ActionButton(LOCTEXT("KillMe", "Kill me"), TEXT("Me.Kill"), uint8(EKind::Danger))];
	Cheats->AddSlot()[ActionButton(LOCTEXT("ReviveMe", "Revive"), TEXT("Me.Revive"), uint8(EKind::Accent))];

	TSharedRef<SWrapBox> Speed = Wrap();
	for (const float Scale : {0.5f, 1.0f, 2.0f, 3.0f, 5.0f})
	{
		Speed->AddSlot()
		[
			ActionButton(Txt(FString::Printf(TEXT("x%g"), Scale)), FString::Printf(TEXT("Me.Speed %g"), Scale), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([this, Scale]()
			             {
				             const UKGDevComponent* D = DevLink();
				             return D && FMath::IsNearlyEqual(D->SpeedScale, Scale);
			             }))
		];
	}

	// Role picker: filter box + every role of the catalog as a chip, coloured by alignment.
	SAssignNew(RolesBox, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0, 4.0));
	TSharedRef<SWidget> Roles =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(Font(TEXT("Bold"), 11))
			.ColorAndOpacity_Lambda([this]()
			{
				const AKGPlayerState* Me = ReadMe();
				const FKGRoleInfo* Role = Me ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
				                                                             Me->GetPrivateRoleId()) : nullptr;
				return FSlateColor(Role ? AlignmentColor(Role->GetAlignment()) : S().Muted);
			})
			.Text_Lambda([this]()
			{
				const AKGPlayerState* Me = ReadMe();
				return Me && !Me->GetPrivateRoleId().IsNone()
					       ? FText::Format(LOCTEXT("MyRole", "You are: {0}"), Txt(Me->GetPrivateRoleId().ToString()))
					       : LOCTEXT("NoRole", "No role yet (roles are dealt at the reveal)");
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
		[
			SNew(SBox).HeightOverride(30.0f)
			[
				SNew(SEditableTextBox)
				.Style(&S().TextBox)
				.Font(Font(TEXT("Regular"), 11))
				.HintText(LOCTEXT("RoleFilter", "Filter roles..."))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					RoleFilter = Text.ToString();
					RebuildRoles();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			RolesBox.ToSharedRef()
		];

	// Items: every catalog item as a tile; click = 1, right click = a full stack.
	TSharedRef<SWrapBox> Items = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0, 4.0));
	const TArray<FKGItemDef>& Catalog = UKGItemCatalog::GetAll();
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		const FKGItemDef& Def = Catalog[i];
		const FName Id = Def.Id;
		const int32 Stack = FMath::Max(1, Def.StackSize);
		Items->AddSlot()
		[
			SNew(SKGItemSlot)
			.SlotIndex(i)
			.ItemId(Id)
			.Count(0)
			.Size(44.0f)
			.ToolTipText(FText::Format(LOCTEXT("ItemTip", "{0}\nclick: +1   right click: +{1}"), Def.DisplayName, FText::AsNumber(Stack)))
			.IsEnabled_Lambda([this]() { return CanRunServer(); })
			.OnClicked(FKGOnSlotClicked::CreateLambda([this, Id, Stack](int32, const FPointerEvent& Mouse)
			{
				const int32 Count = Mouse.GetEffectingButton() == EKeys::RightMouseButton ? Stack : 1;
				Run(FString::Printf(TEXT("Me.Give %s %d"), *Id.ToString(), Count));
			}))
		];
	}
	TSharedRef<SWrapBox> Gold = Wrap();
	Gold->AddSlot()[ActionButton(LOCTEXT("Coins", "+100 coins"), TEXT("Me.Coins 100"))];
	Gold->AddSlot()[ActionButton(LOCTEXT("Coins1k", "+999 coins"), TEXT("Me.Coins 999"))];
	Gold->AddSlot()[ActionButton(LOCTEXT("Gold", "+500 profile gold"), TEXT("Me.Gold 500"), uint8(EKind::Normal), false,
	                             FLinearColor::Transparent, LOCTEXT("GoldTip", "kg.Me.Gold 500: cosmetic shop gold on this machine"))];

	TSharedRef<SWrapBox> Spawn = Wrap();
	Spawn->AddSlot()[ActionButton(LOCTEXT("Chest", "Chest"), TEXT("Spawn.Chest 0 0"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("MyChest", "My chest"), TEXT("Spawn.Chest 0 1"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("LockedChest", "Locked chest"), TEXT("Spawn.Chest 1 1"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("Stool", "Stool"), TEXT("Spawn.Seat Stool"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("Chair", "Chair"), TEXT("Spawn.Seat Chair"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("Bench", "Bench"), TEXT("Spawn.Seat Bench"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("PickupCoin", "Coin pickup"), TEXT("Spawn.Pickup Coin 25"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("LootCrate", "Loot: crate"), TEXT("Spawn.Loot Crate"))];
	Spawn->AddSlot()[ActionButton(LOCTEXT("LootFish", "Loot: fishing"), TEXT("Spawn.Loot Fishing"))];

	// Fishing (kg.Fish.*): rod, forced bites, land, sell, the market stall, the tension overlay.
	TSharedRef<SWrapBox> Fishing = Wrap();
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishRod", "Give rod"), TEXT("Fish.Give Rod"), uint8(EKind::Accent))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishBite", "Force bite"), TEXT("Fish.Bite"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishBiteCarp", "Golden carp bite"), TEXT("Fish.Bite GoldenCarp"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishBiteJunk", "Bottle bite"), TEXT("Fish.Bite MessageBottle"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishLand", "Land it"), TEXT("Fish.Land"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishGiveSalmon", "+Salmon 5 kg"), TEXT("Fish.Give Salmon 5"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishSell", "Sell catch"), TEXT("Fish.Sell"))];
	Fishing->AddSlot()[ActionButton(LOCTEXT("FishMarket", "Market stall here"), TEXT("Fish.Market"))];
	Fishing->AddSlot()
	[
		ActionButton(LOCTEXT("FishTension", "Tension overlay"), TEXT("Fish.Tension"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([]()
		             {
			             const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.Fish.Debug"));
			             return CVar && CVar->GetInt() != 0;
		             }))
	];

	// Emotes: one button per catalog row (kg.Emote <id>), then stop / bots / third-person camera.
	TSharedRef<SWrapBox> Emotes = Wrap();
	for (const FKGEmoteDef& Def : FKGEmoteCatalog::GetAll())
	{
		Emotes->AddSlot()[ActionButton(Def.DisplayName, FString::Printf(TEXT("Emote %s"), *Def.Id.ToString()))];
	}
	TSharedRef<SWrapBox> EmoteTools = Wrap();
	EmoteTools->AddSlot()[ActionButton(LOCTEXT("EmoteStop", "Stop"), TEXT("Emote stop"))];
	EmoteTools->AddSlot()[ActionButton(LOCTEXT("EmoteCam", "3rd-person cam"), TEXT("Emote.Cam"))];
	EmoteTools->AddSlot()[ActionButton(LOCTEXT("EmoteBotsDance", "Bots: dance"), TEXT("Emote.Bots dance"))];
	EmoteTools->AddSlot()[ActionButton(LOCTEXT("EmoteBotsAll", "Bots: all emotes"), TEXT("Emote.Bots all"))];
	EmoteTools->AddSlot()[ActionButton(LOCTEXT("EmoteBotsStop", "Bots: stop"), TEXT("Emote.Bots stop"))];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecCheats", "CHEATS"), Cheats)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Row(LOCTEXT("SpeedRow", "Move speed"), Speed)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecRole", "MY ROLE"), Roles)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecItems", "GIVE ITEMS"), Items)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Gold]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecSpawn", "SPAWN IN FRONT OF ME"), Spawn)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecFishing", "FISHING (kg.Fish)"), Fishing)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecEmotes", "EMOTES (kg.Emote)"), Emotes)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[EmoteTools];
}

void SKGDevPanel::RebuildRoles()
{
	using namespace KGDevPanelPrivate;
	if (!RolesBox.IsValid())
	{
		return;
	}
	RolesBox->ClearChildren();
	TArray<const FKGRoleInfo*> Roles;
	for (const FKGRoleInfo& Role : FKGRoleListGenerator::GetDefaultCatalog())
	{
		if (RoleFilter.IsEmpty() || Role.RoleId.ToString().Contains(RoleFilter, ESearchCase::IgnoreCase))
		{
			Roles.Add(&Role);
		}
	}
	// Town, then Impatient, then Neutral; alphabetical inside.
	Roles.Sort([](const FKGRoleInfo& A, const FKGRoleInfo& B)
	{
		return A.GetAlignment() != B.GetAlignment() ? A.GetAlignment() < B.GetAlignment()
		                                            : A.RoleId.LexicalLess(B.RoleId);
	});
	for (const FKGRoleInfo* Role : Roles)
	{
		const FName RoleId = Role->RoleId;
		RolesBox->AddSlot()
		[
			ActionButton(Txt(RoleId.ToString()), TEXT("Me.Role ") + RoleId.ToString(), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([this, RoleId]()
			             {
				             const AKGPlayerState* Me = ReadMe();
				             return Me && Me->GetPrivateRoleId() == RoleId;
			             }),
			             AlignmentColor(Role->GetAlignment()))
		];
	}
}

TSharedRef<SWidget> SKGDevPanel::BuildWorldTab()
{
	using namespace KGDevPanelPrivate;
	// Teleport targets grouped (Area, Landmark, district/street/place... from the map info, Marker).
	TSharedRef<SVerticalBox> Places = SNew(SVerticalBox);
	const TArray<FKGDevLocation>& Locations = FKGDev::GatherLocations(LocalWorld());
	FString Group;
	TSharedPtr<SWrapBox> GroupBox;
	for (const FKGDevLocation& L : Locations)
	{
		if (!GroupBox.IsValid() || L.Group != Group)
		{
			Group = L.Group;
			Places->AddSlot().AutoHeight().Padding(0.0f, Places->NumSlots() > 0 ? 8.0f : 0.0f, 0.0f, 4.0f)[Caption(Txt(Group.ToUpper()))];
			GroupBox = Wrap();
			Places->AddSlot().AutoHeight()[GroupBox.ToSharedRef()];
		}
		GroupBox->AddSlot()[ActionButton(Txt(L.Name), FString::Printf(TEXT("World.Goto \"%s\""), *L.Name), uint8(EKind::Chip))];
	}
	if (Locations.Num() == 0)
	{
		Places->AddSlot().AutoHeight()[Caption(LOCTEXT("NoPlaces", "NO NAMED PLACES IN THIS LEVEL"))];
	}

	TSharedRef<SWrapBox> Look = Wrap();
	for (const TCHAR* Name : {TEXT("Day"), TEXT("Dusk"), TEXT("Night"), TEXT("Dawn")})
	{
		Look->AddSlot()[ActionButton(Txt(Name), FString::Printf(TEXT("World.Look %s"), Name), uint8(EKind::Chip))];
	}

	auto HiddenToggle = [this](const TCHAR* GroupName, const FText& Label)
	{
		const FName GroupId(GroupName);
		return ActionButton(Label, FString::Printf(TEXT("World.Hide %s"), GroupName), uint8(EKind::Chip),
		                    TAttribute<bool>::CreateLambda([this, GroupId]()
		                    {
			                    const UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld());
			                    return Dev && Dev->HiddenGroups.Contains(GroupId);
		                    }),
		                    FLinearColor::Transparent, LOCTEXT("HideTip", "Selected = hidden (this machine only)"));
	};
	TSharedRef<SWrapBox> Perf = Wrap();
	Perf->AddSlot()[HiddenToggle(TEXT("Grass"), LOCTEXT("HideGrass", "Hide grass"))];
	Perf->AddSlot()[HiddenToggle(TEXT("Foliage"), LOCTEXT("HideFoliage", "Hide foliage"))];
	Perf->AddSlot()[HiddenToggle(TEXT("Dress"), LOCTEXT("HideDress", "Hide dressing"))];
	Perf->AddSlot()[HiddenToggle(TEXT("Village"), LOCTEXT("HideVillage", "Hide village props"))];

	TSharedRef<SWrapBox> Stats = Wrap();
	for (const TCHAR* Stat : {TEXT("fps"), TEXT("unit"), TEXT("unitgraph"), TEXT("game"), TEXT("none")})
	{
		Stats->AddSlot()[ActionButton(Txt(FString::Printf(TEXT("stat %s"), Stat)), FString::Printf(TEXT("World.Stat %s"), Stat))];
	}
	Stats->AddSlot()[ActionButton(LOCTEXT("NavMesh", "Navmesh"), TEXT("World.NavMesh"))];
	Stats->AddSlot()
	[
		ActionButton(LOCTEXT("Markers", "Chore markers"), TEXT("World.ChoreMarkers"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]()
		             {
			             const UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld());
			             return Dev && Dev->bShowChoreMarkers;
		             }))
	];

	TSharedRef<SWrapBox> Crates = Wrap();
	Crates->AddSlot()[ActionButton(LOCTEXT("Crate1", "1 crate"), TEXT("Spawn.Crate 1"))];
	Crates->AddSlot()[ActionButton(LOCTEXT("Crate5", "5 crates"), TEXT("Spawn.Crate 5"))];
	Crates->AddSlot()[ActionButton(LOCTEXT("Crate15", "15 crates"), TEXT("Spawn.Crate 15"))];

	TSharedRef<float> SunPitch = MakeShared<float>(-42.0f);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecTeleport", "TELEPORT"), Places)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecLight", "LIGHT PREVIEW (THIS MACHINE)"), Look)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SliderRow(LOCTEXT("SunPitch", "Sun pitch"), -90.0f, 10.0f, 1.0f,
			          TAttribute<float>::CreateLambda([SunPitch]() { return *SunPitch; }),
			          [this, SunPitch](float V)
			          {
				          *SunPitch = V;
				          RunQuiet(FString::Printf(TEXT("World.Sun %.1f"), V));
			          },
			          0)
		]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecPerf", "PERF TESTS (THIS MACHINE)"), Perf)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Stats]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecCrates", "BREAKABLE CRATES"), Crates)];
}

TSharedRef<SWidget> SKGDevPanel::BuildChoresTab()
{
	using namespace KGDevPanelPrivate;
	TSharedRef<SWrapBox> Actions = Wrap();
	Actions->AddSlot()[ActionButton(LOCTEXT("DoneAll", "Complete mine"), TEXT("Chore.Done all"), uint8(EKind::Accent))];
	Actions->AddSlot()[ActionButton(LOCTEXT("ResetMine", "Reset mine"), TEXT("Chore.Reset"))];
	Actions->AddSlot()[ActionButton(LOCTEXT("DealAll", "Deal everyone new"), TEXT("Chore.Reset all"))];
	Actions->AddSlot()
	[
		ActionButton(LOCTEXT("Markers2", "Markers"), TEXT("World.ChoreMarkers"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([this]()
		             {
			             const UKGDevSubsystem* Dev = UKGDevSubsystem::Get(LocalWorld());
			             return Dev && Dev->bShowChoreMarkers;
		             }))
	];

	Actions->AddSlot()
	[
		ActionButton(LOCTEXT("AutoWin", "Auto-win"), TEXT("Chore.AutoWin"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([]() { return UKGChoreComponent::IsAutoWin(); }))
	];
	// Every chore minigame, playable anywhere (server-validated; practice unless it is on your list).
	TSharedRef<SWrapBox> Minigames = Wrap();
	for (const FKGChoreDef& Def : FKGChoreCatalog::GetAll())
	{
		Minigames->AddSlot()[ActionButton(FText::FromString(Def.Id.ToString() + (Def.IsVisual() ? TEXT(" *") : TEXT(""))),
		                                  TEXT("Chore.Play ") + Def.Id.ToString())];
	}

	SAssignNew(ChoresBox, SVerticalBox);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(Font(TEXT("Bold"), 12))
			.ColorAndOpacity(S().Cream)
			.Text_Lambda([this]()
			{
				const AKGGameState* GS = ReadGameState();
				const AKGPlayerState* Me = ReadMe();
				int32 Done = 0;
				for (const bool b : Me ? Me->TaskDone : TArray<bool>())
				{
					Done += b ? 1 : 0;
				}
				return Txt(FString::Printf(TEXT("Preparation %.0f%%   -   your chores %d / %d"), GS ? GS->Preparation * 100.0f : 0.0f,
				                           Done, Me ? Me->TaskIds.Num() : 0));
			})
		]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecChoreActions", "ACTIONS"), Actions)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecMinigames", "PLAY A MINIGAME HERE  (* = visible to others)"), Minigames)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecStations", "ALL CHORES"), ChoresBox.ToSharedRef())];
}

uint32 SKGDevPanel::ChoresSignature() const
{
	uint32 Hash = 0x85EBCA6Bu;
	if (const AKGPlayerState* Me = ReadMe())
	{
		for (const FName Id : Me->TaskIds)
		{
			Hash = HashCombine(Hash, GetTypeHash(Id));
		}
	}
	if (const UWorld* World = LocalWorld())
	{
		for (TActorIterator<AKGTaskStation> It(World); It; ++It)
		{
			Hash = HashCombine(Hash, GetTypeHash(*It));
		}
	}
	return Hash;
}

void SKGDevPanel::RebuildChores()
{
	using namespace KGDevPanelPrivate;
	ChoresSig = ChoresSignature();
	if (!ChoresBox.IsValid())
	{
		return;
	}
	ChoresBox->ClearChildren();
	TArray<AKGTaskStation*> Stations;
	for (TActorIterator<AKGTaskStation> It(LocalWorld()); It; ++It)
	{
		Stations.Add(*It);
	}
	Stations.Sort([](const AKGTaskStation& A, const AKGTaskStation& B) { return A.TaskId.LexicalLess(B.TaskId); });
	if (Stations.Num() == 0)
	{
		ChoresBox->AddSlot().AutoHeight()[Caption(LOCTEXT("NoStations", "NO CHORE STATIONS IN THIS LEVEL"))];
	}
	for (const AKGTaskStation* Station : Stations)
	{
		const FName TaskId = Station->TaskId;
		const FString Id = TaskId.ToString();
		// 0 = not yours, 1 = yours & open, 2 = yours & done
		auto State = [this, TaskId]() -> int32
		{
			const AKGPlayerState* Me = ReadMe();
			const int32 i = Me ? Me->TaskIds.IndexOfByKey(TaskId) : INDEX_NONE;
			return i == INDEX_NONE ? 0 : (Me->TaskDone.IsValidIndex(i) && Me->TaskDone[i]) ? 2 : 1;
		};
		ChoresBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(FKGMenuStyle::WithAlpha(S().Panel, 0.8f), 8.0f))
			.Padding(FMargin(10.0f, 5.0f, 6.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(Font(TEXT("Bold"), 11))
						.ColorAndOpacity_Lambda([State]()
						{
							const int32 St = State();
							return FSlateColor(St == 1 ? S().Gold : St == 2 ? S().Good : S().CreamDim);
						})
						.Text_Lambda([State, Id]()
						{
							const int32 St = State();
							return Txt(FString::Printf(TEXT("%s%s"), *Id, St == 1 ? TEXT("   YOURS") : St == 2 ? TEXT("   DONE") : TEXT("")));
						})
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(S().Muted).Text(Txt(Station->TaskName))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					ActionButton(LOCTEXT("GoChore", "Go"), TEXT("Chore.Goto ") + Id)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.Visibility_Lambda([State]() { return State() == 1 ? EVisibility::Visible : EVisibility::Hidden; })
					[
						ActionButton(LOCTEXT("DoChore", "Done"), TEXT("Chore.Done ") + Id, uint8(EKind::Accent))
					]
				]
			]
		];
	}
}

TSharedRef<SWidget> SKGDevPanel::BuildDebugTab()
{
	using namespace KGDevPanelPrivate;
	TSharedRef<SWrapBox> Hud = Wrap();
	static const TCHAR* HudNames[] = {TEXT("Off"), TEXT("1 Loop"), TEXT("2 Hurt"), TEXT("3 Healthy"), TEXT("4 Epilogue"), TEXT("5 Backstab")};
	for (int32 i = 0; i < UE_ARRAY_COUNT(HudNames); ++i)
	{
		Hud->AddSlot()
		[
			ActionButton(Txt(HudNames[i]), FString::Printf(TEXT("Debug.HUDDemo %d"), i), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([i]()
			             {
				             const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.HUDDemo"));
				             return CVar && CVar->GetInt() == i;
			             }))
		];
	}

	auto VM = [this]() -> UKGViewmodelComponent*
	{
		const AKGCharacter* C = PC.IsValid() ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr;
		return C ? C->GetViewmodel() : nullptr;
	};
	TSharedRef<SWrapBox> Presets = Wrap();
	static const TCHAR* PresetNames[] = {TEXT("1 Desktop"), TEXT("2 Couch"), TEXT("3 Classic")};
	for (int32 i = 0; i < 3; ++i)
	{
		Presets->AddSlot()
		[
			ActionButton(Txt(PresetNames[i]), FString::Printf(TEXT("Debug.VM Preset %d"), i + 1), uint8(EKind::Chip),
			             TAttribute<bool>::CreateLambda([VM, i]() { const UKGViewmodelComponent* V = VM(); return V && V->GetSettings().Preset == i + 1; }))
		];
	}
	Presets->AddSlot()
	[
		ActionButton(LOCTEXT("LeftHand", "Left-handed"), TEXT("Debug.VM Left 1"), uint8(EKind::Chip),
		             TAttribute<bool>::CreateLambda([VM]() { const UKGViewmodelComponent* V = VM(); return V && V->GetSettings().bLeftHanded; }))
	];
	Presets->AddSlot()[ActionButton(LOCTEXT("RightHand", "Right-handed"), TEXT("Debug.VM Left 0"))];

	auto SettingSlider = [this, VM](const FText& Label, const TCHAR* Key, float Min, float Max, float Step,
	                                TFunction<float(const FKGViewmodelSettings&)> Get)
	{
		const FString KeyName = Key;
		return SliderRow(Label, Min, Max, Step,
		                 TAttribute<float>::CreateLambda([VM, Get]() { const UKGViewmodelComponent* V = VM(); return V ? Get(V->GetSettings()) : 0.0f; }),
		                 [this, KeyName](float Value) { RunQuiet(FString::Printf(TEXT("Debug.VM %s %f"), *KeyName, Value)); });
	};
	TSharedRef<SVerticalBox> Sliders = SNew(SVerticalBox);
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMFov", "VM FOV"), TEXT("FOV"), 54.0f, 68.0f, 0.5f, [](const FKGViewmodelSettings& S) { return S.ViewmodelFOV; })];
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMX", "Offset X"), TEXT("X"), -2.5f, 2.5f, 0.05f, [](const FKGViewmodelSettings& S) { return float(S.Offset.X); })];
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMY", "Offset Y"), TEXT("Y"), -2.5f, 2.5f, 0.05f, [](const FKGViewmodelSettings& S) { return float(S.Offset.Y); })];
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMZ", "Offset Z"), TEXT("Z"), -2.5f, 2.5f, 0.05f, [](const FKGViewmodelSettings& S) { return float(S.Offset.Z); })];
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMBob", "Bob"), TEXT("Bob"), 0.0f, 1.0f, 0.05f, [](const FKGViewmodelSettings& S) { return S.BobScale; })];
	Sliders->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SettingSlider(LOCTEXT("VMSway", "Sway"), TEXT("Sway"), 0.0f, 1.0f, 0.05f, [](const FKGViewmodelSettings& S) { return S.SwayScale; })];

	// Tuning values straight from UKGViewmodelComponent's reflected floats (its API stays untouched).
	TSharedRef<SVerticalBox> Tuning = SNew(SVerticalBox);
	const UKGViewmodelComponent* Defaults = GetDefault<UKGViewmodelComponent>();
	for (const FName Prop : FKGDev::GetViewmodelTuningProperties())
	{
		const FFloatProperty* Float = FindFProperty<FFloatProperty>(UKGViewmodelComponent::StaticClass(), Prop);
		const float Default = Float ? Float->GetPropertyValue_InContainer(Defaults) : 0.0f;
		const float Max = FMath::Max(1.0f, FMath::Abs(Default) * 4.0f);
		Tuning->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SliderRow(Txt(Prop.ToString()), 0.0f, Max, Max / 200.0f,
			          TAttribute<float>::CreateLambda([VM, Prop]()
			          {
				          const UKGViewmodelComponent* V = VM();
				          const FFloatProperty* P = FindFProperty<FFloatProperty>(UKGViewmodelComponent::StaticClass(), Prop);
				          return V && P ? P->GetPropertyValue_InContainer(V) : 0.0f;
			          }),
			          [this, Prop](float Value) { RunQuiet(FString::Printf(TEXT("Debug.VMTune %s %f"), *Prop.ToString(), Value)); },
			          3)
		];
	}

	TSharedRef<SWidget> Net =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(Font(TEXT("Mono"), 10))
			.ColorAndOpacity(S().Cream)
			.Text_Lambda([this]()
			{
				const UWorld* World = LocalWorld();
				const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
				const APlayerState* PS = PC.IsValid() ? PC->PlayerState.Get() : nullptr;
				return Txt(FString::Printf(TEXT("mode   %s\nping   %.0f ms\nin     %.1f KB/s\nout    %.1f KB/s\nconns  %d\nfps    %.0f (%.1f ms)"),
				                           !World ? TEXT("-") : World->GetNetMode() == NM_Client ? TEXT("client")
				                           : World->GetNetMode() == NM_ListenServer ? TEXT("listen server") : TEXT("standalone"),
				                           PS ? PS->GetPingInMilliseconds() : 0.0f,
				                           Driver ? Driver->InBytesPerSecond / 1024.0f : 0.0f,
				                           Driver ? Driver->OutBytesPerSecond / 1024.0f : 0.0f,
				                           Driver ? Driver->ClientConnections.Num() : 0, SmoothedFps,
				                           SmoothedFps > 0.0f ? 1000.0f / SmoothedFps : 0.0f));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ActionButton(LOCTEXT("NetLog", "Log net"), TEXT("Debug.Net"))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ActionButton(LOCTEXT("StatNet", "stat net"), TEXT("World.Stat net"))]
		];

	TSharedRef<SWrapBox> State = Wrap();
	State->AddSlot()[ActionButton(LOCTEXT("Dump", "Copy state to log"), TEXT("Debug.Dump"), uint8(EKind::Accent),
	                              false, FLinearColor::Transparent, LOCTEXT("DumpTip", "kg.Debug.Dump: KG_DUMP lines in the log + clipboard"))];
	State->AddSlot()[ActionButton(LOCTEXT("Help", "Command list to log"), TEXT("Dev.Help"))];
	State->AddSlot()[ActionButton(LOCTEXT("ListBots", "Players to log"), TEXT("Bot.List"))];
	State->AddSlot()[ActionButton(LOCTEXT("ListPlaces", "Places to log"), TEXT("World.Places"))];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecHud", "HUD DEMO (kg.HUDDemo)"), Hud)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecVM", "VIEWMODEL"), Presets)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Sliders]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecTuning", "VIEWMODEL TUNING (LIVE)"), Tuning)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecNet", "NETWORK"), Net)]
		+ SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("SecState", "STATE"), State)];
}

#undef LOCTEXT_NAMESPACE

#endif
