"""Offscreen captures of L_Morrowmere_v2: the plan's sightlines S1-S7 plus overview shots, no window, no editor.

  $env:KG_SHOTS="top,S1_postcard"   # optional subset (comma separated); default = all
  & "D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "D:/Kill Godot/KillGodot.uproject"
      "/Game/KillGodot/Maps/L_Morrowmere_v2?game=/Script/Engine.GameModeBase" -game -RenderOffScreen -nosound -ResX=1600 -ResY=900 -unattended -nosplash
      -ExecCmds="py C:/path/without/spaces/kg_capture_v2.py"      (-ExecutePythonScript is editor-only)
  or simply: powershell -File Tools/Unreal/kg_capture_v2.ps1 [shot,shot]

A real game session renders frames (a commandlet SceneCapture only shows fog), so this runs inside -game with the
plain GameModeBase (no match flow, no HUD, no nightfall): a Slate post-tick callback waits for streaming, then moves the
level's KG_CaptureCam through SHOTS and runs "HighResShot WxH filename=V2_<name>" for each, then quits.
PNGs land in Saved/Screenshots/WindowsEditor/V2_<name>.png (the .ps1 moves them to Saved/Screenshots/V2).
"""
import math
import os
import time

import unreal

W, H = 1600, 900
WARMUP_S = float(os.environ.get("KG_WARMUP", "14"))
PER_SHOT_S = 2.5

# name: (eye (x, y, z) m, target (x, y, z) m, fov) ; ortho: ("ortho", (cx, cy), width m)
SHOTS = {
    "top": ("ortho", (5.0, 0.0), 270.0),
    "top_town": ("ortho", (8.0, 8.0), 130.0),
    "aerial_sea": ((40.0, 200.0, 70.0), (5.0, 0.0, 8.0), 60.0),
    "aerial_west": ((-150.0, 90.0, 55.0), (0.0, 0.0, 6.0), 60.0),
    "S1_postcard": ((14.22, 21.46, 6.7), (45.0, 150.0, 6.0), 70.0),
    "S2_pilgrim": ((26.98, 74.75, 2.9), (-5.62, -61.4, 20.0), 60.0),
    "S3_belvedere": ((-3.4, -52.16, 15.7), (20.23, 46.55, 0.0), 75.0),
    "S4_garden_axis": ((45.0, 24.3, 9.7), (45.0, 150.0, 5.0), 70.0),
    "S5_lighthouse": ((-24.0, 67.2, 3.2), (68.0, 95.0, 20.0), 75.0),
    "S6_bridge": ((-63.0, 18.2, 9.4), (-80.0, 3.5, 9.0), 75.0),
    "S7_windmill": ((55.32, -19.76, 9.7), (97.0, -62.0, 16.0), 70.0),
    "plaza": ((12.0, 16.0, 6.8), (0.0, -10.0, 9.0), 80.0),
    "plaza_east": ((0.0, 5.0, 6.8), (25.0, 0.0, 8.0), 80.0),
    "long_ope": ((-2.0, 38.5, 3.7), (-12.0, 28.0, 6.5), 80.0),
    "quay": ((-1.5, 58.0, 3.7), (20.0, 37.5, 4.0), 80.0),
    "balcony_lane": ((-30.0, -12.0, 9.7), (20.0, -22.0, 9.0), 80.0),
    "brookside": ((-60.0, 45.0, 8.0), (-85.0, 0.0, 10.0), 75.0),
    "crown": ((-40.0, -48.0, 15.7), (-5.0, -62.0, 22.0), 75.0),
    # dressing tour (Tools/Unreal/dressing/v2): eye level per district, aerials, night
    "sq_north": ((10.5, 16.5, 6.7), (3, -8, 6.5), 75.0),
    "sq_east": ((1.0, 4.0, 6.7), (21, 0, 6.5), 75.0),
    "sq_garden": ((8, -1, 6.7), (17, -9, 5.8), 75.0),
    "sq_south": ((6.5, -6.0, 6.7), (14, 21, 5.3), 75.0),
    "sq_west": ((19, 8, 6.7), (-3, 3, 7.0), 75.0),
    "hb_quay_e": ((36, 37, 3.7), (5, 49, 3.0), 75.0),
    "hb_jetty": ((26.2, 72.5, 2.9), (18, 40, 4.2), 75.0),
    "hb_mole": ((-6, 84, 4.2), (20, 60, 2.5), 75.0),
    "hb_fishmarket": ((34, 44, 2.9), (44, 33, 2.8), 75.0),
    "hb_tide_alley": ((-0.8, 31.3, 3.7), (11.6, 24.9, 4.5), 75.0),
    "hb_boathouse": ((4, 57, 2.9), (-12, 60, 3.0), 75.0),
    "st_ropewalk": ((-21, 34, 6.7), (2, 14, 6.5), 75.0),
    "st_market_street": ((42, 11.5, 7.3), (22, 9, 6.5), 75.0),
    "st_backlane_w": ((-31, 21, 6.7), (-4, -1, 6.5), 75.0),
    "st_upper_e": ((28, -23, 9.7), (56, -20, 9.5), 75.0),
    "st_wellcourt": ((-24, -9, 9.7), (-37, -15, 8.5), 75.0),
    "st_backlane_e": ((44, -4, 6.7), (20, -6, 6.5), 75.0),
    "ch_graveyard": ((-23, -49, 15.7), (-32, -63, 14.5), 75.0),
    "ch_belvedere": ((-10, -50.5, 15.9), (5, -46, 14.6), 75.0),
    "jp_garden": ((53, 23, 9.7), (44, 15, 8.5), 75.0),
    "jp_island": ((45, 127, 4.0), (45, 150, 8.0), 75.0),
    "cs_farm": ((48, -62, 11.9), (66, -76, 10.5), 75.0),
    "cs_fields": ((60, -82, 11.3), (46, -100, 10.1), 75.0),
    "cs_orchard": ((66, -10, 10.1), (82, -40, 9.3), 75.0),
    "co_headland": ((69, 42, 10.4), (68, 88, 17.6), 75.0),
    "co_keeper": ((62, 86, 14.9), (70, 95, 16.0), 75.0),
    "wl_forest": ((-100, -28, 16.0), (-112, -60, 17.6), 75.0),
    "wl_north": ((20, -80, 15.7), (0, -110, 10.7), 75.0),
    "bk_smithy": ((-86, 17, 10.1), (-97, 12, 9.3), 75.0),
    "bk_mill": ((-94, -38, 15.2), (-100, -60, 14.5), 75.0),
    "bk_beach": ((-36, 56, 4.4), (-44, 66, 1.5), 75.0),
    "bk_ford": ((-62, 30, 8.7), (-72, 36, 6.2), 75.0),
    "wl_camp": ((-73.0, -80.0, 16.2), (-80.8, -84.4, 17.3), 75.0),
    "wl_stones": ((-22.0, -99.0, 11.4), (-29.8, -105.3, 9.8), 75.0),
    "wl_cave": ((10.0, -126.0, 11.2), (15.3, -136.1, 11.6), 75.0),
    "aerial_town_s": ((20.0, 95.0, 40.0), (10.0, 5.0, 5.0), 60.0),
    "aerial_n": ((5.0, -115.0, 48.0), (10.0, 20.0, 0.0), 60.0),
    "aerial_east": ((125.0, -25.0, 45.0), (15.0, 10.0, 5.0), 60.0),
    "aerial_square": ((-14.0, 30.0, 26.0), (11.0, 2.0, 5.0), 60.0),
    # polish pass: plan 11.4 props, calm basin, clock faces, dormers, maypole
    "pol_clock": ((15.5, 1.5, 7.0), (12.6, -14.5, 12.3), 50.0),
    "pol_grand_stair": ((17.5, 36.0, 3.7), (14.2, 20.5, 6.8), 75.0),
    "pol_bridge": ((-69.0, 29.0, 11.5), (-75.4, 16.8, 7.6), 70.0),
    "pol_harbour_light": ((13.0, 90.0, 4.6), (20.85, 101.27, 4.2), 70.0),
    "pol_quay_steps": ((13.0, 58.0, 3.2), (4.0, 50.5, 0.8), 75.0),
    "pol_basin": ((25.0, 38.0, 6.0), (25.0, 70.0, 0.0), 80.0),
    "pol_dormers": ((-8.0, 8.0, 21.0), (-26.0, -14.0, 11.0), 65.0),
    "pol_maypole": ((14.5, 11.0, 6.8), (6.5, 16.0, 8.0), 75.0),
    # SPRINT-022 visual pass: garden (3 angles), water, towers, cliffs, dusk sea
    "jp_garden2": ((48.3, 12.6, 9.7), (53.5, 18.0, 8.6), 75.0),
    "jp_garden3": ((46.2, 25.0, 9.7), (52.5, 16.5, 8.4), 75.0),
    "jp_bridge": ((57.0, 14.6, 9.6), (50.0, 21.5, 8.6), 70.0),
    "wt_koi": ((47.4, 15.2, 9.9), (51.4, 20.0, 8.2), 70.0),
    "wt_brook": ((-58.0, 38.0, 7.5), (-68.0, 33.0, 5.2), 70.0),
    "wt_quay": ((14.0, 43.0, 3.9), (26.0, 60.0, 0.0), 75.0),
    "wt_sea_dusk": ((17.0, 98.0, 5.5), (30.0, 175.0, 0.0), 75.0, "dusk"),
    "tw_clock_harbour": ((26.5, 74.0, 3.0), (12.6, -14.4, 24.0), 55.0),
    "tw_clock": ((16.5, -1.0, 6.8), (12.6, -14.4, 21.0), 70.0),
    "tw_bell": ((-12.0, -44.0, 16.0), (-5.6, -61.4, 25.0), 70.0),
    "tw_lighthouse": ((38.0, 72.0, 3.0), (68.0, 95.0, 31.0), 60.0),
    "cl_basin": ((22.0, 64.0, 2.4), (52.0, 74.0, 6.0), 80.0),
    "cl_jetty": ((24.5, 71.0, 3.2), (56.0, 86.0, 7.0), 75.0),
    "cl_point": ((42.0, 124.0, 4.5), (80.0, 104.0, 8.0), 70.0),
    "lm_waterwheel": ((-72.5, 9.0, 9.5), (-80.0, 3.5, 9.5), 70.0),
    "lm_telescope": ((-2.0, -45.0, 15.9), (6.3, -48.8, 15.0), 70.0),
    "night_square": ((12.0, 16.0, 6.8), (0.0, -10.0, 9.0), 80.0, "night"),
    "night_harbour": ((-6.0, 84.0, 5.0), (25.0, 45.0, 6.0), 75.0, "night"),
    "night_town": ((20.0, 95.0, 40.0), (10.0, 5.0, 5.0), 60.0, "night"),
    "night_lighthouse": ((10.0, 75.0, 12.0), (68.0, 95.0, 25.0), 70.0, "night"),
}
NIGHT = {"pitch": -22.0, "yaw": 200.0, "lux": 0.9, "color": (0.45, 0.58, 1.0), "sky": 0.35}   # AKGGameState Night look
DUSK = {"pitch": -6.0, "yaw": 95.0, "lux": 4.0, "color": (1.0, 0.52, 0.28), "sky": 0.55}      # low sun over the sea


def log(m):
    unreal.log(f"KG_CAPTURE_V2 {m}")


def look(eye, tgt):
    dx, dy, dz = tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]
    return unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                          yaw=math.degrees(math.atan2(dy, dx)))


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == os.environ.get("KG_V2_MAPNAME", "L_Morrowmere_v2") and                     unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Tour:
    def __init__(self):
        sel = os.environ.get("KG_SHOTS", "")
        self.todo = [s for s in (sel.split(",") if sel else list(SHOTS)) if s in SHOTS]
        self.t0 = time.time()
        self.state = "warmup"
        self.next_t = self.t0 + WARMUP_S
        self.cam = None
        self.world = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"tour of {len(self.todo)} shots, warm-up {WARMUP_S}s")

    def setup(self):
        self.world = game_world()
        if not self.world:
            return False
        for cmd in ("r.Streaming.FullyLoadUsedTextures 1", "t.MaxFPS 30", "r.MotionBlurQuality 0"):
            unreal.SystemLibrary.execute_console_command(self.world, cmd)
        cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
        if not cams:
            log("no KG_CaptureCam in the level (rebuild with kg_build_village_v2.py)")
            return False
        self.cam = cams[0]
        unreal.SystemLibrary.execute_console_command(self.world, "showhud")   # toggle: once
        pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        pc.set_view_target_with_blend(self.cam, 0.0)
        return True

    def aim(self, name):
        spec = SHOTS[name]
        cc = self.cam.camera_component
        if spec[0] == "ortho":
            (cx, cy), width = spec[1], spec[2]
            cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
            cc.set_editor_property("ortho_width", width * 100.0)
            cc.set_editor_property("ortho_far_clip_plane", 80000.0)
            self.cam.set_actor_location_and_rotation(unreal.Vector(cx * 100, cy * 100, 25000.0),
                                                     unreal.Rotator(roll=0.0, pitch=-90.0, yaw=-90.0), False, False)
        else:
            eye, tgt, fov = spec[:3]
            self.set_night(spec[3] if len(spec) > 3 else False)
            cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
            cc.set_editor_property("field_of_view", fov)
            self.cam.set_actor_location_and_rotation(unreal.Vector(eye[0] * 100, eye[1] * 100, eye[2] * 100),
                                                     look(eye, tgt), False, False)
        pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        pc.set_view_target_with_blend(self.cam, 0.0)
        self.hide_ui()

    def set_night(self, night):
        """The game's Night phase look (AKGGameState::GetLook): low moonlight, dim sky; day = the level's own light.
        night may also be "dusk" (SPRINT-022: a low warm sun for the sea shots)."""
        if getattr(self, "_night", False) == night:
            return
        look = DUSK if night == "dusk" else NIGHT
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.DirectionalLight):
            lc = a.get_component_by_class(unreal.DirectionalLightComponent)
            if not hasattr(self, "_day"):
                self._day = (a.get_actor_rotation(), lc.intensity, lc.get_editor_property("light_color"))
            if night:
                a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=look["pitch"], yaw=look["yaw"]), False)
                lc.set_intensity(look["lux"])
                c = look["color"]
                lc.set_light_color(unreal.LinearColor(c[0], c[1], c[2], 1.0))
            else:
                a.set_actor_rotation(self._day[0], False)
                lc.set_intensity(self._day[1])
                lc.set_light_color(unreal.LinearColor(self._day[2].r / 255.0, self._day[2].g / 255.0, self._day[2].b / 255.0, 1.0))
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.SkyLight):
            sc = a.get_component_by_class(unreal.SkyLightComponent)
            if not hasattr(self, "_sky"):
                self._sky = sc.intensity
            sc.set_intensity(look["sky"] if night else self._sky)
            sc.recapture_sky()
        self._night = night

    def hide_ui(self):
        """AKGHUD draws on the canvas (DrawHUD): 'showhud' toggles it off; screen messages off too."""
        unreal.SystemLibrary.execute_console_command(self.world, "DisableAllScreenMessages")

    def tick(self, dt):
        now = time.time()
        try:
            if self.state == "warmup":
                if now < self.next_t:
                    return
                if not self.setup():
                    if now - self.t0 > 120:
                        log("no game world - giving up")
                        self.finish()
                    return
                self.state = "aim"
                self.next_t = now
            if now < self.next_t:
                return
            if self.state == "aim":
                if not self.todo:
                    if os.environ.get("KG_NAVCHECK", "1") == "1":
                        try:
                            self.navcheck()
                        except Exception as e:
                            log(f"navcheck error {e}")
                    self.finish()
                    return
                self.cur = self.todo.pop(0)
                self.aim(self.cur)
                self.state = "shoot"
                self.next_t = now + (4.0 if len(SHOTS[self.cur]) > 3 else 1.2)   # let the view settle (exposure, streaming)
            elif self.state == "shoot":
                # AutomationLibrary.take_high_res_screenshot is a no-op in -game; the console command works.
                unreal.SystemLibrary.execute_console_command(self.world, f"HighResShot {W}x{H} filename=V2_{self.cur}")
                log(f"shot {self.cur}")
                self.state = "aim"
                self.next_t = now + PER_SHOT_S
        except Exception as e:
            log(f"error {e}")
            self.finish()

    def navcheck(self):
        """Real navmesh paths from the fountain to every home/civic door, every chore (tower chores: the tower door)
        and every player start -> Saved/KG_V2_NavCheck.json. Needs the navmesh built into the level."""
        import json
        L = json.load(open("D:/Kill Godot/Tools/Level/morrowmere_layout_v2.json", encoding="utf-8"))

        def door(b, out=0.8):
            w, d = b["size"]
            cw = w // 2
            lx = -cw + 1 + 2 * (cw // 2) if w >= 4 else 0.0
            yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
            c, s_ = math.cos(yaw), math.sin(yaw)
            ly = -d / 2.0 - out
            return (b["at"][0] + lx * c - ly * s_, b["at"][1] + lx * s_ + ly * c, b["z"])

        targets = []
        for b in L["houses"]:
            targets.append(("door " + b["id"], door(b)))
        for k, b in L["landmarks"].items():
            if b.get("kind") in ("civic", "tower") and not b.get("open"):
                targets.append(("door " + k, door(b)))
        towers = {t["id"] for t in L["tasks"] if t.get("tower")}
        tcls = unreal.load_class(None, "/Script/KillGodot.KGTaskStation")
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, tcls):
            tid = str(a.get_editor_property("task_id"))
            if tid in towers:
                continue   # lookout floor, reached by ladder: the tower door is checked above
            p = a.get_actor_location()
            targets.append(("task " + tid, (p.x / 100.0, p.y / 100.0, p.z / 100.0)))
        for nm, pt in (("jetty root", (18.4, 39.0, 2.0)), ("jetty ramp foot", (19.4, 43.4, 1.2)),
                       ("jetty mid", (23.3, 59.2, 1.2)), ("jetty end", (27.0, 74.8, 1.2)), ("jetty head w", (22.5, 76.8, 1.2)),
                       ("fish pier mid", (35.3, 48.3, 1.2)), ("mole mid", (-2.25, 88.88, 2.5)),
                       ("fish ramp top", (40.9, 38.9, 2.0)), ("fish ramp mid", (39.4, 41.4, 1.6)),
                       ("fish ramp foot", (38.4, 43.1, 1.25)), ("fish pier start", (37.5, 44.6, 1.2))):
            targets.append(("probe " + nm, pt))
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.PlayerStart):
            p = a.get_actor_location()
            targets.append(("start " + a.get_name(), (p.x / 100.0, p.y / 100.0, p.z / 100.0 - 1.0)))
        f = L["landmarks"]["fountain"]
        start = unreal.Vector(f["at"][0] * 100.0 + 250.0, f["at"][1] * 100.0, f["z"] * 100.0 + 60.0)
        res, bad = {}, []
        for name, (x, y, z) in targets:
            zz = (z if z is not None else 20.0) * 100.0 + 60.0
            end = unreal.Vector(x * 100.0, y * 100.0, zz)
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(self.world, start, end)
            ok = bool(path and path.is_valid() and not path.is_partial())
            length = path.get_path_length() / 100.0 if path else -1.0
            res[name] = {"ok": ok, "len_m": round(length, 1), "at": [round(x, 2), round(y, 2), round(zz / 100.0, 2)]}
            if not ok:
                bad.append(name)
        out = {"targets": len(targets), "reachable": len(targets) - len(bad), "unreachable": bad, "paths": res}
        json.dump(out, open("D:/Kill Godot/Saved/KG_V2_NavCheck.json", "w"), indent=1)
        log(f"navcheck {out['reachable']}/{out['targets']} reachable; unreachable: {bad}")

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        w = self.world or game_world()
        unreal.SystemLibrary.quit_game(w, None, unreal.QuitPreference.QUIT, False)


TOUR = Tour()
