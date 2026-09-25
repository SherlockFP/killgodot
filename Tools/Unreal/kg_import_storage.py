"""Storage chest mesh for AKGStorageChest: the Quaternius Fantasy Props MegaKit "Chest_Wood" (CC0), imported as one
STATIC mesh (the source is rigged: lid on a bone, baked closed) into a NEW path. Headless, never touches maps or
existing assets:

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_storage.py"

Result: /Game/KillGodot/Items/Storage/SM_KG_Chest_Wood (AKGStorageChest picks it up automatically when no pirate or
kit chest exists). Folder names contain "[Standard]": paths are built with os.path, never glob.
"""
import os

import unreal

SRC = os.path.join("D:/Kill Godot/Art/Source", "Quaternius_FantasyPropsMegaKit", "Fantasy Props MegaKit[Standard]",
                   "Exports", "glTF", "Chest_Wood.gltf")
DEST = "/Game/KillGodot/Items/Storage"
NAME = "SM_KG_Chest_Wood"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"KG_STORAGE: {msg}")


def main():
    if not os.path.isfile(SRC):
        log(f"source missing: {SRC}")
        return
    pipe = unreal.InterchangeGenericAssetsPipeline()
    pipe.set_editor_property("asset_name", NAME)
    pipe.get_editor_property("common_meshes_properties").set_editor_property(
        "force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    mesh = pipe.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mesh.set_editor_property("build_nanite", False)
    mesh.set_editor_property("collision", True)
    pipe.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(pipe)

    t = unreal.AssetImportTask()
    t.set_editor_property("filename", SRC.replace("\\", "/"))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", stack)
    tools.import_asset_tasks([t])
    paths = [str(p) for p in t.get_editor_property("imported_object_paths")]
    log(f"imported {paths}")

    meshes = [p for p in eal.list_assets(DEST, recursive=True, include_folder=False)
              if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    for p in meshes:
        sm = unreal.load_asset(p)
        # Box collision so the chest blocks pawns and the E trace like the other props.
        unreal.EditorStaticMeshLibrary.remove_collisions(sm)
        unreal.EditorStaticMeshLibrary.add_simple_collisions(sm, unreal.ScriptCollisionShapeType.BOX)
        eal.save_loaded_asset(sm)
        bb = sm.get_bounding_box()
        log(f"RESULT {sm.get_path_name()} min=({bb.min.x:.1f}, {bb.min.y:.1f}, {bb.min.z:.1f}) "
            f"max=({bb.max.x:.1f}, {bb.max.y:.1f}, {bb.max.z:.1f})")
    eal.save_directory(DEST, only_if_is_dirty=True, recursive=True)


main()
