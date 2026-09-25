"""Re-furnish every house of the LIVE L_Morrowmere with Tools/Unreal/kg_interiors.py, without a full village rebuild,
then check each house (capsule walk + stair profile) and save the level. PIE must be stopped.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_refurnish.py --timeout 900

Removes the outliner folder Village/Interior (and the pre-2026-09-24 interior leftovers), runs furnish() for the 16
homes and the 6 landmark houses with the same frames build_house uses, then per house:
  - rays down the stair centre line: treads must rise step by step and the landing must sit at the upper floor (301),
  - a player capsule (r 32, lifted 20 cm so steps do not count) swept door -> stair foot -> up the stair -> landing ->
    upstairs room must hit nothing.
Prints KG_REFURNISH / KG_VERIFY lines; the failures count is the last line.
"""
import importlib
import sys
import time

import unreal

sys.path.insert(0, "D:/Kill Godot/Tools/Unreal")
import kg_interiors as KI  # noqa: E402
importlib.reload(KI)

LEVEL = "/Game/KillGodot/Maps/L_Morrowmere"


def builder_helpers():
    """kg_build_village.py's helpers (ground, Frame, warm_light, LAYOUT...) without running the build."""
    src = open("D:/Kill Godot/Tools/Unreal/kg_build_village.py", encoding="utf-8").read()
    src = src[:src.index("\nnew_level()\n")]
    ns = {"__name__": "kg_builder_helpers"}
    exec(compile(src, "kg_build_village_helpers", "exec"), ns)
    return ns


def houses(ns):
    """(spec, style) in build order: homes get style = layout index, landmarks 20+k (see town())."""
    out = [(h, k) for k, h in enumerate(ns["LAYOUT"]["houses"])]
    out += [(ns["lm"](n), 20 + k) for k, n in enumerate(["town_hall", "inn", "bakery", "boathouse", "mill_barn", "church"])]
    return out


def frame(ns, spec):
    at, (w, d) = spec["at"], spec["size"]
    x, y, yaw = at[0] * 100.0, at[1] * 100.0, ns["facing_yaw"](at, spec["face"])
    gz = max(ns["ground"](x + dx, y + dy) for dx in (-w * 50, w * 50) for dy in (-d * 50, d * 50)) + 2.0
    return ns["Frame"](x, y, gz, yaw), w, d


def clear(actors):
    doomed = []
    for a in actors.get_all_level_actors():
        fp = str(a.get_folder_path())
        if fp.startswith("Village/Interior") or fp.startswith("Village/Lights/Interior"):
            doomed.append(a)
        elif fp == "Village/Houses" and isinstance(a, unreal.StaticMeshActor):
            sm = a.static_mesh_component.static_mesh
            if sm and sm.get_name().startswith("Stair_Interior"):
                doomed.append(a)
    actors.destroy_actors(doomed)
    return len(doomed)


def verify(world, f, w, d, style):
    vis = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1
    h = KI.House(f, w, d, style, None)

    def at(lx, ly, lz):
        wx, wy = h.world(lx, ly)
        return unreal.Vector(wx, wy, f.z + lz)

    def ray(lx, ly, z0, z1):
        hit = unreal.SystemLibrary.line_trace_single(world, at(lx, ly, z0), at(lx, ly, z1), vis, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        return round(t[4].z - f.z, 1) if t and t[0] else None
    treads = [ray(h.stair_x, h.foot_y + 10.0 + k * (h.ey - KI.LANDING - 20.0 - h.foot_y) / 20, 296.0, -50.0)
              for k in range(21)]
    landing = ray(h.stair_x, h.ey - 50.0, 400.0, 100.0)
    rising = all(t is not None for t in treads) and all(b >= a - 1.0 for a, b in zip(treads, treads[1:]))

    def sz(ly):
        return max(0.0, min(301.0, (ly - h.foot_y) * 0.714))
    pts = [(h.door_x, -h.ey + 70.0, 2.0), (h.door_x, max(-h.ey + 70.0, h.approach_y), 2.0),
           (-h.ex + 100.0, h.approach_y, 2.0)]
    ly = h.foot_y
    while ly < h.ey - KI.LANDING:
        pts.append((-h.ex + 100.0, ly, sz(ly) + 2.0))
        ly += 40.0
    side = -h.ex + (250.0 if w >= 6 else 248.0)      # along the balustrade, clear of its posts
    pts += [(-h.ex + 100.0, h.ey - 60.0, 301.0), (side, h.ey - 60.0, 301.0), (side, -h.ey * 0.1, 301.0)]
    if w >= 6:
        pts.append((h.ex * 0.35, -h.ey * 0.1, 301.0))
    blocked = []
    for a, b in zip(pts, pts[1:]):
        hit = unreal.SystemLibrary.capsule_trace_single(world, at(a[0], a[1], a[2] + 20.0 + 88.0),
                                                        at(b[0], b[1], b[2] + 20.0 + 88.0), 32.0, 88.0, vis, False, [],
                                                        unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        if t and t[0]:
            blocked.append((tuple(round(v) for v in a), t[9].get_actor_label() if t[9] else "?"))
    ok = rising and treads[-1] > 250.0 and landing is not None and abs(landing - 301.0) < 4.0 and not blocked
    return ok, treads, landing, blocked


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        print("KG_REFURNISH: PIE is running - stop it first")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or not world.get_path_name().startswith(LEVEL + "."):
        print("KG_REFURNISH: L_Morrowmere is not the open level")
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ns = builder_helpers()
    t0 = time.time()
    removed = clear(actors)
    total, frames = 0, []
    for spec, style in houses(ns):
        f, w, d = frame(ns, spec)
        total += KI.furnish(f, w, d, style, ns["warm_light"])
        frames.append((f, w, d, style))
    print(f"KG_REFURNISH removed {removed}, spawned {total} in {time.time() - t0:.1f}s")
    bad = 0
    for idx, (f, w, d, style) in enumerate(frames):
        ok, treads, landing, blocked = verify(world, f, w, d, style)
        bad += 0 if ok else 1
        print(f"KG_VERIFY house {idx} {w}x{d} style {style}: {'OK' if ok else 'FAIL'} treads {treads[0]}..{treads[-1]} "
              f"landing {landing} blocked {blocked}")
    print("KG_REFURNISH saved", les.save_current_level())
    print(f"KG_VERIFY failures {bad} of {len(frames)}")


main()
