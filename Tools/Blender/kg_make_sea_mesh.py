"""Sea surface mesh for the wave material: a warped grid that is dense around the harbour and coarse towards the
horizon, so the vertex-displaced waves look detailed where players are and cost little elsewhere.

  blender --background --factory-startup --python Tools/Blender/kg_make_sea_mesh.py -- Art/Packed/KG_Sea.glb

Blender frame (metres): the harbour focus is at (0, -55) (UE (0, 5500) cm). Z = 0 is sea level.
"""
import math
import sys

import bpy

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)

N = 240                  # quads per side
HALF = 700.0             # metres from the focus to the edge
FOCUS = (0.0, -55.0)
POWER = 2.1              # >1 packs vertices near the focus (spacing ~0.6 m there, tens of metres at the edge)

verts, faces = [], []
for j in range(N + 1):
    v = j / N * 2.0 - 1.0
    for i in range(N + 1):
        u = i / N * 2.0 - 1.0
        x = FOCUS[0] + math.copysign(abs(u) ** POWER, u) * HALF
        y = FOCUS[1] + math.copysign(abs(v) ** POWER, v) * HALF
        verts.append((x, y, 0.0))
for j in range(N):
    for i in range(N):
        a = j * (N + 1) + i
        faces.append((a, a + 1, a + N + 2, a + N + 1))

mesh = bpy.data.meshes.new("SM_KG_Sea")
mesh.from_pydata(verts, [], faces)
mesh.update()
obj = bpy.data.objects.new("SM_KG_Sea", mesh)
bpy.context.scene.collection.objects.link(obj)
obj.data.materials.append(bpy.data.materials.new("M_KG_SeaSurface"))
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB")
print(f"KG_SEA: {len(verts)} verts, {len(faces)} quads -> {out}")
