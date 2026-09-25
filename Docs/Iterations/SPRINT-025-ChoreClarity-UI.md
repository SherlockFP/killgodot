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

## Stabilisation 2026-09-26 (sprint was cut off by the usage limit)
- `KillGodot.HUD.MapInput`: "release exactly at HoldSeconds closes" failed on double arithmetic
  ((50.0 + 0.3) - 50.0 < 0.3). Code fixed, not the test: `KGMapInput::HeldLongEnough` compares with a 1 µs tolerance so
  the documented inclusive threshold holds for world-time stamps.
- `KillGodot.Chores.Timing`: MendNets and FuelLighthouse were "UNSOLVED" (120 s give-up), not slow.
  - MendNets: `AutoPlay` tied every rope but never called `Solve()` (only `OnRelease` did). Fixed in the minigame:
    the scripted player now completes the stage like a real release does. Median 7.7 s.
  - FuelLighthouse wick stage: the cut only ended at `CutProg >= 1.0`, but the float projection of a cursor sitting on
    the end point lands at 0.9999999 on some slopes (2 of 3 seeds stalled). Fixed in the minigame: the cut ends within a
    quarter pixel of the end. Median 10.9 s. Both floors still sit under the scripted times.
- Game-thread hang (found by the "after" HUD shot run, which froze the frame after `kg.Chore.Reset all`): the chore
  tracker's step-shortening loop in `UI/KGHUDChoreMarkers.inl` cut 2 characters and appended 3 dots per pass, so a step
  that did not fit made the string grow forever. Rewritten to shorten until "<step>..." fits. This would have frozen a
  real session the first time a long step met a narrow tracker.
- `Tools/Unreal/kg_hud_shots.py`: `PlayerController.get_pawn` does not exist in the Python API (every placement step
  failed in the "before" run, so the near/mid/far/edge shots were all taken from the spawn point); uses
  `get_controlled_pawn()` now.
- `kg.Map.Debug 2` (forced "tapped open") behaved like a real tap, so `kg.Map.Debug 0` left the map open and the two HUD
  demo shots were taken under the map. `KGHUDMap.inl` now closes the map when a forced state ends.
- Evidence (offscreen, `Tools/Unreal/kg_hud_shots.ps1 -Tag after`, both 1280x720 and 1920x1080):
  `Saved/Screenshots/HUD/after_<WxH>_{near,mid,far,edge}.png` (markers + tracker at 3 m / 12 m / 35 m and behind the
  camera, with distances), `after_<WxH>_bigmap.png` (hold-M, "Release to close"), `after_<WxH>_fullmap.png` (tap,
  "Close · hold to peek"), `after_<WxH>_{demo2,demo3}.png` (UI pass: working a chore, interact prompt);
  before/after contact sheets `Saved/Screenshots/HUD/contact_1280x720.png`, `contact_1920x1080.png`
  (`Tools/Unreal/kg_contact_sheet.py`). Panel minigame shots stay in `Saved/UIShots/chore_*_1280x720.png`.
