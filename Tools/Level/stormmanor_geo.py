"""Storm Manor (map 2) build geometry, shared by the builder, the chore generator, the minimap and the verifier.

Pure Python (no unreal, no shapely), so it runs inside UE's embedded Python and in system Python alike.

  import stormmanor_geo as G
  L = G.layout()                 # Tools/Level/stormmanor_layout.json with the SPRINT-018 build fixes applied
  G.Z0, G.floor_z("F1")          # cm

Build fixes on top of the SPRINT-017 design (the design doc keeps its numbers; these only make it buildable with the
2 m kit and the 1 m-per-2 m stair slope):
  * Z0 = +5 m: the whole manor stands 5 m over the world origin because FKGWaves::SeaLevel is 0 (everything under
    z 0 swims). The cellar floor is then 2 m over the sea, the boathouse slip reaches down to it.
  * Stairs: every straight stair runs 6 m for its 3 m rise on whole 2 m cells (floor holes are cell-exact). The two
    "spiral" stairs become straight runs along a wall (library -> master bedroom, master bedroom -> storm tower);
    the tower ladder moves to the tower's east wall.
  * Doors that a stair well would swallow move along the same wall (blue room, studio). Secret-passage ends and
    chore anchors that fell into a stair well move next to it.
"""
import json
import math
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LAYOUT_FILE = os.path.join(ROOT, "Tools", "Level", "stormmanor_layout.json")

Z0 = 500.0                       # cm: ground floor over the world origin (sea level 0)
STOREY = 300.0
FLOORS = {"C": -1, "F0": 0, "F1": 1, "F2": 2, "F3": 3}
ORDER = ["C", "F0", "F1", "F2", "F3"]
CELL = 2.0                       # m

# stair id -> (bottom, top) in metres (width from the layout)
STAIR_FIX = {
    "st_grand_w": ((-9, -2), (-9, -8)),
    "st_grand_e": ((9, -2), (9, -8)),
    "st_cellar": ((-30, -5), (-24, -5)),
    "st_crypt": ((12, -11), (18, -11)),
    "st_library_spiral": ((33, -12), (33, -18)),
    "st_clock": ((6, -19), (0, -19)),
    "st_tower_spiral": ((29, -16), (29, -22)),
    "st_tower_ladder": ((33, -18), (33, -18)),
    "st_sea": ((0, 40), (0, 34)),
}
DOOR_FIX = {"d_corridor_w__blue_room": (-32, -12), "d_corridor_w__studio": (-20, -8)}
SECRET_FIX = {("S3", "a"): (-9.4, -1.2), ("S6", "b"): (23.0, -13.0), ("S4", "a"): (-48.0, 20.8)}
ANCHOR_FIX = {"letter_blue": (-32, -13.4), "pantry_china": (-32.6, 1.0), "tower_lamp": (30.0, -20.0)}

# Rooms the builder closes with walls although the design calls them grounds (glass house, boat shed).
ENCLOSED_GROUNDS = {"greenhouse", "boathouse"}
OPEN_CIRCULATION = {"cliff_path"}


def floor_z(fid):
    """Floor top (cm) of a level."""
    return Z0 + FLOORS[fid] * STOREY


def layout(path=LAYOUT_FILE):
    L = json.load(open(path, encoding="utf-8"))
    for s in L["stairs"]:
        if s["id"] in STAIR_FIX:
            b, t = STAIR_FIX[s["id"]]
            s["bottom"], s["top"] = list(b), list(t)
    for d in L["doors"]:
        if d["id"] in DOOR_FIX:
            d["at"] = list(DOOR_FIX[d["id"]])
    for s in L["secrets"]:
        for end in ("a", "b"):
            if (s["id"], end) in SECRET_FIX:
                s["at_" + end] = list(SECRET_FIX[(s["id"], end)])
    for a in L["anchors"]:
        if a["id"] in ANCHOR_FIX:
            a["at"] = list(ANCHOR_FIX[a["id"]])
    return L


def rooms(L):
    return {r["id"]: r for r in L["rooms"]}


def enclosed(r):
    """Walls, floor and ceiling of its own (rooms, corridors, the glass house and the boat shed)."""
    if r is None:
        return False
    if r["id"] in ENCLOSED_GROUNDS:
        return True
    if r["id"] in OPEN_CIRCULATION:
        return False
    return r["kind"] in ("room", "circulation")


def pip(poly, x, y):
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            inside = not inside
    return inside


def cells_of(poly):
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    out = []
    for i in range(int(math.floor(min(xs) / CELL)), int(math.ceil(max(xs) / CELL))):
        for j in range(int(math.floor(min(ys) / CELL)), int(math.ceil(max(ys) / CELL))):
            if pip(poly, i * CELL + 1.0, j * CELL + 1.0):
                out.append((i, j))
    return out


def rect_of(poly):
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    return min(xs), min(ys), max(xs), max(ys)


def stair_geom(s):
    """(u, footprint cells, rect) of a stair: u = unit run direction bottom -> top (m), cells on the 2 m grid."""
    bx, by = s["bottom"]
    tx, ty = s["top"]
    w = s["width"]
    if s["kind"] == "ladder":
        return (1.0, 0.0), [(int(math.floor(bx / CELL)), int(math.floor(by / CELL)))], (bx - 0.5, by - 0.5, bx + 0.5, by + 0.5)
    L_ = math.hypot(tx - bx, ty - by)
    u = ((tx - bx) / L_, (ty - by) / L_)
    if abs(u[0]) > 0.5:
        rect = (min(bx, tx), by - w / 2.0, max(bx, tx), by + w / 2.0)
    else:
        rect = (bx - w / 2.0, min(by, ty), bx + w / 2.0, max(by, ty))
    cells = [(i, j) for i in range(int(math.floor(rect[0] / CELL)), int(math.ceil(rect[2] / CELL)))
             for j in range(int(math.floor(rect[1] / CELL)), int(math.ceil(rect[3] / CELL)))]
    return u, cells, rect


class Grid:
    """Per-level cell occupancy: occ[fid][(i, j)] = room id ('~void' for the hall's upper storey)."""

    def __init__(self, L):
        self.L = L
        self.R = rooms(L)
        self.occ = {f: {} for f in ORDER}
        for r in L["rooms"]:
            for c in cells_of(r["poly"]):
                self.occ[r["floor"]][c] = r["id"]
            if r.get("storeys", 1) == 2 and r.get("void"):
                for c in cells_of(r["void"]):
                    self.occ["F1"].setdefault(c, "~void:" + r["id"])
        # stair wells: the lower level's part outside any room joins the lower room (annex); the upper level's
        # cells under the run are holes in its floor
        self.holes = {f: set() for f in ORDER}
        self.stair_cells = {f: {} for f in ORDER}
        for s in L["stairs"]:
            lo = self.R[s["lower"]]["floor"]
            up = self.R[s["upper"]]["floor"]
            u, cells, rect = stair_geom(s)
            for c in cells:
                if c not in self.occ[lo]:
                    self.occ[lo][c] = s["lower"]
                self.stair_cells[lo][c] = s["id"]
                self.holes[up].add(c)

    def room_at(self, fid, c):
        rid = self.occ[fid].get(c)
        if rid is None:
            return None
        if rid.startswith("~void:"):
            return self.R[rid[6:]]
        return self.R[rid]

    def is_void(self, fid, c):
        rid = self.occ[fid].get(c)
        return bool(rid and rid.startswith("~void:"))

    def island(self, pad_cells=2):
        """Cells of the rock plateau: every ground-floor or cellar cell, grown by pad_cells."""
        base = set(self.occ["F0"]) | set(self.occ["C"])
        out = set(base)
        for (i, j) in base:
            for di in range(-pad_cells, pad_cells + 1):
                for dj in range(-pad_cells, pad_cells + 1):
                    out.add((i + di, j + dj))
        return out


def anchor_floor(L, a):
    return rooms(L)[a["room"]]["floor"]
