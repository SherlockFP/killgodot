"""Procedural village & farm dressing props for Morrowmere (pack KG_DressVillage). Headless Blender 5.2, no textures.
Stylised Quaternius-like low poly: chunky, slightly bevelled, flat-shaded, warm saturated palette, real-world scale.

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_village.py -- \
      [out.glb] [preview_dir] [--only A,B] [--no-preview] [--no-verify] [--closeup A,B] [--tile 512]
      [--manifest path.json] [--prefix DressVillage_preview] [--no-export]

Output: Art/Packed/KG_DressVillage.glb (one mesh object per prop, SM_KG_<Name>, metres, Z up), the import manifest
Art/Packed/KG_DressVillage_Clean.json and contact sheets Art/Concept/DressVillage_preview_<n>.png.

Vertex colour attribute "Col" (FLOAT_COLOR, face corner, exported as COLOR_0): RGB = LINEAR albedo, A = mask:
  Sway material (M_KG_JapanFoliage): 0 at anchors -> 1 at free-hanging cloth / leaves / ribbons
  Glow material (M_KG_JapanGlow):    1 on emissive faces
  VC material (M_KG_PropVCLinear):   0
Pivots: bottom centre (min Z = 0) unless the prop says otherwise (hinges, hubs, string ends: see PIVOT_NOTES).
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

ROOT = "D:/Kill Godot"

# ------------------------------------------------------------------------------------------------ arguments
_argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
_pos, _opt, _i = [], {}, 0
while _i < len(_argv):
    a = _argv[_i]
    if a.startswith("--"):
        if a in ("--no-preview", "--no-verify", "--no-export"):
            _opt[a] = True
        else:
            _opt[a] = _argv[_i + 1]
            _i += 1
    else:
        _pos.append(a)
    _i += 1
OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_DressVillage.glb"
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"
TILE = int(_opt.get("--tile", 512))
ONLY = [s if s.startswith("SM_KG_") else "SM_KG_" + s for s in _opt["--only"].split(",")] if "--only" in _opt else None
MANIFEST = _opt.get("--manifest", f"{ROOT}/Art/Packed/KG_DressVillage_Clean.json")
PREFIX = _opt.get("--prefix", "DressVillage_preview")

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


def S(x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    return Matrix.Diagonal((x, y, z, 1.0))


def circle(n, r=1.0, phase=0.0):
    return [(r * math.cos(phase + 2 * math.pi * k / n), r * math.sin(phase + 2 * math.pi * k / n)) for k in range(n)]


def ellipse_pts(cx, cy, rx, ry, n, a0=0.0, a1=360.0):
    return [(cx + rx * math.cos(math.radians(lerp(a0, a1, k / n))), cy + ry * math.sin(math.radians(lerp(a0, a1, k / n))))
            for k in range(n)]


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


def rgb(c):
    return (c[0], c[1], c[2])


def jit(c, rng, amt=0.08, hue=0.0):
    k = 1.0 + rng.uniform(-amt, amt)
    out = (c[0] * k, c[1] * k, c[2] * k, c[3])
    if hue:
        out = mix(out, (c[1] * k, c[0] * k, c[2] * k, c[3]), rng.uniform(0, hue))
    return out


# palette (sRGB hex -> linear)
WOOD, WOOD_DK, WOOD_LT, WOOD_GREY = C("9C6535"), C("5F3B22"), C("CC955A"), C("8F8373")
IRON, IRON_LT = C("35343A"), C("5A5860")
STONE, STONE_LT, STONE_DK = C("B5AC9E"), C("D8CFBF"), C("877E72")
SAND, SAND_LT, PLASTER = C("E0CDA6"), C("EFE2C6"), C("F4ECDC")
RED, RED_DK, BLUE, BLUE_LT, NAVY = C("DA3F2C"), C("A82D22"), C("2F6DC0"), C("62AEE6"), C("243E78")
YELLOW, GREEN, GREEN_DK, LEAF, TEAL = C("F6C431"), C("5EAE3E"), C("3B7A2A"), C("6BBF45"), C("25A59C")
ORANGE, PURPLE, PINK, WHITE, CREAM, GOLD = C("F08A28"), C("8C4DB8"), C("F170A2"), C("F5F0E6"), C("F0E2C0"), C("E2B03C")
HAY, HAY_DK, BURLAP, TERRA, SOIL, ROPE = C("E9C75E"), C("C69C3C"), C("C9A56B"), C("CC6B3D"), C("5A3E2B"), C("C8B48A")
WATER, WATER_DK, WATER_LT, FOAM = C("3DB8DA"), C("2789B5"), C("8AD8EE"), C("E4F7FB")
MOSS, BLACK = C("7D9A45"), C("22212A")
FLOWERS = [RED, YELLOW, WHITE, PINK, C("9C6BE0"), ORANGE, C("FF5E7E")]


def stone_tone(p, base=STONE, seed=0.0, moss=0.0):
    c = mix(STONE_DK, STONE_LT, 0.5 + 0.55 * nz(p, 2.4, (seed, 1.3, 2.1)))
    c = mix(c, base, 0.45)
    c = mul(c, 1.0 + 0.07 * nz(p, 8.0, (2.0, seed, 5.0)))
    if moss > 0:
        m = smoothstep(0.0, 0.5, nz(p, 3.0, (7.0, 3.0, seed))) * moss
        c = mix(c, MOSS, m)
    return with_a(c, 0.0)


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
    Face colour overrides may be RGBA (whole corner colour) or RGB (keeps the vertex alpha: use this on Sway meshes
    so the wind mask stays continuous across faces).
    Shading: FLAT faces are faceted, SMOOTH faces are always smooth to each other, AUTO edges turn sharp above
    `angle` degrees. Closed islands get their normals recalculated outward on finalize (safety net)."""

    def __init__(self, name, angle=38.0, material="VC", pivot="bottom"):
        self.name, self.angle, self.material, self.pivot = name, angle, material, pivot
        self.V, self.VC, self.F, self.FC, self.FS = [], [], [], [], []
        self.CN = {}
        self.M = Matrix.Identity(4)
        self.flip = False
        self._st = []
        self.info = {}

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
        ins = self.M @ Vector(inside)
        pts = [self.V[i] for i in ids]
        cen = sum(pts, Vector()) / len(pts)
        ids = list(ids)
        if newell(pts).dot(cen - ins) < 0:
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
        return ids

    def cbox(self, c, s, b, col, sh=AUTO):
        """Chamfered box (bevel b). Returns the index of its first vertex."""
        start = len(self.V)
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
        return start

    def beam(self, p0, p1, w, h, col, up=(0, 0, 1), bevel=0.0, sh=AUTO):
        """Box from p0 to p1 (length axis), w across, h along `up`."""
        p0, p1 = Vector(p0), Vector(p1)
        x = p1 - p0
        L = x.length
        x.normalize()
        y = Vector(up).cross(x)
        if y.length < 1e-6:
            y = Vector((0, 1, 0)).cross(x)
            if y.length < 1e-6:
                y = Vector((1, 0, 0)).cross(x)
        y.normalize()
        z = x.cross(y)
        with self.xf(frame((p0 + p1) * 0.5, x, y, z)):
            if bevel > 0:
                return self.cbox((0, 0, 0), (L, w, h), bevel, col, sh)
            s = len(self.V)
            self.box((0, 0, 0), (L, w, h), col, sh)
            return s

    def lathe(self, prof, seg, col=None, colfn=None, segcol=None, sh=SMOOTH, phase=0.0, rfn=None, closed=False,
              facefn=None):
        """Surface of revolution around local Z. prof: [(r, z)] traced bottom-axis -> outside -> top-axis
        (outward normals, CCW in the (r, z) plane). r == 0 end points collapse to one apex vertex.
        facefn(i, j) -> colour override per quad (i = profile segment, j = angular segment)."""
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
            if isinstance(A, int) and isinstance(B, int):
                continue
            for j in range(seg):
                fc = facefn(i, j) if facefn else (segcol[i] if segcol else None)
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
        if isinstance(radii, (int, float)):
            radii = [radii] * len(pts)
        return self.sweep(pts, circle(sides, 1.0, phase), radii, radii, col, colfn, sh, cap0, cap1, False, up, tip,
                          rmod=rmod)

    def blob(self, c, r, sub=1, amp=0.15, freq=1.6, seed=0.0, col=None, colfn=None, sh=SMOOTH, rot=None):
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
            ids.append(self.v(p, colfn(self.M @ p, u) if colfn else col))
        for a, b, cc in F:
            self.f((ids[a], ids[b], ids[cc]), None, sh)
        return ids

    def cyl(self, p0, p1, r0, r1=None, seg=8, col=None, sh=AUTO, caps=True, phase=0.0, capcol=None, colfn=None):
        """Capped cylinder / cone frustum between two points."""
        p0, p1 = Vector(p0), Vector(p1)
        d = p1 - p0
        L = d.length
        z = d.normalized()
        x = z.orthogonal().normalized()
        y = z.cross(x)
        r1 = r0 if r1 is None else r1
        prof = ([(0, 0)] if caps else []) + [(r0, 0), (r1, L)] + ([(0, L)] if caps else [])
        segcol = ([capcol, None, capcol] if caps else [None]) if capcol else None
        with self.xf(frame(p0, x, y, z)):
            return self.lathe(prof, seg, col=col, sh=sh, phase=phase, segcol=segcol, colfn=colfn)

    def torus(self, R_, r, seg, sides, col=None, sh=SMOOTH, colfn=None, phase=0.0):
        prof = [(R_ + r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
        return self.lathe(prof, seg, col=col, colfn=colfn, sh=sh, closed=True, phase=phase)

    def ext2d(self, pts, w0, w1, col, side=None, sh=AUTO, topcol=None):
        """Extrude a 2D polygon (local XY, may be concave) between local z=w0 and z=w1 (+Z face on top)."""
        pts = [tuple(p) for p in pts]
        n = len(pts)
        area = sum(pts[k][0] * pts[(k + 1) % n][1] - pts[(k + 1) % n][0] * pts[k][1] for k in range(n))
        if area < 0:
            pts = pts[::-1]
        b = [self.v((x, y, w0), col) for x, y in pts]
        t = [self.v((x, y, w1), col) for x, y in pts]
        self.f(t, topcol, FLAT)
        self.f(list(reversed(b)), topcol, FLAT)
        for k in range(n):
            kn = (k + 1) % n
            self.f((b[k], b[kn], t[kn], t[k]), side, sh)

    def cloth(self, grid, colfn, thick=0.004, facefn=None, sh=SMOOTH):
        """Double-sided sheet with a small thickness (no coplanar back faces). grid[i][j] = local point.
        colfn(i, j, p) -> RGBA (alpha = sway mask); facefn(i, j) -> RGB override per quad (alpha kept)."""
        rows, cols = len(grid), len(grid[0])
        P = [[Vector(grid[i][j]) for j in range(cols)] for i in range(rows)]
        N = [[None] * cols for _ in range(rows)]
        for i in range(rows):
            for j in range(cols):
                du = P[i][min(j + 1, cols - 1)] - P[i][max(j - 1, 0)]
                dv = P[min(i + 1, rows - 1)][j] - P[max(i - 1, 0)][j]
                nn = du.cross(dv)
                N[i][j] = nn.normalized() if nn.length > 1e-12 else Vector((0, 0, 1))
        for sd in (1, -1):
            ids = [[self.v(P[i][j] + N[i][j] * (thick * 0.5 * sd), colfn(i, j, P[i][j])) for j in range(cols)]
                   for i in range(rows)]
            for i in range(rows - 1):
                for j in range(cols - 1):
                    q = (ids[i][j], ids[i][j + 1], ids[i + 1][j + 1], ids[i + 1][j])
                    fc = facefn(i, j) if facefn else None
                    self.f(q if sd == 1 else q[::-1], fc, sh)

    def card(self, pts, col, thick=0.003, alphas=None):
        """Double-sided convex polygon (fan) with a small thickness; alphas per point (sway)."""
        pts = [Vector(p) for p in pts]
        nn = newell(pts)
        nn = nn.normalized() if nn.length > 1e-12 else Vector((0, 0, 1))
        for sd in (1, -1):
            ids = [self.v(p + nn * (thick * 0.5 * sd), with_a(col, alphas[k] if alphas else col[3]))
                   for k, p in enumerate(pts)]
            self.f(ids if sd == 1 else ids[::-1], None, SMOOTH)


# ------------------------------------------------------------------------------------------------ finalize
MATS = {}
MAT_NAME = {"VC": "M_KG_DV_VC", "Sway": "M_KG_DV_Sway", "Glow": "M_KG_DV_Glow"}


def recenter(mb):
    if getattr(mb, "floor", False):          # flatten anything that dips below the ground (rocks at post feet)
        for i in range(len(mb.V)):
            if -0.2 < mb.V[i].z < 0.0:
                mb.V[i].z = 0.0
    if mb.pivot == "keep":
        return
    mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
    mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
    if mb.pivot == "bottom":
        off = Vector(((mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5, mn.z))
    elif mb.pivot == "bottom_z":           # keep XY as built, only drop min Z to 0
        off = Vector((0.0, 0.0, mn.z))
    else:
        off = Vector((0.0, 0.0, 0.0))
    for i in range(len(mb.V)):
        mb.V[i] = mb.V[i] - off
    mb.info["recentred_by"] = tuple(round(c, 3) for c in off)


def finalize(mb):
    recenter(mb)
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
            if len(fc) == 3:
                cols[ls[pi]:ls[pi] + lt[pi], :3] = fc
            else:
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

    area = np.empty(npoly, np.float32)
    me.polygons.foreach_get("area", area)
    degenerate = int((area < 1e-9).sum())
    changed = me.validate(verbose=False, clean_customdata=False)

    obj = bpy.data.objects.new(mb.name, me)
    bpy.context.scene.collection.objects.link(obj)
    mname = MAT_NAME[mb.material]
    mat = MATS.get(mname)
    if mat is None:
        mat = MATS[mname] = bpy.data.materials.new(mname)
    me.materials.append(mat)
    tris = int((lt - 2).sum())
    diag = dict(closed=closed_n, open=open_n, flipped=flipped, degenerate=degenerate, validate_changed=bool(changed))
    return obj, tris, diag


# ================================================================================================ shared bits
def flower_clump(mb, rng, c, r=0.08, blooms=None, n_fl=4, leaf=None, alpha=0.0, fl_r=0.028, sub_leaf=1):
    """Leafy blob with a few flower heads on top (planters, window boxes, maypole crown)."""
    leaf = leaf or jit(GREEN, rng, 0.12)
    c = Vector(c)
    mb.blob(c, (r, r * rng.uniform(0.85, 1.1), r * 0.8), sub=sub_leaf, amp=0.25, seed=rng.random() * 40,
            col=with_a(leaf, alpha))
    blooms = blooms or FLOWERS
    bc = rng.choice(blooms)
    for _ in range(n_fl):
        u = rand_unit(rng)
        u.z = abs(u.z) * 0.8 + 0.35
        u.normalize()
        p = c + Vector((u.x * r, u.y * r, u.z * r * 0.8)) * rng.uniform(0.85, 1.05)
        col = bc if rng.random() < 0.75 else rng.choice(blooms)
        mb.blob(p, fl_r * rng.uniform(0.8, 1.2), sub=0, amp=0.1, seed=rng.random() * 9, col=with_a(jit(col, rng, 0.06), alpha))


def flower_box(mb, rng, L=1.2, D=0.24, H=0.22, box_col=None, trim=None, blooms=None, ivy=True, brackets=True):
    """Window box centred on X, bottom at z=0, front = -Y, back face at y=+D/2."""
    box_col = box_col or TEAL
    trim = trim or WOOD_DK
    t = 0.03
    mb.cbox((0, -D / 2 + t / 2, H / 2), (L, t, H), 0.008, box_col)
    mb.cbox((0, D / 2 - t / 2, H / 2), (L, t, H), 0.008, box_col)
    for sx in (-1, 1):
        mb.cbox((sx * (L / 2 - t / 2), 0, H / 2), (t, D - 2 * t, H), 0.006, box_col)
    mb.box((0, 0, 0.015), (L - 2 * t, D - 2 * t, 0.03), mul(box_col, 0.6))
    mb.box((0, 0, H - 0.035), (L - 2 * t, D - 2 * t, 0.02), SOIL)
    for sy in (-1, 1):
        mb.cbox((0, sy * (D / 2 - 0.005), H + 0.01), (L + 0.03, 0.05, 0.03), 0.008, trim)
    mb.box((0, -D / 2 - 0.001, H * 0.42), (L - 0.12, 0.006, 0.018), mul(box_col, 0.7))
    if brackets:
        for sx in (-1, 1):
            x = sx * L * 0.3
            mb.box((x, D / 2 - 0.02, -0.07), (0.025, 0.02, 0.14), IRON)
            mb.beam((x, D / 2 - 0.02, -0.14), (x, -D / 2 + 0.05, -0.005), 0.022, 0.018, IRON, up=(1, 0, 0))
    n = max(3, int(L / 0.11))
    for k in range(n):
        x = -L / 2 + 0.07 + (L - 0.14) * k / (n - 1)
        flower_clump(mb, rng, (x, rng.uniform(-0.03, 0.03), H + 0.04), r=rng.uniform(0.065, 0.085), blooms=blooms,
                     n_fl=rng.randint(3, 5))
    if ivy:
        for k in range(max(2, int(L / 0.28))):
            x = -L / 2 + 0.1 + (L - 0.2) * (k + rng.uniform(0.2, 0.8)) / max(2, int(L / 0.28))
            drop = rng.uniform(0.14, 0.3)
            pts = [Vector((x + 0.02 * math.sin(i * 1.3), -D / 2 - 0.02 - 0.015 * math.sin(i * 0.7), H - drop * i / 6))
                   for i in range(7)]
            mb.tube(pts, 0.004, 4, col=GREEN_DK)
            for i in range(1, 7):
                p = pts[i] + Vector((rng.uniform(-0.02, 0.02), -0.01, 0))
                mb.blob(p, (0.024, 0.012, 0.02), sub=0, amp=0.1, col=jit(GREEN_DK, rng, 0.15))


def sack(mb, rng, col=None, mark=None, open_top=False, h=0.62, r=0.23):
    """Burlap sack standing at the origin (bottom z=0), front -Y. Returns nothing."""
    col = col or jit(BURLAP, rng, 0.08)
    s0 = len(mb.V)
    k = h / 0.62
    if open_top:
        prof = [(0, 0), (r * 0.72, 0), (r * 0.9, 0.05 * k), (r, 0.2 * k), (r * 0.97, 0.36 * k), (r * 0.9, 0.46 * k),
                (r * 0.86, 0.5 * k), (r * 0.95, 0.53 * k), (r * 0.82, 0.54 * k), (r * 0.76, 0.5 * k), (0, 0.48 * k)]
        segcol = [None] * 9 + [HAY]
    else:
        prof = [(0, 0), (r * 0.74, 0), (r * 0.9, 0.05 * k), (r, 0.18 * k), (r * 0.96, 0.34 * k), (r * 0.8, 0.44 * k),
                (r * 0.45, 0.5 * k), (r * 0.25, 0.535 * k), (r * 0.28, 0.56 * k), (r * 0.45, 0.6 * k),
                (r * 0.3, 0.64 * k), (0, 0.63 * k)]
        segcol = None
    ph = rng.random() * 6.28
    mb.lathe(prof, 11, col=col, segcol=segcol, sh=AUTO,
             rfn=lambda a, i, z: 1.0 + 0.07 * math.sin(3 * a + ph + i) + 0.04 * math.sin(5 * a - i * 0.7))
    mb.deform(s0, lambda p: Vector((p.x, p.y * 0.8, p.z)))
    mb.recolor(s0, lambda p, c: mul(c, 0.93 + 0.1 * nz(p, 9.0, (ph, 1, 2))))
    mb.cavity(s0, dark=0.35, light=0.2)
    if not open_top:
        with mb.xf(T(0, 0, 0.535 * k) @ S(1, 0.8, 1)):
            mb.torus(r * 0.29, 0.012, 10, 5, col=C("7A5A34"))
    else:
        mb.blob((0, 0, 0.5 * k), (r * 0.75, r * 0.6, 0.05 * k), sub=1, amp=0.1, col=HAY)
    if mark:
        with mb.xf(frame((0, -r * 0.8 - 0.002, 0.24 * k), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
            mb.ext2d(ellipse_pts(0, 0, 0.06, 0.06, 12), -0.03, 0.004, mark)
            mb.ext2d(ellipse_pts(0, 0, 0.035, 0.035, 10), -0.03, 0.007, col)


# ================================================================================================ 1. fountain
def make_fountain():
    mb = MB("SM_KG_Fountain", angle=33)
    rng = random.Random(3)
    ph8 = math.pi / 8

    def st(base=SAND, moss=0.0, seed=1.0):
        return lambda r, z, a, i: stone_tone((r * math.cos(a), r * math.sin(a), z), base, seed,
                                             moss * (1 - smoothstep(0.1, 0.7, z)))

    # plinth
    mb.lathe([(0, 0), (1.84, 0), (1.84, 0.1), (1.76, 0.13), (0, 0.13)], 8, colfn=st(STONE, 0.5, 2.0), sh=AUTO,
             phase=ph8)
    # basin wall (octagonal ring) with a pale rim
    prof = [(1.3, 0.12), (1.58, 0.12), (1.58, 0.56), (1.66, 0.6), (1.66, 0.72), (1.6, 0.76), (1.28, 0.76),
            (1.24, 0.72), (1.24, 0.6), (1.3, 0.56)]

    def wall_col(r, z, a, i):
        if z > 0.59:
            return stone_tone((r * math.cos(a), r * math.sin(a), z), SAND_LT, 3.0)
        return stone_tone((r * math.cos(a), r * math.sin(a), z), SAND, 1.5, 0.55 * (1 - smoothstep(0.15, 0.5, z)))
    mb.lathe(prof, 8, colfn=wall_col, sh=AUTO, phase=ph8, closed=True)
    # raised panels on the eight outer faces
    inr = 1.58 * math.cos(ph8)
    for k in range(8):
        with mb.xf(R(k * 45.0, 'Z') @ T(inr, 0, 0)):
            W, H, t, zc = 0.86, 0.32, 0.04, 0.34
            fcol = jit(SAND_LT, rng, 0.04)
            mb.box((0.012, 0, zc + H / 2 - t / 2), (0.028, W, t), fcol)
            mb.box((0.012, 0, zc - H / 2 + t / 2), (0.028, W, t), fcol)
            for sy in (-1, 1):
                mb.box((0.012, sy * (W / 2 - t / 2), zc), (0.028, t, H - 2 * t), fcol)
            mb.box((0.004, 0, zc), (0.016, W - 2 * t, H - 2 * t), mix(SAND, STONE_DK, 0.35))
            # a little carved shell in the middle
            with mb.xf(frame((0.012, 0, zc - 0.07), (0, 1, 0), (0, 0, 1), (1, 0, 0))):
                pts = [(0, 0)] + [(0.07 * math.cos(math.radians(a)), 0.07 * math.sin(math.radians(a)))
                                  for a in range(0, 181, 20)]
                mb.ext2d(pts, -0.01, 0.006, SAND_LT)
    # water in the basin: darker at the wall, foam ring where the streams land
    wprof = [(1.46, 0.55), (1.3, 0.55), (1.22, 0.55), (1.15, 0.55), (1.08, 0.55), (0.9, 0.55), (0.6, 0.55), (0, 0.55)]
    wc = [WATER_DK, WATER, WATER_LT, FOAM, WATER_LT, WATER, WATER, WATER]
    mb.lathe(wprof, 24, colfn=lambda r, z, a, i: wc[i], sh=SMOOTH)
    # coins and lily pads in the basin
    for k in range(9):
        a = rng.uniform(0, 6.28)
        r = rng.uniform(0.7, 1.35)
        with mb.xf(T(r * math.cos(a), r * math.sin(a), 0.549) @ R(rng.uniform(-8, 8), 'X')):
            mb.lathe([(0, 0), (0.028, 0), (0.028, 0.007), (0, 0.007)], 7, col=jit(GOLD, rng, 0.1), sh=AUTO)
    for k, (a, r, s) in enumerate(((0.6, 1.05, 0.15), (2.4, 0.95, 0.12), (4.3, 1.1, 0.14), (5.3, 0.85, 0.11))):
        with mb.xf(T(r * math.cos(a), r * math.sin(a), 0.551) @ R(rng.uniform(0, 360), 'Z')):
            pts = [(0, 0)] + [(s * math.cos(math.radians(d)), s * math.sin(math.radians(d))) for d in range(25, 340, 30)]
            mb.ext2d(pts, 0.0, 0.012, jit(C("4E9E3A"), rng, 0.08))
            if k % 2 == 0:
                for p in range(6):
                    ang = p * 60
                    mb.blob((0.03 * math.cos(math.radians(ang)), 0.03 * math.sin(math.radians(ang)), 0.03),
                            (0.03, 0.018, 0.012), sub=0, amp=0.05, col=PINK,
                            rot=Matrix.Rotation(math.radians(ang), 3, 'Z') @ Matrix.Rotation(math.radians(-30), 3, 'Y'))
                mb.blob((0, 0, 0.035), 0.016, sub=0, amp=0.05, col=YELLOW)
    # central column
    cprof = [(0, 0.12), (0.62, 0.12), (0.62, 0.24), (0.56, 0.3), (0.46, 0.4), (0.36, 0.52), (0.3, 0.64), (0.26, 0.78),
             (0.24, 1.0), (0.29, 1.05), (0.29, 1.11), (0.24, 1.16), (0.24, 1.3), (0, 1.3)]
    mb.lathe(cprof, 8, colfn=st(SAND, 0.6, 4.0), sh=AUTO, phase=ph8)
    # middle bowl (scalloped, 8 lobes) + water
    bprof = [(0, 1.24), (0.28, 1.24), (0.5, 1.29), (0.72, 1.36), (0.88, 1.45), (0.97, 1.55), (0.99, 1.62), (0.93, 1.65),
             (0.88, 1.6), (0, 1.6)]
    lobe = lambda a: 0.5 + 0.5 * math.cos(8 * a)  # noqa: E731
    mb.lathe(bprof, 24, colfn=lambda r, z, a, i: (WATER if i == 9 else stone_tone((r * math.cos(a), r * math.sin(a), z),
                                                                                   SAND_LT if i >= 5 else SAND, 5.0)),
             segcol=[None] * 8 + [WATER], sh=AUTO, rfn=lambda a, i, z: 1.0 + (0.06 * lobe(a) if i >= 3 else 0.0))
    # streams from the middle bowl lobes into the basin
    for k in range(8):
        a = k * math.pi / 4
        ca, sa = math.cos(a), math.sin(a)
        rz = bezier((1.03, 0, 1.61), (1.15, 0, 1.64), (1.2, 0, 1.2), (1.17, 0, 0.55), 10)
        pts = [Vector((p.x * ca, p.x * sa, p.z)) for p in rz]
        wid = [interp([(0, 0.05), (0.3, 0.055), (1.0, 0.075)], i / 10) for i in range(11)]
        thk = [interp([(0, 0.022), (0.3, 0.03), (1.0, 0.04)], i / 10) for i in range(11)]
        mb.sweep(pts, circle(6), wid, thk, colfn=lambda i, kk, p: mix(FOAM, WATER_LT, 0.35 + 0.4 * math.sin(i * 0.9) ** 2),
                 sh=SMOOTH)
        mb.blob((1.17 * ca, 1.17 * sa, 0.56), (0.11, 0.11, 0.04), sub=1, amp=0.25, seed=k, col=FOAM)
    # upper column + upper bowl (4 lobes) + streams
    mb.lathe([(0, 1.58), (0.2, 1.58), (0.17, 1.66), (0.13, 1.76), (0.12, 1.96), (0.15, 2.02), (0.15, 2.06), (0, 2.06)],
             8, colfn=st(SAND, 0.0, 6.0), sh=AUTO, phase=ph8)
    uprof = [(0, 2.02), (0.12, 2.02), (0.28, 2.08), (0.4, 2.16), (0.46, 2.24), (0.47, 2.29), (0.43, 2.31), (0.4, 2.27),
             (0, 2.27)]
    mb.lathe(uprof, 16, colfn=lambda r, z, a, i: stone_tone((r * math.cos(a), r * math.sin(a), z), SAND_LT, 7.0),
             segcol=[None] * 7 + [WATER], sh=AUTO,
             rfn=lambda a, i, z: 1.0 + (0.09 * (0.5 + 0.5 * math.cos(4 * a)) if i >= 3 else 0.0))
    for k in range(4):
        a = k * math.pi / 2
        ca, sa = math.cos(a), math.sin(a)
        rz = bezier((0.5, 0, 2.28), (0.59, 0, 2.3), (0.67, 0, 1.95), (0.68, 0, 1.6), 8)
        pts = [Vector((p.x * ca, p.x * sa, p.z)) for p in rz]
        mb.sweep(pts, circle(6), [0.035 + 0.01 * i / 8 for i in range(9)], [0.018 + 0.008 * i / 8 for i in range(9)],
                 colfn=lambda i, kk, p: mix(FOAM, WATER_LT, 0.4), sh=SMOOTH)
        mb.blob((0.68 * ca, 0.68 * sa, 1.61), (0.07, 0.07, 0.025), sub=1, amp=0.25, seed=k + 20, col=FOAM)
    # top pedestal + golden leaping fish spouting water
    mb.lathe([(0, 2.2), (0.11, 2.2), (0.09, 2.27), (0.07, 2.42), (0.1, 2.46), (0.1, 2.5), (0, 2.5)], 8,
             colfn=st(SAND_LT, 0.0, 8.0), sh=AUTO, phase=ph8)
    path = bezier((0.0, 0, 2.54), (0.13, 0, 2.7), (0.12, 0, 2.97), (-0.03, 0, 3.12), 12)
    rad = [interp([(0, 0.03), (0.15, 0.042), (0.45, 0.085), (0.7, 0.09), (0.88, 0.07), (1.0, 0.04)], i / 12)
           for i in range(13)]
    gold_hi, gold_lo = C("F4CC5A"), C("C98E2A")
    mb.sweep(path, circle(10), [r * 0.72 for r in rad], rad,
             colfn=lambda i, k, p: mix(gold_lo, gold_hi, 0.5 + 0.5 * math.sin(k * 0.63 + 1.0) + 0.15 * (i % 2)),
             sh=SMOOTH, tip=0.03)
    F = frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))
    fin = C("F09A2A")
    with mb.xf(F):
        mb.ext2d([(0.01, 2.6), (-0.15, 2.52), (-0.1, 2.5), (0.0, 2.53), (0.1, 2.5), (0.16, 2.53), (0.04, 2.6)],
                 -0.012, 0.012, fin)
        # dorsal fin on the convex (+X) side of the arc
        def outward(i):
            tng = (path[i + 1] - path[i - 1]).normalized()
            return Vector((tng.z, 0, -tng.x))
        b0 = path[4] + outward(4) * rad[4] * 0.7
        b2 = path[8] + outward(8) * rad[8] * 0.7
        tipp = path[5] + outward(5) * (rad[5] + 0.1)
        mb.ext2d([(b0.x, b0.z), (tipp.x, tipp.z), (b2.x, b2.z), (path[6].x, path[6].z)], -0.01, 0.01, fin)
    # eyes
    pe = path[10]
    for sy in (-1, 1):
        mb.blob((pe.x - 0.01, sy * 0.045, pe.z), 0.02, sub=1, amp=0.0, col=WHITE)
        mb.blob((pe.x - 0.012, sy * 0.058, pe.z), 0.011, sub=0, amp=0.0, col=BLACK)
    # water jet from the mouth
    m = path[-1] + Vector((-0.02, 0, 0.03))
    mb.tube([m, m + Vector((0, 0, 0.18))], [0.02, 0.018], 6, col=FOAM)
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        ca, sa = math.cos(a), math.sin(a)
        pts = bezier(m + Vector((0, 0, 0.17)), (m.x + 0.12 * ca, 0.12 * sa, 3.36), (0.3 * ca, 0.3 * sa, 3.2),
                     (0.33 * ca, 0.33 * sa, 2.28), 10)
        mb.tube(pts, [0.016 - 0.004 * i / 10 for i in range(11)], 5, colfn=lambda i, kk, p: mix(FOAM, WATER_LT, 0.3))
    return mb


# ================================================================================================ 2. maypole
def spiral_pole(mb, z0, z1, rfun, seg, pitch, rows_per_turn, row_col):
    """Pole whose faces follow a helix, so row colours make crisp candy-cane stripes."""
    h = pitch / rows_per_turn
    n_rows = int(math.ceil((z1 - z0) / h)) + 1
    cache = {}

    def vid(i, j):
        z = z0 + i * h + (j / seg) * pitch
        zc = min(max(z, z0), z1)
        jj = j % seg
        key = (jj, round(zc, 5))
        if key not in cache:
            a = 2 * math.pi * jj / seg
            r = rfun(zc)
            cache[key] = mb.v((r * math.cos(a), r * math.sin(a), zc), WHITE)
        return cache[key]

    for i in range(-rows_per_turn - 1, n_rows):
        for j in range(seg):
            q = [vid(i, j), vid(i, j + 1), vid(i + 1, j + 1), vid(i + 1, j)]
            uq = []
            for x in q:
                if x not in uq:
                    uq.append(x)
            if len(uq) < 3:
                continue
            zs = [mb.V[x].z for x in uq]
            if max(zs) - min(zs) < 1e-6:
                continue
            mb.f(uq, row_col(i), SMOOTH)
    for zc, top in ((z0, False), (z1, True)):
        ring = [cache[(j, round(zc, 5))] for j in range(seg)]
        c = mb.v((0, 0, zc), WHITE)
        for j in range(seg):
            jn = (j + 1) % seg
            mb.f((ring[j], ring[jn], c) if top else (c, ring[jn], ring[j]), row_col(0), FLAT)


MAYPOLE_HOOP_Z, MAYPOLE_HOOP_R = 5.05, 0.58


def make_maypole():
    mb = MB("SM_KG_Maypole", angle=35, material="Sway", pivot="keep")
    rng = random.Random(21)
    # painted tub at the foot, planted with flowers
    tub = [(0, 0), (0.6, 0), (0.63, 0.05), (0.63, 0.36), (0.59, 0.4), (0.53, 0.4), (0.53, 0.34), (0, 0.34)]
    mb.lathe(tub, 14, col=WOOD, sh=AUTO,
             facefn=lambda i, j: (rgb(SOIL) if i == 6 else rgb(jit(C("2E8C8A") if j % 2 else C("35A09C"), rng, 0.05))
                                  if i in (1, 2) else rgb(WOOD_DK)))
    for z in (0.09, 0.3):
        with mb.xf(T(0, 0, z)):
            mb.torus(0.638, 0.013, 14, 4, col=IRON_LT, sh=AUTO)
    for k in range(9):
        a = k * 2 * math.pi / 9 + 0.2
        flower_clump(mb, rng, (0.34 * math.cos(a), 0.34 * math.sin(a), 0.42), r=rng.uniform(0.1, 0.13),
                     n_fl=5, fl_r=0.035)
    # candy-striped pole
    spiral_pole(mb, 0.3, 5.9, lambda z: 0.1 - 0.025 * (z - 0.3) / 5.6, 10, 1.0, 12,
                lambda i: rgb(RED) if (i // 3) % 2 == 0 else rgb(WHITE))
    # flower crown: hoop + two arches covered in leaves and flowers
    with mb.xf(T(0, 0, MAYPOLE_HOOP_Z)):
        mb.torus(MAYPOLE_HOOP_R, 0.03, 24, 6, col=WOOD_DK)
    for rot in (0.0, 90.0):
        with mb.xf(R(rot, 'Z')):
            arc = [(MAYPOLE_HOOP_R * math.cos(t), 0, MAYPOLE_HOOP_Z + 0.5 * math.sin(t))
                   for t in [math.pi * k / 12 for k in range(13)]]
            mb.tube(arc, 0.022, 6, col=WOOD_DK)
    spots = [(MAYPOLE_HOOP_R * math.cos(2 * math.pi * k / 18), MAYPOLE_HOOP_R * math.sin(2 * math.pi * k / 18),
              MAYPOLE_HOOP_Z) for k in range(18)]
    for rot in (0.0, 90.0):
        for t in (0.25, 0.4, 0.5, 0.6, 0.75):
            ang = math.pi * t
            x, z = MAYPOLE_HOOP_R * math.cos(ang), MAYPOLE_HOOP_Z + 0.5 * math.sin(ang)
            c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
            spots.append((x * c, x * s, z))
    for p in spots:
        mb.blob(p, (0.11, 0.1, 0.08), sub=1, amp=0.3, seed=rng.random() * 30,
                col=with_a(jit(C("4F9F38"), rng, 0.15), 0.25))
        for _ in range(3):
            u = rand_unit(rng)
            q = Vector(p) + u * rng.uniform(0.07, 0.1)
            mb.blob(q, 0.034, sub=0, amp=0.1, seed=rng.random(), col=with_a(jit(rng.choice(FLOWERS), rng, 0.05), 0.25))
    # hanging bells? no - golden finial ball + pennant
    mb.lathe([(0, 5.88), (0.06, 5.88), (0.1, 5.95), (0.11, 6.02), (0.09, 6.09), (0.05, 6.13), (0, 6.14)], 10,
             col=GOLD, sh=SMOOTH)
    mb.cyl((0, 0, 6.1), (0, 0, 6.55), 0.012, seg=5, col=WOOD_DK)
    grid = []
    for i in range(6):
        v = i / 5
        row = []
        for j in range(5):
            u = j / 4
            L = 0.5
            x = u * L
            tail = 0.06 * u if v > 0.5 else 0.0
            z = 6.52 - v * 0.2 * (1 - 0.35 * u) + (0.0 if v < 0.5 else 0.0) - 0.02 * u
            zz = 6.52 - 0.2 * (1 - 0.3 * u) * v
            if abs(v - 0.5) < 0.01:
                x = x - 0.08 * u
            row.append((x - tail * 0.0, 0.03 * math.sin(u * 3.0), zz))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(RED if i < 3 else YELLOW, (j / 4) ** 1.2), thick=0.004)
    return mb


RIBBON_COLS = [RED, YELLOW, BLUE, GREEN, PINK, ORANGE, PURPLE, WHITE]


def make_maypole_ribbons():
    mb = MB("SM_KG_Maypole_Ribbons", angle=40, material="Sway", pivot="keep")
    rng = random.Random(8)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        ca, sa = math.cos(a), math.sin(a)
        rad = Vector((ca, sa, 0))
        tan = Vector((-sa, ca, 0))
        reach = rng.uniform(2.1, 2.5)
        endz = rng.uniform(0.9, 1.3)
        rz = bezier((MAYPOLE_HOOP_R + 0.02, 0, MAYPOLE_HOOP_Z - 0.02), (1.25, 0, 4.7), (reach, 0, 3.0),
                    (reach + 0.1, 0, endz), 22)
        twist0 = rng.uniform(-0.3, 0.3)
        grid = []
        for i, p in enumerate(rz):
            t = i / 22
            base = rad * p.x + Vector((0, 0, p.z))
            tw = twist0 + 1.1 * t * t * math.sin(k * 1.7 + 2.0 * t)
            wdir = tan * math.cos(tw) + rad * math.sin(tw) * 0.6
            wdir.normalize()
            w = 0.13
            grid.append([base - wdir * w / 2, base + wdir * w / 2])
        col = RIBBON_COLS[k]
        mb.cloth(grid, lambda i, j, p, col=col: with_a(col, smoothstep(0.0, 1.0, i / 22) ** 1.3), thick=0.004)
    return mb


# ================================================================================================ 3. windmill
MILL_HUB = (2.75, 0.0, 8.6)


def make_windmill_body():
    mb = MB("SM_KG_Windmill_Body", angle=30, pivot="keep")
    rng = random.Random(5)
    Z0, Z1, RB, RT = 0.5, 7.8, 2.3, 1.75

    def rt(z):
        return RB + (RT - RB) * (z - Z0) / (Z1 - Z0)
    beta = math.atan((RB - RT) / (Z1 - Z0))

    def mount(theta_deg, z, out=0.0):
        th = math.radians(theta_deg)
        nr = Vector((math.cos(th), math.sin(th), 0))
        t = Vector((-math.sin(th), math.cos(th), 0))
        u = Vector((0, 0, 1))
        up = u * math.cos(beta) - nr * math.sin(beta)
        n = nr * math.cos(beta) + u * math.sin(beta)
        o = Vector((0, 0, z)) + nr * (rt(z) + out)
        return frame(o, t, up, n)

    # stone base: two courses of blocks
    blk = {}

    def block_col(i, j):
        if (i, j) not in blk:
            blk[(i, j)] = rgb(jit(rng.choice([STONE, STONE_DK, C("A39A8C"), C("9C9486")]), rng, 0.06))
        return blk[(i, j)]
    mb.lathe([(0, 0), (2.5, 0), (2.5, 0.25), (2.47, 0.27), (2.47, 0.5), (2.36, 0.56), (0, 0.56)], 16, col=STONE,
             sh=AUTO, facefn=lambda i, j: block_col(i, j) if i in (1, 3) else rgb(STONE_DK), phase=0.1)
    # whitewashed tower with a blue band under the cap
    zs = [0.5, 0.9, 1.5, 2.1, 2.7, 3.3, 3.9, 4.5, 5.1, 5.7, 6.3, 6.9, 7.2, 7.4, 7.8]
    prof = [(0, 0.5)] + [(rt(z), z) for z in zs] + [(0, 7.8)]
    segcol = [None] * len(prof)
    segcol[zs.index(7.2) + 1] = BLUE

    def plaster(r, z, a, i):
        c = mix(PLASTER, C("DCCFB6"), 0.35 + 0.35 * nz((r * math.cos(a), r * math.sin(a), z), 1.3, (3, 3, 3)))
        c = mix(c, C("B9AE9A"), 0.5 * (1 - smoothstep(0.5, 1.6, z)))
        return with_a(c, 0.0)
    mb.lathe(prof, 20, colfn=plaster, segcol=segcol, sh=AUTO)

    # door (south, -Y) with stone jambs and voussoirs, planked leaf, iron straps
    with mb.xf(mount(-90, 0.56)):
        W, HR = 1.0, 1.35
        arch = [(-W / 2, 0), (W / 2, 0), (W / 2, HR)] + [(W / 2 * math.cos(math.radians(t)), HR + W / 2 * math.sin(
            math.radians(t))) for t in range(15, 180, 15)] + [(-W / 2, HR)]
        mb.ext2d(arch, -0.2, 0.02, WOOD)
        for x in (-0.25, 0.0, 0.25):
            top = HR + math.sqrt(max(0.0, 0.25 - x * x)) - 0.03
            mb.box((x, top / 2 + 0.02, 0.022), (0.014, top - 0.02, 0.006), WOOD_DK)
        for v in (0.35, 1.25):
            mb.box((-0.12, v, 0.03), (0.72, 0.05, 0.02), IRON)
        with mb.xf(T(0.3, 0.95, 0.04)):
            mb.torus(0.04, 0.008, 10, 4, col=IRON_LT)
        for s in (-1, 1):
            vv = 0.0
            for k, hgt in enumerate((0.42, 0.46, 0.47)):
                wdt = 0.24 if (k % 2 == 0) else 0.17
                mb.cbox((s * (W / 2 + wdt / 2), vv + hgt / 2, -0.03), (wdt, hgt - 0.02, 0.2), 0.02,
                        jit(STONE_LT, rng, 0.06))
                vv += hgt
        for k in range(7):
            ph = math.radians((k + 0.5) * 180 / 7)
            with mb.xf(T(0.61 * math.cos(ph), HR + 0.61 * math.sin(ph), -0.03) @ R(math.degrees(ph) - 90, 'Z')):
                mb.cbox((0, 0, 0), (0.2 if k != 3 else 0.24, 0.22 if k != 3 else 0.28, 0.2), 0.02,
                        STONE_LT if k == 3 else jit(C("CFC6B6"), rng, 0.05))
    # door steps
    mb.cbox((0, -2.7, 0.1), (1.3, 0.5, 0.2), 0.02, jit(STONE, rng, 0.05))
    mb.cbox((0, -2.55, 0.29), (1.2, 0.3, 0.18), 0.02, jit(STONE_LT, rng, 0.05))

    # windows (tilted to the taper) with shutters and a few flower boxes
    def window(theta, z, flowers):
        with mb.xf(mount(theta, z)):
            mb.ext2d([(-0.25, -0.35), (0.25, -0.35), (0.25, 0.35), (-0.25, 0.35)], -0.2, 0.005, C("2A4466"))
            mb.box((0, 0, 0.015), (0.04, 0.68, 0.02), CREAM)
            mb.box((0, 0.05, 0.015), (0.48, 0.04, 0.02), CREAM)
            for s in (-1, 1):
                mb.box((s * 0.27, 0, 0.0), (0.05, 0.8, 0.09), CREAM)
                mb.box((0, s * 0.38, 0.0), (0.6, 0.05, 0.09), CREAM)
                mb.cbox((s * 0.44, 0, 0.02), (0.26, 0.74, 0.03), 0.006, C("2F9C8E"))
                mb.box((s * 0.44, 0.18, 0.04), (0.2, 0.04, 0.012), C("217266"))
                mb.box((s * 0.44, -0.18, 0.04), (0.2, 0.04, 0.012), C("217266"))
            mb.cbox((0, -0.42, 0.04), (0.66, 0.05, 0.14), 0.01, STONE_LT)
        if flowers:
            M = mount(theta, z - 0.62, 0.12)
            with mb.xf(M @ frame((0, 0, 0), (1, 0, 0), (0, 0, -1), (0, 1, 0))):
                flower_box(mb, rng, L=0.62, D=0.2, H=0.16, box_col=C("2F9C8E"), ivy=True, brackets=False)
    window(-90, 4.3, True)
    window(35, 5.2, False)
    window(145, 3.9, True)
    window(-140, 6.1, False)
    window(-40, 6.6, False)
    # lantern beside the door
    with mb.xf(mount(-68, 1.95)):
        mb.box((0, 0, 0.02), (0.1, 0.16, 0.04), IRON)
        mb.beam((0, 0.02, 0.0), (0, 0.02, 0.32), 0.03, 0.03, IRON, up=(0, 1, 0))
        mb.cbox((0, -0.12, 0.3), (0.16, 0.24, 0.16), 0.01, IRON)
        mb.box((0, -0.12, 0.3), (0.13, 0.2, 0.17), C("FFD27A"))
    # reefing stage (gallery) with railing and struts
    zs_, zt_ = 2.62, 2.72
    rin = rt(2.7) - 0.1
    mb.lathe([(rin, zs_), (3.12, zs_), (3.12, zt_), (rin, zt_)], 28, col=WOOD, sh=AUTO, closed=True,
             facefn=lambda i, j: rgb(jit(WOOD_LT if j % 2 else WOOD, rng, 0.06)) if i == 2 else rgb(WOOD_DK))
    for k in range(28):
        a = 2 * math.pi * (k + 0.5) / 28
        mb.box((3.04 * math.cos(a), 3.04 * math.sin(a), zt_ + 0.45), (0.06, 0.06, 0.9), WOOD_DK)
    for z, r in ((3.64, 0.035), (3.2, 0.022)):
        with mb.xf(T(0, 0, z)):
            mb.torus(3.04, r, 28, 6, col=WOOD)
    for k in range(8):
        a = math.radians(22.5 + 45 * k)
        ca, sa = math.cos(a), math.sin(a)
        mb.beam(((rt(1.5) - 0.05) * ca, (rt(1.5) - 0.05) * sa, 1.5), (2.9 * ca, 2.9 * sa, 2.6), 0.08, 0.08, WOOD_DK,
                up=(0, 0, 1))
    # curb + ogee cap with red shingle rows + golden finial
    mb.lathe([(0, 7.72), (1.98, 7.72), (1.98, 7.92), (0, 7.92)], 20, col=WOOD_DK, sh=AUTO)
    cap = [(0, 7.9), (2.08, 7.9), (2.1, 8.0), (2.02, 8.25), (1.88, 8.6), (1.66, 8.95), (1.38, 9.28), (1.02, 9.58),
           (0.62, 9.82), (0.28, 9.98), (0, 10.04)]
    rc = [WOOD_DK, RED_DK, RED, C("C23A2A"), RED, C("C23A2A"), RED, C("C23A2A"), RED, C("C23A2A")]
    mb.lathe(cap, 20, col=RED, segcol=rc, sh=AUTO)
    mb.lathe([(0, 10.0), (0.05, 10.0), (0.05, 10.12), (0.12, 10.18), (0.13, 10.27), (0.1, 10.35), (0.04, 10.4),
              (0, 10.42)], 10, col=GOLD, sh=SMOOTH)
    # breast / hood over the windshaft (+X) with a small gable roof
    mb.cbox((1.78, 0, 8.45), (1.05, 1.2, 1.0), 0.02, WOOD)
    for z in (8.15, 8.45, 8.75):
        mb.box((2.305, 0, z), (0.01, 1.16, 0.02), WOOD_DK)
    th = math.atan2(0.4, 0.7)
    for s in (-1, 1):
        nrm = Vector((0, s * math.sin(th), math.cos(th)))
        mb.beam((1.2, s * 0.36, 9.16), (2.42, s * 0.36, 9.16), 0.86, 0.06, C("B33526"), up=nrm, bevel=0.01)
    with mb.xf(frame((2.3, 0, 8.95), (0, 1, 0), (0, 0, 1), (1, 0, 0))):
        mb.ext2d([(-0.62, 0), (0.62, 0), (0, 0.36)], -0.06, 0.02, WOOD_LT)
    # windshaft to the sail hub
    mb.cyl((1.9, 0, MILL_HUB[2]), (2.5, 0, MILL_HUB[2]), 0.16, seg=10, col=WOOD_DK)
    with mb.xf(T(2.42, 0, MILL_HUB[2]) @ R(90, 'Y')):
        mb.torus(0.17, 0.025, 10, 4, col=IRON)
    # tail pole with braces and a cart wheel
    mb.beam((-1.6, 0, 8.3), (-5.15, 0, 0.62), 0.16, 0.16, WOOD_DK, up=(0, 1, 0), bevel=0.02)
    for s in (-1, 1):
        mb.beam((-1.85, s * 0.6, 8.0), (-3.45, 0, 5.0), 0.1, 0.1, WOOD, up=(0, 0, 1))
    with mb.xf(T(-5.2, 0, 0.5) @ R(90, 'Y')):
        mb.lathe([(0.42, -0.04), (0.5, -0.04), (0.5, 0.04), (0.42, 0.04)], 16, col=WOOD_DK, sh=AUTO, closed=True)
        mb.lathe([(0, -0.1), (0.08, -0.1), (0.08, 0.1), (0, 0.1)], 8, col=IRON, sh=AUTO)
        for k in range(8):
            a = k * math.pi / 4
            mb.beam((0.07 * math.cos(a), 0.07 * math.sin(a), 0), (0.43 * math.cos(a), 0.43 * math.sin(a), 0), 0.04,
                    0.04, WOOD)
    mb.info["hub"] = MILL_HUB
    return mb


def make_windmill_sails():
    mb = MB("SM_KG_Windmill_Sails", angle=35, pivot="keep")
    rng = random.Random(9)
    L, R0 = 4.7, 1.0
    X = Vector((1, 0, 0))
    for k in range(4):
        phi = math.radians(45 + 90 * k)
        d = Vector((0, math.cos(phi), math.sin(phi)))
        e = Vector((0, -math.sin(phi), math.cos(phi)))

        def P(r, s, x=0.0, d=d, e=e):
            return d * r + e * s + X * x
        mb.beam(P(0.0, 0, 0.12), P(L, 0, 0.12), 0.16, 0.14, WOOD_DK, up=X, bevel=0.012)
        for s in (-0.3, 1.16):
            mb.beam(P(R0 - 0.05, s, 0.1), P(L - 0.05, s, 0.1), 0.05, 0.05, WOOD, up=X)
        n = 11
        for i in range(n):
            r = R0 + (L - 0.1 - R0) * i / (n - 1)
            mb.beam(P(r, -0.33, 0.1), P(r, 1.19, 0.1), 0.045, 0.045, WOOD_LT, up=X)
        mb.beam(P(R0, -0.15, 0.06), P(L - 0.1, -0.15, 0.06), 0.28, 0.02, WOOD, up=X)
        grid = []
        rows, cols = 10, 5
        for i in range(rows + 1):
            u = i / rows
            row = []
            for j in range(cols + 1):
                v = j / cols
                r = R0 + 0.04 + (L - 0.18 - R0) * u
                s = 0.03 + 1.1 * v
                x = 0.04 - 0.09 * math.sin(math.pi * u) * math.sin(math.pi * v)
                row.append(P(r, s, x))
            grid.append(row)
        mb.cloth(grid, lambda i, j, p: with_a(mix(CREAM, WHITE, 0.3 + 0.3 * math.sin(i * 1.3 + j)), 0.0), thick=0.008,
                 facefn=lambda i, j: rgb(C("E8D6B0")) if j == 0 else None)
    # hub boss, iron cap, axle stub
    mb.cyl((-0.2, 0, 0), (0.24, 0, 0), 0.3, seg=8, col=WOOD_DK, phase=math.pi / 8)
    mb.cyl((0.24, 0, 0), (0.31, 0, 0), 0.22, 0.17, seg=10, col=IRON)
    for k in range(6):
        a = k * math.pi / 3
        mb.blob((0.25, 0.24 * math.cos(a), 0.24 * math.sin(a)), 0.025, sub=0, amp=0.0, col=IRON_LT)
    mb.cyl((-0.48, 0, 0), (-0.2, 0, 0), 0.16, seg=10, col=WOOD_DK)
    return mb


# ================================================================================================ 4. stage
def make_stage():
    mb = MB("SM_KG_Stage", angle=35, pivot="keep")
    mb.floor = True
    rng = random.Random(11)
    DZ = 0.8
    # deck boards (along Y)
    for k in range(12):
        x = -1.5 + 0.25 * (k + 0.5)
        mb.cbox((x, 0, DZ - 0.025), (0.244, 2.0, 0.05), 0.008, jit(WOOD_LT if k % 2 else C("C08A50"), rng, 0.05))
    # frame posts
    for x in (-1.42, 0.0, 1.42):
        for y in (-0.92, 0.92):
            mb.box((x, y, (DZ - 0.05) / 2), (0.12, 0.12, DZ - 0.05), WOOD_DK)
    # skirts: painted red front with yellow trim, plain wood sides/back
    mb.box((0, -0.98, 0.42), (3.0, 0.04, 0.66), RED)
    mb.box((0, -1.0, 0.72), (3.02, 0.03, 0.06), YELLOW)
    mb.box((0, -1.0, 0.12), (3.02, 0.03, 0.05), YELLOW)
    for s in (-1, 1):
        mb.box((s * 1.48, 0, 0.42), (0.04, 1.96, 0.66), jit(WOOD, rng, 0.04))
    mb.box((0, 0.98, 0.42), (3.0, 0.04, 0.66), WOOD)
    # swags on the front skirt (blue / white / red bands)
    for k in range(3):
        cx = -1.0 + k * 1.0
        grid = []
        for i in range(5):
            v = i / 4
            row = []
            for j in range(9):
                u = j / 8
                x = cx - 0.45 + 0.9 * u
                sag = 0.26 * math.sin(math.pi * u) * (0.35 + 0.65 * v)
                row.append((x, -1.02 - 0.03 * math.sin(math.pi * u) * v, 0.74 - 0.02 - sag - 0.02 * v))
            grid.append(row)
        mb.cloth(grid, lambda i, j, p: BLUE, thick=0.006,
                 facefn=lambda i, j: rgb(BLUE) if i < 1 else rgb(WHITE) if i < 3 else rgb(RED))
        mb.blob((cx - 0.45, -1.03, 0.72), (0.05, 0.03, 0.05), sub=1, amp=0.1, col=YELLOW)
    mb.blob((1.5 - 0.05, -1.03, 0.72), (0.05, 0.03, 0.05), sub=1, amp=0.1, col=YELLOW)
    # steps (front centre)
    for k, (y0, y1, z) in enumerate(((-1.9, -1.6, 0.2), (-1.6, -1.3, 0.4), (-1.3, -1.0, 0.6))):
        mb.box((0, (y0 + y1) / 2, z / 2 - 0.015), (1.1, y1 - y0, z - 0.03), WOOD_DK)
        mb.cbox((0, (y0 + y1) / 2 - 0.02, z - 0.015), (1.14, y1 - y0 + 0.04, 0.03), 0.006, jit(WOOD_LT, rng, 0.05))
    for s in (-1, 1):
        mb.beam((s * 0.58, -1.98, 0.02), (s * 0.58, -1.0, 0.78), 0.05, 0.2, WOOD, up=(0, 0, 1))
    # railings: sides + back full, front only at the corners; stair handrails
    RT_ = DZ + 0.95

    def post(x, y, top=RT_):
        mb.cbox((x, y, (DZ + top) / 2), (0.08, 0.08, top - DZ), 0.01, WOOD_DK)
        mb.lathe([(0, top), (0.05, top), (0.05, top + 0.03), (0.03, top + 0.06), (0, top + 0.07)], 6, col=WOOD_DK,
                 sh=AUTO)

    def rail(p0, p1):
        mb.beam(p0, p1, 0.07, 0.05, WOOD, up=(0, 0, 1), bevel=0.01)
        mb.beam((p0[0], p0[1], p0[2] - 0.45), (p1[0], p1[1], p1[2] - 0.45), 0.04, 0.04, WOOD, up=(0, 0, 1))
    for s in (-1, 1):
        for y in (-0.95, -0.3, 0.3):
            post(s * 1.45, y)
        rail((s * 1.45, -0.95, RT_), (s * 1.45, 0.95, RT_))
        post(s * 0.62, -0.95)
        rail((s * 1.45, -0.95, RT_), (s * 0.62, -0.95, RT_))
        post(s * 0.62, -1.85, 1.15)
        mb.beam((s * 0.62, -1.85, 1.15), (s * 0.62, -0.95, RT_), 0.06, 0.05, WOOD, up=(0, 0, 1), bevel=0.01)
        mb.cbox((s * 0.62, -1.85, 0.2 + 0.47), (0.08, 0.08, 0.94), 0.01, WOOD_DK)
    for x in (-0.72, 0.0, 0.72):
        post(x, 0.95)
    rail((-1.45, 0.95, RT_), (1.45, 0.95, RT_))
    # banner frame at the back: tall posts, crossbar, blue banner with a golden fish, pennants on top
    for s in (-1, 1):
        mb.cbox((s * 1.45, 0.95, (DZ + 3.0) / 2), (0.12, 0.12, 3.0 - DZ), 0.012, WOOD_DK)
        mb.cyl((s * 1.45, 0.95, 3.0), (s * 1.45, 0.95, 3.45), 0.012, seg=5, col=WOOD_DK)
        mb.card([(s * 1.45, 0.95, 3.44), (s * 1.45 + s * 0.34, 0.95, 3.36), (s * 1.45, 0.95, 3.26)], RED, thick=0.006)
        mb.blob((s * 1.45, 0.95, 3.03), 0.07, sub=1, amp=0.0, col=GOLD)
    mb.beam((-1.58, 0.95, 2.92), (1.58, 0.95, 2.92), 0.1, 0.1, WOOD, up=(0, 0, 1), bevel=0.012)
    grid = []
    for i in range(9):
        v = i / 8
        row = []
        for j in range(7):
            u = j / 6
            x = -0.75 + 1.5 * u
            z = 2.86 - 1.05 * v
            if i == 8:
                z += 0.18 * (1 - abs(2 * u - 1))
            row.append((x, 0.9 - 0.02 * math.sin(math.pi * u) * v, z))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: NAVY, thick=0.008,
             facefn=lambda i, j: rgb(GOLD) if (j in (0, 5) or i == 0) else rgb(NAVY))
    with mb.xf(frame((0, 0.884, 2.38), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        cx, cy = 0.02, 0.0
        pts = ellipse_pts(cx, cy, 0.3, 0.13, 16, -150, 150) + [(cx - 0.44, 0.14), (cx - 0.36, 0.0), (cx - 0.44, -0.14)]
        mb.ext2d(pts, 0.0, 0.012, GOLD)
        mb.ext2d(ellipse_pts(cx + 0.18, 0.03, 0.03, 0.03, 8), 0.0, 0.016, NAVY)
        mb.ext2d([(-0.08, 0.11), (0.06, 0.12), (-0.1, 0.2)], 0.0, 0.012, GOLD)
    for k in range(5):
        mb.box((-0.6 + 0.3 * k, 0.9, 2.915), (0.03, 0.03, 0.12), GOLD)
    # lectern with a red drape
    mb.cbox((0, 0.45, DZ + 0.47), (0.42, 0.34, 0.94), 0.015, WOOD_DK)
    with mb.xf(T(0, 0.45, DZ + 1.0) @ R(-18, 'X')):
        mb.cbox((0, 0, 0), (0.62, 0.46, 0.04), 0.01, WOOD)
        mb.box((0, -0.2, 0.03), (0.6, 0.04, 0.03), WOOD_DK)
    mb.box((0, 0.27, DZ + 0.62), (0.36, 0.02, 0.56), RED)
    mb.box((0, 0.262, DZ + 0.62), (0.3, 0.02, 0.5), RED_DK)
    with mb.xf(frame((0, 0.255, DZ + 0.66), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        mb.ext2d(ellipse_pts(0, 0, 0.08, 0.08, 12), 0.0, 0.01, GOLD)
        mb.ext2d(ellipse_pts(0, 0, 0.05, 0.05, 10), 0.0, 0.013, RED)
    for x in (-0.12, 0.0, 0.12):
        mb.box((x, 0.262, DZ + 0.33), (0.03, 0.02, 0.05), GOLD)
    # a scroll and a bell on the lectern top (story: the town crier's spot)
    with mb.xf(T(0, 0.45, DZ + 1.0) @ R(-18, 'X')):
        mb.cyl((-0.2, -0.05, 0.05), (0.02, -0.05, 0.05), 0.025, seg=8, col=CREAM)
    with mb.xf(T(0.18, 0.36, DZ + 1.05)):
        mb.lathe([(0, 0), (0.07, 0), (0.068, 0.02), (0.048, 0.07), (0.035, 0.11), (0, 0.12)], 10, col=GOLD, sh=SMOOTH)
        mb.cyl((0, 0, 0.11), (0, 0, 0.22), 0.012, seg=6, col=WOOD_DK)
    return mb


# ================================================================================================ 5. bunting
BUNT_COLS = [RED, YELLOW, BLUE, GREEN, ORANGE, WHITE, PINK, TEAL]


def make_bunting():
    mb = MB("SM_KG_Bunting", angle=40, material="Sway", pivot="keep")
    rng = random.Random(2)
    L, sag = 6.0, 0.45

    def line(t):
        return Vector((L * t, 0, -sag * 4 * t * (1 - t)))
    mb.tube([line(i / 32) for i in range(33)], 0.009, 5, col=ROPE)
    for e in (0.0, 1.0):
        with mb.xf(T(L * e, 0, 0) @ R(90, 'X')):
            mb.torus(0.03, 0.007, 8, 4, col=ROPE)
    n = 19
    for k in range(n):
        tc = (k + 0.5) / n
        w = 0.25 / L
        a, b = line(tc - w / 2), line(tc + w / 2)
        mid = (a + b) * 0.5
        drop = rng.uniform(0.28, 0.33)
        yo = rng.uniform(-0.05, 0.05)
        tip = mid + Vector((rng.uniform(-0.03, 0.03), yo, -drop))
        m1 = a.lerp(tip, 0.5) + Vector((0, yo * 0.3 + 0.01, 0))
        m2 = b.lerp(tip, 0.5) + Vector((0, yo * 0.3 + 0.01, 0))
        col = BUNT_COLS[k % len(BUNT_COLS)]
        nn = (b - a).cross(tip - a).normalized()
        for sd in (1, -1):
            off = nn * 0.002 * sd
            ids = [mb.v(p + off, with_a(col, al)) for p, al in ((a, 0.0), (b, 0.0), (m2, 0.5), (m1, 0.5), (tip, 1.0))]
            f1 = (ids[0], ids[1], ids[2], ids[3])
            f2 = (ids[3], ids[2], ids[4])
            if sd == -1:
                f1, f2 = f1[::-1], f2[::-1]
            mb.f(f1, None, SMOOTH)
            mb.f(f2, None, SMOOTH)
        mb.box((mid.x, 0, mid.z + 0.005), (0.25, 0.012, 0.02), with_a(mul(col, 0.8), 0.0))
    return mb


# ================================================================================================ 6. laundry line
def make_laundry_line():
    mb = MB("SM_KG_LaundryLine", angle=40, material="Sway", pivot="keep")
    mb.floor = True
    rng = random.Random(4)
    X0 = 1.95

    def zl(x):
        return 1.95 - 0.13 * (1 - (x / X0) ** 2)
    for s in (-1, 1):
        mb.cbox((s * 2.0, 0, 1.03), (0.09, 0.09, 2.06), 0.012, WOOD_GREY)
        mb.cbox((s * 2.0, 0, 1.99), (0.07, 0.56, 0.07), 0.01, WOOD_GREY)
        for sy in (-1, 1):
            mb.beam((s * 2.0, 0, 1.72), (s * 2.0, sy * 0.2, 1.97), 0.04, 0.04, WOOD_GREY, up=(1, 0, 0))
        mb.blob((s * 2.0, 0, 0.02), (0.12, 0.12, 0.05), sub=1, amp=0.3, col=STONE_DK)
    mb.tube([(-X0 + 2 * X0 * i / 30, 0, zl(-X0 + 2 * X0 * i / 30)) for i in range(31)], 0.006, 5, col=ROPE)

    def hang(x0, x1, drop, col, rows=6, cols=5, wave=0.03, seed=0.0, facefn=None, flare=0.0, dropfn=None, dz=0.0):
        grid = []
        for i in range(rows + 1):
            v = i / rows
            row = []
            for j in range(cols + 1):
                u = j / cols
                x = lerp(x0 - flare * v, x1 + flare * v, u)
                d = dropfn(u) if dropfn else drop
                z = zl(min(max(x, -X0), X0)) - 0.012 - dz - d * v
                y = wave * math.sin(u * math.pi * 2 + seed) * v + 0.025 * v * v
                row.append((x, y, z))
            grid.append(row)
        mb.cloth(grid, lambda i, j, p: with_a(col, clamp((zl(min(max(p[0], -X0), X0)) - p[2]) / 1.0) ** 0.9),
                 thick=0.005, facefn=facefn)

    def pin(x):
        mb.box((x, 0, zl(x) - 0.015), (0.018, 0.024, 0.07), with_a(WOOD_LT, 0.0))
    # bed sheet with blue stripes
    hang(-1.8, -1.02, 0.98, WHITE, rows=7, cols=8, seed=0.5,
         facefn=lambda i, j: rgb(BLUE_LT) if j in (1, 6) else None)
    pin(-1.78)
    pin(-1.41)
    pin(-1.04)
    # red shirt (body + sleeves)
    hang(-0.84, -0.44, 0.62, RED, rows=5, cols=4, seed=1.3, facefn=lambda i, j: rgb(RED_DK) if j == 2 and i > 0 else None)
    hang(-0.98, -0.84, 0.34, RED, rows=3, cols=2, seed=2.0, flare=0.05)
    hang(-0.44, -0.3, 0.34, RED, rows=3, cols=2, seed=2.4, flare=0.05)
    pin(-0.83)
    pin(-0.45)
    # blue trousers
    hang(-0.18, 0.24, 0.24, C("3A5C9A"), rows=2, cols=4, seed=0.2)
    hang(-0.18, 0.01, 0.62, C("3A5C9A"), rows=5, cols=2, seed=0.7, dz=0.22)
    hang(0.05, 0.24, 0.62, C("3A5C9A"), rows=5, cols=2, seed=1.1, dz=0.22)
    pin(-0.17)
    pin(0.23)
    # socks
    for x, c in ((0.33, YELLOW), (0.44, RED)):
        hang(x, x + 0.08, 0.26, c, rows=3, cols=1, seed=x * 10, facefn=lambda i, j: rgb(WHITE) if i == 0 else None)
        pin(x + 0.04)
    # gingham tea towel
    hang(0.56, 0.98, 0.5, WHITE, rows=6, cols=6, seed=0.9,
         facefn=lambda i, j: rgb(RED) if (i + j) % 2 == 0 else rgb(WHITE))
    pin(0.58)
    pin(0.96)
    # green dress (flared)
    hang(1.12, 1.6, 0.95, GREEN, rows=7, cols=5, seed=2.2, flare=0.12,
         facefn=lambda i, j: rgb(WHITE) if i == 0 else (rgb(YELLOW) if i == 6 else None))
    pin(1.14)
    pin(1.58)
    hang(1.7, 1.88, 0.3, PINK, rows=3, cols=2, seed=3.0)
    pin(1.79)
    # wicker basket with folded laundry
    with mb.xf(T(1.2, -0.62, 0) @ S(1, 0.72, 1)):
        prof = [(0, 0), (0.2, 0), (0.24, 0.04), (0.28, 0.22), (0.3, 0.25), (0.27, 0.26), (0.25, 0.23), (0, 0.23)]
        mb.lathe(prof, 14, col=HAY_DK, sh=AUTO,
                 facefn=lambda i, j: rgb(HAY if (j + i) % 2 else HAY_DK) if i in (1, 2) else None)
    for k, (c, dz) in enumerate(((WHITE, 0.24), (BLUE_LT, 0.28), (YELLOW, 0.31))):
        mb.cbox((1.2 + 0.02 * k, -0.62, dz), (0.3 - 0.03 * k, 0.24 - 0.02 * k, 0.04), 0.012, c)
    return mb


# ================================================================================================ 7. planters
def make_flower_box():
    mb = MB("SM_KG_FlowerBox", angle=38, pivot="keep")
    flower_box(mb, random.Random(12), L=1.2, D=0.24, H=0.22, box_col=C("2F9C8E"), trim=WOOD_DK)
    return mb


def make_planter_large():
    mb = MB("SM_KG_Planter_Large", angle=38)
    rng = random.Random(13)
    W, H = 1.2, 0.55
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.cbox((sx * (W / 2 - 0.05), sy * (W / 2 - 0.05), (H + 0.07) / 2), (0.1, 0.1, H + 0.07), 0.012, WOOD_DK)
            mb.blob((sx * (W / 2 - 0.05), sy * (W / 2 - 0.05), H + 0.1), 0.055, sub=1, amp=0.0, col=GOLD)
    for side in range(4):
        with mb.xf(R(90 * side, 'Z')):
            for k in range(3):
                z = 0.06 + k * 0.165 + 0.08
                mb.cbox((0, -(W / 2 - 0.04), z), (W - 0.2, 0.05, 0.155), 0.01,
                        jit(C("3E7FB8") if k % 2 else C("3571A6"), rng, 0.04))
    mb.box((0, 0, H - 0.04), (W - 0.1, W - 0.1, 0.03), SOIL)
    mb.box((0, 0, 0.03), (W - 0.1, W - 0.1, 0.06), WOOD_DK)
    s0 = len(mb.V)
    mb.blob((0, 0, H + 0.36), (0.42, 0.42, 0.4), sub=2, amp=0.12, seed=3.0,
            colfn=lambda p, u: with_a(mix(C("3F8A34"), C("7CC455"), 0.35 + 0.5 * u.z + 0.15 * nz(p, 6.0)), 0.0))
    mb.cavity(s0, 0.3, 0.2)
    for k in range(10):
        a = k * 2 * math.pi / 10 + 0.3
        flower_clump(mb, rng, (0.44 * math.cos(a), 0.44 * math.sin(a), H + 0.02), r=0.09, n_fl=4,
                     blooms=[RED, YELLOW, WHITE, PINK])
    for k in range(7):
        u = rand_unit(rng)
        u.z = abs(u.z) * 0.6 + 0.4
        u.normalize()
        mb.blob(Vector((0, 0, H + 0.36)) + Vector((u.x * 0.42, u.y * 0.42, u.z * 0.4)), 0.045, sub=0, amp=0.1,
                col=rng.choice([PINK, WHITE, YELLOW]))
    return mb


def make_planter_pot():
    mb = MB("SM_KG_Planter_Pot", angle=38)
    rng = random.Random(14)
    prof = [(0, 0), (0.17, 0), (0.19, 0.03), (0.23, 0.36), (0.27, 0.37), (0.27, 0.45), (0.23, 0.46), (0.22, 0.42),
            (0, 0.42)]
    s0 = len(mb.V)
    mb.lathe(prof, 14, colfn=lambda r, z, a, i: with_a(mix(TERRA, C("E08A58"), 0.3 + 0.2 * math.sin(a * 3)), 0.0),
             segcol=[None] * 7 + [SOIL], sh=AUTO)
    for k in range(4):
        a = k * math.pi / 2 + 0.3
        mb.blob((0.1 * math.cos(a), 0.1 * math.sin(a), 0.52), (0.14, 0.13, 0.1), sub=1, amp=0.25, seed=k,
                col=jit(GREEN, rng, 0.1))
    for k in range(7):
        a = k * 2 * math.pi / 7
        r = rng.uniform(0.05, 0.16)
        c = Vector((r * math.cos(a), r * math.sin(a), rng.uniform(0.62, 0.74)))
        mb.cyl((c.x * 0.6, c.y * 0.6, 0.5), c, 0.006, seg=4, col=GREEN_DK)
        for q in range(5):
            u = rand_unit(rng)
            u.z = abs(u.z)
            mb.blob(c + u * 0.035, 0.028, sub=0, amp=0.1, col=jit(RED if k % 3 else C("FF5E7E"), rng, 0.05))
    return mb


# ================================================================================================ 8. hanging signs
SIGN_HINGE_BRACKET = (0.55, 0.0, 0.0)
SIGN_HINGE_POST = (0.6, 0.0, 2.7)
SIGN_F = frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))        # front face (+w) faces -Y
SIGN_B = frame((0, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 1, 0))        # back face (+w) faces +Y (mirrored emblem)


def sign_emblem(kind):
    """[(polygon, colour, extra_depth)] in the board plane (u = X, v = Z), panel centre at (0, -0.34)."""
    out = []
    if kind == "Fish":
        cx, cy = 0.03, -0.33
        body = ellipse_pts(cx, cy, 0.19, 0.08, 20, -150, 150) + [(cx - 0.28, cy + 0.085), (cx - 0.235, cy),
                                                                 (cx - 0.28, cy - 0.085)]
        out.append((body, C("F2F2EA"), 0.0))
        out.append(([(cx - 0.04, cy + 0.07), (cx + 0.06, cy + 0.075), (cx - 0.07, cy + 0.125)], C("F2F2EA"), 0.0))
        out.append((ellipse_pts(cx + 0.11, cy + 0.02, 0.018, 0.018, 8), C("1A2A40"), 0.004))
        out.append(([(cx + 0.05, cy + 0.06), (cx + 0.065, cy + 0.06), (cx + 0.07, cy - 0.06), (cx + 0.055, cy - 0.06)],
                    C("A9C3D6"), 0.003))
        wave = [(u, -0.49 + 0.015 * math.sin(u * 40)) for u in [-0.24 + 0.48 * k / 16 for k in range(17)]]
        out.append((wave + [(u, y - 0.025) for u, y in reversed(wave)], C("8FD3F0"), 0.0))
    elif kind == "Bread":
        loaf = ellipse_pts(0, -0.43, 0.21, 0.17, 18, 0, 180) + [(-0.21, -0.43), (-0.2, -0.47), (0.2, -0.47),
                                                               (0.21, -0.43)]
        out.append((loaf, C("E3A24C"), 0.0))
        for x in (-0.1, 0.0, 0.1):
            y = -0.32 - 0.02 * abs(x) * 5
            out.append(([(x - 0.045, y - 0.02), (x - 0.025, y - 0.035), (x + 0.045, y + 0.02), (x + 0.025, y + 0.035)],
                        CREAM, 0.004))
    elif kind == "Ale":
        out.append(([(-0.13, -0.52), (0.09, -0.52), (0.09, -0.25), (-0.13, -0.25)], C("E9A93A"), 0.0))
        for y in (-0.47, -0.31):
            out.append(([(-0.14, y - 0.012), (0.1, y - 0.012), (0.1, y + 0.012), (-0.14, y + 0.012)], WOOD_DK, 0.004))
        out.append(([(0.08, -0.29), (0.17, -0.29), (0.2, -0.32), (0.2, -0.44), (0.17, -0.47), (0.08, -0.47),
                     (0.08, -0.43), (0.15, -0.43), (0.16, -0.42), (0.16, -0.34), (0.15, -0.33), (0.08, -0.33)],
                    C("C98A2E"), 0.0))
        out.append(([(-0.15, -0.27), (0.11, -0.27), (0.125, -0.23), (0.09, -0.2), (0.05, -0.21), (0.02, -0.17),
                     (-0.03, -0.18), (-0.06, -0.16), (-0.1, -0.19), (-0.14, -0.2), (-0.16, -0.24)], WHITE, 0.003))
        out.append((ellipse_pts(-0.1, -0.3, 0.018, 0.03, 8), WHITE, 0.003))
    elif kind == "Anvil":
        out.append(([(-0.27, -0.27), (-0.1, -0.235), (0.2, -0.235), (0.2, -0.31), (0.08, -0.33), (0.06, -0.42),
                     (0.14, -0.46), (0.14, -0.51), (-0.14, -0.51), (-0.14, -0.46), (-0.06, -0.42), (-0.08, -0.33),
                     (-0.14, -0.31)], C("3C3E48"), 0.0))
        out.append(([(-0.1, -0.245), (0.19, -0.245), (0.19, -0.26), (-0.1, -0.26)], C("7A7D8A"), 0.003))
        for (x, y, s) in ((0.12, -0.17, 0.035), (0.2, -0.2, 0.025), (0.04, -0.15, 0.028), (-0.05, -0.18, 0.02)):
            out.append(([(x, y - s), (x + s * 0.35, y), (x, y + s), (x - s * 0.35, y)], YELLOW, 0.0))
    elif kind == "Herb":
        out.append((ellipse_pts(0, -0.47, 0.12, 0.08, 12, 180, 360) + [(0.13, -0.47), (-0.13, -0.47)], STONE_DK, 0.004))
        stem = [Vector((0.02 * math.sin(t * 3.0), -0.47 + 0.33 * t)) for t in [k / 10 for k in range(11)]]
        left = [(p.x - 0.008, p.y) for p in stem]
        right = [(p.x + 0.008, p.y) for p in reversed(stem)]
        out.append((left + right, GREEN_DK, 0.0))
        for k, t in enumerate((0.3, 0.45, 0.6, 0.75, 0.9)):
            p = stem[int(t * 10)]
            s = -1 if k % 2 else 1
            ang = math.radians(35 * s)
            pts = []
            for q in range(12):
                a = 2 * math.pi * q / 12
                lx, ly = 0.065 * math.cos(a) + 0.065, 0.026 * math.sin(a)
                lx *= s
                pts.append((p.x + lx * math.cos(ang) - ly * math.sin(ang), p.y + lx * math.sin(ang) + ly * math.cos(ang)))
            out.append((pts, C("6CC24A"), 0.002))
        out.append((ellipse_pts(stem[-1].x, stem[-1].y + 0.03, 0.026, 0.04, 10), C("6CC24A"), 0.002))
        for (x, y) in ((-0.17, -0.22), (0.17, -0.27), (-0.19, -0.33)):
            out.append((ellipse_pts(x, y, 0.025, 0.025, 8), C("B784E6"), 0.0))
            out.append((ellipse_pts(x, y, 0.01, 0.01, 6), YELLOW, 0.003))
    return out


SIGN_STYLE = {"Fish": (C("2C6FB0"), WOOD_DK), "Bread": (C("C0442F"), WOOD_DK), "Ale": (C("2F7A45"), WOOD_DK),
              "Anvil": (C("E08A2E"), IRON), "Herb": (C("EFE3C4"), C("7A4FA0"))}


def make_sign(kind):
    mb = MB(f"SM_KG_HangingSign_{kind}", angle=38, pivot="keep")
    panel, frm = SIGN_STYLE[kind]
    # eye rings around the hinge bar + iron straps down to the board
    for s in (-1, 1):
        with mb.xf(T(s * 0.26, 0, 0) @ R(90, 'Y')):
            mb.torus(0.032, 0.007, 10, 4, col=IRON_LT)
        mb.box((s * 0.26, 0, -0.07), (0.022, 0.012, 0.08), IRON)
        mb.blob((s * 0.26, 0, -0.105), 0.014, sub=0, amp=0.0, col=IRON_LT)

    def rrect(hw, top, bot, ch):
        return [(-hw + ch, bot), (hw - ch, bot), (hw, bot + ch), (hw, top - ch), (hw - ch, top), (-hw + ch, top),
                (-hw, top - ch), (-hw, bot + ch)]
    with mb.xf(SIGN_F):
        mb.ext2d(rrect(0.37, -0.09, -0.6, 0.06), -0.022, 0.022, frm)
        mb.ext2d(rrect(0.33, -0.12, -0.565, 0.045), -0.03, 0.03, panel)
        # decorative crest on top of the board
        mb.ext2d([(-0.08, -0.095), (0.08, -0.095), (0.05, -0.06), (0.0, -0.045), (-0.05, -0.06)], -0.018, 0.018, frm)
    for M in (SIGN_F, SIGN_B):
        with mb.xf(M):
            for pts, col, dz in sign_emblem(kind):
                mb.ext2d(pts, 0.02, 0.042 + dz, col)
    return mb


def make_sign_bracket():
    mb = MB("SM_KG_SignBracket", angle=38, pivot="keep")
    mb.box((0.01, 0, -0.17), (0.02, 0.09, 0.44), IRON)
    for z in (0.0, -0.36):
        mb.cyl((0.02, 0, z), (0.035, 0, z), 0.015, seg=6, col=IRON_LT)
    mb.box((0.49, 0, 0.0), (0.98, 0.035, 0.035), IRON)
    mb.beam((0.02, 0, -0.36), (0.62, 0, -0.018), 0.022, 0.022, IRON, up=(0, 1, 0))

    def spiral(cx, cz, r0, r1, a0, turns, n=24):
        return [(cx + lerp(r0, r1, k / n) * math.cos(a0 + turns * 2 * math.pi * k / n), 0,
                 cz + lerp(r0, r1, k / n) * math.sin(a0 + turns * 2 * math.pi * k / n)) for k in range(n + 1)]
    mb.tube(spiral(0.2, -0.13, 0.1, 0.025, math.pi * 0.5, 1.3), 0.009, 5, col=IRON)
    mb.tube(spiral(0.96, 0.05, 0.05, 0.014, -math.pi * 0.5, 1.2, 18), 0.011, 5, col=IRON)
    for x in (0.55 - 0.3, 0.55 + 0.3):
        mb.box((x, 0, 0), (0.012, 0.045, 0.045), IRON_LT)
    mb.info["sign_hinge"] = SIGN_HINGE_BRACKET
    return mb


def make_sign_post():
    mb = MB("SM_KG_SignPost", angle=38, pivot="keep")
    mb.cbox((0, 0, 0.12), (0.38, 0.38, 0.24), 0.03, STONE)
    mb.cbox((0, 0, 1.55), (0.16, 0.16, 2.9), 0.02, WOOD_DK)
    mb.lathe([(0, 3.0), (0.12, 3.0), (0, 3.13)], 4, col=WOOD_DK, sh=FLAT, phase=math.pi / 4)
    mb.cbox((0.47, 0, 2.83), (1.1, 0.1, 0.12), 0.015, WOOD)
    mb.beam((0.06, 0, 2.3), (0.52, 0, 2.78), 0.07, 0.07, WOOD, up=(0, 1, 0))
    mb.box((0.6, 0, 2.7), (0.78, 0.03, 0.03), IRON)
    for x in (0.23, 0.97):
        mb.box((x, 0, 2.745), (0.025, 0.035, 0.09), IRON)
    mb.box((0, -0.083, 1.45), (0.2, 0.006, 0.27), CREAM)
    mb.box((0.02, -0.084, 1.52), (0.12, 0.006, 0.015), C("5A4A3A"))
    mb.box((-0.01, -0.084, 1.47), (0.14, 0.006, 0.012), C("5A4A3A"))
    mb.box((0.0, -0.084, 1.42), (0.1, 0.006, 0.012), C("5A4A3A"))
    mb.blob((0, -0.088, 1.57), 0.012, sub=0, amp=0.0, col=RED)
    mb.info["sign_hinge"] = SIGN_HINGE_POST
    return mb


# ================================================================================================ 9. yard clutter
def wood_log(mb, rng, c, r, L, seg=7):
    """Round log lying along Y centred at c: bark sides, pale end grain with a darker ring."""
    bark = jit(C("6E4B2F"), rng, 0.12)
    end = jit(C("E4BA7C"), rng, 0.06)
    ring = mul(end, 0.78)
    with mb.xf(T(*c) @ R(-90, 'X') @ R(rng.uniform(0, 360), 'Z')):
        mb.lathe([(0, -L / 2), (r * 0.5, -L / 2), (r, -L / 2), (r, L / 2), (r * 0.5, L / 2), (0, L / 2)], seg, col=bark,
                 segcol=[end, ring, bark, ring, end], sh=AUTO, rfn=lambda a, i, z: 1.0 + 0.05 * math.sin(3 * a + r * 50))


def make_firewood_stack():
    mb = MB("SM_KG_Firewood_Stack", angle=38)
    rng = random.Random(31)
    for sy in (-1, 1):
        mb.box((0, sy * 0.2, 0.04), (1.9, 0.1, 0.08), WOOD_DK)
    for sx in (-1, 1):
        for sy, top in ((-1, 1.2), (1, 1.32)):
            mb.cbox((sx * 0.88, sy * 0.24, top / 2), (0.07, 0.07, top), 0.01, WOOD_GREY)
    rows = 5
    for rr in range(rows):
        n = 9 if rr % 2 == 0 else 8
        z = 0.08 + 0.085 + rr * 0.155
        for k in range(n):
            x = -0.8 + (1.6 / 8) * (k + (0.0 if rr % 2 == 0 else 0.5))
            if rr == rows - 1 and rng.random() < 0.3:
                continue
            wood_log(mb, rng, (x + rng.uniform(-0.01, 0.01), rng.uniform(-0.03, 0.03), z), rng.uniform(0.072, 0.09),
                     rng.uniform(0.55, 0.62), seg=rng.choice((6, 7)))
    # plank lean-to roof sloping down to the front (-Y)
    for k in range(10):
        x = -0.95 + 0.19 * (k + 0.5)
        mb.beam((x, -0.44, 1.19), (x, 0.44, 1.34), 0.185, 0.03, jit(WOOD_GREY if k % 2 else C("9A8C78"), rng, 0.05),
                up=(0, 0, 1), bevel=0.006)
    mb.beam((-1.0, -0.26, 1.24), (1.0, -0.26, 1.24), 0.05, 0.05, WOOD_GREY)
    mb.beam((-1.0, 0.26, 1.34), (1.0, 0.26, 1.34), 0.05, 0.05, WOOD_GREY)
    # chopping block with an axe at the front right
    with mb.xf(T(0.62, -0.62, 0)):
        mb.lathe([(0, 0), (0.2, 0), (0.2, 0.36), (0.14, 0.4), (0, 0.4)], 9, col=C("6E4B2F"),
                 segcol=[None, None, C("E4BA7C"), C("E4BA7C")], sh=AUTO)
        mb.beam((0.02, 0.0, 0.4), (0.3, 0.1, 0.78), 0.035, 0.035, WOOD_LT, up=(0, 1, 0), bevel=0.006)
        with mb.xf(T(0.02, 0, 0.42) @ R(-35, 'Y')):
            mb.ext2d([(-0.02, -0.06), (0.1, -0.08), (0.12, 0.06), (0.02, 0.04), (-0.02, 0.03)], -0.012, 0.012, IRON_LT)
    return mb


def make_sack():
    mb = MB("SM_KG_Sack", angle=40)
    sack(mb, random.Random(32), mark=RED)
    return mb


def make_sack_pile():
    mb = MB("SM_KG_SackPile", angle=40)
    rng = random.Random(33)
    marks = [RED, BLUE, None, GREEN, RED, None]
    spots = [(-0.42, 0.0, 0.17, 3), (0.0, 0.02, 0.17, -4), (0.42, -0.01, 0.17, 5), (-0.21, 0.03, 0.47, 8),
             (0.21, 0.0, 0.47, -6), (0.0, 0.05, 0.76, 2)]
    for k, (x, y, z, yaw) in enumerate(spots):
        with mb.xf(T(x, y + 0.31 * 0 - 0.02, z) @ R(yaw, 'Z') @ S(1.0, 1.0, 0.9) @ R(-90, 'X') @ T(0, 0, -0.31)):
            sack(mb, rng, mark=marks[k])
    with mb.xf(T(0.86, -0.1, 0) @ R(20, 'Z')):
        sack(mb, rng, open_top=True, h=0.6)
    # a wooden scoop in the open sack
    mb.beam((0.86, -0.1, 0.52), (0.98, -0.05, 0.72), 0.03, 0.02, WOOD_LT, up=(0, 1, 0))
    for k in range(9):
        a = rng.uniform(0, 6.28)
        r = rng.uniform(0.25, 0.5)
        mb.blob((0.86 + r * math.cos(a) * 0.6, -0.35 + r * math.sin(a) * 0.3, 0.0), (0.05, 0.05, 0.015), sub=0,
                amp=0.2, col=HAY_DK)
    return mb


def make_wheelbarrow():
    mb = MB("SM_KG_Wheelbarrow", angle=38)
    rng = random.Random(34)
    tray, rim, frame_c = jit(WOOD_LT, rng, 0.04), C("C0452F"), WOOD_DK
    # rails from the axle back to the handles
    for s in (-1, 1):
        mb.beam((0.66, s * 0.12, 0.22), (-0.95, s * 0.27, 0.58), 0.05, 0.06, frame_c, up=(0, 0, 1), bevel=0.008)
        mb.cyl((-0.95 + 0.0, s * 0.27, 0.58), (-1.12, s * 0.285, 0.62), 0.024, seg=6, col=WOOD_LT)
        mb.cbox((-0.36, s * 0.235, 0.22), (0.05, 0.05, 0.44), 0.008, frame_c)
        mb.box((-0.36, s * 0.235, 0.015), (0.09, 0.07, 0.03), IRON)
    # tray: bottom + sloped front/back + flared sides
    mb.cbox((0.0, 0, 0.47), (0.9, 0.52, 0.03), 0.006, tray)
    mb.beam((0.44, -0.27, 0.47), (0.6, -0.27, 0.78), 0.03, 0.5, tray, up=(1, 0, 0)) if False else None
    with mb.xf(T(0.45, 0, 0.47) @ R(-35, 'Y')):
        mb.cbox((0, 0, 0.17), (0.03, 0.56, 0.34), 0.006, tray)
    with mb.xf(T(-0.45, 0, 0.47) @ R(15, 'Y')):
        mb.cbox((0, 0, 0.14), (0.03, 0.56, 0.28), 0.006, tray)
    for s in (-1, 1):
        with mb.xf(T(0, s * 0.27, 0.47) @ R(s * -14, 'X')):
            with mb.xf(frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
                mb.ext2d([(-0.48, 0.0), (0.46, 0.0), (0.68, 0.3), (-0.53, 0.28)], -0.015, 0.015, tray)
            mb.beam((-0.53, s * 0.0, 0.29), (0.68, 0.0, 0.31), 0.04, 0.04, rim, up=(0, 0, 1))
    # wheel
    with mb.xf(T(0.68, 0, 0.21) @ R(-90, 'X')):
        mb.lathe([(0.17, -0.03), (0.21, -0.035), (0.21, 0.035), (0.17, 0.03)], 16, col=IRON, sh=AUTO, closed=True)
        mb.lathe([(0.15, -0.025), (0.175, -0.025), (0.175, 0.025), (0.15, 0.025)], 16, col=WOOD, sh=AUTO, closed=True)
        mb.lathe([(0, -0.07), (0.05, -0.07), (0.05, 0.07), (0, 0.07)], 8, col=WOOD_DK, sh=AUTO)
        for k in range(8):
            a = k * math.pi / 4
            mb.beam((0.04 * math.cos(a), 0.04 * math.sin(a), 0), (0.16 * math.cos(a), 0.16 * math.sin(a), 0), 0.025,
                    0.025, WOOD)
    return mb


def make_broom():
    mb = MB("SM_KG_Broom", angle=40)
    mb.cyl((0, 0, 0.3), (0, 0, 1.45), 0.018, seg=6, col=WOOD_LT)
    mb.lathe([(0, 0), (0.12, 0), (0.14, 0.04), (0.12, 0.16), (0.07, 0.3), (0.035, 0.37), (0, 0.38)], 12,
             colfn=lambda r, z, a, i: with_a(mix(HAY_DK, HAY, 0.5 + 0.5 * math.sin(a * 11)), 0.0), sh=AUTO,
             rfn=lambda a, i, z: 1.0 + 0.07 * math.sin(11 * a) * (1 if i < 3 else 0.3))
    for z, r in ((0.22, 0.095), (0.31, 0.058)):
        with mb.xf(T(0, 0, z)):
            mb.torus(r, 0.01, 12, 4, col=RED)
    return mb


def make_rain_barrel():
    mb = MB("SM_KG_RainBarrel", angle=35)
    rng = random.Random(35)
    for sx in (-1, 1):
        mb.cbox((sx * 0.2, 0, 0.06), (0.1, 0.72, 0.12), 0.012, WOOD_DK)
    z0 = 0.12
    prof = [(0, z0), (0.29, z0), (0.32, z0 + 0.1), (0.35, z0 + 0.45), (0.32, z0 + 0.82), (0.3, z0 + 0.9),
            (0.27, z0 + 0.9), (0.27, z0 + 0.83), (0, z0 + 0.83)]
    staves = [rgb(jit(WOOD if j % 2 else C("A86E3C"), rng, 0.06)) for j in range(16)]
    mb.lathe(prof, 16, col=WOOD, sh=AUTO,
             facefn=lambda i, j: rgb(WATER) if i == 7 else (staves[j] if i in (1, 2, 3, 4, 5, 6) else rgb(WOOD_DK)))
    for zz, rr in ((z0 + 0.1, 0.325), (z0 + 0.45, 0.355), (z0 + 0.8, 0.322)):
        with mb.xf(T(0, 0, zz)):
            mb.lathe([(rr - 0.01, -0.025), (rr + 0.012, -0.025), (rr + 0.012, 0.025), (rr - 0.01, 0.025)], 16, col=IRON,
                     sh=AUTO, closed=True)
    mb.blob((0.1, 0.05, z0 + 0.832), (0.05, 0.03, 0.004), sub=0, amp=0.0, col=GREEN)
    # brass tap at the front and a bucket under it
    mb.cyl((0, -0.32, z0 + 0.2), (0, -0.44, z0 + 0.2), 0.022, seg=8, col=GOLD)
    mb.cyl((0, -0.43, z0 + 0.2), (0, -0.43, z0 + 0.13), 0.018, 0.014, seg=8, col=GOLD)
    mb.box((0, -0.41, z0 + 0.26), (0.02, 0.02, 0.07), GOLD)
    with mb.xf(T(0.05, -0.5, 0)):
        mb.lathe([(0, 0), (0.12, 0), (0.15, 0.24), (0.13, 0.24), (0.11, 0.19), (0, 0.19)], 12, col=C("8A5A32"),
                 segcol=[None, None, None, None, WATER], sh=AUTO,
                 facefn=lambda i, j: rgb(WATER) if i == 4 else (rgb(C("8A5A32") if j % 2 else C("9E6A3C")) if i == 1 else None))
        with mb.xf(T(0, 0, 0.18)):
            mb.torus(0.14, 0.008, 12, 4, col=IRON)
    return mb


# ================================================================================================ 10. market goods
def open_crate(mb, rng, c, sx, sy, h, col=None):
    col = col or jit(WOOD_LT, rng, 0.05)
    cx, cy, cz = c
    mb.box((cx, cy, cz + 0.01), (sx, sy, 0.02), mul(col, 0.8))
    for s in (-1, 1):
        for k in range(2):
            z = cz + 0.02 + (h - 0.02) * (0.25 + 0.5 * k)
            mb.cbox((cx, cy + s * (sy / 2 - 0.01), z), (sx, 0.02, (h - 0.02) * 0.42), 0.004, jit(col, rng, 0.05))
            mb.cbox((cx + s * (sx / 2 - 0.01), cy, z), (0.02, sy - 0.04, (h - 0.02) * 0.42), 0.004, jit(col, rng, 0.05))
    for sx_ in (-1, 1):
        for sy_ in (-1, 1):
            mb.box((cx + sx_ * (sx / 2 - 0.02), cy + sy_ * (sy / 2 - 0.02), cz + h / 2), (0.035, 0.035, h), mul(col, 0.85))


def basket(mb, rng, c, rb, rt, h, sy=1.0, handle=False):
    cx, cy, cz = c
    with mb.xf(T(cx, cy, cz) @ S(1, sy, 1)):
        mb.lathe([(0, 0), (rb, 0), (rt, h), (rt + 0.015, h + 0.02), (rt - 0.015, h + 0.02), (rt - 0.02, h), (0, h - 0.01)],
                 14, col=HAY_DK, sh=AUTO,
                 facefn=lambda i, j: rgb(HAY if j % 2 else HAY_DK) if i == 1 else (rgb(C("A87C34")) if i == 6 else None))
        if handle:
            arc = [(rt * math.cos(t) * 0.95, 0, h + rt * 0.9 * math.sin(t)) for t in [math.pi * k / 10 for k in range(11)]]
            mb.tube(arc, 0.012, 5, col=HAY_DK)


def heap(rng, cx, cy, hx, hy, z0, peak, step, jitter=0.25):
    """Points on a mound (rectangular footprint): (x, y, z)."""
    out = []
    nx, ny = max(1, int(2 * hx / step)), max(1, int(2 * hy / step))
    for i in range(nx):
        for j in range(ny):
            x = cx - hx + step * (i + 0.5 + rng.uniform(-jitter, jitter))
            y = cy - hy + step * (j + 0.5 + rng.uniform(-jitter, jitter))
            u, v = (x - cx) / hx, (y - cy) / hy
            z = z0 + peak * max(0.0, 1 - u * u) * max(0.0, 1 - v * v)
            out.append((x, y, z))
    return out


def apple(mb, rng, p, col, s=1.0):
    with mb.xf(T(*p) @ R(rng.uniform(-25, 25), 'X') @ R(rng.uniform(0, 360), 'Z') @ S(s)):
        mb.lathe([(0, -0.036), (0.022, -0.04), (0.038, -0.022), (0.042, 0.0), (0.036, 0.022), (0.016, 0.03),
                  (0, 0.024)], 6, colfn=lambda r, z, a, i: with_a(mix(col, mix(col, YELLOW, 0.5), 0.3 * (0.5 + 0.5 *
                                                                                            math.sin(a + 1))), 0.0),
                 sh=SMOOTH)
        mb.box((0, 0, 0.034), (0.006, 0.006, 0.022), WOOD_DK)


def make_goods_apples():
    mb = MB("SM_KG_Goods_Apples", angle=40)
    rng = random.Random(41)
    open_crate(mb, rng, (-0.2, 0, 0), 0.52, 0.38, 0.16)
    mb.box((-0.2, 0, 0.1), (0.48, 0.34, 0.1), C("7A2A20"))
    reds = [C("D63A2A"), C("C92E24"), C("E0492C")]
    for p in heap(rng, -0.2, 0, 0.22, 0.15, 0.17, 0.08, 0.075):
        apple(mb, rng, p, jit(rng.choice(reds), rng, 0.08))
    for p in heap(rng, -0.2, 0, 0.12, 0.08, 0.23, 0.04, 0.075):
        apple(mb, rng, p, jit(rng.choice(reds), rng, 0.08))
    basket(mb, rng, (0.25, 0.02, 0), 0.12, 0.17, 0.13, handle=True)
    mb.blob((0.25, 0.02, 0.1), (0.15, 0.15, 0.03), sub=1, amp=0.0, col=C("5C7A2C"))
    greens = [C("8CC63F"), C("A6CF4A")]
    for p in heap(rng, 0.25, 0.02, 0.12, 0.12, 0.15, 0.06, 0.075):
        if math.hypot(p[0] - 0.25, p[1] - 0.02) < 0.15:
            apple(mb, rng, p, jit(rng.choice(greens), rng, 0.08))
    for (x, y) in ((0.05, -0.17), (0.43, -0.12), (0.1, 0.16)):
        apple(mb, rng, (x, y, 0.04), jit(rng.choice(reds), rng, 0.08))
    return mb


def cabbage(mb, rng, c, r):
    c = Vector(c)
    s0 = len(mb.V)
    mb.blob(c + Vector((0, 0, r * 0.05)), (r * 0.78, r * 0.78, r * 0.72), sub=1, amp=0.1, seed=rng.random() * 20,
            colfn=lambda p, u: with_a(mix(C("C5E48A"), C("9CCB5E"), 0.5 - 0.4 * u.z), 0.0))
    n = 5
    ph = rng.uniform(0, 6.28)
    for k in range(n):
        az = ph + 2 * math.pi * k / n
        grid = []
        for i in range(4):
            el = math.radians(-35 + 80 * i / 3)
            row = []
            for j in range(4):
                a = az + math.radians(-55 + 110 * j / 3)
                rr = r * (1.0 + 0.12 * (i / 3) ** 2)
                row.append(c + Vector((rr * math.cos(el) * math.cos(a), rr * math.cos(el) * math.sin(a),
                                       rr * 0.85 * math.sin(el))))
            grid.append(row)
        dark = jit(C("5FAE45"), rng, 0.1)
        mb.cloth(grid, lambda i, j, p, dark=dark: with_a(mix(dark, C("B8DC86"), 0.45 if j in (1, 2) and i < 2 else 0.0),
                                                        0.0), thick=0.006)
    return s0


def make_goods_cabbages():
    mb = MB("SM_KG_Goods_Cabbages", angle=40)
    rng = random.Random(42)
    open_crate(mb, rng, (-0.1, 0, 0), 0.64, 0.44, 0.16, col=jit(WOOD, rng, 0.05))
    mb.box((-0.1, 0, 0.09), (0.6, 0.4, 0.08), C("4A3424"))
    for i in range(3):
        for j in range(2):
            cabbage(mb, rng, (-0.3 + 0.2 * i + rng.uniform(-0.01, 0.01), -0.1 + 0.2 * j, 0.2 + (0.02 if i == 1 else 0)),
                    rng.uniform(0.085, 0.1))
    cabbage(mb, rng, (0.36, -0.08, 0.085), 0.09)
    cabbage(mb, rng, (0.34, 0.14, 0.08), 0.085)
    # a bunch of carrots
    for k in range(4):
        a = -0.3 + 0.2 * k
        base = Vector((0.3, -0.24, 0.02))
        d = Vector((math.cos(a), 0.3 * math.sin(a * 3), 0.05)).normalized()
        mb.cyl(base, base + d * 0.16, 0.022, 0.004, seg=6, col=ORANGE)
        mb.blob(base - d * 0.03 + Vector((0, 0, 0.02)), (0.05, 0.02, 0.02), sub=0, amp=0.2, col=GREEN)
    return mb


def fish_mesh(mb, rng, p, yaw, L, back, belly, stripes=False):
    """Fish lying on its side on a flat surface: length along X, height along Y (flat), thin in Z."""
    with mb.xf(T(*p) @ R(yaw, 'Z')):
        n = 10
        pts = [(-L / 2 + L * i / n, 0, 0) for i in range(n + 1)]
        h = [interp([(0, 0.02), (0.15, 0.1), (0.4, 0.13), (0.75, 0.11), (1.0, 0.05)], 1 - i / n) * L * 1.3
             for i in range(n + 1)]
        h = [max(x, 0.008) for x in h]

        def fc(i, k, q):
            side = math.cos(2 * math.pi * k / 8)
            c = back if side > 0.1 else belly
            if stripes and side > 0.1 and i % 2 == 0:
                c = mul(back, 0.6)
            return with_a(c, 0.0)
        mb.sweep(pts, circle(8), [x * 0.5 for x in h], [x * 0.22 for x in h], colfn=fc, sh=SMOOTH, tip=L * 0.03)
        tail = [(-L / 2 + 0.01, 0.0), (-L / 2 - L * 0.22, L * 0.13), (-L / 2 - L * 0.17, 0.0), (-L / 2 - L * 0.22, -L * 0.13)]
        mb.ext2d(tail, -0.004, 0.004, mix(back, belly, 0.3))
        mb.blob((L * 0.36, L * 0.02, L * 0.035), L * 0.03, sub=0, amp=0.0, col=WHITE)
        mb.blob((L * 0.365, L * 0.02, L * 0.05), L * 0.017, sub=0, amp=0.0, col=BLACK)


def make_goods_fish():
    mb = MB("SM_KG_Goods_Fish", angle=40)
    rng = random.Random(43)
    W, D, H = 0.9, 0.5, 0.1
    mb.box((0, 0, 0.015), (W, D, 0.03), WOOD_DK)
    for s in (-1, 1):
        mb.cbox((0, s * (D / 2 - 0.015), H / 2), (W, 0.03, H), 0.006, C("3F7FA8"))
        mb.cbox((s * (W / 2 - 0.015), 0, H / 2), (0.03, D - 0.06, H), 0.006, C("3F7FA8"))
    grid = []
    for i in range(7):
        row = []
        for j in range(11):
            x = -W / 2 + 0.03 + (W - 0.06) * j / 10
            y = -D / 2 + 0.03 + (D - 0.06) * i / 6
            z = 0.075 + 0.012 * math.sin(j * 2.1 + i * 1.3) + 0.01 * math.cos(j * 0.9 - i * 2.2)
            row.append((x, y, z))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(mix(C("E8F6FA"), C("B9E0EE"), 0.5 + 0.5 * math.sin(i * 2 + j * 1.7)), 0.0),
             thick=0.01, sh=FLAT)
    kinds = [(C("3E8C8C"), C("DDE8EA"), True), (C("D8503A"), C("F2C6A6"), False), (C("8A9AA8"), C("E6ECEE"), False)]
    spots = [(-0.26, -0.1, 12, 0.34), (0.02, -0.12, -8, 0.36), (0.28, -0.08, 16, 0.3), (-0.2, 0.11, -170, 0.32),
             (0.1, 0.12, 175, 0.38), (0.32, 0.13, -165, 0.28)]
    for k, (x, y, yaw, L) in enumerate(spots):
        back, belly, st = kinds[k % 3]
        fish_mesh(mb, rng, (x, y, 0.1), yaw, L, jit(back, rng, 0.05), belly, st)
    for k in range(3):
        mb.blob((-0.36 + 0.04 * k, 0.0 + 0.03 * k, 0.1), (0.035, 0.028, 0.028), sub=1, amp=0.05, col=YELLOW)
    # chalk price board on a stick
    mb.cyl((0.4, 0.2, 0.08), (0.4, 0.2, 0.36), 0.008, seg=4, col=WOOD_DK)
    mb.cbox((0.4, 0.2, 0.36), (0.16, 0.012, 0.1), 0.004, C("2E3436"))
    mb.box((0.4, 0.193, 0.37), (0.1, 0.004, 0.012), WHITE)
    mb.box((0.39, 0.193, 0.345), (0.07, 0.004, 0.012), WHITE)
    return mb


CRUST, CRUST_DK, CRUMB = C("C98A43"), C("9E5F2A"), C("EBC98A")


def make_goods_bread():
    mb = MB("SM_KG_Goods_Bread", angle=40)
    rng = random.Random(44)
    basket(mb, rng, (-0.27, 0.0, 0), 0.13, 0.17, 0.16, sy=0.85)
    for k in range(4):
        a = -0.5 + k * 0.35
        base = Vector((-0.27 + 0.06 * math.cos(k * 1.7), 0.04 * math.sin(k * 2.3), 0.04))
        d = Vector((math.sin(a) * 0.55, 0.2 * math.cos(k * 2.0), 1.0)).normalized()
        z = d
        x = z.orthogonal().normalized()
        y = z.cross(x)
        with mb.xf(frame(base, x, y, z)):
            mb.lathe([(0, 0), (0.02, 0.005), (0.032, 0.03), (0.035, 0.3), (0.03, 0.44), (0.018, 0.47), (0, 0.475)], 8,
                     colfn=lambda r, zz, a_, i: with_a(mix(CRUST, CRUST_DK, 0.3 + 0.3 * math.sin(a_ * 2)), 0.0), sh=SMOOTH)
            for s in range(4):
                mb.box((0.03, 0, 0.1 + s * 0.09), (0.012, 0.03, 0.045), CRUMB)
    mb.cbox((0.2, 0.02, 0.015), (0.5, 0.36, 0.03), 0.006, WOOD_LT)
    for (x, y, r) in ((0.08, 0.06, 0.1), (0.3, 0.1, 0.09), (0.3, -0.08, 0.085)):
        s0 = len(mb.V)
        mb.blob((x, y, 0.03 + r * 0.45), (r, r, r * 0.6), sub=2, amp=0.05, seed=x * 10,
                colfn=lambda p, u: with_a(mix(CRUST_DK, CRUST, 0.3 + 0.7 * u.z), 0.0))
        mb.cavity(s0, 0.2, 0.15)
        for ang in (45, -45):
            with mb.xf(T(x, y, 0.03 + r * 1.02) @ R(ang, 'Z')):
                mb.box((0, 0, 0), (r * 1.1, 0.014, 0.012), CRUMB)
    # pretzel
    pts = []
    for k in range(29):
        t = 2 * math.pi * k / 28
        pts.append((0.1 + 0.06 * math.sin(t) * (1 + 0.4 * math.cos(t)), -0.12 + 0.05 * math.sin(2 * t) - 0.02,
                    0.045 + 0.008 * math.sin(3 * t)))
    mb.tube(pts, 0.014, 6, col=CRUST_DK)
    for k in range(5):
        mb.blob((-0.1 + 0.05 * k, -0.2 + 0.01 * k, 0.03), 0.035, sub=1, amp=0.05, col=jit(CRUST, rng, 0.08))
    return mb


def make_goods_pottery():
    mb = MB("SM_KG_Goods_Pottery", angle=35)
    rng = random.Random(45)
    mb.cbox((0, 0, 0.008), (0.92, 0.5, 0.016), 0.004, HAY)
    for k in range(9):
        mb.box((-0.44 + 0.11 * k, 0, 0.017), (0.012, 0.5, 0.004), HAY_DK)

    def pot(p, prof, col, band=None, seg=14):
        with mb.xf(T(*p)):
            mb.lathe(prof, seg, colfn=lambda r, z, a, i: with_a(band if (band and 0.4 < z / prof[-2][1] < 0.52) else col, 0.0),
                     sh=SMOOTH)
    # amphora (terracotta) with handles
    pot((-0.3, 0.06, 0.016), [(0, 0), (0.06, 0), (0.1, 0.06), (0.13, 0.16), (0.12, 0.26), (0.07, 0.33), (0.05, 0.37),
                              (0.065, 0.4), (0.05, 0.4), (0, 0.39)], TERRA, C("7A3A22"))
    for s in (-1, 1):
        arc = [(-0.3 + s * (0.06 + 0.05 * math.sin(math.pi * k / 8)), 0.06, 0.39 - 0.12 * k / 8) for k in range(9)]
        mb.tube(arc, 0.012, 5, col=TERRA)
    # blue glazed vase
    pot((-0.08, 0.12, 0.016), [(0, 0), (0.05, 0), (0.08, 0.05), (0.09, 0.12), (0.05, 0.22), (0.04, 0.27), (0.06, 0.3),
                               (0.045, 0.3), (0, 0.29)], C("2F68C0"), WHITE)
    # stack of bowls
    for k in range(3):
        with mb.xf(T(0.14, 0.08, 0.016 + 0.03 * k)):
            mb.lathe([(0, 0), (0.04, 0), (0.1, 0.045), (0.105, 0.055), (0.095, 0.055), (0.035, 0.012), (0, 0.012)], 14,
                     col=CREAM, segcol=[None, None, C("2C5FA8"), C("2C5FA8"), None, None], sh=AUTO)
    # green pitcher with spout + handle
    with mb.xf(T(0.33, 0.05, 0.016)):
        mb.lathe([(0, 0), (0.06, 0), (0.08, 0.06), (0.075, 0.14), (0.055, 0.2), (0.06, 0.24), (0.05, 0.24), (0, 0.23)],
                 12, col=C("4E9E52"), sh=SMOOTH)
        mb.ext2d([(0.05, 0.2), (0.1, 0.25), (0.05, 0.24)], -0.02, 0.02, C("4E9E52")) if False else None
        with mb.xf(frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
            mb.ext2d([(0.045, 0.2), (0.1, 0.25), (0.05, 0.24)], -0.02, 0.02, C("4E9E52"))
        arc = [(-0.06 - 0.05 * math.sin(math.pi * k / 8), 0, 0.2 - 0.12 * k / 8) for k in range(9)]
        mb.tube(arc, 0.011, 5, col=C("4E9E52"))
    # cups and a small jug at the front
    for (x, y, c) in ((-0.1, -0.14, C("E8D7B0")), (0.02, -0.16, C("C9463A")), (0.24, -0.15, C("E8D7B0"))):
        with mb.xf(T(x, y, 0.016)):
            mb.lathe([(0, 0), (0.035, 0), (0.042, 0.07), (0.035, 0.07), (0.03, 0.012), (0, 0.012)], 10, col=c, sh=AUTO)
    pot((-0.3, -0.15, 0.016), [(0, 0), (0.04, 0), (0.07, 0.05), (0.06, 0.1), (0.03, 0.13), (0.04, 0.15), (0, 0.145)],
        C("B85A34"), C("E8D7B0"))
    return mb


def make_goods_cloth():
    mb = MB("SM_KG_Goods_Cloth", angle=35)
    rng = random.Random(46)
    cols = [RED, BLUE, YELLOW, GREEN, PURPLE]
    spots = [(-0.36, 0.07), (-0.21, 0.07), (-0.06, 0.07), (-0.285, 0.19), (-0.135, 0.19)]
    for k, (x, z) in enumerate(spots):
        c = cols[k]
        with mb.xf(T(x, 0.0, z) @ R(-90, 'X')):
            mb.lathe([(0, -0.26), (0.02, -0.26), (0.055, -0.26), (0.07, -0.25), (0.07, 0.25), (0.055, 0.26), (0.02, 0.26),
                      (0, 0.26)], 12, col=c, segcol=[WOOD_LT, mul(c, 0.75), c, c, c, mul(c, 0.75), WOOD_LT], sh=AUTO)
        # a loose flap hanging down the front of the roll
        if k < 3:
            grid = [[(x - 0.05 + 0.1 * j, -0.26 - 0.005 * i * i, z - 0.03 * i) for j in range(2)] for i in range(4)]
            grid = [[(x - 0.065 + 0.13 * j / 1, -0.2 + 0.0, z) for j in range(2)]]
    # folded stacks with stripes / checks
    stack = [(C("F2E6CC"), None), (C("C0442F"), WHITE), (C("2F6DC0"), None), (C("F6C431"), C("E08A2E")),
             (C("8C4DB8"), None)]
    for k, (c, pat) in enumerate(stack):
        z = 0.02 + 0.045 * k
        mb.cbox((0.24 + rng.uniform(-0.01, 0.01), 0.02, z + 0.02), (0.34, 0.28, 0.04), 0.012, c)
        if pat:
            for s in range(3):
                mb.box((0.16 + 0.08 * s, 0.02, z + 0.02), (0.025, 0.285, 0.042), pat)
    # draped cloth over the front edge of the stack
    grid = []
    for i in range(6):
        v = i / 5
        row = []
        for j in range(5):
            u = j / 4
            x = 0.1 + 0.28 * u
            y = -0.12 - 0.04 * v
            z = 0.25 - 0.22 * v * v
            row.append((x, -0.12 - 0.02 * math.sin(math.pi * v) if i < 2 else -0.14, 0.26) if False else (x, y, z))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(TEAL, 0.0), thick=0.006,
             facefn=lambda i, j: rgb(WHITE) if i == 4 else None)
    return mb


# ================================================================================================ 11. awnings
def make_awning(name, c1, c2):
    mb = MB(f"SM_KG_{name}", angle=40, material="Sway")
    rng = random.Random(51)
    Wx, yf, yb, zf, zb = 1.45, -0.95, 0.95, 2.25, 2.6
    for sx in (-1, 1):
        for y, top in ((yf, zf), (yb, zb)):
            mb.cbox((sx * Wx, y, top / 2), (0.1, 0.1, top), 0.012, WOOD_DK)
            mb.cbox((sx * Wx, y, 0.06), (0.18, 0.18, 0.12), 0.02, STONE)
        mb.beam((sx * Wx, yf, zf), (sx * Wx, yb, zb), 0.08, 0.08, WOOD, up=(0, 0, 1))
    mb.beam((-Wx - 0.1, yf, zf), (Wx + 0.1, yf, zf), 0.08, 0.08, WOOD, up=(0, 0, 1))
    mb.beam((-Wx - 0.1, yb, zb), (Wx + 0.1, yb, zb), 0.08, 0.08, WOOD, up=(0, 0, 1))
    nx, ny = 20, 6
    x0, x1, y0, y1 = -1.56, 1.56, -1.06, 1.04

    def zc(y):
        return lerp(zf, zb, (y - yf) / (yb - yf)) + 0.07

    def stripe(j):
        return rgb(c1) if (j // 2) % 2 == 0 else rgb(c2)
    grid = []
    for i in range(ny + 1):
        v = i / ny
        y = lerp(y0, y1, v)
        row = []
        for j in range(nx + 1):
            u = j / nx
            x = lerp(x0, x1, u)
            sag = 0.05 * math.sin(math.pi * ((x - x0) / (x1 - x0) * 2 % 1.0)) * math.sin(math.pi * v)
            row.append((x, y, zc(y) - sag))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(c1, 0.0), thick=0.01, facefn=lambda i, j: stripe(j))
    # scalloped valance hanging from the front edge (sways)
    grid = []
    for i in range(4):
        v = i / 3
        row = []
        for j in range(nx * 2 + 1):
            u = j / (nx * 2)
            x = lerp(x0, x1, u)
            scal = 0.1 * abs(math.sin(math.pi * (x - x0) / ((x1 - x0) / 10)))
            z = zc(y0) - (0.3 - 0.1 + scal) * v
            row.append((x, y0 - 0.005 - 0.01 * v, z))
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(c1, (i / 3) ** 1.3), thick=0.008, facefn=lambda i, j: stripe(j // 2))
    for sx in (-1, 1):
        mb.blob((sx * Wx, yf, zf + 0.06), 0.05, sub=1, amp=0.0, col=GOLD)
    return mb


# ================================================================================================ 12. farm
def make_haystack():
    mb = MB("SM_KG_Haystack", angle=28)
    rng = random.Random(61)
    prof = [(0, 0), (1.34, 0), (1.42, 0.14), (1.4, 0.5), (1.3, 0.9), (1.12, 1.35), (0.87, 1.8), (0.57, 2.2), (0.27, 2.5),
            (0.06, 2.63), (0, 2.66)]

    def hay(r, z, a, i):
        p = (r * math.cos(a), r * math.sin(a), z)
        c = mix(HAY_DK, HAY, 0.55 + 0.45 * nz(p, 3.0, (1, 2, 3)))
        c = mix(c, C("B7A274"), smoothstep(1.9, 2.6, z) * 0.6)
        c = mix(c, C("9C7A34"), (1 - smoothstep(0.0, 0.4, z)) * 0.5)
        return with_a(c, 0.0)
    s0 = len(mb.V)
    mb.lathe(prof, 24, colfn=hay, sh=AUTO,
             rfn=lambda a, i, z: 1.0 + 0.05 * nz((math.cos(a) * 2, math.sin(a) * 2, z), 1.5) + 0.035 * math.sin(9 * a + z * 3))
    mb.cavity(s0, 0.4, 0.25)
    mb.cyl((0, 0, 2.3), (0.03, 0, 3.0), 0.04, 0.03, seg=6, col=WOOD_DK)
    for k in range(40):
        a = rng.uniform(0, 6.28)
        z = rng.uniform(0.2, 2.3)
        r = interp([(p[1], p[0]) for p in prof[1:-1]], z)
        base = Vector((r * 0.97 * math.cos(a), r * 0.97 * math.sin(a), z))
        out = Vector((math.cos(a), math.sin(a), -0.6)).normalized()
        mb.cyl(base, base + out * rng.uniform(0.15, 0.3), 0.012, 0.002, seg=3, col=jit(HAY, rng, 0.1))
    grid = []
    for i in range(3):
        row = []
        for j in range(24):
            a = 2 * math.pi * j / 24
            rr = 1.35 + 0.35 * i + 0.1 * math.sin(a * 5)
            row.append((rr * math.cos(a), rr * math.sin(a), 0.015 - 0.005 * i))
        row.append(row[0])
        grid.append(row)
    mb.cloth(grid, lambda i, j, p: with_a(mix(HAY_DK, HAY, 0.4), 0.0), thick=0.01, sh=FLAT)
    # pitchfork stuck in the stack
    a = math.radians(-65)
    ra = Vector((math.cos(a), math.sin(a), 0))
    head = ra * 1.12 + Vector((0, 0, 1.2))
    dirv = (ra * 0.55 + Vector((0, 0, 0.85))).normalized()
    mb.cyl(head, head + dirv * 1.3, 0.022, seg=6, col=WOOD_LT)
    side = Vector((0, 0, 1)).cross(ra).normalized()
    mb.beam(head - side * 0.12, head + side * 0.12, 0.025, 0.025, IRON, up=dirv)
    for s in (-0.1, 0.0, 0.1):
        mb.cyl(head + side * s, head + side * s - dirv * 0.3, 0.008, 0.004, seg=4, col=IRON_LT)
    return mb


def make_haybale_round():
    mb = MB("SM_KG_HayBale_Round", angle=30)
    R_, L = 0.65, 1.2
    radii = [0.0, 0.14, 0.28, 0.42, 0.56]
    prof = [(r, -L / 2) for r in radii] + [(R_ - 0.02, -L / 2 + 0.01), (R_, -L / 2 + 0.06), (R_ + 0.01, 0.0),
                                           (R_, L / 2 - 0.06), (R_ - 0.02, L / 2 - 0.01)] + [(r, L / 2) for r in radii[::-1]]
    nseg = len(prof) - 1
    end_cols = [HAY, HAY_DK, HAY, HAY_DK, HAY, HAY_DK]

    def fcol(i, j):
        if i < 5:
            return rgb(end_cols[i])
        if i >= nseg - 5:
            return rgb(end_cols[nseg - 1 - i])
        return None
    with mb.xf(T(0, 0, R_) @ R(-90, 'X')):
        s0 = len(mb.V)
        mb.lathe(prof, 24, sh=AUTO, facefn=fcol,
                 colfn=lambda r, z, a, i: with_a(mix(HAY_DK, HAY, 0.55 + 0.45 * math.sin(a * 13 + z * 20)), 0.0),
                 rfn=lambda a, i, z: 1.0 + (0.02 * math.sin(a * 17 + i) if 4 < i < len(prof) - 5 else 0.0))
        for z in (-0.3, 0.3):
            with mb.xf(T(0, 0, z)):
                mb.torus(R_ + 0.012, 0.012, 24, 4, col=C("C9452E"))
    return mb


def make_haybale_square():
    mb = MB("SM_KG_HayBale_Square", angle=35)
    rng = random.Random(63)
    n = 8
    for k in range(n):
        x = -0.5 + 1.0 * (k + 0.5) / n
        s0 = mb.cbox((x, rng.uniform(-0.01, 0.01), 0.21 + rng.uniform(-0.005, 0.005)),
                     (1.0 / n - 0.004, 0.48 + rng.uniform(-0.02, 0.02), 0.42 + rng.uniform(-0.015, 0.015)), 0.02,
                     jit(mix(HAY, HAY_DK, rng.random() * 0.6), rng, 0.06))
    for x in (-0.25, 0.25):
        for (c, s) in (((x, -0.247, 0.21), (0.025, 0.006, 0.43)), ((x, 0.247, 0.21), (0.025, 0.006, 0.43)),
                       ((x, 0, 0.422), (0.025, 0.5, 0.006))):
            mb.box(c, s, C("C9452E"))
    for k in range(10):
        s = rng.choice((-1, 1))
        y, z = rng.uniform(-0.2, 0.2), rng.uniform(0.05, 0.38)
        mb.cyl((s * 0.49, y, z), (s * (0.55 + rng.uniform(0, 0.06)), y + rng.uniform(-0.04, 0.04), z - 0.03), 0.008,
               0.002, seg=3, col=HAY)
    return mb


def make_scarecrow():
    mb = MB("SM_KG_Scarecrow", angle=38, material="Sway")
    rng = random.Random(64)
    mb.cbox((0, 0, 1.08), (0.08, 0.08, 2.16), 0.01, WOOD_DK)
    mb.cbox((0, 0, 1.55), (1.5, 0.07, 0.07), 0.01, WOOD)
    # sack head with a stitched face (front = -Y) and a straw hat
    s0 = len(mb.V)
    mb.blob((0, 0, 1.86), (0.16, 0.15, 0.18), sub=2, amp=0.06, seed=3, col=BURLAP)
    mb.cavity(s0, 0.3, 0.2)
    for sx in (-1, 1):
        with mb.xf(T(sx * 0.06, -0.145, 1.9) @ R(90, 'X')):
            for ang in (45, -45):
                with mb.xf(R(ang, 'Y')):
                    mb.box((0, 0, 0), (0.06, 0.012, 0.012), BLACK)
    for k in range(7):
        t = (k - 3) / 3
        mb.box((t * 0.07, -0.145 + 0.01 * abs(t), 1.8 - 0.02 * (1 - t * t)), (0.015, 0.01, 0.025), BLACK)
    mb.box((0, -0.15, 1.85), (0.03, 0.02, 0.03), C("E0703A"))
    with mb.xf(T(0, 0, 1.98) @ R(-8, 'X') @ R(6, 'Y')):
        mb.lathe([(0, 0), (0.3, 0.0), (0.31, 0.02), (0.16, 0.03), (0.13, 0.16), (0.06, 0.2), (0, 0.205)], 14,
                 colfn=lambda r, z, a, i: with_a(mix(HAY, HAY_DK, 0.5 + 0.5 * math.sin(a * 9)), 0.0),
                 segcol=[None, None, None, None, None, None], sh=AUTO)
        mb.lathe([(0.155, 0.03), (0.162, 0.03), (0.15, 0.08), (0.143, 0.08)], 14, col=RED, sh=AUTO, closed=True)
    # plaid shirt torso with a ragged hem, patches
    plaid_a, plaid_b = C("C23A30"), C("7A1E2A")

    def shirt_col(i, j):
        return rgb(plaid_a) if (i + j) % 2 == 0 else rgb(plaid_b if (j % 4 < 2) else C("E0C060"))
    with mb.xf(S(1.0, 0.72, 1.0)):
        s0 = len(mb.V)
        mb.lathe([(0.2, 0.95), (0.21, 1.1), (0.22, 1.3), (0.24, 1.5), (0.2, 1.6), (0.07, 1.65), (0, 1.66)], 12,
                 col=plaid_a, facefn=lambda i, j: shirt_col(i, j), sh=AUTO)
        rngh = random.Random(5)
        for vi in range(s0, s0 + 12):
            mb.V[vi] = mb.V[vi] - Vector((0, 0, rngh.uniform(0.0, 0.08)))
            mb.VC[vi] = with_a(mb.VC[vi], 0.5)
    mb.cbox((0.08, -0.16, 1.25), (0.1, 0.02, 0.1), 0.01, YELLOW)
    mb.cbox((-0.1, -0.155, 1.42), (0.08, 0.02, 0.07), 0.01, C("3A5C9A"))
    # sleeves along the crossbar with straw poking out
    for s in (-1, 1):
        mb.tube([(s * 0.18, 0, 1.55), (s * 0.45, 0, 1.54), (s * 0.66, 0, 1.52)], [0.1, 0.09, 0.1], 8,
                col=plaid_a, colfn=lambda i, k, p: with_a(plaid_a if (i + k) % 2 == 0 else plaid_b, 0.0))
        for k in range(6):
            a = 2 * math.pi * k / 6
            b = Vector((s * 0.66, 0.06 * math.cos(a), 1.52 + 0.06 * math.sin(a)))
            mb.cyl(b, b + Vector((s * rng.uniform(0.1, 0.18), 0.04 * math.cos(a), 0.04 * math.sin(a) - 0.04)), 0.012,
                   0.003, seg=3, col=HAY)
        # rag strips hanging from the sleeves (sway)
        for k in range(2):
            x = s * (0.35 + 0.18 * k)
            grid = [[(x - 0.025 + 0.05 * j, -0.02 - 0.03 * i * i / 9, 1.46 - 0.33 * i / 5) for j in range(2)]
                    for i in range(6)]
            c = [YELLOW, BLUE, GREEN, WHITE][(k + (s > 0) * 2) % 4]
            mb.cloth(grid, lambda i, j, p, c=c: with_a(c, i / 5), thick=0.004)
    for k in range(8):
        a = 2 * math.pi * k / 8
        mb.cyl((0.05 * math.cos(a), 0.05 * math.sin(a), 1.68), (0.12 * math.cos(a), 0.12 * math.sin(a), 1.62), 0.012,
               0.003, seg=3, col=HAY)
    # dangling trouser legs (partial sway)
    for s in (-1, 1):
        mb.tube([(s * 0.09, 0, 0.98), (s * 0.1, -0.01, 0.7), (s * 0.1, -0.02, 0.48)], [0.085, 0.08, 0.085], 8,
                colfn=lambda i, k, p: with_a(C("3A5C9A"), 0.2 * i))
        for k in range(5):
            a = 2 * math.pi * k / 5
            b = Vector((s * 0.1 + 0.05 * math.cos(a), -0.02 + 0.05 * math.sin(a), 0.48))
            mb.cyl(b, b + Vector((0.02 * math.cos(a), 0.02 * math.sin(a), -rng.uniform(0.08, 0.14))), 0.011, 0.003,
                   seg=3, col=with_a(HAY, 0.4))
    mb.cbox((0.1, -0.075, 0.75), (0.08, 0.02, 0.08), 0.01, with_a(C("C9452E"), 0.3))
    # a crow perched on the right arm
    cx, cz = 0.52, 1.62
    mb.blob((cx, 0, cz), (0.09, 0.05, 0.055), sub=1, amp=0.05, col=BLACK, rot=Matrix.Rotation(math.radians(20), 3, 'Y'))
    mb.blob((cx + 0.08, 0, cz + 0.05), 0.04, sub=1, amp=0.0, col=BLACK)
    mb.cyl((cx + 0.11, 0, cz + 0.05), (cx + 0.17, 0, cz + 0.035), 0.013, 0.002, seg=5, col=C("4A4A50"))
    mb.card([(cx - 0.06, -0.012, cz + 0.02), (cx - 0.17, -0.012, cz - 0.03), (cx - 0.16, 0.012, cz - 0.06),
             (cx - 0.05, 0.012, cz - 0.02)], BLACK)
    for sy in (-1, 1):
        mb.blob((cx + 0.105, sy * 0.028, cz + 0.065), 0.008, sub=0, amp=0.0, col=YELLOW)
    return mb


def make_beehive():
    mb = MB("SM_KG_Beehive", angle=35)
    rng = random.Random(65)
    mb.lathe([(0, 0), (0.3, 0), (0.28, 0.46), (0.25, 0.5), (0, 0.5)], 10, col=C("6E4B2F"),
             segcol=[None, None, C("C99A62"), C("E4BA7C")], sh=AUTO, rfn=lambda a, i, z: 1.0 + 0.05 * math.sin(3 * a))
    prof = [(0, 0.5), (0.3, 0.5)]
    n = 10
    for k in range(n):
        t = (k + 0.5) / n
        r = 0.3 * math.sqrt(max(0.0, 1 - t ** 1.6)) + 0.01
        prof.append((r * 1.04, 0.5 + 0.5 * t - 0.02))
        prof.append((r * 0.98, 0.5 + 0.5 * (k + 1) / n))
    prof.append((0, 1.0))
    mb.lathe(prof, 16, col=HAY, sh=AUTO,
             segcol=[HAY_DK] + [HAY if k % 2 == 0 else HAY_DK for k in range(len(prof) - 2)])
    with mb.xf(frame((0, -0.285, 0.5), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        mb.ext2d([(-0.06, 0.0), (0.06, 0.0)] + [(0.06 * math.cos(math.radians(a)), 0.06 * math.sin(math.radians(a)))
                                              for a in range(30, 180, 30)], -0.05, 0.012, C("2A1E14"))
    mb.cbox((0, -0.33, 0.49), (0.18, 0.1, 0.02), 0.004, WOOD_LT)
    for k, (x, y, z) in enumerate(((0.08, -0.42, 0.62), (-0.12, -0.38, 0.72), (0.02, -0.5, 0.85), (0.2, -0.3, 0.9))):
        with mb.xf(T(x, y, z) @ R(k * 70, 'Z')):
            mb.blob((0, 0, 0), (0.02, 0.013, 0.013), sub=1, amp=0.0, col=YELLOW)
            mb.box((0, 0, 0), (0.008, 0.028, 0.028), BLACK)
            for s in (-1, 1):
                mb.card([(0, 0, 0.01), (-0.012, s * 0.03, 0.025), (0.012, s * 0.028, 0.02)], WHITE)
    return mb


def dry_stone(mb, rng, c, s, col):
    s0 = mb.cbox(c, s, min(0.035, min(s) * 0.3), col)
    mb.deform(s0, lambda p: p + Vector((rng.uniform(-0.012, 0.012), rng.uniform(-0.012, 0.012), rng.uniform(-0.01, 0.01))))
    return s0


STONES = [C("A8A092"), C("8F887C"), C("B3A68E"), C("9A9CA0"), C("C0B6A2")]


def make_stone_wall():
    mb = MB("SM_KG_StoneWall", angle=30)
    rng = random.Random(66)
    mb.box((0, 0, 0.4), (3.98, 0.3, 0.78), mul(STONE_DK, 0.45))
    z = 0.0
    for rr in range(4):
        h = rng.uniform(0.16, 0.21)
        d = 0.56 - 0.1 * (z + h / 2) / 0.8
        x = -2.0
        while x < 2.0 - 1e-6:
            L = rng.uniform(0.26, 0.52)
            if 2.0 - (x + L) < 0.2:
                L = 2.0 - x
            s0 = dry_stone(mb, rng, (x + L / 2, rng.uniform(-0.015, 0.015), z + h / 2),
                           (L - 0.02, d + rng.uniform(-0.03, 0.02), h - 0.012), jit(rng.choice(STONES), rng, 0.06))
            mb.recolor(s0, lambda p, c, top=z + h: mix(c, MOSS, 0.55) if (p.z > top - 0.02 and rng.random() < 0.3) else c)
            x += L
        z += h
    x = -1.97
    while x < 1.95:
        t = rng.uniform(0.09, 0.14)
        hgt = rng.uniform(0.2, 0.27)
        with mb.xf(T(x + t / 2, 0, z + hgt / 2 - 0.02) @ R(rng.uniform(-8, 8), 'Y')):
            s0 = dry_stone(mb, rng, (0, 0, 0), (t, 0.44 + rng.uniform(-0.03, 0.03), hgt), jit(rng.choice(STONES), rng, 0.06))
        mb.recolor(s0, lambda p, c: mix(c, MOSS, 0.6) if (p.z > z + hgt - 0.08 and rng.random() < 0.45) else c)
        x += t + 0.01
    for k in range(7):
        xx = rng.uniform(-1.8, 1.8)
        sy = rng.choice((-1, 1))
        mb.blob((xx, sy * 0.3, 0.03), (0.12, 0.08, 0.07), sub=1, amp=0.3, seed=k, col=jit(C("5E9E3A"), rng, 0.12))
    mb.floor = True
    return mb


def make_stone_wall_end():
    mb = MB("SM_KG_StoneWall_End", angle=30)
    rng = random.Random(67)
    mb.box((0, 0, 0.5), (0.6, 0.56, 0.95), mul(STONE_DK, 0.45))
    z = 0.0
    for rr, h in enumerate((0.3, 0.26, 0.28)):
        for k in range(2):
            if rr % 2 == 0:
                c = (0, -0.16 + 0.32 * k, z + h / 2)
                s = (0.68, 0.32, h - 0.015)
            else:
                c = (-0.17 + 0.34 * k, 0, z + h / 2)
                s = (0.33, 0.64, h - 0.015)
            dry_stone(mb, rng, c, s, jit(rng.choice(STONES), rng, 0.06))
        z += h
    s0 = dry_stone(mb, rng, (0, 0, z + 0.06), (0.8, 0.74, 0.12), jit(STONE_LT, rng, 0.04))
    mb.recolor(s0, lambda p, c: mix(c, MOSS, 0.5) if (p.z > z + 0.1 and rng.random() < 0.4) else c)
    return mb


def make_water_trough():
    mb = MB("SM_KG_WaterTrough", angle=35)
    rng = random.Random(68)
    L, W, H = 1.7, 0.6, 0.55
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.cbox((sx * (L / 2 - 0.1), sy * (W / 2 - 0.06), 0.06), (0.1, 0.1, 0.12), 0.01, WOOD_DK)
    mb.box((0, 0, 0.14), (L, W, 0.04), WOOD_DK)
    for s in (-1, 1):
        for k in range(3):
            z = 0.16 + 0.13 * k + 0.065
            mb.cbox((0, s * (W / 2 - 0.025), z), (L, 0.05, 0.125), 0.008, jit(WOOD if k % 2 else C("A86E3C"), rng, 0.05))
        mb.cbox((s * (L / 2 - 0.025), 0, 0.35), (0.05, W - 0.1, 0.4), 0.008, jit(WOOD, rng, 0.05))
        for x in (-L / 2 + 0.06, L / 2 - 0.06):
            mb.box((x, s * (W / 2 + 0.001), 0.35), (0.06, 0.006, 0.42), IRON)
    mb.box((0, 0, 0.47), (L - 0.1, W - 0.1, 0.02), WATER)
    mb.blob((-0.3, 0.05, 0.482), (0.05, 0.03, 0.004), sub=0, amp=0.0, col=C("E0A030"))
    mb.blob((0.2, -0.1, 0.482), (0.04, 0.025, 0.004), sub=0, amp=0.0, col=GREEN)
    # cast-iron hand pump at the +X end
    pg = C("2E7D4F")
    with mb.xf(T(L / 2 + 0.12, 0, 0)):
        mb.cbox((0, 0, 0.06), (0.26, 0.26, 0.12), 0.02, STONE)
        mb.lathe([(0, 0.12), (0.08, 0.12), (0.07, 0.2), (0.055, 0.25), (0.055, 0.9), (0.07, 0.92), (0.07, 1.0),
                  (0.05, 1.03), (0, 1.04)], 10, col=pg, sh=AUTO)
        mb.cyl((0, 0, 0.82), (-0.3, 0, 0.72), 0.03, 0.025, seg=8, col=pg)
        mb.cyl((-0.3, 0, 0.72), (-0.32, 0, 0.66), 0.03, seg=8, col=pg)
        mb.blob((0, 0, 1.07), 0.04, sub=1, amp=0.0, col=GOLD)
        mb.beam((0.0, 0, 1.0), (0.42, 0, 1.18), 0.03, 0.03, IRON, up=(0, 1, 0))
        mb.blob((0.44, 0, 1.19), 0.035, sub=1, amp=0.0, col=IRON_LT)
    return mb


def make_pumpkin():
    mb = MB("SM_KG_Pumpkin", angle=40)
    s0 = len(mb.V)
    mb.lathe([(0, 0.03), (0.12, 0.0), (0.2, 0.04), (0.24, 0.12), (0.235, 0.21), (0.18, 0.28), (0.07, 0.31),
              (0.02, 0.29), (0, 0.285)], 24,
             colfn=lambda r, z, a, i: with_a(mix(ORANGE, C("C45A16"), abs(math.sin(4 * a)) ** 3 * 0.8), 0.0),
             sh=SMOOTH, rfn=lambda a, i, z: 1.0 - 0.07 * abs(math.sin(4 * a)) ** 0.7)
    mb.cavity(s0, 0.3, 0.15)
    stem = bezier((0, 0, 0.27), (0.0, 0, 0.35), (0.04, 0, 0.38), (0.08, 0.02, 0.36), 5)
    mb.tube(stem, [0.03, 0.026, 0.022, 0.02, 0.018, 0.016], 6, col=C("6B7A2E"), sh=AUTO)
    mb.card([(0.05, 0.02, 0.3), (0.2, 0.12, 0.33), (0.22, 0.02, 0.36), (0.12, -0.06, 0.32)], GREEN)
    vine = [(0.05 + 0.05 * math.cos(t), 0.08 + 0.05 * math.sin(t), 0.3 + 0.02 * t) for t in [k * 0.5 for k in range(13)]]
    mb.tube(vine, 0.005, 4, col=GREEN_DK)
    return mb


def make_cabbage():
    mb = MB("SM_KG_Cabbage", angle=40)
    cabbage(mb, random.Random(69), (0, 0, 0.15), 0.16)
    mb.floor = True
    return mb


def make_wheat_clump():
    mb = MB("SM_KG_WheatClump", angle=40, material="Sway")
    rng = random.Random(70)
    for k in range(22):
        a = rng.uniform(0, 6.28)
        r = rng.uniform(0.0, 0.09)
        base = Vector((r * math.cos(a), r * math.sin(a), 0))
        hgt = rng.uniform(0.85, 1.15)
        lean = Vector((math.cos(a), math.sin(a), 0)) * (0.1 + r * 1.5)
        droop = rng.uniform(0.03, 0.1)
        top = base + lean + Vector((0, 0, hgt))
        pts = bezier(base, base + Vector((0, 0, hgt * 0.5)), top - lean * 0.3 + Vector((0, 0, -0.05)),
                     top + lean.normalized() * droop - Vector((0, 0, droop * 0.6)), 5)
        mb.tube(pts, [0.006, 0.005, 0.005, 0.004, 0.004, 0.003], 3,
                colfn=lambda i, kk, p: with_a(mix(C("9CB34A"), HAY, i / 5), clamp(p.z / 1.1) ** 1.2))
        tip = pts[-1]
        d = (pts[-1] - pts[-2]).normalized()
        c = tip + d * 0.05
        z_ = d
        x_ = z_.orthogonal().normalized()
        y_ = z_.cross(x_)
        with mb.xf(frame(tip, x_, y_, z_)):
            mb.lathe([(0, 0), (0.012, 0.01), (0.016, 0.04), (0.014, 0.08), (0.008, 0.11), (0, 0.125)], 5,
                     colfn=lambda rr, zz, aa, i: with_a(mix(C("E9B84A"), C("D09A34"), (i % 2) * 0.6), clamp(tip.z / 1.1)),
                     sh=AUTO)
        if k % 2 == 0:
            lp = [base + Vector((0, 0, 0.1)), base + Vector((0, 0, 0.3)) + lean * 0.8,
                  base + Vector((0, 0, 0.35)) + lean * 2.2]
            side = Vector((-math.sin(a), math.cos(a), 0)) * 0.012
            grid = [[p - side, p + side] for p in lp]
            mb.cloth(grid, lambda i, j, p: with_a(C("8EAE44"), clamp(p.z / 1.1)), thick=0.003)
    return mb


# ================================================================================================ 13. animals
def make_sheep():
    mb = MB("SM_KG_Sheep", angle=40)
    rng = random.Random(71)
    wool = lambda p, u: with_a(mix(C("F4EEE0"), C("DCD2C0"), 0.5 - 0.5 * u.z + 0.2 * nz(p, 8.0)), 0.0)  # noqa: E731
    face = C("2E2A2A")
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.26, sy * 0.13
            mb.cyl((x, y, 0.0), (x, y, 0.5), 0.038, 0.045, seg=6, col=face)
            mb.cyl((x, y, 0.0), (x, y, 0.06), 0.042, seg=6, col=C("1A1818"))
    s0 = len(mb.V)
    mb.blob((0, 0, 0.64), (0.44, 0.28, 0.27), sub=2, amp=0.08, seed=2, colfn=wool)
    for k in range(18):
        u = rand_unit(rng)
        u.z = u.z * 0.8 + 0.15
        u.normalize()
        p = Vector((u.x * 0.42, u.y * 0.27, u.z * 0.26 + 0.64))
        mb.blob(p, rng.uniform(0.1, 0.15), sub=1, amp=0.2, seed=rng.random() * 30, colfn=wool)
    mb.cavity(s0, 0.25, 0.15)
    mb.blob((-0.47, 0, 0.72), 0.08, sub=1, amp=0.2, colfn=wool)
    hx, hz = 0.52, 0.8
    rot = Matrix.Rotation(math.radians(35), 3, 'Y')
    mb.blob((hx, 0, hz), (0.15, 0.09, 0.1), sub=2, amp=0.03, col=face, rot=rot)
    mb.blob((hx - 0.04, 0, hz + 0.1), (0.1, 0.1, 0.07), sub=1, amp=0.2, colfn=wool)
    for sy in (-1, 1):
        with mb.xf(T(hx - 0.06, sy * 0.1, hz + 0.02) @ R(sy * 30, 'X') @ R(10, 'Z')):
            mb.blob((0, sy * 0.05, 0), (0.035, 0.07, 0.018), sub=1, amp=0.0, col=face)
            mb.blob((0.003, sy * 0.055, 0.008), (0.022, 0.05, 0.01), sub=0, amp=0.0, col=C("C98A8A"))
        mb.blob((hx + 0.03, sy * 0.07, hz + 0.03), 0.022, sub=1, amp=0.0, col=WHITE)
        mb.blob((hx + 0.045, sy * 0.082, hz + 0.03), 0.012, sub=0, amp=0.0, col=BLACK)
    mb.blob((hx + 0.12, 0, hz - 0.07), (0.03, 0.045, 0.025), sub=1, amp=0.0, col=C("4A3E3E"))
    return mb


def make_chicken(name, body_c, tail_c):
    mb = MB(f"SM_KG_{name}", angle=40)
    rng = random.Random(72)
    s0 = len(mb.V)
    mb.blob((0, 0, 0.2), (0.14, 0.1, 0.1), sub=2, amp=0.05, seed=1,
            colfn=lambda p, u: with_a(mix(body_c, mul(body_c, 0.8), 0.5 - 0.5 * u.z), 0.0))
    mb.blob((0.07, 0, 0.23), (0.08, 0.075, 0.09), sub=1, amp=0.05, col=body_c)
    for k in range(5):
        a = math.radians(-40 + 20 * k)
        base = Vector((-0.1, 0, 0.23))
        tipv = base + Vector((-0.1 * math.cos(math.radians(50)), 0.07 * math.sin(a), 0.15 + 0.02 * math.cos(a * 2)))
        mb.card([base + Vector((0, -0.02, 0)), tipv + Vector((0, -0.015, 0)), tipv + Vector((0.02, 0.015, 0.02)),
                 base + Vector((0.02, 0.02, 0))], jit(tail_c, rng, 0.08), thick=0.006)
    for sy in (-1, 1):
        mb.blob((-0.01, sy * 0.09, 0.21), (0.085, 0.025, 0.055), sub=1, amp=0.05, col=mul(body_c, 0.88),
                rot=Matrix.Rotation(math.radians(-15), 3, 'Y'))
    hx, hz = 0.13, 0.34
    mb.blob((hx, 0, hz), 0.055, sub=1, amp=0.03, col=body_c)
    for k in range(3):
        mb.blob((hx - 0.03 + 0.025 * k, 0, hz + 0.055 + 0.01 * (k == 1)), (0.018, 0.01, 0.025), sub=0, amp=0.0, col=RED)
    mb.blob((hx + 0.045, 0, hz - 0.045), (0.012, 0.01, 0.022), sub=0, amp=0.0, col=RED)
    mb.cyl((hx + 0.045, 0, hz - 0.005), (hx + 0.09, 0, hz - 0.012), 0.016, 0.002, seg=5, col=C("F2A830"))
    for sy in (-1, 1):
        mb.blob((hx + 0.025, sy * 0.042, hz + 0.012), 0.01, sub=0, amp=0.0, col=BLACK)
        mb.cyl((0.0, sy * 0.04, 0.13), (0.01, sy * 0.045, 0.01), 0.008, seg=4, col=C("F2A830"))
        for a in (-35, 0, 35):
            d = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0))
            mb.beam((0.01, sy * 0.045, 0.006), Vector((0.01, sy * 0.045, 0.006)) + d * 0.045, 0.008, 0.008, C("F2A830"))
    mb.floor = True
    return mb


def make_cat_sleeping():
    mb = MB("SM_KG_Cat_Sleeping", angle=40)
    fur, fur_dk, white = C("E48A3A"), C("B85E22"), C("F6EEDC")
    n = 14
    pts, rad = [], []
    for i in range(n + 1):
        t = i / n
        a = math.radians(-120 + 270 * t)
        pts.append((0.13 * math.cos(a), 0.13 * math.sin(a), 0.075))
        rad.append(interp([(0, 0.06), (0.3, 0.085), (0.7, 0.08), (1.0, 0.06)], t))
    mb.sweep(pts, circle(10), rad, [r * 0.85 for r in rad], sh=SMOOTH,
             colfn=lambda i, k, p: with_a(white if math.sin(2 * math.pi * k / 10) < -0.6 else (fur_dk if i % 3 == 0 else fur), 0.0))
    hp = Vector(pts[-1]) + Vector((0.03, -0.04, 0.0))
    mb.blob(hp, (0.075, 0.07, 0.06), sub=2, amp=0.02, col=fur,
            colfn=lambda p, u: with_a(white if u.y < -0.55 and u.z < 0.2 else fur, 0.0))
    for s in (-1, 1):
        ex = hp + Vector((s * 0.04, -0.01, 0.045))
        mb.cyl(ex, ex + Vector((s * 0.015, 0.0, 0.05)), 0.025, 0.003, seg=4, col=fur_dk)
        mb.box((hp.x + s * 0.028, hp.y - 0.064, hp.z + 0.012), (0.025, 0.006, 0.005), BLACK)
    mb.blob(hp + Vector((0, -0.068, -0.012)), (0.012, 0.006, 0.008), sub=0, amp=0.0, col=C("E88A9A"))
    tail = []
    for i in range(10):
        t = i / 9
        a = math.radians(-125 - 150 * t)
        rr = 0.19 + 0.02 * t
        tail.append((rr * math.cos(a), rr * math.sin(a), 0.035 + 0.01 * math.sin(t * 3)))
    mb.tube(tail, [0.03 - 0.012 * i / 9 for i in range(10)], 6,
            colfn=lambda i, k, p: with_a(fur_dk if i % 2 == 0 else fur, 0.0))
    mb.floor = True
    return mb


def make_coop():
    mb = MB("SM_KG_Coop", angle=35)
    rng = random.Random(75)
    red, trim, roof = C("B8412E"), C("F2ECE0"), C("3F5F6B")
    W, D, Z0, Z1 = 1.3, 0.9, 0.55, 1.22
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.cbox((sx * (W / 2 - 0.05), sy * (D / 2 - 0.05), Z0 / 2), (0.08, 0.08, Z0), 0.01, WOOD_DK)
    mb.box((0, 0, Z0 + 0.025), (W + 0.04, D + 0.04, 0.05), WOOD_DK)
    nb = 11
    for side in range(2):
        s = -1 if side == 0 else 1
        for k in range(nb):
            x = -W / 2 + W * (k + 0.5) / nb
            mb.box((x, s * D / 2, (Z0 + Z1) / 2 + 0.025), (W / nb - 0.008, 0.03, Z1 - Z0 - 0.05), jit(red, rng, 0.05))
    for s in (-1, 1):
        for k in range(7):
            y = -D / 2 + D * (k + 0.5) / 7
            top = Z1 + 0.33 * (1 - abs(y) / (D / 2 + 0.2))
            mb.box((s * W / 2, y, (Z0 + top) / 2 + 0.025), (0.03, D / 7 - 0.008, top - Z0 - 0.05), jit(red, rng, 0.05))
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.box((sx * (W / 2 + 0.005), sy * (D / 2 + 0.005), (Z0 + Z1) / 2 + 0.02), (0.05, 0.05, Z1 - Z0), trim)
    for sy in (-1, 1):
        mb.box((0, sy * (D / 2 + 0.01), Z1 - 0.01), (W + 0.06, 0.04, 0.05), trim)
    # door opening + pop-hole on the front (-Y), ramp down to the ground
    with mb.xf(frame((-0.25, -D / 2 - 0.016, Z0 + 0.05), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        mb.ext2d([(-0.13, 0), (0.13, 0), (0.13, 0.22)] + [(0.13 * math.cos(math.radians(a)), 0.22 + 0.13 * math.sin(
            math.radians(a))) for a in range(30, 180, 30)] + [(-0.13, 0.22)], -0.02, 0.004, C("2A1E14"))
        mb.ext2d([(-0.16, -0.01), (0.16, -0.01), (0.16, 0.02), (-0.16, 0.02)], -0.02, 0.012, trim)
    rs, re = Vector((-0.25, -D / 2 - 0.02, Z0 + 0.04)), Vector((-0.25, -D / 2 - 0.75, 0.02))
    mb.beam(rs, re, 0.26, 0.03, WOOD, up=(0, 0, 1))
    for k in range(6):
        p = rs.lerp(re, (k + 0.5) / 6.5) + Vector((0, 0, 0.025))
        mb.beam(p - Vector((0.12, 0, 0)), p + Vector((0.12, 0, 0)), 0.025, 0.02, WOOD_DK, up=(0, 0, 1))
    with mb.xf(frame((0.3, -D / 2 - 0.016, Z0 + 0.4), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        mb.ext2d([(-0.12, -0.1), (0.12, -0.1), (0.12, 0.1), (-0.12, 0.1)], -0.02, 0.004, C("2A3A50"))
        mb.box((0, 0, 0.006), (0.02, 0.2, 0.01), trim)
        mb.box((0, 0, 0.006), (0.24, 0.02, 0.01), trim)
        for s in (-1, 1):
            mb.box((0, s * 0.11, 0.006), (0.28, 0.025, 0.014), trim)
            mb.box((s * 0.13, 0, 0.006), (0.025, 0.24, 0.014), trim)
    # nesting box bump-out on +X with a sloped lid
    mb.box((W / 2 + 0.17, 0, Z0 + 0.2), (0.34, 0.6, 0.34), red)
    with mb.xf(T(W / 2 + 0.17, 0, Z0 + 0.4) @ R(18, 'Y')):
        mb.cbox((0, 0, 0), (0.42, 0.66, 0.035), 0.008, roof)
    mb.box((W / 2 + 0.345, 0, Z0 + 0.2), (0.01, 0.62, 0.05), trim)
    # gable roof of shingle rows
    ridge = Z1 + 0.36
    for s in (-1, 1):
        for row in range(4):
            t0, t1 = row / 4, (row + 1) / 4 + 0.08
            y0, z0 = s * (D / 2 + 0.16) * (1 - t0), Z1 - 0.04 + (ridge - Z1 + 0.04) * t0
            y1, z1 = s * (D / 2 + 0.16) * (1 - t1), Z1 - 0.04 + (ridge - Z1 + 0.04) * t1
            mb.beam((0, y0, z0), (0, y1, z1), W + 0.3, 0.04, jit(roof if row % 2 else mul(roof, 1.15), rng, 0.04),
                    up=(0, 0, 1), bevel=0.008)
    mb.cbox((0, 0, ridge + 0.03), (W + 0.34, 0.1, 0.06), 0.01, mul(roof, 0.8))
    mb.blob((W / 2 + 0.1, 0, ridge + 0.1), 0.001, sub=0, amp=0.0, col=roof)
    # weather-vane rooster
    mb.cyl((0.45, 0, ridge + 0.05), (0.45, 0, ridge + 0.35), 0.008, seg=4, col=IRON)
    with mb.xf(frame((0.45, 0, ridge + 0.35), (1, 0, 0), (0, 0, 1), (0, -1, 0))):
        mb.ext2d([(-0.1, 0.0), (0.06, 0.0), (0.09, 0.05), (0.07, 0.1), (0.04, 0.08), (0.02, 0.04), (-0.05, 0.05),
                  (-0.1, 0.12), (-0.08, 0.03)], -0.006, 0.006, IRON)
    return mb


# ================================================================================================ registry
# name -> (builder, material, collision, preview view)
PROPS = [
    ("Fountain", make_fountain, "complex"),
    ("Maypole", make_maypole, "box"),
    ("Maypole_Ribbons", make_maypole_ribbons, "none"),
    ("Windmill_Body", make_windmill_body, "complex"),
    ("Windmill_Sails", make_windmill_sails, "none"),
    ("Stage", make_stage, "complex"),
    ("Bunting", make_bunting, "none"),
    ("LaundryLine", make_laundry_line, "complex"),
    ("FlowerBox", make_flower_box, "none"),
    ("Planter_Large", make_planter_large, "box"),
    ("Planter_Pot", make_planter_pot, "box"),
    ("HangingSign_Fish", lambda: make_sign("Fish"), "none"),
    ("HangingSign_Bread", lambda: make_sign("Bread"), "none"),
    ("HangingSign_Ale", lambda: make_sign("Ale"), "none"),
    ("HangingSign_Anvil", lambda: make_sign("Anvil"), "none"),
    ("HangingSign_Herb", lambda: make_sign("Herb"), "none"),
    ("SignBracket", make_sign_bracket, "none"),
    ("SignPost", make_sign_post, "complex"),
]

VIEWS = {
    "Windmill_Body": dict(el=12, az=35, with_={"SM_KG_Windmill_Sails": MILL_HUB}),
    "Windmill_Sails": dict(el=8, az=70),
    "Maypole": dict(el=12, az=-30, with_={"SM_KG_Maypole_Ribbons": (0, 0, 0)}),
    "SignPost": dict(el=12, az=-35, with_={"SM_KG_HangingSign_Ale": SIGN_HINGE_POST}),
    "SignBracket": dict(el=15, az=-40, with_={"SM_KG_HangingSign_Fish": SIGN_HINGE_BRACKET}),
    "Bunting": dict(el=10, az=-15),
}


# ================================================================================================ build + export
def build_all():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs, stats = [], []
    for name, fn, col in PROPS:
        full = "SM_KG_" + name
        if ONLY and full not in ONLY:
            continue
        mb = fn()
        assert mb.name == full, (mb.name, full)
        obj, tris, diag = finalize(mb)
        objs.append(obj)
        mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
        mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
        stats.append((full, tris, mn, mx, diag, mb.material, col, mb.info))
    print("\nKG_DV: object                        tris   size (cm)             min z   mat   col")
    for name, tris, mn, mx, diag, mat, col, info in stats:
        sz = (mx - mn) * 100
        print(f"KG_DV: {name:30s} {tris:6d}  {sz.x:5.0f} x {sz.y:5.0f} x {sz.z:5.0f}  min({mn.x * 100:6.0f},{mn.y * 100:6.0f},"
              f"{mn.z * 100:6.0f})  {mat:5s} {col:8s} {diag} {info if info else ''}")
    print(f"KG_DV: total tris {sum(s[1] for s in stats)} in {len(stats)} props")
    return objs, stats


def export(path, objs):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"KG_DV: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


def write_manifest(stats):
    data = {"props": {s[0].replace("SM_KG_", ""): {"material": s[5], "collision": s[6]} for s in stats}}
    with open(MANIFEST, "w") as fh:
        json.dump(data, fh, indent=1)
    print(f"KG_DV: manifest {MANIFEST} ({len(data['props'])} props)")


# ================================================================================================ preview
def _node_mat(name):
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    att = nt.nodes.new("ShaderNodeAttribute")
    att.attribute_name = "Col"
    nt.links.new(att.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    mat.use_backface_culling = True
    return mat


def _flat_mat(name, rgb_):
    mat = bpy.data.materials.new(name)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*rgb_, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat


def render_previews(objs, closeups=None, outdir=None):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 32
        sc.eevee.use_shadows = True
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x = sc.render.resolution_y = TILE
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("KG_PreviewWorld")
    sc.world = world
    bg = next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
    if bg:
        bg.inputs["Color"].default_value = (0.52, 0.68, 0.86, 1.0)
        bg.inputs["Strength"].default_value = 0.8
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(4)
    sun.data.color = (1.0, 0.95, 0.86)
    sc.collection.objects.link(sun)
    plain = _node_mat("P_Plain")
    for o in objs:
        o.data.materials[0] = plain
    gm = bpy.data.meshes.new("PreviewGround")
    gm.from_pydata([(-300, -300, 0), (300, -300, 0), (300, 300, 0), (-300, 300, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PreviewGround", gm)
    sc.collection.objects.link(ground)
    ground.data.materials.append(_flat_mat("P_Ground", (0.34, 0.38, 0.26)))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 50
    cam.data.sensor_width = 36
    cam.data.clip_start = 0.02
    cam.data.clip_end = 800
    sc.collection.objects.link(cam)
    sc.camera = cam
    lbl_curve = bpy.data.curves.new("Label", "FONT")
    lbl_curve.size = 0.03
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
    tmp = tempfile.mkdtemp(prefix="kg_dv_")
    by_name = {o.name: o for o in objs}

    def shoot(name, path, az=None, el=None, zoom=1.0):
        o = by_name[name]
        cfg = VIEWS.get(name.replace("SM_KG_", ""), {})
        extras = {k: v for k, v in cfg.get("with_", {}).items() if k in by_name}
        for x in objs:
            x.hide_render = not (x is o or x.name in extras)
            x.location = (0, 0, 0)
        for k, off in extras.items():
            by_name[k].location = off
        corners = []
        for x in [o] + [by_name[k] for k in extras]:
            corners += [x.matrix_world @ Vector(c) for c in x.bound_box]
        bpy.context.view_layer.update()
        corners = []
        for x in [o] + [by_name[k] for k in extras]:
            corners += [x.matrix_world @ Vector(c) for c in x.bound_box]
        zmin = min(c.z for c in corners)
        ground.location.z = min(0.0, zmin) - 0.01
        el = math.radians(cfg.get("el", 16) if el is None else el)
        az = math.radians(cfg.get("az", -35) if az is None else az)
        sun.rotation_euler = (math.radians(50), 0, az - math.radians(55))
        c = sum(corners, Vector()) / len(corners)
        d = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
        f = -d
        right = f.cross(Vector((0, 0, 1))).normalized()
        up = right.cross(f)
        dist = max(max(abs((p - c).dot(right)), abs((p - c).dot(up))) / tan_h - (p - c).dot(f) for p in corners)
        dist = dist * 1.06 / zoom + 0.05
        cam.location = c - f * dist
        cam.rotation_euler = f.to_track_quat("-Z", "Y").to_euler()
        lbl.data.body = name.replace("SM_KG_", "")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)

    if closeups:
        os.makedirs(outdir, exist_ok=True)
        for name in closeups:
            full = name if name.startswith("SM_KG_") else "SM_KG_" + name
            if full not in by_name:
                continue
            cfg = VIEWS.get(full.replace("SM_KG_", ""), {})
            for tag, azo in (("a", None), ("b", cfg.get("az", -35) + 150)):
                p = os.path.join(outdir, f"DV_{full.replace('SM_KG_', '')}_{tag}.png")
                shoot(full, p, az=azo)
                print(f"KG_DV: closeup {p}")
        return

    os.makedirs(PREVIEW_DIR, exist_ok=True)
    names = [o.name for o in objs]
    per = 12
    for si in range(0, len(names), per):
        chunk = names[si:si + per]
        cols = 4
        rows = (len(chunk) + cols - 1) // cols
        sheet = np.zeros((rows * TILE, cols * TILE, 4), np.float32)
        sheet[..., 3] = 1.0
        for k, name in enumerate(chunk):
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
        path = os.path.join(PREVIEW_DIR, f"{PREFIX}_{si // per + 1}.png")
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        print(f"KG_DV: preview {path}")
    shutil.rmtree(tmp, ignore_errors=True)


def verify(path, stats):
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
        amin = amax = -1.0
        if ca:
            arr = np.empty(len(ca.data) * 4, np.float32)
            ca.data.foreach_get("color", arr)
            a = arr[3::4]
            amin, amax = float(a.min()), float(a.max())
        good = o.name in expect and ca is not None and len(me.materials) > 0
        ok &= good
        print(f"KG_DV_VERIFY: {o.name:30s} tris {tris:6d} (built {expect.get(o.name)})  color={ca.name if ca else None}"
              f"  alpha [{amin:.2f}, {amax:.2f}]  mat={me.materials[0].name if me.materials else None}")
    missing = set(expect) - {o.name for o in bpy.data.objects}
    if missing:
        ok = False
        print(f"KG_DV_VERIFY: MISSING {sorted(missing)}")
    print(f"KG_DV_VERIFY: {'OK' if ok else 'FAILED'}")


def main():
    objs, stats = build_all()
    if "--no-export" not in _opt:
        export(OUT_GLB, objs)
        write_manifest(stats)
    if "--closeup" in _opt:
        render_previews(objs, closeups=_opt["--closeup"].split(","), outdir=_opt.get("--outdir", PREVIEW_DIR))
    elif "--no-preview" not in _opt:
        render_previews(objs)
    if "--no-verify" not in _opt and "--no-export" not in _opt:
        verify(OUT_GLB, stats)


main()
