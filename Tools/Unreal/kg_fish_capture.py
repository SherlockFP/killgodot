"""Offscreen fishing captures on L_Morrowmere_v2 (run by kg_fish_capture.ps1 in -game, KG game mode, no window):
the player on the jetty head goes through the real fishing path with the kg.Fish.* dev verbs and a HighResShot is taken
at each beat (first-person rod, line, bobber, HUD), then a bot fishes for a third-person shot, then Madam Brine's stall.

Shots (Saved/Screenshots/Fish/<name>.png): fish_ready, fish_waiting, fish_bite, fish_fight, fish_catch, fish_tp, fish_market.
"""
import math
import time

import unreal

W, H = 1600, 900
WARMUP_S = 22.0
JETTY = (2900.0, 7530.0, 260.0)      # jetty head deck (Morrowmere v2), facing north over the basin
BOT_AT = (3180.0, 7462.0, 260.0)
STALL = (4392.0, 3241.0, 285.0)
STALL_FRONT = (-0.493, 0.870)


def log(m):
    unreal.log(f"KG_FISH_CAPTURE {m}")


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


class Capture:
    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.steps = []
        self.next_t = self.t0 + WARMUP_S
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log("started")

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)
        log(f"cmd {c}")

    def pc(self):
        return unreal.GameplayStatics.get_player_controller(self.world, 0)

    def shot(self, name):
        self.cmd(f"HighResShot {W}x{H} filename=Fish_{name}")

    def face(self, pitch, yaw):
        self.pc().set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))

    def place_bot(self):
        me = self.pc().get_controlled_pawn()
        cls = unreal.load_class(None, "/Script/KillGodot.KGCharacter")
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, cls):
            if a != me:
                a.set_actor_location_and_rotation(unreal.Vector(*BOT_AT), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0), False, True)
                ctrl = a.get_controller()
                if ctrl:
                    ctrl.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-4.0, yaw=90.0))
                log(f"bot {a.get_name()} placed")
                return True
        return False

    def free_cam(self, eye, tgt, fov=60.0):
        cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
        cam = cams[0] if cams else unreal.GameplayStatics.get_actor_of_class(self.world, unreal.CameraActor)
        if not cam:
            cam = self.world.spawn_actor(unreal.CameraActor, unreal.Vector(*eye), look(eye, tgt)) if hasattr(self.world, "spawn_actor") else None
        if not cam:
            log("no camera actor for the free shots")
            return
        cam.camera_component.set_editor_property("field_of_view", fov)
        cam.set_actor_location_and_rotation(unreal.Vector(*eye), look(eye, tgt), False, False)
        self.pc().set_view_target_with_blend(cam, 0.0)

    def plan(self):
        s = self.steps
        s.append((0.5, lambda: [self.cmd(c) for c in ("t.MaxFPS 30", "r.MotionBlurQuality 0", "DisableAllScreenMessages",
                                                         "kg.Bot.Add 1", "kg.Bot.AI 0", "kg.Match.Freeze 1")]))
        s.append((1.5, lambda: self.cmd(f"kg.World.Goto {JETTY[0]:.0f} {JETTY[1]:.0f} {JETTY[2]:.0f}")))
        s.append((1.5, lambda: self.face(-7.0, 90.0)))
        s.append((0.5, lambda: self.cmd("kg.Fish.Cast out")))
        s.append((2.5, lambda: self.shot("ready")))
        s.append((0.5, lambda: self.cmd("kg.Fish.Cast 0.62")))
        s.append((3.0, lambda: self.shot("waiting")))
        # forced bite lands 0.4 s after the verb; a mackerel's window is 0.9 s (+0.2): shoot, then strike at once
        s.append((0.55, lambda: self.cmd("kg.Fish.Bite Mackerel")))
        s.append((0.05, lambda: self.shot("bite")))
        s.append((0.8, lambda: self.cmd("kg.Fish.Hook")))
        s.append((0.05, lambda: self.shot("fight")))
        s.append((0.8, lambda: self.cmd("kg.Fish.Land")))
        s.append((0.9, lambda: self.shot("catch")))
        s.append((1.0, lambda: self.place_bot()))
        s.append((1.0, lambda: self.cmd("kg.Fish.BotCast 0.6")))
        s.append((2.5, lambda: self.free_cam((BOT_AT[0] - 330.0, BOT_AT[1] - 160.0, BOT_AT[2] + 90.0),
                                             (BOT_AT[0] + 60.0, BOT_AT[1] + 420.0, BOT_AT[2] - 40.0), 70.0)))
        s.append((1.5, lambda: self.shot("tp")))
        # standing in the arcade in front of the fish table (STALL Z is the table top, the floor is ~80 cm lower)
        eye = (STALL[0] + STALL_FRONT[0] * 330.0, STALL[1] + STALL_FRONT[1] * 330.0, STALL[2] + 80.0)
        s.append((0.5, lambda: self.free_cam(eye, (STALL[0], STALL[1], STALL[2] + 150.0), 75.0)))
        s.append((2.0, lambda: self.shot("market")))
        s.append((2.0, lambda: self.finish()))

    def tick(self, dt):
        now = time.time()
        if now < self.next_t:
            return
        try:
            if self.world is None:
                self.world = game_world()
                if not self.world:
                    if now - self.t0 > 150:
                        log("no game world")
                        self.finish()
                    return
                self.plan()
            if not self.steps:
                return
            wait, fn = self.steps.pop(0)
            fn()
            self.next_t = now + wait
        except Exception as e:
            log(f"error {e}")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        unreal.SystemLibrary.quit_game(self.world or game_world(), None, unreal.QuitPreference.QUIT, False)


CAPTURE = Capture()
