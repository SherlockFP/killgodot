"""Coast zone dressing: the shoreline outside the harbour.

EAST HEADLAND (x >= 45 m, y > 20 m)
  - Lighthouse keeper's yard on the pad around the tower (7000, 5700): vegetable garden with scarecrow, laundry
    line, lamp-oil lean-to, woodpile, outdoor table, signal mast, sea-view bench (seat), cliff-edge fence.
  - Eastern viewpoint deck on the ridge (8150, 5300) overlooking the cove and the wreck.
  - The wreck of the "Gannet": a big broken hull (ribs, keel, planking, tilted mast, foredeck) beached in the east
    cove (11100, 5950), spilled cargo, a salvager's stall, flotsam bobbing in the surf.
  - Maidens' Ring standing stones (10300, 3600), the ruined watch-house (9300, 3300), a stepping-stone trail from
    the lighthouse lane down to the cove, rope handrail on the steep lighthouse path.
  - Fisherman's corner on the little beach west of the headland (4900, 5300); tide pools in the east cove.
WEST SHORE (x <= -45 m, y > 30 m)
  - Net-menders' strand, beached boats, beach bonfire ring with bench seats, driftwood along the tide line,
    tide pools, the skeleton of a fishing smack, lookout cairn on the knoll, smuggler's cove with a jetty and a
    hidden chest behind the boulders.
SEA: sea stacks and waterline boulders on both sides, bell buoys that bob and rock.
"""
import math
import random

import unreal
import kg_dress_common as C

R = random.Random(20260924)
P, N, V, PIR, WP = C.P, C.N, C.V, C.PIR, C.WP
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/"
KIT = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/"
CHEST_WOOD = "/Game/KillGodot/Items/Storage/SM_KG_Chest_Wood"

LH = (7000.0, 5700.0)
LH_YAW = 30.0

BEAM = V + "Corner_Exterior_Wood"        # 21x24x300, pivot bottom centre: ribs, posts, frames (collides)
LINE = IP + "SM_KG_RailPost"             # 14x14x108: thin lines / small stakes (no collision)
FENCE = V + "Prop_WoodenFence_Single"    # 206x12x84 centred on X (collides)
RAIL = IP + "SM_KG_Railing_1m"           # 100x9x102 (collides)
HULL_PLANK = V + "Floor_WoodDark"        # 200x200x2 dark boards, scaled into long hull strakes (collides)
LOG = PIR + "Planks_2"                   # 197x45x10: log/lumber stacks (collides)
FLAME_MAT = "/Game/KillGodot/Materials/M_KG_TaskMarker"   # unlit glowing yellow-orange: flame tongues
LOOSE_PLANK = [PIR + "Planks_1", PIR + "Planks_3"]   # scattered / roofs (no collision)
ROCKS = [N + "Rock_Medium_1", N + "Rock_Medium_2", N + "Rock_Medium_3"]
PEBBLES = [N + f"Pebble_Round_{i}" for i in range(1, 6)] + [N + f"Pebble_Square_{i}" for i in range(1, 7)]
STEPS = [N + "RockPath_Round_Small_1", N + "RockPath_Round_Small_2", N + "RockPath_Round_Small_3"]
DRIFT = [N + f"DeadTree_{i}" for i in range(1, 6)]
PBARRELS = [PIR + f"Barrel_{i}" for i in range(0, 14)]

POIS = []


# ============================================================================================ small helpers
def gz(x, y):
    return C.ground(x, y)


def poi(name, x, y):
    POIS.append((name, round(x), round(y)))


def vadd(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vsub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vmul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def vlen(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def vnorm(a):
    n = vlen(a) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _vec(a):
    return unreal.Vector(float(a[0]), float(a[1]), float(a[2]))


def rot_zx(zdir, xdir):
    """Rotator whose +Z points along zdir (and +X as close as possible to xdir): (roll, pitch, yaw)."""
    r = unreal.MathLibrary.make_rot_from_zx(_vec(zdir), _vec(xdir))
    return r.roll, r.pitch, r.yaw


def rot_xz(xdir, zdir):
    r = unreal.MathLibrary.make_rot_from_xz(_vec(xdir), _vec(zdir))
    return r.roll, r.pitch, r.yaw


# ---- instancing: one HISM per mesh in the zone's foliage field. The FIRST use of a mesh fixes its collision and
# cull distance (KGFoliageField keeps one component per mesh), so every mesh has exactly one role here.
_batches = {}
_mode = {}


def inst(path, x, y, z, yaw=0.0, s=1.0, pitch=0.0, roll=0.0, collide=False, cull=7000.0):
    if path not in _mode:
        _mode[path] = (collide, cull)
    _batches.setdefault(path, []).append((x, y, z, yaw, s, pitch, roll))


def flush():
    for path, tr in _batches.items():
        col, cull = _mode[path]
        C.instanced(path, tr, collide=col, cull=cull, sub="Instanced")
    _batches.clear()


def beam(a, b, thick=1.0, xhint=(0.0, 0.0, 1.0), path=BEAM, collide=True, cull=0.0, depth=None):
    """A mesh whose length runs along its +Z (pivot at the bottom) stretched from world point a to b."""
    d = vsub(b, a)
    L = vlen(d)
    if L < 1.0:
        return
    if abs(vnorm(d)[2]) > 0.95 and xhint == (0.0, 0.0, 1.0):
        xhint = (1.0, 0.0, 0.0)
    roll, pitch, yaw = rot_zx(d, xhint)
    length = 300.0 if path == BEAM else 108.0 if path == LINE else 966.0 if "FlagTall_1" in path else 756.0
    inst(path, a[0], a[1], a[2], yaw, (thick, depth or thick, L / length), pitch, roll, collide=collide, cull=cull)


def put(path, x, y, z=None, yaw=0.0, s=1.0, pitch=0.0, roll=0.0, sub="Props", collide=True, cull=0.0, sink=0.0,
        claim=0.0, shadow=True):
    zz = (gz(x, y) - sink) if z is None else z
    return C.place(path, x, y, zz, yaw=yaw, pitch=pitch, roll=roll, scale=s, sub=sub, collide=collide, cull=cull,
                   claim_r=claim, shadow=shadow)


def clutter(path, x, y, z=None, yaw=None, s=1.0, pitch=0.0, roll=0.0, sink=1.0, cull=6000.0):
    """Small non-colliding prop through the instancer."""
    zz = (gz(x, y) - sink) if z is None else z
    inst(path, x, y, zz, R.uniform(0, 360) if yaw is None else yaw, s, pitch, roll, collide=False, cull=cull)


def seat(path, x, y, z=None, face=0.0, height=None, bench=False):
    """AKGSeat: the sitter faces `face` (world yaw). Quaternius seats face their local +Y, so the mesh turns -90."""
    s = C.seat(path, x, y, z, yaw=face, seat_height=height)
    if s and s.get_class().get_name() == "KGSeat":
        try:
            s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
        except Exception:
            pass
    return s


def mover_off(path, x, y, z, **kw):
    """Cosmetic mover without collision (cloth, flags, small flotsam)."""
    a = C.mover(path, x, y, z, **kw)
    if a:
        a.set_actor_enable_collision(False)
    return a


def attach(children, parent):
    for c in children:
        if not c:
            continue
        c.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


def resample(pts, step):
    """Even spacing along a polyline (fence pieces keep their natural length)."""
    out, carry = [pts[0]], 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        t = step - carry
        while t <= seg:
            out.append((ax + (bx - ax) * t / seg, ay + (by - ay) * t / seg))
            t += step
        carry = seg - (t - step)
    return out


def fence_line(pts, sink=4.0, skip=()):
    """Wooden fence following the terrain along a polyline (instanced, collides)."""
    k = 0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(round(L / 200.0)))
        for i in range(n):
            k += 1
            if k in skip:
                continue
            x0, y0 = ax + (bx - ax) * i / n, ay + (by - ay) * i / n
            x1, y1 = ax + (bx - ax) * (i + 1) / n, ay + (by - ay) * (i + 1) / n
            z0, z1 = gz(x0, y0), gz(x1, y1)
            seg = math.hypot(x1 - x0, y1 - y0)
            yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
            pitch = math.degrees(math.atan2(z1 - z0, seg))
            inst(FENCE, (x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5 - sink, yaw, (seg / 204.0, 1.0, 1.0),
                 pitch, 0.0, collide=True, cull=14000.0)


def rock(x, y, z=None, s=1.0, yaw=None, sink=0.25, pitch=0.0, roll=0.0, path=None):
    """Big rock (instanced, collides, never culled). sink = fraction of its height buried."""
    path = path or R.choice(ROCKS)
    s3 = (s, s, s) if isinstance(s, (int, float)) else s
    zz = (C.ground_min(x, y, 120.0 * s3[0]) if z is None else z) - 200.0 * s3[2] * sink
    inst(path, x, y, zz, R.uniform(0, 360) if yaw is None else yaw, s3, pitch, roll, collide=True, cull=0.0)


def pebbles(cx, cy, r, n, zmax=9999.0, s=(0.8, 1.6)):
    for _ in range(n):
        a, d = R.uniform(0, 2 * math.pi), r * math.sqrt(R.random())
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        if gz(x, y) > zmax or not C.in_zone(x, y):
            continue
        clutter(R.choice(PEBBLES), x, y, gz(x, y) - 2.0, s=R.uniform(*s), cull=5000.0)


FLOWER_SCALE = {"Flower_3_Group": (0.3, 0.48), "Flower_4_Group": (0.26, 0.4), "Flower_3_Single": (0.28, 0.42),
                "Flower_4_Single": (0.28, 0.42), "Bush_Common_Flowers": (0.4, 0.65), "Bush_Common": (0.45, 0.75),
                "Petal_1": (0.7, 1.1), "Petal_2": (0.6, 0.9), "Petal_4": (0.8, 1.2), "Petal_5": (0.5, 0.8),
                "Grass_Wispy_Short": (0.5, 0.8), "Fern_1": (0.4, 0.6), "Plant_1": (0.5, 0.8)}


def flowers(cx, cy, r, n, kinds=None, s=(0.7, 1.2)):
    kinds = kinds or [N + "Flower_3_Group", N + "Flower_4_Group", N + "Bush_Common_Flowers", N + "Petal_2",
                      N + "Petal_5", N + "Flower_3_Single"]
    for _ in range(n):
        a, d = R.uniform(0, 2 * math.pi), r * math.sqrt(R.random())
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        if not C.in_zone(x, y) or C.lane_distance(x, y) < 60.0 or C.building_hit(x, y, 40.0):
            continue
        k = R.choice(kinds)
        clutter(k, x, y, gz(x, y) - 4.0, s=R.uniform(*FLOWER_SCALE.get(k.split("/")[-1], s)), cull=7000.0)


def flame(x, y, z, s=1.0):
    """Stylised flame: wispy grass clumps with the glowing marker material, flickering (sway) via KGSpinner."""
    mat = unreal.load_asset(FLAME_MAT)
    for k, (path, sc, yaw) in enumerate(((N + "Grass_Wispy_Short", (0.42, 0.42, 0.8), 0.0),
                                         (N + "Grass_Wispy_Short", (0.3, 0.3, 1.05), 70.0),
                                         (N + "Grass_Common_Short", (0.55, 0.55, 0.55), 140.0))):
        a = mover_off(path, x, y, z, yaw=yaw, scale=(sc[0] * s, sc[1] * s, sc[2] * s), sway=5.0 + 2.0 * k,
                      sway_hz=1.3 + 0.4 * k)
        if a and mat:
            comp = a.get_component_by_class(unreal.StaticMeshComponent)
            for i in range(comp.get_num_materials()):
                comp.set_material(i, mat)
            comp.set_editor_property("cast_shadow", False)


def woodpile(x, y, yaw, n=5, tiers=3, length=0.55):
    """Stacked split logs (short dark planks on edge) with a couple of loose ones."""
    c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    g = min(gz(x + c * d, y + sn * d) for d in (-80.0, 0.0, 80.0))
    for t in range(tiers):
        for k in range(n - t):
            off = (k - (n - t - 1) * 0.5) * 30.0
            px, py = x - sn * off, y + c * off
            inst(LOG, px, py, g + 8.0 + t * 26.0, yaw + R.uniform(-4, 4), (length, 0.65, 2.6), 0.0, 90.0,
                 collide=True, cull=9000.0)


def light(x, y, z, i=10.0, r=900.0, color=(255, 170, 95)):
    if C.stats.get("lights", 0) >= 10:
        return None
    return C.light(x, y, z, intensity=i, radius=r, color=color)


def breakable(path, x, y, z=None, yaw=None):
    return C.breakable(path, x, y, z, R.uniform(0, 360) if yaw is None else yaw)


def lean_to(cx, cy, face, w=320.0, d=200.0, h_front=250.0, h_back=205.0):
    """Open timber lean-to: 4 posts, rails, a sloping plank roof. Returns (frame, base_z)."""
    S = C.Frame(cx, cy, C.facing_yaw((cx, cy), face), w, d)
    C.clear_grass(S.x, S.y, max(w, d) * 0.7)
    base = min(gz(*S.world(lx, ly)) for lx in (-w * 0.5, w * 0.5) for ly in (-d * 0.5, d * 0.5))
    hw, hd = w * 0.5 - 10.0, d * 0.5 - 5.0
    posts = {}
    for lx in (-hw, hw):
        for ly, h in ((-hd, h_front), (hd, h_back)):
            x, y = S.world(lx, ly)
            beam((x, y, gz(x, y) - 20.0), (x, y, base + h), thick=0.7)
            posts[(lx, ly)] = (x, y, base + h)
    for lx in (-hw, hw):
        beam(posts[(lx, -hd)], posts[(lx, hd)], thick=0.6)
    beam(posts[(-hw, -hd)], posts[(hw, -hd)], thick=0.6)
    beam(posts[(-hw, hd)], posts[(hw, hd)], thick=0.6)
    n = int(w / 50.0) + 1
    for k in range(n):
        lx = -w * 0.5 - 15.0 + k * 50.0
        x0, y0 = S.world(lx, hd + 23.0)
        x1, y1 = S.world(lx, -hd - 23.0)
        z0, z1 = base + h_back - 6.0, base + h_front + 18.0
        mid = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
        roll, pitch, yaw = rot_xz((x1 - x0, y1 - y0, z1 - z0), (0.0, 0.0, 1.0))
        inst(LOOSE_PLANK[k % 2], mid[0], mid[1], mid[2], yaw, ((d + 46.0) / 222.0, 1.18, 1.0), pitch, roll,
             collide=False, cull=12000.0)
    return S, base


# ============================================================================================ EAST: lighthouse
def keeper_yard():
    F = C.Frame(LH[0], LH[1], LH_YAW, 440.0, 440.0, "lighthouse")
    poi("Lighthouse keeper's yard", *LH)

    # --- vegetable garden (west side of the pad), fenced, with a scarecrow
    G = C.Frame(6430.0, 5790.0, LH_YAW, 360.0, 240.0)
    C.clear_grass(G.x, G.y, 260.0)
    for ly in (-70.0, 0.0, 70.0):
        x, y = G.world(0.0, ly)
        inst(WP + "SM_KG_DigMound", x, y, gz(x, y) - 14.0, LH_YAW, (2.6, 0.5, 0.6), collide=False, cull=7000.0)
        for k in range(-5, 6):
            px, py = G.world(k * 26.0 + R.uniform(-4, 4), ly + R.uniform(-6, 6))
            if ly == 0.0:
                clutter(N + "Plant_7", px, py, gz(px, py) + 4.0, s=R.uniform(0.35, 0.5), cull=6000.0)
            else:
                clutter(P + "Carrot", px, py, gz(px, py) + 12.0, s=R.uniform(0.9, 1.2), cull=5000.0)
    corners = [G.world(-190.0, -130.0), G.world(190.0, -130.0), G.world(190.0, 130.0), G.world(-190.0, 130.0)]
    for i in range(4):
        (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
        L = math.hypot(bx - ax, by - ay)
        n = int(round(L / 100.0))
        for k in range(n):
            if i == 0 and k in (n // 2, n // 2 - 1):     # gate facing the tower path
                continue
            x, y = ax + (bx - ax) * (k + 0.5) / n, ay + (by - ay) * (k + 0.5) / n
            inst(RAIL, x, y, gz(x, y) - 3.0, math.degrees(math.atan2(by - ay, bx - ax)), (L / n / 100.0, 1.0, 0.7),
                 collide=True, cull=9000.0)
    sx, sy = G.world(150.0, 95.0)
    put(P + "Dummy", sx, sy, yaw=LH_YAW + 200.0, cull=12000.0)                       # scarecrow
    x, y = G.world(-150.0, 90.0)
    clutter(P + "FarmCrate_Carrot", x, y, yaw=LH_YAW + 10.0)
    x, y = G.world(-120.0, -100.0)
    clutter(P + "Bucket_Wooden_1", x, y)
    x, y = G.world(205.0, -40.0)
    clutter(WP + "SM_KG_Shovel", x, y, gz(x, y) + 45.0, yaw=LH_YAW + 90.0, pitch=0.0, roll=70.0)
    x, y = G.world(-205.0, 20.0)
    clutter(P + "Bag", x, y, yaw=LH_YAW + 80.0)

    # --- laundry line behind the tower (sea side), cloths sway in the wind
    a = (6480.0, 6000.0)
    b = (6880.0, 6135.0)
    za, zb = gz(*a), gz(*b)
    for (px, py), pz in ((a, za), (b, zb)):
        beam((px, py, pz - 20.0), (px, py, pz + 195.0), thick=0.55)
    la, lb = (a[0], a[1], za + 185.0), (b[0], b[1], zb + 185.0)
    beam(la, lb, thick=0.18, path=LINE, collide=False, cull=8000.0)
    lyaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    cloths = [P + "Banner_1_Cloth", P + "Banner_2_Cloth", P + "Banner_1_Cloth", P + "Banner_2_Cloth"]
    for k, t in enumerate((0.18, 0.4, 0.62, 0.84)):
        x, y, z = la[0] + (lb[0] - la[0]) * t, la[1] + (lb[1] - la[1]) * t, la[2] + (lb[2] - la[2]) * t
        mover_off(cloths[k], x, y, z + 2.0, yaw=lyaw, scale=(0.75, 1.0, 0.42 + 0.06 * (k % 2)), sway=7.0,
                  sway_hz=0.4 + 0.07 * k)
    x, y = la[0] + (lb[0] - la[0]) * 0.5, la[1] + (lb[1] - la[1]) * 0.5
    clutter(P + "Bucket_Metal", x + 60.0, y - 80.0)
    clutter(IP + "SM_KG_Rug_Runner", x - 40.0, y - 110.0, gz(x - 40.0, y - 110.0) + 20.0, yaw=lyaw + 10.0,
            s=(0.6, 0.8, 1.0), pitch=0.0, roll=0.0, cull=5000.0)

    # --- lamp-oil lean-to on the east side of the pad
    S, base = lean_to(7560.0, 5830.0, LH)
    # oil barrels under it (back row two tiers), crates, a locker
    for k, lx in enumerate((-100.0, 0.0, 100.0)):
        x, y = S.world(lx, 45.0)
        put(PBARRELS[(k * 5) % 14], x, y, base, yaw=R.uniform(0, 360), s=0.9, cull=10000.0)
    x, y = S.world(-50.0, 45.0)
    put(PBARRELS[7], x, y, base + 92.0, yaw=R.uniform(0, 360), s=0.85, cull=10000.0)
    x, y = S.world(95.0, -40.0)
    put(P + "Barrel_Holder", x, y, base, yaw=S.yaw + 90.0, cull=10000.0)
    x, y = S.world(-110.0, -45.0)
    C.loot_chest(x, y, base, yaw=S.yaw + 180.0, table="Chest", name="Keeper's Locker", path=CHEST_WOOD)
    x, y = S.world(-190.0, -120.0)
    breakable(P + "Crate_Wooden", x, y)
    x, y = S.world(-210.0, -30.0)
    breakable(P + "Barrel", x, y)
    x, y = S.world(0.0, -40.0)
    clutter(P + "Rope_2", x, y, base + 1.0)
    x, y = S.world(150.0, -110.0)
    put(P + "Lantern_Wall", x, y, base + 150.0, yaw=S.yaw - 90.0, collide=False, cull=8000.0)
    light(x, y, base + 190.0, 9.0, 700.0)

    # --- oil barrels by the chore (outside its reserved circle)
    for x, y in ((7440.0, 5470.0), (7500.0, 5390.0), (7470.0, 5545.0)):
        put(PBARRELS[R.randrange(14)], x, y, gz(x, y) - 3.0, yaw=R.uniform(0, 360), s=0.85, cull=10000.0)
    clutter(P + "Bucket_Metal", 7385.0, 5560.0)
    woodpile(7560.0, 5480.0, 100.0, n=4, tiers=2)

    # --- door torch
    x, y = F.world(240.0, -240.0)
    put(PIR + "Torch_1", x, y, yaw=LH_YAW, cull=12000.0)
    light(x, y, gz(x, y) + 270.0, 10.0, 800.0)

    # --- woodpile against the west wall + chopping block
    x, y = F.world(-275.0, -40.0)
    woodpile(x, y, LH_YAW + 90.0, n=6, tiers=4, length=0.5)
    x, y = F.world(-380.0, -220.0)
    put(P + "Anvil_Log", x, y, sink=45.0, yaw=R.uniform(0, 360), s=(1.0, 1.0, 0.6), cull=9000.0)
    clutter(P + "Axe_Bronze", x + 5.0, y, gz(x, y) + 58.0, yaw=40.0, pitch=0.0, roll=-20.0)
    for k in range(4):
        px, py = x + R.uniform(-90, 90), y + R.uniform(-90, 90)
        inst(LOG, px, py, gz(px, py) + 2.0, R.uniform(0, 360), (0.3, 0.6, 2.4), 0.0, 90.0 if k % 2 else 0.0,
             collide=True, cull=9000.0)

    # --- keeper's outdoor table (west, between garden and tower)
    tx, ty = 6590.0, 5545.0
    C.clear_grass(tx, ty, 170.0)
    tz = C.ground_min(tx, ty, 60.0)
    put(KIT + "Kitchen_Square_Table", tx, ty, tz, yaw=LH_YAW + 8.0, cull=9000.0)
    seat(P + "Chair_1", tx - 75.0, ty + 10.0, gz(tx - 75.0, ty + 10.0), face=0.0 + 8.0, height=49.8)
    seat(P + "Stool", tx + 30.0, ty - 80.0, gz(tx + 30.0, ty - 80.0), face=95.0, height=58.2)
    for path, ox, oy in ((P + "Mug", -10, 12), (KIT + "Kitchen_Bottle", 18, -14), (P + "Table_Plate", 12, 18),
                         (WP + "SM_KG_Fish_Mackerel", 12, 18), (P + "CandleStick", -22, -20)):
        clutter(path, tx + ox, ty + oy, tz + 67.0 + (2.0 if "Fish" in path else 0.0), cull=4500.0)

    # --- signal mast with a pennant (NE corner of the pad)
    mx, my = 7690.0, 6050.0
    mz = gz(mx, my)
    put(PIR + "FlagLow_0", mx, my, mz - 30.0, cull=0.0)
    mover_off(PIR + "FlagLow_1", mx + 70.0, my, mz + 560.0, yaw=0.0, sway=5.0, sway_hz=0.6)

    # --- sea-view bench behind the tower (faces the shrine island)
    bx, by = 7170.0, 6120.0
    seat(P + "Bench", bx, by, C.ground_min(bx, by, 120.0) + 2.0, face=100.0, height=49.6, bench=True)
    clutter(P + "Lantern_Wall", bx + 170.0, by + 20.0, gz(bx + 170.0, by + 20.0) - 8.0, yaw=100.0, cull=6000.0)
    flowers(bx - 60.0, by - 90.0, 160.0, 6)

    # --- cliff-edge fence: follows the brow of the pad from the west round the north to the east
    pts = []
    for deg in range(164, 12, -4):
        a = math.radians(deg)
        r = 420.0
        while r < 1400.0 and gz(LH[0] + r * math.cos(a), LH[1] + r * math.sin(a)) > 780.0:
            r += 25.0
        pts.append((LH[0] + (r - 40.0) * math.cos(a), LH[1] + (r - 40.0) * math.sin(a)))
    pts = resample(pts, 200.0)
    fence_line(pts, skip=(len(pts) // 2,))
    # flowers and bushes along the fence
    for (x, y) in pts[::2]:
        flowers(x, y, 120.0, 2, kinds=[N + "Bush_Common_Flowers", N + "Flower_4_Group", N + "Grass_Wispy_Short"])


def viewpoint():
    """Timber deck on the east brow of the lighthouse ridge, looking down on the cove and the wreck."""
    ox, oy, yaw = 7990.0, 5570.0, 22.0
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def W(fx, sx):          # fx forward (toward the cove), sx sideways
        return ox + fx * cy_ - sx * sy_, oy + fx * sy_ + sx * cy_

    deck_z = max(gz(*W(fx, sx)) for fx in (0.0, 60.0) for sx in (-200.0, 0.0, 200.0)) + 4.0
    poi("East viewpoint deck", ox + 200.0, oy)
    C.clear_grass(ox + 200.0, oy, 320.0)

    for fx in (100.0, 300.0):
        for sx in (-100.0, 100.0):
            x, y = W(fx, sx)
            put(V + "Floor_WoodDark", x, y, deck_z, yaw=yaw, cull=0.0)
    # supports + braces
    for fx in (20.0, 200.0, 390.0):
        for sx in (-190.0, 190.0):
            x, y = W(fx, sx)
            g = gz(x, y)
            if g < deck_z - 10.0:
                beam((x, y, g - 30.0), (x, y, deck_z - 2.0), thick=0.9)
    for sx in (-190.0, 190.0):
        x0, y0 = W(390.0, sx)
        x1, y1 = W(200.0, sx)
        beam((x0, y0, gz(x0, y0) + 10.0), (x1, y1, deck_z - 8.0), thick=0.6)
    # railings on three sides (+ corner posts)
    for k in range(4):
        x, y = W(50.0 + k * 100.0, -195.0)
        inst(RAIL, x, y, deck_z, yaw, 1.0, collide=True)
        x, y = W(50.0 + k * 100.0, 195.0)
        inst(RAIL, x, y, deck_z, yaw, 1.0, collide=True)
        x, y = W(398.0, -150.0 + k * 100.0)
        inst(RAIL, x, y, deck_z, yaw + 90.0, 1.0, collide=True)
    for fx, sx in ((0.0, -195.0), (0.0, 195.0), (398.0, -195.0), (398.0, 195.0)):
        x, y = W(fx, sx)
        inst(IP + "SM_KG_RailPost", x, y, deck_z, yaw, (1.2, 1.2, 1.1), collide=False, cull=9000.0)
    # bench looking out, a signal cannon, a lantern post, a spyglass crate
    x, y = W(250.0, -100.0)
    seat(P + "Bench", x, y, deck_z, face=yaw, height=49.6, bench=True)
    x, y = W(300.0, 110.0)
    put(PIR + "cannon_0_001", x, y, deck_z - 8.0, yaw=yaw, s=0.8, cull=12000.0)
    clutter(PIR + "Cannon_Ball_0", *W(170.0, 150.0), z=deck_z, s=0.35)
    x, y = W(40.0, 160.0)                       # signal brazier: a cauldron on a tripod with a live flame
    for k in range(3):
        a = math.radians(k * 120.0 + 30.0)
        beam((x + 45.0 * math.cos(a), y + 45.0 * math.sin(a), deck_z), (x, y, deck_z + 95.0), thick=0.3)
    put(P + "Cauldron", x, y, deck_z + 55.0, s=0.5, collide=False, cull=10000.0)
    flame(x, y, deck_z + 85.0, 0.7)
    light(x, y, deck_z + 150.0, 10.0, 900.0, color=(255, 140, 60))
    x, y = W(60.0, -160.0)
    clutter(PIR + "crates_1", x, y, deck_z, yaw=yaw + 15.0, s=0.7)
    clutter(P + "Scroll_1", x, y, deck_z + 49.0, yaw=yaw + 40.0, s=1.5)
    C.ambient("A_Wind_Loop", ox + 200.0, oy, deck_z + 200.0, 0.6)


def lighthouse_path():
    """Rope handrail up the steep last stretch of the lighthouse lane + a waymark signpost at the trail fork."""
    samples = [s for s in C.lane_samples("east_market", 260.0) if s[1] > 3300.0]
    prev = None
    for (x, y, dx, dy) in samples:
        ex, ey = x + dy * 250.0, y - dx * 250.0          # east edge (+ margin)
        if not C.in_zone(ex, ey):
            prev = None
            continue
        z = gz(ex, ey)
        beam((ex, ey, z - 20.0), (ex, ey, z + 95.0), thick=0.45)
        top = (ex, ey, z + 88.0)
        if prev:
            beam(prev, top, thick=0.14, path=LINE, collide=False, cull=8000.0)
        prev = top
    # signpost where the trail leaves the lane
    x, y = 6700.0, 3200.0
    z = gz(x, y)
    beam((x, y, z - 20.0), (x, y, z + 230.0), thick=0.6)
    for k, (yaw, zz) in enumerate(((5.0, 190.0), (80.0, 160.0), (-30.0, 130.0))):
        inst(LOOSE_PLANK[1], x + 55.0 * math.cos(math.radians(yaw)), y + 55.0 * math.sin(math.radians(yaw)), z + zz,
             yaw, (0.55, 0.55, 1.6), 0.0, 90.0)
    poi("Waymark signpost (trail fork)", x, y)


def trail(points, step=150.0, wide=False):
    """Stepping-stone trail (flat, non-colliding) along a polyline."""
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / step))
        for i in range(n):
            t = (i + R.uniform(0.2, 0.8)) / n
            x = ax + (bx - ax) * t + R.uniform(-35, 35)
            y = ay + (by - ay) * t + R.uniform(-35, 35)
            if not C.in_zone(x, y) or C.lane_distance(x, y) < 0.0:
                continue
            sc = R.uniform(0.9, 1.2) if wide else R.uniform(0.8, 1.05)
            inst(STEPS[R.randrange(3)], x, y, gz(x, y) - 5.0, R.uniform(0, 360), (sc, sc, 1.3), collide=False,
                 cull=8000.0)


# ============================================================================================ EAST: wreck
def wreck(bow, stern, bow_sink, stern_sink, list_deg, width, height, ribs, name, breach=(0.3, 0.42), mast=True,
          deck=True, seed=1, planks=1.0):
    """Compose a broken hull from beams and planks. bow/stern: (x, y) of the keel ends. The high side (+S) faces
    the viewer and is smashed open; the keel is buried `*_sink` cm into the sand/seabed."""
    rr = random.Random(seed)
    b3 = (bow[0], bow[1], gz(*bow) - bow_sink)
    s3 = (stern[0], stern[1], gz(*stern) - stern_sink)
    Fw = vnorm(vsub(b3, s3))
    L = vlen(vsub(b3, s3))
    S0 = vnorm(cross(Fw, (0.0, 0.0, 1.0)))
    U0 = vnorm(cross(S0, Fw))
    a = math.radians(list_deg)
    S = vadd(vmul(S0, math.cos(a)), vmul(U0, math.sin(a)))       # the +S side is lifted by the list
    U = vadd(vmul(U0, math.cos(a)), vmul(S0, -math.sin(a)))

    def W(t, y, z):
        return vadd(vadd(s3, vmul(Fw, t * L)), vadd(vmul(S, y), vmul(U, z)))

    def wf(t):
        f = 1.0 if t < 0.6 else max(0.1, 1.0 - ((t - 0.6) / 0.4) ** 1.6 * 0.9)
        if t < 0.15:
            f *= 0.82 + 0.18 * t / 0.15
        return f

    def hf(t):
        return 1.0 + 0.3 * max(0.0, (t - 0.72) / 0.28) ** 2 + 0.12 * max(0.0, (0.1 - t) / 0.1)

    prof = [(0.0, 0.0), (0.46, 0.1), (0.78, 0.34), (0.95, 0.66), (1.0, 1.0)]

    def hp(t, side, k):
        py, pz = prof[k]
        return W(t, side * py * width * 0.5 * wf(t), pz * height * hf(t))

    # keel, stem and sternpost
    n = max(2, int(L / 280.0))
    for i in range(n):
        beam(W(i / n, 0, -10), W((i + 1) / n, 0, -10), thick=1.8, xhint=U)
    stem = [W(1.0, 0, -10), W(1.02, 0, height * 0.45), W(1.035, 0, height * 0.9), W(1.03, 0, height * 1.35)]
    for p, q in zip(stem, stem[1:]):
        beam(p, q, thick=1.5, xhint=Fw)
    beam(W(0.0, 0, -10), W(-0.015, 0, height * 1.1), thick=1.5, xhint=Fw)
    # ribs
    ts = [0.05 + i * 0.9 / (ribs - 1) for i in range(ribs)]
    for t in ts:
        in_breach = breach[0] <= t <= breach[1]
        for side in (-1, 1):
            top = 4
            if side == 1:
                top = 1 if in_breach else rr.choice((2, 3, 3, 4))
            elif in_breach:
                top = 3
            for k in range(top):
                beam(hp(t, side, k), hp(t, side, k + 1), thick=0.95, xhint=Fw)
        # a deck beam across at 70 % height on some ribs
        if not in_breach and rr.random() < 0.45:
            beam(W(t, -width * 0.47 * wf(t), height * 0.68), W(t, width * 0.47 * wf(t), height * 0.72),
                 thick=0.7, xhint=Fw)
    # planking rows between the ribs
    for side in (-1, 1):
        for k in range(4):
            for frac in ((0.3, 0.75) if side == 1 else (0.2, 0.55, 0.9)):
                t = 0.0
                while t < 1.0:
                    t1 = min(1.0, t + 0.145)
                    skip = False
                    if side == 1:
                        skip = (breach[0] - 0.05 <= t <= breach[1]) or rr.random() < (0.35 + 0.15 * k)
                    else:
                        skip = rr.random() < 0.08 or (k == 3 and rr.random() < 0.3)
                    skip = skip or rr.random() > planks
                    if not skip:
                        pa = vadd(vmul(hp(t, side, k), 1 - frac), vmul(hp(t, side, k + 1), frac))
                        pb = vadd(vmul(hp(t1, side, k), 1 - frac), vmul(hp(t1, side, k + 1), frac))
                        mid = vmul(vadd(pa, pb), 0.5)
                        out = vnorm(vsub(mid, W((t + t1) * 0.5, 0, height * 0.55)))
                        roll, pitch, yaw = rot_xz(vsub(pb, pa), out)
                        seg = vlen(vsub(pb, pa))
                        inst(HULL_PLANK, mid[0], mid[1], mid[2], yaw, (seg / 196.0, 0.34, 4.0), pitch, roll,
                             collide=True, cull=0.0)
                    t = t1
    # low-side gunwale rail
    for i in range(6):
        t0, t1 = i / 6.0, (i + 1) / 6.0
        beam(hp(t0, -1, 4), hp(t1, -1, 4), thick=1.1, xhint=U)
    # quarterdeck (a pier section as the raised deck with its rope rail), tilted with the hull, half flooded
    if deck:
        fd = W(0.2, 0.0, height * 0.62 - 140.0 * height / 400.0)
        roll, pitch, yaw = rot_xz(Fw, U)
        put(PIR + "pier_2", fd[0], fd[1], fd[2], yaw=yaw, pitch=pitch, roll=roll,
            s=(0.75, 0.85 * wf(0.2) * width / 250.0, height / 400.0), cull=0.0)
    if mast:
        base = W(0.56, 0.0, 0.0)
        mdir = vnorm(vadd(vadd(vmul(U, 0.83), vmul(S, -0.45)), vmul(Fw, -0.2)))
        top = vadd(base, vmul(mdir, 740.0))
        beam(base, top, thick=1.4, path=PIR + "FlagTall_1", xhint=Fw, collide=True)
        yard_c = vadd(base, vmul(mdir, 560.0))
        yard_dir = vnorm(vadd(Fw, vmul(U, 0.35)))
        beam(vadd(yard_c, vmul(yard_dir, -300.0)), vadd(yard_c, vmul(yard_dir, 260.0)), thick=1.0,
             path=PIR + "FlagLow_0", xhint=U, collide=False)
        # the torn sail hangs from the yard down the low side into the water
        sail = vadd(yard_c, vmul(U, -60.0))
        roll, pitch, yaw = rot_xz(Fw, S)
        put(PIR + "Broken_Flag_1", sail[0], sail[1], sail[2] - 120.0, yaw=yaw, pitch=pitch + 10.0, roll=roll,
            s=(0.8, 0.8, 1.0), collide=False, cull=0.0)
        put(PIR + "Broken_Flag_0", top[0], top[1], top[2] - 40.0, yaw=R.uniform(0, 360), collide=False, cull=0.0)
    return W


def east_wreck():
    bow, stern = (11850.0, 5300.0), (10350.0, 6500.0)
    W = wreck(bow, stern, 50.0, 30.0, 24.0, 640.0, 470.0, 14, "Gannet", seed=11)
    mid = W(0.5, 0, 0)
    poi("Wreck of the Gannet (east cove)", mid[0], mid[1])
    # treasure chest wedged inside the dry bow
    c = W(0.72, 70.0, 0.0)
    C.loot_chest(c[0], c[1], gz(c[0], c[1]) - 8.0, yaw=R.uniform(0, 360), table="Chest", name="Ship's Strongbox",
                 path=PIR + "chest_gold_0")
    # spilled cargo on the sand (high side, toward the beach)
    for k, (t, off) in enumerate(((0.9, 420.0), (0.78, 520.0), (0.62, 470.0), (0.95, 250.0), (0.7, 700.0),
                                  (0.55, 640.0), (0.85, 760.0), (0.4, 560.0))):
        p = W(t, off, 0.0)
        x, y = p[0] + R.uniform(-60, 60), p[1] + R.uniform(-60, 60)
        g = gz(x, y)
        if g < -40.0:
            continue
        tipped = k % 3 == 0
        put(PBARRELS[(k * 3) % 14], x, y, g - (20.0 if tipped else 25.0), yaw=R.uniform(0, 360),
            pitch=90.0 if tipped else R.uniform(-8, 8), s=0.9, cull=11000.0)
    for k, (t, off) in enumerate(((0.83, 330.0), (0.66, 380.0), (0.97, 420.0))):
        p = W(t, off, 0.0)
        breakable(P + "Crate_Wooden" if k != 1 else P + "Barrel", p[0], p[1])
    for t, off in ((0.72, 300.0), (0.5, 330.0)):
        p = W(t, off, 0.0)
        clutter(PIR + "crates_0", p[0], p[1], gz(p[0], p[1]) - 15.0, yaw=R.uniform(0, 360), pitch=R.uniform(-15, 15),
                s=0.9, cull=9000.0)
    # the cannon fell out and lies on its side in the surf
    p = W(0.45, 470.0, 0.0)
    put(PIR + "cannon_0", p[0], p[1], gz(p[0], p[1]) - 35.0, yaw=R.uniform(0, 360), roll=25.0, s=0.8, cull=0.0)
    p2 = W(0.4, 610.0, 0.0)
    clutter(PIR + "Cannon_Ball_1", p2[0], p2[1], gz(p2[0], p2[1]) - 10.0, s=0.8, cull=8000.0)
    # loose planks, rope and lost bits scattered up the beach
    for k in range(18):
        t = R.uniform(0.3, 1.1)
        p = W(t, R.uniform(300.0, 1100.0), 0.0)
        x, y = p[0], p[1]
        if gz(x, y) < -45.0 or not C.in_zone(x, y):
            continue
        clutter(LOOSE_PLANK[k % 2], x, y, gz(x, y) - 2.0, pitch=R.uniform(-6, 6), roll=R.uniform(-8, 8),
                s=R.uniform(0.6, 1.1), cull=9000.0)
    for path in (P + "Rope_3", P + "Rope_1", P + "Chain_Coil", P + "Shield_Wooden", P + "Bottle_1", PIR + "crates_1"):
        p = W(R.uniform(0.5, 1.0), R.uniform(300.0, 800.0), 0.0)
        clutter(path, p[0], p[1], gz(p[0], p[1]) - 3.0, pitch=R.uniform(-10, 10), cull=6000.0)
    # flotsam bobbing off the stern
    for k, (dx, dy) in enumerate(((-500.0, 380.0), (-150.0, 700.0), (-900.0, 150.0), (250.0, 900.0))):
        x, y = stern[0] + dx, stern[1] + dy
        path = [PIR + "crates_0", PBARRELS[4], LOOSE_PLANK[0], PBARRELS[9]][k]
        mover_off(path, x, y, -30.0 if "Barrel" in path else -15.0, yaw=R.uniform(0, 360),
                  pitch=90.0 if "Barrel" in path else 0.0, bob=9.0 + 3.0 * k, spin=(0.0, 3.0 + k, 0.0))
    C.ambient("A_Lapping_Loop", mid[0], mid[1], 120.0, 0.9)
    salvager_stall(W)


def salvager_stall(W):
    """A salvager has set up on the beach: stall frame, salvaged goods, lantern, stool."""
    x, y = 11050.0, 5260.0
    z = C.ground_min(x, y, 120.0)
    C.clear_grass(x, y, 250.0)
    poi("Salvager's stall", x, y)
    yaw = 200.0
    put(P + "Stall_Empty", x, y, z, yaw=yaw, cull=12000.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def L(lx, ly):
        return x + lx * c - ly * s, y + lx * s + ly * c

    for path, lx, ly, zz in ((PIR + "crates_0", -55.0, 10.0, 0.0), (PIR + "crates_1", 30.0, 15.0, 0.0),
                             (PIR + "Cannon_Ball_0", 30.0, 15.0, 70.0), (P + "Coin_Pile", -50.0, 10.0, 70.0),
                             (P + "Bottle_1", -30.0, 25.0, 70.0), (P + "Shield_Wooden", 60.0, -60.0, 0.0)):
        px, py = L(lx, ly)
        clutter(path, px, py, z + zz, yaw=yaw + R.uniform(-20, 20), s=0.6 if "Ball" in path else 1.0)
    px, py = L(0.0, -120.0)
    seat(P + "Stool", px, py, gz(px, py), face=yaw + 90.0, height=58.2)
    px, py = L(-140.0, -40.0)
    breakable(P + "Barrel", px, py)
    px, py = L(150.0, -20.0)
    put(PIR + "Torch_2", px, py, yaw=yaw, cull=10000.0)
    light(px, py, gz(px, py) + 260.0, 9.0, 750.0)
    px, py = L(-100.0, -170.0)
    clutter(WP + "SM_KG_Oar", px, py, gz(px, py) + 3.0, yaw=yaw + 60.0)
    clutter(WP + "SM_KG_Oar", px + 20.0, py + 25.0, gz(px, py) + 6.0, yaw=yaw + 72.0)


def east_cove_rest():
    """Tide pools on the east strip, rocks at the cove's cliffs, dune grass, driftwood."""
    for cx, cy in ((12580.0, 6250.0), (12700.0, 6750.0), (12450.0, 5800.0)):
        tide_pool(cx, cy)
    # boulders at the foot of the east cliff
    for x, y, s in ((12950.0, 6000.0, 1.6), (13050.0, 6600.0, 2.2), (12980.0, 7150.0, 1.9), (12800.0, 7500.0, 1.4),
                    (13150.0, 5500.0, 2.0), (12300.0, 5350.0, 1.1)):
        rock(x, y, s=s, sink=0.3)
    driftwood_line([(9300.0, 5750.0), (10300.0, 5700.0), (11400.0, 5620.0), (12300.0, 5700.0)], 7)
    dune_grass(9200.0, 12600.0, 4600.0, 5600.0, 14, zmin=170.0, zmax=320.0)


def tide_pool(cx, cy, n=9, r=170.0):
    poi("Tide pool", cx, cy)
    for k in range(n):
        a = k * 2 * math.pi / n + R.uniform(-0.2, 0.2)
        rr = r * R.uniform(0.8, 1.15)
        x, y = cx + rr * math.cos(a), cy + rr * math.sin(a)
        if k % 3 == 0:
            rock(x, y, s=R.uniform(0.3, 0.5), sink=0.35)
        else:
            clutter(STEPS[k % 3], x, y, gz(x, y) - 2.0, s=R.uniform(0.5, 0.8), pitch=R.uniform(-15, 15),
                    roll=R.uniform(-15, 15), cull=6000.0)
    for _ in range(5):
        a, d = R.uniform(0, 2 * math.pi), R.uniform(0, r * 0.7)
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        clutter(N + "Plant_7", x, y, gz(x, y) - 8.0, s=R.uniform(0.3, 0.6), cull=5000.0)
    pebbles(cx, cy, r * 1.3, 10)
    x, y = cx + R.uniform(-40, 40), cy + R.uniform(-40, 40)
    clutter(WP + R.choice(["SM_KG_Fish_Cod", "SM_KG_Fish_Mackerel", "SM_KG_Fish_Salmon"]), x, y, gz(x, y) + 3.0,
            roll=90.0, cull=4500.0)


def driftwood_line(points, n):
    for _ in range(n):
        i = R.randrange(len(points) - 1)
        (ax, ay), (bx, by) = points[i], points[i + 1]
        t = R.random()
        x, y = ax + (bx - ax) * t + R.uniform(-120, 120), ay + (by - ay) * t + R.uniform(-120, 120)
        if not C.in_zone(x, y):
            continue
        inst(R.choice(DRIFT), x, y, gz(x, y) + 8.0, R.uniform(0, 360), R.uniform(0.16, 0.26), 0.0,
             R.choice((88.0, -88.0, 92.0)), collide=False, cull=10000.0)


def dune_grass(x0, x1, y0, y1, n, zmin=40.0, zmax=260.0):
    kinds = [N + "Grass_Wispy_Short", N + "Grass_Wispy_Tall", N + "Grass_Common_Tall"]
    placed, tries = 0, 0
    while placed < n and tries < n * 12:
        tries += 1
        x, y = R.uniform(x0, x1), R.uniform(y0, y1)
        z = gz(x, y)
        if not (zmin < z < zmax) or not C.in_zone(x, y) or C.lane_distance(x, y) < 50.0 or C.overlaps(x, y, 200.0):
            continue
        # tufts come in clumps
        for _ in range(R.randint(2, 3)):
            px, py = x + R.uniform(-60, 60), y + R.uniform(-60, 60)
            clutter(R.choice(kinds), px, py, gz(px, py) - 4.0, s=R.uniform(0.45, 0.75), cull=6000.0)
        placed += 1


# ============================================================================================ EAST: plateau
def standing_stones():
    cx, cy = 10300.0, 3600.0
    poi("Maidens' Ring standing stones", cx, cy)
    C.clear_grass(cx, cy, 420.0)
    n = 9
    tops = {}
    for k in range(n):
        a = k * 2 * math.pi / n + 0.2
        x, y = cx + 560.0 * math.cos(a), cy + 560.0 * math.sin(a)
        h = R.uniform(1.35, 1.8) if k not in (0, 1) else 1.6
        if k == 5:       # one maiden has fallen
            rock(x, y, s=(0.55, 0.45, 1.5), yaw=math.degrees(a), sink=0.12, pitch=84.0, path=N + "Rock_Medium_2")
            continue
        rock(x, y, s=(0.55, 0.42, h), yaw=math.degrees(a) + 90.0, sink=0.05, path=N + "Rock_Medium_2")
        tops[k] = (x, y, C.ground_min(x, y, 66.0) - 190.0 * h * 0.05 + 190.0 * h)
        for j in range(2):
            clutter(N + "Grass_Wispy_Short", x + R.uniform(-70, 70), y + R.uniform(-70, 70), s=0.6)
    # the lintel across the two tallest maidens (a trilithon gate facing the lighthouse)
    (x0, y0, z0), (x1, y1, z1) = tops[0], tops[1]
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    rock((x0 + x1) * 0.5, (y0 + y1) * 0.5, z=min(z0, z1) - 30.0, s=(1.35, 0.4, 0.3), yaw=yaw, sink=0.0,
         path=N + "Rock_Medium_2")
    rock(cx, cy, s=(0.6, 0.5, 0.32), sink=0.1, path=N + "Rock_Medium_3")          # the altar stone
    top = gz(cx, cy) + 48.0
    for ox, oy, path in ((-30, 10, P + "Candle_2"), (25, -15, P + "Candle_1"), (5, 30, P + "Candle_2"),
                         (-10, -25, P + "Coin_Pile"), (30, 20, KIT + "Kitchen_Bowl"), (-35, -5, P + "Chalice")):
        clutter(path, cx + ox, cy + oy, top, cull=4500.0)
    # a fairy ring of mushrooms and petals inside the circle
    for k in range(20):
        a = k * 2 * math.pi / 20
        x, y = cx + 300.0 * math.cos(a), cy + 300.0 * math.sin(a)
        clutter(N + ("Mushroom_Common" if k % 2 else "Petal_1"), x, y, s=R.uniform(0.6, 0.9), cull=5000.0)
    flowers(cx, cy, 900.0, 18)


def ruined_watchhouse():
    """Roofless stone watch-house: broken brick walls, a fallen wall, rubble, vines, a chest in the corner."""
    cx, cy, yaw = 9250.0, 3330.0, 18.0
    poi("Ruined watch-house", cx, cy)
    C.clear_grass(cx, cy, 330.0)
    Fr = C.Frame(cx, cy, yaw, 400.0, 400.0)
    base = C.ground_min(cx, cy, 250.0) - 10.0
    walls = [  # (lx, ly, wall yaw offset, mesh, z scale)
        (-100.0, -200.0, 0.0, "Wall_UnevenBrick_Door_Round", 1.0),
        (100.0, -200.0, 0.0, "Wall_UnevenBrick_Straight", 0.55),
        (200.0, -100.0, 90.0, "Wall_UnevenBrick_Window_Thin_Round", 1.0),
        (200.0, 100.0, 90.0, "Wall_UnevenBrick_Straight", 0.75),
        (-100.0, 200.0, 180.0, "Wall_UnevenBrick_Straight", 0.4),
        (-200.0, 100.0, 270.0, "Wall_UnevenBrick_Window_Wide_Round", 0.9),
        (-200.0, -100.0, 270.0, "Wall_UnevenBrick_Straight", 1.15),
    ]
    for lx, ly, wy, m, zs in walls:
        x, y = Fr.world(lx, ly)
        put(V + m, x, y, base, yaw=yaw + wy, s=(1.0, 1.0, zs), cull=0.0)
    for lx, ly, zs in ((-200.0, -200.0, 1.2), (200.0, -200.0, 0.7), (200.0, 200.0, 0.85), (-200.0, 200.0, 0.5)):
        x, y = Fr.world(lx, ly)
        put(V + "Corner_Exterior_Brick", x, y, base, yaw=yaw, s=(1.0, 1.0, zs), cull=0.0)
    # the fallen wall lies outside on the grass
    x, y = Fr.world(120.0, 360.0)
    put(V + "Wall_UnevenBrick_Straight", x, y, gz(x, y) + 15.0, yaw=yaw + 180.0, pitch=0.0, roll=-84.0,
        s=(1.0, 1.0, 0.8), cull=0.0)
    # rubble
    bricks = [V + f"Prop_Brick{i}" for i in range(1, 5)]
    for _ in range(40):
        lx, ly = R.uniform(-320, 320), R.uniform(-320, 460)
        x, y = Fr.world(lx, ly)
        clutter(R.choice(bricks), x, y, gz(x, y) + 5.0, pitch=R.uniform(-30, 30), roll=R.uniform(-30, 30),
                s=R.uniform(0.9, 1.4), cull=5000.0)
    for lx, ly in ((150.0, 280.0), (-150.0, -120.0), (60.0, 140.0)):
        x, y = Fr.world(lx, ly)
        clutter(P + "Vase_Rubble_Medium", x, y, s=1.2, cull=6000.0)
    for lx, ly, wy, m in ((200.0, -100.0, 90.0, "Prop_Vine1"), (-200.0, -100.0, 270.0, "Prop_Vine2"),
                          (-100.0, -200.0, 0.0, "Prop_Vine4")):
        x, y = Fr.world(lx * 1.12, ly * 1.12 if abs(ly) > 150 else ly)
        put(V + m, x, y, base + 280.0, yaw=yaw + wy, collide=False, cull=9000.0)
    # a bush grows inside, the old hearth-cauldron, and the watchman's chest
    x, y = Fr.world(80.0, 90.0)
    clutter(N + "Bush_Common", x, y, s=0.9, cull=9000.0)
    x, y = Fr.world(-120.0, 120.0)
    put(P + "Cauldron", x, y, sink=8.0, yaw=R.uniform(0, 360), cull=9000.0)
    x, y = Fr.world(130.0, -130.0)
    C.loot_chest(x, y, gz(x, y), yaw=yaw + 225.0, table="Chest", name="Watchman's Chest", path=CHEST_WOOD)
    x, y = Fr.world(-140.0, -130.0)
    breakable(P + "Crate_Wooden", x, y)
    x, y = Fr.world(-40.0, -150.0)
    woodpile(x, y, yaw, n=3, tiers=2, length=0.4)
    flowers(cx, cy, 700.0, 10)


def fisher_corner_east():
    """Little beach west of the headland: a boat on trestles being tarred, rods, crab pots, a fish bucket."""
    cx, cy = 4950.0, 5350.0
    poi("Fisherman's corner (headland beach)", cx, cy)
    yaw = 70.0
    z = C.ground_min(cx, cy, 150.0)
    for d in (-100.0, 100.0):
        x, y = cx + d * math.cos(math.radians(yaw)), cy + d * math.sin(math.radians(yaw))
        put(P + "Stool", x, y, gz(x, y) - 5.0, yaw=yaw, cull=8000.0)
    put(WP + "SM_KG_Rowboat", cx, cy, z + 118.0, yaw=yaw, roll=180.0, cull=0.0)    # upturned on trestles
    for k, (ox, oy) in enumerate(((180.0, -160.0), (230.0, -60.0), (200.0, -110.0))):
        x, y = cx + ox, cy + oy
        put(P + "Cage_Small", x, y, gz(x, y) + (85.0 if k == 2 else 0.0) - 3.0, yaw=R.uniform(0, 360), s=0.9,
            collide=k < 2, cull=8000.0)
    x, y = cx - 170.0, cy - 120.0
    put(P + "Barrel", x, y, yaw=0.0, cull=9000.0)
    for k in range(4):
        clutter(WP + "SM_KG_FishingRod", x + R.uniform(-8, 8), y + R.uniform(-8, 8), gz(x, y) + 85.0,
                yaw=k * 90.0 + R.uniform(-20, 20), pitch=R.uniform(62, 75), cull=6000.0)
    x, y = cx - 60.0, cy - 190.0
    clutter(P + "Bucket_Wooden_1", x, y)
    for k in range(3):
        clutter(WP + "SM_KG_Fish_Mackerel", x + R.uniform(-8, 8), y + R.uniform(-8, 8), gz(x, y) + 25.0,
                pitch=R.uniform(40, 70), cull=4500.0)
    x, y = cx + 60.0, cy - 210.0
    clutter(P + "Pot_1_Lid", x, y)                      # the tar pot
    woodpile(x + 60.0, y - 40.0, 20.0, n=3, tiers=2, length=0.4)
    x, y = cx + 120.0, cy + 160.0
    clutter(WP + "SM_KG_Oar", x, y, gz(x, y) + 3.0, yaw=yaw + 10.0)
    seat(P + "Stool", cx - 220.0, cy - 20.0, None, face=yaw - 60.0, height=58.2)
    breakable(P + "Crate_Wooden", cx - 250.0, cy + 110.0)
    driftwood_line([(4600.0, 5750.0), (5200.0, 5900.0)], 2)
    pebbles(cx, cy, 500.0, 18)


def headland_rocks():
    """Boulders along the headland waterline and the north cliff foot."""
    placed = 0
    for k in range(60):
        a = R.uniform(math.radians(-20), math.radians(200))
        r = R.uniform(1300.0, 2300.0)
        x, y = LH[0] + r * math.cos(a), LH[1] + r * math.sin(a) * 0.9
        g = gz(x, y)
        if not (-150.0 < g < 120.0) or not C.in_zone(x, y) or C.overlaps(x, y, 150.0):
            continue
        s = R.uniform(0.7, 1.8)
        rock(x, y, s=s, sink=0.35)
        C.claim(x, y, 140.0 * s)
        placed += 1
        if placed >= 22:
            break


def fishing_ledge():
    """A path from the gap in the cliff fence down the north slope to a flat rock ledge at the water."""
    poi("Fishing ledge below the lighthouse", 7000.0, 7380.0)
    pts = [(7010.0, 6330.0), (6990.0, 6650.0), (7030.0, 6950.0), (7000.0, 7250.0)]
    trail(pts, step=110.0, wide=True)
    prev = None
    for k in range(9):                      # rope rail on stakes down the east side of the path
        t = k / 8.0
        y = 6380.0 + t * 880.0
        x = 7140.0 + 20.0 * math.sin(t * 3.0)
        z = gz(x, y)
        beam((x, y, z - 20.0), (x, y, z + 95.0), thick=0.45)
        top = (x, y, z + 88.0)
        if prev:
            beam(prev, top, thick=0.14, path=LINE, collide=False, cull=8000.0)
        prev = top
    # the ledge: flat slabs at the waterline, a stool, rods, a bucket of the day's catch, a lantern
    for x, y, s in ((7000.0, 7390.0, (0.9, 0.8, 0.25)), (6820.0, 7330.0, (0.6, 0.6, 0.3)),
                    (7190.0, 7360.0, (0.7, 0.6, 0.28)), (7060.0, 7520.0, (0.8, 0.7, 0.35))):
        rock(x, y, s=s, sink=0.2, path=N + "Rock_Medium_3")
    lz = C.ground_min(7000.0, 7390.0, 150.0) + 45.0
    seat(P + "Stool", 6960.0, 7370.0, lz - 8.0, face=90.0, height=58.2)
    for k, (x, y, yaw) in enumerate(((7050.0, 7440.0, 80.0), (6900.0, 7420.0, 100.0))):
        clutter(WP + "SM_KG_FishingRod", x, y, lz + 10.0, yaw=yaw, pitch=28.0, cull=6000.0)
    clutter(P + "Bucket_Wooden_1", 7040.0, 7360.0, lz - 6.0)
    for k in range(3):
        clutter(WP + "SM_KG_Fish_Cod", 7040.0 + R.uniform(-6, 6), 7360.0 + R.uniform(-6, 6), lz + 18.0,
                pitch=R.uniform(50, 80), cull=4500.0)
    clutter(P + "Rope_1", 6930.0, 7300.0, lz - 8.0)
    x, y = 6880.0, 7300.0
    beam((x, y, gz(x, y) - 20.0), (x, y, gz(x, y) + 160.0), thick=0.5)
    clutter(P + "Lantern_Wall", x + 10.0, y, gz(x, y) + 40.0, yaw=0.0, cull=6000.0)


def net_yard():
    """Lobster-pot maker's yard west of the lighthouse lane: a loaded cart, pot stacks, workbench, rope."""
    cx, cy = 5100.0, 3300.0
    poi("Pot-maker's yard", cx, cy)
    C.clear_grass(cx, cy, 380.0)
    put(V + "Prop_Wagon", cx - 150.0, cy + 120.0, C.ground_min(cx - 150.0, cy + 120.0, 150.0), yaw=75.0, cull=12000.0)
    for k in range(3):                       # pots on the cart bed
        clutter(P + "Cage_Small", cx - 150.0 + (k - 1) * 70.0, cy + 110.0 + R.uniform(-10, 10),
                C.ground_min(cx - 150.0, cy + 120.0, 150.0) + 95.0, yaw=R.uniform(0, 360), s=0.8, cull=8000.0)
    for k in range(6):                       # stacked pots against the bench
        x, y = cx + 180.0 + (k % 3) * 80.0, cy - 120.0
        put(P + "Cage_Small", x, y, gz(x, y) - 3.0 + (k // 3) * 80.0, yaw=R.uniform(-8, 8), s=0.9,
            collide=k < 3, cull=8000.0)
    put(P + "Workbench", cx + 120.0, cy + 60.0, C.ground_min(cx + 120.0, cy + 60.0, 100.0), yaw=180.0, cull=10000.0)
    wz = C.ground_min(cx + 120.0, cy + 60.0, 100.0) + 89.0
    for path, ox, oy in ((P + "Rope_1", -40, 0), (P + "Rope_2", 50, 10), (P + "Axe_Bronze", 10, -20)):
        clutter(path, cx + 120.0 + ox, cy + 60.0 + oy, wz, cull=5000.0)
    seat(P + "Stool", cx + 120.0, cy - 30.0, None, face=90.0, height=58.2)
    for k, (x, y) in enumerate(((cx - 280.0, cy - 150.0), (cx - 200.0, cy - 230.0))):
        breakable(P + ("Barrel" if k == 0 else "Crate_Wooden"), x, y)
    clutter(P + "Rope_3", cx + 20.0, cy - 200.0)
    clutter(P + "Bucket_Wooden_1", cx - 60.0, cy - 160.0)
    woodpile(cx + 350.0, cy + 150.0, 0.0, n=4, tiers=2, length=0.6)
    flowers(cx, cy, 600.0, 8)


def picnic():
    cx, cy = 9700.0, 4280.0
    poi("Picnic on the headland meadow", cx, cy)
    C.clear_grass(cx, cy, 160.0)
    clutter(IP + "SM_KG_Rug_Rect", cx, cy, gz(cx, cy) + 1.5, yaw=25.0, s=(0.9, 0.9, 1.0), cull=6000.0)
    z = gz(cx, cy) + 2.0
    for path, ox, oy in ((P + "Bag", -60, 30), (P + "FarmCrate_Apple", 40, -20), (IP + "SM_KG_Bread", 10, 30),
                         (IP + "SM_KG_Cheese", -20, -30), (KIT + "Kitchen_Bottle", 60, 40), (P + "Mug", 30, 55),
                         (P + "Mug", -40, -50), (KIT + "Kitchen_Plate", -5, 0)):
        clutter(path, cx + ox, cy + oy, z, cull=4500.0)
    flowers(cx, cy, 350.0, 8)


def sailors_rest():
    """Graves of drowned fishermen on the slope above the west beach: mounds, oars as markers, crosses, candles."""
    cx, cy = -6600.0, 4250.0
    poi("Drowned Men's Rest (sailors' graves)", cx, cy)
    C.clear_grass(cx, cy, 520.0)
    k = 0
    for row in range(2):
        for col in range(4):
            x = cx - 330.0 + col * 220.0 + R.uniform(-20, 20)
            y = cy - 120.0 + row * 300.0
            g = gz(x, y)
            inst(WP + "SM_KG_DigMound", x, y, g - 12.0, 90.0 + R.uniform(-5, 5), (0.75, 1.6, 0.55), collide=False,
                 cull=8000.0)
            hx, hy = x, y + 110.0                                 # head of the grave: toward the sea
            hz = gz(hx, hy)
            if k % 3 == 1:
                clutter(WP + "SM_KG_Oar", hx, hy, hz + 175.0, yaw=R.uniform(0, 360), pitch=-86.0, cull=8000.0)
            else:
                beam((hx, hy, hz - 25.0), (hx, hy, hz + 115.0), thick=0.4)
                beam((hx - 38.0, hy, hz + 78.0), (hx + 38.0, hy, hz + 78.0), thick=0.32, xhint=(0.0, 0.0, 1.0))
            if k % 2 == 0:
                clutter(P + "Candle_2", x + 30.0, y + 80.0, hz, cull=4500.0)
            clutter(R.choice([N + "Flower_3_Single", N + "Petal_2", N + "Petal_4"]), x - 20.0, y + 40.0, g - 2.0,
                    s=0.6, cull=5000.0)
            k += 1
    # low stone kerb round the plot, a mourning bench, a lantern post, a ship's bell memorial
    for i in range(22):
        a = i * 2 * math.pi / 22
        x, y = cx + 560.0 * math.cos(a), cy + 380.0 * math.sin(a)
        clutter(STEPS[i % 3], x, y, gz(x, y) - 6.0, yaw=math.degrees(a), s=(0.5, 0.5, 2.2), cull=7000.0)
    seat(P + "Bench", cx + 60.0, cy + 470.0, C.ground_min(cx + 60.0, cy + 470.0, 100.0) + 2.0, face=270.0,
         height=49.6, bench=True)
    mx, my = cx + 430.0, cy + 180.0
    mz = gz(mx, my)
    beam((mx - 50.0, my, mz - 20.0), (mx - 50.0, my, mz + 170.0), thick=0.6)
    beam((mx + 50.0, my, mz - 20.0), (mx + 50.0, my, mz + 170.0), thick=0.6)
    beam((mx - 62.0, my, mz + 165.0), (mx + 62.0, my, mz + 165.0), thick=0.55, xhint=(0.0, 0.0, 1.0))
    put(P + "Bucket_Metal", mx, my, mz + 160.0, roll=180.0, s=1.2, collide=False, cull=9000.0)
    clutter(P + "Candle_2", mx - 20.0, my + 25.0, mz)
    clutter(P + "Candle_1", mx + 18.0, my + 20.0, mz)
    flowers(mx, my, 120.0, 5, kinds=[N + "Flower_3_Group", N + "Petal_5", N + "Petal_2"])


def smokehouse():
    """Fish smokehouse lean-to on the slope: fish hanging from the rafters, a smouldering pit, barrels."""
    S, base = lean_to(-5700.0, 4650.0, (-5700.0, 3600.0), w=360.0, d=220.0, h_front=240.0, h_back=210.0)
    poi("Fish smokehouse", S.x, S.y)
    fish = ["SM_KG_Fish_Cod", "SM_KG_Fish_Salmon", "SM_KG_Fish_Mackerel"]
    for row, ly in enumerate((-40.0, 40.0)):
        a = S.world(-160.0, ly)
        b = S.world(160.0, ly)
        zz = base + 205.0
        beam((a[0], a[1], zz), (b[0], b[1], zz), thick=0.12, path=LINE, collide=False, cull=8000.0)
        for k in range(7):
            x, y = S.world(-135.0 + k * 45.0, ly)
            clutter(WP + fish[(k + row) % 3], x, y, zz - 30.0, yaw=S.yaw, pitch=88.0, cull=6000.0)
    x, y = S.world(0.0, 0.0)
    for i in range(8):
        a = i * 2 * math.pi / 8
        clutter(STEPS[i % 3], x + 60.0 * math.cos(a), y + 60.0 * math.sin(a), gz(x, y) - 4.0, s=(0.3, 0.3, 1.4))
    flame(x, y, gz(x, y) - 12.0, 0.4)
    for k, lx in enumerate((-190.0, 200.0)):
        px, py = S.world(lx, 60.0)
        put(PBARRELS[k * 6 + 1], px, py, gz(px, py) - 3.0, yaw=R.uniform(0, 360), s=0.85, cull=10000.0)
    px, py = S.world(150.0, -170.0)
    breakable(P + "Crate_Wooden", px, py)
    px, py = S.world(-150.0, -175.0)
    clutter(P + "FarmCrate_Empty", px, py)
    woodpile(*S.world(-60.0, 150.0), S.yaw, n=4, tiers=2, length=0.45)
    C.ambient("A_Fire_Loop", x, y, gz(x, y) + 60.0, 0.4)


def shipwright():
    """A new boat on the stocks near the tide line: fresh keel and ribs, planks and tools around it."""
    bow, stern = (-9150.0, 4920.0), (-9750.0, 5100.0)
    poi("Shipwright's slip (boat under construction)", -9450.0, 5010.0)
    C.claim(-9450.0, 5010.0, 450.0)
    W = wreck(bow, stern, -60.0, -60.0, 0.0, 250.0, 180.0, 7, "Kittiwake", breach=(2.0, 2.0), mast=False,
              deck=False, seed=23, planks=0.35)
    for t in (0.2, 0.5, 0.8):                    # the stocks holding it up
        p = W(t, 0.0, -10.0)
        put(P + "Stool", p[0], p[1], gz(p[0], p[1]) - 2.0, yaw=R.uniform(0, 360), s=(1.2, 1.2, 1.0), cull=9000.0)
    for k in range(5):                           # a stack of fresh planks
        x, y = -9500.0 + 20.0 * k, 4700.0
        inst(LOOSE_PLANK[k % 2], x, y, gz(x, y) + 4.0 + k * 10.0, 10.0 + R.uniform(-3, 3), 1.0, collide=False,
             cull=9000.0)
    put(P + "Workbench", -9250.0, 4700.0, C.ground_min(-9250.0, 4700.0, 110.0), yaw=15.0, cull=10000.0)
    wz = C.ground_min(-9250.0, 4700.0, 110.0) + 89.0
    for path, ox, oy in ((P + "Axe_Bronze", -30, 0), (P + "Rope_1", 40, 10), (P + "Pot_1_Lid", 70, -10)):
        clutter(path, -9250.0 + ox, 4700.0 + oy, wz, cull=5000.0)
    put(P + "Barrel", -9650.0, 4760.0, cull=9000.0)
    clutter(WP + "SM_KG_Oar", -9800.0, 4850.0, None, yaw=40.0)
    clutter(P + "Bucket_Wooden_1", -9150.0, 5150.0)


def west_fishing_spot():
    cx, cy = -7550.0, 5520.0
    poi("Surf-casting spot", cx, cy)
    for k in range(3):
        x, y = cx + (k - 1) * 90.0, cy + 60.0
        clutter(WP + "SM_KG_FishingRod", x, y, gz(x, y) + 5.0, yaw=90.0 + R.uniform(-10, 10), pitch=55.0, cull=7000.0)
        beam((x + 20.0, y, gz(x, y) - 20.0), (x + 20.0, y, gz(x, y) + 50.0), thick=0.15, path=LINE, collide=False,
             cull=6000.0)
    seat(P + "Stool", cx, cy - 80.0, None, face=90.0, height=58.2)
    clutter(P + "Bucket_Metal", cx + 90.0, cy - 60.0)
    clutter(WP + "SM_KG_Fish_Mackerel", cx + 95.0, cy - 55.0, gz(cx, cy) + 25.0, pitch=70.0, cull=4500.0)
    clutter(P + "Bag", cx - 100.0, cy - 90.0)


def sand_castle():
    cx, cy = -9050.0, 5480.0
    poi("Sand castle", cx, cy)
    z = gz(cx, cy)
    inst(WP + "SM_KG_DigMound", cx, cy, z - 6.0, 0.0, (1.3, 1.3, 1.4), collide=False, cull=8000.0)
    for k in range(4):
        a = math.radians(k * 90.0 + 45.0)
        x, y = cx + 55.0 * math.cos(a), cy + 55.0 * math.sin(a)
        inst(WP + "SM_KG_DigMound", x, y, z + 18.0, 0.0, (0.3, 0.3, 1.2), collide=False, cull=8000.0)
    clutter(WP + "SM_KG_Shovel", cx + 120.0, cy - 40.0, z + 4.0, yaw=200.0)
    clutter(P + "Bucket_Wooden_1", cx - 110.0, cy + 30.0, z - 2.0, roll=180.0)
    clutter(PIR + "Broken_Flag_0", cx, cy, z + 30.0, s=0.2, cull=5000.0)


def fresh_catch_stall():
    cx, cy = -4950.0, 4650.0
    poi("Fresh-catch cart", cx, cy)
    C.clear_grass(cx, cy, 250.0)
    z = C.ground_min(cx, cy, 150.0)
    put(P + "Stall_Cart_Empty", cx, cy, z, yaw=160.0, cull=12000.0)
    c, sn = math.cos(math.radians(160.0)), math.sin(math.radians(160.0))
    for k, path in enumerate([WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Mackerel",
                              WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Salmon"]):
        lx = -150.0 + k * 45.0
        x, y = cx + lx * c, cy + lx * sn
        clutter(path, x, y, z + 92.0, yaw=160.0 + 90.0 + R.uniform(-15, 15), cull=5000.0)
    x, y = cx - 60.0 * sn, cy + 60.0 * c
    clutter(P + "FarmCrate_Empty", x, y)
    breakable(P + "Barrel", cx + 200.0, cy - 110.0)
    seat(P + "Stool", cx + 80.0 * sn, cy - 80.0 * c, None, face=160.0 - 90.0, height=58.2)


def ladder(x, y, z, yaw, height):
    """KGLadder climb volume + rails/rungs. The climber stands on the ladder's -X side (after yaw)."""
    lad = C.spawn_class("/Script/KillGodot.KGLadder", x, y, z, yaw, sub="Gameplay")
    if lad:
        try:
            lad.set_height(height)
        except Exception as e:
            unreal.log_warning(f"KG_DRESS ladder: {e}")
    c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for side in (-28.0, 28.0):
        bx, by = x - sn * side, y + c * side
        beam((bx, by, z - 10.0), (bx, by, z + height + 10.0), thick=0.35)
    rung = 30.0
    while rung < height:
        a = (x - sn * -28.0, y + c * -28.0, z + rung)
        b = (x - sn * 28.0, y + c * 28.0, z + rung)
        beam(a, b, thick=0.22, path=LINE, collide=False, cull=9000.0)
        rung += 38.0
    return lad


def beacon_tower():
    """Timber signal beacon on the west knoll: 4 legs, braces, a railed platform with a burning brazier, a ladder."""
    cx, cy, yaw = -10350.0, 3470.0, 20.0
    poi("Signal beacon tower (west knoll)", cx, cy)
    C.clear_grass(cx, cy, 260.0)
    F = C.Frame(cx, cy, yaw, 240.0, 240.0)
    top = max(gz(*F.world(lx, ly)) for lx in (-120.0, 120.0) for ly in (-120.0, 120.0)) + 520.0
    legs = []
    for lx, ly in ((-120.0, -120.0), (120.0, -120.0), (120.0, 120.0), (-120.0, 120.0)):
        bx, by = F.world(lx * 1.25, ly * 1.25)
        tx, ty = F.world(lx * 0.95, ly * 0.95)
        legs.append(((bx, by, gz(bx, by) - 30.0), (tx, ty, top - 4.0)))
        beam((bx, by, gz(bx, by) - 30.0), (tx, ty, top + 110.0), thick=1.1)
    for i in range(4):                       # cross braces on two levels
        (a0, a1), (b0, b1) = legs[i], legs[(i + 1) % 4]
        for f0, f1 in ((0.1, 0.5), (0.5, 0.9)):
            pa = vadd(vmul(a0, 1 - f0), vmul(a1, f0))
            pb = vadd(vmul(b0, 1 - f1), vmul(b1, f1))
            beam(pa, pb, thick=0.45, xhint=(0.0, 0.0, 1.0))
    for lx in (-60.0, 60.0):                 # platform boards
        for ly in (-60.0, 60.0):
            x, y = F.world(lx, ly)
            inst(HULL_PLANK, x, y, top, yaw, (0.6, 0.6, 4.0), collide=True, cull=0.0)
    for k in range(3):                       # railing on three sides; the ladder side stays open
        for lx, ly, wy in ((-80.0 + k * 80.0, -122.0, 0.0), (-80.0 + k * 80.0, 122.0, 0.0), (-122.0, -80.0 + k * 80.0, 90.0)):
            x, y = F.world(lx, ly)
            inst(RAIL, x, y, top + 4.0, yaw + wy, (0.8, 1.0, 1.0), collide=True)
    x, y = F.world(0.0, 0.0)
    for k in range(3):
        a = math.radians(k * 120.0)
        beam((x + 40.0 * math.cos(a), y + 40.0 * math.sin(a), top + 4.0), (x, y, top + 85.0), thick=0.3)
    put(P + "Cauldron", x, y, top + 45.0, s=0.55, collide=False, cull=0.0)
    flame(x, y, top + 75.0, 0.9)
    light(x, y, top + 170.0, 16.0, 1400.0, color=(255, 140, 60))
    # ladder up the open (+X) face
    lx, ly = F.world(150.0, 0.0)
    ladder(lx, ly, gz(lx, ly), yaw + 180.0, top - gz(lx, ly) + 10.0)
    # firewood waiting at the foot, a bucket of water for when it gets out of hand
    woodpile(*F.world(-60.0, -230.0), yaw, n=4, tiers=3, length=0.5)
    clutter(P + "Bucket_Wooden_1", *F.world(200.0, -150.0))
    mover_off(PIR + "FlagLow_1", *F.world(-120.0, -120.0), z=top + 110.0, yaw=yaw, sway=6.0, sway_hz=0.55)


def tinker_camp():
    """A travelling tinker's camp on the west slope: stall-tent, bedroll, pots and pans, a cart, a fire."""
    cx, cy = -8950.0, 4150.0
    poi("Tinker's camp", cx, cy)
    C.clear_grass(cx, cy, 380.0)
    z = C.ground_min(cx, cy, 130.0)
    put(P + "Stall_Empty", cx, cy, z, yaw=200.0, cull=12000.0)
    c, sn = math.cos(math.radians(200.0)), math.sin(math.radians(200.0))

    def L(lx, ly):
        return cx + lx * c - ly * sn, cy + lx * sn + ly * c

    for path, lx, ly in ((P + "Cauldron", -40.0, 10.0), (P + "Pot_1", 40.0, 5.0), (KIT + "Kitchen_SmallPan", 60.0, 30.0),
                         (P + "Vase_4", -70.0, 20.0), (P + "Bottle_1", 10.0, 25.0)):
        x, y = L(lx, ly)
        clutter(path, x, y, z + 75.0 if "Cauldron" not in path else None, s=0.5 if "Cauldron" in path else 1.0)
    x, y = L(0.0, 190.0)
    clutter(IP + "SM_KG_Rug_Runner", x, y, gz(x, y) + 1.0, yaw=200.0 + 90.0, cull=6000.0)   # bedroll
    clutter(P + "Bag", *L(-110.0, 230.0))
    fx, fy = L(170.0, -170.0)
    for i in range(8):
        a = i * 2 * math.pi / 8
        clutter(STEPS[i % 3], fx + 65.0 * math.cos(a), fy + 65.0 * math.sin(a), gz(fx, fy) - 4.0, s=(0.35, 0.35, 1.5))
    flame(fx, fy, gz(fx, fy) - 8.0, 0.5)
    seat(P + "Stool", fx - 150.0, fy + 20.0, None, face=0.0, height=58.2)
    x, y = L(-260.0, -60.0)
    put(V + "Prop_Wagon", x, y, C.ground_min(x, y, 160.0), yaw=110.0, cull=12000.0)
    for k in range(3):
        clutter(PIR + ("crates_0" if k % 2 else "crates_1"), x + (k - 1) * 60.0, y + R.uniform(-15, 15),
                C.ground_min(x, y, 160.0) + 95.0, s=0.7, cull=8000.0)
    breakable(P + "Crate_Wooden", *L(120.0, 120.0))
    breakable(P + "Barrel", *L(200.0, 60.0))
    put(P + "Cage_Small", *L(-150.0, 90.0), yaw=30.0, s=0.8, cull=8000.0)
    flowers(cx, cy, 700.0, 10)


def boat_graveyard():
    """Old boats left to rot on the grass above the tide: upturned hulls, a rib skeleton, weeds through them."""
    cx, cy = -11050.0, 4550.0
    poi("Boat graveyard", cx, cy)
    for k, (ox, oy, yaw, roll, pitch) in enumerate(((0.0, 0.0, 30.0, 180.0, 6.0), (380.0, 160.0, 75.0, 172.0, -4.0),
                                                     (-360.0, 220.0, 150.0, 12.0, 3.0))):
        x, y = cx + ox, cy + oy
        z = gz(x, y)
        put(WP + "SM_KG_Rowboat", x, y, z + (48.0 if roll > 90 else 5.0), yaw=yaw, roll=roll, pitch=pitch, cull=0.0)
        for j in range(3):
            clutter(N + R.choice(["Grass_Wispy_Tall", "Fern_1", "Grass_Common_Tall"]), x + R.uniform(-120, 120),
                    y + R.uniform(-80, 80), s=R.uniform(0.5, 0.8))
    wreck((cx + 150.0, cy - 350.0), (cx - 450.0, cy - 200.0), 15.0, 15.0, 6.0, 190.0, 140.0, 6, "Puffin",
          breach=(0.35, 0.7), mast=False, deck=False, seed=31, planks=0.25)
    for k in range(4):
        clutter(WP + "SM_KG_Oar", cx + R.uniform(-400, 400), cy + R.uniform(-300, 300), None, yaw=R.uniform(0, 360))
    clutter(P + "Chain_Coil", cx + 200.0, cy - 120.0)
    breakable(P + "Barrel", cx - 200.0, cy - 60.0)


def old_battery():
    """The Old Battery: two cannons behind a low stone wall on the saddle above the east cove, shot and powder."""
    cx, cy = 11750.0, 4430.0
    poi("The Old Battery (cannons over the cove)", cx, cy)
    C.clear_grass(cx, cy, 480.0)
    base = C.ground_min(cx, cy, 300.0)
    # a crescent of low wall facing the sea (north)
    for k in range(7):
        a = math.radians(40.0 + k * 16.7)
        x, y = cx + 430.0 * math.cos(a), cy + 430.0 * math.sin(a)
        put(V + "Wall_UnevenBrick_Straight", x, y, gz(x, y) - 20.0, yaw=math.degrees(a) + 90.0, s=(0.55, 1.0, 0.3),
            cull=14000.0)
    for k, ox in enumerate((-160.0, 170.0)):
        x, y = cx + ox, cy + 170.0
        put(PIR + "cannon_0", x, y, gz(x, y) - 5.0, yaw=90.0 + R.uniform(-8, 8), s=0.7, cull=0.0)
    for k, (ox, oy) in enumerate(((-20.0, -60.0), (40.0, -110.0))):      # shot pyramids
        clutter(PIR + "Cannon_Ball_1", cx + ox, cy + oy, None, s=0.55, cull=8000.0)
    for k, (ox, oy) in enumerate(((-320.0, -60.0), (-260.0, -150.0), (300.0, -80.0))):   # powder kegs
        if k == 1:
            breakable(P + "Barrel", cx + ox, cy + oy)
        else:
            put(PBARRELS[(k * 5 + 3) % 14], cx + ox, cy + oy, gz(cx + ox, cy + oy) - 3.0, yaw=R.uniform(0, 360), s=0.8,
                cull=10000.0)
    put(PIR + "FlagTall_1", cx + 380.0, cy - 160.0, gz(cx + 380.0, cy - 160.0) - 40.0, s=(1.0, 1.0, 0.7), cull=0.0)
    mover_off(PIR + "FlagTall_0", cx + 470.0, cy - 160.0, gz(cx + 380.0, cy - 160.0) + 520.0, yaw=0.0, sway=5.0,
              sway_hz=0.5)
    put(P + "WeaponStand", cx - 120.0, cy - 250.0, yaw=0.0, cull=10000.0)
    seat(P + "Bench", cx + 150.0, cy - 260.0, C.ground_min(cx + 150.0, cy - 260.0, 100.0) + 2.0, face=90.0,
         height=49.6, bench=True)


# ============================================================================================ SEA
def sea_stacks():
    stacks = [  # (x, y, footprint scale, height scale, crown)
        (7700.0, 8700.0, 2.6, 4.2, True), (8650.0, 9350.0, 1.9, 3.0, False), (9400.0, 8600.0, 1.5, 2.4, False),
        (12400.0, 8700.0, 2.2, 3.6, True), (11600.0, 9400.0, 1.4, 2.0, False),
        (-9300.0, 8300.0, 2.5, 4.4, True), (-10500.0, 9100.0, 1.8, 2.8, False), (-7900.0, 8900.0, 1.5, 2.2, False),
        (-12300.0, 8600.0, 2.0, 3.2, False), (-5600.0, 8600.0, 1.3, 1.8, False),
    ]
    for x, y, fs, hs, crown in stacks:
        if not C.in_zone(x, y):
            continue
        g = gz(x, y)
        rock(x, y, z=g, s=(fs, fs * 0.9, hs), sink=0.05, path=N + "Rock_Medium_1")
        rock(x + 60.0, y - 40.0, z=g + 190.0 * hs * 0.55, s=(fs * 0.8, fs * 0.75, hs * 0.55), sink=0.0,
             path=N + "Rock_Medium_3")
        for k in range(R.randint(3, 5)):          # skirt of boulders at the waterline
            a = R.uniform(0, 2 * math.pi)
            d = 150.0 * fs * R.uniform(1.0, 1.4)
            rock(x + d * math.cos(a), y + d * math.sin(a), z=g, s=R.uniform(0.6, 1.1), sink=0.1)
        if crown:                                 # a wind-bent pine clinging to the top
            top = g + 190.0 * hs * 0.55 + 190.0 * hs * 0.55 - 40.0
            inst(N + "Pine_3", x + 40.0, y - 20.0, top, R.uniform(0, 360), R.uniform(0.45, 0.6), R.uniform(-8, 8),
                 R.uniform(-8, 8), collide=False, cull=0.0)
        poi("Sea stack", x, y)
    # waterline rocks off both beaches
    for x0, x1, y0, y1, n in ((8600.0, 13000.0, 6600.0, 7800.0, 8), (-12000.0, -4800.0, 6000.0, 7200.0, 12)):
        placed = 0
        for _ in range(n * 10):
            x, y = R.uniform(x0, x1), R.uniform(y0, y1)
            g = gz(x, y)
            if not (-230.0 < g < -60.0) or not C.in_zone(x, y) or C.overlaps(x, y, 250.0):
                continue
            s = R.uniform(0.9, 1.7)
            rock(x, y, z=g, s=(s, s, s * R.uniform(1.0, 1.6)), sink=0.1)
            C.claim(x, y, 200.0)
            placed += 1
            if placed >= n:
                break
    C.ambient("A_Gulls_Loop", 8200.0, 8900.0, 800.0, 0.8)
    C.ambient("A_Gulls_Loop", -9300.0, 8300.0, 800.0, 0.8)


def bell_buoy(x, y, lamp=False):
    poi("Bell buoy", x, y)
    base = C.mover(PIR + "Barrel_13", x, y, -55.0, yaw=R.uniform(0, 360), scale=(1.5, 1.5, 1.0), bob=14.0,
                   sway=5.0, sway_hz=0.28)
    if not base:
        return
    parts = []
    top = (x, y, 60.0 + 170.0)
    for k in range(3):
        a = math.radians(k * 120.0 + 15.0)
        foot = (x + 55.0 * math.cos(a), y + 55.0 * math.sin(a), 60.0)
        d = vsub(top, foot)
        roll, pitch, yaw = rot_zx(d, (1.0, 0.0, 0.0))
        parts.append(put(BEAM, foot[0], foot[1], foot[2], yaw=yaw, pitch=pitch, roll=roll,
                         s=(0.45, 0.45, vlen(d) / 300.0), sub="Motion", collide=False, cull=0.0))
    parts.append(put(P + "Bucket_Metal", x, y, top[2] + 30.0, roll=180.0, s=1.4, sub="Motion", collide=False,
                     cull=0.0))
    parts.append(put(PIR + "FlagTall_0", x, y, top[2] + 25.0, s=(0.35, 1.0, 0.35), sub="Motion", collide=False,
                     cull=0.0))
    parts.append(put(PIR + "Barrel_2", x, y, -10.0, s=(1.05, 1.05, 0.4), sub="Motion", collide=False, cull=0.0))
    attach(parts, base)
    if lamp:
        light(x, y, top[2] + 10.0, 6.0, 600.0, color=(255, 120, 80))


# ============================================================================================ WEST SHORE
def net_menders():
    """Fishermen's strand at the east end of the west beach: drying racks with fish, an upturned boat, pots."""
    cx, cy = -5400.0, 5150.0
    poi("Net-menders' strand", cx, cy)
    # fish drying rack: two A-frames and a pole, fish hanging
    a = (cx - 150.0, cy - 40.0)
    b = (cx + 190.0, cy + 20.0)
    for px, py in (a, b):
        z = gz(px, py)
        beam((px - 25.0, py, z - 15.0), (px, py, z + 200.0), thick=0.5)
        beam((px + 25.0, py, z - 15.0), (px, py, z + 200.0), thick=0.5)
    za, zb = gz(*a) + 195.0, gz(*b) + 195.0
    beam((a[0], a[1], za), (b[0], b[1], zb), thick=0.3, path=LINE, collide=False, cull=8000.0)
    fish = ["SM_KG_Fish_Cod", "SM_KG_Fish_Mackerel", "SM_KG_Fish_Salmon"]
    for k in range(9):
        t = (k + 0.5) / 9
        x, y, z = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, za + (zb - za) * t
        clutter(WP + fish[k % 3], x, y, z - 30.0, yaw=R.uniform(0, 360), pitch=88.0, cull=6000.0)
    # net-mending bench (seat) and baskets
    seat(P + "Bench", cx + 20.0, cy - 230.0, None, face=80.0, height=49.6, bench=True)
    x, y = cx - 250.0, cy - 200.0
    clutter(P + "Rope_3", x, y)
    clutter(P + "Rope_1", x + 60.0, y + 50.0)
    for k in range(4):     # stacked lobster pots
        x, y = cx + 330.0 + (k % 2) * 90.0, cy - 150.0
        put(P + "Cage_Small", x, y, gz(x, y) - 3.0 + (k // 2) * 82.0, yaw=R.uniform(-10, 10), s=0.9,
            collide=k < 2, cull=8000.0)
    # upturned boat as a shelter + oars
    x, y = cx - 480.0, cy + 120.0
    put(WP + "SM_KG_Rowboat", x, y, gz(x, y) + 45.0, yaw=20.0, roll=180.0, pitch=8.0, cull=0.0)
    clutter(WP + "SM_KG_Oar", x + 60.0, y - 120.0, gz(x + 60.0, y - 120.0) + 3.0, yaw=35.0)
    for path, ox, oy in ((P + "Bucket_Wooden_1", 150.0, 150.0), (P + "Barrel", 260.0, 90.0),
                         (P + "FarmCrate_Empty", 120.0, 230.0), (P + "Bag", -300.0, -60.0)):
        x, y = cx + ox, cy + oy
        if "Barrel" in path:
            breakable(path, x, y)
        else:
            clutter(path, x, y, cull=6000.0)
    x, y = cx + 100.0, cy + 300.0
    put(PIR + "Torch_0", x, y, cull=10000.0)
    light(x, y, gz(x, y) + 265.0, 8.0, 800.0)


def beached_boats():
    cx, cy = -6700.0, 5450.0
    poi("Beached boats", cx, cy)
    for k, (ox, oy, yaw, roll) in enumerate(((0.0, 0.0, 75.0, 9.0), (330.0, 120.0, 100.0, -12.0))):
        x, y = cx + ox, cy + oy
        put(WP + "SM_KG_Rowboat", x, y, gz(x, y) + 22.0, yaw=yaw, roll=roll, pitch=-3.0, cull=0.0)
        clutter(WP + "SM_KG_Oar", x + 40.0, y - 20.0, gz(x, y) + 40.0, yaw=yaw + 5.0, cull=6000.0)
    # a keg and nets, a mooring post and rope
    x, y = cx - 180.0, cy - 160.0
    beam((x, y, gz(x, y) - 30.0), (x, y, gz(x, y) + 110.0), thick=0.8)
    clutter(P + "Rope_2", x + 50.0, y + 10.0)
    breakable(P + "Barrel", cx + 150.0, cy - 220.0)
    clutter(P + "Bucket_Metal", cx + 230.0, cy - 160.0)
    clutter(WP + "SM_KG_FishingRod", cx - 60.0, cy - 150.0, gz(cx - 60.0, cy - 150.0) + 4.0, yaw=30.0)


def bonfire():
    cx, cy = -8400.0, 5230.0
    poi("Beach bonfire ring", cx, cy)
    z = gz(cx, cy)
    # stone ring and the fire
    for k in range(10):
        a = k * 2 * math.pi / 10
        x, y = cx + 95.0 * math.cos(a), cy + 95.0 * math.sin(a)
        clutter(STEPS[k % 3], x, y, gz(x, y) - 4.0, yaw=math.degrees(a), s=(0.45, 0.45, 1.6), cull=7000.0)
    C.claim(cx, cy, 650.0)
    for k in range(6):          # a teepee of split logs round the flames
        a = math.radians(k * 60.0 + 15.0)
        beam((cx + 55.0 * math.cos(a), cy + 55.0 * math.sin(a), z - 8.0), (cx + 8.0 * math.cos(a),
             cy + 8.0 * math.sin(a), z + 75.0), thick=0.5)
    flame(cx, cy, z - 5.0, 1.0)
    # a cauldron of fish stew on a tripod
    kx, ky = cx - 170.0, cy + 60.0              # the stew pot hangs on its own tripod beside the fire
    put(P + "Cauldron", kx, ky, gz(kx, ky) + 30.0, s=0.5, collide=False, cull=8000.0)
    for k in range(3):
        a = math.radians(k * 120.0)
        beam((kx + 70.0 * math.cos(a), ky + 70.0 * math.sin(a), gz(kx, ky) - 10.0), (kx, ky, gz(kx, ky) + 150.0),
             thick=0.3)
    flame(kx, ky, gz(kx, ky) - 5.0, 0.45)
    light(cx, cy, z + 90.0, 22.0, 1300.0, color=(255, 140, 60))
    C.ambient("A_Fire_Loop", cx, cy, z + 60.0, 0.9)
    # three bench seats round the fire facing it
    for k, a_deg in enumerate((200.0, 290.0, 20.0)):
        a = math.radians(a_deg)
        x, y = cx + 260.0 * math.cos(a), cy + 260.0 * math.sin(a)
        seat(P + "Bench", x, y, C.ground_min(x, y, 100.0) + 2.0, face=a_deg + 180.0, height=49.6, bench=True)
    # fish on sticks leaning over the fire
    for k in range(4):
        a = math.radians(110.0 + k * 22.0)
        x, y = cx + 120.0 * math.cos(a), cy + 120.0 * math.sin(a)
        clutter(WP + "SM_KG_Fish_Mackerel", x, y, z + 45.0, yaw=math.degrees(a) + 180.0, pitch=-35.0, cull=5000.0)
        beam((x, y, z - 10.0), (cx + 60.0 * math.cos(a), cy + 60.0 * math.sin(a), z + 70.0), thick=0.08,
             path=LINE, collide=False, cull=5000.0)
    # blanket, mugs, bottles, a guitar-less sing-along
    x, y = cx + 150.0, cy + 250.0
    clutter(IP + "SM_KG_Rug_Oval", x, y, gz(x, y) + 1.0, yaw=30.0, cull=6000.0)
    for path, ox, oy in ((P + "Mug", 20, 10), (P + "Mug", -30, -20), (P + "Bottle_1", 40, -30),
                         (KIT + "Kitchen_Bowl", -10, 35), (P + "Bread" if False else IP + "SM_KG_Bread", 55, 30)):
        clutter(path, x + ox, y + oy, gz(x, y) + 2.0, cull=4500.0)
    # tiki torches and a driftwood pile for later
    for a_deg in (65.0, 245.0):
        a = math.radians(a_deg)
        x, y = cx + 430.0 * math.cos(a), cy + 430.0 * math.sin(a)
        put(PIR + "Torch_0", x, y, yaw=a_deg, cull=10000.0)
    x, y = cx - 380.0, cy + 130.0
    for k in range(3):
        inst(R.choice(DRIFT), x + R.uniform(-40, 40), y + R.uniform(-40, 40), gz(x, y) + 15.0 + k * 12.0,
             R.uniform(0, 360), R.uniform(0.14, 0.2), 0.0, 90.0, collide=False, cull=10000.0)
    breakable(P + "Barrel", cx + 330.0, cy - 250.0)
    clutter(P + "Barrel_Apples", cx + 400.0, cy - 180.0, cull=8000.0)


def west_tide_pools():
    for cx, cy in ((-10050.0, 5700.0), (-10400.0, 5800.0), (-9700.0, 5760.0)):
        tide_pool(cx, cy, n=8, r=150.0)
    for x, y, s in ((-10250.0, 5600.0, 1.3), (-9850.0, 5950.0, 1.1), (-10600.0, 5650.0, 0.9)):
        rock(x, y, s=s, sink=0.35)


def west_smack():
    """Skeleton of an old fishing smack half in the water."""
    W = wreck((-11350.0, 5420.0), (-10850.0, 6120.0), 40.0, 25.0, -12.0, 280.0, 210.0, 8, "Mary", breach=(0.45, 0.6),
              mast=False, deck=False, seed=5)
    m = W(0.5, 0, 0)
    poi("Skeleton of the fishing smack", m[0], m[1])
    p = W(0.3, -260.0, 0.0)
    put(PBARRELS[6], p[0], p[1], gz(p[0], p[1]) - 30.0, pitch=80.0, yaw=R.uniform(0, 360), cull=9000.0)
    p = W(0.85, 200.0, 0.0)
    clutter(P + "Chain_Coil", p[0], p[1], cull=6000.0)


def lookout_cairn():
    cx, cy = -9950.0, 3700.0
    poi("Lookout cairn (west knoll)", cx, cy)
    z = gz(cx, cy)
    C.clear_grass(cx, cy, 220.0)
    for k, (s, dz) in enumerate(((0.55, 0.0), (0.42, 70.0), (0.3, 125.0), (0.2, 165.0))):
        rock(cx + R.uniform(-10, 10), cy + R.uniform(-10, 10), z=z + dz, s=(s, s, s * 0.8), sink=0.05)
    beam((cx, cy, z + 150.0), (cx, cy, z + 480.0), thick=0.45)
    mover_off(PIR + "FlagLow_1", cx + 70.0, cy, z + 360.0, yaw=0.0, sway=6.0, sway_hz=0.55)
    seat(P + "Bench", cx + 60.0, cy + 330.0, C.ground_min(cx + 60.0, cy + 330.0, 100.0) + 2.0, face=95.0,
         height=49.6, bench=True)
    flowers(cx, cy, 500.0, 10)


def smugglers_cove():
    """Sand strip at the foot of the west cliff: boulder wall with a hidden stash, a jetty, contraband, tally table."""
    cx, cy = -12700.0, 6600.0
    poi("Smuggler's cove", cx, cy)
    # boulder wall at the cliff foot; the stash nook sits between the two big ones, screened by a smaller rock
    for x, y, s in ((-13150.0, 5900.0, 2.4), (-13200.0, 6250.0, 2.8), (-13150.0, 6800.0, 2.6), (-13250.0, 7200.0, 3.0),
                    (-12930.0, 6440.0, 1.2), (-13050.0, 7550.0, 2.2), (-12950.0, 5750.0, 1.3)):
        rock(x, y, s=s, sink=0.3)
    C.loot_chest(-13110.0, 6530.0, gz(-13110.0, 6530.0) - 4.0, yaw=20.0, table="Chest", name="Smuggler's Stash",
                 path=PIR + "chest_diamond_0")
    put(P + "Lantern_Wall", -13060.0, 6620.0, gz(-13060.0, 6620.0) + 5.0, yaw=0.0, collide=False, cull=6000.0)
    light(-13040.0, 6590.0, gz(-13040.0, 6590.0) + 80.0, 5.0, 450.0)
    clutter(P + "Coin_Pile_2", -13030.0, 6480.0, cull=4000.0)
    # jetty from the dry sand out into the water, a rowboat tied up at its end
    deck = 100.0
    x0 = -12950.0
    for k, piece in enumerate(("pier_0", "pier_1")):
        put(PIR + piece, x0 + 244.0 + k * 488.0, 7000.0, deck - 140.0, yaw=0.0, cull=0.0)
    put(PIR + "Torch_1", x0 + 900.0, 6890.0, deck, cull=10000.0)
    light(x0 + 900.0, 6890.0, deck + 260.0, 9.0, 850.0)
    C.mover(WP + "SM_KG_Rowboat", x0 + 1100.0, 6830.0, 12.0, yaw=8.0, bob=5.0, sway=3.0, sway_hz=0.25)
    clutter(PIR + "crates_0", x0 + 350.0, 7010.0, deck, yaw=R.uniform(0, 360), s=0.8)
    clutter(PBARRELS[5], x0 + 460.0, 7020.0, deck, yaw=R.uniform(0, 360), s=0.8)
    clutter(P + "Rope_2", x0 + 820.0, 7080.0, deck + 1.0)
    # contraband on the sand: a crate stack under a tarp, rum barrels, smashable crates
    tz = gz(-12780.0, 6250.0)
    for k, (x, y, zz) in enumerate(((-12780.0, 6250.0, 0.0), (-12780.0, 6250.0, 70.0), (-12710.0, 6200.0, 0.0),
                                    (-12850.0, 6190.0, 0.0), (-12760.0, 6320.0, 0.0))):
        put(PIR + ("crates_0" if k % 2 else "crates_1"), x, y, tz - 3.0 + zz, yaw=R.uniform(-15, 15), cull=10000.0)
    put(IP + "SM_KG_Rug_Rect", -12775.0, 6245.0, tz + 142.0, yaw=10.0, s=(0.7, 0.9, 1.0), pitch=6.0, collide=False,
        cull=8000.0)
    for k, (x, y) in enumerate(((-12560.0, 6300.0), (-12500.0, 6380.0), (-12610.0, 6390.0))):
        put(PBARRELS[(k * 4 + 2) % 14], x, y, gz(x, y) - 4.0, yaw=R.uniform(0, 360), s=0.85, cull=10000.0)
    for k, (x, y) in enumerate(((-12640.0, 6080.0), (-12540.0, 6150.0), (-12860.0, 6060.0), (-12600.0, 7250.0),
                                (-12700.0, 7300.0))):
        breakable(P + ("Crate_Wooden" if k % 2 == 0 else "Barrel"), x, y)
    # the tally table
    tx, ty = -12720.0, 6760.0
    tz = C.ground_min(tx, ty, 140.0)
    put(P + "Table_Large", tx, ty, tz, yaw=95.0, s=(0.6, 1.0, 1.0), cull=9000.0)
    for path, ox, oy in ((P + "Scroll_2", 0, 0), (P + "Coin_Pile_2", 10, 40), (P + "Coin_Pile", -15, -35),
                         (P + "Bottle_1", 20, -60), (P + "CandleStick", -20, 55), (P + "Key_Gold", 5, 20)):
        clutter(path, tx + ox, ty + oy, tz + 81.0, cull=4500.0)
    seat(P + "Stool", tx - 110.0, ty - 10.0, None, face=0.0, height=58.2)
    for path, x, y in ((P + "Chain_Coil", -12420.0, 6500.0), (P + "Rope_3", -12450.0, 6650.0),
                       (P + "Cage_Small", -12900.0, 6950.0), (WP + "SM_KG_Shovel", -12620.0, 6520.0),
                       (WP + "SM_KG_DugHole", -12900.0, 6120.0), (WP + "SM_KG_DigMound", -12840.0, 6000.0)):
        clutter(path, x, y, cull=7000.0)
    put(PIR + "Torch_3", -12500.0, 6560.0, cull=10000.0)
    C.ambient("A_Lapping_Loop", -12200.0, 6900.0, 100.0, 0.9)


def west_shore_rest():
    driftwood_line([(-4800.0, 5620.0), (-7000.0, 5660.0), (-9000.0, 5620.0), (-11500.0, 5700.0)], 14)
    dune_grass(-12500.0, -4600.0, 4500.0, 5300.0, 26, zmin=190.0, zmax=320.0)
    # washed-up cargo and bits along the tide line
    for k in range(10):
        x, y = R.uniform(-11800.0, -4800.0), R.uniform(5450.0, 5720.0)
        if not C.in_zone(x, y) or C.overlaps(x, y, 150.0):
            continue
        g = gz(x, y)
        choice = k % 5
        if choice == 0:
            put(R.choice(PBARRELS), x, y, g - 30.0, yaw=R.uniform(0, 360), pitch=R.choice((0.0, 85.0)), s=0.85,
                cull=10000.0)
        elif choice == 1:
            clutter(PIR + "crates_1", x, y, g - 18.0, pitch=R.uniform(-20, 20), s=0.8, cull=8000.0)
        elif choice == 2:
            clutter(LOOSE_PLANK[k % 2], x, y, g - 2.0, s=R.uniform(0.6, 1.0), cull=8000.0)
        elif choice == 3:
            clutter(P + "Rope_1", x, y, g - 1.0, cull=5000.0)
        else:
            clutter(WP + "SM_KG_Fish_Cod", x, y, g + 3.0, roll=90.0, cull=4500.0)
        C.claim(x, y, 150.0)
    for k in range(8):
        pebbles(R.uniform(-12000.0, -5000.0), R.uniform(5300.0, 5700.0), 300.0, 8)


# ============================================================================================ entry
def dress():
    R.seed(20260924)
    POIS.clear()
    _batches.clear()
    _mode.clear()
    keeper_yard()
    viewpoint()
    lighthouse_path()
    trail([(6620.0, 3250.0), (7600.0, 3450.0), (8600.0, 3350.0), (9000.0, 3380.0)])
    trail([(9550.0, 3420.0), (10000.0, 3500.0)])
    trail([(10650.0, 3900.0), (10800.0, 4500.0), (10950.0, 5000.0), (11000.0, 5150.0)])
    ruined_watchhouse()
    standing_stones()
    old_battery()
    east_wreck()
    east_cove_rest()
    fisher_corner_east()
    headland_rocks()
    sea_stacks()
    bell_buoy(9500.0, 7900.0, lamp=True)
    bell_buoy(-6200.0, 7100.0)
    fishing_ledge()
    net_yard()
    picnic()
    net_menders()
    fresh_catch_stall()
    smokehouse()
    sailors_rest()
    beached_boats()
    west_fishing_spot()
    shipwright()
    sand_castle()
    bonfire()
    west_tide_pools()
    west_smack()
    lookout_cairn()
    beacon_tower()
    tinker_camp()
    boat_graveyard()
    smugglers_cove()
    west_shore_rest()
    flush()
    C.stats["pois"] = len(POIS)
    for name, x, y in POIS:
        unreal.log(f"KG_COAST_POI {name} @ ({x}, {y})")
