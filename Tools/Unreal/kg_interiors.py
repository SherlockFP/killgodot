"""House interiors for Tools/Unreal/kg_build_village.py::build_house: a real staircase to the upper floor and furnished,
lived-in rooms (kitchen/dining + living corner downstairs, bedroom upstairs; the landmarks get their own trade).

  furnish(f, w, d, style, warm_light) -> number of actors spawned

f is build_house's Frame (house-local: origin = footprint centre on the ground, local -Y = front/door side, cm),
w/d the footprint in metres, style the builder's style index (20..25 = landmarks, see LANDMARKS), warm_light the
builder's shadowless movable point light. Deterministic: every house seeds its own RNG from its position, so the
builder's shared RNG sequence (and with it the rest of the village) is untouched. Editor Python only.

Stair: the kit's Stair_Interior_Solid (rises 3.00 m over 3.85 m, measured from Art/Packed/KG_Village.glb) sits in the
builder's 2 x 4 m stair well at the back-left (upper tiles i == 0, j >= cd - 2 are missing). It climbs TOWARDS THE BACK
so it is entered from the front of the house, and ends on a 1 m landing tile against the back wall; you step off it
sideways onto the upper floor. A handrail follows the open side; a balustrade guards the rest of the well.
Clearances: walls reach 9 cm inside the footprint (WALL), brick corners 38 cm; the doorway, the door swing, the walk
from the door to the stair foot and the landing exit are reserved and stay empty. At most two warm lights per house
(hearth + bedside candle), shadowless.
Gameplay: every chair, stool and bench is an AKGSeat (E to sit); each home's chest at the bed foot is an
AKGStorageChest owned through HouseIndex = the house's index in morrowmere_layout.json (= build_house's style) with a
roll of the 'Chest' loot table. Both fall back to plain meshes if the classes are missing from the running editor.
Assets: Tools/Unreal/kg_import_furniture.py (JayBee kitchen/bedroom + our Art/Packed/KG_Interior.glb).
Iterate without a full village rebuild: Tools/Unreal/kg_refurnish.py (clears Village/Interior, re-furnishes every
house of the live level, checks stairs + walkways with a player capsule, saves).
"""
import math
import random

import unreal

V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
K = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_"
B = "/Game/KillGodot/Env/Furniture/KG_Bedroom/StaticMeshes/Bedroom_"
I = "/Game/KillGodot/Env/Furniture/KG_Interior/StaticMeshes/SM_KG_"
WP = "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_"
FOLDER = "Village/Interior"
FLOOR_H = 300.0
WALL = 12.0            # wall trims reach 9 cm inside the footprint edge
LANDING = 100.0        # landing tile at the top of the stair, against the back wall (91 cm usable)
STAIR_RUN = 385.0      # Stair_Interior_Solid: first riser (Y=0) -> landing edge (Y=3.85 m); rise 300
STAIR_TOE = 35.0       # stringers start 35 cm in front of the first riser
LANDMARKS = {20: "town_hall", 21: "inn", 22: "bakery", 23: "boathouse", 24: "mill_barn", 25: "church"}

# Measured surface heights (cm, from the meshes) so nothing floats or sinks.
TOP = {K + "Cupboard": 89.7, K + "DoubleCupboard": 89.7, K + "LongTable": 66.8, K + "Square_Table": 66.8,
       B + "BedsideTable": 46.3, B + "BedsideCabinet": 45.5, B + "SmallTable2": 45.0, B + "ChestSquareIron": 41.5,
       B + "ChestRoundIron": 43.2, B + "Wardrobe": 203.8, P + "Table_Large": 81.3, P + "Cabinet": 99.3,
       P + "Nightstand_Shelf": 120.9, P + "Barrel": 85.8, P + "Crate_Wooden": 82.1, P + "Workbench": 88.4,
       I + "Hearth": 129.0, P + "Bench": 49.6}
SHELVES = {K + "Shelf2": [23.8, 65.8, 107.8, 149.8], K + "Shelf4": [13.3, 62.2, 105.5, 148.3],
           P + "Bookcase_2": [14.1, 76.8, 115.1, 153.5, 191.9], P + "Nightstand_Shelf": [45.2, 70.4, 93.2]}
JARS = [K + "Jar", K + "YellowJar", K + "Bottle", K + "YellowBottle", K + "SmallPot", K + "Bowl", P + "Pot_1_Lid",
        P + "Bottle_1", P + "Vase_4", K + "SmallPotLid", P + "Potion_2", P + "Mug"]
BOOKS = [P + "BookGroup_Medium_1", P + "BookGroup_Medium_2", P + "BookGroup_Medium_3"]
SMALL_BOOKS = [P + "BookGroup_Small_1", P + "BookGroup_Small_2", P + "BookGroup_Small_3", P + "Book_Stack_1",
               P + "Book_Stack_2"]

_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_mesh, _bounds = {}, {}


def mesh(path):
    if path not in _mesh:
        _mesh[path] = unreal.load_asset(path)
        if not _mesh[path]:
            unreal.log_warning(f"KG_INTERIORS missing mesh {path}")
    return _mesh[path]


def bounds(path):
    """Local mesh bounds (cm): (x0, y0, z0, x1, y1, z1)."""
    if path not in _bounds:
        m = mesh(path)
        bb = m.get_bounding_box() if m else None
        _bounds[path] = (bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z) if bb else (-10, -10, 0, 10, 10, 10)
    return _bounds[path]


def rot(u, v, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return u * c - v * s, u * s + v * c


def footprint(path, yaw, scale=(1.0, 1.0, 1.0)):
    """Axis-aligned rect (x0, y0, x1, y1) of the mesh around its pivot after scale + yaw."""
    x0, y0, _, x1, y1, _ = bounds(path)
    pts = [rot(x * scale[0], y * scale[1], yaw) for x in (x0, x1) for y in (y0, y1)]
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts))


def overlap(a, b, pad=0.0):
    return a[0] < b[2] + pad and b[0] < a[2] + pad and a[1] < b[3] + pad and b[1] < a[3] + pad


# ======================================================================================================== house
class House:
    def __init__(self, f, w, d, style, warm_light):
        self.f, self.w, self.d, self.style = f, w, d, style
        self.ex, self.ey, self.cw, self.cd = w * 50.0, d * 50.0, w // 2, d // 2
        self.door_x = -self.cw * 100.0 + 100.0 + (self.cw // 2) * 200.0      # same rule as build_house
        self.R = random.Random(int(f.x * 7.0) * 1000003 + int(f.y * 13.0) + style * 7919)
        self.kind = LANDMARKS.get(style, "home")
        self.warm_light = warm_light
        self.count = 0
        self.lights = 0
        self.chests = 0
        self.stair_x, self.stair_y = -self.ex + 100.0, self.ey - LANDING - STAIR_RUN   # stair pivot (yaw 180)
        self.foot_y = self.stair_y - STAIR_TOE
        # where a player stands to step onto the stair (capsule r 34 clear of the front wall in 6 m deep houses)
        self.approach_y = max(self.foot_y - 45.0, -self.ey + WALL + 38.0)

    def world(self, lx, ly):
        f = self.f
        return f.x + lx * f.c - ly * f.s, f.y + lx * f.s + ly * f.c

    def put(self, path, lx, ly, lz, lyaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0), collide=True, sub=""):
        m = mesh(path)
        if not m:
            return None
        wx, wy = self.world(lx, ly)
        a = _actors.spawn_actor_from_object(m, unreal.Vector(wx, wy, self.f.z + lz),
                                            unreal.Rotator(roll=roll, pitch=pitch, yaw=self.f.yaw + lyaw))
        if tuple(scale) != (1.0, 1.0, 1.0):
            a.set_actor_scale3d(unreal.Vector(*scale))
        a.set_folder_path(FOLDER + (("/" + sub) if sub else ""))
        if not collide:
            a.set_actor_enable_collision(False)
        self.count += 1
        return a

    def spawn_class(self, cls, lx, ly, lz, lyaw=0.0, sub=""):
        """Gameplay actor (AKGSeat, AKGStorageChest) at a house-local transform."""
        wx, wy = self.world(lx, ly)
        a = _actors.spawn_actor_from_class(cls, unreal.Vector(wx, wy, self.f.z + lz),
                                           unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.f.yaw + lyaw))
        a.set_folder_path(FOLDER + (("/" + sub) if sub else ""))
        self.count += 1
        return a

    def light(self, lx, ly, lz, intensity, radius):
        if self.lights >= 2:
            return
        wx, wy = self.world(lx, ly)
        self.warm_light(wx, wy, self.f.z + lz, intensity, radius, folder=FOLDER + "/Lights")
        self.lights += 1
        self.count += 1

    # -------------------------------------------------------------------------------------------- wall segments
    def segments(self, level, wall):
        """[(t0, t1, kind)] 2 m wall pieces along a wall, mirroring build_house: kind in solid|wide|thin|door|grid."""
        n = self.cw if wall in ("front", "back") else self.cd
        out = []
        for k in range(n):
            off = -n * 100.0 + 100.0 + k * 200.0
            t = off if wall in ("front", "right") else -off
            if level == 0:
                if wall == "front":
                    kind = "door" if k == n // 2 else ("wide" if k % 2 else "solid")
                else:
                    kind = "thin" if k % 2 == 1 else "solid"
            else:
                kind = "wide" if k % 2 == 0 else "grid"
            out.append((t - 100.0, t + 100.0, kind))
        return out

    def holes(self, level, wall):
        """Openings (t0, t1, kind) measured on the kit walls: door 1.3 m, wide window 1.2 m, thin 0.8 m; sills 1.1 m."""
        half = {"door": 68.0, "wide": 62.0, "thin": 40.0}
        return [((a + b) / 2 - half[k], (a + b) / 2 + half[k], k) for a, b, k in self.segments(level, wall) if k in half]

    def wall_ok(self, level, wall, t0, t1, height):
        """Nothing across a doorway; tall things (> 100 cm reaches the sills) not in front of a window."""
        for a, b, kind in self.holes(level, wall):
            if a < t1 - 1 and t0 + 1 < b and (kind == "door" or height > 100.0):
                return False
        return True


class Room:
    """One floor: free-space bookkeeping in house-local cm. Rect tags: 'block' (furniture), 'keep' (walkways: only
    rugs may overlap), 'stair'."""

    def __init__(self, h, level):
        self.h, self.level = h, level
        self.z = 2.0 if level == 0 else FLOOR_H + 1.0          # tops of the kit floor tiles
        self.x0, self.x1 = -h.ex + WALL, h.ex - WALL
        self.y0, self.y1 = -h.ey + WALL, h.ey - WALL
        self.rects = []
        # exterior corner posts poke inside (build_house places them unrotated at the four corners)
        post = V + ("Corner_Exterior_Brick" if (level == 0 and h.style % 2 == 0) else "Corner_Exterior_Wood")
        b = bounds(post)
        for sx in (-1, 1):
            for sy in (-1, 1):
                x, y = sx * h.ex, sy * h.ey
                self.take((x + b[0] - 3.0, y + b[1] - 3.0, x + b[3] + 3.0, y + b[4] + 3.0), "block")

    def take(self, r, tag="block"):
        self.rects.append((r, tag))

    def inside(self, r):
        return r[0] >= self.x0 - 0.5 and r[2] <= self.x1 + 0.5 and r[1] >= self.y0 - 0.5 and r[3] <= self.y1 + 0.5

    def free(self, r, allow=(), pad=0.0):
        return self.inside(r) and not any(overlap(r, q, pad) for q, tag in self.rects if tag not in allow)

    def keep_path(self, p0, p1, half):
        """Reserve a walkway (squares every 25 cm along the segment)."""
        n = max(1, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / 25.0))
        for k in range(n + 1):
            x = p0[0] + (p1[0] - p0[0]) * k / n
            y = p0[1] + (p1[1] - p0[1]) * k / n
            self.take((x - half, y - half, x + half, y + half), "keep")

    # ------------------------------------------------------------------------------------------------ placing
    def spot(self, path, lx, ly, lyaw, scale=(1.0, 1.0, 1.0), allow=(), pad=0.0):
        fp = footprint(path, lyaw, scale)
        r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
        return r if self.free(r, allow, pad) else None

    def place(self, path, lx, ly, lyaw, scale=(1.0, 1.0, 1.0), tag="block", allow=(), collide=True, check=True,
              lz=0.0, pad=0.0):
        fp = footprint(path, lyaw, scale)
        r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
        if check and not self.free(r, allow, pad):
            return None
        lz += max(0.0, -bounds(path)[2]) * scale[2]           # feet modelled below the pivot must not sink
        self.h.put(path, lx, ly, self.z + lz, lyaw, scale=scale, collide=collide)
        if tag:
            self.take(r, tag)
        return Item(path, lx, ly, self.z + lz, lyaw, r, scale)

    def wall_candidates(self, path, walls, scale=(1.0, 1.0, 1.0), gap=2.0, step=5.0, prefer=None, extra_yaw=0.0):
        """(lx, ly, lyaw, wall, t) with the mesh's back (-Y) flush against each wall, all along it."""
        out = []
        for wall in walls:
            yaw = {"back": 180.0, "front": 0.0, "left": -90.0, "right": 90.0}[wall] + extra_yaw
            fp = footprint(path, yaw, scale)
            if wall in ("back", "front"):
                mid = (fp[0] + fp[2]) / 2
                lo, hi = self.x0 - fp[0] + mid, self.x1 - fp[2] + mid          # allowed centres
                ly = self.y1 - gap - fp[3] if wall == "back" else self.y0 + gap - fp[1]
                c = math.ceil(lo / step) * step
                while c <= hi + 0.01:
                    out.append((c - mid, ly, yaw, wall, c))
                    c += step
            else:
                mid = (fp[1] + fp[3]) / 2
                lo, hi = self.y0 - fp[1] + mid, self.y1 - fp[3] + mid
                lx = self.x1 - gap - fp[2] if wall == "right" else self.x0 + gap - fp[0]
                c = math.ceil(lo / step) * step
                while c <= hi + 0.01:
                    out.append((lx, c - mid, yaw, wall, c))
                    c += step
        if prefer:
            out.sort(key=prefer)
        else:
            self.h.R.shuffle(out)
        return out

    def on_wall(self, path, walls, scale=(1.0, 1.0, 1.0), gap=2.0, prefer=None, tall=None, tag="block",
                collide=True, allow=(), extra_yaw=0.0):
        height = tall if tall is not None else bounds(path)[5] * scale[2]
        for lx, ly, yaw, wall, t in self.wall_candidates(path, walls, scale, gap, prefer=prefer, extra_yaw=extra_yaw):
            fp = footprint(path, yaw, scale)
            r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
            if not self.free(r, allow):
                continue
            t0, t1 = (r[0], r[2]) if wall in ("back", "front") else (r[1], r[3])
            if not self.h.wall_ok(self.level, wall, t0, t1, height):
                continue
            it = self.place(path, lx, ly, yaw, scale, tag=tag, collide=collide, check=False)
            it.wall, it.t = wall, t
            return it
        return None

    def wall_mount(self, path, wall, t, z, gap=0.5, lyaw_extra=0.0, collide=False):
        """Hang something on a wall (no floor footprint): back of the mesh flush with the wall at height z."""
        yaw = {"back": 180.0, "front": 0.0, "left": -90.0, "right": 90.0}[wall] + lyaw_extra
        fp = footprint(path, yaw)
        if wall == "back":
            lx, ly = t - (fp[0] + fp[2]) / 2, self.y1 - gap - fp[3]
        elif wall == "front":
            lx, ly = t - (fp[0] + fp[2]) / 2, self.y0 + gap - fp[1]
        elif wall == "right":
            lx, ly = self.x1 - gap - fp[2], t - (fp[1] + fp[3]) / 2
        else:
            lx, ly = self.x0 + gap - fp[0], t - (fp[1] + fp[3]) / 2
        r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
        if not self.inside(r) or any(overlap(r, q) for q, tag in self.rects if tag in ("stair", "tall")):
            return None                                    # never through the stair or a wardrobe/bookcase
        self.h.put(path, lx, ly, self.z + z, yaw, collide=collide)
        return Item(path, lx, ly, self.z + z, yaw, None)


class Item:
    def __init__(self, path, lx, ly, lz, lyaw, rect, scale=(1.0, 1.0, 1.0)):
        self.path, self.lx, self.ly, self.lz, self.lyaw, self.rect, self.scale = path, lx, ly, lz, lyaw, rect, scale
        self.wall, self.t = None, None

    def at(self, u, v):
        """Item-local (u, v) -> house-local (x, y)."""
        du, dv = rot(u * self.scale[0], v * self.scale[1], self.lyaw)
        return self.lx + du, self.ly + dv

    def top(self):
        return self.lz + TOP.get(self.path, bounds(self.path)[5]) * self.scale[2]


SEAT_HEIGHT = {P + "Chair_1": 49.8, P + "Stool": 58.2, P + "Bench": 49.6, K + "Chair": 44.2, K + "Stool": 67.9,
               B + "FancyChairRed": 49.0}


def seat(room, path, x, y, yaw, allow=(), stand=75.0):
    """Every chair / stool / bench is a real seat (AKGSeat: E to sit). The furniture faces its local +Y, the seat
    actor's sitter faces +X, so the actor turns +90 and the mesh -90. `stand`: where the sitter is put on standing up
    (+ in front, - behind the seat; chairs pulled up to a table stand up backwards). Plain mesh if the class is
    missing from the running editor."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGSeat")
    if not cls:
        return room.place(path, x, y, yaw, allow=allow)
    r = room.spot(path, x, y, yaw, allow=allow)
    if not r:
        return None
    lift = max(0.0, -bounds(path)[2])
    a = room.h.spawn_class(cls, x, y, room.z + lift, yaw + 90.0, sub="Seats")
    a.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
    a.set_editor_property("seat_height", SEAT_HEIGHT.get(path, bounds(path)[5] * 0.5))
    a.set_editor_property("stand_distance", stand)
    a.set_seat_mesh(mesh(path))
    room.take(r, "block")
    return Item(path, x, y, room.z + lift, yaw, r)


def house_chest(h, room, path, x, y, yaw):
    """The family's lockable storage chest (AKGStorageChest, E to open): owned via HouseIndex = the house's index in
    Tools/Level/morrowmere_layout.json (build_house's style), a few starting valuables from the 'Chest' loot table.
    Plain mesh for landmarks or when the class is missing."""
    cls = unreal.load_class(None, "/Script/KillGodot.KGStorageChest")
    if h.kind != "home" or cls is None:
        return room.place(path, x, y, yaw)
    r = room.spot(path, x, y, yaw)
    if not r:
        return None
    c = h.spawn_class(cls, x, y, room.z, yaw, sub="Chests")
    for k, v in (("mesh_scale", 1.0), ("house_index", int(h.style)), ("starting_loot_table", "Chest"),
                 ("loot_seed", h.R.randint(1, 2 ** 30)), ("slots", 16)):
        try:
            c.set_editor_property(k, v)
        except Exception as e:   # a renamed property must not cost the whole house
            unreal.log_warning(f"KG_INTERIORS chest {k}: {e}")
    try:
        c.set_editor_property("display_name", unreal.Text("Family chest"))
    except Exception:
        pass
    c.set_chest_mesh(mesh(path))
    room.take(r, "block")
    h.chests += 1
    return Item(path, x, y, room.z, yaw, r)


def on(h, base, path, u, v, z=None, lyaw=0.0, scale=(1.0, 1.0, 1.0)):
    """Put clutter on a surface of `base` (item-local u, v; z = height above the base pivot, default its top)."""
    x, y = base.at(u, v)
    zz = (base.top() if z is None else base.lz + z) + max(0.0, -bounds(path)[2]) * scale[2]
    return h.put(path, x, y, zz, base.lyaw + lyaw, scale=scale, collide=False)


# ======================================================================================================== stairs
def stairs(h, g, up):
    """Solid kit stair rising to the back in the well, handrail on the open side, landing, joists, balustrade."""
    sx, sy = h.stair_x, h.stair_y
    h.put(V + "Stair_Interior_Solid", sx, sy, 0.0, 180.0, sub="Stairs")
    h.put(I + "StairRail", sx, sy, 0.0, 180.0, sub="Stairs")
    tile = V + ("Floor_WoodDark" if h.style % 2 else "Floor_WoodLight")           # matches the upper floor
    h.put(tile, sx, h.ey - LANDING / 2, FLOOR_H, 0.0, scale=(1.0, LANDING / 200.0, 1.0), sub="Stairs")

    # Ceiling joists under the upper floor (hide the tile seams), trimmers round the well. Decorative: no collision.
    zj = FLOOR_H - 1.0 - 6.0

    def beam(x0, y0, length, yaw):
        h.put(V + "Corner_Exterior_Wood", x0, y0, zj, yaw, pitch=-90.0, scale=(0.5, 0.5, length / 300.0),
              collide=False, sub="Stairs")
    for k in range(1, h.cd):
        y = -h.ey + 200.0 * k
        x0 = -h.ex + WALL if y <= h.ey - 400.0 + 1.0 else -h.ex + 200.0
        beam(x0, y, h.ex - WALL - x0, 0.0)
    beam(-h.ex + 200.0, h.ey - 400.0, 400.0 - LANDING, 90.0)

    # Balustrade round the well on the upper floor: along its front edge (wall -> corner) and its open side
    # (corner -> landing, where the stair's own newel takes over). The landing's side stays open: that is the exit.
    rx, ry = -h.ex + 206.0, h.ey - 406.0
    run_x = rx - (-h.ex + WALL)
    for k in range(2):
        h.put(I + "Railing_1m", -h.ex + WALL + run_x * (k + 0.5) / 2, ry, FLOOR_H + 1.0, 0.0,
              scale=(run_x / 200.0, 1.0, 1.0), sub="Stairs")
    run_y = (h.ey - LANDING - 6.0) - ry
    for k in range(3):
        h.put(I + "Railing_1m", rx, ry + run_y * (k + 0.5) / 3, FLOOR_H + 1.0, 90.0,
              scale=(run_y / 300.0, 1.0, 1.0), sub="Stairs")
    for x, y in ((-h.ex + WALL + 7.0, ry), (rx, ry), (rx, h.ey - LANDING - 6.0)):
        h.put(I + "RailPost", x, y, FLOOR_H + 1.0, 0.0, sub="Stairs")

    # Reservations. Ground: the stair body + its foot + the walk from the door. Upper: the well + the landing exit.
    g.take((-h.ex, h.foot_y, -h.ex + 200.0, h.ey), "stair")
    g.take((-h.ex, h.foot_y - 110.0, -h.ex + 215.0, h.foot_y), "keep")
    g.take((h.door_x - 95.0, -h.ey, h.door_x + 95.0, -h.ey + 190.0), "keep")
    # walk in from the door, then along the front of the room to the stair foot (clear of the newel post)
    g.keep_path((h.door_x, -h.ey + 140.0), (h.door_x, h.approach_y), 50.0)
    g.keep_path((h.door_x, h.approach_y), (-h.ex + 100.0, h.approach_y), 50.0)
    up.take((-h.ex, h.ey - 430.0, -h.ex + 222.0, h.ey), "stair")
    # upstairs: step off the landing sideways, then walk along the balustrade to the front; wider houses also get a
    # walk across the middle to the far side
    ex_x = -h.ex + 245.0
    up.take((-h.ex + 200.0, h.ey - LANDING - 20.0, -h.ex + 300.0, h.ey), "keep")
    up.keep_path((ex_x, h.ey - 60.0), (ex_x, -h.ey + 110.0), 36.0)
    if h.w >= 6:
        up.keep_path((ex_x, -h.ey * 0.1), (h.ex - 90.0, -h.ey * 0.1), 40.0)


def under_landing(h, g):
    """Storage in the nook behind the stair's top (under the landing)."""
    y = h.ey - WALL - 30.0
    picks = h.R.sample([P + "Bag", P + "Bucket_Wooden_1", P + "FarmCrate_Empty", P + "Bag"], 2)
    for k, path in enumerate(picks):
        h.put(path, -h.ex + 55.0 + k * 85.0, y, 2.0, 180.0 + h.R.uniform(-6, 6), collide=False)


# ======================================================================================================== ground floor
def hearth(h, g):
    """Stone fireplace on a closed stretch of the right wall (under the roof chimney), else the back wall."""
    cy = h.ey * 0.4
    item = g.on_wall(I + "Hearth", ["right"], prefer=lambda c: abs(c[4] - cy), tag="block")
    if item is None:
        item = g.on_wall(I + "Hearth", ["back", "left"], prefer=lambda c: -c[4] if c[3] == "back" else abs(c[4]))
    if item is None:
        return None
    # apron in front stays walkable (rugs allowed)
    x0, y0 = item.at(-100.0, 95.0)
    x1, y1 = item.at(100.0, 150.0)
    g.take((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)), "keep")
    fx, fy = item.at(0.0, 45.0)
    h.light(fx, fy, 70.0, 16.0, 900.0)                                      # firelight
    # mantel: candle, jug, a pot or two
    on(h, item, P + "CandleStick", -70.0, 50.0, lyaw=h.R.uniform(-20, 20))
    on(h, item, h.R.choice([P + "Vase_4", K + "Bottle", P + "Bottle_1"]), 62.0, 45.0, lyaw=h.R.uniform(0, 360))
    on(h, item, h.R.choice([P + "Mug", K + "Jar", P + "Potion_4"]), 30.0, 40.0, lyaw=h.R.uniform(0, 360))
    if h.R.random() < 0.6:
        on(h, item, P + "Pot_1_Lid", -25.0, 45.0, lyaw=h.R.uniform(0, 360))
    # firewood beside it, herbs drying on the wall on the other side
    side = h.R.choice((-1, 1))
    for s in (side, -side):
        u = s * 136.0
        x, y = item.at(u, 34.0)
        if g.spot(I + "Firewood", x, y, item.lyaw):
            g.place(I + "Firewood", x, y, item.lyaw)
            break
    return item


def stove(h, g):
    """Narrow houses: a black iron cooking stove (glowing plate) with pots, firewood beside it, the firelight above."""
    item = g.on_wall(K + "Cooker", ["right", "back"], prefer=lambda c: abs(c[4] - h.ey * 0.4) + (0 if c[3] == "right" else 300))
    if item is None:
        return None
    on(h, item, K + "SmallPot", -12.0, 0.0, lyaw=h.R.uniform(0, 360))
    on(h, item, K + "SmallPan", 18.0, 8.0, lyaw=h.R.uniform(0, 360))
    fx, fy = item.at(0.0, 70.0)
    h.light(fx, fy, 120.0, 12.0, 800.0)
    for s_ in h.R.sample((-1, 1), 2):
        x, y = item.at(s_ * 80.0, 30.0)
        if g.spot(I + "Firewood", x, y, item.lyaw):
            g.place(I + "Firewood", x, y, item.lyaw)
            break
    x0, y0 = item.at(-60.0, 90.0)
    x1, y1 = item.at(60.0, 130.0)
    g.take((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)), "keep")
    return item


def counter(h, g, near):
    """Kitchen dresser/counter with pots on top; a wall shelf of jars above it when the wall is closed."""
    big = h.w >= 6 and h.R.random() < 0.6
    path = K + ("DoubleCupboard" if big else "Cupboard")
    sc = (1.0, 0.62, 1.0)
    nx, ny = (near.lx, near.ly) if near else (h.ex, h.ey)
    item = g.on_wall(path, ["back", "right", "left", "front"], scale=sc,
                     prefer=lambda c: math.hypot(c[0] - nx, c[1] - ny) + (0 if c[3] in ("back", "right") else 400))
    if item is None:
        return None
    x0, _, _, x1, _, _ = bounds(path)
    span = x1 - x0
    stuff = [K + "SmallPot", K + "SmallPan", K + "Bowl", K + "Jar", K + "YellowJar", K + "Bottle", I + "Bread",
             I + "Cheese", K + "Cup", P + "Mug", K + "SmallPotLid", P + "Table_Plate"]
    n = 4 if big else 3
    for k, p in enumerate(h.R.sample(stuff, n)):
        u = x0 + span * (k + 0.5) / n + h.R.uniform(-8, 8)
        on(h, item, p, u, h.R.uniform(-8, 6), lyaw=h.R.uniform(0, 360))
    # shelf of jars on the wall above (only on closed wall)
    t0, t1 = (item.rect[0], item.rect[2]) if item.wall in ("back", "front") else (item.rect[1], item.rect[3])
    tm = (t0 + t1) / 2
    if h.wall_ok(0, item.wall, tm - 60, tm + 60, 200) and h.R.random() < 0.85:
        shelf = g.wall_mount(P + "Shelf_Simple", item.wall, tm, 160.0)
        for k in range(3 if shelf else 0):
            on(h, shelf, h.R.choice(JARS[:7]), -40.0 + k * 40.0 + h.R.uniform(-6, 6), 19.0, z=10.1,
               lyaw=h.R.uniform(0, 360))
    elif h.R.random() < 0.7:
        g.wall_mount(I + "HerbRack", item.wall, tm, 205.0)
    return item


def dining(h, g):
    """Table + chairs (+ bench) with a laid table, on a rug. Picks the biggest set that fits the free floor."""
    options = []
    if h.w >= 6:
        options.append((P + "Table_Large", 3, 60.0))
    options += [(K + "LongTable", 2, 55.0), (K + "Square_Table", 1, 55.0)]
    for path, per_side, chair_d in options:
        for yaw in (0.0, 90.0):
            fp = footprint(path, yaw)
            ext = (fp[0] - (chair_d + 10 if yaw == 90.0 else 0), fp[1] - (chair_d + 10 if yaw == 0.0 else 0),
                   fp[2] + (chair_d + 10 if yaw == 90.0 else 0), fp[3] + (chair_d + 10 if yaw == 0.0 else 0))
            best = None
            x = g.x0 - ext[0]
            while x <= g.x1 - ext[2]:
                y = g.y0 - ext[1]
                while y <= g.y1 - ext[3]:
                    r = (x + ext[0], y + ext[1], x + ext[2], y + ext[3])
                    if g.free(r, pad=25.0 if h.w >= 6 else 6.0):
                        score = math.hypot(x - h.ex * 0.15, y - h.ey * 0.05)
                        if best is None or score < best[0]:
                            best = (score, x, y)
                    y += 20.0
                x += 20.0
            if best:
                return lay_table(h, g, path, best[1], best[2], yaw + h.R.uniform(-3, 3), per_side)
    return wall_table(h, g)


def face_table(path, u, v):
    """Item-local yaw that squares a chair (front = +Y) at table-local (u, v) up to the nearest table edge."""
    x0, y0, _, x1, y1, _ = bounds(path)
    if max(y0 - v, v - y1) >= max(x0 - u, u - x1):
        return 0.0 if v < 0 else 180.0
    return -90.0 if u < 0 else 90.0


def wall_table(h, g):
    """Narrow rooms: a table pushed against the wall, seats on its open sides (the spot with the most seats wins)."""
    cx, cy = h.ex * 0.2, 0.0
    best = None
    for path in (K + "LongTable", K + "Square_Table", B + "SmallTable2"):
        x0, y0, _, x1, y1, _ = bounds(path)
        spots = [(x0 - 30.0, 0.0), (x1 + 30.0, 0.0)]
        spots += [(x0 + 40.0, y1 + 32.0), (x1 - 40.0, y1 + 32.0)] if path.endswith("LongTable") else [(0.0, y1 + 32.0)]
        for lx, ly, yaw, wall, t in g.wall_candidates(path, ["right", "back", "front", "left"], gap=3.0):
            fp = footprint(path, yaw)
            r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
            t0, t1 = (r[0], r[2]) if wall in ("back", "front") else (r[1], r[3])
            if not g.free(r) or not h.wall_ok(0, wall, t0, t1, 70.0):
                continue
            it = Item(path, lx, ly, g.z, yaw, r)
            free = [(u, v) for u, v in spots if g.spot(P + "Stool", *it.at(u, v), yaw + face_table(path, u, v))]
            score = (len(free), -math.hypot(lx - cx, ly - cy))
            if len(free) >= 2 and (best is None or score > best[0]):
                best = (score, path, lx, ly, yaw, free)
    if best is None:
        return None
    _, path, lx, ly, yaw, free = best
    t = g.place(path, lx, ly, yaw, check=False)
    for u, v in free:
        sx, sy = t.at(u, v)
        syaw = t.lyaw + face_table(path, u, v) + h.R.uniform(-8, 8)
        for sp in (h.R.choice([P + "Chair_1", K + "Chair"]), P + "Stool"):
            if seat(g, sp, sx, sy, syaw, stand=-62.0):
                break
    set_table(h, t, [(u, v) for u, v in free if v > 0])
    return t


def set_table(h, table, places):
    """Plates (+ a cup) in front of each place, a candle in the middle, bread/cheese/bowl."""
    x0, y0, _, x1, y1, _ = bounds(table.path)
    for u, v in places:
        pu = max(x0 + 18.0, min(x1 - 18.0, u))
        pv = max(y0 + 18.0, min(y1 - 18.0, v * 0.45))
        on(h, table, h.R.choice([P + "Table_Plate", K + "Plate"]), pu, pv, lyaw=h.R.uniform(0, 360))
        if h.R.random() < 0.6:
            on(h, table, h.R.choice([P + "Mug", K + "Cup", P + "Chalice"]), pu + (14.0 if pu < 0 else -14.0),
               pv * 0.6, lyaw=h.R.uniform(0, 360))
    on(h, table, h.R.choice([P + "CandleStick", P + "CandleStick_Triple"]), h.R.uniform(-10, 10), 0.0,
       lyaw=h.R.uniform(-20, 20))
    for p in h.R.sample([I + "Bread", I + "Cheese", K + "Bowl", P + "Bottle_1", K + "Jar"], 2):
        on(h, table, p, h.R.choice((-1, 1)) * h.R.uniform(25, max(26.0, (x1 - x0) / 2 - 20)), h.R.uniform(-8, 8),
           lyaw=h.R.uniform(0, 360))


def lay_table(h, g, path, x, y, yaw, per_side):
    x0, y0, _, x1, y1, _ = bounds(path)
    # rug first (the biggest that stays clear of walls, stair and hearth), then the table on it
    big = path == P + "Table_Large"
    rugs = [(I + "Rug_Rect", 1.25 if big else 1.0)] + ([] if big else [(I + "Rug_Oval", 1.0)]) +         [(I + "Rug_Rect", 1.0), (I + "Rug_Oval", 1.0)]
    if not big and h.R.random() < 0.5:
        rugs[0], rugs[1] = rugs[1], rugs[0]
    ryaw = yaw + (0.0 if abs(x1 - x0) >= abs(y1 - y0) else 90.0)
    for rug, rs in rugs:
        if g.spot(rug, x, y, ryaw, scale=(rs, rs, 1.0), allow=("keep",)):
            h.put(rug, x, y, g.z + 0.3, ryaw, scale=(rs, rs, 1.0), collide=False)
            break
    table = g.place(path, x, y, yaw, check=False)
    use_bench = path == P + "Table_Large" and h.R.random() < 0.5
    seats = []
    for side in (-1, 1):
        if side == 1 and use_bench:
            seats.append((P + "Bench", 0.0, y1 + 38.0))
            continue
        for k in range(per_side):
            u = x0 + (x1 - x0) * (k + 0.5) / per_side
            seats.append((h.R.choice([P + "Chair_1", P + "Chair_1", K + "Chair"]), u, (y0 - 30.0) if side < 0 else (y1 + 30.0)))
    if per_side == 1:                   # square table: stools at the ends too
        seats += [(P + "Stool", x0 - 32.0, 0.0), (P + "Stool", x1 + 32.0, 0.0)]
    places = []
    for spath, u, v in seats:
        jitter_v = h.R.uniform(-6, 10) * (1 if v > 0 else -1)
        sx, sy = table.at(u + h.R.uniform(-5, 5), v + jitter_v)
        syaw = face_table(path, u, v)
        if seat(g, spath, sx, sy, yaw + syaw + h.R.uniform(-8, 8), allow=("keep",), stand=-62.0) and                 spath != P + "Bench" and abs(v) > 1.0:
            places.append((u, v))
    g.take(table.rect, "block")
    set_table(h, table, places)
    return table


def tall_storage(h, g):
    """A filled bookcase or a pantry shelf against closed wall."""
    if h.R.random() < 0.5:
        path = P + "Bookcase_2"
    else:
        path = K + h.R.choice(["Shelf2", "Shelf2", "Shelf4"])
    item = g.on_wall(path, ["left", "right", "back", "front"], tag="tall")
    if item is None:
        return None
    if path == P + "Bookcase_2":
        for z in SHELVES[path][1:]:
            if h.R.random() < 0.85:
                on(h, item, h.R.choice(BOOKS), h.R.uniform(-8, 8), 2.0, z=z + 0.2)
        on(h, item, h.R.choice(SMALL_BOOKS + [P + "Vase_4"]), h.R.uniform(-40, 40), 0.0, z=SHELVES[path][0] + 0.2,
           lyaw=h.R.uniform(-30, 30))
    else:
        lv = SHELVES[path]
        x0, y0, _, x1, y1, _ = bounds(path)
        for z in lv:
            n = 1 if path.endswith("Shelf4") else h.R.randint(2, 3)
            for k in range(n):
                u = x0 + 12 + (x1 - x0 - 24) * (k + 0.5) / n
                on(h, item, h.R.choice(JARS), u + h.R.uniform(-4, 4), (y0 + y1) / 2, z=z + 0.2, lyaw=h.R.uniform(0, 360))
    return item


def storage(h, g, n):
    """Barrels, sacks and crates in corners / along walls."""
    kinds = [P + "Barrel", P + "Barrel_Apples", P + "Bag", P + "Bag", P + "Crate_Wooden", P + "FarmCrate_Carrot",
             P + "FarmCrate_Apple", P + "Bucket_Wooden_1"]
    corners = [(g.x0, g.y0), (g.x1, g.y0), (g.x0, g.y1), (g.x1, g.y1)]
    placed = 0
    for k in range(n * 3):
        if placed >= n:
            break
        path = h.R.choice(kinds)
        cx, cy = h.R.choice(corners)
        item = g.on_wall(path, ["left", "right", "back", "front"], gap=h.R.uniform(3, 10),
                         prefer=lambda c: math.hypot(c[0] - cx, c[1] - cy) + h.R.uniform(0, 40),
                         extra_yaw=h.R.uniform(-25, 25), collide=True)
        if item:
            placed += 1
            if path == P + "Crate_Wooden" and h.R.random() < 0.5:
                on(h, item, h.R.choice([P + "Bag", P + "FarmCrate_Apple", P + "Bucket_Wooden_1"]), 0.0, 0.0,
                   lyaw=h.R.uniform(-30, 30))
    return placed


def fireside_seat(h, g, fire):
    """An armchair or bench angled towards the fire, with a candle stand or a small table."""
    if fire is None:
        return
    for path in (B + "FancyChairRed", P + "Chair_1"):
        for u, v, turn in ((150.0, 175.0, -35.0), (-150.0, 175.0, 35.0), (0.0, 230.0, 0.0)):
            x, y = fire.at(u, v)
            yaw = fire.lyaw + 180.0 + turn + h.R.uniform(-8, 8)
            if g.spot(path, x, y, yaw):
                seat(g, path, x, y, yaw)
                x2, y2 = fire.at(u + (60.0 if u >= 0 else -60.0), v + 20.0)
                side = h.R.choice([B + "BedsideTable", P + "Stool"])
                it = g.place(side, x2, y2, yaw) if g.spot(side, x2, y2, yaw) else None
                if it:
                    on(h, it, h.R.choice([P + "Mug", P + "Book_Stack_1", P + "Candle_2"]), 0.0, 0.0,
                       z=TOP.get(side, 58.2), lyaw=h.R.uniform(0, 360))
                return


def hearth_rug(h, g, fire):
    if fire is None:
        return
    x, y = fire.at(0.0, 150.0)
    r = footprint(I + "Rug_Oval", fire.lyaw)
    rr = (x + r[0], y + r[1], x + r[2], y + r[3])
    if g.free(rr, allow=("keep",)):
        h.put(I + h.R.choice(["Rug_Oval", "Rug_Round"]), x, y, g.z + 0.35, fire.lyaw, collide=False)


def ground_home(h, g):
    fire = hearth(h, g) if h.w >= 6 else None
    fire = fire or stove(h, g)
    if h.w >= 6:
        counter(h, g, fire)
        hearth_rug(h, g, fire)
        dining(h, g)
    else:                                   # narrow house: the table gets the wall space before the dresser
        dining(h, g)
        counter(h, g, fire)
    fireside_seat(h, g, fire)
    tall_storage(h, g)
    storage(h, g, 3 if h.w <= 4 else 5)
    under_landing(h, g)
    # herbs drying from the joist nearest the fire
    if fire is not None:
        for k in range(h.R.randint(2, 3)):
            x, y = fire.at(-50.0 + k * 45.0 + h.R.uniform(-8, 8), 160.0)
            yj = -h.ey + 200.0 * round((y + h.ey) / 200.0)
            if -h.ey + 1 < yj < h.ey - 1 and (x > -h.ex + 210 or yj < h.ey - 400):
                h.put(I + "HerbBundle", x, yj, FLOOR_H - 13.0, h.R.uniform(0, 360), collide=False)


# ======================================================================================================== upper floor
def bedroom(h, u, beds=1, double=None):
    """Bed(s) with headboard to a wall, nightstand + candle (and its light), chest at the foot, wardrobe, rug, desk."""
    placed = []
    for b in range(beds):
        dbl = (h.w >= 6 and h.R.random() < 0.7) if double is None else double
        colour = h.R.choice(["Red", "Blue"])
        cands = [B + ("DoubleBed" if dbl else "SingleBed") + colour]
        if dbl and h.w >= 6 and h.R.random() < 0.3:
            cands.insert(0, P + h.R.choice(["Bed_Twin1", "Bed_Twin2"]))
        cands.append(B + "SingleBed" + colour)
        bed = None
        for path in cands:
            # Beds face +Y (foot) with the headboard at -Y: flush the headboard (mesh -Y) against the wall.
            bed = u.on_wall(path, ["back", "right", "front", "left"], gap=3.0, tall=90.0,
                            prefer=lambda c: {"back": 0, "right": 60, "front": 120, "left": 200}[c[3]] + h.R.uniform(0, 80))
            if bed:
                break
        if bed is None:
            continue
        placed.append(bed)
        x0, y0, _, x1, y1, _ = bounds(bed.path)
        # nightstand on a free side of the headboard
        for side in h.R.sample((-1, 1), 2):
            ns = h.R.choice([B + "BedsideTable", B + "BedsideCabinet", P + "Nightstand_Shelf"])
            nb = bounds(ns)
            u_ = (x1 + 6 - nb[0]) if side > 0 else (x0 - 6 - nb[3])
            x, y = bed.at(u_, y0 - nb[1] + 1.0)
            it = u.place(ns, x, y, bed.lyaw) if u.spot(ns, x, y, bed.lyaw) else None
            if it:
                on(h, it, h.R.choice([P + "CandleStick", P + "Candle_2"]), 0.0, 0.0, lyaw=h.R.uniform(0, 360))
                if h.R.random() < 0.6:
                    on(h, it, h.R.choice([P + "Book_7", P + "Book_5", P + "Scroll_1"]), 8.0, 4.0,
                       lyaw=h.R.uniform(0, 360))
                lx, ly = it.at(0.0, 10.0)
                h.light(lx, ly, FLOOR_H + 120.0, 6.0, 650.0)
                break
        if h.lights < 2 and b == 0:                       # no room for a nightstand: a candle glow over the pillow
            lx, ly = bed.at(0.0, y0 + 30.0)
            h.light(lx, ly, FLOOR_H + 150.0, 5.0, 600.0)
        # chest at the foot
        ch = h.R.choice([B + "ChestSquareIron", B + "ChestRoundIron"])
        cb = bounds(ch)
        x, y = bed.at(0.0, y1 + 4.0 - cb[1])
        if u.spot(ch, x, y, bed.lyaw):
            if h.chests == 0:
                house_chest(h, u, ch, x, y, bed.lyaw)
            else:
                u.place(ch, x, y, bed.lyaw)
        # rug beside / under the foot
        rug = h.R.choice([I + "Rug_Runner", I + "Rug_Oval", I + "Rug_Round"])
        side = h.R.choice((-1, 1))
        rx, ry = bed.at(side * ((x1 - x0) / 2 + 45.0), 10.0)
        rf = footprint(rug, bed.lyaw + 90.0)
        rr = (rx + rf[0], ry + rf[1], rx + rf[2], ry + rf[3])
        if u.free(rr, allow=("keep",)):
            h.put(rug, rx, ry, u.z + 0.3, bed.lyaw + 90.0, collide=False)
        else:
            rx, ry = bed.at(0.0, y1 + 70.0)
            rf = footprint(I + "Rug_Oval", bed.lyaw)
            if u.free((rx + rf[0], ry + rf[1], rx + rf[2], ry + rf[3]), allow=("keep",)):
                h.put(I + "Rug_Oval", rx, ry, u.z + 0.3, bed.lyaw, collide=False)
    if h.chests == 0 and h.kind == "home":               # no room at the bed foot: against any free wall
        ch = B + "ChestSquareIron"
        near = placed[0] if placed else None
        for lx, ly, yaw, wall, t in u.wall_candidates(ch, ["front", "right", "back", "left"], gap=4.0, prefer=(
                (lambda c: math.hypot(c[0] - near.lx, c[1] - near.ly)) if near else None)):
            if u.spot(ch, lx, ly, yaw) and house_chest(h, u, ch, lx, ly, yaw):
                break
    return placed


def wardrobe(h, u):
    item = u.on_wall(B + "Wardrobe", ["right", "front", "back", "left"], tag="tall")
    if item is None:
        item = u.on_wall(P + "Cabinet", ["right", "front", "back", "left"])
        if item:
            on(h, item, h.R.choice([P + "Vase_4", P + "CandleStick_Triple", K + "Bottle"]), h.R.uniform(-30, 30), 0.0,
               lyaw=h.R.uniform(0, 360))
    return item


def desk(h, u):
    tbl = u.on_wall(B + "SmallTable2", ["front", "right", "left", "back"], gap=4.0)
    if tbl is None:
        return None
    x, y = tbl.at(0.0, 45.0)
    for chair in (B + "FancyChairRed", P + "Stool"):
        if u.spot(chair, x, y, tbl.lyaw + 180.0):
            seat(u, chair, x, y, tbl.lyaw + 180.0 + h.R.uniform(-12, 12), stand=-62.0)
            break
    on(h, tbl, h.R.choice([P + "Scroll_2", P + "Book_7", P + "Book_Stack_2"]), h.R.uniform(-20, 0), 0.0,
       lyaw=h.R.uniform(-30, 30))
    on(h, tbl, h.R.choice([P + "Candle_1", P + "Potion_1", K + "Cup"]), 28.0, 5.0, lyaw=h.R.uniform(0, 360))
    return tbl


def centre_rug(h, room, target):
    """The biggest rug that fits on open floor (may cross walkways), as close to `target` as possible."""
    for rug in h.R.sample([I + "Rug_Rect", I + "Rug_Oval"], 2) + [I + "Rug_Round", I + "Rug_Runner"]:
        for yaw in (0.0, 90.0):
            fp = footprint(rug, yaw)
            best = None
            x = room.x0 - fp[0]
            while x <= room.x1 - fp[2]:
                y = room.y0 - fp[1]
                while y <= room.y1 - fp[3]:
                    if room.free((x + fp[0], y + fp[1], x + fp[2], y + fp[3]), allow=("keep",), pad=8.0):
                        sc = math.hypot(x - target[0], y - target[1])
                        if best is None or sc < best[0]:
                            best = (sc, x, y)
                    y += 20.0
                x += 20.0
            if best:
                h.put(rug, best[1], best[2], room.z + 0.3, yaw + h.R.uniform(-4, 4), collide=False)
                room.take((best[1] + fp[0], best[2] + fp[1], best[1] + fp[2], best[2] + fp[3]), "keep")
                return rug
    return None


def corner_set(h, u):
    """One lived-in vignette: a reading nook, a wash stand or a store of the family's goods."""
    kind = h.R.choice(["reading", "wash", "store"])
    if kind == "reading":
        bc = u.on_wall(P + "Bookcase_2", ["right", "front", "left", "back"], tag="tall")
        if bc:
            for z in SHELVES[P + "Bookcase_2"][1:]:
                if h.R.random() < 0.8:
                    on(h, bc, h.R.choice(BOOKS), h.R.uniform(-8, 8), 2.0, z=z + 0.2)
            x, y = bc.at(h.R.choice((-1, 1)) * 45.0, 95.0)
            yaw = bc.lyaw + 180.0 + h.R.uniform(-30, 30)
            if u.spot(B + "FancyChairRed", x, y, yaw):
                seat(u, B + "FancyChairRed", x, y, yaw)
            return
    if kind == "wash":
        d = u.on_wall(B + "BedsideCabinet", ["front", "right", "left", "back"], gap=3.0)
        if d:
            on(h, d, K + "Bowl", -4.0, 2.0, lyaw=h.R.uniform(0, 360))
            on(h, d, h.R.choice([K + "Bottle", K + "Jar", P + "Mug"]), 12.0, 4.0, lyaw=h.R.uniform(0, 360))
            u.on_wall(P + "Bucket_Wooden_1", [d.wall], prefer=lambda c: math.hypot(c[0] - d.lx, c[1] - d.ly),
                      extra_yaw=h.R.uniform(0, 360), collide=False)
            return
    for path in h.R.sample([P + "Crate_Wooden", P + "Bag", P + "Barrel", P + "FarmCrate_Empty", B + "ChestRoundIron"], 3):
        u.on_wall(path, ["front", "right", "back", "left"], extra_yaw=h.R.uniform(-20, 20))


def upper_home(h, u):
    bedroom(h, u)
    wardrobe(h, u)
    desk(h, u)
    corner_set(h, u)
    if h.w >= 6 and h.R.random() < 0.5:
        u.on_wall(h.R.choice([P + "Crate_Wooden", P + "Bag", P + "Barrel"]), ["front", "right", "back"],
                  extra_yaw=h.R.uniform(-20, 20))
    centre_rug(h, u, (h.ex * 0.3, -h.ey * 0.2))


# ======================================================================================================== landmarks
def ground_inn(h, g):
    fire = hearth(h, g)
    hearth_rug(h, g, fire)
    bar = g.on_wall(K + "DoubleCupboard", ["back", "right", "front"], scale=(1.0, 0.62, 1.0))
    if bar:
        x0, _, _, x1, _, _ = bounds(bar.path)
        for k in range(5):
            on(h, bar, h.R.choice([P + "Mug", P + "Mug", K + "Cup", P + "Bottle_1"]), x0 + 20 + k * (x1 - x0 - 40) / 4, 0.0,
               lyaw=h.R.uniform(0, 360))
    g.on_wall(P + "Barrel_Holder", ["back", "right", "left"], tall=126.0)
    for _ in range(2):
        dining(h, g)
    storage(h, g, 4)
    under_landing(h, g)


def ground_hall(h, g):
    fire = hearth(h, g)
    hearth_rug(h, g, fire)
    dining(h, g)
    for _ in range(2):
        tall_storage(h, g)
    g.on_wall(P + "BookStand", ["front", "right", "left"], extra_yaw=h.R.uniform(-15, 15))
    for wall in ("left", "right"):
        spans = [s for s in h.segments(0, wall) if s[2] == "solid"]
        if spans:
            s = h.R.choice(spans)
            g.wall_mount(P + "Banner_1_Cloth", wall, (s[0] + s[1]) / 2, 280.0)
    storage(h, g, 2)
    under_landing(h, g)


def ground_bakery(h, g):
    fire = hearth(h, g)
    c = counter(h, g, fire)
    if c:
        for k in range(3):
            on(h, c, I + "Bread", -30.0 + k * 30.0, 10.0, lyaw=h.R.uniform(0, 360))
    tbl = g.on_wall(P + "Workbench", ["back", "right", "left", "front"])
    if tbl:
        for k in range(4):
            on(h, tbl, I + "Bread", -70.0 + k * 45.0, h.R.uniform(-15, 15), lyaw=h.R.uniform(0, 360))
    storage(h, g, 5)
    for _ in range(3):
        g.on_wall(P + "Bag", ["left", "right", "back", "front"], extra_yaw=h.R.uniform(-30, 30))
    under_landing(h, g)
    fireside_seat(h, g, fire)


def ground_boathouse(h, g):
    fire = hearth(h, g)
    hearth_rug(h, g, fire)
    bench = g.on_wall(P + "Workbench", ["right", "back", "left", "front"])
    if bench:
        on(h, bench, WP + "Oar", -60.0, 5.0, lyaw=h.R.uniform(-10, 10))
        on(h, bench, P + "Rope_1", 50.0, 0.0, lyaw=h.R.uniform(0, 360))
    for path in (P + "Rope_2", P + "Rope_3", P + "Chain_Coil"):
        g.on_wall(path, ["left", "right", "back", "front"], tall=10.0, extra_yaw=h.R.uniform(0, 360), collide=False)
    storage(h, g, 6)
    dining(h, g)
    under_landing(h, g)


def ground_barn(h, g):
    fire = hearth(h, g)
    g.on_wall(P + "Workbench", ["right", "back", "left", "front"])
    for _ in range(4):
        g.on_wall(P + "Bag", ["left", "right", "back", "front"], extra_yaw=h.R.uniform(-30, 30))
    storage(h, g, 8)
    dining(h, g)
    under_landing(h, g)
    fireside_seat(h, g, fire)


def ground_chapel(h, g):
    """Pews facing an altar at the back wall, candles, banners, a lectern."""
    altar = g.on_wall(P + "Cabinet", ["back"], prefer=lambda c: abs(c[4]))
    if altar:
        for u in (-45.0, 45.0):
            on(h, altar, P + "CandleStick_Triple", u, 0.0)
        on(h, altar, P + "Book_7", 0.0, 2.0)
        for u in (-110.0, 110.0):
            x, y = altar.at(u, 10.0)
            g.place(P + "CandleStick_Stand", x, y, altar.lyaw)
        ax, ay = altar.at(0.0, 30.0)
        h.light(ax, ay, 220.0, 12.0, 900.0)
    y = h.ey - 300.0                                   # two columns of pews facing the altar, aisle down the middle
    while y > -h.ey + 200.0:
        for x in (h.ex - WALL - 150.0, -h.ex + WALL + 150.0):
            if g.spot(P + "Bench", x, y, 0.0):
                seat(g, P + "Bench", x, y, h.R.uniform(-2, 2), stand=58.0)
        y -= 115.0
    g.on_wall(P + "BookStand", ["right"], extra_yaw=h.R.uniform(-10, 10))
    for wall in ("left", "right"):
        for s in h.segments(0, wall):
            if s[2] == "solid" and h.R.random() < 0.6:
                g.wall_mount(P + h.R.choice(["Banner_1_Cloth", "Banner_2_Cloth"]), wall, (s[0] + s[1]) / 2, 285.0)
    under_landing(h, g)


def upper_dorm(h, u):
    bedroom(h, u, beds=3, double=False)
    wardrobe(h, u)
    storage(h, u, 2)


def upper_archive(h, u):
    desk(h, u)
    for _ in range(3):
        tall_storage(h, u)
    storage(h, u, 3)
    bedroom(h, u, beds=1, double=False)


def upper_loft(h, u):
    for _ in range(5):
        u.on_wall(P + h.R.choice(["Bag", "Crate_Wooden", "FarmCrate_Empty", "Barrel"]), ["left", "right", "back", "front"],
                  extra_yaw=h.R.uniform(-30, 30))
    bedroom(h, u, beds=1, double=False)
    storage(h, u, 3)


PLANS = {"home": (ground_home, upper_home), "inn": (ground_inn, upper_dorm), "town_hall": (ground_hall, upper_archive),
         "bakery": (ground_bakery, upper_home), "boathouse": (ground_boathouse, upper_home),
         "mill_barn": (ground_barn, upper_loft), "church": (ground_chapel, upper_archive)}


# ======================================================================================================== entry point
def furnish(f, w, d, style, warm_light):
    """Stairs + furnished rooms for one build_house house. Returns the number of actors spawned."""
    h = House(f, w, d, style, warm_light)
    g, up = Room(h, 0), Room(h, 1)
    stairs(h, g, up)
    ground, upper = PLANS.get(h.kind, PLANS["home"])
    ground(h, g)
    upper(h, up)
    return h.count
