"""Offscreen shots of any level (SPRINT-022 lab + before/after sets): -game -RenderOffScreen, KG_CaptureCam tour.
  $env:KG_LAB_MAP = "L_KG_S22_Lab"; $env:KG_LAB_SHOTS = "name:x,y,z,tx,ty,tz,fov[,night];..." (metres, degrees)
  $env:KG_LAB_PREFIX = "LAB_"   (file name prefix; PNGs in Saved/Screenshots/WindowsEditor)
Driven by Tools/Unreal/kg_s22_lab_capture.ps1. Same mechanism as kg_capture_v2.py (Slate post-tick, HighResShot).
"""
import math
import os
import time

import unreal

MAP = os.environ.get("KG_LAB_MAP", "L_KG_S22_Lab")
PREFIX = os.environ.get("KG_LAB_PREFIX", "LAB_")
RES = os.environ.get("KG_LAB_RES", "1600x900")
SHOTS = []
for part in os.environ.get("KG_LAB_SHOTS", "").split(";"):
    if ":" not in part:
        continue
    n, v = part.split(":", 1)
    vals = v.split(",")
    SHOTS.append((n.strip(), [float(t) for t in vals[:7]], len(vals) > 7 and vals[7].strip() == "night"))
EXTRA = [c for c in os.environ.get("KG_LAB_CMDS", "").split(";") if c.strip()]


def log(m):
    unreal.log(f"KG_LABCAP {m}")


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == MAP and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Tour:
    def __init__(self):
        self.t0 = time.time()
        self.state, self.next_t, self.world, self.cam = "warmup", self.t0 + float(os.environ.get("KG_WARMUP", "12")), None, None
        self.todo = list(SHOTS)
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def night(self, on):
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.DirectionalLight):
            lc = a.get_component_by_class(unreal.DirectionalLightComponent)
            if not hasattr(self, "_day"):
                self._day = (a.get_actor_rotation(), lc.intensity)
            if on:
                a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-22.0, yaw=200.0), False)
                lc.set_intensity(0.9)
            else:
                a.set_actor_rotation(self._day[0], False)
                lc.set_intensity(self._day[1])

    def tick(self, dt):
        now = time.time()
        try:
            if now < self.next_t:
                return
            if self.state == "warmup":
                self.world = game_world()
                if not self.world:
                    if now - self.t0 > 150:
                        self.finish()
                    self.next_t = now + 1.0
                    return
                for c in ["r.Streaming.FullyLoadUsedTextures 1", "t.MaxFPS 30", "r.MotionBlurQuality 0",
                          "DisableAllScreenMessages"] + EXTRA:
                    unreal.SystemLibrary.execute_console_command(self.world, c)
                cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
                if not cams:
                    log("no KG_CaptureCam")
                    self.finish()
                    return
                self.cam = cams[0]
                unreal.SystemLibrary.execute_console_command(self.world, "showhud")
                self.state = "aim"
            if self.state == "aim":
                if not self.todo:
                    self.finish()
                    return
                self.cur = self.todo.pop(0)
                n, (x, y, z, tx, ty, tz, fov), nt = self.cur
                self.night(nt)
                dx, dy, dz = tx - x, ty - y, tz - z
                rot = unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), yaw=math.degrees(math.atan2(dy, dx)))
                self.cam.camera_component.set_editor_property("field_of_view", fov)
                self.cam.set_actor_location_and_rotation(unreal.Vector(x * 100, y * 100, z * 100), rot, False, False)
                unreal.GameplayStatics.get_player_controller(self.world, 0).set_view_target_with_blend(self.cam, 0.0)
                self.state, self.next_t = "shoot", now + (4.0 if nt else 1.5)
            elif self.state == "shoot":
                unreal.SystemLibrary.execute_console_command(self.world, f"HighResShot {RES} filename={PREFIX}{self.cur[0]}")
                log(f"shot {self.cur[0]}")
                self.state, self.next_t = "aim", now + 2.5
        except Exception as e:
            log(f"error {e}")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        unreal.SystemLibrary.quit_game(self.world or game_world(), None, unreal.QuitPreference.QUIT, False)


TOUR = Tour()
