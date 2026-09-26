#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;

/** SPRINT-033/034 HUD layer (drawn by AKGHUD::DrawHUD): frost at the screen edge, the bite flash, wolf / Mist lines, the
 *  arrow back to the nearest path, the howl subtitle and the Camp Vigil line. Reads AKGForestPlayerInfo (owner-only). */
namespace KGForestHUD
{
	void Draw(AHUD* Hud, UCanvas* Canvas, float Scale);
}
