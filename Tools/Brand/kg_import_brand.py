"""Import the KillGo brand UI textures (Tools/Brand/kg_brand_keyart.py + kg_brand_logo.py -> Art/Brand) into
/Game/KillGodot/UI/Brand.

  T_KG_Load_Harbour / _Heart / _Crown / _Sakura   1920x1080 loading screens (no text; lines in Docs/Lore/KillGo_Lore.md 5.6)
  T_KG_Logo                                       2048 px wordmark, transparent

Settings: UserInterface2D (TC_EDITOR_ICON), no mips, TEXTUREGROUP_UI, sRGB, never stream.
Headless (no window):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -script="D:/Kill Godot/Tools/Brand/kg_import_brand.py" -unattended -nullrhi -nosplash -nopause
Re-running replaces the textures in place (new folder; nothing references them yet).
"""
import os

import unreal

DEST = "/Game/KillGodot/UI/Brand"
ITEMS = [
    ("T_KG_Load_Harbour", ("Art", "Brand", "Loading", "T_KG_Load_Harbour.png")),
    ("T_KG_Load_Heart", ("Art", "Brand", "Loading", "T_KG_Load_Heart.png")),
    ("T_KG_Load_Crown", ("Art", "Brand", "Loading", "T_KG_Load_Crown.png")),
    ("T_KG_Load_Sakura", ("Art", "Brand", "Loading", "T_KG_Load_Sakura.png")),
    ("T_KG_Logo", ("Art", "Brand", "KillGo_Logo.png")),
]


def main():
    root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    ok = 0
    for name, rel in ITEMS:
        path = os.path.join(root, *rel)
        if not os.path.exists(path):
            unreal.log_error(f"KG_BRAND_IMPORT missing {path} (run Tools/Brand/kg_brand_keyart.py)")
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
            unreal.log_error(f"KG_BRAND_IMPORT import failed for {name}")
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
        ok += 1
        unreal.log(f"KG_BRAND_IMPORT {DEST}/{name} {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()} "
                   f"comp={tex.get_editor_property('compression_settings')} mips={tex.get_editor_property('mip_gen_settings')}")
    unreal.log(f"KG_BRAND_IMPORT done {ok}/{len(ITEMS)}")


main()
