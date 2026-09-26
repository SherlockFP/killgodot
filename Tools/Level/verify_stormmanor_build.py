"""SPRINT-018 acceptance checks for Storm Manor, from the build artefacts (system Python, seconds).

  python Tools/Level/verify_stormmanor_build.py            (exit 0 = every check passes)

Reads Saved/KG_SM_BuildReport.json (builder), Saved/KG_SM_Probe.json (-game probe: nav paths + placement traces),
Saved/KG_WorldChoreRoutes.json (kg.WorldChore.Routes on the manor), Tools/Level/stormmanor_world_chores.resolved.json,
the minimap files, the menu source and Docs/Level/StormManor_Renders.png. Writes Saved/KG_SM_Verify.json.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stormmanor_geo as G  # noqa: E402

ROOT = G.ROOT
S = os.path.join(ROOT, "Saved")


def load(path):
    try:
        return json.load(open(path, encoding="utf-8"))
    except (OSError, ValueError):
        return None


# SPRINT-018 baselines (Docs/Iterations/SPRINT-018-StormManor-Build.md, its build report)
OLD_DENSITY = 1325 / 5520.0            # indoor dressing meshes per m2 of rooms + corridors
OLD_SPARSE = {"blue_room": 51, "nursery": 42, "study": 35}
OLD_ROOMS = {"great_hall", "vestibule", "dining", "kitchen", "pantry", "laundry", "chapel", "library", "ballroom",
             "wine_cellar", "spark_room", "cistern", "gallery", "blue_room", "bath", "red_room", "studio", "nursery",
             "study", "master", "billiard", "attic", "clock_room", "storm_tower"}
OLD_CHORES = ["WineForTheHall", "CisternWater", "Firewood", "Chandelier", "Shutters", "SignalLamp", "PozzosSupper",
              "GuestLetters", "BailTheBoat", "PotsIndoors", "SparkJar", "MusicBox", "HangTheSheets", "HangThePortrait",
              "LostBook", "ClockAndBell", "GParcel", "GraveLanterns", "SetTheTable"]
SHOT_DIR = os.path.join(ROOT, "Saved", "Screenshots", "SM")
SHEET = os.path.join(ROOT, "Docs", "Level", "StormManor_Renders.png")


def g2_tests():
    """Latest Saved/Gauntlet/G2-tests-*.log -> {test: pass|fail}."""
    import glob
    import re
    logs = sorted(glob.glob(os.path.join(ROOT, "Saved", "Gauntlet", "G2-tests-*.log")), key=os.path.getmtime)
    out = {}
    # oldest -> newest: every test keeps its latest result (other agents' gate runs may use a narrower filter)
    for log in logs:
        for line in open(log, encoding="utf-8", errors="ignore"):
            m = re.search(r"Test Completed\. Result=\{(\w+)\} Name=\{[^}]*\} Path=\{([^}]*)\}", line)
            if m:
                out[m.group(2)] = "pass" if m.group(1).lower() == "success" else "fail"
    return out


def sheet(cols=6, w=440):
    """Docs/Level/StormManor_Renders.png: every Saved/Screenshots/SM/*.png, labelled."""
    from PIL import Image, ImageDraw
    files = sorted(f for f in os.listdir(SHOT_DIR) if f.endswith(".png"))
    order = ["aerial", "top", "courtyard", "great_hall", "staircase", "enfilade", "long_gallery"]
    files.sort(key=lambda f: (next((i for i, o in enumerate(order) if f.startswith(o)), 50),
                              1 if f.startswith(("secret_", "trap_")) else 0, f))
    h = int(w * 9 / 16)
    rows = (len(files) + cols - 1) // cols
    img = Image.new("RGB", (cols * w, rows * h), (20, 18, 16))
    d = ImageDraw.Draw(img)
    for k, f in enumerate(files):
        im = Image.open(os.path.join(SHOT_DIR, f)).convert("RGB").resize((w, h))
        x, y = (k % cols) * w, (k // cols) * h
        img.paste(im, (x, y))
        d.rectangle([x, y, x + 8 + 7 * len(f[:-4]), y + 16], fill=(0, 0, 0))
        d.text((x + 4, y + 2), f[:-4], fill=(255, 214, 120) if f.startswith(("secret_", "trap_")) else (255, 255, 255))
    img.save(SHEET, optimize=True)
    print(f"wrote {SHEET}: {len(files)} shots")


def main():
    if "--sheet" in sys.argv:
        sheet()
    L = G.layout()
    rep = load(os.path.join(S, "KG_SM_BuildReport.json")) or {}
    probe = load(os.path.join(S, "KG_SM_Probe.json")) or {}
    routes = load(os.path.join(S, "KG_WorldChoreRoutes.json")) or {}
    chores = load(os.path.join(ROOT, "Tools", "Level", "stormmanor_world_chores.resolved.json")) or {}
    st = rep.get("stats", {})
    res = []

    def check(aid, name, ok, detail=""):
        res.append({"acc": aid, "check": name, "ok": bool(ok), "detail": detail})

    named = [r["id"] for r in L["rooms"] if r["kind"] == "room"]
    check(1, "level built, no failed steps", rep.get("level") and not st.get("failed_steps"),
          f"failed={st.get('failed_steps')} secs={rep.get('secs')}")
    check(1, "SPRINT-040: >= 45 named rooms in the layout (was 24)", len(named) >= 45, f"{len(named)}")
    check(1, "walls, floors, doors, windows, stairs", st.get("wall_pieces", 0) > 400 and st.get("floor_tiles", 0) > 800
          and st.get("doors", 0) >= 45 and st.get("windows", 0) >= 100 and st.get("stairs", 0) >= 10,
          f"walls {st.get('wall_pieces')} floors {st.get('floor_tiles')} doors {st.get('doors')} windows {st.get('windows')} "
          f"stairs {st.get('stairs')}")
    check(3, "secret passages: every secret is a built AKGSecretPassage pair (>= 8)",
          st.get("passages") == 2 * len(L["secrets"]) and len(L["secrets"]) >= 8 and not st.get("setp_failed"),
          f"{st.get('passages')} ends for {len(L['secrets'])} secrets; setp_failed {st.get('setp_failed')}")
    check(1, "20 player starts + meeting markers", st.get("starts") == 20, f"starts {st.get('starts')}")
    check(2, "lights <= 70, shadowed <= 5", 0 < st.get("lights", 0) <= 70 and st.get("hero_lights", 99) <= 5,
          f"lights {st.get('lights')} hero {st.get('hero_lights')} refused {st.get('lights_refused', 0)}")
    # ---- SPRINT-040 acceptance
    sys.path.insert(0, os.path.join(ROOT, "Tools", "Level"))
    import validate_stormmanor as VS
    res_v = VS.analyse(VS.load())
    check(1, "validate_stormmanor.py passes (grid, doors, stairs, gates, gather <= 30 s at every gate)",
          not res_v["err"], "; ".join(res_v["err"][:4]) + " | " + ", ".join(f"N={g['N']}:{g['far_s']}s"
                                                                          for g in res_v["gates"]))
    new_wings = {r["wing"] for r in L["rooms"] if r["kind"] == "room" and r["id"] not in OLD_ROOMS}
    c2 = [r["id"] for r in L["rooms"] if r["floor"] == "C2" and r["kind"] == "room"]
    check(1, "two new wings + north block + grand staircase + long portrait corridors + enfilade + 2nd basement",
          {"east", "west", "north"} <= new_wings and "staircase_hall" in named and "long_gallery" in G.rooms(L)
          and "north_corridor" in G.rooms(L) and len(c2) >= 3,
          f"new wings {sorted(new_wings)}; C2 rooms {c2}")
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    fp = unary_union([Polygon(r["poly"]) for r in L["rooms"] if r["floor"] == "F0" and r["kind"] in ("room", "circulation")]).area
    check(1, "house footprint ~2x SPRINT-018 (ground floor 2240 m2)", fp >= 1.8 * 2240, f"{fp:.0f} m2 = {fp / 2240:.2f}x")
    check(1, "wings open by player count: wing-gate doors built and tagged", st.get("wing_gates", 0) >= 4,
          f"{st.get('wing_gates')} gate doors (KG_WingGate + KG_MinN_n)")
    indoor = [r for r in L["rooms"] if r["kind"] in ("room", "circulation")]
    area_in = sum(Polygon(r["poly"]).area for r in indoor)
    dressed = rep.get("rooms_dressed", {})
    mesh_in = sum(dressed.get(r["id"], 0) for r in indoor)
    dens = mesh_in / area_in if area_in else 0.0
    check(2, "density >= 1.5x SPRINT-018 per m2 indoors (0.240 -> >= 0.360)", dens >= 1.5 * OLD_DENSITY,
          f"{mesh_in} meshes / {area_in:.0f} m2 = {dens:.3f} ({dens / OLD_DENSITY:.2f}x)")
    sparse = {rid: dressed.get(rid, 0) for rid in ("blue_room", "nursery", "study")}
    check(2, "the sparse rooms filled (blue room, nursery, study >= 1.5x their SPRINT-018 counts)",
          all(sparse[k] >= 1.5 * OLD_SPARSE[k] for k in sparse), f"{sparse} (was {OLD_SPARSE})")
    wm, wf = st.get("white_meshes", None), st.get("white_fixed", {})
    check(2, "white pack meshes recoloured (every PropVC mesh without vertex colours gets a flat colour)",
          wm is not None and all(w in wf for w in wm), f"{len(wm or [])} white meshes, fixed {len(wf)}: "
                                                         f"{sorted(wf.items())[:6]}")
    comps = L.get("compartments", [])
    check(3, ">= 10 hidden compartments built (AKGHiddenCompartment)", len(comps) >= 10 and st.get("compartments") == len(comps),
          f"layout {len(comps)} built {st.get('compartments')}")
    regions = load(os.path.join(ROOT, "Art", "Textures", "Map", "KG_MapRegions_Stormmanor.json")) or {}
    secret_rooms = [r["id"] for r in L["rooms"] if r.get("secret")]
    leaked = [x["id"] for x in regions.get("regions", []) if x["id"] in secret_rooms]
    check(3, "minimap: secret rooms never in the map data (the HUD draws discovered passages only)", not leaked,
          f"secret rooms {secret_rooms}; leaked {leaked}")
    tests = g2_tests()
    need = ["KillGodot.Traps.Arming", "KillGodot.Traps.Trigger", "KillGodot.Traps.Cooldown", "KillGodot.Manor.Secrets",
            "KillGodot.Manor.Deal", "KillGodot.WorldChores.StormManor"]
    check(5, "automation tests: trap arming / trigger / cooldown, manor secrets + deal, manor catalog",
          all(tests.get(t) == "pass" for t in need), f"{ {t: tests.get(t) for t in need} }")
    check(5, ">= 6 trap kinds built as AKGTrap", len({t["effect"] for t in L.get("traps", [])}) >= 6
          and st.get("traps") == len(L.get("traps", [])), f"{st.get('traps')} traps, kinds "
                                                           f"{sorted({t['effect'] for t in L.get('traps', [])})}")
    missing = rep.get("missing_meshes", [])
    check(1, "no missing meshes", not missing, ", ".join(m.split("/")[-1] for m in missing[:8]))
    nav = probe.get("nav", {})
    check(1, "navmesh built (probe paths exist)", nav.get("targets", 0) > 0 and nav.get("reachable", 0) > 0,
          f"{nav.get('reachable')}/{nav.get('targets')}")
    dressed = rep.get("rooms_dressed", {})
    thin = [r for r in named if dressed.get(r, 0) < 12]
    check(2, "every named room dressed (>= 12 meshes each)", not thin, f"thin: {thin}; total {sum(dressed.values())}")
    grounds = [g for g in ("courtyard", "greenhouse", "graveyard", "boathouse", "service_yard") if dressed.get(g, 0) < 8]
    check(2, "grounds dressed (courtyard, greenhouse, graveyard, boathouse, yard)", not grounds, f"thin: {grounds}")
    plain = rep.get("plain_actors_per_floor", {})
    check(2, "<= 900 plain actors per floor, repeats instanced", plain and max(plain.values()) <= 900
          and rep.get("instances", 0) > 1000, f"{plain} instances {rep.get('instances')}")
    pl = probe.get("placement", {})
    check(2, "placement: no floating props (traced)", pl.get("checked", 0) > 200 and not pl.get("floating"),
          f"checked {pl.get('checked')} floating {len(pl.get('floating', []))} "
          f"{[f['m'] + '@' + str(f.get('room')) for f in pl.get('floating', [])[:6]]}")
    check(2, "placement: no sunk props", "sunk" in pl and not pl.get("sunk"), f"sunk {len(pl.get('sunk', []))}")
    check(2, "placement: nothing blocks doors / stairs (pre-check)", st.get("precheck_bad", 1) == 0,
          f"{st.get('precheck_bad')} {rep.get('precheck', [])[:4]}")
    cl = chores.get("chores", [])
    new_manor = [c["id"] for c in cl if c["id"] not in OLD_CHORES and not c.get("secret")]
    secret_ch = [c["id"] for c in cl if c.get("secret")]
    check(4, "world-chore data: the 19 old + >= 8 manor-only + >= 3 secret chores", chores.get("map") == "L_StormManor"
          and all(c in {x["id"] for x in cl} for c in OLD_CHORES) and len(new_manor) >= 8 and len(secret_ch) >= 3
          and all(c.get("reward_secret") or c.get("reward_compartment") for c in cl if c.get("secret")),
          f"{len(cl)} chores: {len(new_manor)} new manor, secret {secret_ch}")
    unreach_chore = [k for k in nav.get("unreachable", []) if k.startswith("chore ")]
    check(3, "every chore spot reachable on the navmesh", nav and not unreach_chore, f"{unreach_chore}")
    rlist = routes.get("routes") or routes.get("chores") or []
    if isinstance(routes, dict) and not rlist:
        # kg.WorldChore.Routes writes {ChoreId: [variant objects], "pass": n, "fail": n, ...}
        for cid, vs in routes.items():
            if isinstance(vs, list):
                rlist += [dict(v, chore=cid) for v in vs if isinstance(v, dict)]
    # (the secret chores start in the crypt / observatory: reached through their passage, not on the navmesh)
    manor_ids = {c["id"] for c in chores.get("chores", []) if not c.get("secret")}
    mine = [r for r in rlist if isinstance(r, dict) and r.get("chore", r.get("id")) in manor_ids]
    bad_routes = [f"{r.get('chore', r.get('id'))}/{r.get('variant')}" for r in mine if not r.get("reachable", r.get("ok"))]
    check(3, "route check: every chore x variant reachable (kg.WorldChore.Routes)", mine and not bad_routes,
          f"{len(mine)} routes, failing {bad_routes}")
    # the tower top is reached by the KGLadder only (no navmesh link; bots do not climb): listed, not failed
    LADDER_ONLY = {"room tower_top"}
    unreach_room = [k for k in nav.get("unreachable", []) if (k.startswith("room ") or k.startswith("start "))
                    and k not in LADDER_ONLY]
    check(3, "bots: every room and start reachable from the meeting table", nav and not unreach_room,
          f"{unreach_room} (ladder-only, not counted: {sorted(LADDER_ONLY)})")
    other = [k for k in nav.get("unreachable", []) if not (k.startswith("room ") or k.startswith("start ") or k.startswith("chore "))]
    check(3, "doors (both sides) and secret ends reachable", nav and not other, f"{other[:10]}")
    menu = open(os.path.join(ROOT, "Source", "KillGodot", "UI", "Menu", "KGMenuActions.cpp"), encoding="utf-8").read()
    check(4, "menu lists Storm Manor", "L_StormManor" in menu and '"Storm Manor"' in menu)
    mm = os.path.join(ROOT, "Art", "Textures", "Map")
    check(4, "minimap texture + regions from the JSON", os.path.exists(os.path.join(mm, "T_KG_Map_Stormmanor.png"))
          and st.get("minimap_regions", 0) >= 30, f"regions {st.get('minimap_regions')}")
    smoke = os.path.join(S, "Logs", "MatchSmoke_StormManor.log")
    rp = os.path.join(S, "KG_SM_BuildReport.json")
    ok_smoke = (os.path.exists(smoke) and "Match decided:" in open(smoke, encoding="utf-8", errors="ignore").read()
                and os.path.getmtime(smoke) > os.path.getmtime(rp))      # (a smoke of THIS build)
    check(4, "headless whole match on L_StormManor reaches a winner", ok_smoke, smoke)
    shots = sorted(os.path.splitext(f)[0] for f in os.listdir(SHOT_DIR)) if os.path.isdir(SHOT_DIR) else []
    check(6, "render sheet >= 30 shots incl. >= 4 secrets and >= 3 traps",
          os.path.exists(SHEET) and len(shots) >= 30 and sum(s.startswith("secret_") for s in shots) >= 4
          and sum(s.startswith("trap_") for s in shots) >= 3,
          f"{len(shots)} shots, secrets {[s for s in shots if s.startswith('secret_')]}, "
          f"traps {[s for s in shots if s.startswith('trap_')]}")
    out = {"checks": res, "pass": all(r["ok"] for r in res)}
    json.dump(out, open(os.path.join(S, "KG_SM_Verify.json"), "w"), indent=1)
    for r in res:
        print(f"[{'PASS' if r['ok'] else 'FAIL'}] A{r['acc']} {r['check']}  {r['detail']}")
    print("ALL PASS" if out["pass"] else "SOME CHECKS FAIL")
    return 0 if out["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
