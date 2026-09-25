"""Build the Morrowmere v2 underground into L_Morrowmere_v2: the well cellar, the old mine tunnel and the catacombs
under Crown Hill (Tools/Level/underground_layout.py), plus the surface ends of the passages and the map layer.

Headless (editor closed), standalone: loads the level, rebuilds the underground, saves:
  UnrealEditor-Cmd "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_build_underground.py" -unattended -nosplash -nopause -nullrhi
  options (after the script path, inside -script="..."): --no-save, --no-map (skip the map texture import),
  --check-level (build the underground alone into the scratch map L_KG_UndergroundCheck; saves nothing else)
From the village builder (hook, current level, the village step saves):
  import kg_build_underground; kg_build_underground.build_into_current_level()

Idempotent: everything it spawns is tagged KG_Underground (folder Underground/...) and removed first. It runs AFTER the
village report, so verify_v2_build.py's actor counts (ladders, chests, target points) are the village's own.
Pieces:
  floors / walls / ceilings on the 2 m grid: kit Floor_* + Wall_UnevenBrick_Straight (tinted dark MIs), the
  KG_DressUnder pack (Tools/Blender/kg_make_dress_underground.py): NicheWall_2m, CryptVault_2m, Mine*_2m, WellShaft,
  CryptStair, Ladder_4m, CryptGate, SkullPile, BoneScatter, Sarcophagus, CandleCluster (kit fallbacks if missing),
  gameplay: AKGPassage x4 (well rim <-> shaft top, mausoleum door <-> crypt stair door), AKGLadder (the shaft),
  AKGKeyGate (the vault, crypt key), AKGStorageChest x2 (Smugglers' Nook: Cellar loot + a crypt key; the vault:
  CryptVault loot), AKGBreakable barrels / urns, AKGFlickerLight candles and lanterns (no shadows, small radii),
  AKGUndergroundInfo (regions, below-ground volumes, the map texture, the underground look + ambience),
  TargetPoints tagged KG_Loc_* (kg.World.Goto Catacombs / Cellar ...).
Logs KG_UNDER lines; writes Saved/KG_UndergroundReport.json.
"""
import importlib
import json
import math
import os
import sys
import time
import zlib

import unreal

ROOT = "D:/Kill Godot"
for p in (f"{ROOT}/Tools/Level", f"{ROOT}/Tools/Unreal"):
    if p not in sys.path:
        sys.path.insert(0, p)
import underground_layout as U  # noqa: E402

importlib.reload(U)

LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
N = "/Game/KillGodot/Env/KG_Nature/StaticMeshes/"
DU = "/Game/KillGodot/Env/Dress/KG_DressUnder_Clean/StaticMeshes/SM_KG_"
KIT_MATS = "/Game/KillGodot/Env/KG_Village/Materials/"
UNDER_MATS = "/Game/KillGodot/Env/KG_Village/Materials/Underground/"
REPORT = f"{ROOT}/Saved/KG_UndergroundReport.json"
TAG = "KG_Underground"
M = 100.0
F = U.FLOOR_Z * M          # floor (cm)
WALL = 312.0
MINE = U.MINE_H * M
STAIR = U.STAIR_H * M
VAULT_TOP = WALL + 100.0
SAVE_ASSETS = True         # False in --check-level: write nothing but the new check map
CHECK_LEVEL = "/Game/KillGodot/Maps/Dev/L_KG_UndergroundCheck"
LIGHT_K = 0.55             # look round 2: moodier (every light scaled)   # SM_KG_CryptVault_2m: springing on the wall tops, top surface +1.0 m

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
stats = {"pieces": 0, "lights": 0, "gameplay": 0, "missing": []}
_mesh = {}
_mi = {}


def log(m):
    unreal.log(f"KG_UNDER {m}")


def mesh(path):
    if path not in _mesh:
        full = path if "." in path.rsplit("/", 1)[-1] else f"{path}.{path.rsplit('/', 1)[-1]}"
        _mesh[path] = unreal.load_asset(full) if eal.does_asset_exist(path) else None
        if not _mesh[path]:
            stats["missing"].append(path)
    return _mesh[path]


def pick(*paths):
    """First existing mesh path (pack piece, then kit fallbacks)."""
    for p in paths:
        if p and mesh(p):
            return p
    return None


# ------------------------------------------------------------------------------------------------ materials
TINTS = {   # kit MI kind -> multiplier per style (kit BaseColorFactor x tint)
    "crypt": {"UnevenBrick": (0.40, 0.45, 0.58), "Brick": (0.46, 0.50, 0.62), "RockTrim": (0.42, 0.46, 0.56),
              "WoodTrim": (0.50, 0.45, 0.45), "Plaster": (0.42, 0.46, 0.56), "RedBrick": (0.45, 0.45, 0.55)},
    "cellar": {"UnevenBrick": (0.62, 0.47, 0.36), "Brick": (0.66, 0.50, 0.38), "RockTrim": (0.6, 0.48, 0.38),
               "WoodTrim": (0.64, 0.50, 0.38), "Plaster": (0.62, 0.52, 0.42), "RedBrick": (0.66, 0.5, 0.4)},
}


def under_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    for style, kinds in TINTS.items():
        for kind, t in kinds.items():
            base = unreal.load_asset(f"{KIT_MATS}MI_{kind}")
            if not base:
                continue
            try:
                bc = mel.get_material_instance_vector_parameter_value(base, "BaseColorFactor")
            except Exception:
                bc = unreal.LinearColor(1, 1, 1, 1)
            name = f"MI_KG_Under_{style}_{kind}"
            path = UNDER_MATS + name
            mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
            if not mi:
                mi = tools.create_asset(name, UNDER_MATS.rstrip("/"), unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
                mi.set_editor_property("parent", base)
            mel.set_material_instance_vector_parameter_value(mi, "BaseColorFactor",
                                                             unreal.LinearColor(bc.r * t[0], bc.g * t[1], bc.b * t[2], bc.a))
            mel.update_material_instance(mi)
            if SAVE_ASSETS:
                eal.save_loaded_asset(mi)
            _mi[(style, kind)] = mi
    log(f"materials: {len(_mi)}")


def tint(actor, style):
    comp = actor.static_mesh_component
    m = comp.static_mesh
    if not m or style not in TINTS:
        return
    for i, sm in enumerate(m.get_editor_property("static_materials")):
        mi = sm.get_editor_property("material_interface")
        nm = mi.get_name() if mi else ""
        kind = nm[3:]
        if kind == "Plaster":
            kind = "UnevenBrick"   # the kit walls' plaster face: stone underground, both sides
        if nm.startswith("MI_") and (style, kind) in _mi:
            comp.set_material(i, _mi[(style, kind)])


# ------------------------------------------------------------------------------------------------ spawning
def put(path, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, folder="Underground", style=None, collide=True, shadow=False,
        cull=0.0):
    m = mesh(path) if path else None
    if not m:
        return None
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    comp = a.static_mesh_component
    comp.set_static_mesh(m)
    if isinstance(scale, (int, float)):
        scale = (scale, scale, scale)
    if tuple(scale) != (1.0, 1.0, 1.0):
        a.set_actor_scale3d(unreal.Vector(*scale))
    comp.set_editor_property("cast_shadow", shadow)
    if cull > 0:
        comp.set_editor_property("ld_max_draw_distance", cull)
    if not collide:
        a.set_actor_enable_collision(False)
    if style:
        tint(a, style)
    a.tags = [TAG]
    a.set_folder_path(folder)
    stats["pieces"] += 1
    return a


def spawn_class(path, x, y, z, yaw=0.0, folder="Underground/Gameplay"):
    cls = unreal.load_class(None, path)
    if not cls:
        stats["missing"].append(path)
        log(f"missing class {path} (rebuild KillGodotEditor)")
        return None
    a = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.tags = [TAG]
    a.set_folder_path(folder)
    stats["gameplay"] += 1
    return a


def light(x, y, z, intensity=10.0, radius=520.0, color=(1.0, 0.62, 0.32), flicker=0.18, folder="Underground/Lights"):
    a = spawn_class("/Script/KillGodot.KGFlickerLight", x, y, z, 0.0, folder)
    if not a:
        return None
    intensity *= LIGHT_K
    a.set_editor_property("base_intensity", intensity)
    a.set_editor_property("flicker", flicker)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=int(color[0] * 255), g=int(color[1] * 255), b=int(color[2] * 255), a=255))
    lc.set_editor_property("cast_shadows", False)
    stats["lights"] += 1
    return a


def marker(name, x, y, z):
    a = actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z))
    a.tags = [TAG, f"KG_Loc_{name}"]
    a.set_actor_label(f"KG_Loc_{name}")
    a.set_folder_path("Underground/Markers")
    return a


def clear():
    doomed = [a for a in actors.get_all_level_actors() if TAG in [str(t) for t in a.tags]]
    if doomed:
        actors.destroy_actors(doomed)
    log(f"cleared {len(doomed)} old underground actors")


# ------------------------------------------------------------------------------------------------ grid helpers
def cxy(i, j):
    x, y = U.centre(i, j)
    return x * M, y * M


def style_of(c):
    return U.CELLS[c]["style"] if c else None


def top_of(c):
    k = U.CELLS[c]["ceiling"]
    return {"vault": VAULT_TOP, "flat": WALL, "high": STAIR, "mine": MINE, "none": 0.0}[k]


SIDES = {"N": (0, -1), "S": (0, 1), "W": (-1, 0), "E": (1, 0)}


def edge_frame(i, j, side):
    """Edge midpoint (cm), the inward normal (into cell i,j) and the yaw that turns local -Y onto it."""
    x, y = cxy(i, j)
    dx, dy = SIDES[side]
    ex, ey = x + dx * U.CELL * M / 2.0, y + dy * U.CELL * M / 2.0
    n = (-dx, -dy)
    yaw = {(0, -1): 0.0, (0, 1): 180.0, (1, 0): 90.0, (-1, 0): -90.0}[n]
    return ex, ey, n, yaw


def wall(i, j, side, c):
    style = style_of(c)
    ex, ey, n, yaw = edge_frame(i, j, side)
    folder = f"Underground/{style.capitalize()}"
    if style == "mine":
        p = pick(DU + "MineWall_2m")
        if p:
            return put(p, ex, ey, F, yaw, folder=folder, style=None)
        return put(V + "Wall_UnevenBrick_Straight", ex - n[0] * 31, ey - n[1] * 31, F, yaw, scale=(1.0, 1.0, MINE / WALL),
                   folder=folder, style="cellar")
    niche = (c == "O") or (c == "C" and (i * 3 + j * 5) % 5 != 0)
    if style in ("crypt",) and niche and pick(DU + "NicheWall_2m"):
        return put(DU + "NicheWall_2m", ex, ey, F - 3.0, yaw, scale=(1.04, 1.0, 1.01), folder=folder)   # close the corner seams
    kit_style = "cellar" if style == "cellar" else "crypt"
    h = WALL
    if style == "shaft":
        h = 440.0
    # 1.21 wide so neighbours overlap at the corners (the kit relies on corner posts), 5 cm into the floor
    return put(V + "Wall_UnevenBrick_Straight", ex - n[0] * 31, ey - n[1] * 31, F - 5.0, yaw, scale=(1.21, 1.0, (h + 5.0) / WALL),
               folder=folder, style=kit_style)


def lintel(i, j, side, lo, hi, style):
    """Masonry filling an open edge between two ceiling heights (lo..hi above the floor)."""
    ex, ey, n, yaw = edge_frame(i, j, side)
    return put(V + "Wall_UnevenBrick_Straight", ex - n[0] * 31, ey - n[1] * 31, F + lo, yaw,
               scale=(1.0, 1.0, (hi - lo) / WALL), folder="Underground/Lintels", style=style)


def corridor_axis(i, j):
    """'X' if the cell runs east-west, 'Y' if north-south, None for junctions / rooms."""
    ns = bool(U.cell_at(i, j - 1)) or bool(U.cell_at(i, j + 1))
    ew = bool(U.cell_at(i - 1, j)) or bool(U.cell_at(i + 1, j))
    if ns and not ew:
        return "Y"
    if ew and not ns:
        return "X"
    return None


# ------------------------------------------------------------------------------------------------ the shell
def shell():
    stair_cells = set(tuple(c) for c in U.STAIR["cells"])
    for i, j, c in U.cells():
        x, y = cxy(i, j)
        st = style_of(c)
        # floor
        if st == "mine":
            put(pick(DU + "MineFloor_2m", V + "Floor_UnevenBrick"), x, y, F - (0 if pick(DU + "MineFloor_2m") else 1), 0.0,
                folder="Underground/Mine", style=None if pick(DU + "MineFloor_2m") else "cellar")
        elif (i, j) not in stair_cells:
            fp = V + ("Floor_Brick" if st == "cellar" else "Floor_UnevenBrick")
            put(fp, x, y, F - 1, 90.0 * ((i + j) % 2), scale=(1.03, 1.03, 1.0), folder="Underground/Floors",
                style="cellar" if st == "cellar" else "crypt")
        # ceiling
        kind = U.CELLS[c]["ceiling"]
        if kind == "vault":
            axis = corridor_axis(i, j)
            if axis and pick(DU + "CryptVault_2m"):
                put(DU + "CryptVault_2m", x, y, F + WALL, 0.0 if axis == "X" else 90.0, folder="Underground/Ceilings", shadow=True)
            else:
                put(V + "Floor_UnevenBrick", x, y, F + WALL + 1, 0.0, roll=180.0, folder="Underground/Ceilings", style="crypt", shadow=True)
                if kind == "vault":   # junction under a vault run: close the gap up to the crown
                    put(V + "Floor_UnevenBrick", x, y, F + VAULT_TOP + 1, 0.0, roll=180.0, folder="Underground/Ceilings",
                        style="crypt", shadow=True)
        elif kind == "flat":
            put(V + ("Floor_WoodDark" if st == "cellar" else "Floor_UnevenBrick"), x, y, F + WALL + 1, 0.0, roll=180.0,
                folder="Underground/Ceilings", style="cellar" if st == "cellar" else "crypt", shadow=True)
            if st == "cellar" and (i + j) % 2 == 0:   # timber beams under the cellar planks
                put(V + "Corner_Exterior_Wood", x, y - 90.0, F + WALL - 10.0, 0.0, pitch=90.0, scale=(0.9, 0.9, 0.68),
                    folder="Underground/Cellar", collide=False)
        elif kind == "mine":
            p = pick(DU + "MineCeiling_2m")
            if p:
                axis = corridor_axis(i, j)
                put(p, x, y, F + MINE, 90.0 if axis == "Y" else 0.0, folder="Underground/Mine", shadow=True)
            else:
                put(V + "Floor_UnevenBrick", x, y, F + MINE + 1, 0.0, roll=180.0, folder="Underground/Mine", style="cellar", shadow=True)
        elif kind == "high":
            put(V + "Floor_UnevenBrick", x, y, F + STAIR + 1, 0.0, roll=180.0, folder="Underground/Ceilings", style="crypt", shadow=True)
        # walls + lintels
        for side, (dx, dy) in SIDES.items():
            nb = U.cell_at(i + dx, j + dy)
            if (i, j) in stair_cells and side in ("W", "E", "N"):
                continue                          # the CryptStair prop has its own side walls and end wall
            if not nb:
                if not ((i, j) in stair_cells):
                    wall(i, j, side, c)
                continue
            if c == "X" or nb == "X":
                continue
            ha, hb = top_of(c), top_of(nb)
            if ha > hb + 1.0:
                # open edge into a lower neighbour: fill from its ceiling up to ours, facing into this cell
                lintel(i, j, side, hb, ha, "cellar" if st == "cellar" else "crypt")
    log(f"shell: {stats['pieces']} pieces")


# ------------------------------------------------------------------------------------------------ rooms
def at(i, j, fx=0.0, fy=0.0):
    """Point inside cell (i, j): fx, fy in -1..1 of the half cell."""
    x, y = cxy(i, j)
    return x + fx * U.CELL * M / 2.0, y + fy * U.CELL * M / 2.0


def candles(i, j, fx, fy, light_int=9.0, radius=480.0, color=(1.0, 0.6, 0.3), z=0.0):
    x, y = at(i, j, fx, fy)
    put(pick(DU + "CandleCluster", P + "Candle_2"), x, y, F + z, (i * 37 + j * 11) % 360, folder="Underground/Props", collide=False)
    light(x, y, F + z + 55.0, light_int, radius, color)


def crypt_props():
    # corridors: candles on every other cell, against a wall
    for i, j, c in U.cells("C"):
        if (i + j) % 2 == 0:
            side = next((s for s, (dx, dy) in SIDES.items() if not U.cell_at(i + dx, j + dy)), "N")
            dx, dy = SIDES[side]
            candles(i, j, dx * 0.7, dy * 0.7, 8.0, 460.0)
        elif (i * 7 + j) % 3 == 0:
            x, y = at(i, j, 0.35, -0.3)
            put(pick(DU + "BoneScatter"), x, y, F, (i * 53) % 360, folder="Underground/Props", collide=False)
    # crypt hall: candle stands, skulls, urns
    for (i, j, fx, fy) in ((5, 8, -0.4, -0.4), (7, 8, 0.4, -0.4)):
        x, y = at(i, j, fx, fy)
        put(P + "CandleStick_Stand", x, y, F, 0.0, folder="Underground/Props", style=None)
        light(x, y, F + 150.0, 12.0, 620.0)
    x, y = at(7, 10, 0.3, 0.3)
    put(pick(DU + "SkullPile"), x, y, F, 200.0, folder="Underground/Props")
    x, y = at(5, 10, -0.2, 0.2)
    put(pick(DU + "BoneScatter"), x, y, F, 30.0, folder="Underground/Props", collide=False)
    # corners, clear of the arches at (4|5, 9) and (7|8, 9) and the corridor mouths (walk check, kg_underground_walk.py)
    for k, (i, j, fx, fy) in enumerate(((5, 10, -0.7, 0.7), (7, 8, 0.75, -0.75), (7, 10, 0.75, 0.75))):
        breakable(P + ("Vase_2" if k % 2 == 0 else "Vase_4"), *at(i, j, fx, fy), "CryptUrn")
    # ossuary: skull walls (niche walls all round), piles, one candle
    for (i, j, fx, fy, yaw) in ((0, 8, 0.0, 0.0, 10.0), (1, 10, 0.2, 0.3, 140.0), (0, 10, -0.3, 0.4, 250.0)):
        x, y = at(i, j, fx, fy)
        put(pick(DU + "SkullPile"), x, y, F, yaw, folder="Underground/Props")
    x, y = at(1, 8, 0.0, -0.2)
    put(pick(DU + "BoneScatter"), x, y, F, 80.0, folder="Underground/Props", collide=False)
    candles(1, 9, 0.5, 0.0, 7.0, 520.0)
    breakable(P + "Vase_4", *at(0, 9, -0.6, 0.0), "CryptUrn")
    # crypt chapel: the sarcophagus between candles, a cold light from nowhere
    sx, sy = at(11, 9)
    put(pick(DU + "Sarcophagus", P + "Crate_Wooden"), sx, sy, F, 0.0, folder="Underground/Props")
    put(P + "Chalice", sx + 30.0, sy - 10.0, F + 82.0, 0.0, folder="Underground/Props", collide=False)
    candles(10, 9, 0.2, 0.0, 9.0, 520.0)
    candles(12, 9, -0.2, 0.0, 9.0, 520.0)
    light(sx, sy - 150.0, F + 260.0, 4.0, 700.0, (0.55, 0.7, 1.0), 0.0)
    breakable(P + "Vase_2", *at(12, 8, 0.6, -0.6), "CryptUrn")
    # the stair up to the mausoleum door
    si, sj = U.STAIR["cells"][-1]          # foot cell (south end)
    fx_, fy_ = cxy(si, sj)
    foot = (fx_, fy_ + U.CELL * M / 2.0)
    yaw = U.STAIR["yaw"]
    p = pick(DU + "CryptStair")
    if p:
        put(p, foot[0], foot[1], F, yaw, folder="Underground/Stair", shadow=True)
    else:
        # kit fallback: two straight flights, walls either side
        for k in range(2):
            put(V + "Stairs_Exterior_Straight", foot[0], foot[1] - 104.0 - k * 208.0, F + k * 120.0, -90.0, folder="Underground/Stair")
    top = (foot[0], foot[1] - 440.0)
    light(top[0], top[1] + 60.0, F + 240.0 + 160.0, 9.0, 520.0)
    candles(si, sj, -0.6, 0.7, 6.0, 380.0)
    # the vault: the treasure chest, coins, candles, gold light
    vx, vy = at(3, 1, 0.0, -0.3)
    chest = spawn_class("/Script/KillGodot.KGStorageChest", vx, vy, F, 90.0)
    if chest:
        chest.set_editor_property("starting_loot_table", "CryptVault")
        chest.set_editor_property("loot_seed", zlib.crc32(b"crypt_vault") & 0x7FFFFFFF)
        chest.set_editor_property("display_name", unreal.Text("The Crypt Hoard"))
    for (fx, fy) in ((-0.5, 0.2), (0.4, 0.4), (0.0, 0.7)):
        x, y = at(3, 2, fx, fy)
        put(P + "Coin_Pile_2", x, y, F, (fx * 90) % 360, folder="Underground/Props", collide=False)
    candles(2, 1, -0.3, -0.5, 8.0, 480.0)
    candles(4, 1, 0.3, -0.5, 8.0, 480.0)
    light(*at(3, 2), F + 200.0, 6.0, 650.0, (1.0, 0.8, 0.4), 0.05)
    put(pick(DU + "SkullPile"), *at(2, 3, -0.5, 0.5), F, 45.0, folder="Underground/Props")
    # the gate
    gi, gj = U.GATE["cell"]
    gx, gy = cxy(gi, gj)
    hinge = (gx - U.CELL * M / 2.0 + 5.0, gy - U.CELL * M / 2.0)
    gate = spawn_class("/Script/KillGodot.KGKeyGate", hinge[0], hinge[1], F, 0.0)
    if gate:
        gm = mesh(DU + "CryptGate")
        for comp in gate.get_components_by_class(unreal.StaticMeshComponent):
            if gm:
                comp.set_static_mesh(gm)
    candles(gi, gj, 0.6, 0.6, 6.0, 380.0)


def breakable(path, x, y, table, yaw=0.0):
    m = mesh(path)
    b = spawn_class("/Script/KillGodot.KGBreakable", x, y, F + 5.0, yaw, "Underground/Breakables")
    if b and m:
        b.set_mesh(m)
        b.set_editor_property("loot_table", table)
        b.set_editor_property("loot_seed", zlib.crc32(f"{x:.0f},{y:.0f}".encode()) & 0x7FFFFFFF)
    return b


def mine_props():
    for n, (i, j, c) in enumerate(U.cells("T")):
        if n % 3 == 0:
            side = next((s for s, (dx, dy) in SIDES.items() if not U.cell_at(i + dx, j + dy)), "E")
            ex, ey, nrm, yaw = edge_frame(i, j, side)
            put(P + "Lantern_Wall", ex + nrm[0] * 25.0, ey + nrm[1] * 25.0, F + 170.0, yaw + 180.0, folder="Underground/Mine",
                collide=False)
            light(ex + nrm[0] * 45.0, ey + nrm[1] * 45.0, F + 190.0, 9.0, 560.0, (1.0, 0.66, 0.36), 0.1)
        axis = corridor_axis(i, j)
        if n % 2 == 1 and axis:
            # against a side wall of a straight run (bends and junctions stay clear; walk check, kg_underground_walk.py)
            side = 0.65 if n % 4 == 1 else -0.65
            x, y = at(i, j, side, 0.2) if axis == "Y" else at(i, j, 0.2, side)
            put(N + f"Rock_Medium_{1 + n % 3}", x, y, F - 10.0, n * 47.0, scale=0.35, folder="Underground/Mine")
        if n % 4 == 2:
            x, y = at(i, j, -0.55, -0.4)
            put(N + "Mushroom_Common", x, y, F, n * 31.0, scale=0.6, folder="Underground/Mine", collide=False)
    # the broken wall: rubble where the miners hit the crypt
    for k, (i, j, fx, fy, s) in enumerate(((5, 16, -0.3, -0.5, 0.8), (7, 16, 0.3, -0.4, 0.7), (5, 17, -0.4, 0.3, 0.55))):
        x, y = at(i, j, fx, fy)
        put(N + f"Rock_Medium_{1 + k}", x, y, F - 15.0, k * 80.0, scale=s, folder="Underground/Mine")
    x, y = at(7, 17, 0.4, 0.3)
    put(P + "Pickaxe_Bronze", x, y, F + 55.0, 30.0, roll=-20.0, folder="Underground/Mine", collide=False)
    put(P + "Chain_Coil", *at(6, 17, 0.1, 0.4), F, 60.0, folder="Underground/Mine", collide=False)
    light(*at(6, 16, 0.0, -0.3), F + 220.0, 7.0, 600.0, (1.0, 0.62, 0.32), 0.12)


def cellar_props():
    # main room: racks, barrels, crates, a workbench, two lanterns
    for (i, j, side) in ((4, 26, "W"), (8, 29, "E")):
        ex, ey, n, yaw = edge_frame(i, j, side)
        put(P + "Barrel_Holder", ex + n[0] * 45.0, ey + n[1] * 45.0, F, yaw + 90.0, folder="Underground/Cellar", style=None)
    for k, (i, j, fx, fy) in enumerate(((4, 29, -0.5, 0.5), (5, 29, -0.3, 0.6), (8, 26, 0.5, -0.5), (7, 26, 0.6, -0.6))):
        x, y = at(i, j, fx, fy)
        put(P + "Barrel", x, y, F, k * 70.0, folder="Underground/Cellar")
    for (i, j, fx, fy) in ((6, 26, -0.4, -0.5), (5, 27, 0.2, 0.0), (7, 28, 0.3, 0.3)):
        breakable(P + "Barrel", *at(i, j, fx, fy), "Barrel")
    for k, (i, j, fx, fy) in enumerate(((4, 27, -0.5, 0.0), (4, 28, -0.6, 0.3))):
        x, y = at(i, j, fx, fy)
        put(P + "Crate_Wooden", x, y, F, k * 25.0, scale=0.85, folder="Underground/Cellar")
    put(P + "Workbench", *at(7, 29, 0.25, 0.3), F, 180.0, folder="Underground/Cellar")   # beside the shaft arch, not in it
    put(P + "Shelf_Small_Bottles", *at(7, 29, 0.0, 0.62), F + 110.0, 180.0, folder="Underground/Cellar", collide=False)
    for (i, j) in ((5, 27), (7, 28)):
        x, y = at(i, j)
        light(x, y, F + 250.0, 10.0, 700.0, (1.0, 0.64, 0.34), 0.08)
    # the barrel store: racks along the walls, a wall of barrels half hiding the tunnel mouth at (10, 25)
    for (i, j, side) in ((9, 27, "W"), (9, 29, "S")):
        ex, ey, n, yaw = edge_frame(i, j, side)
        put(P + "Barrel_Holder", ex + n[0] * 45.0, ey + n[1] * 45.0, F, yaw + 90.0, folder="Underground/Cellar")
    # the pile covers the west half of the mouth; the east half stays a squeeze-free way in (walk check: the old pile
    # closed the tunnel, kg_underground_walk.py)
    for k, (fx, fy) in enumerate(((-0.75, -0.7), (-0.45, -0.2), (-0.95, -0.1))):
        x, y = at(10, 26, fx, fy)
        put(P + "Barrel", x, y, F, k * 50.0, folder="Underground/Cellar")
    put(P + "Barrel", *at(10, 26, -0.75, -0.7), F + 88.0, 20.0, folder="Underground/Cellar")
    put(P + "Crate_Wooden", *at(10, 27, 0.5, 0.2), F, 10.0, scale=0.8, folder="Underground/Cellar")
    light(*at(10, 28), F + 240.0, 6.0, 520.0, (1.0, 0.6, 0.3), 0.1)
    # Smugglers' Nook: the loot chest (a crypt key always inside), candles, crates
    nx, ny = at(2, 27, -0.4, 0.3)
    chest = spawn_class("/Script/KillGodot.KGStorageChest", nx, ny, F, 90.0)
    if chest:
        item = unreal.KGItemStack()
        item.set_editor_property("item_id", "CryptKey")
        item.set_editor_property("count", 1)
        chest.set_editor_property("starting_items", [item])
        chest.set_editor_property("starting_loot_table", "Cellar")
        chest.set_editor_property("loot_seed", zlib.crc32(b"smugglers_nook") & 0x7FFFFFFF)
        chest.set_editor_property("display_name", unreal.Text("Smugglers' Chest"))
    put(P + "Crate_Wooden", *at(3, 28, 0.4, 0.5), F, 15.0, scale=0.75, folder="Underground/Cellar")
    candles(2, 28, -0.5, 0.5, 7.0, 420.0)


def well_shaft():
    wx, wy, wz = U.WELL
    x, y = wx * M, wy * M
    put(pick(DU + "WellShaft"), x, y, F, 0.0, folder="Underground/Shaft", shadow=True)
    lad_x = x + 55.0
    put(pick(DU + "Ladder_4m"), lad_x, y, F, 0.0, folder="Underground/Shaft", collide=False)
    ladder = spawn_class("/Script/KillGodot.KGLadder", lad_x, y, F, 0.0, "Underground/Shaft")
    if ladder:
        ladder.set_editor_property("height", 390.0)
    light(x, y, F + 400.0, 7.0, 520.0, (0.78, 0.88, 1.0), 0.0)
    # passages: the rim up top <-> the top of the ladder down here
    gz = wz * M
    top = spawn_class("/Script/KillGodot.KGPassage", x, y, gz, 0.0, "Underground/Passages")
    if top:
        top.set_editor_property("passage_id", "WellTop")
        top.set_editor_property("target_id", "WellShaft")
        top.set_editor_property("prompt", unreal.Text("Climb down the well"))
        top.set_editor_property("hitbox_extent", unreal.Vector(70.0, 70.0, 45.0))
        top.set_editor_property("hitbox_offset", unreal.Vector(0.0, 0.0, 120.0))
        top.set_editor_property("arrival_local", unreal.Vector(0.0, 150.0, 100.0))
        top.set_editor_property("arrival_yaw", 90.0)
        top.set_editor_property("sound_name", "S_Passage_Ladder")
    shaft = spawn_class("/Script/KillGodot.KGPassage", x, y, F, 0.0, "Underground/Passages")
    if shaft:
        shaft.set_editor_property("passage_id", "WellShaft")
        shaft.set_editor_property("target_id", "WellTop")
        shaft.set_editor_property("auto_exit", True)
        shaft.set_editor_property("hitbox_extent", unreal.Vector(75.0, 75.0, 60.0))
        shaft.set_editor_property("hitbox_offset", unreal.Vector(0.0, 0.0, 370.0))
        shaft.set_editor_property("arrival_local", unreal.Vector(20.0, 0.0, 250.0))   # on the ladder, facing it (+X)
        shaft.set_editor_property("arrival_yaw", 0.0)
        shaft.set_editor_property("arrive_falling", True)
        shaft.set_editor_property("sound_name", "S_Passage_Ladder")
    # the surface: a well mouth on the Old Well's platform (the rim you look into)
    put(pick(DU + "WellMouth"), x, y, gz + 80.0, 0.0, folder="Underground/Surface", collide=False, shadow=True)


def mausoleum_door():
    mx, my = U.MAUSOLEUM["at"]
    mz = U.MAUSOLEUM["z"] * M
    myaw = U.MAUSOLEUM["yaw"]
    door = spawn_class("/Script/KillGodot.KGPassage", mx * M, my * M, mz, myaw, "Underground/Passages")
    if door:
        # the mausoleum's arched door recess faces its local +Y (SM_KG_Mausoleum, kg_make_dress_wilds.py)
        door.set_editor_property("passage_id", "MausoleumDoor")
        door.set_editor_property("target_id", "CryptDoor")
        door.set_editor_property("prompt", unreal.Text("Descend into the catacombs"))
        door.set_editor_property("hitbox_extent", unreal.Vector(62.0, 35.0, 110.0))
        door.set_editor_property("hitbox_offset", unreal.Vector(0.0, 145.0, 140.0))
        door.set_editor_property("arrival_local", unreal.Vector(0.0, 340.0, 110.0))
        door.set_editor_property("arrival_yaw", 90.0)
        door.set_editor_property("sound_name", "S_Passage_Door")
    si, sj = U.STAIR["cells"][-1]
    fx_, fy_ = cxy(si, sj)
    foot_y = fy_ + U.CELL * M / 2.0
    yaw = U.STAIR["yaw"]                    # the stair rises along its local +X
    crypt = spawn_class("/Script/KillGodot.KGPassage", fx_, foot_y, F, yaw, "Underground/Passages")
    if crypt:
        crypt.set_editor_property("passage_id", "CryptDoor")
        crypt.set_editor_property("target_id", "MausoleumDoor")
        crypt.set_editor_property("prompt", unreal.Text("Climb out through the mausoleum"))
        crypt.set_editor_property("hitbox_extent", unreal.Vector(20.0, 60.0, 105.0))
        crypt.set_editor_property("hitbox_offset", unreal.Vector(465.0, 0.0, 240.0 + 105.0))
        crypt.set_editor_property("arrival_local", unreal.Vector(390.0, 0.0, 240.0 + 100.0))
        crypt.set_editor_property("arrival_yaw", 180.0)
        crypt.set_editor_property("sound_name", "S_Passage_Door")


def info_and_markers(do_map):
    data = json.load(open(f"{ROOT}/Art/Textures/Map/KG_MapRegions_Underground_v2.json", encoding="utf-8"))
    cls = unreal.load_class(None, "/Script/KillGodot.KGUndergroundInfo")
    if not cls:
        log("AKGUndergroundInfo missing (rebuild KillGodotEditor)")
        return
    info = actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, F))
    info.tags = [TAG]
    info.set_actor_label("KG_UndergroundInfo")
    info.set_folder_path("Underground/Gameplay")
    import kg_make_minimap as MM
    importlib.reload(MM)
    if do_map:
        tex = MM.import_texture(f"{ROOT}/Art/Textures/Map/T_KG_Map_Underground_v2.png", "Underground_v2")
    else:
        tex = unreal.load_asset("/Game/KillGodot/UI/Map/T_KG_Map_Underground_v2")
    if tex:
        info.set_editor_property("map_texture", tex)
    info.set_editor_property("world_min", unreal.Vector2D(*data["world_min"]))
    info.set_editor_property("world_max", unreal.Vector2D(*data["world_max"]))
    info.set_editor_property("map_title", unreal.Text("Underground"))
    info.set_editor_property("exposure_bias", -1.25)   # look round 2 (the village look is +0.4)
    info.set_editor_property("vignette", 0.65)
    info.set_editor_property("regions", [MM._region(r) for r in data["regions"]])
    boxes = []
    for lo, hi in data["volumes"]:
        b = unreal.Box()
        b.set_editor_property("min", unreal.Vector(*lo))
        b.set_editor_property("max", unreal.Vector(*hi))
        try:
            b.set_editor_property("is_valid", True)
        except Exception:
            b.set_editor_property("b_is_valid", True)
        boxes.append(b)
    info.set_editor_property("volumes", boxes)
    log(f"info: {len(data['regions'])} regions, {len(boxes)} volumes, texture {tex.get_path_name() if tex else None}")
    for name, (i, j) in (("Catacombs", (6, 9)), ("Well_Cellar", (6, 27)), ("Treasure_Vault", (3, 2)), ("Old_Mine_Tunnel", (10, 22)),
                         ("Crypt_Chapel", (11, 9)), ("Ossuary", (0, 9))):
        x, y = cxy(i, j)
        marker(name, x, y, F + 20.0)


def build_into_current_level(do_map=True):
    t0 = time.time()
    stats.update({"pieces": 0, "lights": 0, "gameplay": 0, "missing": []})
    clear()
    under_materials()
    shell()
    crypt_props()
    mine_props()
    cellar_props()
    well_shaft()
    mausoleum_door()
    info_and_markers(do_map)
    rep = {"level": LEVEL, "stats": {k: v for k, v in stats.items() if k != "missing"}, "missing": sorted(set(stats["missing"])),
           "cells": len(list(U.cells())), "secs": round(time.time() - t0, 1)}
    if SAVE_ASSETS:                     # the check map (--check-level) leaves the real level's report alone
        json.dump(rep, open(REPORT, "w"), indent=1)
    log(f"built: {json.dumps(rep)}")
    return rep


def main(argv):
    global SAVE_ASSETS
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if "--check-level" in argv:
        # the underground alone in a NEW scratch map (the real level untouched, no shared asset re-saved) for
        # Tools/Unreal/kg_underground_walk.ps1 -Map /Game/KillGodot/Maps/Dev/L_KG_UndergroundCheck
        SAVE_ASSETS = False
        if eal.does_asset_exist(CHECK_LEVEL):
            les.load_level(CHECK_LEVEL)
        else:
            les.new_level(CHECK_LEVEL)
        build_into_current_level(do_map=False)
        log(f"saved {CHECK_LEVEL}: {les.save_current_level()}")
        return
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    build_into_current_level(do_map="--no-map" not in argv)
    if "--no-save" not in argv:
        ok = les.save_current_level()
        log(f"saved {LEVEL}: {ok}")


if __name__ != "kg_build_underground":   # run as a script (commandlet / remote exec), not imported by the village builder
    main(sys.argv[1:])
