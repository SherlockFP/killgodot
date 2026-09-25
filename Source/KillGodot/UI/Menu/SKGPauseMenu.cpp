#include "UI/Menu/SKGPauseMenu.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "Online/KGSessions.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Menu/KGMenuStyle.h"
#include "UI/Menu/SKGMenuArt.h"
#include "UI/Menu/SKGMenuWidgets.h"
#include "UI/Menu/SKGSettingsMenu.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGPauseMenu"

namespace
{
	/** True when keyboard focus sits on the bare game viewport (alt-tab, map events) instead of any widget. */
	bool PauseMenuFocusDrifted()
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
}

void SKGPauseMenu::Construct(const FArguments& InArgs)
{
	const FKGMenuStyle& S = FKGMenuStyle::Get();
	OwningPlayer = InArgs._OwningPlayer;
	OnResume = InArgs._OnResume;

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	auto Add = [this, &List](const FText& Text, FSimpleDelegate OnClick)
	{
		TSharedPtr<SKGMenuButton> Button;
		List->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SAssignNew(Button, SKGMenuButton)
			.Kind(EKGButtonKind::Menu)
			.Height(60.0f)
			.MinWidth(440.0f)
			.TextAlign(HAlign_Left)
			.Text(Text)
			.OnClicked(OnClick)
		];
		Buttons.Add(Button);
	};
	Add(LOCTEXT("Resume", "Resume"), FSimpleDelegate::CreateSP(this, &SKGPauseMenu::Resume));
	Add(LOCTEXT("Settings", "Settings"), FSimpleDelegate::CreateSP(this, &SKGPauseMenu::OpenSettings));
	Add(LOCTEXT("Leave", "Leave to main menu"), FSimpleDelegate::CreateSP(this, &SKGPauseMenu::ConfirmLeave));
	Add(LOCTEXT("Quit", "Quit game"), FSimpleDelegate::CreateSP(this, &SKGPauseMenu::ConfirmQuit));

	ChildSlot
	[
		SNew(SOverlay)
		// Blurred, dimmed game view
		+ SOverlay::Slot()
		[
			SNew(SBackgroundBlur)
			.bApplyAlphaToBlur(true)
			.BlurStrength_Lambda([this]() { return 7.0f * Appear; })
			.Padding(0.0f)
			[
				SNew(SImage)
				.Image(&S.WhiteBrush)
				.ColorAndOpacity_Lambda([this]()
				{
					return FSlateColor(FKGMenuStyle::WithAlpha(FKGMenuStyle::Get().Ink, (0.55f + 0.2f * SettingsAnim) * Appear));
				})
			]
		]
		// Button column
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(TAttribute<FMargin>::CreateLambda([this]()
		{
			return FMargin(FMath::Clamp(ViewSize.X * 0.057f, 40.0f, 110.0f), 40.0f, 0.0f, 40.0f);
		}))
		[
			SNew(SBox)
			.MinDesiredWidth(540.0f)
			.Visibility_Lambda([this]() { return SettingsAnim > 0.98f ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
			.RenderTransform_Lambda([this]()
			{
				const float Slide = -50.0f * (1.0f - FKGMenuStyle::EaseOutBack(Appear)) - 80.0f * SettingsAnim;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(Slide, 0.0f)));
			})
			[
				SNew(SBorder)
				.BorderImage(&S.NoBrush)
				.Padding(0.0f)
				.ColorAndOpacity_Lambda([this]()
				{
					return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Appear * 1.5f, 0.0f, 1.0f) * (1.0f - SettingsAnim));
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Paused", "Paused"))
						.Font(S.CaptionFont)
						.ColorAndOpacity(S.Lantern)
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(24.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(SKGLogo)
						.Size(58.0f)
						.ShowTagline(false)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(this, &SKGPauseMenu::GetSessionText)
						.Font(S.BodyBoldFont)
						.ColorAndOpacity(S.CreamDim)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FKGSessions::Get().DescribeHostedSession(OwningPlayer.Get()); })
						.Visibility_Lambda([this]()
						{
							return FKGSessions::Get().DescribeHostedSession(OwningPlayer.Get()).IsEmpty() ? EVisibility::Collapsed
							                                                                            : EVisibility::HitTestInvisible;
						})
						.Font(FKGMenuStyle::Get().SmallFont)
						.ColorAndOpacity(FKGMenuStyle::Get().Gold)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 4.0f, 0.0f, 30.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StillRunning", "The match keeps running while this menu is open."))
						.Font(S.SmallFont)
						.ColorAndOpacity(S.Muted)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						List
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(28.0f, 22.0f, 0.0f, 0.0f)
					[
						KGMenuUI::MakeKeyHint(LOCTEXT("KeyEsc", "Esc"), LOCTEXT("HintResume", "Resume"))
					]
				]
			]
		]
		// Settings
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
		// Dialogs
		+ SOverlay::Slot()
		[
			SAssignNew(ModalHost, SOverlay)
		]
	];

	PlayOpen();
}

void SKGPauseMenu::PlayOpen()
{
	Appear = 0.0f;
	SettingsAnim = 0.0f;
	bSettingsClosePending = false;
	if (SettingsHost.IsValid())
	{
		SettingsHost->ClearChildren();
	}
	Settings.Reset();
	if (ModalHost.IsValid())
	{
		ModalHost->ClearChildren();
	}
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		Buttons[Index]->PlayIntro(0.06f + 0.045f * Index);
	}
}

TSharedPtr<SWidget> SKGPauseMenu::GetInitialFocus() const
{
	return Buttons.Num() > 0 ? Buttons[0] : nullptr;
}

FText SKGPauseMenu::GetSessionText() const
{
	const APlayerController* PC = OwningPlayer.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return FText::GetEmpty();
	}
	const AGameStateBase* GameState = World->GetGameState();
	const int32 Players = GameState ? GameState->PlayerArray.Num() : 0;
	switch (World->GetNetMode())
	{
	case NM_ListenServer:
		return FText::Format(LOCTEXT("SessionHost", "You are hosting  \u00B7  {0} in the village"), FText::AsNumber(Players));
	case NM_Client:
		return FText::Format(LOCTEXT("SessionClient", "Online  \u00B7  {0} in the village"), FText::AsNumber(Players));
	default:
		return LOCTEXT("SessionOffline", "Offline practice");
	}
}

void SKGPauseMenu::Resume()
{
	OnResume.ExecuteIfBound();
}

void SKGPauseMenu::OpenSettings()
{
	if (Settings.IsValid())
	{
		return;
	}
	SettingsHost->ClearChildren();
	SettingsHost->AddSlot()
	[
		SAssignNew(Settings, SKGSettingsMenu)
		.OnClosed_Lambda([this]() { bSettingsClosePending = true; })
	];
	KGMenu::FocusWidget(Settings->GetInitialFocus());
}

void SKGPauseMenu::ConfirmLeave()
{
	const APlayerController* PC = OwningPlayer.Get();
	const bool bHosting = PC && PC->GetWorld() && PC->GetWorld()->GetNetMode() == NM_ListenServer;
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("LeaveTitle", "Leave the match?"))
		.Body(bHosting
			      ? LOCTEXT("LeaveBodyHost", "You are the host: leaving ends the match for everyone and returns you to the main menu.")
			      : LOCTEXT("LeaveBodyClient", "You will return to the main menu. The village carries on without you."))
		.ConfirmText(LOCTEXT("LeaveConfirm", "Leave"))
		.ConfirmKind(EKGButtonKind::Danger)
		.CancelText(LOCTEXT("LeaveCancel", "Stay"))
		.OnConfirm_Lambda([this]() { KGMenu::LeaveToMainMenu(OwningPlayer.Get()); })
		.OnCancel(this, &SKGPauseMenu::CloseModal));
}

void SKGPauseMenu::ConfirmQuit()
{
	const APlayerController* PC = OwningPlayer.Get();
	const bool bHosting = PC && PC->GetWorld() && PC->GetWorld()->GetNetMode() == NM_ListenServer;
	ShowModal(SNew(SKGModal)
		.Title(LOCTEXT("QuitTitle", "Quit Kill Godot?"))
		.Body(bHosting ? LOCTEXT("QuitBodyHost", "You are the host: quitting ends the match for everyone.")
		               : LOCTEXT("QuitBody", "The village will have to wait for you."))
		.ConfirmText(LOCTEXT("QuitConfirm", "Quit"))
		.ConfirmKind(EKGButtonKind::Danger)
		.CancelText(LOCTEXT("QuitCancel", "Stay"))
		.OnConfirm_Lambda([this]() { KGMenu::QuitGame(OwningPlayer.Get()); })
		.OnCancel(this, &SKGPauseMenu::CloseModal));
}

void SKGPauseMenu::ShowModal(const TSharedRef<SKGModal>& Modal)
{
	if (!IsModalOpen())
	{
		FocusBeforeModal = FSlateApplication::Get().GetUserFocusedWidget(0);
	}
	ModalHost->ClearChildren();
	ModalHost->AddSlot()[Modal];
	KGMenu::FocusWidget(Modal->GetDefaultFocus());
}

void SKGPauseMenu::CloseModal()
{
	ModalHost->ClearChildren();
	const TSharedPtr<SWidget> Previous = FocusBeforeModal.Pin();
	FocusBeforeModal.Reset();
	KGMenu::FocusWidget(Previous.IsValid() ? Previous : (Settings.IsValid() ? Settings->GetInitialFocus() : GetInitialFocus()));
}

bool SKGPauseMenu::IsModalOpen() const
{
	return ModalHost.IsValid() && ModalHost->GetNumWidgets() > 0;
}

void SKGPauseMenu::HandleBack()
{
	if (IsModalOpen())
	{
		CloseModal();
	}
	else if (Settings.IsValid())
	{
		Settings->RequestClose();
	}
	else
	{
		Resume();
	}
}

void SKGPauseMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	Appear = FMath::Min(1.0f, Appear + InDeltaTime / 0.25f);
	SettingsAnim = FKGMenuStyle::Approach(SettingsAnim, Settings.IsValid() ? 1.0f : 0.0f, InDeltaTime, 12.0f);
	if (bSettingsClosePending)
	{
		// Deferred so the settings widget is never destroyed inside its own click handler.
		bSettingsClosePending = false;
		SettingsHost->ClearChildren();
		Settings.Reset();
		KGMenu::FocusWidget(Buttons.IsValidIndex(1) ? Buttons[1] : GetInitialFocus());
	}
	else if (Appear >= 1.0f && PauseMenuFocusDrifted())
	{
		if (IsModalOpen())
		{
			KGMenu::FocusWidget(StaticCastSharedRef<SKGModal>(ModalHost->GetChildren()->GetChildAt(0))->GetDefaultFocus());
		}
		else
		{
			KGMenu::FocusWidget(Settings.IsValid() ? Settings->GetInitialFocus() : GetInitialFocus());
		}
	}
}

FReply SKGPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		HandleBack();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKGPauseMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

#undef LOCTEXT_NAMESPACE
