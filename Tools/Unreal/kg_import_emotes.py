"""Import the third-person emote clips (Tools/Blender/kg_make_emotes.py) onto the villager skeleton.

  UnrealEditor-Cmd "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -Script="D:/Kill Godot/Tools/Unreal/kg_import_emotes.py" -unattended -nullrhi -nosplash
  # only some clips:
  UnrealEditor-Cmd "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -Script="D:/Kill Godot/Tools/Unreal/kg_import_emotes.py --only Wave,Sit_Loop" -unattended -nullrhi -nosplash

Source: Art/Export/Villagers/Emotes/A_KG_Emote_<Name>.fbx (armature-only FBX, metres, 30 fps, in place).
Destination: /Game/KillGodot/Characters/Villager/Anims/Emotes/A_KG_Emote_<Name> on the skeleton of
/Game/KillGodot/Characters/Villager/SK_KG_Villager_M. Legacy FBX importer (same path as kg_import_villagers.py) with
import_uniform_scale 100. Logs one `KG_EMOTE_IMPORT name:length` line per clip.

Layering (for the AnimBP): "upper" clips are meant to be layered from spine_01 up over locomotion, "full" clips drive
the whole body; loops: Reel, Sit_Loop, Sus.
"""
import os
import sys

import unreal

SRC = "D:/Kill Godot/Art/Export/Villagers/Emotes"
MESH = "/Game/KillGodot/Characters/Villager/SK_KG_Villager_M"
DEST = "/Game/KillGodot/Characters/Villager/Anims/Emotes"
LOOPS = {"Reel", "Sit_Loop", "Sus", "Fish_Hold", "Fish_Reel"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(msg)
    print(msg)


def only_clips():
    """--only a,b (commandlet -Script args): import just these clips (names without the A_KG_Emote_ prefix)."""
    argv = list(sys.argv)
    if "--only" in argv and argv.index("--only") + 1 < len(argv):
        return [c for c in argv[argv.index("--only") + 1].split(",") if c]
    return []


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


def import_clip(fbx, name, skeleton):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f"{SRC}/{fbx}")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", anim_ui(skeleton))
    tools.import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths"))


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    mesh = unreal.load_asset(MESH)
    if not mesh:
        raise RuntimeError(f"KG_EMOTE_IMPORT: missing {MESH} (run kg_import_villagers.py first)")
    skeleton = mesh.get_editor_property("skeleton")
    wanted = only_clips()
    files = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".fbx") and f.startswith("A_KG_Emote_"))
    report = []
    for fbx in files:
        name = os.path.splitext(fbx)[0]
        short = name[len("A_KG_Emote_"):]
        if wanted and short not in wanted and name not in wanted:
            continue
        import_clip(fbx, name, skeleton)
        seq = unreal.load_asset(f"{DEST}/{name}")
        if not isinstance(seq, unreal.AnimSequence):
            log(f"KG_EMOTE_IMPORT {name}:MISSING")
            report.append(f"{name}:MISSING")
            continue
        if short in LOOPS:
            try:
                seq.set_editor_property("b_loop", True)
            except Exception:  # noqa: BLE001 - property name differs between engine versions
                pass
        eal.save_loaded_asset(seq)
        length = seq.get_play_length()
        log(f"KG_EMOTE_IMPORT {name}:{length:.3f}")
        report.append(f"{name}:{length:.2f}s")
    eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log(f"KG_EMOTE_IMPORT done ({len(report)}): {report} skeleton={skeleton.get_path_name()}")


main()
