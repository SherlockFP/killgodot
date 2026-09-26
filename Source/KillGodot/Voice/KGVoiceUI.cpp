#include "Voice/KGVoiceUI.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/KGCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/KGGameUserSettings.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/Menu/KGMenuStyle.h"
#include "Voice/KGMouthComponent.h"
#include "Voice/KGVoiceCommands.h"
#include "Voice/KGVoiceComponent.h"
#include "Voice/KGVoiceRules.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "KGVoiceUI"

namespace KGVoiceUIPrivate
{
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	const FLinearColor Cream = Srgb(255, 244, 222);
	const FLinearColor Muted = Srgb(196, 182, 164);
	const FLinearColor Gold = Srgb(255, 184, 77);
	const FLinearColor InkTop = Srgb(40, 31, 58);
	const FLinearColor InkBottom = Srgb(19, 14, 29);
	const FLinearColor Ghost = Srgb(79, 209, 197);
	const FLinearColor Crimson = Srgb(255, 72, 94);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
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

	/** Text on a dark pill. Returns the pill size. */
	FVector2D DrawPill(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FSlateFontInfo& Info,
	                   const FVector2D& Centre, const FLinearColor& TextColor, float Alpha, const FLinearColor& Outline = FLinearColor::Transparent)
	{
		const FVector2D Size = MeasureText(Text, Info) + FVector2D(22.0, 10.0);
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FVector2f(Centre - Size * 0.5), FVector2f(Size), WithAlpha(InkBottom, 0.82f * Alpha),
		                             -1.0f, WithAlpha(Outline, Alpha), Outline.A > 0.0f ? 1.2f : 0.0f);
		DrawLabel(Out, Layer + 1, Geo, Text, Info, Centre, WithAlpha(TextColor, Alpha));
		return Size;
	}

	/** Speaker glyph: a dot with two sound arcs to the right, pulsing with Level. */
	void DrawSpeaker(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float Size, float Level,
	                 const FLinearColor& Col, float Alpha)
	{
		const float R = Size * (0.32f + 0.10f * Level);
		FKGMenuStyle::DrawRoundedBox(Out, Layer, Geo, FVector2f(C - FVector2D(R + 3.0f)), FVector2f((R + 3.0f) * 2.0f), WithAlpha(InkBottom, 0.75f * Alpha), -1.0f);
		FKGMenuStyle::DrawRoundedBox(Out, Layer + 1, Geo, FVector2f(C - FVector2D(R)), FVector2f(R * 2.0f), WithAlpha(Col, Alpha), -1.0f);
		for (int32 Arc = 0; Arc < 2; ++Arc)
		{
			const float AR = Size * (0.62f + 0.28f * Arc);
			const float A = Alpha * (Arc == 0 ? 0.95f : (0.35f + 0.6f * Level));
			TArray<FVector2f> Pts;
			for (int32 i = 0; i <= 8; ++i)
			{
				const float Ang = FMath::DegreesToRadians(-42.0f + 84.0f * i / 8.0f);
				Pts.Add(FVector2f(C + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * AR));
			}
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, WithAlpha(Col, A), true, 2.0f);
		}
	}

	struct FMarker
	{
		FVector2D Pos = FVector2D::ZeroVector;
		float Size = 24.0f;
		float Level = 0.0f;
		bool bGhost = false;
		FString Prompt;
	};
}

using namespace KGVoiceUIPrivate;

// =================================================================================================================

class SKGVoiceLayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SKGVoiceLayer) {}
	SLATE_ARGUMENT(APlayerController*, PlayerController)
	SLATE_END_ARGS()

	static constexpr float DeadZone = 26.0f;
	static constexpr float CursorMax = 150.0f;
	static constexpr float SlotRadius = 150.0f;

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PlayerController;
		SetVisibility(EVisibility::HitTestInvisible);
	}

	// ---- wheel state ----
	void Open(int32 InMenu)
	{
		Menu = FMath::Clamp(InMenu, 0, FKGVoiceCommandCatalog::MenuCount - 1);
		bOpen = true;
		Cursor = FVector2D::ZeroVector;
		Selected = INDEX_NONE;
	}
	void Close() { bOpen = false; }
	bool IsOpen() const { return bOpen; }
	int32 GetMenu() const { return Menu; }
	int32 GetSelected() const { return Selected; }
	void AddDelta(const FVector2D& Delta)
	{
		Cursor += Delta;
		if (Cursor.Size() > CursorMax)
		{
			Cursor *= CursorMax / Cursor.Size();
		}
		if (Cursor.Size() <= DeadZone)
		{
			return;
		}
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(Cursor.Y, Cursor.X)) + 90.0f;
		const float D = FMath::Fmod(Deg + 360.0f + 22.5f, 360.0f);
		Selected = FMath::Clamp(static_cast<int32>(D / 45.0f), 0, 7);
	}
	void Select(int32 Slot)
	{
		Selected = FMath::Clamp(Slot, 0, 7);
		Cursor = FVector2D::ZeroVector;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.0f, 100.0f); }

	virtual void Tick(const FGeometry& Geo, const double InCurrentTime, const float InDeltaTime) override
	{
		SLeafWidget::Tick(Geo, InCurrentTime, InDeltaTime);
		OpenAlpha = FKGMenuStyle::Approach(OpenAlpha, bOpen ? 1.0f : 0.0f, InDeltaTime, 16.0f);
		for (int32 i = 0; i < 8; ++i)
		{
			SlotGrow[i] = FKGMenuStyle::Approach(SlotGrow[i], i == Selected ? 1.0f : 0.0f, InDeltaTime, 18.0f);
		}
		UpdateMarkers(Geo);
		UpdateSelf(InDeltaTime);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& CullingRect,
	                      FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style,
	                      bool bParentEnabled) const override
	{
		const FVector2D Local(Geo.GetLocalSize());
		// ---- markers over heads ----
		for (const FMarker& M : Markers)
		{
			if (M.Level >= 0.0f)
			{
				DrawSpeaker(Out, LayerId, Geo, M.Pos, M.Size, FMath::Clamp(M.Level, 0.0f, 1.0f), M.bGhost ? Ghost : Gold, 0.95f);
			}
			if (!M.Prompt.IsEmpty())
			{
				DrawPill(Out, LayerId + 2, Geo, M.Prompt, Font(12, true), M.Pos + FVector2D(0.0f, M.Size * 1.1f), Gold, 0.95f, Gold * 0.6f);
			}
		}
		// ---- own state, bottom centre ----
		if (!SelfLine.IsEmpty())
		{
			const FVector2D C(Local.X * 0.5, Local.Y * 0.80);
			const FVector2D Size = DrawPill(Out, LayerId + 3, Geo, SelfLine, Font(12, true), C, SelfColor, SelfAlpha, SelfColor * 0.5f);
			if (SelfLevel >= 0.0f)
			{
				const float W = Size.X - 24.0f;
				FKGMenuStyle::DrawRoundedBox(Out, LayerId + 5, Geo, FVector2f(C + FVector2D(-W * 0.5, Size.Y * 0.5 + 4.0)), FVector2f(W, 4.0f),
				                             WithAlpha(InkTop, SelfAlpha), 2.0f);
				FKGMenuStyle::DrawRoundedBox(Out, LayerId + 6, Geo, FVector2f(C + FVector2D(-W * 0.5, Size.Y * 0.5 + 4.0)),
				                             FVector2f(FMath::Max(2.0f, W * SelfLevel), 4.0f), WithAlpha(SelfColor, SelfAlpha), 2.0f);
			}
		}
		if (!PartnerLine.IsEmpty())
		{
			DrawPill(Out, LayerId + 3, Geo, PartnerLine, Font(13, true), FVector2D(Local.X * 0.5, Local.Y * 0.74), Cream, 0.95f, Gold * 0.6f);
		}
		// ---- the radial ----
		if (OpenAlpha <= 0.01f)
		{
			return LayerId + 8;
		}
		const float A = OpenAlpha;
		const float K = 0.75f + 0.25f * FKGMenuStyle::EaseOutBack(OpenAlpha);
		const FVector2D C = Local * 0.5;
		const TArray<FKGVoiceMenuDef>& Menus = FKGVoiceCommandCatalog::GetMenus();
		const FKGVoiceMenuDef& Def = Menus[FMath::Clamp(Menu, 0, Menus.Num() - 1)];
		const float Disc = 430.0f * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 8, Geo, FVector2f(C - FVector2D(Disc * 0.5f)), FVector2f(Disc, Disc),
		                             WithAlpha(InkBottom, 0.72f * A), -1.0f, WithAlpha(Cream, 0.14f * A), 1.5f);
		const float Inner = 112.0f * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 9, Geo, FVector2f(C - FVector2D(Inner * 0.5f)), FVector2f(Inner, Inner),
		                             WithAlpha(InkTop, 0.9f * A), -1.0f, WithAlpha(Gold, 0.35f * A), 1.5f);
		const FSlateFontInfo SlotFont = Font(12, true);
		for (int32 i = 0; i < FKGVoiceCommandCatalog::SlotsPerMenu && i < Def.Commands.Num(); ++i)
		{
			const FKGVoiceCommandDef* Cmd = FKGVoiceCommandCatalog::Get(Def.Commands[i]);
			if (!Cmd)
			{
				continue;
			}
			const float Ang = FMath::DegreesToRadians(-90.0f + 45.0f * i);
			const FVector2D P = C + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * SlotRadius * K;
			const float G = SlotGrow[i];
			const FString Text = Cmd->Line.ToString().Replace(TEXT("{place}"), TEXT("..."));
			const FVector2D TextSize = MeasureText(Text, SlotFont);
			const FVector2D SlotSize = (TextSize + FVector2D(28.0, 16.0)) * (1.0f + 0.1f * G) * K;
			FKGMenuStyle::DrawRoundedBox(Out, LayerId + 9, Geo, FVector2f(P - SlotSize * 0.5), FVector2f(SlotSize),
			                             WithAlpha(FMath::Lerp(InkTop, Gold * 0.45f, G), 0.92f * A), -1.0f,
			                             WithAlpha(FMath::Lerp(Cream * 0.4f, Gold, G), (0.35f + 0.65f * G) * A), 1.5f + G);
			DrawLabel(Out, LayerId + 10, Geo, Text, SlotFont, P, WithAlpha(FMath::Lerp(Cream, Gold, G), A));
			DrawLabel(Out, LayerId + 10, Geo, FString::FromInt(i + 1), Font(9, true), P + FVector2D(SlotSize.X * 0.5 - 9.0, -SlotSize.Y * 0.5 + 8.0),
			          WithAlpha(Muted, 0.9f * A));
		}
		DrawLabel(Out, LayerId + 10, Geo, Def.Title.ToString().ToUpper(), Font(11, true), C - FVector2D(0.0f, 10.0f), WithAlpha(Gold, A));
		const FString Hint = FText::Format(LOCTEXT("Release", "release {0}"), Def.Key.GetDisplayName()).ToString();
		DrawLabel(Out, LayerId + 10, Geo, Hint, Font(9), C + FVector2D(0.0f, 10.0f), WithAlpha(Muted, A));
		const FVector2D Dot = C + Cursor * (SlotRadius / CursorMax) * K;
		FKGMenuStyle::DrawRoundedBox(Out, LayerId + 11, Geo, FVector2f(Dot - FVector2D(5.0f)), FVector2f(10.0f, 10.0f), WithAlpha(Gold, 0.9f * A), -1.0f);
		return LayerId + 12;
	}

private:
	void UpdateMarkers(const FGeometry& Geo)
	{
		Markers.Reset();
		APlayerController* Viewer = PC.Get();
		UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
		if (!World || !Viewer->PlayerCameraManager)
		{
			return;
		}
		const UKGVoiceComponent* MyVoice = UKGVoiceComponent::FindForController(Viewer);
		const AKGCharacter* MyBody = Cast<AKGCharacter>(Viewer->GetPawn());
		const UKGEmoteComponent* NearOffer = UKGEmoteComponent::FindNearbyOffer(MyBody);
		const FVector CamLoc = Viewer->PlayerCameraManager->GetCameraLocation();
		const FVector2D Local(Geo.GetLocalSize());
		FIntPoint VP;
		Viewer->GetViewportSize(VP.X, VP.Y);
		const FVector2D PixelToLocal(VP.X > 0 ? Local.X / VP.X : 1.0, VP.Y > 0 ? Local.Y / VP.Y : 1.0);
		for (TActorIterator<AKGCharacter> It(World); It; ++It)
		{
			const AKGCharacter* Body = *It;
			const APlayerState* PS = Body->GetPlayerState();
			if (!PS || Body->IsDead())
			{
				continue;
			}
			const bool bSelf = Body == MyBody;
			if (bSelf && (!Body->GetEmote() || Body->GetEmote()->GetCameraAlpha() < 0.5f))
			{
				continue;   // you cannot see your own head in first person
			}
			const UKGMouthComponent* Mouth = Body->GetMouth();
			const bool bTalking = (Mouth && Mouth->IsTalking()) || (MyVoice && MyVoice->WasHeardRecently(PS));
			const bool bOffer = NearOffer && Body->GetEmote() == NearOffer;
			if (!bTalking && !bOffer)
			{
				continue;
			}
			const float Up = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 36.0f;
			const FVector Head = Body->GetActorLocation() + FVector(0.0f, 0.0f, Up);
			const float Dist = static_cast<float>(FVector::Dist(CamLoc, Head));
			if (Dist > 3500.0f)
			{
				continue;
			}
			FVector2D Screen;
			if (!Viewer->ProjectWorldLocationToScreen(Head, Screen, true))
			{
				continue;
			}
			FMarker& M = Markers.AddDefaulted_GetRef();
			M.Pos = Screen * PixelToLocal;
			M.Size = FMath::GetMappedRangeValueClamped(FVector2D(300.0, 3000.0), FVector2D(30.0, 16.0), Dist);
			M.Level = bTalking ? (Mouth ? Mouth->GetJawOpen() : 0.5f) : -1.0f;
			const AKGPlayerState* KGPS = Cast<AKGPlayerState>(PS);
			M.bGhost = KGPS && KGPS->LifeState == EKGLifeState::Ghost;
			if (bOffer)
			{
				M.Prompt = NearOffer->GetPartnerPrompt(false).ToString();
			}
		}
	}

	void UpdateSelf(float DeltaTime)
	{
		SelfLine.Reset();
		PartnerLine.Reset();
		SelfLevel = -1.0f;
		APlayerController* Viewer = PC.Get();
		const UKGVoiceComponent* Voice = UKGVoiceComponent::FindForController(Viewer);
		if (!Viewer || !Voice)
		{
			return;
		}
		const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
		const bool bOpenMic = Settings && Settings->GetOpenMic();
		const EKGVoiceChannel Channel = Voice->GetLocalChannel();
		if (Voice->IsTransmitting())
		{
			if (Channel == EKGVoiceChannel::Closed)
			{
				SelfLine = LOCTEXT("VoiceClosed", "VOICE CLOSED").ToString();
				SelfColor = Crimson;
			}
			else
			{
				const FText Room = Channel == EKGVoiceChannel::Ghost ? LOCTEXT("RoomGhost", "GHOSTS")
				                   : Channel == EKGVoiceChannel::Square ? LOCTEXT("RoomSquare", "SQUARE") : LOCTEXT("RoomNearby", "NEARBY");
				SelfLine = FText::Format(bOpenMic ? LOCTEXT("MicOpen", "MIC OPEN  ·  {0}") : LOCTEXT("Talking", "TALKING  ·  {0}"), Room).ToString();
				SelfColor = Channel == EKGVoiceChannel::Ghost ? Ghost : Gold;
				SelfLevel = FMath::Clamp(Voice->GetLocalAmplitude() * 4.0f, 0.0f, 1.0f);
			}
			SelfAlpha = 0.95f;
		}
		else
		{
			SelfAlpha = 0.0f;
		}
		const UKGEmoteComponent* Emote = UKGEmoteComponent::FindForPlayer(Viewer->PlayerState);
		if (Emote && Emote->GetPartner().IsActive())
		{
			PartnerLine = Emote->GetPartnerPrompt(true).ToString();
		}
	}

	TWeakObjectPtr<APlayerController> PC;
	bool bOpen = false;
	int32 Menu = 0;
	float OpenAlpha = 0.0f;
	float SlotGrow[8] = {};
	FVector2D Cursor = FVector2D::ZeroVector;
	int32 Selected = INDEX_NONE;
	TArray<FMarker> Markers;
	FString SelfLine;
	FLinearColor SelfColor = Gold;
	float SelfAlpha = 0.0f;
	float SelfLevel = -1.0f;
	FString PartnerLine;
};

// =================================================================================================================

namespace KGVoiceUIPrivate
{
	struct FEntry
	{
		TSharedPtr<SKGVoiceLayer> Layer;
		TWeakObjectPtr<APlayerController> PC;
		TWeakObjectPtr<UGameViewportClient> Viewport;
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
		return Entry && Entry->Layer.IsValid() ? Entry : nullptr;
	}

	void Detach(FEntry& Entry)
	{
		if (Entry.Layer.IsValid() && Entry.Viewport.IsValid())
		{
			Entry.Viewport->RemoveViewportWidgetContent(Entry.Layer.ToSharedRef());
		}
		Entry.Layer.Reset();
	}
}

void FKGVoiceUI::Ensure(APlayerController* PC)
{
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = LP ? LP->ViewportClient.Get() : nullptr;
	if (!Viewport)
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
	if (!Entry.Layer.IsValid())
	{
		Entry.Layer = SNew(SKGVoiceLayer).PlayerController(PC);
		Entry.PC = PC;
		Entry.Viewport = Viewport;
	}
	if (!Entry.Layer->GetParentWidget().IsValid() && GFrameCounter >= Entry.NextAttachFrame)
	{
		Entry.NextAttachFrame = GFrameCounter + 30;
		Viewport->RemoveViewportWidgetContent(Entry.Layer.ToSharedRef());
		Viewport->AddViewportWidgetContent(Entry.Layer.ToSharedRef(), 4);
	}
}

void FKGVoiceUI::Remove(APlayerController* PC)
{
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (FEntry* Entry = LP ? Entries().Find(LP) : nullptr)
	{
		Detach(*Entry);
		Entries().Remove(LP);
	}
}

void FKGVoiceUI::RemoveForWorld(const UWorld* World)
{
	for (auto It = Entries().CreateIterator(); It; ++It)
	{
		const APlayerController* PC = It.Value().PC.Get();
		if (!It.Key().IsValid() || !PC || PC->GetWorld() == World)
		{
			Detach(It.Value());
			It.RemoveCurrent();
		}
	}
}

bool FKGVoiceUI::IsWheelOpen(const APlayerController* PC)
{
	const FEntry* Entry = Find(PC);
	return Entry && Entry->Layer->IsOpen();
}

int32 FKGVoiceUI::GetOpenMenu(const APlayerController* PC)
{
	const FEntry* Entry = Find(PC);
	return Entry && Entry->Layer->IsOpen() ? Entry->Layer->GetMenu() : INDEX_NONE;
}

void FKGVoiceUI::OpenWheel(APlayerController* PC, int32 Menu)
{
	if (FEntry* Entry = Find(PC))
	{
		Entry->Layer->Open(Menu);
	}
}

void FKGVoiceUI::UpdateWheel(APlayerController* PC, const FVector2D& MouseDelta)
{
	if (FEntry* Entry = Find(PC); Entry && Entry->Layer->IsOpen())
	{
		Entry->Layer->AddDelta(MouseDelta);
	}
}

void FKGVoiceUI::SelectWheelSlot(APlayerController* PC, int32 Slot)
{
	if (FEntry* Entry = Find(PC); Entry && Entry->Layer->IsOpen())
	{
		Entry->Layer->Select(Slot);
	}
}

int32 FKGVoiceUI::CloseWheel(APlayerController* PC, bool bSend)
{
	FEntry* Entry = Find(PC);
	if (!Entry || !Entry->Layer->IsOpen())
	{
		return INDEX_NONE;
	}
	const int32 Menu = Entry->Layer->GetMenu();
	const int32 Slot = Entry->Layer->GetSelected();
	Entry->Layer->Close();
	const TArray<FKGVoiceMenuDef>& Menus = FKGVoiceCommandCatalog::GetMenus();
	if (!bSend || Slot == INDEX_NONE || !Menus.IsValidIndex(Menu) || !Menus[Menu].Commands.IsValidIndex(Slot))
	{
		return INDEX_NONE;
	}
	return Menus[Menu].Commands[Slot];
}

#undef LOCTEXT_NAMESPACE
