#include "UI/Forest/KGForestHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Forest/KGForestActors.h"
#include "Forest/KGForestMap.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void Tile(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Col);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}

	void Text(UCanvas* C, const FString& S, float X, float Y, float Scale, const FLinearColor& Col, bool bCentre = true)
	{
		UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
		if (!Font)
		{
			return;
		}
		FCanvasTextItem T(FVector2D(X, Y), FText::FromString(S), Font, Col);
		T.Scale = FVector2D(Scale);
		T.bCentreX = bCentre;
		T.EnableShadow(FLinearColor(0, 0, 0, 0.85f), FVector2D(2.0f, 2.0f));
		C->DrawItem(T);
	}

	/** Edge vignette in bands (frost = pale blue, bite = red). */
	void Vignette(UCanvas* C, const FLinearColor& Col, float Amount, float Depth)
	{
		const float W = C->ClipX, H = C->ClipY;
		const int32 Bands = 10;
		for (int32 i = 0; i < Bands; ++i)
		{
			const float F = 1.0f - float(i) / Bands;
			const float A = Amount * F * F * 0.5f;
			const float D = Depth * (i + 1) / Bands;
			const FLinearColor K(Col.R, Col.G, Col.B, A);
			Tile(C, 0, 0, W, D * 0.6f, K);
			Tile(C, 0, H - D * 0.6f, W, D * 0.6f, K);
			Tile(C, 0, 0, D, H, K);
			Tile(C, W - D, 0, D, H, K);
		}
	}
}

void KGForestHUD::Draw(AHUD* Hud, UCanvas* Canvas, float Scale)
{
	UWorld* World = Hud ? Hud->GetWorld() : nullptr;
	if (!World || !Canvas)
	{
		return;
	}
	const AGameStateBase* GS = World->GetGameState();
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	APlayerController* PC = Hud->GetOwningPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;

	// Howl subtitle for everyone within earshot (the sector only when close).
	if (const AKGForestDirector* D = AKGForestDirector::Get(World); D && Me && Now - D->LastHowlTime < 4.5f)
	{
		const float Dist = FVector::Dist2D(D->LastHowlAt, Me->GetActorLocation()) / 100.0f;
		if (Dist <= KGForest::HearHowl)
		{
			const FString Where = Dist <= KGForest::HowlSectorR + 60.0f && D->LastHowlSector
				? FString::Printf(TEXT("A wolf howls - close, in the %s wood."), *FKGForestMap::SectorName(D->LastHowlSector))
				: FString(TEXT("Far off, a wolf howls."));
			Text(Canvas, Where, W * 0.5f, H * 0.80f, 1.1f * Scale, FLinearColor(0.85f, 0.88f, 1.0f, 0.95f));
		}
	}

	AKGForestPlayerInfo* Info = AKGForestPlayerInfo::FindLocal(World);
	if (!Info)
	{
		return;
	}
	// frost creeping in from the edges (the Mist), red flash on a bite
	if (Info->Frost > 0.01f)
	{
		const float Pulse = 1.0f + 0.08f * FMath::Sin(Now * 2.2f);
		Vignette(Canvas, FLinearColor(0.82f, 0.9f, 1.0f), FMath::Clamp(Info->Frost * Pulse, 0.0f, 1.0f), H * (0.18f + 0.3f * Info->Frost));
	}
	const float SinceBite = Now - Info->BittenAt;
	if (SinceBite >= 0.0f && SinceBite < 0.9f)
	{
		Vignette(Canvas, FLinearColor(0.75f, 0.02f, 0.02f), 1.0f - SinceBite / 0.9f, H * 0.3f);
	}
	// stage line
	FString Stage;
	FLinearColor Col(1.0f, 0.93f, 0.8f, 1.0f);
	const EKGWolfStage WS = static_cast<EKGWolfStage>(Info->WolfStage);
	if (Info->MistStage == uint8(EKGMistStage::Tongue))
	{
		Stage = TEXT("The Mist is coming. Walk to a path or a light.");
		Col = FLinearColor(0.8f, 0.9f, 1.0f, 1.0f);
	}
	else if (WS == EKGWolfStage::Attack)
	{
		Stage = TEXT("WOLVES! Get back to the path!");
		Col = FLinearColor(1.0f, 0.45f, 0.35f, 1.0f);
	}
	else if (WS == EKGWolfStage::Eyes)
	{
		Stage = TEXT("Eyes in the dark. Walk back to the path - now.");
		Col = FLinearColor(1.0f, 0.8f, 0.4f, 1.0f);
	}
	else if (WS == EKGWolfStage::Howl)
	{
		Stage = TEXT("You are being watched. Head for a path or a light.");
	}
	else if (Info->MistStage == uint8(EKGMistStage::Frost))
	{
		Stage = TEXT("Your breath fogs. The deep forest is not for walking alone.");
		Col = FLinearColor(0.85f, 0.92f, 1.0f, 1.0f);
	}
	if (!Stage.IsEmpty())
	{
		Text(Canvas, Stage, W * 0.5f, H * 0.13f, 1.25f * Scale, Col);
	}
	// the arrow back to safety (compass: relative to where the camera looks)
	if (!Stage.IsEmpty() && !FVector(Info->SafeDir).IsNearlyZero() && PC)
	{
		const float CamYaw = PC->GetControlRotation().Yaw;
		const float DirYaw = FVector(Info->SafeDir).Rotation().Yaw;
		const float Rel = FMath::DegreesToRadians(FRotator::NormalizeAxis(DirYaw - CamYaw));
		const FVector2D C(W * 0.5f, H * 0.21f);
		const FVector2D Tip = C + FVector2D(FMath::Sin(Rel), -FMath::Cos(Rel)) * 34.0f * Scale;
		const FVector2D Side = FVector2D(FMath::Cos(Rel), FMath::Sin(Rel)) * 14.0f * Scale;
		const FVector2D Back = C - FVector2D(FMath::Sin(Rel), -FMath::Cos(Rel)) * 12.0f * Scale;
		const FLinearColor ArrowCol(1.0f, 0.85f, 0.45f, 0.95f);
		Canvas->K2_DrawLine(Back + Side, Tip, 4.0f * Scale, ArrowCol);
		Canvas->K2_DrawLine(Back - Side, Tip, 4.0f * Scale, ArrowCol);
		Canvas->K2_DrawLine(Back + Side, Back - Side, 3.0f * Scale, ArrowCol);
		Text(Canvas, FString::Printf(TEXT("path %d m"), FMath::RoundToInt(Info->SafeDist)), C.X, C.Y + 30.0f * Scale, 0.85f * Scale, ArrowCol);
	}
	// the server's own line for this player (vigil, wall return, evidence read-outs)
	if (!Info->Line.IsEmpty() && Now < Info->LineUntil)
	{
		Text(Canvas, Info->Line, W * 0.5f, H * 0.72f, 1.05f * Scale, FLinearColor(1.0f, 0.95f, 0.85f, 1.0f));
	}
	// Camp Vigil line
	if (Info->Vigil > 0)
	{
		const AKGCampfire* F = AKGCampfire::Get(World);
		const FString V = FString::Printf(TEXT("Camp Vigil: %s  -  fire %d%%"),
		                                  Info->Vigil == 2 ? TEXT("keep the fire burning") : TEXT("walk to the camp in the north wood"),
		                                  F ? FMath::RoundToInt(F->Fuel) : 0);
		Text(Canvas, V, 24.0f * Scale, H * 0.30f, 0.95f * Scale, FLinearColor(1.0f, 0.78f, 0.45f, 1.0f), false);
	}
}
