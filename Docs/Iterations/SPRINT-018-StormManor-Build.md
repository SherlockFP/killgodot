# SPRINT-018 — Map 2 "Storm Manor": build it

**Objective (user-visible):** a second playable map, "Storm Manor", picked from the menu. It is built headless from the
approved SPRINT-017 design (`Docs/Level/StormManor_Plan.md`, `Tools/Level/stormmanor_layout.json`). The user asked for it
"big, full and detailed inside".

## Acceptance (fixed, never edited mid-sprint)
1. `/Game/KillGodot/Maps/L_StormManor` is built HEADLESS from the JSON by `Tools/Unreal/kg_build_stormmanor.py` plus
   `Tools/Unreal/kg_build_stormmanor_all.ps1`:
   - the rock island and the sea, the 5 levels, all 24 rooms with walls, floors and ceilings, doors, stairs with bot
     ramps, windows
   - secret passages: MVP = a hidden door or an interact-teleport between the two ends, using existing interactables
     (`AKGPassage`)
   - the Great Hall is the meeting place, with the `KG_Gallows` / `KG_BotHub` markers the game mode needs, player
     starts, the NavMeshBoundsVolume and a navmesh built headless
   - a storm look: night, a rain-dark sky, warm interior lights (at most 60 lights, shadowless except the hero lights)
2. FULL AND DETAILED: every room is dressed with a story vignette, reusing the Furniture/Props/Village kits and dress
   packs. At most 900 plain actors per floor, and repeated meshes are instanced. The grounds are dressed too
   (courtyard, greenhouse, graveyard, boathouse). The placement validator finds no floating, sunk or blocking props.
3. Chores: all 19 manor chores exist as world-chore data, placed and reachable (route check), with panel fallbacks where
   a world chore needs code that does not exist yet. Bots can roam and reach the meeting room.
4. Playable: the menu lists "Storm Manor", and a headless whole-match smoke on L_StormManor reaches a winner
   (`kg_match_smoke.ps1 -Map`). The minimap regions and texture come from the JSON.
5. Evidence: a render tour with an exterior aerial, the Great Hall, 8+ rooms, a secret passage and the cellar
   → `Docs/Level/StormManor_Renders.png`, with 2 look rounds. The invariants (`run_invariants.ps1`) stay green for
   Morrowmere.

**Out of scope** (proposals for the next sprint): the Power-Outage sabotage code, the storm/lightning manager,
rattling windows, and any C++ gameplay systems the plan lists as new.

## Writable scope
New files:
- `Tools/Unreal/kg_build_stormmanor.py`, `Tools/Unreal/kg_build_stormmanor_all.ps1`, `Tools/Unreal/kg_sm_dress.py`
- `Tools/Unreal/kg_capture_stormmanor.py`, `Tools/Unreal/kg_capture_stormmanor.ps1` (render tour, nav and placement probe)
- `Tools/Level/stormmanor_geo.py` (shared build geometry), `Tools/Level/stormmanor_world_chores.json`,
  `Tools/Level/gen_stormmanor_chores.py`,
  `Tools/Level/stormmanor_world_chores.resolved.json`, `Tools/Level/render_stormmanor_minimap.py`,
  `Tools/Level/verify_stormmanor_build.py`
- `Source/KillGodot/Chores/WorldChores/KGWorldChoreData_StormManor.gen.inl`
- `Docs/Level/StormManor_Renders.png`, `Art/Textures/Map/*StormManor*`, `/Game/KillGodot/Maps/L_StormManor`,
  `/Game/KillGodot/UI/Map/T_KG_Map_StormManor`
Edited:
- `Source/KillGodot/Chores/WorldChores/KGWorldChoreTypes.{h,cpp}`, `KGWorldChoreWorld.cpp` (one catalog per map)
- `Source/KillGodot/Core/KGGameState.cpp` (lights tagged `KG_FixedLook` keep the map's own look)
- `Source/KillGodot/UI/Menu/KGMenuActions.cpp` (menu entry), `Source/KillGodot/Private/Tests/KGWorldChoreTests.cpp`
- `Tools/Gauntlet/kg_match_smoke.ps1` (optional `-LogName`, so the manor smoke never overwrites the invariant's log)
- `Docs/Backlog.md` (proposals only)
Read-only (another agent is working on them): `kg_build_village_v2.py`, `dressing/v2/*`, `verify_v2_build.py`, water
materials. Their helpers are imported read-only or copied into the new files.

## Limits
- Headless only (no windows, no editor). Builds share the UBT mutex; never kill other processes.
- At most 3 fix attempts per failing check, 2 look rounds for the renders, stop on a plateau.
- No commit.

## Result
2026-09-26: `verify_stormmanor_build.py` ALL PASS (23 checks), `run_invariants.ps1` ALL INVARIANTS HOLD.
- 1: built headless in ~3 s + navmesh; 42 lights (3 hero), 52 doors, 129 windows, 10 stairs, 12 passage ends.
- 2: 1597 dressing meshes over 37 rooms/grounds; a second vignette layer from the /Env/Ext packs (`kg_sm_dress.py`,
  EXT table); plain actors per floor C 153 / F0 411 / F1 275 / F2 120 / F3 15; 1054 props traced: 0 floating, 0 sunk.
- 3: 19 chores / 56 anchors all reachable, 28/28 chore routes; 236/237 nav targets (the tower top is ladder-only).
- 4: menu entry; match smoke on L_StormManor: "Match decided: Impatient win" (12 bots).
- 5: `Docs/Level/StormManor_Renders.png` (24 shots), 2 look rounds (round 2: fuller bedrooms/library/nursery, the
  cellar cliff blob and the untextured kegs removed).
- Fixed on the way: HISM fields were saved fz(floor) too low (spawned at the origin now), no RecastNavMesh before the
  nav step, rim rock boxes filling the cellar at the stair feet and over the tunnel, the probe's own-collision traces.
