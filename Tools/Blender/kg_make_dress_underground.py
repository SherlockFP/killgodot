"""Procedural underground / dig dressing props (pack KG_DressUnder). Headless Blender 5.2, no textures.
Stylised Quaternius-like low poly: chunky, slightly bevelled, flat-shaded, saturated toon palette, real-world scale.
Helpers and the build/export/preview/verify machinery are copied from kg_make_dress_terrace.py (not importable: it
runs main() on import); skull/bone/soil/crater/ashlar helpers are new.

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_underground.py -- \
      [out.glb] [preview_dir] [--only A,B] [--no-preview] [--no-verify] [--no-export] [--tile 512]
      [--manifest path.json] [--prefix DressUnder_preview]
  blender --background --factory-startup --python Tools/Blender/kg_make_dress_underground.py -- --verify-clean in.glb

Output: Art/Packed/KG_DressUnder.glb (one mesh object per prop, SM_KG_<Name>, metres, Z up), the import manifest
Art/Packed/KG_DressUnder_Clean.json and contact sheets Art/Concept/DressUnder_preview.png / _2.png.
Then: kg_sanitize_glb.py -- Art/Packed/KG_DressUnder.glb Art/Packed/KG_DressUnder_Clean.glb

AXES: props are authored in Blender but used in Unreal. The glTF import maps UE_x = Blender_x, UE_y = -Blender_y,
Z = Z. Every prop is authored inside `with mb.xf(UE):` (UE = S(1, -1, 1); MB flips the winding for mirrored
matrices) so all coordinates in the make_* functions, the EXPECT table and the preview tile specs are UE-local.
Colour callbacks that receive world (Blender) positions convert with ue(p).

Vertex colour attribute "Col" (FLOAT_COLOR, face corner, exported as COLOR_0): RGB = LINEAR albedo, A = mask:
  Glow material (M_KG_DU_Glow): 1 on emissive faces (glints, candle flames, well daylight, door light seam), 0 elsewhere
  VC material (M_KG_DU_VC): 0 everywhere
Pivots (object origin, UE axes):
  dig props (DigMound, DigX, DigGlint, DigHole_1..3, DirtPile, GraveOpen_1/2, BuriedChest): ground centre, all Z >= 0
  NicheWall_2m / MineWall_2m : face plane Y=0 facing -Y, body to +Y, X -1..1, Z=0 floor (MineWall post at X=-1, Y=-0.12;
                               rotating a wall 180 deg puts its post at local X=+1)
  CryptVault_2m              : springing line Z=0 (sits on 3.12 m walls), spans Y -1.25..1.25, X -1..1
  MineCeiling_2m             : underside centre (rock hangs to -0.1, cap beam to -0.22 at X=-1)
  MineFloor_2m               : top centre (top Z=0, 0.1 thick)
  CryptGate                  : hinge bottom (leaf along +X to 1.9)
  CryptStair                 : foot of the first step (X=0, Y=0, Z=0), rises along +X
  everything else            : bottom centre
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
LOG = "KG_DU"

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
OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_DressUnder.glb"
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"
TILE = int(_opt.get("--tile", 512))
ONLY = [s if s.startswith("SM_KG_") else "SM_KG_" + s for s in _opt["--only"].split(",")] if "--only" in _opt else None
MANIFEST = _opt.get("--manifest", f"{ROOT}/Art/Packed/KG_DressUnder_Clean.json")
PREFIX = _opt.get("--prefix", "DressUnder_preview")

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


UE = S(1, -1, 1)          # author in UE axes: Blender_y = -UE_y


def ue(p):
    """World (Blender) position -> UE position."""
    return Vector((p.x, -p.y, p.z))


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


def ccw(poly):
    return list(poly) if area2(poly) > 0 else list(poly)[::-1]


def dedupe(poly, eps=1e-5):
    out = []
    for p in poly:
        if not out or abs(p[0] - out[-1][0]) > eps or abs(p[1] - out[-1][1]) > eps:
            out.append(p)
    if len(out) > 1 and abs(out[0][0] - out[-1][0]) < eps and abs(out[0][1] - out[-1][1]) < eps:
        out.pop()
    return out


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


def prof_h(prof, r):
    """Height of a lathe top profile (list of (r, z), axis point last) at radius r."""
    pts = sorted(prof[1:], key=lambda t: t[0])
    if r <= pts[0][0]:
        return pts[0][1]
    for (r0, z0), (r1, z1) in zip(pts, pts[1:]):
        if r0 <= r <= r1:
            return lerp(z0, z1, (r - r0) / max(r1 - r0, 1e-9))
    return pts[-1][1]


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


def pick(rng, fam, amt=0.06):
    return jit(rng.choice(fam), rng, amt)


# palette (sRGB hex -> linear)
SOIL_DK, SOIL, SOIL_LT, SOIL_DRY = C("3E2A1C"), C("5B3D27"), C("7A5234"), C("8E6843")
PIT = C("120C08")
PEBBLES = [C("8E8A82"), C("A39C90"), C("77736C"), C("9A8F7E"), C("B0A898")]
GRASS0, GRASS1 = C("3F6B24"), C("8DBA45")
PAINT, PAINT_DK, CHIP = C("D8352A"), C("A82820"), C("CDBB9C")
WOOD, WOOD_DK, WOOD_LT = C("8B5A33"), C("563622"), C("A8744A")
TIMBER, TIMBER_DK, TIMBER_LT = C("6B4428"), C("4A2E1A"), C("8A5E3A")
IRON, IRON_LT, RUST = C("35343A"), C("5A5860"), C("8C4A26")
GOLD, GOLD_DK, GOLD_HI = C("F2C230"), C("C8922A"), C("FFE07A")
GLINT = C("FFF1C4", a=1.0)
IVORY, IVORY_DK, IVORY_HI, SOCKET = C("E9DFC6"), C("CFC2A2"), C("F5EEDC"), C("2A221E")
DUST, DUST_DK = C("B3A68C"), C("7A6E5C")
CATA = [C("8D8F95"), C("80838B"), C("878A92"), C("767880"), C("6E7078"), C("92959D")]
VAULT = [C("7F828A"), C("777A83"), C("868991"), C("70737C"), C("7B7E86")]
FRAME = [C("A4A7AE"), C("9C9FA7"), C("ABAEB5"), C("A0A2A8")]
CATA_DAMP, MOSS_DK = C("4A5654"), C("56703E")
SARC = [C("A7A59E"), C("9E9D98"), C("ADABA3")]
CREAM, WAX = C("F0E2C0"), C("E6D3A4")
FLAME_Y, FLAME_O = C("FFD84A", a=1.0), C("FF8A22", a=1.0)
ROCK = [C("8A7866"), C("7D6D5D"), C("94826E"), C("736454"), C("857465"), C("9A8872")]
DIRT, DIRT_LT, DIRT_DK = C("6E5239"), C("80644A"), C("47352A")
WELLST = [C("8C9097"), C("7E838B"), C("969AA0"), C("858A90"), C("7A7F87")]
MORTAR = C("4A4D52")
SKY = C("D8ECFF", a=1.0)
DOORGLOW = C("FFB55C", a=1.0)
VSTONES = [C("A8A092"), C("8F887C"), C("B3A68E"), C("9A9CA0"), C("C0B6A2")]    # village dry stone
ROPE = C("C8B48A")
WATER = C("3DB8DA")
ROOT_C = C("8A6844")


def soil_col(p, seed=0.0, dry=0.0):
    t = 0.5 + 0.5 * nz(p, 5.0, (seed, 2.0, 1.0))
    c = mix(SOIL, SOIL_LT, t)
    c = mul(c, 1.0 + 0.1 * nz(p, 13.0, (1.0, seed, 4.0)))
    if dry > 0:
        c = mix(c, SOIL_DRY, dry)
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
    """Accumulates one mesh (from kg_make_dress_terrace.py; lathe gains rfn/zfn, blob gains rot, plus sweep/tube from
    kg_make_dress_village.py): vertices with colours, faces with optional flat colour override and shading mode.
    FLAT faces are faceted, SMOOTH always smooth, AUTO edges turn sharp above `angle` degrees. Closed islands get their
    normals recalculated outward on finalize (safety net)."""

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

    def lathe(self, prof, seg, col=None, colfn=None, segcol=None, sh=SMOOTH, phase=0.0, closed=False, facefn=None,
              rfn=None, zfn=None):
        """Surface of revolution around local Z. prof: [(r, z)] traced bottom-axis -> outside -> top-axis
        (outward normals, CCW in the (r, z) plane). r == 0 end points collapse to one apex vertex.
        rfn(a, i, z) -> radius factor, zfn(r, a, i, z) -> z offset (per ring vertex)."""
        start = len(self.V)
        rings = []
        for i, (r, z) in enumerate(prof):
            if r < 1e-6:
                rings.append(self.v((0, 0, z), colfn(0.0, z, 0.0, i) if colfn else col))
            else:
                ring = []
                for j in range(seg):
                    a = phase + 2 * math.pi * j / seg
                    rr = r * (rfn(a, i, z) if rfn else 1.0)
                    zz = z + (zfn(rr, a, i, z) if zfn else 0.0)
                    ring.append(self.v((rr * math.cos(a), rr * math.sin(a), zz), colfn(rr, zz, a, i) if colfn else col))
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

    def blob(self, c, r, sub=1, amp=0.15, freq=1.6, seed=0.0, col=None, sh=SMOOTH, rot=None):
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
            ids.append(self.v(c + lp, col))
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

    def sweep(self, pts, prof, sx=None, sy=None, col=None, sh=AUTO, cap0=True, cap1=True, up=(0, 0, 1), tip=0.0):
        """Sweep a 2D profile along a polyline (parallel-transport frames). sx/sy: per-point profile scales."""
        pts = [Vector(p) for p in pts]
        n, m = len(pts), len(prof)
        if area2(prof) < 0:
            prof = prof[::-1]
        Tn = [(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized() for i in range(n)]
        side = Vector(up).cross(Tn[0])
        if side.length < 1e-6:
            side = Vector((0, 1, 0)).cross(Tn[0])
            if side.length < 1e-6:
                side = Vector((1, 0, 0)).cross(Tn[0])
        side.normalize()
        sides, ups = [], []
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
            for px, py in prof:
                mx = sx[i] if sx else 1.0
                my = sy[i] if sy else 1.0
                ring.append(self.v(pts[i] + sides[i] * (px * mx) + ups[i] * (py * my), col))
            rings.append(ring)
        for i in range(n - 1):
            for k in range(m):
                kn = (k + 1) % m
                self.f((rings[i][k], rings[i][kn], rings[i + 1][kn], rings[i + 1][k]), None, sh)
        if cap0:
            self.f(list(reversed(rings[0])), None, FLAT)
        if tip > 0:
            apex = self.v(pts[-1] + Tn[-1] * tip, col)
            for k in range(m):
                self.f((rings[-1][k], rings[-1][(k + 1) % m], apex), None, sh)
        elif cap1:
            self.f(rings[-1], None, FLAT)
        return rings

    def tube(self, pts, radii, sides=6, col=None, sh=SMOOTH, cap0=True, cap1=True, tip=0.0, up=(0, 0, 1)):
        if isinstance(radii, (int, float)):
            radii = [radii] * len(pts)
        return self.sweep(pts, circle(sides), radii, radii, col, sh, cap0, cap1, up, tip)


def hface(mb, ids, sh=FLAT, c=None):
    """Height-field face: wound so its world normal points up."""
    ids = list(ids)
    if newell([mb.V[i] for i in ids]).z < 0:
        ids.reverse()
    mb.F.append(ids)
    mb.FC.append(c)
    mb.FS.append(sh)


# ------------------------------------------------------------------------------------------------ finalize
MATS = {}
MAT_NAME = {"VC": "M_KG_DU_VC", "Glow": "M_KG_DU_Glow"}


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
    """Staggered vertical joint positions for one course between x0 and x1 (avoid joints of the course below)."""
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


WALLF = frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))       # local (u, v, w) = (X, Z, Y): wall face at w=0 -> -Y
YZX = frame((0, 0, 0), (0, 1, 0), (0, 0, 1), (1, 0, 0))         # local (u, v, w) = (Y, Z, X)


def arch_side(o, z0, z1, side, n=8):
    """One side (side=-1 left / +1 right) of a round-arched opening between z0 and z1, ascending in z.
    Above the crown it closes to the centre line (xc, z1)."""
    xc, zs_, r = o["xc"], o["zs"], o["r"]
    top = zs_ + r

    def hw(z):
        return r if z <= zs_ else math.sqrt(max(r * r - (z - zs_) ** 2, 0.0))
    zt = min(z1, top)
    zsamp = {z0, zt}
    if z0 < zs_ < zt:
        zsamp.add(zs_)
    for k in range(1, n):
        z = zs_ + r * math.sin(0.5 * math.pi * k / n)
        if max(z0, zs_) < z < zt - 1e-6:
            zsamp.add(z)
    pts = [(xc + side * hw(z), z) for z in sorted(zsamp)]
    if z1 > top + 1e-6:
        pts.append((xc, z1))
    return pts


def arch_outline(o, n=10, grow=0.0):
    xc, zb, zs_, r = o["xc"], o["zb"], o["zs"], o["r"] + grow
    pts = [(xc - r, zb - grow), (xc + r, zb - grow)]
    for k in range(n + 1):
        a = math.pi * k / n
        pts.append((xc + r * math.cos(a), zs_ + r * math.sin(a)))
    return dedupe(pts)


def ashlar(mb, rng, x0, x1, zs, d0, d1, fam, openings=(), bevel=0.03, jl=(0.55, 1.0), first=(0.3, 0.8),
           recess=0.01, colfn=None, arcn=8, min_end=0.3, amt=0.06):
    """Coursed stone blocks in the current local frame (u, v, w): u from x0 to x1, courses at zs (v), front face at
    w=d0 (chamfered), back at d1. Round-arched openings dict(xc, zb, r, zs) are cut out of the courses (an opening's
    bottom must lie on a course line). colfn(local_pos, col, seed) recolours every block vertex."""
    Minv = mb.M.inverted()
    prev = []
    for ci in range(len(zs) - 1):
        z0, z1 = zs[ci], zs[ci + 1]
        ops = [o for o in openings if o["zb"] < z1 - 1e-6 and o["zs"] + o["r"] > z0 + 1e-6]
        js = course_joints(rng, x0, x1, prev, jl[0], jl[1], first=first, min_end=min_end)
        js = [j for j in js if all(abs(j - o["xc"]) > o["r"] + 0.12 for o in ops)]
        js = sorted(set(js + [o["xc"] for o in ops]))
        prev = js
        edges = [x0] + js + [x1]
        for a, b in zip(edges, edges[1:]):
            oa = next((o for o in ops if abs(o["xc"] - a) < 1e-6), None)
            ob = next((o for o in ops if abs(o["xc"] - b) < 1e-6), None)
            right = arch_side(ob, z0, z1, -1, arcn) if ob else [(b, z0), (b, z1)]
            left = list(reversed(arch_side(oa, z0, z1, 1, arcn))) if oa else [(a, z1), (a, z0)]
            poly = dedupe(right + left)
            if len(poly) < 3 or abs(area2(poly)) < 1e-4:
                continue
            poly = inset2(ccw(poly), 0.002)
            s0 = mb.prism(poly, d0 + rng.uniform(0.0, recess), d1, pick(rng, fam, amt), b0=bevel)
            if colfn:
                seed = rng.random() * 50
                mb.recolor(s0, lambda p, c, seed=seed: colfn(Minv @ p, c, seed))


# ---------------------------------------------------------------------------------------------- small props
def pebble(mb, rng, pos, r):
    mb.blob(pos, (r, r * 0.85, r * 0.6), sub=0, amp=0.2, seed=rng.random() * 50, col=pick(rng, PEBBLES, 0.08), sh=FLAT)


def clod(mb, rng, pos, r, col=None):
    col = col or with_a(mix(SOIL, SOIL_LT, rng.random()), 0.0)
    mb.blob(pos, (r, r * 0.9, r * 0.72), sub=0, amp=0.3, seed=rng.random() * 50, col=jit(col, rng, 0.1), sh=FLAT)


def tuft(mb, pos, rng, n=6, h=0.14, spread=0.05, col0=GRASS0, col1=GRASS1):
    """Grass tuft: thin 3-sided blades (from kg_make_dress_wilds.py)."""
    pos = Vector(pos)
    for _ in range(n):
        a = rng.random() * 2 * math.pi
        r = rng.random() * spread
        base = pos + Vector((r * math.cos(a), r * math.sin(a), 0))
        hh = h * rng.uniform(0.55, 1.1)
        lean = Vector((math.cos(a), math.sin(a), 0)) * hh * rng.uniform(0.15, 0.45)
        tipp = base + Vector((0, 0, hh)) + lean
        w = 0.014 + 0.008 * rng.random()
        b = [base + Vector((w * math.cos(a + k * 2.094), w * math.sin(a + k * 2.094), 0)) for k in range(3)]
        ids = [mb.v(p, with_a(col0, 0.0)) for p in b]
        t = mb.v(tipp, with_a(mix(col1, C("C9D16A"), rng.random() * 0.3), 0.0))
        cen = (b[0] + b[1] + b[2] + tipp) / 4
        for k in range(3):
            mb.f_out((ids[k], ids[(k + 1) % 3], t), cen, None, SMOOTH)


def skull(mb, pos, yaw=0.0, s=1.0, pitch=0.0, roll=0.0, lod=0, seed=0.0, col=IVORY):
    """Stylised skull, local front = -Y, origin at the jaw bottom, ~0.14 wide x 0.18 deep x 0.17 tall (s=1)."""
    dk = mix(col, IVORY_DK, 0.7)
    with mb.xf(T(*pos) @ R(yaw, 'Z') @ R(roll, 'Y') @ R(pitch, 'X') @ S(s)):
        mb.blob((0, 0.015, 0.1), (0.07, 0.086, 0.072), sub=1, amp=0.04, seed=seed, col=col, sh=AUTO)
        if lod == 0:
            mb.cbox((0, -0.045, 0.045), (0.096, 0.07, 0.09), 0.02, dk)
            for sx in (-1, 1):
                mb.blob((sx * 0.029, -0.079, 0.088), (0.022, 0.012, 0.02), sub=0, amp=0.05, col=SOCKET, sh=FLAT)
            mb.box((0, -0.081, 0.026), (0.066, 0.008, 0.007), SOCKET)
        else:
            mb.box((0, -0.045, 0.045), (0.094, 0.07, 0.09), dk)
            for sx in (-1, 1):
                mb.blob((sx * 0.029, -0.079, 0.088), (0.021, 0.012, 0.019), sub=0, amp=0.05, col=SOCKET, sh=FLAT)
            mb.box((0, -0.081, 0.027), (0.06, 0.006, 0.007), SOCKET)
        mb.box((0, -0.083, 0.056), (0.018, 0.01, 0.022), SOCKET)


def bone(mb, p0, p1, r=0.016, col=IVORY):
    """Long bone: shaft + a knob at each end (knobs flattened across the bone)."""
    p0, p1 = Vector(p0), Vector(p1)
    d = (p1 - p0).normalized()
    side = d.cross(Vector((0, 0, 1)))
    side = side.normalized() if side.length > 1e-6 else Vector((1, 0, 0))
    up = side.cross(d).normalized()
    rot = Matrix((side, d, up)).transposed()
    mb.cyl(p0 + d * r, p1 - d * r, r, seg=6, col=col, caps=False, sh=AUTO)
    for p in (p0 + d * r * 0.6, p1 - d * r * 0.6):
        mb.blob(p, (r * 2.0, r * 1.3, r * 1.35), sub=0, amp=0.06, col=col, sh=FLAT, rot=rot)


def flame(mb, pos, h=0.03, r=0.009):
    prof = [(0, 0), (r * 0.7, 0.12 * h), (r, 0.32 * h), (r * 0.85, 0.52 * h), (r * 0.5, 0.75 * h), (0, h)]
    with mb.xf(T(*pos)):
        mb.lathe(prof, 6, colfn=lambda rr, z, a, i: mix(FLAME_Y, FLAME_O, smoothstep(0.2 * h, h, z)), sh=SMOOTH)


def star4(mb, size, col, thick=0.004):
    R_, r = size / 2, size * 0.1
    pts = []
    for k in range(8):
        a = math.pi / 4 * k
        rr = R_ if k % 2 == 0 else r
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    mb.ext2d(pts, -thick / 2, thick / 2, col, topcol=col)


# ---------------------------------------------------------------------------------------------- crater surfaces
def crater(mb, rings, seg, p=2.0, colfn=None, rn=0.03, zn=0.02, seed=0.0, sh=FLAT, phase=0.0):
    """Open height-field surface of rings (outer -> inner): (hx, hy, z, t, wr, wz). Ring shape is a superellipse of
    exponent p (2 = ellipse); wr/wz weight the radial / height noise; t (0 rim .. 1 centre) feeds colfn(x, y, z, t).
    hx == 0 -> a centre vertex."""
    ids = []
    ex = 2.0 / p
    for i, (hx, hy, z, t, wr, wz) in enumerate(rings):
        if hx < 1e-6:
            ids.append(mb.v((0.0, 0.0, z), colfn(0.0, 0.0, z, t)))
            continue
        ring = []
        for j in range(seg):
            a = phase + 2 * math.pi * j / seg
            ca, sa = math.cos(a), math.sin(a)
            q = Vector((ca * 1.3, sa * 1.3, seed + 0.45 * i))
            d = rn * wr * nz(q, 1.0, (seed, 1.7, 0.3))
            x = (hx + d) * math.copysign(abs(ca) ** ex, ca)
            y = (hy + d) * math.copysign(abs(sa) ** ex, sa)
            zz = max(0.0, z + zn * wz * nz(q, 1.6, (4.1, seed, 2.2)))
            ring.append(mb.v((x, y, zz), colfn(x, y, zz, t)))
        ids.append(ring)
    for i in range(len(ids) - 1):
        A, B = ids[i], ids[i + 1]
        for j in range(seg):
            jn = (j + 1) % seg
            if isinstance(B, int):
                hface(mb, (A[j], A[jn], B), sh)
            else:
                hface(mb, (A[j], A[jn], B[jn], B[j]), sh)


def pit_colfn(seed, dark, start=0.12, dry=0.3):
    def fn(x, y, z, t):
        p = Vector((x, y, z))
        c = soil_col(p, seed, dry * (1.0 - smoothstep(0.0, 0.15, t)))
        c = mix(c, SOIL_DK, smoothstep(start, start + 0.4, t))
        c = mix(c, PIT, dark * smoothstep(start + 0.2, 0.95, t))
        return with_a(c, 0.0)
    return fn


def hole_rings(R_, h, rc, rp):
    return [(R_, R_, 0.0, 0.0, 0.5, 0.0),
            (R_ - (R_ - rc) * 0.5, R_ - (R_ - rc) * 0.5, h * 0.62, 0.0, 1.0, 1.0),
            (rc + 0.015, rc + 0.015, h * 0.97, 0.04, 1.0, 0.5),
            (rc - 0.035, rc - 0.035, h * 0.93, 0.12, 1.0, 0.4),
            (rp + (rc - rp) * 0.5, rp + (rc - rp) * 0.5, h * 0.5, 0.35, 0.8, 0.3),
            (rp + 0.025, rp + 0.025, h * 0.14 + 0.015, 0.6, 0.4, 0.0),
            (rp, rp, 0.015, 0.7, 0.2, 0.0),
            (rp * 0.66, rp * 0.66, 0.015, 0.85, 0.0, 0.0),
            (rp * 0.33, rp * 0.33, 0.015, 0.95, 0.0, 0.0),
            (0.0, 0.0, 0.015, 1.0, 0.0, 0.0)]


def lump_fns(n, amp_r=0.05, amp_z=0.02, seed=0.0):
    rfn = lambda a, i, z: 1.0 + (amp_r * nz((1.4 * math.cos(a), 1.4 * math.sin(a), 0.5 * i), 1.0, (seed, 1, 2))  # noqa: E731
                                 if 0 < i < n - 1 else 0.0)
    zfn = lambda r, a, i, z: (amp_z * nz((2 * math.cos(a), 2 * math.sin(a), 0.3 * i), 1.0, (9, seed, 1))  # noqa: E731
                              if 1 < i < n - 1 else 0.0)
    return rfn, zfn


# ================================================================================================ dig props
MOUND = [(0, 0), (0.6, 0), (0.575, 0.035), (0.5, 0.095), (0.39, 0.165), (0.26, 0.226), (0.13, 0.266), (0, 0.278)]


def make_dig_mound():
    mb = MB("SM_KG_DigMound", angle=40)
    rng = random.Random(101)
    rfn, zfn = lump_fns(len(MOUND), 0.04, 0.02, 3.0)
    with mb.xf(UE):
        mb.lathe(MOUND, 16, colfn=lambda r, z, a, i: soil_col(Vector((r * math.cos(a), r * math.sin(a), z)), 1.0,
                                                               0.3 * smoothstep(0.12, 0.28, z)),
                 sh=FLAT, rfn=rfn, zfn=zfn)
        for k in range(7):
            a, r = rng.uniform(0, 6.28), rng.uniform(0.1, 0.48)
            clod(mb, rng, (r * math.cos(a), r * math.sin(a), prof_h(MOUND, r) + 0.005), rng.uniform(0.035, 0.06))
        for k in range(5):
            a, r = rng.uniform(0, 6.28), rng.uniform(0.15, 0.55)
            pebble(mb, rng, (r * math.cos(a), r * math.sin(a), prof_h(MOUND, r) + 0.004), rng.uniform(0.018, 0.03))
        for k in range(4):
            a = k * 1.57 + rng.uniform(-0.4, 0.4)
            tuft(mb, (0.54 * math.cos(a), 0.54 * math.sin(a), 0.0), rng, n=6, h=0.13, spread=0.04)
    return mb


def chipped_strip(rng, L, w, n=7):
    top, bot = [], []
    for k in range(n + 1):
        x = -L / 2 + L * k / n
        ct = rng.choice((0.0, 0.0, 0.018, 0.03)) if 0 < k < n else 0.0
        cb = rng.choice((0.0, 0.0, 0.018, 0.03)) if 0 < k < n else 0.0
        top.append((x, w / 2 - ct))
        bot.append((x, -w / 2 + cb))
    bot[0] = (-L / 2, -w / 2 + rng.uniform(0.01, 0.04))
    top[-1] = (L / 2, w / 2 - rng.uniform(0.01, 0.04))
    return [(-L / 2 + 0.03, -w / 2)] + bot[1:] + [(L / 2, -w / 2 + 0.02)] + top[::-1][:-1] + [(-L / 2, w / 2 - 0.02)] \
        + [bot[0]]


def make_dig_x():
    mb = MB("SM_KG_DigX", angle=40)
    rng = random.Random(102)
    patch = [(0, 0), (0.65, 0), (0.6, 0.012), (0.45, 0.022), (0.25, 0.028), (0, 0.03)]
    rfn, _ = lump_fns(len(patch), 0.03, 0.0, 5.0)
    with mb.xf(UE):
        mb.lathe(patch, 14, colfn=lambda r, z, a, i: soil_col(Vector((r * math.cos(a), r * math.sin(a), z)), 2.0, 0.35),
                 sh=FLAT, rfn=rfn)
        z0, z1 = 0.014, 0.036

        def strip(L, w, at, ang):
            col = jit(PAINT, rng, 0.06)
            with mb.xf(R(ang, 'Z') @ T(at, 0, 0)):
                mb.ext2d(chipped_strip(rng, L, w), z0, z1, mix(col, PAINT_DK, 0.35), topcol=col)
                for _ in range(3):
                    cx, cy, s = rng.uniform(-L / 2 + 0.05, L / 2 - 0.05), rng.uniform(-w / 3, w / 3), \
                        rng.uniform(0.018, 0.035)
                    mb.ext2d([(cx - s, cy - s * 0.6), (cx + s * 0.8, cy - s * 0.8), (cx + s, cy + s * 0.5),
                              (cx - s * 0.4, cy + s * 0.7)], z1 - 0.001, z1 + 0.002, CHIP, topcol=CHIP)
        strip(1.1, 0.16, 0.0, 45)
        for sgn in (-1, 1):
            strip(0.465, 0.16, sgn * (0.085 + 0.465 / 2), -45)
        # stake with a red rag
        sx, sy = 0.44, 0.42
        with mb.xf(T(sx, sy, 0) @ R(4, 'X')):
            mb.cbox((0, 0, 0.235), (0.045, 0.045, 0.47), 0.008, WOOD)
            mb.lathe([(0, 0.47), (0.032, 0.47), (0, 0.5)], 4, col=WOOD_LT, sh=FLAT, phase=math.pi / 4)
            mb.blob((0.0, -0.026, 0.43), (0.03, 0.02, 0.025), sub=0, amp=0.1, col=PAINT_DK, sh=FLAT)
            with mb.xf(R(-35, 'Z') @ frame((0, -0.02, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))):
                mb.ext2d([(0.0, 0.45), (0.15, 0.435), (0.17, 0.37), (0.11, 0.3), (0.06, 0.34), (0.01, 0.4)],
                         -0.003, 0.003, PAINT, topcol=jit(PAINT, rng, 0.05))
        for k in range(3):
            a = rng.uniform(0, 6.28)
            clod(mb, rng, (0.55 * math.cos(a), 0.55 * math.sin(a), 0.01), rng.uniform(0.025, 0.04))
    return mb


def make_dig_glint():
    mb = MB("SM_KG_DigGlint", angle=40, material="Glow")
    rng = random.Random(103)
    patch = [(0, 0), (0.4, 0), (0.36, 0.018), (0.26, 0.04), (0.13, 0.055), (0, 0.06)]
    rfn, zfn = lump_fns(len(patch), 0.05, 0.01, 7.0)
    with mb.xf(UE):
        mb.lathe(patch, 12, colfn=lambda r, z, a, i: soil_col(Vector((r * math.cos(a), r * math.sin(a), z)), 3.0, 0.2),
                 sh=FLAT, rfn=rfn, zfn=zfn)
        for k in range(5):
            a, r = rng.uniform(0, 6.28), rng.uniform(0.15, 0.33)
            clod(mb, rng, (r * math.cos(a), r * math.sin(a), prof_h(patch, r)), rng.uniform(0.022, 0.04))
        # half-buried coin (tilted towards -Y) + a second coin + a ring
        ax = Vector((0.15, -0.85, 0.5)).normalized()
        c = Vector((0.03, -0.02, 0.058))
        mb.cyl(c - ax * 0.007, c + ax * 0.007, 0.056, seg=14, col=GOLD, capcol=GOLD_HI, sh=FLAT)
        with mb.xf(T(c.x, c.y, c.z)):
            pass
        ax2 = Vector((-0.3, 0.2, 0.93)).normalized()
        c2 = Vector((0.13, 0.08, 0.048))
        mb.cyl(c2 - ax2 * 0.005, c2 + ax2 * 0.005, 0.036, seg=10, col=GOLD_DK, capcol=GOLD, sh=FLAT)
        with mb.xf(T(-0.09, 0.05, 0.05) @ R(65, 'X') @ R(20, 'Y')):
            mb.torus(0.03, 0.008, 12, 5, col=GOLD, sh=SMOOTH)
        # four-pointed glints (emissive)
        for size, pos, yaw, pitch in ((0.14, (0.02, -0.06, 0.2), 10, 80), (0.1, (-0.12, 0.05, 0.28), 70, 65),
                                      (0.08, (0.16, 0.06, 0.12), -40, 85), (0.12, (-0.04, 0.12, 0.33), 130, 70)):
            with mb.xf(T(*pos) @ R(yaw, 'Z') @ R(pitch, 'X') @ R(rng.uniform(0, 45), 'Z')):
                star4(mb, size, GLINT)
    return mb


HOLES = {1: dict(R=0.65, h=0.12, rc=0.5, rp=0.35, dark=0.7, seg=16, clods=5, peb=3),
         2: dict(R=0.75, h=0.22, rc=0.58, rp=0.42, dark=0.88, seg=18, clods=7, peb=4),
         3: dict(R=0.85, h=0.32, rc=0.66, rp=0.5, dark=1.0, seg=20, clods=9, peb=5)}


def rim_clods(mb, rng, rc, h, n, k=1.0):
    for i in range(n):
        a = 2 * math.pi * (i + rng.uniform(0.1, 0.9)) / n
        r = rc + rng.uniform(-0.03, 0.05)
        clod(mb, rng, (r * math.cos(a), r * math.sin(a), h * 0.88), rng.uniform(0.03, 0.055) * k)


def make_dig_hole(stage):
    P = HOLES[stage]
    mb = MB(f"SM_KG_DigHole_{stage}", angle=40)
    rng = random.Random(110 + stage)
    with mb.xf(UE):
        crater(mb, hole_rings(P["R"], P["h"], P["rc"], P["rp"]), P["seg"], 2.0,
               pit_colfn(stage * 3.0, P["dark"], start=0.12 if stage < 3 else 0.06), rn=0.035, zn=0.02 + 0.01 * stage,
               seed=stage * 1.3)
        rim_clods(mb, rng, P["rc"], P["h"], P["clods"], 0.9 + 0.1 * stage)
        for k in range(P["peb"]):
            a = rng.uniform(0, 6.28)
            r = rng.uniform(P["rc"] + 0.02, P["R"] - 0.06)
            pebble(mb, rng, (r * math.cos(a), r * math.sin(a), P["h"] * 0.45), rng.uniform(0.018, 0.03))
        if stage == 3:
            for k, a in enumerate((0.7, 2.6, 4.4)):
                ca, sa = math.cos(a), math.sin(a)
                rc, h = P["rc"], P["h"]
                pts = [(rc + 0.02, 0.7 * h), (rc - 0.06, 0.62 * h), (rc - 0.12, 0.45 * h), (rc - 0.15, 0.28 * h),
                       (rc - 0.14, 0.18 * h)]
                pts = [Vector((r * math.cos(a + 0.05 * i), r * math.sin(a + 0.05 * i), z)) for i, (r, z) in enumerate(pts)]
                mb.tube(pts, [0.014, 0.012, 0.009, 0.006, 0.004], 5, col=jit(ROOT_C, rng, 0.08), sh=SMOOTH, tip=0.02,
                        cap1=False)
                mb.tube([pts[1], pts[1] + Vector((-0.04 * ca + 0.03 * sa, -0.04 * sa - 0.03 * ca, -0.04))], 0.004, 4,
                        col=ROOT_C, sh=SMOOTH)
    return mb


PILE = [(0, 0), (0.5, 0), (0.47, 0.05), (0.4, 0.15), (0.3, 0.28), (0.19, 0.39), (0.08, 0.47), (0, 0.5)]


def make_dirt_pile():
    mb = MB("SM_KG_DirtPile", angle=40)
    rng = random.Random(106)
    rfn, zfn = lump_fns(len(PILE), 0.06, 0.03, 11.0)
    with mb.xf(UE):
        mb.lathe(PILE, 14, colfn=lambda r, z, a, i: soil_col(Vector((r * math.cos(a), r * math.sin(a), z)), 4.0,
                                                              0.25 * smoothstep(0.2, 0.5, z)),
                 sh=FLAT, rfn=rfn, zfn=zfn)
        for k in range(11):
            a, r = rng.uniform(0, 6.28), rng.uniform(0.08, 0.44)
            clod(mb, rng, (r * math.cos(a), r * math.sin(a), max(prof_h(PILE, r) - 0.01, 0.03)), rng.uniform(0.035, 0.07))
        for k in range(3):
            a = rng.uniform(0, 6.28)
            pebble(mb, rng, (0.45 * math.cos(a), 0.45 * math.sin(a), 0.03), rng.uniform(0.02, 0.03))
    return mb


def grave_rings(hx, hy, h):
    """Rounded-rect grave rim: outer half extents hx, hy; pit floor at 0.015."""
    rim = 0.1 + 0.3 * h
    return [(hx, hy, 0.0, 0.0, 0.3, 0.0),
            (hx - rim * 0.3, hy - rim * 0.3, h * 0.6, 0.0, 1.0, 1.0),
            (hx - rim * 0.6, hy - rim * 0.6, h, 0.06, 1.0, 0.5),
            (hx - rim * 0.85, hy - rim * 0.85, h * 0.66, 0.3, 0.6, 0.3),
            (hx - rim, hy - rim, h * 0.2 + 0.01, 0.55, 0.2, 0.0),
            (hx - rim - 0.02, hy - rim - 0.02, 0.015, 0.7, 0.0, 0.0),
            ((hx - rim) * 0.66, (hy - rim) * 0.55, 0.015, 0.85, 0.0, 0.0),
            ((hx - rim) * 0.33, (hy - rim) * 0.2, 0.015, 0.95, 0.0, 0.0),
            (0.0, 0.0, 0.015, 1.0, 0.0, 0.0)]


def make_grave_open(stage):
    mb = MB(f"SM_KG_GraveOpen_{stage}", angle=40)
    rng = random.Random(120 + stage)
    hx, hy, h = (1.1, 0.5, 0.15) if stage == 1 else (1.2, 0.6, 0.3)
    rings = grave_rings(hx, hy, h)
    with mb.xf(UE):
        crater(mb, rings, 28, 5.0, pit_colfn(20.0 + stage, 0.85 if stage == 1 else 1.0, start=0.1), rn=0.03,
               zn=0.015 + 0.02 * stage, seed=stage * 2.1, phase=math.pi / 28)
        crest_x, crest_y = rings[2][0], rings[2][1]
        for k in range(8 + 4 * stage):
            t = rng.uniform(0, 1)
            if rng.random() < 0.65:
                x, y = rng.uniform(-crest_x, crest_x), rng.choice((-1, 1)) * crest_y
            else:
                x, y = rng.choice((-1, 1)) * crest_x, rng.uniform(-crest_y, crest_y)
            clod(mb, rng, (x, y, h * 0.85), rng.uniform(0.03, 0.06))
        if stage == 2:
            # dug-out heap along the +Y long side
            heap = [(0, 0), (1.0, 0), (0.92, 0.12), (0.74, 0.28), (0.5, 0.42), (0.22, 0.495), (0, 0.5)]
            rfn, zfn = lump_fns(len(heap), 0.06, 0.03, 13.0)
            with mb.xf(T(0.0, 0.8, 0.0) @ S(1.08, 0.33, 1.0)):
                mb.lathe(heap, 18, colfn=lambda r, z, a, i: soil_col(Vector((r * math.cos(a), r * math.sin(a), z)),
                                                                      6.0, 0.25 * smoothstep(0.2, 0.5, z)),
                         sh=FLAT, rfn=rfn, zfn=zfn)
            for k in range(9):
                x = rng.uniform(-0.9, 0.9)
                y = 0.8 + rng.uniform(-0.2, 0.2)
                zt = 0.5 * (1.0 - (abs(x) / 1.08) ** 2) - 3.0 * (y - 0.8) ** 2
                clod(mb, rng, (x, y, max(zt, 0.05)), rng.uniform(0.04, 0.07))
            # cracked coffin lid inside, slightly askew
            A = [(-0.8, -0.17), (-0.38, -0.24), (0.06, -0.2064), (0.12, -0.1), (0.03, 0.02), (0.14, 0.13),
                 (0.1, 0.2034), (-0.38, 0.24), (-0.8, 0.17)]
            B = [(0.06, -0.2064), (0.8, -0.15), (0.8, 0.15), (0.1, 0.2034), (0.14, 0.13), (0.03, 0.02), (0.12, -0.1)]
            lid = C("6B4A30")
            with mb.xf(T(0.04, -0.02, 0.02) @ R(6, 'Z')):
                mb.ext2d(inset2(ccw(A), 0.008), 0.0, 0.05, mix(lid, WOOD_DK, 0.4), topcol=lid)
                with mb.xf(T(0.12, 0.0, -0.006) @ R(-2.5, 'X') @ R(3, 'Z') @ T(-0.12, 0, 0)):
                    mb.ext2d(inset2(ccw(B), 0.008), 0.0, 0.05, mix(lid, WOOD_DK, 0.4), topcol=jit(lid, rng, 0.08))
                cross = C("8A6A48")
                mb.box((-0.42, 0.0, 0.056), (0.5, 0.05, 0.012), cross)
                mb.box((-0.52, 0.0, 0.056), (0.05, 0.26, 0.012), cross)
                for p in ((0.4, 0.05), (-0.1, -0.12), (0.55, -0.08)):
                    clod(mb, rng, (p[0], p[1], 0.055), rng.uniform(0.025, 0.04))
    return mb


def make_buried_chest():
    mb = MB("SM_KG_BuriedChest", angle=35)
    rng = random.Random(130)
    with mb.xf(UE):
        crater(mb, hole_rings(0.8, 0.26, 0.64, 0.5), 20, 2.0, pit_colfn(31.0, 0.8, start=0.1), rn=0.035, zn=0.03,
               seed=4.4)
        rim_clods(mb, rng, 0.64, 0.26, 8)
        with mb.xf(R(10, 'Z')):
            # body: three plank courses, visible from the pit floor up to 0.24
            for k, (za, zb) in enumerate(((0.005, 0.085), (0.085, 0.165), (0.165, 0.24))):
                mb.cbox((0, 0, (za + zb) / 2), (0.7, 0.45, zb - za), 0.012, jit(mix(WOOD_DK, TIMBER, rng.random()), rng))
            for sx in (-1, 1):
                x = sx * 0.24
                mb.box((x, -0.229, 0.125), (0.055, 0.012, 0.235), IRON)
                mb.box((x, 0.229, 0.125), (0.055, 0.012, 0.235), IRON)
                for sy in (-1, 1):
                    mb.cbox((sx * 0.345, sy * 0.22, 0.225), (0.03, 0.03, 0.04), 0.006, IRON_LT)
            mb.cbox((0, -0.232, 0.19), (0.08, 0.02, 0.1), 0.006, GOLD_DK)
            mb.box((0, -0.244, 0.18), (0.016, 0.006, 0.03), IRON)
            # soil heaped against the chest
            for k in range(9):
                a = 2 * math.pi * k / 9 + rng.uniform(-0.2, 0.2)
                clod(mb, rng, (0.4 * math.cos(a), 0.27 * math.sin(a), 0.03), rng.uniform(0.035, 0.06),
                     col=with_a(mix(SOIL_DK, SOIL, rng.random()), 0.0))
            # gold heap inside + loose coins + a goblet
            mb.blob((0, 0.0, 0.215), (0.3, 0.18, 0.07), sub=1, amp=0.18, seed=3.0, col=GOLD, sh=FLAT)
            for k in range(6):
                x, y = rng.uniform(-0.25, 0.25), rng.uniform(-0.14, 0.14)
                z = 0.215 + 0.07 * math.sqrt(max(0.0, 1 - (x / 0.3) ** 2 - (y / 0.18) ** 2)) - 0.004
                with mb.xf(T(x, y, z) @ R(rng.uniform(-25, 25), 'X') @ R(rng.uniform(-25, 25), 'Y')):
                    mb.lathe([(0, -0.003), (0.024, -0.003), (0.024, 0.003), (0, 0.003)], 8,
                             col=jit(GOLD_HI, rng, 0.08), sh=FLAT)
            with mb.xf(T(0.12, 0.03, 0.25) @ R(-12, 'Y')):
                mb.lathe([(0, 0), (0.035, 0), (0.035, 0.008), (0.009, 0.02), (0.009, 0.06), (0.03, 0.075),
                          (0.042, 0.11), (0.036, 0.112), (0.02, 0.085), (0, 0.082)], 8, col=GOLD_DK, sh=FLAT)
            # lid, hinged at the back top edge, open ~100 degrees
            with mb.xf(T(0, 0.225, 0.24) @ R(-100, 'X') @ T(0, -0.225, 0)):
                poly = [(-0.225, 0.0), (0.225, 0.0)] + [(0.225 * math.cos(math.radians(d)), 0.15 * math.sin(math.radians(d)))
                                                        for d in (25, 50, 75, 105, 130, 155)]
                with mb.xf(YZX):
                    mb.prism(poly, -0.35, 0.35, jit(WOOD_DK, rng, 0.04), b0=0.012, b1=0.012)
                    big = [(u * 1.05, v * 1.1 - 0.004) for u, v in poly]
                    for x in (-0.24, 0.24):
                        mb.prism(big, x - 0.028, x + 0.028, IRON, sh=FLAT)
                mb.cbox((0, 0, -0.004), (0.64, 0.4, 0.01), 0.003, WOOD)
                mb.cbox((0, -0.225, 0.04), (0.07, 0.02, 0.06), 0.005, GOLD_DK)
        for k in range(4):
            a, r = rng.uniform(0, 6.28), rng.uniform(0.42, 0.5)
            with mb.xf(T(r * math.cos(a), r * math.sin(a), 0.02) @ R(rng.uniform(-10, 10), 'X')):
                mb.lathe([(0, 0), (0.024, 0), (0.024, 0.006), (0, 0.006)], 8, col=GOLD, sh=FLAT)
    return mb


# ================================================================================================ catacomb kit
def cata_colfn(damp_top=0.55, ao_depth=0.4):
    def fn(l, c, seed):
        u, v, w = l
        c = mul(c, 1.0 + 0.06 * nz(l, 3.0, (seed, 1, 2)))
        damp = smoothstep(damp_top, 0.05, v)
        c = mix(c, CATA_DAMP, 0.55 * damp * (0.6 + 0.4 * nz(l, 2.5, (seed, 4, 4))))
        moss = smoothstep(0.22, 0.0, v) * smoothstep(-0.1, 0.4, nz(l, 3.0, (7, seed, 1)))
        c = mix(c, MOSS_DK, 0.65 * moss)
        if ao_depth:
            c = mul(c, lerp(1.0, 0.42, smoothstep(0.03, ao_depth, w)))
        return with_a(c, 0.0)
    return fn


def make_niche_wall():
    mb = MB("SM_KG_NicheWall_2m", angle=30)
    rng = random.Random(140)
    ops = [dict(xc=xc, zb=zb, r=0.35, zs=zb + 0.2) for zb in (0.5, 1.45) for xc in (-0.5, 0.5)]
    zs = [0.0, 0.26, 0.5, 1.2, 1.45, 2.15, 2.5, 2.82, 3.12]
    colfn = cata_colfn()
    with mb.xf(UE):
        with mb.xf(WALLF):
            ashlar(mb, rng, -1.0, 1.0, zs, 0.0, 0.5, CATA, ops, bevel=0.03, colfn=colfn, jl=(0.55, 0.95), arcn=5)
            Minv = mb.M.inverted()
            for o in ops:
                xc, zb, zs_ = o["xc"], o["zb"], o["zs"]
                s0 = mb.prism(arch_outline(o, 5), 0.38, 0.5, pick(rng, CATA))
                mb.recolor(s0, lambda p, c: colfn(Minv @ p, c, 9.0))
                # proud frame: voussoir ring, jambs, sill
                n = 5
                for k in range(n):
                    a0, a1 = math.pi * k / n, math.pi * (k + 1) / n
                    ro = 0.45 + (0.03 if k == n // 2 else 0.0)
                    am = (a0 + a1) / 2
                    poly = [(xc + 0.35 * math.cos(a), zs_ + 0.35 * math.sin(a)) for a in (a0, a1)] + \
                           [(xc + ro * math.cos(a), zs_ + ro * math.sin(a)) for a in (a1, a0)]
                    s0 = mb.prism(inset2(ccw(poly), 0.003), -0.025, 0.0, pick(rng, FRAME), b0=0.012)
                    mb.recolor(s0, lambda p, c: colfn(Minv @ p, c, 3.0))
                for sx in (-1, 1):
                    poly = [(xc + sx * 0.35, zb), (xc + sx * 0.45, zb), (xc + sx * 0.45, zs_ - 0.003),
                            (xc + sx * 0.35, zs_ - 0.003)]
                    mb.prism(ccw(poly), -0.025, 0.0, pick(rng, FRAME), b0=0.012)
                s0 = mb.prism([(xc - 0.47, zb - 0.07), (xc + 0.47, zb - 0.07), (xc + 0.47, zb), (xc - 0.47, zb)],
                              -0.045, 0.0, pick(rng, FRAME), b0=0.014)
                mb.recolor(s0, lambda p, c: colfn(Minv @ p, c, 5.0))
        # contents: long bones at the back, skulls in front (UE coords; niche floor at zb, depth Y 0..0.38)
        for k, o in enumerate(ops):
            xc, zb = o["xc"], o["zb"]
            nb = 2 if k % 2 == 0 else 1
            for b in range(nb):
                L = rng.uniform(0.4, 0.5)
                y = 0.31 - 0.065 * b
                ang = math.radians(rng.uniform(-7, 7))
                dx = rng.uniform(-0.04, 0.04)
                p0 = (xc + dx - L / 2 * math.cos(ang), y - L / 2 * math.sin(ang), zb + 0.021)
                p1 = (xc + dx + L / 2 * math.cos(ang), y + L / 2 * math.sin(ang), zb + 0.021)
                bone(mb, p0, p1, 0.017, jit(IVORY, rng, 0.05))
            L = rng.uniform(0.38, 0.46)
            bone(mb, (xc - L / 2, 0.285, zb + 0.056), (xc + L / 2, 0.275, zb + 0.056), 0.016, jit(IVORY, rng, 0.05))
            if k in (0, 3):
                skull(mb, (xc - 0.14, 0.13, zb), rng.uniform(-18, -5), rng.uniform(0.95, 1.05), lod=1, seed=k * 3.0)
                skull(mb, (xc + 0.15, 0.12, zb), rng.uniform(5, 20), rng.uniform(0.92, 1.0), lod=1, seed=k * 3.0 + 1)
            else:
                skull(mb, (xc + rng.uniform(-0.05, 0.05), 0.13, zb), rng.uniform(-12, 12), 1.02, lod=1, seed=k * 3.0)
    return mb


VR = (1.25 ** 2 + 0.75 ** 2) / 1.5          # barrel vault: chord 2.5, rise 0.75 -> radius 1.4167
VCZ = 0.75 - VR


def vault_pt(rad, t):
    y, z = rad * math.sin(t), VCZ + rad * math.cos(t)
    return (clamp(y, -1.25, 1.25), max(z, 0.0))


def make_crypt_vault():
    mb = MB("SM_KG_CryptVault_2m", angle=30)
    rng = random.Random(150)
    th = math.asin(1.25 / VR)
    nt = 12
    ts = [-th + 2 * th * i / nt for i in range(nt + 1)]
    with mb.xf(UE):
        with mb.xf(YZX):                                   # local (u, v, w) = (Y, Z, X)
            prev = []
            for i in range(nt):
                t0, t1 = ts[i], ts[i + 1]
                poly = dedupe([vault_pt(VR, t0), vault_pt(VR, t1), vault_pt(VR + 0.25, t1), vault_pt(VR + 0.25, t0)])
                js = course_joints(rng, -1.0, 1.0, prev, 0.55, 0.95, first=(0.3, 0.8))
                prev = js
                edges = [-1.0] + js + [1.0]
                for a, b in zip(edges, edges[1:]):
                    col = mul(pick(rng, VAULT), 1.0 + 0.08 * math.cos(0.5 * (t0 + t1) * 2.0))
                    mb.prism(inset2(ccw(poly), 0.002), a + 0.002, b - 0.002, col, b0=0.02, b1=0.02)
            # raised voussoir ribs (4 cm below the intrados) at X -1..-0.8 and 0..0.2
            thr = math.acos(-VCZ / (VR - 0.04))
            for x0 in (-1.0, 0.0):
                n = 7
                for k in range(n):
                    t0, t1 = -thr + 2 * thr * k / n, -thr + 2 * thr * (k + 1) / n
                    ro = VR + 0.1 + (0.03 if k == n // 2 else 0.0)
                    poly = dedupe([vault_pt(VR - 0.04, t0), vault_pt(VR - 0.04, (t0 + t1) / 2), vault_pt(VR - 0.04, t1),
                                   vault_pt(ro, t1), vault_pt(ro, t0)])
                    mb.prism(inset2(ccw(poly), 0.003), x0 + 0.004, x0 + 0.196, pick(rng, FRAME, 0.05), b0=0.018,
                             b1=0.018)
            # impost cornice on both wall tops (7 cm proud of the 2 m corridor walls)
            for sy in (-1, 1):
                prof = [(1.25, 0.0), (0.97, 0.0), (0.93, 0.035), (0.93, 0.1), (0.965, 0.14), (1.25, 0.14)]
                prof = [(sy * u, v) for u, v in prof]
                for a, b in ((-1.0, 0.0), (0.0, 1.0)):
                    mb.prism(ccw(prof), a + 0.002, b - 0.002, pick(rng, FRAME, 0.04), b0=0.012, b1=0.012)
    return mb


def make_skull_pile():
    mb = MB("SM_KG_SkullPile", angle=35)
    rng = random.Random(160)
    base = [(0, 0), (0.45, 0), (0.42, 0.04), (0.33, 0.1), (0.2, 0.15), (0, 0.17)]
    rfn, zfn = lump_fns(len(base), 0.06, 0.015, 2.0)
    with mb.xf(UE):
        with mb.xf(S(1.0, 0.6, 1.0)):
            mb.lathe(base, 12, colfn=lambda r, z, a, i: with_a(mix(DUST_DK, DUST, 0.4 + 0.5 * nz((r * math.cos(a),
                                                               r * math.sin(a), z), 6.0)), 0.0), sh=FLAT, rfn=rfn, zfn=zfn)
        mb.blob((0, 0.03, 0.18), (0.27, 0.14, 0.2), sub=1, amp=0.12, seed=2.0, col=mix(DUST_DK, IVORY_DK, 0.3), sh=FLAT)
        for p0, p1 in (((-0.44, -0.02, 0.03), (-0.1, 0.1, 0.24)), ((0.43, 0.06, 0.03), (0.12, -0.05, 0.3)),
                       ((-0.25, -0.2, 0.02), (0.15, -0.22, 0.03)), ((0.05, 0.22, 0.03), (0.38, 0.1, 0.1)),
                       ((-0.05, 0.08, 0.45), (0.12, 0.2, 0.2))):
            bone(mb, p0, p1, 0.017, jit(IVORY, rng, 0.06))
        spots = [((-0.33, -0.12, 0.03), 0), ((-0.11, -0.15, 0.07), 0), ((0.11, -0.15, 0.07), 0), ((0.33, -0.12, 0.03), 0),
                 ((-0.22, 0.13, 0.05), 180), ((0.0, 0.15, 0.08), 180), ((0.22, 0.13, 0.05), 180),
                 ((-0.26, -0.03, 0.17), 0), ((-0.09, -0.06, 0.2), 0), ((0.09, -0.05, 0.2), 0), ((0.27, -0.02, 0.17), 0),
                 ((0.0, 0.02, 0.36), 0)]
        for k, (p, yaw) in enumerate(spots):
            skull(mb, p, yaw + rng.uniform(-22, 22), rng.uniform(0.95, 1.08), pitch=rng.uniform(-10, 12),
                  roll=rng.uniform(-10, 10), lod=0 if k in (1, 2, 11) else 1, seed=k * 1.7,
                  col=jit(IVORY, rng, 0.05))
    return mb


def make_bone_scatter():
    mb = MB("SM_KG_BoneScatter", angle=35)
    rng = random.Random(170)
    with mb.xf(UE):
        for k, (x, y, ang, L) in enumerate(((-0.35, -0.2, 20, 0.44), (-0.1, 0.25, -35, 0.4), (0.3, 0.05, 80, 0.42),
                                            (0.1, -0.3, 5, 0.3), (-0.45, 0.2, 100, 0.34), (0.42, -0.3, -50, 0.28),
                                            (0.2, 0.35, 15, 0.36))):
            a = math.radians(ang)
            r = 0.016 if L > 0.35 else 0.013
            z = r + (0.028 if k == 6 else 0.0)
            bone(mb, (x - L / 2 * math.cos(a), y - L / 2 * math.sin(a), z), (x + L / 2 * math.cos(a), y + L / 2 * math.sin(a),
                                                                             z), r, jit(IVORY, rng, 0.06))
        skull(mb, (-0.05, -0.02, 0.0), 30, 0.68, lod=0, seed=1.0)
        skull(mb, (0.38, 0.32, 0.048), -60, 0.66, roll=85, lod=0, seed=2.0, col=IVORY_DK)
        # ribs: curved thin arcs lying flat
        for k in range(3):
            cx, cy = -0.3 + 0.07 * k, -0.42 + 0.03 * k
            pts = [Vector((cx + 0.14 * math.cos(t), cy + 0.09 * math.sin(t), 0.009)) for t in
                   [math.radians(d) for d in range(10, 171, 32)]]
            mb.tube(pts, 0.008, 4, col=IVORY_DK, sh=SMOOTH)
        for k in range(5):
            x, y = rng.uniform(-0.5, 0.5), rng.uniform(-0.45, 0.45)
            with mb.xf(T(x, y, 0.012) @ R(rng.uniform(0, 180), 'Z')):
                mb.cbox((0, 0, 0), (rng.uniform(0.04, 0.07), 0.022, 0.022), 0.006, jit(IVORY_DK, rng, 0.06))
    return mb


def make_sarcophagus():
    mb = MB("SM_KG_Sarcophagus", angle=30)
    rng = random.Random(180)
    with mb.xf(UE):
        base = pick(rng, SARC, 0.03)
        s0 = mb.cbox((0, 0, 0.06), (2.1, 0.8, 0.12), 0.025, mul(base, 0.92))
        mb.recolor(s0, lambda p, c: mix(c, MOSS_DK, 0.35 * smoothstep(-0.1, 0.5, nz(p, 3.0, (1, 2, 3)))))
        mb.cbox((0, 0, 0.36), (1.96, 0.68, 0.5), 0.02, base)
        light, dark = mix(base, C("E4E1D8"), 0.25), mul(base, 0.8)
        for sy in (-1, 1):
            for x in (-0.49, 0.49):
                y = sy * 0.34
                mb.box((x, y + sy * 0.01, 0.36), (0.8, 0.02, 0.34), dark)
                for dz in (-0.14, 0.14):
                    mb.box((x, y + sy * 0.018, 0.36 + dz), (0.84, 0.03, 0.06), light)
                for dx in (-0.39, 0.39):
                    mb.box((x + dx, y + sy * 0.018, 0.36), (0.06, 0.03, 0.34), light)
        for sx in (-1, 1):
            mb.box((sx * 0.99, 0, 0.36), (0.02, 0.44, 0.3), dark)
        mb.cbox((0, 0, 0.645), (2.1, 0.8, 0.07), 0.02, pick(rng, SARC, 0.03))
        top = mix(pick(rng, SARC, 0.03), C("E4E1D8"), 0.1)
        with mb.xf(YZX):
            mb.prism([(-0.36, 0.68), (0.36, 0.68), (0.26, 0.77), (-0.26, 0.77)], -1.0, 1.0, top, b0=0.02, b1=0.02)
        relief = mix(top, C("F2EFE8"), 0.3)
        mb.cbox((0.1, 0, 0.785), (1.2, 0.1, 0.03), 0.008, relief)
        mb.cbox((-0.3, 0, 0.785), (0.1, 0.42, 0.03), 0.008, relief)
    return mb


def make_crypt_gate():
    mb = MB("SM_KG_CryptGate", angle=35)
    rng = random.Random(190)

    def rusty(p, c):
        return mix(c, RUST, 0.45 * smoothstep(0.5, 0.0, p.z) * (0.5 + 0.5 * nz(p, 4.0, (1, 1, 1))))
    with mb.xf(UE):
        s0 = len(mb.V)
        mb.cbox((0.025, 0, 1.25), (0.05, 0.05, 2.5), 0.008, IRON)             # hinge stile
        mb.cbox((1.875, 0, 1.25), (0.05, 0.05, 2.5), 0.008, IRON)             # latch stile
        for x in (0.025, 1.875):
            mb.blob((x, 0, 2.54), 0.04, sub=1, amp=0.0, col=IRON_LT, sh=AUTO)
        for z in (0.25, 2.2):
            mb.cbox((0.95, 0, z), (1.9, 0.045, 0.07), 0.01, IRON_LT)
        for k in range(15):
            x = 0.12 * (k + 1)
            mb.box((x, 0, 1.21), (0.03, 0.03, 2.38), IRON)
            with mb.xf(T(x, 0, 2.4)):
                mb.lathe([(0, 0), (0.022, 0), (0.022, 0.025), (0.011, 0.035), (0.034, 0.085), (0, 0.2)], 4,
                         col=IRON_LT, sh=FLAT, phase=math.pi / 4)
        for z in (0.35, 2.05):                                                 # hinge knuckles
            mb.cyl((0.02, 0, z - 0.07), (0.02, 0, z + 0.07), 0.034, seg=8, col=IRON_LT, sh=AUTO)
        # lock plate with keyhole on both faces
        mb.cbox((1.7, 0, 1.15), (0.2, 0.06, 0.3), 0.012, mix(IRON_LT, RUST, 0.2))
        for sy in (-1, 1):
            mb.cyl((1.7, sy * 0.03, 1.19), (1.7, sy * 0.034, 1.19), 0.016, seg=8, col=C("0E0D10"), sh=FLAT)
            mb.box((1.7, sy * 0.032, 1.155), (0.012, 0.004, 0.05), C("0E0D10"))
            with mb.xf(T(1.62, sy * 0.045, 1.1) @ R(90, 'Y')):
                mb.torus(0.035, 0.008, 10, 4, col=IRON_LT)
        mb.recolor(s0, lambda p, c: rusty(p, c))
    return mb


def make_candle_cluster():
    mb = MB("SM_KG_CandleCluster", angle=35, material="Glow")
    rng = random.Random(200)
    puddle = [(0, 0), (0.19, 0), (0.17, 0.007), (0.11, 0.012), (0, 0.013)]
    rfn, _ = lump_fns(len(puddle), 0.18, 0.0, 6.0)
    with mb.xf(UE):
        mb.lathe(puddle, 12, col=WAX, sh=FLAT, rfn=rfn)
        for k, (x, y, h, r) in enumerate(((0.0, 0.0, 0.26, 0.024), (0.075, 0.035, 0.18, 0.021),
                                          (-0.065, 0.05, 0.14, 0.02), (0.045, -0.07, 0.11, 0.018),
                                          (-0.08, -0.045, 0.2, 0.022), (0.12, -0.02, 0.08, 0.025),
                                          (-0.02, 0.105, 0.1, 0.017))):
            col = jit(CREAM, rng, 0.04)
            with mb.xf(T(x, y, 0.004) @ R(rng.uniform(-3, 3), 'X')):
                mb.lathe([(0, 0), (r, 0), (r, h - 0.012), (r * 0.9, h - 0.002), (r * 0.5, h - 0.007),
                          (0, h - 0.008)], 8, colfn=lambda rr, z, a, i, h=h, col=col:
                         mix(mul(col, 0.92), mix(col, C("FFF8E6"), 0.4), z / h), sh=AUTO, phase=rng.random())
                for d in range(2):
                    a = rng.uniform(0, 6.28)
                    zd = h - 0.02 - rng.uniform(0.0, 0.04)
                    mb.blob((r * math.cos(a), r * math.sin(a), zd), (0.0075, 0.0075, 0.02), sub=0, amp=0.05,
                            col=mix(col, C("FFF8E6"), 0.3), sh=SMOOTH)
                mb.cyl((0, 0, h - 0.009), (0, 0, h + 0.006), 0.0025, seg=4, col=C("2A2420"), sh=FLAT)
                flame(mb, (0, 0, h + 0.002), 0.03 + 0.004 * (k % 2), 0.0085)
    return mb


# ================================================================================================ mine kit
def pn(x, v, f=1.0, o=(0.0, 0.0, 0.0), k=0.9):
    """Noise periodic in x with period 2 m (tiles seamlessly along X)."""
    return nz((k * math.cos(math.pi * x), k * math.sin(math.pi * x), v * f), 1.0, o)


def rock_col(x, v, seed=0.0, dark=1.0):
    band = int(math.floor(v * 2.2 + 1.3 * pn(x, v, 0.8, (seed, 3, 1)))) % len(ROCK)
    c = mul(ROCK[band], dark * (1.0 + 0.08 * pn(x, v, 3.0, (1, seed, 7), k=2.0)))
    return with_a(c, 0.0)


def make_mine_floor():
    mb = MB("SM_KG_MineFloor_2m", angle=35)
    rng = random.Random(210)
    xs = [-1 + 0.25 * i for i in range(9)]
    ys = [-1 + 0.1 * j for j in range(21)]

    def rut(y):
        return smoothstep(0.1, 0.02, abs(abs(y) - 0.45))

    def h(x, y):
        edge = smoothstep(1.0, 0.8, abs(y))
        return 0.009 * pn(x, y, 1.6, (2, 2, 2)) * edge - 0.009 * edge - 0.016 * rut(y)

    def col(x, y):
        c = mix(DIRT, DIRT_LT, 0.5 + 0.5 * pn(x, y, 2.5, (5, 1, 1), k=1.5))
        c = mix(c, DIRT_DK, 0.75 * rut(y))
        return with_a(c, 0.0)
    with mb.xf(UE):
        G = [[mb.v((x, y, h(x, y)), col(x, y)) for y in ys] for x in xs]
        for i in range(len(xs) - 1):
            for j in range(len(ys) - 1):
                xc, yc = (xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2
                mb.f_out([G[i][j], G[i + 1][j], G[i + 1][j + 1], G[i][j + 1]], (xc, yc, -0.5), None, FLAT)
        sidec = with_a(DIRT_DK, 0.0)
        B = {(sx, sy): mb.v((sx, sy, -0.1), sidec) for sx in (-1, 1) for sy in (-1, 1)}
        mb.f_out([B[(-1, -1)], B[(1, -1)], B[(1, 1)], B[(-1, 1)]], (0, 0, 0.5), None, FLAT)
        inside = (0, 0, -0.05)
        mb.f_out([G[0][j] for j in range(len(ys))] + [B[(-1, 1)], B[(-1, -1)]], inside, None, FLAT)
        mb.f_out([G[-1][j] for j in range(len(ys))] + [B[(1, 1)], B[(1, -1)]], inside, None, FLAT)
        mb.f_out([G[i][0] for i in range(len(xs))] + [B[(1, -1)], B[(-1, -1)]], inside, None, FLAT)
        mb.f_out([G[i][-1] for i in range(len(xs))] + [B[(1, 1)], B[(-1, 1)]], inside, None, FLAT)
        n = 0
        while n < 9:
            x, y = rng.uniform(-0.85, 0.85), rng.uniform(-0.85, 0.85)
            if rut(y) > 0.1:
                continue
            r = rng.uniform(0.018, 0.035)
            pebble(mb, rng, (x, y, h(x, y) + 0.2 * r), r)
            n += 1
    return mb


def make_mine_wall():
    mb = MB("SM_KG_MineWall_2m", angle=35)
    rng = random.Random(220)
    xs = [-1 + 0.25 * i for i in range(9)]
    zs = [0.3 * k for k in range(10)]

    def disp(x, z):
        d = 0.036 * pn(x, z, 1.3, (2, 5, 1)) + 0.016 * pn(x, z, 3.1, (7, 1, 3), k=1.8)
        return clamp(d, -0.05, 0.05)
    with mb.xf(UE):
        G = [[mb.v((x, disp(x, z), z), rock_col(x, z, 0.0, 1.0 - 0.18 * smoothstep(0.5, 0.0, z))) for z in zs]
             for x in xs]
        for k, (x, z, rx, rz) in enumerate(((-0.55, 1.9, 0.32, 0.2), (0.3, 1.2, 0.36, 0.24), (0.55, 2.3, 0.25, 0.16),
                                            (-0.35, 0.75, 0.28, 0.18))):
            mb.blob((x, 0.02, z), (rx, 0.07, rz), sub=1, amp=0.2, seed=k * 3.3, col=rock_col(x, z + 0.2, 1.0, 0.95),
                    sh=FLAT)
        for i in range(8):
            for k in range(9):
                xc, zc = (xs[i] + xs[i + 1]) / 2, (zs[k] + zs[k + 1]) / 2
                mb.f_out([G[i][k], G[i + 1][k], G[i + 1][k + 1], G[i][k + 1]], (xc, 0.4, zc), None, FLAT)
        back = with_a(mul(ROCK[3], 0.8), 0.0)
        B = {(sx, sz): mb.v((sx, 0.5, sz), back) for sx in (-1, 1) for sz in (0.0, 2.7)}
        inside = (0, 0.25, 1.35)
        mb.f_out([B[(-1, 0.0)], B[(1, 0.0)], B[(1, 2.7)], B[(-1, 2.7)]], inside, None, FLAT)
        mb.f_out([G[0][k] for k in range(10)] + [B[(-1, 2.7)], B[(-1, 0.0)]], inside, None, FLAT)
        mb.f_out([G[8][k] for k in range(10)] + [B[(1, 2.7)], B[(1, 0.0)]], inside, None, FLAT)
        mb.f_out([G[i][9] for i in range(9)] + [B[(1, 2.7)], B[(-1, 2.7)]], inside, None, FLAT)
        mb.f_out([G[i][0] for i in range(9)] + [B[(1, 0.0)], B[(-1, 0.0)]], inside, None, FLAT)
        # rubble at the foot of the face
        for k, x in enumerate((-0.55, -0.1, 0.35, 0.72)):
            mb.blob((x + rng.uniform(-0.08, 0.08), -0.07, 0.05), (rng.uniform(0.1, 0.16), 0.08, 0.08), sub=1, amp=0.25,
                    seed=k * 4.0, col=rock_col(x, 0.2, 5.0, 0.9), sh=FLAT)
        # timber post in front of the face at X=-1
        s0 = mb.cbox((-1.0, -0.12, 1.3), (0.2, 0.2, 2.6), 0.025, TIMBER)
        mb.recolor(s0, lambda p, c: with_a(mix(TIMBER_DK, TIMBER_LT, 0.5 + 0.5 * math.sin(p.z * 9.0 + nz(p, 2.0) * 3.0)),
                                           0.0))
        mb.cbox((-1.0, -0.12, 0.04), (0.26, 0.26, 0.08), 0.02, rock_col(-1.0, 0.0, 9.0, 0.85))
        mb.box((-0.94, -0.23, 1.85), (0.03, 0.02, 0.03), IRON)
    return mb


def make_mine_ceiling():
    mb = MB("SM_KG_MineCeiling_2m", angle=35)
    xs = [-1 + 0.25 * i for i in range(9)]
    ys = [-1 + 0.25 * j for j in range(9)]

    def h(x, y):
        return -0.1 * clamp(0.5 + 0.6 * pn(x, y, 1.4, (4, 4, 4)))
    with mb.xf(UE):
        G = [[mb.v((x, y, h(x, y)), rock_col(x, y * 0.6, 3.0, 0.8)) for y in ys] for x in xs]
        for i in range(8):
            for j in range(8):
                xc, yc = (xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2
                mb.f_out([G[i][j], G[i + 1][j], G[i + 1][j + 1], G[i][j + 1]], (xc, yc, 0.15), None, FLAT)
        top = with_a(mul(ROCK[3], 0.7), 0.0)
        B = {(sx, sy): mb.v((sx, sy, 0.3), top) for sx in (-1, 1) for sy in (-1, 1)}
        inside = (0, 0, 0.15)
        mb.f_out([B[(-1, -1)], B[(1, -1)], B[(1, 1)], B[(-1, 1)]], inside, None, FLAT)
        mb.f_out([G[0][j] for j in range(9)] + [B[(-1, 1)], B[(-1, -1)]], inside, None, FLAT)
        mb.f_out([G[8][j] for j in range(9)] + [B[(1, 1)], B[(1, -1)]], inside, None, FLAT)
        mb.f_out([G[i][0] for i in range(9)] + [B[(1, -1)], B[(-1, -1)]], inside, None, FLAT)
        mb.f_out([G[i][8] for i in range(9)] + [B[(1, 1)], B[(-1, 1)]], inside, None, FLAT)
        s0 = mb.cbox((-1.0, 0.0, -0.11), (0.2, 2.0, 0.22), 0.025, TIMBER)
        mb.recolor(s0, lambda p, c: with_a(mix(TIMBER_DK, TIMBER_LT, 0.5 + 0.5 * math.sin(p.y * 7.0 + nz(p, 2.0) * 3.0)),
                                           0.0))
        for sy in (-0.55, 0.45):
            mb.cbox((-1.0, sy, 0.01), (0.16, 0.18, 0.04), 0.01, WOOD_DK)
    return mb


# ================================================================================================ well + ladder
def ladder_rung_z(top=4.0, step=0.3, first=0.3):
    z, out = first, []
    while z < top - 0.05:
        out.append(z)
        z += step
    return out


def make_ladder():
    mb = MB("SM_KG_Ladder_4m", angle=35)
    rng = random.Random(230)
    with mb.xf(UE):
        for sy in (-1, 1):
            s0 = mb.cbox((0, sy * 0.26, 2.0), (0.06, 0.04, 4.0), 0.008, jit(WOOD, rng, 0.05))
            mb.recolor(s0, lambda p, c: with_a(mul(c, 1.0 + 0.1 * nz(p, 3.0)), 0.0))
        for z in ladder_rung_z():
            col = jit(WOOD_LT, rng, 0.07)
            mb.cyl((0, -0.245, z), (0, 0.245, z), 0.018, seg=6, col=col, sh=AUTO)
            for sy in (-1, 1):
                mb.box((0, sy * 0.283, z), (0.03, 0.008, 0.03), C("2A2826"))
    return mb


def make_well_shaft():
    mb = MB("SM_KG_WellShaft", angle=30, material="Glow")
    rng = random.Random(240)
    NC = 10
    zs = [0.42 * k for k in range(NC + 1)]                   # 10 courses, 0 .. 4.2
    ri, ro = 0.8, 1.1
    ZN = zs[6]                                               # top of the doorway notch (a course line, 2.52)
    prof = [(ri, 0.0), (ro, 0.0), (ro, ZN), (ro, 4.2), (ri, 4.2)]
    info = [("bot", 0), ("out", 0), ("out", 0), ("top", 0)]
    for c in range(NC - 1, -1, -1):
        z0 = zs[c]
        if c > 0:
            prof.append((ri, z0 + 0.016))
            info.append(("stone", c))
            prof.append((ri + 0.018, z0))
            info.append(("mortar", c))
            prof.append((ri, z0 - 0.016))
            info.append(("mortar", c))
    info.append(("stone", 0))
    seg = 24
    stone_cols = {}

    def damp(z):
        return 1.0 - 0.45 * smoothstep(1.8, 0.0, z)

    def facefn(i, j):
        kind, c = info[i]
        zm = (zs[c] + zs[c + 1]) / 2 if c < NC else 4.2
        if kind == "stone":
            sid = ((j + (c % 2)) // 2) % (seg // 2)
            if (c, sid) not in stone_cols:
                r2 = random.Random(c * 97 + sid * 13)
                stone_cols[(c, sid)] = jit(r2.choice(WELLST), r2, 0.07)
            col = mul(stone_cols[(c, sid)], damp(zm))
            if zm < 1.2:
                col = mix(col, MOSS_DK, 0.3 * smoothstep(1.2, 0.2, zm))
            return with_a(col, 0.0)
        if kind == "mortar":
            return with_a(mul(MORTAR, damp(zs[c])), 0.0)
        return with_a(mul(WELLST[1], 0.8), 0.0)
    # doorway (spec change): faces 15..20 (225..315 deg = UE -Y side) are cut out below ZN; the notch gets a lintel,
    # two radial jambs and a flat ashlar portal slab with a 1.1 x 2.2 round-arched opening. +X side stays plain.
    door = set(range(15, 21))
    iN = next(i for i, (r, z) in enumerate(prof) if abs(z - ZN) < 1e-6 and r > ri + 0.01 and r < ro - 0.01)
    oN = 2
    n = len(prof)
    with mb.xf(UE):
        cache = {}

        class _Ids:                                          # vertices created on first use (no loose verts)
            def __getitem__(self, i):
                return _Row(i)

        class _Row:
            def __init__(self, i):
                self.i = i

            def __getitem__(self, j):
                j %= seg
                if (self.i, j) not in cache:
                    r, z = prof[self.i]
                    a = 2 * math.pi * j / seg
                    cache[(self.i, j)] = mb.v((r * math.cos(a), r * math.sin(a), z), WELLST[0])
                return cache[(self.i, j)]
        ids = _Ids()
        for i in range(n):
            i1 = (i + 1) % n
            low = max(prof[i][1], prof[i1][1]) <= ZN + 1e-6
            for j in range(seg):
                if low and j in door:
                    continue
                jn = (j + 1) % seg
                mb.f((ids[i][j], ids[i][jn], ids[i1][jn], ids[i1][j]), facefn(i, j), FLAT)
        jcol = with_a(mul(WELLST[2], 0.9), 0.0)
        for j in door:
            a = 2 * math.pi * (j + 0.5) / seg
            mb.f_out((ids[iN][j], ids[iN][j + 1], ids[oN][j + 1], ids[oN][j]),
                     (0.95 * math.cos(a), 0.95 * math.sin(a), ZN + 0.3), jcol, FLAT)
        for jb, da in ((min(door), -0.1), (max(door) + 1, 0.1)):
            a = 2 * math.pi * jb / seg + da
            loop = [ids[0][jb], ids[1][jb], ids[2][jb]] + [ids[i][jb] for i in range(iN, n)]
            mb.f_out(loop, (0.95 * math.cos(a), 0.95 * math.sin(a), 1.2), jcol, FLAT)
        with mb.xf(frame((0, -0.8, 0), (1, 0, 0), (0, 0, 1), (0, -1, 0))):     # local (u, v, w) = (X, Z, -Y - 0.8)
            ashlar(mb, rng, -0.79, 0.79, [0.0, 0.62, 1.22, 1.75, ZN], 0.0, 0.3, WELLST,
                   [dict(xc=0.0, zb=0.0, r=0.55, zs=1.65)], bevel=0.025, jl=(0.4, 0.6), first=(0.2, 0.25),
                   colfn=lambda l, c, seed: with_a(mul(c, damp(l[1]) * (1.0 + 0.05 * nz(l, 3.0, (seed, 1, 1)))), 0.0),
                   min_end=0.15)
        # stone collar around the timber cap
        mb.lathe([(0.96, 4.2), (ro, 4.2), (ro, 4.4), (0.96, 4.4)], seg, col=with_a(WELLST[2], 0.0), sh=FLAT, closed=True,
                 phase=math.pi / seg)
        # timber grate: planks with gaps, bearers, then the daylight disc (Glow, faces down) under a solid lid
        for k in range(8):
            y = -0.77 + 0.22 * k
            half = math.sqrt(max(0.95 ** 2 - y * y, 0.04)) - 0.01
            s0 = mb.cbox((0, y, 4.235), (2 * half, 0.16, 0.07), 0.012, jit(TIMBER, rng, 0.08))
        for x in (-0.45, 0.45):
            half = math.sqrt(0.95 ** 2 - x * x) - 0.01
            mb.cbox((x, 0, 4.31), (0.14, 2 * half, 0.08), 0.012, TIMBER_DK)
        lid_cols = [SKY, with_a(TIMBER_DK, 0.0), with_a(TIMBER_DK, 0.0), with_a(TIMBER_DK, 0.0)]
        mb.lathe([(0, 4.352), (0.95, 4.352), (0.95, 4.4), (0, 4.4)], 16, col=TIMBER_DK, sh=FLAT, segcol=lid_cols)
        # iron ring bolts on the inside
        for ang, z in ((52.5, 3.1), (157.5, 1.7)):
            with mb.xf(R(ang, 'Z') @ T(ri * math.cos(math.pi / seg), 0, z)):
                mb.cbox((-0.012, 0, 0), (0.026, 0.14, 0.18), 0.008, mix(IRON_LT, RUST, 0.3))
                for sy in (-1, 1):
                    mb.cbox((-0.04, sy * 0.03, 0.03), (0.04, 0.018, 0.03), 0.005, IRON)
                mb.cyl((-0.05, -0.045, 0.03), (-0.05, 0.045, 0.03), 0.013, seg=6, col=IRON)
                with mb.xf(T(-0.05, 0, 0.03 - 0.075) @ R(90, 'Y')):
                    mb.torus(0.075, 0.015, 14, 5, col=mix(IRON_LT, RUST, 0.25))
    return mb


def make_well_mouth():
    mb = MB("SM_KG_WellMouth", angle=35)
    rng = random.Random(250)
    ri, ro = 0.475, 0.66
    with mb.xf(UE):
        def rim_col(p, c):
            q = ue(p)
            r = math.hypot(q.x, q.y)
            if r < ri + 0.02:
                k = smoothstep(0.03, 0.5, q.z)
                c = mix(mix(PIT, c, 0.25), c, k)
            return with_a(c, 0.0)
        for ci, (z0, z1) in enumerate(((0.0, 0.16), (0.16, 0.31), (0.31, 0.45))):
            n = 9
            off = (ci % 2) * 0.5
            for k in range(n):
                a0 = 2 * math.pi * (k + off) / n + 0.006
                a1 = 2 * math.pi * (k + 1 + off) / n - 0.006
                rr = ro + rng.uniform(-0.015, 0.012)
                poly = [(ri * math.cos(a0), ri * math.sin(a0)), (rr * math.cos(a0), rr * math.sin(a0)),
                        (rr * math.cos(a1), rr * math.sin(a1)), (ri * math.cos(a1), ri * math.sin(a1))]
                s0 = mb.prism(poly, z0 + 0.003, z1, pick(rng, VSTONES), b1=0.022)
                mb.recolor(s0, rim_col)
        n = 8
        for k in range(n):
            a0, a1 = 2 * math.pi * (k + 0.25) / n + 0.005, 2 * math.pi * (k + 1.25) / n - 0.005
            am = (a0 + a1) / 2
            poly = [(0.46 * math.cos(a), 0.46 * math.sin(a)) for a in (a1, am, a0)] + \
                   [(0.69 * math.cos(a), 0.69 * math.sin(a)) for a in (a0, am, a1)]
            s0 = mb.prism(poly, 0.45, 0.55, mix(pick(rng, VSTONES), C("D8CFBF"), 0.2), b1=0.025)
            mb.recolor(s0, lambda p, c: mix(c, C("7D9A45"), 0.5) if (rng.random() < 0.08) else c)
        # dark lining + black bottom disc (reads as depth)
        mb.lathe([(0.44, 0.34), (0.44, 0.02)], 18, colfn=lambda r, z, a, i: with_a(mix(PIT, mul(VSTONES[1], 0.35),
                                                                                           z / 0.34), 0.0), sh=FLAT)
        mb.lathe([(0.445, 0.02), (0, 0.02)], 18, col=with_a(PIT, 0.0), sh=FLAT)
        # crank: two posts, log roller with rope coils, iron crank handle
        for sx in (-1, 1):
            s0 = mb.cbox((sx * 0.8, 0, 0.7), (0.12, 0.12, 1.4), 0.015, jit(WOOD, rng, 0.05))
            mb.cbox((sx * 0.8, 0, 0.04), (0.2, 0.2, 0.08), 0.02, pick(rng, VSTONES))
            mb.cbox((sx * 0.8, 0, 1.39), (0.16, 0.16, 0.03), 0.008, WOOD_DK)
        mb.cyl((-0.74, 0, 1.22), (0.74, 0, 1.22), 0.065, seg=8, col=WOOD_LT, capcol=WOOD, sh=AUTO)
        mb.cyl((-0.9, 0, 1.22), (0.93, 0, 1.22), 0.02, seg=6, col=IRON)
        mb.beam((0.93, 0, 1.24), (0.93, 0, 0.96), 0.03, 0.04, IRON, up=(0, 1, 0))
        mb.cyl((0.92, 0, 0.98), (1.08, 0, 0.98), 0.02, seg=6, col=WOOD_DK)
        for k in range(4):
            with mb.xf(T(-0.28 + 0.075 * k, 0, 1.22) @ R(90, 'Y')):
                mb.torus(0.078, 0.018, 8, 4, col=jit(ROPE, rng, 0.06))
        # rope down to a bucket hanging over the opening
        bx, by = 0.05, -0.078
        mb.cyl((bx, by, 1.2), (bx, by, 0.93), 0.012, seg=5, col=ROPE)
        mb.lathe([(0, 0.62), (0.12, 0.62), (0.145, 0.88), (0.132, 0.88), (0.11, 0.64), (0, 0.64)], 10,
                 col=WOOD, sh=AUTO, phase=0.3, facefn=lambda i, j: with_a(WOOD if j % 2 else WOOD_LT, 0.0)
                 if i == 1 else None, rfn=None)
        for z, r in ((0.665, 0.126), (0.84, 0.142)):
            with mb.xf(T(bx, by, z)):
                mb.torus(r, 0.008, 10, 3, col=IRON)
        with mb.xf(T(bx, by, 0.0)):
            pass
        with mb.xf(T(bx, by, 0)):
            mb.lathe([(0.12, 0.8), (0, 0.8)], 10, col=WATER, sh=FLAT)
            pts = [Vector((0.145 * math.cos(math.radians(d)), 0, 0.855 + 0.075 * math.sin(math.radians(d))))
                   for d in range(0, 181, 30)]
            mb.tube(pts, 0.007, 4, col=IRON, sh=SMOOTH)
        # the top of a ladder inside the rim, leaning on the far wall
        for sx in (-1, 1):
            mb.beam((sx * 0.19, 0.28, 0.02), (sx * 0.19, 0.36, 0.68), 0.05, 0.04, jit(WOOD, rng, 0.06),
                    up=(0, -1, 0), bevel=0.006)
        for z in (0.2, 0.5):
            y = 0.28 + 0.08 * (z - 0.02) / 0.66
            mb.cyl((-0.19, y, z), (0.19, y, z), 0.016, seg=6, col=WOOD_LT)
        for k in range(4):
            a = rng.uniform(0, 6.28)
            tuft(mb, (0.68 * math.cos(a), 0.68 * math.sin(a), 0.0), rng, n=5, h=0.14, spread=0.03)
    return mb


# ================================================================================================ crypt stair
def make_crypt_stair():
    mb = MB("SM_KG_CryptStair", angle=30, material="Glow")
    rng = random.Random(260)
    STEPF = frame((0, 0, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))       # local (u, v, w) = (X, Z, Y)
    STEPS = [C("9A9DA4"), C("92959C"), C("A1A4AB"), C("8E9198")]
    colfn = cata_colfn(damp_top=0.45, ao_depth=0.0)
    with mb.xf(UE):
        with mb.xf(STEPF):
            for k in range(1, 13):
                x0, x1, zt = 0.3 * (k - 1), 0.3 * k, 0.2 * k
                poly = [(x0, 0.0), (x1, 0.0), (x1, zt), (x0 + 0.03, zt), (x0, zt - 0.028)]
                s0 = mb.prism(poly, -1.0, 1.0, pick(rng, STEPS, 0.05))
                mb.recolor(s0, lambda p, c, zt=zt: with_a(mix(c, C("B7BAC0"), 0.35) if abs(p.z - zt) < 1e-4 and
                                                          abs(p.y) < 0.9 else c, 0.0))
        # landing: support + flagstones
        mb.box((4.2, 0, 1.14), (1.2, 2.0, 2.28), with_a(mul(STEPS[1], 0.8), 0.0))
        for xa, xb in ((3.6, 4.2), (4.2, 4.8)):
            for ya, yb in ((-1.0, 0.0), (0.0, 1.0)):
                mb.cbox(((xa + xb) / 2, (ya + yb) / 2, 2.34), (xb - xa - 0.004, yb - ya - 0.004, 0.12), 0.02,
                        pick(rng, STEPS, 0.06))
        # side walls (face at |Y| = 1.0, body to 1.3), 0 .. 5.4
        zs = [0.0, 0.68, 1.35, 2.02, 2.7, 3.38, 4.05, 4.72, 5.4]
        for sy in (1, -1):
            with mb.xf(frame((0, sy * 1.0, 0), (1, 0, 0), (0, 0, 1), (0, sy, 0))):
                ashlar(mb, rng, 0.0, 4.8, zs, 0.0, 0.3, CATA, (), bevel=0.03, jl=(1.0, 1.5), first=(0.6, 1.2),
                       colfn=colfn, min_end=0.45)
        # end wall at X = 4.8 with an arched door opening
        op = dict(xc=0.0, zb=2.4, r=0.58, zs=4.02)
        with mb.xf(frame((4.8, 0, 0), (0, 1, 0), (0, 0, 1), (1, 0, 0))):     # local (u, v, w) = (Y, Z, X - 4.8)
            ashlar(mb, rng, -1.3, 1.3, [2.4, 2.95, 3.5, 4.05, 4.7, 5.05, 5.4], 0.0, 0.3, CATA, [op], bevel=0.03,
                   jl=(0.5, 0.9), first=(0.3, 0.55), colfn=colfn, min_end=0.25, arcn=6)
            Minv = mb.M.inverted()
            # frame: jambs + voussoirs (6 cm proud) and a threshold
            for sx in (-1, 1):
                for za, zb in ((2.4, 2.95), (2.95, 3.5), (3.5, 4.02)):
                    poly = [(sx * 0.58, za + 0.002), (sx * 0.8, za + 0.002), (sx * 0.8, zb - 0.002),
                            (sx * 0.58, zb - 0.002)]
                    s0 = mb.prism(ccw(poly), -0.06, 0.0, pick(rng, FRAME), b0=0.015)
            n = 7
            for k in range(n):
                a0, a1 = math.pi * k / n, math.pi * (k + 1) / n
                am = (a0 + a1) / 2
                ro = 0.8 + (0.06 if k == n // 2 else 0.0)
                poly = [(0.58 * math.cos(a), 4.02 + 0.58 * math.sin(a)) for a in (a0, a1)] + \
                       [(ro * math.cos(a), 4.02 + ro * math.sin(a)) for a in (a1, a0)]
                s0 = mb.prism(inset2(ccw(poly), 0.003), -0.06 - (0.02 if k == n // 2 else 0.0), 0.0, pick(rng, FRAME),
                              b0=0.015)
            mb.cbox((0, 2.42, 0.1), (1.24, 0.04, 0.4), 0.01, pick(rng, FRAME))
            # door leaf: 5 boards, arched top (1.1 x 2.1, bottom at 2.47), recessed 10 cm
            zb, zsp, rd = 2.47, 4.02, 0.55

            def top(u):
                return zsp + math.sqrt(max(rd * rd - u * u, 0.0))
            bw = 1.1 / 5
            for k in range(5):
                a, b = -0.55 + bw * k + 0.003, -0.55 + bw * (k + 1) - 0.003
                us = [b - (b - a) * t / 4 for t in range(5)]
                poly = [(a, zb), (b, zb)] + [(u, top(u)) for u in us]
                s0 = mb.prism(ccw(dedupe(poly)), 0.1, 0.16, jit(mix(TIMBER, WOOD, rng.random() * 0.5), rng, 0.06),
                              b0=0.008)
            for zc in (2.8, 3.75):
                mb.box((0, zc, 0.093), (1.06, 0.07, 0.014), IRON)
                for k in range(5):
                    mb.box((-0.4 + 0.2 * k, zc, 0.082), (0.026, 0.026, 0.012), IRON_LT)
            mb.cbox((0.33, 3.3, 0.093), (0.08, 0.11, 0.014), 0.004, IRON)
            with mb.xf(T(0.33, 3.22, 0.078)):
                mb.torus(0.058, 0.011, 12, 4, col=IRON_LT)
            # warm light behind the door: shows as a thin seam around and under the leaf (Glow)
            mb.prism(inset2(ccw(arch_outline(op, 12)), 0.004), 0.17, 0.19, DOORGLOW)
    return mb


# ================================================================================================ registry
PROPS = [
    ("DigMound", make_dig_mound, "none"),
    ("DigX", make_dig_x, "none"),
    ("DigGlint", make_dig_glint, "none"),
    ("DigHole_1", lambda: make_dig_hole(1), "none"),
    ("DigHole_2", lambda: make_dig_hole(2), "none"),
    ("DigHole_3", lambda: make_dig_hole(3), "none"),
    ("DirtPile", make_dirt_pile, "none"),
    ("GraveOpen_1", lambda: make_grave_open(1), "none"),
    ("GraveOpen_2", lambda: make_grave_open(2), "none"),
    ("BuriedChest", make_buried_chest, "none"),
    ("NicheWall_2m", make_niche_wall, "box"),
    ("CryptVault_2m", make_crypt_vault, "box"),
    ("SkullPile", make_skull_pile, "box"),
    ("BoneScatter", make_bone_scatter, "none"),
    ("Sarcophagus", make_sarcophagus, "box"),
    ("CryptGate", make_crypt_gate, "box"),
    ("CandleCluster", make_candle_cluster, "none"),
    ("MineFloor_2m", make_mine_floor, "box"),
    ("MineWall_2m", make_mine_wall, "box"),
    ("MineCeiling_2m", make_mine_ceiling, "box"),
    ("WellShaft", make_well_shaft, "complex"),
    ("Ladder_4m", make_ladder, "none"),
    ("CryptStair", make_crypt_stair, "complex"),
    ("WellMouth", make_well_mouth, "none"),
]

BELOW_ZERO_OK = {"MineFloor_2m", "MineCeiling_2m"}

# expected bounds in UE axes (metres) for the pivot / extents check; None = not checked
EXPECT_UE = {
    "DigMound": ((-0.6, -0.6, 0.0), (0.6, 0.6, 0.28)),
    "DigX": ((-0.65, -0.65, 0.0), (0.65, 0.65, 0.5)),
    "DigGlint": ((-0.4, -0.4, 0.0), (0.4, 0.4, None)),
    "DigHole_1": ((-0.65, -0.65, 0.0), (0.65, 0.65, None)),
    "DigHole_2": ((-0.75, -0.75, 0.0), (0.75, 0.75, None)),
    "DigHole_3": ((-0.85, -0.85, 0.0), (0.85, 0.85, None)),
    "DirtPile": ((-0.5, -0.5, 0.0), (0.5, 0.5, 0.5)),
    "GraveOpen_1": ((-1.1, -0.5, 0.0), (1.1, 0.5, None)),
    "GraveOpen_2": ((-1.2, -0.6, 0.0), (1.2, None, 0.5)),
    "BuriedChest": ((-0.8, -0.8, 0.0), (0.8, 0.8, None)),
    "NicheWall_2m": ((-1.0, -0.045, 0.0), (1.0, 0.5, 3.12)),
    "CryptVault_2m": ((-1.0, -1.25, 0.0), (1.0, 1.25, 1.0)),
    "SkullPile": ((-0.45, None, 0.0), (0.45, None, 0.55)),
    "BoneScatter": ((None, None, 0.0), (None, None, 0.12)),
    "Sarcophagus": ((-1.05, -0.4, 0.0), (1.05, 0.4, 0.8)),
    "CryptGate": ((0.0, None, 0.0), (1.9, None, 2.6)),
    "CandleCluster": ((None, None, 0.0), (None, None, None)),
    "MineFloor_2m": ((-1.0, -1.0, -0.1), (1.0, 1.0, 0.0)),
    "MineWall_2m": ((-1.1, -0.22, 0.0), (1.0, 0.5, 2.7)),
    "MineCeiling_2m": ((-1.1, -1.0, -0.22), (1.0, 1.0, 0.3)),
    "WellShaft": ((-1.1, -1.1, 0.0), (1.1, 1.1, 4.4)),
    "Ladder_4m": ((-0.03, -0.28, 0.0), (0.03, 0.28, 4.0)),
    "CryptStair": ((0.0, -1.3, 0.0), (5.1, 1.3, 5.4)),
    "WellMouth": ((None, None, 0.0), (None, None, 1.4)),
}


def expect_blender(e):
    (x0, y0, z0), (x1, y1, z1) = e
    return ((x0, None if y1 is None else -y1, z0), (x1, None if y0 is None else -y0, z1))


EXPECT = {k: expect_blender(v) for k, v in EXPECT_UE.items()}


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
        if name not in BELOW_ZERO_OK:                    # nothing may dip under the ground (tilted stakes, skulls...)
            for v in mb.V:
                v.z = max(v.z, 0.0)
        obj, tris, diag = finalize(mb)
        objs.append(obj)
        mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
        mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
        stats.append((full, tris, mn, mx, diag, mb.material, col, mb.info))
    print(f"\n{LOG}: object (UE axes)                tris   size (cm)             min (cm)               mat   col")
    for name, tris, mn, mx, diag, mat, col, info in stats:
        sz = (mx - mn) * 100
        print(f"{LOG}: {name:30s} {tris:6d}  {sz.x:5.0f} x {sz.y:5.0f} x {sz.z:5.0f}  min({mn.x * 100:6.0f},"
              f"{-mx.y * 100:6.0f},{mn.z * 100:6.0f}) max({mx.x * 100:6.0f},{-mn.y * 100:6.0f},{mx.z * 100:6.0f})  "
              f"{mat:5s} {col:8s} {diag}")
    print(f"{LOG}: total tris {sum(s[1] for s in stats)} in {len(stats)} props")
    return objs, stats


def export(path, objs):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"{LOG}: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


def write_manifest(stats):
    data = {"props": {s[0].replace("SM_KG_", ""): {"material": s[5], "collision": s[6]} for s in stats}}
    with open(MANIFEST, "w") as fh:
        json.dump(data, fh, indent=1)
    print(f"{LOG}: manifest {MANIFEST} ({len(data['props'])} props)")


# ================================================================================================ preview
GLOW_NODE = []


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
        GLOW_NODE.append(m)
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    mat.use_backface_culling = True
    return mat


def _flat_mat(name, rgb_):
    mat = bpy.data.materials.new(name)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*rgb_, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat, bsdf


def _bounds(o):
    cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))),
            Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs))))


WARM = (1.0, 0.62, 0.3)
COOL = (0.75, 0.85, 1.0)
DIRT_GROUND = (0.2, 0.16, 0.11)
STONE_GROUND = (0.16, 0.16, 0.17)


def preview_tiles(names):
    """Tile specs in UE axes: items = [(key, (x, y, z), yaw_deg)]; key = prop short name | HUMAN.
    az: 0 = camera on the -Y side looking +Y (sees wall faces), -90 = camera on -X looking +X."""
    H = "HUMAN"

    def P(n, hx=1.0, hy=0.35, **kw):
        return dict(label=n, items=[(n, (0, 0, 0), 0), (H, (hx, hy, 0), 0)], **kw)
    t1 = [
        P("DigMound", 1.0, el=30), P("DigX", 1.05, el=34), P("DigGlint", 0.75, el=26),
        P("DigHole_1", 1.0, el=38), P("DigHole_2", 1.1, el=38), P("DigHole_3", 1.2, el=38),
        P("DirtPile", 0.9, el=26), P("GraveOpen_1", 1.5, el=36), P("GraveOpen_2", 1.6, el=36),
        P("BuriedChest", 1.2, el=34),
        P("NicheWall_2m", 1.35, -0.45, az=-20, el=10),
        dict(label="CryptVault_2m (from below)", items=[("CryptVault_2m", (0, 0, 2.3), 0), (H, (1.3, -0.4, 0), 0)],
             az=-30, el=-12, target=(0, 0, 2.2), dist=5.2),
        P("SkullPile", 0.8, -0.2, el=18), P("BoneScatter", 0.95, el=34), P("Sarcophagus", 1.6, el=24),
        P("CryptGate", 2.3, -0.3, az=-20, el=10), dict(label="CandleCluster", items=[("CandleCluster", (0, 0, 0), 0),
                                                                                      (H, (0.45, 0.3, 0), 0)],
                                                        az=-30, el=24, target=(0.1, 0, 0.25), dist=1.6),
        dict(label="MineFloor_2m", items=[("MineFloor_2m", (0, 0, 0), 0), (H, (0.4, 0.5, 0), 0)], el=30),
        P("MineWall_2m", 1.4, -0.6, az=-25, el=10),
        dict(label="MineCeiling_2m (from below)", items=[("MineCeiling_2m", (0, 0, 2.7), 0), (H, (0.3, 0.3, 0), 0)],
             az=-30, el=-10, target=(0, 0, 2.0), dist=5.4),
        P("WellShaft", 1.8, -0.2, el=18), P("Ladder_4m", 0.9, 0.3, az=-60, el=14),
        dict(label="CryptStair", items=[("CryptStair", (0, 0, 0), 0), (H, (-0.4, 0.3, 0), 0)], az=-120, el=32),
        P("WellMouth", 1.4, el=24),
    ]

    corridor = []
    for cx in (-2.0, 0.0, 2.0):
        corridor += [("NicheWall_2m", (cx, 1.0, 0), 0), ("NicheWall_2m", (cx, -1.0, 0), 180),
                     ("CryptVault_2m", (cx, 0, 3.12), 0)]
    mine = []
    for cx in (-2.0, 0.0, 2.0):
        mine += [("MineFloor_2m", (cx, 0, 0), 0), ("MineWall_2m", (cx, 1.0, 0), 0), ("MineWall_2m", (cx, -1.0, 0), 180),
                 ("MineCeiling_2m", (cx, 0, 2.7), 0)]
    mine_half = [it for it in mine if not (it[0] == "MineWall_2m" and it[2] == 180)]
    corr_half = [it for it in corridor if not (it[0] == "NicheWall_2m" and it[2] == 180)]
    dig = [("DigMound", (0, 0, 0), 0), ("DigX", (1.55, 0, 0), 0), ("DigGlint", (2.9, 0, 0), 0),
           ("DigHole_1", (4.25, 0, 0), 0), ("DigHole_2", (5.85, 0, 0), 0), ("DigHole_3", (7.65, 0, 0), 0),
           ("DirtPile", (9.3, 0, 0), 0), (H, (-1.1, 0.4, 0), 0)]
    t2 = [
        dict(label="Catacomb corridor (candle light)", dark=True, lens=20, az=-90, el=-3, target=(1.8, 0, 1.6),
             dist=4.4, ground_col=STONE_GROUND, glow=3.0,
             items=corridor + [("CandleCluster", (-1.2, 0.72, 0), 0), ("CandleCluster", (1.3, -0.75, 0), 20),
                               ("CandleCluster", (2.6, 0.7, 0), 50), ("SkullPile", (2.3, -0.55, 0), 180),
                               ("BoneScatter", (0.4, 0.15, 0), 0), (H, (0.6, -0.4, 0), -90)],
             lights=[((-1.2, 0.55, 0.45), 18, WARM), ((1.3, -0.55, 0.45), 18, WARM), ((2.6, 0.5, 0.45), 18, WARM),
                     ((-1.0, 0.0, 2.4), 45, WARM)]),
        dict(label="Catacomb cutaway (dim)", dark=True, az=-15, el=10, ground_col=STONE_GROUND, glow=3.0,
             items=corr_half + [("Sarcophagus", (0.2, -0.05, 0), 0), ("SkullPile", (-2.0, 0.55, 0), 0),
                                ("CandleCluster", (1.4, 0.65, 0), 0), ("CandleCluster", (-0.9, 0.7, 0), 0),
                                ("BoneScatter", (-1.2, -0.3, 0), 0), ("CryptGate", (3.0, -1.0, 0), 90),
                                (H, (1.9, -0.3, 0), 0)],
             lights=[((1.4, 0.4, 0.5), 25, WARM), ((-0.9, 0.45, 0.5), 25, WARM), ((0.0, -2.5, 2.0), 90, WARM),
                     ((-2.5, -1.5, 3.0), 50, COOL)], zoom=1.05),
        dict(label="Mine tunnel (lantern)", dark=True, lens=20, az=-90, el=-2, target=(2.0, 0, 1.3), dist=4.6,
             ground_col=DIRT_GROUND, items=mine + [(H, (0.8, -0.3, 0), -90)],
             lights=[((0.0, 0.6, 2.0), 60, WARM), ((-2.2, -0.5, 1.8), 35, WARM), ((2.4, 0.4, 1.6), 30, WARM)]),
        dict(label="Mine cutaway (dim)", dark=True, az=-15, el=12, ground_col=DIRT_GROUND,
             items=mine_half + [(H, (0.3, -0.2, 0), 0)],
             lights=[((0.0, -0.3, 2.0), 70, WARM), ((0.0, -3.0, 2.0), 60, WARM)]),
        dict(label="Dig stages (+ human)", items=dig, az=-8, el=30),
        dict(label="Graves + chest", items=[("GraveOpen_1", (0, 0, 0), 0), ("GraveOpen_2", (2.8, 0, 0), 0),
                                            ("BuriedChest", (5.35, 0, 0), 0), (H, (-1.7, 0.35, 0), 0)], az=-8, el=32),
        dict(label="Holes from 1.7 m eye height", items=[("DigHole_1", (-1.75, 0.2, 0), 0), ("DigHole_2", (0, 0.2, 0), 0),
                                                        ("DigHole_3", (1.95, 0.2, 0), 0)],
             az=0, el=34, target=(0, 0.5, 0), dist=3.0, lens=24),
        dict(label="CryptStair from the foot (dim)", dark=True, lens=24, az=-90, el=-12, target=(3.8, 0, 3.1), dist=5.6,
             ground_col=STONE_GROUND, glow=3.0,
             items=[("CryptStair", (0, 0, 0), 0), ("CandleCluster", (4.4, 0.72, 2.4), 0), (H, (4.0, -0.4, 2.4), -90)],
             lights=[((4.4, 0.6, 2.9), 35, WARM), ((1.2, 0, 2.6), 40, WARM), ((-2.0, 0, 3.0), 30, COOL)]),
        dict(label="CryptStair overview", items=[("CryptStair", (0, 0, 0), 0), (H, (4.1, -0.3, 2.4), 0)], az=-55, el=40),
        dict(label="WellShaft: inside, looking up", dark=True, lens=16, az=-30, el=-78, target=(0, 0, 4.2), dist=2.9,
             ground_col=STONE_GROUND, glow=3.0, items=[("WellShaft", (0, 0, 0), 0), ("Ladder_4m", (0.66, 0, 0), 0)],
             lights=[((0, 0, 3.9), 25, COOL), ((-0.3, 0.2, 1.0), 20, WARM)]),
        dict(label="WellShaft + Ladder from below", dark=True, lens=24, az=-20, el=-80, target=(0, 0, 2.4), dist=4.6,
             ground=-12.0, glow=3.0, items=[("WellShaft", (0, 0, 0), 0), ("Ladder_4m", (0.66, 0, 0), 0)],
             lights=[((0, 0, 3.8), 30, COOL), ((0, 0, 1.2), 25, WARM)]),
        dict(label="WellMouth + human", items=[("WellMouth", (0, 0, 0), 0), (H, (1.35, 0.4, 0), 0)], az=-25, el=34,
             target=(0.15, 0, 0.6), dist=3.4),
        dict(label="NicheWall close (candle)", dark=True, lens=35, az=-12, el=4, target=(0, 0, 1.25), dist=3.4,
             ground_col=STONE_GROUND, glow=3.0,
             items=[("NicheWall_2m", (0, 0, 0), 0), ("CandleCluster", (0.25, -0.3, 0), 0)],
             lights=[((0.25, -0.5, 0.45), 20, WARM), ((-0.5, -1.4, 1.8), 30, WARM)]),
        dict(label="Holes top-down check", items=[("DigHole_3", (0, 0, 0), 0), ("GraveOpen_2", (0, 1.9, 0), 0),
                                                 ("BuriedChest", (2.1, 0.6, 0), 0)], az=-5, el=70),
        dict(label="Gate + Sarcophagus close", items=[("CryptGate", (-1.0, 0.6, 0), 0), ("Sarcophagus", (0, -0.6, 0), 0)],
             az=-25, el=16),
        dict(label="Skulls close-up", items=[("SkullPile", (0, 0, 0), 0), ("BoneScatter", (1.0, -0.1, 0), 0)],
             az=-20, el=22, target=(0.45, 0, 0.2), dist=2.2),
    ]
    keep = set(names)

    def ok(t):
        return t["items"] == "LINEUP" or all(k in keep or k == "HUMAN" for k, _, _ in t["items"])
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
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(4)
    sun.data.color = (1.0, 0.95, 0.86)
    sc.collection.objects.link(sun)
    pts = []
    for k in range(4):
        L = bpy.data.objects.new(f"PL{k}", bpy.data.lights.new(f"PL{k}", "POINT"))
        L.data.shadow_soft_size = 0.05
        sc.collection.objects.link(L)
        pts.append(L)
    plain = _node_mat("P_Plain")
    glow = _node_mat("P_Glow", glow=True)
    by_key = {}
    for o in objs:
        o.data.materials[0] = glow if o.data.materials[0].name == MAT_NAME["Glow"] else plain
        by_key[o.name.replace("SM_KG_", "")] = o
    hm = MB("PV_Human")
    hm.cbox((0, 0, 0.9), (0.46, 0.28, 1.8), 0.05, C("E0673F"))
    hm.cbox((0, 0.13, 1.6), (0.3, 0.04, 0.08), 0.01, C("2B2B33"))      # "eyes" band: front = UE -Y
    human, _, _ = finalize(hm)
    human.data.materials[0] = plain
    by_key["HUMAN"] = human
    gm = bpy.data.meshes.new("PreviewGround")
    gm.from_pydata([(-300, -300, 0), (300, -300, 0), (300, 300, 0), (-300, 300, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PreviewGround", gm)
    sc.collection.objects.link(ground)
    gmat, gbsdf = _flat_mat("P_Ground", (0.34, 0.38, 0.26))
    ground.data.materials.append(gmat)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
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
    lbl.visible_shadow = False
    tmp_root = os.environ.get("KG_TMP") or tempfile.gettempdir()
    os.makedirs(tmp_root, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="kg_du_", dir=tmp_root)
    pool = list(by_key.values())

    def lineup_items():
        items, x = [], 0.0
        order = [n for n, _, _ in PROPS if n in by_key]
        for k in order:
            o = by_key[k]
            o.matrix_world = Matrix.Identity(4)
            bpy.context.view_layer.update()
            mn, mx = _bounds(o)
            items.append((k, (x - mn.x, (mn.y + mx.y) / 2, -mn.z), 0))      # UE y = -Blender y
            x += (mx.x - mn.x) + 0.6
            if k == "BuriedChest":
                items.append(("HUMAN", (x, 0, 0), 0))
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
            o.matrix_world = T(loc[0], -loc[1], loc[2]) @ R(-yaw, 'Z')
            shown.append((key, o))
        bpy.context.view_layer.update()
        corners = []
        for key, o in shown:
            corners += [o.matrix_world @ Vector(c) for c in o.bound_box]
        zmin = min(c.z for c in corners)
        ground.location.z = tile.get("ground", min(0.0, zmin)) - 0.01
        dark = tile.get("dark", False)
        if bg:
            bg.inputs["Color"].default_value = (0.012, 0.014, 0.02, 1.0) if dark else (0.52, 0.68, 0.86, 1.0)
            bg.inputs["Strength"].default_value = 1.0 if dark else 0.8
        gbsdf.inputs["Base Color"].default_value = (*tile.get("ground_col", (0.34, 0.38, 0.26)), 1.0)
        sun.hide_render = dark
        for k, L in enumerate(pts):
            lt = tile.get("lights", [])
            if k < len(lt):
                p, e, c = lt[k]
                L.hide_render = False
                L.location = (p[0], -p[1], p[2])
                L.data.energy = e
                L.data.color = c
            else:
                L.hide_render = True
        GLOW_NODE[0].inputs[1].default_value = tile.get("glow", 1.0)
        if em:
            em.inputs["Emission Color"].default_value = (1, 1, 1, 1)
            em.inputs["Emission Strength"].default_value = 1.0 if dark else 0.0
        lens = tile.get("lens", 50)
        cam.data.lens = lens
        tan_h = 18.0 / lens
        lbl.location = (-0.096 * tan_h, -0.096 * tan_h, -0.1)          # close to the lens: interiors can't hide it
        lbl_curve.size = 0.008 * tan_h
        el = math.radians(tile.get("el", 16))
        az = math.radians(tile.get("az", -35))
        d = Vector((math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el)))   # Blender axes
        az_b = math.atan2(d.x, -d.y)
        sun.rotation_euler = (math.radians(50), 0, az_b + math.radians(tile.get("sun", -55)))
        f = -d
        if "target" in tile:
            t = tile["target"]
            c = Vector((t[0], -t[1], t[2]))
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
        print(f"{LOG}: preview {path}")
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
    seen = set()
    for o in sorted(bpy.data.objects, key=lambda o: o.name):
        if o.type != "MESH":
            continue
        seen.add(o.name)
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
                        bad.append(f"{lab}.{ax}(blender)={g:.3f}!={w}")
        bm = bmesh.new()
        bm.from_mesh(me)
        loose = sum(1 for v in bm.verts if not v.link_faces)
        bm.free()
        mat = manifest.get(short, {}).get("material", "?")
        alpha_ok = (amax <= 0.001) if mat == "VC" else (amax >= 0.999 and amin <= 0.001)
        good = (ca is not None and color0.get(o.name, False) and not bad and loose == 0 and alpha_ok
                and o.location.length < 1e-6 and (not expect_tris or o.name in expect_tris))
        ok &= good
        print(f"{LOG}_VERIFY: {o.name:26s} tris {tris:5d}  dims {mx.x - mn.x:5.2f} x {mx.y - mn.y:5.2f} x {mx.z - mn.z:5.2f}"
              f"  UE min ({mn.x:6.3f},{-mx.y:6.3f},{mn.z:6.3f}) max ({mx.x:6.3f},{-mn.y:6.3f},{mx.z:6.3f})"
              f"  origin {tuple(round(c, 3) for c in o.location)}  attr={ca.name if ca else None}"
              f"/{ca.data_type if ca else ''} COLOR_0={color0.get(o.name)}  alpha[{amin:.2f},{amax:.2f}] {mat}"
              f"/{manifest.get(short, {}).get('collision', '?')}  loose={loose}  {'OK' if good else 'FAIL ' + str(bad)}")
    missing = set(expect_tris) - seen
    if missing:
        ok = False
        print(f"{LOG}_VERIFY: MISSING {sorted(missing)}")
    print(f"{LOG}_VERIFY: total tris {total}; {'OK' if ok else 'FAILED'}")


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
