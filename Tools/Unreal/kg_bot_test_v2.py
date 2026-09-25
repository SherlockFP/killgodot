"""Headless 20-bot soak of L_Morrowmere_v2 in a real game session (-game -nullrhi, no window): roaming, the meeting
run-up to the Fountain Square, the meeting ring, day chores, and navmesh paths to every chore.

  powershell -File Tools/Unreal/kg_bot_test_v2.ps1          (launcher; runs this via -ExecCmds="py <copy>")
Output: Saved/KG_V2_BotTest.json (+ KG_BOT / KG_BOTSTATS lines in Saved/Logs/kg_bot_test_v2.log).

Timeline (real seconds; env KG_ROAM_S / KG_GATHER_S / KG_CHORE_S override):
  A roam   : kg.Bot.Add 20 before the match (no chores): bots roam the level anchors (AKGBotController). Every second:
             position, district (terrace polygon), stalls (< 0.5 m in 15 s), distance walked.
  B gather : kg.BotGather 1: every bot walks to KG_BotHub (the fountain). Arrival = within 10 m. Timeout KG_GATHER_S.
  C meeting: kg.Match.Start + kg.Match.Phase Meeting: AKGGameMode's ring at the gallows; checks everyone landed on the
             square (z within 1.5 m of the terrace, within 12 m of the gallows).
  D chores : kg.Match.Phase Day + kg.Match.Freeze 1: bots work their chores (KG_BOT chore ... done lines).
  E paths  : navmesh path hub -> every chore station and every PlayerStart -> every chore (length, partial/failed).
"""
import json
import math
import os
import statistics
import time

import unreal

ROOT = "D:/Kill Godot"
OUT = f"{ROOT}/Saved/KG_V2_BotTest.json"
ROAM_S = float(os.environ.get("KG_ROAM_S", "150"))
GATHER_S = float(os.environ.get("KG_GATHER_S", "120"))
CHORE_S = float(os.environ.get("KG_CHORE_S", "240"))
BOTS = int(os.environ.get("KG_BOTS", "20"))
L = json.load(open(f"{ROOT}/Tools/Level/morrowmere_layout_v2.json", encoding="utf-8"))
TERR = [(t["district"], t["polygon"], t["z"]) for t in L["terraces"]]


def log(m):
    unreal.log(f"KG_BOTTEST {m}")


def inside(p, poly):
    x, y = p
    c = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c


def district(x, y):
    for d, poly, _z in TERR:
        if inside((x, y), poly):
            return d
    return "outskirts"


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == "L_Morrowmere_v2" and unreal.GameplayStatics.get_game_mode(w):
                return w
        except Exception:
            continue
    return None


def pct(vals, q):
    if not vals:
        return None
    s = sorted(vals)
    k = min(len(s) - 1, max(0, int(round(q * (len(s) - 1)))))
    return round(s[k], 1)


class Soak:
    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.state = "boot"
        self.state_t = self.t0
        self.next_sample = 0.0
        self.out = {"bots_requested": BOTS}
        self.track = {}      # name -> dict
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        log(f"armed: roam {ROAM_S}s, gather <= {GATHER_S}s, chores {CHORE_S}s")

    # ------------------------------------------------------------------ helpers
    def cmd(self, c):
        unreal.SystemLibrary.execute_console_command(self.world, c)
        log(f"> {c}")

    def bots(self):
        out = []
        for p in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.Character):
            c = p.get_controller()
            if not c or c.get_class().get_name() != "KGBotController":
                continue
            ps = None
            for getter in (lambda: c.player_state, lambda: c.get_editor_property("player_state"),
                           lambda: p.get_editor_property("player_state")):
                try:
                    ps = getter()
                    if ps:
                        break
                except Exception:
                    continue
            # names can repeat (random villager names): keep them unique per body
            out.append((f"{ps.get_player_name() if ps else 'bot'}#{p.get_name().rsplit('_', 1)[-1]}", p))
        return out

    def hub(self):
        hubs = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_BotHub")
        return hubs[0].get_actor_location() if hubs else None

    def go(self, state):
        self.state = state
        self.state_t = time.time()
        log(f"state {state}")

    def sample_roam(self, now):
        for name, p in self.bots():
            loc = p.get_actor_location()
            t = self.track.setdefault(name, {"districts": {}, "levels": set(), "dist": 0.0, "last": None,
                                            "still_since": now, "stalls": 0, "max_still": 0.0})
            d = district(loc.x / 100.0, loc.y / 100.0)
            t["districts"][d] = t["districts"].get(d, 0) + 1
            t["levels"].add(int(round(loc.z / 100.0)))
            if t["last"] is not None:
                step = math.hypot(loc.x - t["last"][0], loc.y - t["last"][1]) / 100.0
                t["dist"] += step
                if step > 0.5:
                    still = now - t["still_since"]
                    t["max_still"] = max(t["max_still"], still)
                    if still >= 15.0:
                        t["stalls"] += 1
                    t["still_since"] = now
            t["last"] = (loc.x, loc.y, loc.z)

    # ------------------------------------------------------------------ phases
    def tick(self, dt):
        now = time.time()
        try:
            self.step(now)
        except Exception as e:
            import traceback
            log(f"error {e}\n{traceback.format_exc()}")
            self.finish()

    def step(self, now):
        el = now - self.state_t
        if self.state == "boot":
            if el < 6.0:
                return
            self.world = game_world()
            if not self.world:
                if now - self.t0 > 120:
                    log("no game world")
                    self.finish()
                return
            self.cmd("t.MaxFPS 30")
            self.cmd("kg.Me.God 1")
            self.cmd(f"kg.Bot.Add {BOTS}")
            self.cmd("kg.BotStats reset")
            self.go("roam")
            return
        if self.state == "roam":
            if now >= self.next_sample:
                self.next_sample = now + 1.0
                self.sample_roam(now)
            if el >= ROAM_S:
                self.cmd("kg.BotStats")
                self.report_roam()
                self.cmd("kg.BotStats reset")
                self.cmd("kg.BotGather 1")
                self.gather = {}
                self.gather_start = {n: p.get_actor_location() for n, p in self.bots()}
                self.go("gather")
            return
        if self.state == "gather":
            h = self.hub()
            for name, p in self.bots():
                if name in self.gather:
                    continue
                loc = p.get_actor_location()
                if h and math.hypot(loc.x - h.x, loc.y - h.y) < 1000.0:
                    self.gather[name] = round(el, 1)
            if len(self.gather) >= len(self.bots()) or el >= GATHER_S:
                self.cmd("kg.BotStats")
                self.report_gather(h)
                self.cmd("kg.BotGather 0")
                self.cmd("kg.Match.Start 4242")
                self.go("match")
            return
        if self.state == "match":
            if el < 4.0:
                return
            self.cmd("kg.Match.Phase Meeting")
            self.go("meeting")
            return
        if self.state == "meeting":
            if el < 4.0:
                return
            self.report_meeting()
            self.cmd("kg.Match.Phase Day")
            self.cmd("kg.Match.Freeze 1")
            self.cmd("kg.BotStats reset")
            self.go("chores")
            return
        if self.state == "chores":
            if el >= CHORE_S:
                self.cmd("kg.BotStats")
                self.out["chores"] = {"seconds": CHORE_S, "alive_bots": sum(1 for _n, p in self.bots())}
                self.report_paths()
                self.finish()

    # ------------------------------------------------------------------ reports
    def report_roam(self):
        per = {}
        all_d = {}
        for name, t in self.track.items():
            per[name] = {"districts": sorted(t["districts"]), "levels_m": sorted(t["levels"]),
                         "walked_m": round(t["dist"]), "stalls_15s": t["stalls"], "max_still_s": round(t["max_still"], 1)}
            for d, n in t["districts"].items():
                all_d[d] = all_d.get(d, 0) + n
        walked = [v["walked_m"] for v in per.values()]
        nd = [len(v["districts"]) for v in per.values()]
        self.out["roam"] = {
            "seconds": ROAM_S, "bots": len(per),
            "districts_visited_by_anyone": sorted(all_d), "bot_seconds_per_district": dict(sorted(all_d.items())),
            "districts_per_bot": {"min": min(nd) if nd else 0, "median": statistics.median(nd) if nd else 0, "max": max(nd) if nd else 0},
            "walked_m": {"min": min(walked) if walked else 0, "median": statistics.median(walked) if walked else 0,
                         "max": max(walked) if walked else 0},
            "bots_with_15s_stall": sum(1 for v in per.values() if v["stalls_15s"] > 0),
            "levels_seen_m": sorted({z for v in per.values() for z in v["levels_m"]}),
            "per_bot": per}
        log(f"roam: {json.dumps({k: v for k, v in self.out['roam'].items() if k != 'per_bot'})}")

    def report_gather(self, h):
        times = list(self.gather.values())
        bots = self.bots()
        missing = [n for n, _p in bots if n not in self.gather]
        dist0 = [math.hypot(v.x - h.x, v.y - h.y) / 100.0 for v in self.gather_start.values()] if h else []
        still = {}
        for n, p in bots:
            if n in missing:
                loc = p.get_actor_location()
                still[n] = {"at_m": [round(loc.x / 100, 1), round(loc.y / 100, 1), round(loc.z / 100, 1)],
                            "district": district(loc.x / 100.0, loc.y / 100.0),
                            "to_hub_m": round(math.hypot(loc.x - h.x, loc.y - h.y) / 100.0, 1) if h else None}
        self.out["gather"] = {"bots": len(bots), "arrived": len(times), "timeout_s": GATHER_S,
                              "start_dist_m": {"median": round(statistics.median(dist0), 1) if dist0 else None,
                                               "max": round(max(dist0), 1) if dist0 else None},
                              "arrival_s": {"min": min(times) if times else None, "median": pct(times, 0.5),
                                            "p90": pct(times, 0.9), "max": max(times) if times else None},
                              "per_bot_s": dict(sorted(self.gather.items(), key=lambda kv: kv[1])), "not_arrived": still}
        log(f"gather: {json.dumps({k: v for k, v in self.out['gather'].items() if k != 'per_bot_s'})}")

    def report_meeting(self):
        g = unreal.GameplayStatics.get_all_actors_with_tag(self.world, "KG_Gallows")
        gl = g[0].get_actor_location() if g else None
        ok, bad = 0, []
        for name, p in self.bots():
            loc = p.get_actor_location()
            d = math.hypot(loc.x - gl.x, loc.y - gl.y) / 100.0 if gl else -1
            on_square = abs(loc.z / 100.0 - 5.0 - 0.9) < 1.5 and 0 <= d < 12.0
            if on_square:
                ok += 1
            else:
                bad.append({"bot": name, "at_m": [round(loc.x / 100, 1), round(loc.y / 100, 1), round(loc.z / 100, 1)],
                            "from_gallows_m": round(d, 1)})
        self.out["meeting"] = {"bots": len(self.bots()), "on_square": ok, "off": bad}
        log(f"meeting: {ok} on the square, off: {bad}")

    def report_paths(self):
        hub = self.hub()
        if hub:
            hub = unreal.Vector(hub.x + 250.0, hub.y, hub.z - 100.0)   # beside the fountain, on the paving
        towers = {t["id"] for t in L["tasks"] if t.get("tower")}
        tcls = unreal.load_class(None, "/Script/KillGodot.KGTaskStation")
        stations = [(str(a.get_editor_property("task_id")), a.get_actor_location())
                    for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, tcls)]
        starts = [a.get_actor_location() for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.PlayerStart)]
        res = {}
        for tid, p in stations:
            if tid in towers:
                # lookout floor, reached by the tower ladder (not navmesh): check the path to the tower door instead
                lm = next((b for b in L["landmarks"].values() if b.get("kind") == "tower" and
                           math.hypot(b["at"][0] * 100 - p.x, b["at"][1] * 100 - p.y) < 400), None)
                if lm:
                    yaw = math.radians(lm.get("yaw", lm["face_deg"] + 90.0))
                    ly = -lm["size"][1] / 2.0 - 0.9
                    p = unreal.Vector(lm["at"][0] * 100 - ly * 100 * math.sin(yaw), lm["at"][1] * 100 + ly * 100 * math.cos(yaw),
                                      lm["z"] * 100 + 20)
            end = unreal.Vector(p.x, p.y, p.z + 60.0)
            lens, fails = [], 0
            for s in [hub] + starts:
                if s is None:
                    continue
                path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                    self.world, unreal.Vector(s.x, s.y, s.z + 60.0), end)
                if path and path.is_valid() and not path.is_partial():
                    lens.append(path.get_path_length() / 100.0)
                else:
                    fails += 1
            res[tid] = {"from_hub_m": round(lens[0], 1) if lens else None, "ok": len(lens), "failed": fails,
                        "walk_s_at_6mps": round(lens[0] / 6.0, 1) if lens else None,
                        "max_m": round(max(lens), 1) if lens else None}
            if tid in towers:
                res[tid]["note"] = "tower door (the chore is up the ladder)"
        self.out["paths"] = {"stations": len(stations), "sources": 1 + len(starts),
                             "all_ok": all(v["failed"] == 0 for v in res.values()), "per_chore": dict(sorted(res.items()))}
        log(f"paths: {len(stations)} chores x {1 + len(starts)} sources, all ok = {self.out['paths']['all_ok']}")

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.out["total_s"] = round(time.time() - self.t0)
        json.dump(self.out, open(OUT, "w"), indent=1, default=list)
        log(f"done -> {OUT}")
        w = self.world or game_world()
        unreal.SystemLibrary.quit_game(w, None, unreal.QuitPreference.QUIT, False)


SOAK = Soak()
