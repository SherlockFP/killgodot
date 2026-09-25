# Zone dressing (filling Morrowmere with content)

The user's verdict on the map: "it feels very empty — fill it, a LOT, with content". This folder holds one module per
zone, `dress_<zone>.py`, each defining `dress()`. They are run live by `Tools/Unreal/kg_dress.py` and at the end of the
full village build (`kg_build_village.py` calls `kg_dress.run()` after the meadow, before chores/nav).

## Files
- `kg_dress_common.py` (import as `C`): ground/slope/ground_min, zones (`C.zone_of`, `C.in_zone`), layout helpers
  (`C.buildings()` frames with `.door`, `C.lanes()`, `C.lane_samples(name)`, `C.lane_distance`, `C.task_spots()`),
  occupancy (`C.free(x, y, r)`, `C.claim`), spawning (`C.place`, `C.instanced`, `C.light`, `C.ambient`,
  `C.breakable`, `C.seat`, `C.loot_chest`, `C.mover`, `C.spawn_class`, `C.scatter`), `C.clear_grass(x, y, r)`.
  Owned by the coordinator: do not edit it; put zone-specific helpers in your own module.
- `asset_catalog.md` — every usable static mesh with its size (cm) and pivot. Only use paths from it (or from a
  dressing pack the prop factories publish under `/Game/KillGodot/Env/Dress/`). Never use the stale folders
  `/Env/Pirate/`, `/Env/PirateC2/`, `/Env/PirateClean/`, `/Env/PirateProps/`, `/Env/Japan/`, `/Env/JapanProps/`.

## Run your zone live (editor open, PIE stopped)
```
python Tools/Unreal/kg_remote.py --timeout 900 -c "import sys; sys.path.insert(0, 'D:/Kill Godot/Tools/Unreal'); import kg_dress, importlib; importlib.reload(kg_dress); kg_dress.run(['harbour'])"
```
It clears `Dress/<Zone>`, runs your `dress()`, rebuilds nav, saves the level and prints `KG_DRESS <zone>: {stats}`
(including `missing` mesh paths and any traceback). If it prints "PIE is running", someone else is testing: wait a
minute and retry. Every editor command goes through `Tools/Unreal/kg_lock.py`, so parallel agents queue up.

## Look at it
```
python Tools/Unreal/kg_capture.py "shot:harbour_a:X,Y,Z,PITCH,YAW" "shot:harbour_b:..."
```
-> `Saved/Screenshots/KG_Cap_<name>.png` (Read the PNG to see it). Always prefix names with your zone. Eye level is
`C.ground(x, y) + 170`. Editor sprites (speaker/flag/gamepad icons) show in captures; ignore them.

## Rules
- Stay in your zone (`C.free()` checks it by default). Keep lanes, doors (`C.buildings()[i].door`), chores
  (`C.task_spots()`), player starts and the well/gallows walkable: 2 m corridors for players and bots (NavMesh).
- Nothing floating or half-sunk: use `C.ground_min(x, y, r)` for wide props, skip steep spots with `C.slope`.
- Collision on anything knee-high or bigger that people would walk into; none on flat/small clutter.
- Performance budget per zone: <= 900 plain actors (`stats['props']`), repeated meshes through `C.instanced()`
  (one AKGFoliageField per zone, a HISM per mesh: cheap; supports collision), <= 10 new point lights (wilds <= 14),
  `cull=` on small props (<= 1 m: 5000-7000 cm; 1-3 m: 9000-12000; big landmarks: none).
- Compose vignettes with a story (a fisherman's corner, a abandoned camp, a market stall with goods), not uniform
  random scatter. Tuck clutter against walls and fences, vary yaw/scale, keep colours cohesive.
- Gameplay hooks where they fit: `C.seat()` on benches/stools (E to sit), `C.loot_chest()` at points of interest,
  `C.breakable()` for smashable crates/barrels/pots, `C.mover()` for windmill sails / swinging signs / bobbing buoys.
  All of these classes are compiled into the running editor (since 15:07); broken crates drop FKGLoot items.
- Do not: run PIE, close the editor, run the full village build, import assets (the prop factories do that),
  touch `MaterialEditingLibrary.delete_all_material_expressions`, or edit files outside your module.

## v2 map (L_Morrowmere_v2, "The Amphitheatre Cove")
- Modules: `dressing/v2/dress_<zone>.py` (square, harbour, streets, church, japan, countryside, coast, wilds, beckside,
  plus the town-wide `town` pass for lit shell windows), sharing `dressing/v2/kit.py` (facade window rhythm, flower
  boxes, signs, bunting, laundry, stalls + goods, paving, benches/stools as seats, buoys, boats, farm and wild pieces).
- Helpers: `Tools/Unreal/kg_dress_common_v2.py` (same API as v1 `C`, plus `C.anchor/anchors/xy`, `C.buildings()` with
  `.kind/.district/.door`, `C.lane/lane_edge/lane_samples`, `C.polar` round the basin, `C.free/free_line/find/why_blocked`,
  `C.in_garden`, `C.remove_builder`, commandlet-safe spawning, and a dry-run backend that only records).
- Run headless (editor closed): `powershell -File Tools/Unreal/kg_dress_v2.ps1 [-Zones "square harbour"] [-Fast]`,
  then `kg_build_v2_all.ps1 -From 6` (navmesh + capture) and `python Tools/Level/verify_v2_build.py`.
  Full rebuild: `kg_build_v2_all.ps1` (dressing is step 5). Live editor: `kg_dress.run(['square'], level="v2")`.
- Plan in seconds without UE: `python Tools/Unreal/kg_dress_v2.py --dry [zone ...]` -> `Saved/KG_V2_DressPlan.json` +
  `Saved/KG_V2_DressPlan_<zone|core>.png`; `KG_DRESS_REPORT=Saved/KG_V2_DressPlan.json python Tools/Level/verify_v2_build.py`
  checks the plan's clearances (doors, stairs, ramps, central 2 m of every walkway, chores, starts) before a UE run.
- Rules as above; v2 reserved areas are built from the layout (door aprons + door-to-street paths, stair/ramp corridors,
  wall strips, arches, bridges, chores, starts, fountain, gallows, well). Budgets: <= 900 actors, <= 10 lights per zone.
- Polish pass (plan 11.4): the Blender pack `KG_DressTerrace` (`Tools/Blender/kg_make_dress_terrace.py` ->
  `Art/Packed/KG_DressTerrace_Clean.glb/.json`, imported by build step 2 with `kg_import_dress_pack.py KG_DressTerrace_Clean`)
  replaces kit fallbacks at the source: `prep_v2_placements.py` puts `DT:Balustrade_2m` + `DT:Balustrade_Post` on the
  harbour and crown walls (the sea window over the Grand Stair, the Belvedere), `DT:QuayWall_4m(_Ring)` on 4 m chords of
  the quay edge, `DT:QuaySteps` at the two water stairs and `DT:StoneArchBridge_7m` for the Old Stone Bridge;
  `kg_build_village_v2.py` adds the four `DT:ClockFace` dials, the `DT:HarbourLight` at the mole tip, kit
  `Roof_Dormer_RoundTile` dormers on exposed 6 m-span roofs (upper town, heart, crown) and the `KG_BotHub` marker at the
  fountain. The square module places the ribboned Maypole (SW pocket, by the sea window). Without the pack
  (`KG_NO_TERRACE=1` or no manifest) every piece falls back to the kit version.
