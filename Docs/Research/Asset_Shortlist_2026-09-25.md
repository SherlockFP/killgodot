# Asset shortlist for decorating Morrowmere v2 (and Storm Manor, forest, weather, boats, loot), 2026-09-25

Research only. **Nothing here has been downloaded.** Every download needs the user's explicit OK on the concrete list in
§5. Checks were made live on 2026-09-25:
- **Licences:** from each page (itch licence field, Kenney/OGA pages, poly.pizza `"Licence"` field, Sketchfab API, Poly Haven API).
- **Sizes:** from the itch upload list, HTTP HEAD `Content-Length` of the Kenney zips and of each poly.pizza `.glb`, and the Poly Haven files API.
- **Triangle counts:** from each poly.pizza model's `"Tris"` field, or from the Sketchfab API.

Anything not published is marked *measure*.

Inputs read: `Docs/Research/AssetResearch.md`, `Tools/Assets/approved_sources.json`, `Docs/Credits.md`,
`Tools/Unreal/dressing/asset_catalog.md` (+ `Art/Packed/KG_Dress*_Clean.json`), `SPRINT-022-V2VisualPass.md`.

## 0. What we already have, and the real gaps

**Already in hand:**
- Quaternius Medieval Village / Fantasy Props / Stylized Nature MegaKits.
- JayBee and Azrael68 furniture.
- zisongbr pirate props (piers, barrels, crates, chests, cannon, flags).
- The Blender-made dress packs:
  - Harbour: wreck, buoys, nets, lobster traps, bollards, fish racks.
  - Village: fountain, maypole, windmill, bunting, laundry, 5 hanging signs.
  - Landmarks.
  - Terrace: balustrades, quay walls, stone bridge.
  - Under.
  - Wilds: tent, campfire, tea house, gravestones.
  - Japan: torii, pagoda, sakura, lanterns, koi, noren stall.
- The only boat is `SM_KG_Rowboat`.

**Real gaps:**
1. Proper boats and ships (sail boats, a moored fishing boat, a distant ship, dock sections).
2. Market goods: fish, vegetables, bread, cargo, textiles. The stalls exist but look empty.
3. Japanese architecture pieces (shoji walls, doors) for the tea house and garden.
4. House and facade variety (SPRINT-022 wants at least 6 archetypes). Most of this is procedural work, not downloads (§4).
5. Countryside buildings (barns, silo, coop).
6. Forest camp and survival props.
7. Storm Manor graveyard pieces (iron fences, crypts, dead trees).
8. Weather: rain and splash sprites, and overcast, storm and night sky lighting.
9. Nature variety beyond the single Quaternius nature kit.

## 1. Ranked shortlist

The style-fit score (1–5) is against our look: Quaternius shape language, a vibrant palette, vertex colour through
`M_KG_PropVCLinear`.

| # | Name (author) | URL | License | Size MB | What it adds (district) | Perf notes | Style fit |
|---|---|---|---|---|---|---|---|
| 1 | **Quaternius boats + harbour picks** (Pirate Kit 2023 + Ships 2018, single models on poly.pizza) | https://poly.pizza/bundle/Pirate-kit-0q5ulmIYqQ · Sail Boat https://poly.pizza/m/BgSZXwmm7k · Boat https://poly.pizza/m/5UEl54KsuC · Lifeboat https://poly.pizza/m/Bkd4KKQA4O · Sail Ship https://poly.pizza/m/cIzO4MBPqI | CC0 1.0 | ~1.0 (11 glb) | **Harbour and coast:** a real low-poly boat set. It covers moored sail boats, rowboats and lifeboats on the quay and a Sail Ship anchored off the mole. Dock/Dock Broken add pier variety, and Anchor, Cannon, Barrel and Bucket of Fish are quay clutter. **Storm Manor:** boathouse. **Loot:** Chest Closed (1,636) / Coins (2,024) optional | Tris: Sail Boat 1,020 · Boat 224 · Lifeboat 626 · Sail Ship 3,302 · Small Ship 5,578 · Dock 1,912 · Dock Broken 2,052 · Anchor 544 · Cannon 2,362 · Bucket of Fish 3,860 (decimate to ≤1.5k) · Barrel 640. **Skip "Ship" (20,632 tris).** Ships 2018 = flat colours; Pirate Kit = small textured atlas → bake to VC | 5 |
| 2 | **Quaternius Modular Sushi Restaurant Kit, picks** | https://poly.pizza/bundle/Modular-Sushi-Restaurant-Kit-LJZrZsNPM7 · https://quaternius.com/packs/sushirestaurantkit.html | CC0 1.0 | 0.7 (21 glb) | **Japan district:** Shoji Wall, Shoji Interior, Japanese Door, Red Wood Wall and Arch give the tea house and garden pavilions real Japanese walls, plus low Table/Bench/Stool. **Harbour fish market:** Dead Tuna, Flounder, Mackerel, Salmon, Eel, Octopus and Squid on the stalls, plus Wooden Board, Bowl and Pots | 21 picks = 13,676 tris. Walls 106–444, fish 672–1,876 (decimate the Flounder/Squid/Octopus to ≤700), cull 5,000 cm. Skip the sushi truck (14k), fridge and oven | 5 |
| 3 | **KayKit Resource Bits (free tier)** (Kay Lousberg) | https://kaylousberg.itch.io/resource-bits | CC0 | 8.1 | **Market, harbour cargo, loot:** 75+ pieces: wood/plank stacks, stone blocks, iron/copper/silver/gold ingots, textile bolts, fuel. Cargo on the quay, the blacksmith yard, cloth stalls, loot-drop meshes | One 1024² gradient atlas (downsamples to 128²). Sample the atlas → VC and drop the texture. Low-poly "Bits" (mobile-targeted) | 4 |
| 4 | **Quaternius Ultimate Food Pack, picks** | https://poly.pizza/bundle/Ultimate-Food-Pack-h3WC1gyRb4 | CC0 1.0 | 0.5 (18 glb) | **Market stalls, kitchens, loot food:** bread, bread slice, croissant, apple, tomato, eggplant, pepper, carrot, turnip, lettuce, broccoli, pumpkin, banana, egg, steak, chicken leg, cooking pots. Skip modern items (burger, soda, pizza, ketchup) | 18 picks = 7,850 tris (132–976 each). Flat colours → direct VC. Merge a stall's goods into one "StallGoods_X" mesh per variety (1 draw) | 4 |
| 5 | **Kenney Survival Kit 2.0** | https://kenney.nl/assets/survival-kit (mirror https://opengameart.org/content/survival-kit) | CC0 | 1.9 | **Forest (SPRINT-033/034), wilds camp:** 80 models: tents (full/half/frame), campfire, bedrolls, workbenches, fishing stand, signposts, fences, box/barrel/chest, fallen trees, tools | Tiny pieces (the older version on poly.pizza is 44–641 tris each; v2 is a remake: *measure*). Kenney's 2020s kits ship one small colormap PNG (*confirm at pack step*) → VC | 3 |
| 6 | **KayKit Halloween Bits (free tier)** | https://kaylousberg.itch.io/halloween-bits | CC0 | 5.3 | **Storm Manor, church graveyard, wilds:** 60+ pieces: iron fence and gate (plus damaged versions), arch gate, crypt, gravestones and markers, coffins, dead trees, autumn pines, lanterns and post lanterns, candles, shrine, cobble/path tiles. Skip the pumpkins on Morrowmere | poly.pizza copies: 56–1,496 tris (most < 500; one coffin variant 2,064). 1024² gradient atlas → VC | 4 |
| 7 | **Quaternius Farm Buildings** | https://quaternius.itch.io/lowpoly-farm-buildings · https://poly.pizza/bundle/Farm-Buildings-Bundle-ppbnhEfNEt | CC0 | 3.4 (itch) / 1.0 (10 glb) | **Countryside:** Barn, Big Barn, Small Barn, Open Barn, Silo, Silo House, Chicken Coop, Tower Windmill, 2 fences. Farmyard variety beyond the MegaKit homes | 188–4,464 tris. Flat colours → direct VC. Buildings: cull none, box/complex collision | 4 |
| 8 | **Kenney Fantasy Town Kit 2.0** | https://kenney.nl/assets/fantasy-town-kit (mirror https://opengameart.org/content/fantasy-town-kit) | CC0 | 3.9 | **Facade and roof variety, market:** 160+ modular walls, roofs, stalls, fountain, lanterns, banners and carts, as extra roof forms, porches and stall canopies for SPRINT-022 archetypes | Low-poly, one colormap (*confirm*). **Risk:** Kenney's crisp toy geometry vs MegaKit bevels. Use only for roofs, dormers, awnings and stalls on whole districts, never mixed wall-by-wall. Kit scale must be uniformly rescaled to the 300 cm storey | 3 |
| 9 | **KayKit Forest Nature Pack (free tier)** | https://kaylousberg.itch.io/kaykit-forest | CC0 | 6.1 | **Forest (bigger forest per Backlog), wilds:** 100+ trees, bushes, rocks and 2 grass styles. Mid- and far-layer fill so the forest is not one repeated Quaternius tree | 1024² gradient atlas → 128² or VC. Cheapest trees on the list. **Risk:** rounder cartoon crowns than Quaternius. Keep them in the forest's back rows (≥ 30 m from paths), never next to MegaKit trees | 3 |
| 10 | **Quaternius Medieval Village Pack (2020), picks** | https://poly.pizza/bundle/Medieval-Village-Pack-NsHhjhlrfY · https://quaternius.com/packs/medievalvillage.html | CC0 1.0 | ~1.0 for the props / 5.0 incl. buildings | **Square, streets, countryside:** Well (1,870), Market Stand ×2 (1,132/1,548), Gazebo (698), Cart (2,659), Bonfire (602), Hay, Bags, Packages, Crate. Optionally Fantasy House ×3 (3,024–7,162), Inn (7,756), Stable (6,246) and Blacksmith (7,659) as **non-enterable back-row archetypes** | Props are cheap. The buildings are 3–8k tris: use them only where no door is needed, cull 12,000+, and decimate 30%. Skip Bell Tower (10,120) and Mill (7,018) | 4 |
| 11 | **Kenney Pirate Kit 2.1** | https://kenney.nl/assets/pirate-kit (mirror https://opengameart.org/content/pirate-kit) | CC0 | 3.2 | **Harbour and coast (alternative to #1):** 70 models: ships, rowboats, docks, cannons, chests, barrels, fortress walls (v2.1) | Low-poly colormap (*confirm*). Take it only if #1 lacks a piece (e.g. fortress wall for the cape battery) | 3 |
| 12 | **Low Poly Greek Fishing Boat** (Muyaya Concept, Sketchfab) | https://sketchfab.com/3d-models/315a83fe28014d2f890acbe16f04a26f | **CC-BY 4.0** (credit line required) | *measure* (Sketchfab login needed) | **Harbour hero fishing boat:** a traditional kaiki with wheelhouse, the "proper boat" moored at the quay for chores and fishing. Better silhouette than any CC0 option | 4,890 tris, 1 material, 1 palette texture → VC trivially. Not fetchable by `kg_fetch.py` (Sketchfab needs the user's login) | 4 |
| 13 | **RG Poly Medieval Props Small Pack** | https://rg-poly.itch.io/medieval-props-small-pack-low-poly | CC0 (states "no generative AI") | 21 (.7z) | **Harbour boatyard, laundry, square:** wooden boat frame on stocks, clothesline laundry, vise, broom, archery targets; 3 colour variants each | One atlas per asset (*measure tris*). The 21 MB is mostly multi-format duplicates | 3 |
| 14 | **Kenney Particle Pack** | https://kenney.nl/assets/particle-pack | CC0 | 15.0 | **Weather and VFX:** 80+ greyscale sprites (smoke, sparks, streaks, circles, dirt, flares) as rain streaks, splash rings, chimney smoke, fog wisps and storm spray for Niagara | Small textures. Pack 4–6 used sprites into one 512² flipbook atlas and never ship the full set. Translucent overdraw is the cost: cap the rain particle count and use GPU sprites | 4 |
| 15 | **Poly Haven sky HDRIs:** Approaching Storm, Kloofendal Overcast (Pure Sky), Qwantani Night (Pure Sky) | https://polyhaven.com/a/approaching_storm · https://polyhaven.com/a/kloofendal_overcast_puresky · https://polyhaven.com/a/qwantani_night_puresky | CC0 | 4.2 at 1k (1.6 + 1.2 + 1.4); 16.2 at 2k | **Weather states:** SkyLight/reflection cubemaps for storm, overcast and night (Storm Manor's storm, a Morrowmere rain phase) | One cubemap each, 1k is enough. **Style risk:** photographic, so use only as a SkyLight source (lighting colour), never as the visible sky. The toon sky stays ours | 2 |
| 16 | **Tiny Treats Baked Goods** (Isa Lousberg) | https://tinytreats.itch.io/baked-goods | CC0 | 3.9 | **Bakery stall and inn:** 24+ pies, cakes, bread loaves. Same studio family as KayKit (gradient atlas) | Atlas → VC; cull 5,000 | 4 |
| 17 | **Vertex color trees set** (vertexcat) | https://vertexcat.itch.io/vertex-color-trees-set | CC0 1.0 | 0.11 | **Wilds and forest far ring:** 6 trees for the distant ring behind the forest and on the cape | 116–597 tris, **already vertex coloured** (direct to `M_KG_PropVCLinear`, no bake) | 3 |
| 18 | **Kenney Graveyard Kit 5.0** | https://kenney.nl/assets/graveyard-kit (mirror https://opengameart.org/content/graveyard-kit) | CC0 | 3.6 | **Storm Manor grounds (supplement to #6):** fences, gates, crypts, lantern posts, trees (v5 "updated trees, colors") | Colormap (*confirm*). Only if #6 is not enough. Don't mix both fence styles on one wall | 3 |
| 19 | **Modular Wooden Docks** (loafbrr) | https://loafbrr.itch.io/wooden-docks | CC0 | 151 | **Harbour:** 176 grid pieces (planks, posts, ladders, fences, props) if the quay needs more modular pier than DressTerrace/pirate piers give | 15,856 tris for the **whole** set (cheap), but 15 materials on a trimsheet → bake to VC. A heavy download for light geometry | 3 |
| 20 | **Modular Village Pack** (Fertile Soil Productions) | https://fertile-soil-productions.itch.io/modular-village-pack | CC0 | 0.86 | **Facade blockout, back streets:** 155 modular pieces (walls, roofs, doors) for quick archetype blockouts | OBJ with solid-colour MTL materials → direct VC. Blocky; style fit only after bevel/palette pass | 3 |

**Checked and rejected:**

| Candidate | Reason |
|---|---|
| Japanese packs on itch: Sinto Shrine Essentials, 7 Japanese Traditional Props, Low Poly Japanese Street Pack, Mini Japan | Paid, and/or the AI-disclosure field says generative AI was used |
| RG Poly Asia / Pirate Island / Medieval Village megapacks | Paid ($25+) |
| loafbrr Medieval Fantasy Set / Bamboo Huts / Wood Cabin | Paid |
| Quaternius Ultimate Fantasy RTS | Diorama scale (Docks 7.4k, Port 6.7k tris) |
| Quaternius Pirate "Ship" | 20.6k tris |
| Quaternius Bestiary | QAL licence, not CC0 |
| SimplePolygon kits | Custom licence (no redistribution), outside the CC0 policy |
| Kenney Watercraft Kit | Modern boats |

## 2. Style-fit risks (per family)
- **Quaternius (#1, 2, 4, 7, 10):** same author as our kits. The older ones (2018–2020) are flatter and less bevelled. That
  is fine for props; for buildings, keep them in back rows.
- **KayKit / Tiny Treats (#3, 6, 9, 16):** the same gradient-atlas family, chunkier and rounder. Good for small goods and
  the graveyard. Risky for trees next to Quaternius trees (one hero style per category, AssetResearch §16.1).
- **Kenney (#5, 8, 11, 14, 18):** very clean, sharp and geometric. Use them for camp, stall, roof and awning pieces spread
  across a whole area, never interleaved with MegaKit walls on one facade. Run the palette snap so the colours converge.
- **Photo HDRIs (#15):** lighting only.

## 3. Performance cost and budgets (the game must not stutter; everyone should be able to play)
- **One material for everything new.** Every pack is baked to face-corner vertex colour and imported with
  `M_KG_PropVCLinear`. This adds **no texture memory** and no new shader permutations. Draw calls are 1 per HISM per mesh
  per zone when placed through `C.instanced()`.
- **Triangle budgets per piece:**

  | Kind | Budget |
  |---|---|
  | Clutter / goods | ≤ 800 |
  | Props | ≤ 2,000 |
  | Set pieces / boats | ≤ 6,000 |
  | Non-enterable buildings | ≤ 8,000 |

  - Over budget: decimate (collapse) in the pack step: Bucket of Fish 3,860, Flounder 1,876, Squid 1,296, the Village-Pack houses.
  - Excluded outright: Ship 20.6k, Bell Tower 10.1k, Sushi Truck 14.4k.
- **Merge a market stall's goods** (fish on ice, a crate of vegetables) into one mesh per variety in Blender, so one stall
  is 2–3 draws, not 30.
- **Culling, as in the dressing README:** small ≤ 1 m at 5,000–7,000 cm; 1–3 m at 9,000–12,000; buildings, boats and
  landmarks have none. The zone budgets stay (≤ 900 actors, ≤ 10 lights).
- **Mesh variety:** ≤ ~40 new unique meshes per zone, to cap the HISM count.
- **Other import settings:** Nanite off (the import script already does it). Add an auto LOD1 (50%) for meshes over
  2k tris (proposal: a LOD option in the import step).
- **Estimated memory added by the top 10, after the bake:** under 10 MB of mesh data in total, and no textures. The
  downloads are larger than what ships, because the source archives carry FBX/OBJ/glTF duplicates and atlases we discard.
- **Web port question (user):** UE5 has no official in-browser export. The realistic routes are streaming or a separate
  web client, and are a separate decision. This shortlist does not block any of them: no textures, low triangle counts, one
  material. The same content also serves low-end PCs through the Low scalability tier.

## 4. Make instead of download (user: "gerekirse sen de yap, üret")
No free CC0 pack covers these well, so extend our procedural Blender packs:
- **Japanese garden** (no CC0 pack found). Extend `kg_make_japan_props.py` with:
  - stone lantern on a plinth;
  - chozuya basin;
  - bamboo fence;
  - engawa veranda;
  - raked-gravel tiles;
  - small arched bridge sized to real water.

  #2 supplies the walls and doors.
- **Signage.** Add more hanging-sign glyphs to `KG_DressVillage`: net, key, boot, candle, clock, anchor. Add a painted
  harbour-master board and district way-finding posts.
- **House facades and roofs (SPRINT-022 archetypes).** Jettied upper storey, bay window, outside stair, porch, dormers
  and chimney variants as kit pieces in a new `KG_DressFacade` pack. Use #8 and #20 only as shape references or fillers.
- **Loot.** Bottles, map scrolls, a coin pouch, a lockbox, and pre-cut breakable crate/barrel pieces (AssetResearch §9:
  no free pre-fractured pack exists).
- **Weather meshes.** Rain-streak cards, puddle decals and wind-bent flag/banner variants on the Sway material.

## 5. Import plan (through our pipeline)
0. **Approval and registration.**
   - The user approves the list below: name, URL, licence and size are in §1.
   - Add each approved pack to `Tools/Assets/approved_sources.json` (folder, urls, license, recommended_import).
   - Add each credit line to `Docs/Credits.md` **at import time**. CC0: listed anyway. #12 CC-BY: in-game credit
     "Low poly Greek Fishing Boat by Muyaya Concept, CC BY 4.0".
1. **Fetch (headless, no windows):** `python Tools/Assets/kg_fetch.py <page_url> Art/Source/<Folder>`.
   - **Supported by `kg_fetch.py`:** itch (#3, 6, 7, 9, 13, 16, 17, 19, 20), OpenGameArt (the Kenney mirrors of #5, 8,
     11, 18, which carry the same zip names and versions as kenney.nl) and poly.pizza (#1, 2, 4, 10, one model page each).
   - **Not supported:**
     - #14: Kenney Particle Pack has no OGA mirror. Add a kenney.nl direct-zip case (proposal) or fetch it by hand.
     - #15: Poly Haven. Add an `api.polyhaven.com/files` case (proposal).
     - #12: Sketchfab needs the user's login. The user downloads it, or it goes through the Blender MCP Sketchfab downloader.
   - `kg_fetch` refuses executables and extracts with `tar.exe`. RG Poly's `.7z` extracts with bsdtar (*confirm*).
2. **Pack (headless Blender 5.2):** one glb per theme, not per source.
   - **Theme packs:**
     - `KG_DressBoats`: #1, #12.
     - `KG_DressMarket`: #2 fish, #3, #4, #16, #10 props.
     - `KG_DressJapanArch`: #2 walls.
     - `KG_DressFarm`: #7.
     - `KG_DressCamp`: #5.
     - `KG_DressGrave`: #6, #18.
     - `KG_DressForest`: #9, #17.
     - `KG_DressFacadeX`: #8, #20.
   - Folder-of-glTF sources start with `Tools/Blender/kg_pack_kit.py <gltf_dir> <out.glb> <prefix>`.
   - Then a new `Tools/Blender/kg_make_dress_extern.py` (proposal, pack whitelist in a JSON) does, per piece:
     1. **Whitelist:** drop modern and off-theme items and anything over budget that can't be decimated.
     2. **Rescale** each source **uniformly** to project units: 1 m = 100 uu, 300 cm storey, 210 cm door, and the MegaKit
        grid for Kenney/Fertile Soil. Set the pivot to bottom-centre (hinge edge for doors). Name it `SM_KG_<Name>`.
     3. **Colour → vertex colour** "Col" (FLOAT_COLOR, CORNER, RGB = linear albedo), per source type (AssetResearch §16.3):
        - Flat material colours (Ships, Food, Farm, Fertile Soil): the material base colour.
        - Gradient/colormap atlases (KayKit, Tiny Treats, Kenney): the texel at each face's UV centroid.
        - Textured (Pirate Kit, Sushi, loafbrr trimsheet): the average texel per face.
        - Already vertex-coloured (vertexcat): kept.
        - Optional nearest-swatch snap to `kg_common.PALETTE` in OKLab, plus the fake-AO darkening that
          `kg_make_dress_harbour.py` uses.
     4. **Alpha mask** for the material family. Sway for sails, flags, laundry, bunting and tree crowns. Glow for lantern
        glass. Fish for the fish (if they should wiggle on the stall, otherwise VC).
     5. **Decimate** to the §3 budgets. Merge stall-goods arrangements. Apply transforms.
   - Output: `Art/Packed/<Pack>.glb`.
3. **Sanitise:** `blender --background --factory-startup --python Tools/Blender/kg_sanitize_glb.py -- Art/Packed/<Pack>.glb Art/Packed/<Pack>_Clean.glb`.
   It triangulates, merges doubles, drops loose geometry and sets one plain material, so the colours live in COLOR_0.
4. **Manifest:** `Art/Packed/<Pack>_Clean.json`, `{"props": {"<Name>": {"material": "VC|Glow|Sway|Fish", "collision": "complex|box|none"}}}`.
   - `complex`: piers, docks, boats people stand on.
   - `box`: barns, crates, fences, gravestones, tents.
   - `none`: goods, small clutter.
5. **Import (headless, only when the editor is closed):**
   `UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_dress_pack.py <Pack>_Clean" -unattended -nosplash -nopause -nullrhi`
   - Meshes land in `/Game/KillGodot/Env/Dress/<Pack>_Clean/StaticMeshes/`, with `M_KG_PropVCLinear` / Glow / Sway / Fish,
     Nanite off and the chosen collision.
   - If an import fails half-way, bump the name (`_Clean2`) and never retry into the same folder.
6. **Catalogue:** run `Tools/Unreal/kg_asset_catalog.py` to refresh `Tools/Unreal/dressing/asset_catalog.md/.json`.
   The dressing modules may only use catalogue paths.
7. **Dress:**
   - Place the pieces in `Tools/Unreal/dressing/v2/dress_<zone>.py` / `kit.py`: repeated pieces via `C.instanced()` with
     `cull=`, boats via `C.mover()` (bob/sway), stall goods on the existing `Stall_*`.
   - Plan dry first: `python Tools/Unreal/kg_dress_v2.py --dry <zone>`, then `powershell -File Tools/Unreal/kg_dress_v2.ps1 -Zones "<zone>"`.
8. **Verify:**
   - `python Tools/Level/verify_v2_build.py` (clearances, and the SPRINT-022 placement validator).
   - The headless perf capture (≤ +0.5 ms at 1080p, the same bar as the water item).
   - `run_invariants.ps1`.

## 6. Approval list (what the user would say yes to)
Numbers match the table in §1.

| Pack | Size | Licence |
|---|---|---|
| 1. Quaternius boats + harbour: 11 models from poly.pizza | ~1.0 MB | CC0 |
| 2. Quaternius Sushi Kit picks: 21 models from poly.pizza | 0.7 MB | CC0 |
| 3. KayKit Resource Bits, free tier (itch) | 8.1 MB | CC0 |
| 4. Quaternius Ultimate Food picks: 18 models from poly.pizza | 0.5 MB | CC0 |
| 5. Kenney Survival Kit 2.0 | 1.9 MB | CC0 |
| 6. KayKit Halloween Bits, free tier | 5.3 MB | CC0 |
| 7. Quaternius Farm Buildings | 3.4 MB | CC0 |
| 8. Kenney Fantasy Town Kit 2.0 | 3.9 MB | CC0 |
| 9. KayKit Forest Nature Pack, free tier | 6.1 MB | CC0 |
| 10. Quaternius Medieval Village Pack (2020) picks: ~15 props + optional 6 buildings from poly.pizza | 1.0–5.0 MB | CC0 |

**Top-10 total:** about 32–36 MB to download, and well under 10 MB of mesh data in the game after the bake.

Optional extras:

| Pack | Size | Licence |
|---|---|---|
| 11. Kenney Pirate Kit 2.1 | 3.2 MB | CC0 |
| 12. Muyaya Greek Fishing Boat | size not listed; needs the user's Sketchfab login | CC-BY 4.0 |
| 13. RG Poly Small Props | 21 MB | CC0 |
| 14. Kenney Particle Pack | 15 MB | CC0 |
| 15. Poly Haven: 3 sky HDRIs at 1k | 4.2 MB | CC0 |
| 16. Tiny Treats Baked Goods | 3.9 MB | CC0 |
| 17. vertexcat trees | 0.1 MB | CC0 |
| 18. Kenney Graveyard Kit 5.0 | 3.6 MB | CC0 |
| 19. loafbrr Wooden Docks | 151 MB | CC0 |
| 20. Fertile Soil Modular Village | 0.9 MB | CC0 |

## 7. Exact picks for the poly.pizza entries (model IDs)
- **#1:**

  | Model | ID |
  |---|---|
  | Sail Boat | BgSZXwmm7k |
  | Boat | 5UEl54KsuC |
  | Lifeboat | Bkd4KKQA4O |
  | Sail Ship | cIzO4MBPqI |
  | Small Ship | W2vMzztgIi |
  | Dock | MBZUZAOWew |
  | Dock Broken | 1Ve8ZBLSB0 |
  | Anchor | ejWQny9Gcz |
  | Cannon | J15vlPVvKK |
  | Bucket of Fish | G61jVPof0G |
  | Barrel | Eko3cjAMW9 |

  Optional loot: Chest Closed (AngpV0HxD8), Coins (VaGFKE0n0F).
- **#2:**

  | Model | ID |
  |---|---|
  | Shoji Wall | YQu7UD8YIS |
  | Shoji Interior | JHWjGn5rtL |
  | Japanese Door | t4otyljz8K |
  | Red Wood Wall | fCt0206mzB |
  | Torii Gate | 7SyXZ62xR5 |
  | Table | HkPCEdQ5d5 |
  | Bench | nARUaxtRHA |
  | Stool | ngWUQ7Dt9u |
  | Dead Tuna | Rf86QRhYYc |
  | Dead Flounder | DvXwMLGL8e |
  | Dead Mackerel | SRIt9gKM5j |
  | Dead Salmon | EDOyeLovqG |
  | Dead Fish | 7r0bphYNPY |
  | Dead Eel | ncfRdJZ0xT |
  | Dead Octopus | Z6O59Ps7CW |
  | Squid | gReSPnGdkr |
  | Wooden Board | CfQ9CnzQiy |
  | Bowl | cXIpPoXb1U |
  | Plate | JyOqlmg0O8 |
  | Pot Filled | kXCojdgsiN |
  | Large Pot Filled | xL4YlaHjS2 |

  The Arch piece is in the bundle but its model ID was not captured; add it at approval if wanted.
- **#4:**

  | Model | ID |
  |---|---|
  | Bread | 1gzhqdQ4vu |
  | Bread Slice | OkkMwZSFTb |
  | Croissant | xOy52a3rOF |
  | Apple Green | 3VXWnjDOEw |
  | Tomato | ByggGxctjw |
  | Eggplant | QV0aZsjWPc |
  | Pepper Green | fh3RxFIH3j |
  | Carrot | l4YmYv8hFK |
  | Turnip | taMEmsQCye |
  | Lettuce | MEmUwHUHNR |
  | Broccoli | 6exp0qAEVd |
  | Pumpkin | bvLvqnU1jX |
  | Banana | ruOFtE0B6Z |
  | Egg | ngjyRi84lk |
  | Steak | abCzZiCgB0 |
  | Chicken Leg | Ed5NPaeT2N |
  | Cooking Pot | jAUb3FoCN7 |
  | Cooking Pot | lMEdEOMg9L |

- **#10:**

  | Model | ID |
  |---|---|
  | Well | QlqncKYxXb |
  | Market Stand | hts7l0NZxW |
  | Market Stand | DGIM5HGISb |
  | Gazebo | xYZB1TmGMv |
  | Cart | l7bDe7ak6j |
  | Bonfire | Azj9hJwwwG |
  | Hay | Yu8TOERkpw |
  | Bag | VRfAODZ0Xk |
  | Bags | gzvyAQ797z |
  | Bag Open | rJuZexcuhU |
  | Package | kYvD6QCQRd |
  | Package | mWkgWyrCfM |
  | Crate | 3OEFd1AWfa |
  | Bench | 7uSlZo3n9Y |
  | Bench | jLxjFxFRpw |

  Optional buildings:

  | Model | ID |
  |---|---|
  | Fantasy House | he3p42mUTH |
  | Fantasy House | BH2XHWUNmF |
  | Fantasy House | dcPho4SUA3 |
  | Fantasy Inn | x3ZcGn3jr4 |
  | Fantasy Stable | qhNQSOGGbi |
  | Fantasy Blacksmith | bV52eTG1Aj |

- **#7 (poly.pizza route):** all 10 models of `Farm-Buildings-Bundle-ppbnhEfNEt`, or the itch zip instead.
