# KILL GODOT — Asset Research (3D, animation, first-person, UI, audio, VFX)

- **Written:** 2026-09-24
- **For:** Morrowmere (coastal colonial fishing village). UE 5.8, Blender 5.2, PC first, mobile later.
- **Related docs:** `06_Art_Direction.md` (palette, toon look, budgets), `04_Map_Village.md` (map, houses, underground), `05_Tech_Architecture.md` (voice → jaw).

**How this was checked**

- **Pages loaded:** every row comes from a page that was actually opened: itch.io, quaternius.com, poly.pizza, the Sketchfab v3 API, OpenGameArt, Fab listing data (read in a browser), syntystore.com, Google Fonts `METADATA.pb` files and publishers' licence pages.
- **Nothing was downloaded.**
- **Partial checks:** rows marked *(partial)* or *(unverified)* could not be fully confirmed.
- **Search limits:** the web-search quota ran out partway through. The last checks were direct page loads, so a missing item does not prove it doesn't exist.
- **itch.io listings:** itch browse pages only show about 36 results per tag combination, so many tag combinations were used.
- **Prices:** as seen on 2026-09-24. They change often, especially Fab's "limited-time free" items.

**Scope rules (from the lead)**

- **Budget:** free assets only. Paid packs appear only in §14 as optional upgrades.
- **Kenney:** no Kenney 3D kits, because the style was rejected. Kenney is listed only for UI and SFX.
- **Look:** vivid, saturated colours. Speed matters: poly counts, number of materials and texture approach are noted everywhere.

**Target specs (from 06_Art_Direction.md)**

- **Colour:** one 256×256 palette atlas (8×8 swatches, each a vertical light-to-dark gradient) and one master material (`M_KG_Palette`).
- **Shading:** toon ramp. Inverted-hull outlines on characters and held items. Sobel post-process outline on PC.
- **Triangle budgets (LOD0):**
  - Small prop: 50–800
  - House shell: 3–6k
  - All furniture in one house: 15–25k
  - Tree: 0.8–2.5k
  - Character: 6–10k
  - First-person arms: 4–6k
  - First-person weapon: 3–8k, with its own 512–1024 px texture
- **Mobile scene limits:** ≤ 400k triangles on screen and ≤ 350 draw calls.
- **Scale:** 1 uu = 1 cm. Door 100×210 cm, floor-to-floor 300 cm, stair riser 18 cm.

**Legend**

- **License**
  - **CC0:** public domain, no credit needed.
  - **CC-BY:** credit required (§17).
  - **Custom:** the author's own terms, summarised in the row.
  - **Fab Std (P0/Pro0):** Fab Standard License, free on both the Personal and Professional tiers. It allows commercial games but not redistribution of the raw asset.
  - **Fab Std (Personal free):** only the Personal tier is free. The Professional tier is paid, and you need it once you pass the Personal tier's revenue limit (reportedly US$100k/yr; confirm on Fab).
  - **NOT STATED:** the page gives no licence. Do not ship until the author confirms in writing.
- **Mobile**
  - **A:** palette or vertex colour, 1–2 materials, low triangle count. Drop-in.
  - **B:** fine after merging materials, downscaling textures or decimating.
  - **C:** heavy (PBR 2–4K textures, dozens of materials, Nanite/Virtual Texture, or >50k triangles). Reference only, or needs heavy rework.
- **Fit:** 1 (poor) to 5 (ideal) for this game's look and needs.

---

## 0. Key findings

1. **Best style anchor: the Quaternius "MegaKit" family (2024–26).**
   - Packs: Medieval Village MegaKit, Fantasy Props MegaKit, Stylized Nature MegaKit, Universal Base Characters, Modular Character Outfits – Fantasy, and Universal Animation Library 1 and 2.
   - All CC0, one author, one art direction, one shared humanoid rig.
   - The free "Standard" tier of each kit holds about 60–70% of the models.
2. **No free kit gives a complete two-floor furnished house.** The best free route:
   - **Shells:** MegaKit walls, floors, stairs and roofs. The walls have interior and exterior faces.
   - **Furniture:** single-palette CC0 furniture from JayBee Little Creations and Azrael68.
   - **Clutter:** KayKit "Bits", Fantasy Props MegaKit, and Daniel Mistage's Armory and Deep Sea Hunter's Station.
   - The single-palette packs suit the planned house editor well.
3. **Jaw and mouth: none of the main free character packs documents a jaw bone or mouth shape keys** (Quaternius, KayKit, the Fab free humanoids). Options:
   - (a) The puppet direction in `06_Art_Direction.md` (rigid parts and a hinged jaw). This makes the jaw trivial, and `05_Tech_Architecture.md` already defines the `JawOpen` driver.
   - (b) Add a `jaw` bone to Quaternius Universal Base Characters.
   - (c) **Synty Sidekick FREE Starter** (Fab, free). It has ARKit facial blendshapes including jaw open and mouth shapes.
   - (d) **styloo "The Company"** (CC0). It has a Rigify facial rig with a mouth, but the characters are fantasy types at 8–10k triangles.
4. **First-person viewmodel**
   - **Arms:** Drillimpact *PSX First Person Arms* (CC0) ships 18 first-person animations, including knife draw, idle and two hits.
   - **Guns:** the best low-poly guns are TastyTony's Sketchfab catalogue (CC-BY, flat colours). It has a Desert Eagle, a Pfeifer Zeliska "hand cannon" and a Simeon North 1826 flintlock. Add Milaein's blunderbuss.
   - **Joke melee:** almost every silly melee weapon exists as CC0 on poly.pizza (umbrella, frying pan, rolling pin, baguette, swordfish, candlestick).
   - **Mobile catch:** UE5's **First Person Rendering does not work on the mobile renderer**, so mobile needs a fallback.
5. **Coast**
   - **Harbour and coastline:** SimplePolygon docks, cliffs, basalt and pines (palette-based; credit required).
   - **Boats:** CC-BY boats from Sketchfab, including Razer820's 1600s–1700s sloop and Muyaya Concept's rowboat and fishing boat.
   - **Fishing clutter:** Daniel Mistage's Deep Sea Hunter's Station.
   - **Lighthouse:** no free lighthouse can be entered, so build the interior yourself.
6. **Underground:** loafbrr's *Mines & Cave Modular Set* (CC0) plus zisongbr's *LowPoly Pirate Props* (CC0: 4 cave types, piers, contraband) cover smuggler tunnels and sea caves. Sea caves are still the thinnest category.
7. **Licence traps found**
   - Quaternius' newest pack (Bestiary, Aug 2026) uses the new **QAL**, not CC0.
   - Several itch packs state **no licence**.
   - SimplePolygon's **Sketchfab** copies are CC-BY-**ND**, so use the itch copies.
   - Kynnelo cave tunnels are **BY-SA**. DARK_PALETTE Halloween is **BY-ND**.
   - **BBC SFX and Tabletop Audio are non-commercial.** FreePD is offline.
   - Several items are **AI-generated** (Polyy.AI, "Knife FPS Animations", SimpliGen, FreeStylized skies), which triggers Steam's AI disclosure.
8. **Fonts:** nearly every gothic or colonial display font lacks **Cyrillic**. Use a UE Composite Font with a Cyrillic fallback (§11).

---

## 1. Starter stack for the vertical slice

| # | Pack(s) | Role | License | Free tier gives |
|---|---|---|---|---|
| 1 | **Quaternius Medieval Village MegaKit** | House shells, town buildings, interior walls, stairs | CC0 | 170 of 304 models (Pro $9.99 = all) |
| 2 | **Quaternius Fantasy Props MegaKit** | Props, market stalls, chests, breakables, tools, food, books | CC0 | 94 of 211 |
| 3 | **Quaternius Stylized Nature MegaKit** | Trees, rocks, grass, flowers | CC0 | 81 of 116 |
| 4 | **JayBee Little Creations** House + Kitchen + Bedroom, plus **Azrael68** beds, furniture, bookshelves, doors | Furnished interiors; house-editor catalogue | CC0 | Everything in the free tiers |
| 5 | **Daniel Mistage** STYLIZED Fantasy Armory + Deep Sea Hunter's Station | Blacksmith, harbour and fishing clutter, captain's cabin | Fab Std (P0/Pro0); itch copy is custom | Full packs |
| 6 | **SimplePolygon** Wooden Docks, Modular Cliffs 2, Basalt Rocks, Pine Tree Pack | Harbour, coast, sea stacks, New England pines | Custom (free, credit required, no redistribution) | Full packs |
| 7 | **Boats and fish:** Razer820 Low Poly Sloop, Muyaya Concept rowboat and Greek fishing boat, Quaternius Ships Pack and Animated Cute Fish | Harbour life, fishing | CC-BY 4.0 / CC0 | Full |
| 8 | **Quaternius Universal Base Characters + Modular Outfits Fantasy + Universal Animation Library 1 and 2**, plus KayKit Character Animations | Humanoid base or fallback, cosmetics reference, all third-person animation | CC0 | UBC: 2 bodies and 5 hairstyles free. Universal Animation Library: 45 + 42 animations free. KayKit: all 161 animations free |
| 9 | **loafbrr** Mines & Cave Set + Mine Object Collection, **zisongbr** LowPoly Pirate Props + Cave Entrance, **KayKit Dungeon Pack** | Smuggler tunnels, mine, sea caves, crypt, cellars | CC0 | Full (KayKit: 200 of 275) |
| 10 | **loafbrr** Halloween Props + **KayKit** Halloween Bits + Quaternius trapdoor, cobweb and cauldron (poly.pizza) | Spooky, graveyard, ghost props | CC0 | Full / 60+ |
| 11 | **Drillimpact** PSX First Person Arms + **TastyTony** guns + **Milaein** blunderbuss + poly.pizza CC0 joke melee | First-person viewmodel | CC0 / CC-BY | Full |
| 12 | **tharlevfx** Water Materials + **Good SKY** + **Vefects** Stylized Fire | Sea, sky, fire | CC BY 4.0 / Fab Std | Full |
| 13 | **Kenney** UI Pack Adventure, RPG Expansion, Fantasy UI Borders and audio packs; **game-icons.net**; Google Fonts; **Sonniss** GDC bundles; **RandomMind** CC0 music; **Kevin MacLeod** | UI and audio | CC0 / CC BY 3.0 / OFL / Sonniss licence / CC BY 4.0 | Full |

**Why these fit together:**

- **One family for the big pieces.** Rows 1–3 and 8 come from the same Quaternius art direction: chunky, bevelled, simple shapes, shared texture sets, one humanoid rig. Buildings, props, trees and people will match in shape language before any rework.
- **Shared colour through the palette.** Rows 4, 5, 6, 7, 9 and 10 are all palette-, gradient-atlas- or flat-colour-based. The `kg_process_asset.py` step (nearest palette colour → UV collapse onto `T_KG_Palette`) gives them all the same colours. After that, the only differences left are silhouette and bevel, and those are close enough in these packs.
- **First-person items are the exception.** Row 11 items sit close to the camera and get their own 512–1024 px texture, as the art direction allows.
- **What to avoid.** Hand-painted packs (bitgem, pixelzedge, cotman_sam), PSX-textured packs (Goblinatron, chilly_durango) and PBR packs (SOI, StylArts, Poly Haven). Use them only as reference, or repaint or bake them to the palette.

---

## 2. Town buildings and modular house shells

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Medieval Village MegaKit | Quaternius | https://quaternius.itch.io/medieval-village-megakit · https://quaternius.com/packs/medievalvillagemegakit.html | CC0 | 304 models (Jan 2025). Walls snap to a grid and "include both an exterior and an interior at the same time". Also floors, stairs, modular roofs, doors, windows, vines, furniture and props. **Tiers:** Standard free (170), Pro $9.99+ (all), Source $14.99+ (.blend, UE/Unity/Godot projects, wear-colour shaders, collisions). Textured. FBX/OBJ/glTF. Triangle counts, balconies and 2nd-floor support **not stated** | B | 5 | Measure grid vs 300 cm floor / 210 cm door, then scale the **whole kit uniformly**. Remap textures to the palette (average colour per face → swatch). Test stacking two floors with a stair opening. Split doors and shutters for gameplay |
| Stylised Low Poly Modular Medieval House Pack | JayBee Little Creations | https://jaybeelittlecreations.itch.io/modular-medieval-town-house | CC0 | Modular walls, roofs etc. Free zip 149 kB; supporter zip $3.99 (511 kB). **One material for all meshes**; small palette texture that you recolour by editing its columns. Marked "in development". Stairs and upper-floor pieces not listed | A | 4 | Point the UVs at `T_KG_Palette`. Check the grid against the MegaKit |
| Medieval Village Pack (2020) | Quaternius | https://quaternius.com/packs/medievalvillage.html | CC0 | 44 buildings and props, textured. FBX/OBJ/Blend | A/B | 3 | Older style; use for background houses |
| LowPoly Farm Buildings | Quaternius | https://quaternius.itch.io/lowpoly-farm-buildings | CC0 | 13 farm buildings (barn, silo…), flat colours | A | 3 | Farm and meadow area. Assign palette |
| Modular Village Pack | Fertile Soil Productions | https://fertile-soil-productions.itch.io/modular-village-pack | CC0 | 155 modular pieces, flat colours, OBJ/MTL | A | 4 | Good for blockout. Swap material colours for palette colours |
| Free Fantasy Medieval Houses | EmaceArt | https://emaceart.itch.io/free-fantasy-medieval-houses-and-props-pack | Custom: free commercial use, no credit, no redistribution | 165+ houses, inns and props. Props ~70 polys, heaviest building ~35k. Diffuse + normal maps, 3 LODs. **No interiors** (those are in the paid Slavica pack) | B | 3 | Decimate big buildings, drop normal maps, bake to palette |
| Roadside Tales Fences & Walls | EmaceArt | https://emaceart.itch.io/roadside-tales-fences-and-wall-bundle-pack-1-3-lods | Custom (same as above) | 127 meshes / 128 prefabs. 16–5,400 tris (median ~300). 2 diffuse materials, recolourable from one palette. LOD0–3 | A | 4 | Garden fences and walls for the 20 houses |
| Village Asset Pack (itch) / Village Asset Pack Houses FREE + Village Props FREE (Fab) | styloo | https://styloo.itch.io/vil · https://www.fab.com/listings/a25410d5-1ef9-40dc-95e9-bdfc1234018a · https://www.fab.com/listings/54739a75-1212-4e4e-a806-810fad5b00af | itch: CC0 · Fab: Fab Std (P0/Pro0) | 7 houses, cart, fences, streetlight, trees. Fab version: houses 33 meshes / 6.9k tris / 7 materials / 1K textures; props 18 meshes / 5.3k tris / 14 materials. GLB only | A | 3 | Convert GLB → FBX in Blender. Merge materials |
| Hypno's Low-Poly Fantasy Village | atblack | https://atblack.itch.io/hypnos-low-poly-fantasy-village-pack | Custom ("use it for whatever you want") | 24 models: 3 buildings, wall, gate, bridge, fences, **well**, wagon, lamp post, banner. Solid colours | A | 3 | Well and gate for the square |
| Jampot 01 Rustic / 02 Tavern | Jampot.gg | https://jampot-gg.itch.io/rustic-pack · https://jampot-gg.itch.io/tavern-pack | Rustic: **NOT STATED**. Tavern: custom (commercial OK, no resale) | 21 each: wall, corner, floor, doors, windows, fences, barrels, crates, bench. **Flat colour per material, no textures** | A | 3 | Merge materials. Confirm the Rustic licence |
| Architecture Pack 001 | CreativeTrio (poly.pizza) | https://poly.pizza/bundle/Architecture-Pack-001-ntWKh7113q | CC0 | 10: **church**, cottage, barn, cabin shed, bridge, castle | A | 4 | Church exterior base; build the interior |
| Carpenter Gothic Church | jrich01 (poly.pizza) | https://poly.pizza/m/Vq4Gik1t7c | CC-BY 3.0 | Wooden clapboard church with bell tower. The most "New England" building found | A | 4 | Exterior; add interior pews from furniture packs |
| Windmill | Pixel (poly.pizza) | https://poly.pizza/m/9BIMVqxyHV | CC-BY 3.0 | Single low-poly windmill | A | 3 | Separate the sails and put the pivot on the hub (wind subsystem). The map needs it enterable, so add an interior |
| Well | Jarlan Perez (poly.pizza) | https://poly.pizza/m/1lK9mD0zDZw | CC-BY 3.0 | Single well | A | 3 | — |
| Bell | Quaternius (poly.pizza) | https://poly.pizza/m/cILuaO115Y | CC0 | Single bell (for the bell tower) | A | 4 | Put the pivot at the top for swinging |
| FANTASTIC – Village Pack | Tidal Flask Studios (Fab) | https://www.fab.com/listings/52529a12-e88e-41a0-8834-b87306f20c24 | Fab Std, €0 on both tiers ("Permanent") | 400+ modular village assets: walls, props, food, stylised nature, **stylised water**. UE/Blender/FBX. Tagged Mobile. **No interiors** | A/B | 5 | Check it still shows €0 when you claim it. Also a good reference for water and foliage |
| Stylized Fantasy Provencal | StylArts / Leartes (Fab) | https://www.fab.com/listings/ced19ea1-31ed-437f-ae64-2b6b1561fede | Fab Std (P0/Pro0) | 131 meshes of colourful village exteriors, LODs, 92 materials, 109 textures (512–2K). UE 4.26–5.8. **Uses Virtual Texture** | C | 3 | PC only unless you remove the Virtual Texture use. Great colour reference |
| Lowpoly Handpainted Environment | Yevheniia Yaremko (Fab) | https://www.fab.com/listings/d7408876-2301-468a-a2e9-6eea38ddc10d | Fab Std (P0/Pro0) | 84 meshes (buildings, ruins, vegetation), <1,500 tris each. 92 materials, 202 textures (1–2K), summer and winter variants. UE 4.18–5.6, listed for iOS/Android | B | 4 | Merge materials, bake the hand-painted look to the palette, migrate to 5.8 |
| POLY – Medieval Camp | Animpic Studio (Fab) | https://www.fab.com/listings/436d467a-6955-4aac-be0d-a05c99966ea2 | Fab Std | 74 models: 4 houses, 15 weapons, 2 skinned vehicles, 40 props. UE 4.24–5.7 | A | 4 | Carts and tents for the market |
| Low Poly Market Pack | atomdev (Fab) | https://www.fab.com/listings/db13cfd8-6e4f-4ebb-8b16-4680b5a72c24 | CC-BY | Medieval market, **1 material with a colour atlas**, snow/wind shader. UE 4.26–5.4 | A | 4 | Market stalls for the bazaar |
| Fantasy FREE – Low Poly | ithappy (Fab) | https://www.fab.com/listings/e5d17709-8ebe-44af-946f-5991117095bc | Fab Std (P0/Pro0) | 26 models: houses, hut, cart, barrel, sack, crates, tree, spruce. FBX/GLB/Blend/UE | A | 3 | — |
| Stylized Medieval Environment Lite | Forge Pulse (Fab) | https://www.fab.com/listings/8fd69079-bfb5-4605-bdda-f71e5e2532bf | Fab Std | Cottage, 2 trees, fence, crate, barrel, rock | B | 3 | — |
| KayKit Medieval Hexagon Pack | Kay Lousberg | https://kaylousberg.itch.io/kaykit-medieval-hexagon | CC0 (Extra $9.99 adds 150+) | 200+ free: blacksmith, lumbermill, church, tavern, market, windmill, watermill, mine, well, houses, ships. **Gradient atlas** (1024 → 128). Diorama scale, **no interiors** | A | 3 | Background or distant hills only. Good colour reference |
| Medieval Props Minipack 3 – Building / 6 – Watch Tower | Typetree Studio / dyohandri (Sketchfab) | https://sketchfab.com/3d-models/c5e6fe842e02480a81b621fb3159262d · https://sketchfab.com/3d-models/86021aa8df064f82a69bbc3b231df52e | CC-BY | Minipack 3: walls, windows, door, floor, base, roof. 15,778 faces, **1 material, 1 texture** | A | 3 | Part of a 7-pack series with one style (see §3) |
| Modular Medieval Tavern v1.0 | Danilo Waselciac (Fab) | https://www.fab.com/listings/1640160b-d406-4de4-ab79-e6d953c7a11e | Fab Std (Personal free; Pro €17.56) | 10 modular pieces + props. 22,688 tris, 10 materials, 21 PBR 2K textures | B | 3 | Tavern shell reference; bake to palette |
| Frontier fortifications | XyloScenics (Sketchfab) | https://sketchfab.com/models/18de4e133f8242ee84a33f86fad74bb3 | CC-BY 4.0 | Wooden fort from the pike-and-musket era. 26.7k tris, PBR | B/C | 3 | Palisade and cannon-battery reference |
| 1500+ lowpoly textured medieval props | PixelLated | https://pixellated.itch.io/1500-lowpoly-textured-models | Custom, informal ("can't sell it… free for creating games") | 1,500+ models on one atlas in 7 variants; houses with open interiors. RAR + Unity files | A? | 3 | Worth a look; ask for a clearer licence |
| Medieval Inspired Modular Low Poly Prop Pack | TweakedStudios | https://tweakedstudios.itch.io/medieval-inspired-modular-low-poly-prop-pack | **NOT STATED** (pay what you want) | 2,784 modular pieces: 9 wall sets × 139, 4 roof types, windows, doors, railings, beams, floors, **stairs that can form spirals**. 6–1,000 verts per piece. Flat single colours or 32 px textures, 512² | A | 4 (if licensed) | Very relevant (spiral stairs for the lighthouse), but ask for a licence first |
| LOKIT Low Poly Medieval Village | Standout 7 | https://standout7.itch.io/lokit-village | **NOT STATED** | 70+ FBX free (walls, roofs, doors, beams, props); paid tiers 200+. Whole kit uses 4 textures | A | 3 | Confirm the licence |
| Medieval Village POLY Pack Demo | Nicrom | https://nicrom.itch.io/medieval-village-poly-pack-demo | **NOT STATED** | 110 models; 2+2 house presets **with interiors** you can toggle. Unity package + FBX | B | 2 | Reference for interior layouts |
| 3D Retro Medieval/Fantasy Building Kit | chilly_durango | https://chilly-durango.itch.io/medieval-building-parts | CC0 | 14 building parts (incl. **stairs, fireplace**), 15 furniture, 9 decorations. PSX-style 256² photo textures, .blend | B | 2 | Blockout only; swap textures for palette colours |
| Free Medieval Houses 3D Low Poly | Free Game Assets (CraftPix) | https://free-game-assets.itch.io/free-medieval-houses-3d-low-poly-models | Custom: free commercial, no credit | 20 FBX. Contents and triangle counts not stated | ? | 2 | — |
| Highlands – Fantasy Buildings | Visions Mind | https://visionsmind.itch.io/highlands-fantasy-pack-3d | CC0 | 7 buildings incl. a windmill; 4K PBR, 751 MB | C | 2 | Reference only |
| Stylized Medieval Village (Unity URP) | RG Poly | https://rg-poly.itch.io/stylized-medieval-village-unity-urp | CC0 | 210 models, avg ~478 polys, 30 materials, 20 textures at 2048². Unity package only | B | 2 | Needs Unity to export FBX |
| Medieval house pack / church / tavern | Daniel Andersson (OpenGameArt) | https://opengameart.org/content/medieval-house-pack · https://opengameart.org/content/medieval-church · https://opengameart.org/content/medieval-tavern | CC0 | Textured .blend files; church interior unfinished | C | 2 | — |

---

## 3. House interiors and furniture (player houses, tavern, shops)

Nothing free is a full two-floor furnished colonial house. Build it from shells (§2), furniture (below) and clutter (§9).

**Single-palette and vertex-colour furniture (best for the house editor)**

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Low Poly Medieval Kitchen Furniture | JayBee Little Creations | https://jaybeelittlecreations.itch.io/low-poly-medieval-kitchen-asset-pack | CC0 | 59 meshes (free 624 kB; supporter $3.99). **Same single-material palette** as the JayBee house kit | A | 5 | Remap palette. Kitchen for all 4 house types |
| Low Poly Medieval Bedroom Furniture | JayBee Little Creations | https://jaybeelittlecreations.itch.io/low-poly-medieval-bedroom-furniture-asset-pack | CC0 | 72 models (free 588 kB; supporter 3.5 MB). Single shared material. Item list not on page | A | 5 | Beds, wardrobes, chests for the upper floor |
| Medieval Inn & Peasant Beds | Azrael68 | https://azrael68.itch.io/medieval-inn-beds | CC0 | 7 beds from peasant to noble. **Vertex colours, no textures.** FBX 218 kB, metre scale, bottom-centre pivot | A | 4 | Remap vertex colours to palette |
| Medieval Fantasy Furniture | Azrael68 | https://azrael68.itch.io/medieval-fantasy-furniture | CC0 | Small table, dining table, chair, stool. Vertex colours | A | 4 | Physics furniture for barricades |
| Medieval Bookshelf & Library Pack | Azrael68 | https://azrael68.itch.io/medieval-bookshelf-library-pack-low-poly | CC0 | 10 bookshelves (3–6 shelves), simple wood/stone palette, FBX 213 kB, bottom-centre pivots | A | 4 | Make one into a **secret bookcase door**: split it, put the pivot on the hinge edge |
| Medieval Dungeon Door Pack | Azrael68 | https://azrael68.itch.io/freepaid-medieval-door-pack-low-poly | CC0 | 10 procedural doors, "Low Poly (Mobile friendly)". Pivots set for rotation | A | 4 | Front, back and cellar doors; lay one flat for a trapdoor |
| Medieval Window Pack | Azrael68 | https://azrael68.itch.io/medieval-window-pack-procedural-low-poly | **NOT STATED** | Windows (count not stated), FBX 92 kB | A | 3 | Confirm licence. Breakable windows (the map needs them) |
| Free Low Poly Furniture Pack 240+ | Se1dev | https://se1dev.itch.io/free-low-poly-furniture-pack-240-models | Custom: personal and commercial OK, credit optional, no resale as a pack | 240 models, **~350 tris each (~83.5k total)**. **One 256×256 palette atlas, one material** for the whole pack. Mixes modern items (office, appliances) with traditional ones (trestle tables, bookcases) | A | 3 | Drop the modern items; remap palette |
| Free Low-Poly Furniture (22 props) | Quin.GS | https://quin-gs.itch.io/furniture-lowpoly-cc0 | CC0 | Beds, cabinets, tables, chairs, stools, **carpets**, clock, flowers. Double bed 838 tris, others <500. Simple per-colour materials | A | 3 | Merge materials into palette |
| Medieval Furniture | AnyRPG (OpenGameArt) | https://opengameart.org/content/medieval-furniture-0 | CC0 | Tables, chairs, chests, bed. **1,946 tris total**. .blend | A | 3 | — |
| Low Poly House Interior | sjolle (OpenGameArt) | https://opengameart.org/content/low-poly-house-interior | CC0 | ~50 furniture models on a **single texture**. Style (modern or rustic) not confirmed | A | 3 | Check the style before using |
| Isometric – Interiors | PolyArt3D (Fab) | https://www.fab.com/listings/e2696b23-31be-4c5d-8792-d08b2915b5cd | Fab Std (P0/Pro0), in Epic's Permanent free collection | 238 cartoon interior pieces: **modular walls, floors, furniture**. 10–1,340 tris, 9 materials, **one 1024 atlas**. UE 4.22–5.8. "Runs smoothly on mobile" | A | 4 | Restyle or remove the modern props |
| Interiors FREE – Cozy Cartoon Pack | Mnostva Art (Fab) | https://www.fab.com/listings/ef194997-eb8c-4fc0-9d11-dad28d00eaa2 | Fab Std (P0/Pro0) | 40 props, 98–4,593 tris, **1 material, 1 palette texture** (2048 → 128). UE 4.26–5.7. Some items are modern | A | 3 | Bed, table, chairs, jars and teapot are usable |
| Cute Furniture FREE | ithappy (Fab) | https://www.fab.com/listings/3eca0616-aa73-4348-9af5-cb4c173f237e | Fab Std (P0/Pro0) | 66 assets, 70k tris total, 2–3 materials, one 1024 palette. **Doors and cabinets open.** UE 4.20–5.8 | A | 3 | Modern-cute style; reskin through the palette |

**Tavern and pub**

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Low Poly Tavern Interior | i.we.d (Sketchfab) | https://sketchfab.com/3d-models/5a71f5639df14b7bb6a67cab93d22079 | CC-BY 4.0 | 40 props with default and **shabby** variants. 46–898 tris each, 13.3k faces total. **2 materials, one 512² texture** | A | 4 | Shabby variants can be the "damaged" state |
| Low_Poly Medieval Tavern Interior | BattleRoach | https://battleroach.itch.io/low-poly-m | CC0 | 8: barrel, barrel stand, table, bench, shelf, stool, chair, bar. Texture approach not stated | B | 3 | — |
| Nordic Tavern Assets | T Allen Studios (OpenGameArt) | https://opengameart.org/content/nordic-tavern-assets | CC-BY 4.0 | 14: table, bench, beam, fire pit, counter, shelving, barrels, bread, pot, cup, pitcher. **One texture PNG**, FBX | A | 3 | — |
| Tavern + Inn Interiors — Free Sampler | UpDraft Art (Fab) | https://www.fab.com/listings/076d5f8b-3fa8-4dfd-8760-57ea32eab023 | Fab Std (P0/Pro0) | 4 props (trestle table, stool, bottle, mug), 4.7k tris, 1 material | A | 3 | Tiny sampler |
| Tavern Furniture Assets | Bumroker (Sketchfab) | https://sketchfab.com/3d-models/604783ded8d04490ad52174c1b87df34 | CC-BY | Tables, planks, **stairs**, floors, chimney, bar, doors, carpets. 6.9k faces but **31 materials / 24 textures** | C → B | 2 | Must bake to one atlas |
| Stylized Free Pub Mini Pack | Hatty | https://hattylaird.itch.io/stylized-free-pub-pack | Custom: commercial OK, no resale | 27 items: stools, chairs, tables, tankards, barrels, plates. Hand-painted, .blend only | B | 2 | Style mismatch |
| Medieval Interior Asset Pack | SOI | https://soi.itch.io/medieval-interior-asset-pack | Custom: "any purpose incl. commercial" | 38 tavern props, PBR, 307 MB | C | 2 | Reference |
| Piano | jeremy (poly.pizza) | https://poly.pizza/m/7U-93vxPOER | CC-BY 3.0 | Low-poly piano (old Google Poly-era model). Triangle count not shown | A | 3 | For the tavern piano puzzle; recolour |

**Cosy and PBR interiors (bake to palette, or PC only)**

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Medieval Room Kit | 1Luka2 (Fab) | https://www.fab.com/listings/52610433-0210-40e5-884b-abc12cf8901e | CC BY 4.0 | 48 hand-painted **modular walls, floors, furniture, decor**. ~100–1k polys each, 3 materials, 17 textures at 4K PBR. UE 5.0–5.8 + FBX. Published Aug 2026 | B (downscale 4K → 1K) | 4 | Bake to palette |
| Stylized Library (48 cozy props) | Daria Borovleva / "drelanns" | https://www.fab.com/listings/6150cf8e-8105-4da0-9209-3edbc2afb673 · https://drelanns.itch.io/stylized-library | Fab: Fab Std (Personal free, Pro €8.77) · itch: **NOT STATED** | 48 props (bookcases, **fireplace**, armchair, books, scrolls; also some modern items like a radio and camera). 119–4,827 tris (~51.5k total). PBR atlases 512–2K; ships a UE 5.7 version | B/C | 3 | Remove the modern items. Hill-house library |
| Stylized House Interior | StylArts / Leartes (Fab) | https://www.fab.com/listings/ab92e5d3-6db6-4cf3-bff5-c2c98ae8db5b | Fab Std (P0/Pro0) | 206 meshes, 120 materials/instances, 235 textures up to 4K, LODs. UE 4.25–5.8 | C | 3 | PC reference; kitchen and dining clutter |
| Medieval Village Props – Furniture, Containers & Clutter | DuplexG (Fab) | https://www.fab.com/listings/b8fc5a33-8dce-48d1-8175-1015aa4c9436 | Fab Std (Personal free, Pro €9.65) | 70+ meshes, 36–3,000 verts, auto LODs, **70+ materials**, 210+ textures at 2K. UE 5.3–5.7 | C | 3 | Bake into an atlas |
| Stylized Low-Poly Interior – Wooden Pack | Ayuo Dev | https://ayuo-dev.itch.io/stylized-low-poly-furniture-wooden-pack | **NOT STATED** | 57: chairs, 2 beds, bookshelf, nightstands, 3 tables, cupboard, desk, **wardrobe**, 3 floor tiles. Several textures per model, 63 MB | B | 3 | Confirm licence; bake to atlas |
| Fantasy Interior Props Free | CaptainCatSparrow | https://captaincatsparrow.itch.io/fantasy-interior-props-free | Custom: commercial OK, no redistribution | 18 kitchen and dining props (candle, pot, pan, mortar, jug, plates). 92–1,496 tris (~11.5k total). One 2048×4096 PBR atlas | B | 3 | Downscale to 1K, drop metallic |
| Low Poly Medieval House Interior | niko-3d-models | https://niko-3d-models.itch.io/3d-assets-bundle-medieval-house-interior | **NOT STATED** | Chest that opens, barrels, tables, chairs, cutlery, bottles, plates, **candles, picture frames**, food. No walls. .blend + FBX 693 kB | B | 3 | Confirm licence |
| Cozy tavern set (fireplace, shelf, rug, candle, painting, barrel, bottles, tavern stairs…) | Nick Slough (poly.pizza) | https://poly.pizza/m/uVogFlDES1 (fireplace) · https://poly.pizza/m/tUKc5vLVnj (tavern floor) | CC-BY 3.0 | 46 models on his profile, made in Google Blocks, flat colours | A | 3 | Merge vertex colours; needs credit |

**Kitchen, bakery and shop clutter** (KayKit and Tiny Treats: husband-and-wife studio, same 1024² gradient atlas, which can drop to 128²)

| Name | Author / Source | URL | License | Contents | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| KayKit Restaurant Bits | Kay Lousberg | https://kaylousberg.itch.io/restaurant-bits | CC0 | 140+ free (kitchenware; food raw, cooked and chopped); Extra +75 | A | 3 | Remove modern items |
| Tiny Treats Baked Goods | Isa Lousberg | https://tinytreats.itch.io/baked-goods | CC0 | 24+ pies, cakes, bread | A | 4 | Bakery ("Dumpling") |
| Tiny Treats Bakery Interior | Isa Lousberg | https://tinytreats.itch.io/bakery-interior | CC0 | 58+ counters, ingredients, floors, walls, doors. Includes a coffee machine and cash register | A | 3 | Remove modern items |
| Tiny Treats House Plants | Isa Lousberg | https://tinytreats.itch.io/house-plants | CC0 | 100+ (5 plants × 4 pots) | A | 3 | Windowsills |
| Tiny Treats Homely House | Isa Lousberg | https://tinytreats.itch.io/homely-house | CC0 | 16+ models (one house, fences, trees). Exterior only | A | 2 | Too modern |
| Medieval Props Minipack 1 / 2 / 5 / 7 | Typetree / dyohandri (Sketchfab) | https://sketchfab.com/3d-models/d75bcabb07784e0b97bbcaba53a60727 (1: containers) · https://sketchfab.com/3d-models/cf12041cfa614411b3546204ffc39333 (2: furniture) · https://sketchfab.com/3d-models/f5eab50ac40d404ab95f186cfc2a3707 (5: blacksmith) · https://sketchfab.com/3d-models/a8db571456f74d3885ccffe08b1ca421 (7: tools and food) | #1 Sketchfab Free Standard; #2, #5, #7 CC-BY | 100–200 tris per prop, **shared 512² atlas, 1 material**, 1 unit = 1 m | A | 4 | One consistent series; credit the author |
| STYLIZED Fantasy Armory | Daniel Mistage | https://www.fab.com/listings/aaaa7ffa-c4dc-434b-a6ac-3f0030a28905 · https://daniel-mistage.itch.io/stylized-fantasy-armory-low-poly-3d-art | Fab: Fab Std (P0/Pro0) · itch: custom (no redistribution, no NFTs) | 251 meshes: **anvils**, weapon racks, chests, barrels, furniture, cauldron, books, lamps, 14 building pieces. 60–20k verts (mostly 100–4.5k). 2 materials, **one 2K atlas**. UE 5.5/5.7 | A | 5 | Blacksmith ("Hodge") and house clutter |


---

## 4. Coast: harbour, boats, fishing gear, lighthouse

**Note on SimplePolygon:** some SimplePolygon models are also on Sketchfab, but those copies are **CC-BY-ND** (no edits allowed). Always download from their **itch** page instead: its licence is free for commercial use and allows edits, but requires credit and forbids redistribution.

### 4.1 Harbour, docks, fish market

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Wooden Docks – Lowpoly Pack | SimplePolygon | https://simplepolygon.itch.io/wooden-docks | Custom: free personal and commercial use, **credit required**, no redistribution or resale | Modular docks with and without nails. About 8.5k tris for the whole set (measured on the Sketchfab copy). `LowpolyDocks.fbx` + palette PNG | A | 5 | Recolour through the palette. Build 3 piers and the mail pier |
| Modular Wooden Docks | loafbrr (itch / OpenGameArt) | https://loafbrr.itch.io/wooden-docks | CC0 | 176 grid pieces (docks, props, fences). 15,856 tris total. One trimsheet, but 15 materials. 151 MB | B | 4 | Merge materials; replace the textured wood with palette colours |
| Fishing village | WizP | https://wizp.itch.io/fishing-village | Custom: any use, no resale | "Several models" (no list on the page), cute low-poly. **2 materials with a colour palette.** FBX/DAE/GLTF | A | 4 | Contents and triangle counts not listed |
| TIDELINE – Coastal Harbor (free sample) | Candle Light | https://candlelightgame.itch.io/tideline-coastal-harbor | Custom: commercial use and edits OK, no redistribution | Free sample is 12 of 252 models (full pack $3.20). Solid-colour materials, no textures. States "no AI" | A | 3 | The .blend source is only in the paid tier |
| Stylize Fish Market | LowPolyBoy (Fab) | https://www.fab.com/listings/c27207bd-1949-4734-9329-65b9f1ec2a5a | CC BY 4.0 | 68 meshes: fish stalls, crates, crab, octopus. 170k tris total, 3 materials, 6 textures (~1.1K); tagged "gradient". .blend | B | 4 | Decimate. The fish also work as joke melee weapons |
| Fishing Town | LowPolyBoy (Fab / Sketchfab) | https://www.fab.com/listings/43468395-d701-48e3-a3f3-53c3e2282938 · https://sketchfab.com/3d-models/907592e0212c41e9b23cf792dd962d88 | CC BY 4.0 | Whole medieval seaside scene: 192 meshes, **286k tris**, 3 materials, 1–2 textures | C → B | 3 | Kitbash coastal houses and piers; split up and decimate |
| Hand-Painted Fish Market Stall | duckcracker02 (Sketchfab) | https://sketchfab.com/3d-models/cf2ed4d11385403d980fea31a0102093 | CC-BY 4.0 | 4,132 tris, 1 material, 2 hand-painted textures | A/B | 3 | Repaint to flat colour |
| Stylized Pirate Island Pack | CGlads (Sketchfab) | https://sketchfab.com/3d-models/77d8d91e3fb14ef289850bc59d8683d6 | CC-BY 4.0 | Ships, **docks, piers**, huts, watchtowers, palms, barrels, crates. 10.7k tris, 1 material, 5 textures | A | 3 | Tropical; swap palms for pines |
| Stylized Rowboat & Wooden Dock | pixelzedge (Sketchfab) | https://sketchfab.com/3d-models/1617f14206314c27945649e07ffbcf05 | CC-BY 4.0 | Rowboat with oars + modular dock. 22k tris, 6 materials, hand-painted 2K | B | 3 | Decimate; style differs (hand-painted) |
| Pirate Kit (Nov 2023) | Quaternius | https://quaternius.com/packs/piratekit.html | CC0 | 71 models, textured, animated characters. Includes dock pieces and **cannons** (e.g. https://poly.pizza/m/J15vlPVvKK). The itch URL returned 404 | A/B | 4 | Cannons for the lighthouse-cape battery; harbour bits |
| Harbor Generator (Geometry Nodes) | n0mad | https://n0madcoder.itch.io/harbor-generator-blender-geometry-nodes | ISC | Blender geometry-nodes generator for dock layouts with 90° corners; placeholder textures | – | 2 | Harbour blockout only |

### 4.2 Boats

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Low Poly Sloop Sailing Ship | Razer820 (Sketchfab) | https://sketchfab.com/3d-models/1b27f1f60e0e49f886984ea099977757 | CC-BY 4.0 | A 1600s–1700s sloop. 10.7k tris, 2 materials, 3 textures | A/B | 5 | The anchored "hero" ship. Replace the skull-and-crossbones sail; separate the sails for the wind system |
| Low Poly Greek Fishing Boat | Muyaya Concept (Sketchfab) | https://sketchfab.com/3d-models/315a83fe28014d2f890acbe16f04a26f | CC-BY 4.0 | Traditional wooden fishing boat (kaiki). 4,890 tris, **1 material, 1 palette texture** | A | 4 | Recolour hull to period colours |
| Stylized Low Poly Rowboat with Paddles (+ lifebuoy, buoys) | Muyaya Concept (Sketchfab) | https://sketchfab.com/3d-models/f2c35c716f32474e96cce3625073e6b8 | CC-BY 4.0 | Rowboat 1,277 tris; lifebuoy 100 tris; red/green navigation buoys 1,294 tris each | A | 4 | The buoys look modern; the rowboat is the one to row |
| Full Low Poly Sea & Ships Pack | Muyaya Concept (Sketchfab) | https://sketchfab.com/3d-models/0a770f4c0a854c5ca805831cd5cb2bdd | CC-BY 4.0 | 12 models: **lighthouse**, sailing ships, rowboats, buoys. 100–800 tris each (30.7k total), but 19 materials / 16 textures | B | 4 | Merge into one atlas |
| Ships Pack | Quaternius | https://quaternius.com/packs/ships.html | CC0 | 6 simple ships (2018), flat untextured materials. FBX/OBJ/Blend | A | 4 | Palette assign |
| Pirate Ship – Low Poly | dudichies (Sketchfab) | https://sketchfab.com/3d-models/266c201cc3da4cd488e6ade363f7097d | CC-BY 4.0 | 4,813 tris, 10 materials, 1 texture | A/B | 3 | Distant ship |
| Ocean Assets – Starter Pack | Atomic Realm | https://atomicrealm.itch.io/ocean-assets-starter | Custom: commercial OK, **credit link required**, no redistribution | 11: pirate ship (5.1k tris), small boat, **crashed ship**, buoy, coral, submarine. One 1024 atlas | A | 3 | Shipwreck for the sea caves |
| Boats – PolyPack (itch) / Boats – PolyPack Starter (Fab) | ALSTRA INFINITE | https://alstrainfinite.itch.io/boats · https://www.fab.com/listings/43c1084d-4c14-46ce-a109-a3f51841f2ca | Custom: free commercial, **credit required**, no redistribution | Fishing boat, 2 wooden boats, kayaks, paddles, fishing rod (plus modern boats). One 48 kB texture. Fab version: 10+ boats, 1.4k tris, 256 palette | A | 3 | Remove the modern boats |
| Medieval Boat | AnyRPG (OpenGameArt) | https://opengameart.org/content/medieval-boat | CC0 | Rowboat, 356 tris, .blend | A | 3 | — |
| Fishing Boat – low poly | crookedmouth (OpenGameArt) | https://opengameart.org/content/fishing-boat-low-poly | CC0 | 772 polys + boat stand (dry dock) | A | 3 | Boatyard |

### 4.3 Fishing gear and fish

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| STYLIZED Deep Sea Hunter's Station | Daniel Mistage | https://www.fab.com/listings/b4878def-d341-4494-a244-62de2b3dcfa6 · https://daniel-mistage.itch.io/stylized-deep-sea-hunters-station | Fab Std (P0/Pro0) · itch: custom (games OK, no redistribution or NFTs) | 130–140+ props: harpoons, hooks, chains, crates, chests, lamps, charts, shelves, **fish heads, crab claws**, captain's-cabin furniture. 30–1,650 verts, 6 materials, 2K atlas + 1K. UE 5.5–5.8 | A | 5 | Best free harbour and fishing clutter kit; matches the Armory pack |
| Animated Cute Fish Pack | Quaternius | https://quaternius.com/packs/cutefish.html | CC0 | 52 animated models: fish **plus fishing rods and lures**. Flat untextured materials | A | 5 | Fish-market stock, fishing minigame |
| LowPoly Animated Fish | Quaternius | https://quaternius.itch.io/lowpoly-animated-fish | CC0 | Fish, shark, dolphin, manta, whale; rigged with swim animation | A | 4 | — |
| Swordfish | Quaternius (poly.pizza) | https://poly.pizza/m/7hMOlBjln0 | CC0 | Animated swordfish | A | 5 | Also the "swordfish" melee skin (§8.4) |
| Low-Poly Net | TepidGames (Sketchfab) | https://sketchfab.com/3d-models/c75a54c229e447ba81e1220537a4fd10 | CC-BY 4.0 | Fishing net, 5.1k tris, 2 materials, no textures | A | 4 | Same author also has rods, hooks and a bluegill (976 tris). His coiled rope is 20–40k tris, too heavy |
| Island Pack Lite | Shawk Studios (Fab) | https://www.fab.com/listings/9a28853a-3d91-40c7-9575-104f7ad42952 | Fab Std (P0/Pro0) | 36 models: net, fishing rod, fish, fish bucket, buoy, barrel, raft, port, market, tower. **1 material, 256² texture, 2,156 verts total** | A | 3 | Tropical look, but tiny and clean |
| fishing hut 3d models pack | dashastepnova (Sketchfab) | https://sketchfab.com/3d-models/bfa967e7af37489a81c1b25fe50abf67 | CC-BY 4.0 | 13: barrels, boats, buckets, **anchor**, 3 fish, lifebuoy, deck/house. 44–840 faces each, but 16 materials and textures up to 4K | B | 3 | Re-atlas |
| Crab trap | pelmeshko_tf2 (Sketchfab) | https://sketchfab.com/3d-models/cc703842da4c410ba38d8b53e23b46cf | CC-BY 4.0 | Lobster/crab trap, 1,300 tris, 1 material, realistic 4K with transparency | B | 3 | Bake to flat colour; remove alpha |
| Fish – PolyPack | ALSTRA INFINITE | https://alstrainfinite.itch.io/fish | Custom: free commercial, credit required | 4 fish + 1 shark, one texture | A | 3 | — |
| Frello's Anchor | Bram Verheyen (Sketchfab) | https://sketchfab.com/3d-models/d3c352bb36ee4e01ba242c778ec1b02c | CC-BY | Stylised seaport anchor, 4,961 faces | A/B | 4 | Harbour decoration or melee skin |
| Low Poly Medieval Props Small Pack | RG Poly | https://rg-poly.itch.io/medieval-props-small-pack-low-poly | CC0 | Vise, broom, archery targets, **boat frame, laundry on a line**. **One atlas**, 3 colour variants | A | 4 | Boatyard, washing lines (wind system) |

### 4.4 Lighthouse and cape

None of these can be entered. The map calls for a 333-step enterable lighthouse, so model the interior shell yourself (spiral stair, lamp room) and use one of these as the look reference.

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Faro Costero Low Poly (Coastal Lighthouse) | Blender Pink (Fab) | https://www.fab.com/listings/774306de-40ff-4454-8c44-a95536fea359 | Fab Std (P0/Pro0) | 18 m lighthouse, 4,478 tris, **vertex colour, no textures**, 2 materials. Blender/GLB. Currently €0; the seller's text says "free for a limited time" (no end date on Fab) | A | 5 | Claim soon. Needs a vertex-colour material, or remap to palette |
| The Lighthouse | cotman_sam (Sketchfab) | https://sketchfab.com/3d-models/1a85945dd2a840f594bf6cb003176a54 | CC-BY 4.0 | **18th-century** lighthouse + fishing cottage. 13.3k tris, but 34 materials and 31 hand-painted 512 textures | B | 5 (period look) | Big re-atlas or vertex-paint job; best period reference |
| Lighthouse (#SketchfabWeeklyChallenge) | Bob.Ho (Sketchfab, also on Fab) | https://sketchfab.com/3d-models/963185c272f749e586386d000bfadcb3 | CC-BY 4.0 | Cartoony. 1,822 tris, 3 materials, 1 texture | A | 4 | — |
| Lighthouse, Low Poly | flaremedia (Sketchfab) | https://sketchfab.com/3d-models/8cbfa6920c8e45a9a9abf37adb74dbf8 | CC-BY 4.0 | 1,029 tris, 4 flat materials | A | 3 | Easy to hollow out |
| Lighthouse Stylized Low-Poly | Wirefall Studio (Fab) | https://www.fab.com/listings/a45bc324-53a9-4dbb-b60f-ab0613059bbc | Fab Std (Personal free, Pro €6.14) | 13 meshes, 13.9k tris, 7 materials, 2K textures. blend/fbx/obj | B | 4 | — |
| Free Lighthouse | Daniel Dormin | https://daniel-dormin.itch.io/lighthouse | Custom: credit not required | Lighthouse, land, rocks + combined scene. Blend/FBX/GLB | A? | 3 | Triangle count not stated |
| 3TD Harbour Pack | Ron Kapaun / 3TD (OpenGameArt) | https://opengameart.org/content/3td-harbour-pack | CC0 | Lighthouse on a rock, dock, wharf, shack. DAE, 18 MB textures (2014) | C | 2 | Dated |
| Lighthouse (various) | jeremy; Jarlan Perez (poly.pizza) | https://poly.pizza/m/0SWQTv1whoA · https://poly.pizza/m/d4j9R8L8xpE | CC-BY 3.0 | Old Google Poly-era models *(partial: triangle counts not shown)* | A | 2 | — |

---

## 5. Terrain and nature: coast, cliffs, mountains, meadows, forest, water

### 5.1 Coast, cliffs, rocks, beach

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Modular Cliffs / Modular Cliffs 2 | SimplePolygon | https://simplepolygon.itch.io/modular-cliffs · https://simplepolygon.itch.io/modular-cliffs-v2 | Custom (credit, no redistribution) | Stackable cliff and island edges. Cliffs 2 ≈ 12.5k tris for the set. Palette | A | 5 | Stack into sea stacks and the lighthouse cape |
| Basalt Rocks | SimplePolygon | https://simplepolygon.itch.io/free-basalt-rocks | Custom (credit) | Basalt columns, ~6.5k tris for the set. Palette | A | 5 | Sea stacks, cave walls |
| Modular Terrain Pack | Fertile Soil Productions | https://fertile-soil-productions.itch.io/modular-terrain-pack | CC0 | Hills, escarpments/cliffs, **beach**, waterfalls, streams, **caves**. Vertex colours + several materials per model. OBJ only; some normals flipped | A/B | 4 | Fix normals, scale ×100 for UE (cm), merge materials |
| Low Poly Cliffs & Rocks | Jay Creations (Fab) | https://www.fab.com/listings/5665a40c-01f5-42c4-8e2b-2c0d92ab686e | Fab Std (P0/Pro0) | 51 meshes: 17 cliffs, **6 vista mountains**, 17 rocks. <4k tris each, LODs. Triplanar colour-variation material with snow/moss top. UE 4.26–5.5 (Unreal project only) | A | 5 | Migrate to 5.8. To edit in Blender, export from UE |
| Low poly rocks and trees | Trekkerac | https://happykeys.itch.io/rocks-and-dead-trees | CC0 | Rocks and **dead trees**. Uses the MilkAndBanana gradient palette | A | 4 | The "Dry Tree" landmark candidate |
| Free Pack – Rocks Stylized | PolyOne (Fab) | https://www.fab.com/listings/a0746c4b-428b-4556-b4c9-98f70c2a30d4 | Fab Std (Personal free, Pro €1.74) | 11 rocks, 3.1k tris, 1 material, one 4K base colour. UE 5.7–5.8 + DCC formats | A (at 1K) | 4 | Downscale texture |
| Smuggler's Cove | Poly Haven (Fab) | https://www.fab.com/listings/a0935013-5959-47c2-97d9-75478ded0e6b | Fab shows CC-BY (Poly Haven content is normally CC0) | 17th-century Dutch ships and pinnace (100–150k tris), 7-piece wooden pier, props, sand. Realistic PBR, Nanite | C | 2 | Period reference only |

### 5.2 Mountains (visual ring, 2–3 km)

| Name | Author / Source | URL | License | Contents | Mobile | Fit | Notes |
|---|---|---|---|---|---|---|---|
| Mountain low poly (for distant mountains) | ahmagh2e (Sketchfab) | https://sketchfab.com/3d-models/cb7f28b5ee0e4ddfb12700ff9d9d35c8 | CC-BY 4.0 | 5k tris, 1 material, 3 textures | A | 4 | Background ring; also usable as an impostor source |
| Low Poly Mountain Free | lowpolyartwork (Sketchfab) | https://sketchfab.com/3d-models/dabda46f9be2416c93a4b584be17786b | CC-BY 4.0 | 5.1k tris, 2 flat materials, no textures | A | 4 | — |
| Hills 'n Mountains | HakanBacon | https://hakanbacon.itch.io/hills-n-mountains | Custom: any project, credit optional | 4 centre pieces, 9 hills with ramps/cliffs, mountains. Swappable textures (snowy, muddy, rocky). .blend costs $1+ | A | 4 | — |
| Vista mountains (in Low Poly Cliffs & Rocks) | Jay Creations (Fab) | see §5.1 | Fab Std | 6 vista mountains | A | 4 | — |

### 5.3 Meadows, forest, trees (incl. dead tree)

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Stylized Nature MegaKit | Quaternius | https://quaternius.itch.io/stylized-nature-megakit | CC0 | 116 models (free 81; Pro $9.99; Source $14.99 adds grass/leaf wind shaders for UE). 40 trees (7 swappable leaf types), 35 plants and flowers, 27 rocks, grass, bushes. Textured. **No water** | A/B | 5 | Style anchor. Drive WPO wind with `UKGWindSubsystem` |
| KayKit Forest Nature Pack | Kay Lousberg | https://kaylousberg.itch.io/kaykit-forest | CC0 (Extra $9.99 adds colour variants and terrain) | 100+ free: trees, bushes, rocks, 2 grass styles. **Gradient atlas** | A | 5 | Cheapest trees for mobile |
| Pine Tree Pack (+ Forest Bits) | SimplePolygon | https://simplepolygon.itch.io/pine-tree-pack · https://simplepolygon.itch.io/forest-bits *(Forest Bits partial)* | Custom (credit) | 3 pines, ~2.7k tris total. Palette. Forest Bits: mushrooms, rocks | A | 5 | New England spruce/pine forest |
| Low Poly Pine Trees | TheTeaGuns | https://theteaguns.itch.io/low-poly-pine-trees | CC-BY 4.0 | 3 trees (normal + snowy) + 2 stumps; one shared texture with Evergreen/Pine/Alien variants | A | 4 | — |
| Vertex color trees set | vertexcat | https://vertexcat.itch.io/vertex-color-trees-set | CC0 | 6 trees, 116–597 tris, **vertex colours, no texture** | A | 4 | Remap vertex colours |
| Low Poly Nature: Essentials | Vertex Rage Studio (Fab) | https://www.fab.com/listings/a607441d-b811-440b-a20c-59e74804c4ce | Fab Std (Personal free, Pro €4.38) | 62 meshes (trees, bushes, grass, plants, rocks, fences), 32–1,612 polys, 2 master materials, **one 512 palette** | A | 5 | Water in the screenshots is not included |
| Low Poly Simple Nature Pack | JustCreate (Fab) | https://www.fab.com/listings/dbc49f41-e739-40c4-a09c-3587d30180de | Fab Std (P0/Pro0) | 24 meshes, LODs, **1 material, one 1024 texture**. UE 5.2–5.7 | A | 4 | — |
| Stylized Low Poly Nature Lite | JustCreate (Fab) | https://www.fab.com/listings/fef6b0c6-19a4-47bc-913f-c8328910cec4 | Fab Std | 6 models, FBX, UE demo | A | 3 | — |
| Lowpoly Environment – Nature – Free | Polytope Studio (Fab) | https://www.fab.com/listings/d9327821-8977-439a-b2b7-d0a53e4c8728 | Fab Std (P0/Pro0) | 25 meshes, LODs 0–2, 15 materials, shader with wind and colour gradients. UE 5.4–5.7 | A/B | 4 | — |
| FREE Stylized Foliage Pack | StyleHex (Fab) | https://www.fab.com/listings/b494d1c5-8d02-48c4-9038-54bb8970a26f | Fab Std (P0/Pro0) | 29 meshes, LODs, vertex colour, 2 materials, 256 textures. UE 4.27–5.8. Uses runtime virtual textures to blend with terrain | A/B | 4 | The virtual textures cost extra on mobile |
| FREE Stylized Forest Sample | StyleHex (Fab) | https://www.fab.com/listings/3c1a31b6-e523-4a0f-97d0-26b0db69b6bc | Fab Std | Stylised trees and foliage | B | 3 | — |
| ToonLab Toon Nature Pack | ToonLab (Fab) | https://www.fab.com/listings/3dc642fe-69e4-407c-ac3f-d57ac6db2ede | CC-BY | 20 models × 3 colour variants, one texture, built for mobile | A | 4 | — |
| Willowwood Nature Pack | BluBluGames (Fab) | https://www.fab.com/listings/02523a32-55b0-45df-ac12-6a418d8574ef | Fab Std | Trees, willows, water, bridge, ruins. UE 5.1 project + FBX | B | 4 | Stream and bridge |
| Ultimate Nature Pack (2019) | Quaternius | https://quaternius.com/packs/ultimatenature.html | CC0 | 150 models, flat colours | A | 3 | Older style |
| Ultimate Crops | Quaternius | https://quaternius.com/packs/ultimatecrops.html | CC0 | 102 models, 5 growth stages | A | 4 | Vegetable garden, farm |
| KayKit Halloween Bits / Spooktober | Kay Lousberg | see §10.5 | CC0 | **Dead trees**, scarecrow | A | 5 | Dry Tree landmark |
| Low Poly Tree Pack | Broken Vector | https://brokenvector.itch.io/low-poly-tree-pack | **NOT STATED** | 38 trees, 4 colour schemes incl. "dry" | A | 3 | Confirm licence |
| Grimm Assets Landscape Pack | EspadadaManha | https://espadadamanha.itch.io/grimm-assets-low-poly-landscape-pack | **NOT STATED** | 5 creepy dead-style trees + 6 hills | A | 3 | Confirm licence |

### 5.4 Water and sea

| Name | Author / Source | URL | License | What it is | Mobile | Fit | Notes |
|---|---|---|---|---|---|---|---|
| Water Materials | tharlevfx (Fab) | https://www.fab.com/listings/063155ea-d9d2-4f29-b09f-33270b0bc861 | CC BY 4.0 (checked on the page) | 12 water types (ocean, river, waterfall, pool, pixel) + 8 cheaper variants. 26 materials, 13 material functions, vertex-paint driven. UE 4.13–5.8. 278 ratings | A/B (cheap variants) | 4 | Base for the toon sea: add banded depth colour and foam lines |
| FANTASTIC – Village Pack water | Tidal Flask (Fab) | see §2 | Fab Std | Stylised water included | A | 4 | — |
| VaOceanMobile | ufna (GitHub) | https://github.com/ufna/VaOceanMobile | MIT | Mobile/web ocean shader. README says UE 4.9–4.27 and 5.0–5.5 | A | 3 | Mobile fallback; built mainly for top-down views |
| fishies | maythaswang (GitHub) | https://github.com/maythaswang/fishies | MIT | UE5 stylised ocean: Gerstner/sine waves, depth colour, fresnel, caustics | ? | 3 | Learning project; lift out the materials |
| Water Physics plugin | Mans Isaksson (Fab) | https://www.fab.com/listings/bfc6b8a2-967e-45a8-bfa7-d947340992a8 | Fab Std, €0 | Boat buoyancy that works with UE's Water system | – | 4 | Rowable boats |
| Water Planes (sample) | Epic Games (Fab) | https://www.fab.com/listings/d6355850-4bff-41e3-9e65-5403bb0a2e6b | Fab Std, €0 | Ocean with displaced waves and foam peaks + blueprints (realistic) | B | 3 | Restyle to toon colours |

---

## 6. Characters (and the jaw/mouth question)

**Context:** `06_Art_Direction.md` proposes **painted wooden puppets**, pending approval. They would use rigid parts, a hinged jaw and a humanoid skeleton. If that direction is approved, packs matter for **animation** (§7), **cosmetic shapes** (hats, hair, clothing references) and the **fallback** "stylised humans" route, not for the body.

**Jaw summary:** among the free options, only **Synty Sidekick** (free starter) and **styloo The Company** (CC0) ship a working mouth rig. Everything else needs a jaw bone added in Blender.

| Name | Author / Source | URL | License | Contents & poly style | Rig / jaw? | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|---|
| Universal Base Characters (UBC) | Quaternius | https://quaternius.itch.io/universal-base-characters · https://quaternius.com/packs/universalbasecharacters.html | CC0 (free: 2 bodies + 5 hairstyles; Source $19.99+: 6 bodies, 20 hairstyles, eye/skin shaders, .blend, UE 5.4.4 project) | 6 bodies (Superhero, Regular, Teen × M/F), **~13k tris each** | Humanoid rig shared with Universal Animation Library and the Outfits pack; tested in UE 5.4.4+. **No jaw, face bones or shape keys documented.** UE Mannequin naming not stated | B (decimate to 6–10k) | 4 (fallback route) | Add a `jaw` bone as a child of `head` and weight chin and lower lip. Add a dark mouth-interior plane. Optional `MouthOpen` shape key → morph target |
| Modular Character Outfits – Fantasy | Quaternius | https://quaternius.itch.io/modular-character-outfits-fantasy | CC0 (Source $20+) | 12 outfits in 62 parts (82 in Source), 3 texture variants each: peasant, noble, wizard, knight, ranger, crowns/hats. v2.1 (Jul 2026) reduced clipping | Same rig as UBC | B | 5 | Split hats, collars and aprons into socketed cosmetics. Model the puritan capotain hat, coif and white collar on these bases |
| Ultimate Modular Men / Women (2022) | Quaternius | https://quaternius.com/packs/ultimatemodularcharacters.html · https://quaternius.com/packs/ultimatemodularwomen.html | CC0 | 11 men / 10 women, 4 swappable parts each, 24 animations, flat colours | Own rig (the Women page mentions a humanoid version). No jaw | A | 2 | Older; prefer UBC |
| RPG Character Pack (2020) | Quaternius | https://quaternius.com/packs/rpgcharacters.html | CC0 | 6 textured, rigged, animated fantasy characters | No jaw | A | 2 | — |
| KayKit Adventurers | Kay Lousberg | https://kaylousberg.itch.io/kaykit-adventurers | CC0 ("don't resell unmodified"). Free: Knight, Barbarian, Rogue (+hooded), Mage, Ranger. Extra $7.95 adds Engineer, Druid and Barbarian_Large with 3 alternate textures each; Source $11.95 | Chibi low-poly. **Body, head, arms and legs are separate rigid meshes.** One gradient atlas, 25+ accessories incl. crossbows, removable helmet | "Rig_Medium", shared by all KayKit characters (23 bones according to a third-party PR; *unverified*). Not UE Mannequin; retarget. Faces are in the texture | A | 4 | **Closest free match to the puppet approach**: cut the head into upper and lower halves, add a jaw bone, weight the lower half 100% |
| KayKit Skeletons | Kay Lousberg | https://kaylousberg.itch.io/kaykit-skeletons | CC0 (free: 4 skeletons) | Gradient atlas | Rig_Medium | A | 2 | Ghoul or spectral NPC |
| KayKit Spooktober (Legacy) | Kay Lousberg | https://kaylousberg.itch.io/kaykit-spooktober | CC0 | "Jack" and "Wendy" characters + dead trees, fences, tombstones | KayKit rig | A | 3 | — |
| **Sidekick Modular Characters – FREE Starter Pack** | Synty | https://www.fab.com/listings/8d8e9639-d93f-4f1d-8332-32ae0ef14bca · https://syntystore.com/products/sidekick-modular-characters-starter-pack | Fab Std (P0/Pro0) / Synty EULA (5 seats, no redistribution, no generative-AI use) | 57 sci-fi civilian + fantasy knight parts, 91 human base parts. **1 material, 32×32 palette texture.** UE 5.3–5.8 | "Unreal Engine Mannequin compatible"; Unity Humanoid. **Facial blendshapes (ARKit-style):** brows, eyes, blink, cheeks, **jaw**, full mouth set, tongue. Separate teeth and tongue meshes | A/B | 5 (for the mouth) | Drive the jaw-open morph from `JawOpen`. The Sidekick plugin (https://www.fab.com/listings/911a0dcd-c9d2-4850-95c2-823dbbd4959d) is Windows-editor only and rated 2.2; **bake each character to one mesh** so the game doesn't depend on it. Check the Synty EULA before using it in the house editor (§17) |
| The Company Characters | styloo | https://styloo.itch.io/company | CC0 (.blend needs $3+) | 5: elf with bow, sorcerer, knight, dwarf, dragon. **8–10k tris**, 2048² textures, GLB/FBX | **Rigify rigs with facial rigs (eyes, mouth, hair)**; the author says the weighting is "not perfect". No animations (use Mixamo) | B | 3 | Study its mouth rig. Fantasy types, not townsfolk |
| Creative Characters FREE | ithappy (Fab) | https://www.fab.com/listings/94fd60a2-5659-4fc4-af1d-a8cdd2681c2e | Fab Std (P0/Pro0) | 30 parts, 34k tris total, 2 materials, one 1024 palette, 14 animations | Humanoid rig (Mixamo-compatible). Expressions come from **swapping face meshes**, not blendshapes | A | 4 | Face-swap approach is like the planned eye/brow flipbook |
| Stylized NPC – Peasant Nolant (DEMO) | Winged Boots (Fab) | https://www.fab.com/listings/d49af8e0-b16c-4f1f-849f-48b30bd59740 | Fab Std (P0/Pro0) | Peasant, 2,073 tris, 2K textures, idle + walk. Unity/FBX | Rigged; no jaw mentioned | A | 4 | Townsfolk NPC reference |
| Free Medieval 3D People Low Poly Pack | Free Game Assets (CraftPix) | https://free-game-assets.itch.io/free-medieval-3d-people-low-poly-pack | Custom: "royalty free usage in unlimited projects" | 14 models: peasants, traders, king, queen, entourage. **64×64 textures**, FBX. Triangle count not stated | Rigged, **no animations**, no jaw | A | 3 | Villager NPC base |
| Ghost | Quaternius (poly.pizza) | https://poly.pizza/m/Iip30bDHmu | CC0 | Animated ghost | – | A | 3 | — |
| Ghost Character | Polygonal Mind (poly.pizza) | https://poly.pizza/m/CKLHPoYhE9 | CC0 | Ghost | – | A | 3 | Recommended instead: player mesh + fresnel/translucent "Ghost Cyan" material |
| Mixamo characters | Adobe | https://www.mixamo.com | Adobe terms (see §7) | Mostly semi-realistic | 65-joint skeleton, **no jaw** | – | 1 | Use Mixamo for animations only |
| *Reference only:* Stylized Girl with blendshapes | 2Fasts | https://2fasts.itch.io/stylized-girl-character-rigged-humanoid-game-ready-with-blendshapes | **Licence not stated; paid $19.99** | ~67k tris | 53 bones incl. jaw; 20 blendshapes incl. A/E/O/F visemes and MouthOpen | C | 1 | Reference for viseme naming |

**Voice-driven mouth, asset side:** `05_Tech_Architecture.md` already defines the runtime chain (EOS RTC PCM → envelope follower → `JawOpen` 0–1). Assets must provide:

1. **The jaw bone.** Every character skeleton has a bone named `jaw`, a child of `head`, pivoting just below the ears. The rest pose is mouth closed, and the jaw opens by rotating around the local pitch axis.
   - **Puppet or KayKit-style heads:** split the head into two rigid pieces and weight the lower piece 100% to `jaw`.
   - **UBC-style skinned heads:** weight-paint the chin and lower lip, and add a dark mouth-interior mesh so the gap doesn't look hollow.
2. **Optional morph fallback.** Add a `MouthOpen` shape key; it imports as a morph target in UE with "Import Morph Targets" on. A bone rotation is cheaper than a morph on mobile.
3. **AnimBP.** After locomotion, add a **Transform (Modify) Bone** node on `jaw`: Rotation mode "Add to Existing", Parent Bone Space, pitch = `JawOpen` × ~25°. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-transform-bone-in-unreal-engine
4. **Retargeting.** The IK Retargeter leaves an unmapped `jaw` alone, so drive it after retargeting. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine
5. **Fallback if engine VOIP is used instead of EOS RTC.** `UVOIPTalker::GetVoiceLevel()`: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UVOIPTalker

How R.E.P.O. itself does this (the top half of the head hinging with voice volume) is an observation of the game, not a documented source. The game is Unity, by Semiwork.

**Blender rigging tools** (all verified pages):

| Tool | URL | Licence and price | Notes |
|---|---|---|---|
| Auto-Rig Pro | https://superhivemarket.com/products/auto-rig-pro | GPL; $50 full, $25 lite | Exports UE4/UE5 Mannequin naming and axes, facial rig, shape keys, Remap retargeting. Blender 2.93–5.2 |
| Rigodotify | https://felesmachina.itch.io/rigodotify | Price not captured | Rigify extension that outputs a UE/Unity/Godot game skeleton with a **jaw under the head**, plus metarigs tuned for Quaternius packs |
| BlenderTools (Send to Unreal) | https://github.com/EpicGamesExt/BlenderTools | MIT | Blender → UE transfer |
| Mixamo Rig | https://extensions.blender.org/add-ons/mixamo-rig/ | GPLv3, free | Blender 4.2 LTS; not supported on 5.5+ |
| Rigify | Built into Blender | GPL, free | Face metarig has a jaw. The DEF- bones need cleanup for game export |

---

## 7. Animations (third-person)

| Name | Author / Source | URL | License | Contents | Skeleton / UE | Fit | Notes |
|---|---|---|---|---|---|---|---|
| Universal Animation Library (UAL1) | Quaternius | https://quaternius.itch.io/universal-animation-library | CC0 (45 free; Pro $9.99 = 120+; Source $14.99 = .blend) | 8-direction locomotion (walk, jog, sprint), push, crawl, swim, sit, **death**, combat and gun, emotes. Clip names include `Idle_Loop`, `Walk_Loop`, `Jog_Fwd_Loop`, `Sprint_Loop`. Root motion and in-place versions | Universal humanoid rig (same as UBC and Outfits since v2.0, Jan 2026). GLB for UE 5+. Mixamo-compatible. Retarget with the IK Retargeter | 5 | If clips jitter in UE, set animation compression to default |
| Universal Animation Library 2 | Quaternius | https://quaternius.itch.io/universal-animation-library-2 | CC0 (42 free; $14.99 = 130+ and Source) | Locomotion **including carrying**, 3- and 4-hit **melee combos**, sword, parkour and climbing, farming, fishing, idles and emotes (**talking, lantern, waving, coughing**), knockback and death, zombie | Same rig | 5 | Carrying locomotion covers package delivery; talking idles suit meetings |
| KayKit Character Animations | Kay Lousberg | https://kaylousberg.itch.io/kaykit-character-animations | CC0 (all 161 free; Source $14.99) | Idle, hit, death, spawn, **interact**, walk, run, jump, crawl, sneak, dodge, **crouch**, 1H/2H/unarmed/dual melee, block, ranged and spells, wave, cheer, **sit, lie down**, dig, **lockpick**, fish, hammer | Rig_Medium (100+), Rig_Large (25+) | 4 | Lockpicking for the Locksmith/Smuggler roles. No carry animation listed |
| Mixamo | Adobe | https://www.mixamo.com | Adobe FAQ (read on an Adobe Community copy): free, royalty-free, commercial games allowed, no credit required; you may not redistribute the raw character or animation files. Needs an Adobe account; Adobe can change the terms | ~2,300 clips. Confirmed: "Floating" (ghost), "Bayonet Stab", "Being Carried" (3 variants), seated idles. Stabbing, crouch-walk, pickup and dying clips very likely exist *(names not verified)* | 65-joint skeleton → IK Retargeter | 4 | Keep a copy of the terms page with your files |
| Kevin Iglesias | kevdev | https://kevdev.itch.io/ | Commercial use OK, no credit needed | Free: **Basic Motions FREE** (118 files: idles, 8-way run, sprint, jump, turns, talk), **Crafting/Villager FREE** (62: farming, fishing, gather, hammer, mining). Paid: Basic Motions $18 (eat, drink, loot, sit, sleep, social), Crafting $25 (**carry, pick up/drop, deaths**), Melee $23, Throwing $12, 4-pack $65 | Generic humanoid with a `B-root` bone. "Godot/Unreal" FBX on itch; the author notes UE retargeting quirks | 3 | Villager work loops |
| Game Animation Sample (GASP) | Epic Games | https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016 | Epic content: UE-only, commercial OK | 500+ motion-matching animations (locomotion, vault, mantle), updated for 5.8 | UEFN Mannequin | 3 | Heavy for mobile; source of locomotion to retarget |
| Rokoko free mocap packs | Rokoko | https://www.rokoko.com/resources/download-263-rokoko-motion-capture-assets | Free, commercial use in games allowed (email sign-up) | 263 clips, FBX, 30 fps | Preset for Mixamo, UE4/UE5, HumanIK | 2 | Mocap style clashes with the cartoon look |
| ActorCore free motions | Reallusion | https://actorcore.reallusion.com/3d-motion/free | **Games need a separate distribution licence** (per CG Channel) *(partial)* | 32 motions | – | 1 | Avoid |

**Coverage check:**

| Need | Free source |
|---|---|
| Locomotion | UAL1 |
| Carry object | UAL2 carrying locomotion |
| Interact | KayKit Interact |
| Knife / melee | UAL2 melee combos, KayKit 1H melee, Mixamo "Bayonet Stab" |
| Death | UAL1, UAL2, KayKit |
| Ghost float | Mixamo "Floating", or a sine-wave bob in the AnimBP |
| Sit / sleep | UAL1, KayKit |
| Lockpick | KayKit |
| Fishing | UAL2, KayKit |
| Hammer | KayKit |
| Talking idles | UAL2 |
| Lantern idle | UAL2 |


---

## 8. First-person viewmodel (CS2-style)

Art direction targets: arms 4–6k tris, weapons 3–8k tris with their own 512–1024 px texture, one material. Each weapon needs draw, idle, **inspect**, attack and reload animations, plus bob and sway.

Almost every free source below comes in well **under** those triangle budgets. There is room to add bevels and detail, or to keep them lean for mobile.

### 8.1 Arms and hands rigs

| Name | Author / Source | URL | License | Contents, polys, material approach | FP anims included | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|---|
| **PSX First Person Arms** | Drillimpact | https://drillimpact.itch.io/psx-first-person-arms-free | **CC0** | ~1,176 faces (on the Sketchfab copy). FBX/GLB/.blend. **512² hand-painted texture**, bare-hand and black-glove versions. Bones cleaned up in a recent update | **18:** relax, push L/R, jab L/R, guard_draw, guard_idle, grab L/R, finger_gun idle/fix/fire/broken, **knife_draw, knife_idle, knife_hit_01, knife_hit_02** | A | **5** | Repaint as a colonial sleeve (linen cuff, wool coat) or as puppet arms with a ball-jointed wrist. Add a `weapon_r` / `grip` socket bone. The knife set covers the melee slot today |
| WRAD ARMS | wriks | https://wriks.itch.io/wrad-arms | **CC0** | 1,200 tris, 512² texture, 2 skin tones, IK rig. GLB/FBX/OBJ. Half-Life 1 style | None | A | 4 | Second base; you author all animations |
| Low Poly Cartoon Hands | Dandushik (Sketchfab) | https://sketchfab.com/3d-models/ac4362b21c094491a705cb671a9aee52 | CC-BY | 1,196 faces, 1 material, **no texture (flat colour)**. The rigged .blend is linked from the description (Google Drive) | None | A | 4 | Flat colours fit the palette. Keep the Drive-linked file with your licence records |
| cartoon FPS Arms | bumstrum (Sketchfab) | https://sketchfab.com/3d-models/25d06c227fa3419b92fe65f39887b0b8 | CC-BY | 1,264 faces, one 1K texture, rigged | 1 (name not stated) | A | 4 | Cartoon proportions fit; retexture the sleeves |
| Unreal Engine FPS Arms | conceptyphoon (Sketchfab) | https://sketchfab.com/3d-models/3978de7e44404707a732d2745db8a5c7 | CC-BY, but **built from Epic's GASP Mannequin** (Epic content, UE-only) | 3,756 faces; **keeps the UE5 Mannequin bone hierarchy** | None | A/B | 4 | Epic/Fab first-person animations plug in with no retargeting. Credit both sources; UE-only is fine here |
| Rigged Low Poly FPS Hands | hvarley (Sketchfab) | https://sketchfab.com/3d-models/c0d32b85e1ff4c4aa710d416545104b1 | CC-BY 4.0 | 2,148 faces, 2 materials, 1 texture; Counter-Strike 1.6 style | None | A | 3 | The SMG in the preview isn't the author's; use the arms only |
| Fps Arms Rigged (+ double-barrel shotgun) | TALLDOOR (Sketchfab) | https://sketchfab.com/3d-models/8b0c7222ea954c8992da5f33031b12d4 | CC-BY (includes bumstrum's arms) | 7,372 faces, PS1-style double-barrel, 4 materials | **8** (names not stated) | B | 3 | Reference for break-action reload timing |
| First Person arms | bumstrum (Sketchfab) | https://sketchfab.com/3d-models/e3c42c05b22944e5839deb8e003f0987 | CC-BY | 7,240 faces, 2K hand-painted, gloves | None | B | 2 | Fallback |
| Low Poly FPS Rifle and Hands | Robin Lamb (OpenGameArt) | https://opengameart.org/content/low-poly-fps-rifle-and-hands | **CC0** | Rifle + hands, Blend/FBX/glTF/OBJ, 162 KB | Shoot | A | 3 | Prototype |
| fps arms (rigged only) | para (OpenGameArt) | https://opengameart.org/content/fps-arms-rigged-only | **CC0** | ~8k tris (MakeHuman), 1024² texture, **IK rig + handle bone** | 1 crude sample | B | 2 | Study the handle-bone setup |
| Low Poly FPS Arms Pack | NetSriK (OpenGameArt) | https://opengameart.org/content/low-poly-fps-arms-pack | CC-BY 4.0 | 5 arm-with-weapon sets + 14 guns, textured, .blend | None | B | 2 | — |
| PSX-Weapons Assets | Kuptchi | https://kuptchi.itch.io/f | "Free to use by anyone, for anything without any need to credit us" (no named licence; don't resell) *(weapon list partial)* | Arms + 4 rigged weapons, magazines, shells, muzzle flashes. FBX/.blend, 482 MB | **80+** (names not stated) | B | 3 | Largest free first-person animation library found. Ask for a named licence before shipping |
| Knife FPS Animations | Artem Dubinin (Fab) | https://www.fab.com/listings/ccff0d84-8219-4daf-9859-348b393b3d83 | Fab Std (P0/Pro0) | First-person hands on the **UE5 skeleton** (5,700 faces) + karambit. 15 hand + 3 knife animations, FPS blueprint, placeholder SFX. UE 5.3–5.6. **Fab flag: "Generated with AI: yes"** | 15 + 3 (clip list not captured) | A | 4 | Triggers Steam AI disclosure if shipped. Fine as a timing reference |
| UE First Person template (5.6+) | Epic Games | https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-template-in-unreal-engine | Epic content (UE-only) | Full-body Mannequin with First Person Rendering. The Arena Shooter variant adds pistol, rifle and grenade-launcher animations (`Content/Variant_Shooter/Anims`) | Pistol/rifle sets | PC only (rendering path) | 4 | Reference setup and skeleton |
| PSX FP Hands Pack | imaginais | https://imaginais.itch.io/psx-fp-hands-pack-rigged | **NOT STATED** | Rigged Blender file | Idle, "I'm cold", hat idle, **drink** | A | 2 | Fun social emotes, but unusable until licensed |

### 8.2 First-person animation sets

| Name | Author / Source | URL | License | Weapon / clips | Fit | Notes |
|---|---|---|---|---|---|---|
| Concrete Block FPS Animation Pack | YushFX (Fab) | https://www.fab.com/listings/76bce009-f180-4a08-bf7f-c7ade02ca63a | Fab Std (P0/Pro0) | 26 first-person animations: walk, run, sprint, **attack**, **take item**. Epic skeleton with IK bones, camera on the head joint. UE 5.3–5.8 | 4 | Swing template for the frying pan, hammer and fish |
| [Animated] Tomahawk | CoinCoin (OpenGameArt) | https://opengameart.org/content/animated-tomahawk | CC-BY 4.0 | First-person melee: idle, walk, run, ATK1, ATK2, ATK1+2 (with and without hit), jump set. glTF | 4 | Archetype for the "one-hand blunt/blade" melee set |
| Pirate Sword (Animated) | Lucian Pavel (OpenGameArt) | https://opengameart.org/content/pirate-sword-animated | **CC0** | 2,063-tri cutlass with first-person arm animations, 2K textures (clip names not stated) | 4 | On theme |
| Super Simple FPS Pack | Zero Flex Gaming (Fab) | https://www.fab.com/listings/107074c0-ac5e-47f7-9003-2df3658569a6 | Fab Std (P0/Pro0) | M4 + pistol animations for the UE5 skeleton (aim down sights, tactical sprint, crouch), recoil system, weapon data table. Uses the default UE5 arms; UE 5.5 | 3 | Recoil and weapon-table logic |
| Stylized Low Poly Botanic Pistol | commiessar (Sketchfab) | https://sketchfab.com/3d-models/f175ca2168404919983af2b26fef4b9f | CC-BY | 1,224 faces, 2K texture. Gun-only animations: **walk, idle, fire, draw in, draw out, reload** | 3 | Timing reference |
| Fps Rig (Glock + arms) | J-Toastie (poly.pizza) | https://poly.pizza/m/uxko5LkGia | CC-BY 3.0 | Rigged and animated arms + pistol (clips not stated) | 3 | — |
| [Animated] Shotgun | CoinCoin (OpenGameArt) | https://opengameart.org/content/animated-shotgun | CC-BY 4.0 | Lever-action Winchester 1887, animated (clip list not stated) | 2 | — |
| Animated Guns Pack | Quaternius | https://quaternius.com/packs/animatedguns.html | CC0 (on Quaternius' site; poly.pizza shows one of its models as CC-BY 3.0, so download from Quaternius) | 6 guns (P90, **revolver**, pistol, **shotgun**, sniper). Untextured. Gun-only animations | 3 | Revolver and shotgun bases |
| G17 / M4 FPS Weapon Animations | BarcodeGames (Fab) | https://www.fab.com/listings/5b920af4-7a41-484e-afd2-323605fdaf77 | Licence field shows "-" | Draw, fire, reload, reload-empty, holster. Rated 2.0 | 1 | Avoid |

### 8.3 Guns (pistols, hand cannon, revolvers, flintlocks, blunderbuss, shotguns, rifles/muskets)

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| **TastyTony low-poly catalogue** | TastyTony (Sketchfab) | https://sketchfab.com/TastyTony — model URL pattern https://sketchfab.com/3d-models/<uid> | CC-BY | Hundreds of guns, **flat colours (multi-material, no textures)**. Relevant uids: **Desert Eagle** `b81da261345f4462b2c4412352162287` (4,299 faces) · **Pfeifer Zeliska** `1166a63d74504e2bb00706b2f14a87d2` (3,294; giant revolver = "hand cannon") · Magnum Research BFR `01ebdd81…` (2,410) · Colt SAA `84c4960b…` (3,356) · **Simeon North 1826 flintlock pistol** `ae5d67b37e2c4522a2294b0d45840d65` (2,777 faces, 11 materials) · Winchester 1897 `555cc635…` · Remington 870 `8766498c…` (1,444) | A/B | **5** | Merge materials into one palette texture (or bake to 512). Add `muzzle` and `grip` sockets. Split hammer, cylinder and frizzen as child bones. The short uids were truncated in research, so look them up on the profile |
| Flat Shaded Low Poly Flintlock Weapons | TheTeaGuns | https://theteaguns.itch.io/flat-shaded-low-poly-flintlock-weapons | CC-BY 4.0 (pay what you want) | **Flintlock pistol, pistol-axe, musket, musketoon.** One shared texture, **separate moving parts** | A | 5 | Period-correct set; recolour |
| Low poly pirate blunderbuss | Milaein (Sketchfab) | https://sketchfab.com/3d-models/075bb97c059046e1b3c8447b54651d1b | CC-BY | **964 faces**, flat colours, 1 material, 1 texture | A | **5** | Scale up the flared muzzle for comedy |
| Lowpoly Blunderbuss | Dawnstar-Chronicles (Sketchfab) | https://sketchfab.com/3d-models/a7446afc06d9422eb9202354f203fc00 | CC-BY | 880 faces, 1 material + baked normal map | A | 3 | Drop the normal map |
| Flintlock Gun – Low Poly Game | TheoKain (Sketchfab) | https://sketchfab.com/models/c3c3e678992c404b87bc5860df63674d | CC-BY 4.0 | 412 tris, hand-painted, **pivots set on trigger and hammer** | A | 4 | Repaint flat |
| Low Poly Flintlock | HenryMead (Sketchfab) | https://sketchfab.com/models/01a245e4974f4a6292b3d553cc46efea | CC-BY | 340 faces | A | 3 | — |
| Flintlock (first-person) | djonvincent (OpenGameArt) | https://opengameart.org/content/flintlock | CC0 | 1,235 tris first-person pistol with a **12-frame fire animation**, .blend | A | 3 | — |
| Musket | MainFable (Sketchfab) | https://sketchfab.com/3d-models/9a9c8e24bb40415d9da2b197d3025728 | CC-BY | 1,224 faces, 1 texture | A | 4 | — |
| Musket Low Poly / Musket | WhiteOak; OTG_Savage1714 (Sketchfab) | https://sketchfab.com/models/a496bebb0fea4ab99e6252ab81556a4c · https://sketchfab.com/models/c777cf6bf93f45888d398803b59d7f14 | CC-BY | 2.9k faces / 504 faces | A | 3 | — |
| Low-Poly Double-Barreled Shotgun | patinuns (Sketchfab) | https://sketchfab.com/3d-models/0180ef5f6b53487683ea0c6853e467f6 | Sketchfab "Free Standard" | 2,186 faces, **4 materials, no textures** | A | 4 | Split the barrels so they hinge for reload |
| Shotgun Double Barrel / Revolver | CreativeTrio (poly.pizza) | https://poly.pizza/m/k0fA37Awl8 · https://poly.pizza/m/wFQbxzzgqU | **CC0** | Single models, same set as the CC0 crossbow and hunter's knife | A | 4 | — |
| Rigged Desert Eagle | PuKkBuMXDD (poly.pizza) | https://poly.pizza/m/e9k4dwOzCX | CC-BY 3.0 | Rigged hand cannon | A | 4 | — |
| Double Barrel Shotgun (rigged) | J-Toastie (poly.pizza) | https://poly.pizza/m/Emvvx56omx | CC-BY 3.0 | Fully rigged | A | 4 | — |
| Toony Revolver | Michael Fuchs (poly.pizza) | https://poly.pizza/m/bgglx3Gdc4_ | CC-BY 3.0 | Cartoon Colt | A | 4 | — |
| Low Poly Western Weapon Pack | tt-3D (Fab) | https://www.fab.com/listings/c13dbec3-7b1c-43ab-8294-e7474a2cb355 | Fab Std (P0/Pro0) | **Revolver, shotgun, rifle**, knife, cigar. 30–700 tris each, **1 material, 256 palette** | A | 4 | Can be reshaped into a flintlock or blunderbuss |
| Weapon Pack 03 (Animated Revolver) | Horizon Store (Fab) | https://www.fab.com/listings/05c6741c-5233-4367-9c0d-67b49c8aa745 | Fab Std (P0/Pro0) | Historic revolver (25.6k tris), stock, **powder flask**, bullets. 46k tris total, 4K, 3–4 LODs, rigged, 3 animations, Epic-skeleton scale. UE 5.7 | C | 3 | Take the powder flask; decimate |
| Stylized guns | Zenthos Studio (Fab) | https://www.fab.com/listings/bcce3adc-d3f8-4f04-a014-92d267661788 | CC-BY | 5 stylised guns, **1,712 tris total** (228–538 each), one 4096 texture | A (downscale to 512) | 4 | — |
| Weapons FREE | ithappy (Fab) | https://www.fab.com/listings/9939041c-a505-47f5-a636-ee51381787ea | Fab Std (P0/Pro0) | 8: shotgun, sniper, rifle, revolver, **baseball bat, hockey stick, axe, knife** | A | 4 | — |
| Low poly weapons + Test map | Cosmoart (Fab) | https://www.fab.com/listings/1f5dd738-5316-4845-9cbe-6bdc4a6e1f5f | Fab Std (P0/Pro0). The itch copy is CC0 but $3 minimum for all formats | 29 guns, **one palette texture**, recolourable parts | A | 4 | — |
| Low Poly Weapons Set | MR POLY (Fab) | https://www.fab.com/listings/47827bc6-d20b-4567-b6e1-4650f5bc8057 | Fab Std (P0/Pro0) | Pistol, rifle, pump shotgun; separate triggers and mags; colour variants | A | 3 | — |
| CC0 Flat Guns East / West | Pichuliru | https://pichuliru.itch.io/cc0-flat-guns-east · https://pichuliru.itch.io/cc0-flat-guns-west | East: CC0. West: **conflicting** (title says CC0, licence field says CC-BY 4.0), so credit to be safe | 10 guns each, flat recolourable materials, **rigged with attachment bones**. Blend/FBX/GLB/OBJ | A | 3 | Modern designs; West has a pump shotgun and pistol |
| Low Poly Pirate Assets | iedalton (Sketchfab) | https://sketchfab.com/models/6cc11f23f16043efbc69417bd5b96f5a | CC-BY 4.0 | Cannon, musket, sword, dagger; 13.4k tris | B | 3 | — |
| Flintlock (Balkan) | IsoGL (Fab) | https://www.fab.com/listings/03495d17-c929-4c80-b9c6-5b122ba9b289 | Fab Std, **Personal free only** (Pro €3.50) | 27 MB OBJ | C | 2 | — |
| *AI-generated, avoid:* [FREE] 3D Pirate Weapons Pack | Polyy.AI | https://polyyai.itch.io/3d-pirate-weapons-pack | CC0 | 23 weapons incl. flintlock and musket, 2.3k–5.8k tris. **The page says it is AI-generated** | B | 1 | Steam AI disclosure |

### 8.4 Joke melee weapons (knife replacements and skins)

The skin list from `06_Art_Direction.md`: umbrella sword, hammer, pan, swordfish, baguette, oar/shovel, candlestick, clock-hand knife, scythe, rubber chicken, giant quill.

| Item | Author / Source | URL | License | Notes | Mobile | Fit |
|---|---|---|---|---|---|---|
| Closed Umbrella | CreativeTrio (poly.pizza) | https://poly.pizza/m/o0CUgpt8pm | CC0 | 436 polys | A | 5 |
| Candlestick | CreativeTrio (poly.pizza) | https://poly.pizza/m/tknOVwxT8B | CC0 | Puritan theme | A | 5 |
| Frying Pan | Pichuliru (poly.pizza) | https://poly.pizza/m/u1SOUzLBQc | CC0 | Also a CreativeTrio pan at https://poly.pizza/m/JeVqTzcwUT (licence not checked) | A | 5 |
| Rolling Pin / Cleaver | MilkAndBanana (poly.pizza) | https://poly.pizza/m/nbI0WJqJN3 · https://poly.pizza/m/t69bDPwFuZ | CC0 | Same author has a Laddle (https://poly.pizza/m/R80obLY1z6, licence not checked) and many kitchen tools | A | 5 |
| Baguette / Cool Baguette | Isa Lousberg; Polygonal Mind (poly.pizza) | https://poly.pizza/m/HPvkMpqqTg · https://poly.pizza/m/5oxca5I4QV | CC0 | Cool Baguette 814 tris | A | 5 |
| Swordfish | Quaternius (poly.pizza) | https://poly.pizza/m/7hMOlBjln0 | CC0 | Animated fish; strip the rig. **The fish sword** | A | 5 |
| Hunters Knife / Crossbow / Pitchfork | CreativeTrio (poly.pizza) | https://poly.pizza/m/a2avVUVeYD · https://poly.pizza/m/kHb0kA11oD · https://poly.pizza/m/dtLT2Nj0KF | CC0 | Default knife and farm tools | A | 4 |
| Scythe | Quaternius (poly.pizza) | https://poly.pizza/m/yE9TKCewO8 | CC0 (Quaternius on poly.pizza) *(licence on the model page not checked)* | Scythe skin | A | 4 |
| Broom Set | MiniPoly (poly.pizza) | https://poly.pizza/m/JQfsnKeAeW | CC-BY 3.0 | Witch-broom skin | A | 4 |
| Fish Bonez | LevyVanMalder (Sketchfab) | https://sketchfab.com/3d-models/97ea767b8f6747fc9b6807709dfa04f7 | CC-BY | Fish-bone weapon, 2,725 faces | A | 4 |
| Frello's Anchor | Bram Verheyen (Sketchfab) | https://sketchfab.com/3d-models/d3c352bb36ee4e01ba242c778ec1b02c | CC-BY | 4,961 faces | B | 4 |
| KayKit RPG Tools Bits | Kay Lousberg | https://kaylousberg.itch.io/rpg-tools-bits | CC0 | 45+ free: **hammer, mallet, pickaxe, axe**, wrenches, **lantern**, scissors, anvil. Fishing rods, keys and lockpicks are in the paid Extra tier | A | 5 |
| Toony Kitchen | Sigun Studio | https://sigunstudio.itch.io/toony-kitchen-models | CC0 (don't redistribute the pack) | 250+ models, 30+ kitchenware, one shared texture | A | 4 |
| Low Poly Food Pack | Felipe Greboge | https://greboge.itch.io/low-poly-food-pack | CC0 | 47 kitchen utensils, shared pixpal material, GLB *(individual items partial)* | A | 4 |
| CC0 Flat Blunt Melee / Bladed Melee | Pichuliru | https://pichuliru.itch.io/cc0-flat-blunt-melee · https://pichuliru.itch.io/cc0-flat-bladed-melee | CC0 | 10 each (wrench and golf club seen; blades) | A | 3–4 |
| Cartoon Free Food Pack | Mnostva Art (Fab) | https://www.fab.com/listings/6d96a809-eb62-4779-a161-d8859c3f1eeb | Fab Std (P0/Pro0) | 75 foods incl. **bread, sardine, salmon, meat on the bone**. 91.6k tris total, 1 material, palette | A | 4 |
| Free Fantasy Weapon Sample Pack | AFox1 / Prop Garden (Fab) | https://www.fab.com/listings/d5be0dc9-1a41-4be2-a63a-5ed436f3445d | Fab Std (Personal free, Pro €4.38) | 12 weapons incl. **hammer, sickle, mace**. 500–1,200 verts, static + skeletal versions, 21 materials, 2–4K | B | 4 |
| Low poly tools | sjolle | https://sjolle.itch.io/low-poly-tools | CC0 | ~50 carpenter tools, FBX | A | 3 |
| Low-Poly Tools & Weapons | Mike Klubnika | https://mikeklubnika.itch.io/low-poly-toolsweapons | "Completely free", credit requested | 33 items, correct pivots | A | 3 |

**Rejected:**

| Pack | Reason |
|---|---|
| AikdeIno brooms | Terms are unclear for commercial use |
| Kelni Stylized Toon Weapons | No licence stated |
| Inkxo arms | CC-BY-**NC** |
| Tesseract freebies | Personal tier only |

**Gaps: model these yourself.** Rubber chicken, giant quill, clock-hand knife, oar (use the Muyaya rowboat oar), shovel (not confirmed free).

### 8.5 First-person rendering notes and a pipeline

**Docs**

- **First Person Rendering:** https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-rendering
  - Added as **Experimental in UE 5.5**; self-shadow added in 5.6.
  - **Component setting:** `First Person Primitive Type`.
  - **Camera settings:** `Enable First Person Field Of View` / `First Person Field of View`, and `Enable First Person Scale` / `First Person Scale`. The scale setting shrinks the viewmodel toward the camera so it can't clip into walls.
  - **Material nodes:** First Person Output, Transform Position, Is First Person.
  - **Limits:** Epic's docs say it does **not work with the mobile renderer or forward rendering**, and it needs "Allow Static Lighting" off.
- **First Person template (5.6+):** https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-template-in-unreal-engine

**Recommended pipeline**

1. **One arms rig on UE5 Mannequin bone names.** Either:
   - bind Drillimpact's CC0 arms (or the puppet arms) onto Epic bone names in Blender, or
   - start from conceptyphoon's Mannequin arms.

   Either way, Epic and Fab first-person animations (Knife, Concrete Block, template pistol and rifle) retarget easily.
2. **Weapon rig standard.** Root → `grip` (pivot at the palm) → child bones for moving parts (hammer, frizzen, cylinder, break-action barrels) → `muzzle` socket. Attach to `hand_r`.
3. **Melee archetypes.** Author 4 animation sets in Blender and share them across all skins:
   - small blade (knife, cleaver, quill)
   - one-hand blunt (pan, hammer, rolling pin, candlestick, baguette, fish)
   - long two-hand (oar, broom, pitchfork, scythe, umbrella)
   - pistol

   Each set has draw, idle, inspect, attack A, attack B and stab. Use CoinCoin's tomahawk and Drillimpact's knife clips as timing references.
4. **Clipping**
   - **PC:** native First Person Rendering (separate viewmodel FOV around 70, First Person Scale around 0.1–0.2).
   - **Mobile:** separate low-poly viewmodel. Either scale it down close to the camera with a small near-clip plane, or trace forward and pull the weapon back or down near walls. This is general practice, not taken from a verified page.

---

## 9. Props (general, breakables, punishment, food, tools, loot)

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Fantasy Props MegaKit | Quaternius | https://quaternius.itch.io/fantasy-props-megakit · https://quaternius.com/packs/fantasypropsmegakit.html | CC0 (free 94; Pro $9.99 = 200+ with worn variants and collisions; Source $14.99) | 211 models: weapons, tools, vegetables, potions, **market stalls, chests**, furniture, books, **breakables**. 4 shared texture sets. FBX/OBJ/glTF (Blend in Source) | A/B | 5 | Style anchor. Remap to palette; pre-fracture crates, barrels and pots |
| KayKit Resource Bits | Kay Lousberg | https://kaylousberg.itch.io/resource-bits | CC0 (free 75+; Extra $4.99 adds coins, gems, crates, barrels, chests, food) | Wood, stone, ingots, textiles, fuel. Gradient atlas | A | 4 | Firewood, sacks |
| KayKit RPG Tools Bits | Kay Lousberg | https://kaylousberg.itch.io/rpg-tools-bits | CC0 (free 45+; Extra $4.99 adds **lockpicks, padlocks, keys**, fishing gear) | Pickaxe, hammer, axe, anvil, lantern, scissors | A | 5 | Keys and lockpicks are **paid**; model free ones yourself (trivial) |
| KayKit Board Game Bits | Kay Lousberg | https://kaylousberg.itch.io/board-game-bits | CC0 (free 75+) | Tokens, dice, meeples, coins, portraits | A | 3 | Chess tables in the square; role tokens |
| KayKit Fantasy Weapons Bits | Kay Lousberg | https://kaylousberg.itch.io/fantasy-weapons-bits | CC0 (free 25+) | Swords, axes, hammers, bows, staves, shields, spears. **No firearms** | A | 3 | — |
| Medieval Torture Devices | iPoly3D (poly.pizza) | https://poly.pizza/bundle/Medieval-Torture-Devices-ujQ7HGMreQ | CC0 | 14: **2 gallows, 2 pillories**, guillotine, cages, stool | A | 5 | The gallows stage in the square |
| Free CC0 Medieval Props | 3dmodelscc0 | https://3dmodelscc0.itch.io/free-cc0-medieval-props | CC0 | 10: **gallows**, guillotine, **market stall**, goblet, mug, grain sack. Texture approach not stated | ? | 3 | Check the style |
| Old gallows | adam127 (Sketchfab) | https://sketchfab.com/3d-models/old-gallows-e9d235ecfc974e44b1caa97b0980e157 | CC-BY 4.0 | 624 tris, 2K texture | A | 4 | — |
| Pillory | yaroslav.ostapenko (Sketchfab) | https://sketchfab.com/3d-models/pillory-876ae395b0834e29acb242054e07ccf3 | CC-BY 4.0 | 480 tris, 1K, stylised | A | 4 | Stocks for public shaming |
| Gallows / Gallows B | bumstrum (Sketchfab) | https://sketchfab.com/models/b496d60155cb4b2fb5864c3e2c68a0e2 · https://sketchfab.com/models/7e3fb43ce01d4b8d89967be8375c2d04 | CC-BY 4.0 | 3.5k / 3.2k tris | A | 3 | — |
| Another 3D Low Poly Pillory | yukitsu-senpai (Sketchfab) | https://sketchfab.com/models/1d7bc8ae26da490e93f55be7419991d6 | CC-BY 4.0 | 1.3k tris | A | 3 | — |
| Low Poly Medieval Environment Pack (35+) | anastasita.3d (Sketchfab, also on Fab) | https://sketchfab.com/3d-models/low-poly-medieval-environment-pack-35-props-a850530905a24d97bc4aa83353aba134 | CC-BY 4.0 | Bright, colourful fences, campfire, barrels, tools, trees. 24k tris total | A | 4 | Colour fits already |
| Medieval Props Minipack 4 (market stall, fence, cart) | Typetree / dyohandri (Sketchfab) | https://sketchfab.com/3d-models/44658609c33d46c18bf3e5eb5221f262 | CC-BY | 100–200 tris each, shared 512² atlas | A | 4 | — |
| Tiny Treats Baked Goods | Isa Lousberg | https://tinytreats.itch.io/baked-goods | CC0 | 24+ pies, cakes, bread | A | 4 | Loot "bread" |
| Ultimate Food (2019) / Ultimate Crops (2020) | Quaternius | https://quaternius.com/packs/ultimatefood.html · https://quaternius.com/packs/ultimatecrops.html | CC0 | 103 / 102 models | A | 3–4 | — |
| Farm Animal / Ultimate Animated Animals | Quaternius | https://quaternius.com/packs/farmanimal.html · https://quaternius.com/packs/ultimateanimatedanimals.html | CC0 | 7 / 12 animals, 12+ animations each | A | 4 | Chicken coop, barn, dog |
| Ultimate RPG Pack / RPG Essentials | Quaternius | https://quaternius.com/packs/ultimaterpg.html · https://quaternius.com/packs/rpg.html | CC0 | 106 / 13 models | A | 3 | — |
| Modular Weapons Pack (swords, daggers, bows) | Quaternius | https://quaternius.com/packs/medievalweapons.html | CC0 | 24 models | A | 3 | — |
| LowPoly Medieval Weapons | Quaternius | https://quaternius.itch.io/lowpoly-medieval-weapons | CC0 | Medieval weapons | A | 3 | — |
| Low Poly Medieval Weapons | LowPolyAssets | https://lowpolyassets.itch.io/low-poly-medieval-weapons | CC0 | 60+ flat-colour meshes incl. farm tools (no crossbow or dagger listed) | A | 4 | — |
| FREE Low Poly Weapon Pack | Kickin' It Studios | https://kickin-it-studios.itch.io/low-poly-weapon-pack | Custom: "may NOT be re-sold, but other use is OK" | 37 weapons incl. daggers and a pitchfork. Vertex-colour and textured versions | A | 4 | — |
| 3d weapons Asset pack | styloo | https://styloo.itch.io/3d-weapons-asset-pack | CC0 | Bayonet, knives, swords | A | 3 | — |
| Free Low Poly Mining Assets | Pure Poly (Fab) | https://www.fab.com/listings/e034b038-207c-43f3-9815-0470e9f04a66 | Fab Std | 22 assets on **one 256 px texture**: lantern, cart, pickaxe, ores | A | 4 | Mine |
| Bell | Quaternius (poly.pizza) | https://poly.pizza/m/cILuaO115Y | CC0 | Church and bell-tower bell | A | 4 | — |
| Cannon | Quaternius (poly.pizza) | https://poly.pizza/m/J15vlPVvKK | CC0 | From the Pirate Kit (Nov 2023) | A | 4 | Cannon battery |
| Fantasy bundle | CreativeTechLab (poly.pizza) | https://poly.pizza/bundle/Fantasy-bundle-8SikjZpoGt | CC-BY | 20: potions, books, candle, skull | A | 3 | Herbalist shop |
| Witch cottage pack | curated by 2x_Helix (poly.pizza) | https://poly.pizza/bundle/Witch-cottage-pack-Nv92uZNma4 | Mixed: mostly CC-BY, some CC0 (check each model) | 34 models by many authors (cauldron, spellbook, potions); styles vary | A | 3 | — |
| Medieval Props textured | Clint Bellanger / Lamoot (OpenGameArt) | https://opengameart.org/content/medieval-props-textured | CC-BY 3.0 (credit Lamoot) | Well, tombstones, fence | B | 2 | — |

**Breakables (no free pre-fractured pack found):**

- **PC:** fracture crates, barrels, pots, windows and chairs in UE **Fracture Mode** (Geometry Collections, `GC_KG_*`).
- **Mobile:** swap in pre-cut static pieces made in Blender. Keep each piece on the palette UVs, and give the inside faces a "raw wood" palette swatch.

---

## 10. Underground and spooky

### 10.1 Cellars, trapdoors, secret rooms

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Trapdoor / Trap Door (2 models) | Quaternius (poly.pizza) | https://poly.pizza/m/Ph6MpmwySS · https://poly.pizza/m/PALqVBff9b | CC0 | Wooden hatches, FBX/glTF | A | 5 | Separate lid and frame; pivot on the hinge |
| Medieval Dungeon Door Pack | Azrael68 | https://azrael68.itch.io/freepaid-medieval-door-pack-low-poly | CC0 | 10 doors, pivots set for rotation, mobile-friendly | A | 4 | Cellar doors; lay one flat for a trapdoor |
| Medieval Bookshelf & Library Pack | Azrael68 | https://azrael68.itch.io/medieval-bookshelf-library-pack-low-poly | CC0 | 10 bookshelves, flat palette | A | 4 | Build the **secret bookcase door** (hill-house secret room) |
| Outdoor Cellar Door | s4shko_ (Sketchfab) | https://sketchfab.com/3d-models/1d8736ede47149c0be75c99e47266047 | CC-BY | **86 faces**, 1 material, 5 PBR maps | A | 3 | Drop the PBR maps; recolour as painted wood |
| Trapdoor (animated) | Notvolts (Sketchfab) | https://sketchfab.com/3d-models/c4b7941fc4874a4a8ce58864a585e5e2 | CC-BY | 1,448 faces, 1 material, 4 textures, open animation | A | 3 | — |
| Spectral Horror Furniture | Azrael68 | https://azrael68.itch.io/spectral-horror-furniture | CC0 (credit appreciated) | 4 pieces (tables, chair, stool), vertex colours, ghostly blue | A | 3 | Ghost-only rooms. Sibling pack "Haunted Spectral Beds" (https://azrael68.itch.io/haunted-spectral-beds) *(unverified)* |
| KayKit Dungeon Pack (Remastered) | Kay Lousberg | https://kaylousberg.itch.io/kaykit-dungeon-pack | CC0 (free 200; Extra $7.95 = 275+ incl. tavern bar, beds, 6 textures; Source $11.95) | Walls, floors, stairs, doors, chests, barrels, tables, crates, traps, banners. **Gradient atlas.** FBX/GLTF/OBJ | A | 5 | Wine cellar, crypt, jail cells in the town hall |

### 10.2 Catacombs and crypts (under the church)

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| Haunted Cemetery Low Poly: Game Asset Pack | ronildo.facanha (Sketchfab) | https://sketchfab.com/3d-models/da41bb9f2ffe42fb8a69438e92dd2936 | CC-BY | **1,558 faces total, 4 materials, 0 textures** (flat colour). Mausoleums, graves, bones, crows. The author says it is mobile-optimised | A | 4 | Push colours vibrant |
| Free Modular Low Poly Dungeon Pack | RGS_Dev | https://rgsdev.itch.io/free-modular-low-poly-dungeon-pack-by-rgsdev | CC0 ("credit is not needed") | Walls, doors, floors, stairs, traps, chest, props. Not UV-mapped; one material per colour. 6.7 MB | A/B | 3 | Convert material colours to palette UVs |
| Modular Dungeons Pack (2019) / Modular Dungeon Pack (2018) | Quaternius | https://quaternius.com/packs/modulardungeon.html · https://quaternius.com/packs/medievaldungeon.html | CC0 | 48 / 41 models (walls, barrels, torches, potions); flat colours | A | 3 | — |
| Ultimate Modular Ruins | Quaternius | https://quaternius.com/packs/ultimatemodularruins.html | CC0 | 90 textured ruin and dungeon pieces | A/B | 3 | Abandoned chapel |
| Ultimate Low Poly Dungeon | Broken Vector | https://brokenvector.itch.io/ultimate-low-poly-dungeon | CC-BY 4.0 (code MIT) | 185 assets with collision meshes, FBX/.blend | A | 4 | — |
| Stylized Dungeon Props Pack | J's 3D (Fab) | https://www.fab.com/listings/ddd47def-2332-4485-b399-952c4e69a569 | Fab Std (P0/Pro0) | 77 meshes (barrels, shelves, **candles**, banners, jars), 66–6,000 verts, **flat-colour materials, no textures** (33 + 32 instances). UE 5.6 | A/B | 4 | Merge materials |
| Stylized Dungeon Pack | CobraGamesAssets (Fab) | https://www.fab.com/listings/c6e91312-202a-4d80-a6f1-1b374bb27dce | Fab Std | 67 meshes, UE 5.3–5.5 | B | 3 | — |
| GameDev Polygon Kit – Dark Dungeon Free | AssetHunts! (Fab) | https://www.fab.com/listings/7023788e-b32b-475c-a7d6-af57200068d6 | Fab Std | 50+ models, FBX/GLB | A/B | 3 | — |
| Low-poly vampire crypt game assets | AnaMaTo | https://anamato.itch.io/low-poly-vampire-crypt-game-assets | **NOT STATED** | 24 FBX: walls, floor, doors/windows, **coffin with separate lid**, skull, bones, web, torches, spiral pillars, pedestals, fire pit | A | 4 (if licensed) | The opening coffin suits body-reveal moments |
| Beneath the Barrow | JessSwynn (Sketchfab) | https://sketchfab.com/3d-models/f041fc9f0ad047af810c36d5cadb468a | CC-BY | Isometric crypt diorama: 8,803 faces, 8 materials, 4 textures. Webs and fire are real meshes | B | 3 | Pull out the coffin and webs |
| Low Poly Pixel Art Spooky Graveyard | Obviously Dracula | https://obviouslydracula.itch.io/low-poly-pixel-art-spooky-graveyard-models | CC0 | 22: 3 mausoleums, 8 gravestones, gate, fences, lamp. Pixel textures | A | 2 | Use the mausoleum shells as crypt entrances; retexture |
| Graveyard and Crypt | xmorg (OpenGameArt) | https://opengameart.org/content/graveyard-and-crypt | CC-BY 3.0 | Mausoleum with detachable underground crypt, caskets, torch, rigged skeleton (2010) | B | 2 | Layout reference |
| PSX Dungeon – Modular Asset Pack | Goblinatron | https://goblinatron.itch.io/psx-dungeon | CC BY 4.0 | 40+ very low-poly models, 64×64 textures | A | 2 | Blockout; wrong style |
| 3TD Fantasy Ruins | Ron Kapaun (OpenGameArt) | https://opengameart.org/content/3td-fantasy-ruins-pack | CC0 | Ruined cathedral and walls, DAE | B | 2 | — |

### 10.3 Smuggler tunnels, mine, sewers

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| **Mines and Cave Modular Set** | loafbrr | https://loafbrr.itch.io/mines-and-cave-set | CC0 | 178 objects, **20,990 tris total**, 4 materials, 1K trimsheet. Blend/GLB/FBX. 4.8★ (11 ratings) | A | 5 | Swap the trimsheet for palette colours. Wooden-support tunnels double as smuggler tunnels |
| Mine Object Collection | loafbrr | https://loafbrr.itch.io/mine-object-collection | CC0 | Tracks, **carts**, pickaxes, jars, pipes, posts. 12,757 tris total, 1K textures | A | 4 | Physics mine carts on rails |
| free mine assets pack | rubberduck (OpenGameArt) | https://opengameart.org/content/free-mine-assets-pack | CC0 | 35+ wooden supports, stones, stalactites, rails set up for array/curve modifiers, lamps, cart. Realistic textures | B | 3 | Use the low-poly stones; retexture |
| Sewers | Elbolilloduro | https://elbolilloduro.itch.io/sewers | Models CC0, **but the textures come from texturelib.com / texturer.com** | Straight and curved tunnels, arches, pipes, valves, debris. PSX style | A | 3 | **Replace the textures** (this also avoids the third-party texture terms) |
| Mine Cave Models | BrettDow (Sketchfab) | https://sketchfab.com/3d-models/a350b908462248599aaaa1e3cd2d48ea | CC-BY | 7,788 faces, 6 materials, **0 textures** | A | 3 | Merge materials |
| Modular Low Poly Cave Tunnels and Rocks | Kynnelo Vyskenon | https://kynnelovyskenon.itch.io/modular-low-poly-cave-tunnels-and-rocks-asset-pack | **CC BY-SA 4.0** | Tunnel segments that snap at their ends, plus rocks | A | 3 | **Share-alike:** any mesh you edit must stay BY-SA. Avoid mixing into core art |
| Modular_Sewer_System | elopez7 (OpenGameArt) | https://opengameart.org/content/modularsewersystem | CC0, **but the brick texture is from textures.com** | Tunnel segments, corners, slopes (2015) | B | 2 | Retexture |

### 10.4 Sea caves and caves (tidal caves, behind the waterfall)

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| **LowPoly Pirate Props** | zisongbr (OpenGameArt) | https://opengameart.org/content/lowpoly-pirate-props | CC0 | **4 cave types**, 3 piers, 15 barrels, crates, **4 chests**, cannons, 4 torches, flags (separate material), planks. 558 KB | A | 5 | A ready-made smuggler grotto and black-market stall |
| LowPoly Cave Entrance | zisongbr (OpenGameArt) | https://opengameart.org/content/lowpoly-cave-entrance | CC0 | Stylised cave mouth, 719 KB | A | 4 | Cliff-side or waterfall cave entrance |
| Low poly cave assets | "agdg reject" (OpenGameArt) | https://opengameart.org/content/low-poly-cave-assets | CC0 | 8 OBJ (pillar, stalactites, stalagmites, flowstone, stone), one 17 KB texture | A | 4 | Fix scale |
| The Pirate and Kraken's Hideaway | konstantinvelikov (Sketchfab) | https://sketchfab.com/3d-models/8baf76154c3b4b8c9b3e3a4b81a0de06 | CC-BY | Stylised **sea-cave** diorama (wrecked sloop, hammock, treasure, crabs, kraken). 51,986 faces, 2 materials, 3 textures | B | 4 | Break into props; **art-direction target** for the sea caves |
| Seaside Treasure Cave | Felix_Cooper (Sketchfab) | https://sketchfab.com/3d-models/02932015c21446a29e5ed0538c50460d | CC-BY | Cave inside a sea rock with a shrine and idol. 67,997 faces, 6 materials | B/C | 3 | Decimate |
| Modular caves | 3dAssatsForGames (Sketchfab) | https://sketchfab.com/3d-models/66d33b65d4fc471db78a7e4da2232623 | CC-BY | 13 modules + example layout, 27,828 faces, 1 material, 2 textures | A/B | 3 | — |
| Low Poly Stone Cave | rasmon (Sketchfab) | https://sketchfab.com/3d-models/7de9bcea32ba431eb165287ad67f3d7f | CC-BY | 4,994 faces, 1 material, 6 textures | A | 3 | — |
| Cave Minerals | SimplePolygon (poly.pizza) | https://poly.pizza/m/l51xGk3wbv | CC-BY 3.0 | 5 minerals × 6 colour variants on one palette | A | 3 | Add emissive for glowing grotto accents |
| Modular Terrain Pack (caves, waterfalls) | Fertile Soil Productions | see §5.1 | CC0 | Includes caves | A | 3 | — |

No free modular **sea-grotto** kit exists. Kitbash one from zisongbr caves + loafbrr tunnels + SimplePolygon basalt, add a tide-plane water material, and use "Kraken's Hideaway" as the visual target.

### 10.5 Spooky, ghost and witch props

| Name | Author / Source | URL | License | Contents, polys, material approach | Mobile | Fit | Blender edit notes |
|---|---|---|---|---|---|---|---|
| **Halloween Props** | loafbrr | https://loafbrr.itch.io/halloween-props | CC0 | 128 props: coffins, tombstones, **ghost-sheet variants**, pumpkins, cobweb/blood/face decals. 26,330 tris total, 5 materials, 2K textures | A/B | 5 | Downscale textures to 1K for mobile |
| Spooky Prop Set | loafbrr | https://loafbrr.itch.io/spooky-prop-set | CC0 | 50: **cages**, coffins, ghosts, pumpkins. 26,408 tris, **23 materials** at 1K | B | 4 | Bake to one atlas |
| KayKit Halloween Bits | Kay Lousberg | https://kaylousberg.itch.io/halloween-bits | CC0 (free 60+; Extra adds 30+) | Gravestones, mausoleum, pumpkins, skulls, **dead trees, scarecrow**, cart. Gradient atlas | A | 5 | Graveyard, farm scarecrows |
| KayKit Spooktober (Legacy) | Kay Lousberg | https://kaylousberg.itch.io/kaykit-spooktober | CC0 | 2 characters, dead trees, fences, tombstones, tiles | A | 4 | — |
| Stylized Halloween Asset Pack | WillowBoxArt | https://willowboxart.itch.io/stylized-halloween-asset-pack · https://sketchfab.com/3d-models/f8b5eeeae0524156a78ce529bc3000b6 | Sketchfab copy: **CC BY 4.0**; itch copy states no licence (use the Sketchfab copy) | 27: **witch, witch cat, ghosts**, skeleton, cauldrons, brooms, candles, gravestones. **1 material, one 1254² atlas.** 135k faces total | B | 5 | Decimate the characters |
| Free Demo of Low Poly Fantasy Halloween Pack | moldydoldy (Sketchfab) | https://sketchfab.com/3d-models/a5714d1768184671a7793d793b846408 | CC-BY | 9,229 faces, 1 material, 1 texture: dead trees, coffin, crypt, headstones | A | 4 | — |
| FREE Necropoly Candle & Pumpkin | EmaceArt | https://emaceart.itch.io/necropoly-halloween-candles-pumpkins-free | Custom: personal and commercial, no credit, no resale | 3 melted candles, 2 pumpkin candles, 3 jack-o'-lanterns with emissive eyes | A | 4 | Glow comes from bloom, so no extra lights (fits the light budget) |
| FREE Haunted Grounds / Gravefield Cemetery Kit FREE | EmaceArt | https://emaceart.itch.io/freenecropoly · https://emaceart.itch.io/cemetry | Haunted Grounds CC0; Gravefield FREE custom (EmaceArt) | 74 models (candles, lanterns, signs) / 62 prefabs (muted colours) | A | 3 | — |
| Free Low-Poly Halloween Kit | AssetQuest | https://assetquest.itch.io/free-low-poly-halloween-kit | "Royalty Free" | 72 props: graves, coffins, pumpkins; single base-colour texture | A | 4 | — |
| Low Poly Potions | Azrael68 | https://azrael68.itch.io/low-poly-potions | CC0 | 4 potions, vertex colours, ready for emission | A | 4 | Herbalist, witch |
| Cobweb / Cauldron / Ghost Skull | Quaternius (poly.pizza) | https://poly.pizza/m/EHYNWew6JK · https://poly.pizza/m/QaWJOPa6Gt · https://poly.pizza/m/TX8r9WBXpe | CC0 | Single props (the ghost skull is animated) | A | 4 | — |
| Candelabra | bitgem (Sketchfab) | https://sketchfab.com/3d-models/89f71a03a338491c92ada680e6f51749 | CC-BY | 447 faces, 1 material, hand-painted | A | 4 | Repaint flat |
| Low Poly Tombstones — Free Sample | POLYGROVE (Fab) | https://www.fab.com/listings/33bc0ba2-a61d-4d4f-ac2f-8e8144f100b1 | CC BY 4.0 | 3 stones, 1.9k tris, 1 texture | A | 4 | — |
| Free Pack – Cobwebs | PolyOne (Fab) | https://www.fab.com/listings/c09fb18e-eb8f-4a65-b0b6-c8a664d6b08d | Fab Std (Personal free, Pro €0.86) | 16 cobwebs, 20–100 tris each, one 4K texture | A (at 1K) | 4 | — |
| Low-Poly Ghost Pack | MyUsernameIsGood (Sketchfab) | https://sketchfab.com/3d-models/a12ad7afa5b247018c0daeec3e0152bc | CC-BY | 22,240 tris, 15 flat-colour materials | B | 3 | Merge materials |
| Low-Poly Prison Cage | BuntyS (Sketchfab) | https://sketchfab.com/3d-models/133ab66f21de45f0848b2602e014a00f | CC-BY | 2,290 faces, cage with a door | A | 3 | Town-hall cells |
| Crows | basileios; TheNorthernHarpy (Sketchfab) | https://sketchfab.com/3d-models/c71c8388507f439383f52df2f360efbb · https://sketchfab.com/3d-models/05fd8f3f3cb64aebab692f3122d2256d | CC-BY | 1,631 faces with 1 animation; or 96 faces with **10 animations** (voxel/Blockbench style) | A | 3 | Crows on the Dry Tree |
| Free Animated Cartoon Skeleton | Overaction Game Studio | https://overactiongames.itch.io/free-animated-cartoon-low-poly-skeleton-enemy | CC0 | Idle, run, attack, spawn, die | A | 3 | — |
| 3D dungeon debris – skull and bones | Paul Wortmann (OpenGameArt) | https://opengameart.org/content/3d-dungeon-debris-skull-and-bones | CC0 | Skull, ribcage, spine, pelvis, limbs; OBJ | A | 3 | Ossuary walls |
| Dark Cemetery Props — Stylized Gothic | SimpliGen (Fab) | https://www.fab.com/listings/8c39b9a8-4b00-4bef-a361-7ab336058499 | Fab Std (Personal free, Pro €5.26) | 13 meshes, LODs 0–3, 2 master materials, 57 textures at 2K, candle fire. UE 5.7. **Fab flag: "Generated with AI: yes"** | B | 3 | Steam AI disclosure |
| Witch House Assets | Gleao | https://gleao.itch.io/witch-house-assets | **NOT STATED** | 29 meshes (cauldron, broom, jars, skull candle, books), only 6,120 verts, but **38 materials and 260 textures** | C | 3 | Confirm licence; re-material |

**Rejected:**

| Pack | Reason |
|---|---|
| DARK_PALETTE Low Poly Halloween | CC BY-**ND** |
| SDAssets Bone Dungeon | Licensed for Smile Game Builder only |
| G4G Crypt Zombie | AI-assisted, custom licence |
| Woshi Gang dungeon | AI-generated, no licence stated |
| InsomniaArt Dungeon/Torture Chamber | No licence stated, no textures |
| Quaternius Bestiary | QAL, not CC0; fine to use, but note the licence |


---

## 11. UI: fonts, icons, parchment

### 11.1 Fonts

**Licence and how subsets were checked**

- All fonts below are **SIL OFL 1.1**. Script support was read from `raw.githubusercontent.com/google/fonts/main/ofl/<name>/METADATA.pb`.
- Specimen URL pattern: `https://fonts.google.com/specimen/<Name+With+Plus>`, e.g. https://fonts.google.com/specimen/Pirata+One

**OFL rules** (from the OFL text and FAQ, https://openfontlicense.org/ofl-faq/)

- You may bundle a font with a commercial game. You may not sell the font on its own.
- Ship each font's copyright notice and `OFL.txt`.
- A credits-screen mention is good practice.
- **A subset is a modified font.** A subset of a font with a Reserved Font Name must not keep that name. This applies to Pirata, New Rocker, the Unifraktur fonts and IM FELL.

**Russian and other Cyrillic text:** most period display fonts have **no Cyrillic**. Use a UE **Composite Font**: the Latin display face as the default sub-font, plus a Cyrillic-range sub-font.

| Font | Role | Cyrillic | Fit | Notes |
|---|---|---|---|---|
| IM Fell English / IM Fell English SC / IM Fell DW Pica | Title, headings | **No** | 5 | Real 17th-century printing type. The strongest colonial feel |
| Pirata One | Title, banners | **No** | 5 | Readable blackletter |
| Grenze Gotisch | Headings | **No** | 5 | Variable weight 100–900 |
| New Rocker | Headings | **No** | 4 | — |
| Cinzel | Menu headers | **No** | 4 | Roman capitals |
| Almendra / Almendra Display | Headings | **No** | 3–4 | — |
| UnifrakturMaguntia / UnifrakturCook | Decorative | **No** | 3 | Hard to read at small sizes |
| MedievalSharp, Metamorphous, Uncial Antiqua, Eagle Lake, Macondo, Cinzel Decorative | Decorative | **No** | 3 | — |
| Creepster | "You died" style banners | **No** | 3 | Cartoon-spooky |
| Nosifer | Decorative | **No** | 2 | — |
| Jacquarda Bastarda 9 | — | **No** | 2 | Pixel blackletter, wrong style |
| **Alegreya / Alegreya SC** | Body text + Cyrillic small-caps headings | **Yes** | 5 | Best single family for both languages |
| **EB Garamond** | Body (period feel) | **Yes** | 5 | — |
| **Lora** | Body | **Yes** | 5 | — |
| **Nunito** | UI labels, numbers, mobile | **Yes** | 5 | — |
| Spectral | Body | **Yes** | 4 | — |
| Cormorant / Cormorant Unicase | Headings | **Yes** | 4 | Too thin for small mobile text |
| Kurale | Cyrillic display fallback | **Yes** | 4 | — |
| Ruslan Display | Cyrillic title fallback | **Yes** | 4 | Old-Russian look |
| Old Standard TT | Cyrillic heading fallback | **Yes** | 4 | — |
| Playfair Display | Headings | **Yes** | 4 | — |
| Rubik Wet Paint | Spooky banners | **Yes** | 3 | Dripping; a Cyrillic alternative to Creepster |
| Oranienbaum, Philosopher, Yeseva One, Underdog, Amatic SC | Accents | **Yes** | 3 | — |
| Fruktur | Blackletter | Only `cyrillic-ext` listed | 3 | Basic Russian letters **not confirmed**; test in the preview |
| Crimson Pro, Libre Baskerville, Atkinson Hyperlegible (Next) | Body | **No** | 3 | — |

**Suggested set**

| Role | Latin | Cyrillic fallback |
|---|---|---|
| Title | IM Fell English SC or Pirata One | Ruslan Display or Old Standard TT |
| Headings | Alegreya SC | (same font, already has Cyrillic) |
| Body | Alegreya or EB Garamond | (same font) |
| UI labels, numbers | Nunito | (same font) |
| Spooky banners | Creepster | Rubik Wet Paint |

### 11.2 Icons

| Name | Author / Source | URL | License | Contents | Fit | Notes |
|---|---|---|---|---|---|---|
| game-icons.net | Lorc, Delapouite, Skoll, Caro Asercion, Viscious Speed and others | https://game-icons.net/ (terms: https://game-icons.net/about.html) | **CC BY 3.0, credit required** | 4,180 SVG icons. Confirmed relevant: `lorc/gibbet` (gallows and noose), `lorc/stiletto` (knife), `lorc/pointy-hat` (witch hat), `lorc/witch-flight`, `lorc/candle-light`, `lorc/cauldron`, `lorc/skull-crossed-bones`, `lorc/magnifying-glass`, `lorc/quill-ink`, `lorc/poison-bottle`, `lorc/burning-embers` (pyre), `delapouite/pitchfork`, `delapouite/church`; `/tags/death.html` has 48 more (coffin, tombstone, ghost, executioner hood, guillotine…) | 5 | Role and ability icons. Recolour the SVGs to the palette. `lorc/noose`, `lorc/pitchfork` and `lorc/death-skull` return 404; the noose is "gibbet" |
| Board Game Icons v1.1 | Kenney | https://kenney.nl/assets/board-game-icons | CC0 | 250 icons (dice, cards, tokens) | 4 | Votes, trial UI |
| Emotes Pack | Kenney | https://kenney.nl/assets/emotes-pack | CC0 | 480 files | 4 | Social reactions over heads |
| Game Icons (input prompts) | Kenney | https://kenney.nl/assets/game-icons | CC0 | 105 **gamepad and input-prompt** icons (not fantasy) | 3 | Control prompts |
| Shikashi's Fantasy Icons Pack | shikashipx | https://shikashipx.itch.io/shikashis-fantasy-icons-pack | CC BY 4.0 (credit Matt Firth + game-icons.net) | 284 pixel icons, 32×32 | 1 | Pixel style doesn't fit |

### 11.3 Parchment and paper UI

| Name | Author / Source | URL | License | Contents | Fit |
|---|---|---|---|---|---|
| UI Pack – Adventure | Kenney | https://kenney.nl/assets/ui-pack-adventure | CC0 | 130 ornate, parchment-style panels and buttons | 5 |
| UI Pack (RPG Expansion) | Kenney | https://kenney.nl/assets/ui-pack-rpg-expansion | CC0 | 85 files (beige and brown panels, bars, buttons) | 4 |
| Fantasy UI Borders | Kenney | https://kenney.nl/assets/fantasy-ui-borders | CC0 | 140 panel borders and frames | 4 |
| UI Pack 2.0 | Kenney | https://kenney.nl/assets/ui-pack | CC0 | 430 files, modern vector | 3 |
| Parchment | OpenGameArt | https://opengameart.org/content/parchment | CC0 | One parchment PNG + GIMP file (low resolution) | 2 |
| Paper 001 | ambientCG | https://ambientcg.com/view?id=Paper001 (licence: https://docs.ambientcg.com/license/) | CC0 | PBR paper 1–4K; tint for parchment | 3 |

Tip: the vote ballots, wills ("vasiyet") and Night Journal pages can reuse one parchment 9-slice with the IM Fell and Alegreya fonts.

---

## 12. Audio

### 12.1 Sound effects

| Name | Author / Source | URL | License | Contents | Fit | Notes |
|---|---|---|---|---|---|---|
| RPG Audio | Kenney | https://kenney.nl/assets/rpg-audio | CC0 | 50: foley, **footsteps**, weapons, knife, creaks, book, coins | 5 | — |
| Impact Sounds | Kenney | https://kenney.nl/assets/impact-sounds | CC0 | 130 impacts (wood, metal, glass…) | 4 | Breakables |
| Interface Sounds / UI Audio | Kenney | https://kenney.nl/assets/interface-sounds · https://kenney.nl/assets/ui-audio | CC0 | 100 / 50 UI clicks | 4 | — |
| Music Jingles | Kenney | https://kenney.nl/assets/music-jingles | CC0 | 85 jingles | 3 | Role reveal, win and lose stings |
| Voiceover Pack / Voiceover Pack (Fighter) | Kenney | https://kenney.nl/assets/voiceover-pack · https://kenney.nl/assets/voiceover-pack-fighter | CC0 | 90 / 45 voice clips | 2 | — |
| #GameAudioGDC bundles | Sonniss | https://sonniss.com/gameaudiogdc (archive 2015–2024) · https://gdc.sonniss.com/ (current: 2026 bundle, 7.47 GB, 347+ files) · licence: https://sonniss.com/gdc-bundle-license/ | Royalty-free, commercial OK, **no credit needed**. No redistributing the sounds as sound effects; **no AI/ML training** | Tens of GB of professional SFX (bells, doors, ambiences, water, wood) | 5 | The 2025 bundle was not listed |
| 100 CC0 SFX / 100 CC0 SFX #2 / 100 CC0 metal and wood SFX | rubberduck (OpenGameArt) | https://opengameart.org/content/100-cc0-sfx · https://opengameart.org/content/100-cc0-sfx-2 · https://opengameart.org/content/100-cc0-metal-and-wood-sfx | CC0 | **Bells, door squeaks and opens/closes**, paper, keys, wood, footsteps, thunder, locks, hammers | 4 | — |
| CC0 Sounds Library (collection) | ETTiNGRiNDER (OpenGameArt) | https://opengameart.org/content/cc0-sounds-library | CC0 collection | Ghost sounds, screams, heartbeat, clock ticking, footsteps | 4 | — |
| CC0 Sound Effects (collection) | OwlishMedia (OpenGameArt) | https://opengameart.org/content/cc0-sound-effects | Intended CC0; check each item | Footsteps, door sets, bells | 4 | — |
| Footstep Sounds (collection) | OwlishMedia (OpenGameArt) | https://opengameart.org/content/footstep-sounds | **Mixed licences** | Footsteps on many surfaces | 3 | Check each file |
| Horror Sound Effects Library | Little Robot Sound Factory (OpenGameArt) | https://opengameart.org/content/horror-sound-effects-library | **CC BY 3.0** | 69 sounds incl. **3 knife stabs**, evil laughs, screams, gate opening | 5 | Credit required |
| Church Bell | Ulrich Metzner, edited by qubodup (OpenGameArt) | https://opengameart.org/content/church-bell | **CC BY-SA 3.0** | One church bell | 3 | Share-alike; a CC0 bell from Freesound is safer |
| Freesound | freesound.org | https://freesound.org/help/faq/ — CC0-only search example: `https://freesound.org/search/?q=church+bell&f=license:%22Creative+Commons+0%22` (779 results) | **Per sound:** CC0 fine; CC BY needs credit; **CC BY-NC not allowed commercially** | Huge library | 5 | Filter to CC0 |
| Pixabay sound effects | Pixabay | https://pixabay.com/service/license-summary/ | Pixabay Content License: commercial OK, no credit; no redistributing files on their own | e.g. 2,614 church-bell results | 4 | — |
| Soundly Free | Soundly | https://getsoundly.com/faq/can-i-use-the-sounds-in-a-video-game/ | Commercial and game use OK; no reselling the sounds | 3,000+ sounds | 4 | Use the Freesound add-on's "CC0-only" setting |
| ZapSplat | ZapSplat | https://www.zapsplat.com/license-type/standard-license/ | Standard License: **free members must credit ZapSplat**; paid tiers don't *(partial: page returned 403)* | — | 3 | — |
| **Do not use:** BBC Sound Effects | BBC | sound-effects.bbcrewind.co.uk | RemArc licence: personal, educational or research use only *(partial)* | — | 0 | Not for commercial games |

### 12.2 Music (spooky folk, harpsichord, tavern, organ)

| Name | Author / Source | URL | License | Contents | Fit | Notes |
|---|---|---|---|---|---|---|
| Medieval tracks (Bard's Tale, Minstrel Dance, Market Day, The Old Tower Inn, Harvest Season) | RandomMind (OpenGameArt) | https://opengameart.org/users/randommind (e.g. https://opengameart.org/content/medieval-the-old-tower-inn) | **CC0** | Lute, flute, viola folk; each has a full track and a loop | 5 | Tavern, market |
| Dowland 1597, "If my complaints could passions move" | Of Far Different Nature (OpenGameArt) | https://opengameart.org/content/historic-renaissance-music-from-1597-if-my-complaints-could-passions-move-by-john-dowland | **CC0** | Lute and harpsichord, the right period | 5 | Menu or meeting phase |
| CC0 Fantasy Music & Sounds (collection) | Sir Gawain (OpenGameArt) | https://opengameart.org/content/cc0-fantasy-music-sounds | CC0 | Town theme, "Forgotten tomb" ambience, more | 4 | — |
| Good CC0 Music (collection) | AnyRPG (OpenGameArt) | https://opengameart.org/content/good-cc0-music | CC0 | 43 tracks incl. Spooky Loop, Horror Loop, Haunting piano | 4 | Night phase |
| CC0 - Dark Music | josepharaoh99 (OpenGameArt) | https://opengameart.org/content/cc0-dark-music | CC0 | 3 tracks | 3 | — |
| Kevin MacLeod | incompetech | https://incompetech.com/music/royalty-free/faq.html | **CC BY 4.0, credit required** (a credits screen is fine) | Confirmed on incompetech: Achaidh Cheide, Folk Round, Minstrel Guild, Master of the Feast, Lord of the Land, Moonlight Hall, Midnight Tale, Myst on the Moor, **Danse Macabre**, Ghost Story, Oppressive Gloom, Gathering Darkness, Darkest Child, Dark Walk, **Investigations**, Crossing the Chasm. Confirmed via Free Music Archive: Thatched Villagers, Pippin the Hunchback, Rites | 5 | Village Consort, Suonatore di Liuto and Sneaky Snitch are *unverified on incompetech* |
| Collection – Celtic & Medieval | Alexander Nakarada | https://alexandernakarada.bandcamp.com/album/collection-celtic-medieval | **CC BY 4.0** (commercial OK) | 147 tracks incl. **Tavern Loop One, Village Ambiance**, Now We Feast, Bonfire | 5 | serpentsoundstudios.com no longer hosts his music; use Bandcamp |
| Free Fantasy Medieval Ambient Music Pack | alkakrab | https://alkakrab.itch.io/free-fantasy-medieval-ambient-music-pack | **No formal licence**; the author writes "Absolutely Free For Commercial use" | 10 ambient tracks + loops, MP3/WAV/OGG, "no AI" | 3 | Get written terms (CC0 or CC BY) before shipping |
| soundimage.org | Eric Matyas | https://soundimage.org/attribution-info/ | Free with **credit in the game**: "Music by Eric Matyas / www.soundimage.org" (paid no-credit licence: $30 per track) | Large library | 4 | — |
| Pixabay Music | Pixabay | (licence above) | No credit | — | 3 | Content ID and AI tracks not addressed in the licence summary |
| **Do not use:** Tabletop Audio | — | https://tabletopaudio.com/about.html | **CC BY-NC-ND 4.0** | — | 0 | Non-commercial |
| **Do not use:** FreePD | — | https://freepd.com/ | Site offline (closure notice) | — | 0 | Avoid mirrors: you can't prove where the files came from |

---

## 13. Sky, VFX, shaders

| Name | Author / Source | URL | License | Contents | Mobile | Fit | Notes |
|---|---|---|---|---|---|---|---|
| Good SKY | Uneasy Game Dev (Fab) | https://www.fab.com/listings/6eb8de95-710e-45cf-a029-e48e709aef03 | Fab Std (P0/Pro0) | Sky blueprint: day/night, stars, moon, storm. 5 materials, 11 textures up to 2K. **Desktop, mobile and VR.** UE 4.18–5.8. 273 ratings | A | 4 | Development paused (critical fixes only). Base for day, dusk and night phases |
| QMS Cartoon Skybox Pack FREE | QM Studio (Fab) | https://www.fab.com/listings/fbd94684-627f-4a45-bfd6-8f35da74881e | Fab Std | 6 cartoon skies, 2048×1024, UE 5.0–5.4 | A | 5 | Mobile sky dome (the art direction asks for a hand-painted cloud dome on mobile) |
| Stylized Clouds Pack Vol 07 | PolyOne Studio (Fab) | https://www.fab.com/listings/fe7db3c6-506c-4def-9f3e-b621f4a39dc1 | Fab Std | 15 mesh clouds, ~32 tris each | A | 4 | Cloud cards on mobile |
| Cloudy Skyboxes | Screaming Brain Studios | https://opengameart.org/content/cloudy-skyboxes-0 · https://screamingbrainstudios.itch.io/cloudy-skyboxes-pack | CC0 | 25 cubemaps (512 px) + 25 panoramas (2048×1024), semi-realistic | A | 3 | — |
| Poly Haven HDRIs | Poly Haven | https://polyhaven.com/license | CC0 | Realistic HDRIs | B | 3 | Lighting reference only |
| FreeStylized skyboxes | freestylized.com | https://freestylized.com/all-skybox/ | Royalty-free, commercial OK | 130+ anime/Ghibli-style skies (2K free). **Partly made with generative AI** | A | 3 | Steam AI disclosure |
| Stylized Fire VFX | Vefects (Fab) | https://www.fab.com/listings/d2855faa-e4df-4acd-8f08-07dd5933d276 | Fab Std | Anime-style Niagara fire, UE 4.27, 5.0–5.8 | A/B | 5 | Fireplaces, torches, pyre |
| Stylish Fire VFX | VfxSTOCK (Fab) | https://www.fab.com/listings/01e8534c-5877-4ce2-8948-9a696100de11 | Fab Std | 4+ Niagara fires, UE 5.0–5.6 | A/B | 4 | — |
| [18] Free Spells Niagara | UrtanoVFX (Fab) | https://www.fab.com/listings/967b7892-d46c-4d75-8121-33418a6899ce | Fab Std | 18 magic effects | B | 4 | Witch and ghost abilities |
| Free Magic Niagara | Lord Enot Store (Fab) | https://www.fab.com/listings/d0fe50c4-6ebe-40d5-b78a-56960832f49e | Fab Std | Magic effects, UE 5.0–5.8 | B | 4 | — |
| Free Niagara Particles | SoftTofuVFX (Fab) | https://www.fab.com/listings/183732bc-c2fb-465c-9453-f70a1ce7ba2c | CC BY 4.0 | Sparkles, **feathers**, **leaves** | A | 4 | Death effects (feather storm), wind-blown leaves |
| Niagara Examples Pack | Epic Games (Fab) | https://www.fab.com/listings/0e188eca-4e54-4fb2-a9ed-d8b8a565e600 | Fab Std | Fire, smoke, **mist cards**, sparks, footsteps; built with scalability in mind. UE 5.7 | A/B | 4 | Mobile fog cards |
| M5 VFX Vol2 Fire and Flames | M5VFX (Fab) | https://www.fab.com/listings/c5b0270a-a295-4644-a4be-42cb1e56a197 | Fab Std | Fairly realistic fire and smoke | B | 3 | — |
| Fog Area | Light Stefar (Fab) | https://www.fab.com/listings/b68cca49-60ce-4460-a8de-433c683d720a | Fab Std (Personal free, Pro €0.86) | Local fog volumes (sphere/box), 1 material + 7 instances. UE 5.5–5.8 | PC | 3 | Crypt, harbour and fog-siege event |
| Concept Art Shader Pack | Lukas Schmidt (Fab) | https://www.fab.com/listings/af214474-9ba1-430f-9bdf-24010ab7673d | Fab Std | Post-process outlines, hatching, toon shading. UE 4.27, 5.0–5.5 | PC | 4 | Sobel outline reference |
| Simple Cel Shading Lite | Manyak Studios (Fab) | https://www.fab.com/listings/851ec366-f1b0-41c5-b008-84dadf9d51b5 | Fab Std (P0/Pro0) | 15 materials, 85 textures up to 1K, 19 test meshes. UE 5.4/5.8. Published 14 Sep 2026 | likely A | 4 | — |
| Simple Post Process Lite | Manyak Studios (Fab) | https://www.fab.com/listings/9eafb843-1716-48c0-ba93-aef955bfe65b | Fab Std | Comic-outline post-process | PC | 3 | — |
| Interactable Object Highlight | WalisonlLima (Fab) | https://www.fab.com/listings/9f4921fe-c0ee-4537-9bdb-8f2f57f3e324 | CC-BY | Outline highlight for interactable objects | A/B | 4 | Pick-up and interaction prompts |
| UE_CelLit | shjh3117 (GitHub) | https://github.com/shjh3117/UE_CelLit | MIT | Cel-lit plugin for UE 5.7.x, focused on characters and non-photoreal looks. Not compatible with Substrate; mobile not mentioned | ? | 3 | Check it works on 5.8 |
| Unreal5_ToonShader / Advanced-Outline-Shader-UE5 / Unreal-Engine-Toon-Outline | chrisloop / Arystos / kyaustad (GitHub) | https://github.com/chrisloop/Unreal5_ToonShader · https://github.com/Arystos/Advanced-Outline-Shader-UE5 · https://github.com/kyaustad/Unreal-Engine-Toon-Outline | **No licence file**, so not reusable | Toon and outline examples | – | 2 | Learning reference only |
| Painterly Sky Background | Kalponic Studio | https://kalponic-studio.itch.io/painterly-sky-background | Commercial OK, no redistribution | 50 2D images, **AI-made** | – | 2 | Avoid |
| *Rejected:* AllSky Free | — | — | Unity games only | — | – | 0 | — |

**Recommended:** build the toon look in-house, as `06_Art_Direction.md` already specifies:

- **Master material:** one master material with an `N·L` ramp.
- **Outlines:** inverted-hull outlines on characters and viewmodels, and a Sobel post-process outline on PC.
- **Sky:** SkyAtmosphere plus Volumetric Cloud on PC, a painted sky dome on mobile. Use the packs above as references and effect sources.

---

## 14. Optional paid upgrades (outside the free budget; listed for completeness)

| Item | Price (2026-09-24) | Why it could be worth it |
|---|---|---|
| Quaternius Pro tiers (Medieval Village, Fantasy Props, Stylized Nature, UAL1) | $9.99 each | Unlocks the remaining 30–40% of each style-anchor kit. Source tiers ($14.99–20) add .blend files and UE projects |
| KayKit Extra tiers (Dungeon $7.95 with tavern/bar/beds; RPG Tools $4.99 with **keys, padlocks, lockpicks, fishing gear**; Resource $4.99 with coins, chests) | $3.95–9.99 | Same gradient atlas |
| KayKit Mystery Series 6 / 5 / 4 | $19.99 each | Farmers, Cleric, Lorekeeper, **Witch**, Werewolf, Vampire: social-deduction archetypes |
| **Beelim "Early Modern / Colonial Town Scene"** · **"Colonial Harbour Scene"** | $4.50 · $4.66 | **The only explicitly colonial packs found.** https://beelim-solutions.itch.io/3d-models-early-town · https://beelim-solutions.itch.io/port-pack. Licence not checked |
| RG Poly Medieval Megapack – Low Poly | $32.20 (in Creator Bundle Vol. 1, $25, 85 packs, countdown running) | CC0, 1,304 models on **one shared atlas**, ~320 polys each, incl. docks and palisades. https://rg-poly.itch.io/medieval-megapack-low-poly · https://itch.io/b/3905/creator-bundle-volume-1 |
| EmaceArt Slavica Medieval Village/Town MEGA | $66 | 800+ assets with **interiors**; half-timbered modular walls. https://emaceart.itch.io/slavica |
| EmaceArt Gravefield (cemetery) / Cemetery Tomb Pack | $13.75 / $1.97 | 800+ modular graveyard pieces, LODs, mobile-friendly |
| Jampot Cellar Pack / Prison Pack | $2.49 each | Flat-colour cellars and cells |
| loafbrr Modular Sewer Set / Mausoleum Dungeon | $8 / $10 | Same author as the CC0 mine set |
| Venturon Dungeon & Catacombs · Miguel Lobo Low Poly Dungeon | $5 · $19.99 | Catacomb niches and sarcophagi · prison and forge rooms |
| Daniel Mistage Fisherman's Shack | $50 | Matches the Deep Sea Hunter's Station |
| Kevin Iglesias Basic Motions / Crafting / Melee / Throwing | $18 / $25 / $23 / $12 (4-pack $65) | Carry, pick up/drop, deaths, eat/drink, throwing knives |
| Auto-Rig Pro | $50 | Clean UE Mannequin export + facial rig |
| **Synty** POLYGON Pirate / Knights / Western | $49.99 ($25 sale) / $29.99 ($15) / $29.99 | 9 flintlock variants, muskets, blunderbusses, forts (Pirate); 56 modular house + 18 church pieces (Knights); wooden church, gallows, windmill (Western). UE 5.3+ |
| Synty Fantasy Village / Fantasy Kingdom / Dungeon Realms | $299.99 ($150) / $349.99 ($175) / $199.99 | Big kits; Kingdom has enterable interiors |
| Synty Sidekick Fantasy Villagers | $199.99 | Villagers with facial blendshapes |
| SyntyPass | $40/mo (3-month minimum) or $30/mo yearly | **Catch:** after you cancel, a live game may only get minor bug-fix updates. Buy packs outright instead |

**Synty EULA warning.** It bans NFTs, generative-AI training and use in "Metaverse-related and/or Game Creation Software". **Check this clause against the planned player house editor** before using any Synty asset, including the free Sidekick starter.

---

## 15. Gaps: nothing suitable free, so model in Blender or kitbash

| Need (from 04_Map_Village.md) | Best partial source | Plan |
|---|---|---|
| Colonial architecture (saltbox houses, clapboard, meeting house, customs house) | Carpenter Gothic Church (CC-BY); Beelim colonial packs (paid) | Kitbash MegaKit shells + custom clapboard and shutter trim pieces on the palette |
| Enterable lighthouse with 333-step spiral | Faro Costero / cotman_sam exteriors; TweakedStudios spiral stairs (unlicensed) | Model the shell and stairs; use an exterior pack for the look |
| Enterable windmill with a working mechanism | Pixel windmill (CC-BY) | Model the interior |
| Harbour crane, customs house, mail pier, ice-cream cart | – | Model; simple shapes |
| Puritan clothing (capotain hat, coif, white collar, cloak) and all cosmetic hats | Quaternius Outfits Fantasy (bases) | Model on the puppet or UBC rig |
| Statues (Vladimir & Estragon), chess tables, skittles, dartboard, tavern stage | KayKit Board Game Bits (chess pieces) | Model |
| Modular sea grotto with tide | zisongbr caves, loafbrr tunnels | Kitbash |
| Keys, lockpicks, padlocks (free) | KayKit RPG Tools Extra (paid) | Model (trivial) |
| First-person animations for flintlock and blunderbuss (draw, inspect, fire, ram-rod reload) | TheTeaGuns (moving parts), TheoKain (pivots) | Author in Blender on the arms rig |
| Rubber chicken, giant quill, clock-hand knife, shovel | – | Model; most are under 500 tris |
| Pre-fractured breakables | – | Fracture in UE (PC); pre-cut in Blender (mobile) |

---

## 16. Style unification: making mixed packs look like one game

**Goal:** everything reads as the same "painted toy village".

- **Colour:** handled by the palette atlas.
- **Shape language:** handled by pack choice and light geometry edits.
- **Lighting and shading:** handled by one toon master material, outlines and one colour grade.

### 16.1 Choose packs by shape language first

- **Prefer:** chunky, bevelled or softly faceted, slightly exaggerated proportions. Examples: Quaternius MegaKits, KayKit, Daniel Mistage, SimplePolygon, Azrael68, JayBee.
- **Avoid or rework:** PSX photo-textures, hand-painted texture detail, PBR realism, noisy micro-detail. Keep these as reference, or strip their textures and remap to the palette.
- **One hero style per category:**
  - Buildings: MegaKit.
  - Nature: Stylized Nature MegaKit, with SimplePolygon/KayKit only where MegaKit has no equivalent.
  - Props: Fantasy Props MegaKit first, then KayKit and Mistage.

  Mixing styles within one category shows up most.

### 16.2 Scale and grid (Blender 1 unit = 1 m; UE 1 uu = 1 cm)

- **Blender scene settings:** Unit System Metric, Unit Scale 1.0.
- **Export and import:** FBX export with "Apply Scalings: FBX Units Scale", UE import scale 1.0. Test with a 180 cm mannequin and a 100×210 cm door frame before batch-processing a kit.
- **Measure each kit's grid.** Measure MegaKit wall width and floor height, the JayBee grid, and the SimplePolygon dock plank length.
  - Rescale **whole kits uniformly** to the project standards: floor-to-floor 300 cm, door 100×210 cm, stair riser 18 cm.
  - Never scale individual modular pieces, or snapping breaks.
- **Known offenders:**
  - Fertile Soil terrain needs ×100.
  - KayKit Medieval Hexagon is diorama-scale; keep it for background only.
  - Azrael68 furniture is already in metres with bottom-centre pivots.
- **Pivot rules:**
  - Props: bottom-centre.
  - Modular pieces: the grid corner.
  - Doors, shutters, trapdoors, bookcase doors: the hinge edge.
  - Weapons: the palm grip.

### 16.3 One palette, one material (what `kg_process_asset.py` should do per source type)

| Source type | Examples | How to remap to `T_KG_Palette` (256², 8×8 swatches with vertical gradients) |
|---|---|---|
| Flat material colours | Quaternius older packs, Jampot, Quin.GS, RGS_Dev, TastyTony | Read each material's base colour → nearest palette swatch (compare in OKLab or CIELAB, not RGB) → collapse the UVs of that material's faces onto the swatch → delete the materials |
| Vertex colours | Azrael68, vertexcat, Faro Costero, Fertile Soil | Read the average vertex colour per face → nearest swatch → UV collapse. Optionally keep a vertex-colour channel for baked AO |
| Palette or gradient atlas | KayKit, Tiny Treats, JayBee, SimplePolygon, Se1dev, Mistage, Vertex Rage | Sample the texel under each face's UV centroid → nearest swatch → re-point the UVs. Keep the gradient direction (V axis = light to dark) so the pack's built-in shading survives |
| Textured, non-palette | Quaternius MegaKits, Typetree, i.we.d | Average texture colour per face (or per UV island) → nearest swatch. Review wood grain, roof tiles and stone by eye. If detail is lost, keep the original texture and only harmonise it with the colour grade, as a separate `MI_KG_Textured` |
| Hand-painted or PBR | cotman_sam, bitgem, SOI, StylArts | Don't auto-convert. Repaint, or rebuild the colours as flat faces |

- **Pick colours for readability, not accuracy.** Map to the **named palette** in `06_Art_Direction.md`:
  - roofs: Tile Red `#E0413A` / Orange `#F28C28`
  - walls: Cream `#FFF1D6`
  - timber: Wood `#A0612B` / Dark Wood `#5A3A22`
  - doors and trim: Mustard `#F2C230`
  - water: Turquoise `#1FB5C4`

  Give every house archetype a distinct roof and door colour so it can be recognised from 20 m.
- **Keep one material per mesh.** Metal and glass come from palette swatches plus a mask value (a spare palette row or a vertex-colour channel) that sets the single sharp specular highlight. No roughness, metallic or normal maps.
- **Viewmodel exception:** weapons and arms get their own 512–1024 px texture for skins (wear 0.00–1.00, pattern seed), but still start from palette colours.

### 16.4 Geometry clean-up

- **Decimate to budget.** Use Decimate (Collapse) or Limit Dissolve. Examples:
  - UBC ~13k → 6–10k
  - LowPolyBoy Fishing Town 286k → split and decimate
  - Kraken's Hideaway 52k → props
- **Shading normals:** use consistent face-weighted normals and "shade auto smooth" at ~30–40°. A mix of flat-shaded and smooth-shaded packs looks inconsistent. Bevel only the silhouette edges (1–2 segments) on hero pieces so the toon ramp and outline catch them.
- **Merge interior meshes.** Per room, merge static clutter that doesn't need physics into one mesh (or an ISM/Packed Level Actor, per `04_Map_Village.md`).
- **Split into separate objects:** anything that opens, breaks, is carried or is a barricade (doors, lids, shutters, chairs, tables, crates).
- **Breakables:** keep the whole mesh plus 3–8 pre-cut pieces in Blender (Cell Fracture add-on or manual cuts). Inner faces use a "raw wood" swatch. On PC, UE Fracture Mode Geometry Collections (`GC_KG_*`) can replace them.
- **Collision:** `UCX_` boxes for everything; simple convex hulls for furniture. Never use complex collision on physics furniture.
- **LODs:** LOD1 about 50%, LOD2 about 25%. House shells need an HLOD proxy. Trees need a billboard or impostor for the 2–3 km vista ring.

### 16.5 UE-side unification

- **Master material:** `M_KG_Palette`, with a toon ramp on `N·L` (3 bands; shadow = saturated dark colour, not black), rim light, and a vertex-colour AO multiply.
- **Parameters:** material instances expose Hue/Saturation/Value shift and a palette swap. That's how a pack that still looks "off" gets nudged into line, and how the night variant (`MI_KG_Palette_Night`) works.
- **Outlines:**
  - Characters and held items: inverted hull (mobile-safe).
  - Environment on PC: a depth+normal Sobel post-process that thins with distance.
  - Environment on mobile: no post outline; bake thin dark trim strips onto key building edges in Blender.
- **One colour grade** (LUT) per time-of-day phase, applied to everything. Push saturation here rather than per asset, so mismatched packs converge.
- **Height fog and atmosphere** tie mismatched distant assets together cheaply. Use lilac volumetric fog on PC and exponential height fog + cards on mobile, per the art direction.
- **Performance:**
  - No Nanite or Virtual Texture on anything meant for mobile. Stylized Fantasy Provencal and Tiny Talisman use them.
  - Watch draw calls: ≤350 on mobile. The single palette material is what makes batching possible.

---

## 17. Licence checklist and credits

### 17.1 What each licence requires

| Licence | Credit? | Other duties / traps | Examples in this report |
|---|---|---|---|
| CC0 1.0 | No (a thank-you line is optional) | None. Keep proof of the licence at download time | Quaternius (older packs and MegaKits), KayKit, Tiny Treats, JayBee, Azrael68, loafbrr, zisongbr, RandomMind, poly.pizza CC0 items |
| Quaternius Asset License (QAL) v1.0 (8/28/2026) | No | You may not repackage, resell or redistribute the assets themselves. Only the Bestiary (Aug 2026) is marked QAL so far; other pack pages still say CC0. **The version in force when you download is the one that applies**, so archive the pack page | Bestiary – Dungeon Monsters |
| CC BY 3.0 / 4.0 | **Yes**: title, author, source link, licence link, and whether you modified it | Edits allowed. Keep the credits screen current | Sketchfab CC-BY models, poly.pizza CC-BY 3.0 items, game-icons.net, Kevin MacLeod, Nakarada, Little Robot Sound Factory, tharlevfx Water, 1Luka2, LowPolyBoy, TheTeaGuns, Broken Vector, Goblinatron |
| CC BY-SA | Yes | **Edited versions must stay BY-SA**, so keep them out of core art | Kynnelo cave tunnels; OGA Church Bell |
| CC BY-ND | Yes | **No edits allowed**, so they can't be recoloured. Avoid | SimplePolygon's *Sketchfab* copies, DARK_PALETTE Halloween |
| CC BY-NC / NC-ND | – | **No commercial use.** Avoid | Tabletop Audio, Inkxo arms |
| SIL OFL 1.1 (fonts) | Recommended | Ship each `OFL.txt` and copyright line. Renamed subsets for Reserved Font Names | All Google Fonts listed |
| Fab Standard License | No | Commercial games OK; no redistributing raw assets. **Personal tier is limited to smaller creators** (reported US$100k/yr revenue limit; confirm) → the Professional tier is paid for some items | Many Fab rows (see "Personal free" marks) |
| Epic content / UE-only (GASP, templates, some legacy "UE Marketplace" items, conceptyphoon arms derived from GASP) | No | **Only inside Unreal Engine products.** Fine for this project | GASP, First Person template, Infuse Medieval Dungeon |
| Custom "free, credit required, no redistribution" | **Yes** | Credit exactly as the author asks | SimplePolygon (itch), ALSTRA INFINITE, Atomic Realm (link required) |
| Custom "free, no credit, no redistribution/resale" | No | Don't re-share the raw files (matters for any future modding SDK) | EmaceArt, Daniel Mistage (itch), Se1dev, CaptainCatSparrow, Kickin' It, Hatty |
| Mixamo (Adobe terms) | No | Don't distribute the raw FBX files; Adobe account needed; terms can change | Mixamo animations |
| Sonniss GDC licence | No | No redistributing as SFX; **no AI/ML training** | GDC bundles |
| Pixabay Content License | No | No standalone redistribution | Pixabay SFX and music |
| ZapSplat free tier | **Yes** (credit ZapSplat) | Gold removes it | ZapSplat |
| Synty EULA | No | 5 seats; no generative AI; **no "Game Creation Software"** (check against the house editor); SyntyPass cancel limits | Sidekick Starter, POLYGON |
| **NOT STATED** | – | **Do not ship.** Email the author for written terms (CC0 or CC BY) | TweakedStudios, LOKIT, Nicrom, Azrael68 Window Pack, Ayuo Dev, niko-3d, drelanns (itch), AnaMaTo, Gleao, Jampot Rustic, imaginais, Broken Vector trees, Grimm, 2Fasts, alkakrab (informal) |
| AI-generated content | – | **Steam requires disclosure.** Prefer to avoid | Polyy.AI weapons, "Knife FPS Animations" (Fab flag), SimpliGen Dark Cemetery (Fab flag), FreeStylized skies, Kalponic skies |
| Third-party textures inside "CC0" packs | – | The textures carry their own terms. **Replace them** | Elbolilloduro Sewers (texturelib/texturer), elopez7 sewer (textures.com) |

### 17.2 Process (feeds `Docs/Credits.md`, per 06_Art_Direction §9.5)

1. **For every imported pack, record:** pack name, author, source URL, licence and its version, download date, tier (free/Pro/Extra) and the exact credit text.
2. **Archive proof:** a screenshot or PDF of the pack page including the licence, the zip's `License.txt`, and the Fab order or library entry. Licences change (Quaternius QAL 2026, KayKit tiers).
3. **Tag each UE asset folder with its source** (e.g. `/Game/KG/Env/_Src_Quaternius_MVM/`) so credits can be audited later.
4. **Before shipping, run the checks below.**

| Check | Why |
|---|---|
| Is anything used still "NOT STATED"? | Must be none |
| Any AI-flagged items? | Steam disclosure |
| BY-SA edits kept isolated? | They must stay BY-SA |
| Fab items that need the Professional tier? | Pay before the revenue threshold is crossed |
| Is the Synty clause compatible with the house editor? | EULA bans "Game Creation Software" |

### 17.3 Credit lines, ready to paste (edit to match what you actually use)

```
3D MODELS
"Low Poly Sloop Sailing Ship" by Razer820 — https://sketchfab.com/3d-models/1b27f1f60e0e49f886984ea099977757 — CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/) — modified
"Low Poly Greek Fishing Boat" and "Stylized Low Poly Rowboat with Paddles" by Muyaya Concept — Sketchfab — CC BY 4.0 — modified
"The Lighthouse" by cotman_sam — https://sketchfab.com/3d-models/1a85945dd2a840f594bf6cb003176a54 — CC BY 4.0 — modified
Wooden Docks, Modular Cliffs, Basalt Rocks, Pine Tree Pack by SimplePolygon — https://simplepolygon.itch.io
Low-poly guns by TastyTony — https://sketchfab.com/TastyTony — CC BY 4.0 — modified
"Low poly pirate blunderbuss" by Milaein — https://sketchfab.com/3d-models/075bb97c059046e1b3c8447b54651d1b — CC BY 4.0 — modified
"Flat Shaded Low Poly Flintlock Weapons" by TheTeaGuns — https://theteaguns.itch.io — CC BY 4.0
"Low Poly Tavern Interior" by i.we.d — Sketchfab — CC BY 4.0
"Medieval Props Minipack" series by Typetree Studio (dyohandri) — Sketchfab — CC BY 4.0
"Haunted Cemetery Low Poly" by ronildo.facanha — Sketchfab — CC BY 4.0
"Stylized Halloween Asset Pack" by WillowBoxArt — Sketchfab — CC BY 4.0
"Carpenter Gothic Church" by jrich01 [CC-BY 3.0] via Poly Pizza (https://poly.pizza/m/Vq4Gik1t7c)
"Windmill" by Pixel [CC-BY 3.0] via Poly Pizza (https://poly.pizza/m/9BIMVqxyHV)
"Piano" by jeremy [CC-BY 3.0] via Poly Pizza (https://poly.pizza/m/7U-93vxPOER)
"Water Materials" by tharlevfx — Fab — CC BY 4.0
"Medieval Room Kit" by 1Luka2 — Fab — CC BY 4.0
"Fishing Town" / "Stylize Fish Market" by LowPolyBoy — Fab/Sketchfab — CC BY 4.0
Boats/Fish PolyPack by ALSTRA INFINITE — https://alstrainfinite.itch.io
Ocean Assets Starter Pack by Atomic Realm — https://atomicrealm.itch.io/ocean-assets-starter
Also CC0 assets by Quaternius (quaternius.com), Kay & Isa Lousberg (KayKit / Tiny Treats), JayBee Little Creations, Azrael68, loafbrr, zisongbr, CreativeTrio, Drillimpact — thank you!

ICONS
Icons by Lorc, Delapouite <add all used authors> — https://game-icons.net — CC BY 3.0 (https://creativecommons.org/licenses/by/3.0/)

MUSIC
"Danse Macabre" Kevin MacLeod (incompetech.com)
Licensed under Creative Commons: By Attribution 4.0 License
http://creativecommons.org/licenses/by/4.0/
(one block per track)
"Tavern Loop One" by Alexander Nakarada (alexandernakarada.bandcamp.com) — CC BY 4.0
Music by Eric Matyas — www.soundimage.org   (only if used)
Medieval music by RandomMind (OpenGameArt) — CC0 — thank you!

SOUND
"Horror Sound Effects Library" by Little Robot Sound Factory — https://opengameart.org/content/horror-sound-effects-library — CC BY 3.0
"<sound title>" by <username> (https://freesound.org/s/<id>/) — CC BY 4.0   (for any CC-BY Freesound clip)
Sound effects from ZapSplat (https://www.zapsplat.com)   (only if free-tier ZapSplat sounds are used)
Sonniss #GameAudioGDC bundle; Kenney (kenney.nl) — CC0

FONTS
IM FELL fonts © Igino Marini — SIL Open Font License 1.1
Pirata One © 2012 Rodrigo Fuenzalida, Nicolas Massi — SIL Open Font License 1.1
Alegreya © Juan Pablo del Peral, Huerta Tipográfica — SIL Open Font License 1.1
Nunito © Vernon Adams, Cyreal, Jacques Le Bailly — SIL Open Font License 1.1
(ship each font's OFL.txt with the game)
```

---

## 18. Unverified or open items

**Kits to measure in Blender**
- **Quaternius Medieval Village MegaKit:** triangle counts, grid size, and whether upper floors and balconies work. Measure after download.
- **JayBee house kit:** piece list (stairs, upper floor). It is marked "in development".

**Characters and rigs**
- **KayKit Rig_Medium:** bone count (23 comes from a third-party PR) and names.
- **Faces:** whether faces are texture-only.
- **Quaternius UBC:** bone naming compared with the UE Mannequin.

**Animation and weapon details**
- **Clip and weapon lists not captured:** Kuptchi PSX weapons (weapon list and 80+ animation names), bumstrum, TALLDOOR and J-Toastie clip names, the Knife FPS Animations clip list.
- **Mixamo clip names** (knife stab, pickup, crouch-walk): not confirmed individually. The terms were read on an Adobe Community copy of the FAQ, because helpx returned 403.
- **TastyTony uids:** the short ones (BFR, Colt SAA, Winchester 1897, Remington 870) were truncated during research, so look them up on the profile.

**Pricing and tiers**
- **Quaternius Pro tier:** itch lists it at $9.99, while quaternius.com lists Standard/Pro/Source without a price.
- **Faro Costero lighthouse:** "limited time" free, with no end date shown.
- **FANTASTIC Village, Low Poly Cliffs & Rocks, Water Physics:** €0 on both tiers but not flagged "free" in Fab's data. Confirm at claim time.

**Licences to confirm**
- **Beelim colonial packs:** licence not checked.
- **Pichuliru "CC0 Flat Guns West":** conflicting licence fields.
- **Poly.pizza models:** mostly without triangle counts. The CreativeTrio Frying Pan, MilkAndBanana Laddle and Quaternius Scythe licences were not opened.

**Pages that couldn't be loaded**
- Behind a Fab login ("mature content"): Medieval Fantasy Kitchen Accessories Pack 01, Bugrimov Horror Props, ithappy Food FREE.
- Kenney pages were skipped by request.
- A separate weapons agent looked at Quaternius "Ultimate Guns" (40 guns, types not listed), so whether it has a flintlock is unknown.

**Research coverage**
- Web-search quota ran out mid-research. itch.io browse pages only show about 36 results per tag combination, and pagination/free filters return 403, so itch coverage is broad but not exhaustive. A manual scroll of https://itch.io/game-assets/free/tag-3d/tag-low-poly in a normal browser may surface more.
