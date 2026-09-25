"""Orchard Upland (zone "countryside", slope z 8-11, the windmill knoll): open and sunny, hard to hide.

  * The farmyard in front of the barn: bale stacks, a roofed hen coop, the chopping block and log pile, a trough,
    a laden cart, beehives along the yard edge, a lantern post (FeedAnimals / HarvestCarrots stay clear).
  * The pen: a hay pile, a trough, buckets and feed sacks inside the builder's fence.
  * The walled fields: a scarecrow in each field with a flapping rag, wildflower strips along the walls, a tool left
    against a wall, water barrels at the gates.
  * The windmill knoll: flour sacks and a cart by the door (GrindFlour clear), a bench looking back over the town.
  * The apple orchard: apple crates and baskets under the trees, a picnic table, a ladder against a trunk.
  * Farmhouse H20: flower boxes, a vegetable garden, washing, a woodpile. Waymark signposts at the lane junctions.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, DV, DH, DW, IP, KI = C.V, C.N, C.P, C.PIR, C.WP, C.DV, C.DH, C.DW, C.IP, C.K
R = C.rng


def m(xm, ym):
    return xm * C.M, ym * C.M


def spot(x, y, r, reach=300.0, flat=45.0, **kw):
    for _ in range(3):
        s = C.find(x, y, r, reach=reach, **kw)
        if not s:
            return None
        if C.slope(s[0], s[1], min(r, 150.0)) <= flat:
            return s
        C.claim(s[0], s[1], r * 0.5)
    return None


def farmyard():
    fy = C.xy("farmyard")
    barn = C.building("mill_barn")
    face = barn.front_yaw()
    items = [((-420.0, -380.0), "bales"), ((380.0, -420.0), "coop"), ((-520.0, 180.0), "chop"), ((120.0, 520.0), "trough"),
             ((520.0, 180.0), "hives"), ((-150.0, -600.0), "cart"), ((0.0, 0.0), "lantern"), ((-300.0, 480.0), "bales2")]
    for (dx, dy), what in items:
        r = {"bales": 200.0, "bales2": 170.0, "coop": 180.0, "chop": 150.0, "trough": 120.0, "hives": 150.0,
             "cart": 200.0, "lantern": 40.0}[what]
        s = spot(fy[0] + dx, fy[1] + dy, r)
        if not s:
            continue
        x, y = s
        if what in ("bales", "bales2"):
            K.bale_stack(x, y, face + R.uniform(-20, 20), 1.0 if what == "bales" else 0.85)
        elif what == "coop":
            f = K.F(x, y, phi=K.face_dir(x, y, *fy))
            for sx in (-1, 1):
                for sy in (-1, 1):
                    K.post(*f.w(sx * 105.0, sy * 70.0), 190.0 if sy > 0 else 220.0, 1.0)
            f.put(V + "Roof_Wooden_2x1", 0.0, -80.0, 212.0, 0.0, scale=(1.1, 1.05, 0.8), collide=False, sub="Coop",
                  on_ground=True)
            for k, (lx, ly, dz) in enumerate(((-55.0, 20.0, 0.0), (35.0, 25.0, 0.0), (-10.0, 20.0, 78.0))):
                f.solid(P + "Cage_Small", lx, ly, dz, R.uniform(-10, 10))
            f.clutter(P + "FarmCrate_Empty", 70.0, -110.0, 0.0, 20.0)
            for k in range(4):
                f.clutter(N + "Grass_Wispy_Short", R.uniform(-80, 80), R.uniform(-40, 40), -5.0, None, 0.7)
            C.claim(x, y, r)
        elif what == "chop":
            K.solid(P + "Anvil_Log", x, y, None, R.uniform(0, 360))
            K.clutter(P + "Axe_Bronze", x, y, C.ground(x, y) + 100.0, None, 1.0, 0.0, 20.0)
            K.log_pile(x + 150.0, y + 60.0, face, 2, 280.0)
        elif what == "trough":
            K.solid(WP + "Rowboat", x, y, C.ground(x, y) + 16.0, face + 90.0, (0.55, 0.62, 0.5))
            K.clutter(P + "Bucket_Wooden_1", x + 90.0, y + 40.0, None, None, 1.0)
            C.claim(x, y, r)
        elif what == "hives":
            for j in range(3):
                K.beehive(x + (j - 1) * 95.0, y + R.uniform(-10, 10), face)
        elif what == "cart":
            f = K.F(x, y, phi=face + 60.0)
            f.put(V + "Prop_Wagon", 0, 0, 0, 0, sub="Farm", on_ground=True)
            for k, (lx, ly) in enumerate(((-40, -150), (40, -150), (-40, -70))):
                f.clutter(PIR + "Barrel_%d" % (1 + k % 3), lx, ly, 78.0, None, 0.7)
            for k, (lx, ly) in enumerate(((-35, 20), (35, 25))):
                f.clutter(P + "Bag", lx, ly, 62.0, None, 0.9)            # SPRINT-022: on the wagon bed
            C.claim(x, y, r)
        elif what == "lantern":
            K.lantern_post(x, y, face, light=7.0, radius=900.0)


def pen():
    pn = C.anchor("pen")
    x0, y0 = pn["x"], pn["y"]
    fx, fy = C.task_spots()["FeedAnimals"]
    for (dx, dy), what in (((-250.0, 200.0), "hay"), ((230.0, 220.0), "trough"), ((260.0, -220.0), "sacks"),
                           ((-260.0, -230.0), "buckets")):
        x, y = x0 + dx, y0 + dy
        if math.hypot(x - fx, y - fy) < 250.0:
            continue
        if what == "hay":
            K.bale_stack(x, y, R.uniform(0, 180), 0.7)
        elif what == "trough":
            K.solid(WP + "Rowboat", x, y, C.ground(x, y) + 16.0, 0.0, (0.5, 0.55, 0.45))
        elif what == "sacks":
            K.sacks(x, y, R.uniform(0, 360), 3)
        else:
            K.clutter(P + "Bucket_Wooden_1", x, y, None, None, 1.0)
            K.clutter(P + "Bucket_Metal", x + 45.0, y + 15.0, None, None, 1.0)
        C.claim(x, y, 90.0)
    for k in range(10):
        px, py = x0 + R.uniform(-420, 420), y0 + R.uniform(-420, 420)
        K.clutter(N + "Grass_Wispy_Short", px, py, None, None, R.uniform(0.5, 0.8))


def fields():
    hx, hy = C.task_spots()["HarvestCarrots"]
    for fdef in C.LAYOUT["fields"]:
        poly = C.cm(fdef["polygon"])
        cx, cy = C.centroid(poly)
        if fdef["crop"] != "apple rows (CommonTree_2 scale 0.8, 5 m grid)":
            K.scarecrow(cx + R.uniform(-200, 200), cy + R.uniform(-200, 200), R.uniform(0, 360))
        # wildflower strips outside the walls, a barrel by each corner gate
        for i in range(len(poly)):
            (ax, ay), (bx, by) = poly[i], poly[(i + 1) % len(poly)]
            L = math.hypot(bx - ax, by - ay)
            ux, uy = (bx - ax) / L, (by - ay) / L
            nx, ny = uy, -ux                                     # outward for a CCW-ish polygon; test below
            mx, my = (ax + bx) / 2, (ay + by) / 2
            if C.pip(poly, mx + nx * 150.0, my + ny * 150.0):
                nx, ny = -nx, -ny
            for k in range(int(L // 260)):
                s = 130.0 + k * 260.0
                px, py = ax + ux * s + nx * 170.0, ay + uy * s + ny * 170.0
                if C.free(px, py, 60.0) and math.hypot(px - hx, py - hy) > 250.0:
                    K.flower_patch(px, py, 90.0, 3)
            px, py = ax + nx * 200.0 + ux * 120.0, ay + ny * 200.0 + uy * 120.0
            if C.free(px, py, 60.0) and math.hypot(px - hx, py - hy) > 250.0:
                K.solid(P + "Barrel", px, py, None, R.uniform(0, 360))
                K.clutter(P + "Bucket_Wooden_1", px + 60.0, py + 20.0, None, None, 1.0)
                C.claim(px, py, 70.0)


def windmill_knoll():
    wm = C.building("windmill")
    gx, gy = C.task_spots()["GrindFlour"]
    for (t, out), what in (((-280.0, 170.0), "sacks"), ((300.0, 220.0), "cart"), ((0.0, 700.0), "bench")):
        x, y = wm.at("front", t, out)
        s = spot(x, y, 130.0 if what != "sacks" else 80.0)
        if not s or math.hypot(s[0] - gx, s[1] - gy) < 230.0:
            continue
        if what == "sacks":
            K.sacks(s[0], s[1], R.uniform(0, 360), 4)
            K.sacks(s[0] + 80.0, s[1] - 40.0, R.uniform(0, 360), 2)
        elif what == "cart":
            f = K.F(s[0], s[1], phi=wm.front_yaw() + 70.0)
            f.put(P + "Stall_Cart_Empty", 0, 0, 0, 0, sub="Mill", on_ground=True)
            for k in range(4):
                f.clutter(P + "Bag", -60 + k * 40, 0, 83.0, None, 0.85)
            C.claim(s[0], s[1], 180.0)
        else:
            K.bench(s[0], s[1], K.face_dir(s[0], s[1], *C.O_BASIN))


def orchard():
    poly = C.cm(next(f for f in C.LAYOUT["fields"] if f["name"] == "apple_orchard")["polygon"])
    cx, cy = C.centroid(poly)
    xs, ys = [p[0] for p in poly], [p[1] for p in poly]
    n = 0
    for _ in range(300):
        if n >= 14:
            break
        x, y = R.uniform(min(xs), max(xs)), R.uniform(min(ys), max(ys))
        if not C.pip(poly, x, y) or not C.free(x, y, 60.0, zone=False):
            continue
        roll = n % 4
        if roll == 0:
            K.clutter(P + "FarmCrate_Apple", x, y, None, None, 1.0)
            K.clutter(P + "FarmCrate_Apple", x + 10.0, y + 5.0, C.ground(x, y) + 24.0, None, 1.0)
        elif roll == 1:
            K.solid(P + "Barrel_Apples", x, y, None, R.uniform(0, 360))
        elif roll == 2:
            K.clutter(P + "FarmCrate_Empty", x, y, None, None, 1.0)
            K.clutter(P + "Bucket_Wooden_1", x + 50.0, y, None, None, 1.0)
        else:
            # a ladder against the trunk: two rails and rungs
            yaw = R.uniform(0, 360)
            dx, dy = C.fwd(yaw, 60.0)
            gz = C.ground(x, y)
            for side in (-1, 1):
                ox, oy = C.fwd(yaw + 90.0, 22.0 * side)
                K.rod((x + ox, y + oy, gz), (x + ox + dx, y + oy + dy, gz + 260.0), 0.35, cull=9000.0)
            for k in range(1, 7):
                t = k / 7.0
                a = (x - C.fwd(yaw + 90.0, 22.0)[0] + dx * t, y - C.fwd(yaw + 90.0, 22.0)[1] + dy * t, gz + 260.0 * t)
                b = (x + C.fwd(yaw + 90.0, 22.0)[0] + dx * t, y + C.fwd(yaw + 90.0, 22.0)[1] + dy * t, gz + 260.0 * t)
                K.rod(a, b, 0.25, cull=9000.0)
        C.claim(x, y, 60.0)
        n += 1
    s = spot(cx, cy, 140.0, reach=600.0, zone=False)
    if s:
        K.table_set(s[0], s[1], R.uniform(0, 90), stools=4, food=True)


def farmhouse():
    bs = K.zone_buildings("countryside")
    K.dress_buildings([b for b in bs if b.kind in ("home", "infill")], 0.8, 1.0, 0.4)
    h = C.building("H20")
    if not h:
        return
    for side, what in (("back", "veg"), ("left", "laundry"), ("right", "woodpile")):
        x, y = h.at(side, 0.0, K.FACADE + 280.0)
        s = spot(x, y, 200.0, reach=200.0)
        if not s:
            continue
        if what == "veg":
            K.veg_bed(s[0], s[1], h.nyaw(side) + 90.0, 360.0, 240.0)
        elif what == "laundry":
            K.laundry_line(s[0], s[1], h.tyaw(side))
        else:
            x, y = h.at(side, 0.0, K.FACADE + 40.0)
            K.woodpile(x, y, h.tyaw(side), 5)


def signposts():
    for a, targets in (("orchard_lane:start", ("windmill", "farmyard")), ("farm_track:start", ("farmyard", "windmill")),
                       ("hilltop_track:start", ("church", "farmyard"))):
        p = C.anchor(a)
        s = C.find(p["x"] + 250.0, p["y"] + 250.0, 40.0, reach=250.0)
        if s:
            K.signpost_arrows(s[0], s[1], [C.xy(t) for t in targets])


def dress():
    K.reset()
    for name, fn in (("farmyard", farmyard), ("pen", pen), ("fields", fields), ("windmill", windmill_knoll),
                     ("orchard", orchard), ("farmhouse", farmhouse), ("signs", signposts), ("smash", lambda: K.smash_piles([(C.xy("farmyard")[0] + 600.0, C.xy("farmyard")[1] - 200.0), (C.xy("windmill")[0] - 500.0, C.xy("windmill")[1] + 300.0)]))):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
