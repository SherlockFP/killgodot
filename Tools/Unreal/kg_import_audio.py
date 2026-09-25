"""Import the synthesised sounds (Art/Audio/*.wav from Tools/Audio/kg_synth_sfx.py) into /Game/KillGodot/Audio,
mark *_Loop waves as looping and create a shared ambience attenuation. Live-safe (creates/updates sound assets).

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_audio.py --timeout 600
"""
import glob
import os
import sys

import unreal

# --only S_Fish_,S_FishReel (commandlet -Script args): only the WAVs / sound waves whose names start with these prefixes.
ONLY = []
if "--only" in sys.argv and sys.argv.index("--only") + 1 < len(sys.argv):
    ONLY = [p for p in sys.argv[sys.argv.index("--only") + 1].split(",") if p]

DEST = "/Game/KillGodot/Audio"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for path in sorted(glob.glob("D:/Kill Godot/Art/Audio/*.wav")):
    if ONLY and not any(os.path.basename(path).startswith(p) for p in ONLY):
        continue
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
    if ONLY and not any(path.rsplit("/", 1)[-1].startswith(p) for p in ONLY):
        continue
    w = unreal.load_asset(path)
    if isinstance(w, unreal.SoundWave):
        w.set_editor_property("looping", w.get_name().endswith("_Loop"))
        eal.save_loaded_asset(w)
        n += 1

att_path = f"{DEST}/SA_KG_Ambience"
if not eal.does_asset_exist(att_path):
    att = tools.create_asset("SA_KG_Ambience", DEST, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = att.get_editor_property("attenuation")
    s.set_editor_property("falloff_distance", 3500.0)
    shape = s.get_editor_property("attenuation_shape_extents")
    s.set_editor_property("attenuation_shape_extents", unreal.Vector(900.0, 0.0, 0.0))
    att.set_editor_property("attenuation", s)
    eal.save_loaded_asset(att)
print(f"KG_AUDIO {n} sound waves")
