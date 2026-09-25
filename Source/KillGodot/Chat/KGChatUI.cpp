#include "Chat/KGChatUI.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGChatRules.h"
#include "Chat/KGEmoji.h"
#include "Chat/KGEmojiText.h"
#include "Character/KGCharacter.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteCatalog.h"
#include "Emote/KGEmoteComponent.h"
#include "Emote/KGEmoteSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Core/KGGameState.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGSlateWidgets.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/Menu/KGMenuStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/Reveal/KGStreamerMode.h"

#define LOCTEXT_NAMESPACE "KGChat"

namespace KGChatUIPrivate
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.0f)
	{
		FLinearColor C = FLinearColor::FromSRGBColor(FColor(R, G, B));
		C.A = A;
		return C;
	}
	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	// HUD palette (UI/KGHUD.cpp): warm-dark ink panels, cream text, lantern gold.
	const FLinearColor Cream = Srgb(255, 244, 222);
	const FLinearColor Muted = Srgb(196, 182, 164);
	const FLinearColor Gold = Srgb(255, 184, 77);
	const FLinearColor InkTop = Srgb(40, 31, 58);
	const FLinearColor InkBottom = Srgb(19, 14, 29);

	constexpr float PanelWidth = 540.0f;
	constexpr float LeftMargin = 28.0f;
	constexpr float BottomMargin = 176.0f;   // above the purse pill and the vitals card (KGHUD DrawVitals / DrawPurse)
	constexpr float IdleHold = 12.0f;
	constexpr float IdleFade = 1.5f;
	constexpr int32 IdleLines = 8;
	constexpr int32 InlineEmoji = 19;
	constexpr int32 InputEmoji = 20;
	constexpr float BubbleSeconds = 2.8f;

	FSlateFontInfo Font(int32 Size, bool bBold = false, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	const FTextBlockStyle& LineStyle()
	{
		static const FTextBlockStyle Style = FTextBlockStyle()
			.SetFont(Font(14))
			.SetColorAndOpacity(FSlateColor(Cream))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
			.SetSelectedBackgroundColor(FSlateColor(WithAlpha(Gold, 0.35f)))
			.SetHighlightColor(Gold);
		return Style;
	}

	const FEditableTextBoxStyle& InputStyle()
	{
		static FEditableTextBoxStyle Style = []()
		{
			const FKGMenuStyle& M = FKGMenuStyle::Get();
			FEditableTextBoxStyle S = M.TextBox;
			const FSlateRoundedBoxBrush Clear(FLinearColor::Transparent, 8.0f);
			S.SetBackgroundImageNormal(Clear)
				.SetBackgroundImageHovered(Clear)
				.SetBackgroundImageFocused(Clear)
				.SetBackgroundImageReadOnly(Clear)
				.SetPadding(FMargin(6.0f, 5.0f))
				.SetFont(Font(15))
				.SetForegroundColor(FSlateColor(Cream))
				.SetFocusedForegroundColor(FSlateColor(Cream));
			return S;
		}();
		return Style;
	}

	float Saturate(float X)
	{
		return FMath::Clamp(X, 0.0f, 1.0f);
	}

	FVector2D MeasureText(const FString& Text, const FSlateFontInfo& Info)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D::ZeroVector;
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Info);
	}

	void DrawLabel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text,
	              const FSlateFontInfo& Info, const FVector2D& Centre, const FLinearColor& Color)
	{
		const FVector2D Size = MeasureText(Text, Info);
		FSlateDrawElement::MakeText(Out, Layer,
		                            Geo.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Centre - Size * 0.5))),
		                            Text, Info, ESlateDrawEffect::None, Color);
	}

	void DrawEmoji(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, int32 Emoji, const FVector2D& Centre,
	               float Size, float Alpha)
	{
		const FSlateBrush* Brush = FKGEmoji::GetLargeBrush(Emoji, 64);
		if (!Brush || Size <= 1.0f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, Layer,
		                           Geo.ToPaintGeometry(FVector2f(Size, Size),
		                                               FSlateLayoutTransform(FVector2f(Centre - FVector2D(Size * 0.5f)))),
		                           Brush, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
	}
}

using namespace KGChatUIPrivate;

// =================================================================================================================
// One chat line: channel chip + "Name: text" with inline emoji images.
// =================================================================================================================

class SKGChatRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGChatRow) {}
		SLATE_ARGUMENT(FKGChatMessage, Message)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Opacity = InArgs._Opacity;
		// Streamer mode (SPRINT-015): other players' names become this match's pseudonyms, in the name and in the text.
		FKGChatMessage Masked = InArgs._Message;
		if (KGStreamer::IsEnabled())
		{
			Masked.SenderName = KGStreamer::DisplayNameOf(nullptr, Masked.SenderName);
			Masked.Text = KGStreamer::MaskText(nullptr, Masked.Text);
		}
		const FKGChatMessage& M = Masked;
		const bool bSystem = M.Channel == EKGChatChannel::System;
		const bool bHint = M.HasFlag(EKGChatFlags::Hint);
		const bool bAction = M.HasFlag(EKGChatFlags::Action);
		const FLinearColor ChanColor = FKGChatRules::ChannelColor(M.Channel);
		const FLinearColor NameColor = FKGChatRules::NameColor(M.ColorSeed);

		// Body tint hints at the channel without shouting: ghosts slightly cyan, the killers' channel slightly rose.
		FLinearColor Body = Cream;
		if (M.Channel == EKGChatChannel::Dead)
		{
			Body = FMath::Lerp(Cream, ChanColor, 0.35f);
		}
		else if (M.Channel == EKGChatChannel::Team)
		{
			Body = FMath::Lerp(Cream, ChanColor, 0.25f);
		}

		FString Text;
		TArray<FKGEmojiTextMarshaller::FSpan> Spans;
		auto AddSpan = [&Spans](int32 Begin, int32 End, const FLinearColor& Color, bool bBold)
		{
			FKGEmojiTextMarshaller::FSpan Span;
			Span.Begin = Begin;
			Span.End = End;
			Span.Color = Color;
			Span.bBold = bBold;
			Spans.Add(Span);
		};
		if (bSystem)
		{
			Text = M.Text;
			AddSpan(0, Text.Len(), bHint ? Muted : FMath::Lerp(Cream, ChanColor, 0.55f), !bHint);
		}
		else if (bAction)
		{
			Text = FString::Printf(TEXT("* %s %s *"), *M.SenderName, *M.Text);
			AddSpan(0, 2, Muted, false);
			AddSpan(2, 2 + M.SenderName.Len(), NameColor, true);
			AddSpan(2 + M.SenderName.Len(), Text.Len(), Muted, false);
		}
		else
		{
			Text = M.SenderName + TEXT(": ") + M.Text;
			AddSpan(0, M.SenderName.Len(), NameColor, true);
			AddSpan(M.SenderName.Len(), Text.Len(), Body, false);
		}
		TSharedRef<FKGEmojiTextMarshaller> Marshaller = FKGEmojiTextMarshaller::Create(InlineEmoji, false);
		Marshaller->SetBoldFont(Font(14, true));
		Marshaller->SetSpans(MoveTemp(Spans));

		TSharedRef<SWidget> Chip = SNullWidget::NullWidget;
		if (!bHint)
		{
			Chip = SNew(SBorder)
				.BorderImage(KGSlate::Rounded(WithAlpha(ChanColor, 0.18f), 5.0f, WithAlpha(ChanColor, 0.75f), 1.0f))
				.Padding(FMargin(5.0f, 1.0f, 5.0f, 1.0f))
				[
					SNew(STextBlock)
					.Text(FKGChatRules::ChannelLabel(M.Channel))
					.Font(Font(9, true, 80))
					.ColorAndOpacity(ChanColor)
				];
		}

		SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SKGChatRow::GetRowVisibility));
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(WithAlpha(InkBottom, 0.62f), 8.0f))
			.BorderBackgroundColor(this, &SKGChatRow::GetBackground)
			.ColorAndOpacity(this, &SKGChatRow::GetTint)
			.Padding(FMargin(7.0f, 3.0f, 10.0f, 3.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 3.0f, bHint ? 0.0f : 7.0f, 0.0f)
				[
					Chip
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SMultiLineEditableText)
					.Visibility(EVisibility::HitTestInvisible)
					.IsReadOnly(true)
					.AutoWrapText(true)
					.TextStyle(&LineStyle())
					.Marshaller(Marshaller)
					.Text(FText::FromString(Text))
				]
			]
		];
	}

private:
	TAttribute<float> Opacity;

	float GetOpacity() const { return Opacity.Get(1.0f); }
	FSlateColor GetBackground() const { return FLinearColor(1.0f, 1.0f, 1.0f, GetOpacity()); }
	FLinearColor GetTint() const { return FLinearColor(1.0f, 1.0f, 1.0f, GetOpacity()); }
	EVisibility GetRowVisibility() const
	{
		return GetOpacity() > 0.01f ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	}
};

// =================================================================================================================
// Emoji picker cell
// =================================================================================================================

DECLARE_DELEGATE_TwoParams(FKGOnEmojiPicked, int32 /*Index*/, bool /*bReact*/);
DECLARE_DELEGATE_OneParam(FKGOnEmojiHovered, int32 /*Index or INDEX_NONE*/);

class SKGEmojiCell : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGEmojiCell) : _Index(0), _Size(30.0f) {}
		SLATE_ARGUMENT(int32, Index)
		SLATE_ARGUMENT(float, Size)
		SLATE_EVENT(FKGOnEmojiPicked, OnPicked)
		SLATE_EVENT(FKGOnEmojiHovered, OnHovered)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Index = InArgs._Index;
		OnPicked = InArgs._OnPicked;
		OnHovered = InArgs._OnHovered;
		const FSlateBrush* Brush = FKGEmoji::GetLargeBrush(Index, FMath::RoundToInt(InArgs._Size));
		TSharedRef<SWidget> Content = Brush
			? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Brush))
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(FKGEmoji::GetId(Index))).Font(Font(8)));
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(this, &SKGEmojiCell::GetBackground)
			.Padding(4.0f)
			.ToolTipText(FText::FromString(FString::Printf(TEXT(":%s:"), FKGEmoji::GetId(Index))))
			[
				SNew(SBox)
				.WidthOverride(InArgs._Size)
				.HeightOverride(InArgs._Size)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					Content
				]
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const bool bRight = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
		if (bRight || MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			OnPicked.ExecuteIfBound(Index, bRight);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		bHovered = true;
		OnHovered.ExecuteIfBound(Index);
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		bHovered = false;
		OnHovered.ExecuteIfBound(INDEX_NONE);
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return FCursorReply::Cursor(EMouseCursor::Hand);
	}

private:
	int32 Index = 0;
	bool bHovered = false;
	FKGOnEmojiPicked OnPicked;
	FKGOnEmojiHovered OnHovered;

	const FSlateBrush* GetBackground() const
	{
		return bHovered ? KGSlate::Rounded(WithAlpha(Gold, 0.22f), 9.0f, WithAlpha(Gold, 0.85f), 1.5f)
		                : KGSlate::Rounded(FLinearColor(1.0f, 1.0f, 1.0f, 0.03f), 9.0f);
	}
};

// =================================================================================================================
// Reaction bubbles over heads
// =================================================================================================================

class SKGReactionLayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGReactionLayer) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PlayerController;
		SetVisibility(EVisibility::HitTestInvisible);
	}

	void Add(APlayerState* Sender, int32 Emoji)
	{
		if (!Sender || Emoji == INDEX_NONE)
		{
			return;
		}
		FBubble* Bubble = Bubbles.FindByPredicate([Sender](const FBubble& B) { return B.Sender == Sender; });
		if (!Bubble)
		{
			Bubble = &Bubbles.AddDefaulted_GetRef();
			Bubble->Sender = Sender;
		}
		Bubble->Emoji = Emoji;
		Bubble->Start = FPlatformTime::Seconds();
		Bubble->bSelf = PC.IsValid() && PC->PlayerState == Sender;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		const double Now = FPlatformTime::Seconds();
		Bubbles.RemoveAll([Now](const FBubble& B) { return !B.Sender.IsValid() || Now - B.Start > BubbleSeconds; });
		APlayerController* Viewer = PC.Get();
		if (!Viewer)
		{
			Bubbles.Reset();
			return;
		}
		int32 VX = 0;
		int32 VY = 0;
		Viewer->GetViewportSize(VX, VY);
		const FVector2D Local = FVector2D(AllottedGeometry.GetLocalSize());
		const FVector2D PixelToLocal = VX > 0 && VY > 0 ? Local / FVector2D(VX, VY) : FVector2D(1.0);
		const FVector CamLoc = Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation()
		                                                   : FVector::ZeroVector;
		for (FBubble& B : Bubbles)
		{
			bool bTarget = false;
			// Your own bubble sits low in the centre in first person, over your head in the emote camera.
			const AKGCharacter* SelfBody = B.bSelf ? Cast<AKGCharacter>(Viewer->GetPawn()) : nullptr;
			const bool bSelfVisible = SelfBody && SelfBody->GetEmote() && SelfBody->GetEmote()->GetCameraAlpha() > 0.5f;
			if (B.bSelf && !bSelfVisible)
			{
				// First person: you cannot see your own head, so your bubble sits low in the centre of the screen.
				B.Pos = FVector2D(Local.X * 0.5f, Local.Y * 0.72f);
				B.Size = 58.0f;
				bTarget = true;
			}
			else if (const APawn* Pawn = B.Sender->GetPawn())
			{
				float Up = 95.0f;
				if (const ACharacter* Char = Cast<ACharacter>(Pawn))
				{
					Up = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 48.0f;
				}
				const FVector Head = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, Up);
				FVector2D Screen;
				if (Viewer->ProjectWorldLocationToScreen(Head, Screen, true))
				{
					const float Dist = FVector::Dist(CamLoc, Head);
					B.Pos = Screen * PixelToLocal;
					B.Size = static_cast<float>(FMath::GetMappedRangeValueClamped(FVector2D(300.0, 3000.0), FVector2D(72.0, 34.0), static_cast<double>(Dist)));
					bTarget = B.Pos.X > -80.0f && B.Pos.Y > -80.0f && B.Pos.X < Local.X + 80.0f && B.Pos.Y < Local.Y + 80.0f;
					if (bTarget && Viewer->GetWorld())
					{
						FCollisionQueryParams Params(SCENE_QUERY_STAT(KGReactionBubble), false, Pawn);
						Params.AddIgnoredActor(Viewer->GetPawn());
						FHitResult Hit;
						bTarget = !Viewer->GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, Head, ECC_Visibility, Params);
					}
				}
			}
			B.Vis = FKGMenuStyle::Approach(B.Vis, bTarget ? 1.0f : 0.0f, InDeltaTime, 12.0f);
		}
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& CullingRect,
	                      FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style,
	                      bool bParentEnabled) const override
	{
		const double Now = FPlatformTime::Seconds();
		for (const FBubble& B : Bubbles)
		{
			const float Age = static_cast<float>(Now - B.Start);
			const float Pop = FKGMenuStyle::EaseOutBack(Age / 0.28f);
			const float Fade = 1.0f - Saturate((Age - (BubbleSeconds - 0.45f)) / 0.45f);
			const float A = Fade * B.Vis * Saturate(Age / 0.08f);
			if (A <= 0.01f)
			{
				continue;
			}
			const float Size = B.Size * Pop;
			const float Bob = FMath::Sin(Age * 3.2f) * 3.0f;
			const FVector2D Tail(B.Pos.X, B.Pos.Y + Bob);
			const FVector2D BoxSize(Size + 18.0f, Size + 14.0f);
			const FVector2D BoxPos(Tail.X - BoxSize.X * 0.5f, Tail.Y - BoxSize.Y - 9.0f);
			const float R = FMath::Min(BoxSize.X, BoxSize.Y) * 0.34f;
			// shadow, bubble, tail, emoji
			FKGMenuStyle::DrawRoundedBox(Out, LayerId, Geo, FVector2f(BoxPos + FVector2D(0.0f, 4.0f)), FVector2f(BoxSize),
			                             FLinearColor(0.0f, 0.0f, 0.0f, 0.22f * A), R);
			FKGVertexCanvas Canvas(Out, Geo, A);
			const FLinearColor Fill = Srgb(255, 250, 238);
			const FLinearColor Ink = Srgb(42, 24, 51);
			const float TW = FMath::Max(8.0f, Size * 0.2f);
			Canvas.Tri(FVector2f(Tail.X - TW - 2.0f, BoxPos.Y + BoxSize.Y - 3.0f), FVector2f(Tail.X + TW + 2.0f, BoxPos.Y + BoxSize.Y - 3.0f),
			           FVector2f(Tail + FVector2D(0.0f, 2.0f)), Ink, Ink, Ink);
			Canvas.Flush(LayerId + 1);
			FKGMenuStyle::DrawRoundedBox(Out, LayerId + 2, Geo, FVector2f(BoxPos), FVector2f(BoxSize), WithAlpha(Fill, A), R,
			                             WithAlpha(Ink, A), 2.5f);
			Canvas.Tri(FVector2f(Tail.X - TW, BoxPos.Y + BoxSize.Y - 3.5f), FVector2f(Tail.X + TW, BoxPos.Y + BoxSize.Y - 3.5f),
			           FVector2f(Tail - FVector2D(0.0f, 1.5f)), Fill, Fill, Fill);
			Canvas.Flush(LayerId + 3);
			DrawEmoji(Out, LayerId + 4, Geo, B.Emoji, BoxPos + BoxSize * 0.5f, Size, A);
		}
		return LayerId + 5;
	}

private:
	struct FBubble
	{
		TWeakObjectPtr<APlayerState> Sender;
		int32 Emoji = 0;
		double Start = 0.0;
		float Vis = 0.0f;
		FVector2D Pos = FVector2D::ZeroVector;
		float Size = 48.0f;
		bool bSelf = false;
	};

	TWeakObjectPtr<APlayerController> PC;
	TArray<FBubble> Bubbles;
};

// =================================================================================================================
// Reaction wheel (hold G)
// =================================================================================================================

class SKGReactionWheel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGReactionWheel) {}
	SLATE_END_ARGS()

	/** GetSelected() values from here up are emote rows (FKGEmoteCatalog index + EmoteBase). */
	static constexpr int32 EmoteBase = 100;
	/** Virtual cursor length: short flick = emoji ring, long flick = emote ring. */
	static constexpr float DeadZone = 28.0f;
	static constexpr float InnerLimit = 100.0f;
	static constexpr float CursorMax = 190.0f;
	static constexpr float EmojiRadius = 124.0f;
	static constexpr float EmoteRadius = 222.0f;

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::HitTestInvisible);
		EmoteGrow.SetNumZeroed(FKGEmoteCatalog::GetAll().Num());
	}

	/** bInEmotes: the emote ring is live (a living body); ghosts only get the emoji ring. */
	void Open(bool bInEmotes)
	{
		bOpen = true;
		bEmotes = bInEmotes;
		Cursor = FVector2D::ZeroVector;
		Selected = INDEX_NONE;
		Favorites = UKGEmoteSubsystem::GetFavorites();
	}

	void Close() { bOpen = false; }
	bool IsOpen() const { return bOpen; }
	int32 GetSelected() const { return Selected; }
	bool IsEmoteSelected() const { return Selected >= EmoteBase; }
	const FKGEmoteDef* GetSelectedEmote() const { return IsEmoteSelected() ? FKGEmoteCatalog::Get(Selected - EmoteBase) : nullptr; }

	void AddDelta(const FVector2D& Delta)
	{
		Cursor += Delta;
		const float Max = bEmotes ? CursorMax : 150.0f;
		if (Cursor.Size() > Max)
		{
			Cursor *= Max / Cursor.Size();
		}
		const float Len = Cursor.Size();
		if (Len <= DeadZone)
		{
			return;
		}
		// Slot 0 at the top, clockwise (screen Y grows downwards).
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(Cursor.Y, Cursor.X)) + 90.0f;
		const int32 NumEmotes = FKGEmoteCatalog::GetAll().Num();
		if (bEmotes && Len > InnerLimit && NumEmotes > 0)
		{
			const float Step = 360.0f / NumEmotes;
			const float D = FMath::Fmod(Deg + 360.0f + Step * 0.5f, 360.0f);
			Selected = EmoteBase + FMath::Clamp(static_cast<int32>(D / Step), 0, NumEmotes - 1);
		}
		else
		{
			const float D = FMath::Fmod(Deg + 360.0f + 22.5f, 360.0f);
			Selected = FMath::Clamp(static_cast<int32>(D / 45.0f), 0, 7);
		}
	}

	void Select(int32 Slot)
	{
		Selected = FMath::Clamp(Slot, 0, 7);
		Cursor = FVector2D::ZeroVector;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(560.0f, 560.0f); }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		OpenAlpha = FKGMenuStyle::Approach(OpenAlpha, bOpen ? 1.0f : 0.0f, InDeltaTime, 16.0f);
		EmoteAlpha = FKGMenuStyle::Approach(EmoteAlpha, bOpen && bEmotes ? 1.0f : 0.0f, InDeltaTime, 12.0f);
		for (int32 i = 0; i < 8; ++i)
		{
			SlotGrow[i] = FKGMenuStyle::Approach(SlotGrow[i], i == Selected ? 1.0f : 0.0f, InDeltaTime, 18.0f);
		}
		for (int32 i = 0; i < EmoteGrow.Num(); ++i)
		{
			EmoteGrow[i] = FKGMenuStyle::Approach(EmoteGrow[i], i + EmoteBase == Selected ? 1.0f : 0.0f, InDeltaTime, 18.0f);
		}
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& CullingRect,
	                      FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style,
	                      bool bParentEnabled) const override
	{
		if (OpenAlpha <= 0.01f)
		{
			return LayerId;
		}
		const float A = OpenAlpha;
		const float K = 0.75f + 0.25f * FKGMenuStyle::EaseOutBack(OpenAlpha);
		const FVector2D C = FVector2D(Geo.GetLocalSize()) * 0.5f;

		// ---- outer emote ring (under the emoji disc) ----
		const TArray<FKGEmoteDef>& Emotes = FKGEmoteCatalog::GetAll();
		const float EA = A * EmoteAlpha;
		if (EA > 0.01f && Emotes.Num() > 0)
		{
			const float Outer = 540.0f * K;
			FKGMenuStyle::DrawRoundedBox(Out, LayerId, Geo, FVector2f(C - FVector2D(Outer * 0.5f)), FVector2f(Outer, Outer),
			                             WithAlpha(InkBottom, 0.62f * EA), -1.0f, WithAlpha(Gold, 0.22f * EA), 1.5f);
			for (int32 i = 0; i < Emotes.Num(); ++i)
			{
				const FKGEmoteDef& Def = Emotes[i];
				const float Ang = FMath::DegreesToRadians(-90.0f + 360.0f / Emotes.Num() * i);
				const FVector2D P = C + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * EmoteRadius * K;
				const float G = EmoteGrow.IsValidIndex(i) ? EmoteGrow[i] : 0.0f;
				const float SlotSize = (66.0f + 12.0f * G) * K;
				FKGMenuStyle::DrawRoundedBox(Out, LayerId + 1, Geo, FVector2f(P - FVector2D(SlotSize * 0.5f)), FVector2f(SlotSize, SlotSize),
				                             WithAlpha(FMath::Lerp(InkTop, Gold * 0.45f, G), 0.9f * EA), -1.0f,
				                             WithAlpha(FMath::Lerp(Cream * 0.35f, Gold, G), (0.3f + 0.7f * G) * EA), 1.2f + G);
				DrawEmoji(Out, LayerId + 2, Geo, FKGEmoji::Find(Def.Emoji), P - FVector2D(0.0f, 8.0f * K), (30.0f + 8.0f * G) * K, EA);
				DrawLabel(Out, LayerId + 3, Geo, Def.DisplayName.ToString(), Font(9, true), P + FVector2D(0.0f, 19.0f * K),
				          WithAlpha(FMath::Lerp(Cream, Gold, G), (0.8f + 0.2f * G) * EA));
				const int32 Fav = Favorites.IndexOfByKey(Def.Id);
				if (Fav != INDEX_NONE)
				{
					// Shift+N badge: a gold digit with an underline in the top-right corner of the slot.
					const FVector2D Badge = P + FVector2D(SlotSize * 0.36f, -SlotSize * 0.36f);
					DrawLabel(Out, LayerId + 3, Geo, FString::FromInt(Fav + 1), Font(9, true), Badge, WithAlpha(Gold, EA));
					FKGMenuStyle::DrawRoundedBox(Out, LayerId + 3, Geo, FVector2f(Badge + FVector2D(-5.0f, 7.0f)), FVector2f(10.0f, 2.0f),
					                             WithAlpha(Gold, EA), 1.0f);
				}
			}
		}

		// ---- emoji disc (inner ring) ----
		const float Disc = 340.0f * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 4, Geo, FVector2f(C - FVector2D(Disc * 0.5f)), FVector2f(Disc, Disc),
		                             WithAlpha(InkBottom, 0.78f * A), -1.0f, WithAlpha(Cream, 0.14f * A), 1.5f);
		const float Inner = 118.0f * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 5, Geo, FVector2f(C - FVector2D(Inner * 0.5f)), FVector2f(Inner, Inner),
		                             WithAlpha(InkTop, 0.9f * A), -1.0f, WithAlpha(Gold, 0.35f * A), 1.5f);
		const TArray<int32>& Emojis = FKGChatUI::GetWheelEmojis();
		for (int32 i = 0; i < 8 && i < Emojis.Num(); ++i)
		{
			const float Ang = FMath::DegreesToRadians(-90.0f + 45.0f * i);
			const FVector2D P = C + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * EmojiRadius * K;
			const float G = SlotGrow[i];
			const float SlotSize = (70.0f + 16.0f * G) * K;
			FKGMenuStyle::DrawRoundedBox(Out, LayerId + 5, Geo, FVector2f(P - FVector2D(SlotSize * 0.5f)),
			                             FVector2f(SlotSize, SlotSize),
			                             WithAlpha(FMath::Lerp(InkTop, Gold * 0.45f, G), (0.9f) * A), -1.0f,
			                             WithAlpha(FMath::Lerp(Cream * 0.4f, Gold, G), (0.35f + 0.65f * G) * A), 1.5f + G);
			DrawEmoji(Out, LayerId + 6, Geo, Emojis[i], P, (48.0f + 14.0f * G) * K, A);
			DrawLabel(Out, LayerId + 7, Geo, FString::FromInt(i + 1), Font(10, true),
			         P + FVector2D(SlotSize * 0.36f, SlotSize * 0.36f), WithAlpha(Cream, 0.7f * A));
		}
		FString Label = bEmotes ? LOCTEXT("WheelPickBoth", "flick: emoji / further: emote").ToString()
		                        : LOCTEXT("WheelPick", "flick the mouse").ToString();
		if (const FKGEmoteDef* Def = GetSelectedEmote())
		{
			Label = Def->DisplayName.ToString();
		}
		else if (Selected != INDEX_NONE && Emojis.IsValidIndex(Selected))
		{
			Label = FString::Printf(TEXT(":%s:"), FKGEmoji::GetId(Emojis[Selected]));
		}
		DrawLabel(Out, LayerId + 7, Geo, Label, Font(Selected == INDEX_NONE ? 10 : 13, true), C - FVector2D(0.0f, 8.0f),
		          WithAlpha(Gold, A));
		DrawLabel(Out, LayerId + 7, Geo, LOCTEXT("WheelRelease", "release G").ToString(), Font(10), C + FVector2D(0.0f, 14.0f),
		         WithAlpha(Muted, A));
		// virtual cursor (scaled so the ring radii line up with the flick distance)
		const FVector2D Dot = C + Cursor * (EmoteRadius / CursorMax) * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 8, Geo, FVector2f(Dot - FVector2D(5.0f)), FVector2f(10.0f, 10.0f),
		                             WithAlpha(Gold, 0.9f * A), -1.0f);
		return LayerId + 9;
	}

private:
	bool bOpen = false;
	bool bEmotes = false;
	float OpenAlpha = 0.0f;
	float EmoteAlpha = 0.0f;
	float SlotGrow[8] = {};
	TArray<float> EmoteGrow;
	TArray<FName> Favorites;
	FVector2D Cursor = FVector2D::ZeroVector;
	int32 Selected = INDEX_NONE;
};

// =================================================================================================================
// The overlay: log + input + picker + bubbles + wheel
// =================================================================================================================

class SKGChatOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGChatOverlay) : _Embedded(false) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
		/** Lobby panel mode: fills its slot, input always visible, no bubbles / wheel, never grabs focus. */
		SLATE_ARGUMENT(bool, Embedded)
	SLATE_END_ARGS()

	TSharedPtr<SKGReactionLayer> Bubbles;
	TSharedPtr<SKGReactionWheel> Wheel;

	virtual ~SKGChatOverlay() override
	{
		Unbind();
	}

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PlayerController;
		bEmbedded = InArgs._Embedded;
		bTyping = bEmbedded;
		InputMarshaller = FKGEmojiTextMarshaller::Create(InputEmoji, true);
		const FKGMenuStyle& MS = FKGMenuStyle::Get();

		// Emoji picker grid.
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f));
		for (int32 i = 0; i < FKGEmoji::Num(); ++i)
		{
			Grid->AddSlot(i % 8, i / 8)
			[
				SNew(SKGEmojiCell)
				.Index(i)
				.Size(bEmbedded ? 28.0f : 32.0f)
				.OnPicked(FKGOnEmojiPicked::CreateSP(this, &SKGChatOverlay::HandlePicked))
				.OnHovered(FKGOnEmojiHovered::CreateSP(this, &SKGChatOverlay::HandleHovered))
			];
		}
		const FSlateBrush* SmileBrush = FKGEmoji::GetLargeBrush(FKGEmoji::Find(TEXT("smile")), 24);

		SAssignNew(LogScroll, SScrollBox)
			.Style(&MS.ScrollBox)
			.ScrollBarStyle(&MS.ScrollBar)
			.ScrollBarVisibility(bEmbedded ? EVisibility::Visible : EVisibility::Collapsed)
			.ScrollBarThickness(FVector2D(5.0f, 5.0f))
			.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);

		TSharedRef<SWidget> Picker =
			SNew(SBorder)
			.Visibility(this, &SKGChatOverlay::GetPickerVisibility)
			.BorderImage(KGSlate::Rounded(WithAlpha(InkBottom, 0.95f), 12.0f, WithAlpha(Gold, 0.45f), 1.5f))
			.Padding(FMargin(8.0f, 6.0f, 8.0f, 6.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					Grid
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 4.0f, 2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &SKGChatOverlay::GetPickerHint)
					.Font(Font(10))
					.ColorAndOpacity(Muted)
					.AutoWrapText(true)
				]
			];

		TSharedRef<SWidget> Input =
			SNew(SBorder)
			.Visibility(this, &SKGChatOverlay::GetTypingVisibility)
			.BorderImage(this, &SKGChatOverlay::GetInputBackground)
			.Padding(FMargin(6.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&MS.BareButton)
					.OnClicked(this, &SKGChatOverlay::HandleChannelClicked)
					.ToolTipText(LOCTEXT("ChannelTip", "Channel (Tab)"))
					[
						SNew(SBorder)
						.BorderImage(this, &SKGChatOverlay::GetChannelChip)
						.Padding(FMargin(8.0f, 3.0f))
						[
							SNew(STextBlock)
							.Text(this, &SKGChatOverlay::GetChannelText)
							.Font(Font(10, true, 80))
							.ColorAndOpacity(this, &SKGChatOverlay::GetChannelColor)
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
				[
					SAssignNew(InputBox, SMultiLineEditableTextBox)
					.Style(&InputStyle())
					.Marshaller(InputMarshaller)
					.AutoWrapText(true)
					.AllowMultiLine(false)
					.ClearTextSelectionOnFocusLoss(false)
					.SelectAllTextWhenFocused(false)
					.RevertTextOnEscape(false)
					.AllowContextMenu(false)
					.HintText(this, &SKGChatOverlay::GetHintText)
					.OnTextChanged(this, &SKGChatOverlay::HandleTextChanged)
					.OnKeyDownHandler(this, &SKGChatOverlay::HandleInputKeyDown)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &SKGChatOverlay::GetCounterText)
					.Font(Font(10, true))
					.ColorAndOpacity(this, &SKGChatOverlay::GetCounterColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&MS.BareButton)
					.OnClicked(this, &SKGChatOverlay::HandlePickerClicked)
					.ToolTipText(LOCTEXT("PickerTip", "Emojis"))
					[
						SNew(SBorder)
						.BorderImage(this, &SKGChatOverlay::GetPickerButtonBrush)
						.Padding(4.0f)
						[
							SmileBrush
								? StaticCastSharedRef<SWidget>(SNew(SImage).Image(SmileBrush))
								: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(TEXT(":)"))).Font(Font(14, true)))
						]
					]
				]
			];

		if (bEmbedded)
		{
			// Lobby: fills the lobby card's chat slot; the input row is always there (the lobby is a menu).
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SBorder)
					.BorderImage(&MS.InsetBrush)
					.Padding(FMargin(8.0f, 6.0f))
					[
						LogScroll.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Picker
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Input
				]
			];
			return;
		}

		SetVisibility(EVisibility::SelfHitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay)
			.Visibility(this, &SKGChatOverlay::GetOverlayVisibility)
			+ SOverlay::Slot()
			[
				SAssignNew(Bubbles, SKGReactionLayer).PlayerController(PC)
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SAssignNew(Wheel, SKGReactionWheel)
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(LeftMargin, 0.0f, 0.0f, BottomMargin)
			[
				SNew(SBox)
				.WidthOverride(PanelWidth)
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)
					// ---- log ----
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(this, &SKGChatOverlay::GetLogBackground)
						.Padding(FMargin(6.0f))
						.Visibility(EVisibility::SelfHitTestInvisible)
						[
							SNew(SBox)
							.MaxDesiredHeight(this, &SKGChatOverlay::GetLogMaxHeight)
							[
								LogScroll.ToSharedRef()
							]
						]
					]
					// ---- emoji picker ----
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						Picker
					]
					// ---- input row ----
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						Input
					]
				]
			]
		];
	}

	/** Hidden while the pre-game lobby screen shows its own chat panel. */
	void SetSuppressed(bool bInSuppressed) { bSuppressed = bInSuppressed; }

	// ---- binding ----

	void SetChat(UKGChatComponent* InChat)
	{
		if (Chat.Get() == InChat)
		{
			return;
		}
		Unbind();
		Chat = InChat;
		LogScroll->ClearChildren();
		Rows.Reset();
		if (InChat)
		{
			LineHandle = InChat->OnLine.AddSP(this, &SKGChatOverlay::AppendLine);
			for (const FKGChatLine& Line : InChat->GetHistory())
			{
				AppendLine(Line);
			}
		}
	}

	UKGChatComponent* GetChat() const { return Chat.Get(); }

	// ---- typing ----

	bool IsTyping() const { return bTyping; }
	TSharedPtr<SWidget> GetFocusTarget() const { return InputBox; }

	void BeginTyping(const FString& Prefill)
	{
		bTyping = true;
		bPicker = false;
		SentCursor = INDEX_NONE;
		PickChannel(true);
		SetInputText(Prefill);
		LogScroll->SetScrollBarVisibility(EVisibility::Visible);
		LogScroll->ScrollToEnd();
	}

	void EndTyping()
	{
		bTyping = false;
		bPicker = false;
		TypingEndedAt = FPlatformTime::Seconds();
		SetInputText(FString());
		LogScroll->SetScrollBarVisibility(EVisibility::Collapsed);
		LogScroll->ScrollToEnd();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		if (bEmbedded && !Chat.IsValid() && PC.IsValid())
		{
			SetChat(UKGChatComponent::FindForController(PC.Get()));   // the component replicates after the menu opens
		}
		if (!bTyping)
		{
			return;
		}
		PickChannel(false);
		if (bConvertPending)
		{
			ConvertInput();
		}
		if (bEmbedded)
		{
			return;   // a menu: never pull focus away from its buttons
		}
		// Keep the caret in the box (a click on the game view must not strand the keyboard in UI-only mode).
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive() && InputBox.IsValid() &&
		    !InputBox->HasKeyboardFocus())
		{
			FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
		}
	}

private:
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<UKGChatComponent> Chat;
	FDelegateHandle LineHandle;
	TSharedPtr<SScrollBox> LogScroll;
	TSharedPtr<SMultiLineEditableTextBox> InputBox;
	TSharedPtr<FKGEmojiTextMarshaller> InputMarshaller;
	TArray<TSharedRef<SWidget>> Rows;
	uint32 LatestSerial = 0;
	bool bTyping = false;
	bool bPicker = false;
	bool bUpdatingText = false;
	bool bConvertPending = false;
	bool bEmbedded = false;
	bool bSuppressed = false;
	bool bHasChannel = false;
	EKGChatChannel Channel = EKGChatChannel::All;
	EKGChatChannel PreferredChannel = EKGChatChannel::All;
	TArray<FString> Sent;
	int32 SentCursor = INDEX_NONE;
	int32 HoveredEmoji = INDEX_NONE;
	double TypingEndedAt = -1000.0;

	void Unbind()
	{
		if (UKGChatComponent* Old = Chat.Get())
		{
			Old->OnLine.Remove(LineHandle);
		}
		LineHandle.Reset();
		Chat.Reset();
	}

	void AppendLine(const FKGChatLine& Line)
	{
		LatestSerial = FMath::Max(LatestSerial, Line.Serial);
		const uint32 Serial = Line.Serial;
		const double ReceivedAt = Line.ReceivedAt;
		TWeakPtr<SKGChatOverlay> WeakThis = StaticCastSharedRef<SKGChatOverlay>(AsShared());
		TSharedRef<SWidget> Row = SNew(SKGChatRow)
			.Message(Line.Message)
			.Opacity_Lambda([WeakThis, Serial, ReceivedAt]()
			{
				const TSharedPtr<SKGChatOverlay> Self = WeakThis.Pin();
				return Self.IsValid() ? Self->GetLineOpacity(Serial, ReceivedAt) : 0.0f;
			});
		Rows.Add(Row);
		LogScroll->AddSlot().Padding(FMargin(0.0f, 1.5f))[Row];
		while (Rows.Num() > UKGChatComponent::HistorySize)
		{
			LogScroll->RemoveSlot(Rows[0]);
			Rows.RemoveAt(0);
		}
		if (!bTyping || LogScroll->GetScrollOffsetOfEnd() - LogScroll->GetScrollOffset() < 40.0f)
		{
			LogScroll->ScrollToEnd();
		}
	}

	float GetLineOpacity(uint32 Serial, double ReceivedAt) const
	{
		if (bTyping)
		{
			return 1.0f;
		}
		if (LatestSerial - Serial >= static_cast<uint32>(IdleLines))
		{
			return 0.0f;
		}
		// Closing the chat lets recent history linger for a few seconds before it fades.
		const double Shown = FMath::Max(ReceivedAt, TypingEndedAt - IdleHold + 4.0);
		const float Age = static_cast<float>(FPlatformTime::Seconds() - Shown);
		return 1.0f - Saturate((Age - IdleHold) / IdleFade);
	}

	// ---- channels ----

	TArray<EKGChatChannel> GetChannels() const
	{
		const UKGChatComponent* C = Chat.Get();
		const UWorld* World = C ? C->GetWorld() : nullptr;
		const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
		if (!C)
		{
			return {};
		}
		return FKGChatRules::GetSendableChannels(C->MakeLocalParticipant(), GS ? GS->GetPhase() : EKGPhase::Lobby,
		                                         GS ? GS->GetPhaseRemaining() : 0.0f);
	}

	void PickChannel(bool bReset)
	{
		const TArray<EKGChatChannel> Channels = GetChannels();
		bHasChannel = Channels.Num() > 0;
		if (!bHasChannel)
		{
			return;
		}
		if (bReset && Channels.Contains(PreferredChannel))
		{
			Channel = PreferredChannel;
		}
		if (!Channels.Contains(Channel))
		{
			Channel = Channels[0];
		}
	}

	void CycleChannel(int32 Dir)
	{
		const TArray<EKGChatChannel> Channels = GetChannels();
		if (Channels.Num() == 0)
		{
			return;
		}
		const int32 Current = Channels.IndexOfByKey(Channel);
		Channel = Channels[(FMath::Max(Current, 0) + Dir + Channels.Num()) % Channels.Num()];
		PreferredChannel = Channel;
		bHasChannel = true;
	}

	FReply HandleChannelClicked()
	{
		CycleChannel(1);
		return FReply::Handled().SetUserFocus(InputBox.ToSharedRef(), EFocusCause::SetDirectly);
	}

	FText GetChannelText() const
	{
		return bHasChannel ? FKGChatRules::ChannelLabel(Channel) : LOCTEXT("ChanClosed", "CLOSED");
	}

	FSlateColor GetChannelColor() const
	{
		return bHasChannel ? FKGChatRules::ChannelColor(Channel) : Muted;
	}

	const FSlateBrush* GetChannelChip() const
	{
		const FLinearColor C = bHasChannel ? FKGChatRules::ChannelColor(Channel) : Muted;
		return KGSlate::Rounded(WithAlpha(C, 0.16f), 7.0f, WithAlpha(C, 0.8f), 1.2f);
	}

	// ---- input ----

	void SetInputText(const FString& Text)
	{
		TGuardValue<bool> Guard(bUpdatingText, true);
		InputBox->SetText(FText::FromString(Text));
		InputBox->GoTo(ETextLocation::EndOfDocument);
	}

	void HandleTextChanged(const FText& NewText)
	{
		// Converting here would be undone: the editable layout refreshes itself with the pre-callback text right
		// after OnTextChanged (live marshaller). Convert on the next tick instead.
		if (!bUpdatingText)
		{
			bConvertPending = true;
		}
	}

	void ConvertInput()
	{
		bConvertPending = false;
		const FString Raw = GetInputString();
		// Shortcodes close with ':' so they convert at once. Emoticons wait for the next space, so ":o" does not eat
		// the start of ":ok:" while you type.
		FString Converted = FKGEmoji::ConvertShortcodes(FKGEmoji::ConvertUnicode(Raw));
		int32 LastSpace = INDEX_NONE;
		Converted.FindLastChar(TEXT(' '), LastSpace);
		if (LastSpace != INDEX_NONE)
		{
			Converted = FKGEmoji::ConvertEmoticons(Converted.Left(LastSpace + 1)) + Converted.Mid(LastSpace + 1);
		}
		Converted.ReplaceInline(TEXT("\r"), TEXT(""));
		Converted.ReplaceInline(TEXT("\n"), TEXT(" "));
		Converted = Converted.Left(FKGChatRules::MaxChars);
		if (Converted != Raw)
		{
			SetInputText(Converted);
		}
	}

	FString GetInputString() const
	{
		return InputBox.IsValid() ? InputBox->GetText().ToString() : FString();
	}

	void Send()
	{
		const FString Text = FKGEmoji::ConvertAll(GetInputString()).TrimStartAndEnd();
		UKGChatComponent* C = Chat.Get();
		if (!Text.IsEmpty() && C)
		{
			if (!bHasChannel && !Text.StartsWith(TEXT("/")))
			{
				C->AddHint(FKGChatRules::RejectReason(EKGChatReject::Closed));
			}
			else
			{
				C->SubmitInput(Channel, Text);
			}
			Sent.Remove(Text);
			Sent.Add(Text);
			if (Sent.Num() > 20)
			{
				Sent.RemoveAt(0);
			}
		}
		if (bEmbedded)
		{
			SentCursor = INDEX_NONE;
			SetInputText(FString());
			LogScroll->ScrollToEnd();
			return;
		}
		FKGChatUI::CloseInput(PC.Get());
	}

	void RecallSent(int32 Dir)
	{
		if (Sent.Num() == 0)
		{
			return;
		}
		SentCursor = SentCursor == INDEX_NONE ? (Dir < 0 ? Sent.Num() - 1 : INDEX_NONE)
		                                      : SentCursor + Dir;
		if (SentCursor == INDEX_NONE || SentCursor >= Sent.Num())
		{
			SentCursor = INDEX_NONE;
			SetInputText(FString());
			return;
		}
		SentCursor = FMath::Max(SentCursor, 0);
		SetInputText(Sent[SentCursor]);
	}

	FReply HandleInputKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
	{
		const FKey Key = Event.GetKey();
		if (Key == EKeys::Enter)
		{
			Send();
			return FReply::Handled();
		}
		if (Key == EKeys::Escape)
		{
			if (bPicker)
			{
				bPicker = false;
			}
			else if (bEmbedded)
			{
				SetInputText(FString());   // the lobby keeps its own Back handling on its buttons
			}
			else
			{
				FKGChatUI::CloseInput(PC.Get());
			}
			return FReply::Handled();
		}
		if (Key == EKeys::Tab)
		{
			CycleChannel(Event.IsShiftDown() ? -1 : 1);
			return FReply::Handled();
		}
		if (Key == EKeys::Up || Key == EKeys::Down)
		{
			RecallSent(Key == EKeys::Up ? -1 : 1);
			return FReply::Handled();
		}
		if (Key == EKeys::PageUp || Key == EKeys::PageDown)
		{
			const float Step = 120.0f * (Key == EKeys::PageUp ? -1.0f : 1.0f);
			LogScroll->SetScrollOffset(FMath::Clamp(LogScroll->GetScrollOffset() + Step, 0.0f, LogScroll->GetScrollOffsetOfEnd()));
			return FReply::Handled();
		}
		if (Event.IsControlDown())
		{
			static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
			                              EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
			for (int32 i = 0; i < UE_ARRAY_COUNT(Digits); ++i)
			{
				if (Key == Digits[i] && FKGChatUI::GetWheelEmojis().IsValidIndex(i))
				{
					React(FKGChatUI::GetWheelEmojis()[i]);
					return FReply::Handled();
				}
			}
		}
		return FReply::Unhandled();
	}

	void React(int32 Emoji)
	{
		if (UKGChatComponent* C = Chat.Get())
		{
			C->RequestReaction(Emoji);
		}
	}

	// ---- picker ----

	FReply HandlePickerClicked()
	{
		bPicker = !bPicker;
		return FReply::Handled().SetUserFocus(InputBox.ToSharedRef(), EFocusCause::SetDirectly);
	}

	void HandlePicked(int32 Index, bool bReact)
	{
		if (bReact)
		{
			React(Index);
		}
		else if (GetInputString().Len() < FKGChatRules::MaxChars)
		{
			InputBox->InsertTextAtCursor(FString::Chr(FKGEmoji::ToChar(Index)));
		}
		FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
	}

	void HandleHovered(int32 Index)
	{
		HoveredEmoji = Index;
	}

	FText GetPickerHint() const
	{
		if (HoveredEmoji != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("PickerHover", ":{0}:     click: insert     right-click: react over your head"),
			                     FText::FromString(FKGEmoji::GetId(HoveredEmoji)));
		}
		return LOCTEXT("PickerHint", "Click: insert     Right-click: react     Ctrl+1-8: quick react     Esc: close");
	}

	// ---- attributes ----

	EVisibility GetOverlayVisibility() const
	{
		return bSuppressed ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
	}

	EVisibility GetTypingVisibility() const
	{
		return bTyping ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetPickerVisibility() const
	{
		return bTyping && bPicker ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetLogBackground() const
	{
		return bTyping ? KGSlate::Rounded(WithAlpha(InkBottom, 0.55f), 12.0f, FLinearColor(1.0f, 0.93f, 0.8f, 0.08f), 1.0f)
		               : KGSlate::Rounded(FLinearColor::Transparent, 12.0f);
	}

	FOptionalSize GetLogMaxHeight() const
	{
		return bTyping ? 330.0f : 260.0f;
	}

	const FSlateBrush* GetInputBackground() const
	{
		return KGSlate::Rounded(WithAlpha(InkBottom, 0.94f), 10.0f, WithAlpha(Gold, 0.9f), 1.5f);
	}

	const FSlateBrush* GetPickerButtonBrush() const
	{
		return bPicker ? KGSlate::Rounded(WithAlpha(Gold, 0.25f), 8.0f, Gold, 1.2f)
		               : KGSlate::Rounded(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), 8.0f);
	}

	FText GetHintText() const
	{
		if (!bHasChannel)
		{
			const UKGChatComponent* C = Chat.Get();
			if (C && C->IsSilenced())
			{
				return FKGChatRules::RejectReason(EKGChatReject::Silenced);
			}
			return LOCTEXT("HintClosed", "Chat is closed now. /help, or hold G to react.");
		}
		return LOCTEXT("HintOpen", "Say something...   Tab: channel   Esc: cancel");
	}

	FText GetCounterText() const
	{
		const int32 Len = GetInputString().Len();
		return Len >= FKGChatRules::MaxChars - 40 ? FText::AsNumber(FKGChatRules::MaxChars - Len) : FText::GetEmpty();
	}

	FSlateColor GetCounterColor() const
	{
		return GetInputString().Len() >= FKGChatRules::MaxChars - 10 ? FKGChatRules::ChannelColor(EKGChatChannel::Team)
		                                                               : Muted;
	}
};

// =================================================================================================================
// FKGChatUI
// =================================================================================================================

namespace KGChatUIPrivate
{
	struct FEntry
	{
		TSharedPtr<SKGChatOverlay> Overlay;
		TWeakObjectPtr<UGameViewportClient> Viewport;
		TWeakObjectPtr<APlayerController> PC;
		bool bLookIgnored = false;
		bool bWasCursorVisible = false;
		uint64 NextAttachFrame = 0;
	};

	TMap<TWeakObjectPtr<const ULocalPlayer>, FEntry>& Entries()
	{
		static TMap<TWeakObjectPtr<const ULocalPlayer>, FEntry> Map;
		return Map;
	}

	FEntry* Find(const APlayerController* PC)
	{
		const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		FEntry* Entry = LP ? Entries().Find(LP) : nullptr;
		return Entry && Entry->Overlay.IsValid() && Entry->PC.Get() == PC ? Entry : nullptr;
	}

	void HandleReaction(APlayerController* Viewer, APlayerState* Sender, int32 Emoji)
	{
		if (FEntry* Entry = Find(Viewer))
		{
			Entry->Overlay->Bubbles->Add(Sender, Emoji);
		}
	}

	void Detach(FEntry& Entry)
	{
		if (Entry.bLookIgnored)
		{
			if (APlayerController* PC = Entry.PC.Get())
			{
				PC->SetIgnoreLookInput(false);
			}
			Entry.bLookIgnored = false;
		}
		if (UGameViewportClient* Viewport = Entry.Viewport.Get(); Viewport && Entry.Overlay.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(Entry.Overlay.ToSharedRef());
		}
		if (Entry.Overlay.IsValid())
		{
			Entry.Overlay->SetChat(nullptr);
		}
	}
}

const TArray<int32>& FKGChatUI::GetWheelEmojis()
{
	static const TArray<int32> Emojis = []()
	{
		TArray<int32> Out;
		for (const TCHAR* Id : {TEXT("laugh"), TEXT("sus"), TEXT("skull"), TEXT("thumbsup"), TEXT("heart"), TEXT("wave"),
		                        TEXT("thumbsdown"), TEXT("angry")})
		{
			Out.Add(FKGEmoji::Find(Id));
		}
		return Out;
	}();
	return Emojis;
}

void FKGChatUI::Ensure(APlayerController* PC, UKGChatComponent* Chat)
{
	using namespace KGChatUIPrivate;
	static bool bBound = false;
	if (!bBound)
	{
		bBound = true;
		UKGChatComponent::OnReaction().AddStatic(&HandleReaction);
	}
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = LP ? LP->ViewportClient.Get() : nullptr;
	if (!Viewport || !Chat)
	{
		return;
	}
	for (auto It = Entries().CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().PC.IsValid())
		{
			Detach(It.Value());
			It.RemoveCurrent();
		}
	}
	FEntry& Entry = Entries().FindOrAdd(LP);
	if (Entry.PC.Get() != PC || Entry.Viewport.Get() != Viewport)
	{
		Detach(Entry);
		Entry = FEntry();
	}
	if (!Entry.Overlay.IsValid())
	{
		Entry.Overlay = SNew(SKGChatOverlay).PlayerController(PC);
		Entry.PC = PC;
		Entry.Viewport = Viewport;
	}
	// Map travel empties the viewport: put the overlay back (remove first, so it can never be added twice).
	if (!Entry.Overlay->GetParentWidget().IsValid() && GFrameCounter >= Entry.NextAttachFrame)
	{
		Entry.NextAttachFrame = GFrameCounter + 30;
		Viewport->RemoveViewportWidgetContent(Entry.Overlay.ToSharedRef());
		Viewport->AddViewportWidgetContent(Entry.Overlay.ToSharedRef(), 3);
	}
	Entry.Overlay->SetChat(Chat);
}

void FKGChatUI::Remove(APlayerController* PC)
{
	using namespace KGChatUIPrivate;
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	FEntry Entry;
	if (LP && Entries().RemoveAndCopyValue(LP, Entry))
	{
		if (Entry.Overlay.IsValid() && Entry.Overlay->IsTyping())
		{
			CloseInput(PC);
		}
		Detach(Entry);
	}
}

void FKGChatUI::RemoveForWorld(const UWorld* World)
{
	using namespace KGChatUIPrivate;
	for (auto It = Entries().CreateIterator(); It; ++It)
	{
		const APlayerController* PC = It.Value().PC.Get();
		if (!It.Key().IsValid() || !PC || PC->GetWorld() == World)
		{
			if (PC && It.Value().Overlay.IsValid() && It.Value().Overlay->IsTyping())
			{
				CloseInput(const_cast<APlayerController*>(PC));
			}
			Detach(It.Value());
			It.RemoveCurrent();
		}
	}
}

void FKGChatUI::RemoveAll()
{
	using namespace KGChatUIPrivate;
	for (TPair<TWeakObjectPtr<const ULocalPlayer>, FEntry>& Pair : Entries())
	{
		Detach(Pair.Value);
	}
	Entries().Reset();
}

bool FKGChatUI::IsTyping(const APlayerController* PC)
{
	const KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	return Entry && Entry->Overlay->IsTyping();
}

void FKGChatUI::OpenInput(APlayerController* PC, const FString& Prefill)
{
	KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	if (!Entry || Entry->Overlay->IsTyping())
	{
		return;
	}
	CloseWheel(PC, false);
	Entry->Overlay->BeginTyping(Prefill);
	Entry->bWasCursorVisible = PC->ShouldShowMouseCursor();
	// UI-only: keys go to the text box, the pawn gets no movement / look input until we give it back.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Entry->Overlay->GetFocusTarget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	PC->FlushPressedKeys();
}

void FKGChatUI::CloseInput(APlayerController* PC)
{
	KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	if (!Entry || !Entry->Overlay->IsTyping())
	{
		return;
	}
	Entry->Overlay->EndTyping();
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(Entry->bWasCursorVisible);
	PC->FlushPressedKeys();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

bool FKGChatUI::IsWheelOpen(const APlayerController* PC)
{
	const KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	return Entry && Entry->Overlay->Wheel->IsOpen();
}

void FKGChatUI::OpenWheel(APlayerController* PC)
{
	KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	if (!Entry || Entry->Overlay->IsTyping() || Entry->Overlay->Wheel->IsOpen())
	{
		return;
	}
	// The emote ring needs a living body (ghosts react through Dead chat only).
	const APlayerState* PS = PC->PlayerState;
	const AKGPlayerState* KGPS = Cast<AKGPlayerState>(PS);
	const bool bEmotes = UKGEmoteComponent::FindForPlayer(PS) != nullptr && (!KGPS || KGPS->LifeState == EKGLifeState::Alive);
	Entry->Overlay->Wheel->Open(bEmotes);
	if (!Entry->bLookIgnored)
	{
		PC->SetIgnoreLookInput(true);   // the mouse steers the wheel, not the camera; movement keeps working
		Entry->bLookIgnored = true;
	}
}

void FKGChatUI::UpdateWheel(APlayerController* PC, const FVector2D& MouseDelta)
{
	if (KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC); Entry && Entry->Overlay->Wheel->IsOpen())
	{
		Entry->Overlay->Wheel->AddDelta(MouseDelta);
	}
}

void FKGChatUI::SelectWheelSlot(APlayerController* PC, int32 Slot)
{
	if (KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC); Entry && Entry->Overlay->Wheel->IsOpen())
	{
		Entry->Overlay->Wheel->Select(Slot);
	}
}

void FKGChatUI::CloseWheel(APlayerController* PC, bool bSend)
{
	KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC);
	if (!Entry)
	{
		return;
	}
	if (Entry->bLookIgnored)
	{
		PC->SetIgnoreLookInput(false);
		Entry->bLookIgnored = false;
	}
	if (!Entry->Overlay->Wheel->IsOpen())
	{
		return;
	}
	const int32 Slot = Entry->Overlay->Wheel->GetSelected();
	const FKGEmoteDef* Emote = Entry->Overlay->Wheel->GetSelectedEmote();
	Entry->Overlay->Wheel->Close();
	UKGChatComponent* Chat = Entry->Overlay->GetChat();
	if (bSend && Chat && Emote)
	{
		Chat->RequestEmote(Emote->Id);   // bubble + narration + body animation, validated by the server
	}
	else if (bSend && Chat && GetWheelEmojis().IsValidIndex(Slot))
	{
		Chat->RequestReaction(GetWheelEmojis()[Slot]);
	}
}

void FKGChatUI::SetSuppressed(APlayerController* PC, bool bSuppressed)
{
	if (KGChatUIPrivate::FEntry* Entry = KGChatUIPrivate::Find(PC))
	{
		if (bSuppressed)
		{
			CloseWheel(PC, false);
			CloseInput(PC);
		}
		Entry->Overlay->SetSuppressed(bSuppressed);
	}
}

TSharedRef<SWidget> FKGChatUI::MakeLobbyPanel(APlayerController* PC)
{
	TSharedRef<SKGChatOverlay> Panel = SNew(SKGChatOverlay).PlayerController(PC).Embedded(true);
	Panel->SetChat(UKGChatComponent::FindForController(PC));   // may still be null: the panel binds when it replicates
	return Panel;
}

void FKGChatUI::ShowBubble(APlayerController* Viewer, APlayerState* Sender, int32 EmojiIndex)
{
	KGChatUIPrivate::HandleReaction(Viewer, Sender, EmojiIndex);
}

#undef LOCTEXT_NAMESPACE
