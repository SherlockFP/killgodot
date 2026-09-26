"""Storm Manor offscreen session: the render tour (KG_SM_MODE=shots) or the probe (KG_SM_MODE=probe). No window.

  powershell -File Tools/Unreal/kg_capture_stormmanor.ps1                 # shots -> Saved/Screenshots/SM/<name>.png
  powershell -File Tools/Unreal/kg_capture_stormmanor.ps1 -Mode probe     # -> Saved/KG_SM_Probe.json (+ routes)

Runs inside -game with the plain GameModeBase (no match flow, no HUD), started with -ExecCmds="py <copy of this file>";
a Slate post-tick callback waits for the world, works, then quits (same pattern as kg_capture_v2.py).
Probe = the SPRINT-018 checks that need real collision and the real navmesh:
  * nav: navmesh paths from the meeting table to every room (its KG_BotSpot), every chore spot (its stand point for
    the ladder chore), every player start, every secret-passage end, both sides of every door
  * placement: every dressing prop of Saved/KG_SM_Props.json (the builder's dump) gets 5 Visibility traces under its
    bottom face (itself skipped, HISM instances by their transform): floating = no support within 8 cm under any
    sample; sunk = bottom more than 6 cm under its floor top (analytic, from the same dump)
  * kg.WorldChore.Routes: every manor chore x variant on the navmesh (Saved/KG_WorldChoreRoutes.json)
"""
import json
import math
import os
import time

import unreal

MODE = os.environ.get("KG_SM_MODE", "shots")
W, H = 1600, 900
WARMUP_S = float(os.environ.get("KG_WARMUP", "16"))
PER_SHOT_S = 2.5
ROOT = "D:/Kill Godot"
Z0 = 8.0         # m (stormmanor_geo.Z0; SPRINT-040 raised it from 5 m for the second basement)
ZSHIFT = Z0 - 5.0   # the SHOTS table below keeps the SPRINT-018 world heights (Z0 = 5); aim() adds this
FLOOR_REL = {"C2": -6, "C": -3, "F0": 0, "F1": 3, "F2": 6, "F3": 9}

# name: (eye (x, y, z) m, target (x, y, z) m, fov); z relative to the world (floor tops: C 2, F0 5, F1 8, F2 11, F3 14)
SHOTS = {
    "aerial_se": ((62.0, 78.0, 44.0), (0.0, 0.0, 6.0), 55.0),
    "aerial_nw": ((-70.0, -60.0, 40.0), (0.0, 5.0, 6.0), 55.0),
    "top": ("ortho", (0.0, 3.0), 185.0),
    "courtyard": ((24.0, 31.0, 6.8), (0.0, 8.0, 8.0), 75.0),
    "great_hall": ((5.5, -1.0, 7.2), (-1.0, -15.0, 6.0), 80.0),
    "great_hall_gallery": ((-8.0, -18.5, 9.8), (2.0, -6.0, 5.5), 80.0),
    "vestibule": ((8.5, 9.0, 6.7), (-6.0, 3.0, 6.0), 80.0),
    "dining": ((-11.0, -7.0, 6.7), (-19.0, -17.0, 5.6), 80.0),
    "kitchen": ((-23.0, -9.0, 6.7), (-31.0, -21.0, 5.6), 80.0),
    "library": ((24.0, -11.0, 6.7), (31.0, -21.0, 6.0), 80.0),
    "chapel": ((16.0, -11.0, 6.9), (16.0, -19.5, 6.2), 80.0),
    "ballroom": ((11.0, 5.0, 6.8), (30.0, -7.0, 6.0), 80.0),
    "billiard": ((11.0, 5.0, 9.7), (18.0, -4.0, 8.6), 80.0),
    "nursery": ((-9.0, 7.0, 9.7), (6.0, 2.0, 8.6), 80.0),
    "master": ((23.0, -13.0, 9.7), (32.0, -21.0, 8.6), 80.0),
    "blue_room": ((-23.0, -13.0, 9.7), (-32.0, -21.0, 8.6), 80.0),
    "study": ((21.0, -13.0, 9.7), (12.0, -21.0, 8.6), 80.0),
    "attic": ((-11.5, -5.5, 12.7), (-30.0, -20.0, 11.5), 80.0),
    "clock_room": ((9.0, -11.0, 12.7), (-5.0, -19.5, 12.0), 80.0),
    "secret_library": ((28.0, -13.0, 6.7), (22.5, -16.0, 6.2), 70.0),
    "secret_nursery": ((0.0, 3.0, 9.7), (8.5, 7.3, 8.8), 70.0),
    "cellar_wine": ((-19.0, -1.0, 3.7), (-30.0, -12.0, 2.8), 80.0),
    "cellar_cistern": ((-5.0, -3.0, 3.8), (10.0, -15.0, 2.6), 80.0),
    "spark_room": ((-7.0, -5.0, 3.7), (-14.0, -15.0, 2.8), 80.0),
    "tunnel": ((-28.0, 2.0, 3.6), (-28.0, 30.0, 3.0), 80.0),
    "boathouse": ((-10.0, 39.0, 3.8), (6.0, 48.0, 2.6), 80.0),
    "greenhouse": ((35.5, 1.5, 6.8), (46.0, 18.0, 5.8), 80.0),
    "graveyard": ((-35.5, 6.0, 7.2), (-48.0, 22.0, 6.0), 75.0),
    "tower_top": ((24.0, -28.0, 16.5), (30.0, -20.0, 14.5), 70.0),
    "sea_front": ((0.0, 95.0, 9.0), (0.0, 20.0, 9.0), 55.0),
    # ---- SPRINT-040 (same height convention: floor tops C2 -1, C 2, F0 5, F1 8, F2 11)
    "aerial_ne": ((72.0, -78.0, 46.0), (0.0, -14.0, 6.0), 55.0),
    "aerial_w": ((-92.0, -10.0, 34.0), (-10.0, -20.0, 6.0), 55.0),
    "staircase_hall": ((0.0, -25.2, 7.2), (0.0, -38.0, 7.4), 80.0),
    "long_gallery": ((-33.0, -42.0, 6.6), (20.0, -42.0, 6.3), 70.0),
    "music_room": ((-25.0, -25.0, 6.9), (-31.0, -37.0, 5.6), 80.0),
    "green_salon": ((-17.0, -25.0, 6.9), (-23.0, -39.0, 5.6), 80.0),
    "blue_salon": ((-9.0, -25.0, 6.9), (-15.0, -39.0, 5.6), 80.0),
    "yellow_salon": ((9.0, -25.0, 6.9), (15.0, -39.0, 5.6), 80.0),
    "card_room": ((23.2, -25.0, 6.9), (17.0, -39.0, 5.6), 80.0),
    "trophy_room": ((25.0, -25.0, 6.9), (33.0, -39.0, 5.6), 80.0),
    "servants_hall": ((-44.8, -34.8, 6.9), (-53.0, -43.0, 5.6), 80.0),
    "housekeeper": ((-44.8, -24.8, 6.9), (-53.0, -33.0, 5.6), 80.0),
    "boiler_room": ((-38.8, -24.8, 6.9), (-43.0, -33.0, 5.6), 80.0),
    "east_hall": ((36.0, -1.0, 6.7), (36.0, -43.0, 6.2), 70.0),
    "smoking_room": ((45.2, -34.8, 6.9), (39.0, -43.0, 5.6), 80.0),
    "gun_room": ((46.8, -34.8, 6.9), (53.0, -43.0, 5.6), 80.0),
    "map_room": ((45.2, -24.8, 6.9), (39.0, -33.0, 5.6), 80.0),
    "games_room": ((46.8, -24.8, 6.9), (53.0, -33.0, 5.6), 80.0),
    "orangery": ((38.8, -10.8, 6.9), (53.0, -23.0, 5.6), 80.0),
    "morning_room": ((38.8, -0.8, 6.9), (53.0, -9.0, 5.6), 80.0),
    "grand_landing": ((0.0, -25.2, 10.2), (0.0, -39.0, 8.8), 80.0),
    "picture_gallery": ((-38.8, -24.8, 9.9), (-53.0, -43.0, 8.6), 80.0),
    "lilac_room": ((-24.8, -24.8, 9.9), (-33.0, -39.0, 8.6), 80.0),
    "sewing_room": ((-8.8, -24.8, 9.9), (-23.0, -39.0, 8.6), 80.0),
    "chinese_room": ((8.8, -24.8, 9.9), (23.0, -39.0, 8.6), 80.0),
    "dressing_room": ((24.8, -24.8, 9.9), (33.0, -39.0, 8.6), 80.0),
    "gold_room": ((46.8, -34.8, 9.9), (53.0, -43.0, 8.6), 80.0),
    "ivory_room": ((46.8, -24.8, 9.9), (53.0, -33.0, 8.6), 80.0),
    "guest_bath": ((38.8, -16.8, 9.9), (53.0, -23.0, 8.6), 80.0),
    "wine_catacombs": ((-22.8, -0.8, 0.7), (-33.0, -15.0, -0.2), 80.0),
    "vault": ((-6.8, -4.8, 0.7), (-20.0, -15.0, -0.2), 80.0),
    "ossuary": ((5.2, -4.8, 0.7), (-5.0, -15.0, -0.2), 80.0),
    # the secrets (S7 fireplace, S9 crawlway, S8 crypt, S10 observatory)
    "secret_fireplace": ((21.0, -30.5, 6.4), (16.6, -27.0, 5.8), 70.0),
    "secret_crawlway": ((-47.5, -40.0, 6.5), (-53.4, -36.0, 5.8), 70.0),
    "secret_crypt": ((10.8, -10.8, 0.8), (21.0, -19.0, -0.2), 80.0),
    "secret_observatory": ((46.8, -36.8, 12.7), (53.0, -43.0, 11.8), 80.0),
    # the traps (a 4th item = console command run before the shot + seconds to wait)
    "trap_trapdoor": ((-20.5, -14.0, 7.2), (-20.5, -8.5, 5.0), 70.0),
    "trap_chandelier": ((4.0, -26.0, 6.2), (0.0, -35.0, 6.8), 75.0),
    "trap_chandelier_fall": ((4.0, -26.0, 6.2), (0.0, -35.0, 5.6), 75.0, ("kg.Trap.Fire T4", 3.6)),
    "trap_portrait": ((0.0, -40.4, 6.7), (0.0, -43.6, 6.5), 70.0),
    "trap_ballroom": ((14.0, 4.0, 6.6), (24.0, -2.0, 7.2), 75.0),
}


def log(m):
    unreal.log(f"KG_SM_CAPTURE {m}")


def look(eye, tgt):
    dx, dy, dz = tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]
    return unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                          yaw=math.degrees(math.atan2(dy, dx)))


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == "L_StormManor" and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


class Session:
    def __init__(self):
        sel = os.environ.get("KG_SHOTS", "")
        self.todo = [s for s in (sel.split(",") if sel else list(SHOTS)) if s in SHOTS] if MODE == "shots" else []
        self.t0 = time.time()
        self.state = "warmup"
        self.next_t = self.t0 + WARMUP_S
        self.cam = None
        self.world = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"mode {MODE}, {len(self.todo)} shots, warm-up {WARMUP_S}s")

    def setup(self):
        self.world = game_world()
        if not self.world:
            return False
        if MODE == "shots":
            for cmd in ("r.Streaming.FullyLoadUsedTextures 1", "t.MaxFPS 30", "r.MotionBlurQuality 0"):
                unreal.SystemLibrary.execute_console_command(self.world, cmd)
            cams = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_CaptureCam")
            if not cams:
                log("no KG_CaptureCam")
                return False
            self.cam = cams[0]
            unreal.SystemLibrary.execute_console_command(self.world, "showhud")
            unreal.SystemLibrary.execute_console_command(self.world, "DisableAllScreenMessages")
        return True

    def aim(self, name):
        spec = SHOTS[name]
        cc = self.cam.camera_component
        if spec[0] == "ortho":
            (cx, cy), width = spec[1], spec[2]
            cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
            cc.set_editor_property("ortho_width", width * 100.0)
            cc.set_editor_property("ortho_far_clip_plane", 80000.0)
            self.cam.set_actor_location_and_rotation(unreal.Vector(cx * 100, cy * 100, 20000.0),
                                                     unreal.Rotator(roll=0.0, pitch=-90.0, yaw=-90.0), False, False)
        else:
            eye, tgt, fov = spec[:3]
            eye = (eye[0], eye[1], eye[2] + ZSHIFT)
            tgt = (tgt[0], tgt[1], tgt[2] + ZSHIFT)
            cc.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
            cc.set_editor_property("field_of_view", fov)
            self.cam.set_actor_location_and_rotation(unreal.Vector(eye[0] * 100, eye[1] * 100, eye[2] * 100),
                                                     look(eye, tgt), False, False)
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
                self.state = "aim" if MODE == "shots" else "probe"
                self.next_t = now
            if now < self.next_t:
                return
            if self.state == "probe":
                self.probe()
                self.state = "routes"
                self.next_t = now + 1.0
                return
            if self.state == "routes":
                unreal.SystemLibrary.execute_console_command(self.world, "kg.WorldChore.Routes")
                self.state = "quit"
                self.next_t = now + 8.0
                return
            if self.state == "quit":
                self.finish()
                return
            if self.state == "aim":
                if not self.todo:
                    self.finish()
                    return
                self.cur = self.todo.pop(0)
                self.aim(self.cur)
                self.state = "shoot"
                self.next_t = now + 1.6
                pre = SHOTS[self.cur][3] if len(SHOTS[self.cur]) > 3 else None
                if pre:         # SPRINT-040: e.g. fire a trap, then shoot while it acts
                    unreal.SystemLibrary.execute_console_command(self.world, pre[0])
                    log(f"pre {self.cur}: {pre[0]}")
                    self.next_t = now + pre[1]
            elif self.state == "shoot":
                unreal.SystemLibrary.execute_console_command(self.world, f"HighResShot {W}x{H} filename=SM_{self.cur}")
                log(f"shot {self.cur}")
                self.state = "aim"
                self.next_t = now + PER_SHOT_S
        except Exception:
            import traceback
            log("error " + traceback.format_exc())
            self.finish()

    # ------------------------------------------------------------------------------------------------ probe
    def probe(self):
        out = {"nav": self.navcheck(), "placement": self.placement()}
        json.dump(out, open(f"{ROOT}/Saved/KG_SM_Probe.json", "w"), indent=1)
        log(f"probe: nav {out['nav']['reachable']}/{out['nav']['targets']} reachable, placement "
            f"{out['placement']['checked']} checked, floating {len(out['placement']['floating'])}, "
            f"sunk {len(out['placement']['sunk'])}")

    def navcheck(self):
        w = self.world
        L = json.load(open(f"{ROOT}/Tools/Level/stormmanor_world_chores.resolved.json", encoding="utf-8"))
        rep = json.load(open(f"{ROOT}/Saved/KG_SM_BuildReport.json"))
        targets = []
        for a in unreal.GameplayStatics.get_all_actors_with_tag(w, "KG_BotSpot"):
            p = a.get_actor_location()
            targets.append(("room " + a.get_actor_label()[8:] if hasattr(a, "get_actor_label") else "room", p))
        lay = json.load(open(f"{ROOT}/Tools/Level/stormmanor_layout.json", encoding="utf-8"))
        secret_rooms = {r["id"] for r in lay["rooms"] if r.get("secret")}
        secret_ends = {f"{s['id']}_{k}" for s in lay["secrets"] for k in ("a", "b") if s[k] in secret_rooms}
        for an in L["anchors"]:
            if an.get("room") in secret_rooms:
                continue        # SPRINT-040: the crypt / observatory are reached through their passage only
            st = an.get("stand")
            if st:
                targets.append(("chore " + an["id"] + " (stand)", unreal.Vector(st[0] * 100, st[1] * 100, st[2] * 100 + 60)))
            else:
                targets.append(("chore " + an["id"], unreal.Vector(an["at"][0] * 100, an["at"][1] * 100, an["z"] * 100 + 60)))
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart):
            targets.append(("start " + a.get_name(), a.get_actor_location()))
        pcls = unreal.load_class(None, "/Script/KillGodot.KGPassage")
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, pcls):
            if str(a.get_editor_property("passage_id")) in secret_ends:
                continue
            p = a.get_actor_location()
            f = a.get_actor_forward_vector()
            targets.append(("secret " + str(a.get_editor_property("passage_id")),
                            unreal.Vector(p.x + f.x * 130.0, p.y + f.y * 130.0, p.z + 60.0)))
        for d in rep.get("doors", []):
            for sgn in (-1, 1):
                targets.append((f"door {d['id']} {'+' if sgn > 0 else '-'}",
                                unreal.Vector(d["x"] + d["nx"] * sgn * 90.0, d["y"] + d["ny"] * sgn * 90.0,
                                              (Z0 + FLOOR_REL[d["fid"]]) * 100 + 60)))
        start = unreal.Vector(-400.0, -900.0, Z0 * 100.0 + 60.0)
        res, bad = {}, []
        for name, p in targets:
            # like kg.WorldChore.Routes (NavPoint): the nearest navmesh within 1.5 m / 2.5 m up-down of the spot
            q = unreal.NavigationSystemV1.project_point_to_navigation(w, p, None, None, unreal.Vector(150.0, 150.0, 90.0))
            if q is None:   # same floor first (a roof deck 2.4 m up must not win), then the routes' wider box
                q = unreal.NavigationSystemV1.project_point_to_navigation(w, p, None, None, unreal.Vector(150.0, 150.0, 250.0))
            p = q if q is not None else p
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(w, start, p)
            ok = bool(path and path.is_valid() and not path.is_partial())
            length = path.get_path_length() / 100.0 if path else -1.0
            res[name] = {"ok": ok, "len_m": round(length, 1), "at": [round(p.x / 100, 2), round(p.y / 100, 2), round(p.z / 100, 2)]}
            if not ok:
                bad.append(name)
        return {"targets": len(targets), "reachable": len(targets) - len(bad), "unreachable": bad, "paths": res}

    def placement(self):
        w = self.world
        props = json.load(open(f"{ROOT}/Saved/KG_SM_Props.json"))
        hism = {}
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
            for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
                hism[comp.get_path_name()] = comp
        floating, sunk = [], []
        checked = 0
        for p in props:
            b = p["b"]
            s = p["s"]
            bottom = p["z"] + b[2] * s[2]
            if bottom < p["floor"] - 6.0 and p.get("floor_prop"):
                sunk.append({"m": p["m"], "at": [p["x"], p["y"], round(bottom, 1)], "floor": p["floor"]})
            if not p.get("check"):
                continue
            checked += 1
            c, sn = math.cos(math.radians(p["yaw"])), math.sin(math.radians(p["yaw"]))
            gaps = []
            for fu, fv in ((0.5, 0.5), (0.2, 0.2), (0.8, 0.2), (0.2, 0.8), (0.8, 0.8)):
                u = (b[0] + (b[3] - b[0]) * fu) * s[0]
                v = (b[1] + (b[4] - b[1]) * fv) * s[1]
                x = p["x"] + u * c - v * sn
                y = p["y"] + u * sn + v * c
                # start just under the prop: a trace started inside its own collision stopped there (the first
                # blocking hit ends a multi trace), which read as "floating" for 385 props in the first round
                # clutter without collision (a jar on a shelf board) is traced against the visual triangles from just
                # above its bottom, so the board it stands on counts even where the shelf's simple box is coarser
                solid = p.get("collide", True)
                hits = unreal.SystemLibrary.line_trace_multi(
                    w, unreal.Vector(x, y, bottom + (-0.3 if solid else 2.0)), unreal.Vector(x, y, bottom - 150.0),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, not solid, [], unreal.DrawDebugTrace.NONE, True)
                gap = None
                for hit in hits or []:
                    t = hit.to_tuple()
                    loc_, actor, comp, item = t[4], t[9], t[10], t[13]
                    if actor and p.get("label") and actor.get_actor_label() == p["label"]:
                        continue
                    if comp and isinstance(comp, unreal.InstancedStaticMeshComponent) and item >= 0:
                        try:
                            xf = comp.get_instance_transform(item, True)
                            xf = xf[1] if isinstance(xf, tuple) else xf
                            if abs(xf.translation.x - p["x"]) < 1.0 and abs(xf.translation.y - p["y"]) < 1.0 and \
                                    abs(xf.translation.z - p["z"]) < 1.0:
                                continue
                        except Exception:
                            pass
                    if actor and isinstance(actor, unreal.StaticMeshActor) and not p.get("hism"):
                        lc = actor.get_actor_location()
                        if abs(lc.x - p["x"]) < 1.0 and abs(lc.y - p["y"]) < 1.0 and abs(lc.z - p["z"]) < 1.0:
                            continue
                    if loc_.z > bottom + 3.0:
                        continue
                    gap = bottom - loc_.z
                    break
                if gap is None and p.get("floor_prop") and abs(bottom - p["floor"]) <= 3.0:
                    gap = bottom - p["floor"]          # started inside the 2 cm floor tile (initial overlap)
                gaps.append(gap)
            real = [g for g in gaps if g is not None]
            if not real or min(real) > 8.0:
                floating.append({"m": p["m"], "at": [round(p["x"]), round(p["y"]), round(bottom, 1)], "room": p.get("room"),
                                 "gaps": [None if g is None else round(g, 1) for g in gaps]})
        return {"checked": checked, "floating": floating, "sunk": sunk}

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log(f"done in {time.time() - self.t0:.0f}s")
        w = self.world or game_world()
        unreal.SystemLibrary.quit_game(w, None, unreal.QuitPreference.QUIT, False)


SESSION = Session()
