# SPRINT-026 — Movement feel: strafe-based bunny hop (skill, not automatic)

User (2026-09-25): "Improve the movement. Add something like bunnyhop, tied to stamina, but the bunnyhop must not be
automatic: it should use strafe logic."

## Acceptance (fixed)
1. **Source-style air strafing** in UKGCharacterMovement:
   - air acceleration projected on wish-direction (air control from mouse + A/D strafing);
   - speed gain only from correctly synced strafes;
   - no auto-hop: holding jump does NOT re-jump, and each hop needs a fresh jump press timed on landing (a small buffer
     window of about 80 ms is allowed);
   - ground friction applies after a missed frame window.
   Networked through the CMC prediction (saved moves) with no corrections at 100 ms ping. Evidence: the netcode smoke
   with scripted inputs, and correction counts.
2. **Stamina and caps.**
   - Each jump costs stamina; chaining hops costs more.
   - A soft cap on max hop speed (about 1.35× sprint) that needs good strafing.
   - Stamina exhaustion stops chaining.
   - Carrying items, the knife out, fishing and chores disable or penalise hopping.
   - Numbers written in the GDD with rationale: the social deduction balance means killers mustn't out-run everyone
     forever.
   Evidence: automation tests for the strafe math and the stamina costs.
3. **Feel.**
   - Landing/jump camera bob and viewmodel reaction (FPArms2).
   - Footstep and landing sounds by speed.
   - Crouch-jump (+ ledge-reach).
   - A slight speed readout in the dev panel only.
   - Movement settings exposed: FOV kick on speed on/off.
   Evidence: a headless scripted run that shows the speed curve for good vs bad strafes (CSV + a graph PNG).
4. **Bots are unaffected** (they don't hop). **Invariants** pass.

## Writable scope
- Character/KGCharacterMovement.*
- the jump/landing hooks in KGCharacter.cpp (surgical)
- the viewmodel bob in KGViewmodelComponent.cpp
- new tests/smokes
- the Settings entries
- docs

## Limits
- 3 fix attempts per failing check
- 2 feel rounds
- plateau stop

## Stabilisation 2026-09-26 (sprint was cut off by the usage limit)
- `KillGodot.Character.AirStrafe` failed on its soft-cap check: the test sampled the gain exactly AT the cap and expected
  it smaller than below the cap, but the taper `1 - (speed - cap) / cap` is continuous and equals 1 at the cap by
  design (a step there would be a hard cap). Test fixed: samples 1.3x cap for "smaller", checks continuity at the cap.
- Evidence run `Tools/Unreal/kg_move_smoke.ps1` (two windowless -nullrhi processes, PktLag 50 ms each way):
  - client `KG_MOVE_DONE speed=621.9 corrections=15`; 12 051 `KG_MOVE_CSV` samples (the -nullrhi client runs ~1200 fps).
  - `python Tools/Unreal/kg_move_curve.py` -> `Docs/Level/SPRINT-026_speed_curve.csv` + `.png`: bad strafe plateaus
    at 320 uu/s (walk speed, one jump, no chaining); good strafe climbs 320 -> 1138 uu/s in 3 s, then drops at
    8.9 s / 10.0 s / 10.5 s (server snaps).
  - Fix applied: the smoke script only runs on player pawns (`IsPlayerControlled()`); listen-server bots are also
    "locally controlled" and had been running it (6 bot `KG_MOVE_DONE` lines in the server log).
- **Open (acceptance 1 not met): 15 corrections at 100 ms ping.** Cause: the chain-hop gate (`bHopChainBlocked`,
  `HopGainScale` from `Stamina.bExhausted`) and the hop stamina charge (`HopCounter` poll) live in `AKGCharacter::Tick`,
  outside the CMC move stream. The server simulates a batch of client moves against a flag that is only refreshed once
  per server frame, and client replays after a correction re-increment `HopCounter` (double charge). The first
  correction lands exactly when stamina runs out (~6 chain hops, 8.9 s) and the disagreement then cascades. The bad
  phase (no chaining) has 0 corrections, so the strafe math itself replays cleanly. Proposal in the Backlog: move
  stamina / hop-streak / `TimeSinceJumpPressed` into a custom `FSavedMove_Character` and charge hops inside the CMC.
- **Open (acceptance 2, tuning):** the good strafe reaches 1138 uu/s = 1.96x sprint against the "about 1.35x" soft cap;
  the taper (floor 0.08) only bites near 1.9x cap. Proposal: a steeper taper or a hard ceiling at ~1.5x cap.
