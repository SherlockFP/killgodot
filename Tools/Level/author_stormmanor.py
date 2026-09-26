"""Authoring script for Tools/Level/stormmanor_layout.json (map 2, "Storm Manor" / "Firtinali Malikane").

    python Tools/Level/author_stormmanor.py        # writes stormmanor_layout.json
    python Tools/Level/validate_stormmanor.py      # must print PASS
    python Tools/Level/render_stormmanor.py        # writes Docs/Level/StormManor.png

Design doc: Docs/Level/StormManor_Plan.md (SPRINT-017, design only; SPRINT-018 builds it).
Edit here and re-run rather than hand-editing the JSON (hand edits are overwritten).

Frame: metres, UE axes (x east, +y south = toward the sea stair and the mainland; the UE top view shows +y down).
UE cm = m * 100. Every room corner sits on the 2 m kit grid (Quaternius village walls are 2 m wide, 3 m tall), so the
v2 builder's wall/floor/stair pieces tile every room without cuts. Floors are 3 m apart (FLOOR_H = 300 cm):
    C  (cellar, boathouse and the smugglers' tunnel at sea level)  z = -3
    F0 (ground floor + the grounds on the rock plateau)             z =  0
    F1 (guest floor, gallery around the Great Hall's void)          z =  3
    F2 (attic, clock room, roof walk, storm tower room)             z =  6
    F3 (storm tower top, open platform)                             z =  9
Rooms are polygons (axis-aligned, counter-clockwise not required). `counts` marks the named rooms of the brief (24);
circulation, outdoor and grounds areas are regions too but are listed separately.
"""
import json
import os
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "stormmanor_layout.json")

FLOORS = [
    {"id": "C", "name_tr": "Mahzen", "name_en": "Cellar", "z": -3.0},
    {"id": "F0", "name_tr": "Zemin Kat", "name_en": "Ground floor", "z": 0.0},
    {"id": "F1", "name_tr": "Birinci Kat", "name_en": "First floor", "z": 3.0},
    {"id": "F2", "name_tr": "Çatı Katı", "name_en": "Attic floor", "z": 6.0},
    {"id": "F3", "name_tr": "Kule Tepesi", "name_en": "Tower top", "z": 9.0},
    # SPRINT-040: the second basement (vaults, wine catacombs, ossuary, the crypt under the chapel)
    {"id": "C2", "name_tr": "Alt Mahzen", "name_en": "Lower vaults", "z": -6.0},
]
FLOORS.sort(key=lambda f: f["z"])


def rect(x0, x1, y0, y1):
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]


ROOMS = []


def room(rid, floor, poly, name_tr, name_en, kind="room", wing=None, risk="orta", min_n=6, surface="wood",
         counts=None, dead_end_ok=False, **kw):
    r = {"id": rid, "floor": floor, "poly": poly, "name_tr": name_tr, "name_en": name_en, "kind": kind,
         "counts": (kind == "room") if counts is None else counts, "wing": wing, "risk": risk, "min_n": min_n,
         "surface": surface, "dead_end_ok": dead_end_ok, "hides": [], "evidence": []}
    r.update(kw)
    ROOMS.append(r)
    return r


# ------------------------------------------------------------------------------------------------ rooms
# Ground floor (F0). Centre: the Great Hall (double height, gallery ring above) and the entrance hall.
room("great_hall", "F0", rect(-10, 10, -20, 0), "Büyük Salon", "Great Hall", wing="hall", risk="güvenli",
     surface="stone", storeys=2, void=rect(-6, 6, -16, 0),
     purpose="meeting room: long table, 20 chairs, hearth, chandelier; spawn ring",
     dressing="long oak table + 20 chairs, hearth on the north wall, chandelier on a rope from the clock room, "
              "twin grand stairs, Pozzo's banners, the empty chair at the head")
room("vestibule", "F0", rect(-10, 10, 0, 10), "Giriş Holü", "Entrance Hall", wing="hall", risk="güvenli",
     surface="stone", purpose="front door to the courtyard; post desk; cloak closet",
     dressing="chequered floor, coat racks, umbrella stand dripping, post desk with pigeonholes, storm lanterns")
room("dining", "F0", rect(-22, -10, -20, -6), "Yemek Salonu", "Dining Room", wing="west", surface="wood",
     purpose="formal dining for the committee", dressing="banquet table set for 13, candelabras, sideboard, portraits")
room("kitchen", "F0", rect(-34, -22, -24, -8), "Mutfak", "Kitchen", wing="west", surface="stone",
     purpose="cooking; the service stair and the dumbwaiter", dressing="cooker range, cauldron, hanging pans, "
     "long table, herb racks, service stair up to the guest floor")
room("pantry", "F0", rect(-34, -22, -8, 4), "Kiler", "Pantry", wing="west", surface="flour",
     purpose="stores; stair down to the wine cellar", dressing="sacks, barrels, jars on shelves, flour on the floor")
room("laundry", "F0", rect(-22, -10, -6, 4), "Çamaşırhane", "Laundry", wing="west", surface="wet",
     purpose="washing tubs, mangle; a wash spot for blood", dressing="tubs, mangle, sheets on lines, soap, steam")
room("chapel", "F0", rect(10, 22, -20, -10), "Şapel", "Chapel", wing="east", surface="stone",
     purpose="bell rope, candles, crypt stair to the cistern", dressing="pews, altar, bell rope, candle racks, "
     "confessional (secret door), crypt stair")
room("library", "F0", rect(22, 34, -24, -10), "Kütüphane", "Library", wing="east", surface="carpet",
     purpose="reading, fireplace, Pozzo's private spiral stair", dressing="floor-to-ceiling shelves, ladders, "
     "fireplace, globe, reading chairs, rotating bookcase (secret)")
room("ballroom", "F0", rect(10, 34, -10, 6), "Balo Salonu", "Ballroom", wing="east", surface="wood",
     purpose="the biggest open room; orchestrion; French doors to the courtyard", dressing="polished floor, "
     "mirrors, orchestrion (music box organ), dust sheets on chairs, two chandeliers, tall windows")
room("servants_corridor", "F0", rect(-22, 22, -24, -20), "Uşak Koridoru", "Servants' Corridor", kind="circulation",
     wing="service", risk="tehlikeli", surface="stone",
     purpose="44 m service spine behind the hall; lamps only at both ends", dressing="bell board, coat hooks, "
     "crates, a single lamp at each end")
# Grounds on the plateau (F0)
room("courtyard", "F0", [[-10, 10], [10, 10], [10, 6], [34, 6], [34, 34], [-10, 34]], "Ön Avlu",
     "Front Court", kind="grounds", wing="grounds", surface="mud",
     purpose="front court: statue fountain, potted lemons, the sea stair", dressing="gravel + mud, "
     "Vladimir-and-Estragon statue fountain, lemon pots, iron lamps, sea wall with the stair gate")
room("service_yard", "F0", rect(-34, -10, 4, 34), "Hizmet Avlusu", "Service Yard", kind="grounds", wing="grounds",
     min_n=8, surface="mud", purpose="kitchen yard: woodshed, washing lines, chicken coop, gate to the graveyard",
     dressing="woodshed, chopping block, empty washing lines whipping in the wind, coop, carts, puddles")
room("greenhouse", "F0", rect(34, 50, 0, 22), "Sera", "Greenhouse", kind="grounds", wing="grounds", min_n=8,
     surface="mud", purpose="glass house; rain drums on it; old well to the cistern",
     dressing="glass panes (some cracked), benches of pots, palms, water butt, old well cover")
room("graveyard", "F0", rect(-52, -34, 4, 26), "Aile Mezarlığı", "Family Graveyard", kind="grounds", wing="grounds",
     risk="tehlikeli", min_n=12, surface="mud", purpose="Pozzo family graves and mausoleum; 4 grave lanterns",
     dressing="headstones, iron fence, mausoleum (secret stair to the wine cellar), dead tree, lanterns")
room("cliff_path", "F0", rect(-40, -34, -24, 4), "Kayalık Patika", "Cliff Path", kind="circulation",
     wing="grounds", risk="tehlikeli", min_n=12, surface="mud",
     purpose="exposed path along the west cliff: kitchen back door to the graveyard",
     dressing="rope rail, spray, crooked lamp posts, a bench nobody uses")

# Cellar (C) and sea level
room("wine_cellar", "C", rect(-34, -18, -16, 0), "Şarap Mahzeni", "Wine Cellar", wing="cellar", risk="tehlikeli",
     min_n=8, surface="dust", purpose="wine racks; stair to the pantry; the smugglers' tunnel",
     dressing="vaulted brick, racks, barrels, candle niches, cobwebs")
room("spark_room", "C", rect(-18, -6, -16, -4), "Kıvılcım Odası", "Spark Room", wing="cellar", surface="stone",
     purpose="Anselm's spark board: Leyden jars feed the manor's lamps (outage sabotage + fix)",
     dressing="copper coils, glass jars in racks, big knife switch, gauges, sparks")
room("cistern", "C", rect(-6, 14, -18, -2), "Sarnıç", "Cistern", wing="cellar", risk="tehlikeli", surface="wet",
     purpose="rainwater cistern under the hall; well windlass; crypt stair", dressing="columns in black water, "
     "plank walkways, windlass and bucket, drips, the chandelier's rope hole above")
room("smugglers_tunnel", "C", [[-30, 0], [-26, 0], [-26, 38], [-12, 38], [-12, 42], [-30, 42]], "Kaçakçı Tüneli",
     "Smugglers' Tunnel", kind="circulation", wing="cellar", risk="tehlikeli", min_n=8, surface="wet",
     purpose="rock tunnel from the wine cellar to the boathouse", dressing="rough rock, rails, old lanterns, drips")
room("boathouse", "C", rect(-12, 12, 38, 50), "Kayıkhane", "Boathouse", kind="grounds", wing="grounds", min_n=8,
     surface="wet", purpose="sea-level boathouse; rowboat, pump, washed-up 'G.' parcels",
     dressing="slip with a rowboat, pump, nets, oars, crates, waves washing in, sea stair up to the courtyard")

# First floor (F1)
room("gallery", "F1", [[-10, -20], [10, -20], [10, 0], [6, 0], [6, -16], [-6, -16], [-6, 0], [-10, 0]],
     "Portre Galerisi", "Portrait Gallery", wing="hall", surface="carpet",
     purpose="U-shaped gallery around the hall void; overlooks the meeting table",
     dressing="balustrade over the hall, Madam Vellum's portraits, Anselm's portrait (eyes peephole), busts")
room("corridor_w", "F1", rect(-34, -10, -12, -8), "Misafir Koridoru", "Guest Corridor", kind="circulation",
     wing="guest", surface="carpet", purpose="west guest corridor; service stairs up and down",
     dressing="runner rug, sconces, side tables, guest door plaques")
room("blue_room", "F1", rect(-34, -22, -24, -12), "Mavi Misafir Odası", "Blue Guest Room", wing="guest", min_n=8,
     surface="carpet", purpose="guest bedroom", dressing="blue canopy bed, wardrobe, washstand, trunk")
room("bath", "F1", rect(-22, -10, -24, -12), "Hamam", "Bath House", wing="guest", min_n=8, surface="wet",
     purpose="copper tub + boiler; a wash spot; steamy (sight 5 m)", dressing="copper tubs, boiler, towels, "
     "steam, tiled floor, drying rack")
room("red_room", "F1", rect(-34, -22, -8, 4), "Kırmızı Misafir Odası", "Red Guest Room", wing="guest", min_n=8,
     surface="carpet", purpose="guest bedroom", dressing="red bed, wardrobe, writing desk, fireplace")
room("studio", "F1", rect(-22, -10, -8, 4), "Portre Atölyesi", "Portrait Studio", wing="guest", min_n=8,
     surface="paint", purpose="Madam Vellum's studio: easels, paint (paint footprints)",
     dressing="easels, canvases under sheets, paint pots, a model's chair, skylight")
room("nursery", "F1", rect(-10, 10, 0, 8), "Çocuk Odası", "Nursery", wing="hall", surface="wood",
     purpose="the Messenger Boy's room that is always ready; interior window over the hall",
     dressing="small bed, blue hat on a hook, rocking horse, music box, toy boats, wardrobe (secret)")
room("corridor_e", "F1", rect(10, 34, -12, -8), "Doğu Koridoru", "East Corridor", kind="circulation", wing="master",
     surface="carpet", purpose="east corridor to the study, master bedroom, billiards, terrace",
     dressing="runner rug, clocks by Anselm (all stopped), sconces")
room("study", "F1", rect(10, 22, -24, -12), "Çalışma Odası", "Study", wing="master", surface="carpet",
     purpose="Pozzo's study: desk, safe, map of the coast", dressing="desk, safe, map table, telescope, "
     "hunting trophies, letters")
room("master", "F1", rect(22, 34, -24, -12), "Efendi Yatak Odası", "Master Bedroom", wing="master", min_n=8,
     surface="carpet", purpose="Pozzo's bedroom; spiral stairs down to the library and up to the tower",
     dressing="four-poster bed, wardrobe, vanity, fireplace, Lucky's rope hook")
room("billiard", "F1", rect(10, 22, -8, 6), "Bilardo Odası", "Billiard Room", wing="master", surface="wood",
     purpose="billiards and cards; door to the storm terrace", dressing="billiard table, cue rack, card table, "
     "drinks cabinet, smoking chairs")
room("storm_terrace", "F1", rect(22, 34, -8, 6), "Fırtına Terası", "Storm Terrace", kind="outdoor", wing="master",
     risk="tehlikeli", min_n=10, surface="wet", purpose="open roof terrace over the ballroom: rain, wind, lightning",
     dressing="stone balustrade, toppled chairs, flapping awning, lightning rod mast")

# Attic floor (F2) and the tower
room("attic", "F2", rect(-34, -10, -24, -4), "Tavan Arası", "Attic", wing="upper", risk="tehlikeli", min_n=10,
     surface="dust", purpose="attic + servants' beds (Lucky's cot); drying lines; lamp-oil store",
     dressing="rafters, trunks, dust sheets, servants' cots, drying lines, oil cans, dumbwaiter head")
room("clock_room", "F2", rect(-10, 10, -20, -10), "Saat Odası", "Clock Room", wing="upper", min_n=10,
     surface="wood", purpose="Anselm's great clock above the hall; the chandelier winch",
     dressing="giant clockwork, pendulum, the great dial seen from the inside, chandelier winch, gears")
room("roof_walk", "F2", rect(10, 26, -22, -18), "Çatı Yolu", "Roof Walk", kind="outdoor", wing="upper",
     risk="tehlikeli", min_n=10, surface="wet", purpose="catwalk between the clock room and the storm tower",
     dressing="plank catwalk, rope rail, chimneys, slates rattling, lightning in your face")
room("storm_tower", "F2", rect(26, 34, -24, -16), "Fırtına Kulesi", "Storm Tower", wing="upper", risk="tehlikeli",
     min_n=10, surface="stone", purpose="tower room: Leyden jars charge from the lightning rod; ladder to the top",
     dressing="jar rack at the rod cable, spiral stair, ladder hatch, windows on four sides")
room("tower_top", "F3", rect(26, 34, -24, -16), "Kule Tepesi", "Tower Top", kind="outdoor", wing="upper",
     risk="tehlikeli", min_n=10, surface="wet", counts=False, dead_end_ok=True, part_of="storm_tower",
     purpose="signal lamp for the Messenger Boy's boat; lightning rod", dressing="signal lamp, rod, parapet")

# ------------------------------------------------------------------------------------------------ SPRINT-040 expansion
# "Much bigger, a rich house that goes on and on": a north block (grand staircase hall between two enfilades of
# salons, a 68 m portrait gallery behind them, guest rooms above), a West Servants' Wing, an East Guest Wing, a
# second basement (wine catacombs, vault, ossuary) and two secret rooms (the crypt under the chapel, a hidden
# observatory on the east roof). Wings open by player count (Region Gates, min_n): north + west at 8, east at 10,
# the lower vaults at 12; a 6-player match keeps the original 24-room manor.
NORTH, WEST, EAST, DEEP = 8, 8, 10, 12
# F0 north block: an enfilade (doors on the y = -32 axis) from the music room to the trophy room
room("music_room", "F0", rect(-34, -24, -40, -24), "Müzik Salonu", "Music Room", wing="north", min_n=NORTH,
     surface="wood", purpose="grand piano, harp, the tuning chore", dressing="grand piano, harp, music stands, "
     "sheet music, gilt chairs in rows, hearth")
room("green_salon", "F0", rect(-24, -16, -40, -24), "Yeşil Salon", "Green Salon", wing="north", min_n=NORTH,
     surface="carpet", purpose="enfilade salon", dressing="green sofas, tea tables, palms, portraits, cabinets")
room("blue_salon", "F0", rect(-16, -8, -40, -24), "Mavi Salon", "Blue Salon", wing="north", min_n=NORTH,
     surface="carpet", purpose="enfilade salon; ladies' card tables", dressing="blue armchairs, card tables, vitrines")
room("staircase_hall", "F0", rect(-8, 8, -40, -24), "Büyük Merdiven Holü", "Grand Staircase Hall", wing="north",
     min_n=NORTH, surface="stone", purpose="twin grand stairs to the guest floor; a chandelier (trap)",
     dressing="twin stairs, statues on plinths, a great chandelier, banners, a grandfather clock")
room("yellow_salon", "F0", rect(8, 16, -40, -24), "Sarı Salon", "Yellow Salon", wing="north", min_n=NORTH,
     surface="carpet", purpose="enfilade salon", dressing="yellow sofas, harpsichord, flower vases, mirrors")
room("card_room", "F0", rect(16, 24, -40, -24), "Kart Odası", "Card Room", wing="north", min_n=NORTH,
     surface="wood", purpose="card tables; the fireplace passage (secret S7)", dressing="card tables, "
     "chips, decanters, a hearth with a false back")
room("trophy_room", "F0", rect(24, 34, -40, -24), "Av Odası", "Trophy Room", wing="north", min_n=NORTH,
     surface="wood", purpose="hunting trophies; the room that locks itself (trap)", dressing="mounted heads, "
     "gun racks, a bear rug, leather chairs, cabinets")
room("long_gallery", "F0", rect(-34, 34, -44, -40), "Uzun Portre Galerisi", "Long Portrait Gallery",
     kind="circulation", wing="north", min_n=NORTH, risk="tehlikeli", surface="carpet",
     purpose="68 m portrait corridor behind the salons; creaking boards; lamps that go out",
     dressing="portraits end to end, busts, runner rugs, clocks, benches")
# F0 West Servants' Wing
room("west_passage", "F0", rect(-38, -34, -44, -24), "Batı Hizmet Geçidi", "West Service Passage",
     kind="circulation", wing="west", min_n=WEST, surface="stone", purpose="servants' passage; stair up",
     dressing="coat hooks, bell board, lamps, crates")
room("servants_hall", "F0", rect(-54, -44, -44, -34), "Hizmetkâr Salonu", "Servants' Hall", wing="west",
     min_n=WEST, surface="stone", purpose="the servants' long table; the crawlway (secret S9)",
     dressing="long table, benches, dresser with crockery, bell board, hearth")
room("housekeeper", "F0", rect(-54, -44, -34, -24), "Kâhya Odası", "Housekeeper's Room", wing="west",
     min_n=WEST, surface="wood", purpose="keys, ledgers, the jam cupboard", dressing="desk, key board, ledgers, "
     "armchair, jam shelves, a bed in the corner")
room("silver_room", "F0", rect(-44, -38, -44, -34), "Gümüş Odası", "Silver Room", wing="west", min_n=WEST,
     surface="wood", purpose="the butler's pantry: silver chest, polishing bench", dressing="silver chest, "
     "cabinets of plate, polishing bench, candlesticks")
room("boiler_room", "F0", rect(-44, -38, -34, -24), "Kazan Dairesi", "Boiler Room", wing="west", min_n=WEST,
     surface="stone", purpose="the boiler that heats the bath; lamp oil", dressing="boiler, pipes, coal heap, "
     "lamp shelves, oil cans")
# F0 East Guest Wing
room("east_hall", "F0", rect(34, 38, -44, 0), "Doğu Kanat Holü", "East Wing Hall", kind="circulation",
     wing="east", min_n=EAST, surface="stone", purpose="44 m hall of the guest wing; stair up; creaking floor",
     dressing="runner rugs, console tables, busts, sconces")
room("smoking_room", "F0", rect(38, 46, -44, -34), "Tütün Odası", "Smoking Room", wing="east", min_n=EAST,
     surface="carpet", purpose="gentlemen's smoking room", dressing="leather chesterfields, humidor, pipe racks, "
     "a wall safe behind a painting")
room("gun_room", "F0", rect(46, 54, -44, -34), "Silah Odası", "Gun Room", wing="east", min_n=EAST,
     surface="wood", purpose="gun cabinets (locked), boots, game bags", dressing="gun cabinets, boot rack, "
     "game bags, antlers, a workbench")
room("map_room", "F0", rect(38, 46, -34, -24), "Harita Odası", "Map Room", wing="east", min_n=EAST,
     surface="carpet", purpose="sea charts of the coast; the star chart goes here", dressing="map table, globes, "
     "chart chests, a telescope, framed charts")
room("games_room", "F0", rect(46, 54, -34, -24), "Oyun Odası", "Games Room", wing="east", min_n=EAST,
     surface="wood", purpose="the chessboard chore; backgammon, cards", dressing="chess table, game boxes, "
     "armchairs, a dart board")
room("orangery", "F0", rect(38, 54, -24, -10), "Portakallık", "Orangery", wing="east", min_n=EAST,
     surface="stone", purpose="orange trees in tubs; the aviary", dressing="orange trees, aviary cage, benches, "
     "a fountain, watering cans")
room("morning_room", "F0", rect(38, 54, -10, 0), "Sabah Odası", "Morning Room", wing="east", min_n=EAST,
     surface="carpet", purpose="breakfast room over the greenhouse", dressing="breakfast table, sideboard, "
     "sofas, flowers, a writing desk")
# F1: the guest floor over the north block and the wings
room("north_corridor", "F1", rect(-34, 34, -44, -40), "Üst Portre Koridoru", "Upper Portrait Corridor",
     kind="circulation", wing="north", min_n=NORTH, surface="carpet",
     purpose="the guest floor's north spine over the long gallery", dressing="portraits, runner, side tables")
room("grand_landing", "F1", rect(-8, 8, -40, -24), "Büyük Sahanlık", "Grand Landing", kind="circulation",
     wing="north", min_n=NORTH, surface="carpet", purpose="top of the twin grand stairs",
     dressing="balustrades, statues, sofas, a tall window")
room("lilac_room", "F1", rect(-34, -24, -40, -24), "Leylak Misafir Odası", "Lilac Guest Room", wing="north",
     min_n=NORTH, surface="carpet", purpose="guest bedroom", dressing="lilac bed, wardrobe, vanity, trunk")
room("sewing_room", "F1", rect(-24, -8, -40, -24), "Dikiş Odası", "Sewing Room", wing="north", min_n=NORTH,
     surface="wood", purpose="dress forms, the linen press", dressing="dress forms, sewing tables, bolts of "
     "cloth, linen press, baskets")
room("chinese_room", "F1", rect(8, 24, -40, -24), "Çin Odası", "Chinese Room", wing="north", min_n=NORTH,
     surface="carpet", purpose="Pozzo senior's collection: lacquer, vases", dressing="lacquer cabinets, vases, "
     "screens, a daybed, silk hangings")
room("dressing_room", "F1", rect(24, 34, -40, -24), "Giyinme Odası", "Dressing Room", wing="north",
     min_n=NORTH, surface="carpet", purpose="Pozzo's wardrobes; the fireplace passage comes out here",
     dressing="wardrobes, mirrors, shoe racks, a chaise, hat boxes")
room("upper_west", "F1", rect(-38, -34, -44, -24), "Batı Üst Geçit", "Upper West Passage", kind="circulation",
     wing="west", min_n=WEST, surface="wood", purpose="top of the service stair", dressing="linen shelves, lamps")
room("picture_gallery", "F1", rect(-54, -38, -44, -24), "Resim Galerisi", "Picture Gallery", wing="west",
     min_n=WEST, surface="carpet", dead_end_ok=True, purpose="the family's great paintings; a portrait that watches",
     dressing="paintings frame to frame, statues, benches, velvet ropes")
room("east_corridor", "F1", rect(34, 38, -44, -4), "Doğu Misafir Koridoru", "East Guest Corridor",
     kind="circulation", wing="east", min_n=EAST, surface="carpet", purpose="the guest wing's corridor",
     dressing="runner, guest door plaques, side tables, sconces")
room("green_room", "F1", rect(38, 46, -44, -34), "Yeşil Misafir Odası", "Green Guest Room", wing="east",
     min_n=EAST, surface="carpet", purpose="guest bedroom; the room that locks (trap)", dressing="green bed, "
     "wardrobe, desk, trunk")
room("gold_room", "F1", rect(46, 54, -44, -34), "Altın Misafir Odası", "Gold Guest Room", wing="east",
     min_n=EAST, surface="carpet", purpose="guest bedroom; a panel to the hidden observatory (secret S10)",
     dressing="gold bed, wardrobe, vanity, a ladder behind the panel")
room("rose_room", "F1", rect(38, 46, -34, -24), "Gül Misafir Odası", "Rose Guest Room", wing="east",
     min_n=EAST, surface="carpet", purpose="guest bedroom", dressing="rose bed, wardrobe, washstand")
room("ivory_room", "F1", rect(46, 54, -34, -24), "Fildişi Misafir Odası", "Ivory Guest Room", wing="east",
     min_n=EAST, surface="carpet", purpose="guest bedroom", dressing="ivory bed, wardrobe, writing desk")
room("guest_bath", "F1", rect(38, 54, -24, -16), "Misafir Hamamı", "Guest Bath", wing="east", min_n=EAST,
     surface="wet", purpose="the guest wing's bath", dressing="bathtubs, basins, mirrors, towel rails")
# F2: the hidden observatory on the east roof (secret room: reached only through S10)
room("observatory", "F2", rect(46, 54, -44, -36), "Gizli Rasathane", "Hidden Observatory", wing="east",
     min_n=EAST, surface="wood", secret=True, dead_end_ok=True,
     purpose="Anselm's secret star room: a brass telescope under a slit roof", dressing="telescope, star charts, "
     "orrery, a cot, notebooks")
# C2: the lower vaults
room("wine_catacombs", "C2", rect(-34, -22, -16, 0), "Şarap Katakombları", "Wine Catacombs", wing="deep",
     min_n=DEEP, risk="tehlikeli", surface="dust", purpose="racks of old vintages in vaulted niches",
     dressing="wine niches, barrels, candles, cobwebs")
room("catacomb_steps", "C2", rect(-22, -18, -10, -2), "Katakomb Merdiveni", "Catacomb Steps", kind="circulation",
     wing="cellar", min_n=8, surface="stone", purpose="stair down from the wine cellar", dressing="lantern, rope")
room("vault", "C2", [[-22, -16], [-6, -16], [-6, -4], [-18, -4], [-18, -10], [-22, -10]], "Kasa Dairesi", "Strong Vault", wing="deep", min_n=DEEP,
     risk="tehlikeli", surface="stone", purpose="the family strongroom: a floor safe", dressing="iron chests, "
     "strongboxes, shelves of deeds, a floor safe")
room("ossuary", "C2", rect(-6, 6, -16, -4), "Kemiklik", "Ossuary", wing="deep", min_n=DEEP, risk="tehlikeli",
     surface="dust", purpose="bones of the old monks under the cistern", dressing="skull walls, niches, candles")
room("ossuary_steps", "C2", rect(6, 10, -12, -4), "Kemiklik Merdiveni", "Ossuary Steps", kind="circulation",
     wing="cellar", surface="stone", purpose="stair down from the cistern", dressing="lantern, rope rail")
room("crypt", "C2", rect(10, 22, -20, -10), "Şapel Kriptası", "Chapel Crypt", wing="deep", surface="stone",
     secret=True, dead_end_ok=True, purpose="the Pozzo crypt under the chapel (secret S8): a relic",
     dressing="sarcophagi, candles, a reliquary, bones")

R = {r["id"]: r for r in ROOMS}

# ------------------------------------------------------------------------------------------------ doors
DOORS = []


def door(a, b, at, width=2.0, kind="door", note=None):
    d = {"id": f"d_{a}__{b}", "a": a, "b": b, "at": list(at), "width": width, "kind": kind}
    if note:
        d["note"] = note
    DOORS.append(d)


# F0
door("great_hall", "vestibule", (0, 0), 4.0, "arch")
door("great_hall", "dining", (-10, -13))
door("great_hall", "chapel", (10, -15))
door("great_hall", "ballroom", (10, -5), 4.0, "double")
door("great_hall", "servants_corridor", (-4, -20), note="service door beside the hearth")
door("vestibule", "courtyard", (0, 10), 4.0, "front", note="the front door: storm gusts slam it")
door("vestibule", "laundry", (-10, 2))
door("vestibule", "ballroom", (10, 3))
door("dining", "kitchen", (-22, -14))
door("dining", "servants_corridor", (-16, -20))
door("dining", "laundry", (-16, -6))
door("kitchen", "servants_corridor", (-22, -22))
door("kitchen", "pantry", (-28, -8))
door("kitchen", "cliff_path", (-34, -18), note="kitchen back door")
door("pantry", "laundry", (-22, -1))
door("pantry", "service_yard", (-28, 4), note="service yard door")
door("laundry", "service_yard", (-16, 4))
door("service_yard", "courtyard", (-10, 22), 4.0, "gate", note="iron gate between the yards")
door("chapel", "servants_corridor", (16, -20))
door("library", "servants_corridor", (22, -22))
door("library", "ballroom", (28, -10))
door("ballroom", "courtyard", (22, 6), 4.0, "french")
door("ballroom", "greenhouse", (34, 3))
door("courtyard", "greenhouse", (34, 14))
door("service_yard", "graveyard", (-34, 16), kind="gate")
door("cliff_path", "graveyard", (-37, 4), kind="gate")
# C
door("wine_cellar", "spark_room", (-18, -10))
door("spark_room", "cistern", (-6, -10))
door("wine_cellar", "smugglers_tunnel", (-28, 0))
door("smugglers_tunnel", "boathouse", (-12, 40))
# F1
door("gallery", "bath", (-10, -17))
door("gallery", "corridor_w", (-10, -10))
door("gallery", "corridor_e", (10, -10))
door("gallery", "study", (10, -17))
door("gallery", "nursery", (-8, 0))
door("gallery", "nursery", (8, 0))
DOORS[-1]["id"] += "_e"
DOORS[-2]["id"] += "_w"
door("corridor_w", "blue_room", (-28, -12))
door("corridor_w", "bath", (-16, -12))
door("corridor_w", "red_room", (-28, -8))
door("corridor_w", "studio", (-16, -8))
door("blue_room", "bath", (-22, -18))
door("red_room", "studio", (-22, -2))
door("studio", "nursery", (-10, 2))
door("nursery", "billiard", (10, 3))
door("corridor_e", "study", (16, -12))
door("corridor_e", "master", (28, -12))
door("corridor_e", "billiard", (16, -8))
door("corridor_e", "storm_terrace", (28, -8))
door("billiard", "storm_terrace", (22, 0), kind="french")
door("study", "master", (22, -18))
# F2
door("attic", "clock_room", (-10, -15))
door("clock_room", "roof_walk", (10, -19))
door("roof_walk", "storm_tower", (26, -20))
# SPRINT-040 F0 north block: the enfilade on y = -32, the long gallery behind, service doors to the old house
door("staircase_hall", "servants_corridor", (0, -24), 4.0, "double", note="gate: opens at 8 players")
door("green_salon", "servants_corridor", (-19, -24))
door("yellow_salon", "servants_corridor", (12, -24))
door("music_room", "kitchen", (-29, -24))
door("trophy_room", "library", (28, -24))
door("music_room", "green_salon", (-24, -32), 2.0, "double")
door("green_salon", "blue_salon", (-16, -32), 2.0, "double")
door("blue_salon", "staircase_hall", (-8, -32), 2.0, "double")
door("staircase_hall", "yellow_salon", (8, -32), 2.0, "double")
door("yellow_salon", "card_room", (16, -32), 2.0, "double")
door("card_room", "trophy_room", (24, -32), 2.0, "double")
door("long_gallery", "music_room", (-29, -40))
door("long_gallery", "blue_salon", (-12, -40))
door("long_gallery", "staircase_hall", (0, -40), 4.0, "arch")
door("long_gallery", "card_room", (20, -40))
door("long_gallery", "trophy_room", (29, -40))
# F0 west wing
door("west_passage", "long_gallery", (-34, -42))
door("west_passage", "music_room", (-34, -36))
door("west_passage", "cliff_path", (-36, -24), note="the west wing's back door; opens at 8 players")
door("west_passage", "silver_room", (-38, -39))
door("west_passage", "boiler_room", (-38, -29))
door("silver_room", "servants_hall", (-44, -39))
door("boiler_room", "housekeeper", (-44, -29))
door("servants_hall", "housekeeper", (-49, -34))
# F0 east wing
door("east_hall", "long_gallery", (34, -42))
door("east_hall", "trophy_room", (34, -30))
door("east_hall", "ballroom", (34, -5), note="gate: opens at 10 players")
door("east_hall", "greenhouse", (36, 0))
door("east_hall", "smoking_room", (38, -39))
door("east_hall", "map_room", (38, -29))
door("east_hall", "orangery", (38, -19))
door("east_hall", "morning_room", (38, -3))
door("smoking_room", "gun_room", (46, -39))
door("map_room", "games_room", (46, -29))
door("gun_room", "games_room", (50, -34))
door("games_room", "orangery", (50, -24))
door("orangery", "morning_room", (46, -10), 2.0, "double")
door("morning_room", "greenhouse", (44, 0), kind="french")
# F1
door("north_corridor", "upper_west", (-34, -42))
door("north_corridor", "east_corridor", (34, -42))
door("north_corridor", "lilac_room", (-29, -40))
door("north_corridor", "sewing_room", (-16, -40))
door("north_corridor", "grand_landing", (0, -40), 4.0, "arch")
door("north_corridor", "chinese_room", (16, -40))
door("north_corridor", "dressing_room", (29, -40))
door("grand_landing", "sewing_room", (-8, -30))
door("grand_landing", "chinese_room", (8, -30))
door("lilac_room", "sewing_room", (-24, -30))
door("chinese_room", "dressing_room", (24, -30))
door("lilac_room", "blue_room", (-28, -24), note="the lilac room's door to the old guest floor")
door("upper_west", "picture_gallery", (-38, -40))
door("upper_west", "picture_gallery", (-38, -28))
DOORS[-1]["id"] += "_s"
DOORS[-2]["id"] += "_n"
door("east_corridor", "corridor_e", (34, -10), note="gate: opens at 10 players")
door("east_corridor", "green_room", (38, -39))
door("east_corridor", "rose_room", (38, -29))
door("east_corridor", "guest_bath", (38, -20))
door("green_room", "gold_room", (46, -39))
door("rose_room", "ivory_room", (46, -29))
door("gold_room", "ivory_room", (50, -34))
door("ivory_room", "guest_bath", (50, -24))
# C2
door("catacomb_steps", "wine_catacombs", (-22, -3), note="gate: opens at 12 players")
door("catacomb_steps", "vault", (-18, -7), note="the vault door; opens at 12 players")
door("vault", "wine_catacombs", (-22, -13))
door("vault", "ossuary", (-6, -10))
door("ossuary", "ossuary_steps", (6, -5), note="gate: opens at 12 players")

# ------------------------------------------------------------------------------------------------ stairs
STAIRS = []


def stair(sid, name_tr, lower, upper, bottom, top, width=2.0, kind="service"):
    zl = next(f["z"] for f in FLOORS if f["id"] == R[lower]["floor"])
    zu = next(f["z"] for f in FLOORS if f["id"] == R[upper]["floor"])
    STAIRS.append({"id": sid, "name_tr": name_tr, "lower": lower, "upper": upper, "bottom": list(bottom),
                   "top": list(top), "z0": zl, "z1": zu, "width": width, "kind": kind})


stair("st_grand_w", "Büyük Merdiven (batı)", "great_hall", "gallery", (-8, -3), (-8, -9), 2.0, "grand")
stair("st_grand_e", "Büyük Merdiven (doğu)", "great_hall", "gallery", (8, -3), (8, -9), 2.0, "grand")
stair("st_service_1", "Uşak Merdiveni (alt)", "kitchen", "corridor_w", (-24, -11), (-30, -11), 2.0, "service")
stair("st_service_2", "Uşak Merdiveni (üst)", "corridor_w", "attic", (-12, -9), (-18, -9), 2.0, "service")
stair("st_cellar", "Kiler Merdiveni", "wine_cellar", "pantry", (-30, -4), (-24, -4), 2.0, "service")
stair("st_crypt", "Kripta Merdiveni", "cistern", "chapel", (12, -12), (18, -12), 2.0, "service")
stair("st_library_spiral", "Pozzo'nun Döner Merdiveni", "library", "master", (31, -14), (31, -17), 2.0, "spiral")
stair("st_clock", "Saat Merdiveni", "gallery", "clock_room", (6, -18), (0, -18), 2.0, "service")
stair("st_tower_spiral", "Kule Merdiveni", "master", "storm_tower", (30, -22), (30, -19), 2.0, "spiral")
stair("st_tower_ladder", "Kule Merdiveni (el)", "storm_tower", "tower_top", (28, -22), (28, -22), 1.0, "ladder")
stair("st_sea", "Rıhtım Merdiveni", "boathouse", "courtyard", (0, 39), (0, 33), 4.0, "outdoor")
# SPRINT-040 (authored in the build form: 6 m run per 3 m rise on whole 2 m cells, no STAIR_FIX needed)
stair("st_grand_nw", "Büyük Merdiven (kuzey-batı)", "staircase_hall", "grand_landing", (-5, -26), (-5, -32), 2.0,
      "grand")
stair("st_grand_ne", "Büyük Merdiven (kuzey-doğu)", "staircase_hall", "grand_landing", (5, -26), (5, -32), 2.0,
      "grand")
stair("st_west", "Batı Hizmet Merdiveni", "west_passage", "upper_west", (-35, -26), (-35, -32), 2.0, "service")
stair("st_east", "Doğu Kanat Merdiveni", "east_hall", "east_corridor", (37, -8), (37, -14), 2.0, "service")
stair("st_catacomb", "Katakomb Merdiveni", "catacomb_steps", "wine_cellar", (-21, -4), (-21, -10), 2.0, "service")
stair("st_ossuary", "Kemiklik Merdiveni", "ossuary_steps", "cistern", (7, -6), (7, -12), 2.0, "service")

# ------------------------------------------------------------------------------------------------ secret passages
SECRETS = [
    {"id": "S1", "name_tr": "Dönen Kitaplık", "a": "library", "at_a": [23.2, -16], "b": "chapel", "at_b": [20.8, -16],
     "how": "pull the red book: the shelf turns into the confessional"},
    {"id": "S2", "name_tr": "Yemek Asansörü", "a": "kitchen", "at_a": [-33, -22], "b": "attic", "at_b": [-33, -22],
     "how": "climb into the dumbwaiter and haul the rope (2 floors)"},
    {"id": "S3", "name_tr": "Anselm'in Portresi", "a": "gallery", "at_a": [-9.2, -4], "b": "clock_room",
     "at_b": [-9, -12], "how": "the portrait swings open; a ladder behind the clock face. Its eyes are a peephole "
                                "over the meeting table"},
    {"id": "S4", "name_tr": "Anıt Mezar Merdiveni", "a": "graveyard", "at_a": [-48, 22], "b": "wine_cellar",
     "at_b": [-33, -14], "how": "slide the lid of Pozzo senior's tomb; steps down to a false wine rack"},
    {"id": "S5", "name_tr": "Eski Kuyu", "a": "greenhouse", "at_a": [47, 19], "b": "cistern", "at_b": [12.5, -3.5],
     "how": "the old well under the greenhouse drains into the cistern; iron rungs"},
    {"id": "S6", "name_tr": "Duvar Arası", "a": "nursery", "at_a": [8.8, 7], "b": "master", "at_b": [33, -13],
     "how": "the back of the nursery wardrobe: a crawl between the walls to Pozzo's bedroom"},
    # SPRINT-040
    {"id": "S7", "name_tr": "Şömine Geçidi", "kind": "fireplace", "a": "card_room", "at_a": [16.8, -27.0],
     "b": "dressing_room", "at_b": [33.2, -36.0],
     "how": "the card room hearth's back plate swings in: a sooty stair up behind Pozzo's wardrobes"},
    {"id": "S8", "name_tr": "Kripta Kapağı", "kind": "crypt", "a": "chapel", "at_a": [16.0, -19.0], "b": "crypt",
     "at_b": [16.0, -19.0], "how": "the altar slab slides aside: steps down to the Pozzo crypt"},
    {"id": "S9", "name_tr": "Uşak Sürünme Yolu", "kind": "crawlway", "a": "servants_hall", "at_a": [-53.2, -36.0],
     "b": "music_room", "at_b": [-33.2, -27.0],
     "how": "a low hatch behind the servants' dresser: a crawl between the walls to the music room"},
    {"id": "S10", "name_tr": "Rasathane Merdiveni", "kind": "observatory", "a": "gold_room", "at_a": [53.2, -38.5],
     "b": "observatory", "at_b": [53.2, -38.5],
     "how": "a panel beside the gold room's bed: a ladder up to Anselm's hidden observatory"},
]
SECRET_KIND = {"S1": "bookcase", "S2": "dumbwaiter", "S3": "portrait", "S4": "tomb", "S5": "well", "S6": "wardrobe"}
for _s in SECRETS:
    _s.setdefault("kind", SECRET_KIND.get(_s["id"], "panel"))
SECRET_ROOMS = [r["id"] for r in ROOMS if r.get("secret")]
SECRET_RULES = {
    "who": "everyone once discovered; the Impatient see all of them on their map from the start",
    "discover": "a seam glints for 0.35 s during a lightning flash within 4 m (LOS); opening it marks it for "
                "everyone who saw it",
    "travel_s": 3.0,
    "noise_m": 12,
    "trace": "the panel stays ajar for 20 s and a dust puff hangs at both ends",
    "counter": "any player can wedge a passage shut for the rest of the day (4 s, visible wedge); the Locksmith "
               "can lock or unlock it",
    "not_for_connectivity": True,
    # SPRINT-040 build: every end is an AKGSecretPassage. Undiscovered, E examines it (the seam, the loose plate) and
    # opens it for everyone (server-replicated, stays open); only then is it drawn on the minimap (the Impatient see
    # all of them from the start). The secret rooms (crypt, observatory) are reached through their passage only.
    "build": "AKGSecretPassage pairs; discovered state replicated; minimap draws discovered ones only",
    "secret_rooms": "crypt (S8), observatory (S10)",
}

# ------------------------------------------------------------------------------------------------ anchors + chores
ANCHORS = []


def anchor(aid, rid, at, label_tr, kind="spot", r=1.6, **kw):
    a = {"id": aid, "room": rid, "at": list(at), "label_tr": label_tr, "kind": kind, "r": r}
    a.update(kw)
    ANCHORS.append(a)


anchor("hall_table", "great_hall", (0, -9), "uzun masadaki sürahi", "Table", r=2.5)
anchor("hall_hearth", "great_hall", (2.5, -18.8), "salonun şöminesi", "Hearth")
anchor("hall_chandelier", "great_hall", (0, -13.5), "indirilen avize", "Chandelier")
anchor("wine_rack", "wine_cellar", (-24, -12), "şarap rafları", "WineRack")
anchor("cistern_well", "cistern", (4, -10), "sarnıç çıkrığı", "Well", fill=True)
anchor("kitchen_kettle", "kitchen", (-31, -19), "mutfak kazanı", "Cauldron", sabotage="poison")
anchor("kitchen_pass", "kitchen", (-27, -15), "servis tezgâhı", "Counter")
anchor("dining_table", "dining", (-16, -13), "ziyafet masası", "Table", r=2.5)
anchor("bath_boiler", "bath", (-13, -21), "hamam kazanı", "Boiler", sabotage="poison")
anchor("greenhouse_butt", "greenhouse", (38, 3), "sera su fıçısı", "Butt", sabotage="poison")
anchor("woodshed", "service_yard", (-28, 9), "odunluk", "Woodpile")
anchor("library_fire", "library", (33, -20), "kütüphane şöminesi", "Hearth")
anchor("chapel_bell", "chapel", (12, -19), "çan ipi", "BellRope")
anchor("chapel_candles", "chapel", (20, -19), "mum kutusu", "Candles")
anchor("great_clock", "clock_room", (-6, -18.5), "büyük saatin kurgusu", "Clock")
anchor("clock_winch", "clock_room", (-2, -12), "avize çıkrığı", "Winch")
anchor("attic_oil", "attic", (-30, -8), "lamba yağı sandığı", "OilStore")
anchor("attic_lines", "attic", (-20, -20), "tavan arası ipleri", "Lines")
anchor("tower_lamp", "tower_top", (30, -20), "sinyal feneri", "SignalLamp")
anchor("tower_jars", "storm_tower", (32.5, -22.5), "paratoner kavanoz rafı", "JarRack")
anchor("spark_board", "spark_room", (-12, -15), "kıvılcım panosu", "SparkBoard")
anchor("post_desk", "vestibule", (7, 8), "posta masası", "PostDesk")
anchor("letter_blue", "blue_room", (-28, -14), "Mavi Oda", "Door", r=1.3)
anchor("letter_red", "red_room", (-28, -6), "Kırmızı Oda", "Door", r=1.3)
anchor("letter_master", "master", (26, -14), "Efendi'nin odası", "Door", r=1.3)
anchor("letter_study", "study", (16, -14), "çalışma odası", "Door", r=1.3)
anchor("letter_nursery", "nursery", (-6, 1.5), "çocuk odası", "Door", r=1.3)
anchor("letter_billiard", "billiard", (16, -6), "bilardo odası", "Door", r=1.3)
anchor("win_blue", "blue_room", (-33, -18), "Mavi Oda penceresi", "Shutter", r=1.2)
anchor("win_red", "red_room", (-33, -2), "Kırmızı Oda penceresi", "Shutter", r=1.2)
anchor("win_studio", "studio", (-16, 3), "atölye penceresi", "Shutter", r=1.2)
anchor("win_nursery", "nursery", (0, 7), "çocuk odası penceresi", "Shutter", r=1.2)
anchor("win_billiard", "billiard", (19, 5), "bilardo penceresi", "Shutter", r=1.2)
anchor("laundry_tub", "laundry", (-18, -1), "ıslak çarşaf sepeti", "Basket")
anchor("bath_rack", "bath", (-19, -14), "hamam kurutma askısı", "Lines")
anchor("studio_easel", "studio", (-16, -3), "bitmiş portre", "Easel")
anchor("gallery_frame", "gallery", (-4, -19), "galerideki boş çerçeve", "Frame")
anchor("nursery_cylinder", "nursery", (4, 4), "müzik kutusu silindiri", "MusicBox")
anchor("orchestrion", "ballroom", (31, -7), "orkestriyon", "Orchestrion")
anchor("billiard_book", "billiard", (13, -4), "unutulmuş kitap", "Book")
anchor("library_shelf", "library", (25, -22.5), "boş raf", "Shelf")
anchor("boat_pump", "boathouse", (6, 46), "sintine pompası", "Pump")
anchor("boat_rope", "boathouse", (-2, 48.5), "kayık halatı", "Mooring")
anchor("g_parcel", "boathouse", (-8, 44), "denizden vuran G. kolisi", "Parcel")
anchor("pantry_china", "pantry", (-31, -3), "porselen dolabı", "Shelf")
anchor("parcel_corner", "vestibule", (-6, 8), "G. kolileri köşesi", "ParcelPile")
anchor("gallery_cleat", "gallery", (-8, -18.5), "galerideki avize halatı", "Winch")
anchor("win_ballroom", "ballroom", (32, 5.2), "balo salonu penceresi", "Shutter", r=1.2)
anchor("courtyard_pots", "courtyard", (24, 24), "limon saksıları", "Pots")
anchor("greenhouse_bench", "greenhouse", (44, 14), "sera tezgâhı", "Bench")
anchor("grave_1", "graveyard", (-48, 9), "mezar feneri 1", "GraveLamp", r=1.2)
anchor("grave_2", "graveyard", (-40, 11), "mezar feneri 2", "GraveLamp", r=1.2)
anchor("grave_3", "graveyard", (-44, 17), "mezar feneri 3", "GraveLamp", r=1.2)
anchor("grave_4", "graveyard", (-38, 23), "mezar feneri 4", "GraveLamp", r=1.2)
anchor("fuse_coil", "clock_room", (6, -12), "kıvılcım bobini (ikinci şalter)", "SparkCoil")
# SPRINT-040 manor chores (new wings) + secret chores
anchor("clock_long", "long_gallery", (-24, -43.2), "uzun galerideki büyük saat", "Clock")
anchor("clock_stair", "staircase_hall", (-6.5, -38.8), "merdiven holündeki saat", "Clock")
anchor("clock_trophy", "trophy_room", (33.0, -37.0), "av odasındaki saat", "Clock")
anchor("silver_chest", "silver_room", (-42.8, -43.0), "gümüş sandığı", "SilverChest")
anchor("silver_bench", "silver_room", (-39.0, -35.0), "cila tezgâhı", "Bench")
anchor("piano", "music_room", (-29.0, -34.0), "kuyruklu piyano", "Piano")
anchor("music_cabinet", "blue_salon", (-9.0, -38.8), "nota dolabı (akort çatalı)", "Cabinet")
anchor("aviary", "orangery", (52.5, -12.0), "kuşhane", "Aviary")
anchor("seed_sack", "greenhouse", (47.0, 5.0), "kuş yemi çuvalı", "Sack")
anchor("coal_heap", "service_yard", (-32.0, 7.0), "kömür yığını", "Coal")
anchor("boiler", "boiler_room", (-42.8, -26.0), "kazan", "Boiler")
anchor("chess_box", "card_room", (22.8, -38.8), "satranç taşı kutusu", "Box")
anchor("chessboard", "games_room", (50.0, -29.0), "satranç masası", "Chess")
anchor("rack_1", "wine_catacombs", (-33.0, -14.0), "eski bağ nişi 1", "WineRack")
anchor("rack_2", "wine_catacombs", (-33.0, -5.0), "eski bağ nişi 2", "WineRack")
anchor("rack_3", "wine_catacombs", (-27.0, -15.0), "eski bağ nişi 3", "WineRack")
anchor("chapel_cand_w", "chapel", (11.0, -11.2), "batı şamdanı", "Candles")
anchor("chapel_cand_e", "chapel", (21.0, -13.0), "doğu şamdanı", "Candles")
anchor("portrait_1", "long_gallery", (-10.0, -43.2), "tozlu portre 1", "Frame")
anchor("portrait_2", "long_gallery", (14.0, -43.2), "tozlu portre 2", "Frame")
anchor("portrait_3", "picture_gallery", (-46.0, -43.0), "tozlu tablo", "Frame")
anchor("win_green", "green_room", (40.0, -43.2), "Yeşil Oda penceresi", "Shutter", r=1.2)
anchor("win_gold", "gold_room", (52.0, -43.2), "Altın Oda penceresi", "Shutter", r=1.2)
anchor("win_ivory", "ivory_room", (53.2, -28.0), "Fildişi Oda penceresi", "Shutter", r=1.2)
anchor("tea_green", "green_salon", (-20.0, -38.8), "yeşil salondaki çay masası", "Table")
anchor("tea_yellow", "yellow_salon", (12.0, -38.8), "sarı salondaki çay masası", "Table")
anchor("linen_press", "sewing_room", (-12.0, -38.8), "çarşaf dolabı", "Linen")
anchor("bed_green", "green_room", (42.0, -36.0), "Yeşil Oda yatağı", "Bed")
anchor("bed_rose", "rose_room", (42.0, -26.5), "Gül Oda yatağı", "Bed")
anchor("bed_lilac", "lilac_room", (-29.0, -26.5), "Leylak Oda yatağı", "Bed")
anchor("bath_e_tub", "guest_bath", (50.0, -22.0), "misafir küveti", "Boiler")
anchor("vase_chinese", "chinese_room", (16.0, -26.0), "Çin vazosu", "Pots")
anchor("hall_trophies", "trophy_room", (29.0, -26.0), "av trofeleri", "Frame")
anchor("housekeeper_keys", "housekeeper", (-49.0, -25.0), "anahtar panosu", "Keys")
anchor("servants_table", "servants_hall", (-49.0, -39.0), "hizmetkâr masası", "Table", r=2.0)
anchor("gun_bench", "gun_room", (50.0, -43.0), "tüfek tezgâhı", "Bench")
anchor("smoking_humidor", "smoking_room", (42.0, -35.0), "puro kutusu", "Box")
anchor("morning_table", "morning_room", (46.0, -5.0), "kahvaltı masası", "Table", r=2.0)
anchor("dressing_mirror", "dressing_room", (26.0, -38.8), "boy aynası", "Frame")
anchor("vault_ledger", "vault", (-12.0, -15.2), "tapu rafları", "Shelf")
anchor("ossuary_niche", "ossuary", (0.0, -15.2), "kafatası nişi", "Shelf")
# secret chores (given to whoever discovers the secret)
anchor("crypt_relic", "crypt", (19.0, -12.0), "kriptadaki rölikerlik", "Relic")
anchor("chapel_reliquary", "chapel", (17.8, -19.2), "şapeldeki boş rölikerlik", "Relic")
anchor("obs_scope", "observatory", (48.5, -41.5), "pirinç teleskop", "Scope")
anchor("map_table", "map_room", (42.0, -29.0), "harita masası", "Table", r=2.0)
anchor("smuggler_ledger", "wine_cellar", (-31.0, -15.2), "sahte raftaki kaçakçı defteri", "Book")
anchor("study_safe", "study", (20.5, -22.6), "Pozzo'nun kasası", "Safe")
anchor("red_book", "library", (29.0, -11.0), "kırmızı kitabın rafı", "Book")

ITEMS = {
    "Bottles": {"label_tr": "şarap sepeti", "speed": 1.0, "fragile": True},
    "Bucket": {"label_tr": "kova", "speed": 1.0, "liquid": True},
    "Logs": {"label_tr": "odun demeti", "speed": 0.85},
    "Plates": {"label_tr": "tabak yığını", "speed": 1.0, "fragile": True},
    "Tray": {"label_tr": "yemek tepsisi", "speed": 1.0, "fragile": True},
    "Letters": {"label_tr": "mühürlü mektuplar", "speed": 1.0, "count": 3},
    "OilCan": {"label_tr": "yağ bidonu", "speed": 0.9},
    "Jar": {"label_tr": "dolu Leyden kavanozu", "speed": 1.0, "charged": True},
    "Sheets": {"label_tr": "ıslak çarşaflar", "speed": 0.85},
    "Portrait": {"label_tr": "portre", "speed": 0.85},
    "Cylinder": {"label_tr": "müzik silindiri", "speed": 1.0},
    "Book": {"label_tr": "kitap", "speed": 1.0},
    "Pot": {"label_tr": "limon saksısı", "speed": 0.7},
    "Parcel": {"label_tr": "G. kolisi", "speed": 0.55, "two_person": True},
    "Taper": {"label_tr": "yanan fitil", "speed": 1.0, "flame": True},
    # SPRINT-040
    "Silver": {"label_tr": "gümüş takımı", "speed": 1.0, "fragile": True},
    "Fork": {"label_tr": "akort çatalı", "speed": 1.0},
    "Seed": {"label_tr": "kuş yemi", "speed": 1.0},
    "Coal": {"label_tr": "kömür kovası", "speed": 0.85},
    "Chessmen": {"label_tr": "satranç taşları", "speed": 1.0},
    "TeaTray": {"label_tr": "çay tepsisi", "speed": 1.0, "fragile": True, "count": 2},
    "Linen": {"label_tr": "temiz çarşaflar", "speed": 1.0, "count": 3},
    "Keys": {"label_tr": "kâhyanın anahtarları", "speed": 1.0, "count": 3},
    "Relic": {"label_tr": "kriptanın röliği", "speed": 0.9},
    "Chart": {"label_tr": "yıldız haritası", "speed": 1.0},
    "Ledger": {"label_tr": "kaçakçı defteri", "speed": 1.0},
    "RedBook": {"label_tr": "kırmızı kitap", "speed": 1.0},
}

CHORES = []


def chore(cid, title_tr, title_en, steps, variants=None, twist=None):
    c = {"id": cid, "title_tr": title_tr, "title_en": title_en, "steps": steps}
    if variants:
        c["variants"] = variants
    if twist:
        c["twist"] = twist
    CHORES.append(c)


def take(at, item, secs, label_tr, **kw):
    return dict(verb="take", at=at, item=item, secs=secs, label_tr=label_tr, **kw)


def bring(at, item, secs, label_tr, **kw):
    return dict(verb="bring", at=at, item=item, secs=secs, label_tr=label_tr, **kw)


def work(at, secs, label_tr, **kw):
    return dict(verb="work", at=at, secs=secs, label_tr=label_tr, **kw)


chore("WineForTheHall", "Salona Şarap", "Wine for the hall",
      [take("wine_rack", "Bottles", 2.0, "Mahzenden bir şarap sepeti al"),
       bring("hall_table", "Bottles", 2.0, "Uzun masadaki sürahiyi doldur (koşarsan şişeler şıngırdar)")],
      twist="the Impatient can poison the decanter: the next pourer sees a purple ring and must empty it first")
chore("CisternWater", "Sarnıçtan Su", "Water from the cistern",
      [take("cistern_well", "Bucket", 3.0, "Sarnıçtan dolu bir kova çek", fill=1.0),
       bring("$dst", "Bucket", 1.5, "Kovayı $dst boşalt (yürü, koşma!)", min_fill=0.35)],
      variants=[{"name": "kitchen", "vars": {"dst": "kitchen_kettle"}},
                {"name": "bath", "vars": {"dst": "bath_boiler"}},
                {"name": "greenhouse", "vars": {"dst": "greenhouse_butt"}}])
chore("Firewood", "Şömine Odunu", "Firewood",
      [take("woodshed", "Logs", 2.0, "Odunluktan bir demet odun al"),
       bring("$dst", "Logs", 1.5, "Odunu $dst at (3 m'den fırlatmak da sayılır)")],
      variants=[{"name": "hall", "vars": {"dst": "hall_hearth"}},
                {"name": "library", "vars": {"dst": "library_fire"}}])
chore("Chandelier", "Avizeyi Yak", "Light the chandelier",
      [work("$winch", 4.0, "Çıkrığı çevir, avize salona iner"),
       work("hall_chandelier", 3.0, "Salonda avizenin mumlarını yak; avize ışıl ışıl geri yükselir")],
      variants=[{"name": "clock", "vars": {"winch": "clock_winch"}},
                {"name": "gallery", "vars": {"winch": "gallery_cleat"}}],
      twist="everyone in the hall sees the chandelier come down and the name of whoever lights it")
chore("Shutters", "Kepenkleri Kapat", "Close the shutters",
      [work(["$w1", "$w2", "$w3"], 2.0, "Çarpan 3 kepengi kapat ve mandalla", any_order=True)],
      variants=[{"name": "west", "vars": {"w1": "win_blue", "w2": "win_red", "w3": "win_studio"}},
                {"name": "front", "vars": {"w1": "win_nursery", "w2": "win_billiard", "w3": "win_ballroom"}},
                # (SPRINT-040: the master bedroom's windows now face the east guest wing: the studio instead)
                {"name": "east", "vars": {"w1": "win_studio", "w2": "win_nursery", "w3": "win_billiard"}}],
      twist="the Impatient can throw a shutter open again (noise cover 25 m, rain wets the floor = footprints)")
chore("SignalLamp", "Sinyal Feneri", "Signal lamp",
      [take("attic_oil", "OilCan", 2.0, "Tavan arasından bir yağ bidonu al"),
       bring("tower_lamp", "OilCan", 3.0, "Çatı yolundan geç, kuleye tırman, feneri doldur: ışık denize uzanır")],
      twist="the beam sweeps the courtyard and the sea stair every 8 s for the rest of the day")
chore("PozzosSupper", "Pozzo'nun Akşam Yemeği", "Pozzo's supper",
      [take("kitchen_pass", "Tray", 2.0, "Servis tezgâhından tepsiyi al"),
       bring("$dst", "Tray", 1.5, "Tepsiyi Pozzo'ya götür: $dst")],
      variants=[{"name": "study", "vars": {"dst": "letter_study"}}, {"name": "bedroom", "vars": {"dst": "letter_master"}}])
chore("GuestLetters", "Misafir Mektupları", "Guest letters",
      [take("post_desk", "Letters", 2.0, "Posta masasından mühürlü mektupları al"),
       bring(["$m1", "$m2", "$m3"], "Letters", 1.0, "Her mektubu doğru kapının altından at", any_order=True)],
      variants=[{"name": "family", "vars": {"m1": "letter_study", "m2": "letter_nursery", "m3": "letter_billiard"}},
                {"name": "guests", "vars": {"m1": "letter_blue", "m2": "letter_red", "m3": "letter_master"}}])
chore("BailTheBoat", "Kayığı Boşalt", "Bail the boat",
      [work("boat_pump", 2.0, "Sintine pompasını 3 kez bas", repeat=3),
       work("boat_rope", 2.0, "Kayığın halatını sıkıca bağla")],
      twist="a wave washes in every 20 s: stand on the slip and you get soaked (wet footprints for 60 s)")
chore("PotsIndoors", "Saksıları İçeri Al", "Pots indoors",
      [take("courtyard_pots", "Pot", 2.0, "Fırtınada devrilen bir limon saksısını kaldır"),
       bring("greenhouse_bench", "Pot", 1.5, "Saksıyı seradaki tezgâha koy")])
chore("SparkJar", "Kıvılcım Kavanozu", "Spark jar",
      [take("tower_jars", "Jar", 2.0, "Kulede şimşekle dolan bir kavanoz al"),
       bring("spark_board", "Jar", 2.0, "Kavanozu mahzendeki kıvılcım panosuna tak (koşarsan boşalır)")],
      twist="a delivered jar ends a running power outage at once; running drains it, like water")
chore("MusicBox", "Müzik Kutusu", "Music box",
      [take("nursery_cylinder", "Cylinder", 1.5, "Çocuk odasından müzik silindirini al"),
       bring("orchestrion", "Cylinder", 2.0, "Silindiri balo salonundaki orkestriyona tak")],
      twist="the orchestrion plays for 40 s: heard 30 m, it masks footsteps and kill sounds in the ballroom")
chore("HangTheSheets", "Çamaşırları As", "Hang the sheets",
      [take("laundry_tub", "Sheets", 2.0, "Çamaşırhaneden ıslak çarşaf sepetini al"),
       bring("$dst", "Sheets", 3.0, "Çarşafları ipe as (asılı çarşaflar görüşü keser)")],
      variants=[{"name": "attic", "vars": {"dst": "attic_lines"}}, {"name": "bath", "vars": {"dst": "bath_rack"}}])
chore("HangThePortrait", "Portreyi As", "Hang the portrait",
      [take("studio_easel", "Portrait", 2.0, "Atölyeden bitmiş portreyi al"),
       bring("gallery_frame", "Portrait", 2.0, "Galerideki boş çerçeveye as")],
      twist="the portrait shows the face of a random living player: the Impatient can swap it for a framed one")
chore("LostBook", "Kayıp Kitap", "The lost book",
      [take("billiard_book", "Book", 1.5, "Bilardo odasında unutulan kitabı al"),
       bring("library_shelf", "Book", 1.5, "Kütüphanedeki boş rafa koy")])
chore("ClockAndBell", "Saat ve Çan", "Clock and bell",
      [work("great_clock", 4.0, "Büyük saati kur"),
       work("chapel_bell", 3.0, "Şapelde çan ipini çek: bütün malikâne duyar")])
chore("GParcel", "G. Kolisi", "The G. parcel",
      [take("g_parcel", "Parcel", 2.0, "Kayıkhanede denizden vuran G. kolisini kaldır"),
       bring("parcel_corner", "Parcel", 1.5, "Koliyi giriş holündeki G. köşesine taşı (tek başına yavaş, iki kişiyle hızlı)")],
      twist="two-person carry; opening it is forbidden, and everyone opens it (cosmetic loot)")
chore("GraveLanterns", "Mezar Fenerleri", "Grave lanterns",
      [take("chapel_candles", "Taper", 1.5, "Şapelden yanan bir fitil al"),
       work(["grave_1", "grave_2", "grave_3", "grave_4"], 1.5, "Rüzgârın söndürdüğü 4 mezar fenerini yak",
            any_order=True)],
      twist="the taper blows out on the cliff path if you sprint; the graveyard glows for the night")
chore("SetTheTable", "Sofrayı Kur", "Set the table",
      [take("pantry_china", "Plates", 1.5, "Kilerdeki porselen dolabından tabakları al"),
       bring("dining_table", "Plates", 2.0, "Ziyafet masasına diz (koşarsan tabak kırılır)")])

# ---- SPRINT-040 manor-only chores (the new wings; SPRINT-016 "simple but fun": take / bring / work)
chore("WindTheClocks", "Büyük Saatleri Kur", "Wind the grand clocks",
      [work(["clock_long", "clock_stair", "clock_trophy"], 2.5, "Üç büyük saati kur (hepsi aynı dakikada durmuş)",
            any_order=True)],
      twist="the clocks chime together for 5 s when the last one is wound: heard all over the north block")
chore("PolishTheSilver", "Gümüşleri Parlat", "Polish the silver",
      [take("silver_chest", "Silver", 2.0, "Gümüş odasındaki sandıktan takımı al"),
       bring("$dst", "Silver", 3.0, "Gümüşleri parlatıp $dst diz (koşarsan şıngırdar)")],
      variants=[{"name": "dining", "vars": {"dst": "dining_table"}},
                {"name": "servants", "vars": {"dst": "servants_table"}}])
chore("TuneThePiano", "Piyanoyu Akort Et", "Tune the piano",
      [take("music_cabinet", "Fork", 1.5, "Mavi salondaki nota dolabından akort çatalını al"),
       bring("piano", "Fork", 4.0, "Müzik salonundaki kuyruklu piyanoyu akort et")],
      twist="a tuned piano can be played (E): 20 s of music that masks footsteps in the enfilade")
chore("FeedTheAviary", "Kuşhaneyi Besle", "Feed the aviary",
      [take("seed_sack", "Seed", 1.5, "Seradaki çuvaldan kuş yemi al"),
       bring("aviary", "Seed", 2.5, "Portakallıktaki kuşhaneyi besle")],
      twist="fed birds sing; startled birds (someone running past) screech for 3 s")
chore("StokeTheBoiler", "Kazanı Besle", "Stoke the boiler",
      [take("coal_heap", "Coal", 2.0, "Hizmet avlusundaki yığından bir kova kömür al"),
       bring("boiler", "Coal", 2.5, "Batı kanadındaki kazana kömürü at")],
      twist="a stoked boiler steams the bath house for the day (sight 5 m there)")
chore("SetTheChessboard", "Satranç Tahtasını Diz", "Set the chessboard",
      [take("chess_box", "Chessmen", 1.5, "Kart odasındaki kutudan satranç taşlarını al"),
       bring("chessboard", "Chessmen", 3.0, "Oyun odasındaki satranç tahtasını diz")])
chore("SortTheWine", "Şarapları Ayır", "Sort the wine",
      [work(["rack_1", "rack_2", "rack_3"], 2.0, "Katakombdaki üç nişte eski şarapları yıllarına göre diz",
            any_order=True)])
chore("LightTheChapel", "Şapeli Aydınlat", "Light the chapel",
      [take("chapel_candles", "Taper", 1.5, "Şapelin mum kutusundan yanan bir fitil al"),
       work(["chapel_cand_w", "chapel_cand_e"], 1.5, "İki büyük şamdanı yak", any_order=True)],
      twist="a lit chapel shows shadows on the stained glass: who is inside is visible from the hall")
chore("DustThePortraits", "Portrelerin Tozunu Al", "Dust the portraits",
      [work(["portrait_1", "portrait_2", "portrait_3"], 2.0, "Galerilerdeki üç tozlu portreyi sil", any_order=True)],
      twist="one portrait's eyes follow you (the witness portrait trap)")
chore("AirTheGuestRooms", "Misafir Odalarını Havalandır", "Air the guest rooms",
      [work(["win_green", "win_gold", "win_ivory"], 2.0, "Doğu kanadındaki üç pencereyi aç, havalandır, kapat",
            any_order=True)])
chore("MakeTheBeds", "Yatakları Yap", "Make the beds",
      [take("linen_press", "Linen", 2.0, "Dikiş odasındaki dolaptan temiz çarşafları al"),
       bring(["bed_lilac", "bed_green", "bed_rose"], "Linen", 2.0, "Üç misafir yatağını yap", any_order=True)])
chore("HotWater", "Sıcak Su", "Hot water",
      [take("boiler", "Bucket", 2.5, "Kazandan bir kova sıcak su al", fill=1.0),
       bring("bath_e_tub", "Bucket", 1.5, "Misafir hamamının küvetine dök (yürü, koşma!)", min_fill=0.35)])
chore("TheDeedCount", "Tapuları Say", "Count the deeds",
      [work("vault_ledger", 3.0, "Kasa dairesindeki tapuları say"),
       work("ossuary_niche", 2.0, "Kemiklikteki nişe sayım defterini koy")])
chore("KeyRound", "Anahtar Turu", "The key round",
      [take("housekeeper_keys", "Keys", 1.5, "Kâhyanın panosundan anahtar demetini al"),
       bring(["smoking_humidor", "gun_bench", "morning_table"], "Keys", 1.5, "Doğu kanadında üç dolabı kilitle",
             any_order=True)])
chore("DustTheChina", "Porselenlerin Tozunu Al", "Dust the china",
      [work(["vase_chinese", "dressing_mirror", "hall_trophies"], 2.0, "Çin vazosunu, boy aynasını ve trofeleri sil",
            any_order=True)])
chore("AfternoonTea", "İkindi Çayı", "Afternoon tea",
      [take("kitchen_pass", "TeaTray", 2.0, "Mutfaktaki servis tezgâhından çay tepsisini al"),
       bring(["tea_green", "tea_yellow"], "TeaTray", 1.5, "Yeşil ve sarı salonlara çay bırak", any_order=True)],
      twist="the enfilade: carry the tray through the salons without running (fragile)")

# ---- SPRINT-040 SECRET chores: never dealt; given to whoever discovers their secret. Reward = map knowledge
# (reward_secret: another passage opens for everyone = a shortcut) or a clue (reward_compartment opens for everyone).
chore("CryptRelic", "Kriptanın Röliği", "The crypt relic",
      [take("crypt_relic", "Relic", 2.0, "Kriptadaki röliği al"),
       bring("chapel_reliquary", "Relic", 2.0, "Röliği şapeldeki boş rölikerliğe koy")])
CHORES[-1].update(secret="S8", reward_secret="S2")
chore("StarChart", "Yıldız Haritası", "The star chart",
      [take("obs_scope", "Chart", 3.0, "Teleskoptan fırtına yıldızlarını haritaya geçir"),
       bring("map_table", "Chart", 2.0, "Haritayı harita odasındaki masaya bırak")])
CHORES[-1].update(secret="S10", reward_compartment="K9")
chore("SmugglersLedger", "Kaçakçı Defteri", "The smugglers' ledger",
      [take("smuggler_ledger", "Ledger", 1.5, "Sahte şarap rafının arkasındaki defteri al"),
       bring("study_safe", "Ledger", 2.0, "Defteri Pozzo'nun kasasına kilitle")])
CHORES[-1].update(secret="S4", reward_secret="S7")
chore("LibraryCipher", "Kırmızı Kitabın Şifresi", "The red book's cipher",
      [take("red_book", "RedBook", 1.5, "Dönen kitaplığı açan kırmızı kitabı al"),
       bring("study_safe", "RedBook", 2.0, "Kitaptaki şifreyi kasada dene")])
CHORES[-1].update(secret="S1", reward_compartment="K4")

# ------------------------------------------------------------------------------------------------ hides + evidence
HIDES = {
    "great_hall": [((-9, -19), "under the dais curtain")],
    "vestibule": [((-8.5, 8.5), "cloak closet (Vestiyer)")],
    "dining": [((-21, -7), "sideboard cabinet")],
    "kitchen": [((-33, -9.5), "under the long table")],
    "pantry": [((-33, 3), "behind the sack pile")],
    "laundry": [((-11, 3), "linen hamper")],
    "chapel": [((21, -11), "confessional")],
    "library": [((33, -11), "window-seat curtain")],
    "ballroom": [((11.5, 5), "behind the dust sheets"), ((33, 5), "stage curtain")],
    "wine_cellar": [((-33, -1), "empty barrel")],
    "spark_room": [((-7, -5), "behind the jar racks")],
    "cistern": [((13, -17), "between the columns (knee-deep)")],
    "gallery": [((9, -1), "behind a bust plinth")],
    "blue_room": [((-23, -23), "wardrobe")],
    "bath": [((-11, -13), "behind the steam screen")],
    "red_room": [((-33, 3), "wardrobe")],
    "studio": [((-11, -7), "under a canvas sheet")],
    "nursery": [((9, 7), "wardrobe (secret S6)")],
    "study": [((11, -23), "behind the map cabinet")],
    "master": [((23, -23), "under the four-poster")],
    "billiard": [((21, 5), "drinks cabinet alcove")],
    "attic": [((-33, -23), "trunk"), ((-12, -5), "dust-sheet heap")],
    "clock_room": [((9, -11), "inside the clock case")],
    "storm_tower": [((27, -17), "under the stair")],
    "boathouse": [((10, 49), "under the upturned boat")],
    "greenhouse": [((49, 1), "behind the palms")],
    "graveyard": [((-51, 25), "open grave")],
    "courtyard": [((33, 33), "behind the statue plinth")],
    "service_yard": [((-33, 33), "behind the woodshed wall")],
}
EVIDENCE = {
    "great_hall": "ash from the hearth on shoes; chandelier state",
    "vestibule": "wet coats and umbrellas count who came in from outside",
    "dining": "place cards moved; wine stains",
    "kitchen": "knife block (a missing knife is visible)",
    "pantry": "flour floor: footprints 120 s",
    "laundry": "wash tub turns red after a wash (45 s)",
    "chapel": "candle count (the taper chore), bell rope sway",
    "library": "books pulled out: the S1 bookcase stays ajar 20 s",
    "ballroom": "polished floor shows wet footprints from the French doors",
    "wine_cellar": "dust floor: footprints 120 s",
    "spark_room": "soot on the hands of whoever pulled the switch (45 s)",
    "cistern": "wet trousers to the knee (60 s); ripples",
    "gallery": "the portrait peephole: the eyes move when someone is behind",
    "blue_room": "unmade bed, letters under the door",
    "bath": "wet footprints; the tub is a wash spot (visible)",
    "red_room": "letters under the door; open shutter",
    "studio": "paint floor: coloured footprints 90 s",
    "nursery": "the rocking horse keeps rocking 10 s after someone passes",
    "study": "the safe log (who opened it)",
    "master": "spiral-stair door creaks (12 m)",
    "billiard": "ball positions change; cue missing = weapon",
    "attic": "dust floor: footprints 120 s",
    "clock_room": "the clock stops while someone is inside the case",
    "storm_tower": "empty jar slots show who took one",
    "boathouse": "wet footprints; the boat is bailed or not",
    "greenhouse": "mud footprints; cracked pane",
    "graveyard": "mud footprints; fresh earth on the open grave",
    "courtyard": "mud footprints; lemon pots",
    "service_yard": "mud footprints; chopping block (axe = weapon)",
}
# SPRINT-040 rooms
HIDES.update({
    "music_room": [((-33, -39), "behind the harp's cover")], "green_salon": [((-23, -39), "behind the palm screen")],
    "blue_salon": [((-15, -25), "curtained window seat")], "staircase_hall": [((7, -39), "under the stair")],
    "yellow_salon": [((15, -39), "behind the harpsichord")], "card_room": [((17, -25), "curtained alcove")],
    "trophy_room": [((33, -25), "behind the bear")], "servants_hall": [((-53, -43), "coal cupboard")],
    "housekeeper": [((-53, -25), "the bed curtain")], "silver_room": [((-39, -43), "plate cupboard")],
    "boiler_room": [((-43, -33), "behind the boiler")], "smoking_room": [((45, -43), "chesterfield alcove")],
    "gun_room": [((53, -43), "boot cupboard")], "map_room": [((45, -25), "chart chest")],
    "games_room": [((53, -25), "behind the dart board screen")], "orangery": [((39, -23), "among the orange tubs")],
    "morning_room": [((53, -1), "behind the sofa")], "lilac_room": [((-33, -39), "wardrobe")],
    "sewing_room": [((-23, -39), "among the dress forms")], "chinese_room": [((23, -39), "lacquer screen")],
    "dressing_room": [((25, -25), "wardrobe")], "picture_gallery": [((-53, -25), "behind a velvet curtain")],
    "green_room": [((45, -43), "wardrobe")], "gold_room": [((47, -43), "wardrobe")],
    "rose_room": [((45, -25), "wardrobe")], "ivory_room": [((53, -25), "wardrobe")],
    "guest_bath": [((53, -23), "behind the screen")], "wine_catacombs": [((-33, -1), "empty niche")],
    "vault": [((-7, -15), "behind the iron chests")], "ossuary": [((5, -15), "bone niche")],
    "crypt": [((21, -11), "behind a sarcophagus")], "observatory": [((47, -37), "under the cot")],
})
EVIDENCE.update({
    "music_room": "the piano lid shows fresh fingerprints; the metronome still ticking",
    "green_salon": "tea cups still warm (someone was here < 60 s ago)",
    "blue_salon": "a card game abandoned mid-hand",
    "staircase_hall": "the chandelier's state; footprints on the stone stairs",
    "yellow_salon": "the harpsichord's dust shows who touched it",
    "card_room": "soot on the hearth plate (S7 used)",
    "trophy_room": "a missing rifle from the rack; the door that locks",
    "servants_hall": "the bell board shows the last room that rang",
    "housekeeper": "the key board: a missing key is visible",
    "silver_room": "a missing knife from the silver canteen",
    "boiler_room": "coal dust on shoes (45 s)",
    "smoking_room": "fresh ash in the tray",
    "gun_room": "a gun cabinet ajar",
    "map_room": "the star chart (secret chore) on the table",
    "games_room": "the chess position changes",
    "orangery": "wet soil footprints; the birds screech when startled",
    "morning_room": "the breakfast is half eaten",
    "lilac_room": "unmade bed", "sewing_room": "a pair of shears missing (weapon)",
    "chinese_room": "a vase moved: its dust ring", "dressing_room": "a wardrobe door ajar (S7)",
    "picture_gallery": "the watching portrait remembers who passed",
    "green_room": "the door that locks itself", "gold_room": "a loose panel beside the bed (S10)",
    "rose_room": "unmade bed", "ivory_room": "letters under the door", "guest_bath": "wet footprints",
    "wine_catacombs": "dust floor: footprints 120 s", "vault": "the floor safe's dial position",
    "ossuary": "dust floor: footprints 120 s", "crypt": "the relic's empty place", "observatory": "the telescope's aim",
})
# ------------------------------------------------------------------------------------------------ SPRINT-040 compartments
# Hidden compartments (AKGHiddenCompartment): E on the odd brick / drawer / book opens it for everyone (replicated);
# loot spills out (a KGLoot table) or a clue stays readable in it (evidence). K4 and K9 are also opened by the
# rewards of the secret chores LibraryCipher and StarChart.
COMPARTMENTS = []


def compartment(cid, rid, at, kind, prompt, loot=None, clue=None, yaw=0.0):
    COMPARTMENTS.append({"id": cid, "room": rid, "at": list(at), "kind": kind, "prompt": prompt, "loot": loot,
                         "clue": clue, "yaw": yaw})


compartment("K1", "wine_catacombs", (-33.4, -9.0), "LooseBrick", "Tap the loose brick",
            clue="A torn label in the bricks: 'G. - 1893 - not for the committee'.")
compartment("K2", "study", (13.0, -23.4), "FalseDrawer", "Pull the desk drawer all the way out", loot="Chest")
compartment("K3", "library", (31.0, -23.4), "HollowBook", "Open the heavy atlas",
            clue="Pages cut into a hollow. Inside: a list of the guests' rooms, one name circled in red.")
compartment("K4", "vault", (-12.0, -8.0), "FloorSafe", "Try the floor safe's dial", loot="CryptVault")
compartment("K5", "kitchen", (-33.4, -15.0), "LooseBrick", "Tap the loose brick by the range", loot="Pot")
compartment("K6", "housekeeper", (-45.0, -33.4), "FalseDrawer", "Pull the ledger drawer",
            clue="The housekeeper's ledger: one guest has asked for the key to the gun room twice.")
compartment("K7", "chinese_room", (9.0, -38.0), "LacquerBox", "Slide the lacquer box's false bottom", loot="Pot")
compartment("K8", "master", (24.0, -22.8), "FloorSafe", "Lift the board under the rug", loot="Chest")
compartment("K9", "smoking_room", (39.0, -43.4), "WallSafe", "Swing the painting aside",
            clue="A letter: 'The boat will not come while the storm holds. Wait. - G.'")
compartment("K10", "sewing_room", (-23.0, -25.0), "SewingBox", "Lift the sewing box's tray",
            clue="A scrap of cloth with a dark stain, cut from a coat sleeve.")
compartment("K11", "ossuary", (0.0, -4.8), "LooseBrick", "Push the skull in the wall", loot="CryptUrn")
compartment("K12", "green_room", (45.2, -36.0), "FalseDrawer", "Pull the writing desk's drawer", loot="Pot")

# ------------------------------------------------------------------------------------------------ SPRINT-040 traps
# The generic trap framework (Source/KillGodot/Traps/): arm -> telegraph -> trigger -> cooldown, event log hook.
# Every trap is seen or heard before it hurts; none kills outright. The Impatient arm the sabotage ones (E, then a
# 60 s personal cooldown); the alarm floors and the watching portraits are passive (always armed).
TRAPS = []


def trap(tid, effect, rid, at, zone, policy="Impatient", passive=False, telegraph=2.0, active=1.0, cooldown=45.0,
         damage=0.0, radius=600.0, target=None, target_room=None, note=""):
    TRAPS.append({"id": tid, "effect": effect, "room": rid, "at": list(at), "zone": list(zone), "policy": policy,
                  "passive": passive, "telegraph_s": telegraph, "active_s": active, "cooldown_s": cooldown,
                  "damage": damage, "radius_cm": radius, "target": list(target) if target else None,
                  "target_room": target_room, "note": note})


trap("T1", "Trapdoor", "dining", (-20.5, -8.5), (1.0, 1.0), telegraph=1.2, active=3.0, damage=10.0,
     target=(-26.0, -10.0), target_room="wine_cellar", note="the boards groan and sag 1.2 s before they drop")
trap("T2", "Trapdoor", "ballroom", (13.0, -3.0), (1.0, 1.0), telegraph=1.2, active=3.0, damage=10.0,
     target=(10.5, -7.0), target_room="cistern", note="drops into the cistern (knee-deep water)")
trap("T3", "FallingObject", "ballroom", (24.0, -2.0), (1.6, 1.6), telegraph=2.5, active=4.0, cooldown=90.0,
     damage=40.0, note="the ballroom chandelier sways and creaks 2.5 s, then falls")
trap("T4", "FallingObject", "staircase_hall", (0.0, -35.0), (1.6, 1.6), telegraph=2.5, active=4.0, cooldown=90.0,
     damage=40.0, note="the great chandelier of the staircase hall")
trap("T5", "LockDoors", "trophy_room", (29.0, -32.0), (4.6, 7.6), telegraph=1.0, active=20.0, cooldown=90.0,
     note="the trophy room seals for 20 s (clunk of every lock first)")
trap("T6", "LockDoors", "green_room", (42.0, -39.0), (3.6, 4.6), telegraph=1.0, active=20.0, cooldown=90.0,
     note="the green guest room seals for 20 s")
trap("T7", "LightsOut", "long_gallery", (0.0, -42.0), (30.0, 1.6), telegraph=1.5, active=25.0, cooldown=90.0,
     radius=3600.0, note="the gallery's gas lamps flicker, then go out for 25 s")
trap("T8", "LightsOut", "servants_hall", (-49.0, -39.0), (4.6, 4.6), telegraph=1.5, active=25.0, cooldown=90.0,
     radius=1600.0, note="the servants' hall lamps go out")
trap("T9", "Alarm", "long_gallery", (-18.0, -42.0), (1.0, 1.6), policy="None", passive=True, telegraph=0.3,
     active=8.0, cooldown=20.0, note="a creaking board: pings everyone's minimap for 8 s")
trap("T10", "Alarm", "east_hall", (36.0, -24.0), (1.6, 1.0), policy="None", passive=True, telegraph=0.3,
     active=8.0, cooldown=20.0, note="a creaking board in the east wing hall")
trap("T11", "Alarm", "north_corridor", (12.0, -42.0), (1.0, 1.6), policy="None", passive=True, telegraph=0.3,
     active=8.0, cooldown=20.0, note="a creaking board on the guest floor")
trap("T12", "Witness", "gallery", (-9.6, -12.0), (0.5, 0.5), policy="None", passive=True, radius=900.0,
     note="Anselm's portrait: its eyes remember who passed")
trap("T13", "Witness", "picture_gallery", (-53.6, -34.0), (0.5, 0.5), policy="None", passive=True, radius=1100.0,
     note="the Pozzo matriarch's portrait watches the picture gallery")
trap("T14", "Witness", "long_gallery", (0.0, -43.6), (0.5, 0.5), policy="None", passive=True, radius=1400.0,
     note="the founder's portrait at the middle of the long gallery")

for rid, lst in HIDES.items():
    R[rid]["hides"] = [{"at": list(p), "what": w} for p, w in lst]
for rid, ev in EVIDENCE.items():
    R[rid]["evidence"] = [ev]


# ------------------------------------------------------------------------------------------------ windows
def _edges(poly):
    n = len(poly)
    for i in range(n):
        yield poly[i], poly[(i + 1) % n]


def _on_seg(p, a, b, tol=0.05):
    (x, y), (x0, y0), (x1, y1) = p, a, b
    if abs(x0 - x1) < 1e-6:
        return abs(x - x0) < tol and min(y0, y1) - tol <= y <= max(y0, y1) + tol
    return abs(y - y0) < tol and min(x0, x1) - tol <= x <= max(x0, x1) + tol


def _shared(p, rid, floor):
    """True when p (on rid's wall) is also on the wall of another indoor room of the same floor."""
    for o in ROOMS:
        if o["id"] == rid or o["floor"] != floor or o["kind"] in ("grounds", "outdoor"):
            continue
        if any(_on_seg(p, a, b) for a, b in _edges(o["poly"])):
            return True
    return False


def math_dist(p, q):
    return ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2) ** 0.5


WINDOWS = []
NO_WINDOWS = {"servants_corridor", "spark_room", "wine_cellar", "cistern", "smugglers_tunnel", "pantry",
              "clock_room"} | {r["id"] for r in ROOMS if r["floor"] == "C2"}
for r in ROOMS:
    if r["kind"] in ("grounds", "outdoor") and r["id"] not in ("greenhouse", "boathouse"):
        continue
    if r["id"] in NO_WINDOWS:
        continue
    for a, b in _edges(r["poly"]):
        L = abs(b[0] - a[0]) + abs(b[1] - a[1])
        vertical = abs(a[0] - b[0]) < 1e-6
        k = 0
        s = 2.0
        while s < L - 1.0:
            t = s / L
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            s += 4.0
            if _shared(p, r["id"], r["floor"]):
                continue
            if r["floor"] == "F1" and any(_on_seg(p, va, vb) for va, vb in _edges(R["great_hall"]["void"])):
                continue    # balustrade over the hall void, not a wall
            if any(math_dist(p, d["at"]) < 1.8 for d in DOORS if r["id"] in (d["a"], d["b"])):
                continue
            h = zlib.crc32(f"{r['id']}{p[0]:.1f}{p[1]:.1f}".encode()) % 5
            WINDOWS.append({"room": r["id"], "floor": r["floor"], "at": [round(p[0], 2), round(p[1], 2)],
                            "axis": "v" if vertical else "h", "kind": "exterior", "rattle": h == 0})
            k += 1
WINDOWS.append({"room": "nursery", "floor": "F1", "at": [0.0, 0.0], "axis": "h", "kind": "interior",
                "rattle": False, "note": "the nursery window over the Great Hall"})


# ------------------------------------------------------------------------------------------------ the rest
CHOKEPOINTS = [
    {"floor": "F0", "at": [0, -22], "name_tr": "Uşak Koridoru (44 m, 4 m geniş, ortası karanlık)"},
    {"floor": "C", "at": [-28, 20], "name_tr": "Kaçakçı Tüneli (4 m, 50 m)"},
    {"floor": "C", "at": [0, 36], "name_tr": "Rıhtım Merdiveni"},
    {"floor": "F0", "at": [-37, -10], "name_tr": "Kayalık Patika (6 m, açık)"},
    {"floor": "F2", "at": [18, -20], "name_tr": "Çatı Yolu (4 m, açık)"},
    {"floor": "F1", "at": [-8, -6], "name_tr": "Büyük Merdivenler (2 m)"},
    {"floor": "F3", "at": [28, -22], "name_tr": "Kule el merdiveni (tek giriş)"},
]
SPAWN = {"room": "great_hall", "center": [0.0, -9.0], "radius": 6.0, "count": 20, "face": "centre"}
MEETING = {"room": "great_hall", "at": [0.0, -9.0], "gather_s_max": 30.0, "gather_window_s": 35.0,
           "note": "the long table; the empty chair at its head is Godot's"}
STORM = {
    "lightning": {"interval_s": [20, 45], "flash_s": 0.35, "seeded": "FKGRng at dawn; the day's storm strength "
                  "(1-3) is announced at dawn", "effect": "rooms with exterior windows and the outdoors get "
                  "KG_SIGHT_DAY for the flash (unlit rooms: 5 m -> 30 m)",
                  "thunder_delay_s": 1.5, "thunder_hear_mul": 0.5, "thunder_s": 2.0},
    "outage": {"name_tr": "Işık Kesintisi", "who": "Impatient (sabotage verb at the spark board or any lamp "
               "junction)", "cooldown": "once per day, never in the first 90 s of a day",
               "effect": "all manor lamps off: sight 5 m except hearths, candles, lightning; the grounds stay "
                         "lit by lightning only",
               "fix": "hold the spark board switch (cellar) and the spark coil (clock room) at the same time "
                      "for 3 s, or deliver a charged jar (SparkJar chore) to the board",
               "fix_spots": ["spark_board", "fuse_coil"], "trace": "soot on the saboteur's hands for 45 s",
               "auto_end_s": 60},
    "rattle": "seeded rattle windows bang open (sound 25 m, masks footsteps 6 m around), rain wets the floor under "
              "them (footprints 60 s); any player closes one with E (2 s)",
    "gusts": "the front door and the French doors slam on gusts (sound 25 m), not deadly",
    "sea": "the boathouse slip is washed every 20 s (wet 60 s)",
}
WINGS = {
    "hall": "Büyük Salon ve giriş", "west": "Batı kanadı (hizmet)", "east": "Doğu kanadı (tören)",
    "service": "Uşak koridoru", "cellar": "Mahzen", "guest": "Misafir katı", "master": "Efendi dairesi",
    "upper": "Çatı ve kule", "grounds": "Bahçe ve kıyı",
    # SPRINT-040 (the old "west"/"east" keys name the old house's halves; the new wings use the same words)
    "north": "Kuzey blok (merdiven holü, salonlar, uzun galeri)", "deep": "Alt mahzen",
}
# Region Gates at build time: a door between two areas with different min_n is a wing gate (locked while fewer
# players are in the lobby, AKGDoor tags KG_WingGate + KG_MinN_<n>); a chore opens at the highest min_n of its
# rooms (every variant), so a 6-player match is never dealt a chore behind a locked door.
AN_ROOM = {a["id"]: a["room"] for a in ANCHORS}


def _chore_rooms(ch):
    out = set()
    for v in ch.get("variants") or [{"vars": {}}]:
        for st in ch["steps"]:
            for a in (st["at"] if isinstance(st["at"], list) else [st["at"]]):
                a = v["vars"].get(a[1:], a) if a.startswith("$") else a
                out.add(AN_ROOM[a])
    return out


for _c in CHORES:
    _c["min_players"] = max(R[r]["min_n"] for r in _chore_rooms(_c))
for _d in DOORS:
    na, nb = R[_d["a"]]["min_n"], R[_d["b"]]["min_n"]
    if na != nb:
        _d["gate_min_n"] = max(na, nb)

L = {
    "_doc": "Storm Manor (map 2) layout, SPRINT-017 design. Generated by Tools/Level/author_stormmanor.py - do not "
            "hand-edit. Metres, UE axes (x east, +y south). Floors 3 m apart on the 2 m kit grid. Validated by "
            "validate_stormmanor.py, drawn by render_stormmanor.py. Plan: Docs/Level/StormManor_Plan.md.",
    "_schema": {
        "rooms": "id, floor, poly (m), kind room|circulation|outdoor|grounds, counts (named room of the brief), "
                 "wing, risk güvenli|orta|tehlikeli, min_n (Region Gate), surface (footprint material), "
                 "hides[], evidence[], storeys/void (double-height rooms)",
        "doors": "a, b rooms on the same floor, at = the door centre on their shared wall, width m",
        "stairs": "lower -> upper room, bottom/top points (m), kind grand|service|spiral|ladder|outdoor",
        "secrets": "vent-like passages: a/b rooms and points; never used for connectivity",
        "chores": "SPRINT-016 steps: take | bring | work, at = anchor id (a list = any order), $vars from variants",
    },
    "version": 1,
    "map": "L_StormManor",
    "kit": {"cell_m": 2.0, "storey_m": 3.0, "stair_run_per_rise": 1.2833, "walls": "Quaternius village kit "
            "(UnevenBrick outside, Plaster inside)"},
    "floors": FLOORS,
    "wings": WINGS,
    "rooms": ROOMS,
    "doors": DOORS,
    "stairs": STAIRS,
    "secrets": SECRETS,
    "secret_rules": SECRET_RULES,
    "windows": WINDOWS,
    "anchors": ANCHORS,
    "items": ITEMS,
    "chores": CHORES,
    "compartments": COMPARTMENTS,
    "traps": TRAPS,
    "chokepoints": CHOKEPOINTS,
    "spawn": SPAWN,
    "meeting": MEETING,
    "storm": STORM,
    "speeds": {"run": 5.8, "walk_carry": 3.2, "climb": 2.6, "stair_mul": 1.18},
}

if __name__ == "__main__":
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(L, f, ensure_ascii=False, indent=1)
    n_counted = sum(1 for r in ROOMS if r["counts"])
    print(f"wrote {OUT}: {len(ROOMS)} areas ({n_counted} named rooms), {len(DOORS)} doors, {len(STAIRS)} stairs, "
          f"{len(SECRETS)} secrets, {len(WINDOWS)} windows, {len(CHORES)} chores, {len(ANCHORS)} anchors, "
          f"{len(COMPARTMENTS)} compartments, {len(TRAPS)} traps")
