"""SPRINT-025: offscreen HUD proof on L_Morrowmere_v2 (run by Tools/Unreal/kg_hud_shots.ps1 in -game, KG game mode, no
window): the local villager gets fresh chores, is teleported to 3 distances from one of its chore stations (plus a case
where the station is behind the camera), and a HighResShot is taken of each moment; then the held (big) map and the
tapped (full) map through the kg.Map.Debug dev cvar.

Shots (Saved/Screenshots/HUD/<W>x<H>/hud_<name>.png): near, mid, far, edge, bigmap, fullmap, demo2 (working a chore),
demo3 (interact prompt).
Env: KG_HUD_W, KG_HUD_H (resolution), KG_HUD_TAG (file prefix).
"""
import math
import os
import time

import unreal

W = int(os.environ.get("KG_HUD_W", "1280"))
H = int(os.environ.get("KG_HUD_H", "720"))
TAG = os.environ.get("KG_HUD_TAG", "hud")
WARMUP_S = 22.0
DISTANCES = [("near", 300.0), ("mid", 1200.0), ("far", 3500.0)]


def log(m):
    unreal.log(f"KG_HUD_SHOTS {m}")


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == "L_Morrowmere_v2" and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


def ground(world, x, y, zhint):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, zhint + 300), unreal.Vector(x, y, zhint - 800),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        return hit.to_tuple()[5].z
    return zhint


class Capture:
    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.steps = []
        self.next_t = self.t0 + WARMUP_S
        self.station = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"started {W}x{H} tag={TAG}")

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)
        log(f"cmd {c}")

    def pc(self):
        return unreal.GameplayStatics.get_player_controller(self.world, 0)

    def shot(self, name):
        self.cmd(f"HighResShot {W}x{H} filename={TAG}_{name}")

    def pick_station(self):
        ps = self.pc().player_state
        ids = []
        try:
            ids = [str(t) for t in ps.get_editor_property("task_ids")]
        except Exception as e:
            log(f"task_ids unreadable: {e}")
        log(f"my chores: {ids}")
        stations = unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.KGTaskStation)
        by_id = {str(s.get_editor_property("task_id")): s for s in stations}
        for i in ids:
            if i in by_id:
                self.station = by_id[i]
                break
        if self.station is None and stations:
            self.station = stations[0]
        log(f"station: {self.station.get_editor_property('task_id') if self.station else None}")

    def place(self, dist, yaw_off=0.0):
        if not self.station:
            return
        # unreal.Controller exposes K2_GetPawn as get_controlled_pawn (there is no get_pawn); the reflected property
        # is the fallback.
        pc = self.pc()
        pawn = pc.get_controlled_pawn() if hasattr(pc, "get_controlled_pawn") else pc.get_editor_property("pawn")
        if pawn is None:
            log("no pawn yet")
            return
        at = self.station.get_actor_location()
        fwd = self.station.get_actor_forward_vector()
        # Stand along the station's facing so the prop is between us and the building behind it.
        ang = math.atan2(fwd.y, fwd.x)
        x = at.x + math.cos(ang) * dist
        y = at.y + math.sin(ang) * dist
        z = ground(self.world, x, y, at.z) + 96.0
        pawn.set_actor_location(unreal.Vector(x, y, z), False, True)
        yaw = math.degrees(math.atan2(at.y - y, at.x - x)) + yaw_off
        pitch = -math.degrees(math.atan2(max(0.0, z - at.z - 40.0), max(dist, 1.0)))
        self.pc().set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        log(f"placed at {dist:.0f} cm, yaw_off {yaw_off}")

    def plan(self):
        s = self.steps
        s.append((0.0, lambda: self.cmd("kg.Chore.Reset all")))
        s.append((1.0, self.pick_station))
        for name, dist in DISTANCES:
            s.append((0.2, lambda d=dist: self.place(d)))
            s.append((0.9, lambda n=name: self.shot(n)))
        s.append((0.2, lambda: self.place(1200.0, 135.0)))
        s.append((0.9, lambda: self.shot("edge")))
        s.append((0.2, lambda: self.place(1200.0)))
        s.append((0.2, lambda: self.cmd("kg.Map.Debug 1")))
        s.append((1.2, lambda: self.shot("bigmap")))
        s.append((0.2, lambda: self.cmd("kg.Map.Debug 2")))
        s.append((1.2, lambda: self.shot("fullmap")))
        s.append((0.2, lambda: self.cmd("kg.Map.Debug 0")))
        s.append((0.2, lambda: self.cmd("kg.HUDDemo 2")))
        s.append((1.0, lambda: self.shot("demo2")))
        s.append((0.2, lambda: self.cmd("kg.HUDDemo 3")))
        s.append((1.0, lambda: self.shot("demo3")))
        s.append((0.2, lambda: self.cmd("kg.HUDDemo 0")))
        s.append((2.0, lambda: self.cmd("quit")))

    def tick(self, dt):
        now = time.time()
        if now < self.next_t:
            return
        if self.world is None:
            self.world = game_world()
            if self.world is None:
                log("no game world yet")
                self.next_t = now + 2.0
                return
            self.plan()
        if not self.steps:
            return
        delay, fn = self.steps.pop(0)
        try:
            fn()
        except Exception as e:
            log(f"step failed: {e}")
        self.next_t = now + (self.steps[0][0] if self.steps else 0.5)


CAP = Capture()
