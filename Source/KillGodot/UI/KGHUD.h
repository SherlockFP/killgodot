#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KGHUD.generated.h"

/**
 * Minimal diegetic-first HUD (Docs/08_UI_UX.md §6): centre dot crosshair (red while a backstab is ready),
 * health + stamina bars bottom-left. Canvas-drawn so it needs no assets; the CommonUI HUD replaces it in M10.
 */
UCLASS()
class KILLGODOT_API AKGHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void DrawBar(float X, float Y, float Width, float Height, float Alpha, const FLinearColor& Fill);
	/** Phase + timer, alive count, own role, role card, death screen, epilogue banner, interact prompt. */
	void DrawMatchInfo(float S);
	void DrawTextAt(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, bool bCentre,
	                bool bLarge = true);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color);
	/** Counter-Strike style map card while the round spins up: map name, mode, a tip, a loading bar. */
	void DrawLoadingCard(float S);
	float CardStartTime = -1.0f;
	int32 TipIndex = 0;

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor CrosshairColor = FLinearColor::FromSRGBColor(FColor(255, 255, 255, 220));

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor BackstabColor = FLinearColor::FromSRGBColor(FColor(200, 16, 46, 255)); // Impatient crimson #C8102E

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor HealthColor = FLinearColor::FromSRGBColor(FColor(224, 65, 58, 240)); // #E0413A

	UPROPERTY(EditAnywhere, Category = "HUD")
	FLinearColor StaminaColor = FLinearColor::FromSRGBColor(FColor(242, 194, 48, 240)); // #F2C230
};
