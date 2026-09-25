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
]


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
]
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
anchor("win_master", "master", (33, -20), "yatak odası penceresi", "Shutter", r=1.2)
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
anchor("win_ballroom", "ballroom", (33, -5), "balo salonu penceresi", "Shutter", r=1.2)
anchor("courtyard_pots", "courtyard", (24, 24), "limon saksıları", "Pots")
anchor("greenhouse_bench", "greenhouse", (44, 14), "sera tezgâhı", "Bench")
anchor("grave_1", "graveyard", (-48, 9), "mezar feneri 1", "GraveLamp", r=1.2)
anchor("grave_2", "graveyard", (-40, 11), "mezar feneri 2", "GraveLamp", r=1.2)
anchor("grave_3", "graveyard", (-44, 17), "mezar feneri 3", "GraveLamp", r=1.2)
anchor("grave_4", "graveyard", (-38, 23), "mezar feneri 4", "GraveLamp", r=1.2)
anchor("fuse_coil", "clock_room", (6, -12), "kıvılcım bobini (ikinci şalter)", "SparkCoil")

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
                {"name": "east", "vars": {"w1": "win_nursery", "w2": "win_billiard", "w3": "win_master"}}],
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
              "clock_room"}
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
}

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
          f"{len(SECRETS)} secrets, {len(WINDOWS)} windows, {len(CHORES)} chores, {len(ANCHORS)} anchors")
