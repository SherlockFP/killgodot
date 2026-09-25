"""Brookside, the Morrow Brook valley (zone "beckside", slope z 1-18): work and water noise, shade.

  * The brook banks: reeds and grasses in the shallows, willow-like trees and ferns on the banks, stones at the
    cascades, driftwood where the brook meets the beach.
  * The smithy yard (ForgeNails / SharpenTools clear): the forge glow, an iron and coal heap, a quench barrel, horse
    shoes on the wall, a cart of tools, the Anvil sign on a post.
  * The watermill race at the waterwheel: a timber flume, sacks, a millstone, the miller's bench.
  * The tannery by the brook: stretched hides on frames, soaking vats (barrels), a dye rack of coloured cloth.
  * The woodcutter camp (ChopWood clear): big log piles, a sawhorse, a lean-to with a stool and an axe, a campfire.
  * The mill pond: a jetty of planks, a rowboat, a fisherman's stool; lily-like clover on the water edge.
  * The beach at the mouth: driftwood, shells, seaweed, a beached boat, a net rack, the smoke hut.
  * Brookside cottages: flower boxes, pots, gardens.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, DV, DH, DW, IP = C.V, C.N, C.P, C.PIR, C.WP, C.DV, C.DH, C.DW, C.IP
R = C.rng
STREAM = C.stream_pts()
SW = C.LAYOUT["stream"]["width"] * C.M


def m(xm, ym):
    return xm * C.M, ym * C.M


def spot(x, y, r, reach=300.0, flat=60.0, **kw):
    for _ in range(4):
        s = C.find(x, y, r, reach=reach, **kw)
        if not s:
            return None
        if C.slope(s[0], s[1], min(r, 150.0)) <= flat:
            return s
        C.claim(s[0], s[1], r * 0.6)
    return None


def banks():
    """Reeds in the shallows, trees and ferns on both banks, stones at the cascades."""
    n_tree = 0
    for x, y, ux, uy, s in C.resample(STREAM, 260.0, start=200.0, end_trim=300.0):
        nx, ny = -uy, ux
        for side in (-1, 1):
            # reeds right at the water line
            for k in range(2):
                off = SW * 0.5 + R.uniform(-30.0, 40.0)
                px, py = x + nx * side * off + ux * R.uniform(-90, 90), y + ny * side * off + uy * R.uniform(-90, 90)
                if C.lane_distance(px, py) < 60.0 or C.reserved_hit(px, py, 20.0):
                    continue
                K.clutter(N + R.choice(["Grass_Common_Tall", "Grass_Wispy_Tall", "Grass_Common_Tall"]), px, py,
                          C.ground(px, py) - 8.0, None, R.uniform(0.7, 1.1), cull=7000.0)
            # bank planting a little further out
            off = SW * 0.5 + R.uniform(250.0, 520.0)
            px, py = x + nx * side * off, y + ny * side * off
            if not C.free(px, py, 80.0) or C.zone_of(px, py) != "beckside":
                continue
            roll = R.random()
            if roll < 0.22 and n_tree < 26:
                K.tree(px, py, path=N + R.choice(["CommonTree_4", "CommonTree_1"]), s=R.uniform(0.6, 0.85))
                n_tree += 1
            elif roll < 0.55:
                K.clutter(N + "Fern_1", px, py, C.ground(px, py) - 5.0, None, R.uniform(0.4, 0.7))
                K.clutter(N + "Plant_1", px + 70.0, py + 40.0, C.ground(px, py) - 5.0, None, R.uniform(0.5, 0.8))
                C.claim(px, py, 80.0)
            elif roll < 0.7:
                sc = R.uniform(0.3, 0.5)             # SPRINT-022: bank rocks sink ~30 % of their height, not 45 cm
                K.solid(N + "Rock_Medium_%d" % R.randint(1, 3), px, py, C.ground_min(px, py, 60.0) - 60.0 * sc, R.uniform(0, 360), sc)
                C.claim(px, py, 90.0)
    C.stats["bank_trees"] = n_tree
    for cas in C.LAYOUT["stream"]["cascades"]:
        cx, cy = m(*cas["at"])
        for k in range(6):
            a = R.uniform(0, 6.28)
            px, py = cx + R.uniform(80, 260) * math.cos(a), cy + R.uniform(80, 260) * math.sin(a)
            if C.reserved_hit(px, py, 30.0):
                continue
            K.clutter(N + "Pebble_Round_%d" % R.randint(1, 5), px, py, C.ground(px, py) - 4.0, None, R.uniform(2.0, 3.5))


def smithy_yard():
    sm = C.building("smithy")
    tasks = [C.task_spots()[t] for t in ("ForgeNails", "SharpenTools")]
    front = sm.front_yaw()
    for (t, out), what in (((-320.0, 260.0), "heap"), ((320.0, 300.0), "quench"), ((0.0, 650.0), "cart"),
                           ((-420.0, -150.0), "coal"), ((300.0, 520.0), "sign")):
        x, y = sm.at("front", t, out) if out > 0 else sm.at("left", t, -out)
        r = {"heap": 110.0, "quench": 70.0, "cart": 200.0, "coal": 110.0, "sign": 45.0}[what]
        s = spot(x, y, r)
        if not s or any(math.hypot(s[0] - tx, s[1] - ty) < 230.0 for tx, ty in tasks):
            continue
        if what == "heap":
            for k in range(7):
                K.clutter(PIR + "Cannon_Ball_%d" % (k % 2), s[0] + R.uniform(-50, 50), s[1] + R.uniform(-50, 50),
                          C.ground(*s) + (k // 4) * 25.0, None, R.uniform(0.3, 0.45))
            K.clutter(P + "Chain_Coil", s[0] + 90.0, s[1], None, None, 1.0)
            C.claim(s[0], s[1], r)
        elif what == "quench":
            K.solid(P + "Barrel", s[0], s[1], None, R.uniform(0, 360))
            K.clutter(P + "Bucket_Metal", s[0] + 60.0, s[1] + 20.0, None, None, 1.0)
            C.claim(s[0], s[1], r)
        elif what == "cart":
            f = K.F(s[0], s[1], phi=front + 60.0)
            f.put(P + "Stall_Cart_Empty", 0, 0, 0, 0, sub="Smithy", on_ground=True)
            for k, pth in enumerate((P + "Sword_Bronze", P + "Axe_Bronze", P + "Pickaxe_Bronze", P + "Shield_Wooden")):
                f.clutter(pth, -80 + k * 50, 0, 88.0, 90.0, 1.0, 0.0, 90.0)
            C.claim(s[0], s[1], r)
        elif what == "coal":
            K.clutter(WP + "DigMound", s[0], s[1], None, None, (1.6, 1.6, 1.2))
            K.clutter(N + "Pebble_Square_%d" % R.randint(1, 6), s[0] + 20.0, s[1], C.ground(*s) + 20.0, None, 1.4)
            K.clutter(WP + "Shovel", s[0] + 70.0, s[1] + 40.0, C.ground(*s) + 15.0, None, 1.0, -70.0, 0.0)
            C.claim(s[0], s[1], r)
        elif what == "sign":
            K.signpost("Anvil", s[0], s[1], front + 90.0)
    fx, fy = sm.at("front", 0.0, -100.0)
    C.light(fx, fy, C.ground(fx, fy) + 120.0, 14.0, 700.0, (255, 110, 40))     # forge glow


def watermill_race():
    wx, wy = C.xy("waterwheel")
    ww = C.anchor("waterwheel")
    axle = ww["face_deg"]
    # a timber flume on posts leading from upstream onto the wheel
    ux, uy = math.cos(math.radians(axle + 90.0)), math.sin(math.radians(axle + 90.0))
    # upstream side = the stream point with the higher bed
    best = min(range(len(STREAM)), key=lambda i: math.hypot(STREAM[i][0] - wx, STREAM[i][1] - wy))
    up = STREAM[max(0, best - 1)]
    if (up[0] - wx) * ux + (up[1] - wy) * uy < 0:
        ux, uy = -ux, -uy
    zt = C.ground(wx, wy) + 360.0
    for k in range(5):
        d = 180.0 + k * 150.0
        px, py = wx + ux * d, wy + uy * d
        K.post(px + uy * 55.0, py - ux * 55.0, zt - C.ground(px, py) - 20.0, 0.8)
        K.post(px - uy * 55.0, py + ux * 55.0, zt - C.ground(px, py) - 20.0, 0.8)
    a = (wx + ux * 170.0, wy + uy * 170.0, zt)
    b = (wx + ux * 800.0, wy + uy * 800.0, zt + 40.0)
    for off in (-45.0, 0.0, 45.0):
        K.rod((a[0] + uy * off, a[1] - ux * off, a[2]), (b[0] + uy * off, b[1] - ux * off, b[2]), 1.5, cull=15000.0)
    C.place(C.E + "Plane", (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, zt + 18.0, yaw=math.degrees(math.atan2(uy, ux)),
            scale=(6.3, 0.8, 1.0), collide=False, sub="Race", material=C.DX + "M_KG_BrookWater_v2", overhead=True)
    s = spot(wx - uy * 450.0, wy + ux * 450.0, 110.0)
    if s:
        K.sacks(s[0], s[1], R.uniform(0, 360), 4)
        K.solid(P + "Whetstone", s[0] + 150.0, s[1] + 40.0, None, R.uniform(0, 360))
        C.claim(s[0] + 150.0, s[1] + 40.0, 80.0)
    s = spot(wx - uy * 650.0, wy + ux * 650.0, 140.0)
    if s:
        K.bench(s[0], s[1], K.face_dir(s[0], s[1], wx, wy))


def tannery():
    """Hide frames and soaking vats on the bank downstream of the Old Stone Bridge."""
    bx, by = C.xy("bridge:old_stone_bridge")
    s = spot(bx + 900.0, by + 1300.0, 260.0, reach=700.0, water_ok=False)
    if not s:
        return
    x, y = s
    face = K.face_dir(x, y, *C.O_BASIN)
    f = K.F(x, y, phi=face)
    for k, lx in enumerate((-160.0, 0.0, 160.0)):
        ax, ay = f.w(lx, -60.0)
        K.post(ax - 55.0, ay, 180.0, 0.55)
        K.post(ax + 55.0, ay, 180.0, 0.55)
        K.clutter(IP + "Rug_Rect", ax, ay, C.ground(ax, ay) + 170.0, f.yaw, (0.45, 0.6, 1.0), 0.0, 90.0)
    for k, lx in enumerate((-120.0, 0.0, 120.0)):
        K.solid(PIR + "Barrel_%d" % R.choice([2, 5, 8]), *f.w(lx, 150.0), None, R.uniform(0, 360), 0.9)
    # dye rack: coloured cloth on a pole
    ax, ay = f.w(-260.0, 220.0)
    bx2, by2 = f.w(260.0, 220.0)
    za, zb = K.post(ax, ay, 220.0, 0.6), K.post(bx2, by2, 220.0, 0.6)
    K.rod((ax, ay, za - 5.0), (bx2, by2, zb - 5.0), 0.28)
    for k in range(5):
        t = (k + 0.5) / 5.0
        C.mover(P + ("Banner_1_Cloth" if k % 2 else "Banner_2_Cloth"), ax + (bx2 - ax) * t, ay + (by2 - ay) * t,
                za - 8.0, yaw=f.yaw, scale=(0.45, 1.0, 0.5), sway=4.0, sway_hz=0.3, sub="Tannery")
    C.claim(x, y, 300.0)


def woodcutter():
    wx, wy = C.xy("woodcutter")
    cx, cy = C.task_spots()["ChopWood"]
    for (dx, dy), what in (((450.0, 250.0), "logs"), ((-420.0, 300.0), "logs2"), ((300.0, -420.0), "lean"),
                           ((-350.0, -380.0), "fire"), ((0.0, 520.0), "saw")):
        s = spot(wx + dx, wy + dy, 190.0 if what.startswith("logs") else 150.0)
        if not s or math.hypot(s[0] - cx, s[1] - cy) < 250.0:
            continue
        face = K.face_dir(s[0], s[1], wx, wy)
        if what.startswith("logs"):
            K.log_pile(s[0], s[1], face + 90.0, 3 if what == "logs" else 2, 360.0)
        elif what == "lean":
            f = K.F(s[0], s[1], phi=face)
            for lx in (-140.0, 140.0):
                K.post(*f.w(lx, -80.0), 210.0, 1.0)
                K.post(*f.w(lx, 80.0), 150.0, 1.0)
            for i in range(4):
                f.clutter(V + "Floor_WoodDark", -105 + i * 70, 0, 190.0, 0.0, (0.36, 0.95, 1.0), -18.0, 0.0)
            C.seat(P + "Stool", *f.w(0.0, 20.0), face)
            K.woodpile(*f.w(0.0, 90.0), f.yaw, 4)
            C.claim(s[0], s[1], 190.0)
        elif what == "fire":
            K.campfire(s[0], s[1], light=10.0)
            K.log(s[0] + 160.0, s[1], C.ground(s[0] + 160.0, s[1]), face, 170.0, 1.6)
        else:
            f = K.F(s[0], s[1], phi=face)
            for sx in (-1, 1):
                for sgn in (-1, 1):
                    a = f.w(sx * 55.0 + sgn * 20.0, 0.0)
                    b = f.w(sx * 55.0 - sgn * 20.0, 0.0)
                    K.rod((a[0], a[1], C.ground(*a)), (b[0], b[1], C.ground(*a) + 85.0), 0.5)
            K.log(s[0], s[1], C.ground(s[0], s[1]) + 60.0, f.phi + 90.0, 260.0, 1.5)
            C.claim(s[0], s[1], 150.0)


def mill_pond():
    p = C.anchor("pond:mill_pond")
    px, py, r = p["x"], p["y"], p["radius"] * C.M
    for a in range(0, 360, 20):
        x, y = px + (r + 40.0) * math.cos(math.radians(a)), py + (r + 40.0) * math.sin(math.radians(a))
        if C.lane_distance(x, y) < 80.0 or C.reserved_hit(x, y, 30.0):
            continue
        K.clutter(N + R.choice(["Grass_Common_Tall", "Grass_Wispy_Tall", "Clover_2"]), x, y, C.ground(x, y) - 6.0, None,
                  R.uniform(0.7, 1.0))
    # plank landing + a rowboat + a fisherman's stool on the side away from the paths
    best = None
    for a in range(0, 360, 15):
        x, y = px + (r + 150.0) * math.cos(math.radians(a)), py + (r + 150.0) * math.sin(math.radians(a))
        if C.free(x, y, 120.0) and (best is None or C.lane_distance(x, y) > C.lane_distance(*best[:2])):
            best = (x, y, a)
    if best:
        x, y, a = best
        dx, dy = -math.cos(math.radians(a)), -math.sin(math.radians(a))
        z = p["z"] + 5.0            # SPRINT-022: the anchor z is already in cm (the landing used to hang at 1.48 km)
        for k in range(3):
            K.clutter(V + "Floor_WoodDark", x + dx * (k * 110.0), y + dy * (k * 110.0), z + 20.0, a, (0.55, 0.6, 1.0))
            # SPRINT-022: the jetty posts reach the pond bed (0.8 m under the water), they used to stop in mid-water
            px_, py_ = x + dx * (k * 110.0) - dy * 60.0, y + dy * (k * 110.0) + dx * 60.0
            zb = C.ground(px_, py_) - 10.0            # the pond bed (or the bank) under each post
            K.post(px_, py_, z + 20.0 - zb, 0.6, z=zb)
        C.seat(P + "Stool", x + dx * 180.0, y + dy * 180.0, a + 180.0, z=z + 21.0)
        K.clutter(WP + "FishingRod", x + dx * 240.0, y + dy * 240.0, z + 70.0, a + 180.0, 1.0, -20.0, 0.0)
        K.moored_boat(x + dx * 330.0 - dy * 200.0, y + dy * 330.0 + dx * 200.0, a + 90.0)
        C.claim(x, y, 150.0)


def beach():
    bx, by = C.xy("bridge:beach_bridge")
    placed = 0
    for _ in range(400):
        if placed >= 14:
            break
        x, y = bx + R.uniform(-2500, 1800), by + R.uniform(0, 1400)
        g = C.ground(x, y)
        if C.zone_of(x, y) != "beckside" or not (-10.0 < g < 140.0) or not C.free(x, y, 90.0):
            continue
        roll = placed % 5
        if roll == 0:
            K.solid(DH + "Driftwood_A", x, y, g - 8.0, R.uniform(0, 360))
        elif roll == 1:
            K.clutter(DH + "Driftwood_B", x, y, g - 5.0, None, 1.0)
        elif roll == 2:
            K.clutter(DH + "ShellCluster", x, y, g - 2.0, None, 1.0)
        elif roll == 3:
            K.clutter(DH + "SeaweedClump", x, y, g - 3.0, None, 1.0)
        else:
            K.clutter(DH + "NetPile", x, y, g - 3.0, None, 0.8)
        C.claim(x, y, 90.0)
        placed += 1
    s = spot(bx - 900.0, by + 700.0, 190.0, reach=600.0, flat=80.0)
    if s:
        K.beached_boat(s[0], s[1], R.uniform(0, 360), upturned=False, lean=12.0)
    s = spot(bx + 600.0, by + 300.0, 250.0, reach=700.0, flat=80.0)
    if s:
        K.prop(DH + "SmokeHut", s[0], s[1], C.ground_min(s[0], s[1], 200.0) - 5.0,
               yaw=K.face_dir(s[0], s[1], bx, by) - 90.0, sub="Beach", claim=260.0)
        C.light(s[0], s[1], C.ground(s[0], s[1]) + 80.0, 4.0, 500.0, (255, 120, 60))
    s = spot(bx - 300.0, by + 250.0, 190.0, reach=600.0, flat=80.0)
    if s:
        K.prop(DH + "NetRack", s[0], s[1], None, yaw=R.uniform(0, 360), sub="Beach", claim=180.0)


def cottages():
    bs = [b for b in K.zone_buildings("beckside") if b.kind in ("home", "infill")]
    K.dress_buildings(bs, 0.7, 1.0, 0.35)
    for f in bs:
        side = "back" if K.exposed(f, "back") else "left"
        x, y = f.at(side, 0.0, K.FACADE + 260.0)
        s = spot(x, y, 180.0, reach=150.0)
        if s:
            kind = R.choice(["veg", "laundry", "wood"])
            if kind == "veg":
                K.veg_bed(s[0], s[1], f.nyaw(side) + 90.0, 300.0, 200.0)
            elif kind == "laundry":
                K.laundry_line(s[0], s[1], f.tyaw(side))
            else:
                K.woodpile(s[0], s[1], f.tyaw(side), 5)


def dress():
    K.reset()
    for name, fn in (("smithy", smithy_yard), ("mill", watermill_race), ("woodcutter", woodcutter), ("tannery", tannery),
                     ("pond", mill_pond), ("beach", beach), ("cottages", cottages), ("banks", banks), ("smash", lambda: K.smash_piles([(C.xy("smithy")[0] + 500.0, C.xy("smithy")[1] + 400.0), (C.xy("woodcutter")[0] - 500.0, C.xy("woodcutter")[1] - 300.0)]))):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
