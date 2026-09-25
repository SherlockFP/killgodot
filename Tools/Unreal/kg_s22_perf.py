"""SPRINT-022 water perf A/B in a headless -game session (no window): frame time with the OLD water materials
(M_KG_Ocean on the sea, M_KG_PondWater / M_KG_BrookWater_v2 on the planes) vs the NEW family (MI_KG_Water_*), same
views, same session, alternating A B A B, 1920x1080, vsync off, uncapped.
  powershell -File Tools/Unreal/kg_s22_perf.ps1      -> Saved/KG_V2_WaterPerf.json
The number is the mean frame time over 4 s per view and material set (the offscreen game is GPU bound, so the delta is
the water's GPU cost); the acceptance is <= +0.5 ms.
"""
import json
import math
import time

import unreal

OUT = "D:/Kill Godot/Saved/KG_V2_WaterPerf.json"
VIEWS = {
    "quay": ((14.0, 43.0, 3.9), (26.0, 60.0, 0.0)),
    "basin": ((25.0, 38.0, 6.0), (25.0, 70.0, 0.0)),
    "open_sea": ((17.0, 98.0, 5.5), (30.0, 175.0, 0.0)),
    "koi": ((47.4, 15.2, 9.9), (51.4, 20.0, 8.2)),
    "brook": ((-58.0, 38.0, 7.5), (-68.0, 33.0, 5.2)),
}
OLD = {"sea": "/Game/KillGodot/Materials/M_KG_Ocean", "plane": "/Game/KillGodot/Materials/M_KG_PondWater"}
NEW = {"sea": "/Game/KillGodot/Materials/S22/MI_KG_Water_Sea", "Brook": "/Game/KillGodot/Materials/S22/MI_KG_Water_Brook",
       "Pond": "/Game/KillGodot/Materials/S22/MI_KG_Water_Pond", "koi": "/Game/KillGodot/Materials/S22/MI_KG_Water_Koi"}
WARM, MEASURE = 2.0, 4.0


def log(m):
    unreal.log(f"KG_PERF {m}")


def look(eye, tgt):
    dx, dy, dz = tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]
    return unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), yaw=math.degrees(math.atan2(dy, dx)))


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == __import__("os").environ.get("KG_V2_MAPNAME", "L_Morrowmere_v2") and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Perf:
    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.plan = []
        for v in VIEWS:
            for rep in range(2):
                self.plan += [(v, "old", rep), (v, "new", rep)]
        self.res = {}
        self.state = "wait"
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def water_actors(self):
        out = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.StaticMeshActor):
            c = a.static_mesh_component
            m = c.static_mesh.get_name() if c.static_mesh else ""
            lab = a.get_actor_label()
            f = str(a.get_folder_path())
            if lab == "Sea":
                out.append((c, "sea"))
            elif f.startswith("V2/Water/") or f == "V2/Brook":
                out.append((c, f.split("/")[-1]))
            elif lab == "KoiPondWater":
                out.append((c, "koi"))
        return out

    def apply(self, which):
        for c, kind in self.waters:
            if which == "old":
                path = OLD["sea"] if kind == "sea" else OLD["plane"]
            else:
                path = NEW.get(kind) or NEW["Pond"]
            mat = unreal.load_asset(path)
            if mat:
                c.set_material(0, mat)

    def tick(self, dt):
        now = time.time()
        try:
            if self.state == "wait":
                if now - self.t0 < 12.0:
                    return
                self.world = game_world()
                if not self.world:
                    return
                for cmd in ("t.MaxFPS 0", "r.VSync 0", "r.MotionBlurQuality 0", "showhud", "DisableAllScreenMessages"):
                    unreal.SystemLibrary.execute_console_command(self.world, cmd)
                self.cam = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")[0]
                self.waters = self.water_actors()
                log(f"{len(self.waters)} water actors")
                self.state = "next"
            if self.state == "next":
                if not self.plan:
                    self.finish()
                    return
                self.cur = self.plan.pop(0)
                v, which, rep = self.cur
                eye, tgt = VIEWS[v]
                self.apply(which)
                self.cam.set_actor_location_and_rotation(unreal.Vector(*(c * 100.0 for c in eye)), look(eye, tgt), False, False)
                unreal.GameplayStatics.get_player_controller(self.world, 0).set_view_target_with_blend(self.cam, 0.0)
                self.state, self.t1, self.dts = "warm", now, []
            elif self.state == "warm":
                if now - self.t1 > WARM:
                    self.state, self.t1 = "measure", now
            elif self.state == "measure":
                self.dts.append(dt)
                if now - self.t1 > MEASURE:
                    v, which, rep = self.cur
                    ms = 1000.0 * sum(self.dts) / max(1, len(self.dts))
                    self.res.setdefault(v, {}).setdefault(which, []).append(round(ms, 3))
                    log(f"{v} {which} {rep}: {ms:.2f} ms over {len(self.dts)} frames")
                    self.state = "next"
        except Exception as e:
            log(f"error {e}")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        summary = {}
        for v, d in self.res.items():
            if d.get("old") and d.get("new"):
                o, n = sum(d["old"]) / len(d["old"]), sum(d["new"]) / len(d["new"])
                summary[v] = {"old_ms": round(o, 2), "new_ms": round(n, 2), "delta_ms": round(n - o, 2)}
        worst = max((s["delta_ms"] for s in summary.values()), default=None)
        json.dump({"views": summary, "raw": self.res, "worst_delta_ms": worst, "pass": worst is not None and worst <= 0.5},
                  open(OUT, "w"), indent=1)
        log(f"done: worst delta {worst} ms -> {OUT}")
        unreal.SystemLibrary.quit_game(self.world or game_world(), None, unreal.QuitPreference.QUIT, False)


PERF = Perf()
