"""Morrowmere v2 underground: the well cellar, the old mine tunnel and the catacombs under Crown Hill.

Single source of truth for the Unreal builder (Tools/Unreal/kg_build_underground.py, imported inside the editor) and
the map renderer (Tools/Level/render_underground_map.py, system Python). Pure Python, no third-party imports.

    python Tools/Level/underground_layout.py        -> writes Tools/Level/underground_layout_v2.json (+ prints a check)

The underground is a 2 m grid in the SAME persistent level as the village, directly below it (so the underground map
lines up with the village map): floors at z 1.8 m, i.e. above the sea (the character swims below the wave surface
anywhere) and at least ~2.5 m under the terrain (Well Court z 8, Crown Hill z 14; checked against
Art/Packed/KG_Terrain_v2_heights.json by check()). Entrances are AKGPassage pairs (the well rim <-> the top of the well
shaft ladder, the mausoleum door <-> the crypt stair door): the terrain is one mesh and cannot be cut.

Grid: cell (i, j) spans x in [X0 + 2i, X0 + 2i + 2], y in [Y0 + 2j, Y0 + 2j + 2] (metres, UE axes: +y = south, the sea).
The well shaft cell (6, 30) is centred exactly under the Old Well (-33.13, -14.84); the crypt stair runs under the
mausoleum (-33.23, -65.07).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

CELL = 2.0
X0 = -46.13
Y0 = -75.84
FLOOR_Z = 1.8            # m, the walking surface
WALL_H = 3.12            # kit wall height (Wall_UnevenBrick_Straight)
MINE_H = 2.7             # MineWall_2m height
STAIR_H = 5.4            # CryptStair side walls

#          0123456789012       column i -> x centre X0 + 2i + 1 (-45.13 .. -21.13)
MAP = [
    "             ",  # 0   y -74.84
    "  VVV        ",  # 1
    "  VVV        ",  # 2
    "  VVV        ",  # 3
    "   G         ",  # 4   the vault gate (crypt key)
    "   C  S      ",  # 5   S = crypt stair up to the mausoleum door (top at row 5)
    "   C  S      ",  # 6
    "   C  S      ",  # 7
    "OO C HHH  PPP",  # 8
    "OOCCCHHHCCPPP",  # 9
    "OO   HHH  PPP",  # 10
    "      C      ",  # 11
    "      C      ",  # 12
    "      C      ",  # 13
    "      C      ",  # 14
    "      C      ",  # 15
    "     JJJ     ",  # 16  where the old mine broke into the crypt
    "     JJJ     ",  # 17
    "       T     ",  # 18
    "       T     ",  # 19
    "       TTTT  ",  # 20
    "          T  ",  # 21
    "          T  ",  # 22
    "          T  ",  # 23
    "          T  ",  # 24
    "          T  ",  # 25  the tunnel mouth, hidden behind the barrel racks
    "    WWWWWBB  ",  # 26
    "  LLWWWWWBB  ",  # 27
    "  LLWWWWWBB  ",  # 28
    "    WWWWWBB  ",  # 29
    "      X      ",  # 30  the well shaft (ladder up to the Old Well)
    "             ",  # 31
    "             ",  # 32
    "             ",  # 33
]
ROWS, COLS = len(MAP), len(MAP[0])

# style: floor / wall / ceiling family. crypt = cool blue-grey stone + niches + vaults; cellar = warm brick + timber;
# mine = rough rock + timber frames.
CELLS = {
    "C": {"style": "crypt", "ceiling": "vault", "niches": True},
    "G": {"style": "crypt", "ceiling": "vault", "niches": False},
    "H": {"style": "crypt", "ceiling": "flat"},
    "S": {"style": "stair", "ceiling": "high"},
    "O": {"style": "crypt", "ceiling": "flat"},
    "P": {"style": "crypt", "ceiling": "flat"},
    "V": {"style": "crypt", "ceiling": "flat"},
    "J": {"style": "mine", "ceiling": "mine"},
    "T": {"style": "mine", "ceiling": "mine"},
    "W": {"style": "cellar", "ceiling": "flat"},
    "B": {"style": "cellar", "ceiling": "flat"},
    "L": {"style": "cellar", "ceiling": "flat"},
    "X": {"style": "shaft", "ceiling": "none"},
}

# Map regions (the location toast + the underground map labels). layer 2 = place (toasts), 0 = the district.
REGIONS = [
    # layer 2 places are connected unions (rooms included); the rooms inside them are layer 3 and win where they are.
    {"id": "well_cellar", "name": "Well Cellar", "chars": "WXBL", "layer": 2, "icon": "well", "priority": 90},
    {"id": "barrel_store", "name": "Barrel Store", "chars": "B", "layer": 3, "icon": "", "priority": 60},
    {"id": "smugglers_nook", "name": "Smugglers' Nook", "chars": "L", "layer": 3, "icon": "", "priority": 60},
    {"id": "old_mine_tunnel", "name": "Old Mine Tunnel", "chars": "T", "layer": 2, "icon": "", "priority": 70},
    {"id": "broken_wall", "name": "Broken Wall", "chars": "J", "layer": 2, "icon": "", "priority": 55},
    {"id": "catacombs", "name": "Catacombs", "chars": "CGHSOPV", "layer": 2, "icon": "grave", "priority": 95},
    {"id": "crypt_hall", "name": "Crypt Hall", "chars": "H", "layer": 3, "icon": "", "priority": 70},
    {"id": "mausoleum_stair", "name": "Mausoleum Stair", "chars": "S", "layer": 3, "icon": "", "priority": 65},
    {"id": "ossuary", "name": "Ossuary", "chars": "O", "layer": 3, "icon": "grave", "priority": 75},
    {"id": "crypt_chapel", "name": "Crypt Chapel", "chars": "P", "layer": 3, "icon": "church", "priority": 75},
    {"id": "treasure_vault", "name": "Treasure Vault", "chars": "V", "layer": 3, "icon": "star", "priority": 85},
]
DISTRICT = {"id": "underground", "name": "Underground", "layer": 0}

# Crypt stair: foot on the south edge of (6, 7), rising north to the landing + door under the mausoleum.
STAIR = {"cells": [(6, 5), (6, 6), (6, 7)], "yaw": -90.0}

# Where the vault gate hangs: on the edge between (3, 4) and (3, 3), hinge on the west side.
GATE = {"cell": (3, 4), "edge": "N"}

# Openings that are NOT plain (cell pairs): arched stone doorways (Wall_Arch) between rooms.
ARCHES = [((2, 9), (1, 9)), ((4, 9), (5, 9)), ((7, 9), (8, 9)), ((9, 9), (10, 9)), ((6, 10), (6, 11)), ((6, 15), (6, 16)),
          ((3, 5), (3, 4)), ((6, 29), (6, 30)), ((3, 27), (4, 27)), ((3, 28), (4, 28))]

# Well and mausoleum anchors (layout v2, metres).
WELL = (-33.13, -14.84, 8.0)
MAUSOLEUM = {"at": (-33.23, -65.07), "z": 14.0, "yaw": 76.53 - 90.0}


def cell_at(i, j):
    if 0 <= j < ROWS and 0 <= i < COLS:
        c = MAP[j][i]
        return c if c != " " else None
    return None


def centre(i, j):
    return X0 + CELL * i + CELL / 2.0, Y0 + CELL * j + CELL / 2.0


def cells(chars=None):
    for j in range(ROWS):
        for i in range(COLS):
            c = cell_at(i, j)
            if c and (chars is None or c in chars):
                yield i, j, c


def ceiling_height(c):
    k = CELLS[c]["ceiling"]
    return {"vault": WALL_H + 0.75, "flat": WALL_H, "high": STAIR_H, "mine": MINE_H, "none": 4.4}[k]


def outline(cellset):
    """Boundary loops (metres) of a union of grid cells: list of polygons (outer first)."""
    edges = {}
    for i, j in cellset:
        x0, y0 = X0 + CELL * i, Y0 + CELL * j
        x1, y1 = x0 + CELL, y0 + CELL
        for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
            key = (round(b[0], 3), round(b[1], 3), round(a[0], 3), round(a[1], 3))
            if key in edges:
                del edges[key]          # shared edge between two cells of the set
            else:
                edges[(round(a[0], 3), round(a[1], 3), round(b[0], 3), round(b[1], 3))] = True
    nxt = {}
    for (ax, ay, bx, by) in edges:
        nxt.setdefault((ax, ay), []).append((bx, by))
    loops = []
    used = set()
    for start in list(nxt):
        for first in nxt[start]:
            if (start, first) in used:
                continue
            loop = [start]
            a, b = start, first
            while True:
                used.add((a, b))
                loop.append(b)
                outs = [n for n in nxt.get(b, []) if (b, n) not in used]
                if not outs or b == start:
                    break
                a, b = b, outs[0]
            if loop[-1] == loop[0]:
                loop = loop[:-1]
            # drop collinear points
            pts = []
            for k in range(len(loop)):
                p0, p1, p2 = loop[k - 1], loop[k], loop[(k + 1) % len(loop)]
                if abs((p1[0] - p0[0]) * (p2[1] - p1[1]) - (p1[1] - p0[1]) * (p2[0] - p1[0])) > 1e-6:
                    pts.append(p1)
            loops.append(pts)
    loops.sort(key=lambda L: -abs(area(L)))
    return loops


def area(poly):
    return 0.5 * sum(poly[k][0] * poly[(k + 1) % len(poly)][1] - poly[(k + 1) % len(poly)][0] * poly[k][1] for k in range(len(poly)))


def regions():
    """Map regions in cm (the AKGUndergroundInfo / FKGMapRegion format of KG_MapRegions_*.json)."""
    out = []
    allc = [(i, j) for i, j, _ in cells()]
    loops = outline(allc)
    cx = sum(centre(i, j)[0] for i, j in allc) / len(allc)
    cy = sum(centre(i, j)[1] for i, j in allc) / len(allc)
    out.append({"id": DISTRICT["id"], "name": DISTRICT["name"], "kind": "district", "layer": 0, "toast": True,
                "center": [round(cx * 100, 1), round(cy * 100, 1)], "radius": 0.0,
                "polygon": [[round(x * 100, 1), round(y * 100, 1)] for x, y in loops[0]],
                "label": [round(cx * 100, 1), round(cy * 100, 1)], "icon": "", "priority": -1})
    for r in REGIONS:
        cs = [(i, j) for i, j, c in cells(r["chars"])]
        loops = outline(cs)
        # label on the cell nearest the centroid
        mx = sum(centre(i, j)[0] for i, j in cs) / len(cs)
        my = sum(centre(i, j)[1] for i, j in cs) / len(cs)
        li, lj = min(cs, key=lambda c: (centre(*c)[0] - mx) ** 2 + (centre(*c)[1] - my) ** 2)
        lx, ly = centre(li, lj)
        out.append({"id": r["id"], "name": r["name"], "kind": "place" if r["layer"] == 2 else "building", "layer": r["layer"],
                    "toast": True, "center": [round(lx * 100, 1), round(ly * 100, 1)], "radius": 0.0,
                    "polygon": [[round(x * 100, 1), round(y * 100, 1)] for x, y in loops[0]],
                    "label": [round(lx * 100, 1), round(ly * 100, 1)], "icon": r["icon"], "priority": r["priority"]})
    return out


def volumes():
    """Below-ground boxes (cm): one per row run of cells, floor - 0.5 m .. ceiling + 0.6 m."""
    out = []
    for j in range(ROWS):
        i = 0
        while i < COLS:
            c = cell_at(i, j)
            if not c:
                i += 1
                continue
            k = i
            top = 0.0
            while k < COLS and cell_at(k, j):
                top = max(top, ceiling_height(cell_at(k, j)))
                k += 1
            x0, x1 = X0 + CELL * i, X0 + CELL * k
            y0, y1 = Y0 + CELL * j, Y0 + CELL * (j + 1)
            out.append([[round(x0 * 100, 1), round(y0 * 100, 1), round((FLOOR_Z - 0.5) * 100, 1)],
                        [round(x1 * 100, 1), round(y1 * 100, 1), round((FLOOR_Z + top + 0.6) * 100, 1)]])
            i = k
    return out


def check(verbose=True):
    """Every cell stays >= 1.5 m under the terrain (ceiling top incl. 0.3 m slab)."""
    path = os.path.join(ROOT, "Art", "Packed", "KG_Terrain_v2_heights.json")
    if not os.path.exists(path):
        return []
    d = json.load(open(path))
    c = d["core"]
    nx, ny, step = c["nx"], c["ny"], c["step"]
    H = c["heights"]

    def h(x, y):
        i = int(round((x - c["x0"]) / step))
        j = int(round((y - c["y0"]) / step))
        return H[j * nx + i] if 0 <= i < nx and 0 <= j < ny else None

    bad = []
    for i, j, ch in cells():
        top = FLOOR_Z + ceiling_height(ch) + 0.3
        x, y = centre(i, j)
        for dx in (-1.2, 0.0, 1.2):
            for dy in (-1.2, 0.0, 1.2):
                g = h(x + dx, y + dy)
                if g is not None and g - top < 1.5:
                    bad.append((i, j, ch, round(g, 2), round(top, 2)))
    if verbose:
        print(f"KG_UNDERGROUND check: {len(list(cells()))} cells, {len(bad)} too shallow {bad[:6]}")
    return bad


def as_json():
    return {
        "_doc": "Generated by Tools/Level/underground_layout.py - do not edit.",
        "grid": {"x0": X0, "y0": Y0, "cell": CELL, "cols": COLS, "rows": ROWS, "floor_z": FLOOR_Z},
        "map": MAP,
        "cells": CELLS,
        "stair": STAIR,
        "gate": GATE,
        "arches": ARCHES,
        "regions": regions(),
        "volumes": volumes(),
    }


if __name__ == "__main__":
    out = os.path.join(HERE, "underground_layout_v2.json")
    json.dump(as_json(), open(out, "w"), indent=1)
    bad = check()
    print(f"KG_UNDERGROUND layout -> {out}: {len(list(cells()))} cells, {len(REGIONS) + 1} regions, {len(volumes())} volumes")
    raise SystemExit(1 if bad else 0)
