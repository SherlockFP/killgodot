"""Procedural house-interior props (vertex-coloured, no textures) that the CC0 kits lack: hearth, rugs, hanging herbs,
stair/gallery railings, firewood, bread and cheese.

  blender --background --factory-startup --python Tools/Blender/kg_make_interior_props.py -- Art/Packed/KG_Interior.glb [preview.png|none]

One .glb, one object per prop at the world origin with its pivot baked in (Unreal relies on it). Like the Quaternius
and JayBee furniture, every prop FACES Blender -Y (= Unreal +Y); wall-mounted props have their back on the Y=0 plane.
  SM_KG_Hearth       stone fireplace, back on Y=0, 1.9 x 0.95 x 2.97 m (reaches the 3 m ceiling). Slot 1 = embers.
  SM_KG_Rug_Oval / _Rect / _Runner / _Round   1.2 cm thick floor rugs, centred on the origin.
  SM_KG_HerbRack     1 m wall batten (pivot at the wall, batten centre) with herb/garlic/chilli bundles hanging ~0.5 m.
  SM_KG_HerbBundle   single bundle; pivot at the top of the string (hang from a beam).
  SM_KG_Firewood     log pile 0.55 x 0.55 m, log ends facing front.
  SM_KG_Bread / SM_KG_Cheese   table food.
  SM_KG_StairRail    handrail + balusters for the kit's Stair_Interior_Solid, in that mesh's own frame: stair rises
                     along +Y (first riser Y=0, landing from Y=3.85), rail on the -X stringer (X=-0.815). The
                     stringer is ray-cast from Art/Packed/KG_Village.glb so the balusters stand exactly on it.
  SM_KG_Railing_1m   gallery balustrade 1 m long along X (X -0.5..0.5), on the Y=0 line, 1.0 m tall.
  SM_KG_RailPost     0.11 m newel post, 1.08 m tall.
Colour attribute "Col" (corner domain) is sRGB (M_KG_PropVC squares it); faces tagged "hot" use material slot 1
(M_KG_Embers, emissive) instead.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(ROOT, "Art", "Packed", "KG_Interior.glb")
PREVIEW = argv[1] if len(argv) > 1 else os.path.join(ROOT, "Art", "Concept", "InteriorProps_preview.png")
OUT = OUT if os.path.isabs(OUT) else os.path.join(ROOT, OUT)
PREVIEW = PREVIEW if os.path.isabs(PREVIEW) or PREVIEW.lower() == "none" else os.path.join(ROOT, PREVIEW)
KIT = os.path.join(ROOT, "Art", "Packed", "KG_Village.glb")

bpy.ops.wm.read_factory_settings(use_empty=True)
TAU = 2.0 * math.pi
R = random.Random(7)

# sRGB palette (vibrant toon, warm woods against cool accents)
STONE = [(0.64, 0.60, 0.54), (0.56, 0.53, 0.49), (0.70, 0.66, 0.58), (0.52, 0.49, 0.46), (0.60, 0.55, 0.47)]
MORTAR = (0.33, 0.30, 0.27)
SOOT = (0.11, 0.10, 0.10)
FIREBACK = (0.20, 0.16, 0.14)
PLASTER = (0.88, 0.82, 0.70)
WOOD_D = (0.34, 0.21, 0.12)
WOOD_M = (0.48, 0.31, 0.18)
WOOD_L = (0.62, 0.43, 0.26)
BARK = (0.30, 0.21, 0.13)
LOG_END = (0.80, 0.64, 0.42)
LOG_RING = (0.66, 0.48, 0.29)
EMBER = [(1.0, 0.50, 0.10), (1.0, 0.75, 0.28), (0.95, 0.30, 0.06)]
RED, RED_D = (0.74, 0.17, 0.14), (0.50, 0.10, 0.10)
OCHRE, CREAM = (0.90, 0.63, 0.20), (0.93, 0.86, 0.68)
TEAL, NAVY, GREEN = (0.12, 0.52, 0.54), (0.17, 0.24, 0.45), (0.32, 0.50, 0.22)
HERB = [(0.36, 0.56, 0.20), (0.26, 0.44, 0.16), (0.46, 0.60, 0.26)]
LAVENDER, GARLIC, CHILLI, TWINE = (0.58, 0.46, 0.80), (0.93, 0.90, 0.82), (0.82, 0.16, 0.10), (0.80, 0.71, 0.52)
CRUST, CRUST_L, CHEESE, RIND = (0.72, 0.44, 0.18), (0.90, 0.70, 0.42), (0.98, 0.82, 0.36), (0.88, 0.62, 0.20)


def jit(c, k=0.05):
    return tuple(max(0.0, min(1.0, v * (1.0 + R.uniform(-k, k)))) for v in c)


# ----------------------------------------------------------------------------------------------------------------
# mesh builder: verts + faces with per-face sRGB colour and material slot (0 = M_KG_PropVC, 1 = M_KG_Embers)
# ----------------------------------------------------------------------------------------------------------------
class MB:
    def __init__(self):
        self.v, self.f = [], []   # f: (indices, rgb, slot, smooth)

    def vert(self, p):
        self.v.append(Vector(p))
        return len(self.v) - 1

    def face(self, idx, col, slot=0, smooth=False, ref=None):
        """ref: a point on the inside; the face is wound so its normal points away from it."""
        if len(set(idx)) < 3:
            return
        idx = list(idx)
        if ref is not None:
            pts = [self.v[i] for i in idx]
            n = Vector((0, 0, 0))
            for k in range(len(pts)):      # Newell normal
                a, b = pts[k], pts[(k + 1) % len(pts)]
                n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
            c = sum(pts, Vector()) / len(pts)
            if n.dot(c - Vector(ref)) < 0:
                idx.reverse()
        self.f.append((tuple(idx), tuple(col[:3]), slot, smooth))


def box(mb, lo, hi, col, slot=0, skip=()):
    """Axis-aligned box lo..hi. col: rgb or fn(face_name)->rgb. skip: face names to omit (e.g. 'b' against a wall)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    p = [mb.vert(v) for v in ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                               (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))]
    faces = {"bottom": (0, 3, 2, 1), "top": (4, 5, 6, 7), "front": (0, 1, 5, 4), "back": (2, 3, 7, 6),
             "left": (3, 0, 4, 7), "right": (1, 2, 6, 5)}
    ref = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    for name, q in faces.items():
        if name in skip:
            continue
        mb.face([p[i] for i in q], col(name) if callable(col) else col, slot, ref=ref)


def cbox(mb, lo, hi, ch, col, slot=0):
    """Chamfered box (all 12 edges bevelled by ch) - reads as a cut stone under the outline post-process."""
    lo, hi = Vector(lo), Vector(hi)
    c, h = (lo + hi) / 2, (hi - lo) / 2
    ch = min(ch, *(h * 0.45))
    pts = {}
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                for ax in range(3):   # three corner verts per corner, each pulled in on the other two axes
                    q = [sx * h.x, sy * h.y, sz * h.z]
                    for o in range(3):
                        if o != ax:
                            q[o] -= (sx, sy, sz)[o] * ch
                    pts[(sx, sy, sz, ax)] = mb.vert(c + Vector(q))
    colf = (lambda k: col(k)) if callable(col) else (lambda k: col)
    for ax in range(3):       # 6 main faces
        for s in (-1, 1):
            o1, o2 = [a for a in range(3) if a != ax]
            quad = []
            for u, v in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                key = [0, 0, 0]
                key[ax], key[o1], key[o2] = s, u, v
                quad.append(pts[(key[0], key[1], key[2], ax)])
            mb.face(quad, colf(ax), slot, ref=c)
    for a in range(3):        # 12 edge bevels
        b_, c_ = [o for o in range(3) if o != a]
        for sb in (-1, 1):
            for sc in (-1, 1):
                q = []
                for sa in (-1, 1):
                    for ax in (b_, c_):
                        key = [0, 0, 0]
                        key[a], key[b_], key[c_] = sa, sb, sc
                        q.append(pts[(key[0], key[1], key[2], ax)])
                mb.face([q[0], q[1], q[3], q[2]], colf(3), slot, ref=c)
    for sx in (-1, 1):        # 8 corner triangles
        for sy in (-1, 1):
            for sz in (-1, 1):
                mb.face([pts[(sx, sy, sz, 0)], pts[(sx, sy, sz, 1)], pts[(sx, sy, sz, 2)]], colf(3), slot, ref=c)


def cyl(mb, p0, p1, r, n, side, cap, slot=0, rings=None, jag=0.0, smooth=True):
    """Cylinder p0->p1 (radius r or per-ring radii), capped with `cap` colour (callable(ring_k) allowed)."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0).normalized()
    u = ax.cross(Vector((0, 0, 1)) if abs(ax.z) < 0.9 else Vector((1, 0, 0))).normalized()
    v = ax.cross(u).normalized()
    radii = rings or [(0.0, r), (1.0, r)]
    ring_idx, ring_c = [], []
    for t, rr in radii:
        c = p0 + (p1 - p0) * t
        ring_c.append(c)
        ring_idx.append([mb.vert(c + (u * math.cos(TAU * j / n) + v * math.sin(TAU * j / n))
                                 * rr * (1.0 + R.uniform(-jag, jag))) for j in range(n)])
    for k in range(len(ring_idx) - 1):
        a, b = ring_idx[k], ring_idx[k + 1]
        for j in range(n):
            j1 = (j + 1) % n
            mb.face([a[j], a[j1], b[j1], b[j]], side(k) if callable(side) else side, slot, smooth,
                    ref=(ring_c[k] + ring_c[k + 1]) / 2)
    mid = (p0 + p1) / 2
    for ring, cc in ((ring_idx[0], p0), (ring_idx[-1], p1)):
        if cap is None:
            continue
        ctr = mb.vert(cc)
        for j in range(n):
            mb.face([ring[j], ring[(j + 1) % n], ctr], cap(0) if callable(cap) else cap, slot, ref=mid)


def blob(mb, c, rx, ry, rz, col, slot=0, n=8, m=5, jag=0.12):
    """Low-poly lumpy ellipsoid (UV sphere with noise)."""
    c = Vector(c)
    rows = []
    for i in range(1, m):
        th = math.pi * i / m
        rows.append([mb.vert(c + Vector((rx * math.sin(th) * math.cos(TAU * j / n), ry * math.sin(th) * math.sin(TAU * j / n),
                                         rz * math.cos(th)))
                             + Vector((R.uniform(-jag, jag) * rx, R.uniform(-jag, jag) * ry, R.uniform(-jag, jag) * rz)))
                     for j in range(n)])
    top, bot = mb.vert(c + Vector((0, 0, rz))), mb.vert(c - Vector((0, 0, rz)))
    colf = col if callable(col) else (lambda k: col)
    for j in range(n):
        j1 = (j + 1) % n
        mb.face([top, rows[0][j], rows[0][j1]], colf(0), slot, ref=c)
        mb.face([bot, rows[-1][j1], rows[-1][j]], colf(m), slot, ref=c)
        for i in range(len(rows) - 1):
            mb.face([rows[i][j], rows[i + 1][j], rows[i + 1][j1], rows[i][j1]], colf(i + 1), slot, ref=c)


def material(name, emissive=False):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    bsdf = nt.nodes.get("Principled BSDF")
    nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    if emissive:
        nt.links.new(vc.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 4.0
    return m


OBJS = []


def build(mb, name):
    bm = bmesh.new()
    bv = [bm.verts.new(p) for p in mb.v]
    lay = bm.loops.layers.float_color.new("Col")
    slots_used = sorted({f[2] for f in mb.f})
    for idx, col, slot, smooth in mb.f:
        try:
            f = bm.faces.new([bv[i] for i in idx])
        except ValueError:
            continue
        f.material_index = slots_used.index(slot)
        f.smooth = smooth
        for lp in f.loops:
            lp[lay] = (col[0], col[1], col[2], 1.0)
    bm.normal_update()
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.color_attributes.active_color = me.color_attributes["Col"]
    me.color_attributes.render_color_index = me.color_attributes.active_color_index   # exported as COLOR_0
    for s in slots_used:
        me.materials.append(material("M_KG_Embers", True) if s == 1 else material("M_KG_PropVC"))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    OBJS.append(ob)
    return ob


# ----------------------------------------------------------------------------------------------------------------
# HEARTH: stone firebox block, timber lintel, oak mantel, plastered chimney breast up to the ceiling, logs + embers
# ----------------------------------------------------------------------------------------------------------------
def stone_face(mb, axis_u, u0, u1, z0, z1, plane, normal_sign, depth=0.025, holes=()):
    """Lay coursed stones on a vertical face. axis_u 'x' or 'y' = along-face axis; plane = the face coordinate on the
    other horizontal axis; stones protrude `depth` along normal_sign. holes: [(u0,u1,z0,z1)] left open."""
    z = z0
    row = 0
    while z < z1 - 0.02:
        hgt = min(R.uniform(0.15, 0.21), z1 - z)
        u = u0 - (R.uniform(0.05, 0.18) if row % 2 else 0.0)
        while u < u1 - 0.01:
            wid = R.uniform(0.22, 0.40)
            a, b = max(u, u0), min(u + wid, u1)
            if b - a > 0.05 and not any(a < h1 and b > h0 and z < hz1 and z + hgt > hz0 for h0, h1, hz0, hz1 in holes):
                g = 0.012
                lo_n, hi_n = (plane - 0.01, plane + depth) if normal_sign > 0 else (plane - depth, plane + 0.01)
                if axis_u == "x":
                    lo, hi = (a + g, lo_n, z + g), (b - g, hi_n, z + hgt - g)
                else:
                    lo, hi = (lo_n, a + g, z + g), (hi_n, b - g, z + hgt - g)
                cbox(mb, lo, hi, 0.018, jit(R.choice(STONE), 0.06))
            u += wid
        z += hgt
        row += 1


def build_hearth():
    mb = MB()
    W, D, H1 = 0.85, 0.72, 1.20            # half width, depth, firebox block height
    OX, OZ, BACK = 0.47, 0.84, -0.26       # opening half width, opening height, firebox back plane
    # mortar cores (stones sit 2.5 cm proud of them)
    box(mb, (-W, -D, 0), (-OX, 0, H1), MORTAR, skip=("back", "bottom"))
    box(mb, (OX, -D, 0), (W, 0, H1), MORTAR, skip=("back", "bottom"))
    box(mb, (-OX, -D, OZ), (OX, 0, H1), MORTAR, skip=("back", "bottom"))
    box(mb, (-OX, BACK, 0), (OX, 0, OZ), lambda k: FIREBACK, skip=("back", "bottom"))
    # sooty firebox lining + floor
    box(mb, (-OX + 0.001, -D + 0.02, 0.0), (OX - 0.001, BACK, 0.10), lambda k: SOOT if k == "top" else (0.26, 0.24, 0.22))
    box(mb, (-OX + 0.002, -D + 0.05, OZ - 0.08), (OX - 0.002, BACK, OZ), SOOT, skip=("top",))
    stone_face(mb, "x", -W, W, 0.0, H1, -D, -1, holes=[(-OX, OX, 0.0, OZ + 0.02)])
    stone_face(mb, "y", -D, 0.0, 0.0, H1, -W, -1)
    stone_face(mb, "y", -D, 0.0, 0.0, H1, W, +1)
    # hearth slab in front
    for k, (a, b) in enumerate(((-0.95, -0.30), (-0.30, 0.33), (0.33, 0.95))):
        cbox(mb, (a + 0.01, -0.95, 0.0), (b - 0.01, -D + 0.01, 0.07), 0.015, jit(STONE[k + 1], 0.05))
    # timber lintel over the opening
    cbox(mb, (-OX - 0.14, -D - 0.05, OZ - 0.01), (OX + 0.14, -D + 0.12, OZ + 0.15), 0.02, WOOD_D)
    # mantel shelf on two corbels
    cbox(mb, (-0.98, -D - 0.13, H1), (0.98, 0.0, H1 + 0.09), 0.02, WOOD_M)
    for s in (-1, 1):
        cbox(mb, (s * 0.80 - 0.05, -D - 0.10, H1 - 0.16), (s * 0.80 + 0.05, -D + 0.02, H1), 0.015, WOOD_D)
    # plastered chimney breast (slight taper) with a wooden band at the ceiling
    box(mb, (-0.64, -0.50, H1 + 0.09), (0.64, 0.0, 2.80), PLASTER, skip=("back", "bottom"))
    box(mb, (-0.67, -0.53, 2.80), (0.67, 0.0, 2.97), WOOD_D, skip=("back",))
    # logs + ember bed + flames (slot 1 = emissive)
    for (x0, x1, y, z, r) in ((-0.30, 0.26, -0.44, 0.17, 0.065), (-0.24, 0.32, -0.56, 0.16, 0.06),
                              (-0.22, 0.22, -0.50, 0.28, 0.058)):
        cyl(mb, (x0, y, z), (x1, y + R.uniform(-0.04, 0.04), z), r, 8, BARK,
            lambda k: LOG_END, jag=0.08, smooth=False)
    for k in range(9):
        blob(mb, (R.uniform(-0.32, 0.32), R.uniform(-0.64, -0.34), 0.105), R.uniform(0.05, 0.09), R.uniform(0.04, 0.07),
             0.025, R.choice(EMBER), slot=1, n=6, m=3)
    for k in range(5):     # flame tongues: thin twisted cones
        x, y = -0.20 + k * 0.10 + R.uniform(-0.03, 0.03), -0.50 + R.uniform(-0.05, 0.05)
        hgt = R.uniform(0.22, 0.40)
        cyl(mb, (x, y, 0.20), (x + R.uniform(-0.03, 0.03), y, 0.20 + hgt), 0.05, 5, (1.0, 0.62, 0.15), None, slot=1,
            rings=[(0.0, 0.05), (0.45, 0.042), (1.0, 0.004)], smooth=False)
    return build(mb, "SM_KG_Hearth")


# ----------------------------------------------------------------------------------------------------------------
# RUGS
# ----------------------------------------------------------------------------------------------------------------
TH = 0.012


def rug_grid(mb, hx, hy, nx, ny, colf, fringe=None):
    """Flat rectangular rug: top grid nx*ny cells coloured by colf(u, v) with u, v in [-1, 1]; bevelled rim."""
    g = [[mb.vert((-hx + 2 * hx * i / nx, -hy + 2 * hy * j / ny, TH)) for j in range(ny + 1)] for i in range(nx + 1)]
    for i in range(nx):
        for j in range(ny):
            u, v = -1 + (2 * i + 1) / nx, -1 + (2 * j + 1) / ny
            mb.face([g[i][j], g[i + 1][j], g[i + 1][j + 1], g[i][j + 1]], colf(u, v), ref=(0, 0, -10))
    rim = colf(1.0, 1.0)
    for (a0, b0), (a1, b1) in (((-hx, -hy), (hx, -hy)), ((hx, -hy), (hx, hy)), ((hx, hy), (-hx, hy)), ((-hx, hy), (-hx, -hy))):
        p = [mb.vert((a0, b0, TH)), mb.vert((a1, b1, TH)), mb.vert((a1 * 1.004, b1 * 1.004, 0.0)), mb.vert((a0 * 1.004, b0 * 1.004, 0.0))]
        mb.face([p[1], p[0], p[3], p[2]], rim, ref=(0, 0, TH / 2))
    if fringe:
        for s in (-1, 1):
            k = 0
            y = -hy + 0.03
            while y < hy - 0.02:
                x0 = s * hx
                box(mb, (min(x0, x0 + s * 0.07), y - 0.008, 0.0), (max(x0, x0 + s * 0.07), y + 0.008, 0.006), fringe)
                y += 0.045
                k += 1


def rug_oval(mb, rx, ry, bands, n=40):
    """Braided rag rug: concentric elliptical bands, each its own colour, rounded rim."""
    rings = [[mb.vert((rx * t * math.cos(TAU * j / n), ry * t * math.sin(TAU * j / n), TH)) for j in range(n)]
             for t in [k / len(bands) for k in range(1, len(bands) + 1)]]
    ctr = mb.vert((0, 0, TH))
    for j in range(n):
        mb.face([ctr, rings[0][j], rings[0][(j + 1) % n]], bands[0], ref=(0, 0, -10))
    for k in range(1, len(bands)):
        for j in range(n):
            j1 = (j + 1) % n
            mb.face([rings[k - 1][j], rings[k][j], rings[k][j1], rings[k - 1][j1]], jit(bands[k], 0.03), ref=(0, 0, -10))
    lip = [mb.vert((rx * 1.01 * math.cos(TAU * j / n), ry * 1.01 * math.sin(TAU * j / n), 0.0)) for j in range(n)]
    for j in range(n):
        j1 = (j + 1) % n
        mb.face([rings[-1][j], lip[j], lip[j1], rings[-1][j1]], bands[-1], ref=(0, 0, TH / 2))


def build_rugs():
    out = []
    mb = MB()
    rug_oval(mb, 0.95, 0.62, [OCHRE, RED, CREAM, RED_D, OCHRE, TEAL, RED, CREAM, RED_D])
    out.append(build(mb, "SM_KG_Rug_Oval"))
    mb = MB()
    rug_oval(mb, 0.70, 0.70, [TEAL, CREAM, NAVY, OCHRE, TEAL, CREAM, NAVY], n=36)
    out.append(build(mb, "SM_KG_Rug_Round"))

    def rect_col(u, v):
        a, b = abs(u), abs(v)
        if a > 0.90 or b > 0.86:
            return NAVY
        if a > 0.84 or b > 0.76:
            return CREAM
        if abs(a * 1.25 + b) < 0.55:
            return OCHRE if abs(a * 1.25 + b) > 0.30 else CREAM
        if (round(u * 11) + round(v * 7)) % 4 == 0 and a < 0.78 and b < 0.68:
            return RED_D
        return RED
    mb = MB()
    rug_grid(mb, 1.10, 0.75, 44, 30, rect_col, fringe=CREAM)
    out.append(build(mb, "SM_KG_Rug_Rect"))

    def runner_col(u, v):
        a = abs(v)
        if a > 0.82:
            return GREEN
        if a > 0.70:
            return CREAM
        k = int((u + 1) * 9)
        return (TEAL, OCHRE, CREAM, RED)[k % 4] if a < 0.35 else (TEAL if k % 2 else NAVY)
    mb = MB()
    rug_grid(mb, 1.20, 0.40, 36, 12, runner_col, fringe=CREAM)
    out.append(build(mb, "SM_KG_Rug_Runner"))
    return out


# ----------------------------------------------------------------------------------------------------------------
# HERBS
# ----------------------------------------------------------------------------------------------------------------
def bundle(mb, top, kind, length):
    """A tied bunch hanging from `top` (string -> tie -> bunch fanning downward)."""
    top = Vector(top)
    tie = top - Vector((0, 0, 0.07))
    cyl(mb, top, tie, 0.004, 4, TWINE, None, smooth=False)
    if kind == "garlic":
        z = tie.z
        for k in range(5):
            z -= 0.055
            blob(mb, (tie.x + R.uniform(-0.02, 0.02), tie.y + R.uniform(-0.01, 0.01), z), 0.035, 0.035, 0.032, jit(GARLIC, 0.03),
                 n=7, m=4, jag=0.06)
        return
    if kind == "chilli":
        cyl(mb, tie, tie - Vector((0, 0, length)), 0.003, 4, TWINE, None, smooth=False)
        for k in range(8):
            z = tie.z - 0.03 - k * length / 8
            a = R.uniform(0, TAU)
            base = Vector((tie.x + 0.012 * math.cos(a), tie.y + 0.012 * math.sin(a), z))
            cyl(mb, base, base + Vector((0.03 * math.cos(a), 0.03 * math.sin(a), -0.07)), 0.013, 5, jit(CHILLI, 0.05), None,
                rings=[(0.0, 0.013), (1.0, 0.002)], smooth=False)
        return
    col = LAVENDER if kind == "lavender" else None
    for k in range(9):           # stems fanning out and down; leafy/flower tip
        a = TAU * k / 9 + R.uniform(-0.2, 0.2)
        spread = R.uniform(0.03, 0.07)
        tip = tie + Vector((spread * math.cos(a), spread * math.sin(a), -length * R.uniform(0.8, 1.0)))
        cyl(mb, tie, tie + (tip - tie) * 0.45, 0.004, 4, (0.45, 0.50, 0.25), None, smooth=False)
        blob(mb, tie + (tip - tie) * 0.72, 0.028, 0.028, length * 0.30, col or jit(R.choice(HERB), 0.06), n=5, m=3, jag=0.2)
    blob(mb, tie, 0.018, 0.018, 0.012, TWINE, n=6, m=3, jag=0.0)


def build_herbs():
    mb = MB()
    cbox(mb, (-0.50, -0.055, -0.035), (0.50, 0.0, 0.035), 0.01, WOOD_D)
    kinds = ["herb", "garlic", "lavender", "chilli", "herb"]
    for k, kind in enumerate(kinds):
        x = -0.40 + k * 0.20
        cyl(mb, (x, -0.05, 0.0), (x, -0.12, 0.012), 0.012, 6, WOOD_M, WOOD_L, smooth=False)
        bundle(mb, (x, -0.105, 0.0), kind, R.uniform(0.26, 0.34))
    rack = build(mb, "SM_KG_HerbRack")
    mb = MB()
    bundle(mb, (0, 0, 0), "herb", 0.32)
    return [rack, build(mb, "SM_KG_HerbBundle")]


# ----------------------------------------------------------------------------------------------------------------
# FIREWOOD, FOOD
# ----------------------------------------------------------------------------------------------------------------
def build_firewood():
    mb = MB()
    rows = [(4, 0.068), (3, 0.068), (2, 0.068), (1, 0.066)]
    for ri, (cnt, r) in enumerate(rows):
        z = r + ri * r * 1.72
        for k in range(cnt):
            x = (k - (cnt - 1) / 2) * r * 2.02 + R.uniform(-0.008, 0.008)
            y0, y1 = -0.28 + R.uniform(-0.03, 0.03), 0.27 + R.uniform(-0.03, 0.03)
            rr = r * R.uniform(0.9, 1.0)
            cyl(mb, (x, y1, z), (x, y0, z), rr, 8, jit(BARK, 0.08),
                lambda k_: jit(LOG_END, 0.04), jag=0.07, smooth=False)
            cyl(mb, (x, y0 + 0.002, z), (x, y0 - 0.004, z), rr * 0.55, 8, LOG_RING, LOG_RING, smooth=False)
    return build(mb, "SM_KG_Firewood")


def build_food():
    mb = MB()
    blob(mb, (0, 0, 0.055), 0.15, 0.085, 0.06, lambda k: CRUST if k > 1 else CRUST_L, n=12, m=6, jag=0.03)
    for k in range(3):   # scoring slashes
        x = -0.07 + k * 0.07
        box(mb, (x - 0.012, -0.05, 0.108), (x + 0.012, 0.05, 0.116), CREAM)
    bread = build(mb, "SM_KG_Bread")
    mb = MB()
    n = 20
    cut = 3                                                     # a wedge is missing
    top, bot = [], []
    for j in range(n - cut + 1):
        a = TAU * j / n
        top.append(mb.vert((0.14 * math.cos(a), 0.14 * math.sin(a), 0.08)))
        bot.append(mb.vert((0.14 * math.cos(a), 0.14 * math.sin(a), 0.0)))
    ct, cb = mb.vert((0, 0, 0.08)), mb.vert((0, 0, 0.0))
    mid = (0, 0, 0.04)
    for j in range(n - cut):
        mb.face([ct, top[j], top[j + 1]], CHEESE, ref=mid)
        mb.face([cb, bot[j + 1], bot[j]], RIND, ref=mid)
        mb.face([bot[j], bot[j + 1], top[j + 1], top[j]], RIND, ref=mid)
    a_end = TAU * (n - cut) / n
    mb.face([cb, bot[0], top[0], ct], CHEESE, ref=(0.07 * math.cos(0.4), 0.07 * math.sin(0.4), 0.04))
    mb.face([ct, top[-1], bot[-1], cb], CHEESE, ref=(0.07 * math.cos(a_end - 0.4), 0.07 * math.sin(a_end - 0.4), 0.04))
    return [bread, build(mb, "SM_KG_Cheese")]


# ----------------------------------------------------------------------------------------------------------------
# RAILINGS (match Stair_Interior_Solid from the Medieval Village kit)
# ----------------------------------------------------------------------------------------------------------------
RAIL_W = (0.43, 0.27, 0.15)
RAIL_D = (0.33, 0.20, 0.11)


def stair_profile(x):
    """Ray-cast the kit's Stair_Interior_Solid from above along Y at lateral position x -> [(y, z or None)]."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=KIT)
    new = [o for o in bpy.data.objects if o not in before]
    stair = [o for o in new if o.name.startswith("Stair_Interior_Solid") and "Extended" not in o.name][0]
    bvh = BVHTree.FromObject(stair, bpy.context.evaluated_depsgraph_get())
    out = []
    for k in range(-40, 440):
        y = k / 100.0
        hit = bvh.ray_cast(Vector((x, y, 5.0)), Vector((0, 0, -1)))
        out.append((y, hit[0].z if hit[0] else None))
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    return out


def build_stair_rail():
    """Balusters stand on the sloped side stringer (X -0.88..-0.75); the handrail runs 0.92 m above it."""
    X = -0.815
    prof = [(y, z) for y, z in stair_profile(X) if z is not None]
    slope_pts = [(y, z) for y, z in prof if 0.1 < z < 2.9]
    n = len(slope_pts)
    my, mz = sum(p[0] for p in slope_pts) / n, sum(p[1] for p in slope_pts) / n
    k = sum((p[0] - my) * (p[1] - mz) for p in slope_pts) / sum((p[0] - my) ** 2 for p in slope_pts)
    y_top = min(y for y, z in prof if z > 2.99)           # where the stringer reaches the landing

    def sz(y):                                            # stringer top
        return min(mz + (y - my) * k, 3.03)
    print("KG_INTERIOR stair stringer: slope %.4f, z(0)=%.3f, top at Y=%.3f" % (k, sz(0.0), y_top))
    H = 0.92
    mb = MB()
    yb, yt = 0.02, y_top - 0.05
    # newel posts: bottom one stands on the floor, top one on the landing edge
    for y, z0, z1 in ((yb, 0.0, sz(yb) + H + 0.08), (yt, sz(yt), sz(yt) + 0.98)):
        cbox(mb, (X - 0.055, y - 0.055, z0), (X + 0.055, y + 0.055, z1), 0.012, RAIL_D)
        cbox(mb, (X - 0.07, y - 0.07, z1), (X + 0.07, y + 0.07, z1 + 0.045), 0.012, RAIL_W)
    # balusters every ~0.3 m (one per tread) between the newels
    y = yb + 0.30
    while y < yt - 0.15:
        cbox(mb, (X - 0.022, y - 0.022, sz(y) - 0.02), (X + 0.022, y + 0.022, sz(y) + H - 0.06), 0.006, RAIL_W)
        y += 0.30
    # sloped handrail (swept box) newel to newel
    hw, hh = 0.035, 0.035
    p0, p1 = Vector((X, yb + 0.05, sz(yb + 0.05) + H - 0.03)), Vector((X, yt - 0.05, sz(yt - 0.05) + H - 0.03))
    d = (p1 - p0).normalized()
    nrm = Vector((0, -d.z, d.y))
    corners = [Vector((-hw, 0, 0)) - nrm * hh, Vector((hw, 0, 0)) - nrm * hh, Vector((hw, 0, 0)) + nrm * hh,
               Vector((-hw, 0, 0)) + nrm * hh]
    a = [mb.vert(p0 + c) for c in corners]
    b = [mb.vert(p1 + c) for c in corners]
    mid = (p0 + p1) / 2
    for i in range(4):
        i1 = (i + 1) % 4
        mb.face([a[i], a[i1], b[i1], b[i]], RAIL_D, ref=mid)
    mb.face(a, RAIL_D, ref=mid)
    mb.face(b, RAIL_D, ref=mid)
    return build(mb, "SM_KG_StairRail")


def build_gallery():
    mb = MB()
    cbox(mb, (-0.5, -0.04, 0.0), (0.5, 0.04, 0.07), 0.01, RAIL_D)             # bottom rail
    cbox(mb, (-0.5, -0.045, 0.95), (0.5, 0.045, 1.02), 0.012, RAIL_D)        # handrail
    for k in range(5):
        x = -0.4 + k * 0.2
        cbox(mb, (x - 0.022, -0.022, 0.07), (x + 0.022, 0.022, 0.95), 0.006, RAIL_W)
    rail = build(mb, "SM_KG_Railing_1m")
    mb = MB()
    cbox(mb, (-0.055, -0.055, 0.0), (0.055, 0.055, 1.04), 0.012, RAIL_D)
    cbox(mb, (-0.07, -0.07, 1.04), (0.07, 0.07, 1.08), 0.012, RAIL_W)
    return [rail, build(mb, "SM_KG_RailPost")]


# ----------------------------------------------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------------------------------------------
build_hearth()
build_rugs()
build_herbs()
build_firewood()
build_food()
build_stair_rail()
build_gallery()

for o in bpy.data.objects:
    if o not in OBJS:
        bpy.data.objects.remove(o, do_unlink=True)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True,
                          export_apply=False)
for o in OBJS:
    vs = [v.co for v in o.data.vertices]
    mn = [min(p[i] for p in vs) for i in range(3)]
    mx = [max(p[i] for p in vs) for i in range(3)]
    print(f"KG_INTERIOR {o.name:20s} min ({mn[0]:+.3f}, {mn[1]:+.3f}, {mn[2]:+.3f}) max ({mx[0]:+.3f}, {mx[1]:+.3f}, "
          f"{mx[2]:+.3f}) tris {sum(len(p.vertices) - 2 for p in o.data.polygons)} mats {[m.name for m in o.data.materials]}")
print(f"KG_INTERIOR: {len(OBJS)} meshes -> {OUT}")

if PREVIEW.lower() != "none":
    for o in OBJS:   # linearise for the preview only (after export)
        for d in o.data.color_attributes["Col"].data:
            c = d.color
            d.color = (c[0] ** 2.2, c[1] ** 2.2, c[2] ** 2.2, 1.0)
    layout = {"SM_KG_Hearth": (0.0, 0.0), "SM_KG_Rug_Oval": (2.6, 0.0), "SM_KG_Rug_Rect": (5.4, 0.0),
              "SM_KG_Rug_Runner": (2.6, 2.2), "SM_KG_Rug_Round": (5.4, 2.2), "SM_KG_HerbRack": (0.0, 2.0),
              "SM_KG_HerbBundle": (1.0, 2.6), "SM_KG_Firewood": (-1.4, 0.3), "SM_KG_Bread": (-1.4, 1.5),
              "SM_KG_Cheese": (-1.0, 1.5), "SM_KG_StairRail": (8.5, -1.0), "SM_KG_Railing_1m": (-2.8, 0.2),
              "SM_KG_RailPost": (-3.5, 0.2)}
    for o in OBJS:
        o.location = (layout[o.name][0], layout[o.name][1], 1.4 if o.name.startswith("SM_KG_Herb") else 0.0)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    sh = scn.display.shading
    sh.light, sh.color_type, sh.show_object_outline, sh.show_cavity = "STUDIO", "VERTEX", True, True
    scn.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("PV")
    w.color = (0.62, 0.68, 0.74)
    scn.world = w
    cam = bpy.data.objects.new("PV", bpy.data.cameras.new("PV"))
    scn.collection.objects.link(cam)
    cam.data.type, cam.data.ortho_scale = "ORTHO", 13.5
    cam.location = (3.2, -9.0, 7.5)
    cam.rotation_euler = (math.radians(58), 0, math.radians(8))
    scn.camera = cam
    scn.render.resolution_x, scn.render.resolution_y = 1800, 1100
    scn.render.filepath = PREVIEW
    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print("KG_INTERIOR preview ->", PREVIEW)
