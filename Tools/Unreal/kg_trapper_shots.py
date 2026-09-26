"""SPRINT-041 offscreen Trapper shots (run by Tools/Unreal/kg_trapper_shots.ps1 in a windowless -game session on
L_Morrowmere_v2, warmup held, bots frozen). Each scene is staged by the dev verb kg.Trapper.Stage in front of the
local player (Source/KillGodot/Abilities/KGAbilityDev.h), the camera turns 90 degrees between scenes.

Shots (Saved/Screenshots/Trapper/<tag>_<name>.png): rest (the telegraph: ajar seam, teeth tips, tongue tip, drool),
bite (mid-bite on a bot), marks (the chest after the bite), flinch (the throw-test reaction), snare (open + a caught
bot), ui (the ability bar + the Mimic targeting prompt).
Env: KG_TR_W, KG_TR_H, KG_TR_TAG.
"""
import os
import time

import unreal

W = int(os.environ.get("KG_TR_W", "1600"))
H = int(os.environ.get("KG_TR_H", "900"))
TAG = os.environ.get("KG_TR_TAG", "trapper")
WARMUP_S = 22.0


def log(m):
    unreal.log(f"KG_TRAPPER_SHOTS {m}")


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == "L_Morrowmere_v2" and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Shots:
    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.yaw = None
        # (seconds after the previous step, action)
        self.steps = [
            (0.0, lambda: self.cmd("kg.BotAI 0")),
            (0.5, lambda: self.cmd("kg.Me.God")),
            (0.5, lambda: self.turn(0)),
            (0.5, lambda: self.cmd("kg.Trapper.Stage rest")),
            (2.0, lambda: self.shot("rest")),
            (1.0, lambda: self.turn(90)),
            (0.5, lambda: self.cmd("kg.Trapper.Stage bite")),
            (0.9, lambda: self.shot("bite")),
            (6.5, lambda: self.shot("marks")),
            (1.0, lambda: self.turn(90)),
            (0.5, lambda: self.cmd("kg.Trapper.Stage flinch")),
            (0.12, lambda: self.shot("flinch")),
            (1.5, lambda: self.turn(90)),
            (0.5, lambda: self.cmd("kg.Trapper.Stage snare")),
            (1.8, lambda: self.shot("snare")),
            (1.0, lambda: self.turn(45)),   # 90 would land back on the rest chest (360 degrees)
            (0.5, lambda: self.cmd("kg.Trapper.Stage ui")),
            (1.5, lambda: self.shot("ui")),
            (3.0, lambda: self.cmd("quit")),
        ]
        self.next_t = self.t0 + WARMUP_S
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"started {W}x{H} tag={TAG}")

    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)
        log(f"cmd {c}")

    def shot(self, name):
        self.cmd(f"HighResShot {W}x{H} filename={TAG}_{name}")

    def turn(self, dyaw):
        pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        rot = pc.get_control_rotation()
        if self.yaw is None:
            self.yaw = rot.yaw
        self.yaw += dyaw
        pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-16.0, yaw=self.yaw))
        log(f"turn yaw={self.yaw:.0f}")

    def tick(self, dt):
        now = time.time()
        if now < self.next_t:
            return
        if self.world is None:
            self.world = game_world()
            if self.world is None:
                self.next_t = now + 1.0
                return
        if not self.steps:
            unreal.unregister_slate_post_tick_callback(self.handle)
            return
        wait, fn = self.steps.pop(0)
        try:
            fn()
        except Exception as e:
            log(f"step failed: {e}")
        self.next_t = now + (self.steps[0][0] if self.steps else 0.5)


_shots = Shots()
