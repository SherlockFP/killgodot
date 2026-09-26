# SPRINT-040 — Storm Manor expansion: a huge, endless rich house with secrets and traps

User (2026-09-26): "Make the manor much bigger. The inside should be huge and detailed, the kind of very rich house that
goes on and on. Secret rooms, hidden compartments, secret tasks, manor-only tasks, extra traps and so on."
Builds on SPRINT-017 (design) and SPRINT-018 (build: 24 rooms, 1597 props, 23/23 checks). Also fixes 018's leftovers:
sparse rooms (blue room, nursery, study) and white pack meshes.

## Acceptance (fixed)
1. **Much bigger.**
   - At least 2 new wings (e.g. an East Guest Wing and a West Servants'/Gallery Wing), a grand staircase hall, long
     portrait corridors, enfilades of connected salons, and a second basement level (vaults, wine catacombs).
   - Target ≥ 45 named rooms, a footprint ~2× the current one.
   - Wings open by player count (the existing lockable-region idea), so 6–8 player matches stay dense. The Great Hall
     stays reachable within 30 s from the unlocked area.
   - Authored in author_stormmanor.py → JSON; validate_stormmanor.py passes.
2. **Dense and detailed.** Every room reads as lived-in luxury: layered furniture, rugs, paintings, shelves with small
   props, lighting pools.
   - Density at least 1.5× SPRINT-018 per m², within the budgets: ≤ 900 plain actors per floor, instancing for repeats,
     ≤ 70 lights, ≤ 5 shadowed.
   - The previously sparse rooms get filled.
   - The white-mesh pack colours are fixed.
   - The placement validator shows 0 violations.
3. **Secrets.**
   - ≥ 8 secret rooms/passages: rotating bookcase, portrait door, fireplace passage, false wardrobe, dumbwaiter, a
     crypt under the chapel, a servants' crawlway, a hidden observatory.
   - ≥ 10 hidden compartments (loose brick, false drawer, hollow book, floor safe…) holding loot or evidence.
     Discovered by interaction; server-replicated state, once found it stays open for everyone.
   - Minimap: a secret is only drawn after someone discovers it.
4. **Manor-only chores and secret chores.**
   - ≥ 8 new manor chores in the SPRINT-016 "simple but fun" style: wind the grand clocks, polish the silver, tune the
     piano, feed the aviary, stoke the boiler, set the chessboard, sort the wine, light the chapel.
   - ≥ 3 SECRET chores that only appear once a secret is found. They give a reward and map knowledge: a shortcut
     unlocked, or a clue.
5. **Traps (telegraphed, fair, social-deduction flavoured).** At least 6:
   - trapdoor to the cellar
   - falling chandelier
   - locking door (the room seals for 20 s)
   - gas lamps going out
   - a creaking-floor alarm that pings the minimap
   - a portrait with eyes that watch (a witness camera)
   The Impatient can arm some of them as sabotage with a cooldown. All are visible or audible before triggering, never
   an instant death from nowhere, and they enter the event log where one exists. Tests for arming, trigger and cooldown.
6. **Evidence and invariants.**
   - Headless rebuild; verify_stormmanor_build.py extended with the new checks and green.
   - Match smoke on L_StormManor reaches a winner.
   - Render sheet with ≥ 30 shots, including 4 secrets and 3 traps (2 look rounds).
   - The Morrowmere run_invariants.ps1 stays green.

## Writable scope
- Tools/Level/author_stormmanor.py, validate_stormmanor.py, render_stormmanor.py, stormmanor_layout.json
- Tools/Unreal/kg_build_stormmanor*.py/.ps1, kg_sm_dress.py, kg_capture_stormmanor.py, Tools/Level/verify_stormmanor_build.py
- new C++ under Source/KillGodot/Manor/ (secrets, compartments) and a GENERIC trap framework in Source/KillGodot/Traps/ (arm / telegraph / trigger / cooldown / event hook) that SPRINT-041's Trapper role reuses, with minimal registration hooks
- manor chore data plus minimal hooks into Chores/WorldChores
- tests, docs

## Limits
- 3 fix attempts per failing check
- 2 look rounds
- plateau stop
- extras go in as proposals

Starts when a main-model slot frees up (after the v2 cliff/stairs sprint).

## Result
2026-09-26: `verify_stormmanor_build.py` ALL PASS (34 checks), `validate_stormmanor.py` PASS, match smoke on L_StormManor
"Match decided: Impatient win" (12 bots), render sheet `Docs/Level/StormManor_Renders.png` (71 shots), 2 look rounds.
- 1: 56 named rooms (was 24) + 22 corridors/grounds; north block (grand staircase hall, 68 m Long Portrait Gallery + Upper
  Portrait Corridor, an enfilade of 6 salons), West Servants' Wing, East Guest Wing, lower vaults C2 (wine catacombs,
  strong vault, ossuary); indoor floor 5520 -> 11280 m2, ground-floor footprint 2.18x. Wing gates (min_n): north + west 8,
  east 10, vaults 12; 33 gate doors (KG_WingGate + KG_MinN_n, UKGManorSubsystem locks them in the lobby); the Great Hall is
  <= 15.2 s from every open point at every gate. Z0 8 m (C2 over the waves).
- 2: 5378 dressing meshes (4942 indoors = 0.438/m2, 1.83x); the luxury layer (kg_sm_dress.lux) + a centre group per big
  room; blue room/nursery/study 90/97/88 (was 51/42/35); 70 lights (4 shadowed); plain actors max 564/floor; 2980 props
  traced: 0 floating, 0 sunk, 0 pre-check; white props recoloured by an explicit list (see proposals).
- 3: 10 secrets as AKGSecretPassage pairs (S7 fireplace, S8 crypt, S9 servants' crawlway, S10 hidden observatory + S1-S6)
  incl. 2 secret rooms; 12 AKGHiddenCompartment (loot / readable clues); the minimap draws discovered secrets only.
- 4: 16 manor-only chores + 4 secret chores (given on discovery; rewards open a shortcut or a clue compartment).
- 5: generic trap framework Source/KillGodot/Traps (FKGTrapMachine, UKGTrapSubsystem with arm policies + event log,
  AKGTrap); 14 traps of 6 kinds; tests KillGodot.Traps.* and KillGodot.Manor.* pass (G2 81/81).
- 6: run_invariants.ps1: I2 v2 verify, I3 smokes (chat/emote/fish/chore/dig/voice/partner) and I4 match smoke PASS; I1 first failed only because other agents' UnrealEditor.exe held the module DLL (LNK1104), rerun `run_gates.ps1`: G1 PASS, G2 88/88 PASS.
- Open items and extras: Docs/Backlog.md, Proposed (SPRINT-040 lines); verification debt #19.
