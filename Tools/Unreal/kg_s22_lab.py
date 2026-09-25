"""SPRINT-022 lab level: try kit combinations, house archetypes, landmarks and water materials OUTSIDE the real map.

Headless commandlet (writes only /Game/KillGodot/Maps/Test/L_KG_S22_Lab, never L_Morrowmere_v2):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_s22_lab.py <scene>" -unattended -nosplash -nopause -nullrhi
  scenes: kit | houses | water | towers   (several: "kit,houses")
Look at it: powershell -File Tools/Unreal/kg_s22_lab_capture.ps1 -Shots "name:x,y,z,tx,ty,tz,fov;..."
The v2 builder is imported with KG_V2_NOBUILD=1, so every helper (build_house, tower, spawn, materials) is the real one.
"""
import json
import math
import os
import sys

import unreal

os.environ["KG_V2_NOBUILD"] = "1"
TOOLS = "D:/Kill Godot/Tools/Unreal"
LAB = "/Game/KillGodot/Maps/Test/L_KG_S22_Lab"
sys.path.insert(0, TOOLS)
sys.path.insert(0, "D:/Kill Godot/Tools/Level")
B = {"__name__": "kg_build_village_v2_lab"}
exec(compile(open(f"{TOOLS}/kg_build_village_v2.py", encoding="utf-8").read(), "kg_build_village_v2", "exec"), B)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
M = 100.0


def log(m):
    unreal.log(f"KG_LAB {m}")
    print(f"KG_LAB {m}")


def load_district_mis():
    """The district MIs exist already (the v2 build made them): load, never re-save (the editor may have them open)."""
    for dist, kinds in B["TINTS"].items():
        for kind, tints in kinds.items():
            for v in range(len(tints)):
                path = B["DISTRICT_MATS"] + f"MI_KG_{dist}_{kind}" + (f"_{v}" if len(tints) > 1 else "")
                mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
                B["_dmi"][(dist, kind, v)] = mi or unreal.load_asset(f"{B['KIT_MATS']}MI_{kind}")


def open_lab():
    if unreal.EditorAssetLibrary.does_asset_exist(LAB):
        les.load_level(LAB)
        doomed = [a for a in eas.get_all_level_actors() if a.get_class().get_name() not in ("WorldSettings", "Brush")]
        eas.destroy_actors(doomed)
    else:
        les.new_level(LAB)
    B["sky_and_light"] = B["V1"]["sky_and_light"]
    try:
        B["V1"]["sky_and_light"]()
        B["V1"]["look_post_process"]()
    except Exception as e:
        log(f"sky failed {e}")
    g = B["spawn"]("/Engine/BasicShapes/Plane", 0.0, 0.0, 0.0, scale=(400.0, 400.0, 1.0), folder="Lab")
    if g:
        mat = unreal.load_asset("/Game/KillGodot/Env/KG_Village/Materials/MI_UnevenBrick")
        if mat:
            g.static_mesh_component.set_material(0, mat)
    cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0.0, 0.0, 3000.0))
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.tags = ["KG_CaptureCam"]
    cam.set_actor_label("KG_CaptureCam")


def scene_kit():
    """Kit pieces at the SAME transform as a front wall piece (builder frame: front = local -Y, yaw 0)."""
    V = B["V"]
    f = B["Frame"](0.0, 0.0, 0.0, 0.0)
    combos = [
        ["Wall_Plaster_Window_Wide_Round", "WindowShutters_Wide_Round_Open"],
        ["Wall_Plaster_Straight", "Balcony_Simple_Straight"],
        ["Wall_Plaster_Door_Round", "Roof_Wooden_2x1"],
        ["Wall_Plaster_Window_Wide_Flat", "WindowShutters_Wide_Flat_Open", "Window_Wide_Flat1"],
        ["Wall_Plaster_Straight", "Overhang_Plaster_Long"],
        ["Wall_UnevenBrick_Window_Thin_Round", "WindowShutters_Thin_Round_Closed"],
        ["Wall_Plaster_Straight", "Stairs_Exterior_Straight"],
        ["Wall_Plaster_Straight", "Roof_Support2", "Prop_Support"],
    ]
    for i, names in enumerate(combos):
        x = (i - len(combos) / 2.0) * 400.0
        for n in names:
            B["spawn"](V + n, x, -300.0, 0.0, 0.0, folder="Lab/Kit")
    # a marker cube on the local -Y side (the street side in the builder's frame)
    B["spawn"]("/Engine/BasicShapes/Cube", -1800.0, -700.0, 50.0, scale=(0.4, 0.4, 1.0), folder="Lab/Kit")
    # hip roof test: tower roof scaled onto a 6x8 and a 4x6 box
    for j, (w, d) in enumerate(((6, 8), (4, 6), (6, 6))):
        x0 = -1200.0 + j * 1100.0
        B["spawn"](V + "Roof_Tower_RoundTiles", x0, 900.0, 300.0, 0.0, scale=(w * 100.0 / 566.0 * 1.12, d * 100.0 / 542.0 * 1.12, 0.62),
                   folder="Lab/Kit")
        B["spawn"]("/Engine/BasicShapes/Cube", x0, 900.0, 150.0, scale=(w, d, 3.0), folder="Lab/Kit")


def scene_houses():
    """One building of every archetype (all storeys that fit), in a row, plus a party-wall block of four."""
    import kg_archetypes_v2 as ARCH
    L = B["LAYOUT"]
    x = -2600.0
    k = 0
    specs = []
    for name, A in ARCH.ARCHETYPES.items():
        for st in A["storeys"]:
            spec = {"id": f"LAB{k:02d}", "at": [x / M, 12.0], "size": [6, 8], "face_deg": -90.0, "yaw": 0.0, "z": 0.0,
                    "storeys": st, "style": k, "district": "heart", "archetype": name, "name": f"{name} {st}"}
            spec["details"] = ARCH.details(spec)
            specs.append(spec)
            x += 900.0
            k += 1
    for i, sp in enumerate(specs):
        B["FREE_SIDES"][sp["id"]] = ["right"]
        B["build_house"](sp, "home" if i % 2 == 0 else "infill", {}, i, 0)
        log(f"house {sp['id']} {sp['archetype']} x {sp['at'][0]:.0f} m")
    log(f"houses: {len(specs)} archetype samples")


def scene_water():
    """Tanks (1.6 m walls, water at 1.4 m over the sand floor) with a stair, a rock and a post crossing the surface,
    one per water material; plus a sloping beach under the sea material for the shoreline fade."""
    import importlib
    import kg_make_water_v2 as W
    importlib.reload(W)
    mats = W.ensure()
    cube = "/Engine/BasicShapes/Cube"
    for i, (name, mpath) in enumerate(mats.items()):
        x = -1500.0 + i * 1100.0
        y = 1200.0
        for sx, sy, scx, scy in ((0, -1, 9.3, 0.3), (0, 1, 9.3, 0.3), (-1, 0, 0.3, 9.3), (1, 0, 0.3, 9.3)):
            B["spawn"](cube, x + sx * 465.0, y + sy * 465.0, 80.0, scale=(scx, scy, 1.6), folder="Lab/Water")
        B["spawn"](B["V"] + "Stairs_Exterior_Straight", x - 200.0, y - 300.0, 0.0, 0.0, folder="Lab/Water")
        B["spawn"](B["N"] + "Rock_Medium_2", x + 180.0, y + 120.0, 0.0, 30.0, scale=1.4, folder="Lab/Water")
        B["spawn"](B["V"] + "Corner_Exterior_Wood", x + 250.0, y - 250.0, 0.0, 0.0, scale=(1.5, 1.5, 1.0), folder="Lab/Water")
        p = B["spawn"]("/Engine/BasicShapes/Plane", x, y, 140.0, scale=(9.0, 9.0, 1.0), folder="Lab/Water", collide=False)
        mi = unreal.load_asset(mpath)
        if p and mi:
            p.static_mesh_component.set_material(0, mi)
    # beach: a tilted slab running under a wide sea plane
    B["spawn"](cube, 0.0, 3500.0, 60.0, pitch=-8.0, scale=(30.0, 20.0, 1.0), folder="Lab/Water")
    sea = B["spawn"]("/Engine/BasicShapes/Plane", 0.0, 3500.0, 100.0, scale=(40.0, 30.0, 1.0), folder="Lab/Water", collide=False)
    if sea:
        sea.static_mesh_component.set_material(0, unreal.load_asset(mats["MI_KG_Water_Sea"]))
    for k in range(4):
        B["spawn"](B["V"] + "Corner_Exterior_Wood", -600.0 + k * 400.0, 3300.0, -100.0, scale=(2.0, 2.0, 1.2), folder="Lab/Water")
    log(f"water: {list(mats)}")


def scene_probe():
    """A plain builder house facing -Y (yaw 0) with kit extras on its front wall transform (orientation probe)."""
    V = B["V"]
    spec = {"id": "PROBE", "at": [0.0, 5.0], "size": [6, 8], "face_deg": -90.0, "yaw": 0.0, "z": 0.0, "storeys": 2,
            "style": 1, "district": "heart", "name": "probe"}
    f = B["build_house"](spec, "infill", {}, 0, 0)
    # front wall cells at local y = -400 (d = 8), t = -200, 0, 200; level 1 at z 300
    for n, t, z in (("WindowShutters_Wide_Round_Open", -200.0, 300.0), ("Balcony_Simple_Straight", 0.0, 300.0),
                    ("Roof_Wooden_2x1", 200.0, 0.0), ("Roof_Support2", 200.0, 300.0)):
        f.put(V + n, t, -400.0, z, 0.0, "Lab/Probe")
    B["spawn"]("/Engine/BasicShapes/Cube", 0.0, -300.0, 25.0, scale=(0.5, 0.5, 0.5), folder="Lab/Probe")   # street marker


SCENES = {"probe": scene_probe, "kit": scene_kit, "houses": scene_houses, "water": scene_water}
args = [a for a in sys.argv[1:] if a and not a.startswith("-")]
want = (args[0] if args else "kit").split(",")
open_lab()
load_district_mis()
for s in want:
    try:
        SCENES[s]()
    except Exception:
        import traceback
        log(f"scene {s} FAILED:\n{traceback.format_exc()}")
les.save_current_level()
log(f"saved {LAB} with {want}")
