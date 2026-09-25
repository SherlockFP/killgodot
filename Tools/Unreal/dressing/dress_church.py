"""Church zone: the gothic-cosy churchyard of Morrowmere (circle r=24 m around (-2800, -4500)).

Existing (kg_build_village.py): church (-2800,-4800) 8x12 m facing (-2400,-4000), bell tower (-1900,-5200), graveyard
grid centred (-3800,-4000) with an iron fence at y=-3440, dead tree, candle stand, chores LightCandles / RingBell /
TendGraves, church_lane ending at the church door.

Points of interest built here (see POIS):
  lychgate + low churchyard walls where the lane enters, the hooded-saint statue on the forecourt lawn, the tended
  graveyard (old stool-like markers encased in proper stones + new rows), the Morrow family plot behind an iron fence,
  the mausoleum / crypt at the head of the graveyard (future catacomb entrance: its door apron stays empty), the
  gravedigger's shed with coffins and a loot chest, the fresh-dug grave (digging teaser, buried iron chest), the
  paupers' field of wooden crosses, the forgotten graves in the west hollow (robbed grave), the old hill graves behind
  the church, the drowned sailors' memorial on the hilltop, the bell-ringer's rest at the tower and a wayside calvary
  outside the wall (unconsecrated graves).
"""
import math
import random

import unreal
import kg_dress_common as C

V, N, P, WP = C.V, C.N, C.P, C.WP
MAT_TRIM = "/Game/KillGodot/Env/KG_Village/Materials/MI_RockTrim"
MAT_PINE = "/Game/KillGodot/Env/KG_Nature/Materials/Leaves_Pine"
MAT_UBRICK = "/Game/KillGodot/Env/KG_Village/Materials/MI_UnevenBrick"
CHEST_IRON = "/Game/KillGodot/Env/Furniture/KG_Bedroom/StaticMeshes/Bedroom_ChestRoundIron"

CX, CY = -2800.0, -4500.0
R = random.Random(1666)
POIS = []
_inst = {}
_mats = {}


# ============================================================================================ small helpers
def g(x, y):
    return C.ground(x, y)


def gmin(x, y, r):
    return C.ground_min(x, y, r)


def norm(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def mat(path):
    if path not in _mats:
        _mats[path] = unreal.load_asset(path)
    return _mats[path]


def override(actor, mat_path, slots=None):
    """Swap materials on a placed actor (all slots, or only the given slot indices)."""
    if not actor:
        return actor
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    for i in range(comp.get_num_materials()):
        if slots is None or i in slots:
            comp.set_material(i, mat(mat_path))
    return actor


def inst(path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=6000.0):
    """Queue one instance; the first queue call of a mesh fixes its collision/cull (one HISM per mesh)."""
    e = _inst.setdefault(path, {"t": [], "collide": collide, "cull": cull})
    e["t"].append((x, y, z, yaw, scale, pitch, roll))


def flush():
    for path, e in _inst.items():
        C.instanced(path, e["t"], collide=e["collide"], cull=e["cull"], sub="Instanced")
    _inst.clear()


def put(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, sub="Props", collide=True, cull=0.0,
        claim=0.0, sink=0.0, shadow=True):
    return C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=scale, sub=sub, collide=collide, cull=cull,
                   claim_r=claim, sink=sink, shadow=shadow)


def rot_xz(X, Z):
    r = unreal.MathLibrary.make_rot_from_xz(unreal.Vector(*X), unreal.Vector(*Z))
    return r.roll, r.pitch, r.yaw


class Tilt:
    """A leaning local frame at (x, y, z): yaw (deg), fb = lean forward/back (top towards +Y local), side = lean
    sideways (top towards +X local). Exposes world axes ex, ey, up and pos(lx, ly, lz)."""

    def __init__(self, x, y, z, yaw, fb=0.0, side=0.0):
        self.x, self.y, self.z = x, y, z
        up = norm((math.tan(math.radians(side)), math.tan(math.radians(fb)), 1.0))
        ex = norm((1.0 - up[0] * up[0], -up[0] * up[1], -up[0] * up[2]))
        ey = cross(up, ex)
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        rot = lambda v: (v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2])  # noqa: E731
        self.ex, self.ey, self.up = rot(ex), rot(ey), rot(up)

    def pos(self, lx, ly, lz):
        return tuple(b + self.ex[i] * lx + self.ey[i] * ly + self.up[i] * lz for i, b in enumerate((self.x, self.y, self.z)))

    def piece(self, path, lx, ly, lz, X, Z, scale, collide=True, cull=9000.0):
        """Instance with mesh X/Z axes given as world vectors."""
        x, y, z = self.pos(lx, ly, lz)
        roll, pitch, yaw = rot_xz(X, Z)
        inst(path, x, y, z, yaw, scale, pitch, roll, collide=collide, cull=cull)


def neg(v):
    return tuple(-c for c in v)


class Fr:
    """Flat local frame; local -Y is the front."""

    def __init__(self, x, y, yaw, z=None):
        self.x, self.y, self.yaw = x, y, yaw
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        self.z = g(x, y) if z is None else z

    def w(self, lx, ly):
        return self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c

    def put(self, path, lx, ly, lz=None, lyaw=0.0, **kw):
        x, y = self.w(lx, ly)
        z = g(x, y) if lz is None else self.z + lz
        return put(path, x, y, z, yaw=self.yaw + lyaw, **kw)

    def inst(self, path, lx, ly, lz=None, lyaw=0.0, **kw):
        x, y = self.w(lx, ly)
        z = g(x, y) if lz is None else self.z + lz
        inst(path, x, y, z, self.yaw + lyaw, **kw)


def poi(name, x, y, desc):
    POIS.append({"name": name, "x": round(x), "y": round(y), "desc": desc})


def ok(x, y, r, **kw):
    return C.free(x, y, r, **kw)


def facing(ax, ay, bx, by):
    """Frame yaw whose front (local -Y) looks from a at b."""
    return math.degrees(math.atan2(by - ay, bx - ax)) + 90.0


# ============================================================================================ existing level
def claim_existing():
    """Claim everything already standing in/near the zone (trees, rocks, old graves, other zones' props) so the
    scatter never lands inside it. Returns the old graveyard 'stool' markers to encase."""
    stools = []
    for a in C.actors.get_all_level_actors():
        f = str(a.get_folder_path())
        if f.startswith("Dress/Church") or f.startswith("Village/Interior") or f in ("Sky", "Lighting", "Terrain"):
            continue
        l = a.get_actor_location()
        if math.hypot(l.x - CX, l.y - CY) > 2700.0:
            continue
        comp = a.get_component_by_class(unreal.StaticMeshComponent)
        if not comp or not comp.static_mesh or a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if a.get_class().get_name() in ("KGTaskStation", "KGSeat", "KGStorageChest", "KGBreakable"):
                C.claim(l.x, l.y, 90.0)
            continue
        name = comp.static_mesh.get_name()
        _, ext = a.get_actor_bounds(False)
        e = max(ext.x, ext.y)
        if f == "Village/Graveyard" and name == "Stairs_Exterior_Platform":
            stools.append((l.x, l.y, a.get_actor_rotation().yaw))
            C.claim(l.x, l.y, 60.0)
        elif "Tree" in name or "Pine" in name or "Sakura" in name:
            C.claim(l.x, l.y, 130.0 if e > 300 else 90.0)
        elif "Rock_" in name:
            C.claim(l.x, l.y, e * 0.8)
        elif "Fence" in name:
            fx = math.cos(math.radians(a.get_actor_rotation().yaw))
            fy = math.sin(math.radians(a.get_actor_rotation().yaw))
            for t in (-80.0, -40.0, 0.0, 40.0, 80.0):
                C.claim(l.x + fx * t, l.y + fy * t, 25.0)
        elif f.startswith("Village/House") or f.startswith("Village/Landmarks") or f.startswith("Village/Ladders"):
            continue          # building footprints are covered by C.building_hit
        else:
            C.claim(l.x, l.y, min(e, 220.0) + 10.0)
    return stools


# ============================================================================================ grave kit
def stone(kind, x, y, yaw, fb=0.0, side=0.0, s=1.0, sink=4.0, z=None, flowers=0.0, candles=0.0, claim=True):
    """One grave marker at (x, y); its inscribed face (and the grave body) is on local -Y. kind: tablet, tablet2,
    slab, arch, obelisk, xstone, xwood, cairn, rock, tomb, ledger, mound, broken."""
    zz = (gmin(x, y, 45.0) - sink) if z is None else z
    t = Tilt(x, y, zz, yaw, fb, side)
    ex, ey, up = t.ex, t.ey, t.up
    if kind in ("tablet", "tablet2", "broken"):
        mesh = V + ("Prop_ExteriorBorder_Straight1" if kind != "tablet2" else "Prop_ExteriorBorder_Straight2")
        h = (1.0 if kind != "broken" else 0.55) * s
        # upright border stone: mesh Y -> up (height 70), mesh Z -> -ey (thickness 13)
        t.piece(mesh, 0.0, 6.0, 0.0, ex, neg(ey), (0.34 * s, h * R.uniform(0.95, 1.2), 1.0))
    elif kind == "slab":
        a = put(V + "Wall_UnevenBrick_Straight", *t.pos(0.0, 5.0, 0.0), sub="Graves", cull=9000.0,
                scale=(0.36 * s, 0.6, 0.26 * s * R.uniform(0.95, 1.12)))
        if a:
            r, p, yw = rot_xz(ex, up)
            a.set_actor_rotation(unreal.Rotator(roll=r, pitch=p, yaw=yw), False)
            override(a, MAT_TRIM)
    elif kind == "arch":
        t.piece(V + "DoorFrame_Round_Brick", 0.0, 0.0, 0.0, ex, up, (0.4 * s, 0.3, 0.33 * s))
        t.piece(V + "Prop_ExteriorBorder_Straight2", 0.0, 12.0, 0.0, ex, neg(ey), (0.3 * s, 1.15 * s, 0.8))
    elif kind == "obelisk":
        hz = 0.42 * s * R.uniform(0.9, 1.25)
        t.piece(V + "Corner_Exterior_Brick", 4.0, -3.0, 0.0, ex, up, (0.5 * s, 0.5 * s, hz))
        inst(P + "Vase_4", *t.pos(0.0, 0.0, 318.0 * hz - 6.0), yaw + R.uniform(0, 90), 0.62 * s, collide=True, cull=9000.0)
    elif kind == "xstone":
        t.piece(V + "Corner_Exterior_Brick", 2.0, -2.0, 0.0, ex, up, (0.3 * s, 0.3 * s, 0.37 * s))
        t.piece(V + "Corner_Exterior_Brick", -32.0 * s, 0.0, 78.0 * s, neg(up), ex, (0.28 * s, 0.28 * s, 0.2 * s))
    elif kind == "xwood":
        t.piece(V + "Corner_Exterior_Wood", 0.0, 0.0, -10.0, ex, up, (0.5 * s, 0.5 * s, 0.43 * s))
        t.piece(V + "Corner_Exterior_Wood", -30.0 * s, 0.0, 80.0 * s, neg(up), ex, (0.45 * s, 0.45 * s, 0.2 * s))
    elif kind == "cairn":
        t.piece(V + "Corner_Exterior_Brick", 0.0, 0.0, 0.0, ex, up, (0.9 * s, 0.3, 0.2 * s * R.uniform(0.9, 1.2)))
    elif kind == "rock":
        inst(N + "Rock_Medium_2", x, y, zz - 6.0, R.uniform(0, 360), 0.16 * s * R.uniform(0.9, 1.2),
             R.uniform(-6, 6), R.uniform(-6, 6), collide=True, cull=9000.0)
    elif kind == "tomb":
        f = Fr(x, y, yaw, z=zz)
        a = f.put(V + "Wall_UnevenBrick_Straight", -23.0, -60.0, lz=0.0, lyaw=90.0, sub="Graves", cull=0.0,
                  scale=(1.0, 2.2, 0.21 * s))
        override(a, MAT_TRIM)
        lid_z = 312.0 * 0.21 * s
        f.inst(V + "Prop_ExteriorBorder_Straight1", 54.0, -60.0, lid_z, 90.0, scale=(1.08, 1.55, 1.0),
               collide=True, cull=9000.0)
        if claim:
            C.claim(*f.w(0.0, -60.0), 110.0)
    elif kind == "ledger":
        f = Fr(x, y, yaw, z=zz + 2.0)
        f.inst(V + "Prop_ExteriorBorder_Straight1", 45.0, -120.0, 0.0, 90.0, scale=(0.95, 1.3, 1.0), collide=True,
               cull=9000.0)
    elif kind == "mound":
        f = Fr(x, y, yaw, z=zz)
        f.inst(WP + "SM_KG_DigMound", 0.0, -95.0, -8.0, 0.0, scale=(0.72, 1.45, 0.55), cull=7000.0)
    if claim and kind not in ("tomb",):
        C.claim(x, y, 45.0)
    if flowers and R.random() < flowers:
        offering(x, y, yaw, "flowers")
    if candles and R.random() < candles:
        offering(x, y, yaw, "candles")


def offering(x, y, yaw, kind):
    """Flowers / candles / a vase at the foot of a marker (front side)."""
    f = Fr(x, y, yaw)
    if kind == "flowers":
        for k in range(R.randint(1, 3)):
            lx, ly = R.uniform(-28, 28), R.uniform(-38, -22)
            path = R.choice([N + "Flower_3_Single", N + "Flower_4_Single", N + "Flower_3_Single"])
            f.inst(path, lx, ly, None, R.uniform(0, 360), scale=R.uniform(0.18, 0.26), cull=5000.0)
        if R.random() < 0.5:
            f.inst(N + R.choice(["Petal_1", "Petal_2", "Petal_4"]), R.uniform(-20, 20), -40.0, None, R.uniform(0, 360),
                   scale=R.uniform(0.45, 0.7), cull=5000.0)
    elif kind == "candles":
        for k in range(R.randint(2, 4)):
            lx, ly = R.uniform(-30, 30), R.uniform(-34, -18)
            f.inst(P + R.choice(["Candle_1", "Candle_2", "Candle_2"]), lx, ly, None, R.uniform(0, 360),
                   scale=R.uniform(1.1, 1.6), cull=3500.0)
    elif kind == "vase":
        x2, y2 = f.w(R.uniform(-22, 22), -30.0)
        z2 = g(x2, y2)
        inst(P + "Vase_4", x2, y2, z2, R.uniform(0, 360), 0.55, collide=True, cull=6000.0)
        inst(N + "Flower_3_Group", x2, y2, z2 + 14.0, R.uniform(0, 360), 0.14, cull=5000.0)


def hedge(x, y, yaw, sx=1.0, sy=0.7, sz=0.75, sub="Hedges"):
    a = put(N + "Bush_Common", x, y, g(x, y) - 6.0, yaw=yaw, scale=(sx, sy, sz), sub=sub, collide=True, cull=12000.0)
    C.claim(x, y, 70.0 * max(sx, sy))
    return override(a, MAT_PINE)


def yew(x, y, s=1.0):
    put(N + "Pine_3", x, y, g(x, y) - 10.0, yaw=R.uniform(0, 360), scale=(0.42 * s, 0.42 * s, 0.62 * s), sub="Trees",
        claim=90.0)


def dead_tree(path, x, y, s, mushrooms=True):
    put(N + path, x, y, gmin(x, y, 60.0) - 15.0, yaw=R.uniform(0, 360), scale=s, sub="Trees", claim=110.0)
    if mushrooms:
        for k in range(R.randint(3, 6)):
            a = R.uniform(0, 2 * math.pi)
            d = R.uniform(40, 110)
            mx, my = x + math.cos(a) * d, y + math.sin(a) * d
            inst(N + R.choice(["Mushroom_Common", "Mushroom_Common", "Mushroom_Laetiporus"]), mx, my, g(mx, my) - 3.0,
                 R.uniform(0, 360), R.uniform(0.35, 0.7), cull=4500.0)


def lamp_post(x, y, yaw, intensity=9.0, radius=900.0, color=(255, 170, 95), sub="Lamps"):
    """Wooden post with the kit's wall lantern (arm along local +Y after yaw) + its light."""
    z = g(x, y)
    put(V + "Corner_Exterior_Wood", x, y, z - 5.0, yaw=yaw, scale=(1.1, 1.1, 0.95), sub=sub, claim=40.0)
    put(P + "Lantern_Wall", x, y, z + 160.0, yaw=yaw, sub=sub, collide=False)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    C.light(x - 100.0 * s, y + 100.0 * c, z + 215.0, intensity=intensity, radius=radius, color=color)


def bench(x, y, face_yaw, z=None, sub="Seats"):
    """KGSeat bench; face_yaw = world direction the sitter looks."""
    zz = g(x, y) if z is None else z
    s = C.seat(P + "Bench", x, y, zz, yaw=face_yaw, seat_height=49.6, sub=sub)
    if s and s.get_class().get_name() == "KGSeat":
        try:
            s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
            s.set_editor_property("stand_distance", 70.0)
            s.set_seat_mesh(C.mesh(P + "Bench"))
        except Exception as e:  # keep the zone alive
            unreal.log_warning(f"KG_DRESS church bench: {e}")
    C.claim(x, y, 120.0)
    return s


def smash(path, x, y, yaw=None):
    C.breakable(path, x, y, g(x, y) + 2.0, yaw=R.uniform(0, 360) if yaw is None else yaw)
    C.claim(x, y, 45.0)


def path_stones(pts, step=105.0, width=40.0, sink=9.0):
    """Stepping-stone path along a polyline (no collision)."""
    kinds = [N + "RockPath_Square_Small_1", N + "RockPath_Square_Small_2", N + "RockPath_Square_Small_3",
             N + "RockPath_Round_Small_1", N + "RockPath_Round_Small_2", N + "RockPath_Round_Small_3"]
    carry = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / seg, (by - ay) / seg
        t = carry
        while t < seg:
            j = R.uniform(-width, width) * 0.5
            x, y = ax + dx * t - dy * j, ay + dy * t + dx * j
            inst(R.choice(kinds), x, y, g(x, y) - sink, R.uniform(0, 360), R.uniform(0.62, 0.8), cull=7000.0)
            C.claim(x, y, 50.0)
            t += step * R.uniform(0.85, 1.1)
        carry = t - seg


def clear(x, y, r):
    try:
        C.clear_grass(x, y, r)
    except Exception as e:
        unreal.log_warning(f"KG_DRESS church clear_grass: {e}")


def lane_frame():
    for name, w, pts in C.lanes():
        if name == "church_lane":
            return w, pts
    return 320.0, [(-400.0, -1600.0), (-2400.0, -4000.0)]


# ============================================================================================ POIs
def paths():
    """Stepping-stone paths off the lane end: to the graveyard aisle, up to the crypt, round to the tower door, down
    to the shed."""
    path_stones([(-2560.0, -3960.0), (-2900.0, -3935.0), (-3300.0, -3890.0), (-3560.0, -3878.0)])
    path_stones([(-3800.0, -3878.0), (-4180.0, -3875.0)], step=120.0)
    path_stones([(-3680.0, -4120.0), (-3680.0, -4380.0), (-3760.0, -4600.0)], step=115.0, width=20.0)
    path_stones([(-2380.0, -4090.0), (-2090.0, -4390.0), (-2060.0, -4560.0), (-2280.0, -5000.0), (-2330.0, -5280.0),
                 (-2230.0, -5530.0), (-2060.0, -5630.0)], step=115.0, width=25.0)
    path_stones([(-2420.0, -3900.0), (-2560.0, -3560.0), (-2750.0, -3200.0), (-3050.0, -3080.0)], step=120.0)


def lychgate():
    """Roofed timber gate where the church lane enters the churchyard, low stone walls either side."""
    w, pts = lane_frame()
    (ax, ay), (bx, by) = pts[-2], pts[-1]
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    gx, gy = ax + dx * 250.0, ay + dy * 250.0
    yaw = math.degrees(math.atan2(-dx, dy))          # local +X across the lane (towards the NE), +Y back down it
    f = Fr(gx, gy, yaw)
    posts = [(sx * 235.0, sy * 85.0) for sx in (-1, 1) for sy in (-1, 1)]
    gz = max(g(*f.w(px, py)) for px, py in posts)
    f.z = gz
    top = 285.0
    for px, py in posts:
        wx, wy = f.w(px, py)
        base = g(wx, wy) - 6.0
        put(V + "Corner_Exterior_Wood", wx, wy, base, yaw=yaw, scale=(1.5, 1.5, (gz + top - base) / 300.0), sub="Lychgate")
        C.claim(wx, wy, 35.0)
    for sy in (-85.0, 85.0):        # tie beams across the lane
        f.put(V + "Corner_Exterior_Wood", -270.0, sy, top - 18.0, pitch=-90.0, scale=(1.1, 1.1, 1.8), sub="Lychgate")
    for sx in (-235.0, 235.0):      # side plates along it
        f.put(V + "Corner_Exterior_Wood", sx, -110.0, top - 16.0, roll=90.0, scale=(1.0, 1.0, 0.74), sub="Lychgate")
    f.put(V + "Roof_RoundTiles_4x4", 0.0, 0.0, top, scale=(0.98, 0.5, 0.42), sub="Lychgate")
    f.put(V + "Roof_Front_Brick4", 0.0, -100.0, top, scale=(0.98, 0.5, 0.42), sub="Lychgate")
    f.put(V + "Roof_Front_Brick4", 0.0, 100.0, top, lyaw=180.0, scale=(0.98, 0.5, 0.42), sub="Lychgate")
    cx_, cy_ = f.w(0.0, 0.0)
    C.mover(P + "Chandelier", cx_, cy_, gz + top - 4.0, yaw=yaw, scale=0.5, sway=3.0, sway_hz=0.3, sub="Lychgate")
    C.light(cx_, cy_, gz + 225.0, intensity=10.0, radius=950.0)
    # Pall-bearers' benches along the sides.
    for sx, face in ((-198.0, yaw), (198.0, yaw + 180.0)):
        bx_, by_ = f.w(sx, 0.0)
        bench(bx_, by_, face)
    # Low stone walls: 4 segments on the SW side, 2 on the NE side (the sakura), pillars with urns at the ends.
    for side, n in ((-1, 4), (1, 2)):
        for k in range(n):
            lx = side * (265.0 + 100.0 + 200.0 * k)
            e1, e2 = f.w(lx - 100.0, 0.0), f.w(lx + 100.0, 0.0)
            hs = [g(*e1), g(*e2), g(*f.w(lx, 0.0))]
            bot, topz = min(hs) - 8.0, max(hs) + 85.0
            wx, wy = f.w(lx, 0.0)
            a = put(V + "Wall_UnevenBrick_Straight", wx, wy, bot, yaw=yaw, scale=(1.0, 0.8, (topz - bot) / 312.0),
                    sub="Walls", cull=0.0)
            override(a, MAT_UBRICK)
            f.inst(V + "Prop_ExteriorBorder_Straight1", lx, -24.0, topz - f.z, 0.0, scale=(1.02, 0.62, 1.0),
                   collide=True, cull=9000.0)
            for t in (-80.0, -40.0, 0.0, 40.0, 80.0):
                C.claim(*f.w(lx + t, 0.0), 30.0)
            if R.random() < 0.6:     # ivy and flowers at the foot of the wall (churchyard side = local -Y)
                fx, fy = f.w(lx + R.uniform(-60, 60), -45.0)
                inst(N + R.choice(["Fern_1", "Plant_7", "Clover_1"]), fx, fy, g(fx, fy) - 3.0, R.uniform(0, 360),
                     R.uniform(0.4, 0.6), cull=5000.0)
        for lx in (side * 262.0, side * (265.0 + 200.0 * n + 12.0)):
            wx, wy = f.w(lx, 0.0)
            gb = g(wx, wy) - 8.0
            put(V + "Corner_ExteriorWide_Brick", wx, wy, gb, yaw=yaw, scale=(0.78, 0.78, (gz + 118.0 - gb) / 322.0),
                sub="Walls")
            put(P + "Vase_2", wx - 3.0, wy + 3.0, gz + 118.0, yaw=R.uniform(0, 90), scale=0.5, sub="Walls")
            C.claim(wx, wy, 45.0)
    # Parish notice board outside the gate, facing the arrivals.
    nx, ny = f.w(-450.0, 95.0)
    fb_ = Fr(nx, ny, yaw + 180.0)
    for sx in (-80.0, 80.0):
        x, y = fb_.w(sx, 0.0)
        put(V + "Corner_Exterior_Wood", x, y, g(x, y) - 5.0, yaw=fb_.yaw, scale=(0.9, 0.9, 0.62), sub="Lychgate")
    put(V + "Wall_Plaster_WoodGrid", nx, ny, g(nx, ny) + 85.0, yaw=fb_.yaw, scale=(0.85, 0.3, 0.33), sub="Lychgate")
    for k, lx in enumerate((-45.0, 0.0, 45.0)):
        fb_.put(P + ("Scroll_1" if k % 2 == 0 else "Scroll_2"), lx, -14.0, 118.0 + (k % 2) * 22.0, 90.0, sub="Lychgate",
                collide=False, cull=4000.0)
    C.claim(nx, ny, 110.0)
    # Yews just inside the gate.
    for sx in (-420.0, 420.0):
        yew(*f.w(sx, -170.0), s=0.9)
    clear(gx, gy, 300.0)
    poi("Lychgate", gx, gy, "roofed timber gate over the church lane with a swaying candle chandelier, pall-bearers' "
        "benches and low stone churchyard walls")
    return f


def statue():
    """Hooded saint on a plinth on the forecourt lawn, facing the lane."""
    x, y = -2300.0, -3450.0
    yaw = facing(x, y, -1800.0, -3560.0)
    f = Fr(x, y, yaw)
    gb = gmin(x, y, 90.0) - 10.0
    put(V + "Corner_ExteriorWide_Brick", x + 8.0, y - 8.0, gb, yaw=yaw, scale=(1.95, 1.95, (g(x, y) + 30.0 - gb) / 322.0),
        sub="Statue")
    top = g(x, y) + 30.0 + 128.0
    put(V + "Corner_ExteriorWide_Brick", x + 6.0, y - 6.0, g(x, y) + 30.0, yaw=yaw + 180.0, scale=(1.35, 1.35, 0.4),
        sub="Statue")
    a = put(P + "Dummy", x, y, top - 42.0, yaw=yaw - 90.0, scale=1.15, sub="Statue")
    override(a, MAT_TRIM)
    C.claim(x, y, 120.0)
    # Votive candles, flowers, a laid wreath of petals at the foot.
    for k in range(9):
        a_ = math.radians(R.uniform(-80, 80) - 90.0 + yaw)
        d = R.uniform(95, 120)
        cx_, cy_ = x + math.cos(a_) * d, y + math.sin(a_) * d
        inst(P + R.choice(["Candle_1", "Candle_2"]), cx_, cy_, g(cx_, cy_), R.uniform(0, 360), R.uniform(1.2, 1.8),
             cull=3500.0)
    for k in range(6):
        lx, ly = R.uniform(-90, 90), R.uniform(-115, -95)
        f.inst(N + R.choice(["Flower_3_Single", "Flower_4_Single"]), lx, ly, None, R.uniform(0, 360),
               scale=R.uniform(0.2, 0.3), cull=5000.0)
    f.inst(N + "Petal_5", 0.0, -118.0, None, 0.0, scale=0.6, cull=5000.0)
    C.light(*f.w(0.0, -140.0), g(*f.w(0.0, -140.0)) + 45.0, intensity=4.0, radius=500.0, color=(255, 150, 80))
    # Dark hedge crescent + yews behind, benches either side facing the lane.
    for k in range(5):
        a_ = math.radians(40.0 + k * 25.0)
        hx, hy = f.w(math.cos(a_) * -260.0 * (1 if k < 5 else 1), math.sin(a_) * 230.0)
        hedge(hx, hy, yaw + (k - 2) * 25.0, sx=1.0, sy=0.7, sz=0.7)
    yew(*f.w(-340.0, 120.0), 1.0)
    yew(*f.w(340.0, 120.0), 1.0)
    for sx in (-265.0, 265.0):
        bx_, by_ = f.w(sx, -20.0)
        bench(bx_, by_, yaw - 90.0 + (12.0 if sx < 0 else -12.0))
    clear(x, y, 320.0)
    poi("Statue of the hooded saint", x, y, "stone saint on a two-tier plinth with votive candles, flowers, "
        "a dark hedge crescent, yews and two benches")


def encase_old(stools):
    """The builder's graveyard markers are stool-shaped platforms: wrap each in a proper stone."""
    for k, (x, y, yaw) in enumerate(stools):
        roll = R.random()
        if roll < 0.22:
            stone("tomb", x, y, yaw, s=1.05, sink=6.0, z=gmin(x, y, 60.0) - 6.0, flowers=0.5, candles=0.4, claim=False)
        else:
            a = put(V + "Wall_UnevenBrick_Straight", *Fr(x, y, yaw).w(0.0, 4.0), gmin(x, y, 40.0) - 6.0, yaw=yaw,
                    scale=(0.38, 0.72, 0.29 * R.uniform(1.0, 1.15)), sub="Graves", cull=9000.0)
            override(a, MAT_TRIM)
            if R.random() < 0.35:
                stone("ledger", x, y, yaw, claim=False)
            elif R.random() < 0.4:
                offering(x, y, yaw, "vase")
            if R.random() < 0.55:
                offering(x, y, yaw, "flowers")
            if R.random() < 0.35:
                offering(x, y, yaw, "candles")


def graveyard(stools):
    """The tended graveyard on the plateau: encased old markers, new rows, stepping-stone aisle, lamp, sounds."""
    clear(-3800.0, -4000.0, 720.0)
    clear(-3800.0, -4450.0, 450.0)
    encase_old(stools)
    kinds = ["tablet", "tablet2", "slab", "arch", "obelisk", "xstone", "tablet", "slab"]
    # South row between the grid and the fence (the TendGraves chore aisle stays open at x=-3680).
    for x in (-4160.0, -3930.0, -3450.0, -3230.0):
        y = -3570.0 + R.uniform(-15, 15)
        if ok(x, y, 45.0):
            stone(R.choice(kinds), x, y, R.uniform(-5, 5), side=R.uniform(-3, 3), flowers=0.6, candles=0.4)
    # North row flanking the crypt forecourt.
    for x in (-4160.0, -3480.0, -3280.0):
        y = -4500.0 + R.uniform(-15, 15)
        if ok(x, y, 45.0):
            stone(R.choice(["tomb", "arch", "slab", "obelisk"]), x, y, R.uniform(-5, 5), flowers=0.6, candles=0.4)
    lamp_post(-3440.0, -3690.0, 90.0, intensity=9.0, radius=900.0)
    # Small offerings on the TendGraves aisle: a watering bucket, a basket of flowers.
    inst(P + "Bucket_Wooden_1", -3560.0, -3800.0, g(-3560.0, -3800.0), 30.0, 1.0, cull=6000.0)
    C.ambient("A_Crickets_Loop", -3800.0, -4000.0, g(-3800.0, -4000.0) + 150.0, volume=0.55)
    # West edge hedge on the lip of the plateau.
    for k in range(5):
        y = -4380.0 + k * 190.0
        x = -4345.0 + R.uniform(-15, 15)
        if ok(x, y, 70.0, allow_reserved=True):
            hedge(x, y, 90.0 + R.uniform(-8, 8))
    poi("Graveyard", -3800.0, -4000.0, "tended graveyard: old markers re-cut as stone slabs and chest tombs, new rows, "
        "flowers, candles, stepping-stone aisle, lamp post")


def old_cluster(cx, cy, r, n, name, kinds, tilt=12.0, flowers=0.15, candles=0.05, grass=False, desc=""):
    """Tilted old markers scattered in loose rows inside a circle."""
    placed = 0
    row_yaw = R.uniform(-10, 10) + 180.0
    for _ in range(n * 25):
        if placed >= n:
            break
        x, y = cx + R.uniform(-r, r), cy + R.uniform(-r, r)
        if math.hypot(x - cx, y - cy) > r or not ok(x, y, 55.0) or C.slope(x, y, 60.0) > 60.0:
            continue
        k = R.choice(kinds)
        stone(k, x, y, row_yaw + R.uniform(-18, 18), fb=R.uniform(-tilt, tilt), side=R.uniform(-tilt, tilt) * 0.7,
              sink=R.uniform(4, 14), s=R.uniform(0.85, 1.1), flowers=flowers, candles=candles)
        C.claim(x, y, 70.0)
        if R.random() < 0.35:
            for j in range(R.randint(1, 3)):
                fx, fy = x + R.uniform(-60, 60), y + R.uniform(-60, 60)
                inst(N + R.choice(["Fern_1", "Grass_Wispy_Tall", "Clover_2", "Plant_7"]), fx, fy, g(fx, fy) - 4.0,
                     R.uniform(0, 360), R.uniform(0.35, 0.6), cull=5000.0)
        placed += 1
    if not grass:
        clear(cx, cy, r * 0.6)
    poi(name, cx, cy, desc)
    return placed


def family_plot():
    """The Morrow family plot: iron railings, stone corner posts, obelisk and arched stones, candles."""
    x, y = -2950.0, -3640.0
    f = Fr(x, y, 0.0)
    hw, hd = 220.0, 160.0
    clear(x, y, 280.0)

    def rail(lx, ly, lyaw):
        wx, wy = f.w(lx, ly)
        put(V + "Prop_MetalFence_Ornament", wx, wy, g(wx, wy) - 4.0, yaw=lyaw, scale=(0.56, 1.0, 0.34), sub="FamilyPlot",
            cull=12000.0)
        C.claim(wx, wy, 45.0)

    for lx in (-165.0, 165.0):                 # north side with the gate gap in the middle
        rail(lx, -hd, 0.0)
    for lx in (-165.0, -55.0, 55.0, 165.0):
        rail(lx, hd, 0.0)
    for ly in (-107.0, 0.0, 107.0):
        rail(-hw, ly, 90.0)
        rail(hw, ly, 90.0)
    for px, py in ((-hw, -hd), (hw, -hd), (-hw, hd), (hw, hd), (-112.0, -hd), (112.0, -hd)):
        wx, wy = f.w(px, py)
        put(V + "Corner_Exterior_Brick", wx + 4.0, wy - 3.0, g(wx, wy) - 5.0, scale=(0.5, 0.5, 0.36), sub="FamilyPlot")
        put(P + "Vase_4", wx, wy, g(wx, wy) - 5.0 + 112.0, yaw=R.uniform(0, 90), scale=0.5, sub="FamilyPlot",
            collide=False)
    # Graves: obelisk in the middle, two arched stones, a little cross, ledgers; all face the gate (north).
    ox, oy = f.w(0.0, 95.0)
    stone("obelisk", ox, oy, 0.0, s=1.25, sink=5.0, candles=1.0, flowers=1.0)
    for lx in (-135.0, 135.0):
        sx, sy = f.w(lx, 100.0)
        stone("arch", sx, sy, 0.0, s=1.0, flowers=1.0)
        stone("ledger", sx, sy, 0.0)
    cx_, cy_ = f.w(-150.0, -60.0)
    stone("xstone", cx_, cy_, 10.0, s=0.7, flowers=1.0)
    offering(*f.w(60.0, 60.0), 0.0, "vase")
    for k in range(8):
        lx, ly = R.uniform(-190, 190), R.uniform(-130, 130)
        f.inst(N + R.choice(["Petal_1", "Petal_3", "Clover_1", "Flower_3_Group"]), lx, ly, None, R.uniform(0, 360),
               scale=R.uniform(0.25, 0.45), cull=5000.0)
    C.light(*f.w(0.0, 40.0), g(*f.w(0.0, 40.0)) + 60.0, intensity=4.0, radius=550.0, color=(255, 160, 90))
    poi("Morrow family plot", x, y, "iron-railed family plot with a candle-lit obelisk, arched stones, ledgers, urns")


def crypt():
    """Stone mausoleum at the head of the graveyard, facing south; its door apron stays empty (catacomb entrance)."""
    x, y, fl = -3800.0, -4960.0, 605.0
    f = Fr(x, y, 180.0, z=fl)
    sub = "Crypt"
    # Plinth skirt (stone brick all round), floor, walls, corner piers, gables, roof.
    for side, n_off in ((0.0, 0), (180.0, 0), (-90.0, 0), (90.0, 0)):
        for off in (-100.0, 100.0):
            if side == 0.0:
                lx, ly, lyaw = off, -222.0, 0.0
            elif side == 180.0:
                lx, ly, lyaw = -off, 222.0, 180.0
            elif side == -90.0:
                lx, ly, lyaw = -222.0, -off, -90.0
            else:
                lx, ly, lyaw = 222.0, off, 90.0
            wx, wy = f.w(lx, ly)
            bot = g(wx, wy) - 30.0
            a = put(V + "Wall_UnevenBrick_Straight", wx, wy, bot, yaw=f.yaw + lyaw,
                    scale=(1.12, 0.9, max(0.1, (fl + 6.0 - bot) / 312.0)), sub=sub)
            override(a, MAT_TRIM)
    for i in (-100.0, 100.0):
        for j in (-100.0, 100.0):
            f.put(V + "Floor_Brick", i, j, 6.0, sub=sub)
    f.put(V + "Wall_UnevenBrick_Door_Round", 0.0, -200.0, 6.0, 0.0, sub=sub)
    for lx in (-150.0, 150.0):
        f.put(V + "Wall_UnevenBrick_Straight", lx, -200.0, 6.0, 0.0, scale=(0.5, 1.0, 1.0), sub=sub)
    for off in (-100.0, 100.0):
        f.put(V + "Wall_UnevenBrick_Straight", -off, 200.0, 6.0, 180.0, sub=sub)
        f.put(V + "Wall_UnevenBrick_Window_Thin_Round", -200.0, -off, 6.0, -90.0, sub=sub)
        f.put(V + "Wall_UnevenBrick_Window_Thin_Round", 200.0, off, 6.0, 90.0, sub=sub)
    for sx in (-1, 1):
        for sy in (-1, 1):
            f.put(V + "Corner_ExteriorWide_Brick", sx * 200.0, sy * 200.0, 6.0, 0.0, sub=sub)
    f.put(V + "Roof_RoundTiles_4x4", 0.0, 0.0, 306.0, sub=sub)
    for ly, lyaw in ((-200.0, 0.0), (200.0, 180.0)):
        a = f.put(V + "Roof_Front_Brick4", 0.0, ly, 306.0, lyaw, sub=sub)
        override(a, MAT_UBRICK, slots=(1,))
    # Iron gate in the doorway, steps down to the apron, piers with urns, ivy.
    f.put(V + "Prop_MetalFence_Ornament", 0.0, -212.0, 8.0, 0.0, scale=(0.6, 1.0, 0.72), sub=sub)
    f.put(V + "Stairs_Exterior_Straight", 0.0, -300.0, None, 0.0, scale=(1.15, 0.45, 0.2), sub=sub)
    for lx in (-165.0, 165.0):
        f.put(V + "Prop_Vine1" if lx < 0 else V + "Prop_Vine2", lx, -236.0, 290.0, 0.0, sub=sub, collide=False)
    f.put(V + "Prop_Vine6", 0.0, 238.0, 300.0, 180.0, sub=sub, collide=False)
    # Inside: a sarcophagus, candles, the eerie glow that leaks through the gate.
    override(f.put(V + "Wall_UnevenBrick_Straight", -20.0, 60.0, 6.0, 90.0, scale=(0.95, 2.0, 0.26), sub=sub), MAT_TRIM)
    for lx in (-60.0, 0.0, 60.0):
        f.inst(P + R.choice(["Candle_1", "Candle_2"]), lx + R.uniform(-8, 8), 60.0 + R.uniform(-20, 20),
               6.0 + 81.0, R.uniform(0, 360), scale=1.5, cull=3500.0)
    f.put(P + "CandleStick_Stand", -140.0, 140.0, 6.0, 0.0, sub=sub)
    f.put(P + "CandleStick_Stand", 140.0, 140.0, 6.0, 0.0, sub=sub)
    lx_, ly_ = f.w(0.0, 20.0)
    C.light(lx_, ly_, fl + 170.0, intensity=12.0, radius=800.0, color=(110, 255, 190))
    for i in range(12):
        C.claim(*f.w(R.uniform(-200, 200), R.uniform(-200, 200)), 60.0)
    for ly in (-300.0, -380.0, -460.0, -540.0):   # the apron: nothing may be put here
        C.claim(*f.w(0.0, ly), 150.0)
    # Forecourt: yews at the corners, stone piers with urns flanking the approach, candles on the steps.
    yew(*f.w(-300.0, -140.0), 1.05)
    yew(*f.w(300.0, -140.0), 1.05)
    for lx in (-175.0, 175.0):
        wx, wy = f.w(lx, -390.0)
        put(V + "Corner_ExteriorWide_Brick", wx, wy, g(wx, wy) - 8.0, scale=(0.7, 0.7, 0.4), sub=sub)
        put(P + "Vase_2", wx - 3.0, wy + 3.0, g(wx, wy) - 8.0 + 128.0, yaw=R.uniform(0, 90), scale=0.5, sub=sub)
        C.claim(wx, wy, 50.0)
    for k in range(7):
        lx = R.uniform(-110, 110)
        f.inst(P + R.choice(["Candle_1", "Candle_2"]), lx, -262.0 + R.uniform(-15, 5), None, R.uniform(0, 360),
               scale=R.uniform(1.2, 1.8), cull=3500.0)
    for side in (-1, 1):
        for k in range(3):
            f.inst(N + R.choice(["Fern_1", "Plant_1"]), side * R.uniform(230, 280), R.uniform(-180, 180), None,
                   R.uniform(0, 360), scale=R.uniform(0.4, 0.6), cull=5000.0)
    clear(x, y + 150.0, 450.0)
    poi("Crypt of the Morrows", x, y, "stone mausoleum with an iron gate, sarcophagus and green candle glow inside; "
        "yews and urn piers; the door apron is kept empty for the future catacomb entrance")


def shed():
    """Gravedigger's open shed: plank walls, tile roof, tools, coffins, loot chest, a swinging lantern."""
    x, y = -3300.0, -2860.0
    f = Fr(x, y, 0.0)
    sub = "Shed"
    posts = [(sx * 190.0, sy * 140.0) for sx in (-1, 1) for sy in (-1, 1)]
    gz = max(g(*f.w(px, py)) for px, py in posts)
    f.z = gz
    for px, py in posts:
        wx, wy = f.w(px, py)
        base = g(wx, wy) - 6.0
        put(V + "Corner_Exterior_Wood", wx, wy, base, scale=(1.3, 1.3, (gz + 235.0 - base) / 300.0), sub=sub)
    f.put(V + "Roof_RoundTiles_4x4", 0.0, 0.0, 232.0, scale=(0.82, 0.62, 0.36), sub=sub)
    f.put(V + "Roof_Front_Brick4", 0.0, 150.0, 232.0, 180.0, scale=(0.82, 0.62, 0.36), sub=sub)
    for lx in (-100.0, 100.0):                   # plank back wall, 3 boards high
        for k in range(3):
            f.put(V + "Prop_WoodenFence_Single", lx, 150.0, -4.0 + k * 78.0, 0.0, sub=sub)
    for sx in (-1, 1):                           # half side walls at the back
        for k in range(3):
            f.put(V + "Prop_WoodenFence_Extension1", sx * 196.0, 88.0, -4.0 + k * 78.0, 90.0, scale=(0.55, 1.0, 1.0),
                  sub=sub)
    for i in (-100.0, 100.0):
        for j in (-70.0, 70.0):
            f.put(V + "Floor_WoodDark", i, j, -3.0, scale=(1.0, 0.72, 1.0), sub=sub, collide=False)
    # Inside.
    f.put(P + "Workbench", -60.0, 95.0, 2.0, 180.0, sub=sub)
    f.put(P + "Peg_Rack", -60.0, 140.0, 150.0, 180.0, sub=sub, collide=False)
    f.put(P + "Axe_Bronze", -95.0, 128.0, 128.0, 180.0, sub=sub, collide=False)
    f.put(P + "Pickaxe_Bronze", -25.0, 128.0, 130.0, 180.0, sub=sub, collide=False)
    for k, ly in enumerate((-10.0, -40.0)):      # shovels leaning on the east side wall
        f.put(WP + "SM_KG_Shovel", 176.0, ly + 60.0, 128.0, 180.0 + k * 8.0, pitch=-72.0, sub=sub, collide=False)
    f.put(P + "Bottle_1", -120.0, 90.0, 91.0, 0.0, sub=sub, collide=False, cull=4000.0)
    f.put(P + "Mug", -20.0, 85.0, 91.0, 0.0, sub=sub, collide=False, cull=4000.0)
    f.put(P + "CandleStick", 10.0, 100.0, 91.0, 0.0, sub=sub, collide=False, cull=4000.0)
    f.put(P + "Book_Stack_1", -140.0, 100.0, 91.0, 20.0, sub=sub, collide=False, cull=4000.0)
    f.put(P + "Stool", 0.0, 10.0, 2.0, 15.0, sub=sub)
    f.put(P + "Rope_2", -150.0, -60.0, 2.0, 0.0, sub=sub, collide=False, cull=5000.0)
    f.put(P + "Bucket_Metal", 90.0, 0.0, 2.0, 0.0, sub=sub, collide=False)
    lx, ly = f.w(120.0, 105.0)
    C.loot_chest(lx, ly, gz + 2.0, yaw=f.yaw + 180.0, table="Chest", name="Gravedigger's chest")
    f.put(P + "Lantern_Wall", -190.0, -140.0, 150.0, 180.0, sub=sub, collide=False)
    lx, ly = f.w(-190.0, -250.0)
    C.light(lx, ly, gz + 205.0, intensity=8.0, radius=800.0)
    for bx_, by_ in ((-150.0, -190.0), (-80.0, -215.0)):
        smash(P + "Barrel", *f.w(bx_, by_))
    smash(V + "Prop_Crate", *f.w(260.0, 100.0))
    smash(P + "Crate_Wooden", *f.w(265.0, -20.0))
    # Coffins: a stack of three by the west wall, one standing, one on the hearse wagon.
    for k in range(3):
        f.put(V + "Prop_Crate", -285.0, 40.0, -2.0 + k * 43.0, R.uniform(-4, 4), scale=(0.55, 1.6, 0.4), sub=sub)
    f.put(V + "Prop_Crate", -250.0, -120.0, 95.0, 10.0, roll=-78.0, scale=(0.55, 1.6, 0.4), sub=sub)
    f.put("/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_Firewood", 280.0, -150.0, None, 40.0, sub=sub)
    C.claim(x, y, 300.0)
    wx, wy = -3780.0, -2480.0
    if ok(wx, wy, 150.0, allow_lane=True):
        put(V + "Prop_Wagon", wx, wy, g(wx, wy) - 2.0, yaw=205.0, sub=sub, claim=210.0)
        put(V + "Prop_Crate", wx - 49.0, wy + 104.0, g(wx, wy) + 75.0, yaw=205.0, scale=(0.55, 1.6, 0.4), sub=sub)
    clear(x, y, 330.0)
    poi("Gravedigger's shed", x, y, "open plank shed with workbench, tool rack, shovels, stacked coffins, barrels "
        "to smash, a swinging lantern and the gravedigger's loot chest; hearse wagon nearby")


def fresh_grave():
    """Half-dug grave: hole, spoil heap with a shovel in it, coffin waiting, lantern pole, buried iron chest."""
    x, y = -2760.0, -2790.0
    f = Fr(x, y, 8.0)
    sub = "FreshGrave"
    gz = g(x, y)
    f.put(WP + "SM_KG_DugHole", 0.0, 0.0, -2.0, 0.0, scale=(0.95, 1.75, 1.0), sub=sub, collide=False)
    f.put(WP + "SM_KG_DigMound", 125.0, 10.0, -6.0, 0.0, scale=(1.0, 1.7, 1.5), sub=sub, collide=False)
    f.put(WP + "SM_KG_DigMound", 150.0, -80.0, -10.0, 30.0, scale=(0.7, 0.7, 1.1), sub=sub, collide=False)
    f.put(WP + "SM_KG_Shovel", 118.0, -10.0, 118.0, 90.0, pitch=-62.0, sub=sub, collide=False)
    f.put(P + "Pickaxe_Bronze", 175.0, 60.0, 8.0, 20.0, roll=88.0, sub=sub, collide=False)
    f.put(P + "Rope_3", -60.0, 150.0, 1.0, 0.0, sub=sub, collide=False)
    f.put(P + "Bucket_Wooden_1", -95.0, -70.0, 0.0, 0.0, sub=sub, collide=False)
    # Coffin waiting on the grass beside the hole, lid leaning on it.
    f.put(V + "Prop_Crate", -120.0, -10.0, -2.0, 4.0, scale=(0.55, 1.6, 0.4), sub=sub)
    # Unpainted cross waiting to go up, leaning on the spoil heap.
    t = Tilt(*f.w(40.0, -150.0), gz - 4.0, f.yaw, fb=-14.0, side=5.0)
    t.piece(V + "Corner_Exterior_Wood", 0.0, 0.0, 0.0, t.ex, t.up, (0.5, 0.5, 0.43))
    t.piece(V + "Corner_Exterior_Wood", -30.0, 0.0, 88.0, neg(t.up), t.ex, (0.45, 0.45, 0.2))
    # Lantern pole.
    px, py = f.w(-80.0, -150.0)
    put(V + "Corner_Exterior_Wood", px, py, g(px, py) - 20.0, scale=(0.8, 0.8, 0.62), sub=sub)
    put(P + "Lantern_Wall", px, py, g(px, py) + 60.0, yaw=f.yaw + 90.0, sub=sub, collide=False)
    C.light(px - 90.0, py, g(px, py) + 120.0, intensity=7.0, radius=700.0)
    # What the spade struck: an iron-bound chest in the bottom of the hole.
    cx_, cy_ = f.w(0.0, 15.0)
    ch = C.loot_chest(cx_, cy_, gz - 26.0, yaw=f.yaw + 90.0, table="Grave", name="Buried chest", path=CHEST_IRON)
    try:
        ch.set_editor_property("mesh_scale", 0.9)
    except Exception:
        pass
    C.claim(x, y, 260.0)
    clear(x, y, 260.0)
    poi("Fresh-dug grave", x, y, "half-dug grave with spoil heap, shovel, pickaxe, waiting coffin and cross, lantern "
        "pole; an iron chest (Grave loot) lies in the hole: the digging teaser")


def paupers_field():
    """Loose rows of wooden crosses, mounds and fieldstones below the graveyard bank."""
    n = 0
    for row, y0 in enumerate((-3000.0, -2830.0, -2660.0, -2500.0)):
        x = -4350.0 + R.uniform(0, 80)
        while x < -3560.0:
            y = y0 + R.uniform(-25, 25)
            if ok(x, y, 60.0) and C.slope(x, y, 60.0) < 50.0:
                k = R.choices(["xwood", "xwood", "mound", "cairn", "rock", "broken"], k=1)[0]
                yaw = R.uniform(-10, 10)
                stone(k, x, y, yaw, fb=R.uniform(-9, 9), side=R.uniform(-7, 7), s=R.uniform(0.85, 1.05),
                      flowers=0.25, candles=0.05)
                if k in ("xwood", "cairn", "rock") and R.random() < 0.5:
                    stone("mound", x, y, yaw, claim=False)
                C.claim(x, y, 75.0)
                n += 1
            x += R.uniform(140, 190)
    poi("Paupers' field", -3950.0, -2750.0, f"{n} wooden crosses, mounds and fieldstones in loose rows below the bank")


def forgotten_graves():
    """West hollow below the plateau: overgrown, tilted and broken stones, a robbed grave, a pale wisp."""
    n = old_cluster(-4720.0, -4650.0, 380.0, 16, "Forgotten graves",
                    ["tablet", "tablet2", "broken", "cairn", "rock", "xwood", "xstone", "slab"], tilt=16.0,
                    flowers=0.05, candles=0.0, grass=True,
                    desc="overgrown hollow of leaning, broken stones under a twisted tree; a robbed grave and a "
                         "pale wisp light")
    tx, ty = -4560.0, -4250.0
    if ok(tx, ty, 150.0):
        put(N + "TwistedTree_3", tx, ty, gmin(tx, ty, 80.0) - 20.0, yaw=200.0, scale=0.42, sub="Trees", claim=160.0)
    # The robbed grave: open hole, stone knocked over, a shovel dropped, the chest still inside.
    rx, ry = -4800.0, -4420.0
    if ok(rx, ry, 120.0):
        f = Fr(rx, ry, 95.0)
        f.put(WP + "SM_KG_DugHole", 0.0, 0.0, -4.0, 0.0, scale=(0.95, 1.7, 1.0), sub="Forgotten", collide=False)
        f.put(WP + "SM_KG_DigMound", -120.0, 20.0, -8.0, 0.0, scale=(0.9, 1.5, 1.2), sub="Forgotten", collide=False)
        f.put(WP + "SM_KG_Shovel", 90.0, 60.0, 4.0, 150.0, sub="Forgotten", collide=False)
        stone("tablet", *f.w(0.0, 130.0), f.yaw, fb=70.0, sink=2.0, claim=False)
        cx_, cy_ = f.w(0.0, 5.0)
        C.loot_chest(cx_, cy_, g(rx, ry) - 24.0, yaw=f.yaw, table="Grave", name="Robbed grave", path=CHEST_IRON)
        C.claim(rx, ry, 170.0)
        poi("Robbed grave", rx, ry, "open grave with a dropped shovel and a toppled stone; the grave robbers left a chest")
    C.light(-4700.0, -4750.0, g(-4700.0, -4750.0) + 90.0, intensity=5.0, radius=750.0, color=(150, 200, 255))
    return n


def hill():
    """Old hill graves behind the church and the drowned sailors' memorial on the hilltop."""
    old_cluster(-2850.0, -5750.0, 420.0, 14, "Old hill graves",
                ["tablet", "tablet2", "xstone", "cairn", "xwood", "broken", "slab", "arch"], tilt=11.0,
                flowers=0.2, candles=0.1, grass=True,
                desc="leaning old stones and crosses on the slope behind the church")
    mx, my = -2720.0, -6080.0
    f = Fr(mx, my, facing(mx, my, -2720.0, -4000.0))
    gb = gmin(mx, my, 100.0) - 10.0
    put(V + "Corner_ExteriorWide_Brick", mx + 9.0, my - 9.0, gb, yaw=f.yaw, scale=(1.9, 1.9, (g(mx, my) + 25.0 - gb) / 322.0),
        sub="Memorial")
    put(V + "Corner_ExteriorWide_Brick", mx + 5.0, my - 5.0, g(mx, my) + 25.0, yaw=f.yaw, scale=(0.95, 0.95, 1.0),
        sub="Memorial")
    put(P + "Vase_2", mx, my, g(mx, my) + 25.0 + 322.0, yaw=0.0, scale=0.7, sub="Memorial")
    C.claim(mx, my, 130.0)
    for lx, lyaw in ((-40.0, 20.0), (40.0, -20.0)):     # crossed oars leaning on the plinth
        f.put(WP + "SM_KG_Oar", lx, -95.0, 5.0, 90.0 + lyaw, pitch=58.0, sub="Memorial", collide=False)
    f.put(P + "Rope_3", 0.0, -120.0, None, 0.0, sub="Memorial", collide=False)
    for k in range(10):
        lx, ly = R.uniform(-110, 110), R.uniform(-130, -100)
        f.inst(P + R.choice(["Candle_1", "Candle_2"]) if k % 2 else N + R.choice(["Flower_3_Single", "Flower_4_Single"]),
               lx, ly, None, R.uniform(0, 360), scale=R.uniform(1.2, 1.7) if k % 2 else R.uniform(0.2, 0.3), cull=4000.0)
    C.light(*f.w(0.0, -150.0), g(*f.w(0.0, -150.0)) + 50.0, intensity=5.0, radius=600.0, color=(255, 160, 90))
    bx_, by_ = f.w(0.0, -330.0)
    bench(bx_, by_, f.yaw - 90.0)
    C.ambient("A_Wind_Loop", mx, my, g(mx, my) + 200.0, volume=0.6)
    clear(mx, my - 150.0 * 0, 260.0)
    poi("Drowned sailors' memorial", mx, my, "hilltop obelisk with an urn, crossed oars, rope, candles and flowers; a "
        "bench looks over the church roof to the sea")


def bell_tower():
    """Bell-ringer's rest around the tower: bench with a view, lamp, barrels and crates to smash, rope."""
    x0, y0 = -1900.0, -5200.0
    bench(-1900.0, -4840.0, 90.0)
    inst(P + "Mug", -1780.0, -4830.0, g(-1780.0, -4830.0) + 1.0, 0.0, 1.0, cull=3500.0)
    lamp_post(-1650.0, -4880.0, 90.0, intensity=8.0, radius=850.0)
    for bx_, by_ in ((-1630.0, -5020.0), (-1625.0, -5110.0)):
        smash(P + "Barrel", bx_, by_)
    smash(P + "Crate_Wooden", -1630.0, -5240.0)
    put(P + "Rope_3", -1700.0, -5480.0, g(-1700.0, -5480.0) + 1.0, yaw=30.0, sub="Tower", collide=False, cull=5000.0)
    put(P + "Bucket_Wooden_1", -1640.0, -5350.0, None, sub="Tower", collide=False, cull=5000.0)
    put(P + "Barrel_Holder", -1580.0, -5430.0, None, yaw=90.0, sub="Tower")
    C.claim(-1580.0, -5430.0, 80.0)
    for k in range(6):
        a = R.uniform(0, 2 * math.pi)
        fx, fy = -1900.0 + math.cos(a) * 270.0, -4960.0 + R.uniform(-30, 30)
        inst(N + R.choice(["Flower_4_Group", "Flower_3_Group"]), fx + R.uniform(-150, 150), fy, g(fx, fy) - 3.0,
             R.uniform(0, 360), R.uniform(0.3, 0.45), cull=5000.0)
    clear(-1900.0, -4880.0, 250.0)
    poi("Bell-ringer's rest", -1900.0, -4840.0, "bench under the tower looking south, lamp post, barrels and crates to "
        "smash, rope and barrel rack; stepping-stone path from the church door")


def church_surrounds():
    """Flowers and leaning slabs along the church walls, planters and candles by the door, a stonemason's corner."""
    ch = [b for b in C.buildings() if b.name == "church"][0]
    f = Fr(ch.x, ch.y, ch.yaw)
    for side in (1, -1):                                  # flower beds along both long walls
        ly = -520.0
        while ly < 540.0:
            f.inst(N + R.choice(["Flower_3_Group", "Flower_4_Group", "Petal_2", "Clover_1", "Plant_7", "Petal_5"]),
                   side * R.uniform(440, 480), ly, None, R.uniform(0, 360), scale=R.uniform(0.3, 0.5), cull=5000.0)
            ly += R.uniform(55, 85)
    for ly in (-330.0, -40.0, 230.0, 480.0):              # old slabs leaning on the west wall
        x, y = f.w(468.0, ly)
        stone(R.choice(["tablet", "tablet2", "slab"]), x, y, f.yaw + 90.0, fb=8.0, sink=6.0, flowers=0.5, candles=0.3)
    for lx in (-200.0, 340.0):                            # planters by the door
        x, y = f.w(lx, -660.0)
        put(P + "Vase_2", x, y, g(x, y) - 2.0, yaw=R.uniform(0, 90), scale=0.75, sub="Church")
        inst(N + "Flower_3_Group", x, y, g(x, y) + 22.0, R.uniform(0, 360), 0.26, cull=5000.0)
        C.claim(x, y, 50.0)
    for k in range(10):                                   # candles along the front wall
        lx = R.choice([R.uniform(-330, -120), R.uniform(250, 380)])
        f.inst(P + R.choice(["Candle_1", "Candle_2"]), lx, -625.0 + R.uniform(-10, 10), None, R.uniform(0, 360),
               scale=R.uniform(1.2, 1.8), cull=3500.0)
    # Stonemason's corner behind the church: rubble, bricks, blank stones waiting to be cut.
    sx, sy = f.w(250.0, 760.0)
    if ok(sx, sy, 90.0, allow_reserved=True):
        fs = Fr(sx, sy, f.yaw + 180.0)
        fs.put(P + "Vase_Rubble_Medium", 0.0, 0.0, None, 20.0, sub="Church", collide=False)
        for k in range(6):
            fs.inst(V + R.choice(["Prop_Brick1", "Prop_Brick2", "Prop_Brick3", "Prop_Brick4"]), R.uniform(-80, 80),
                    R.uniform(-40, 40), None, R.uniform(0, 360), scale=1.0, cull=4000.0)
        for k, lx in enumerate((-90.0, -60.0, -30.0)):
            stone("tablet", *fs.w(lx + 150.0, 30.0), fs.yaw, fb=-18.0 - k * 4.0, sink=2.0, claim=False)
        fs.put(P + "Pickaxe_Bronze", 60.0, -40.0, 3.0, 40.0, roll=85.0, sub="Church", collide=False)
        fs.put(P + "Bucket_Wooden_1", -100.0, -30.0, None, 0.0, sub="Church", collide=False)
        C.claim(sx, sy, 150.0)
    # Hedges at the front corners, a bench facing the door path.
    for lx in (-560.0, 560.0):
        x, y = f.w(lx, -540.0)
        if ok(x, y, 70.0, allow_reserved=True):
            hedge(x, y, f.yaw + 90.0)
    clear(*f.w(0.0, -760.0), 260.0)
    poi("Church door", *f.w(100.0, -760.0), "planters, candles along the wall, flower beds and "
        "leaning old slabs along the walls; stonemason's corner at the back")


def calvary():
    """Wayside cross in the east hollow, outside the wall: votive candles, benches, unconsecrated graves."""
    x, y = -950.0, -4300.0
    if not ok(x, y, 150.0):
        return
    f = Fr(x, y, facing(x, y, -1600.0, -3400.0))
    gb = gmin(x, y, 100.0) - 10.0
    put(V + "Corner_ExteriorWide_Brick", x + 10.0, y - 10.0, gb, yaw=f.yaw, scale=(2.2, 2.2, (g(x, y) + 20.0 - gb) / 322.0),
        sub="Calvary")
    put(V + "Corner_ExteriorWide_Brick", x + 7.0, y - 7.0, g(x, y) + 20.0, yaw=f.yaw, scale=(1.5, 1.5, 0.12),
        sub="Calvary")
    t = Tilt(x, y, g(x, y) + 58.0, f.yaw)
    put(V + "Corner_Exterior_Wood", x, y, g(x, y) + 40.0, yaw=f.yaw, scale=(1.5, 1.5, 1.1), sub="Calvary")
    ox, oy = f.w(-110.0, 0.0)
    put(V + "Corner_Exterior_Wood", ox, oy, g(x, y) + 40.0 + 250.0, yaw=f.yaw, pitch=-90.0, scale=(1.3, 1.3, 0.74),
        sub="Calvary")
    put(P + "Banner_2_Cloth", *f.w(0.0, -18.0), g(x, y) + 40.0 + 262.0, yaw=f.yaw, scale=(0.35, 1.0, 0.45), sub="Calvary",
        collide=False)
    C.claim(x, y, 170.0)
    for k in range(12):
        lx, ly = R.uniform(-120, 120), R.uniform(-150, -120)
        f.inst(P + R.choice(["Candle_1", "Candle_2"]) if k % 3 else N + "Flower_3_Single", lx, ly, None,
               R.uniform(0, 360), scale=R.uniform(1.2, 1.8) if k % 3 else 0.22, cull=4000.0)
    for lx in (-230.0, 230.0):
        bx_, by_ = f.w(lx, -330.0)
        bench(bx_, by_, f.yaw + 90.0 + (-15.0 if lx < 0 else 15.0))
    for k in range(6):
        a = math.radians(-30.0 + k * 50.0)
        hx, hy = f.w(math.cos(a) * 330.0, math.sin(a) * 260.0 + 60.0)
        if ok(hx, hy, 70.0):
            hedge(hx, hy, R.uniform(0, 360), sx=0.9, sy=0.8)
    # Unconsecrated graves outside the wall: low mounds with rough stones.
    n = 0
    for _ in range(60):
        if n >= 6:
            break
        gx, gy = x + R.uniform(-600, 300), y + R.uniform(250, 700)
        if ok(gx, gy, 70.0) and C.slope(gx, gy, 60.0) < 50.0:
            stone(R.choice(["rock", "cairn", "xwood"]), gx, gy, R.uniform(-30, 30), fb=R.uniform(-10, 10),
                  side=R.uniform(-10, 10), flowers=0.3)
            stone("mound", gx, gy, 0.0, claim=False)
            C.claim(gx, gy, 90.0)
            n += 1
    clear(x, y, 300.0)
    poi("Wayside calvary", x, y, "tall wooden cross on a stepped stone base with a hanging cloth, votive candles, two "
        "benches, a hedge ring and unconsecrated graves outside the wall")


def dead_trees():
    for path, x, y, s in (("DeadTree_3", -3050.0, -5780.0, 0.48), ("DeadTree_1", -4600.0, -5050.0, 0.6),
                          ("DeadTree_4", -4250.0, -3260.0, 0.42), ("DeadTree_5", -1200.0, -4800.0, 0.36),
                          ("DeadTree_2", -3580.0, -5480.0, 0.5), ("DeadTree_1", -2350.0, -6150.0, 0.45)):
        if ok(x, y, 110.0):
            dead_tree(path, x, y, s)


def ground_cover():
    """Ferns, wispy grass and mushrooms in the shady corners; crimson shrubs as colour accents."""
    def pick():
        a = R.uniform(0, 2 * math.pi)
        d = 2350.0 * math.sqrt(R.random())
        return CX + math.cos(a) * d, CY + math.sin(a) * d
    C.scatter([N + "Fern_1", N + "Grass_Wispy_Tall", N + "Plant_1", N + "Grass_Wispy_Short"], 70, pick, 70.0,
              sub="Scatter", scale=(0.45, 0.8), cull=6000.0)
    C.scatter([N + "Bush_Common", N + "Bush_Common_Flowers"], 12, pick, 120.0, sub="Scatter", scale=(0.55, 0.8),
              cull=10000.0)
    C.scatter([N + "Mushroom_Common", N + "Pebble_Round_1", N + "Pebble_Round_3", N + "Pebble_Square_2"], 40, pick,
              35.0, sub="Scatter", scale=(0.6, 1.1), cull=4000.0)


# ============================================================================================ entry
def dress():
    stools = claim_existing()
    paths()
    lychgate()
    crypt()
    family_plot()
    statue()
    church_surrounds()
    graveyard(stools)
    shed()
    fresh_grave()
    bell_tower()
    hill()
    forgotten_graves()
    paupers_field()
    calvary()
    dead_trees()
    ground_cover()
    flush()
    C.stats["pois"] = len(POIS)
    C.stats["poi_list"] = [p["name"] for p in POIS]
