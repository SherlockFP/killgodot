"""Import the packed environment kits (Art/Packed/KG_*.glb from Tools/Blender/kg_pack_kit.py) as static meshes and
set collision per kit. Headless (safe while the user's editor is open; the content browser picks the files up):

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_env.py"
"""
import unreal

PACKED = "D:/Kill Godot/Art/Packed"
KITS = {"KG_Village": "/Game/KillGodot/Env", "KG_Nature": "/Game/KillGodot/Env", "KG_Props": "/Game/KillGodot/Env"}
# Tiny decoration: no collision at all (players walk through grass and petals).
NO_COLLISION = ("Grass_", "Flower_", "Clover_", "Petal_", "Fern_", "Plant_", "Mushroom_", "Pebble_", "Prop_Vine",
                "Bush_")
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_kit(name, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f"{PACKED}/{name}.glb")
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


def setup_collision(path, kit):
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        return None
    name = mesh.get_name()
    body = mesh.get_editor_property("body_setup")
    if body is None:
        return name
    if name.startswith(NO_COLLISION):
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    elif kit == "KG_Props":
        # Props get a box so they can be grabbed/thrown later.
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
        unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    else:
        # Walls with door holes, stairs, terrain-like rocks: collide with the real triangles.
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh)
    return name


for kit, dest in KITS.items():
    paths = import_kit(kit, dest)
    names = [n for n in (setup_collision(p, kit) for p in paths) if n]
    unreal.log(f"KG_ENV: {kit} -> {len(names)} static meshes ({len(paths)} assets) under {dest}")
