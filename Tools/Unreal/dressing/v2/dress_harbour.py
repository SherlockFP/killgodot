"""Harbour Row, the quay and the basin (zone "harbour", quay z 2, basin floor -3): the postcard, busy, many witnesses.

Composition, west -> east round the basin (the amphitheatre frame: O = basin centre, quay edge r 26 m, promenade
r 26-33 m, Harbour Row fronts r ~34 m, Tide Alley r 43 m):
  * the promenade gets grey stone flags; its sea edge a working rhythm between the builder's bollards (rope coils,
    lobster-trap stacks, fish barrels, net piles, a lifebuoy post at each water stair);
  * the house side a string of doorstep scenes: the net-menders' corner (MendNets), a harbour cafe terrace, drying
    racks, benches looking at the boats, flower boxes, the fishmonger's sign, laundry over Tide Alley;
  * the boathouse yard + slipway (FixBoat): a hull upturned on trestles, the tar pot, planks, oars, a workbench;
  * the jetty and the fish pier: deck-edge cargo on one side (a 2 m lane stays clear), mooring piles, boats alongside,
    a lantern at the T-head; the crane's cargo apron at the jetty root;
  * the Fish Market arcade: the day's catch on every table and counter, baskets, the tally desk, the Fish sign;
  * the mole: lobster pots and a fisherman's perch looking out to sea (the harbour-lamp chore stays clear);
  * the basin: buoys bobbing on the swell, loose barrels, extra boats moored alongside the jetty.
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
QZ = 200.0 + 2.0            # quay top
DECK = 122.0                # jetty / fish pier deck top (lane z 1.2 m + 2 cm)
O = C.O_BASIN
TH0, TH1 = 177.0, 314.0     # quay arc (degrees round O, continuous)


def m(xm, ym):
    return xm * C.M, ym * C.M


BOATS = [(b[0] * C.M, b[1] * C.M) for b in C.LAYOUT["boats"]]


def inward(x, y):
    """World yaw pointing from (x, y) towards the basin centre (the sea view)."""
    return K.face_dir(x, y, *O)


# ============================================================================================ promenade
def paving():
    K.pave_lane("quay_promenade", QZ - 0.5, mesh=V + "Floor_UnevenBrick")
    poly = C.square_poly("west_quay_apron")
    K.pave(poly, 90.0, lambda x, y, i, j: V + "Floor_UnevenBrick", QZ - 0.5, inset=30.0)


def sea_edge():
    """A working rhythm along the quay edge, 0.7-1.2 m in from the water, between the bollards."""
    kinds = ["ropes", "traps", "barrels", "netpile", "baskets", "traps", "crates", "ropes", "anchor", "baskets"]
    k = 0
    th = TH0 + 3.0
    while th < TH1 - 2.0:
        x, y = C.polar(2600.0 + 95.0, th)
        kind = kinds[k % len(kinds)]
        r = 55.0 if kind in ("ropes", "baskets") else 70.0
        if C.free(x, y, r, allow_lane=True, lane_gap=0.0):
            along = th + 90.0          # tangent (world yaw) of the arc
            f = K.F(x, y, phi=inward(x, y), z=QZ)
            if kind == "ropes":
                for j, rp in enumerate((P + "Rope_3", P + "Rope_2", P + "Rope_1")):
                    f.clutter(rp, (j - 1) * 45.0, 0.0, j * 8.0, None, 1.0)
            elif kind == "traps":
                f.solid(DH + "LobsterTrapStack", 0.0, 0.0, 0.0, R.uniform(-10, 10))
                f.clutter(DH + "Buoy_Red", 70.0, 20.0, 0.0, None, 0.55, 0.0, 80.0)
            elif kind == "barrels":
                f.solid(DH + "FishBarrel", -30.0, 0.0, 0.0, R.uniform(0, 360))
                f.solid(DH + "FishBarrel", 42.0, 12.0, 0.0, R.uniform(0, 360), 0.9)
            elif kind == "netpile":
                f.clutter(DH + "NetPile", 0.0, 0.0, 0.0, R.uniform(-20, 20), 0.85)
                f.clutter(DH + "Buoy_Striped", 55.0, 10.0, 0.0, None, 0.4, 0.0, 85.0)
            elif kind == "baskets":
                f.solid(DH + "FishBasket", -25.0, 0.0, 0.0, R.uniform(0, 360))
                K.fish_crate(*f.w(40.0, 5.0), QZ, along, 4)
            elif kind == "crates":
                f.solid(PIR + "crates_0", -30.0, 0.0, 0.0, R.uniform(-10, 10))
                f.solid(PIR + "crates_1", 40.0, 5.0, 0.0, R.uniform(-10, 10))
                f.clutter(PIR + "crates_0", -25.0, 0.0, 70.0, R.uniform(-20, 20), 0.8)
            elif kind == "anchor":
                f.solid(DH + "Anchor", 0.0, 0.0, 0.0, R.uniform(-30, 30), 0.8)
            C.claim(x, y, r)
            k += 1
            th += R.uniform(4.2, 6.0)
        else:
            th += 0.8
    # lifebuoy posts at the water stairs
    for ws in C.LAYOUT["quay"]["water_steps"]:
        x, y = m(*ws)
        rr, th = C.polar_of(x, y)
        for dth in (-4.5, 4.5):
            px, py = C.polar(2600.0 + 70.0, th + dth)
            if C.free(px, py, 30.0, allow_lane=True, lane_gap=0.0):
                K.post(px, py, 150.0, 0.9)
                K.prop(DH + "Lifebuoy", px, py, QZ + 70.0, yaw=inward(px, py) - 90.0, collide=False, sub="Quay")
                C.claim(px, py, 35.0)
                break


def house_side():
    """Doorstep scenes against the Harbour Row facades (the strip between the promenade and the walls)."""
    scenes = ["bench", "pots", "rack", "traps", "cafe", "oars", "bench", "barrels", "pots"]
    size = {"bench": (290.0, 35.0), "rack": (280.0, 65.0), "cafe": (230.0, 95.0), "pots": (180.0, 35.0),
            "traps": (200.0, 50.0), "oars": (250.0, 40.0), "barrels": (170.0, 45.0)}
    k = 0
    th = TH0 + 5.0
    while th < TH1 - 3.0:
        scene = scenes[k % len(scenes)]
        L, r = size[scene]
        rad = 3390.0 - 31.0 - r - 8.0
        x, y = C.polar(rad, th)
        along = th + 90.0
        t = C.terrace_at(x, y)
        if t and t["name"] == "quay" and C.free_line(x, y, along, L, r, allow_lane=True, lane_gap=0.0):
            sea = inward(x, y)
            f = K.F(x, y, phi=sea, z=QZ)
            if scene == "bench":
                K.bench(x, y, sea)
            elif scene == "rack":
                K.prop(DH + "FishRack", x, y, QZ, yaw=K.yaw_front(sea), sub="Quay")
            elif scene == "cafe":
                for j, lx in enumerate((-60.0, 60.0)):
                    K.table_set(*f.w(lx, 0.0), along + R.uniform(-10, 10), stools=2, food=j == 0)
            elif scene == "pots":
                for j in range(3):
                    K.potted(*f.w(-60.0 + j * 60.0, R.uniform(-8, 8)), big=j != 1)
            elif scene == "traps":
                f.solid(DH + "LobsterTrap", -50.0, 0.0, 0.0, R.uniform(-15, 15))
                f.solid(DH + "LobsterTrap", 55.0, 4.0, 0.0, R.uniform(-15, 15))
                f.clutter(DH + "LobsterTrap", 0.0, 2.0, 39.0, R.uniform(-25, 25), 0.95)
            elif scene == "oars":
                f.clutter(DH + "Oars", 0.0, 0.0, 0.0, R.uniform(-10, 10) + 90.0)
                f.solid(DH + "FishBarrel", 100.0, 0.0, 0.0, R.uniform(0, 360))
            elif scene == "barrels":
                f.solid(PIR + "Barrel_%d" % R.choice([0, 3, 4]), -45.0, 0.0, 0.0, R.uniform(0, 360), 0.72)
                C.breakable(P + "Barrel", *f.w(35.0, 0.0), yaw=R.uniform(0, 360))
            C.claim_line(x, y, along, L, r)
            k += 1
            th += R.uniform(3.2, 5.0)
        else:
            th += 0.6


def mend_nets():
    """The net-menders' corner round the MendNets chore: stools, a net pile, rope, a basket, a lantern post."""
    tx, ty = C.task_spots()["MendNets"]
    for dx, dy, what in ((-260.0, -40.0, "stool"), (-250.0, 150.0, "netpile"), (40.0, -260.0, "ropes"),
                         (-150.0, -230.0, "basket"), (260.0, 170.0, "lantern")):
        s = C.find(tx + dx, ty + dy, 45.0, reach=120.0, allow_lane=True, lane_gap=0.0)
        if not s:
            continue
        if what == "stool":
            K.stool(s[0], s[1], K.face_dir(s[0], s[1], tx, ty))
        elif what == "netpile":
            K.clutter(DH + "NetPile", s[0], s[1], QZ, None, 0.9)
        elif what == "ropes":
            K.clutter(P + "Rope_3", s[0], s[1], QZ, None, 1.0)
            K.clutter(P + "Rope_1", s[0] + 40.0, s[1] + 20.0, QZ + 8.0, None, 1.0)
        elif what == "basket":
            K.solid(DH + "FishBasket", s[0], s[1], QZ, R.uniform(0, 360))
        elif what == "lantern":
            K.lantern_post(s[0], s[1], K.face_dir(s[0], s[1], tx, ty), light=8.0, radius=800.0)
        C.claim(s[0], s[1], 45.0)


def boathouse_yard():
    """Boathouse + slipway (FixBoat): an upturned hull on trestles, tar pot, planks, oars, a workbench, a lantern."""
    bh = C.building("boathouse")
    fx, fy = C.task_spots()["FixBoat"]
    # hull on trestles on the apron north of the slipway
    for (dx, dy), what in (((150.0, -380.0), "hull"), ((-320.0, -150.0), "bench"), ((-330.0, 220.0), "planks"),
                           ((300.0, 330.0), "tar"), ((-180.0, 400.0), "oars")):
        x, y = bh.at("front", dx, K.FACADE + 150.0 + abs(dy) * 0.3)
        x, y = x + dy * 0.0, y + dy
        s = C.find(x, y, 90.0 if what != "hull" else 180.0, reach=200.0, allow_lane=True, lane_gap=0.0, zone=False)
        if not s or C.zone_of(*s) != "harbour":
            continue
        if what == "hull":
            for off in (-100.0, 100.0):
                px, py = s[0] + off * math.cos(math.radians(bh.yaw)), s[1] + off * math.sin(math.radians(bh.yaw))
                K.solid(P + "Stool", px, py, QZ - 2.0, bh.yaw)
            K.prop(WP + "Rowboat", s[0], s[1], QZ + 59.0 + 52.0, yaw=bh.yaw, roll=180.0, sub="Boatyard", cull=15000.0)
            C.claim(s[0], s[1], 190.0)
        elif what == "bench":
            K.solid(P + "Workbench", s[0], s[1], QZ, bh.yaw + 90.0)
            K.clutter(P + "Axe_Bronze", s[0] + 20.0, s[1], QZ + 89.0, None, 1.0, 0.0, 90.0)
            K.clutter(P + "Rope_1", s[0] - 40.0, s[1] + 10.0, QZ + 89.0, None, 0.8)
            C.claim(s[0], s[1], 110.0)
        elif what == "planks":
            for j in range(4):
                K.clutter(PIR + "Planks_%d" % (j % 4), s[0], s[1], QZ + j * 10.0, bh.yaw + R.uniform(-6, 6), 1.0)
            C.claim(s[0], s[1], 120.0)
        elif what == "tar":
            K.solid(P + "Cauldron", s[0], s[1], QZ, R.uniform(0, 360))
            K.woodpile(s[0] + 80.0, s[1] + 20.0, R.uniform(0, 180), 3)
            C.claim(s[0], s[1], 90.0)
        elif what == "oars":
            K.clutter(DH + "Oars", s[0], s[1], QZ, bh.yaw + R.uniform(-10, 10))
            C.claim(s[0], s[1], 90.0)
    K.hang_sign("Fish", bh, "front", bh.door_t + 190.0, 300.0)
    x, y = bh.at("front", -200.0, K.FACADE + 130.0)
    C.light(x, y, QZ + 260.0, 7.0, 800.0)


def jetty():
    """Deck-edge cargo on the jetty (one side at a time, 2 m clear), mooring piles, boats alongside, a T-head lantern."""
    pts = C.lane("long_jetty")
    L = C.polyline_len(pts)
    ux_, uy_ = (pts[1][0] - pts[0][0]) / L, (pts[1][1] - pts[0][1]) / L
    nx, ny = -uy_, ux_
    yaw = math.degrees(math.atan2(uy_, ux_))
    ux2, uy2 = C.task_spots()["UnloadFish"]
    s = 700.0
    side = 1
    while s < L - 200.0:
        x, y = pts[0][0] + ux_ * s, pts[0][1] + uy_ * s
        ex, ey = x + nx * side * 128.0, y + ny * side * 128.0
        if math.hypot(ex - ux2, ey - uy2) > 260.0:
            # deck-edge cargo is small and walk-through: the 3 m deck keeps its central 2 m clear
            what = R.choice(["crate", "basket", "rope", "barrel", "trap"])
            if what == "crate":
                K.fish_crate(ex, ey, DECK, yaw + R.uniform(-10, 10), 4)
            elif what == "basket":
                K.clutter(DH + "FishBasket", ex, ey, DECK, None, 0.7)
            elif what == "rope":
                K.clutter(P + "Rope_2", ex, ey, DECK, None, 0.9)
            elif what == "barrel":
                K.clutter(DH + "FishBarrel", ex, ey, DECK, None, 0.6)
            else:
                K.clutter(DH + "LobsterTrap", ex, ey, DECK, yaw + 90.0, 0.5)
        # mooring pile in the water just off the deck (walk-through: the builder's rowboats float with physics)
        px, py = x + nx * side * 200.0, y + ny * side * 200.0
        if all(math.hypot(px - bx, py - by) > 380.0 for bx, by in BOATS):
            K.clutter(DH + "Piling", px, py, -250.0, R.uniform(0, 360), (1.0, 1.0, 1.25))
        side = -side
        s += R.uniform(420.0, 560.0)
    # boats alongside (cosmetic, bobbing), clear of the builder's floating rowboats
    for s_, sd in ((1400.0, 1), (2300.0, -1), (2900.0, 1), (1900.0, -1)):
        x, y = pts[0][0] + ux_ * s_ + nx * sd * 330.0, pts[0][1] + uy_ * s_ + ny * sd * 330.0
        if all(math.hypot(x - bx, y - by) > 450.0 for bx, by in BOATS):
            K.moored_boat(x, y, yaw + R.uniform(-8, 8))
    # festival bunting along the jetty: from the root torches out to the T-head lantern posts
    tops = [(p[0], p[1], p[2] + 245.0) for p in C.PL["lamps"] if p[4] == "jetty"]
    tops.sort(key=lambda q: math.hypot(q[0] - pts[0][0], q[1] - pts[0][1]))
    for a, b in zip(tops, tops[1:]):
        K.bunting(a, b)
    # T-head: a lantern post at each end of the head, a crate pile, a fisherman on a stool
    hp = C.lane("jetty_head")
    hdx, hdy = hp[1][0] - hp[0][0], hp[1][1] - hp[0][1]
    hl0 = math.hypot(hdx, hdy)
    hnx, hny = -hdy / hl0, hdx / hl0                     # across the head
    if (hnx * (O[0] - hp[0][0]) + hny * (O[1] - hp[0][1])) > 0:
        hnx, hny = -hnx, -hny                            # the sea side of the head (away from the basin centre)
    for (hx, hy), light in ((hp[0], 9.0), (hp[1], 0.0)):
        cx, cy = (hp[0][0] + hp[1][0]) / 2, (hp[0][1] + hp[1][1]) / 2
        dx, dy = cx - hx, cy - hy
        d = math.hypot(dx, dy)
        px, py = hx + dx / d * 45.0 + hnx * 128.0, hy + dy / d * 45.0 + hny * 128.0
        K.post(px, py, 300.0, 1.1, z=DECK - 5.0)
        K.prop(P + "Lantern_Wall", px, py, DECK + 175.0, yaw=K.face_dir(px, py, *O) - 90.0 + 180.0, collide=False, sub="Jetty")
        if light:
            C.light(px, py, DECK + 230.0, light, 1100.0)
    hx, hy = hp[1]
    cx, cy = (hp[0][0] + hp[1][0]) / 2, (hp[0][1] + hp[1][1]) / 2
    fx, fy = hx + (cx - hx) * 0.18, hy + (cy - hy) * 0.18
    hux, huy = (hp[1][0] - hp[0][0]), (hp[1][1] - hp[0][1])
    hl = math.hypot(hux, huy)
    nhx, nhy = -huy / hl, hux / hl
    sx_, sy_ = fx + nhx * 128.0, fy + nhy * 128.0
    K.stool(sx_, sy_, math.degrees(math.atan2(nhy, nhx)), z=DECK)
    K.clutter(WP + "FishingRod", sx_ + nhx * 40.0, sy_ + nhy * 40.0, DECK + 45.0, math.degrees(math.atan2(nhy, nhx)), 1.0,
              -18.0, 0.0)
    K.clutter(P + "Bucket_Wooden_1", sx_ - nhy * 50.0, sy_ + nhx * 50.0, DECK, None, 1.0)


def fish_pier():
    pts = C.lane("fish_pier")
    (ax, ay), (bx, by) = pts[0], pts[-1]
    L = math.hypot(bx - ax, by - ay)
    ux_, uy_ = (bx - ax) / L, (by - ay) / L
    nx, ny = -uy_, ux_
    yaw = math.degrees(math.atan2(uy_, ux_))
    for s_, side, what in ((350.0, 1, "crates"), (650.0, -1, "basket"), (L - 120.0, 1, "stool")):
        x, y = ax + ux_ * s_ + nx * side * 128.0, ay + uy_ * s_ + ny * side * 128.0
        if what == "crates":
            K.fish_crate(x, y, DECK, yaw, 4)
            K.clutter(PIR + "crates_1", x, y, DECK + 24.0, yaw + 15.0, 0.4)
        elif what == "basket":
            K.clutter(DH + "FishBasket", x, y, DECK, None, 0.7)
        else:
            K.stool(x, y, yaw, z=DECK)
            K.clutter(WP + "FishingRod", x + ux_ * 40.0, y + uy_ * 40.0, DECK + 45.0, yaw, 1.0, -18.0, 0.0)
        px, py = ax + ux_ * s_ + nx * side * 200.0, ay + uy_ * s_ + ny * side * 200.0
        if all(math.hypot(px - bx, py - by) > 380.0 for bx, by in BOATS):
            K.clutter(DH + "Piling", px, py, -250.0, R.uniform(0, 360), (1.0, 1.0, 1.25))
    K.moored_boat(ax + ux_ * 500.0 - nx * 320.0, ay + uy_ * 500.0 - ny * 320.0, yaw + 180.0)


def crane_apron():
    """Cargo waiting under the hoist at the jetty root."""
    cx, cy = C.xy("crane")
    for dx, dy, what in ((-260.0, -140.0, "stack"), (230.0, -190.0, "sacks"), (-120.0, 260.0, "barrels")):
        s = C.find(cx + dx, cy + dy, 80.0, reach=160.0, allow_lane=True, lane_gap=0.0)
        if not s:
            continue
        if what == "stack":
            K.crate_stack(s[0], s[1], R.uniform(0, 90), ((0, 0, 0), (1, 0, 0), (0, 0, 1)), breakable_top=True)
        elif what == "sacks":
            K.sacks(s[0], s[1], R.uniform(0, 360), 4)
        else:
            K.barrels(s[0], s[1], 2, 45.0, breakable=1)


def fish_market():
    """The day's catch on the arcade's tables and counters, baskets and barrels, the tally desk, a Fish sign."""
    fm = C.anchor("fish_market")
    f = K.F(fm["x"], fm["y"], yaw=fm["yaw"], z=QZ)
    for lx in (-200.0, 200.0):               # Stall_Empty counters (builder)
        for k in range(8):
            f.clutter(K.FISH[k % 3], lx - 60 + (k % 4) * 40, -22 + (k // 4) * 30, 83 + 7, 90 + R.uniform(-15, 15), 1.0,
                      0.0, 90.0, on_ground=False)
    for k in range(10):                      # Table_Large (builder, local (0, 150), turned 0)
        f.clutter(K.FISH[k % 3], -110 + (k % 5) * 55, 130 + (k // 5) * 30, 81 + 7, R.uniform(-15, 15), 1.1, 0.0, 90.0,
                  on_ground=False)
    f.clutter(WP + "Fish_GoldenCarp", 20, 160, 81 + 9, 30, 1.2, 0.0, 90.0, on_ground=False)
    for lx, ly in ((-330.0, -40.0), (330.0, -60.0), (-280.0, 120.0)):
        f.solid(DH + "FishBasket", lx, ly, 0.0, R.uniform(0, 360))
    K.fish_crate(*f.w(260.0, 110.0), QZ, f.yaw, 5)
    K.fish_crate(*f.w(-240.0, -150.0), QZ, f.yaw + 20.0, 4)
    x, y = f.w(0.0, -420.0)
    s = C.find(x, y, 45.0, reach=200.0, allow_lane=True, lane_gap=0.0)
    if s:
        K.signpost("Fish", s[0], s[1], f.phi + 90.0)
    x, y = f.w(-120.0, 60.0)
    K.clutter(P + "BookStand", x, y, QZ, f.yaw, 1.0)


def mole():
    """Lobster pots along the mole's sea side, a fisherman's perch with a lantern, the lamp chore kept clear."""
    pts = C.lane("mole_walk")
    L = C.polyline_len(pts)
    lx, ly = C.task_spots()["LightHarbourLamp"]
    z = 250.0 + 2.0
    for k, (x, y, ux_, uy_, s_) in enumerate(C.resample(pts, 620.0, start=500.0, end_trim=500.0)):
        nx, ny = -uy_, ux_
        # sea side = away from the basin centre
        sx, sy = x + nx * 152.0, y + ny * 152.0
        if math.hypot(sx - O[0], sy - O[1]) < math.hypot(x - O[0], y - O[1]):
            sx, sy = x - nx * 152.0, y - ny * 152.0
        if math.hypot(sx - lx, sy - ly) < 300.0:
            continue
        yaw = math.degrees(math.atan2(uy_, ux_))
        if k % 3 == 1:
            K.stool(sx, sy, K.face_dir(x, y, sx, sy) , z=z)
            K.clutter(WP + "FishingRod", sx + (sx - x) * 0.4, sy + (sy - y) * 0.4, z + 45.0, K.face_dir(x, y, sx, sy), 1.0,
                      -20.0, 0.0)
            K.clutter(P + "Bucket_Wooden_1", sx + uy_ * 50.0, sy - ux_ * 50.0, z, None, 1.0)
            if k == 4:
                K.lantern_post(sx + ux_ * 80.0, sy + uy_ * 80.0, yaw + 90.0, light=7.0, radius=700.0)
        else:
            K.solid(DH + ("LobsterTrapStack" if k % 2 else "LobsterTrap"), sx, sy, z, yaw + R.uniform(-10, 10), 0.62)
            K.clutter(P + "Rope_%d" % R.randint(1, 3), sx + ux_ * 70.0, sy + uy_ * 70.0, z, None, 0.8)


def moored_rowboats():
    """The layout's moored rowboats (boats[]): the builder spawns them as physics-floating breakables that drift
    into the quay and flip; the dressing takes them over as bobbing moored boats with a mooring buoy each."""
    C.remove_builder("V2/Sea/Boats", "Rowboat")
    for bx, by in BOATS:
        yaw = inward(bx, by) + 90.0 + R.uniform(-20, 20)
        K.moored_boat(bx, by, yaw)
        dx, dy = C.fwd(yaw, 230.0)
        K.buoy(bx + dx, by + dy, "Buoy_Red")


def basin():
    """Buoys bobbing on the swell, a few loose barrels, clear of the jetty, piers and moored boats."""
    rng_pts = []
    boats = [m(*b) for b in C.LAYOUT["boats"]]
    for _ in range(400):
        if len(rng_pts) >= 18:
            break
        r = R.uniform(700.0, 2400.0)
        th = R.uniform(0, 360)
        x, y = C.polar(r, th)
        if C.lane_distance(x, y) < 250.0 or any(math.hypot(x - bx, y - by) < 350.0 for bx, by in boats):
            continue
        if any(math.hypot(x - px, y - py) < 450.0 for px, py in rng_pts):
            continue
        if C.ground(x, y) > -150.0:
            continue
        rng_pts.append((x, y))
    for k, (x, y) in enumerate(rng_pts):
        if k % 6 == 5:
            K.float_barrel(x, y)
        else:
            K.buoy(x, y)
            if k % 3 == 0:     # a pair (a trap line)
                K.buoy(x + R.uniform(-160, 160), y + R.uniform(-160, 160), "Buoy_Red")


def tide_alley():
    """Washing strung over Tide Alley from the Harbour Row backs to posts on the Harbour Wall parapet."""
    pts = C.lane("tide_alley")
    rows = [f for f in K.zone_buildings("harbour") if f.kind in ("home", "infill")]
    n = 0
    for x, y, ux_, uy_ in C.lane_samples("tide_alley", 520.0, start=300.0, end_trim=300.0):
        # the row back facing the alley: nearest building point on the sea side of the alley
        best = None
        for f in rows:
            for side in ("back", "front", "left", "right"):
                hx, hy = K.facade_pt(f, side, 0.0, 0.0)
                d = math.hypot(hx - x, hy - y)
                if d < 420.0 and (best is None or d < best[0]):
                    best = (d, f, side)
        if not best or R.random() < 0.3:
            continue
        _, f, side = best
        hx, hy = K.facade_pt(f, side, R.uniform(-100, 100), 2.0)
        # across the alley to the wall top (inland side)
        r0, th = C.polar_of(x, y)
        wx, wy = C.polar(4450.0 + 40.0, th)
        top = 500.0
        K.post(wx, wy, 230.0, 0.7, z=top - 5.0)
        K.laundry_across((hx, hy, f.z + 470.0), (wx, wy, top + 215.0), movers=2)
        n += 1
    C.stats["alley_lines"] = n


def quay_lights():
    """Lantern posts along the quay edge (every ~14 m, alternate ones lit) and a flag mast at the jetty root."""
    spots = []
    for th in range(int(TH0) + 4, int(TH1) - 2, 2):
        x, y = C.polar(2600.0 + 70.0, th)
        if C.free(x, y, 30.0, allow_lane=True, lane_gap=0.0):
            spots.append((x, y))
    n = K.lantern_row(spots, every=1400.0, light_every=2, max_lights=4, face=lambda x, y: inward(x, y) + 180.0)
    C.stats["quay_lanterns"] = n
    root = C.lane("long_jetty")[0]
    s = C.find(root[0] - 350.0, root[1] - 150.0, 40.0, reach=250.0, allow_lane=True, lane_gap=0.0)
    if s:
        top = K.post(s[0], s[1], 780.0, 1.3)
        C.mover(P + "Banner_2_Cloth", s[0], s[1], top + 20.0, yaw=60.0, scale=(0.9, 1.0, 0.5), sway=9.0, sway_hz=0.45,
                sub="Flags")
        C.claim(s[0], s[1], 45.0)


def row_fronts():
    bs = K.zone_buildings("harbour")
    K.dress_buildings(bs, boxes_ground=0.55, pots=0.9, ivy_chance=0.2)
    K.facade_rhythm([b for b in bs if b.kind in ("home", "infill", "civic")],
                    kinds=["pots", "barrel", "crate", "pots", "flowers", "butt"])
    homes = [f for f in bs if f.kind == "home"]
    if homes:
        f = homes[len(homes) // 2]
        K.hang_sign("Fish", f, "front", f.door_t - 170.0, 290.0)


def dress():
    K.reset()
    for name, fn in (("paving", paving), ("fronts", row_fronts), ("mend", mend_nets), ("boathouse", boathouse_yard),
                     ("crane", crane_apron), ("fish_market", fish_market), ("sea_edge", sea_edge),
                     ("house_side", house_side), ("jetty", jetty), ("fish_pier", fish_pier), ("mole", mole),
                     ("boats", moored_rowboats), ("quay_lights", quay_lights), ("basin", basin), ("tide_alley", tide_alley), ("smash", lambda: K.smash_piles([C.polar(3250.0, t) for t in (190.0, 225.0, 262.0, 300.0)], allow_lane=True, lane_gap=0.0))):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
