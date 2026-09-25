#include "Cosmetics/KGCosmeticShop.h"
#include "Cosmetics/KGCosmeticCatalog.h"
#include "Cosmetics/KGCosmeticsComponent.h"
#include "Cosmetics/KGProfileSave.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/KGSlateWidgets.h"
#include "KillGodot.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KGShop"

namespace KGShopPrivate
{
	constexpr int32 Columns = 4;

	TMap<TWeakObjectPtr<const ULocalPlayer>, FKGModalHandle>& Windows()
	{
		static TMap<TWeakObjectPtr<const ULocalPlayer>, FKGModalHandle> Map;
		return Map;
	}

	FLinearColor BuyColor() { return FLinearColor::FromSRGBColor(FColor(196, 146, 28)); }
	FLinearColor EquipColor() { return FLinearColor::FromSRGBColor(FColor(64, 140, 74)); }
	FLinearColor UnequipColor() { return FLinearColor::FromSRGBColor(FColor(110, 84, 62)); }
	FLinearColor TabColor() { return FLinearColor::FromSRGBColor(FColor(82, 60, 44)); }
	FLinearColor TabSelectedColor() { return FLinearColor::FromSRGBColor(FColor(196, 146, 28)); }
}

class SKGCosmeticShop : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKGCosmeticShop) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, PlayerController)
	SLATE_END_ARGS()

	virtual ~SKGCosmeticShop() override
	{
		UKGProfileSave::OnProfileChanged().Remove(ProfileHandle);
	}

	void Construct(const FArguments& InArgs)
	{
		using namespace KGSlate;
		PC = InArgs._PlayerController;
		MessageColor = Colors::Muted();
		Message = LOCTEXT("Welcome", "Welcome to the market! Gold is earned by playing - banked match coins, rewards, the Almanac.");
		ProfileHandle = UKGProfileSave::OnProfileChanged().AddSP(this, &SKGCosmeticShop::Rebuild);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Solid(Colors::Backdrop()))
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(1160.0f)
				.HeightOverride(720.0f)
				[
					SNew(SBorder)
					.BorderImage(Rounded(Colors::Panel(), 16.0f, Colors::Gold() * FLinearColor(1, 1, 1, 0.7f), 2.0f))
					.Padding(22.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ShopTitle", "Village Market"))
									.Font(Font(28, true))
									.ColorAndOpacity(Colors::Gold())
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ShopSubtitle",
									              "Hats, hoods and oddities from Morrowmere's stalls. Paid only with gold you earn by playing: no real money, no random boxes."))
									.Font(Font(12))
									.ColorAndOpacity(Colors::Muted())
									.AutoWrapText(true)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f)
							[
								SNew(SBorder)
								.BorderImage(Rounded(Colors::PanelInner(), 18.0f, Colors::Gold() * FLinearColor(1, 1, 1, 0.8f), 1.5f))
								.Padding(FMargin(16.0f, 8.0f))
								[
									SNew(STextBlock)
									.Text(this, &SKGCosmeticShop::GetGoldText)
									.Font(Font(18, true))
									.ColorAndOpacity(Colors::Gold())
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SKGTextButton)
								.Text(LOCTEXT("Close", "Close"))
								.OnClicked(FSimpleDelegate::CreateSP(this, &SKGCosmeticShop::Close))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 10.0f)
						[
							SAssignNew(Tabs, SHorizontalBox)
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(Grid, SUniformGridPanel).SlotPadding(FMargin(6.0f))
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f, 0.0f, 0.0f)
							[
								SAssignNew(Preview, SBox).WidthOverride(310.0f)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return Message; })
								.ColorAndOpacity_Lambda([this]() { return FSlateColor(MessageColor); })
								.Font(Font(13, true))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ShopHint", "Esc: close"))
								.Font(Font(11))
								.ColorAndOpacity(Colors::Muted())
							]
						]
					]
				]
			]
		];
		Rebuild();
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			Close();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

private:
	TWeakObjectPtr<APlayerController> PC;
	int32 Filter = INDEX_NONE;
	FName Selected;
	FText Message;
	FLinearColor MessageColor;
	TSharedPtr<SHorizontalBox> Tabs;
	TSharedPtr<SUniformGridPanel> Grid;
	TSharedPtr<SBox> Preview;
	FDelegateHandle ProfileHandle;

	static UKGProfileSave* Profile() { return UKGProfileSave::GetProfile(); }

	void Close()
	{
		UKGCosmeticShop::CloseShop(PC.Get());
	}

	FText GetGoldText() const
	{
		return FText::Format(LOCTEXT("GoldPill", "{0} gold"), FText::AsNumber(Profile()->GetGold()));
	}

	bool PassesFilter(const FKGCosmeticDef& Def) const
	{
		return Filter == INDEX_NONE || static_cast<int32>(Def.Slot) == Filter;
	}

	void SetMessage(const FText& Text, const FLinearColor& Color)
	{
		Message = Text;
		MessageColor = Color;
	}

	void Rebuild()
	{
		if (!UKGCosmeticCatalog::Find(Selected) || !PassesFilter(*UKGCosmeticCatalog::Find(Selected)))
		{
			Selected = NAME_None;
			for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
			{
				if (PassesFilter(Def))
				{
					Selected = Def.Id;
					break;
				}
			}
		}
		RebuildTabs();
		RebuildGrid();
		RebuildPreview();
	}

	void RebuildTabs()
	{
		Tabs->ClearChildren();
		auto AddTab = [this](int32 SlotFilter, const FText& Label)
		{
			const bool bSelected = Filter == SlotFilter;
			Tabs->AddSlot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SKGTextButton)
				.Text(Label)
				.Color(bSelected ? KGShopPrivate::TabSelectedColor() : KGShopPrivate::TabColor())
				.TextColor(bSelected ? FLinearColor(0.08f, 0.06f, 0.04f, 1.0f) : KGSlate::Colors::Cream())
				.OnClicked(FSimpleDelegate::CreateSP(this, &SKGCosmeticShop::SetFilter, SlotFilter))
			];
		};
		AddTab(INDEX_NONE, LOCTEXT("TabAll", "All"));
		for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EKGCosmeticSlot::Count); ++SlotIndex)
		{
			AddTab(SlotIndex, UKGCosmeticCatalog::GetSlotName(static_cast<EKGCosmeticSlot>(SlotIndex)));
		}
	}

	void SetFilter(int32 SlotFilter)
	{
		Filter = SlotFilter;
		Rebuild();
	}

	void RebuildGrid()
	{
		Grid->ClearChildren();
		int32 Index = 0;
		for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
		{
			if (!PassesFilter(Def))
			{
				continue;
			}
			Grid->AddSlot(Index % KGShopPrivate::Columns, Index / KGShopPrivate::Columns)
			[
				MakeCard(Def)
			];
			++Index;
		}
	}

	FText StatusText(const FKGCosmeticDef& Def) const
	{
		if (Profile()->IsEquipped(Def.Id))
		{
			return LOCTEXT("StatusEquipped", "Equipped");
		}
		if (Profile()->Owns(Def.Id))
		{
			return LOCTEXT("StatusOwned", "Owned");
		}
		return Def.Price <= 0 ? LOCTEXT("StatusFree", "Free")
		                      : FText::Format(LOCTEXT("StatusPrice", "{0} gold"), FText::AsNumber(Def.Price));
	}

	FLinearColor StatusColor(const FKGCosmeticDef& Def) const
	{
		if (Profile()->Owns(Def.Id))
		{
			return KGSlate::Colors::Good();
		}
		return Profile()->GetGold() >= Def.Price ? KGSlate::Colors::Gold() : KGSlate::Colors::Bad();
	}

	TSharedRef<SWidget> MakeActionButton(const FKGCosmeticDef& Def, int32 FontSize, float MinWidth)
	{
		const FName Id = Def.Id;
		if (!Profile()->Owns(Id))
		{
			const bool bAfford = Profile()->GetGold() >= Def.Price;
			return SNew(SKGTextButton)
				.Text(Def.Price <= 0 ? LOCTEXT("TakeFree", "Take (free)")
				                     : FText::Format(LOCTEXT("BuyFor", "Buy - {0}"), FText::AsNumber(Def.Price)))
				.Color(KGShopPrivate::BuyColor())
				.TextColor(FLinearColor(0.08f, 0.06f, 0.04f, 1.0f))
				.FontSize(FontSize)
				.MinWidth(MinWidth)
				.Enabled(bAfford)
				.OnClicked(FSimpleDelegate::CreateSP(this, &SKGCosmeticShop::DoBuy, Id));
		}
		if (Profile()->IsEquipped(Id))
		{
			return SNew(SKGTextButton)
				.Text(LOCTEXT("Unequip", "Unequip"))
				.Color(KGShopPrivate::UnequipColor())
				.FontSize(FontSize)
				.MinWidth(MinWidth)
				.OnClicked(FSimpleDelegate::CreateSP(this, &SKGCosmeticShop::DoUnequip, Id));
		}
		return SNew(SKGTextButton)
			.Text(LOCTEXT("Equip", "Equip"))
			.Color(KGShopPrivate::EquipColor())
			.FontSize(FontSize)
			.MinWidth(MinWidth)
			.OnClicked(FSimpleDelegate::CreateSP(this, &SKGCosmeticShop::DoEquip, Id));
	}

	TSharedRef<SWidget> MakeCard(const FKGCosmeticDef& Def)
	{
		using namespace KGSlate;
		const FName Id = Def.Id;
		const FLinearColor Rarity = UKGItemCatalog::GetRarityColor(Def.Rarity);
		const bool bSelected = Selected == Id;
		return SNew(SBox)
			.WidthOverride(180.0f)
			.HeightOverride(190.0f)
			[
				SNew(SBorder)
				.BorderImage(Rounded(bSelected ? Colors::SlotHover() : Colors::PanelInner(), 10.0f, Rarity,
				                     bSelected ? 3.0f : 1.5f))
				.Padding(10.0f)
				.OnMouseButtonDown_Lambda([this, Id](const FGeometry&, const FPointerEvent&)
				{
					Selected = Id;
					Rebuild();
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Text(UKGItemCatalog::GetRarityName(Def.Rarity))
							.Font(Font(10, true))
							.ColorAndOpacity(Rarity)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(UKGCosmeticCatalog::GetSlotName(Def.Slot))
							.Font(Font(10))
							.ColorAndOpacity(Colors::Muted())
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 6.0f)
					[
						SNew(SKGGlyphTile).Size(60.0f).Color(Def.IconColor).Glyph(Def.Glyph).FontSize(20)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(Def.DisplayName)
						.Font(Font(13, true))
						.ColorAndOpacity(Colors::Cream())
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Text(StatusText(Def))
						.Font(Font(12, true))
						.ColorAndOpacity(StatusColor(Def))
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Bottom).HAlign(HAlign_Center)
					[
						MakeActionButton(Def, 12, 140.0f)
					]
				]
			];
	}

	void RebuildPreview()
	{
		using namespace KGSlate;
		const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Selected);
		if (!Def)
		{
			Preview->SetContent(SNullWidget::NullWidget);
			return;
		}
		const FLinearColor Rarity = UKGItemCatalog::GetRarityColor(Def->Rarity);
		const FText SlotName = UKGCosmeticCatalog::GetSlotName(Def->Slot);
		const FName Worn = Profile()->GetEquipped(Def->Slot);
		const FKGCosmeticDef* WornDef = UKGCosmeticCatalog::Find(Worn);
		const FText Wearing = WornDef ? FText::Format(LOCTEXT("PreviewWearing", "Currently on your {0}: {1}"), SlotName,
		                                              WornDef->DisplayName)
		                              : FText::Format(LOCTEXT("PreviewEmpty", "Nothing on your {0} yet."), SlotName);
		Preview->SetContent(
			SNew(SBorder)
			.BorderImage(Rounded(Colors::PanelInner(), 12.0f, Rarity, 2.0f))
			.Padding(16.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("PreviewLabel", "PREVIEW")).Font(Font(10, true)).ColorAndOpacity(Colors::Muted())
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
				[
					SNew(SKGGlyphTile).Size(120.0f).Color(Def->IconColor).Glyph(Def->Glyph).FontSize(34)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Def->DisplayName).Font(Font(20, true)).ColorAndOpacity(Rarity).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("PreviewKind", "{0} - worn on the {1}"),
					                    UKGItemCatalog::GetRarityName(Def->Rarity), SlotName))
					.Font(Font(12))
					.ColorAndOpacity(Colors::Muted())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
				[
					SNew(STextBlock).Text(Def->Description).Font(Font(13)).ColorAndOpacity(Colors::Cream()).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PreviewNote",
					              "Everyone in the village sees it on you; your own first-person view stays clear."))
					.Font(Font(11))
					.ColorAndOpacity(Colors::Muted())
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Wearing).Font(Font(11, true)).ColorAndOpacity(Colors::Muted()).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SSpacer)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(StatusText(*Def)).Font(Font(16, true)).ColorAndOpacity(StatusColor(*Def))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					MakeActionButton(*Def, 15, 250.0f)
				]
			]);
	}

	void DoBuy(FName Id)
	{
		const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id);
		if (!Def)
		{
			return;
		}
		Selected = Id;
		switch (Profile()->Buy(Id))
		{
		case EKGBuyResult::Bought:
			SetMessage(FText::Format(LOCTEXT("Bought", "Bought {0}! Press Equip to wear it."), Def->DisplayName),
			           KGSlate::Colors::Good());
			break;
		case EKGBuyResult::NotEnoughGold:
			SetMessage(FText::Format(LOCTEXT("NoGold", "Not enough gold: {0} needed, you have {1}. Play a few matches!"),
			                         FText::AsNumber(Def->Price), FText::AsNumber(Profile()->GetGold())),
			           KGSlate::Colors::Bad());
			break;
		case EKGBuyResult::AlreadyOwned:
			SetMessage(LOCTEXT("Owned", "You already own that."), KGSlate::Colors::Muted());
			break;
		default:
			break;
		}
		Rebuild();
	}

	void DoEquip(FName Id)
	{
		const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id);
		if (Def && Profile()->Equip(Id))
		{
			Selected = Id;
			SetMessage(FText::Format(LOCTEXT("Equipped", "Now wearing {0}."), Def->DisplayName), KGSlate::Colors::Good());
		}
		Rebuild();
	}

	void DoUnequip(FName Id)
	{
		if (const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id))
		{
			Profile()->Unequip(Def->Slot);
			SetMessage(FText::Format(LOCTEXT("Unequipped", "Took off {0}."), Def->DisplayName), KGSlate::Colors::Muted());
		}
		Rebuild();
	}
};

void UKGCosmeticShop::OpenShop(APlayerController* PC)
{
	if (!PC && GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
	{
		PC = GEngine->GameViewport->GetWorld()->GetFirstPlayerController();
	}
	if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || IsShopOpen(PC))
	{
		return;
	}
	for (auto It = KGShopPrivate::Windows().CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().Viewport.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	UKGProfileSave::GetProfile();   // load (and create on first launch) before the UI reads it
	TSharedRef<SKGCosmeticShop> Shop = SNew(SKGCosmeticShop).PlayerController(PC);
	FKGModalHandle Handle;
	if (KGSlate::OpenModal(PC, Shop, 200, Handle))
	{
		KGShopPrivate::Windows().Add(PC->GetLocalPlayer(), Handle);
	}
}

void UKGCosmeticShop::CloseShop(APlayerController* PC)
{
	FKGModalHandle Handle;
	if (PC && KGShopPrivate::Windows().RemoveAndCopyValue(PC->GetLocalPlayer(), Handle))
	{
		KGSlate::CloseModal(Handle);
	}
}

bool UKGCosmeticShop::IsShopOpen(const APlayerController* PC)
{
	const FKGModalHandle* Handle = PC ? KGShopPrivate::Windows().Find(PC->GetLocalPlayer()) : nullptr;
	return Handle && Handle->IsOpen();
}

// ---------------------------------------------------------------------------------------------------------------
// Dev console commands (not in Shipping).
// ---------------------------------------------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
namespace KGShopConsole
{
	using FWorldArgs = FConsoleCommandWithWorldAndArgsDelegate;

	void RefreshAllCosmetics()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const UWorld* World = Context.World();
			const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
			if (!GS)
			{
				continue;
			}
			for (const APlayerState* PS : GS->PlayerArray)
			{
				if (UKGCosmeticsComponent* Cosmetics = UKGCosmeticsComponent::FindForPlayer(PS))
				{
					Cosmetics->RefreshVisuals();
				}
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs OpenShop(
		TEXT("kg.OpenShop"), TEXT("Open the Village Market (cosmetics shop)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGCosmeticShop::OpenShop(World ? World->GetFirstPlayerController() : nullptr);
		}));

	FAutoConsoleCommandWithWorldAndArgs GiveGold(
		TEXT("kg.GiveGold"), TEXT("kg.GiveGold <Amount> : add earned gold to the local profile (dev)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGProfileSave::GetProfile()->AwardGold(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100, TEXT("DevCommand"));
		}));

	FAutoConsoleCommandWithWorldAndArgs BankCoins(
		TEXT("kg.BankCoins"), TEXT("Convert the Coin items in your pockets into profile gold (end-of-match hook)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Banked = UKGProfileSave::BankMatchCoins(World ? World->GetFirstPlayerController() : nullptr);
			UE_LOG(LogKillGodot, Log, TEXT("kg.BankCoins -> %d gold"), Banked);
		}));

	FAutoConsoleCommandWithWorldAndArgs ResetProfile(
		TEXT("kg.ResetProfile"), TEXT("Delete the local cosmetics profile (starter gold + items again)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGProfileSave::ResetProfile();
		}));

	FAutoConsoleCommandWithWorldAndArgs ListCosmetics(
		TEXT("kg.ListCosmetics"), TEXT("Log the cosmetic catalog and the local profile"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UKGProfileSave* Profile = UKGProfileSave::GetProfile();
			UE_LOG(LogKillGodot, Log, TEXT("Profile gold: %d"), Profile->GetGold());
			for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
			{
				UE_LOG(LogKillGodot, Log, TEXT("  %-16s %-20s %-9s %-10s %5d %s%s"), *Def.Id.ToString(),
				       *Def.DisplayName.ToString(), *UKGCosmeticCatalog::GetSlotName(Def.Slot).ToString(),
				       *UKGItemCatalog::GetRarityName(Def.Rarity).ToString(), Def.Price,
				       Profile->Owns(Def.Id) ? TEXT("owned") : TEXT(""), Profile->IsEquipped(Def.Id) ? TEXT(" EQUIPPED") : TEXT(""));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Unlock(
		TEXT("kg.UnlockCosmetic"), TEXT("kg.UnlockCosmetic <Id|all> : own a cosmetic without paying (dev reward path)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGProfileSave* Profile = UKGProfileSave::GetProfile();
			if (Args.Num() > 0 && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
			{
				for (const FKGCosmeticDef& Def : UKGCosmeticCatalog::GetAll())
				{
					Profile->GrantCosmetic(Def.Id, TEXT("DevCommand"));
				}
				return;
			}
			const FName Id = Args.Num() > 0 ? UKGCosmeticCatalog::ResolveLoose(Args[0]) : NAME_None;
			Profile->GrantCosmetic(Id, TEXT("DevCommand"));
		}));

	FAutoConsoleCommandWithWorldAndArgs Equip(
		TEXT("kg.Equip"), TEXT("kg.Equip <Id> : wear an owned cosmetic (kg.UnlockCosmetic first)"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName Id = Args.Num() > 0 ? UKGCosmeticCatalog::ResolveLoose(Args[0]) : NAME_None;
			if (!UKGProfileSave::GetProfile()->Equip(Id))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("kg.Equip: '%s' unknown or not owned"), Args.Num() > 0 ? *Args[0] : TEXT(""));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Unequip(
		TEXT("kg.Unequip"), TEXT("kg.Unequip <Head|Face|Shoulders|Back|Belt|all>"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UKGProfileSave* Profile = UKGProfileSave::GetProfile();
			const FString Which = Args.Num() > 0 ? Args[0] : TEXT("all");
			for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EKGCosmeticSlot::Count); ++SlotIndex)
			{
				const EKGCosmeticSlot Slot = static_cast<EKGCosmeticSlot>(SlotIndex);
				if (Which.Equals(TEXT("all"), ESearchCase::IgnoreCase) ||
				    Which.Equals(UKGCosmeticCatalog::GetSlotName(Slot).ToString(), ESearchCase::IgnoreCase))
				{
					Profile->Unequip(Slot);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs Nudge(
		TEXT("kg.CosmeticNudge"),
		TEXT("kg.CosmeticNudge <Id> <X> <Y> <Z> [Pitch Yaw Roll] [Scale] | <Id> reset : live-tune a placement in villager component space"),
		FWorldArgs::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName Id = Args.Num() > 0 ? UKGCosmeticCatalog::ResolveLoose(Args[0]) : NAME_None;
			const FKGCosmeticDef* Def = UKGCosmeticCatalog::Find(Id);
			if (!Def)
			{
				return;
			}
			if (Args.Num() >= 2 && Args[1].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
			{
				UKGCosmeticsComponent::DevPlacementOverrides().Remove(Id);
			}
			else if (Args.Num() >= 4)
			{
				auto F = [&Args](int32 i, float Default) { return Args.IsValidIndex(i) ? FCString::Atof(*Args[i]) : Default; };
				const FRotator Base = Def->Placement.Rotator();
				const FTransform Placement(FRotator(F(4, Base.Pitch), F(5, Base.Yaw), F(6, Base.Roll)),
				                           FVector(F(1, 0.0f), F(2, 0.0f), F(3, 0.0f)),
				                           FVector(F(7, Def->Placement.GetScale3D().X)));
				UKGCosmeticsComponent::DevPlacementOverrides().Add(Id, Placement);
				UE_LOG(LogKillGodot, Log, TEXT("kg.CosmeticNudge %s -> Place(FVector(%.1ff, %.1ff, %.1ff), FRotator(%.1ff, %.1ff, %.1ff), %.2ff)"),
				       *Id.ToString(), Placement.GetLocation().X, Placement.GetLocation().Y, Placement.GetLocation().Z,
				       Placement.Rotator().Pitch, Placement.Rotator().Yaw, Placement.Rotator().Roll,
				       Placement.GetScale3D().X);
			}
			RefreshAllCosmetics();
		}));
}
#endif

#undef LOCTEXT_NAMESPACE
