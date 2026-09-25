"""Import the Quaternius villagers + UAL1 animations (Tools/Blender/kg_build_villagers.py) onto one shared skeleton.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_villagers.py"
"""
import unreal

SRC = "D:/Kill Godot/Art/Export/Villagers"
DEST = "/Game/KillGodot/Characters/Villager"
ANIM_DEST = "/Game/KillGodot/Characters/Villager/Anims"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"KG_IMPORT: {msg}")


def task(fbx, dest, name, ui):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f"{SRC}/{fbx}")
    t.set_editor_property("destination_path", dest)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", ui)
    tools.import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths"))


def mesh_ui(skeleton=None):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", True)
    if skeleton:
        ui.set_editor_property("skeleton", skeleton)
    return ui


def anim_ui(skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skeleton)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    # Armature-only FBX comes in metres while the skinned meshes land in cm: scale the tracks to match.
    ui.get_editor_property("anim_sequence_import_data").set_editor_property("import_uniform_scale", 100.0)
    return ui


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    task("SK_KG_Villager_M.fbx", DEST, "SK_KG_Villager_M", mesh_ui())
    male = unreal.load_asset(f"{DEST}/SK_KG_Villager_M")
    skeleton = male.get_editor_property("skeleton")
    log(f"male bounds={male.get_bounds().box_extent} skeleton={skeleton.get_path_name()}")
    task("SK_KG_Villager_F.fbx", DEST, "SK_KG_Villager_F", mesh_ui(skeleton))
    for fp in ("SK_KG_FPArms_M", "SK_KG_FPArms_F"):
        task(f"{fp}.fbx", DEST, fp, mesh_ui(skeleton))
    import os
    anim_dir = f"{SRC}/Anims"
    report = []
    for fbx in sorted(os.listdir(anim_dir)):
        if not fbx.lower().endswith(".fbx"):
            continue
        name = os.path.splitext(fbx)[0]
        task(f"Anims/{fbx}", ANIM_DEST, name, anim_ui(skeleton))
        seq = unreal.load_asset(f"{ANIM_DEST}/{name}")
        if isinstance(seq, unreal.AnimSequence):
            report.append(f"{name}:{seq.get_play_length():.2f}s")
        else:
            report.append(f"{name}:MISSING")
    log(f"animations ({len(report)}): {report}")
    # Skeleton, physics asset, materials and textures are side products of the import: save them explicitly.
    eal.save_directory("/Game/KillGodot/Characters/Villager", only_if_is_dirty=False, recursive=True)
    male = unreal.load_asset(f"{DEST}/SK_KG_Villager_M")
    mats = [str(m.get_editor_property("material_interface").get_name()) if m.get_editor_property("material_interface") else "None"
            for m in male.get_editor_property("materials")]
    log(f"saved; male materials={mats}")


main()
