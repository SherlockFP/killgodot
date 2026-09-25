# Morrowmere v2 — "The Amphitheatre Cove"

Lens: composition and landmarks. Morrowmere should read the way a Sea of Thieves outpost, a Zelda village or a Ghost of Tsushima vista does.
Diagram: `Docs/Level/v2_iconic.png`. It is a top-down plan in metres in the UE frame (x east, +y toward the sea; the sea is at the bottom of the image, as in the editor top view), with a section cut along the main axis.

> **Özet (TR):** Köy artık düz bir çayıra serpiştirilmiş evler olmayacak. Limana bakan bir **amfitiyatro** olacak: dört teras (rıhtım 1.8 m, Meydan 6 m, Yukarı Mahalle 10 m, Taç Tepesi 16 m) yuvarlak bir liman havuzuna iniyor. Tek bir **ana eksen** her şeyi bağlıyor: çan kulesi → belediye saat kulesi → çeşme → Büyük Merdiven → iskele → liman ağzı → deniz toriisi → ada pagodası. Bu eksenin iki yanında iki dev simge duruyor: sağ önde fener, sol arkada çan kulesi. Evler bitişik sıralar halinde; her mahallenin kendi rengi ve malzemesi var. Meydanın ortasında bir çeşme var ve meydan dört köşeden kapalı. Denize açılan tek pencere Büyük Merdiven.

---

## 1. Diagnosis: why the current map reads as "things placed side by side"

Sources: `Tools/Level/morrowmere_layout.json`, `Tools/Blender/kg_build_terrain.py`, `Tools/Unreal/kg_build_village.py`, and the captures `KG_Cap_top/aerial/plaza`, `KG_Cap_harbour_base_over`, `KG_Cap_church_pre_over`, `KG_Cap_japan_base_over`, plus three new eye-level shots `KG_Cap_plan_iconic_1..3`.

1. **One flat plate, no elevation hierarchy.** Everything sits on `plateau_z = 4.0`. The terrain pulls every lane and area toward that plateau (`lerp(h, PLATEAU…, 0.7*lane_factor)`), and every building gets a flat pad. The only relief is a 2.5 m "church knoll" at (-28,-48) and a 9 m lighthouse bump at (70,57). In the aerial capture every ridge line is the same height, so the roofscape is a uniform carpet with no skyline.
2. **The square is a void, not a room.** `square` is a 13 m-radius dirt disc at (0,-8) with nothing in its centre. The well (-7,-3), gallows (6,-12) and notice board (-6,-14) sit on its rim like litter. The closest facades are the town hall front (y ≈ -22) and houses at (-14,4) and (20,6), all 14-20 m from the centre, with grass gaps between them. Capture `KG_Cap_plaza` shows a tan field with four detached houses far off: no enclosure (a good plaza needs a width-to-eave ratio of about 2-3:1; here it is about 6:1), and no heart.
3. **Detached boxes with random facings.** The 16 houses are free-standing 4-8 m boxes with 6-12 m of grass around each: no party walls, no building line, no rows. Facings point at arbitrary lane vertices, for example (-30,6) faces (-30,-8) and (50,16) faces (58,18). Houses are also the same size, style and colour everywhere (red tile over stucco/half-timber), so no district reads as a place.
4. **Spoke lanes that lead nowhere.** Seven wobbly 2.4-5 m dirt splines radiate from the disc. None of them ends on a landmark, and none has a hierarchy of main street versus alley. `harbour_street` runs straight to the pier at x=0, which is the one real axis, but `KG_Cap_plan_iconic_1` shows it ending on an empty horizon because the island at (45,150) sits 16° off it.
5. **Landmarks hidden or pushed into corners.** The church (-28,-48) and bell tower (-19,-52) sit behind houses and trees at the back, on a 2.5 m knoll (`KG_Cap_church_pre_over`). The lighthouse is alone in the far east corner (70,57). The town hall is an ordinary 8x10 box. The tallest things in most views are trees, so nothing is visible from everywhere. The 04_Map doc promised a bell tower and a Dead Tree in the square, and neither exists.
6. **A ruler-straight coast.** The coast in `height()` depends only on y (`smooth(-34,-70,y)`), which gives a straight east-west beach from x=-90 to 90 (see `KG_Cap_top`). There is no cove, no quay wall, no headland framing and no harbour mouth. The pier is a single straight 44 m plank off a sandy beach.
7. **Random scatter reads as noise.** 230 trees are scattered at 15-140 m radius, and props are dropped into lane centres (`KG_Cap_plan_iconic_1`: barrels and benches mid-street). Trees fill the gaps between houses, so the village dissolves into the forest instead of reading as a figure against a ground.
8. **Themed pieces are wedged in.** The Japanese garden (26,20) is a lawn with a bridge between houses (20,6), (11,20) and (32,6) (`KG_Cap_japan_base_over`). It has no relationship to the shrine island it is meant to echo.
9. **Uniform background.** The mountain ring is the same at every bearing (radius 120-210 m), so turning around gives no sense of orientation and no reveal.

**Root cause:** there is no composition hierarchy. The map has no axis, no terraces, no enclosure, no frames and no landmark placed where a view ends.

---

## 2. Concept

**The village is a theatre whose stage is the harbour.** Four terraces step down in a horseshoe to a round stone basin enclosed by a curving mole (west) and a cliff headland (east). The **harbour mouth frames the shrine island**. One **Processional Axis** runs from the church bell tower on Crown Hill through the town hall clock tower, the fountain, the Grand Stair and the Long Jetty, out through the harbour mouth to the sea torii and the pagoda. Walk it one way and you see the island. Walk it back and you see the bell tower. **Two hero towers balance the composition diagonally:** the Bell Tower (inland, centre-left, top about 32 m) and the Lighthouse (seaward, right, top about 32 m). The windmill (east skyline) and the harbour light (left, low) act as minor counterweights.

Skyline rule: ridge heights climb from the sea to the crown like a pyramid, giving a quay row at about 11 m, heart houses at about 15 m, the upper row at about 19 m and the church at about 26 m. Towers punctuate it. From any street, looking inland shows a rising stack of roofs, and looking seaward shows roofs dropping away to water.

## 3. Terrain and elevation (z in m, sea = 0)

| Terrace | z | Extent (approx.) | Edge |
|---|---|---|---|
| Harbour basin | -3 (floor) | quay edge (-22,58)…(0,43.5)…(17,41.5)…(32,44)…(43,58)…(44,84)…(49,106) | stone quay wall, 1.8 m above water |
| **Quay / Harbour Row** | 1.8 | band between the basin and the Harbour Wall, x -30…48 | **Harbour Wall** 4.2 m high: (-24,22)(-12,27)(0,28.5)(20,28.5)(36,29)(46,36) |
| **The Heart** | 6.0 | (-30,-22)…(36,-22)(42,-10)(54,6)(56,26)(46,36) down to the Harbour Wall | **Upper Wall** 4 m: (-30,-22)→(36,-22) |
| **Upper Town** | 10.0 | y -22…-52, x -50…40 | **Crown bank** 6 m (stone-faced grassy bank): (-40,-56)(-24,-54)(0,-52)(18,-54)(28,-64) |
| **Crown Hill** | 16.0 | y -52…-92, x -52…28 | backed by the forest ring |
| Brookside valley | slopes 13 (y -70) → 1.5 (y 60) | west of x ≈ -30 | the brook's banks; gentle, no walls |
| Orchard upland | 9.0; windmill knoll 12 (r 10 m at (76,-48)) | x 36…106, y -78…10 | grassy slope up from the Heart (z 6 → 9) |
| Lighthouse Point | 8 (y 20) → 13 (y 70+) | headland east of the basin | **sea cliffs** 8-13 m on its west, south and east faces |
| Mole (breakwater) | 2.4 | (-24,59)(-23.5,82)(-17,96)(-4,103)(10,105), 7 m wide rock-armoured, 3.5 m walk | harbour light at the tip |
| Shingle beach | 0.8 | west shore x -110…-28, y 60…68 | brook mouth at (-38,66) |
| Surroundings | rises from 7 to 40-55 m beyond r ≈ 95 m | hand-placed forest groves frame the bowl | open to the sea only in the south |

**Water.** The **Morrow Brook** (3-4 m wide, 0.4 m deep, wadeable) drops from a small waterfall in the NW forest at (-80,-80), passes the forge waterwheel (-47,-14) and the Old Stone Bridge (-41,3), falls over a **cascade** at (-34,44) beside the quay, and runs across the shingle beach to the sea (-38,66). The **koi pond** in the Sakura Garden (43,20) spills a thin cascade down the Harbour Wall near (44,33).

Terrain implementation note: replace the radial plateau in `kg_build_terrain.py` with polygon terraces taken from the layout (`terraces: [{name, z | z_from/z_to along y, poly}]`). Retaining walls and quay walls are kit stone wall runs placed along `walls[]` polylines. Stairs are kit steps with an invisible collision ramp so the NavMesh treats them as walkable. Every stair shown in the plan also has a ramp route nearby (Well Ramp, Graveyard Path, Orchard Lane, Brook Path).

## 4. Districts and identity

| District | Where | Walls / roofs / accents | Mood |
|---|---|---|---|
| **Harbour Row** | quay, z 1.8 | pastel stucco (sea-glass teal, coral, butter, sky blue) with white trim; terracotta roofs; blue shutters, orange buoys, nets, ropes | Portofino postcard; busy, noisy |
| **The Heart** | Fountain Square, z 6 | cream limestone, bright red tile; red/white striped awnings, yellow-red bunting, white fountain | the living room of the town; meetings and trials |
| **Upper Town** | Balcony Lane, z 10 | ochre half-timber and dark oak (today's kit look), deep red-brown roofs; geranium boxes, green shutters | quiet, domestic, views over the roofs |
| **Crown Hill** | z 16 | blue-grey stone, slate roofs; gold bell, purple stained-glass glow, lavender, cypress and yew | solemn, windy, the highest ground |
| **Brookside** | west valley | dark timber, mossy stone, dark shingle; forge glow, waterwheel, willow | work and water noise, shade |
| **Orchard Upland** | east, z 9 | whitewash with barn-red trim, straw thatch; golden wheat, apple rows, windmill | open, sunny, long sightlines, hard to hide |
| **Sakura Garden** (+ island) | east edge of the Heart | vermilion, sakura pink, dark pine, raked gravel | the calm place, spiritually linked to the island |
| **Lighthouse Point** | headland | white/red striped tower, grey granite, yellow gorse | exposed, dramatic, the edge of the world |

## 5. The Town Heart: Fountain Square (z 6)

- Square polygon: (-4,-10)(24,-10)(27,3)(25,17)(1,18)(-5,9), about 30 x 27 m, cobbled. It is enclosed on three sides by continuous 2-storey frontage (eaves about 5-6 m, so width to height is about 2.5:1).
- **Fountain** at (10,4), 7 m across, with a white stone basin and a statue (Vladimir & Estragon, per lore). It stands on the axis and is the spawn centre.
- **Moot Stage + gallows** at (4,-7): an 8 x 4 m timber platform in front of the town hall steps. This is the meeting and trial place, and everyone faces the town hall.
- **Dead Tree** (Kuru Ağaç) at (-1,-3): a black twisted silhouette against the cream facades, forming a triangle with the fountain and the stage.
- **Notice board** at (15,-10), on the town hall steps beside the clock tower.
- **Market stalls** along the east edge, x 21-25, y -6…12, under striped awnings.
- **Openings (pinwheel, so no street looks straight through the square):** NW corner has Pilgrim's Way (north, up) and Rope Walk (west). NE corner has Orchard Lane (east, up). SE corner has Sakura Walk. SW corner has the Cat Steps (down). The south edge is the **only window**: a balustrade with the Grand Stair cut into it, framed by H4 and H5, looking over Harbour Row roofs into the basin and out to the island (S1).
- **Spawn:** 20 player starts in a ring of radius 8.5 m around the fountain, facing inward.

## 6. Street network

| Name | Width | Polyline (x,y) | Type / z |
|---|---|---|---|
| Quay Promenade | 7.0 | (-26,55)(-16,46)(0,40)(17,38.5)(30,40.5)(40,47)(43,56) | stone quay, 1.8 |
| Tide Alley | 2.4 | (-14,31.5)(0,30.5)(12,30.3) | back alley behind Harbour Row, under the Harbour Wall |
| **Grand Stair** | 7.0 | (12.8,16.5)(14.9,26)(17,37) | stair cut into the terrace, 3 flights and 2 landings, 6 → 1.8, on axis |
| Cat Steps | 2.6 | (-3,17)(-7,27)(-12,40) | stepped alley (optional arch over it from M2) |
| Net Stairs | 3.0 | (30,16)(31,29)(33,40) | stair to the fish market |
| Rope Walk | 4.5 | (-4,-9)(-18,-12)(-30,-6)(-38,1)(-44,3)(-54,1)(-64,-10)(-70,-22) | street across the Old Stone Bridge to the woodcutter |
| Brook Path | 3.0 | (-38,5)(-35,18)(-31,32)(-28,44)(-24,52) | path down the brook's east bank past the cascade to the quay (bot route 6 → 1.8) |
| **Pilgrim's Way** | 4.0 | (-2,-11)(-2,-24)(-2,-28)(-1,-44)(6,-49)(-1,-54)(-3,-57) | stair through the Upper Wall, then switchbacks to the Belvedere, 6 → 16 |
| Upper Lane ("Balcony Lane") | 3.6 | (-46,-29)(-30,-27)(-16,-28)(-2,-27)(12,-27)(26,-26)(36,-22)(42,-15) | lane at 10 with a parapet on its south side overlooking the Heart |
| Well Ramp | 3.6 | (-26,-9)(-36,-16)(-44,-25) | ramp 6 → 10 (about 1:6) |
| Graveyard Path | 3.2 | (-46,-29)(-48,-44)(-42,-58) | ramp 10 → 16 |
| Orchard Lane | 4.5 | (24,-8)(36,-13)(46,-18)(58,-24)(68,-32)(73,-42) | street 6 → 9 → windmill knoll 12 |
| Sakura Walk | 3.0 | (25,15)(34,18)(42,20)(45,29) | gravel path to the garden torii |
| Cliff Path | 2.6 | (45,30)(54,40)(50,50)(57,62)(62,76)(65,82) | rope-railed switchbacks 6 → 13 |
| Cliff Stair | 2.4 | (43,55)(48,48) | quay → first switchback |
| Long Jetty (+ T head) | 4.0 | (18,41)→(25,73); head (19,73.5)→(31,72.5) | timber, on axis; rowboats to the island |
| Fish Pier | 3.0 | (38,48)→(39,60) | timber |
| Mole Walk | 3.5 | (-24,59)(-23.5,82)(-17,96)(-4,103)(10,105) | breakwater walk to the harbour light (a dead-end risk spot) |

Loops: Heart ↔ Quay has 5 links (Brook Path, Cat Steps, Grand Stair, Net Stairs, Sakura/Cliff). Heart ↔ Upper has 3 (Well Ramp, Pilgrim's Way, Orchard Lane via Balcony Lane). Upper ↔ Crown has 2 (Pilgrim's Way, Graveyard Path). The only deliberate dead ends are exposed risk spots: the lighthouse top, the mole tip, the jetty head, the bell tower top and the island.

## 7. Buildings (footprint w x d in m, w = front width; face = the point the front door faces)

**Player homes (20):**

| ID | District | Centre | Size | Faces |
|---|---|---|---|---|
| H1 | Harbour Row | (-21,42) | 6x6 | harbour centre (14,74) |
| H2 | Harbour Row | (-6,37) | 4x6 | (14,74) |
| H3 | Harbour Row | (0,35) | 6x6 | (14,74) |
| H4 | Harbour Row | (7,34) | 6x6 | (14,74), west gatepost of the Grand Stair |
| H5 | Harbour Row | (24,34) | 6x6 | (14,74), east gatepost |
| M1 | Heart | (-9,7) | 6x6 | square (0,7) |
| M2 | Heart ("Balcony House") | (-16,19) | 6x6 | sea (-16,40) |
| M3 | Heart | (31,11) | 6x6 | square (22,11) |
| M4 | Heart | (19,-16) | 6x6 | square (19,0) |
| U1 | Upper | (-34,-36) | 6x8 | downhill (-34,-20) |
| U2 | Upper (Well Court) | (-27,-43) | 6x6 | court (-16,-43) |
| U3 | Upper (Well Court) | (-6,-38) | 6x6 | court (-20,-38) |
| U4 | Upper | (6,-37) | 6x6 | downhill (6,-20) |
| U5 | Upper | (17,-36) | 4x6 | downhill (17,-20) |
| U6 | Upper | (28,-36) | 6x6 | downhill (28,-20) |
| B1 | Brookside | (-27,11) | 6x6 | brook (-40,11) |
| B2 | Brookside | (-54,-8) | 4x6 | brook (-44,-8) |
| B3 | Brookside | (-54,12) | 6x8 | Rope Walk (-50,0) |
| O1 | Orchard | (50,-7) | 6x6 | Orchard Lane (44,-16) |
| O2 | Orchard | (74,-8) | 6x6 | sea view (74,10) |

Harbour Row and Upper Town are built as **rows**: 0-1 m gaps, closed with kit wall pieces or garden walls and alternating 2 and 3 storeys, so each district reads as one continuous facade. Harbour Row radiates around the basin like theatre seats.

**Civic buildings and landmarks:**

| ID | Name | Centre | Size | Faces | Note |
|---|---|---|---|---|---|
| TH | Town Hall | (4,-16) | 8x10 | square (4,0) | back wall is the Upper Wall; meeting hall, archive, cells |
| CT | Clock Tower | (11,-17) | 4x4, 3 levels, top ≈18 m | — | climbable (ladder); watchman over the square; offset from the axis so the bell tower is not hidden |
| INN | Inn "The Latecomer" | (32,0) | 8x8 | square (20,0) | balcony over the square |
| BK | Bakery | (-9,-4) | 6x6 | square (0,-4) | chimney smoke marks the NW corner |
| CH | Church | (-16,-68) | 8x12 | town (-16,-50) | candles; stained-glass glow at night |
| BT | Bell Tower | (-6,-63) | 4x4, 4 levels, top ≈32 m | — | **hero landmark #1**, on the axis |
| — | Belvedere | (-12…14, -60…-53) | terrace | — | parapet over the whole town, the reward vista (S3) |
| — | Graveyard | (-40…-25, -79…-62) | 15x17 | — | fenced, cypress row |
| MA | Mausoleum | (-33,-83) | 4x4 | graveyard | **future catacombs entrance** |
| — | Old Well | (-16,-34) | Ø 3 | — | in Well Court (-24…-9, -39…-29) beside the Old Oak; **future cellar** |
| SM | Forge (open shed) | (-37,-13) | 4x6 | brook | waterwheel on the brook at (-47,-14) |
| — | Old Stone Bridge | (-41,3) | 7x5.5 | — | Rope Walk over the brook |
| — | Woodcutter camp | (-70,-26) | — | — | forest edge |
| BH | Boathouse | (-25,52) | 6x8 | basin (-10,56) | slipway into the basin |
| FM | Fish Market hall | (37,41) | 8x6, open sides | basin | nets, crates, gulls |
| KH | Keeper's hut | (57,76) | 4x4 | — | on the Cliff Path |
| LH | Lighthouse | (66,86) | 4x4, 5 levels, top ≈32 m | — | **hero landmark #2** |
| — | Harbour light | (10,105) | small beacon | — | mole tip; green night light |
| MB | Mill Barn | (63,-17) | 8x8 | lane (56,-26) | |
| — | Windmill | (76,-48) | Ø 6, new Blender prop, top ≈26 m | — | on the knoll; turning sails (`C.mover`) |
| — | Fields / pen | (50…66, -48…-34) / (42…50, -40…-30) | — | — | carrots, wheat; animals |
| — | Sakura Garden | (34…55, 10…33) | — | — | koi pond (43,20), Giant Sakura (51,15), garden torii (45,30) |
| — | Shrine island | (45,150), r ≈17 | — | — | pagoda; sea torii at (42,119) |

## 8. Landmarks and sightlines

| # | Viewpoint → target | What the player sees (fore / mid / back) |
|---|---|---|
| **S1 Postcard** | square south balustrade (12,14) → pagoda (45,150) | stair balustrades and H4/H5 as the frame / the basin with the jetty as a leading line / the harbour mouth between the mole light and the cliffs, then the sea torii, then the pagoda |
| **S2 Pilgrim view** | jetty head (24,70) → bell tower | boats and jetty / Grand Stair rising to the fountain, town hall and clock tower / Upper Town roofs, church, bell tower and the mountain ring |
| **S3 Belvedere** | Belvedere (2,-56) → lighthouse | Upper Town roofs / the square and Harbour Row / basin, lighthouse, island (panorama reward) |
| **S4 Lighthouse reveal** | West quay by the Brook Path foot (-24,50) → lighthouse | the cascade and boathouse / the basin curve / the lighthouse on its cliff, revealed as you come round the brook bend |
| **S5 Bridge** | Old Stone Bridge (-41,5) → waterwheel | brook / turning wheel / forge glow |
| **S6 Sakura gate** | garden (43,22) → sea torii | koi pond / vermilion garden torii framing / the island in the gap beyond the cliff edge |

Reveal moments:
- Rope Walk → Fountain Square: the corner entry compresses you, then the square opens with the fountain.
- Grand Stair: the harbour opens with each flight.
- Cliff Path: the second switchback suddenly shows open sea.
- Pilgrim's Way: the last flight tops out at the Belvedere panorama.
- Orchard Lane: the windmill appears over the crest.

Night: warm windows everywhere, the lighthouse beam sweeping the basin, the bell tower lit from inside, lanterns on the jetty and a green harbour light. The two towers become the navigation beacons.

## 9. Chores (existing 18 plus 3 optional)

| # | Chore | New position | District / elevation |
|---|---|---|---|
| 1 | DrawWater | (-16,-32) Old Well | Upper 10 |
| 2 | PostNotice | (15,-10) | Heart 6 |
| 3 | FileReports | (4,-10) town hall step | Heart 6 |
| 4 | RingBell | (-6,-63) top of bell tower | Crown 16 + 9 |
| 5 | LightCandles | (-16,-60) church door | Crown 16 |
| 6 | TendGraves | (-32,-70) | Crown 16 |
| 7 | ForgeNails | (-40,-13) | Brookside |
| 8 | SharpenTools | (-44,-17) grindstone at the waterwheel | Brookside |
| 9 | BakeBread | (-5,-4) | Heart 6 |
| 10 | PourAle | (26,0) | Heart 6 |
| 11 | StockStall | (22,7) | Heart 6 |
| 12 | MendNets | (37,46) | Quay 1.8 |
| 13 | UnloadFish | (25,73) jetty head | Quay / sea |
| 14 | FuelLighthouse | (66,86) top | Point 13 + 12 |
| 15 | HarvestCarrots | (58,-41) | Orchard 9 |
| 16 | FeedAnimals | (46,-35) | Orchard 9 |
| 17 | ChopWood | (-70,-24) | forest edge |
| 18 | FixBoat | (-20,54) boathouse slip | Quay 1.8 |
| 19* | WindClock (optional) | clock tower top (11,-17) | Heart 6 + 6 |
| 20* | FeedKoi (optional) | (40,22) | Sakura Garden |
| 21* | LightHarbourLamp (optional) | mole tip (10,105) | Mole (long exposed walk) |

Every terrace and district has at least 2 chores, and the task list forces vertical travel through stairs, which are ambush points. Chores stay outside buildings, at front steps, as today (bots cannot open doors).

## 10. Set-dressing zones (for `Tools/Unreal/dressing/`)

| Zone module | Area | Vignettes |
|---|---|---|
| `dress_square` → **heart** | Fountain Square + NW/NE/SE corners | fountain benches, market stalls, bunting across the corners, stage props, pigeons, café tables at the inn |
| **harbour** (new) | quay, jetty, fish pier, mole, boathouse, Tide Alley | net racks, crates, buoys, moored rowboats, crab pots, a crane, gull perches, mole rocks |
| **upper** (new) | Balcony Lane, Well Court | flower boxes, laundry lines between houses, cats, parapet planters, Old Oak with a swing |
| `dress_church` → **crown** | church, graveyard, belvedere, mausoleum | cypress rows, lanterns, tomb clusters, telescope on the belvedere |
| **brookside** (new) | brook, bridge, forge, woodcutter | waterwheel mover, stepping stones, reeds, anvil yard, log piles |
| `dress_countryside` → **orchard** | fields, pen, barn, windmill | hay bales, scarecrows, apple rows, fences, sail mover |
| **garden** (new) | Sakura Garden | lanterns, raked gravel, stepping stones, bamboo |
| **point** (new) | Cliff Path, lighthouse, keeper's hut | rope rails, gorse, oil barrels, a signal cannon |
| `dress_wilds` | outside the core | **hand-placed groves** (NW, N, NE, E, W masses as in the diagram), rocks, the brook waterfall. **No random tree scatter inside the core**: only hero trees (Old Oak, Giant Sakura, Willow at the bridge, Yew and cypress on the Crown) |

## 11. Size, performance, build order

- **Core** is x -78…82, y -90…60, which gives 160 x 150 m of land. The harbour basin, mole and point sit at y 40…106. Nav bounds should cover x -80…85, y -92…108, z -2…40. From the square to the lighthouse is about 110 m of path (about 19 s running), and to the church about 75 m.
- **Performance:** the terraces and retaining walls act as natural occluders (Upper Town is hidden from the quay by the Harbour Wall and the Heart frontage). Rows use shared kit walls, so there are fewer corner pieces. Dressing respects the existing budget of 900 actors per zone with HISM for repeats. Trees drop from 230 scattered to about 120 in groves outside the core.
- **Order (visible progress first):**
  1. Terrain v2: terraces, basin, mole, headland, brook, and the three walls. This is the biggest visual change.
  2. Move the landmarks (bell tower, lighthouse, town hall and clock tower, fountain, jetty).
  3. Rebuild the houses as rows by district, with per-district wall and roof material swaps.
  4. Stairs, collision ramps and a nav test (bots running every chore loop).
  5. Windmill, waterwheel and harbour light props.
  6. Zone dressing per section 10.
