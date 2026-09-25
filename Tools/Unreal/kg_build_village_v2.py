"""Build /Game/KillGodot/Maps/L_Morrowmere_v2 ("The Amphitheatre Cove") from Tools/Level/morrowmere_layout_v2.json.
L_Morrowmere and Tools/Unreal/kg_build_village.py stay untouched (their helpers are reused read-only, see V1 below).

Pipeline (all headless; the editor may be closed):
  1. blender --background --factory-startup --python Tools/Blender/kg_build_terrain_v2.py -- Art/Packed/KG_Terrain_v2.glb
  2. UnrealEditor-Cmd.exe KillGodot.uproject -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_terrain_v2.py harbour"
  3. python Tools/Level/prep_v2_placements.py                     (numpy + shapely -> Art/Packed/KG_V2_Placements.json)
  4. UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript
         -Script="D:/Kill Godot/Tools/Unreal/kg_build_village_v2.py" -unattended -nosplash -nopause -nullrhi
Live editor alternative for step 4: python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_build_village_v2.py --timeout 1800

Plan: Docs/Level/Morrowmere_v2_Plan.md section 11.2. Deterministic; re-running rebuilds the level from scratch.
Headless notes: EditorActorSubsystem.spawn_actor_from_object spawns nothing inside a commandlet, so every static mesh is
spawned as a StaticMeshActor from its class (ActorShim; kg_interiors gets the same shim). Navigation is rebuilt when the
editor opens the map (RebuildNavigation is attempted here, see the log line KG_V2 nav).
NOTE: unreal.Rotator positional order is (roll, pitch, yaw) and unreal.Color is BGRA: always use keywords.
"""
import importlib
import json
import math
import random
import sys
import time

import unreal

TOOLS = "D:/Kill Godot/Tools/Unreal"
sys.path.insert(0, TOOLS)
sys.path.insert(0, "D:/Kill Godot/Tools/Level")
import kg_archetypes_v2 as ARCH  # noqa: E402  (SPRINT-022 house archetypes, pure Python)
LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
# SPRINT-022 test builds: KG_V2_LEVEL=/Game/KillGodot/Maps/Test/<name> builds a copy (while the editor has the real map
# open); KG_V2_TEST=1 also skips everything that writes shared assets (district MI saves, minimap, underground hook).
LEVEL = __import__("os").environ.get("KG_V2_LEVEL") or LEVEL
TEST_BUILD = __import__("os").environ.get("KG_V2_TEST") == "1"
LAYOUT = json.load(open("D:/Kill Godot/Tools/Level/morrowmere_layout_v2.json", encoding="utf-8"))
PL = json.load(open("D:/Kill Godot/Art/Packed/KG_V2_Placements.json"))
HM = json.load(open("D:/Kill Godot/Art/Packed/KG_Terrain_v2_heights.json"))
REPORT = "D:/Kill Godot/Saved/KG_V2_BuildReport.json"
TERRAIN_DIR = "/Game/KillGodot/Env/Terrain/KG_Terrain_v2/StaticMeshes/"
TERRAIN_TILES = ["SM_KG_TerrainV2_Core_00", "SM_KG_TerrainV2_Core_01", "SM_KG_TerrainV2_Core_10", "SM_KG_TerrainV2_Core_11",
                 "SM_KG_TerrainV2_Outer"]
M = 100.0
FLOOR_H = 300.0
PREFIX = {
    "V": "/Game/KillGodot/Env/KG_Village/StaticMeshes/",
    "N": "/Game/KillGodot/Env/KG_Nature/StaticMeshes/",
    "P": "/Game/KillGodot/Env/KG_Props/StaticMeshes/",
    "JP": "/Game/KillGodot/Env/JapanC3/KG_JapanProps_Clean2/StaticMeshes/SM_KG_",
    "PIR": "/Game/KillGodot/Env/PirateC3/KG_PirateProps_Clean2/StaticMeshes/SM_KG_Pirate_",
    "DV": "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_",
    "DW": "/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_",
    "DH": "/Game/KillGodot/Env/Dress/KG_DressHarbour_Clean/StaticMeshes/SM_KG_",
    "WP": "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_",
    "DT": "/Game/KillGodot/Env/Dress/KG_DressTerrace_Clean/StaticMeshes/SM_KG_",   # plan 11.4 props (kg_make_dress_terrace.py)
    "DL": "/Game/KillGodot/Env/Dress/KG_DressLandmarks_Clean/StaticMeshes/SM_KG_",   # SPRINT-022 landmarks + cliffs
    "E": "/Engine/BasicShapes/",
}
V, N, P, JP, PIR, DV, DW, DH, WP, DT = (PREFIX[k] for k in ("V", "N", "P", "JP", "PIR", "DV", "DW", "DH", "WP", "DT"))
AUDIO = "/Game/KillGodot/Audio/"
KIT_MATS = "/Game/KillGodot/Env/KG_Village/Materials/"
DISTRICT_MATS = "/Game/KillGodot/Env/KG_Village/Materials/District/"

T0 = time.time()
rng = random.Random(2026)
stats = {"pieces": 0, "houses": 0, "shells": 0, "civic": 0, "towers": 0, "lights": 0}
_real_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log(f"KG_V2 {msg}")
    print(f"KG_V2 {msg}")


# =================================================================================================== headless spawning
class ActorShim:
    """EditorActorSubsystem stand-in: spawn_actor_from_object(StaticMesh) -> StaticMeshActor from class (works in
    commandlets); everything else is forwarded."""

    def __init__(self, real):
        self._real = real

    def spawn_actor_from_object(self, obj, loc, rot=None):
        rot = rot or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
        if isinstance(obj, unreal.StaticMesh):
            a = self._real.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            if a:
                a.static_mesh_component.set_static_mesh(obj)
            return a
        return self._real.spawn_actor_from_object(obj, loc, rot)

    def __getattr__(self, name):
        return getattr(self._real, name)


actors = ActorShim(_real_actors)


def v1_helpers():
    """kg_build_village.py's helpers without running its build (same trick as kg_refurnish.py)."""
    src = open(f"{TOOLS}/kg_build_village.py", encoding="utf-8").read()
    src = src[:src.index("\nnew_level()\n")]
    ns = {"__name__": "kg_builder_v1_helpers"}
    exec(compile(src, "kg_build_village_v1_helpers", "exec"), ns)
    return ns


V1 = v1_helpers()
import kg_interiors  # noqa: E402  (already imported/reloaded by the v1 helpers)
kg_interiors._actors = actors


# =================================================================================================== terrain height
_core = HM["core"]
_cx0, _cy0, _cst, _cnx, _cny, _ch = _core["x0"], _core["y0"], _core["step"], _core["nx"], _core["ny"], _core["heights"]


def ground(x, y):
    """Terrain height (cm) at UE (x, y) cm: the 0.5 m core grid, else the 2.5 m outer grid."""
    xm, ym = x / 100.0, y / 100.0
    fi, fj = (xm - _cx0) / _cst, (ym - _cy0) / _cst
    if 0 <= fi < _cnx - 1 and 0 <= fj < _cny - 1:
        i, j = int(fi), int(fj)
        tx, ty = fi - i, fj - j
        a, b = _ch[j * _cnx + i], _ch[j * _cnx + i + 1]
        c, d = _ch[(j + 1) * _cnx + i], _ch[(j + 1) * _cnx + i + 1]
        return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)
    size, step, n, h = HM["size"], HM["step"], HM["n"], HM["heights"]
    bx, by = xm + size / 2, -ym + size / 2
    fi, fj = max(0.0, min(n - 1e-3, bx / step)), max(0.0, min(n - 1e-3, by / step))
    i, j = int(fi), int(fj)
    tx, ty = fi - i, fj - j
    a, b = h[j * (n + 1) + i], h[j * (n + 1) + i + 1]
    c, d = h[(j + 1) * (n + 1) + i], h[(j + 1) * (n + 1) + i + 1]
    return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)


# The v1 helpers read these module globals at call time: point them at v2.
V1.update({"actors": actors, "ground": ground, "LAYOUT": LAYOUT, "stats": stats, "PLAZA": (998.0, 376.0)})


# =================================================================================================== meshes + materials
_mesh_cache = {}


def mesh(path):
    if ":" in path and not path.startswith("/"):
        k, name = path.split(":", 1)
        path = PREFIX[k] + name
    if path not in _mesh_cache:
        m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not m and path.startswith("/Engine/"):
            m = unreal.load_asset(path)
        _mesh_cache[path] = m
        if not m:
            stats.setdefault("missing", []).append(path)
            unreal.log_warning(f"KG_V2 missing mesh {path}")
    return _mesh_cache[path]


def have(path):
    """Optional pack meshes (DT = plan 11.4): True when imported; the kit fallback is used otherwise."""
    if ":" in path and not path.startswith("/"):
        k, name = path.split(":", 1)
        path = PREFIX[k] + name
    return unreal.EditorAssetLibrary.does_asset_exist(path)


# District tints (multiplied into the kit's glTF BaseColorFactor). Several entries = per-building variants.
TINTS = {
    "harbour_row": {"Plaster": [(0.60, 0.90, 0.86), (1.0, 0.70, 0.60), (1.0, 0.92, 0.58), (0.64, 0.80, 1.0)],
                    "RoundTiles": [(1.0, 0.64, 0.46)], "UnevenBrick": [(1.0, 0.97, 0.92)], "WoodTrim": [(0.50, 0.66, 0.98)]},
    "heart": {"Plaster": [(1.02, 1.02, 1.0), (1.0, 0.94, 0.82)], "UnevenBrick": [(1.08, 1.02, 0.88)],
              "RoundTiles": [(1.0, 0.48, 0.40)]},
    "upper_town": {"Plaster": [(1.0, 0.82, 0.52), (0.98, 0.88, 0.66)], "RoundTiles": [(0.64, 0.36, 0.30)],
                   "WoodTrim": [(0.62, 0.48, 0.36)], "UnevenBrick": [(0.95, 0.85, 0.78)]},
    "crown_hill": {"UnevenBrick": [(0.72, 0.80, 0.95)], "Plaster": [(0.86, 0.90, 0.98)], "RoundTiles": [(0.50, 0.60, 0.82)]},
    "sakura_garden": {"WoodTrim": [(1.0, 0.36, 0.26)], "RoundTiles": [(0.42, 0.40, 0.46)], "Plaster": [(1.02, 1.0, 0.98)]},
    "brookside": {"WoodTrim": [(0.52, 0.42, 0.36)], "UnevenBrick": [(0.80, 0.92, 0.72)], "RoundTiles": [(0.56, 0.44, 0.36)],
                  "Plaster": [(0.92, 0.88, 0.80)]},
    "orchard_upland": {"Plaster": [(1.05, 1.05, 1.02)], "WoodTrim": [(0.92, 0.34, 0.28)], "RoundTiles": [(1.0, 0.84, 0.50)]},
    "lighthouse_point": {"Plaster": [(1.05, 1.05, 1.05)], "RoundTiles": [(1.0, 0.40, 0.34)], "UnevenBrick": [(1.05, 1.05, 1.05)]},
}
_dmi = {}           # (district, kind, variant) -> MaterialInstanceConstant
_slot_kinds = {}    # mesh path -> [(slot, kind)]


def district_materials():
    """MI_KG_<district>_<kind>[_v] children of the kit MIs, overriding BaseColorFactor (created once, re-used)."""
    if TEST_BUILD:
        for dist, kinds in TINTS.items():
            for kind, tints in kinds.items():
                for v in range(len(tints)):
                    path = DISTRICT_MATS + f"MI_KG_{dist}_{kind}" + (f"_{v}" if len(tints) > 1 else "")
                    _dmi[(dist, kind, v)] = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path)                         else unreal.load_asset(f"{KIT_MATS}MI_{kind}")
        log(f"district materials: {len(_dmi)} loaded (test build, nothing saved)")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    made = 0
    for dist, kinds in TINTS.items():
        for kind, tints in kinds.items():
            base = unreal.load_asset(f"{KIT_MATS}MI_{kind}")
            if not base:
                continue
            try:
                bc = mel.get_material_instance_vector_parameter_value(base, "BaseColorFactor")
            except Exception:
                bc = unreal.LinearColor(1, 1, 1, 1)
            for v, t in enumerate(tints):
                name = f"MI_KG_{dist}_{kind}" + (f"_{v}" if len(tints) > 1 else "")
                path = DISTRICT_MATS + name
                mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
                if not mi:
                    mi = tools.create_asset(name, DISTRICT_MATS.rstrip("/"), unreal.MaterialInstanceConstant,
                                            unreal.MaterialInstanceConstantFactoryNew())
                    mi.set_editor_property("parent", base)
                    made += 1
                mel.set_material_instance_vector_parameter_value(
                    mi, "BaseColorFactor", unreal.LinearColor(bc.r * t[0], bc.g * t[1], bc.b * t[2], bc.a))
                mel.update_material_instance(mi)
                eal.save_loaded_asset(mi)
                _dmi[(dist, kind, v)] = mi
    log(f"district materials: {len(_dmi)} ({made} new)")


def slot_kinds(m):
    key = m.get_path_name()
    if key not in _slot_kinds:
        out = []
        for i, sm in enumerate(m.get_editor_property("static_materials")):
            mi = sm.get_editor_property("material_interface")
            nm = mi.get_name() if mi else ""
            if nm.startswith("MI_") and nm[3:] in ("Plaster", "UnevenBrick", "RoundTiles", "WoodTrim"):
                out.append((i, nm[3:]))
        _slot_kinds[key] = out
    return _slot_kinds[key]


def tint(actor, district, variant=0):
    if not district or district not in TINTS or not actor:
        return
    comp = actor.static_mesh_component
    m = comp.static_mesh
    if not m:
        return
    for i, kind in slot_kinds(m):
        n = len(TINTS[district].get(kind, []))
        if n:
            comp.set_material(i, _dmi[(district, kind, variant % n)])


# =================================================================================================== placement
def spawn(path, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0), folder="V2", collide=True, hidden=False,
          district=None, variant=0, label=None, movable=False):
    m = mesh(path)
    if not m:
        return None
    a = _real_actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z),
                                            unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if not a:
        return None
    comp = a.static_mesh_component
    if movable:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(m)
    if isinstance(scale, (int, float)):
        scale = (scale, scale, scale)
    if tuple(scale) != (1.0, 1.0, 1.0):
        a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_folder_path(folder)
    if not collide:
        a.set_actor_enable_collision(False)
    if hidden:
        a.set_actor_hidden_in_game(True)
    if label:
        a.set_actor_label(label)
    if district:
        tint(a, district, variant)
    stats["pieces"] += 1
    return a


def warm_light(x, y, z, intensity=10.0, radius=900.0, folder="V2/Lights", color=(255, 170, 95)):
    a = _real_actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    lc.set_editor_property("cast_shadows", False)
    a.set_folder_path(folder)
    stats["lights"] += 1
    return a


V1["warm_light"] = warm_light


class Frame:
    """Local frame: origin at the footprint centre at pad height, local -Y = front (door side)."""

    def __init__(self, x, y, z, yaw, district=None, variant=0):
        self.x, self.y, self.z, self.yaw = x, y, z, yaw
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        self.district, self.variant = district, variant

    def world(self, lx, ly):
        return self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c

    def put(self, path, lx, ly, lz, lyaw=0.0, folder="V2/Houses", scale=1.0, pitch=0.0, roll=0.0, collide=True,
            tinted=True):
        wx, wy = self.world(lx, ly)
        return spawn(path, wx, wy, self.z + lz, self.yaw + lyaw, pitch, roll, scale, folder, collide,
                     district=self.district if tinted else None, variant=self.variant)


def lm(name):
    return LAYOUT["landmarks"][name]


def door_local(w):
    cw = w // 2
    return -cw * 100.0 + 100.0 + (cw // 2) * 200.0


# =================================================================================================== buildings
ROOFS = {(4, 4), (4, 6), (4, 8), (6, 4), (6, 6), (6, 8), (6, 10), (6, 12), (6, 14), (8, 8), (8, 10), (8, 12), (8, 14)}


def sides_world(b):
    """{side: (p0, p1)} in metres for front/back/left/right of a building spec."""
    w, d = b["size"]
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    c, s = math.cos(yaw), math.sin(yaw)
    cx, cy = b["at"]

    def wpt(lx, ly):
        return (cx + lx * c - ly * s, cy + lx * s + ly * c)
    hw, hd = w / 2.0, d / 2.0
    return {"front": (wpt(-hw, -hd), wpt(hw, -hd)), "back": (wpt(-hw, hd), wpt(hw, hd)),
            "left": (wpt(-hw, -hd), wpt(-hw, hd)), "right": (wpt(hw, -hd), wpt(hw, hd))}


def _on_segment(p, a, b, tol=0.15):
    dx, dy = b[0] - a[0], b[1] - a[1]
    ll = dx * dx + dy * dy
    t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / max(ll, 1e-9)
    if t < -0.02 or t > 1.02:
        return False
    qx, qy = a[0] + t * dx, a[1] + t * dy
    return math.hypot(p[0] - qx, p[1] - qy) <= tol


def party_walls(buildings):
    """For party-wall units of one block: {id: {side: levels_to_skip}} (the earlier unit keeps the shared wall)."""
    skip = {}
    for i, b in enumerate(buildings):
        if not b.get("block"):
            continue
        sb = sides_world(b)
        for a in buildings[:i]:
            if a.get("block") != b.get("block"):
                continue
            sa = sides_world(a)
            for side, (p0, p1) in sb.items():
                if any(_on_segment(p0, *seg) and _on_segment(p1, *seg) for seg in sa.values()):
                    skip.setdefault(b["id"], {})[side] = max(skip.get(b["id"], {}).get(side, 0), a.get("storeys", 2))
    return skip


# SPRINT-022: the kit wall pieces carry their exterior dressing (stone plinth, beams, window frames and glass, the
# shutter / balcony mounts) on mesh +Y; the v2 builder used to place them with that face INSIDE the house. They are now
# turned 180 degrees and moved out by WALL_OUT, so the slab occupies exactly the same space (31 cm outside the
# footprint edge, 10 cm inside) and the dressed face looks at the street; interiors see the plain plaster face.
WALL_OUT = 21.0


def wall_xf(side, off, edge, out=0.0):
    """(lx, ly, lyaw) of a flipped wall cell at `off` along `side` (edge = half size, cm; out = extra jetty)."""
    e = edge + WALL_OUT + out
    if side == "front":
        return off, -e, 180.0
    if side == "back":
        return -off, e, 0.0
    if side == "left":
        return -e, -off, 90.0
    return e, off, -90.0


def wall_run(f, cells, edge, floor_z, pieces, side, folder="V2/Houses", out=0.0):
    for k in range(cells):
        off = -cells * 100.0 + 100.0 + k * 200.0
        piece = pieces(k, cells)
        if piece is None:
            continue
        lx, ly, lyaw = wall_xf(side, off, edge, out)
        f.put(piece, lx, ly, floor_z, lyaw, folder)


def wall_style(district, style):
    """(ground-floor material, upper piece fn) per district identity (plan section 2)."""
    if district == "crown_hill":
        return "UnevenBrick", lambda k, n, lvl: V + ("Wall_UnevenBrick_Window_Thin_Round" if k % 2 else "Wall_UnevenBrick_Straight")
    if district == "upper_town":
        return ("UnevenBrick" if style % 3 else "Plaster"), \
            lambda k, n, lvl: V + ("Wall_Plaster_Window_Wide_Flat" if k % 2 == 0 else "Wall_Plaster_WoodGrid")
    if district == "harbour_row":
        return "Plaster", lambda k, n, lvl: V + ("Wall_Plaster_Window_Thin_Round" if k % 2 == 0 else "Wall_Plaster_Straight")
    if district in ("brookside", "orchard_upland"):
        return "UnevenBrick", lambda k, n, lvl: V + ("Wall_Plaster_WoodGrid" if k % 2 else "Wall_Plaster_Window_Wide_Flat")
    lower = "UnevenBrick" if style % 2 == 0 else "Plaster"
    return lower, lambda k, n, lvl: V + ("Wall_Plaster_Window_Wide_Round" if (k + lvl) % 2 == 0 else "Wall_Plaster_WoodGrid")


DORMER_DISTRICTS = ("upper_town", "heart", "crown_hill")


def dormers(f, spec, w, d, top, parallel, skip, storeys, folder):
    """Kit Roof_Dormer_RoundTile on the 6 m-span round-tile roofs of the upper town (plus Heart / Crown): on the
    street and back slopes of ridge-parallel roofs, or on a gable roof's side slopes that no neighbour hides.
    Fit from the kit meshes (Blender probe): the 6 m roof's tile surface is z = 5.07 - 1.535 x (m, x from the ridge);
    the dormer faces its local +X, its front sill at x = 1.5, z = 0.03 and its back buried 0.8 m in the roof when
    its origin sits at x0 = 0.5 m, z = 1.92 m over the eave line."""
    if spec.get("district") not in DORMER_DISTRICTS or storeys < 2:
        return
    x0, dz = 50.0, 192.0
    placed = 0
    if parallel and d == 6:
        # ridge along the frame X (width w = 6): slopes face front (-Y, the street) and back (+Y)
        for side_sign, lyaw in ((-1.0, -90.0), (1.0, 90.0)):
            f.put(V + "Roof_Dormer_RoundTile", 0.0, side_sign * x0, top + dz, lyaw, folder)
            placed += 1
    elif not parallel and w == 6 and d >= 6:
        # ridge along the frame Y (depth d): side slopes, only where no party wall/neighbour stands
        n = max(1, int(d // 4))
        for side, sx, lyaw in (("left", -1.0, 180.0), ("right", 1.0, 0.0)):
            if skip.get(side, 0) > 0:
                continue
            for k in range(n):
                ly = -d * 50.0 + (k + 0.5) * d * 100.0 / n
                f.put(V + "Roof_Dormer_RoundTile", sx * x0, ly, top + dz, lyaw, folder)
                placed += 1
    stats["dormers"] = stats.get("dormers", 0) + placed


def build_house(spec, kind, skip_sides, variant, row_index=0):
    """Homes and infill shells with an archetype (SPRINT-022) get their own facade grammar; civic buildings and
    anything without one keep the classic district style."""
    if kind != "civic" and ARCH.arch_of(spec) is not None:
        return build_archetype(spec, kind, skip_sides, variant)
    return build_house_classic(spec, kind, skip_sides, variant, row_index)


def build_house_classic(spec, kind, skip_sides, variant, row_index=0):
    """kind: home | infill | civic. Storeys 1-3, party walls skipped, district tint, interior for homes/civic."""
    x, y = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec.get("yaw", spec["face_deg"] + 90.0)
    gz = spec["z"] * M + 2.0
    w, d = spec["size"]
    storeys = max(1, min(3, int(spec.get("storeys", 2))))
    style = int(spec.get("style", 0))
    district = spec.get("district")
    f = Frame(x, y, gz, yaw, district, variant)
    cw, cd = w // 2, d // 2
    shell = kind == "infill"
    lower, upper_piece = wall_style(district, style)
    folder = "V2/Shells" if shell else ("V2/Civic" if kind == "civic" else "V2/Houses")
    ex, ey = w * 50.0, d * 50.0
    skip = skip_sides or {}

    # floors: ground (pad), first floor with the stair well (homes/civic), a full ceiling under a third storey
    if shell:
        f.put(V + "Floor_Brick", 0.0, 0.0, 1.0, 0.0, folder, scale=(cw, cd, 1.0))
    else:
        for i in range(cw):
            for j in range(cd):
                lx, ly = -w * 50 + 100 + i * 200, -d * 50 + 100 + j * 200
                f.put(V + ("Floor_Brick" if style % 3 else "Floor_RedBrick"), lx, ly, 1.0, 0.0, folder)
                if storeys >= 2 and not (i == 0 and j >= cd - 2):
                    f.put(V + ("Floor_WoodDark" if style % 2 else "Floor_WoodLight"), lx, ly, FLOOR_H, 0.0, folder)
    if storeys >= 3 or (shell and storeys >= 2):
        for lvl in range(1 if shell else 2, storeys):
            f.put(V + "Floor_WoodDark", 0.0, 0.0, lvl * FLOOR_H, 0.0, folder, scale=(cw, cd, 1.0))

    def ground_front(k, n):
        if k == n // 2:
            return V + (f"Wall_{lower}_Door_Flat" if shell else f"Wall_{lower}_Door_Round")
        return V + (f"Wall_{lower}_Window_Wide_Round" if k % 2 else f"Wall_{lower}_Straight")

    def ground_side(k, n):
        return V + (f"Wall_{lower}_Window_Thin_Round" if k % 2 == 1 else f"Wall_{lower}_Straight")

    for lvl in range(storeys):
        z = lvl * FLOOR_H
        for side, n, e in (("front", cw, ey), ("back", cw, ey), ("left", cd, ex), ("right", cd, ex)):
            if lvl < skip.get(side, 0):
                continue
            if lvl == 0:
                fn = ground_front if side == "front" else ground_side
            else:
                fn = (lambda k, n_, lvl=lvl: upper_piece(k, n_, lvl))
            wall_run(f, n, e, z, fn, side, folder)
        for sx in (-1, 1):
            for sy in (-1, 1):
                side = "left" if sx < 0 else "right"
                if lvl < skip.get(side, 0):
                    continue
                f.put(V + ("Corner_Exterior_Brick" if (lvl == 0 and lower == "UnevenBrick") else "Corner_Exterior_Wood"),
                      sx * ex, sy * ey, z, 0.0, folder)

    # roof: gable to the street by default; 6 m units alternate (ridge parallel to the street) inside a block
    top = storeys * FLOOR_H
    parallel = (w == 6 and row_index % 2 == 1 and (d, w) in ROOFS and d in (4, 6, 8))
    if parallel:
        f.put(V + f"Roof_RoundTiles_{d}x{w}", 0.0, 0.0, top, 90.0, folder)
        for side, lyaw, lx in (("left", -90.0, -ex), ("right", 90.0, ex)):
            if skip.get(side, 0) < storeys + 1:
                f.put(V + f"Roof_Front_Brick{d}", lx, 0.0, top, lyaw, folder)
    else:
        rw, rd = (w, d) if (w, d) in ROOFS else (min(w, 8), min(d, 12))
        f.put(V + f"Roof_RoundTiles_{rw}x{rd}", 0.0, 0.0, top, 0.0, folder)
        if w in (4, 6, 8):
            f.put(V + f"Roof_Front_Brick{w}", 0.0, -ey, top, 0.0, folder)
            f.put(V + f"Roof_Front_Brick{w}", 0.0, ey, top, 180.0, folder)
    f.put(V + ("Prop_Chimney" if style % 2 else "Prop_Chimney2"), (ex - 90.0) * (1 if style % 2 else -1), ey * 0.4,
          top + 60.0, 0.0, folder)
    dormers(f, spec, w, d, top, parallel, skip, storeys, folder)

    door_x = door_local(w)
    if shell:
        # static closed door in the flat doorway (no interior)
        f.put(V + "Door_1_Flat", door_x - 55.0, -ey - 5.0, 3.0, 0.0, folder)
        stats["shells"] += 1
        return f
    # real door (KGDoor), bell, lantern, light
    wx = f.x + (door_x - 61.5) * f.c - (-ey - 11.0) * f.s
    wy = f.y + (door_x - 61.5) * f.s + (-ey - 11.0) * f.c
    dcls = unreal.load_class(None, "/Script/KillGodot.KGDoor")
    door = _real_actors.spawn_actor_from_class(dcls, unreal.Vector(wx, wy, gz + 3.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=f.yaw - 90.0))
    door.set_folder_path("V2/Doors")
    door.set_actor_label(f"Door_{spec.get('id', kind)}")
    f.put(WP + "Doorbell", door_x - 105.0, -ey - 31.0, 215.0, -90.0, "V2/Props", tinted=False)
    f.put(P + "Lantern_Wall", door_x + 140.0, -ey - 31.0, 150.0, 180.0, "V2/Props", tinted=False)
    lx, ly = f.world(door_x + 140.0, -ey - 130.0)
    warm_light(lx, ly, gz + 205.0, 10.0, 800.0)
    if style % 3 == 0:
        f.put(V + "Prop_Vine4", -ex + 40.0, -ey - 30.0, 0.0, 0.0, "V2/Props", tinted=False)
    # interior: stair + furnished rooms (chest HouseIndex = style = home_index for homes)
    n0 = stats["pieces"]
    stats["pieces"] += kg_interiors.furnish(f, w, d, style, warm_light)
    stats["interior_pieces"] = stats.get("interior_pieces", 0) + stats["pieces"] - n0
    stats["houses" if kind == "home" else "civic"] += 1
    return f


# =================================================================================================== archetypes (S22)
PAINT_DIR = "/Game/KillGodot/Materials/S22/"
_paint = {}
JET = 32.0                  # jetty: the upper front walls stand this much further out (cm)
FREE_SIDES = {}             # building id -> sides with open ground for an outside stair (set by buildings())
FACADES = {}                # building id -> what was built (verify_v2_build same-looking-neighbour check)


def paint_mi(name):
    """MI_KG_Paint_<name> (Tools/Unreal/kg_make_paint_v2.py) or None (natural wood)."""
    if not name or ARCH.PAINTS.get(name) is None:
        return None
    if name not in _paint:
        path = PAINT_DIR + f"MI_KG_Paint_{name}"
        _paint[name] = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    return _paint[name]


def painted(actor, name, slot=0):
    mi = paint_mi(name)
    if actor and mi:
        actor.static_mesh_component.set_material(slot, mi)


def roof_piece(across, along):
    """Kit round-tile roof `across` m over the eaves and `along` m along the ridge: the exact piece, or a longer one
    scaled along the ridge (scaling along the ridge keeps the slopes)."""
    if (across, along) in ROOFS:
        return f"Roof_RoundTiles_{across}x{along}", 1.0
    longer = sorted(a for (c, a) in ROOFS if c == across and a >= along)
    if longer:
        return f"Roof_RoundTiles_{across}x{longer[0]}", along / float(longer[0])
    return f"Roof_RoundTiles_{min(across, 8)}x{max(along, 4)}", 1.0


WINDOW_FOR = {"Window_Wide_Round": "Window_Wide_Round1", "Window_Wide_Flat": "Window_Wide_Flat1",
              "Window_Thin_Round": "Window_Thin_Round1"}
SHUTTER_FOR = {"Window_Wide_Round": "WindowShutters_Wide_Round", "Window_Wide_Flat": "WindowShutters_Wide_Flat",
               "Window_Thin_Round": "WindowShutters_Thin_Round"}


def build_archetype(spec, kind, skip_sides, variant):
    """One home / shell from its archetype (Tools/Level/kg_archetypes_v2.py): walls with the archetype's window rhythm
    and materials, roof form (gable / ridge / hip / cross-gable), jetty, balcony, porch canopy, outside stair,
    chimneys, painted shutters and door. Same pad, floors, party walls and interior as the classic builder."""
    A = ARCH.arch_of(spec)
    x, y = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec.get("yaw", spec["face_deg"] + 90.0)
    gz = spec["z"] * M + 2.0
    w, d = spec["size"]
    storeys = max(1, min(3, int(spec.get("storeys", 2))))
    style = int(spec.get("style", 0))
    district = spec.get("district")
    det = spec.get("details") or ARCH.details(spec)
    f = Frame(x, y, gz, yaw, district, variant)
    cw, cd = w // 2, d // 2
    shell = kind == "infill"
    folder = "V2/Shells" if shell else "V2/Houses"
    ex, ey = w * 50.0, d * 50.0
    skip = skip_sides or {}
    bid = spec.get("id", "")
    rec = {"archetype": spec["archetype"], "roof": A["roof"], "storeys": storeys, "ground": A["ground"],
           "upper": A["upper"], "district": district, "variant": variant, "extras": []}

    # outside stair: only on a free side (decided before the walls: its landing needs a first-floor door)
    stair_side = None
    if A["stair"] and storeys >= 2:
        for sd in ("right", "left"):
            if skip.get(sd, 0) == 0 and sd in FREE_SIDES.get(bid, ()):
                stair_side = sd
                break
    spec["_stair_side"] = stair_side

    # floors (as the classic builder)
    if shell:
        f.put(V + "Floor_Brick", 0.0, 0.0, 1.0, 0.0, folder, scale=(cw, cd, 1.0))
    else:
        for i in range(cw):
            for j in range(cd):
                lx, ly = -w * 50 + 100 + i * 200, -d * 50 + 100 + j * 200
                f.put(V + ("Floor_Brick" if style % 3 else "Floor_RedBrick"), lx, ly, 1.0, 0.0, folder)
                if storeys >= 2 and not (i == 0 and j >= cd - 2):
                    f.put(V + ("Floor_WoodDark" if style % 2 else "Floor_WoodLight"), lx, ly, FLOOR_H, 0.0, folder)
    if storeys >= 3 or (shell and storeys >= 2):
        for lvl in range(1 if shell else 2, storeys):
            f.put(V + "Floor_WoodDark", 0.0, 0.0, lvl * FLOOR_H, 0.0, folder, scale=(cw, cd, 1.0))

    # balcony cells (the window behind a balcony becomes a door)
    bal_lvl, bal_cells = None, []
    if A["balcony"] and storeys >= 2:
        bal_lvl = storeys - 1 if A["balcony"] == "top_centre" else 1
        bal_cells = list(range(cw)) if A["balcony"] == "full" else [cw // 2]

    shutters = []
    for lvl in range(storeys):
        z = lvl * FLOOR_H
        jet = JET if (A["jetty"] and lvl >= 1) else 0.0
        for side, n, e in (("front", cw, ey), ("back", cw, ey), ("left", cd, ex), ("right", cd, ex)):
            if lvl < skip.get(side, 0):
                continue

            def pc(k, n_, side=side, lvl=lvl):
                name = ARCH.piece(spec, side, lvl, k, n_, shell)
                if side == "front" and lvl == bal_lvl and k in bal_cells:
                    name = f"Wall_{A['upper']}_Door_Flat"
                return V + name if name else None

            out = jet if side == "front" else 0.0
            wall_run(f, n, e, z, pc, side, folder, out=out)
            # the kit's window frames with glass in every window opening (same transform as the wall cell)
            for k in range(n):
                name = pc(k, n)
                frame_ = next((fr for wk, fr in WINDOW_FOR.items() if name and wk in name), None)
                if frame_:
                    lx, ly, lyaw = wall_xf(side, -n * 100.0 + 100.0 + k * 200.0, e, out)
                    f.put(V + frame_, lx, ly, z, lyaw, "V2/Facade", collide=False, tinted=False)
            # shutters on the dressed windows (street side and open ends; the back only upstairs)
            if A["shutters"] and (side != "back" or lvl >= 1):
                for k in range(n):
                    name = ARCH.piece(spec, side, lvl, k, n, shell)
                    kind_ = next((sk for wk, sk in SHUTTER_FOR.items() if name and wk in name), None)
                    if kind_ and not (side == "front" and lvl == bal_lvl and k in bal_cells):
                        off = -n * 100.0 + 100.0 + k * 200.0
                        shutters.append((kind_, side, off, e, z, out))
        for sx in (-1, 1):
            for sy in (-1, 1):
                side = "left" if sx < 0 else "right"
                if lvl < skip.get(side, 0):
                    continue
                oy = -jet if sy < 0 else 0.0
                f.put(V + ("Corner_Exterior_Brick" if (lvl == 0 and A["ground"] == "UnevenBrick") else "Corner_Exterior_Wood"),
                      sx * ex, sy * ey + oy, z, 0.0, folder)
        if jet:
            _jetty_bits(f, w, ex, ey, lvl, skip, folder)
    if A["jetty"] and storeys >= 2:
        rec["extras"].append("jetty")

    closed = det.get("shutters_closed")
    for kind_, side, off, e, z, out in shutters:
        lx, ly, lyaw = wall_xf(side, off, e, out)
        a = f.put(V + kind_ + ("_Closed" if closed and z == 0.0 else "_Open"), lx, ly, z, lyaw, "V2/Facade",
                  collide=False, tinted=False)
        painted(a, det.get("shutters"))
    if shutters:
        rec["extras"].append(f"shutters:{det.get('shutters')}")

    # balcony: plank floor on brackets, the kit rail on the wall transform, short end rails
    if bal_cells:
        z = bal_lvl * FLOOR_H
        for k in bal_cells:
            off = -cw * 100.0 + 100.0 + k * 200.0
            lx, ly, lyaw = wall_xf("front", off, ey)
            f.put(V + "Balcony_Simple_Straight", lx, ly, z, lyaw, "V2/Facade", tinted=False)
            f.put(V + "Floor_WoodDark", off, -ey - 31.0 - 52.0, z + 2.0, 0.0, "V2/Facade", scale=(1.0, 0.52, 1.0), tinted=False)
            for bx in (off - 80.0, off + 80.0):
                f.put(V + "Roof_Support2", bx, -ey - 31.0, z + 4.0, 180.0, "V2/Facade", collide=False, tinted=False)
        x0 = -cw * 100.0 + 100.0 + bal_cells[0] * 200.0 - 100.0
        x1 = -cw * 100.0 + 100.0 + bal_cells[-1] * 200.0 + 100.0
        for ex_ in (x0 + 4.0, x1 - 4.0):
            f.put(V + "Prop_WoodenFence_Single", ex_, -ey - 31.0 - 52.0, z + 2.0, 90.0, "V2/Facade", scale=(0.5, 1.0, 1.25),
                  tinted=False)
        rec["extras"].append(f"balcony:{A['balcony']}")

    # roof
    top = storeys * FLOOR_H
    rf = A["roof"]
    parallel = False
    if rf == "gable":
        piece_, sc = roof_piece(w, d)
        f.put(V + piece_, 0.0, 0.0, top, 0.0, folder, scale=(1.0, sc, 1.0))
        if w in (4, 6, 8):
            f.put(V + f"Roof_Front_Brick{w}", 0.0, -ey, top, 0.0, folder)
            f.put(V + f"Roof_Front_Brick{w}", 0.0, ey, top, 180.0, folder)
    elif rf in ("ridge", "cross"):
        parallel = True
        piece_, sc = roof_piece(d, w)
        f.put(V + piece_, 0.0, 0.0, top, 90.0, folder, scale=(1.0, sc, 1.0))
        for side, lyaw, lx in (("left", -90.0, -ex), ("right", 90.0, ex)):
            if skip.get(side, 0) < storeys + 1 and d in (4, 6, 8):
                f.put(V + f"Roof_Front_Brick{d}", lx, 0.0, top, lyaw, folder)
        if rf == "cross":
            # a street gable over the door bay: a 4 m roof across the main ridge, its gable end on the facade
            f.put(V + "Roof_RoundTiles_4x4", door_local(w), -ey + 190.0, top, 0.0, folder, scale=(1.0, 0.9, 1.0))
            f.put(V + "Roof_Front_Brick4", door_local(w), -ey, top, 0.0, folder)
    else:   # hip: the kit tower roof stretched over the footprint, flattened to a house pitch
        f.put(V + "Roof_Tower_RoundTiles", 0.0, 0.0, top - 6.0, 0.0, folder,
              scale=((w * 100.0 + 150.0) / 569.0, (d * 100.0 + 150.0) / 578.0, 0.52 if storeys >= 2 else 0.45))
    if A["dormers"]:
        dormers(f, spec, w, d, top, parallel, skip, storeys, folder)

    # chimneys
    ch = A["chimney"]
    if rf == "hip":
        spots = [(-(ex - 140.0), 0.0), (ex - 140.0, 0.0)] if ch == "both" else [((ex - 140.0) * (1 if "right" in ch else -1), 0.0)]
    elif ch == "both":
        spots = [(-(ex - 90.0), ey * 0.4), (ex - 90.0, ey * 0.4)]
    elif ch == "back":
        spots = [(0.0, ey - 70.0)]
    else:
        spots = [((ex - 90.0) * (1 if "right" in ch else -1), ey * 0.4)]
    for k, (cx, cy) in enumerate(spots):
        f.put(V + ("Prop_Chimney2" if (k + style) % 2 else "Prop_Chimney"), cx, cy, top + 60.0, 0.0, folder,
              scale=(1.0, 1.0, 1.35 if ch == "left_tall" else 1.0))

    # porch canopy over the door
    door_x = door_local(w)
    if A["porch"]:
        lx, ly, lyaw = wall_xf("front", door_x, ey)
        f.put(V + "Roof_Wooden_2x1", lx, ly, 238.0, lyaw, "V2/Facade", tinted=False)
        for bx in (door_x - 95.0, door_x + 95.0):
            f.put(V + "Roof_Support2", bx, -ey - 31.0, 236.0, 180.0, "V2/Facade", collide=False, tinted=False)
        rec["extras"].append("porch")

    # outside stair up the free side to the first-floor door
    if stair_side:
        _outside_stair(f, stair_side, ex, ey)
        rec["extras"].append(f"stair:{stair_side}")

    FACADES[bid] = rec
    door_mesh = A["door"]
    if shell:
        flat = door_mesh if "Flat" in door_mesh else door_mesh.replace("Round", "Flat")
        a = f.put(V + flat, door_x - 55.0, -ey - 5.0, 3.0, 0.0, folder, tinted=False)
        painted(a, det.get("door"), 0)
        stats["shells"] += 1
        return f
    # real door (KGDoor) with the archetype's leaf and paint, bell, lantern, light
    wx = f.x + (door_x - 61.5) * f.c - (-ey - 11.0) * f.s
    wy = f.y + (door_x - 61.5) * f.s + (-ey - 11.0) * f.c
    dcls = unreal.load_class(None, "/Script/KillGodot.KGDoor")
    door = _real_actors.spawn_actor_from_class(dcls, unreal.Vector(wx, wy, gz + 3.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=f.yaw - 90.0))
    door.set_folder_path("V2/Doors")
    door.set_actor_label(f"Door_{spec.get('id', kind)}")
    try:
        leaf = next((c for c in door.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Leaf"), None)
        lm_ = mesh(V + door_mesh)
        if leaf and lm_:
            leaf.set_static_mesh(lm_)
            mi = paint_mi(det.get("door"))
            if mi:
                leaf.set_material(0, mi)
    except Exception as ex_:
        log(f"door leaf {bid}: {ex_}")
    f.put(WP + "Doorbell", door_x - 105.0, -ey - 31.0, 215.0, -90.0, "V2/Props", tinted=False)
    f.put(P + "Lantern_Wall", door_x + 140.0, -ey - 31.0, 150.0, 180.0, "V2/Props", tinted=False)
    lx, ly = f.world(door_x + 140.0, -ey - 130.0)
    warm_light(lx, ly, gz + 205.0, 10.0, 800.0)
    n0 = stats["pieces"]
    stats["pieces"] += kg_interiors.furnish(f, w, d, style, warm_light)
    stats["interior_pieces"] = stats.get("interior_pieces", 0) + stats["pieces"] - n0
    stats["houses"] += 1
    return f


def _jetty_bits(f, w, ex, ey, lvl, skip, folder):
    """The overhang of a jettied storey: a beam along the floor line, brackets under it, the side slots closed with
    wide corner posts, and a floor strip so the room has no gap along the front."""
    z = lvl * FLOOR_H
    f.put(V + "Corner_Exterior_Wood", -ex - 25.0, -ey - 31.0 - JET * 0.5, z - 14.0, 0.0, "V2/Facade", pitch=-90.0,
          scale=(1.3, 1.3, (w * 100.0 + 50.0) / 300.0), tinted=False)
    for k in range(w // 2 + 1):
        bx = max(-ex + 12.0, min(ex - 12.0, -ex + k * 200.0))
        f.put(V + "Roof_Support2", bx, -ey - 31.0, z - 4.0, 180.0, "V2/Facade", collide=False, tinted=False)
    for sx, side in ((-1, "left"), (1, "right")):
        if lvl >= skip.get(side, 0):
            f.put(V + "Corner_ExteriorWide_Wood", sx * (ex + 10.0), -ey - JET * 0.5 - 8.0, z, 0.0, folder, scale=(1.4, 1.6, 1.0))
    f.put(V + "Floor_WoodDark", 0.0, -ey - JET * 0.5 - 10.0, z + 1.0, 0.0, folder, scale=(w / 2.0, (JET + 22.0) / 200.0, 1.0))


STAIR_RISER_YAW = 0.0      # kit riser yaw (frame-relative) that climbs towards local -Y (checked in the S22 lab)


def _outside_stair(f, side, ex, ey):
    """Three 1 m kit risers along a free side wall, climbing from the back towards the front, onto a stone landing
    (a platform block) in front of the first-floor door the archetype put on the side's front cell."""
    sx = -1.0 if side == "left" else 1.0
    xo = sx * (ex + 31.0 + 101.0)
    y_top = -ey + 100.0                                  # landing beside the first side cell (door cell k = 0)
    for k in range(3):
        y = y_top + 200.0 + (2 - k) * 208.0
        f.put(V + "Stairs_Exterior_Straight", xo, y, k * 100.0, STAIR_RISER_YAW, "V2/Facade", tinted=False)
    f.put(V + "Stairs_Exterior_Platform", xo, y_top, 0.0, 0.0, "V2/Facade", scale=(1.0, 1.0, 3.0), tinted=False)

DL = "/Game/KillGodot/Env/Dress/KG_DressLandmarks_Clean/StaticMeshes/SM_KG_"   # SPRINT-022 landmark pack
CLOCK_MINUTE_S = 120.0      # idle turn rate (lobby); in a match AKGSpinner drives ClockHand_* meshes from the match clock


def spinner(path, x, y, z, yaw, rate, folder, label=None):
    cls = unreal.load_class(None, "/Script/KillGodot.KGSpinner")
    if not cls or not mesh(path):
        return spawn(path, x, y, z, yaw, folder=folder, collide=False)
    a = _real_actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.set_mesh(mesh(path))
    a.set_editor_property("spin_rate", rate)
    a.set_actor_enable_collision(False)
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    stats["pieces"] += 1
    return a


def clock_top(f, levels, folder):
    """SPRINT-022 clock tower crown on the kit shaft: stone clock stage with four dials and turning hands, an open
    belfry with a small bell, an octagonal verdigris spire with a gilded weathervane (about +19 m)."""
    z0 = levels * FLOOR_H
    f.put(DL + "ClockStage", 0.0, 0.0, z0, 0.0, folder, tinted=False)
    f.put(DL + "Belfry", 0.0, 0.0, z0 + 348.0, 0.0, folder, tinted=False)
    f.put(DL + "Spire", 0.0, 0.0, z0 + 348.0 + 382.0, 0.0, folder, tinted=False)
    zc = z0 + 198.0
    for lx, ly, lyaw in ((0.0, -236.0, 180.0), (0.0, 236.0, 0.0), (-236.0, 0.0, 90.0), (236.0, 0.0, -90.0)):
        f.put(DL + "ClockDial", lx, ly, zc, lyaw, folder, collide=False, tinted=False)
        ux, uy = (lx / 236.0, ly / 236.0)
        wx, wy = f.world(lx + ux * 10.0, ly + uy * 10.0)
        yaw = f.yaw + lyaw
        spinner(DL + "ClockHand_Minute", wx, wy, f.z + zc, yaw, unreal.Rotator(roll=0.0, pitch=-360.0 / CLOCK_MINUTE_S, yaw=0.0),
                "V2/Towers/Clock")
        spinner(DL + "ClockHand_Hour", wx, wy, f.z + zc, yaw, unreal.Rotator(roll=0.0, pitch=-30.0 / CLOCK_MINUTE_S, yaw=0.0),
                "V2/Towers/Clock")
    stats["clock_faces"] = 4
    stats["clock_hands"] = 8


def bell_top(f, levels, folder):
    """SPRINT-022 the bell tower's bell: an oak beam across the open lookout under the roof, a bronze bell hanging
    from it over the RingBell station (AKGChoreFx strikes it at the station, 2.5 m up)."""
    z = (levels - 1) * FLOOR_H + 275.0
    f.put(DL + "BellBeam", 0.0, 0.0, z, 0.0, folder, collide=False, tinted=False)
    f.put(DL + "Bell", 0.0, 0.0, z + 28.0, 0.0, folder, collide=False, tinted=False)
    stats["bell"] = 1


def lighthouse_top(f, levels, folder):
    """SPRINT-022 lighthouse crown: a corbelled gallery with an iron railing and a glazed lamp room with a copper dome."""
    z0 = levels * FLOOR_H
    f.put(DL + "LighthouseGallery", 0.0, 0.0, z0, 0.0, folder, tinted=False)
    f.put(DL + "LampRoom", 0.0, 0.0, z0 + 50.0, 0.0, folder, tinted=False)
    stats["lighthouse_top"] = 1


def tower(spec, levels, light=None, district="crown_hill", clock=False, crown=None):
    """4x4 m stone tower (v1 recipe) at the JSON pad height; climbable ladder to the lookout floor.
    clock=True: four ClockFace dials (plan 11.4 pack) on the plain third-storey walls (level 2), one per side.
    crown (SPRINT-022, needs the KG_DressLandmarks pack): "clock" | "bell" | "lighthouse" replaces the plain roof."""
    x, y = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec.get("yaw", spec["face_deg"] + 90.0)
    gz = spec["z"] * M + 2.0
    f = Frame(x, y, gz, yaw, district, 0)
    folder = "V2/Towers"
    for lvl in range(levels):
        z = lvl * FLOOR_H
        top = lvl == levels - 1

        def piece(k, n, lvl=lvl, top=top):
            if top:
                return V + "Wall_Arch"
            if lvl == 0 and k == 0:
                return V + "Wall_UnevenBrick_Door_Round"
            return V + ("Wall_UnevenBrick_Window_Thin_Round" if lvl % 2 else "Wall_UnevenBrick_Straight")

        for side in ("front", "back", "left", "right"):
            wall_run(f, 2, 200.0, z, piece, side, folder)
        for sx in (-1, 1):
            for sy in (-1, 1):
                f.put(V + "Corner_Exterior_Brick", sx * 200.0, sy * 200.0, z, 0.0, folder)
        if lvl == 0 or top:
            for i in (-100.0, 100.0):
                for j in (-100.0, 100.0):
                    if top and i > 0 and j < 0:
                        continue
                    f.put(V + "Floor_Brick", i, j, z + 1.0, 0.0, folder)
    crown = crown if (crown and have(DL + "ClockStage")) else None
    if crown == "clock":
        clock_top(f, levels, folder)
        clock = False
    elif crown == "lighthouse":
        lighthouse_top(f, levels, folder)
    else:
        f.put(V + "Roof_Tower_RoundTiles", 0.0, 0.0, levels * FLOOR_H, 0.0, folder)
        if crown == "bell":
            bell_top(f, levels, folder)
    if clock and levels >= 3 and have(DT + "ClockFace"):
        # the wall pieces stand 31 cm proud of the 2 m edge; the dial's back plane is its pivot, its face local +Y
        for lx, ly, lyaw in ((0.0, -233.0, 180.0), (0.0, 233.0, 0.0), (-233.0, 0.0, 90.0), (233.0, 0.0, -90.0)):
            f.put(DT + "ClockFace", lx, ly, 2 * FLOOR_H + 150.0, lyaw, folder, collide=False, tinted=False)
        stats["clock_faces"] = 4
    wx, wy = f.world(100.0, -10.0)
    V1["ladder"](wx, wy, gz + 2.0, yaw + 90.0, (levels - 1) * FLOOR_H, folder="V2/Ladders")
    if light:
        lz = levels * FLOOR_H + 220.0 if crown == "lighthouse" else (levels - 1) * FLOOR_H + 170.0
        warm_light(x, y, gz + lz, light[0], light[1], folder="V2/Lights", color=light[2] if len(light) > 2 else (255, 170, 95))
    stats["towers"] += 1
    return f


def open_hall(spec, roof, posts, props=(), folder="V2/Civic", eave=330.0, roof_z=1.0):
    """Open arcade (fish market): posts, a round-tile roof (eave height / roof z-scale keep it under the JSON
    top_z so the S4 garden axis sees over it), brick floor, trade props."""
    x, y = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec.get("yaw", spec["face_deg"] + 90.0)
    gz = spec["z"] * M + 2.0
    w, d = spec["size"]
    f = Frame(x, y, gz, yaw, spec.get("district"), 1)
    for (px, py) in posts:
        f.put(V + "Corner_Exterior_Wood", px, py, 0.0, 0.0, folder, scale=(1.3, 1.3, eave / 300.0))
    f.put(V + roof[0], 0.0, 0.0, eave, roof[1], folder, scale=(1.0, 1.0, roof_z))
    for i in range(w // 2):
        for j in range(d // 2):
            f.put(V + "Floor_Brick", -w * 50 + 100 + i * 200, -d * 50 + 100 + j * 200, 1.0, 0.0, folder, tinted=False)
    for name, lx, ly, lyaw in props:
        f.put(name, lx, ly, 2.0, lyaw, folder, tinted=False)
    warm_light(x, y, gz + 260.0, 12.0, 900.0)
    stats["civic"] += 1
    return f


def _in_rect(b, px, py, pad=0.0):
    w, d = b["size"]
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    dx, dy = px - b["at"][0], py - b["at"][1]
    lx = dx * math.cos(yaw) + dy * math.sin(yaw)
    ly = -dx * math.sin(yaw) + dy * math.cos(yaw)
    return abs(lx) <= w / 2.0 + pad and abs(ly) <= d / 2.0 + pad


def free_sides(allb, skips):
    """FREE_SIDES[id] = sides (left / right) of outside-stair archetypes with a clear, level 2.6 m strip: no other
    building, no walkway, no stair or ramp, the ground within 0.4 m of the pad (metres, layout frame)."""
    segs = []
    for l in LAYOUT["lanes"]:
        for a_, b_ in zip(l["points"], l["points"][1:]):
            segs.append((a_, b_, l["width"] / 2.0 + 0.4))
    for st in LAYOUT["stairs"]:
        segs.append((st["from"], st["to"], st["width"] / 2.0 + 0.4))
    for rp in LAYOUT["ramps"]:
        for a_, b_ in zip(rp["points"], rp["points"][1:]):
            segs.append((a_, b_, rp["width"] / 2.0 + 0.4))
    others = [b for b in allb if b.get("size")] + [v for v in LAYOUT["landmarks"].values() if v.get("size")]
    for b in allb:
        A = ARCH.arch_of(b)
        if not A or not A.get("stair"):
            continue
        w, d = b["size"]
        yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
        c, s_ = math.cos(yaw), math.sin(yaw)
        ok = []
        for side, sx in (("left", -1.0), ("right", 1.0)):
            if skips.get(b["id"], {}).get(side):
                continue
            good = True
            for u in (0.4, 1.3, 2.3):
                for v in [-d / 2.0 + 0.2 + i * 0.5 for i in range(int(d / 0.5))]:
                    lx, ly = sx * (w / 2.0 + 0.31 + u), v
                    px, py = b["at"][0] + lx * c - ly * s_, b["at"][1] + lx * s_ + ly * c
                    if any(o is not b and _in_rect(o, px, py, 0.3) for o in others):
                        good = False
                    elif any(V1["seg_dist"](px, py, a_[0], a_[1], b_[0], b_[1]) < r for a_, b_, r in segs):
                        good = False
                    elif abs(ground(px * M, py * M) / M - b["z"]) > 0.4:
                        good = False
                    if not good:
                        break
                if not good:
                    break
            if good:
                ok.append(side)
        FREE_SIDES[b["id"]] = ok
    stats["outside_stair_sides"] = {k: v for k, v in FREE_SIDES.items()}


def buildings():
    homes = sorted(LAYOUT["houses"], key=lambda b: b["home_index"])
    infill = LAYOUT["infill"]
    civic_keys = [k for k, b in LAYOUT["landmarks"].items() if b.get("kind") == "civic" and b.get("interior")]
    allb = homes + infill + [dict(lm(k), id=lm(k).get("id", k)) for k in civic_keys]
    skips = party_walls(allb)
    free_sides(allb, skips)
    block_index = {}
    for b in allb:
        blk = b.get("block")
        ri = block_index.get(blk, 0)
        block_index[blk] = ri + 1
        kind = "home" if b in homes else ("infill" if b in infill else "civic")
        variant = int(b.get("style", 0)) + ri
        build_house(b, kind, skips.get(b["id"]), variant, ri)
    stats["party_wall_sides"] = sum(len(v) for v in skips.values())
    stats["facades"] = FACADES
    # towers
    tower(lm("bell_tower"), 4, light=(10.0, 900.0), district="crown_hill", crown="bell")
    tower(lm("clock_tower"), 4, light=(8.0, 800.0), district="heart", clock=True, crown="clock")
    tower(lm("lighthouse"), 5, light=(400.0, 9000.0, (255, 236, 190)), district="lighthouse_point", crown="lighthouse")
    # open halls
    fm = lm("fish_market")
    open_hall(fm, ("Roof_RoundTiles_6x8", 90.0), [(sx * 380.0, sy * 280.0) for sx in (-1, 0, 1) for sy in (-1, 1)],
              [(P + "Stall_Empty", -200.0, 0.0, 0.0), (P + "Stall_Empty", 200.0, 0.0, 0.0), (P + "Table_Large", 0.0, 150.0, 0.0),
               (DH + "FishRack", 0.0, 230.0, 0.0), (P + "Barrel", -330.0, 200.0, 0.0), (P + "Crate_Wooden", 330.0, 220.0, 30.0),
               (DH + "FishBasket", 120.0, -200.0, 0.0), (DH + "FishBarrel", -120.0, -210.0, 0.0)],
              eave=260.0, roof_z=0.55)          # top ~ 2 + 2.6 + 0.55 * 3.7 = 6.7 m < top_z 8.5
    V1["open_shed"](lm("smithy")["at"][0] * M, lm("smithy")["at"][1] * M, lm("smithy")["yaw"], folder="V2/Smithy")
    # SPRINT-022: the v1 shed drops its tools at pivot height; seat the pickaxe (pivot mid-handle) on the floor
    for a in _real_actors.get_all_level_actors():
        if str(a.get_folder_path()) == "V2/Smithy" and a.get_class().get_name() == "StaticMeshActor":
            sm = a.static_mesh_component.static_mesh
            if sm and sm.get_name() == "Pickaxe_Bronze":
                lo = sm.get_bounding_box().min.z
                p_ = a.get_actor_location()
                a.set_actor_location(unreal.Vector(p_.x, p_.y, p_.z - lo), False, False)


# =================================================================================================== landmarks
def gallows(spec):
    """v1 gallows (4x4 platform, stair, frame, noose) turned to face the fountain; TargetPoint KG_Gallows."""
    x, y = spec["at"][0] * M, spec["at"][1] * M
    gz = spec["z"] * M
    f = Frame(x, y, gz, spec["yaw"])
    fo = "V2/Gallows"
    for ox in (-100.0, 100.0):
        for oy in (-100.0, 100.0):
            f.put(V + "Stairs_Exterior_Platform", ox, oy, 0.0, 0.0, fo, tinted=False)
    f.put(V + "Stairs_Exterior_Straight", 0.0, 300.0, 0.0, 180.0, fo, tinted=False)
    for ox in (-180.0, 180.0):
        f.put(V + "Corner_Exterior_Wood", ox, -180.0, 100.0, 0.0, fo, scale=(1.4, 1.4, 1.2), tinted=False)
    f.put(V + "Corner_Exterior_Wood", -215.0, -180.0, 460.0, 0.0, fo, scale=(1.4, 1.4, 1.43), pitch=-90.0, tinted=False)
    f.put(V + "Corner_Exterior_Wood", 0.0, -180.0, 372.0, 0.0, fo, scale=(0.3, 0.3, 0.3), tinted=False)
    f.put(P + "Banner_1", -180.0, -165.0, 430.0, 0.0, fo, tinted=False)
    marker = _real_actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, gz + 100.0))
    marker.tags = ["KG_Gallows"]
    marker.set_actor_label("KG_Gallows")
    marker.set_folder_path("Gameplay")


def windmill(spec):
    """dress_countryside recipe: Windmill_Body + Windmill_Sails on an AKGSpinner (sails toward `face`)."""
    x0, y0 = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec["yaw"]
    base = spec["z"] * M - 6.0
    f = Frame(x0, y0, base, yaw)
    spawn(DV + "Windmill_Body", x0, y0, base, yaw - 90.0, folder="V2/Windmill", label="Windmill")
    hx, hy = f.world(0.0, -300.0)
    cls = unreal.load_class(None, "/Script/KillGodot.KGSpinner")
    if cls:
        s = _real_actors.spawn_actor_from_class(cls, unreal.Vector(hx, hy, base + 867.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw - 90.0))
        s.set_mesh(mesh(DV + "Windmill_Sails"))
        s.set_editor_property("spin_rate", unreal.Rotator(roll=20.0, pitch=0.0, yaw=0.0))
        s.set_actor_enable_collision(False)
        s.set_folder_path("V2/Windmill")
    else:
        spawn(DV + "Windmill_Sails", hx, hy, base + 867.0, yaw - 90.0, folder="V2/Windmill", collide=False)
    x, y = f.world(0.0, 260.0)
    warm_light(x, y, base + 420.0, 5.0, 700.0)


def waterwheel(spec):
    """Paddle wheel on the brook. SPRINT-022: the KG_DressLandmarks Waterwheel (rims, spokes, 16 paddles) turning on a
    KGSpinner about its axle; the older plank hub stays as the fallback without the pack."""
    x, y = spec["at"][0] * M, spec["at"][1] * M
    yaw = spec["face_deg"]          # axle direction; the wheel plane (yaw + 90) follows the brook
    zc = ground(x, y) + 170.0
    if have(DL + "Waterwheel"):
        spinner(DL + "Waterwheel", x, y, zc + 60.0, yaw - 90.0, unreal.Rotator(roll=0.0, pitch=-25.0, yaw=0.0),
                "V2/Waterwheel", label="Waterwheel")
        c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        for sg in (-1, 1):
            px, py = x + c * sg * 125.0, y + s_ * sg * 125.0
            spawn(V + "Corner_Exterior_Wood", px, py, ground(px, py) - 20.0, yaw, scale=(1.6, 1.6, (zc + 60.0 - ground(px, py) + 40.0) / 300.0),
                  folder="V2/Waterwheel")
        stats["waterwheel"] = "pack"
        return
    cls = unreal.load_class(None, "/Script/KillGodot.KGSpinner")
    hub = None
    if cls:
        hub = _real_actors.spawn_actor_from_class(cls, unreal.Vector(x, y, zc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw + 90.0))
        hub.set_mesh(mesh(PIR + "Barrel_3"))
        hub.set_editor_property("spin_rate", unreal.Rotator(roll=0.0, pitch=-25.0, yaw=0.0))
        hub.set_folder_path("V2/Waterwheel")
    for k in range(6):
        a = spawn(V + "Floor_WoodDark", x, y, zc, yaw + 90.0, pitch=k * 30.0, scale=(1.75, 0.45, 4.0),
                  folder="V2/Waterwheel", collide=False, movable=True)
        if hub and a:
            a.attach_to_actor(hub, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, False)
    # axle posts on both banks
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for sg in (-1, 1):
        px, py = x + c * sg * 120.0, y + s * sg * 120.0
        spawn(V + "Corner_Exterior_Wood", px, py, ground(px, py) - 20.0, yaw, scale=(1.4, 1.4, 0.72), folder="V2/Waterwheel")


def landmarks_s22():
    """SPRINT-022 small landmarks placed by the builder (the geometric ones - arch portals, buttresses, cliffs - come
    from the placements file): the Belvedere telescope on the crown parapet aimed at the harbour, a wall fountain."""
    if not have(DL + "Telescope"):
        return
    bt = lm("belvedere_telescope")
    x, y = bt["at"][0] * M, bt["at"][1] * M
    tx, ty = LAYOUT["basin"]["center"][0] * M, LAYOUT["basin"]["center"][1] * M
    phi = math.degrees(math.atan2(ty - y, tx - x))
    spawn(DL + "Telescope", x, y, ground(x, y) - 2.0, phi - 90.0, folder="V2/Landmarks", label="BelvedereTelescope")
    # wall fountain: on the Upper Wall facing Back Lane west, where the low side is open ground
    best = None
    for w in LAYOUT["retaining_walls"]:
        if not w["name"].startswith("upper_wall"):
            continue
        for (ax, ay), (bx, by) in zip(w["points"], w["points"][1:]):
            L_ = math.hypot(bx - ax, by - ay)
            if L_ < 4.0:
                continue
            mx, my = (ax + bx) / 2, (ay + by) / 2
            ux, uy = (bx - ax) / L_, (by - ay) / L_
            nx, ny = -uy, ux
            if ground((mx + nx * 2) * M, (my + ny * 2) * M) > ground((mx - nx * 2) * M, (my - ny * 2) * M):
                nx, ny = -nx, -ny
            fx, fy = mx + nx * 1.6, my + ny * 1.6
            dl = min(V1["seg_dist"](fx, fy, *l["points"][i], *l["points"][i + 1]) - l["width"] / 2.0
                     for l in LAYOUT["lanes"] for i in range(len(l["points"]) - 1))
            clear = all(not _in_rect(b, fx, fy, 0.8) for b in LAYOUT["houses"] + LAYOUT["infill"])
            if clear and 0.6 < dl < 3.0 and (best is None or dl < best[0]):
                best = (dl, mx + nx * 0.36, my + ny * 0.36, nx, ny)
    if best:
        _, x, y, nx, ny = best
        z = ground((x + nx * 0.5) * M, (y + ny * 0.5) * M)
        spawn(DL + "WallFountain", x * M, y * M, z - 2.0, math.degrees(math.atan2(ny, nx)) - 90.0, folder="V2/Landmarks",
              label="WallFountain")
        stats["wall_fountain"] = [round(x, 2), round(y, 2)]


def graveyard(spec):
    x, y = spec["at"][0] * M, spec["at"][1] * M
    gz = spec["z"] * M
    f = Frame(x, y, gz, spec["yaw"])
    w, d = spec["size"]
    fo = "V2/Graveyard"
    # stone rows (front of the graves faces the gate side, local -Y)
    for r in range(5):
        for c in range(7):
            lx, ly = -w * 50 + 250 + c * (w * 100 - 500) / 6, -d * 50 + 330 + r * 300
            if abs(lx) < 160:
                continue   # central path
            m = DW + ("Gravestone_Round" if (r + c) % 3 else "Gravestone_Cross")
            f.put(m, lx + rng.uniform(-20, 20), ly, -3.0, rng.uniform(-8, 8), fo, tinted=False)
    # iron fence around, gate gap on the front
    for side, n, e in (("front", w // 2, d * 50.0), ("back", w // 2, d * 50.0), ("left", d // 2, w * 50.0), ("right", d // 2, w * 50.0)):
        for k in range(n):
            off = -n * 100.0 + 100.0 + k * 200.0
            if side == "front" and abs(off) < 150:
                continue
            lx, ly, lyaw = {"front": (off, -e, 0.0), "back": (off, e, 180.0), "left": (-e, off, 90.0), "right": (e, off, -90.0)}[side]
            f.put(V + "Prop_MetalFence_Simple", lx, ly, 0.0, lyaw, fo, scale=(1.0, 1.0, 0.5), tinted=False)
    f.put(DW + "GateArch", 0.0, -d * 50.0, 0.0, 0.0, fo, tinted=False)
    f.put(N + "DeadTree_2", w * 40.0, -d * 30.0, 0.0, 40.0, fo, tinted=False)
    f.put(P + "CandleStick_Stand", 60.0, -d * 50.0 + 120.0, 0.0, 0.0, fo, tinted=False)
    warm_light(*f.world(60.0, -d * 50.0 + 120.0), gz + 160.0, 4.0, 500.0)


def fountain_square():
    fs = lm("fountain")
    spawn(DV + "Fountain", fs["at"][0] * M, fs["at"][1] * M, fs["z"] * M, 0.0, folder="V2/Square", label="Fountain")
    # KG_BotHub: where bots gather for meetings and the centre of their roaming (AKGBotController); the meeting ring
    # at the gallows opens toward it (AKGGameMode::StartMeeting).
    hub = _real_actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(fs["at"][0] * M, fs["at"][1] * M, fs["z"] * M + 100.0))
    hub.tags = ["KG_BotHub"]
    hub.set_actor_label("KG_BotHub")
    hub.set_folder_path("Gameplay")
    gallows(lm("gallows"))
    nb = lm("notice_board")
    V1["notice_board"](nb["at"][0] * M, nb["at"][1] * M, nb["yaw"])
    dt = lm("dead_tree")
    spawn(N + "DeadTree_2", dt["at"][0] * M, dt["at"][1] * M, dt["z"] * M - 5.0, 40.0, scale=1.4, folder="V2/Square")
    ms = lm("market_stalls")
    f = Frame(ms["at"][0] * M, ms["at"][1] * M, ms["z"] * M, ms["yaw"])
    for k, lx in enumerate((-250.0, 0.0, 250.0)):
        f.put(P + ("Stall_Empty" if k != 1 else "Stall_Cart_Empty"), lx, 0.0, 0.0, 0.0, "V2/Square", tinted=False)
    f.put(P + "Barrel_Apples", 380.0, -90.0, 0.0, 0.0, "V2/Square", tinted=False)
    f.put(P + "FarmCrate_Apple", -380.0, -80.0, 0.0, 25.0, "V2/Square", tinted=False)
    # benches facing the fountain, planters at the Town Hall steps, bunting to the Inn
    cx, cy = fs["at"][0] * M, fs["at"][1] * M
    for a in (35.0, 145.0, 215.0, 325.0):
        bx, by = cx + 430.0 * math.cos(math.radians(a)), cy + 430.0 * math.sin(math.radians(a))
        spawn(P + "Bench", bx, by, fs["z"] * M, a + 90.0, folder="V2/Square")
    th = lm("town_hall")
    tf = Frame(th["at"][0] * M, th["at"][1] * M, th["z"] * M, th["yaw"])
    for lx in (-330.0, 330.0):
        tf.put(DV + "Planter_Large", lx, -th["size"][1] * 50.0 - 120.0, 0.0, 0.0, "V2/Square", tinted=False)
    inn = lm("inn")
    ax, ay = tf.world(0.0, -th["size"][1] * 50.0)
    bx, by = inn["at"][0] * M, inn["at"][1] * M
    dx, dy = bx - ax, by - ay
    ln = math.hypot(dx, dy)
    spawn(DV + "Bunting", ax, ay, th["z"] * M + 560.0, math.degrees(math.atan2(dy, dx)), scale=(ln / 604.0, 1.0, 1.0),
          folder="V2/Square", collide=False)
    # four corner lamps
    poly = next(q for q in LAYOUT["squares"] if q["name"] == "fountain_square")["polygon"]
    for i in (0, 3, 5, 7):
        px, py = poly[i]
        vx, vy = cx / M - px, cy / M - py
        n = math.hypot(vx, vy)
        x, y = (px + vx / n * 1.2) * M, (py + vy / n * 1.2) * M
        V1["lamp_post"](x, y, math.degrees(math.atan2(-vx, vy)), folder="V2/Street")


def japan_garden():
    """SPRINT-022: the Sakura Garden from PL["garden"] (prep_v2_placements.garden): the koi pond on the flat lawn with
    real water and a dark bed, the arch bridge across its short axis (end blocks on the lawn, water under the arch),
    the torii across the Sakura Walk on level ground past the stair head, stone lanterns on plinths beside the paths,
    small lamps along the stepping stones, grounded rocks. Nothing stands on the Garden Stair any more."""
    G = PL.get("garden")
    fo = "V2/Garden"
    if not G:
        log("garden: no PL['garden'] (rerun prep_v2_placements.py)")
        return
    pd = G["pond"]
    px, py = pd["at"][0] * M, pd["at"][1] * M
    spawn(JP + "KoiPondRim", px, py, pd["rim_z"] * M, pd["yaw"], folder=fo, label="KoiPond")
    cx, cy = pd["centre"][0] * M, pd["centre"][1] * M
    # oval water and bed (flattened engine cylinders) that fit inside the rim's stones, not rectangles
    bed = spawn("/Engine/BasicShapes/Cylinder", cx, cy, pd["ground"] * M + 1.5, pd["yaw"], scale=(6.9, 4.1, 0.01), folder=fo,
                collide=False)
    bed_mat = unary_first("/Game/KillGodot/Materials/S22/M_KG_PondBed", None)
    if bed and bed_mat:
        bed.static_mesh_component.set_material(0, bed_mat)
    water = spawn("/Engine/BasicShapes/Cylinder", cx, cy, pd["water_z"] * M - 0.5, pd["yaw"], scale=(7.1, 4.3, 0.01), folder=fo,
                  collide=False, label="KoiPondWater")
    if water:
        water.static_mesh_component.set_material(0, unary_first("/Game/KillGodot/Materials/S22/MI_KG_Water_Koi",
                                                                 "/Game/KillGodot/Materials/M_KG_PondWater"))
    br = G["bridge"]
    spawn(JP + "ArchBridge", br["at"][0] * M, br["at"][1] * M, br["z"] * M, br["yaw"], folder=fo, label="KoiBridge")
    koi = _real_actors.spawn_actor_from_class(unreal.load_class(None, "/Script/KillGodot.KGFishSchool"),
                                              unreal.Vector(cx, cy, pd["water_z"] * M))
    koi.set_editor_property("fish_mesh", mesh(JP + "Koi"))
    for k, v in (("count", 7), ("radius", 150.0), ("min_depth", 8.0), ("max_depth", 18.0), ("speed", 40.0),
                 ("fish_scale", 0.9), ("species", "Koi")):
        koi.set_editor_property(k, v)
    koi.set_folder_path(fo)
    t = G["torii"]
    spawn(JP + "Torii", t["at"][0] * M, t["at"][1] * M, t["z"] * M + 41.0 * t["scale"] - 4.0, t["yaw"], scale=t["scale"], folder=fo,
          label="GardenTorii")
    pv = lm("pavilion")
    spawn(DW + "TeaHouse", pv["at"][0] * M, pv["at"][1] * M, pv["z"] * M + 4.0, pv["yaw"] - 90.0, scale=0.9, folder=fo, label="Pavilion")
    warm_light(pv["at"][0] * M, pv["at"][1] * M, pv["z"] * M + 250.0, 5.0, 600.0)
    gs = lm("giant_sakura")
    spawn(JP + "Sakura_A", gs["at"][0] * M, gs["at"][1] * M, gs["z"] * M - 10.0, 30.0, scale=1.6, folder=fo)
    for name, x, y, z, sc in G["trees"]:
        spawn(JP + name, x * M, y * M, z * M, rng.uniform(0, 360), scale=sc, folder=fo)
    for x, y, z, sc in G["rocks"]:
        spawn(JP + "GardenRock", x * M, y * M, z * M, rng.uniform(0, 360), scale=sc, folder=fo)
    for x, y, z in G["lamps"]:
        spawn(JP + "GardenLamp", x * M, y * M, z * M + 14.0, 0.0, folder=fo)
        warm_light(x * M, y * M, z * M + 84.0, 3.0, 350.0)
    for k, (x, y, z) in enumerate(G["lanterns"]):
        spawn(DL + "LanternPlinth", x * M, y * M, z * M, 15.0 * k, folder=fo) if have(DL + "LanternPlinth") else None
        top = z * M + (36.0 if have(DL + "LanternPlinth") else 0.0)
        spawn(JP + "ToroLantern", x * M, y * M, top + 6.0, 15.0 * k, folder=fo)
        if k < 2:
            warm_light(x * M, y * M, top + 150.0, 6.0, 500.0)


def unary_first(path, fallback):
    """Load a material asset, or the fallback path (None = no fallback)."""
    for p_ in (path, fallback):
        if p_ and unreal.EditorAssetLibrary.does_asset_exist(p_):
            return unreal.load_asset(p_)
    return None


def shrine_island():
    pg = lm("pagoda")
    ix, iy = pg["at"][0] * M, pg["at"][1] * M
    fo = "V2/Island"
    spawn("/Game/KillGodot/Env/Island/KG_Island/StaticMeshes/KG_Island", ix, iy, 0.0, folder=fo, label="ShrineIsland")
    st = lm("sea_torii")
    spawn(JP + "Torii", st["at"][0] * M, st["at"][1] * M, -200.0, st["face_deg"] - 90.0, folder=fo, label="SeaTorii")
    warm_light(st["at"][0] * M, st["at"][1] * M, 900.0, 25.0, 2500.0, folder=fo)
    spawn(JP + "Pagoda", ix, iy, 555.0, 180.0, folder=fo, label="Pagoda")
    for ox, oy in [(-450, -650), (450, -650), (-700, 200), (700, 250)]:
        spawn(JP + "ToroLantern", ix + ox, iy + oy, 555.0, 0.0, folder=fo)
        warm_light(ix + ox, iy + oy, 705.0, 8.0, 600.0, folder=fo)
    for name, ox, oy, sc in [("Sakura_A", -1100, 600, 1.1), ("Sakura_B", 1150, -400, 1.0), ("BonsaiPine", 900, 900, 1.2),
                             ("Sakura_A", -600, 1300, 0.9), ("Maple", 300, 1400, 1.0)]:
        spawn(JP + name, ix + ox, iy + oy, 470.0, rng.uniform(0, 360), scale=sc, folder=fo)


def harbour_light():
    x, y = LAYOUT["mole"]["light"][0] * M, LAYOUT["mole"]["light"][1] * M
    z = LAYOUT["mole"]["top_z"] * M
    fo = "V2/HarbourLight"
    if have(DT + "HarbourLight"):
        # plan 11.4: iron lantern tower on a stone plinth, green (starboard) glass glowing (M_KG_JapanGlow)
        spawn(DT + "HarbourLight", x, y, z, 0.0, folder=fo, label="HarbourLight")
        warm_light(x, y, z + 370.0, 14.0, 2600.0, folder=fo, color=(90, 255, 120))
        return
    spawn(V + "Stairs_Exterior_Platform", x, y, z, 0.0, scale=(0.9, 0.9, 0.6), folder=fo)
    spawn(V + "Corner_Exterior_Brick", x, y, z + 60.0, 0.0, scale=(2.2, 2.2, 1.1), folder=fo, district="lighthouse_point")
    spawn(P + "Lantern_Wall", x, y + 30.0, z + 330.0, 0.0, folder=fo)
    warm_light(x, y, z + 420.0, 12.0, 2500.0, folder=fo, color=(90, 255, 120))


def well_and_oak():
    w = lm("well")
    V1["well"](w["at"][0] * M, w["at"][1] * M)
    oak = lm("old_oak")
    spawn(N + "CommonTree_3", oak["at"][0] * M, oak["at"][1] * M, oak["z"] * M - 10.0, 20.0, scale=1.8, folder="V2/Square")


def farm_bits():
    fy = lm("farmyard")
    x, y = fy["at"][0] * M, fy["at"][1] * M
    for name, ox, oy, yaw in [("Stall_Cart_Empty", 300, -250, 70), ("Barrel_Holder", -300, 200, 0), ("FarmCrate_Carrot", 100, 300, 20),
                              ("FarmCrate_Empty", 170, 330, 0), ("Bucket_Wooden_1", -120, -300, 0)]:
        spawn(P + name, x + ox, y + oy, ground(x + ox, y + oy), yaw, folder="V2/Farm")
    spawn(V + "Prop_Wagon", x - 500.0, y + 50.0, ground(x - 500.0, y + 50.0), 70.0, folder="V2/Farm")
    pen = lm("pen")
    px, py = pen["at"][0] * M, pen["at"][1] * M
    hw = pen["size"][0] * 50.0
    for side in range(4):
        for k in range(5):
            t = -hw + 100.0 + k * 200.0
            if side == 3 and k == 2:
                continue   # gate
            lx, ly, yaw = [(t, -hw, 0.0), (t, hw, 0.0), (-hw, t, 90.0), (hw, t, 90.0)][side]
            spawn(V + "Prop_WoodenFence_Single", px + lx, py + ly, ground(px + lx, py + ly), yaw, folder="V2/Farm")
    V1["woodcutter"](lm("woodcutter")["at"][0] * M, lm("woodcutter")["at"][1] * M)


def slipway():
    """Boathouse slip: timber deck sloping from the quay (2.0) into the basin (-1.0) along the landmark facing."""
    sw = lm("slipway")
    x, y = sw["at"][0] * M, sw["at"][1] * M
    a = math.radians(sw["face_deg"])
    u = (math.cos(a), math.sin(a))                     # seaward
    pitch = -math.degrees(math.atan2(3.0, 8.0))
    for k in range(4):
        sc = -300.0 + k * 200.0
        z = 50.0 - sc * (3.0 / 8.0)
        for off in (-100.0, 100.0):
            px, py = x + u[0] * sc - u[1] * off, y + u[1] * sc + u[0] * off
            spawn(V + "Floor_WoodDark", px, py, z, sw["face_deg"], pitch=pitch, scale=(1.07, 1.0, 1.0), folder="V2/Harbour")
    spawn(WP + "Rowboat", x - u[0] * 150.0, y - u[1] * 150.0, 120.0, sw["face_deg"], pitch=pitch, folder="V2/Harbour")
    # SPRINT-022: the slip stands on timber piles down to the basin floor (it used to end in mid-water)
    for k in range(1, 4):
        sc = -300.0 + k * 200.0
        zt = 50.0 - sc * (3.0 / 8.0) - 12.0
        for off in (-180.0, 180.0):
            px, py = x + u[0] * sc - u[1] * off, y + u[1] * sc + u[0] * off
            zb = ground(px, py) - 20.0
            if zt - zb > 40.0:
                spawn(V + "Corner_Exterior_Wood", px, py, zb, sw["face_deg"], scale=(1.4, 1.4, (zt - zb) / 300.0), folder="V2/Harbour")


# =================================================================================================== placements file
def placements():
    n = 0
    for it in PL["items"]:
        p = it["p"]
        s = it.get("s", (1.0, 1.0, 1.0))
        path = it["m"] if not it["m"].startswith("E:") else "/Engine/BasicShapes/" + it["m"][2:]
        a = spawn(path, p[0], p[1], p[2], it.get("y", 0.0), it.get("pi", 0.0), it.get("ro", 0.0), s, it["f"],
                  collide=bool(it.get("c", 1)), hidden=bool(it.get("h", 0)), district=it.get("d"))
        if a and it["m"].startswith("V:Stairs_Exterior"):
            a.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
        n += 1
    water_mat = unreal.load_asset("/Game/KillGodot/Materials/M_KG_PondWater")
    # SPRINT-022 water family (kg_make_water_v2.py): brook ribbons flow along their local X, the mill pond is still;
    # the vertical koi-spill curtain keeps the old material.
    fam = {"V2/Water/Brook": unary_first("/Game/KillGodot/Materials/S22/MI_KG_Water_Brook", None),
           "V2/Water/Pond": unary_first("/Game/KillGodot/Materials/S22/MI_KG_Water_Pond", None),
           "V2/KoiSpill": unary_first("/Game/KillGodot/Materials/S22/MI_KG_Water_Koi", None)}
    for w in PL["water"]:
        a = spawn("/Engine/BasicShapes/Plane", w["p"][0], w["p"][1], w["p"][2], w["y"], w.get("pi", 0.0), w.get("ro", 0.0),
                  tuple(w["s"]), w["f"], collide=False)
        if a:
            mat = None if w.get("vertical") else fam.get(w["f"])
            a.static_mesh_component.set_material(0, mat or water_mat)
    for x, y, z, yaw, kind in PL["lamps"]:
        if kind == "jetty":
            spawn(PIR + "Torch_0", x, y, z - 25.0, 0.0, folder="V2/Jetty")
            warm_light(x, y, z + 230.0, 12.0, 900.0, folder="V2/Jetty")
        else:
            V1["lamp_post"](x, y, yaw, folder="V2/Street")
    log(f"placements: {n} items, {len(PL['water'])} water, {len(PL['lamps'])} lamps")


def nature():
    cls = unreal.load_class(None, "/Script/KillGodot.KGFoliageField")
    for bucket, collide, cull in (("forest", True, 26000.0), ("crops", False, 9000.0)):
        field = _real_actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0))
        field.set_actor_label(f"KG_{bucket.capitalize()}")
        field.set_folder_path("V2/Nature")
        for path, rows in PL["hism"][bucket].items():
            m = mesh(path)
            if not m:
                continue
            tr = [unreal.Transform(unreal.Vector(r[0], r[1], r[2]), unreal.Rotator(roll=0.0, pitch=0.0, yaw=r[3]),
                                   unreal.Vector(r[4], r[4], r[4])) for r in rows]
            cull_m = 34000.0 if "Pine" in path else cull
            field.add_instances(m, tr, collide, cull_m)
        stats[f"hism_{bucket}"] = field.get_instance_total()
    gcls = unreal.load_class(None, "/Script/KillGodot.KGGrassField")
    g = _real_actors.spawn_actor_from_class(gcls, unreal.Vector(0.0, 0.0, 0.0))
    g.set_actor_label("KG_Meadow")
    g.set_folder_path("V2/Nature")
    for layer, rows in enumerate(PL["grass"]):
        tr = [unreal.Transform(unreal.Vector(r[0], r[1], r[2]), unreal.Rotator(roll=0.0, pitch=0.0, yaw=r[3]),
                               unreal.Vector(r[4], r[4], r[5])) for r in rows]
        g.add_clumps(layer, tr)
    stats["grass"] = g.get_clump_count()


# =================================================================================================== world
def new_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        level_sub.load_level(LEVEL)
        # keep only the level's own infrastructure; volumes (nav bounds, post process) are Brush subclasses: clear them
        keep = ("WorldSettings", "Brush", "WorldDataLayers", "LevelBounds")
        doomed = [a for a in _real_actors.get_all_level_actors() if a.get_class().get_name() not in keep]
        _real_actors.destroy_actors(doomed)
        log(f"cleared {len(doomed)} actors")
    else:
        level_sub.new_level(LEVEL)
        log("new level")


def terrain_and_sea():
    for t in TERRAIN_TILES:
        a = spawn(TERRAIN_DIR + t, 0.0, 0.0, 0.0, folder="V2/Terrain", label=t.replace("SM_KG_", ""))
    sea = spawn("/Game/KillGodot/Env/Sea/KG_Sea/StaticMeshes/KG_Sea", 0.0, 0.0, 0.0, folder="V2/Terrain", label="Sea", collide=False)
    if sea:
        sea.static_mesh_component.set_editor_property("bounds_scale", 3.0)
        sm = unary_first("/Game/KillGodot/Materials/S22/MI_KG_Water_Sea", None)     # SPRINT-022 water family
        if sm:
            sea.static_mesh_component.set_material(0, sm)
            stats["sea_material"] = "MI_KG_Water_Sea"


def sea_life():
    cls = unreal.load_class(None, "/Script/KillGodot.KGFishSchool")
    O = LAYOUT["basin"]["center"]
    for species, x, y, count, radius, dmin, dmax, speed, scale in [
            ("Mackerel", O[0] * M - 600.0, O[1] * M + 300.0, 24, 450.0, 60.0, 200.0, 140.0, 1.0),
            ("Mackerel", 3800.0, 11000.0, 22, 520.0, 80.0, 260.0, 150.0, 1.0),
            ("Cod", -1500.0, 12500.0, 12, 700.0, 250.0, 500.0, 70.0, 1.0),
            ("Salmon", 8000.0, 12500.0, 14, 600.0, 120.0, 320.0, 170.0, 1.0),
            ("GoldenCarp", 4200.0, 13200.0, 3, 300.0, 150.0, 300.0, 60.0, 1.2)]:
        sch = _real_actors.spawn_actor_from_class(cls, unreal.Vector(x, y, 0.0))
        sch.set_editor_property("fish_mesh", mesh(WP + f"Fish_{species}"))
        for k, v in (("count", count), ("radius", radius), ("min_depth", dmin), ("max_depth", dmax), ("speed", speed),
                     ("fish_scale", scale), ("species", species)):
            sch.set_editor_property(k, v)
        sch.set_folder_path("V2/Sea/Fish")
    for x, y, count, radius, lo, hi in [(O[0] * M, O[1] * M, 7, 1600.0, -1900.0, -1200.0), (4500.0, 15000.0, 5, 1200.0, -2200.0, -1500.0),
                                         (6800.0, 9500.0, 4, 900.0, -3000.0, -2400.0)]:
        flock = _real_actors.spawn_actor_from_class(cls, unreal.Vector(x, y, 0.0))
        flock.set_editor_property("fish_mesh", mesh("/Game/KillGodot/Env/Birds/KG_Gull/StaticMeshes/KG_Gull"))
        for k, v in (("count", count), ("radius", radius), ("min_depth", lo), ("max_depth", hi), ("speed", 480.0),
                     ("fish_scale", 2.2), ("species", "Gull")):
            flock.set_editor_property(k, v)
        flock.set_folder_path("V2/Sky/Gulls")
    bcls = unreal.load_class(None, "/Script/KillGodot.KGBreakable")
    buoy = unreal.load_class(None, "/Script/KillGodot.KGBuoyancyComponent")
    for k, (bx, by) in enumerate(LAYOUT.get("boats", [])):
        b = _real_actors.spawn_actor_from_class(bcls, unreal.Vector(bx * M, by * M, 20.0),
                                                unreal.Rotator(roll=0.0, pitch=0.0, yaw=rng.uniform(0, 360)))
        b.set_mesh(mesh(WP + "Rowboat"))
        b.set_editor_property("health", 1.0e6)
        comp = b.get_component_by_class(buoy) if buoy else None
        if comp:
            comp.set_editor_property("full_depth", 8.0)
        b.set_folder_path("V2/Sea/Boats")
        stats["boats"] = stats.get("boats", 0) + 1


def breakables():
    # crate stack on the inner edge of the Quay Promenade (clear of the fish-pier ramp and the Net Stairs ope)
    for k in range(8):
        a = k * 0.8
        V1["breakable"]("Crate_Wooden" if k % 3 else "Barrel", 3050.0 + 110.0 * math.cos(a), 3440.0 + 80.0 * math.sin(a),
                        yaw=rng.uniform(0, 360), folder="V2/Breakables")
    fs = lm("fountain")["at"]
    for name, ox, oy in [("Crate_Wooden", 900, -620), ("Crate_Wooden", 960, -700), ("Barrel", -700, 900), ("Barrel", -760, 980)]:
        V1["breakable"](name, fs[0] * M + ox, fs[1] * M + oy, yaw=rng.uniform(0, 360), folder="V2/Breakables")
    jh = next(l for l in LAYOUT["lanes"] if l["name"] == "jetty_head")["points"]
    # SPRINT-022: along the head's centre line (on its walkable deck box), they used to hang off the side and drop
    ux, uy = jh[1][0] - jh[0][0], jh[1][1] - jh[0][1]
    n_ = math.hypot(ux, uy)
    ux, uy = ux / n_, uy / n_
    for k in range(3):
        s_ = 1.6 + k * 0.8
        V1["breakable"]("Crate_Wooden", (jh[0][0] + ux * s_) * M, (jh[0][1] + uy * s_) * M, 150.0, rng.uniform(-10, 10),
                        folder="V2/Breakables")


def ambient(sound, x, y, z, volume=1.0, attenuate=True):
    a = _real_actors.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(x, y, z))
    comp = a.get_component_by_class(unreal.AudioComponent)
    comp.set_sound(unreal.load_asset(AUDIO + sound))
    comp.set_editor_property("volume_multiplier", volume)
    if attenuate:
        comp.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO + "SA_KG_Ambience"))
    else:
        comp.set_editor_property("allow_spatialization", False)
    a.set_folder_path("V2/Audio")
    stats["sounds"] = stats.get("sounds", 0) + 1


def soundscape():
    ambient("A_Wind_Loop", 0, 0, 800, 0.35, attenuate=False)
    for x, y in [(-9000, 7000), (-6000, 6900), (-3000, 7000), (5600, 8000), (8500, 10300), (11500, 9200)]:
        ambient("A_Sea_Loop", x, y, 60, 1.0)
    O = LAYOUT["basin"]["center"]
    ambient("A_Lapping_Loop", O[0] * M - 1500, O[1] * M - 1500, 250, 0.9)
    ambient("A_Lapping_Loop", O[0] * M + 1500, O[1] * M - 1800, 250, 0.9)
    ambient("A_Gulls_Loop", O[0] * M, O[1] * M, 900, 0.9)
    lh = lm("lighthouse")["at"]
    ambient("A_Gulls_Loop", lh[0] * M, lh[1] * M, 2000, 0.8)
    ambient("A_Gulls_Loop", 4500, 15000, 1500, 0.8)
    for bx, by in ((-9500, -3000), (-4000, -9800), (3000, -10500), (10500, -2500), (-9800, 4000)):
        ambient("A_Birds_Loop", bx, by, ground(bx, by) + 600, 0.7)
    sm = lm("smithy")["at"]
    ambient("A_Fire_Loop", sm[0] * M, sm[1] * M, ground(sm[0] * M, sm[1] * M) + 120, 0.9)
    ww = lm("waterwheel")["at"]
    ambient("A_Lapping_Loop", ww[0] * M, ww[1] * M, ground(ww[0] * M, ww[1] * M) + 100, 0.6)


# =================================================================================================== gameplay
TOWER_TOP_TASKS = {"RingBell": ("bell_tower", 4), "FuelLighthouse": ("lighthouse", 5), "WindClock": ("clock_tower", 4)}
HINTS = {"FileReports": ("BookStand", 0), "LightCandles": ("CandleStick_Triple", 90), "PourAle": ("Barrel_Holder", 0),
         "BakeBread": ("Cauldron", 0), "UnloadFish": ("Crate_Wooden", 0), "FuelLighthouse": ("Barrel", 0),
         "FixBoat": ("Barrel", 0), "FeedAnimals": ("Stall_Cart_Empty", 0), "TendGraves": ("Bucket_Wooden_1", 0),
         "RingBell": ("Rope_2", 0), "WindClock": ("Key_Metal", 0), "FeedKoi": ("Bag", 0), "LightHarbourLamp": ("Bottle_1", 0),
         "GrindFlour": ("Bag", 0), "MendNets": None, "HarvestCarrots": ("FarmCrate_Carrot", 0), "ChopWood": None,
         "SharpenTools": ("Whetstone", 0), "DrawWater": ("Bucket_Wooden_1", 0), "StockStall": None, "PostNotice": None,
         "ForgeNails": None}


def _hint_clear(px, py):
    """True when (px, py) cm is outside every home / civic door apron (1.6 m) and every stair corridor."""
    for b in LAYOUT["houses"] + [v for v in LAYOUT["landmarks"].values() if v.get("interior")]:
        w, d = b["size"]
        cw = w // 2
        lx = -cw + 1 + 2 * (cw // 2) if w >= 4 else 0.0
        yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
        c, s_ = math.cos(yaw), math.sin(yaw)
        ly = -d / 2.0 - 0.8
        dx, dy = b["at"][0] + lx * c - ly * s_, b["at"][1] + lx * s_ + ly * c
        if math.hypot(px / M - dx, py / M - dy) < 1.6:
            return False
    for st in LAYOUT["stairs"]:
        if V1["seg_dist"](px / M, py / M, st["from"][0], st["from"][1], st["to"][0], st["to"][1]) < st["width"] / 2.0 + 0.5:
            return False
    return True


def task_stations():
    cls = unreal.load_class(None, "/Script/KillGodot.KGTaskStation")
    decks = [l for l in LAYOUT["lanes"] if l["kind"] in ("pier", "mole")]

    def deck_z(x, y):
        """Chores over the water (jetty head, mole tip) stand on the deck, not on the sea bed."""
        best, bz = 1e9, None
        for l in decks:
            for (ax, ay), (bx, by) in zip(l["points"], l["points"][1:]):
                d = V1["seg_dist"](x / M, y / M, ax, ay, bx, by) - l["width"] / 2.0
                if d < best:
                    best, bz = d, float(l["z"])
        return bz * M + 2.0 if best < 1.5 else None

    for t in LAYOUT["tasks"]:
        x, y = t["at"][0] * M, t["at"][1] * M
        z = ground(x, y)
        if z < 150.0 and deck_z(x, y) is not None:
            z = max(z, deck_z(x, y))
        if t["id"] in TOWER_TOP_TASKS:
            name, levels = TOWER_TOP_TASKS[t["id"]]
            tw = lm(name)
            f = Frame(tw["at"][0] * M, tw["at"][1] * M, tw["z"] * M + 2.0, tw["yaw"])
            x, y = f.world(-80.0, 80.0)
            z = tw["z"] * M + 2.0 + (levels - 1) * FLOOR_H
        st = _real_actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z))
        st.set_editor_property("task_id", t["id"])
        st.set_editor_property("task_name", t["name"])
        st.set_editor_property("work_seconds", float(t["secs"]))
        st.set_actor_label(f"Task_{t['id']}")
        st.set_folder_path("Gameplay/Tasks")
        hint = HINTS.get(t["id"])
        if hint:
            # SPRINT-022: the hint prop goes beside the station but never into a door apron or a stair corridor
            hx, hy = (x + 110.0, y + 60.0)
            for ox, oy in ((110.0, 60.0), (-110.0, 60.0), (110.0, -60.0), (-110.0, -60.0), (0.0, 130.0), (130.0, 0.0),
                           (-130.0, 0.0), (0.0, -130.0)):
                if t["id"] in TOWER_TOP_TASKS or _hint_clear(x + ox, y + oy):
                    hx, hy = x + ox, y + oy
                    break
            hz = z if (t["id"] in TOWER_TOP_TASKS or ground(hx, hy) < z - 150.0) else ground(hx, hy)
            if t["id"] in TOWER_TOP_TASKS:
                hx, hy = x + 40.0, y + 30.0
            spawn(P + hint[0], hx, hy, hz, hint[1], folder="Gameplay/TaskProps")
    if any(t["id"] == "MendNets" for t in LAYOUT["tasks"]):
        t = next(t for t in LAYOUT["tasks"] if t["id"] == "MendNets")
        spawn(DH + "NetRack", t["at"][0] * M + 150.0, t["at"][1] * M, ground(t["at"][0] * M + 150.0, t["at"][1] * M), 60.0,
              folder="Gameplay/TaskProps")
    stats["tasks"] = len(LAYOUT["tasks"])


def player_starts():
    for x, y, z, yaw in PL["starts"]:
        s = _real_actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, z + 20.0))
        s.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False)
        s.set_folder_path("Gameplay/Starts")
    stats["starts"] = len(PL["starts"])


def capture_camera():
    """Movable CameraActor for the offscreen capture tour (Tools/Unreal/kg_capture_v2.py); unused in play."""
    cam = _real_actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0.0, 0.0, 3000.0))
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_label("KG_CaptureCam")
    cam.tags = ["KG_CaptureCam"]
    cam.set_folder_path("Gameplay/Dev")


def minimap():
    """Coordinator hook: redraw T_KG_Map_Morrowmere_v2 with the real trees + place AKGMapInfo (saved with the level)."""
    if TEST_BUILD:
        cls = unreal.load_class(None, "/Script/KillGodot.KGMapInfo")
        info = _real_actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0)) if cls else None
        calm_water(info)
        return
    import kg_make_minimap
    importlib.reload(kg_make_minimap)
    info = kg_make_minimap.build_minimap("D:/Kill Godot/Tools/Level/morrowmere_layout_v2.json")
    calm_water(info)


def calm_water(info=None):
    """Sheltered harbour basin (plan section 6: swell x0.25 inside the mole): AKGMapInfo pushes these numbers into
    FKGWaves (swimming, buoyancy) and MPC_KG_Water (M_KG_Ocean WPO + tint) at load. v1 leaves bCalmWater off."""
    if info is None:
        info = next((a for a in _real_actors.get_all_level_actors() if a.get_class().get_name() == "KGMapInfo"), None)
    if not info:
        log("calm water: no KGMapInfo")
        return
    b = LAYOUT["basin"]
    mole_r = LAYOUT["mole"].get("radius", 35.5)
    try:
        info.set_editor_property("calm_water", bool(b.get("calm_mask", True)))
        info.set_editor_property("calm_centre", unreal.Vector2D(b["center"][0] * M, b["center"][1] * M))
        info.set_editor_property("calm_radius", (mole_r - 1.5) * M)      # full swell again at the mole's inner toe
        info.set_editor_property("calm_fade", 1000.0)
        info.set_editor_property("calm_wave_scale", 0.25)
        info.set_editor_property("calm_ripple", 2.5)
        info.set_editor_property("calm_tint", unreal.LinearColor(r=0.02, g=0.30, b=0.27, a=1.0))
        info.set_editor_property("calm_tint_amount", 0.45)
        mpc = unreal.load_asset("/Game/KillGodot/Materials/MPC_KG_Water")
        if mpc:
            info.set_editor_property("water_parameters", mpc)
        stats["calm_water"] = {"centre_m": b["center"], "radius_m": mole_r - 1.5, "fade_m": 10.0, "swell": 0.25}
        log(f"calm water: centre {b['center']} r {mole_r - 1.5} m, swell x0.25")
    except Exception as ex:
        log(f"calm water: AKGMapInfo lacks the Water properties (rebuild KillGodotEditor): {ex}")


def navigation():
    nb = LAYOUT["nav_bounds"]
    lo, hi = nb["min"], nb["max"]
    c = [(lo[i] + hi[i]) / 2.0 * M for i in range(3)]
    e = [(hi[i] - lo[i]) / 2.0 for i in range(3)]          # metres = brush half-extent (100 cm) * scale
    vol = _real_actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(*c))
    vol.set_actor_scale3d(unreal.Vector(*e))
    vol.set_folder_path("Gameplay")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    except Exception as ex:
        log(f"nav rebuild failed: {ex}")
    navs = [a for a in _real_actors.get_all_level_actors() if a.get_class().get_name() == "RecastNavMesh"]
    log(f"nav: bounds centre {c} scale {e}; RecastNavMesh actors after rebuild: {len(navs)}")
    stats["navmesh_actors"] = len(navs)


# =================================================================================================== report
def report():
    counts = {}
    classes = {}
    for a in _real_actors.get_all_level_actors():
        fp = str(a.get_folder_path())
        top = "/".join(fp.split("/")[:2]) if fp else "(root)"
        counts[top] = counts.get(top, 0) + 1
        cn = a.get_class().get_name()
        classes[cn] = classes.get(cn, 0) + 1
    stats["secs"] = round(time.time() - T0, 1)
    out = {"level": LEVEL, "stats": stats, "folders": counts, "classes": classes,
           "missing_meshes": sorted(set(stats.get("missing", [])))}
    stats.pop("missing", None)
    with open(REPORT, "w") as f:
        json.dump(out, f, indent=1)
    log(f"report -> {REPORT}: {json.dumps(stats)}")
    log(f"classes: {json.dumps(classes)}")


def step(name, fn, *a):
    t = time.time()
    try:
        fn(*a)
        log(f"step {name}: {time.time() - t:.1f}s, pieces {stats['pieces']}")
    except Exception:
        import traceback
        log(f"step {name} FAILED:\n{traceback.format_exc()}")
        stats.setdefault("failed_steps", []).append(name)


def build():
    new_level()
    district_materials()
    step("sky", V1["sky_and_light"])
    step("post", V1["look_post_process"])
    step("terrain", terrain_and_sea)
    step("placements", placements)
    step("buildings", buildings)
    step("square", fountain_square)
    step("well", well_and_oak)
    step("graveyard", graveyard, lm("graveyard"))
    step("mausoleum", lambda: spawn(DW + "Mausoleum", lm("mausoleum")["at"][0] * M, lm("mausoleum")["at"][1] * M,
                                    lm("mausoleum")["z"] * M, lm("mausoleum")["face_deg"] - 90.0, folder="V2/Graveyard",
                                    label="Mausoleum"))
    step("garden", japan_garden)
    step("island", shrine_island)
    step("harbour_light", harbour_light)
    step("windmill", windmill, lm("windmill"))
    step("waterwheel", waterwheel, lm("waterwheel"))   # axle along face_deg (wheel plane follows the brook)
    step("landmarks_s22", landmarks_s22)                # SPRINT-022: Belvedere telescope, Well Court wall fountain
    step("farm", farm_bits)
    step("slipway", slipway)
    step("nature", nature)
    step("sea_life", sea_life)
    step("breakables", breakables)
    step("sound", soundscape)
    step("tasks", task_stations)
    step("starts", player_starts)
    step("capture_cam", capture_camera)
    step("minimap", minimap)
    step("nav", navigation)
    report()
    if not TEST_BUILD:
        step("underground", lambda: importlib.import_module("kg_build_underground").build_into_current_level())  # KG_DIG hook (after the report: the village counts stay the village's)
    saved = level_sub.save_current_level()
    log(f"saved {LEVEL}: {saved} in {time.time() - T0:.0f}s")


# KG_V2_NOBUILD=1: import the helpers only (SPRINT-022 lab levels, tests); the normal run builds the map.
if __import__("os").environ.get("KG_V2_NOBUILD") != "1":
    build()
