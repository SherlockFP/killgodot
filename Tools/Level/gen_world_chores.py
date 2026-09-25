"""SPRINT-016: resolves Tools/Level/morrowmere_world_chores.json against the v2 layout and embeds it in the game.

  python Tools/Level/gen_world_chores.py

Resolves the authoring helpers of every anchor into plain world data (metres, UE frame):
  "door": [house_id, along_m, out_m]         -> at/z/yaw at that house's front door (+ along the wall, + outwards)
  "tower_top": [landmark, levels, lx, ly]    -> the lookout floor of a ladder tower (local cm like the v2 builder),
                                                 stand = the tower door, climb_m = (levels - 1) * 3 m
  "lane": [lane_name, t, side_m]             -> point t (fractional index) of a layout lane, side_m to its left
Writes
  Tools/Level/morrowmere_world_chores.resolved.json           (dev builds read this when present: no recompile)
  Source/KillGodot/Chores/WorldChores/KGWorldChoreData.gen.inl (the same JSON embedded for every build)
"""
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "Tools", "Level", "morrowmere_world_chores.json")
LAYOUT = os.path.join(ROOT, "Tools", "Level", "morrowmere_layout_v2.json")
OUT_JSON = os.path.join(ROOT, "Tools", "Level", "morrowmere_world_chores.resolved.json")
OUT_INL = os.path.join(ROOT, "Source", "KillGodot", "Chores", "WorldChores", "KGWorldChoreData.gen.inl")
FLOOR_H = 3.0


def building(L, bid):
    for b in L["houses"] + L.get("infill", []):
        if b["id"] == bid:
            return b
    if bid in L["landmarks"]:
        return L["landmarks"][bid]
    raise KeyError(bid)


def door_point(b, out=0.8):
    """Same formula as Tools/Unreal/kg_capture_v2.py navcheck (the v2 builder's front door, 0.8 m outside)."""
    w, d = b["size"]
    cw = w // 2
    lx = -cw + 1 + 2 * (cw // 2) if w >= 4 else 0.0
    yaw = math.radians(b.get("yaw", b["face_deg"] + 90.0))
    c, s = math.cos(yaw), math.sin(yaw)
    ly = -d / 2.0 - out
    return b["at"][0] + lx * c - ly * s, b["at"][1] + lx * s + ly * c


def resolve_anchor(L, a):
    a = dict(a)
    if "door" in a:
        bid, along, out = a.pop("door")
        b = building(L, bid)
        x, y = door_point(b)
        fx, fy = b["face"][0] - b["at"][0], b["face"][1] - b["at"][1]
        n = math.hypot(fx, fy) or 1.0
        fx, fy = fx / n, fy / n
        tx, ty = -fy, fx
        a["at"] = [round(x + along * tx + out * fx, 2), round(y + along * ty + out * fy, 2)]
        a["z"] = b["z"]
        a.setdefault("yaw", round(math.degrees(math.atan2(fy, fx)), 1))
        a.setdefault("house", bid)
    elif "tower_top" in a:
        name, levels, lx, ly = a.pop("tower_top")
        tw = L["landmarks"][name]
        yaw = tw.get("yaw", tw["face_deg"] + 90.0)
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        x = tw["at"][0] + (lx * c - ly * s) / 100.0
        y = tw["at"][1] + (lx * s + ly * c) / 100.0
        a["at"] = [round(x, 2), round(y, 2)]
        a["z"] = round(tw["z"] + 0.02 + (levels - 1) * FLOOR_H, 2)
        sx, sy = door_point(tw)
        a["stand"] = [round(sx, 2), round(sy, 2), tw["z"]]
        a["climb_m"] = round((levels - 1) * FLOOR_H, 2)
        a.setdefault("yaw", yaw)
    elif "lane" in a:
        name, t, side = a.pop("lane")
        lane = next(l for l in L["lanes"] if l["name"] == name)
        pts = lane["points"]
        i = min(int(math.floor(t)), len(pts) - 2)
        f = t - i
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        x, y = ax + (bx - ax) * f, ay + (by - ay) * f
        dx, dy = bx - ax, by - ay
        n = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / n, dx / n
        a["at"] = [round(x + nx * side, 2), round(y + ny * side, 2)]
        a["z"] = float(lane["z"])
        a["stand"] = [round(x, 2), round(y, 2), float(lane["z"])]
        a.setdefault("yaw", round(math.degrees(math.atan2(-ny, -nx)), 1))   # faces the lane
    a.setdefault("yaw", 0.0)
    if "stand" in a and len(a["stand"]) == 2:
        a["stand"] = a["stand"] + [a["z"]]
    a.setdefault("climb_m", 0.0)
    return a


def main():
    L = json.load(open(LAYOUT, encoding="utf-8"))
    D = json.load(open(SRC, encoding="utf-8"))
    D["anchors"] = [resolve_anchor(L, a) for a in D["anchors"]]
    ids = {a["id"] for a in D["anchors"]}
    # sanity: every step target exists after variant substitution
    for c in D["chores"]:
        for v in c.get("variants", [{"name": "default", "vars": {}}]):
            for s in c["steps"]:
                ats = s["at"] if isinstance(s["at"], list) else [s["at"]]
                for at in ats:
                    at = v["vars"].get(at[1:], at) if at.startswith("$") else at
                    assert at in ids, f"{c['id']}/{v['name']}: unknown anchor {at}"
                assert s.get("item", "Bucket") in D["items"], s
    text = json.dumps(D, indent=None, separators=(",", ":"), ensure_ascii=True)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(D, f, indent=1)
    chunks = [text[i:i + 6000] for i in range(0, len(text), 6000)]   # MSVC: a wide literal must stay under 16 KB
    with open(OUT_INL, "w", encoding="utf-8", newline="\n") as f:
        f.write("// GENERATED by Tools/Level/gen_world_chores.py from Tools/Level/morrowmere_world_chores.json - do not edit.\n")
        f.write("// Included once by KGWorldChoreTypes.cpp.\n")
        f.write("static const TCHAR* const GKGWorldChoreJsonChunks[] = {\n")
        for ch in chunks:
            f.write('\tTEXT(R"KGJSON(' + ch + ')KGJSON"),\n')
        f.write("};\n")
    print(f"world chores: {len(D['chores'])} chores, {len(D['anchors'])} anchors -> {OUT_INL} ({len(text)} chars, {len(chunks)} chunks)")


if __name__ == "__main__":
    main()
