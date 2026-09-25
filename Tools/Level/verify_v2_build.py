"""Numeric checks of the built Morrowmere v2 (system Python; no editor needed).

    python Tools/Level/verify_v2_build.py        -> prints a report, writes Saved/KG_V2_Verify.json, exit 1 on failure

Inputs: the layout (Tools/Level/morrowmere_layout_v2.json), the placements the builder spawned
(Art/Packed/KG_V2_Placements.json), the terrain heights and the builder's report (Saved/KG_V2_BuildReport.json).
Checks:
  - validate_layout.py rules (PASS) - overlaps, doors <= 3 m from walkways, stairs/ramps, network, sightlines
  - no retaining / cheek / parapet wall piece stands inside a building footprint or blocks a door (1.2 m apron)
  - every home + civic door: the terrain in front of the door is within 0.35 m of the door pad (a step, not a wall)
  - every door reaches the fountain over the walk graph (validate_layout.network), with the run time
  - the real navmesh: Saved/KG_V2_NavCheck.json (fountain -> every door, ground chore, player start, pier/mole probes)
  - stairs: kit risers placed = risers x width/2 per stair, one hidden ramp per flight
  - gameplay: 22 task stations, 20 player starts, 20 chests (HouseIndex 0..19), 3 ladders, the KG_Gallows and
    KG_BotHub markers
  - polish (plan 11.4 pack KG_DressTerrace): balustrade / quay wall / steps / bridge pieces go through the same wall
    checks; the calm basin is set on AKGMapInfo; clock faces + dormers counted; every material on the map's (H)ISMs has
    an instancing-ready base (Saved/KG_V2_MaterialCheck.json, Tools/Unreal/kg_fix_gltf_materials.py)
"""
import json
import math
import os
import sys

import networkx as nx
from shapely.geometry import Point, Polygon

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "Tools", "Blender"))
import validate_layout as VL  # noqa: E402
import prep_v2_placements as PP  # noqa: E402  (ground(), stair profiles; importing does not rebuild the file)

L = VL.load()
PL = json.load(open(os.path.join(ROOT, "Art", "Packed", "KG_V2_Placements.json")))
REP = json.load(open(os.path.join(ROOT, "Saved", "KG_V2_BuildReport.json")))


def piece_poly(it):
    """Footprint of a kit wall piece (2 m x 0.4 m x scale), exterior on local -Y."""
    x, y = it["p"][0] / 100.0, it["p"][1] / 100.0
    s = it.get("s", [1, 1, 1])
    yaw = math.radians(it.get("y", 0.0))
    ux, uy = math.cos(yaw), math.sin(yaw)
    vx, vy = -math.sin(yaw), math.cos(yaw)
    hx = 1.0 * s[0]
    y0, y1 = -0.31 * s[1], 0.09 * s[1]
    pts = [(x + ux * a + vx * b, y + uy * a + vy * b) for a, b in ((-hx, y0), (hx, y0), (hx, y1), (-hx, y1))]
    return Polygon(pts)


# Footprints (m, UE local x0, x1, y0, y1) of the plan 11.4 pack pieces checked like the kit walls.
DT_FOOT = {"DT:Balustrade_2m": (-1.0, 1.0, -0.175, 0.175), "DT:Balustrade_Post": (-0.22, 0.22, -0.22, 0.22),
           "DT:QuayWall_4m": (-2.0, 2.0, -0.9, 0.0), "DT:QuayWall_4m_Ring": (-2.0, 2.0, -0.9, 0.0),
           "DT:QuaySteps": (0.0, 4.2, 0.0, 1.3)}


def dt_poly(it):
    x0, x1, y0, y1 = DT_FOOT[it["m"]]
    x, y = it["p"][0] / 100.0, it["p"][1] / 100.0
    s = it.get("s", [1, 1, 1])
    yaw = math.radians(it.get("y", 0.0))
    ux, uy = math.cos(yaw), math.sin(yaw)
    vx, vy = -math.sin(yaw), math.cos(yaw)
    pts = [(x + ux * a * s[0] + vx * b * s[1], y + uy * a * s[0] + vy * b * s[1]) for a, b in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    return Polygon(pts)


DRESS_BUDGET = {"actors": 900, "lights": 10, "lights_wilds": 14}
ACTOR_KINDS = ("actor", "mover", "seat", "breakable", "chest", "light", "sound")


def dressing(L, W, out, err):
    """Zone dressing (Saved/KG_V2_DressReport.json, written by Tools/Unreal/kg_dress_v2.py):
    budgets per zone, module errors, and clearance of every COLLIDING placement against the door aprons, the
    stair and ramp corridors, the central 2 m of every walkway, the chores and the player starts."""
    rp = os.environ.get("KG_DRESS_REPORT") or os.path.join(ROOT, "Saved", "KG_V2_DressReport.json")
    if not os.path.exists(rp):
        out["info"]["dressing"] = "not dressed (run Tools/Unreal/kg_dress_v2.py)"
        return
    rep = json.load(open(rp))
    rec = rep["record"]
    zones = {}
    for z, st in rep["zones"].items():
        if st.get("error") or st.get("errors"):
            err.append(f"dressing {z}: module error {str(st.get('error') or st.get('errors'))[-300:]}")
        zr = [r for r in rec if r["zone"] == z]
        actors = sum(1 for r in zr if r["kind"] in ACTOR_KINDS)
        inst = sum(1 for r in zr if r["kind"] == "inst")
        lights = sum(1 for r in zr if r["kind"] == "light")
        zones[z] = {"actors": actors, "instances": inst, "lights": lights, "seats": st.get("seats", 0),
                    "movers": st.get("movers", 0), "breakables": st.get("breakables", 0), "chests": st.get("chests", 0)}
        if actors > DRESS_BUDGET["actors"]:
            err.append(f"dressing {z}: {actors} actors > {DRESS_BUDGET['actors']}")
        if lights > DRESS_BUDGET["lights_wilds" if z == "wilds" else "lights"]:
            err.append(f"dressing {z}: {lights} lights over budget")
    out["info"]["dressing_zones"] = zones
    out["info"]["dressing_totals"] = {k: sum(v[k] for v in zones.values()) for k in
                                      ("actors", "instances", "lights", "seats", "movers", "breakables", "chests")}
    # keep-clear shapes
    from shapely.geometry import LineString
    from shapely.ops import unary_union
    keep = []
    for bid, cat, b in W.buildings:
        if cat == "home" or (cat == "civic" and b.get("interior")):
            keep.append(("door " + bid, Point(VL.door(b, 0.8)).buffer(0.7)))
    for st in L["stairs"]:
        keep.append(("stair " + st["name"], LineString([st["from"], st["to"]]).buffer(st["width"] / 2.0 - 0.1)))
    for rp_ in L["ramps"]:
        keep.append(("ramp " + rp_["name"], LineString(rp_["points"]).buffer(rp_["width"] / 2.0 - 0.1)))
    for ln in L["lanes"]:
        keep.append(("walkway " + ln["name"], LineString(ln["points"]).buffer(1.0)))       # the central 2 m
    towers = {t["id"] for t in L["tasks"] if t.get("tower")}
    for t in L["tasks"]:
        if t["id"] not in towers:
            keep.append(("chore " + t["id"], Point(t["at"]).buffer(1.0)))
    for x, y, z, _yaw in PL["starts"]:
        keep.append(("start", Point(x / 100.0, y / 100.0).buffer(0.5)))
    hits = {}
    solid = [r for r in rec if r.get("c") and not r.get("o") and r["kind"] not in ("light", "sound")]
    for r in solid:
        if "hx" in r:      # oriented footprint rectangle (metres)
            c, sn = math.cos(math.radians(r["yaw"])), math.sin(math.radians(r["yaw"]))
            hx, hy = r["hx"] / 100.0, r["hy"] / 100.0
            fx, fy = r["fx"] / 100.0, r["fy"] / 100.0
            p = Polygon([(fx + a * c - b * sn, fy + a * sn + b * c) for a, b in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))])
        else:
            p = Point(r["x"] / 100.0, r["y"] / 100.0).buffer(max(0.1, r["r"] / 100.0 * 0.8))
        for name, shp in keep:
            if p.intersects(shp):
                hits.setdefault(name, []).append(f"{r['zone']}:{r['m']}@{r['x'] / 100:.1f},{r['y'] / 100:.1f}")
                break
    out["info"]["dressing_solid_checked"] = len(solid)
    out["info"]["dressing_clear"] = "all clear" if not hits else {k: v[:4] for k, v in hits.items()}
    for name, v in hits.items():
        err.append(f"dressing blocks {name}: {len(v)} prop(s), e.g. {v[0]}")


# ============================================================================================ placement (SPRINT-022)
# Geometric validator over the trace samples of Tools/Unreal/kg_placement_check_v2.py (every exterior prop of the
# level, builder and dressing, actors and HISM instances). Nothing may float (> 8 cm gap under the whole footprint),
# be sunk past its tolerance, stand on a stair tread (unless whitelisted) or intersect a stair / ramp / bridge / door
# apron. Mesh classes below decide what a prop is (name fragments, first match wins).
FLOAT_TOL = 8.0          # cm
ATTACHED = ("HerbRack", "FishingRod", "Lantern_Wall", "FlowerBox", "Sign", "Bracket", "Bunting", "Banner", "Vine", "Ivy", "Doorbell", "ClockFace",
            "ClockHand", "Dormer", "Chimney", "GlowCard", "BeamCone", "Window", "Shutter", "Awning", "Laundry", "Rope",
            "Chain_Coil", "Garland", "Wall_", "Roof_", "Balcony", "Overhang", "Maypole_Ribbons", "Windmill_Sails",
            "Bell_", "Belfry", "LampRoom", "Gallery", "Spire", "ClockStage", "Telescope_Tube", "Weathervane")
WATER = ("Buoy", "Lifebuoy", "Rowboat", "Koi", "Fish_", "SeaweedClump", "Lily", "Lotus", "Plane", "Piling", "Shipwreck")
PLANT = ("Tree", "Pine", "Sakura", "Maple", "Bamboo", "Bonsai", "Birch", "Willow", "Cypress", "Bush", "Shrub", "Fern",
         "Grass", "Flower", "Clover", "Plant_", "Reed", "Wheat", "Carrot", "Cabbage", "Mushroom", "Heather",
         "Gorse", "Lavender", "Crop")
ROCK = ("Rock", "Stone", "Boulder", "Menhir", "Cliff", "SeaStack", "Driftwood", "ShellCluster", "PondRim")
FLAT = ("Pebble", "Petal_", "Floor_", "RockPath", "Rug", "Prop_ExteriorBorder", "Pirate_Planks", "Stairs_Exterior_Platform", "NetPile",
        "DigMound")
STAIR_OK = ("Planter_Large", "Balustrade")          # may stand on a stair landing / tread by design
TOL_SUNK = {"plant": 0.6, "rock": 0.55, "flat": 0.0, "prop": 0.25}      # x height (plus the minimum below)
TOL_SUNK_MIN = {"plant": 25.0, "rock": 15.0, "flat": 30.0, "prop": 10.0}


TERRAIN_LIKE = ("KG_Island",)      # terrain pieces, checked with the terrain


def prop_class(it):
    m = it["m"]
    if it.get("hid") or m == "Sphere" or m in ("Cube", "Cylinder") and ("/Lanterns" in it["f"] or it["f"].startswith("V2/")):
        return "skip"                     # hidden colliders, glow orbs (lanterns, the lighthouse beam hub)
    if m in TERRAIN_LIKE or "/Buoys" in it["f"]:
        return "skip"                     # the island is terrain; buoys float on the water by design
    if (m.startswith("Floor_") or "Planks" in m or "Rug" in m) and it["tilt"] > 8.0:
        return "skip"                     # boards / hides laid as roofs, lean-tos, ramps, signpost arrows, drying lines
    if any(k in m for k in ATTACHED):
        return "skip"
    if m.startswith("Corner_") and (it["tilt"] > 30.0 or min(it["sx"], it["sy"]) < 12.0):
        return "skip"                     # rods / ropes / beams made of stretched posts
    if any(k in m for k in WATER):
        return "skip"
    if any(k in m for k in PLANT):
        return "plant"
    if any(k in m for k in ROCK):
        return "rock"
    if any(k in m for k in FLAT):
        return "flat"
    return "prop"


def _sample_state(s, h):
    """(gap cm: >0 above the support, <0 embedded; support surface or None) for one bottom sample."""
    bz = s["bz"]
    under = [x for x in s["s"] if x["z"] <= bz + FLOAT_TOL]
    over = [x for x in s["s"] if bz + FLOAT_TOL < x["z"] <= bz + max(10.0, h) + 50.0 and x["nz"] > 0.5 and _is_ground(x)]
    if over:
        top = max(over, key=lambda x: x["z"])
        return bz - top["z"], top
    if under:
        top = max(under, key=lambda x: x["z"])
        return bz - top["z"], top
    return 1e9, None


def _is_ground(x):
    """Surfaces a prop can be sunk into: the terrain and kit floors / paving."""
    return x["k"] == "terrain" or x["m"].startswith("Floor_")


def _is_stair(sup):
    return bool(sup) and (sup["m"].startswith("Stairs_Exterior_Straight") or sup["f"].startswith("V2/StairRamps")
                          or sup["a"].startswith("StairRamp"))


def placement_shapes(L, W):
    """Keep-out shapes with a z band: stairs, ramps, bridges, door aprons (metres)."""
    from shapely.geometry import LineString
    keep = []
    for st in L["stairs"]:
        keep.append(("stair " + st["name"], LineString([st["from"], st["to"]]).buffer(st["width"] / 2.0 - 0.05, cap_style=2),
                     min(st["z0"], st["z1"]) - 0.3, max(st["z0"], st["z1"]) + 2.0))
    for rp in L["ramps"]:
        keep.append(("ramp " + rp["name"], LineString(rp["points"]).buffer(rp["width"] / 2.0 - 0.05, cap_style=2),
                     min(rp["z0"], rp["z1"]) - 0.3, max(rp["z0"], rp["z1"]) + 2.0))
    for br in L["bridges"]:
        if br["name"] == "ford_stones":
            continue
        ax, ay = br["along"]
        n = math.hypot(ax, ay)
        ax, ay = ax / n, ay / n
        cx, cy = br["at"]
        hs, hw = br["span"] / 2.0, br["width"] / 2.0
        poly = Polygon([(cx + ax * a - ay * b, cy + ay * a + ax * b) for a, b in ((-hs, -hw), (hs, -hw), (hs, hw), (-hs, hw))])
        keep.append(("bridge " + br["name"], poly, br["deck_z"] - 0.6, br["deck_z"] + 2.0))
    gb = PL.get("garden", {}).get("bridge")
    if gb:
        keep.append(("bridge koi_bridge", Polygon(gb["poly"]), gb["deck_z"] - 0.7, gb["deck_z"] + 2.0))
    for bid, cat, b in W.buildings:
        if cat == "home" or (cat == "civic" and b.get("interior")):
            w, d = b["size"]
            yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
            c, s_ = math.cos(yaw), math.sin(yaw)
            cw = w // 2
            lx0 = -cw + 1 + 2 * (cw // 2) if w >= 4 else 0.0
            pts = []
            for lx, ly in ((lx0 - 0.6, -d / 2.0 - 0.32), (lx0 + 0.6, -d / 2.0 - 0.32), (lx0 + 0.6, -d / 2.0 - 1.5),
                           (lx0 - 0.6, -d / 2.0 - 1.5)):
                pts.append((b["at"][0] + lx * c - ly * s_, b["at"][1] + lx * s_ + ly * c))
            keep.append(("door " + bid, Polygon(pts), b["z"] - 0.3, b["z"] + 2.2))
    return keep


def placement(L, W, out, err):
    path = os.environ.get("KG_PLACE_SAMPLES") or os.path.join(ROOT, "Saved", "KG_V2_PlacementSamples.json")
    if not os.path.exists(path):
        out["info"]["placement"] = "not sampled (run Tools/Unreal/kg_placement_check_v2.ps1)"
        err.append("placement: no samples (run Tools/Unreal/kg_placement_check_v2.ps1)")
        return
    D = json.load(open(path))
    keep = placement_shapes(L, W)
    # Props resting on other props: small clutter has no collision, so traces fall through it. A sample whose point
    # lies inside another prop's footprint, between that prop's bottom and its top + 8 cm, is supported by it.
    grid = {}
    for k, it in enumerate(D["items"]):
        xs = [p[0] for p in it["foot"]]
        ys = [p[1] for p in it["foot"]]
        for gx in range(int(min(xs) // 200), int(max(xs) // 200) + 1):
            for gy in range(int(min(ys) // 200), int(max(ys) // 200) + 1):
                grid.setdefault((gx, gy), []).append(k)
    polys = {}

    def on_prop(k0, s):
        for k in grid.get((int(s["x"] // 200), int(s["y"] // 200)), ()):
            if k == k0:
                continue
            o = D["items"][k]
            lo = min(q["bz"] for q in o["smp"])
            if not (lo - FLOAT_TOL <= s["bz"] <= o["top"] + FLOAT_TOL):
                continue
            if k not in polys:
                polys[k] = Polygon([(x, y) for x, y in o["foot"]]).buffer(2.0)
            if polys[k].covers(Point(s["x"], s["y"])):
                return o
        return None
    viol = {"floating": [], "sunk": [], "on_stair": [], "intersects": []}
    counts = {}
    for k0, it in enumerate(D["items"]):
        cls = prop_class(it)
        counts[cls] = counts.get(cls, 0) + 1
        if cls == "skip":
            continue
        h = it["h"]
        smp = it["smp"][:1] if cls == "plant" else it["smp"]
        if it["tilt"] > 15.0:
            # leaning / fallen props (tools against a wall, a barrel on its side): the lowest corner is the contact
            zmin = it["top"] - h
            smp = [dict(q, bz=zmin) for q in smp]
        states = [_sample_state(s, h) for s in smp]
        gaps = [g for g, _ in states]
        tag = f"{it['f'] or it['a']}:{it['m']}@{smp[0]['x'] / 100:.1f},{smp[0]['y'] / 100:.1f},{smp[0]['bz'] / 100:.2f}"
        if min(gaps) > FLOAT_TOL and not any(on_prop(k0, s) for s in smp):
            g = min(gaps)
            viol["floating"].append((tag, "no support within 3 m" if g > 1e8 else f"gap {g:.0f} cm"))
        burial = min(max(0.0, -g) for g in gaps)
        tol = max(TOL_SUNK_MIN[cls], TOL_SUNK[cls] * h)
        if burial > tol and it["tilt"] <= 15.0:         # a leaning tool / lying log may bite into the soil
            viol["sunk"].append((tag, f"sunk {burial:.0f} cm > {tol:.0f}"))
        if not any(k in it["m"] for k in STAIR_OK):
            touching = [sup for g, sup in states if g <= FLOAT_TOL]
            if cls != "flat" and any(_is_stair(sup) for sup in touching):     # paving under a riser is ground
                viol["on_stair"].append((tag, "stands on a stair tread / ramp"))
            foot = Polygon([(x / 100.0, y / 100.0) for x, y in it["foot"]]) if cls != "plant" else \
                Point(smp[0]["x"] / 100.0, smp[0]["y"] / 100.0).buffer(0.25)
            if not foot.is_valid or foot.area < 1e-4:
                foot = Point(smp[0]["x"] / 100.0, smp[0]["y"] / 100.0).buffer(0.1)
            z0, z1 = min(s["bz"] for s in it["smp"]) / 100.0, it["top"] / 100.0
            for name, shp, lo, hi in keep:
                if z1 < lo or z0 > hi:
                    continue
                if cls == "flat" and not name.startswith("stair "):
                    continue          # paving is the ground itself: it may run under ramps, bridges and door steps
                if name.startswith("bridge ") and "Bridge" in it["m"]:
                    continue          # the bridge itself
                if foot.intersection(shp).area > 0.01:
                    viol["intersects"].append((tag, name))
                    break
    rep = {"items": len(D["items"]), "classes": counts, "violations": {k: len(v) for k, v in viol.items()},
           "list": {k: [f"{a} ({b})" for a, b in v] for k, v in viol.items()}}
    json.dump(rep, open(os.path.join(ROOT, "Saved", "KG_V2_PlacementReport.json"), "w"), indent=1)
    n = sum(len(v) for v in viol.values())
    out["info"]["placement"] = (f"{len(D['items'])} props sampled ({sum(v for k, v in counts.items() if k != 'skip')} "
                                f"checked, {D.get('traces', 0)} traces): " + ", ".join(f"{k} {len(v)}" for k, v in viol.items()))
    for k, v in viol.items():
        for a, b in v[:6]:
            err.append(f"placement {k}: {a} ({b})")
        if len(v) > 6:
            err.append(f"placement {k}: ... {len(v) - 6} more (Saved/KG_V2_PlacementReport.json)")


# ============================================================================================ house variety (SPRINT-022)
def house_variety(L, info_lines, out, err):
    """At least 6 archetypes among the homes, no two neighbouring homes / shells with the same archetype or the
    same visible signature (Tools/Level/kg_archetypes_v2.py), the builder built what the layout asked for
    (KG_V2_BuildReport stats.facades), the home count about 10 % down from 20 and the town-core coverage >= 20 %."""
    import kg_archetypes_v2 as ARCH
    bs = L["houses"] + L["infill"]
    missing = [b["id"] for b in bs if not ARCH.arch_of(b)]
    pairs = ARCH.neighbours(bs)
    same = [(bs[i]["id"], bs[j]["id"], bs[i].get("archetype")) for i, j in pairs
            if bs[i].get("archetype") == bs[j].get("archetype") or ARCH.signature(bs[i]) == ARCH.signature(bs[j])]
    homes = {b.get("archetype") for b in L["houses"]}
    built = REP.get("stats", {}).get("facades", {})
    mismatch = [b["id"] for b in bs if built and built.get(b["id"], {}).get("archetype") != b.get("archetype")]
    cov = next((m for m in info_lines if "coverage" in m), "")
    try:
        cov_v = float(cov.split("coverage")[1].split("%")[0])
    except (IndexError, ValueError):
        cov_v = None
    out["info"]["house_variety"] = (f"{len(L['houses'])} homes + {len(L['infill'])} shells, {len(homes)} archetypes among "
                                    f"the homes, {len({b.get('archetype') for b in bs})} overall; {len(pairs)} neighbour "
                                    f"pairs, {len(same)} same-looking; built facades {len(built)}; core coverage {cov_v}%")
    if missing:
        err.append(f"house variety: no archetype for {missing[:8]}")
    if len(homes) < 6:
        err.append(f"house variety: only {len(homes)} archetypes among the homes (need >= 6)")
    for a, b, k in same:
        err.append(f"house variety: neighbours {a} and {b} look the same ({k})")
    if not built:
        err.append("house variety: the build report has no facades (rebuild with kg_build_village_v2.py)")
    for bid in mismatch[:8]:
        err.append(f"house variety: {bid} built as {built.get(bid, {}).get('archetype')}, layout says another archetype")
    if not (16 <= len(L["houses"]) <= 18):
        err.append(f"house variety: {len(L['houses'])} homes (SPRINT-022: about 10 % fewer than 20)")
    if cov_v is None or cov_v < 20.0:
        err.append(f"house variety: town-core coverage {cov_v} % < 20 %")


# ============================================================================================ cliff faces (SPRINT-022 #6)
FACE_H, FACE_L, FACE_GAP = 6.0, 15.0, 8.0      # m: a face this tall and long must be broken at least every FACE_GAP m


def cliff_faces(L, out, err):
    """Every steep terrain face (> 65 deg; PL["faces"], prep_v2_placements.terrain_faces) is clustered; faces taller
    than 6 m and longer than 15 m within 60 m of a public walkway must be clad or broken (cliff rock pieces, kit
    walls, quay stones, buildings within 2.5 m) so that no bare stretch longer than 8 m remains."""
    from shapely.geometry import LineString, MultiPoint
    from shapely.ops import unary_union
    pts = PL.get("faces")
    if pts is None:
        out["info"]["cliff_faces"] = "no face samples (rerun prep_v2_placements.py)"
        err.append("cliff faces: no samples in KG_V2_Placements.json (rerun prep_v2_placements.py)")
        return
    cover = []
    for it in PL["items"]:
        m = it["m"]
        x, y = it["p"][0] / 100.0, it["p"][1] / 100.0
        yaw = math.radians(it.get("y", 0.0))
        if m.startswith("DL:Cliff"):
            w = {"DL:Cliff_A": 8.9, "DL:Cliff_B": 7.0, "DL:Cliff_C": 5.85, "DL:CliffTalus": 9.0}[m] * it.get("s", [1, 1, 1])[0]
            ux, uy = math.cos(yaw), math.sin(yaw)
            cover.append(LineString([(x - ux * w / 2, y - uy * w / 2), (x + ux * w / 2, y + uy * w / 2)]).buffer(2.5))
        elif it["f"].split("/")[1] in ("Walls", "Quay", "Parapets", "StairWalls", "RampWalls", "Mole"):
            cover.append(Point(x, y).buffer(2.2))
    for bid, _, b in VL.World(L).buildings:
        cover.append(VL.rect(b, 2.5))
    COV = unary_union(cover) if cover else None
    WALKS = unary_union([LineString(l["points"]).buffer(l["width"] / 2.0) for l in L["lanes"]] +
                        [Polygon(q["polygon"]) for q in L["squares"]])
    # cluster on a grid (points <= 1.3 m apart are one face)
    cell = {}
    for k, p_ in enumerate(pts):
        cell.setdefault((int(p_[0] // 1.3), int(p_[1] // 1.3)), []).append(k)
    seen = [False] * len(pts)
    faces = []
    for k in range(len(pts)):
        if seen[k]:
            continue
        stack, grp = [k], []
        seen[k] = True
        while stack:
            i = stack.pop()
            grp.append(i)
            cx, cy = int(pts[i][0] // 1.3), int(pts[i][1] // 1.3)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for j in cell.get((cx + dx, cy + dy), ()):
                        if not seen[j] and math.hypot(pts[i][0] - pts[j][0], pts[i][1] - pts[j][1]) <= 1.3:
                            seen[j] = True
                            stack.append(j)
        faces.append(grp)
    report, bad = [], 0
    for grp in faces:
        P_ = [pts[i] for i in grp]
        h = max(p_[2] for p_ in P_) - min(p_[3] for p_ in P_)
        mp = MultiPoint([(p_[0], p_[1]) for p_ in P_])
        rect_ = mp.minimum_rotated_rectangle
        if rect_.geom_type != "Polygon":
            continue
        c_ = list(rect_.exterior.coords)
        e1, e2 = math.dist(c_[0], c_[1]), math.dist(c_[1], c_[2])
        length = max(e1, e2)
        if h < FACE_H or length < FACE_L:
            continue
        if WALKS.distance(mp) > 60.0:
            continue
        ax = (c_[1][0] - c_[0][0], c_[1][1] - c_[0][1]) if e1 >= e2 else (c_[2][0] - c_[1][0], c_[2][1] - c_[1][1])
        n_ = math.hypot(*ax)
        ax = (ax[0] / n_, ax[1] / n_)
        proj = sorted((p_[0] * ax[0] + p_[1] * ax[1], COV is not None and COV.contains(Point(p_[0], p_[1]))) for p_ in P_)
        worst, run0 = 0.0, None
        for t_, cov in proj:
            if cov:
                run0 = None
                continue
            run0 = t_ if run0 is None else run0
            worst = max(worst, t_ - run0)
        cx_, cy_ = mp.centroid.x, mp.centroid.y
        rec = {"at": [round(cx_, 1), round(cy_, 1)], "height_m": round(h, 1), "length_m": round(length, 1),
               "bare_stretch_m": round(worst, 1), "covered": round(sum(1 for _, c in proj if c) / len(proj), 2)}
        report.append(rec)
        if worst > FACE_GAP:
            bad += 1
            err.append(f"cliff face at {rec['at']} ({rec['height_m']} m tall, {rec['length_m']} m long): bare for "
                       f"{rec['bare_stretch_m']} m (covered {int(rec['covered'] * 100)} %)")
    out["info"]["cliff_faces"] = (f"{len(report)} faces > {FACE_H:.0f} m tall and > {FACE_L:.0f} m long near public spaces, "
                                  f"{bad} unbroken: {report}")


def main():
    out = {"errors": [], "info": {}}
    err = out["errors"]
    e, w, info, times = VL.validate(L, quiet=True)
    out["info"]["validate_layout"] = "PASS" if not e else f"FAIL {len(e)}"
    err += [f"layout: {m}" for m in e]
    W = VL.World(L)
    house_variety(L, info, out, err)

    # --- wall pieces vs buildings / doors
    walls = [it for it in PL["items"] if (it["m"] == "V:Wall_UnevenBrick_Straight" or it["m"] in DT_FOOT) and
             it["f"].split("/")[1] in ("Walls", "Parapets", "StairWalls", "RampWalls", "Quay")]
    rects = {bid: VL.rect(b) for bid, _, b in W.buildings}
    doors = {}
    for bid, cat, b in W.buildings:
        if cat == "home" or (cat == "civic" and b.get("interior")):
            doors[bid] = (b, Point(VL.door(b, 0.6)).buffer(0.6))
    inside, blocked = [], []
    for it in walls:
        poly = dt_poly(it) if it["m"] in DT_FOOT else piece_poly(it)
        for bid, r in rects.items():
            if poly.intersection(r.buffer(-0.25)).area > 0.05:
                inside.append((it["f"], bid))
        for bid, (b, apron) in doors.items():
            if poly.intersects(apron):
                blocked.append((it["f"], bid))
    out["info"]["wall_pieces_checked"] = len(walls)
    for f, bid in sorted(set(inside)):
        err.append(f"wall piece ({f}) inside building {bid}")
    for f, bid in sorted(set(blocked)):
        err.append(f"wall piece ({f}) blocks the door of {bid}")

    # --- door steps: terrain right in front of each door vs the pad height
    steps = {}
    for bid, (b, _) in doors.items():
        dx, dy = VL.door(b, 0.8)
        gz = PP.g1(dx, dy)
        steps[bid] = round(gz - b["z"], 2)
        if abs(gz - b["z"]) > 0.35:
            err.append(f"door {bid} ({b.get('name')}): ground in front {gz:.2f} vs pad {b['z']:.2f}")
    out["info"]["door_step_max_m"] = max(abs(v) for v in steps.values())

    # --- every door reaches the fountain over the walk graph
    G = VL.network(W)
    root = ("hub", "fountain_square")
    dist = nx.single_source_dijkstra_path_length(G, root, weight="w")
    runs = {}
    for bid, (b, _) in doors.items():
        nid, dd = VL.nearest_node(G, VL.door(b), 6.0)
        if nid is None or nid not in dist:
            err.append(f"door {bid} unreachable on the walk graph")
        else:
            runs[bid] = round(dist[nid] + dd / VL.RUN, 1)
    out["info"]["doors_reachable"] = f"{len(runs)}/{len(doors)}"
    out["info"]["door_run_s_max"] = max(runs.values()) if runs else None
    out["info"]["door_runs_s"] = runs

    # --- stairs
    per = {}
    for it in PL["items"]:
        if it["f"] == "V2/Stairs" and it["m"].startswith("V:Stairs_Exterior"):
            per.setdefault("n", 0)
            per["n"] += 1
    need = sum(int(abs(s["z1"] - s["z0"])) * int(round(s["width"] / 2)) for s in L["stairs"])
    ramps = sum(1 for it in PL["items"] if it["f"] == "V2/StairRamps")
    flights = sum(len([f for f in PP.STAIR_P[s["name"]]["flights"] if f]) for s in L["stairs"])
    out["info"]["stair_risers"] = f"{per.get('n', 0)}/{need}"
    out["info"]["stair_hidden_ramps"] = f"{ramps}/{flights}"
    if per.get("n", 0) != need:
        err.append(f"stair risers {per.get('n', 0)} != {need}")
    if ramps != flights:
        err.append(f"hidden ramps {ramps} != flights {flights}")

    # --- gameplay actors from the build report
    cl = REP.get("classes", {})
    want = {"KGTaskStation": len(L["tasks"]), "PlayerStart": L["spawn"]["count"], "KGStorageChest": len(L["houses"]),
            "KGLadder": 3, "TargetPoint": 2, "NavMeshBoundsVolume": 1}   # TargetPoints: KG_Gallows + KG_BotHub
    for k, v in want.items():
        if cl.get(k, 0) != v:
            err.append(f"{k}: {cl.get(k, 0)} in the level, expected {v}")
    homes_doors = cl.get("KGDoor", 0)
    out["info"]["actors"] = {k: cl.get(k, 0) for k in ("StaticMeshActor", "PointLight", "KGDoor", "KGSeat", "KGStorageChest",
                                                        "KGLadder", "KGTaskStation", "PlayerStart", "KGBreakable",
                                                        "KGFishSchool", "AmbientSound")}
    out["info"]["build_stats"] = REP.get("stats", {})
    out["info"]["missing_meshes"] = REP.get("missing_meshes", [])
    if homes_doors < len(L["houses"]):
        err.append(f"KGDoor {homes_doors} < homes {len(L['houses'])}")
    for m in REP.get("missing_meshes", []):
        err.append(f"missing mesh {m}")
    if REP.get("stats", {}).get("failed_steps"):
        err.append(f"builder steps failed: {REP['stats']['failed_steps']}")

    # --- real navmesh paths (written by the capture session, Tools/Unreal/kg_capture_v2.py navcheck)
    nav_path = os.path.join(ROOT, "Saved", "KG_V2_NavCheck.json")
    if os.path.exists(nav_path):
        nav = json.load(open(nav_path))
        out["info"]["navmesh_paths"] = f"{nav['reachable']}/{nav['targets']} reachable from the fountain"
        for n in nav["unreachable"]:
            err.append(f"navmesh: no full path fountain -> {n}")
    else:
        out["info"]["navmesh_paths"] = "not checked (run Tools/Unreal/kg_capture_v2.ps1)"

    # --- polish: plan 11.4 pack, calm basin, clock faces, dormers, material usage
    dt = {}
    for it in PL["items"]:
        if it["m"].startswith("DT:"):
            dt[it["m"][3:]] = dt.get(it["m"][3:], 0) + 1
    st = REP.get("stats", {})
    out["info"]["terrace_pack"] = dt or "not used (kit fallbacks; Art/Packed/KG_DressTerrace_Clean.json missing)"
    out["info"]["clock_faces"] = st.get("clock_faces", 0)
    out["info"]["dormers"] = st.get("dormers", 0)
    out["info"]["calm_water"] = st.get("calm_water", "not set")
    if L["basin"].get("calm_mask") and "calm_water" not in st:
        err.append("calm basin not set on AKGMapInfo (rebuild KillGodotEditor, then kg_build_village_v2.py)")
    mc_path = os.path.join(ROOT, "Saved", "KG_V2_MaterialCheck.json")
    if os.path.exists(mc_path):
        mc = json.load(open(mc_path))
        out["info"]["material_usage"] = (f"{mc['ism_components']} ISM components / {len(mc['ism_materials'])} materials, "
                                         f"{len(mc['nanite_materials'])} Nanite materials: "
                                         f"{'PASS' if mc['pass'] else 'FAIL'}")
        for p in mc["problems"]:
            err.append(f"material usage: {p}")
    else:
        out["info"]["material_usage"] = "not checked (run Tools/Unreal/kg_fix_gltf_materials.py)"

    dressing(L, W, out, err)
    placement(L, W, out, err)
    cliff_faces(L, out, err)

    path = os.path.join(ROOT, "Saved", "KG_V2_Verify.json")
    json.dump(out, open(path, "w"), indent=1)
    for k, v in out["info"].items():
        if k not in ("door_runs_s", "build_stats"):
            print(f"INFO  {k}: {v}")
    for m in err:
        print("ERROR", m)
    print("PASS" if not err else f"FAIL ({len(err)})", "->", path)
    sys.exit(0 if not err else 1)


if __name__ == "__main__":
    main()
