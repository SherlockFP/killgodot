"""Morrowmere v2: compute every geometry-heavy placement for the Unreal builder (system Python: numpy + shapely).

    python Tools/Level/prep_v2_placements.py            -> Art/Packed/KG_V2_Placements.json

Run after Tools/Blender/kg_build_terrain_v2.py (reads Art/Packed/KG_Terrain_v2_heights.json) and before
Tools/Unreal/kg_build_village_v2.py (UE Python has no numpy/shapely, so it only spawns what this file lists).

Output (UE centimetres, degrees):
  items : static meshes  {m: "<prefix>:<name>", p: [x,y,z], y/pi/ro: yaw/pitch/roll, s: [sx,sy,sz], c: collide 0/1,
          h: hidden-in-game 0/1, f: outliner folder, d: district (material tint) or absent}
  hism  : {"forest"|"crops": {mesh: [[x,y,z,yaw,scale], ...]}}  (AKGFoliageField, "forest" collides)
  grass : 4 meadow layers of [x,y,z,yaw,s,sz] for AKGGrassField.add_clumps
  lamps : [[x,y,z,yaw]]  lamp posts (arm along local +Y)       starts: [[x,y,z,yaw]]
  water : brook / pond / spill planes (M_KG_PondWater)            stats: counts
Mesh prefixes: V village kit, N nature, P props, JP japan, PIR pirate, DV/DW/DH dress village/wilds/harbour, E engine,
DT the plan 11.4 pack KG_DressTerrace (Tools/Blender/kg_make_dress_terrace.py: balustrades, quay wall + steps, stone
arch bridge; used when Art/Packed/KG_DressTerrace_Clean.json exists and KG_NO_TERRACE is unset, kit fallbacks otherwise).
Kit conventions (measured): wall pieces are 2 m wide, exterior faces local -Y, span y -0.31..+0.09, 3.12 m tall;
Stairs_Exterior_Straight_* climb 1 m toward local +Y over y -1.08..+1.0 (2.08 m run), 2 m wide.
"""
import json
import math
import os
import sys

import numpy as np
import shapely
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import unary_union

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "Blender"))
import kg_build_terrain_v2 as T  # noqa: E402  (stair_profile, rect_poly, fbm - no Blender needed)

L = json.load(open(os.path.join(HERE, "morrowmere_layout_v2.json"), encoding="utf-8"))
HM = json.load(open(os.path.join(ROOT, "Art", "Packed", "KG_Terrain_v2_heights.json")))
OUT_PATH = os.path.join(ROOT, "Art", "Packed", "KG_V2_Placements.json")
rng = np.random.default_rng(20260924)

# ------------------------------------------------------------------------------------------------ terrain sampling
_core = HM["core"]
_ch = np.array(_core["heights"], dtype=np.float64).reshape(_core["ny"], _core["nx"])
_oh = np.array(HM["heights"], dtype=np.float64).reshape(HM["n"] + 1, HM["n"] + 1)


def ground(x, y):
    """Terrain z (m) at UE metres; vectorised. Core grid first, outer grid (Blender-row layout) elsewhere."""
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)
    fi = (x - _core["x0"]) / _core["step"]
    fj = (y - _core["y0"]) / _core["step"]
    inside = (fi >= 0) & (fj >= 0) & (fi <= _core["nx"] - 1.001) & (fj <= _core["ny"] - 1.001)
    out = np.zeros(np.broadcast(x, y).shape)
    if inside.any():
        i = np.floor(fi[inside]).astype(int)
        j = np.floor(fj[inside]).astype(int)
        tx, ty = fi[inside] - i, fj[inside] - j
        a, b = _ch[j, i], _ch[j, i + 1]
        c, d = _ch[j + 1, i], _ch[j + 1, i + 1]
        out[inside] = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty
    o = ~inside
    if o.any():
        size, step, n = HM["size"], HM["step"], HM["n"]
        bi = np.clip((x[o] + size / 2) / step, 0, n - 1.001)
        bj = np.clip((-y[o] + size / 2) / step, 0, n - 1.001)
        i = np.floor(bi).astype(int)
        j = np.floor(bj).astype(int)
        tx, ty = bi - i, bj - j
        a, b = _oh[j, i], _oh[j, i + 1]
        c, d = _oh[j + 1, i], _oh[j + 1, i + 1]
        out[o] = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty
    return out


def g1(x, y):
    return float(ground(np.array([x]), np.array([y]))[0])


# ------------------------------------------------------------------------------------------------ output helpers
items, lamps, water = [], [], []
hism = {"forest": {}, "crops": {}}
stats = {}


def put(m, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, s=(1.0, 1.0, 1.0), c=True, f="V2", h=False, d=None):
    it = {"m": m, "p": [round(x * 100, 1), round(y * 100, 1), round(z * 100, 1)], "y": round(yaw, 2), "f": f}
    if pitch:
        it["pi"] = round(pitch, 2)
    if roll:
        it["ro"] = round(roll, 2)
    if tuple(s) != (1.0, 1.0, 1.0):
        it["s"] = [round(v, 4) for v in s]
    if not c:
        it["c"] = 0
    if h:
        it["h"] = 1
    if d:
        it["d"] = d
    items.append(it)
    stats[f.split("/")[1] if "/" in f else f] = stats.get(f.split("/")[1] if "/" in f else f, 0) + 1
    return it


def inst(bucket, mesh, x, y, z, yaw, sc):
    hism[bucket].setdefault(mesh, []).append([round(x * 100, 1), round(y * 100, 1), round(z * 100, 1), round(yaw, 1),
                                              round(sc, 3)])


def yaw_face(nx, ny):
    """Actor yaw so local -Y (kit exterior / house front) points along (nx, ny)."""
    return math.degrees(math.atan2(nx, -ny))


def yaw_up(ux, uy):
    """Actor yaw so local +Y points along (ux, uy) (kit stairs climb toward +Y)."""
    return math.degrees(math.atan2(-ux, uy))


def yaw_x(ux, uy):
    return math.degrees(math.atan2(uy, ux))


def norm(vx, vy):
    n = math.hypot(vx, vy)
    return (vx / n, vy / n) if n > 1e-9 else (1.0, 0.0)


# ------------------------------------------------------------------------------------------------ layout geometry
def rect(b, pad=0.0):
    return Polygon(T.rect_poly(b, pad))


BUILDINGS = [b for b in L["houses"]] + [b for b in L["infill"]] + [
    dict(b, _key=k) for k, b in L["landmarks"].items() if "size" in b and not b.get("is_prop")]
B_RECTS = unary_union([rect(b) for b in BUILDINGS])
STAIR_P = {s["name"]: T.stair_profile(s) for s in L["stairs"]}
CORRIDORS = unary_union([LineString([s["from"], s["to"]]).buffer(s["width"] / 2 + 0.3, cap_style=2) for s in L["stairs"]] +
                        [LineString(r["points"]).buffer(r["width"] / 2 + 0.3, cap_style=2) for r in L["ramps"]])
LANES = unary_union([LineString(l["points"]).buffer(l["width"] / 2.0, cap_style=2) for l in L["lanes"]])
SQUARES = unary_union([Polygon(q["polygon"]) for q in L["squares"]])
AREAS = unary_union([Point(a["center"]).buffer(a["radius"]) for a in L["areas"]])
WALK = unary_union([LANES, SQUARES, CORRIDORS])
ST = L["stream"]
STREAM = LineString([p[:2] for p in ST["points"]])
LAND = Polygon(L["coast"]["land_polygon"]).buffer(0)
TERR = {t["name"]: Polygon(t["polygon"]).buffer(0) for t in L["terraces"]}
TOWN = unary_union([TERR[n] for n in ("quay", "heart", "upper", "garden", "crown")])
FIELDS = unary_union([Polygon(f["polygon"]) for f in L["fields"]])
SIGHT = unary_union([LineString([s["from"], s["to"]]).buffer(4.0) for s in L["sightlines"]])
DISTRICT_OF_TERRACE = {t["name"]: t["district"] for t in L["terraces"]}


def district_at(x, y):
    p = Point(x, y)
    for n, g in TERR.items():
        if g.covers(p):
            return DISTRICT_OF_TERRACE[n]
    return None


def resample(pts, step=2.0):
    """Split a polyline into n pieces of <= step: [(centre, tangent, length, s_centre)]."""
    line = LineString(pts)
    S = line.length
    n = max(1, int(math.ceil(S / step - 1e-6)))
    st = S / n
    out = []
    for k in range(n):
        sc = (k + 0.5) * st
        a = line.interpolate(max(0.0, sc - st / 2))
        b = line.interpolate(min(S, sc + st / 2))
        c = line.interpolate(sc)
        out.append(((c.x, c.y), norm(b.x - a.x, b.y - a.y), st, sc))
    return out, S


# ================================================================================================ retaining walls
WALL = "V:Wall_UnevenBrick_Straight"
TERRACE = (os.path.exists(os.path.join(ROOT, "Art", "Packed", "KG_DressTerrace_Clean.json"))
           and not os.environ.get("KG_NO_TERRACE"))
BALUSTRADE_WALLS = ("harbour_wall_", "crown_wall_")   # the square's sea window over the Grand Stair; the Belvedere
QUAY_FACE = 0.4                                        # m: QuayWall_4m sea face seaward of the layout's quay edge


def wall_stack(x, y, yaw, top, bottom, length, sy=1.6, f="V2/Walls", d=None, cap=True, min_courses=1):
    """Kit wall courses (3 m pitch, 3.12 tall) from `top` (+0.12 lip) down past `bottom`."""
    h = top - bottom
    n = max(min_courses, int(math.ceil(h / 3.0 - 1e-6)))
    base = top - n * 3.0
    for k in range(n):
        put(WALL, x, y, base + k * 3.0, yaw, s=(length / 2.0, sy, 1.0), f=f, d=d)
    if cap:
        put("V:Wall_BottomCover", x, y, top + 0.12, yaw, s=(length / 2.0, sy * 0.95, 1.0), f=f, d=d)
    return n


def retaining_wall(pts, top, bottom, courses, parapet, district=None, name="", skip=None, cap=True):
    pieces, S = resample(pts, 2.0)
    prev_t, since = None, 99.0
    balustrade = bool(parapet) and TERRACE and name.startswith(BALUSTRADE_WALLS)
    prev_rail = None        # (end x, end y, yaw) of the last balustrade section, for the newel posts
    rails_since = 0
    for k, ((cx, cy), (tx, ty), st, sc) in enumerate(pieces):
        n1 = (-ty, tx)
        za = g1(cx + n1[0] * 1.6, cy + n1[1] * 1.6)
        zb = g1(cx - n1[0] * 1.6, cy - n1[1] * 1.6)
        low = n1 if za < zb else (-n1[0], -n1[1])
        if skip is not None and skip.covers(Point(cx, cy)):
            continue
        yaw = yaw_face(*low)
        px, py = cx + low[0] * 0.06, cy + low[1] * 0.06
        n = wall_stack(px, py, yaw, top, bottom, st, f="V2/Walls", d=district, cap=cap and not parapet,
                       min_courses=courses or 1)
        # pilasters at the ends, kinks > 4 deg and every 8 m
        kink = prev_t is not None and math.degrees(math.acos(max(-1, min(1, prev_t[0] * tx + prev_t[1] * ty)))) > 4.0
        since += st
        if k == 0 or kink or since >= 8.0 or k == len(pieces) - 1:
            e = (k == len(pieces) - 1)
            ex = cx + tx * (st / 2 if e else -st / 2)
            ey = cy + ty * (st / 2 if e else -st / 2)
            put("V:Corner_Exterior_Brick", ex + low[0] * 0.33, ey + low[1] * 0.33, top - n * 3.0, yaw,
                s=(1.3, 1.3, (n * 3.0 + 0.2) / 3.01), f="V2/Walls", d=district)
            since = 0.0
        prev_t = (tx, ty)
        if parapet:
            blocked = B_RECTS.distance(Point(cx, cy)) < 0.5 or CORRIDORS.distance(Point(cx, cy)) < 0.2
            if balustrade:
                # plan 11.4 Balustrade_2m on the wall body (kit wall spans cx-0.03 .. cx+0.37 toward the low side),
                # newel posts at both ends of every run, at kinks, beside stair heads and every third section
                bx, by = cx + low[0] * 0.18, cy + low[1] * 0.18
                ryaw = yaw_x(tx, ty)
                sx0, sy0 = bx - tx * st / 2, by - ty * st / 2
                if blocked:
                    if prev_rail:
                        put("DT:Balustrade_Post", prev_rail[0], prev_rail[1], top + 0.1, prev_rail[2], f="V2/Parapets")
                    prev_rail = None
                    continue
                if prev_rail is None or kink or rails_since >= 3:
                    put("DT:Balustrade_Post", sx0, sy0, top + 0.1, ryaw, f="V2/Parapets")
                    rails_since = 0
                put("DT:Balustrade_2m", bx, by, top + 0.1, ryaw, s=(st / 2.0, 1.0, 1.0), f="V2/Parapets")
                rails_since += 1
                prev_rail = (bx + tx * st / 2, by + ty * st / 2, ryaw)
                continue
            if blocked:
                continue
            put(WALL, cx + low[0] * 0.14, cy + low[1] * 0.14, top, yaw, s=(st / 2.0, 1.0, 0.33), f="V2/Parapets",
                d=district)
    if balustrade and prev_rail:
        put("DT:Balustrade_Post", prev_rail[0], prev_rail[1], top + 0.1, prev_rail[2], f="V2/Parapets")
    return S


def quay_wall_pack(q, skip):
    """Plan 11.4 QuayWall_4m: granite sections (coping at the quay top, sea face on the edge line, local +Y to the
    sea) on 4 m chords of the quay edge, a mooring-ring section every third; gaps at the jetty / fish-pier ramps."""
    line = LineString(q["edge"])
    S = line.length
    n = max(1, int(math.ceil(S / 4.0 - 1e-6)))
    st = S / n
    for k in range(n):
        a, b = line.interpolate(k * st), line.interpolate((k + 1) * st)
        cx, cy = (a.x + b.x) / 2, (a.y + b.y) / 2
        if skip is not None and skip.covers(Point(cx, cy)):
            continue
        tx, ty = norm(b.x - a.x, b.y - a.y)
        n1 = (-ty, tx)
        sea = n1 if not LAND.covers(Point(cx + n1[0] * 1.5, cy + n1[1] * 1.5)) else (-n1[0], -n1[1])
        # sea face 0.37 m out from the edge line, like the kit wall's exterior: the terrain's quay cliff (0.5 m grid
        # steps) stays hidden behind the granite
        put("DT:QuayWall_4m_Ring" if k % 3 == 1 else "DT:QuayWall_4m", cx + sea[0] * QUAY_FACE, cy + sea[1] * QUAY_FACE,
            q["top_z"], yaw_up(*sea), s=(math.hypot(b.x - a.x, b.y - a.y) / 4.0 + 0.01, 1.0, 1.0), f="V2/Walls",
            d=None)


def walls():
    for w in L["retaining_walls"]:
        top_terr = district_for_wall(w)
        if w["top_z"] == w["bottom_z"]:
            # marker: 1 m field wall between level terraces
            pieces, _ = resample(w["points"], 2.0)
            for (cx, cy), (tx, ty), st, _ in pieces:
                put(WALL, cx, cy, g1(cx, cy) - 0.15, yaw_face(-ty, tx), s=(st / 2, 1.0, 0.36), f="V2/Walls")
            continue
        retaining_wall(w["points"], w["top_z"], w["bottom_z"], w.get("courses", 1), w.get("parapet"), top_terr, w["name"])
    # the quay wall: 2 courses from 2.0 down into the basin, gaps where the jetty / fish pier ramps cross
    q = L["quay"]
    def ramp_gap(r):
        """Ramp corridor extended 3 m past its foot (where the pier starts), so no wall piece stands across it."""
        (ax, ay), (bx, by) = r["points"][0], r["points"][-1]
        u = norm(bx - ax, by - ay)
        return LineString([(ax, ay), (bx + u[0] * 3.0, by + u[1] * 3.0)]).buffer(r["width"] / 2 + 0.4, cap_style=2)

    skip = unary_union([ramp_gap(r) for r in L["ramps"] if r["name"] in ("jetty_ramp", "fish_pier_ramp")])
    if TERRACE:
        quay_wall_pack(q, skip)
    else:
        retaining_wall(q["edge"], q["top_z"], q["bottom_z"] - 1.0, 2, False, "harbour_row", "quay", skip=skip)
    # bollards every 8 m, 0.7 m inland
    line = LineString(q["edge"])
    s = 4.0
    while s < line.length - 2:
        p = line.interpolate(s)
        a, b = line.interpolate(max(0, s - 0.5)), line.interpolate(min(line.length, s + 0.5))
        tx, ty = norm(b.x - a.x, b.y - a.y)
        n1 = (-ty, tx)
        inland = n1 if LAND.covers(Point(p.x + n1[0] * 2, p.y + n1[1] * 2)) else (-n1[0], -n1[1])
        bx, by = p.x + inland[0] * 0.75, p.y + inland[1] * 0.75
        if skip.distance(Point(bx, by)) > 2.0 and min(math.dist((bx, by), ws) for ws in q["water_steps"]) > 3.0:
            put("DH:Bollard", bx, by, 2.0, yaw_face(-inland[0], -inland[1]) - 90.0, f="V2/Quay")
        s += 8.0
    # water steps: a kit flight along the wall, outside it, down from the quay into the basin
    for ws in q["water_steps"]:
        d, sa = 1e9, 0.0
        sp = line.project(Point(ws))
        a, b = line.interpolate(max(0, sp - 1)), line.interpolate(min(line.length, sp + 1))
        tx, ty = norm(b.x - a.x, b.y - a.y)
        n1 = (-ty, tx)
        sea = n1 if not LAND.covers(Point(ws[0] + n1[0] * 2, ws[1] + n1[1] * 2)) else (-n1[0], -n1[1])
        if TERRACE:
            # QuaySteps: top tread at the quay (local 0), 12 x 0.25 m down along local +X, 1.3 m out to sea (+Y)
            lx, ly = sea[1], -sea[0]                               # local +X for yaw_up(sea)
            put("DT:QuaySteps", ws[0] - lx * 2.1 + sea[0] * QUAY_FACE, ws[1] - ly * 2.1 + sea[1] * QUAY_FACE, q["top_z"],
                yaw_up(*sea), f="V2/Quay")
            continue
        ox, oy = ws[0] + sea[0] * 1.6, ws[1] + sea[1] * 1.6      # centreline 1.6 m out from the wall line
        # climb direction +t: foot 3 risers away along -t
        for k in range(3):
            sc = -3 * 2.08 + k * 2.08 + 1.08
            put("V:Stairs_Exterior_Straight", ox + tx * sc, oy + ty * sc, -1.0 + k, yaw_up(tx, ty), f="V2/Quay",
                d=None)
        put("V:Floor_Brick", ox + tx * 1.0, oy + ty * 1.0, 2.0, yaw_up(tx, ty), s=(1.0, 1.0, 1.0), f="V2/Quay")


def district_for_wall(w):
    n = w["name"]
    for key, dist in (("harbour", "heart"), ("sakura", "sakura_garden"), ("garden_west", "sakura_garden"),
                      ("upper", "upper_town"), ("crown", "crown_hill")):
        if n.startswith(key):
            return dist
    return None


# ================================================================================================ stairs + ramps
RISERS = {1: ["V:Stairs_Exterior_Straight"]}


def riser_row(n):
    if n == 1:
        return ["V:Stairs_Exterior_Straight"]
    return ["V:Stairs_Exterior_Straight_L"] + ["V:Stairs_Exterior_Straight_Center"] * (n - 2) + ["V:Stairs_Exterior_Straight_R"]


def side_walls(pts_fn, length, zfn, half_w, f, near_ok):
    """Cheek walls along a stair/ramp corridor where the terrain beside it differs by > 0.35 m."""
    n = max(1, int(math.ceil(length / 2.0)))
    st = length / n
    for sgn in (-1.0, 1.0):
        for k in range(n):
            s0, s1 = k * st, (k + 1) * st
            sm = (s0 + s1) / 2
            (cx, cy), (ux, uy) = pts_fn(sm)
            px, py = -uy * sgn, ux * sgn                     # outward normal of this side
            zs = [zfn(s0), zfn(sm), zfn(s1)]
            side = [g1(*[v for v in (pts_fn(s)[0][0] + px * (half_w + 1.0), pts_fn(s)[0][1] + py * (half_w + 1.0))])
                    for s in (s0, sm, s1)]
            diff_hi = max(sd - sz for sd, sz in zip(side, zs))
            diff_lo = max(sz - sd for sd, sz in zip(side, zs))
            wx, wy = cx + px * (half_w + 0.55), cy + py * (half_w + 0.55)
            if B_RECTS.distance(Point(wx, wy)) < 0.3 or near_ok(wx, wy):
                continue
            if diff_hi > 0.35:      # cutting: wall faces into the corridor, top at the terrain
                wall_stack(wx, wy, yaw_face(-px, -py), max(side) - 0.02, min(zs), st, sy=2.0, f=f, cap=True)
            elif diff_lo > 0.35:    # embankment: wall faces outward, top a little over the tread line
                ex, ey = cx + px * (half_w + 0.3), cy + py * (half_w + 0.3)
                wall_stack(ex, ey, yaw_face(px, py), max(zs) + 0.05, min(side), st, sy=2.0, f=f, cap=True)


def stairs():
    for s in L["stairs"]:
        P = STAIR_P[s["name"]]
        lo, u, length = P["lo"], P["u"], P["length"]
        perp = (u[1], -u[0])              # local +X when local +Y = u (UE yaw convention)
        yaw = yaw_up(*u)
        ncross = int(round(s["width"] / 2.0))
        row = riser_row(ncross)
        for (s0, zb) in P["risers"]:
            sc = s0 + 1.08
            for k, m in enumerate(row):
                off = -s["width"] / 2.0 + 1.0 + 2.0 * k
                x = lo[0] + u[0] * sc + perp[0] * off
                y = lo[1] + u[1] * sc + perp[1] * off
                put(m, x, y, zb, yaw, c=False, f="V2/Stairs")
        # landings: brick tiles at the landing level
        knots = P["knots"]
        for (sa, za), (sb, zb2) in zip(knots, knots[1:]):
            if abs(za - zb2) < 1e-6 and sb - sa > 0.3:
                nal = max(1, int(math.ceil((sb - sa) / 2.0 - 1e-6)))
                ln = (sb - sa) / nal
                for i in range(nal):
                    sm = sa + (i + 0.5) * ln
                    for k in range(ncross):
                        off = -s["width"] / 2.0 + 1.0 + 2.0 * k
                        put("V:Floor_Brick", lo[0] + u[0] * sm + perp[0] * off, lo[1] + u[1] * sm + perp[1] * off,
                            za + 0.01, yaw, s=(1.0, ln / 2.0, 1.0), f="V2/Stairs")
                if s["name"] == "grand_stair" and sb - sa > 2.5:
                    for sg in (-1, 1):
                        sm = (sa + sb) / 2
                        off = sg * (s["width"] / 2.0 - 0.7)
                        put("DV:Planter_Large", lo[0] + u[0] * sm + perp[0] * off, lo[1] + u[1] * sm + perp[1] * off,
                            za, yaw, f="V2/Stairs")
        # hidden ramp collision per flight (bots + ragdolls), top ~0.14 m over the step inner corners
        s_run = 0.0
        ri = 0
        for fl in P["flights"]:
            if fl == 0:
                continue
            s0, zb = P["risers"][ri]
            ri += fl
            run, rise = fl * 2.08, float(fl)
            hyp = math.hypot(run, rise)
            sm = s0 + run / 2
            zm = zb + rise / 2 + 0.14 - 0.1 / math.cos(math.atan2(rise, run))   # box centre: top face 0.14 over the tread line
            pitch = math.degrees(math.atan2(rise, run))
            put("E:Cube", lo[0] + u[0] * sm, lo[1] + u[1] * sm, zm, yaw_x(*u), pitch=pitch,
                s=(hyp + 0.3, s["width"], 0.2), f="V2/StairRamps", h=True)
        # cheek walls where the stair is cut into / raised over the terraces
        prof_s = [k[0] for k in knots]
        prof_z = [k[1] for k in knots]

        def pts_fn(sm, lo=lo, u=u):
            return (lo[0] + u[0] * sm, lo[1] + u[1] * sm), u

        side_walls(pts_fn, length, lambda sm: float(np.interp(sm, prof_s, prof_z)), s["width"] / 2.0,
                   "V2/StairWalls", lambda x, y: False)
        stats["stairs"] = stats.get("stairs", 0) + 1


def ramps():
    for r in L["ramps"]:
        if r["name"] in ("jetty_ramp", "fish_pier_ramp"):
            continue
        line = LineString(r["points"])
        S = line.length

        def pts_fn(sm, line=line, S=S):
            a = line.interpolate(max(0, sm - 0.5))
            b = line.interpolate(min(S, sm + 0.5))
            c = line.interpolate(sm)
            return (c.x, c.y), norm(b.x - a.x, b.y - a.y)

        zfn = (lambda sm, r=r, S=S: r["z0"] + (r["z1"] - r["z0"]) * sm / S)
        others = CORRIDORS.difference(line.buffer(r["width"] / 2 + 0.3, cap_style=2))
        side_walls(pts_fn, S, zfn, r["width"] / 2.0, "V2/RampWalls",
                   lambda x, y, o=others: o.distance(Point(x, y)) < 1.0 or LANES.difference(
                       line.buffer(r["width"] / 2 + 1.5)).distance(Point(x, y)) < 0.4)


# ================================================================================================ street joins
def street_joins():
    for j in L["street_joins"]:
        a, b = j["points"]
        t = norm(b[0] - a[0], b[1] - a[1])
        span = math.dist(a, b)
        if j["kind"] == "arch":
            ope = next(l for l in L["lanes"] if l["name"] == j["over"][0])
            z0 = float(ope["z"])
            od = norm(ope["points"][-1][0] - ope["points"][0][0], ope["points"][-1][1] - ope["points"][0][1])
            # arch pieces centred on the ope centreline, filling the span
            hit = LineString([a, b]).intersection(LineString(ope["points"]))
            cc = (hit.x, hit.y) if not hit.is_empty and hit.geom_type == "Point" else ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            s_c = (cc[0] - a[0]) * t[0] + (cc[1] - a[1]) * t[1]
            offs = [s_c]
            k = 1
            while s_c - 2 * k - 1 > -0.2 or s_c + 2 * k + 1 < span + 0.2:
                if s_c - 2 * k - 1 > -0.2:
                    offs.append(s_c - 2 * k)
                if s_c + 2 * k + 1 < span + 0.2:
                    offs.append(s_c + 2 * k)
                k += 1
            for sg in (-1.0, 1.0):
                n = (od[0] * sg, od[1] * sg)
                for i, so in enumerate(sorted(offs)):
                    x = a[0] + t[0] * so + n[0] * 1.0
                    y = a[1] + t[1] * so + n[1] * 1.0
                    put("V:Wall_Arch", x, y, z0, yaw_face(*n), f="V2/Arches", d="heart")
                    put("V:Wall_Plaster_Window_Thin_Round" if i % 2 == 0 else "V:Wall_Plaster_Straight", x, y, z0 + 3.0,
                        yaw_face(*n), f="V2/Arches", d="heart")
            lo_s, hi_s = min(offs) - 1.0, max(offs) + 1.0
            for so in np.arange(lo_s + 1.0, hi_s, 2.0):
                x, y = a[0] + t[0] * so, a[1] + t[1] * so
                put("V:Floor_WoodDark", x, y, z0 + 3.0, yaw_face(*od), f="V2/Arches")
                put("V:Floor_Brick", x, y, z0 + 2.98, yaw_face(*od), c=False, f="V2/Arches")   # soffit
            mid = (a[0] + t[0] * (lo_s + hi_s) / 2, a[1] + t[1] * (lo_s + hi_s) / 2)
            put("V:Roof_RoundTiles_4x4", mid[0], mid[1], z0 + 6.0, yaw_face(*od) + 90.0,
                s=((hi_s - lo_s) / 4.0, 2.4 / 4.0, 0.7), f="V2/Arches", d="heart")
            stats["arches"] = stats.get("arches", 0) + 1
        else:
            # garden wall 2.3 m with an open gate in the middle
            gz = [g1(*a), g1(*b)]
            nrm = (-t[1], t[0])
            mid = span / 2
            segs = [(0.0, mid - 0.6), (mid + 0.6, span)]
            for sa, sb in segs:
                if sb - sa < 0.2:
                    continue
                ln = sb - sa
                cx, cy = a[0] + t[0] * (sa + sb) / 2, a[1] + t[1] * (sa + sb) / 2
                put(WALL, cx, cy, g1(cx, cy) - 0.05, yaw_face(*nrm), s=(ln / 2.0, 1.0, 0.75), f="V2/GardenWalls")
            gx, gy = a[0] + t[0] * (mid - 0.55), a[1] + t[1] * (mid - 0.55)
            put("V:Door_1_Flat", gx, gy, g1(gx, gy), yaw_face(*nrm) + 70.0, f="V2/GardenWalls")


# ================================================================================================ harbour: jetty, pier, mole
PIER = ["PIR:pier_0", "PIR:pier_1", "PIR:pier_2"]


def pier_line(pts, deck=1.2, f="V2/Jetty"):
    line = LineString(pts)
    S = line.length
    n = max(1, int(math.ceil(S / 4.88)))
    st = S / n
    for k in range(n):
        c = line.interpolate((k + 0.5) * st)
        a, b = line.interpolate(k * st), line.interpolate((k + 1) * st)
        u = norm(b.x - a.x, b.y - a.y)
        put(PIER[k % 3], c.x, c.y, deck - 1.40, yaw_x(*u), s=(st / 4.88, 1.2, 1.0), f=f)
    # the pirate pier meshes give the navmesh nothing to stand on: one hidden walkable deck box per pier line
    a0, b0 = line.coords[0], line.coords[-1]
    u = norm(b0[0] - a0[0], b0[1] - a0[1])
    put("E:Cube", (a0[0] + b0[0]) / 2, (a0[1] + b0[1]) / 2, deck - 0.1, yaw_x(*u), s=(S + 0.4, 2.8, 0.2), f=f + "Deck", h=True)
    return line


def timber_ramp(r, f="V2/Jetty"):
    a, b = r["points"][0], r["points"][-1]
    u = norm(b[0] - a[0], b[1] - a[1])
    ln = math.dist(a, b)
    pitch = math.degrees(math.atan2(r["z1"] - r["z0"], ln))
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    put("V:Floor_WoodDark", mx, my, (r["z0"] + r["z1"]) / 2 + 0.02, yaw_x(*u), pitch=pitch,
        s=(ln / 2.0 + 0.1, r["width"] / 2.0, 1.0), f=f)
    # posts
    for sg in (-1, 1):
        px, py = b[0] - u[1] * sg * (r["width"] / 2 - 0.1), b[1] + u[0] * sg * (r["width"] / 2 - 0.1)
        put("V:Corner_Exterior_Wood", px, py, -2.0, 0.0, s=(0.6, 0.6, (r["z1"] + 2.0) / 3.0), f=f)


def harbour():
    lanes = {l["name"]: l for l in L["lanes"]}
    pier_line(lanes["long_jetty"]["points"])
    pier_line(lanes["jetty_head"]["points"])
    pier_line(lanes["fish_pier"]["points"], f="V2/FishPier")
    for r in L["ramps"]:
        if r["name"] in ("jetty_ramp", "fish_pier_ramp"):
            timber_ramp(r)
    # jetty lanterns at 1/3 and 2/3 (the builder adds the light)
    jl = LineString(lanes["long_jetty"]["points"])
    for frac in (1 / 3, 2 / 3, 1.0):
        p = jl.interpolate(frac, normalized=True)
        lamps.append([round(p.x * 100 - 150, 1), round(p.y * 100, 1), 120.0, 90.0, "jetty"])
    # mole: flag tiles on the walk, rock armour on the outer face
    mo = L["mole"]
    O = L["basin"]["center"]
    pieces, S = resample(mo["points"], 2.0)
    for k, ((cx, cy), (tx, ty), st, sc) in enumerate(pieces):
        n1 = (-ty, tx)
        out = n1 if math.dist((cx + n1[0], cy + n1[1]), O) > math.dist((cx, cy), O) else (-n1[0], -n1[1])
        zt = L["quay"]["top_z"] + (mo["top_z"] - L["quay"]["top_z"]) * float(T.smooth(0.0, 6.0, np.array(sc)))
        for off in (-0.875, 0.875):
            put("V:Floor_Brick", cx + out[0] * off, cy + out[1] * off, zt + 0.02, yaw_face(*out),
                s=(st / 2.0, 0.875, 1.0), f="V2/Mole")
        if k % 2 == 0:
            r = rng.uniform(3.0, 4.2)
            put(f"N:Rock_Medium_{1 + k % 3}", cx + out[0] * r, cy + out[1] * r, -0.8, float(rng.uniform(0, 360)),
                s=(1.3, 1.3, float(rng.uniform(0.8, 1.3))), f="V2/Mole")
        if k % 3 == 1:
            r = rng.uniform(2.6, 3.4)
            put(f"N:Rock_Medium_{1 + (k + 1) % 3}", cx - out[0] * r, cy - out[1] * r, -1.2, float(rng.uniform(0, 360)),
                s=(0.9, 0.9, 0.9), f="V2/Mole")
    # sea stacks off Lighthouse Point, bell buoy at the harbour mouth
    for (x, y, sc, yw) in ((60.0, 118.0, 1.0, 30.0), (88.0, 114.0, 0.8, 110.0), (112.0, 101.0, 1.2, 200.0), (47.0, 108.0, 0.6, 300.0)):
        put("DH:SeaStack_A" if yw < 150 else "DH:SeaStack_B", x, y, g1(x, y) - 0.5, yw, s=(sc, sc, sc), f="V2/Coast")
    put("DH:BellBuoy", 34.0, 104.0, -0.6, 0.0, f="V2/Harbour")
    put("DH:CargoHoist", *L["landmarks"]["crane"]["at"], 2.0, L["landmarks"]["crane"]["face_deg"] - 90.0, f="V2/Harbour")


# ================================================================================================ bridges, brook, spill
def bridges():
    for b in L["bridges"]:
        u = norm(*b["along"])
        perp = (-u[1], u[0])
        x, y = b["at"]
        if b["name"] == "old_stone_bridge" and TERRACE:
            # plan 11.4 StoneArchBridge_7m (deck top at the ends = pivot); the hidden flat deck box stays for nav
            put("DT:StoneArchBridge_7m", x, y, b["deck_z"], yaw_x(*u), s=(b["span"] / 7.0, b["width"] / 4.0, 1.0),
                f="V2/Bridges", d=None)
            put("E:Cube", x, y, b["deck_z"] - 0.25, yaw_x(*u), s=(b["span"] + 1.0, b["width"], 0.5), f="V2/Bridges", h=True)
        elif b["name"] == "old_stone_bridge":
            n = 4
            st = b["span"] / n
            for k in range(n):
                sc = -b["span"] / 2 + (k + 0.5) * st
                for off in (-1.0, 1.0):
                    put("V:Floor_Brick", x + u[0] * sc + perp[0] * off, y + u[1] * sc + perp[1] * off, b["deck_z"] + 0.02,
                        yaw_x(*u), s=(st / 2.0, 1.0, 1.0), f="V2/Bridges")
                for sg in (-1, 1):
                    px, py = x + u[0] * sc + perp[0] * sg * (b["width"] / 2 - 0.1), y + u[1] * sc + perp[1] * sg * (b["width"] / 2 - 0.1)
                    put(WALL, px, py, b["deck_z"] - 1.1, yaw_face(perp[0] * sg, perp[1] * sg), s=(st / 2.0, 1.0, 0.7),
                        f="V2/Bridges", d="brookside")
            put("E:Cube", x, y, b["deck_z"] - 0.25, yaw_x(*u), s=(b["span"] + 1.0, b["width"], 0.5), f="V2/Bridges", h=True)
        elif b["name"] == "ford_stones":
            for k in range(6):
                sc = -b["span"] / 2 + k * b["span"] / 5
                sx, sy = x + u[0] * sc + perp[0] * rng.uniform(-0.2, 0.2), y + u[1] * sc + perp[1] * rng.uniform(-0.2, 0.2)
                put("JP:SteppingStone", sx, sy, g1(sx, sy) + 0.25, float(rng.uniform(0, 360)), s=(1.8, 1.8, 2.2),
                    f="V2/Bridges")
        else:
            ends = [g1(x - u[0] * b["span"] / 2, y - u[1] * b["span"] / 2), g1(x + u[0] * b["span"] / 2, y + u[1] * b["span"] / 2)]
            put("DW:WoodBridge", x, y, sum(ends) / 2 - 0.05, yaw_x(*u), s=(b["span"] / 6.7 + 0.15, b["width"] / 2.0, 1.0),
                f="V2/Bridges")


def brook():
    pts = ST["points"]
    for (ax, ay, az), (bx, by, bz) in zip(pts, pts[1:]):
        seg = math.dist((ax, ay), (bx, by))
        n = max(1, int(math.ceil(seg / 6.0)))
        for k in range(n):
            t0, t1 = k / n, (k + 1) / n
            x0, y0, z0 = ax + (bx - ax) * t0, ay + (by - ay) * t0, az + (bz - az) * t0
            x1, y1, z1 = ax + (bx - ax) * t1, ay + (by - ay) * t1, az + (bz - az) * t1
            ln = math.dist((x0, y0), (x1, y1))
            u = norm(x1 - x0, y1 - y0)
            water.append({"p": [round((x0 + x1) * 50, 1), round((y0 + y1) * 50, 1), round(((z0 + z1) / 2 + 0.42) * 100, 1)],
                          "y": round(yaw_x(*u), 2), "pi": round(math.degrees(math.atan2(z1 - z0, ln)), 3),
                          "s": [round(ln + 0.6, 3), round(ST["width"] + 1.4, 3), 1.0], "f": "V2/Brook"})
    for p in ST.get("ponds", []):
        water.append({"p": [p["center"][0] * 100, p["center"][1] * 100, p["z"] * 100], "y": 0.0, "pi": 0.0,
                      "s": [p["radius"] * 2 + 2.0, p["radius"] * 2 + 2.0, 1.0], "f": "V2/Brook"})
    # koi spill: channel on the garden terrace + a curtain down the Sakura Wall
    sp = L["koi_spill"]["points"]
    sw = next(w for w in L["retaining_walls"] if w["name"].startswith("sakura"))
    cross = LineString(sw["points"]).intersection(LineString([p[:2] for p in sp]))
    if not cross.is_empty:
        cy = cross.y if cross.geom_type == "Point" else list(cross.geoms)[0].y
        water.append({"p": [4500.0, round((sp[0][1] + cy) * 50, 1), 808.0], "y": 90.0, "pi": 0.0,
                      "s": [round(cy - sp[0][1], 2), 0.8, 1.0], "f": "V2/KoiSpill"})
        water.append({"p": [4500.0, round((cy + 0.62) * 100, 1), 500.0], "y": 90.0, "pi": 0.0, "ro": 90.0,
                      "s": [0.8, 6.2, 1.0], "f": "V2/KoiSpill", "vertical": 1})
        put("N:Rock_Medium_2", 45.0, cy + 1.6, 1.6, 30.0, s=(0.7, 0.7, 0.5), f="V2/KoiSpill")


# ================================================================================================ fields
def fields():
    wheat_step, carrot_step = 0.9, 1.1
    for fdef in L["fields"]:
        poly = Polygon(fdef["polygon"])
        ring = list(poly.exterior.coords)
        if fdef["crop"].startswith("apple"):
            pass
        else:
            # dry-stone wall with a 3 m gate on the southern (+y) edge
            edges = list(zip(ring, ring[1:]))
            south = max(range(len(edges)), key=lambda i: (edges[i][0][1] + edges[i][1][1]) / 2)
            for i, (a, b) in enumerate(edges):
                pts = [a, b]
                pieces, S = resample(pts, 2.0)
                for (cx, cy), (tx, ty), st, sc in pieces:
                    if i == south and abs(sc - S / 2) < 1.6:
                        continue
                    put(WALL, cx, cy, g1(cx, cy) - 0.12, yaw_face(-ty, tx), s=(st / 2.0, 1.15, 0.3), f="V2/Fields",
                        d="orchard_upland")
        minx, miny, maxx, maxy = poly.bounds
        crop = fdef["crop"]
        if crop.startswith("apple"):
            step, mesh_list, bucket, sc_rng = 5.0, ["N:CommonTree_2", "N:CommonTree_4"], "forest", (0.7, 0.9)
        elif crop == "wheat":
            step, mesh_list, bucket, sc_rng = wheat_step, ["N:Grass_Wispy_Tall", "N:Grass_Common_Tall"], "crops", (1.1, 1.5)
        elif crop == "carrots":
            step, mesh_list, bucket, sc_rng = carrot_step, ["N:Plant_7", "N:Clover_2"], "crops", (0.9, 1.2)
        else:
            step, mesh_list, bucket, sc_rng = 1.2, ["N:Plant_1", "N:Plant_7_Big"], "crops", (0.8, 1.1)
        xs = np.arange(minx + step / 2, maxx, step)
        ys = np.arange(miny + step / 2, maxy, step)
        X, Y = np.meshgrid(xs, ys)
        X = X + rng.uniform(-0.15, 0.15, X.shape) * step
        inside = shapely.contains_xy(poly.buffer(-1.2), X, Y) & ~shapely.contains_xy(WALK.buffer(1.0), X, Y) & \
            ~shapely.contains_xy(B_RECTS.buffer(1.5), X, Y)
        Z = ground(X[inside], Y[inside])
        for x, y, z in zip(X[inside], Y[inside], Z):
            m = mesh_list[int(rng.integers(0, len(mesh_list)))]
            inst(bucket, m, x, y, z - 0.05, float(rng.uniform(0, 360)), float(rng.uniform(*sc_rng)))
        stats["crop_" + crop] = int(inside.sum())


# ================================================================================================ lamps
def lane_lamps():
    """One lamp per stair head/foot, ope end and lane junction (~40), never mid-ope."""
    cand = []
    for s in L["stairs"]:
        P = STAIR_P[s["name"]]
        for end, zz, sgn in ((P["lo"], P["zlo"], -1), (P["hi"], P["zhi"], 1)):
            perp = (P["u"][1], -P["u"][0])
            off = s["width"] / 2.0 + 0.9
            x, y = end[0] + perp[0] * off + P["u"][0] * sgn * 1.2, end[1] + perp[1] * off + P["u"][1] * sgn * 1.2
            cand.append((x, y, (-perp[0], -perp[1]), 3))
    for l in L["lanes"]:
        if l["kind"] in ("pier", "mole"):
            continue
        pts = l["points"]
        for end, nxt in ((pts[0], pts[1]), (pts[-1], pts[-2])):
            u = norm(nxt[0] - end[0], nxt[1] - end[1])
            perp = (-u[1], u[0])
            off = l["width"] / 2.0 + 0.8
            x, y = end[0] + perp[0] * off + u[0] * 1.5, end[1] + perp[1] * off + u[1] * 1.5
            pri = 2 if l["kind"] in ("ope", "alley") else 1
            cand.append((x, y, (-perp[0], -perp[1]), pri))
    cand.sort(key=lambda c: -c[3])
    chosen = []
    for x, y, arm, pri in cand:
        p = Point(x, y)
        if B_RECTS.distance(p) < 0.6 or WALK.contains(p) or not LAND.contains(p):
            continue
        if STREAM.distance(p) < 2.5 or any(math.dist((x, y), (c[0], c[1])) < 9.0 for c in chosen):
            continue
        if abs(x) > 125 or abs(y) > 120:
            continue
        chosen.append((x, y, arm))
    for x, y, arm in chosen:
        lamps.append([round(x * 100, 1), round(y * 100, 1), round(g1(x, y) * 100, 1), round(yaw_up(*arm), 1), "street"])
    stats["lamps"] = len(chosen)


# ================================================================================================ nature
def nature():
    trees = [f"N:CommonTree_{i}" for i in range(1, 6)]
    twisted = [f"N:TwistedTree_{i}" for i in range(1, 6)]
    pines = [f"N:Pine_{i}" for i in range(1, 6)]
    rocks = [f"N:Rock_Medium_{i}" for i in range(1, 4)]
    keep_out = unary_union([TOWN.buffer(2.0), WALK.buffer(4.0), B_RECTS.buffer(4.0), STREAM.buffer(ST["width"] / 2 + 2.5),
                            FIELDS.buffer(2.0), SIGHT, AREAS.buffer(2.0), Point(L["landmarks"]["windmill"]["at"]).buffer(14.0),
                            Point(L["landmarks"]["lighthouse"]["at"]).buffer(10.0)])
    # --- groves (inside the 260 m core): clustered by noise, denser to the N / NW / NE to frame the bowl
    step = 5.5
    xs = np.arange(-128.0, 128.0, step)
    ys = np.arange(-123.0, 100.0, step)
    X, Y = np.meshgrid(xs, ys)
    X = X + rng.uniform(-2.2, 2.2, X.shape)
    Y = Y + rng.uniform(-2.2, 2.2, Y.shape)
    clump = T.fbm(X * 1.0, Y * 1.0, 26.0, 3, seed=31)
    frame = np.clip((-Y - 40.0) / 60.0, 0, 1) * 0.35 + np.clip((np.abs(X) - 70.0) / 50.0, 0, 1) * 0.3
    ok = (clump + frame > 0.18) & shapely.contains_xy(LAND.buffer(-4.0), X, Y) & ~shapely.contains_xy(keep_out, X, Y)
    Z = ground(X, Y)
    ok &= Z > 2.2
    n_tr = 0
    for x, y, z, c in zip(X[ok], Y[ok], Z[ok], clump[ok]):
        near_brook = STREAM.distance(Point(x, y)) < 12.0
        m = (twisted if near_brook and rng.random() < 0.15 else trees)[int(rng.integers(0, 5))]
        if y < -95 and rng.random() < 0.5:
            m = pines[int(rng.integers(0, 5))]
        inst("forest", m, x, y, z - 0.15, float(rng.uniform(0, 360)), float(rng.uniform(0.9, 1.6)))
        n_tr += 1
    stats["grove_trees"] = n_tr
    # --- outer forest + mountain pines (beyond the core rectangle, inside 300 m)
    step = 8.0
    xs = np.arange(-300.0, 300.0, step)
    ys = np.arange(-300.0, 140.0, step)
    X, Y = np.meshgrid(xs, ys)
    X = X + rng.uniform(-3, 3, X.shape)
    Y = Y + rng.uniform(-3, 3, Y.shape)
    outer = ~((X > -128) & (X < 128) & (Y > -123) & (Y < 123))
    Z = ground(X, Y)
    dens = T.fbm(X, Y, 40.0, 3, seed=41)
    ok = outer & (Z > 3.0) & (Z < 62.0) & (dens > -0.15) & (np.hypot(X, Y) < 300) & shapely.contains_xy(LAND.buffer(-5), X, Y)
    n_p = 0
    for x, y, z in zip(X[ok], Y[ok], Z[ok]):
        m = pines[int(rng.integers(0, 5))] if (z > 14 or rng.random() < 0.55) else trees[int(rng.integers(0, 5))]
        inst("forest", m, x, y, z - 0.2, float(rng.uniform(0, 360)), float(rng.uniform(1.2, 2.6)))
        n_p += 1
    stats["outer_trees"] = n_p
    # --- rocks on steep ground / cliff tops, a few boulders in the meadows
    step = 7.0
    xs = np.arange(-250.0, 250.0, step)
    ys = np.arange(-250.0, 125.0, step)
    X, Y = np.meshgrid(xs, ys)
    X = X + rng.uniform(-3, 3, X.shape)
    Y = Y + rng.uniform(-3, 3, Y.shape)
    Z = ground(X, Y)
    gx = (ground(X + 1.0, Y) - ground(X - 1.0, Y)) / 2.0
    gy = (ground(X, Y + 1.0) - ground(X, Y - 1.0)) / 2.0
    slope = np.hypot(gx, gy)
    pick = rng.random(X.shape)
    ok = (((slope > 0.55) & (pick < 0.35)) | (pick < 0.012)) & (Z > 1.0) & ~shapely.contains_xy(keep_out, X, Y) & \
        shapely.contains_xy(LAND, X, Y)
    n_r = 0
    for x, y, z in zip(X[ok], Y[ok], Z[ok]):
        inst("forest", rocks[int(rng.integers(0, 3))], x, y, z - 0.4, float(rng.uniform(0, 360)), float(rng.uniform(0.8, 2.8)))
        n_r += 1
    stats["rocks"] = n_r
    # --- brook banks: ferns, bushes (no collision)
    line = STREAM
    s = 1.0
    while s < line.length:
        p = line.interpolate(s)
        a, b = line.interpolate(max(0, s - 0.5)), line.interpolate(min(line.length, s + 0.5))
        u = norm(b.x - a.x, b.y - a.y)
        for sg in (-1, 1):
            off = ST["width"] / 2 + rng.uniform(0.6, 2.6)
            x, y = p.x - u[1] * sg * off, p.y + u[0] * sg * off
            if WALK.buffer(0.8).contains(Point(x, y)) or B_RECTS.buffer(1).contains(Point(x, y)):
                continue
            m = ["N:Fern_1", "N:Bush_Common", "N:Plant_1_Big", "N:Grass_Common_Tall"][int(rng.integers(0, 4))]
            inst("crops", m, x, y, g1(x, y) - 0.05, float(rng.uniform(0, 360)), float(rng.uniform(0.8, 1.4)))
        s += 2.2
    # --- headland gorse / flowers
    head = TERR["headland"]
    X = rng.uniform(50, 130, 700)
    Y = rng.uniform(20, 110, 700)
    ok = shapely.contains_xy(head.buffer(-2), X, Y) & ~shapely.contains_xy(keep_out, X, Y)
    for x, y in zip(X[ok], Y[ok]):
        m = ["N:Bush_Common_Flowers", "N:Flower_3_Group", "N:Flower_4_Group", "N:Bush_Common"][int(rng.integers(0, 4))]
        inst("crops", m, x, y, g1(x, y) - 0.05, float(rng.uniform(0, 360)), float(rng.uniform(0.8, 1.5)))


def meadow():
    """AKGGrassField clumps on the soft land (no grass on walkways, flat town stone, sand, rock or water)."""
    step = 1.2
    xs = np.arange(-126.0, 126.0, step)
    ys = np.arange(-121.0, 72.0, step)
    X, Y = np.meshgrid(xs, ys)
    X = X + rng.uniform(-0.45, 0.45, X.shape)
    Y = Y + rng.uniform(-0.45, 0.45, Y.shape)
    Z = ground(X, Y)
    gx = (ground(X + 0.6, Y) - ground(X - 0.6, Y)) / 1.2
    gy = (ground(X, Y + 0.6) - ground(X, Y - 0.6)) / 1.2
    slope = np.hypot(gx, gy)
    stone = unary_union([TERR["quay"], SQUARES, WALK.buffer(0.35), B_RECTS.buffer(0.4), AREAS,
                         STREAM.buffer(ST["width"] / 2 + 0.7), FIELDS.buffer(0.5),
                         unary_union([Point(p["center"]).buffer(p["radius"] + 1.5) for p in ST.get("ponds", [])]),
                         Point(L["landmarks"]["koi_pond"]["at"]).buffer(4.5),
                         Polygon(T.rect_poly(L["landmarks"]["graveyard"], 0.0))])
    ok = (Z > 1.6) & (Z < 26.0) & (slope < 0.85) & shapely.contains_xy(LAND.buffer(-1.0), X, Y) & \
        ~shapely.contains_xy(stone, X, Y)
    town = shapely.contains_xy(TOWN, X, Y)
    lane_d = shapely.distance(LANES, shapely.points(X[ok], Y[ok]))
    patch = np.sin(X[ok] * 0.11 + 1.3) * np.cos(Y[ok] * 0.09 - 0.4) + 0.35 * np.sin(X[ok] * 0.41 + Y[ok] * 0.33)
    r = rng.random(lane_d.shape)
    layer = np.where(lane_d < 2.5, 0, np.where(patch > 0.45, 2, np.where((patch < -0.65) & (r < 0.5), 3,
                                                                          np.where(r < 0.7, 1, 0))))
    layer = np.where(town[ok], 0, layer)          # town gardens / crown lawns: short grass only
    out = [[], [], [], []]
    for x, y, z, lyr in zip(X[ok], Y[ok], Z[ok], layer):
        sc = float(rng.uniform(0.8, 1.3))
        out[int(lyr)].append([round(x * 100, 1), round(y * 100, 1), round((z - 0.03) * 100, 1),
                              round(float(rng.uniform(0, 360)), 1), round(sc, 3), round(sc * float(rng.uniform(0.85, 1.15)), 3)])
    stats["grass"] = [len(o) for o in out]
    return out


# ================================================================================================ starts
BENCH_ANGLES, BENCH_R = (35.0, 145.0, 215.0, 325.0), 4.3     # fountain benches (kg_build_village_v2.fountain_square)


def starts():
    sp = L["spawn"]
    cx, cy = sp["center"]
    benches = [Point(cx + BENCH_R * math.cos(math.radians(a)), cy + BENCH_R * math.sin(math.radians(a))).buffer(1.6)
               for a in BENCH_ANGLES]
    obstacles = unary_union([Polygon(T.rect_poly(L["landmarks"][k], 0.6)) for k in ("market_stalls", "gallows", "dead_tree",
                                                                                     "notice_board")] + [B_RECTS] + benches)
    out = []
    for k in range(sp["count"]):
        a = math.radians(k * 360.0 / sp["count"] + 9.0)
        for r in (sp["radius"], 7.0, 8.0, 6.5, 8.5, 6.0, 9.0, 5.5):
            x, y = cx + r * math.cos(a), cy + r * math.sin(a)
            if obstacles.distance(Point(x, y)) > 0.6 and SQUARES.buffer(-0.5).contains(Point(x, y)):
                break
        out.append([round(x * 100, 1), round(y * 100, 1), round((g1(x, y) + 1.0) * 100, 1),
                    round(math.degrees(math.atan2(cy - y, cx - x)), 1)])
    return out


# ================================================================================================ main
def main():
    walls()
    stairs()
    ramps()
    street_joins()
    harbour()
    bridges()
    brook()
    fields()
    lane_lamps()
    nature()
    grass = meadow()
    st = starts()
    data = {"_doc": __doc__.splitlines()[0], "layout": "morrowmere_layout_v2.json", "items": items, "hism": hism,
            "grass": grass, "lamps": lamps, "water": water, "starts": st, "stats": stats}
    with open(OUT_PATH, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    print(f"KG_V2_PREP items {len(items)}, forest {sum(len(v) for v in hism['forest'].values())}, crops "
          f"{sum(len(v) for v in hism['crops'].values())}, grass {stats['grass']}, lamps {len(lamps)}, water {len(water)}, "
          f"starts {len(st)} -> {OUT_PATH}")
    print("KG_V2_PREP stats", json.dumps(stats))


if __name__ == "__main__":
    main()
