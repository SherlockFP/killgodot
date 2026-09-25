#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SWidget;
class UGameViewportClient;

/** A full-screen Slate window pushed onto one local player's viewport (see KGSlate::OpenModal). */
struct KILLGODOT_API FKGModalHandle
{
	TSharedPtr<SWidget> Widget;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	TWeakObjectPtr<APlayerController> PlayerController;
	/** Menus run with a visible cursor: restore that instead of forcing game-only input. */
	bool bWasCursorVisible = false;

	bool IsOpen() const { return Widget.IsValid(); }
};

/**
 * Small asset-free Slate kit shared by the inventory window and the Village Market (no UMG/editor assets):
 * rounded brushes, the warm "village" palette, an item tile, a clickable slot and a text button.
 */
namespace KGSlate
{
	/** Adds Widget to PC's viewport, gives it keyboard focus, shows the cursor. False if PC is not local. */
	KILLGODOT_API bool OpenModal(APlayerController* PC, const TSharedRef<SWidget>& Widget, int32 ZOrder,
	                             FKGModalHandle& OutHandle);
	/** Removes the widget and restores game input (or the menu's cursor mode). Safe to call twice. */
	KILLGODOT_API void CloseModal(FKGModalHandle& Handle);

	/** Rounded box brush of the given fill (and optional outline); cached, lives for the whole session. */
	KILLGODOT_API const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius,
	                                         const FLinearColor& Outline = FLinearColor::Transparent,
	                                         float OutlineWidth = 0.0f);
	KILLGODOT_API const FSlateBrush* Solid(const FLinearColor& Fill);
	KILLGODOT_API FSlateFontInfo Font(int32 Size, bool bBold = false);

	namespace Colors
	{
		KILLGODOT_API FLinearColor Backdrop();
		KILLGODOT_API FLinearColor Panel();
		KILLGODOT_API FLinearColor PanelInner();
		KILLGODOT_API FLinearColor Slot();
		KILLGODOT_API FLinearColor SlotHover();
		KILLGODOT_API FLinearColor Gold();
		KILLGODOT_API FLinearColor Cream();
		KILLGODOT_API FLinearColor Muted();
		KILLGODOT_API FLinearColor Good();
		KILLGODOT_API FLinearColor Bad();
	}
}

DECLARE_DELEGATE_TwoParams(FKGOnSlotClicked, int32 /*Slot*/, const FPointerEvent& /*Mouse*/);
DECLARE_DELEGATE_OneParam(FKGOnSlotHovered, int32 /*Slot, INDEX_NONE on leave*/);

/** Coloured rounded tile with a short glyph (item or cosmetic "icon"). */
class KILLGODOT_API SKGGlyphTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGGlyphTile) : _Size(48.0f), _Color(FLinearColor::White), _FontSize(16) {}
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(FString, Glyph)
		SLATE_ARGUMENT(int32, FontSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

/** One inventory slot: tile + count, hover highlight, left/right click. */
class KILLGODOT_API SKGItemSlot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGItemSlot) : _SlotIndex(0), _Count(0), _Size(68.0f) {}
		SLATE_ARGUMENT(int32, SlotIndex)
		SLATE_ARGUMENT(FName, ItemId)
		SLATE_ARGUMENT(int32, Count)
		SLATE_ARGUMENT(float, Size)
		SLATE_EVENT(FKGOnSlotClicked, OnClicked)
		SLATE_EVENT(FKGOnSlotHovered, OnHovered)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	int32 SlotIndex = 0;
	bool bHovered = false;
	FKGOnSlotClicked OnClicked;
	FKGOnSlotHovered OnHovered;
};

/** Rounded text button without style assets. */
class KILLGODOT_API SKGTextButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGTextButton)
		: _Color(FLinearColor(0.45f, 0.32f, 0.2f, 1.0f)), _TextColor(FLinearColor::White), _FontSize(13),
		  _MinWidth(90.0f), _Enabled(true) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(FLinearColor, TextColor)
		SLATE_ARGUMENT(int32, FontSize)
		SLATE_ARGUMENT(float, MinWidth)
		SLATE_ATTRIBUTE(bool, Enabled)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	FLinearColor BaseColor;
	TAttribute<bool> Enabled;
	bool bHovered = false;
	FSimpleDelegate OnClicked;
};
