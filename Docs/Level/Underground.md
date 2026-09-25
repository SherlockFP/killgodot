# Morrowmere v2: the underground (well cellar, old mine tunnel, catacombs)

> **Özet (TR):** Köyün tam altında, aynı haritada, 2 m ızgaralı bir yeraltı: Eski Kuyu'nun altında **Kuyu Kileri**
> (fıçı deposu, Kaçakçı Köşesi, gizli maden tüneli), Taç Tepesi'nin altında **Katakomplar** (kemik nişli koridorlar,
> Kript Salonu, Kemikhane, Kript Şapeli, anahtarla açılan Hazine Odası) ve ikisini bağlayan **eski maden tüneli**
> (Taç Tepesi ↔ Well Court gizli kısayolu). Terrain tek mesh olduğu için delinmiyor: kuyu ağzı ve mozole kapısı
> `AKGPassage` geçitleri (kısa karartma + ışınlama). Oynanış kuralları `01_GDD_Core.md §16`.

**Data:** `Tools/Level/underground_layout.py` is the single source (grid map, cell styles, regions, stair, gate).
`python Tools/Level/underground_layout.py` writes `Tools/Level/underground_layout_v2.json` and checks every cell stays
at least 1.5 m under the v2 terrain (`Art/Packed/KG_Terrain_v2_heights.json`): currently 101 cells, 0 too shallow.

## Frame
- Grid: 13 × 34 cells of 2 m, origin x0 −46.13, y0 −75.84 (UE metres, +y = south). Cell (6, 30) is centred exactly
  under the Old Well (−33.13, −14.84); the crypt stair (cells (6, 5..7)) runs under the mausoleum (−33.23, −65.07).
- Floors at **z 1.8 m**: above the sea surface (the character swims whenever it is below the wave height, anywhere) and
  2.5 m (Well Court, z 8) to 8 m (Crown Hill, z 14) under the terrain. Ceilings: kit walls 3.12 m, crypt vaults +1.0 m,
  mine 2.7 m, the crypt stair 5.4 m, the well shaft 4.4 m.
- Below-ground volumes (38 boxes, one per row run) and the map regions live on `AKGUndergroundInfo` in the level.

## Rooms (map regions; layer 2 = toast, layer 3 = rooms inside)
| Region | Cells | What |
|---|---|---|
| Well Cellar (2) | W X B L | warm brick, timber ceiling, barrel racks, 3 breakable barrels, workbench, lanterns; the shaft (X) with the ladder up |
| Barrel Store (3) | B | racks and a stack of barrels half hiding the tunnel mouth (the hidden tunnel) |
| Smugglers' Nook (3) | L | the Smugglers' Chest: always 1 Crypt Key + `Cellar` loot |
| Old Mine Tunnel (2) | T | rough rock, timber frames, lanterns every 3 cells, rubble, mushrooms, 12 m winding |
| Broken Wall (2) | J | where the miners broke into the crypt: rubble, a pickaxe, chain |
| Catacombs (2) | C G H S O P V | cool blue-grey stone, bone-niche walls, barrel vaults, candles on every other cell |
| Crypt Hall (3) | H | candle stands, skulls, 3 breakable urns (`CryptUrn`) |
| Mausoleum Stair (3) | S | `SM_KG_CryptStair`: 12 steps up to the door under the mausoleum |
| Ossuary (3) | O | skull walls all round, skull piles |
| Crypt Chapel (3) | P | the sarcophagus between candles, a cold light |
| Treasure Vault (3) | V | behind the iron gate (`AKGKeyGate`, Crypt Key): The Crypt Hoard (`CryptVault` loot), coin piles |

## Entrances (`AKGPassage`, `Source/KillGodot/Dig/KGPassage.h`)
| Id | Where | E / trigger | Arrives |
|---|---|---|---|
| WellTop | the Old Well's rim (new `SM_KG_WellMouth` on the kit well) | "Climb down the well" | top of the shaft ladder, facing it (falls onto the ladder) |
| WellShaft | the top 1.4 m of the shaft | auto: climbing **up** into it | beside the well, facing away |
| MausoleumDoor | the mausoleum's door recess | "Descend into the catacombs" | the stair landing, facing down |
| CryptDoor | the door at the top of the crypt stair | "Climb out through the mausoleum" | in front of the mausoleum steps |
Owner gets a 0.8 s fade from black; everyone near either end hears the door / ladder.

## Build
- Props: `Tools/Blender/kg_make_dress_underground.py` → `Art/Packed/KG_DressUnder_Clean.glb` (24 props: dig holes,
  graves, crypt/mine modular kit, well shaft, stair, gate, skulls, candles) → `/Game/KillGodot/Env/Dress/KG_DressUnder_Clean`.
- Assets in one commandlet: `Tools/Unreal/kg_import_dig_assets.py [audio,pack,arms]`.
- Map layer: `python Tools/Level/render_underground_map.py [--preview]` → `Art/Textures/Map/T_KG_Map_Underground_v2.png`
  + `KG_MapRegions_Underground_v2.json` (same bounds as the village map; the village drawn as a dim ghost underneath).
- Level: `Tools/Unreal/kg_build_underground.py` (commandlet, standalone: load, rebuild, save; idempotent via the
  `KG_Underground` tag). The village builder calls it after its report (one hook line in `kg_build_village_v2.py`), so a
  full `kg_build_v2_all.ps1` run rebuilds it; then step 6 (navmesh) covers it. ~420 pieces, 28 lights, 44 gameplay actors.
- Look: kit walls/floors with dark tinted MIs (`/Game/KillGodot/Env/KG_Village/Materials/Underground/`, crypt = cool,
  cellar = warm; the plaster face of the kit wall is re-skinned as stone), `AKGFlickerLight` candles/lanterns (no
  shadows, radius ≤ 7 m, flicker only near a camera), `AKGUndergroundInfo` blends in an unbound post-process while the
  camera is below (exposure −1.25 vs the village +0.4, vignette, cooler grade) and a 2D ambience loop with drips.
- Renders: `powershell -File Tools/Unreal/kg_capture_dig.ps1` → `Saved/Screenshots/Dig/*.png` (13 underground shots,
  2 surface entrances, 9 dig-spot shots via `-KGDigShots`).

## Checks
- Tests: `KillGodot.Dig.*` (Rules, SpotGeneration, LootDeterminism, ServerValidation, Underground) inside `run_gates.ps1`.
- Network: `Tools/Unreal/kg_dig_smoke.ps1` (listen server + client, -nullrhi): shovel out, 3 mound stages replicate,
  loot in the pockets, E on the well rim, both machines see the client in the Well Cellar. Part of invariant I3.
- Walkability: `Tools/Unreal/kg_underground_walk.ps1 [-Map ...]` (-game -nullrhi, read-only): a character capsule on a
  25 cm grid over every cell, flood-filled from the shaft ladder and from the crypt stair foot; PASS = every room but
  the vault reached, both sides meet through the old mine tunnel, and the vault is reached after `kg.Dig.Gate open`.
  When the real level must not be touched (editor open), `kg_build_underground.py --check-level` builds the
  underground alone into the scratch map `/Game/KillGodot/Maps/Dev/L_KG_UndergroundCheck` and the check runs there.

## Status (2026-09-25, sprint finish)
- The v2 pipeline hook works: `kg_build_v2_all.ps1 -From 4 -To 8` rebuilt the village with the underground inside
  (418 pieces, 28 lights, 44 gameplay actors), dressing, navmesh, material check (PASS) and captures; `verify_v2_build.py`
  PASS; dig smoke PASS on that map.
- The first walk check found 4 blockers in that build (the workbench in the shaft arch, an urn in the hall's west arch,
  a rock in the east-west run of the mine tunnel, the barrel pile closing the tunnel mouth). Fixed in
  `kg_build_underground.py` and verified PASS on the check map. **The live L_Morrowmere_v2 still has the old props until
  the next level build** (queued while the editor is open): `kg_build_v2_all.ps1 -From 4 -To 7`, then
  `kg_underground_walk.ps1` and `kg_capture_dig.ps1`.
