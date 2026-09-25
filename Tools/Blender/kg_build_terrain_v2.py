"""Morrowmere v2 terrain ("The Amphitheatre Cove") from Tools/Level/morrowmere_layout_v2.json.

  blender --background --factory-startup --python Tools/Blender/kg_build_terrain_v2.py -- Art/Packed/KG_Terrain_v2.glb
  python Tools/Blender/kg_build_terrain_v2.py --preview Docs/Level/v2_terrain_preview.png     (no Blender: numpy only)

Plan: Docs/Level/Morrowmere_v2_Plan.md section 11.1. All maths in UE metres (x east, +y toward the sea, z up);
Blender gets (x, -y) at the very end. Two grids sample the same height():
  - core  x -130..130, y -125..125 (UE), 0.5 m step, split in 2 x 2 tiles  (SM_KG_TerrainV2_Core_<i><j>)
  - outer 640 m at 2.5 m step, faces fully inside the core rectangle dropped (SM_KG_TerrainV2_Outer)
Core seam vertices are re-interpolated from the 2.5 m samples, so the two grids meet without cracks.

Height priority (first match wins): kit stairs (tread line - 5 cm) and ramps (linear) > brook bed / pond > building
pads on slope land > flat terraces > slope terraces (profile + mounds + gentle noise, blended toward flat neighbours)
> natural land (meadow + mountain ring, blended toward terraces) ; sea (outside coast.land_polygon), the round basin
and the mole override below the land.
Hard edges: terrain vertices on the HIGH side of a retaining wall / quay edge / cliff line within one cell are moved
onto the line, so the step is a vertical face exactly on the boundary; the kit wall (0.4 m, back face on the line)
covers the low side. Stair and ramp corridors are never snapped.

Outputs: <glb>, <glb>_heights.json  {size, step, n, heights (outer, v1 layout: row j = Blender y),
         core: {x0, y0, step, nx, ny, heights}}  core rows are UE y: h[j*nx + i] at (x0 + i*step, y0 + j*step) metres.
"""
import json
import math
import os
import sys
import time

import numpy as np

try:
    import bpy  # noqa: F401
except ImportError:
    bpy = None

HERE = os.path.dirname(os.path.abspath(__file__))
LAYOUT_FILE = os.path.join(HERE, "..", "Level", "morrowmere_layout_v2.json")
L = json.load(open(LAYOUT_FILE, encoding="utf-8"))

CORE = (-130.0, -125.0, 130.0, 125.0)   # UE metres, multiples of 2.5
CORE_STEP = 0.5
OUTER_SIZE, OUTER_STEP = 640.0, 2.5
RISER_RUN = 2.08

COBBLE = (0.58, 0.55, 0.50)
FLAGS = (0.63, 0.60, 0.55)
SAND = (0.93, 0.80, 0.52)
WET_SAND = (0.70, 0.60, 0.42)
GRASS_A = (0.33, 0.62, 0.22)
GRASS_B = (0.46, 0.70, 0.26)
GRASS_SHORT = (0.42, 0.66, 0.25)
PATH = (0.62, 0.47, 0.30)
GRAVEL = (0.74, 0.70, 0.62)
ROCK = (0.52, 0.49, 0.46)
ROCK_DARK = (0.38, 0.36, 0.35)


# ================================================================================================ numpy helpers
def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _hash(ix, iy, seed):
    h = (ix.astype(np.int64) * 374761393 + iy.astype(np.int64) * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float64) / float(0xFFFFFF) * 2.0 - 1.0


def vnoise(x, y, seed):
    ix, iy = np.floor(x), np.floor(y)
    fx, fy = x - ix, y - iy
    ux, uy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _hash(ix, iy, seed), _hash(ix + 1, iy, seed)
    c, d = _hash(ix, iy + 1, seed), _hash(ix + 1, iy + 1, seed)
    return (a + (b - a) * ux) * (1 - uy) + (c + (d - c) * ux) * uy


def fbm(x, y, scale, octaves=4, seed=0):
    tot, amp, freq, norm = 0.0, 1.0, 1.0 / scale, 0.0
    for o in range(octaves):
        tot = tot + amp * vnoise(x * freq, y * freq, seed + 17 * o)
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return tot / norm


def seg_proj(px, py, ax, ay, bx, by):
    """Distance to segment AB and the clamped parameter t (vectorised over points)."""
    dx, dy = bx - ax, by - ay
    ll = max(1e-12, dx * dx + dy * dy)
    t = np.clip(((px - ax) * dx + (py - ay) * dy) / ll, 0.0, 1.0)
    qx, qy = ax + t * dx, ay + t * dy
    return np.hypot(px - qx, py - qy), t


def polyline_info(px, py, pts):
    """min distance, arclength at the foot point, foot point x/y."""
    best = np.full(px.shape, 1e9)
    s_at = np.zeros(px.shape)
    fx, fy = np.zeros(px.shape), np.zeros(px.shape)
    acc = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        d, t = seg_proj(px, py, ax, ay, bx, by)
        seg = math.hypot(bx - ax, by - ay)
        m = d < best
        best = np.where(m, d, best)
        s_at = np.where(m, acc + t * seg, s_at)
        fx = np.where(m, ax + t * (bx - ax), fx)
        fy = np.where(m, ay + t * (by - ay), fy)
        acc += seg
    return best, s_at, fx, fy, acc


def polyline_dist(px, py, pts, closed=False):
    pts = list(pts) + ([pts[0]] if closed else [])
    best = np.full(px.shape, 1e9)
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        d, _ = seg_proj(px, py, ax, ay, bx, by)
        best = np.minimum(best, d)
    return best


def pip(px, py, poly):
    """Even-odd point in polygon (vectorised)."""
    inside = np.zeros(px.shape, dtype=bool)
    n = len(poly)
    for k in range(n):
        x1, y1 = poly[k]
        x2, y2 = poly[(k + 1) % n]
        if y1 == y2:
            continue
        cond = (y1 > py) != (y2 > py)
        xint = x1 + (py - y1) * (x2 - x1) / (y2 - y1)
        inside ^= cond & (px < xint)
    return inside


def bbox_mask(px, py, poly, pad):
    xs, ys = [p[0] for p in poly], [p[1] for p in poly]
    return (px >= min(xs) - pad) & (px <= max(xs) + pad) & (py >= min(ys) - pad) & (py <= max(ys) + pad)


def prof(pr, px, py):
    s = (px - pr["origin"][0]) * pr["dir"][0] + (py - pr["origin"][1]) * pr["dir"][1]
    st = pr["stops"]
    return np.interp(s, [a for a, _ in st], [b for _, b in st])


def rect_poly(b, pad=0.0):
    w, d = b["size"]
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    ux, uy = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
    cx, cy = b["at"]
    hw, hd = w / 2.0 + pad, d / 2.0 + pad
    return [(cx + sx * hw * ux[0] + sy * hd * uy[0], cy + sx * hw * ux[1] + sy * hd * uy[1])
            for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]


# ================================================================================================ stair profile
def stair_profile(s):
    """(a, b, length, u, profile(t_along)) for one kit stair: tread line from the LOW end (metres along)."""
    fa, fb = s["from"], s["to"]
    za, zb = s["z0"], s["z1"]
    lo, hi, zlo, zhi = (fa, fb, za, zb) if za < zb else (fb, fa, zb, za)
    length = math.dist(lo, hi)
    u = ((hi[0] - lo[0]) / length, (hi[1] - lo[1]) / length)
    n = int(round(zhi - zlo))
    nl = s.get("landings", 0)
    flights = [n // (nl + 1) + (1 if k < n % (nl + 1) else 0) for k in range(nl + 1)]
    spare = max(0.0, length - n * RISER_RUN)
    land = spare / nl if nl else 0.0
    lead = 0.0 if nl else spare / 2.0
    # piecewise-linear knots (s, z) of the tread line (inner corners of the steps)
    knots = [(0.0, zlo)]
    sc, zc = lead, zlo
    if lead > 0:
        knots.append((sc, zc))
    risers = []   # (s_start, z_base) of every kit riser piece
    for k, f in enumerate(flights):
        for r in range(f):
            risers.append((sc, zc))
            sc += RISER_RUN
            zc += 1.0
            knots.append((sc, zc))
        if k < len(flights) - 1:
            sc += land
            knots.append((sc, zc))
    knots.append((length, zhi))
    return dict(lo=lo, hi=hi, zlo=zlo, zhi=zhi, length=length, u=u, knots=knots, risers=risers, land=land,
                flights=flights)


# ================================================================================================ height field
class Field:
    def __init__(self, px, py):
        self.px, self.py = px, py

    def build(self):
        t0 = time.time()
        px, py = self.px, self.py
        shape = px.shape
        z = self.natural()
        kind = np.zeros(shape, dtype=np.int8)   # 0 natural, 1 flat terrace, 2 slope terrace, 3 sea, 4 stair/ramp, 5 stream
        tid = np.full(shape, -1, dtype=np.int16)
        land = pip(px, py, L["coast"]["land_polygon"])
        self.land = land

        # --- terraces
        flat_z = np.full(shape, np.nan)
        terr_z = np.full(shape, np.nan)
        for k, t in enumerate(L["terraces"]):
            bb = bbox_mask(px, py, t["polygon"], 0.0)
            ins = np.zeros(shape, dtype=bool)
            ins[bb] = pip(px[bb], py[bb], t["polygon"])
            ins &= tid < 0
            tid[ins] = k
            if t.get("z") is not None:
                terr_z[ins] = t["z"]
                flat_z[ins] = t["z"]
                kind[ins] = 1
            else:
                zz = prof(t["z_profile"], px[ins], py[ins])
                terr_z[ins] = zz
                kind[ins] = 2
        self.tid = tid
        # mounds (on slope land only)
        for m in L.get("mounds", []):
            r = np.hypot(px - m["center"][0], py - m["center"][1])
            bump = m["height"] * (1.0 - smooth(0.0, m["radius"], r))
            sel = kind == 2
            terr_z[sel] += bump[sel]
        # gentle rolling noise on slope land, faded near lanes / buildings (added after the flat blend)
        slope_noise = 0.7 * fbm(px + 40.0, py - 70.0, 22.0, 3, seed=5)

        # --- distances to flat terraces (for blending slope + natural land) and to walls
        wall_lines = [w["points"] for w in L["retaining_walls"] if w["top_z"] != w["bottom_z"]]
        d_wall = np.full(shape, 1e9)
        for pts in wall_lines:
            bb = bbox_mask(px, py, pts, 4.0)
            d_wall[bb] = np.minimum(d_wall[bb], polyline_dist(px[bb], py[bb], pts))
        # inverse-distance weighted flat neighbour height (continuous where two flat terraces are close)
        near_flat_w = np.zeros(shape)
        acc_w = np.zeros(shape)
        acc_z = np.zeros(shape)
        for t in L["terraces"]:
            if t.get("z") is None:
                continue
            soft = t.get("soft_m", 8.0)
            bb = bbox_mask(px, py, t["polygon"], soft)
            d = polyline_dist(px[bb], py[bb], t["polygon"], closed=True)
            w = 1.0 - smooth(0.0, soft, d)
            wi = w / (d + 0.5) ** 2
            acc_w[bb] += wi
            acc_z[bb] += wi * t["z"]
            near_flat_w[bb] = np.maximum(near_flat_w[bb], w)
        near_flat_z = np.where(acc_w > 0, acc_z / np.maximum(acc_w, 1e-12), 0.0)
        # never blend across a retaining wall (the step there is architecture)
        near_flat_w *= smooth(1.0, 3.0, d_wall)

        # slope terraces: blend toward the flat neighbour; then the noise
        sel = kind == 2
        zz = terr_z[sel]
        zz = zz + (near_flat_z[sel] - zz) * near_flat_w[sel]
        terr_z[sel] = zz

        # --- natural land: blend toward the nearest terrace (distance to the terrace union)
        nat = kind == 0
        R = 45.0
        acc_w = np.zeros(shape)
        acc_z = np.zeros(shape)
        w_nat = np.zeros(shape)
        for t in L["terraces"]:
            bb = bbox_mask(px, py, t["polygon"], R) & nat
            if not bb.any():
                continue
            d = polyline_dist(px[bb], py[bb], t["polygon"], closed=True)
            if t.get("z") is not None:
                tz = np.full(d.shape, float(t["z"]))
            else:
                tz = prof(t["z_profile"], px[bb], py[bb])
            w = 1.0 - smooth(0.0, R, d)
            wi = w / (d + 0.5) ** 2
            acc_w[bb] += wi
            acc_z[bb] += wi * tz
            w_nat[bb] = np.maximum(w_nat[bb], w)
        z_terr_near = np.where(acc_w > 0, acc_z / np.maximum(acc_w, 1e-12), z)
        z = np.where(nat, z + (z_terr_near - z) * w_nat, z)
        z = np.where(kind > 0, terr_z, z)

        # --- building pads on slope / natural land (flat terraces are already at pad height)
        pad_w = np.zeros(shape)
        pad_z = np.zeros(shape)
        bld = [b for b in L["houses"] + L["infill"]] + [b for b in L["landmarks"].values()
                                                       if "size" in b and not b.get("is_prop")]
        for b in bld:
            t = next((t for t in L["terraces"] if t["name"] == b.get("terrace")), None)
            if t is not None and t.get("z") is not None:
                continue
            poly = rect_poly(b, 1.0)
            bb = bbox_mask(px, py, poly, 4.0)
            ins = pip(px[bb], py[bb], poly)
            d = np.where(ins, 0.0, polyline_dist(px[bb], py[bb], poly, closed=True))
            w = 1.0 - smooth(0.0, 4.0, d)
            cur = pad_w[bb]
            upd = w > cur
            pz = pad_z[bb]
            pz[upd] = b["z"]
            pad_z[bb] = pz
            pad_w[bb] = np.maximum(cur, w)
        soft_land = kind != 1
        z = np.where(soft_land & (pad_w > 0), z + (pad_z - z) * pad_w, z)

        # noise on slope land away from lanes and pads
        lane_d = np.full(shape, 1e9)
        for l in L["lanes"]:
            bb = bbox_mask(px, py, l["points"], 8.0)
            lane_d[bb] = np.minimum(lane_d[bb], polyline_dist(px[bb], py[bb], l["points"]) - l["width"] / 2.0)
        self.lane_d = lane_d
        quiet = smooth(1.0, 6.0, lane_d) * (1.0 - pad_w) * (1.0 - near_flat_w)
        z = np.where((kind == 2) | (kind == 0), z + slope_noise * quiet, z)

        # --- brook: bed + banks, mill pond
        st = L["stream"]
        spts = [p[:2] for p in st["points"]]
        sz = [p[2] for p in st["points"]]
        bb = bbox_mask(px, py, spts, st["width"] / 2.0 + 3.0)
        d, s_at, _, _, _ = polyline_info(px[bb], py[bb], spts)
        cum = [0.0]
        for a, b in zip(spts, spts[1:]):
            cum.append(cum[-1] + math.dist(a, b))
        bed = np.interp(s_at, cum, sz)
        half = st["width"] / 2.0
        k = 1.0 - smooth(half, half + 2.5, d)
        zb = z[bb]
        zb = zb + (bed - zb) * k
        z[bb] = zb
        kb = kind[bb]
        kb[d < half + 0.4] = 5
        kind[bb] = kb
        self.stream_d = np.full(shape, 1e9)
        self.stream_d[bb] = d
        for p in st.get("ponds", []):
            r = np.hypot(px - p["center"][0], py - p["center"][1])
            k = 1.0 - smooth(p["radius"], p["radius"] + 2.5, r)
            z = z + ((p["z"] - 0.8) - z) * k
            kind[r < p["radius"] + 0.3] = 5

        # --- ramps (linear, 3 m soft shoulder on soft land) and kit stairs (tread line), highest priority
        corridor = np.zeros(shape, dtype=bool)
        for r in L["ramps"]:
            pts = r["points"]
            hw = r["width"] / 2.0
            bb = bbox_mask(px, py, pts, hw + 4.0)
            d, s_at, _, _, total = polyline_info(px[bb], py[bb], pts)
            rz = r["z0"] + (r["z1"] - r["z0"]) * np.clip(s_at / total, 0, 1)
            zb, kb, cb = z[bb], kind[bb], corridor[bb]
            core_sel = d <= hw + 0.3
            shoulder = (d > hw + 0.3) & (d < hw + 3.3) & (kb != 1)
            ws = 1.0 - smooth(hw + 0.3, hw + 3.3, d)
            zb = np.where(shoulder, zb + (rz - zb) * ws, zb)
            zb = np.where(core_sel, rz, zb)
            kb[core_sel] = 4
            cb |= core_sel
            z[bb], kind[bb], corridor[bb] = zb, kb, cb
        self.stair_info = []
        for s in L["stairs"]:
            P = stair_profile(s)
            lo, u, length = P["lo"], P["u"], P["length"]
            hw = s["width"] / 2.0 + 0.3
            bb = bbox_mask(px, py, [P["lo"], P["hi"]], hw + 1.0)
            ax = (px[bb] - lo[0]) * u[0] + (py[bb] - lo[1]) * u[1]
            ay = -(px[bb] - lo[0]) * u[1] + (py[bb] - lo[1]) * u[0]
            sel = (np.abs(ay) <= hw) & (ax >= -0.3) & (ax <= length + 0.3)
            ks = [k[0] for k in P["knots"]]
            kz = [k[1] for k in P["knots"]]
            tz = np.interp(ax, ks, kz) - 0.05
            zb, kb, cb = z[bb], kind[bb], corridor[bb]
            zb = np.where(sel, tz, zb)
            kb[sel] = 4
            cb |= sel
            z[bb], kind[bb], corridor[bb] = zb, kb, cb
            self.stair_info.append((s["name"], P))

        # --- sea, basin, mole
        coast = L["coast"]
        sea = ~land
        cpts = coast["land_polygon"]
        d_coast = polyline_dist(px, py, cpts, closed=True)
        hard_lines = [L["quay"]["edge"]] + [c["points"] for c in L["cliffs"]]
        d_hard = np.full(shape, 1e9)
        for pts in hard_lines:
            d_hard = np.minimum(d_hard, polyline_dist(px, py, pts))
        sf = coast["sea_floor"]
        deep = sf["near_z"] + (sf["far_z"] - sf["near_z"]) * np.clip(d_coast / sf["far_at_m"], 0, 1)
        beachy = smooth(0.0, 3.0, d_hard - d_coast)             # 1 where the nearest shore is a beach
        shelf = 0.3 + (deep - 0.3) * smooth(0.0, 14.0, d_coast)
        zsea = deep + (shelf - deep) * beachy
        bs = L["basin"]
        rb = np.hypot(px - bs["center"][0], py - bs["center"][1])
        zsea = np.where(rb <= bs["radius"], np.minimum(zsea, bs["floor_z"]), zsea)
        # mole: 3.5 m walk at 2.5, 1:1 armour down to the sea bed
        mo = L["mole"]
        bb = bbox_mask(px, py, mo["points"], mo["width"] + 6.0)
        dm = np.full(shape, 1e9)
        sm = np.zeros(shape)
        dmb, smb, _, _, _ = polyline_info(px[bb], py[bb], mo["points"])
        dm[bb], sm[bb] = dmb, smb
        # the walk leaves the quay (2.0) and rises to top_z over 6 m: no ledge for the nav agent at the root
        top = L["quay"]["top_z"] + (mo["top_z"] - L["quay"]["top_z"]) * smooth(0.0, 6.0, sm)
        zm = top - np.maximum(0.0, dm - mo["walk_width"] / 2.0) * 1.0
        zsea = np.maximum(zsea, np.where(dm < 12.0, zm, -99.0))
        self.mole_d = dm
        z = np.where(sea, zsea, z)
        kind[sea] = 3
        # beach: natural land within 6 m of a beach shore
        beach = land & (kind == 0) & (d_coast < 6.0)
        kb = smooth(0.0, 6.0, d_coast)
        z = np.where(beach, coast["beach_z"] + (z - coast["beach_z"]) * kb, z)
        self.d_coast, self.d_hard = d_coast, d_hard

        # --- hard-edge snapping: vertices on the high side within one cell move onto the line
        snap_x, snap_y = px.copy(), py.copy()
        lines = [(w["points"], (w["top_z"] + w["bottom_z"]) / 2.0) for w in L["retaining_walls"] if w["top_z"] != w["bottom_z"]]
        lines += [(L["quay"]["edge"], -0.5)]
        lines += [(c["points"], 0.0 if c["bottom_z"] < 0 else 1.0 + c["bottom_z"]) for c in L["cliffs"]]
        cell = self.cell
        moved = np.zeros(shape, dtype=bool)
        for pts, mid in lines:
            bb = bbox_mask(px, py, pts, cell + 0.1)
            d, _, fx, fy, _ = polyline_info(px[bb], py[bb], pts)
            m = (d <= cell * 0.999) & (z[bb] > mid) & ~corridor[bb] & ~moved[bb]
            sx, sy, mv = snap_x[bb], snap_y[bb], moved[bb]
            sx[m], sy[m] = fx[m], fy[m]
            mv |= m
            snap_x[bb], snap_y[bb], moved[bb] = sx, sy, mv
        self.snap_x, self.snap_y = snap_x, snap_y
        self.z, self.kind, self.corridor = z, kind, corridor
        self.d_wall = d_wall
        print(f"KG_TERRAIN_V2 field {px.size} pts in {time.time() - t0:.1f}s, snapped {int(moved.sum())}")
        return z

    def natural(self):
        """Meadow + mountain ring (v1 recipe, no plateau pull), open toward the sea (+y)."""
        px, py = self.px, self.py
        r = np.hypot(px, py)
        h = 4.0 + 0.5 * fbm(px, py, 40.0, 4, seed=1)
        h = h + smooth(45, 110, r) * (3.0 + 5.0 * fbm(px + 300, py, 60.0, 4, seed=2))
        open_south = smooth(50, 110, py) * (1.0 - smooth(90, 160, np.abs(px)))
        ring = smooth(120, 210, r) * (1.0 - open_south)
        h = h + ring * (38.0 + 34.0 * fbm(px, py + 500, 90.0, 5, seed=3))
        # raise the rim toward the grid edge on the land sides so no sea shows past the mountains
        rim = smooth(235.0, 318.0, np.maximum(np.abs(px), -py)) * (1.0 - smooth(40.0, 90.0, py))
        h = h + rim * (30.0 + 12.0 * fbm(px - 70, py, 50.0, 3, seed=4))
        return h

    def colours(self):
        px, py, z, kind, tid = self.px, self.py, self.z, self.kind, self.tid
        shape = z.shape
        g = 0.5 + 0.5 * fbm(px + 90, py - 40, 18.0, 3, seed=9)
        c = np.stack([GRASS_A[i] + (GRASS_B[i] - GRASS_A[i]) * g for i in range(3)], axis=-1)

        def paint(mask, col, k=1.0):
            if np.isscalar(k):
                kk = np.full(shape, k)
            else:
                kk = k
            for i in range(3):
                c[..., i] = np.where(mask, c[..., i] + (col[i] - c[..., i]) * kk, c[..., i])

        names = [t["name"] for t in L["terraces"]]
        tsurf = {k: t.get("surface") for k, t in enumerate(L["terraces"])}
        flat = kind == 1
        paint(flat, GRASS_SHORT, 0.6)
        # walkways: lanes (true width), squares, stairs, ramps
        walk = self.lane_d <= 0.0
        onw = np.zeros(shape, dtype=bool)
        for q in L["squares"]:
            bb = bbox_mask(px, py, q["polygon"], 0.0)
            m = np.zeros(shape, dtype=bool)
            m[bb] = pip(px[bb], py[bb], q["polygon"])
            onw |= m
        walk |= onw
        lane_surface = np.full(shape, "", dtype=object)
        for l in L["lanes"]:
            bb = bbox_mask(px, py, l["points"], l["width"])
            m = np.zeros(shape, dtype=bool)
            m[bb] = polyline_dist(px[bb], py[bb], l["points"]) <= l["width"] / 2.0
            lane_surface[m] = l.get("surface", "dirt")
        cob = walk & (flat | (np.isin(lane_surface, ["cobble", "stone_flags"])))
        paint(cob, COBBLE, 0.9)
        dirt = walk & ~cob & np.isin(lane_surface, ["dirt"])
        paint(dirt, PATH, 0.85)
        grav = walk & ~cob & np.isin(lane_surface, ["gravel"])
        paint(grav, GRAVEL, 0.85)
        # garden terrace: gravel; quay + mole + stairs: stone flags
        for k, s in tsurf.items():
            if s == "stone_flags":
                paint(tid == k, FLAGS, 0.9)
            elif s == "gravel":
                paint((tid == k) & walk, GRAVEL, 0.9)
        paint(self.corridor, FLAGS, 0.9)
        # sea bed / beach / stream
        sea = kind == 3
        sand = (z < 1.4) & ~flat & ~self.corridor
        paint(sand, SAND, smooth(1.4, 0.4, z))
        paint(sea, WET_SAND, 1.0)
        paint(sea & (z < -4.0), (0.55, 0.52, 0.40), smooth(-4.0, -9.0, z))
        paint(self.stream_d < 2.2, WET_SAND, 1.0 - smooth(1.6, 2.2, self.stream_d))
        paint(self.mole_d < 1.9, FLAGS, 1.0)
        # slopes + cliffs: rock
        step = self.cell
        zx = np.gradient(z, step, axis=1)
        zy = np.gradient(z, step, axis=0)
        slope = np.hypot(zx, zy)
        rock = np.clip(smooth(0.9, 1.6, slope) + smooth(40.0, 60.0, z), 0, 1)
        rock = np.where(self.corridor | (self.d_wall < 1.2), 0.0, rock)
        armour = smooth(1.75, 2.1, self.mole_d) * (1.0 - smooth(7.0, 9.0, self.mole_d)) * (z > -3.2)
        rock = np.maximum(rock, armour)
        rc = np.stack([ROCK[i] + (ROCK_DARK[i] - ROCK[i]) * g for i in range(3)], axis=-1)
        c = c + (rc - c) * rock[..., None]
        return np.clip(c, 0, 1)


def core_grid():
    x0, y0, x1, y1 = CORE
    nx = int(round((x1 - x0) / CORE_STEP)) + 1
    ny = int(round((y1 - y0) / CORE_STEP)) + 1
    xs = x0 + np.arange(nx) * CORE_STEP
    ys = y0 + np.arange(ny) * CORE_STEP
    px, py = np.meshgrid(xs, ys)
    return px, py


def outer_grid():
    n = int(OUTER_SIZE / OUTER_STEP)
    half = OUTER_SIZE / 2
    xs = -half + np.arange(n + 1) * OUTER_STEP
    bys = -half + np.arange(n + 1) * OUTER_STEP          # Blender y (row j), UE y = -by
    px, pby = np.meshgrid(xs, bys)
    return px, -pby, n


def fix_seam(zc, step_ratio):
    """Core border vertices between the 2.5 m knots become linear, matching the outer grid edges."""
    for row in (0, -1):
        v = zc[row, :]
        idx = np.arange(v.size)
        knots = idx[::step_ratio]
        zc[row, :] = np.interp(idx, knots, v[knots])
    for col in (0, -1):
        v = zc[:, col]
        idx = np.arange(v.size)
        knots = idx[::step_ratio]
        zc[:, col] = np.interp(idx, knots, v[knots])
    return zc


def compute():
    px, py = core_grid()
    F = Field(px, py)
    F.cell = CORE_STEP
    F.build()
    col = F.colours()
    F.z = fix_seam(F.z, int(round(OUTER_STEP / CORE_STEP)))
    ox, oy, n = outer_grid()
    G = Field(ox, oy)
    G.cell = OUTER_STEP
    G.build()
    gcol = G.colours()
    # the outer grid must agree with the core at the shared knots
    return F, col, G, gcol, n


def log_stairs(F):
    """Terrace z at every stair end (must equal z0/z1)."""
    x0, y0 = CORE[0], CORE[1]
    for name, P in F.stair_info:
        for p, want in ((P["lo"], P["zlo"]), (P["hi"], P["zhi"])):
            i, j = int(round((p[0] - x0) / CORE_STEP)), int(round((p[1] - y0) / CORE_STEP))
            u = P["u"]
            # sample 1 m outside the stair end, on the terrace
            sgn = -1 if p is P["lo"] else 1
            qx, qy = p[0] + sgn * u[0] * 1.0, p[1] + sgn * u[1] * 1.0
            qi, qj = int(round((qx - x0) / CORE_STEP)), int(round((qy - y0) / CORE_STEP))
            print(f"KG_TERRAIN stair {name}: end z {F.z[j, i]:.2f} / beyond {F.z[qj, qi]:.2f} want {want:.2f}")


# ================================================================================================ outputs
def write_heights(path, F, G, n):
    core = {"x0": CORE[0], "y0": CORE[1], "step": CORE_STEP, "nx": F.z.shape[1], "ny": F.z.shape[0],
            "heights": [round(float(v), 3) for v in F.z.ravel()]}
    out = {"size": OUTER_SIZE, "step": OUTER_STEP, "n": n, "heights": [round(float(v), 3) for v in G.z.ravel()],
           "core": core, "frame": "outer rows = Blender y (v1 layout); core rows = UE y metres"}
    with open(path, "w") as f:
        json.dump(out, f)


def preview(path, F, col):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    z = F.z
    gx = np.gradient(z, CORE_STEP, axis=1)
    gy = np.gradient(z, CORE_STEP, axis=0)
    # sun from the north-west (UE: -x, -y), UE y grows downward in the image like the plan
    lx, ly, lz = -0.6, -0.6, 0.55
    nrm = np.sqrt(gx * gx + gy * gy + 1.0)
    shade = np.clip((-gx * lx - gy * ly + lz) / nrm / math.sqrt(lx * lx + ly * ly + lz * lz), 0, 1)
    img = col * (0.35 + 0.75 * shade[..., None])
    water = z < 0.0
    img[water] = img[water] * 0.4 + np.array([0.15, 0.45, 0.6]) * 0.6
    fig = plt.figure(figsize=(13, 12.5), dpi=110)
    ax = fig.add_axes([0.04, 0.04, 0.94, 0.93])
    ax.imshow(np.clip(img, 0, 1), extent=(CORE[0], CORE[2], CORE[3], CORE[1]), interpolation="bilinear")
    cs = ax.contour(F.px, F.py, z, levels=[0, 2, 5, 8, 11, 14, 18, 24], colors="k", linewidths=0.4, alpha=0.5)
    ax.clabel(cs, fontsize=6, fmt="%g")
    for w in L["retaining_walls"]:
        xs, ys = zip(*w["points"])
        ax.plot(xs, ys, color="#222", lw=1.6)
    ax.set_title("Morrowmere v2 terrain (core 0.5 m) - Tools/Blender/kg_build_terrain_v2.py")
    ax.set_xlim(CORE[0], CORE[2])
    ax.set_ylim(CORE[3], CORE[1])
    fig.savefig(path)
    print("KG_TERRAIN_V2 preview ->", path)


def build_blender(out, F, col, G, gcol, n):
    import bpy
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    mat = bpy.data.materials.new("M_KG_TerrainVC")
    mat.use_nodes = True
    nt = mat.node_tree
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    bsdf = next(nd for nd in nt.nodes if nd.type == "BSDF_PRINCIPLED")
    nt.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])

    def make(name, vx, vy, vz, cols, face_mask=None):
        ny, nx = vz.shape
        verts = np.stack([vx.ravel(), -vy.ravel(), vz.ravel()], axis=-1)   # Blender (x, -y)
        jj, ii = np.meshgrid(np.arange(ny - 1), np.arange(nx - 1), indexing="ij")
        a = (jj * nx + ii).ravel()
        # CCW seen from +Z whatever the row direction: the core rows run along UE +y (Blender -y), the outer rows
        # along Blender +y, so the winding flips with the sign of the row step.
        if vy[-1, 0] > vy[0, 0]:
            quads = np.stack([a, a + nx, a + nx + 1, a + 1], axis=-1)
        else:
            quads = np.stack([a, a + 1, a + nx + 1, a + nx], axis=-1)
        if face_mask is not None:
            quads = quads[face_mask.ravel()]
        used = np.unique(quads)
        remap = np.full(verts.shape[0], -1, dtype=np.int64)
        remap[used] = np.arange(used.size)
        verts = verts[used]
        quads = remap[quads]
        me = bpy.data.meshes.new(name)
        me.vertices.add(verts.shape[0])
        me.vertices.foreach_set("co", verts.astype(np.float32).ravel())
        nq = quads.shape[0]
        me.loops.add(nq * 4)
        me.loops.foreach_set("vertex_index", quads.astype(np.int32).ravel())
        me.polygons.add(nq)
        me.polygons.foreach_set("loop_start", (np.arange(nq) * 4).astype(np.int32))
        me.polygons.foreach_set("loop_total", np.full(nq, 4, dtype=np.int32))
        me.update(calc_edges=True)
        me.validate()
        cc = cols.reshape(-1, 3)[used]
        ca = me.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        rgba = np.concatenate([cc, np.ones((cc.shape[0], 1))], axis=1).astype(np.float32)
        ca.data.foreach_set("color", rgba.ravel())
        me.color_attributes.active_color = ca
        me.polygons.foreach_set("use_smooth", np.ones(nq, dtype=bool))
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        print(f"KG_TERRAIN_V2 {name}: {verts.shape[0]} verts {nq} quads")
        return ob

    # core in 2 x 2 tiles sharing their seam rows
    ny, nx = F.z.shape
    mx, my = nx // 2, ny // 2
    for tj, (ja, jb) in enumerate(((0, my + 1), (my, ny))):
        for ti, (ia, ib) in enumerate(((0, mx + 1), (mx, nx))):
            make(f"SM_KG_TerrainV2_Core_{tj}{ti}", F.snap_x[ja:jb, ia:ib], F.snap_y[ja:jb, ia:ib], F.z[ja:jb, ia:ib],
                 col[ja:jb, ia:ib])
    # outer: drop faces fully inside the core rectangle
    ox, oy = G.px, G.py
    cx = (ox[:-1, :-1] + ox[1:, 1:]) / 2.0
    cy = (oy[:-1, :-1] + oy[1:, 1:]) / 2.0
    keep = ~((cx > CORE[0]) & (cx < CORE[2]) & (cy > CORE[1]) & (cy < CORE[3]))
    make("SM_KG_TerrainV2_Outer", G.px, G.py, G.z, gcol, keep)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True)
    print(f"KG_TERRAIN_V2: -> {out}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    F, col, G, gcol, n = compute()
    log_stairs(F)
    zc = F.z
    print(f"KG_TERRAIN_V2 core z {zc.min():.1f}..{zc.max():.1f}, outer {G.z.min():.1f}..{G.z.max():.1f}")
    if argv and argv[0] == "--preview":
        preview(argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "Docs", "Level", "v2_terrain_preview.png"), F, col)
        return
    out = argv[0] if argv else os.path.join(HERE, "..", "..", "Art", "Packed", "KG_Terrain_v2.glb")
    write_heights(out.replace(".glb", "_heights.json"), F, G, n)
    if bpy is not None:
        build_blender(out, F, col, G, gcol, n)


if __name__ == "__main__":   # Blender --python and plain python both run as __main__; importable for prep
    main()
