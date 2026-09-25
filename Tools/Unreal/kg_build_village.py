"""Build /Game/KillGodot/Maps/L_Morrowmere — the coastal fishing village — from the imported CC0 kits,
following Tools/Level/morrowmere_layout.json (lanes, zones, houses, landmarks, chores).

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_build_village.py --timeout 900      (live editor, PIE stopped)

Deterministic (seeded tooling RNG). Re-running rebuilds the level from scratch.
NOTE: unreal.Rotator positional order is (roll, pitch, yaw) — always use keywords.
UE frame: X east, +Y towards the sea (south in Blender), Z up, cm. Plaza centre at (0, -800).
"""
import importlib
import json
import math
import random
import sys

import unreal

sys.path.insert(0, "D:/Kill Godot/Tools/Unreal")
import kg_interiors  # noqa: E402  (stairs + furnished rooms)
import kg_dress_common  # noqa: E402  (building footprints)
importlib.reload(kg_interiors)

LEVEL = "/Game/KillGodot/Maps/L_Morrowmere"
V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
N = "/Game/KillGodot/Env/KG_Nature/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
TERRAIN = "/Game/KillGodot/Env/Terrain/KG_Terrain/StaticMeshes/SM_KG_Terrain"
HEIGHTS = "D:/Kill Godot/Art/Packed/KG_Terrain_heights.json"
PLAZA = (0.0, -800.0)
DOOR_CLASS = unreal.load_class(None, "/Script/KillGodot.KGDoor")
FLOOR_H = 300.0

rng = random.Random(1848)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_mesh_cache = {}
_hm = json.load(open(HEIGHTS))
stats = {"pieces": 0, "houses": 0, "nature": 0, "props": 0}


# --------------------------------------------------------------------------------------------------- helpers
def mesh(path):
    if path not in _mesh_cache:
        _mesh_cache[path] = unreal.load_asset(path)
        if not _mesh_cache[path]:
            unreal.log_warning(f"KG_VILLAGE missing mesh {path}")
    return _mesh_cache[path]


def ground(x, y):
    """Terrain height (cm) at UE (x, y), bilinear from the Blender heightmap."""
    size, step, n, h = _hm["size"], _hm["step"], _hm["n"], _hm["heights"]
    bx, by = x / 100.0 + size / 2, -y / 100.0 + size / 2
    fi, fj = max(0.0, min(n - 1e-3, bx / step)), max(0.0, min(n - 1e-3, by / step))
    i, j = int(fi), int(fj)
    tx, ty = fi - i, fj - j
    a, b = h[j * (n + 1) + i], h[j * (n + 1) + i + 1]
    c, d = h[(j + 1) * (n + 1) + i], h[(j + 1) * (n + 1) + i + 1]
    return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)


def place(path, x, y, z, yaw=0.0, scale=1.0, folder="Village", label=None, collide=True):
    m = mesh(path)
    if not m:
        return None
    a = actors.spawn_actor_from_object(m, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if scale != 1.0:
        a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    if not collide:
        a.set_actor_enable_collision(False)
    stats["pieces"] += 1
    return a


def warm_light(x, y, z, intensity=10.0, radius=900.0, folder="Village/Lights"):
    """Movable, shadowless warm point light (lanterns/torches). Candela units."""
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=170, b=95, a=255))
    lc.set_editor_property("cast_shadows", False)
    a.set_folder_path(folder)
    stats["lights"] = stats.get("lights", 0) + 1
    return a


def lamp_post(x, y, yaw, folder="Village/Street"):
    """3 m wooden post with the kit's wall lantern hanging off it; the arm points along local +Y after yaw."""
    z = ground(x, y)
    place(V + "Corner_Exterior_Wood", x, y, z, yaw, folder=folder)
    place(P + "Lantern_Wall", x, y, z + 175.0, yaw, folder=folder)
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    warm_light(x - 105.0 * s_, y + 105.0 * c, z + 230.0, 14.0, 1100.0)


class Frame:
    """Local house frame: origin at the footprint centre on the ground, local -Y is the front (door side)."""

    def __init__(self, x, y, z, yaw):
        self.x, self.y, self.z, self.yaw = x, y, z, yaw
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def put(self, path, lx, ly, lz, lyaw=0.0, folder="Village/Houses", scale=1.0):
        wx = self.x + lx * self.c - ly * self.s
        wy = self.y + lx * self.s + ly * self.c
        return place(path, wx, wy, self.z + lz, self.yaw + lyaw, scale, folder)


# --------------------------------------------------------------------------------------------------- houses
ROOFS = {(4, 4), (4, 6), (4, 8), (6, 4), (6, 6), (6, 8), (6, 10), (6, 12), (8, 8), (8, 10), (8, 12)}


def wall_run(f, length_cells, edge, floor_z, pieces, side_yaw):
    """One side of the house: `length_cells` 2 m wall pieces along the side, exterior facing outward."""
    for k in range(length_cells):
        off = -length_cells * 100.0 + 100.0 + k * 200.0
        piece = pieces(k, length_cells)
        if side_yaw == 0.0:      # front (local -Y)
            f.put(piece, off, -edge, floor_z, 0.0)
        elif side_yaw == 180.0:  # back
            f.put(piece, -off, edge, floor_z, 180.0)
        elif side_yaw == -90.0:  # left (local -X)
            f.put(piece, -edge, -off, floor_z, -90.0)
        else:                    # right
            f.put(piece, edge, off, floor_z, 90.0)


def build_house(x, y, yaw, w, d, style):
    """w, d in metres (even). Ground floor stone/brick, upper floor plaster, round-tile roof, interior."""
    gz = max(ground(x + dx, y + dy) for dx in (-w * 50, w * 50) for dy in (-d * 50, d * 50)) + 2.0
    f = Frame(x, y, gz, yaw)
    cw, cd = w // 2, d // 2
    lower = "UnevenBrick" if style % 2 == 0 else "Plaster"

    # Foundation skirt + floors.
    for i in range(cw):
        for j in range(cd):
            lx, ly = -w * 50 + 100 + i * 200, -d * 50 + 100 + j * 200
            f.put(V + ("Floor_Brick" if style % 3 else "Floor_RedBrick"), lx, ly, 1.0)
            if not (i == 0 and j >= cd - 2):          # stair well at the back-left
                f.put(V + ("Floor_WoodDark" if style % 2 else "Floor_WoodLight"), lx, ly, FLOOR_H)

    def ground_piece(k, n):
        if k == n // 2:
            return V + f"Wall_{lower}_Door_Round"
        return V + (f"Wall_{lower}_Window_Wide_Round" if k % 2 else f"Wall_{lower}_Straight")

    def side_ground(k, n):
        return V + (f"Wall_{lower}_Window_Thin_Round" if k % 2 == 1 else f"Wall_{lower}_Straight")

    def upper(k, n):
        return V + ("Wall_Plaster_Window_Wide_Round" if k % 2 == 0 else "Wall_Plaster_WoodGrid")

    ex, ey = w * 50.0, d * 50.0
    wall_run(f, cw, ey, 0.0, ground_piece, 0.0)
    wall_run(f, cw, ey, 0.0, side_ground, 180.0)
    wall_run(f, cd, ex, 0.0, side_ground, -90.0)
    wall_run(f, cd, ex, 0.0, side_ground, 90.0)
    for side, n, e in ((0.0, cw, ey), (180.0, cw, ey), (-90.0, cd, ex), (90.0, cd, ex)):
        wall_run(f, n, e, FLOOR_H, upper, side)
    for sx in (-1, 1):
        for sy in (-1, 1):
            f.put(V + ("Corner_Exterior_Brick" if lower == "UnevenBrick" else "Corner_Exterior_Wood"),
                  sx * ex, sy * ey, 0.0)
            f.put(V + "Corner_Exterior_Wood", sx * ex, sy * ey, FLOOR_H)

    # Roof + gable ends.
    rw, rd = (w, d) if (w, d) in ROOFS else (min(w, 8), min(d, 12))
    f.put(V + f"Roof_RoundTiles_{rw}x{rd}", 0.0, 0.0, 2 * FLOOR_H, folder="Village/Houses")
    if w in (4, 6, 8):
        f.put(V + f"Roof_Front_Brick{w}", 0.0, -ey, 2 * FLOOR_H, 0.0)
        f.put(V + f"Roof_Front_Brick{w}", 0.0, ey, 2 * FLOOR_H, 180.0)
    f.put(V + ("Prop_Chimney" if style % 2 else "Prop_Chimney2"), ex - 90.0, ey * 0.4, 2 * FLOOR_H + 60.0)

    # Real, openable door (KGDoor: replicated, lockable, breakable) in the front doorway, opening inwards.
    door_x = -cw * 100.0 + 100.0 + (cw // 2) * 200.0
    wx = f.x + (door_x - 61.5) * f.c - (-ey - 11.0) * f.s
    wy = f.y + (door_x - 61.5) * f.s + (-ey - 11.0) * f.c
    door = actors.spawn_actor_from_class(DOOR_CLASS, unreal.Vector(wx, wy, gz + 3.0),
                                         unreal.Rotator(roll=0.0, pitch=0.0, yaw=f.yaw - 90.0))
    door.set_folder_path("Village/Doors")
    # Brass door bell beside the door (ringing it is heard inside - interiors sprint).
    f.put("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Doorbell", door_x - 105.0, -ey - 31.0, 215.0, -90.0,
          folder="Village/Props")

    # Door + outside details.
    lx = door_x + 140.0
    f.put(P + "Lantern_Wall", lx, -ey - 31.0, 150.0, 180.0, folder="Village/Props")
    wx = f.x + lx * f.c - (-ey - 130.0) * f.s
    wy = f.y + lx * f.s + (-ey - 130.0) * f.c
    warm_light(wx, wy, gz + 205.0, 10.0, 800.0)
    if style % 3 == 0:
        f.put(V + "Prop_Vine4", -ex + 40.0, -ey - 30.0, 0.0, 0.0, folder="Village/Props")
    f.put(P + ("Barrel" if style % 2 else "FarmCrate_Empty"), ex - 60.0, -ey - 70.0, 0.0, rng.uniform(0, 360),
          folder="Village/Props")

    # Interior: stair up the back-left well + furnished rooms, hearth/bedside lights (Tools/Unreal/kg_interiors.py).
    stats["pieces"] += kg_interiors.furnish(f, w, d, style, warm_light)
    stats["houses"] += 1


# --------------------------------------------------------------------------------------------------- world
def new_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        level_sub.load_level(LEVEL)
        doomed = [a for a in actors.get_all_level_actors()
                  if not isinstance(a, (unreal.WorldSettings, unreal.Brush)) and a.get_class().get_name() != "WorldDataLayers"]
        actors.destroy_actors(doomed)
    else:
        level_sub.new_level(LEVEL)


def sky_and_light():
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(roll=0.0, pitch=-16.0, yaw=62.0))
    sun.set_actor_label("Sun")
    # Movable: nothing is baked (Lumen off, no lightmaps), so every light must be dynamic.
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property("intensity", 7.5)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=208, b=160, a=255))   # golden hour
    lc.set_editor_property("atmosphere_sun_light", True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000))
    sky.set_actor_label("SkyLight")
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 1.3)
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label("HeightFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.012)
    fc.set_editor_property("fog_height_falloff", 0.12)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.55, 0.68, 0.85, 1.0))
    clouds = actors.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0))
    clouds.set_actor_label("Clouds")
    cc = clouds.get_component_by_class(unreal.VolumetricCloudComponent)
    # Lower, thicker layer of big puffy cumulus (MI_KG_Clouds) so the sky reads dramatic from the village.
    cc.set_editor_property("layer_bottom_altitude", 2.2)
    cc.set_editor_property("layer_height", 5.5)
    cc.set_editor_property("material", unreal.load_asset("/Game/KillGodot/Materials/MI_KG_Clouds"))
    atm = [a for a in actors.get_all_level_actors() if a.get_class().get_name() == "SkyAtmosphere"][0]
    ac = atm.get_component_by_class(unreal.SkyAtmosphereComponent)
    ac.set_editor_property("mie_scattering_scale", 0.006)       # a touch of coastal haze
    ac.set_editor_property("rayleigh_scattering_scale", 0.04)   # deeper blue
    for a in (sun, sky, fog):
        a.set_folder_path("Lighting")


def terrain_and_sea():
    t = place(TERRAIN, 0, 0, 0, folder="Terrain", label="Terrain")
    # Wave-displaced ocean (M_KG_Ocean, synced with FKGWaves in C++); dense around the harbour.
    sea = place("/Game/KillGodot/Env/Sea/KG_Sea/StaticMeshes/KG_Sea", 0, 0, 0, folder="Terrain", label="Sea", collide=False)
    sea.get_component_by_class(unreal.StaticMeshComponent).set_editor_property("bounds_scale", 3.0)
    # Invisible sea floor blocker so nobody walks off into the ocean forever.
    return t


def look_post_process():
    vol = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    vol.set_actor_label("KG_PP_Look")
    vol.set_editor_property("unbound", True)
    s = vol.get_editor_property("settings")
    wb = unreal.WeightedBlendables()
    wb.set_editor_property("array", [unreal.WeightedBlendable(1.0, unreal.load_asset("/Game/KillGodot/Materials/M_KG_PP_Outline"))])
    s.set_editor_property("weighted_blendables", wb)
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", 0.4)
    s.set_editor_property("override_color_saturation", True)
    s.set_editor_property("color_saturation", unreal.Vector4(1.12, 1.12, 1.12, 1.0))
    vol.set_editor_property("settings", s)
    vol.set_folder_path("Lighting")


# =================================================================================================== layout-driven town
LAYOUT = json.load(open("D:/Kill Godot/Tools/Level/morrowmere_layout.json"))
M = 100.0   # layout metres -> cm


def lm(name):
    return LAYOUT["landmarks"][name]


def facing_yaw(at, face):
    """House front (local -Y) looks at `face`."""
    return math.degrees(math.atan2(face[1] - at[1], face[0] - at[0])) + 90.0


def place_rot(path, x, y, z, pitch=0.0, yaw=0.0, roll=0.0, scale3=(1.0, 1.0, 1.0), folder="Village/Props", collide=True):
    m = mesh(path)
    if not m:
        return None
    a = actors.spawn_actor_from_object(m, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    a.set_actor_scale3d(unreal.Vector(*scale3))
    a.set_folder_path(folder)
    if not collide:
        a.set_actor_enable_collision(False)
    stats["pieces"] += 1
    return a


def house_at(spec, style, folder_label=None):
    at, (w, d) = spec["at"], spec["size"]
    build_house(at[0] * M, at[1] * M, facing_yaw(at, spec["face"]), w, d, style)


def ladder(x, y, z, yaw, height, folder="Village/Ladders"):
    """KGLadder climb volume + visual rails/rungs from kit posts. Climber stands on the -X side (after yaw)."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGLadder")
    lad = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    lad.set_height(height)
    lad.set_folder_path(folder)
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for side in (-28.0, 28.0):   # rails along the ladder's local Y
        place_rot(V + "Corner_Exterior_Wood", x - s_ * side, y + c * side, z, yaw=yaw,
                  scale3=(0.35, 0.35, height / 300.0 + 0.05), folder=folder)
    rung = 30.0
    while rung < height:
        place_rot(V + "Corner_Exterior_Wood", x - s_ * -28.0, y + c * -28.0, z + rung, yaw=yaw, roll=-90.0,
                  scale3=(0.25, 0.25, 0.19), folder=folder, collide=False)
        rung += 38.0
    return lad


def tower(x, y, yaw, levels, style, folder="Village/Landmarks", light=None):
    """4x4 m stone tower: `levels` storeys, open arches on top, round-tile spire. Climbable: an inside ladder
    rises through a hatch in the lookout floor (the ground floor and the lookout are the only floors)."""
    gz = max(ground(x + dx, y + dy) for dx in (-200, 200) for dy in (-200, 200)) + 2.0
    f = Frame(x, y, gz, yaw)
    lower = "UnevenBrick"
    for lvl in range(levels):
        z = lvl * FLOOR_H
        top = lvl == levels - 1

        def piece(k, n, lvl=lvl, top=top):
            if top:
                return V + "Wall_Arch"
            if lvl == 0 and k == 0:
                return V + f"Wall_{lower}_Door_Round"
            return V + (f"Wall_{lower}_Window_Thin_Round" if lvl % 2 else f"Wall_{lower}_Straight")

        for side in (0.0, 180.0, -90.0, 90.0):
            wall_run(f, 2, 200.0, z, piece, side)
        for sx in (-1, 1):
            for sy in (-1, 1):
                f.put(V + "Corner_Exterior_Brick", sx * 200.0, sy * 200.0, z, folder=folder)
        if lvl == 0 or top:
            for i in (-100.0, 100.0):
                for j in (-100.0, 100.0):
                    if top and i > 0 and j < 0:
                        continue   # hatch above the ladder
                    f.put(V + "Floor_Brick", i, j, z + 1.0, folder=folder)
    f.put(V + "Roof_Tower_RoundTiles", 0.0, 0.0, levels * FLOOR_H, folder=folder)
    # Ladder in the hatch column, facing local +Y so you step off onto the lookout floor.
    lx, ly = 100.0, -10.0
    wx, wy = x + lx * f.c - ly * f.s, y + lx * f.s + ly * f.c
    ladder(wx, wy, gz + 2.0, yaw + 90.0, (levels - 1) * FLOOR_H)
    if light:
        warm_light(x, y, gz + (levels - 1) * FLOOR_H + 170.0, light[0], light[1], folder="Village/Lights")
    return gz


def open_shed(x, y, yaw, folder="Village/Smithy"):
    """Smithy: four tall posts under a 4x6 round-tile roof, open on all sides."""
    gz = ground(x, y) + 2.0
    f = Frame(x, y, gz, yaw)
    for sx in (-1, 1):
        for sy in (-1, 1):
            f.put(V + "Corner_Exterior_Wood", sx * 190.0, sy * 290.0, 0.0, folder=folder)
    f.put(V + "Roof_RoundTiles_4x6", 0.0, 0.0, 300.0, folder=folder)
    for i in (-100.0, 100.0):
        for j in (-200.0, 0.0, 200.0):
            f.put(V + "Floor_Brick", i, j, 1.0, folder=folder)
    for name, lx, ly, lyaw in [("Anvil", 0, -60, 0), ("Anvil_Log", 60, 80, 30), ("Cauldron", -110, 160, 0),
                               ("Workbench", 130, 0, 90), ("WeaponStand", -150, -200, 90), ("Whetstone", 100, 260, 0),
                               ("Barrel", -150, 250, 0), ("Chain_Coil", 60, -200, 0), ("Pickaxe_Bronze", 140, -120, 0)]:
        f.put(P + name, lx, ly, 2.0, lyaw, folder=folder)
    warm_light(x, y, gz + 250.0, 14.0, 900.0, folder=folder)   # forge glow


def well(x, y):
    gz = ground(x, y)
    place_rot(V + "Stairs_Exterior_Platform", x, y, gz, scale3=(0.75, 0.75, 0.8), folder="Village/Square")
    for sx in (-1, 1):
        place_rot(V + "Corner_Exterior_Wood", x + sx * 80.0, y, gz, scale3=(1.0, 1.0, 0.75), folder="Village/Square")
    place_rot(V + "Corner_Exterior_Wood", x - 95.0, y, gz + 225.0, pitch=-90.0, scale3=(1.0, 1.0, 0.63),
              folder="Village/Square")
    place_rot(V + "Roof_Wooden_2x1", x, y + 70.0, gz + 225.0, folder="Village/Square")
    place_rot(P + "Bucket_Wooden_1", x + 30.0, y - 70.0, gz + 80.0, folder="Village/Square")


def gallows(x, y):
    gz = ground(x, y)
    for ox in (-100.0, 100.0):
        for oy in (-100.0, 100.0):
            place_rot(V + "Stairs_Exterior_Platform", x + ox, y + oy, gz, folder="Village/Gallows")
    place_rot(V + "Stairs_Exterior_Straight", x, y + 300.0, gz, yaw=180.0, folder="Village/Gallows")
    for ox in (-180.0, 180.0):
        place_rot(V + "Corner_Exterior_Wood", x + ox, y - 180.0, gz + 100.0, scale3=(1.4, 1.4, 1.2), folder="Village/Gallows")
    place_rot(V + "Corner_Exterior_Wood", x - 215.0, y - 180.0, gz + 460.0, pitch=-90.0, scale3=(1.4, 1.4, 1.43),
              folder="Village/Gallows")
    place_rot(V + "Corner_Exterior_Wood", x, y - 180.0, gz + 372.0, scale3=(0.3, 0.3, 0.3), folder="Village/Gallows")
    place_rot(P + "Banner_1", x - 180.0, y - 165.0, gz + 430.0, folder="Village/Gallows")
    marker = actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, gz + 100.0))
    marker.tags = ["KG_Gallows"]
    marker.set_actor_label("KG_Gallows")
    marker.set_folder_path("Gameplay")


def notice_board(x, y, yaw):
    gz = ground(x, y)
    f = Frame(x, y, gz, yaw)
    for sx in (-1, 1):
        f.put(V + "Corner_Exterior_Wood", sx * 95.0, 0.0, 0.0, folder="Village/Square")
    board = place_rot(V + "Wall_Plaster_WoodGrid", x, y, gz + 80.0, yaw=yaw, scale3=(0.9, 0.35, 0.5),
                      folder="Village/Square")
    for k, name in enumerate(["Scroll_1", "Scroll_2", "Scroll_1"]):
        f.put(P + name, -50.0 + k * 50.0, -20.0, 150.0 + (k % 2) * 25.0, 90.0, folder="Village/Square")


def graveyard(cx, cy):
    gz = ground(cx, cy)
    for i in range(-3, 4):   # iron fence on the lane side
        place_rot(V + "Prop_MetalFence_Simple", cx + i * 196.0, cy + 560.0, ground(cx + i * 196.0, cy + 560.0),
                  scale3=(1.0, 1.0, 0.45), folder="Village/Graveyard")
    for row in range(3):
        for col in range(5):
            x, y = cx - 480.0 + col * 240.0, cy - 250.0 + row * 250.0
            place_rot(V + "Stairs_Exterior_Platform", x, y, ground(x, y) - 5.0, yaw=rng.uniform(-6, 6),
                      scale3=(0.34, 0.1, 0.62), folder="Village/Graveyard")
    place_rot(N + "DeadTree_2", cx + 520.0, cy - 300.0, ground(cx + 520.0, cy - 300.0), yaw=40.0, folder="Village/Graveyard")
    place_rot(P + "CandleStick_Stand", cx, cy + 380.0, ground(cx, cy + 380.0), folder="Village/Graveyard")


def farm(cx, cy):
    # Four fenced fields of carrots/crops, feed trough and the barn from the landmarks.
    for fx, fy in ((-520, -300), (220, -300), (-520, 350), (220, 350)):
        for r in range(4):
            for c in range(6):
                x, y = cx + fx + c * 110.0, cy + fy + r * 100.0
                place_rot(N + ("Plant_7" if (r + c) % 2 else "Grass_Wispy_Short"), x, y, ground(x, y), yaw=rng.uniform(0, 360),
                          folder="Village/Farm", collide=False)
    for i in range(-5, 6):
        for y in (cy - 420.0, cy + 780.0):
            place_rot(V + "Prop_WoodenFence_Single", cx + i * 205.0, y, ground(cx + i * 205.0, y), folder="Village/Farm")
    for name, ox, oy, yaw in [("FarmCrate_Carrot", -100, 80, 20), ("FarmCrate_Empty", 0, 60, 0), ("Bucket_Wooden_1", 90, 30, 0),
                              ("Stall_Cart_Empty", 600, 420, 90), ("Barrel_Holder", 700, 150, 0)]:
        place_rot(P + name, cx + ox, cy + oy, ground(cx + ox, cy + oy), yaw=yaw, folder="Village/Farm")
    place_rot(V + "Prop_Wagon", cx - 700.0, cy + 100.0, ground(cx - 700.0, cy + 100.0), yaw=70.0, folder="Village/Farm")


def woodcutter(cx, cy):
    for k in range(3):   # log pile (roof ridge logs laid down)
        place_rot(V + "Roof_Log", cx + 150.0, cy - 200.0 + k * 55.0, ground(cx, cy) - 385.0 * 0.4 + 25.0 * (k % 2),
                  yaw=90.0, scale3=(0.4, 0.4, 0.4), folder="Village/Woodcutter")
    for name, ox, oy in [("Anvil_Log", 0, 0), ("Axe_Bronze", 20, 10), ("Bucket_Wooden_1", -120, 60)]:
        place_rot(P + name, cx + ox, cy + oy, ground(cx + ox, cy + oy) + (60.0 if name == "Axe_Bronze" else 0.0),
                  roll=(70.0 if name == "Axe_Bronze" else 0.0), folder="Village/Woodcutter")
    for k in range(4):
        a = k * 1.7
        x, y = cx + 500.0 * math.cos(a), cy + 500.0 * math.sin(a)
        place_rot(N + "DeadTree_1", x, y, ground(x, y) - 30.0, yaw=rng.uniform(0, 360), scale3=(0.35, 0.35, 0.18),
                  folder="Village/Woodcutter")   # stumps


def fish_market(cx, cy):
    for k, (ox, oy, yaw) in enumerate([(-450, -150, 90), (450, -150, -90), (0, 250, 180)]):
        x, y = cx + ox, cy + oy
        place_rot(P + "Stall_Empty", x, y, ground(x, y), yaw=yaw, folder="Village/FishMarket")
        place_rot(P + "Table_Large", x, y, ground(x, y), yaw=yaw + 90.0, folder="Village/FishMarket")
        fish = "/Game/KillGodot/Items/Melee/Swordfish_7hMOlBjln0/StaticMeshes/SM_KG_Swordfish"
        place_rot(fish, x, y, ground(x, y) + 80.0, yaw=yaw + 80.0, scale3=(0.5, 0.5, 0.5), folder="Village/FishMarket")
    for name, ox, oy in [("Barrel", -300, 300), ("Barrel", -250, 360), ("Crate_Wooden", 300, 320), ("Crate_Wooden", 360, 250),
                         ("Rope_3", 150, -350), ("Rope_2", -100, -380), ("Bucket_Metal", 200, 100), ("Chain_Coil", -350, -350)]:
        place_rot(P + name, cx + ox, cy + oy, ground(cx + ox, cy + oy), yaw=rng.uniform(0, 360), folder="Village/FishMarket")


def dock_from(x0, y_start):
    y0 = y_start
    while ground(x0, y0) > 60.0 and y0 < 9000:
        y0 += 100.0
    deck_z = 110.0
    for j in range(22):
        y = y0 - 400.0 + j * 200.0
        for sx in (-100.0, 100.0):
            place(V + "Floor_WoodDark", x0 + sx, y, deck_z, 0.0, folder="Village/Dock")
    end = y0 - 400.0 + 21 * 200.0
    for name, ox, oy in [("Barrel", -120, 600), ("Crate_Wooden", 130, 900), ("Rope_2", 60, 1500), ("Barrel", 140, 2400),
                         ("Crate_Metal", -140, 2800), ("Bucket_Wooden_1", 90, 3300)]:
        place(P + name, x0 + ox, y0 + oy - 400.0, deck_z + 2.0, rng.uniform(0, 360), folder="Village/Dock")
    for sy in (y0 + 400.0, end):
        for sx in (-230.0, 230.0):
            place(V + "Corner_Exterior_Wood", x0 + sx, sy, deck_z - 20.0, 0.0, folder="Village/Dock")
            place(P + "Lantern_Wall", x0 + sx, sy, deck_z + 155.0, -90.0 if sx < 0 else 90.0, folder="Village/Dock")
            warm_light(x0 + sx * 0.55, sy, deck_z + 210.0, 12.0, 1000.0, folder="Village/Dock")
    return end


def lane_lamps():
    for lane in LAYOUT["lanes"]:
        pts = [(p[0] * M, p[1] * M) for p in lane["points"]]
        off = lane["width"] * 50.0 + 120.0
        carry, side = 700.0, 1
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            seg = math.hypot(bx - ax, by - ay)
            dx, dy = (bx - ax) / seg, (by - ay) / seg
            t = carry
            while t < seg:
                px, py = ax + dx * t, ay + dy * t
                nx, ny = -dy * side, dx * side
                x, y = px + nx * off, py + ny * off
                if ground(x, y) > 120.0:
                    # Lantern arm (local +Y after yaw) points back over the lane.
                    lamp_post(x, y, math.degrees(math.atan2(-ny, -nx)) - 90.0)
                t += 1500.0
                side = -side
            carry = t - seg


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def is_clear(x, y, margin=300.0):
    """Away from lanes, yards, houses and landmarks (cm)."""
    for lane in LAYOUT["lanes"]:
        pts = [(p[0] * M, p[1] * M) for p in lane["points"]]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            if seg_dist(x, y, ax, ay, bx, by) < lane["width"] * 50.0 + margin:
                return False
    for a in LAYOUT["areas"]:
        if math.hypot(x - a["center"][0] * M, y - a["center"][1] * M) < a["radius"] * M + margin:
            return False
    for h in LAYOUT["houses"] + [v for v in LAYOUT["landmarks"].values()]:
        r = (max(h["size"]) * 50.0 if "size" in h else 450.0) + margin + 150.0
        if math.hypot(x - h["at"][0] * M, y - h["at"][1] * M) < r:
            return False
    return True


def nature_layout():
    trees = [f"CommonTree_{i}" for i in range(1, 6)]
    pines = [f"Pine_{i}" for i in range(1, 6)]
    rocks = [f"Rock_Medium_{i}" for i in range(1, 4)]
    # Grass itself comes from the KGGrassField meadow; statics are only accents now.
    small = ["Bush_Common", "Bush_Common_Flowers", "Flower_3_Group", "Flower_4_Group", "Fern_1", "Plant_1_Big"]

    # Dense forests go into one AKGFoliageField (HISM per mesh) when the class exists; else plain actors.
    field_cls = unreal.load_class(None, "/Script/KillGodot.KGFoliageField")
    field = None
    batches = {}
    if field_cls:
        field = actors.spawn_actor_from_class(field_cls, unreal.Vector(0.0, 0.0, 0.0))
        field.set_actor_label("KG_Forest")
        field.set_folder_path("Nature")

    def scatter(names, count, rmin, rmax, smin, smax, folder, margin=300.0, max_h=4500.0, collide=True):
        if field and collide and folder != "Nature/Ground":
            return scatter_instanced(names, count, rmin, rmax, smin, smax, margin, max_h)
        placed, tries = 0, 0
        while placed < count and tries < count * 30:
            tries += 1
            a, r = rng.uniform(0, 2 * math.pi), rng.uniform(rmin, rmax)
            x, y = r * math.cos(a), r * math.sin(a) - 800.0
            z = ground(x, y)
            if z < 150.0 or z > max_h or not is_clear(x, y, margin):
                continue
            place(N + rng.choice(names), x, y, z - 10.0, rng.uniform(0, 360), rng.uniform(smin, smax), folder, collide=collide)
            placed += 1
            stats["nature"] += 1

    def scatter_instanced(names, count, rmin, rmax, smin, smax, margin, max_h):
        placed, tries = 0, 0
        while placed < count and tries < count * 30:
            tries += 1
            a, r = rng.uniform(0, 2 * math.pi), rng.uniform(rmin, rmax)
            x, y = r * math.cos(a), r * math.sin(a) - 800.0
            z = ground(x, y)
            if z < 150.0 or z > max_h or not is_clear(x, y, margin):
                continue
            sc = rng.uniform(smin, smax)
            batches.setdefault(rng.choice(names), []).append(unreal.Transform(
                unreal.Vector(x, y, z - 10.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=rng.uniform(0, 360)),
                unreal.Vector(sc, sc, sc)))
            placed += 1
            stats["nature"] += 1

    dense = 2.6 if field else 1.0
    scatter(trees, int(230 * dense), 1500, 14000, 0.9, 1.6, "Nature/Trees", margin=250.0)
    scatter(pines, int(380 * dense * 1.4), 7000, 30000, 1.2, 2.8, "Nature/Pines", max_h=7800)
    scatter(rocks, int(70 * dense * 0.8), 3000, 26000, 1.0, 3.5, "Nature/Rocks", max_h=8200)
    for name, tr in batches.items():
        field.add_instances(mesh(N + name), tr, True, 32000.0 if name.startswith("Pine") else 22000.0)
    scatter(small, 220, 800, 11000, 0.9, 1.6, "Nature/Ground", margin=80.0, collide=False)


def lane_distance(x, y):
    best = 1e9
    for lane in LAYOUT["lanes"]:
        pts = [(p[0] * M, p[1] * M) for p in lane["points"]]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            best = min(best, seg_dist(x, y, ax, ay, bx, by) - lane["width"] * 50.0)
    return best


def grass_fields():
    """Tsushima-style meadow: ~120 cm jittered grid of wind-swept clumps, patchy tall grass, short at lane edges."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGGrassField")
    field = actors.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0))
    field.set_actor_label("KG_Meadow")
    field.set_folder_path("Nature")
    layers = [[], [], [], []]
    step = 120.0
    y = -11000.0
    while y < 5600.0:
        x = -11500.0
        while x < 11500.0:
            jx, jy = x + rng.uniform(-45, 45), y + rng.uniform(-45, 45)
            z = ground(jx, jy)
            # No grass on sand, rock heights, or anything built/walked on.
            if 250.0 < z < 2600.0 and is_clear(jx, jy, 40.0):
                lane = lane_distance(jx, jy)
                patch = math.sin(jx * 0.0011 + 1.3) * math.cos(jy * 0.0009 - 0.4) + 0.35 * math.sin(jx * 0.0041 + jy * 0.0033)
                if lane < 250.0:
                    layer = 0
                elif patch > 0.45:
                    layer = 2
                elif patch < -0.65 and rng.random() < 0.5:
                    layer = 3
                else:
                    layer = 1 if rng.random() < 0.7 else 0
                sc = rng.uniform(0.8, 1.3)
                layers[layer].append(unreal.Transform(unreal.Vector(jx, jy, z - 3.0),
                                                      unreal.Rotator(roll=0.0, pitch=0.0, yaw=rng.uniform(0, 360)),
                                                      unreal.Vector(sc, sc, sc * rng.uniform(0.85, 1.15))))
            x += step
        y += step
    for i, tr in enumerate(layers):
        field.add_clumps(i, tr)
    stats["grass"] = sum(len(l) for l in layers)


BREAKABLE_CLASS = None


def breakable(name, x, y, z=None, yaw=0.0, folder="Village/Breakables"):
    """Punchable/physics crate or barrel (AKGBreakable)."""
    global BREAKABLE_CLASS
    BREAKABLE_CLASS = BREAKABLE_CLASS or unreal.load_class(None, "/Script/KillGodot.KGBreakable")
    zz = ground(x, y) + 5.0 if z is None else z
    b = actors.spawn_actor_from_class(BREAKABLE_CLASS, unreal.Vector(x, y, zz), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    b.set_mesh(mesh(P + name))
    b.set_folder_path(folder)
    stats["breakables"] = stats.get("breakables", 0) + 1
    return b


def breakables():
    fm = [a for a in LAYOUT["areas"] if a["name"] == "fish_market"][0]["center"]
    cx, cy = fm[0] * M, fm[1] * M
    for k in range(10):   # stacks by the fish market
        a = k * 0.63
        breakable("Crate_Wooden" if k % 3 else "Barrel", cx + 520.0 * math.cos(a), cy - 300.0 + 260.0 * math.sin(a),
                  yaw=rng.uniform(0, 360))
    for k in range(3):    # a small pyramid of crates on the dock
        breakable("Crate_Wooden", -150.0 + k * 75.0, 6400.0, 115.0, rng.uniform(-10, 10))
    breakable("Crate_Wooden", -110.0, 6400.0, 190.0, 15.0)
    breakable("Barrel", 150.0, 7600.0, 115.0)
    breakable("Barrel", 150.0, 7700.0, 115.0)
    for name, ox, oy in [("Crate_Wooden", 1500, 150), ("Crate_Wooden", 1560, 60), ("Barrel", -1300, -300),
                         ("Barrel", -1360, -220), ("Crate_Wooden", 300, 1200), ("FarmCrate_Empty", -500, 1100)]:
        breakable(name, PLAZA[0] + ox, PLAZA[1] + oy, yaw=rng.uniform(0, 360))


AUDIO = "/Game/KillGodot/Audio/"


def ambient(sound, x, y, z, volume=1.0, attenuate=True, folder="Audio"):
    a = actors.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(x, y, z))
    comp = a.get_component_by_class(unreal.AudioComponent)
    comp.set_sound(unreal.load_asset(AUDIO + sound))
    comp.set_editor_property("volume_multiplier", volume)
    if attenuate:
        comp.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO + "SA_KG_Ambience"))
    else:
        comp.set_editor_property("allow_spatialization", False)
    a.set_folder_path(folder)
    stats["sounds"] = stats.get("sounds", 0) + 1


def soundscape():
    """The village should never be silent: sea along the shore, water at the dock, gulls, birds, forge, wind."""
    ambient("A_Wind_Loop", 0, 0, 800, 0.35, attenuate=False)
    for x in range(-7000, 7001, 2300):   # surf along the beach
        y = 5000.0
        while ground(x, y) > 40.0 and y < 9000:
            y += 150.0
        ambient("A_Sea_Loop", x, y, 60, 1.0)
    ambient("A_Lapping_Loop", 0, 6800, 100, 0.9)
    ambient("A_Lapping_Loop", 0, 8800, 100, 0.9)
    ambient("A_Gulls_Loop", 0, 7500, 900, 0.9)
    lh = lm("lighthouse")["at"]
    ambient("A_Gulls_Loop", lh[0] * M, lh[1] * M, 1500, 0.8)
    for bx, by in ((-4200, 2200), (3500, -2500), (-3000, -4200), (5200, 2500), (-6200, -300)):
        ambient("A_Birds_Loop", bx, by, ground(bx, by) + 600, 0.7)
    sm = lm("smithy")["at"]
    ambient("A_Fire_Loop", sm[0] * M, sm[1] * M, ground(sm[0] * M, sm[1] * M) + 120, 0.9)


WP = "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/"


def sea_life():
    """Fish schools (cosmetic, time-driven, scatter from swimmers) and moored rowboats that ride the waves."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGFishSchool")
    for species, x, y, count, radius, dmin, dmax, speed, scale in [
            ("Mackerel", -500.0, 7600.0, 26, 450.0, 60.0, 200.0, 140.0, 1.0),
            ("Mackerel", 700.0, 9300.0, 22, 520.0, 80.0, 260.0, 150.0, 1.0),
            ("Cod", -1600.0, 11500.0, 12, 700.0, 250.0, 500.0, 70.0, 1.0),
            ("Salmon", 5200.0, 7800.0, 14, 600.0, 120.0, 320.0, 170.0, 1.0),
            ("GoldenCarp", 2600.0, 12500.0, 3, 300.0, 150.0, 300.0, 60.0, 1.2)]:
        sch = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, 0.0))
        sch.set_editor_property("fish_mesh", mesh(WP + f"SM_KG_Fish_{species}"))
        for k, v in (("count", count), ("radius", radius), ("min_depth", dmin), ("max_depth", dmax),
                     ("speed", speed), ("fish_scale", scale), ("species", species)):
            sch.set_editor_property(k, v)
        sch.set_folder_path("Sea/Fish")
        stats["fish_schools"] = stats.get("fish_schools", 0) + 1
    # Gull flocks: the same school actor, "depth" negative = circling in the air above the harbour and the island.
    for x, y, count, radius, lo, hi in [(0.0, 7500.0, 7, 1600.0, -1900.0, -1200.0), (4500.0, 15000.0, 5, 1200.0, -2200.0, -1500.0),
                                         (6800.0, 5600.0, 4, 900.0, -3000.0, -2400.0)]:
        flock = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, 0.0))
        flock.set_editor_property("fish_mesh", mesh("/Game/KillGodot/Env/Birds/KG_Gull/StaticMeshes/KG_Gull"))
        for k, v in (("count", count), ("radius", radius), ("min_depth", lo), ("max_depth", hi), ("speed", 480.0),
                     ("fish_scale", 2.2), ("species", "Gull")):
            flock.set_editor_property(k, v)
        flock.set_folder_path("Sky/Gulls")
    for k, (x, y, yaw) in enumerate([(-420.0, 7400.0, 80.0), (430.0, 8200.0, 100.0), (-460.0, 9300.0, 95.0)]):
        boat = breakable("Barrel", x, y, 20.0, yaw, folder="Sea/Boats")
        boat.set_mesh(mesh(WP + "SM_KG_Rowboat"))
        boat.set_editor_property("health", 1.0e6)   # boats are not punchable (rowing comes with the fishing sprint)
        # Ride high: full lift within 8 cm of the keel keeps the waterline under the floorboards.
        boat.get_component_by_class(unreal.load_class(None, "/Script/KillGodot.KGBuoyancyComponent")).set_editor_property("full_depth", 8.0)
        stats["boats"] = stats.get("boats", 0) + 1


PIR = "/Game/KillGodot/Env/PirateC3/KG_PirateProps_Clean2/StaticMeshes/SM_KG_Pirate_"
JP = "/Game/KillGodot/Env/JapanC3/KG_JapanProps_Clean2/StaticMeshes/SM_KG_"
ISLAND = (4500.0, 15000.0)        # shrine island across the bay (UE cm)
GARDEN = (2600.0, 2000.0)         # Japanese garden in the village (layout area "japan_garden")


def pirate_dock(x0, y_start):
    """Harbour pier from the pirate kit: rope-railed deck sections (4.9 m) running out to sea, flag, torches."""
    y0 = y_start
    while ground(x0, y0) > 80.0 and y0 < 9000:
        y0 += 100.0
    y0 -= 300.0
    deck_top = 110.0
    for k in range(9):
        piece = PIR + ["pier_0", "pier_1", "pier_2"][k % 3]
        place_rot(piece, x0, y0 + k * 488.0, deck_top - 140.0, yaw=90.0, folder="Village/Dock")
    end = y0 + 8 * 488.0
    place_rot(PIR + "FlagTall_1", x0 + 110.0, end + 150.0, deck_top - 20.0, folder="Village/Dock")
    place_rot(PIR + "FlagTall_0", x0 + 110.0, end + 150.0, deck_top + 820.0, yaw=0.0, folder="Village/Dock")
    for k in (1, 4, 7):
        place_rot(PIR + "Torch_0", x0 - 115.0, y0 + k * 488.0, deck_top - 25.0, yaw=0.0, folder="Village/Dock")
        warm_light(x0 - 115.0, y0 + k * 488.0, deck_top + 230.0, 12.0, 900.0, folder="Village/Dock")
    for name, ox, oy in [("Barrel_3", -60, 700), ("Barrel_7", 60, 760), ("crates_0", 50, 1900), ("crates_1", -50, 1960),
                         ("Barrel_12", -60, 3100), ("Cannon_Ball_0", 70, 3600)]:
        place_rot(PIR + name, x0 + ox, y0 + oy, deck_top, yaw=rng.uniform(0, 360), folder="Village/Dock")
    return end


def shrine_island():
    """Iconic landmark: a small island across the bay with a pagoda on top and a vermilion torii standing in the sea."""
    ix, iy = ISLAND
    place("/Game/KillGodot/Env/Island/KG_Island/StaticMeshes/KG_Island", ix, iy, 0.0, folder="Island", label="ShrineIsland")
    # Torii in the shallows on the village side, facing the village (Itsukushima).
    place_rot(JP + "Torii", ix - 300.0, iy - 3100.0, -200.0, yaw=0.0, folder="Island")
    warm_light(ix - 300.0, iy - 3100.0, 900.0, 25.0, 2500.0, folder="Island")
    place_rot(JP + "Pagoda", ix, iy, 555.0, yaw=180.0, folder="Island")
    for k, (ox, oy) in enumerate([(-450, -650), (450, -650), (-700, 200), (700, 250)]):
        place_rot(JP + "ToroLantern", ix + ox, iy + oy, 555.0, yaw=0.0, folder="Island")
        warm_light(ix + ox, iy + oy, 555.0 + 150.0, 8.0, 600.0, folder="Island")
    for name, ox, oy, sc in [("Sakura_A", -1100, 600, 1.1), ("Sakura_B", 1150, -400, 1.0), ("BonsaiPine", 900, 900, 1.2),
                             ("Sakura_A", -600, 1300, 0.9), ("Maple", 300, 1400, 1.0)]:
        place_rot(JP + name, ix + ox, iy + oy, 470.0, yaw=rng.uniform(0, 360), scale3=(sc, sc, sc), folder="Island")
    ambient("A_Gulls_Loop", ix, iy, 1500, 0.8)


def japan_garden():
    """A Japanese garden in the village: raised koi pond with an arched bridge, lanterns, lamps, sakura, bamboo."""
    gx, gy = GARDEN
    gz = ground(gx, gy)
    rim = gz + 55.0
    place_rot(JP + "KoiPondRim", gx, gy, rim, folder="Village/Garden")
    water = place_rot("/Engine/BasicShapes/Plane", gx + 40.0, gy + 10.0, rim - 18.0, scale3=(7.6, 4.6, 1.0),
                      folder="Village/Garden", collide=False)
    water.get_component_by_class(unreal.StaticMeshComponent).set_material(0, unreal.load_asset("/Game/KillGodot/Materials/M_KG_PondWater"))
    place_rot(JP + "ArchBridge", gx + 40.0, gy, rim + 5.0, yaw=0.0, folder="Village/Garden")
    koi = actors.spawn_actor_from_class(unreal.load_class(None, "/Script/KillGodot.KGFishSchool"), unreal.Vector(gx + 40.0, gy, rim - 18.0))
    koi.set_editor_property("fish_mesh", mesh(JP + "Koi"))
    for k, v in (("count", 7), ("radius", 160.0), ("min_depth", 12.0), ("max_depth", 25.0), ("speed", 40.0),
                 ("fish_scale", 0.9), ("species", "Koi")):
        koi.set_editor_property(k, v)
    koi.set_folder_path("Village/Garden")
    for ox, oy in [(-620, -380), (700, -400), (-650, 420), (720, 380)]:
        place_rot(JP + "GardenLamp", gx + ox, gy + oy, ground(gx + ox, gy + oy), folder="Village/Garden")
        warm_light(gx + ox, gy + oy, ground(gx + ox, gy + oy) + 70.0, 3.0, 350.0, folder="Village/Garden")
    for ox, oy in [(-800, 0), (850, 60)]:
        place_rot(JP + "ToroLantern", gx + ox, gy + oy, ground(gx + ox, gy + oy), folder="Village/Garden")
        warm_light(gx + ox, gy + oy, ground(gx + ox, gy + oy) + 150.0, 6.0, 500.0, folder="Village/Garden")
    for name, ox, oy, sc in [("Sakura_B", -900, -700, 0.9), ("Sakura_A", 950, -750, 0.85), ("Maple", 1000, 700, 0.9),
                             ("Bamboo", -1000, 700, 1.0), ("Bamboo", -1150, 350, 0.9), ("BonsaiPine", 0, 900, 0.8),
                             ("GardenRock", -350, 520, 1.0), ("GardenRock", 520, 560, 0.8)]:
        if kg_dress_common.building_hit(gx + ox, gy + oy, 150.0):
            continue   # never grow a tree through a house (interiors walk test)
        place_rot(JP + name, gx + ox, gy + oy, ground(gx + ox, gy + oy), yaw=rng.uniform(0, 360),
                  scale3=(sc, sc, sc), folder="Village/Garden")
    for k in range(7):   # stepping-stone path from the lane
        x, y = gx - 200.0 + k * 70.0, gy - 900.0 + k * 75.0
        place_rot(JP + "SteppingStone", x, y, ground(x, y) - 2.0, yaw=rng.uniform(0, 360), folder="Village/Garden",
                  collide=False)
    # A few sakura around the village and yatai food stalls at the markets.
    for x, y in [(-1500, -4200), (3600, -1800), (-4400, 1400), (1700, 4200)]:
        if is_clear(x, y, 150.0) and not kg_dress_common.building_hit(x, y, 200.0):
            place_rot(JP + "Sakura_A", x, y, ground(x, y), yaw=rng.uniform(0, 360), scale3=(0.9, 0.9, 0.9), folder="Village/Sakura")
    for x, y, yaw in [(1400, 4200, 200.0), (-1000, -200, 30.0)]:
        place_rot(JP + "Noren_Stall", x, y, ground(x, y), yaw=yaw, folder="Village/Market")
        warm_light(x, y, ground(x, y) + 240.0, 5.0, 450.0, folder="Village/Market")


def task_spot(t):
    """Chores must be reachable with doors closed (bots cannot open doors yet): anything inside a building moves
    to its front step; tower chores go to the tower door; the graveyard chore sits between the stone rows."""
    x, y = t["at"][0] * M, t["at"][1] * M
    for spec in LAYOUT["houses"] + [v for v in LAYOUT["landmarks"].values() if "size" in v]:
        (bx, by), (w, d) = spec["at"], spec["size"]
        if math.hypot(x - bx * M, y - by * M) < max(w, d) * 50.0:
            fx, fy = spec["face"][0] - bx, spec["face"][1] - by
            n = math.hypot(fx, fy)
            return bx * M + fx / n * (d * 50.0 + 170.0), by * M + fy / n * (d * 50.0 + 170.0)
    for name, yaw in (("bell_tower", 0.0), ("lighthouse", 30.0)):
        tx, ty = lm(name)["at"]
        if math.hypot(x - tx * M, y - ty * M) < 300.0:
            # Tower front is local -Y rotated by yaw.
            return tx * M + math.sin(math.radians(yaw)) * 380.0, ty * M - math.cos(math.radians(yaw)) * 380.0
    if t["id"] == "TendGraves":
        return x + 120.0, y + 125.0
    return x, y


TOWER_TOP_TASKS = {"RingBell": ("bell_tower", 4), "FuelLighthouse": ("lighthouse", 5)}


def task_stations():
    cls = unreal.load_class(None, "/Script/KillGodot.KGTaskStation")
    for t in LAYOUT["tasks"]:
        x, y = task_spot(t)
        z = ground(x, y)
        if t["id"] in TOWER_TOP_TASKS:
            # Up the ladder, on the lookout floor (a long, exposed trip: prime backstab territory).
            name, levels = TOWER_TOP_TASKS[t["id"]]
            tx, ty = lm(name)["at"]
            x, y = tx * M - 80.0, ty * M + 80.0
            z = max(ground(x + dx, y + dy) for dx in (-200, 200) for dy in (-200, 200)) + 2.0 + (levels - 1) * FLOOR_H
        st = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z))
        st.set_editor_property("task_id", t["id"])
        st.set_editor_property("task_name", t["name"])
        st.set_editor_property("work_seconds", float(t["secs"]))
        st.set_actor_label(f"Task_{t['id']}")
        st.set_folder_path("Gameplay/Tasks")
    # Props that make the chore readable where the zone has nothing yet.
    hints = {"PostNotice": None, "FileReports": ("BookStand", 0), "LightCandles": ("CandleStick_Triple", 90),
             "PourAle": ("Barrel_Holder", 0), "StockStall": ("Stall_Empty", 0), "BakeBread": ("Cauldron", 0),
             "UnloadFish": ("Crate_Wooden", 0), "FuelLighthouse": ("Barrel", 0), "FixBoat": ("Barrel", 0),
             "FeedAnimals": ("Stall_Cart_Empty", 0), "TendGraves": ("Bucket_Wooden_1", 0), "RingBell": ("Rope_2", 0)}
    for t in LAYOUT["tasks"]:
        hint = hints.get(t["id"])
        if hint:
            x, y = task_spot(t)
            x, y = x + 110.0, y + 60.0
            place(P + hint[0], x, y, ground(x, y), hint[1], folder="Gameplay/TaskProps")


def navigation():
    vol = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(500.0, 800.0, 400.0))
    vol.set_actor_scale3d(unreal.Vector(80.0, 75.0, 25.0))   # ~160 x 150 x 50 m around the village
    vol.set_folder_path("Gameplay")
    # Spawning the volume does not generate tiles by itself; build now so bots can path (0.5 s).
    unreal.SystemLibrary.execute_console_command(
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")


def gameplay_starts():
    px, py = PLAZA
    for k in range(12):
        a = math.radians(k * 30.0 + 15.0)
        x, y = px + 900.0 * math.cos(a), py + 900.0 * math.sin(a)
        start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, ground(x, y) + 120))
        start.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(a) + 180.0), False)
        start.set_folder_path("Gameplay")


def town():
    for k, h in enumerate(LAYOUT["houses"]):
        house_at(h, k)
    for k, name in enumerate(["town_hall", "inn", "bakery", "boathouse", "mill_barn", "church"]):
        house_at(lm(name), 20 + k)
    bt = lm("bell_tower")["at"]
    tower(bt[0] * M, bt[1] * M, 0.0, 4, 1, light=(10.0, 900.0))
    lh = lm("lighthouse")["at"]
    tower(lh[0] * M, lh[1] * M, 30.0, 5, 2, light=(400.0, 9000.0))   # the beacon
    sm = lm("smithy")["at"]
    open_shed(sm[0] * M, sm[1] * M, 90.0)
    w = lm("well")["at"]
    well(w[0] * M, w[1] * M)
    g = lm("gallows")["at"]
    gallows(g[0] * M, g[1] * M)
    nb = lm("notice_board")["at"]
    notice_board(nb[0] * M, nb[1] * M, 20.0)
    gy = lm("graveyard")["at"]
    graveyard(gy[0] * M, gy[1] * M)
    fa = [a for a in LAYOUT["areas"] if a["name"] == "farm"][0]["center"]
    farm(fa[0] * M, fa[1] * M)
    wc = [a for a in LAYOUT["areas"] if a["name"] == "woodcutter"][0]["center"]
    woodcutter(wc[0] * M, wc[1] * M)
    fm = [a for a in LAYOUT["areas"] if a["name"] == "fish_market"][0]["center"]
    fish_market(fm[0] * M, fm[1] * M)
    pirate_dock(0.0, 5000.0)
    lane_lamps()
    # Square dressing: benches, barrels, banners around the well and gallows.
    for name, ox, oy, yaw in [("Bench", -600, 700, 10), ("Bench", 500, 800, -20), ("Barrel_Apples", 900, -200, 0),
                              ("FarmCrate_Apple", 850, 0, 30), ("Stall_Empty", 1300, 300, -70), ("Stall_Cart_Empty", -1100, 500, 60)]:
        x, y = PLAZA[0] + ox, PLAZA[1] + oy
        place(P + name, x, y, ground(x, y), yaw, folder="Village/Square")


new_level()
sky_and_light()
terrain_and_sea()
look_post_process()
town()
nature_layout()
breakables()
sea_life()
shrine_island()
japan_garden()
soundscape()
grass_fields()
# Zone dressing (Tools/Unreal/dressing/dress_*.py): the bulk of the clutter, props and points of interest.
# Runs after the meadow so modules can clear grass under floors, tents and gardens.
import kg_dress  # noqa: E402
importlib.reload(kg_dress)
_dress = kg_dress.run(save=False, nav=False, restore=False)
stats["dress"] = {z: s.get("props", 0) + s.get("instances", 0) if "error" not in s else "ERROR" for z, s in _dress.items()}
task_stations()
importlib.reload(importlib.import_module("kg_make_minimap")).build_minimap()   # HUD map: texture + AKGMapInfo (layout by level name)
navigation()
gameplay_starts()
level_sub.save_current_level()
unreal.log(f"KG_VILLAGE built {LEVEL}: {stats}")
print(f"KG_VILLAGE {stats}")
