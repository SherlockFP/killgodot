"""Render the Morrowmere minimap: a stylised schematic PNG (no text) + a regions JSON the HUD labels live.

    python Tools/Level/render_minimap.py [layout.json] [--out Art/Textures/Map] [--size 2048] [--preview]

Default layout: Tools/Level/morrowmere_layout.json (v1). Works with v2 (morrowmere_layout_v2.json) too: every section
is optional and unknown sections are ignored, so the script keeps working while the layout grows.

Outputs (in --out):
  T_KG_Map_<Key>.png   (<Key> from the layout file: Morrowmere, Morrowmere_v2) square, north (-Y) up, x east -> right; covers world_min..world_max exactly. Sea, beach,
                       height-tinted hillshaded land, fields, water, lanes/squares, piers, buildings, towers. No text:
                       labels are drawn live by AKGHUD so they stay crisp and localisable.
  KG_MapRegions_<Key>.json  {"world_min", "world_max" (cm), "regions": [{id, name, kind, layer, toast, center, radius,
                       polygon, label, icon, priority}]} - read by Tools/Unreal/kg_make_minimap.py into AKGMapInfo.
  --preview            also Saved/Minimap/preview_<layout>.png with labels + icons drawn (review only).

Region layers (AKGHUD picks the highest layer that contains the player, then the smallest):
  0 district   (v2 terraces / island)       toast + location line
  1 street     (lanes, buffered)            location line only
  2 place      (squares, areas, fields)     toast + location line
  3 building   (landmark footprints)        toast + location line
 -1 poi        (well, gallows, torii...)    map icon + label only
"""
import argparse
import json
import math
import os
import re

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage
from shapely.geometry import LineString, MultiPolygon, Point, Polygon
from shapely.ops import polylabel, unary_union

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_LAYOUT = os.path.join(ROOT, "Tools", "Level", "morrowmere_layout.json")
HEIGHTS = os.path.join(ROOT, "Art", "Packed", "KG_Terrain_heights.json")
OUT_DIR = os.path.join(ROOT, "Art", "Textures", "Map")
SS = 2   # supersampling for vector layers


# ================================================================================================= palette (sRGB)
def hexc(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


SEA_DEEP = hexc("#21628f")
SEA_MID = hexc("#2f86b3")
SEA_SHALLOW = hexc("#5cc0d4")
FOAM = hexc("#e9fbff")
SAND = hexc("#f0dca2")
GRASS_LOW = hexc("#a6d77a")
GRASS = hexc("#88c567")
GRASS_HIGH = hexc("#6cab5c")
HILL = hexc("#5a9455")
ROCK = hexc("#8f9a82")
PAVE = hexc("#f5ead0")          # cobbles / squares
PAVE_EDGE = hexc("#a88a5e")
FLAGS = hexc("#e9e2d2")         # stone flags (quay)
DIRT = hexc("#dcbc84")
DIRT_EDGE = hexc("#a57f4c")
GRAVEL = hexc("#ece3cf")
TIMBER = hexc("#b07a48")
TIMBER_EDGE = hexc("#6d4726")
WATER = hexc("#4fb3d9")
WATER_EDGE = hexc("#2b7fa8")
HOME = hexc("#e46a4a")          # warm terracotta roofs
HOME_EDGE = hexc("#8e3526")
SHELL = hexc("#cf8d6d")         # infill / shells
SHELL_EDGE = hexc("#83503a")
CIVIC = hexc("#f2b13c")         # civic buildings: HUD gold family, the first thing the eye finds
CIVIC_EDGE = hexc("#8f5a12")
TOWER = hexc("#7a67d8")         # towers: HUD violet
TOWER_EDGE = hexc("#34296e")
WALL = hexc("#5b4a3a")
CLIFF = hexc("#6a5a4c")
FIELD = {"carrots": hexc("#d59a4f"), "wheat": hexc("#f0cf5c"), "cabbages": hexc("#8fcf6a"), "apple": hexc("#7cbd5a")}

# ================================================================================================= names + icons
# Evocative English names (localised later through the String Table). id -> (name, icon).
NAMES = {
    # places / areas / squares
    "square": ("Gallows Square", "gallows"),
    "fountain_square": ("Fountain Square", "fountain"),
    "fish_market": ("Fish Market", "fish"),
    "church_yard": ("Churchyard", "church"),
    "smithy_yard": ("The Forge", "anvil"),
    "farm": ("Hollow Farm", "farm"),
    "woodcutter": ("Woodcutter's Camp", "axe"),
    "lighthouse": ("Lighthouse", "lighthouse"),
    "japan_garden": ("Sakura Garden", "torii"),
    "well_court": ("Well Court", "well"),
    "quay_head": ("Quay Head", "anchor"),
    "pilgrim_garden": ("Pilgrim Garden", "tree"),
    "belvedere": ("The Belvedere", "telescope"),
    "west_quay_apron": ("West Quay", "anchor"),
    "carrot_field": ("Carrot Field", "farm"),
    "wheat_field": ("Wheat Field", "farm"),
    "apple_orchard": ("Apple Orchard", "tree"),
    "cabbage_plots": ("Cabbage Patch", "farm"),
    "graveyard": ("Graveyard", "grave"),
    # landmarks
    "town_hall": ("Town Hall", "hall"),
    "church": ("Church of the Morrow", "church"),
    "bell_tower": ("Bell Tower", "bell"),
    "inn": ("The Latecomer Inn", "inn"),
    "bakery": ("Bakery", "bread"),
    "smithy": ("The Forge", "anvil"),
    "well": ("Old Well", "well"),
    "gallows": ("Moot Stage", "gallows"),
    "notice_board": ("Notice Board", "notice"),
    "boathouse": ("Boathouse", "boat"),
    "mill_barn": ("Red Barn", "barn"),
    "old_stone_bridge": ("Old Stone Bridge", ""),
    "clock_tower": ("Clock Tower", "clock"),
    "mausoleum": ("Mausoleum", "grave"),
    "windmill": ("Windmill", "windmill"),
    "pavilion": ("Garden Pavilion", "torii"),
    "fountain": ("Fountain", "fountain"),
    "dead_tree": ("Dead Tree", "tree"),
    "market_stalls": ("Market Stalls", "market"),
    "old_oak": ("Old Oak", "tree"),
    "belvedere_telescope": ("Telescope", "telescope"),
    "koi_pond": ("Koi Pond", "koi"),
    "garden_torii": ("Garden Gate", "torii"),
    "giant_sakura": ("Great Sakura", "tree"),
    "sea_torii": ("Sea Gate", "torii"),
    "pagoda": ("Pagoda", "pagoda"),
    "island": ("Shrine Island", "pagoda"),
    "harbour_light": ("Harbour Light", "lighthouse"),
    "waterwheel": ("Waterwheel", "mill"),
    "crane": ("Harbour Crane", "anchor"),
    "slipway": ("Slipway", "boat"),
    "farmyard": ("Farmyard", "farm"),
    "pen": ("Sheep Pen", "farm"),
    "pier": ("The Long Pier", "anchor"),
    # streets (v1)
    "harbour_street": ("Harbour Street", ""),
    "west_lane": ("West Lane", ""),
    "east_market": ("Market Row", ""),
    "church_lane": ("Chapel Lane", ""),
    "farm_track": ("Farm Track", ""),
    "back_alley": ("Cutpurse Alley", ""),
    "fish_alley": ("Gull Alley", ""),
    # streets (v2)
    "quay_promenade": ("Quay Promenade", ""),
    "tide_alley": ("Tide Alley", ""),
    "cat_ope_quay": ("The Long Ope", ""),
    "cat_ope_heart": ("The Long Ope", ""),
    "cat_steps": ("The Long Ope", ""),
    "chapel_steps": ("The Long Ope", ""),
    "net_ope_quay": ("Net Stairs", ""),
    "net_ope_heart": ("Net Stairs", ""),
    "net_stairs": ("Net Stairs", ""),
    "grand_stair": ("Grand Stair", ""),
    "scala": ("The Scala", ""),
    "pilgrim_stair": ("Pilgrim Stair", ""),
    "garden_stair": ("Garden Stair", ""),
    "cliff_stair": ("Cliff Stair", ""),
    "well_ramp": ("Cart Ramp", ""),
    "orchard_ramp": ("Orchard Ramp", ""),
    "graveyard_ramp": ("Graveyard Ramp", ""),
    "hilltop_track": ("Hilltop Track", ""),
    "jetty_ramp": ("Long Jetty", ""),
    "fish_pier_ramp": ("Fish Pier", ""),
    "rope_walk": ("Rope Walk", ""),
    "market_street": ("Market Street", ""),
    "back_lane_west": ("West Back Lane", ""),
    "back_lane_east": ("East Back Lane", ""),
    "chapel_wynd": ("The Long Ope", ""),
    "chapel_wynd_upper": ("The Long Ope", ""),
    "balcony_lane": ("Balcony Lane", ""),
    "crown_walk": ("Crown Walk", ""),
    "graveyard_path": ("Graveyard Path", ""),
    "brook_path": ("Brook Path", ""),
    "mill_lane": ("Mill Lane", ""),
    "mill_bridge_way": ("Mill Bridge Way", ""),
    "woods_path": ("Woods Path", ""),
    "hollow_way": ("Hollow Way", ""),
    "orchard_lane": ("Orchard Lane", ""),
    "mill_track": ("Mill Track", ""),
    "field_lane": ("Field Lane", ""),
    "orchard_walk": ("Orchard Walk", ""),
    "sakura_walk": ("Sakura Walk", ""),
    "koi_path": ("Koi Path", ""),
    "headland_road": ("Headland Road", ""),
    "orchard_link": ("Orchard Link", ""),
    "long_jetty": ("Long Jetty", ""),
    "jetty_head": ("Jetty Head", ""),
    "fish_pier": ("Fish Pier", ""),
    "mole_walk": ("The Mole", ""),
    # districts (v2)
    "harbour_row": ("Harbour Row", ""),
    "heart": ("The Heart", ""),
    "upper_town": ("Upper Town", ""),
    "crown_hill": ("Crown Hill", ""),
    "brookside": ("Brookside", ""),
    "orchard_upland": ("Orchard Upland", ""),
    "sakura_garden": ("Sakura Garden", ""),
    "lighthouse_point": ("Lighthouse Point", ""),
}
POI_RADIUS = {"graveyard": 8.0, "woodcutter": 7.0, "farmyard": 8.0, "pen": 5.0, "island": 17.0}
TOWERS = {"bell_tower", "lighthouse", "clock_tower"}
# v1 has a bare gallows in the square (the Moot Stage is v2's).
V1_NAMES = {"gallows": ("Gallows", "gallows"), "square": ("Gallows Square", "")}
_ACTIVE = {}


def _entry(ident):
    return _ACTIVE.get(ident) or NAMES.get(ident)


def pretty(ident):
    e = _entry(ident)
    return e[0] if e else " ".join(w.capitalize() for w in ident.split("_"))


def icon_of(ident, default=""):
    e = _entry(ident)
    return (e[1] if e else "") or default


# ================================================================================================= geometry helpers
def rect_poly(at, size, yaw_deg, grow=0.0):
    w, d = size[0] / 2.0 + grow, size[1] / 2.0 + grow
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    return Polygon([(at[0] + lx * c - ly * s, at[1] + lx * s + ly * c) for lx, ly in ((-w, -d), (w, -d), (w, d), (-w, d))])


def facing_yaw(at, face):
    """v1 builder: the house front (local -Y) looks at `face`."""
    return math.degrees(math.atan2(face[1] - at[1], face[0] - at[0])) + 90.0


def building_yaw(spec):
    if "yaw" in spec:
        return float(spec["yaw"])
    if "face" in spec:
        return facing_yaw(spec["at"], spec["face"])
    return 0.0


def polys(geom):
    if geom is None or geom.is_empty:
        return []
    if isinstance(geom, Polygon):
        return [geom]
    if isinstance(geom, MultiPolygon):
        return list(geom.geoms)
    return [g for g in getattr(geom, "geoms", []) if isinstance(g, Polygon)]


def lane_line(lane):
    pts = [(p[0], p[1]) for p in lane.get("points", [])]
    return LineString(pts) if len(pts) >= 2 else None


# ================================================================================================= world model
class World:
    """Everything the renderer and the region builder need, normalised from v1 or v2."""

    def __init__(self, L, path):
        self.L = L
        self.path = path
        self.v2 = "coast" in L or L.get("version", 1) >= 2
        self.heights = None
        if not self.v2 and os.path.exists(HEIGHTS):
            self.heights = json.load(open(HEIGHTS))
        lms = L.get("landmarks", {})
        # Buildings: (id, polygon, kind) with kind home / shell / civic / tower.
        self.buildings = []
        for k, h in enumerate(L.get("houses", [])):
            if "size" in h:
                self.buildings.append((h.get("id", f"house{k}"), rect_poly(h["at"], h["size"], building_yaw(h)), "home"))
        for k, h in enumerate(L.get("infill", [])):
            if "size" in h:
                self.buildings.append((h.get("id", f"infill{k}"), rect_poly(h["at"], h["size"], building_yaw(h)), "shell"))
        for name, h in lms.items():
            kind = h.get("kind", "civic" if not self.v2 else "")
            if kind not in ("civic", "special", "tower") and name not in TOWERS and name != "smithy":
                continue   # v2 point features (fountain, pagoda, farmyard...) are icons, not roofs
            if name in TOWERS or kind == "tower":
                size = h.get("size", [4, 4])
                self.buildings.append((name, Point(h["at"]).buffer(max(size) * 0.62, 24), "tower"))
            elif "size" in h:
                self.buildings.append((name, rect_poly(h["at"], h["size"], building_yaw(h)), "civic"))
            elif name == "smithy":   # v1 open shed
                self.buildings.append((name, rect_poly(h["at"], (4, 6), 90.0), "civic"))
        self.piers = []          # (LineString, width)
        self.water = []          # polygons (ponds, wells, streams)
        self.island = None
        extras = {}
        if not self.v2:
            extras = self.v1_extras()
        self.extras = extras

    # ---- v1: features the v1 builder hard-codes outside the layout JSON (kg_build_village.py)
    def v1_extras(self):
        ex = {}
        # pirate_dock(0, 5000): starts where the ground drops below 0.8 m, minus 3 m; 9 x 4.88 m sections.
        y0 = 50.0
        while self.height_at(0.0, y0) > 0.8 and y0 < 90.0:
            y0 += 1.0
        y0 -= 3.0
        end = y0 + 8 * 4.88
        self.piers.append((LineString([(0.0, y0 - 2.4), (0.0, end + 2.4)]), 3.2))
        ex["pier"] = {"at": [0.0, (y0 + end) / 2.0], "line": [[0.0, y0 - 2.4], [0.0, end + 2.4]]}
        # Shrine island across the bay + the sea torii in the shallows.
        self.island = Point(45.0, 150.0).buffer(17.0, 64)
        ex["island"] = {"at": [45.0, 150.0], "radius": 17.0}
        ex["sea_torii"] = {"at": [42.0, 119.0]}
        ex["pagoda"] = {"at": [45.0, 150.0]}
        # Japanese garden koi pond (7.6 x 4.6 m plane) + the square's well.
        self.water.append(rect_poly((26.4, 20.1), (7.6, 4.6), 0.0))
        if "well" in self.L.get("landmarks", {}):
            self.water.append(Point(self.L["landmarks"]["well"]["at"]).buffer(0.9, 24))
        return ex

    def height_at(self, x, y):
        h = self.heights
        if not h:
            return 4.0
        size, step, n, arr = h["size"], h["step"], h["n"], h["heights"]
        bx, by = x + size / 2, -y + size / 2
        fi, fj = max(0.0, min(n - 1e-3, bx / step)), max(0.0, min(n - 1e-3, by / step))
        i, j = int(fi), int(fj)
        tx, ty = fi - i, fj - j
        a, b = arr[j * (n + 1) + i], arr[j * (n + 1) + i + 1]
        c, d = arr[(j + 1) * (n + 1) + i], arr[(j + 1) * (n + 1) + i + 1]
        return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty

    # ---- bounds: every playable thing + a margin, squared
    def bounds(self):
        xs, ys = [], []

        def add(x, y):
            xs.append(x)
            ys.append(y)

        for lane in self.L.get("lanes", []):
            for p in lane.get("points", []):
                add(p[0], p[1])
        for _, poly, _ in self.buildings:
            for x, y in poly.exterior.coords:
                add(x, y)
        for a in self.L.get("areas", []):
            add(a["center"][0] - a["radius"], a["center"][1] - a["radius"])
            add(a["center"][0] + a["radius"], a["center"][1] + a["radius"])
        for t in self.L.get("tasks", []):
            add(*t["at"][:2])
        for line, _ in self.piers:
            for x, y in line.coords:
                add(x, y)
        # The shrine island is scenery across the bay (outside the nav bounds): not worth half the map.
        nb = self.L.get("nav_bounds")
        if nb:
            add(nb["min"][0], nb["min"][1])
            add(nb["max"][0], nb["max"][1])
        pad = 14.0
        x0, x1, y0, y1 = min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad
        span = max(x1 - x0, y1 - y0)
        span = math.ceil(span / 10.0) * 10.0
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        return round(cx - span / 2.0, 1), round(cy - span / 2.0, 1), span

    def island_geom(self):
        if self.island is not None:
            return self.island
        isl = self.L.get("landmarks", {}).get("island")
        if isl:
            self.island = Point(isl["at"][0], isl["at"][1]).buffer(isl.get("radius", 17.0), 64)
        return self.island


# ================================================================================================= raster painter
class Painter:
    def __init__(self, x0, y0, span, n):
        self.x0, self.y0, self.span, self.n = x0, y0, span, n
        self.N = n * SS
        self.k = self.N / span   # pixels per metre (supersampled)
        self.img = Image.new("RGBA", (self.N, self.N), (0, 0, 0, 255))

    def P(self, pts):
        return [((x - self.x0) * self.k, (y - self.y0) * self.k) for x, y in pts]

    def mask(self, geoms):
        m = Image.new("L", (self.N, self.N), 0)
        d = ImageDraw.Draw(m)
        for g in geoms if isinstance(geoms, (list, tuple)) else [geoms]:
            for poly in polys(g):
                d.polygon(self.P(poly.exterior.coords), fill=255)
                for hole in poly.interiors:
                    d.polygon(self.P(hole.coords), fill=0)
        return m

    def fill(self, geoms, color, alpha=1.0, blur_m=0.0, offset_m=(0.0, 0.0)):
        m = self.mask(geoms)
        if offset_m != (0.0, 0.0):
            m = m.transform(m.size, Image.AFFINE, (1, 0, -offset_m[0] * self.k, 0, 1, -offset_m[1] * self.k))
        if blur_m > 0:
            m = m.filter(ImageFilter.GaussianBlur(blur_m * self.k))
        if alpha < 1.0:
            m = m.point(lambda v: int(v * alpha))
        layer = Image.new("RGBA", m.size, color[:3] + (255,))
        self.img.paste(layer, (0, 0), m)

    def stroke(self, geoms, width_m, color, alpha=1.0):
        rings = []
        for g in geoms if isinstance(geoms, (list, tuple)) else [geoms]:
            for poly in polys(g):
                rings.append(poly.exterior.buffer(width_m / 2.0, 8))
                rings.extend(r.buffer(width_m / 2.0, 8) for r in [LineString(h.coords) for h in poly.interiors])
        if rings:
            self.fill(unary_union(rings), color, alpha)

    def inner_stroke(self, geom, width_m, color, alpha=1.0):
        ring = geom.difference(geom.buffer(-width_m, 8))
        self.fill(ring, color, alpha)

    def line(self, pts, width_m, color, alpha=1.0, cap=1):
        self.fill(LineString(pts).buffer(width_m / 2.0, 8, cap_style=cap), color, alpha)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def ramp(z, stops):
    """stops: [(z, rgba)] ascending -> float RGB array."""
    zs = np.array([s[0] for s in stops], dtype=np.float32)
    out = np.zeros(z.shape + (3,), dtype=np.float32)
    for c in range(3):
        out[..., c] = np.interp(z, zs, np.array([s[1][c] for s in stops], dtype=np.float32))
    return out


# ================================================================================================= terrain raster
def terrain(world, pt):
    """Height field (m) + land mask at supersampled resolution -> base RGB (uint8) with sea, beach, hillshade."""
    N, k = pt.N, pt.k
    xs = pt.x0 + (np.arange(N, dtype=np.float32) + 0.5) / k
    ys = pt.y0 + (np.arange(N, dtype=np.float32) + 0.5) / k
    L = world.L
    if world.heights:
        h = world.heights
        n, step, size = h["n"], h["step"], h["size"]
        grid = np.array(h["heights"], dtype=np.float32).reshape(n + 1, n + 1)   # [j][i], i = (x+size/2)/step
        gi = (xs + size / 2) / step
        gj = (-ys + size / 2) / step
        GJ, GI = np.meshgrid(gj, gi, indexing="ij")
        Z = ndimage.map_coordinates(grid, [GJ, GI], order=3, mode="nearest").astype(np.float32)
        land = Z > 0.15
        isl = world.island_geom()
        if isl is not None:
            im = np.array(pt.mask(isl)) > 127
            Z = np.where(im & ~land, 3.0, Z)
            land |= im
    else:
        land_poly = Polygon(L["coast"]["land_polygon"]) if "coast" in L else Polygon(
            [(-1e4, -1e4), (1e4, -1e4), (1e4, 1e4), (-1e4, 1e4)])
        geoms = [land_poly]
        isl = world.island_geom()
        if isl is not None:
            geoms.append(isl)
        land = np.array(pt.mask(geoms)) > 127
        # Terraces paint their heights; natural ground takes the nearest terrace height and rises gently away from
        # it; "soft" terraces blend into the natural ground over soft_m.
        Xg = xs[None, :]
        Yg = ys[:, None]
        Zt = np.full((N, N), np.nan, dtype=np.float32)
        soft = []
        for t in L.get("terraces", []):
            if len(t.get("polygon", [])) < 3:
                continue
            m = np.array(pt.mask(Polygon(t["polygon"]).buffer(0))) > 127
            if t.get("z") is not None:
                zt = np.full((N, N), float(t["z"]), dtype=np.float32)
            elif t.get("z_profile"):
                pr = t["z_profile"]
                s = (Xg - pr["origin"][0]) * pr["dir"][0] + (Yg - pr["origin"][1]) * pr["dir"][1]
                st = np.array(pr["stops"], dtype=np.float32)
                zt = np.interp(s, st[:, 0], st[:, 1]).astype(np.float32)
            else:
                continue
            m &= np.isnan(Zt)
            Zt = np.where(m, zt, Zt)
            if t.get("edge") == "soft":
                soft.append((m, float(t.get("soft_m", 6.0))))
        hard = np.zeros((N, N), dtype=bool)
        for t in L.get("terraces", []):
            if t.get("edge") != "soft" and t.get("z") is not None and len(t.get("polygon", [])) >= 3:
                hard |= np.array(pt.mask(Polygon(t["polygon"]).buffer(0))) > 127
        known = ~np.isnan(Zt)
        if known.any():
            dist_t, (ii, jj) = ndimage.distance_transform_edt(~known, return_indices=True)
            natural = Zt[ii, jj] + np.minimum(dist_t / k * 0.08, 8.0)
        else:
            natural = np.full((N, N), float(L.get("plateau_z", 5.0)), dtype=np.float32)
        Z = np.where(known, Zt, natural)
        for m, sm in soft:
            inside = ndimage.distance_transform_edt(m) / k
            w = smoothstep(0.0, sm, inside)
            Z = np.where(m, natural * (1 - w) + Zt * w, Z)
        Z = np.where(hard, Z, ndimage.gaussian_filter(Z, sigma=4.0 * k))   # natural slopes read soft, terraces crisp
        Z = np.where(land, Z, -3.0).astype(np.float32)
        for mnd in L.get("mounds", []):
            d = np.hypot(Xg - mnd["center"][0], Yg - mnd["center"][1])
            kk = np.clip(1 - d / mnd["radius"], 0, 1)
            Z = Z + mnd["height"] * kk * kk * (3 - 2 * kk)
        beach = L.get("coast", {}).get("beach_z", 0.8)
        # Sand strip: land within ~4 m of the sea where no terrace sits.
        dist_sea = ndimage.distance_transform_edt(land) / k
        Z = np.where(land & (dist_sea < 4.0) & (Z < 3.0), np.minimum(Z, beach), Z)
    # Hillshade from a softened height field (light from the north-west, like a classic map).
    Zs = ndimage.gaussian_filter(np.where(land, Z, np.minimum(Z, 0.0)), sigma=1.2 * k)
    gy, gx = np.gradient(Zs, 1.0 / k)
    lx, ly, lz = -0.55, -0.65, 0.9
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    shade = (-gx * lx - gy * ly + lz) / np.sqrt(gx * gx + gy * gy + 1.0) / ln
    shade = np.clip(shade, 0.0, 1.2)
    # Land colours by height.
    land_rgb = ramp(Z, [(-1.0, SAND), (0.9, SAND), (1.6, GRASS_LOW), (6.0, GRASS), (14.0, GRASS_HIGH), (24.0, HILL),
                        (40.0, ROCK), (80.0, ROCK)])
    land_rgb *= (0.62 + 0.42 * shade)[..., None]
    # Sea: shallow near the coast -> deep; two faint "contour" lines follow the shore.
    dist = ndimage.distance_transform_edt(~land) / k
    t_deep = smoothstep(0.0, 28.0, dist)
    sea_rgb = ramp(t_deep, [(0.0, SEA_SHALLOW), (0.35, SEA_MID), (1.0, SEA_DEEP)])
    for d0, a in ((3.2, 0.22), (8.5, 0.12)):
        band = np.exp(-((dist - d0) / (0.28)) ** 2) * a
        sea_rgb = sea_rgb * (1 - band[..., None]) + np.array(FOAM[:3], np.float32) * band[..., None]
    foam = np.exp(-((dist - 0.25) / 0.45) ** 2) * 0.85 * (dist > 0)
    sea_rgb = sea_rgb * (1 - foam[..., None]) + np.array(FOAM[:3], np.float32) * foam[..., None]
    rgb = np.where(land[..., None], land_rgb, sea_rgb)
    # Edge vignette: the map fades darker toward the texture border (reads as "beyond the village").
    u = (np.arange(N, dtype=np.float32) + 0.5) / N
    e = np.minimum(np.minimum(u[None, :], 1 - u[None, :]), np.minimum(u[:, None], 1 - u[:, None]))
    rgb *= (0.72 + 0.28 * smoothstep(0.0, 0.14, e))[..., None]
    return np.clip(rgb, 0, 255).astype(np.uint8), Z, land


# ================================================================================================= vector layers
SURFACE = {"cobble": (PAVE, PAVE_EDGE), "cobble_fan": (PAVE, PAVE_EDGE), "stone_flags": (FLAGS, PAVE_EDGE),
           "dirt": (DIRT, DIRT_EDGE), "gravel": (GRAVEL, PAVE_EDGE), "grass": (GRASS_LOW, PAVE_EDGE),
           "timber": (TIMBER, TIMBER_EDGE)}
PAVED_AREAS = {"square", "fish_market", "smithy_yard", "church_yard", "well_court", "quay_head"}
DIRT_AREAS = {"farm", "woodcutter"}


def draw_features(world, pt):
    L = world.L
    # Fields (v2) with crop rows.
    for f in L.get("fields", []):
        poly = Polygon(f["polygon"]).buffer(0)
        crop = f.get("crop", "")
        col = next((c for key, c in FIELD.items() if key in crop), FIELD["cabbages"])
        pt.fill(poly, col, 0.9)
        minx, miny, maxx, maxy = poly.bounds
        rows = []
        y = miny + 1.0
        while y < maxy:
            rows.append(LineString([(minx - 1, y), (maxx + 1, y)]).buffer(0.35))
            y += 2.2
        if rows:
            pt.fill(unary_union(rows).intersection(poly.buffer(-0.8)), (0, 0, 0), 0.12)
        pt.stroke(poly, 0.5, WALL, 0.55)
    # v1 farm: a striped field in the farm area.
    if not world.v2:
        for a in L.get("areas", []):
            if a["name"] == "farm":
                cx, cy = a["center"]
                poly = rect_poly((cx - 2.0, cy - 3.0), (14.0, 10.0), 0.0)
                pt.fill(poly, FIELD["carrots"], 0.9)
                rows = [LineString([(cx - 10, cy - 7.5 + i * 2.0), (cx + 6, cy - 7.5 + i * 2.0)]).buffer(0.35) for i in range(6)]
                pt.fill(unary_union(rows).intersection(poly.buffer(-0.6)), (0, 0, 0), 0.14)
                pt.stroke(poly, 0.45, WALL, 0.5)
    # Stream + ponds (v2).
    st = L.get("stream")
    if st and len(st.get("points", [])) >= 2:
        line = LineString([(p[0], p[1]) for p in st["points"]])
        w = st.get("width", 3.0)
        pt.fill(line.buffer(w / 2 + 0.6, 12), WATER_EDGE, 0.8)
        pt.fill(line.buffer(w / 2, 12), WATER)
        for pond in st.get("ponds", []):
            g = Point(pond["center"]).buffer(pond["radius"], 48)
            pt.fill(g.buffer(0.6), WATER_EDGE, 0.8)
            pt.fill(g, WATER)
    ks = L.get("koi_spill")
    if ks and len(ks.get("points", [])) >= 2:
        pt.fill(LineString(ks["points"]).buffer(ks.get("width", 0.8) / 2 + 0.2), WATER)
    for name in ("koi_pond",):
        lmk = L.get("landmarks", {}).get(name)
        if lmk:
            world.water.append(Point(lmk["at"]).buffer(3.2, 32))


TREE_STYLE = {   # mesh keyword -> (canopy radius m at scale 1, colour)
    "sakura": (2.8, hexc("#f4a6c8")), "maple": (2.4, hexc("#e8743f")), "bamboo": (1.2, hexc("#9bd36a")),
    "pine": (2.0, hexc("#3f7f4f")), "dead": (1.6, hexc("#9a8a74")), "twisted": (2.2, hexc("#6f9a4e")),
    "bonsai": (1.4, hexc("#4f8f55")), "oak": (3.4, hexc("#5fa04e")), "tree": (2.8, hexc("#5aa04f")),
    "bush": (0.9, hexc("#6fb257")),
}


def draw_trees(world, pt, trees):
    """trees: [[x_m, y_m, scale, mesh_name], ...] dumped from the level by kg_make_minimap.py."""
    if not trees:
        return
    x0, y0, span = pt.x0, pt.y0, pt.span
    items = []
    for x, y, sc, name in trees:
        if not (x0 - 5 < x < x0 + span + 5 and y0 - 5 < y < y0 + span + 5):
            continue
        low = name.lower()
        r, col = next((v for key, v in TREE_STYLE.items() if key in low), TREE_STYLE["tree"])
        items.append((x, y, min(r * max(sc, 0.4), 7.0), col))
    if not items:
        return
    shadow = Image.new("L", (pt.N, pt.N), 0)
    ds = ImageDraw.Draw(shadow)
    for x, y, r, _ in items:
        cx, cy = (x - x0 + 0.5) * pt.k, (y - y0 + 0.8) * pt.k
        ds.ellipse([cx - r * pt.k, cy - r * pt.k, cx + r * pt.k, cy + r * pt.k], fill=255)
    shadow = shadow.filter(ImageFilter.GaussianBlur(0.6 * pt.k)).point(lambda v: int(v * 0.38))
    pt.img.paste(Image.new("RGBA", shadow.size, (18, 40, 22, 255)), (0, 0), shadow)
    d = ImageDraw.Draw(pt.img)
    for x, y, r, col in sorted(items, key=lambda t: t[1]):
        cx, cy, R = (x - x0) * pt.k, (y - y0) * pt.k, r * pt.k
        dark = tuple(int(c * 0.72) for c in col[:3])
        d.ellipse([cx - R, cy - R, cx + R, cy + R], fill=dark)
        d.ellipse([cx - R * 0.9, cy - R * 0.95, cx + R * 0.8, cy + R * 0.75], fill=col[:3])
        hl = tuple(min(255, int(c * 1.18 + 12)) for c in col[:3])
        d.ellipse([cx - R * 0.62, cy - R * 0.66, cx + R * 0.05, cy + R * 0.0], fill=hl)


def draw_graveyards(world, pt):
    L = world.L
    spots = []
    gy = L.get("landmarks", {}).get("graveyard")
    if gy:
        size = gy.get("size", [16, 14])
        spots.append(rect_poly(gy["at"], size, gy.get("yaw", 0.0)) if "size" in gy else Point(gy["at"]).buffer(7.0, 32))
    for g in spots:
        pt.fill(g, hexc("#6f9a62"), 0.85)
        minx, miny, maxx, maxy = g.bounds
        stones = []
        y = miny + 1.6
        row = 0
        while y < maxy - 1.0:
            x = minx + 1.4 + (0.8 if row % 2 else 0.0)
            while x < maxx - 1.0:
                p = Point(x, y)
                if g.buffer(-0.6).contains(p):
                    stones.append(rect_poly((x, y), (0.9, 0.45), 0.0))
                x += 2.0
            y += 2.2
            row += 1
        if stones:
            pt.fill(unary_union(stones), hexc("#d9d6cf"))
            pt.stroke(stones, 0.12, hexc("#5a564e"), 0.8)
        pt.stroke(g, 0.45, WALL, 0.7)


def draw_water(world, pt):
    for name, rad in (("fountain", 1.7), ("well", 0.9)):
        lmk = world.L.get("landmarks", {}).get(name)
        if lmk and world.v2:
            g = Point(lmk["at"][:2]).buffer(rad, 32)
            pt.fill(g.buffer(0.55), hexc("#e8e2d4"))
            pt.stroke(g.buffer(0.55), 0.18, PAVE_EDGE, 0.9)
            world.water.append(g)
    for g in world.water:
        pt.fill(g.buffer(0.5), WATER_EDGE, 0.9)
        pt.fill(g, WATER)


def draw_lanes(world, pt):
    L = world.L
    groups = {}   # surface -> [geoms]
    for lane in L.get("lanes", []):
        line = lane_line(lane)
        if line is None or lane.get("kind") in ("pier",):
            continue
        surf = lane.get("surface") or ("dirt" if "track" in lane["name"] else "cobble")
        groups.setdefault(surf, []).append(line.buffer(lane.get("width", 3.0) / 2.0, 10))
    for sq in L.get("squares", []):
        groups.setdefault(sq.get("surface", "cobble"), []).append(Polygon(sq["polygon"]).buffer(0))
    for r in L.get("ramps", []):
        if len(r.get("points", [])) >= 2:
            groups.setdefault("cobble", []).append(LineString(r["points"]).buffer(r.get("width", 3.0) / 2.0, 10))
    for a in L.get("areas", []):
        if a["name"] in PAVED_AREAS:
            groups.setdefault("cobble", []).append(Point(a["center"]).buffer(a["radius"] * (0.8 if world.v2 else 0.95), 48))
        elif a["name"] in DIRT_AREAS:
            groups.setdefault("dirt", []).append(Point(a["center"]).buffer(a["radius"] * 0.75, 48))
        elif a["name"] == "japan_garden" and not world.v2:
            groups.setdefault("gravel", []).append(Point(a["center"]).buffer(a["radius"] * 0.8, 48))
    if "quay" in L and len(L["quay"].get("edge", [])) >= 2:
        pass   # the quay terrace is already paved via the promenade + terrace tint
    merged = {s: unary_union(g) for s, g in groups.items()}
    everything = unary_union(list(merged.values())) if merged else None
    if everything is not None:
        # One soft shadow + one outline under every paved surface so junctions merge cleanly.
        pt.fill(everything, (0, 0, 0), 0.18, blur_m=0.5, offset_m=(0.25, 0.4))
        pt.fill(everything.buffer(0.45, 8), PAVE_EDGE, 0.85)
    for surf in ("grass", "dirt", "gravel", "stone_flags", "cobble", "cobble_fan"):
        if surf in merged:
            pt.fill(merged[surf], SURFACE.get(surf, SURFACE["cobble"])[0])
    for surf, g in merged.items():
        if surf not in ("grass", "dirt", "gravel", "stone_flags", "cobble", "cobble_fan"):
            pt.fill(g, SURFACE.get(surf, SURFACE["cobble"])[0])
    # Stairs: tread hatching.
    for s in L.get("stairs", []):
        a, b = s["from"], s["to"]
        dx, dy = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(dx, dy) or 1.0
        ux, uy = dx / ln, dy / ln
        w = s.get("width", 3.0) / 2.0
        body = LineString([a, b]).buffer(w, cap_style=2)
        pt.fill(body, PAVE)
        steps = max(3, int(ln / 0.9))
        treads = []
        for i in range(1, steps):
            cx, cy = a[0] + ux * ln * i / steps, a[1] + uy * ln * i / steps
            treads.append(LineString([(cx - uy * w, cy + ux * w), (cx + uy * w, cy - ux * w)]).buffer(0.09))
        pt.fill(unary_union(treads), PAVE_EDGE, 0.8)
        pt.stroke(body, 0.3, PAVE_EDGE, 0.9)
    # Retaining walls and cliffs.
    for w in L.get("retaining_walls", []):
        if len(w.get("points", [])) >= 2:
            pt.line(w["points"], 0.9, WALL, 0.85)
    for c in L.get("cliffs", []):
        if len(c.get("points", [])) >= 2:
            pt.line(c["points"], 1.6, CLIFF, 0.8)
    q = L.get("quay")
    if q and len(q.get("edge", [])) >= 2:
        pt.line(q["edge"], 0.9, WALL, 0.9)
    # Mole (v2): rock armour + walk.
    mole = L.get("mole")
    if mole and len(mole.get("points", [])) >= 2:
        line = LineString(mole["points"])
        pt.fill(line.buffer(mole.get("width", 6.0) / 2.0, 10), ROCK)
        pt.fill(line.buffer(mole.get("walk_width", 3.0) / 2.0, 10), FLAGS)
    # Piers / jetties (timber) + bridges.
    for lane in L.get("lanes", []):
        if lane.get("kind") == "pier":
            line = lane_line(lane)
            if line is not None:
                world.piers.append((line, lane.get("width", 3.0)))
    for line, w in world.piers:
        body = line.buffer(w / 2.0, cap_style=2)
        pt.fill(body, (0, 0, 0), 0.25, blur_m=0.5, offset_m=(0.3, 0.5))
        pt.fill(body.buffer(0.35, join_style=2), TIMBER_EDGE)
        pt.fill(body, TIMBER)
        planks = []
        c = line.coords
        (x0, y0), (x1, y1) = c[0], c[-1]
        ln = line.length
        ux, uy = (x1 - x0) / ln, (y1 - y0) / ln
        d = 1.1
        while d < ln:
            cx, cy = x0 + ux * d, y0 + uy * d
            planks.append(LineString([(cx - uy * w / 2, cy + ux * w / 2), (cx + uy * w / 2, cy - ux * w / 2)]).buffer(0.06))
            d += 1.1
        if planks:
            pt.fill(unary_union(planks), TIMBER_EDGE, 0.55)
    for b in L.get("bridges", []):
        ax, ay = b["along"]
        n = math.hypot(ax, ay) or 1.0
        ax, ay = ax / n, ay / n
        half = b.get("span", 5.0) / 2.0
        line = LineString([(b["at"][0] - ax * half, b["at"][1] - ay * half), (b["at"][0] + ax * half, b["at"][1] + ay * half)])
        body = line.buffer(b.get("width", 2.5) / 2.0, cap_style=2)
        pt.fill(body.buffer(0.35, join_style=2), WALL)
        pt.fill(body, FLAGS if "stone" in b["name"] else TIMBER)


def draw_buildings(world, pt):
    homes = [p for _, p, k in world.buildings if k == "home"]
    shells = [p for _, p, k in world.buildings if k == "shell"]
    civic = [p for _, p, k in world.buildings if k == "civic"]
    towers = [p for _, p, k in world.buildings if k == "tower"]
    allb = homes + shells + civic + towers
    if allb:
        pt.fill(unary_union(allb), (20, 12, 30), 0.42, blur_m=0.7, offset_m=(0.7, 1.1))   # cast shadow (sun NW)
    for group, fill, edge in ((shells, SHELL, SHELL_EDGE), (homes, HOME, HOME_EDGE), (civic, CIVIC, CIVIC_EDGE)):
        for poly in group:
            pt.fill(poly, fill)
            # Roof: lit north-west slope, ridge line along the long axis.
            c = list(poly.exterior.coords)[:4]
            e0 = math.dist(c[0], c[1])
            e1 = math.dist(c[1], c[2])
            if e0 >= e1:
                m0 = ((c[0][0] + c[3][0]) / 2, (c[0][1] + c[3][1]) / 2)
                m1 = ((c[1][0] + c[2][0]) / 2, (c[1][1] + c[2][1]) / 2)
                half_a = Polygon([c[0], c[1], m1, m0])
            else:
                m0 = ((c[0][0] + c[1][0]) / 2, (c[0][1] + c[1][1]) / 2)
                m1 = ((c[3][0] + c[2][0]) / 2, (c[3][1] + c[2][1]) / 2)
                half_a = Polygon([c[0], m0, m1, c[3]])
            half_b = poly.difference(half_a)
            # whichever half faces north-west (smaller x + y centroid) is lit
            lit, dark = (half_a, half_b) if (half_a.centroid.x + half_a.centroid.y) < (half_b.centroid.x + half_b.centroid.y) else (half_b, half_a)
            pt.fill(lit, (255, 255, 255), 0.16)
            pt.fill(dark, (0, 0, 0), 0.10)
            pt.line([m0, m1], 0.28, (255, 240, 220), 0.55)
            pt.inner_stroke(poly, 0.38, edge, 0.95)
    for poly in towers:
        pt.fill(poly, TOWER)
        pt.fill(poly.buffer(-poly.length / (2 * math.pi) * 0.45), (255, 255, 255), 0.22)
        pt.inner_stroke(poly, 0.4, TOWER_EDGE, 0.95)


def render(world, n, trees=None):
    x0, y0, span = world.bounds()
    pt = Painter(x0, y0, span, n)
    rgb, Z, land = terrain(world, pt)
    pt.img = Image.fromarray(rgb, "RGB").convert("RGBA")
    draw_features(world, pt)
    draw_graveyards(world, pt)
    draw_lanes(world, pt)
    draw_water(world, pt)
    draw_trees(world, pt, trees)
    draw_buildings(world, pt)
    out = pt.img.convert("RGB").resize((n, n), Image.LANCZOS)
    return out, (x0, y0, span)


# ================================================================================================= regions
def region(rid, name, kind, layer, toast, center, label, icon, priority, radius=0.0, polygon=None):
    r = {"id": rid, "name": name, "kind": kind, "layer": layer, "toast": toast,
         "center": [round(center[0] * 100.0, 1), round(center[1] * 100.0, 1)], "radius": round(radius * 100.0, 1),
         "polygon": [], "label": [round(label[0] * 100.0, 1), round(label[1] * 100.0, 1)], "icon": icon,
         "priority": priority}
    if polygon is not None and not polygon.is_empty:
        g = polygon.simplify(0.4)
        big = max(polys(g), key=lambda p: p.area)
        r["polygon"] = [[round(x * 100.0, 1), round(y * 100.0, 1)] for x, y in list(big.exterior.coords)[:-1]]
    return r


def label_point(geom):
    try:
        p = polylabel(max(polys(geom), key=lambda q: q.area), tolerance=0.5)
        return (p.x, p.y)
    except Exception:
        c = geom.representative_point()
        return (c.x, c.y)


def build_regions(world):
    L = world.L
    out = []
    used = set()
    lms = L.get("landmarks", {})

    # 0: districts (v2): union of the terraces that name them.
    by_district = {}
    for t in L.get("terraces", []):
        if t.get("district") and len(t.get("polygon", [])) >= 3:
            by_district.setdefault(t["district"], []).append(Polygon(t["polygon"]).buffer(0))
    for did, geoms in by_district.items():
        g = unary_union(geoms)
        out.append(region(did, pretty(did), "district", 0, True, (g.centroid.x, g.centroid.y), label_point(g), "", 90,
                          polygon=g))
    isl = world.island_geom()
    if isl is not None:
        c = isl.centroid
        out.append(region("island", pretty("island"), "district", 0, True, (c.x, c.y), (c.x, c.y + 10.0),
                          icon_of("island", "pagoda"), 85, radius=isl.length / (2 * math.pi) + 2.0))
        used.add("island")

    # 2: places - squares, areas (unless a square already covers them), fields.
    squares = []
    for sq in L.get("squares", []):
        g = Polygon(sq["polygon"]).buffer(0)
        squares.append(g)
        out.append(region(sq["name"], pretty(sq["name"]), "place", 2, True, (g.centroid.x, g.centroid.y), label_point(g),
                          icon_of(sq["name"]), 75, polygon=g))
        used.add(sq["name"])
    for a in L.get("areas", []):
        c = Point(a["center"])
        if a["name"] in used or any(s.contains(c) for s in squares):
            continue
        name = pretty(a["name"])
        if any(r["name"] == name for r in out):
            continue
        prio = 80 if a["name"] in ("square", "fish_market", "church_yard") else 65
        icon = icon_of(a["name"])
        # Areas named after a tower/landmark get the landmark's icon at the landmark itself.
        out.append(region(a["name"], name, "place", 2, True, a["center"], a["center"], icon, prio, radius=a["radius"]))
        used.add(a["name"])
    for f in L.get("fields", []):
        g = Polygon(f["polygon"]).buffer(0)
        out.append(region(f["name"], pretty(f["name"]), "place", 2, True, (g.centroid.x, g.centroid.y), label_point(g),
                          icon_of(f["name"], "farm"), 55, polygon=g))

    # 3: buildings with footprints; -1: point landmarks.
    for bid, poly, kind in world.buildings:
        if bid not in lms:
            continue
        name = pretty(bid)
        dup = next((r for r in out if r["name"] == name), None)
        c = poly.centroid
        if dup is not None:
            # The area already carries this name (e.g. v1 "Old Chapel" yard + church): keep one label, but let the
            # building itself be a toast region too.
            dup["icon"] = dup["icon"] or icon_of(bid)
            out.append(region(bid, name, "building", 3, True, (c.x, c.y), (c.x, c.y), "", -1,
                              polygon=poly.buffer(0.8, join_style=2)))
            continue
        prio = 72 if kind in ("civic", "tower") else 50
        out.append(region(bid, name, "building", 3, True, (c.x, c.y), (c.x, c.y), icon_of(bid, "house"), prio,
                          polygon=poly.buffer(0.8, join_style=2)))
        used.add(bid)
    for lid, lmk in lms.items():
        if lid in used or any(b[0] == lid for b in world.buildings) or lid == "island":
            continue
        at = lmk["at"][:2]
        name = pretty(lid)
        if any(r["name"] == name and math.dist([r["center"][0] / 100, r["center"][1] / 100], at) < 20 for r in out):
            continue
        rad = POI_RADIUS.get(lid, 0.0)
        if not rad and max(lmk.get("size", [0, 0])) >= 10:
            rad = max(lmk["size"]) / 2.0 + 1.0
        if rad > 0:
            out.append(region(lid, name, "place", 2, True, at, at, icon_of(lid, "star"), 60, radius=rad))
        else:
            prio = 45 if lid in ("well", "gallows", "fountain", "sea_torii", "pagoda", "harbour_light", "windmill") else 30
            out.append(region(lid, name, "poi", -1, False, at, at, icon_of(lid, "star"), prio))
    for eid, ex in world.extras.items():
        if eid in used or any(r["id"] == eid for r in out):
            continue
        if "line" in ex:
            g = LineString(ex["line"]).buffer(2.4, cap_style=2)
            out.append(region(eid, pretty(eid), "street", 1, False, ex["at"], ex["at"], icon_of(eid), 40, polygon=g))
        else:
            out.append(region(eid, pretty(eid), "poi", -1, False, ex["at"], ex["at"], icon_of(eid, "star"), 45))

    # 1: streets, stairs, ramps (location line, small labels). Segments sharing a name merge into one street
    # (v2: Cat Steps + opes + Chapel Wynd + Chapel Steps = "The Long Ope").
    ways = {}
    for lane in L.get("lanes", []):
        line = lane_line(lane)
        if line is not None:
            ways.setdefault(pretty(lane["name"]), []).append((lane["name"], line, lane.get("width", 3.0), lane.get("kind", "lane")))
    for st in L.get("stairs", []):
        ways.setdefault(pretty(st["name"]), []).append((st["name"], LineString([st["from"], st["to"]]), st.get("width", 3.0), "stair"))
    for rp in L.get("ramps", []):
        if len(rp.get("points", [])) >= 2:
            ways.setdefault(pretty(rp["name"]), []).append((rp["name"], LineString(rp["points"]), rp.get("width", 3.0), "ramp"))
    map_labels = {re.sub(r"\s+z\s.*$", "", ml.get("text", "")).strip().lower(): ml["at"] for ml in L.get("map_labels", [])}
    taken = {r["name"] for r in out if r["layer"] >= 2}
    for name, segs in ways.items():
        g = unary_union([line.buffer(w / 2.0 + 1.0, 8) for _, line, w, _ in segs])
        longest = max(segs, key=lambda sg: sg[1].length)
        mid = longest[1].interpolate(0.5, normalized=True)
        label = map_labels.get(name.lower(), (mid.x, mid.y))
        total = sum(sg[1].length for sg in segs)
        kinds = {sg[3] for sg in segs}
        wmax = max(sg[2] for sg in segs)
        prio = 38 if kinds & {"main", "quay", "mole"} or wmax >= 4.0 else 32 if "ope" in kinds else 25 if total > 25 else 12
        if name in taken:
            prio = -1   # a place already carries this name
        out.append(region(longest[0], name, "street", 1, False, (mid.x, mid.y), label, "", prio, polygon=g))
    return out


# ================================================================================================= preview
ICON_COL = {"church": "#b69cff", "bell": "#b69cff", "grave": "#b69cff", "gallows": "#ff485e", "hall": "#ffb84d",
            "inn": "#ffb84d", "bread": "#ffb84d", "market": "#ffb84d", "notice": "#ffb84d", "fountain": "#6ee1eb",
            "well": "#6ee1eb", "koi": "#6ee1eb", "anchor": "#6ee1eb", "fish": "#6ee1eb", "boat": "#6ee1eb",
            "lighthouse": "#ffe096", "clock": "#ffb84d"}


def preview(img, bounds, regions, path):
    x0, y0, span = bounds
    im = img.convert("RGBA").resize((1400, 1400), Image.LANCZOS)
    k = 1400 / span
    d = ImageDraw.Draw(im)
    try:
        fb = ImageFont.truetype("arialbd.ttf", 15)
        fs = ImageFont.truetype("arial.ttf", 11)
        fd = ImageFont.truetype("arialbd.ttf", 20)
    except OSError:
        fb = fs = fd = ImageFont.load_default()
    # Same rule as AKGHUD's full map: highest priority first, skip a label that would overlap one already placed.
    placed = []
    for r in sorted(regions, key=lambda r: -r["priority"]):
        if r["priority"] < 0:
            continue
        lx, ly = (r["label"][0] / 100 - x0) * k, (r["label"][1] / 100 - y0) * k
        font = fd if r["layer"] == 0 else fs if r["layer"] == 1 else fb
        text = r["name"].upper() if r["layer"] == 0 else r["name"]
        tw = d.textlength(text, font=font)
        th = 22 if r["layer"] == 0 else 12 if r["layer"] == 1 else 16
        ty = ly + (11 if r["icon"] else -th / 2)
        box = (lx - tw / 2 - 3, ty - 2 - (18 if r["icon"] else 0), lx + tw / 2 + 3, ty + th + 2)
        if any(not (box[2] < b[0] or box[0] > b[2] or box[3] < b[1] or box[1] > b[3]) for b in placed):
            if r["icon"]:
                d.ellipse([lx - 5, ly - 5, lx + 5, ly + 5], fill=ICON_COL.get(r["icon"], "#fff4de"), outline="#281f3a", width=2)
            continue
        placed.append(box)
        if r["icon"]:
            d.ellipse([lx - 8, ly - 8, lx + 8, ly + 8], fill=ICON_COL.get(r["icon"], "#fff4de"), outline="#281f3a", width=2)
        d.text((lx - tw / 2, ty), text, font=font, fill="#fff4de" if r["layer"] != 1 else "#3b2f25",
               stroke_width=3 if r["layer"] != 1 else 0, stroke_fill="#281f3a")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.convert("RGB").save(path)


# ================================================================================================= main
def rel(path):
    try:
        return os.path.relpath(os.path.abspath(path), ROOT).replace("\\", "/")
    except ValueError:   # other drive
        return os.path.abspath(path).replace("\\", "/")


def map_key(layout_path):
    """morrowmere_layout.json -> Morrowmere, morrowmere_layout_v2.json -> Morrowmere_v2 (asset + file suffix)."""
    stem = os.path.splitext(os.path.basename(layout_path))[0]
    m = re.match(r"(\w+?)_layout(_\w+)?$", stem)
    base, suffix = (m.group(1), m.group(2) or "") if m else (stem, "")
    return base[:1].upper() + base[1:] + suffix


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("layout", nargs="?", default=DEFAULT_LAYOUT)
    ap.add_argument("--out", default=OUT_DIR)
    ap.add_argument("--size", type=int, default=2048)
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--trees", default=None, help="tree dump from the level (kg_make_minimap.py); default: "
                    "Saved/Minimap/trees_<Key>.json when present")
    a = ap.parse_args()
    L = json.load(open(a.layout, encoding="utf-8"))
    world = World(L, a.layout)
    _ACTIVE.clear()
    if not world.v2:
        _ACTIVE.update(V1_NAMES)
    trees_path = a.trees or os.path.join(ROOT, "Saved", "Minimap", f"trees_{map_key(a.layout)}.json")
    trees = json.load(open(trees_path)) if os.path.exists(trees_path) else None
    img, bounds = render(world, a.size, trees)
    regions = build_regions(world)
    os.makedirs(a.out, exist_ok=True)
    key = map_key(a.layout)
    png = os.path.join(a.out, f"T_KG_Map_{key}.png")
    img.save(png, optimize=True)
    x0, y0, span = bounds
    data = {
        "_doc": "Generated by Tools/Level/render_minimap.py - do not edit. Units: cm (UE). Image: north (-Y) up, "
                "u = (x - world_min.x) / (world_max.x - world_min.x), v likewise with y.",
        "key": key,
        "layout": rel(a.layout),
        "png": rel(png),
        "size": a.size,
        "world_min": [round(x0 * 100.0, 1), round(y0 * 100.0, 1)],
        "world_max": [round((x0 + span) * 100.0, 1), round((y0 + span) * 100.0, 1)],
        "regions": regions,
    }
    rj = os.path.join(a.out, f"KG_MapRegions_{key}.json")
    json.dump(data, open(rj, "w", encoding="utf-8"), indent=1)
    print(f"KG_MINIMAP {png} {a.size}px  bounds x[{x0}, {x0 + span}] y[{y0}, {y0 + span}] m  regions {len(regions)} -> {rj}")
    if a.preview:
        pv = os.path.join(ROOT, "Saved", "Minimap", f"preview_{key}.png")
        preview(img, bounds, regions, pv)
        print(f"KG_MINIMAP preview {pv}")


if __name__ == "__main__":
    main()
