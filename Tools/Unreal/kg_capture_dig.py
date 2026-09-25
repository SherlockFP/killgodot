"""Offscreen captures of the underground (well cellar, tunnel, catacombs) and of dug holes in L_Morrowmere_v2.

  powershell -File Tools/Unreal/kg_capture_dig.ps1 [shot,shot]      (no window, no sound; -game -RenderOffScreen)
Runs inside -game with the plain GameModeBase and -KGDigShots: UKGDigSubsystem generates the free-roam dig spots and
AKGDigManager::DevPrepareShots digs one spot of every look to a telling stage; unreal.KGDigManager.dev_shot_points()
returns where they are. Same mechanism as Tools/Unreal/kg_capture_v2.py (moves the level's KG_CaptureCam, HighResShot).
PNGs: Saved/Screenshots/WindowsEditor/Dig_<name>.png, moved by the .ps1 to Saved/Screenshots/Dig/<name>.png.
"""
import math
import os
import time

import unreal

W, H = 1600, 900
WARMUP_S = float(os.environ.get("KG_WARMUP", "16"))
PER_SHOT_S = 2.5

# name: (eye (x, y, z) m, target (x, y, z) m, fov)   floors are at z 1.8 m
SHOTS = {
    "cellar": ((-29.0, -16.9, 3.4), (-37.0, -23.0, 2.3), 80.0),
    "cellar_shaft": ((-33.1, -20.5, 3.3), (-33.1, -14.8, 3.0), 75.0),
    "nook": ((-37.0, -19.0, 3.3), (-41.6, -20.8, 2.2), 75.0),
    "barrel_store": ((-28.6, -16.8, 3.3), (-25.0, -24.8, 2.6), 75.0),
    "tunnel": ((-25.1, -25.5, 3.3), (-25.1, -35.0, 2.9), 75.0),
    "broken_wall": ((-31.3, -38.8, 3.4), (-33.1, -47.0, 3.3), 75.0),
    "catacomb_corridor": ((-33.13, -45.4, 3.4), (-33.13, -54.0, 3.0), 75.0),
    "crypt_hall": ((-31.6, -55.4, 3.5), (-34.8, -59.2, 2.4), 80.0),
    "crypt_stair": ((-33.13, -53.8, 3.2), (-33.13, -64.5, 5.3), 70.0),
    "ossuary": ((-41.3, -56.84, 3.3), (-45.5, -56.84, 2.2), 80.0),
    "crypt_chapel": ((-27.6, -56.84, 3.4), (-22.0, -56.84, 2.3), 80.0),
    "vault_gate": ((-39.13, -61.8, 3.3), (-39.13, -70.0, 2.6), 75.0),
    "vault": ((-39.13, -68.4, 3.5), (-39.13, -74.5, 2.3), 80.0),
    "surface_well": ((-29.5, -10.5, 10.0), (-33.13, -14.84, 9.0), 70.0),
    "surface_mausoleum": ((-30.2, -56.0, 15.8), (-33.1, -64.0, 15.2), 70.0),
}
DIG_NAMES = ["dig_grave_open", "dig_grave_half", "dig_mound", "dig_hole1", "dig_hole2", "dig_hole3", "dig_x", "dig_glint",
             "dig_chest"]


def log(m):
    unreal.log(f"KG_CAPTURE_DIG {m}")


def look(eye, tgt):
    dx, dy, dz = tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]
    return unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), yaw=math.degrees(math.atan2(dy, dx)))


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
        sel = os.environ.get("KG_SHOTS", "")
        self.sel = [s for s in sel.split(",") if s] if sel else None
        self.todo = []
        self.shots = dict(SHOTS)
        self.t0 = time.time()
        self.state = "warmup"
        self.next_t = self.t0 + WARMUP_S
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"warm-up {WARMUP_S}s")

    def setup(self):
        self.world = game_world()
        if not self.world:
            return False
        for cmd in ("r.Streaming.FullyLoadUsedTextures 1", "t.MaxFPS 30", "r.MotionBlurQuality 0", "DisableAllScreenMessages"):
            unreal.SystemLibrary.execute_console_command(self.world, cmd)
        cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
        if not cams:
            log("no KG_CaptureCam in the level")
            return False
        self.cam = cams[0]
        unreal.SystemLibrary.execute_console_command(self.world, "showhud")
        try:
            pts = unreal.KGDigManager.dev_shot_points(self.world)
        except Exception as e:  # noqa: BLE001
            log(f"dev_shot_points failed: {e}")
            pts = []
        for name, p in zip(DIG_NAMES, pts):
            if abs(p.x) + abs(p.y) < 1.0:
                continue
            x, y, z = p.x / 100.0, p.y / 100.0, p.z / 100.0
            self.shots[name] = (self.clear_eye(x, y, z), (x, y, z + 0.1), 70.0)
            log(f"dig shot {name} at {x:.1f},{y:.1f},{z:.1f}")
        self.todo = [s for s in (self.sel or list(self.shots)) if s in self.shots]
        log(f"{len(self.todo)} shots")
        return True

    def clear_eye(self, x, y, z):
        """An eye ~2.9 m from the spot, 1.75 m up, with nothing (tree trunk, stone, wall) in a 0.6 m wide tube between it
        and the spot (a thin centre ray missed the trunk that filled half of dig_grave_open)."""
        r = 2.86
        for deg in (143.5, 36.5, -143.5, -36.5, 90.0, -90.0, 180.0, 0.0):
            ex, ey = x + r * math.cos(math.radians(deg)), y + r * math.sin(math.radians(deg))
            try:
                hit = unreal.SystemLibrary.sphere_trace_single(
                    self.world, unreal.Vector(x * 100, y * 100, z * 100 + 60.0), unreal.Vector(ex * 100, ey * 100, z * 100 + 175.0),
                    30.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            except Exception as e:  # noqa: BLE001
                log(f"trace failed ({e}); default eye")
                break
            t = hit.to_tuple() if hit else None
            if not (t and t[0]):     # HitResult.blocking_hit (same use as kg_refurnish.py)
                log(f"eye at {deg:.0f} deg")
                return (ex, ey, z + 1.75)
            try:
                who = t[9].get_name() if t[9] else "?"
            except Exception:  # noqa: BLE001
                who = "?"
            log(f"eye at {deg:.0f} deg blocked by {who}")
        return (x - 2.3, y + 1.7, z + 1.75)

    def aim(self, name):
        eye, tgt, fov = self.shots[name]
        cc = self.cam.camera_component
        cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        cc.set_editor_property("field_of_view", fov)
        self.cam.set_actor_location_and_rotation(unreal.Vector(eye[0] * 100, eye[1] * 100, eye[2] * 100), look(eye, tgt), False, False)
        pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        pc.set_view_target_with_blend(self.cam, 0.0)

    def tick(self, dt):
        now = time.time()
        try:
            if self.state == "warmup":
                if now < self.next_t:
                    return
                if not self.setup():
                    if now - self.t0 > 150:
                        log("no game world - giving up")
                        self.finish()
                    return
                self.state = "aim"
                self.next_t = now
            if now < self.next_t:
                return
            if self.state == "aim":
                if not self.todo:
                    self.finish()
                    return
                self.cur = self.todo.pop(0)
                self.aim(self.cur)
                self.state = "shoot"
                self.next_t = now + 1.6    # the underground look blends in over ~0.4 s, candles settle
            elif self.state == "shoot":
                unreal.SystemLibrary.execute_console_command(self.world, f"HighResShot {W}x{H} filename=Dig_{self.cur}")
                log(f"shot {self.cur}")
                self.state = "aim"
                self.next_t = now + PER_SHOT_S
        except Exception as e:  # noqa: BLE001
            log(f"error {e}")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        unreal.SystemLibrary.quit_game(self.world or game_world(), None, unreal.QuitPreference.QUIT, False)


TOUR = Tour()
