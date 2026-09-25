"""Import the packed EXTERNAL kits (Tools/Blender/kg_ext_pack.py -> Art/Packed/Ext/KG_Ext_<Tag>.glb + .json) as static
meshes under /Game/KillGodot/Env/Ext/KG_Ext_<Tag>/StaticMeshes/SM_KG_<Tag>_<piece> with the house vertex-colour material
(M_KG_PropVCLinear), collision from the manifest (complex / box / none) and Nanite off. New folders only: never
touches shipped assets.

Headless (works while the user's editor is open, it is a separate process):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_import_ext_pack.py KPirate KKForest" -unattended -nosplash -nopause -nullrhi
No tags = every Art/Packed/Ext/KG_Ext_*.glb. Result summary: Saved/Logs/kg_import_ext_<stamp>.json
"""
import glob
import json
import os
import sys
import time

import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAT = "/Game/KillGodot/Materials/M_KG_PropVCLinear"
PACKED = "D:/Kill Godot/Art/Packed/Ext"
DEST = "/Game/KillGodot/Env/Ext"
tags = [a for a in sys.argv[1:] if a and not a.startswith("-")]
if not tags:
    tags = [os.path.basename(p)[7:-4] for p in sorted(glob.glob(f"{PACKED}/KG_Ext_*.glb"))]
mat = unreal.load_asset(MAT)
assert mat, MAT
results = {}
for tag in tags:
    src = f"{PACKED}/KG_Ext_{tag}.glb"
    man = f"{PACKED}/KG_Ext_{tag}.json"
    if not (os.path.exists(src) and os.path.exists(man)):
        results[tag] = {"error": "packed glb/json missing"}
        continue
    props = json.load(open(man))["props"]
    dest = DEST   # Interchange adds a folder named after the file: /Env/Ext/KG_Ext_<Tag>/StaticMeshes
    meshes = f"{dest}/KG_Ext_{tag}/StaticMeshes"
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t0 = time.time()
    tools.import_asset_tasks([t])
    ok, bad = [], []
    for name, spec in props.items():
        path = f"{meshes}/SM_KG_{name}"
        m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
        if not m:
            bad.append(name)
            continue
        for i in range(max(1, len(m.get_editor_property("static_materials")))):
            m.set_material(i, mat)
        ns = m.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        m.set_editor_property("nanite_settings", ns)
        col = spec.get("collision", "box")
        unreal.EditorStaticMeshLibrary.remove_collisions(m)
        bs = m.get_editor_property("body_setup")
        if col == "complex":
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        elif col == "box":
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptingCollisionShapeType.BOX)
        eal.save_loaded_asset(m)
        ok.append(name)
    # drop the throw-away import material/texture folder Interchange made next to the meshes (meshes use MAT)
    for sub in ("Materials", "Textures"):
        p = f"{dest}/KG_Ext_{tag}/{sub}"
        if eal.does_directory_exist(p):
            eal.delete_directory(p)
    results[tag] = {"ok": len(ok), "missing": bad, "folder": meshes, "secs": round(time.time() - t0)}
    unreal.log(f"KG_EXT_IMPORT {tag}: {len(ok)} ok -> {meshes}; missing {len(bad)}: {bad[:8]}")
stamp = time.strftime("%Y%m%d_%H%M%S")
json.dump(results, open(f"D:/Kill Godot/Saved/Logs/kg_import_ext_{stamp}.json", "w"), indent=1)
unreal.log(f"KG_EXT_IMPORT done: {sum(r.get('ok', 0) for r in results.values())} meshes in {len(results)} packs")
