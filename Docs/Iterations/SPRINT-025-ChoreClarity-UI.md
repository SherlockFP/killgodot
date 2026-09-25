# SPRINT-025 — Make chores easy to find and do, and a UI overhaul

User (2026-09-25, playing in the editor):
- "Make doing the chores simpler; they should be easy to see."
- "Holding M should open the minimap and make it big."
- "Improve the chores and their visibility, and improve the UI; it's very bad right now."

## Acceptance (fixed)
1. **Chore visibility in the world.** Your open chores (panel and world chores) show:
   - an in-world marker: a soft glow/outline on the station or item, plus a floating icon that scales with distance and
     clamps to the screen edge when off-screen, with the distance in metres;
   - a HUD chore tracker (top-left): the next step of each chore in one short line, the active chore highlighted.
   The Impatient's fake chores look identical to observers.
   Evidence: offscreen shots at 3 distances plus an off-screen-edge case; readable at 720p.
2. **Hold M for the big map.** Holding M shows a large map overlay (≈70% of the screen) with your chores, the places,
   you, and step waypoints. Releasing it returns to the minimap. A tap still toggles the full map, as today.
   Evidence: shots, and a test of the input logic.
3. **Simpler chore interaction.**
   - Every chore step is completable with one clear prompt: E, with the key glyph, the action verb and a progress ring.
   - Panel minigames get an "Easy" tuning: generous timing windows, fewer repeats, a target of 20–40 s including walking.
   - Failing never resets more than the current stage.
   Evidence: the automation timing check per minigame (median ≤ 40 s with the scripted player).
4. **UI overhaul pass (HUD + prompts + chore panel + menus consistency).**
   - One visual language: typography scale, spacing grid, panel style, icon set, colour roles, motion.
   - Every existing HUD element redone to it: vitals, gold, phase card, chore list, prompts, minimap frame, chat,
     emote wheel, dev panel stays dev.
   - A style sheet doc (Docs/08_UI_UX.md section) and a before/after contact sheet at 1080p and 720p.
   Evidence: contact sheet; no overlap/overflow at 720p–1440p (an automated layout check where possible).
5. **Invariants:** run_invariants.ps1 passes.

## Writable scope
- UI/ (KGHUD.cpp, KGHUDMap.inl and new UI files)
- Chores/UI (the panel and the minigame tuning tables)
- Chores/WorldChores/*Hud*
- UI/Menu styling files (visual only)
- tests, docs

Starts after the SPRINT-016 finisher is done, because they share the chore HUD files.

## Limits
- 2 look rounds per screen
- 3 fix attempts per failing check
- plateau stop
- extras go in as proposals
