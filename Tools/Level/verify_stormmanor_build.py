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


def main():
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
    check(1, "24 named rooms in the layout", len(named) == 24, f"{len(named)}")
    check(1, "walls, floors, doors, windows, stairs", st.get("wall_pieces", 0) > 400 and st.get("floor_tiles", 0) > 800
          and st.get("doors", 0) >= 45 and st.get("windows", 0) >= 100 and st.get("stairs", 0) >= 10,
          f"walls {st.get('wall_pieces')} floors {st.get('floor_tiles')} doors {st.get('doors')} windows {st.get('windows')} "
          f"stairs {st.get('stairs')}")
    check(1, "secret passages: 6 pairs of AKGPassage", st.get("passages") == 12, f"{st.get('passages')}")
    check(1, "20 player starts + meeting markers", st.get("starts") == 20, f"starts {st.get('starts')}")
    check(1, "lights <= 60, hero (shadowed) <= 4", 0 < st.get("lights", 0) <= 60 and st.get("hero_lights", 99) <= 4,
          f"lights {st.get('lights')} hero {st.get('hero_lights')}")
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
    check(3, "19 manor chores as world-chore data", len(chores.get("chores", [])) == 19 and chores.get("map") == "L_StormManor",
          f"{len(chores.get('chores', []))} chores, {len(chores.get('anchors', []))} anchors")
    unreach_chore = [k for k in nav.get("unreachable", []) if k.startswith("chore ")]
    check(3, "every chore spot reachable on the navmesh", nav and not unreach_chore, f"{unreach_chore}")
    rlist = routes.get("routes") or routes.get("chores") or []
    if isinstance(routes, dict) and not rlist:
        rlist = [v for v in routes.values() if isinstance(v, dict)]
    manor_ids = {c["id"] for c in chores.get("chores", [])}
    mine = [r for r in rlist if isinstance(r, dict) and r.get("chore", r.get("id")) in manor_ids]
    bad_routes = [f"{r.get('chore', r.get('id'))}/{r.get('variant')}" for r in mine if not r.get("reachable", r.get("ok"))]
    check(3, "route check: every chore x variant reachable (kg.WorldChore.Routes)", mine and not bad_routes,
          f"{len(mine)} routes, failing {bad_routes}")
    unreach_room = [k for k in nav.get("unreachable", []) if k.startswith("room ") or k.startswith("start ")]
    check(3, "bots: every room and start reachable from the meeting table", nav and not unreach_room, f"{unreach_room}")
    other = [k for k in nav.get("unreachable", []) if not (k.startswith("room ") or k.startswith("start ") or k.startswith("chore "))]
    check(3, "doors (both sides) and secret ends reachable", nav and not other, f"{other[:10]}")
    menu = open(os.path.join(ROOT, "Source", "KillGodot", "UI", "Menu", "KGMenuActions.cpp"), encoding="utf-8").read()
    check(4, "menu lists Storm Manor", "L_StormManor" in menu and '"Storm Manor"' in menu)
    mm = os.path.join(ROOT, "Art", "Textures", "Map")
    check(4, "minimap texture + regions from the JSON", os.path.exists(os.path.join(mm, "T_KG_Map_Stormmanor.png"))
          and st.get("minimap_regions", 0) >= 30, f"regions {st.get('minimap_regions')}")
    smoke = os.path.join(S, "Logs", "MatchSmoke_StormManor.log")
    ok_smoke = os.path.exists(smoke) and "Match decided:" in open(smoke, encoding="utf-8", errors="ignore").read()
    check(4, "headless whole match on L_StormManor reaches a winner", ok_smoke, smoke)
    check(5, "render sheet", os.path.exists(os.path.join(ROOT, "Docs", "Level", "StormManor_Renders.png")))
    out = {"checks": res, "pass": all(r["ok"] for r in res)}
    json.dump(out, open(os.path.join(S, "KG_SM_Verify.json"), "w"), indent=1)
    for r in res:
        print(f"[{'PASS' if r['ok'] else 'FAIL'}] A{r['acc']} {r['check']}  {r['detail']}")
    print("ALL PASS" if out["pass"] else "SOME CHECKS FAIL")
    return 0 if out["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
