"""Headless wrapper for kg_asset_catalog.py (which is written for the live editor): scan the asset registry first,
then run the catalogue writer.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_asset_catalog_cmd.py" -unattended -nosplash -nopause -nullrhi
"""
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/Game/KillGodot"], force_rescan=True)
reg.wait_for_completion()
exec(open("D:/Kill Godot/Tools/Unreal/kg_asset_catalog.py").read())
unreal.log("KG_CATALOG_CMD done")
