# Morrowmere v2: build report ("The Amphitheatre Cove")

**Status (2026-09-24):** built headless as a new level, `/Game/KillGodot/Maps/L_Morrowmere_v2`, with the editor closed.
- `L_Morrowmere` and every v1 script are untouched.
- The v1 builder is reused read-only: `kg_build_village_v2.py` execs `kg_build_village.py` up to `new_level()`, the same trick `kg_refurnish.py` uses.
- All checks pass: `validate_layout`, `verify_v2_build`, and the real-navmesh path check (79/79).

> **Özet (TR):** v2 haritası editör kapalıyken, tamamen komut satırından kuruldu: `L_Morrowmere_v2`.
> - Arazi: 0,5 m çekirdek ızgara.
> - Kasaba: 20 ev, 36 dolgu kabuk, 6 iç mekânlı kamu yapısı, balık hali, 3 merdivenli kule.
> - Mimari: istinat duvarları, 8 kit merdiven (görünmez rampa çarpışmalı), iskele, mendirek, köprüler, dere.
> - Bahçeler: sakura bahçesi, ada, kilise, mezarlık.
> - Oynanış: 22 görev, 20 başlangıç noktası, navmesh.
> - Doğrulama: tüm kapılar, görevler ve başlangıçlar çeşmeden gerçek navmesh üzerinde erişilebilir (79/79).
> - Menüde "Morrowmere: Amphitheatre Cove" olarak seçilebilir; v1 varsayılan ve yedek olarak kaldı.
>
> Editör açılınca kontrol edilecekler aşağıda (§4).

![renders](v2_Build_Renders.png)

## 1. How to rebuild (all headless, no window)

```powershell
powershell -File Tools/Unreal/kg_build_v2_all.ps1          # steps 1-7 (5 = dressing), about 10 min
python Tools/Level/verify_v2_build.py                      # numeric checks -> Saved/KG_V2_Verify.json
```

| # | Step | Tool | Output |
|---|---|---|---|
| 1 | Terrain | `Tools/Blender/kg_build_terrain_v2.py` (Blender CLI; also `python ... --preview x.png` without Blender) | `Art/Packed/KG_Terrain_v2.glb`, `KG_Terrain_v2_heights.json` |
| 2 | Import | `Tools/Unreal/kg_import_terrain_v2.py harbour` (commandlet) | `/Game/KillGodot/Env/Terrain/KG_Terrain_v2/…` and the `KG_DressHarbour_Clean` prop pack (first import) |
| 3 | Placements | `Tools/Level/prep_v2_placements.py` (numpy + shapely; UE Python has neither) | `Art/Packed/KG_V2_Placements.json` |
| 4 | Village | `Tools/Unreal/kg_build_village_v2.py` (commandlet, `-nullrhi`) | `L_Morrowmere_v2.umap`, `Saved/KG_V2_BuildReport.json`, minimap |
| 5 | Navmesh | `ResavePackages -BuildNavigationData` plus the `-ini:` override `bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False` | navmesh saved into the map |
| 6 | Capture | `Tools/Unreal/kg_capture_v2.ps1` (`-game -RenderOffScreen -nosound`, GameModeBase) | `Saved/Screenshots/V2/*.png`, `Saved/KG_V2_NavCheck.json` |

**Headless gotchas found on the way**
- `EditorActorSubsystem.spawn_actor_from_object` spawns nothing in a commandlet. Every mesh is therefore spawned as a StaticMeshActor from its class, and `kg_interiors._actors` gets the same shim.
- In a commandlet, `RebuildNavigation` is blocked by the editor world's `AsyncLoadLock`, which is only released by editor ticks. The ini override in step 5 fixes it.
- A commandlet `SceneCapture2D` renders only fog. `-ExecutePythonScript` is editor-only, but `-ExecCmds="py <path without spaces>"` works in `-game`.
- `AutomationLibrary.take_high_res_screenshot` does nothing in `-game`; the `HighResShot WxH filename=` console command works.

## 2. What was built

**Terrain** (`SM_KG_TerrainV2_Core_00..11` + `_Outer`, 318k verts)
- **Core:** 260 × 250 m at a 0.5 m step, split in 2 × 2 tiles.
- **Outer ring:** 640 m at 2.5 m.
- **Terraces:** flat at quay 2 / Heart 5 / garden and Upper 8 / Crown 14.
- **Crisp steps:** high-side vertices within one cell are snapped onto every retaining-wall, quay-edge and cliff line, so each step is a vertical face hidden by the 0.64 m kit wall.
- **Slope terraces:** Brookside, Orchard and Headland are graded to their profiles and blended toward flat neighbours, never across a wall. The windmill knoll is included.
- **Stairs:** the tread line minus 5 cm, so terrace z matches z0/z1 at every stair end (logged).
- **Ramps:** graded, with 3 m shoulders.
- **Water:**
  - the round basin at −3;
  - the mole, which rises from the quay at 2.0 to 2.5 over 6 m;
  - the sea floor, shelving at beaches and cut hard at the cliffs and quay;
  - the brook bed and banks, and the mill pond.
- **Pads:** buildings on slopes get pads.
- **Rim:** a mountain ring lifted at the grid edge on the land sides.
- **Look:** vertex colours as in the plan, with the same `MI_KG_Terrain` material.

**Architecture**
- **Homes:**
  - 20 homes, with a KGDoor, doorbell, lantern and light, and a `kg_interiors.furnish` interior;
  - each home's chest has **HouseIndex = home_index 0..19**;
  - 1–3 storeys, with the third storey a ceilinged attic.
- **Infill:** 36 shells, exterior only, with a static flat door.
- **Party walls:** 26 shared sides are skipped. When one unit is taller, the upper wall is kept.
- **Roofs:** gable to the street; 6 m units alternate their ridge inside a block.
- **Civic with interiors:** Town Hall, Inn, Bakery, Boathouse, Church and Barn (`kg_interiors` room plans 20–25).
- **Open halls:** the Fish Market arcade (kept under its `top_z`, so the S4 view clears it) and the smithy shed.
- **Towers:** Bell (4 levels), Clock (4) and Lighthouse (5, with the 400 cd beacon). Each has a KGLadder to the lookout floor, where the RingBell, WindClock and FuelLighthouse stations sit.
- **District tints:** 32 `MI_KG_<district>_<Plaster|UnevenBrick|RoundTiles|WoodTrim>` children of the kit's glTF MIs, overriding `BaseColorFactor`, in `/Game/KillGodot/Env/KG_Village/Materials/District/`.
  - Harbour Row stucco comes in 4 pastel variants.
  - Upper Town is ochre half-timber; Crown is blue-grey stone.
  - Sakura timber is vermilion; Orchard trim is barn red.
- **Retaining walls:**
  - 13 runs plus the quay wall: 404 wall pieces;
  - Upper-Terrace pilasters at kinks and every 8 m;
  - 135 parapet pieces (skipped at buildings and stair gaps);
  - caps on the walls without a parapet;
  - 2 courses of quay wall, with gaps at the jetty and fish-pier ramps;
  - bollards every 8 m and 2 water-step flights.
- **Stairs:**
  - 8 kit stairs, 54 riser pieces (L / Center / R across the width);
  - brick landings, with planters on the Grand Stair;
  - **16 hidden ramp boxes** (one per flight, BlockAll, hidden in game);
  - kit risers have no collision;
  - cheek walls where a stair is cut into, or raised over, a terrace; ramps get side walls the same way.
- **Street joins:**
  - 4 sottoportego bridge rooms over the opes: arches centred on the ope, windowed walls above, a floor and soffit, and a roof;
  - 4 garden walls, each with an open gate.

**Landmarks and set pieces**
- **Fountain Square:** the dressing-pack fountain, v1 gallows turned to face the fountain (TargetPoint `KG_Gallows`), notice board, Dead Tree, 3 market stalls, 4 benches, planters at the Town Hall steps, bunting to the Inn and 4 corner lamps.
- **Upper Town:** the well and the Old Oak in Well Court.
- **Crown:**
  - a graveyard of 5 rows of dressing gravestones with an iron fence and the GateArch lychgate;
  - the mausoleum (`SM_KG_Mausoleum`);
  - the church and the bell tower.
- **Sakura Garden:**
  - the koi pond (rim, water, arch bridge, koi school), garden torii on x = 45, and the pavilion (`SM_KG_TeaHouse`);
  - the giant sakura, bamboo, lanterns;
  - the koi spill: a channel plus a curtain down the Sakura Wall.
- **Sea front:**
  - the shrine island and pagoda, with the sea torii moved onto the axis;
  - the Long Jetty + T-head and the fish pier (pirate pier sections, each with a **hidden walkable deck box**; the pier meshes give nav nothing to stand on);
  - timber ramps and jetty torches;
  - the mole: flag tiles, rock armour, and the harbour light (green, 12 cd);
  - the bell buoy, the cargo hoist, 4 sea stacks off the Point, 6 moored rowboats and the boathouse slipway.
- **Brookside:**
  - Old Stone Bridge (deck, parapets, hidden deck collision), 2 WoodBridges, ford stones;
  - brook water ribbons that follow the bed and a mill-pond plane;
  - the waterwheel: a KGSpinner hub with 6 attached paddles;
  - the smithy and the woodcutter.
- **Orchard Upland:**
  - the windmill (the dress_countryside recipe, with sails on a KGSpinner);
  - walled fields (carrots, wheat, cabbages) with gates, and the apple orchard;
  - the pen, the farmyard and the barn.
- **Lighting:** sun, sky, clouds, fog and the look PP (v1 functions).
- **Life and sound:**
  - the ocean and sea life (fish schools, gull flocks);
  - 19 ambient sounds;
  - 21 breakables.
- **Vegetation:**
  - KGGrassField meadow of 21.6k clumps (short grass in town gardens);
  - KGFoliageField groves (clustered, framing N / NW / NE, none inside the town terraces or sightline corridors) and an outer pine ring: 2189 instances;
  - a crops field of 1703 instances;
  - 36 street lamps at stair heads, ope ends and junctions.
- **Gameplay:**
  - 22 task stations with hint props (UnloadFish and LightHarbourLamp stand on the deck);
  - 20 PlayerStarts on the 7.5 m ring (moved clear of stalls and benches);
  - the NavMeshBoundsVolume from `nav_bounds` and the navmesh;
  - `KG_CaptureCam`, a movable camera used only by the capture tour.
- **Minimap:** the coordinator's `kg_make_minimap.build_minimap(layout_v2)` hook runs at the end of the build. It produces `T_KG_Map_Morrowmere_v2` (with the real trees), plus AKGMapInfo with 96 regions.

**Numbers:** 7,115 actors in total.
- 6,729 StaticMeshActors, of which 1,790 are interior pieces.
- 141 lights.
- 26 KGDoors, 90 seats, 20 chests, 3 ladders.

**Menu:** `KGMenu::GetMaps()` gets a second entry, "Morrowmere: Amphitheatre Cove", pointing at `/Game/KillGodot/Maps/L_Morrowmere_v2`.
- L_Morrowmere stays first, as the default and the fallback.
- This is code, not data, because the list is hard-coded in `KGMenuActions.cpp`; there is no map config.
- Making v2 the default means swapping the two entries.
- Gate G1 (2026-09-24 18:0x): `KGMenuActions.cpp` compiled without errors. The link failed only on `Emote/KGEmoteSubsystem.cpp(159)`, which the emote agent was editing at that time (it needs an `AGameStateBase` include). Rerun `run_gates.ps1` once that lands. Until the editor DLL is rebuilt, the v2 entry does not appear in the menu. The map itself loads through the URL: `open /Game/KillGodot/Maps/L_Morrowmere_v2`.

## 3. Verification

| Check | Result |
|---|---|
| `validate_layout.py` (overlaps, doors ≤ 3 m, stairs, ramps, walls, network, 2 routes per district, coverage 22.6 %, 7 sightlines in 3D) | PASS |
| Wall pieces inside a building footprint or across a door apron | 0 of 604 |
| Step at each door (terrain in front vs pad) | 0.00 m max |
| Doors reaching the fountain on the walk graph | 26/26, max 24.6 s |
| Stairs | 54/54 risers, 16/16 hidden ramps |
| **Real navmesh** (`find_path_to_location_synchronously` from the fountain) | **79/79**: every home and civic door, the tower doors, 19 ground chores, 20 starts, jetty / fish pier / mole probes |
| Missing meshes, failed build steps | none |

**Renders** are in `Saved/Screenshots/V2/*.png` (1600×900), with a contact sheet at `Docs/Level/v2_Build_Renders.png`. They show:
- top and oblique aerials;
- the 7 designed sightlines S1–S7;
- the plaza, the Long Ope, the quay, Balcony Lane, Brookside and the Crown.

**Read against the plan:**
- **S1 Postcard:** the gatehouses frame the jetty leading line to the torii and pagoda.
- **S2 Pilgrim:** the jetty looks up the Grand Stair to the square.
- **S3 Belvedere:** the stepped axis runs down to the fountain, the clock tower and the island.
- **S4 Garden axis:** it clears the lowered fish-market roof.
- **S5:** the basin opens with the lighthouse above the Sakura Wall.
- **S7:** the windmill sits at the end of Orchard Lane.
- **The plaza:** fully enclosed, with the Scala climbing on the axis and the bell tower above it.

Two problems were found in the images and fixed during the build: the outer terrain's winding (it was back-face culled, so the sea showed past the hills), and the fish-market roof blocking S4.

## 4. Known gaps and what to check in the editor

1. **Open the map once and save it.**
   - Confirm that the navmesh (green overlay, `P`) covers the stairs through the ramp boxes, the jetty decks and the mole.
   - The minimap texture and the district MIs should load without warnings.
2. **Stair kit orientation.** Look at the Grand Stair, Scala and Pilgrim Stair close up.
   - Is `_L`/`_R` mirrored? The side trims would face inward.
   - Do the ramp boxes sit just over the treads?
   - Fix it in `prep_v2_placements.riser_row` if needed.
3. **Prop facings** that could not be verified from the kit data:
   - the market stalls, pavilion TeaHouse, graveyard GateArch and WoodBridges (arch height versus the bank);
   - the waterwheel paddles, which spin in PIE only.
   - Each is one yaw or z constant in `kg_build_village_v2.py`.
4. **Basin calm mask:** the ocean waves run full height inside the basin. The plan scales them ×0.25; that needs the M_KG_Ocean material and FKGWaves (C++).
5. **Brook water:** `M_KG_PondWater` reads pale, almost like a path, from a distance. It needs a darker, flowing MI.
6. **Roof tints:** the kit's orange tile texture limits what a multiply can do, so "slate-blue" Crown roofs come out dark brown. Real slate needs a desaturating MI or a texture swap.
7. **Not yet built:**
   - dormers on 3-storey units;
   - emissive window cards on shells;
   - the lighthouse beam;
   - clock faces and the bell;
   - the Belvedere telescope and benches;
   - cypress rows and lavender;
   - the Balustrade, RetainingWall, QuayWall, StoneArchBridge and HarbourLight props from §11.4. The kit fallbacks are used instead.
8. **Bots:**
   - `KGBotController` wanders around the v1 plaza constant (0, −8 m, r 17 m). In v2 that falls on the Town Hall / square area, so it roughly works, but it should read the level, for example the `KG_Gallows` marker or AKGMapInfo.
   - A 20-bot meeting test on v2 is still to be done.
9. **Dressing:** done (§5). Still open: dormers, clock faces, the §11.4 props and the basin calm mask.
10. **Lighting:** the level uses the v1 golden-hour setup. The capture tour runs under GameModeBase; the game's day/night phases drive the sun in real matches.

## 5. Dressing (plan §11.3): done 2026-09-24

The dressing runs headless as build step 5 (`Tools/Unreal/kg_dress_v2.py`, using `kg_dress_common_v2.py` and
`Tools/Unreal/dressing/v2/`). It covers all nine zones plus a town-wide window pass.
- Totals: 272 actors, 4,666 HISM instances, 27 lights, 44 seats, 93 movers, 28 breakables, 7 loot chests.
- Also added: cobbled streets and squares, lit shell windows (M_KG_WindowGlow_v2, stronger at night), the lighthouse
  beam (spinning cones, night only), calm dark brook water (M_KG_BrookWater_v2), slate Crown roofs.
- Checks: `verify_v2_build.py` PASS, dressing clearance "all clear", navmesh 79/79.
- Renders: `Docs/Level/v2_Dressed_Renders.png`.

### Migration plan (as originally written)

Keep `dress_*.py` and `kg_dress_common.py` working on v1 until every zone has been ported. Then:

1. **A v2 context in `kg_dress_common`.** Add `LAYOUT_FILE` and `LEVEL` switches, picked by the open level's name (`L_Morrowmere_v2` means v2).
   - `ground()` reads `KG_Terrain_v2_heights.json`, core grid first; copy it from `kg_build_village_v2.ground`.
   - `zone_of()` becomes the ordered point-in-polygon test over `LAYOUT["zones"]`.
   - `buildings()` iterates houses + infill + sized landmarks with `.kind`, `.district` and `.door`.
   - `reserved()` gains the stair and ramp corridors, the arches and a 1 m strip in front of every retaining wall. The corridors can be read straight from `KG_V2_Placements.json` (`V2/Stairs*`, `V2/*Walls`).
   - New helpers: `anchors(zone)`, `terrace_z()`, `in_garden()`.
2. **`kg_dress.run(level=...)`.** `_ensure_level` loads v1 or v2 and clears `Dress/<Zone>` in that level only. The v2 builder then calls `kg_dress.run(save=False, nav=False, restore=False)` before `task_stations()`, as v1 does, followed by the nav step.
3. **Port the modules one at a time,** replacing hard-coded v1 coordinates (about 775 literals) with anchors. Capture each one with `kg_capture_v2.ps1`, then move on. Order:
   1. `dress_square`: fan-square polygon, the east-flank stalls, the town_hall / inn / bakery / clock_tower names.
   2. `dress_harbour`: `quay.edge`, `long_jetty`, `mole.points`, fish market, boathouse. Delete the beach-strip logic.
   3. `dress_streets`: ring lanes by name, the back gardens (`in_garden`), a dark Long Ope, geraniums on Balcony Lane.
   4. `dress_church`: the graveyard polygon, Belvedere telescope and benches, cypress rows.
   5. `dress_japan`: from the koi_pond / garden_torii / pavilion / giant_sakura anchors. The island is already on the axis.
   6. `dress_countryside`: `fields[]`, windmill, pen, farmyard. The windmill and field walls exist already, so skip or replace them.
   7. `dress_coast`: Headland Road and the lighthouse.
   8. `dress_wilds`: groves only outside the terraces. The builder already places the base forest, so wilds adds points of interest.
   9. New `dress_beckside`: reeds and willows on the banks, the watermill race, tannery vats, driftwood. Register it in `kg_dress.zones()`.
4. **Budgets stay as they are:** ≤ 900 actors per zone, HISM for repeats, ≤ 10 lights. After each zone, rerun `verify_v2_build.py` and the navcheck, because dressing must not block doors, stairs or chores.

## 6. Files

**New**
- `Tools/Blender/kg_build_terrain_v2.py`
- `Tools/Unreal/kg_import_terrain_v2.py`
- `Tools/Level/prep_v2_placements.py`
- `Tools/Unreal/kg_build_village_v2.py`
- `Tools/Unreal/kg_capture_v2.py`, `Tools/Unreal/kg_capture_v2.ps1`
- `Tools/Unreal/kg_build_v2_all.ps1`
- `Tools/Level/verify_v2_build.py`
- `Docs/Level/v2_Build_Report.md`, `Docs/Level/v2_Build_Renders.png`

**Generated**
- `Art/Packed/KG_Terrain_v2.glb`, `KG_Terrain_v2_heights.json`, `KG_V2_Placements.json`
- `Content/KillGodot/Maps/L_Morrowmere_v2.umap`
- `Content/KillGodot/Env/Terrain/KG_Terrain_v2/`
- `Content/KillGodot/Env/KG_Village/Materials/District/` (32 MIs)
- `Content/KillGodot/Env/Dress/KG_DressHarbour_Clean/` (the existing pack, imported for the first time)

**Edited**
- `Source/KillGodot/UI/Menu/KGMenuActions.cpp`: the second map entry, the only C++ change.
