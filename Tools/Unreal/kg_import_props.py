"""Import first-person melee props (poly.pizza CC0) as static meshes.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_props.py"
"""
import glob
import os

import unreal

SRC = "D:/Kill Godot/Art/Source/PolyPizza_Melee"
DEST = "/Game/KillGodot/Items/Melee"
tools = unreal.AssetToolsHelpers.get_asset_tools()

for path in sorted(glob.glob(f"{SRC}/*.glb")):
    base = os.path.splitext(os.path.basename(path))[0].rsplit("_", 1)[0]
    name = "SM_KG_" + base.replace("__", "_")
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    imported = [str(p) for p in t.get_editor_property("imported_object_paths")]
    meshes = [p for p in imported if unreal.load_asset(p).__class__.__name__ == "StaticMesh"]
    for p in meshes:
        m = unreal.load_asset(p)
        unreal.log(f"KG_PROP: {p} extent={m.get_bounds().box_extent}")
