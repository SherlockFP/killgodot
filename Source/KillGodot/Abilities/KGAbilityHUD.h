#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;

/**
 * SPRINT-041 HUD layer, drawn through AHUD::OnHUDPostRender (registered by UKGAbilitySubsystem, so AKGHUD is untouched):
 * - owner only: the ability bar (charges, cooldown, Alt+N keys), the targeting prompt with a live target check, the
 *   Trapper's private notes (tripwire alerts, bites) and the last server verdict;
 * - everyone: the look-at wound line (bite marks / snare wounds on a victim or a body within 3.5 m).
 */
namespace KGAbilityHUD
{
	void Draw(AHUD* Hud, UCanvas* Canvas);
}
