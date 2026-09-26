"""Import the placeholder bark voices (Art/Audio/Barks/*.wav from Tools/Audio/kg_synth_barks.py) into
/Game/KillGodot/Audio/Barks as sound waves (SPRINT-023). Headless (editor closed):

  & "D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_barks.py" -unattended -nosplash -nullrhi

or live: python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_barks.py --timeout 600
"""
import glob
import os

import unreal

SRC = "D:/Kill Godot/Art/Audio/Barks"
DEST = "/Game/KillGodot/Audio/Barks"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for path in sorted(glob.glob(os.path.join(SRC, "S_Bark_*.wav"))):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
tools.import_asset_tasks(tasks)

n = 0
for path in eal.list_assets(DEST, False, False):
    w = unreal.load_asset(path)
    if isinstance(w, unreal.SoundWave):
        w.set_editor_property("looping", False)
        w.set_editor_property("sound_group", unreal.SoundGroup.SOUNDGROUP_VOICE)
        eal.save_asset(path, only_if_is_dirty=True)
        n += 1
unreal.log(f"KG_BARKS imported {len(tasks)} wavs, {n} sound waves in {DEST}")
