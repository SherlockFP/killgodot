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
- new C++ under Source/KillGodot/Manor/ (secrets, compartments, traps), with minimal registration hooks
- manor chore data plus minimal hooks into Chores/WorldChores
- tests, docs

## Limits
- 3 fix attempts per failing check
- 2 look rounds
- plateau stop
- extras go in as proposals

Starts when a main-model slot frees up (after the v2 cliff/stairs sprint).
