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
