"""Harbour zone dressing: Morrowmere's busy fishing port (the waterfront, y > 36 m, -45 < x < 45 m).

Story beats, west -> east (world cm, X east, +Y towards the sea, the waterline is at y ~5600):
  * Smugglers' cove camp (-4000, 5000): campfire, stools, rum barrels, a hidden pirate chest by the rocks.
  * Burnt old pier (-2350, 5700..7700): charred pilings sticking out of the bay, lost cargo bobbing about.
  * Smokehouse (-2900, 4600): open shed on the slope, fish racks over a fire, salt barrels, fishing chest.
  * Drying racks (-3500..-2300, 5000..5200): fish and sailcloth drying in the wind; beached boats below.
  * Boat yard + slipway (-2000..-1100, 4900..5950): upturned hull on trestles, tar pot, a boat on the ramp.
  * Pier gate (0, 5130): lantern arch with a signboard over the main pier; pilings and moored boats along it.
  * Fish market additions (400, 4600): fish on every table, fish crates, baskets; net mending corner (800, 4700).
  * Harbourmaster's office (1000, 5250): 4x4 m stilt hut with desk, ledgers and a strongbox, tally desk outside.
  * Chowder stand (1600..2150, 4500..4950): stall, simmering cauldron, tables and stools facing the sea.
  * Fishermen's jetty (2200, 5120..7320): plank jetty with a T-head, moored boats, rods, crates, lanterns.
  * Cargo hoist (2520, 5060): mast + boom with a crate hanging over the shallows, winch, wagon, crate stacks.
  * Lobster yard (3100..3700, 4800..5400): trap stacks, buoy heaps, repair bench, beached lobster boat.
  * Breakwater (3700..4400, 5600..7000): rock arm into the sea with a beacon.
  * The whole waterline: driftwood, seaweed, shells, rocks, planks; buoys bobbing all over the bay.
"""
import math
import random

import unreal

import kg_dress_common as C

P, V, N, PIR, WP, JP = C.P, C.V, C.N, C.PIR, C.WP, C.JP
K = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_"
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/SM_KG_"
POST = V + "Corner_Exterior_Wood"          # 21x24x300, pivot at the base
POSTW = V + "Corner_ExteriorWide_Wood"     # 29x33x300
TILE = V + "Floor_WoodDark"                # 200x200x2, centred
BOAT = WP + "SM_KG_Rowboat"                # 328x129x89, length along X, pivot 31 above the keel
BOBBER = WP + "SM_KG_Bobber"
FISH = [WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Mackerel", WP + "SM_KG_Fish_Salmon"]
CAGE = P + "Cage_Small"                    # lobster trap stand-in, 85x88x81
P1 = "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_"   # pack_village.md (front = +Y)
R = random.Random(3107)

PIER_DECK = 110.0
JX, J_DECK = 2200.0, 100.0                 # fishermen's jetty centre line / deck top
J_Y0, J_Y1 = 5120.0, 6920.0                # stem
T_X0, T_X1, T_Y1 = 1700.0, 2700.0, 7320.0  # T-head

WARM = (255, 170, 95)
FIRE = (255, 130, 60)


# ============================================================================================ helpers
_B = {}


def inst(path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=6000.0):
    """Queue an instance (flushed per mesh at the end). The first call per mesh fixes collide/cull."""
    b = _B.setdefault(path, {"tr": [], "collide": collide, "cull": cull})
    b["tr"].append((x, y, z, yaw, scale, pitch, roll))


def flush():
    for path, b in _B.items():
        C.instanced(path, b["tr"], collide=b["collide"], cull=b["cull"], sub="Instanced")
    _B.clear()


def g(x, y):
    return C.ground(x, y)


def warn(msg):
    C.stats.setdefault("warn", []).append(msg)


def put(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, sub="Props", collide=True, cull=0.0, r=0.0,
        sink=0.0, shadow=True):
    a = C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=scale, sub=sub, collide=collide, cull=cull,
                shadow=shadow, sink=sink)
    if r > 0.0:
        C.claim(x, y, r)
    return a


def post(x, y, z0, z1, th=1.0, wide=False, collide=True):
    """Vertical wooden post from z0 up to z1 (instanced, collides)."""
    inst(POSTW if wide else POST, x, y, z0, R.uniform(0, 360) if not wide else 0.0,
         (th, th, max(0.05, (z1 - z0) / 300.0)), collide=True, cull=0.0)


def beam(a, b, th=1.0, path=POST, unit=300.0, collide=True, cull=0.0):
    """Wooden beam from point a to point b (3D, cm), instanced (a pivot-at-base post mesh stretched along Z)."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    elev = math.degrees(math.asin(max(-1.0, min(1.0, dz / max(1e-3, length)))))
    inst(path, a[0], a[1], a[2], yaw, (th, th, length / unit), pitch=elev - 90.0, collide=collide, cull=cull)


def rope(a, b, th=0.22):
    """Thin tarred line between two points (a stretched post actor, no collision): rigging, mooring lines."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    elev = math.degrees(math.asin(max(-1.0, min(1.0, dz / max(1e-3, length)))))
    put(POST, a[0], a[1], a[2], yaw=yaw, pitch=elev - 90.0, scale=(th, th, length / 300.0), sub="Rigging",
        collide=False, cull=9000.0, shadow=False)


def logs_fire(x, y, gz=None, n=5, size=1.0, light=0.0):
    """Campfire: short logs leaning together in a teepee over a ring of stones (optional ember light)."""
    gz = g(x, y) if gz is None else gz
    for k in range(n):
        a = k * 2.0 * math.pi / n + R.uniform(-0.2, 0.2)
        bx, by = x + 38.0 * size * math.cos(a), y + 38.0 * size * math.sin(a)
        beam((bx, by, gz - 4.0), (x + 4.0 * math.cos(a), y + 4.0 * math.sin(a), gz + 48.0 * size), 1.3 * size)
    for k in range(9):
        a = k * 2.0 * math.pi / 9
        inst(N + "Pebble_Round_%d" % (k % 5 + 1), x + 68.0 * size * math.cos(a), y + 68.0 * size * math.sin(a),
             gz - 2.0, R.uniform(0, 360), 1.3 * size, cull=6000.0)
    if light > 0.0:
        C.light(x, y, gz + 90.0, light, 850.0, FIRE)
    C.claim(x, y, 85.0 * size)


def woodpile(x, y, yaw, rows=3, per=5, length=0.32):
    """Stack of split logs (short lying beams) against a wall or fence."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    gz = C.ground_min(x, y, 80.0)
    for r in range(rows):
        for k in range(per - r):
            off = (k - (per - r - 1) / 2.0) * 30.0
            bx, by = x - off * s, y + off * c
            a = (bx - c * 48.0, by - s * 48.0, gz + 12.0 + r * 26.0)
            b = (bx + c * 48.0 * length / 0.32, by + s * 48.0 * length / 0.32, gz + 12.0 + r * 26.0)
            beam(a, b, 1.25)
    C.claim(x, y, 90.0)


def lantern_post(x, y, yaw, light=0.0, radius=900.0, tall=1.0):
    """3 m post with the kit's wall lantern; the lantern arm points along local +Y after yaw."""
    z = g(x, y) - 5.0
    post(x, y, z, z + 300.0 * tall + 5.0, 1.1)
    put(P + "Lantern_Wall", x, y, z + 175.0 * tall + 5.0, yaw=yaw, sub="Lanterns", cull=12000.0)
    C.claim(x, y, 45.0)
    if light > 0.0:
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        C.light(x - 105.0 * s, y + 105.0 * c, z + 230.0 * tall, light, radius, WARM)


def fish_crate(x, y, z, yaw, n=4, crate=True):
    """Shallow crate (or a table top) of fresh fish."""
    if crate:
        inst(P + "FarmCrate_Empty", x, y, z + 2.0, yaw, 1.0, cull=7000.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for k in range(n):
        off = (k - (n - 1) / 2.0) * (10.0 if crate else 16.0)
        fx, fy = x - off * s, y + off * c
        inst(R.choice(FISH), fx, fy, z + (12.0 if crate else 6.0) + (k % 2) * 3.0, yaw + R.uniform(-12, 12),
             R.uniform(0.9, 1.1), roll=90.0, cull=6000.0)


def crate_stack(x, y, yaw, layout, breakable_top=False):
    """layout: list of (dx, dy, level) in crate units; wooden crates, collide."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    gz = C.ground_min(x, y, 90.0)
    for (dx, dy, lvl) in layout:
        lx, ly = dx * 88.0, dy * 92.0
        wx, wy = x + lx * c - ly * s, y + lx * s + ly * c
        inst(P + "Crate_Wooden", wx, wy, gz + 5.0 + lvl * 88.0, yaw + R.uniform(-6, 6), 1.0, collide=True, cull=12000.0)
    C.claim(x, y, 60.0 + 45.0 * max(abs(d[0]) + abs(d[1]) for d in layout))


def barrel_group(x, y, n=3, r=55.0, pirate=True):
    gz = C.ground_min(x, y, 60.0)
    for k in range(n):
        a = k * 2.0 * math.pi / max(1, n) + R.uniform(-0.3, 0.3)
        bx, by = x + (r if n > 1 else 0.0) * math.cos(a), y + (r if n > 1 else 0.0) * math.sin(a)
        if pirate:
            inst(PIR + "Barrel_%d" % R.choice([0, 3, 4, 10]), bx, by, gz, R.uniform(0, 360), 0.72, collide=True,
                 cull=12000.0)
        else:
            inst(P + "Barrel", bx, by, gz, R.uniform(0, 360), 1.0, collide=True, cull=12000.0)
    C.claim(x, y, r + 50.0)


def seat(path, x, y, face_yaw, z=None, height=58.2, stand=75.0):
    """AKGSeat whose sitter faces world yaw `face_yaw`. Furniture meshes face their local +Y (kg_interiors
    convention): actor yaw = face, mesh rotated -90."""
    s = C.seat(path, x, y, z, yaw=face_yaw, seat_height=height)
    try:
        s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
        s.set_editor_property("stand_distance", stand)
    except Exception:
        pass
    C.claim(x, y, 35.0)
    return s


def breakable(path, x, y, z=None, yaw=None):
    C.breakable(path, x, y, z, R.uniform(0, 360) if yaw is None else yaw)
    C.claim(x, y, 50.0)


def buoy(x, y, big=False):
    """Red/white float (the fishing bobber scaled up), bobbing on the swell."""
    sc = R.uniform(6.5, 8.0) if big else R.uniform(4.5, 6.0)
    C.mover(BOBBER, x, y, -12.0 * sc / 6.0, yaw=R.uniform(0, 360), roll=R.uniform(-8, 8), scale=sc,
            bob=R.uniform(8.0, 14.0), sway=R.uniform(3.0, 6.0), sway_hz=R.uniform(0.2, 0.35), sub="Buoys")


def float_barrel(x, y):
    C.mover(PIR + "Barrel_%d" % R.choice([0, 3, 4]), x, y, -40.0, yaw=R.uniform(0, 360), roll=90.0, scale=0.7,
            bob=R.uniform(8.0, 12.0), sway=3.0, sway_hz=0.22, sub="Buoys")


def moored_boat(x, y, yaw, cargo=True):
    """Rowboat riding the swell next to a jetty (cosmetic mover; not rideable)."""
    C.mover(BOAT, x, y, 6.0, yaw=yaw, scale=1.0, bob=6.0, sway=2.5, sway_hz=0.18, sub="Boats")
    C.claim(x, y, 150.0)


def beached_boat(x, y, yaw, lean=10.0, upturned=False, trestles=False, cargo=()):
    gz = C.ground_min(x, y, 120.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    if upturned:
        z = gz + 58.0 - 6.0
        if trestles:
            for off in (-100.0, 100.0):
                inst(P + "Stool", x + off * c, y + off * s, C.ground(x + off * c, y + off * s) - 2.0, yaw, 1.0,
                     collide=True, cull=9000.0)
            z = gz + 59.0 + 58.0 - 8.0
        put(BOAT, x, y, z, yaw=yaw, roll=180.0 + R.uniform(-3, 3), sub="Boats", cull=15000.0)
    else:
        put(BOAT, x, y, gz + 12.0, yaw=yaw, roll=lean, pitch=R.uniform(-4, 4), sub="Boats", cull=15000.0)
        for k, item in enumerate(cargo):
            off = -80.0 + k * 70.0
            inst(item, x + off * c, y + off * s, gz + 5.0, yaw + R.uniform(-20, 20), 0.8, cull=6000.0)
    C.claim(x, y, 185.0)


def rack(x0, y0, x1, y1, height=200.0, hang="fish", every=26.0):
    """Drying rack: posts every ~2.4 m and a crossbar, hung with fish (or sailcloth)."""
    L = math.hypot(x1 - x0, y1 - y0)
    n = max(1, int(round(L / 240.0)))
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    bar_z = []
    for k in range(n + 1):
        px, py = x0 + ux * L * k / n, y0 + uy * L * k / n
        gz = C.ground(px, py)
        post(px, py, gz - 10.0, gz + height + 15.0, 0.8)
        bar_z.append(gz + height)
    zt = max(bar_z)
    for k in range(n):
        ax, ay = x0 + ux * L * k / n, y0 + uy * L * k / n
        bx, by = x0 + ux * L * (k + 1) / n, y0 + uy * L * (k + 1) / n
        za, zb = bar_z[k], bar_z[k + 1]
        beam((ax, ay, za), (bx, by, zb), 0.6)
        yaw = math.degrees(math.atan2(uy, ux))
        if hang == "cloth" and k % 2 == 0:
            mx, my = (ax + bx) / 2, (ay + by) / 2
            inst(P + "Banner_1_Cloth", mx, my, (za + zb) / 2 - 2.0, yaw, (2.6, 1.0, 0.62), cull=9000.0)
        else:
            t = 0.08
            while t < 0.93:
                fx, fy = ax + (bx - ax) * t, ay + (by - ay) * t
                fz = za + (zb - za) * t - 30.0
                inst(R.choice(FISH), fx, fy, fz, yaw + 90.0 + R.uniform(-15, 15), R.uniform(0.9, 1.2),
                     pitch=90.0 + R.uniform(-6, 6), cull=6000.0)
                t += every / (L / n)
    steps = int(L / 60.0)
    for k in range(steps + 1):
        C.claim(x0 + ux * L * k / max(1, steps), y0 + uy * L * k / max(1, steps), 55.0)


def claim_existing():
    """Everything the builder already put in the zone (market, pier, lamps, trees, breakables) counts as taken."""
    n = 0
    for a in C.actors.get_all_level_actors():
        try:
            f = str(a.get_folder_path())
            if f.startswith(("Dress", "Terrain", "Sky", "Sea/Fish", "Audio", "Lighting", "Island")):
                continue
            cls = a.get_class().get_name()
            if cls in ("PointLight", "AmbientSound", "KGFishSchool", "PlayerStart", "KGFoliageField", "KGGrassField"):
                continue
            loc = a.get_actor_location()
            if not (-4800.0 < loc.x < 4800.0 and 3300.0 < loc.y < 12000.0):
                continue
            o, e = a.get_actor_bounds(False)
            r = max(e.x, e.y)
            if r < 8.0 or r > 900.0:
                continue
            C.claim(o.x, o.y, min(r, 350.0) * 0.85)
            n += 1
        except Exception:
            continue
    C.stats["claimed_existing"] = n


# ============================================================================================ vignettes
def pier_gate():
    """Lantern arch over the main pier's landward end with the harbour signboard and pennants."""
    y = 5130.0
    tops = []
    for sx in (-1.0, 1.0):
        x = sx * 195.0
        gz = g(x, y) - 10.0
        post(x, y, gz, 540.0, 1.9, wide=True)
        put(P + "Lantern_Wall", x, y, 330.0, yaw=90.0 * sx, sub="Gate", cull=12000.0)   # arm over the deck
        put(P + "Banner_1_Cloth", x, y - 24.0, 505.0, yaw=0.0, scale=(0.9, 1.0, 0.8), sub="Gate",
            collide=False, cull=12000.0)
        tops.append((x, y))
        C.claim(x, y, 40.0)
    beam((-260.0, y, 505.0), (260.0, y, 505.0), 1.6, path=POSTW)
    # Signboard hanging under the beam (a plank board on two short ropes), readable from land and sea.
    put(TILE, 0.0, y, 452.0, yaw=0.0, roll=90.0, scale=(1.35, 0.3, 1.0), sub="Gate", collide=False, cull=15000.0)
    for side in (-1.0, 1.0):   # a big carved fish nailed to both faces of the board
        put(WP + "SM_KG_Fish_Salmon", 0.0, y + side * 5.0, 452.0, yaw=0.0, scale=2.3, sub="Gate", collide=False,
            cull=15000.0)
    for sx in (-60.0, 60.0):
        put(P + "Banner_2_Cloth", sx, y + 6.0, 432.0, yaw=0.0, scale=(0.55, 1.0, 0.35), sub="Gate", collide=False,
            cull=12000.0)
    C.light(0.0, y, 330.0, 10.0, 1000.0, WARM)
    # Mooring bollards + rope coils at the foot of the gate.
    for sx in (-1.0, 1.0):
        x = sx * 300.0
        gz = g(x, 5250.0)
        post(x, 5250.0, gz - 30.0, gz + 70.0, 1.6, wide=True)
        inst(P + "Rope_2", x + sx * 60.0, 5230.0, g(x + sx * 60.0, 5230.0) + 1.0, R.uniform(0, 360), 0.9,
             cull=6000.0)
        C.claim(x, 5250.0, 60.0)


def pier_extras():
    """Pilings and moored boats along the main pier, a fisherman's stool at the far end, deck-edge clutter."""
    for k, (sx, y, top) in enumerate([(1, 5750, 160), (-1, 6250, 170), (1, 6650, 150), (-1, 7150, 175),
                                      (1, 7650, 160), (-1, 8150, 150), (1, 8600, 170), (-1, 9000, 180)]):
        x = sx * 185.0
        post(x, y, g(x, y) - 20.0, top, 1.5, wide=True)
        if k % 3 == 0:   # a twin piling (dolphin)
            post(x + sx * 40.0, y + 25.0, g(x, y) - 20.0, top - 25.0, 1.3, wide=True)
    moored_boat(-340.0, 6150.0, 93.0)
    moored_boat(345.0, 6950.0, 86.0)
    rope((-200.0, 6150.0, 150.0), (-260.0, 6060.0, 60.0))
    rope((200.0, 6950.0, 150.0), (265.0, 7040.0, 60.0))
    # Deck-edge clutter (|x| >= 80: the centre stays walkable).
    for (x, y, path, yaw, sc) in [(-80, 5480, P + "Rope_2", 20, 0.65), (85, 6330, P + "Bucket_Wooden_1", 0, 1.0),
                                  (-82, 7050, P + "Rope_1", 70, 0.9), (80, 7400, P + "Rope_3", 10, 0.55),
                                  (-82, 8420, P + "Bucket_Metal", 40, 0.9), (80, 8760, P + "Rope_2", 0, 0.65)]:
        inst(path, x, y, PIER_DECK + 1.0, yaw, sc, cull=6000.0)
    fish_crate(88.0, 6250.0, PIER_DECK, 90.0, 4)
    fish_crate(-88.0, 7950.0, PIER_DECK, 90.0, 3)
    for (x, y) in ((85.0, 6330.0), (-82.0, 8420.0)):
        inst(R.choice(FISH), x + R.uniform(-8, 8), y + R.uniform(-8, 8), PIER_DECK + 26.0, R.uniform(0, 360), 1.0,
             roll=90.0, cull=6000.0)
    # End of the pier: stool, rod over the water, bait bucket, lantern on the flag pole side.
    seat(P + "Stool", -60.0, 8930.0, 90.0, z=PIER_DECK, height=58.2)
    inst(WP + "SM_KG_FishingRod", -40.0, 8960.0, PIER_DECK + 40.0, 80.0, 1.0, pitch=28.0, cull=6000.0)
    inst(P + "Bucket_Wooden_1", -100.0, 8880.0, PIER_DECK + 1.0, 0.0, 1.0, cull=6000.0)
    inst(WP + "SM_KG_Fish_Mackerel", -100.0, 8880.0, PIER_DECK + 26.0, 30.0, 1.0, roll=90.0, cull=6000.0)


def market_extras():
    """Fish on every market table, crates of the day's catch, baskets and buckets between the stalls."""
    for (x, y, table_yaw) in [(-50.0, 4450.0, 180.0), (850.0, 4450.0, 0.0), (400.0, 4850.0, 270.0)]:
        top = g(x, y) + 81.0
        c, s = math.cos(math.radians(table_yaw)), math.sin(math.radians(table_yaw))
        for off in (-120.0, -85.0, -50.0, 50.0, 85.0, 120.0):   # the swordfish lies in the middle
            fx, fy = x + off * c, y + off * s
            inst(R.choice(FISH), fx + R.uniform(-6, 6) * s, fy + R.uniform(-6, 6) * c, top + 6.0,
                 table_yaw + 90.0 + R.uniform(-15, 15), R.uniform(0.95, 1.15), roll=90.0, cull=6000.0)
        for off in (-95.0, 95.0):
            inst(K + "Plate", x + off * c - 30.0 * s, y + off * s + 30.0 * c, top + 1.0, 0.0, 1.2, cull=5000.0)
    # Crates of fish and baskets beside the stalls (kept off the street).
    for (x, y, yaw) in [(-230.0, 4330.0, 10.0), (-235.0, 4560.0, 80.0), (1040.0, 4330.0, 100.0),
                        (1050.0, 4560.0, 5.0), (600.0, 4980.0, 90.0)]:
        if C.free(x, y, 45.0, lane_gap=10.0):
            gz = g(x, y)
            fish_crate(x, y, gz, yaw, 4)
            fish_crate(x + 12.0, y + 6.0, gz + 24.0, yaw + 8.0, 3)
            C.claim(x, y, 50.0)
        else:
            warn(f"market crate {x:.0f},{y:.0f} not free")
    for (x, y) in [(-200.0, 4650.0), (1010.0, 4700.0), (250.0, 4990.0)]:
        if C.free(x, y, 30.0, lane_gap=10.0):
            gz = g(x, y)
            inst(P + "Bucket_Wooden_1", x, y, gz, R.uniform(0, 360), 1.1, cull=6000.0)
            inst(WP + "SM_KG_Fish_Mackerel", x, y, gz + 26.0, R.uniform(0, 360), 1.0, roll=90.0, cull=6000.0)
            C.claim(x, y, 30.0)


def mend_nets_corner():
    """Around the MendNets chore (800, 4700): a mender's bench, rope and net coils, a rod, a basket."""
    seat(P + "Bench", 1080.0, 4690.0, 180.0, height=49.6)
    for (x, y, path, sc) in [(1020.0, 4820.0, P + "Rope_3", 1.0), (1000.0, 4560.0, P + "Rope_2", 0.9),
                             (1090.0, 4820.0, P + "Chain_Coil", 0.7), (560.0, 4790.0, P + "Rope_1", 1.0)]:
        inst(path, x, y, g(x, y) + 1.0, R.uniform(0, 360), sc, cull=6000.0)
    for k in range(3):   # a heap of coiled nets (rope coils stacked)
        inst(P + "Rope_3", 1150.0, 4580.0, g(1150.0, 4580.0) + 1.0 + k * 9.0, R.uniform(0, 360), 1.0 - k * 0.15,
             cull=6000.0)
    inst(WP + "SM_KG_FishingRod", 1160.0, 4770.0, g(1160.0, 4770.0) + 5.0, 200.0, 1.0, pitch=62.0, cull=6000.0)
    C.claim(1120.0, 4700.0, 110.0)


def harbourmaster_office():
    """4x4 m stilt office at the pier root: door to the land (north), windows on the sea and the pier."""
    x, y, yaw = 1000.0, 5260.0, 0.0
    f = C.Frame(x, y, yaw, 400.0, 400.0, "harbourmaster")
    fz = max(g(*f.world(sx * 210.0, sy * 210.0)) for sx in (-1, 0, 1) for sy in (-1, 0, 1)) + 4.0

    def wput(path, lx, ly, lyaw, z=fz, scale=1.0, collide=True, cull=0.0):
        wx, wy = f.world(lx, ly)
        return put(path, wx, wy, z, yaw=yaw + lyaw, scale=scale, sub="Harbourmaster", collide=collide, cull=cull)

    for i in (-100.0, 100.0):
        for j in (-100.0, 100.0):
            wx, wy = f.world(i, j)
            inst(TILE, wx, wy, fz, yaw, 1.0, collide=True, cull=0.0)
    front = [V + "Wall_Plaster_Door_Flat", V + "Wall_Plaster_Window_Wide_Flat"]
    back = [V + "Wall_Plaster_Window_Wide_Flat", V + "Wall_Plaster_Straight"]
    left = [V + "Wall_Plaster_Window_Thin_Round", V + "Wall_Plaster_Straight"]
    right = [V + "Wall_Plaster_Straight", V + "Wall_Plaster_Window_Thin_Round"]
    for k in range(2):
        off = -100.0 + k * 200.0
        wput(front[k], off, -200.0, 0.0)
        wput(back[k], -off, 200.0, 180.0)
        wput(left[k], -200.0, -off, -90.0)
        wput(right[k], 200.0, off, 90.0)
    # Corner posts run down to the sand (stilts); a board skirt closes the gap under the floor.
    for sx in (-1, 1):
        for sy in (-1, 1):
            wx, wy = f.world(sx * 200.0, sy * 200.0)
            post(wx, wy, g(wx, wy) - 15.0, fz + 300.0, 1.2)
    for (lx, ly) in ((0.0, 200.0), (200.0, 0.0), (-200.0, 0.0), (0.0, -200.0)):
        wx, wy = f.world(lx, ly)
        post(wx, wy, g(wx, wy) - 15.0, fz, 1.2)
    for side, (lx, ly, lyaw) in enumerate([(-100, 212, 0), (100, 212, 0), (-212, -100, 90), (-212, 100, 90),
                                           (212, -100, 90), (212, 100, 90)]):
        wx, wy = f.world(lx, ly)
        low = min(g(wx + dx, wy + dy) for dx in (-100, 0, 100) for dy in (-100, 0, 100) if True)
        if fz - low > 12.0:
            inst(V + "Prop_WoodenFence_Single", wx, wy, fz - 84.0, yaw + lyaw, 1.0, collide=True, cull=12000.0)
    wput(V + "Roof_RoundTiles_4x4", 0.0, 0.0, 0.0, z=fz + 300.0)
    wput(V + "Roof_Front_Brick4", 0.0, -200.0, 0.0, z=fz + 300.0)
    wput(V + "Roof_Front_Brick4", 0.0, 200.0, 180.0, z=fz + 300.0)
    wput(V + "Prop_Chimney2", 110.0, 80.0, 0.0, z=fz + 360.0)
    # Door side: lantern, doorbell, a step, oars leaning on the wall, a harbour banner on the pier side.
    wput(P + "Lantern_Wall", 40.0, -231.0, 180.0, z=fz + 150.0, cull=12000.0)
    wput(WP + "SM_KG_Doorbell", -210.0, -231.0, -90.0, z=fz + 215.0, cull=6000.0)
    for k, lx in enumerate((130.0, 165.0)):   # handle on the sand, blade resting against the wall
        wx, wy = f.world(lx + k * 6.0, -300.0)
        inst(WP + "SM_KG_Oar", wx, wy, g(wx, wy) + 2.0, yaw + 90.0 + k * 5.0, 1.0, pitch=72.0, cull=6000.0)
    wx, wy = f.world(-232.0, 60.0)
    put(P + "Banner_1", wx, wy, fz + 270.0, yaw=yaw + 180.0, sub="Harbourmaster", collide=False, cull=12000.0)
    # Inside: desk + chair, ledgers, lamp, the strongbox (loot), a shelf of charts.
    wput(K + "Square_Table", 90.0, 110.0, 0.0, cull=9000.0)
    for (path, lx, ly, dz, lyaw) in [(P + "Book_Stack_1", 70.0, 120.0, 67.0, 20.0), (P + "Scroll_1", 110.0, 95.0, 67.0, 60.0),
                                     (P + "CandleStick", 115.0, 135.0, 67.0, 0.0), (P + "Coin_Pile", 60.0, 90.0, 67.0, 0.0),
                                     (K + "Bottle", 125.0, 80.0, 67.0, 0.0), (P + "Scroll_2", 80.0, 100.0, 69.0, 110.0)]:
        wput(path, lx, ly, lyaw, z=fz + dz, collide=False, cull=5000.0)
    wx, wy = f.world(90.0, 40.0)
    seat(P + "Chair_1", wx, wy, yaw + 90.0, z=fz, height=49.8, stand=-70.0)
    wput(P + "Shelf_Small_Bottles", -120.0, 175.0, 180.0, z=fz + 120.0, collide=False, cull=6000.0)
    wx, wy = f.world(-140.0, 120.0)
    C.loot_chest(wx, wy, fz, yaw=yaw + 90.0, table="Chest", name="Harbourmaster's Strongbox")
    C.light(x, y - 120.0, fz + 230.0, 8.0, 800.0, WARM)
    C.claim(x, y, 300.0)
    # Tally desk outside on the pier side: the harbourmaster counts every crate that lands.
    tx, ty = 560.0, 5300.0
    gz = g(tx, ty)
    put(K + "Square_Table", tx, ty, gz, yaw=8.0, sub="Harbourmaster", cull=9000.0)
    for (path, dx, dy, dz, yw) in [(P + "Book_5", -10, 10, 67, 30), (P + "Coin_Pile_2", 20, -15, 67, 0),
                                   (P + "Scroll_1", -20, -20, 67, 80), (K + "Cup", 25, 20, 67, 0)]:
        inst(path, tx + dx, ty + dy, gz + dz, yw, 1.0, cull=5000.0)
    seat(K + "Chair", tx + 70.0, ty, 180.0, height=44.2, stand=-70.0)
    fish_crate(tx - 20.0, ty + 110.0, g(tx - 20.0, ty + 110.0), 20.0, 4)
    fish_crate(tx + 70.0, ty + 115.0, g(tx + 70.0, ty + 115.0), 95.0, 3)
    C.claim(tx, ty + 40.0, 110.0)


def signal_cannon():
    """Old harbour salute gun on the beach west of the pier root, pointing out to sea, with a ball pile."""
    x, y = -480.0, 5360.0
    gz = C.ground_min(x, y, 120.0)
    put(PIR + "cannon_0_001", x, y, gz - 4.0, yaw=90.0, sub="Quay", cull=15000.0)
    inst(PIR + "Cannon_Ball_0", x - 170.0, y - 40.0, g(x - 170.0, y - 40.0) - 4.0, 30.0, 0.7, collide=True,
         cull=9000.0)
    C.claim(x, y, 170.0)


def chowder_stand():
    """Seaside chowder stand: a stall, a simmering cauldron, tables and stools looking out over the bay."""
    sx, sy = 1850.0, 4540.0
    put(P + "Stall_Empty", sx, sy, g(sx, sy), yaw=180.0, sub="Chowder", cull=15000.0)
    put(P + "Table_Large", sx, sy - 5.0, g(sx, sy), yaw=0.0, scale=(0.62, 0.8, 1.0), sub="Chowder", cull=12000.0)
    top = g(sx, sy) + 81.0
    for (path, dx, dz) in [(K + "SmallPot", -60, 0), (K + "Bowl", -20, 0), (K + "Bowl", 5, 0), (K + "Laddle", 30, 3),
                           (K + "Bottle", 60, 0), (K + "YellowBottle", 72, 0), (IP + "Bread", 40, 0)]:
        inst(path, sx + dx, sy + R.uniform(-15, 15), top + dz, R.uniform(0, 360), 1.0, cull=5000.0)
    C.claim(sx, sy, 130.0)
    # Cauldron over a fire.
    cx, cy = 1600.0, 4640.0
    gz = g(cx, cy)
    logs_fire(cx, cy, gz, 5, 1.0, light=9.0)
    put(P + "Cauldron", cx, cy, gz + 22.0, yaw=0.0, sub="Chowder", cull=12000.0)
    C.ambient("A_Fire_Loop", cx, cy, gz + 80.0, 0.5)
    C.claim(cx, cy, 110.0)
    inst(P + "Barrel_Apples", 1700.0, 4470.0, g(1700.0, 4470.0), 0.0, 1.0, collide=True, cull=9000.0)
    inst(P + "FarmCrate_Carrot", 2010.0, 4480.0, g(2010.0, 4480.0), 20.0, 1.0, cull=7000.0)
    C.claim(1700.0, 4470.0, 45.0)
    # Tables with stools (seats), each with a meal.
    for (tx, ty, ty_yaw) in [(1580.0, 4880.0, 10.0), (1900.0, 4830.0, -15.0), (1400.0, 4700.0, 30.0)]:
        if C.lane_distance(tx, ty) < 120.0:
            warn(f"chowder table {tx:.0f},{ty:.0f} on a lane")
            continue
        gz = g(tx, ty)
        put(K + "Square_Table", tx, ty, gz, yaw=ty_yaw, sub="Chowder", cull=9000.0)
        c, s = math.cos(math.radians(ty_yaw)), math.sin(math.radians(ty_yaw))
        for side in (-1.0, 1.0):
            px, py = tx + side * 75.0 * c, ty + side * 75.0 * s
            seat(K + "Stool", px, py, ty_yaw + (0.0 if side < 0 else 180.0), height=67.9, stand=-70.0)
        for (path, dx, dy) in [(K + "Bowl", -15, 10), (K + "Bowl", 18, -12), (P + "Mug", 5, 25), (K + "Spoon", -25, -5),
                               (IP + "Bread", 20, 15)]:
            inst(path, tx + dx, ty + dy, gz + 67.0, R.uniform(0, 360), 1.0, cull=5000.0)
        C.claim(tx, ty, 120.0)
    lantern_post(2000.0, 4630.0, 30.0)


def jetty():
    """The fishermen's plank jetty: stem from the beach to a T-head, pilings, stringers, boats, cargo."""
    # Deck tiles (the first ones sink into the sand so the walk-on is flush).
    y = J_Y0 + 100.0
    while y < J_Y1:
        inst(TILE, JX, y, J_DECK - 1.0, 90.0, (1.0, 1.5, 1.0), collide=True, cull=0.0)
        y += 200.0
    for x in range(int(T_X0) + 100, int(T_X1), 200):
        for yy in (J_Y1 + 100.0, J_Y1 + 300.0):
            inst(TILE, float(x), yy, J_DECK - 1.0, 0.0, 1.0, collide=True, cull=0.0)
    # Pilings (skip where the beach is higher than the deck) + stringers and edge boards.
    y = J_Y0 + 200.0
    while y <= J_Y1:
        for sx in (-1.0, 1.0):
            px = JX + sx * 160.0
            if g(px, y) < J_DECK - 25.0:
                post(px, y, g(px, y) - 20.0, J_DECK + 45.0, 1.35, wide=True)
        y += 400.0
    for x in range(int(T_X0), int(T_X1) + 1, 250):
        for yy in (J_Y1 + 10.0, T_Y1 + 10.0):
            if abs(x - JX) < 180.0 and yy < T_Y1:
                continue
            post(float(x), yy, g(float(x), yy) - 20.0, J_DECK + 45.0, 1.35, wide=True)
    for sx in (-1.0, 1.0):
        beam((JX + sx * 150.0, J_Y0 + 60.0, J_DECK - 16.0), (JX + sx * 150.0, J_Y1, J_DECK - 16.0), 0.9)
    beam((T_X0, J_Y1 + 5.0, J_DECK - 16.0), (T_X1, J_Y1 + 5.0, J_DECK - 16.0), 0.9)
    beam((T_X0, T_Y1 - 5.0, J_DECK - 16.0), (T_X1, T_Y1 - 5.0, J_DECK - 16.0), 0.9)
    beam((T_X0 + 5.0, J_Y1, J_DECK - 16.0), (T_X0 + 5.0, T_Y1, J_DECK - 16.0), 0.9)
    beam((T_X1 - 5.0, J_Y1, J_DECK - 16.0), (T_X1 - 5.0, T_Y1, J_DECK - 16.0), 0.9)
    # Claim the deck (props on it are placed by hand below).
    y = J_Y0
    while y <= T_Y1:
        C.claim(JX, y, 170.0)
        y += 150.0
    for x in range(int(T_X0), int(T_X1) + 1, 150):
        C.claim(float(x), J_Y1 + 200.0, 170.0)
    # Lantern posts on the T-head corners.
    lantern_post(T_X0 + 40.0, T_Y1 - 40.0, 225.0, light=11.0, radius=1100.0)
    lantern_post(T_X1 - 40.0, T_Y1 - 40.0, 135.0, light=11.0, radius=1100.0)
    lantern_post(JX + 130.0, 5950.0, 90.0)
    # Moored boats (both sides of the stem and off the T-head) with mooring lines.
    for (bx, by, yaw, px, py) in [(JX - 245.0, 5900.0, 88.0, JX - 160.0, 6000.0),
                                  (JX + 250.0, 6450.0, 94.0, JX + 160.0, 6400.0),
                                  (JX - 250.0, 6600.0, 91.0, JX - 160.0, 6800.0),
                                  (1950.0, 7470.0, 2.0, 2000.0, T_Y1 + 10.0),
                                  (2480.0, 7480.0, 176.0, 2450.0, T_Y1 + 10.0)]:
        moored_boat(bx, by, yaw)
        rope((px, py, J_DECK + 30.0), (bx + (px - bx) * 0.45, by + (py - by) * 0.45, 45.0))
    # Floating barrel buoys off the T-head.
    float_barrel(1500.0, 7250.0)
    float_barrel(2950.0, 7150.0)
    # Deck cargo along the edges: fish crates, barrels, traps, rope, a fisherman's corner.
    dz = J_DECK + 1.0
    for (x, y, yaw, n) in [(JX + 110.0, 5500.0, 90.0, 4), (JX + 110.0, 5580.0, 95.0, 3), (JX - 110.0, 6250.0, 88.0, 4),
                           (2600.0, 7220.0, 0.0, 4)]:
        fish_crate(x, y, dz - 1.0, yaw, n)
    for (x, y, path, sc) in [(JX - 115.0, 5420.0, P + "Rope_2", 0.8), (JX + 115.0, 6120.0, P + "Rope_1", 1.0),
                             (JX - 118.0, 6750.0, P + "Chain_Coil", 0.6), (1800.0, T_Y1 - 45.0, P + "Rope_3", 0.8),
                             (2350.0, T_Y1 - 50.0, P + "Rope_2", 0.8)]:
        inst(path, x, y, dz, R.uniform(0, 360), sc, cull=6000.0)
    for (x, y) in [(JX + 105.0, 6600.0), (JX - 105.0, 5700.0)]:
        inst(PIR + "Barrel_%d" % R.choice([3, 4, 10]), x, y, dz - 1.0, R.uniform(0, 360), 0.6, collide=True,
             cull=9000.0)
    for (x, y, lvl) in [(1780.0, 7010.0, 0), (1860.0, 7010.0, 0), (1820.0, 7015.0, 1)]:
        inst(CAGE, x, y, dz + 3.0 + lvl * 80.0, R.uniform(-8, 8), 0.95, collide=True, cull=9000.0)
    breakable(P + "Crate_Wooden", 2620.0, 7020.0, dz + 5.0)
    breakable(PIR + "crates_0", 2540.0, 7010.0, dz + 5.0)
    # Fisherman on the T-head: stool, rod out over the water, bait bucket, catch.
    seat(P + "Stool", 2150.0, T_Y1 - 55.0, 90.0, z=dz, height=58.2)
    inst(WP + "SM_KG_FishingRod", 2170.0, T_Y1 - 25.0, dz + 40.0, 90.0, 1.0, pitch=25.0, cull=6000.0)
    inst(P + "Bucket_Wooden_1", 2090.0, T_Y1 - 60.0, dz, 0.0, 1.0, cull=6000.0)
    inst(WP + "SM_KG_Fish_Cod", 2090.0, T_Y1 - 60.0, dz + 26.0, 40.0, 1.0, roll=90.0, cull=6000.0)
    seat(P + "Stool", 2450.0, J_Y1 + 60.0, 270.0, z=dz, height=58.2)
    inst(WP + "SM_KG_FishingRod", 2470.0, J_Y1 + 30.0, dz + 40.0, 270.0, 1.0, pitch=22.0, cull=6000.0)
    C.ambient("A_Lapping_Loop", JX, 6800.0, 80.0, 0.8)


def cargo_hoist():
    """Mast-and-boom hoist at the jetty root: a crate hangs over the shallows above a boat being unloaded."""
    mx, my = 2540.0, 5060.0
    gz = g(mx, my) - 15.0
    top = gz + 590.0
    post(mx, my, gz, top, 2.0, wide=True)
    tip = (2420.0, 5560.0, top + 60.0)
    base = (mx - 5.0, my + 10.0, top - 110.0)
    beam(base, tip, 1.4, path=POSTW)
    mid = ((base[0] + tip[0]) / 2, (base[1] + tip[1]) / 2, (base[2] + tip[2]) / 2)
    beam((mx, my + 10.0, gz + 260.0), mid, 0.9)                       # brace
    rope((mx, my, top - 10.0), tip, 0.16)                             # top stay
    crate_z = 170.0
    put(P + "Crate_Wooden", tip[0], tip[1], crate_z, yaw=18.0, roll=4.0, sub="Hoist", cull=12000.0)
    rope((tip[0], tip[1], crate_z + 88.0), tip, 0.18)
    for dx in (-30.0, 30.0):
        rope((tip[0] + dx, tip[1], crate_z + 88.0), (tip[0], tip[1], crate_z + 150.0), 0.1)
    moored_boat(2560.0, 5700.0, 100.0)
    # Winch at the mast foot: a barrel drum on two posts, chain and rope.
    for dy in (-45.0, 45.0):
        post(mx + 70.0, my + dy, g(mx + 70.0, my + dy) - 10.0, g(mx + 70.0, my) + 70.0, 0.9)
    put(PIR + "Barrel_3", mx + 70.0, my - 40.0, g(mx + 70.0, my) + 40.0, yaw=90.0, roll=90.0, scale=(0.55, 0.55, 0.8),
        sub="Hoist", cull=12000.0)
    inst(P + "Chain_Coil", mx + 150.0, my + 60.0, g(mx + 150.0, my + 60.0) + 1.0, 20.0, 0.8, cull=6000.0)
    C.claim(mx, my, 150.0)
    # Cargo waiting on the sand: crate stacks, a loaded wagon, barrels; some can be smashed.
    crate_stack(2750.0, 4900.0, 15.0, [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0.5, 0.5, 1)])
    crate_stack(2470.0, 4780.0, -10.0, [(0, 0, 0), (1, 0, 0), (0.5, 0, 1)])
    barrel_group(2860.0, 5130.0, 3)
    # Fish cart backed down the beach beside the jetty root (the wagon's bed centre is 115 cm behind its pivot).
    bx, by = 1930.0, 5250.0
    put(V + "Prop_Wagon", bx, by + 115.0, g(bx, by) - 2.0, yaw=0.0, sub="Hoist", cull=15000.0)
    for (dy, path, sc) in [(-80.0, P + "Crate_Wooden", 0.75), (30.0, PIR + "crates_1", 0.8)]:
        inst(path, bx + R.uniform(-10, 10), by + dy, g(bx, by) + 72.0, R.uniform(-8, 8), sc, collide=True,
             cull=12000.0)
    fish_crate(bx, by + 110.0, g(bx, by) + 72.0, 0.0, 4)
    C.claim(bx, by - 90.0, 100.0)
    C.claim(bx, by + 90.0, 100.0)
    for (x, y) in [(2660.0, 5230.0), (2980.0, 4980.0), (2400.0, 5150.0)]:
        if C.free(x, y, 45.0):
            breakable(R.choice([P + "Crate_Wooden", PIR + "crates_0", P + "Barrel"]), x, y)


def lobster_yard():
    """Trap stacks, buoy heaps and a repair bench on the east beach; a lobster boat pulled up on the sand."""
    for (x, y, yaw, layout) in [(3250.0, 5000.0, 12.0, [(0, 0, 0), (1, 0, 0), (0, 0, 1), (1, 0, 1), (0.5, 0, 2)]),
                                (3420.0, 5180.0, -20.0, [(0, 0, 0), (0, 0, 1), (0, 0, 2)]),
                                (3180.0, 5250.0, 40.0, [(0, 0, 0), (1, 0, 0), (2, 0, 0), (0.5, 0, 1), (1.5, 0, 1)]),
                                (3650.0, 5050.0, 70.0, [(0, 0, 0), (1, 0, 0)])]:
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        gz = C.ground_min(x, y, 100.0)
        for (dx, dy, lvl) in layout:
            lx, ly = dx * 86.0, dy * 90.0
            inst(CAGE, x + lx * c - ly * s, y + lx * s + ly * c, gz + 3.0 + lvl * 80.0, yaw + R.uniform(-7, 7),
                 0.95, collide=True, cull=9000.0)
        C.claim(x, y, 150.0)
    # Heap of spare floats and a coil of line.
    for k in range(7):
        bx, by = 3560.0 + R.uniform(-45, 45), 5260.0 + R.uniform(-45, 45)
        inst(BOBBER, bx, by, g(bx, by) + 18.0 + (k // 4) * 18.0, R.uniform(0, 360), 4.5, pitch=R.uniform(70, 110),
             cull=6000.0)
    inst(P + "Rope_3", 3620.0, 5320.0, g(3620.0, 5320.0) + 1.0, 30.0, 1.0, cull=6000.0)
    C.claim(3570.0, 5270.0, 90.0)
    # Repair bench.
    bx, by = 3480.0, 4830.0
    put(P + "Workbench", bx, by, g(bx, by), yaw=5.0, sub="Lobster", cull=12000.0)
    inst(CAGE, bx - 40.0, by, g(bx, by) + 89.0, 15.0, 0.7, collide=False, cull=7000.0)
    inst(P + "Rope_1", bx + 60.0, by + 5.0, g(bx, by) + 89.0, 0.0, 0.6, cull=5000.0)
    seat(P + "Stool", bx, by + 95.0, 270.0, height=58.2)
    inst(P + "Bucket_Metal", bx + 140.0, by + 20.0, g(bx + 140.0, by + 20.0), 0.0, 1.0, cull=6000.0)
    C.claim(bx, by, 140.0)
    lantern_post(3330.0, 4760.0, 0.0, light=9.0, radius=900.0)
    beached_boat(3520.0, 5480.0, 75.0, lean=12.0, cargo=(CAGE, CAGE))
    beached_boat(2980.0, 5300.0, 100.0, upturned=True, trestles=True)
    for (x, y) in [(3000.0, 5080.0), (3750.0, 5250.0)]:
        if C.free(x, y, 45.0):
            breakable(R.choice([P + "Crate_Wooden", P + "Barrel"]), x, y)


def breakwater():
    """A rock arm curling out from the east beach into the sea, a beacon at its end."""
    pts = [(4350.0, 5500.0), (4300.0, 5850.0), (4220.0, 6200.0), (4120.0, 6550.0), (3990.0, 6880.0), (3840.0, 7150.0)]
    rocks = [N + "Rock_Medium_1", N + "Rock_Medium_2", N + "Rock_Medium_3"]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        for t in (0.0, 0.5):
            x, y = ax + (bx - ax) * t + R.uniform(-60, 60), ay + (by - ay) * t + R.uniform(-60, 60)
            sea = g(x, y)
            want_top = R.uniform(70.0, 160.0)
            sc = max(0.8, (want_top - sea + 40.0) / 210.0)
            inst(R.choice(rocks), x, y, sea - 25.0 * sc, R.uniform(0, 360), (sc * R.uniform(0.9, 1.1), sc, sc),
                 pitch=R.uniform(-8, 8), roll=R.uniform(-8, 8), collide=True, cull=0.0)
    ex, ey = pts[-1]
    lantern_post(ex - 40.0, ey + 60.0, 200.0, light=10.0, radius=1200.0, tall=1.0)
    for k in range(4):
        inst(N + "Rock_Medium_%d" % (k % 3 + 1), 4400.0 - k * 120.0 + R.uniform(-40, 40), 5380.0 + k * 40.0,
             g(4400.0 - k * 120.0, 5380.0 + k * 40.0) - 30.0, R.uniform(0, 360), 0.45, collide=True, cull=0.0)


def west_boatyard():
    """Slipway from the boathouse down into the water with a boat on it; an upturned hull being tarred."""
    sx = -1250.0
    y = 5170.0
    while y < 5990.0:
        z0, z1 = g(sx, y - 100.0), g(sx, y + 100.0)
        zc = (z0 + z1) / 2.0 + 3.0
        drop = math.degrees(math.atan2(z0 - z1, 200.0))
        inst(TILE, sx, y, zc, 0.0, (1.1, 1.0, 1.0), roll=drop, collide=True, cull=0.0)
        for dx in (-70.0, 70.0):   # launch rails
            beam((sx + dx, y - 100.0, z0 + 8.0), (sx + dx, y + 100.0, z1 + 8.0), 0.6)
        y += 200.0
    C.claim(sx, 5300.0, 140.0)
    C.claim(sx, 5500.0, 140.0)
    by = 5470.0
    slope_deg = math.degrees(math.atan2(g(sx, by - 150.0) - g(sx, by + 150.0), 300.0))
    put(BOAT, sx, by, g(sx, by) + 40.0, yaw=90.0, pitch=-slope_deg, sub="Boatyard", cull=15000.0)
    rope((sx, by - 160.0, g(sx, by - 160.0) + 60.0), (sx - 20.0, 5160.0, g(sx, 5160.0) + 60.0), 0.15)
    # Winch at the head of the slip.
    for dx in (-60.0, 60.0):
        post(sx + dx, 5150.0, g(sx + dx, 5150.0) - 10.0, g(sx, 5150.0) + 75.0, 0.9)
    put(PIR + "Barrel_4", sx - 50.0, 5150.0, g(sx, 5150.0) + 42.0, yaw=0.0, roll=90.0, pitch=0.0, scale=(0.55, 0.55, 1.0),
        sub="Boatyard", cull=12000.0)
    C.claim(sx, 5150.0, 70.0)
    # Upturned hull on trestles, tar pot on embers, bucket, planks.
    beached_boat(-1900.0, 5150.0, 10.0, upturned=True, trestles=True)
    tx, ty = -1850.0, 4930.0
    logs_fire(tx, ty, None, 4, 0.7)
    put(P + "Pot_1_Lid", tx, ty, g(tx, ty) + 26.0, yaw=30.0, scale=1.3, sub="Boatyard", cull=9000.0)
    inst(P + "Bucket_Wooden_1", tx + 70.0, ty - 30.0, g(tx + 70.0, ty - 30.0), 0.0, 1.0, cull=6000.0)
    for k in range(3):
        inst(PIR + "Planks_%d" % (k % 4), -2100.0, 4980.0 + k * 3.0, g(-2100.0, 4980.0) + k * 9.0, 80.0 + k * 4.0, 0.9,
             cull=7000.0)
    C.claim(tx, ty, 80.0)
    C.claim(-2100.0, 4980.0, 110.0)
    for (x, y) in [(-1650.0, 5230.0), (-1720.0, 5300.0)]:
        if C.free(x, y, 45.0):
            breakable(P + "Barrel" if x < -1700 else P + "Crate_Wooden", x, y)
    # Oars and rope against the boathouse's seaward wall.
    for k in range(3):
        inst(WP + "SM_KG_Oar", -1500.0 + k * 45.0, 5135.0, g(-1500.0 + k * 45.0, 5135.0) + 2.0, 270.0 + R.uniform(-5, 5),
             1.0, pitch=70.0 + k * 3.0, cull=6000.0)
    inst(P + "Rope_2", -950.0, 5160.0, g(-950.0, 5160.0) + 1.0, 0.0, 1.0, cull=6000.0)


def smokehouse():
    """Open smokehouse on the west slope: brick back wall, fish racks over a fire, salt barrels, a chest."""
    x, y, yaw = -2900.0, 4600.0, 90.0
    f = C.Frame(x, y, yaw, 400.0, 600.0, "smokehouse")
    corners = [f.world(sx * 190.0, sy * 290.0) for sx in (-1, 1) for sy in (-1, 1)]
    roof_z = max(g(cx, cy) for cx, cy in corners) + 300.0
    for (cx, cy) in corners:
        post(cx, cy, g(cx, cy) - 15.0, roof_z + 5.0, 1.3)
    wx, wy = f.world(0.0, 0.0)
    put(V + "Roof_RoundTiles_4x6", wx, wy, roof_z, yaw=yaw, sub="Smokehouse")
    for k, ly in enumerate((-200.0, 0.0, 200.0)):
        bx, by = f.world(-200.0, ly)
        base = C.ground_min(bx, by, 100.0) - 5.0
        put(V + ("Wall_UnevenBrick_Straight" if k != 1 else "Wall_UnevenBrick_Window_Thin_Round"), bx, by, base,
            yaw=yaw - 90.0, scale=(1.0, 1.0, (roof_z + 20.0 - base) / 312.0), sub="Smokehouse")
    # Two racks of fish hanging over the smoke.
    for lx in (-20.0, 110.0):
        a, b = f.world(lx, -250.0), f.world(lx, 250.0)
        rack(a[0], a[1], b[0], b[1], height=min(roof_z - g(*a) - 60.0, 230.0), hang="fish", every=22.0)
    fx, fy = f.world(45.0, 0.0)
    gz = g(fx, fy)
    logs_fire(fx, fy, gz, 4, 0.8, light=8.0)
    C.ambient("A_Fire_Loop", fx, fy, gz + 80.0, 0.6)
    # Salt barrels, fish crates, gutting table, the chest (loot) in the back corner.
    for (lx, ly) in [(-150.0, -240.0), (-150.0, -170.0), (-90.0, -250.0)]:
        bx, by = f.world(lx, ly)
        inst(P + "Barrel", bx, by, g(bx, by) - 3.0, R.uniform(0, 360), 0.9, collide=True, cull=9000.0)
    tx, ty = f.world(-130.0, 60.0)
    put(P + "Table_Large", tx, ty, g(tx, ty) - 4.0, yaw=yaw + 90.0, scale=(0.6, 0.7, 1.0), sub="Smokehouse", cull=12000.0)
    fish_crate(tx, ty, g(tx, ty) + 77.0, yaw, 5, crate=False)
    inst(K + "KitchenKnife", tx + 20.0, ty - 10.0, g(tx, ty) + 80.0, 30.0, 1.0, cull=4000.0)
    for (lx, ly, rot) in [(150.0, -220.0, 0.0), (150.0, 220.0, 70.0), (60.0, -220.0, 20.0)]:
        bx, by = f.world(lx, ly)
        fish_crate(bx, by, g(bx, by), yaw + rot, 4)
    cx, cy = f.world(-150.0, 215.0)
    C.loot_chest(cx, cy, g(cx, cy) - 2.0, yaw=0.0, table="Fishing", name="Smokehouse Chest")
    C.claim(x, y, 360.0)
    C.claim(*f.world(0.0, -300.0), 200.0)
    C.claim(*f.world(0.0, 300.0), 200.0)
    # Firewood stack against the outside of the back wall.
    bx, by = f.world(-262.0, -120.0)
    woodpile(bx, by, yaw, rows=3, per=5)
    bx, by = f.world(-262.0, 140.0)
    woodpile(bx, by, yaw, rows=2, per=4)
    for (x2, y2) in [(-2500.0, 4750.0), (-3300.0, 4750.0)]:
        if C.free(x2, y2, 45.0):
            breakable(P + "Barrel", x2, y2)


def drying_racks():
    """Racks of fish and sailcloth drying on the west beach."""
    rack(-3550.0, 4990.0, -2350.0, 5000.0, height=195.0, hang="fish")
    rack(-3400.0, 5200.0, -2600.0, 5215.0, height=190.0, hang="cloth")
    rack(-2150.0, 4820.0, -2150.0, 5250.0, height=185.0, hang="fish")
    # Baskets and crates at the rack feet.
    for (x, y, yaw) in [(-3300.0, 5080.0, 20.0), (-2700.0, 5090.0, 95.0), (-2250.0, 5300.0, 0.0)]:
        if C.free(x, y, 40.0):
            fish_crate(x, y, g(x, y), yaw, 4)
            C.claim(x, y, 45.0)
    for (x, y) in [(-3000.0, 5100.0), (-2400.0, 4900.0)]:
        if C.free(x, y, 30.0):
            inst(P + "Bucket_Wooden_1", x, y, g(x, y), 0.0, 1.0, cull=6000.0)
            C.claim(x, y, 30.0)
    seat(P + "Stool", -2900.0, 5100.0, 270.0, height=58.2)


def west_beach_boats():
    beached_boat(-2380.0, 5470.0, 68.0, lean=11.0, cargo=(WP + "SM_KG_Oar", P + "Rope_1"))
    beached_boat(-3150.0, 5520.0, 104.0, lean=-9.0, cargo=(P + "Bucket_Wooden_1", WP + "SM_KG_Oar"))
    for (x, y) in [(-2650.0, 5400.0), (-2200.0, 5370.0)]:
        post(x, y, g(x, y) - 30.0, g(x, y) + 80.0, 1.5, wide=True)   # mooring stakes
        C.claim(x, y, 40.0)
    rope((-2650.0, 5400.0, g(-2650.0, 5400.0) + 60.0), (-2450.0, 5430.0, g(-2450.0, 5430.0) + 45.0), 0.12)


def smugglers_cove():
    """Far west quiet corner: a campfire, stools, rum barrels, a broken flag, and a pirate chest by the rocks."""
    cx, cy = -3980.0, 5050.0
    gz = g(cx, cy)
    logs_fire(cx, cy, gz, 5, 1.0, light=7.0)
    for k, a in enumerate((200.0, 290.0, 20.0)):
        ra = math.radians(a)
        sx, sy = cx + 150.0 * math.cos(ra), cy + 150.0 * math.sin(ra)
        seat(P + "Stool", sx, sy, a + 180.0, height=58.2)
    for (dx, dy, path) in [(-70.0, -250.0, P + "Bag"), (-10.0, -275.0, P + "Bag"), (60.0, -240.0, PIR + "crates_1")]:
        inst(path, cx + dx, cy + dy, g(cx + dx, cy + dy) - 2.0, R.uniform(0, 360), 0.9, collide=True, cull=9000.0)
    C.claim(cx, cy - 255.0, 90.0)
    breakable(P + "Barrel", -4150.0, 4700.0)
    breakable(P + "Barrel", -3820.0, 4760.0)
    barrel_group(-4250.0, 4850.0, 3)
    inst(K + "Bottle", cx + 110.0, cy + 60.0, g(cx + 110.0, cy + 60.0), 0.0, 1.0, pitch=85.0, cull=4000.0)
    inst(K + "YellowBottle", cx - 90.0, cy + 110.0, g(cx - 90.0, cy + 110.0), 0.0, 1.0, cull=4000.0)
    put(PIR + "Broken_Flag_0", -3750.0, 4870.0, g(-3750.0, 4870.0) - 20.0, yaw=30.0, roll=6.0, sub="Cove", cull=15000.0)
    # The hidden chest between the rocks.
    kx, ky = -4300.0, 5330.0
    for (dx, dy, sc, k) in [(-160.0, 60.0, 0.6, 1), (150.0, 90.0, 0.5, 2), (0.0, 190.0, 0.45, 3)]:
        inst(N + "Rock_Medium_%d" % k, kx + dx, ky + dy, g(kx + dx, ky + dy) - 20.0, R.uniform(0, 360), sc,
             collide=True, cull=0.0)
    C.loot_chest(kx, ky, g(kx, ky) - 6.0, yaw=200.0, table="Chest", name="Smuggler's Cache",
                 path=PIR + "chest_common_0")
    C.claim(kx, ky, 200.0)
    beached_boat(-3700.0, 5460.0, 82.0, lean=14.0, cargo=(PIR + "crates_1",))


def burnt_pier():
    """Charred remains of the old pier sticking out of the bay west of the boathouse."""
    for k, y in enumerate(range(5700, 7900, 380)):
        for sx in (-1.0, 1.0):
            if R.random() < 0.18:
                continue
            x = -2350.0 + sx * 110.0 + R.uniform(-15, 15)
            top = 150.0 - k * 12.0 + R.uniform(-50, 40)
            base = g(x, y) - 20.0
            inst(POSTW, x, y, base, R.uniform(0, 360), (1.5, 1.5, (top - base) / 300.0), pitch=R.uniform(-7, 7),
                 roll=R.uniform(-7, 7), collide=True, cull=0.0)
        if k in (1, 4):
            inst(TILE, -2350.0, float(y), 105.0 - k * 8.0, 0.0, (1.1, 0.9, 1.0), pitch=R.uniform(-12, 12),
                 roll=R.uniform(-8, 8), collide=True, cull=0.0)
    C.mover(P + "Crate_Wooden", -2050.0, 7300.0, -45.0, yaw=35.0, roll=8.0, scale=0.9, bob=9.0, sway=4.0,
            sway_hz=0.2, sub="Buoys")
    float_barrel(-2700.0, 6500.0)


def bay_buoys():
    """Channel markers along the main pier and clusters of lobster-pot floats across the bay."""
    for (x, y) in [(-650.0, 7200.0), (650.0, 7250.0), (-760.0, 8450.0), (760.0, 8400.0), (-850.0, 9700.0),
                   (870.0, 9650.0)]:
        buoy(x, y, big=True)
    for (cx, cy, n) in [(3200.0, 7900.0, 3), (1300.0, 9000.0, 2), (-1500.0, 8300.0, 3), (-3400.0, 7100.0, 2),
                        (2500.0, 9700.0, 2), (-600.0, 10600.0, 2), (-3000.0, 9400.0, 3), (3900.0, 8800.0, 2)]:
        for k in range(n):
            x, y = cx + R.uniform(-160, 160), cy + R.uniform(-160, 160)
            if C.in_zone(x, y):
                buoy(x, y)


def waterline_scatter():
    """Driftwood, seaweed, shells, pebbles, rocks and planks along the waterline on both sides of the pier."""

    def band(lo, hi, xs):
        def pick():
            for _ in range(40):
                x = R.uniform(*R.choice(xs))
                y = R.uniform(4800.0, 5900.0)
                h = g(x, y)
                if lo < h < hi:
                    return x, y
            return 99999.0, 99999.0
        return pick

    sides = [(-4450.0, -700.0), (700.0, 3600.0)]
    C.scatter([N + "Plant_7", N + "Plant_7_Big"], 46, band(-20.0, 55.0, sides), 70.0, sub="Beach",
              scale=(0.6, 1.0), cull=6000.0, sink=10.0)
    C.scatter([N + "Pebble_Round_%d" % k for k in range(1, 6)] + [N + "Pebble_Square_%d" % k for k in range(1, 7)],
              90, band(-10.0, 110.0, sides), 30.0, sub="Beach", scale=(0.35, 0.8), cull=5000.0, sink=2.0)
    n = 0
    pick = band(-60.0, 40.0, sides)
    for _ in range(300):
        if n >= 12:
            break
        x, y = pick()
        if not C.free(x, y, 110.0):
            continue
        n += 1
        C.claim(x, y, 110.0)
        inst(N + "Rock_Medium_%d" % R.choice([1, 2, 3]), x, y, g(x, y) - 12.0, R.uniform(0, 360), R.uniform(0.18, 0.35),
             pitch=R.uniform(-10, 10), collide=True, cull=0.0)
    # Driftwood: small dead trees lying on their side, loose beams, plank piles.
    n = 0
    for _ in range(400):
        if n >= 16:
            break
        x, y = band(-15.0, 90.0, sides)()
        if not C.free(x, y, 90.0):
            continue
        n += 1
        C.claim(x, y, 90.0)
        kind = n % 3
        if kind == 0:
            inst(N + "DeadTree_%d" % R.choice([1, 2, 3]), x, y, g(x, y) + 4.0, R.uniform(0, 360), R.uniform(0.09, 0.13),
                 pitch=90.0 + R.uniform(-4, 4), cull=8000.0)
        elif kind == 1:
            L = R.uniform(0.35, 0.7)
            a = R.uniform(0, 360)
            beam((x, y, g(x, y) + 4.0), (x + 300.0 * L * math.cos(math.radians(a)), y + 300.0 * L * math.sin(math.radians(a)),
                                          g(x, y) + 2.0), 1.2)
        else:
            inst(PIR + "Planks_%d" % R.choice([0, 1, 2, 3]), x, y, g(x, y) - 2.0, R.uniform(0, 360), R.uniform(0.6, 0.9),
                 roll=R.uniform(-5, 5), cull=7000.0)
    # Beach-grass tufts along the top of the sand.
    C.scatter([N + "Grass_Wispy_Short", N + "Grass_Wispy_Tall", N + "Grass_Common_Short"], 40,
              band(150.0, 260.0, [(-4450.0, -1700.0), (1300.0, 4400.0)]), 70.0, sub="Beach", scale=(0.6, 1.0),
              cull=6000.0, sink=4.0)
    # A lost oar, a bottle with a message, a lone boot-sized fish.
    for (x, y, path, pitch) in [(-1700.0, 5560.0, WP + "SM_KG_Oar", 0.0), (1500.0, 5500.0, K + "Bottle", 80.0),
                                (-3500.0, 5620.0, K + "Bottle", 85.0), (3000.0, 5560.0, WP + "SM_KG_Oar", 0.0)]:
        inst(path, x, y, g(x, y) + 3.0, R.uniform(0, 360), 1.0, pitch=pitch, cull=5000.0)


def quay_lanterns():
    """Lantern posts along the top of the beach (unlit ones between the lit vignettes)."""
    for (x, y, yaw) in [(-3600.0, 4700.0, 10.0), (-2300.0, 4700.0, -15.0), (-650.0, 5200.0, 20.0),
                        (4000.0, 5100.0, -20.0)]:
        if C.free(x, y, 50.0, lane_gap=20.0):
            lantern_post(x, y, yaw)
        else:
            warn(f"lantern {x:.0f},{y:.0f} not free")


def rope_walk():
    """Rope walk on the west meadow: a 9 m line of T-posts carrying three strands being twisted into rope,
    the twisting bench at one end, the weighted sledge at the other, finished coils piled around."""
    x0, x1, y = -3450.0, -2600.0, 4170.0
    n = 6
    zs = []
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        gz = g(x, y)
        post(x, y, gz - 10.0, gz + 105.0, 0.7)
        beam((x, y - 28.0, gz + 100.0), (x, y + 28.0, gz + 100.0), 0.45)   # cross piece
        zs.append(gz + 106.0)
    for dy in (-16.0, 0.0, 16.0):
        for k in range(n):
            xa, xb = x0 + (x1 - x0) * k / n, x0 + (x1 - x0) * (k + 1) / n
            rope((xa, y + dy, zs[k]), (xb, y + dy, zs[k + 1]), 0.14)
    for k in range(int((x1 - x0) / 60.0) + 1):
        C.claim(x0 + k * 60.0, y, 60.0)
    # Twisting bench (west end) and the sledge (east end).
    put(P + "Workbench", x0 - 150.0, y, g(x0 - 150.0, y), yaw=90.0, sub="RopeWalk", cull=12000.0)
    put(P + "Barrel_Holder", x0 - 150.0, y + 150.0, g(x0 - 150.0, y + 150.0), yaw=0.0, sub="RopeWalk", cull=12000.0)
    for k in range(3):
        inst(P + "Rope_3", x0 - 140.0 + k * 5.0, y - 10.0, g(x0 - 150.0, y) + 89.0 + k * 8.0, R.uniform(0, 360),
             0.8 - k * 0.15, cull=6000.0)
    C.claim(x0 - 150.0, y + 60.0, 150.0)
    inst(P + "Crate_Metal", x1 + 110.0, y, g(x1 + 110.0, y), 0.0, 0.8, collide=True, cull=12000.0)
    inst(P + "Chain_Coil", x1 + 110.0, y - 90.0, g(x1 + 110.0, y - 90.0) + 1.0, 30.0, 0.8, cull=6000.0)
    rope((x1, y, zs[-1]), (x1 + 80.0, y, g(x1 + 110.0, y) + 70.0), 0.2)
    C.claim(x1 + 110.0, y - 40.0, 90.0)
    for (dx, dy) in [(-300.0, -130.0), (-280.0, 150.0), (200.0, 140.0), (420.0, -120.0)]:
        x, yy = (x0 + x1) / 2 + dx, y + dy
        if C.free(x, yy, 50.0):
            for k in range(R.randint(2, 4)):
                inst(P + "Rope_%d" % R.choice([2, 3]), x + R.uniform(-8, 8), yy + R.uniform(-8, 8),
                     g(x, yy) + 1.0 + k * 9.0, R.uniform(0, 360), 0.9 - k * 0.12, cull=6000.0)
            C.claim(x, yy, 55.0)
    seat(P + "Bench", (x0 + x1) / 2, y - 190.0, 90.0, height=49.6)


def sail_loft():
    """Sailcloth drying on lines between tall posts on the east meadow - colour visible from the whole bay."""
    posts = [(2050.0, 4130.0), (2400.0, 4180.0), (2750.0, 4150.0)]
    tops = []
    for (x, y) in posts:
        gz = g(x, y)
        post(x, y, gz - 15.0, gz + 290.0, 1.1)
        tops.append((x, y, gz + 280.0))
        C.claim(x, y, 40.0)
    cloths = [P + "Banner_1_Cloth", P + "Banner_2_Cloth"]
    for k, (a, b) in enumerate(zip(tops, tops[1:])):
        rope(a, b, 0.14)
        for t in (0.25, 0.72):
            x, y, z = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t - 4.0
            yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
            inst(cloths[(k + int(t * 2)) % 2], x, y, z, yaw + R.uniform(-4, 4), (1.7, 1.0, 0.82), cull=12000.0)
        steps = 6
        for j in range(steps + 1):
            C.claim(a[0] + (b[0] - a[0]) * j / steps, a[1] + (b[1] - a[1]) * j / steps, 50.0)
    # Sailmaker's basket and a folded pile under the line.
    inst(P + "Bucket_Wooden_1", 2230.0, 4260.0, g(2230.0, 4260.0), 0.0, 1.2, cull=6000.0)
    for k in range(3):   # folded sailcloth pile
        inst(P + "Banner_%d_Cloth" % (k % 2 + 1), 2600.0 + k * 6.0, 4290.0 + k * 4.0, g(2600.0, 4290.0) + 4.0 + k * 5.0,
             10.0 + k * 7.0, (0.9, 1.0, 0.45), pitch=90.0, cull=7000.0)
    C.claim(2420.0, 4280.0, 120.0)


def cooperage():
    """Barrel maker's yard on the east meadow: a barrel pyramid, staves, hoops and a steaming pot."""
    x, y = 3480.0, 4280.0
    gz = C.ground_min(x, y, 150.0)
    for row, n in enumerate((3, 2, 1)):
        for k in range(n):
            off = (k - (n - 1) / 2.0) * 72.0
            inst(P + "Barrel", x + off, y, gz + row * 86.0 - 2.0, R.uniform(0, 360), 1.0, collide=True, cull=12000.0)
    C.claim(x, y, 130.0)
    put(P + "Workbench", x + 40.0, y + 230.0, g(x + 40.0, y + 230.0), yaw=8.0, sub="Cooperage", cull=12000.0)
    for k in range(3):
        inst(PIR + "Planks_%d" % k, x - 20.0 + k * 30.0, y + 225.0, g(x, y + 230.0) + 89.0 + k * 6.0, 8.0 + k * 5.0,
             0.5, cull=6000.0)
    inst(P + "Chain_Coil", x + 200.0, y + 60.0, g(x + 200.0, y + 60.0) + 1.0, 0.0, 0.7, cull=6000.0)
    put(P + "Anvil", x + 230.0, y + 170.0, g(x + 230.0, y + 170.0), yaw=30.0, sub="Cooperage", cull=12000.0)
    logs_fire(x - 230.0, y + 160.0, None, 4, 0.7)
    put(P + "Cauldron", x - 230.0, y + 160.0, g(x - 230.0, y + 160.0) + 14.0, yaw=0.0, scale=0.85, sub="Cooperage",
        cull=12000.0)
    inst(P + "Barrel", x - 40.0, y + 110.0, g(x - 40.0, y + 110.0) + 35.0, 90.0, 1.0, pitch=90.0, collide=True,
         cull=12000.0)
    C.claim(x, y + 190.0, 190.0)
    seat(P + "Stool", x + 40.0, y + 320.0, 270.0, height=58.2)
    for (bx, by) in [(x + 180.0, y - 120.0), (x - 190.0, y - 60.0)]:
        if C.free(bx, by, 45.0):
            breakable(P + "Barrel", bx, by)


def gutting_station():
    """At the water's edge east of the office: fish are gutted on a trestle table, gulls wait for scraps."""
    x, y = 1330.0, 5400.0
    gz = C.ground_min(x, y, 120.0)
    put(P + "Table_Large", x, y, gz, yaw=5.0, scale=(0.7, 0.8, 1.0), sub="Gutting", cull=12000.0)
    fish_crate(x, y, gz + 81.0 - 6.0, 5.0, 6, crate=False)
    inst(K + "KitchenKnife", x + 40.0, y + 15.0, gz + 82.0, 70.0, 1.2, cull=4000.0)
    for (dx, dy) in [(-150.0, 40.0), (140.0, -30.0)]:
        inst(P + "Bucket_Wooden_1", x + dx, y + dy, g(x + dx, y + dy), R.uniform(0, 360), 1.1, cull=6000.0)
        inst(R.choice(FISH), x + dx, y + dy, g(x + dx, y + dy) + 27.0, R.uniform(0, 360), 1.0, roll=90.0, cull=6000.0)
    for (dx, dy, yaw) in [(-60.0, -110.0, 10.0), (60.0, -115.0, 80.0)]:
        fish_crate(x + dx, y + dy, g(x + dx, y + dy), yaw, 4)
    C.claim(x, y - 30.0, 170.0)
    for (px, py, top) in [(1500.0, 5620.0, 95.0), (1180.0, 5650.0, 80.0)]:
        post(px, py, g(px, py) - 20.0, top, 1.6, wide=True)
        put("/Game/KillGodot/Env/Birds/KG_Gull/StaticMeshes/KG_Gull", px, py, top + 6.0,
            yaw=R.uniform(0, 360), scale=(0.9, 0.9, 1.0), sub="Gulls", collide=False, cull=9000.0)
        C.claim(px, py, 40.0)


def west_quay_pile():
    """Between the pier and the boathouse: nets in coils, a crate pyramid and mooring bollards."""
    for (x, y) in [(-720.0, 5480.0), (-280.0, 5560.0)]:
        post(x, y, g(x, y) - 30.0, g(x, y) + 75.0, 1.7, wide=True)
        inst(P + "Rope_1", x + 35.0, y + 20.0, g(x + 35.0, y + 20.0) + 1.0, R.uniform(0, 360), 0.8, cull=6000.0)
        C.claim(x, y, 45.0)
    for k in range(4):
        inst(P + "Rope_3", -760.0 + R.uniform(-10, 10), 5330.0 + R.uniform(-10, 10), g(-760.0, 5330.0) + 1.0 + k * 9.0,
             R.uniform(0, 360), 1.1 - k * 0.15, cull=6000.0)
    C.claim(-760.0, 5330.0, 70.0)
    crate_stack(-300.0, 5390.0, 25.0, [(0, 0, 0), (1, 0, 0), (0.5, 0, 1)])
    for (x, y) in [(-420.0, 5540.0), (-610.0, 5580.0)]:
        if C.free(x, y, 45.0):
            breakable(R.choice([P + "Crate_Wooden", PIR + "crates_1"]), x, y)


def wreck():
    """East beach: the bleached remains of a boat half buried in the sand, planks strewn around."""
    x, y = 4050.0, 5460.0
    put(BOAT, x, y, g(x, y) - 18.0, yaw=35.0, pitch=-14.0, roll=48.0, sub="Wreck", cull=15000.0)
    for k in range(5):
        a = R.uniform(0, 2 * math.pi)
        px, py = x + R.uniform(120, 260) * math.cos(a), y + R.uniform(120, 260) * math.sin(a)
        inst(PIR + "Planks_%d" % R.choice([0, 1, 2, 3]), px, py, g(px, py) - 3.0, R.uniform(0, 360), 0.7,
             roll=R.uniform(-6, 6), cull=7000.0)
    inst(WP + "SM_KG_Oar", x + 150.0, y + 140.0, g(x + 150.0, y + 140.0) + 3.0, 200.0, 1.0, cull=6000.0)
    C.claim(x, y, 220.0)


def bay_boats():
    """Two boats riding at anchor out in the bay, each with its marker float."""
    for (x, y, yaw) in [(-1800.0, 7750.0, 30.0), (1150.0, 8700.0, -25.0)]:
        C.mover(BOAT, x, y, 6.0, yaw=yaw, bob=7.0, sway=3.0, sway_hz=0.16, sub="Boats")
        buoy(x + 260.0, y + 90.0)


def boathouse_store():
    """Gear stacked along the boathouse's landward wall (the alley to house 4 stays 3 m wide)."""
    y = 4430.0
    for (x, what) in [(-1530.0, "traps"), (-1400.0, "barrels"), (-1250.0, "crates"), (-1110.0, "rods"),
                      (-980.0, "barrels2")]:
        gz = C.ground_min(x, y, 60.0)
        if what == "traps":
            for lvl in range(3):
                inst(CAGE, x, y, gz + 3.0 + lvl * 80.0, R.uniform(-8, 8), 0.95, collide=True, cull=9000.0)
        elif what.startswith("barrels"):
            for k, dx in enumerate((-30.0, 35.0)):
                inst(P + "Barrel", x + dx, y + (k * 12.0), gz - 2.0, R.uniform(0, 360), 0.95, collide=True, cull=9000.0)
            if what == "barrels2":
                fish_crate(x, y, gz + 88.0, 0.0, 3)
        elif what == "crates":
            for (dx, lvl) in ((-44.0, 0), (44.0, 0), (0.0, 1)):
                inst(P + "Crate_Wooden", x + dx, y, gz + 5.0 + lvl * 88.0, R.uniform(-6, 6), 1.0, collide=True,
                     cull=9000.0)
        else:   # fishing rods and oars leaning on the wall
            for k in range(4):
                path = WP + ("SM_KG_FishingRod" if k % 2 else "SM_KG_Oar")
                inst(path, x - 45.0 + k * 28.0, y - 30.0, gz + 2.0, 90.0 + R.uniform(-6, 6), 1.0, pitch=74.0,
                     cull=6000.0)
        C.claim(x, y, 60.0)
    breakable(P + "Crate_Wooden", -870.0, 4400.0)


def fish_pen():
    """A ring of stakes in the east bay holding a floating fish pen, a plank catwalk on the landward side."""
    cx, cy, r = 3150.0, 6250.0, 280.0
    n = 9
    pts = []
    for k in range(n):
        a = k * 2.0 * math.pi / n
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        post(x, y, g(x, y) - 20.0, 95.0 + R.uniform(-15, 15), 1.3, wide=True)
        pts.append((x, y))
    for (a, b) in zip(pts, pts[1:] + pts[:1]):
        rope((a[0], a[1], 55.0), (b[0], b[1], 55.0), 0.16)
    for k in range(6):   # net floats around the rim
        a = k * 2.0 * math.pi / 6 + 0.3
        C.mover(BOBBER, cx + (r - 10.0) * math.cos(a), cy + (r - 10.0) * math.sin(a), -8.0, roll=90.0, scale=3.5,
                bob=6.0, sub="Buoys")
    for k in range(3):   # catwalk planks on stakes along the landward side
        x = cx - 200.0 + k * 200.0
        inst(TILE, x, cy - r - 90.0, 72.0, 0.0, (1.0, 0.6, 1.0), roll=R.uniform(-2, 2), collide=True, cull=0.0)
        for dx in (-90.0, 90.0):
            post(x + dx, cy - r - 150.0, g(x + dx, cy - r - 150.0) - 20.0, 70.0, 0.9)
    float_barrel(cx + 520.0, cy + 100.0)
    C.claim(cx, cy, r + 120.0)


def memorial():
    """Sailors' memorial on the east rise: a standing stone, candles, flowers and a bell facing the sea."""
    x, y = 4050.0, 4330.0
    gz = g(x, y)
    inst(N + "Rock_Medium_2", x, y, gz - 20.0, 70.0, (0.35, 0.3, 0.75), collide=True, cull=0.0)
    for (dx, dy) in [(-90.0, 80.0), (95.0, 70.0)]:
        put(P + "CandleStick_Stand", x + dx, y + dy, g(x + dx, y + dy), yaw=R.uniform(0, 360), sub="Memorial",
            cull=9000.0)
    for k in range(6):
        a = math.radians(40.0 + k * 20.0)
        cx2, cy2 = x + 70.0 * math.cos(a), y + 70.0 * math.sin(a)
        inst(P + "Candle_%d" % (k % 2 + 1), cx2, cy2, g(cx2, cy2), 0.0, 1.3, cull=4000.0)
    for (dx, dy, path) in [(-40.0, 110.0, N + "Flower_3_Group"), (50.0, 120.0, N + "Flower_4_Single"),
                           (0.0, 150.0, N + "Petal_2")]:
        inst(path, x + dx, y + dy, g(x + dx, y + dy) - 3.0, R.uniform(0, 360), 0.6, cull=6000.0)
    inst(WP + "SM_KG_Oar", x - 60.0, y - 10.0, gz + 2.0, 30.0, 1.0, pitch=70.0, cull=6000.0)
    # Bell frame beside the stone.
    bx, by = x + 180.0, y - 40.0
    for dy in (-45.0, 45.0):
        post(bx, by + dy, g(bx, by + dy) - 10.0, g(bx, by) + 190.0, 0.8)
    beam((bx, by - 60.0, g(bx, by) + 185.0), (bx, by + 60.0, g(bx, by) + 185.0), 0.7)
    put(WP + "SM_KG_Doorbell", bx, by, g(bx, by) + 176.0, yaw=0.0, scale=2.2, sub="Memorial", collide=False,
        cull=9000.0)
    C.claim(x + 60.0, y + 20.0, 190.0)
    seat(P + "Bench", x, y + 330.0, 90.0, height=49.6)


def beacon_pyre():
    """Unlit signal pyre on the west rise: a tall teepee of logs in a ring of stones, seen from the whole bay."""
    x, y = -3950.0, 4400.0
    gz = C.ground_min(x, y, 120.0)
    for k in range(9):
        a = k * 2.0 * math.pi / 9 + R.uniform(-0.1, 0.1)
        bx, by = x + 130.0 * math.cos(a), y + 130.0 * math.sin(a)
        beam((bx, by, gz - 10.0), (x + 8.0 * math.cos(a), y + 8.0 * math.sin(a), gz + 290.0), 1.6)
    for k in range(12):
        a = k * 2.0 * math.pi / 12
        inst(N + "Rock_Medium_%d" % (k % 3 + 1), x + 175.0 * math.cos(a), y + 175.0 * math.sin(a), gz - 18.0,
             R.uniform(0, 360), 0.14, collide=True, cull=0.0)
    woodpile(x + 260.0, y + 40.0, 90.0, rows=3, per=4)
    C.claim(x, y, 230.0)


def bait_shed():
    """Lean-to bait shed on the west slope: two small roofs on posts over bait barrels, crates and a table."""
    x, y, yaw = -1980.0, 4380.0, 0.0
    f = C.Frame(x, y, yaw, 440.0, 160.0, "bait")
    top = max(g(*f.world(sx * 220.0, sy * 80.0)) for sx in (-1, 1) for sy in (-1, 1)) + 225.0
    for lx in (-215.0, 0.0, 215.0):
        for ly in (-5.0, 145.0):
            wx, wy = f.world(lx, ly)
            post(wx, wy, g(wx, wy) - 10.0, top - (0.0 if ly < 0 else 60.0), 1.0)
    for lx in (-110.0, 110.0):
        wx, wy = f.world(lx, 0.0)
        put(V + "Roof_Wooden_2x1", wx, wy, top, yaw=yaw, sub="BaitShed", cull=15000.0)
    for (lx, ly) in [(-160.0, 80.0), (-90.0, 90.0)]:
        wx, wy = f.world(lx, ly)
        inst(P + "Barrel", wx, wy, g(wx, wy) - 2.0, R.uniform(0, 360), 0.9, collide=True, cull=9000.0)
    wx, wy = f.world(60.0, 80.0)
    put(K + "LongTable", wx, wy, g(wx, wy) - 2.0, yaw=yaw, sub="BaitShed", cull=9000.0)
    fish_crate(wx - 40.0, wy, g(wx, wy) + 65.0, yaw, 4)
    inst(P + "Bucket_Wooden_1", wx + 50.0, wy + 5.0, g(wx, wy) + 65.0, 0.0, 1.0, cull=5000.0)
    wx, wy = f.world(170.0, 70.0)
    fish_crate(wx, wy, g(wx, wy), yaw + 90.0, 4)
    fish_crate(wx + 5.0, wy - 5.0, g(wx, wy) + 24.0, yaw + 95.0, 3)
    C.claim(x, y + 60.0, 240.0)
    breakable(P + "Crate_Wooden", x + 300.0, y + 140.0)


def bunting(a, b, sway=6.0):
    """Pennant string (pack_village Bunting: 6 m along +X from its west end, 45 cm sag) between two points."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    L = math.hypot(dx, dy)
    C.mover(P1 + "Bunting", a[0], a[1], (a[2] + b[2]) / 2.0, yaw=math.degrees(math.atan2(dy, dx)),
            scale=(L / 600.0, 1.0, 1.0), sway=sway, sway_hz=0.3, sub="Bunting")


def hang_sign(kind, px, py, yaw):
    """Sign post (arm along +X after yaw) with a swinging hanging sign (pack_village recipe)."""
    pz = g(px, py)
    put(P1 + "SignPost", px, py, pz, yaw=yaw, sub="Signs", cull=12000.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    C.mover(P1 + "HangingSign_" + kind, px + c * 60.0, py + s * 60.0, pz + 270.0, yaw=yaw, sway=5.0, sway_hz=0.35,
            sub="Signs")
    C.claim(px, py, 45.0)


def festive():
    """Bunting over the pier and the jetty, signs for the market and the chowder stand, window boxes, laundry."""
    # Two tall masts beside the main pier carry bunting from the gate beam out over the deck.
    for sx in (-1.0, 1.0):
        prev = (sx * 250.0, 5130.0, 500.0)
        for y in (6450.0, 7900.0):
            x = sx * 190.0
            post(x, y, g(x, y) - 20.0, 520.0, 1.4, wide=True)
            bunting(prev, (x, y, 500.0))
            prev = (x, y, 500.0)
    # Jetty entrance masts with a string across and one along each side to the T-head lanterns' height.
    for sx in (-1.0, 1.0):
        x = JX + sx * 185.0
        post(x, 5180.0, g(x, 5180.0) - 20.0, 430.0, 1.3, wide=True)
        C.claim(x, 5180.0, 35.0)
    bunting((JX - 185.0, 5180.0, 415.0), (JX + 185.0, 5180.0, 415.0))
    for sx in (-1.0, 1.0):
        x = JX + sx * 185.0
        post(x, 6300.0, g(x, 6300.0) - 20.0, 430.0, 1.3, wide=True)
        bunting((x, 5180.0, 415.0), (x, 6300.0, 415.0))
    # Over the chowder tables: two poles and the stand's lantern post carry a zig-zag of pennants.
    bz = g(2000.0, 4630.0) + 290.0
    for (px, py) in [(1760.0, 5020.0), (1380.0, 4930.0)]:
        post(px, py, g(px, py) - 15.0, bz + 12.0, 1.1)
        C.claim(px, py, 35.0)
    bunting((1760.0, 5020.0, bz), (2000.0, 4630.0, bz))
    bunting((1380.0, 4930.0, bz), (1760.0, 5020.0, bz))
    # Signs.
    hang_sign("Fish", -330.0, 4200.0, 0.0)
    hang_sign("Ale", 2080.0, 4520.0, 180.0)
    # Harbourmaster's window boxes and door pots (office frame: centre (1000, 5260), yaw 0, door at local x -100).
    ox, oy = 1000.0, 5260.0
    fz = max(g(ox + sx * 210.0, oy + sy * 210.0) for sx in (-1, 0, 1) for sy in (-1, 0, 1)) + 4.0
    put(P1 + "FlowerBox", ox + 100.0, oy - 245.0, fz + 100.0, yaw=180.0, sub="Harbourmaster", collide=False, cull=7000.0)
    put(P1 + "FlowerBox", ox - 100.0, oy + 245.0, fz + 100.0, yaw=0.0, sub="Harbourmaster", collide=False, cull=7000.0)
    for (lx, ly, yaw) in [(-245.0, 100.0, 90.0), (245.0, 100.0, -90.0)]:
        put(P1 + "FlowerBox", ox + lx, oy + ly, fz + 110.0, yaw=yaw, scale=(0.7, 1.0, 1.0), sub="Harbourmaster",
            collide=False, cull=7000.0)
    for dx in (-190.0, -10.0):
        x, y = ox + dx, oy - 262.0
        put(P1 + "Planter_Pot", x, y, g(x, y), yaw=180.0, scale=0.8, sub="Harbourmaster", cull=6000.0)
    # Laundry of the harbourmaster's family between the office and the chowder stand.
    put(P1 + "LaundryLine", 1500.0, 5170.0, g(1500.0, 5170.0) - 3.0, yaw=0.0, sub="Laundry", cull=12000.0)
    C.claim(1500.0, 5170.0, 150.0)


# ============================================================================================ entry
def dress():
    claim_existing()
    pier_gate()
    pier_extras()
    harbourmaster_office()
    jetty()
    cargo_hoist()
    chowder_stand()
    market_extras()
    mend_nets_corner()
    signal_cannon()
    smokehouse()
    drying_racks()
    west_boatyard()
    west_beach_boats()
    smugglers_cove()
    lobster_yard()
    breakwater()
    burnt_pier()
    gutting_station()
    west_quay_pile()
    wreck()
    rope_walk()
    sail_loft()
    cooperage()
    boathouse_store()
    fish_pen()
    memorial()
    beacon_pyre()
    bait_shed()
    festive()
    bay_boats()
    bay_buoys()
    quay_lanterns()
    waterline_scatter()
    flush()
