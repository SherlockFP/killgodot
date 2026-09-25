"""Procedural water / fishing / digging props (vertex-coloured, no textures) for the fishing + treasure-dig loops.

  blender --background --factory-startup --python Tools/Blender/kg_make_water_props.py -- Art/Packed/KG_WaterProps.glb [preview.png]

One .glb, one object per prop, every object at the world origin with its pivot baked into the mesh (Unreal relies on it):
  SM_KG_Fish_Mackerel / _Cod / _Salmon / _GoldenCarp   head at +X, pivot at body centre. Vertex colour ALPHA =
                                                       0 at the nose .. 1 at the tail tip (drives the tail wiggle).
  SM_KG_Rowboat   +X forward, pivot at hull centre on the waterline (Z=0), keel ~-0.3, rim ~+0.35.
  SM_KG_Oar       2.2 m along +X, pivot at the handle end (X=0), blade at +X.
  SM_KG_FishingRod 1.9 m along +X, pivot at the butt (X=0), rod tip exactly at (1.9, 0, 0), reel/guides hang to -Z.
  SM_KG_Bobber    pivot at the waterline point (float centre), antenna up +Z.
  SM_KG_Shovel    1.2 m shaft along +X, pivot at the grip end (X=0), blade at +X.
  SM_KG_DigMound / SM_KG_DugHole   1.2 m footprint, pivot at ground centre (Z=0 = ground).
  SM_KG_Doorbell  pivot at the wall mount (wall plane X=0), bracket along +X, bell hanging to -Z.

Colour attribute "Col" (corner domain): RGB is an sRGB colour (the UE materials linearise it, same convention as
M_KG_Terrain), alpha = 1 except on the fish. Style: vibrant toon, warm woods/brass against teal paint (TF2 warm/cool).
The optional preview PNG is a Workbench contact sheet (fish side + 3/4 views, then the props); pass "none" to skip.
Set KG_WATERPROPS_DETAIL=<dir> to also write zoomed inspection sheets there. The script prints per-object bounds,
triangle counts and mesh checks (islands with inward normals, non-manifold edges, degenerate faces).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(ROOT, "Art", "Packed", "KG_WaterProps.glb")
PREVIEW = argv[1] if len(argv) > 1 else os.path.join(ROOT, "Art", "Concept", "WaterProps_preview.png")
if not os.path.isabs(OUT):
    OUT = os.path.join(ROOT, OUT)
if not os.path.isabs(PREVIEW):
    PREVIEW = os.path.join(ROOT, PREVIEW)

bpy.ops.wm.read_factory_settings(use_empty=True)
TAU = 2.0 * math.pi
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


# ----------------------------------------------------------------------------------------------------------------
# small maths helpers
# ----------------------------------------------------------------------------------------------------------------
def clamp(v, a=0.0, b=1.0):
    return max(a, min(b, v))


def sm(a, b, x):
    t = clamp((x - a) / (b - a))
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def mul(c, k):
    return tuple(clamp(c[i] * k) for i in range(3))


def sgnpow(v, e):
    return math.copysign(abs(v) ** e, v)


def h01(*a):
    """Deterministic hash -> [0, 1)."""
    s = 0.0
    for i, v in enumerate(a):
        s += v * (12.9898, 78.233, 37.719, 4.123, 91.7)[i % 5]
    return (math.sin(s) * 43758.5453) % 1.0


def mono(pts):
    """Monotone cubic (Fritsch-Carlson) interpolation through (x, y) control points -> f(x)."""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    n = len(pts)
    d = [(ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]) for i in range(n - 1)]
    m = [d[0]] + [0.0 if d[i - 1] * d[i] <= 0 else (d[i - 1] + d[i]) / 2 for i in range(1, n - 1)] + [d[-1]]
    for i in range(n - 1):
        if d[i] == 0:
            m[i] = m[i + 1] = 0.0
            continue
        a, b = m[i] / d[i], m[i + 1] / d[i]
        s = a * a + b * b
        if s > 9:
            t = 3 / math.sqrt(s)
            m[i], m[i + 1] = t * a * d[i], t * b * d[i]

    def f(x):
        if x <= xs[0]:
            return ys[0]
        if x >= xs[-1]:
            return ys[-1]
        i = 0
        while xs[i + 1] < x:
            i += 1
        hh = xs[i + 1] - xs[i]
        t = (x - xs[i]) / hh
        t2, t3 = t * t, t * t * t
        return ((2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * hh * m[i]
                + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * hh * m[i + 1])
    return f


def perp(axis):
    """Any unit vector perpendicular to axis."""
    a = Vector(axis).normalized()
    ref = Z if abs(a.z) < 0.9 else X
    return a.cross(ref).normalized()


# ----------------------------------------------------------------------------------------------------------------
# mesh builder: verts + faces with per-corner sRGB colours and smooth flags
# ----------------------------------------------------------------------------------------------------------------
class MB:
    def __init__(self):
        self.v, self.f, self.c, self.sm = [], [], [], []

    def vert(self, p):
        self.v.append(Vector(p))
        return len(self.v) - 1

    def verts(self, pts):
        return [self.vert(p) for p in pts]

    def face(self, idx, col, smooth=True):
        # drop repeated indices (collapsed quads become triangles)
        out, cols = [], []
        percorner = not isinstance(col[0], (int, float))
        for k, i in enumerate(idx):
            if out and out[-1] == i:
                continue
            out.append(i)
            cols.append(col[k] if percorner else col)
        while len(out) > 1 and out[0] == out[-1]:
            out.pop()
            cols.pop()
        if len(set(out)) < 3 or len(set(out)) != len(out):
            return
        self.f.append(tuple(out))
        self.c.append([tuple(c[:3]) for c in cols])
        self.sm.append(smooth)

    def loft(self, rings, colf, closed_ring=True, closed_path=False, smooth=True, vcol=None):
        """rings: lists of vertex indices (a 1-element list is a pole). colf(i, j) -> rgb or per-corner list.
        vcol: optional {vertex index: rgb} -> smooth per-vertex colours instead of colf."""
        nr = len(rings)
        for i in range(nr if closed_path else nr - 1):
            a, b = rings[i], rings[(i + 1) % nr]
            n = max(len(a), len(b))
            segs = n if closed_ring else n - 1
            for j in range(segs):
                j1 = (j + 1) % n
                if len(a) == 1:
                    q = [a[0], b[j1], b[j]]
                elif len(b) == 1:
                    q = [a[j], a[j1], b[0]]
                else:
                    q = [a[j], a[j1], b[j1], b[j]]
                self.face(q, [vcol[k] for k in q] if vcol else colf(i, j), smooth)

    def xform(self, fn, start=0):
        for k in range(start, len(self.v)):
            self.v[k] = Vector(fn(self.v[k]))


def ring_pts(center, u, v, ru, rv, n, phase=0.0):
    return [center + u * (ru * math.cos(phase + TAU * j / n)) + v * (rv * math.sin(phase + TAU * j / n))
            for j in range(n)]


def lathe(mb, origin, axis, profile, n, colf, smooth=True, ref=None, phase=0.0):
    """profile: [(t, r) or (t, ry, rz)] along axis; r == 0 -> pole. colf(i, j) for band i."""
    axis = Vector(axis).normalized()
    u = Vector(ref).normalized() if ref is not None else perp(axis)
    v = axis.cross(u).normalized()
    rings = []
    for p in profile:
        t, ry = p[0], p[1]
        rz = p[2] if len(p) > 2 else ry
        c = Vector(origin) + axis * t
        rings.append([mb.vert(c)] if ry <= 0 and rz <= 0 else mb.verts(ring_pts(c, u, v, ry, rz, n, phase)))
    mb.loft(rings, colf, smooth=smooth)
    return rings


def tube(mb, p0, p1, r0, r1, n, col, cap=True, smooth=True):
    """Cone/cylinder p0->p1; r == 0 end becomes a point, otherwise capped flat."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = p1 - p0
    L = ax.length
    prof = []
    if cap and r0 > 0:
        prof.append((0.0, 0.0))
    prof += [(0.0, r0), (L, r1)]
    if cap and r1 > 0:
        prof.append((L, 0.0))
    colf = col if callable(col) else (lambda i, j: col)
    lathe(mb, p0, ax, prof, n, colf, smooth)


def sweep(mb, path, profile, ref, colf, closed=False, cap=True, smooth=True):
    """Sweep a closed 2D profile [(a, b)] along path; frame A = T x ref (mitred), B = A x T."""
    path = [Vector(p) for p in path]
    n = len(path)
    rings = []
    for k in range(n):
        if closed:
            dp, dn = path[k] - path[k - 1], path[(k + 1) % n] - path[k]
        else:
            dp = path[k] - path[k - 1] if k > 0 else path[1] - path[0]
            dn = path[k + 1] - path[k] if k < n - 1 else path[-1] - path[-2]
        dp.normalize()
        dn.normalize()
        T = (dp + dn).normalized()
        A = T.cross(Vector(ref)).normalized()
        B = A.cross(T).normalized()
        mit = 1.0 / max(0.35, T.dot(dn))
        rings.append(mb.verts([path[k] + A * (a * mit) + B * b for a, b in profile]))
    if not closed and cap:
        c0 = mb.vert(sum((mb.v[i] for i in rings[0]), Vector()) / len(rings[0]))
        c1 = mb.vert(sum((mb.v[i] for i in rings[-1]), Vector()) / len(rings[-1]))
        rings = [[c0]] + rings + [[c1]]
        mb.loft(rings, lambda i, j: colf(clamp(i - 1, 0, n - 2), j), closed_path=False, smooth=smooth)
    else:
        mb.loft(rings, colf, closed_path=closed, smooth=smooth)


def torus(mb, center, normal, R, r, nu, nv, col, smooth=True):
    normal = Vector(normal).normalized()
    u = perp(normal)
    v = normal.cross(u)
    rings = []
    for i in range(nu):
        a = TAU * i / nu
        d = u * math.cos(a) + v * math.sin(a)
        c = Vector(center) + d * R
        rings.append(mb.verts(ring_pts(c, d, normal, r, r, nv)))
    mb.loft(rings, (col if callable(col) else (lambda i, j: col)), closed_path=True, smooth=smooth)


def chamfer_box(mb, center, size, ch, col, smooth=False):
    """Box with chamfered long edges (along X) and inset end caps. col(face_kind) or rgb."""
    sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
    prof = [(sy, sz - ch), (sy - ch, sz), (-sy + ch, sz), (-sy, sz - ch), (-sy, -sz + ch), (-sy + ch, -sz),
            (sy - ch, -sz), (sy, -sz + ch)]
    c = Vector(center)
    rings = [[mb.vert(c + Vector((-sx, 0, 0)))]]
    for x, k in ((-sx, 0.7), (-sx + ch, 1.0), (sx - ch, 1.0), (sx, 0.7)):
        rings.append(mb.verts([c + Vector((x, y * k + (0 if k == 1 else 0), z * k)) for y, z in prof]))
    rings.append([mb.vert(c + Vector((sx, 0, 0)))])
    colf = col if callable(col) else (lambda i, j: col)
    mb.loft(rings, colf, smooth=smooth)


def blob(mb, center, radius, squash, seed, colf, subdiv=1, rough=0.28):
    """Lumpy low-poly stone/clod: subdivided icosahedron with noise displacement. Flat shaded."""
    t = (1 + 5 ** 0.5) / 2
    vs = [Vector(p).normalized() for p in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
                                            (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    fs = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
          (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10),
          (8, 6, 7), (9, 8, 1)]
    for _ in range(subdiv):
        cache, nf = {}, []

        def mid(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                vs.append(((vs[a] + vs[b]) / 2).normalized())
                cache[key] = len(vs) - 1
            return cache[key]
        for a, b, c in fs:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        fs = nf
    off = Vector((seed * 3.1, seed * 1.7, seed * 2.3))
    base = len(mb.v)
    for p in vs:
        d = 1.0 + rough * noise.noise(p * 1.6 + off)
        mb.vert(Vector(center) + Vector((p.x * radius * d, p.y * radius * d, p.z * radius * d * squash)))
    for k, (a, b, c) in enumerate(fs):
        n = (vs[a] + vs[b] + vs[c]).normalized()
        mb.face((base + a, base + b, base + c), colf(k, n), smooth=False)


# ----------------------------------------------------------------------------------------------------------------
# object creation / validation
# ----------------------------------------------------------------------------------------------------------------
def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    bsdf = nt.nodes.get("Principled BSDF")
    if bsdf:
        nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.6
    return m


REPORT = {}


def build(mb, name, mat="M_KG_PropVC", alpha_fn=None, recalc=True, sharp_deg=48.0):
    bm = bmesh.new()
    bv = [bm.verts.new(p) for p in mb.v]
    bm.verts.index_update()
    bm.verts.ensure_lookup_table()
    lay = bm.loops.layers.float_color.new("Col")
    alphas = [alpha_fn(p) if alpha_fn else 1.0 for p in mb.v]
    dup = 0
    for fi, idx in enumerate(mb.f):
        try:
            f = bm.faces.new([bv[i] for i in idx])
        except ValueError:
            dup += 1
            continue
        f.smooth = mb.sm[fi]
        pos = {vi: k for k, vi in enumerate(idx)}
        for lp in f.loops:
            k = pos[lp.vert.index]
            c = mb.c[fi][k]
            lp[lay] = (c[0], c[1], c[2], alphas[lp.vert.index])
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    col = me.color_attributes.get("Col")
    me.color_attributes.active_color = col
    try:
        me.color_attributes.render_color_index = me.color_attributes.active_color_index
    except Exception:
        pass
    try:
        me.set_sharp_from_angle(angle=math.radians(sharp_deg))
    except Exception:
        pass
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(material(mat))
    REPORT[name] = {"dup": dup}
    return ob


def validate(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    tris = sum(len(f.verts) - 2 for f in bm.faces)
    degen = sum(1 for f in bm.faces if f.calc_area() < 1e-9)
    for f in [f for f in bm.faces if f.calc_area() < 1e-9][:4]:
        print("  degenerate face", ob.name, tuple(round(c, 4) for c in f.calc_center_median()), len(f.verts))
    open_e = sum(1 for e in bm.edges if not e.is_manifold)
    # islands -> signed volume (positive = normals outward)
    seen, neg, islands = set(), 0, 0
    for f0 in bm.faces:
        if f0.index in seen:
            continue
        islands += 1
        stack, vol = [f0], 0.0
        seen.add(f0.index)
        while stack:
            f = stack.pop()
            vs = [l.vert.co for l in f.loops]
            for k in range(1, len(vs) - 1):
                vol += vs[0].dot(vs[k].cross(vs[k + 1])) / 6.0
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if vol < 0:
            neg += 1
    bm.free()
    xs = [v.co for v in ob.data.vertices]
    mn = Vector((min(p.x for p in xs), min(p.y for p in xs), min(p.z for p in xs)))
    mx = Vector((max(p.x for p in xs), max(p.y for p in xs), max(p.z for p in xs)))
    REPORT[ob.name].update(tris=tris, degen=degen, open_edges=open_e, islands=islands, neg_islands=neg, mn=mn, mx=mx)


# ----------------------------------------------------------------------------------------------------------------
# FISH
# ----------------------------------------------------------------------------------------------------------------
def fin(mb, bases, tips, n, thick, fc, segs=2, bend=0.0):
    """Thin closed fin plate. Rays base_i -> tip_i, thickness along n tapering to a sharp outer edge.
    fc(i_face, t_corner) -> rgb."""
    R = len(bases)
    A = [[0] * (segs + 1) for _ in range(R)]
    B = [[0] * (segs + 1) for _ in range(R)]
    for i in range(R):
        for k in range(segs + 1):
            t = k / segs
            p = bases[i].lerp(tips[i], t) + n * (bend * t * t)
            if k == segs:
                A[i][k] = B[i][k] = mb.vert(p)
            else:
                th = thick * 0.5 * (1.0 - 0.8 * t)
                A[i][k], B[i][k] = mb.vert(p + n * th), mb.vert(p - n * th)
    for i in range(R - 1):
        for k in range(segs):
            t0, t1 = k / segs, (k + 1) / segs
            ca, cb = fc(i, t0), fc(i, t1)
            mb.face([A[i][k], A[i + 1][k], A[i + 1][k + 1], A[i][k + 1]], [ca, ca, cb, cb])
            mb.face([B[i][k], B[i][k + 1], B[i + 1][k + 1], B[i + 1][k]], [ca, cb, cb, ca])
    for k in range(segs):
        t0, t1 = k / segs, (k + 1) / segs
        mb.face([A[0][k], A[0][k + 1], B[0][k + 1], B[0][k]], [fc(0, t0), fc(0, t1), fc(0, t1), fc(0, t0)])
        e = R - 1
        mb.face([A[e][k], B[e][k], B[e][k + 1], A[e][k + 1]],
                [fc(e - 1, t0), fc(e - 1, t0), fc(e - 1, t1), fc(e - 1, t1)])
    for i in range(R - 1):
        mb.face([A[i][0], B[i][0], B[i + 1][0], A[i + 1][0]], fc(i, 0.0))


def eye(mb, P, N, r, iris, n=10):
    N = N.normalized()
    u = perp(N)
    v = N.cross(u)
    rings = [[mb.vert(P - N * (0.45 * r))],
             mb.verts(ring_pts(P - N * (0.3 * r), u, v, 0.8 * r, 0.8 * r, n)),
             mb.verts(ring_pts(P + N * (0.12 * r), u, v, r, r, n)),
             mb.verts(ring_pts(P + N * (0.26 * r), u, v, 0.78 * r, 0.78 * r, n)),
             mb.verts(ring_pts(P + N * (0.36 * r), u, v, 0.48 * r, 0.48 * r, n)),
             [mb.vert(P + N * (0.42 * r))]]
    dark, pupil = (0.10, 0.09, 0.10), (0.02, 0.02, 0.04)
    cols = [dark, dark, dark, iris, pupil]
    mb.loft(rings, lambda i, j: cols[i])


def fish(name, L, sp):
    """Normalised build: nose at x=0, tail tip at x=-1; scaled to L metres and centred at the end."""
    mb = MB()
    BL = sp["BL"]
    zc, ht, hb, wd = mono(sp["zc"]), mono(sp["ht"]), mono(sp["hb"]), mono(sp["w"])
    sn = sp["nose"]
    nt, nb = sp.get("ridge", 1.6), sp.get("belly", 2.3)

    def rnd(s):
        if s >= sn:
            return 1.0
        q = 1.0 - s / sn
        return math.sqrt(max(0.0, 1.0 - q * q))

    def surf(s, th):
        r = rnd(s)
        c, sn_ = math.cos(th), math.sin(th)
        z = zc(s) + (ht(s) * r * c if c >= 0 else hb(s) * r * c)
        y = wd(s) * r * sgnpow(sn_, 2.0 / (nt if c >= 0 else nb))
        return Vector((-s * BL, y, z))

    def normal(s, th):
        e = 1e-3
        ds = surf(s + e, th) - surf(s - e, th)
        dt = surf(s, th + e) - surf(s, th - e)
        nrm = ds.cross(dt).normalized()
        P = surf(s, th)
        if nrm.dot(P - Vector((P.x, 0, zc(s)))) < 0:
            nrm = -nrm
        return nrm

    NT, NR = sp.get("rows", 24), 16
    s0 = 0.012
    gill = sp.get("gill", 0.2)
    GW = 0.006                                   # half width of the gill-cover line
    S = []
    for i in range(NT - 2):
        u = i / (NT - 3)
        s = s0 + (1 - s0) * (0.4 * u + 0.6 * (0.5 - 0.5 * math.cos(math.pi * u)))
        if abs(s - gill) > 2.2 * GW:
            S.append(s)
    S = sorted(S + [gill - GW, gill + GW])
    NT = len(S)
    gill_band = S.index(gill - GW) + 1           # band index (rings list has the nose pole first)

    def warp(s, th):
        # bend the rings around the gill into an arc (bulging towards the tail at mid height)
        return s + 0.022 * math.sin(th) ** 2 * math.exp(-((s - gill) / 0.05) ** 2)

    TH = [TAU * j / NR for j in range(NR)]
    rings = [[mb.vert((0.0, 0.0, zc(0.0)))]]
    stagger = sp.get("stagger", False)
    for k, s in enumerate(S):
        pts = []
        for j, th in enumerate(TH):
            ss = s
            if stagger and j % 2 and k + 1 < NT:
                ss += 0.5 * (S[k + 1] - s) * sm(0.2, 0.28, s) * (1 - sm(0.86, 0.93, s))
            pts.append(surf(warp(ss, th), th))
        rings.append(mb.verts(pts))
    rings.append([mb.vert((-BL - 0.01, 0.0, zc(1.0)))])
    colfn = sp["col"]
    mouth = sp.get("mouth", 0.05)

    def body_col(i, j):
        # i: band between rings[i] and rings[i+1] (0 = nose fan, NT = tail cap)
        si = [0.0] + S + [1.0]
        sa, sb = si[i], si[i + 1]
        sf = 0.5 * (sa + sb)
        thf = TAU * (j + 0.5) / NR
        znf = math.cos(thf)
        corners = []
        for (s, th, ci, cj) in ((sa, TH[j], i, j), (sa, TH[(j + 1) % NR], i, (j + 1) % NR),
                                (sb, TH[(j + 1) % NR], i + 1, (j + 1) % NR), (sb, TH[j], i + 1, j)):
            c = colfn(s, math.cos(th), sf, znf, ci, cj, i, j)
            if i == gill_band and abs(znf) < 0.9:      # gill cover edge: thin dark arc
                c = mul(colfn(sf, znf, sf, znf, i, j, i, j), 0.62)
            if sf < mouth and -0.45 < znf < 0.0:
                c = (0.22, 0.08, 0.09)
            corners.append(c)
        if i == 0:
            corners = [corners[0], corners[2], corners[3]]
        elif i == NT:
            corners = [corners[0], corners[1], corners[2]]
        return corners

    mb.loft(rings, body_col)

    # --- fins ------------------------------------------------------------------------------------------------
    fcol = sp["fin"]
    thick = sp.get("fin_thick", 0.012)

    def fc_for(kind):
        return lambda i, t: fcol(kind, i, t)

    def ridge_fin(s0_, s1_, height, sweep_, shape, kind, R=6, top=True, segs=2, th=thick):
        bases, tips = [], []
        for k in range(R):
            t = k / (R - 1)
            s = s0_ + (s1_ - s0_) * t
            zz = zc(s) + (ht(s) - 0.006 if top else -(hb(s) - 0.006))
            b = Vector((-s * BL, 0.0, zz))
            hgt = height * shape(t)
            bases.append(b)
            tips.append(b + Vector((-sweep_ * hgt, 0.0, hgt if top else -hgt)))
        fin(mb, bases, tips, Y, th, fc_for(kind), segs=segs)

    for (s0_, s1_, h, swp, shape, R) in sp.get("dorsal", []):
        ridge_fin(s0_, s1_, h, swp, shape, "dorsal", R)
    for (s0_, s1_, h, swp, shape, R) in sp.get("anal", []):
        ridge_fin(s0_, s1_, h, swp, shape, "anal", R, top=False)
    for (s0_, s1_, h, swp, shape, R) in sp.get("adipose", []):
        ridge_fin(s0_, s1_, h, swp, shape, "dorsal", R, th=0.02)
    for s in sp.get("finlets", []):
        ridge_fin(s, s + 0.03, 0.022, 0.9, lambda t: 1.0 - 0.8 * t, "finlet", 2, segs=1, th=0.006)
        ridge_fin(s, s + 0.03, 0.02, 0.9, lambda t: 1.0 - 0.8 * t, "finlet", 2, top=False, segs=1, th=0.006)

    # tail (caudal) fin: fan of rays from the peduncle, forked by ray length
    tl = sp["tail"]
    Rt = 9
    O = Vector((-BL + 0.025, 0.0, zc(1.0)))
    hbt = 0.75 * min(ht(1.0), hb(1.0))
    tlen = 1.0 + O.x
    bases, tips = [], []
    for k in range(Rt):
        v = -1.0 + 2.0 * k / (Rt - 1)
        f = tl["fork"] + (1 - tl["fork"]) * abs(v) ** tl.get("p", 1.2)
        bases.append(O + Z * (v * hbt))
        tips.append(O + Vector((-tlen * f, 0.0, v * tl["span"] * (1.0 - tl.get("pinch", 0.0) * (1 - abs(v))))))
    fin(mb, bases, tips, Y, thick * 1.1, fc_for("tail"), segs=3)

    # paired fins (pectoral behind the gill, pelvic under the belly)
    def paired(s, th, size, u_dir, v_dir, shape, kind, R=5, spread=1.4, droop=0.0):
        for side in (1, -1):
            P = surf(s, th if side > 0 else TAU - th)
            N = normal(s, th if side > 0 else TAU - th)
            u = Vector((u_dir[0], u_dir[1] * side, u_dir[2])).normalized()
            vv = Vector((v_dir[0], v_dir[1] * side, v_dir[2])).normalized()
            nrm = u.cross(vv).normalized()
            bl = size * 0.32
            bases, tips = [], []
            for k in range(R):
                t = k / (R - 1)
                b = P - N * 0.006 + vv * ((t - 0.5) * bl)
                bases.append(b)
                tips.append(b + u * (size * shape(t)) + vv * ((t - 0.5) * bl * spread))
            fin(mb, bases, tips, nrm, thick * 0.9, fc_for(kind), segs=2, bend=droop * side)

    for (s, th, size, ud, vd, shape) in sp.get("pectoral", []):
        paired(s, math.radians(th), size, ud, vd, shape, "pectoral", droop=0.0)
    for (s, th, size, ud, vd, shape) in sp.get("pelvic", []):
        paired(s, math.radians(th), size, ud, vd, shape, "pelvic", R=4)

    # barbels (thin cones hanging from the chin / mouth corners)
    for (s, th, length, direction) in sp.get("barbels", []):
        sides = (1, -1) if th not in (180,) else (1,)
        for side in sides:
            a = math.radians(th) if side > 0 else TAU - math.radians(th)
            P = surf(s, a) - normal(s, a) * 0.002
            d = Vector((direction[0], direction[1] * side, direction[2])).normalized()
            tube(mb, P, P + d * length, 0.0055, 0.0, 5, sp["barbel_col"])

    # eyes
    es, eth, er = sp["eye"]
    for side in (1, -1):
        a = math.radians(eth) if side > 0 else TAU - math.radians(eth)
        eye(mb, surf(es, a), normal(es, a), er, sp["iris"])

    # scale to metres, pivot at body centre (mid of nose..tail tip, body axis height)
    zmid = zc(0.4)
    mb.xform(lambda p: Vector(((p.x + 0.5) * L, p.y * L, (p.z - zmid) * L)))
    xmax = max(p.x for p in mb.v)
    xmin = min(p.x for p in mb.v)
    return build(mb, name, "M_KG_FishVC", alpha_fn=lambda p: clamp((xmax - p.x) / (xmax - xmin)), sharp_deg=55.0)


def fin_colour(base, tip, stripe=0.9, spots=None):
    def f(kind, i, t):
        c = mix(base, tip, sm(0.0, 1.0, t))
        if i % 2 == 1:
            c = mul(c, stripe)
        if spots and kind in spots[0] and 0.2 < t < 0.95 and h01(i, round(t * 7), len(kind)) < spots[1]:
            c = spots[2]
        return c
    return f


def col_mackerel(s, zn, sf, znf, ci, cj, fi, fj):
    back, mid, belly = (0.05, 0.50, 0.58), (0.55, 0.82, 0.84), (0.94, 0.96, 0.98)
    c = mix(belly, mid, sm(-0.45, 0.0, zn))
    c = mix(c, back, sm(0.05, 0.4, zn))
    if -0.3 < zn < 0.3 and 0.3 < s < 0.9:
        c = mix(c, (0.80, 0.90, 0.72), 0.25)          # faint gold-green iridescence on the flank
    # wavy tiger bars on the back: dark vertices on every 3rd ring, shifted one ring on alternate rows
    if zn > 0.3 and 0.24 < s < 0.93 and (ci + (cj % 2)) % 3 == 0:
        c = (0.03, 0.15, 0.27)
    if s < 0.2 and zn > 0.5:
        c = mul(back, 0.85)
    return c


def col_cod(s, zn, sf, znf, ci, cj, fi, fj):
    back, flank, belly = (0.52, 0.45, 0.20), (0.74, 0.67, 0.40), (0.97, 0.94, 0.82)
    c = mix(belly, flank, sm(-0.55, -0.05, zn))
    c = mix(c, back, sm(0.1, 0.6, zn))
    hh = h01(ci, min(cj, 16 - cj), 3)                # symmetric speckles
    if zn > -0.3 and hh < 0.3 and s > 0.05:
        c = mix(c, (0.30, 0.25, 0.10), 0.8)
    ll = (3, 13) if s < 0.3 else (4, 12)             # pale lateral line, arching over the pectoral fin
    if cj in ll and 0.14 < s < 0.97:
        c = mix(c, (0.98, 0.96, 0.86), 0.7)
    return c


def col_salmon(s, zn, sf, znf, ci, cj, fi, fj):
    back, silver, rose, belly = (0.30, 0.46, 0.60), (0.80, 0.85, 0.90), (0.98, 0.50, 0.60), (0.99, 0.99, 0.98)
    c = mix(belly, silver, sm(-0.6, -0.15, zn))
    w = math.exp(-((zn + 0.05) / 0.32) ** 2) * sm(0.12, 0.3, s) * (1 - sm(0.82, 0.97, s))
    c = mix(c, rose, 0.9 * w)
    c = mix(c, back, sm(0.35, 0.75, zn))
    if zn > 0.15 and h01(ci, min(cj, 16 - cj), 5) < 0.36 and s > 0.08:
        c = (0.07, 0.08, 0.13)
    return c


def col_carp(s, zn, sf, znf, ci, cj, fi, fj):
    back, gold, belly = (1.0, 0.40, 0.03), (1.0, 0.70, 0.08), (1.0, 0.90, 0.52)
    c = mix(belly, gold, sm(-0.6, -0.05, zn))
    c = mix(c, back, sm(0.35, 0.85, zn))
    if 0.2 < sf < 0.95:
        # every face is one big scale: bright leading edge fading to a darker trailing edge, rows alternate
        lead = ci == fi
        c = mix(c, (1.0, 0.97, 0.70), 0.45) if lead else mul(c, 0.86)
        if fj % 2:
            c = mul(c, 0.95)
    elif sf <= 0.2 and zn > 0.25:
        c = (0.98, 0.22, 0.05)                        # red-orange crown on the head
    return c


def build_fish():
    out = []
    out.append(fish("SM_KG_Fish_Mackerel", 0.35, dict(
        BL=0.80, nose=0.06, rows=26, ridge=1.7, belly=2.2, gill=0.2, mouth=0.05,
        zc=[(0, -0.008), (0.3, 0.0), (1, 0.0)],
        ht=[(0, 0.035), (0.15, 0.072), (0.36, 0.092), (0.58, 0.078), (0.8, 0.042), (0.93, 0.02), (1, 0.016)],
        hb=[(0, 0.03), (0.15, 0.066), (0.38, 0.084), (0.6, 0.068), (0.8, 0.036), (0.93, 0.018), (1, 0.015)],
        w=[(0, 0.03), (0.15, 0.056), (0.38, 0.064), (0.6, 0.054), (0.8, 0.03), (0.93, 0.014), (1, 0.011)],
        dorsal=[(0.30, 0.44, 0.075, 0.55, lambda t: 1.0 - 0.85 * t ** 0.8, 6),
                (0.58, 0.66, 0.045, 0.6, lambda t: 1.0 - 0.7 * t, 4)],
        anal=[(0.60, 0.68, 0.04, 0.6, lambda t: 1.0 - 0.7 * t, 4)],
        finlets=[0.71, 0.77, 0.83, 0.89],
        tail=dict(fork=0.34, span=0.13, p=1.1, pinch=0.1),
        pectoral=[(0.24, 100, 0.10, (-1.0, 0.45, -0.1), (0.25, 0.0, 1.0), lambda t: 0.45 + 0.55 * t ** 1.4)],
        pelvic=[(0.34, 150, 0.045, (-1.0, 0.35, -0.55), (1.0, 0.0, 0.15), lambda t: 0.6 + 0.4 * t)],
        eye=(0.075, 62, 0.027), iris=(0.86, 0.90, 0.92),
        col=col_mackerel,
        fin=fin_colour((0.30, 0.55, 0.60), (0.62, 0.80, 0.82), 0.88),
    )))
    out.append(fish("SM_KG_Fish_Cod", 0.55, dict(
        BL=0.83, nose=0.11, rows=24, ridge=1.9, belly=2.4, gill=0.22, mouth=0.06,
        zc=[(0, -0.012), (0.3, 0.0), (1, 0.006)],
        ht=[(0, 0.05), (0.12, 0.088), (0.3, 0.108), (0.5, 0.094), (0.75, 0.055), (0.92, 0.03), (1, 0.025)],
        hb=[(0, 0.048), (0.12, 0.09), (0.3, 0.11), (0.5, 0.085), (0.75, 0.045), (0.92, 0.024), (1, 0.021)],
        w=[(0, 0.052), (0.12, 0.084), (0.3, 0.084), (0.5, 0.064), (0.75, 0.034), (0.92, 0.016), (1, 0.012)],
        dorsal=[(0.27, 0.40, 0.07, 0.25, lambda t: 0.55 + 0.45 * math.sin(math.pi * min(1, t * 1.3)) ** 0.7, 5),
                (0.44, 0.61, 0.06, 0.2, lambda t: 0.5 + 0.5 * math.sin(math.pi * t) ** 0.7, 5),
                (0.65, 0.81, 0.05, 0.2, lambda t: 0.5 + 0.5 * math.sin(math.pi * t) ** 0.7, 5)],
        anal=[(0.44, 0.61, 0.05, 0.2, lambda t: 0.4 + 0.6 * math.sin(math.pi * t) ** 0.7, 5),
              (0.65, 0.81, 0.045, 0.2, lambda t: 0.4 + 0.6 * math.sin(math.pi * t) ** 0.7, 5)],
        tail=dict(fork=0.9, span=0.115, p=2.0),
        pectoral=[(0.25, 100, 0.10, (-1.0, 0.5, -0.25), (0.2, 0.0, 1.0),
                   lambda t: 0.62 + 0.38 * math.sin(math.pi * t) ** 0.6)],
        pelvic=[(0.19, 160, 0.06, (-1.0, 0.3, -0.6), (1.0, 0.0, 0.05), lambda t: 0.5 + 0.5 * t)],
        barbels=[(0.05, 180, 0.045, (-0.5, 0.0, -1.0))],
        barbel_col=(0.90, 0.84, 0.62),
        eye=(0.11, 55, 0.025), iris=(0.88, 0.80, 0.50),
        col=col_cod,
        fin=fin_colour((0.52, 0.47, 0.25), (0.74, 0.68, 0.46), 0.9,
                       spots=(("dorsal", "tail"), 0.25, (0.33, 0.28, 0.13))),
    )))
    out.append(fish("SM_KG_Fish_Salmon", 0.60, dict(
        BL=0.80, nose=0.07, rows=24, ridge=1.7, belly=2.3, gill=0.2, mouth=0.055,
        zc=[(0, -0.006), (1, 0.003)],
        ht=[(0, 0.038), (0.15, 0.082), (0.38, 0.106), (0.58, 0.092), (0.8, 0.05), (0.93, 0.03), (1, 0.026)],
        hb=[(0, 0.034), (0.15, 0.072), (0.4, 0.088), (0.6, 0.078), (0.8, 0.044), (0.93, 0.026), (1, 0.022)],
        w=[(0, 0.03), (0.15, 0.058), (0.4, 0.066), (0.6, 0.054), (0.8, 0.03), (0.93, 0.016), (1, 0.012)],
        dorsal=[(0.36, 0.50, 0.08, 0.3, lambda t: 1.0 - 0.6 * t ** 1.2, 6)],
        adipose=[(0.80, 0.86, 0.03, 0.5, lambda t: 0.4 + 0.6 * math.sin(math.pi * t), 4)],
        anal=[(0.66, 0.76, 0.05, 0.35, lambda t: 1.0 - 0.6 * t, 5)],
        tail=dict(fork=0.74, span=0.14, p=1.4),
        pectoral=[(0.22, 105, 0.085, (-1.0, 0.45, -0.35), (0.25, 0.0, 1.0), lambda t: 0.5 + 0.5 * t)],
        pelvic=[(0.48, 150, 0.05, (-1.0, 0.35, -0.55), (1.0, 0.0, 0.15), lambda t: 0.6 + 0.4 * t)],
        eye=(0.075, 60, 0.021), iris=(0.84, 0.86, 0.88),
        col=col_salmon,
        fin=fin_colour((0.42, 0.50, 0.60), (0.66, 0.72, 0.78), 0.9,
                       spots=(("dorsal", "tail"), 0.3, (0.10, 0.11, 0.16))),
    )))
    out.append(fish("SM_KG_Fish_GoldenCarp", 0.50, dict(
        BL=0.77, nose=0.09, rows=24, ridge=1.8, belly=2.4, gill=0.19, mouth=0.05,
        zc=[(0, -0.02), (0.3, 0.0), (1, 0.0)],
        ht=[(0, 0.045), (0.12, 0.108), (0.33, 0.158), (0.55, 0.14), (0.78, 0.072), (0.93, 0.042), (1, 0.036)],
        hb=[(0, 0.04), (0.12, 0.084), (0.35, 0.114), (0.55, 0.106), (0.78, 0.056), (0.93, 0.035), (1, 0.03)],
        w=[(0, 0.036), (0.12, 0.07), (0.35, 0.08), (0.55, 0.07), (0.78, 0.04), (0.93, 0.02), (1, 0.015)],
        dorsal=[(0.31, 0.72, 0.095, 0.35, lambda t: 1.0 - 0.45 * sm(0.0, 0.3, t) - 0.15 * t, 8)],
        anal=[(0.68, 0.78, 0.055, 0.4, lambda t: 1.0 - 0.55 * t, 5)],
        tail=dict(fork=0.66, span=0.19, p=0.75),
        pectoral=[(0.22, 105, 0.13, (-1.0, 0.5, -0.4), (0.25, 0.0, 1.0),
                   lambda t: 0.6 + 0.4 * math.sin(math.pi * t) ** 0.6)],
        pelvic=[(0.44, 150, 0.075, (-1.0, 0.35, -0.6), (1.0, 0.0, 0.15), lambda t: 0.55 + 0.45 * t)],
        barbels=[(0.03, 115, 0.03, (-0.4, 0.3, -1.0))],
        barbel_col=(1.0, 0.62, 0.20),
        eye=(0.09, 60, 0.023), iris=(1.0, 0.86, 0.30), stagger=True,
        col=col_carp,
        fin=fin_colour((0.92, 0.10, 0.06), (1.0, 0.62, 0.16), 0.86),
    )))
    return out


# ----------------------------------------------------------------------------------------------------------------
# ROWBOAT
# ----------------------------------------------------------------------------------------------------------------
WOOD_A, WOOD_B = (0.82, 0.54, 0.29), (0.70, 0.43, 0.22)
WOOD_IN_A, WOOD_IN_B = (0.90, 0.67, 0.40), (0.82, 0.58, 0.33)
WOOD_DARK, LIP = (0.44, 0.26, 0.13), (0.40, 0.23, 0.12)
TEAL = (0.08, 0.62, 0.66)
IRON = (0.22, 0.23, 0.26)
BRASS = (0.96, 0.72, 0.22)


def build_rowboat():
    mb = MB()
    HALF = 1.5
    Bf = mono([(0, 0.41), (0.12, 0.52), (0.40, 0.625), (0.60, 0.61), (0.78, 0.47), (0.90, 0.29), (0.97, 0.12),
               (1.0, 0.0)])
    Kf = mono([(0, -0.10), (0.15, -0.22), (0.40, -0.27), (0.62, -0.265), (0.82, -0.20), (0.93, -0.10), (1.0, 0.06)])
    Sf = mono([(0, 0.43), (0.3, 0.36), (0.5, 0.35), (0.75, 0.39), (1.0, 0.55)])

    def U(x):
        return (x + HALF) / (2 * HALF)

    def rake(x, z):
        u = U(x)
        return 0.28 * sm(0.78, 1.0, u) ** 2 * (z - Kf(u)) - 0.16 * (1 - sm(0.0, 0.06, u)) * (z - Kf(u))

    def sec(x, phi, dB=0.0, dK=0.0):
        u = U(x)
        B = max(0.0, Bf(u) - dB)
        K = Kf(u) + dK
        Sr = Sf(u)
        a = 0.62 + 0.38 * sm(0.72, 1.0, u)
        y = B * math.sin(phi) ** a
        z = K + (Sr - K) * (1 - math.cos(phi))
        return y, z

    def point(x, phi, side, delta=0.0, dB=0.0, dK=0.0):
        y, z = sec(x, phi, dB, dK)
        if delta:
            e = 1e-3
            y1, z1 = sec(x, min(math.pi / 2, phi + e), dB, dK)
            y0, z0 = sec(x, max(0.0, phi - e), dB, dK)
            ty, tz = y1 - y0, z1 - z0
            ln = math.hypot(ty, tz) or 1.0
            y += delta * tz / ln
            z += -delta * ty / ln
        return Vector((x + rake(x, z), side * y, z))

    P = 5
    edges = [0.5 * math.pi * b / P for b in range(P + 1)]
    D = 0.011
    st, lab = [(0.0, 0.0)], []            # starboard side: keel -> rim, labels of the segment below each point
    for b in range(P):
        mid = 0.5 * (edges[b] + edges[b + 1])
        if b == 0:
            st += [(mid, 0.0), (edges[1], 0.0)]
            lab += [0, 0]
        else:
            st += [(edges[b], D), (mid, D * 0.5), (edges[b + 1], 0.0)]
            lab += ["lip_top" if b == P - 1 else "lip", b, b]
    ring_lab = list(reversed(lab)) + lab
    NX = 27
    xs = [-HALF + 2 * HALF * (1 - (1 - i / (NX - 1)) ** 1.3) for i in range(NX)]

    def make_rings(xlist, seq, dB, dK, weld_last):
        rings = []
        for i, x in enumerate(xlist):
            last = weld_last and i == len(xlist) - 1
            pts = [point(x, phi, -1, d, dB, dK) for phi, d in reversed(seq[1:])] + [point(x, 0.0, 1, 0, dB, dK)] + \
                  [point(x, phi, 1, d, dB, dK) for phi, d in seq[1:]]
            if last:
                cache, idx = {}, []
                for p in pts:
                    key = round(p.z, 5)
                    if key not in cache:
                        cache[key] = mb.vert(Vector((p.x, 0.0, p.z)))
                    idx.append(cache[key])
                rings.append(idx)
            else:
                rings.append(mb.verts(pts))
        return rings

    # outer shell (clinker planks: every plank's lower edge stands proud of the one below)
    outer = make_rings(xs, st, 0.0, 0.0, True)

    def plank_col(label, a, b):
        if label == "lip":
            return LIP
        if label == "lip_top":
            return (0.97, 0.92, 0.80)
        if label == P - 1:
            return TEAL
        return a if label % 2 == 0 else b

    mb.loft(outer, lambda i, j: plank_col(ring_lab[j], WOOD_A, WOOD_B), closed_ring=False)

    # inner shell (thinner plank bands, lighter wood)
    T = 0.03
    st_in, lab_in = [(0.0, 0.0)], []
    for b in range(P):
        st_in += [(0.5 * (edges[b] + edges[b + 1]), 0.0), (edges[b + 1], 0.0)]
        lab_in += [b, b]
    ring_lab_in = list(reversed(lab_in)) + lab_in
    xb = HALF
    while Bf(U(xb)) - T < 0.0:
        xb -= 0.002
    xin = [(-HALF + T) + ((xb) - (-HALF + T)) * (x + HALF) / (2 * HALF) for x in xs]
    inner = make_rings(xin, st_in, T, T, True)
    mb.loft(inner, lambda i, j: plank_col(ring_lab_in[j], WOOD_IN_A, WOOD_IN_B) if ring_lab_in[j] != P - 1
            else (0.10, 0.55, 0.60), closed_ring=False)

    # transom: horizontal strips across the stern (outer face and inner face)
    for rg, labs, pal in ((outer[0], ring_lab, (WOOD_A, WOOD_B)), (inner[0], ring_lab_in, (WOOD_IN_A, WOOD_IN_B))):
        M = len(rg)
        for j in range(M // 2):
            mb.face([rg[j], rg[j + 1], rg[M - 2 - j], rg[M - 1 - j]], plank_col(labs[j], *pal), smooth=False)

    # rim: bridge outer rim loop to inner rim loop (closed solid)
    def rim_loop(rings):
        return [r[0] for r in rings] + [r[-1] for r in reversed(rings[:-1])]
    ro, ri = rim_loop(outer), rim_loop(inner)
    for k in range(len(ro)):
        k1 = (k + 1) % len(ro)
        mb.face([ro[k], ro[k1], ri[k1], ri[k]], WOOD_DARK, smooth=False)

    # gunwale rail on top of the rim
    mid = [(mb.v[a] + mb.v[b]) / 2 for a, b in zip(ro, ri)]
    rail = [(0.036, -0.03), (0.036, 0.012), (0.024, 0.026), (-0.024, 0.026), (-0.036, 0.012), (-0.036, -0.03)]
    sweep(mb, mid, rail, Z, lambda i, j: mul(WOOD_DARK, 1.15) if j in (2,) else WOOD_DARK, closed=True)

    # keel + stem strip
    kpath = [mb.v[r[len(r) // 2]] for r in outer[:-1]]
    bow = sorted({mb.v[i].z: mb.v[i] for i in outer[-1]}.items())
    kpath += [p for _, p in bow]
    keel = [(0.006, -0.026), (-0.03, -0.02), (-0.036, 0.0), (-0.03, 0.02), (0.006, 0.026)]
    kpath[-1] = kpath[-1] + Vector((0, 0, 0.03))
    sweep(mb, kpath, keel, Y, lambda i, j: mul(WOOD_DARK, 0.85))

    # interior: ribs, risers, floorboards, two thwarts (bench seats), oarlocks
    def inner_y_at(x, z):
        lo, hi = 0.0, math.pi / 2
        for _ in range(30):
            m = 0.5 * (lo + hi)
            if sec(x, m, T, T)[1] < z:
                lo = m
            else:
                hi = m
        return sec(x, 0.5 * (lo + hi), T, T)[0]

    def inner_z_at(x, y):
        lo, hi = 0.0, math.pi / 2
        for _ in range(30):
            m = 0.5 * (lo + hi)
            if sec(x, m, T, T)[0] < y:
                lo = m
            else:
                hi = m
        return sec(x, 0.5 * (lo + hi), T, T)[1]

    rib = [(0.012, -0.02), (0.012, 0.02), (-0.012, 0.02), (-0.012, -0.02)]
    for xr in (-1.05, -0.5, 0.05, 0.6):
        pts = []
        for k in range(-8, 9):
            phi = (math.pi / 2) * abs(k) / 8 * 0.97
            y, z = sec(xr, phi, T + 0.006, T + 0.006)
            pts.append(Vector((xr, math.copysign(y, k) if k else 0.0, z)))
        sweep(mb, pts, rib, X, lambda i, j: (0.64, 0.40, 0.21), smooth=False)

    riser_z = 0.105
    for side in (1, -1):
        pts = [Vector((x, side * (inner_y_at(x, riser_z) - 0.012), riser_z)) for x in
               [-1.25 + 0.2 * k for k in range(12)]]
        sweep(mb, pts, [(0.016, -0.018), (0.016, 0.018), (-0.016, 0.018), (-0.016, -0.018)], Z,
              lambda i, j: (0.62, 0.38, 0.19), smooth=False)

    for yb in (-0.17, 0.0, 0.17):
        pts = [Vector((x, yb, inner_z_at(x, abs(yb)) + 0.012)) for x in [-1.05 + 0.2 * k for k in range(11)]]
        sweep(mb, pts, [(0.012, -0.062), (0.012, 0.062), (-0.012, 0.062), (-0.012, -0.062)], Y,
              lambda i, j: (0.86, 0.62, 0.36) if j == 3 else (0.72, 0.50, 0.28), smooth=False)

    bench_z = 0.14
    for xb_ in (-0.62, 0.42):
        yb = inner_y_at(xb_, bench_z) + 0.003
        chamfer_box(mb, (xb_, 0, bench_z), (0.24, 2 * yb, 0.036), 0.008,
                    lambda i, j: (0.93, 0.70, 0.42) if j in (1, 2) else (0.78, 0.55, 0.31))
        zf = inner_z_at(xb_, 0.0)
        chamfer_box(mb, (xb_, 0, 0.5 * (bench_z + zf)), (0.06, 0.06, bench_z - zf), 0.006,
                    lambda i, j: (0.66, 0.42, 0.22))
        # oarlocks behind the rowing thwart
    xo = -0.62 + 0.30
    for side in (1, -1):
        rimp = point(xo, math.pi / 2, side)
        rimi = point(xo, math.pi / 2, side, 0, T, T)
        c = (rimp + rimi) / 2 + Vector((0, 0, 0.026))
        tube(mb, c - Vector((0, 0, 0.03)), c + Vector((0, 0, 0.03)), 0.008, 0.008, 8, IRON)
        torus(mb, c + Vector((0, 0, 0.055)), Y, 0.028, 0.0065, 10, 6, IRON)

    # pivot: hull centre (bbox X centre) on the waterline
    xmin, xmax = min(p.x for p in mb.v), max(p.x for p in mb.v)
    cx = 0.5 * (xmin + xmax)
    mb.xform(lambda p: Vector((p.x - cx, p.y, p.z)))
    return build(mb, "SM_KG_Rowboat", sharp_deg=40.0)


def build_oar():
    mb = MB()
    # (x, half-width Y, half-thickness Z, colour of the band that starts here)
    wood, grip, leather = (0.80, 0.55, 0.31), (0.92, 0.72, 0.46), (0.46, 0.25, 0.12)
    rows = [(0.0, 0.0, 0.0, grip), (0.0, 0.011, 0.011, grip), (0.008, 0.017, 0.017, grip), (0.03, 0.019, 0.019, grip),
            (0.13, 0.019, 0.019, grip), (0.15, 0.015, 0.015, wood), (0.19, 0.021, 0.021, wood),
            (0.62, 0.022, 0.022, WOOD_DARK), (0.625, 0.027, 0.027, leather), (0.79, 0.027, 0.027, WOOD_DARK),
            (0.795, 0.022, 0.022, wood), (1.40, 0.022, 0.022, wood), (1.52, 0.03, 0.016, wood),
            (1.64, 0.055, 0.011, wood), (1.78, 0.071, 0.009, wood), (1.93, 0.077, 0.008, (0.97, 0.93, 0.82)),
            (1.955, 0.077, 0.008, TEAL), (2.10, 0.075, 0.008, TEAL), (2.165, 0.06, 0.0075, TEAL),
            (2.192, 0.034, 0.006, TEAL), (2.2, 0.0, 0.0, TEAL)]
    rings = []
    n = 12
    for x, ry, rz, _ in rows:
        c = Vector((x, 0, 0))
        rings.append([mb.vert(c)] if ry == 0 else mb.verts(ring_pts(c, Y, Z, ry, rz, n)))
    mb.loft(rings, lambda i, j: rows[i + 1][3] if i + 1 < len(rows) and i > 0 else rows[max(1, i)][3])
    return build(mb, "SM_KG_Oar", sharp_deg=40.0)


# ----------------------------------------------------------------------------------------------------------------
# FISHING ROD, BOBBER
# ----------------------------------------------------------------------------------------------------------------
def build_rod():
    mb = MB()
    BAMBOO, NODE, CORK_A, CORK_B = (0.93, 0.80, 0.44), (0.60, 0.44, 0.20), (0.86, 0.64, 0.40), (0.76, 0.54, 0.32)
    THREAD = (0.86, 0.14, 0.10)
    rows = [(0.0, 0.0, (0.20, 0.10, 0.08)), (0.0, 0.011, (0.20, 0.10, 0.08)), (0.006, 0.0165, (0.20, 0.10, 0.08)),
            (0.02, 0.0165, BRASS)]
    x = 0.026
    rows.append((x, 0.0165, CORK_A))
    k = 0
    while x < 0.26:                        # cork rings with a gentle swell
        x = min(0.26, x + 0.026)
        t = (x - 0.026) / 0.234
        k += 1
        rows.append((x, 0.0155 + 0.0022 * math.sin(math.pi * t), CORK_A if k % 2 == 0 else CORK_B))
    rows += [(0.262, 0.0135, BRASS), (0.275, 0.0135, IRON), (0.355, 0.0135, BRASS), (0.368, 0.0145, CORK_A),
             (0.395, 0.014, CORK_B), (0.42, 0.012, CORK_A), (0.435, 0.0115, BRASS), (0.445, 0.0098, BAMBOO)]
    nodes = [0.63, 0.86, 1.09, 1.32, 1.54, 1.74]
    guides = [(0.76, 0.0095), (1.02, 0.0078), (1.27, 0.0064), (1.50, 0.0052), (1.70, 0.0042), (1.875, 0.0032)]

    def rr(x):                                       # bamboo taper
        t = (x - 0.445) / (1.9 - 0.445)
        return 0.0098 * (1 - t) + 0.0021 * t - 0.0012 * math.sin(math.pi * t)
    events = []
    for nx in nodes:
        events += [(nx - 0.008, 1.0, NODE), (nx - 0.003, 1.22, NODE), (nx + 0.003, 1.22, BAMBOO),
                   (nx + 0.008, 1.0, BAMBOO)]
    for gx, _ in guides:
        events += [(gx - 0.014, 1.0, THREAD), (gx - 0.0125, 1.12, THREAD), (gx + 0.0125, 1.12, BAMBOO),
                   (gx + 0.014, 1.0, BAMBOO)]
    xs = sorted(set([round(0.445 + (1.895 - 0.445) * i / 16, 4) for i in range(17)]))
    evx = [e[0] for e in events]
    cur = BAMBOO
    samples = sorted([(x_, 1.0, None) for x_ in xs if all(abs(x_ - e) > 0.01 for e in evx)] + events,
                     key=lambda e: e[0])
    for x_, k_, c in samples:
        if c is not None:
            cur = c
        if x_ > 0.445:
            rows.append((x_, rr(x_) * k_, cur))
    rows.append((1.9, 0.0, BAMBOO))
    n = 10
    rings = []
    for x_, r_, _ in rows:
        c = Vector((x_, 0, 0))
        rings.append([mb.vert(c)] if r_ == 0 else mb.verts(ring_pts(c, Y, Z, r_, r_, n)))
    mb.loft(rings, lambda i, j: rows[i + 1][2] if i > 0 else rows[1][2])

    # line guides: wire rings hanging under the rod on little stalks
    for gx, R in guides:
        r0 = rr(gx) * 1.12
        top = -r0 - 0.0035
        cz = top - R
        torus(mb, (gx, 0, cz), X, R, 0.0011, 10, 4, (0.80, 0.82, 0.86))
        for s in (-1, 1):
            tube(mb, (gx + s * 0.009, 0, -r0 * 0.6), (gx + s * 0.002, 0, cz + R * 0.9), 0.0011, 0.0011, 4,
                 (0.70, 0.72, 0.76))

    # reel: small centre-pin reel under the reel seat
    rx, rz = 0.315, -0.058
    tube(mb, (rx - 0.022, 0, -0.012), (rx + 0.022, 0, -0.012), 0.0035, 0.0035, 6, IRON)
    tube(mb, (rx, 0, -0.013), (rx, 0, rz + 0.03), 0.005, 0.004, 6, IRON)
    prof = [(-0.017, 0.0), (-0.017, 0.028), (-0.0155, 0.0345), (-0.010, 0.0345), (-0.010, 0.026), (0.010, 0.026),
            (0.010, 0.0345), (0.0155, 0.0345), (0.017, 0.028), (0.017, 0.0)]
    pc = [(0.72, 0.13, 0.10), (0.72, 0.13, 0.10), BRASS, (0.72, 0.13, 0.10), (0.96, 0.94, 0.84),
          (0.72, 0.13, 0.10), BRASS, (0.72, 0.13, 0.10), (0.72, 0.13, 0.10)]
    lathe(mb, (rx, 0, rz), Y, prof, 16, lambda i, j: pc[i], ref=X)
    tube(mb, (rx, 0.017, rz), (rx, 0.021, rz), 0.006, 0.006, 8, BRASS)
    tube(mb, (rx, 0.021, rz), (rx + 0.022, 0.021, rz - 0.014), 0.0035, 0.0035, 6, BRASS)
    tube(mb, (rx + 0.022, 0.021, rz - 0.014), (rx + 0.022, 0.040, rz - 0.014), 0.0045, 0.004, 8, WOOD_DARK)
    return build(mb, "SM_KG_FishingRod", sharp_deg=50.0)


def build_bobber():
    mb = MB()
    RED, WHITE, DARK, YEL = (0.93, 0.10, 0.08), (0.98, 0.98, 0.96), (0.18, 0.16, 0.18), (1.0, 0.86, 0.14)
    prof = [(-0.047, 0.0, DARK), (-0.046, 0.0022, DARK), (-0.036, 0.0032, DARK), (-0.031, 0.0048, WHITE)]
    R = 0.03
    for k in range(1, 12):
        a = math.pi * k / 12
        z = -R * math.cos(a)
        r = R * math.sin(a) * (1.0 + 0.06 * math.sin(a))
        prof.append((z, r, WHITE if z <= 0.0001 else RED))
    prof += [(0.0305, 0.0045, RED), (0.031, 0.0028, WHITE), (0.042, 0.0026, RED), (0.052, 0.0024, WHITE),
             (0.061, 0.0022, YEL), (0.0625, 0.0042, YEL), (0.069, 0.0042, YEL), (0.0715, 0.0, YEL)]
    rings = []
    for z, r, _ in prof:
        c = Vector((0, 0, z))
        rings.append([mb.vert(c)] if r == 0 else mb.verts(ring_pts(c, X, Y, r, r, 16)))
    mb.loft(rings, lambda i, j: prof[i + 1][2] if i + 1 < len(prof) - 1 else prof[i][2])
    torus(mb, (0, 0, -0.049), X, 0.0028, 0.0008, 8, 4, (0.7, 0.72, 0.76))
    return build(mb, "SM_KG_Bobber", sharp_deg=50.0)


# ----------------------------------------------------------------------------------------------------------------
# SHOVEL
# ----------------------------------------------------------------------------------------------------------------
def build_shovel():
    mb = MB()
    WOOD, HANDLE = (0.80, 0.54, 0.30), (0.88, 0.66, 0.40)
    STEEL, STEEL_D, EDGE = (0.44, 0.50, 0.58), (0.30, 0.34, 0.40), (0.82, 0.85, 0.88)
    # D-grip: crossbar along Y (back surface touches X=0) + two arms into the shaft
    cb = 0.017
    tube(mb, (cb, -0.066, 0), (cb, 0.066, 0), 0.017, 0.017, 10,
         lambda i, j: HANDLE if i == 1 else mul(HANDLE, 0.9))
    for s in (-1, 1):
        pts = []
        for k in range(9):
            t = k / 8
            p0, p1, p2 = Vector((cb, s * 0.06, 0)), Vector((0.10, s * 0.065, 0)), Vector((0.165, s * 0.004, 0))
            pts.append(p0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + p2 * t * t)
        sweep(mb, pts, [(0.012 * math.cos(a), 0.012 * math.sin(a)) for a in [TAU * q / 8 for q in range(8)]], Z,
              lambda i, j: HANDLE)
    prof = [(0.15, 0.0, IRON), (0.15, 0.021, IRON), (0.19, 0.021, WOOD)]
    for k in range(1, 9):
        x = 0.19 + (1.02 - 0.19) * k / 8
        prof.append((x, 0.019 + 0.001 * math.sin(k), WOOD if k % 2 else mul(WOOD, 0.93)))
    prof += [(1.03, 0.021, STEEL_D), (1.20, 0.024, STEEL_D), (1.21, 0.0, STEEL_D)]
    rings = []
    for x, r, _ in prof:
        c = Vector((x, 0, 0))
        rings.append([mb.vert(c)] if r == 0 else mb.verts(ring_pts(c, Y, Z, r, r, 10)))
    mb.loft(rings, lambda i, j: prof[i + 1][2] if 0 < i < len(prof) - 2 else prof[i][2])
    for s in (-1, 1):
        tube(mb, (1.12, 0, s * 0.020), (1.12, 0, s * 0.0275), 0.006, 0.0045, 6, (0.60, 0.62, 0.66))

    # blade: dished spade with a rounded point, lifted ~10 degrees, starts at the socket end
    blade_start = len(mb.v)
    x0, x1 = 1.19, 1.47
    wf = mono([(0.0, 0.112), (0.08, 0.124), (0.2, 0.128), (0.5, 0.125), (0.72, 0.105), (0.88, 0.07), (0.97, 0.03),
               (1.0, 0.0)])
    NB, NW = 12, 8
    rings = []
    for k in range(NB + 1):
        t = k / NB
        x = x0 + (x1 - x0) * t
        w = wf(t)
        if k == NB:
            rings.append([mb.vert((x, 0, 0.0))])
            continue
        pts = []
        for q in range(NW + 1):              # top surface, y from -w to w
            yy = -w + 2 * w * q / NW
            dish = 0.55 * yy * yy
            th = 0.0035 * (1 - (yy / max(w, 1e-4)) ** 2) + 0.0012 + 0.006 * (1 - sm(0.0, 0.2, t))
            pts.append(Vector((x, yy, dish + th)))
        for q in range(NW, -1, -1):          # bottom surface back
            yy = -w + 2 * w * q / NW
            dish = 0.55 * yy * yy
            th = 0.0035 * (1 - (yy / max(w, 1e-4)) ** 2) + 0.0012 + 0.006 * (1 - sm(0.0, 0.2, t))
            pts.append(Vector((x, yy, dish - th)))
        rings.append(mb.verts(pts))
    cap = mb.vert(Vector((x0 - 0.004, 0, 0)))
    rings = [[cap]] + rings

    def bcol(i, j):
        t = (i - 0.5) / NB
        edge = (j in (NW, 2 * NW + 1) or (t > 0.8 and j in (0, NW - 1, NW + 1, 2 * NW)) or t > 0.9)
        return EDGE if edge else (STEEL if j < NW else STEEL_D)
    mb.loft(rings, bcol, smooth=True)
    # tread: rolled top edge of the blade
    tube(mb, (x0 + 0.01, -0.118, 0.012), (x0 + 0.01, 0.118, 0.012), 0.007, 0.007, 8, STEEL_D)
    lift = Matrix.Rotation(math.radians(10), 4, "Y")
    pivot = Vector((1.19, 0, 0))
    mb.xform(lambda p: pivot + lift @ (p - pivot), start=blade_start)
    return build(mb, "SM_KG_Shovel", sharp_deg=40.0)


# ----------------------------------------------------------------------------------------------------------------
# DIG MOUND / DUG HOLE
# ----------------------------------------------------------------------------------------------------------------
SOIL_A, SOIL_B, SOIL_C = (0.43, 0.27, 0.15), (0.33, 0.20, 0.11), (0.56, 0.37, 0.21)
CLOD = (0.60, 0.42, 0.25)


def soil_col(k, n):
    c = (0.62, 0.44, 0.27) if n.z > 0.3 else (0.50, 0.34, 0.20)
    return mul(c, 0.92 + 0.16 * h01(k, 7))


def stone_col(k, n):
    c = (0.70, 0.68, 0.64) if n.z > 0.2 else (0.52, 0.52, 0.54)
    return mul(c, 0.94 + 0.12 * h01(k, 3))


def polar_ground(mb, R, rings_r, zf, colp, S=30, seed=1.0):
    """Top surface over a disc: centre vertex + rings. zf(r, a) -> z. Faces point +Z."""
    centre = mb.vert((0, 0, zf(0.0, 0.0)))
    rings = [[centre]]
    for r in rings_r:
        pts = []
        for j in range(S):
            a = -TAU * j / S                     # clockwise ring order -> faces point up
            jit = 1.0 + (0.07 * noise.noise(Vector((math.cos(a) * 2.2, math.sin(a) * 2.2, seed))) if r >= R * 0.99
                         else 0.03 * noise.noise(Vector((math.cos(a) * 3 + r * 5, math.sin(a) * 3, seed + 3))))
            rr = r * jit
            pts.append(Vector((rr * math.cos(a), rr * math.sin(a), zf(r, a))))
        rings.append(mb.verts(pts))
    vcol = {k: colp(mb.v[k]) for r in rings for k in r}
    mb.loft(rings, None, vcol=vcol)
    return rings


def soil_at(p, seed, dark=0.0):
    """Loose soil: dark earth with lighter crumbly patches (per vertex, from 3D noise)."""
    n1 = noise.noise(Vector((p.x * 7.0, p.y * 7.0, p.z * 7.0 + seed)))
    n2 = noise.noise(Vector((p.x * 19.0 + 5, p.y * 19.0, p.z * 19.0 + seed)))
    c = mix(SOIL_B, SOIL_A, sm(-0.4, 0.3, n1))
    c = mix(c, mix(SOIL_C, CLOD, 0.4), sm(0.1, 0.5, n2) * 0.9)
    return mix(c, (0.12, 0.075, 0.045), dark)


def build_mound():
    mb = MB()
    R, H = 0.6, 0.3

    def zf(r, a):
        if r >= R * 0.99:
            return 0.0
        q = r / R
        base = H * math.cos(0.5 * math.pi * q) ** 1.6
        d = Vector((math.cos(a) * r, math.sin(a) * r, 0.0))
        lump = 0.05 * noise.noise(d * 5.0 + Vector((0, 0, 0.7))) + 0.02 * noise.noise(d * 13.0 + Vector((3, 1, 0)))
        return max(0.0, base + lump * math.sin(math.pi * min(1.0, q * 1.1)))
    rr = [0.05, 0.11, 0.17, 0.23, 0.29, 0.35, 0.41, 0.47, 0.52, 0.565, 0.6]
    rings = polar_ground(mb, R, rr, zf, lambda p: soil_at(p, 1.0), S=32)
    bot = mb.vert((0, 0, 0))
    mb.loft([rings[-1], [bot]], lambda i, j: SOIL_B)
    for k in range(20):
        a = TAU * h01(k, 1)
        r = 0.05 + 0.48 * h01(k, 2) ** 0.8
        s = 0.022 + 0.04 * h01(k, 4)
        blob(mb, (r * math.cos(a), r * math.sin(a), zf(r, a) + s * 0.2), s, 0.7, k, soil_col)
    for k in range(6):
        a = TAU * h01(k, 9)
        r = 0.12 + 0.4 * h01(k, 8)
        s = 0.018 + 0.016 * h01(k, 6)
        blob(mb, (r * math.cos(a), r * math.sin(a), zf(r, a) + s * 0.2), s, 0.55, k + 40, stone_col, rough=0.18)
    return build(mb, "SM_KG_DigMound", sharp_deg=38.0)


def build_hole():
    mb = MB()
    R = 0.6
    prof = mono([(0.0, -0.30), (0.1, -0.29), (0.2, -0.21), (0.28, -0.08), (0.34, 0.03), (0.41, 0.095),
                 (0.49, 0.06), (0.56, 0.015), (0.6, -0.005)])

    def zf(r, a):
        lump = 0.025 * noise.noise(Vector((math.cos(a) * 3.0, math.sin(a) * 3.0, r * 4 + 1.3)))
        return prof(r) + (lump if 0.3 < r < 0.58 else 0.0)
    rr = [0.07, 0.14, 0.2, 0.25, 0.3, 0.34, 0.38, 0.42, 0.46, 0.51, 0.56, 0.6]

    def colp(p):
        return soil_at(p, 4.0, dark=0.85 * (1 - sm(-0.28, 0.02, p.z)))   # darker the deeper it goes
    polar_ground(mb, R, rr, zf, colp, S=32, seed=4.0)
    for k in range(14):
        a = TAU * h01(k, 21)
        r = 0.36 + 0.17 * h01(k, 22)
        s = 0.025 + 0.03 * h01(k, 23)
        blob(mb, (r * math.cos(a), r * math.sin(a), zf(r, a) + s * 0.2), s, 0.7, k + 70, soil_col)
    for k in range(4):
        a = TAU * h01(k, 31)
        r = 0.38 + 0.18 * h01(k, 32)
        s = 0.016 + 0.014 * h01(k, 33)
        blob(mb, (r * math.cos(a), r * math.sin(a), zf(r, a) + s * 0.2), s, 0.55, k + 90, stone_col, rough=0.18)
    ob = build(mb, "SM_KG_DugHole", recalc=False, sharp_deg=38.0)
    # the ground surface is open: make sure its faces point up (blobs are closed and wound outward already)
    me = ob.data
    up = sum(p.normal.z * p.area for p in me.polygons)
    if up < 0:
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        bm.to_mesh(me)
        bm.free()
    return ob


# ----------------------------------------------------------------------------------------------------------------
# DOORBELL
# ----------------------------------------------------------------------------------------------------------------
def build_doorbell():
    mb = MB()
    IRON_B, IRON_H = (0.16, 0.15, 0.17), (0.30, 0.29, 0.32)
    BR, BR_HI, BR_LO = (0.98, 0.74, 0.20), (1.0, 0.90, 0.52), (0.78, 0.50, 0.12)
    # wall plate: rounded rectangle in the YZ plane, back face on X=0
    def rrect(w, h, rad, n=4):
        pts = []
        for cx, cy, a0 in ((w - rad, h - rad, 0), (-w + rad, h - rad, 90), (-w + rad, -h + rad, 180),
                           (w - rad, -h + rad, 270)):
            for k in range(n + 1):
                a = math.radians(a0 + 90 * k / n)
                pts.append((cx + rad * math.cos(a), cy + rad * math.sin(a)))
        return pts
    rings = [[mb.vert((0, 0, 0))]]
    for x, ins in ((0.0, 0.0), (0.009, 0.0), (0.014, 0.006)):
        rings.append(mb.verts([Vector((x, y * (1 - ins / 0.04), z * (1 - ins / 0.075))) for y, z in
                               rrect(0.04, 0.075, 0.018)]))
    rings.append([mb.vert((0.014, 0, 0))])
    mb.loft(rings, lambda i, j: IRON_H if i == 2 else IRON_B, smooth=False)
    for zz in (0.052, -0.052):
        lathe(mb, (0.013, 0, zz), X, [(0, 0.0), (0, 0.0065), (0.004, 0.0055), (0.0065, 0.003), (0.0075, 0.0)], 8,
              lambda i, j: BRASS)
    # arm with an end scroll, and a curved brace below it
    rnd = [(0.0065 * math.cos(a), 0.0065 * math.sin(a)) for a in [TAU * q / 8 for q in range(8)]]
    arm = [Vector((0.012 + 0.2 * k / 10, 0, 0.025 - 0.004 * (k / 10) ** 2)) for k in range(11)]
    c0 = arm[-1] + Vector((0, 0, 0.022))
    for k in range(1, 12):                     # scroll curling up and back
        a = -math.pi / 2 + 1.5 * math.pi * k / 11
        rad = 0.022 * (1 - 0.45 * k / 11)
        arm.append(c0 + Vector((rad * math.cos(a), 0, rad * math.sin(a))))
    sweep(mb, arm, rnd, Y, lambda i, j: IRON_H if j in (1, 2) else IRON_B)
    brace = []
    for k in range(13):
        t = k / 12
        p0, p1, p2 = Vector((0.012, 0, -0.05)), Vector((0.05, 0, 0.0)), Vector((0.13, 0, 0.022))
        brace.append(p0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + p2 * t * t)
    sweep(mb, brace, [(0.005 * math.cos(a), 0.005 * math.sin(a)) for a in [TAU * q / 8 for q in range(8)]], Y,
          lambda i, j: IRON_B)
    # hanging link + bell crown
    bx = 0.175
    ztop = arm[int(0.175 / 0.02)].z
    torus(mb, (bx, 0, ztop - 0.004), Y, 0.011, 0.0028, 12, 5, IRON_B)
    torus(mb, (bx, 0, ztop - 0.03), X, 0.009, 0.0035, 12, 5, BR_LO)
    # bell: closed lathe profile (outer surface down to the flared lip, inner surface back up)
    bt = ztop - 0.036
    prof = [(0.0, 0.0), (0.0, 0.016), (-0.006, 0.026), (-0.018, 0.034), (-0.04, 0.037), (-0.07, 0.041),
            (-0.095, 0.048), (-0.112, 0.058), (-0.121, 0.064), (-0.126, 0.063), (-0.124, 0.056), (-0.112, 0.048),
            (-0.09, 0.040), (-0.06, 0.034), (-0.03, 0.031), (-0.012, 0.022), (-0.007, 0.0)]
    pc = [BR_HI, BR_HI, BR, BR, BR, BR, BR, BR_HI, BR_LO, BR_LO, BR_LO, BR_LO, BR_LO, BR_LO, BR_LO, BR_LO]
    rings = []
    for dz, r in prof:
        c = Vector((bx, 0, bt + dz))
        rings.append([mb.vert(c)] if r == 0 else mb.verts(ring_pts(c, X, Y, r, r, 24)))
    mb.loft(rings, lambda i, j: pc[i])
    # clapper, pull cord and tassel (warm red rope)
    tube(mb, (bx, 0, bt - 0.006), (bx, 0, bt - 0.118), 0.003, 0.003, 6, IRON_B)
    lathe(mb, (bx, 0, bt - 0.118), -Z, [(-0.0, 0.0), (0.002, 0.009), (0.01, 0.013), (0.018, 0.009), (0.021, 0.0)],
          10, lambda i, j: IRON_H)
    RED, RED_D = (0.86, 0.14, 0.10), (0.62, 0.08, 0.07)
    cord = [Vector((bx + 0.004 * math.sin(k * 1.3), 0, bt - 0.138 - 0.012 * k)) for k in range(7)]
    sweep(mb, cord, [(0.0028 * math.cos(a), 0.0028 * math.sin(a)) for a in [TAU * q / 6 for q in range(6)]], Y,
          lambda i, j: RED if i % 2 == 0 else RED_D)
    tz = cord[-1].z
    lathe(mb, (cord[-1].x, 0, tz), -Z, [(0, 0.0), (0.0, 0.006), (0.008, 0.007), (0.012, 0.005), (0.016, 0.009),
                                        (0.052, 0.013), (0.056, 0.0)], 10,
          lambda i, j: BRASS if i in (1, 2) else (RED if j % 2 == 0 else RED_D))
    return build(mb, "SM_KG_Doorbell", sharp_deg=45.0)


# ----------------------------------------------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------------------------------------------
objs = build_fish()
objs += [build_rowboat(), build_oar(), build_rod(), build_bobber(), build_shovel(), build_mound(), build_hole(),
         build_doorbell()]
for o in objs:
    validate(o)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                          export_apply=False)

print("KG_WATERPROPS bounds (metres, Blender Z-up):")
for o in objs:
    r = REPORT[o.name]
    print(f"  {o.name:22s} min ({r['mn'].x:+.3f}, {r['mn'].y:+.3f}, {r['mn'].z:+.3f})  "
          f"max ({r['mx'].x:+.3f}, {r['mx'].y:+.3f}, {r['mx'].z:+.3f})  tris {r['tris']:5d}  "
          f"islands {r['islands']:3d} neg {r['neg_islands']} open_edges {r['open_edges']} degen {r['degen']} "
          f"dupfaces {r['dup']}")
print(f"KG_WATERPROPS: {len(objs)} meshes -> {OUT}")


# ----------------------------------------------------------------------------------------------------------------
# preview contact sheet (Workbench). Colours are linearised in place AFTER the export (preview only).
# ----------------------------------------------------------------------------------------------------------------
VIEWS = {
    "side": Matrix.Identity(4),
    "q": Matrix.Rotation(math.radians(22), 4, "X") @ Matrix.Rotation(math.radians(-32), 4, "Z"),
    "top": Matrix.Rotation(math.radians(70), 4, "X") @ Matrix.Rotation(math.radians(-10), 4, "Z"),
    "high": Matrix.Rotation(math.radians(48), 4, "X") @ Matrix.Rotation(math.radians(-28), 4, "Z"),
    "front": Matrix.Rotation(math.radians(12), 4, "X") @ Matrix.Rotation(math.radians(-80), 4, "Z"),
    "back": Matrix.Rotation(math.radians(18), 4, "X") @ Matrix.Rotation(math.radians(130), 4, "Z"),
}


def render_sheet(path, layout, res=1800, crops=None):
    """Grid of linked duplicates, each rotated to its view and scaled to fill a cell; orthographic camera."""
    scn = bpy.context.scene
    crops = crops or {}
    coll = bpy.data.collections.new("PV_Sheet")
    scn.collection.children.link(coll)
    rows, cols = len(layout), max(len(r) for r in layout)
    for r, row in enumerate(layout):
        for c, (name, view) in enumerate(row):
            src = bpy.data.objects[name]
            dup = bpy.data.objects.new(name + "_pv", src.data)
            coll.objects.link(dup)
            R = VIEWS[view]
            vs = [v.co for v in src.data.vertices]
            if name in crops:
                lo, hi = crops[name]
                vs = [p for p in vs if all(lo[k] <= p[k] <= hi[k] for k in range(3))]
            pts = [R @ p for p in vs]
            mn = Vector((min(p.x for p in pts), 0, min(p.z for p in pts)))
            mx = Vector((max(p.x for p in pts), 0, max(p.z for p in pts)))
            k = 0.88 / max(mx.x - mn.x, mx.z - mn.z)
            target = Vector((c - (cols - 1) / 2, 0, (rows - 1) / 2 - r))
            dup.matrix_world = Matrix.Translation(target) @ Matrix.Scale(k, 4) @ Matrix.Translation(-(mn + mx) / 2) @ R
    cam_d = bpy.data.cameras.new("PV")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = max(rows, cols)
    cam = bpy.data.objects.new("PV", cam_d)
    coll.objects.link(cam)
    cam.location = (0, -20, 0)
    cam.rotation_euler = (math.radians(90), 0, 0)
    scn.camera = cam
    scn.render.resolution_x = res
    scn.render.resolution_y = int(res * rows / cols)
    scn.render.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    for o in list(coll.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(coll)
    print(f"KG_WATERPROPS preview -> {path}")


def render_preview(path):
    scn = bpy.context.scene
    for o in objs:
        col = o.data.color_attributes["Col"]
        for d in col.data:
            c = d.color
            d.color = (c[0] ** 2.2, c[1] ** 2.2, c[2] ** 2.2, c[3])
        o.hide_render = True
    scn.render.engine = "BLENDER_WORKBENCH"
    sh = scn.display.shading
    sh.light = "STUDIO"
    sh.color_type = "VERTEX"
    sh.show_object_outline = True
    sh.show_cavity = True
    sh.cavity_type = "WORLD"
    scn.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("PVW")
    world.color = (0.62, 0.68, 0.74)
    scn.world = world
    fish = ["SM_KG_Fish_Mackerel", "SM_KG_Fish_Cod", "SM_KG_Fish_Salmon", "SM_KG_Fish_GoldenCarp"]
    render_sheet(path, [
        [(f, "side") for f in fish],
        [(f, "q") for f in fish],
        [("SM_KG_Rowboat", "q"), ("SM_KG_Rowboat", "side"), ("SM_KG_Rowboat", "high"), ("SM_KG_Oar", "q")],
        [("SM_KG_FishingRod", "q"), ("SM_KG_Bobber", "q"), ("SM_KG_Shovel", "q"), ("SM_KG_Doorbell", "q")],
        [("SM_KG_DigMound", "q"), ("SM_KG_DugHole", "high"), ("SM_KG_Fish_GoldenCarp", "top"),
         ("SM_KG_Rowboat", "back")],
    ])
    detail = os.environ.get("KG_WATERPROPS_DETAIL")
    if detail:     # zoomed inspection sheets (not part of the deliverable)
        render_sheet(os.path.join(detail, "fish_side.png"), [[(f, "side")] for f in fish[:2]], res=1400)
        render_sheet(os.path.join(detail, "fish_side2.png"), [[(f, "side")] for f in fish[2:]], res=1400)
        render_sheet(os.path.join(detail, "fish_q.png"), [[(fish[0], "q"), (fish[1], "q")],
                                                          [(fish[2], "q"), (fish[3], "q")]], res=1600)
        render_sheet(os.path.join(detail, "fish_top.png"), [[(f, "top") for f in fish]], res=1600)
        render_sheet(os.path.join(detail, "boat.png"), [[("SM_KG_Rowboat", "q"), ("SM_KG_Rowboat", "high")],
                                                        [("SM_KG_Rowboat", "side"), ("SM_KG_Rowboat", "back")]],
                     res=1600)
        render_sheet(os.path.join(detail, "small.png"), [[("SM_KG_FishingRod", "q"), ("SM_KG_Shovel", "high")],
                                                         [("SM_KG_Doorbell", "q"), ("SM_KG_Bobber", "q")]],
                     res=1400, crops={"SM_KG_FishingRod": (Vector((-0.02, -0.1, -0.12)), Vector((0.5, 0.1, 0.05))),
                                      "SM_KG_Shovel": (Vector((0.9, -0.2, -0.2)), Vector((1.6, 0.2, 0.2)))})
        render_sheet(os.path.join(detail, "ground.png"), [[("SM_KG_DigMound", "q"), ("SM_KG_DugHole", "high")],
                                                          [("SM_KG_Oar", "q"), ("SM_KG_FishingRod", "side")]],
                     res=1400)


if PREVIEW and PREVIEW.lower() != "none":
    render_preview(PREVIEW)
