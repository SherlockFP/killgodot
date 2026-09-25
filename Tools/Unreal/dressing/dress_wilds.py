"""Zone "wilds": the forests, meadows and hills around Morrowmere (everything outside the village zones).

Points of interest (UE cm; see POIS / the KG_WILDS_POIS print), linked to the village by worn stepping-stone /
plank trails (grass trodden flat) with signposts at the forks:
  west : expanded woodcutter yard, ruined watchtower + curtain wall on the west shelf (climbable) with a viewpoint
         bench, hunter's camp (A-frame shelters, campfire + log seats, drying racks, high seat, archery dummies),
         trapper's snare line, mushroom wood with a fallen giant and a mossy chest, the Old Oak (swing + bench),
         knight's grave, standing-stone circle on the NW ridge, fairy ring glade
  north: wayside shrine at the fork, overturned-wagon ambush, abandoned cottage with an overgrown garden,
         wishing well, charcoal burner's mound
  east : woodland fishing pond with a jetty, berry meadow with skeps + a picnic, hilltop lookout with a signal
         beacon, smugglers' cave in the dell wall, buried treasure dig in the dell
Clutter (flowers, ferns, bushes, pebbles, stumps, logs, copses) densifies every edge and trail.

Run live: kg_dress.run(['wilds']) (see README.md). Only touches Dress/Wilds (+ trodden grass under trails/POIs).
"""
import math
import random

import unreal

import kg_dress_common as C

N, P, V, PIR, JP, WP = C.N, C.P, C.V, C.PIR, C.JP, C.WP
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/"
FURN = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/"
KNIFE = "/Game/KillGodot/Items/Melee/Hunters_Knife_a2avVUVeYD/StaticMeshes/SM_KG_Hunters_Knife"
R = random.Random(90210)

# Grass removal is persistent (KG_Meadow instances are saved with the level; the full rebuild regrows them).
# Positions below are final; set False while experimenting with new layouts.
CLEAR_GRASS = True

# Per-mesh HISM settings: the zone's KGFoliageField keeps ONE component per mesh, so collide/cull are fixed per path.
SOLID = {N + "Rock_Medium_1", N + "Rock_Medium_2", N + "Rock_Medium_3", V + "Roof_Log", V + "Corner_Exterior_Wood",
         V + "Prop_WoodenFence_Single", V + "Prop_WoodenFence_Extension1", N + "DeadTree_1", N + "DeadTree_2",
         N + "DeadTree_3", P + "Anvil_Log", P + "Crate_Wooden", PIR + "Barrel_3", IP + "SM_KG_Railing_1m"} \
    | {N + f"CommonTree_{i}" for i in range(1, 6)} | {N + f"Pine_{i}" for i in range(1, 6)}
CULL = {V + "Roof_Log": 12000.0, V + "Corner_Exterior_Wood": 12000.0, N + "Rock_Medium_1": 20000.0, N + "Rock_Medium_2": 20000.0,
        N + "Rock_Medium_3": 20000.0, V + "Prop_WoodenFence_Single": 10000.0, V + "Prop_WoodenFence_Extension1": 10000.0,
        N + "Bush_Common": 9000.0, N + "Bush_Common_Flowers": 9000.0, N + "Fern_1": 8000.0, P + "Crate_Wooden": 9000.0,
        PIR + "Barrel_3": 9000.0, IP + "SM_KG_Railing_1m": 9000.0}
for _i in range(1, 6):
    CULL[N + f"CommonTree_{_i}"] = 25000.0
    CULL[N + f"Pine_{_i}"] = 28000.0
for _i in (1, 2, 3):
    CULL[N + f"DeadTree_{_i}"] = 20000.0

_batch = {}
_trees = []                # (x, y, r) of pre-existing level trees/rocks/woodcutter props (+ copses we add)
_meadow = []
POIS = []                  # (name, x, y) for the report
skipped = {"zone": 0, "tree": 0}
FAILS = []                 # big placements that were refused (x, y, r, reason) - printed for tuning


# ============================================================================================ low-level helpers
def g(x, y):
    return C.ground(x, y)


def tree_hit(x, y, r):
    for (tx, ty, tr) in _trees:
        if abs(tx - x) < tr + r and abs(ty - y) < tr + r and math.hypot(tx - x, ty - y) < tr + r:
            return True
    return False


def ok(x, y, r=0.0, trees=True, lane=True):
    """Zone + lanes + buildings + existing trees (not the run's own claims: POI parts overlap on purpose)."""
    why = None
    if not C.in_zone(x, y):
        skipped["zone"] += 1
        why = "zone"
    elif lane and C.lane_distance(x, y) < r + 40.0:
        why = "lane"
    elif C.building_hit(x, y, r + 40.0):
        why = "building"
    elif trees and r > 0 and tree_hit(x, y, r):
        skipped["tree"] += 1
        why = "tree"
    if why and r >= 100.0:
        FAILS.append((round(x), round(y), round(r), why))
    return why is None


def prop(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, s=1.0, col=True, cull=0.0, sink=0.0, r=0.0, sub="Props",
         trees=True, lane=True, shadow=True):
    """Plain actor with zone/tree checks; r > 0 claims the footprint for later scatter."""
    if not ok(x, y, r, trees, lane):
        return None
    if cull == 0.0 and not col:
        cull = 7000.0
    return C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=s, sub=sub, collide=col, cull=cull,
                   sink=sink, claim_r=r, shadow=shadow)


def inst(path, x, y, z=None, yaw=None, s=1.0, pitch=0.0, roll=0.0, sink=2.0, check=True, r=0.0):
    """Queue one HISM instance (flushed at the end). Collision/cull come from SOLID/CULL per mesh."""
    if check and not ok(x, y, r, trees=r > 0, lane=r > 0):
        return False
    zz = (g(x, y) - sink) if z is None else z
    _batch.setdefault(path, []).append((x, y, zz, R.uniform(0.0, 360.0) if yaw is None else yaw, s, pitch, roll))
    if r > 0:
        C.claim(x, y, r)
    return True


def flush():
    for path, tr in _batch.items():
        C.instanced(path, tr, collide=path in SOLID, cull=CULL.get(path, 6500.0), sub="Instanced")
    _batch.clear()


def rot_offset(rot, lx, ly, lz):
    f, rr, u = rot.get_forward_vector(), rot.get_right_vector(), rot.get_up_vector()
    return (f.x * lx + rr.x * ly + u.x * lz, f.y * lx + rr.y * ly + u.y * lz, f.z * lx + rr.z * ly + u.z * lz)


def log(cx, cy, cz, a, length=300.0, dia=45.0, elev=0.0):
    """A lying timber (Roof_Log: axis along local Y, body at z 385..524) centred at (cx, cy, cz), axis pointing to
    compass angle `a` (deg, 0 = +X), tilted by `elev` degrees."""
    sd, sl = dia / 125.0, length / 1070.0
    rot = unreal.Rotator(roll=elev, pitch=0.0, yaw=a - 90.0)
    ox, oy, oz = rot_offset(rot, -1.5 * sd, 0.0, 454.5 * sd)
    _batch.setdefault(V + "Roof_Log", []).append((cx - ox, cy - oy, cz - oz, a - 90.0, (sd, sl, sd), 0.0, elev))


def post(x, y, h=300.0, z=None, yaw=0.0, thick=1.0):
    """Wooden post (Corner_Exterior_Wood 21x24x300, pivot at the base)."""
    zz = g(x, y) - 8.0 if z is None else z
    _batch.setdefault(V + "Corner_Exterior_Wood", []).append((x, y, zz, yaw, (thick, thick, h / 300.0), 0.0, 0.0))


def beam(x, y, z, a, length, thick=1.0):
    """Horizontal beam from (x, y, z) towards compass angle a (Corner_Exterior_Wood pitched -90 runs along +X)."""
    _batch.setdefault(V + "Corner_Exterior_Wood", []).append((x, y, z, a, (thick, thick, length / 300.0), -90.0, 0.0))


def seat_log(x, y, face_yaw, length=200.0, dia=50.0):
    """Log bench you can sit on (AKGSeat with the Roof_Log mesh scaled inside it). Sitter faces face_yaw."""
    if not ok(x, y, 60.0):
        return None
    sd, sl = dia / 125.0, length / 1070.0
    s = C.seat(V + "Roof_Log", x, y, g(x, y) - 6.0, yaw=face_yaw, seat_height=139.0 * sd - 10.0)
    try:
        comp = s.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_editor_property("relative_scale3d", unreal.Vector(sd, sl, sd))
        comp.set_editor_property("relative_location", unreal.Vector(1.5 * sd, 0.0, -385.0 * sd))
    except Exception as e:
        unreal.log_warning(f"KG_DRESS wilds seat_log: {e}")
    C.claim(x, y, 80.0)
    return s


def bench(x, y, face_yaw, z=None):
    """Kit bench as a seat (the bench mesh is long along X; the sitter faces +X, so turn the mesh 90)."""
    if not ok(x, y, 120.0, trees=False):
        return None
    s = C.seat(P + "Bench", x, y, (C.ground_min(x, y, 130.0) - 2.0) if z is None else z, yaw=face_yaw, seat_height=48.0)
    try:
        s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        s.set_seat_mesh(C.mesh(P + "Bench"))
    except Exception as e:
        unreal.log_warning(f"KG_DRESS wilds bench: {e}")
    C.claim(x, y, 150.0)
    clear(x, y, 90.0)
    return s


def stool(x, y, z, face_yaw):
    return C.seat(P + "Stool", x, y, z, yaw=face_yaw, seat_height=58.0)


def chest(x, y, yaw, name, table="Chest", path=None, z=None):
    if not C.in_zone(x, y):
        return None
    c = C.loot_chest(x, y, (g(x, y) - 3.0) if z is None else z, yaw=yaw, table=table, name=name, path=path)
    C.claim(x, y, 80.0)
    clear(x, y, 40.0)
    return c


def smash(path, x, y, yaw=None):
    if not ok(x, y, 50.0) or C.slope(x, y, 60.0) > 25.0:
        return None
    C.claim(x, y, 55.0)
    clear(x, y, 30.0)
    return C.breakable(path, x, y, g(x, y) + 3.0, R.uniform(0, 360) if yaw is None else yaw)


def glow(x, y, z, intensity=8.0, radius=700.0, color=(255, 170, 95)):
    if C.stats.get("lights", 0) >= 14:
        return None
    return C.light(x, y, z, intensity, radius, color)


def poi(name, x, y):
    POIS.append((name, round(x), round(y)))
    C.claim(x, y, 150.0)


def clear(x, y, r):
    """Trample the meadow (remove KG_Meadow clumps whose bounds touch the sphere). Cached components: fast."""
    if not CLEAR_GRASS or not C.in_zone(x, y):
        return 0
    if not _meadow:
        for a in C.actors.get_all_level_actors():
            if a.get_actor_label() == "KG_Meadow":
                _meadow.extend(a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
        if not _meadow:
            _meadow.append(None)
    n = 0
    for comp in _meadow:
        if comp is None:
            continue
        hit = comp.get_instances_overlapping_sphere(unreal.Vector(x, y, g(x, y) + 40.0), r, True)
        if hit:
            comp.remove_instances(list(hit))
            n += len(hit)
    C.stats["grass_cleared"] = C.stats.get("grass_cleared", 0) + n
    return n


def ring_pts(cx, cy, r, n, a0=0.0):
    return [(cx + r * math.cos(math.radians(a0 + k * 360.0 / n)), cy + r * math.sin(math.radians(a0 + k * 360.0 / n)),
             a0 + k * 360.0 / n) for k in range(n)]


def local(f, lx, ly):
    return f.world(lx, ly)


# ============================================================================================ small kits
def campfire(x, y, light=True, intensity=24.0):
    z = g(x, y)
    for (px, py, a) in ring_pts(x, y, 62.0, 9, R.uniform(0, 40)):
        inst(N + f"Pebble_Round_{R.randint(1, 5)}", px, py, z - 3.0, a, 1.5, check=False)
    for k in range(4):          # teepee of sticks leaning in
        a = k * 90.0 + 20.0
        log(x + 18.0 * math.cos(math.radians(a)), y + 18.0 * math.sin(math.radians(a)), z + 28.0, a + 180.0,
            length=95.0, dia=10.0, elev=-50.0)
    inst(P + "Pot_1", x + 5.0, y - 3.0, z + 1.0, 0.0, 0.7, check=False)   # ember bed
    if light:
        glow(x, y, z + 70.0, intensity, 900.0, (255, 140, 60))
        C.ambient("A_Fire_Loop", x, y, z + 60.0, 0.8)
    C.claim(x, y, 110.0)
    clear(x, y, 150.0)


def aframe(x, y, yaw, sc=1.0):
    """Hunter's A-frame shelter: two plank roof halves on the ground, a bag and a fern."""
    f = C.Frame(x, y, yaw)
    z = C.ground_min(x, y, 140.0 * sc) - 4.0
    for piece in ("Roof_Wooden_2x1_Center", "Roof_Wooden_2x1_Center_Mirror"):
        C.place(V + piece, x, y, z + 12.0, yaw=yaw, scale=(1.15 * sc, 0.72 * sc, 1.55 * sc), sub="Camp", cull=12000.0)
    C.place(P + "Bag", *f.world(-40.0, 30.0), z=z, yaw=yaw + 20.0, sub="Camp", collide=False, cull=5000.0)
    C.place(IP + "SM_KG_Rug_Runner", *f.world(20.0, -10.0), z=z + 6.0, yaw=yaw + 90.0, scale=(0.75, 1.0, 1.0), sub="Camp",
            collide=False, cull=5000.0)      # bedroll
    C.claim(x, y, 190.0 * sc)
    clear(x, y, 150.0 * sc)


def drying_rack(x, y, a, fish=True):
    """Two posts, a pole, hanging fish / hides / herbs."""
    c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
    x0, y0, x1, y1 = x - c * 110.0, y - s * 110.0, x + c * 110.0, y + s * 110.0
    zt = max(g(x0, y0), g(x1, y1))
    post(x0, y0, 190.0, g(x0, y0) - 8.0, a)
    post(x1, y1, 190.0, g(x1, y1) - 8.0, a)
    beam(x0 - c * 15.0, y0 - s * 15.0, zt + 172.0, a, 250.0, 0.8)
    kinds = ["SM_KG_Fish_Salmon", "SM_KG_Fish_Cod", "HIDE", "SM_KG_Fish_Mackerel", "HERB", "SM_KG_Fish_Salmon", "HIDE"] if fish \
        else ["HERB", "HIDE", "HERB", "HERB", "HIDE", "HERB"]
    for k, kind in enumerate(kinds):
        t = -90.0 + k * (180.0 / max(1, len(kinds) - 1))
        hx, hy = x + c * t, y + s * t
        if kind == "HIDE":
            C.place(P + "Banner_1_Cloth", hx, hy, zt + 176.0, yaw=a, scale=(0.55, 1.0, 0.42), sub="Camp", collide=False, cull=6000.0)
        elif kind == "HERB":
            inst(IP + "SM_KG_HerbBundle", hx, hy, zt + 172.0, a, 1.3, check=False)
        else:
            inst(WP + kind, hx, hy, zt + 140.0, a, 1.4, pitch=90.0, check=False)
    C.claim(x, y, 130.0)
    clear(x, y, 90.0)


def log_pile(x, y, a, rows=3, length=320.0, dia=42.0):
    """Pyramid of felled timbers, axis along compass angle a."""
    if not ok(x, y, length * 0.45):
        return
    c, s = math.cos(math.radians(a + 90.0)), math.sin(math.radians(a + 90.0))
    z0 = C.ground_min(x, y, length * 0.4) - 4.0
    for row in range(rows):
        n = rows - row
        for k in range(n):
            off = (k - (n - 1) / 2.0) * dia * 1.02
            log(x + c * off, y + s * off, z0 + dia * 0.5 + row * dia * 0.86, a + R.uniform(-3, 3),
                length=length * R.uniform(0.9, 1.05), dia=dia * R.uniform(0.9, 1.1))
    for side in (-1, 1):   # chocks
        off = side * (rows * dia * 0.5 + 12.0)
        inst(N + "Pebble_Square_1", x + c * off, y + s * off, None, a, 1.2, check=False)
    C.claim(x, y, length * 0.5)
    clear(x, y, length * 0.3)


def sawhorse(x, y, a, with_log=True):
    """Two X-frames + a rail; optional log being sawn."""
    if not ok(x, y, 110.0):
        return
    ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
    z = g(x, y)
    for t in (-55.0, 55.0):
        px, py = x + ca * t, y + sa * t
        for lean in (-28.0, 28.0):
            _batch.setdefault(V + "Corner_Exterior_Wood", []).append((px, py, z - 5.0, a, (0.6, 0.6, 0.36), 0.0, lean))
    log(x, y, z + 88.0, a, length=150.0, dia=12.0)
    if with_log:
        log(x + ca * 10.0, y + sa * 10.0, z + 110.0, a + 4.0, length=210.0, dia=34.0)
    C.claim(x, y, 110.0)
    clear(x, y, 70.0)


def stump(x, y, s=1.0, axe=False, shroom=False):
    if not inst(P + "Anvil_Log", x, y, g(x, y) - 8.0, None, (0.8 * s, 0.8 * s, 0.55 * s), check=True, r=40.0 * s):
        return False
    top = g(x, y) - 8.0 + 59.0 * s
    if axe:
        inst(P + "Axe_Bronze", x + 6.0, y, top + 22.0, R.uniform(0, 360), 1.0, roll=24.0, check=False)
    if shroom:
        inst(N + "Mushroom_Laetiporus", x + 22.0, y + 10.0, top - 25.0, R.uniform(0, 360), 0.45, check=False)
    return True


def fallen_tree(x, y, a, s=0.55, dead=2):
    """A dead tree lying on its side (DeadTree_* rolled 86 deg), root end at (x, y)."""
    return prop(N + f"DeadTree_{dead}", x, y, g(x, y) - 25.0 * s, yaw=a, roll=86.0, pitch=R.uniform(-4, 4), s=s, col=True,
                cull=16000.0, sub="Fallen", trees=False)


def signpost(x, y, targets, h=230.0):
    """Post with arrow boards pointing at each target (x, y)."""
    if not ok(x, y, 30.0, trees=True, lane=False):
        return
    z = g(x, y) - 10.0
    post(x, y, h, z, 0.0, 1.3)
    for k, (tx, ty) in enumerate(targets):
        a = math.degrees(math.atan2(ty - y, tx - x))
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        inst(PIR + ("Planks_0" if k % 2 == 0 else "Planks_2"), x + ca * 44.0, y + sa * 44.0, z + h - 45.0 - k * 30.0, a,
             (0.46, 0.55, 1.0), roll=90.0, check=False)
    inst(N + "Pebble_Round_2", x + 25.0, y - 18.0, None, None, 1.2, check=False)
    inst(N + "Clover_2", x - 20.0, y + 15.0, None, None, 0.6, check=False)
    C.claim(x, y, 60.0)
    clear(x, y, 50.0)


def lantern_post(x, y, a, light=False, intensity=9.0):
    z = g(x, y) - 8.0
    post(x, y, 260.0, z, a, 1.1)
    C.place(P + "Lantern_Wall", x, y, z + 150.0, yaw=a, sub="Lanterns", collide=False, cull=9000.0)
    if light:
        ca, sa = math.cos(math.radians(a + 90.0)), math.sin(math.radians(a + 90.0))
        glow(x + ca * 100.0, y + sa * 100.0, z + 215.0, intensity, 800.0)
    C.claim(x, y, 50.0)


def fence_run(pts, broken=0.25):
    """Wooden fence along a polyline (2.06 m sections); some sections missing/leaning (abandoned look)."""
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        n = max(1, int(round(seg / 206.0)))
        a = math.degrees(math.atan2(by - ay, bx - ax))
        for k in range(n):
            t = (k + 0.5) / n
            x, y = ax + (bx - ax) * t, ay + (by - ay) * t
            if R.random() < broken * 0.4:
                continue
            lean = R.uniform(8, 22) * R.choice((-1, 1)) if R.random() < broken else R.uniform(-2, 2)
            if not C.in_zone(x, y) or C.lane_distance(x, y) < 60.0 or tree_hit(x, y, 60.0):
                continue
            inst(V + "Prop_WoodenFence_Single", x, y, g(x, y) - 4.0, a, 1.0, roll=lean, check=False)
            C.claim(x, y, 105.0)


def flower_patch(x, y, r=180.0, n=7, big=False):
    for _ in range(n):
        a, d = R.uniform(0, 2 * math.pi), r * math.sqrt(R.random())
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        pick = R.random()
        if pick < 0.4:
            inst(N + R.choice(["Petal_1", "Petal_2", "Petal_3", "Petal_5"]), px, py, None, None, R.uniform(1.0, 1.5))
        elif pick < 0.58:
            inst(N + R.choice(["Flower_3_Group", "Flower_3_Group", "Flower_4_Group"]), px, py, None, None,
                 R.uniform(0.3, 0.4) if big else R.uniform(0.22, 0.3))
        elif pick < 0.82:
            inst(N + R.choice(["Clover_1", "Clover_2", "Plant_7"]), px, py, None, None, R.uniform(0.7, 1.1))
        else:
            inst(N + R.choice(["Flower_3_Single", "Flower_4_Single"]), px, py, None, None, R.uniform(0.22, 0.3))


def shroom_ring(x, y, r=150.0, n=11):
    for (px, py, a) in ring_pts(x, y, r, n, R.uniform(0, 30)):
        inst(N + "Mushroom_Common", px + R.uniform(-15, 15), py + R.uniform(-15, 15), None, None, R.uniform(0.6, 0.95))
    for _ in range(4):
        a, d = R.uniform(0, 2 * math.pi), R.uniform(0, r * 0.6)
        inst(N + R.choice(["Petal_2", "Petal_4", "Clover_1"]), x + d * math.cos(a), y + d * math.sin(a), None, None, 0.9)
    clear(x, y, r * 0.55)


def rock_cluster(x, y, s=1.0, n=3, ferns=True):
    for k in range(n):
        a, d = R.uniform(0, 2 * math.pi), R.uniform(0, 160.0 * s) if k else 0.0
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        sc = s * (R.uniform(0.45, 0.75) if k == 0 else R.uniform(0.18, 0.35))
        inst(N + f"Rock_Medium_{R.randint(1, 3)}", px, py, g(px, py) - 25.0 * sc, None, sc, check=True, r=120.0 * sc)
    if ferns:
        for k in range(2):
            a = R.uniform(0, 2 * math.pi)
            inst(N + "Fern_1", x + 170.0 * s * math.cos(a), y + 170.0 * s * math.sin(a), None, None, R.uniform(0.4, 0.6))


def bush_clump(x, y, flowers=True, n=3):
    for k in range(n):
        a, d = R.uniform(0, 2 * math.pi), R.uniform(0, 160.0) if k else 0.0
        inst(N + ("Bush_Common_Flowers" if flowers and R.random() < 0.6 else "Bush_Common"), x + d * math.cos(a),
             y + d * math.sin(a), None, None, R.uniform(0.6, 1.0), sink=10.0)


def copse(x, y, n=5, r=700.0, pines=False):
    """A small stand of trees (instanced, colliding), avoiding trails/POIs already claimed."""
    placed = 0
    for _ in range(n * 12):
        if placed >= n:
            break
        a, d = R.uniform(0, 2 * math.pi), r * math.sqrt(R.random())
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        if not C.free(px, py, 180.0) or tree_hit(px, py, 250.0):
            continue
        path = N + (f"Pine_{R.randint(1, 5)}" if pines or R.random() < 0.25 else f"CommonTree_{R.randint(1, 5)}")
        sc = R.uniform(0.8, 1.25) if "Common" in path else R.uniform(0.9, 1.5)
        inst(path, px, py, g(px, py) - 12.0, None, sc, check=False)
        C.claim(px, py, 200.0)
        _trees.append((px, py, 150.0))
        placed += 1
        if R.random() < 0.5:
            inst(N + R.choice(["Fern_1", "Plant_1", "Bush_Common"]), px + R.uniform(-150, 150), py + R.uniform(-150, 150),
                 None, None, R.uniform(0.5, 0.8))
    return placed


# ============================================================================================ trails
def trail(pts, kind="stone", step=95.0, width=40.0, edge_every=800.0):
    """Worn stepping stones (or planks) along a polyline through trodden grass; edge detail every ~8 m."""
    carry, walked = 0.0, 0.0
    next_edge = edge_every * 0.5
    last_clear = -1e9
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / seg, (by - ay) / seg
        t = carry
        while t < seg:
            x, y = ax + dx * t, ay + dy * t
            nx, ny = -dy, dx
            j = R.uniform(-width, width)
            px, py = x + nx * j, y + ny * j
            a = math.degrees(math.atan2(dy, dx))
            if C.in_zone(px, py) and C.lane_distance(px, py) > 30.0 and not C.building_hit(px, py, 20.0):
                if kind == "plank" or (kind == "mixed" and R.random() < 0.3):
                    inst(PIR + R.choice(["Planks_0", "Planks_1", "Planks_2", "Planks_3"]), px, py, g(px, py) - 4.0,
                         a + 90.0 + R.uniform(-8, 8), (0.62, 1.0, 1.0), check=False)
                elif R.random() < 0.8:
                    inst(N + R.choice(["RockPath_Round_Small_1", "RockPath_Round_Small_2", "RockPath_Round_Small_3",
                                       "RockPath_Square_Small_1", "RockPath_Square_Small_2", "RockPath_Round_Thin"]),
                         px, py, g(px, py) - 5.0, None, R.uniform(0.55, 0.8), check=False)
                else:
                    inst(JP + "SteppingStone", px, py, g(px, py) - 4.0, None, R.uniform(0.9, 1.2), check=False)
                if R.random() < 0.3:     # pebbles kicked aside
                    inst(N + f"Pebble_Round_{R.randint(1, 5)}", px + nx * R.uniform(60, 110) * R.choice((-1, 1)),
                         py + ny * R.uniform(60, 110), None, None, R.uniform(0.6, 1.0), check=False)
            C.claim(x, y, 70.0)        # keep the trail itself clear of later scatter
            walked += step
            if walked - last_clear >= 130.0:
                clear(x, y, 12.0)
                last_clear = walked
            if walked >= next_edge:
                next_edge += edge_every * R.uniform(0.8, 1.2)
                trail_edge(x, y, dx, dy)
            t += step * R.uniform(0.85, 1.15)
        carry = t - seg


def trail_edge(x, y, dx, dy):
    """Something to look at beside the trail: flowers, a stump, a rock with ferns, a bush, a log, a lost item."""
    side = R.choice((-1, 1))
    nx, ny = -dy * side, dx * side
    d = R.uniform(170.0, 280.0)
    px, py = x + nx * d, y + ny * d
    if not C.in_zone(px, py) or C.lane_distance(px, py) < 120.0 or C.overlaps(px, py, 60.0) or tree_hit(px, py, 60.0):
        return
    pick = R.random()
    if pick < 0.26:
        flower_patch(px, py, 150.0, 7)
    elif pick < 0.42:
        stump(px, py, R.uniform(0.8, 1.1), shroom=R.random() < 0.6)
        flower_patch(px + nx * 80.0, py + ny * 80.0, 90.0, 3)
    elif pick < 0.56:
        rock_cluster(px, py, R.uniform(0.5, 0.8), 2)
    elif pick < 0.72:
        bush_clump(px, py, True, 2)
    elif pick < 0.84:
        log(px, py, C.ground_min(px, py, 100.0) + 16.0, math.degrees(math.atan2(dy, dx)) + R.uniform(-25, 25),
            length=R.uniform(180, 260), dia=R.uniform(28, 40))
        inst(N + "Mushroom_Common", px + nx * 40.0, py + ny * 40.0, None, None, 0.7, check=False)
        C.claim(px, py, 120.0)
        clear(px, py, 40.0)
    else:
        item = R.choice([P + "Bucket_Wooden_1", P + "Bag", WP + "SM_KG_Shovel", P + "Rope_2", P + "FarmCrate_Empty",
                         P + "Barrel", P + "Cage_Small"])
        z = g(px, py) + (4.0 if "Shovel" in item else 0.0)
        C.place(item, px, py, z, yaw=R.uniform(0, 360), sub="Lost", collide=item.endswith("Barrel"), cull=6000.0)
        flower_patch(px, py, 120.0, 3)
        C.claim(px, py, 80.0)
        clear(px, py, 35.0)


# ============================================================================================ POIs: west
def woodcutter_yard():
    """Expanded woodcutter camp around the ChopWood chore (-6000, 800): timber piles, a firewood shed, sawhorses,
    stumps with axes, a loaded wagon and a felled tree being limbed."""
    cx, cy = -6000.0, 800.0
    poi("Woodcutter yard", cx, cy)
    log_pile(-6650.0, 150.0, 80.0, 3, 340.0, 44.0)
    log_pile(-6950.0, 650.0, 95.0, 4, 380.0, 46.0)
    log_pile(-5450.0, 1350.0, 20.0, 2, 260.0, 38.0)
    # Firewood shed: four posts, plank roof, split wood stacked inside.
    sx, sy, syaw = -6700.0, 1350.0, 10.0
    f = C.Frame(sx, sy, syaw)
    if ok(sx, sy, 200.0):
        zb = C.ground_min(sx, sy, 220.0)
        for lx, ly, h in ((-190, -90, 250), (190, -90, 250), (-190, 90, 215), (190, 90, 215)):
            px, py = f.world(lx, ly)
            post(px, py, h, g(px, py) - 10.0, syaw, 1.2)
        for lx in (-100.0, 100.0):
            px, py = f.world(lx, 0.0)
            C.place(V + "Roof_Wooden_2x1_Middle", px, py, zb + 128.0, yaw=syaw, roll=-10.0, scale=(1.0, 1.15, 1.0),
                    sub="Woodcutter", cull=12000.0)
        for k in range(6):
            px, py = f.world(-150.0 + k * 60.0, 40.0)
            inst(IP + "SM_KG_Firewood", px, py, zb - 2.0, syaw + 180.0, 1.15, check=False)
            inst(IP + "SM_KG_Firewood", px, py, zb + 50.0, syaw + 180.0, 1.1, check=False)
        C.claim(sx, sy, 250.0)
        clear(sx, sy, 160.0)
        px, py = f.world(-90.0, -150.0)
        C.place(P + "Barrel", px, py, g(px, py), yaw=30.0, sub="Woodcutter", cull=9000.0)
    sawhorse(-5500.0, 1150.0, 35.0)
    sawhorse(-6450.0, 1150.0, -60.0, with_log=False)
    for (x, y, s, axe) in ((-6350.0, 450.0, 1.0, True), (-5700.0, 1250.0, 0.9, False), (-6600.0, 900.0, 1.1, True),
                           (-5400.0, 600.0, 0.8, False), (-6250.0, 1500.0, 1.0, False), (-7150.0, 1050.0, 0.9, False)):
        stump(x, y, s, axe=axe, shroom=not axe)
    # Wagon loaded with timber, waiting to go to town.
    wx, wy, wyaw = -6800.0, -450.0, 115.0
    if ok(wx, wy, 150.0):
        zw = C.ground_min(wx, wy, 180.0)
        C.place(V + "Prop_Wagon", wx, wy, zw + 2.0, yaw=wyaw, sub="Woodcutter", cull=15000.0)
        fw = C.Frame(wx, wy, wyaw)
        for k in range(3):
            px, py = fw.world(-35.0 + k * 35.0, -120.0)
            log(px, py, zw + 118.0 + (k % 2) * 30.0, wyaw + 90.0, length=300.0, dia=34.0)
        C.claim(wx, wy - 100.0, 260.0)
    fallen_tree(-7400.0, 200.0, 40.0, 0.5, 1)
    for k in range(10):   # chips
        a, d = R.uniform(0, 2 * math.pi), R.uniform(80.0, 260.0)
        inst(N + f"Pebble_Square_{R.randint(1, 6)}", -6000.0 + d * math.cos(a), 800.0 + d * math.sin(a), None, None,
             R.uniform(0.3, 0.55), check=False)
    for x, y in ((-5350.0, 900.0), (-6300.0, 1100.0)):
        smash(P + "Crate_Wooden", x, y)
    smash(P + "Barrel", -5300.0, 1050.0)
    bench(-6350.0, -50.0, 60.0)
    inst(P + "Bucket_Wooden_1", -6200.0, 0.0, None, 20.0, 1.0, check=False)
    lantern_post(-5600.0, 1500.0, -30.0, light=True)
    signpost(-5750.0, 150.0, [(-8900.0, 1700.0), (-6300.0, -2200.0), (-4600.0, -200.0)])
    fence_run([(-7300.0, -300.0), (-7500.0, 500.0), (-7550.0, 1300.0), (-7200.0, 1800.0)], broken=0.3)
    C.ambient("A_Birds_Loop", -6400.0, 600.0, g(-6400.0, 600.0) + 500.0, 0.6)


def watchtower_ruin():
    """Ruined stone watchtower on the west shelf: broken walls, a fallen spire, rubble, a torn banner, a climbable
    ladder to a patched lookout floor, a loot chest, a crumbling curtain wall around it."""
    x, y, yaw = -8900.0, 1700.0, 15.0
    poi("Ruined watchtower", x, y)
    f = C.Frame(x, y, yaw)
    gz = C.ground_min(x, y, 260.0) + 2.0
    B = "UnevenBrick"
    layout = {  # level -> {(side, k): piece}; missing = collapsed
        0: {(0.0, 0): f"Wall_{B}_Door_Round", (0.0, 1): f"Wall_{B}_Straight", (180.0, 0): f"Wall_{B}_Window_Thin_Round",
            (180.0, 1): f"Wall_{B}_Straight", (-90.0, 0): f"Wall_{B}_Straight", (-90.0, 1): f"Wall_{B}_Window_Thin_Round",
            (90.0, 0): f"Wall_{B}_Straight"},
        1: {(0.0, 1): f"Wall_{B}_Window_Thin_Round", (180.0, 0): f"Wall_{B}_Straight", (180.0, 1): f"Wall_{B}_Window_Thin_Round",
            (-90.0, 0): f"Wall_{B}_Straight"},
        2: {(180.0, 0): f"Wall_{B}_Straight", (-90.0, 0): "Wall_Arch"},
    }
    for lvl, pieces in layout.items():
        z = gz + lvl * C.FLOOR_H
        for (side, k), piece in pieces.items():
            off = -100.0 + k * 200.0
            lx, ly = {0.0: (off, -200.0), 180.0: (-off, 200.0), -90.0: (-200.0, -off), 90.0: (200.0, off)}[side]
            px, py = f.world(lx, ly)
            C.place(V + piece, px, py, z, yaw=yaw + side, sub="Ruin")
    for sx, sy, h in ((-1, -1, 2), (1, -1, 1), (-1, 1, 3), (1, 1, 1)):
        for lvl in range(h):
            px, py = f.world(sx * 200.0, sy * 200.0)
            C.place(V + "Corner_Exterior_Brick", px, py, gz + lvl * C.FLOOR_H, yaw=yaw, sub="Ruin")
    for i in (-100.0, 100.0):
        for j in (-100.0, 100.0):
            px, py = f.world(i, j)
            C.place(V + "Floor_UnevenBrick", px, py, gz + 1.0, yaw=yaw, sub="Ruin")
            if not (i > 0 and j < 0):
                C.place(V + "Floor_WoodDark", px, py, gz + C.FLOOR_H, yaw=yaw + R.choice((0, 90)), sub="Ruin")
    lx, ly = f.world(100.0, -10.0)
    lad = C.spawn_class("/Script/KillGodot.KGLadder", lx, ly, gz + 2.0, yaw + 90.0, sub="Ruin")
    if lad:
        lad.set_height(C.FLOOR_H)
        ladder_visual(lx, ly, gz + 2.0, yaw + 90.0, C.FLOOR_H)
    # the spire lies where it fell, half buried
    px, py = f.world(520.0, 380.0)
    C.place(V + "Roof_Tower_RoundTiles", px, py, g(px, py) - 160.0, yaw=yaw + 35.0, roll=62.0, scale=0.8, sub="Ruin")
    for lx_, ly_, a in ((330.0, -120.0, 20.0), (-380.0, 260.0, -40.0)):   # fallen wall slabs
        px, py = f.world(lx_, ly_)
        C.place(V + f"Wall_{B}_Straight", px, py, g(px, py) - 18.0, yaw=yaw + a, roll=86.0, sub="Ruin")
    for _ in range(45):   # rubble
        a, d = R.uniform(0, 2 * math.pi), R.uniform(230.0, 560.0)
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        inst(V + f"Prop_Brick{R.randint(1, 4)}", px, py, g(px, py) + 4.0, None, R.uniform(1.0, 1.8), pitch=R.uniform(-20, 20),
             roll=R.uniform(-30, 30), check=False)
    for lx_, ly_ in ((-260.0, -330.0), (290.0, 270.0), (-300.0, 100.0)):
        px, py = f.world(lx_, ly_)
        inst(P + "Vase_Rubble_Medium", px, py, None, None, 1.6, check=False)
    for k, (px, py, a) in enumerate(ring_pts(x, y, 780.0, 11, yaw + 10.0)):   # curtain wall fragments
        if k in (2, 3, 7) or not C.in_zone(px, py):
            continue
        z = C.ground_min(px, py, 110.0) - 10.0
        lean = R.choice((0.0, 0.0, 0.0, R.uniform(6, 12)))
        C.place(V + f"Wall_{B}_Straight", px, py, z, yaw=a + 90.0, roll=lean, scale=(1.0, 1.0, R.uniform(0.35, 0.62)), sub="Ruin")
        if R.random() < 0.5:
            C.place(V + "Corner_Exterior_Brick", px + 100.0 * math.cos(math.radians(a + 90.0)),
                    py + 100.0 * math.sin(math.radians(a + 90.0)), z, yaw=a, scale=(1.0, 1.0, R.uniform(0.4, 0.75)), sub="Ruin")
    px, py = f.world(-150.0, -260.0)   # torn banner on a broken pole
    C.place(PIR + "Broken_Flag_0", px, py, g(px, py) - 10.0, yaw=yaw, roll=6.0, sub="Ruin")
    C.mover(P + "Banner_2_Cloth", px + 8.0, py, g(px, py) + 300.0, yaw=yaw + 90.0, scale=0.8, sway=5.0, sway_hz=0.3)
    for lx_, ly_, side in ((-60.0, -232.0, 0.0), (-232.0, 60.0, -90.0), (60.0, 232.0, 180.0)):
        px, py = f.world(lx_, ly_)
        C.place(V + R.choice(["Prop_Vine1", "Prop_Vine2", "Prop_Vine5"]), px, py, gz + 290.0, yaw=yaw + side, sub="Ruin",
                collide=False, cull=9000.0)
    px, py = f.world(-100.0, 110.0)
    chest(px, py, yaw + 180.0, "Tower cache", "Chest", PIR + "chest_silver_0", z=gz + 2.0)
    px, py = f.world(90.0, 120.0)
    smash(P + "Barrel", px, py)
    px, py = f.world(-120.0, -120.0)
    stool(px, py, gz + C.FLOOR_H + 2.0, yaw - 90.0)     # lookout stool upstairs
    C.claim(x, y, 420.0)
    clear(x, y, 330.0)
    C.ambient("A_Wind_Loop", x, y, gz + 500.0, 0.5)


def ladder_visual(x, y, z, yaw, height):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for side in (-28.0, 28.0):
        _batch.setdefault(V + "Corner_Exterior_Wood", []).append(
            (x - s * side, y + c * side, z, yaw, (0.35, 0.35, height / 300.0 + 0.05), 0.0, 0.0))
    rung = 30.0
    while rung < height:
        _batch.setdefault(V + "Corner_Exterior_Wood", []).append(
            (x + s * 28.0, y - c * 28.0, z + rung, yaw, (0.25, 0.25, 0.19), 0.0, -90.0))
        rung += 38.0


def viewpoint_west():
    """Bench on the shelf edge looking over the rooftops to the harbour; a cairn and a lantern post."""
    x, y = -8250.0, 2350.0
    poi("West viewpoint bench", x, y)
    bench(x, y, 20.0)
    lantern_post(x - 250.0, y - 120.0, 110.0, light=False)
    for k in range(4):   # cairn
        inst(N + f"Pebble_Square_{k + 1}", x + 250.0, y + 60.0, g(x + 250.0, y + 60.0) + k * 12.0, R.uniform(0, 360),
             1.6 - k * 0.3, check=False)
    flower_patch(x - 100.0, y + 200.0, 200.0, 8, big=True)
    inst(P + "Bag", x + 110.0, y - 60.0, None, 40.0, 1.0, check=False)


def high_seat(x, y, yaw):
    """Hunter's high seat: a roofed plank platform 3 m up on four posts, railings, a ladder and a stool."""
    f = C.Frame(x, y, yaw)
    if not ok(x, y, 160.0):
        return
    zb = C.ground_min(x, y, 130.0)
    zf = zb + 300.0
    for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        px, py = f.world(sx * 95.0, sy * 95.0)
        post(px, py, zf + 210.0 - g(px, py), g(px, py) - 10.0, yaw, 1.1)
    C.place(V + "Floor_WoodDark", x, y, zf, yaw=yaw, sub="Camp")
    C.place(V + "Roof_Wooden_2x1", *f.world(0.0, -80.0), z=zf + 200.0, yaw=yaw, scale=(1.0, 1.1, 0.8), sub="Camp", cull=12000.0)
    for lx, ly, a in ((-50.0, 100.0, 0.0), (50.0, 100.0, 0.0), (100.0, 50.0, 90.0), (100.0, -50.0, 90.0),
                      (-100.0, 50.0, 90.0), (-100.0, -50.0, 90.0)):   # rails on back and sides (front = ladder side)
        px, py = f.world(lx, ly)
        inst(IP + "SM_KG_Railing_1m", px, py, zf, yaw + a, 1.0, check=False)
    lx, ly = f.world(0.0, -125.0)
    lad = C.spawn_class("/Script/KillGodot.KGLadder", lx, ly, g(lx, ly), yaw + 90.0, sub="Camp")
    if lad:
        lad.set_height(zf - g(lx, ly))
        ladder_visual(lx, ly, g(lx, ly), yaw + 90.0, zf - g(lx, ly))
    stool(*f.world(0.0, 40.0), zf + 2.0, yaw - 90.0)
    C.claim(x, y, 170.0)
    clear(x, y, 110.0)


def hunter_camp():
    """Hunter's camp between two trees: A-frame shelters, campfire with log seats and a cauldron, drying racks
    with fish/hides/herbs, archery dummies, a high seat, weapon stand, traps and a loot chest."""
    cx, cy = -6350.0, -2250.0
    poi("Hunter's camp", cx, cy)
    campfire(cx, cy)
    for k, a in enumerate((200.0, 290.0, 20.0)):
        sx, sy = cx + 230.0 * math.cos(math.radians(a)), cy + 230.0 * math.sin(math.radians(a))
        seat_log(sx, sy, a + 180.0, 190.0, 48.0)
    tx, ty = cx + 150.0, cy - 110.0
    C.place(P + "Cauldron", tx, ty, g(tx, ty), yaw=10.0, scale=0.6, sub="Camp", cull=9000.0)
    aframe(cx - 520.0, cy - 250.0, 100.0)
    aframe(cx - 380.0, cy + 380.0, 150.0, 0.9)
    drying_rack(cx + 420.0, cy + 250.0, 60.0, fish=True)
    drying_rack(cx - 150.0, cy - 520.0, 5.0, fish=False)
    C.place(P + "WeaponStand", cx + 330.0, cy - 330.0, g(cx + 330.0, cy - 330.0), yaw=130.0, sub="Camp")
    for dx, dy in ((-120.0, -1200.0), (180.0, -1350.0)):   # archery dummies down-range
        px, py = cx + dx, cy + dy
        if ok(px, py, 60.0):
            C.place(P + "Dummy", px, py, g(px, py), yaw=90.0 + R.uniform(-10, 10), sub="Camp")
            clear(px, py, 50.0)
    for dx, dy in ((650.0, -700.0), (800.0, -350.0), (300.0, -900.0), (-300.0, -950.0), (900.0, 150.0)):
        if ok(cx + dx, cy + dy, 160.0):
            high_seat(cx + dx, cy + dy, math.degrees(math.atan2(-dy, -dx)) + 90.0)
            break
    for px, py in ((cx + 520.0, cy - 60.0), (cx - 620.0, cy + 60.0)):
        C.place(P + "Cage_Small", px, py, g(px, py), yaw=R.uniform(0, 360), scale=0.8, sub="Camp", collide=False, cull=7000.0)
    for name, dx, dy, yaw in (("Bag", 150.0, 180.0, 30.0), ("Rope_1", -90.0, 200.0, 0.0), ("Bucket_Wooden_1", 120.0, -200.0, 0.0),
                              ("Pot_1_Lid", -180.0, -130.0, 0.0)):
        inst(P + name, cx + dx, cy + dy, None, yaw, 1.0, check=False)
    inst(WP + "SM_KG_Fish_Salmon", cx + 190.0, cy - 60.0, g(cx + 190.0, cy - 60.0) + 8.0, 70.0, 1.0, check=False)
    chest(cx - 640.0, cy - 420.0, 60.0, "Hunter's stash", "Chest", PIR + "chest_common_0")
    smash(P + "Crate_Wooden", cx + 560.0, cy + 120.0)
    smash(PIR + "crates_0", cx - 700.0, cy + 350.0)
    log_pile(cx + 100.0, cy + 560.0, 170.0, 2, 220.0, 32.0)
    fence_run([(cx - 900.0, cy - 700.0), (cx - 950.0, cy + 200.0)], broken=0.5)
    lantern_post(cx - 280.0, cy + 60.0, 20.0, light=False)
    clear(cx, cy, 330.0)
    C.ambient("A_Crickets_Loop", cx, cy, g(cx, cy) + 200.0, 0.5)


def trapper_line():
    """Trapper's snare line on the path north: cages, rope, a skinning bench with a knife and a drying hide."""
    x, y = -5750.0, -3300.0
    poi("Trapper's snare line", x, y)
    for k, (dx, dy) in enumerate(((0.0, 0.0), (-350.0, -420.0), (250.0, -700.0))):
        px, py = x + dx, y + dy
        if ok(px, py, 60.0):
            C.place(P + "Cage_Small", px, py, g(px, py), yaw=R.uniform(0, 360), scale=0.7 + 0.1 * k, sub="Traps", cull=7000.0)
            inst(P + "Rope_1", px + 60.0, py + 30.0, None, None, 1.0, check=False)
            inst(N + "Bush_Common", px - 120.0, py + 40.0, None, None, 0.6, sink=10.0, check=False)
            clear(px, py, 45.0)
    tx, ty = x + 300.0, y + 200.0
    C.place(P + "Table_Large", tx, ty, g(tx, ty), yaw=70.0, scale=(0.6, 1.0, 1.0), sub="Traps")
    inst(KNIFE, tx, ty, g(tx, ty) + 82.0, 30.0, 1.0, check=False)
    C.place(P + "Banner_1_Cloth", tx, ty + 30.0, g(tx, ty) + 81.0, yaw=160.0, roll=88.0, scale=(0.9, 1.0, 0.5), sub="Traps",
            collide=False, cull=6000.0)
    clear(tx, ty, 90.0)
    bush_clump(x + 150.0, y - 350.0, True, 3)
    flower_patch(x - 250.0, y + 250.0, 180.0, 6)


def mushroom_wood():
    """A shady wood west of the hunter's camp: a fallen giant, shelf fungus, a mushroom ring and a mossy chest."""
    x, y = -7900.0, -300.0
    poi("Mushroom wood & fallen giant", x, y)
    fallen_tree(x - 150.0, y + 120.0, -25.0, 0.62, 3)
    shroom_ring(x + 260.0, y - 180.0, 170.0, 12)
    for k in range(6):
        a, d = R.uniform(0, 2 * math.pi), R.uniform(150.0, 520.0)
        inst(N + "Mushroom_Laetiporus", x + d * math.cos(a), y + d * math.sin(a), None, None, R.uniform(0.5, 0.8))
        inst(N + "Fern_1", x + d * math.cos(a + 0.6), y + d * math.sin(a + 0.6), None, None, R.uniform(0.45, 0.7))
    chest(x + 150.0, y + 330.0, 150.0, "Mossy chest", "Chest")
    stump(x + 450.0, y + 250.0, 1.2, shroom=True)
    stump(x - 350.0, y - 350.0, 0.9, shroom=True)
    rock_cluster(x - 500.0, y + 480.0, 0.8, 3)


def old_oak():
    """The Old Oak: a huge twisted tree alone on the west meadow with a rope swing, a bench and a lantern."""
    x, y = -8000.0, -1900.0
    poi("The Old Oak", x, y)
    if ok(x, y, 200.0, trees=False):
        C.place(N + "TwistedTree_5", x, y, g(x, y) - 30.0, yaw=35.0, scale=1.4, sub="Oak", cull=0.0)
    C.claim(x, y, 350.0)
    _trees.append((x, y, 250.0))
    bench(x + 420.0, y + 330.0, 215.0)
    sx, sy = x + 380.0, y - 300.0      # rope swing
    for dx in (-35.0, 35.0):
        post(sx + dx, sy, 400.0, g(sx, sy) + 55.0, 0.0, 0.18)
    inst(PIR + "Planks_0", sx, sy, g(sx, sy) + 55.0, 0.0, (0.4, 0.7, 1.4), check=False)
    clear(sx, sy, 60.0)
    flower_patch(x, y, 550.0, 14, big=True)
    inst(P + "Lantern_Wall", x - 120.0, y + 60.0, g(x, y) + 170.0, 200.0, 1.0, check=False)
    inst(N + "Mushroom_Laetiporus", x + 60.0, y + 90.0, g(x, y) + 40.0, 120.0, 0.9, check=False)
    clear(x + 300.0, y, 200.0)


def knights_grave():
    """A lone cairn on the ridge path with a sword driven into it, a shield, and fresh flowers."""
    x, y = -7050.0, -4350.0
    poi("Knight's grave", x, y)
    z = C.ground_min(x, y, 100.0)
    for k in range(7):
        a = k * 51.0
        inst(N + f"Pebble_Square_{(k % 6) + 1}", x + 45.0 * math.cos(math.radians(a)), y + 45.0 * math.sin(math.radians(a)),
             z - 2.0, a, 2.2, check=False)
    for k in range(3):
        inst(N + f"Pebble_Round_{k + 1}", x + R.uniform(-15, 15), y + R.uniform(-15, 15), z + 18.0 + k * 14.0, None, 2.0 - k * 0.4,
             check=False)
    inst(P + "Sword_Bronze", x, y, z + 118.0, 20.0, 1.3, pitch=180.0, check=False)
    inst(P + "Shield_Wooden", x + 70.0, y - 40.0, z + 30.0, 200.0, 1.0, pitch=-15.0, check=False)
    flower_patch(x - 60.0, y + 50.0, 90.0, 5, big=True)
    inst(P + "Candle_2", x - 50.0, y - 50.0, z, 0.0, 1.5, check=False)
    C.claim(x, y, 120.0)
    clear(x, y, 60.0)


def stone_circle():
    """Ancient standing-stone circle on the NW ridge: 9 menhirs (one fallen) and a trilithon, a flat altar stone
    with candles and a chalice, a faint cold glow, a mushroom ring; a hidden chest behind the tallest stone."""
    x, y = -8200.0, -6100.0
    poi("Standing-stone circle", x, y)
    zc = g(x, y)
    pts = ring_pts(x, y, 640.0, 9, 12.0)
    tops = {}
    for k, (px, py, a) in enumerate(pts):
        if not C.in_zone(px, py):
            continue
        z = C.ground_min(px, py, 60.0) - 35.0
        tall = 2.35 if k == 4 else (1.75 if k in (0, 1) else R.uniform(1.6, 2.05))
        tops[k] = z + (200.0, 184.0, 200.0)[k % 3] * tall
        if k == 7:   # fallen stone
            C.place(N + "Rock_Medium_2", px, py, g(px, py) - 25.0, yaw=a, roll=82.0, scale=(0.4, 0.44, 1.5), sub="Stones")
            continue
        C.place(N + f"Rock_Medium_{(k % 3) + 1}", px, py, z, yaw=a + 90.0 + R.uniform(-10, 10), pitch=R.uniform(-4, 4),
                roll=R.uniform(-3, 3), scale=(0.34, 0.42, tall), sub="Stones")
        C.claim(px, py, 130.0)
    # trilithon lintel across stones 0 and 1
    (ax, ay, _), (bx, by, _) = pts[0], pts[1]
    mx, my = (ax + bx) / 2.0, (ay + by) / 2.0
    top = min(tops.get(0, 0.0), tops.get(1, 0.0)) - 40.0
    C.place(N + "Rock_Medium_2", mx, my, top, yaw=math.degrees(math.atan2(by - ay, bx - ax)), scale=(1.45, 0.4, 0.35), sub="Stones")
    # altar
    za = C.ground_min(x, y, 100.0) + 25.0
    C.place(N + "RockPath_Square_Wide", x, y, za, yaw=17.0, scale=(1.0, 0.7, 2.2), sub="Stones")
    za += 35.0 * 2.2 - 5.0
    for dx, dy in ((-40.0, -20.0), (30.0, 25.0), (45.0, -30.0)):
        inst(P + "Candle_2", x + dx, y + dy, za, 0.0, 1.4, check=False)
    inst(P + "Chalice", x - 5.0, y + 5.0, za, 0.0, 1.5, check=False)
    flower_patch(x, y, 400.0, 12)
    shroom_ring(x, y, 300.0, 16)
    glow(x, y, zc + 220.0, 10.0, 900.0, (140, 190, 255))
    px, py, _ = ring_pts(x, y, 920.0, 9, 12.0)[4]
    chest(px, py, 0.0, "Ancient cache", "Grave", PIR + "chest_diamond_0")
    copse(x - 900.0, y - 700.0, 5, 700.0, pines=True)
    clear(x, y, 520.0)
    C.ambient("A_Wind_Loop", x, y, zc + 400.0, 0.6)


def fairy_glade():
    """Fairy ring among three trees near the church woods, a spilled basket of carrots and a dropped lantern."""
    x, y = -5450.0, -5550.0
    poi("Fairy ring glade", x, y)
    shroom_ring(x, y + 150.0, 160.0, 13)
    flower_patch(x, y + 150.0, 320.0, 10, big=True)
    C.place(P + "Bucket_Wooden_1", x + 220.0, y + 60.0, g(x + 220.0, y + 60.0), yaw=20.0, roll=80.0, sub="Glade", collide=False,
            cull=5000.0)
    for k in range(5):
        inst(P + "Carrot", x + 250.0 + k * 18.0, y + 110.0 + R.uniform(-15, 15), g(x + 250.0, y + 110.0) + 3.0, None, 0.8,
             pitch=80.0, check=False)
    inst(P + "Lantern_Wall", x - 180.0, y + 230.0, g(x - 180.0, y + 230.0) - 5.0, 60.0, 0.8, roll=70.0, check=False)
    clear(x + 150.0, y + 100.0, 120.0)


# ============================================================================================ POIs: north
def wayside_shrine():
    """Little roofed shrine at the north fork: stone plinth, candles, offerings, flowers, a signpost."""
    x, y = 150.0, -4650.0
    poi("Wayside shrine", x, y)
    z = g(x, y)
    C.place(V + "Stairs_Exterior_Platform", x, y, z - 5.0, yaw=0.0, scale=(0.5, 0.42, 0.95), sub="Shrine")
    for dx in (-48.0, 48.0):
        post(x + dx, y + 12.0, 240.0, z - 5.0, 0.0, 0.9)
    C.place(V + "Roof_Wooden_2x1", x, y - 40.0, z + 225.0, yaw=0.0, scale=(0.55, 0.65, 0.6), sub="Shrine", cull=9000.0)
    C.place(V + "Wall_Arch", x, y + 18.0, z + 90.0, yaw=0.0, scale=(0.45, 1.0, 0.42), sub="Shrine")
    top = z + 90.0
    inst(P + "CandleStick_Triple", x, y + 5.0, top, 0.0, 1.0, check=False)
    for dx, name in ((-30.0, "SM_KG_Bread"), (32.0, "SM_KG_Cheese")):
        inst(IP + name, x + dx, y - 12.0, top, R.uniform(0, 360), 1.0, check=False)
    inst(P + "Coin_Pile", x + 10.0, y - 25.0, top, 0.0, 1.2, check=False)
    inst(P + "Vase_4", x - 60.0, y - 50.0, None, 0.0, 0.8, check=False)
    inst(N + "Flower_4_Single", x - 60.0, y - 50.0, z + 20.0, None, 0.26, check=False)
    glow(x, y - 30.0, z + 150.0, 5.0, 400.0, (255, 190, 110))
    flower_patch(x, y - 80.0, 220.0, 9, big=True)
    clear(x, y - 60.0, 140.0)
    signpost(x + 260.0, y - 120.0, [(-800.0, -8100.0), (-4200.0, -6850.0), (0.0, -2200.0)])


def ambush_cart():
    """A wagon tipped on its side on the north track: spilled crates (smashable), weapons, apples and coins."""
    x, y, a = 250.0, -6000.0, 70.0
    poi("Overturned wagon", x, y)
    if ok(x, y, 150.0, trees=False):
        C.place(V + "Prop_Wagon", x, y, g(x, y) - 12.0, yaw=a, roll=26.0, pitch=4.0, sub="Cart")
    for dx, dy in ((220.0, 160.0), (300.0, -60.0), (-200.0, 230.0)):
        smash(P + ("Crate_Wooden" if dx > 0 else "Barrel_Apples"), x + dx, y + dy)
    inst(P + "FarmCrate_Apple", x + 120.0, y + 330.0, None, 35.0, 1.0, roll=25.0, check=False)
    inst(P + "FarmCrate_Empty", x - 60.0, y + 380.0, None, 80.0, 1.0, roll=-70.0, check=False)
    inst(P + "Sword_Bronze", x - 80.0, y + 260.0, g(x - 80.0, y + 260.0) + 6.0, 30.0, 1.0, roll=90.0, check=False)
    inst(P + "Shield_Wooden", x - 150.0, y + 180.0, g(x - 150.0, y + 180.0) + 4.0, 10.0, 1.0, pitch=-85.0, check=False)
    inst(P + "Coin_Pile_2", x + 60.0, y + 300.0, None, None, 1.2, check=False)
    C.claim(x, y, 250.0)
    clear(x, y + 100.0, 260.0)


def abandoned_cottage():
    """Abandoned cottage: plaster walls with a collapsed corner, a sagging roof, boarded windows, vines, an old
    bed and table inside, a loot chest, an overgrown garden with a scarecrow and a broken fence."""
    x, y, yaw = -800.0, -8150.0, 160.0
    poi("Abandoned cottage", x, y)
    f = C.Frame(x, y, yaw)
    gz = C.ground_min(x, y, 280.0) + 2.0
    plan = {(0.0, 0): "Wall_Plaster_Door_Flat", (0.0, 1): "Wall_Plaster_Window_Wide_Flat",
            (180.0, 0): "Wall_Plaster_WoodGrid", (-90.0, 0): "Wall_Plaster_Window_Wide_Flat",
            (-90.0, 1): "Wall_Plaster_Straight", (90.0, 0): "Wall_Plaster_Straight"}
    for (side, k), piece in plan.items():
        off = -100.0 + k * 200.0
        lx, ly = {0.0: (off, -200.0), 180.0: (-off, 200.0), -90.0: (-200.0, -off), 90.0: (200.0, off)}[side]
        px, py = f.world(lx, ly)
        C.place(V + piece, px, py, gz, yaw=yaw + side, sub="Cottage")
    for sx, sy in ((-1, -1), (1, -1), (-1, 1)):
        px, py = f.world(sx * 200.0, sy * 200.0)
        C.place(V + "Corner_Exterior_Wood", px, py, gz, yaw=yaw, sub="Cottage")
    for i in (-100.0, 100.0):
        for j in (-100.0, 100.0):
            px, py = f.world(i, j)
            C.place(V + "Floor_WoodDark", px, py, gz + 1.0, yaw=yaw, sub="Cottage")
    C.place(V + "Roof_RoundTiles_4x4", x, y, gz + 290.0, yaw=yaw, pitch=-7.0, roll=5.0, sub="Cottage")
    px, py = f.world(380.0, 150.0)
    C.place(V + "Wall_Plaster_Straight", px, py, g(px, py) - 15.0, yaw=yaw + 90.0, roll=84.0, sub="Cottage")
    px, py = f.world(120.0, 370.0)
    C.place(V + "Wall_Plaster_WoodGrid", px, py, g(px, py) + 30.0, yaw=yaw + 180.0, roll=-60.0, sub="Cottage")
    px, py = f.world(100.0, -236.0)    # boards over the window
    for dz in (95.0, 140.0, 175.0):
        inst(PIR + "Planks_1", px, py, gz + dz, yaw + R.uniform(-12, 12), (0.75, 0.6, 1.0), roll=90.0, check=False)
    px, py = f.world(-160.0, -250.0)
    C.place(V + "Door_1_Flat", px, py, gz + 2.0, yaw=yaw + 12.0, roll=-8.0, sub="Cottage")
    for lx, ly, side in ((-120.0, -236.0, 0.0), (-236.0, 80.0, -90.0), (236.0, -60.0, 90.0)):
        px, py = f.world(lx, ly)
        C.place(V + R.choice(["Prop_Vine1", "Prop_Vine2", "Prop_Vine4"]), px, py, gz + 270.0, yaw=yaw + side, sub="Cottage",
                collide=False, cull=9000.0)
    for name, lx, ly, lyaw, pr in (("Bed_Twin2", -90.0, 80.0, 90.0, 0.0), ("Chair_1", 80.0, -60.0, 30.0, 80.0),
                                   ("Cabinet", 150.0, 150.0, 180.0, 0.0), ("Bucket_Metal", -130.0, -120.0, 0.0, 0.0)):
        px, py = f.world(lx, ly)
        C.place(P + name, px, py, gz + 2.0, yaw=yaw + lyaw, roll=pr, sub="Cottage", collide=name != "Bucket_Metal", cull=8000.0)
    px, py = f.world(60.0, 80.0)
    C.place(FURN + "Kitchen_Square_Table", px, py, gz + 2.0, yaw=yaw + 20.0, sub="Cottage", cull=8000.0)
    px, py = f.world(130.0, -150.0)
    chest(px, py, yaw + 90.0, "Old cottage chest", "Chest", None, z=gz + 2.0)
    px, py = f.world(-40.0, 0.0)
    glow(px, py, gz + 150.0, 3.0, 380.0, (255, 150, 80))
    inst(P + "Candle_1", px, py, gz + 2.0, 0.0, 1.5, check=False)
    gx_, gy_ = f.world(-50.0, -620.0)    # overgrown garden with a scarecrow
    for r_ in range(3):
        for c_ in range(5):
            px, py = f.world(-250.0 + c_ * 110.0, -540.0 - r_ * 100.0)
            inst(N + ("Plant_7" if (r_ + c_) % 2 else "Clover_1"), px, py, None, None, R.uniform(0.8, 1.2))
    inst(P + "Carrot", *f.world(-140.0, -640.0), None, None, 1.0, check=False)
    px, py = f.world(320.0, -600.0)
    C.place(P + "Dummy", px, py, g(px, py), yaw=yaw + 180.0, sub="Cottage")
    pts = [f.world(-420.0, -380.0), f.world(-420.0, -900.0), f.world(460.0, -900.0), f.world(460.0, -380.0)]
    fence_run(pts, broken=0.6)
    for lx, ly in ((-330.0, 250.0), (330.0, -330.0)):
        px, py = f.world(lx, ly)
        smash(P + "Vase_2", px, py)
    C.claim(x, y, 330.0)
    C.claim(gx_, gy_, 350.0)
    clear(x, y, 230.0)
    clear(gx_, gy_, 280.0)
    bush_clump(*f.world(-480.0, 200.0), True, 3)


def wishing_well():
    """An old overgrown well in a clearing north of the church: stone ring, winch posts, a little roof, a bucket,
    coins tossed on the rim, candles, flowers."""
    x, y = -4150.0, -6850.0
    poi("Wishing well", x, y)
    z = C.ground_min(x, y, 90.0)
    C.place(V + "Stairs_Exterior_Platform", x, y, z - 10.0, scale=(0.72, 0.72, 0.85), sub="Well")
    for sx in (-1, 1):
        post(x + sx * 80.0, y, 235.0, z - 10.0, 0.0, 1.0)
    beam(x - 95.0, y, z + 215.0, 0.0, 190.0, 1.0)
    C.place(V + "Roof_Wooden_2x1", x, y + 70.0, z + 222.0, yaw=0.0, scale=(1.0, 1.0, 0.9), sub="Well", cull=12000.0)
    inst(P + "Bucket_Wooden_1", x + 30.0, y - 70.0, z + 75.0, 0.0, 1.0, check=False)
    inst(P + "Rope_2", x - 40.0, y - 40.0, z + 75.0, None, 0.6, check=False)
    for k in range(5):
        inst(P + "Coin", x + R.uniform(-55, 55), y + R.uniform(-55, 55), z + 76.0, None, 1.5, pitch=90.0, check=False)
    for dx, dy in ((70.0, 60.0), (-60.0, 70.0)):
        inst(P + "Candle_1", x + dx, y + dy, z + 75.0, 0.0, 1.6, check=False)
    C.place(V + "Prop_Vine4", x - 60.0, y - 55.0, z + 70.0, yaw=0.0, scale=0.6, sub="Well", collide=False, cull=7000.0)
    flower_patch(x, y, 300.0, 12, big=True)
    bench(x + 250.0, y - 260.0, 135.0)
    C.claim(x, y, 180.0)
    clear(x, y, 200.0)


def charcoal_mound():
    """Charcoal burner's mound: an earth mound, stacked billets, a shovel, a bucket and a lean-to."""
    x, y = -2600.0, -8300.0
    poi("Charcoal burner's mound", x, y)
    if ok(x, y, 180.0):
        inst(WP + "SM_KG_DigMound", x, y, g(x, y) - 10.0, 0.0, (2.6, 2.6, 3.2), check=False)
        C.claim(x, y, 220.0)
    log_pile(x + 380.0, y + 120.0, 70.0, 2, 200.0, 26.0)
    inst(WP + "SM_KG_Shovel", x - 260.0, y + 150.0, g(x - 260.0, y + 150.0) + 5.0, 40.0, 1.0, check=False)
    inst(IP + "SM_KG_Firewood", x + 250.0, y - 250.0, None, 200.0, 1.3, check=False)
    smash(P + "Barrel", x - 300.0, y - 200.0)
    inst(P + "Bucket_Metal", x - 220.0, y - 60.0, None, None, 1.0, check=False)
    aframe(x - 150.0, y - 480.0, 20.0, 0.85)
    clear(x, y, 260.0)


# ============================================================================================ POIs: east
def fishing_pond():
    """Woodland pond (pond-water plane in a flat hollow) ringed with rocks and reeds, lily pads, a plank jetty with a
    rowboat, a fishing rod on a stump, a bench and a bucket of the day's catch."""
    x, y, yaw = 6700.0, 1200.0, 15.0
    poi("Woodland fishing pond", x, y)
    f = C.Frame(x, y, yaw)
    ex, ey = 320.0, 220.0
    zs = [g(*f.world(ex * i / 4.0, ey * j / 4.0)) for i in range(-4, 5) for j in range(-4, 5) if i * i + j * j <= 16]
    zw = max(zs) + 3.0
    if ok(x, y, 250.0, trees=False):
        w = C.place("/Engine/BasicShapes/Plane", x, y, zw, yaw=yaw, scale=(ex * 2.0 / 100.0, ey * 2.0 / 100.0, 1.0), sub="Pond",
                    collide=False)
        if w:
            w.get_component_by_class(unreal.StaticMeshComponent).set_material(0, unreal.load_asset("/Game/KillGodot/Materials/M_KG_PondWater"))
    for k in range(24):   # rim: rocks, pebbles, reeds (ellipse slightly larger than the plane corners)
        t = k * 2 * math.pi / 24
        lx, ly = ex * 1.02 * math.cos(t), ey * 1.02 * math.sin(t)
        px, py = f.world(lx, ly)
        if k % 3 == 0:
            inst(N + f"Rock_Medium_{R.randint(1, 3)}", px, py, zw - 20.0, None, R.uniform(0.13, 0.2), check=False)
        else:
            inst(N + R.choice(["Pebble_Round_1", "Pebble_Round_3", "Pebble_Square_2", "Pebble_Round_5"]), px, py, zw - 5.0, None,
                 R.uniform(1.4, 2.0), check=False)
        px, py = f.world(lx * 1.15, ly * 1.2)
        inst(N + R.choice(["Plant_1", "Fern_1", "Clover_2"]), px, py, None, None, R.uniform(0.35, 0.55))
    # plane corners: the water is an ellipse visually, hide the rectangle corners under reed clumps
    for sx in (-1, 1):
        for sy in (-1, 1):
            px, py = f.world(sx * ex * 0.92, sy * ey * 0.92)
            inst(N + "Bush_Common", px, py, zw - 30.0, None, 0.55, sink=0.0, check=False)
    for k in range(6):   # lily pads + blossoms
        px, py = f.world(R.uniform(-ex * 0.6, ex * 0.6), R.uniform(-ey * 0.55, ey * 0.55))
        inst(N + "Plant_7", px, py, zw - 6.0, None, R.uniform(0.35, 0.5), check=False)
        if k % 2 == 0:
            inst(N + "Petal_4", px + 10.0, py, zw - 2.0, None, 0.8, check=False)
    for k in range(4):   # jetty
        px, py = f.world(150.0 + k * 34.0, -150.0 + k * 42.0)
        inst(PIR + "Planks_1", px, py, zw + 10.0, yaw + 125.0, (0.55, 1.1, 1.0), check=False)
    for lx, ly in ((125.0, -125.0), (245.0, 20.0)):
        px, py = f.world(lx, ly)
        post(px, py, 60.0, zw - 30.0, 0.0, 0.7)
    px, py = f.world(-80.0, 30.0)
    C.place(WP + "SM_KG_Rowboat", px, py, zw - 6.0, yaw=yaw + 70.0, sub="Pond", collide=False, cull=12000.0)
    px, py = f.world(-60.0, -330.0)
    stump(px, py, 1.0)
    inst(WP + "SM_KG_FishingRod", px, py, g(px, py) + 60.0, yaw + 100.0, 1.0, pitch=18.0, check=False)
    px, py = f.world(50.0, -340.0)
    inst(P + "Bucket_Wooden_1", px, py, None, 0.0, 1.0, check=False)
    inst(WP + "SM_KG_Fish_Mackerel", px, py, g(px, py) + 24.0, 30.0, 1.0, check=False)
    bench(*f.world(-340.0, -300.0), yaw + 50.0)
    signpost(6300.0, 1620.0, [(8550.0, -1850.0), (9750.0, 700.0), (6700.0, 1200.0)])
    lantern_post(*f.world(280.0, -320.0), yaw, light=True)
    clear(x, y, 330.0)
    C.claim(x, y, 400.0)
    C.ambient("A_Lapping_Loop", x, y, zw + 50.0, 0.35)
    C.ambient("A_Birds_Loop", x + 500.0, y - 600.0, zw + 600.0, 0.6)


def berry_meadow():
    """Berry bushes and wildflowers among trees on the east slope, baskets of fruit, bee skeps on a bench, and a
    picnic blanket with bread, cheese and a bottle."""
    x, y = 7500.0, -1350.0
    poi("Berry meadow & picnic", x, y)
    for k in range(10):
        a, d = k * 0.63 + R.uniform(-0.2, 0.2), R.uniform(200.0, 560.0)
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        if ok(px, py, 80.0):
            inst(N + "Bush_Common_Flowers", px, py, None, None, R.uniform(0.75, 1.05), sink=10.0, check=False)
            C.claim(px, py, 90.0)
    for dx, dy in ((80.0, 120.0), (-150.0, 60.0)):
        inst(P + "FarmCrate_Apple", x + dx, y + dy, None, None, 1.0, check=False)
    inst(P + "Barrel_Apples", x - 60.0, y + 200.0, None, 20.0, 1.0, check=False)
    tx, ty = x + 250.0, y - 250.0
    C.place(P + "Table_Large", tx, ty, g(tx, ty), yaw=30.0, scale=(0.55, 0.8, 0.85), sub="Berries")
    for k in range(3):
        inst(P + "Vase_2", tx - 60.0 + k * 60.0, ty - 30.0 + k * 30.0, g(tx, ty) + 68.0, None, 0.7, check=False)
    clear(x, y, 200.0)
    # picnic
    px, py = 8000.0, -1050.0
    if ok(px, py, 150.0):
        z = g(px, py)
        C.place(IP + "SM_KG_Rug_Rect", px, py, z + 3.0, yaw=25.0, sub="Picnic", collide=False, cull=7000.0)
        for name, dx, dy in (("SM_KG_Bread", -40.0, 10.0), ("SM_KG_Cheese", 30.0, -20.0)):
            inst(IP + name, px + dx, py + dy, z + 4.0, None, 1.2, check=False)
        for name, dx, dy in (("Bottle_1", 60.0, 30.0), ("Mug", 20.0, 40.0), ("Mug", -60.0, -35.0), ("Table_Plate", -10.0, -10.0)):
            inst(P + name, px + dx, py + dy, z + 4.0, None, 1.0, check=False)
        inst(P + "Bag", px + 140.0, py + 60.0, None, 200.0, 0.9, check=False)
        inst(P + "FarmCrate_Apple", px - 150.0, py + 40.0, None, 60.0, 0.9, check=False)
        C.claim(px, py, 160.0)
        clear(px, py, 170.0)
    flower_patch(x - 300.0, y - 200.0, 320.0, 12, big=True)
    bush_clump(x + 500.0, y + 300.0, True, 3)


def east_lookout():
    """Hilltop lookout on the east hill: a signal-fire beacon on a stone base, a bench facing the village and the
    lighthouse, a flag, firewood and a supply crate."""
    x, y = 8550.0, -1850.0
    poi("East hill lookout & beacon", x, y)
    z = C.ground_min(x, y, 150.0)
    C.place(V + "Stairs_Exterior_Platform", x, y, z - 30.0, yaw=15.0, scale=(0.9, 0.9, 1.0), sub="Lookout")
    C.place(N + "RockPath_Round_Wide", x, y, z + 62.0, yaw=40.0, scale=(0.7, 0.7, 1.0), sub="Lookout", collide=False)
    for k in range(4):
        a = k * 90.0 + 45.0
        log(x + 22.0 * math.cos(math.radians(a)), y + 22.0 * math.sin(math.radians(a)), z + 115.0, a + 180.0,
            length=120.0, dia=13.0, elev=-48.0)
    glow(x, y, z + 190.0, 14.0, 1100.0, (255, 130, 50))
    C.ambient("A_Fire_Loop", x, y, z + 120.0, 0.6)
    C.place(PIR + "FlagLow_0", x + 170.0, y - 120.0, g(x + 170.0, y - 120.0) - 10.0, yaw=0.0, scale=0.45, sub="Lookout")
    C.place(PIR + "FlagLow_1", x + 170.0, y - 120.0, g(x + 170.0, y - 120.0) + 225.0, yaw=0.0, scale=(0.7, 1.0, 0.7),
            sub="Lookout", collide=False)
    bench(x - 320.0, y + 260.0, 170.0)
    C.place(P + "Crate_Wooden", x + 200.0, y + 150.0, g(x + 200.0, y + 150.0), yaw=20.0, sub="Lookout")
    inst(IP + "SM_KG_Firewood", x - 180.0, y - 170.0, None, 30.0, 1.2, check=False)
    inst(IP + "SM_KG_Firewood", x - 120.0, y - 230.0, None, 60.0, 1.1, check=False)
    for k in range(3):
        a = 200.0 + k * 50.0
        seat_log(x + 260.0 * math.cos(math.radians(a)), y + 260.0 * math.sin(math.radians(a)), a + 180.0, 170.0, 44.0)
    clear(x, y, 300.0)
    C.ambient("A_Wind_Loop", x, y, z + 400.0, 0.5)


def smugglers_cave():
    """Cave mouth in the north wall of the east dell, facing south: two rock jambs, a lintel slab and a roof mass
    against the hillside make a dark mouth; a torch, crates, a coin trail and a treasure chest inside."""
    x, y = 9750.0, 700.0          # mouth centre; the cave runs north (-Y) into the hill
    poi("Smugglers' cave", x, y)
    f = C.Frame(x, y, 0.0)       # local -Y = into the hill, +Y = out to the dell
    zm = g(x, y)
    # jambs (Rock_Medium_3: yaw so its bulk sits outside the opening)
    for side in (-1, 1):
        px, py = f.world(side * 330.0, -60.0)
        C.place(N + "Rock_Medium_3", px, py, g(px, py) - 50.0, yaw=90.0 + 90.0 * side, scale=(1.05, 0.95, 1.55), sub="Cave")
    px, py = f.world(0.0, -430.0)     # back mass
    C.place(N + "Rock_Medium_1", px, py, g(px, py) - 60.0, yaw=180.0, scale=(2.3, 1.7, 2.1), sub="Cave")
    px, py = f.world(0.0, -150.0)     # roof mass over the mouth
    C.place(N + "Rock_Medium_2", px, py, zm + 250.0, yaw=0.0, roll=6.0, scale=(2.3, 1.7, 0.95), sub="Cave")
    C.claim(x, y - 200.0, 500.0)
    ix, iy = f.world(0.0, -190.0)
    chest(ix, iy, 90.0, "Smugglers' hoard", "Chest", PIR + "chest_gold_0")
    for lx, ly, name in ((-160.0, -200.0, "crates_1"), (170.0, -170.0, "Barrel_5"), (150.0, -60.0, "crates_0")):
        px, py = f.world(lx, ly)
        C.place(PIR + name, px, py, g(px, py), yaw=R.uniform(0, 360), sub="Cave")
    for k in range(6):   # coin trail spilling out
        px, py = f.world(R.uniform(-40, 40), -120.0 + k * 55.0)
        inst(P + ("Coin_Pile" if k % 2 else "Coin_Pile_2"), px, py, None, None, 1.4, check=False)
    tx, ty = f.world(-200.0, 120.0)
    C.place(PIR + "Torch_1", tx, ty, g(tx, ty) - 5.0, yaw=90.0, sub="Cave")
    glow(tx, ty + 40.0, g(tx, ty) + 240.0, 12.0, 700.0, (255, 150, 70))
    glow(ix, iy + 60.0, zm + 90.0, 4.0, 300.0, (255, 200, 90))      # the hoard glints
    inst(P + "Rope_3", *f.world(80.0, 150.0), None, None, 1.0, check=False)
    smash(PIR + "crates_0", *f.world(320.0, 260.0))
    smash(PIR + "Barrel_9", *f.world(-330.0, 300.0))
    rock_cluster(*f.world(550.0, 200.0), 0.6, 3)
    clear(x, y, 250.0)


def treasure_dig():
    """X marks the spot: a half-dug hole in the dell bowl, a shovel, a lantern, a map scroll, and a buried chest."""
    x, y = 10050.0, 1250.0
    poi("Buried treasure dig", x, y)
    inst(WP + "SM_KG_DugHole", x, y, g(x, y) - 2.0, 0.0, 1.4, check=False)
    inst(WP + "SM_KG_DigMound", x + 170.0, y + 60.0, None, 30.0, 1.3, check=False)
    chest(x, y, 30.0, "Buried chest", "Chest", PIR + "chest_common_0", z=g(x, y) - 50.0)
    inst(WP + "SM_KG_Shovel", x - 110.0, y + 90.0, g(x - 110.0, y + 90.0) + 40.0, 50.0, 1.0, pitch=-35.0, check=False)
    inst(P + "Scroll_1", x - 150.0, y - 60.0, None, 20.0, 2.0, check=False)
    inst(P + "Lantern_Wall", x + 130.0, y - 120.0, g(x + 130.0, y - 120.0) - 4.0, 70.0, 0.8, check=False)
    for k in range(2):   # an X of planks
        inst(PIR + "Planks_0", x - 220.0, y - 180.0, g(x - 220.0, y - 180.0) + 2.0 + k * 3.0, 45.0 + 90.0 * k, (0.6, 0.6, 1.0),
             check=False)
    clear(x, y, 200.0)


def drystone_wall(pts, h=0.3, gap_every=0):
    """Low field-boundary wall of kit brick sections following the ground, gaps where trails cross, a few
    toppled stones. pts: polyline (cm)."""
    k = 0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        n = max(1, int(round(seg / 200.0)))
        a = math.degrees(math.atan2(by - ay, bx - ax))
        for i in range(n):
            t = (i + 0.5) / n
            x, y = ax + (bx - ax) * t, ay + (by - ay) * t
            k += 1
            if not C.in_zone(x, y) or C.lane_distance(x, y) < 150.0 or tree_hit(x, y, 90.0) or C.overlaps(x, y, 60.0):
                continue
            if gap_every and k % gap_every == 0:
                for _ in range(3):
                    inst(V + f"Prop_Brick{R.randint(1, 4)}", x + R.uniform(-90, 90), y + R.uniform(-90, 90), None, None,
                         R.uniform(1.4, 2.0), pitch=R.uniform(-20, 20), check=False)
                continue
            z = C.ground_min(x, y, 100.0) - 12.0
            C.place(V + "Wall_UnevenBrick_Straight", x, y, z, yaw=a + R.uniform(-3, 3), roll=R.uniform(-3, 3),
                    scale=(1.02, 1.0, h * R.uniform(0.85, 1.15)), sub="Walls", cull=14000.0)
            C.claim(x, y, 110.0)
            if R.random() < 0.3:
                inst(N + R.choice(["Fern_1", "Clover_2", "Bush_Common_Flowers"]), x + R.uniform(-60, 60), y + R.uniform(-60, 60),
                     None, None, R.uniform(0.4, 0.6))


def sheep_fold():
    """Round stone sheep fold on the east slope with a gate gap, a trough, a feed corner, a shepherd's lean-to
    and an oar-crook leaning by the gate."""
    x, y = 9000.0, -700.0
    poi("Shepherd's fold", x, y)
    for k, (px, py, a) in enumerate(ring_pts(x, y, 430.0, 13, 0.0)):
        if k == 6:   # gate gap facing west (towards the village)
            post(px, py - 60.0, 140.0, None, 0.0, 1.1)
            post(px, py + 60.0, 140.0, None, 0.0, 1.1)
            continue
        if not C.in_zone(px, py):
            continue
        C.place(V + "Wall_UnevenBrick_Straight", px, py, C.ground_min(px, py, 100.0) - 12.0, yaw=a + 90.0,
                scale=(1.05, 1.0, 0.32), sub="Fold", cull=14000.0)
    C.claim(x, y, 470.0)
    clear(x, y, 360.0)
    for k in range(5):
        inst(IP + "SM_KG_Firewood", x + 150.0 + R.uniform(-40, 40), y - 180.0 + k * 45.0, None, 100.0, 1.0, check=False)
    inst(N + "Plant_7_Big", x + 60.0, y + 150.0, None, None, 0.9, check=False)
    C.place(P + "FarmCrate_Empty", x - 120.0, y + 60.0, g(x - 120.0, y + 60.0), yaw=90.0, scale=(1.6, 1.2, 1.3), sub="Fold")
    inst(P + "Bucket_Wooden_1", x - 170.0, y + 140.0, None, None, 1.0, check=False)
    inst(P + "Bucket_Metal", x - 60.0, y - 120.0, None, None, 1.0, check=False)
    aframe(x + 700.0, y + 250.0, -70.0, 0.9)
    inst(WP + "SM_KG_Oar", x - 470.0, y + 100.0, g(x - 470.0, y + 100.0) + 70.0, 10.0, 0.7, pitch=-70.0, check=False)
    flower_patch(x - 700.0, y, 250.0, 8)


# ============================================================================================ trails + fill
TRAILS = {
    "w_tower": [(-5650.0, 450.0), (-6300.0, 1100.0), (-7000.0, 1300.0), (-7700.0, 1500.0), (-8300.0, 1550.0)],
    "w_view": [(-8500.0, 2050.0), (-8250.0, 2250.0)],
    "w_hunter": [(-5850.0, 50.0), (-6000.0, -700.0), (-6150.0, -1400.0), (-6250.0, -1950.0)],
    "w_stones": [(-6500.0, -2700.0), (-6700.0, -3500.0), (-7000.0, -4200.0), (-7400.0, -5000.0), (-7700.0, -5550.0)],
    "w_trap": [(-6200.0, -2700.0), (-5800.0, -3000.0)],
    "w_wood": [(-6100.0, -150.0), (-6900.0, -300.0), (-7500.0, -350.0)],
    "w_oak": [(-6650.0, -2150.0), (-7200.0, -2050.0), (-7650.0, -1950.0)],
    "n_main": [(100.0, -4100.0), (150.0, -4450.0), (250.0, -5000.0), (150.0, -5600.0), (-150.0, -6700.0), (-500.0, -7400.0),
               (-700.0, -7800.0)],
    "n_west": [(-500.0, -7100.0), (-1500.0, -7300.0), (-2600.0, -7700.0), (-3500.0, -7300.0), (-4000.0, -7000.0),
               (-4800.0, -7050.0), (-5700.0, -6600.0), (-6600.0, -6000.0), (-7500.0, -5600.0)],
    "n_glade": [(-5700.0, -6600.0), (-5500.0, -5900.0)],
    "e_main": [(6280.0, 1350.0), (6450.0, 850.0), (6900.0, 600.0), (7400.0, 400.0), (7800.0, 350.0)],
    "e_hill": [(7800.0, 350.0), (7650.0, -500.0), (7800.0, -1000.0), (8100.0, -1400.0), (8350.0, -1650.0)],
    "e_cave": [(7800.0, 350.0), (8400.0, 650.0), (9000.0, 900.0), (9400.0, 1050.0), (9700.0, 950.0)],
    "e_dig": [(9750.0, 1050.0), (9950.0, 1200.0)],
    "e_fold": [(7700.0, -400.0), (8200.0, -600.0), (8550.0, -700.0)],
}


def trails():
    for name, pts in TRAILS.items():
        trail(pts, kind="mixed" if name in ("w_hunter", "e_main", "n_west") else "stone")
    signpost(-6150.0, -2650.0, [(-8200.0, -6100.0), (-5750.0, -3300.0), (-6000.0, 800.0)])
    signpost(-500.0, -7250.0, [(-800.0, -8150.0), (-4150.0, -6850.0), (150.0, -4650.0)])
    signpost(7950.0, 500.0, [(8550.0, -1850.0), (9750.0, 700.0), (6700.0, 1200.0)])
    signpost(-5600.0, -6450.0, [(-8200.0, -6100.0), (-5450.0, -5550.0), (-4150.0, -6850.0)])
    signpost(-6500.0, -1950.0, [(-8000.0, -1900.0), (-6350.0, -2250.0)])


def edge_fill():
    """Vignettes every ~7 m along the village edge and the wilds belt: copses, rocks, bushes, flower patches,
    stumps, logs, mushroom rings. Candidates on rings around the plaza, kept only where they land in the wilds."""
    placed = 0
    for ring_r in (4800.0, 5350.0, 5900.0, 6450.0, 7000.0, 7550.0, 8100.0, 8650.0, 9200.0, 9750.0, 10300.0):
        n = int(2 * math.pi * ring_r / 560.0)
        for k in range(n):
            a = 2 * math.pi * k / n + R.uniform(-0.03, 0.03)
            x = C.PLAZA[0] + ring_r * math.cos(a) * 1.05 + R.uniform(-250, 250)
            y = C.PLAZA[1] + ring_r * math.sin(a) * 0.95 + R.uniform(-250, 250)
            if not (-10500.0 < x < 11000.0 and -10000.0 < y < 3200.0):
                continue
            if not C.free(x, y, 150.0) or tree_hit(x, y, 180.0):
                continue
            pick = R.random()
            if pick < 0.16:
                copse(x, y, R.randint(2, 4), 500.0)
            elif pick < 0.28:
                rock_cluster(x, y, R.uniform(0.55, 1.0), R.randint(2, 4))
            elif pick < 0.44:
                bush_clump(x, y, R.random() < 0.7, R.randint(2, 4))
                flower_patch(x + 150.0, y, 150.0, 4)
            elif pick < 0.62:
                flower_patch(x, y, 280.0, R.randint(8, 13), big=R.random() < 0.35)
            elif pick < 0.7:
                stump(x, y, R.uniform(0.8, 1.2), shroom=True)
                flower_patch(x, y, 120.0, 3)
            elif pick < 0.78:
                log(x, y, C.ground_min(x, y, 120.0) + 16.0, R.uniform(0, 360), length=R.uniform(200, 320), dia=R.uniform(30, 42))
                inst(N + "Fern_1", x + 120.0, y + 60.0, None, None, 0.5, check=False)
                inst(N + "Mushroom_Common", x - 60.0, y + 40.0, None, None, 0.7, check=False)
            elif pick < 0.84:
                shroom_ring(x, y, R.uniform(90, 140), 9)
            elif pick < 0.9:     # young sapling with a ring of stones
                inst(N + f"CommonTree_{R.randint(1, 5)}", x, y, g(x, y) - 8.0, None, R.uniform(0.35, 0.5), check=False)
                for k in range(4):
                    inst(N + f"Pebble_Round_{R.randint(1, 5)}", x + 70.0 * math.cos(k * 1.57), y + 70.0 * math.sin(k * 1.57), None,
                         None, 1.2, check=False)
                _trees.append((x, y, 60.0))
            elif pick < 0.95:    # standing dead snag with shelf fungus
                inst(N + f"DeadTree_{R.randint(1, 3)}", x, y, g(x, y) - 20.0, None, R.uniform(0.35, 0.55), check=False)
                inst(N + "Mushroom_Laetiporus", x + 30.0, y, g(x, y) + 60.0, None, 0.6, check=False)
                _trees.append((x, y, 90.0))
            else:                # fern hollow
                for k in range(4):
                    inst(N + "Fern_1", x + R.uniform(-160, 160), y + R.uniform(-160, 160), None, None, R.uniform(0.5, 0.8))
                inst(N + "Mushroom_Common", x, y, None, None, 0.8, check=False)
            C.claim(x, y, 180.0)
            placed += 1
    return placed


def ground_cover():
    """Cheap non-colliding scatter for texture: pebbles, clover, petals and ferns across the near wilds."""
    def pick():
        a, d = R.uniform(0, 2 * math.pi), R.uniform(5000.0, 10500.0)
        return C.PLAZA[0] + d * math.cos(a), C.PLAZA[1] + d * math.sin(a)
    C.scatter([N + f"Pebble_Round_{i}" for i in range(1, 6)], 350, pick, 40.0, sub="Scatter", scale=(0.8, 1.4), cull=5000.0)
    C.scatter([N + "Petal_1", N + "Petal_2", N + "Petal_3", N + "Petal_5", N + "Clover_1", N + "Clover_2"], 600, pick, 55.0,
              sub="Scatter", scale=(0.9, 1.5), cull=5500.0)
    C.scatter([N + "Fern_1", N + "Plant_1", N + "Plant_7_Big"], 220, pick, 90.0, sub="Scatter", scale=(0.45, 0.8), cull=7000.0)
    C.scatter([N + "Bush_Common", N + "Bush_Common_Flowers"], 120, pick, 120.0, sub="Scatter", scale=(0.55, 0.9), cull=9000.0)
    C.scatter([N + "Mushroom_Common"], 120, pick, 40.0, sub="Scatter", scale=(0.5, 0.9), cull=4500.0)


# ============================================================================================ entry
def load_existing():
    """Claim the builder's trees/rocks/woodcutter props so nothing is spawned into a trunk."""
    for a in C.actors.get_all_level_actors():
        f = str(a.get_folder_path())
        if not (f.startswith("Nature/Trees") or f.startswith("Nature/Pines") or f.startswith("Nature/Rocks")
                or f.startswith("Village/Woodcutter")):
            continue
        l = a.get_actor_location()
        s = a.get_actor_scale3d().x
        r = 130.0 * s if "Rocks" in f else (60.0 if "Woodcutter" in f else 110.0)
        _trees.append((l.x, l.y, r))
        C.claim(l.x, l.y, r)


def dress():
    R.seed(90210)
    _batch.clear()
    _trees.clear()
    _meadow.clear()
    POIS.clear()
    load_existing()
    # POIs first (they claim their space), then trails, then the generic fill.
    woodcutter_yard()
    watchtower_ruin()
    viewpoint_west()
    hunter_camp()
    trapper_line()
    mushroom_wood()
    old_oak()
    knights_grave()
    stone_circle()
    fairy_glade()
    wayside_shrine()
    ambush_cart()
    abandoned_cottage()
    wishing_well()
    charcoal_mound()
    fishing_pond()
    berry_meadow()
    east_lookout()
    smugglers_cave()
    treasure_dig()
    sheep_fold()
    trails()
    drystone_wall([(8150.0, -2350.0), (8350.0, -1100.0), (8250.0, -100.0), (8600.0, 800.0), (8900.0, 1500.0)], 0.3, gap_every=7)
    drystone_wall([(-7300.0, -3200.0), (-7900.0, -3000.0), (-8700.0, -3100.0), (-9300.0, -2700.0)], 0.28, gap_every=6)
    drystone_wall([(-2000.0, -7050.0), (-1300.0, -6700.0), (-900.0, -6300.0)], 0.3, gap_every=5)
    C.stats["edge_vignettes"] = edge_fill()
    ground_cover()
    flush()
    C.stats["pois"] = len(POIS)
    C.stats["skipped"] = dict(skipped)
    print("KG_WILDS_POIS", POIS)
    print("KG_WILDS_FAILS", FAILS[:40])
