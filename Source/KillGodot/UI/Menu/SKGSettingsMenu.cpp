#include "UI/Menu/SKGSettingsMenu.h"

#include "Core/KGGameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Menu/SKGMenuWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGSettings"

namespace
{
	constexpr int32 KGQualityCount = 7;
	constexpr int32 KGFrameRateCount = 8;
	constexpr float KGFrameRates[KGFrameRateCount] = {30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f, 0.0f};
	constexpr float KGDisplayConfirmSeconds = 12.0f;

	int32 SettingsGetQuality(const UGameUserSettings& S, int32 Index)
	{
		switch (Index)
		{
		case 0: return S.GetViewDistanceQuality();
		case 1: return S.GetShadowQuality();
		case 2: return S.GetTextureQuality();
		case 3: return S.GetAntiAliasingQuality();
		case 4: return S.GetFoliageQuality();
		case 5: return S.GetVisualEffectQuality();
		default: return S.GetPostProcessingQuality();
		}
	}

	void SettingsSetQuality(UGameUserSettings& S, int32 Index, int32 Value)
	{
		switch (Index)
		{
		case 0: S.SetViewDistanceQuality(Value); break;
		case 1: S.SetShadowQuality(Value); break;
		case 2: S.SetTextureQuality(Value); break;
		case 3: S.SetAntiAliasingQuality(Value); break;
		case 4: S.SetFoliageQuality(Value); break;
		case 5: S.SetVisualEffectQuality(Value); break;
		default: S.SetPostProcessingQuality(Value); break;
		}
	}

	float SettingsResolutionScale(const UGameUserSettings& S)
	{
		float Normalized = 0.0f;
		float Value = 100.0f;
		float Min = 0.0f;
		float Max = 100.0f;
		S.GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
		return Value;
	}

	TArray<FText> SettingsQualityOptions()
	{
		return {LOCTEXT("QualityLow", "Low"), LOCTEXT("QualityMedium", "Medium"), LOCTEXT("QualityHigh", "High"),
		        LOCTEXT("QualityEpic", "Epic")};
	}

	FText SettingsResolutionLabel(const FIntPoint& Resolution)
	{
		return FText::Format(LOCTEXT("ResolutionFormat", "{0} \u00D7 {1}"), FText::AsNumber(Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()),
		                     FText::AsNumber(Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
	}

	EKGVolumeChannel SettingsChannel(int32 Channel)
	{
		return static_cast<EKGVolumeChannel>(FMath::Clamp(Channel, 0, 3));
	}
}

// --- FKGSettingsSnapshot ------------------------------------------------------------------------------------------

FKGSettingsSnapshot FKGSettingsSnapshot::Capture(const UKGGameUserSettings& Settings)
{
	FKGSettingsSnapshot Out;
	Out.WindowMode = static_cast<int32>(Settings.GetFullscreenMode());
	Out.Resolution = Settings.GetScreenResolution();
	Out.bVSync = Settings.IsVSyncEnabled();
	Out.FrameRateLimit = Settings.GetFrameRateLimit();
	Out.ResolutionScale = SettingsResolutionScale(Settings);
	for (int32 Index = 0; Index < KGQualityCount; ++Index)
	{
		Out.Quality[Index] = SettingsGetQuality(Settings, Index);
	}
	Out.Sensitivity = Settings.GetMouseSensitivity();
	Out.bInvertY = Settings.GetInvertY();
	Out.FieldOfView = Settings.GetFieldOfView();
	Out.ViewmodelPreset = Settings.GetViewmodelPreset();
	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		Out.Volumes[Channel] = Settings.GetVolume(SettingsChannel(Channel));
	}
	return Out;
}

void FKGSettingsSnapshot::Restore(UKGGameUserSettings& Settings) const
{
	Settings.SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(WindowMode));
	Settings.SetScreenResolution(Resolution);
	Settings.SetVSyncEnabled(bVSync);
	Settings.SetFrameRateLimit(FrameRateLimit);
	if (ResolutionScale <= 0.0f)
	{
		Settings.ScalabilityQuality.ResolutionQuality = ResolutionScale; // 0 = engine default screen percentage
	}
	else
	{
		Settings.SetResolutionScaleValueEx(ResolutionScale);
	}
	for (int32 Index = 0; Index < KGQualityCount; ++Index)
	{
		SettingsSetQuality(Settings, Index, Quality[Index]);
	}
	Settings.SetMouseSensitivity(Sensitivity);
	Settings.SetInvertY(bInvertY);
	Settings.SetFieldOfView(FieldOfView);
	Settings.SetViewmodelPreset(ViewmodelPreset);
	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		Settings.SetVolume(SettingsChannel(Channel), Volumes[Channel]);
	}
}

bool FKGSettingsSnapshot::Matches(const FKGSettingsSnapshot& Other) const
{
	if (WindowMode != Other.WindowMode || Resolution != Other.Resolution || bVSync != Other.bVSync ||
		!FMath::IsNearlyEqual(FrameRateLimit, Other.FrameRateLimit) ||
		!FMath::IsNearlyEqual(ResolutionScale, Other.ResolutionScale, 0.5f) ||
		!FMath::IsNearlyEqual(Sensitivity, Other.Sensitivity, 0.001f) || bInvertY != Other.bInvertY ||
		!FMath::IsNearlyEqual(FieldOfView, Other.FieldOfView, 0.01f) || ViewmodelPreset != Other.ViewmodelPreset)
	{
		return false;
	}
	for (int32 Index = 0; Index < KGQualityCount; ++Index)
	{
		if (Quality[Index] != Other.Quality[Index])
		{
			return false;
		}
	}
	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		if (!FMath::IsNearlyEqual(Volumes[Channel], Other.Volumes[Channel], 0.001f))
		{
			return false;
		}
	}
	return true;
}

// --- SKGSettingsMenu ------------------------------------------------------------------------------------------------

void SKGSettingsMenu::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	OnClosed = InArgs._OnClosed;
	if (const UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		Snapshot = FKGSettingsSnapshot::Capture(*Settings);
	}
	RefreshResolutions();

	const FText TabNames[Tab_Count] = {
		LOCTEXT("TabGraphics", "Graphics"), LOCTEXT("TabGameplay", "Gameplay"), LOCTEXT("TabAudio", "Audio"),
		LOCTEXT("TabControls", "Controls"), LOCTEXT("TabLanguage", "Language")};

	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	for (int32 Tab = 0; Tab < Tab_Count; ++Tab)
	{
		Tabs->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SAssignNew(TabButtons[Tab], SKGMenuButton)
			.Kind(EKGButtonKind::Tab)
			.Height(46.0f)
			.Text(TabNames[Tab])
			.IsSelected_Lambda([this, Tab]() { return ActiveTab == Tab; })
			.OnClicked_Lambda([this, Tab]() { SelectTab(Tab, false); })
		];
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&S.PanelBrush)
			.Padding(0.0f)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			.RenderTransform_Lambda([this]()
			{
				const float Scale = FMath::Lerp(0.965f, 1.0f, FKGMenuStyle::EaseOutBack(Appear));
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2f(Scale)));
			})
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Appear * 1.6f, 0.0f, 1.0f)); })
			[
				SNew(SVerticalBox)
				// Header
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(44.0f, 34.0f, 44.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						KGMenuUI::MakePanelTitle(LOCTEXT("Title", "Settings"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 18.0f, 0.0f)
						[
							KGMenuUI::MakeKeyHint(LOCTEXT("KeyQE", "Q / E"), LOCTEXT("HintTabs", "Switch tab"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							KGMenuUI::MakeKeyHint(LOCTEXT("KeyEsc", "Esc"), LOCTEXT("HintBack", "Back"))
						]
					]
				]
				// Tabs
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(32.0f, 18.0f, 32.0f, 0.0f)
				[
					Tabs
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(40.0f, 0.0f, 40.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(1.0f)
					[
						SNew(SImage)
						.Image(&S.WhiteBrush)
						.ColorAndOpacity(FKGMenuStyle::WithAlpha(S.Cream, 0.10f))
					]
				]
				// Body
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(30.0f, 14.0f, 22.0f, 6.0f)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return ActiveTab; })
					+ SWidgetSwitcher::Slot()[BuildGraphicsTab()]
					+ SWidgetSwitcher::Slot()[BuildGameplayTab()]
					+ SWidgetSwitcher::Slot()[BuildAudioTab()]
					+ SWidgetSwitcher::Slot()[BuildControlsTab()]
					+ SWidgetSwitcher::Slot()[BuildLanguageTab()]
				]
				// Footer
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(40.0f, 12.0f, 40.0f, 30.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(S.BodyBoldFont)
						.ColorAndOpacity(S.Gold)
						.Text_Lambda([this]()
						{
							return IsDirty() ? LOCTEXT("Unsaved", "Unsaved changes") : FText::GetEmpty();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Ghost)
						.MinWidth(150.0f)
						.Text(LOCTEXT("AutoDetect", "Auto-detect"))
						.Visibility_Lambda([this]() { return ActiveTab == Tab_Graphics ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnClicked(this, &SKGSettingsMenu::AutoDetect)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Ghost)
						.MinWidth(150.0f)
						.Text(LOCTEXT("ResetTab", "Reset"))
						.Visibility_Lambda([this]()
						{
							return ActiveTab == Tab_Gameplay || ActiveTab == Tab_Audio ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.OnClicked(this, &SKGSettingsMenu::ResetTab)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Ghost)
						.MinWidth(150.0f)
						.Text(LOCTEXT("Discard", "Discard"))
						.IsEnabled_Lambda([this]() { return IsDirty(); })
						.OnClicked(this, &SKGSettingsMenu::Discard)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Accent)
						.MinWidth(170.0f)
						.Text(LOCTEXT("Apply", "Apply"))
						.IsEnabled_Lambda([this]() { return IsDirty(); })
						.OnClicked(this, &SKGSettingsMenu::Apply)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SKGMenuButton)
						.Kind(EKGButtonKind::Secondary)
						.MinWidth(150.0f)
						.Text(LOCTEXT("Back", "Back"))
						.OnClicked(this, &SKGSettingsMenu::RequestClose)
					]
				]
			]
		]
		+ SOverlay::Slot()
		[
			SAssignNew(ModalHost, SOverlay)
		]
	];
}

TSharedPtr<SWidget> SKGSettingsMenu::GetInitialFocus() const
{
	return TabButtons[ActiveTab];
}

TSharedRef<SWidget> SKGSettingsMenu::MakePage(const TSharedRef<SVerticalBox>& Content) const
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	return SNew(SScrollBox)
		.Style(&S.ScrollBox)
		.ScrollBarStyle(&S.ScrollBar)
		.ScrollBarThickness(FVector2D(6.0f, 6.0f))
		.ScrollBarPadding(FMargin(10.0f, 4.0f, 0.0f, 4.0f))
		.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
		.NavigationDestination(EDescendantScrollDestination::IntoView)
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 12.0f, 12.0f)
		[
			Content
		];
}

namespace
{
	void SettingsAddSection(const TSharedRef<SVerticalBox>& Box, const FText& Title, bool bFirst = false)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(6.0f, bFirst ? 8.0f : 22.0f, 6.0f, 10.0f)
		[
			KGMenuUI::MakeSectionHeader(Title)
		];
	}

	void SettingsAddRow(const TSharedRef<SVerticalBox>& Box, const TSharedRef<SWidget>& Row)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			Row
		];
	}
}

TSharedRef<SWidget> SKGSettingsMenu::MakeQualityRow(const FText& Label, const FText& Description, int32 QualityIndex)
{
	return SNew(SKGSettingRow)
		.Label(Label)
		.Description(Description)
		[
			SNew(SKGOptionSelector)
			.Options(SettingsQualityOptions())
			.SelectedIndex_Lambda([QualityIndex]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? FMath::Clamp(SettingsGetQuality(*Settings, QualityIndex), 0, 3) : 0;
			})
			.OnSelectionChanged_Lambda([QualityIndex](int32 Index)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					SettingsSetQuality(*Settings, QualityIndex, Index);
				}
			})
		];
}

TSharedRef<SWidget> SKGSettingsMenu::BuildGraphicsTab()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	SettingsAddSection(Box, LOCTEXT("SectionDisplay", "Display"), true);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("WindowMode", "Window mode"))
		[
			SNew(SKGOptionSelector)
			.Options(TArray<FText>{LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Borderless window"),
			                       LOCTEXT("Windowed", "Windowed")})
			.SelectedIndex_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? static_cast<int32>(Settings->GetFullscreenMode()) : 2;
			})
			.OnSelectionChanged_Lambda([this](int32 Index)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					const EWindowMode::Type Mode = EWindowMode::ConvertIntToWindowMode(Index);
					Settings->SetFullscreenMode(Mode);
					RefreshResolutions();
					if (Mode == EWindowMode::WindowedFullscreen || !Resolutions.Contains(Settings->GetScreenResolution()))
					{
						Settings->SetScreenResolution(Mode == EWindowMode::WindowedFullscreen || Resolutions.Num() == 0
							                              ? Settings->GetDesktopResolution()
							                              : Resolutions.Last());
					}
				}
			})
		]);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("Resolution", "Resolution"))
		.Description(LOCTEXT("ResolutionDesc", "Borderless always uses your desktop resolution."))
		[
			SNew(SKGOptionSelector)
			.IsEnabled_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings && Settings->GetFullscreenMode() != EWindowMode::WindowedFullscreen;
			})
			.Options_Lambda([this]() { return ResolutionLabels; })
			.SelectedIndex_Lambda([this]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Resolutions.IndexOfByKey(Settings->GetScreenResolution()) : INDEX_NONE;
			})
			.OverrideText_Lambda([this]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings && !Resolutions.Contains(Settings->GetScreenResolution())
					? SettingsResolutionLabel(Settings->GetScreenResolution())
					: FText::GetEmpty();
			})
			.OnSelectionChanged_Lambda([this](int32 Index)
			{
				UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				if (Settings && Resolutions.IsValidIndex(Index))
				{
					Settings->SetScreenResolution(Resolutions[Index]);
				}
			})
		]);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("VSync", "VSync"))
		.Description(LOCTEXT("VSyncDesc", "Removes tearing, adds a little input latency."))
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SKGToggle)
				.IsChecked_Lambda([]()
				{
					const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
					return Settings && Settings->IsVSyncEnabled();
				})
				.OnToggled_Lambda([](bool bValue)
				{
					if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
					{
						Settings->SetVSyncEnabled(bValue);
					}
				})
			]
		]);
	{
		TArray<FText> RateLabels;
		for (const float Rate : KGFrameRates)
		{
			RateLabels.Add(Rate <= 0.0f ? LOCTEXT("Unlimited", "Unlimited") : FText::Format(LOCTEXT("FpsFormat", "{0} FPS"), FText::AsNumber(FMath::RoundToInt(Rate))));
		}
		SettingsAddRow(Box,
			SNew(SKGSettingRow)
			.Label(LOCTEXT("FrameRate", "Frame rate limit"))
			[
				SNew(SKGOptionSelector)
				.Options(RateLabels)
				.SelectedIndex_Lambda([]()
				{
					const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
					const float Limit = Settings ? Settings->GetFrameRateLimit() : 0.0f;
					if (Limit <= 0.0f)
					{
						return KGFrameRateCount - 1;
					}
					int32 Best = 0;
					for (int32 Index = 1; Index < KGFrameRateCount - 1; ++Index)
					{
						if (FMath::Abs(KGFrameRates[Index] - Limit) < FMath::Abs(KGFrameRates[Best] - Limit))
						{
							Best = Index;
						}
					}
					return Best;
				})
				.OnSelectionChanged_Lambda([](int32 Index)
				{
					UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
					if (Settings && Index >= 0 && Index < KGFrameRateCount)
					{
						Settings->SetFrameRateLimit(KGFrameRates[Index]);
					}
				})
			]);
	}
	{
		float Normalized = 0.0f;
		float Value = 100.0f;
		float MinScale = 50.0f;
		float MaxScale = 100.0f;
		if (const UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
		{
			Settings->GetResolutionScaleInformationEx(Normalized, Value, MinScale, MaxScale);
		}
		MinScale = FMath::Max(MinScale, 50.0f);
		MaxScale = FMath::Max(MaxScale, MinScale + 1.0f);
		SettingsAddRow(Box,
			SNew(SKGSettingRow)
			.Label(LOCTEXT("RenderScale", "Render scale"))
			.Description(LOCTEXT("RenderScaleDesc", "Renders the 3D scene at a lower resolution for more FPS; menus stay sharp."))
			.ValueText_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				const float Scale = Settings ? SettingsResolutionScale(*Settings) : 100.0f;
				return Scale <= 0.0f ? LOCTEXT("ScaleAuto", "Auto")
				                     : FText::Format(LOCTEXT("PercentFormat", "{0}%"), FText::AsNumber(FMath::RoundToInt(Scale)));
			})
			[
				SNew(SKGSlider)
				.MinValue(MinScale)
				.MaxValue(MaxScale)
				.StepSize(1.0f)
				.Value_Lambda([MaxScale]()
				{
					// 0 means "engine default" (sg.ResolutionQuality unset): show the knob at full resolution.
					const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
					const float Scale = Settings ? SettingsResolutionScale(*Settings) : MaxScale;
					return Scale <= 0.0f ? MaxScale : Scale;
				})
				.OnValueChanged_Lambda([](float NewValue)
				{
					if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
					{
						Settings->SetResolutionScaleValueEx(NewValue);
					}
				})
			]);
	}

	SettingsAddSection(Box, LOCTEXT("SectionQuality", "Quality"));
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("Preset", "Quality preset"))
		.Description(LOCTEXT("PresetDesc", "Sets every option below at once."))
		[
			SNew(SKGOptionSelector)
			.Options(SettingsQualityOptions())
			.SelectedIndex_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Settings->GetOverallScalabilityLevel() : INDEX_NONE;
			})
			.OverrideText_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : INDEX_NONE;
				return Level < 0 || Level > 3 ? LOCTEXT("QualityCustom", "Custom") : FText::GetEmpty();
			})
			.OnSelectionChanged_Lambda([](int32 Index)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetOverallScalabilityLevel(Index);
				}
			})
		]);
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("ViewDistance", "View distance"), FText::GetEmpty(), 0));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("Shadows", "Shadows"), FText::GetEmpty(), 1));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("Textures", "Textures"), FText::GetEmpty(), 2));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("AntiAliasing", "Anti-aliasing"), FText::GetEmpty(), 3));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("Foliage", "Foliage"), LOCTEXT("FoliageDesc", "Grass and plant density."), 4));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("Effects", "Effects"), FText::GetEmpty(), 5));
	SettingsAddRow(Box, MakeQualityRow(LOCTEXT("PostProcess", "Post-processing"), FText::GetEmpty(), 6));

	return MakePage(Box);
}

TSharedRef<SWidget> SKGSettingsMenu::BuildGameplayTab()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	SettingsAddSection(Box, LOCTEXT("SectionMouse", "Mouse"), true);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("Sensitivity", "Mouse sensitivity"))
		.ValueText_Lambda([]()
		{
			const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
			FNumberFormattingOptions Format;
			Format.MinimumFractionalDigits = 2;
			Format.MaximumFractionalDigits = 2;
			return FText::AsNumber(Settings ? Settings->GetMouseSensitivity() : 1.0f, &Format);
		})
		[
			SNew(SKGSlider)
			.MinValue(UKGGameUserSettings::MinMouseSensitivity)
			.MaxValue(UKGGameUserSettings::MaxMouseSensitivity)
			.StepSize(0.05f)
			.Value_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Settings->GetMouseSensitivity() : 1.0f;
			})
			.OnValueChanged_Lambda([this](float NewValue)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetMouseSensitivity(NewValue);
					OnLiveValueChanged();
				}
			})
		]);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("InvertY", "Invert look up/down"))
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SKGToggle)
				.IsChecked_Lambda([]()
				{
					const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
					return Settings && Settings->GetInvertY();
				})
				.OnToggled_Lambda([this](bool bValue)
				{
					if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
					{
						Settings->SetInvertY(bValue);
						OnLiveValueChanged();
					}
				})
			]
		]);

	SettingsAddSection(Box, LOCTEXT("SectionView", "View"));
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("FieldOfView", "Field of view"))
		.Description(LOCTEXT("FovDesc", "Horizontal, in degrees."))
		.ValueText_Lambda([]()
		{
			const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
			return FText::Format(LOCTEXT("DegreesFormat", "{0}\u00B0"),
			                     FText::AsNumber(FMath::RoundToInt(Settings ? Settings->GetFieldOfView() : UKGGameUserSettings::DefaultFieldOfView)));
		})
		[
			SNew(SKGSlider)
			.MinValue(UKGGameUserSettings::MinFieldOfView)
			.MaxValue(UKGGameUserSettings::MaxFieldOfView)
			.StepSize(1.0f)
			.Value_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Settings->GetFieldOfView() : UKGGameUserSettings::DefaultFieldOfView;
			})
			.OnValueChanged_Lambda([this](float NewValue)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetFieldOfView(NewValue);
					OnLiveValueChanged();
				}
			})
		]);
	SettingsAddRow(Box,
		SNew(SKGSettingRow)
		.Label(LOCTEXT("Viewmodel", "Viewmodel position"))
		.Description(LOCTEXT("ViewmodelDesc", "Where your hands and held item sit on screen (CS-style presets)."))
		[
			SNew(SKGOptionSelector)
			.bWrap(true)
			.Options(TArray<FText>{LOCTEXT("VmDesktop", "Desktop"), LOCTEXT("VmCouch", "Couch"), LOCTEXT("VmClassic", "Classic")})
			.SelectedIndex_Lambda([]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Settings->GetViewmodelPreset() - 1 : 0;
			})
			.OnSelectionChanged_Lambda([this](int32 Index)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetViewmodelPreset(Index + 1);
					OnLiveValueChanged();
				}
			})
		]);

	return MakePage(Box);
}

TSharedRef<SWidget> SKGSettingsMenu::MakeVolumeRow(const FText& Label, const FText& Description, int32 Channel)
{
	return SNew(SKGSettingRow)
		.Label(Label)
		.Description(Description)
		.ValueText_Lambda([Channel]()
		{
			const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
			return FText::Format(LOCTEXT("VolumeFormat", "{0}%"),
			                     FText::AsNumber(FMath::RoundToInt(100.0f * (Settings ? Settings->GetVolume(SettingsChannel(Channel)) : 1.0f))));
		})
		[
			SNew(SKGSlider)
			.MinValue(0.0f)
			.MaxValue(1.0f)
			.StepSize(0.01f)
			.Value_Lambda([Channel]()
			{
				const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
				return Settings ? Settings->GetVolume(SettingsChannel(Channel)) : 1.0f;
			})
			.OnValueChanged_Lambda([this, Channel](float NewValue)
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetVolume(SettingsChannel(Channel), NewValue);
					OnLiveValueChanged();
				}
			})
		];
}

TSharedRef<SWidget> SKGSettingsMenu::BuildAudioTab()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	SettingsAddSection(Box, LOCTEXT("SectionVolume", "Volume"), true);
	SettingsAddRow(Box, MakeVolumeRow(LOCTEXT("Master", "Master"), LOCTEXT("MasterDesc", "Everything you hear."),
	                                  static_cast<int32>(EKGVolumeChannel::Master)));
	SettingsAddRow(Box, MakeVolumeRow(LOCTEXT("Music", "Music"), LOCTEXT("MusicDesc", "Menu and match music."),
	                                  static_cast<int32>(EKGVolumeChannel::Music)));
	SettingsAddRow(Box, MakeVolumeRow(LOCTEXT("EffectsVolume", "Effects"),
	                                  LOCTEXT("EffectsDesc", "Footsteps, doors, fights and the village around you."),
	                                  static_cast<int32>(EKGVolumeChannel::Effects)));
	SettingsAddRow(Box, MakeVolumeRow(LOCTEXT("Voice", "Voice chat"), LOCTEXT("VoiceDesc", "Other players' voices."),
	                                  static_cast<int32>(EKGVolumeChannel::Voice)));
	return MakePage(Box);
}

TSharedRef<SWidget> SKGSettingsMenu::MakeKeysRow(const FText& Label, const TArray<FText>& Keys, const FText& Description)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedRef<SHorizontalBox> Caps = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (Keys[Index].EqualTo(FText::FromString(TEXT("/"))))
		{
			Caps->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("KeyOr", "or"))
				.Font(S.SmallFont)
				.ColorAndOpacity(S.Muted)
			];
			continue;
		}
		Caps->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(Index > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			KGMenuUI::MakeKeyCap(Keys[Index])
		];
	}
	return SNew(SKGSettingRow)
		.Label(Label)
		.Description(Description)
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				Caps
			]
		];
}

TSharedRef<SWidget> SKGSettingsMenu::BuildControlsTab()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const FText Or = FText::FromString(TEXT("/"));

	SettingsAddSection(Box, LOCTEXT("SectionMovement", "Movement"), true);
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Move", "Move"), {FText::FromString(TEXT("W")), FText::FromString(TEXT("A")),
	                                                         FText::FromString(TEXT("S")), FText::FromString(TEXT("D"))}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Jump", "Jump"), {LOCTEXT("KeySpace", "Space")}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Sprint", "Sprint"), {LOCTEXT("KeyShift", "Shift")}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Crouch", "Crouch"), {LOCTEXT("KeyCtrl", "Ctrl"), Or, FText::FromString(TEXT("C"))}));

	SettingsAddSection(Box, LOCTEXT("SectionActions", "Actions"));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Interact", "Interact"), {FText::FromString(TEXT("E"))}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Attack", "Attack / throw"), {LOCTEXT("KeyLMB", "Left mouse")}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Shove", "Shove"), {LOCTEXT("KeyRMB", "Right mouse")}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Inspect", "Inspect item"), {FText::FromString(TEXT("F"))}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Blade", "Blade"), {FText::FromString(TEXT("B"))},
	                                LOCTEXT("BladeDesc", "Impatient only.")));

	SettingsAddSection(Box, LOCTEXT("SectionMeeting", "Meeting & trial"));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Accuse", "Accuse"), {FText::FromString(TEXT("V"))}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Guilty", "Vote guilty"), {FText::FromString(TEXT("Y"))}));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Innocent", "Vote innocent"), {FText::FromString(TEXT("N"))}));

	SettingsAddSection(Box, LOCTEXT("SectionOther", "Other"));
	SettingsAddRow(Box, MakeKeysRow(LOCTEXT("Menu", "Menu"), {LOCTEXT("KeyEscName", "Esc")}));

	Box->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 18.0f, 8.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RebindSoon", "Key rebinding arrives in a later update."))
		.Font(S.SmallFont)
		.ColorAndOpacity(S.Muted)
	];
	return MakePage(Box);
}

TSharedRef<SWidget> SKGSettingsMenu::BuildLanguageTab()
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	SettingsAddSection(Box, LOCTEXT("SectionLanguage", "Language"), true);

	struct FLanguageOption
	{
		FString Code;
		FText Native;
		FText English;
	};
	const TArray<FLanguageOption> Languages = {
		{TEXT("en"), FText::AsCultureInvariant(TEXT("English")), LOCTEXT("LangEnglish", "English")},
		{TEXT("tr"), FText::AsCultureInvariant(TEXT("T\u00FCrk\u00E7e")), LOCTEXT("LangTurkish", "Turkish")},
		{TEXT("ru"), FText::AsCultureInvariant(TEXT("\u0420\u0443\u0441\u0441\u043A\u0438\u0439")), LOCTEXT("LangRussian", "Russian")},
	};

	for (const FLanguageOption& Option : Languages)
	{
		const FString Code = Option.Code;
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(SKGMenuButton)
			.Kind(EKGButtonKind::Card)
			.IsSelected_Lambda([Code]() { return UKGGameUserSettings::GetActiveLanguage().StartsWith(Code); })
			.OnClicked_Lambda([this, Code, Option]()
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetLanguage(Code);
					LanguageNotice = FText::Format(
						LOCTEXT("LanguageSet", "{0} selected. Translations are still in progress, so some text may stay in English for now."),
						Option.Native);
				}
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Option.Native)
						.Font(FKGMenuStyle::Font("Black", 24, 20))
						.ColorAndOpacity(S.Cream)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(Option.English)
						.Font(S.SmallFont)
						.ColorAndOpacity(S.Muted)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(&S.BadgeBrush)
					.Padding(FMargin(10.0f, 4.0f))
					.Visibility_Lambda([Code]()
					{
						return UKGGameUserSettings::GetActiveLanguage().StartsWith(Code) ? EVisibility::HitTestInvisible
						                                                                : EVisibility::Collapsed;
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LangActive", "Active"))
						.Font(S.CaptionFont)
						.ColorAndOpacity(S.Gold)
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]
				]
			]
		];
	}

	Box->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 10.0f, 8.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text_Lambda([this]()
		{
			return LanguageNotice.IsEmpty()
				? LOCTEXT("LanguageHint", "Applies immediately and is remembered for the next launch.")
				: LanguageNotice;
		})
		.Font(S.SmallFont)
		.ColorAndOpacity(S.CreamDim)
		.AutoWrapText(true)
	];
	return MakePage(Box);
}

void SKGSettingsMenu::RefreshResolutions()
{
	Resolutions.Reset();
	ResolutionLabels.Reset();
	const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	const EWindowMode::Type Mode = Settings->GetFullscreenMode();
	if (Mode == EWindowMode::WindowedFullscreen)
	{
		Resolutions.Add(Settings->GetDesktopResolution());
	}
	else if (Mode == EWindowMode::Fullscreen)
	{
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	}
	else
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
	}
	Resolutions.RemoveAll([](const FIntPoint& R) { return R.X <= 0 || R.Y <= 0; });
	TArray<FIntPoint> Unique;
	for (const FIntPoint& R : Resolutions)
	{
		Unique.AddUnique(R);
	}
	Resolutions = MoveTemp(Unique);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return static_cast<int64>(A.X) * A.Y != static_cast<int64>(B.X) * B.Y
			? static_cast<int64>(A.X) * A.Y < static_cast<int64>(B.X) * B.Y
			: A.X < B.X;
	});
	for (const FIntPoint& R : Resolutions)
	{
		ResolutionLabels.Add(SettingsResolutionLabel(R));
	}
}

void SKGSettingsMenu::SelectTab(int32 Tab, bool bFocusTab)
{
	ActiveTab = FMath::Clamp(Tab, 0, Tab_Count - 1);
	if (bFocusTab && TabButtons[ActiveTab].IsValid())
	{
		KGMenu::FocusWidget(TabButtons[ActiveTab]);
	}
}

bool SKGSettingsMenu::IsDirty() const
{
	const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	return Settings && !FKGSettingsSnapshot::Capture(*Settings).Matches(Snapshot);
}

void SKGSettingsMenu::OnLiveValueChanged()
{
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		Settings->ApplyLiveSettings();
	}
}

void SKGSettingsMenu::Apply()
{
	UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	const bool bDisplayChanged = Settings->GetFullscreenMode() != static_cast<EWindowMode::Type>(Snapshot.WindowMode) ||
		Settings->GetScreenResolution() != Snapshot.Resolution;
	Settings->ApplySettings(false);
	Snapshot = FKGSettingsSnapshot::Capture(*Settings);

	if (!bDisplayChanged || GIsEditor)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
		return;
	}

	ConfirmCountdown = KGDisplayConfirmSeconds;
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("KeepTitle", "Keep these display settings?"))
		.Body_Lambda([this]()
		{
			return FText::Format(LOCTEXT("KeepBody", "Your previous display settings come back in {0} seconds."),
			                     FText::AsNumber(FMath::Max(0, FMath::CeilToInt(ConfirmCountdown))));
		})
		.ConfirmText(LOCTEXT("Keep", "Keep"))
		.CancelText(LOCTEXT("Revert", "Revert"))
		.OnConfirm(this, &SKGSettingsMenu::KeepDisplayMode)
		.OnCancel(this, &SKGSettingsMenu::RevertDisplayMode));
}

void SKGSettingsMenu::KeepDisplayMode()
{
	ConfirmCountdown = -1.0f;
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}
	CloseModal();
}

void SKGSettingsMenu::RevertDisplayMode()
{
	ConfirmCountdown = -1.0f;
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		Settings->RevertVideoMode();
		Settings->ApplyResolutionSettings(false);
		Settings->SaveSettings();
		Snapshot = FKGSettingsSnapshot::Capture(*Settings);
	}
	RefreshResolutions();
	CloseModal();
}

void SKGSettingsMenu::Discard()
{
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		Snapshot.Restore(*Settings);
		Settings->RevertPendingScalability();
		for (int32 Index = 0; Index < KGQualityCount; ++Index)
		{
			SettingsSetQuality(*Settings, Index, Snapshot.Quality[Index]);
		}
		Settings->ApplyLiveSettings();
	}
	RefreshResolutions();
}

void SKGSettingsMenu::AutoDetect()
{
	if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
	{
		// Measures CPU/GPU for about a second and fills in the detected levels; they take effect on Apply.
		Settings->RunHardwareBenchmark();
	}
}

void SKGSettingsMenu::ResetTab()
{
	UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	if (ActiveTab == Tab_Gameplay)
	{
		Settings->SetMouseSensitivity(1.0f);
		Settings->SetInvertY(false);
		Settings->SetFieldOfView(UKGGameUserSettings::DefaultFieldOfView);
		Settings->SetViewmodelPreset(1);
	}
	else if (ActiveTab == Tab_Audio)
	{
		Settings->SetVolume(EKGVolumeChannel::Master, 1.0f);
		Settings->SetVolume(EKGVolumeChannel::Music, 0.7f);
		Settings->SetVolume(EKGVolumeChannel::Effects, 1.0f);
		Settings->SetVolume(EKGVolumeChannel::Voice, 1.0f);
	}
	OnLiveValueChanged();
}

void SKGSettingsMenu::RequestClose()
{
	if (IsModalOpen())
	{
		return;
	}
	if (!IsDirty())
	{
		Close();
		return;
	}
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("UnsavedTitle", "Apply your changes?"))
		.Body(LOCTEXT("UnsavedBody", "Some settings were changed but not applied yet."))
		.ConfirmText(LOCTEXT("ApplyAndClose", "Apply"))
		.CancelText(LOCTEXT("DiscardAndClose", "Discard"))
		.OnConfirm_Lambda([this]()
		{
			CloseModal();
			Apply();
			if (!IsModalOpen())
			{
				Close();
			}
		})
		.OnCancel_Lambda([this]()
		{
			CloseModal();
			Discard();
			Close();
		}));
}

void SKGSettingsMenu::Close()
{
	OnClosed.ExecuteIfBound();
}

void SKGSettingsMenu::ShowModal(const TSharedRef<SKGModal>& Modal)
{
	if (!IsModalOpen())
	{
		FocusBeforeModal = FSlateApplication::Get().GetUserFocusedWidget(0);
	}
	ModalHost->ClearChildren();
	ModalHost->AddSlot()[Modal];
	KGMenu::FocusWidget(Modal->GetDefaultFocus());
}

void SKGSettingsMenu::CloseModal()
{
	ModalHost->ClearChildren();
	const TSharedPtr<SWidget> Previous = FocusBeforeModal.Pin();
	FocusBeforeModal.Reset();
	KGMenu::FocusWidget(Previous.IsValid() ? Previous : StaticCastSharedPtr<SWidget>(TabButtons[ActiveTab]));
}

bool SKGSettingsMenu::IsModalOpen() const
{
	return ModalHost.IsValid() && ModalHost->GetNumWidgets() > 0;
}

void SKGSettingsMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Appear = FMath::Min(1.0f, Appear + InDeltaTime / 0.3f);
	if (ConfirmCountdown > 0.0f)
	{
		ConfirmCountdown -= InDeltaTime;
		if (ConfirmCountdown <= 0.0f)
		{
			RevertDisplayMode();
		}
	}
}

FReply SKGSettingsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsModalOpen())
	{
		if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
		{
			// Focus drifted outside the dialog: Esc still means the dialog's cancel.
			ModalHost->GetChildren()->GetChildAt(0)->OnKeyDown(MyGeometry, InKeyEvent);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		SelectTab((ActiveTab + Tab_Count - 1) % Tab_Count, true);
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		SelectTab((ActiveTab + 1) % Tab_Count, true);
		return FReply::Handled();
	}
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		RequestClose();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGSettingsMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Clicking empty panel space keeps keyboard focus inside the settings (so Esc and Q/E keep working).
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

#undef LOCTEXT_NAMESPACE
