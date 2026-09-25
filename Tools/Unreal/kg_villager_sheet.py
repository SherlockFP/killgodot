"""SPRINT-027a contact sheets on L_Morrowmere_v2 (run by kg_villager_sheet.ps1 in -game, KG game mode, no window).
kg.Look.Lineup spawns pose-locked villager dummies floating over the harbour basin north of the jetty head; a free
camera on the jetty shoots them. Then roles are dealt for the first-person cuff shot and revealed for the sash shot.

Shots (Saved/Screenshots/Villagers/<name>.png): lobby20_idle, archetypes_<first>_<pose>, fp_cuff, sash_reveal.
"""
import math
import os
import time

import unreal

W, H = 1600, 900
WARMUP_S = 22.0
JETTY = (2900.0, 7530.0, 260.0)           # jetty head deck, facing north (+Y) over the basin
ROW = (2900.0, 8250.0, 330.0, 270.0)      # lineup centre + yaw (faces the jetty)
ARCHETYPES = 40                           # >= FKGVillagerLookGen::Archetypes().Num() (extra slots wrap)
PART = os.environ.get("KG_SHEET_PART", "all")   # all | lineup | roles (kg_villager_sheet.ps1 -Part)


def log(m):
    unreal.log(f"KG_VILLAGER_SHEET {m}")


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
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log("started")

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)

    def pc(self):
        return unreal.GameplayStatics.get_player_controller(self.world, 0)

    def shot(self, name):
        self.cmd(f"HighResShot {W * 2}x{H * 2} filename=Villagers_{name}")
        log(f"shot {name}")

    def free_cam(self, eye, tgt, fov=60.0):
        if not self.cam:
            cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
            self.cam = cams[0] if cams else unreal.GameplayStatics.get_actor_of_class(self.world, unreal.CameraActor)
        if not self.cam:
            # No camera actor in the map: the pawn itself is the camera (kg.World.Goto + control rotation).
            self.cmd(f"kg.World.Goto {eye[0]:.0f} {eye[1]:.0f} {eye[2] - 64.0:.0f}")
            self.pc().set_control_rotation(look(eye, tgt))
            return
        self.cam.camera_component.set_editor_property("field_of_view", fov)
        self.cam.set_actor_location_and_rotation(unreal.Vector(*eye), look(eye, tgt), False, False)
        self.pc().set_view_target_with_blend(self.cam, 0.0)

    def own_cam(self):
        if self.cam:
            self.pc().set_view_target_with_blend(self.pc().get_controlled_pawn(), 0.0)

    def lineup(self, count, seed, pose, first=0):
        x, y, z, yaw = ROW
        self.cmd(f"kg.Look.Lineup {count} {seed} {pose} {x:.0f} {y:.0f} {z:.0f} {yaw:.0f} {first}")

    def frame_row(self, count):
        # Two rows of count/2 (or one row): stand back far enough for the widest row.
        per_row = max(1, (count + 1) // 2)
        width = per_row * 110.0
        dist = max(600.0, width * 0.95)
        x, y, z, _ = ROW
        self.free_cam((x, y - dist, z + 140.0), (x, y + 80.0, z + 85.0), 60.0)

    def bot_body(self):
        me = self.pc().get_controlled_pawn()
        cls = unreal.load_class(None, "/Script/KillGodot.KGCharacter")
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, cls):
            if a != me and not a.actor_has_tag("KG_Lineup"):
                return a
        return None

    def sash_cam(self):
        bot = self.bot_body()
        if not bot:
            log("no bot for the sash shot")
            return
        loc = bot.get_actor_location()
        fwd = bot.get_actor_forward_vector()
        eye = (loc.x + fwd.x * 330.0, loc.y + fwd.y * 330.0, loc.z + 60.0)
        self.free_cam(eye, (loc.x, loc.y, loc.z + 20.0), 55.0)

    def plan(self):
        s = self.steps
        s.append((0.5, lambda: [self.cmd(c) for c in ("t.MaxFPS 30", "r.MotionBlurQuality 0", "DisableAllScreenMessages",
                                                         "kg.Match.Freeze 1", "kg.Bot.AI 0", "showhud")]))   # showhud toggles AKGHUD off
        s.append((1.0, lambda: self.cmd(f"kg.World.Goto {JETTY[0]:.0f} {JETTY[1]:.0f} {JETTY[2]:.0f}")))
        if PART in ("all", "lineup"):
            # 1. the seeded 20-player lobby (acceptance 1 evidence)
            s.append((1.0, lambda: self.lineup(20, 1, "idle")))
            s.append((1.0, lambda: self.frame_row(20)))
            s.append((2.5, lambda: self.shot("lobby20_idle")))
            # 2. the catalogue: 10 archetypes per sheet, three poses each
            for first in range(0, ARCHETYPES, 10):
                for pose in ("idle", "sit", "dance"):
                    s.append((0.5, lambda f=first, p=pose: self.lineup(10, 0, p, f)))
                    s.append((1.0, lambda: self.frame_row(10)))
                    s.append((2.0, lambda f=first, p=pose: self.shot(f"archetypes_{f:02d}_{p}")))
            s.append((0.5, lambda: self.cmd("kg.Look.Lineup 0")))
        if PART in ("all", "roles"):
            # 3. first-person cuff: deal roles (a real lobby: the bots wear their seeded looks), look at my own hands
            s.append((0.5, lambda: self.own_cam()))
            s.append((0.5, lambda: self.cmd("kg.Bot.Fill 8")))
            s.append((2.0, lambda: self.cmd("kg.Match.Phase Day")))
            s.append((4.0, lambda: log("roles dealt")))
            s.append((0.5, lambda: self.pc().set_control_rotation(unreal.Rotator(roll=0.0, pitch=-16.0, yaw=90.0))))
            s.append((1.5, lambda: self.shot("fp_cuff")))
            # 4. public sash: reveal every role, shoot a bot (its seeded look + the sash)
            s.append((0.5, lambda: self.cmd("kg.Match.Reveal")))
            s.append((1.5, lambda: self.sash_cam()))
            s.append((2.0, lambda: self.shot("sash_reveal")))
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
