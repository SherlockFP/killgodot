"""Digging + underground assets in one headless run (editor closed). Each step is the normal importer with a filter.

  python Tools/Audio/kg_synth_sfx_dig.py Art/Audio
  blender --background --factory-startup --python Tools/Blender/kg_make_dress_underground.py
  blender --background --factory-startup --python Tools/Blender/kg_make_fp_arms2.py -- --only shovel_idle,shovel_dig,shovel_draw
  "D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/Kill Godot/KillGodot.uproject"
      -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_dig_assets.py [audio,pack,arms]" -unattended -nullrhi -nosplash
Default: every step. Then Tools/Unreal/kg_build_underground.py puts the underground into L_Morrowmere_v2.
"""
import sys

import unreal

HERE = "D:/Kill Godot/Tools/Unreal/"
STEPS = {
    "audio": ("kg_import_audio.py", ["--only", "S_Dig_,S_Cave_,S_Gate_,S_Passage_"]),
    "pack": ("kg_import_dress_pack.py", ["KG_DressUnder_Clean"]),
    "arms": ("kg_import_fp_arms2.py", ["--only", "shovel_idle,shovel_dig,shovel_draw"]),
}
want = next((a for a in sys.argv[1:] if a and not a.startswith("-")), "audio,pack,arms").split(",")

for key in want:
    script, args = STEPS[key]
    unreal.log(f"KG_DIG_IMPORT step {key}: {script} {' '.join(args)}")
    sys.argv = [HERE + script] + args
    code = compile(open(HERE + script, encoding="utf-8").read(), HERE + script, "exec")
    exec(code, {"__name__": "kg_dig_step", "__file__": HERE + script})
unreal.log("KG_DIG_IMPORT done")
