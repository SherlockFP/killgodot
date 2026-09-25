"""Shrine island for across the bay: a separate mesh (so the main terrain need not be re-imported), vertex coloured
like the terrain (wet sand -> sand -> grass -> a little rock), gentle hill with a flat top for the pagoda.

  blender --background --factory-startup --python Tools/Blender/kg_make_island.py -- Art/Packed/KG_Island.glb

Pivot at sea level (Z 0) in the island centre; radius ~34 m at the waterline, top ~5.5 m. Heights in metres.
"""
import math
import sys

import bpy
from mathutils import Vector, noise

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
R = 42.0
N = 120
STEP = 2 * R / N


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def lerp3(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def height(x, y):
    r = math.hypot(x, y) * (1.0 + 0.12 * noise.noise(Vector((x / 18.0, y / 18.0, 0.3))))
    h = -3.0 + 8.8 * (1.0 - smooth(6.0, 36.0, r))        # dome from -3 m (sea floor) to +5.8 m
    h += 0.6 * noise.fractal(Vector((x / 9.0, y / 9.0, 1.7)), 0.5, 2.0, 3) * smooth(20.0, 12.0, r)
    if r < 8.0:
        h = h * (1.0 - smooth(8.0, 5.0, r)) + 5.6 * smooth(8.0, 5.0, r)   # flat top terrace for the shrine
    return h


verts, faces, cols = [], [], []
for j in range(N + 1):
    for i in range(N + 1):
        x, y = -R + i * STEP, -R + j * STEP
        z = height(x, y)
        verts.append((x, y, z))
        g = 0.5 + 0.5 * noise.noise(Vector((x / 6.0, y / 6.0, 4.0)))
        c = lerp3((0.33, 0.62, 0.22), (0.46, 0.70, 0.26), g)
        c = lerp3((0.70, 0.60, 0.42), c, smooth(0.2, 2.2, z)) if z < 2.2 else c
        if z < 1.0:
            c = lerp3((0.62, 0.53, 0.38), (0.93, 0.80, 0.52), smooth(-1.0, 1.0, z))
        cols.append(c)
for j in range(N):
    for i in range(N):
        a = j * (N + 1) + i
        faces.append((a, a + 1, a + N + 2, a + N + 1))

mesh = bpy.data.meshes.new("SM_KG_Island")
mesh.from_pydata(verts, [], faces)
mesh.update()
col = mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
for k, c in enumerate(cols):
    col.data[k].color = (c[0], c[1], c[2], 1.0)
mesh.color_attributes.active_color = col
for p in mesh.polygons:
    p.use_smooth = True
obj = bpy.data.objects.new("SM_KG_Island", mesh)
bpy.context.scene.collection.objects.link(obj)
obj.data.materials.append(bpy.data.materials.new("M_KG_Plain"))
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True)
print("KG_ISLAND", out, len(verts))
