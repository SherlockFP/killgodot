"""Harbour & coast dressing props for Morrowmere (pack KG_DressHarbour). Headless Blender 5.2, no textures.

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_harbour.py -- \
      [out.glb] [preview_dir] [--only NetRack,Anchor] [--no-preview] [--no-verify] [--tile 512]
      [--closeup Shipwreck,SmokeHut] [--outdir DIR] [--manifest Art/Packed/KG_DressHarbour_Clean.json]

Output: one mesh object per prop (SM_KG_<Name>, metres, Z up, pivot at the bottom centre = min Z 0), contact sheets
<preview_dir>/DressHarbour_preview_<n>.png and (with --manifest) the import manifest read by
Tools/Unreal/kg_import_dress_pack.py: {"props": {Name: {"material": VC|Glow|Sway, "collision": complex|box|none}}}.

Frame: Blender +X = UE +X and Blender +Y = UE -Y after the glTF round trip, so a prop's UE "front" (-Y, the
Frame convention of kg_dress_common) is built facing Blender +Y here.
Vertex colour attribute "Col" (FLOAT_COLOR, face corner): RGB = LINEAR albedo, A = mask:
  Glow (BellBuoy lamp, SmokeHut embers): 1 on emissive faces.   Sway (NetRack net, SeaweedClump blades,
  ShipwreckMast sail + loose ropes): 0 at the anchors -> 1 at the free-hanging ends.   VC: 0.
Double-sided thin parts (fins, net panels, flags) use a 0.6 mm offset back face so kg_sanitize_glb's merge-doubles
keeps both sides.
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
        if a in ("--no-preview", "--no-verify"):
            _opt[a] = True
        else:
            _opt[a] = _argv[_i + 1]
            _i += 1
    else:
        _pos.append(a)
    _i += 1
OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_DressHarbour.glb"
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"
TILE = int(_opt.get("--tile", 512))
ONLY = set(_opt["--only"].split(",")) if "--only" in _opt else None

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


def S(x, y, z):
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


def resample(pts, step):
    """Evenly spaced (position, tangent) samples along a polyline."""
    pts = [Vector(p) for p in pts]
    segs = [(a, b, (b - a).length) for a, b in zip(pts, pts[1:]) if (b - a).length > 1e-9]
    total = sum(s[2] for s in segs)
    n = max(1, int(round(total / step)))
    out = []
    for k in range(n):
        d = (k + 0.5) * total / n
        for a, b, L in segs:
            if d <= L:
                out.append((a.lerp(b, d / L), (b - a).normalized()))
                break
            d -= L
    return out


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

    def __init__(self, name, angle=38.0, material="M_KG_Harbour_VC"):
        self.name, self.angle, self.material = name, angle, material
        self.V, self.VC, self.F, self.FC, self.FS = [], [], [], [], []
        self.CN = {}
        self.M = Matrix.Identity(4)
        self.flip = False
        self._st = []
        self.center_xy = False

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
    def box(self, c, s, col, sh=FLAT, fc=None):
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
            if y.length < 1e-6:
                y = Vector((1, 0, 0)).cross(x)
        y.normalize()
        z = x.cross(y)
        with self.xf(frame((p0 + p1) * 0.5, x, y, z)):
            if bevel > 0:
                self.cbox((0, 0, 0), (L, w, h), bevel, col, sh)
            else:
                self.box((0, 0, 0), (L, w, h), col, FLAT if sh == AUTO else sh)

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
        """Extrude closed 2D profile [(side, up)] along a path. colfn(i, k, p_local)."""
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
        return self.sweep(pts, circle(sides, 1.0, phase), radii, radii, col, colfn, sh, cap0, cap1, False, up, tip,
                          capcol=capcol, rmod=rmod)

    def blob(self, c, r, sub=1, amp=0.15, freq=1.6, seed=0.0, col=None, colfn=None, sh=SMOOTH, rot=None):
        """Noisy ellipsoid. colfn(p_local, unit_dir)."""
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
            ids.append(self.v(p, colfn(p, u) if colfn else col))
        for a, b, cc in F:
            self.f((ids[a], ids[b], ids[cc]), None, sh)
        return ids


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


# ================================================================================================ palette
VC_M, SWAY_M, GLOW_M = "M_KG_Harbour_VC", "M_KG_Harbour_Sway", "M_KG_Harbour_Glow"
WOOD, WOOD_LT, WOOD_DK = C("8B5A33"), C("B9854F"), C("5A3A24")
GREYWOOD, GREYWOOD_LT, GREYWOOD_DK = C("8A7A68"), C("B4A58F"), C("5C5044")
DRIFT, DRIFT_DK = C("D2C7B2"), C("8E8474")
ROPE, ROPE_DK = C("D6B478"), C("A07C4A")
NET, NET_LT, NET_DK = C("2C9A88"), C("6CC4B0"), C("1B5E55")
ORANGE, RED, RED_DK, WHITE = C("F28A1E"), C("D8352A"), C("9E2A22"), C("F3EFE6")
TEAL, YELLOW, BLUE = C("1E9AA0"), C("F2C12E"), C("2F6DB5")
IRON, IRON_LT, RUST = C("34353B"), C("5E6068"), C("8C4A26")
ALGAE, ALGAE_DK, BARN = C("5A8A38"), C("2F4A26"), C("DAD4C4")
SAND, SAND_WET = C("E6CC92"), C("BD9E68")
EYE = C("141414")
FISH = {
    "mackerel": (C("2F7896"), C("E4ECEF")),
    "herring": (C("3F5F80"), C("EEF1F2")),
    "cod": (C("8E8558"), C("EDE6D2")),
    "snapper": (C("D8463A"), C("F6C0A8")),
    "smoked": (C("8A4A1C"), C("DDA457")),
    "dried": (C("9A8E7E"), C("DDD0B4")),
}


# ================================================================================================ shared helpers
def wv(base, rng, amt=0.35):
    """Per-piece colour variation."""
    return mix(base, mul(base, 1.28) if rng.random() < 0.5 else mul(base, 0.74), rng.uniform(0.0, amt))


def granite(p, base="9D978E", light="B9B3A9", dark="746F69", moss_amt=0.0, moss_up=None, seed=0.0, scale=1.0):
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


def grime(p, c, tide=1.0, band=0.25, seed=0.0, barn=0.85):
    """Sea grime below `tide` (z, m): wet darkening, algae and barnacle specks, fading in over `band`."""
    z = p[2]
    k = smoothstep(tide + band, tide - band, z)
    if k <= 0.0:
        return c
    p = Vector(p)
    wet = mul(c, 0.6)
    alg = mix(ALGAE_DK, ALGAE, 0.5 + 0.5 * nz(p, 4.0, (seed, 1, 2)))
    g = mix(wet, alg, clamp(0.3 + 0.7 * nz(p, 2.5, (2, seed, 5))))
    if barn > 0:
        b = smoothstep(0.2, 0.45, nz(p, 13.0, (seed, 7, 3))) * smoothstep(tide + 0.05, tide - 0.35, z)
        g = mix(g, BARN, b * barn)
    return with_a(mix(c, g, k), c[3])


def rock2(mb, center, half, seed, sub=2, cuts=6, amp=0.14, lean=0.0, colfn=None, col=None, flat=0.72):
    """Faceted boulder (plane cuts on a noisy icosphere), bottom flattened below -flat*half_z."""
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
        if flat and q.z < -half[2] * flat:
            q.z = -half[2] * flat + (q.z + half[2] * flat) * 0.15
        q = q + Vector(center)
        ids.append(mb.v(q, colfn(q, u) if colfn else (col or C("888888"))))
    for a, b, c in F:
        mb.f((ids[a], ids[b], ids[c]), None, AUTO)
    return ids


def torus(mb, R, r, nu=16, nv=6, col=None, colfn=None, fcol=None, sh=SMOOTH):
    """Torus in the local XY plane (axis Z). colfn(p, a, b); fcol(j) flat colour per major segment."""
    rings = []
    for j in range(nu):
        a = 2 * math.pi * j / nu
        ca, sa = math.cos(a), math.sin(a)
        ring = []
        for k in range(nv):
            b = 2 * math.pi * k / nv
            rr = R + r * math.cos(b)
            p = (rr * ca, rr * sa, r * math.sin(b))
            ring.append(mb.v(p, colfn(p, a, b) if colfn else col))
        rings.append(ring)
    for j in range(nu):
        jn = (j + 1) % nu
        for k in range(nv):
            kn = (k + 1) % nv
            mb.f((rings[j][k], rings[jn][k], rings[jn][kn], rings[j][kn]), fcol(j) if fcol else None, sh)


def ribbon(mb, a, b, n, w, ca, cb=None):
    """Single-sided strip from a to b, width w, facing n (net cords on a two-sided material)."""
    a, b = Vector(a), Vector(b)
    s = (b - a).cross(Vector(n))
    if s.length < 1e-9:
        return
    s = s.normalized() * (w * 0.5)
    cb = ca if cb is None else cb
    mb.f([mb.v(a - s, ca), mb.v(a + s, ca), mb.v(b + s, cb), mb.v(b - s, cb)], None, FLAT)


def poly2s(mb, pts, cols, off=0.0006, sh=FLAT):
    """Double-sided planar polygon (back face offset by `off` so merge-doubles keeps it)."""
    pts = [Vector(p) for p in pts]
    n = newell(pts)
    n = n.normalized() if n.length > 1e-12 else Vector((0, 0, 1))
    cols = cols if isinstance(cols, list) else [cols] * len(pts)
    mb.f([mb.v(p, c) for p, c in zip(pts, cols)], None, sh)
    mb.f([mb.v(p - n * off, c) for p, c in zip(pts, cols)][::-1], None, sh)


def grid_sheet(mb, G, colfn, double=False, skip=None, sh=SMOOTH, off=0.0008):
    """Surface through a grid of points G[i][j]; colfn(i, j, p). double=True adds an offset back side."""
    rows, cols = len(G), len(G[0])

    def nrm(i, j):
        a = G[i][min(j + 1, cols - 1)] - G[i][max(j - 1, 0)]
        b = G[min(i + 1, rows - 1)][j] - G[max(i - 1, 0)][j]
        n = a.cross(b)
        return n.normalized() if n.length > 1e-12 else Vector((0, 0, 1))

    for side in ((0, 1) if double else (0,)):
        ids = [[mb.v(G[i][j] - (nrm(i, j) * off if side else Vector()), colfn(i, j, G[i][j])) for j in range(cols)]
               for i in range(rows)]
        for i in range(rows - 1):
            for j in range(cols - 1):
                if skip and skip(i, j):
                    continue
                q = (ids[i][j], ids[i][j + 1], ids[i + 1][j + 1], ids[i + 1][j])
                mb.f(q if side == 0 else q[::-1], None, sh)


def plate(mb, c, t, w, pts2, th, col, sh=FLAT):
    """Convex polygon pts2 [(s, q)] in the plane (t, w) around c, extruded th along t x w."""
    c, t, w = Vector(c), Vector(t).normalized(), Vector(w).normalized()
    n = t.cross(w).normalized()
    top = [mb.v(c + t * s + w * q + n * th / 2, col) for s, q in pts2]
    bot = [mb.v(c + t * s + w * q - n * th / 2, col) for s, q in pts2]
    mb.f_out(top, c, None, sh)
    mb.f_out(bot, c, None, sh)
    m = len(pts2)
    for k in range(m):
        kn = (k + 1) % m
        mb.f_out([top[k], top[kn], bot[kn], bot[k]], c, None, sh)


def hexa(mb, P, col, sh=FLAT):
    """Convex 8-corner block: P[0..3] bottom ring, P[4..7] top ring (same order)."""
    ids = [mb.v(p, col) for p in P]
    cen = sum((Vector(p) for p in P), Vector()) / 8
    for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        mb.f_out([ids[i] for i in f], cen, None, sh)


def dome(mb, c, rx, ry, h, rings=6, segs=20, rim=None, zfn=None, colfn=None, col=None, sh=SMOOTH, power=0.8):
    """Open polar heightfield cap z = cz + h (1 - rn^2)^power (+ zfn(x, y, rn)); rim(theta) scales the outline.
    Returns zat(x, y) -> (z or None, rn) for draping things on it."""
    cx, cy, cz = c

    def zat(x, y):
        dx, dy = (x - cx) / rx, (y - cy) / ry
        th = math.atan2(dy, dx)
        s = rim(th) if rim else 1.0
        rn = math.hypot(dx, dy) / s
        if rn > 1.0 + 1e-6:
            return None, rn
        rn = min(rn, 1.0)
        return cz + h * (1 - rn * rn) ** power + (zfn(x, y, rn) if zfn else 0.0), rn

    def mk(x, y):
        z, rn = zat(x, y)
        p = (x, y, cz if z is None else z)
        return mb.v(p, colfn(p, rn) if colfn else col)

    ctr = mk(cx, cy)
    RR = []
    for k in range(1, rings + 1):
        rn = k / rings
        ring = []
        for m in range(segs):
            th = 2 * math.pi * m / segs
            s = rim(th) if rim else 1.0
            ring.append(mk(cx + rx * rn * s * math.cos(th), cy + ry * rn * s * math.sin(th)))
        RR.append(ring)
    for m in range(segs):
        mn = (m + 1) % segs
        mb.f((ctr, RR[0][m], RR[0][mn]), None, sh)
        for k in range(rings - 1):
            mb.f((RR[k][m], RR[k + 1][m], RR[k + 1][mn], RR[k][mn]), None, sh)
    return zat


def cyl(mb, p0, p1, r, sides=8, col=None, colfn=None, sh=AUTO, r1=None, tip=0.0, capcol=None):
    return mb.tube([p0, p1], [r, r if r1 is None else r1], sides, col=col, colfn=colfn, sh=sh, tip=tip, capcol=capcol)


def rope(mb, pts, r=0.014, sides=5, col=ROPE, alpha=None):
    """Tube along pts; alpha(s in 0..1) sets the Sway mask along the rope."""
    n = len(pts)
    colfn = (lambda i, k, p: with_a(col, alpha(i / max(1, n - 1)))) if alpha else None
    mb.tube(pts, [r] * n, sides, col=col, colfn=colfn, sh=AUTO)


def catenary(p0, p1, sag, n=10):
    p0, p1 = Vector(p0), Vector(p1)
    return [p0.lerp(p1, i / n) - Vector((0, 0, sag * 4 * (i / n) * (1 - i / n))) for i in range(n + 1)]


def rope_coil(mb, c, r0, turns=3.0, rr=0.016, col=ROPE, seed=0.0):
    """Flat coil of rope lying on the ground around c (spiralling inward)."""
    cx, cy, cz = c
    n = int(turns * 14)
    pts = []
    for i in range(n + 1):
        t = i / n * turns * 2 * math.pi
        r = r0 - (t / (2 * math.pi)) * rr * 2.1
        z = cz + rr * (1.0 + 0.3 * max(0.0, math.sin(t * 1.7 + seed)))
        pts.append((cx + r * math.cos(t + seed), cy + r * math.sin(t + seed), z))
    rope(mb, pts, rr, 5, col)
    return pts


def cork(mb, c, d, r=0.04, L=0.1, col=ORANGE):
    """Net float threaded along direction d."""
    d = Vector(d).normalized()
    s = Vector((0, 0, 1)).cross(d)
    if s.length < 1e-6:
        s = Vector((1, 0, 0))
    s.normalize()
    u = d.cross(s)
    with mb.xf(frame(Vector(c), s, u, d)):
        mb.lathe([(0, -L / 2), (r * 0.8, -L / 2), (r, -L * 0.28), (r, L * 0.28), (r * 0.8, L / 2), (0, L / 2)], 8,
                 col=col, sh=AUTO)


def chain_link(mb, c, t, s, R=0.034, r=0.009, stretch=1.55, col=IRON):
    t = Vector(t).normalized()
    s = Vector(s)
    s = (s - t * s.dot(t))
    s = s.normalized() if s.length > 1e-6 else Vector((0, 0, 1))
    n = t.cross(s)
    with mb.xf(frame(Vector(c), t, s, n) @ S(stretch, 1.0, 1.0)):
        torus(mb, R, r, 8, 4, col=col, sh=AUTO)


def chain(mb, pts, step=0.1, R=0.034, r=0.009, colfn=None):
    for k, (p, t) in enumerate(resample(pts, step)):
        a = Vector((0, 0, 1)).cross(t)
        if a.length < 1e-6:
            a = Vector((1, 0, 0))
        side = a if k % 2 == 0 else t.cross(a)
        chain_link(mb, p, t, side, R, r, col=colfn(p) if colfn else IRON)


FISH_PROF = [(0.0, 0.14), (0.07, 0.64), (0.22, 1.0), (0.45, 0.86), (0.64, 0.45), (0.76, 0.2)]


def fish(mb, M, L, kind="mackerel", flat=0.5, eyes=True, alpha=0.0):
    """Low-poly fish in frame M: nose at the origin, body along +X to the tail tip at X=L, back towards +Z."""
    back, belly = FISH[kind]
    back, belly = with_a(back, alpha), with_a(belly, alpha)
    mid = mix(back, belly, 0.55)
    H = L * 0.22
    ring = [mid, back, back, mid, belly, belly]
    with mb.xf(M):
        pts = [(s * L, 0.0, 0.0) for s, _ in FISH_PROF]
        sy = [H * 0.5 * h for _, h in FISH_PROF]
        sx = [v * flat for v in sy]
        mb.sweep(pts, circle(6), sx, sy, colfn=lambda i, k, p: ring[k], sh=SMOOTH)
        fin = mix(back, belly, 0.3)
        rt, rb = (0.74 * L, 0, 0.1 * H), (0.74 * L, 0, -0.1 * H)
        tt, tb, nt = (L, 0, 0.62 * H), (L, 0, -0.62 * H), (0.9 * L, 0, 0)
        for tri in ((rt, tt, nt), (rt, nt, rb), (rb, nt, tb)):
            poly2s(mb, tri, fin)
        poly2s(mb, [(0.28 * L, 0, 0.42 * H), (0.52 * L, 0, 0.38 * H), (0.36 * L, 0, 0.8 * H)], back)
        if eyes:
            ey = 0.5 * 0.64 * H * flat
            for sgn in (-1, 1):
                mb.box((0.075 * L, sgn * ey, 0.12 * H), (0.1 * H, 0.012, 0.1 * H), with_a(EYE, alpha))


def starfish(mb, M, r=0.1, col=None, alpha=0.0, seed=0):
    """Five-armed star lying in local XY (top towards +Z), radius r."""
    rng = random.Random(seed)
    col = col or C("E8612C")
    tipc = mix(col, C("FFD9A0"), 0.35)
    with mb.xf(M):
        h = r * 0.24
        top = mb.v((0, 0, h), with_a(mix(col, C("FFB070"), 0.3), alpha))
        mid, out, bot = [], [], []
        for k in range(10):
            ang = 2 * math.pi * k / 10 + rng.uniform(-0.05, 0.05)
            rr = r * (1.0 if k % 2 == 0 else 0.4) * rng.uniform(0.9, 1.05)
            ca, sa = math.cos(ang), math.sin(ang)
            mid.append(mb.v((rr * 0.5 * ca, rr * 0.5 * sa, h * 0.8), with_a(col, alpha)))
            out.append(mb.v((rr * ca, rr * sa, h * 0.3), with_a(tipc if k % 2 == 0 else col, alpha)))
            bot.append(mb.v((rr * ca * 0.97, rr * sa * 0.97, 0.0), with_a(mul(col, 0.6), alpha)))
        for k in range(10):
            kn = (k + 1) % 10
            mb.f((top, mid[k], mid[kn]), None, SMOOTH)
            mb.f((mid[k], out[k], out[kn], mid[kn]), None, SMOOTH)
            mb.f((out[k], bot[k], bot[kn], out[kn]), None, AUTO)
        mb.f(list(reversed(bot)), None, FLAT)


def lobster_float(mb, M, cols, alpha=0.0):
    """Foam trap float (spindle along local +Z, 0..0.3 m) on a stick."""
    with mb.xf(M):
        prof = [(0, 0), (0.03, 0.005), (0.055, 0.04), (0.065, 0.1), (0.065, 0.2), (0.05, 0.26), (0.025, 0.295), (0, 0.3)]
        seg = []
        for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
            zm = (z0 + z1) / 2
            seg.append(with_a(cols[0] if zm < 0.1 else cols[1] if zm < 0.2 else cols[2], alpha))
        mb.lathe(prof, 10, col=with_a(cols[0], alpha), segcol=seg, sh=SMOOTH)
        cyl(mb, (0, 0, -0.14), (0, 0, 0.44), 0.011, 5, col=with_a(WOOD_LT, alpha))


def tuft(mb, c, h, col, n=3, rng=None, alpha=0.0):
    """Cheap grass tuft: n crossed double-sided blades."""
    rng = rng or random
    c = Vector(c)
    for k in range(n):
        a = k * math.pi / n + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(a), math.sin(a), 0)) * (0.05 + h * 0.15)
        lean = Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0)) * h
        poly2s(mb, [c - d, c + d, c + lean], [with_a(mul(col, 0.7), alpha), with_a(mul(col, 0.7), alpha),
                                                 with_a(mix(col, C("E8E4A0"), 0.25), alpha)])


# ================================================================================================ 1. NetRack
def make_net_rack():
    mb = MB("SM_KG_NetRack", angle=40, material=SWAY_M)
    rng = random.Random(21)
    half, bar_z, bar_y = 1.5, 2.4, 0.105

    def pole_col(i, k, p):
        g = 0.5 + 0.5 * nz((p[0] * 3.0 + k, p[1] * 3.0, p[2] * 0.8), 1.0, (3, 1, 2))
        return with_a(mix(GREYWOOD_DK, GREYWOOD_LT, g), 0.0)

    for sx in (-1, 1):
        def px(z):
            return sx * half - sx * 0.035 * z / 2.65
        mb.tube([(px(z), 0.0, z) for z in (0.0, 0.8, 1.6, 2.2, 2.65)], [0.075, 0.07, 0.066, 0.062, 0.058], 8,
                colfn=pole_col, sh=AUTO)
        with mb.xf(T(px(bar_z), 0, bar_z)):
            torus(mb, 0.07, 0.013, 10, 4, col=ROPE_DK, sh=AUTO)
        with mb.xf(T(px(bar_z) - sx * 0.02, bar_y, bar_z) @ R(90, 'Y')):
            torus(mb, 0.052, 0.012, 10, 4, col=ROPE_DK, sh=AUTO)
        for k in range(2):
            a = rng.uniform(0, 2 * math.pi) + k * math.pi
            rock2(mb, (px(0) + 0.13 * math.cos(a), 0.13 * math.sin(a), 0.04), (0.09, 0.075, 0.06), seed=5 + k + sx,
                  sub=1, cuts=4, colfn=lambda q, u: granite(q, seed=2.0))
    bar_pts = [(x, bar_y, bar_z - 0.012 * math.sin(math.pi * (x + 1.68) / 3.36)) for x in np.linspace(-1.68, 1.68, 7)]
    mb.tube(bar_pts, [0.045] * 7, 7, col=C("A58B69"), sh=AUTO)

    # ---- the net: diamond mesh of cords hanging from the bar, pleated, billowing towards +Y
    x0, x1 = -1.36, 1.36
    top_z = bar_z - 0.055
    NI, NJ = 30, 20

    def bottom(u):
        return 0.42 + 0.1 * math.sin(u * 5.3 + 0.6) + 0.06 * nz((u * 3.0, 0.5, 0.2))

    def P(u, v):
        x = x0 + (x1 - x0) * u + 0.03 * math.sin(v * 3.0 + u * 11.0) * v
        z = top_z - v * (top_z - bottom(u))
        y = bar_y + 0.05 + v * (0.13 * math.sin(math.pi * u) + 0.045 * math.sin(u * 23.0) + 0.04 * nz((u * 4.0, v * 3.0, 1.0)))
        return Vector((x, y, z))

    def ncol(u, v, amax=1.0):
        g = 0.5 + 0.5 * nz((u * 6.0, v * 6.0, 2.0))
        c = mix(NET, NET_LT, g * 0.55 + 0.3 * (1 - v))
        if math.hypot((u - 0.7) / 0.07, (v - 0.55) / 0.11) < 1.0:
            c = C("E27A2C")          # a fresh orange repair patch
        return with_a(c, clamp(v) ** 1.1 * amax)

    def lattice(Pf, nj, colf, normal):
        for i in range(NI):
            for j in range(nj + 1):
                if (i + j) % 2:
                    continue
                for dj in (-1, 1):
                    j2 = j + dj
                    if 0 <= j2 <= nj:
                        ribbon(mb, Pf(i / NI, j / nj), Pf((i + 1) / NI, j2 / nj), normal, 0.021,
                               colf(i / NI, j / nj), colf((i + 1) / NI, j2 / nj))

    lattice(P, NJ, ncol, (0, 1, 0))
    rope(mb, [P(i / NI, 0.0) for i in range(0, NI + 1, 2)], 0.012, 4, ROPE, alpha=lambda s: 0.0)
    rope(mb, [P(i / NI, 1.0) for i in range(0, NI + 1, 2)], 0.014, 4, ROPE_DK, alpha=lambda s: 1.0)
    for u in (0.0, 1.0):
        rope(mb, [P(u, j / NJ) for j in range(0, NJ + 1, 2)], 0.011, 4, ROPE, alpha=lambda s: s)
    for i in range(1, NI, 3):
        cork(mb, P(i / NI, 0.0) + Vector((0, 0.015, -0.035)), (1, 0, 0), 0.038, 0.095, ORANGE)
        p = P(i / NI, 1.0)
        with mb.xf(T(p.x, p.y, p.z - 0.012)):
            mb.lathe([(0, -0.028), (0.016, -0.02), (0.019, 0.0), (0.016, 0.02), (0, 0.028)], 6,
                     col=with_a(C("5B6068"), 1.0), sh=AUTO)

    # back drape (the net folded over the bar)
    def Pb(u, v):
        x = x0 + 0.05 + (x1 - x0 - 0.1) * u
        z = top_z - v * (top_z - (1.4 + 0.16 * math.sin(u * 4.1 + 1.0)))
        y = bar_y - 0.055 - v * (0.05 + 0.035 * math.sin(u * 19.0 + 0.5))
        return Vector((x, y, z))

    lattice(Pb, 10, lambda u, v: ncol(u, v * 0.4 + 0.3, 0.5), (0, -1, 0))
    rope(mb, [Pb(i / NI, 1.0) for i in range(0, NI + 1, 2)], 0.012, 4, ROPE, alpha=lambda s: 0.5)
    # a starfish tangled in the mesh
    p = P(0.31, 0.78) + Vector((0, 0.03, 0))
    starfish(mb, T(*p) @ R(-90, 'X') @ R(20, 'Z'), 0.085, alpha=0.8, seed=3)
    return mb


# ================================================================================================ 2. NetPile
def make_net_pile():
    mb = MB("SM_KG_NetPile", angle=45)
    rng = random.Random(22)
    a, b, H = 0.72, 0.52, 0.42

    def rim(th):
        return 1.0 + 0.14 * nz((math.cos(th) * 1.3, math.sin(th) * 1.3, 0.4))

    def zf(x, y, rn):
        k = 1 - rn * rn
        return (0.05 * math.sin(x * 11.0 + 4.0 * nz((x, y, 0.3), 2.0)) + 0.07 * nz((x * 2.0, y * 2.0, 1.7))) * k

    zat = dome(mb, (0, 0, -0.01), a, b, H, rings=9, segs=28, rim=rim, zfn=zf,
               colfn=lambda p, rn: with_a(mix(NET_DK, mul(NET_DK, 1.4), 0.5 + 0.5 * nz(p, 5.0)), 0.0))

    def surf_n(x, y):
        e = 0.02
        zx = (zat(x + e, y)[0] or 0) - (zat(x - e, y)[0] or 0)
        zy = (zat(x, y + e)[0] or 0) - (zat(x, y - e)[0] or 0)
        return Vector((-zx / (2 * e), -zy / (2 * e), 1.0)).normalized()

    for ang in (math.radians(40), math.radians(-50)):
        d = Vector((math.cos(ang), math.sin(ang), 0))
        pp = Vector((-d.y, d.x, 0))
        off = -1.0
        while off < 1.0:
            run, t = [], -1.0
            while t <= 1.02:
                q = pp * off + d * t
                z, rn = zat(q.x, q.y)
                if z is not None and rn < 0.95:
                    run.append(Vector((q.x, q.y, z)))
                elif run:
                    break
                t += 0.045
            for p0, p1 in zip(run, run[1:]):
                n = surf_n(p0.x, p0.y)
                c = mix(NET, NET_LT, 0.2 + 0.5 * clamp(p0.z / H) + 0.2 * nz(p0, 6.0))
                ribbon(mb, p0 + n * 0.012, p1 + surf_n(p1.x, p1.y) * 0.012, n, 0.017, c)
            off += 0.075
    for k in range(6):
        for _ in range(40):
            x, y = rng.uniform(-a, a), rng.uniform(-b, b)
            z, rn = zat(x, y)
            if z is not None and rn < 0.8:
                break
        yaw = rng.uniform(0, math.pi)
        cork(mb, (x, y, z + 0.03), (math.cos(yaw), math.sin(yaw), 0.15 * rng.uniform(-1, 1)), 0.04, 0.1,
             ORANGE if k % 3 else YELLOW)
    rope_coil(mb, (0.7, -0.42, 0.0), 0.2, 3.0, 0.017, ROPE, seed=1.0)
    trail = []
    for s in np.linspace(0, 1, 9):
        x, y = lerp(0.52, 0.05, s), lerp(-0.34, 0.02, s)
        z = zat(x, y)[0]
        trail.append((x, y, (z if z is not None else 0.0) + 0.016))
    rope(mb, [(0.7 + 0.2, -0.42, 0.017)] + trail, 0.016, 5, ROPE)
    return mb


# ================================================================================================ 3. LobsterTrap
def make_lobster_trap():
    mb = MB("SM_KG_LobsterTrap", angle=40)
    rng = random.Random(31)
    wood, wood_dk = C("A68A66"), C("6B5340")
    L, W, Rh, zc = 0.92, 0.56, 0.27, 0.075
    for sy in (-1, 1):
        mb.cbox((0, sy * 0.25, 0.028), (L, 0.06, 0.056), 0.008, wv(wood_dk, rng))
    for k in range(6):
        mb.cbox((-0.4 + 0.16 * k, 0, 0.066), (0.07, W, 0.02), 0.005, wv(wood, rng))
    for x in (-0.42, 0.0, 0.42):
        mb.tube([(x, Rh * math.cos(t), zc + Rh * math.sin(t)) for t in np.linspace(0, math.pi, 12)], [0.017] * 12, 5,
                col=wood_dk, sh=AUTO)
    for k in range(1, 9):
        th = k * math.pi / 9
        rr = Rh + 0.02
        y, z = rr * math.cos(th), zc + rr * math.sin(th)
        mb.beam((-0.475, y, z), (0.475, y, z), 0.05, 0.012, wv(wood, rng, 0.5),
                up=(0, math.cos(th), math.sin(th)), bevel=0.003)
    # netting heads at both ends with a funnel entrance
    netc = C("2E7E6E")
    hc = Vector((0.0, zc + 0.12))
    rh = 0.08
    for sx in (-1, 1):
        x = sx * 0.455
        hole, outer = [], []
        for m in range(16):
            ph = 2 * math.pi * m / 16
            d = Vector((math.cos(ph), math.sin(ph)))
            h0 = hc.y - zc
            bb = h0 * d.y
            cc = h0 * h0 - Rh * Rh
            t = -bb + math.sqrt(bb * bb - cc)
            if d.y < -1e-6:
                t = min(t, (zc + 0.005 - hc.y) / d.y)
            hole.append(hc + d * rh)
            outer.append(hc + d * t)
        for m in range(16):
            mn = (m + 1) % 16
            q = [(x, hole[m].x, hole[m].y), (x, outer[m].x, outer[m].y), (x, outer[mn].x, outer[mn].y),
                 (x, hole[mn].x, hole[mn].y)]
            poly2s(mb, q, [netc, mul(netc, 0.8), mul(netc, 0.8), netc], sh=SMOOTH)
        # funnel into the trap
        x2 = sx * 0.2
        for m in range(12):
            a0, a1 = 2 * math.pi * m / 12, 2 * math.pi * (m + 1) / 12
            q = [(x, hc.x + rh * math.cos(a0), hc.y + rh * math.sin(a0)),
                 (x2, hc.x + 0.04 * math.cos(a0), hc.y - 0.02 + 0.04 * math.sin(a0)),
                 (x2, hc.x + 0.04 * math.cos(a1), hc.y - 0.02 + 0.04 * math.sin(a1)),
                 (x, hc.x + rh * math.cos(a1), hc.y + rh * math.sin(a1))]
            poly2s(mb, q, [netc, mul(netc, 0.55), mul(netc, 0.55), netc], sh=SMOOTH)
        mb.box((x, 0, zc + 0.005), (0.03, W - 0.02, 0.02), wood_dk)
    mb.blob((0.0, 0.0, 0.13), (0.07, 0.05, 0.05), sub=1, amp=0.2, col=C("E6DCC6"))
    mb.cbox((0.28, 0.1, 0.105), (0.2, 0.09, 0.06), 0.008, C("B0503A"))
    # rope from the top down to a coil and a float beside the trap
    top = (0.05, 0.0, zc + Rh + 0.035)
    rope(mb, [top, (0.08, 0.14, zc + Rh), (0.1, 0.27, 0.2), (0.12, 0.34, 0.03), (0.15, 0.42, 0.017)], 0.014, 5, ROPE)
    rope_coil(mb, (0.02, 0.5, 0.0), 0.13, 2.5, 0.014, ROPE, seed=2.0)
    lobster_float(mb, T(0.34, 0.48, 0.065) @ R(-25, 'Z') @ R(90, 'Y') @ T(0, 0, -0.15), (YELLOW, RED, WHITE))
    return mb


# ================================================================================================ 4. LobsterTrapStack
def box_trap(mb, M, paint, rng):
    L, W, H = 0.9, 0.55, 0.38
    dk = mix(paint, C("3A3028"), 0.45)
    with mb.xf(M):
        for sx in (-1, 1):
            for sy in (-1, 1):
                mb.box((sx * (L / 2 - 0.02), sy * (W / 2 - 0.02), H / 2), (0.04, 0.04, H), dk)
        for k in range(4):
            mb.box((0, -W / 2 + 0.07 + k * (W - 0.14) / 3, 0.011), (L - 0.02, 0.06, 0.022), wv(paint, rng, 0.3))
        for sy in (-1, 1):
            for k in range(4):
                mb.box((0, sy * (W / 2 - 0.004), 0.06 + k * 0.092), (L - 0.03, 0.012, 0.05), wv(paint, rng, 0.3))
        for k in range(5):
            mb.box((0, -W / 2 + 0.05 + k * (W - 0.1) / 4, H - 0.006), (L - 0.02, 0.05, 0.012), wv(paint, rng, 0.3))
        netc = C("2E7E6E")
        for sx in (-1, 1):
            x = sx * (L / 2 - 0.02)
            mb.box((x, 0, H - 0.02), (0.04, W, 0.04), dk)
            mb.box((x, 0, 0.03), (0.04, W, 0.04), dk)
            xp = sx * (L / 2 - 0.035)
            poly2s(mb, [(xp, -W / 2 + 0.04, 0.05), (xp, W / 2 - 0.04, 0.05), (xp, W / 2 - 0.04, H - 0.04),
                        (xp, -W / 2 + 0.04, H - 0.04)], netc)
            xo = sx * (L / 2 - 0.0305)
            poly2s(mb, [(xo, -0.06, 0.13), (xo, 0.06, 0.13), (xo, 0.06, 0.25), (xo, -0.06, 0.25)], C("12302A"))


def make_lobster_trap_stack():
    mb = MB("SM_KG_LobsterTrapStack", angle=40)
    rng = random.Random(32)
    paints = [C("A68A66"), C("4FA3A0"), C("E0B23A"), C("C9563F"), C("A68A66"), C("6E9FC9")]
    H = 0.38
    layout = [((0.0, -0.29, 0.0), 0.0), ((0.03, 0.29, 0.0), 2.0), ((0.02, -0.27, H), -3.0), ((-0.05, 0.3, H), 4.0),
              ((0.0, 0.01, 2 * H), 88.0)]
    for k, (p, yaw) in enumerate(layout):
        box_trap(mb, T(*p) @ R(yaw, 'Z'), paints[k], rng)
    box_trap(mb, T(0.49, -0.3, 0.45) @ R(90, 'Y'), paints[5], rng)
    rope_coil(mb, (0.05, 0.1, 3 * H), 0.16, 3.0, 0.015, ROPE, seed=0.5)
    lobster_float(mb, T(-0.25, -0.62, 0.065) @ R(15, 'Z') @ R(90, 'Y') @ T(0, 0, -0.15), (RED, WHITE, BLUE))
    lobster_float(mb, T(0.25, -0.72, 0.065) @ R(-40, 'Z') @ R(90, 'Y') @ T(0, 0, -0.15), (YELLOW, C("2A8C5A"), YELLOW))
    return mb


# ================================================================================================ 5. Buoys
def make_buoy_red():
    mb = MB("SM_KG_Buoy_Red", angle=35)
    zc, R0, RZ = 0.52, 0.38, 0.36
    prof = [(0, zc - RZ)]
    n = 14
    for k in range(1, n):
        ang = -math.pi / 2 + math.pi * k / n
        prof.append((R0 * math.cos(ang), zc + RZ * math.sin(ang)))
    prof.append((0, zc + RZ))
    segcol = [None] * n
    segcol[6] = segcol[7] = WHITE
    mb.lathe(prof, 16, colfn=lambda r, z, a, i: grime((r * math.cos(a), r * math.sin(a), z), RED, 0.33, 0.07, 1.0),
             segcol=segcol, sh=SMOOTH)
    top = zc + RZ
    mb.lathe([(0, top - 0.02), (0.09, top - 0.02), (0.09, top + 0.02), (0, top + 0.03)], 10, col=IRON, sh=AUTO)
    with mb.xf(T(0, 0, top + 0.09) @ R(90, 'X')):
        torus(mb, 0.06, 0.016, 10, 5, col=IRON_LT, sh=AUTO)
    bot = zc - RZ
    mb.lathe([(0, bot - 0.03), (0.06, bot - 0.02), (0.06, bot + 0.02), (0, bot + 0.03)], 8, col=IRON, sh=AUTO)
    chain(mb, [(0, 0, bot - 0.02), (0.0, 0.0, 0.0)], step=0.075, R=0.022, r=0.007,
          colfn=lambda p: grime(p, IRON, 0.3, 0.1, 2.0))
    return mb


def make_buoy_striped():
    mb = MB("SM_KG_Buoy_Striped", angle=35)

    def r_of(z):
        if z <= 0.3:
            return 0.07
        if z <= 0.42:
            return lerp(0.07, 0.34, smoothstep(0.3, 0.42, z) ** 0.7)
        if z <= 0.95:
            return 0.34
        if z <= 1.4:
            return lerp(0.34, 0.08, (z - 0.95) / 0.45)
        return 0.06

    zs = [0.0, 0.3, 0.36, 0.42, 0.5, 0.68, 0.86, 0.95, 1.1, 1.25, 1.4, 1.58, 1.76, 1.94]
    prof = [(0, 0.0)] + [(r_of(z), z) for z in zs] + [(0, 1.94)]
    segcol = []
    for k in range(len(prof) - 1):
        zm = (prof[k][1] + prof[k + 1][1]) / 2
        if zm < 0.5:
            segcol.append(None)
        else:
            band = int((zm - 0.5) / 0.18) if zm < 0.95 else 2 + int((zm - 0.95) / 0.16)
            segcol.append(RED if band % 2 == 0 else WHITE)
    segcol[-1] = RED
    mb.lathe(prof, 14, colfn=lambda r, z, a, i: grime((r * math.cos(a), r * math.sin(a), z), C("3A3B42"), 0.46, 0.08, 3.0),
             segcol=segcol, sh=AUTO)
    mb.lathe([(0, 1.93), (0.06, 1.94), (0.12, 2.02), (0.13, 2.08), (0.11, 2.15), (0.06, 2.19), (0, 2.2)], 12, col=RED,
             sh=SMOOTH)
    for sx in (-1, 1):
        with mb.xf(T(sx * 0.2, 0, 0.99) @ R(90, 'X')):
            torus(mb, 0.045, 0.012, 8, 4, col=IRON_LT, sh=AUTO)
    return mb


def make_bell_buoy():
    mb = MB("SM_KG_BellBuoy", angle=35, material=GLOW_M)
    hull_red, hull_blk, deck = C("D23A2C"), C("2C2D33"), C("4A4B52")
    prof = [(0, 0.0), (0.14, 0.0), (0.14, 0.42), (0.34, 0.5), (0.62, 0.6), (0.84, 0.74), (0.93, 0.9), (0.94, 1.02),
            (0.94, 1.1), (0.94, 1.18), (0.92, 1.26), (0.84, 1.31), (0.0, 1.33)]

    def colfn(r, z, a, i):
        return grime((r * math.cos(a), r * math.sin(a), z), hull_blk if z < 0.95 else hull_red, 0.9, 0.12, 1.0)

    segcol = [None] * 12
    segcol[8] = WHITE
    segcol[11] = deck
    mb.lathe(prof, 20, colfn=colfn, segcol=segcol, sh=AUTO)
    with mb.xf(T(0, 0, 0.98)):
        torus(mb, 0.965, 0.055, 20, 6, col=C("222226"), sh=SMOOTH)
    z0, z1, r0, r1 = 1.31, 2.9, 0.52, 0.2

    def leg(sx, sy, z):
        t = (z - z0) / (z1 - z0)
        r = r0 + (r1 - r0) * t
        return Vector((sx * r, sy * r, z))

    corners = [(1, 1), (-1, 1), (-1, -1), (1, -1)]
    for sx, sy in corners:
        mb.beam(leg(sx, sy, z0), leg(sx, sy, z1), 0.075, 0.075, hull_red, bevel=0.012)
    for k in range(4):
        c0, c1 = corners[k], corners[(k + 1) % 4]
        for z in (2.05, 2.72):
            mb.beam(leg(*c0, z), leg(*c1, z), 0.06, 0.06, hull_red, bevel=0.01)
        mb.beam(leg(*c0, z0 + 0.06), leg(*c1, 2.02), 0.045, 0.04, hull_red)
        mb.beam(leg(*c1, z0 + 0.06), leg(*c0, 2.02), 0.045, 0.04, hull_red)
    rx = leg(1, 1, 2.72).x
    mb.beam((-rx, 0, 2.72), (rx, 0, 2.72), 0.07, 0.07, hull_red, bevel=0.01)
    bronze, bronze_dk = C("D1A145"), C("7E5A22")
    bprof = [(0.0, 2.36), (0.12, 2.33), (0.16, 2.2), (0.19, 2.08), (0.215, 2.0), (0.245, 1.97), (0.252, 1.995),
             (0.228, 2.05), (0.19, 2.2), (0.165, 2.4), (0.12, 2.5), (0.0, 2.52)]
    mb.lathe(bprof, 16, colfn=lambda r, z, a, i: bronze_dk if i < 5 else mix(bronze, C("F0D080"), 0.3 * (z > 2.3)),
             sh=AUTO)
    with mb.xf(T(0, 0, 2.6) @ R(90, 'X')):
        torus(mb, 0.05, 0.013, 8, 4, col=IRON, sh=AUTO)
    for sx, sy in corners:
        top = leg(sx, sy, 2.6)
        d = Vector((sx, sy, 0)).normalized()
        end = d * 0.3 + Vector((0, 0, 2.08))
        cyl(mb, top, end, 0.012, 5, col=IRON)
        mb.blob(end - Vector((0, 0, 0.02)), 0.045, sub=1, amp=0.05, col=IRON_LT)
    mb.cbox((0, 0, 2.93), (0.48, 0.48, 0.06), 0.012, hull_red)
    blk = C("1E1F24")
    lens = C("FF5A36", a=1.0)
    lprof = [(0, 2.96), (0.09, 2.96), (0.09, 3.03), (0.1, 3.04), (0.1, 3.2), (0.125, 3.21), (0.125, 3.24), (0.03, 3.34),
             (0, 3.35)]
    mb.lathe(lprof, 12, col=blk, segcol=[blk, blk, blk, lens, blk, blk, blk, blk], sh=AUTO)
    return mb


# ================================================================================================ 6. Anchor, Bollard, Piling
def make_anchor():
    mb = MB("SM_KG_Anchor", angle=40)

    def icol(p):
        c = mix(IRON, RUST, 0.75 * smoothstep(0.05, 0.6, nz(p, 3.0, (1, 2, 3))))
        c = mul(c, 0.92 + 0.12 * nz(p, 9.0))
        return grime(p, c, 0.22, 0.1, 4.0, barn=0.5)

    mb.tube([(0, 0, 0.08), (0, 0, 0.8), (0, 0, 1.6)], [0.058, 0.052, 0.045], 8, colfn=lambda i, k, p: icol(p), sh=AUTO)
    mb.blob((0, 0, 0.1), (0.1, 0.075, 0.085), sub=1, amp=0.05, colfn=lambda p, u: icol(p))
    for s in (-1, 1):
        pts = bezier((0, 0, 0.1), (s * 0.3, 0, 0.02), (s * 0.62, 0, 0.2), (s * 0.66, 0, 0.62), 10)
        radii = [lerp(0.056, 0.036, k / 10) for k in range(11)]
        mb.tube(pts, radii, 8, colfn=lambda i, k, p: icol(p), sh=AUTO, tip=0.07)
        t = (pts[8] - pts[6]).normalized()
        plate(mb, pts[7] + Vector((-s * 0.02, 0, 0)), t, (0, 1, 0),
              [(-0.2, 0), (0.0, -0.13), (0.14, -0.1), (0.2, 0), (0.14, 0.1), (0.0, 0.13)], 0.035, icol(pts[7]))
    mb.tube([(0, -0.66, 1.36), (0, 0.66, 1.36)], [0.034, 0.034], 8, colfn=lambda i, k, p: icol(p), sh=AUTO)
    for s in (-1, 1):
        mb.blob((0, s * 0.69, 1.36), 0.055, sub=1, amp=0.05, colfn=lambda p, u: icol(p))
    mb.cbox((0, 0, 1.36), (0.13, 0.13, 0.1), 0.015, IRON)
    mb.cbox((0, 0, 1.63), (0.1, 0.05, 0.1), 0.015, IRON)
    with mb.xf(T(0, 0, 1.78) @ R(90, 'X')):
        torus(mb, 0.14, 0.026, 16, 6, colfn=lambda p, a, b: icol((p[0], 0, 1.78 + p[1])), sh=SMOOTH)
    chain(mb, [(0.13, 0.0, 1.72), (0.26, 0.05, 1.3), (0.34, 0.09, 0.8), (0.38, 0.12, 0.3), (0.43, 0.15, 0.03),
               (0.6, 0.24, 0.02), (0.82, 0.3, 0.02)], step=0.105, R=0.036, r=0.0095,
          colfn=lambda p: icol(p))
    return mb


def make_bollard():
    mb = MB("SM_KG_Bollard", angle=35)
    blk = C("2A2B30")

    def colfn(r, z, a, i):
        c = mix(blk, RUST, 0.5 * smoothstep(0.12, 0.0, z) + 0.3 * smoothstep(0.3, 0.7, nz((math.cos(a), math.sin(a), z * 4))))
        if z > 0.47:
            c = mix(c, IRON_LT, 0.7)
        return c

    prof = [(0, 0), (0.24, 0), (0.24, 0.04), (0.2, 0.06), (0.15, 0.09), (0.14, 0.2), (0.14, 0.3), (0.15, 0.36),
            (0.21, 0.4), (0.23, 0.44), (0.22, 0.49), (0.18, 0.52), (0, 0.53)]
    mb.lathe(prof, 16, colfn=colfn, sh=AUTO)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        mb.lathe([(0, 0.04), (0.025, 0.04), (0.025, 0.06), (0, 0.065)], 6, col=IRON_LT, sh=FLAT)
        mb.deform(len(mb.V) - 14, lambda p, a=a: Vector((p.x + 0.2 * math.cos(a), p.y + 0.2 * math.sin(a), p.z)))
    with mb.xf(T(0, 0, 0.25) @ R(6, 'Y')):
        torus(mb, 0.162, 0.022, 16, 5, col=ROPE, sh=AUTO)
    rope(mb, [(0.17, 0.02, 0.25), (0.24, 0.06, 0.18), (0.3, 0.1, 0.07), (0.36, 0.14, 0.02)], 0.022, 6, ROPE)
    rope_coil(mb, (0.48, 0.26, 0.0), 0.16, 2.2, 0.022, ROPE, seed=3.3)
    return mb


def make_piling():
    mb = MB("SM_KG_Piling", angle=40)
    zs = [0.0, 0.15, 0.3, 0.45, 0.6, 0.75, 0.9, 1.05, 1.2, 1.35, 1.6, 1.9, 2.2, 2.5, 2.75, 2.9]

    def rfn(a, i, z):
        return 1.0 + 0.06 * nz((math.cos(a) * 2, math.sin(a) * 2, z * 0.8), 1.0, (4, 4, 4)) - 0.03 * (z / 3.0)

    def colfn(r, z, a, i):
        p = (r * math.cos(a), r * math.sin(a), z)
        g = 0.5 + 0.5 * nz((math.cos(a) * 9.0, math.sin(a) * 9.0, z * 0.6))
        c = mix(GREYWOOD_DK, GREYWOOD_LT, g)
        return grime(p, c, 1.05, 0.2, 5.0)

    prof = [(0, 0.0)] + [(0.17, z) for z in zs] + [(0.13, 2.97), (0, 3.0)]
    s0 = len(mb.V)
    mb.lathe(prof, 12, colfn=colfn, rfn=rfn, sh=AUTO)
    mb.deform(s0, lambda p: Vector((p.x, p.y, p.z + (0.07 * p.x / 0.17 + 0.025 * nz(p, 12.0) if p.z > 2.85 else 0.0))))
    mb.recolor(s0, lambda p, c: mix(c, C("C9B08A"), 0.6) if p.z > 2.95 and math.hypot(p.x, p.y) < 0.14 else c)
    mb.lathe([(0.178, 2.62), (0.185, 2.63), (0.185, 2.7), (0.178, 2.71)], 12, col=IRON, sh=AUTO)
    rng = random.Random(55)
    for k in range(26):
        a = rng.uniform(0, 2 * math.pi)
        z = rng.uniform(0.1, 0.85)
        n = Vector((math.cos(a), math.sin(a), 0))
        base = n * 0.165 + Vector((0, 0, z))
        cyl(mb, base, base + n * rng.uniform(0.02, 0.035), rng.uniform(0.012, 0.02), 5, col=BARN, r1=0.004)
    pts = []
    for i in range(29):
        t = i / 28 * 2.3 * 2 * math.pi
        pts.append((0.19 * math.cos(t), 0.19 * math.sin(t), 2.22 + 0.11 * i / 28))
    pts += [(0.2, 0.05, 2.1), (0.23, 0.08, 1.85)]
    rope(mb, pts, 0.02, 5, ROPE)
    return mb


# ================================================================================================ 7. Driftwood
def drift_col(dark=DRIFT_DK, light=DRIFT, seed=0.0):
    def f(i, k, p):
        g = 0.5 + 0.5 * nz((p[0] * 1.5, p[1] * 14.0, p[2] * 14.0), 1.0, (seed, 2, 1))
        c = mix(dark, light, 0.35 + 0.65 * g)
        return mul(c, 0.92 + 0.12 * nz(p, 7.0, (1, seed, 3)))
    return f


def knobbly(seed, amt=0.12):
    return lambda i, k: 1.0 + amt * noise.noise(Vector((i * 0.7, k * 0.9, seed)))


def make_driftwood_a():
    mb = MB("SM_KG_Driftwood_A", angle=40)
    xs = np.linspace(-1.05, 1.05, 12)

    def rad(x):
        return lerp(0.2, 0.1, (x + 1.05) / 2.1) + 0.05 * math.exp(-((x + 1.05) / 0.2) ** 2)

    pts = [(x, 0.06 * math.sin(x * 2.0), rad(x) * 0.9 + 0.03 * math.sin(x * 1.3 + 0.5)) for x in xs]
    col = drift_col(seed=1.0)
    mb.tube(pts, [rad(x) for x in xs], 9, colfn=col, sh=AUTO, rmod=knobbly(1.0), capcol=C("C2B49A"))
    rng = random.Random(61)
    for x, ang, L in ((-0.3, 60, 0.32), (0.35, -50, 0.28), (0.75, 75, 0.22)):
        a = math.radians(ang)
        base = Vector((x, 0.06 * math.sin(x * 2.0), rad(x) * 0.9))
        d = Vector((0.4, math.cos(a), math.sin(abs(a)) * 0.8)).normalized()
        mb.tube([base, base + d * L * 0.5, base + d * L], [rad(x) * 0.42, rad(x) * 0.3, rad(x) * 0.2], 6, colfn=col,
                sh=AUTO, tip=0.05)
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.3, 0.3)
        base = Vector((-1.02, 0.0, rad(-1.05) * 0.9))
        d = Vector((-0.55, math.cos(a), math.sin(a))).normalized()
        end = base + d * rng.uniform(0.25, 0.4)
        end.z = max(end.z, 0.04)
        mb.tube([base, base.lerp(end, 0.5), end], [0.06, 0.045, 0.028], 6, colfn=col, sh=AUTO, tip=0.04)
    return mb


def make_driftwood_b():
    mb = MB("SM_KG_Driftwood_B", angle=40)
    col = drift_col(C("7A7163"), C("B0A695"), seed=4.0)
    main = bezier((-0.72, 0.0, 0.07), (-0.25, 0.1, 0.05), (0.2, -0.05, 0.12), (0.62, 0.12, 0.3), 10)
    mb.tube(main, [lerp(0.075, 0.03, k / 10) for k in range(11)], 7, colfn=col, sh=AUTO, rmod=knobbly(4.0, 0.15),
            tip=0.06)
    side = bezier(main[4], main[4] + Vector((0.12, -0.12, -0.03)), (0.3, -0.35, 0.05), (0.5, -0.48, 0.035), 7)
    mb.tube(side, [lerp(0.045, 0.02, k / 7) for k in range(8)], 6, colfn=col, sh=AUTO, rmod=knobbly(5.0, 0.12),
            tip=0.04)
    for base, d, L in ((main[7], (0.1, 0.5, 0.5), 0.22), (side[4], (0.3, -0.2, 0.6), 0.15), (main[2], (-0.2, -0.4, 0.5), 0.14)):
        d = Vector(d).normalized()
        mb.tube([base, base + d * L], [0.018, 0.01], 5, colfn=col, sh=AUTO, tip=0.03)
    mb.tube([(-0.72, 0.0, 0.07), (-0.8, -0.02, 0.07)], [0.075, 0.07], 7, colfn=col, capcol=C("C9BBA0"))
    return mb


# ================================================================================================ 8. FishRack
def make_fish_rack():
    mb = MB("SM_KG_FishRack", angle=40)
    rng = random.Random(71)
    pole = C("8C7358")

    def pc(i, k, p):
        return mul(pole, 0.85 + 0.25 * (0.5 + 0.5 * nz((p[0] * 3, p[1] * 3, p[2]), 1.0, (k, 1, 1))))

    ridge_z = 1.8
    for x in (-1.1, 1.1):
        for sy in (-1, 1):
            mb.tube([(x, sy * 0.58, 0.0), (x, -sy * 0.09, ridge_z + 0.18)], [0.048, 0.04], 7, colfn=pc, sh=AUTO)
        with mb.xf(T(x, 0, ridge_z - 0.06) @ R(90, 'Y')):
            torus(mb, 0.07, 0.013, 8, 4, col=ROPE_DK, sh=AUTO)
    mb.tube([(-1.35, 0, ridge_z), (1.35, 0, ridge_z)], [0.045, 0.045], 8, colfn=pc, sh=AUTO)
    rail_z = 1.05
    ry = 0.58 - (0.58 + 0.09) * rail_z / (ridge_z + 0.18)
    for sy in (-1, 1):
        mb.tube([(-1.3, sy * (ry + 0.05), rail_z), (1.3, sy * (ry + 0.05), rail_z)], [0.035, 0.035], 7, colfn=pc, sh=AUTO)
        mb.tube([(-1.2, sy * 0.5, 0.22), (1.2, sy * 0.5, 0.22)], [0.03, 0.03], 6, colfn=pc, sh=AUTO)
    kinds = ["dried", "dried", "cod", "dried"]

    def hang(x, y, z, side, L):
        M = T(x, y, z) @ R(side * rng.uniform(2, 7), 'X') @ R(rng.uniform(-8, 8), 'Z') @ R(-90, 'Y') @ T(-L, 0, 0)
        fish(mb, M, L, rng.choice(kinds), flat=0.24, eyes=False)

    for x in np.linspace(-0.95, 0.95, 7):
        L = rng.uniform(0.42, 0.52)
        for sy in (-1, 1):
            hang(x + rng.uniform(-0.03, 0.03), sy * 0.06, ridge_z + 0.01, sy, L)
    for sy in (-1, 1):
        for x in np.linspace(-0.95, 0.95, 6):
            hang(x + 0.08 + rng.uniform(-0.03, 0.03), sy * (ry + 0.1), rail_z + 0.01, sy, rng.uniform(0.38, 0.46))
    return mb


# ================================================================================================ 9. SeaweedClump
def make_seaweed():
    mb = MB("SM_KG_SeaweedClump", angle=50, material=SWAY_M)
    rng = random.Random(41)
    base_c, mid_c, tip_c = C("3B4A20"), C("6E7A2A"), C("B38A36")
    rock2(mb, (0, 0, 0.05), (0.13, 0.11, 0.07), seed=4, sub=1, cuts=4,
          colfn=lambda q, u: with_a(granite(q, "6F6A62", "8C877E", "4E4A45", seed=1.0), 0.0))
    nb = 9
    for k in range(nb):
        phi = 2 * math.pi * k / nb + rng.uniform(-0.25, 0.25)
        Lb, w = rng.uniform(0.55, 0.95), rng.uniform(0.07, 0.12)
        rise = rng.uniform(0.2, 0.38)
        dr = Vector((math.cos(phi), math.sin(phi), 0))
        tv = Vector((-math.sin(phi), math.cos(phi), 0))
        curve = bezier((0.03, 0.06, 0), (0.12, rise, 0), (0.35 * Lb, rise * 0.9, 0), (0.8 * Lb, 0.015, 0), 11)
        G = []
        for i, q in enumerate(curve):
            s = i / 11
            hw = w * 0.5 * (0.2 if s < 0.1 else max(0.25, math.sin(math.pi * min(0.97, s)) ** 0.6))
            twist = 0.35 * math.sin(s * 3.0 + k)
            side = (tv * math.cos(twist) + Vector((0, 0, 1)) * math.sin(twist))
            c = dr * q.x + Vector((0, 0, q.y))
            row = []
            for j in (-1, 0, 1):
                ruff = Vector((0, 0, 0.02 * math.sin(s * 30 + k * 1.7) * abs(j) * s))
                row.append(c + side * (hw * j) + ruff)
            G.append(row)

        def colf(i, j, p, k=k):
            s = i / 11
            c = mix(base_c, mid_c, smoothstep(0.0, 0.4, s))
            c = mix(c, tip_c, smoothstep(0.45, 1.0, s) * (0.7 + 0.3 * math.sin(k)))
            return with_a(mul(c, 0.9 + 0.2 * (j == 1)), smoothstep(0.03, 0.95, s))

        grid_sheet(mb, G, colf, sh=SMOOTH)
        if k % 2 == 0:
            q = curve[2]
            mb.blob(dr * q.x + Vector((0, 0, q.y + 0.02)) + tv * 0.03, 0.028, sub=1, amp=0.05,
                    col=with_a(C("A08E30"), 0.15))
    for k in range(6):
        phi = rng.uniform(0, 2 * math.pi)
        dr = Vector((math.cos(phi), math.sin(phi), 0))
        tv = Vector((-math.sin(phi), math.cos(phi), 0))
        L = rng.uniform(0.35, 0.55)
        G = []
        for i in range(8):
            s = i / 7
            c = dr * (0.06 + L * s) + Vector((0, 0, 0.03 + 0.2 * math.sin(math.pi * s * 0.8) * (1 - s)))
            G.append([c - tv * 0.012, c + tv * 0.012])
        grid_sheet(mb, G, lambda i, j, p: with_a(mix(C("3E7E30"), C("7CC048"), i / 7), (i / 7) ** 0.8), sh=SMOOTH)
    return mb


# ================================================================================================ 10. ShellCluster
def scallop(mb, M, R, col, col2):
    with mb.xf(M):
        nu, nv = 13, 5
        G = []
        for j in range(nv + 1):
            v = j / nv
            row = []
            for i in range(nu + 1):
                u = -1 + 2 * i / nu
                ang = u * 1.05
                rib = 0.5 + 0.5 * math.cos(u * 6.5 * math.pi)
                r = v * R * (1 + 0.04 * rib)
                z = R * 0.26 * math.sin(math.pi * min(v, 0.999)) ** 0.7 * (1 - 0.3 * abs(u)) + rib * 0.035 * R * v
                row.append(Vector((r * math.sin(ang), r * math.cos(ang) - R * 0.45, z)))
            G.append(row)

        def cf(j, i, p):
            u = -1 + 2 * i / nu
            v = j / nv
            rib = 0.5 + 0.5 * math.cos(u * 6.5 * math.pi)
            c = mix(col2, col, rib)
            return mix(c, WHITE, 0.25 * math.sin(v * 16) ** 2)

        grid_sheet(mb, G, cf, sh=AUTO)
        edge = [G[nv][i] for i in range(nu + 1)]
        for a, b in zip(edge, edge[1:]):
            mb.f([mb.v(b, mul(col, 0.7)), mb.v(a, mul(col, 0.7)), mb.v((a.x, a.y, 0.0), mul(col, 0.5)),
                  mb.v((b.x, b.y, 0.0), mul(col, 0.5))], None, FLAT)
        for s in (-1, 1):
            poly2s(mb, [(0, -R * 0.45, 0.004), (s * R * 0.3, -R * 0.5, 0.004), (s * R * 0.22, -R * 0.3, 0.012)], mul(col, 0.85))


def make_shell_cluster():
    mb = MB("SM_KG_ShellCluster", angle=40)
    rng = random.Random(42)
    scallop(mb, T(-0.12, 0.05, 0) @ R(25, 'Z'), 0.085, C("F29A78"), C("D86A50"))
    scallop(mb, T(0.14, -0.12, 0) @ R(-60, 'Z'), 0.07, C("F4E8D4"), C("D9C3A8"))
    scallop(mb, T(0.24, 0.15, 0) @ R(140, 'Z'), 0.05, C("F2C45A"), C("D89A3A"))
    cream, brown, lip = C("EFD9B4"), C("B06A3A"), C("F29AA6")
    with mb.xf(T(0.02, 0.22, 0.056) @ R(-20, 'Z') @ R(90, 'Y') @ T(0, 0, -0.085)):
        mb.lathe([(0, 0), (0.012, 0.005), (0.02, 0.03), (0.045, 0.06), (0.058, 0.085), (0.05, 0.105), (0.04, 0.12),
                  (0.03, 0.135), (0.02, 0.15), (0.01, 0.165), (0, 0.175)], 12,
                 colfn=lambda r, z, a, i: mix(cream, brown, smoothstep(0.3, 0.9, math.sin(a + z * 60))),
                 rfn=lambda a, i, z: 1.0 + 0.12 * math.sin(a + z * 60), sh=SMOOTH)
        mb.blob((0.0, -0.045, 0.075), (0.028, 0.012, 0.045), sub=1, amp=0.05, col=lip)
    for k in range(3):
        a = rng.uniform(0, 2 * math.pi)
        mb.blob((-0.2 + 0.05 * k, -0.16 + 0.03 * k, 0.012), (0.042, 0.02, 0.016), sub=1, amp=0.05,
                colfn=lambda p, u: C("2A3656") if u.z > -0.2 else C("4A5A7A"), rot=R(math.degrees(a), 'Z').to_3x3())
    starfish(mb, T(-0.02, -0.08, 0.0) @ R(12, 'Z'), 0.085, C("E8612C"), seed=1)
    starfish(mb, T(0.3, -0.02, 0.0) @ R(50, 'Z'), 0.055, C("9A4FB0"), seed=2)
    with mb.xf(T(-0.3, 0.12, 0)):
        mb.lathe([(0, 0), (0.05, 0), (0.052, 0.006), (0.04, 0.011), (0, 0.013)], 14,
                 colfn=lambda r, z, a, i: mix(C("EDE3CF"), C("C9B48E"), 0.6 * (r < 0.035) * (0.5 + 0.5 * math.cos(5 * a))),
                 sh=SMOOTH)
    for k in range(5):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0.15, 0.33)
        s = rng.uniform(0.018, 0.035)
        rock2(mb, (r * math.cos(a), r * math.sin(a), s * 0.5), (s * 1.2, s, s * 0.7), seed=10 + k, sub=1, cuts=3,
              colfn=lambda q, u: granite(q, seed=k, scale=4.0))
    return mb


# ================================================================================================ 11. Oars
def oar(mb, M, paint):
    with mb.xf(M):
        wood = C("C99B60")

        def colfn(i, k, p):
            x = p[0]
            if x > 2.08:
                return paint[0]
            if x > 1.94:
                return paint[1]
            return mul(wood, 0.95 + 0.1 * nz((x * 3, 0, 0)))

        mb.blob((0, 0, 0), (0.03, 0.026, 0.026), sub=1, amp=0.02, col=WOOD_DK)
        mb.tube([(0.0, 0, 0), (0.14, 0, 0)], [0.021, 0.021], 8, col=WOOD_DK, sh=AUTO)
        mb.tube([(0.14, 0, 0), (0.9, 0, 0), (1.62, 0, 0)], [0.026, 0.026, 0.024], 8, colfn=colfn, sh=AUTO)
        mb.tube([(0.55, 0, 0), (0.7, 0, 0)], [0.031, 0.031], 8, col=C("6B4226"), sh=AUTO)
        xs = [1.55, 1.7, 1.85, 2.0, 2.2, 2.3]
        ws = [0.022, 0.045, 0.068, 0.076, 0.076, 0.066]
        hs = [0.022, 0.014, 0.011, 0.01, 0.01, 0.009]
        zo = [0.0, -0.006, -0.012, -0.014, -0.014, -0.014]
        mb.sweep([(x, 0, z) for x, z in zip(xs, zo)], [(-1, -1), (1, -1), (1, 1), (-1, 1)], ws, hs, colfn=colfn, sh=AUTO)


def make_oars():
    mb = MB("SM_KG_Oars", angle=40)
    oar(mb, T(-1.15, -0.05, 0.026), (RED, WHITE))
    G = Vector((-0.75, -0.62, 0.129))
    B = Vector((-0.75 + 2.3 * 0.866, -0.62 + 2.3 * 0.5, 0.026))
    xa = (B - G).normalized()
    ya = Vector((0, 0, 1)).cross(xa).normalized()
    oar(mb, frame(G, xa, ya, xa.cross(ya)), (TEAL, WHITE))
    return mb


# ================================================================================================ 12. FishBasket, FishBarrel
def woven(mb, prof, seg, c1, c2, sh=SMOOTH):
    rings = []
    for (r, z) in prof:
        rings.append([mb.v((r * math.cos(2 * math.pi * j / seg), r * math.sin(2 * math.pi * j / seg), z), c1)
                      for j in range(seg)])
    for i in range(len(prof) - 1):
        for j in range(seg):
            jn = (j + 1) % seg
            mb.f((rings[i][j], rings[i][jn], rings[i + 1][jn], rings[i + 1][j]), c1 if (i + j) % 2 == 0 else c2, sh)


def fish_heap(mb, rng, n, rim_r, z, kinds, standing=0, L=(0.26, 0.36), over=0):
    for k in range(n):
        Lk = rng.uniform(*L)
        a = 2 * math.pi * k / n + rng.uniform(-0.3, 0.3)
        kind = rng.choice(kinds)
        if k < standing:
            r = rim_r * rng.uniform(0.1, 0.55)
            M = (T(r * math.cos(a), r * math.sin(a), z + 0.02) @ R(math.degrees(a) + rng.uniform(-40, 40), 'Z')
                 @ R(-90 + rng.uniform(-25, 25), 'Y') @ T(-0.5 * Lk, 0, 0))
        elif k < standing + over:
            r = rim_r * 0.55
            M = (T(r * math.cos(a), r * math.sin(a), z + 0.05) @ R(math.degrees(a), 'Z') @ R(-12, 'Y')
                 @ R(90, 'X') @ T(-0.25 * Lk, 0, 0))
        else:
            r = rim_r * rng.uniform(0.0, 0.5)
            M = (T(r * math.cos(a), r * math.sin(a), z + 0.03 + 0.02 * (k % 3)) @ R(rng.uniform(0, 360), 'Z')
                 @ R(90 + rng.uniform(-15, 15), 'X') @ R(rng.uniform(-8, 8), 'Y') @ T(-0.5 * Lk, 0, 0))
        fish(mb, M, Lk, kind, flat=0.5, eyes=True)


def make_fish_basket():
    mb = MB("SM_KG_FishBasket", angle=40)
    rng = random.Random(81)
    c1, c2, dk = C("D5AA66"), C("A57A3E"), C("7A5A30")
    mb.lathe([(0, 0.0), (0.2, 0.0)], 24, col=dk, sh=FLAT)
    prof = [(0.2, 0.0), (0.23, 0.02), (0.255, 0.08), (0.27, 0.14), (0.28, 0.2), (0.29, 0.26), (0.297, 0.32), (0.3, 0.36)]
    woven(mb, prof, 24, c1, c2)
    mb.lathe([(0.285, 0.36), (0.275, 0.3), (0.26, 0.22)], 24, col=dk, sh=SMOOTH)
    with mb.xf(T(0, 0, 0.365)):
        torus(mb, 0.298, 0.026, 24, 6, colfn=lambda p, a, b: c2 if int(a * 24 / math.pi) % 2 else c1, sh=SMOOTH)
    for s in (-1, 1):
        pts = [(s * 0.3, -0.09, 0.36), (s * 0.33, -0.07, 0.43), (s * 0.34, 0.0, 0.46), (s * 0.33, 0.07, 0.43),
               (s * 0.3, 0.09, 0.36)]
        mb.tube(pts, [0.017] * 5, 6, col=c2, sh=AUTO)
    dome(mb, (0, 0, 0.24), 0.27, 0.27, 0.07, rings=3, segs=16,
         colfn=lambda p, rn: mix(C("8FA3B0"), C("C9D4DA"), 0.5 + 0.5 * nz(p, 12.0)))
    fish_heap(mb, rng, 9, 0.27, 0.27, ["mackerel", "herring", "snapper", "mackerel"], standing=0, over=3)
    return mb


def make_fish_barrel():
    mb = MB("SM_KG_FishBarrel", angle=40)
    rng = random.Random(82)
    Hb = 0.85

    def r_of(z):
        return 0.27 + 0.045 * math.sin(math.pi * z / Hb)

    seg = 16
    zs = [0.0, 0.1, 0.25, 0.425, 0.6, 0.75, Hb]
    rings = [[mb.v((r_of(z) * math.cos(2 * math.pi * j / seg), r_of(z) * math.sin(2 * math.pi * j / seg), z), WOOD)
              for j in range(seg)] for z in zs]
    stave = [wv(C("9A6A3C"), rng, 0.4) for _ in range(seg)]
    for i in range(len(zs) - 1):
        for j in range(seg):
            jn = (j + 1) % seg
            mb.f((rings[i][j], rings[i][jn], rings[i + 1][jn], rings[i + 1][j]), stave[j], SMOOTH)
    mb.lathe([(0, 0.0), (r_of(0.0), 0.0)], seg, col=WOOD_DK, sh=FLAT)
    rin = r_of(Hb) - 0.028
    mb.lathe([(r_of(Hb), Hb), (rin, Hb), (rin, Hb - 0.1)], seg, col=C("6E4A2A"), sh=AUTO)
    for z0 in (0.06, 0.2, 0.62, 0.76):
        r0, r1 = r_of(z0), r_of(z0 + 0.045)
        mb.lathe([(r0 - 0.004, z0), (r0 + 0.01, z0), (r1 + 0.01, z0 + 0.045), (r1 - 0.004, z0 + 0.045)], seg,
                 col=IRON, sh=AUTO, closed=True)
    dome(mb, (0, 0, Hb - 0.06), rin, rin, 0.07, rings=3, segs=16,
         colfn=lambda p, rn: mix(C("7F97A8"), C("D0DAE0"), 0.5 + 0.5 * nz(p, 14.0)))
    fish_heap(mb, rng, 12, rin, Hb - 0.02, ["mackerel", "herring", "snapper", "mackerel", "cod"], standing=5, over=2)
    return mb


# ================================================================================================ 13. Lifebuoy
def make_lifebuoy():
    mb = MB("SM_KG_Lifebuoy", angle=40)
    Rr, rr = 0.31, 0.085
    with mb.xf(T(0, 0, Rr + rr) @ R(90, 'X')):
        torus(mb, Rr, rr, 24, 8, col=WHITE, fcol=lambda j: RED if (j // 3) % 2 == 0 else WHITE, sh=SMOOTH)
        att = [math.radians(22.5 + 90 * k) for k in range(4)]
        for k in range(4):
            a0, a1 = att[k], att[(k + 1) % 4] + (2 * math.pi if k == 3 else 0.0)
            pts = []
            for i in range(9):
                t = i / 8
                a = lerp(a0, a1, t)
                rad = Rr + rr + 0.006 + 0.018 * math.sin(math.pi * t)
                pts.append((rad * math.cos(a), rad * math.sin(a), 0.0))
            rope(mb, pts, 0.011, 5, C("E8DDBF"))
            a = att[k]
            ca, sa = math.cos(a), math.sin(a)
            with mb.xf(frame(Vector((Rr * ca, Rr * sa, 0)), Vector((ca, sa, 0)), Vector((0, 0, 1)), Vector((-sa, ca, 0)))):
                torus(mb, rr + 0.006, 0.011, 10, 4, col=C("E8DDBF"), sh=AUTO)
    return mb


# ================================================================================================ 14. CargoHoist
def crate(mb, c, size, col, rng, band=None):
    cx, cy, cz = c
    sx, sy, sz = size
    mb.cbox((cx, cy, cz + sz / 2), (sx - 0.02, sy - 0.02, sz - 0.02), 0.01, col)
    dk = mul(col, 0.62)
    e = 0.05
    for a in (-1, 1):
        for b in (-1, 1):
            mb.box((cx, cy + a * (sy / 2 - e / 2), cz + sz / 2 + b * (sz / 2 - e / 2)), (sx, e, e), dk)
            mb.box((cx + a * (sx / 2 - e / 2), cy, cz + sz / 2 + b * (sz / 2 - e / 2)), (e, sy - 2 * e, e), dk)
            mb.box((cx + a * (sx / 2 - e / 2), cy + b * (sy / 2 - e / 2), cz + sz / 2), (e, e, sz - 2 * e), dk)
    if band:
        for s in (-1, 1):
            mb.box((cx, cy + s * (sy / 2 - 0.004), cz + sz / 2), (sx * 0.6, 0.012, 0.09), band)


def make_cargo_hoist():
    mb = MB("SM_KG_CargoHoist", angle=38)
    rng = random.Random(91)
    wood, dk = C("8B5A33"), C("5E3C24")
    for sy in (-1, 1):
        mb.cbox((0.1, sy * 0.6, 0.1), (1.7, 0.22, 0.2), 0.02, wv(dk, rng))
    for sx in (-1, 1):
        mb.cbox((sx * 0.6, 0, 0.3), (0.22, 1.5, 0.2), 0.02, wv(dk, rng))
    for k in range(6):
        mb.cbox((-0.45 + 0.19 * k, 0, 0.415), (0.17, 1.1, 0.03), 0.006, wv(wood, rng))
    mb.cbox((0, 0, 2.7), (0.26, 0.26, 4.6), 0.025, wood)
    mb.cbox((0, 0, 5.03), (0.32, 0.32, 0.06), 0.015, dk)
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        mb.beam((sx * 0.6, sy * 0.6, 0.42), (sx * 0.12, sy * 0.12, 1.7), 0.11, 0.11, wv(dk, rng), bevel=0.012)
    arm_z = 4.25
    mb.beam((-0.55, 0, arm_z), (3.05, 0, arm_z), 0.2, 0.22, wood, bevel=0.02)
    mb.beam((0.13, 0, 3.0), (1.75, 0, arm_z - 0.1), 0.14, 0.14, wv(wood, rng), bevel=0.015)
    cyl(mb, (0.0, 0, 5.02), (2.9, 0, arm_z + 0.11), 0.02, 6, col=IRON)
    for x, z in ((0.0, 3.0), (0.13, arm_z), (1.75, arm_z - 0.1), (2.9, arm_z + 0.11)):
        mb.box((x, 0, z), (0.1, 0.3, 0.1), IRON)
    mb.box((-0.55, 0, arm_z + 0.2), (0.3, 0.3, 0.2), C("6E6A64"))

    def sheave(c):
        with mb.xf(T(*c) @ R(90, 'X')):
            mb.lathe([(0, -0.045), (0.1, -0.045), (0.12, -0.025), (0.1, 0.0), (0.12, 0.025), (0.1, 0.045), (0, 0.045)],
                     12, col=C("7A6048"), sh=AUTO)
        for s in (-1, 1):
            mb.box((c[0], s * 0.06, c[2] + 0.06), (0.16, 0.012, 0.3), IRON)

    sheave((2.85, 0, arm_z - 0.2))
    sheave((0.25, 0, arm_z + 0.25))
    wx, wz = 0.5, 0.95
    for s in (-1, 1):
        mb.cbox((wx, s * 0.3, 0.68), (0.5, 0.07, 0.52), 0.01, dk)
    with mb.xf(T(wx, 0, wz) @ R(90, 'X')):
        mb.lathe([(0, -0.3), (0.13, -0.3), (0.13, -0.24), (0.1, -0.23), (0.1, 0.23), (0.13, 0.24), (0.13, 0.3), (0, 0.3)],
                 12, col=dk, segcol=[dk, dk, dk, ROPE, dk, dk, dk], sh=AUTO)
        mb.lathe([(0.105, -0.2), (0.125, -0.2), (0.125, 0.2), (0.105, 0.2)], 12, col=ROPE, sh=SMOOTH, closed=True)
    cyl(mb, (wx, 0.3, wz), (wx, 0.42, wz), 0.02, 6, col=IRON)
    mb.beam((wx, 0.42, wz), (wx + 0.28, 0.42, wz + 0.1), 0.04, 0.03, IRON)
    cyl(mb, (wx + 0.28, 0.42, wz + 0.1), (wx + 0.28, 0.58, wz + 0.1), 0.022, 6, col=WOOD_LT)
    rope(mb, [(wx, 0, wz + 0.12), (0.34, 0, arm_z + 0.2)], 0.016, 5, ROPE)
    rope(mb, [(0.25, 0, arm_z + 0.37), (2.85, 0, arm_z - 0.08)], 0.016, 5, ROPE)
    hook_z = 2.45
    rope(mb, [(2.97, 0, arm_z - 0.2), (2.97, 0, hook_z + 0.3)], 0.016, 5, ROPE)
    mb.cbox((2.95, 0, hook_z + 0.18), (0.12, 0.1, 0.22), 0.02, C("7A6048"))
    mb.tube([(2.95, 0, hook_z + 0.07), (2.95, 0, hook_z - 0.06), (2.99, 0, hook_z - 0.13), (3.05, 0, hook_z - 0.09),
             (3.05, 0, hook_z - 0.03)], [0.02, 0.02, 0.018, 0.015, 0.012], 6, col=IRON, sh=AUTO)
    cz = 1.05
    crate(mb, (2.95, 0, cz), (0.7, 0.62, 0.55), C("C08A50"), rng, band=TEAL)
    for sx in (-1, 1):
        for sy in (-1, 1):
            rope(mb, [(2.99, 0, hook_z - 0.11), (2.95 + sx * 0.3, sy * 0.26, cz + 0.55)], 0.011, 4, ROPE)
    return mb


# ================================================================================================ 15. SmokeHut
def make_smoke_hut():
    mb = MB("SM_KG_SmokeHut", angle=35, material=GLOW_M)
    rng = random.Random(61)
    HW, base_z, eave, ridge = 1.5, 0.25, 2.15, 3.05
    soot = C("2A2420")
    wall = C("8A6444")
    s_walls = len(mb.V)
    # stone plinth + floor + threshold
    for side in range(4):
        for k in range(8):
            t = -HW + (k + 0.5) * (2 * HW / 8)
            if side == 0 and abs(t) < 0.65:
                continue
            L = 2 * HW / 8 - 0.02
            if side == 0:
                c, s = (t, HW, 0.13), (L, 0.3, 0.27)
            elif side == 1:
                c, s = (t, -HW, 0.13), (L, 0.3, 0.27)
            elif side == 2:
                c, s = (HW, t, 0.13), (0.3, L, 0.27)
            else:
                c, s = (-HW, t, 0.13), (0.3, L, 0.27)
            c = (c[0], c[1], c[2] + rng.uniform(-0.01, 0.015))
            mb.cbox(c, s, 0.03, granite(c, seed=side * 3 + k))
    mb.box((0, 0, 0.02), (2.9, 2.9, 0.04), C("54412F"))
    mb.cbox((0, HW + 0.02, 0.03), (1.2, 0.36, 0.06), 0.015, granite((0, 2, 0), seed=9.0))
    # eave walls (x = +-HW): vertical boards
    for sx in (-1, 1):
        for k in range(15):
            y = -HW + 0.1 + k * 0.2
            mb.box((sx * (HW - 0.03), y, (base_z + eave) / 2), (0.05, 0.19, eave - base_z), wv(wall, rng))
    # gable walls (y = +-HW) with slanted tops, door gap at the front (+Y)

    def ztop(x):
        return eave + (ridge - eave) * (1 - abs(x) / HW) - 0.04

    door_hw, door_top = 0.58, 2.0
    for sy in (-1, 1):
        y = sy * (HW - 0.03)
        for k in range(15):
            xa, xb = -HW + k * 0.2 + 0.005, -HW + (k + 1) * 0.2 - 0.005
            zb = base_z
            if sy == 1 and xb > -door_hw and xa < door_hw:
                zb = door_top + 0.06
            c = wv(wall, rng)
            P = [(xa, y - 0.025, zb), (xb, y - 0.025, zb), (xb, y + 0.025, zb), (xa, y + 0.025, zb),
                 (xa, y - 0.025, ztop(xa)), (xb, y - 0.025, ztop(xb)), (xb, y + 0.025, ztop(xb)), (xa, y + 0.025, ztop(xa))]
            hexa(mb, P, c)
    for sx in (-1, 1):
        for sy in (-1, 1):
            mb.cbox((sx * (HW - 0.03), sy * (HW - 0.03), (0.05 + eave + 0.05) / 2), (0.14, 0.14, eave), 0.015, C("5E4230"))
        mb.cbox((sx * (HW - 0.03), 0, eave + 0.02), (0.14, 2 * HW + 0.1, 0.1), 0.015, C("5E4230"))
    for sx in (-1, 1):
        mb.cbox((sx * door_hw, HW - 0.01, (0.04 + door_top) / 2), (0.1, 0.12, door_top), 0.012, C("5E4230"))
    mb.cbox((0, HW - 0.01, door_top + 0.03), (1.36, 0.14, 0.1), 0.012, C("5E4230"))
    mb.cbox((0, 0, ridge - 0.1), (0.14, 2 * HW + 0.6, 0.14), 0.015, C("4A3424"))
    mb.recolor(s_walls, lambda p, c: with_a(mix(c, soot, smoothstep(1.4, 3.1, p.z) * 0.7), 0.0))
    # roof: shingle rows, ridge along Y, eaves over +-X
    s_roof = len(mb.V)
    ov, rows = 1.82, 7
    rz = lambda t: ridge + 0.02 - (ridge - eave) * (t * ov / HW)
    for sx in (-1, 1):
        nrm = Vector((sx * (ridge - eave) / HW, 0, 1)).normalized()
        for r in range(rows):
            t0, t1 = r / rows, (r + 1) / rows + 0.05
            for q in range(4):
                y0 = -1.82 + q * 0.91
                yc = y0 + 0.455 + rng.uniform(-0.01, 0.01)
                lift = nrm * (0.012 * (r % 2) + rng.uniform(0, 0.008))
                p0 = Vector((sx * ov * t0, yc, rz(t0))) + lift
                p1 = Vector((sx * ov * t1, yc, rz(t1))) + lift
                c = mix(C("5B4A3F"), C("7A6452") if (r + q) % 2 else C("4A3C33"), rng.uniform(0.2, 0.8))
                mb.beam(p0, p1, 0.9, 0.05, c, up=nrm)
    mb.beam((0, -1.86, ridge + 0.06), (0, 1.86, ridge + 0.06), 0.3, 0.07, C("3A2E26"), bevel=0.012)
    # smoke louvre on the ridge
    for sy in (-1, 1):
        for sx in (-1, 1):
            mb.box((sx * 0.3, sy * 0.85, ridge + 0.2), (0.07, 0.07, 0.3), C("3A2E26"))
    for sx in (-1, 1):
        nrm = Vector((sx * 0.25, 0, 0.5)).normalized()
        mb.beam((0, 0, ridge + 0.5), (sx * 0.5, 0, ridge + 0.3), 2.0, 0.045, C("4A3C33"), up=nrm)
    mb.recolor(s_roof, lambda p, c: with_a(mix(c, soot, smoothstep(0.7, 0.0, abs(p.x)) * 0.6 +
                                                 0.25 * smoothstep(0.3, 0.7, nz(p, 2.0))), 0.0))
    # open plank door hinged at the left post, swung outward
    with mb.xf(T(-door_hw + 0.05, HW + 0.02, 0.06) @ R(100, 'Z')):
        for k in range(5):
            mb.cbox((0.105 + 0.21 * k, 0.0, 0.94), (0.2, 0.045, 1.86), 0.008, wv(C("7A5436"), rng))
        for z in (0.35, 1.55):
            mb.box((0.53, -0.04, z), (1.0, 0.035, 0.12), C("5E4230"))
        mb.beam((0.1, -0.04, 0.4), (0.95, -0.04, 1.5), 0.1, 0.035, C("5E4230"), up=(0, -1, 0))
        for z in (0.35, 1.55):
            mb.box((0.2, 0.03, z), (0.4, 0.012, 0.06), IRON)
    # fish sign on the front gable
    sc = Vector((0, HW + 0.05, 2.5))
    plate(mb, sc, (1, 0, 0), (0, 0, 1), [(-0.32, 0), (-0.22, -0.13), (0.05, -0.16), (0.26, -0.08), (0.3, 0.0), (0.26, 0.08),
                                          (0.05, 0.16), (-0.22, 0.13)], 0.04, C("E8792E"))
    plate(mb, sc + Vector((-0.42, 0, 0)), (1, 0, 0), (0, 0, 1), [(-0.12, -0.15), (0.1, 0.0), (-0.12, 0.15)], 0.04, C("E8792E"))
    mb.box((0.19, HW + 0.075, 2.53), (0.06, 0.012, 0.06), WHITE)
    mb.box((0.2, HW + 0.08, 2.53), (0.03, 0.012, 0.03), EYE)
    mb.box((-0.02, HW + 0.074, 2.5), (0.03, 0.012, 0.22), C("B85A1E"))
    # interior racks with smoked fish (along the side walls and up high)
    for x, z, n in ((-1.05, 2.1, 5), (1.05, 2.1, 5), (-0.45, 2.62, 4), (0.45, 2.62, 4)):
        cyl(mb, (x, -1.42, z), (x, 1.42, z), 0.035, 6, col=C("3A2A1E"))
        for y in np.linspace(-1.0, 1.0, n):
            L = rng.uniform(0.38, 0.46)
            M = T(x, y + rng.uniform(-0.04, 0.04), z - 0.02) @ R(90 + rng.uniform(-15, 15), 'Z') @ R(-90, 'Y') @ T(-L, 0, 0)
            fish(mb, M, L, "smoked", flat=0.4, eyes=False)
    # fire pit (glowing embers)
    fx, fy = 0.0, -0.6
    for k in range(9):
        a = 2 * math.pi * k / 9
        rock2(mb, (fx + 0.38 * math.cos(a), fy + 0.38 * math.sin(a), 0.1), (0.1, 0.09, 0.08), seed=30 + k, sub=1, cuts=4,
              colfn=lambda q, u: with_a(mul(granite(q, seed=3.0), 0.7), 0.0))
    dome(mb, (fx, fy, 0.03), 0.3, 0.3, 0.07, rings=3, segs=14,
         colfn=lambda p, rn: mix(C("FFC24A", a=1.0), C("C0300E", a=1.0), smoothstep(0.1, 0.8, rn + 0.3 * nz(p, 9.0))))
    for k in range(3):
        a = k * 2.1 + 0.3
        d = Vector((math.cos(a), math.sin(a), 0))
        p0 = Vector((fx, fy, 0.12)) - d * 0.05
        p1 = p0 + d * 0.42 + Vector((0, 0, -0.04))
        mb.tube([p0, p1], [0.05, 0.045], 6, colfn=lambda i, kk, p: C("FF7A1E", a=0.9) if i == 0 else C("2A221E"),
                sh=AUTO, capcol=C("FF8A2A", a=1.0))
    # firewood stack against the -X wall, outside
    firewood(mb, (-(HW + 0.24), -0.55, 0.0), rng, length=0.9, rows=(3, 2, 1), r=0.075, axis='Y')
    return mb


def firewood(mb, c, rng, length=0.9, rows=(4, 3, 2), r=0.075, axis='Y'):
    cx, cy, cz = c
    for row, cnt in enumerate(rows):
        for k in range(cnt):
            off = (k - (cnt - 1) / 2) * (2 * r + 0.01)
            z = cz + r + row * (2 * r - 0.02)
            if axis == 'Y':
                p0, p1 = (cx + off, cy - length / 2, z), (cx + off, cy + length / 2, z)
            else:
                p0, p1 = (cx - length / 2, cy + off, z), (cx + length / 2, cy + off, z)
            mb.tube([p0, p1], [r * rng.uniform(0.85, 1.05)] * 2, 7, col=wv(C("5E4330"), rng), sh=AUTO,
                    capcol=wv(C("D2A874"), rng, 0.2))


# ================================================================================================ 16. Shipwreck
def make_shipwreck():
    mb = MB("SM_KG_Shipwreck", angle=32)
    mb.center_xy = True
    rng = random.Random(77)
    HL, B2, NS = 7.0, 2.3, 11

    def hb(x):
        u = x / HL
        if u >= 0:
            return B2 * max(0.0, 1 - u ** 2.4) ** 0.6
        return B2 * (1 - 0.4 * (-u) ** 2.5)

    def zk(x):
        u = x / HL
        return 1.7 * ((u - 0.55) / 0.45) ** 2.0 if u > 0.55 else 0.0

    def zg(x):
        u = x / HL
        return 3.0 + 0.5 * u * u + (0.3 * u ** 3 if u > 0 else 0.0)

    e = 2.0 / 2.6

    def P(x, t, side):
        phi = clamp(t) * math.pi / 2
        yb = hb(x) * max(0.0, math.sin(phi)) ** e
        z = zk(x) + (zg(x) - zk(x)) * (1 - max(0.0, math.cos(phi)) ** e)
        return Vector((x, side * yb, z))

    def Nrm(x, t, side):
        d = 0.01
        a = P(x + d, t, side) - P(x - d, t, side)
        b = P(x, min(1.0, t + d), side) - P(x, max(0.0, t - d), side)
        n = a.cross(b)
        if n.length < 1e-9:
            n = Vector((0, side, 0))
        n.normalize()
        ref = P(x, t, side) - Vector((x, 0, (zk(x) + zg(x)) * 0.5))
        return n if n.dot(ref) >= 0 else -n

    plank = C("5E4634")
    inner = C("3A2C22")
    red_af = C("7A3B2B")
    stripe_top, stripe2 = C("3E8F8C"), C("CDBB8E")

    def hull_col(k, x, t, p):
        if k <= 3:
            c = red_af
        elif k == NS - 1:
            c = stripe_top
        elif k == NS - 2:
            c = stripe2
        else:
            c = plank
        c = mul(c, 0.82 + 0.3 * (0.5 + 0.5 * nz((x * 0.35, k * 1.7, 0.3))))
        c = mix(c, C("8A7A66"), 0.3 * smoothstep(0.1, 0.6, nz((x * 0.8, t * 5, k))))
        if t < 0.45:
            g = smoothstep(0.45, 0.2, t)
            c = mix(c, mix(ALGAE_DK, ALGAE, 0.5 + 0.5 * nz((x * 2, t * 7, 1.0))), 0.55 * g)
            c = mix(c, BARN, 0.8 * g * smoothstep(0.25, 0.5, nz((x * 6, t * 18, 2.0))))
        return with_a(c, 0.0)

    xs = list(np.linspace(-6.0, 6.95, 34))
    stern_end = [rng.uniform(-6.0, -4.8) for _ in range(NS)]

    def present(k, x, side):
        if x < stern_end[k] + (0.0 if side < 0 else 0.4):
            return False
        if side > 0 and k >= 3:
            hw = 2.2 + 0.25 * k + 1.0 * nz((k * 0.61, 0.3, 0.7))
            hx = -1.4 + 0.6 * nz((k * 0.37, 2.0, 0.1))
            if abs(x - hx) < hw:
                return False
        if side < 0 and 6 <= k <= 8 and -3.4 < x < -1.9:
            return False
        return True

    th = 0.07
    for side in (-1, 1):
        for k in range(NS):
            t0, t1 = k / NS + 0.004, (k + 1) / NS - 0.004
            runs, cur = [], []
            for x in xs:
                if present(k, x, side):
                    cur.append(x)
                elif cur:
                    runs.append(cur)
                    cur = []
            if cur:
                runs.append(cur)
            for run in runs:
                if len(run) < 2:
                    continue
                O0, O1, I0, I1 = [], [], [], []
                for x in run:
                    n0, n1 = Nrm(x, t0, side), Nrm(x, t1, side)
                    p0 = P(x, t0, side) + n0 * 0.035
                    p1 = P(x, t1, side)
                    O0.append(mb.v(p0, hull_col(k, x, t0, p0)))
                    O1.append(mb.v(p1, hull_col(k, x, t1, p1)))
                    I0.append(mb.v(p0 - n0 * th, inner))
                    I1.append(mb.v(p1 - n1 * th, inner))
                for i in range(len(run) - 1):
                    quads = [(O0[i], O0[i + 1], O1[i + 1], O1[i]), (I0[i], I1[i], I1[i + 1], I0[i + 1]),
                             (O0[i], I0[i], I0[i + 1], O0[i + 1]), (O1[i], O1[i + 1], I1[i + 1], I1[i])]
                    for q in quads:
                        mb.f(q, None, AUTO)
                mb.f((O0[0], O1[0], I1[0], I0[0]), C("A08A6A"), FLAT)
                mb.f((O0[-1], I0[-1], I1[-1], O1[-1]), C("A08A6A"), FLAT)
    # orient strake faces consistently outward (they are closed solids -> finalize recalculates)
    # ribs (frames): bleached, some snapped on the upper (starboard) side
    rib_c = C("BCA98A")
    for xf_ in np.arange(-6.3, 6.0, 0.6):
        tend = 1.0 if rng.random() < 0.6 else rng.uniform(0.45, 0.9)
        ts_p = [1 - i / 9 for i in range(10)]
        ts_s = [i / 9 * tend for i in range(1, 10)]
        pts = [P(xf_, t, -1) - Nrm(xf_, t, -1) * 0.13 for t in ts_p] + [P(xf_, t, 1) - Nrm(xf_, t, 1) * 0.13 for t in ts_s]
        pts = [p for i, p in enumerate(pts) if i == 0 or (p - pts[i - 1]).length > 0.02]
        if hb(xf_) < 0.3:
            continue
        mb.sweep(pts, [(-0.065, -0.06), (0.065, -0.06), (0.065, 0.06), (-0.065, 0.06)],
                 colfn=lambda i, kk, p: with_a(mul(rib_c, 0.8 + 0.3 * (0.5 + 0.5 * nz(p, 2.0))), 0.0), sh=AUTO,
                 capcol=C("E0D2B8"))
    # keel + stem, gunwales, deck
    kp = [(x, 0, zk(x) - 0.14) for x in np.linspace(-6.4, 6.6, 22)] + [(6.95, 0, 1.7), (7.05, 0, 2.6), (7.12, 0, 3.5),
                                                                        (7.15, 0, 4.1)]
    mb.sweep(kp, [(-0.13, -0.14), (0.13, -0.14), (0.13, 0.14), (-0.13, 0.14)], col=C("3E3028"), sh=AUTO)
    for side in (-1, 1):
        run = []
        for x in xs + [7.0]:
            if x <= 6.9 and present(NS - 1, x, side):
                run.append(P(x, 1.0, side) + Nrm(x, 1.0, side) * 0.02 + Vector((0, 0, 0.04)))
            elif len(run) > 1:
                mb.sweep(run, [(-0.06, -0.07), (0.06, -0.07), (0.06, 0.07), (-0.06, 0.07)], col=C("4A3A2E"), sh=AUTO)
                run = []
            else:
                run = []
        if len(run) > 1:
            mb.sweep(run, [(-0.06, -0.07), (0.06, -0.07), (0.06, 0.07), (-0.06, 0.07)], col=C("4A3A2E"), sh=AUTO)
    deck_c = C("8C7A62")
    for x in (3.5, 4.1, 4.7, 5.3, 5.9, -0.6, 1.2):
        w = hb(x) - 0.12
        y1 = w if x > 0 or x < -0.5 else w * 0.5
        mb.beam((x, -w, zg(x) - 0.12), (x, y1, zg(x) - 0.12), 0.16, 0.16, C("4A3A2E"))
    for y in np.arange(-1.5, 1.6, 0.2):
        if rng.random() < 0.3:
            continue
        xa, xb = 3.3, 6.2
        while xb > xa and abs(y) > hb(xb) - 0.14:
            xb -= 0.1
        if xb - xa < 0.4:
            continue
        xb -= rng.uniform(0.0, 0.8)
        mb.beam((xa, y, zg(xa) - 0.02), (xb, y, zg(xb) - 0.02), 0.18, 0.05, wv(deck_c, rng, 0.4))
    mb.tube([(6.7, 0, zg(6.7) + 0.05), (8.2, 0, zg(6.7) + 0.75), (8.9, 0, zg(6.7) + 1.05)], [0.14, 0.11, 0.09], 8,
            col=C("4A3A2E"), sh=AUTO, tip=0.25)
    # lay her on her port side, bow up a touch
    rot = R(-5, 'Y') @ R(68, 'X')
    mb.deform(0, lambda p: rot @ p)
    return mb


# ================================================================================================ 17. ShipwreckMast
def make_shipwreck_mast():
    mb = MB("SM_KG_ShipwreckMast", angle=35, material=SWAY_M)
    rng = random.Random(83)
    tilt = R(17, 'Y') @ R(6, 'X')
    s0 = len(mb.V)
    wood = C("5E4634")

    def mcol(i, k, p):
        return with_a(mul(wood, 0.85 + 0.3 * (0.5 + 0.5 * nz((k * 0.8, p[2] * 0.7, 1.0)))), 0.0)

    zs = [0.0, 0.8, 1.6, 2.4, 3.2, 4.0, 4.8, 5.6, 6.3]
    mb.tube([(0, 0, z) for z in zs], [lerp(0.19, 0.125, z / 6.3) for z in zs], 9, colfn=mcol, sh=AUTO,
            rmod=lambda i, k: 1.0 + 0.04 * noise.noise(Vector((i, k, 0.5))), capcol=C("C9B08A"))
    mb.deform(s0, lambda p: Vector((p.x, p.y, p.z + (0.35 * abs(nz((p.x * 20, p.y * 20, 1.0))) if p.z > 6.25 else 0.0))))
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.3, 0.3)
        b = Vector((0.1 * math.cos(a), 0.1 * math.sin(a), 6.25))
        cyl(mb, b, b + Vector((0.03 * math.cos(a), 0.03 * math.sin(a), rng.uniform(0.3, 0.65))), 0.035, 4,
            col=C("B89E78"), tip=0.0, r1=0.004)
    for z in (2.2, 4.4):
        r = lerp(0.19, 0.125, z / 6.3) + 0.012
        mb.lathe([(r, z - 0.05), (r, z + 0.05)], 12, col=IRON, sh=AUTO)
    mb.deform(s0, lambda p: tilt @ p)
    A = tilt @ Vector((0.22, 0, 5.0))
    d = Vector((0.05, 1.0, -0.3)).normalized()
    Y0, Y1 = A - d * 2.2, A + d * 2.4
    mb.tube([Y0, A, Y1], [0.055, 0.08, 0.05], 8, col=C("4E3A2C"), sh=AUTO, tip=0.12)
    with mb.xf(T(*A) @ R(90, 'X')):
        torus(mb, 0.12, 0.02, 8, 4, col=ROPE_DK, sh=AUTO)
    # torn sail
    NI, NJ = 14, 10
    canvas, stain, band = C("E7DCBF"), C("B7A57C"), C("B5503F")
    G, Ls = [], []
    for j in range(NJ + 1):
        v = j / NJ
        row = []
        for i in range(NI + 1):
            u = i / NI
            top = Y0.lerp(Y1, 0.06 + 0.88 * u) + Vector((0, 0, -0.07))
            L = 2.9 + 0.3 * math.sin(u * 7.3) + 0.45 * nz((u * 3, 0.5, 0.5))
            if 0.5 < u < 0.78:
                L -= 0.9 * math.sin(math.pi * (u - 0.5) / 0.28)
            L *= 1 - 0.07 * (i % 2)
            bill = 0.38 * math.sin(math.pi * v) ** 0.8 * (0.3 + 0.7 * math.sin(math.pi * u)) + 0.05 * math.sin(v * 9 + u * 5)
            row.append(top + Vector((bill, 0.08 * math.sin(v * 4 + u * 3), -v * L)))
        G.append(row)

    def scol(j, i, p):
        u, v = i / NI, j / NJ
        c = mix(canvas, stain, smoothstep(0.1, 0.7, nz((u * 4, v * 4, 3.0))) * 0.7 + 0.25 * v * v)
        if 0.3 < v < 0.43:
            c = mix(c, band, 0.8)
        return with_a(c, v ** 0.9)

    def skip(j, i):
        u, v = (i + 0.5) / NI, (j + 0.5) / NJ
        if i == 8 and v > 0.45:
            return True
        return v > 0.3 and nz((u * 5, v * 5, 3.0)) > 0.38

    grid_sheet(mb, G, scol, skip=skip, sh=SMOOTH)
    top = tilt @ Vector((0, 0, 6.1))
    rope(mb, catenary(top, (-2.3, 1.5, 0.03), 0.5, 12), 0.022, 5, ROPE_DK, alpha=lambda s: 0.3 * math.sin(math.pi * s))
    rope(mb, catenary(top, (-1.7, -2.2, 0.03), 0.45, 12), 0.022, 5, ROPE_DK, alpha=lambda s: 0.3 * math.sin(math.pi * s))
    dang = [Y1 + Vector((0.05 * math.sin(s * 3), 0.1 * s, -2.0 * s)) for s in np.linspace(0, 1, 8)]
    rope(mb, dang, 0.018, 5, ROPE_DK, alpha=lambda s: s)
    g0 = Vector((Y0.x + 0.3, Y0.y - 0.3, 0.02))
    rope(mb, catenary(Y0, g0, 0.1, 8) + [g0 + Vector((0.4, -0.3, 0)), g0 + Vector((0.9, -0.25, 0))], 0.018, 5, ROPE_DK,
         alpha=lambda s: 0.2 * math.sin(math.pi * min(1.0, s * 1.3)))
    return mb


# ================================================================================================ 18. SeaStacks
def stack_color(top, seed):
    def f(p, c):
        p = Vector(p)
        band = math.sin(p.z * 3.1 + 1.6 * nz(p, 0.45, (seed, 0, 0)))
        c = mix(C("7A6A5A"), C("BBA487"), 0.5 + 0.5 * band)
        c = mix(c, C("5F554D"), 0.6 * smoothstep(0.2, 0.7, nz(p, 0.9, (seed, 4, 1))))
        c = mul(c, 0.94 + 0.12 * nz(p, 6.0, (2, seed, 3)))
        g = smoothstep(0.25, 0.55, nz((p.x * 4.0, p.y * 4.0, p.z * 0.25), 1.0, (seed, 9, 9))) * smoothstep(top * 0.45, top * 0.9, p.z)
        c = mix(c, C("EEEAE0"), g * 0.85)
        c = grime(p, c, 1.1, 0.35, seed)
        return with_a(c, 0.0)
    return f


def stack_top(mb, rng, c, rx, ry, seed, nest=True):
    grass, grass_lt = C("6C9A3E"), C("97C257")
    zat = dome(mb, c, rx, ry, 0.42, rings=4, segs=18, rim=lambda th: 1.0 + 0.18 * nz((math.cos(th), math.sin(th), seed)),
               colfn=lambda p, rn: with_a(mix(grass, grass_lt, 0.5 + 0.5 * nz(p, 3.0)), 0.0), sh=AUTO)
    for k in range(14):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0.2, 0.85)
        x, y = c[0] + rx * r * math.cos(a), c[1] + ry * r * math.sin(a)
        z = zat(x, y)[0]
        if z is not None:
            tuft(mb, (x, y, z - 0.02), rng.uniform(0.25, 0.45), mix(grass, C("B8C860"), rng.random() * 0.5), 3, rng)
    if nest:
        x, y = c[0] + rx * 0.3, c[1] - ry * 0.2
        z = zat(x, y)[0] or c[2]
        with mb.xf(T(x, y, z + 0.03)):
            torus(mb, 0.18, 0.07, 12, 5, colfn=lambda p, a, b: mix(C("7A5A34"), C("B08A58"), 0.5 + 0.5 * nz(Vector(p) * 30)),
                  sh=AUTO)
            for k in range(2):
                mb.blob((0.04 * (k * 2 - 1), 0.02, 0.05), (0.045, 0.035, 0.035), sub=1, amp=0.02, col=C("D8E6EA"))


def pillar(mb, c, prof, seed, lean=(0.0, 0.0), seg=20, jitter=0.12):
    """Eroded rock column: prof [(z, r)] (bottom open, flat-ish top), strata ledges, faceted noise, leaning by
    lean * (z/H)^2. Returns (start index, top centre)."""
    s0 = len(mb.V)
    H = prof[-1][0]
    rings = []
    for z, r in prof:
        ring = []
        ledge = 1.0 + 0.07 * smoothstep(0.55, 0.9, math.sin(z * 2.2 + seed))
        for j in range(seg):
            a = 2 * math.pi * j / seg
            k = 1.0 + 0.2 * nz((math.cos(a) * 1.3, math.sin(a) * 1.3, z * 0.35), 1.0, (seed, 2, 0)) \
                + 0.07 * nz((math.cos(a) * 4, math.sin(a) * 4, z * 1.1), 1.0, (seed, 5, 5))
            rr = r * k * ledge
            t = (z / H) ** 2
            ring.append(mb.v((c[0] + rr * math.cos(a) + lean[0] * t, c[1] + rr * math.sin(a) + lean[1] * t, c[2] + z),
                             C("888888")))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for j in range(seg):
            jn = (j + 1) % seg
            mb.f((rings[i][j], rings[i][jn], rings[i + 1][jn], rings[i + 1][j]), None, AUTO)
    top = Vector((c[0] + lean[0], c[1] + lean[1], c[2] + H + 0.08))
    tid = mb.v(top, C("888888"))
    for j in range(seg):
        mb.f((rings[-1][j], rings[-1][(j + 1) % seg], tid), None, AUTO)
    rng = random.Random(seed)
    mb.deform(s0, lambda p: p + Vector((nz(p, 1.7, (seed, 1, 1)), nz(p, 1.7, (1, seed, 1)), 0.4 * nz(p, 1.7, (1, 1, seed)))) * jitter
              if p.z > c[2] + 0.05 else p)
    return s0, top


def make_sea_stack_a():
    mb = MB("SM_KG_SeaStack_A", angle=28)
    rng = random.Random(101)
    s0 = len(mb.V)
    prof = [(0.0, 2.7), (0.5, 2.6), (1.0, 2.4), (1.5, 2.25), (2.0, 2.1), (2.6, 2.0), (3.2, 1.85), (3.8, 1.8), (4.4, 1.7),
            (5.0, 1.62), (5.6, 1.55), (6.2, 1.5), (6.8, 1.42), (7.4, 1.38), (8.0, 1.35), (8.6, 1.4), (9.2, 1.55),
            (9.7, 1.8), (10.1, 1.95), (10.5, 1.85), (10.8, 1.6)]
    _, top = pillar(mb, (0, 0, 0), prof, 3.0, lean=(0.5, -0.2), seg=20, jitter=0.18)
    rock2(mb, (2.5, -1.3, 0.55), (1.05, 0.9, 0.7), seed=41, sub=2, cuts=6)
    rock2(mb, (-2.2, 1.5, 0.45), (0.9, 0.75, 0.55), seed=43, sub=2, cuts=6)
    rock2(mb, (-1.1, -2.4, 0.3), (0.6, 0.55, 0.38), seed=47, sub=2, cuts=5)
    rock2(mb, (1.2, 2.3, 0.25), (0.5, 0.45, 0.3), seed=49, sub=1, cuts=4)
    zt = max(v.z for v in mb.V[s0:])
    mb.recolor(s0, stack_color(zt, 1.0))
    mb.cavity(s0, dark=0.5, light=0.25)
    stack_top(mb, rng, (top.x, top.y, top.z - 0.3), 1.5, 1.35, 1.0)
    return mb


def make_sea_stack_b():
    mb = MB("SM_KG_SeaStack_B", angle=28)
    rng = random.Random(102)
    s0 = len(mb.V)
    prof = [(0.0, 2.2), (0.5, 2.05), (1.0, 1.85), (1.6, 1.6), (2.2, 1.45), (2.8, 1.3), (3.4, 1.22), (4.0, 1.15),
            (4.6, 1.12), (5.1, 1.2), (5.5, 1.3), (5.8, 1.15), (6.0, 0.9)]
    _, top = pillar(mb, (-0.3, 0.0, 0), prof, 7.0, lean=(-0.9, 0.25), seg=18, jitter=0.15)
    prof2 = [(0.0, 1.3), (0.6, 1.15), (1.2, 1.0), (1.8, 0.9), (2.4, 0.82), (3.0, 0.72), (3.6, 0.62), (4.0, 0.5),
             (4.25, 0.3)]
    pillar(mb, (1.3, -0.25, 0), prof2, 11.0, lean=(0.45, -0.1), seg=14, jitter=0.12)
    rock2(mb, (1.9, 1.4, 0.4), (0.85, 0.75, 0.5), seed=61, sub=2, cuts=6)
    rock2(mb, (-2.2, -0.9, 0.3), (0.65, 0.6, 0.38), seed=63, sub=2, cuts=5)
    zt = max(v.z for v in mb.V[s0:])
    mb.recolor(s0, stack_color(zt, 2.0))
    mb.cavity(s0, dark=0.5, light=0.25)
    stack_top(mb, rng, (top.x, top.y, top.z - 0.28), 1.0, 0.9, 2.0, nest=False)
    return mb


# ================================================================================================ 19. TidePool
def anemone(mb, c, r, col, rng):
    c = Vector(c)
    with mb.xf(T(*c)):
        mb.lathe([(0, 0), (r, 0), (r * 0.9, r * 0.9), (r * 0.7, r * 1.0), (0, r * 1.0)], 8, col=mul(col, 0.6), sh=AUTO)
        for k in range(9):
            a = 2 * math.pi * k / 9 + rng.uniform(-0.2, 0.2)
            d = Vector((math.cos(a) * 0.6, math.sin(a) * 0.6, 1.0)).normalized()
            b = Vector((math.cos(a) * r * 0.6, math.sin(a) * r * 0.6, r * 0.95))
            cyl(mb, b, b + d * r * rng.uniform(0.9, 1.4), r * 0.16, 4, col=col, r1=r * 0.05)


def crab(mb, M):
    shell, dk = C("E0502A"), C("A8341A")
    with mb.xf(M):
        mb.blob((0, 0, 0.035), (0.07, 0.055, 0.028), sub=1, amp=0.08, col=shell)
        for s in (-1, 1):
            for k in range(3):
                x = -0.03 + 0.03 * k
                mb.tube([(x, s * 0.045, 0.03), (x - 0.01, s * 0.09, 0.055), (x - 0.02, s * 0.12, 0.0)],
                        [0.008, 0.007, 0.004], 4, col=dk, sh=AUTO)
            mb.tube([(0.05, s * 0.035, 0.035), (0.08, s * 0.06, 0.045)], [0.009, 0.009], 4, col=dk, sh=AUTO)
            mb.blob((0.1, s * 0.065, 0.045), (0.03, 0.018, 0.018), sub=1, amp=0.05, col=shell)
            cyl(mb, (0.06, s * 0.018, 0.05), (0.07, s * 0.02, 0.075), 0.004, 4, col=dk)
            mb.blob((0.07, s * 0.02, 0.078), 0.007, sub=0, amp=0.0, col=EYE)


def make_tide_pool():
    mb = MB("SM_KG_TidePool", angle=30)
    rng = random.Random(111)
    rx, ry = 1.15, 0.82

    def outline(th):
        return 1.0 + 0.12 * nz((math.cos(th) * 1.4, math.sin(th) * 1.4, 7.0))

    wz = 0.07
    dome(mb, (0, 0, wz), rx, ry, 0.0, rings=4, segs=28, rim=outline,
         colfn=lambda p, rn: with_a(mix(C("1E6F8E"), C("58C9C4"), smoothstep(0.1, 1.0, rn) + 0.15 * nz(Vector(p) * 5)), 0.0))
    # wet gravel lip sloping down to the ground
    seg = 28
    inner, outer = [], []
    for m in range(seg):
        th = 2 * math.pi * m / seg
        s = outline(th)
        inner.append(mb.v((rx * s * math.cos(th), ry * s * math.sin(th), wz), C("8A7A62")))
        outer.append(mb.v((rx * s * 1.4 * math.cos(th), ry * s * 1.45 * math.sin(th), -0.02), C("B89C70")))
    for m in range(seg):
        mn = (m + 1) % seg
        mb.f((inner[m], outer[m], outer[mn], inner[mn]), None, SMOOTH)
    s0 = len(mb.V)
    for k in range(16):
        th = 2 * math.pi * k / 16 + rng.uniform(-0.12, 0.12)
        s = outline(th)
        big = 1.0 if (k % 5 in (0, 1)) else 0.55
        h = rng.uniform(0.18, 0.4) * big + 0.08
        hx = rng.uniform(0.22, 0.34) * (0.6 + 0.6 * big)
        c = (rx * s * 1.08 * math.cos(th), ry * s * 1.1 * math.sin(th), h * 0.45)
        rock2(mb, c, (hx, hx * 0.85, h), seed=200 + k, sub=2 if big > 0.9 else 1, cuts=6, amp=0.14)
    mb.recolor(s0, lambda p, c: with_a(grime(p, mix(C("6E655C"), C("988C7E"), 0.5 + 0.5 * nz(p, 2.5)), 0.2, 0.1, 3.0, 0.7), 0.0))
    mb.recolor(s0, lambda p, c: mix(c, C("D98A9A"), 0.6 * smoothstep(0.35, 0.6, nz(Vector(p) * 5, 1.0, (4, 4, 4)))))
    mb.cavity(s0, dark=0.5, light=0.25)
    starfish(mb, T(0.55, -0.58, 0.22) @ R(-25, 'X') @ R(30, 'Z'), 0.09, C("E8612C"), seed=5)
    starfish(mb, T(-0.35, 0.1, wz - 0.005), 0.08, C("9A4FB0"), seed=6)
    anemone(mb, (-0.8, -0.35, wz - 0.01), 0.045, C("F06A9A"), rng)
    anemone(mb, (0.75, 0.3, wz - 0.01), 0.04, C("7AD07A"), rng)
    anemone(mb, (0.2, 0.62, wz - 0.01), 0.035, C("F0A040"), rng)
    for k in range(4):
        a = rng.uniform(0, 2 * math.pi)
        p = (0.7 * rx * math.cos(a), 0.7 * ry * math.sin(a), wz - 0.02)
        tuft(mb, p, rng.uniform(0.12, 0.2), C("4E9A3C"), 3, rng)
    crab(mb, T(1.35, 0.35, 0.0) @ R(160, 'Z'))
    for k in range(5):
        mb.blob((-1.18 + 0.05 * k, -0.52 + 0.03 * (k % 2), 0.13 + 0.02 * k), (0.04, 0.02, 0.015), sub=1, amp=0.05,
                col=C("2A3656"), rot=R(40 * k, 'Z').to_3x3())
    return mb


# ================================================================================================ 20. JettySection
def make_jetty():
    mb = MB("SM_KG_JettySection", angle=38)
    rng = random.Random(121)
    DZ, L, W = 2.5, 4.0, 2.0
    pl = C("9A7552")
    n = 20
    pitch = L / n
    for i in range(n):
        x = -L / 2 + pitch * (i + 0.5)
        c = mix(pl, C("B48C62") if rng.random() < 0.5 else C("6E5238"), rng.uniform(0.0, 0.7))
        mb.cbox((x + rng.uniform(-0.008, 0.008), rng.uniform(-0.03, 0.03), DZ - 0.03 + rng.uniform(-0.004, 0.004)),
                (pitch - 0.02, W - rng.uniform(0, 0.06), 0.06), 0.01, c)
    dk = C("4E3A2A")
    for sy in (-0.8, 0.0, 0.8):
        mb.box((0, sy, DZ - 0.15), (L, 0.14, 0.18), dk)
    postc = C("7A6A58")

    def pcol(r, z, a, i):
        p = (r * math.cos(a), r * math.sin(a), z)
        return grime(p, mul(postc, 0.85 + 0.25 * (0.5 + 0.5 * nz((math.cos(a) * 5, math.sin(a) * 5, z)))), 1.3, 0.2, 7.0)

    for px in (-1.0, 1.0):
        mb.cbox((px, 0, DZ - 0.32), (0.22, W + 0.3, 0.16), 0.015, dk)
        for sy in (-1, 1):
            tall = (px > 0 and sy < 0) or (px < 0 and sy > 0)
            ztop = DZ + 0.85 if tall else DZ - 0.24
            with mb.xf(T(px, sy * 1.08, 0)):
                mb.lathe([(0, 0), (0.13, 0), (0.13, 0.5), (0.13, 1.0), (0.13, 1.5), (0.13, 2.0), (0.13, ztop - 0.04),
                          (0.11, ztop), (0, ztop + 0.01)], 10, colfn=pcol, sh=AUTO)
            if tall:
                pts = []
                for i in range(17):
                    t = i / 16 * 2.2 * 2 * math.pi
                    pts.append((px + 0.145 * math.cos(t), sy * 1.08 + 0.145 * math.sin(t), DZ + 0.45 + 0.1 * i / 16))
                rope(mb, pts, 0.02, 5, ROPE)
        mb.beam((px, -1.0, 0.7), (px, 1.0, DZ - 0.42), 0.1, 0.08, dk, up=(1, 0, 0))
        mb.beam((px, 1.0, 0.7), (px, -1.0, DZ - 0.42), 0.1, 0.08, dk, up=(1, 0, 0))
    return mb


# ================================================================================================ 21. SandCastle
def make_sand_castle():
    mb = MB("SM_KG_SandCastle", angle=40)
    rng = random.Random(131)

    def scol(p, wet=0.0):
        return mix(mix(SAND, C("F0DCA8"), 0.5 + 0.5 * nz(Vector(p) * 12)), SAND_WET, wet)

    dome(mb, (0, 0, -0.01), 0.58, 0.52, 0.09, rings=5, segs=24, colfn=lambda p, rn: scol(p, smoothstep(0.7, 1.0, rn) * 0.8))
    s0 = len(mb.V)

    def tower(c, r0, r1, h, merlons):
        with mb.xf(T(*c)):
            mb.lathe([(0, 0), (r0, 0), (r0 * 0.985, h * 0.33), (r0 * 1.01, h * 0.36), (r1 * 1.02, h * 0.66),
                      (r1, h * 0.69), (r1, h), (0, h)], 14, colfn=lambda r, z, a, i: scol((r * math.cos(a), r * math.sin(a), z)),
                     sh=AUTO)
            for k in range(merlons):
                a = 2 * math.pi * k / merlons
                mb.box((r1 * 0.82 * math.cos(a), r1 * 0.82 * math.sin(a), h + 0.022), (0.04, 0.04, 0.045),
                       scol((a, k, 1)))

    tower((0.02, 0.03, 0.06), 0.17, 0.14, 0.26, 7)
    corners = [(0.3, 0.26), (-0.28, 0.27), (-0.29, -0.25), (0.3, -0.26)]
    for c in corners:
        tower((c[0], c[1], 0.05), 0.09, 0.075, 0.18, 4)
    for k in range(4):
        a, b = Vector((*corners[k], 0)), Vector((*corners[(k + 1) % 4], 0))
        d = (b - a)
        mid = (a + b) / 2
        with mb.xf(frame(Vector((mid.x, mid.y, 0.05)), d.normalized(), Vector((0, 0, 1)).cross(d.normalized()), Vector((0, 0, 1)))):
            Ld = d.length - 0.12
            mb.box((0, 0, 0.055), (Ld, 0.05, 0.11), scol(mid))
            for q in range(4):
                mb.box((-Ld / 2 + (q + 0.5) * Ld / 4, 0, 0.125), (0.035, 0.05, 0.03), scol(mid))
            if k == 3:
                mb.box((0, 0.026, 0.045), (0.07, 0.01, 0.08), C("8A6E45"))
    mb.recolor(s0, lambda p, c: mul(c, 0.95 + 0.1 * nz(Vector(p) * 20)))
    cyl(mb, (0.02, 0.03, 0.34), (0.02, 0.03, 0.56), 0.005, 4, col=C("E8D8B0"))
    poly2s(mb, [(0.02, 0.03, 0.55), (0.02, 0.03, 0.47), (0.14, 0.035, 0.51)], C("E0392B"))
    for p, cc in (((0.16, 0.0, 0.2), C("F29AA6")), ((-0.3, 0.15, 0.15), WHITE), ((0.3, -0.12, 0.13), C("F2C45A"))):
        mb.blob(p, (0.015, 0.012, 0.012), sub=1, amp=0.05, col=cc)
    with mb.xf(T(0.5, -0.32, 0.075) @ R(30, 'Z') @ R(90, 'Y')):
        mb.lathe([(0, -0.07), (0.065, -0.07), (0.08, 0.07), (0.072, 0.07), (0.058, -0.06), (0, -0.06)], 12, col=RED,
                 sh=AUTO)
    cyl(mb, (-0.42, -0.4, 0.012), (-0.2, -0.46, 0.012), 0.01, 5, col=YELLOW)
    plate(mb, Vector((-0.14, -0.475, 0.008)), (1, -0.27, 0), (0.27, 1, 0), [(-0.05, -0.035), (0.04, -0.03), (0.06, 0.0),
                                                                            (0.04, 0.03), (-0.05, 0.035)], 0.006, YELLOW)
    return mb


# ================================================================================================ build + export
PROPS = [
    # name, builder, material, collision, preview view
    ("Shipwreck", make_shipwreck, "VC", "complex", dict(el=22, az=-40)),
    ("ShipwreckMast", make_shipwreck_mast, "Sway", "complex", dict(el=10, az=-60)),
    ("SmokeHut", make_smoke_hut, "Glow", "complex", dict(el=14, az=150)),
    ("CargoHoist", make_cargo_hoist, "VC", "complex", dict(el=12, az=160)),
    ("SeaStack_A", make_sea_stack_a, "VC", "complex", dict(el=8, az=150)),
    ("SeaStack_B", make_sea_stack_b, "VC", "complex", dict(el=8, az=150)),
    ("BellBuoy", make_bell_buoy, "Glow", "box", dict(el=12, az=150)),
    ("NetRack", make_net_rack, "Sway", "box", dict(el=10, az=160)),
    ("JettySection", make_jetty, "VC", "complex", dict(el=18, az=150)),
    ("TidePool", make_tide_pool, "VC", "complex", dict(el=40, az=150)),
    ("NetPile", make_net_pile, "VC", "box", dict(el=30, az=150)),
    ("LobsterTrap", make_lobster_trap, "VC", "box", dict(el=24, az=120)),
    ("LobsterTrapStack", make_lobster_trap_stack, "VC", "box", dict(el=18, az=150)),
    ("Buoy_Red", make_buoy_red, "VC", "box", dict(el=12, az=150)),
    ("Buoy_Striped", make_buoy_striped, "VC", "box", dict(el=12, az=150)),
    ("Anchor", make_anchor, "VC", "box", dict(el=10, az=150)),
    ("Bollard", make_bollard, "VC", "box", dict(el=24, az=150)),
    ("Piling", make_piling, "VC", "box", dict(el=10, az=150)),
    ("Driftwood_A", make_driftwood_a, "VC", "box", dict(el=28, az=150)),
    ("Driftwood_B", make_driftwood_b, "VC", "none", dict(el=30, az=150)),
    ("FishRack", make_fish_rack, "VC", "box", dict(el=12, az=130)),
    ("SeaweedClump", make_seaweed, "Sway", "none", dict(el=35, az=150)),
    ("ShellCluster", make_shell_cluster, "VC", "none", dict(el=45, az=150)),
    ("Oars", make_oars, "VC", "none", dict(el=40, az=150)),
    ("FishBasket", make_fish_basket, "VC", "box", dict(el=35, az=150)),
    ("FishBarrel", make_fish_barrel, "VC", "box", dict(el=30, az=150)),
    ("Lifebuoy", make_lifebuoy, "VC", "none", dict(el=8, az=170)),
    ("SandCastle", make_sand_castle, "VC", "none", dict(el=30, az=150)),
]
MAT_NAME = {"VC": VC_M, "Sway": SWAY_M, "Glow": GLOW_M}


def build_all():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs, stats = [], []
    for name, fn, mat, col, view in PROPS:
        if ONLY and name not in ONLY:
            continue
        mb = fn()
        assert mb.name == "SM_KG_" + name, (mb.name, name)
        assert mb.material == MAT_NAME[mat], (name, mb.material, mat)
        mn = Vector((min(v.x for v in mb.V), min(v.y for v in mb.V), min(v.z for v in mb.V)))
        mx = Vector((max(v.x for v in mb.V), max(v.y for v in mb.V), max(v.z for v in mb.V)))
        sh = Vector(((mn.x + mx.x) / 2 if mb.center_xy else 0.0, (mn.y + mx.y) / 2 if mb.center_xy else 0.0, mn.z))
        mb.deform(0, lambda p: p - sh)
        obj, tris, diag = finalize(mb)
        objs.append(obj)
        mn, mx = mn - sh, mx - sh
        stats.append((obj.name, tris, mn, mx, diag))
    print("\nKG_HARBOUR: object                       tris     min (x, y, z)              max (x, y, z)          size")
    for name, tris, mn, mx, diag in stats:
        sz = mx - mn
        print(f"KG_HARBOUR: {name:26s} {tris:6d}  ({mn.x:6.2f},{mn.y:6.2f},{mn.z:6.2f})  ({mx.x:6.2f},{mx.y:6.2f},{mx.z:6.2f})"
              f"  {sz.x:5.2f} x {sz.y:5.2f} x {sz.z:5.2f}  {diag}")
    print(f"KG_HARBOUR: total tris {sum(s[1] for s in stats)}")
    return objs, stats


def export(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                              export_materials="EXPORT")
    print(f"KG_HARBOUR: exported {path} ({os.path.getsize(path) / 1024:.0f} KB)")


def write_manifest(path):
    props = {name: {"material": mat, "collision": col} for name, fn, mat, col, view in PROPS if not ONLY or name in ONLY}
    with open(path, "w") as f:
        json.dump({"props": props}, f, indent=1)
    print(f"KG_HARBOUR: manifest {path} ({len(props)} props)")


# ================================================================================================ preview
def _node_mat(name, glow, two_sided=False):
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
    mat.use_backface_culling = not two_sided
    return mat


def _flat_mat(name, rgb):
    mat = bpy.data.materials.new(name)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat


def render_previews(objs, closeups=None, outdir=None):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 32
        sc.eevee.use_shadows = True
        sc.eevee.use_fast_gi = True
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
        bg.inputs["Strength"].default_value = 0.75
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.8
    sun.data.angle = math.radians(4)
    sun.data.color = (1.0, 0.95, 0.86)
    sc.collection.objects.link(sun)
    pm = {GLOW_M: _node_mat("P_Glow", True), SWAY_M: _node_mat("P_Sway", False, two_sided=True)}
    plain = _node_mat("P_Plain", False)
    for o in objs:
        o.data.materials[0] = pm.get(o.data.materials[0].name, plain)
    gm = bpy.data.meshes.new("PreviewGround")
    gm.from_pydata([(-300, -300, 0), (300, -300, 0), (300, 300, 0), (-300, 300, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PreviewGround", gm)
    sc.collection.objects.link(ground)
    ground.data.materials.append(_flat_mat("P_Ground", (0.46, 0.4, 0.29)))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 50
    cam.data.sensor_width = 36
    cam.data.clip_start = 0.02
    cam.data.clip_end = 800
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
    tmp = tempfile.mkdtemp(prefix="kg_harbour_")
    by_name = {o.name: o for o in objs}
    views = {"SM_KG_" + p[0]: p[4] for p in PROPS}

    def shoot(name, path, az=None, el=None):
        o = by_name[name]
        cfg = views[name]
        for x in objs:
            x.hide_render = x is not o
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
            if full not in by_name:
                continue
            for tag, azo in (("a", None), ("b", views[full]["az"] + 150)):
                p = os.path.join(outdir, f"{full}_{tag}.png")
                shoot(full, p, az=azo)
                print(f"KG_HARBOUR: closeup {p}")
        return

    os.makedirs(PREVIEW_DIR, exist_ok=True)
    names = [o.name for o in objs]
    sheets = [names[i:i + 8] for i in range(0, len(names), 8)]
    for si, group in enumerate(sheets):
        cols, rows = 4, 2
        sheet = np.zeros((rows * TILE, cols * TILE, 4), np.float32)
        sheet[..., 3] = 1.0
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
        path = os.path.join(PREVIEW_DIR, f"DressHarbour_preview_{si + 1}.png")
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        print(f"KG_HARBOUR: preview {path}")
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
        amin = amax = float("nan")
        if ca:
            arr = np.empty(len(ca.data) * 4, np.float32)
            ca.data.foreach_get("color", arr)
            a = arr[3::4]
            amin, amax = float(a.min()), float(a.max())
        good = o.name in expect and ca is not None and len(me.materials) > 0
        ok &= good
        print(f"KG_HARBOUR_VERIFY: {o.name:26s} tris {tris:6d} (built {expect.get(o.name)})  color={ca.name if ca else None}"
              f"  alpha [{amin:.2f}, {amax:.2f}]  mat={me.materials[0].name if me.materials else None}")
    missing = set(expect) - {o.name for o in bpy.data.objects}
    if missing:
        ok = False
        print(f"KG_HARBOUR_VERIFY: MISSING {sorted(missing)}")
    print(f"KG_HARBOUR_VERIFY: {'OK' if ok else 'FAILED'}")


def main():
    objs, stats = build_all()
    export(OUT_GLB)
    if "--manifest" in _opt:
        write_manifest(_opt["--manifest"])
    if "--closeup" in _opt:
        render_previews(objs, closeups=_opt["--closeup"].split(","), outdir=_opt.get("--outdir", PREVIEW_DIR))
    elif "--no-preview" not in _opt:
        render_previews(objs)
    if "--no-verify" not in _opt:
        verify(OUT_GLB, stats)


main()
