"""SPRINT-022 test build helper: dress a COPY of the v2 map (KG_V2_LEVEL, e.g. /Game/KillGodot/Maps/Test/L_Morrowmere_v2_S22)
with the real zone modules, without the shared-asset fix-ups of kg_dress_v2.py (slate MI, ISM usage flags, minimap),
so it is safe while the editor has L_Morrowmere_v2 open. Writes Saved/KG_V2_DressReport.json like the real runner.
  set KG_V2_LEVEL=/Game/KillGodot/Maps/Test/L_Morrowmere_v2_S22
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_s22_testdress.py"
"""
import importlib.util
import json
import os
import sys
import time
import traceback

import unreal

HERE = "D:/Kill Godot/Tools/Unreal"
sys.path.insert(0, HERE)
ORDER = ["square", "harbour", "streets", "church", "japan", "countryside", "coast", "wilds", "beckside", "town"]
import kg_dress_common_v2 as C  # noqa: E402

assert "/Test/" in C.LEVEL, f"test dressing only runs on a Test/ copy, not {C.LEVEL}"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(C.LEVEL)
t0 = time.time()
C.log(f"existing builder props: {C.dump_existing()}")
report = {}
for zone in ORDER:
    t = time.time()
    removed = C.begin(zone)
    try:
        spec = importlib.util.spec_from_file_location(f"kg_dress_v2_zone_{zone}", os.path.join(HERE, "dressing", "v2", f"dress_{zone}.py"))
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        mod.dress()
        st = dict(C.finish())
    except Exception:
        st = dict(C.stats)
        st["error"] = traceback.format_exc()
    if "missing" in st:
        st["missing"] = sorted(st["missing"])
    st["cleared"] = removed
    st["secs"] = round(time.time() - t, 1)
    report[zone] = st
    C.log(f"{zone}: " + json.dumps({k: v for k, v in st.items() if k != "blocked"})[:600])
saved = les.save_current_level()
json.dump({"level": C.LEVEL, "secs": round(time.time() - t0, 1), "zones": report, "record": C.RECORD},
          open("D:/Kill Godot/Saved/KG_V2_DressReport.json", "w"))
C.log(f"test dress saved {C.LEVEL}: {saved} in {time.time() - t0:.0f}s")
