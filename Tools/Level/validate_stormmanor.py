"""Validate Tools/Level/stormmanor_layout.json (map 2, Storm Manor). Companion of validate_layout.py (Morrowmere v2),
which stays untouched: the manor is a multi-floor interior, so it gets its own rules.

    python Tools/Level/validate_stormmanor.py [path/to/layout.json] [--quiet]

Exit code 0 and a final "PASS" line when every hard rule holds. Hard rules:
  - every section is present; every room corner is on the 2 m kit grid
  - no overlaps: rooms of one floor may touch but not overlap; nothing on the floor above a double-height void
  - every door sits on the shared wall of its two rooms (the whole door width), both rooms on one floor
  - every stair joins consecutive floors, its bottom/top lie in its lower/upper room, straight flights fit the kit
    (horizontal run >= 1.28 x rise)
  - connectivity (doors + stairs, never secret passages): every area is reachable from the meeting room, also at
    every Region Gate (N = 6, 8, 10, 12, 16, 20: the rooms open at N form one connected piece)
  - loops: dead-end areas must be flagged dead_end_ok, and a bridge (single door/stair whose loss cuts the map)
    may only cut off dead_end_ok areas
  - the meeting room is within gather_s_max (30 s) of every walkable point, at run speed (5.8 m/s, stairs x1.18)
  - every chore step (and every variant) is reachable; chore time (walk from the hall + carry + work) <= 75 s
  - every named room has >= 1 chore spot or secret passage end, and >= 1 hiding spot or evidence surface
  - the brief's counts: 18-24 named rooms, 3 floors + cellar, >= 3 secret passages, >= 15 chores
It prints the gather time per area, chore times and the Region Gate table. analyse() is reused by
render_stormmanor.py.
"""
import itertools
import json
import math
import os
import sys

import networkx as nx
from shapely.geometry import LineString, Point, Polygon

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.path.join(HERE, "stormmanor_layout.json")
GATES = [6, 8, 10, 12, 16, 20]
STEP = 1.0          # walk-grid spacing (m)
INSET = 0.45        # keep grid nodes this far from walls
LINK = 1.6          # doors / stairs / anchors link to grid nodes within this radius


def load(path=DEFAULT):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def resolve(step_at, vars_):
    ats = step_at if isinstance(step_at, list) else [step_at]
    out = []
    for a in ats:
        out.append(vars_.get(a[1:], a) if a.startswith("$") else a)
    return out


def variants(ch):
    return ch.get("variants") or [{"name": "-", "vars": {}}]


def analyse(L):
    err, warn, info = [], [], []
    for key in ("floors", "rooms", "doors", "stairs", "secrets", "windows", "anchors", "chores", "spawn", "meeting",
                "storm", "speeds"):
        if key not in L:
            err.append(f"missing section {key}")
    if err:
        return {"err": err, "warn": warn, "info": info}
    FZ = {f["id"]: f["z"] for f in L["floors"]}
    FORDER = [f["id"] for f in sorted(L["floors"], key=lambda f: f["z"])]
    RM = {r["id"]: r for r in L["rooms"]}
    PG = {r["id"]: Polygon(r["poly"]) for r in L["rooms"]}
    SP = L["speeds"]
    RUN, CARRY, CLIMB, SMUL = SP["run"], SP["walk_carry"], SP["climb"], SP["stair_mul"]

    # ---- grid + polygons
    for r in L["rooms"]:
        for x, y in r["poly"]:
            if abs(x / 2 - round(x / 2)) > 1e-6 or abs(y / 2 - round(y / 2)) > 1e-6:
                err.append(f"{r['id']}: corner ({x},{y}) off the 2 m kit grid")
        if not PG[r["id"]].is_valid or PG[r["id"]].area < 4:
            err.append(f"{r['id']}: invalid polygon")
        if r["floor"] not in FZ:
            err.append(f"{r['id']}: unknown floor {r['floor']}")

    # ---- overlaps
    for f in FZ:
        ids = [r["id"] for r in L["rooms"] if r["floor"] == f]
        for a, b in itertools.combinations(ids, 2):
            ov = PG[a].intersection(PG[b]).area
            if ov > 0.01:
                err.append(f"overlap {a} x {b} on {f}: {ov:.1f} m2")
    for r in L["rooms"]:
        if r.get("void"):
            up = FORDER[FORDER.index(r["floor"]) + 1]
            vp = Polygon(r["void"])
            if not PG[r["id"]].buffer(0.01).contains(vp):
                err.append(f"{r['id']}: void outside the room")
            for o in L["rooms"]:
                if o["floor"] == up and PG[o["id"]].intersection(vp).area > 0.01:
                    err.append(f"{o['id']} ({up}) sits over the {r['id']} void")

    # ---- doors
    for d in L["doors"]:
        a, b = d["a"], d["b"]
        if a not in RM or b not in RM:
            err.append(f"{d['id']}: unknown room")
            continue
        if RM[a]["floor"] != RM[b]["floor"]:
            err.append(f"{d['id']}: rooms on different floors")
            continue
        p = Point(d["at"])
        shared = PG[a].boundary.intersection(PG[b].boundary)
        if shared.is_empty or shared.distance(p) > 0.05:
            err.append(f"{d['id']}: not on the shared wall of {a} and {b}")
            continue
        h = d["width"] / 2.0
        segs = [LineString([(p.x - h, p.y), (p.x + h, p.y)]), LineString([(p.x, p.y - h), (p.x, p.y + h)])]
        if not any(shared.buffer(0.05).contains(s) for s in segs):
            err.append(f"{d['id']}: the {d['width']} m opening does not fit the shared wall")

    # ---- stairs
    for s in L["stairs"]:
        lo, up = RM.get(s["lower"]), RM.get(s["upper"])
        if not lo or not up:
            err.append(f"{s['id']}: unknown room")
            continue
        if FORDER.index(up["floor"]) != FORDER.index(lo["floor"]) + 1:
            err.append(f"{s['id']}: {lo['floor']} -> {up['floor']} are not consecutive floors")
        if not PG[s["lower"]].contains(Point(s["bottom"])):
            err.append(f"{s['id']}: bottom not inside {s['lower']}")
        if not PG[s["upper"]].contains(Point(s["top"])):
            err.append(f"{s['id']}: top not inside {s['upper']}")
        run = math.dist(s["bottom"], s["top"])
        rise = s["z1"] - s["z0"]
        if s["kind"] in ("grand", "service", "outdoor") and run < 1.28 * rise - 1e-6:
            err.append(f"{s['id']}: run {run:.2f} m too short for a {rise} m rise (kit needs {1.28 * rise:.2f})")

    # ---- anchors, secrets
    AN = {a["id"]: a for a in L["anchors"]}
    for a in L["anchors"]:
        if a["room"] not in RM or not PG[a["room"]].contains(Point(a["at"])):
            err.append(f"anchor {a['id']} not inside {a['room']}")
    for s in L["secrets"]:
        for k in ("a", "b"):
            rid, p = s[k], s["at_" + k]
            if rid not in RM or PG[rid].distance(Point(p)) > 0.01:
                err.append(f"secret {s['id']}: end {k} not in {rid}")

    # ---- walk graph (time in seconds at run speed)
    G = nx.Graph()
    nodes_of = {}
    for r in L["rooms"]:
        pg = PG[r["id"]]
        inner = pg.buffer(-INSET)
        x0, y0, x1, y1 = pg.bounds
        pts = {}
        nx_ = int((x1 - x0) / STEP)
        ny_ = int((y1 - y0) / STEP)
        for i in range(nx_):
            for j in range(ny_):
                p = (x0 + STEP * (i + 0.5), y0 + STEP * (j + 0.5))
                if inner.contains(Point(p)):
                    pts[(i, j)] = p
                    G.add_node((r["id"], i, j), room=r["id"], p=p)
        for (i, j), p in pts.items():
            for di, dj in ((1, 0), (0, 1), (1, 1), (1, -1)):
                q = (i + di, j + dj)
                if q in pts and (abs(di) + abs(dj) == 1 or ((i + di, j) in pts and (i, j + dj) in pts)):
                    G.add_edge((r["id"], i, j), (r["id"], *q), t=STEP * math.hypot(di, dj) / RUN)
        nodes_of[r["id"]] = pts
        if not pts:
            err.append(f"{r['id']}: no walkable grid")

    def link(node, rid, p, label):
        pts = nodes_of.get(rid, {})
        near = [(math.dist(p, q), k) for k, q in pts.items() if math.dist(p, q) <= LINK]
        if not near:
            near = sorted((math.dist(p, q), k) for k, q in pts.items())[:1]
            if not near or near[0][0] > 3.0:
                err.append(f"{label}: no walkable ground near {p} in {rid}")
                return
        for dd, k in near:
            G.add_edge(node, (rid, *k), t=dd / RUN)

    for d in L["doors"]:
        if d["a"] in RM and d["b"] in RM:
            n = ("door", d["id"])
            link(n, d["a"], d["at"], d["id"])
            link(n, d["b"], d["at"], d["id"])
    for s in L["stairs"]:
        if s["lower"] not in RM or s["upper"] not in RM:
            continue
        nb, nt = ("stb", s["id"]), ("stt", s["id"])
        link(nb, s["lower"], s["bottom"], s["id"])
        link(nt, s["upper"], s["top"], s["id"])
        rise = s["z1"] - s["z0"]
        if s["kind"] == "ladder":
            t = rise / CLIMB
        elif s["kind"] == "spiral":
            t = rise * 2.2 * SMUL / RUN
        else:
            t = math.hypot(math.dist(s["bottom"], s["top"]), rise) * SMUL / RUN
        G.add_edge(nb, nt, t=t, stair=s["id"])
    for a in L["anchors"]:
        if a["room"] in RM:
            link(("an", a["id"]), a["room"], a["at"], "anchor " + a["id"])
    mt = L["meeting"]
    link(("meet",), mt["room"], mt["at"], "meeting")
    SECRET_ROOMS = {rid for rid, r in RM.items() if r.get("secret")}
    travel_s = L.get("secret_rules", {}).get("travel_s", 3.0)
    for s in L["secrets"]:
        for k in ("a", "b"):
            if s[k] in RM:
                link(("sec", s["id"], k), s[k], s["at_" + k], "secret " + s["id"])
        # SPRINT-040: a secret ROOM is reached through its passage only (the only secret edges in the walk graph,
        # so the shortcuts never flatter the normal gather times)
        if s["a"] in SECRET_ROOMS or s["b"] in SECRET_ROOMS:
            G.add_edge(("sec", s["id"], "a"), ("sec", s["id"], "b"), t=travel_s, secret=s["id"])
    for rid in SECRET_ROOMS:
        if not any(rid in (s["a"], s["b"]) for s in L["secrets"]):
            err.append(f"secret room {rid} has no secret passage")

    T = nx.single_source_dijkstra_path_length(G, ("meet",), weight="t")
    room_max = {}
    for r in L["rooms"]:
        ts = [T.get((r["id"], *k), math.inf) for k in nodes_of.get(r["id"], {})]
        room_max[r["id"]] = max(ts) if ts else math.inf
        if room_max[r["id"]] == math.inf:
            err.append(f"{r['id']} is not reachable from the meeting room")
    gmax = mt["gather_s_max"]
    for rid, t in room_max.items():
        if t < math.inf and t > gmax:
            err.append(f"{rid}: farthest point is {t:.1f} s from the meeting room (> {gmax:.0f} s)")
    all_t = sorted(T[n] for n in G.nodes if len(n) == 3 and n in T and n[0] in RM)
    p90 = all_t[int(0.9 * (len(all_t) - 1))] if all_t else math.inf
    tmax = all_t[-1] if all_t else math.inf
    for s in L["secrets"]:
        for k in ("a", "b"):
            if T.get(("sec", s["id"], k), math.inf) == math.inf:
                err.append(f"secret {s['id']} end {k} unreachable")

    # ---- room graph: dead ends, bridges
    RG = nx.MultiGraph()
    RG.add_nodes_from(r for r in RM if r not in SECRET_ROOMS)      # secret rooms: checked through their passage
    for d in L["doors"]:
        RG.add_edge(d["a"], d["b"], key=d["id"])
    for s in L["stairs"]:
        RG.add_edge(s["lower"], s["upper"], key=s["id"])
    for rid in RG.nodes:
        deg = len(set(RG.neighbors(rid)) - {rid})
        if deg <= 1 and not RM[rid].get("dead_end_ok"):
            err.append(f"{rid}: dead end (1 neighbour) without dead_end_ok")
    simple = nx.Graph()
    simple.add_nodes_from(RG.nodes)
    for u, v in RG.edges():
        simple.add_edge(u, v, n=RG.number_of_edges(u, v))
    bridges = []
    for u, v in nx.bridges(simple):
        if simple[u][v]["n"] > 1:
            continue
        H = simple.copy()
        H.remove_edge(u, v)
        comp = min(nx.connected_components(H), key=len)
        bridges.append((u, v, sorted(comp)))
        if not all(RM[c].get("dead_end_ok") for c in comp):
            err.append(f"bridge {u} - {v} cuts off {sorted(comp)} (a single point of failure)")
    if not nx.is_connected(simple):
        err.append("room graph is not connected")

    # ---- Region Gates
    gate_rows = []
    for N in GATES:
        open_ = {rid for rid, r in RM.items() if r["min_n"] <= N and rid not in SECRET_ROOMS}
        sub = simple.subgraph(open_)
        ok = nx.is_connected(sub) and mt["room"] in open_
        if not ok:
            err.append(f"N={N}: open rooms are not one connected piece")
        # SPRINT-040: open areas stay within the gather limit of the meeting room using open doors/stairs only
        open_nodes = [n for n in G.nodes if (len(n) == 3 and n[0] in open_) or n[0] in ("door", "stb", "stt", "meet")]
        Tn = nx.single_source_dijkstra_path_length(G.subgraph(open_nodes), ("meet",), weight="t")
        far = max((Tn.get(n, math.inf) for n in open_nodes if len(n) == 3), default=0.0)
        if far > gmax:
            err.append(f"N={N}: the farthest open point is {far:.1f} s from the meeting room (> {gmax:.0f} s)")
        n_ch = 0
        for ch in L["chores"]:
            if ch.get("secret"):
                continue
            for v in variants(ch):
                rooms = {AN[a]["room"] for st in ch["steps"] for a in resolve(st["at"], v["vars"]) if a in AN}
                if rooms <= open_:
                    n_ch += 1
                    break
        area = sum(PG[r].area for r in open_)
        gate_rows.append({"N": N, "areas": len(open_), "rooms": sum(1 for r in open_ if RM[r]["counts"]),
                          "chores": n_ch, "area_m2": round(area), "m2_per_player": round(area / N),
                          "far_s": round(far, 1)})
    if gate_rows and gate_rows[0]["chores"] < 8:
        err.append(f"N=6 has only {gate_rows[0]['chores']} chores open (< 8)")

    # ---- chores
    D = {}

    def dist_from(an):
        if an not in D:
            D[an] = nx.single_source_dijkstra_path_length(G, ("an", an), weight="t")
        return D[an]

    chore_rows = []
    for ch in L["chores"]:
        worst = 0.0
        for v in variants(ch):
            prev = None       # None = the meeting point
            total, carry = 0.0, None
            for st in ch["steps"]:
                ats = resolve(st["at"], v["vars"])
                for a in ats:
                    if a not in AN:
                        err.append(f"{ch['id']}/{v['name']}: unknown anchor {a}")
                if any(a not in AN for a in ats):
                    break
                spd = CARRY * (L["items"].get(carry, {}).get("speed", 1.0) if carry else 1.0)
                best, last = math.inf, None
                for perm in (itertools.permutations(ats) if st.get("any_order") else [ats]):
                    t, cur = 0.0, prev
                    for a in perm:
                        tt = (T.get(("an", a), math.inf) if cur is None else dist_from(cur).get(("an", a), math.inf))
                        t += tt * RUN / spd
                        cur = a
                    if t < best:
                        best, last = t, perm[-1]
                if best == math.inf:
                    err.append(f"{ch['id']}/{v['name']}: step at {ats} unreachable")
                    break
                total += best + st["secs"] * st.get("repeat", 1) * len(ats)
                prev = last
                if st["verb"] == "take":
                    carry = st["item"]
            worst = max(worst, total)
            chore_rows.append({"chore": ch["id"], "variant": v["name"], "t": round(total, 1)})
            if total > 75.0:
                err.append(f"{ch['id']}/{v['name']}: {total:.0f} s > 75 s")
            elif total > 55.0:
                warn.append(f"{ch['id']}/{v['name']}: {total:.0f} s (> 55 s, long)")
        ch["_worst"] = worst

    # ---- interactions per named room
    chore_rooms = {AN[a]["room"] for a in AN}
    sec_rooms = {s[k] for s in L["secrets"] for k in ("a", "b")}
    # SPRINT-040: compartments and traps are things to do too; both must sit inside their rooms
    for c in L.get("compartments", []):
        if c["room"] not in RM or not PG[c["room"]].contains(Point(c["at"])):
            err.append(f"compartment {c['id']} not inside {c['room']}")
        sec_rooms.add(c["room"])
    for t in L.get("traps", []):
        if t["room"] not in RM or not PG[t["room"]].contains(Point(t["at"])):
            err.append(f"trap {t['id']} not inside {t['room']}")
        if t.get("target") and (t.get("target_room") not in RM or not PG[t["target_room"]].contains(Point(t["target"]))):
            err.append(f"trap {t['id']}: target not inside {t.get('target_room')}")
        if t.get("target_room") and FORDER.index(RM[t["target_room"]]["floor"]) >= FORDER.index(RM[t["room"]]["floor"]):
            err.append(f"trap {t['id']}: a trapdoor must drop to a lower floor")
        sec_rooms.add(t["room"])
    for ch in L["chores"]:
        if ch.get("secret") and ch["secret"] not in {s["id"] for s in L["secrets"]}:
            err.append(f"{ch['id']}: unknown secret {ch['secret']}")
        if ch.get("reward_secret") and ch["reward_secret"] not in {s["id"] for s in L["secrets"]}:
            err.append(f"{ch['id']}: unknown reward secret {ch['reward_secret']}")
        if ch.get("reward_compartment") and ch["reward_compartment"] not in {c["id"] for c in L.get("compartments", [])}:
            err.append(f"{ch['id']}: unknown reward compartment {ch['reward_compartment']}")
    for r in L["rooms"]:
        if not r["counts"]:
            continue
        rid = r["id"]
        has_do = rid in chore_rooms or rid in sec_rooms or any(
            RM[x].get("part_of") == rid and x in chore_rooms for x in RM)
        if not has_do:
            err.append(f"{rid}: no chore spot or secret passage")
        if not (r.get("hides") or r.get("evidence")):
            err.append(f"{rid}: no hiding spot or evidence surface")

    # ---- brief counts
    n_named = sum(1 for r in L["rooms"] if r["counts"])
    floors_used = {r["floor"] for r in L["rooms"] if r["counts"]}
    # SPRINT-040 brief: >= 45 named rooms on the cellar, the second basement and 3 floors; >= 8 secret passages;
    # >= 10 hidden compartments; >= 6 trap kinds; >= 3 secret chores
    if n_named < 45:
        err.append(f"{n_named} named rooms (SPRINT-040 brief: >= 45)")
    if not {"C2", "C", "F0", "F1", "F2"} <= floors_used:
        err.append(f"named rooms only on {sorted(floors_used)} (need both basements + 3 floors)")
    if len(L["secrets"]) < 8:
        err.append("fewer than 8 secret passages")
    if len(L.get("compartments", [])) < 10:
        err.append("fewer than 10 hidden compartments")
    kinds = {t["effect"] for t in L.get("traps", [])}
    if len(kinds) < 6:
        err.append(f"only {len(kinds)} trap kinds (need 6)")
    if sum(1 for c in L["chores"] if c.get("secret")) < 3:
        err.append("fewer than 3 secret chores")
    if len(L["chores"]) < 15:
        err.append("fewer than 15 chores")
    n_rattle = sum(1 for w in L["windows"] if w.get("rattle"))
    if n_rattle < 6:
        err.append(f"only {n_rattle} rattle windows")

    info.append(f"{n_named} named rooms + {len(RM) - n_named} other areas, {len(L['doors'])} doors, "
                f"{len(L['stairs'])} stairs, {len(L['secrets'])} secret passages, {len(L['windows'])} windows "
                f"({n_rattle} rattle), {len(L['chores'])} chores")
    info.append(f"gather to the {mt['room']}: p90 {p90:.1f} s, max {tmax:.1f} s (limit {gmax:.0f} s)")
    return {"err": err, "warn": warn, "info": info, "room_max": room_max, "p90": p90, "tmax": tmax,
            "bridges": bridges, "gates": gate_rows, "chores": chore_rows, "G": G, "T": T}


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    L = load(args[0] if args else DEFAULT)
    res = analyse(L)
    quiet = "--quiet" in sys.argv
    for s in res["info"]:
        print("INFO", s)
    if not quiet and "room_max" in res:
        RM = {r["id"]: r for r in L["rooms"]}
        print("\nGather time to the meeting room (farthest point of each area, run 5.8 m/s):")
        for rid, t in sorted(res["room_max"].items(), key=lambda kv: -kv[1]):
            print(f"  {t:5.1f} s  {rid:18s} {RM[rid]['floor']:3s} {RM[rid]['name_tr']}")
        print("\nChores (walk 3.2 m/s from the hall, carry speed x item, + work):")
        for c in res["chores"]:
            print(f"  {c['t']:5.1f} s  {c['chore']}/{c['variant']}")
        print("\nRegion Gates:")
        for g in res["gates"]:
            print(f"  N={g['N']:2d}: {g['areas']:2d} areas, {g['rooms']:2d} named rooms, {g['chores']:2d} chores, "
                  f"{g['area_m2']} m2 ({g['m2_per_player']} m2/player), farthest {g['far_s']} s")
        print("\nBridges (single links):", [(u, v) for u, v, _ in res["bridges"]] or "none")
    for s in res["warn"]:
        print("WARN", s)
    for s in res["err"]:
        print("ERROR", s)
    print("FAIL" if res["err"] else "PASS")
    return 1 if res["err"] else 0


if __name__ == "__main__":
    sys.exit(main())
