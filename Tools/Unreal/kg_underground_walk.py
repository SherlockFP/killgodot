"""Walkability check of the built underground (read-only; runs inside -game on L_Morrowmere_v2, saves nothing).

  powershell -File Tools/Unreal/kg_underground_walk.ps1 [-Map /Game/KillGodot/Maps/Dev/L_KG_UndergroundCheck]
  (the scratch map comes from kg_build_underground.py --check-level: the underground alone, when the real level
  must not be rebuilt)
Samples every walkable cell of Tools/Level/underground_layout_v2.json on a 25 cm grid with a character-sized capsule
(radius 34 cm like AKGCharacter; the bottom 46 cm are left out = the default step height, so rubble and bones don't
count), links free neighbours with a short capsule sweep and flood-fills from the bottom of the well shaft ladder.
Logs KG_WALK lines: every region reached / not reached, every cell with no reachable sample, an ASCII plan
('#' reached, 'x' no free sample, 'o' free but cut off, 'S' stair cells are skipped), then quits.
The stair (S) is not sampled (it rises); the crypt side is flood-filled a second time from the stair foot.
The vault gate is closed in a fresh level, so the vault (V) is expected to be cut off; then `kg.Dig.Gate open` and
the vault must be reached from the corridor.
"""
import json
import os
import time

import sys

import unreal

ROOT = "D:/Kill Godot"
MAP_NAME = os.environ.get("KG_WALK_MAP", "L_Morrowmere_v2").split("/")[-1]
if f"{ROOT}/Tools/Level" not in sys.path:
    sys.path.insert(0, f"{ROOT}/Tools/Level")
import underground_layout as U  # noqa: E402  (region -> cell chars)
D = json.load(open(f"{ROOT}/Tools/Level/underground_layout_v2.json", encoding="utf-8"))
G = D["grid"]
X0, Y0, CELL, FZ = G["x0"] * 100.0, G["y0"] * 100.0, G["cell"] * 100.0, G["floor_z"] * 100.0
MAP = D["map"]
STEP = 25.0
R, HH = 34.0, 67.0          # capsule radius, half height (covers floor + 46 .. floor + 180 cm)
CZ = FZ + 46.0 + HH
VIS = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1


def log(m):
    unreal.log(f"KG_WALK {m}")


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == MAP_NAME and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


def blocked(world, a, b):
    hit = unreal.SystemLibrary.capsule_trace_single(world, a, b, R, HH, VIS, False, [], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    return bool(t and t[0])


def hit_name(world, a, b):
    hit = unreal.SystemLibrary.capsule_trace_single(world, a, b, R, HH, VIS, False, [], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    if not (t and t[0]):
        return "clear"
    try:
        comp = t[10]
        what = comp.static_mesh.get_name() if isinstance(comp, unreal.StaticMeshComponent) and comp.static_mesh else comp.get_name()
        return f"{t[9].get_class().get_name()}:{what}@{t[5].x:.0f},{t[5].y:.0f},{t[5].z:.0f}"
    except Exception as e:  # noqa: BLE001
        return f"hit ({e})"


def run(world):
    n = int(CELL / STEP)
    cells = {(i, j): MAP[j][i] for j in range(len(MAP)) for i in range(len(MAP[j])) if MAP[j][i] != " "}
    pts = {}
    for (i, j), ch in cells.items():
        if ch == "S":
            continue
        for a in range(n):
            for b in range(n):
                gx, gy = i * n + a, j * n + b
                p = unreal.Vector(X0 + (gx + 0.5) * STEP, Y0 + (gy + 0.5) * STEP, CZ)
                pts[(gx, gy)] = p
    free = {k for k, p in pts.items() if not blocked(world, p, unreal.Vector(p.x, p.y, p.z + 1.0))}
    log(f"samples {len(pts)}, free {len(free)}")

    def fill(start_xy):
        sx, sy = start_xy
        start = min(free, key=lambda k: (pts[k].x - sx) ** 2 + (pts[k].y - sy) ** 2)
        seen, todo = {start}, [start]
        while todo:
            k = todo.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (k[0] + dx, k[1] + dy)
                if q in free and q not in seen and not blocked(world, pts[k], pts[q]):
                    seen.add(q)
                    todo.append(q)
        return seen

    wi, wj = next((i, j) for (i, j), c in cells.items() if c == "X")
    shaft = (X0 + (wi + 0.5) * CELL, Y0 + (wj + 0.5) * CELL)
    reach = fill(shaft)
    si, sj = max((c for c in cells if cells[c] == "S"), key=lambda c: c[1])   # the stair foot is its southmost cell
    foot = (X0 + (si + 0.5) * CELL, Y0 + (sj + 1.5) * CELL)
    crypt = fill(foot)
    log(f"from the shaft: {len(reach)} samples; from the crypt stair foot: {len(crypt)} samples; "
        f"joined through the tunnel: {bool(reach & crypt)}")
    both = reach | crypt
    by_cell = {}
    for (gx, gy) in pts:
        c = (gx // n, gy // n)
        s = by_cell.setdefault(c, [0, 0])
        if (gx, gy) in free:
            s[0] += 1
        if (gx, gy) in both:
            s[1] += 1
    plan = []
    for j, row in enumerate(MAP):
        line = ""
        for i, ch in enumerate(row):
            if ch == " ":
                line += " "
            elif ch == "S":
                line += "S"
            else:
                f, r = by_cell.get((i, j), (0, 0))
                line += "#" if r else ("o" if f else "x")
        plan.append(line)
    for j, line in enumerate(plan):
        log(f"plan {j:2d} |{line}|  {MAP[j]}")
    for reg in U.REGIONS:
        chars = reg["chars"]
        rc = [c for c in cells if cells[c] in chars and cells[c] != "S"]
        got = sum(1 for c in rc if by_cell.get(c, (0, 0))[1])
        log(f"region {reg['name']}: {got}/{len(rc)} cells reached")
    cut = sorted(c for c in cells if cells[c] != "S" and not by_cell.get(c, (0, 0))[1])
    log(f"cut off cells: {[(c[0], c[1], cells[c]) for c in cut]}")
    # what closes each reached -> cut-off border: sweep the capsule across it on three lanes, name the first hit
    for c in cut:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            o = (c[0] + dx, c[1] + dy)
            if o not in cells or cells[o] == "S" or not by_cell.get(o, (0, 0))[1]:
                continue
            hits = []
            for lane in (-0.3, 0.0, 0.3):
                ax = X0 + (o[0] + 0.5 + lane * abs(dy)) * CELL
                ay = Y0 + (o[1] + 0.5 + lane * abs(dx)) * CELL
                a = unreal.Vector(ax, ay, CZ)
                b = unreal.Vector(ax - dx * CELL, ay - dy * CELL, CZ)
                hits.append(hit_name(world, a, b))
            log(f"border {o}{cells[o]} -> {c}{cells[c]}: {hits}")
    # the secret shortcut: the shaft side and the crypt side must meet (the tunnel); name what splits them
    joined = bool(reach & crypt)
    if not joined:
        side = {}
        for k in reach:
            side[(k[0] // n, k[1] // n)] = side.get((k[0] // n, k[1] // n), 0) | 1
        for k in crypt:
            side[(k[0] // n, k[1] // n)] = side.get((k[0] // n, k[1] // n), 0) | 2
        splits = [(k, (k[0] + dx, k[1] + dy)) for k in sorted(reach) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
                  if (k[0] + dx, k[1] + dy) in crypt]
        for k, q in splits[:12]:
            log(f"split {k}->{q} (cell {(k[0] // n, k[1] // n)}): {hit_name(world, pts[k], pts[q])}")
        log(f"split sample pairs: {len(splits)}")
        log(f"split cells (1 shaft side, 2 crypt side, 3 both): "
            f"{sorted((c, v) for c, v in side.items() if cells.get(c) in ('T', 'J'))}")
    return joined and not [c for c in cut if cells[c] != "V"], joined, not [c for c in cut if cells[c] != "V"]


def vault(world):
    """After kg.Dig.Gate open: flood from the gate cell's south neighbour over the vault + gate cells."""
    n = int(CELL / STEP)
    cells = {(i, j): MAP[j][i] for j in range(len(MAP)) for i in range(len(MAP[j])) if MAP[j][i] in "VGC"}
    pts = {}
    for (i, j) in cells:
        for a in range(n):
            for b in range(n):
                gx, gy = i * n + a, j * n + b
                pts[(gx, gy)] = unreal.Vector(X0 + (gx + 0.5) * STEP, Y0 + (gy + 0.5) * STEP, CZ)
    free = {k for k, p in pts.items() if not blocked(world, p, unreal.Vector(p.x, p.y, p.z + 1.0))}
    gi, gj = next(c for c, ch in cells.items() if ch == "G")
    sx, sy = X0 + (gi + 0.5) * CELL, Y0 + (gj + 1.5) * CELL
    start = min(free, key=lambda k: (pts[k].x - sx) ** 2 + (pts[k].y - sy) ** 2)
    seen, todo = {start}, [start]
    while todo:
        k = todo.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            q = (k[0] + dx, k[1] + dy)
            if q in free and q not in seen and not blocked(world, pts[k], pts[q]):
                seen.add(q)
                todo.append(q)
    vc = [c for c, ch in cells.items() if ch == "V"]
    got = sum(1 for c in vc if any((gx // n, gy // n) == c for (gx, gy) in seen))
    log(f"vault with the gate open: {got}/{len(vc)} cells reached")
    return got == len(vc)


class Walk:
    def __init__(self):
        self.t0 = time.time()
        self.state = "wait"
        self.result = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, dt):
        now = time.time() - self.t0
        if self.state == "wait":
            if now < 12.0:
                return
            self.world = game_world()
            if not self.world and now < 150.0:
                return
            if not self.world:
                log("no game world")
                return self.finish(False)
            try:
                self.result = run(self.world)
            except Exception as e:  # noqa: BLE001
                log(f"error {e}")
                return self.finish(False)
            unreal.SystemLibrary.execute_console_command(self.world, "kg.Dig.Gate open")
            self.state, self.at = "gate", now + 4.0     # the gate swings open on its own tick
        elif self.state == "gate" and now >= self.at:
            try:
                v = vault(self.world)
            except Exception as e:  # noqa: BLE001
                log(f"error {e}")
                v = False
            ok, joined, rooms = self.result
            log(f"verdict {'PASS' if ok and v else 'FAIL'} (every room but the locked vault reached: {rooms}, "
                f"shaft and crypt joined through the tunnel: {joined}, vault open with the gate open: {v})")
            self.finish(True)

    def finish(self, _ok):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        unreal.SystemLibrary.quit_game(self.world, None, unreal.QuitPreference.QUIT, False)


WALK = Walk()
