"""Shared helpers for the zone dressing modules (Tools/Unreal/dressing/dress_<zone>.py).

A dressing module fills one zone of L_Morrowmere with content (props, clutter, points of interest, lights,
sounds). It exposes `dress()` and uses only this module to touch the level. The runner (kg_dress.py) clears the
zone's outliner folder "Dress/<Zone>" before calling it, so re-running a zone is idempotent. The full village
build (kg_build_village.py) calls every zone at the end.

UE frame: X east, +Y towards the sea, Z up, centimetres. Layout JSON is in metres (x100).
NOTE: unreal.Rotator positional order is (roll, pitch, yaw) and unreal.Color is BGRA: always use keywords.
"""
import json
import math
import random
import zlib

import unreal

LEVEL = "/Game/KillGodot/Maps/L_Morrowmere"
V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
N = "/Game/KillGodot/Env/KG_Nature/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
PIR = "/Game/KillGodot/Env/PirateC3/KG_PirateProps_Clean2/StaticMeshes/SM_KG_Pirate_"
JP = "/Game/KillGodot/Env/JapanC3/KG_JapanProps_Clean2/StaticMeshes/SM_KG_"
WP = "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/"
DRESS = "/Game/KillGodot/Env/Dress/"          # new dressing packs land under here (one sub folder per pack)
AUDIO = "/Game/KillGodot/Audio/"
HEIGHTS = "D:/Kill Godot/Art/Packed/KG_Terrain_heights.json"
LAYOUT = json.load(open("D:/Kill Godot/Tools/Level/morrowmere_layout.json"))
M = 100.0
PLAZA = (0.0, -800.0)
ISLAND = (4500.0, 15000.0)
GARDEN = (2600.0, 2000.0)
FLOOR_H = 300.0
SEA_Z = 0.0                                    # mean sea level (waves +-~40 cm); the beach is where ground() < ~60

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_hm = json.load(open(HEIGHTS))
_mesh_cache = {}
rng = random.Random(7)

# Set by the runner for the zone being dressed.
ZONE = ""
FOLDER = "Dress"
stats = {}


# ============================================================================================ zones
def zone_of(x, y):
    """Which dressing zone owns the point (cm). First match wins; every agent dresses only its own zone."""
    xm, ym = x / M, y / M
    if math.hypot(xm - 0.0, ym + 10.0) < 22.0:
        return "square"
    if math.hypot(xm - 26.0, ym - 20.0) < 15.0 or math.hypot(xm - ISLAND[0] / M, ym - ISLAND[1] / M) < 40.0:
        return "japan"
    if math.hypot(xm + 28.0, ym + 45.0) < 24.0:
        return "church"
    if ym > 36.0 and -45.0 < xm < 45.0:
        return "harbour"
    if (ym > 20.0 and xm >= 45.0) or (ym > 30.0 and xm <= -45.0):
        return "coast"
    if ym < -25.0 and xm > 8.0 and math.hypot(xm, ym + 8.0) < 110.0:
        return "countryside"
    if -52.0 < xm < 62.0 and -40.0 < ym < 40.0:
        return "streets"
    return "wilds"


def in_zone(x, y, zone=None):
    return zone_of(x, y) == (zone or ZONE)


# ============================================================================================ terrain + layout
def ground(x, y):
    """Terrain height (cm) at UE (x, y), bilinear from the Blender heightmap (same as the builder)."""
    size, step, n, h = _hm["size"], _hm["step"], _hm["n"], _hm["heights"]
    bx, by = x / 100.0 + size / 2, -y / 100.0 + size / 2
    fi, fj = max(0.0, min(n - 1e-3, bx / step)), max(0.0, min(n - 1e-3, by / step))
    i, j = int(fi), int(fj)
    tx, ty = fi - i, fj - j
    a, b = h[j * (n + 1) + i], h[j * (n + 1) + i + 1]
    c, d = h[(j + 1) * (n + 1) + i], h[(j + 1) * (n + 1) + i + 1]
    return 100.0 * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)


def slope(x, y, r=100.0):
    """Max height difference (cm) across a 2r square: use to skip steep spots for flat-bottomed props."""
    hs = [ground(x + dx, y + dy) for dx in (-r, r) for dy in (-r, r)]
    return max(hs) - min(hs)


def ground_min(x, y, r):
    """Lowest terrain under a footprint of radius r: sink a prop to it so no edge floats."""
    return min(ground(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r))


def lm(name):
    return LAYOUT["landmarks"][name]


def area(name):
    a = [a for a in LAYOUT["areas"] if a["name"] == name][0]
    return a["center"][0] * M, a["center"][1] * M, a["radius"] * M


def facing_yaw(at, face):
    return math.degrees(math.atan2(face[1] - at[1], face[0] - at[0])) + 90.0


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def lanes():
    """[(name, width_cm, [(x, y) cm ...])]"""
    return [(l["name"], l["width"] * M, [(p[0] * M, p[1] * M) for p in l["points"]]) for l in LAYOUT["lanes"]]


def lane_distance(x, y):
    """Distance (cm) from the nearest lane EDGE; negative = on the lane."""
    best = 1e9
    for _, w, pts in lanes():
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            best = min(best, seg_dist(x, y, ax, ay, bx, by) - w * 0.5)
    return best


def lane_samples(name, step=300.0):
    """Points along a lane centre line: [(x, y, dir_x, dir_y)]; the lane edge is +- width/2 along (-dir_y, dir_x)."""
    for n, w, pts in lanes():
        if n != name:
            continue
        out, carry = [], 0.0
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            seg = math.hypot(bx - ax, by - ay)
            dx, dy = (bx - ax) / seg, (by - ay) / seg
            t = carry
            while t < seg:
                out.append((ax + dx * t, ay + dy * t, dx, dy))
                t += step
            carry = t - seg
        return out
    return []


class Frame:
    """Oriented local frame (building footprint): local -Y is the FRONT (door side), +X to the right."""

    def __init__(self, x, y, yaw, w=0.0, d=0.0, name=""):
        self.x, self.y, self.yaw, self.w, self.d, self.name = x, y, yaw, w, d, name
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def world(self, lx, ly):
        return self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c

    def local(self, wx, wy):
        dx, dy = wx - self.x, wy - self.y
        return dx * self.c + dy * self.s, -dx * self.s + dy * self.c

    def inside(self, wx, wy, pad=0.0):
        lx, ly = self.local(wx, wy)
        return abs(lx) <= self.w * 0.5 + pad and abs(ly) <= self.d * 0.5 + pad


def buildings():
    """Every built footprint (cm): houses, landmark houses, towers, smithy shed. Frame.w/d are full sizes (cm);
    .door is the world position just outside the front door (None for open structures)."""
    out = []
    specs = [(f"house{k}", h) for k, h in enumerate(LAYOUT["houses"])]
    specs += [(n, lm(n)) for n in ("town_hall", "inn", "bakery", "boathouse", "mill_barn", "church")]
    for name, h in specs:
        (ax, ay), (w, d) = h["at"], h["size"]
        f = Frame(ax * M, ay * M, facing_yaw(h["at"], h["face"]), w * M, d * M, name)
        cw = w // 2
        door_x = -cw * 100.0 + 100.0 + (cw // 2) * 200.0
        f.door = f.world(door_x, -d * 50.0 - 150.0)
        out.append(f)
    for name, yaw in (("bell_tower", 0.0), ("lighthouse", 30.0)):
        ax, ay = lm(name)["at"]
        f = Frame(ax * M, ay * M, yaw, 440.0, 440.0, name)
        f.door = f.world(-100.0, -370.0)
        out.append(f)
    sx, sy = lm("smithy")["at"]
    f = Frame(sx * M, sy * M, 90.0, 400.0, 600.0, "smithy")
    f.door = None
    out.append(f)
    return out


_BUILDINGS = None


def building_hit(x, y, pad=60.0):
    """True when (x, y) is inside a building footprint (+pad)."""
    global _BUILDINGS
    _BUILDINGS = _BUILDINGS or buildings()
    return any(f.inside(x, y, pad) for f in _BUILDINGS)


def task_spots():
    """World (x, y) of every chore station (same relocation rules as the builder's task_spot)."""
    out = {}
    for t in LAYOUT["tasks"]:
        x, y = t["at"][0] * M, t["at"][1] * M
        moved = False
        for spec in LAYOUT["houses"] + [v for v in LAYOUT["landmarks"].values() if "size" in v]:
            (bx, by), (w, d) = spec["at"], spec["size"]
            if math.hypot(x - bx * M, y - by * M) < max(w, d) * 50.0:
                fx, fy = spec["face"][0] - bx, spec["face"][1] - by
                n = math.hypot(fx, fy)
                x, y, moved = bx * M + fx / n * (d * 50.0 + 170.0), by * M + fy / n * (d * 50.0 + 170.0), True
                break
        if not moved:
            for name, yaw in (("bell_tower", 0.0), ("lighthouse", 30.0)):
                tx, ty = lm(name)["at"]
                if math.hypot(x - tx * M, y - ty * M) < 300.0:
                    x, y = tx * M + math.sin(math.radians(yaw)) * 380.0, ty * M - math.cos(math.radians(yaw)) * 380.0
                    break
            if t["id"] == "TendGraves":
                x, y = x + 120.0, y + 125.0
        out[t["id"]] = (x, y)
    return out


def reserved():
    """Circles (x, y, r) that must stay walkable: doors, chores, player starts, the dock deck, the gallows steps."""
    out = []
    for f in buildings():
        if f.door:
            out.append((f.door[0], f.door[1], 230.0))
    for x, y in task_spots().values():
        out.append((x, y, 230.0))
    px, py = PLAZA
    for k in range(12):
        a = math.radians(k * 30.0 + 15.0)
        out.append((px + 900.0 * math.cos(a), py + 900.0 * math.sin(a), 160.0))
    gx, gy = lm("gallows")["at"]
    out.append((gx * M, gy * M + 300.0, 200.0))
    wx, wy = lm("well")["at"]
    out.append((wx * M, wy * M, 220.0))
    return out


_RESERVED = None


# ============================================================================================ occupancy
_CELL = 400.0
_grid = {}


def _cells(x, y, r):
    for i in range(int(math.floor((x - r) / _CELL)), int(math.floor((x + r) / _CELL)) + 1):
        for j in range(int(math.floor((y - r) / _CELL)), int(math.floor((y + r) / _CELL)) + 1):
            yield i, j


def claim(x, y, r):
    """Mark a circle as taken by something placed this run (spatial hash)."""
    for key in _cells(x, y, r):
        _grid.setdefault(key, []).append((x, y, r))


def overlaps(x, y, r):
    for key in _cells(x, y, r):
        for (cx, cy, cr) in _grid.get(key, ()):
            if math.hypot(x - cx, y - cy) < r + cr:
                return True
    return False


def free(x, y, r, lane_gap=40.0, zone=True, allow_lane=False, allow_reserved=False):
    """A spot is free when it is in this zone, off lanes (unless allow_lane), outside buildings, away from
    doors/chores/spawns, and not overlapping anything claimed this run."""
    global _RESERVED
    if zone and not in_zone(x, y):
        return False
    if not allow_lane and lane_distance(x, y) < r + lane_gap:
        return False
    if building_hit(x, y, r + 40.0):
        return False
    if not allow_reserved:
        _RESERVED = _RESERVED if _RESERVED is not None else reserved()
        for (cx, cy, cr) in _RESERVED:
            if math.hypot(x - cx, y - cy) < r + cr:
                return False
    return not overlaps(x, y, r)


def is_clear(x, y, margin=300.0):
    """The builder's 'open country' test: away from lanes, yards, houses and landmarks."""
    for _, w, pts in lanes():
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            if seg_dist(x, y, ax, ay, bx, by) < w * 0.5 + margin:
                return False
    for a in LAYOUT["areas"]:
        if math.hypot(x - a["center"][0] * M, y - a["center"][1] * M) < a["radius"] * M + margin:
            return False
    for h in LAYOUT["houses"] + [v for v in LAYOUT["landmarks"].values()]:
        r = (max(h["size"]) * 50.0 if "size" in h else 450.0) + margin + 150.0
        if math.hypot(x - h["at"][0] * M, y - h["at"][1] * M) < r:
            return False
    return True


# ============================================================================================ spawning
def mesh(path):
    if path not in _mesh_cache:
        _mesh_cache[path] = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not _mesh_cache[path]:
            unreal.log_warning(f"KG_DRESS missing mesh {path}")
            stats.setdefault("missing", set()).add(path)
    return _mesh_cache[path]


def _folder(sub):
    return f"{FOLDER}/{sub}" if sub else FOLDER


def place(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, sub="", collide=True, cull=0.0,
          shadow=True, claim_r=0.0, sink=0.0, label=None):
    """Spawn a static mesh. z=None -> on the terrain (minus `sink`). scale: float or (sx, sy, sz).
    cull (cm) > 0 hides it beyond that distance (use 4000-8000 for small clutter). claim_r > 0 reserves the
    footprint so later free() calls avoid it. Returns the actor (or None when the mesh is missing)."""
    m = mesh(path)
    if not m:
        return None
    zz = (ground(x, y) - sink) if z is None else z
    a = actors.spawn_actor_from_object(m, unreal.Vector(x, y, zz), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    s3 = (scale, scale, scale) if isinstance(scale, (int, float)) else scale
    if s3 != (1.0, 1.0, 1.0):
        a.set_actor_scale3d(unreal.Vector(*s3))
    a.set_folder_path(_folder(sub))
    if label:
        a.set_actor_label(label)
    comp = a.get_component_by_class(unreal.StaticMeshComponent)
    if not collide:
        a.set_actor_enable_collision(False)
        comp.set_editor_property("can_ever_affect_navigation", False)
    if cull > 0.0:
        comp.set_editor_property("ld_max_draw_distance", cull)
    if not shadow:
        comp.set_editor_property("cast_shadow", False)
    if claim_r > 0.0:
        claim(x, y, claim_r)
    stats["props"] = stats.get("props", 0) + 1
    return a


_field_cls = None
_fields = {}


def instanced(path, transforms, collide=False, cull=12000.0, sub="Instanced"):
    """Many copies of one mesh: [(x, y, z, yaw, scale)] or [(x, y, z, yaw, scale, pitch, roll)] (scale float or
    3-tuple). Uses one AKGFoliageField (HISM per mesh) per zone when the class is compiled, plain actors
    otherwise. Use it for anything repeated ~5+ times (fence runs, crop rows, stones, clutter). Returns the count."""
    transforms = [tuple(t) + (0.0, 0.0) if len(t) == 5 else tuple(t) for t in transforms]
    global _field_cls
    m = mesh(path)
    if not m or not transforms:
        return 0
    _field_cls = _field_cls or unreal.load_class(None, "/Script/KillGodot.KGFoliageField")
    if _field_cls:
        field = _fields.get(ZONE)
        if field is None:
            field = actors.spawn_actor_from_class(_field_cls, unreal.Vector(0.0, 0.0, 0.0))
            field.set_actor_label(f"KG_Dress_{ZONE}_Instances")
            field.set_folder_path(_folder(sub))
            _fields[ZONE] = field
        tr = []
        for (x, y, z, yaw, sc, pitch, roll) in transforms:
            s3 = (sc, sc, sc) if isinstance(sc, (int, float)) else sc
            tr.append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                                       unreal.Vector(*s3)))
        field.add_instances(m, tr, collide, cull)
        stats["instances"] = stats.get("instances", 0) + len(tr)
        return len(tr)
    for (x, y, z, yaw, sc, pitch, roll) in transforms:
        place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=sc, sub=sub, collide=collide, cull=cull,
              shadow=collide)
    return len(transforms)


def light(x, y, z, intensity=8.0, radius=700.0, color=(255, 170, 95), sub="Lights"):
    """Movable, shadowless point light (candelas). Keep them sparse: every light costs."""
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc = a.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    lc.set_editor_property("cast_shadows", False)
    a.set_folder_path(_folder(sub))
    stats["lights"] = stats.get("lights", 0) + 1
    return a


def ambient(sound, x, y, z, volume=1.0, sub="Audio"):
    """Looping positional ambience (existing assets under /Game/KillGodot/Audio: A_*_Loop)."""
    a = actors.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(x, y, z))
    comp = a.get_component_by_class(unreal.AudioComponent)
    comp.set_sound(unreal.load_asset(AUDIO + sound))
    comp.set_editor_property("volume_multiplier", volume)
    comp.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO + "SA_KG_Ambience"))
    a.set_folder_path(_folder(sub))
    stats["sounds"] = stats.get("sounds", 0) + 1
    return a


def spawn_class(class_path, x, y, z, yaw=0.0, sub="Gameplay"):
    """Gameplay actors, e.g. '/Script/KillGodot.KGBreakable' (then .set_mesh(mesh(...))), KGLadder, KGTaskStation."""
    cls = unreal.load_class(None, class_path)
    if not cls:
        unreal.log_warning(f"KG_DRESS missing class {class_path}")
        return None
    a = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.set_folder_path(_folder(sub))
    stats["gameplay"] = stats.get("gameplay", 0) + 1
    return a


def breakable(path, x, y, z=None, yaw=0.0, sub="Breakables"):
    """Punchable physics crate/barrel (AKGBreakable). Use for crates, barrels, pots people will want to smash."""
    b = spawn_class("/Script/KillGodot.KGBreakable", x, y, (ground(x, y) + 5.0) if z is None else z, yaw, sub)
    if b:
        b.set_mesh(mesh(path))
        low = path.lower()
        table = "Barrel" if "barrel" in low else "Pot" if ("pot" in low or "vase" in low) else "Crate"
        try:
            b.set_editor_property("loot_table", table)   # drops FKGLoot items when smashed (new C++ property)
        except Exception:
            pass
    return b


def seat(path, x, y, z=None, yaw=0.0, seat_height=None, sub="Seats"):
    """A bench/stool/chair people can sit on (AKGSeat: E to sit). The sitter faces local +X after `yaw`.
    Falls back to a plain mesh while the class is not compiled into the running editor."""
    zz = ground(x, y) if z is None else z
    s = spawn_class("/Script/KillGodot.KGSeat", x, y, zz, yaw, sub) if unreal.load_class(None, "/Script/KillGodot.KGSeat") else None
    if not s:
        return place(path, x, y, zz, yaw=yaw, sub=sub)
    s.set_seat_mesh(mesh(path))
    if seat_height is not None:
        s.set_editor_property("seat_height", seat_height)
    return s


def loot_chest(x, y, z=None, yaw=0.0, table="Chest", name="Chest", path=None, house_index=-1, sub="Loot"):
    """Lootable storage chest (AKGStorageChest, E to open; loot rolled once per match from an FKGLoot table:
    Chest, Crate, Barrel, Pot, Grave, Fishing). Plain chest mesh while the class is not compiled."""
    zz = ground(x, y) if z is None else z
    chest_mesh = path or "/Game/KillGodot/Items/Storage/SM_KG_Chest_Wood"
    c = spawn_class("/Script/KillGodot.KGStorageChest", x, y, zz, yaw, sub) if unreal.load_class(None, "/Script/KillGodot.KGStorageChest") else None
    if not c:
        return place(chest_mesh, x, y, zz, yaw=yaw, scale=0.75, sub=sub)
    if path:
        c.set_chest_mesh(mesh(path))
    for k, v in (("starting_loot_table", table), ("loot_seed", zlib.crc32(f"{x:.0f},{y:.0f}".encode()) & 0x7FFFFFFF),
                 ("display_name", name)) + ((("house_index", house_index),) if house_index >= 0 else ()):
        try:
            c.set_editor_property(k, v)
        except Exception as e:   # never lose a whole zone to one property
            unreal.log_warning(f"KG_DRESS chest {k}: {e}")
    stats["chests"] = stats.get("chests", 0) + 1
    return c


def mover(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, spin=(0.0, 0.0, 0.0), sway=0.0, sway_hz=0.35,
          bob=0.0, sub="Motion"):
    """Cosmetic motion (AKGSpinner): spin=(pitch, yaw, roll) deg/s (windmill sails: roll), sway = pendulum degrees
    around local X (hanging signs, lanterns), bob = cm (buoys). Static mesh while the class is not compiled."""
    zz = ground(x, y) if z is None else z
    cls = unreal.load_class(None, "/Script/KillGodot.KGSpinner")
    if not cls:
        return place(path, x, y, zz, yaw=yaw, pitch=pitch, roll=roll, scale=scale, sub=sub)
    a = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, zz), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    a.set_mesh(mesh(path))
    s3 = (scale, scale, scale) if isinstance(scale, (int, float)) else scale
    a.set_actor_scale3d(unreal.Vector(*s3))
    a.set_editor_property("spin_rate", unreal.Rotator(roll=spin[2], pitch=spin[0], yaw=spin[1]))
    a.set_editor_property("sway_degrees", sway)
    a.set_editor_property("sway_hz", sway_hz)
    a.set_editor_property("bob_cm", bob)
    a.set_folder_path(_folder(sub))
    stats["movers"] = stats.get("movers", 0) + 1
    return a


def scatter(paths, count, pick, r_prop, tries_per=30, sub="Scatter", scale=(0.9, 1.2), collide=False, cull=6000.0,
            sink=2.0, max_slope=9999.0, instanced_ok=True):
    """Random scatter inside this zone. pick() -> (x, y) candidate; accepted when free(x, y, r_prop).
    Non-colliding scatter goes through instanced() (cheap)."""
    batch, placed = {}, 0
    for _ in range(count * tries_per):
        if placed >= count:
            break
        x, y = pick()
        if not free(x, y, r_prop) or slope(x, y, r_prop) > max_slope:
            continue
        p = rng.choice(paths)
        sc = rng.uniform(*scale)
        claim(x, y, r_prop)
        if instanced_ok and not collide:
            batch.setdefault(p, []).append((x, y, ground(x, y) - sink, rng.uniform(0, 360), sc))
        else:
            place(p, x, y, yaw=rng.uniform(0, 360), scale=sc, sub=sub, collide=collide, cull=cull, sink=sink)
        placed += 1
    for p, tr in batch.items():
        instanced(p, tr, collide=False, cull=cull, sub=sub)
    return placed


# ============================================================================================ level housekeeping
def clear_folder(prefix):
    """Destroy every actor whose outliner folder is `prefix` or below it. Returns the count."""
    doomed = []
    for a in actors.get_all_level_actors():
        f = str(a.get_folder_path())
        if f == prefix or f.startswith(prefix + "/"):
            doomed.append(a)
    if doomed:
        actors.destroy_actors(doomed)
    return len(doomed)


def clear_grass(x, y, r):
    """Remove meadow clumps (KG_Meadow HISM instances) inside a circle: under tents, floors, rock gardens...
    Persistent (the instances are saved with the level). The full rebuild regrows grass then clears again."""
    removed = 0
    for a in actors.get_all_level_actors():
        if a.get_actor_label() != "KG_Meadow":
            continue
        for comp in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            hit = comp.get_instances_overlapping_sphere(unreal.Vector(x, y, ground(x, y)), r, True)
            if hit:
                comp.remove_instances(list(hit))
                removed += len(hit)
    stats["grass_cleared"] = stats.get("grass_cleared", 0) + removed
    return removed


def begin(zone):
    """Runner hook: start dressing `zone` (clears its folder)."""
    global ZONE, FOLDER, stats, _RESERVED, _BUILDINGS
    ZONE, FOLDER = zone, f"Dress/{zone.capitalize()}"
    stats = {}
    _fields.pop(zone, None)
    rng.seed(zlib.crc32(zone.encode()))
    removed = clear_folder(FOLDER)
    _RESERVED = None
    _BUILDINGS = None
    return removed
