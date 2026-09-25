"""Procedural grass clumps for the Ghost-of-Tsushima-style meadow (Docs/06_Art_Direction.md: stylised, windy).

  blender --background --factory-startup --python Tools/Blender/kg_make_grass.py -- Art/Packed/KG_Grass.glb

Each clump is a few dozen curved, tapered blades (3 segments each) in a ~0.9 m disc.
Vertex colour: R = height along the blade (0 root .. 1 tip, drives wind + gradient), G = per-blade random
(colour variation), B = 1 on flower petals. Normals point up (soft, uniform shading like stylised foliage).
Clumps: SM_KG_GrassShort (0.35 m), SM_KG_GrassMid (0.6 m), SM_KG_GrassTall (0.95 m), SM_KG_GrassFlowers.
"""
import math
import random
import sys

import bpy

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
rng = random.Random(7)


def clump(name, blades, height, radius, width, flowers=0):
    verts, faces, cols = [], [], []

    def blade(cx, cy, h, w, yaw, lean, rnd, petal=False):
        base = len(verts)
        segs = 3
        dx, dy = math.cos(yaw), math.sin(yaw)          # blade faces this way
        px, py = -dy, dx                               # width direction
        for s in range(segs + 1):
            t = s / segs
            bend = lean * t * t                        # curves over near the tip
            half = w * 0.5 * (1.0 - t * 0.92)          # taper to a point
            z = h * t * (1.0 - 0.25 * lean * t)
            ox, oy = cx + dx * bend, cy + dy * bend
            verts.append((ox - px * half, oy - py * half, z))
            verts.append((ox + px * half, oy + py * half, z))
            c = (t, rnd, 1.0 if petal else 0.0, 1.0)
            cols.extend([c, c])
        for s in range(segs):
            a = base + s * 2
            faces.append((a, a + 1, a + 3, a + 2))

    for _ in range(blades):
        r = radius * math.sqrt(rng.random())
        a = rng.random() * 2 * math.pi
        blade(r * math.cos(a), r * math.sin(a), height * rng.uniform(0.6, 1.15), width * rng.uniform(0.8, 1.3),
              rng.random() * 2 * math.pi, rng.uniform(0.05, 0.35) * height, rng.random())
    for _ in range(flowers):
        r = radius * 0.8 * math.sqrt(rng.random())
        a = rng.random() * 2 * math.pi
        cx, cy = r * math.cos(a), r * math.sin(a)
        h = height * rng.uniform(1.0, 1.25)
        blade(cx, cy, h, 0.012, rng.random() * 6.28, 0.02, rng.random())          # stem
        for k in range(5):                                                     # petals: small flat quad fan
            base = len(verts)
            ang = k * 2 * math.pi / 5
            tip = (cx + 0.035 * math.cos(ang), cy + 0.035 * math.sin(ang), h + 0.01)
            left = (cx + 0.02 * math.cos(ang - 0.6), cy + 0.02 * math.sin(ang - 0.6), h)
            right = (cx + 0.02 * math.cos(ang + 0.6), cy + 0.02 * math.sin(ang + 0.6), h)
            verts.extend([(cx, cy, h), left, tip, right])
            c = (1.0, rng.random(), 1.0, 1.0)
            cols.extend([c, c, c, c])
            faces.append((base, base + 1, base + 2, base + 3))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    col = mesh.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            col.data[li].color = cols[mesh.loops[li].vertex_index]
    mesh.color_attributes.active_color = col
    # Up-facing normals: stylised grass reads as one soft surface instead of noisy blades.
    mesh.normals_split_custom_set_from_vertices([(0.0, 0.0, 1.0)] * len(verts))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    mat = bpy.data.materials.get("M_KG_GrassVC") or bpy.data.materials.new("M_KG_GrassVC")
    obj.data.materials.append(mat)
    return len(faces)


tris = 0
tris += clump("SM_KG_GrassShort", 40, 0.35, 0.45, 0.035)
tris += clump("SM_KG_GrassMid", 46, 0.62, 0.5, 0.04)
tris += clump("SM_KG_GrassTall", 50, 0.95, 0.5, 0.045)
tris += clump("SM_KG_GrassFlowers", 34, 0.55, 0.45, 0.04, flowers=4)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True)
print(f"KG_GRASS: 4 clumps, {tris} quads -> {out}")
