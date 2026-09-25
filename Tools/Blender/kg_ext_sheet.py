"""Render one contact-sheet tile per packed external kit (Art/Packed/Ext/KG_Ext_<Tag>.glb): every piece normalised
into a grid cell, 3/4 orthographic view, Workbench with the baked vertex colours (so the tile also proves the colour
bake). Then Tools/Blender/kg_contact_sheet.py (PIL) tiles the PNGs with labels.

  blender --background --factory-startup --python Tools/Blender/kg_ext_sheet.py -- <out_dir> <glb> [<glb> ...]
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir, glbs = argv[0], argv[1:]
os.makedirs(out_dir, exist_ok=True)

for glb in glbs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=glb)
    objs = sorted([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: o.name)
    n = len(objs)
    if not n:
        continue
    cols = max(1, math.ceil(math.sqrt(n * 1.6)))
    rows = math.ceil(n / cols)
    for i, o in enumerate(objs):
        o.select_set(False)
        bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
        mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
        mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
        dim = max((mx - mn).length, 1e-4)
        s = 0.85 / max(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, 1e-4)
        o.scale = (s, s, s)
        c = (mn + mx) / 2 * s
        o.location = Vector(((i % cols) - c.x, -(i // cols) - c.y, -mn.z * s))
    # one flat material with the colour attribute, Workbench flat + vertex colour
    mat = bpy.data.materials.new("Sheet")
    mat.use_nodes = True
    for o in objs:
        o.data.materials.clear()
        o.data.materials.append(mat)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "VERTEX"
    sh.show_shadows = False
    sh.show_cavity = True
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x = 1920
    scene.render.resolution_y = max(400, int(1920 * (rows + 0.5) / (cols + 0.5)))
    scene.render.film_transparent = False
    scene.world = bpy.data.worlds.new("W")
    scene.world.color = (0.12, 0.12, 0.12)
    cam = bpy.data.cameras.new("C")
    cam.type = "ORTHO"
    cam.ortho_scale = (cols + 0.5) * 1.02
    co = bpy.data.objects.new("C", cam)
    scene.collection.objects.link(co)
    cx, cy = (cols - 1) / 2, -(rows - 1) / 2
    co.location = Vector((cx, cy - 20, 20 * 0.9 + 0.4))
    co.rotation_euler = (math.radians(48), 0, 0)
    scene.camera = co
    tag = os.path.splitext(os.path.basename(glb))[0].replace("KG_Ext_", "")
    scene.render.filepath = os.path.join(out_dir, f"{tag}_{n}.png")
    bpy.ops.render.render(write_still=True)
    print("KG_SHEET", tag, n, scene.render.filepath)
