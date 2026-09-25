"""Fishing assets in one headless run (editor closed): the synthesised fishing sounds, the first-person rod clips and the
third-person upper-body fishing clips. Each step is the normal importer with an --only filter.

  python Tools/Audio/kg_synth_sfx.py Art/Audio S_Fish_,S_FishReel
  blender --background --factory-startup --python Tools/Blender/kg_make_fp_arms2.py -- --only rod_idle,rod_windup,rod_windup_in,rod_cast,rod_reel,rod_hookset
  blender --background --factory-startup --python Tools/Blender/kg_make_emotes.py -- --only Fish_Hold,Fish_Cast,Fish_Reel --preview
  "D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/Kill Godot/KillGodot.uproject"
      -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_fishing_assets.py" -unattended -nullrhi -nosplash -stdout
"""
import sys

import unreal

HERE = "D:/Kill Godot/Tools/Unreal/"
STEPS = [
    ("kg_import_audio.py", "S_Fish_,S_FishReel"),
    ("kg_import_fp_arms2.py", "rod_idle,rod_windup,rod_windup_in,rod_cast,rod_reel,rod_hookset"),
    ("kg_import_emotes.py", "Fish_Hold,Fish_Cast,Fish_Reel"),
]

for script, only in STEPS:
    unreal.log(f"KG_FISH_IMPORT step {script} --only {only}")
    sys.argv = [HERE + script, "--only", only]
    code = compile(open(HERE + script, encoding="utf-8").read(), HERE + script, "exec")
    exec(code, {"__name__": "kg_fish_step", "__file__": HERE + script})
unreal.log("KG_FISH_IMPORT done")
