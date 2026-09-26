"""Headless dressing of L_Morrowmere_v2 (plan section 11.3): runs Tools/Unreal/dressing/v2/dress_<zone>.py.

UE commandlet (the editor may stay closed; nothing opens a window):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_dress_v2.py [zone zone ...]" -unattended -nosplash -nopause -nullrhi
  or: powershell -File Tools/Unreal/kg_build_v2_all.ps1 -From 5 -To 5      (step 5 = dressing)
  1. loads L_Morrowmere_v2, creates the v2 helper assets once (kg_dress_v2_assets: window glow cards, lighthouse
     beam cone, brook water, slate roof texture),
  2. dumps the builder's prop footprints (Saved/KG_V2_Existing.json) and dresses the zones in plan order
     (square, harbour, streets, church, japan, countryside, coast, wilds, beckside, then the town-wide "town" pass),
  3. level fix-ups (brook water material), the minimap (kg_make_minimap with layout_v2), saves the level,
  4. writes Saved/KG_V2_DressReport.json (per-zone stats + every placement) for Tools/Level/verify_v2_build.py.
  Zones given on the command line are re-dressed alone (their Dress/<Zone> folder is cleared first).

Dry run (system Python, seconds, no UE):
  python Tools/Unreal/kg_dress_v2.py --dry [zone ...]   -> Saved/KG_V2_DressPlan.json + Saved/KG_V2_DressPlan_<area>.png
  Same module code with a recording backend: use it to lay out vignettes and check clearances before a UE run.
"""
import importlib
import importlib.util
import json
import os
import sys
import time
import traceback

HERE = "D:/Kill Godot/Tools/Unreal"
sys.path.insert(0, HERE)
ROOT = "D:/Kill Godot"
REPORT = f"{ROOT}/Saved/KG_V2_DressReport.json"
PLAN = f"{ROOT}/Saved/KG_V2_DressPlan.json"
ORDER = ["square", "tabletop", "harbour", "streets", "church", "japan", "countryside", "coast", "wilds", "beckside", "town"]

try:
    import unreal
    UE = True
except ImportError:
    UE = False


def _args():
    a = [x for x in sys.argv[1:] if x and not x.startswith("-")]
    return [z for z in a if z in ORDER] or list(ORDER)


def _load(zone):
    path = os.path.join(HERE, "dressing", "v2", f"dress_{zone}.py")
    spec = importlib.util.spec_from_file_location(f"kg_dress_v2_zone_{zone}", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def dress_zones(C, names):
    report = {}
    for zone in names:
        if not os.path.exists(os.path.join(HERE, "dressing", "v2", f"dress_{zone}.py")):
            continue
        t0 = time.time()
        removed = C.begin(zone)
        try:
            mod = _load(zone)
            mod.dress()
            st = dict(C.finish())
        except Exception:
            st = dict(C.stats)
            st["error"] = traceback.format_exc()
        if "missing" in st:
            st["missing"] = sorted(st["missing"])
        st["cleared"] = removed
        st["secs"] = round(time.time() - t0, 1)
        report[zone] = st
        C.log(f"{zone}: " + json.dumps({k: v for k, v in st.items() if k not in ("blocked",)})[:1500])
    return report


def run_ue(names):
    import kg_dress_common_v2 as C
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(C.LEVEL)
    t0 = time.time()
    try:
        import kg_dress_v2_assets
        importlib.reload(kg_dress_v2_assets)
        C.log(f"assets: {kg_dress_v2_assets.ensure()}")
    except Exception:
        C.log(f"assets FAILED:\n{traceback.format_exc()}")
    C.log(f"existing builder props: {C.dump_existing()}")
    report = dress_zones(C, names)
    try:
        import kg_dress_v2_assets
        C.log(f"level fixups: {kg_dress_v2_assets.level_fixups()}")
        forest = [C.mp(k) for k in C.PL["hism"]["forest"]] + [C.mp(k) for k in C.PL["hism"]["crops"]]
        C.log(f"ISM usage flag set on: {kg_dress_v2_assets.fix_ism_usage(list(C.ISM_MESHES) + forest)}")
    except Exception:
        C.log(f"level fixups FAILED:\n{traceback.format_exc()}")
    try:
        if os.environ.get("KG_DRESS_FAST") == "1":
            raise RuntimeError("KG_DRESS_FAST=1: minimap skipped")
        import kg_make_minimap
        importlib.reload(kg_make_minimap)
        kg_make_minimap.build_minimap(C.LAYOUT_FILE)
        C.log("minimap rebuilt")
    except Exception:
        C.log(f"minimap FAILED:\n{traceback.format_exc()}")
    saved = les.save_current_level()
    old = {}
    if set(names) != set(ORDER) and os.path.exists(REPORT):
        try:
            old = json.load(open(REPORT))
        except ValueError:
            old = {}
    zones = dict(old.get("zones", {}))
    zones.update(report)
    rec = [r for r in old.get("record", []) if r["zone"] not in report] + C.RECORD
    json.dump({"level": C.LEVEL, "secs": round(time.time() - t0, 1), "zones": zones, "record": rec}, open(REPORT, "w"))
    C.log(f"saved {C.LEVEL}: {saved}; report -> {REPORT} ({len(rec)} placements) in {time.time() - t0:.0f}s")


def run_dry(names):
    import kg_dress_common_v2 as C
    report = dress_zones(C, names)
    json.dump({"zones": report, "record": C.RECORD}, open(PLAN, "w"))
    tot = {k: sum(z.get(k, 0) for z in report.values() if isinstance(z.get(k, 0), int)) for k in
           ("props", "instances", "lights", "movers", "seats", "breakables", "chests")}
    print(f"KG_DRESS_V2 dry: {tot}")
    for z, st in report.items():
        print(f"  {z}: " + ", ".join(f"{k}={st[k]}" for k in ("props", "instances", "lights", "movers", "seats",
                                                             "breakables", "chests") if k in st)
              + (f"  ERROR {st['error'][-600:]}" if "error" in st else "")
              + (f"  blocked {len(st.get('blocked', []))}" if st.get("blocked") else "")
              + (f"  WARN {st['warn']}" if st.get("warn") else ""))
    try:
        sys.path.insert(0, f"{ROOT}/Tools/Level")
        import dress_plan_plot
        dress_plan_plot.plot(PLAN, names)
    except Exception:
        traceback.print_exc()


if UE:
    run_ue(_args())
elif __name__ == "__main__":
    run_dry(_args())
