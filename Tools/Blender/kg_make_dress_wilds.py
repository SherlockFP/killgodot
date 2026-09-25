"""KG_DressWilds prop factory: procedural low-poly props for the wilds, the church yard and the Japanese quarter of
Morrowmere (vibrant Quaternius / Sea-of-Thieves look, vertex colours only, no textures). Headless Blender 5.2.

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_wilds.py -- \
      [--out Art/Packed/KG_DressWilds.glb] [--only A,B,...] [--manifest Art/Packed/KG_DressWilds_Clean.json]
      [--previews Art/Concept] [--no-preview] [--closeup Name,Name] [--tile 640]

One mesh object per prop, named SM_KG_<Name>, at the origin, metres, Z up. Pivot = bottom centre (min Z = 0) unless
noted in SPECS (bridge: deck-end level, lantern string: west anchor, shovel: blade tip). Props FACE Blender -Y
(= Unreal +Y after the glTF import).

Vertex colour attribute "Col" (FLOAT_COLOR, face corner): RGB = LINEAR albedo, A = mask
   Glow  -> M_KG_JapanGlow     A = 1 on emissive faces (embers, flames, lantern paper)
   Sway  -> M_KG_JapanFoliage  A = 0 at anchors .. 1 on free leaves / cloth (wind)
   VC    -> M_KG_PropVCLinear  A = 0
The --manifest file lists material + collision per prop for Tools/Unreal/kg_import_dress_pack.py.
"""
import colorsys
import json
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
from mathutils.bvhtree import BVHTree

ROOT = "D:/Kill Godot"

# ------------------------------------------------------------------------------------------------ arguments
_argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
_opt, _i = {}, 0
while _i < len(_argv):
    a = _argv[_i]
    if a in ("--no-preview",):
        _opt[a] = True
    elif a.startswith("--"):
        _opt[a] = _argv[_i + 1]
        _i += 1
    _i += 1


def _abs(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


OUT_GLB = _abs(_opt.get("--out", "Art/Packed/KG_DressWilds.glb"))
PREVIEW_DIR = _abs(_opt.get("--previews", "Art/Concept"))
TILE = int(_opt.get("--tile", 640))
ONLY = [s.strip() for s in _opt["--only"].split(",")] if "--only" in _opt else None

FLAT, SMOOTH, AUTO = 0, 1, 2
MAT_VC, MAT_GLOW, MAT_SWAY = "M_KG_Dress_VC", "M_KG_Dress_Glow", "M_KG_Dress_Sway"


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


def frame(o, x, y, z):
    return Matrix(((x[0], y[0], z[0], o[0]), (x[1], y[1], z[1], o[1]), (x[2], y[2], z[2], o[2]), (0, 0, 0, 1)))


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(deg, axis):
    return Matrix.Rotation(math.radians(deg), 4, axis)


def S(x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    return Matrix.Diagonal((x, y, z, 1.0))


def circle(n, r=1.0, phase=0.0):
    return [(r * math.cos(phase + 2 * math.pi * k / n), r * math.sin(phase + 2 * math.pi * k / n)) for k in range(n)]


def bezier(p0, p1, p2, p3, n):
    p0, p1, p2, p3 = Vector(p0), Vector(p1), Vector(p2), Vector(p3)
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


def catenary(p0, p1, sag, n):
    p0, p1 = Vector(p0), Vector(p1)
    return [p0.lerp(p1, i / n) - Vector((0, 0, sag * 4 * (i / n) * (1 - i / n))) for i in range(n + 1)]


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
    A colour may be a callable f(world_pos) -> rgba (evaluated per vertex).

    Shading: FLAT faces are faceted, SMOOTH faces are always smooth to each other, AUTO edges turn sharp above
    `angle` degrees. Closed islands get their normals recalculated outward on finalize (safety net)."""

    def __init__(self, name, angle=38.0, material=MAT_VC):
        self.name, self.angle, self.material = name, angle, material
        self.V, self.VC, self.F, self.FC, self.FS = [], [], [], [], []
        self.CN = {}
        self.M = Matrix.Identity(4)
        self.flip = False
        self._st = []

    @contextmanager
    def xf(self, M):
        self._st.append((self.M, self.flip))
        self.M = self.M @ M
        self.flip = self.M.to_3x3().determinant() < 0
        try:
            yield
        finally:
            self.M, self.flip = self._st.pop()

    def v(self, co, c):
        p = self.M @ Vector(co)
        self.V.append(p)
        self.VC.append(tuple(c(p)) if callable(c) else tuple(c))
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

    def f_dir(self, ids, d, c=None, s=AUTO):
        """Face whose normal points along world direction d."""
        pts = [self.V[i] for i in ids]
        ids = list(ids)
        if newell(pts).dot(Vector(d)) < 0:
            ids.reverse()
        self.F.append(ids)
        self.FC.append(c)
        self.FS.append(s)

    def deform(self, start, fn):
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
            if vn[i].length < 1e-12:
                continue
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
             rmod=None, up=(0, 0, 1), phase=0.0, capcol=None):
        if not isinstance(radii, (list, tuple)):
            radii = [radii] * len(pts)
        return self.sweep(pts, circle(sides, 1.0, phase), radii, radii, col, colfn, sh, cap0, cap1, False, up, tip,
                          rmod=rmod, capcol=capcol)

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
            vid = self.v(p, (lambda q, u=u: colfn(q, u)) if colfn else col)
            if nrm:
                dd, w = nrm(self.V[vid], u)
                self.CN[vid] = (Vector(dd).normalized(), w)
            ids.append(vid)
        for a, b, cc in F:
            self.f((ids[a], ids[b], ids[cc]), None, sh)
        return ids

    def sheet2(self, grid, colf, colb=None, sh=SMOOTH, nrm=None):
        """Double-sided grid (rows x cols of local points). colf(i, j, p) front, colb back (default = front)."""
        rows, cols = len(grid), len(grid[0])
        for side in (0, 1):
            cf = colf if side == 0 or colb is None else colb
            ids = [[self.v(grid[i][j], cf(i, j, Vector(grid[i][j]))) for j in range(cols)] for i in range(rows)]
            if nrm:
                for i in range(rows):
                    for j in range(cols):
                        d, w = nrm(i, j, self.V[ids[i][j]], side)
                        self.CN[ids[i][j]] = (Vector(d).normalized(), w)
            for i in range(rows - 1):
                for j in range(cols - 1):
                    q = (ids[i][j], ids[i][j + 1], ids[i + 1][j + 1], ids[i + 1][j])
                    self.f(q if side == 0 else q[::-1], None, sh)

    def poly2(self, pts, colf, colb=None, sh=FLAT):
        """Double-sided planar polygon."""
        for side in (0, 1):
            c = colf if side == 0 or colb is None else colb
            ids = [self.v(p, c) for p in pts]
            self.f(ids if side == 0 else ids[::-1], None, sh)

    def prism(self, outline, y0, y1, col, bevel=0.0, sh=AUTO, front_col=None):
        """Outline [(x, z)] in the XZ plane (any winding, simple polygon) extruded from y0 (front) to y1 (back),
        with an optional chamfer. Returns the vertex start index."""
        start = len(self.V)
        pts = [Vector((x, z)) for x, z in outline]
        m = len(pts)
        area = sum(pts[k].x * pts[(k + 1) % m].y - pts[(k + 1) % m].x * pts[k].y for k in range(m))
        if area < 0:
            pts = pts[::-1]

        def inset(d):
            if d <= 0:
                return pts
            out = []
            for k in range(m):
                a, b, c = pts[k - 1], pts[k], pts[(k + 1) % m]
                e1, e2 = (b - a).normalized(), (c - b).normalized()
                n1, n2 = Vector((e1.y, -e1.x)), Vector((e2.y, -e2.x))   # outward normals (CCW)
                nb = n1 + n2
                nb = nb.normalized() if nb.length > 1e-6 else n1
                cosh = max(0.35, nb.dot(n1))
                out.append(b - nb * (d / cosh))
            return out

        if bevel > 0:
            layers = [(y0, inset(bevel)), (y0 + bevel, pts), (y1 - bevel, pts), (y1, inset(bevel))]
        else:
            layers = [(y0, pts), (y1, pts)]
        rings = [[self.v((p.x, y, p.y), col) for p in L] for y, L in layers]
        for i in range(len(rings) - 1):
            A, B = rings[i], rings[i + 1]
            for k in range(m):
                kn = (k + 1) % m
                self.f((A[k], B[k], B[kn], A[kn]), None, sh)
        self.f(rings[0], front_col, FLAT)
        self.f(list(reversed(rings[-1])), None, FLAT)
        return start

    def hole_wall(self, o, eu, ev, W, H, hole, fcol, depth=None, wall_col=None, back_col=None, thick=None,
                  bcol=None, sh=FLAT):
        """Rectangle W x H in the plane o + eu*u + ev*v (front normal eu x ev) with a hole [(u, v)] (CCW, star
        shaped from its centroid; may touch the border). depth: blind recess, walls wall_col(t: 0 front..1 back),
        back face back_col. thick: through hole (back face of the wall coloured bcol, reveal walls wall_col)."""
        o, eu, ev = Vector(o), Vector(eu), Vector(ev)
        n = eu.cross(ev).normalized()
        hole = [tuple(h) for h in hole]
        cu = sum(h[0] for h in hole) / len(hole)
        cv = sum(h[1] for h in hole) / len(hole)

        def ang(u, v):
            return math.atan2(v - cv, u - cu)

        # insert hole points on the rays towards the rectangle corners
        for (qu, qv) in ((0, 0), (W, 0), (W, H), (0, H)):
            a = ang(qu, qv)
            d = Vector((math.cos(a), math.sin(a)))
            m = len(hole)
            for k in range(m):
                p0, p1 = Vector(hole[k]), Vector(hole[(k + 1) % m])
                e = p1 - p0
                den = d.x * e.y - d.y * e.x
                if abs(den) < 1e-12:
                    continue
                w0 = p0 - Vector((cu, cv))
                t = (w0.x * e.y - w0.y * e.x) / den
                s = (w0.x * d.y - w0.y * d.x) / den
                if t > 0 and 1e-6 < s < 1 - 1e-6:
                    hole.insert(k + 1, tuple(p0 + e * s))
                    break

        def ray_rect(u, v):
            du, dv = u - cu, v - cv
            L = math.hypot(du, dv)
            du, dv = du / L, dv / L
            ts = []
            if abs(du) > 1e-9:
                ts.append(((W if du > 0 else 0.0) - cu) / du)
            if abs(dv) > 1e-9:
                ts.append(((H if dv > 0 else 0.0) - cv) / dv)
            t = min(tt for tt in ts if tt > 0)
            return (min(W, max(0.0, cu + du * t)), min(H, max(0.0, cv + dv * t)))

        outer = [ray_rect(u, v) for u, v in hole]

        def P(u, v, d=0.0):
            return o + eu * u + ev * v - n * d

        def ring_faces(Ouv, Iuv, d, flip):
            m = len(Iuv)
            for k in range(m):
                kn = (k + 1) % m
                quad = [Ouv[k], Ouv[kn], Iuv[kn], Iuv[k]]
                uniq = []
                for q in quad:
                    if all(math.hypot(q[0] - w[0], q[1] - w[1]) > 1e-6 for w in uniq):
                        uniq.append(q)
                if len(uniq) < 3:
                    continue
                pts = [P(u, v, d) for u, v in uniq]
                if newell(pts).length < 1e-9:
                    continue
                ids = [self.v(p, fcol if not flip else bcol) for p in pts]
                self.f_dir(ids, -n if flip else n, None, sh)

        ring_faces(outer, hole, 0.0, False)
        D = depth if depth is not None else thick
        if D:
            m = len(hole)
            for k in range(m):
                kn = (k + 1) % m
                a, b = hole[k], hole[kn]
                if math.hypot(a[0] - b[0], a[1] - b[1]) < 1e-6:
                    continue
                ids = [self.v(P(*a, 0.0), wall_col(0.0)), self.v(P(*b, 0.0), wall_col(0.0)),
                       self.v(P(*b, D), wall_col(1.0)), self.v(P(*a, D), wall_col(1.0))]
                mid = (P(*a, D * 0.5) + P(*b, D * 0.5)) * 0.5
                inward = (o + eu * cu + ev * cv - n * D * 0.5) - mid
                self.f_dir(ids, inward, None, sh)
            if depth is not None:
                ids = [self.v(P(u, v, D), back_col) for u, v in hole]
                self.f_dir(ids, n, None, FLAT)
            else:
                ring_faces(outer, hole, D, True)


# ------------------------------------------------------------------------------------------------ finalize
MATS = {}


def finalize(mb):
    me = bpy.data.meshes.new(mb.name)
    me.from_pydata([tuple(v) for v in mb.V], [], [tuple(f) for f in mb.F])
    me.update()
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
VERM = C("E5452D")
VERM_DK = C("B9372A")
LACQUER = C("2A2426")
GOLD = C("D6A83C")
IRON = C("2C2B31")
IRON_L = C("4A4852")
ROPE = C("CDB888")
WOOD = C("8B5A33")
WOOD_D = C("563622")
WOOD_L = C("C28C57")
WOOD_GREY = C("9A8670")
BLACK = C("0C0A0A")
EMBER = C("FF7A1E", a=1.0)
FLAME_Y = C("FFD84A", a=1.0)
FLAME_O = C("FF9A2A", a=1.0)
MOSS, MOSS_L = C("5F8A35"), C("8DB04A")


def granite(p, base="9D978E", light="B9B3A9", dark="746F69", moss_amt=0.0, moss_up=None, seed=0.0, scale=1.0):
    """Grey stone with low-frequency variation and optional moss (moss_up = upward facing factor 0..1)."""
    p = Vector(p) * scale
    c = mix(C(dark), C(light), 0.5 + 0.6 * nz(p, 2.3, (seed, 3, 7)))
    c = mix(c, C(base), 0.35)
    c = mul(c, 1.0 + 0.08 * nz(p, 9.0, (1, seed, 2)))
    if moss_amt > 0:
        m = smoothstep(-0.05, 0.45, nz(p, 2.8, (11, 2, seed))) * moss_amt
        if moss_up is not None:
            m *= moss_up
        c = mix(c, mix(MOSS, MOSS_L, 0.5 + 0.5 * nz(p, 7, (seed, 5, 1))), m)
    return with_a(c, 0.0)


def lichen(p, c, amt=0.5, seed=0.0, freq=6.0):
    """Yellow/orange lichen spots on stone."""
    k = smoothstep(0.35, 0.55, nz(p, freq, (seed, 9, 4))) * amt
    return mix(c, mix(C("D9A441"), C("C8C27A"), 0.5 + 0.5 * nz(p, 3, (seed, 1, 1))), k)


def bark(p, base="6A4B35", dark="3E2C20", seed=0.0):
    band = 0.5 + 0.5 * math.sin(p.z * 17.0 + p.x * 11.0 + nz(p, 3.0, (seed, 0, 0)) * 3.0)
    c = mix(C(dark), C(base), 0.45 + 0.3 * nz(p, 2.5, (3, seed, 7)) + 0.25 * band)
    return with_a(c, 0.0)


def plank_col(rng, base=WOOD, lt=None, dk=None):
    lt = lt or C("A87248")
    dk = dk or C("6E4428")
    return mix(base, lt if rng.random() < 0.5 else dk, rng.uniform(0.0, 0.7))


def moss_tops(mb, fstart, amt=0.85, thr=0.55, seed=0.0, freq=1.6, bias=0.0):
    """Flat moss patches on upward faces created since face index fstart."""
    for fi in range(fstart, len(mb.F)):
        ids = mb.F[fi]
        pts = [mb.V[i] for i in ids]
        n = newell(pts)
        if n.length < 1e-12:
            continue
        n.normalize()
        if n.z < thr:
            continue
        cen = sum(pts, Vector()) / len(pts)
        base = mb.FC[fi] or tuple(sum(mb.VC[i][k] for i in ids) / len(ids) for k in range(4))
        k = smoothstep(-0.15, 0.3, nz(cen, freq, (seed, 1, 2)) + bias) * amt * smoothstep(thr, 0.95, n.z)
        if k <= 0.03:
            continue
        m = with_a(mix(MOSS, MOSS_L, 0.5 + 0.5 * nz(cen, 5.0, (2, seed, 0))), base[3])
        mb.FC[fi] = mix(base, m, k)


def rock(mb, center, half, seed, sub=2, cuts=6, amp=0.14, lean=0.0, colfn=None, bottom=None, flat_below=None):
    """Faceted boulder: noisy icosphere cut by random planes. colfn(p_world, unit_dir)."""
    rng = random.Random(seed)
    V, F = icosphere(sub)
    planes = []
    for i in range(cuts):
        n = rand_unit(rng)
        n.z = n.z * 0.6 + (0.45 if i == 0 else 0.0)
        n.normalize()
        planes.append((n, rng.uniform(0.55, 0.82)))
    off = Vector((seed * 3.1, seed * 1.7, 0.3))
    pos = []
    for u in V:
        p = u * (1 + amp * noise.noise(u * 1.6 + off) + 0.04 * noise.noise(u * 5 + off))
        for n, d in planes:
            t = p.dot(n) - d
            if t > 0:
                p = p - n * t
        q = Vector((p.x * half[0], p.y * half[1], p.z * half[2]))
        q.x += lean * ((q.z / half[2] + 1) * 0.5) ** 2
        pos.append(q)
    zmin = min(q.z for q in pos)
    ids = []
    for q, u in zip(pos, V):
        if bottom is not None:
            q = q + Vector((0, 0, bottom - zmin))
        q = q + Vector(center)
        if flat_below is not None and q.z < flat_below:
            q.z = flat_below
        ids.append(mb.v(q, (lambda w, u=u: colfn(w, u)) if colfn else C("888888")))
    for a, b, c in F:
        mb.f((ids[a], ids[b], ids[c]), None, AUTO)
    return ids


def tuft(mb, pos, rng, n=7, h=0.25, spread=0.07, col0=None, col1=None, alpha=0.0):
    """Grass tuft: thin 3-sided blades."""
    col0 = col0 or C("3F6B24")
    col1 = col1 or C("8DBA45")
    pos = Vector(pos)
    for _ in range(n):
        a = rng.random() * 2 * math.pi
        r = rng.random() * spread
        base = pos + Vector((r * math.cos(a), r * math.sin(a), 0))
        hh = h * rng.uniform(0.55, 1.1)
        lean = Vector((math.cos(a), math.sin(a), 0)) * hh * rng.uniform(0.15, 0.5)
        tipp = base + Vector((0, 0, hh)) + lean
        w = 0.016 + 0.01 * rng.random()
        b = [base + Vector((w * math.cos(a + k * 2.094), w * math.sin(a + k * 2.094), 0)) for k in range(3)]
        ids = [mb.v(p, with_a(col0, 0.0)) for p in b]
        t = mb.v(tipp, with_a(mix(col1, C("C9D16A"), rng.random() * 0.3), alpha))
        for k in range(3):
            mb.f_out((ids[k], ids[(k + 1) % 3], t), base + Vector((0, 0, hh * 0.3)), None, SMOOTH)


def leaf_blob(mb, c, r, rng, col, sub=0, flat=0.55, alpha=0.0):
    rot = (Matrix.Rotation(rng.random() * 6.28, 3, 'Z') @ Matrix.Rotation(rng.uniform(-0.6, 0.6), 3, 'X'))
    mb.blob(c, (r, r * rng.uniform(0.8, 1.0), r * flat), sub=sub, amp=0.1, seed=rng.random() * 40,
            col=with_a(col, alpha), sh=FLAT, rot=rot)


def flame(mb, c, h, r, col_out=FLAME_O, seed=0.0, twist=0.6):
    """Stylised flame tongue (emissive: alpha 1)."""
    prof = [(0, 0), (r * 0.7, 0.03 * h), (r, 0.2 * h), (r * 0.92, 0.4 * h), (r * 0.62, 0.62 * h), (r * 0.3, 0.82 * h),
            (0, h)]
    s0 = len(mb.V)
    with mb.xf(T(*c)):
        mb.lathe(prof, 7, colfn=lambda rr, z, a, i: mix(FLAME_Y, col_out, smoothstep(0.1 * h, 0.8 * h, z)),
                 sh=SMOOTH, phase=seed)
    cz = Vector(c).z

    def tw(p):
        t = (p.z - cz) / h
        a = twist * t * math.sin(seed * 3 + t * 4)
        dx = 0.25 * r * math.sin(t * 5.0 + seed) * t
        x, y = p.x - c[0], p.y - c[1]
        return Vector((c[0] + x * math.cos(a) - y * math.sin(a) + dx, c[1] + x * math.sin(a) + y * math.cos(a), p.z))
    mb.deform(s0, tw)


# ================================================================================================ roofs
def hip_roof(mb, n, R_, rtop, z0, rise, lift, flare, thick, urise, cols, nu=6, nv=5, nvu=2, rot=0.0,
             ripple=0.0, powv=1.6):
    """Curved hip roof over a regular n-gon (circumradius R_ at the eave, rtop at the top ring). Closed shell.
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
        s = (R_ + (rtop - R_) * v) * (1 + flare * cf * cf * (1 - v) ** 2)
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
            fc = cols["soffit_edge"] if j == 0 else (cols["soffit_dk"] if (ci % 2 == 0) else cols["soffit"])
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


def gable(mb, x_half, y0, y1, z_eave, z_ridge, thick, col, rows=0, row_col=None, overhang=0.0):
    """Two sloped slabs (ridge along Y). rows > 0 adds overlapping shingle/tile rows."""
    for sx in (-1, 1):
        p0 = Vector((0.0, 0.0, z_ridge))
        p1 = Vector((sx * x_half, 0.0, z_eave))
        d = (p1 - p0).normalized()
        nrm = Vector((sx * (z_ridge - z_eave), 0, x_half)).normalized()
        if rows <= 0:
            a, b = p0 + nrm * thick * 0.5 - d * 0.03, p1 + nrm * thick * 0.5
            mb.beam((a.x, (y0 + y1) / 2, a.z), (b.x, (y0 + y1) / 2, b.z), y1 - y0, thick, col, up=nrm, bevel=0.01)
        else:
            L = (p1 - p0).length
            for r in range(rows):
                t0, t1 = r / rows, min(1.0, (r + 1.25) / rows)
                a = p0 + d * (L * t0) + nrm * (thick * 0.5 + 0.02 * (rows - r) / rows)
                b = p0 + d * (L * t1) + nrm * (thick * 0.5 + 0.02 * (rows - r) / rows)
                c = row_col(r) if row_col else col
                mb.beam((a.x, (y0 + y1) / 2, a.z), (b.x, (y0 + y1) / 2, b.z), y1 - y0, thick, c, up=nrm, bevel=0.01)


# ================================================================================================ WILDS
def make_tent_a():
    mb = MB("SM_KG_Tent_A", angle=40)
    rng = random.Random(1)
    L, W, H = 2.3, 1.08, 1.55        # length (Y), half width (X), ridge height; door at -Y
    y0, y1 = -L / 2, L / 2
    canvas, canvas_d = C("EFE0BC"), C("CDB68C")
    stripe, stripe2, patch = C("D0462C"), C("2F7F9A"), C("3E7AA8")
    inner = C("A58760")

    def ccol(side):
        def f(i, j, p):
            v = abs(p.x) / W
            c = mix(canvas, canvas_d, 0.3 + 0.3 * nz(p, 2.2, (side, 3, 1)))
            c = mix(c, C("8A7050"), smoothstep(0.9, 1.0, v) * 0.7)
            return c
        return f

    def icol(i, j, p):
        return mix(inner, C("6B5238"), smoothstep(0.4, 1.0, abs(p.x) / W) * 0.5)

    def spt(side, u, v, lift=0.0):
        n_in = Vector((-side * H, 0, -W)).normalized()
        y = lerp(y0 - 0.1, y1 + 0.1, u)
        sag = 0.06 * math.sin(math.pi * u) * math.sin(math.pi * v) + 0.02 * math.sin(math.pi * v)
        return Vector((side * W * v, y, H * (1 - v) + 0.015)) + n_in * (sag - lift)

    nu, nv = 10, 8
    for side in (-1, 1):
        grid = [[spt(side, j / nu, i / nv) for j in range(nu + 1)] for i in range(nv + 1)]
        if side < 0:
            mb.sheet2(grid, ccol(side), icol)
        else:
            mb.sheet2([r[::-1] for r in grid], ccol(side), icol)
        # crisp painted stripes + a sewn patch, as thin overlays just above the canvas
        for (va, vb, c) in ((0.64, 0.78, stripe), (0.81, 0.86, stripe2)):
            g = [[spt(side, j / nu, lerp(va, vb, i)) for j in range(nu + 1)] for i in (0, 1)]
            g = [[p + (spt(side, 0.5, 0.5, 0.006) - spt(side, 0.5, 0.5)) for p in r] for r in g]
            mb.sheet2(g if side < 0 else [r[::-1] for r in g], lambda i, j, p, c=c: c, lambda i, j, p: inner)
        if side > 0:
            g = [[spt(side, lerp(0.3, 0.45, a), lerp(0.28, 0.46, b), 0.007) for a in (0, 1)] for b in (0, 1)]
            mb.sheet2([r[::-1] for r in g], lambda i, j, p: patch, lambda i, j, p: inner)
    # back panel (double sided)
    mb.poly2([(0, y1 + 0.02, H - 0.02), (-W * 0.98, y1 + 0.02, 0.02), (W * 0.98, y1 + 0.02, 0.02)],
             lambda p: inner, lambda p: mix(canvas, canvas_d, 0.4))
    # front: left flap hanging (slightly open), right flap rolled back
    grid = []
    for i in range(7):
        v = i / 6
        row = []
        for j in range(5):
            u = j / 4
            zt = H * (1 - v)
            x = -W * v * u - 0.02 * (1 - u)
            y = y0 - 0.02 - 0.06 * u * v + 0.02 * math.sin(v * 5)
            row.append(Vector((x, y, zt * 1.0 + 0.01)))
        grid.append(row)
    mb.sheet2(grid, lambda i, j, p: mix(canvas, canvas_d, 0.2 + 0.4 * (j / 4)), lambda i, j, p: inner)
    roll = [Vector((0.06 + W * 0.92 * t, y0 - 0.05, H * (1 - t) * 0.97 + 0.04)) for t in np.linspace(0.02, 0.95, 8)]
    mb.tube(roll, [0.06 - 0.02 * t for t in np.linspace(0, 1, 8)], 7, col=mix(canvas, canvas_d, 0.5))
    mb.tube([Vector((0.5, y0 - 0.1, H * 0.55 + 0.1)), Vector((0.46, y0 - 0.05, H * 0.5))], 0.012, 4, col=stripe)
    # ground sheet + bedroll + lantern inside
    mb.cbox((0, 0, 0.012), (2 * W * 0.9, L * 0.95, 0.02), 0.005, C("3D5A4A"))
    mb.cbox((-0.36, 0.25, 0.07), (0.6, 1.5, 0.1), 0.03, C("8E3B2F"))
    mb.cbox((-0.36, 0.86, 0.13), (0.45, 0.25, 0.1), 0.04, C("E9E0CB"))
    mb.tube([(0.45, 0.6, 0.12), (0.45, -0.1, 0.12)], 0.1, 8, col=C("5B6E3A"))
    # poles, ridge, guy ropes, pegs
    for yy in (y0 - 0.04, y1 + 0.04):
        mb.tube([(0, yy, 0), (0, yy, H + 0.14)], 0.03, 6, col=WOOD_D)
    mb.tube([(0, y0 - 0.16, H - 0.02), (0, y1 + 0.16, H - 0.02)], 0.028, 6, col=WOOD)

    def peg(p, d):
        p = Vector(p)
        mb.beam(p - Vector((0, 0, 0.02)), p + Vector((0, 0, 0.12)) - Vector(d) * 0.04, 0.035, 0.035, WOOD_L,
                up=(1, 0, 0) if abs(d[0]) < 0.5 else (0, 1, 0))
    for sy, yy in ((-1, y0 - 0.04), (1, y1 + 0.04)):
        a, b = Vector((0, yy, H + 0.1)), Vector((0, yy + sy * 0.95, 0.04))
        mb.tube([a, b], 0.009, 4, col=ROPE)
        peg(b, (0, sy, 0))
    for sx in (-1, 1):
        for yy in (-0.6, 0.6):
            a = Vector((sx * W * 0.72, yy, H * 0.28 + 0.02))
            b = Vector((sx * (W + 0.5), yy, 0.04))
            mb.tube([a, b], 0.008, 4, col=ROPE)
            peg(b, (sx, 0, 0))
    return mb


def make_campfire():
    mb = MB("SM_KG_Campfire", angle=30, material=MAT_GLOW)
    rng = random.Random(3)

    def scol(p, u):
        return lichen(p, granite(p, base="8E8A84", light="B1ADA5", dark="5E5A55", seed=3.0, scale=2.0), 0.2, 3.0)

    n = 11
    for k in range(n):
        a = 2 * math.pi * k / n + rng.uniform(-0.08, 0.08)
        r = 0.55 + rng.uniform(-0.03, 0.03)
        with mb.xf(T(r * math.cos(a), r * math.sin(a), 0) @ R(math.degrees(a) + 90, 'Z')):
            s0 = len(mb.V)
            rock(mb, (0, 0, 0), (0.15 * rng.uniform(0.9, 1.25), 0.12 * rng.uniform(0.9, 1.2),
                                 0.11 * rng.uniform(0.8, 1.3)), seed=k + 10, sub=1, cuts=5, amp=0.12, colfn=scol,
                 bottom=-0.02)
            # soot on the inner faces
            mb.recolor(s0, lambda p, c: mul(c, 0.55 + 0.45 * smoothstep(0.45, 0.62, Vector((p.x, p.y)).length)))
    # ash bed with ember spots
    def ash(r, z, a, i):
        p = Vector((r * math.cos(a), r * math.sin(a), z))
        e = smoothstep(0.1, 0.45, nz(p, 7.0, (2, 2, 2))) * (1 - smoothstep(0.2, 0.42, r))
        return mix(C("3A3532"), EMBER, e)
    mb.lathe([(0, 0.005), (0.46, 0.005), (0.47, 0.02), (0.36, 0.045), (0.18, 0.06), (0, 0.065)], 14, colfn=ash,
             sh=SMOOTH)
    # teepee logs: bark outside, charred inside, glowing ends
    for k in range(6):
        a = 2 * math.pi * k / 6 + 0.3
        p0 = Vector((0.4 * math.cos(a), 0.4 * math.sin(a), 0.05))
        p1 = Vector((0.06 * math.cos(a + 0.4), 0.06 * math.sin(a + 0.4), 0.42))
        pts = [p0.lerp(p1, t) for t in (0, 0.33, 0.66, 1.0)]

        def lc(i, kk, p, k=k):
            t = i / 3
            c = mix(bark(p, seed=k), C("1E1A18"), smoothstep(0.25, 0.7, t))
            return mix(c, EMBER, smoothstep(0.75, 1.0, t) * (0.6 + 0.4 * (kk % 2)))
        mb.tube(pts, [0.055, 0.052, 0.048, 0.045], 7, colfn=lc, sh=AUTO, capcol=C("1E1A18"), tip=0.04)
    # flames
    flame(mb, (0.0, 0.0, 0.08), 0.62, 0.17, seed=0.5)
    flame(mb, (0.11, -0.07, 0.07), 0.42, 0.11, seed=2.1)
    flame(mb, (-0.1, -0.05, 0.07), 0.36, 0.1, seed=4.0)
    flame(mb, (-0.02, 0.12, 0.07), 0.45, 0.11, seed=5.3)
    # spit: two forked sticks + crossbar + kettle
    for sx in (-1, 1):
        x = sx * 0.78
        mb.tube([(x, 0, -0.05), (x + sx * 0.02, 0, 0.9)], [0.022, 0.018], 5, col=C("7A5A3C"))
        mb.tube([(x + sx * 0.02, 0, 0.78), (x + sx * 0.09, 0.02, 0.98)], [0.014, 0.01], 5, col=C("7A5A3C"))
        mb.tube([(x + sx * 0.01, 0, 0.8), (x - sx * 0.06, -0.02, 0.97)], [0.014, 0.01], 5, col=C("7A5A3C"))
    mb.tube([(-0.92, 0.0, 0.9), (0.92, 0.0, 0.9)], 0.017, 6, col=C("8C6A48"))
    mb.tube([(0, 0, 0.9), (0, 0, 0.8)], 0.006, 4, col=IRON)
    with mb.xf(T(0, 0, 0.62)):
        mb.lathe([(0, 0), (0.08, 0), (0.12, 0.04), (0.13, 0.1), (0.1, 0.15), (0.06, 0.16), (0.06, 0.17),
                  (0, 0.175)], 12, col=IRON, sh=AUTO)
        mb.tube([(0.1, 0, 0.13), (0.19, 0, 0.19)], [0.018, 0.01], 5, col=IRON)
        mb.tube([(-0.09, 0, 0.16), (0, 0, 0.22), (0.09, 0, 0.16)], 0.006, 4, col=IRON_L)
    # spare logs
    for (x, y, yaw) in ((0.95, 0.45, 20), (0.98, 0.62, 35)):
        with mb.xf(T(x, y, 0.07) @ R(yaw, 'Z')):
            mb.tube([(-0.28, 0, 0), (0.28, 0, 0)], 0.065, 8, colfn=lambda i, k, p: bark(p, seed=7), sh=AUTO,
                    capcol=C("C9A06A"))
    return mb


def make_menhir(name, H, W, D, seed, lean=0.0, notch=0.0, spiral=False, taper=0.35, stone="8A8F99"):
    """Standing stone: rings of an irregular superellipse section, tapering and rounding at the top, cut by random
    vertical planes for chunky facets; sunk 12 cm. notch > 0 splits the top into two horns."""
    mb = MB(name, angle=24)
    rng = random.Random(seed)
    nseg = 16
    prof = [(0.0, 1.06), (0.06, 1.0), (0.2, 0.98), (0.38, 0.95), (0.55, 0.91), (0.7, 0.85), (0.82, 0.76), (0.9, 0.64),
            (0.96, 0.46), (1.0, 0.2)]
    planes = []
    for i in range(8):
        n = rand_unit(rng)
        n.z *= 0.3
        n.normalize()
        planes.append((n, rng.uniform(0.74, 0.9)))
    off = Vector((seed * 1.3, seed * 0.7, 0.0))

    def sgp(x, e):
        return math.copysign(abs(x) ** e, x)

    def place(t, sc, a):
        cx, sy_ = math.cos(a), math.sin(a)
        q = Vector((sgp(cx, 0.55) * sc, sgp(sy_, 0.55) * sc, t * 2 - 1))
        q *= 1 + 0.07 * nz(q, 1.5, off)
        for n, d in planes:
            k = q.dot(n) - d
            if k > 0:
                q = q - n * k
        tap = 1 - taper * t
        x = q.x * W / 2 * tap + lean * (t ** 1.6) * H
        y = q.y * D / 2 * (1 - taper * 0.4 * t)
        z = t * H + 0.04 * nz(q, 3.0, off)
        if notch > 0:
            z -= notch * max(0.0, 1 - abs(cx) / 0.95) ** 0.7 * smoothstep(0.5, 1.0, t)
        return Vector((x, y, z))

    def col(w):
        c = granite(w, base=stone, light="AEB3BC", dark="5F646D", moss_amt=0.95,
                    moss_up=1 - smoothstep(0.0, 0.7, w.z), seed=seed, scale=0.9)
        c = lichen(w, c, 0.9, seed, 2.5)
        base_moss = (1 - smoothstep(0.0, 0.45 + 0.2 * nz(w, 2.0, (seed, 4, 4)), w.z)) * 0.85
        c = mix(c, with_a(mix(MOSS, MOSS_L, 0.4 + 0.4 * nz(w, 5.0)), 0.0), base_moss)
        band = 0.93 + 0.07 * math.sin(w.z * 6 + nz(w, 1.1) * 2)
        return mul(c, band * (0.82 + 0.18 * smoothstep(0.0, 1.2, w.z)))

    s0 = len(mb.V)
    rings = []
    for (t, sc) in prof:
        rings.append([mb.v(place(t, sc, 2 * math.pi * j / nseg + 0.2), col) for j in range(nseg)])
    for i in range(len(rings) - 1):
        for j in range(nseg):
            jn = (j + 1) % nseg
            mb.f((rings[i][j], rings[i][jn], rings[i + 1][jn], rings[i + 1][j]), None, AUTO)
    topc = place(1.0, 0.0, 0.0)
    topc.z = H - (notch * 0.9 if notch else -0.03)
    top = mb.v(topc, col)
    for j in range(nseg):
        mb.f((rings[-1][j], rings[-1][(j + 1) % nseg], top), None, AUTO)
    mb.f(list(reversed(rings[0])), None, FLAT)
    for i in range(s0, len(mb.V)):
        mb.V[i].z -= 0.12
    mb.cavity(s0, dark=0.5, light=0.25)
    moss_tops(mb, 0, amt=0.9, thr=0.5, seed=seed, freq=2.0)
    if spiral:
        bvh = BVHTree.FromPolygons([tuple(v) for v in mb.V], [tuple(f) for f in mb.F])
        ochre = C("B4432A")

        def project(x, z):
            hit = bvh.ray_cast(Vector((x, -5.0, z)), Vector((0, 1, 0)))
            if hit[0] is None:
                return None
            return hit[0] + hit[1] * 0.006
        for (cx, cz, turns, r0, r1) in ((0.02, H * 0.52, 2.6, 0.03, 0.34), (-0.1, H * 0.28, 1.8, 0.02, 0.16)):
            pts = []
            for k in range(int(turns * 26)):
                th = k / 26 * 2 * math.pi
                r = r0 + (r1 - r0) * th / (turns * 2 * math.pi)
                p = project(cx + r * math.cos(th), cz + r * math.sin(th))
                if p is not None:
                    pts.append(p)
            if len(pts) > 3:
                mb.tube(pts, 0.022, 4, col=ochre, sh=SMOOTH)
        for k in range(5):
            p = project(-0.25 + 0.12 * k, H * 0.73)
            if p is not None:
                mb.blob(p, 0.03, sub=0, amp=0.0, col=ochre)
    # foot stones + grass
    for k in range(4):
        a = rng.random() * 6.28
        r = W * 0.5 + rng.uniform(0.05, 0.25)
        rock(mb, (r * math.cos(a), r * math.sin(a) * 0.7, 0), (0.18, 0.14, 0.1), seed=seed * 7 + k, sub=1, cuts=4,
             colfn=lambda w, u: granite(w, base=stone, seed=seed, moss_amt=0.5, moss_up=smoothstep(0.2, 0.9, u.z)),
             bottom=-0.05)
    for k in range(6):
        a = rng.random() * 6.28
        r = W * 0.45 + rng.uniform(0.0, 0.2)
        tuft(mb, (r * math.cos(a), r * math.sin(a) * 0.6, -0.02), rng, n=6, h=0.28)
    return mb


# ================================================================================================ CHURCH YARD
def grave_stone(p, base="B7B1A5", light="CFC9BD", dark="8D877D", seed=0.0, moss=0.6):
    c = granite(p, base=base, light=light, dark=dark, moss_amt=moss, moss_up=1 - smoothstep(0.0, 0.35, p.z), seed=seed,
                scale=2.0)
    c = lichen(p, c, 0.45, seed, 7.0)
    return mul(c, 0.8 + 0.2 * smoothstep(0.0, 0.6, p.z))


def engrave(mb, y, z0, lines, w=0.34, col=None):
    col = col or C("4A4640")
    z = z0
    for k, frac in enumerate(lines):
        mb.box((0, y, z), (w * frac, 0.006, 0.024), col)
        z -= 0.06


def make_grave_round():
    mb = MB("SM_KG_Gravestone_Round", angle=30)
    col = lambda p: grave_stone(p, seed=1.0)
    mb.cbox((0, 0.02, 0.07), (0.74, 0.32, 0.14), 0.02, col)
    out = [(-0.3, 0.12), (0.3, 0.12), (0.3, 0.74)]
    for k in range(1, 12):
        a = math.pi * k / 12
        out.append((0.3 * math.cos(a), 0.74 + 0.3 * math.sin(a)))
    out.append((-0.3, 0.74))
    mb.prism(out, -0.065, 0.065, col, bevel=0.022)
    # raised border line + engraving
    yF = -0.066
    arc = [Vector((0.25 * math.cos(math.pi * k / 16), yF, 0.74 + 0.25 * math.sin(math.pi * k / 16))) for k in range(17)]
    mb.tube(arc, 0.012, 4, col=C("9C968A"))
    mb.box((0, yF - 0.002, 0.83), (0.03, 0.008, 0.14), C("4A4640"))
    mb.box((0, yF - 0.002, 0.85), (0.1, 0.008, 0.03), C("4A4640"))
    engrave(mb, yF - 0.002, 0.66, [0.8, 1.0, 0.55, 0.7])
    rng = random.Random(4)
    for k in range(5):
        tuft(mb, (rng.uniform(-0.4, 0.4), rng.uniform(-0.2, 0.2), 0), rng, n=5, h=0.2)
    return mb


def make_grave_cross():
    mb = MB("SM_KG_Gravestone_Cross", angle=30)
    col = lambda p: grave_stone(p, base="A9A7A0", seed=2.0)
    mb.cbox((0, 0, 0.08), (0.62, 0.42, 0.16), 0.02, col)
    mb.cbox((0, 0, 0.23), (0.46, 0.3, 0.14), 0.02, col)
    mb.cbox((0, 0, 0.3 + 0.5), (0.15, 0.13, 1.0), 0.02, col)
    mb.cbox((0, 0, 1.02), (0.58, 0.13, 0.15), 0.02, col)
    mb.box((0, -0.151, 0.23), (0.28, 0.006, 0.05), C("4A4640"))
    mb.box((0, -0.066, 0.62), (0.05, 0.006, 0.4), C("8A857B"))
    rng = random.Random(5)
    for k in range(4):
        tuft(mb, (rng.uniform(-0.35, 0.35), rng.uniform(-0.25, 0.25), 0), rng, n=5, h=0.2)
    return mb


# ================================================================================================ ICONIC: cave mouth
def make_cave():
    mb = MB("SM_KG_CaveMouth", angle=24)
    Y0, Y1 = -1.3, 3.0            # tunnel start (inside the rock face) / back

    def wt(t):
        return 1.55 * (1 - 0.35 * t)

    def ht(t):
        return 3.2 * (1 - 0.32 * t)

    def zs(t):
        return ht(t) - wt(t) * 0.95

    def prof_at(y):
        """(w, h, s) of the carved opening at depth y (flares out in front of the tunnel)."""
        t = clamp((y - Y0) / (Y1 - Y0))
        fl = 1.0 + 0.32 * smoothstep(Y0, Y0 - 1.6, y)
        w, h = wt(t) * fl, ht(t) * (1.0 + 0.18 * (fl - 1))
        return w, h, h - w * 0.95

    def carve(p, margin):
        if p.y > Y1 or p.z < -0.05:
            return p
        w, h, s = prof_at(p.y)
        w, h = w * margin, h * margin
        s = h - w * 0.95
        x, z = p.x, p.z
        if z <= s:
            if abs(x) < w:
                return Vector((math.copysign(w, x if abs(x) > 1e-4 else 1.0), p.y, z))
            return p
        ex, ez = x / w, (z - s) / (h - s)
        r = math.hypot(ex, ez)
        if r < 1.0:
            if r < 1e-4:
                ex, ez, r = 0.0, 1.0, 1.0
            return Vector((ex / r * w, p.y, s + ez / r * (h - s)))
        return p

    def rockcol(w, u):
        c = granite(w, base="9C8E7E", light="B9AB97", dark="6B5F55", seed=5.0, scale=0.6)
        band = 0.5 + 0.5 * math.sin(w.z * 3.2 + 1.5 * nz(w, 0.8, (4, 4, 4)))
        c = mul(c, 0.9 + 0.14 * band)
        c = lichen(w, c, 0.25, 5.0, 2.0)
        g = smoothstep(0.35, 0.85, u.z) * smoothstep(1.2, 2.6, w.z)
        grass = mix(C("5C8E34"), C("86B447"), 0.5 + 0.5 * nz(w, 2.0, (1, 1, 1)))
        return with_a(mix(c, grass, g * smoothstep(-0.3, 0.2, nz(w, 0.9, (7, 1, 3)))), 0.0)

    rocks = [  # centre, half size, seed, sub
        ((-2.85, -0.1, 1.9), (1.45, 1.9, 2.25), 1, 3), ((2.75, 0.1, 1.7), (1.35, 2.0, 2.05), 2, 3),
        ((0.1, -0.35, 4.05), (2.8, 1.8, 1.2), 3, 3), ((0.3, 1.9, 3.5), (3.3, 2.4, 2.1), 4, 3),
        ((-2.5, 2.4, 1.7), (1.7, 1.7, 1.8), 5, 2), ((2.6, 2.2, 1.5), (1.7, 1.8, 1.6), 6, 2),
        ((-2.35, -2.25, 0.35), (0.7, 0.55, 0.55), 7, 2), ((2.45, -2.1, 0.28), (0.55, 0.5, 0.42), 8, 2),
        ((3.45, -1.25, 0.45), (0.6, 0.6, 0.6), 9, 2), ((-3.6, -1.3, 0.6), (0.7, 0.8, 0.7), 10, 2),
        ((-1.2, -1.2, 5.0), (1.1, 0.9, 0.55), 11, 2),
    ]
    rock_ids = []
    for cen, half, seed, sub in rocks:
        ids = rock(mb, cen, half, seed, sub=sub, cuts=10, amp=0.13, colfn=rockcol, flat_below=-0.3)
        rock_ids.append(ids)
    # carve the opening through every rock (vertices pushed onto the arch, darker the deeper they sit)
    for ids in rock_ids:
        for i in ids:
            p = mb.V[i]
            inside = p.y > Y0 - 0.05
            q = carve(p, 1.035 if inside else 1.0)
            if (q - p).length > 1e-6:
                mb.V[i] = q
                d = clamp((q.y - (Y0 - 1.6)) / (Y1 - Y0 + 1.6))
                mb.VC[i] = mul(mb.VC[i], 0.75 - 0.55 * d)
    mb.cavity(0, dark=0.55, light=0.25)
    # tunnel shell: inward faces, darkening to black
    nr, na = 14, 20

    def arch(t):
        w, h, s = wt(t), ht(t), zs(t)
        pts = [(w, 0.0), (w * 1.02, s * 0.5)]
        for k in range(na + 1):
            ph = math.pi * k / na
            pts.append((w * math.cos(ph), s + (h - s) * math.sin(ph)))
        pts += [(-w * 1.02, s * 0.5), (-w, 0.0)]
        out = []
        for j, (x, z) in enumerate(pts):
            jit = 1.0 + 0.07 * nz((x * 1.3, t * 4.0, z * 1.3), 1.0, (2, 2, 2))
            out.append((x * jit, 0.0 if j in (0, len(pts) - 1) else z * (1 + 0.03 * nz((x, t * 3, z), 2.0))))
        return out

    rings = []
    for i in range(nr + 1):
        t = i / nr
        y = lerp(Y0, Y1, t)
        k = 1 - smoothstep(0.0, 0.9, t)
        ring = []
        for (x, z) in arch(t):
            c = granite((x, y, z), base="8A7D70", light="A09282", dark="5A5048", seed=5.0, scale=0.6)
            c = mix(BLACK, mul(c, 0.55), k ** 1.4)
            ring.append(mb.v((x, y, z), c))
        rings.append(ring)
    for i in range(nr):
        A, B = rings[i], rings[i + 1]
        for j in range(len(A) - 1):
            mb.f((A[j], A[j + 1], B[j + 1], B[j]), None, AUTO)
        mb.f((A[-1], A[0], B[0], B[-1]), None, FLAT)          # floor strip
    back = rings[-1]
    mb.f(list(back), BLACK, FLAT)
    # floor colour: dirt in front fading to black
    # stalactites under the lip
    rng = random.Random(8)
    for k in range(9):
        ph = rng.uniform(0.2, 0.8) * math.pi
        y = rng.uniform(Y0 - 0.2, Y0 + 0.7)
        w, h, s = prof_at(y)
        x, z = w * math.cos(ph) * 0.97, s + (h - s) * math.sin(ph) * 0.97
        L = rng.uniform(0.18, 0.45)
        with mb.xf(T(x, y, z + 0.05)):
            mb.lathe([(0, -L), (rng.uniform(0.05, 0.09), -0.05), (0.08, 0.1), (0, 0.12)], 5,
                     col=mul(C("8C7F72"), 0.6), sh=FLAT, phase=rng.random())
    # hanging vines over the mouth
    vine, leaf = C("3E5E22"), C("5E8F2E")
    for k in range(7):
        x = -1.5 + 3.0 * k / 6 + rng.uniform(-0.15, 0.15)
        y = Y0 - 1.1 - rng.uniform(0.0, 0.3)
        w, h, s = prof_at(y)
        ex = clamp(x / (w * 1.02), -0.98, 0.98)
        ztop = s + (h - s) * math.sqrt(1 - ex * ex) + 0.1
        L = rng.uniform(0.6, 1.6) * (1 - 0.5 * abs(ex))
        pts = [Vector((x + 0.05 * math.sin(i * 1.3 + k), y - 0.02 * i, ztop - L * i / 8)) for i in range(9)]
        mb.tube(pts, 0.012, 4, col=vine)
        for p in pts[1:]:
            for _ in range(2):
                leaf_blob(mb, p + Vector((rng.uniform(-0.06, 0.06), rng.uniform(-0.05, 0.02), rng.uniform(-0.05, 0.05))),
                          rng.uniform(0.05, 0.08), rng, mix(leaf, C("86B447"), rng.random()))
    # a broken plank barricade on the left side and a lantern hook
    plank = C("7B5A3E")
    with mb.xf(T(-0.95, Y0 - 0.55, 0.0)):
        mb.beam((-0.55, 0, 0.35), (0.35, 0, 1.55), 0.2, 0.04, plank, up=(0, 1, 0), bevel=0.008)
        mb.beam((-0.5, -0.03, 1.3), (0.25, -0.03, 0.5), 0.18, 0.04, mix(plank, C("5E4330"), 0.5), up=(0, 1, 0),
                bevel=0.008)
    mb.beam((0.2, Y0 - 0.9, 0.03), (1.1, Y0 - 1.3, 0.06), 0.2, 0.04, plank, up=(0, 0, 1), bevel=0.008)
    for k in range(10):
        tuft(mb, (rng.uniform(-3.6, 3.6), rng.uniform(-2.9, -1.6), 0.0), rng, n=7, h=0.32)
    return mb


# ================================================================================================ ICONIC: mausoleum
def make_mausoleum():
    mb = MB("SM_KG_Mausoleum", angle=30, material=MAT_GLOW)
    rng = random.Random(61)

    def st(p):
        c = granite(p, base="C7C0B2", light="DCD5C8", dark="958E83", seed=2.0, scale=0.9)
        c = mul(c, 1 - 0.3 * (1 - smoothstep(0.2, 1.3, p.z)))
        c = mul(c, 0.93 + 0.07 * math.sin(p.x * 9 + 2 * nz(p, 1.3)))
        return lichen(p, c, 0.2, 2.0, 4.0)

    def st_dk(p):
        return mul(st(p), 0.82)

    roofc = lambda p: with_a(mix(C("5E6B72"), C("7A878C"), 0.5 + 0.5 * nz(p, 2.0)), 0.0)
    X, YF, YB, Z0, Z1 = 1.8, -1.7, 2.4, 0.3, 3.1
    f0 = len(mb.F)
    # platform + step
    mb.cbox((0, 0.2, 0.15), (4.3, 5.0, 0.3), 0.03, st_dk)
    mb.cbox((0, -2.5, 0.075), (2.3, 0.42, 0.15), 0.02, st_dk)
    # body walls (sides, back, top) as single faces
    for sx in (-1, 1):
        ids = [mb.v(p, st) for p in ((sx * X, YF, Z0), (sx * X, YB, Z0), (sx * X, YB, Z1), (sx * X, YF, Z1))]
        mb.f_dir(ids, (sx, 0, 0), None, FLAT)
    ids = [mb.v(p, st) for p in ((-X, YB, Z0), (X, YB, Z0), (X, YB, Z1), (-X, YB, Z1))]
    mb.f_dir(ids, (0, 1, 0), None, FLAT)
    ids = [mb.v(p, st) for p in ((-X, YF, Z1), (X, YF, Z1), (X, YB, Z1), (-X, YB, Z1))]
    mb.f_dir(ids, (0, 0, 1), None, FLAT)
    # ashlar courses on the side/back walls (thin raised bands)
    for z in np.arange(Z0 + 0.45, Z1 - 0.1, 0.45):
        for sx in (-1, 1):
            mb.box((sx * (X + 0.008), (YF + YB) / 2, z), (0.02, YB - YF, 0.035), st_dk)
        mb.box((0, YB + 0.008, z), (2 * X, 0.02, 0.035), st_dk)
    # front facade with the arched door recess
    dw, dsp = 0.62, 1.62
    hole = [(X - dw, 0.0), (X + dw, 0.0)]
    for k in range(0, 15):
        a = math.pi * k / 14
        hole.append((X + dw * math.cos(a), dsp + dw * math.sin(a)))
    mb.hole_wall((-X, YF, Z0), (1, 0, 0), (0, 0, 1), 2 * X, Z1 - Z0, hole, st, depth=0.75,
                 wall_col=lambda t: mix(C("4A4540"), C("0E0C0B"), t), back_col=C("060505"))
    # door surround (raised band following the arch)
    band = [Vector((-dw - 0.08, YF - 0.03, Z0))] + [
        Vector(((dw + 0.08) * math.cos(math.pi - math.pi * k / 16), YF - 0.03,
                Z0 + dsp + (dw + 0.08) * math.sin(math.pi - math.pi * k / 16))) for k in range(17)] + [
        Vector((dw + 0.08, YF - 0.03, Z0))]
    mb.sweep(band, [(-0.07, -0.04), (0.07, -0.04), (0.07, 0.04), (-0.07, 0.04)], col=lambda p: mul(st(p), 1.08),
             vertical=False, up=(0, -1, 0))
    mb.cbox((0, YF - 0.06, Z0 + dsp + dw + 0.12), (0.2, 0.12, 0.26), 0.02, st)      # keystone
    # iron grille doors: left closed, right ajar
    def arch_top(x):
        return Z0 + dsp + math.sqrt(max(0.0, dw * dw - x * x)) - 0.04

    def grille(hx, sgn, open_deg):
        """Leaf hinged at x=hx, reaching towards the centre when closed; opens inwards (+Y)."""
        a = math.radians(open_deg)
        d = Vector((-sgn * math.cos(a), math.sin(a), 0))
        L = dw - 0.03
        base = Vector((hx, YF + 0.14, 0))
        for k in range(6):
            s = L * (k + 0.5) / 6
            q = base + d * s
            zt = arch_top(clamp(hx - sgn * s, -dw + 0.03, dw - 0.03))
            mb.tube([(q.x, q.y, Z0 + 0.03), (q.x, q.y, zt)], 0.017, 5, col=IRON, sh=AUTO)
        for z in (Z0 + 0.15, Z0 + 1.0, Z0 + 1.55):
            p1 = base + d * L
            mb.tube([(base.x, base.y, z), (p1.x, p1.y, z)], 0.02, 5, col=IRON_L, sh=AUTO)
    grille(-dw + 0.02, -1, 0)
    grille(dw - 0.02, 1, 58)
    # corner pilasters with capitals
    for sx in (-1, 1):
        mb.cbox((sx * (X - 0.17), YF + 0.17, Z0 + 1.4), (0.44, 0.44, 2.8), 0.02, st)
        mb.cbox((sx * (X - 0.17), YF + 0.17, Z1 - 0.1), (0.54, 0.54, 0.16), 0.02, st_dk)
        mb.cbox((sx * (X - 0.17), YB - 0.17, Z0 + 1.4), (0.44, 0.44, 2.8), 0.02, st)
    # portico: two columns + entablature with a name plaque
    for sx in (-1, 1):
        with mb.xf(T(sx * 1.05, YF - 0.42, Z0)):
            mb.cbox((0, 0, 0.07), (0.36, 0.36, 0.14), 0.015, st_dk)
            mb.lathe([(0, 0.14), (0.15, 0.14), (0.16, 0.18), (0.14, 0.22), (0.13, 1.4), (0.118, 2.28), (0.14, 2.32),
                      (0.17, 2.38), (0, 2.38)], 14, col=st, sh=AUTO)
            mb.cbox((0, 0, 2.43), (0.38, 0.38, 0.1), 0.015, st_dk)
    mb.cbox((0, YF - 0.25, Z0 + 2.6), (2.75, 0.62, 0.3), 0.025, st)
    mb.cbox((0, YF - 0.57, Z0 + 2.6), (1.3, 0.03, 0.19), 0.008, C("E6E0D0"))
    for k in range(9):
        mb.box((-0.48 + 0.12 * k, YF - 0.588, Z0 + 2.6), (0.07, 0.008, 0.08), C("3E3A34"))
    # cornice
    mb.cbox((0, (YF + YB) / 2, Z1 + 0.09), (3.95, 4.5, 0.2), 0.03, st_dk)
    mb.cbox((0, (YF + YB) / 2 - 0.0, Z1 + 0.22), (4.1, 4.66, 0.08), 0.02, st)
    # pediments (front + back) and the stone slab roof
    zp = Z1 + 0.26
    tri = [(-2.02, 0.0), (2.02, 0.0), (0.0, 1.05)]
    for (ya, yb) in ((YF - 0.33, YF - 0.05), (YB + 0.05, YB + 0.33)):
        with mb.xf(T(0, 0, zp)):
            mb.prism(tri, ya, yb, st, bevel=0.02)
    fr = len(mb.F)
    with mb.xf(T(0, 0, zp)):
        gable(mb, 2.22, YF - 0.45, YB + 0.45, -0.1, 1.1, 0.13, roofc, rows=6,
              row_col=lambda r: (lambda p, r=r: mul(roofc(p), 0.92 + 0.08 * (r % 2))))
    moss_tops(mb, fr, amt=0.9, thr=0.3, seed=3.0, freq=1.3, bias=-0.05)
    mb.tube([(0, YF - 0.46, zp + 1.22), (0, YB + 0.46, zp + 1.22)], 0.07, 6, col=C("56626A"), sh=AUTO)
    # tympanum medallion with a cross, apex cross, corner acroteria
    with mb.xf(T(0, YF - 0.34, zp + 0.42) @ R(90, 'X')):
        mb.lathe([(0, -0.02), (0.3, -0.02), (0.3, 0.04), (0.24, 0.07), (0, 0.07)], 16, col=st, sh=AUTO)
    mb.box((0, YF - 0.42, zp + 0.42), (0.07, 0.04, 0.36), C("8E877C"))
    mb.box((0, YF - 0.42, zp + 0.48), (0.22, 0.04, 0.07), C("8E877C"))
    mb.cbox((0, YF - 0.2, zp + 1.25), (0.3, 0.3, 0.2), 0.02, st)
    mb.cbox((0, YF - 0.2, zp + 1.7), (0.13, 0.13, 0.8), 0.015, st)
    mb.cbox((0, YF - 0.2, zp + 1.83), (0.46, 0.13, 0.13), 0.015, st)
    for sx in (-1, 1):
        with mb.xf(T(sx * 1.98, YF - 0.2, zp)):
            mb.lathe([(0, 0), (0.14, 0), (0.14, 0.12), (0, 0.34)], 4, col=st, sh=FLAT, phase=math.pi / 4)
    # urns with red flowers + candles (glow) on the step
    for sx in (-1, 1):
        with mb.xf(T(sx * 1.6, -2.05, Z0)):
            mb.lathe([(0, 0), (0.14, 0), (0.14, 0.06), (0.07, 0.1), (0.06, 0.16), (0.13, 0.26), (0.2, 0.4),
                      (0.21, 0.5), (0.17, 0.5), (0.17, 0.47), (0, 0.47)], 12, col=st, sh=AUTO)
            for k in range(7):
                a = k * 2.4
                r = 0.05 + 0.08 * (k % 3) / 2
                leaf_blob(mb, (r * math.cos(a), r * math.sin(a), 0.55 + 0.04 * (k % 2)), 0.07, rng, C("3F6B24"))
                mb.blob((r * math.cos(a + 0.5), r * math.sin(a + 0.5), 0.61 + 0.03 * (k % 2)), 0.045, sub=0,
                        amp=0.1, col=C("D02A36") if k % 3 else C("F2EDE0"), sh=FLAT)
    for (x, y, h) in ((-0.95, -2.42, 0.22), (-0.8, -2.55, 0.14), (-1.05, -2.6, 0.1), (0.9, -2.5, 0.18),
                      (1.02, -2.4, 0.11)):
        with mb.xf(T(x, y, 0.15)):
            mb.lathe([(0, 0), (0.035, 0), (0.035, h), (0.03, h + 0.01), (0, h + 0.01)], 8, col=C("EFE6CF"), sh=AUTO)
            mb.tube([(0, 0, h + 0.005), (0, 0, h + 0.025)], 0.004, 3, col=IRON)
        flame(mb, (x, y, 0.15 + h + 0.015), 0.06, 0.018, seed=x * 7)
    # ivy up the left corner and across the cornice
    ivy = [Vector((-X - 0.05, YF - 0.1, 0.3))]
    for k in range(1, 16):
        t = k / 15
        ivy.append(Vector((-X - 0.06 + 0.35 * t * t + 0.06 * math.sin(k), YF - 0.26 + 0.05 * math.sin(k * 1.7),
                           0.3 + 3.0 * min(1.0, t * 1.15))))
    mb.tube(ivy, 0.018, 4, col=C("4A3A28"))
    for p in ivy[1:]:
        for _ in range(3):
            leaf_blob(mb, p + Vector((rng.uniform(-0.12, 0.12), rng.uniform(-0.08, 0.04), rng.uniform(-0.1, 0.1))),
                      rng.uniform(0.06, 0.1), rng, mix(C("3F6B24"), C("6E9E3A"), rng.random()))
    moss_tops(mb, f0, amt=0.75, thr=0.8, seed=1.0, freq=1.8, bias=-0.15)
    return mb


# ================================================================================================ ICONIC: tea house
def make_teahouse():
    mb = MB("SM_KG_TeaHouse", angle=35, material=MAT_GLOW)
    rng = random.Random(71)
    FZ = 0.45                                       # floor level
    post, beam_c = C("5A3B28"), C("6B4631")
    board = C("C69C6C")
    plaster = C("D9C49A")
    paper = C("F4EFE2")
    lat = C("6E4B33")
    # foundation stones + posts
    posts = [(-1.2, -0.9), (0, -0.9), (1.2, -0.9), (-1.2, 1.5), (0, 1.5), (1.2, 1.5), (-1.2, 0.3), (1.2, 0.3),
             (-1.5, -1.5), (1.5, -1.5), (-1.5, 1.5), (1.5, 1.5)]
    for (x, y) in posts:
        rock(mb, (x, y, 0), (0.17, 0.17, 0.12), seed=int((x + 3) * 10 + (y + 3) * 100), sub=1, cuts=5,
             colfn=lambda w, u: granite(w, seed=1.0, moss_amt=0.4, moss_up=u.z), bottom=-0.04)
        top = FZ + 2.17
        mb.cbox((x, y, (0.08 + top) / 2), (0.11, 0.11, top - 0.08), 0.012, post)
    # floor frame + veranda boards (along X) + tatami room
    for (c, s) in (((0, -1.5, FZ - 0.08), (3.1, 0.12, 0.14)), ((0, 1.5, FZ - 0.08), (3.1, 0.12, 0.14)),
                   ((-1.5, 0, FZ - 0.08), (0.12, 3.1, 0.14)), ((1.5, 0, FZ - 0.08), (0.12, 3.1, 0.14))):
        mb.cbox(c, s, 0.01, beam_c)
    for k in range(6):
        y = -1.5 + 0.1 * (k + 0.5)
        mb.cbox((0, y, FZ - 0.015), (3.08, 0.094, 0.03), 0.004, plank_col(rng, board, C("D9B384"), C("A87F55")))
    for sx in (-1, 1):
        for k in range(3):
            x = sx * (1.2 + 0.1 * (k + 0.5))
            mb.cbox((x, 0.3, FZ - 0.015), (0.094, 2.4, 0.03), 0.004, plank_col(rng, board, C("D9B384"), C("A87F55")))
    mats = [((-1.2, 0.4), (-0.9, -0.1)), ((0.4, 1.2), (-0.9, 0.7)), ((-0.4, 1.2), (0.7, 1.5)), ((-1.2, -0.4), (-0.1, 1.5)),
            ((-0.4, 0.4), (-0.1, 0.7))]
    for (xa, xb), (ya, yb) in mats:
        mb.cbox(((xa + xb) / 2, (ya + yb) / 2, FZ - 0.02), (xb - xa - 0.012, yb - ya - 0.012, 0.05), 0.008,
                C("C7C27A", v=0.95 + 0.1 * rng.random()))
        long_x = (xb - xa) > (yb - ya)
        for s in (-1, 1):
            if long_x:
                mb.box(((xa + xb) / 2, (ya + yb) / 2 + s * ((yb - ya) / 2 - 0.03), FZ + 0.006), (xb - xa - 0.02, 0.04, 0.005),
                       C("2E3A2A"))
            else:
                mb.box(((xa + xb) / 2 + s * ((xb - xa) / 2 - 0.03), (ya + yb) / 2, FZ + 0.006), (0.04, yb - ya - 0.02, 0.005),
                       C("2E3A2A"))
    # back wall: plaster with a round window (through), lattice in it
    wz0, wz1 = FZ + 0.05, FZ + 2.2
    cx, cz, rr = 0.35, 1.15, 0.38
    hole = [(1.2 + cx + rr * math.cos(a), cz + rr * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 25)[:-1]]
    # wall seen from the front (inside the room): plane at y=1.47, u along -X... keep front normal towards -Y
    mb.hole_wall((-1.2, 1.46, wz0), (1, 0, 0), (0, 0, 1), 2.4, wz1 - wz0, [(u, v) for u, v in hole], plaster,
                 thick=0.08, wall_col=lambda t: C("B79E73"), bcol=mul(plaster, 0.95))
    for k in range(-3, 4):
        x = cx + 0.1 * k
        hh = math.sqrt(max(0.0, rr * rr - (0.1 * k) ** 2))
        mb.box((x, 1.5, wz0 + cz), (0.02, 0.03, 2 * hh), C("A58A4E"))
    for k in range(-3, 4):
        z = 0.1 * k
        ww = math.sqrt(max(0.0, rr * rr - z * z))
        mb.box((cx, 1.5, wz0 + cz + z), (2 * ww, 0.025, 0.02), C("A58A4E"))
    # scroll (kakejiku) + ikebana on the back wall, left of the window
    mb.cbox((-0.55, 1.4, FZ + 1.25), (0.42, 0.02, 1.0), 0.004, C("E9E1CC"))
    mb.box((-0.55, 1.388, FZ + 1.3), (0.3, 0.01, 0.6), C("EEE8D8"))
    mb.box((-0.55, 1.385, FZ + 1.15), (0.16, 0.008, 0.28), C("3B4A5A"))
    mb.blob((-0.55, 1.384, FZ + 1.42), 0.05, sub=1, amp=0.0, col=C("C8402E"))
    for z in (FZ + 0.75, FZ + 1.75):
        mb.tube([(-0.8, 1.39, z), (-0.3, 1.39, z)], 0.015, 6, col=C("4A3222"))
    with mb.xf(T(-0.55, 1.25, FZ)):
        mb.lathe([(0, 0), (0.07, 0), (0.09, 0.08), (0.06, 0.2), (0.04, 0.24), (0, 0.24)], 10, col=C("2F4C6E"), sh=AUTO)
        for k in range(3):
            mb.tube([(0, 0, 0.2), (0.12 * math.cos(k * 2), 0.06 * math.sin(k * 2), 0.5 + 0.1 * k)], 0.008, 4,
                    col=C("4A6B2A"))
            mb.blob((0.12 * math.cos(k * 2), 0.06 * math.sin(k * 2), 0.5 + 0.1 * k), 0.05, sub=0, amp=0.1,
                    col=C("F2A2B8") if k != 1 else C("FFFFFF"), sh=FLAT)
    # shoji screens (side walls): paper panels with kumiko lattice
    def shoji(x0, y0, x1, y1, z0, h, n_h=3, n_v=6):
        p0, p1 = Vector((x0, y0, 0)), Vector((x1, y1, 0))
        d = p1 - p0
        L = d.length
        dn = d.normalized()
        nrm = Vector((-dn.y, dn.x, 0))
        mid = (p0 + p1) * 0.5
        with mb.xf(frame((mid.x, mid.y, z0), dn, nrm, (0, 0, 1))):
            mb.box((0, 0, 0.14), (L, 0.03, 0.24), lat)
            mb.box((0, 0, 0.26 + (h - 0.26) / 2), (L - 0.04, 0.012, h - 0.3), paper)
            for s in (-1, 1):
                mb.box((s * (L / 2 - 0.02), 0, h / 2), (0.04, 0.04, h), lat)
                for k in range(1, n_h):
                    x = -L / 2 + L * k / n_h
                    mb.box((x, s * 0.009, 0.26 + (h - 0.26) / 2), (0.014, 0.008, h - 0.3), lat)
                for k in range(1, n_v):
                    z = 0.26 + (h - 0.3) * k / n_v
                    mb.box((0, s * 0.009, z), (L - 0.04, 0.008, 0.014), lat)
            mb.box((0, 0, h - 0.02), (L, 0.04, 0.04), lat)
    sh_h = 1.8
    shoji(-1.2, -0.9, -1.2, -0.3, FZ, sh_h)     # left wall, 4 panels
    shoji(-1.2, -0.3, -1.2, 0.3, FZ, sh_h)
    shoji(-1.2, 0.3, -1.2, 0.9, FZ, sh_h)
    shoji(-1.2, 0.9, -1.2, 1.46, FZ, sh_h)
    shoji(1.2, 0.3, 1.2, 0.9, FZ, sh_h)         # right wall: two panels, front half slid open
    shoji(1.18, 0.9, 1.18, 1.46, FZ, sh_h)
    shoji(1.22, 0.4, 1.22, 0.96, FZ, sh_h)
    shoji(-1.15, -0.87, -0.6, -0.87, FZ, sh_h)  # front: panels slid to the sides
    shoji(0.6, -0.87, 1.15, -0.87, FZ, sh_h)
    # lintels (kamoi) + upper plaster band
    for (a, b) in (((-1.2, -0.9), (1.2, -0.9)), ((-1.2, -0.9), (-1.2, 1.5)), ((1.2, -0.9), (1.2, 1.5))):
        mb.beam((a[0], a[1], FZ + sh_h + 0.04), (b[0], b[1], FZ + sh_h + 0.04), 0.09, 0.08, beam_c)
        mb.beam((a[0], a[1], FZ + 2.03), (b[0], b[1], FZ + 2.03), 0.06, 0.26, plaster)
    for (a, b) in (((-1.5, -1.5), (1.5, -1.5)), ((-1.5, -1.5), (-1.5, 1.5)), ((1.5, -1.5), (1.5, 1.5)),
                   ((-1.5, 1.5), (1.5, 1.5))):
        mb.beam((a[0], a[1], FZ + 2.1), (b[0], b[1], FZ + 2.1), 0.12, 0.14, beam_c)
    mb.box((0, 0.0, FZ + 2.18), (3.0, 3.0, 0.03), C("8A6A4C"))   # ceiling
    # roof: pyramid (hogyo) with tile ripples, ridge rolls and a finial
    tile, tile_hi = C("3E4A5A"), C("52606F")
    roof_cols = dict(top=tile, top_hi=tile_hi, fascia=C("2B323D"), soffit=C("7A5A40"), soffit_dk=C("5E4531"),
                     soffit_edge=C("D9B384"))
    corners = hip_roof(mb, 4, 2.05 / math.cos(math.pi / 4), 0.22, FZ + 2.26, 1.45, 0.18, 0.05, 0.1, 0.55, roof_cols,
                       nu=16, nv=6, nvu=2, rot=math.pi / 4, ripple=0.035, powv=1.35)
    for cd in corners:
        ridge = [p + Vector((0, 0, 0.04)) for p in cd["ridge"]]
        mb.tube([cd["top"] + cd["out"] * 0.08 + Vector((0, 0, 0.1))] + ridge, [0.05] + [0.065] * len(ridge), 6,
                col=C("2B323D"), sh=SMOOTH)
    ztop = FZ + 2.26 + 1.45
    mb.lathe([(0, ztop - 0.05), (0.16, ztop - 0.05), (0.18, ztop + 0.02), (0.1, ztop + 0.1), (0.13, ztop + 0.2),
              (0.08, ztop + 0.32), (0, ztop + 0.38)], 8, col=C("B9913E"), sh=AUTO)
    # interior: round low table, tea set, cushions, andon lantern (glow)
    with mb.xf(T(0.05, 0.4, FZ)):
        mb.lathe([(0, 0.26), (0.34, 0.26), (0.34, 0.3), (0, 0.3)], 18, col=C("5B3824"), sh=AUTO)
        for k in range(4):
            a = k * math.pi / 2 + 0.6
            mb.cbox((0.22 * math.cos(a), 0.22 * math.sin(a), 0.13), (0.05, 0.05, 0.26), 0.008, C("4A2E1E"))
        mb.lathe([(0, 0.3), (0.08, 0.3), (0.1, 0.36), (0.08, 0.42), (0.03, 0.43), (0.03, 0.46), (0, 0.46)], 10,
                 col=IRON, sh=AUTO)
        mb.tube([(0.08, 0, 0.38), (0.16, 0, 0.44)], [0.015, 0.008], 4, col=IRON)
        for k in range(2):
            with mb.xf(T(-0.15 + 0.3 * k, -0.15, 0.3)):
                mb.lathe([(0, 0), (0.03, 0), (0.04, 0.05), (0, 0.05)], 8, col=C("6E8F5A"), sh=AUTO)
    for (x, y, c) in ((0.05, -0.1, C("C0392B")), (0.05, 0.9, C("2E4F8C")), (-0.55, 0.4, C("C0392B"))):
        mb.cbox((x, y, FZ + 0.04), (0.5, 0.5, 0.08), 0.03, c)
    with mb.xf(T(0.85, 1.15, FZ)):
        for sx in (-1, 1):
            for sy in (-1, 1):
                mb.box((sx * 0.12, sy * 0.12, 0.3), (0.03, 0.03, 0.6), C("3A2618"))
        mb.box((0, 0, 0.36), (0.24, 0.24, 0.44), C("FFE2A6", a=0.85))
        mb.box((0, 0, 0.6), (0.27, 0.27, 0.03), C("3A2618"))
    # front: shoe stone with geta + red paper lantern at the eave
    rock(mb, (0, -1.85, 0), (0.34, 0.22, 0.1), seed=77, sub=1, cuts=5, colfn=lambda w, u: granite(w, seed=7.0),
         bottom=0.1)
    for sx in (-1, 1):
        mb.cbox((sx * 0.07, -1.85, 0.215), (0.09, 0.2, 0.025), 0.005, C("B98A57"))
        mb.box((sx * 0.07, -1.85, 0.228), (0.02, 0.1, 0.004), C("B8342A"))
    lx, ly = 1.3, -1.62
    mb.tube([(lx, ly, FZ + 2.03), (lx, ly, FZ + 1.9)], 0.006, 4, col=IRON)
    lred = C("E0301E", a=0.8)
    prof = [(0, 0.0), (0.08, 0.0), (0.08, 0.03)]
    for k in range(9):
        t = k / 8
        prof.append((0.085 + 0.08 * math.sin(math.pi * t) ** 0.8, 0.03 + 0.34 * t))
    prof += [(0.08, 0.37), (0.08, 0.4), (0, 0.4)]
    with mb.xf(T(lx, ly, FZ + 1.5)):
        mb.lathe(prof, 12, colfn=lambda r, z, a, i: C("1C1A1A") if i < 3 or i > len(prof) - 4 else
                 (lred if i % 2 else C("A01E16", a=0.8)), sh=SMOOTH)
    # hanging sign on the front beam
    mb.cbox((-0.7, -1.56, FZ + 1.9), (0.7, 0.03, 0.2), 0.006, C("3A2A1E"))
    for k in range(3):
        mb.box((-0.9 + 0.2 * k, -1.577, FZ + 1.9), (0.1, 0.006, 0.1), C("F0E6CC"))
    return mb


# ================================================================================================ wood bridge
def make_wood_bridge():
    mb = MB("SM_KG_WoodBridge", angle=35)
    rng = random.Random(81)
    RISE, HALF = 0.32, 2.6

    def zd(x):
        return RISE * (1 - (x / HALF) ** 2)

    def sl(x):
        return -2 * RISE * x / (HALF * HALF)

    # log stringers
    xs = np.linspace(-2.95, 2.95, 14)
    for sy in (-1, 1):
        mb.tube([(x, sy * 0.62, zd(clamp(x, -HALF, HALF)) - 0.2) for x in xs], 0.13, 8,
                colfn=lambda i, k, p: bark(p, seed=sy), sh=AUTO, capcol=C("C9A06A"))
    # planks across the arc
    xs2 = np.linspace(-2.75, 2.75, 800)
    zs2 = [zd(clamp(x, -HALF, HALF)) for x in xs2]
    arc = np.concatenate([[0], np.cumsum(np.sqrt(np.diff(xs2) ** 2 + np.diff(zs2) ** 2))])
    step, s, k = 0.22, 0.11, 0
    while s < arc[-1]:
        x = float(np.interp(s, arc, xs2))
        a = math.atan(sl(clamp(x, -HALF, HALF)))
        t = Vector((math.cos(a), 0, math.sin(a)))
        nrm = Vector((-math.sin(a), 0, math.cos(a)))
        c = plank_col(rng, C("9A7552"), C("B8916A"), C("6E5238"))
        L = 1.5 - rng.uniform(0, 0.08)
        if k == 13:
            L = 0.9
        yo = rng.uniform(-0.04, 0.04) + (0.4 if k == 13 else 0.0)
        with mb.xf(frame(Vector((x, yo, zd(clamp(x, -HALF, HALF)) - 0.03)), t, Vector((0, 1, 0)), nrm) @
                   R(rng.uniform(-1.5, 1.5), 'Z')):
            mb.cbox((0, 0, 0), (step - 0.025, L, 0.055), 0.01, c)
        s += step
        k += 1
    # posts + rustic handrail + sagging rope
    post_x = [-2.45, -1.22, 0.0, 1.22, 2.45]
    for sy in (-1, 1):
        tops = []
        for x in post_x:
            z0 = zd(x) - 0.32
            z1 = zd(x) + 1.0
            mb.tube([(x, sy * 0.8, z0), (x + rng.uniform(-0.02, 0.02), sy * 0.8, z1)], 0.055, 7,
                    colfn=lambda i, k, p: bark(p, seed=3), sh=AUTO, capcol=C("C9A06A"))
            tops.append(Vector((x, sy * 0.8, z1 - 0.07)))
            mb.tube([(x, sy * 0.8 - sy * 0.06, zd(x) - 0.2), (x, sy * 0.8 + sy * 0.07, zd(x) - 0.2)], 0.02, 4,
                    col=ROPE)
        rail = []
        for x in np.linspace(-2.6, 2.6, 16):
            rail.append(Vector((x, sy * 0.8, zd(clamp(x, -HALF, HALF)) + 0.93 + 0.015 * math.sin(x * 3))))
        mb.tube(rail, 0.04, 7, colfn=lambda i, k, p: bark(p, base="7B5A3E", seed=4), sh=AUTO, capcol=C("C9A06A"))
        for a, b in zip(post_x, post_x[1:]):
            pa = Vector((a, sy * 0.8, zd(a) + 0.48))
            pb = Vector((b, sy * 0.8, zd(b) + 0.48))
            mb.tube(catenary(pa, pb, 0.1, 6), 0.014, 4, col=ROPE)
        # diagonal braces at the ends
        for sx in (-1, 1):
            x = sx * 2.45
            mb.tube([(x, sy * 0.8, zd(x) + 0.75), (x + sx * 0.45, sy * 0.95, -0.02)], 0.035, 6,
                    colfn=lambda i, k, p: bark(p, seed=5), sh=AUTO)
    # legs + cross bracing
    for x in (-1.3, 1.3):
        for sy in (-1, 1):
            mb.tube([(x, sy * 0.62, zd(x) - 0.3), (x, sy * 0.66, -1.3)], 0.1, 7, colfn=lambda i, k, p: bark(p, seed=6),
                    sh=AUTO, capcol=C("B8905A"))
        mb.tube([(x, -0.66, -1.0), (x, 0.66, zd(x) - 0.36)], 0.045, 6, colfn=lambda i, k, p: bark(p, seed=7), sh=AUTO)
        mb.tube([(x, 0.66, -1.0), (x, -0.66, zd(x) - 0.36)], 0.045, 6, colfn=lambda i, k, p: bark(p, seed=8), sh=AUTO)
    # end sills + bank stones
    for sx in (-1, 1):
        mb.tube([(sx * 2.8, -0.95, -0.2), (sx * 2.8, 0.95, -0.2)], 0.15, 8, colfn=lambda i, k, p: bark(p, seed=9),
                sh=AUTO, capcol=C("C9A06A"))
        for k in range(3):
            rock(mb, (sx * rng.uniform(2.9, 3.2), rng.uniform(-1.0, 1.0), -0.3), (0.3, 0.25, 0.22), seed=90 + k + sx,
                 sub=1, cuts=5, colfn=lambda w, u: granite(w, seed=2.0, moss_amt=0.6, moss_up=u.z))
    # a lantern hanging from the first post (unlit)
    with mb.xf(T(-2.45, -0.8, zd(-2.45) + 1.0)):
        mb.tube([(0, 0, -0.02), (0, -0.18, -0.02)], 0.012, 4, col=IRON)
        mb.tube([(0, -0.17, -0.02), (0, -0.17, -0.12)], 0.004, 3, col=IRON)
        mb.cbox((0, -0.17, -0.22), (0.12, 0.12, 0.16), 0.01, C("E8C46A"))
        with mb.xf(T(0, -0.17, -0.14)):
            mb.lathe([(0, 0), (0.09, 0), (0, 0.07)], 4, col=IRON, sh=FLAT, phase=math.pi / 4)
    return mb


# ================================================================================================ cemetery gate
def make_gate_arch():
    mb = MB("SM_KG_GateArch", angle=30)
    rng = random.Random(91)
    brick = lambda p: with_a(mix(C("8E4A38"), C("A85E44"), 0.5 + 0.5 * nz(p, 6.0)), 0.0)
    capst = lambda p: grave_stone(p, base="B8B2A6", seed=3.0, moss=0.3)
    for sx in (-1, 1):
        x = sx * 1.62
        mb.cbox((x, 0, 0.12), (0.72, 0.72, 0.24), 0.02, capst)
        for k in range(10):
            z = 0.24 + 0.24 * k
            mb.cbox((x, 0, z + 0.12), (0.6, 0.6, 0.235), 0.015, lambda p, k=k: mul(brick(p), 0.9 + 0.12 * (k % 2)))
        mb.cbox((x, 0, 2.72), (0.76, 0.76, 0.16), 0.02, capst)
        with mb.xf(T(x, 0, 2.8)):
            mb.lathe([(0, 0), (0.3, 0), (0, 0.28)], 4, col=capst, sh=FLAT, phase=math.pi / 4)
        mb.blob((x, 0, 3.2), 0.13, sub=1, amp=0.0, col=capst, sh=SMOOTH)
    # wrought iron arch between the pillars
    def zc(x, off=0.0):
        return 2.45 + 0.95 * math.cos(math.pi * x / 2.62) + off
    xs = np.linspace(-1.31, 1.31, 25)
    mb.tube([(x, 0, zc(x)) for x in xs], 0.028, 6, col=IRON, sh=AUTO)
    mb.tube([(x, 0, zc(x, -0.26)) for x in xs], 0.024, 6, col=IRON, sh=AUTO)
    for x in np.linspace(-1.2, 1.2, 17):
        mb.tube([(x, 0, zc(x, -0.26)), (x, 0, zc(x))], 0.012, 4, col=IRON)
    # scroll curls on top of the arch
    def scroll(cx, cz, r, sgn, turns=1.4):
        pts = []
        for k in range(18):
            t = k / 17
            a = sgn * t * turns * 2 * math.pi
            rr = r * (1 - 0.7 * t)
            pts.append((cx + rr * math.sin(a), 0, cz + r - rr * math.cos(a)))
        mb.tube(pts, 0.012, 4, col=IRON, sh=SMOOTH)
    for sx in (-1, 1):
        for x0 in (0.45, 0.9):
            scroll(sx * x0, zc(x0), 0.12, sx)
    # centre plaque + cross
    mb.cbox((0, 0, zc(0, -0.5)), (0.8, 0.04, 0.2), 0.01, IRON)
    for k in range(7):
        mb.box((-0.27 + 0.09 * k, -0.025, zc(0, -0.5)), (0.05, 0.01, 0.09), C("D8C9A0"))
    mb.box((0, 0, zc(0) + 0.28), (0.04, 0.04, 0.55), IRON)
    mb.box((0, 0, zc(0) + 0.38), (0.26, 0.04, 0.04), IRON)
    # gate leaves: hinged at the pillars, swung outwards (towards -Y)
    def leaf(hx, sgn, open_deg):
        with mb.xf(T(hx, 0, 0) @ R(sgn * open_deg, 'Z')):
            W = 1.27
            xs_ = [-sgn * (0.03 + (W - 0.06) * k / 9) for k in range(10)]
            for x in xs_:
                mb.tube([(x, 0, 0.1), (x, 0, 1.88)], 0.014, 4, col=IRON)
                with mb.xf(T(x, 0, 1.88)):
                    mb.lathe([(0, 0), (0.028, 0.03), (0, 0.11)], 4, col=IRON, sh=FLAT)
            for z in (0.14, 1.0, 1.78):
                mb.box((-sgn * W / 2, 0, z), (W, 0.035, 0.04), IRON)
            mb.box((-sgn * 0.02, 0, 0.98), (0.045, 0.045, 1.8), IRON)
            mb.box((-sgn * (W - 0.02), 0, 0.98), (0.045, 0.045, 1.8), IRON)
            for k in range(3):
                x = -sgn * (0.22 + 0.4 * k)
                mb.tube([(x - 0.12, 0, 1.12), (x, 0, 1.3), (x + 0.12, 0, 1.12)], 0.01, 4, col=IRON)
    leaf(-1.3, -1, 72)
    leaf(1.3, 1, 60)
    rng2 = random.Random(3)
    for sx in (-1, 1):
        for k in range(3):
            tuft(mb, (sx * 1.62 + rng2.uniform(-0.45, 0.45), rng2.uniform(-0.45, 0.45), 0), rng2, n=6, h=0.25)
    return mb


# ================================================================================================ more wilds
def mushroom(mb, pos, h, cr, rng, cap=None, stem=None, spots=True, lean=(0.0, 0.0), alpha=0.0):
    cap = cap or C("D8342C")
    stem = stem or C("F1E8D2")
    x, y, z = pos
    with mb.xf(T(x, y, z) @ R(lean[0], 'X') @ R(lean[1], 'Y')):
        mb.lathe([(0, 0), (cr * 0.38, 0), (cr * 0.32, h * 0.45), (cr * 0.28, h * 0.86), (0, h * 0.86)], 7,
                 col=with_a(stem, alpha), sh=SMOOTH)
        mb.lathe([(0, h * 0.78), (cr * 0.9, h * 0.8), (cr, h * 0.86), (cr * 0.88, h * 0.98), (cr * 0.55, h * 1.1),
                  (0, h * 1.15)], 9, colfn=lambda r, zz, a, i: with_a(cap if i > 0 else mul(stem, 0.85), alpha),
                 sh=SMOOTH, phase=rng.random())
        if spots:
            for k in range(5):
                a = rng.random() * 6.28
                th = rng.uniform(0.35, 1.1)
                p = (cr * 0.75 * math.cos(th) * math.cos(a), cr * 0.75 * math.cos(th) * math.sin(a),
                     h * 0.9 + h * 0.24 * math.sin(th))
                mb.blob(p, (cr * 0.14, cr * 0.14, cr * 0.05), sub=0, amp=0.0, col=with_a(C("FFF8EC"), alpha), sh=FLAT)


def make_tent_lean():
    mb = MB("SM_KG_Tent_Lean", angle=40)
    rng = random.Random(2)
    tarp, tarp_d, inner = C("6F8A3C"), C("566F2E"), C("3F5226")
    stick = C("7A5A3C")
    for sx in (-1, 1):
        x = sx * 1.15
        mb.tube([(x, -0.78, -0.05), (x, -0.78, 1.36)], [0.04, 0.034], 6, col=stick)
        mb.tube([(x, -0.78, 1.25), (x + sx * 0.1, -0.76, 1.5)], [0.026, 0.018], 5, col=stick)
        mb.tube([(x, -0.78, 1.25), (x - sx * 0.07, -0.8, 1.5)], [0.026, 0.018], 5, col=stick)
    mb.tube([(-1.38, -0.78, 1.42), (1.38, -0.78, 1.42)], 0.035, 6, col=C("8C6A48"))

    def tp(u, v, lift=0.0):
        x = lerp(-1.3, 1.3, u)
        y = lerp(-0.84, 1.0, v)
        z = lerp(1.47, 0.03, v) - 0.1 * math.sin(math.pi * clamp(v)) * (0.6 + 0.4 * math.sin(math.pi * u))
        if v < 0:
            z = 1.47 + v * 1.5
        return Vector((x, y, z + lift))
    nu, nv = 10, 9
    vs = [-0.07] + [k / (nv - 1) for k in range(nv)]
    grid = [[tp(j / nu, v) for j in range(nu + 1)] for v in vs]

    def tc(i, j, p):
        c = mix(tarp, tarp_d, 0.3 + 0.4 * nz(p, 1.7, (4, 4, 4)))
        if 0.3 < p.x < 0.75 and 0.0 < p.y < 0.45:
            c = C("8C6B3A")
        return c
    mb.sheet2([r[::-1] for r in grid], tc, lambda i, j, p: inner)
    for sx in (-1, 1):
        a = tp(0.5 + sx * 0.5, 1.0)
        b = a + Vector((sx * 0.25, 0.35, -0.02))
        mb.tube([a, b], 0.008, 4, col=ROPE)
        mb.beam(b - Vector((0, 0, 0.03)), b + Vector((0, 0.03, 0.1)), 0.03, 0.03, WOOD_L)
        mb.tube([(sx * 1.15, -0.78, 1.4), (sx * 1.6, -1.35, 0.02)], 0.008, 4, col=ROPE)
    # bedroll, pack, firewood
    mb.cbox((-0.35, 0.05, 0.06), (0.62, 1.4, 0.1), 0.03, C("8E3B2F"))
    mb.tube([(-0.64, -0.5, 0.14), (-0.06, -0.5, 0.14)], 0.1, 9, col=C("C9A15A"), sh=AUTO)
    mb.cbox((0.55, 0.3, 0.22), (0.36, 0.26, 0.44), 0.06, C("7A5234"))
    mb.cbox((0.55, 0.18, 0.34), (0.3, 0.06, 0.22), 0.03, C("5E3E26"))
    for k in range(3):
        with mb.xf(T(0.55 + 0.02 * k, -0.35, 0.06 + 0.1 * (k == 2)) @ R(90 + 8 * k, 'Z')):
            mb.tube([(-0.25, 0.1 * (k - 1) if k < 2 else 0.0, 0), (0.25, 0.1 * (k - 1) if k < 2 else 0.0, 0)], 0.055, 7,
                    colfn=lambda i, kk, p: bark(p, seed=2), capcol=C("C9A06A"), sh=AUTO)
    return mb


def make_log_bench():
    mb = MB("SM_KG_LogBench", angle=35)
    r = 0.19
    top = C("CFA672")
    prof = [(-r, 0.0), (-r, -0.015)] + [(r * math.cos(a), r * math.sin(a)) for a in np.linspace(math.pi * 1.05, math.pi * 1.95, 7)] + \
           [(r, -0.015), (r, 0.0)]
    prof = [(-x, y) for x, y in prof]

    def col(i, k, p):
        return top if p.z > 0.445 else bark(p, seed=4)
    pts = [(0.0, y, 0.45 + 0.01 * math.sin(y * 3)) for y in np.linspace(-0.92, 0.92, 7)]
    mb.sweep(pts, prof, colfn=col, capcol=C("D8B07E"), sh=AUTO)
    for y in (-0.92, 0.92):
        with mb.xf(T(0, y + (0.004 if y > 0 else -0.004), 0.42) @ R(90, 'X')):
            for k, rr in enumerate((0.13, 0.08, 0.035)):
                mb.lathe([(0, 0), (rr, 0), (rr, 0.002), (0, 0.002)], 10, col=C("B88A55") if k % 2 == 0 else C("E0BC8A"),
                         sh=FLAT) if False else None
    for y in (-0.6, 0.6):
        mb.tube([(-0.3, y, 0.13), (0.3, y, 0.13)], 0.13, 9, colfn=lambda i, k, p: bark(p, seed=5), capcol=C("D2AC78"),
                sh=AUTO)
    return mb


def make_fallen_log():
    mb = MB("SM_KG_FallenLog", angle=35)
    rng = random.Random(6)
    n = 12
    pts = [Vector((lerp(-1.8, 1.7, i / (n - 1)), 0.12 * math.sin(i * 0.6), 0.26 - 0.02 * i / n)) for i in range(n)]
    radii = [0.28 - 0.05 * i / (n - 1) for i in range(n)]

    def col(i, k, p):
        c = bark(p, base="6E5038", dark="3E2C20", seed=6)
        up = (p.z - pts[i].z) / radii[i]
        m = smoothstep(0.35, 0.8, up + 0.3 * nz(p, 2.5, (1, 1, 1)))
        return mix(c, with_a(mix(MOSS, MOSS_L, 0.5 + 0.5 * nz(p, 6.0)), 0.0), m * 0.9)
    mb.tube(pts, radii, 11, colfn=col, sh=AUTO, capcol=C("D8B07E"), cap1=False,
            rmod=lambda i, k: 1.0 + 0.05 * math.sin(k * 2.3 + i))
    # sawn end rings
    with mb.xf(T(-1.805, pts[0].y, pts[0].z) @ R(-90, 'Y')):
        for k, rr in enumerate((0.2, 0.13, 0.06)):
            mb.lathe([(0, 0.004 * (k + 1)), (rr, 0.004 * (k + 1)), (0, 0.004 * (k + 1) + 0.001)], 11,
                     col=C("B98B56") if k % 2 == 0 else C("E2BE8C"), sh=FLAT)
    # broken end: splinters
    e = pts[-1]
    for k in range(7):
        a = 2 * math.pi * k / 7 + rng.random() * 0.3
        base = e + Vector((0, math.cos(a), math.sin(a))) * radii[-1] * 0.6
        tipp = base + Vector((rng.uniform(0.12, 0.35), math.cos(a) * 0.04, math.sin(a) * 0.04))
        mb.tube([base, tipp], [0.07, 0.01], 4, col=C("C9A06A"), tip=0.02, cap1=False)
    ids = [mb.v(e + Vector((0.01, 0, 0)) + Vector((0, math.cos(a), math.sin(a))) * radii[-1] * 0.98, C("B08658"))
           for a in np.linspace(0, 2 * math.pi, 12)[:-1]]
    mb.f_dir(ids, (1, 0, 0), None, FLAT)
    # stubs, fungus, mushrooms, moss tufts
    for (i, a, L) in ((4, 1.1, 0.35), (8, -1.9, 0.25)):
        d = Vector((0.3, math.cos(a), math.sin(a))).normalized()
        mb.tube([pts[i] + d * radii[i] * 0.6, pts[i] + d * (radii[i] + L)], [0.06, 0.04], 6,
                colfn=lambda ii, k, p: bark(p, seed=8), capcol=C("C9A06A"), sh=AUTO)
    for (x, a) in ((-0.9, -1.4), (-0.7, -1.2), (0.4, -1.6)):
        i = min(range(n), key=lambda j: abs(pts[j].x - x))
        c = pts[i] + Vector((0, math.cos(a), math.sin(a))) * (radii[i] + 0.01)
        with mb.xf(T(c.x, c.y, c.z) @ R(math.degrees(a) - 90, 'X')):
            mb.lathe([(0, 0), (0.12, 0.0), (0.13, 0.03), (0.08, 0.05), (0, 0.05)], 9, col=C("D98A2B"), sh=AUTO) if False else None
            mb.blob((0, 0.05, 0), (0.12, 0.08, 0.03), sub=1, amp=0.1, col=C("D98A2B"), sh=FLAT)
    mushroom(mb, (0.9, -0.33, 0), 0.14, 0.06, rng)
    mushroom(mb, (1.0, -0.4, 0), 0.1, 0.045, rng)
    for k in range(6):
        tuft(mb, (rng.uniform(-1.7, 1.6), rng.choice((-0.33, 0.33)), 0), rng, n=6, h=0.28)
    return mb


def make_stump():
    mb = MB("SM_KG_Stump_Big", angle=35)
    rng = random.Random(7)
    ph = 0.7

    def rfn(a, i, z):
        return 1.0 + 0.32 * max(0.0, math.cos(5 * a + ph)) ** 2 * math.exp(-max(z, 0.0) / 0.18) + \
            0.05 * nz((math.cos(a), math.sin(a), z), 2.0)
    prof = [(0, -0.05), (0.66, -0.05), (0.64, 0.05), (0.56, 0.14), (0.5, 0.28), (0.47, 0.5), (0.46, 0.68), (0.43, 0.72)]

    def col(r, z, a, i):
        return bark(Vector((r * math.cos(a), r * math.sin(a), z)), base="6E5038", seed=7) if i < len(prof) - 1 else C("7A5A3C")
    rings = mb.lathe(prof, 16, colfn=col, rfn=rfn, sh=AUTO)
    top_ring = rings[-1]
    # annual-ring top
    cen = mb.v((0.02, 0.0, 0.74), C("B98B56"))
    for rr, c in ((0.36, C("D7B07A")), (0.27, C("BE9060")), (0.17, C("DDB885")), (0.08, C("B98B56"))):
        pass
    ring_ids = []
    for (rr, c) in ((0.4, C("DDB885")), (0.3, C("BE9060")), (0.2, C("DDB885")), (0.1, C("BE9060"))):
        ring_ids.append([mb.v((rr * math.cos(a) * 1.02, rr * math.sin(a), 0.72 + 0.02 * (0.4 - rr) / 0.4), c)
                         for a in [2 * math.pi * j / 16 for j in range(16)]])
    allr = [top_ring] + ring_ids
    for A, B in zip(allr, allr[1:]):
        for j in range(16):
            jn = (j + 1) % 16
            mb.f((A[j], A[jn], B[jn], B[j]), None, FLAT)
    for j in range(16):
        mb.f((ring_ids[-1][j], ring_ids[-1][(j + 1) % 16], cen), None, FLAT)
    # roots
    for k in range(5):
        a = (2 * math.pi * k - ph) / 5
        d = Vector((math.cos(a), math.sin(a), 0))
        pts = [d * 0.55 + Vector((0, 0, 0.12)), d * 0.85 + Vector((0, 0, 0.05)), d * 1.1 + Vector((0, 0, -0.04))]
        mb.tube(pts, [0.12, 0.08, 0.04], 6, colfn=lambda i, kk, p: bark(p, seed=9), sh=AUTO, tip=0.05)
    # axe stuck in the top
    with mb.xf(T(0.08, -0.05, 0.74) @ R(-35, 'Z') @ R(28, 'Y')):
        mb.cbox((0, 0, 0.02), (0.2, 0.035, 0.14), 0.008, C("8A8E96"))
        mb.box((0.02, 0, -0.04), (0.17, 0.02, 0.04), C("B9BDC4"))
        mb.tube([(-0.02, 0, 0.06), (-0.04, 0, 0.72)], [0.024, 0.02], 6, col=C("C08A55"))
    mushroom(mb, (-0.45, 0.35, 0.05), 0.16, 0.07, rng, cap=C("E0A33A"), spots=False)
    mushroom(mb, (-0.52, 0.28, 0.03), 0.11, 0.05, rng, cap=C("E0A33A"), spots=False)
    for k in range(6):
        a = rng.random() * 6.28
        tuft(mb, (0.7 * math.cos(a), 0.7 * math.sin(a), 0), rng, n=6, h=0.3)
    return mb


def ruin_st(p, seed=0.0):
    c = granite(p, base="A89F8E", light="C4BBA8", dark="7C7366", seed=seed, scale=1.4)
    return lichen(p, c, 0.35, seed, 3.0)


def block_wall(mb, rng, x0, x1, hfn, thick=0.6, course=0.3, window=None, seed=0.0):
    """Ragged wall of chamfered blocks along X (y centred on 0). hfn(x) = broken top height."""
    z = 0.0
    row = 0
    while True:
        x = x0 - (rng.uniform(0.0, 0.3) if row % 2 else 0.0)
        any_block = False
        while x < x1 - 0.05:
            L = rng.uniform(0.42, 0.8)
            xa, xb = max(x, x0), min(x + L, x1)
            xc = (xa + xb) / 2
            x += L
            if xb - xa < 0.15:
                continue
            if z + course * 0.5 > hfn(xc):
                continue
            if window and window[0] < xc < window[1] and window[2] < z + course / 2 < window[3]:
                continue
            any_block = True
            yo = rng.uniform(-0.03, 0.03)
            with mb.xf(T(xc, yo, z + course / 2) @ R(rng.uniform(-2, 2), 'Z')):
                mb.cbox((0, 0, 0), (xb - xa - 0.025, thick - rng.uniform(0, 0.06), course - 0.02), 0.035,
                        lambda p, s=rng.random(): mul(ruin_st(p, seed), 0.88 + 0.2 * s))
        z += course
        row += 1
        if not any_block and z > 0.3:
            break
        if z > 6:
            break


def make_ruin_wall_a():
    mb = MB("SM_KG_RuinWall_A", angle=30)
    rng = random.Random(21)
    f0 = len(mb.F)

    def h(x):
        return 2.5 - 1.4 * smoothstep(-0.6, 1.9, x) + 0.3 * math.sin(x * 3.3) + 0.15 * math.sin(x * 7.1)
    mb.cbox((0, 0, 0.08), (4.2, 0.8, 0.16), 0.03, lambda p: mul(ruin_st(p, 1), 0.8))
    with mb.xf(T(0, 0, 0.16)):
        block_wall(mb, rng, -2.0, 2.0, h, window=(-1.2, -0.45, 0.9, 1.75), seed=1.0)
        mb.cbox((-0.82, 0, 1.87), (1.1, 0.64, 0.24), 0.03, lambda p: mul(ruin_st(p, 2), 0.95))
        mb.cbox((-0.82, 0, 0.84), (0.9, 0.7, 0.1), 0.02, lambda p: mul(ruin_st(p, 2), 0.9))
    moss_tops(mb, f0, amt=0.95, thr=0.6, seed=2.0, freq=1.5, bias=0.1)
    # rubble + fallen blocks
    for k in range(7):
        x, y = rng.uniform(-1.6, 2.2), rng.choice((-1, 1)) * rng.uniform(0.45, 0.9)
        with mb.xf(T(x, y, 0.1) @ R(rng.uniform(0, 90), 'Z') @ R(rng.uniform(-12, 12), 'X')):
            mb.cbox((0, 0, 0.02), (rng.uniform(0.3, 0.6), rng.uniform(0.25, 0.4), 0.26), 0.035,
                    lambda p: mul(ruin_st(p, 3), 0.9))
    for k in range(10):
        tuft(mb, (rng.uniform(-2.0, 2.0), rng.choice((-1, 1)) * rng.uniform(0.4, 0.7), 0), rng, n=6, h=0.3)
    return mb


def make_ruin_wall_b():
    mb = MB("SM_KG_RuinWall_B", angle=30)
    rng = random.Random(22)
    f0 = len(mb.F)
    with mb.xf(T(-0.6, -0.6, 0)):
        mb.cbox((1.3, 0, 0.06), (2.9, 0.75, 0.12), 0.03, lambda p: mul(ruin_st(p, 1), 0.8))
        with mb.xf(T(0, 0, 0.12)):
            block_wall(mb, rng, -0.3, 2.6, lambda x: 1.6 - 1.0 * smoothstep(0.2, 2.6, x) + 0.2 * math.sin(x * 4.0),
                       thick=0.55, seed=4.0)
        with mb.xf(R(90, 'Z')):
            mb.cbox((1.0, 0, 0.06), (2.3, 0.75, 0.12), 0.03, lambda p: mul(ruin_st(p, 1), 0.8))
            with mb.xf(T(0, 0, 0.12)):
                block_wall(mb, rng, 0.28, 2.0, lambda x: 1.7 - 0.9 * smoothstep(0.4, 2.0, x) + 0.2 * math.sin(x * 3.0),
                           thick=0.55, seed=5.0)
    moss_tops(mb, f0, amt=0.95, thr=0.6, seed=3.0, freq=1.5, bias=0.1)
    for k in range(6):
        x, y = rng.uniform(0.0, 1.8), rng.uniform(0.0, 1.2)
        with mb.xf(T(x, y, 0.1) @ R(rng.uniform(0, 90), 'Z') @ R(rng.uniform(-12, 12), 'Y')):
            mb.cbox((0, 0, 0.02), (rng.uniform(0.3, 0.55), rng.uniform(0.25, 0.4), 0.26), 0.035,
                    lambda p: mul(ruin_st(p, 3), 0.9))
    for k in range(9):
        tuft(mb, (rng.uniform(-0.8, 2.0), rng.uniform(-0.9, 1.6), 0), rng, n=6, h=0.3)
    return mb


def make_ruin_arch():
    mb = MB("SM_KG_RuinArch", angle=30)
    rng = random.Random(23)
    f0 = len(mb.F)
    for sx in (-1, 1):
        mb.cbox((sx * 1.6, 0, 0.1), (0.95, 0.95, 0.2), 0.03, lambda p: mul(ruin_st(p, 1), 0.8))
        z = 0.2
        for k in range(7):
            hgt = 0.3
            with mb.xf(T(sx * 1.6 + rng.uniform(-0.02, 0.02), rng.uniform(-0.02, 0.02), z + hgt / 2) @ R(rng.uniform(-3, 3), 'Z')):
                mb.cbox((0, 0, 0), (0.72 if k % 2 else 0.78, 0.72, hgt - 0.02), 0.035,
                        lambda p, s=rng.random(): mul(ruin_st(p, 2), 0.88 + 0.2 * s))
            z += hgt
        mb.cbox((sx * 1.6, 0, z + 0.06), (0.86, 0.8, 0.12), 0.02, lambda p: ruin_st(p, 3))
    zs = 0.2 + 7 * 0.3 + 0.12
    r0, r1, n = 1.25, 1.9, 11
    for k in range(n):
        a0, a1 = math.pi * k / n, math.pi * (k + 1) / n
        g = 0.012
        drop = -0.06 if k == 3 else 0.0
        out = [(r0 * math.cos(a0 + g), zs + r0 * math.sin(a0 + g)), (r1 * math.cos(a0 + g), zs + r1 * math.sin(a0 + g)),
               (r1 * math.cos(a1 - g), zs + r1 * math.sin(a1 - g)), (r0 * math.cos(a1 - g), zs + r0 * math.sin(a1 - g))]
        if k == n // 2:
            out[1] = (out[1][0], out[1][1] + 0.12)
            out[2] = (out[2][0], out[2][1] + 0.12)
        with mb.xf(T(0, 0, drop)):
            mb.prism(out, -0.36, 0.36, lambda p, s=rng.random(): mul(ruin_st(p, 4), 0.9 + 0.18 * s), bevel=0.03)
    # broken spandrel blocks above the right haunch
    for (x, z, w) in ((1.55, zs + 1.2, 0.7), (1.25, zs + 1.72, 0.55), (1.75, zs + 1.55, 0.35)):
        with mb.xf(T(x, 0, z) @ R(rng.uniform(-4, 4), 'Y')):
            mb.cbox((0, 0, 0), (w, 0.68, 0.3), 0.035, lambda p: mul(ruin_st(p, 5), 0.92))
    moss_tops(mb, f0, amt=0.95, thr=0.55, seed=4.0, freq=1.4, bias=0.1)
    # fallen voussoir + rubble
    with mb.xf(T(-0.9, -1.0, 0.0) @ R(30, 'Z') @ R(80, 'X')):
        mb.prism([(0, 0), (0.62, 0), (0.55, 0.36), (0.07, 0.36)], -0.3, 0.3, lambda p: ruin_st(p, 6), bevel=0.03)
    for k in range(6):
        with mb.xf(T(rng.uniform(-2.2, 2.2), rng.uniform(-1.2, 1.2), 0.08) @ R(rng.uniform(0, 90), 'Z')):
            mb.cbox((0, 0, 0), (rng.uniform(0.2, 0.4), rng.uniform(0.2, 0.3), 0.18), 0.03, lambda p: mul(ruin_st(p, 7), 0.9))
    # ivy on the left pier
    ivy = [Vector((-1.95 + 0.1 * math.sin(k), -0.4 - 0.03 * k % 2, 0.15 + 0.3 * k)) for k in range(10)]
    ivy += [Vector((-1.8 + 0.25 * k, -0.42, zs + 0.3 + 0.35 * k)) for k in range(1, 4)]
    mb.tube(ivy, 0.018, 4, col=C("4A3A28"))
    for p in ivy[1:]:
        for _ in range(3):
            leaf_blob(mb, p + Vector((rng.uniform(-0.12, 0.12), rng.uniform(-0.08, 0.02), rng.uniform(-0.12, 0.12))),
                      rng.uniform(0.06, 0.1), rng, mix(C("3F6B24"), C("6E9E3A"), rng.random()))
    for k in range(10):
        tuft(mb, (rng.uniform(-2.3, 2.3), rng.uniform(-0.9, 0.9), 0), rng, n=6, h=0.3)
    return mb


def make_ruin_pillar():
    mb = MB("SM_KG_RuinPillar", angle=35)
    rng = random.Random(24)
    f0 = len(mb.F)
    marble = lambda p: granite(p, base="D8D0BF", light="ECE5D6", dark="A89F8E", seed=8.0, scale=1.2)
    mb.cbox((0, 0, 0.12), (0.95, 0.95, 0.24), 0.03, lambda p: mul(marble(p), 0.85))
    mb.lathe([(0, 0.24), (0.42, 0.24), (0.43, 0.3), (0.38, 0.36), (0.36, 0.4), (0, 0.4)], 20, col=marble, sh=AUTO)

    def flute(a, i, z):
        return 1.0 - 0.06 * abs(math.sin(8 * a))
    z = 0.4
    for k in range(4):
        h = 0.52 if k < 3 else 0.45
        with mb.xf(T(rng.uniform(-0.015, 0.015), rng.uniform(-0.015, 0.015), 0) @ R(rng.uniform(-3, 3), 'Z')):
            s0 = len(mb.V)
            mb.lathe([(0, z), (0.31 - 0.01 * k, z), (0.305 - 0.01 * k, z + h), (0, z + h)], 32, col=marble, rfn=flute,
                     sh=AUTO)
            if k == 3:
                def brk(p, zt=z + h):
                    if p.z > zt - 0.01:
                        return Vector((p.x, p.y, zt - 0.28 * smoothstep(-0.2, 0.3, p.x) + 0.08 * nz(p, 7.0) - 0.1 * (p.y > 0.1)))
                    return p
                mb.deform(s0, brk)
        z += h
    moss_tops(mb, f0, amt=0.9, thr=0.6, seed=5.0, freq=2.0, bias=0.1)
    # fallen capital
    with mb.xf(T(0.95, 0.55, 0.0) @ R(25, 'Z') @ R(-15, 'X')):
        mb.cbox((0, 0, 0.12), (0.78, 0.78, 0.2), 0.03, marble)
        mb.lathe([(0, 0.2), (0.36, 0.2), (0.3, 0.34), (0, 0.34)], 16, col=marble, sh=AUTO)
    with mb.xf(T(-0.7, 0.45, 0.2) @ R(80, 'Y') @ R(10, 'X')):
        mb.lathe([(0, -0.25), (0.28, -0.25), (0.28, 0.2), (0, 0.2)], 24, col=marble, rfn=flute, sh=AUTO)
    for k in range(8):
        tuft(mb, (rng.uniform(-0.9, 1.2), rng.uniform(-0.8, 0.9), 0), rng, n=6, h=0.3)
    return mb


def make_signpost():
    mb = MB("SM_KG_Signpost", angle=35)
    rng = random.Random(25)
    wood = C("8A6A4A")
    mb.cbox((0, 0, 1.2), (0.12, 0.12, 2.4), 0.015, lambda p: mix(wood, C("6E5238"), 0.5 + 0.5 * nz(p, 4.0)))
    with mb.xf(T(0, 0, 2.4)):
        mb.lathe([(0, 0), (0.12, 0.0), (0.12, 0.03), (0, 0.16)], 4, col=C("5E3E26"), sh=FLAT, phase=math.pi / 4)
    boards = [(2.12, 20, C("F0E2BE"), C("3A2A20")), (1.84, 160, C("3E8C8A"), C("F2EAD2")),
              (1.56, 255, C("C9452E"), C("F7EBD0"))]
    for (z, yaw, bc, tc) in boards:
        with mb.xf(T(0, 0, z) @ R(yaw, 'Z')):
            L, h = 0.78, 0.2
            out = [(0.06, -h / 2), (L - 0.14, -h / 2), (L, 0.0), (L - 0.14, h / 2), (0.06, h / 2)]
            mb.prism(out, -0.02, 0.02, lambda p, bc=bc: mix(bc, mul(bc, 0.8), 0.5 + 0.5 * nz(p, 5.0)), bevel=0.008)
            for s in (-1, 1):
                for k in range(4 + rng.randint(0, 2)):
                    mb.box((0.14 + 0.09 * k, s * 0.021, 0.0), (0.06, 0.004, 0.08), tc)
            mb.box((0.03, 0.0, 0.0), (0.04, 0.05, 0.05), IRON)
    for k in range(7):
        a = 2 * math.pi * k / 7
        rock(mb, (0.2 * math.cos(a), 0.2 * math.sin(a), 0), (0.13, 0.11, 0.1), seed=250 + k, sub=1, cuts=4,
             colfn=lambda w, u: granite(w, seed=2.0, moss_amt=0.5, moss_up=u.z), bottom=-0.03)
    for k in range(4):
        tuft(mb, (rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 0), rng, n=6, h=0.3)
    return mb


def make_mushroom_ring():
    mb = MB("SM_KG_MushroomRing", angle=40)
    rng = random.Random(26)
    for k in range(15):
        a = 2 * math.pi * k / 15 + rng.uniform(-0.1, 0.1)
        r = 1.1 + rng.uniform(-0.1, 0.1)
        big = k in (3, 9)
        h = rng.uniform(0.2, 0.32) if big else rng.uniform(0.08, 0.18)
        cr = h * rng.uniform(0.45, 0.6)
        cap = C("D8342C") if k % 5 else C("E8913A")
        mushroom(mb, (r * math.cos(a), r * math.sin(a), 0), h, cr, rng, cap=cap,
                 lean=(rng.uniform(-8, 8), rng.uniform(-8, 8)))
        if rng.random() < 0.5:
            b = a + 0.08
            mushroom(mb, ((r + 0.08) * math.cos(b), (r + 0.08) * math.sin(b), 0), h * 0.5, cr * 0.5, rng, cap=cap,
                     lean=(rng.uniform(-12, 12), 0))
        tuft(mb, ((r - 0.12) * math.cos(a + 0.2), (r - 0.12) * math.sin(a + 0.2), 0), rng, n=5, h=0.2)
    for k in range(5):
        a = rng.random() * 6.28
        mushroom(mb, (0.3 * math.cos(a), 0.3 * math.sin(a), 0), 0.07, 0.035, rng, cap=C("F2EAD8"), spots=False)
    return mb


def make_berry_bush():
    mb = MB("SM_KG_BerryBush", angle=40, material=MAT_SWAY)
    rng = random.Random(27)
    cen, rad = Vector((0, 0, 0.55)), (0.62, 0.58, 0.5)
    for k in range(5):
        a = 2 * math.pi * k / 5
        mb.tube([(0, 0, 0), (0.25 * math.cos(a), 0.25 * math.sin(a), 0.45)], [0.04, 0.02], 5, col=C("5A4030"))

    def en(p, u):
        d = Vector(((p.x - cen.x) / rad[0] ** 2, (p.y - cen.y) / rad[1] ** 2, (p.z - cen.z) / rad[2] ** 2))
        return d.normalized(), 0.65
    s0 = len(mb.V)
    lights = [C("4E8A2E"), C("5E9A34"), C("3F7A2A"), C("6FAA3C")]
    blobs = [(0, 0, 0.62, 0.42)] + [(0.38 * math.cos(a), 0.36 * math.sin(a), 0.45 + 0.12 * math.sin(a * 2), 0.33)
                                     for a in np.linspace(0, 2 * math.pi, 7)[:-1]]
    for (x, y, z, r) in blobs:
        base = rng.choice(lights)
        mb.blob((x, y, z), (r, r, r * 0.85), sub=1, amp=0.18, seed=rng.random() * 30,
                colfn=lambda p, u, base=base: with_a(mul(base, 0.75 + 0.35 * smoothstep(0.2, 1.0, p.z)),
                                                     clamp(0.15 + 0.85 * smoothstep(0.15, 0.9, p.z))),
                nrm=en, sh=SMOOTH)
    bvh = BVHTree.FromPolygons([tuple(v) for v in mb.V], [tuple(f) for f in mb.F])
    for k in range(46):
        d = rand_unit(rng)
        d.z = abs(d.z) * 0.9 + 0.05
        d.normalize()
        o = cen + Vector((d.x * 2, d.y * 2, d.z * 2))
        hit = bvh.ray_cast(o, (cen - o).normalized())
        if hit[0] is None or hit[0].z < 0.18:
            continue
        p = hit[0] + hit[1] * 0.02
        a = clamp(0.15 + 0.85 * smoothstep(0.15, 0.9, p.z))
        col = C("D42A3A") if rng.random() < 0.75 else C("8E1B4C")
        mb.blob(p, 0.04, sub=1, amp=0.0, col=with_a(col, a), sh=SMOOTH)
    return mb


# ================================================================================================ more church yard
def make_grave_celtic():
    mb = MB("SM_KG_Gravestone_Celtic", angle=30)
    col = lambda p: grave_stone(p, base="8F959B", light="A9AFB5", dark="6A7075", seed=4.0)
    mb.prism([(-0.3, 0.0), (0.3, 0.0), (0.26, 0.3), (-0.26, 0.3)], -0.2, 0.2, col, bevel=0.02)
    mb.prism([(-0.12, 0.28), (0.12, 0.28), (0.1, 1.52), (0, 1.58), (-0.1, 1.52)], -0.08, 0.08, col, bevel=0.018)
    mb.prism([(-0.38, 1.18), (0.38, 1.18), (0.4, 1.26), (0.38, 1.33), (-0.38, 1.33), (-0.4, 1.26)], -0.08, 0.08, col,
             bevel=0.018)
    with mb.xf(T(0, 0, 1.255) @ R(90, 'X')):
        mb.lathe([(0.2, -0.06), (0.28, -0.06), (0.28, 0.06), (0.2, 0.06)], 24, col=col, sh=AUTO, closed=True)
    knot = C("565B61")
    for k in range(6):
        z = 0.42 + 0.1 * k
        mb.box((0, -0.082, z), (0.14 if k % 2 else 0.08, 0.006, 0.03), knot)
    for sx in (-1, 1):
        mb.box((sx * 0.25, -0.082, 1.255), (0.12, 0.006, 0.03), knot)
    mb.blob((0, -0.082, 1.255), (0.05, 0.01, 0.05), sub=1, amp=0.0, col=knot)
    engrave(mb, -0.203, 0.2, [0.9, 0.6], w=0.36)
    rng = random.Random(8)
    for k in range(5):
        tuft(mb, (rng.uniform(-0.4, 0.4), rng.uniform(-0.3, 0.3), 0), rng, n=5, h=0.22)
    return mb


def make_grave_broken():
    mb = MB("SM_KG_Gravestone_Broken", angle=30)
    col = lambda p: grave_stone(p, base="ABA497", seed=5.0, moss=0.9)
    mb.cbox((0, 0.02, 0.07), (0.72, 0.32, 0.14), 0.02, col)
    jag = [(0.28, 0.5), (0.15, 0.58), (0.05, 0.47), (-0.08, 0.62), (-0.2, 0.52), (-0.28, 0.56)]
    mb.prism([(-0.28, 0.12), (0.28, 0.12)] + jag, -0.065, 0.065, col, bevel=0.018)
    engrave(mb, -0.068, 0.4, [0.8, 0.5], w=0.3)
    top = list(reversed(jag)) + [(0.28, 0.72)] + [(0.28 * math.cos(math.pi * k / 10), 0.72 + 0.28 * math.sin(math.pi * k / 10))
                                                   for k in range(1, 10)] + [(-0.28, 0.72)]
    with mb.xf(T(0.12, 0.28, 0.066) @ R(14, 'Z') @ R(88, 'X')):
        mb.prism(top, -0.065, 0.065, col, bevel=0.018)
    rng = random.Random(9)
    for k in range(4):
        with mb.xf(T(rng.uniform(-0.35, 0.4), rng.uniform(-0.35, -0.15), 0.02) @ R(rng.uniform(0, 90), 'Z')):
            mb.cbox((0, 0, 0), (0.07, 0.05, 0.04), 0.01, col)
    for k in range(6):
        tuft(mb, (rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.3), 0), rng, n=6, h=0.25)
    return mb


def make_grave_obelisk():
    mb = MB("SM_KG_Gravestone_Obelisk", angle=30)
    col = lambda p: grave_stone(p, base="C2BBAE", light="D8D2C6", dark="958E83", seed=6.0, moss=0.5)
    mb.cbox((0, 0, 0.1), (0.95, 0.95, 0.2), 0.025, col)
    mb.cbox((0, 0, 0.3), (0.75, 0.75, 0.2), 0.025, col)
    mb.cbox((0, 0, 0.675), (0.55, 0.55, 0.55), 0.02, col)
    mb.cbox((0, 0, 0.99), (0.64, 0.64, 0.08), 0.02, col)
    z0 = 1.03
    with mb.xf(T(0, 0, z0)):
        mb.lathe([(0, 0), (0.2 * 1.414, 0), (0.13 * 1.414, 1.6), (0.14 * 1.414, 1.6), (0, 1.84)], 4, col=col, sh=FLAT,
                 phase=math.pi / 4)
    mb.cbox((0, -0.279, 0.68), (0.36, 0.012, 0.32), 0.004, C("E3DDCF"))
    engrave(mb, -0.287, 0.78, [0.8, 1.0, 0.6], w=0.24)
    with mb.xf(T(0, -0.19, z0 + 0.75) @ R(90, 'X')):
        mb.lathe([(0.1, -0.012), (0.13, -0.012), (0.13, 0.012), (0.1, 0.012)], 14, col=C("6E7A48"), sh=AUTO, closed=True)
    rng = random.Random(10)
    for k in range(6):
        tuft(mb, (rng.uniform(-0.55, 0.55), rng.uniform(-0.55, 0.55), 0), rng, n=6, h=0.25)
    return mb


def make_grave_mound():
    mb = MB("SM_KG_GraveMound", angle=40)
    rng = random.Random(28)

    def rfn(a, i, z):
        return 1.0 + 0.06 * nz((math.cos(a) * 2, math.sin(a) * 2, z * 3), 1.0)

    def col(r, z, a, i):
        p = Vector((r * math.cos(a), r * math.sin(a), z))
        c = mix(C("6B4A2E"), C("54391F"), 0.5 + 0.5 * nz(p, 6.0))
        g = smoothstep(-0.1, 0.3, nz(p, 2.2, (3, 3, 3))) * 0.8
        return mix(c, C("5F8C34"), g)
    with mb.xf(S(2.05, 1.0, 1.0)):
        mb.lathe([(0, -0.03), (0.47, -0.03), (0.45, 0.06), (0.38, 0.16), (0.25, 0.24), (0.1, 0.28), (0, 0.29)], 14,
                 colfn=col, rfn=rfn, sh=AUTO)
    # wooden head cross tied with rope + flowers
    wood = C("7A5A3C")
    mb.beam((-1.05, 0, -0.1), (-1.05, 0, 0.78), 0.06, 0.05, wood, up=(0, 1, 0), bevel=0.008)
    mb.beam((-1.05, -0.2, 0.58), (-1.05, 0.2, 0.58), 0.05, 0.05, wood, up=(0, 0, 1), bevel=0.008)
    mb.box((-1.05, 0, 0.58), (0.07, 0.07, 0.07), ROPE)
    flowers = [C("F2E24A"), C("F2F0E8"), C("C84FA0"), C("E0402E")]
    for k in range(9):
        x, y = -0.6 + rng.uniform(-0.2, 0.25), rng.uniform(-0.15, 0.15)
        z = 0.2 + rng.uniform(0.0, 0.06)
        mb.tube([(x, y, z - 0.02), (x + rng.uniform(-0.04, 0.04), y, z + 0.1)], 0.006, 3, col=C("3F6B24"))
        mb.blob((x, y, z + 0.11), 0.03, sub=0, amp=0.1, col=rng.choice(flowers), sh=FLAT)
    for k in range(10):
        a = rng.random() * 6.28
        tuft(mb, (0.95 * math.cos(a), 0.45 * math.sin(a), -0.02), rng, n=6, h=0.22)
    return mb


def make_coffin():
    mb = MB("SM_KG_Coffin", angle=30)
    wood, lid, brass = C("5A3522"), C("6E4128"), C("C8A04A")
    out = [(-1.0, -0.22), (-0.45, -0.33), (1.0, -0.2), (1.0, 0.2), (-0.45, 0.33), (-1.0, 0.22)]

    def slab(pts, z0, z1, col, sc=1.0):
        bot = [mb.v((x * sc, y * sc, z0), col) for x, y in pts]
        top = [mb.v((x * sc, y * sc, z1), col) for x, y in pts]
        n = len(pts)
        for k in range(n):
            kn = (k + 1) % n
            mb.f_out((bot[k], bot[kn], top[kn], top[k]), (0, 0, (z0 + z1) / 2), None, FLAT)
        mb.f_dir(top, (0, 0, 1), None, FLAT)
        mb.f_dir(bot, (0, 0, -1), None, FLAT)
    slab(out, 0.0, 0.36, lambda p: mix(wood, C("4A2A1A"), 0.5 + 0.5 * math.sin(p.z * 60)))
    slab(out, 0.36, 0.43, lid, 1.04)
    slab(out, 0.43, 0.47, mul(lid, 1.1), 0.9)
    mb.box((0.05, 0, 0.48), (0.8, 0.06, 0.02), brass)
    mb.box((-0.2, 0, 0.48), (0.06, 0.34, 0.02), brass)
    for x in (-0.55, 0.0, 0.55):
        for sy in (-1, 1):
            yy = sy * (0.33 - (x + 0.45) * 0.13 / 1.45 * (1 if x > -0.45 else 0)) + sy * 0.03
            mb.tube([(x - 0.08, yy, 0.2), (x - 0.08, yy + sy * 0.03, 0.18), (x + 0.08, yy + sy * 0.03, 0.18),
                     (x + 0.08, yy, 0.2)], 0.012, 4, col=brass)
    return mb


def make_shovel():
    mb = MB("SM_KG_Shovel", angle=35)
    steel, dirt = C("7C7F86"), C("5E4430")
    out = [(0.0, 0.0), (0.09, 0.03), (0.12, 0.1), (0.12, 0.3), (0.035, 0.34), (-0.035, 0.34), (-0.12, 0.3), (-0.12, 0.1),
           (-0.09, 0.03)]
    mb.prism(out, -0.007, 0.007, lambda p: mix(steel, dirt, 1 - smoothstep(0.06, 0.18, p.z)), bevel=0.003)
    mb.tube([(0, 0, 0.3), (0, 0, 0.44)], [0.028, 0.02], 6, col=steel)
    mb.tube([(0, 0, 0.43), (0, 0, 1.18)], 0.018, 6, colfn=lambda i, k, p: C("B98A57"))
    mb.tube([(0, 0, 1.16), (-0.07, 0, 1.24), (-0.07, 0, 1.3), (0.07, 0, 1.3), (0.07, 0, 1.24), (0, 0, 1.16)], 0.014, 5,
            col=C("B98A57"), cap0=False, cap1=False)
    return mb


# ================================================================================================ more Japan
def make_hokora():
    mb = MB("SM_KG_Hokora", angle=35)
    rng = random.Random(31)
    st = lambda p: granite(p, seed=3.0, moss_amt=0.5, moss_up=1 - smoothstep(0.0, 0.4, p.z), scale=2.0)
    mb.cbox((0, 0, 0.13), (0.8, 0.68, 0.26), 0.03, st)
    mb.cbox((0, 0, 0.37), (0.62, 0.52, 0.22), 0.025, st)
    wood, dk = C("9C6B42"), C("5A3B26")
    z0 = 0.48
    mb.cbox((0, 0.02, z0 + 0.03), (0.56, 0.48, 0.06), 0.01, dk)
    mb.cbox((0, 0.04, z0 + 0.28), (0.44, 0.38, 0.44), 0.01, wood)
    for sx in (-1, 1):
        mb.box((sx * 0.23, -0.16, z0 + 0.28), (0.05, 0.05, 0.5), VERM)
    mb.box((0, -0.155, z0 + 0.25), (0.3, 0.02, 0.34), C("2E2018"))
    for k in range(5):
        mb.box((-0.12 + 0.06 * k, -0.17, z0 + 0.25), (0.012, 0.02, 0.34), C("C49A5E"))
    for z in (z0 + 0.14, z0 + 0.36):
        mb.box((0, -0.17, z), (0.3, 0.02, 0.012), C("C49A5E"))
    mb.box((0, -0.16, z0 + 0.52), (0.52, 0.06, 0.05), VERM)
    # gable roof (ridge along X), copper-green shingles, chigi + katsuogi
    with mb.xf(T(0, 0.04, z0 + 0.54) @ R(90, 'Z')):
        gable(mb, 0.42, -0.4, 0.4, 0.0, 0.26, 0.045, C("3E8A76"), rows=3,
              row_col=lambda r: mix(C("3E8A76"), C("57A68E"), 0.3 * (r % 2)))
    mb.tube([(-0.4, 0.04, z0 + 0.84), (0.4, 0.04, z0 + 0.84)], 0.03, 6, col=C("2F6B5B"))
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.beam((sx * 0.38, 0.04, z0 + 0.8), (sx * 0.44, 0.04 + sy * 0.1, z0 + 1.0), 0.03, 0.02, C("B9913E"))
    for x in (-0.18, 0.0, 0.18):
        mb.tube([(x, -0.03, z0 + 0.88), (x, 0.11, z0 + 0.88)], 0.025, 6, col=C("B9913E"))
    # shimenawa + shide
    rope = [Vector((x, -0.21 - 0.03 * (1 - (x / 0.26) ** 2), z0 + 0.46 - 0.04 * (1 - (x / 0.26) ** 2)))
            for x in np.linspace(-0.26, 0.26, 9)]
    mb.tube(rope, 0.018, 6, col=C("E3D29A"))
    for x in (-0.14, 0.0, 0.14):
        zz = z0 + 0.43
        pts = [(x - 0.02, -0.23, zz), (x + 0.02, -0.23, zz), (x + 0.02, -0.23, zz - 0.05), (x - 0.01, -0.23, zz - 0.07),
               (x + 0.02, -0.23, zz - 0.12), (x - 0.02, -0.23, zz - 0.12)]
        mb.poly2(pts, C("FFFFFF"))
    # offerings on the front ledge + fox guardians
    for k, x in enumerate((-0.2, 0.2)):
        with mb.xf(T(x, -0.3, 0.48)):
            mb.lathe([(0, 0), (0.025, 0), (0.035, 0.035), (0, 0.035)], 8, col=C("F4EFE3"), sh=AUTO)
    mb.blob((0.0, -0.29, 0.51), 0.03, sub=1, amp=0.0, col=C("F08A1E"))
    for sx in (-1, 1):
        with mb.xf(T(sx * 0.55, -0.38, 0.0) @ R(-sx * 15, 'Z')):
            mb.cbox((0, 0, 0.06), (0.2, 0.18, 0.12), 0.02, st)
            mb.blob((0, 0.02, 0.22), (0.07, 0.08, 0.1), sub=1, amp=0.05, col=C("F2EEE6"))
            mb.blob((0, -0.02, 0.35), (0.055, 0.06, 0.055), sub=1, amp=0.05, col=C("F2EEE6"))
            mb.lathe([(0, 0), (0.04, 0), (0, 0.06)], 4, col=C("F2EEE6"), sh=FLAT) if False else None
            for ex in (-1, 1):
                with mb.xf(T(ex * 0.03, -0.01, 0.39)):
                    mb.lathe([(0, 0), (0.018, 0), (0, 0.05)], 4, col=C("F2EEE6"), sh=FLAT)
            mb.lathe([(0, 0), (0.06, 0), (0, -0.06)], 6, col=VERM, sh=FLAT) if False else None
            with mb.xf(T(0, -0.055, 0.3)):
                mb.lathe([(0, -0.07), (0.055, 0.0), (0, 0.0)], 6, col=VERM, sh=FLAT)
            mb.box((0, -0.08, 0.36), (0.03, 0.02, 0.012), C("2A2426"))
    for k in range(5):
        tuft(mb, (rng.uniform(-0.5, 0.5), rng.uniform(-0.4, 0.4), 0), rng, n=5, h=0.2)
    return mb


def make_ema_board():
    mb = MB("SM_KG_EmaBoard", angle=35)
    rng = random.Random(32)
    wood, dk = C("8A5A36"), C("5A3B26")
    for sx in (-1, 1):
        mb.cbox((sx * 0.82, 0, 0.95), (0.09, 0.09, 1.9), 0.01, dk)
    for z in (1.42, 1.02):
        mb.cbox((0, 0, z), (1.8, 0.05, 0.05), 0.008, wood)
    mb.cbox((0, 0, 1.88), (1.95, 0.12, 0.06), 0.01, dk)
    with mb.xf(T(0, 0, 1.91) @ R(90, 'Z')):
        gable(mb, 0.32, -1.08, 1.08, 0.0, 0.2, 0.04, C("4A3A30"), rows=2,
              row_col=lambda r: mix(C("4A3A30"), C("5E4A3C"), r % 2))
    paints = [C("D0402E"), C("2E6FB0"), C("3E8A3E"), C("E0A030"), C("F2EEE6"), C("8A3FA0")]
    for z in (1.42, 1.02):
        x = -0.78
        while x < 0.76:
            for side in (-1, 1):
                if side > 0 and rng.random() < 0.55:
                    continue
                with mb.xf(T(x + rng.uniform(-0.01, 0.01), side * 0.035, z - 0.04) @ R(rng.uniform(-8, 8), 'Y') @
                           R(180 if side > 0 else 0, 'Z')):
                    mb.prism([(-0.075, -0.1), (0.075, -0.1), (0.075, -0.03), (0.0, 0.0), (-0.075, -0.03)], -0.006,
                             0.006, C("E2C699", v=0.9 + 0.15 * rng.random()), bevel=0.0)
                    mb.cbox((0.0, -0.008, -0.06), (0.09, 0.004, 0.055), 0.004, rng.choice(paints))
                    mb.box((0.035, -0.008, -0.075), (0.012, 0.004, 0.04), C("2A2426"))
                    mb.tube([(-0.02, 0, 0.0), (0, 0, 0.03), (0.02, 0, 0.0)], 0.003, 3, col=VERM)
            x += rng.uniform(0.1, 0.13)
    return mb


def make_jizo():
    mb = MB("SM_KG_Jizo", angle=35)
    rng = random.Random(33)
    st = lambda p: granite(p, base="9E9A93", light="BAB6AE", dark="6E6A64", seed=7.0, moss_amt=0.6,
                           moss_up=1 - smoothstep(0.0, 0.3, p.z), scale=3.0)
    mb.lathe([(0, 0), (0.26, 0), (0.26, 0.12), (0, 0.12)], 6, col=st, sh=FLAT)
    mb.lathe([(0, 0.12), (0.2, 0.12), (0.23, 0.16), (0.2, 0.2), (0, 0.2)], 12, col=st, sh=AUTO)
    mb.lathe([(0, 0.2), (0.14, 0.2), (0.15, 0.3), (0.145, 0.44), (0.12, 0.52), (0.07, 0.56), (0, 0.57)], 12, col=st,
             sh=SMOOTH)
    mb.blob((0, 0, 0.64), (0.1, 0.095, 0.105), sub=2, amp=0.0, col=st, sh=SMOOTH)
    mb.blob((0, -0.12, 0.42), (0.045, 0.035, 0.05), sub=1, amp=0.0, col=st)
    for sx in (-1, 1):
        mb.box((sx * 0.035, -0.093, 0.645), (0.03, 0.006, 0.006), C("4A4640"))
    mb.box((0, -0.097, 0.605), (0.02, 0.005, 0.005), C("4A4640"))
    red, red_dk = C("D8322A"), C("A82420")
    mb.lathe([(0.07, 0.555), (0.11, 0.54), (0.175, 0.4), (0.19, 0.33), (0.16, 0.33), (0.14, 0.4), (0.09, 0.53),
              (0.07, 0.545)], 14, colfn=lambda r, z, a, i: red if math.sin(a) < 0.3 else red_dk, sh=SMOOTH, closed=True)
    with mb.xf(T(0, 0, 0.67)):
        mb.lathe([(0, 0.0), (0.112, 0.0), (0.115, 0.03), (0.1, 0.08), (0.06, 0.12), (0, 0.13)], 12, col=red, sh=SMOOTH)
        mb.lathe([(0.1, -0.015), (0.125, -0.005), (0.125, 0.02), (0.1, 0.03)], 12, col=red_dk, sh=SMOOTH, closed=True)
    mb.blob((0, 0, 0.82), 0.03, sub=1, amp=0.1, col=C("F2EEE6"))
    # pinwheel + coins
    with mb.xf(T(0.2, -0.08, 0.12)):
        mb.tube([(0, 0, 0), (0, 0, 0.42)], 0.006, 4, col=C("B9913E"))
        for k, c in enumerate((C("E0402E"), C("F2C12E"), C("2E8AD0"), C("3EA05A"))):
            a = k * math.pi / 2
            mb.poly2([(0, -0.01, 0.42), (0.08 * math.cos(a), -0.01, 0.42 + 0.08 * math.sin(a)),
                      (0.08 * math.cos(a + 0.9), -0.01, 0.42 + 0.08 * math.sin(a + 0.9))], c)
    for k in range(3):
        mb.lathe([(0, 0), (0.02, 0), (0.02, 0.004), (0, 0.004)], 8, col=C("C8A04A"), sh=FLAT) if False else None
        mb.blob((-0.12 + 0.04 * k, -0.2, 0.125), (0.02, 0.02, 0.004), sub=0, amp=0.0, col=C("C8A04A"))
    for k in range(4):
        tuft(mb, (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 0), rng, n=5, h=0.18)
    return mb


def make_chochin_string():
    mb = MB("SM_KG_ChochinString", angle=40, material=MAT_GLOW)
    wire = catenary((0, 0, 0), (6.0, 0, 0), 0.45, 30)
    mb.tube(wire, 0.008, 4, col=C("2A2426"))
    for x in (0.0, 6.0):
        with mb.xf(T(x, 0, 0) @ R(90, 'X')):
            mb.lathe([(0.03, -0.006), (0.045, -0.006), (0.045, 0.006), (0.03, 0.006)], 8, col=IRON, closed=True)
    reds = [C("E0301E", a=0.7), C("F4E2B0", a=0.7), C("E0301E", a=0.7), C("F2B640", a=0.7)]
    for k in range(9):
        x = 0.6 + 0.6 * k
        t = x / 6.0
        zw = -0.45 * 4 * t * (1 - t)
        mb.tube([(x, 0, zw), (x, 0, zw - 0.06)], 0.004, 3, col=IRON)
        top = zw - 0.06
        H = 0.3
        paper = reds[k % len(reds)]
        prof = [(0, top), (0.05, top), (0.05, top - 0.025)]
        for j in range(9):
            s = j / 8
            prof.append((0.055 + 0.08 * math.sin(math.pi * s) ** 0.7, top - 0.025 - H * s))
        prof += [(0.05, top - H - 0.03), (0.05, top - H - 0.05), (0, top - H - 0.05)]
        n = len(prof)
        prof = [(r, z) for r, z in prof]
        # lathe expects bottom->top for outward normals: reverse
        prof = prof[::-1]
        with mb.xf(T(x, 0, 0)):
            mb.lathe(prof, 10, colfn=lambda r, z, a, i, paper=paper: C("1C1A1A") if i < 3 or i > n - 4 else
                     (paper if i % 2 else mul(paper, 0.8)), sh=SMOOTH)
            mb.tube([(0, 0, top - H - 0.05), (0, 0, top - H - 0.14)], [0.015, 0.02], 5, col=C("C8342A"))
            if k % 2 == 0:
                mb.box((0, -0.125, top - H * 0.5), (0.05, 0.02, 0.13), C("2A1414", a=0.7))
    return mb


def carp(mb, z, L, R_, body, belly, fin, seed):
    rng = random.Random(seed)
    n = 14
    pts, radii = [], []
    for i in range(n):
        s = i / (n - 1)
        x = 0.12 + L * s
        pts.append(Vector((x, 0.05 * math.sin(s * 5 + seed) * s, z - 0.35 * s * s + 0.07 * math.sin(s * 7 + seed) * s)))
        radii.append(R_ * interp_r(s))

    def col(i, k, p):
        s = i / (n - 1)
        a = 2 * math.pi * k / 12
        c = mix(body, belly, smoothstep(0.2, -0.8, math.sin(a)))
        c = mul(c, 0.9 + 0.12 * math.sin(s * 34.0))
        if i == 0:
            c = C("F2EEE6")
        return with_a(c, clamp(s * 1.1))
    mb.tube(pts, radii, 12, colfn=col, sh=SMOOTH, capcol=C("1C1A1A", a=0.0), cap1=False)
    # open tail end + tail fins
    e = pts[-1]
    for sz in (-1, 1):
        mb.poly2([e + Vector((-0.08, 0, sz * 0.02)), e + Vector((0.4 * L / 2, 0, sz * R_ * 1.3)),
                  e + Vector((0.3 * L / 2, 0, sz * R_ * 0.2))], with_a(fin, 1.0))
    # eyes
    for sy in (-1, 1):
        p = pts[1] + Vector((0.0, sy * radii[1] * 0.92, radii[1] * 0.25))
        mb.blob(p, (R_ * 0.22, R_ * 0.08, R_ * 0.22), sub=1, amp=0.0, col=with_a(C("F2EEE6"), 0.05))
        mb.blob(p + Vector((0.01, sy * R_ * 0.05, 0)), (R_ * 0.11, R_ * 0.06, R_ * 0.11), sub=1, amp=0.0,
                col=with_a(C("111111"), 0.05))
    # pectoral fins
    for sy in (-1, 1):
        p = pts[3] + Vector((0, sy * radii[3] * 0.9, -radii[3] * 0.4))
        mb.poly2([p, p + Vector((0.25 * L / 2, sy * R_ * 0.5, -R_ * 0.4)), p + Vector((0.3 * L / 2, sy * 0.05, 0))],
                 with_a(fin, 0.3))
    return pts[0]


def interp_r(s):
    tab = [(0.0, 0.62), (0.08, 0.9), (0.25, 1.0), (0.5, 0.9), (0.75, 0.62), (0.9, 0.42), (1.0, 0.36)]
    for (s0, v0), (s1, v1) in zip(tab, tab[1:]):
        if s <= s1:
            return lerp(v0, v1, (s - s0) / (s1 - s0))
    return tab[-1][1]


def make_koinobori():
    mb = MB("SM_KG_Koinobori", angle=40, material=MAT_SWAY)
    mb.tube([(0, 0, -0.1), (0, 0, 6.9)], [0.085, 0.05], 8,
            colfn=lambda i, k, p: C("B99C5A") if int(p.z * 2.5) % 2 else C("A48A4E"), sh=SMOOTH)
    for z in np.arange(0.4, 6.8, 0.4):
        mb.lathe([(0.07 - z * 0.004, z - 0.02), (0.08 - z * 0.004, z), (0.07 - z * 0.004, z + 0.02)], 8, col=C("8A7440"))
    rock(mb, (0, 0, 0), (0.3, 0.3, 0.18), seed=341, sub=1, cuts=5, colfn=lambda w, u: granite(w, seed=1.0), bottom=-0.05)
    # arrow wheel + ball on top
    with mb.xf(T(0, 0, 6.95)):
        mb.blob((0, 0, 0.12), 0.07, sub=1, amp=0.0, col=GOLD)
        for k in range(8):
            a = k * math.pi / 4
            mb.beam((0, 0, 0), (0.22 * math.cos(a), 0, 0.22 * math.sin(a)), 0.012, 0.03, C("E0A030"), up=(0, 1, 0))
    # fukinagashi: five colour ribbons
    ribbons = [C("2E6FB0"), C("F2EEE6"), C("D0402E"), C("F2C12E"), C("3E8A3E")]
    for k, c in enumerate(ribbons):
        a0 = 2 * math.pi * k / 5
        grid = []
        for i in range(8):
            s = i / 7
            x = 0.12 + 1.9 * s
            zc = 6.45 - 0.5 * s * s + 0.08 * math.sin(s * 8 + k)
            row = []
            for j in range(2):
                a = a0 + j * 2 * math.pi / 5
                row.append(Vector((x, 0.16 * math.cos(a) * (1 - 0.3 * s), zc + 0.16 * math.sin(a) * (1 - 0.3 * s))))
            grid.append(row)
        mb.sheet2(grid, lambda i, j, p, c=c: with_a(c, clamp((p.x - 0.1) / 1.9)))
    mb.lathe([(0.15, 6.43), (0.19, 6.45), (0.15, 6.47)], 10, col=GOLD, closed=True) if False else None
    with mb.xf(T(0.12, 0, 6.45) @ R(90, 'Y')):
        mb.lathe([(0.15, -0.02), (0.18, -0.02), (0.18, 0.02), (0.15, 0.02)], 10, col=GOLD, closed=True)
    # carp: father (black), mother (red), child (blue)
    for (z, L, R_, body, belly, seed) in ((5.55, 2.3, 0.3, C("1E2530"), C("D8B45A"), 1),
                                          (4.75, 1.85, 0.25, C("D83A2E"), C("F4C9A0"), 2),
                                          (4.05, 1.45, 0.2, C("2F6FBF"), C("DCE8F2"), 3)):
        mouth = carp(mb, z, L, R_, body, belly, mix(body, C("FFFFFF"), 0.3), seed)
        mb.tube([(0.05, 0, z + 0.03), mouth + Vector((0, 0, R_ * 0.6))], 0.008, 3, col=IRON)
    return mb


def make_tsukubai():
    mb = MB("SM_KG_Tsukubai", angle=35)
    rng = random.Random(34)
    mb.lathe([(0, 0.0), (0.7, 0.0), (0.72, 0.02), (0, 0.025)], 18, col=C("3E3C3A"), sh=AUTO)
    for k in range(26):
        a, r = rng.random() * 6.28, math.sqrt(rng.random()) * 0.62
        mb.blob((r * math.cos(a), r * math.sin(a), 0.03), (0.04, 0.035, 0.02), sub=0, amp=0.1,
                col=rng.choice((C("5E5A55"), C("8C8780"), C("2E2C2A"))), sh=FLAT)
    st = lambda p: granite(p, seed=9.0, moss_amt=0.8, moss_up=1 - smoothstep(0.0, 0.25, p.z), scale=2.5)

    def rfn(a, i, z):
        return 1.0 + 0.06 * nz((math.cos(a), math.sin(a), z * 2), 1.5, (4, 4, 4))
    mb.lathe([(0, 0.0), (0.3, 0.0), (0.34, 0.12), (0.33, 0.3), (0.29, 0.38), (0.19, 0.38), (0.17, 0.33), (0, 0.31)], 16,
             col=st, rfn=rfn, sh=AUTO)
    mb.lathe([(0, 0.335), (0.175, 0.335), (0, 0.336)], 14, col=C("3E7FA6"), sh=FLAT)
    bam = lambda p: with_a(mix(C("8FA048"), C("B9B35A"), 0.5 + 0.5 * math.sin(p.z * 9 + p.x * 9)), 0.0)
    mb.tube([(0.42, 0.28, -0.02), (0.42, 0.28, 0.82)], 0.035, 7, col=bam)
    mb.tube([(0.45, 0.31, 0.72), (0.1, 0.05, 0.6)], 0.022, 6, col=bam)
    mb.tube([(0.1, 0.05, 0.6), (0.08, 0.035, 0.34)], 0.008, 4, col=C("9CC8E0"))
    mb.tube([(-0.28, -0.12, 0.395), (0.22, 0.14, 0.395)], 0.008, 4, col=C("C9B27E"))
    with mb.xf(T(-0.28, -0.12, 0.37)):
        mb.lathe([(0, 0), (0.035, 0), (0.035, 0.06), (0, 0.06)], 8, col=C("C9B27E"), sh=AUTO)
    rock(mb, (0.0, -0.62, 0), (0.28, 0.2, 0.1), seed=345, sub=1, cuts=5, colfn=lambda w, u: granite(w, seed=2.0),
         bottom=-0.02)
    rock(mb, (-0.58, 0.18, 0), (0.2, 0.2, 0.22), seed=346, sub=1, cuts=5,
         colfn=lambda w, u: granite(w, seed=3.0, moss_amt=0.7, moss_up=u.z), bottom=-0.02)
    for k in range(5):
        a = rng.random() * 6.28
        tuft(mb, (0.72 * math.cos(a), 0.72 * math.sin(a), 0), rng, n=6, h=0.22)
    return mb


def make_bamboo_fence():
    mb = MB("SM_KG_BambooFence", angle=35)
    rng = random.Random(35)
    greens = [C("B9B35A"), C("A4AA4E"), C("C9B865"), C("93A04A")]
    for sx in (-1, 1):
        mb.tube([(sx * 0.97, 0, -0.05), (sx * 0.97, 0, 1.5)], 0.045, 8, col=C("8C7A48"), capcol=C("D8C98A"))
    n = 38
    for k in range(n):
        x = -0.92 + 1.84 * k / (n - 1)
        c = rng.choice(greens)
        h = 1.3 + rng.uniform(-0.02, 0.02)
        mb.tube([(x, 0, 0.02), (x, 0, h * 0.5), (x, 0, h)], 0.024, 5,
                colfn=lambda i, kk, p, c=c: mul(c, 0.85 if i == 1 else 1.0), capcol=C("E3D9A0"), phase=0.3 * k)
    rope = C("2A2426")
    for z in (0.3, 0.72, 1.12):
        for sy in (-1, 1):
            mb.tube([(-0.97, sy * 0.045, z), (0.97, sy * 0.045, z)], 0.02, 6, col=C("8C9A48"))
        for x in np.linspace(-0.8, 0.8, 5):
            mb.cbox((x, 0, z), (0.035, 0.13, 0.05), 0.01, rope)
    mb.tube([(-0.99, 0, 1.33), (0.99, 0, 1.33)], 0.036, 7, col=C("8C7A48"), capcol=C("D8C98A"))
    return mb


def make_zen_sand():
    mb = MB("SM_KG_ZenSand", angle=35)
    rng = random.Random(36)
    sand, sand_d, sand_l = C("E3DAC3"), C("CFC4A8"), C("F1EAD8")
    kerb = lambda p: granite(p, base="8C877F", light="A8A399", dark="6A665F", seed=4.0, scale=2.0)
    for side in range(4):
        with mb.xf(R(90 * side, 'Z')):
            x = -2.0
            while x < 1.82:
                L = rng.uniform(0.4, 0.6)
                L = min(L, 1.82 - x)
                mb.cbox((x + L / 2, -1.91, 0.06), (L - 0.02, 0.18, 0.12), 0.02, kerb)
                x += L
    mb.box((0, 0, 0.025), (3.64, 3.64, 0.05), sand_d)
    rocks = [((0.75, 0.45), (0.45, 0.35, 0.55), 361), ((-0.85, -0.7), (0.3, 0.26, 0.32), 362),
             ((-0.95, 0.85), (0.2, 0.17, 0.2), 363)]
    circles = []
    for (x, y), half, seed in rocks:
        mr = max(half[0], half[1]) + 0.14
        with mb.xf(T(x, y, 0.05)):
            mb.lathe([(0, 0.0), (mr, 0.0), (mr * 0.9, 0.04), (mr * 0.5, 0.06), (0, 0.065)], 16,
                     colfn=lambda r, z, a, i: with_a(mix(MOSS, MOSS_L, 0.5 + 0.5 * math.sin(a * 5)), 0.0),
                     rfn=lambda a, i, z: 1 + 0.08 * math.sin(a * 3 + 1), sh=AUTO)
        rock(mb, (x, y, 0.0), half, seed, sub=2, cuts=8,
             colfn=lambda w, u: granite(w, base="7E8288", light="A2A6AB", dark="54585E", seed=seed, moss_amt=0.8,
                                        moss_up=smoothstep(0.5, 0.9, u.z)), bottom=0.02)
        rings = []
        for i in range(4):
            r = mr + 0.1 + 0.1 * i
            with mb.xf(T(x, y, 0.0)):
                mb.lathe([(r - 0.04, 0.049), (r, 0.075), (r + 0.04, 0.049)], 32,
                         colfn=lambda rr, z, a, ii: sand_l if ii == 1 else sand_d, sh=SMOOTH)
            rings.append(r)
        circles.append((x, y, rings[-1] + 0.07))
    y = -1.75
    while y <= 1.76:
        # split the straight ridge around every ring zone
        cuts = []
        for (cx, cy, cr) in circles:
            d = abs(y - cy)
            if d < cr:
                w = math.sqrt(cr * cr - d * d)
                cuts.append((cx - w, cx + w))
        cuts.sort()
        segs, x0 = [], -1.8
        for a, b in cuts:
            if a > x0:
                segs.append((x0, min(a, 1.8)))
            x0 = max(x0, b)
        if x0 < 1.8:
            segs.append((x0, 1.8))
        for xa, xb in segs:
            if xb - xa < 0.05:
                continue
            a0 = mb.v((xa, y - 0.04, 0.049), sand_d)
            b0 = mb.v((xb, y - 0.04, 0.049), sand_d)
            a1 = mb.v((xa, y, 0.075), sand_l)
            b1 = mb.v((xb, y, 0.075), sand_l)
            a2 = mb.v((xa, y + 0.04, 0.049), sand_d)
            b2 = mb.v((xb, y + 0.04, 0.049), sand_d)
            mb.f_dir((a0, b0, b1, a1), (0, -1, 1), None, SMOOTH)
            mb.f_dir((a1, b1, b2, a2), (0, 1, 1), None, SMOOTH)
        y += 0.1
    return mb


def make_small_torii():
    mb = MB("SM_KG_SmallTorii", angle=35)
    PX = 0.78

    def verm_grad(r, z, a, i):
        return mix(C("B9443A"), VERM, smoothstep(0.3, 1.5, z))
    prof = [(0, -0.1), (0.1, -0.1), (0.1, 0.24), (0.085, 0.27), (0.08, 1.2), (0.075, 2.2), (0, 2.2)]
    segcol = [LACQUER, LACQUER, LACQUER, None, None, None]
    for sx in (-1, 1):
        with mb.xf(T(sx * PX, 0, 0)):
            mb.lathe(prof, 14, colfn=verm_grad, segcol=segcol, sh=AUTO)
    mb.cbox((0, 0, 1.72), (2.0, 0.09, 0.13), 0.01, VERM)
    for sx in (-1, 1):
        mb.box((sx * (PX + 0.13), 0, 1.72), (0.05, 0.11, 0.16), VERM_DK)
    HALF = 1.28

    def zk(x):
        return 2.2 + 0.2 * (abs(x) / HALF) ** 2.5
    xs = [HALF * (-1 + 2 * i / 30) for i in range(31)]
    mb.sweep([(x * 0.84, 0, zk(x * 0.84) - 0.02) for x in xs], [(-0.08, -0.12), (0.08, -0.12), (0.08, 0), (-0.08, 0)],
             col=VERM, vertical=True)
    mb.sweep([(x, 0, zk(x)) for x in xs], [(-0.1, 0.0), (0.1, 0.0), (0.1, 0.1), (-0.1, 0.1)], col=VERM, vertical=True)
    mb.sweep([(x * 1.03, 0, zk(x * 1.03)) for x in xs], [(-0.12, 0.1), (0.12, 0.1), (0.12, 0.13), (0.05, 0.17),
                                                        (-0.05, 0.17), (-0.12, 0.13)], col=LACQUER, vertical=True)
    mb.box((0, 0, 1.94), (0.08, 0.08, 0.36), VERM)
    mb.cbox((0, 0, 1.95), (0.24, 0.05, 0.28), 0.01, GOLD)
    mb.box((0, 0, 1.95), (0.19, 0.07, 0.23), C("1B2140"))
    mb.box((0, 0, 1.95), (0.03, 0.08, 0.14), GOLD)
    return mb


# ================================================================================================ registry
# name -> (builder, material, collision, preview view)
SPECS = {
    "CaveMouth": (make_cave, "VC", "complex", dict(el=14, az=-28)),
    "Mausoleum": (make_mausoleum, "Glow", "complex", dict(el=12, az=-32)),
    "TeaHouse": (make_teahouse, "Glow", "complex", dict(el=14, az=-35)),
    "WoodBridge": (make_wood_bridge, "VC", "complex", dict(el=18, az=-30, ground=-1.3)),
    "Campfire": (make_campfire, "Glow", "box", dict(el=28, az=-30)),
    "Tent_A": (make_tent_a, "VC", "complex", dict(el=16, az=-38)),
    "Menhir_A": (lambda: make_menhir("SM_KG_Menhir_A", 4.0, 1.0, 0.7, 11, lean=0.04), "VC", "complex", dict(el=10, az=-30)),
    "Menhir_B": (lambda: make_menhir("SM_KG_Menhir_B", 3.1, 1.5, 0.62, 12, spiral=True, taper=0.25), "VC", "complex",
                 dict(el=10, az=-20)),
    "Menhir_C": (lambda: make_menhir("SM_KG_Menhir_C", 2.6, 1.3, 0.85, 13, notch=0.55, taper=0.15, stone="8E8A84"),
                 "VC", "complex", dict(el=10, az=-30)),
    "GateArch": (make_gate_arch, "VC", "complex", dict(el=10, az=-25)),
    "Gravestone_Round": (make_grave_round, "VC", "box", dict(el=12, az=-30)),
    "Gravestone_Cross": (make_grave_cross, "VC", "box", dict(el=12, az=-30)),
    # ---- batch 2
    "Tent_Lean": (make_tent_lean, "VC", "complex", dict(el=16, az=-35)),
    "LogBench": (make_log_bench, "VC", "box", dict(el=22, az=-55)),
    "FallenLog": (make_fallen_log, "VC", "box", dict(el=22, az=-25)),
    "Stump_Big": (make_stump, "VC", "box", dict(el=24, az=-30)),
    "RuinWall_A": (make_ruin_wall_a, "VC", "complex", dict(el=12, az=-25)),
    "RuinWall_B": (make_ruin_wall_b, "VC", "complex", dict(el=18, az=-40)),
    "RuinArch": (make_ruin_arch, "VC", "complex", dict(el=10, az=-25)),
    "RuinPillar": (make_ruin_pillar, "VC", "complex", dict(el=12, az=-30)),
    "Signpost": (make_signpost, "VC", "complex", dict(el=10, az=-30)),
    "MushroomRing": (make_mushroom_ring, "VC", "none", dict(el=30, az=-30)),
    "BerryBush": (make_berry_bush, "Sway", "none", dict(el=15, az=-30)),
    "Gravestone_Celtic": (make_grave_celtic, "VC", "box", dict(el=12, az=-30)),
    "Gravestone_Broken": (make_grave_broken, "VC", "box", dict(el=22, az=-30)),
    "Gravestone_Obelisk": (make_grave_obelisk, "VC", "box", dict(el=12, az=-30)),
    "GraveMound": (make_grave_mound, "VC", "none", dict(el=25, az=-35)),
    "Coffin": (make_coffin, "VC", "box", dict(el=25, az=-35)),
    "Shovel": (make_shovel, "VC", "none", dict(el=10, az=-30)),
    "Hokora": (make_hokora, "VC", "box", dict(el=14, az=-30)),
    "EmaBoard": (make_ema_board, "VC", "box", dict(el=12, az=-25)),
    "Jizo": (make_jizo, "VC", "box", dict(el=12, az=-25)),
    "ChochinString": (make_chochin_string, "Glow", "none", dict(el=8, az=-15, ground=-1.5)),
    "Koinobori": (make_koinobori, "Sway", "complex", dict(el=8, az=-20)),
    "Tsukubai": (make_tsukubai, "VC", "box", dict(el=28, az=-30)),
    "BambooFence": (make_bamboo_fence, "VC", "box", dict(el=12, az=-25)),
    "ZenSand": (make_zen_sand, "VC", "none", dict(el=40, az=-25)),
    "SmallTorii": (make_small_torii, "VC", "complex", dict(el=10, az=-25)),
}
BATCH1 = ["CaveMouth", "Mausoleum", "TeaHouse", "WoodBridge", "Campfire", "Tent_A", "Menhir_A", "Menhir_B", "Menhir_C",
          "GateArch", "Gravestone_Round", "Gravestone_Cross"]


# ================================================================================================ build + export
def build_all(names):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs, stats = [], []
    for name in names:
        fn = SPECS[name][0]
        mb = fn()
        assert mb.name == "SM_KG_" + name, (mb.name, name)
        mb.material = {"VC": MAT_VC, "Glow": MAT_GLOW, "Sway": MAT_SWAY}[SPECS[name][1]]
        obj, tris, diag = finalize(mb)
        objs.append(obj)
        mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
        mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
        stats.append((obj.name, tris, mn, mx, diag))
    print("\nKG_WILDS: object                     tris     min (x, y, z)              max (x, y, z)          size")
    for name, tris, mn, mx, diag in stats:
        sz = mx - mn
        print(f"KG_WILDS: {name:26s} {tris:6d}  ({mn.x:6.2f},{mn.y:6.2f},{mn.z:6.2f})  ({mx.x:6.2f},{mx.y:6.2f},{mx.z:6.2f})"
              f"  {sz.x:5.2f} x {sz.y:5.2f} x {sz.z:5.2f}  {diag}")
    print(f"KG_WILDS: total tris {sum(s[1] for s in stats)}")
    return objs, stats


def export(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"KG_WILDS: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


def write_manifest(path, names):
    data = {"props": {n: {"material": SPECS[n][1], "collision": SPECS[n][2]} for n in names}}
    with open(path, "w") as f:
        json.dump(data, f, indent=1)
    print(f"KG_WILDS: manifest {path} ({len(names)} props)")


# ================================================================================================ preview
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
        rgb = nt.nodes.new("ShaderNodeRGB")
        rgb.outputs[0].default_value = (1.0, 0.53, 0.18, 1.0)
        nt.links.new(rgb.outputs[0], bsdf.inputs["Emission Color"])
        nt.links.new(m.outputs[0], bsdf.inputs["Emission Strength"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    mat.use_backface_culling = True
    return mat


def _flat_mat(name, rgb):
    mat = bpy.data.materials.new(name)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat


def render_previews(objs, closeups=None):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 32
        sc.eevee.use_shadows = True
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x = sc.render.resolution_y = TILE
    world = bpy.data.worlds.new("KG_PreviewWorld")
    sc.world = world
    bg = world.node_tree.nodes.get("Background") or next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
    if bg:
        bg.inputs["Color"].default_value = (0.52, 0.68, 0.86, 1.0)
        bg.inputs["Strength"].default_value = 0.75
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.8
    sun.data.angle = math.radians(4)
    sun.data.color = (1.0, 0.95, 0.86)
    sc.collection.objects.link(sun)
    pm = {MAT_GLOW: _node_mat("P_Glow", True)}
    plain = _node_mat("P_Plain", False)
    for o in objs:
        o.data.materials[0] = pm.get(o.data.materials[0].name, plain)
    gm = bpy.data.meshes.new("PreviewGround")
    gm.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PreviewGround", gm)
    sc.collection.objects.link(ground)
    ground.data.materials.append(_flat_mat("P_Ground", (0.3, 0.34, 0.22)))
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
    em = next((n for n in lm.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if em:
        em.inputs["Base Color"].default_value = (0.02, 0.02, 0.03, 1)
    lbl.data.materials.append(lm)
    lbl.parent = cam
    lbl.location = (-0.345, -0.345, -1.0)
    tan_h = math.tan(math.atan(18 / 50))
    tmp = tempfile.mkdtemp(prefix="kg_wilds_")
    by_name = {o.name: o for o in objs}

    def shoot(name, path, az=None, el=None, zoom=1.0):
        o = by_name[name]
        cfg = dict(el=14, az=-30, ground=0.0)
        cfg.update(SPECS[name.replace("SM_KG_", "")][3])
        for x in objs:
            x.hide_render = x is not o
        ground.location.z = cfg.get("ground", 0.0)
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
        dist = (dist * 1.06 + 0.05) * zoom
        cam.location = c - f * dist
        cam.rotation_euler = f.to_track_quat("-Z", "Y").to_euler()
        lbl.data.body = name.replace("SM_KG_", "")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)

    os.makedirs(PREVIEW_DIR, exist_ok=True)
    if closeups:
        for name in closeups:
            full = "SM_KG_" + name
            for tag, azo, elo in (("a", None, None), ("b", SPECS[name][3].get("az", -30) + 150, None),
                                  ("c", SPECS[name][3].get("az", -30) + 10, 35)):
                p = os.path.join(PREVIEW_DIR, f"DressWilds_{name}_{tag}.png")
                shoot(full, p, az=azo, el=elo)
                print(f"KG_WILDS: closeup {p}")
        return
    names = [o.name for o in objs]
    for si in range(0, len(names), 8):
        group = names[si:si + 8]
        cols, rows = 4, 2
        sheet = np.zeros((rows * TILE, cols * TILE, 4), np.float32)
        for k, name in enumerate(group):
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
        path = os.path.join(PREVIEW_DIR, f"DressWilds_preview_{si // 8 + 1}.png")
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        print(f"KG_WILDS: preview {path}")
    shutil.rmtree(tmp, ignore_errors=True)


def main():
    names = ONLY or list(SPECS)
    objs, stats = build_all(names)
    export(OUT_GLB)
    if "--manifest" in _opt:
        write_manifest(_abs(_opt["--manifest"]), names)
    if "--closeup" in _opt:
        render_previews(objs, closeups=_opt["--closeup"].split(","))
    elif "--no-preview" not in _opt:
        render_previews(objs)


main()
