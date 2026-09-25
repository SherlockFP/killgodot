"""Shared helpers for the Morrowmere v2 zone dressing (Tools/Unreal/dressing/v2/dress_<zone>.py), imported as C.

v2 context of kg_dress_common (which stays the v1 module, untouched): same API names (C.place, C.instanced, C.free,
C.claim, C.seat, C.breakable, C.loot_chest, C.mover, C.light ...), but everything comes from the v2 data:
  * Tools/Level/morrowmere_layout_v2.json   (zones, buildings + doors, lanes, stairs, ramps, walls, landmarks)
  * Art/Packed/KG_Terrain_v2_heights.json    (0.5 m core grid + 2.5 m outer grid, same ground() as the builder)
  * Art/Packed/KG_V2_Placements.json         (builder pieces: forest trees, starts, lamps)
  * Saved/KG_V2_Existing.json                (footprints of every builder actor, dumped by the runner before dressing)

Two backends, one code path:
  * "ue"  (inside the editor or a commandlet): spawns actors. Static meshes are spawned as StaticMeshActor FROM THE
    CLASS (EditorActorSubsystem.spawn_actor_from_object is a no-op in commandlets), like kg_build_village_v2.
  * "dry" (plain system Python, no `unreal`): records the same placements only. Used by
    `python Tools/Unreal/kg_dress_v2.py --dry` to plot and clearance-check a plan in seconds.
Every spawn is appended to RECORD (zone, kind, mesh, x, y, z, footprint radius, collide) -> the dress report, which
Tools/Level/verify_v2_build.py checks (doors, stairs, ramps, chores, starts, 2 m corridors, budgets).

Placement rules (C.free): in the zone, off walkways (or only in the outer band of wide streets with allow_lane=True, so
>= 2 m stays clear), outside buildings, clear of reserved areas (door aprons + door->street corridors, stair and ramp
corridors extended past both ends, 0.6 m strips along retaining walls and the quay edge, arches, chores, starts,
fountain, gallows steps, well), off the brook, not overlapping earlier claims.

Frame: UE cm, X east, +Y towards the sea. Layout values are metres (x100).
NOTE: unreal.Rotator positional order is (roll, pitch, yaw) and unreal.Color is BGRA: always use keywords.
"""
import json
import math
import os
import random
import zlib

try:
    import unreal
    UE = True
except ImportError:          # dry run (system Python)
    unreal = None
    UE = False

ROOT = "D:/Kill Godot"
LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
LAYOUT_FILE = f"{ROOT}/Tools/Level/morrowmere_layout_v2.json"
HEIGHTS = f"{ROOT}/Art/Packed/KG_Terrain_v2_heights.json"
PLACEMENTS = f"{ROOT}/Art/Packed/KG_V2_Placements.json"
EXISTING = f"{ROOT}/Saved/KG_V2_Existing.json"
SIZES = f"{ROOT}/Tools/Unreal/dressing/v2/mesh_sizes.json"

V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
N = "/Game/KillGodot/Env/KG_Nature/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
PIR = "/Game/KillGodot/Env/PirateC3/KG_PirateProps_Clean2/StaticMeshes/SM_KG_Pirate_"
JP = "/Game/KillGodot/Env/JapanC3/KG_JapanProps_Clean2/StaticMeshes/SM_KG_"
WP = "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_"
DV = "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_"
DH = "/Game/KillGodot/Env/Dress/KG_DressHarbour_Clean/StaticMeshes/SM_KG_"
DW = "/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_"
DX = "/Game/KillGodot/Env/Dress/KG_DressV2/"                       # v2 helper assets (glow cards, beam, water)
K = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_"
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_"
E = "/Engine/BasicShapes/"
AUDIO = "/Game/KillGodot/Audio/"
M = 100.0
FLOOR_H = 300.0
WARM = (255, 170, 95)

LAYOUT = json.load(open(LAYOUT_FILE, encoding="utf-8"))
PL = json.load(open(PLACEMENTS))
_hm = json.load(open(HEIGHTS))
try:
    _SIZES = json.load(open(SIZES))
except (OSError, ValueError):
    _SIZES = {}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) if UE else None
rng = random.Random(7)

# Set by begin() for the zone being dressed.
ZONE = ""
FOLDER = "Dress"
stats = {}
RECORD = []             # every spawn of the run (all zones): dicts, see _rec()
BUDGET = {"props": 900, "lights": 10}


def log(msg):
    if UE:
        unreal.log(f"KG_DRESS_V2 {msg}")
    print(f"KG_DRESS_V2 {msg}")


# ============================================================================================ geometry
def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def seg_proj(px, py, ax, ay, bx, by):
    """(t 0..1, distance, closest x, closest y)."""
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
    qx, qy = ax + t * dx, ay + t * dy
    return t, math.hypot(px - qx, py - qy), qx, qy


def pip(poly, x, y):
    """Even-odd point in polygon (poly: [(x, y)] same units as x, y)."""
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / ((yj - yi) or 1e-12) + xi:
            inside = not inside
        j = i
    return inside


def poly_dist(poly, x, y):
    return min(seg_dist(x, y, *poly[i], *poly[(i + 1) % len(poly)]) for i in range(len(poly)))


def centroid(poly):
    a = cx = cy = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        c = x0 * y1 - x1 * y0
        a += c
        cx += (x0 + x1) * c
        cy += (y0 + y1) * c
    if abs(a) < 1e-9:
        return sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly)
    return cx / (3 * a), cy / (3 * a)


def cm(pts):
    return [(p[0] * M, p[1] * M) for p in pts]


def polyline_len(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))


def resample(pts, step, start=0.0, end_trim=0.0):
    """[(x, y, dx, dy, s)] every `step` along a polyline (unit tangent, arc length)."""
    out = []
    total = polyline_len(pts)
    s_target = start
    acc = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        if seg < 1e-6:
            continue
        ux, uy = (bx - ax) / seg, (by - ay) / seg
        while s_target <= acc + seg and s_target <= total - end_trim:
            t = s_target - acc
            out.append((ax + ux * t, ay + uy * t, ux, uy, s_target))
            s_target += step
        acc += seg
    return out


def at_len(pts, s):
    """(x, y, ux, uy) at arc length s along a polyline."""
    acc = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        if seg > 1e-6 and acc + seg >= s:
            ux, uy = (bx - ax) / seg, (by - ay) / seg
            t = s - acc
            return ax + ux * t, ay + uy * t, ux, uy
        acc += seg
    (ax, ay), (bx, by) = pts[-2], pts[-1]
    seg = max(1e-6, math.hypot(bx - ax, by - ay))
    return bx, by, (bx - ax) / seg, (by - ay) / seg


def yaw_to(ax, ay, bx, by):
    return math.degrees(math.atan2(by - ay, bx - ax))


def fwd(yaw, d):
    return math.cos(math.radians(yaw)) * d, math.sin(math.radians(yaw)) * d


# ============================================================================================ terrain
_core = _hm["core"]
_cx0, _cy0, _cst, _cnx, _cny, _ch = _core["x0"], _core["y0"], _core["step"], _core["nx"], _core["ny"], _core["heights"]


def ground(x, y):
    """Terrain height (cm) at UE (x, y) cm: the 0.5 m core grid, else the 2.5 m outer grid (== the builder)."""
    xm, ym = x / 100.0, y / 100.0
    fi, fj = (xm - _cx0) / _cst, (ym - _cy0) / _cst
    if 0 <= fi < _cnx - 1 and 0 <= fj < _cny - 1:
        i, j = int(fi), int(fj)
        tx, ty = fi - i, fj - j
        a, b = _ch[j * _cnx + i], _ch[j * _cnx + i + 1]
        c, d = _ch[(j + 1) * _cnx + i], _ch[(j + 1) * _cnx + i + 1]
        return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)
    size, step, n, h = _hm["size"], _hm["step"], _hm["n"], _hm["heights"]
    bx, by = xm + size / 2, -ym + size / 2
    fi, fj = max(0.0, min(n - 1e-3, bx / step)), max(0.0, min(n - 1e-3, by / step))
    i, j = int(fi), int(fj)
    tx, ty = fi - i, fj - j
    a, b = h[j * (n + 1) + i], h[j * (n + 1) + i + 1]
    c, d = h[(j + 1) * (n + 1) + i], h[(j + 1) * (n + 1) + i + 1]
    return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)


def slope(x, y, r=100.0):
    """Max height difference (cm) across a 2r square: skip steep spots for flat-bottomed props."""
    hs = [ground(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r)]
    return max(hs) - min(hs)


def ground_min(x, y, r):
    """Lowest terrain under a footprint of radius r: sink a prop to it so no edge floats."""
    return min(ground(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r))


def ground_max(x, y, r):
    return max(ground(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r))


# ============================================================================================ zones + terraces
_ZONES = []
for _z in LAYOUT["zones"]:
    _name = "japan" if _z["name"] == "japan_island" else _z["name"]
    for _poly in _z["polygons"]:
        _p = cm(_poly)
        _ZONES.append((_name, _p, (min(q[0] for q in _p), min(q[1] for q in _p), max(q[0] for q in _p), max(q[1] for q in _p))))
ZONE_ORDER = ["square", "harbour", "streets", "church", "japan", "countryside", "coast", "wilds", "beckside"]


def zone_of(x, y):
    """Ordered point-in-polygon over layout_v2 "zones": the first zone that contains the point, else "wilds".
    japan_island counts as "japan"."""
    for name, poly, (x0, y0, x1, y1) in _ZONES:
        if x0 <= x <= x1 and y0 <= y <= y1 and pip(poly, x, y):
            return name
    return "wilds"


def in_zone(x, y, zone=None):
    return zone_of(x, y) == (zone or ZONE)


def zone_polys(zone):
    return [p for n, p, _ in _ZONES if n == zone]


_TERR = [(t, cm(t["polygon"])) for t in LAYOUT["terraces"]]


def terrace_at(x, y):
    """The layout terrace dict containing (x, y) cm, or None."""
    for t, poly in _TERR:
        if pip(poly, x, y):
            return t
    return None


def terrace_z(x, y):
    """Flat terrace height (cm) at (x, y), else the terrain."""
    t = terrace_at(x, y)
    if t and t.get("z") is not None:
        return float(t["z"]) * M
    return ground(x, y)


TOWN_TERRACES = ("quay", "heart", "upper", "garden", "crown")


# ============================================================================================ buildings
def lm(name):
    return LAYOUT["landmarks"][name]


class Frame:
    """Oriented local frame (building footprint): local -Y is the FRONT (door side), +X to the right.
    w/d full sizes (cm); z pad height (cm)."""

    def __init__(self, x, y, yaw, w=0.0, d=0.0, name="", z=None):
        self.x, self.y, self.yaw, self.w, self.d, self.name = x, y, yaw, w, d, name
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        self.z = ground(x, y) if z is None else z
        self.door = None
        self.kind = ""
        self.district = ""
        self.storeys = 1
        self.block = None
        self.spec = {}

    def world(self, lx, ly):
        return self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c

    def local(self, wx, wy):
        dx, dy = wx - self.x, wy - self.y
        return dx * self.c + dy * self.s, -dx * self.s + dy * self.c

    def inside(self, wx, wy, pad=0.0):
        lx, ly = self.local(wx, wy)
        return abs(lx) <= self.w * 0.5 + pad and abs(ly) <= self.d * 0.5 + pad

    def dist(self, wx, wy):
        """Distance (cm) from the footprint rectangle (0 inside)."""
        lx, ly = self.local(wx, wy)
        dx = max(0.0, abs(lx) - self.w * 0.5)
        dy = max(0.0, abs(ly) - self.d * 0.5)
        return math.hypot(dx, dy)

    @property
    def door_t(self):
        """Local x of the door (the builder's door_local)."""
        cw = int(round(self.w / 200.0))
        return -cw * 100.0 + 100.0 + (cw // 2) * 200.0 if self.w >= 400.0 else 0.0

    def front_yaw(self):
        """World yaw of the outward front normal (local -Y)."""
        return self.yaw - 90.0

    # --- side helpers (street-facing walls): side in front/back/left/right, t along the side, out from the face
    SIDES = {"front": ((0, -1), (1, 0)), "back": ((0, 1), (-1, 0)), "left": ((-1, 0), (0, -1)), "right": ((1, 0), (0, 1))}

    def half(self, side):
        return self.w * 0.5 if side in ("front", "back") else self.d * 0.5

    def face_off(self, side):
        return self.d * 0.5 if side in ("front", "back") else self.w * 0.5

    def at(self, side, t, out):
        (nx, ny), (tx, ty) = Frame.SIDES[side]
        e = self.face_off(side) + out
        return self.world(nx * e + tx * t, ny * e + ty * t)

    def nyaw(self, side):
        (nx, ny), _ = Frame.SIDES[side]
        return self.yaw + math.degrees(math.atan2(ny, nx))

    def tyaw(self, side):
        _, (tx, ty) = Frame.SIDES[side]
        return self.yaw + math.degrees(math.atan2(ty, tx))

    def cells(self, side):
        return int(round((self.w if side in ("front", "back") else self.d) / 200.0))


def _door_world(f, out=80.0):
    return f.world(f.door_t, -f.d * 0.5 - out)


_BUILDINGS = None


def buildings():
    """Every built footprint as a Frame: houses (kind home), infill shells (infill), civic buildings (civic),
    towers (tower), open halls (open), the mausoleum/windmill/pavilion (special). .door = world point 0.8 m in front
    of the door (None for open halls and specials), .district, .storeys, .block, .spec (the JSON)."""
    global _BUILDINGS
    if _BUILDINGS is not None:
        return _BUILDINGS
    out = []
    specs = [("home", h) for h in LAYOUT["houses"]] + [("infill", h) for h in LAYOUT["infill"]]
    for key, b in LAYOUT["landmarks"].items():
        if b.get("is_prop") or "size" not in b or b.get("kind") not in ("civic", "tower", "special"):
            continue
        kind = b["kind"]
        if b.get("open"):
            kind = "open"
        specs.append((kind, dict(b, _key=key)))
    for kind, b in specs:
        (ax, ay), (w, d) = b["at"], b["size"]
        yaw = b.get("yaw", b.get("face_deg", 0.0) + 90.0)
        f = Frame(ax * M, ay * M, yaw, w * M, d * M, b.get("_key") or b.get("id"), z=b.get("z", 0.0) * M)
        f.kind, f.district, f.storeys, f.block, f.spec = kind, b.get("district", ""), int(b.get("storeys", 1)), b.get("block"), b
        f.id = b.get("id", f.name)
        f.door = _door_world(f) if kind in ("home", "infill", "civic", "tower") else None
        out.append(f)
    _BUILDINGS = out
    return out


def building(name):
    """Frame by landmark key (town_hall, inn, ...) or id (H01, I07, C05)."""
    for f in buildings():
        if f.name == name or f.id == name:
            return f
    return None


def doors():
    return [(f, f.door) for f in buildings() if f.door]


# ============================================================================================ walkways
_WALK_KINDS_BAND = {"quay": 1.6, "main": 0.9, "lane": 0.45, "pier": 0.5, "mole": 0.5}


def _walkways():
    """[(name, kind, width cm, pts cm)] lanes + stairs + ramps (stairs extended 1.5 m past both ends)."""
    out = []
    for l in LAYOUT["lanes"]:
        out.append((l["name"], l["kind"], l["width"] * M, cm(l["points"])))
    for s in LAYOUT["stairs"]:
        (ax, ay), (bx, by) = cm([s["from"], s["to"]])
        L = math.hypot(bx - ax, by - ay)
        ux, uy = (bx - ax) / L, (by - ay) / L
        out.append((s["name"], "stair", s["width"] * M,
                    [(ax - ux * 150.0, ay - uy * 150.0), (bx + ux * 150.0, by + uy * 150.0)]))
    for r in LAYOUT["ramps"]:
        out.append((r["name"], "ramp", r["width"] * M, cm(r["points"])))
    return out


WALKWAYS = _walkways()


class _Grid:
    """Spatial hash of items with a reach (cm) -> candidate items near a point."""

    def __init__(self, cell=500.0):
        self.cell, self.g = cell, {}

    def add(self, item, x0, y0, x1, y1):
        c = self.cell
        for i in range(int(math.floor(x0 / c)), int(math.floor(x1 / c)) + 1):
            for j in range(int(math.floor(y0 / c)), int(math.floor(y1 / c)) + 1):
                self.g.setdefault((i, j), []).append(item)

    def near(self, x, y):
        return self.g.get((int(math.floor(x / self.cell)), int(math.floor(y / self.cell))), ())


REACH = 900.0            # max distance a query cares about (prop radius + gaps)
_walk_grid = _Grid()
_SEGS = []               # (name, kind, width, ax, ay, bx, by)
for _name, _kind, _w, _pts in WALKWAYS:
    for (_ax, _ay), (_bx, _by) in zip(_pts, _pts[1:]):
        _SEGS.append((_name, _kind, _w, _ax, _ay, _bx, _by))
        _walk_grid.add(len(_SEGS) - 1, min(_ax, _bx) - _w / 2 - REACH, min(_ay, _by) - _w / 2 - REACH,
                       max(_ax, _bx) + _w / 2 + REACH, max(_ay, _by) + _w / 2 + REACH)


def lanes():
    """[(name, width_cm, [(x, y) cm ...])] incl. stairs and ramps (v1 API)."""
    return [(n, w, pts) for n, _k, w, pts in WALKWAYS]


def lane(name):
    for n, k, w, pts in WALKWAYS:
        if n == name:
            return pts
    raise KeyError(name)


def lane_width(name):
    for n, k, w, pts in WALKWAYS:
        if n == name:
            return w
    raise KeyError(name)


def lane_kind(name):
    for n, k, w, pts in WALKWAYS:
        if n == name:
            return k
    raise KeyError(name)


def lane_distance(x, y):
    """Distance (cm) from the nearest walkway EDGE (lanes, stairs, ramps); negative = on it."""
    best = 1e9
    for i in _walk_grid.near(x, y):
        _n, _k, w, ax, ay, bx, by = _SEGS[i]
        best = min(best, seg_dist(x, y, ax, ay, bx, by) - w * 0.5)
    return best


def nearest_walkway(x, y):
    """(name, kind, width, distance to centre, closest point x, y, tangent ux, uy)."""
    best = None
    for i in _walk_grid.near(x, y):
        n, k, w, ax, ay, bx, by = _SEGS[i]
        t, d, qx, qy = seg_proj(x, y, ax, ay, bx, by)
        if best is None or d - w * 0.5 < best[3] - best[2] * 0.5:
            L = max(1e-6, math.hypot(bx - ax, by - ay))
            best = (n, k, w, d, qx, qy, (bx - ax) / L, (by - ay) / L)
    return best


def walk_blocked(x, y, r, allow_lane=False, lane_gap=40.0):
    """True when a prop of radius r at (x, y) eats into a walkway. allow_lane: wide streets keep their central band
    (quay 4 m, main streets / lanes >= 2 m) clear and let props stand in the outer edge; opes, paths, stairs, ramps
    are never allowed."""
    for i in _walk_grid.near(x, y):
        _n, kind, w, ax, ay, bx, by = _SEGS[i]
        d = seg_dist(x, y, ax, ay, bx, by)
        if allow_lane and kind in _WALK_KINDS_BAND:
            band = max(100.0, w * 0.5 - _WALK_KINDS_BAND[kind] * M)
            if d < band + r:
                return True
        elif d < w * 0.5 + r + lane_gap:
            return True
    return False


def lane_samples(name, step=300.0, offset=0.0, start=0.0, end_trim=0.0):
    """Points along a walkway: [(x, y, ux, uy)] with an optional sideways offset (cm, +left of travel)."""
    out = []
    for x, y, ux, uy, _s in resample(lane(name), step, start, end_trim):
        out.append((x - uy * offset, y + ux * offset, ux, uy))
    return out


def lane_edge(name, side=1, extra=0.0, step=250.0, start=0.0, end_trim=0.0):
    """Points just outside one edge of a lane (side +1 = left of travel), `extra` cm beyond the edge."""
    return lane_samples(name, step, side * (lane_width(name) * 0.5 + extra), start, end_trim)


def squares():
    return {s["name"]: s for s in LAYOUT["squares"]}


def square_poly(name):
    return cm(squares()[name]["polygon"])


def in_square(x, y, name=None):
    for s in LAYOUT["squares"]:
        if (name is None or s["name"] == name) and pip(cm(s["polygon"]), x, y):
            return True
    return False


def stairs():
    return {s["name"]: s for s in LAYOUT["stairs"]}


def ramps():
    return {r["name"]: r for r in LAYOUT["ramps"]}


def walls():
    """[(name, top_z, bottom_z, parapet, pts cm)] retaining walls + the quay edge."""
    out = [(w["name"], w["top_z"], w["bottom_z"], bool(w.get("parapet")), cm(w["points"])) for w in LAYOUT["retaining_walls"]]
    out.append(("quay_edge", LAYOUT["quay"]["top_z"], LAYOUT["quay"]["bottom_z"], False, cm(LAYOUT["quay"]["edge"])))
    return out


_wall_grid = _Grid()
_WSEGS = []
for _wn, _tz, _bz, _par, _pts in walls():
    for (_ax, _ay), (_bx, _by) in zip(_pts, _pts[1:]):
        _WSEGS.append((_wn, _ax, _ay, _bx, _by))
        _wall_grid.add(len(_WSEGS) - 1, min(_ax, _bx) - REACH, min(_ay, _by) - REACH, max(_ax, _bx) + REACH,
                       max(_ay, _by) + REACH)


def wall_distance(x, y):
    best = 1e9
    for i in _wall_grid.near(x, y):
        _n, ax, ay, bx, by = _WSEGS[i]
        best = min(best, seg_dist(x, y, ax, ay, bx, by))
    return best


_STREAM = cm(LAYOUT["stream"]["points"])
_STREAM_W = LAYOUT["stream"]["width"] * M


def stream_distance(x, y):
    """Distance (cm) from the brook centre line."""
    return min(seg_dist(x, y, *a, *b) for a, b in zip(_STREAM, _STREAM[1:]))


def stream_pts():
    return _STREAM


O_BASIN = (LAYOUT["basin"]["center"][0] * M, LAYOUT["basin"]["center"][1] * M)


def polar(r, theta_deg, origin=None):
    """Point (cm) at radius r (cm) and angle theta (deg) around the basin centre O (the amphitheatre frame)."""
    ox, oy = origin or O_BASIN
    return ox + r * math.cos(math.radians(theta_deg)), oy + r * math.sin(math.radians(theta_deg))


def polar_of(x, y, origin=None):
    """(r cm, theta deg) of a point around the basin centre O."""
    ox, oy = origin or O_BASIN
    return math.hypot(x - ox, y - oy), math.degrees(math.atan2(y - oy, x - ox))


# ============================================================================================ chores + reserved
TOWER_TASKS = {"RingBell": "bell_tower", "FuelLighthouse": "lighthouse", "WindClock": "clock_tower"}


def task_spots():
    """World (x, y) cm of every chore station on the ground (tower chores: the tower door)."""
    out = {}
    for t in LAYOUT["tasks"]:
        if t["id"] in TOWER_TASKS:
            f = building(TOWER_TASKS[t["id"]])
            out[t["id"]] = f.door if f and f.door else (t["at"][0] * M, t["at"][1] * M)
        else:
            out[t["id"]] = (t["at"][0] * M, t["at"][1] * M)
    return out


def starts():
    return [(s[0], s[1]) for s in PL["starts"]]


_res_grid = _Grid()
_RES = []            # capsules (ax, ay, bx, by, r, why)


def _add_res(ax, ay, bx, by, r, why):
    _RES.append((ax, ay, bx, by, r, why))
    _res_grid.add(len(_RES) - 1, min(ax, bx) - r - REACH, min(ay, by) - r - REACH, max(ax, bx) + r + REACH,
                  max(ay, by) + r + REACH)


def _build_reserved():
    _RES.clear()
    _res_grid.g.clear()
    for f in buildings():
        if not f.door:
            continue
        x0, y0 = f.world(f.door_t, -f.d * 0.5)
        x1, y1 = f.world(f.door_t, -f.d * 0.5 - 220.0)
        _add_res(x0, y0, x1, y1, 85.0, f"door {f.id}")
        nw = nearest_walkway(x1, y1)
        if nw and nw[3] - nw[2] * 0.5 < 600.0:
            _add_res(x1, y1, nw[4], nw[5], 90.0, f"door path {f.id}")
    for tid, (x, y) in task_spots().items():
        _add_res(x, y, x, y, 200.0, f"chore {tid}")
    for x, y in starts():
        _add_res(x, y, x, y, 120.0, "start")
    fs = lm("fountain")
    _add_res(fs["at"][0] * M, fs["at"][1] * M, fs["at"][0] * M, fs["at"][1] * M, 170.0 + 200.0, "fountain")
    g = lm("gallows")
    gf = Frame(g["at"][0] * M, g["at"][1] * M, g["yaw"])
    _add_res(*gf.world(0.0, 0.0), *gf.world(0.0, 420.0), 260.0, "gallows")
    w = lm("well")
    _add_res(w["at"][0] * M, w["at"][1] * M, w["at"][0] * M, w["at"][1] * M, 230.0, "well")
    for j in LAYOUT["street_joins"]:
        if j["kind"] == "arch":
            (ax, ay), (bx, by) = cm(j["points"])
            _add_res(ax, ay, bx, by, 160.0, "arch")
        else:
            (ax, ay), (bx, by) = cm(j["points"])
            mx, my = (ax + bx) / 2, (ay + by) / 2
            _add_res(mx, my, mx, my, 150.0, "garden gate")
    for name, _k, _w, pts in WALKWAYS:
        pass
    for n, ax, ay, bx, by in _WSEGS:
        _add_res(ax, ay, bx, by, 35.0 if n == "quay_edge" else 60.0, f"wall {n}")
    for b in LAYOUT["bridges"]:
        x, y = b["at"][0] * M, b["at"][1] * M
        ux, uy = b["along"]
        L = b["span"] * M * 0.5 + 250.0
        _add_res(x - ux * L, y - uy * L, x + ux * L, y + uy * L, b["width"] * M * 0.5 + 60.0, f"bridge {b['name']}")


def reserved_hit(x, y, r):
    """The reason string when a circle (x, y, r) touches a reserved capsule, else None."""
    for i in _res_grid.near(x, y):
        ax, ay, bx, by, rr, why = _RES[i]
        if seg_dist(x, y, ax, ay, bx, by) < rr + r:
            return why
    return None


def reserved():
    """[(ax, ay, bx, by, r, why)] capsules that must stay walkable/clear."""
    if not _RES:
        _build_reserved()
    return list(_RES)


# ============================================================================================ occupancy
_CELL = 400.0
_grid = {}


def _cells(x, y, r):
    for i in range(int(math.floor((x - r) / _CELL)), int(math.floor((x + r) / _CELL)) + 1):
        for j in range(int(math.floor((y - r) / _CELL)), int(math.floor((y + r) / _CELL)) + 1):
            yield i, j


def claim(x, y, r):
    """Mark a circle as taken (spatial hash)."""
    for key in _cells(x, y, r):
        _grid.setdefault(key, []).append((x, y, r))


def overlaps(x, y, r):
    for key in _cells(x, y, r):
        for (cx, cy, cr) in _grid.get(key, ()):
            if math.hypot(x - cx, y - cy) < r + cr:
                return True
    return False


_bgrid = _Grid()


def _build_bgrid():
    _bgrid.g.clear()
    for f in buildings():
        rr = math.hypot(f.w, f.d) * 0.5 + REACH
        _bgrid.add(f, f.x - rr, f.y - rr, f.x + rr, f.y + rr)


def building_hit(x, y, pad=60.0):
    """The Frame when (x, y) is inside a building footprint (+pad), else None."""
    if not _bgrid.g:
        _build_bgrid()
    for f in _bgrid.near(x, y):
        if f.inside(x, y, pad):
            return f
    return None


def why_blocked(x, y, r, lane_gap=40.0, zone=True, allow_lane=False, allow_reserved=False, water_ok=False,
                claims=True):
    """Why a prop of radius r cannot stand at (x, y) (None = free)."""
    if zone and not in_zone(x, y):
        return "zone"
    if walk_blocked(x, y, r, allow_lane, lane_gap):
        return "walkway"
    if building_hit(x, y, r + 30.0):
        return "building"
    if not allow_reserved:
        if not _RES:
            _build_reserved()
        why = reserved_hit(x, y, r)
        if why:
            return why
    if not water_ok and stream_distance(x, y) < _STREAM_W * 0.5 + r + 60.0:
        return "brook"
    if claims and overlaps(x, y, r):
        return "claimed"
    return None


def free(x, y, r, lane_gap=40.0, zone=True, allow_lane=False, allow_reserved=False, water_ok=False, claims=True):
    """A spot is free when it is in this zone, off walkways (outer band only with allow_lane), outside buildings,
    clear of reserved areas and the brook, and not overlapping anything claimed."""
    return why_blocked(x, y, r, lane_gap, zone, allow_lane, allow_reserved, water_ok, claims) is None


def free_line(x, y, yaw, length, r, **kw):
    """An elongated footprint (bench, stall, rack): `length` along world yaw, `r` = half its depth. Checks circles of
    radius r every ~r along the long axis."""
    n = max(1, int(math.ceil((length * 0.5 - r) / max(20.0, r))))
    ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    half = max(0.0, length * 0.5 - r)
    for k in range(-n, n + 1):
        t = half * k / n
        if not free(x + ux * t, y + uy * t, r, **kw):
            return False
    return True


def claim_line(x, y, yaw, length, r):
    n = max(1, int(math.ceil((length * 0.5 - r) / max(20.0, r))))
    ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    half = max(0.0, length * 0.5 - r)
    for k in range(-n, n + 1):
        t = half * k / n
        claim(x + ux * t, y + uy * t, r)


def find(x, y, r, reach=250.0, step=50.0, **kw):
    """Nearest free spot to (x, y) within `reach` (spiral search), else None (logged in stats['blocked'])."""
    if free(x, y, r, **kw):
        return x, y
    d = step
    while d <= reach:
        n = max(6, int(2 * math.pi * d / step))
        for k in range(n):
            a = 2 * math.pi * k / n
            xx, yy = x + d * math.cos(a), y + d * math.sin(a)
            if free(xx, yy, r, **kw):
                return xx, yy
        d += step
    stats.setdefault("blocked", []).append(f"{x:.0f},{y:.0f},r{r:.0f}:{why_blocked(x, y, r, **kw)}")
    return None


def in_garden(x, y, margin=40.0):
    """Walled back garden / yard: town terrace ground left after streets, squares, buildings and stairs."""
    t = terrace_at(x, y)
    if not t or t["name"] not in ("quay", "heart", "upper", "garden"):
        return False
    if in_square(x, y):
        return False
    if lane_distance(x, y) < margin + 60.0:
        return False
    if building_hit(x, y, margin):
        return False
    return wall_distance(x, y) > 60.0


def flat_ok(x, y, r, tol=25.0):
    """Ground under a footprint is flat to `tol` cm (no prop hanging over a terrace step)."""
    return slope(x, y, r) <= tol


# ============================================================================================ anchors
def _anchor_table():
    A = {}

    def put(name, xm, ym, z=None, **extra):
        A[name] = dict(x=xm * M, y=ym * M, z=(z * M if z is not None else None), **extra)

    for k, b in LAYOUT["landmarks"].items():
        put(k, b["at"][0], b["at"][1], b.get("z"), yaw=b.get("yaw"), face_deg=b.get("face_deg"), size=b.get("size"))
    for f in buildings():
        if f.door:
            A[f"door:{f.name}"] = dict(x=f.door[0], y=f.door[1], z=f.z, yaw=f.front_yaw())
    for s in LAYOUT["stairs"]:
        hi, lo = (s["from"], s["to"]) if s["z0"] > s["z1"] else (s["to"], s["from"])
        put(f"{s['name']}:head", hi[0], hi[1], max(s["z0"], s["z1"]))
        put(f"{s['name']}:foot", lo[0], lo[1], min(s["z0"], s["z1"]))
    for r in LAYOUT["ramps"]:
        put(f"{r['name']}:start", r["points"][0][0], r["points"][0][1], r["z0"])
        put(f"{r['name']}:end", r["points"][-1][0], r["points"][-1][1], r["z1"])
    for l in LAYOUT["lanes"]:
        put(f"{l['name']}:start", *l["points"][0])
        put(f"{l['name']}:end", *l["points"][-1])
    for b in LAYOUT["bridges"]:
        put(f"bridge:{b['name']}", b["at"][0], b["at"][1], b.get("deck_z"), along=b["along"], span=b["span"])
    for sq in LAYOUT["squares"]:
        cx, cy = centroid(sq["polygon"])
        put(f"square:{sq['name']}", cx, cy, sq.get("z"))
    for t in LAYOUT["tasks"]:
        put(f"task:{t['id']}", t["at"][0], t["at"][1])
    O = LAYOUT["basin"]["center"]
    put("basin", O[0], O[1], LAYOUT["basin"]["floor_z"], radius=LAYOUT["basin"]["radius"])
    put("mole_light", *LAYOUT["mole"]["light"], LAYOUT["mole"]["top_z"])
    for p in LAYOUT["stream"]["ponds"]:
        put(f"pond:{p['name']}", p["center"][0], p["center"][1], p["z"], radius=p["radius"])
    for f_ in LAYOUT["fields"]:
        cx, cy = centroid(f_["polygon"])
        put(f"field:{f_['name']}", cx, cy)
    # Named lines (cm polylines) for the modules
    A["line:quay_edge"] = dict(pts=cm(LAYOUT["quay"]["edge"]))
    A["line:mole"] = dict(pts=cm(LAYOUT["mole"]["points"]))
    A["line:stream"] = dict(pts=_STREAM)
    A["line:balustrade"] = dict(pts=cm(next(q for q in LAYOUT["squares"] if q["name"] == "fountain_square")["polygon"][:2]))
    for n, _k, _w, pts in WALKWAYS:
        A[f"line:{n}"] = dict(pts=pts)
    for n, _t, _b, _p, pts in walls():
        A[f"line:{n}"] = dict(pts=pts)
    return A


ANCHORS = _anchor_table()


def anchor(name):
    """A named layout point: dict(x, y, z (cm or None), ...) - landmarks by key, 'door:<building>',
    '<stair>:head|foot', '<ramp>:start|end', '<lane>:start|end', 'bridge:<name>', 'square:<name>', 'task:<id>',
    'basin', 'mole_light', 'pond:<name>', 'field:<name>', and polylines 'line:<lane|wall|quay_edge|mole|stream|
    balustrade>' (dict(pts=[...]))."""
    return ANCHORS[name]


def xy(name):
    a = ANCHORS[name]
    return a["x"], a["y"]


def anchors(zone=None):
    """{name: anchor} of the named points inside `zone` (default: the zone being dressed)."""
    zone = zone or ZONE
    return {k: v for k, v in ANCHORS.items() if "x" in v and zone_of(v["x"], v["y"]) == zone}


# ============================================================================================ meshes
_PREFIX = {"V": V, "N": N, "P": P, "PIR": PIR, "JP": JP, "WP": WP, "DV": DV, "DH": DH, "DW": DW, "DX": DX, "K": K,
           "IP": IP, "E": E}


def mp(path):
    """Resolve 'V:Wall_Arch' style shorthands to package paths."""
    if ":" in path and not path.startswith("/"):
        k, name = path.split(":", 1)
        return _PREFIX[k] + name
    return path


def size(path):
    """(sx, sy, sz, minx, miny, minz) cm of a mesh (catalogue + probe), or a 100 cm cube guess."""
    s = _SIZES.get(mp(path))
    return tuple(s) if s else (100.0, 100.0, 100.0, -50.0, -50.0, 0.0)


def foot_r(path, scale=1.0):
    s = size(path)
    k = max(scale) if isinstance(scale, (tuple, list)) else scale
    sx = s[0] * (scale[0] if isinstance(scale, (tuple, list)) else k)
    sy = s[1] * (scale[1] if isinstance(scale, (tuple, list)) else k)
    return 0.5 * max(sx, sy)


_mesh_cache = {}


def mesh(path):
    path = mp(path)
    if not UE:
        return path
    if path not in _mesh_cache:
        m = unreal.load_asset(path) if (path.startswith("/Engine/") or unreal.EditorAssetLibrary.does_asset_exist(path)) else None
        _mesh_cache[path] = m
        if not m:
            unreal.log_warning(f"KG_DRESS_V2 missing mesh {path}")
            stats.setdefault("missing", set()).add(path)
    return _mesh_cache[path]


def _rec(kind, path, x, y, z, r, collide, yaw=None, scale=None, **extra):
    d = {"zone": ZONE, "kind": kind, "m": path.split("/")[-1] if path else "", "x": round(x, 1), "y": round(y, 1),
         "z": round(z, 1) if z is not None else None, "r": round(r, 1), "c": bool(collide)}
    name = d["m"]
    if any(w in name for w in ("Tree", "Pine", "Sakura", "Maple", "Bamboo", "Bonsai")):
        d["r"] = round(45.0 * max(_s3(scale if scale is not None else 1.0)[:2]), 1)      # the trunk, not the canopy
        yaw = None
    if yaw is not None and path:
        s = size(path)
        s3 = _s3(scale if scale is not None else 1.0)
        # oriented footprint: centre offset of the mesh bounds + half extents (cm), for exact clearance checks
        cx, cy = (s[3] + s[0] * 0.5) * s3[0], (s[4] + s[1] * 0.5) * s3[1]
        c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        d["fx"], d["fy"] = round(x + cx * c - cy * sn, 1), round(y + cx * sn + cy * c, 1)
        d["hx"], d["hy"], d["yaw"] = round(s[0] * 0.5 * abs(s3[0]), 1), round(s[1] * 0.5 * abs(s3[1]), 1), round(yaw, 1)
    d.update(extra)
    RECORD.append(d)


class _Dummy:
    """Dry-run stand-in for spawned actors (accepts any call)."""

    def __getattr__(self, name):
        return lambda *a, **k: None


def _folder(sub):
    return f"{FOLDER}/{sub}" if sub else FOLDER


def _spawn_mesh(m, x, y, z, yaw, pitch, roll, s3, collide, movable=False):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                      unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if not a:
        return None
    comp = a.static_mesh_component
    if movable:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(m)
    if s3 != (1.0, 1.0, 1.0):
        a.set_actor_scale3d(unreal.Vector(*s3))
    if not collide:
        a.set_actor_enable_collision(False)
        comp.set_editor_property("can_ever_affect_navigation", False)
    return a


def _s3(scale):
    return (float(scale), float(scale), float(scale)) if isinstance(scale, (int, float)) else tuple(float(v) for v in scale)


def place(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, sub="", collide=True, cull=0.0,
          shadow=True, claim_r=0.0, sink=0.0, label=None, movable=False, material=None, overhead=False):
    """Spawn one static mesh actor. z=None -> on the terrain (minus `sink`). scale: float or (sx, sy, sz).
    cull (cm) > 0 hides it beyond that distance. claim_r > 0 reserves the footprint for later free() calls.
    material: path of a material to put on slot 0. Returns the actor (a dummy in dry runs, None if missing)."""
    path = mp(path)
    zz = (ground(x, y) - sink) if z is None else z
    s3 = _s3(scale)
    _rec("actor", path, x, y, zz, 0.0 if overhead else foot_r(path, s3), collide and not overhead, o=overhead,
         yaw=yaw if abs(pitch) < 30 and abs(roll) < 30 else None, scale=s3)
    stats["props"] = stats.get("props", 0) + 1
    if claim_r > 0.0:
        claim(x, y, claim_r)
    if not UE:
        return _Dummy()
    m = mesh(path)
    if not m:
        return None
    a = _spawn_mesh(m, x, y, zz, yaw, pitch, roll, s3, collide, movable)
    if not a:
        return None
    a.set_folder_path(_folder(sub))
    if label:
        a.set_actor_label(label)
    comp = a.static_mesh_component
    if cull > 0.0:
        comp.set_editor_property("ld_max_draw_distance", cull)
    if not shadow:
        comp.set_editor_property("cast_shadow", False)
    if material:
        mat = unreal.load_asset(mp(material))
        if mat:
            comp.set_material(0, mat)
    return a


_fields = {}
ISM_MESHES = set()          # every mesh drawn through a HISM this run (their base materials need the ISM usage flag)


def instanced(path, transforms, collide=False, cull=12000.0, sub="Instanced"):
    """Many copies of one mesh: [(x, y, z, yaw, scale)] or [(x, y, z, yaw, scale, pitch, roll)] (scale float or
    3-tuple). One AKGFoliageField per zone and collision flag (a HISM per mesh; the first call per mesh fixes its
    cull distance). Returns the count."""
    path = mp(path)
    transforms = [tuple(t) + (0.0, 0.0) if len(t) == 5 else tuple(t) for t in transforms]
    if not transforms:
        return 0
    for t in transforms:
        _rec("inst", path, t[0], t[1], t[2], foot_r(path, _s3(t[4])), collide, o=not collide,
             yaw=t[3] if (abs(t[5]) < 30 and abs(t[6]) < 30) else None, scale=t[4])
    stats["instances"] = stats.get("instances", 0) + len(transforms)
    if not UE:
        return len(transforms)
    m = mesh(path)
    if not m:
        return 0
    ISM_MESHES.add(path)
    key = (ZONE, bool(collide))
    field = _fields.get(key)
    if field is None:
        cls = unreal.load_class(None, "/Script/KillGodot.KGFoliageField")
        field = actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0))
        field.set_actor_label(f"KG_DressV2_{ZONE}_{'Solid' if collide else 'Clutter'}")
        field.set_folder_path(_folder(sub))
        _fields[key] = field
    tr = []
    for (x, y, z, yaw, sc, pitch, roll) in transforms:
        tr.append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                                   unreal.Vector(*_s3(sc))))
    field.add_instances(m, tr, collide, cull)
    return len(tr)


class Batch:
    """Collects repeated meshes and flushes them through C.instanced (one HISM per mesh, per collision flag)."""

    def __init__(self):
        self.b = {}

    def add(self, path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=7000.0):
        e = self.b.setdefault((mp(path), bool(collide)), {"tr": [], "cull": cull})
        e["tr"].append((x, y, z, yaw, scale, pitch, roll))

    def flush(self):
        for (path, collide), e in self.b.items():
            instanced(path, e["tr"], collide=collide, cull=e["cull"])
        self.b.clear()


def light(x, y, z, intensity=8.0, radius=700.0, color=WARM, sub="Lights", force=False):
    """Movable, shadowless point light (candelas). Budget: BUDGET['lights'] per zone (returns None past it)."""
    if not force and stats.get("lights", 0) >= BUDGET["lights"]:
        stats["lights_skipped"] = stats.get("lights_skipped", 0) + 1
        return None
    _rec("light", "", x, y, z, 0.0, False)
    stats["lights"] = stats.get("lights", 0) + 1
    if not UE:
        return _Dummy()
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    lc.set_editor_property("cast_shadows", False)
    a.set_folder_path(_folder(sub))
    return a


def ambient(sound, x, y, z, volume=1.0, sub="Audio"):
    """Looping positional ambience (A_*_Loop under /Game/KillGodot/Audio)."""
    stats["sounds"] = stats.get("sounds", 0) + 1
    _rec("sound", sound, x, y, z, 0.0, False)
    if not UE:
        return _Dummy()
    a = actors.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(x, y, z))
    comp = a.get_component_by_class(unreal.AudioComponent)
    comp.set_sound(unreal.load_asset(AUDIO + sound))
    comp.set_editor_property("volume_multiplier", volume)
    comp.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO + "SA_KG_Ambience"))
    a.set_folder_path(_folder(sub))
    return a


def spawn_class(class_path, x, y, z, yaw=0.0, sub="Gameplay"):
    """Gameplay actors ('/Script/KillGodot.KGBreakable' ...)."""
    stats["gameplay"] = stats.get("gameplay", 0) + 1
    if not UE:
        return _Dummy()
    cls = unreal.load_class(None, class_path)
    if not cls:
        unreal.log_warning(f"KG_DRESS_V2 missing class {class_path}")
        return None
    a = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.set_folder_path(_folder(sub))
    return a


def breakable(path, x, y, z=None, yaw=0.0, sub="Breakables", scale=1.0):
    """Punchable physics crate/barrel/pot (AKGBreakable; drops FKGLoot items when smashed)."""
    path = mp(path)
    zz = (ground(x, y) + 5.0) if z is None else z
    _rec("breakable", path, x, y, zz, foot_r(path, scale), True, yaw=yaw, scale=scale)
    stats["breakables"] = stats.get("breakables", 0) + 1
    claim(x, y, foot_r(path, scale))
    b = spawn_class("/Script/KillGodot.KGBreakable", x, y, zz, yaw, sub)
    if b and UE:
        b.set_mesh(mesh(path))
        if scale != 1.0:
            b.set_actor_scale3d(unreal.Vector(*_s3(scale)))
        low = path.lower()
        table = "Barrel" if "barrel" in low else "Pot" if ("pot" in low or "vase" in low) else "Crate"
        for k, v in (("loot_table", table), ("loot_seed", zlib.crc32(f"{x:.0f},{y:.0f}".encode()) & 0x7FFFFFFF)):
            try:
                b.set_editor_property(k, v)
            except Exception:
                pass
    return b


SEAT_H = {P + "Bench": 49.6, P + "Stool": 58.2, P + "Chair_1": 49.8, K + "Chair": 44.2, K + "Stool": 67.9}


def seat(path, x, y, face_yaw, z=None, seat_height=None, stand=75.0, sub="Seats", mesh_yaw=-90.0):
    """A bench/stool people can sit on (AKGSeat, E to sit). The sitter looks along world yaw `face_yaw`.
    Furniture meshes face their local +Y, so the mesh turns `mesh_yaw` (-90) inside the actor (kg_interiors rule)."""
    path = mp(path)
    zz = ground(x, y) if z is None else z
    _rec("seat", path, x, y, zz, foot_r(path), True, yaw=face_yaw + mesh_yaw)
    stats["seats"] = stats.get("seats", 0) + 1
    claim(x, y, foot_r(path) * 0.8)
    s = spawn_class("/Script/KillGodot.KGSeat", x, y, zz, face_yaw, sub)
    if s and UE:
        try:
            s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=mesh_yaw))
            s.set_editor_property("seat_height", seat_height if seat_height is not None else SEAT_H.get(path, 50.0))
            s.set_editor_property("stand_distance", stand)
            s.set_seat_mesh(mesh(path))
        except Exception as e:
            unreal.log_warning(f"KG_DRESS_V2 seat: {e}")
    return s


def loot_chest(x, y, z=None, yaw=0.0, table="Chest", name="Chest", path=None, sub="Loot"):
    """Lootable storage chest at a point of interest (AKGStorageChest, E to open; loot rolled once per match)."""
    zz = ground(x, y) if z is None else z
    mpath = mp(path) if path else "/Game/KillGodot/Items/Storage/SM_KG_Chest_Wood"
    _rec("chest", mpath, x, y, zz, 60.0, True, name=name)
    stats["chests"] = stats.get("chests", 0) + 1
    claim(x, y, 65.0)
    c = spawn_class("/Script/KillGodot.KGStorageChest", x, y, zz, yaw, sub)
    if c and UE:
        if path:
            c.set_chest_mesh(mesh(mpath))
        for k, v in (("starting_loot_table", table), ("loot_seed", zlib.crc32(f"{x:.0f},{y:.0f}".encode()) & 0x7FFFFFFF),
                     ("display_name", name)):
            try:
                c.set_editor_property(k, v)
            except Exception as e:
                unreal.log_warning(f"KG_DRESS_V2 chest {k}: {e}")
    return c


def mover(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, spin=(0.0, 0.0, 0.0), sway=0.0, sway_hz=0.35,
          bob=0.0, sub="Motion", collide=False, material=None, overhead=None):
    """Cosmetic motion (AKGSpinner): spin=(pitch, yaw, roll) deg/s, sway = pendulum degrees around local X (hanging
    signs, bunting, lanterns), bob = cm (buoys). No collision unless collide=True. It only animates while rendered."""
    path = mp(path)
    zz = ground(x, y) if z is None else z
    if overhead is None:
        overhead = not collide
    _rec("mover", path, x, y, zz, 0.0 if overhead else foot_r(path, _s3(scale)), collide, o=overhead)
    stats["movers"] = stats.get("movers", 0) + 1
    if not UE:
        return _Dummy()
    cls = unreal.load_class(None, "/Script/KillGodot.KGSpinner")
    a = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, zz), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    a.set_mesh(mesh(path))
    s3 = _s3(scale)
    if s3 != (1.0, 1.0, 1.0):
        a.set_actor_scale3d(unreal.Vector(*s3))
    a.set_editor_property("spin_rate", unreal.Rotator(roll=spin[2], pitch=spin[0], yaw=spin[1]))
    a.set_editor_property("sway_degrees", sway)
    a.set_editor_property("sway_hz", sway_hz)
    a.set_editor_property("bob_cm", bob)
    if not collide:
        a.set_actor_enable_collision(False)
    if material:
        mat = unreal.load_asset(mp(material))
        comp = a.get_component_by_class(unreal.StaticMeshComponent)
        if mat and comp:
            comp.set_material(0, mat)
    a.set_folder_path(_folder(sub))
    return a


def attach(child, parent):
    """Attach a spawned actor to a mover (it then follows the mover's spin/sway/bob)."""
    if not UE or not child or not parent:
        return
    comp = child.get_component_by_class(unreal.StaticMeshComponent)
    if comp:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    child.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


def scatter(paths, count, pick, r_prop, tries_per=30, scale=(0.9, 1.2), collide=False, cull=6000.0, sink=2.0,
            max_slope=9999.0, batch=None, **free_kw):
    """Random scatter: pick() -> (x, y) candidates, accepted when free(x, y, r_prop). Instanced unless collide."""
    b = batch or Batch()
    placed = 0
    for _ in range(count * tries_per):
        if placed >= count:
            break
        x, y = pick()
        if not free(x, y, r_prop, **free_kw) or slope(x, y, r_prop) > max_slope:
            continue
        p = rng.choice(paths)
        claim(x, y, r_prop)
        b.add(p, x, y, ground(x, y) - sink, rng.uniform(0, 360), rng.uniform(*scale), collide=collide, cull=cull)
        placed += 1
    if batch is None:
        b.flush()
    return placed


# ============================================================================================ level housekeeping
def clear_folder(prefix):
    """Destroy every actor whose outliner folder is `prefix` or below it. Returns the count."""
    if not UE:
        return 0
    doomed = []
    for a in actors.get_all_level_actors():
        f = str(a.get_folder_path())
        if f == prefix or f.startswith(prefix + "/"):
            doomed.append(a)
    if doomed:
        actors.destroy_actors(doomed)
    return len(doomed)


_MEADOW = []


def clear_grass(x, y, r):
    """Remove meadow clumps (KG_Meadow HISM instances) inside a circle (under yards, beds, floors)."""
    if not UE:
        return 0
    if not _MEADOW:
        for a in actors.get_all_level_actors():
            if a.get_actor_label() == "KG_Meadow":
                _MEADOW.extend(a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
    removed = 0
    for comp in _MEADOW:
        hit = comp.get_instances_overlapping_sphere(unreal.Vector(x, y, ground(x, y)), r, True)
        if hit:
            comp.remove_instances(list(hit))
            removed += len(hit)
    stats["grass_cleared"] = stats.get("grass_cleared", 0) + removed
    return removed


SKIP_EXISTING = ("V2/Terrain", "V2/Nature", "V2/Houses", "V2/Shells", "V2/Civic", "V2/Towers", "V2/Lights", "V2/Audio",
                 "V2/Sea", "V2/Sky", "Lighting", "Dress", "V2/Walls", "V2/Parapets", "V2/StairWalls", "V2/RampWalls",
                 "V2/Stairs", "V2/StairRamps", "V2/Brook", "V2/KoiSpill", "V2/Island", "Gameplay/Starts",
                 "Gameplay/Tasks", "Gameplay/Dev", "V2/Doors", "V2/Ladders", "Village/Interior")


def dump_existing():
    """Footprints (x, y, r cm, folder) of the builder's props in the open level -> Saved/KG_V2_Existing.json, so
    dressing never lands on a lamp, bench, stall, torch or hint prop. UE only; dry runs read the file."""
    out = []
    for a in actors.get_all_level_actors():
        try:
            f = str(a.get_folder_path())
            if not f or f.startswith(SKIP_EXISTING):
                continue
            cls = a.get_class().get_name()
            if cls in ("PointLight", "AmbientSound", "KGFishSchool", "PlayerStart", "KGFoliageField", "KGGrassField",
                       "TargetPoint", "CameraActor", "NavMeshBoundsVolume", "KGMapInfo"):
                continue
            o, e = a.get_actor_bounds(False)
            r = max(e.x, e.y)
            if r < 8.0 or r > 700.0:
                continue
            comp = a.get_component_by_class(unreal.StaticMeshComponent)
            sm = comp.static_mesh if comp else None
            out.append([round(o.x, 1), round(o.y, 1), round(min(r, 400.0) * 0.85, 1), f,
                        sm.get_name() if sm else cls])
        except Exception:
            continue
    json.dump(out, open(EXISTING, "w"))
    return len(out)


def existing():
    """[(x, y, r, folder, mesh)] of the builder's props (Saved/KG_V2_Existing.json)."""
    try:
        return [tuple(e) + (("",) if len(e) == 4 else ()) for e in json.load(open(EXISTING))]
    except (OSError, ValueError):
        return []


def remove_builder(folder, mesh_part):
    """Take over builder props this dressing replaces (e.g. the square's plain benches become KGSeat benches):
    destroys the matching actors of the level (UE) and drops their claims. Returns their [(x, y, r)]."""
    gone = [(x, y, r) for x, y, r, f, m in existing() if f == folder and mesh_part in m]
    for x, y, r in gone:
        for key in _cells(x, y, r):
            if (x, y, r) in _grid.get(key, []):
                _grid[key].remove((x, y, r))
    if UE:
        doomed = []
        for a in actors.get_all_level_actors():
            if str(a.get_folder_path()) != folder:
                continue
            comp = a.get_component_by_class(unreal.StaticMeshComponent)
            sm = comp.static_mesh if comp else None
            if (sm and mesh_part in sm.get_name()) or mesh_part in a.get_class().get_name():
                doomed.append(a)
        if doomed:
            actors.destroy_actors(doomed)
        stats["replaced_builder"] = stats.get("replaced_builder", 0) + len(doomed)
    return gone


def claim_existing():
    """Claim the builder's props (Saved/KG_V2_Existing.json) and the forest trees from the placements file."""
    n = 0
    ex = existing()
    if not ex:
        log("no Saved/KG_V2_Existing.json yet (dry run before the first UE run): builder props not claimed")
    for x, y, r, _f, _m in ex:
        claim(x, y, r)
        n += 1
    for path, rows in PL["hism"]["forest"].items():
        rr = 130.0 if "Pine" in path else 110.0
        for r in rows:
            claim(r[0], r[1], rr * max(0.6, r[4]))
            n += 1
    return n


def begin(zone):
    """Runner hook: start dressing `zone` (clears Dress/<Zone>, resets the occupancy)."""
    global ZONE, FOLDER, stats
    ZONE, FOLDER = zone, f"Dress/{zone.capitalize()}"
    stats = {}
    _fields.pop((zone, True), None)
    _fields.pop((zone, False), None)
    _grid.clear()
    rng.seed(zlib.crc32(zone.encode()))
    BUDGET["lights"] = 14 if zone == "wilds" else 10
    removed = clear_folder(FOLDER)
    if not _RES:
        _build_reserved()
    stats["claimed_existing"] = claim_existing()
    return removed


def finish():
    """Runner hook: budget warnings for the zone just dressed."""
    if stats.get("props", 0) > BUDGET["props"]:
        stats.setdefault("warn", []).append(f"props {stats['props']} > {BUDGET['props']}")
    if stats.get("lights", 0) > BUDGET["lights"]:
        stats.setdefault("warn", []).append(f"lights {stats['lights']} > {BUDGET['lights']}")
    return stats
