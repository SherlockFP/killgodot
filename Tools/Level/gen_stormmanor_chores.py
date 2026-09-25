"""SPRINT-018: the 19 Storm Manor chores as world-chore data (the SPRINT-016 format of morrowmere_world_chores.json).

  python Tools/Level/gen_stormmanor_chores.py

Source of truth: the anchors and chores of Tools/Level/stormmanor_layout.json (SPRINT-017 design, Turkish labels) with
the build fixes of stormmanor_geo.py (Z0, moved anchors). This script adds what the engine needs and the design leaves
open: English labels, the item meshes, the anchor looks the C++ already knows (Well, Butt, Woodbox, Bell, Winder, Lamp,
TaperBox, OilShelf, Satchel, Door, CrateStack, ChopBlock, BreadRack, LighthouseLamp; other kinds get the builder's own
props and a plain E box), heights per floor, the ladder climb to the tower top.
Writes (same pair as gen_world_chores.py, one catalog per map in C++):
  Tools/Level/stormmanor_world_chores.json            authoring copy (readable)
  Tools/Level/stormmanor_world_chores.resolved.json   dev builds read it (kg.WorldChore.Reload), no recompile
  Source/KillGodot/Chores/WorldChores/KGWorldChoreData_StormManor.gen.inl   embedded copy for every build
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stormmanor_geo as G  # noqa: E402

ROOT = G.ROOT
OUT_SRC = os.path.join(ROOT, "Tools", "Level", "stormmanor_world_chores.json")
OUT_JSON = os.path.join(ROOT, "Tools", "Level", "stormmanor_world_chores.resolved.json")
OUT_INL = os.path.join(ROOT, "Source", "KillGodot", "Chores", "WorldChores", "KGWorldChoreData_StormManor.gen.inl")
SIZES = os.path.join(ROOT, "Tools", "Unreal", "dressing", "v2", "mesh_sizes.json")
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_"
DV = "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_"
K = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_"

# item -> (mesh, scale, mass kg, extra flags); box / mesh_z come from the mesh size (pivot at the bottom)
ITEMS = {
    "Bottles": (P + "FarmCrate_Empty", 0.8, 6, {"label": "wine basket"}),
    "Bucket": (P + "Bucket_Wooden_1", 1.0, 7, {"liquid": True, "label": "bucket"}),
    "Logs": (IP + "Firewood", 0.9, 14, {"speed": 0.85, "label": "log bundle"}),
    "Plates": (P + "Table_Plate", [1.4, 1.4, 5.0], 4, {"label": "stack of china"}),
    "Tray": (K + "Plate", [2.0, 2.0, 2.0], 3, {"label": "supper tray", "tint": [0.82, 0.70, 0.45]}),
    "Letters": (P + "Book_5", 1.2, 1, {"count": 3, "label": "sealed letters"}),
    "OilCan": (P + "Bottle_1", [2.2, 2.2, 1.4], 5, {"speed": 0.9, "tint": [0.55, 0.38, 0.16], "label": "oil can"}),
    "Jar": (P + "Potion_2", [2.2, 2.2, 1.8], 3, {"label": "charged Leyden jar", "tint": [0.45, 0.75, 1.0]}),
    "Sheets": (P + "Bag", 0.7, 9, {"speed": 0.85, "label": "basket of wet sheets", "tint": [0.95, 0.95, 0.92]}),
    "Portrait": (P + "Shield_Wooden", 1.0, 5, {"speed": 0.85, "label": "framed portrait"}),
    "Cylinder": (P + "Bottle_1", [2.6, 2.6, 0.9], 2, {"label": "music-box cylinder", "tint": [0.85, 0.65, 0.25]}),
    "Book": (P + "Book_7", 1.3, 1, {"label": "lost book"}),
    "Pot": (DV + "Planter_Pot", 0.55, 16, {"speed": 0.7, "label": "lemon pot"}),
    "Parcel": (P + "Crate_Wooden", 0.62, 32, {"speed": 0.55, "two_person": True, "label": "G. parcel"}),
    "Taper": (P + "Candle_2", [1.3, 1.3, 3.6], 1, {"flame": True, "label": "lit taper"}),
}

# anchor id -> (engine kind, English label, extra)
ANCHORS = {
    "hall_table": ("Table", "the decanter on the long table", {"sabotage": "poison"}),
    "hall_hearth": ("Woodbox", "the Great Hall hearth", {}),
    "hall_chandelier": ("Chandelier", "the lowered chandelier", {}),
    "wine_rack": ("WineRack", "the wine racks", {}),
    "cistern_well": ("Well", "the cistern windlass", {}),
    "kitchen_kettle": ("Butt", "the kitchen copper", {"sabotage": "poison"}),
    "kitchen_pass": ("BreadRack", "the servery hatch", {}),
    "dining_table": ("Table", "the banquet table", {}),
    "bath_boiler": ("Butt", "the bath boiler", {"sabotage": "poison"}),
    "greenhouse_butt": ("Butt", "the greenhouse water butt", {"sabotage": "poison"}),
    "woodshed": ("ChopBlock", "the woodshed", {}),
    "library_fire": ("Woodbox", "the library fireplace", {}),
    "chapel_bell": ("Bell", "the chapel bell rope", {}),
    "chapel_candles": ("TaperBox", "the chapel candle box", {}),
    "great_clock": ("Winder", "the great clock's winding key", {}),
    "clock_winch": ("Winder", "the chandelier winch (clock room)", {}),
    "attic_oil": ("OilShelf", "the lamp-oil chest in the attic", {}),
    "attic_lines": ("Lines", "the attic drying lines", {}),
    "tower_lamp": ("LighthouseLamp", "the signal lamp (tower top)", {"sabotage": "snuff"}),
    "tower_jars": ("JarRack", "the lightning-rod jar rack", {}),
    "spark_board": ("SparkBoard", "the spark board (cellar)", {}),
    "post_desk": ("Satchel", "the post desk", {}),
    "letter_blue": ("Door", "the Blue Room", {}),
    "letter_red": ("Door", "the Red Room", {}),
    "letter_master": ("Door", "Pozzo's bedroom", {}),
    "letter_study": ("Door", "the study", {}),
    "letter_nursery": ("Door", "the nursery", {}),
    "letter_billiard": ("Door", "the billiard room", {}),
    "win_blue": ("Shutter", "the Blue Room window", {}),
    "win_red": ("Shutter", "the Red Room window", {}),
    "win_studio": ("Shutter", "the studio window", {}),
    "win_nursery": ("Shutter", "the nursery window", {}),
    "win_billiard": ("Shutter", "the billiard room window", {}),
    "win_master": ("Shutter", "the bedroom window", {}),
    "laundry_tub": ("Basket", "the basket of wet sheets", {}),
    "bath_rack": ("Lines", "the bath drying rack", {}),
    "studio_easel": ("Easel", "the finished portrait", {}),
    "gallery_frame": ("Frame", "the empty frame in the gallery", {}),
    "nursery_cylinder": ("MusicBox", "the music-box cylinder", {}),
    "orchestrion": ("Orchestrion", "the orchestrion", {}),
    "billiard_book": ("Book", "the forgotten book", {}),
    "library_shelf": ("Shelf", "the gap on the library shelf", {}),
    "boat_pump": ("Pump", "the bilge pump", {}),
    "boat_rope": ("Mooring", "the boat's mooring rope", {}),
    "g_parcel": ("CrateStack", "the G. parcel washed ashore", {}),
    "pantry_china": ("BreadRack", "the china cupboard", {}),
    "parcel_corner": ("ParcelPile", "the G. parcel corner", {}),
    "gallery_cleat": ("Winder", "the chandelier rope (gallery)", {}),
    "win_ballroom": ("Shutter", "the ballroom window", {}),
    "courtyard_pots": ("Pots", "the lemon pots", {}),
    "greenhouse_bench": ("Bench", "the greenhouse bench", {}),
    "grave_1": ("Lamp", "grave lantern 1", {"sabotage": "snuff"}),
    "grave_2": ("Lamp", "grave lantern 2", {"sabotage": "snuff"}),
    "grave_3": ("Lamp", "grave lantern 3", {"sabotage": "snuff"}),
    "grave_4": ("Lamp", "grave lantern 4", {"sabotage": "snuff"}),
    "fuse_coil": ("SparkCoil", "the spark coil", {}),
}

# chore id -> (title, blurb, bots, replaces, [(label, effect, cue)] per step)
CHORES = {
    "WineForTheHall": ("Wine for the hall", "A basket of bottles from the wine cellar for the decanter on the long table - "
                       "run and the bottles clink.", True, ["PourAle"],
                       [("Take a basket of wine in the cellar", "", "Thud"),
                        ("Fill the decanter on the long table", "stack", "Pour")]),
    "CisternWater": ("Water from the cistern", "Crank a full bucket out of the cistern and carry it up - walk, don't run, "
                     "or it sloshes out.", True, ["DrawWater"],
                     [("Crank a full bucket up at the cistern", "", "Crank"),
                      ("Pour it into $dst - walk, don't run!", "water", "Pour")]),
    "Firewood": ("Firewood", "A log bundle from the woodshed for a fire (tossing it in counts).", True, ["ChopWood"],
                 [("Take a log bundle at the woodshed", "", "Thud"),
                  ("Feed it to $dst", "stack+fx:ChimneySmoke", "Thud")]),
    "Chandelier": ("Light the chandelier", "Lower the great chandelier on its winch, then light it in front of everyone.",
                   True, [],
                   [("Turn $winch to lower the chandelier", "", "Crank"),
                    ("Light the chandelier over the long table", "light", "Flame")]),
    "Shutters": ("Close the shutters", "Three shutters bang in the storm: close them, any order.", True, [],
                 [("Close the banging shutters", "stack", "Thud")]),
    "SignalLamp": ("Signal lamp", "Lamp oil from the attic, over the roof walk, up the tower ladder, into the lamp.", False,
                   ["FuelLighthouse"],
                   [("Take a can of lamp oil in the attic", "", "Thud"),
                    ("Climb the storm tower and fill the signal lamp", "light+fx:LighthouseGlow", "Pour")]),
    "PozzosSupper": ("Pozzo's supper", "The supper tray from the servery to Pozzo - run and the china breaks.", True, [],
                     [("Take the supper tray at the servery hatch", "", "Thud"),
                      ("Leave it at $dst", "stack", "Knock")]),
    "GuestLetters": ("Guest letters", "Sealed letters from the post desk, one under each marked door.", True, [],
                     [("Take the sealed letters at the post desk", "", "Paper"),
                      ("Slide a letter under each marked door", "stack", "Paper")]),
    "BailTheBoat": ("Bail the boat", "Three strokes of the bilge pump, then make the boat fast.", True, ["FixBoat"],
                    [("Pump the bilge (three strokes)", "", "Crank"),
                     ("Tie the mooring rope", "", "Knot")]),
    "PotsIndoors": ("Pots indoors", "A heavy lemon pot blew over in the courtyard: carry it into the greenhouse.", True, [],
                    [("Lift a fallen lemon pot in the courtyard", "", "Thud"),
                     ("Set it on the greenhouse bench", "stack", "Thud")]),
    "SparkJar": ("Spark jar", "A charged jar from the lightning rod in the tower, all the way down to the cellar board.",
                 True, [],
                 [("Take a charged jar at the tower rack", "", "Thud"),
                  ("Plug it into the spark board in the cellar", "stack", "Whoosh")]),
    "MusicBox": ("Music box", "The cylinder from the nursery music box goes into the ballroom orchestrion.", True, [],
                 [("Take the cylinder from the nursery music box", "", "Thud"),
                  ("Fit it into the orchestrion", "stack", "Grind")]),
    "HangTheSheets": ("Hang the sheets", "Wet sheets from the laundry, hung up to dry.", True, [],
                      [("Take the basket of wet sheets in the laundry", "", "Thud"),
                       ("Hang them on $dst", "stack", "Knot")]),
    "HangThePortrait": ("Hang the portrait", "The finished portrait from the studio into the gallery's empty frame.", True, [],
                        [("Take the finished portrait in the studio", "", "Thud"),
                         ("Hang it in the empty frame in the gallery", "stack", "Thud")]),
    "LostBook": ("The lost book", "Someone left a library book in the billiard room.", True, [],
                 [("Pick up the forgotten book in the billiard room", "", "Paper"),
                  ("Put it back in the gap on the library shelf", "stack", "Paper")]),
    "ClockAndBell": ("Clock and bell", "Wind the great clock under the roof, then ring the chapel bell for the whole manor.",
                     True, ["WindClock", "RingBell"],
                     [("Wind the great clock in the clock room", "fx:ClockChime", "Crank"),
                      ("Pull the chapel bell rope", "fx:BellRing", "Knot")]),
    "GParcel": ("The G. parcel", "A heavy parcel marked 'G.' washed into the boathouse: alone you shuffle, with a helper "
                "you walk.", True, ["UnloadFish"],
                [("Lift the G. parcel in the boathouse", "", "Thud"),
                 ("Carry it to the G. corner in the entrance hall", "stack", "Thud")]),
    "GraveLanterns": ("Grave lanterns", "A lit taper from the chapel and four lanterns in the family graveyard.", True,
                      ["TendGraves"],
                      [("Take a lit taper from the chapel candle box", "", "Flame"),
                       ("Light the four grave lanterns", "light", "Flame")]),
    "SetTheTable": ("Set the table", "China from the pantry for the banquet table.", True, [],
                    [("Take a stack of china in the pantry", "", "Thud"),
                     ("Lay it on the banquet table", "stack", "Thud")]),
}


def item_def(name):
    mesh, scale, mass, extra = ITEMS[name]
    sizes = json.load(open(SIZES))
    s = sizes.get(mesh, [30, 30, 30, -15, -15, 0])
    sc = scale if isinstance(scale, list) else [scale] * 3
    d = {"mesh": f"{mesh}.{mesh.split('/')[-1]}", "scale": scale,
         "box": [max(4, round(s[0] * sc[0] / 2)), max(4, round(s[1] * sc[1] / 2)), max(3, round(s[2] * sc[2] / 2))],
         "mesh_z": round(-(s[5] + s[2] / 2.0) * sc[2]), "mass": mass, "speed": 1.0}
    d.update(extra)
    return d


def main():
    L = G.layout()
    R = G.rooms(L)
    anchors = []
    for a in L["anchors"]:
        kind, label, extra = ANCHORS[a["id"]]
        fid = R[a["room"]]["floor"]
        z = G.floor_z(fid) / 100.0 + 0.02
        out = {"id": a["id"], "kind": kind, "at": [float(a["at"][0]), float(a["at"][1])], "z": round(z, 2),
               "r": float(a["r"]), "label": label, "room": a["room"]}
        out.update(extra)
        if fid == "F3":
            # the signal lamp: reached by the ladder in the storm tower (stand at its foot, climb one storey)
            lad = next(s for s in L["stairs"] if s["kind"] == "ladder")
            out["stand"] = [lad["bottom"][0] - 0.8, lad["bottom"][1], round(G.floor_z("F2") / 100.0 + 0.02, 2)]
            out["climb_m"] = G.STOREY / 100.0
        anchors.append(out)
    ids = {a["id"] for a in anchors}
    chores = []
    for c in L["chores"]:
        title, blurb, bots, replaces, steps_en = CHORES[c["id"]]
        steps = []
        for s, (label, effect, cue) in zip(c["steps"], steps_en):
            st = {"verb": s["verb"], "at": s["at"], "secs": float(s.get("secs", 1.0)), "label": label, "cue": cue}
            for k in ("item", "fill", "min_fill", "repeat"):
                if k in s:
                    st[k] = s[k]
            if s["verb"] == "work" and isinstance(s["at"], list) and c["id"] == "GraveLanterns":
                # the taper has to reach every lantern: carried (bring), used up at the last one
                st["verb"] = "bring"
                st["item"] = "Taper"
            if st["verb"] == "bring":
                st["consume"] = True
            if effect:
                st["effect"] = effect
            steps.append(st)
        ch = {"id": c["id"], "title": title, "blurb": blurb, "bots": bots, "steps": steps}
        if replaces:
            ch["replaces"] = replaces
        if c.get("variants"):
            ch["variants"] = c["variants"]
        chores.append(ch)
    items = {k: item_def(k) for k in ITEMS}
    meet = L["meeting"]["at"]
    D = {
        "_doc": "SPRINT-018 world chores for Storm Manor (Docs/Level/StormManor_Plan.md section 6). GENERATED by "
                "Tools/Level/gen_stormmanor_chores.py from Tools/Level/stormmanor_layout.json - edit the generator, not "
                "this file. Same format as morrowmere_world_chores.json: metres, UE frame, z = floor tops (Z0 = +5 m).",
        "map": "L_StormManor",
        "walk_speed": 3.2,
        "climb_speed": 2.6,
        "hub": [float(meet[0]), float(meet[1]), round(G.floor_z("F0") / 100.0, 2)],
        "items": items,
        "anchors": anchors,
        "chores": chores,
    }
    for c in D["chores"]:
        for v in c.get("variants", [{"name": "default", "vars": {}}]):
            for s in c["steps"]:
                ats = s["at"] if isinstance(s["at"], list) else [s["at"]]
                for at in ats:
                    at = v["vars"].get(at[1:], at) if at.startswith("$") else at
                    assert at in ids, f"{c['id']}/{v['name']}: unknown anchor {at}"
                assert s.get("item", "Bucket") in D["items"], s
    for a in D["anchors"]:
        a.setdefault("yaw", 0.0)
        a.setdefault("climb_m", 0.0)
        if "stand" in a and len(a["stand"]) == 2:
            a["stand"] = a["stand"] + [a["z"]]
    with open(OUT_SRC, "w", encoding="utf-8") as f:
        json.dump(D, f, indent=1, ensure_ascii=False)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(D, f, indent=1)
    text = json.dumps(D, indent=None, separators=(",", ":"), ensure_ascii=True)
    chunks = [text[i:i + 6000] for i in range(0, len(text), 6000)]
    with open(OUT_INL, "w", encoding="utf-8", newline="\n") as f:
        f.write("// GENERATED by Tools/Level/gen_stormmanor_chores.py from Tools/Level/stormmanor_layout.json - do not edit.\n")
        f.write("// Included once by KGWorldChoreTypes.cpp (the Storm Manor catalog; Morrowmere's is KGWorldChoreData.gen.inl).\n")
        f.write("static const TCHAR* const GKGWorldChoreJsonChunks_StormManor[] = {\n")
        for ch in chunks:
            f.write('\tTEXT(R"KGJSON(' + ch + ')KGJSON"),\n')
        f.write("};\n")
    print(f"storm manor chores: {len(D['chores'])} chores, {len(D['anchors'])} anchors -> {OUT_INL} "
          f"({len(text)} chars, {len(chunks)} chunks)")


if __name__ == "__main__":
    main()
