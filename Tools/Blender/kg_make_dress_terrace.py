"""Procedural terrace / harbour-stonework dressing props (pack KG_DressTerrace). Headless Blender 5.2, no textures.
Stylised Quaternius-like low poly: chunky, slightly bevelled, flat-shaded, warm saturated palette, real-world scale.
Helpers, palette and conventions are copied from kg_make_dress_village.py (not importable: it runs main() on import).

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_terrace.py -- \
      [out.glb] [preview_dir] [--only A,B] [--no-preview] [--no-verify] [--no-export] [--tile 512]
      [--manifest path.json] [--prefix DressTerrace_preview]
  blender --background --factory-startup --python Tools/Blender/kg_make_dress_terrace.py -- --verify-clean in.glb

Output: Art/Packed/KG_DressTerrace.glb (one mesh object per prop, SM_KG_<Name>, metres, Z up), the import manifest
Art/Packed/KG_DressTerrace_Clean.json and contact sheets Art/Concept/DressTerrace_preview.png / _2.png.
Then: kg_sanitize_glb.py -- Art/Packed/KG_DressTerrace.glb Art/Packed/KG_DressTerrace_Clean.glb

Vertex colour attribute "Col" (FLOAT_COLOR, face corner, exported as COLOR_0): RGB = LINEAR albedo, A = mask:
  Glow material (M_KG_JapanGlow): 1 on emissive faces (HarbourLight lantern glass), 0 elsewhere
  VC material (M_KG_PropVCLinear): 0
Pivots (object origin, Blender axes):
  Balustrade_2m, Balustrade_Post, HarbourLight : bottom centre
  QuayWall_4m(_Ring)  : top of the sea face, centre (sea face = plane Y=0 facing -Y, coping top Z=0, body to Y=+0.9)
  QuaySteps           : top nosing of the flight on the wall face (X=0, Y=0, Z=0 = quay top); flight runs +X / down
  StoneArchBridge_7m  : deck centre line at road level (walking surface Z=0 at both ends, +0.3 at the crown)
  ClockFace           : dial centre (face points -Y, flat back at Y=0)
"""
import colorsys
import json
import math
import os
import random
import shutil
import struct
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
OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_DressTerrace.glb"
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"
TILE = int(_opt.get("--tile", 512))
ONLY = [s if s.startswith("SM_KG_") else "SM_KG_" + s for s in _opt["--only"].split(",")] if "--only" in _opt else None
MANIFEST = _opt.get("--manifest", f"{ROOT}/Art/Packed/KG_DressTerrace_Clean.json")
PREFIX = _opt.get("--prefix", "DressTerrace_preview")
VILLAGE_CLEAN = f"{ROOT}/Art/Packed/KG_DressVillage_Clean.glb"

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


def newell(pts):
    n = Vector((0.0, 0.0, 0.0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    return n


def area2(poly):
    n = len(poly)
    return sum(poly[k][0] * poly[(k + 1) % n][1] - poly[(k + 1) % n][0] * poly[k][1] for k in range(n)) * 0.5


def inset2(poly, b):
    """Miter-offset a CCW 2D polygon inward by b (clamped for sharp corners)."""
    n = len(poly)
    out = []
    for k in range(n):
        a, p, c = poly[k - 1], poly[k], poly[(k + 1) % n]
        e1 = (p[0] - a[0], p[1] - a[1])
        e2 = (c[0] - p[0], c[1] - p[1])
        l1 = math.hypot(*e1) or 1e-9
        l2 = math.hypot(*e2) or 1e-9
        n1 = (e1[1] / l1, -e1[0] / l1)
        n2 = (e2[1] / l2, -e2[0] / l2)
        d = max(1.0 + n1[0] * n2[0] + n1[1] * n2[1], 0.35)
        out.append((p[0] - b * (n1[0] + n2[0]) / d, p[1] - b * (n1[1] + n2[1]) / d))
    return out


def hsub(a0, a1, z, maxseg=0.35):
    """Points along a horizontal edge (a0, z) -> (a1, z), segments <= maxseg (so deformations stay smooth)."""
    n = max(1, int(math.ceil(abs(a1 - a0) / maxseg - 1e-6)))
    return [(a0 + (a1 - a0) * k / n, z) for k in range(n + 1)]


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


def jit(c, rng, amt=0.08):
    k = 1.0 + rng.uniform(-amt, amt)
    return (c[0] * k, c[1] * k, c[2] * k, c[3])


# palette (sRGB hex -> linear); shared names/values with kg_make_dress_village.py
IRON, IRON_LT = C("35343A"), C("5A5860")
STONE, STONE_LT, STONE_DK = C("B5AC9E"), C("D8CFBF"), C("877E72")
SAND, SAND_LT = C("E0CDA6"), C("EFE2C6")
RED, RED_DK, WHITE, CREAM, GOLD = C("DA3F2C"), C("A82D22"), C("F5F0E6"), C("F0E2C0"), C("E2B03C")
MOSS, BLACK = C("7D9A45"), C("22212A")
ALGAE, ALGAE_LT, ALGAE_DK = C("5A8A38"), C("79A83F"), C("2F4A26")          # as kg_make_dress_harbour.py
BARN = C("DAD4C4")
RUST = C("8C4A26")
VERD, VERD_LT, VERD_DK = C("4FA88C"), C("7CC7A8"), C("357A66")               # copper verdigris
GLASS = C("C8F2D2", a=1.0)                                                   # pale green lantern glass, emissive

# stone families (warm beige-grey, same family as the village kit's STONE / SAND and SM_KG_Fountain)
TERRACE = [C("DCCFB4"), C("E2D5BA"), C("D2C5AB"), C("D8CDB8"), C("CFC3AD")]
GRANITE = [C("CBBA9C"), C("D2C2A3"), C("BEAD92"), C("CDBC9B"), C("B8A88D"), C("D6C6A7")]
BRIDGE = [C("D0BD9A"), C("D8C7A5"), C("C4B18F"), C("CFBFA0"), C("BFAD8B"), C("DBCBA9")]
VOUSS = [C("EBDBBA"), C("E5D4B1"), C("F0E2C4"), C("E1D1AF")]
FLAGS = [C("B0A088"), C("A4957D"), C("BCAC92"), C("998B75"), C("B3A48A")]


def pick(rng, fam, amt=0.06):
    return jit(rng.choice(fam), rng, amt)


def stone_noise(seed=0.0, amt=0.06, f=3.0):
    return lambda p, c: mul(c, 1.0 + amt * nz(p, f, (seed, 1.7, 3.1)))


def sea_tone(c, z, p, seed=0.0):
    """Quay waterline banding (waterline Z=-2.0): dry > -1.35, damp -1.35..-1.9, algae -1.9..-2.5, dark wet below."""
    if z > -1.35:
        return c
    if z > -1.9:
        return mul(c, 1.0 - 0.24 * smoothstep(-1.35, -1.9, z))
    if z >= -2.5:
        t = nz(p, 3.2, (seed, 4.0, 1.0))
        alg = mix(ALGAE_LT, ALGAE, clamp(0.25 + 0.5 * t + 0.6 * smoothstep(-1.95, -2.45, z)))
        alg = mix(alg, ALGAE_DK, 0.35 * smoothstep(-2.2, -2.5, z))
        return with_a(mix(mul(c, 0.55), alg, 0.88), 0.0)
    return with_a(mix(mul(c, 0.4), ALGAE_DK, 0.28 + 0.12 * nz(p, 2.5, (seed, 9.0, 2.0))), 0.0)


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
    """Accumulates one mesh (copied from kg_make_dress_village.py, plus `prism`): vertices with colours, faces with
    optional flat colour override and shading mode. FLAT faces are faceted, SMOOTH always smooth, AUTO edges turn
    sharp above `angle` degrees. Closed islands get their normals recalculated outward on finalize (safety net)."""

    def __init__(self, name, angle=38.0, material="VC", pivot="keep"):
        self.name, self.angle, self.material, self.pivot = name, angle, material, pivot
        self.V, self.VC, self.F, self.FC, self.FS = [], [], [], [], []
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
        y.normalize()
        z = x.cross(y)
        with self.xf(frame((p0 + p1) * 0.5, x, y, z)):
            if bevel > 0:
                return self.cbox((0, 0, 0), (L, w, h), bevel, col, sh)
            s = len(self.V)
            self.box((0, 0, 0), (L, w, h), col, sh)
            return s

    def lathe(self, prof, seg, col=None, colfn=None, segcol=None, sh=SMOOTH, phase=0.0, closed=False, facefn=None):
        """Surface of revolution around local Z. prof: [(r, z)] traced bottom-axis -> outside -> top-axis
        (outward normals, CCW in the (r, z) plane). r == 0 end points collapse to one apex vertex."""
        start = len(self.V)
        rings = []
        for i, (r, z) in enumerate(prof):
            if r < 1e-6:
                rings.append(self.v((0, 0, z), colfn(0.0, z, 0.0, i) if colfn else col))
            else:
                ring = []
                for j in range(seg):
                    a = phase + 2 * math.pi * j / seg
                    ring.append(self.v((r * math.cos(a), r * math.sin(a), z), colfn(r, z, a, i) if colfn else col))
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
        return start

    def blob(self, c, r, sub=1, amp=0.15, freq=1.6, seed=0.0, col=None, sh=SMOOTH):
        V, F = icosphere(sub)
        c = Vector(c)
        rx, ry, rz = r if isinstance(r, (tuple, list)) else (r, r, r)
        off = Vector((seed * 13.1, seed * 7.7, seed * 3.3))
        ids = []
        for u in V:
            d = 1.0 + amp * noise.noise(u * freq + off)
            ids.append(self.v(c + Vector((u.x * rx * d, u.y * ry * d, u.z * rz * d)), col))
        for a, b, cc in F:
            self.f((ids[a], ids[b], ids[cc]), None, sh)
        return ids

    def cyl(self, p0, p1, r0, r1=None, seg=8, col=None, sh=AUTO, caps=True, phase=0.0, capcol=None):
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
            return self.lathe(prof, seg, col=col, sh=sh, phase=phase, segcol=segcol)

    def torus(self, R_, r, seg, sides, col=None, sh=SMOOTH, phase=0.0):
        prof = [(R_ + r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
        return self.lathe(prof, seg, col=col, sh=sh, closed=True, phase=phase)

    def ext2d(self, pts, w0, w1, col, side=None, sh=AUTO, topcol=None):
        """Extrude a 2D polygon (local XY, may be concave) between local z=w0 and z=w1 (+Z face on top)."""
        pts = [tuple(p) for p in pts]
        if area2(pts) < 0:
            pts = pts[::-1]
        n = len(pts)
        b = [self.v((x, y, w0), col) for x, y in pts]
        t = [self.v((x, y, w1), col) for x, y in pts]
        self.f(t, topcol, FLAT)
        self.f(list(reversed(b)), topcol, FLAT)
        for k in range(n):
            kn = (k + 1) % n
            self.f((b[k], b[kn], t[kn], t[k]), side, sh)

    def prism(self, poly, d0, d1, col, b0=0.0, b1=0.0, sh=FLAT):
        """Extrude a 2D polygon (local XY, may be mildly concave) along local Z from d0 to d1, with an optional
        chamfer b0 / b1 around the d0 / d1 cap (the cap is inset by b and pushed in by b). Returns first vertex."""
        poly = [tuple(p) for p in poly]
        if area2(poly) < 0:
            poly = poly[::-1]
        n = len(poly)
        rings = []
        if b0 > 0:
            rings += [(inset2(poly, b0), d0), (poly, d0 + b0)]
        else:
            rings.append((poly, d0))
        if b1 > 0:
            rings += [(poly, d1 - b1), (inset2(poly, b1), d1)]
        else:
            rings.append((poly, d1))
        start = len(self.V)
        ids = [[self.v((x, y, w), col) for x, y in pts] for pts, w in rings]
        self.f(list(reversed(ids[0])), None, sh)
        self.f(ids[-1], None, sh)
        for i in range(len(ids) - 1):
            A, B = ids[i], ids[i + 1]
            for k in range(n):
                k1 = (k + 1) % n
                self.f((A[k], A[k1], B[k1], B[k]), None, sh)
        return start


# ------------------------------------------------------------------------------------------------ finalize
MATS = {}
MAT_NAME = {"VC": "M_KG_DT_VC", "Glow": "M_KG_DT_Glow"}


def recenter(mb):
    if mb.pivot == "keep":
        return
    mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
    mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
    off = Vector(((mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5, mn.z)) if mb.pivot == "bottom" else Vector((0, 0, mn.z))
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
    loose = sum(1 for v in bm.verts if not v.link_faces)
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
    diag = dict(closed=closed_n, open=open_n, flipped=flipped, degenerate=degenerate, loose=loose,
                validate_changed=bool(changed))
    return obj, tris, diag


# ================================================================================================ shared bits
def course_joints(rng, x0, x1, prev, lo, hi, first=(0.3, 0.95), min_end=0.3, clear=0.2):
    """Staggered vertical joint positions for one course between x0 and x1 (avoid joints of the course above)."""
    js = []
    x = x0 + rng.uniform(*first)
    while x1 - x > min_end:
        for _ in range(10):
            if all(abs(x - p) > clear for p in prev):
                break
            x += 0.11
        if x1 - x <= min_end:
            break
        js.append(x)
        x += rng.uniform(lo, hi)
    return js


XZ_FRONT_NEG_Y = frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))   # local (u, v, w) = (X, Z, -Y)
YZ_ALONG_X = frame((0, 0, 0), (0, 1, 0), (0, 0, 1), (1, 0, 0))       # local (u, v, w) = (Y, Z, X)
DIAL = XZ_FRONT_NEG_Y                                                  # local z = proud distance towards -Y


# ================================================================================================ 1. balustrade
BAL_PROF = [(0, 0), (0.068, 0), (0.068, 0.022), (0.054, 0.04), (0.06, 0.062), (0.085, 0.12), (0.099, 0.185),
            (0.095, 0.245), (0.077, 0.305), (0.052, 0.37), (0.041, 0.42), (0.043, 0.448), (0.058, 0.475),
            (0.063, 0.505), (0.07, 0.53), (0.07, 0.56), (0, 0.56)]


def make_balustrade():
    mb = MB("SM_KG_Balustrade_2m", angle=30)
    rng = random.Random(11)
    # plinth: two 1 m stones, 0.18 tall
    for k, x in enumerate((-0.5, 0.5)):
        s0 = mb.cbox((x, 0, 0.09), (0.996, 0.32, 0.18), 0.025, pick(rng, TERRACE))
        mb.recolor(s0, stone_noise(k * 3.0))
    # five turned balusters, 0.4 m apart (sections chain at the same spacing)
    for k, x in enumerate((-0.8, -0.4, 0.0, 0.4, 0.8)):
        col = pick(rng, TERRACE, 0.05)
        mb.cbox((x, 0, 0.21), (0.17, 0.17, 0.06), 0.015, col)
        mb.cbox((x, 0, 0.83), (0.17, 0.17, 0.06), 0.015, col)
        with mb.xf(T(x, 0, 0.24)):
            s0 = mb.lathe(BAL_PROF, 10, col=col, sh=AUTO, phase=math.pi / 10)
        mb.recolor(s0, stone_noise(10.0 + k, 0.05, 6.0))
    # top rail: 0.14 tall, 0.35 wide, overhanging the balusters/plinth, bevelled top; two 1 m stones
    prof = [(-0.15, 0.86), (0.15, 0.86), (0.15, 0.878), (0.175, 0.895), (0.175, 0.962), (0.137, 1.0),
            (-0.137, 1.0), (-0.175, 0.962), (-0.175, 0.895), (-0.15, 0.878)]
    for k, (x0, x1) in enumerate(((-1.0, 0.0), (0.0, 1.0))):
        with mb.xf(YZ_ALONG_X):
            s0 = mb.prism(prof, x0 + 0.002, x1 - 0.002, pick(rng, TERRACE, 0.04), b0=0.012, b1=0.012, sh=FLAT)
        mb.recolor(s0, stone_noise(20.0 + k))
    return mb


def make_post():
    mb = MB("SM_KG_Balustrade_Post", angle=30)
    rng = random.Random(12)
    col = pick(rng, TERRACE, 0.03)
    mb.recolor(mb.cbox((0, 0, 0.11), (0.44, 0.44, 0.22), 0.03, col), stone_noise(1.0))
    mb.recolor(mb.cbox((0, 0, 0.235), (0.4, 0.4, 0.03), 0.01, mul(col, 1.03)), stone_noise(1.5))
    shaft = pick(rng, TERRACE, 0.03)
    mb.recolor(mb.cbox((0, 0, 0.55), (0.36, 0.36, 0.6), 0.02, shaft), stone_noise(2.0))
    panel = mul(shaft, 0.9)
    for k in range(4):
        with mb.xf(R(90 * k, 'Z')):
            mb.cbox((0, -0.18, 0.55), (0.22, 0.03, 0.42), 0.012, panel)
            mb.cbox((0, -0.19, 0.55), (0.16, 0.02, 0.34), 0.008, mul(shaft, 1.04))
    mb.recolor(mb.cbox((0, 0, 0.87), (0.4, 0.4, 0.04), 0.012, mul(col, 1.02)), stone_noise(3.0))
    cap = pick(rng, TERRACE, 0.03)
    mb.recolor(mb.cbox((0, 0, 0.935), (0.44, 0.44, 0.09), 0.03, cap), stone_noise(4.0))
    # finial: neck + ball, top at 1.15
    prof = [(0, 0.98), (0.09, 0.98), (0.09, 1.0), (0.055, 1.018), (0.036, 1.04)]
    zc, rb = 1.095, 0.055
    for d in (-50, -20, 10, 40, 70):
        a = math.radians(d)
        prof.append((rb * math.cos(a), zc + rb * math.sin(a)))
    prof.append((0, 1.15))
    s0 = mb.lathe(prof, 10, col=cap, sh=AUTO, phase=math.pi / 10)
    mb.recolor(s0, stone_noise(5.0, 0.04, 8.0))
    return mb


# ================================================================================================ 3/4. quay wall
QUAY_Z = [-0.3, -0.82, -1.36, -1.9, -2.5, -3.0, -3.5, -4.0, -4.5, -5.0, -5.5, -6.0]


def quay_block(mb, rng, poly, z0, z1, back, front_w, seed, bev=0.035, fam=GRANITE):
    with mb.xf(XZ_FRONT_NEG_Y):
        s0 = mb.prism(poly, -back, front_w, pick(rng, fam), b1=bev)
    m = min(0.12, (z1 - z0) * 0.3)
    mb.recolor(s0, lambda p, c: sea_tone(mul(c, 1.0 + 0.06 * nz(p, 3.0, (seed, 1, 2))),
                                         clamp(p.z, z0 + m, z1 - m), p, seed))


def weed(mb, rng, x, z, y=-0.015, big=1.0):
    mb.blob((x, y, z), (0.13 * big, 0.045, 0.16 * big), sub=0, amp=0.25, seed=rng.random() * 50,
            col=jit(mix(ALGAE, ALGAE_DK, rng.uniform(0.1, 0.6)), rng, 0.1), sh=FLAT)


def barnacles(mb, rng, x, z, y=-0.005, n=3):
    for _ in range(n):
        mb.blob((x + rng.uniform(-0.12, 0.12), y, z + rng.uniform(-0.1, 0.1)), rng.uniform(0.018, 0.03), sub=0,
                amp=0.1, col=jit(BARN, rng, 0.08), sh=FLAT)


def make_quay_wall(ring=False):
    mb = MB("SM_KG_QuayWall_4m_Ring" if ring else "SM_KG_QuayWall_4m", angle=30)
    rng = random.Random(41)          # same seed for both: the ring variant is the same wall
    prev = []
    for ci in range(len(QUAY_Z) - 1):
        z1, z0 = QUAY_Z[ci], QUAY_Z[ci + 1]
        js = course_joints(rng, -2.0, 2.0, prev, 0.75, 1.3)
        edges = [-2.0] + js + [2.0]
        for a, b in zip(edges, edges[1:]):
            g = 0.002
            poly = [(a + g, z0 + g), (b - g, z0 + g), (b - g, z1 - g), (a + g, z1 - g)]
            quay_block(mb, rng, poly, z0, z1, 0.9, -rng.uniform(0.0, 0.012), rng.random() * 20)
        prev = js
    # rounded granite coping, 0.3 tall, 0.08 overhang on the sea side, top at Z=0
    prof = [(-0.08, -0.28), (-0.06, -0.3), (0.9, -0.3), (0.9, -0.025), (0.875, 0.0), (0.02, 0.0)]
    for d in (22.5, 45, 67.5, 90):
        a = math.radians(d)
        prof.append((0.02 - 0.1 * math.sin(a), -0.1 + 0.1 * math.cos(a)))
    for k, (x0, x1) in enumerate(((-2.0, -1.0), (-1.0, 0.0), (0.0, 1.0), (1.0, 2.0))):
        with mb.xf(YZ_ALONG_X):
            s0 = mb.prism(prof, x0 + 0.002, x1 - 0.002, pick(rng, GRANITE, 0.05), b0=0.02, b1=0.02, sh=AUTO)
        mb.recolor(s0, lambda p, c: mul(mix(c, STONE_LT, 0.2), 1.0 + 0.05 * nz(p, 3.0, (k, 5, 1))))
    # weed and barnacles in the algae band
    for k in range(9):
        x = -1.85 + 3.7 * (k + rng.uniform(0.15, 0.85)) / 9
        weed(mb, rng, x, rng.uniform(-2.0, -2.35), big=rng.uniform(0.7, 1.2))
    for k in range(6):
        barnacles(mb, rng, rng.uniform(-1.8, 1.8), rng.uniform(-2.1, -2.7))
    if ring:
        mooring_ring(mb, (0.0, 0.0, -0.7))
    return mb


def mooring_ring(mb, at, rust=0.22):
    x, y, z = at
    iron = mix(IRON, RUST, rust)
    mb.cbox((x, y - 0.016, z), (0.26, 0.032, 0.3), 0.01, mix(IRON, RUST, rust + 0.15))
    for sx in (-1, 1):                                            # bolts
        for sz in (-1, 1):
            mb.cyl((x + sx * 0.09, y - 0.03, z + sz * 0.11), (x + sx * 0.09, y - 0.045, z + sz * 0.11), 0.018,
                   seg=6, col=iron)
    # eye (staple) and the ring hanging from it, tilted a little off the wall
    for sx in (-1, 1):
        mb.cbox((x + sx * 0.045, y - 0.05, z + 0.04), (0.03, 0.05, 0.035), 0.006, iron)
    mb.cyl((x - 0.07, y - 0.075, z + 0.04), (x + 0.07, y - 0.075, z + 0.04), 0.017, seg=8, col=iron)
    with mb.xf(T(x, y - 0.075, z + 0.04) @ R(12, 'X') @ T(0, 0, -0.15) @ R(90, 'X')):
        mb.torus(0.15, 0.022, 16, 6, col=iron, sh=SMOOTH)


# ================================================================================================ 5. quay steps
def make_quay_steps():
    """12 steps (0.25 rise, 0.35 going) along +X: tread k (1..12) is the flat top Z=-0.25k over X 0.35(k-1)..0.35k;
    the quay top (Z=0) is the landing above tread 1. Built as 0.25 m courses: course k spans X 0..0.35k."""
    mb = MB("SM_KG_QuaySteps", angle=30)
    rng = random.Random(52)
    prev = []
    y0, y1 = -1.3, 0.0
    courses = [(-0.25 * (k + 1), -0.25 * k, 0.35 * k) for k in range(1, 13)] + [(-3.5, -3.25, 4.2)]
    for ci, (z0, z1, xmax) in enumerate(courses):
        tread_x = xmax - 0.35 if ci < 12 else xmax      # joints must stay off the exposed tread
        js = [j for j in course_joints(rng, 0.0, xmax, prev, 0.6, 1.1, first=(0.35, 0.9), min_end=0.3)
              if j <= tread_x + 1e-6]
        edges = [0.0] + js + [xmax]
        for a, b in zip(edges, edges[1:]):
            g = 0.002
            s0 = mb.cbox(((a + b) / 2, (y0 + y1) / 2, (z0 + 0.004 + z1) / 2), (b - a - 2 * g, (y1 - y0) - 2 * g,
                                                                             z1 - z0 - 0.004), 0.03,
                         pick(rng, GRANITE), sh=FLAT)
            seed = rng.random() * 20
            mb.recolor(s0, lambda p, c, z0=z0, z1=z1, seed=seed: sea_tone(
                mul(c, 1.0 + 0.06 * nz(p, 3.0, (seed, 1, 2))), clamp(p.z, z0 + 0.05, z1 - 0.05), p, seed))
        prev = js
    # low stepped kerb on the open (-Y) edge, one stone per tread
    for k in range(1, 13):
        zt = -0.25 * k
        s0 = mb.cbox((0.35 * k - 0.175, -1.22, zt + 0.07), (0.346, 0.16, 0.14), 0.025,
                     mix(pick(rng, GRANITE), STONE_LT, 0.25), sh=FLAT)
        mb.recolor(s0, lambda p, c, zt=zt: sea_tone(c, clamp(p.z, zt + 0.02, zt + 0.12), p, 3.0))
    # algae / barnacles on the side face (Y=-1.3) and a weed or two on the end
    def top(x):                                                    # tread height above X
        return -0.25 * max(1, math.ceil(x / 0.35 - 1e-6))
    for k in range(6):
        x = rng.uniform(2.6, 4.05)
        with mb.xf(T(x, -1.3, 0) @ R(180, 'Z')):
            weed(mb, rng, 0.0, min(rng.uniform(-2.0, -2.45), top(x) - 0.25), y=-0.05, big=rng.uniform(0.6, 1.0))
    for k in range(4):
        x = rng.uniform(3.0, 4.05)
        with mb.xf(T(x, -1.3, 0) @ R(180, 'Z')):
            barnacles(mb, rng, 0.0, min(rng.uniform(-2.6, -3.2), top(x) - 0.2), y=-0.03)
    return mb


# ================================================================================================ 6. arch bridge
BR_RI = (2.5 ** 2 + 1.75 ** 2) / (2 * 1.75)       # segmental arch: chord 5.0, rise 1.75 (springing -2.4, crown -0.65)
BR_CZ = -0.65 - BR_RI
BR_RE = BR_RI + 0.35                              # voussoir ring depth 0.35 -> extrados crown -0.30
BR_KD = math.asin(0.3 / BR_RE)                    # keystone half-angle (0.6 wide at the extrados)
BR_KA = (-0.2 - BR_CZ) * math.tan(BR_KD)          # keystone half-width at its top (Z=-0.2, under the string course)
BR_TS = math.asin(2.5 / BR_RI)                    # springing angle from vertical
BR_XSK, BR_ZSK = BR_RE * math.sin(BR_TS), BR_CZ + BR_RE * math.cos(BR_TS)
BR_FACE, BR_PROUD, BR_DEPTH = 2.35, 0.05, 0.5     # outer wall face |y|, ring/string-course proud, face-stone depth


def br_hump(x):
    return 0.15 * (1.0 + math.cos(2 * math.pi * x / 9.0)) if abs(x) <= 4.5 else 0.0


def br_zbase(a):
    return BR_CZ + math.sqrt(max(BR_RE * BR_RE - a * a, 0.0)) if a < BR_XSK else BR_ZSK


def br_deform(p):
    """Built flat, then: everything from Z=-0.2 up rides the deck hump; spandrels stretch between the ring's
    extrados (fixed) and Z=-0.2; wing walls splay outward towards the bottom of the bridge ends."""
    x, y, z = p.x, p.y, p.z
    a = abs(x)
    h = br_hump(x)
    if z >= -0.2 - 1e-6:
        z2 = z + h
    else:
        zb = br_zbase(a)
        z2 = z + (h * clamp((z - zb) / (-0.2 - zb)) if z > zb else 0.0)
    if abs(y) > 1.5 and z < -0.2:
        y += math.copysign(0.35 * smoothstep(3.2, 4.5, a) * clamp((-0.2 - z) / 3.0), y)
    return (x, y, z2)


def br_outline():
    """Inner boundary of the side walls in (a=|x|, z), ordered by decreasing z: keystone side, ring extrados,
    skewback, pier face."""
    pts = [(BR_KA, 1.0), (BR_KA, -0.2)]
    th0 = BR_KD
    n = 14
    for i in range(n + 1):
        th = th0 + (BR_TS - th0) * i / n
        pts.append((BR_RE * math.sin(th), BR_CZ + BR_RE * math.cos(th)))
    pts += [(2.5, -2.4), (2.5, -4.0)]
    return pts


def seg_between(P, z1, z0):
    def at(z):
        for (a0, za), (a1, zb) in zip(P, P[1:]):
            if za >= z >= zb:
                return a0 if za == zb else lerp(a0, a1, (za - z) / (za - zb))
        return P[-1][0]
    out = [(at(z1), z1)]
    out += [(a, z) for a, z in P if z0 + 1e-4 < z < z1 - 1e-4]
    out.append((at(z0), z0))
    return out


def br_face_block(mb, rng, poly_az, sx, sy, fam, seed, front=BR_FACE, depth=BR_DEPTH, bev=0.035, moss=True):
    pts = [(sx * a, z) for a, z in poly_az]
    poly = inset2(pts if area2(pts) > 0 else pts[::-1], 0.002)
    rec = rng.uniform(0.0, 0.01)
    with mb.xf(XZ_FRONT_NEG_Y):                  # w = -Y
        if sy > 0:
            s0 = mb.prism(poly, -(front - rec), -(front - depth), pick(rng, fam), b0=bev)
        else:
            s0 = mb.prism(poly, front - depth, front - rec, pick(rng, fam), b1=bev)

    def col(p, c):
        c = mul(c, 1.0 + 0.06 * nz(p, 3.0, (seed, 2, 1)))
        if moss:
            m = smoothstep(-2.5, -3.2, p.z) * clamp(0.3 + 0.7 * nz(p, 2.5, (seed, 7, 3)))
            c = mix(c, MOSS, 0.55 * m)
        return with_a(c, 0.0)
    mb.recolor(s0, col)


def make_bridge():
    mb = MB("SM_KG_StoneArchBridge_7m", angle=30)
    rng = random.Random(63)
    P = br_outline()
    # ---- side walls (spandrels, abutment faces, wing walls) as ashlar courses cut by the ring / pier outline
    zs = [-0.2, -0.6, -1.05, -1.5, -1.95, -2.4, -2.8, -3.2]
    for sy in (1, -1):
        prev = {1: [], -1: []}
        for ci in range(len(zs) - 1):
            z1, z0 = zs[ci], zs[ci + 1]
            bnd = seg_between(P, z1, z0)
            ms = 0.45 if z0 >= -1.1 else 5.0
            amax = max(a for a, z in bnd)
            for sx in (1, -1):
                js = course_joints(rng, amax, 4.5, prev[sx], 0.65, 1.1, first=(0.3, 0.65), min_end=0.35)
                prev[sx] = js
                edges = js + [4.5]
                e_prev = None
                for bi, e in enumerate(edges):
                    if bi == 0:
                        poly = hsub(bnd[-1][0], e, z0, ms) + hsub(e, bnd[0][0], z1, ms) + bnd[1:-1]
                    else:
                        poly = hsub(e_prev, e, z0, ms) + hsub(e, e_prev, z1, ms)
                    e_prev = e
                    br_face_block(mb, rng, poly, sx, sy, BRIDGE, rng.random() * 30)
    # ---- voussoir ring, lighter stone, proud of the face
    side = [BR_KD + (BR_TS - BR_KD) * k / 7 for k in range(8)]
    bounds = [-t for t in reversed(side)] + side                 # 7 voussoirs a side + the keystone
    for sy in (1, -1):
        for i in range(len(bounds) - 1):
            ta, tb = bounds[i], bounds[i + 1]
            key = i == 7
            inner = [(BR_RI * math.sin(t), BR_CZ + BR_RI * math.cos(t)) for t in (ta, (ta + tb) / 2, tb)]
            if key:                                              # keystone runs up to the string course
                outer = [(BR_KA, -0.2), (-BR_KA, -0.2)]
            else:
                outer = [(BR_RE * math.sin(t), BR_CZ + BR_RE * math.cos(t)) for t in (tb, (ta + tb) / 2, ta)]
            poly = inset2(inner + outer, 0.002) if area2(inner + outer) > 0 else inset2((inner + outer)[::-1], 0.002)
            proud = BR_PROUD + (0.05 if key else 0.0)
            col = pick(rng, VOUSS, 0.05)
            with mb.xf(XZ_FRONT_NEG_Y):
                if sy > 0:
                    s0 = mb.prism(poly, -(BR_FACE + proud), -(BR_FACE - 0.45), col, b0=0.03)
                else:
                    s0 = mb.prism(poly, BR_FACE - 0.45, BR_FACE + proud, col, b1=0.03)
            mb.recolor(s0, stone_noise(i + sy * 40, 0.05))
    # ---- barrel (the inside of the arch), slightly outside the ring intrados
    nt, ny = 16, 8
    rb = BR_RI + 0.012
    ys = [-2.3 + 4.6 * j / ny for j in range(ny + 1)]
    ths = [-BR_TS + 2 * BR_TS * i / nt for i in range(nt + 1)]
    ids = [[mb.v((rb * math.sin(t), y, BR_CZ + rb * math.cos(t)), (0, 0, 0, 0)) for y in ys] for t in ths]
    for i in range(nt):
        for j in range(ny):
            tm = (ths[i] + ths[i + 1]) / 2
            ym = (ys[j] + ys[j + 1]) / 2
            c = mul(pick(rng, BRIDGE, 0.08), 0.82)
            c = mix(c, MOSS, 0.35 * smoothstep(0.75, 1.0, abs(tm) / BR_TS) * rng.random())
            mb.f_out([ids[i][j], ids[i + 1][j], ids[i + 1][j + 1], ids[i][j + 1]],
                     (2 * rb * math.sin(tm), ym, BR_CZ + 2 * rb * math.cos(tm)), with_a(c, 0.0), FLAT)
    # ---- pier faces under the springing (x = +-2.5), between the side-wall stones
    for sx in (1, -1):
        prev = []
        for z1, z0 in ((-2.4, -2.8), (-2.8, -3.2)):
            js = course_joints(rng, -1.85, 1.85, prev, 0.6, 1.0, first=(0.3, 0.8))
            prev = js
            edges = [-1.85] + js + [1.85]
            for a, b in zip(edges, edges[1:]):
                poly = inset2([(a, z0), (b, z0), (b, z1), (a, z1)], 0.002)
                with mb.xf(YZ_ALONG_X):          # w = X
                    if sx > 0:
                        s0 = mb.prism(poly, 2.5 + rng.uniform(0, 0.01), 3.0, pick(rng, BRIDGE), b0=0.035)
                    else:
                        s0 = mb.prism(poly, -3.0, -2.5 - rng.uniform(0, 0.01), pick(rng, BRIDGE), b1=0.035)
                seed = rng.random() * 9
                mb.recolor(s0, lambda p, c, seed=seed: mix(c, MOSS, 0.5 * smoothstep(-2.6, -3.2, p.z) *
                                                          clamp(0.3 + nz(p, 2.5, (seed, 1, 1)))))
    # ---- string course at deck level, proud
    for sy in (1, -1):
        js = course_joints(rng, -4.5, 4.5, [], 0.9, 1.3, first=(0.5, 1.0))
        edges = [-4.5] + js + [4.5]
        for a, b in zip(edges, edges[1:]):
            poly = inset2(hsub(a, b, -0.2) + hsub(b, a, -0.01), 0.002)
            col = mix(pick(rng, BRIDGE, 0.04), STONE_LT, 0.3)
            with mb.xf(XZ_FRONT_NEG_Y):
                if sy > 0:
                    s0 = mb.prism(poly, -(BR_FACE + BR_PROUD), -(BR_FACE - BR_DEPTH), col, b0=0.03)
                else:
                    s0 = mb.prism(poly, BR_FACE - BR_DEPTH, BR_FACE + BR_PROUD, col, b1=0.03)
            mb.recolor(s0, stone_noise(a * 3 + sy))
    # ---- deck: flagstones (flat faces, per-stone colour), gutters by the parapets
    nx, nyd = 20, 9
    xs = [-4.5 + 9.0 * i / nx for i in range(nx + 1)]
    yd = [-2.0 + 4.0 * j / nyd for j in range(nyd + 1)]
    gid = [[mb.v((x, y, 0.0), (0, 0, 0, 0)) for y in yd] for x in xs]
    for i in range(nx):
        for j in range(nyd):
            c = pick(rng, FLAGS, 0.1)
            if j in (0, nyd - 1):
                c = mul(mix(c, MOSS, 0.12), 0.85)
            mb.f_out([gid[i][j], gid[i + 1][j], gid[i + 1][j + 1], gid[i][j + 1]],
                     ((xs[i] + xs[i + 1]) / 2, (yd[j] + yd[j + 1]) / 2, -1.0), with_a(c, 0.0), FLAT)
    # ---- parapets (0.35 thick, 0.85 above the deck incl. coping) and corner pillars
    for sy in (1, -1):
        yc = sy * (2.0 + 0.175)
        x = -3.95
        for L in (0.9875,) * 8:
            s0 = mb.cbox((x + L / 2, yc, 0.35), (L - 0.004, 0.35, 0.76), 0.03, pick(rng, BRIDGE), sh=FLAT)
            mb.recolor(s0, stone_noise(x + sy * 7))
            x += L
        x = -3.95
        for L in (0.6, 1.1, 1.0, 1.1, 0.9, 1.1, 1.2, 0.9):
            s0 = mb.cbox((x + L / 2, yc, 0.785), (L - 0.004, 0.45, 0.13), 0.035,
                         mix(pick(rng, BRIDGE, 0.04), STONE_LT, 0.35), sh=FLAT)
            mb.recolor(s0, lambda p, c: mix(c, MOSS, 0.4) if (p.z > 0.8 and rng.random() < 0.12) else c)
            x += L
        for sx in (1, -1):
            xc = sx * 4.2
            col = mix(pick(rng, BRIDGE, 0.03), STONE_LT, 0.2)
            mb.recolor(mb.cbox((xc, yc, 0.46), (0.5, 0.5, 0.98), 0.035, col, sh=FLAT), stone_noise(xc + sy))
            mb.cbox((xc, yc, 1.0), (0.6, 0.6, 0.1), 0.03, mix(col, STONE_LT, 0.4), sh=FLAT)
            with mb.xf(T(xc, yc, 1.05)):
                mb.lathe([(0, 0), (0.37, 0), (0.37, 0.03), (0, 0.2)], 4, col=mix(col, STONE_LT, 0.3), sh=FLAT,
                         phase=math.pi / 4)
    # ---- end caps (usually buried in the banks), just inside the stone ends; per-stone colours in case they show
    for sx in (1, -1):
        x = sx * 4.49
        gy = [-1.9, -0.95, 0.0, 0.95, 1.9]
        gz = [-3.2, -2.2, -1.2, -0.2, 0.0]
        cid = [[mb.v((x, y, z), (0, 0, 0, 0)) for z in gz] for y in gy]
        for i in range(len(gy) - 1):
            for j in range(len(gz) - 1):
                c = mul(pick(rng, BRIDGE, 0.08), 0.8)
                mb.f_out([cid[i][j], cid[i + 1][j], cid[i + 1][j + 1], cid[i][j + 1]], (0, 0, -1.5),
                         with_a(c, 0.0), FLAT)
    mb.deform(0, lambda p: br_deform(p))
    return mb


# ================================================================================================ 7. harbour light
def make_harbour_light():
    mb = MB("SM_KG_HarbourLight", angle=32, material="Glow")
    rng = random.Random(71)
    ph = math.pi / 8
    oc = 1.0 / math.cos(math.pi / 8)                              # across-flats -> circumradius
    # octagonal stone plinth, 1.8 across (flats on the axes), 0.5 tall, one step
    fam = [pick(rng, TERRACE, 0.05) for _ in range(16)]
    prof = [(0, 0), (0.9 * oc, 0), (0.9 * oc, 0.22), (0.87 * oc, 0.25), (0.68 * oc, 0.25), (0.68 * oc, 0.47),
            (0.65 * oc, 0.5), (0, 0.5)]
    s0 = mb.lathe(prof, 8, col=STONE, sh=FLAT, phase=ph,
                  facefn=lambda i, j: rgb3(fam[j if i < 3 else 8 + j]))
    mb.recolor(s0, stone_noise(1.0, 0.05))

    # tapering cast-iron tower (0.9 -> 0.6 across, 2.6 tall), white with a red band
    def tr(z):
        return 0.45 - 0.15 * (z - 0.5) / 2.6
    zs = [0.5, 0.58, 1.75, 2.2, 3.0]
    prof = [(0, 0.5), (0.49, 0.5), (0.49, 0.58)] + [(tr(z), z) for z in zs[1:]] + [(0.36, 3.1), (0, 3.1)]
    segcol = [WHITE, WHITE, WHITE, WHITE, RED, WHITE, IRON, IRON]
    mb.lathe(prof, 12, col=WHITE, segcol=[rgb3(c) for c in segcol], sh=AUTO, phase=math.pi / 12)
    # door and portholes
    taper = math.degrees(math.atan(0.15 / 2.6))
    zd = 0.95
    with mb.xf(T(0, -tr(zd) + 0.012, zd) @ R(-taper, 'X')):
        mb.cbox((0, 0, 0), (0.34, 0.05, 0.7), 0.012, C("2E5A4C"))
        mb.cbox((0, -0.02, 0.37), (0.4, 0.03, 0.05), 0.008, IRON)
        mb.cyl((0.1, -0.02, 0.0), (0.1, -0.045, 0.0), 0.018, seg=6, col=GOLD)
    for zc, yaw in ((2.55, 0), (1.45, 90), (2.55, 180)):
        with mb.xf(R(yaw, 'Z')):
            r = tr(zc)
            mb.cyl((0, -r + 0.02, zc), (0, -r - 0.03, zc), 0.07, seg=10, col=IRON)
            mb.cyl((0, -r - 0.02, zc), (0, -r - 0.034, zc), 0.05, seg=10, col=C("2A3A4A"))
    # gallery at ~3.1 with brackets and railing
    mb.lathe([(0, 3.08), (0.64, 3.08), (0.64, 3.15), (0, 3.15)], 16, col=IRON, sh=AUTO)
    for k in range(8):
        a = 2 * math.pi * k / 8 + math.pi / 8
        ca, sa = math.cos(a), math.sin(a)
        mb.beam((0.3 * ca, 0.3 * sa, 2.8), (0.6 * ca, 0.6 * sa, 3.08), 0.035, 0.035, IRON, up=(0, 0, 1))
    for k in range(12):
        a = 2 * math.pi * k / 12
        mb.cyl((0.6 * math.cos(a), 0.6 * math.sin(a), 3.15), (0.6 * math.cos(a), 0.6 * math.sin(a), 3.56), 0.014,
               seg=6, col=IRON, caps=False)
    with mb.xf(T(0, 0, 3.56)):
        mb.torus(0.6, 0.022, 20, 5, col=IRON)
    with mb.xf(T(0, 0, 3.35)):
        mb.torus(0.6, 0.012, 20, 4, col=IRON)
    # lantern room: sill, glass cylinder 0.7 across x 0.8 tall (A = 1), astragal bars (A = 0)
    mb.lathe([(0, 3.15), (0.41, 3.15), (0.41, 3.22), (0, 3.22)], 12, col=IRON, sh=AUTO, phase=math.pi / 12)
    mb.lathe([(0, 3.22), (0.35, 3.22), (0.35, 4.02), (0, 4.02)], 12, col=GLASS, sh=FLAT, phase=math.pi / 12,
             segcol=[rgb3(IRON) + (0.0,), None, rgb3(IRON) + (0.0,)])
    for k in range(12):
        a = math.pi / 12 + 2 * math.pi * k / 12
        with mb.xf(R(math.degrees(a), 'Z')):
            mb.box((0.35 * math.cos(math.pi / 12) + 0.004, 0, 3.62), (0.03, 0.028, 0.8), IRON)
    for zc in (3.25, 3.62, 3.99):
        with mb.xf(T(0, 0, zc)):
            mb.lathe([(0.34, -0.018), (0.37, -0.018), (0.37, 0.018), (0.34, 0.018)], 12, col=IRON, sh=FLAT,
                     closed=True, phase=math.pi / 12)
    # copper-green domed cap with ball finial (top ~4.3)
    prof = [(0, 4.02), (0.44, 4.02), (0.44, 4.06), (0.41, 4.075)]
    for d in (10, 28, 46, 64, 80):
        a = math.radians(d)
        prof.append((0.4 * math.cos(a), 4.075 + 0.15 * math.sin(a)))
    prof.append((0.035, 4.23))
    prof.append((0, 4.23))
    ring_cols = [VERD_DK, VERD_DK, VERD_DK, VERD, VERD, VERD_LT, VERD, VERD_LT, VERD, VERD, VERD]
    mb.lathe(prof, 16, col=VERD, segcol=[rgb3(c) for c in ring_cols], sh=AUTO)
    prof = [(0, 4.21), (0.022, 4.21), (0.022, 4.23)]
    zc, rb = 4.258, 0.042
    for d in (-60, -25, 10, 45, 75):
        a = math.radians(d)
        prof.append((rb * math.cos(a), zc + rb * math.sin(a)))
    prof.append((0, zc + rb))
    mb.lathe(prof, 10, col=VERD_LT, sh=SMOOTH)
    return mb


def rgb3(c):
    return (c[0], c[1], c[2])


# ================================================================================================ 8. clock face
def make_clock_face():
    mb = MB("SM_KG_ClockFace", angle=30)
    rng = random.Random(81)
    gold_hi, gold_lo = C("F4CC5A"), C("B8862A")
    with mb.xf(DIAL):                               # local x = X, local y = Z, local z = proud (towards -Y)
        # stone surround 2.0 x 2.0: recessed back slab, mitred raised border 0.1 proud, keystone at the top
        s0 = mb.prism([(-0.98, -0.98), (0.98, -0.98), (0.98, 0.98), (-0.98, 0.98)], 0.0, 0.075,
                      mul(pick(rng, TERRACE), 0.92), b1=0.01)
        mb.recolor(s0, stone_noise(1.0))
        o, i_ = 1.0, 0.8
        sides = [[(-o, -o), (o, -o), (i_, -i_), (-i_, -i_)], [(o, -o), (o, o), (i_, i_), (i_, -i_)],
                 [(o, o), (-o, o), (-i_, i_), (i_, i_)], [(-o, o), (-o, -o), (-i_, -i_), (-i_, i_)]]
        for k, poly in enumerate(sides):
            s0 = mb.prism(inset2(poly if area2(poly) > 0 else poly[::-1], 0.002), 0.0, 0.1, pick(rng, TERRACE),
                          b1=0.025)
            mb.recolor(s0, stone_noise(2.0 + k))
        s0 = mb.prism([(-0.11, 0.76), (0.11, 0.76), (0.155, 1.06), (-0.155, 1.06)], 0.0, 0.13,
                      mix(pick(rng, TERRACE), WHITE, 0.2), b1=0.02)
        mb.recolor(s0, stone_noise(9.0))
        for sx in (-1, 1):                           # little gilded rosettes in the corners
            for sy in (-1, 1):
                with mb.xf(T(sx * 0.705, sy * 0.705, 0.075)):
                    mb.lathe([(0, 0), (0.055, 0), (0.055, 0.012), (0.03, 0.03), (0, 0.035)], 8, col=GOLD, sh=AUTO)
        # dial: cream disc, flat back at Y=0, face 0.12 proud
        mb.lathe([(0, 0.0), (0.7, 0.0), (0.7, 0.12), (0, 0.12)], 32, col=CREAM, sh=FLAT,
                 segcol=[None, rgb3(CREAM), rgb3(C("F6EDD5"))])
        mb.lathe([(0.585, 0.12), (0.6, 0.12), (0.6, 0.123), (0.585, 0.123)], 32, col=C("3A3530"), sh=FLAT,
                 closed=True)
        # gilded bevelled outer ring (dial overall 1.6 across)
        prof = [(0.685, 0.0), (0.8, 0.0), (0.8, 0.1), (0.78, 0.14), (0.74, 0.155), (0.705, 0.145), (0.688, 0.125)]
        rc = [GOLD, gold_lo, GOLD, gold_hi, GOLD, gold_lo, gold_lo]
        mb.lathe(prof, 32, col=GOLD, segcol=[rgb3(c) for c in rc], sh=AUTO, closed=True)
        # hour marks (longer at 12/3/6/9)
        for k in range(12):
            th = math.radians(30 * k)
            d = Vector((math.sin(th), math.cos(th), 0))
            long_ = k % 3 == 0
            r0, r1, w = (0.44, 0.645, 0.055) if long_ else (0.52, 0.645, 0.032)
            mb.beam(d * r0 + Vector((0, 0, 0.128)), d * r1 + Vector((0, 0, 0.128)), w, 0.016, BLACK, up=(0, 0, 1))
        # hands at ~10:08 (hour 304 deg, minute 48 deg clockwise from 12), standing proud of the dial
        hand = C("1E1D24")
        with mb.xf(R(-(10 + 8 / 60) * 30, 'Z')):
            mb.ext2d([(-0.032, -0.09), (0.032, -0.09), (0.024, 0.2), (0.07, 0.27), (0.0, 0.37), (-0.07, 0.27),
                      (-0.024, 0.2)], 0.135, 0.153, hand)
        with mb.xf(R(-8 * 6, 'Z')):
            mb.ext2d([(-0.022, -0.12), (0.022, -0.12), (0.016, 0.47), (0.034, 0.5), (0.0, 0.585), (-0.034, 0.5),
                      (-0.016, 0.47)], 0.157, 0.172, hand)
        mb.lathe([(0, 0.12), (0.055, 0.12), (0.055, 0.178), (0.04, 0.192), (0.015, 0.198), (0, 0.198)], 12,
                 col=hand, sh=AUTO)
        mb.lathe([(0, 0.197), (0.018, 0.197), (0.012, 0.205), (0, 0.206)], 8, col=GOLD, sh=AUTO)
    return mb


# ================================================================================================ registry
PROPS = [
    ("Balustrade_2m", make_balustrade, "box"),
    ("Balustrade_Post", make_post, "box"),
    ("QuayWall_4m", lambda: make_quay_wall(False), "box"),
    ("QuayWall_4m_Ring", lambda: make_quay_wall(True), "box"),
    ("QuaySteps", make_quay_steps, "complex"),
    ("StoneArchBridge_7m", make_bridge, "complex"),
    ("HarbourLight", make_harbour_light, "box"),
    ("ClockFace", make_clock_face, "none"),
]

# expected bounds (Blender axes, metres) for the pivot / extents check; None = not checked
EXPECT = {
    "Balustrade_2m": ((-1.0, -0.175, 0.0), (1.0, 0.175, 1.0)),
    "Balustrade_Post": ((-0.22, -0.22, 0.0), (0.22, 0.22, 1.15)),
    "QuayWall_4m": ((-2.0, -0.08, -6.0), (2.0, 0.9, 0.0)),
    "QuayWall_4m_Ring": ((-2.0, None, -6.0), (2.0, 0.9, 0.0)),
    "QuaySteps": ((0.0, -1.3, -3.5), (4.2, 0.0, None)),
    "StoneArchBridge_7m": ((-4.5, None, -3.2), (4.5, None, None)),
    "HarbourLight": ((-0.9, -0.9, 0.0), (0.9, 0.9, 4.3)),
    "ClockFace": ((-1.0, None, -1.0), (1.0, 0.0, None)),
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
    print("\nKG_DT: object                        tris   size (cm)             min (cm)               mat   col")
    for name, tris, mn, mx, diag, mat, col, info in stats:
        sz = (mx - mn) * 100
        print(f"KG_DT: {name:30s} {tris:6d}  {sz.x:5.0f} x {sz.y:5.0f} x {sz.z:5.0f}  min({mn.x * 100:6.0f},{mn.y * 100:6.0f},"
              f"{mn.z * 100:6.0f}) max({mx.x * 100:6.0f},{mx.y * 100:6.0f},{mx.z * 100:6.0f})  {mat:5s} {col:8s} {diag}")
    print(f"KG_DT: total tris {sum(s[1] for s in stats)} in {len(stats)} props")
    return objs, stats


def export(path, objs):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"KG_DT: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


def write_manifest(stats):
    data = {"props": {s[0].replace("SM_KG_", ""): {"material": s[5], "collision": s[6]} for s in stats}}
    with open(MANIFEST, "w") as fh:
        json.dump(data, fh, indent=1)
    print(f"KG_DT: manifest {MANIFEST} ({len(data['props'])} props)")


# ================================================================================================ preview
def _node_mat(name, glow=False):
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
        m.inputs[1].default_value = 0.9
        nt.links.new(att.outputs["Alpha"], m.inputs[0])
        nt.links.new(att.outputs["Color"], bsdf.inputs["Emission Color"])
        nt.links.new(m.outputs[0], bsdf.inputs["Emission Strength"])
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


def _bounds(o):
    cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))),
            Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs))))


def preview_tiles(names):
    """Tile specs: items = [(key, (x, y, z), yaw_deg)]; key = prop short name | REF | HUMAN | WATER."""
    H = "HUMAN"
    t1 = [
        dict(label="Balustrade_2m", items=[("Balustrade_2m", (0, 0, 0), 0), (H, (1.45, -0.35, 0), 0)]),
        dict(label="Balustrade_Post", items=[("Balustrade_Post", (0, 0, 0), 0), (H, (0.75, -0.3, 0), 0)]),
        dict(label="QuayWall_4m", items=[("QuayWall_4m", (0, 0, 0), 0), (H, (1.2, 0.45, 0), 0)]),
        dict(label="QuayWall_4m_Ring", items=[("QuayWall_4m_Ring", (0, 0, 0), 0), (H, (1.2, 0.45, 0), 0)]),
        dict(label="QuaySteps", items=[("QuaySteps", (0, 0, 0), 0), (H, (1.225, -0.6, -1.0), 0)], az=-30, el=22),
        dict(label="StoneArchBridge_7m", items=[("StoneArchBridge_7m", (0, 0, 0), 0),
                                                (H, (1.0, -0.8, br_hump(1.0)), 0)], az=-28, el=14),
        dict(label="HarbourLight", items=[("HarbourLight", (0, 0, 0), 0), (H, (1.35, -0.4, 0), 0)]),
        dict(label="ClockFace", items=[("ClockFace", (0, 0, 0), 0), (H, (1.5, -0.4, -1.0), 0)], az=-20, el=8),
        dict(label="Fountain (DressVillage ref)", items=[("REF", (0, 0, 0), 0), (H, (2.3, -0.6, 0), 0)]),
        dict(label="Balustrade run: post + 2 x 2m + post",
             items=[("Balustrade_Post", (-2.22, 0, 0), 0), ("Balustrade_2m", (-1, 0, 0), 0),
                    ("Balustrade_2m", (1, 0, 0), 0), ("Balustrade_Post", (2.22, 0, 0), 0), (H, (0.2, -0.7, 0), 0)],
             az=-25, el=14),
        dict(label="Quay context (water at Z=-2)",
             items=[("QuayWall_4m_Ring", (-2, 0, 0), 0), ("QuayWall_4m", (2, 0, 0), 0), ("QuayWall_4m", (6, 0, 0), 0),
                    ("QuaySteps", (0, 0, 0), 0), ("WATER", (0, 0, -2.0), 0), (H, (0.6, 0.45, 0), 0),
                    (H, (2.975, -0.6, -2.5), 0)], az=-25, el=20, ground=-6.0),
    ]
    # lineup, bottoms on the ground, human for scale
    t1.append(dict(label="Lineup", items="LINEUP", az=-12, el=12))
    t2 = [
        dict(label="Bridge: through the arch", items=[("StoneArchBridge_7m", (0, 0, 0), 0), (H, (-3.4, -3.3, -3.2), 0)],
             az=0, el=3),
        dict(label="Bridge: deck", items=[("StoneArchBridge_7m", (0, 0, 0), 0), (H, (1.0, -0.8, br_hump(1.0)), 0)],
             az=-35, el=42),
        dict(label="Bridge: back / wing walls", items=[("StoneArchBridge_7m", (0, 0, 0), 0)], az=145, el=10),
        dict(label="QuayWall: waterline", items=[("QuayWall_4m", (0, 0, 0), 0)], az=-20, el=6,
             target=(0, 0, -2.0), dist=5.2),
        dict(label="Ring close-up", items=[("QuayWall_4m_Ring", (0, 0, 0), 0)], az=-30, el=14,
             target=(0, -0.1, -0.7), dist=3.2, sun=-15),
        dict(label="QuaySteps: from the sea", items=[("QuaySteps", (0, 0, 0), 0), ("QuayWall_4m", (2, 0, 0), 0)],
             az=15, el=30, sun=-30),
        dict(label="QuaySteps: treads", items=[("QuaySteps", (0, 0, 0), 0), ("QuayWall_4m", (2, 0, 0), 0)],
             az=-38, el=38, target=(2.3, -0.6, -1.7), dist=6.0, sun=-25),
        dict(label="HarbourLight: lantern", items=[("HarbourLight", (0, 0, 0), 0)], az=-30, el=8,
             target=(0, 0, 3.6), dist=2.6),
        dict(label="HarbourLight: back", items=[("HarbourLight", (0, 0, 0), 0)], az=150, el=14),
        dict(label="ClockFace: front", items=[("ClockFace", (0, 0, 0), 0)], az=0, el=0),
        dict(label="Balustrade close-up", items=[("Balustrade_2m", (0, 0, 0), 0), ("Balustrade_Post", (1.22, 0, 0), 0)],
             az=-20, el=12, target=(0.4, 0, 0.55), dist=2.6),
        dict(label="Post close-up", items=[("Balustrade_Post", (0, 0, 0), 0)], az=-30, el=18, target=(0, 0, 0.8),
             dist=1.8),
    ]
    keep = set(names)
    def ok(t):
        return t["items"] == "LINEUP" or all(k in keep or k in ("REF", "HUMAN", "WATER") for k, _, _ in t["items"])
    return [t for t in t1 if ok(t)], [t for t in t2 if ok(t)]


def render_previews(objs):
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
    glow = _node_mat("P_Glow", glow=True)
    by_key = {}
    for o in objs:
        o.data.materials[0] = glow if o.data.materials[0].name == MAT_NAME["Glow"] else plain
        by_key[o.name.replace("SM_KG_", "")] = o
    # reference: the existing SM_KG_Fountain from the village pack (Clean glb)
    if os.path.exists(VILLAGE_CLEAN):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=VILLAGE_CLEAN)
        new = [o for o in bpy.data.objects if o not in before]
        ref = next((o for o in new if o.type == "MESH" and o.name.startswith("SM_KG_Fountain")), None)
        for o in new:
            if o is not ref:
                bpy.data.objects.remove(o, do_unlink=True)
        if ref:
            ref.parent = None
            ref.matrix_world = Matrix.Identity(4)
            if len(ref.data.color_attributes):
                ca = ref.data.color_attributes[0]
                print(f"KG_DT: ref colour attribute {ca.name} {ca.data_type} {ca.domain}")
                ca.name = "Col"
            ref.data.materials.clear()
            ref.data.materials.append(plain)
            by_key["REF"] = ref
    hm = MB("PV_Human")
    hm.cbox((0, 0, 0.9), (0.46, 0.28, 1.8), 0.05, C("E0673F"))
    hm.cbox((0, -0.13, 1.6), (0.3, 0.04, 0.08), 0.01, C("2B2B33"))      # "eyes" band: front = -Y
    human, _, _ = finalize(hm)
    human.data.materials[0] = plain
    by_key["HUMAN"] = human
    wm = bpy.data.meshes.new("PV_Water")
    wm.from_pydata([(-6, -9, 0), (10, -9, 0), (10, 0, 0), (-6, 0, 0)], [], [(0, 1, 2, 3)])
    water = bpy.data.objects.new("PV_Water", wm)
    sc.collection.objects.link(water)
    water.data.materials.append(_flat_mat("P_Water", (0.1, 0.42, 0.55)))
    by_key["WATER"] = water
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
    tmp_root = os.environ.get("KG_TMP") or tempfile.gettempdir()
    os.makedirs(tmp_root, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="kg_dt_", dir=tmp_root)
    pool = list(by_key.values())

    def lineup_items():
        items, x = [], 0.0
        order = [n for n, _, _ in PROPS if n in by_key] + (["REF"] if "REF" in by_key else [])
        for k in order:
            o = by_key[k]
            o.matrix_world = Matrix.Identity(4)
            bpy.context.view_layer.update()
            mn, mx = _bounds(o)
            items.append((k, (x - mn.x, -(mn.y + mx.y) / 2, -mn.z), 0))
            x += (mx.x - mn.x) + 0.8
            if k == "QuaySteps":
                items.append(("HUMAN", (x - 0.2, 0, 0), 0))
                x += 0.8
        return items

    def shoot(tile, path):
        items = lineup_items() if tile["items"] == "LINEUP" else tile["items"]
        for o in pool:
            o.hide_render = True
        used, temps, shown = set(), [], []
        for key, loc, yaw in items:
            base = by_key[key]
            o = base
            if key in used:
                o = base.copy()
                sc.collection.objects.link(o)
                temps.append(o)
            used.add(key)
            o.hide_render = False
            o.matrix_world = T(*loc) @ R(yaw, 'Z')
            shown.append((key, o))
        bpy.context.view_layer.update()
        corners = []
        for key, o in shown:
            if key != "WATER":
                corners += [o.matrix_world @ Vector(c) for c in o.bound_box]
        zmin = min(c.z for c in corners)
        ground.location.z = tile.get("ground", min(0.0, zmin)) - 0.01
        el = math.radians(tile.get("el", 16))
        az = math.radians(tile.get("az", -35))
        sun.rotation_euler = (math.radians(50), 0, az + math.radians(tile.get("sun", -55)))
        d = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
        f = -d
        if "target" in tile:
            c = Vector(tile["target"])
            dist = tile["dist"]
        else:
            c = sum(corners, Vector()) / len(corners)
            right = f.cross(Vector((0, 0, 1))).normalized()
            up = right.cross(f)
            dist = max(max(abs((p - c).dot(right)), abs((p - c).dot(up))) / tan_h - (p - c).dot(f) for p in corners)
            dist = dist * 1.06 / tile.get("zoom", 1.0) + 0.05
        cam.location = c - f * dist
        cam.rotation_euler = f.to_track_quat("-Z", "Y").to_euler()
        lbl.data.body = tile["label"]
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        for o in temps:
            bpy.data.objects.remove(o, do_unlink=True)

    os.makedirs(PREVIEW_DIR, exist_ok=True)
    sheets = preview_tiles(list(by_key))
    for si, tiles in enumerate(sheets):
        if not tiles:
            continue
        cols = 4
        rows = (len(tiles) + cols - 1) // cols
        sheet = np.zeros((rows * TILE, cols * TILE, 4), np.float32)
        sheet[..., 3] = 1.0
        for k, tile in enumerate(tiles):
            p = os.path.join(tmp, f"t{si}_{k}.png")
            shoot(tile, p)
            img = bpy.data.images.load(p)
            arr = np.empty(TILE * TILE * 4, np.float32)
            img.pixels.foreach_get(arr)
            bpy.data.images.remove(img)
            r, c = divmod(k, cols)
            y0 = (rows - 1 - r) * TILE
            sheet[y0:y0 + TILE, c * TILE:(c + 1) * TILE] = arr.reshape(TILE, TILE, 4)
        out = bpy.data.images.new(f"sheet{si}", cols * TILE, rows * TILE, alpha=False)
        out.pixels.foreach_set(sheet.ravel())
        path = os.path.join(PREVIEW_DIR, f"{PREFIX}.png" if si == 0 else f"{PREFIX}_{si + 1}.png")
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        print(f"KG_DT: preview {path}")
    shutil.rmtree(tmp, ignore_errors=True)


# ================================================================================================ verify
def _glb_json(path):
    with open(path, "rb") as fh:
        data = fh.read()
    n = struct.unpack("<I", data[12:16])[0]
    return json.loads(data[20:20 + n].decode("utf-8"))


def verify(path, stats=None):
    """Re-import a glb: per object dimensions, pivot/extent check vs EXPECT, tris, colour attribute, alpha range."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    js = _glb_json(path)
    color0 = {}
    for nd in js.get("nodes", []):
        if "mesh" in nd:
            prims = js["meshes"][nd["mesh"]]["primitives"]
            color0[nd.get("name")] = all("COLOR_0" in p["attributes"] for p in prims)
    expect_tris = {s[0]: s[1] for s in stats} if stats else {}
    manifest = {}
    if os.path.exists(MANIFEST):
        manifest = json.load(open(MANIFEST))["props"]
    ok = True
    total = 0
    for o in sorted(bpy.data.objects, key=lambda o: o.name):
        if o.type != "MESH":
            continue
        me = o.data
        tris = sum(len(p.vertices) - 2 for p in me.polygons)
        total += tris
        ca = me.color_attributes[0] if len(me.color_attributes) else None
        amin = amax = -1.0
        if ca:
            arr = np.empty(len(ca.data) * 4, np.float32)
            ca.data.foreach_get("color", arr)
            a = arr[3::4]
            amin, amax = float(a.min()), float(a.max())
        cs = [o.matrix_world @ v.co for v in me.vertices]
        mn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
        mx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
        short = o.name.replace("SM_KG_", "")
        exp = EXPECT.get(short)
        bad = []
        if exp:
            for lab, got, want in (("min", mn, exp[0]), ("max", mx, exp[1])):
                for ax, g, w in zip("xyz", got, want):
                    if w is not None and abs(g - w) > 0.03:
                        bad.append(f"{lab}.{ax}={g:.3f}!={w}")
        bm = bmesh.new()
        bm.from_mesh(me)
        loose = sum(1 for v in bm.verts if not v.link_faces)
        bm.free()
        mat = manifest.get(short, {}).get("material", "?")
        alpha_ok = (amax <= 0.001) if mat == "VC" else (amax >= 0.999 and amin <= 0.001)
        good = (ca is not None and color0.get(o.name, False) and not bad and loose == 0 and alpha_ok
                and o.location.length < 1e-6 and (not expect_tris or o.name in expect_tris))
        ok &= good
        print(f"KG_DT_VERIFY: {o.name:28s} tris {tris:5d}  dims {mx.x - mn.x:5.2f} x {mx.y - mn.y:5.2f} x {mx.z - mn.z:5.2f}"
              f"  min ({mn.x:6.3f},{mn.y:6.3f},{mn.z:6.3f}) max ({mx.x:6.3f},{mx.y:6.3f},{mx.z:6.3f})"
              f"  origin {tuple(round(c, 3) for c in o.location)}  attr={ca.name if ca else None}"
              f"/{ca.data_type if ca else ''} COLOR_0={color0.get(o.name)}  alpha[{amin:.2f},{amax:.2f}] {mat}"
              f"/{manifest.get(short, {}).get('collision', '?')}  loose={loose}  {'OK' if good else 'FAIL ' + str(bad)}")
    missing = set(expect_tris) - {o.name for o in bpy.data.objects}
    if missing:
        ok = False
        print(f"KG_DT_VERIFY: MISSING {sorted(missing)}")
    print(f"KG_DT_VERIFY: total tris {total}; {'OK' if ok else 'FAILED'}")


def main():
    if "--verify-clean" in _opt:
        verify(_opt["--verify-clean"])
        return
    objs, stats = build_all()
    if "--no-export" not in _opt:
        export(OUT_GLB, objs)
        write_manifest(stats)
    if "--no-preview" not in _opt:
        render_previews(objs)
    if "--no-verify" not in _opt and "--no-export" not in _opt:
        verify(OUT_GLB, stats)


main()
