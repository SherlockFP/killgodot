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


def main():
    out = {"errors": [], "info": {}}
    err = out["errors"]
    e, w, info, times = VL.validate(L, quiet=True)
    out["info"]["validate_layout"] = "PASS" if not e else f"FAIL {len(e)}"
    err += [f"layout: {m}" for m in e]
    W = VL.World(L)

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
