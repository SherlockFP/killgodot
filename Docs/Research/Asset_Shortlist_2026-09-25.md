# Asset Shortlist — 2026-09-25

Docs-only research pass to fill 7 confirmed gaps: village house facades/roofs, harbour/boat props
(+ a proper rowboat/skiff), Japanese garden pieces, forest/camp props + an animated CC0 wolf, Storm
Manor interior dressing, market goods, and nature variety. Sources come from `Docs/Research/AssetResearch.md`
(existing candidates, re-verified today), plus new WebSearch/WebFetch checks for the gaps that report had
no section for (Japanese garden, camp props, a confirmed wolf model, market goods variety).

**Verification date:** 2026-09-25, via WebSearch + WebFetch against the live itch.io / Sketchfab / OpenGameArt
pages. Fab.com blocked automated fetching (HTTP 403) for 4 entries below — those keep AssetResearch.md's
last-verified data and are flagged **[Fab: re-check license/price at claim time]**.

No downloads, code, or level changes were made. Nothing below has been imported. `Tools/Assets/approved_sources.json`
is unchanged — everything here is new and needs your go-ahead.

---

## Ranked shortlist

| # | Name | URL | License | Size | Format | Fills gap | Style fit | Perf notes |
|---|---|---|---|---|---|---|---|---|
| 1 | Animated Animals Low Poly (wolf, eagle, dog, cat, piranha) — Quaternius | [OpenGameArt](https://opengameart.org/content/animated-animales-low-poly) | CC0 1.0 | 2.1 MB | FBX | **Forest wolf** (blocks SPRINT-033c) | 4/5 — flat untextured colour, same era as our other Quaternius imports | 5 animals total, low poly (2017-era Quaternius, similar weight to `KG_Nature`); wolf has Death/Idle/Jump/Run/Walk-style clips per the pack's animation set |
| 2 | KayKit Forest Nature Pack | [itch.io](https://kaylousberg.itch.io/kaykit-forest) | CC0 1.0 | 6.1 MB (free tier) | FBX / glTF / OBJ | Nature variety | 5/5 — gradient atlas, same studio as furniture kits already in the game | 100+ trees/bushes/rocks/grass on one shared atlas; cheapest tree option for mobile (tier A in AssetResearch terms) |
| 3 | Free Fantasy Medieval Houses and Props Pack — EmaceArt | [itch.io](https://emaceart.itch.io/free-fantasy-medieval-houses-and-props-pack) | CC0 v1.0 badge + "please don't resell/redistribute standalone" | 4.2 MB (GLB) / 14 MB (FBX) | GLB, FBX, Blend, Unity, Godot | **Village facade/roof variety** (2nd style beside the Quaternius MegaKit already in `KG_Village`) | 3/5 — diffuse+normal textured, needs a bake to the palette | 165+ houses/inns, 3 LODs each, heaviest building ~35k tris (from AssetResearch); decimate + drop normal maps for B-tier mobile |
| 4 | Modular Village Pack — Fertile Soil Productions | [itch.io](https://fertile-soil-productions.itch.io/modular-village-pack) | CC0 1.0 | 860 KB | OBJ + MTL | **Village facade/roof variety** (blockout-friendly) | 4/5 — flat solid-colour materials, trivial to recolour to palette swatches | 155 modular pieces, no textures at all (colour only), so it's the cheapest possible facade variety add; check grid vs our 300 cm floor / 210 cm door before snapping |
| 5 | Wooden Docks — SimplePolygon | [itch.io](https://simplepolygon.itch.io/wooden-docks) | Custom: free commercial + edits, **credit required**, no resale | 1.3 MB (FBX) + 286 KB (palette PNG) | FBX | **Harbour docks** | 5/5 — ships its own palette PNG, same workflow as our pipeline | ~8.5k tris for the whole modular set (measured on Sketchfab mirror); enough for 3 piers + the mail pier |
| 6 | Stylized Low Poly Rowboat with Paddles (+ lifebuoy, buoys) — Muyaya Concept | [Sketchfab](https://sketchfab.com/3d-models/f2c35c716f32474e96cce3625073e6b8) | CC BY 4.0 | not shown (model is 1.3k tris; expect a few MB) | FBX / OBJ / GLB / Blend | **Small rowboat / skiff** | 4/5 — 1 material, simple shapes | 1,300 tris confirmed live; recolour hull to period colours; this is the one to make rowable (the existing `SM_KG_Rowboat` in `KG_WaterProps` can stay as the harbour's static dressing boat) |
| 7 | Pirate Kit — Quaternius | [quaternius.com](https://quaternius.com/packs/piratekit.html) | CC0 | not stated on page (older Quaternius kits run 20–50 MB) | FBX / OBJ / Blend / glTF | **Harbour prop variety** (cannons, dock pieces, animated pirate crew) | 4/5 — Quaternius textured-palette style, matches `PirateC3` already in the game | 71 models per AssetResearch; confirm exact size at claim (the itch mirror 404s — use the quaternius.com link) |
| 8 | Low Poly Japanese Garden Pack (12 models) — Akochan | [itch.io](https://akochan-lu.itch.io/low-poly-japanese-garden-pack-12) | Custom: royalty-free commercial + personal, **no resale of the assets themselves** | 3.5 MB | FBX / GLB / OBJ+MTL | **Japanese garden pieces** | 3/5 — shared texture atlas, needs a bake/remap to match `KG_JapanProps_Clean2`'s vertex-ish look | 12 sakura/bonsai/lantern/bamboo/zen props; smallest, cleanest Japanese-garden-specific pack found (no CC0 option surfaced for this niche) |
| 9 | Low Poly Market Pack — 56 Stalls, Produce & Shop Props — Akochan | [itch.io](https://akochan-lu.itch.io/low-poly-market-pack-56) | Same as #8 (royalty-free commercial, no resale) | 19 MB | FBX / GLB / OBJ+MTL | **Market goods** | 3/5 — same shared-atlas style as #8, so the two packs look consistent together | 56 models, ~2,764 tris/model average; biggest single add here — cull to the stalls/produce/signs you actually place, skip the "modern shop" pieces |
| 10 | Low Poly Food Asset Pack — Kyle Fuji | [itch.io](https://kyle-fuji.itch.io/low-poly-food-asset-pack) | CC0 1.0 | 9.5 MB (Godot package: FBX/glTF) | FBX / glTF | **Market goods** (fruit, veg, sushi, baked/junk food) | 3/5 — PBR atlas per category (512–1024 px), needs downscale + flatten to palette | 58 food props; use the Godot .zip (9.5 MB), skip the 75 MB Blend-source download and the Unity package |
| 11 | Lowpoly Goblin Camp (free folder) — ByndWalker | [itch.io](https://byndwalker.itch.io/lowpoly-goblin-camp) | CC0 1.0 | 5.6 MB | not stated (check on download; typically FBX/Blend for this creator) | **Forest/camp props** | 3/5 — style unconfirmed until opened in Blender, but CC0 and cheap | 2 cabins, 1 stone house, 5 tent designs, branch fences ×2, branch cage, 2 rock scatter variants — reskin the "tribal" look to our woodcutter camp palette |
| 12 | Vertex color trees set — vertexcat | [itch.io](https://vertexcat.itch.io/vertex-color-trees-set) | CC0 | not re-measured today (AssetResearch: small, single-digit MB) | not stated | Nature variety | 4/5 — **already vertex-coloured**, zero texture work | 6 trees, 116–597 tris each; cheapest possible tree variety add, ideal for background fill |
| 13 | Low Poly Nature: Essentials — Vertex Rage Studio | [Fab](https://www.fab.com/listings/a607441d-b811-440b-a20c-59e74804c4ce) | Fab Standard (Personal free / Pro €4.38) | not re-verified **[Fab: re-check at claim time]** | FBX (UE project) | Nature variety | 4/5 — 2 master materials, one 512 palette | 62 meshes: trees, bushes, grass, plants, rocks, fences; 32–1,612 polys each |
| 14 | Medieval Room Kit — 1Luka2 | [Fab](https://www.fab.com/listings/52610433-0210-40e5-884b-abc12cf8901e) | CC BY 4.0 | not re-verified **[Fab: re-check at claim time]** | FBX / UE project | **Mansion interior dressing** (general rooms) | 3/5 — hand-painted 4K PBR, needs a bake to palette and a 4K→1K downscale | 48 modular walls/floors/furniture/decor pieces, ~100–1k polys each, 3 materials/17 textures |
| 15 | Stylized Library (48 cozy props) — Daria Borovleva | [Fab](https://www.fab.com/listings/6150cf8e-8105-4da0-9209-3edbc2afb673) · [itch.io](https://drelanns.itch.io/stylized-library) | Fab Std (Personal free) · itch: **licence not stated — use the Fab link only** | not re-verified **[Fab: re-check at claim time]** | Fab: FBX/UE 5.7; check itch separately | **Mansion library** (Kütüphane, room #8) | 3/5 — PBR atlases 512–2K | 48 props incl. bookcases and a fireplace; drop the modern radio/camera items; complements `Azrael68_Bookshelves` (already approved) for shelving variety |
| 16 | STYLIZED Fantasy Armory — Daniel Mistage | [Fab](https://www.fab.com/listings/aaaa7ffa-c4dc-434b-a6ac-3f0030a28905) · [itch.io](https://daniel-mistage.itch.io/stylized-fantasy-armory-low-poly-3d-art) | Fab Std (Personal free) · itch: custom, no redistribution | not re-verified **[Fab: re-check at claim time]** | FBX / UE 5.5+ | **Mansion grand furniture** (Great Hall, Yemek Salonu dressing: weapon racks, chests, barrels, books, lamps) | 4/5 — 2 materials, one 2K atlas | 251 meshes, 60–20k verts (mostly 100–4.5k); also covers 14 building pieces for background use |
| 17 | STYLIZED Deep Sea Hunter's Station — Daniel Mistage | [Fab](https://www.fab.com/listings/b4878def-d341-4494-a244-62de2b3dcfa6) · [itch.io](https://daniel-mistage.itch.io/stylized-deep-sea-hunters-station) | Fab Std (Personal free) · itch: custom, no redistribution | not re-verified **[Fab: re-check at claim time]** | FBX / UE 5.5+ | **Harbour/fishing clutter** | 5/5 — 6 materials, 2K+1K atlases, same author/style as #16 | 130–140+ props: harpoons, hooks, chains, crates, chests, lamps, charts, fish heads, crab claws, captain's-cabin furniture |
| 18 | Stylize Fish Market — LowPolyBoy | [Fab](https://www.fab.com/listings/c27207bd-1949-4734-9329-65b9f1ec2a5a) | CC BY 4.0 | not re-verified **[Fab: re-check at claim time]** | .blend | Harbour/market | 2/5 — 170k tris total across the pack, needs heavy decimation before use | 68 meshes (fish stalls, crates, crab, octopus), 3 materials, 6 ~1.1K textures; lowest priority here, only worth it for a couple of hero stalls |

---

## Not filled by download (already planned as custom props)

Chandelier, portrait frames/gallery, billiards table + cue rack, and the wine-cellar racks are **already
scoped as custom `KG_DressManor` Blender props** in `Docs/Level/StormManor_Plan.md` §9.2 (e.g. the raise/lower
chandelier is a gameplay object tied to Görev 4, not generic decor — a stock asset wouldn't fit the rig). No
suitable free/CC0 billiards table or ornate picture frame turned up in today's search either, so building
these by hand (as already budgeted: chandelier 1.5k tris, billiards 800 tris, bookcase+rotating bookcase 600
tris) stays the right call. Entries #14–16 above cover the *generic* mansion dressing (room shells, library
shelving, hall furniture) around those custom pieces.

---

## Import note per entry (pipeline: sanitise → vertex-colour/palette → `kg_import_dress_pack.py`)

1. **Wolf pack** — sanitise: keep only `Wolf.fbx` from the zip (drop eagle/dog/cat/piranha unless wanted later). No texture to strip. Assign a grey/brown palette swatch per material slot, then run through `kg_import_dress_pack.py` like the other CC0 FBX drops.
2. **KayKit Forest** — already gradient-atlas; remap the atlas UVs to our palette swatches per material ID, same process used for other KayKit packs.
3. **EmaceArt Houses** — strip normal maps, bake diffuse to an average colour per face (per AssetResearch §16.3), decimate the 35k-tri hero buildings, then import.
4. **Fertile Soil Village** — already flat MTL colours; just remap each MTL colour to the nearest palette swatch, no baking needed.
5. **SimplePolygon Docks** — palette PNG ships with it; point the UVs at our `T_KG_Palette` or merge the two palettes, then import as one modular dock kit.
6. **Muyaya Rowboat** — single material; recolour hull/oars to period colours, keep it as a separate mesh from the static `SM_KG_Rowboat` so it can get physics/rowing later.
7. **Pirate Kit** — same treatment as the already-imported `PirateC3` props; palette-remap and drop into the same folder structure.
8–9. **Akochan Japan Garden + Market** — shared atlas per pack; average-colour-bake each material to the palette (do both packs in the same pass since they're the same author/style, keeps texel density consistent).
10. **Kyle Fuji Food** — downscale the 512–1024 px PBR atlas, strip roughness/normal, flatten to palette swatches per food category.
11. **ByndWalker Goblin Camp** — open in Blender first to confirm material setup (page didn't state it); likely flat colours given the pack's size, so a direct palette remap should work.
12. **vertexcat trees** — already vertex-coloured; just remap the existing vertex colours to our palette (fastest of any entry here).
13. **Vertex Rage Nature Essentials** — one 512 palette already; minor remap only.
14–15. **Room Kit + Library** — bake 4K PBR down to 1K, average-colour to palette, drop modern-looking props (radio, camera) before import.
16–17. **Armory + Hunter's Station** — same author, same 2K-atlas approach; palette-remap once, reuse the process for both.
18. **Fish Market** — decimate hard first (170k → a few thousand tris for the pieces you keep), then palette-remap; lowest priority, do this last if at all.

---

## Download approval list

Copy/paste-ready for a one-shot yes. Sizes are the smallest suitable file per entry (see table above for
alternates).

| # | File to download | Source URL | Size |
|---|---|---|---|
| 1 | `Animated_Animales_Low_Poly.zip` (wolf + 4 others, FBX) | https://opengameart.org/content/animated-animales-low-poly | 2.1 MB |
| 2 | `KayKit_Forest_Nature_Pack.zip` (free tier) | https://kaylousberg.itch.io/kaykit-forest | 6.1 MB |
| 3 | `Free_Fantasy_Medieval_Houses.glb` (GLB variant) | https://emaceart.itch.io/free-fantasy-medieval-houses-and-props-pack | 4.2 MB |
| 4 | `modular_village_collection.zip` | https://fertile-soil-productions.itch.io/modular-village-pack | 860 KB |
| 5 | `LowpolyDocks.fbx` + `SP's Color Palette.png` | https://simplepolygon.itch.io/wooden-docks | 1.3 MB + 286 KB |
| 6 | Rowboat + Paddles bundle (FBX or GLB) | https://sketchfab.com/3d-models/f2c35c716f32474e96cce3625073e6b8 | a few MB (unconfirmed) |
| 7 | Pirate Kit (FBX or glTF) | https://quaternius.com/packs/piratekit.html | unconfirmed, expect 20–50 MB |
| 8 | `Low_Poly_Japanese_Garden_Pack.zip` (smooth-shaded) | https://akochan-lu.itch.io/low-poly-japanese-garden-pack-12 | 3.5 MB |
| 9 | `Low_Poly_Market_Pack_56.zip` (smooth-shaded) | https://akochan-lu.itch.io/low-poly-market-pack-56 | 19 MB |
| 10 | `Low_Poly_Food_Asset_Pack_Godot.zip` | https://kyle-fuji.itch.io/low-poly-food-asset-pack | 9.5 MB |
| 11 | `byndwalker_goblin_camp_free.zip` | https://byndwalker.itch.io/lowpoly-goblin-camp | 5.6 MB |
| 12 | Vertex color trees set (full zip) | https://vertexcat.itch.io/vertex-color-trees-set | small, unconfirmed today |
| 13–18 | 6 Fab listings — **price/availability must be re-confirmed on Fab at claim time** (automated fetch was blocked, HTTP 403) | see table rows #13–18 | unconfirmed |

**Total confirmed download size (entries 1–12): ~53 MB.** Entries 13–18 (Fab) need a manual visit before
committing — Fab blocked the automated check today, and AssetResearch.md already flags Fab pricing/tier as
something to reconfirm at claim time.

Say the word (all of #1–12, a subset, or also try the Fab items) and the actual download/import can happen
in a separate pass — nothing was fetched in this research pass.
