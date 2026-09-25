"""Import the role reveal art (Tools/UI/kg_make_reveal_art.py -> Art/UI/Reveal) as UI textures (SPRINT-037).

  /Game/KillGodot/UI/Reveal/T_KG_Reveal_Town | _Impatient | _Neutral   alignment illustrations, 512 px
  /Game/KillGodot/UI/Reveal/T_KG_Reveal_Mate0..2                        accomplice busts (grey, tinted in Slate)

Settings: UserInterface2D (uncompressed RGBA), no mips, TEXTUREGROUP_UI, sRGB, never stream.
Headless (the editor may stay open, these are new asset paths):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_reveal_art.py" -unattended -nullrhi -nosplash
The C++ (Source/KillGodot/UI/Reveal/SKGRoleReveal.cpp) loads them by path and falls back to painted emblems.
"""
import os

import unreal

DEST = "/Game/KillGodot/UI/Reveal"
NAMES = ("T_KG_Reveal_Town", "T_KG_Reveal_Impatient", "T_KG_Reveal_Neutral",
         "T_KG_Reveal_Mate0", "T_KG_Reveal_Mate1", "T_KG_Reveal_Mate2")


def main():
    src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "UI", "Reveal")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in NAMES:
        path = os.path.join(src, name + ".png")
        if not os.path.exists(path):
            unreal.log_error(f"kg_import_reveal_art: missing {path} (run Tools/UI/kg_make_reveal_art.py)")
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        tex = unreal.load_asset(f"{DEST}/{name}")
        if not tex:
            unreal.log_error(f"kg_import_reveal_art: import failed for {name}")
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
        unreal.log(f"KG_REVEAL_ART imported {DEST}/{name} {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")


main()
