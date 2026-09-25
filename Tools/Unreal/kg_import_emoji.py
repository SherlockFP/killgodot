"""Import the chat emoji atlases (Tools/UI/kg_make_emoji.py -> Art/UI/Emoji) as UI textures.

  /Game/KillGodot/UI/Emoji/T_KG_EmojiAtlas        8x4 cells of 160 px (bubbles, picker, wheel)
  /Game/KillGodot/UI/Emoji/T_KG_EmojiAtlas_Small  8x4 cells of 48 px  (inline chat text)

Settings: UserInterface2D (uncompressed RGBA), no mips, TEXTUREGROUP_UI, sRGB, never stream.
Runs headless (editor closed) or live:
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_emoji.py" -unattended -nullrhi -nosplash
  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_emoji.py
Re-running replaces the textures in place (the C++ loads them by path, nothing else references them).
"""
import os

import unreal

DEST = "/Game/KillGodot/UI/Emoji"
NAMES = ("T_KG_EmojiAtlas", "T_KG_EmojiAtlas_Small")


def main():
    src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "UI", "Emoji")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in NAMES:
        path = os.path.join(src, name + ".png")
        if not os.path.exists(path):
            unreal.log_error(f"kg_import_emoji: missing {path} (run Tools/UI/kg_make_emoji.py)")
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
            unreal.log_error(f"kg_import_emoji: import failed for {name}")
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
        unreal.log(f"KG_EMOJI imported {DEST}/{name} {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")


main()
