"""SPRINT-023 offscreen renders on L_Morrowmere_v2 (run by kg_voice_shots.ps1 in -game, KG game mode, no window):
two bots stand still; a free camera shoots a bot's face with the mouth pinned closed / open, then three frames while it
barks (kg.Bark.Bots), the HUD with the speaking marker + bubble + NEAR line (shot showui), the partner emotes
(kg.Partner.Bots highfive / rps / danceoff) from the side, and the offer prompt on the player's own HUD.

Shots (Saved/Screenshots/Voice/<name>.png): mouth_closed, mouth_open, bark_1..3, bark_ui, talking_ui,
partner_highfive, partner_rps, partner_rps_result_ui, partner_danceoff, partner_offer_ui.
"""
import math
import os
import time

import unreal

W, H = 1600, 900
WARMUP_S = 22.0


def log(m):
    unreal.log(f"KG_VOICE_SHOTS {m}")


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
        self.cam = None
        self.bots = []
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log("started")

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)

    def pc(self):
        return unreal.GameplayStatics.get_player_controller(self.world, 0)

    def shot(self, name, ui=False):
        if ui:
            self.cmd(f"shot showui filename=Voice_{name} nosuffix")
        else:
            self.cmd(f"HighResShot {W * 2}x{H * 2} filename=Voice_{name}")
        log(f"shot {name}")

    def free_cam(self, eye, tgt, fov=45.0):
        if not self.cam:
            cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
            self.cam = cams[0] if cams else unreal.GameplayStatics.get_actor_of_class(self.world, unreal.CameraActor)
        if not self.cam:
            self.cmd(f"kg.World.Goto {eye[0]:.0f} {eye[1]:.0f} {eye[2] - 64.0:.0f}")
            self.pc().set_control_rotation(look(eye, tgt))
            return
        self.cam.camera_component.set_editor_property("field_of_view", fov)
        self.cam.set_actor_location_and_rotation(unreal.Vector(*eye), look(eye, tgt), False, False)
        self.pc().set_view_target_with_blend(self.cam, 0.0)

    def own_cam(self):
        if self.cam:
            self.pc().set_view_target_with_blend(self.pc().get_controlled_pawn(), 0.0)

    def find_bots(self):
        me = self.pc().get_controlled_pawn()
        cls = unreal.load_class(None, "/Script/KillGodot.KGCharacter")
        self.bots = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, cls) if a != me]
        log(f"bots={len(self.bots)}")

    def face_cam(self, dist=115.0, fov=38.0):
        if not self.bots:
            return
        bot = self.bots[0]
        loc = bot.get_actor_location()
        fwd = bot.get_actor_forward_vector()
        head = (loc.x, loc.y, loc.z + 62.0)
        eye = (loc.x + fwd.x * dist, loc.y + fwd.y * dist, loc.z + 64.0)
        self.free_cam(eye, head, fov)

    def pair_cam(self, dist=300.0, fov=50.0):
        if len(self.bots) < 2:
            return
        a, b = self.bots[0].get_actor_location(), self.bots[1].get_actor_location()
        mid = ((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5)
        dx, dy = b.x - a.x, b.y - a.y
        n = math.hypot(dx, dy) or 1.0
        px, py = -dy / n, dx / n
        eye = (mid[0] + px * dist, mid[1] + py * dist, mid[2] + 70.0)
        self.free_cam(eye, (mid[0], mid[1], mid[2] + 20.0), fov)

    def stand_in_front(self, dist=220.0):
        """Put the player's own body in front of bot 0 so the first-person HUD shots see it talking."""
        if not self.bots:
            return
        bot = self.bots[0]
        loc = bot.get_actor_location()
        fwd = bot.get_actor_forward_vector()
        self.cmd(f"kg.World.Goto {loc.x + fwd.x * dist:.0f} {loc.y + fwd.y * dist:.0f} {loc.z:.0f}")
        self.pc().set_control_rotation(look((loc.x + fwd.x * dist, loc.y + fwd.y * dist, loc.z + 64.0), (loc.x, loc.y, loc.z + 60.0)))

    def plan(self):
        s = self.steps
        s.append((0.5, lambda: [self.cmd(c) for c in ("t.MaxFPS 30", "r.MotionBlurQuality 0", "DisableAllScreenMessages",
                                                         "kg.Match.Freeze 1", "kg.Bot.AI 0")]))
        s.append((1.0, lambda: self.cmd("kg.Bot.Add 2")))
        s.append((3.0, lambda: self.cmd("kg.Bot.AI 0")))
        s.append((1.0, lambda: self.find_bots()))
        # 1. the mouth: pinned closed / open on a bot's face
        s.append((0.5, lambda: self.face_cam()))
        s.append((0.5, lambda: self.cmd("kg.Mouth.Force 0")))
        s.append((1.5, lambda: self.shot("mouth_closed")))
        s.append((0.5, lambda: self.cmd("kg.Mouth.Force 1")))
        s.append((1.5, lambda: self.shot("mouth_open")))
        s.append((0.5, lambda: self.cmd("kg.Mouth.Force -1")))
        # 2. a real bark: three frames while the bot says "Over here!" (0.73 s, 3 syllables)
        s.append((1.0, lambda: self.cmd("kg.Bark.Bots overhere")))
        s.append((0.12, lambda: self.shot("bark_1")))
        s.append((0.18, lambda: self.shot("bark_2")))
        s.append((0.2, lambda: self.shot("bark_3")))
        # 3. HUD: stand in front of the bot in first person, bark again -> speaking marker + bubble + NEAR line
        s.append((1.5, lambda: self.own_cam()))
        s.append((0.5, lambda: self.stand_in_front()))
        s.append((1.5, lambda: self.cmd("kg.Bark.Bots meeting")))
        s.append((0.45, lambda: self.shot("bark_ui", ui=True)))
        # 4. own mic pill: synthetic tone from me
        s.append((1.5, lambda: self.cmd("kg.Voice.Tone 1")))
        s.append((1.2, lambda: self.shot("talking_ui", ui=True)))
        s.append((0.3, lambda: self.cmd("kg.Voice.Tone 0")))
        # 4b. the X (Deduction) radial held open, aimed at slot 2
        s.append((0.5, lambda: self.cmd("kg.Voice.Wheel 1 2")))
        s.append((0.6, lambda: self.shot("radial_ui", ui=True)))
        s.append((0.2, lambda: self.cmd("kg.Voice.Wheel close")))
        # 5. my own offer prompt (pill), then partner emotes between the two bots from the side
        s.append((0.5, lambda: self.cmd("kg.Partner highfive")))
        s.append((1.0, lambda: self.shot("partner_offer_ui", ui=True)))
        s.append((0.3, lambda: self.cmd("kg.Partner cancel")))
        s.append((0.5, lambda: self.cmd("kg.Partner.Bots highfive")))
        s.append((0.3, lambda: self.pair_cam()))
        s.append((0.9, lambda: self.shot("partner_highfive")))
        s.append((3.0, lambda: self.cmd("kg.Partner.Bots rps")))
        s.append((0.3, lambda: self.pair_cam()))
        s.append((1.0, lambda: self.shot("partner_rps")))
        s.append((2.3, lambda: self.shot("partner_rps_result_ui", ui=True)))
        s.append((3.5, lambda: self.cmd("kg.Partner.Bots danceoff")))
        s.append((0.3, lambda: self.pair_cam(340.0)))
        s.append((2.5, lambda: self.shot("partner_danceoff")))
        s.append((2.5, lambda: self.finish()))

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
