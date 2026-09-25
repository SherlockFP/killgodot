"""Re-export a glb without materials (colours live in vertex colours), which Unreal's Interchange imports cleanly.
  blender --background --factory-startup --python Tools/Blender/kg_strip_materials.py -- in.glb out.glb"""
import sys

import bpy

a = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=a[0])
for o in bpy.data.objects:
    if o.type == "MESH":
        o.data.materials.clear()
        if o.data.has_custom_normals:
            o.data.free_normals_split() if hasattr(o.data, "free_normals_split") else None
bpy.ops.export_scene.gltf(filepath=a[1], export_format="GLB", export_vertex_color="ACTIVE", export_materials="NONE")
print("KG_STRIP", a[1], len([o for o in bpy.data.objects if o.type == "MESH"]))
