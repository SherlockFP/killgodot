"""Zone dressing runner: fills L_Morrowmere with the content modules in Tools/Unreal/dressing/dress_<zone>.py.

Live (editor open, PIE stopped), one or more zones, then saves the level:
  python Tools/Unreal/kg_remote.py --timeout 900 -c "import sys; sys.path.insert(0, 'D:/Kill Godot/Tools/Unreal'); import kg_dress; kg_dress.run(['harbour'])"
All zones:     kg_dress.run()
From the builder (kg_build_village.py): kg_dress.run(save=False, nav=False, restore=False) before grass/nav.

Each module is loaded fresh from its file on every run (no stale imports) and must define dress() -> None.
The zone's outliner folder "Dress/<Zone>" is emptied first, so runs are idempotent.
"""
import glob
import importlib
import importlib.util
import os
import sys
import time
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "D:/Kill Godot/Tools/Unreal"
sys.path.insert(0, HERE)
import kg_dress_common as C  # noqa: E402

DIR = os.path.join(HERE, "dressing")


DIR_V2 = os.path.join(DIR, "v2")
V2_LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
# Plan order (Docs/Level/Morrowmere_v2_Plan.md section 11.3): square, harbour, streets, church, japan, countryside,
# coast, wilds, beckside. Later zones claim around what earlier ones placed only through the shared layout rules.
V2_ORDER = ["square", "harbour", "streets", "church", "japan", "countryside", "coast", "wilds", "beckside"]


def is_v2(level=None):
    """v2 when asked for explicitly (level="v2" or the v2 map path), else when the open editor level is the v2 map."""
    if level is not None:
        return str(level).lower() in ("v2", "2", V2_LEVEL.lower(), "l_morrowmere_v2")
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        return bool(world) and world.get_path_name().split(".")[0] == V2_LEVEL
    except Exception:
        return False


def zones(v2=False):
    if v2:
        have = {os.path.basename(p)[len("dress_"):-3] for p in glob.glob(os.path.join(DIR_V2, "dress_*.py"))}
        return [z for z in V2_ORDER if z in have] + sorted(have - set(V2_ORDER))
    return sorted(os.path.basename(p)[len("dress_"):-3] for p in glob.glob(os.path.join(DIR, "dress_*.py")))


def _load(zone, v2=False):
    path = os.path.join(DIR_V2 if v2 else DIR, f"dress_{zone}.py")
    spec = importlib.util.spec_from_file_location(f"kg_dress_{'v2_' if v2 else ''}zone_{zone}", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _ensure_level(level=None):
    """Dress only L_Morrowmere (or L_Morrowmere_v2). If another level is open, save it and come back later."""
    level = level or C.LEVEL
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    current = world.get_path_name().split(".")[0] if world else ""
    if current != level:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if current and not current.startswith("/Temp/"):
            les.save_current_level()
        les.load_level(level)
    return current


def run(names=None, save=True, nav=True, restore=True, level=None):
    """Dress the given zones (default: all). level: None = v1 unless the v2 map is open; "v2" = L_Morrowmere_v2
    with kg_dress_common_v2 and dressing/v2/ (see kg_dress_v2.py for the headless runner). Returns {zone: stats}."""
    global C
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        print("KG_DRESS: PIE is running - stop it first (the dressing goes into the editor world)")
        return {}
    v2 = is_v2(level)
    if v2:
        import kg_dress_common_v2
        C = importlib.reload(kg_dress_common_v2)
    else:
        import kg_dress_common
        C = importlib.reload(kg_dress_common)
    previous = _ensure_level(V2_LEVEL if v2 else None)
    if v2:
        C.log(f"existing builder props: {C.dump_existing()}")
    report = {}
    for zone in names or zones(v2):
        t0 = time.time()
        removed = C.begin(zone)
        try:
            mod = _load(zone, v2)
            mod.dress()
            if v2:
                C.finish()
            st = dict(C.stats)
            if "missing" in st:
                st["missing"] = sorted(st["missing"])
            st["cleared"] = removed
            st["secs"] = round(time.time() - t0, 1)
        except Exception:
            st = {"error": traceback.format_exc()}
        report[zone] = st
        print(f"KG_DRESS {zone}: {st}")
    if nav:
        unreal.SystemLibrary.execute_console_command(
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")
    if save:
        les.save_current_level()
    if restore and previous and previous != C.LEVEL and not previous.startswith("/Temp/"):
        les.load_level(previous)
    return report
