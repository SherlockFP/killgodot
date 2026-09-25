"""Import the Morrowmere v2 terrain (Art/Packed/KG_Terrain_v2.glb, made by Tools/Blender/kg_build_terrain_v2.py).
Headless (editor closed or open on another map):

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_terrain_v2.py" -unattended -nosplash -nopause

Meshes land at /Game/KillGodot/Env/Terrain/KG_Terrain_v2/StaticMeshes/SM_KG_TerrainV2_{Core_00..11, Outer}. The v1
terrain (KG_Terrain) is untouched. Same look as v1: the tuned MI_KG_Terrain (vertex colour) material, complex-as-
simple collision, Nanite off (full-res render + collision). Optionally also imports the KG_DressHarbour_Clean prop
pack (bollards, sea stacks, crane, bell buoy...) when it is missing: pass "harbour" as a script argument.
"""
import json
import sys

import unreal

DEST = "/Game/KillGodot/Env/Terrain"
MESHES = f"{DEST}/KG_Terrain_v2/StaticMeshes"
MAT = "/Game/KillGodot/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"KG_TERRAIN_V2: {msg}")
    print(f"KG_TERRAIN_V2: {msg}")


def import_terrain():
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_Terrain_v2.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    tm = unreal.load_asset(f"{MAT}/MI_KG_Terrain") or unreal.load_asset(f"{MAT}/M_KG_Terrain")
    paths = [str(p) for p in t.get_editor_property("imported_object_paths")]
    log(f"imported {len(paths)} objects")
    n = 0
    for p in paths:
        mesh = unreal.load_asset(p)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        mesh.set_material(0, tm)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        ns = mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", ns)
        eal.save_loaded_asset(mesh)
        n += 1
        log(f"{mesh.get_path_name()} bounds {mesh.get_bounds().box_extent}")
    # Stray non-mesh assets the glb importer may have made (default materials) are harmless; list them.
    for a in eal.list_assets(f"{DEST}/KG_Terrain_v2", recursive=True):
        log(f"asset {a}")
    return n


def import_harbour_pack():
    glb = "KG_DressHarbour_Clean"
    path = f"/Game/KillGodot/Env/Dress/{glb}/StaticMeshes/SM_KG_Bollard"
    if eal.does_asset_exist(path):
        log("harbour pack already imported")
        return
    g = {"GLB": glb, "__name__": "kg_import_dress_pack"}
    exec(compile(open("D:/Kill Godot/Tools/Unreal/kg_import_dress_pack.py", encoding="utf-8").read(),
                 "kg_import_dress_pack", "exec"), g)
    log("harbour pack imported")


args = [a.lower() for a in sys.argv[1:]]
if "harbour_only" not in args:
    log(f"terrain meshes: {import_terrain()}")
if "harbour" in args or "harbour_only" in args:
    import_harbour_pack()
