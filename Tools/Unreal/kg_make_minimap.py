"""Minimap data for a village level: render the schematic PNG + regions, import the texture, place AKGMapInfo.

Village builders call it at the end of a rebuild so the map always matches the layout:

    import kg_make_minimap
    kg_make_minimap.build_minimap("D:/Kill Godot/Tools/Level/morrowmere_layout_v2.json")   # current editor level

Standalone:
    python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_make_minimap.py      (live editor, current level, layout by level name)
    UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -unattended -nullrhi -nosplash
        -Script="D:/Kill Godot/Tools/Unreal/kg_make_minimap.py --level /Game/KillGodot/Maps/L_Morrowmere_v2 --save"
    Options: --level <map> (load it first), --layout <json> (default: by level name), --no-render (reuse the PNG/JSON
    in Art/Textures/Map), --save (save the level; build_minimap(save=False) leaves that to the builder).

Pipeline:
  1. Tools/Level/render_minimap.py <layout> (system Python: numpy, Pillow, shapely, scipy; run hidden, no window)
     -> Art/Textures/Map/T_KG_Map_<Key>.png + KG_MapRegions_<Key>.json  (Key: Morrowmere, Morrowmere_v2)
  2. import -> /Game/KillGodot/UI/Map/T_KG_Map_<Key> (UI group, BC7, sRGB, clamp, never streamed; mips on because the
     minimap shows it rotated and minified - without them it shimmers)
  3. one AKGMapInfo "KG_MapInfo" in the level (reused if present): texture, world bounds (cm), regions.
"""
import glob
import json
import os
import re
import shutil
import subprocess
import sys

import unreal

ROOT = "D:/Kill Godot"
RENDER = f"{ROOT}/Tools/Level/render_minimap.py"
MAP_DIR = f"{ROOT}/Art/Textures/Map"
DEST = "/Game/KillGodot/UI/Map"
LEVEL_LAYOUTS = {
    "L_Morrowmere": f"{ROOT}/Tools/Level/morrowmere_layout.json",
    "L_Morrowmere_v2": f"{ROOT}/Tools/Level/morrowmere_layout_v2.json",
}


def log(msg):
    unreal.log(f"KG_MINIMAP: {msg}")


def map_key(layout_path):
    """Same rule as render_minimap.map_key: morrowmere_layout_v2.json -> Morrowmere_v2."""
    stem = os.path.splitext(os.path.basename(layout_path))[0]
    m = re.match(r"(\w+?)_layout(_\w+)?$", stem)
    base, suffix = (m.group(1), m.group(2) or "") if m else (stem, "")
    return base[:1].upper() + base[1:] + suffix


def _system_python():
    """A Python with numpy/Pillow/shapely/scipy (the editor's embedded one has none of them)."""
    cands = [os.environ.get("KG_PYTHON"), shutil.which("python"), shutil.which("python3")]
    cands += sorted(glob.glob(os.path.expandvars(r"%LOCALAPPDATA%\Programs\Python\Python3*\python.exe")), reverse=True)
    for exe in cands:
        if not exe or not os.path.exists(exe) or "WindowsApps" in exe:
            continue
        try:
            ok = subprocess.run([exe, "-c", "import numpy, PIL, shapely, scipy"], capture_output=True, timeout=60,
                                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)).returncode == 0
        except Exception:  # noqa: BLE001
            ok = False
        if ok:
            return exe
    return None


TREE_WORDS = ("tree", "pine", "sakura", "maple", "bamboo", "bonsai", "oak", "bush")


def _xf_of(res):
    if isinstance(res, unreal.Transform):
        return res
    if isinstance(res, tuple):
        return next((r for r in res if isinstance(r, unreal.Transform)), None)
    return None


def dump_trees(key):
    """Tree/bush positions of the current level -> Saved/Minimap/trees_<Key>.json, drawn as canopies on the map."""
    out = []
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.StaticMeshActor):
            mesh = a.static_mesh_component.static_mesh
            if mesh and any(w in mesh.get_name().lower() for w in TREE_WORDS):
                loc, sc = a.get_actor_location(), a.get_actor_scale3d()
                out.append([round(loc.x / 100.0, 2), round(loc.y / 100.0, 2), round(max(sc.x, sc.y), 2), mesh.get_name()])
            continue
        for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = comp.static_mesh
            if not mesh or not any(w in mesh.get_name().lower() for w in TREE_WORDS):
                continue
            for i in range(comp.get_instance_count()):
                xf = _xf_of(comp.get_instance_transform(i, True))
                if xf:
                    t, sc = xf.translation, xf.scale3d
                    out.append([round(t.x / 100.0, 2), round(t.y / 100.0, 2), round(max(sc.x, sc.y), 2), mesh.get_name()])
    path = f"{ROOT}/Saved/Minimap/trees_{key}.json"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(out, open(path, "w"))
    log(f"{len(out)} trees -> {path}")
    return path


def render(layout):
    exe = _system_python()
    if not exe:
        log("no system Python with numpy/Pillow/shapely/scipy: reusing the existing PNG/JSON")
        return False
    res = subprocess.run([exe, RENDER, layout, "--out", MAP_DIR], capture_output=True, text=True, timeout=900,
                         creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    for line in (res.stdout or "").splitlines():
        log(line)
    if res.returncode != 0:
        log(f"render failed ({res.returncode}): {(res.stderr or '')[-1500:]}")
        return False
    return True


def import_texture(png, key):
    name = f"T_KG_Map_{key}"
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", png)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    tex = unreal.load_asset(f"{DEST}/{name}")
    if not tex:
        raise RuntimeError(f"texture import failed: {png}")
    for prop, value in (
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("compression_settings", unreal.TextureCompressionSettings.TC_BC7),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE),
        ("srgb", True),
        ("never_stream", True),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
        ("filter", unreal.TextureFilter.TF_TRILINEAR),
    ):
        try:
            tex.set_editor_property(prop, value)
        except Exception as e:  # noqa: BLE001
            log(f"texture property {prop}: {e}")
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def _region(r):
    s = unreal.KGMapRegion()
    s.set_editor_property("id", r["id"])
    s.set_editor_property("name", unreal.Text(r["name"]))
    s.set_editor_property("kind", r.get("kind", ""))
    s.set_editor_property("layer", int(r.get("layer", 2)))
    s.set_editor_property("toast", bool(r.get("toast", True)))
    s.set_editor_property("center", unreal.Vector2D(*r["center"]))
    s.set_editor_property("radius", float(r.get("radius", 0.0)))
    s.set_editor_property("polygon", [unreal.Vector2D(x, y) for x, y in r.get("polygon", [])])
    s.set_editor_property("label_pos", unreal.Vector2D(*r.get("label", r["center"])))
    s.set_editor_property("icon", r.get("icon") or "None")
    s.set_editor_property("priority", int(r.get("priority", 50)))
    return s


def _map_info():
    cls = unreal.load_class(None, "/Script/KillGodot.KGMapInfo")
    if cls is None:
        raise RuntimeError("AKGMapInfo is not in the loaded editor module: rebuild KillGodotEditor first")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    found = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.KGMapInfo)]
    for extra in found[1:]:
        actors.destroy_actor(extra)
    if found:
        return found[0]
    info = actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0))
    info.set_actor_label("KG_MapInfo")
    info.set_folder_path("Gameplay")
    return info


def current_level_name():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return world.get_name() if world else ""


def build_minimap(layout=None, do_render=True, save=False):
    """Render + import + place/refresh AKGMapInfo in the CURRENT editor level. Returns the actor."""
    level = current_level_name()
    layout = (layout or LEVEL_LAYOUTS.get(level) or LEVEL_LAYOUTS["L_Morrowmere"]).replace("\\", "/")
    key = map_key(layout)
    if do_render:
        try:
            dump_trees(key)
        except Exception as e:  # noqa: BLE001 - the map still renders without canopies
            log(f"tree dump failed: {e}")
        render(layout)
    png = f"{MAP_DIR}/T_KG_Map_{key}.png"
    regions_json = f"{MAP_DIR}/KG_MapRegions_{key}.json"
    if not (os.path.exists(png) and os.path.exists(regions_json)):
        raise RuntimeError(f"missing {png} / {regions_json}: run python Tools/Level/render_minimap.py {layout}")
    data = json.load(open(regions_json, encoding="utf-8"))
    tex = import_texture(png, key)
    info = _map_info()
    info.set_editor_property("map_texture", tex)
    info.set_editor_property("world_min", unreal.Vector2D(*data["world_min"]))
    info.set_editor_property("world_max", unreal.Vector2D(*data["world_max"]))
    info.set_editor_property("map_title", unreal.Text(re.sub(r"_v\d+$", "", key)))
    info.set_editor_property("regions", [_region(r) for r in data["regions"]])
    log(f"{level}: {DEST}/T_KG_Map_{key}, {len(data['regions'])} regions, bounds {data['world_min']}..{data['world_max']} cm")
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        log(f"saved {level}")
    return info


def main(argv):
    args = {"--level": None, "--layout": None}
    flags = set()
    it = iter(argv)
    for a in it:
        if a in args:
            args[a] = next(it, None)
        elif a.startswith("--"):
            flags.add(a)
    if args["--level"]:
        unreal.EditorLoadingAndSavingUtils.load_map(args["--level"])
    build_minimap(args["--layout"], do_render="--no-render" not in flags, save="--save" in flags)


if __name__ != "kg_make_minimap":   # executed as a script (remote exec or -run=PythonScript), not imported
    main(sys.argv[1:])
