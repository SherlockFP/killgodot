# Morrowmere v2: "The town that grew down to the water"

**Lens:** organic coastal town planning (Mousehole, Port Isaac, Reine, Ine no Funaya).
**Diagram:** `Docs/Level/v2_organic.png` (top-down, north/inland at the top, sea at the bottom, 10 m grid, UE metres).
**Frame:** same as `Tools/Level/morrowmere_layout.json`: metres, x east, +y toward the sea, UE cm = m x 100.
**Status:** master plan. Every coordinate below was checked by script: no building overlaps another building, a lane, the stream or the water, and every size is a kit roof size.

> **Özet (TR):** Şu anki harita düz bir çayıra serpiştirilmiş kutulardan oluşuyor. Evler sokak oluşturmuyor, meydanın etrafı boş, deniz kenarı düz bir kum şeridi, yükselti yok, su köyün içine girmiyor. v2'de köy denize doğru inen üç teras üzerine kuruluyor: rıhtım (z 2), çarşı/meydan (z 6), kilise tepesi (z 11). Evler omuz omuza dizilip sokak duvarları oluşturuyor. Rıhtım boyunca denize bakan kesintisiz bir ev sırası (Harbour Row) var. Kavşaktaki meydanı dört yandan binalar çeviriyor. Köyün içinden bir dere akıyor ve üzerinde köprüler var. Evlerin arasında merdivenli dar geçitler (ope) var. Kilise tepede. Bahçe köşkünden bakınca adadaki torii ve pagoda tam karşıda görünüyor. Oyuncu evleri 20, hepsi korunuyor.

---

## 0. The big ideas (read this if nothing else)

1. **Three terraces falling to a harbour.** The quay and Harbour Row sit at z 2, the Market Square and Ropewalk at z 6, Kirk Hill with the church at z 11, and the lighthouse headland at z 15. Every view in town has a foreground (a street), a midground (roofs stepping down) and a background (the harbour, the island, the hills). v1 is one flat 4 m plateau, which is why nothing reads.
2. **Streets are made by houses, not drawn on grass.** Buildings stand shoulder to shoulder along a lane, fronts on the lane edge (0-0.8 m setback), and they rotate with the curve. There are 62 buildings (20 homes, 8 civic, 29 cheap "infill shells", 5 special). Street-wall frontage rises from **13% to 41%**, and building coverage of the town core from **6% to 24%**.
3. **One heart.** The Market Square (about 27 x 20 m, z 6) sits at the crossroads of the High Street (running down to the sea) and the Ropewalk (along the contour). All four sides are closed by facades: Town Hall on raised steps, the inn, the bakery, the guild house and the High Street corners. In the middle stand the Dead Tree and the well. The gallows stage stands on the lower edge, with the harbour behind it.
4. **A curved harbour, not a straight beach.** A stone quay wall bends around a basin. A stone mole curls out from the east headland, and a rocky skerry encloses the west. The pier runs out from Quay Head. **Harbour Row** is a continuous terrace of houses facing the water. This postcard view from the sea (Mousehole) is what sells the map.
5. **Water threads through the town.** The Morrow Beck runs from a mill pond on the wooded hills, past the smithy's waterwheel and under the Beck Bridge, into the harbour at the slipway. It has 3 bridges and a ford. A spring feeds the koi garden, and its overflow falls down the cliff into the harbour.
6. **Stepped alleys ("opes").** Three 2 m stepped passages cut through the rows (Net-loft Ope, Cat's Ope, Tanner's Ope). They make blocks of 20-30 m, many corners, shortcuts and dark gaps. This is ideal for a murder game.
7. **Designed sightlines and landmarks on the edges.** The bell tower closes the view up the Kirk Steps. The pier flag sits at the end of the High Street's deflected vista. Quay Head looks across to the torii and the island, and the garden pavilion stands on the exact island axis (x = 45). The lighthouse is on the headland and the windmill on the farm knoll. You can steer by the skyline from anywhere.

---

## 1. Diagnosis: why v1 reads as "things placed side by side"

Measured from `morrowmere_layout.json`, `kg_build_terrain.py`, `kg_build_village.py` and the captures `KG_Cap_top.png`, `KG_Cap_aerial.png`, `KG_Cap_plaza.png`, `KG_Cap_plan_organic_1.png` (top at 90 m) and `KG_Cap_plan_organic_2.png` (from the sea).

| # | Symptom | Evidence (coordinates in m) |
|---|---|---|
| 1 | **Buildings are free-standing boxes in grass.** Nothing touches, so no street wall forms. | 16 homes and 6 landmarks cover **940 m², 6% of the 130 x 115 m core** (real village cores are 35-50%). Nearest-neighbour gaps are 3-30 m (median about 5 m). Only 13% of lane edges have a building within 2 m. In `plan_organic_1` every house has a lawn all the way round it. |
| 2 | **Each house has its own random rotation.** It "looks at" a point instead of aligning to a street. | Yaws from `face`: H0 149°, H1 77°, H5 162°, H7 167°, H11 202°, H12 225°, H13 104°, H14 207°, church 153°, inn 207°, mill_barn 225°. There are 16 different angles, and even neighbours such as H1 (-9,26) and H4 (-10,38) differ by 13°. |
| 3 | **Houses do not front the lanes.** Some even sit on them. | H8 (-30,6) is 9.2 m from any lane, faces (-30,-8) 14 m away, and sits across open grass. H0 (-14,4), H10 (32,6), H13 (50,16) and H14 (18,-28) **overlap** lane corridors. H11 (30,-16) overlaps the inn. The boathouse (-12,48) is 5.7 m off the harbour street. |
| 4 | **The square is a void.** | The `square` area is a 13 m circle of dirt at (0,-8). Facades close only 43% of its perimeter, and the gaps are 10-20 m wide (`KG_Cap_plaza.png`: 4 gable ends far back, benches floating in the middle). The Town Hall front is 19 m from the centre. |
| 5 | **Spoke-and-hub street plan.** | Five lanes radiate from (0,-8): harbour_street, west_lane, east_market, church_lane and farm_track. Only `back_alley` and `fish_alley` cross-link them. Every district is a dead-end spoke with a building dropped at its tip (smithy -38,4; church -28,-48; farm 40,-44; lighthouse 70,57). The map reads like a diagram. |
| 6 | **It is flat.** | `PLATEAU = 4.0`, and lanes and areas are flattened toward it. The "church knoll" is a **2.5 m** bump. The lighthouse hill is 9 m. There are no terraces, stairs, retaining walls or views over roofs. The church, the most important building, sits 2.5 m above a pub. |
| 7 | **A straight beach, no harbour.** | The coast is `smooth(-34,-70,y)`, a straight east-west line at UE y ≈ 34-70. The pier is one stick off a sand strip. The fish market is a circle on sand at (4,46). No house faces the water, and in `KG_Cap_plan_organic_2` the village sits behind a 20 m band of beach and trees. |
| 8 | **No water inside the town.** | The only fresh water is a 7.6 x 4.6 m koi plane. There is no stream, bridge, pond or harbour basin. |
| 9 | **Themed pieces are islands.** | The Japanese garden is a 11 m circle at (26,20) between houses, with no wall or transition. The torii and pagoda island is at (45,150), but no street or window frames it. Pirate pier, European houses and Japanese garden sit side by side without a reason. |
| 10 | **Random scatter inside the village.** | `nature_layout()` scatters trees and rocks in a ring of radius 15-140 m around the plaza, filtered only by `is_clear`. Trees and 3 m boulders therefore land in every gap between houses (`plan_organic_1`), which reinforces the "sprinkled" look. |
| 11 | **Only 16 homes for up to 20 players.** | `houses` has 16 entries. Docs/04 requires 20 player homes. |

**Root cause:** v1 places *objects* (houses, landmarks, areas) and then connects them with lanes. Real towns place *streets* first, following the ground (contours to the harbour), and houses then **fill the street frontage**. v2 is built in that order: terrain, then water, then streets, then frontage, then landmarks at the ends of views.

---

## 2. Topography and water

### 2.1 Terraces (see contours on the diagram)

| Level | z (m) | What sits there | Transition to the next level |
|---|---|---|---|
| Harbour basin | -2 to -4 | Water, moored boats, pier | Stone quay wall, 2 m. Steps down at Quay Head, slipway at the boathouse. |
| **Quay / Strand** | 2.0 | Strand promenade, Quay Head, Harbour Row, fish market hall, boathouse, east-quay net lofts | 2 m bank behind Harbour Row (y 21-27), taken up by walled back gardens and the three opes |
| **Middle town** | 4 (Cooper's Lane) to 6 (square) | Cooper's Lane, High Street, Ropewalk, **Market Square** | Kirk Hill scarp y -26 to -44 (+5 m): terraced orchard with dry-stone walls, Kirk Steps |
| Garden Terrace | 8 | Koi garden, pavilion at the lip, allotments | Headland ridge rises to 15 |
| Upland Farm | 9-10 (knoll 12) | Farm yard, barn, walled fields, windmill knoll | Gentle; Farm Road rises at 7% |
| **Kirk Hill** | 11-12 | Church, bell tower, graveyard, parsonage, Hill Row | Wooded hills beyond, 14-28 |
| **East Headland** | 15 | Lighthouse, keeper's cottage | Sea cliffs on the south and east (sea caves, future); cliff face above the east quay |
| Beckside | 2 (beach) to 9 (mill pond) | Stream valley, Mill Lane, smithy, watermill, woodcutter | Rises west at 16% into the woods |

The terrain function used for the diagram is in `plan_v2.py` (the scratch generator). It is written for a drop-in replacement of `height()` in `kg_build_terrain.py`:

```python
z = 2 + 2*smooth(30,20,y) + 2*smooth(12,-3,y) + 5*smooth(-26,-44,y) + 3*smooth(-62,-100,y)   # 3 terraces + back hills
z += 2.5*smooth(24,40,x)*smooth(40,20,y)                                                      # east flank / garden terrace
# headland ridge: ellipse centred (72,56), long axis toward the garden (NW), top z 16, cliffs to the sea
z -= 1.5*smooth(20,36,x)*smooth(-30,-50,y); z += 3*gauss((74,-78), r~13)                       # farm plateau + windmill knoll
z += 0.16*max(0,-x-58) + 3*smooth(-60,-90,x)*smooth(30,0,y)                                     # Beckside rising west
beck ravine: -2 m within 1.5-7 m of the beck line; sea: outside the shoreline polygon, -1 to -7 m
```

**Building pads:** each building gets a flat pad at its **front-door street level** (the lane z at its frontage point), not at its centre as v1 does. On the downhill side the kit's `Wall_UnevenBrick_Straight` pieces form a plinth or retaining skirt of 0.5-2 m. That plinth is what gives the town its Cornish stepped silhouette. Harbour Row houses are "split-level" in spirit: front door on the quay, back garden 2 m higher, reached from Cooper's Lane.

### 2.2 Shoreline, harbour, sea

- **Quay wall** (top z 2.0, 2 m stone face) follows (-36,44) (-24,43) (-12,42) (0,41.5) (12,42.5) (22,45.5) (29,51) (33,59) (35,68) (38,74).
- **West shore** (natural shingle beach) runs (-36,44) (-42,46) (-50,49) (-62,53) (-76,56) (-92,58) (-110,60).
- **East headland cliffs** run (38,74) (46,79) (60,83) (76,82) (90,76) (110,70). The sea caves under the lighthouse are future content.
- **Stone mole** (walkable, 5 m wide, top z 2.5): (38,74) (31,81) (19,86.5) (7,88.5), with a harbour beacon lantern at the tip (7,88.5).
- **Skerry** (rocks, climbable, not a path): (-50,50) (-45,58) (-38,64) (-33,66). With the mole it gives the basin a real mouth between (-33,66) and (7,88).
- **Pier** (pirate kit, 6 sections of about 29 m): from Quay Head (2,41.5) to (2,70). Rowboats moor on both sides, and a larger fishing boat (future) moors along the east quay.
- **Shrine island** stays at (45,150), with the **torii in the shallows at (42,119)**. Both sit on the axis x ≈ 42-45 seen from Quay Head and the garden pavilion.
- Wave amplitude inside the basin should be scaled down with a mask polygon bounded by the quay, mole and skerry, so the harbour reads as calm against the rough open sea.

### 2.3 The Morrow Beck (new)

- **Course:** (-100,-95) (-84,-76) (-70,-60) (-62,-47) **mill pond (-64,-50), r 5.5** (-56,-32) (-51,-16) (-47,-4) (-44,8) (-42,20) (-40,32) (-38,40), then the **mouth at (-37,46)** beside the boathouse slip.
- 3 m wide, 0.4-0.6 m deep: players can wade at 60% speed; bots use the crossings. There is a small weir and cascade at Beck Bridge and a 1.5 m drop at the Kirk Hill scarp. It uses the existing `M_KG_PondWater` material on a ribbon mesh made in Blender.
- **Crossings:** Beck Bridge, a stone arch at (-47.5,-4.5) on the Ropewalk. The Quay footbridge at (-38.5,41.5) on the Beach Walk. The Mill footbridge at (-58.5,-38) at the Hill Row / Woods Path. Ford Stones at (-41.2,27.3).
- **Koi garden spring:** the pond at (45,11), r 3.2, overflows in a rill under the pavilion and falls down the cliff into the harbour. You can see it from the quay.

---

## 3. Districts

| District | Character (reference) | Homes | Anchor | Gameplay role |
|---|---|---|---|---|
| **Harbour Row** (quay, z 2) | Mousehole / Ine: continuous terrace facing the water, net lofts against the cliff, fish hall on the quay | H1-H5 | Pier, fish market hall, boathouse | Busiest, noisy, many witnesses. Opes offer escape routes. |
| **Market Town** (z 4-6) | High Street, Cooper's Lane, Ropewalk, **Market Square** | H6-H16 (11) | Town Hall, Dead Tree, inn | Social centre, meetings, dense corners, back gardens |
| **Kirk Hill** (z 11) | Church on the high ground over the roofs, graveyard wall, parsonage | H17-H18 | Church + bell tower (top z ≈ 23) | Quiet and exposed. Catacombs later. The bell chore is up a ladder. |
| **Beckside** (z 2-9) | Stream valley, mill, smithy, working yards, woods edge | H19 | Smithy waterwheel, watermill | Loud (forge, water) and isolated. Bridges are chokepoints. |
| **Garden Terrace** (z 8) | Walled Japanese garden, tea house, allotments | none | Pavilion on the island axis | Calm; framed view; small hide spots |
| **East Headland** (z 15) | Cliff path, keeper's cottage | none | Lighthouse (top z ≈ 30) | Long, lonely trip (backstab territory). Two ways up form a loop. |
| **Upland Farm** (z 9-10) | Walled fields, barn, windmill knoll | H20 | Windmill | Open sightlines, hard to hide, far from help |

Homes: **20 player homes** (H1-H20). Distribution: 5 on the quay, 11 in the middle town, 2 on the hill, 1 in Beckside, 1 on the farm. For player counts below 20, assign homes nearest the square first: H6-H16, then H1-H5, then H17-H20. Unassigned homes become "empty houses" with loot, as Docs/04 already plans.

---

## 4. The town heart: Market Square (z 6)

Polygon: (-13,-5.5) (-5.5,-3.5) (10,-2.5) (14.5,-4.5) (15,-10) (14,-23) (-9,-24) (-12.5,-19). That is about 27 x 20 m (about 480 m²): big enough for 20 players, small enough to feel enclosed. Paving is cobble with a darker ring around the Dead Tree.

- **North side:** **Town Hall C1** (8x10, front at y -24.5, face S) on a **1.2 m podium with 6 full-width steps**. The steps are the meeting "gallery" where players sit and stand. A small clock cupola (new Blender prop) makes it the square's focal point. Beside it is the **Guild house** I27 (6x6, face S). The **notice board D** stands at (-6,-22) at the foot of the steps.
- **West side:** the **Bakery** C3 (6x6, face E) at (-15.5,-17.5), with a chimney smoking. The **Kirk Steps** leave from the north-west corner (-9,-23) and climb toward the bell tower.
- **East side:** **The Late Arrival** inn C2 (8x8, face W) at (19.5,-13), with its beer-garden gap to Garden Row. The **Farm Road** leaves from the north-east corner (13,-22).
- **South side:** the High Street corner houses H13, I6 (Apothecary) and I8 (Post office). The High Street drops away at (2,-4), so the square's south edge opens onto the roofs and the harbour below.
- **Centre:** the **Dead Tree B** (3.5,-15.5), the lore centrepiece from Docs/04, is the one point every lane looks at. The **well A** (-7,-15) has its future cellar hatch.
- **Gallows stage C** (-3,-8.5): a 4 x 4 m timber platform on the lower edge, facing north to the Town Hall steps. The accused stands with the **sea and the island behind them**, which makes trials dramatic by composition. Tag `KG_Gallows`.
- **Market stalls E** (7.5,-8.5) sit along the south-east edge (chore: stock the stall).
- **Spawn:** 12 PlayerStarts on a ring of radius 8 around (1,-13), facing the Dead Tree.

---

## 5. Street network

All lanes are drawn at true width on the diagram. Surfaces: cobble for main streets, packed earth with cobble edging for lanes, stone flags for stairs, dirt for paths.

| Lane | Width | Type | z profile | Polyline (m) |
|---|---|---|---|---|
| **The Strand** (quay promenade) | 7.0 | main, flags | 2.0 flat | (-36.3,40.4) (0.3,37.9) (12.7,39.0) (23.4,42.2) (32.0,49.0) (36.4,57.8) (38.5,67.2) |
| **High Street** | 4.5 | main, cobble, S-curve | 2 to 6 (10%) | (2,34) (0,26) (-1,18) (1,10) (3,2) (2,-4) |
| **Cooper's Lane** | 3.2 | contour lane | 4.0 to 3.5, then down to 2 at the east end | (-40,20) (-28,19) (-15,18.5) (-1,18) (12,19) (20,22.5) (25.8,28) (27.9,36.5) |
| **Ropewalk** | 3.6 | contour lane | 6 to 6.5 | (-12,-8) (-24,-7) (-36,-5) (-44,-4) (-51,-5) (-56,-7) |
| **Garden Row** | 3.6 | lane | 6 to 8 | (14,-4) (24,-2) (34,0) (41,4) (44,8) |
| **Kirk Steps** | 3.0 | stair, 3 landings | 6 to 11 | (-9,-23) (-13,-31) (-17,-38) (-20,-43) |
| **Hill Row** | 3.2 | contour lane | 10.5 to 11 to 10 | (-58,-38) (-46,-43) (-32,-46) (-20,-47.5) (-8,-46) (6,-44) (16,-40) (22,-36) |
| **Farm Road** | 4.0 | main, cart | 6 to 9.5 (7%) | (13,-22) (18,-29) (22,-36) (30,-44) (38,-52) (45,-58) |
| **Beck Lane** | 3.0 | lane along the beck | 2 to 6 | (-33,37) (-37,30) (-39,20) (-41,8) (-44,-4) |
| **Mill Lane** | 3.0 | lane (west bank) | 6.5 to 2 | (-51,-5) (-55,6) (-57,18) (-56,30) (-51,40) (-45,45) |
| **Beach Walk** | 3.0 | path, shingle | 2 to 1 | (-34,39) (-39,42) (-45,45) (-60,48.5) (-78,51) |
| **Lighthouse Path** | 3.0 | path, ridge | 8 to 15 (12%) | (44,8) (51,17) (57,28) (63,40) (69,51) (72,56.5) |
| **Gull Steps** | 2.5 | stair up the cliff, 2 landings | 2 to 13 | (41,63) (46,58) (50,52) (56,47) (62,41) |
| **Timber Track** | 2.5 | path | 6.5 to 9 | (-56,-7) (-64,-12) (-72,-18) (-76,-24) |
| **Woods Path** | 2.5 | path | 10.5 to 9 | (-58,-38) (-66,-32) (-75,-26) |
| **Ford Stones** | 2.0 | path + stepping stones | 3.5 | (-38,27) (-44,27.5) (-55.5,28) |
| **Net-loft Ope** | 2.0 | stepped alley | 4 to 2 | (-15,20) (-15.5,34) |
| **Cat's Ope** | 2.0 | stepped alley | 4 to 2 | (16.5,20.5) (17.3,34.5) |
| **Tanner's Ope** | 2.0 | stepped alley, kinked | 6 to 4 | (-27,-4.5) (-26.1,3) (-27.9,9.5) (-29,17.8) |

**Areas / yards:** Market Square (above). **Quay Head** (3,37) r 8.5, where the High Street meets the quay and the pier. **Church Parvis** (-20,-48) r 6.5. **Smithy Yard** (-59,-9) r 6. **Farm Yard** (47,-60) r 8. **Woodcutter's Clearing** (-77,-25) r 7. **Lighthouse Knoll** (74,60) r 7. **Koi Garden** (45,12) r 10, walled with 2 gates. **Boat Slip** (-31..-24, 38.5..46).

**Network properties.** The network is fully connected, with no dead-end district. Every district has **two ways in or out**. The loops are:

- quay, then Beck Lane, Ropewalk, square, High Street, back to the quay
- square, Kirk Steps, Hill Row, Farm Road, back to the square
- Hill Row, Mill footbridge, Woods Path, woodcutter, Timber Track, smithy, Beck Bridge
- Garden Row, Lighthouse Path, lighthouse, Gull Steps, east quay, Strand

Stairs are NavMesh-walkable. Author them as step meshes with a **simple ramp collision** (max 22°) so bots and physics bodies do not snag.

**Walk times from the square** (network shortest path at 5.8 m/s; Docs/04 caps this at 35 s):

| Destination | Time |
|---|---|
| Church | 8 s |
| Quay Head | 9 s |
| Koi garden | 9 s |
| Graveyard | 10 s |
| Smithy | 11 s |
| Farm | 11 s |
| Pier end | about 14 s |
| Boathouse | 14 s |
| Watermill | 14 s |
| Woodcutter | 15 s |
| Lighthouse | 19 s |
| West beach end | 22 s |

---

## 6. Buildings (all 62)

The `face` angle is the direction the front door looks: UE yaw convention, 0 = east (+x), 90 = toward the sea (+y). **UE actor yaw = face + 90** (the kit house front is local -Y, the same as `facing_yaw()` in v1). Size is frontage x depth (m). Houses along a lane were generated by a **frontage runner**: march along the lane, place each house against the lane edge plus its setback, and align it to the local tangent. Port that runner into the builder (§10) rather than hand-copying these numbers.

**Kinds:**
- **H** = player home (full interior, KGDoor, doorbell, chest).
- **C** = civic (interior).
- **I** = infill shell: exterior only, locked dummy door, no interior and no lights except the occasional wall lantern. Infill shells are what make the street walls, and they are cheap.
- **S** = special structure.

### Harbour Row (Strand, fronts to the water)
| id | name | centre | size | face |
|---|---|---|---|---|
| I1 | Boat-builder's loft | (-29.7, 33.0) | 6x6 | 85 (S) |
| H1 | Home 1 | (-23.8, 31.2) | 6x8 | 85 (S) |
| H2 | Home 2 | (-18.7, 32.2) | 4x6 | 85 (S) |
| *Net-loft Ope* | | x ≈ -15.5 | 2 m | |
| H3 | Home 3 | (-10.9, 31.0) | 6x6 | 88 (S) |
| I2 | Harbourmaster | (-5.9, 31.3) | 4x6 | 88 (S) |
| *Quay Head* | | x -3..+6 | | |
| C5 | Fish Market Hall (open arcade, custom posts + 2 roofs) | (10.8, 32.1) | 9x6 | 95 (S) |
| *Cat's Ope* | | x ≈ 17 | 2 m | |
| H4 | Home 4 | (22.0, 34.5) | 6x6 | 107 (SSW) |
| *Cooper's Lane mouth* | | x ≈ 27 | | |
| H5 | Home 5 | (32.8, 38.8) | 4x8 | 128 (SW) |
| I3 | Net loft (against the cliff) | (39.1, 47.9) | 6x6 | 153 (WSW) |
| I4 | Salting cellar | (42.7, 52.8) | 6x8 | 153 (WSW) |
| C6 | Boathouse + slipway (door on the Strand, slip into the basin) | (-27.5, 43.5) | 6x8 | 270 (N) |

### High Street
| id | name | centre | size | face |
|---|---|---|---|---|
| I5 | Cobbler | (-5.7, 24.6) | 4x6 | 353 (E) |
| H6 | Home 6 | (-6.3, 12.1) | 6x8 | 14 (E) |
| H7 | Home 7 | (-3.5, 6.5) | 6x6 | 14 (E) |
| I6 | Apothecary (square corner) | (-2.6, 1.6) | 4x6 | 14 (E) |
| I7 | Tailor | (5.3, 23.3) | 4x6 | 173 (W) |
| H8 | Home 8 | (5.7, 13.5) | 6x6 | 194 (W) |
| H9 | Home 9 | (8.5, 8.0) | 6x8 | 194 (W) |
| I8 | Post office (square corner) | (8.3, 1.2) | 4x6 | 176 (W) |

### Cooper's Lane (north side, fronts face the sea)
| id | name | centre | size | face |
|---|---|---|---|---|
| H10 | Home 10 | (-33.9, 13.5) | 6x8 | 85 (S) |
| *Tanner's Ope* | | x ≈ -29 | 2 m | |
| H11 | Home 11 | (-24.4, 14.1) | 6x6 | 88 (S) |
| I9 | Weaver | (-18.5, 12.5) | 6x8 | 88 (S) |
| I10 | Chapel of rest | (-13.4, 13.6) | 4x6 | 88 (S) |
| H12 | Home 12 | (15.2, 15.1) | 6x6 | 114 (SSW) |
| I11 | Cooperage | (21.2, 16.5) | 6x8 | 114 (SSW) |

### Ropewalk
| id | name | centre | size | face |
|---|---|---|---|---|
| H13 | Home 13 (south side) | (-16.0, -1.5) | 6x8 | 265 (N) |
| I12 | Sailmaker | (-22.1, -2.2) | 6x6 | 265 (N) |
| H14 | Home 14 | (-30.9, 0.5) | 6x8 | 261 (N) |
| I13 | Rope store | (-36.3, 0.1) | 4x6 | 263 (N) |
| I14 | Candle-maker (north side, faces the view) | (-24.0, -12.1) | 6x6 | 84 (S) |
| H15 | Home 15 | (-30.4, -12.2) | 6x8 | 81 (S) |
| I15 | Store | (-36.2, -10.6) | 6x6 | 81 (S) |
| I16 | Toll house (by Beck Bridge) | (-41.7, -9.3) | 4x6 | 83 (S) |

### Square, Garden Row
| id | name | centre | size | face |
|---|---|---|---|---|
| C1 | **Town Hall** (meeting hall, podium steps) | (0.5, -29.5) | 8x10 | 90 (S) |
| I27 | Guild house | (9.5, -28.5) | 6x6 | 90 (S) |
| C2 | **The Late Arrival** inn | (19.5, -13.0) | 8x8 | 180 (W) |
| C3 | Bakery | (-15.5, -17.5) | 6x6 | 5 (E) |
| H16 | Home 16 (Garden Row) | (28.9, -7.2) | 6x8 | 101 (S) |
| I17 | Painter | (35.1, -5.2) | 6x6 | 107 (SSW) |
| I18 | Grocer | (16.4, 1.7) | 6x6 | 281 (N) |
| I19 | Tea house | (22.1, 4.1) | 6x8 | 281 (N) |
| S7 | Garden pavilion (open, custom roof) on the island axis | (45.0, 20.5) | 6x4 | 90 (S) |

### Kirk Hill
| id | name | centre | size | face |
|---|---|---|---|---|
| C7 | **Church** (door to the parvis and the sea) | (-22.0, -60.0) | 8x12 | 90 (S) |
| S1 | Bell tower, 4 storeys, ladder (touching the church's east side) | (-15.5, -56.0) | 4x4 | 90 (S) |
| S5 | Mausoleum = catacomb stair (future) | (-38.0, -66.0) | 4x4 | 90 (S) |
| I20 | Parsonage | (-6.8, -50.8) | 6x6 | 98 (S) |
| H17 | Home 17 | (-0.7, -51.2) | 6x8 | 98 (S) |
| H18 | Home 18 | (5.0, -49.0) | 6x6 | 98 (S) |
| I21 | Schoolhouse | (11.0, -47.4) | 4x6 | 112 (SSW) |
| I22 | Sexton's house (south side, backs onto the Town Hall) | (-6.7, -40.9) | 6x6 | 278 (N) |
| I23 | Almshouse | (4.2, -39.4) | 4x6 | 278 (N) |

The graveyard is a walled polygon (-44,-50) (-29,-51) (-28,-70) (-45,-69), with its gate on Hill Row at about (-34,-48).

### Beckside
| id | name | centre | size | face |
|---|---|---|---|---|
| H19 | Home 19 | (-61.4, 8.3) | 6x8 | 9 (E) |
| I24 | Dyer's shed | (-61.1, 13.4) | 4x6 | 9 (E) |
| I25 | Tannery | (-61.3, 25.5) | 6x6 | 355 (E) |
| I26 | Fuller's cottage (between the lane and the beck) | (-52.1, 17.9) | 6x6 | 189 (W) |
| S4 | **Smithy**: open forge shed + waterwheel on the beck | (-57.5, -17.0) | 4x6 | 100 (S) |
| I29 | Watermill (mill race from the pond) | (-73.0, -47.0) | 6x6 | 20 (E) |

### Farm and Headland
| id | name | centre | size | face |
|---|---|---|---|---|
| H20 | Home 20, Farmhouse | (38.0, -66.0) | 6x8 | 20 (E) |
| C8 | Barn | (57.0, -62.0) | 8x8 | 180 (W) |
| S6 | Windmill (new Blender prop, knoll z 12) | (74.0, -78.0) | 5x5 | 200 (W) |
| S2 | **Lighthouse**, 5 storeys, ladder | (75.0, 61.0) | 4x4 | 210 (WNW) |
| I28 | Keeper's cottage | (67.0, 58.0) | 4x6 | 150 (WSW) |

**Variety rules for street walls.** The builder must vary these so the rows do not look stamped:
- **Roof direction:** alternate ridge parallel to the street (4x6, 6x6) and gable-to-street (6x8, 4x8). Put gable fronts on corners and at view ends.
- **Setback:** 0-0.8 m, as given in the tables.
- **Plinth height:** from the slope.
- **Wall style:** keep v1's `style % 2` brick or plaster, and add a 3rd colour wash on Harbour Row (Ine/Reine: whites, ochres, reds).
- **Chimney side.**

Neighbours share a party wall, so the builder should skip the duplicate wall piece where two footprints touch.

---

## 7. Landmarks, skyline and sightlines

**Skyline icons.** At least one of these is visible from every street:

| Icon | Location |
|---|---|
| Bell tower | Kirk Hill (N), top ≈ z 23 |
| Lighthouse | Headland (SE), top ≈ z 30, beacon light |
| Windmill | Farm knoll (NE) |
| Pagoda | Island (far S) |
| Town Hall cupola | Heart |

**Designed sightlines** (pink arrows on the diagram):

1. **Square, looking down the High Street.** The High Street S-curve hides and reveals the harbour. The view ends on the **pier flag** and the boats. This is a deflected vista: the street wall closes the frame and pulls you downhill.
2. **Quay Head, looking across the basin.** You see the mole curving from the right, then the **torii** at (42,119), then the **island pagoda**. This is the arrival postcard.
3. **Garden pavilion, looking at the torii and pagoda.** All three sit on x ≈ 45, a formal axis across the water that connects the Japanese garden to the island. This is the reason the Japanese pieces belong here.
4. **Kirk Steps, looking at the bell tower.** The stepped alley aims at the tower. You climb *toward* the landmark.
5. **Beach Walk, looking at the lighthouse.** Across the whole harbour mouth.
6. Bonus: from the church parvis (z 11) you look over two tiers of roofs to the sea. It is the best view in town and a Watcher/Gözcü role perch.

**Landmark placement rule:** landmarks stand at the **ends of streets, tops of stairs and edges of water**, never in the middle of a field.

---

## 8. Chores

The 18 v1 chores are kept (the same ids, so gameplay code does not change) and 2 are new. They are spread so that every district has at least one reason to go there, and so that the far chores make long, lonely trips.

| # | id | where | position (m) | district |
|---|---|---|---|---|
| 1 | DrawWater | well A | (-5.0, -13.0) | square |
| 2 | PostNotice | notice board | (-3.8, -20.6) | square |
| 3 | FileReports | Town Hall steps (door-side desk) | (0.5, -23.0) | square |
| 4 | RingBell | bell tower lookout (ladder) | (-15.5, -56.0) | Kirk Hill |
| 5 | LightCandles | church porch | (-22.0, -53.0) | Kirk Hill |
| 6 | TendGraves | between grave rows | (-36.0, -60.0) | Kirk Hill |
| 7 | ForgeNails | smithy anvil | (-58.0, -11.0) | Beckside |
| 8 | SharpenTools | grindstone on the waterwheel | (-53.0, -13.0) | Beckside |
| 9 | BakeBread | bakery door oven | (-11.5, -16.5) | square |
| 10 | PourAle | inn door, ale cask | (15.0, -12.0) | square |
| 11 | StockStall | market stalls E | (10.5, -6.0) | square |
| 12 | MendNets | net racks on the Strand | (-9.5, 36.5) | harbour |
| 13 | UnloadFish | pier end | (2.0, 66.0) | harbour |
| 14 | FuelLighthouse | lighthouse lamp (ladder) | (75.0, 61.0) | headland |
| 15 | HarvestCarrots | walled field | (40.0, -80.0) | farm |
| 16 | FeedAnimals | trough by the barn | (52.0, -57.0) | farm |
| 17 | ChopWood | woodcutter's clearing | (-77.0, -25.0) | Beckside woods |
| 18 | FixBoat | boathouse slipway | (-27.5, 47.0) | harbour |
| 19 | **FeedKoi** (new) | koi pond edge | (45.0, 14.5) | Garden Terrace |
| 20 | **OilMill** (new) | watermill pond | (-64.0, -45.0) | Beckside |

That is 6 in the square, 3 in the harbour, 3 on Kirk Hill, 5 in Beckside and the woods, 2 on the farm, 1 on the headland and 1 in the garden. All are outside buildings or at doors (v1's `task_spot` rule: bots cannot open doors yet). The two tower chores stay on the ladder lookouts.

---

## 9. Points of interest and future hooks

- **Well cellar** (A): the hatch in the well shaft leads to the smuggler tunnels (Docs/04 underground network).
- **Catacombs** under the mausoleum S5. Their tunnel runs south under Kirk Hill toward the Town Hall cells.
- **Sea caves** under the headland cliffs (60-80, 78-84), reached at low tide from the mole's outer rocks.
- **Mill race**: the watermill's culvert is a crawl route from the pond to Beckside (a hiding place).
- **Opes at night:** unlit on purpose (one lantern at each end). They are the murder corridors.
- **Harbourmaster's roof, the church parvis and the headland** are high perches with long sightlines for Watcher-type roles.
- **Back gardens** behind every row (walled, with sheds, washing lines, water butts) give semi-private hiding space, and each has a garden gate onto an ope or lane.

---

## 10. Set-dressing zones

The zones below partition the map (dashed purple outlines on the diagram). `kg_dress_common.zone_of()` should switch from circles to these polygons so zones never overlap.

| Zone module | Polygon (m) | Content brief |
|---|---|---|
| `dress_harbour` | (-40,27) (30,27) (48,44) (48,76) (-40,76) | Quay wall bollards and rings, net racks, lobster pots, fish crates, harbour steps, boats, ropes, gull perches, fish hall stalls, the mole beacon |
| `dress_streets` | (-40,-4) (-13,-4) (15,-2) (30,4) (30,27) (-40,27) | Shop signs, window boxes, washing lines across the opes, barrels, benches by doors, **back-garden walls, sheds, water butts, vegetable beds** |
| `dress_square` | (-20,-35) (26,-35) (26,2) (15,-2) (-13,-4) (-20,-4) | Cobbles, Dead Tree, well, gallows stage, stalls, bunting, notice board, Town Hall podium and cupola, lamp posts |
| `dress_church` (Kirk Hill) | (-54,-35) (26,-35) (26,-40) (14,-76) (-54,-76) | Graveyard, walls, lychgate, yews, orchard terraces, benches with a view |
| `dress_japan` (Garden Terrace + island) | (26,-8) (58,-8) (58,27) (30,27) (30,4) (26,2), plus the island | Koi pond, bridge, lanterns, sakura, bamboo fence wall, tea house props, allotment beds, the spring rill and waterfall |
| **`dress_beckside` (new)** | (-66,-35) (-54,-35) (-40,-4) (-40,44) (-66,44) | Beck banks, reeds, stepping stones, waterwheel, tannery vats, dye racks, woodpiles, footbridges |
| `dress_countryside` | NE: (26,-8) (100,-8) (100,-100) (14,-100) (14,-76) (26,-40) | Dry-stone field walls, crops, hay, carts, animals, windmill |
| `dress_coast` | E: (58,-8) (100,-8) (100,86) (48,86) (48,27) (58,27) | Headland heather, cliff fence, keeper's garden, cannon battery (Docs/04), cliff rocks |
| `dress_wilds` | W and N remainder | Forest, woodcutter, mill pond, hunters' paths |

Trees are no longer scattered inside the town. The `nature_layout` exclusion must use building polygons, lane buffers and garden polygons. Inside the town, trees appear only as **designed** single trees: in back gardens, on the parvis, the Dead Tree, the orchard terraces and the garden sakura.

---

## 11. Performance

- **Interiors:** 26 (20 homes, plus the Town Hall, inn, bakery, church, boathouse and barn). v1 has 22. The 29 infill shells have **no interior and no point lights**: they use emissive window cards at night.
- **Merging:** convert each kit building to a **Packed Level Actor**, or merge its pieces into HISM by mesh. There are about 60 buildings at about 80-110 pieces each. Build HLOD cells per district. The row layout also helps occlusion: streets are canyons, so most of the town is occluded from any street.
- **Lights:** homes keep 2 movable shadowless lights, and civic buildings keep 2-4. The lane lamps drop from v1's every-15 m on every lane to **one lamp per junction and ope end** (about 30 lamps). Target at most about 120 movable point lights in total, with small attenuation radii (at most 9 m).
- **Water:** the beck is one ribbon mesh with the pond material. The harbour uses the existing ocean with a calm-mask polygon.
- **NavMesh:** the bounds stay about 160 x 150 m plus the headland and farm. Stairs use ramp collision, and the 2 m opes allow 2 bots to pass (capsule radius 42 cm).

---

## 12. How to build it (order that shows progress each sprint)

1. **Terrain v2 + water** (`kg_build_terrain.py`): terraces, harbour basin, quay line, mole, beck ribbon, headland ridge and pads at front-door level. *Visible:* a top capture shows the cove and terraces.
2. **Layout json v2** (`morrowmere_layout_v2.json`), with these changes:
   - `lanes` gain `z` per point.
   - A new `rows` array holds the frontage runs: `{lane, side, start, specs:[kind,name,w,d,setback] | ["gap",m]}`.
   - Hand buildings use `face_deg` instead of a `face` point.
   - `water` holds the quay, mole, skerry, beck and ponds.
   - `gardens` holds the back-garden polygons.
   - `zones` holds the dressing polygons.
   - The frontage runner (about 20 lines in the scratch generator, `frontage()` and `run()`) goes into `kg_build_village.py`.
3. **Harbour Row + Strand + quay wall + pier + mole.** *Visible:* the postcard from the sea (`KG_Cap_harbour`).
4. **Market Square + High Street + civic.** *Visible:* `KG_Cap_plaza` shows enclosed facades and the Town Hall steps.
5. **Kirk Hill, Beckside, Garden Terrace, then the farm and headland.** Re-cut the dressing zones, move the chores, rebuild nav, run the bot walk test.

`build_house()` needs 3 small upgrades:
- a `plinth` height (a brick skirt down to the terrain on the downhill side),
- a `party_walls` flag (skip the side walls that touch a neighbour),
- a `shell=True` mode for infill (no interior, no lights, dummy door).

---

## 13. Risks

- **Kit houses on slopes.** Pads plus plinths are required. On steep spots (the Kirk Hill scarp, the headland) buildings are avoided on purpose. Verify the 2 m bank behind Harbour Row in-engine early.
- **Narrow opes and stairs with bots.** Ramp collision and 2 m clear width are needed. Test 20 bots at a meeting call: everyone converges through the High Street (4.5 m), the Kirk Steps and the Ropewalk. The square has 5 entrances, which should be enough.
- **Density versus combat readability.** The town is denser and more cornered, which is good for murders but can hurt witness gameplay. Keep the Strand (7 m) and the square as open, lit, high-traffic spaces.
- **Performance.** 62 buildings is 2.8x v1's 22. Merging (Packed Level Actors or HISM) is a precondition, not an optimisation.
- **Sightline dependencies.** The island axis needs fog density low enough to see about 120 m at day. At night the torii light and the lighthouse carry the view.
- **Rebuild cost.** This is a layout rewrite, not a patch. Zone dressing modules written against v1 coordinates (the circles in `zone_of`) must be re-targeted. Doing Harbour Row and the square first delivers most of the visible gain early.
- **Wading in the beck and the calm harbour** need small C++ or material tweaks: water volume speed and a wave mask.

---

*Generated with the scratch generator `plan_v2.py`. It holds the data, the validation (overlaps, lanes, water, stream, kit sizes) and the diagram. The metrics in the diagram legend are computed, not estimated.*
