"""Validate Tools/Level/morrowmere_layout_v2.json (Morrowmere v2 layout).

    python Tools/Level/validate_layout.py [path/to/layout.json] [--quiet]

Exit code 0 and a final "PASS" line when every hard rule holds. Hard rules:
  - the file parses and has every required section; buildings have at / size (even m) / face / z / terrace
  - no footprint overlaps: >= 1 m between buildings (members of one party-wall `block` may touch, never overlap),
    >= 1 m between buildings and lanes / stairs / ramps / squares / the stream; no building across a retaining wall
  - every home door (and every civic door) is within 3 m of a lane, stair, ramp, square or yard
  - every chore is within 3 m of a walkable surface (tower chores: the tower door)
  - terrace polygons do not overlap
  - every stair joins two adjacent terraces whose heights equal z0/z1, fits its kit risers, width multiple of 2 m
  - ramps are no steeper than 1:5; retaining walls separate terraces at their top_z / bottom_z
  - the walk network is connected (all homes, chores, the meeting place), with no dead ends except `dead_end_ok`
    spurs, and every district is reachable from the square by two edge-disjoint routes
  - town-core building coverage >= core.min_coverage
  - every designed sightline is clear in 3D (terrain terraces + building masses)
It also prints travel times from the fountain to every chore (run 5.8 m/s, stairs x1.18).

The helpers (load, rect, terrace_z, walkways, network...) are reused by render_layout.py.
"""
import json
import math
import os
import sys

import networkx as nx
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import unary_union

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.path.join(HERE, "morrowmere_layout_v2.json")
RUN, STAIR_MUL, RAMP_MUL = 5.8, 1.18, 1.05
KIT_ROOFS = {(4, 4), (4, 6), (4, 8), (6, 4), (6, 6), (6, 8), (6, 10), (6, 12), (8, 8), (8, 10), (8, 12), (6, 14), (8, 14)}
RIDGE = {4: 3.4, 6: 4.9, 8: 6.2}
RISER_RUN = 2.08


def load(path=DEFAULT):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ------------------------------------------------------------------------------------------------ geometry
def rect(b, pad=0.0):
    w, d = b["size"]
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    ux, uy = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
    cx, cy = b["at"]
    hw, hd = w / 2.0 + pad, d / 2.0 + pad
    return Polygon([(cx + sx * hw * ux[0] + sy * hd * uy[0], cy + sx * hw * ux[1] + sy * hd * uy[1])
                    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))])


def door(b, out=0.0):
    w, d = b["size"]
    cw = w // 2
    lx = -cw + 1 + 2 * (cw // 2) if w >= 4 else 0.0
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    c, s = math.cos(yaw), math.sin(yaw)
    ly = -d / 2.0 - out
    return (b["at"][0] + lx * c - ly * s, b["at"][1] + lx * s + ly * c)


def prof_z(pr, p):
    s = (p[0] - pr["origin"][0]) * pr["dir"][0] + (p[1] - pr["origin"][1]) * pr["dir"][1]
    st = pr["stops"]
    if s <= st[0][0]:
        return st[0][1]
    for (s0, z0), (s1, z1) in zip(st, st[1:]):
        if s <= s1:
            return z0 + (z1 - z0) * (s - s0) / (s1 - s0)
    return st[-1][1]


class World:
    """Pre-built shapely geometry for one layout."""

    def __init__(self, L):
        self.L = L
        self.land = Polygon(L["coast"]["land_polygon"]).buffer(0)
        self.terraces = [(t, Polygon(t["polygon"]).buffer(0)) for t in L["terraces"]]
        self.buildings = []
        for b in L["houses"]:
            self.buildings.append((b["id"], "home", b))
        for b in L["infill"]:
            self.buildings.append((b["id"], "infill", b))
        for k, b in L["landmarks"].items():
            if "size" in b and not b.get("is_prop"):
                self.buildings.append((b.get("id", k), "tower" if b.get("ladder") else "civic", dict(b, _key=k)))
        self.rects = {bid: rect(b) for bid, _, b in self.buildings}
        self.walk = []   # (name, kind, polygon, centreline or None, meta)
        for l in L["lanes"]:
            line = LineString(l["points"])
            self.walk.append((l["name"], "lane:" + l["kind"], line.buffer(l["width"] / 2.0, cap_style=2), line, l))
        for s in L["stairs"]:
            line = LineString([s["from"], s["to"]])
            self.walk.append((s["name"], "stair", line.buffer(s["width"] / 2.0, cap_style=2), line, s))
        for r in L["ramps"]:
            line = LineString(r["points"])
            self.walk.append((r["name"], "ramp", line.buffer(r["width"] / 2.0, cap_style=2), line, r))
        for q in L["squares"]:
            self.walk.append((q["name"], "square", Polygon(q["polygon"]).buffer(0), None, q))
        for a in L["areas"]:
            self.walk.append((a["name"], "yard", Point(a["center"]).buffer(a["radius"]), None, a))
        st = L["stream"]
        self.stream = LineString([p[:2] for p in st["points"]]).buffer(st["width"] / 2.0)

    def terrace_at(self, p):
        pt = Point(p)
        for t, g in self.terraces:
            if g.covers(pt):
                return t
        return None

    def terrace_z(self, p):
        t = self.terrace_at(p)
        if t is None:
            return None
        return t["z"] if t.get("z") is not None else prof_z(t["z_profile"], p)

    def ground_z(self, p):
        """Walkable/terrain height used for LOS: stairs and ramps override terraces; sea = 0."""
        pt = Point(p)
        for name, kind, g, line, m in self.walk:
            if kind in ("stair", "ramp") and g.covers(pt):
                t = line.project(pt) / max(1e-6, line.length)
                return m["z0"] + (m["z1"] - m["z0"]) * t
        z = self.terrace_z(p)
        if z is not None:
            return z
        return None if self.land.covers(pt) else 0.0

    def top_z(self, bid, b):
        if b.get("top_z"):
            return b["top_z"]
        w, d = b["size"]
        return b["z"] + 3.0 * b.get("storeys", 2) + RIDGE.get(min(w, d), 5.0)


# ------------------------------------------------------------------------------------------------ network
def network(W, step=1.5):
    """Walk graph: nodes along every walkway, junctions where corridors meet, squares/yards as open hubs."""
    G = nx.Graph()
    lines = []
    for name, kind, g, line, m in W.walk:
        if line is None:
            continue
        n = max(2, int(math.ceil(line.length / step)) + 1)
        pts = [line.interpolate(line.length * i / (n - 1)) for i in range(n)]
        mul = STAIR_MUL if kind == "stair" else RAMP_MUL if kind == "ramp" else 1.0
        ids = []
        for i, p in enumerate(pts):
            nid = (name, i)
            G.add_node(nid, pos=(p.x, p.y), walk=name, end=(i == 0 or i == n - 1), width=m.get("width", 2.0))
            ids.append(nid)
        for a, b in zip(ids, ids[1:]):
            pa, pb = G.nodes[a]["pos"], G.nodes[b]["pos"]
            G.add_edge(a, b, w=math.dist(pa, pb) * mul / RUN, walk=name)
        width = m.get("width", 2.0)
        lines.append((name, kind, width, ids, m))
    # Junctions between walkways whose corridors touch.
    for i, (na, ka, wa, ia, ma) in enumerate(lines):
        for nb, kb, wb, ib, mb in lines[i + 1:]:
            tol = (wa + wb) / 2.0 + 0.6
            best = {}
            for a in ia:
                pa = G.nodes[a]["pos"]
                for b in ib:
                    dd = math.dist(pa, G.nodes[b]["pos"])
                    if dd <= tol:
                        key = (a[1] // 6)
                        if key not in best or dd < best[key][0]:
                            best[key] = (dd, a, b)
            for dd, a, b in best.values():
                G.add_edge(a, b, w=dd / RUN, walk="junction")
    # Open areas: every walkway node within 1.2 m of the area joins a clique across it.
    for name, kind, g, line, m in W.walk:
        if line is not None:
            continue
        hub = ("hub", name)
        c = g.representative_point() if kind == "square" else g.centroid
        G.add_node(hub, pos=(c.x, c.y), walk=name, end=False, width=0.0, area=g)
        members = [nid for nid, d in G.nodes(data=True)
                   if nid[0] != "hub" and g.distance(Point(d["pos"])) <= d["width"] / 2.0 + 0.8]
        for nid in members:
            G.add_edge(hub, nid, w=math.dist(G.nodes[nid]["pos"], (c.x, c.y)) / RUN, walk=name)
    return G


def nearest_node(G, p, maxd=4.0, allowed=None):
    """Walk node for a point: a square/yard hub when the point is on (or 1.5 m off) that area, else the nearest node."""
    pt = Point(p)
    for nid, d in G.nodes(data=True):
        if nid[0] == "hub" and d["area"].distance(pt) <= 1.5 and (not allowed or d["walk"] in allowed):
            return nid, math.dist(d["pos"], p)
    best, bd = None, 1e9
    for nid, d in G.nodes(data=True):
        if allowed and d["walk"] not in allowed:
            continue
        dd = math.dist(d["pos"], p)
        if dd < bd:
            best, bd = nid, dd
    return (best, bd) if bd <= maxd else (None, bd)


# ------------------------------------------------------------------------------------------------ checks
def validate(L, quiet=False):
    err, warn, info = [], [], []
    W = World(L)
    need = ["lanes", "areas", "houses", "infill", "landmarks", "tasks", "plateau_z", "terraces", "retaining_walls",
            "stairs", "ramps", "stream", "bridges", "basin", "quay", "mole", "coast", "sightlines", "zones", "squares"]
    for k in need:
        if k not in L:
            err.append(f"missing section '{k}'")

    # --- every landmark / prop carries at, size, face and a z or terrace
    for k, b in L["landmarks"].items():
        for f in ("at", "size", "face"):
            if b.get(f) is None:
                err.append(f"landmark {k}: missing '{f}'")
        if b.get("z") is None and not b.get("terrace"):
            err.append(f"landmark {k}: needs z or terrace")

    # --- fields + kit sizes + terrace membership
    for bid, cat, b in W.buildings:
        for k in ("at", "size", "face", "z"):
            if b.get(k) is None:
                err.append(f"{bid}: missing '{k}'")
        if "terrace" not in b:
            err.append(f"{bid}: missing 'terrace'")
        w, d = b["size"]
        if w % 2 or d % 2:
            err.append(f"{bid}: size {w}x{d} is not in even metres")
        if cat in ("home", "infill") and (w, d) not in KIT_ROOFS:
            err.append(f"{bid}: {w}x{d} has no kit roof")
        t = W.terrace_at(b["at"])
        if t is None or t["name"] != b["terrace"]:
            err.append(f"{bid} ({b.get('name')}): centre is on terrace {t and t['name']}, declared {b['terrace']}")
        if not W.land.covers(W.rects[bid]):
            err.append(f"{bid}: footprint leaves the land")

    # --- footprints
    items = [(bid, b, W.rects[bid]) for bid, _, b in W.buildings]
    for i, (ia, ba, ra) in enumerate(items):
        for ib, bb, rb in items[i + 1:]:
            dist = ra.distance(rb)
            same = ba.get("block") and ba.get("block") == bb.get("block")
            if same:
                if ra.intersection(rb).area > 0.25:
                    err.append(f"{ia}/{ib}: party-wall units overlap ({ra.intersection(rb).area:.2f} m2)")
            elif dist < 1.0 - 1e-6:
                err.append(f"{ia} ({ba.get('name')}) / {ib} ({bb.get('name')}): gap {dist:.2f} m < 1 m")
        for name, kind, g, line, m in W.walk:
            if kind == "yard":
                continue
            dist = ra.distance(g)
            if dist < 1.0 - 1e-6:
                err.append(f"{ia} ({ba.get('name')}) vs {kind} {name}: gap {dist:.2f} m < 1 m")
        if ra.distance(W.stream) < 1.0:
            err.append(f"{ia}: within 1 m of the brook")
        for wl in L["retaining_walls"]:
            ln = LineString(wl["points"])
            if ra.buffer(-0.35).intersects(ln):
                err.append(f"{ia}: crosses retaining wall {wl['name']}")

    # --- street joins (garden walls must not block a walkway; arches only over narrow lanes)
    for j in L.get("street_joins", []):
        seg = LineString(j["points"])
        hits = [(n, m.get("width", 0)) for n, k, g, line, m in W.walk if k not in ("yard",) and g.intersects(seg)]
        if j["kind"] == "garden_wall" and hits:
            err.append(f"garden wall {j['between']} blocks {[h[0] for h in hits]}")
        if j["kind"] == "arch" and any(w > 4.5 for _, w in hits):
            err.append(f"arch {j['between']} spans a walkway wider than 4.5 m")

    # --- doors
    walk_union = unary_union([g for _, _, g, _, _ in W.walk])
    for bid, cat, b in W.buildings:
        if cat == "home" or (cat == "civic" and not b.get("open")) or cat == "tower":
            p = Point(door(b))
            dd = walk_union.distance(p)
            if dd > 3.0:
                err.append(f"{bid} ({b.get('name')}): door is {dd:.1f} m from any lane/stair/square")

    # --- chores
    towers = {b.get("_key"): b for _, cat, b in W.buildings if cat == "tower"}
    for t in L["tasks"]:
        p = Point(t["at"])
        if t.get("tower"):
            tw = towers.get(t["tower"])
            if not tw:
                err.append(f"task {t['id']}: tower '{t['tower']}' not found")
                continue
            p = Point(door(tw))
        dd = walk_union.distance(p)
        if dd > 3.0:
            err.append(f"task {t['id']}: {dd:.1f} m from any walkable surface")
        for bid, cat, b in W.buildings:
            if not t.get("tower") and W.rects[bid].contains(p):
                err.append(f"task {t['id']}: inside building {bid} (bots cannot open doors)")
    ids = [t["id"] for t in L["tasks"]]
    for k in ["DrawWater", "PostNotice", "FileReports", "RingBell", "LightCandles", "TendGraves", "ForgeNails", "SharpenTools",
              "BakeBread", "PourAle", "StockStall", "MendNets", "UnloadFish", "FuelLighthouse", "HarvestCarrots",
              "FeedAnimals", "ChopWood", "FixBoat"]:
        if k not in ids:
            err.append(f"chore id {k} missing")

    # --- terraces
    for i, (ta, ga) in enumerate(W.terraces):
        if not ga.is_valid or ga.area < 1.0:
            err.append(f"terrace {ta['name']}: invalid polygon")
        for tb, gb in W.terraces[i + 1:]:
            a = ga.intersection(gb).area
            if a > 0.5:
                err.append(f"terraces {ta['name']} / {tb['name']} overlap by {a:.1f} m2")

    # --- stairs
    for s in L["stairs"]:
        ta, tb = W.terrace_at(s["from"]), W.terrace_at(s["to"])
        za, zb = W.terrace_z(s["from"]), W.terrace_z(s["to"])
        if ta is None or tb is None:
            err.append(f"stair {s['name']}: an end is off the terraces ({ta and ta['name']}, {tb and tb['name']})")
            continue
        if abs(za - s["z0"]) > 0.3 or abs(zb - s["z1"]) > 0.3:
            err.append(f"stair {s['name']}: terraces are {za:.1f}->{zb:.1f}, stair says {s['z0']}->{s['z1']}")
        if ta["name"] == tb["name"]:
            err.append(f"stair {s['name']}: both ends on {ta['name']}")
        elif Polygon(ta["polygon"]).distance(Polygon(tb["polygon"])) > 0.5:
            err.append(f"stair {s['name']}: {ta['name']} and {tb['name']} are not adjacent")
        rise = abs(s["z1"] - s["z0"])
        if abs(rise - round(rise)) > 1e-6 or rise < 1:
            err.append(f"stair {s['name']}: rise {rise} is not whole kit risers")
        length = math.dist(s["from"], s["to"])
        need_len = round(rise) * RISER_RUN + 1.2 * s.get("landings", 0)
        if length + 1e-6 < need_len:
            err.append(f"stair {s['name']}: {length:.1f} m run < {need_len:.1f} m needed ({round(rise)} risers)")
        if s["width"] % 2:
            err.append(f"stair {s['name']}: width {s['width']} is not a multiple of the 2 m kit piece")

    # --- ramps
    for r in L["ramps"]:
        length = LineString(r["points"]).length
        rise = abs(r["z1"] - r["z0"])
        if rise > 0 and length / rise < 5.0:
            err.append(f"ramp {r['name']}: grade 1:{length / rise:.1f} steeper than 1:5")
        for p, z in ((r["points"][0], r["z0"]), (r["points"][-1], r["z1"])):
            tz = W.terrace_z(p)
            if tz is not None and abs(tz - z) > 0.8:
                warn.append(f"ramp {r['name']}: end {p} z {z} vs terrain {tz:.1f}")

    # --- retaining walls
    for wl in L["retaining_walls"]:
        if wl["top_z"] == wl["bottom_z"]:
            continue
        pts = wl["points"]
        bad = 0
        for a, b in zip(pts, pts[1:]):
            mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            dx, dy = b[0] - a[0], b[1] - a[1]
            n = math.hypot(dx, dy)
            nx_, ny_ = -dy / n, dx / n
            z1 = W.terrace_z((mx + nx_ * 1.2, my + ny_ * 1.2))
            z2 = W.terrace_z((mx - nx_ * 1.2, my - ny_ * 1.2))
            zs = sorted(z for z in (z1, z2) if z is not None)
            if len(zs) < 2 or abs(zs[1] - wl["top_z"]) > 0.6 or abs(zs[0] - wl["bottom_z"]) > 0.6:
                bad += 1
        if bad:
            err.append(f"retaining wall {wl['name']}: {bad} segment(s) do not separate {wl['top_z']} / {wl['bottom_z']}")

    # --- network
    G = network(W)
    fountain = L["landmarks"]["fountain"]["at"]
    root = ("hub", "fountain_square") if ("hub", "fountain_square") in G else nearest_node(G, fountain, 20.0)[0]
    comp = nx.node_connected_component(G, root)
    dist = nx.single_source_dijkstra_path_length(G, root, weight="w")
    for nid, d in G.nodes(data=True):
        if nid[0] == "hub" or not d["end"]:
            continue
        if G.degree(nid) <= 1:
            m = [x for x in W.walk if x[0] == d["walk"]][0][4]
            if not m.get("dead_end_ok"):
                err.append(f"dead end: {d['walk']} at ({d['pos'][0]:.1f}, {d['pos'][1]:.1f})")
    missing = [w[0] for w in W.walk if not any(G.nodes[n]["walk"] == w[0] for n in comp)]
    for m in missing:
        err.append(f"walkway {m} is not connected to the square")
    times = []
    for t in L["tasks"]:
        p = t["at"]
        if t.get("tower"):
            p = door(towers[t["tower"]])
        nid, dd = nearest_node(G, p, 6.0)
        if nid is None or nid not in dist:
            err.append(f"task {t['id']}: unreachable on the walk network")
            continue
        extra = {"bell_tower": 9.0, "lighthouse": 12.0, "clock_tower": 9.0}.get(t.get("tower"), 0.0) / 1.5
        times.append((t["id"], dist[nid] + dd / RUN + extra, t.get("district")))
    for b in L["houses"]:
        nid, dd = nearest_node(G, door(b), 6.0)
        if nid is None or nid not in dist:
            err.append(f"home {b['id']}: door unreachable on the walk network")
    # Two edge-disjoint routes square -> each district (loops, no single chokepoint).
    Gs = nx.Graph()
    Gs.add_nodes_from(G.nodes)
    for u, v, d in G.edges(data=True):
        Gs.add_edge(u, v, capacity=1.0)
    district_nodes = {}
    for dist_name, lane_name in L.get("district_hubs", {}).items():
        ids = sorted(n for n in G.nodes if n[0] == lane_name)
        if not ids:
            err.append(f"district hub lane {lane_name} not found")
            continue
        district_nodes[dist_name] = ids[len(ids) // 2]
    for dist_name, nid in district_nodes.items():
        flow = nx.maximum_flow_value(Gs, root, nid)
        if flow < 2:
            err.append(f"district {dist_name}: only {flow:.0f} route(s) from the square (need a loop)")

    # --- coverage
    core = unary_union([g for t, g in W.terraces if t["name"] in L["core"]["polygons"]])
    built = unary_union([W.rects[bid] for bid, _, _ in W.buildings]).intersection(core).area
    cov = built / core.area
    info.append(f"town-core coverage {cov * 100:.1f}% ({built:.0f} / {core.area:.0f} m2)")
    if cov < L["core"]["min_coverage"]:
        err.append(f"town-core coverage {cov * 100:.1f}% < {L['core']['min_coverage'] * 100:.0f}%")

    # --- sightlines (3D)
    for s in L["sightlines"]:
        a, b = s["from"], s["to"]
        T = math.dist(a, b)
        n = int(T / 0.5)
        blocked = None
        for i in range(1, n):
            t = i / n
            d_from, d_to = t * T, (1 - t) * T
            if d_from < 2.0 or d_to < 4.0:
                continue
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            los = s["eye_z"] + (s["target_z"] - s["eye_z"]) * t
            gz = W.ground_z(p)
            if gz is not None and gz > los - 0.3:
                blocked = f"terrain z {gz:.1f} at ({p[0]:.1f}, {p[1]:.1f}), line at {los:.1f}"
                break
            pt = Point(p)
            for bid, cat, bb in W.buildings:
                if W.rects[bid].contains(pt) and W.top_z(bid, bb) > los:
                    blocked = f"{bid} ({bb.get('name')}) top {W.top_z(bid, bb):.1f} > line {los:.1f}"
                    break
            if blocked:
                break
        if blocked:
            err.append(f"sightline {s['name']} blocked: {blocked}")

    # --- homes
    n_homes = len(L["houses"])
    if not 16 <= n_homes <= 20:
        err.append(f"{n_homes} player homes (need 16-20)")
    info.append(f"{n_homes} homes, {len(L['infill'])} infill shells, "
                f"{sum(1 for _, c, _ in W.buildings if c in ('civic', 'tower'))} civic/towers")

    if not quiet:
        print("Travel from the fountain (run 5.8 m/s, stairs x1.18, ladders added):")
        for tid, sec, dname in sorted(times, key=lambda x: x[1]):
            print(f"  {tid:18s} {sec:5.1f} s   {dname}")
        far = max(times, key=lambda x: x[1]) if times else None
        if far:
            info.append(f"farthest chore {far[0]} {far[1]:.1f} s")
    return err, warn, info, times


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    L = load(args[0] if args else DEFAULT)
    err, warn, info, _ = validate(L, quiet="--quiet" in sys.argv)
    for m in info:
        print("INFO ", m)
    for m in warn:
        print("WARN ", m)
    for m in err:
        print("ERROR", m)
    print("PASS" if not err else f"FAIL ({len(err)} errors)")
    sys.exit(0 if not err else 1)


if __name__ == "__main__":
    main()
