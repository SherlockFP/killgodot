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
>
> **Cila turu (2026-09-25, §7):**
> - v2 artık varsayılan harita ("Morrowmere"); v1 "Morrowmere (Classic)" olarak seçilebilir.
> - Liman havuzu sakin: dalga ×0,25, küçük dalgacıklar ve yeşil ton. Yüzme ve şamandıra fiziği de aynı maskeyi kullanıyor.
> - Yeni Blender paketi `KG_DressTerrace`: taş korkuluk, granit rıhtım duvarı ve suya inen merdiven, taş kemer köprü, mendirek feneri, 4 saat kadranı.
> - Ayrıca 84 çatı penceresi ve meydanda bir Maypole.
> - Botlar haritayı okuyor: 20 bot tüm bölgeleri geziyor ve toplantı için en geç 33 sn'de meydanda.
> - Instanced prop malzeme uyarıları sıfırlandı.

![polish](v2_Polish_Renders.png)

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

**Menu (superseded 2026-09-25, see §7.4):** v2 is now entry 0, "Morrowmere" (the default); v1 is "Morrowmere (Classic)".
Originally `KGMenu::GetMaps()` got a second entry, "Morrowmere: Amphitheatre Cove", pointing at `/Game/KillGodot/Maps/L_Morrowmere_v2`.
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
4. **Basin calm mask: done (§7.1).** Before the fix, the ocean waves run full height inside the basin. The plan scales them ×0.25; that needs the M_KG_Ocean material and FKGWaves (C++).
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
8. **Bots: done (§7.3).** Before the fix:
   - `KGBotController` wanders around the v1 plaza constant (0, −8 m, r 17 m). In v2 that falls on the Town Hall / square area, so it roughly works, but it should read the level, for example the `KG_Gallows` marker or AKGMapInfo.
   - A 20-bot meeting test on v2 is still to be done.
9. **Dressing:** done (§5).
   - Dormers, clock faces, the calm mask and most §11.4 props were added on 2026-09-25 (§7).
   - Still open: see §7.7.
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

## 7. Polish pass: done 2026-09-25

All work was headless: Blender CLI, commandlets, the offscreen `-game` capture and `-nullrhi` sessions.
- **Rebuild:** `kg_build_v2_all.ps1` now has 8 steps (7 = material check, 8 = capture) and takes about 8 min.
- **Checks:**
  - `verify_v2_build.py`: **PASS**.
  - Navmesh: **79/79** targets reached.
  - `run_gates.ps1`: **PASS** (build + 38 tests).
  - Chat / emote / fish / chore smokes: **PASS**.
  - v1 `L_Morrowmere` still loads in `-game`, and its bots use its navmesh (30 anchors).

### 7.1 Calm harbour basin
- **One source of truth.** `AKGMapInfo` has new Water properties: `bCalmWater`, `CalmCentre`, `CalmRadius`, `CalmFade`, `CalmWaveScale`, `CalmRipple`, `CalmTint`, `CalmTintAmount`.
  - At register and at BeginPlay it pushes them into `FKGWaves::Calm()`, which drives swimming, buoyancy and the underwater check.
  - It pushes the same values into the new `MPC_KG_Water` (vectors `CalmZone`, `CalmParams`, `CalmTint`), which `M_KG_Ocean` reads.
- **Same maths in C++ and in the material.** `KGWaves.h` and `kg_make_ocean.py` use identical formulas:
  - calm weight w: a smoothstep over `Fade` inside `Radius`;
  - height: swell × lerp(1, 0.25, w), plus w × 2.5 cm of two-train ripples.
- **Material only:**
  - ripple normals broken up by a slow gust mask;
  - a 45 % teal tint;
  - less crest foam and crest colour.
- **Values:**
  - v2: centre = `basin.center`, radius 34 m (the inner toe of the mole), fade 10 m.
  - v1 leaves `bCalmWater` off, and the MPC defaults to radius 0, so the open sea is unchanged.
- **Rebuild:** `kg_make_ocean.py rebuild` rebuilds the graph as a commandlet. Never run it in a live editor.

### 7.2 Plan §11.4 props: the `KG_DressTerrace` pack
- **Pack:** `Tools/Blender/kg_make_dress_terrace.py` writes `Art/Packed/KG_DressTerrace_Clean.glb` + `.json`.
  - Previews: `Art/Concept/DressTerrace_preview*.png`.
  - Style: vertex colours, Quaternius look, stone matched to the fountain.
- **Import:** build step 2 runs `kg_import_dress_pack.py KG_DressTerrace_Clean`. The pack name can now also be passed as the commandlet argument.
- **Swapped in at the source** in `prep_v2_placements.py` (`DT:` prefix). The kit fallback returns if the manifest is missing or `KG_NO_TERRACE=1`.
  - 73 `Balustrade_2m` + 32 `Balustrade_Post` on the harbour walls (the sea window over the Grand Stair) and the crown walls (the Belvedere).
  - 14 `QuayWall_4m(_Ring)` sections on 4 m chords of the quay edge. The sea face sits 0.4 m out, so the terrain step stays hidden.
  - 2 `QuaySteps` flights down to the water.
  - `StoneArchBridge_7m` for the Old Stone Bridge. Its hidden flat deck box is kept for nav.
- **Builder** (`kg_build_village_v2.py`):
  - `HarbourLight` at the mole tip, with a green 14 cd light;
  - 4 `ClockFace` dials on the clock tower's third storey;
  - 84 kit `Roof_Dormer_RoundTile` dormers on exposed 6 m-span roofs (upper town, heart, crown), fitted from the kit geometry;
  - the `KG_BotHub` TargetPoint at the fountain;
  - the calm-water setup.
- **Dressing:**
  - The ribboned Maypole (DressVillage pack) stands in the square's south-west pocket by the sea window.
  - Towers no longer get flower boxes above the first floor, so the dials stay clear.
  - Knee-high shrubs are walk-through. As colliders they made navmesh pockets between garden fences.

### 7.3 Bots read the level (`AKGBotController`)
- **Anchors:**
  - Sources: `KG_BotHub`, the task stations, PlayerStarts, optional `KG_BotSpot` tags, and the street / place / district regions of `AKGMapInfo`.
  - Each is projected to the navmesh and kept only if a full path to the hub exists. v2 keeps 81 of 98.
- **Roaming:** navmesh moves; 65 % of trips go to an anchor within 45 m, the rest anywhere. Stairs are climbed via their hidden ramps.
- **Chores:** the nearest open chore, with per-bot jitter. Chores a bot cannot reach are skipped for a while.
- **Meeting run-up:**
  - In the last 30 s of a day that ends in a meeting, or with `kg.BotGather 1`, every bot walks to a loose 4-9 m ring round the fountain.
  - The gallows meeting ring now opens toward `KG_BotHub`. v1 still uses +Y.
- **Stuck handling:**
  - A bot is stuck after less than 0.8 m of progress in 4 s. It opens a door ahead if there is one; otherwise it side-steps with a hop and repaths.
  - After 4 stuck events in the same place, the dev bot is put back on the nearest anchor. This is counted as a rescue.
  - Path failures are throttled. A swimming bot heads for the nearest low navmesh.
- **Counters:** `kg.BotStats [reset]`.
- **Levels without a navmesh** keep the old plaza wander.

**Soak test:** `powershell -File Tools/Unreal/kg_bot_test_v2.ps1` runs 20 bots in a `-nullrhi` game and writes `Saved/KG_V2_BotTest.json`.

| Phase | Result |
|---|---|
| Roam, 150 s before the match | All 8 districts plus the outskirts visited. Per bot (min / median / max): 3 / 5 / 9 districts, 193 / 373 / 428 m walked. 143 anchor arrivals. 7 stuck events, all self-recovered. 0 path failures. No bot stood still for 15 s or more. Every terrace level from 2 to 15 m was reached. |
| Meeting run-up to the fountain (`kg.BotGather 1`) | **20/20 arrived** within 10 m of the hub. Start distance: median 49 m, max 104 m. Arrival: median **21.3 s**, p90 32.9 s, max **33.4 s**. 1 rescue. |
| Meeting ring (`kg.Match.Phase Meeting`) | 20/20 bodies on the square, within 12 m of the gallows, at terrace height. |
| Day chores, 240 s, timer frozen | 44 chores done at 21 distinct stations. Average 26.9 s from pick to done, walking included. 13 stuck events, 3 path failures. The Impatient killed 10 villagers during this phase. |
| Navmesh paths | Hub + 20 starts to all 22 chores: **462/462 full paths**. Tower chores are checked to the tower door, because the ladder is not navmesh. Longest: FuelLighthouse, 139 m from the hub (about 23 s). |

The first soak run, before the door and unstick fixes, logged 107 stuck events and 7,197 path failures, mostly bots against closed house doors. The final run logged 24 stuck events and no pathfinder spam.

### 7.4 v2 is the default map
- **Menu:** `KGMenu::GetMaps()` index 0 is now v2, "Morrowmere". Quick-play, `kg.Session host` and the lobby fall back to it. v1 stays selectable as "Morrowmere (Classic)".
- **Config:** `EditorStartupMap` is v2. `GameDefaultMap` stays `L_MainMenu`, the front end.
- **Dev tools:**
  - The fake session rows in `KGMenuDebug` point at v2.
  - `autoshot.ps1 -Map` defaults to v2.
  - The HUD map card strips `_vN`, so v2 shows as "MORROWMERE".

### 7.5 Instanced props in packaged builds
- **The problem:** the kit materials are children of Unreal's glTF material (`/InterchangeAssets/gltf/M_Default` via `MI_Default_*`), which lacks the instanced-mesh usage flag. HISM props therefore drew the default checker in `-game` and in cooked builds.
- **The fix:** `Tools/Unreal/kg_fix_gltf_materials.py`, a commandlet run as build step 7:
  - it copies the engine base material and the intermediate MIs to `/Game/KillGodot/Materials/glTF/`;
  - it sets bUsedWithInstancedStaticMeshes and bUsedWithNanite on the copy;
  - it reparents the 62 project MIs onto the copy;
  - it flags the project base materials the map needs.
- **Checked:** every material on the map's 383 (H)ISM components (35 materials, 29,455 instances) and on its Nanite meshes (61) has a ready base. `Saved/KG_V2_MaterialCheck.json`: **PASS**.
- **In the render tour:** "missing usage flag" warnings went from 17 materials to **0**.
- **Not done:** a full cook was not run.

### 7.6 Renders
- **Contact sheet:** `Docs/Level/v2_Polish_Renders.png`, with before/after pairs: basin ×3, quay steps, Grand Stair balustrade, bridge, harbour light, clock, dormers, Maypole, aerials.
- **Files:** the new shots are in `Saved/Screenshots/V2/pol_*.png`; the before set is in `Saved/Screenshots/V2_before/`.

### 7.7 Still open
- **§11.4 props not built:** RetainingWall_2m (the kit walls remain), Waterwheel, GateArch_Ope.
- **Missing details:** the bell, the Belvedere telescope, the lighthouse gallery. The clock hands are static.
- **Grand Stair flights:** they keep their kit cheek walls. A balustrade on a slope would need a raked mesh.
- **Dormers:** the kit mesh is fitted to the 6 m roof profile only, so 4 m roofs have none.
- **Bots:**
  - A wedged bot still needs about 1 rescue per run.
  - The Impatient kill freely by day in soaks, so the chore counts vary from run to run.
- **Not verified:**
  - a full cook of the map;
  - an in-editor look at the navmesh over the new quay steps and bridge.
