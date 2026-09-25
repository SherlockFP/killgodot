"""Rig report for external character packs: per file -> bones (names, count, root), skinned meshes (tris), actions.
Used to write Tools/Unreal/dressing/pack_ext_characters.md (which packs share the Quaternius UBC/UAL skeleton).

  blender --background --factory-startup --python Tools/Blender/kg_ext_rig_info.py -- <out.json> <file_or_glob> [...]
"""
import glob
import json
import os
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
out_path, patterns = argv[0], argv[1:]
files = sorted(f for p in patterns for f in glob.glob(p))
report = {}
for path in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = os.path.splitext(path)[1].lower()
    try:
        if ext in (".glb", ".gltf"):
            bpy.ops.import_scene.gltf(filepath=path)
        else:
            bpy.ops.import_scene.fbx(filepath=path, ignore_leaf_bones=True)
    except Exception as e:  # noqa
        report[path] = {"error": str(e)[:200]}
        continue
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    bones = [b.name for a in arms for b in a.data.bones]
    roots = [b.name for a in arms for b in a.data.bones if b.parent is None]
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
    report[path] = {
        "armatures": len(arms), "bones": len(bones), "roots": roots, "bone_names": bones,
        "meshes": [o.name for o in meshes][:12], "tris": tris,
        "actions": [a.name for a in bpy.data.actions][:60],
        "height_m": round(max((max((o.matrix_world @ v.co).z for v in o.data.vertices) for o in meshes if o.data.vertices), default=0), 2),
    }
json.dump(report, open(out_path, "w"), indent=1)
print("KG_RIG_INFO", len(report), out_path)
