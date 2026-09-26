"""SPRINT-033a/034a: the forest ring of Morrowmere v2 - trails, camp, dens, band raster, forest chores, lint + report.

  python Tools/Level/gen_forest_bands.py            # write everything, lint (exit 1 on a violation)
  python Tools/Level/gen_forest_bands.py --check    # also re-run and compare bytes (determinism check)
  python Tools/Level/gen_forest_bands.py --trails   # trails only (no terrain needed; the terrain build paints them)

Design: Docs/Design/Forest_Threats_Chores.md (sections 2-4, 7, 10). Units: metres, UE frame (x east, +y toward the sea).
Writes
  Tools/Level/morrowmere_forest_v2.json          trails, trail-head lanterns, camp, dens, sectors + the 2 m band raster
                                                  (1 byte band/flags + 1 byte sector per cell, base64). Dev builds read it.
  Source/KillGodot/Forest/KGForestData.gen.inl   the same JSON embedded for every build
  Tools/Level/morrowmere_world_chores.json       the four forest chores + their anchors (id prefix "forest_"),
                                                  merged in place; run gen_world_chores.py afterwards
  Saved/KG_ForestBands_Report.json, Saved/Screenshots/Forest/KG_Cap_forest_bands.png
Band byte: bits 0-2 band (0 village, 1 edge, 2 middle, 3 deep, 4 out of bounds), bit 6 not walkable, bit 7 static safe
(village, trail). Deep = farther than KG_FOREST_DEEP from safety OR on the outer side of the ring trail (design 2.2).
"""
import base64
import hashlib
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT_JSON = os.path.join(HERE, "morrowmere_forest_v2.json")
OUT_INL = os.path.join(ROOT, "Source", "KillGodot", "Forest", "KGForestData.gen.inl")
CHORES = os.path.join(HERE, "morrowmere_world_chores.json")
REPORT = os.path.join(ROOT, "Saved", "KG_ForestBands_Report.json")
IMAGE = os.path.join(ROOT, "Saved", "Screenshots", "Forest", "KG_Cap_forest_bands.png")

# ---- design constants (named like the design doc) ------------------------------------------------------------------
KG_FOREST_EDGE = 12.0
KG_FOREST_DEEP = 30.0
KG_FOREST_SAFE_MAX = 45.0
KG_VIGIL_CAMP_PATH_MAX = 110.0
KG_VIGIL_CAMP_LANE_MIN = 15.0
R_OUT = 178.0            # the playable forest ends here (the Mist Wall); the mountains start beyond
RING_R = 136.0           # the ring trail
TRAIL_W = 2.4
CELL = 2.0
X0, Y0, NX, NY = -200.0, -200.0, 200, 160
SQUARE = (9.98, 3.76)
SECTORS = ["", "West", "CaveRidge", "North", "EastRidge"]


def pol(r, deg):
    """Bearing deg: 0 = east, 90 = north (-y)."""
    a = math.radians(deg)
    return (round(r * math.cos(a), 2), round(-r * math.sin(a), 2))


def ring_points():
    pts = []
    for d in range(220, -34, -4):
        r = RING_R + 4.0 * math.sin(math.radians(d * 3.0))
        pts.append(pol(r, d))
    return pts


def lerp_line(a, b, n):
    return [(round(a[0] + (b[0] - a[0]) * t / n, 2), round(a[1] + (b[1] - a[1]) * t / n, 2)) for t in range(n + 1)]


def trails():
    ring = ring_points()

    def on_ring(deg):
        return min(ring, key=lambda p: abs(math.degrees(math.atan2(-p[1], p[0])) % 360 - deg % 360))

    spur = {
        "west_spur": [(-104.0, -18.0), (-116.0, -20.0), on_ring(172)],
        "hollow_spur": [(-92.0, -52.0), (-96.0, -72.0), on_ring(134)],
        "north_spur": [(2.0, -56.5), (0.0, -72.0), (-3.0, -92.0), (-2.0, -112.0), on_ring(91)],
        "field_spur": [(62.0, -104.0), (61.0, -113.0), on_ring(62)],
        "east_spur": [(90.0, -60.0), (106.0, -58.0), on_ring(23)],
    }
    out = [{"name": "ring_trail", "points": [list(p) for p in ring], "width": TRAIL_W, "loop": True}]
    for k, v in spur.items():
        pts = []
        for a, b in zip(v, v[1:]):
            n = max(1, int(math.dist(a, b) // 6.0))
            seg = lerp_line(a, b, n)
            pts.extend(seg if not pts else seg[1:])
        out.append({"name": k, "points": [list(p) for p in pts], "width": TRAIL_W})
    return out


def along(pts, s):
    """Point + unit tangent at arc length s along a polyline."""
    acc = 0.0
    for a, b in zip(pts, pts[1:]):
        l = math.dist(a, b)
        if acc + l >= s and l > 0:
            t = (s - acc) / l
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), ((b[0] - a[0]) / l, (b[1] - a[1]) / l)
        acc += l
    a, b = pts[-2], pts[-1]
    l = math.dist(a, b) or 1.0
    return tuple(b), ((b[0] - a[0]) / l, (b[1] - a[1]) / l)


def length(pts):
    return sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))


def side(pts, s, off):
    p, t = along(pts, s)
    return (round(p[0] - t[1] * off, 2), round(p[1] + t[0] * off, 2))


def sector_of(x, y):
    deg = math.degrees(math.atan2(-y, x)) % 360.0
    if 150.0 <= deg < 235.0:
        return 1
    if 112.0 <= deg < 150.0:
        return 2
    if 68.0 <= deg < 112.0:
        return 3
    if deg < 68.0 or deg >= 330.0:
        return 4
    return 0


def main_trails_only():
    T = trails()
    data = {"_doc": "trails only (gen_forest_bands.py --trails)", "trails": T}
    if os.path.exists(OUT_JSON):
        old = json.load(open(OUT_JSON, encoding="utf-8"))
        old["trails"] = T
        data = old
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"), sort_keys=True)
    print(f"KG_FOREST_BANDS trails {len(T)} -> {OUT_JSON}")


def build():
    sys.path.insert(0, HERE)
    import shapely
    from shapely.geometry import LineString, Point, Polygon
    from shapely.ops import unary_union
    import prep_v2_placements as P   # terrain + the village geometry (no side effects on import)

    L = P.L
    T = trails()
    trail_geo = unary_union([LineString(t["points"]) for t in T])
    lanes = unary_union([LineString(l["points"]) for l in L["lanes"] if l["kind"] not in ("pier", "mole")])
    lm = L["landmarks"]
    village = unary_union([P.TOWN.buffer(4.0), P.AREAS.buffer(2.0), P.FIELDS.buffer(3.0), P.WALK.buffer(3.0),
                           P.B_RECTS.buffer(4.0), Point(lm["lighthouse"]["at"][:2]).buffer(25.0),
                           Point(lm["windmill"]["at"][:2]).buffer(16.0)])
    ring = next(t for t in T if t["name"] == "ring_trail")["points"]
    inner = Polygon([(0.0, 60.0)] + [tuple(p) for p in ring] + [(0.0, 60.0)]).buffer(0)

    xs = X0 + CELL * (np.arange(NX) + 0.5)
    ys = Y0 + CELL * (np.arange(NY) + 0.5)
    X, Y = np.meshgrid(xs, ys)
    Z = P.ground(X, Y)
    gx = (P.ground(X + 1.0, Y) - P.ground(X - 1.0, Y)) / 2.0
    gy = (P.ground(X, Y + 1.0) - P.ground(X, Y - 1.0)) / 2.0
    slope = np.hypot(gx, gy)
    nb = L["nav_bounds"]
    R = np.hypot(X, Y)
    on_land = shapely.contains_xy(P.LAND.buffer(-2.0), X, Y) & (Z > 0.8)
    in_bounds = (X > nb["min"][0]) & (X < nb["max"][0]) & (Y > nb["min"][1]) & (Y < nb["max"][1]) & (Z < nb["max"][2])
    walk = on_land & (slope < 0.8) & ~shapely.contains_xy(P.B_RECTS, X, Y)
    vil = shapely.contains_xy(village, X, Y)
    on_trail = shapely.contains_xy(trail_geo.buffer(TRAIL_W / 2.0 + 1.0), X, Y)
    safe = (vil | on_trail) & on_land
    outside = ~shapely.contains_xy(inner, X, Y)
    out = ~in_bounds | (R > R_OUT) | ~on_land

    # exact Euclidean distance (m) to the nearest safe cell (deterministic)
    from scipy.ndimage import distance_transform_edt
    D = distance_transform_edt(~(safe & walk)) * CELL   # the runtime escapes to walkable safe cells only

    band = np.full(X.shape, 2, dtype=np.uint8)
    band[D <= KG_FOREST_EDGE] = 1
    band[(D > KG_FOREST_DEEP) | (outside & (D > KG_FOREST_EDGE))] = 3
    band[vil] = 0
    band[out] = 4
    flags = band.copy()
    flags[~walk] |= 0x40
    flags[safe & ~out] |= 0x80
    sect = np.zeros(X.shape, dtype=np.uint8)
    for j in range(NY):
        for i in range(NX):
            if band[j, i] in (1, 2, 3):
                sect[j, i] = sector_of(X[j, i], Y[j, i])

    forest = (band >= 1) & (band <= 3) & walk
    # ---- camp, dens, lanterns ------------------------------------------------------------------------------------
    north = next(t for t in T if t["name"] == "north_spur")["points"]
    camp = side(north, 26.0, -9.0)
    dens = []
    for sid, deg in ((1, 184.0), (2, 131.0), (3, 97.0), (4, 38.0)):
        best = None
        for r in np.arange(166.0, 150.0, -2.0):
            x, y = pol(float(r), deg)
            i, j = int((x - X0) // CELL), int((y - Y0) // CELL)
            if 0 <= i < NX and 0 <= j < NY and band[j, i] == 3 and walk[j, i]:
                best = (x, y)
                break
        if best:
            dens.append({"id": f"den_{SECTORS[sid].lower()}", "sector": SECTORS[sid], "at": [best[0], best[1]]})
    heads = []
    for t in T[1:]:
        p0 = t["points"][0]
        p1 = t["points"][-1]
        heads.append(side(t["points"], 3.0, 2.2))                       # trail head (village end)
        heads.append(side(t["points"], length(t["points"]) - 3.0, 2.2))  # the ring junction
    lanterns = [{"at": [h[0], h[1]]} for h in heads]

    # ---- lint --------------------------------------------------------------------------------------------------
    viol = []
    fd = D[forest]
    worst = float(fd.max()) if fd.size else 0.0
    n_far = int((fd > KG_FOREST_SAFE_MAX).sum())
    if n_far:
        viol.append(f"{n_far} walkable forest cells farther than KG_FOREST_SAFE_MAX ({worst:.1f} m)")
    lane_d = lanes.distance(Point(camp))
    if lane_d < KG_VIGIL_CAMP_LANE_MIN:
        viol.append(f"camp {lane_d:.1f} m from a village lane (< {KG_VIGIL_CAMP_LANE_MIN})")
    camp_path = grid_path(walk & ~out, (SQUARE[0], SQUARE[1]), camp)
    if camp_path is None or camp_path > KG_VIGIL_CAMP_PATH_MAX:
        viol.append(f"camp path from the square {camp_path} m (> {KG_VIGIL_CAMP_PATH_MAX})")
    if len({d['sector'] for d in dens}) < 2:
        viol.append("dens in fewer than 2 sectors")

    per = {}
    for b, bn in ((1, "edge"), (2, "middle"), (3, "deep")):
        for s in range(1, 5):
            per[f"{bn}_{SECTORS[s]}"] = int(((band == b) & walk & (sect == s)).sum() * CELL * CELL)
    stats = {"walkable_forest_m2": int(forest.sum() * CELL * CELL), "per_band_sector_m2": per,
             "worst_distance_to_safe_m": round(worst, 1), "camp_path_m": camp_path,
             "camp_lane_m": round(lane_d, 1), "dens": len(dens), "trail_m": round(sum(length(t['points']) for t in T), 1)}
    raster = base64.b64encode(flags.tobytes()).decode()
    sectors = base64.b64encode(sect.tobytes()).decode()
    camp_z = float(P.g1(*camp))
    for t in T:
        q = t["points"][len(t["points"]) // 2]
        t["mid_z"] = round(float(P.g1(q[0], q[1])), 2)   # navcheck probe height (Tools/Unreal/kg_capture_v2.py)
    data = {
        "_doc": "Generated by Tools/Level/gen_forest_bands.py - do not edit (see its docstring).",
        "version": 1, "cell": CELL, "x0": X0, "y0": Y0, "nx": NX, "ny": NY,
        "consts": {"edge": KG_FOREST_EDGE, "deep": KG_FOREST_DEEP, "safe_max": KG_FOREST_SAFE_MAX, "r_out": R_OUT},
        "square": list(SQUARE), "sector_names": SECTORS, "trails": T,
        "camp": {"at": [camp[0], camp[1]], "z": round(camp_z, 2)}, "dens": dens, "lanterns": lanterns,
        "raster": raster, "sectors": sectors, "stats": stats,
    }
    return data, viol, (X, Y, band, walk, D, T, camp, dens, lanterns), P


def grid_path(walk, a, b):
    """Dijkstra over walkable 2 m cells (8-neighbour); metres or None."""
    import heapq
    ia, ja = int((a[0] - X0) // CELL), int((a[1] - Y0) // CELL)
    ib, jb = int((b[0] - X0) // CELL), int((b[1] - Y0) // CELL)
    dist = {(ia, ja): 0.0}
    pq = [(0.0, ia, ja)]
    while pq:
        d, i, j = heapq.heappop(pq)
        if (i, j) == (ib, jb):
            return round(d, 1)
        if d > dist.get((i, j), 1e9):
            continue
        for di, dj, w in ((1, 0, CELL), (-1, 0, CELL), (0, 1, CELL), (0, -1, CELL), (1, 1, CELL * 1.4142),
                          (1, -1, CELL * 1.4142), (-1, 1, CELL * 1.4142), (-1, -1, CELL * 1.4142)):
            ni, nj = i + di, j + dj
            if 0 <= ni < NX and 0 <= nj < NY and (walk[nj, ni] or (ni, nj) == (ib, jb)):
                nd = d + w
                if nd < dist.get((ni, nj), 1e9):
                    dist[(ni, nj)] = nd
                    heapq.heappush(pq, (nd, ni, nj))
    return None


# ================================================================================================ forest chores
def forest_chores(data, P):
    """The four SPRINT-034a chores + anchors. Every step stays in Edge/Middle (lint below)."""
    T = {t["name"]: t["points"] for t in data["trails"]}
    camp = tuple(data["camp"]["at"])
    ch = next(a for a in P.L["areas"] if a["name"] == "church_yard")["center"]
    wc = next(a for a in P.L["areas"] if a["name"] == "woodcutter")["center"]
    A = []

    def anchor(i, kind, at, label, r=1.6, yaw=0.0):
        A.append({"id": i, "kind": kind, "at": [round(at[0], 2), round(at[1], 2)], "z": round(float(P.g1(*at)), 2),
                  "r": r, "yaw": yaw, "label": label})

    anchor("forest_camp_woodpile", "CampWoodpile", (camp[0] + 3.0, camp[1] + 2.5), "the camp woodpile", 2.0)
    anchor("forest_tent_site", "TentSite", (camp[0] - 4.5, camp[1] - 3.0), "the camp tent site", 2.4)
    for k, (dx, dy) in enumerate(((-7.0, -3.2), (-4.5, -6.2), (-2.0, -3.0))):
        anchor(f"forest_peg_{k + 1}", "TentPeg", (camp[0] + dx, camp[1] + dy), f"tent peg {k + 1}", 1.0)
    anchor("forest_canvas", "CanvasPile", (wc[0] + 3.0, wc[1] + 4.0), "the canvas by the woodcutter's", 1.8)
    anchor("forest_herb_table", "HerbTable", (ch[0] - 4.0, ch[1] + 1.0), "the herb table by the church", 1.6)
    # deadwood: 3 piles 13-16 m off a trail (Middle band, short), two variants
    for var, tn, s0, sg in (("n", "north_spur", 34.0, 1.0), ("w", "north_spur", 38.0, -1.0)):
        for k in range(3):
            anchor(f"forest_deadwood_{var}{k + 1}", "Deadwood", side(T[tn], s0 + 8.0 * k, (13.5 + k) * sg),
                   "a pile of dry branches", 1.6)
    # herbs: 3 clusters 7-10 m off a trail (Edge/Middle), two variants
    for var, tn, s0, sg in (("w", "north_spur", 27.0, -1.0), ("n", "north_spur", 44.0, 1.0)):
        for k in range(3):
            anchor(f"forest_herbs_{var}{k + 1}", "Herbs", side(T[tn], s0 + 6.0 * k, (6.0 + 2.0 * k) * sg),
                   "a clump of wild herbs", 1.4)
    # trail lanterns: 5 along a trail, two variants
    for var, tn, s0 in (("n", "north_spur", 34.0), ("w", "west_spur", 4.0)):
        for k in range(5):
            s = s0 + 7.5 * k
            anchor(f"forest_lantern_{var}{k + 1}", "Lamp", side(T[tn], s, 2.2), f"trail lantern {k + 1}", 1.4,
                   yaw=0.0)
    chores = [
        {"id": "GatherDeadwood", "title": "Gather deadwood",
         "blurb": "Dry branches from three piles off the trail, tied into a bundle for the camp woodpile.",
         "bots": True,
         "variants": [{"name": "north", "vars": {"p1": "forest_deadwood_n1", "p2": "forest_deadwood_n2", "p3": "forest_deadwood_n3"}},
                      {"name": "north_east", "vars": {"p1": "forest_deadwood_w1", "p2": "forest_deadwood_w2", "p3": "forest_deadwood_w3"}}],
         "steps": [
             {"verb": "work", "at": ["$p1", "$p2", "$p3"], "secs": 2.0, "spawn": "Bundle", "cue": "Chop",
              "label": "Snap dry branches off the three piles"},
             {"verb": "bring", "at": "forest_camp_woodpile", "item": "Bundle", "secs": 0.8, "consume": True,
              "effect": "stack", "cue": "Thud", "label": "Stack the bundle on the camp woodpile"}]},
        {"id": "PitchCamp", "title": "Pitch camp",
         "blurb": "Carry the canvas out to the camp (easier with a friend), then drive three pegs.",
         "bots": True,
         "steps": [
             {"verb": "take", "at": "forest_canvas", "item": "Canvas", "secs": 0.8, "cue": "Thud",
              "label": "Pick up the tent canvas at the woodcutter's"},
             {"verb": "bring", "at": "forest_tent_site", "item": "Canvas", "secs": 1.0, "consume": True,
              "effect": "stack", "cue": "Whoosh", "label": "Spread the canvas at the camp"},
             {"verb": "work", "at": ["forest_peg_1", "forest_peg_2", "forest_peg_3"], "secs": 1.5, "cue": "Thud",
              "effect": "stack", "label": "Drive the three tent pegs"}]},
        {"id": "TrailLanterns", "title": "Light the trail lanterns",
         "blurb": "A lit taper from the town hall and five lanterns along a forest trail.",
         "bots": True,
         "variants": [{"name": "north", "vars": {f"l{k}": f"forest_lantern_n{k}" for k in range(1, 6)}},
                      {"name": "west", "vars": {f"l{k}": f"forest_lantern_w{k}" for k in range(1, 6)}}],
         "steps": [
             {"verb": "take", "at": "taper_box", "item": "Taper", "secs": 0.6, "cue": "Flame",
              "label": "Take a lit taper from the town hall box"},
             {"verb": "bring", "at": ["$l1", "$l2", "$l3", "$l4", "$l5"], "item": "Taper", "secs": 0.8, "consume": True,
              "effect": "light", "cue": "Flame", "label": "Light the five trail lanterns"}]},
        {"id": "GatherHerbs", "title": "Gather herbs",
         "blurb": "Three clumps of wild herbs by the trail, for the herb table at the church.",
         "bots": True,
         "variants": [{"name": "crown", "vars": {"h1": "forest_herbs_w1", "h2": "forest_herbs_w2", "h3": "forest_herbs_w3"}},
                      {"name": "north", "vars": {"h1": "forest_herbs_n1", "h2": "forest_herbs_n2", "h3": "forest_herbs_n3"}}],
         "steps": [
             {"verb": "work", "at": ["$h1", "$h2", "$h3"], "secs": 1.5, "spawn": "Herbs", "cue": "Paper",
              "label": "Pick the three clumps of herbs"},
             {"verb": "bring", "at": "forest_herb_table", "item": "Herbs", "secs": 0.8, "consume": True,
              "effect": "stack", "cue": "Thud", "label": "Lay the herbs on the church table"}]},
    ]
    items = {
        "Canvas": {"mesh": "/Game/KillGodot/Env/KG_Props/StaticMeshes/Bag.Bag", "scale": [1.6, 0.9, 0.6],
                   "box": [45, 25, 16], "mesh_z": -16, "mass": 18, "speed": 0.6, "two_person": True,
                   "label": "rolled tent canvas", "tint": [0.78, 0.70, 0.52]},
        "Herbs": {"mesh": "/Game/KillGodot/Env/KG_Nature/StaticMeshes/Flower_3_Group.Flower_3_Group", "scale": 0.5,
                  "box": [16, 16, 10], "mesh_z": -8, "mass": 1, "speed": 1.0, "label": "bunch of herbs"},
    }
    return A, chores, items


def merge_chores(A, chores, items):
    D = json.load(open(CHORES, encoding="utf-8"))
    ids = {c["id"] for c in chores}
    D["anchors"] = [a for a in D["anchors"] if not a["id"].startswith("forest_")] + A
    D["chores"] = [c for c in D["chores"] if c["id"] not in ids] + chores
    D["items"].update(items)
    with open(CHORES, "w", encoding="utf-8") as f:
        json.dump(D, f, indent=1, ensure_ascii=False)
        f.write("\n")


def chore_lint(data, A, band_at):
    viol, info = [], {}
    for a in A:
        b = band_at(*a["at"])
        info[a["id"]] = ["village", "edge", "middle", "deep", "out"][b & 7]
        if (b & 7) == 3:
            viol.append(f"chore anchor {a['id']} lies in the Deep band")
        if a["kind"] == "Herbs" and (b & 7) not in (1, 2):
            viol.append(f"herb cluster {a['id']} not in Edge/Middle")
    return viol, info


def write_outputs(data):
    text = json.dumps(data, separators=(",", ":"), sort_keys=True)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        f.write(text)
    chunks = [text[i:i + 6000] for i in range(0, len(text), 6000)]   # MSVC: a wide literal must stay under 16 KB
    os.makedirs(os.path.dirname(OUT_INL), exist_ok=True)
    with open(OUT_INL, "w", encoding="utf-8") as f:
        f.write("// Generated by Tools/Level/gen_forest_bands.py - do not edit.\n")
        f.write("static const TCHAR* const GKGForestJsonChunks[] = {\n")
        for c in chunks:
            f.write('\tTEXT(R"KGF(' + c + ')KGF"),\n')
        f.write("};\n")
    return hashlib.sha256(text.encode()).hexdigest()


def draw(ctx, viol):
    from PIL import Image, ImageDraw
    X, Y, band, walk, D, T, camp, dens, lanterns = ctx
    col = {0: (200, 190, 170), 1: (150, 210, 120), 2: (70, 150, 70), 3: (25, 70, 40), 4: (90, 90, 110)}
    img = Image.new("RGB", (NX * 4, NY * 4))
    px = img.load()
    for j in range(NY):
        for i in range(NX):
            c = col[int(band[j, i])]
            if not walk[j, i]:
                c = tuple(v // 2 for v in c)
            for a in range(4):
                for b in range(4):
                    px[i * 4 + a, j * 4 + b] = c
    d = ImageDraw.Draw(img)

    def to(p):
        return ((p[0] - X0) / CELL * 4, (p[1] - Y0) / CELL * 4)

    for t in T:
        d.line([to(p) for p in t["points"]], fill=(230, 180, 90), width=4)
    for l in lanterns:
        x, y = to(l["at"])
        d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(255, 220, 90))
    x, y = to(camp)
    d.ellipse([x - 8, y - 8, x + 8, y + 8], fill=(255, 90, 20))
    for dn in dens:
        x, y = to(dn["at"])
        d.rectangle([x - 6, y - 6, x + 6, y + 6], outline=(220, 40, 40), width=3)
    d.text((8, 8), "bands: village / edge / middle / deep / out; trails, lanterns, camp (orange), dens (red)", fill=(255, 255, 255))
    if viol:
        d.text((8, 22), "LINT: " + "; ".join(viol)[:180], fill=(255, 80, 80))
    os.makedirs(os.path.dirname(IMAGE), exist_ok=True)
    img.save(IMAGE)


def main():
    if "--trails" in sys.argv:
        main_trails_only()
        return 0
    data, viol, ctx, P = build()
    X, Y, band, walk, D, T, camp, dens, lanterns = ctx

    def band_at(x, y):
        i, j = int((x - X0) // CELL), int((y - Y0) // CELL)
        return int(band[j, i]) if 0 <= i < NX and 0 <= j < NY else 4

    A, chores, items = forest_chores(data, P)
    cv, cinfo = chore_lint(data, A, band_at)
    viol += cv
    # rough scripted-player route times (grid paths at walking speed 3.2 m/s + dwell), acceptance range 45-90 s
    walkable = walk & (band < 4)
    at = {a["id"]: tuple(a["at"]) for a in A}
    at["taper_box"] = tuple(next(a for a in json.load(open(CHORES, encoding="utf-8"))["anchors"] if a["id"] == "taper_box")["at"])
    routes = {}
    for c in chores:
        vs = c.get("variants") or [{"name": "only", "vars": {}}]
        for v in vs:
            pos, tot = SQUARE, 0.0
            for s in c["steps"]:
                tg = s["at"] if isinstance(s["at"], list) else [s["at"]]
                for t in tg:
                    t = v["vars"].get(t[1:], t) if t.startswith("$") else t
                    p = grid_path(walkable, pos, at[t]) or math.dist(pos, at[t]) * 1.3
                    tot += p / 3.2 + s.get("secs", 1.0)
                    pos = at[t]
            routes[f"{c['id']}/{v['name']}"] = round(tot, 1)
    data["stats"]["chore_route_s"] = routes
    h = write_outputs(data)
    if "--check" in sys.argv:
        data2, _, _, _ = build()
        data2["stats"]["chore_route_s"] = routes
        h2 = hashlib.sha256(json.dumps(data2, separators=(",", ":"), sort_keys=True).encode()).hexdigest()
        if h2 != h:
            viol.append("non-deterministic output (two runs differ)")
        data["stats"]["determinism"] = "identical" if h2 == h else "DIFFERENT"
    merge_chores(A, chores, items)
    per = data["stats"]["per_band_sector_m2"]
    rep = {"stats": data["stats"], "violations": viol, "chore_anchor_bands": cinfo, "sha256": h,
           "decision": ("SPRINT-033a O10: the user chose option (b) - the playable forest was widened (terrain ring pushed "
                        f"out to r {R_OUT:.0f} m, nav_bounds widened) instead of shrinking KG_FOREST_EDGE/DEEP "
                        f"(kept at {KG_FOREST_EDGE:.0f}/{KG_FOREST_DEEP:.0f} m)."),
           "deep_m2": sum(v for k, v in per.items() if k.startswith("deep"))}
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    json.dump(rep, open(REPORT, "w"), indent=1)
    draw(ctx, viol)
    print(f"KG_FOREST_BANDS forest {data['stats']['walkable_forest_m2']} m2 (deep {rep['deep_m2']} m2), worst "
          f"{data['stats']['worst_distance_to_safe_m']} m, camp path {data['stats']['camp_path_m']} m, dens {len(dens)}, "
          f"routes {routes}")
    for v in viol:
        print("KG_FOREST_BANDS VIOLATION", v)
    return 1 if viol else 0


if __name__ == "__main__":
    sys.exit(main())
