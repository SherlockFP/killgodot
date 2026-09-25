#include "Inventory/KGInventoryUI.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/KGInventoryComponent.h"
#include "Inventory/KGInventoryRPCComponent.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGSlateWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGInventoryUI"

namespace KGInvUIPrivate
{
	constexpr int32 Columns = 4;
	constexpr float SlotSize = 68.0f;

	struct FWindow
	{
		FKGModalHandle Modal;
		bool bForContainer = false;
	};

	TMap<TWeakObjectPtr<const ULocalPlayer>, FWindow>& Windows()
	{
		static TMap<TWeakObjectPtr<const ULocalPlayer>, FWindow> Map;
		return Map;
	}

	void Prune()
	{
		for (auto It = Windows().CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || !It.Value().Modal.Viewport.IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	void HashEntries(uint32& Hash, const TArray<FKGItemEntry>& Entries)
	{
		for (const FKGItemEntry& Entry : Entries)
		{
			Hash = HashCombine(Hash, HashCombine(GetTypeHash(Entry.Slot),
			                                     HashCombine(GetTypeHash(Entry.ItemId), GetTypeHash(Entry.Count))));
		}
	}
}

/** The window itself. Polls its sources every frame (cheap) instead of juggling delegate lifetimes. */
class SKGInventoryWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGInventoryWindow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PlayerController;
		using namespace KGSlate;

		auto MakePanel = [](const TAttribute<FText>& Title, const TSharedRef<SWidget>& Grid,
		                    const TSharedRef<SWidget>& Extra) -> TSharedRef<SWidget>
		{
			return SNew(SBorder)
				.BorderImage(Rounded(Colors::PanelInner(), 10.0f))
				.Padding(12.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Title).Font(Font(15, true)).ColorAndOpacity(Colors::Cream())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							Extra
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Grid
					]
				];
		};

		SAssignNew(PocketGrid, SUniformGridPanel).SlotPadding(FMargin(3.0f));
		SAssignNew(ContainerGrid, SUniformGridPanel).SlotPadding(FMargin(3.0f));

		TSharedRef<SWidget> LockButton =
			SNew(SKGTextButton)
			.Text(this, &SKGInventoryWindow::GetLockButtonText)
			.Visibility(this, &SKGInventoryWindow::GetLockVisibility)
			.MinWidth(80.0f)
			.FontSize(11)
			.OnClicked(FSimpleDelegate::CreateSP(this, &SKGInventoryWindow::ToggleLock));

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Solid(Colors::Backdrop()))
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Rounded(Colors::Panel(), 14.0f, Colors::Gold() * FLinearColor(1, 1, 1, 0.6f), 2.0f))
				.Padding(20.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "Inventory"))
							.Font(Font(22, true))
							.ColorAndOpacity(Colors::Gold())
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SKGInventoryWindow::GetGoldText)
							.Font(Font(14, true))
							.ColorAndOpacity(Colors::Gold())
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
						[
							MakePanel(LOCTEXT("Pockets", "Your pockets"), PocketGrid.ToSharedRef(), SNullWidget::NullWidget)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(18.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.Visibility(this, &SKGInventoryWindow::GetContainerVisibility)
							[
								MakePanel(MakeAttributeSP(this, &SKGInventoryWindow::GetContainerTitle),
								          ContainerGrid.ToSharedRef(), LockButton)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.MinDesiredHeight(44.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(this, &SKGInventoryWindow::GetInfoName)
								.Font(Font(14, true))
								.ColorAndOpacity(this, &SKGInventoryWindow::GetInfoColor)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(this, &SKGInventoryWindow::GetInfoDescription)
								.Font(Font(12))
								.ColorAndOpacity(Colors::Muted())
								.AutoWrapText(true)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(this, &SKGInventoryWindow::GetHintText)
						.Font(Font(11))
						.ColorAndOpacity(Colors::Muted())
					]
				]
			]
		];
		Rebuild();
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::E || Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::I)
		{
			FKGInventoryUI::Close(PC.Get());
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	/** Swallow clicks on the backdrop so they never reach the game. */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		const uint32 Now = ComputeSignature();
		if (Now != Signature)
		{
			Rebuild();
		}
	}

private:
	TWeakObjectPtr<APlayerController> PC;
	TSharedPtr<SUniformGridPanel> PocketGrid;
	TSharedPtr<SUniformGridPanel> ContainerGrid;
	uint32 Signature = 0;
	int32 HoveredSlot = INDEX_NONE;
	bool bHoveredContainer = false;

	UKGInventoryComponent* GetPockets() const
	{
		return PC.IsValid() ? UKGInventoryComponent::FindForPlayer(PC->PlayerState) : nullptr;
	}

	UKGInventoryRPCComponent* GetRelay() const
	{
		return UKGInventoryRPCComponent::FindForController(PC.Get());
	}

	bool HasContainer() const
	{
		const UKGInventoryRPCComponent* Relay = GetRelay();
		return Relay && Relay->GetViewedActor() != nullptr;
	}

	uint32 ComputeSignature() const
	{
		uint32 Hash = 0x51ED270Bu;
		if (const UKGInventoryComponent* Pockets = GetPockets())
		{
			Hash = HashCombine(Hash, GetTypeHash(Pockets->GetCapacity()));
			KGInvUIPrivate::HashEntries(Hash, Pockets->GetEntries());
		}
		if (const UKGInventoryRPCComponent* Relay = GetRelay())
		{
			Hash = HashCombine(Hash, GetTypeHash(Relay->GetViewedActor()));
			Hash = HashCombine(Hash, GetTypeHash(Relay->GetViewedCapacity()));
			KGInvUIPrivate::HashEntries(Hash, Relay->GetViewedItems().Entries);
		}
		return Hash;
	}

	void Rebuild()
	{
		Signature = ComputeSignature();
		static const TArray<FKGItemEntry> NoEntries;
		const UKGInventoryComponent* Pockets = GetPockets();
		FillGrid(*PocketGrid, Pockets ? Pockets->GetEntries() : NoEntries, Pockets ? Pockets->GetCapacity() : 12, false);
		const UKGInventoryRPCComponent* Relay = GetRelay();
		if (Relay && Relay->GetViewedActor())
		{
			FillGrid(*ContainerGrid, Relay->GetViewedItems().Entries, FMath::Max(1, Relay->GetViewedCapacity()), true);
		}
		else
		{
			ContainerGrid->ClearChildren();
		}
	}

	void FillGrid(SUniformGridPanel& Grid, const TArray<FKGItemEntry>& Entries, int32 Capacity, bool bContainer)
	{
		Grid.ClearChildren();
		for (int32 Slot = 0; Slot < Capacity; ++Slot)
		{
			const FKGItemEntry* Entry = Entries.FindByPredicate([Slot](const FKGItemEntry& E) { return E.Slot == Slot; });
			Grid.AddSlot(Slot % KGInvUIPrivate::Columns, Slot / KGInvUIPrivate::Columns)
			[
				SNew(SKGItemSlot)
				.SlotIndex(Slot)
				.ItemId(Entry ? Entry->ItemId : FName())
				.Count(Entry ? Entry->Count : 0)
				.Size(KGInvUIPrivate::SlotSize)
				.OnClicked(FKGOnSlotClicked::CreateSP(this, &SKGInventoryWindow::HandleSlotClicked, bContainer))
				.OnHovered(FKGOnSlotHovered::CreateSP(this, &SKGInventoryWindow::HandleSlotHovered, bContainer))
			];
		}
	}

	void HandleSlotClicked(int32 Slot, const FPointerEvent& Mouse, bool bContainer)
	{
		UKGInventoryRPCComponent* Relay = GetRelay();
		if (!Relay)
		{
			return;
		}
		const int32 Num = Mouse.GetEffectingButton() == EKeys::RightMouseButton ? 1 : -1;
		if (Mouse.IsControlDown())
		{
			if (!bContainer)
			{
				Relay->RequestDrop(Slot, Num);
			}
			return;
		}
		if (Relay->GetViewedActor())
		{
			Relay->RequestTransfer(bContainer, Slot, Num);
		}
	}

	void HandleSlotHovered(int32 Slot, bool bContainer)
	{
		HoveredSlot = Slot;
		bHoveredContainer = bContainer;
	}

	void ToggleLock()
	{
		if (UKGInventoryRPCComponent* Relay = GetRelay())
		{
			Relay->RequestSetLocked(!Relay->IsViewedLocked());
		}
	}

	const FKGItemDef* GetHoveredDef(int32* OutCount = nullptr, int32* OutGrams = nullptr) const
	{
		if (HoveredSlot == INDEX_NONE)
		{
			return nullptr;
		}
		const FKGItemEntry* Entry = nullptr;
		if (bHoveredContainer)
		{
			const UKGInventoryRPCComponent* Relay = GetRelay();
			Entry = Relay ? Relay->GetViewedItems().FindSlot(HoveredSlot) : nullptr;
		}
		else if (const UKGInventoryComponent* Pockets = GetPockets())
		{
			Entry = Pockets->FindSlot(HoveredSlot);
		}
		if (OutCount)
		{
			*OutCount = Entry ? Entry->Count : 0;
		}
		if (OutGrams)
		{
			*OutGrams = Entry ? Entry->Grams : 0;
		}
		return Entry ? UKGItemCatalog::Find(Entry->ItemId) : nullptr;
	}

	FText GetGoldText() const
	{
		const UKGInventoryComponent* Pockets = GetPockets();
		return FText::Format(LOCTEXT("GoldLine", "Gold coins: {0}"), FText::AsNumber(Pockets ? Pockets->GetGold() : 0));
	}

	FText GetContainerTitle() const
	{
		const UKGInventoryRPCComponent* Relay = GetRelay();
		if (!Relay)
		{
			return FText::GetEmpty();
		}
		return Relay->IsViewedLocked()
			       ? FText::Format(LOCTEXT("TitleLocked", "{0} (locked)"), Relay->GetViewedTitle())
			       : Relay->GetViewedTitle();
	}

	FText GetLockButtonText() const
	{
		const UKGInventoryRPCComponent* Relay = GetRelay();
		return Relay && Relay->IsViewedLocked() ? LOCTEXT("Unlock", "Unlock") : LOCTEXT("Lock", "Lock");
	}

	EVisibility GetLockVisibility() const
	{
		const UKGInventoryRPCComponent* Relay = GetRelay();
		return Relay && Relay->GetViewedActor() && Relay->CanToggleViewedLock() ? EVisibility::Visible
		                                                                        : EVisibility::Collapsed;
	}

	EVisibility GetContainerVisibility() const
	{
		return HasContainer() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FText GetInfoName() const
	{
		int32 Count = 0;
		int32 Grams = 0;
		const FKGItemDef* Def = GetHoveredDef(&Count, &Grams);
		if (!Def)
		{
			return LOCTEXT("InfoNone", " ");
		}
		const FText Line = FText::Format(LOCTEXT("InfoName", "{0}  x{1}   -   {2}   -   worth {3} gold each"), Def->DisplayName,
		                                 FText::AsNumber(Count), UKGItemCatalog::GetRarityName(Def->Rarity),
		                                 FText::AsNumber(Def->GoldValue));
		if (Grams <= 0)
		{
			return Line;
		}
		// Caught fish carry their weight (Fishing): "Salmon x2 ... - 7.40 kg"
		FNumberFormattingOptions Kg;
		Kg.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("InfoNameWeight", "{0}   -   {1} kg"), Line, FText::AsNumber(Grams / 1000.0, &Kg));
	}

	FSlateColor GetInfoColor() const
	{
		const FKGItemDef* Def = GetHoveredDef();
		return FSlateColor(Def && Def->Rarity != EKGRarity::Common ? UKGItemCatalog::GetRarityColor(Def->Rarity)
		                                                           : KGSlate::Colors::Cream());
	}

	FText GetInfoDescription() const
	{
		const FKGItemDef* Def = GetHoveredDef();
		return Def ? Def->Description : FText::GetEmpty();
	}

	FText GetHintText() const
	{
		return HasContainer()
			       ? LOCTEXT("HintContainer",
			                 "Click: move stack    Right-click: move one    Ctrl+click: drop on the ground    E / Esc: close")
			       : LOCTEXT("HintPockets", "Ctrl+click: drop stack    Ctrl+right-click: drop one    E / Esc / Tab: close");
	}
};

void FKGInventoryUI::Open(APlayerController* PC)
{
	KGInvUIPrivate::Prune();
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || IsOpen(PC))
	{
		return;
	}
	TSharedRef<SKGInventoryWindow> Window = SNew(SKGInventoryWindow).PlayerController(PC);
	KGInvUIPrivate::FWindow Entry;
	if (KGSlate::OpenModal(PC, Window, 60, Entry.Modal))
	{
		KGInvUIPrivate::Windows().Add(PC->GetLocalPlayer(), Entry);
	}
}

void FKGInventoryUI::Close(APlayerController* PC, bool bTellServer)
{
	if (!PC)
	{
		return;
	}
	KGInvUIPrivate::FWindow Entry;
	if (KGInvUIPrivate::Windows().RemoveAndCopyValue(PC->GetLocalPlayer(), Entry))
	{
		KGSlate::CloseModal(Entry.Modal);
	}
	if (bTellServer)
	{
		UKGInventoryRPCComponent* Relay = UKGInventoryRPCComponent::FindForController(PC);
		if (Relay && Relay->GetViewedActor())
		{
			Relay->RequestClose();
		}
	}
}

void FKGInventoryUI::Toggle(APlayerController* PC)
{
	if (IsOpen(PC))
	{
		Close(PC);
	}
	else
	{
		Open(PC);
	}
}

bool FKGInventoryUI::IsOpen(const APlayerController* PC)
{
	const KGInvUIPrivate::FWindow* Entry = PC ? KGInvUIPrivate::Windows().Find(PC->GetLocalPlayer()) : nullptr;
	return Entry && Entry->Modal.IsOpen();
}

void FKGInventoryUI::HandleViewChanged(UKGInventoryRPCComponent* Relay)
{
	const APlayerState* PS = Relay ? Cast<APlayerState>(Relay->GetOwner()) : nullptr;
	APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
	{
		return;
	}
	if (Relay->GetViewedActor())
	{
		if (!IsOpen(PC))
		{
			Open(PC);
		}
		if (KGInvUIPrivate::FWindow* Entry = KGInvUIPrivate::Windows().Find(PC->GetLocalPlayer()))
		{
			Entry->bForContainer = true;
		}
	}
	else if (const KGInvUIPrivate::FWindow* Entry = KGInvUIPrivate::Windows().Find(PC->GetLocalPlayer()))
	{
		if (Entry->bForContainer)
		{
			Close(PC, false);   // the server already closed the view
		}
	}
}

#undef LOCTEXT_NAMESPACE
