#include "Inventory/KGSlateWidgets.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/KGItemCatalog.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace KGSlate
{
	namespace
	{
		TMap<FString, TSharedPtr<FSlateBrush>>& BrushCache()
		{
			static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
			return Cache;
		}

		FLinearColor Hex(uint8 R, uint8 G, uint8 B, uint8 A = 255)
		{
			return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
		}
	}

	const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		const FString Key = FString::Printf(TEXT("R|%s|%.1f|%s|%.1f"), *Fill.ToString(), Radius, *Outline.ToString(),
		                                    OutlineWidth);
		TSharedPtr<FSlateBrush>& Brush = BrushCache().FindOrAdd(Key);
		if (!Brush.IsValid())
		{
			Brush = OutlineWidth > 0.0f
				        ? MakeShared<FSlateRoundedBoxBrush>(Fill, Radius, Outline, OutlineWidth)
				        : MakeShared<FSlateRoundedBoxBrush>(Fill, Radius);
		}
		return Brush.Get();
	}

	const FSlateBrush* Solid(const FLinearColor& Fill)
	{
		const FString Key = FString::Printf(TEXT("S|%s"), *Fill.ToString());
		TSharedPtr<FSlateBrush>& Brush = BrushCache().FindOrAdd(Key);
		if (!Brush.IsValid())
		{
			Brush = MakeShared<FSlateColorBrush>(Fill);
		}
		return Brush.Get();
	}

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	bool OpenModal(APlayerController* PC, const TSharedRef<SWidget>& Widget, int32 ZOrder, FKGModalHandle& OutHandle)
	{
		ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
		if (!Viewport)
		{
			return false;
		}
		Viewport->AddViewportWidgetContent(Widget, ZOrder);
		OutHandle.Widget = Widget;
		OutHandle.Viewport = Viewport;
		OutHandle.PlayerController = PC;
		OutHandle.bWasCursorVisible = PC->ShouldShowMouseCursor();

		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Widget);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->FlushPressedKeys();   // no stuck W/Shift while the window is up
		return true;
	}

	void CloseModal(FKGModalHandle& Handle)
	{
		UGameViewportClient* Viewport = Handle.Viewport.Get();
		if (Viewport && Handle.Widget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(Handle.Widget.ToSharedRef());
		}
		if (APlayerController* PC = Handle.PlayerController.Get())
		{
			if (Handle.bWasCursorVisible)
			{
				FInputModeGameAndUI Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Mode.SetHideCursorDuringCapture(false);
				PC->SetInputMode(Mode);
				PC->SetShowMouseCursor(true);
			}
			else
			{
				PC->SetInputMode(FInputModeGameOnly());
				PC->SetShowMouseCursor(false);
			}
			PC->FlushPressedKeys();
		}
		Handle = FKGModalHandle();
	}

	namespace Colors
	{
		FLinearColor Backdrop() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.55f); }
		FLinearColor Panel() { return Hex(43, 31, 23, 245); }        // dark tarred wood
		FLinearColor PanelInner() { return Hex(58, 42, 31, 255); }
		FLinearColor Slot() { return Hex(74, 54, 40, 255); }
		FLinearColor SlotHover() { return Hex(112, 82, 58, 255); }
		FLinearColor Gold() { return Hex(242, 194, 48, 255); }        // HUD stamina gold #F2C230
		FLinearColor Cream() { return Hex(246, 235, 217, 255); }
		FLinearColor Muted() { return Hex(191, 174, 152, 255); }
		FLinearColor Good() { return Hex(96, 186, 104, 255); }
		FLinearColor Bad() { return Hex(224, 65, 58, 255); }          // HUD health red #E0413A
	}
}

// ---- SKGGlyphTile ----

void SKGGlyphTile::Construct(const FArguments& InArgs)
{
	const FLinearColor Color = InArgs._Color;
	// Dark glyph on light tiles, light glyph on dark ones.
	const bool bLight = Color.GetLuminance() > 0.35f;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._Size)
		.HeightOverride(InArgs._Size)
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(Color, InArgs._Size * 0.22f, Color * 0.6f, 2.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InArgs._Glyph))
				.Font(KGSlate::Font(InArgs._FontSize, true))
				.ColorAndOpacity(bLight ? FLinearColor(0.08f, 0.06f, 0.05f, 1.0f) : KGSlate::Colors::Cream())
			]
		]
	];
}

// ---- SKGItemSlot ----

void SKGItemSlot::Construct(const FArguments& InArgs)
{
	SlotIndex = InArgs._SlotIndex;
	OnClicked = InArgs._OnClicked;
	OnHovered = InArgs._OnHovered;

	const FKGItemDef* Def = UKGItemCatalog::Find(InArgs._ItemId);
	const float Size = InArgs._Size;
	const FLinearColor Rarity = Def ? UKGItemCatalog::GetRarityColor(Def->Rarity) : KGSlate::Colors::Slot();

	TSharedRef<SOverlay> Content = SNew(SOverlay);
	if (Def)
	{
		Content->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SKGGlyphTile).Size(Size * 0.68f).Color(Def->IconColor).Glyph(Def->Glyph).FontSize(Def->Glyph.Len() > 2 ? 11 : 15)
		];
		if (InArgs._Count > 1)
		{
			Content->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(InArgs._Count))
				.Font(KGSlate::Font(12, true))
				.ColorAndOpacity(KGSlate::Colors::Cream())
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
			];
		}
	}

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(FLinearColor::White, 8.0f))
			.BorderBackgroundColor_Lambda([this, Def, Rarity]()
			{
				const FLinearColor Base = bHovered ? KGSlate::Colors::SlotHover() : KGSlate::Colors::Slot();
				// A hint of the rarity colour behind non-common items.
				return FSlateColor(Def && Def->Rarity != EKGRarity::Common ? FMath::Lerp(Base, Rarity, 0.22f) : Base);
			})
			.Padding(0.0f)
			[
				Content
			]
		]
	];
}

FReply SKGItemSlot::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	OnClicked.ExecuteIfBound(SlotIndex, MouseEvent);
	return FReply::Handled();
}

void SKGItemSlot::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	bHovered = true;
	OnHovered.ExecuteIfBound(SlotIndex);
}

void SKGItemSlot::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bHovered = false;
	OnHovered.ExecuteIfBound(INDEX_NONE);
}

// ---- SKGTextButton ----

void SKGTextButton::Construct(const FArguments& InArgs)
{
	BaseColor = InArgs._Color;
	Enabled = InArgs._Enabled;
	OnClicked = InArgs._OnClicked;
	const FLinearColor TextColor = InArgs._TextColor;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(InArgs._MinWidth)
		[
			SNew(SBorder)
			.BorderImage(KGSlate::Rounded(FLinearColor::White, 6.0f))
			.BorderBackgroundColor_Lambda([this]()
			{
				if (!Enabled.Get(true))
				{
					return FSlateColor(BaseColor.Desaturate(0.8f) * FLinearColor(0.55f, 0.55f, 0.55f, 1.0f));
				}
				return FSlateColor(bHovered ? BaseColor * 1.25f : BaseColor);
			})
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.0f, 6.0f))
			[
				SNew(STextBlock)
				.Text(InArgs._Text)
				.Font(KGSlate::Font(InArgs._FontSize, true))
				.ColorAndOpacity_Lambda([this, TextColor]()
				{
					return FSlateColor(Enabled.Get(true) ? TextColor : TextColor * FLinearColor(1, 1, 1, 0.5f));
				})
			]
		]
	];
}

FReply SKGTextButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Enabled.Get(true))
	{
		OnClicked.ExecuteIfBound();
	}
	return FReply::Handled();
}

void SKGTextButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	bHovered = true;
}

void SKGTextButton::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bHovered = false;
}

FCursorReply SKGTextButton::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return Enabled.Get(true) ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}
