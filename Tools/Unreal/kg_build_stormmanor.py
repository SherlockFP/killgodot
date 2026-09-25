"""Build /Game/KillGodot/Maps/L_StormManor ("Storm Manor", map 2) from Tools/Level/stormmanor_layout.json. HEADLESS.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_build_stormmanor.py" -unattended -nosplash -nopause -nullrhi
  or the whole pipeline: powershell -File Tools/Unreal/kg_build_stormmanor_all.ps1

Plan: Docs/Level/StormManor_Plan.md (SPRINT-017), build contract Docs/Iterations/SPRINT-018-StormManor-Build.md.
Geometry (cells, stair wells, build fixes, Z0) comes from Tools/Level/stormmanor_geo.py; room furnishing from
Tools/Unreal/kg_sm_dress.py (kg_interiors + story vignettes). Deterministic; re-running rebuilds the level from scratch.

What it makes, level by level (cellar z-3, ground 0, first +3, attic +6, tower top +9 m over Z0 = +5 m):
  * a rock island (plateau boxes, cliff skirt, lower shelf round the boathouse) in a storm sea (KG_Sea)
  * every room: kit floor tiles, walls laid in runs along the 2 m grid (UnevenBrick outside, plaster inside, doors and
    windows at the layout's points, 1 m fillers), ceilings (the floor above, else a roof deck with a parapet), the Great
    Hall's two-storey void with a gallery balustrade, pitched roofs over the attic, clock room and boathouse
  * KGDoors (their leaf never cuts the navmesh; bots open doors), straight kit stairs scaled to 6 m with hidden bot
    ramps, the tower ladder (KGLadder), balustrades round every stair well
  * secret passages S1-S6 as AKGPassage pairs (E: travel to the other end), panel chore stations, KG_Gallows +
    KG_BotHub at the long table, 20 player starts round it, KG_BotSpot markers in every room, nav bounds, minimap
  * a storm night: moonlight + sky tagged KG_FixedLook (the phase look leaves them alone), dark clouds, fog, warm
    shadowless interior lights (budget 60, hero lights cast shadows)
Repeated meshes go through AKGFoliageField HISMs (kit pieces always, props from 3 copies on a floor); the report
Saved/KG_SM_BuildReport.json lists plain actors per floor, instances, lights, missing meshes and the placement pre-check.
NOTE: unreal.Rotator positional order is (roll, pitch, yaw) and unreal.Color is BGRA: always use keywords.
"""
import importlib
import json
import math
import os
import random
import sys
import time

import unreal

ROOT = "D:/Kill Godot"
TOOLS = f"{ROOT}/Tools/Unreal"
sys.path.insert(0, TOOLS)
sys.path.insert(0, f"{ROOT}/Tools/Level")
import stormmanor_geo as G  # noqa: E402

importlib.reload(G)

LEVEL = os.environ.get("KG_SM_LEVEL") or "/Game/KillGodot/Maps/L_StormManor"
REPORT = f"{ROOT}/Saved/KG_SM_BuildReport.json"
MAT_DIR = "/Game/KillGodot/Env/StormManor/Materials"
V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_"
DL = "/Game/KillGodot/Env/Dress/KG_DressLandmarks_Clean/StaticMeshes/SM_KG_"
DT = "/Game/KillGodot/Env/Dress/KG_DressTerrace_Clean/StaticMeshes/SM_KG_"
DU = "/Game/KillGodot/Env/Dress/KG_DressUnder_Clean/StaticMeshes/SM_KG_"
DW = "/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_"
E = "/Engine/BasicShapes/"
AUDIO = "/Game/KillGodot/Audio/"
M = 100.0
H = G.STOREY
WALL_OUT = 21.0          # exterior walls: slab from 10 cm inside to 31 cm outside the room edge (v2 builder)
WALL_MID = 10.5          # interior walls: slab centred on the edge (+-20.5 cm)
LIGHT_BUDGET = 60

T0 = time.time()
L = G.layout()
GRID = G.Grid(L)
R = GRID.R
_real = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
stats = {"lights": 0, "hero_lights": 0, "doors": 0, "windows": 0, "wall_pieces": 0, "floor_tiles": 0, "stairs": 0,
         "passages": 0, "panel_stations": 0}
FIXED_LOOK = "KG_FixedLook"


def log(msg):
    unreal.log(f"KG_SM {msg}")
    print(f"KG_SM {msg}")


def fz(fid):
    return G.floor_z(fid)


def fid_of_z(z):
    best = "C"
    for f in G.ORDER:
        if z >= fz(f) - 60.0:
            best = f
    return best


# =================================================================================================== meshes
_mesh = {}
_bounds = {}


def mesh(path):
    if path not in _mesh:
        ok = path.startswith("/Engine/") or unreal.EditorAssetLibrary.does_asset_exist(path)
        _mesh[path] = unreal.load_asset(path) if ok else None
        if not _mesh[path]:
            stats.setdefault("missing", set()).add(path)
            unreal.log_warning(f"KG_SM missing mesh {path}")
    return _mesh[path]


def bounds(path):
    """(x0, y0, z0, x1, y1, z1) cm of a mesh."""
    if path not in _bounds:
        m = mesh(path)
        bb = m.get_bounding_box() if m else None
        _bounds[path] = (bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z) if bb else (-50, -50, 0, 50, 50, 100)
    return _bounds[path]


def have(path):
    return path.startswith("/Engine/") or unreal.EditorAssetLibrary.does_asset_exist(path)


# =================================================================================================== batching spawner
class Batcher:
    """Every static mesh goes through here. kind='arch' -> always a HISM instance; 'prop' -> HISM when the same mesh
    appears 3+ times on a floor with the same collision, else a StaticMeshActor. Records feed the report and the
    placement pre-check."""

    def __init__(self):
        self.recs = []
        self.fields = {}
        self.plain = {f: 0 for f in G.ORDER}
        self.instances = 0

    def put(self, path, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, folder="SM", collide=True, kind="prop",
            nav=True, hidden=False, material=None, label=None, cull=0.0, shadow=True, room=None, on=None):
        if not mesh(path):
            return None
        s = (float(scale),) * 3 if isinstance(scale, (int, float)) else tuple(float(v) for v in scale)
        rec = {"m": path, "x": x, "y": y, "z": z, "yaw": yaw, "pitch": pitch, "roll": roll, "s": s, "folder": folder,
               "collide": collide, "kind": kind, "nav": nav, "hidden": hidden, "material": material, "label": label,
               "cull": cull, "shadow": shadow, "room": room, "on": on}
        self.recs.append(rec)
        return rec

    def flush(self):
        groups = {}
        for r in self.recs:
            # wall shelves carry clutter: they keep a collision (mounted high, nobody walks into them)
            if r["kind"] == "prop" and "/Shelf_" in r["m"]:
                r["collide"] = True
            # small clutter (mugs, books, jars) needs no collision: players do not snag on it and it batches
            elif r["kind"] == "prop" and r["collide"]:
                b = bounds(r["m"])
                if (b[5] - b[2]) * r["s"][2] < 30.0:
                    r["collide"] = False
            special = r["hidden"] or r["material"] or r["label"] or not r["nav"] or not r["shadow"]
            f = fid_of_z(r["z"])
            key = (r["m"], bool(r["collide"]), f)
            if special:
                self._actor(r, f)
                continue
            groups.setdefault(key, []).append(r)
        for (path, collide, f), rs in groups.items():
            if (rs[0]["kind"] == "arch" or len(rs) >= 3 or any(r["kind"] == "arch" for r in rs)) and ism_ok(path):
                self._hism(path, collide, f, rs)
            else:
                for r in rs:
                    self._actor(r, f)
        log(f"flush: {len(self.recs)} meshes -> {self.instances} instances + {sum(self.plain.values())} actors "
            f"{json.dumps(self.plain)}")

    def _hism(self, path, collide, f, rs):
        key = (f, collide)
        fld = self.fields.get(key)
        if fld is None:
            cls = unreal.load_class(None, "/Script/KillGodot.KGFoliageField")
            # at the origin: the field's HISMs are not attached to its root after a save/load, so instances stored
            # relative to a raised actor ended up fz(f) too low in -game (F0 walls in the cellar, floors under it)
            fld = _real.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0))
            fld.set_actor_label(f"KG_SM_{f}_{'Solid' if collide else 'Clutter'}")
            fld.set_folder_path(f"StormManor/Instanced")
            self.fields[key] = fld
            self.plain[f] += 1
        tr = [unreal.Transform(unreal.Vector(r["x"], r["y"], r["z"]),
                               unreal.Rotator(roll=r["roll"], pitch=r["pitch"], yaw=r["yaw"]), unreal.Vector(*r["s"]))
              for r in rs]
        b = bounds(path)
        small = max(b[3] - b[0], b[4] - b[1], b[5] - b[2]) < 45.0
        fld.add_instances(mesh(path), tr, collide, 3500.0 if small else 0.0)
        self.instances += len(tr)
        for r in rs:
            r["hism"] = True

    def _actor(self, r, f):
        a = _real.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(r["x"], r["y"], r["z"]),
                                         unreal.Rotator(roll=r["roll"], pitch=r["pitch"], yaw=r["yaw"]))
        if not a:
            return None
        comp = a.static_mesh_component
        comp.set_static_mesh(mesh(r["m"]))
        if r["s"] != (1.0, 1.0, 1.0):
            a.set_actor_scale3d(unreal.Vector(*r["s"]))
        a.set_folder_path(r["folder"])
        if not r["collide"]:
            a.set_actor_enable_collision(False)
            comp.set_editor_property("can_ever_affect_navigation", False)
        if not r["nav"]:
            comp.set_editor_property("can_ever_affect_navigation", False)
        if r["hidden"]:
            a.set_actor_hidden_in_game(True)
        if r["label"]:
            a.set_actor_label(r["label"])
        if r["cull"] > 0.0:
            comp.set_editor_property("ld_max_draw_distance", r["cull"])
        if r.get("bounds_scale"):
            comp.set_editor_property("bounds_scale", r["bounds_scale"])
        if not r["shadow"]:
            comp.set_editor_property("cast_shadow", False)
        if r["material"]:
            mat = unreal.load_asset(r["material"])
            if mat:
                for i in range(max(1, comp.get_num_materials())):
                    comp.set_material(i, mat)
        self.plain[f] += 1
        r["actor"] = True
        return a


_ism_ok = {}


def ism_ok(path):
    """True when every material of the mesh has an instancing-ready base (else HISMs would draw the default checker in
    -game): such meshes stay plain actors instead of touching shared materials."""
    if path not in _ism_ok:
        ok = True
        m = mesh(path)
        for sm in (m.get_editor_property("static_materials") if m else []):
            mi = sm.get_editor_property("material_interface")
            base = mi.get_base_material() if mi else None
            if base and not base.get_editor_property("used_with_instanced_static_meshes"):
                ok = False
                stats.setdefault("no_ism_meshes", []).append(path.split("/")[-1])
                break
        _ism_ok[path] = ok
    return _ism_ok[path]


B = Batcher()


def put(path, x, y, z, yaw=0.0, **kw):
    return B.put(path, x, y, z, yaw, **kw)


def spawn_class(class_path, x, y, z, yaw=0.0, folder="StormManor/Gameplay"):
    cls = unreal.load_class(None, class_path) if class_path.startswith("/Script/") else class_path
    if cls is None:
        log(f"missing class {class_path}")
        return None
    a = _real.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if a:
        a.set_folder_path(folder)
        B.plain[fid_of_z(z)] += 1
    return a


class InteriorShim:
    """kg_interiors' EditorActorSubsystem: static meshes are recorded into the batcher (proxies take the scale /
    folder / collision calls), gameplay classes (AKGSeat ...) spawn for real."""

    class Proxy:
        def __init__(self, rec):
            self.rec = rec

        def set_actor_scale3d(self, v):
            if self.rec:
                self.rec["s"] = (v.x, v.y, v.z)

        def set_folder_path(self, f):
            if self.rec:
                self.rec["folder"] = "StormManor/Interior/" + str(f).split("/")[-1]

        def set_actor_enable_collision(self, b):
            if self.rec:
                self.rec["collide"] = bool(b)

        def __getattr__(self, name):
            return lambda *a, **k: None

    def __init__(self):
        self.room = None

    def spawn_actor_from_object(self, obj, loc, rot=None):
        rot = rot or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
        if isinstance(obj, unreal.StaticMesh):
            path = obj.get_path_name().split(".")[0]
            _mesh.setdefault(path, obj)
            rec = B.put(path, loc.x, loc.y, loc.z, rot.yaw, rot.pitch, rot.roll, folder="StormManor/Interior",
                        room=self.room)
            return InteriorShim.Proxy(rec)
        return _real.spawn_actor_from_object(obj, loc, rot)

    def spawn_actor_from_class(self, cls, loc, rot=None):
        rot = rot or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
        a = _real.spawn_actor_from_class(cls, loc, rot)
        if a:
            B.plain[fid_of_z(loc.z)] += 1
        return a

    def __getattr__(self, name):
        return getattr(_real, name)


SHIM = InteriorShim()


# =================================================================================================== lights
def light(x, y, z, intensity=10.0, radius=900.0, color=(255, 170, 95), folder="StormManor/Lights", hero=False,
          force=False):
    """Warm movable point light, shadowless unless hero (hearths). Budget LIGHT_BUDGET."""
    if stats["lights"] >= LIGHT_BUDGET and not force:
        stats["lights_refused"] = stats.get("lights_refused", 0) + 1
        return None
    a = _real.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    lc.set_editor_property("cast_shadows", bool(hero))
    a.set_folder_path(folder)
    stats["lights"] += 1
    stats["hero_lights"] += 1 if hero else 0
    B.plain[fid_of_z(z)] += 1
    return a


def interior_light(x, y, z, intensity=10.0, radius=900.0, folder="StormManor/Lights"):
    """kg_interiors' warm_light signature."""
    return light(x, y, z, intensity, radius, folder="StormManor/Lights")


# =================================================================================================== materials
def flat_materials():
    """M_KG_SM_Flat (colour + roughness parameters, ISM usage) and its instances. New assets only."""
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = f"{MAT_DIR}/M_KG_SM_Flat"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
    else:
        m = tools.create_asset("M_KG_SM_Flat", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        m.set_editor_property("used_with_instanced_static_meshes", True)
        col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 0)
        col.set_editor_property("parameter_name", "Color")
        col.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 250)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.85)
        emi = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 400)
        emi.set_editor_property("parameter_name", "Glow")
        emi.set_editor_property("default_value", 0.0)
        mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 400)
        mel.connect_material_expressions(col, "", mul, "A")
        mel.connect_material_expressions(emi, "", mul, "B")
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    out = {}
    for name, rgb, rough, glow in (("Rock", (0.10, 0.095, 0.09), 0.95, 0.0), ("RockDark", (0.055, 0.05, 0.05), 0.95, 0.0),
                                   ("Grass", (0.10, 0.17, 0.07), 0.9, 0.0), ("Gravel", (0.24, 0.22, 0.20), 0.95, 0.0),
                                   ("Mud", (0.12, 0.085, 0.06), 0.55, 0.0), ("Felt", (0.05, 0.28, 0.12), 0.9, 0.0),
                                   ("Glass", (0.35, 0.48, 0.55), 0.08, 0.0), ("Canvas", (0.80, 0.74, 0.62), 0.9, 0.0),
                                   ("Brass", (0.60, 0.42, 0.14), 0.35, 0.0), ("Copper", (0.55, 0.26, 0.12), 0.35, 0.0),
                                   ("Sheet", (0.78, 0.78, 0.74), 0.9, 0.0), ("Spark", (0.35, 0.65, 1.0), 0.4, 6.0),
                                   ("Ember", (1.0, 0.45, 0.12), 0.6, 4.0), ("Soot", (0.03, 0.03, 0.035), 0.9, 0.0),
                                   ("Portrait", (0.30, 0.18, 0.12), 0.7, 0.0), ("Moss", (0.12, 0.16, 0.08), 0.95, 0.0)):
        p = f"{MAT_DIR}/MI_KG_SM_{name}"
        mi = unreal.load_asset(p) if eal.does_asset_exist(p) else None
        if not mi:
            mi = tools.create_asset(f"MI_KG_SM_{name}", MAT_DIR, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", m)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(mi, "Glow", glow)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[name] = p
    return out


MAT = {}


# =================================================================================================== level + sky
def new_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        level_sub.load_level(LEVEL)
        keep = ("WorldSettings", "Brush", "WorldDataLayers", "LevelBounds")
        doomed = [a for a in _real.get_all_level_actors() if a.get_class().get_name() not in keep]
        _real.destroy_actors(doomed)
        log(f"cleared {len(doomed)} actors")
    else:
        level_sub.new_level(LEVEL)
        log("new level")


def storm_sky():
    """Night storm: a low cold moon behind the cloud deck, dim sky light, thick dark clouds and fog. Tagged
    KG_FixedLook so AKGGameState's phase lighting leaves the map's own look alone."""
    moon = _real.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000),
                                        unreal.Rotator(roll=0.0, pitch=-38.0, yaw=215.0))
    moon.set_actor_label("Moon")
    moon.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = moon.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property("intensity", 0.9)
    lc.set_editor_property("light_color", unreal.Color(r=150, g=172, b=235, a=255))
    lc.set_editor_property("atmosphere_sun_light", True)
    moon.tags = [FIXED_LOOK]
    _real.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
    sky = _real.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    sky.set_actor_label("SkyLight")
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 0.45)
    sc.set_editor_property("lower_hemisphere_is_black", False)
    sky.tags = [FIXED_LOOK]
    fog = _real.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label("StormFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.03)
    fc.set_editor_property("fog_height_falloff", 0.08)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.10, 0.13, 0.19, 1.0))
    clouds = _real.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0))
    clouds.set_actor_label("StormClouds")
    cc = clouds.get_component_by_class(unreal.VolumetricCloudComponent)
    cc.set_editor_property("layer_bottom_altitude", 1.2)
    cc.set_editor_property("layer_height", 6.0)
    cm = unreal.load_asset("/Game/KillGodot/Materials/MI_KG_Clouds")
    if cm:
        cc.set_editor_property("material", cm)
    atm = [a for a in _real.get_all_level_actors() if a.get_class().get_name() == "SkyAtmosphere"][0]
    ac = atm.get_component_by_class(unreal.SkyAtmosphereComponent)
    ac.set_editor_property("mie_scattering_scale", 0.02)
    ac.set_editor_property("rayleigh_scattering_scale", 0.02)
    vol = _real.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    vol.set_actor_label("KG_SM_PP_Storm")
    vol.set_editor_property("unbound", True)
    s = vol.get_editor_property("settings")
    outline = unreal.load_asset("/Game/KillGodot/Materials/M_KG_PP_Outline")
    if outline:
        wb = unreal.WeightedBlendables()
        wb.set_editor_property("array", [unreal.WeightedBlendable(1.0, outline)])
        s.set_editor_property("weighted_blendables", wb)
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", 0.6)
    s.set_editor_property("override_auto_exposure_min_brightness", True)
    s.set_editor_property("auto_exposure_min_brightness", 0.05)
    s.set_editor_property("override_auto_exposure_max_brightness", True)
    s.set_editor_property("auto_exposure_max_brightness", 1.2)
    s.set_editor_property("override_color_saturation", True)
    s.set_editor_property("color_saturation", unreal.Vector4(1.05, 1.05, 1.1, 1.0))
    s.set_editor_property("override_vignette_intensity", True)
    s.set_editor_property("vignette_intensity", 0.45)
    vol.set_editor_property("settings", s)
    for a in (moon, sky, fog, clouds, atm, vol):
        a.set_folder_path("StormManor/Lighting")


# =================================================================================================== island + sea
def rects_from_cells(cells):
    """Greedy merge of 2 m cells into rectangles [(i0, j0, i1, j1)] (inclusive cell ranges)."""
    left = set(cells)
    out = []
    for c in sorted(cells):
        if c not in left:
            continue
        i0, j0 = c
        i1 = i0
        while (i1 + 1, j0) in left:
            i1 += 1
        j1 = j0
        while all((i, j1 + 1) in left for i in range(i0, i1 + 1)):
            j1 += 1
        for i in range(i0, i1 + 1):
            for j in range(j0, j1 + 1):
                left.discard((i, j))
        out.append((i0, j0, i1, j1))
    return out


def box(x0, y0, x1, y1, ztop, thick, mat, folder, label=None, collide=True):
    """A cube (1 m basic shape) scaled to the rect (metres), top face at ztop cm."""
    return put(E + "Cube", (x0 + x1) * 50.0, (y0 + y1) * 50.0, ztop - thick / 2.0, 0.0,
               scale=((x1 - x0), (y1 - y0), thick / 100.0), folder=folder, material=MAT.get(mat), label=label,
               collide=collide, kind="ground")


GROUND_MAT = {"courtyard": "Gravel", "service_yard": "Mud", "graveyard": "Grass", "cliff_path": "Rock"}


def island_and_sea():
    """Plateau at the ground floor (ground boxes per garden room, rock on the margin), a cliff skirt down into the
    sea, a lower shelf round the boathouse and the tunnel mouth, the storm sea."""
    f0 = GRID.occ["F0"]
    holes0 = GRID.holes["F0"]
    base = set(f0)
    plateau = set()
    for (i, j) in base:
        for di in range(-2, 3):
            for dj in range(-2, 3):
                plateau.add((i + di, j + dj))
    plateau -= holes0
    # cells taken by the sea stair's cut (C level annex) stay open
    cut = {c for c, sid in GRID.stair_cells["C"].items() if sid == "st_sea"}
    plateau -= cut
    by_mat = {}
    for c in plateau:
        r = GRID.room_at("F0", c)
        if r and G.enclosed(r):
            continue                              # the house floor tiles cover it
        mat = GROUND_MAT.get(r["id"], "Gravel") if r else "Rock"
        by_mat.setdefault(mat, set()).add(c)
    n = 0
    for mat, cells in by_mat.items():
        for i0, j0, i1, j1 in rects_from_cells(cells):
            box(i0 * 2.0, j0 * 2.0, (i1 + 1) * 2.0, (j1 + 1) * 2.0, fz("F0"), 60.0, mat, "StormManor/Island")
            n += 1
    # the lower shelf: boathouse + tunnel cells at the cellar level, grown by 2 cells, outside the plateau
    lower = set()
    for c, rid in GRID.occ["C"].items():
        if rid in ("boathouse", "smugglers_tunnel") or c in cut:
            for di in range(-2, 3):
                for dj in range(-2, 3):
                    lower.add((c[0] + di, c[1] + dj))
    lower -= plateau
    lower_open = {c for c in lower if GRID.occ["C"].get(c) is None}
    for i0, j0, i1, j1 in rects_from_cells(lower_open):
        box(i0 * 2.0, j0 * 2.0, (i1 + 1) * 2.0, (j1 + 1) * 2.0, fz("C"), 60.0, "Rock", "StormManor/Island")
        n += 1
    # the rock body under the cellar level (everything), down into the sea
    allc = plateau | lower
    for i0, j0, i1, j1 in rects_from_cells(allc):
        box(i0 * 2.0 - 0.6, j0 * 2.0 - 0.6, (i1 + 1) * 2.0 + 0.6, (j1 + 1) * 2.0 + 0.6, fz("C") - 2.0, 1400.0,
            "RockDark", "StormManor/Island")
        n += 1
    # cliff skirt: the plateau's outer faces between the cellar level and the ground floor (boxes on the rim cells)
    # (a stair hole through the ground floor is not the plateau's edge: rim boxes there filled the cellar round the
    # cellar and crypt stair feet, 2.4 m of rock that cut the cellar off the navmesh)
    inner = set(holes0)
    rim = [c for c in plateau if any((c[0] + d[0], c[1] + d[1]) not in plateau and (c[0] + d[0], c[1] + d[1]) not in lower
                                     and (c[0] + d[0], c[1] + d[1]) not in inner
                                     for d in ((1, 0), (-1, 0), (0, 1), (0, -1))) and c not in GRID.occ["C"]]
    for i0, j0, i1, j1 in rects_from_cells(rim):
        box(i0 * 2.0, j0 * 2.0, (i1 + 1) * 2.0, (j1 + 1) * 2.0, fz("F0") - 58.0, H - 60.0, "Rock", "StormManor/Island")
        n += 1
    # the rim between the plateau and the lower shelf too (the boathouse's back wall of rock)
    # (never over a cellar-level room: the tunnel runs under the plateau's edge to the boathouse)
    rim2 = [c for c in plateau if any((c[0] + d[0], c[1] + d[1]) in lower for d in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            and c not in GRID.occ["C"]]
    for i0, j0, i1, j1 in rects_from_cells(rim2):
        box(i0 * 2.0, j0 * 2.0, (i1 + 1) * 2.0, (j1 + 1) * 2.0, fz("F0") - 58.0, H - 60.0, "Rock", "StormManor/Island")
        n += 1
    # big cliff meshes round the outside for a craggy silhouette (visual; the boxes carry the collision)
    cliffs = [p for p in (DL + "Cliff_A", DL + "Cliff_B", DL + "Cliff_C") if have(p)]
    rng = random.Random(18)
    outer = sorted(c for c in allc if any((c[0] + d[0], c[1] + d[1]) not in allc for d in ((1, 0), (-1, 0), (0, 1), (0, -1))))
    for k, c in enumerate(outer):
        if k % 3 or not cliffs:
            continue
        cx, cy = c[0] * 2.0 + 1.0, c[1] * 2.0 + 1.0
        dx = sum(1 for d in ((1, 0),) if (c[0] + 1, c[1]) not in allc) - sum(1 for d in ((1, 0),) if (c[0] - 1, c[1]) not in allc)
        dy = (1 if (c[0], c[1] + 1) not in allc else 0) - (1 if (c[0], c[1] - 1) not in allc else 0)
        yaw = math.degrees(math.atan2(dy, dx)) if (dx or dy) else rng.uniform(0, 360)
        ztop = fz("C") if c in lower else fz("F0")
        pick = rng.choice(cliffs)
        cb = bounds(pick)
        sz = rng.uniform(1.0, 1.4)
        sxy = (rng.uniform(0.9, 1.3), rng.uniform(0.9, 1.3))
        px, py = cx * M + dx * 150.0, cy * M + dy * 150.0
        # never into a cellar room (a cliff mesh showed through the wine cellar as a pale blob in look round 1)
        # (pushed out to sea until clear, else left out)
        rad = 0.5 * max(cb[3] - cb[0], cb[4] - cb[1]) * max(sxy) + 120.0

        def hits_cellar(x, y):
            return ztop - 45.0 > fz("C") and any(math.hypot(x - (q[0] * 200.0 + 100.0), y - (q[1] * 200.0 + 100.0)) < rad
                                                 for q in GRID.occ["C"])
        push = 0
        while hits_cellar(px, py) and push < 5 and (dx or dy):
            px, py, push = px + dx * 100.0, py + dy * 100.0, push + 1
        if hits_cellar(px, py):
            continue
        put(pick, px, py, ztop - 45.0 - cb[5] * sz, yaw + rng.uniform(-25, 25),
            scale=(sxy[0], sxy[1], sz), folder="StormManor/Cliffs",
            collide=False, kind="prop")
    # the lower shelf's sea edge: a low wall so nobody walks off the boathouse ledge into the storm
    for c in lower_open:
        for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nb = (c[0] + d[0], c[1] + d[1])
            if nb in lower or nb in plateau or nb in GRID.occ["C"]:
                continue
            orient = "v" if d[0] else "h"
            coord = (c[0] + (1 if d[0] > 0 else 0)) * 2.0 if d[0] else (c[1] + (1 if d[1] > 0 else 0)) * 2.0
            t0 = (c[1] if d[0] else c[0]) * 2.0
            parapet_run("C", orient, coord, t0, t0 + 2.0, 1 if (d[0] > 0 or d[1] > 0) else -1, fz("C"), "")
    stats["island_boxes"] = n
    sea = put("/Game/KillGodot/Env/Sea/KG_Sea/StaticMeshes/KG_Sea", 0.0, 0.0, 0.0, folder="StormManor/Sea", collide=False,
              label="Sea", material="/Game/KillGodot/Materials/S22/MI_KG_Water_Sea"
              if have("/Game/KillGodot/Materials/S22/MI_KG_Water_Sea") else None)
    if sea:
        sea["bounds_scale"] = 3.0


# =================================================================================================== walls
def enclosed_at(fid, c):
    if GRID.is_void(fid, c):
        return True
    return G.enclosed(GRID.room_at(fid, c))


def outdoor_at(fid, c):
    r = GRID.room_at(fid, c)
    return r is not None and not GRID.is_void(fid, c) and r["kind"] == "outdoor"


def grounds_at(fid, c):
    r = GRID.room_at(fid, c)
    return r is not None and not G.enclosed(r) and r["kind"] != "outdoor"


def rid_at(fid, c):
    r = GRID.room_at(fid, c)
    return r["id"] if r else None


EDGE_KEYS = {}


def collect_edges():
    """{(fid, orient, coord_m, type, a, b, nsign): [seg index]} over every level. orient 'v' = wall on x = coord
    (runs along y), 'h' = wall on y = coord. The segment index k spans [2k, 2k+2] m along the run. nsign: the owner's
    outward normal (+1 / -1 along the axis). type: ext | int | void | rail | parapet | fence."""
    runs = {}
    for fid in G.ORDER:
        occ = GRID.occ[fid]
        seen = set()
        for c in list(occ):
            for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nb = (c[0] + d[0], c[1] + d[1])
                if d[0]:
                    key = ("v", c[0] + (1 if d[0] > 0 else 0), c[1])
                else:
                    key = ("h", c[1] + (1 if d[1] > 0 else 0), c[0])
                if key in seen:
                    continue
                seen.add(key)
                ra, rb = rid_at(fid, c), rid_at(fid, nb)
                va, vb = GRID.is_void(fid, c), GRID.is_void(fid, nb)
                if ra == rb and va == vb:
                    continue
                ea, eb = enclosed_at(fid, c), enclosed_at(fid, nb)
                sign = 1 if (d[0] > 0 or d[1] > 0) else -1       # normal from c towards nb
                t = None
                if ea and eb:
                    if va != vb and ra == "gallery" or rb == "gallery" and va != vb:
                        # gallery floor edge over the hall void: balustrade on the gallery side
                        gal_c = c if ra == "gallery" else nb
                        t = ("void", "gallery", "~void", sign if gal_c == c else -sign)
                    else:
                        a, b = sorted([(ra or "") + ("~v" if va else ""), (rb or "") + ("~v" if vb else "")])
                        own_is_c = ((ra or "") + ("~v" if va else "")) == a
                        t = ("int", a, b, sign if own_is_c else -sign)
                elif ea or eb:
                    own = c if ea else nb
                    t = ("ext", rid_at(fid, own), rid_at(fid, nb if ea else c) or "", sign if ea else -sign)
                elif outdoor_at(fid, c) or outdoor_at(fid, nb):
                    if rb is None or ra is None:
                        own = c if outdoor_at(fid, c) else nb
                        t = ("parapet", rid_at(fid, own), "", sign if own == c else -sign)
                elif grounds_at(fid, c) or grounds_at(fid, nb):
                    if ra is None or rb is None:
                        own = c if ra else nb
                        t = ("parapet", rid_at(fid, own), "", sign if own == c else -sign)
                    elif "graveyard" in (ra, rb) or {"courtyard", "service_yard"} == {ra, rb}:
                        a, b = sorted([ra, rb])
                        t = ("fence", a, b, sign if ra == a else -sign)
                if t is None:
                    continue
                orient, coord, k = key
                runs.setdefault((fid, orient, coord * 2.0) + t, []).append(k)
    return runs


def split_runs(ks):
    ks = sorted(set(ks))
    out = []
    s = ks[0]
    p = ks[0]
    for k in ks[1:]:
        if k != p + 1:
            out.append((s, p))
            s = k
        p = k
    out.append((s, p))
    return [(a * 2.0, (b + 1) * 2.0) for a, b in out]


def openings_on(fid, orient, coord, t0, t1, pair):
    """Doors and windows on this run: [(tc, width, kind, door)]"""
    out = []
    for d in L["doors"]:
        fa, fb = R[d["a"]]["floor"], R[d["b"]]["floor"]
        if fid not in (fa, fb):
            continue
        x, y = d["at"]
        on = (orient == "v" and abs(x - coord) < 0.01 and t0 - 0.01 <= y <= t1 + 0.01) or \
             (orient == "h" and abs(y - coord) < 0.01 and t0 - 0.01 <= x <= t1 + 0.01)
        if not on:
            continue
        if not ({d["a"], d["b"]} & set(pair)):
            continue
        tc = y if orient == "v" else x
        out.append((tc, float(d["width"]), d["kind"], d))
    return out


def windows_on(fid, orient, coord, t0, t1):
    out = []
    for w in L["windows"]:
        if w["floor"] != fid:
            continue
        x, y = w["at"]
        if orient == "v" and abs(x - coord) < 0.01 and t0 + 0.9 <= y <= t1 - 0.9:
            out.append((y, w))
        elif orient == "h" and abs(y - coord) < 0.01 and t0 + 0.9 <= x <= t1 - 0.9:
            out.append((x, w))
    return out


def piece_xf(orient, coord, t, nsign, off):
    """World (x, y, yaw) of a kit wall piece centred at t along the run, its dressed +Y face along the normal."""
    if orient == "v":
        nx, ny = float(nsign), 0.0
        x, y = coord * M + nx * off, t * M
    else:
        nx, ny = 0.0, float(nsign)
        x, y = t * M, coord * M + ny * off
    yaw = math.degrees(math.atan2(-nx, ny))
    return x, y, yaw, nx, ny


WALL_REG = []          # (fid, room, orient, coord, t0, t1, kind, nsign_out_of_room)  for the dressing (segments)
DOOR_REG = []          # {"fid", "x", "y", "nx", "ny", "rooms", "width", "kind"}
WIN_REG = []


def wall_style(fid, typ, a, b):
    if typ == "ext":
        return "UnevenBrick"
    if fid == "C":
        return "UnevenBrick"
    return "Plaster"


def build_walls():
    runs = collect_edges()
    for (fid, orient, coord, typ, a, b, nsign), ks in sorted(runs.items(), key=lambda kv: str(kv[0])):
        z = fz(fid)
        for t0, t1 in split_runs(ks):
            if typ in ("ext", "int"):
                wall_run(fid, orient, coord, t0, t1, typ, a, b, nsign, z)
            elif typ == "void":
                rail_run(fid, orient, coord, t0, t1, nsign, z, "StormManor/Gallery", inset=18.0)
            elif typ == "parapet":
                parapet_run(fid, orient, coord, t0, t1, nsign, z, a)
            elif typ == "fence":
                fence_run(fid, orient, coord, t0, t1, nsign, z, a, b)


def wall_run(fid, orient, coord, t0, t1, typ, a, b, nsign, z):
    style = wall_style(fid, typ, a, b)
    off = WALL_OUT if typ == "ext" else WALL_MID
    pair = [a.replace("~v", ""), b.replace("~v", "")] if typ == "int" else [a, b]
    if typ == "int" and ("great_hall~v" in (a, b)):
        pair = ["great_hall", b if a == "great_hall~v" else a]
    ops = []
    for tc, w, kind, d in openings_on(fid, orient, coord, t0, t1, pair):
        if kind == "gate":
            continue
        if w >= 3.9:
            for k, dt in enumerate((-1.0, 1.0)):
                ops.append((tc + dt, "arch" if kind == "arch" else "door", d, k))
        else:
            ops.append((tc, "door", d, 0))
    doors_iv = [(tc - 1.0, tc + 1.0) for tc, *_ in ops]
    win_ok = typ == "ext" or (typ == "int" and {"nursery", "great_hall"} <= set(pair))
    if win_ok:
        taken = list(doors_iv)
        for tc, w in windows_on(fid, orient, coord, t0, t1):
            if all(tc + 1.0 <= a0 + 0.01 or tc - 1.0 >= a1 - 0.01 for a0, a1 in taken):
                ops.append((tc, "window", w, 0))
                taken.append((tc - 1.0, tc + 1.0))
        glass_all = "greenhouse" in pair and typ == "ext"
        if glass_all:
            t = t0 + 1.0
            while t <= t1 - 1.0 + 0.01:
                if all(t + 1.0 <= a0 + 0.01 or t - 1.0 >= a1 - 0.01 for a0, a1 in taken):
                    ops.append((t, "window", None, 0))
                    taken.append((t - 1.0, t + 1.0))
                t += 2.0
    ops.sort(key=lambda o: o[0])
    # the boathouse's sea side: a 4 m slip opening in the middle
    slip = typ == "ext" and a == "boathouse" and orient == "h" and nsign > 0
    if slip:
        mid = (t0 + t1) / 2.0
        ops = [o for o in ops if abs(o[0] - mid) > 2.9] + [(mid - 1.0, "open", None, 0), (mid + 1.0, "open", None, 0)]
        ops.sort(key=lambda o: o[0])
    folder = f"StormManor/Walls/{fid}"
    pieces = []
    cur = t0
    for tc, kind, ref, k in ops:
        if tc - 1.0 < cur - 0.01:
            continue
        fill(pieces, cur, tc - 1.0)
        pieces.append((tc, 1.0, kind, ref))
        cur = tc + 1.0
    fill(pieces, cur, t1)
    grid_n = 0
    for tc, half, kind, ref in pieces:
        x, y, yaw, nx, ny = piece_xf(orient, coord, tc, nsign, off)
        sx = half
        if kind == "solid":
            name = f"Wall_{style}_Straight"
            if style == "Plaster" and fid != "C" and half > 0.9:
                grid_n += 1
                if grid_n % 3 == 0:
                    name = "Wall_Plaster_WoodGrid"
            put(V + name, x, y, z, yaw, scale=(sx, 1.0, 1.0), folder=folder, kind="arch")
        elif kind == "door":
            put(V + f"Wall_{style}_Door_Round", x, y, z, yaw, folder=folder, kind="arch")
            door(fid, orient, coord, tc, nsign, off, z, ref, typ)
        elif kind == "arch":
            put(V + "Wall_Arch", x, y, z, yaw, folder=folder, kind="arch", collide=False)
            DOOR_REG.append({"fid": fid, "x": x, "y": y, "nx": nx, "ny": ny, "rooms": [ref["a"], ref["b"]],
                             "width": 2.0, "kind": "arch", "id": ref["id"]})
        elif kind == "open":
            DOOR_REG.append({"fid": fid, "x": x, "y": y, "nx": nx, "ny": ny, "rooms": [a], "width": 2.0, "kind": "slip",
                             "id": "slip"})
        elif kind == "window":
            wname = "Window_Wide_Flat" if (a == "greenhouse" or fid == "F2") else "Window_Wide_Round"
            put(V + f"Wall_{style}_{wname}", x, y, z, yaw, folder=folder, kind="arch")
            put(V + ("Window_Wide_Flat1" if wname.endswith("Flat") else "Window_Wide_Round1"), x, y, z, yaw,
                folder=folder, kind="arch", collide=False)
            if typ == "ext" and a != "greenhouse" and fid in ("F0", "F1"):
                sh = "WindowShutters_Wide_Flat" if wname.endswith("Flat") else "WindowShutters_Wide_Round"
                rattle = bool(ref and ref.get("rattle"))
                put(V + sh + ("_Open" if not rattle else "_Closed"), x, y, z, yaw, folder=folder, kind="arch",
                    collide=False)
            WIN_REG.append({"fid": fid, "x": x, "y": y, "nx": nx, "ny": ny, "room": a, "t": tc, "orient": orient,
                            "coord": coord})
            stats["windows"] += 1
        stats["wall_pieces"] += 1
        # segment registry for the dressing (both rooms of an interior wall)
        seg_kind = {"solid": "solid", "door": "door", "arch": "door", "open": "door", "window": "wide"}[kind]
        for room in {a.replace("~v", ""), b.replace("~v", "")} - {""}:
            WALL_REG.append((fid, room, orient, coord, tc - half, tc + half, seg_kind))
    # sliver pillars at the run ends close the corners (interior L / T joints, exterior convex corners)
    for tend in (t0, t1):
        x, y, yaw, nx, ny = piece_xf(orient, coord, tend, nsign, off)
        put(V + f"Wall_{style}_Straight", x, y, z, yaw, scale=(0.31 if typ == "ext" else 0.21, 1.0, 1.0),
            folder=folder, kind="arch")
    # collision proxies: one hidden box per closed stretch between openings (the kit walls only have complex-as-
    # simple collision; a plain box gives players, bots, bullets and the navmesh a clean simple wall)
    gaps = sorted((tc - (0.62 if kind == "door" else 1.0), tc + (0.62 if kind == "door" else 1.0))
                  for tc, half, kind, ref in pieces if kind in ("door", "arch", "open"))
    cur = t0 - 0.2
    for g0, g1 in gaps + [(t1 + 0.2, t1 + 0.2)]:
        if g0 - cur > 0.05:
            tc = (cur + g0) / 2.0
            x, y, yaw, nx, ny = piece_xf(orient, coord, tc, nsign, off)
            put(E + "Cube", x - 10.5 * nx, y - 10.5 * ny, z + 156.0, yaw, scale=(g0 - cur, 0.41, 3.12),
                folder=f"StormManor/Collision/{fid}", hidden=True, kind="proxy")
            stats["wall_proxies"] = stats.get("wall_proxies", 0) + 1
        cur = max(cur, g1)


def fill(pieces, a, b):
    """Solid pieces over [a, b] m: 2 m where possible, the last metre as a half piece."""
    t = a
    while b - t >= 1.99:
        pieces.append((t + 1.0, 1.0, "solid", None))
        t += 2.0
    if b - t >= 0.99:
        pieces.append((t + 0.5, 0.5, "solid", None))


def door(fid, orient, coord, tc, nsign, off, z, d, typ):
    """KGDoor in a kit doorway (hinge 61.5 cm off centre); the leaf never cuts the navmesh (bots open doors)."""
    if orient == "v":
        nx, ny = float(nsign), 0.0
        px, py = coord * M, tc * M
    else:
        nx, ny = 0.0, float(nsign)
        px, py = tc * M, coord * M
    tx, ty = -ny, nx
    o = off - 10.0
    hx, hy = px - 61.5 * tx + o * nx, py - 61.5 * ty + o * ny
    yaw = math.degrees(math.atan2(ny, nx))
    a = spawn_class("/Script/KillGodot.KGDoor", hx, hy, z + 3.0, yaw, folder="StormManor/Doors")
    if a:
        a.set_actor_label(f"Door_{d['id']}_{int(tc * 10)}")
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            comp.set_editor_property("can_ever_affect_navigation", False)
        stats["doors"] += 1
    DOOR_REG.append({"fid": fid, "x": px, "y": py, "nx": nx, "ny": ny, "rooms": [d["a"], d["b"]], "width": 2.0,
                     "kind": d["kind"], "id": d["id"]})


def rail_run(fid, orient, coord, t0, t1, nsign, z, folder, inset=18.0):
    """Stone balustrade on a floor edge (gallery over the hall, stair wells): 2 m sections + posts. The rail stands
    `inset` cm inside the floor edge (on the -normal side of the edge)."""
    bal = DT + "Balustrade_2m" if have(DT + "Balustrade_2m") else None
    post = DT + "Balustrade_Post" if have(DT + "Balustrade_Post") else IP + "RailPost"
    along_x = True
    if bal:
        bb = bounds(bal)
        along_x = (bb[3] - bb[0]) >= (bb[4] - bb[1])
    t = t0
    while t < t1 - 0.01:
        seg = min(2.0, t1 - t)
        x, y, yaw, nx, ny = piece_xf(orient, coord, t + seg / 2.0, nsign, -inset)
        if bal:
            put(bal, x, y, z + 1.0, yaw if along_x else yaw + 90.0,
                scale=(seg / 2.0, 1.0, 1.0) if along_x else (1.0, seg / 2.0, 1.0), folder=folder, kind="arch")
        else:
            put(IP + "Railing_1m", x, y, z + 1.0, yaw, scale=(seg, 1.0, 1.0), folder=folder, kind="arch")
        t += seg
    for tend in (t0, t1):
        x, y, yaw, nx, ny = piece_xf(orient, coord, tend, nsign, -inset)
        put(post, x, y, z + 1.0, 0.0, folder=folder, kind="arch")


def parapet_run(fid, orient, coord, t0, t1, nsign, z, room):
    """Low stone wall on a roof / terrace / garden edge (1.1 m): a kit wall cut down, the dressed face out; gaps where
    the layout has a door on this line (the sea stair gate)."""
    gaps = []
    for d in L["doors"]:
        if d["kind"] in ("gate",) and room in (d["a"], d["b"]):
            x, y = d["at"]
            tc = y if orient == "v" else x
            if (orient == "v" and abs(x - coord) < 0.01) or (orient == "h" and abs(y - coord) < 0.01):
                gaps.append((tc - d["width"] / 2.0, tc + d["width"] / 2.0))
    for s in L["stairs"]:
        if s["id"] == "st_sea" and room == "courtyard":
            gaps.append((s["bottom"][0] - s["width"] / 2.0, s["bottom"][0] + s["width"] / 2.0))
    folder = f"StormManor/Parapets/{fid}"
    pieces = []
    cur = t0
    for g0, g1 in sorted(gaps):
        if g1 <= t0 or g0 >= t1:
            continue
        fill(pieces, cur, max(cur, g0))
        cur = max(cur, g1)
    fill(pieces, cur, t1)
    for tc, half, kind, _ in pieces:
        x, y, yaw, nx, ny = piece_xf(orient, coord, tc, nsign, WALL_OUT)
        put(V + "Wall_UnevenBrick_Straight", x, y, z, yaw, scale=(half, 1.0, 0.36), folder=folder, kind="arch")
    for tend in (t0, t1):
        x, y, yaw, nx, ny = piece_xf(orient, coord, tend, nsign, WALL_OUT)
        put(V + "Wall_UnevenBrick_Straight", x, y, z, yaw, scale=(0.31, 1.0, 0.42), folder=folder, kind="arch")


def fence_run(fid, orient, coord, t0, t1, nsign, z, a, b):
    """Garden boundaries: wrought iron round the graveyard, a low wall between the courtyard and the service yard;
    gaps at the layout's gates."""
    gaps = []
    for d in L["doors"]:
        if {d["a"], d["b"]} == {a, b}:
            x, y = d["at"]
            tc = y if orient == "v" else x
            gaps.append((tc - d["width"] / 2.0, tc + d["width"] / 2.0))
    pieces = []
    cur = t0
    for g0, g1 in sorted(gaps):
        fill(pieces, cur, max(cur, g0))
        cur = max(cur, g1)
    fill(pieces, cur, t1)
    iron = "graveyard" in (a, b)
    for tc, half, kind, _ in pieces:
        x, y, yaw, nx, ny = piece_xf(orient, coord, tc, nsign, 0.0)
        if iron:
            put(V + "Prop_MetalFence_Ornament", x, y, z, yaw, scale=(half * 2.0 / 1.97, 1.0, 0.72),
                folder="StormManor/Fences", kind="arch")
        else:
            put(V + "Wall_UnevenBrick_Straight", x + nx * WALL_MID, y + ny * WALL_MID, z, yaw, scale=(half, 1.0, 0.36),
                folder="StormManor/Fences", kind="arch")
    for g0, g1 in gaps:
        for tg in (g0, g1):
            x, y, yaw, nx, ny = piece_xf(orient, coord, tg, nsign, 0.0)
            put(V + "Corner_ExteriorWide_Brick", x, y, z, 0.0, scale=(0.8, 0.8, 0.55), folder="StormManor/Fences",
                kind="arch")


# =================================================================================================== floors + roofs
FLOOR_MAT = {"great_hall": "Floor_Brick", "vestibule": "checker", "chapel": "Floor_RedBrick", "dining": "Floor_WoodDark",
             "kitchen": "Floor_Brick", "pantry": "Floor_Brick", "laundry": "Floor_RedBrick", "library": "Floor_WoodDark",
             "ballroom": "Floor_WoodLight", "servants_corridor": "Floor_WoodDark", "bath": "checker",
             "gallery": "Floor_WoodDark", "corridor_w": "Floor_WoodDark", "corridor_e": "Floor_WoodDark",
             "blue_room": "Floor_WoodLight", "red_room": "Floor_WoodDark", "studio": "Floor_WoodLight",
             "nursery": "Floor_WoodLight", "study": "Floor_WoodDark", "master": "Floor_WoodDark",
             "billiard": "Floor_WoodDark", "attic": "Floor_WoodLight", "clock_room": "Floor_WoodDark",
             "storm_tower": "Floor_UnevenBrick", "greenhouse": "Floor_RedBrick", "boathouse": "Floor_WoodDark",
             "storm_terrace": "Floor_UnevenBrick", "roof_walk": "Floor_WoodDark", "tower_top": "Floor_UnevenBrick"}
DECKS = {f: set() for f in G.ORDER}
FLOOR_CELLS = {f: set() for f in G.ORDER}
CHIMNEY_ROOMS = ("great_hall", "library", "kitchen", "red_room", "master", "dining")


def build_floors():
    for fid in G.ORDER:
        z = fz(fid)
        for c, rid in GRID.occ[fid].items():
            if GRID.is_void(fid, c) or c in GRID.holes[fid]:
                continue
            r = R[rid]
            if not (G.enclosed(r) or r["kind"] == "outdoor"):
                continue
            tile = FLOOR_MAT.get(rid, "Floor_UnevenBrick" if fid == "C" else "Floor_WoodDark")
            if tile == "checker":
                tile = "Floor_Brick" if (c[0] + c[1]) % 2 else "Floor_RedBrick"
            put(V + tile, c[0] * 200.0 + 100.0, c[1] * 200.0 + 100.0, z, 0.0, folder=f"StormManor/Floors/{fid}",
                kind="arch")
            stats["floor_tiles"] += 1
            FLOOR_CELLS[fid].add(c)
    # ceilings: the floor above covers it, else a roof deck (the ceiling seen from below, the roof from above)
    for fi, fid in enumerate(G.ORDER[:-1]):
        up = G.ORDER[fi + 1]
        for c in GRID.occ[fid]:
            if not enclosed_at(fid, c):
                continue
            if fid == "F0" and GRID.room_at("F0", c)["id"] == "great_hall" and GRID.is_void("F1", c):
                continue
            if c in GRID.occ[up] or c in GRID.holes[up]:
                continue
            rid = rid_at(fid, c)
            if rid == "greenhouse":
                continue
            put(V + "Floor_UnevenBrick", c[0] * 200.0 + 100.0, c[1] * 200.0 + 100.0, fz(up), 0.0,
                folder=f"StormManor/Roofs/{up}", kind="arch")
            DECKS[up].add(c)
    # collision proxies under the kit tiles (complex-as-simple only, 2 cm thin; see wall_run): merged hidden slabs
    for fid in G.ORDER:
        for i0, j0, i1, j1 in rects_from_cells(FLOOR_CELLS[fid] | DECKS[fid]):
            put(E + "Cube", (i0 + i1 + 1) * 100.0, (j0 + j1 + 1) * 100.0, fz(fid) + 1.0 - 10.0, 0.0,
                scale=((i1 - i0 + 1) * 2.0, (j1 - j0 + 1) * 2.0, 0.2), folder=f"StormManor/Collision/{fid}",
                hidden=True, kind="proxy")
            stats["floor_proxies"] = stats.get("floor_proxies", 0) + 1
    gx0, gy0, gx1, gy1 = G.rect_of(R["greenhouse"]["poly"])
    box(gx0, gy0, gx1, gy1, fz("F1") + 2.0, 4.0, "Glass", "StormManor/Roofs/Glass")
    for k in range(int(gx0) + 2, int(gx1), 2):      # glazing bars (a kit post laid along -Y from the south edge)
        put(V + "Corner_Exterior_Wood", k * M, gy1 * M - 10.0, fz("F1") - 8.0, 90.0, pitch=90.0,
            scale=(0.4, 0.4, (gy1 - gy0 - 0.2) / 3.0), folder="StormManor/Roofs/Glass", collide=False, kind="arch")
    # parapets round the roof decks (building edge only)
    for fid in G.ORDER[1:]:
        lower = G.ORDER[G.ORDER.index(fid) - 1]
        deck = DECKS[fid]
        for c in deck:
            for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nb = (c[0] + d[0], c[1] + d[1])
                if nb in deck or nb in GRID.occ[fid] or enclosed_at(lower, nb):
                    continue
                orient = "v" if d[0] else "h"
                coord = (c[0] + (1 if d[0] > 0 else 0)) * 2.0 if d[0] else (c[1] + (1 if d[1] > 0 else 0)) * 2.0
                t0 = (c[1] if d[0] else c[0]) * 2.0
                parapet_run(fid, orient, coord, t0, t0 + 2.0, 1 if (d[0] > 0 or d[1] > 0) else -1, fz(fid), "")
    # chimneys over the hearth rooms, on their top deck
    for rid in CHIMNEY_ROOMS:
        r = R[rid]
        cells = G.cells_of(r["poly"])
        best = None
        for fid in G.ORDER:
            for c in cells:
                if c in DECKS[fid]:
                    best = (fid, c)
        if best:
            fid, c = best
            put(V + "Prop_Chimney", c[0] * 200.0 + 100.0, c[1] * 200.0 + 100.0, fz(fid), 0.0, scale=(1.3, 1.3, 1.1),
                folder="StormManor/Roofs/Chimneys", kind="prop")


def pitched_roof(rid, ridge_axis):
    """Scaled kit round-tile roof + brick gables over a rectangular top room (visual; the deck below is the ceiling)."""
    x0, y0, x1, y1 = G.rect_of(R[rid]["poly"])
    fid = R[rid]["floor"]
    z = fz(G.ORDER[G.ORDER.index(fid) + 1]) + 2.0
    cx, cy = (x0 + x1) * 50.0, (y0 + y1) * 50.0
    if ridge_axis == "x":
        span, length, yaw = (y1 - y0), (x1 - x0), 90.0
    else:
        span, length, yaw = (x1 - x0), (y1 - y0), 0.0
    sx = (span + 2.9) / 10.9
    sy = (length + 2.0) / 16.05
    sz = min(1.25, 0.5 + span / 20.0)
    put(V + "Roof_RoundTiles_8x14", cx, cy, z, yaw, scale=(sx, sy, sz), folder="StormManor/Roofs/Pitched", kind="prop")
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for sgn, gy in ((-1, 0.0), (1, 180.0)):
        ly = sgn * length * 50.0
        put(V + "Roof_Front_Brick8", cx - ly * s, cy + ly * c, z, yaw + gy, scale=(span / 8.0, 1.0, sz),
            folder="StormManor/Roofs/Pitched", kind="prop")


# =================================================================================================== stairs
STAIR_REG = []


def build_stairs():
    for s in L["stairs"]:
        lo, up = R[s["lower"]]["floor"], R[s["upper"]]["floor"]
        z0, z1 = fz(lo), fz(up)
        u, cells, rect = G.stair_geom(s)
        bx, by = s["bottom"][0] * M, s["bottom"][1] * M
        folder = f"StormManor/Stairs/{s['id']}"
        STAIR_REG.append({"id": s["id"], "lower": s["lower"], "upper": s["upper"], "rect": rect, "u": u,
                          "bottom": s["bottom"], "top": s["top"], "z0": z0, "z1": z1, "kind": s["kind"]})
        if s["kind"] == "ladder":
            ladder_tower(s, z0, z1)
            continue
        run = math.hypot(s["top"][0] - s["bottom"][0], s["top"][1] - s["bottom"][1]) * M
        yaw_mesh = math.degrees(math.atan2(u[0], -u[1]))        # kit stairs climb towards their -Y
        yaw_u = math.degrees(math.atan2(u[1], u[0]))
        rise = z1 - z0
        if s["kind"] == "outdoor":
            n = int(round(rise / 100.0))
            step_len = run / n
            for across in ([-100.0, 100.0] if s["width"] >= 4 else [0.0]):
                for k in range(n):
                    along = step_len * (k + 0.5)
                    x = bx + u[0] * along - u[1] * across
                    y = by + u[1] * along + u[0] * across
                    put(V + "Stairs_Exterior_Straight", x, y, z0 + k * 100.0, yaw_mesh,
                        scale=(1.0, step_len / 208.0, 1.0), folder=folder, nav=False, kind="stairs")
            ramp(bx, by, u, yaw_u, run, rise, z0, s["width"], folder, 0.0)
        else:
            k = run / 443.0
            put(V + "Stair_Interior_Solid", bx, by, z0 + 1.0, yaw_mesh, scale=(1.0, k, rise / 300.0), folder=folder,
                nav=False, kind="stairs")
            for mirror in (1.0, -1.0):          # the kit rail sits on the stair's open side; mirrored for the other
                put(IP + "StairRail", bx, by, z0 + 1.0, yaw_mesh, scale=(mirror, k, rise / 300.0),
                    folder=folder, collide=False, kind="stairs")
            tread = 385.0 * k
            ramp(bx, by, u, yaw_u, tread, rise, z0, 1.8, folder, 0.0)
            # landing box over the last stretch (hidden, walkable)
            lx, ly = bx + u[0] * (tread + (run - tread) / 2.0), by + u[1] * (tread + (run - tread) / 2.0)
            put(E + "Cube", lx, ly, z1 - 9.0, yaw_u, scale=((run - tread) / 100.0 + 0.1, 1.8, 0.2), folder=folder,
                hidden=True, kind="ramp")
        stats["stairs"] += 1
        well_rails(s, up, cells, u)


def ramp(bx, by, u, yaw_u, run, rise, z0, width, folder, lift):
    """Hidden collision ramp over a stair (smooth walking, and the navmesh follows it; the stair mesh doesn't)."""
    hyp = math.hypot(run, rise)
    pitch = math.degrees(math.atan2(rise, run))
    mx, my = bx + u[0] * run / 2.0, by + u[1] * run / 2.0
    zm = z0 + rise / 2.0 + 4.0 - 10.0 / math.cos(math.radians(pitch)) + lift
    put(E + "Cube", mx, my, zm, yaw_u, pitch=pitch, scale=(hyp / 100.0 + 0.3, width, 0.2),
        folder=folder + "/Ramp", hidden=True, kind="ramp")


def well_rails(s, up, cells, u):
    """Balustrade round a stair well on the upper floor, open at the top end (where people step off)."""
    hole = set(cells)
    top_c = (int(math.floor((s["top"][0] - u[0] * 0.5) / 2.0)), int(math.floor((s["top"][1] - u[1] * 0.5) / 2.0)))
    upper = s["upper"]
    for c in hole:
        for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nb = (c[0] + d[0], c[1] + d[1])
            if nb in hole:
                continue
            if GRID.occ[up].get(nb) != upper or GRID.is_void(up, nb):
                continue                                   # a wall (or nothing) on that side
            if c == top_c and (d[0], d[1]) == (int(round(u[0])), int(round(u[1]))):
                continue                                   # the exit
            orient = "v" if d[0] else "h"
            coord = (c[0] + (1 if d[0] > 0 else 0)) * 2.0 if d[0] else (c[1] + (1 if d[1] > 0 else 0)) * 2.0
            t0 = (c[1] if d[0] else c[0]) * 2.0
            nsign = 1 if (d[0] > 0 or d[1] > 0) else -1
            # the rail stands on the floor beside the well: inset towards the neighbour (the floor)
            rail_run(up, orient, coord, t0, t0 + 2.0, -nsign, fz(up), f"StormManor/Stairs/{s['id']}/Well", inset=18.0)


def ladder_tower(s, z0, z1):
    """KGLadder against the tower's east wall up through a hatch to the tower top (climber faces +X)."""
    x, y = s["bottom"][0] * M + 55.0, s["bottom"][1] * M
    height = z1 - z0 + 120.0
    lad = spawn_class("/Script/KillGodot.KGLadder", x, y, z0 + 1.0, 0.0, folder="StormManor/Ladders")
    if lad:
        lad.set_height(height)
    for side in (-28.0, 28.0):
        put(V + "Corner_Exterior_Wood", x, y + side, z0, 0.0, scale=(0.35, 0.35, height / 300.0 + 0.05),
            folder="StormManor/Ladders", kind="arch")
    rung = 30.0
    while rung < height:
        put(V + "Corner_Exterior_Wood", x, y - 28.0, z0 + rung, 0.0, roll=-90.0, scale=(0.25, 0.25, 0.19),
            folder="StormManor/Ladders", collide=False, kind="arch")
        rung += 38.0


# =================================================================================================== gameplay
def meeting_and_starts():
    mx, my = L["meeting"]["at"]
    z = fz("F0")
    g = _real.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(mx * M, my * M, z + 100.0))
    g.tags = ["KG_Gallows"]
    g.set_actor_label("KG_Gallows")
    g.set_folder_path("Gameplay")
    hub = _real.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector((mx - 4.0) * M, my * M, z + 60.0))
    hub.tags = ["KG_BotHub"]
    hub.set_actor_label("KG_BotHub")
    hub.set_folder_path("Gameplay")
    sp = L["spawn"]
    n = int(sp["count"])
    radius = 7.0
    for k in range(n):
        a = 2.0 * math.pi * (k + 0.5) / n
        x, y = (sp["center"][0] + radius * math.cos(a)) * M, (sp["center"][1] + radius * math.sin(a)) * M
        st = _real.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, z + 100.0))
        st.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(a) + 180.0), False)
        st.set_folder_path("Gameplay/Starts")
    stats["starts"] = n
    B.plain["F0"] += n + 2


BOT_AT = {"gallery": (-8.5, -10.0)}      # m; the gallery is a 3 m ring round the hall void: its west walk


def bot_spots():
    """One KG_BotSpot at the middle of every walkable room / garden (bots roam the whole manor, every floor)."""
    n = 0
    for r in L["rooms"]:
        cells = [c for c in G.cells_of(r["poly"]) if c not in GRID.holes[r["floor"]] and c not in GRID.stair_cells[r["floor"]]
                 and not GRID.is_void(r["floor"], c)]
        if not cells:
            continue
        cx = sum(c[0] for c in cells) / len(cells)
        cy = sum(c[1] for c in cells) / len(cells)
        # the most central cell whose middle no blocking prop covers (a spot inside a table is unreachable)
        fid = r["floor"]
        solid = [(q["x"], q["y"], 0.5 * max((bounds(q["m"])[3] - bounds(q["m"])[0]) * q["s"][0],
                                            (bounds(q["m"])[4] - bounds(q["m"])[1]) * q["s"][1]))
                 for q in B.recs if q.get("room") == r["id"] and q["kind"] == "prop" and q["collide"]
                 and fid_of_z(q["z"]) == fid]

        def clear(c):
            px, py = c[0] * 200.0 + 100.0, c[1] * 200.0 + 100.0
            return all(math.hypot(px - x, py - y) > rad + 70.0 for x, y, rad in solid)
        cs = set(cells)
        inner = [c for c in cells if all((c[0] + di, c[1] + dj) in cs for di in (-1, 0, 1) for dj in (-1, 0, 1))]
        free = [c for c in inner if clear(c)] or [c for c in cells if clear(c)] or cells
        best = min(free, key=lambda c: (c[0] - cx) ** 2 + (c[1] - cy) ** 2)
        bx, by = BOT_AT.get(r["id"], (best[0] * 2.0 + 1.0, best[1] * 2.0 + 1.0))
        t = _real.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(bx * M, by * M, fz(r["floor"]) + 60.0))
        t.tags = ["KG_BotSpot"]
        t.set_actor_label(f"BotSpot_{r['id']}")
        t.set_folder_path("Gameplay/BotSpots")
        n += 1
    stats["bot_spots"] = n


SECRET_LOOK = {"S1": ("Rotating bookcase: pull the red book", "Squeeze out of the confessional"),
               "S2": ("Climb into the dumbwaiter", "Ride the dumbwaiter down"),
               "S3": ("Swing Anselm's portrait open", "Climb down behind the clock face"),
               "S4": ("Slide the tomb lid", "Push the false wine rack"),
               "S5": ("Climb down the old well", "Climb the iron rungs"),
               "S6": ("Crawl in behind the wardrobe", "Crawl back between the walls")}


def secrets():
    """AKGPassage pairs (E at one end: travel to the other, door creak at both ends)."""
    n = 0
    for s in L["secrets"]:
        ends = []
        for end in ("a", "b"):
            rid = s[end]
            fid = R[rid]["floor"]
            x, y = s["at_" + end]
            cells = G.cells_of(R[rid]["poly"])
            cx = sum(c[0] * 2 + 1 for c in cells) / len(cells)
            cy = sum(c[1] * 2 + 1 for c in cells) / len(cells)
            yaw = math.degrees(math.atan2(cy - y, cx - x))
            ends.append((rid, fid, x, y, yaw))
        for k, (rid, fid, x, y, yaw) in enumerate(ends):
            me, other = f"{s['id']}_{'ab'[k]}", f"{s['id']}_{'ab'[1 - k]}"
            p = spawn_class("/Script/KillGodot.KGPassage", x * M, y * M, fz(fid) + 2.0, yaw, folder="StormManor/Secrets")
            if not p:
                continue
            p.set_actor_label(f"Secret_{me}_{rid}")
            p.set_editor_property("passage_id", me)
            p.set_editor_property("target_id", other)
            p.set_editor_property("prompt", unreal.Text(SECRET_LOOK[s["id"]][k]))
            p.set_editor_property("hitbox_extent", unreal.Vector(55.0, 55.0, 100.0))
            p.set_editor_property("hitbox_offset", unreal.Vector(0.0, 0.0, 100.0))
            p.set_editor_property("arrival_local", unreal.Vector(130.0, 0.0, 100.0))
            p.set_editor_property("arrival_yaw", 0.0)
            p.set_editor_property("sound_name", "S_Passage_Ladder" if s["id"] in ("S2", "S5") else "S_Passage_Door")
            p.tags = ["KG_Secret", s["id"]]
            n += 1
    stats["passages"] = n


PANELS = [("FileReports", "File Pozzo's letters", "study", (12.0, -22.0)),
          ("PostNotice", "Post the storm notice", "vestibule", (-8.0, 2.0)),
          ("LightCandles", "Light the chapel candles", "chapel", (20.0, -13.0)),
          ("TendGraves", "Tend the family graves", "graveyard", (-45.0, 14.0)),
          ("BakeBread", "Bake bread", "kitchen", (-25.0, -21.0)),
          ("PourAle", "Draw ale in the cellar", "wine_cellar", (-20.0, -8.0)),
          ("MendNets", "Mend the nets", "boathouse", (9.0, 41.0)),
          ("SharpenTools", "Sharpen the tools", "service_yard", (-20.0, 12.0)),
          ("FeedAnimals", "Feed the hens", "service_yard", (-14.0, 30.0)),
          ("ForgeNails", "Mend the spark coils", "spark_room", (-8.5, -6.5))]
PANEL_SECS = {"FileReports": 9.5, "PostNotice": 5.7, "LightCandles": 9.0, "TendGraves": 9.0, "BakeBread": 11.5,
              "PourAle": 6.5, "MendNets": 4.4, "SharpenTools": 9.0, "FeedAnimals": 7.5, "ForgeNails": 9.5}


def panel_stations():
    """SPRINT-014 minigame stations (the 30 % panel share of the deal; 'replaces' keeps pairs apart)."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGTaskStation")
    for tid, name, rid, (x, y) in PANELS:
        st = _real.spawn_actor_from_class(cls, unreal.Vector(x * M, y * M, fz(R[rid]["floor"]) + 2.0))
        st.set_editor_property("task_id", tid)
        st.set_editor_property("task_name", name)
        st.set_editor_property("work_seconds", PANEL_SECS[tid])
        st.set_actor_label(f"Task_{tid}")
        st.set_folder_path("Gameplay/Tasks")
        B.plain[R[rid]["floor"]] += 1
        stats["panel_stations"] += 1


def navigation():
    lo = (-62.0, -34.0, (fz("C") - 250.0) / 100.0)
    hi = (62.0, 58.0, (fz("F3") + 400.0) / 100.0)
    c = [(lo[i] + hi[i]) / 2.0 * M for i in range(3)]
    e = [(hi[i] - lo[i]) / 2.0 for i in range(3)]
    vol = _real.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(*c))
    vol.set_actor_scale3d(unreal.Vector(*e))
    vol.set_folder_path("Gameplay")
    stats["nav_bounds_m"] = [lo, hi]
    # the RecastNavMesh actor must exist before ResavePackages -BuildNavigationData (else it builds nothing: the
    # step-3 log said "Unable to find RecastNavMesh" and the probe found 0 paths); same as kg_build_village_v2
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception as ex:
        log(f"nav rebuild failed: {ex}")
    navs = [a for a in _real.get_all_level_actors() if a.get_class().get_name() == "RecastNavMesh"]
    stats["navmesh_actors"] = len(navs)
    log(f"nav: RecastNavMesh actors after rebuild: {len(navs)}")


def capture_camera():
    cam = _real.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0.0, 0.0, 5000.0))
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_label("KG_CaptureCam")
    cam.tags = ["KG_CaptureCam"]
    cam.set_folder_path("Gameplay/Dev")


def ambient(sound, x, y, z, volume=1.0, attenuate=True):
    snd = unreal.load_asset(AUDIO + sound) if unreal.EditorAssetLibrary.does_asset_exist(AUDIO + sound) else None
    if not snd:
        return
    a = _real.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(x, y, z))
    comp = a.get_component_by_class(unreal.AudioComponent)
    comp.set_sound(snd)
    comp.set_editor_property("volume_multiplier", volume)
    if attenuate and unreal.EditorAssetLibrary.does_asset_exist(AUDIO + "SA_KG_Ambience"):
        comp.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO + "SA_KG_Ambience"))
    else:
        comp.set_editor_property("allow_spatialization", False)
    a.set_folder_path("StormManor/Audio")
    stats["sounds"] = stats.get("sounds", 0) + 1


def soundscape():
    ambient("A_Wind_Loop", 0, 0, fz("F2"), 0.55, attenuate=False)
    for x, y in ((-5500, 1000), (5500, 1000), (0, 5600), (0, -3200), (-3000, 4800), (3500, 4800)):
        ambient("A_Sea_Loop", x, y, 200, 1.0)
    for rid, at in (("great_hall", (250, -1880)), ("library", (3300, -2000)), ("kitchen", (-3100, -1900))):
        ambient("A_Fire_Loop", at[0], at[1], fz(R[rid]["floor"]) + 80, 0.8)
    ambient("A_Lapping_Loop", 0, 4600, fz("C"), 0.9)


def minimap():
    """Texture + regions from the JSON (Tools/Level/render_stormmanor_minimap.py) -> AKGMapInfo."""
    import subprocess
    import kg_make_minimap
    importlib.reload(kg_make_minimap)
    exe = kg_make_minimap._system_python()
    if exe:
        res = subprocess.run([exe, f"{ROOT}/Tools/Level/render_stormmanor_minimap.py"], capture_output=True, text=True,
                             timeout=600, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for line in (res.stdout or "").splitlines()[-4:]:
            log(f"minimap: {line}")
        if res.returncode:
            log(f"minimap render failed: {(res.stderr or '')[-800:]}")
    info = kg_make_minimap.build_minimap(f"{ROOT}/Tools/Level/stormmanor_layout.json", do_render=False)
    info.set_editor_property("map_title", unreal.Text("Storm Manor"))
    stats["minimap_regions"] = len(info.get_editor_property("regions"))


# =================================================================================================== report
def precheck():
    """Placement pre-check on the recorded props (the -game probe does the traced check): nothing inside a door
    apron (1.3 x 1.1 m each side) or a stair footprint (+0.6 m at the bottom), nothing floating in the air."""
    bad = []
    aprons = []
    for d in DOOR_REG:
        tx, ty = -d["ny"], d["nx"]
        aprons.append((d, d["x"], d["y"], tx, ty))
    stairs = []
    for s in STAIR_REG:
        x0, y0, x1, y1 = s["rect"]
        ux, uy = s["u"]
        ex0 = x0 - (0.6 if ux > 0.5 else 0.0) - (0.0)
        ex1 = x1 + (0.6 if ux < -0.5 else 0.0)
        ey0 = y0 - (0.6 if uy > 0.5 else 0.0)
        ey1 = y1 + (0.6 if uy < -0.5 else 0.0)
        stairs.append((s, ex0 * M, ey0 * M, ex1 * M, ey1 * M))
    for r in B.recs:
        if r["kind"] not in ("prop",) or not r["collide"] or not r.get("room"):
            continue
        b = bounds(r["m"])
        h = (b[5] - b[2]) * r["s"][2]
        if h < 25.0:
            continue
        fid = fid_of_z(r["z"])
        rad = 0.5 * max((b[3] - b[0]) * r["s"][0], (b[4] - b[1]) * r["s"][1])
        for d, x, y, tx, ty in aprons:
            if d["fid"] != fid:
                continue
            dx, dy = r["x"] - x, r["y"] - y
            along = abs(dx * tx + dy * ty)
            across = abs(dx * d["nx"] + dy * d["ny"])
            if along < 65.0 + rad * 0.7 and across < 110.0 + rad * 0.7:
                bad.append({"why": "door", "m": r["m"].split("/")[-1], "at": [round(r["x"]), round(r["y"]), round(r["z"])],
                            "door": d["id"]})
                break
        for s, x0, y0, x1, y1 in stairs:
            if fid not in (fid_of_z(s["z0"]), fid_of_z(s["z1"])):
                continue
            if x0 - rad * 0.7 < r["x"] < x1 + rad * 0.7 and y0 - rad * 0.7 < r["y"] < y1 + rad * 0.7 and fid == fid_of_z(s["z0"]):
                bad.append({"why": "stair", "m": r["m"].split("/")[-1], "at": [round(r["x"]), round(r["y"]), round(r["z"])],
                            "stair": s["id"]})
                break
    stats["precheck_bad"] = len(bad)
    return bad


MOUNTED = ("Banner", "Lantern_Wall", "Peg_Rack", "Shelf_Simple", "Shelf_Arch", "HerbRack", "HerbBundle", "Chandelier",
           "ClockFace", "ClockDial", "ClockHand", "Shield_Wooden", "Rope_", "Lifebuoy", "Ladder_4m", "Pouch_Large",
           "Corner_Exterior_Wood", "/Cube", "/Plane", "/Sphere", "Rug_", "RockPath", "LaundryLine", "Torch", "Prop_Chimney",
           "Planks", "Chain_Coil", "Axe_", "Sword_", "Cannon_Ball", "WellMouth", "Rowboat",
           # external kits hung on walls / from ceilings by kg_sm_dress (ext layer)
           "pictureframe_large", "pictureframe_medium", "pictureframe_small", "Cobweb", "_banner_", "torch_mounted",
           "bathroomMirror")


def dump_props():
    """The dressing props (room set) for the -game placement probe (Tools/Unreal/kg_capture_stormmanor.py)."""
    out = []
    for r in B.recs:
        if not r.get("room") or r["kind"] not in ("prop",):
            continue
        b = bounds(r["m"])
        fid = fid_of_z(r["z"])
        floor_top = fz(fid) + 1.0
        bottom = r["z"] + b[2] * r["s"][2]
        tipped = abs(r["pitch"]) > 5.0 or abs(r["roll"]) > 5.0
        mounted = any(k in r["m"] for k in MOUNTED) or ("/Shelf_" in r["m"] and bottom > floor_top + 30.0)
        out.append({"m": r["m"].split("/")[-1], "x": round(r["x"], 1), "y": round(r["y"], 1), "z": round(r["z"], 1),
                    "yaw": round(r["yaw"], 1), "s": [round(v, 3) for v in r["s"]], "b": [round(v, 1) for v in b],
                    "room": r["room"], "floor": floor_top, "hism": bool(r.get("hism")), "label": r.get("label"),
                    "collide": bool(r["collide"]),
                    "floor_prop": (not mounted) and abs(bottom - floor_top) < 30.0,
                    "check": (not mounted) and (not tipped) and r["s"][0] > 0 and (b[5] - b[2]) * r["s"][2] > 3.0})
    with open(f"{ROOT}/Saved/KG_SM_Props.json", "w") as f:
        json.dump(out, f)
    stats["props_dumped"] = len(out)


def report(bad):
    dump_props()
    out = {"level": LEVEL, "secs": round(time.time() - T0, 1), "stats": {k: v for k, v in stats.items() if k != "missing"},
           "plain_actors_per_floor": B.plain, "instances": B.instances, "mesh_records": len(B.recs),
           "missing_meshes": sorted(stats.get("missing", set())), "precheck": bad[:200],
           "doors": DOOR_REG, "windows": len(WIN_REG), "stairs": STAIR_REG,
           "rooms_dressed": getattr(DRESS, "DRESSED", {}) if DRESS else {}}
    with open(REPORT, "w") as f:
        json.dump(out, f, indent=1, default=str)
    log(f"report -> {REPORT}: plain {json.dumps(B.plain)}, instances {B.instances}, lights {stats['lights']} "
        f"(hero {stats['hero_lights']}), doors {stats['doors']}, windows {stats['windows']}, stairs {stats['stairs']}, "
        f"passages {stats['passages']}, precheck bad {len(bad)}, missing {len(out['missing_meshes'])}")


def step(name, fn, *a):
    t = time.time()
    try:
        fn(*a)
        log(f"step {name}: {time.time() - t:.1f}s, records {len(B.recs)}")
    except Exception:
        import traceback
        log(f"step {name} FAILED:\n{traceback.format_exc()}")
        stats.setdefault("failed_steps", []).append(name)


DRESS = None


def dress():
    global DRESS
    import kg_interiors
    importlib.reload(kg_interiors)
    kg_interiors._actors = SHIM
    import kg_sm_dress
    importlib.reload(kg_sm_dress)
    DRESS = kg_sm_dress
    kg_sm_dress.run(_Here(), kg_interiors)


class _Here:
    """This script's globals as a module-like object (a commandlet runs it as a script, not an importable module)."""

    def __getattr__(self, name):
        return globals()[name]


def build():
    global MAT
    new_level()
    MAT = flat_materials()
    step("sky", storm_sky)
    step("island", island_and_sea)
    step("floors", build_floors)
    step("walls", build_walls)
    step("roofs", lambda: [pitched_roof("attic", "x"), pitched_roof("clock_room", "x"), pitched_roof("boathouse", "x")])
    step("stairs", build_stairs)
    step("dress", dress)
    step("secrets", secrets)
    step("panels", panel_stations)
    step("meeting", meeting_and_starts)
    step("botspots", bot_spots)
    step("sound", soundscape)
    step("capture_cam", capture_camera)
    step("flush", B.flush)
    for r in B.recs:
        if r.get("bounds_scale"):
            pass
    step("minimap", minimap)
    step("nav", navigation)
    bad = precheck()
    report(bad)
    saved = level_sub.save_current_level()
    log(f"saved {LEVEL}: {saved} in {time.time() - T0:.0f}s")


if os.environ.get("KG_SM_NOBUILD") != "1":
    build()
