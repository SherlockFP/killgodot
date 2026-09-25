"""Import a Blender-made dressing prop pack into /Game/KillGodot/Env/Dress/<Pack> with the house materials.

Inputs (made by Tools/Blender/kg_make_dress_<pack>.py + Tools/Blender/kg_sanitize_glb.py):
  Art/Packed/<GLB>.glb    sanitised glb, one object per prop named SM_KG_<Name>, vertex colour RGB = LINEAR albedo
  Art/Packed/<GLB>.json   manifest {"props": {"<Name>": {"material": "VC"|"Glow"|"Sway"|"Fish",
                                                          "collision": "complex"|"box"|"none"}}}
     VC    -> M_KG_PropVCLinear (plain)            Glow -> M_KG_JapanGlow (alpha = emissive mask)
     Sway  -> M_KG_JapanFoliage (alpha = wind sway mask: leaves, cloth, flags, laundry, bunting; two-sided)
     Fish  -> M_KG_Fish (alpha = tail wiggle)
     complex -> complex-as-simple (walkable/irregular: piers, ruins, wrecks)  box -> one box  none -> no collision

Live (editor open), new/replaced assets only:
  python Tools/Unreal/kg_remote.py --timeout 900 -c "GLB='KG_DressHarbour_Clean'; exec(open('D:/Kill Godot/Tools/Unreal/kg_import_dress_pack.py').read())"
Headless (editor closed):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_import_dress_pack.py KG_DressTerrace_Clean" -unattended -nosplash -nopause -nullrhi
Meshes land at /Game/KillGodot/Env/Dress/<GLB>/StaticMeshes/SM_KG_<Name> (the Interchange folder layout).
If a re-import ever fails half-way, do NOT retry into the same folder: bump the glb name (e.g. ..._Clean2).
"""
import json
import sys

import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAT = "/Game/KillGodot/Materials"
glb = globals().get("GLB") or next((a for a in sys.argv[1:] if a and not a.startswith("-")), None)
assert glb, "set GLB='<glb file name without extension>' before exec(), or pass it as the script argument"
SRC = f"D:/Kill Godot/Art/Packed/{glb}.glb"
manifest = json.load(open(f"D:/Kill Godot/Art/Packed/{glb}.json"))["props"]
DEST = "/Game/KillGodot/Env/Dress"
MESHES = f"{DEST}/{glb}/StaticMeshes"

t = unreal.AssetImportTask()
t.set_editor_property("filename", SRC)
t.set_editor_property("destination_path", DEST)
t.set_editor_property("automated", True)
t.set_editor_property("replace_existing", True)
t.set_editor_property("save", True)
tools.import_asset_tasks([t])

mats = {"VC": f"{MAT}/M_KG_PropVCLinear", "Glow": f"{MAT}/M_KG_JapanGlow", "Sway": f"{MAT}/M_KG_JapanFoliage",
        "Fish": f"{MAT}/M_KG_Fish"}
mats = {k: unreal.load_asset(v) for k, v in mats.items()}
ok, bad = [], []
for name, spec in manifest.items():
    path = f"{MESHES}/SM_KG_{name}"
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not m:
        bad.append(name)
        continue
    mat = mats.get(spec.get("material", "VC")) or mats["VC"]
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
print(f"KG_DRESS_PACK {glb}: {len(ok)} ok -> {MESHES}; missing: {bad}")
