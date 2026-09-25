"""Brand / key-art render tour of L_Morrowmere_v2 (windowless, -game -RenderOffScreen; see kg_brand_capture.ps1).

Reuses Tools/Unreal/kg_capture_v2.py (the v2 render tour) unchanged: its source is exec'd, the auto-start line is
dropped, and a Tour subclass adds
  - per-shot lighting looks: day (level default), golden, dusk, sunset (backlit), night (the game's Night look),
    each optionally with its own sun yaw/pitch,
  - runtime-only props for a shot (a Quaternius villager with a looping animation) - nothing is saved to the level,
  - 2560x1440 HighResShots named BR_<shot>.png (the .ps1 moves them to Saved/Screenshots/Brand/).
"""
import math
import os

import unreal

BASE = "D:/Kill Godot/Tools/Unreal/kg_capture_v2.py"
_src = open(BASE, encoding="utf-8").read()
_src = _src.replace("TOUR = Tour()", "").replace("filename=V2_", "filename=BR_")
exec(compile(_src, BASE, "exec"), globals())

W, H = int(os.environ.get("KG_BRAND_W", "2560")), int(os.environ.get("KG_BRAND_H", "1440"))
AXIS_SEAWARD_YAW = math.degrees(math.atan2(0.9725, 0.2328))   # 76.5 deg: the processional axis toward the island

# Lighting looks: multipliers on the level's day values. sun = (pitch, yaw) or None (keep the level's sun direction).
LOOKS = {
    "day": {"lux": 1.0, "color": None, "sky": 1.0, "sun": None},
    "golden": {"lux": 0.85, "color": (1.0, 0.72, 0.45), "sky": 0.85, "sun": (-11.0, None)},
    "dusk": {"lux": 0.32, "color": (1.0, 0.50, 0.28), "sky": 0.55, "sun": (-3.5, None)},
    "sunset": {"lux": 0.55, "color": (1.0, 0.55, 0.30), "sky": 0.45, "sun": (-4.0, None)},
    "night": {"lux": None, "color": None, "sky": None, "sun": None},   # uses NIGHT from kg_capture_v2
}

VILLAGER_M = "/Game/KillGodot/Characters/Villager/SK_KG_Villager_M"
VILLAGER_F = "/Game/KillGodot/Characters/Villager/SK_KG_Villager_F"
ANIM = "/Game/KillGodot/Characters/Villager/Anims/"

# name: {eye, tgt, fov, look, sun_yaw (deg the light travels toward), props}
BRAND = {
    # --- key art -------------------------------------------------------------------------------------------------
    "br_dusk_harbour": {"eye": (-6.0, 84.0, 5.0), "tgt": (25.0, 45.0, 6.5), "fov": 72.0, "look": "dusk",
                        "sun_yaw": -150.0},
    "br_dusk_harbour_b": {"eye": (29.0, 77.0, 2.9), "tgt": (10.0, 22.0, 9.0), "fov": 68.0, "look": "dusk",
                          "sun_yaw": -120.0},
    "br_square": {"eye": (15.0, 17.5, 6.9), "tgt": (4.0, -9.0, 9.5), "fov": 74.0, "look": "golden", "sun_yaw": -60.0},
    "br_square_dusk": {"eye": (15.0, 17.5, 6.9), "tgt": (4.0, -9.0, 9.5), "fov": 74.0, "look": "dusk", "sun_yaw": -60.0},
    "br_belvedere": {"eye": (-4.6, -49.6, 15.0), "tgt": (4.0, -10.0, 15.6), "fov": 52.0, "look": "sunset",
                     "sun_yaw": AXIS_SEAWARD_YAW + 180.0,
                     "props": [{"mesh": VILLAGER_M, "anim": ANIM + "A_KG_Idle_Loop", "at": (-2.1, -45.0, 14.0),
                                "face": AXIS_SEAWARD_YAW + 6.0}]},
    "br_belvedere_b": {"eye": (-3.6, -47.8, 14.6), "tgt": (3.0, -10.0, 16.2), "fov": 60.0, "look": "sunset",
                       "sun_yaw": AXIS_SEAWARD_YAW + 180.0,
                       "props": [{"mesh": VILLAGER_M, "anim": ANIM + "A_KG_Idle_Loop", "at": (-2.1, -45.0, 14.0),
                                  "face": AXIS_SEAWARD_YAW + 6.0}]},
    "br_lighthouse_night": {"eye": (18.0, 99.0, 4.2), "tgt": (68.0, 95.0, 21.0), "fov": 60.0, "look": "night"},
    "br_lighthouse_night_b": {"eye": (-24.0, 67.2, 3.2), "tgt": (68.0, 95.0, 22.0), "fov": 48.0, "look": "night"},
    "br_cove_dusk": {"eye": (40.0, 200.0, 70.0), "tgt": (5.0, 0.0, 8.0), "fov": 55.0, "look": "dusk", "sun_yaw": 60.0},
    "br_town_dusk": {"eye": (20.0, 95.0, 40.0), "tgt": (10.0, 5.0, 5.0), "fov": 60.0, "look": "dusk", "sun_yaw": -120.0},
    # --- loading screens (one per major district) ------------------------------------------------------------
    "ld_harbour": {"eye": (36.0, 37.0, 3.7), "tgt": (5.0, 49.0, 3.0), "fov": 75.0, "look": "golden", "sun_yaw": 200.0},
    "ld_heart": {"eye": (19.0, 8.0, 6.8), "tgt": (-3.0, 3.0, 7.5), "fov": 75.0, "look": "golden", "sun_yaw": -30.0},
    "ld_crown": {"eye": (-23.0, -49.0, 15.7), "tgt": (-32.0, -63.0, 14.8), "fov": 75.0, "look": "golden"},
    "ld_sakura": {"eye": (45.0, 24.6, 9.8), "tgt": (45.0, 150.0, 5.0), "fov": 72.0, "look": "golden", "sun_yaw": -100.0},
    "ld_sakura_b": {"eye": (53.0, 23.0, 9.7), "tgt": (44.0, 15.0, 8.5), "fov": 75.0, "look": "golden"},
    "ld_brookside": {"eye": (-63.0, 18.2, 9.4), "tgt": (-80.0, 3.5, 9.0), "fov": 75.0, "look": "golden"},
    "ld_lighthouse": {"eye": (69.0, 42.0, 10.4), "tgt": (68.0, 88.0, 18.5), "fov": 75.0, "look": "golden",
                      "sun_yaw": 160.0},
}
for _k, _v in BRAND.items():
    SHOTS[_k] = (_v["eye"], _v["tgt"], _v["fov"], _v["look"])   # 4-tuple -> the base tour waits 4 s to settle


class BrandTour(Tour):
    def __init__(self):
        sel = os.environ.get("KG_SHOTS", "")
        os.environ["KG_SHOTS"] = ",".join(s for s in (sel.split(",") if sel else list(BRAND)) if s in BRAND)
        self.props = []
        self.logged = False
        super().__init__()

    # -- lighting ------------------------------------------------------------------------------------------------
    def capture_day(self):
        if hasattr(self, "_bday"):
            return
        suns = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.DirectionalLight):
            lc = a.get_component_by_class(unreal.DirectionalLightComponent)
            suns.append((a, lc, a.get_actor_rotation(), lc.intensity, lc.get_editor_property("light_color")))
        skies = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.SkyLight):
            sc = a.get_component_by_class(unreal.SkyLightComponent)
            skies.append((sc, sc.intensity))
        self._bday = (suns, skies)
        for a, lc, rot, lux, col in suns:
            log(f"day sun {a.get_name()} rot={rot} lux={lux} color={col}")
        for sc, i in skies:
            log(f"day sky intensity={i}")
        for cls in ("SkyAtmosphere", "ExponentialHeightFog", "VolumetricCloud", "PostProcessVolume"):
            c = getattr(unreal, cls, None)
            if c:
                log(f"{cls}: {len(unreal.GameplayStatics.get_all_actors_of_class(self.world, c))}")

    def apply_look(self, spec):
        self.capture_day()
        look = spec["look"]
        suns, skies = self._bday
        if look == "night":
            self._night = False
            self.set_night(True)
            return
        self._night = False
        L = LOOKS[look]
        for a, lc, rot, lux, col in suns:
            pitch, yaw = rot.pitch, rot.yaw
            if L["sun"]:
                pitch = L["sun"][0]
            if spec.get("sun_yaw") is not None:
                yaw = spec["sun_yaw"]
            if spec.get("sun_pitch") is not None:
                pitch = spec["sun_pitch"]
            a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw), False)
            lc.set_intensity(lux * L["lux"])
            if L["color"]:
                c = L["color"]
                lc.set_light_color(unreal.LinearColor(c[0], c[1], c[2], 1.0))
            else:
                lc.set_light_color(unreal.LinearColor(col.r / 255.0, col.g / 255.0, col.b / 255.0, 1.0))
        for sc, i in skies:
            sc.set_intensity(i * L["sky"])
            sc.recapture_sky()

    # -- runtime props ---------------------------------------------------------------------------------------------
    def clear_props(self):
        for a in self.props:
            try:
                a.destroy_component(a)
            except Exception:
                pass
        self.props = []

    def spawn_props(self, spec):
        for p in spec.get("props", []):
            x, y, z = p["at"]
            xf = unreal.Transform(location=unreal.Vector(x * 100.0, y * 100.0, z * 100.0),
                                  rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=p["face"] - 90.0))
            # Python in -game cannot spawn actors (no deferred-spawn binding), so the villager is an unattached
            # SkeletalMeshComponent added to the capture camera actor (AddComponentByClass, manual attachment):
            # it keeps its own world transform while the camera moves. Runtime only, never saved.
            try:
                comp = self.cam.add_component_by_class(unreal.SkeletalMeshComponent, True, unreal.Transform(), False)
                comp.set_world_transform(xf, False, False)
            except Exception as e:
                log(f"spawn failed {e}")
                continue
            a = comp
            mesh = unreal.load_asset(p["mesh"])
            try:
                comp.set_skinned_asset_and_update(mesh, True)
            except Exception:
                comp.set_skeletal_mesh_asset(mesh)
            anim = unreal.load_asset(p["anim"]) if p.get("anim") else None
            if anim:
                comp.play_animation(anim, True)
            self.props.append(a)
            log(f"prop {p['mesh'].rsplit('/', 1)[-1]} at {p['at']} face {p['face']:.1f}")

    def aim(self, name):
        spec = BRAND[name]
        cc = self.cam.camera_component
        self.apply_look(spec)
        self.clear_props()
        self.spawn_props(spec)
        cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        cc.set_editor_property("field_of_view", spec["fov"])
        eye, tgt = spec["eye"], spec["tgt"]
        self.cam.set_actor_location_and_rotation(unreal.Vector(eye[0] * 100, eye[1] * 100, eye[2] * 100),
                                                 look(eye, tgt), False, False)
        pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        pc.set_view_target_with_blend(self.cam, 0.0)
        self.hide_ui()


TOUR = BrandTour()
