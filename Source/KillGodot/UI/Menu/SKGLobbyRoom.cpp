#include "UI/Menu/SKGLobbyRoom.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Core/KGLobbyState.h"
#include "Core/KGPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerState.h"
#include "Misc/PackageName.h"
#include "Online/KGSessions.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Menu/SKGMenuWidgets.h"
#include "UI/Reveal/KGStreamerMode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGLobbyRoom"

namespace
{
	const FSlateBrush& LobbyDiscBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 18.0f);
		return Brush;
	}

	float LobbyEaseOut(float T)
	{
		const float X = FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - X, 3.0f);
	}

	/** True when keyboard focus fell back to the bare game viewport (map load, alt-tab). */
	bool LobbyFocusDrifted()
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

	TSharedRef<SWidget> LobbyBadge(const FText& Text, const FLinearColor& Color)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SBorder)
			.BorderImage(&S.BadgeBrush)
			.Padding(FMargin(8.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(S.CaptionFont)
				.ColorAndOpacity(Color)
			];
	}

	/** Read-only value on the right of a settings row (what clients see instead of the host's controls). */
	TSharedRef<SWidget> LobbyValue(TAttribute<FText> Text)
	{
		const FKGMenuStyle& S = FKGMenuStyle::Get();
		return SNew(SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(S.BodyBoldFont)
				.ColorAndOpacity(S.Cream)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}
}

SKGLobbyRoom::FChatPanelFactory& SKGLobbyRoom::ChatPanelFactory()
{
	static FChatPanelFactory Factory;
	return Factory;
}

const TArray<FText>& SKGLobbyRoom::GetRolePresetNames()
{
	static const TArray<FText> Names = {LOCTEXT("PresetClassic", "Classic"), LOCTEXT("PresetTown", "Town-heavy"),
	                                    LOCTEXT("PresetChaos", "Chaos")};
	return Names;
}

void SKGLobbyRoom::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	OwningPlayer = InArgs._OwningPlayer;
	bHost = IsHost();

	ChildSlot
	[
		SNew(SOverlay)
		// Blurred, dimmed village behind the lobby card.
		+ SOverlay::Slot()
		[
			SNew(SBackgroundBlur)
			.bApplyAlphaToBlur(true)
			.BlurStrength_Lambda([this]() { return 6.0f * Appear; })
			.Padding(0.0f)
			[
				SNew(SImage)
				.Image(&S.WhiteBrush)
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FKGMenuStyle::WithAlpha(FKGMenuStyle::Get().Ink, 0.62f * Appear)); })
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return PanelWidth(); })
			.HeightOverride_Lambda([this]() { return PanelHeight(); })
			.RenderTransform_Lambda([this]()
			{
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.0f, 30.0f * (1.0f - LobbyEaseOut(Appear)))));
			})
			[
				SNew(SBorder)
				.BorderImage(&S.PanelBrush)
				.Padding(FMargin(34.0f, 26.0f, 34.0f, 24.0f))
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, LobbyEaseOut(Appear)); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						BuildHeader()
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 18.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						// Seats
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 10.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									KGMenuUI::MakeSectionHeader(LOCTEXT("Players", "Villagers"))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(14.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Font(S.BodyBoldFont)
									.ColorAndOpacity(S.Gold)
									.Text_Lambda([this]()
									{
										const AKGLobbyState* Lobby = GetLobby();
										return Lobby ? FText::Format(LOCTEXT("SeatCount", "{0} / {1}"), FText::AsNumber(Lobby->CountHumans()),
										                             FText::AsNumber(Lobby->GetSettings().MaxPlayers))
										             : FText::GetEmpty();
									})
								]
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
								[
									SAssignNew(PlayersBox, SVerticalBox)
								]
							]
						]
						// Settings + chat
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(28.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride_Lambda([this]() { return FMath::Clamp(PanelWidth() * 0.38f, 340.0f, 520.0f); })
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									BuildSettingsCard()
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 16.0f, 0.0f, 8.0f)
								[
									KGMenuUI::MakeSectionHeader(LOCTEXT("Chat", "Chat"))
								]
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									BuildChatArea()
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 18.0f, 0.0f, 0.0f)
					[
						BuildFooter()
					]
				]
			]
		]
		// Countdown banner
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 20.0f, 0.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(&S.ToastBrush)
			.Padding(FMargin(34.0f, 10.0f))
			.Visibility_Lambda([this]()
			{
				const AKGLobbyState* Lobby = GetLobby();
				return Lobby && Lobby->IsCountingDown() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.RenderTransform_Lambda([this]()
			{
				const float Scale = 1.0f + 0.12f * CountdownPop;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(Scale));
			})
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BoatLeaves", "The boat leaves in"))
					.Font(S.SubheadingFont)
					.ColorAndOpacity(S.CreamDim)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FKGMenuStyle::Font("Black", 40))
					.ColorAndOpacity(S.Gold)
					.Text_Lambda([this]()
					{
						const AKGLobbyState* Lobby = GetLobby();
						return FText::AsNumber(Lobby ? FMath::Max(1, FMath::CeilToInt(Lobby->GetCountdownRemaining())) : 0);
					})
				]
			]
		]
		// Toast
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
		[
			SNew(SBorder)
			.BorderImage(&S.ToastBrush)
			.Padding(FMargin(24.0f, 12.0f))
			.Visibility_Lambda([this]() { return ToastTime > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(ToastTime / 0.35f, 0.0f, 1.0f)); })
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return ToastText; })
				.Font(S.BodyBoldFont)
				.ColorAndOpacity_Lambda([this]()
				{
					return FSlateColor(bToastError ? FMath::Lerp(FKGMenuStyle::Get().Crimson, FKGMenuStyle::Get().Cream, 0.3f)
					                               : FKGMenuStyle::Get().Cream);
				})
			]
		]
		+ SOverlay::Slot()
		[
			SAssignNew(ModalHost, SOverlay)
		]
	];

	RebuildPlayers();
}

// --- Layout -----------------------------------------------------------------------------------------------------------

float SKGLobbyRoom::PanelWidth() const
{
	return FMath::Max(640.0f, FMath::Min(1440.0f, ViewSize.X - 64.0f));
}

float SKGLobbyRoom::PanelHeight() const
{
	return FMath::Max(480.0f, FMath::Min(880.0f, ViewSize.Y - 64.0f));
}

TSharedRef<SWidget> SKGLobbyRoom::BuildHeader()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LobbyCaption", "Pre-game lobby"))
				.Font(S.CaptionFont)
				.ColorAndOpacity(S.Lantern)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(S.HeadingFont)
				.ColorAndOpacity(S.Cream)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.Text_Lambda([this]()
				{
					const AKGLobbyState* Lobby = GetLobby();
					return Lobby ? FText::FromString(Lobby->GetSettings().LobbyName) : FText::GetEmpty();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(S.SmallFont)
				.ColorAndOpacity(S.Muted)
				.Text_Lambda([this]()
				{
					const AKGLobbyState* Lobby = GetLobby();
					if (!Lobby)
					{
						return FText::GetEmpty();
					}
					const FKGLobbySettings& Settings = Lobby->GetSettings();
					const FText Access = Settings.bPrivate ? LOCTEXT("AccessPrivate", "Private")
						: (Settings.bPassword ? LOCTEXT("AccessPassword", "Password") : LOCTEXT("AccessOpen", "Open"));
					return FText::Format(LOCTEXT("LobbySubtitle", "{0}  \u00B7  {1}  \u00B7  {2}"), Access,
					                     FKGSessions::Get().GetBackendName(OwningPlayer.Get()),
					                     bHost ? LOCTEXT("YouHost", "You are the host") : LOCTEXT("YouGuest", "Hosted by another player"));
				})
			]
		]
		// Join code
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(18.0f, 0.0f, 18.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(&S.InsetBrush)
			.Padding(FMargin(18.0f, 8.0f))
			.Visibility_Lambda([this]()
			{
				const AKGLobbyState* Lobby = GetLobby();
				return Lobby && !Lobby->GetSettings().Code.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("JoinCode", "Join code"))
					.Font(S.CaptionFont)
					.ColorAndOpacity(S.Muted)
					.TransformPolicy(ETextTransformPolicy::ToUpper)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FKGMenuStyle::Font("Black", 26, 120))
					.ColorAndOpacity(S.Gold)
					.Text_Lambda([this]()
					{
						const AKGLobbyState* Lobby = GetLobby();
						return Lobby ? FText::AsCultureInvariant(KGStreamer::MaskCode(Lobby->GetSettings().Code)) : FText::GetEmpty();
					})
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Danger)
			.MinWidth(130.0f)
			.Height(46.0f)
			.Text(LOCTEXT("Leave", "Leave"))
			.OnClicked(this, &SKGLobbyRoom::RequestLeave)
		];
}

TSharedRef<SWidget> SKGLobbyRoom::BuildSettingsCard()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	const TArray<FKGMapEntry>& Maps = KGMenu::GetMaps();
	TArray<FText> MapNames;
	for (int32 Index = 0; Index < Maps.Num(); ++Index)
	{
		if (Maps[Index].bAvailable)
		{
			MapIndices.Add(Index);
			MapNames.Add(Maps[Index].DisplayName);
		}
	}
	constexpr float ControlWidth = 250.0f;

	auto MapTitle = [this]()
	{
		const AKGLobbyState* Lobby = GetLobby();
		const FString Path = Lobby ? Lobby->GetSettings().MapPath : FString();
		for (const FKGMapEntry& Map : KGMenu::GetMaps())
		{
			if (Map.MapPath == Path)
			{
				return Map.DisplayName;
			}
		}
		return FText::FromString(FPackageName::GetShortName(Path));
	};
	auto SelectedMap = [this]()
	{
		const AKGLobbyState* Lobby = GetLobby();
		const FString Path = Lobby ? Lobby->GetSettings().MapPath : FString();
		const TArray<FKGMapEntry>& All = KGMenu::GetMaps();
		for (int32 Index = 0; Index < MapIndices.Num(); ++Index)
		{
			if (All[MapIndices[Index]].MapPath == Path)
			{
				return Index;
			}
		}
		return 0;
	};
	auto Setting = [this]() -> FKGLobbySettings
	{
		const AKGLobbyState* Lobby = GetLobby();
		return Lobby ? Lobby->GetSettings() : FKGLobbySettings();
	};

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			KGMenuUI::MakeSectionHeader(bHost ? LOCTEXT("SettingsHost", "Match settings") : LOCTEXT("SettingsGuest", "Match settings (host)"))
		];

	// Map
	Card->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SKGSettingRow)
		.Label(LOCTEXT("Map", "Map"))
		.ControlWidth(ControlWidth)
		[
			bHost ? StaticCastSharedRef<SWidget>(SNew(SKGOptionSelector)
				.Options(MapNames)
				.SelectedIndex_Lambda(SelectedMap)
				.OnSelectionChanged(this, &SKGLobbyRoom::RequestMap))
			      : LobbyValue(TAttribute<FText>::CreateLambda(MapTitle))
		]
	];
	// Player count
	Card->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SKGSettingRow)
		.Label(LOCTEXT("MaxPlayers", "Max players"))
		.ControlWidth(ControlWidth)
		.ValueText_Lambda([bHostCopy = bHost, Setting]() { return bHostCopy ? FText::AsNumber(Setting().MaxPlayers) : FText::GetEmpty(); })
		[
			bHost ? StaticCastSharedRef<SWidget>(SNew(SKGSlider)
				.MinValue(static_cast<float>(AKGLobbyState::MinPlayersToStart))
				.MaxValue(static_cast<float>(KGMenu::MaxPlayers))
				.StepSize(1.0f)
				.Value_Lambda([Setting]() { return static_cast<float>(Setting().MaxPlayers); })
				.OnValueChanged_Lambda([this](float Value)
				{
					const int32 Players = FMath::RoundToInt(Value);
					const AKGLobbyState* Lobby = GetLobby();
					if (Lobby && Players != Lobby->GetSettings().MaxPlayers)
					{
						SendSettings([Players](FKGLobbySettings& Settings) { Settings.MaxPlayers = Players; });
					}
				}))
			      : LobbyValue(TAttribute<FText>::CreateLambda([Setting]() { return FText::AsNumber(Setting().MaxPlayers); }))
		]
	];
	// Role list preset
	Card->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SKGSettingRow)
		.Label(LOCTEXT("RoleList", "Role list"))
		.ControlWidth(ControlWidth)
		[
			bHost ? StaticCastSharedRef<SWidget>(SNew(SKGOptionSelector)
				.Options(GetRolePresetNames())
				.SelectedIndex_Lambda([Setting]() { return static_cast<int32>(Setting().RolePreset); })
				.OnSelectionChanged_Lambda([this](int32 Index)
				{
					SendSettings([Index](FKGLobbySettings& Settings) { Settings.RolePreset = static_cast<uint8>(Index); });
				}))
			      : LobbyValue(TAttribute<FText>::CreateLambda([Setting]()
			        {
				        const TArray<FText>& Names = GetRolePresetNames();
				        const int32 Index = Setting().RolePreset;
				        return Names.IsValidIndex(Index) ? Names[Index] : Names[0];
			        }))
		]
	];
	// Bots
	Card->AddSlot()
	.AutoHeight()
	[
		SNew(SKGSettingRow)
		.Label(LOCTEXT("FillBots", "Fill with bots"))
		.Description(FText::Format(LOCTEXT("FillBotsDesc", "Bots take empty seats up to {0} players."),
		                           FText::AsNumber(AKGLobbyState::MinPlayersToStart)))
		.ControlWidth(ControlWidth)
		[
			bHost ? StaticCastSharedRef<SWidget>(SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SKGToggle)
					.IsChecked_Lambda([Setting]() { return Setting().bFillWithBots; })
					.OnToggled_Lambda([this](bool bValue)
					{
						SendSettings([bValue](FKGLobbySettings& Settings) { Settings.bFillWithBots = bValue; });
					})
				])
			      : LobbyValue(TAttribute<FText>::CreateLambda([Setting]()
			        {
				        return Setting().bFillWithBots ? LOCTEXT("BotsOn", "On") : LOCTEXT("BotsOff", "Off");
			        }))
		]
	];
	return Card;
}

TSharedRef<SWidget> SKGLobbyRoom::BuildChatArea()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	if (ChatPanelFactory())
	{
		return ChatPanelFactory()(TWeakObjectPtr<APlayerController>(OwningPlayer.Get()));
	}
	return SNew(SBorder)
		.BorderImage(&S.InsetBrush)
		.Padding(FMargin(18.0f, 14.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ChatSoon", "Lobby chat arrives with the chat update."))
			.Font(S.SmallFont)
			.ColorAndOpacity(S.Muted)
			.AutoWrapText(true)
			.Justification(ETextJustify::Center)
		];
}

TSharedRef<SWidget> SKGLobbyRoom::BuildFooter()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedRef<SHorizontalBox> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 18.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SKGLobbyRoom::GetStatusText)
			.Font(S.BodyBoldFont)
			.ColorAndOpacity(S.CreamDim)
			.AutoWrapText(true)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			SAssignNew(ReadyButton, SKGMenuButton)
			.Kind(EKGButtonKind::Secondary)
			.MinWidth(190.0f)
			.Height(54.0f)
			.IsSelected_Lambda([this]() { return IsLocalReady(); })
			.Text_Lambda([this]() { return IsLocalReady() ? LOCTEXT("Unready", "Ready!") : LOCTEXT("Ready", "Ready up"); })
			.OnClicked(this, &SKGLobbyRoom::ToggleReady)
		];
	if (bHost)
	{
		Footer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Accent)
			.MinWidth(220.0f)
			.Height(54.0f)
			.IsEnabled_Lambda([this]()
			{
				const AKGLobbyState* Lobby = GetLobby();
				FText Reason;
				return Lobby && (Lobby->IsCountingDown() || Lobby->CanStart(Reason));
			})
			.Text_Lambda([this]()
			{
				const AKGLobbyState* Lobby = GetLobby();
				return Lobby && Lobby->IsCountingDown() ? LOCTEXT("CancelStart", "Cancel start") : LOCTEXT("StartMatch", "Start match");
			})
			.OnClicked(this, &SKGLobbyRoom::PressStart)
		];
	}
	return Footer;
}

void SKGLobbyRoom::RebuildPlayers()
{
	if (!PlayersBox.IsValid())
	{
		return;
	}
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	PlayersBox->ClearChildren();
	const AKGLobbyState* Lobby = GetLobby();
	if (!Lobby)
	{
		return;
	}
	const APlayerState* LocalState = OwningPlayer.IsValid() ? OwningPlayer->PlayerState.Get() : nullptr;
	const TArray<FKGLobbyEntry>& Entries = Lobby->GetEntries();
	for (const FKGLobbyEntry& Entry : Entries)
	{
		// The seat carries its own name copy: a joiner shows up by name as soon as the lobby replicates, even before
		// their player state has (SPRINT-015: names within 1 s on every machine).
		APlayerState* Player = Entry.Player;
		const FString SeatName = AKGLobbyState::GetSeatName(Entry);
		if (!Player && SeatName.IsEmpty())
		{
			continue;
		}
		const bool bYou = Player && Player == LocalState;
		// Streamer mode: everyone else gets this match's pseudonym.
		const FString ShownName = bYou ? SeatName
			: (Player ? KGStreamer::DisplayName(Player) : KGStreamer::DisplayNameOf(Lobby->GetWorld(), SeatName));
		const bool bReady = Entry.bReady;
		const bool bCanKick = bHost && !Entry.bHost && !bYou;
		TWeakObjectPtr<APlayerState> WeakPlayer = Player;
		PlayersBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 10.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(&S.InsetBrush)
			.BorderBackgroundColor(bYou ? FKGMenuStyle::WithAlpha(S.Gold, 0.9f) : FLinearColor::White)
			.Padding(FMargin(14.0f, 8.0f))
			[
				SNew(SHorizontalBox)
				// Avatar colour
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(34.0f)
					.HeightOverride(34.0f)
					[
						SNew(SImage)
						.Image(&LobbyDiscBrush())
						.ColorAndOpacity(AKGLobbyState::GetPlayerColor(Entry.ColorIndex))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ShownName))
					.Font(S.BodyBoldFont)
					.ColorAndOpacity(S.Cream)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.Visibility(Entry.bHost ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					[
						LobbyBadge(LOCTEXT("HostBadge", "HOST"), S.Gold)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.Visibility(bYou ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					[
						LobbyBadge(LOCTEXT("YouBadge", "YOU"), S.Ghost)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(18.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(92.0f)
					[
						SNew(STextBlock)
						.Text(bReady ? LOCTEXT("StateReady", "Ready") : LOCTEXT("StateNotReady", "Not ready"))
						.Font(S.SmallFont)
						.ColorAndOpacity(bReady ? S.Good : S.Muted)
						.Justification(ETextJustify::Right)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SKGCheckBox)
					.bReadOnly(!bYou)
					.IsChecked_Lambda([this, WeakPlayer]()
					{
						const AKGLobbyState* Current = GetLobby();
						const FKGLobbyEntry* Found = Current ? Current->FindEntry(WeakPlayer.Get()) : nullptr;
						return Found && Found->bReady;
					})
					.OnToggled_Lambda([this](bool) { ToggleReady(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(bHost ? 124.0f : 0.0f)
					.Visibility(bHost ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Ghost)
						.Height(36.0f)
						.Visibility(bCanKick ? EVisibility::Visible : EVisibility::Hidden)
						.Text(LOCTEXT("Kick", "Kick"))
						.OnClicked_Lambda([this, WeakPlayer]() { RequestKick(WeakPlayer); })
					]
				]
			]
		];
	}
	// Open seats up to the minimum, so the goal is visible.
	const int32 Humans = Lobby->CountHumans();
	const bool bBots = Lobby->GetSettings().bFillWithBots;
	for (int32 Seat = Humans; Seat < AKGLobbyState::MinPlayersToStart; ++Seat)
	{
		PlayersBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 10.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(&S.InsetBrush)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.45f))
			.Padding(FMargin(14.0f, 8.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(34.0f)
					.HeightOverride(34.0f)
					[
						SNew(SImage)
						.Image(&LobbyDiscBrush())
						.ColorAndOpacity(FKGMenuStyle::WithAlpha(S.Cream, 0.12f))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(bBots ? LOCTEXT("SeatBot", "Open seat  \u00B7  a bot joins at the start")
					            : LOCTEXT("SeatOpen", "Open seat  \u00B7  waiting for a villager"))
					.Font(S.SmallFont)
					.ColorAndOpacity(S.Muted)
				]
			]
		];
	}
}

// --- State ------------------------------------------------------------------------------------------------------------

AKGLobbyState* SKGLobbyRoom::GetLobby() const
{
	if (!CachedLobby.IsValid() && OwningPlayer.IsValid())
	{
		CachedLobby = AKGLobbyState::Get(OwningPlayer.Get());
	}
	return CachedLobby.Get();
}

bool SKGLobbyRoom::IsHost() const
{
	return OwningPlayer.IsValid() && OwningPlayer->IsLobbyHost();
}

bool SKGLobbyRoom::IsLocalReady() const
{
	const AKGLobbyState* Lobby = GetLobby();
	const FKGLobbyEntry* Entry = Lobby && OwningPlayer.IsValid() ? Lobby->FindEntry(OwningPlayer->PlayerState) : nullptr;
	return Entry && Entry->bReady;
}

FText SKGLobbyRoom::GetStatusText() const
{
	const AKGLobbyState* Lobby = GetLobby();
	if (!Lobby)
	{
		return LOCTEXT("StatusConnecting", "Opening the lobby...");
	}
	const FText ReadyCount = FText::Format(LOCTEXT("ReadyCount", "{0} of {1} ready"), FText::AsNumber(Lobby->CountReady()),
	                                       FText::AsNumber(Lobby->CountHumans()));
	if (Lobby->IsCountingDown())
	{
		return FText::Format(LOCTEXT("StatusCountdown", "Starting...  {0}"), ReadyCount);
	}
	if (bHost)
	{
		FText Reason;
		if (!Lobby->CanStart(Reason))
		{
			return FText::Format(LOCTEXT("StatusBlocked", "{0}  \u00B7  {1}"), ReadyCount, Reason);
		}
		return FText::Format(LOCTEXT("StatusHost", "{0}  \u00B7  start whenever the village is set"), ReadyCount);
	}
	return FText::Format(IsLocalReady() ? LOCTEXT("StatusWaitHost", "{0}  \u00B7  waiting for the host to start")
	                                    : LOCTEXT("StatusReadyUp", "{0}  \u00B7  press Ready when you are set"),
	                     ReadyCount);
}

// --- Actions ----------------------------------------------------------------------------------------------------------

void SKGLobbyRoom::SendSettings(TFunctionRef<void(FKGLobbySettings&)> Change)
{
	const AKGLobbyState* Lobby = GetLobby();
	if (!bHost || !Lobby || !OwningPlayer.IsValid())
	{
		return;
	}
	FKGLobbySettings Settings = Lobby->GetSettings();
	Change(Settings);
	OwningPlayer->ServerLobbyApplySettings(Settings);
}

void SKGLobbyRoom::RequestMap(int32 MapIndex)
{
	const TArray<FKGMapEntry>& Maps = KGMenu::GetMaps();
	if (!MapIndices.IsValidIndex(MapIndex))
	{
		return;
	}
	const FKGMapEntry& Map = Maps[MapIndices[MapIndex]];
	const AKGLobbyState* Lobby = GetLobby();
	if (!Lobby || Lobby->GetSettings().MapPath == Map.MapPath)
	{
		return;
	}
	const FString MapPath = Map.MapPath;
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("MapTitle", "Change the map?"))
		.Body(FText::Format(LOCTEXT("MapBody", "Everyone travels to {0} and the lobby opens again there."), Map.DisplayName))
		.ConfirmText(LOCTEXT("MapConfirm", "Change map"))
		.CancelText(LOCTEXT("MapCancel", "Keep"))
		.OnConfirm_Lambda([this, MapPath]()
		{
			CloseModal();
			SendSettings([MapPath](FKGLobbySettings& Settings) { Settings.MapPath = MapPath; });
		})
		.OnCancel(this, &SKGLobbyRoom::CloseModal));
}

void SKGLobbyRoom::ToggleReady()
{
	if (OwningPlayer.IsValid())
	{
		OwningPlayer->ServerLobbySetReady(!IsLocalReady());
	}
}

void SKGLobbyRoom::PressStart()
{
	const AKGLobbyState* Lobby = GetLobby();
	if (!bHost || !Lobby || !OwningPlayer.IsValid())
	{
		return;
	}
	FText Reason;
	if (!Lobby->IsCountingDown() && !Lobby->CanStart(Reason))
	{
		ShowToast(Reason, true);
		return;
	}
	OwningPlayer->ServerLobbyStart(!Lobby->IsCountingDown());
}

void SKGLobbyRoom::RequestLeave()
{
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("LeaveTitle", "Leave the lobby?"))
		.Body(bHost ? LOCTEXT("LeaveBodyHost", "You are the host: the lobby closes for everyone.")
		            : LOCTEXT("LeaveBody", "You go back to the title screen."))
		.ConfirmText(LOCTEXT("LeaveConfirm", "Leave"))
		.ConfirmKind(EKGButtonKind::Danger)
		.CancelText(LOCTEXT("LeaveCancel", "Stay"))
		.OnConfirm_Lambda([this]() { KGMenu::LeaveToMainMenu(OwningPlayer.Get()); })
		.OnCancel(this, &SKGLobbyRoom::CloseModal));
}

void SKGLobbyRoom::RequestKick(TWeakObjectPtr<APlayerState> Target)
{
	if (!Target.IsValid())
	{
		return;
	}
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("KickTitle", "Remove player?"))
		.Body(FText::Format(LOCTEXT("KickBody", "{0} is sent back to the title screen."), FText::FromString(KGStreamer::DisplayName(Target.Get()))))
		.ConfirmText(LOCTEXT("KickConfirm", "Kick"))
		.ConfirmKind(EKGButtonKind::Danger)
		.CancelText(LOCTEXT("KickCancel", "Cancel"))
		.OnConfirm_Lambda([this, Target]()
		{
			CloseModal();
			if (OwningPlayer.IsValid() && Target.IsValid())
			{
				OwningPlayer->ServerLobbyKick(Target.Get());
			}
		})
		.OnCancel(this, &SKGLobbyRoom::CloseModal));
}

// --- Modal / toast / input ------------------------------------------------------------------------------------------

void SKGLobbyRoom::ShowModal(const TSharedRef<SKGModal>& Modal)
{
	if (!IsModalOpen())
	{
		FocusBeforeModal = FSlateApplication::Get().GetUserFocusedWidget(0);
	}
	ModalHost->ClearChildren();
	ModalHost->AddSlot()[Modal];
	KGMenu::FocusWidget(Modal->GetDefaultFocus());
}

void SKGLobbyRoom::CloseModal()
{
	ModalHost->ClearChildren();
	const TSharedPtr<SWidget> Previous = FocusBeforeModal.Pin();
	KGMenu::FocusWidget(Previous.IsValid() ? Previous : GetInitialFocus());
	FocusBeforeModal.Reset();
}

bool SKGLobbyRoom::IsModalOpen() const
{
	return ModalHost.IsValid() && ModalHost->GetNumWidgets() > 0;
}

void SKGLobbyRoom::ShowToast(const FText& Message, bool bError)
{
	ToastText = Message;
	bToastError = bError;
	ToastTime = bError ? 5.0f : 3.5f;
}

TSharedPtr<SWidget> SKGLobbyRoom::GetInitialFocus() const
{
	return ReadyButton;
}

void SKGLobbyRoom::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	Life += InDeltaTime;
	Appear = FMath::Min(1.0f, Appear + InDeltaTime / 0.35f);
	ToastTime = FMath::Max(0.0f, ToastTime - InDeltaTime);
	CountdownPop = FKGMenuStyle::Approach(CountdownPop, 0.0f, InDeltaTime, 6.0f);

	if (const AKGLobbyState* Lobby = GetLobby())
	{
		if (Lobby->GetRevision() != SeenRevision)
		{
			SeenRevision = Lobby->GetRevision();
			RebuildPlayers();
		}
		const int32 Second = Lobby->IsCountingDown() ? FMath::CeilToInt(Lobby->GetCountdownRemaining()) : -1;
		if (Second != LastCountdownSecond)
		{
			if (Second >= 0)
			{
				CountdownPop = 1.0f;
			}
			else if (LastCountdownSecond > 0)
			{
				ShowToast(LOCTEXT("CountdownCancelled", "Start cancelled."));
			}
			LastCountdownSecond = Second;
		}
	}
	if (Life > 0.4f && LobbyFocusDrifted())
	{
		KGMenu::FocusWidget(IsModalOpen() ? StaticCastSharedRef<SKGModal>(ModalHost->GetChildren()->GetChildAt(0))->GetDefaultFocus()
		                                  : GetInitialFocus());
	}
}

FReply SKGLobbyRoom::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		if (IsModalOpen())
		{
			ModalHost->GetChildren()->GetChildAt(0)->OnKeyDown(MyGeometry, InKeyEvent);
		}
		else
		{
			RequestLeave();
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGLobbyRoom::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

#undef LOCTEXT_NAMESPACE
