"""Procedural Japanese garden / shrine props for the partly-Japanese fishing village (Ghost-of-Tsushima mood,
vibrant toon look with a warm TF2 touch). Headless Blender 5.2, no textures.

  blender --background --factory-startup --python Tools/Blender/kg_make_japan_props.py -- \
      [out.glb] [preview_dir] [--no-preview] [--no-verify] [--closeup Torii,Pagoda] [--outdir DIR] [--tile 640]

Output: Art/Packed/KG_JapanProps.glb with one mesh object per prop (all at the origin, metres, Z up) and
contact sheets Art/Concept/JapanProps_preview_1.png / _2.png.

Vertex colour attribute "Col" (FLOAT_COLOR, face corner, exported as COLOR_0):
  RGB = linear albedo.
  A   = mask whose meaning depends on the prop (the material name says which master to use in UE):
        M_KG_JapanProps_Glow    (ToroLantern, GardenLamp, Noren_Stall): 1 on emissive faces, 0 elsewhere
        M_KG_JapanProps_Foliage (Sakura_A/B, Maple, BonsaiPine, Bamboo): 1 on blossoms/leaves (wind sway), 0 wood
        M_KG_JapanProps_Koi     (Koi): 0 at the head tip .. 1 at the tail tip (tail wiggle)
        M_KG_JapanProps_VC      (everything else): 0
Foliage blobs carry custom normals bent towards the canopy shape so the crown shades as one soft volume.
"""
import colorsys
import math
import os
import random
import shutil
import sys
import tempfile
from contextlib import contextmanager

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector, noise

ROOT = "D:/Kill Godot"

# ------------------------------------------------------------------------------------------------ arguments
_argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
_pos, _opt, _i = [], {}, 0
while _i < len(_argv):
    a = _argv[_i]
    if a.startswith("--"):
        if a in ("--no-preview", "--no-verify"):
            _opt[a] = True
        else:
            _opt[a] = _argv[_i + 1]
            _i += 1
    else:
        _pos.append(a)
    _i += 1
OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_JapanProps.glb"
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"
TILE = int(_opt.get("--tile", 640))

FLAT, SMOOTH, AUTO = 0, 1, 2


# ------------------------------------------------------------------------------------------------ small maths
def clamp(x, a=0.0, b=1.0):
    return a if x < a else b if x > b else x


def smoothstep(e0, e1, x):
    t = clamp((x - e0) / (e1 - e0))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def nz(p, f=1.0, o=(0.0, 0.0, 0.0)):
    """Perlin noise in [-1, 1]."""
    return noise.noise(Vector(p) * f + Vector(o))


def interp(tab, s):
    """Piecewise-linear lookup in [(s, value), ...]."""
    if s <= tab[0][0]:
        return tab[0][1]
    for (s0, v0), (s1, v1) in zip(tab, tab[1:]):
        if s <= s1:
            return lerp(v0, v1, (s - s0) / (s1 - s0))
    return tab[-1][1]


def frame(o, x, y, z):
    return Matrix(((x[0], y[0], z[0], o[0]), (x[1], y[1], z[1], o[1]), (x[2], y[2], z[2], o[2]), (0, 0, 0, 1)))


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(deg, axis):
    return Matrix.Rotation(math.radians(deg), 4, axis)


def circle(n, r=1.0, phase=0.0):
    return [(r * math.cos(phase + 2 * math.pi * k / n), r * math.sin(phase + 2 * math.pi * k / n)) for k in range(n)]


def bezier(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1.0 - t
        out.append(p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t))
    return out


def newell(pts):
    n = Vector((0.0, 0.0, 0.0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    return n


def rand_unit(rng):
    while True:
        v = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
        if 0.01 < v.length_squared <= 1.0:
            return v.normalized()


# ------------------------------------------------------------------------------------------------ colours
def _lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def C(hexv, a=0.0, v=1.0, s=1.0, h=0.0):
    """sRGB hex -> linear RGBA (optionally HSV-shifted)."""
    hexv = hexv.lstrip("#")
    r, g, b = (int(hexv[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
    hh, ss, vv = colorsys.rgb_to_hsv(r, g, b)
    r, g, b = colorsys.hsv_to_rgb((hh + h) % 1.0, clamp(ss * s), clamp(vv * v))
    return (_lin(r), _lin(g), _lin(b), a)


def mix(c1, c2, t):
    t = clamp(t)
    return tuple(c1[i] + (c2[i] - c1[i]) * t for i in range(4))


def mul(c, k):
    return (c[0] * k, c[1] * k, c[2] * k, c[3])


def with_a(c, a):
    return (c[0], c[1], c[2], a)


# ------------------------------------------------------------------------------------------------ icosphere
_ICO = {}


def icosphere(sub):
    if sub in _ICO:
        return _ICO[sub]
    t = (1.0 + 5 ** 0.5) / 2.0
    V = [Vector(v).normalized() for v in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
                                          (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
         (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10),
         (8, 6, 7), (9, 8, 1)]
    for _ in range(sub):
        cache = {}

        def mid(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                V.append(((V[a] + V[b]) * 0.5).normalized())
                cache[key] = len(V) - 1
            return cache[key]

        nf = []
        for a, b, c in F:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        F = nf
    _ICO[sub] = (V, F)
    return _ICO[sub]


# ------------------------------------------------------------------------------------------------ mesh builder
class MB:
    """Accumulates one mesh: vertices with colours, faces with optional flat colour override and shading mode.

    Shading: FLAT faces are faceted, SMOOTH faces are always smooth to each other, AUTO edges turn sharp above
    `angle` degrees. Closed islands get their normals recalculated outward on finalize (safety net)."""

    def __init__(self, name, angle=38.0, material="M_KG_JapanProps_VC"):
        self.name, self.angle, self.material = name, angle, material
        self.V, self.VC, self.F, self.FC, self.FS = [], [], [], [], []
        self.CN = {}
        self.M = Matrix.Identity(4)
        self.flip = False
        self._st = []

    # ---- transform stack
    @contextmanager
    def xf(self, M):
        self._st.append((self.M, self.flip))
        self.M = self.M @ M
        self.flip = self.M.to_3x3().determinant() < 0
        try:
            yield
        finally:
            self.M, self.flip = self._st.pop()

    # ---- raw
    def v(self, co, c):
        self.V.append(self.M @ Vector(co))
        self.VC.append(tuple(c))
        return len(self.V) - 1

    def f(self, ids, c=None, s=AUTO):
        ids = list(ids)
        if self.flip:
            ids.reverse()
        self.F.append(ids)
        self.FC.append(c)
        self.FS.append(s)

    def f_out(self, ids, inside, c=None, s=AUTO):
        """Face oriented away from `inside` (local coords) - for convex primitives."""
        ins = self.M @ Vector(inside)
        pts = [self.V[i] for i in ids]
        cen = sum(pts, Vector()) / len(pts)
        ids = list(ids)
        if newell(pts).dot(cen - ins) < 0:
            ids.reverse()
        self.F.append(ids)
        self.FC.append(c)
        self.FS.append(s)

    def cn(self, vid, d, w=1.0):
        """Custom normal hint (local direction, blend weight)."""
        self.CN[vid] = ((self.M.to_3x3() @ Vector(d)).normalized(), w)

    def deform(self, start, fn):
        """Apply fn(Vector)->Vector to every vertex created since `start` (world space)."""
        for i in range(start, len(self.V)):
            self.V[i] = Vector(fn(self.V[i]))

    def recolor(self, start, fn):
        for i in range(start, len(self.V)):
            self.VC[i] = tuple(fn(self.V[i], self.VC[i]))

    def cavity(self, start, dark=0.55, light=0.22):
        """Fake AO in vertex colour: darken concave, lighten convex vertices created since `start`."""
        nb, vn = {}, {}
        for f in self.F:
            if min(f) < start:
                continue
            fn = newell([self.V[i] for i in f])
            for k, i in enumerate(f):
                vn[i] = vn.get(i, Vector()) + fn
                s = nb.setdefault(i, set())
                s.add(f[k - 1])
                s.add(f[(k + 1) % len(f)])
        for i, ns in nb.items():
            avg = sum((self.V[j] for j in ns), Vector()) / len(ns)
            el = sum((self.V[j] - self.V[i]).length for j in ns) / len(ns)
            d = (self.V[i] - avg).dot(vn[i].normalized())
            k = clamp(d / (el * 0.3 + 1e-9), -1.0, 1.0)
            f = 1.0 + (light * k if k > 0 else dark * k)
            c = self.VC[i]
            self.VC[i] = (c[0] * f, c[1] * f, c[2] * f, c[3])

    # ---- primitives
    def box(self, c, s, col, sh=AUTO, fc=None):
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        ids = {}
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    ids[(sx, sy, sz)] = self.v((cx + sx * hx, cy + sy * hy, cz + sz * hz), col)
        for ax in range(3):
            o = [i for i in range(3) if i != ax]
            for sg in (-1, 1):
                vs = []
                for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    k = [0, 0, 0]
                    k[ax], k[o[0]], k[o[1]] = sg, a, b
                    vs.append(ids[tuple(k)])
                self.f_out(vs, c, fc, sh)

    def cbox(self, c, s, b, col, sh=AUTO):
        """Chamfered box (bevel b)."""
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        b = min(b, hx * 0.45, hy * 0.45, hz * 0.45)
        V = {}
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    V[(0, sx, sy, sz)] = self.v((cx + sx * hx, cy + sy * (hy - b), cz + sz * (hz - b)), col)
                    V[(1, sx, sy, sz)] = self.v((cx + sx * (hx - b), cy + sy * hy, cz + sz * (hz - b)), col)
                    V[(2, sx, sy, sz)] = self.v((cx + sx * (hx - b), cy + sy * (hy - b), cz + sz * hz), col)
        for ax in range(3):
            o = [i for i in range(3) if i != ax]
            for sg in (-1, 1):
                vs = []
                for a, bb in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    k = [0, 0, 0]
                    k[ax], k[o[0]], k[o[1]] = sg, a, bb
                    vs.append(V[(ax,) + tuple(k)])
                self.f_out(vs, c, None, sh)
        for a1, a2 in ((0, 1), (0, 2), (1, 2)):
            fr = 3 - a1 - a2
            for s1 in (-1, 1):
                for s2 in (-1, 1):
                    k0 = [0, 0, 0]
                    k0[a1], k0[a2], k0[fr] = s1, s2, -1
                    k1 = list(k0)
                    k1[fr] = 1
                    self.f_out([V[(a1,) + tuple(k0)], V[(a1,) + tuple(k1)], V[(a2,) + tuple(k1)], V[(a2,) + tuple(k0)]],
                               c, None, sh)
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    self.f_out([V[(0, sx, sy, sz)], V[(1, sx, sy, sz)], V[(2, sx, sy, sz)]], c, None, sh)

    def beam(self, p0, p1, w, h, col, up=(0, 0, 1), bevel=0.0, sh=AUTO):
        """Box from p0 to p1 (length axis), w across, h along `up`."""
        p0, p1 = Vector(p0), Vector(p1)
        x = p1 - p0
        L = x.length
        x.normalize()
        y = Vector(up).cross(x)
        if y.length < 1e-6:
            y = Vector((0, 1, 0)).cross(x)
        y.normalize()
        z = x.cross(y)
        with self.xf(frame((p0 + p1) * 0.5, x, y, z)):
            if bevel > 0:
                self.cbox((0, 0, 0), (L, w, h), bevel, col, sh)
            else:
                self.box((0, 0, 0), (L, w, h), col, sh)

    def lathe(self, prof, seg, col=None, colfn=None, segcol=None, sh=SMOOTH, phase=0.0, rfn=None, closed=False):
        """Surface of revolution around local Z. prof: [(r, z)] traced bottom-axis -> outside -> top-axis
        (outward normals). r == 0 end points collapse to a single apex vertex. closed=True wraps the profile."""
        rings = []
        for i, (r, z) in enumerate(prof):
            if r < 1e-6:
                rings.append(self.v((0, 0, z), colfn(0.0, z, 0.0, i) if colfn else col))
            else:
                ring = []
                for j in range(seg):
                    a = phase + 2 * math.pi * j / seg
                    rr = r * (rfn(a, i, z) if rfn else 1.0)
                    ring.append(self.v((rr * math.cos(a), rr * math.sin(a), z), colfn(rr, z, a, i) if colfn else col))
                rings.append(ring)
        pairs = list(range(len(prof) - 1)) + ([len(prof) - 1] if closed else [])
        for i in pairs:
            A, B = rings[i], rings[(i + 1) % len(prof)]
            fc = segcol[i] if segcol else None
            if isinstance(A, int) and isinstance(B, int):
                continue
            for j in range(seg):
                jn = (j + 1) % seg
                if isinstance(A, int):
                    self.f((A, B[jn], B[j]), fc, sh)
                elif isinstance(B, int):
                    self.f((A[j], A[jn], B), fc, sh)
                else:
                    self.f((A[j], A[jn], B[jn], B[j]), fc, sh)
        return rings

    def sweep(self, pts, prof, sx=None, sy=None, col=None, colfn=None, sh=AUTO, cap0=True, cap1=True,
              vertical=False, up=(0, 0, 1), tip=0.0, capcol=None, rmod=None):
        """Extrude closed 2D profile [(side, up)] along a path. vertical=True keeps sections in vertical planes."""
        pts = [Vector(p) for p in pts]
        n, m = len(pts), len(prof)
        area = sum(prof[k][0] * prof[(k + 1) % m][1] - prof[(k + 1) % m][0] * prof[k][1] for k in range(m))
        if area < 0:
            prof = prof[::-1]
        Tn = []
        for i in range(n):
            t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
            Tn.append(t.normalized())
        sides, ups = [], []
        if vertical:
            for t in Tn:
                th = Vector((t.x, t.y, 0.0))
                th = th.normalized() if th.length > 1e-6 else Vector((1, 0, 0))
                sides.append(Vector((0, 0, 1)).cross(th).normalized())
                ups.append(Vector((0, 0, 1)))
        else:
            side = Vector(up).cross(Tn[0])
            if side.length < 1e-6:
                side = Vector((0, 1, 0)).cross(Tn[0])
                if side.length < 1e-6:
                    side = Vector((1, 0, 0)).cross(Tn[0])
            side.normalize()
            for i in range(n):
                if i > 0:
                    q = Tn[i - 1].rotation_difference(Tn[i])
                    side = q @ side
                    side = (side - Tn[i] * side.dot(Tn[i])).normalized()
                sides.append(side.copy())
                ups.append(Tn[i].cross(side).normalized())
        rings = []
        for i in range(n):
            ring = []
            for k, (px, py) in enumerate(prof):
                mx = sx[i] if sx else 1.0
                my = sy[i] if sy else 1.0
                if rmod:
                    f = rmod(i, k)
                    mx, my = mx * f, my * f
                p = pts[i] + sides[i] * (px * mx) + ups[i] * (py * my)
                ring.append(self.v(p, colfn(i, k, p) if colfn else col))
            rings.append(ring)
        for i in range(n - 1):
            for k in range(m):
                kn = (k + 1) % m
                self.f((rings[i][k], rings[i][kn], rings[i + 1][kn], rings[i + 1][k]), None, sh)
        if cap0:
            self.f(list(reversed(rings[0])), capcol, FLAT)
        if tip > 0:
            apex = self.v(pts[-1] + Tn[-1] * tip, colfn(n - 1, 0, pts[-1]) if colfn else col)
            for k in range(m):
                self.f((rings[-1][k], rings[-1][(k + 1) % m], apex), None, sh)
        elif cap1:
            self.f(rings[-1], capcol, FLAT)
        return rings

    def tube(self, pts, radii, sides=8, col=None, colfn=None, sh=SMOOTH, cap0=True, cap1=True, tip=0.0,
             rmod=None, up=(0, 0, 1), phase=0.0):
        return self.sweep(pts, circle(sides, 1.0, phase), radii, radii, col, colfn, sh, cap0, cap1, False, up, tip,
                          rmod=rmod)

    def blob(self, c, r, sub=1, amp=0.15, freq=1.6, seed=0.0, col=None, colfn=None, sh=SMOOTH, rot=None, nrm=None):
        """Noisy ellipsoid. colfn(p_world, unit_dir); nrm(p_world, unit_dir) -> (dir, weight) custom normal."""
        V, F = icosphere(sub)
        c = Vector(c)
        rx, ry, rz = r if isinstance(r, (tuple, list)) else (r, r, r)
        off = Vector((seed * 13.1, seed * 7.7, seed * 3.3))
        ids = []
        for u in V:
            d = 1.0 + amp * noise.noise(u * freq + off)
            lp = Vector((u.x * rx * d, u.y * ry * d, u.z * rz * d))
            if rot is not None:
                lp = rot @ lp
            p = c + lp
            vid = self.v(p, colfn(self.M @ p, u) if colfn else col)
            if nrm:
                dd, w = nrm(self.M @ p, u)
                self.CN[vid] = (Vector(dd).normalized(), w)
            ids.append(vid)
        for a, b, cc in F:
            self.f((ids[a], ids[b], ids[cc]), None, sh)
        return ids

    def sheet2(self, grid, colfn, nrm=None, sh=SMOOTH):
        """Double-sided grid (rows x cols of local points). colfn(i, j, p). Back side uses separate vertices."""
        rows, cols = len(grid), len(grid[0])
        for side in (0, 1):
            ids = [[self.v(grid[i][j], colfn(i, j, grid[i][j])) for j in range(cols)] for i in range(rows)]
            if nrm:
                for i in range(rows):
                    for j in range(cols):
                        d, w = nrm(i, j, self.V[ids[i][j]])
                        self.CN[ids[i][j]] = (Vector(d).normalized(), w)
            for i in range(rows - 1):
                for j in range(cols - 1):
                    q = (ids[i][j], ids[i][j + 1], ids[i + 1][j + 1], ids[i + 1][j])
                    self.f(q if side == 0 else q[::-1], None, sh)

    def poly2(self, pts, cols, nrm=None, sh=SMOOTH):
        """Double-sided convex polygon card (fan triangulated by Blender)."""
        for side in (0, 1):
            ids = [self.v(p, c) for p, c in zip(pts, cols)]
            if nrm:
                for i in ids:
                    d, w = nrm(self.V[i])
                    self.CN[i] = (Vector(d).normalized(), w)
            self.f(ids if side == 0 else ids[::-1], None, sh)

    def panel(self, o, eu, ev, W, H, hole, depth, fcol, back_col, wall_col, outer=None):
        """Wall panel in plane o + eu*u + ev*v (normal eu x ev) with a recessed opening `hole` [(u, v)] CCW.
        fcol(p) colours the frame; the recess walls/back get flat override colours (glow)."""
        o, eu, ev = Vector(o), Vector(eu), Vector(ev)
        n = eu.cross(ev).normalized()

        def P(u, v, d=0.0):
            return o + eu * u + ev * v - n * d

        if outer is None:
            cu, cv = W / 2, H / 2
            outer = []
            for (u, v) in hole:
                dx, dy = u - cu, v - cv
                ts = []
                if abs(dx) > 1e-9:
                    ts.append(((W if dx > 0 else 0.0) - cu) / dx)
                if abs(dy) > 1e-9:
                    ts.append(((H if dy > 0 else 0.0) - cv) / dy)
                t = min(ts)
                outer.append((cu + dx * t, cv + dy * t))
        O = [self.v(P(u, v), fcol(P(u, v))) for u, v in outer]
        I = [self.v(P(u, v), fcol(P(u, v))) for u, v in hole]
        J = [self.v(P(u, v, depth), back_col) for u, v in hole]
        m = len(hole)
        for k in range(m):
            kn = (k + 1) % m
            self.f((O[k], O[kn], I[kn], I[k]), None, AUTO)
            self.f((I[k], I[kn], J[kn], J[k]), wall_col, FLAT)
        self.f(J, back_col, FLAT)


# ------------------------------------------------------------------------------------------------ finalize
MATS = {}


def finalize(mb):
    me = bpy.data.meshes.new(mb.name)
    me.from_pydata([tuple(v) for v in mb.V], [], [tuple(f) for f in mb.F])
    me.update()
    # Safety net: closed islands get outward normals.
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    bm.normal_update()
    seen = bytearray(len(bm.faces))
    flipped = closed_n = open_n = 0
    for f0 in bm.faces:
        if seen[f0.index]:
            continue
        stack, isl = [f0], []
        seen[f0.index] = 1
        while stack:
            f = stack.pop()
            isl.append(f)
            for e in f.edges:
                for lf in e.link_faces:
                    if not seen[lf.index]:
                        seen[lf.index] = 1
                        stack.append(lf)
        if all(len(e.link_faces) == 2 for f in isl for e in f.edges):
            closed_n += 1
            before = [f.normal.copy() for f in isl]
            bmesh.ops.recalc_face_normals(bm, faces=isl)
            bm.normal_update()
            flipped += sum(1 for f, nb in zip(isl, before) if f.normal.dot(nb) < 0)
        else:
            open_n += 1
    bm.to_mesh(me)
    bm.free()
    me.update()

    npoly, nloop = len(me.polygons), len(me.loops)
    lv = np.empty(nloop, np.int32)
    me.loops.foreach_get("vertex_index", lv)
    ls = np.empty(npoly, np.int32)
    me.polygons.foreach_get("loop_start", ls)
    lt = np.empty(npoly, np.int32)
    me.polygons.foreach_get("loop_total", lt)
    VC = np.array(mb.VC, np.float32)
    cols = VC[lv]
    for pi, fc in enumerate(mb.FC):
        if fc is not None:
            cols[ls[pi]:ls[pi] + lt[pi]] = fc
    ca = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    ca.data.foreach_set("color", cols.ravel())
    me.color_attributes.active_color = ca
    try:
        me.color_attributes.render_color_index = me.color_attributes.active_color_index
    except Exception:
        pass

    # Shading: smooth flags + angle based sharp edges (see MB doc).
    fs = np.array(mb.FS, np.int8)
    me.polygons.foreach_set("use_smooth", (fs != FLAT).tolist())
    pn = np.empty(npoly * 3, np.float32)
    me.polygons.foreach_get("normal", pn)
    pn = pn.reshape(-1, 3)
    le = np.empty(nloop, np.int32)
    me.loops.foreach_get("edge_index", le)
    poly_of_loop = np.repeat(np.arange(npoly), lt)
    ne = len(me.edges)
    ef = [[] for _ in range(ne)]
    for li in range(nloop):
        ef[le[li]].append(poly_of_loop[li])
    cos_thr = math.cos(math.radians(mb.angle))
    sharp = np.zeros(ne, bool)
    for ei, fl in enumerate(ef):
        if len(fl) != 2:
            sharp[ei] = len(fl) > 2
            continue
        a, b = fl
        ma, mb_ = fs[a], fs[b]
        if ma == FLAT or mb_ == FLAT:
            sharp[ei] = True
        elif ma == SMOOTH and mb_ == SMOOTH:
            sharp[ei] = False
        else:
            sharp[ei] = float(np.dot(pn[a], pn[b])) < cos_thr
    attr = me.attributes.get("sharp_edge") or me.attributes.new("sharp_edge", "BOOLEAN", "EDGE")
    attr.data.foreach_set("value", sharp.tolist())
    me.update()

    if mb.CN:
        cn = np.empty(nloop * 3, np.float32)
        me.corner_normals.foreach_get("vector", cn)
        cn = cn.reshape(-1, 3)
        for li in range(nloop):
            h = mb.CN.get(int(lv[li]))
            if h is not None:
                d, w = h
                v = Vector(cn[li]) * (1.0 - w) + d * w
                if v.length > 1e-6:
                    cn[li] = v.normalized()
        me.normals_split_custom_set([tuple(x) for x in cn])

    area = np.empty(npoly, np.float32)
    me.polygons.foreach_get("area", area)
    degenerate = int((area < 1e-8).sum())
    changed = me.validate(verbose=False, clean_customdata=False)

    obj = bpy.data.objects.new(mb.name, me)
    bpy.context.scene.collection.objects.link(obj)
    mat = MATS.get(mb.material)
    if mat is None:
        mat = MATS[mb.material] = bpy.data.materials.new(mb.material)
    me.materials.append(mat)
    tris = int((lt - 2).sum())
    diag = dict(closed=closed_n, open=open_n, flipped=flipped, degenerate=degenerate, validate_changed=bool(changed))
    return obj, tris, diag


# ================================================================================================ shared looks
VERM = C("E5452D")          # shrine vermilion (shu-iro), warm
VERM_DK = C("B9372A")
LACQUER = C("2A2426")       # black lacquer / pillar bases
GOLD = C("D6A83C")
NAVY = C("1F2A5A")


def granite(p, base="9D978E", light="B9B3A9", dark="746F69", moss_amt=0.0, moss_up=None, seed=0.0, scale=1.0):
    """Grey granite with low-frequency variation and optional moss (moss_up = upward facing factor 0..1).
    scale > 1 tightens the pattern for small stones."""
    p = Vector(p) * scale
    c = mix(C(dark), C(light), 0.5 + 0.6 * nz(p, 2.3, (seed, 3, 7)))
    c = mix(c, C(base), 0.35)
    c = mul(c, 1.0 + 0.08 * nz(p, 9.0, (1, seed, 2)))
    if moss_amt > 0:
        m = smoothstep(-0.05, 0.45, nz(p, 2.8, (11, 2, seed))) * moss_amt
        if moss_up is not None:
            m *= moss_up
        c = mix(c, mix(C("6C8A3A"), C("8FAA4A"), 0.5 + 0.5 * nz(p, 7, (seed, 5, 1))), m)
    return with_a(c, 0.0)


# ================================================================================================ 1. torii
def make_torii():
    mb = MB("SM_KG_Torii", angle=35)
    PX, LEAN = 3.45, math.radians(1.2)
    stain = C("B9443A", s=0.9)

    def px_at(z):
        return PX - z * math.tan(LEAN)

    def verm_grad(r, z, a, i):
        t = smoothstep(1.2, 3.8, z)
        return mix(stain, VERM, t)

    # Main pillars: black kamaki sleeve, tapered vermilion shaft, daiwa collar, slight inward lean (uchikorobi).
    prof = [(0, -0.4), (0.54, -0.4), (0.54, 1.12), (0.5, 1.22), (0.455, 1.25), (0.45, 1.32), (0.43, 4.6),
            (0.41, 7.85), (0.445, 7.9), (0.445, 8.15), (0.41, 8.2), (0.405, 8.85), (0, 8.85)]
    segcol = [LACQUER] * 4 + [None] * 3 + [VERM_DK] * 3 + [None] * 2
    for side in (-1, 1):
        with mb.xf(T(side * PX, 0, 0) @ R(-side * math.degrees(LEAN), 'Y')):
            mb.lathe(prof, 28, colfn=verm_grad, segcol=segcol, sh=AUTO)

    # Ryobu support legs (sode-bashira) with small black caps, tied to the main pillar by two beams.
    sprof = [(0, -0.4), (0.3, -0.4), (0.3, 0.95), (0.27, 1.02), (0.25, 1.06), (0.235, 5.25), (0, 5.25)]
    for side in (-1, 1):
        for sy in (-1, 1):
            x, y = side * PX, sy * 2.3
            with mb.xf(T(x, y, 0)):
                mb.lathe(sprof, 18, colfn=verm_grad, segcol=[LACQUER] * 3 + [None] * 3, sh=AUTO)
                cap_cols = dict(top=LACQUER, top_hi=LACQUER, fascia=C("1E1A1C"), soffit=VERM_DK, soffit_dk=VERM_DK,
                                soffit_edge=VERM_DK)
                hip_roof(mb, 4, 0.66, 0.06, 5.3, 0.3, 0.07, 0.08, 0.08, 0.1, cap_cols, nu=4, nv=4, nvu=1,
                         rot=math.pi / 4, powv=1.5)
                mb.lathe([(0, 5.2), (0.3, 5.2), (0.3, 5.26), (0, 5.26)], 4, col=VERM_DK, sh=AUTO, phase=math.pi / 4)
            for z in (2.35, 4.85):
                xb = side * (PX - z * math.tan(LEAN) * 0.5)
                mb.beam((xb, y + sy * 0.3, z), (xb, -sy * 0.25, z), 0.28, 0.34, VERM)
                # small wedge blocks where the beam passes the support leg
                mb.box((x, y + sy * 0.33, z), (0.3, 0.1, 0.4), VERM_DK)

    # Nuki (tie beam) through both pillars, with kusabi wedges.
    zn = 6.85
    xn = px_at(zn) + 1.15
    mb.beam((-xn, 0, zn), (xn, 0, zn), 0.4, 0.56, VERM, bevel=0.03)
    for side in (-1, 1):
        mb.box((side * (px_at(zn) + 0.52), 0, zn), (0.17, 0.46, 0.64), VERM_DK)

    # Kasagi curve: nearly flat centre, strong upturn (sori) towards the ends.
    HALF_K, Z_K, RISE_K, P_K = 5.3, 9.0, 0.74, 2.5

    def zk(x):
        return Z_K + RISE_K * (abs(x) / HALF_K) ** P_K

    # Gakuzuka (centre strut) + plaque (gaku) with gold border and simplified gold characters.
    z_sb = zk(0) - 0.46
    mb.box((0, 0, (zn + 0.28 + z_sb) / 2), (0.34, 0.3, z_sb - zn - 0.28), VERM)
    zc = (zn + 0.28 + z_sb) / 2
    mb.cbox((0, 0, zc), (1.12, 0.18, 1.3), 0.04, GOLD)
    mb.box((0, 0, zc), (0.9, 0.23, 1.08), C("1B2140"))
    for dz in (0.26, -0.26):
        mb.box((0, 0, zc + dz + 0.11), (0.4, 0.26, 0.055), GOLD)
        mb.box((0, 0, zc + dz - 0.02), (0.055, 0.26, 0.32), GOLD)
        mb.box((0, 0, zc + dz - 0.11), (0.3, 0.26, 0.05), GOLD)
        mb.box((0.12, 0, zc + dz + 0.02), (0.05, 0.26, 0.14), GOLD)

    def xs(half, n):
        return [half * (-1 + 2 * i / n) for i in range(n + 1)]

    # Shimaki: vermilion beam directly under the kasagi, following its curve.
    sh_x = xs(4.55, 44)
    mb.sweep([(x, 0, zk(x)) for x in sh_x], [(-0.33, -0.46), (0.33, -0.46), (0.33, 0.0), (-0.33, 0.0)],
             col=VERM, vertical=True)

    # Kasagi body (vermilion) + black roof-like cap, flaring thicker towards the ends, end faces slanted out.
    k0 = len(mb.V)
    kx = xs(HALF_K, 56)
    flare = [1.0 + 0.22 * (abs(x) / HALF_K) ** 6 for x in kx]
    mb.sweep([(x, 0, zk(x)) for x in kx], [(-0.44, 0.0), (0.44, 0.0), (0.44, 0.4), (-0.44, 0.4)],
             sx=[1.0] * len(kx), sy=flare, col=VERM, vertical=True)
    cx = xs(HALF_K + 0.1, 56)
    cflare = [1.0 + 0.22 * (abs(x) / HALF_K) ** 6 for x in cx]
    mb.sweep([(x, 0, zk(x)) for x in cx],
             [(-0.52, 0.39), (0.52, 0.39), (0.52, 0.47), (0.24, 0.66), (-0.24, 0.66), (-0.52, 0.47)],
             sx=[1.0 + 0.04 * (f - 1) for f in cflare], sy=cflare, col=LACQUER, vertical=True)

    def slant(p):
        ax = abs(p.x)
        w = smoothstep(HALF_K - 0.7, HALF_K + 0.1, ax)
        up = clamp((p.z - zk(min(ax, HALF_K))) / 0.66, 0.0, 1.3)
        return Vector((p.x + math.copysign(0.24 * up * w, p.x), p.y, p.z))

    mb.deform(k0, slant)
    return mb


# ================================================================================================ roofs
def hip_roof(mb, n, R, rtop, z0, rise, lift, flare, thick, urise, cols, nu=6, nv=5, nvu=2, rot=0.0,
             ripple=0.0, powv=1.6):
    """Curved hip roof over a regular n-gon (circumradius R at the eave, rtop at the top ring).
    Concave slope (z ~ v^powv), corners upturned by `lift` and pushed out by `flare`. Closed shell.
    With ripple > 0, every odd column is raised -> tile rows (nu should be even).
    cols: top, top_hi, fascia, soffit, soffit_dk, soffit_edge. Returns eave corner data."""
    N = n * nu

    def pos(ci, v, under):
        k, uu = divmod(ci, nu)
        u = uu / nu
        a0 = rot + 2 * math.pi * k / n
        a1 = rot + 2 * math.pi * (k + 1) / n
        ex = math.cos(a0) * (1 - u) + math.cos(a1) * u
        ey = math.sin(a0) * (1 - u) + math.sin(a1) * u
        cf = abs(2 * u - 1)
        s = (R + (rtop - R) * v) * (1 + flare * cf * cf * (1 - v) ** 2)
        up = lift * cf ** 3 * (1 - v) ** 2
        if under:
            z = z0 - thick + up + urise * v ** 1.3
        else:
            z = z0 + rise * v ** powv + up
            if ripple and uu % 2 == 1:
                z += ripple * (1 - 0.45 * v)
        return (ex * s, ey * s, z), uu

    top = []
    for j in range(nv + 1):
        row = []
        for ci in range(N):
            p, uu = pos(ci, j / nv, False)
            c = cols["top_hi"] if (ripple and uu % 2 == 1) else cols["top"]
            row.append(mb.v(p, c))
        top.append(row)
    und = []
    for j in range(nvu + 1):
        und.append([mb.v(pos(ci, j / nvu, True)[0], cols["soffit"]) for ci in range(N)])
    for j in range(nv):
        for ci in range(N):
            cin = (ci + 1) % N
            mb.f((top[j][ci], top[j][cin], top[j + 1][cin], top[j + 1][ci]), None, AUTO)
    for j in range(nvu):
        for ci in range(N):
            cin = (ci + 1) % N
            if j == 0:
                fc = cols["soffit_edge"]
            else:
                fc = cols["soffit_dk"] if (ci % 2 == 0) else cols["soffit"]
            mb.f((und[j][ci], und[j + 1][ci], und[j + 1][cin], und[j][cin]), fc, AUTO)
    for ci in range(N):
        cin = (ci + 1) % N
        mb.f((und[0][ci], und[0][cin], top[0][cin], top[0][ci]), cols["fascia"], AUTO)
        mb.f((top[nv][ci], top[nv][cin], und[nvu][cin], und[nvu][ci]), cols["soffit"], AUTO)
    corners = []
    for k in range(n):
        ci = k * nu
        pt, pb = mb.V[top[0][ci]].copy(), mb.V[und[0][ci]].copy()
        out = Vector((pt.x, pt.y, 0)).normalized()
        ridge = [mb.V[top[j][ci]].copy() for j in range(nv + 1)]
        corners.append(dict(top=pt, bottom=pb, out=out, ridge=ridge))
    return corners


def curl(mb, base, out, r0, size, col):
    """Warabi-de: small fern-like curl rising from a roof corner."""
    base, out = Vector(base), Vector(out)
    Z = Vector((0, 0, 1))
    O = base + Z * size
    pts, radii = [], []
    for i in range(10):
        t = i / 9
        phi = t * 1.55 * math.pi
        rho = size * (1 - 0.55 * t)
        pts.append(O + (out * math.sin(phi) - Z * math.cos(phi)) * rho)
        radii.append(r0 * (1 - 0.45 * t))
    mb.tube(pts, radii, 7, col=col, tip=r0 * 0.6)


# ================================================================================================ 2. toro
def make_toro():
    mb = MB("SM_KG_ToroLantern", angle=35, material="M_KG_JapanProps_Glow")

    def st(p, moss=0.35, up=None):
        return granite(p, moss_amt=moss, moss_up=up, seed=4.0, scale=1.8)

    def lcol(moss):
        return lambda r, z, a, i: st((r * math.cos(a), r * math.sin(a), z), moss * (1.0 - smoothstep(0.2, 1.4, z)) + 0.12)

    # kiso (hexagonal base with lotus-like step)
    mb.lathe([(0, -0.06), (0.47, -0.06), (0.48, 0.1), (0.44, 0.14), (0.37, 0.2), (0.31, 0.235), (0.22, 0.255),
              (0, 0.255)], 6, colfn=lcol(0.45), sh=AUTO)
    # sao (round pillar with a central fushi ring)
    mb.lathe([(0, 0.24), (0.17, 0.24), (0.16, 0.27), (0.13, 0.31), (0.12, 0.56), (0.145, 0.58), (0.145, 0.66),
              (0.12, 0.68), (0.115, 0.93), (0.13, 0.98), (0.165, 1.02), (0, 1.02)], 16, colfn=lcol(0.6), sh=AUTO)
    # chudai (flaring hexagonal platform)
    mb.lathe([(0, 1.0), (0.18, 1.0), (0.24, 1.05), (0.33, 1.1), (0.38, 1.14), (0.39, 1.2), (0.35, 1.225),
              (0.3, 1.235), (0, 1.235)], 6, colfn=lcol(0.3), sh=AUTO)
    # hibukuro (fire box): hexagonal, rectangular windows + sun/moon round windows, glowing recess (alpha 1)
    zf0, zf1, RF = 1.235, 1.6, 0.26
    glow_back, glow_wall = C("FFD66E", a=1.0), C("F3A43C", a=1.0)
    for k in range(6):
        a0, a1 = k * math.pi / 3, (k + 1) * math.pi / 3
        c0 = Vector((RF * math.cos(a0), RF * math.sin(a0), zf0))
        c1 = Vector((RF * math.cos(a1), RF * math.sin(a1), zf0))
        eu = c1 - c0
        W = eu.length
        eu.normalize()
        H = zf1 - zf0
        fcol = lambda p: st(p, 0.15)
        if k % 3 == 0:
            cu, cv, rr = W / 2, H * 0.52, 0.078
            angs = sorted({math.atan2(H - cv, W - cu) % (2 * math.pi), math.atan2(H - cv, -cu) % (2 * math.pi),
                           math.atan2(-cv, -cu) % (2 * math.pi), math.atan2(-cv, W - cu) % (2 * math.pi)} |
                          {2 * math.pi * i / 16 for i in range(16)})
            hole = [(cu + rr * math.cos(a), cv + rr * math.sin(a)) for a in angs]
            mb.panel(c0, eu, Vector((0, 0, 1)), W, H, hole, 0.07, fcol, glow_back, glow_wall)
        else:
            a, b, c = 0.048, 0.07, 0.055
            hole = [(a, b), (W - a, b), (W - a, H - c), (a, H - c)]
            outer = [(0, 0), (W, 0), (W, H), (0, H)]
            mb.panel(c0, eu, Vector((0, 0, 1)), W, H, hole, 0.07, fcol, glow_back, glow_wall, outer=outer)
    mb.lathe([(0, zf1 - 0.01), (0.31, zf1 - 0.01), (0.31, 1.64), (0, 1.64)], 6, colfn=lcol(0.1), sh=AUTO)
    # kasa (hexagonal roof, concave, upturned corners, warabi-de curls)
    roof_cols = dict(top=st((0, 0, 1.8), 0.0), top_hi=st((0, 0, 1.8), 0.0), fascia=C("8F8A82"), soffit=C("7D7872"),
                     soffit_dk=C("7D7872"), soffit_edge=C("8F8A82"))
    r0 = len(mb.V)
    corners = hip_roof(mb, 6, 0.62, 0.11, 1.68, 0.27, 0.075, 0.1, 0.08, 0.11, roof_cols, nu=6, nv=6, nvu=2,
                       rot=0.0, powv=1.7)
    mb.recolor(r0, lambda p, c: st(p, 0.55, up=smoothstep(1.7, 1.9, p.z) * 0.8 + 0.2) if c[3] == 0 else c)
    for cdat in corners:
        curl(mb, cdat["top"] - cdat["out"] * 0.02 + Vector((0, 0, -0.03)), cdat["out"], 0.034, 0.07, C("99948B"))
    # ukebana + hoju (jewel)
    mb.lathe([(0, 1.9), (0.12, 1.9), (0.14, 1.94), (0.13, 1.975), (0.095, 2.0), (0.07, 2.02), (0.09, 2.05),
              (0.115, 2.09), (0.11, 2.135), (0.08, 2.18), (0.04, 2.22), (0.012, 2.25), (0, 2.255)], 14,
             colfn=lcol(0.2), sh=AUTO)
    return mb


# ================================================================================================ 3. garden lamp
def make_garden_lamp():
    mb = MB("SM_KG_GardenLamp", angle=40, material="M_KG_JapanProps_Glow")
    blk, cap, glow, glow2 = C("1C1C1E"), C("33302D"), C("FFE2A0", a=1.0), C("FFF3CF", a=1.0)
    prof = [(0, -0.14), (0.012, -0.14), (0.012, 0.0), (0.07, 0.0), (0.075, 0.014), (0.055, 0.03), (0.019, 0.036),
            (0.017, 0.6), (0.046, 0.6), (0.046, 0.655), (0.03, 0.665),
            (0.2, 0.628), (0.207, 0.64), (0.192, 0.667), (0.165, 0.698), (0.125, 0.728), (0.08, 0.752),
            (0.04, 0.767), (0.014, 0.773), (0.014, 0.79), (0.0, 0.795)]
    segcol = [blk] * 8 + [glow2, glow2, glow, glow] + [cap] * 9
    mb.lathe(prof, 20, col=blk, segcol=segcol, sh=AUTO)
    return mb


# ================================================================================================ trees
class TreeGen:
    """Recursive, target-driven branching: every branch aims at the centroid of the canopy targets it serves and
    splits them between its children; radius follows the pipe model r = r_tip * n^pipe."""

    def __init__(self, mb, rng, bark, r_tip=0.03, pipe=0.55, gnarl=0.12, seg_len=0.3, frac=0.5, inertia=0.7,
                 k0=3, min_len=0.3):
        self.mb, self.rng, self.bark = mb, rng, bark
        self.r_tip, self.pipe, self.gnarl, self.seg_len = r_tip, pipe, gnarl, seg_len
        self.frac, self.inertia, self.k0, self.min_len = frac, inertia, k0, min_len
        self.root_split = True
        self.tips = []

    def rad(self, n):
        return self.r_tip * n ** self.pipe

    def path(self, p, d, end, gnarl=None):
        L = (end - p).length
        to = (end - p).normalized()
        p1 = p + d * L * 0.38
        p2 = end - (to * 0.75 + d * 0.25).normalized() * L * 0.3
        m = max(2, int(L / self.seg_len) + 1)
        pts = bezier(p, p1, p2, end, m)
        g = self.gnarl if gnarl is None else gnarl
        off = self.rng.random() * 100.0
        for i in range(1, m):
            t = i / m
            q = pts[i]
            w = Vector((noise.noise(q * 0.9 + Vector((off, 0, 0))), noise.noise(q * 0.9 + Vector((0, off, 0))),
                        noise.noise(q * 0.9 + Vector((0, 0, off)))))
            pts[i] = q + w * g * L * math.sin(math.pi * t)
        return pts

    def branch(self, pts, r0, r1, tip=False, rmod=None):
        m = len(pts)
        radii = [r0 + (r1 - r0) * (i / (m - 1)) for i in range(m)]
        sides = 12 if r0 > 0.13 else 9 if r0 > 0.065 else 7 if r0 > 0.028 else 5
        self.mb.tube(pts, radii, sides, colfn=lambda i, k, p: self.bark(p, radii[i]), sh=SMOOTH, cap0=True,
                     cap1=not tip, tip=(r1 * 2.2 if tip else 0.0), rmod=rmod)

    def split(self, o, axis, targets, k):
        if len(targets) <= k:
            return [[t] for t in targets]
        a = axis.normalized()
        b1 = a.orthogonal().normalized()
        b2 = a.cross(b1)
        P = [((t - o).dot(b1), (t - o).dot(b2)) for t in targets]
        n = len(P)
        if k == 2:
            mx = sum(p[0] for p in P) / n
            my = sum(p[1] for p in P) / n
            sxx = sum((p[0] - mx) ** 2 for p in P)
            syy = sum((p[1] - my) ** 2 for p in P)
            sxy = sum((p[0] - mx) * (p[1] - my) for p in P)
            ang = 0.5 * math.atan2(2 * sxy, sxx - syy)
            dx, dy = math.cos(ang), math.sin(ang)
            order = sorted(range(n), key=lambda i: (P[i][0] - mx) * dx + (P[i][1] - my) * dy)
            h = int(round(n * self.rng.uniform(0.38, 0.62)))
            h = min(max(h, 1), n - 1)
            return [[targets[i] for i in order[:h]], [targets[i] for i in order[h:]]]
        angs = sorted(range(n), key=lambda i: math.atan2(P[i][1], P[i][0]))
        A = [math.atan2(P[i][1], P[i][0]) for i in angs]
        gaps = [(A[(j + 1) % n] - A[j]) % (2 * math.pi) for j in range(n)]
        j0 = (max(range(n), key=lambda j: gaps[j]) + 1) % n
        order = angs[j0:] + angs[:j0]
        out, s = [], 0
        for c in range(k):
            e = round(n * (c + 1) / k)
            out.append([targets[i] for i in order[s:e]])
            s = e
        return [g for g in out if g]

    def grow(self, p, d, targets, depth, r_max):
        n = len(targets)
        r = min(self.rad(n), r_max)
        if depth == 0 and self.root_split and n > 1:
            # fork right at the trunk top: limbs diverge immediately (vase / umbrella silhouette)
            for g in self.split(p, d, targets, self.k0):
                gc = sum(g, Vector()) / len(g)
                dc = (d * 0.45 + (gc - p).normalized()).normalized()
                self.grow(p - d * r * 0.8, dc, g, 1, r)
            return
        if n == 1:
            end = targets[0]
            if (end - p).length < 0.2:
                end = p + d * 0.2
            pts = self.path(p, d, end)
            self.branch(pts, r, self.r_tip * 0.5, tip=True)
            self.tips.append((end, (pts[-1] - pts[-2]).normalized(), depth))
            return
        cen = sum(targets, Vector()) / n
        to = cen - p
        L = max(to.length * self.frac, self.min_len)
        dirn = (d * self.inertia + to.normalized()).normalized()
        end = p + dirn * L
        groups = self.split(end, dirn, targets, self.k0 if depth == 0 else 2)
        rc = max(self.rad(len(g)) for g in groups)
        r_end = min(r, max(r * 0.86, rc))
        pts = self.path(p, d, end)
        self.branch(pts, r, r_end)
        d_end = (pts[-1] - pts[-2]).normalized()
        for g in groups:
            self.grow(pts[-1] - d_end * r_end * 0.6, d_end, g, depth + 1, r_end)

    def trunk(self, pts, r0, r1, flare=0.5, lobes=5):
        """Trunk tube with root flare lobes near the ground."""
        m = len(pts)
        radii = [lerp(r0, r1, (i / (m - 1)) ** 0.8) for i in range(m)]
        sides = 14
        ph = self.rng.random() * 6.28

        def rmod(i, k):
            z = pts[i].z
            th = 2 * math.pi * k / sides
            return 1.0 + flare * math.exp(-max(z, 0.0) / 0.3) * (0.35 + 0.65 * max(0.0, math.cos(lobes * th + ph)))

        self.mb.tube(pts, radii, sides, colfn=lambda i, k, p: self.bark(p, radii[i]), sh=SMOOTH, rmod=rmod,
                     tip=radii[-1] * 0.7)
        return radii


def sample_targets(rng, n, fn, min_d, tries=6000):
    pts = []
    for _ in range(tries):
        p = fn(rng)
        if all((p - q).length >= min_d for q in pts):
            pts.append(p)
            if len(pts) >= n:
                break
    return pts


def ellipsoid_normal(center, radii):
    cx, cy, cz = center
    rx, ry, rz = radii

    def f(p):
        return Vector(((p.x - cx) / (rx * rx), (p.y - cy) / (ry * ry), (p.z - cz) / (rz * rz))).normalized()
    return f


def foliage_clumps(mb, rng, centers, r_range, n_range, flat, colfn, canopy_n, blend=0.55, amp=0.2, cards=0,
                   card_size=(0.12, 0.2), card_col=None, card_shape="quad", fringe=None):
    """Blossom/leaf clumps: noisy blobs (alpha 1) with canopy-bent normals + optional double-sided cards.
    fringe=(r_min, r_max, count): extra small puffs pushed outwards to break up the silhouette."""
    for ci, t in enumerate(centers):
        nb = rng.randint(*n_range)
        tint = rng.random()
        nf = fringe[2] if fringe else 0
        for b in range(nb + nf):
            if b >= nb:
                r = rng.uniform(fringe[0], fringe[1])
                g = canopy_n(t)
                c = t + (g * 0.8 + rand_unit(rng) * 0.6).normalized() * rng.uniform(0.45, 0.75)
            else:
                r = rng.uniform(*r_range)
            if b == 0:
                c = t + Vector((0, 0, r * 0.15))
            elif b < nb:
                c = t + rand_unit(rng) * r * rng.uniform(0.45, 0.8)
                c.z -= r * 0.1
            rz = r * flat

            def nrm(p, u, c=c, r=r, rz=rz):
                g = canopy_n(p)
                l = Vector((u.x / r, u.y / r, u.z / rz)).normalized()
                return (g * blend + l * (1 - blend)).normalized(), 1.0

            mb.blob(c, (r, r * rng.uniform(0.85, 1.1), rz), sub=1, amp=amp, freq=1.9, seed=rng.random() * 50,
                    colfn=lambda p, u, tint=tint: colfn(p, u, tint), nrm=nrm)
            for _ in range(cards):
                u = rand_unit(rng)
                u.z = abs(u.z) * 0.8 + 0.1 if rng.random() < 0.7 else u.z
                u.normalize()
                pos = c + Vector((u.x * r, u.y * r, u.z * rz)) * rng.uniform(0.92, 1.08)
                t1 = u.orthogonal().normalized()
                t1.rotate(Matrix.Rotation(rng.random() * 6.28, 3, u))
                t2 = u.cross(t1)
                s = rng.uniform(*card_size)
                tilt = u * s * 0.35
                cc = card_col(pos, rng.random())
                if card_shape == "star":
                    pts = [pos]
                    cols = [cc]
                    for kk in range(10):
                        ang = kk * math.pi / 5
                        rr = s * (0.62 if kk % 2 == 0 else 0.26)
                        pts.append(pos + (t1 * math.cos(ang) + t2 * math.sin(ang)) * rr + tilt * (0.6 if kk % 2 == 0 else 0.2))
                        cols.append(cc)
                    # star as a fan: build triangles manually (double-sided)
                    for side in (0, 1):
                        ids = [mb.v(p_, c_) for p_, c_ in zip(pts, cols)]
                        g = canopy_n(pos)
                        for i_ in ids:
                            mb.CN[i_] = (g, 1.0)
                        for kk in range(10):
                            tri = (ids[0], ids[1 + kk], ids[1 + (kk + 1) % 10])
                            mb.f(tri if side == 0 else tri[::-1], None, SMOOTH)
                else:
                    pts = [pos - t1 * s * 0.5 - t2 * s * 0.3, pos + t1 * s * 0.1 - t2 * s * 0.5 + tilt * 0.3,
                           pos + t1 * s * 0.5 + t2 * s * 0.3 + tilt, pos - t1 * s * 0.1 + t2 * s * 0.5 + tilt * 0.7]
                    g = canopy_n(pos)
                    mb.poly2(pts, [cc] * 4, nrm=lambda p_, g=g: (g, 1.0))


def sakura_bark(p, r):
    band = 0.5 + 0.5 * math.sin(p.z * 23.0 + nz(p, 3.0) * 2.0)
    c = mix(C("33262A"), C("54403C"), 0.35 + 0.35 * nz(p, 2.2, (4, 1, 1)) + 0.25 * band * smoothstep(0.05, 0.12, r))
    moss = smoothstep(0.1, 0.5, nz(p, 1.8, (9, 9, 9))) * (1 - smoothstep(0.2, 1.6, p.z)) * 0.55
    return with_a(mix(c, C("5E6B3E"), moss), 0.0)


def make_sakura(name, seed, weeping=False):
    mb = MB(name, angle=40, material="M_KG_JapanProps_Foliage")
    rng = random.Random(seed)
    light, mid, deep, shade = C("FFE3EE"), C("F7B3CF"), C("EC82AD"), C("C9588E")
    white = C("FFF4F7")
    if not weeping:
        cc, rad = Vector((0.2, 0.1, 4.25)), (3.45, 3.15, 1.45)
        n_t, min_d, trunk_h, lean = 34, 0.95, 1.45, Vector((0.3, 0.12, 0))

        def tfn(rng):
            th = rng.random() * 2 * math.pi
            el = rng.uniform(-0.5, 1.45)
            s = rng.uniform(0.6, 1.0)
            return cc + Vector((math.cos(el) * math.cos(th) * rad[0] * s, math.cos(el) * math.sin(th) * rad[1] * s,
                                math.sin(el) * rad[2] * s))
    else:
        cc, rad = Vector((0.55, -0.1, 4.2)), (2.45, 2.35, 1.25)
        n_t, min_d, trunk_h, lean = 22, 0.9, 1.7, Vector((0.6, -0.15, 0))

        def tfn(rng):
            th = rng.random() * 2 * math.pi
            el = rng.uniform(-0.2, 1.4)
            s = rng.uniform(0.6, 1.0)
            return cc + Vector((math.cos(el) * math.cos(th) * rad[0] * s, math.cos(el) * math.sin(th) * rad[1] * s,
                                math.sin(el) * rad[2] * s))

    targets = sample_targets(rng, n_t, tfn, min_d)
    tg = TreeGen(mb, rng, sakura_bark, r_tip=0.034, pipe=0.64, gnarl=0.16, seg_len=0.26, frac=0.52, inertia=0.55,
                 k0=4 if not weeping else 3)
    base = Vector((0, 0, -0.25))
    top = Vector((lean.x, lean.y, trunk_h))
    tpts = tg.path(base, Vector((0, 0, 1)), top, gnarl=0.09)
    # finer trunk sampling for the root flare
    tpts = bezier(tpts[0], tpts[0] + Vector((0, 0, 0.6)), top - (top - base).normalized() * 0.5, top, 9)
    r_trunk = tg.rad(len(targets))
    tg.trunk(tpts, r_trunk * 1.15, r_trunk, flare=0.55)
    d0 = (tpts[-1] - tpts[-2]).normalized()
    tg.grow(tpts[-1] - d0 * r_trunk * 0.5, d0, targets, 0, r_trunk)

    zmin = min(t.z for t in targets) - 0.7
    zmax = max(t.z for t in targets) + 0.8
    canopy_n = ellipsoid_normal(cc - Vector((0, 0, 0.6)), (rad[0] + 0.6, rad[1] + 0.6, rad[2] + 0.9))

    def bcol(p, u, tint):
        h = clamp((p.z - zmin) / (zmax - zmin))
        g = canopy_n(p)
        k = clamp(0.1 + 0.55 * h + 0.3 * g.z + 0.08 * u.z + 0.15 * nz(p, 1.7, (3, 3, 3)))
        c = mix(shade, deep, smoothstep(0.0, 0.35, k))
        c = mix(c, mid, smoothstep(0.3, 0.62, k))
        c = mix(c, light, smoothstep(0.6, 0.92, k))
        if tint < 0.2:
            c = mix(c, deep, 0.3)
        elif tint > 0.82:
            c = mix(c, white, 0.3)
        sp = nz(p, 7.0, (1, 9, 4))          # petal sparkle / blotches
        c = mix(c, white, smoothstep(0.3, 0.6, sp) * 0.45)
        c = mix(c, deep, smoothstep(-0.35, -0.6, sp) * 0.3)
        return with_a(c, 1.0)

    if not weeping:
        foliage_clumps(mb, rng, [t[0] for t in tg.tips], (0.36, 0.62), (2, 3), 0.74, bcol, canopy_n, blend=0.72,
                       amp=0.24, fringe=(0.2, 0.3, 1))
    else:
        foliage_clumps(mb, rng, [t[0] for t in tg.tips], (0.38, 0.56), (2, 3), 0.72, bcol, canopy_n, blend=0.72,
                       amp=0.24)
        # hanging blossom strands (shidare-zakura): thin drooping twigs threaded with blossom beads
        hang = [t[0] for t in tg.tips if t[0].z < cc.z + rad[2] * 0.65]
        extra = sample_targets(rng, 30, tfn, 0.55)
        hang += [e for e in extra if e.z < cc.z + 0.2]
        rng.shuffle(hang)
        for s0 in hang[:30]:
            out = Vector((s0.x - cc.x, s0.y - cc.y, 0))
            out = out.normalized() if out.length > 1e-4 else Vector((1, 0, 0))
            L = min(rng.uniform(1.2, 2.6), s0.z - 0.9)
            if L < 0.6:
                continue
            start = s0 + out * 0.3 + Vector((0, 0, -0.05))
            spine = [start + out * (0.3 * math.sin(t * 1.4)) + Vector((0, 0, -L * t))
                     for t in (i / 8 for i in range(9))]
            mb.tube(spine, [0.016 * (1 - 0.6 * i / 8) for i in range(9)], 4,
                    colfn=lambda i, k, p: with_a(sakura_bark(p, 0.01), 1.0), tip=0.02)
            nbead = max(3, int(L / 0.24))
            side = Vector((0, 0, 1)).cross(out)
            for k in range(nbead):
                t = (k + rng.uniform(0.2, 0.8)) / nbead
                zig = side * (0.05 * (1 if k % 2 else -1)) + rand_unit(rng) * 0.035
                sp = start + out * (0.3 * math.sin(t * 1.4)) + Vector((0, 0, -L * t)) + zig
                rb = 0.14 * (1 - 0.5 * t) * rng.uniform(0.7, 1.3)
                col_t = t
                if rng.random() < 0.3 and t < 0.8:
                    sp2 = sp - zig * 2.0 + Vector((0, 0, 0.04))
                    mb.blob(sp2, rb * 0.7, sub=0, amp=0.12, freq=2.0, seed=rng.random() * 50,
                            colfn=lambda p, u, col_t=col_t: with_a(mix(mix(deep, mid, 0.5), light, col_t * 0.8), 1.0),
                            nrm=lambda p, u: ((canopy_n(p) * 0.5 + u * 0.5).normalized(), 1.0))

                def bc(p, u, col_t=col_t):
                    c = mix(mix(deep, mid, 0.55), light, col_t * 0.8 + 0.2 * max(u.z, 0.0))
                    return with_a(mix(c, white, smoothstep(0.3, 0.6, nz(p, 7.0, (1, 9, 4))) * 0.4), 1.0)

                mb.blob(sp, (rb, rb, rb * 1.15), sub=0, amp=0.12, freq=2.0, seed=rng.random() * 50, colfn=bc,
                        nrm=lambda p, u: ((canopy_n(p) * 0.5 + u * 0.5).normalized(), 1.0))
    return mb


def make_maple():
    mb = MB("SM_KG_Maple", angle=40, material="M_KG_JapanProps_Foliage")
    rng = random.Random(33)
    tiers = [(1.9, 2.5, 13, 0.25), (2.75, 2.05, 11, -0.15), (3.5, 1.3, 6, 0.1)]
    targets = []
    for z, rr, n, ox in tiers:
        def tfn(rng, z=z, rr=rr, ox=ox):
            th = rng.random() * 2 * math.pi
            s = math.sqrt(rng.uniform(0.3, 1.0))
            return Vector((ox + math.cos(th) * rr * s, math.sin(th) * rr * s * 0.9, z + rng.uniform(-0.14, 0.14)))
        targets += sample_targets(rng, n, tfn, 0.8)

    def bark(p, r):
        c = mix(C("3F3431"), C("5C4B45"), 0.5 + 0.5 * nz(p, 2.5, (2, 2, 2)))
        return with_a(c, 0.0)

    tg = TreeGen(mb, rng, bark, r_tip=0.022, pipe=0.6, gnarl=0.14, seg_len=0.25, frac=0.5, inertia=0.5, k0=3)
    base = Vector((0, 0, -0.2))
    top = Vector((0.05, 0.0, 0.55))
    tpts = bezier(base, base + Vector((0, 0, 0.3)), top - Vector((0, 0, 0.2)), top, 4)
    rt = tg.rad(len(targets))
    tg.trunk(tpts, rt * 1.2, rt, flare=0.45, lobes=4)
    tg.grow(top - Vector((0, 0, rt * 0.5)), Vector((0, 0, 1)), targets, 0, rt)

    crim, red, scar, orange, amber = C("A5122C"), C("D6262B"), C("EE4A2A"), C("F7812A"), C("FBB23F")
    zmin, zmax = 1.4, 4.3
    canopy_n = ellipsoid_normal((0.1, 0, 2.2), (3.1, 2.8, 1.8))

    def lcol(p, u, tint):
        h = clamp((p.z - zmin) / (zmax - zmin))
        k = clamp(0.1 + 0.5 * h + 0.25 * canopy_n(p).z + 0.12 * u.z + 0.15 * nz(p, 1.5, (5, 1, 2)))
        c = mix(crim, red, smoothstep(0.0, 0.4, k))
        c = mix(c, scar, smoothstep(0.35, 0.7, k))
        c = mix(c, orange, smoothstep(0.65, 1.0, k))
        if tint > 0.78:
            c = mix(c, amber, 0.45)
        elif tint < 0.2:
            c = mix(c, crim, 0.4)
        return with_a(c, 1.0)

    foliage_clumps(mb, rng, [t[0] for t in tg.tips], (0.42, 0.62), (2, 3), 0.42, lcol, canopy_n, blend=0.65, amp=0.22)
    return mb


def make_pine():
    mb = MB("SM_KG_BonsaiPine", angle=40, material="M_KG_JapanProps_Foliage")
    rng = random.Random(44)

    def bark(p, r):
        plates = 0.5 + 0.5 * nz(p, 6.0, (1, 7, 3))
        c = mix(C("2E2826"), C("5A4B44"), 0.3 + 0.5 * plates)
        c = mix(c, C("7A5A48"), 0.25 * smoothstep(0.5, 0.9, plates))
        return with_a(c, 0.0)

    tg = TreeGen(mb, rng, bark, r_tip=0.03, pipe=0.55, gnarl=0.1, seg_len=0.25, frac=0.5, inertia=0.5, k0=2)
    # windswept S-curved trunk leaning towards +X
    ctrl = [Vector(p) for p in [(0, 0, -0.25), (0.05, 0, 0.5), (0.45, 0.05, 1.3), (0.2, -0.05, 2.1), (0.7, 0.05, 2.9),
                                (1.35, 0.0, 3.6), (1.6, -0.05, 4.2)]]
    tpts = []
    # Catmull-Rom through the control points
    P = [ctrl[0]] + ctrl + [ctrl[-1]]
    for i in range(1, len(P) - 2):
        for s in range(4):
            t = s / 4
            p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
            tpts.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                               (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    tpts.append(ctrl[-1])
    radii = tg.trunk(tpts, 0.27, 0.09, flare=0.6, lobes=4)
    tip_d = (tpts[-1] - tpts[-2]).normalized()
    mb.tube([tpts[-1], tpts[-1] + tip_d * 0.35 + Vector((0.1, 0, 0.1))], [0.09, 0.05], 8,
            colfn=lambda i, k, p: bark(p, 0.05), tip=0.05)

    # canopy pads (centre, radius): flat, layered clouds, biased downwind (+X)
    pads = [((1.85, 0.0, 4.75), 1.05), ((0.2, 0.55, 4.05), 0.85), ((2.55, -0.45, 3.75), 1.0), ((-0.75, -0.55, 3.3), 0.9),
            ((1.3, 0.85, 3.15), 0.8), ((2.9, 0.55, 2.7), 0.95), ((-1.05, 0.5, 2.45), 0.8), ((0.7, -1.05, 2.35), 0.85),
            ((1.9, -0.9, 1.9), 0.7)]
    pine_top, pine_mid, pine_dk, pine_hi = C("5F9A3E"), C("35692D"), C("1F4226"), C("8CBB52")
    pad_blobs = []
    for (pc, pr) in pads:
        pc = Vector(pc)
        # attach to the nearest trunk point below the pad
        best = min(range(len(tpts)), key=lambda i: (tpts[i] - (pc - Vector((0, 0, 0.55)))).length)
        best = max(2, min(best, len(tpts) - 2))
        start = tpts[best]
        subs = [pc + Vector((math.cos(a) * pr * 0.45, math.sin(a) * pr * 0.45, -0.05))
                for a in [rng.random() * 6.28 + k * 2.1 for k in range(3)]]
        d0 = (pc - start)
        d0.z = abs(d0.z) * 0.3 + 0.05
        d0.normalize()
        tg.grow(start + d0 * radii[best] * 0.2, d0, subs, 1, radii[best] * 0.62)
        pad_blobs.append((pc, pr, subs))

    for pc, pr, subs in pad_blobs:
        thick = pr * 0.34

        def pcol(p, u, tint, pc=pc, thick=thick):
            k = clamp(0.5 + (p.z - pc.z) / (thick * 1.6) + 0.12 * nz(p, 3.0, (8, 1, 2)))
            c = mix(pine_dk, pine_mid, smoothstep(0.1, 0.5, k))
            c = mix(c, pine_top, smoothstep(0.5, 0.85, k))
            c = mix(c, pine_hi, smoothstep(0.85, 1.05, k) * 0.6)
            if tint > 0.7:
                c = mul(c, 1.07)
            return with_a(c, 1.0)

        def pnrm(p, pc=pc, pr=pr):
            d = Vector(((p.x - pc.x) / pr, (p.y - pc.y) / pr, 0.0))
            return (d * 0.45 + Vector((0, 0, 1.0))).normalized()

        centers = [pc] + subs
        for i, c in enumerate(centers):
            r = pr * (0.62 if i == 0 else 0.5) * rng.uniform(0.9, 1.1)
            rz = thick * (1.0 if i == 0 else 0.8)
            tint = rng.random()

            def nrm(p, u, r=r, rz=rz, pnrm=pnrm):
                l = Vector((u.x / r, u.y / r, u.z / rz)).normalized()
                return (pnrm(p) * 0.6 + l * 0.4).normalized(), 1.0

            mb.blob(c + Vector((0, 0, 0.04 if i == 0 else -0.02)), (r, r * rng.uniform(0.85, 1.05), rz), sub=1,
                    amp=0.16, freq=2.0, seed=rng.random() * 50, colfn=lambda p, u, t=tint, f=pcol: f(p, u, t), nrm=nrm)
        # needle tufts around the pad rim: spiky double-sided triangles
        for k in range(16):
            a = k / 16 * 2 * math.pi + rng.uniform(-0.12, 0.12)
            rr = pr * rng.uniform(0.82, 1.0)
            base = pc + Vector((math.cos(a) * rr, math.sin(a) * rr, rng.uniform(-0.08, 0.02)))
            out = Vector((math.cos(a), math.sin(a), rng.uniform(-0.35, 0.15))).normalized()
            side = Vector((0, 0, 1)).cross(out).normalized()
            L = rng.uniform(0.2, 0.34)
            w = L * 0.3
            pts = [base - side * w, base + side * w, base + out * L]
            cc = with_a(mix(pine_mid, pine_top, rng.random()), 1.0)
            mb.poly2(pts, [cc] * 3, nrm=lambda p_, pnrm=pnrm: (pnrm(p_), 1.0))
    return mb


def make_bamboo():
    mb = MB("SM_KG_Bamboo", angle=40, material="M_KG_JapanProps_Foliage")
    rng = random.Random(55)
    stalk_cols = [C("6FA23E"), C("86B24A"), C("5E9438"), C("A7B85A"), C("79A843")]
    node_col = C("D7DDA2")
    leaf_cols = [C("4A8C32"), C("63A841"), C("7FBE50"), C("56983A"), C("8CC85A")]

    def spot(r):
        a, d = r.random() * 6.28, 0.6 * math.sqrt(r.random())
        return Vector((math.cos(a) * d, math.sin(a) * d, 0))

    spots = sample_targets(rng, 10, spot, 0.17)
    for si, b in enumerate(spots):
        H = rng.uniform(4.0, 6.0) * (0.85 if b.length > 0.4 else 1.0)
        r0 = rng.uniform(0.038, 0.058)
        out = Vector((b.x, b.y, 0))
        out = out.normalized() if out.length > 0.05 else Vector((1, 0, 0))
        lean = math.radians(rng.uniform(3, 12) * (1.3 if b.length > 0.35 else 0.7))
        base_c = stalk_cols[si % len(stalk_cols)]

        def axis(t, b=b, out=out, H=H, lean=lean):
            return Vector((b.x, b.y, -0.1)) + out * (H * math.sin(lean) * t ** 1.6) + Vector((0, 0, H * t))

        # stalk: node ridges every ~0.35-0.5 m (dark band below, pale ridge, clean internode above)
        pts, radii, cols = [], [], []
        z, nodes = 0.12, []
        while z < H * 0.96:
            nodes.append(z)
            z += 0.34 + 0.16 * math.sin(math.pi * z / H) + rng.uniform(-0.03, 0.03)
        pts.append(axis(0.0))
        radii.append(r0 * 1.05)
        cols.append(mul(base_c, 0.85))
        for zn in nodes:
            rr = r0 * (1 - 0.5 * zn / H)
            for dz, f, cc in ((-0.035, 1.0, mul(base_c, 0.8)), (0.0, 1.13, node_col), (0.03, 1.0, base_c)):
                pts.append(axis(clamp((zn + dz + 0.1) / H)))
                radii.append(rr * f)
                cols.append(cc)
        pts.append(axis(1.0))
        radii.append(r0 * 0.3)
        cols.append(base_c)
        mb.tube(pts, radii, 7, colfn=lambda i, k, p, cols=cols: with_a(mul(cols[i], 1 + 0.06 * nz(p, 4)), 0.0),
                tip=0.04)
        # leaf sprays on the upper nodes: short twigs with fans of drooping lanceolate leaves
        for zn in nodes:
            if zn / H < 0.45:
                continue
            for _ in range(2):
                p0 = axis(clamp((zn + 0.1) / H))
                a = rng.random() * 6.28
                d = Vector((math.cos(a), math.sin(a), rng.uniform(0.15, 0.55))).normalized()
                p1 = p0 + d * rng.uniform(0.18, 0.4)
                mb.tube([p0, p1], [0.008, 0.004], 4, col=with_a(base_c, 1.0))
                for _l in range(rng.randint(4, 6)):
                    la = a + rng.uniform(-1.2, 1.2)
                    ld = Vector((math.cos(la), math.sin(la), rng.uniform(-0.55, 0.05))).normalized()
                    L = rng.uniform(0.3, 0.42)
                    w = L * 0.14
                    side = Vector((0, 0, 1)).cross(ld).normalized()
                    lb = p1 + d * rng.uniform(-0.12, 0.0)

                    def P(s_, wv, lb=lb, ld=ld, side=side, L=L):
                        return lb + ld * (L * s_) + side * wv + Vector((0, 0, -1)) * (L * 0.4 * s_ * s_)
                    lc = with_a(mix(leaf_cols[rng.randint(0, 4)], C("B9D86E"), 0.2 * rng.random()), 1.0)
                    grid = [[P(0.0, -0.004), P(0.0, 0.004)], [P(0.38, -w), P(0.38, w)], [P(1.0, -0.002), P(1.0, 0.002)]]
                    mb.sheet2(grid, lambda i, j, p, lc=lc: lc if i > 0 else mul(lc, 0.85),
                              nrm=lambda i, j, p, ld=ld: (Vector((0, 0, 0.85)) + ld * 0.45, 1.0))
    return mb


# ================================================================================================ 8. koi pond rim
def kidney_r(th):
    """Kidney-shaped pond outline in polar form (stone centre line)."""
    a, b = 3.48, 2.12
    e = 1.0 / math.sqrt((math.cos(th) / a) ** 2 + (math.sin(th) / b) ** 2)
    d = math.atan2(math.sin(th - math.pi / 2), math.cos(th - math.pi / 2))
    return e * (1 - 0.3 * math.exp(-(d / 0.55) ** 2)) * (1 + 0.06 * math.cos(th))


def make_pond():
    mb = MB("SM_KG_KoiPondRim", angle=32)
    rng = random.Random(66)

    # --- basin: sloped wall from under the stones down to a pebbly floor at -0.5
    seg = 72
    rings = [(1.0, -0.12), (0.93, -0.3), (0.86, -0.45), (0.78, -0.5), (0.55, -0.52), (0.3, -0.53), (0.0, -0.53)]
    wall, floor = C("4B4238"), C("2B3232")

    def bcol(p):
        k = smoothstep(-0.45, -0.2, p.z)
        c = mix(mix(floor, C("3C4643"), 0.5 + 0.5 * nz(p, 3.0, (2, 7, 1))), wall, k)
        return with_a(mul(c, 1 + 0.12 * nz(p, 11.0)), 0.0)

    ids = []
    for f, z in rings:
        if f == 0.0:
            ids.append(mb.v((0, 0, z), bcol(Vector((0, 0, z)))))
            continue
        ring = []
        for j in range(seg):
            th = 2 * math.pi * j / seg
            r = kidney_r(th) * f - (0.08 if f < 1 else 0.0)
            p = Vector((r * math.cos(th), r * math.sin(th), z + 0.02 * nz((r * math.cos(th), r * math.sin(th), 0), 1.3)))
            ring.append(mb.v(p, bcol(p)))
        ids.append(ring)
    for i in range(len(ids) - 1):
        A, B = ids[i], ids[i + 1]
        for j in range(seg):
            jn = (j + 1) % seg
            if isinstance(B, int):
                mb.f((A[j], A[jn], B), None, SMOOTH)
            else:
                mb.f((A[j], A[jn], B[jn], B[j]), None, SMOOTH)   # faces up (seen from inside the basin)

    # --- rim stones along the outline (arc-length walk)
    stone_cols = ["7A746C", "8E877C", "5F5A55", "857868", "6B7074", "9A9286"]

    def scol(base, seed):
        def f(p, u):
            c = granite(p, base=base, light="B8B1A6", dark="5E5953", moss_amt=0.6,
                        moss_up=smoothstep(0.55, 0.95, u.z), seed=seed, scale=2.2)
            wet = smoothstep(-0.1, -0.3, p.z) * 0.45
            return with_a(mul(c, 1 - wet), 0.0)
        return f

    def stone(c, rx, ry, rz, yaw, sub=1):
        rot = Matrix.Rotation(yaw, 3, 'Z')
        s0 = len(mb.V)
        mb.blob(c, (rx, ry, rz), sub=sub, amp=0.18, freq=1.5, seed=rng.random() * 90, rot=rot,
                colfn=scol(stone_cols[rng.randint(0, 5)], rng.random() * 20), sh=AUTO)
        mb.cavity(s0, dark=0.45, light=0.0)

    th = 0.0
    perim = []
    while th < 2 * math.pi - 0.05:
        s = rng.uniform(0.33, 0.5)
        big = rng.random() < 0.1
        if big:
            s = rng.uniform(0.55, 0.7)
        r = kidney_r(th)
        p = Vector((r * math.cos(th), r * math.sin(th), 0))
        tang = Vector((-math.sin(th), math.cos(th), 0))
        top = rng.uniform(-0.03, 0.08) + (0.25 if big else 0.0)
        rz = s * (0.55 if not big else 0.72)
        stone(p + Vector((0, 0, top - rz)), s * 1.12, s * 0.9, rz, math.atan2(tang.y, tang.x) + rng.uniform(-0.3, 0.3),
              sub=2 if big else 1)
        perim.append((th, s))
        # advance by the stone size (overlap slightly)
        th += (s * 1.55) / max(r, 0.5)
    # inner row of smaller pebbles sloping into the water
    th = rng.random()
    while th < 2 * math.pi + 0.0:
        s = rng.uniform(0.14, 0.22)
        r = kidney_r(th) - 0.42
        p = Vector((r * math.cos(th), r * math.sin(th), rng.uniform(-0.24, -0.16)))
        stone(p, s * 1.1, s * 0.9, s * 0.55, rng.random() * 6.28)
        th += (s * 1.9) / max(r, 0.5)
    # a few outer accent stones
    for _ in range(9):
        th = rng.random() * 6.28
        r = kidney_r(th) + rng.uniform(0.35, 0.55)
        s = rng.uniform(0.18, 0.3)
        stone(Vector((r * math.cos(th), r * math.sin(th), -s * 0.35)), s * 1.2, s, s * 0.55, rng.random() * 6.28)
    # dark pebbles on the floor
    peb = [C("1F2424"), C("3A3F3E"), C("2A2E33"), C("5B5046"), C("6E6A64")]
    for _ in range(90):
        th = rng.random() * 6.28
        f = math.sqrt(rng.random()) * 0.8
        r = kidney_r(th) * f
        s = rng.uniform(0.045, 0.1)
        c = with_a(peb[rng.randint(0, 4)], 0.0)
        mb.blob((r * math.cos(th), r * math.sin(th), -0.53 + s * 0.3), (s * 1.2, s, s * 0.55), sub=0, amp=0.1,
                col=c, rot=Matrix.Rotation(rng.random() * 6.28, 3, 'Z'), sh=SMOOTH)
    return mb


# ================================================================================================ 9. wood walk
def make_walk():
    mb = MB("SM_KG_WoodWalk", angle=38)
    rng = random.Random(9)
    base, lt, dk = C("4F443C"), C("65574B"), C("3A322D")
    n = 26
    pitch = 4.0 / n
    for i in range(n):
        x = -2.0 + pitch * (i + 0.5)
        c = mix(base, lt if rng.random() < 0.5 else dk, rng.uniform(0.0, 0.8))
        L = 1.4 - rng.uniform(0.0, 0.05)
        with mb.xf(T(x, rng.uniform(-0.015, 0.015), 0) @ R(rng.uniform(-0.7, 0.7), 'Z')):
            mb.cbox((0, 0, -0.024 - rng.uniform(0, 0.004)), (pitch - 0.016, L, 0.048), 0.008, c)
    stringer = C("3A302A")
    for sy in (-1, 1):
        mb.box((0, sy * 0.55, -0.135), (4.0, 0.1, 0.18), stringer)
    for x in (-1.75, 0.0, 1.75):
        mb.box((x, 0, -0.29), (0.1, 1.32, 0.13), stringer)
        for sy in (-1, 1):
            mb.box((x, sy * 0.55, -0.62), (0.12, 0.12, 0.66), C("2F2824"))
    return mb


# ================================================================================================ 10. arch bridge
def make_arch_bridge():
    mb = MB("SM_KG_ArchBridge", angle=35)
    rng = random.Random(10)
    L2, H = 3.0, 0.85

    def zd(x):
        return H * (1 - (x / L2) ** 2)

    def sl(x):
        return -2 * H * x / (L2 * L2)

    wood = C("8A6B4E")
    # side girders (keta) following the arch
    gx = [-3.08 + 6.16 * i / 44 for i in range(45)]
    for sy in (-1, 1):
        mb.sweep([(x, sy * 0.74, zd(x)) for x in gx], [(-0.08, -0.36), (0.08, -0.36), (0.08, -0.01), (-0.08, -0.01)],
                 col=VERM, vertical=True)
    # deck planks along the arc
    xs = np.linspace(-3.0, 3.0, 600)
    arc = np.concatenate([[0], np.cumsum(np.sqrt(np.diff(xs) ** 2 + np.diff([zd(x) for x in xs]) ** 2))])
    step = 0.2
    s = step / 2
    while s < arc[-1]:
        x = float(np.interp(s, arc, xs))
        a = math.atan(sl(x))
        t = Vector((math.cos(a), 0, math.sin(a)))
        nrm = Vector((-math.sin(a), 0, math.cos(a)))
        c = mix(wood, C("A5835F") if rng.random() < 0.5 else C("6E533C"), rng.uniform(0, 0.6))
        with mb.xf(frame(Vector((x, 0, zd(x))), t, Vector((0, 1, 0)), nrm)):
            mb.cbox((0, 0, -0.03), (step - 0.022, 1.5, 0.06), 0.01, c)
        s += step
    # cross beams under the deck
    for x in (-2.4, -1.2, 0.0, 1.2, 2.4):
        a = math.atan(sl(x))
        t = Vector((math.cos(a), 0, math.sin(a)))
        nrm = Vector((-math.sin(a), 0, math.cos(a)))
        with mb.xf(frame(Vector((x, 0, zd(x))), t, Vector((0, 1, 0)), nrm)):
            mb.box((0, 0, -0.14), (0.12, 1.4, 0.14), VERM_DK)
    # support bents into the water
    for x in (-1.6, 0.0, 1.6):
        top = zd(x) - 0.3
        for sy in (-1, 1):
            with mb.xf(T(x, sy * 0.62, 0)):
                mb.lathe([(0, -1.3), (0.085, -1.3), (0.085, -0.55), (0.08, -0.5), (0.078, top), (0, top)], 12,
                         colfn=lambda r, z, a, i: mix(C("B9443A"), VERM, smoothstep(-0.9, 0.0, z)),
                         segcol=[LACQUER, LACQUER, LACQUER, None, None], sh=AUTO)
        mb.box((x, 0, top - 0.02), (0.18, 1.62, 0.16), VERM)
        mb.box((x, 0, min(top - 0.45, -0.25)), (0.12, 1.5, 0.12), VERM)
    # stone abutments at the ends
    for sx in (-1, 1):
        mb.cbox((sx * 3.05, 0, -0.33), (0.5, 2.0, 0.66), 0.04, granite((sx * 3, 0, 0), seed=3.0))
    # railings: posts, top rail (round), mid rail, giboshi finials on end + centre posts
    post_x = [-3.0 + 0.75 * i for i in range(9)]
    for sy in (-1, 1):
        for x in post_x:
            end = abs(x) > 2.9
            w = 0.14 if end or x == 0 else 0.09
            h = 0.98 if end or x == 0 else 0.84
            z0 = zd(x) - 0.02
            mb.cbox((x, sy * 0.74, z0 + h / 2), (w, w, h), 0.012, VERM)
            if end or x == 0:
                with mb.xf(T(x, sy * 0.74, z0 + h)):
                    mb.lathe([(0, -0.02), (0.085, -0.02), (0.085, 0.045), (0.06, 0.06), (0.09, 0.1), (0.1, 0.14),
                              (0.088, 0.185), (0.055, 0.225), (0.025, 0.26), (0.01, 0.29), (0, 0.3)], 12,
                             colfn=lambda r, z, a, i: GOLD if z > 0.05 else C("8C6A2A"), sh=AUTO)
        rx = [-3.0 + 6.0 * i / 40 for i in range(41)]
        mb.tube([(x, sy * 0.74, zd(x) + 0.8) for x in rx], [0.042] * len(rx), 8, col=VERM, sh=AUTO)
        mb.sweep([(x, sy * 0.74, zd(x) + 0.42) for x in rx], [(-0.03, -0.03), (0.03, -0.03), (0.03, 0.03), (-0.03, 0.03)],
                 col=VERM_DK, vertical=True)
    return mb


# ================================================================================================ 11. stones
def make_stepping_stone():
    mb = MB("SM_KG_SteppingStone", angle=38)
    ph = 1.3

    def rfn(a, i, z):
        return 1.0 + 0.16 * noise.noise(Vector((math.cos(a) * 1.1, math.sin(a) * 1.1, 4.2))) + 0.07 * math.sin(3 * a + ph)

    s0 = len(mb.V)
    mb.lathe([(0, -0.03), (0.27, -0.03), (0.3, 0.04), (0.295, 0.085), (0.265, 0.12), (0.18, 0.138), (0.08, 0.146),
              (0, 0.15)], 22, col=C("999999"), rfn=rfn, sh=AUTO)
    mb.deform(s0, lambda p: Vector((p.x, p.y * 0.8, p.z + (0.008 * nz(p, 6.0) if p.z > 0.1 else 0.0))))
    mb.recolor(s0, lambda p, c: granite(p, base="8C877F", light="B0AA9F", dark="5A5650", moss_amt=0.45,
                                        moss_up=1 - smoothstep(0.06, 0.13, p.z) if p.z > 0.02 else 0.0, seed=2.0,
                                        scale=3.5))
    mb.cavity(s0, dark=0.5, light=0.35)
    return mb


def rock(mb, center, half, seed, sub=3, cuts=6, amp=0.14, lean=0.0, colfn=None, bottom=-0.1):
    rng = random.Random(seed)
    V, F = icosphere(sub)
    planes = []
    for i in range(cuts):
        n = rand_unit(rng)
        n.z = n.z * 0.6 + (0.45 if i == 0 else 0.0)
        n.normalize()
        planes.append((n, rng.uniform(0.55, 0.82)))
    off = Vector((seed * 3.1, seed * 1.7, 0.3))
    ids = []
    for u in V:
        p = u * (1 + amp * noise.noise(u * 1.6 + off) + 0.04 * noise.noise(u * 5 + off))
        for n, d in planes:
            t = p.dot(n) - d
            if t > 0:
                p = p - n * t
        q = Vector((p.x * half[0], p.y * half[1], p.z * half[2]))
        q.x += lean * ((q.z / half[2] + 1) * 0.5) ** 2
        if q.z < -half[2] * 0.72:
            q.z = -half[2] * 0.72 + (q.z + half[2] * 0.72) * 0.15
        q = q + Vector(center)
        ids.append(mb.v(q, colfn(q, u) if colfn else C("888888")))
    for a, b, c in F:
        mb.f((ids[a], ids[b], ids[c]), None, AUTO)
    zmin = min(mb.V[i].z for i in ids)
    for i in ids:
        mb.V[i].z += bottom - zmin
    return ids


def make_garden_rock():
    mb = MB("SM_KG_GardenRock", angle=24)

    def col(p, u):
        return granite(p, base="858A90", light="AEB2B5", dark="565A60", moss_amt=0.7,
                       moss_up=smoothstep(0.55, 0.9, u.z), seed=6.0)

    mb.angle = 20
    ids = rock(mb, (0, 0, 0), (0.6, 0.47, 0.97), seed=19, sub=3, cuts=16, amp=0.1, lean=0.3, colfn=col, bottom=-0.12)
    ids += rock(mb, (0.62, -0.42, 0), (0.3, 0.26, 0.2), seed=7, sub=2, cuts=9, amp=0.1, lean=0.05, colfn=col,
                bottom=-0.08)
    # re-colour after the final placement: strata bands + height gradient + cavity AO
    for i in ids:
        p = mb.V[i]
        c = mb.VC[i]
        band = 0.5 + 0.5 * math.sin(p.z * 9.0 + 2.0 * nz(p, 1.2, (4, 4, 4)))
        k = (0.8 + 0.25 * smoothstep(-0.1, 1.4, p.z)) * (0.93 + 0.1 * band)
        mb.VC[i] = (c[0] * k, c[1] * k, c[2] * k, 0.0)
    mb.cavity(ids[0], dark=0.6, light=0.3)
    return mb


# ================================================================================================ 12. koi
def make_koi():
    mb = MB("SM_KG_Koi", angle=55, material="M_KG_JapanProps_Koi")
    XH, XT, XB = 0.25, -0.25, -0.112
    white, red, orange, black = C("F8F4EC"), C("E43B1B"), C("F26B21"), C("1C1B1F")
    fin_w = C("F1ECE4")
    W = [(0, 0.0), (0.025, 0.018), (0.07, 0.03), (0.15, 0.043), (0.3, 0.052), (0.45, 0.05), (0.6, 0.041),
         (0.75, 0.029), (0.88, 0.017), (1.0, 0.011)]
    HU = [(0, 0.0), (0.025, 0.016), (0.07, 0.029), (0.15, 0.043), (0.3, 0.051), (0.45, 0.049), (0.6, 0.04),
          (0.75, 0.029), (0.88, 0.019), (1.0, 0.014)]
    HL = [(s, v * 0.88) for s, v in HU]

    def zc(s):
        return 0.004 * math.sin(math.pi * s) - 0.002

    reds = [(0.1, 90, 0.08, 60), (0.34, 75, 0.1, 55), (0.36, 115, 0.07, 45), (0.6, 95, 0.09, 55), (0.83, 80, 0.06, 40)]
    blacks = [(0.26, 125, 0.035, 22), (0.5, 55, 0.045, 24), (0.7, 118, 0.03, 18), (0.44, 100, 0.02, 14)]

    def field(spots, s, thd, p):
        best = 0.0
        for s0, t0, ds, dt in spots:
            dth = (thd - t0 + 180) % 360 - 180
            best = max(best, math.exp(-((s - s0) / ds) ** 2 - (dth / dt) ** 2))
        return best + 0.18 * nz(p, 40.0, (1, 2, 3))

    def body_col(s, th, p):
        thd = math.degrees(th)
        c = white
        if math.sin(th) > -0.35 and s > 0.03:
            fr = field(reds, s, thd, p)
            c = mix(white, mix(red, orange, 0.3 + 0.3 * nz(p, 20)), smoothstep(0.42, 0.55, fr))
            fb = field(blacks, s, thd, p)
            c = mix(c, black, smoothstep(0.5, 0.62, fb))
        return c

    NS, NR = 16, 30
    rings = []
    for i in range(1, NR + 1):
        s = (i / NR) ** 1.2
        x = XH - s * (XH - XB)
        w, hu, hl = interp(W, s), interp(HU, s), interp(HL, s)
        ring = []
        for k in range(NS):
            th = 2 * math.pi * k / NS
            y = w * math.cos(th)
            z = zc(s) + (hu if math.sin(th) > 0 else hl) * math.sin(th)
            p = Vector((x, y, z))
            ring.append(mb.v(p, body_col(s, th, p)))
        rings.append(ring)
    apex = mb.v((XH + 0.004, 0, zc(0) - 0.004), white)
    for k in range(NS):
        kn = (k + 1) % NS
        mb.f((apex, rings[0][k], rings[0][kn]), None, SMOOTH)
    for i in range(NR - 1):
        for k in range(NS):
            kn = (k + 1) % NS
            mb.f((rings[i][k], rings[i + 1][k], rings[i + 1][kn], rings[i][kn]), None, SMOOTH)
    mb.f(rings[-1][::-1], None, SMOOTH)

    def fin_col(i, j, p, base=fin_w, streak=orange, rays=6.0, root_black=False, nu=1):
        v = i / max(nu, 1)
        ray = 0.5 + 0.5 * math.cos(j * math.pi * 2 / 2.0)
        c = mix(base, streak, 0.22 + 0.25 * ray * (1 - 0.7 * v))
        if root_black and v < 0.3:
            c = mix(c, black, 0.75 * (1 - v / 0.3))
        return c

    # caudal fin: two flowing lobes, notch in the middle, gentle sideways sweep
    nu, nv = 7, 10
    grid = []
    for i in range(nu + 1):
        u = i / nu
        row = []
        for j in range(nv + 1):
            v = -1 + 2 * j / nv
            Lv = 0.145 * (0.55 + 0.45 * abs(v) ** 1.3) * (1 - 0.12 * abs(v) ** 6)
            x = XB + 0.012 - u * Lv
            hz = (0.011 + 0.08 * u ** 0.8) * v * (1 + 0.07 * math.sin(v * 7 + u * 5))
            y = 0.012 * math.sin(2.6 * u + v * 0.8) * u
            row.append(Vector((x, y, zc(1.0) + hz)))
        grid.append(row)
    mb.sheet2(grid, lambda i, j, p: fin_col(i, j, p, nu=nu))

    def fin(root, d_len, d_wid, L, Wd, nu=5, nv=4, droop=0.0, root_black=False, curve=0.0):
        g = []
        for i in range(nu + 1):
            u = i / nu
            row = []
            for j in range(nv + 1):
                v = j / nv
                wv = Wd * (1 - 0.55 * u) * (v - 0.15 * u)
                p = root + d_len * (L * u) + d_wid * wv + Vector((0, 0, -droop * u * u)) + d_wid * (curve * u * u)
                row.append(p)
            g.append(row)
        mb.sheet2(g, lambda i, j, p: fin_col(i, j, p, root_black=root_black, nu=nu))

    # pectoral fins (long, butterfly-like), with black ray roots (showa motoguro)
    for sy in (-1, 1):
        root = Vector((XH - 0.2 * (XH - XB), sy * 0.036, zc(0.2) - 0.028))
        fin(root, Vector((-0.42, sy * 0.9, -0.1)).normalized(), Vector((-1, 0, -0.08)).normalized(), 0.125, 0.07,
            droop=0.02, root_black=True, curve=-0.25)
        root = Vector((XH - 0.52 * (XH - XB), sy * 0.022, zc(0.52) - 0.038))
        fin(root, Vector((-0.55, sy * 0.7, -0.45)).normalized(), Vector((-1, 0, 0)), 0.05, 0.03)
    # dorsal fin along the back
    g = []
    for i in range(9):
        u = i / 8
        s = 0.34 + 0.42 * u
        x = XH - s * (XH - XB)
        zb = zc(s) + interp(HU, s) * 0.93
        h = 0.034 * math.sin(math.pi * (0.15 + 0.85 * u)) + 0.006
        g.append([Vector((x, 0, zb)), Vector((x - 0.012 - 0.02 * u, 0.004 * math.sin(u * 3), zb + h))])
    mb.sheet2(g, lambda i, j, p: fin_col(j, i, p, nu=1))
    # anal fin
    root = Vector((XH - 0.8 * (XH - XB), 0, zc(0.8) - interp(HL, 0.8) * 0.9))
    fin(root, Vector((-0.6, 0, -0.8)).normalized(), Vector((-1, 0, 0)), 0.03, 0.025)
    # eyes and barbels
    for sy in (-1, 1):
        s = 0.07
        mb.blob((XH - s * (XH - XB) + 0.002, sy * interp(W, s) * 0.92, zc(s) + 0.008), 0.0075, sub=1, amp=0.0,
                col=C("111114"))
        b0 = Vector((XH - 0.012, sy * 0.009, zc(0.02) - 0.008))
        mb.tube([b0, b0 + Vector((-0.004, sy * 0.012, -0.006)), b0 + Vector((-0.012, sy * 0.02, -0.01))],
                [0.0022, 0.0016, 0.001], 4, col=C("E9C9B0"), tip=0.002)
    # 0.5 m nose to tail tip, pivot at the body centre (bbox centre)
    xmin, xmax = min(v.x for v in mb.V), max(v.x for v in mb.V)
    zmin, zmax = min(v.z for v in mb.V), max(v.z for v in mb.V)
    k = 0.5 / (xmax - xmin)
    ox, oz = (xmin + xmax) / 2, (zmin + zmax) / 2
    mb.deform(0, lambda p: Vector(((p.x - ox) * k, p.y * k, (p.z - oz) * k)))
    # alpha = normalised position along the body, head 0 -> tail 1
    for i in range(len(mb.VC)):
        c = mb.VC[i]
        mb.VC[i] = (c[0], c[1], c[2], clamp((0.25 - mb.V[i].x) / 0.5))
    return mb


# ================================================================================================ 13. pagoda
def make_pagoda():
    mb = MB("SM_KG_Pagoda", angle=35)
    rng = random.Random(51)
    verm, verm_dk, white, dwood = C("DD4A2E"), C("B43A28"), C("F4EFE3"), C("4A2F22")
    tile, tile_hi = C("39414F"), C("4D5767")
    roof_cols = dict(top=tile, top_hi=tile_hi, fascia=C("2C323C"), soffit=C("C4452F"), soffit_dk=C("8E2F22"),
                     soffit_edge=C("EDB64A"))
    ridge_col = C("262B34")

    # --- stone base: two steps of paving slabs + front stairs
    def slabs(half, z0, z1, n):
        step = 2 * half / n
        for i in range(n):
            for j in range(n):
                cx, cy = -half + step * (i + 0.5), -half + step * (j + 0.5)
                c = granite((cx, cy, z1), seed=rng.random() * 9)
                mb.cbox((cx, cy, (z0 + z1) / 2), (step - 0.014, step - 0.014, z1 - z0), 0.02, c)

    slabs(2.05, -0.1, 0.24, 5)
    slabs(1.72, 0.24, 0.55, 4)
    for k in range(3):
        mb.cbox((0, -2.05 - 0.28 * (2 - k) - 0.14, (0.18 * (k + 1)) / 2 - 0.05), (1.3, 0.3, 0.18 * (k + 1) + 0.1), 0.02,
                granite((0, -2.3, k), seed=5.0))

    def tier(a, z0, h, kind, balcony):
        t_start = len(mb.V)
        xs = [-a, -a / 3, a / 3, a]
        posts = set()
        for x in xs:
            posts.add((x, -a))
            posts.add((x, a))
            posts.add((-a, x))
            posts.add((a, x))
        for (x, y) in posts:
            with mb.xf(T(x, y, z0)):
                mb.lathe([(0, 0), (0.09, 0), (0.085, h), (0, h)], 10, col=verm, sh=AUTO)
        bay = 2 * a / 3
        for side in range(4):
            rotm = R(90 * side, 'Z')
            with mb.xf(rotm):
                for b in range(3):
                    cx = -a + bay * (b + 0.5)
                    mb.box((cx, -a, z0 + h / 2), (bay - 0.15, 0.08, h - 0.2), white)
                    if b == 1 and kind == "door":
                        mb.box((cx, -a - 0.02, z0 + 0.1 + (h - 0.35) / 2), (bay - 0.3, 0.08, h - 0.35), dwood)
                        mb.box((cx, -a - 0.045, z0 + 0.1 + (h - 0.35) / 2), (0.035, 0.05, h - 0.35), verm)
                        for zz in (0.3, 0.62):
                            mb.box((cx, -a - 0.045, z0 + 0.1 + (h - 0.35) * zz), (bay - 0.3, 0.05, 0.035), GOLD)
                    if b == 1 and kind == "window":
                        wz = z0 + h * 0.55
                        mb.box((cx, -a - 0.02, wz), (bay - 0.4, 0.08, h * 0.42), dwood)
                        for kk in range(6):
                            bx = cx - (bay - 0.45) / 2 + (bay - 0.45) * kk / 5
                            mb.box((bx, -a - 0.05, wz), (0.03, 0.04, h * 0.42), verm)
                # sill and head beams (nageshi)
                mb.box((0, -a - 0.02, z0 + 0.07), (2 * a + 0.24, 0.13, 0.14), verm)
                mb.box((0, -a - 0.02, z0 + h - 0.1), (2 * a + 0.24, 0.14, 0.2), verm)
                # bracket sets (tokyo) on every post of this side
                for x in xs:
                    zt = z0 + h
                    mb.box((x, -a, zt + 0.06), (0.26, 0.26, 0.12), verm_dk)
                    if abs(abs(x) - a) < 1e-6:
                        continue
                    mb.box((x, -a - 0.2, zt + 0.17), (0.1, 0.5, 0.1), verm)
                    mb.box((x, -a - 0.42, zt + 0.27), (0.15, 0.15, 0.12), verm_dk)
                    mb.box((x, -a - 0.36, zt + 0.37), (0.1, 0.72, 0.1), verm)
                    mb.box((x, -a - 0.68, zt + 0.46), (0.15, 0.15, 0.1), verm_dk)
                # corner bracket arm (diagonal)
            with mb.xf(rotm @ T(-a, -a, z0 + h) @ R(-135, 'Z')):
                mb.box((0.3, 0, 0.17), (0.7, 0.1, 0.1), verm)
                mb.box((0.62, 0, 0.27), (0.15, 0.15, 0.12), verm_dk)
                mb.box((0.62, 0, 0.37), (1.05, 0.1, 0.1), verm)
                mb.box((1.0, 0, 0.46), (0.15, 0.15, 0.1), verm_dk)
            # continuous purlin beam carried by the bracket ends
            with mb.xf(rotm):
                mb.box((0, -a - 0.68, z0 + h + 0.54), (2 * a + 1.3, 0.12, 0.08), verm_dk)
        if balcony:
            ab = a + 0.42
            for side in range(4):
                with mb.xf(R(90 * side, 'Z')):
                    mb.box((0, -a - 0.24, z0 - 0.02), (2 * a + 0.6, 0.5, 0.08), dwood)
                    mb.box((0, -ab, z0 + 0.42), (2 * ab + 0.05, 0.06, 0.06), verm)
                    mb.box((0, -ab, z0 + 0.2), (2 * ab, 0.04, 0.04), verm)
                    for kk in range(5):
                        x = -ab + 2 * ab * kk / 4
                        mb.box((x, -ab, z0 + 0.2), (0.07, 0.07, 0.46), verm)
        # baked ambient occlusion: shadow band under the eaves, slight grime at the wall base
        zt = z0 + h

        def ao(p, c):
            k = 1.0 - 0.24 * smoothstep(zt - 0.7, zt + 0.35, p.z) - 0.1 * (1 - smoothstep(z0, z0 + 0.35, p.z))
            return mul(c, k)
        mb.recolor(t_start, ao)

    def roof(E, z_eave, top_half, z_top, lift, thick=0.17, tiles=10, flare=0.1):
        Rr = E / math.cos(math.pi / 4)
        rt = top_half / math.cos(math.pi / 4)
        rise = z_top - z_eave
        corners = hip_roof(mb, 4, Rr, rt, z_eave, rise, lift, flare, thick, max(0.15, rise * 0.55), roof_cols,
                           nu=2 * tiles, nv=6, nvu=2, rot=math.pi / 4, ripple=0.045, powv=1.55)
        for cd in corners:
            ridge = [p + Vector((0, 0, 0.05)) for p in cd["ridge"]]
            tipp = cd["top"] + cd["out"] * 0.16 + Vector((0, 0, 0.14))
            mb.tube([tipp] + ridge, [0.06] + [0.075] * len(ridge), 7, col=ridge_col, sh=SMOOTH)
            # wind chime (futaku) hanging under the corner
            hb = cd["bottom"] - cd["out"] * 0.08
            mb.tube([hb, hb - Vector((0, 0, 0.12))], [0.008, 0.008], 4, col=C("6B5A2A"), cap0=True)
            with mb.xf(T(hb.x, hb.y, hb.z - 0.3)):
                mb.lathe([(0, 0), (0.07, 0.0), (0.065, 0.06), (0.05, 0.14), (0.02, 0.18), (0, 0.185)], 8,
                         col=C("C99B3B"), sh=AUTO)

    # tiers: (half width, z0, height, wall kind, balcony)
    t1, t2, t3 = (1.25, 0.55, 1.45), (0.98, 2.98, 1.1), (0.78, 4.95, 0.9)
    tier(*t1, "door", False)
    roof(2.3, t1[1] + t1[2] + 0.64, t2[0] + 0.3, t2[1] + 0.06, 0.34)
    tier(*t2, "window", True)
    roof(1.96, t2[1] + t2[2] + 0.64, t3[0] + 0.3, t3[1] + 0.06, 0.3, tiles=9)
    tier(*t3, "window", True)
    z3 = t3[1] + t3[2] + 0.64
    roof(1.68, z3, 0.2, z3 + 0.72, 0.28, tiles=8)
    # sorin spire: roban, fukubachi, ukebana, nine rings, suien, hoju
    zs = z3 + 0.66
    bronze, bronze_dk = C("C39A3E"), C("7C6533")
    mb.cbox((0, 0, zs + 0.1), (0.5, 0.5, 0.2), 0.02, C("3B3F48"))
    prof = [(0, zs + 0.2), (0.22, zs + 0.2), (0.22, zs + 0.24), (0.2, zs + 0.33), (0.13, zs + 0.4), (0.06, zs + 0.42),
            (0.13, zs + 0.45), (0.16, zs + 0.5), (0.05, zs + 0.52)]
    z = zs + 0.56
    for k in range(9):
        prof += [(0.04, z), (0.11 - k * 0.005, z + 0.01), (0.11 - k * 0.005, z + 0.04), (0.04, z + 0.05)]
        z += 0.085
    prof += [(0.035, z + 0.05), (0.09, z + 0.1), (0.1, z + 0.16), (0.07, z + 0.22), (0.035, z + 0.26),
             (0.08, z + 0.3), (0.085, z + 0.35), (0.05, z + 0.41), (0.015, z + 0.45), (0, z + 0.46)]
    mb.lathe(prof, 12, colfn=lambda r, z_, a, i: bronze if r > 0.06 else bronze_dk, sh=AUTO)
    # suien flame ornament: crossed thin plates
    for yaw in (0, 90):
        with mb.xf(T(0, 0, z + 0.05) @ R(yaw, 'Z')):
            mb.box((0, 0, 0.1), (0.28, 0.02, 0.1), bronze)
    mb.deform(0, lambda p: p * 0.93)
    return mb


# ================================================================================================ 14. yatai
def make_stall():
    mb = MB("SM_KG_Noren_Stall", angle=38, material="M_KG_JapanProps_Glow")
    rng = random.Random(14)
    wood, wood_dk, wood_lt, roofc = C("8B5A33"), C("563622"), C("D9A066"), C("3E2E26")
    # cart body
    mb.box((0, 0.05, 0.22), (1.84, 0.74, 0.08), wood_dk)
    mb.box((0, 0.05, 0.56), (1.76, 0.66, 0.66), wood_dk)
    for i in range(8):
        x = -0.87 + 0.2175 * (i + 0.5)
        c = mix(wood, C("A06A3B") if i % 2 else C("7A4E2C"), rng.uniform(0.2, 0.7))
        mb.cbox((x, -0.3, 0.56), (0.205, 0.04, 0.64), 0.008, c)
    for sx in (-1, 1):
        for j in range(3):
            y = -0.2 + 0.25 * j
            mb.cbox((sx * 0.9, y + 0.05, 0.56), (0.03, 0.23, 0.64), 0.006, mix(wood, C("7A4E2C"), rng.random() * 0.5))
    mb.cbox((0, -0.08, 0.925), (2.0, 0.98, 0.05), 0.01, wood_lt)
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.box((sx * 0.8, 0.05 + sy * 0.3, 0.1), (0.07, 0.07, 0.2), wood_dk)
    # wheels
    for sx in (-1, 1):
        with mb.xf(T(sx * 1.0, 0.1, 0.36) @ R(90, 'Y')):
            mb.lathe([(0.3, -0.035), (0.36, -0.035), (0.36, 0.035), (0.3, 0.035)], 20, col=C("5E3D25"), sh=AUTO,
                     closed=True)
            mb.lathe([(0, -0.06), (0.06, -0.06), (0.06, 0.06), (0, 0.06)], 10, col=C("3A2A1E"), sh=AUTO)
            for k in range(8):
                a = k * math.pi / 4
                mb.beam((0.05 * math.cos(a), 0.05 * math.sin(a), 0), (0.31 * math.cos(a), 0.31 * math.sin(a), 0),
                        0.035, 0.03, wood, up=(0, 0, 1))
    # posts and gable roof
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.cbox((sx * 0.86, -0.08 + sy * 0.38, 1.52), (0.07, 0.07, 1.2), 0.008, wood_dk)
    mb.cbox((0, -0.46, 2.07), (1.86, 0.08, 0.1), 0.01, wood_dk)
    mb.cbox((0, 0.3, 2.07), (1.86, 0.08, 0.1), 0.01, wood_dk)
    for sy in (-1, 1):
        for row in range(4):
            t0, t1 = row / 4, (row + 1) / 4 + 0.06
            y0, z0 = -0.08 + sy * 0.78 * (1 - t0), 2.12 + 0.36 * t0
            y1, z1 = -0.08 + sy * 0.78 * (1 - t1), 2.12 + 0.36 * t1
            c = mix(roofc, C("5A4234"), 0.5 * (row % 2) + 0.2 * rng.random())
            mb.beam((0, y0, z0), (0, y1, z1), 2.36 + 0.02 * row, 0.045, c,
                    up=(0, 0, 1), bevel=0.008)
    mb.cbox((0, -0.08, 2.5), (2.42, 0.12, 0.08), 0.015, C("2E221C"))
    # signboard on the roof front
    mb.cbox((0, -0.88, 2.24), (0.9, 0.04, 0.2), 0.01, C("F2E6CC"))
    for k in range(4):
        mb.box((-0.3 + 0.2 * k, -0.905, 2.24), (0.08, 0.02, 0.1), C("1E1B1A"))
    # noren curtain: five indigo strips with a white mon across the middle
    indigo, mon = C("283C8C"), C("F5F1E6")
    strips, sw, gap, top_z, L = 5, 0.34, 0.02, 2.02, 0.52
    mb.tube([(-0.95, -0.53, top_z + 0.01), (0.95, -0.53, top_z + 0.01)], [0.012, 0.012], 6, col=C("2E221C"))
    x0 = -(strips * sw + (strips - 1) * gap) / 2
    for s in range(strips):
        xa = x0 + s * (sw + gap)
        grid = []
        for i in range(11):
            v = i / 10
            row = []
            for j in range(8):
                u = j / 7
                x = xa + u * sw
                y = -0.53 + 0.015 * math.sin(x * 9 + v * 2) * v - 0.03 * v * v
                row.append(Vector((x, y, top_z - v * L)))
            grid.append(row)

        def ncol(i, j, p):
            d = math.hypot(p.x, p.z - (top_z - 0.25))
            ring = smoothstep(0.035, 0.02, abs(d - 0.15))
            dot = smoothstep(0.07, 0.05, d)
            return mix(indigo, mon, max(ring, dot))
        mb.sheet2(grid, ncol)
    # red paper lantern (akachochin), glows (alpha 1 on the paper)
    lx, ly = 1.02, -0.72
    mb.tube([(lx, ly, 2.13), (lx, ly, 2.02)], [0.008, 0.008], 4, col=C("2E221C"))
    lred = C("E0301E", a=1.0)
    prof = [(0, 1.5), (0.1, 1.5), (0.1, 1.535)]
    zs = [1.535 + 0.45 * k / 12 for k in range(13)]
    for z in zs:
        t = (z - 1.535) / 0.45
        prof.append((0.105 + 0.1 * math.sin(math.pi * t) ** 0.8, z))
    prof += [(0.1, 1.985), (0.1, 2.02), (0, 2.02)]
    ncap = 3

    def lcol(r, z, a, i):
        if i < ncap or i > len(prof) - 4:
            return C("1C1A1A")
        c = lred if (i - ncap) % 3 else C("A01E16", a=1.0)
        # black calligraphy band on the street side
        if math.sin(a) < -0.8 and 1.62 < z < 1.9:
            c = C("2A1414", a=1.0)
        return c

    with mb.xf(T(lx, ly, 0)):
        mb.lathe(prof, 16, colfn=lcol, segcol=[C("1C1A1A")] * 3 + [None] * 12 + [C("1C1A1A")] * 3, sh=SMOOTH)
    # counter props: iron pot with lid, bowls, sake bottle
    with mb.xf(T(0.45, 0.05, 0.95)):
        mb.lathe([(0, 0), (0.14, 0), (0.18, 0.05), (0.19, 0.15), (0.17, 0.2), (0.15, 0.2), (0.15, 0.21), (0.1, 0.25),
                  (0.03, 0.26), (0.03, 0.29), (0, 0.29)], 16, col=C("2F2E33"),
                 segcol=[None] * 6 + [C("8B5A33")] * 4, sh=AUTO)
    for i, (bx, by) in enumerate(((-0.2, -0.3), (-0.45, -0.25), (-0.7, -0.32))):
        with mb.xf(T(bx, by, 0.95)):
            mb.lathe([(0, 0), (0.04, 0), (0.045, 0.012), (0.075, 0.05), (0.08, 0.075), (0.07, 0.075), (0.035, 0.03),
                      (0, 0.03)], 12, col=C("F2EDE4"), segcol=[None, None, C("2C3F8A"), C("2C3F8A"), None, None, None],
                     sh=AUTO)
    with mb.xf(T(-0.05, 0.12, 0.95)):
        mb.lathe([(0, 0), (0.05, 0), (0.065, 0.06), (0.06, 0.13), (0.025, 0.2), (0.018, 0.25), (0.025, 0.265),
                  (0, 0.265)], 12, col=C("EDE3C8"), sh=SMOOTH)
    # wooden menu tags hanging under the right eave
    for k in range(4):
        mb.cbox((0.35 + 0.13 * k, -0.47, 1.9), (0.09, 0.012, 0.22), 0.004, C("E9DCC0"))
        mb.box((0.35 + 0.13 * k, -0.479, 1.9), (0.02, 0.006, 0.14), C("1E1B1A"))
    # customer bench
    mb.cbox((0, -0.95, 0.44), (1.3, 0.32, 0.05), 0.01, wood_lt)
    for sx in (-1, 1):
        mb.cbox((sx * 0.55, -0.95, 0.21), (0.06, 0.26, 0.42), 0.008, wood)
    return mb


# ================================================================================================ build + export
BUILDERS = [
    ("SM_KG_Torii", make_torii),
    ("SM_KG_ToroLantern", make_toro),
    ("SM_KG_GardenLamp", make_garden_lamp),
    ("SM_KG_Sakura_A", lambda: make_sakura("SM_KG_Sakura_A", 1234)),
    ("SM_KG_Sakura_B", lambda: make_sakura("SM_KG_Sakura_B", 777, weeping=True)),
    ("SM_KG_Maple", make_maple),
    ("SM_KG_BonsaiPine", make_pine),
    ("SM_KG_Bamboo", make_bamboo),
    ("SM_KG_KoiPondRim", make_pond),
    ("SM_KG_WoodWalk", make_walk),
    ("SM_KG_ArchBridge", make_arch_bridge),
    ("SM_KG_SteppingStone", make_stepping_stone),
    ("SM_KG_GardenRock", make_garden_rock),
    ("SM_KG_Koi", make_koi),
    ("SM_KG_Pagoda", make_pagoda),
    ("SM_KG_Noren_Stall", make_stall),
]


def build_all():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs, stats = [], []
    for name, fn in BUILDERS:
        mb = fn()
        assert mb.name == name, (mb.name, name)
        obj, tris, diag = finalize(mb)
        objs.append(obj)
        mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
        mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
        stats.append((name, tris, mn, mx, diag))
    print("\nKG_JAPAN: object                     tris     min (x, y, z)              max (x, y, z)          size")
    for name, tris, mn, mx, diag in stats:
        sz = mx - mn
        print(f"KG_JAPAN: {name:24s} {tris:6d}  ({mn.x:6.2f},{mn.y:6.2f},{mn.z:6.2f})  ({mx.x:6.2f},{mx.y:6.2f},{mx.z:6.2f})"
              f"  {sz.x:5.2f} x {sz.y:5.2f} x {sz.z:5.2f}  {diag}")
    print(f"KG_JAPAN: total tris {sum(s[1] for s in stats)}")
    return objs, stats


def export(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"KG_JAPAN: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


# ================================================================================================ preview
VIEWS = {
    "SM_KG_Torii": dict(el=10, az=-28, ground=0.0),
    "SM_KG_ToroLantern": dict(el=16, az=-30, ground=0.0),
    "SM_KG_GardenLamp": dict(el=20, az=-30, ground=0.0),
    "SM_KG_Sakura_A": dict(el=10, az=-30, ground=0.0),
    "SM_KG_Sakura_B": dict(el=10, az=-30, ground=0.0),
    "SM_KG_Maple": dict(el=12, az=-30, ground=0.0),
    "SM_KG_BonsaiPine": dict(el=10, az=-25, ground=0.0),
    "SM_KG_Bamboo": dict(el=10, az=-30, ground=0.0),
    "SM_KG_KoiPondRim": dict(el=42, az=-20, ground=-0.06, pond=True),
    "SM_KG_WoodWalk": dict(el=28, az=-35, ground=-0.9),
    "SM_KG_ArchBridge": dict(el=16, az=-30, ground=-0.35),
    "SM_KG_SteppingStone": dict(el=35, az=-30, ground=0.0),
    "SM_KG_GardenRock": dict(el=14, az=-30, ground=0.0),
    "SM_KG_Koi": dict(el=48, az=-40, ground=-0.2),
    "SM_KG_Pagoda": dict(el=10, az=-30, ground=0.0),
    "SM_KG_Noren_Stall": dict(el=12, az=-28, ground=0.0),
}
SHEETS = [
    ["SM_KG_Torii", "SM_KG_Pagoda", "SM_KG_Sakura_A", "SM_KG_Sakura_B",
     "SM_KG_Maple", "SM_KG_BonsaiPine", "SM_KG_Bamboo", "SM_KG_Noren_Stall"],
    ["SM_KG_ToroLantern", "SM_KG_GardenLamp", "SM_KG_KoiPondRim", "SM_KG_Koi",
     "SM_KG_ArchBridge", "SM_KG_WoodWalk", "SM_KG_SteppingStone", "SM_KG_GardenRock"],
]


def _node_mat(name, glow):
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    att = nt.nodes.new("ShaderNodeAttribute")
    att.attribute_name = "Col"
    nt.links.new(att.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    if glow:
        m = nt.nodes.new("ShaderNodeMath")
        m.operation = "MULTIPLY"
        m.inputs[1].default_value = 3.0
        nt.links.new(att.outputs["Alpha"], m.inputs[0])
        nt.links.new(att.outputs["Color"], bsdf.inputs["Emission Color"])
        nt.links.new(m.outputs[0], bsdf.inputs["Emission Strength"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    mat.use_backface_culling = True
    return mat


def _flat_mat(name, rgb):
    mat = bpy.data.materials.new(name)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat


def render_previews(objs, closeups=None, outdir=None):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 48
        sc.eevee.use_shadows = True
        sc.eevee.use_fast_gi = True
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x = sc.render.resolution_y = TILE
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("KG_PreviewWorld")
    sc.world = world
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.52, 0.68, 0.86, 1.0)
        bg.inputs["Strength"].default_value = 0.7
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.8
    sun.data.angle = math.radians(4)
    sun.data.color = (1.0, 0.95, 0.86)
    sc.collection.objects.link(sun)
    # preview materials (after export, so the glb keeps the plain material slots)
    pm = {"M_KG_JapanProps_Glow": _node_mat("P_Glow", True)}
    plain = _node_mat("P_Plain", False)
    for o in objs:
        o.data.materials[0] = pm.get(o.data.materials[0].name, plain)
    # ground plane + pond ground with a kidney hole
    gm = bpy.data.meshes.new("PreviewGround")
    gm.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PreviewGround", gm)
    sc.collection.objects.link(ground)
    ground.data.materials.append(_flat_mat("P_Ground", (0.3, 0.32, 0.25)))
    pv, pf = [], []
    seg = 96
    for j in range(seg):
        th = 2 * math.pi * j / seg
        r = kidney_r(th)
        pv.append((r * math.cos(th), r * math.sin(th), 0))
        pv.append((60 * math.cos(th), 60 * math.sin(th), 0))
    for j in range(seg):
        a, b = 2 * j, 2 * ((j + 1) % seg)
        pf.append((a, a + 1, b + 1, b))
    pgm = bpy.data.meshes.new("PondGround")
    pgm.from_pydata(pv, [], pf)
    pground = bpy.data.objects.new("PondGround", pgm)
    sc.collection.objects.link(pground)
    pground.data.materials.append(ground.data.materials[0])
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 50
    cam.data.sensor_width = 36
    cam.data.clip_start = 0.02
    cam.data.clip_end = 500
    sc.collection.objects.link(cam)
    sc.camera = cam
    lbl_curve = bpy.data.curves.new("Label", "FONT")
    lbl_curve.size = 0.026
    lbl = bpy.data.objects.new("Label", lbl_curve)
    sc.collection.objects.link(lbl)
    lm = bpy.data.materials.new("P_Label")
    em = lm.node_tree.nodes.get("Principled BSDF")
    if em:
        em.inputs["Base Color"].default_value = (0.02, 0.02, 0.03, 1)
        em.inputs["Emission Color"].default_value = (0.02, 0.02, 0.03, 1)
    lbl.data.materials.append(lm)
    lbl.parent = cam
    lbl.location = (-0.345, -0.345, -1.0)
    tan_h = math.tan(math.atan(18 / 50))
    tmp = tempfile.mkdtemp(prefix="kg_japan_")
    by_name = {o.name: o for o in objs}

    def shoot(name, path, az=None, el=None):
        o = by_name[name]
        cfg = VIEWS[name]
        for x in objs:
            x.hide_render = x is not o
        pond = cfg.get("pond", False)
        ground.hide_render = pond
        pground.hide_render = not pond
        ground.location.z = cfg["ground"]
        pground.location.z = cfg["ground"]
        el = math.radians(cfg["el"] if el is None else el)
        az = math.radians(cfg["az"] if az is None else az)
        sun.rotation_euler = (math.radians(50), 0, az - math.radians(55))
        corners = [o.matrix_world @ Vector(c) for c in o.bound_box]
        c = sum(corners, Vector()) / 8
        d = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
        f = -d
        right = f.cross(Vector((0, 0, 1))).normalized()
        up = right.cross(f)
        dist = max(max(abs((p - c).dot(right)), abs((p - c).dot(up))) / tan_h - (p - c).dot(f) for p in corners)
        dist = dist * 1.06 + 0.05
        cam.location = c - f * dist
        cam.rotation_euler = f.to_track_quat("-Z", "Y").to_euler()
        lbl.data.body = name.replace("SM_KG_", "")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)

    if closeups:
        os.makedirs(outdir, exist_ok=True)
        for name in closeups:
            full = name if name.startswith("SM_KG_") else "SM_KG_" + name
            for tag, azo in (("a", None), ("b", VIEWS[full]["az"] + 150)):
                p = os.path.join(outdir, f"{full}_{tag}.png")
                shoot(full, p, az=azo)
                print(f"KG_JAPAN: closeup {p}")
        return

    os.makedirs(PREVIEW_DIR, exist_ok=True)
    for si, names in enumerate(SHEETS):
        cols, rows = 4, 2
        sheet = np.zeros((rows * TILE, cols * TILE, 4), np.float32)
        for k, name in enumerate(names):
            p = os.path.join(tmp, f"{name}.png")
            shoot(name, p)
            img = bpy.data.images.load(p)
            arr = np.empty(TILE * TILE * 4, np.float32)
            img.pixels.foreach_get(arr)
            bpy.data.images.remove(img)
            r, c = divmod(k, cols)
            y0 = (rows - 1 - r) * TILE
            sheet[y0:y0 + TILE, c * TILE:(c + 1) * TILE] = arr.reshape(TILE, TILE, 4)
        out = bpy.data.images.new(f"sheet{si}", cols * TILE, rows * TILE, alpha=False)
        out.pixels.foreach_set(sheet.ravel())
        path = os.path.join(PREVIEW_DIR, f"JapanProps_preview_{si + 1}.png")
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        print(f"KG_JAPAN: preview {path}")
    shutil.rmtree(tmp, ignore_errors=True)


def verify(path, stats):
    """Re-import the glb in an empty scene and check names, triangle counts and COLOR_0 alpha ranges."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    expect = {s[0]: s[1] for s in stats}
    ok = True
    for o in sorted(bpy.data.objects, key=lambda o: o.name):
        if o.type != "MESH":
            continue
        me = o.data
        tris = sum(len(p.vertices) - 2 for p in me.polygons)
        ca = me.color_attributes[0] if len(me.color_attributes) else None
        amin = amax = None
        if ca:
            arr = np.empty(len(ca.data) * 4, np.float32)
            ca.data.foreach_get("color", arr)
            a = arr[3::4]
            amin, amax = float(a.min()), float(a.max())
        good = o.name in expect and ca is not None
        ok &= good
        print(f"KG_JAPAN_VERIFY: {o.name:24s} tris {tris:6d} (built {expect.get(o.name)})  color={ca.name if ca else None}"
              f"  alpha [{amin:.2f}, {amax:.2f}]  mat={me.materials[0].name if me.materials else None}")
    missing = set(expect) - {o.name for o in bpy.data.objects}
    if missing:
        ok = False
        print(f"KG_JAPAN_VERIFY: MISSING {sorted(missing)}")
    print(f"KG_JAPAN_VERIFY: {'OK' if ok else 'FAILED'}")


def main():
    objs, stats = build_all()
    export(OUT_GLB)
    if "--closeup" in _opt:
        render_previews(objs, closeups=_opt["--closeup"].split(","), outdir=_opt.get("--outdir", PREVIEW_DIR))
    elif "--no-preview" not in _opt:
        render_previews(objs)
    if "--no-verify" not in _opt:
        verify(OUT_GLB, stats)


main()
