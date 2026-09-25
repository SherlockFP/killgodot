"""SPRINT-016: offscreen proof of the world chores on Morrowmere v2 (run by Tools/Unreal/kg_worldchore_capture.ps1).

Inside a windowless -game session (plain GameModeBase, no match flow) a Slate post-tick callback:
  1. runs kg.WorldChore.Routes (navmesh route check -> Saved/KG_WorldChoreRoutes.json),
  2. for each pose: kg.WorldChore.Pose <name> (stages the key moment and aims the level's KG_CaptureCam), waits for the
     items and lights to settle, then "HighResShot WxH filename=WC_<name>" (console commands through the player
     controller, which is what routes HighResShot to the viewport in -game),
  3. quits. PNGs land in Saved/Screenshots/WindowsEditor/WC_<name>.png (the .ps1 moves them to Saved/Screenshots/WorldChores).
Env: KG_WC_SHOTS="WaterRun,Nets" (default all), KG_WC_NOSHOTS=1 (routes only).
"""
import os
import time

import unreal

W, H = 1600, 900
POSES = ["WaterRun", "FishToMarket", "BreadDelivery", "Lamplighter", "BellAndClock", "Nets", "Firewood", "Letters",
         "GrainToMill", "LighthouseOil", "Twist_Poison", "Twist_TwoCarry"]


def log(m):
    unreal.log(f"KG_WC_CAPTURE {m}")


# Look round 2 (SPRINT-016): some staged cameras landed inside a market stall, behind a villager's back or next to the
# lighthouse lamp (blown out). For these poses the capture re-aims KG_CaptureCam itself: it tries eyes on a ring around
# the focus (spots from the resolved chore data, ground-snapped) and takes the first one, nearest the preferred yaw, with
# a clear sphere sweep to the focus. dist/height in cm; lift = focus height above the ground.
REFRAME = {
    "FishToMarket": {"focus": ["brine_table"], "lift": 90, "dist": 560, "height": 300, "prefer": 209.5 + 180},
    "Lamplighter": {"focus": ["lamp_3", "lamp_4"], "lift": 170, "dist": 820, "height": 330, "prefer": 200},
    "Nets": {"focus": ["quay_posts"], "lift": 130, "dist": 460, "height": 170, "prefer": 118 - 90},
    "LighthouseOil": {"focus": ["lighthouse_lamp"], "lift": 60, "dist": 2400, "height": -500, "prefer": 225, "fov": 55},
    "Twist_Poison": {"focus": ["inn_trough"], "lift": 40, "dist": 340, "height": 200, "prefer": 166.5 + 180},
}
_ANCHORS = None


def anchor_xy(aid):
    global _ANCHORS
    if _ANCHORS is None:
        import json
        # "py <file>" in -game runs without __file__: the .ps1 passes the data path.
        for p in (os.environ.get("KG_WC_RESOLVED", ""),):
            if p and os.path.exists(p):
                _ANCHORS = {a["id"]: a for a in json.load(open(p))["anchors"]}
                break
        else:
            _ANCHORS = {}
    a = _ANCHORS.get(aid)
    return (a["at"][0] * 100.0, a["at"][1] * 100.0, a.get("z", 0.0) * 100.0) if a else None


def ground(world, x, y, zhint):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, zhint + 200), unreal.Vector(x, y, zhint - 600),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[5].z if hit else zhint


def reframe(world, name):
    import math
    spec = REFRAME.get(name)
    cams = unreal.GameplayStatics.get_all_actors_with_tag(world, "KG_CaptureCam")
    if not spec or not cams:
        return
    pts = [anchor_xy(a) for a in spec["focus"]]
    pts = [p for p in pts if p]
    if not pts:
        return
    fx = sum(p[0] for p in pts) / len(pts)
    fy = sum(p[1] for p in pts) / len(pts)
    fz = ground(world, fx, fy, max(p[2] for p in pts)) + spec["lift"]
    focus = unreal.Vector(fx, fy, fz)
    best = None
    for k in range(24):
        yaw = spec["prefer"] + (k + 1) // 2 * 15.0 * (1 if k % 2 else -1)
        r = math.radians(yaw)
        eye = unreal.Vector(fx + math.cos(r) * spec["dist"], fy + math.sin(r) * spec["dist"], fz + spec["height"])
        clear = True
        for a, b in ((eye, focus), (focus, eye)):
            hit = unreal.SystemLibrary.sphere_trace_single(world, a, b, 25.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                           unreal.DrawDebugTrace.NONE, True)
            if hit and hit.to_tuple()[3] < 0.9 * spec["dist"]:
                clear = False
                break
        if clear:
            best = (yaw, eye)
            break
    if not best:
        log(f"reframe {name}: no clear eye, keeping the staged camera")
        return
    yaw, eye = best
    cam = cams[0]
    rot = unreal.MathLibrary.find_look_at_rotation(eye, focus)
    cam.set_actor_location_and_rotation(eye, rot, False, True)
    if spec.get("fov"):
        cam.camera_component.set_field_of_view(spec["fov"])
    log(f"reframe {name}: yaw {yaw:.0f} eye {eye.x:.0f},{eye.y:.0f},{eye.z:.0f} -> focus {fx:.0f},{fy:.0f},{fz:.0f}")


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == "L_Morrowmere_v2" and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Tour:
    def __init__(self):
        sel = os.environ.get("KG_WC_SHOTS", "")
        self.todo = [] if os.environ.get("KG_WC_NOSHOTS") == "1" else [s for s in (sel.split(",") if sel and sel != "all" else POSES)]
        self.t0 = time.time()
        self.next_t = self.t0 + 14.0
        self.state = "warmup"
        self.world = None
        self.cur = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)

    def tick(self, dt):
        now = time.time()
        if now < self.next_t:
            return
        try:
            if self.state == "warmup":
                self.world = game_world()
                if not self.world:
                    if now - self.t0 > 150:
                        self.finish()
                    return
                for c in ("r.Streaming.FullyLoadUsedTextures 1", "t.MaxFPS 30", "r.MotionBlurQuality 0", "DisableAllScreenMessages"):
                    self.cmd(c)
                self.cmd("kg.WorldChore.Routes")
                self.state = "pose"
                self.next_t = now + 1.0
            elif self.state == "pose":
                if not self.todo:
                    self.finish()
                    return
                self.cur = self.todo.pop(0)
                self.cmd(f"kg.WorldChore.Pose {self.cur}")
                self.state = "aim"
                self.next_t = now + 0.5
            elif self.state == "aim":
                try:
                    reframe(self.world, self.cur)
                except Exception as e:
                    log(f"reframe {self.cur} error {e}")
                self.state = "shoot"
                self.next_t = now + 2.5
            elif self.state == "shoot":
                self.cmd(f"HighResShot {W}x{H} filename=WC_{self.cur}")
                log(f"shot {self.cur}")
                self.state = "pose"
                self.next_t = now + 2.5
        except Exception as e:
            log(f"error {e}")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        w = self.world or game_world()
        unreal.SystemLibrary.quit_game(w, None, unreal.QuitPreference.QUIT, False)


TOUR = Tour()
