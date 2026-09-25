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
                self.state = "shoot"
                self.next_t = now + 3.0
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
