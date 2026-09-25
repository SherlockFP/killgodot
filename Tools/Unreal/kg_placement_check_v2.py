"""Placement samples of L_Morrowmere_v2 for the geometric validator (SPRINT-022, Tools/Level/verify_v2_build.py).

Headless and read-only (nothing is saved, so it may run while the editor is open):
  powershell -File Tools/Unreal/kg_placement_check_v2.ps1      (-game -nullrhi -nosound, no window; ~1 min)
A commandlet world has no static-mesh collision (the bodies are only created once the mesh compilation is ticked), so
the traces run in a -nullrhi game session of the level: this script is started with -ExecCmds="py <copy>" and works from
a Slate post-tick callback once the game world exists, then quits.

For every exterior prop of the level (StaticMeshActors, movers, breakables, seats, chests and every instance of the
dressing / nature HISM fields; architecture, interiors, water and sky are classified, not sampled) it writes:
  - the oriented footprint (world XY corners of the mesh bounds' bottom face) and its height,
  - 5 samples of the bottom face (centre + 4 at 20 % inset): the bottom z and every blocking surface under it,
    found with Visibility line traces (complex collision) that walk down from 1 m above the bottom to 3 m below it,
    skipping the prop itself (its actor, or its own HISM instance).
Rules are evaluated by verify_v2_build.py (system Python + shapely): floating > 8 cm, sunk over tolerance, standing on
a stair tread, intersecting stairs / ramps / bridges / door aprons.
Output: Saved/KG_V2_PlacementSamples.json
"""
import json
import math
import time

import unreal

LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
OUT = "D:/Kill Godot/Saved/KG_V2_PlacementSamples.json"
import os  # noqa: E402
MAPNAME = os.environ.get("KG_V2_MAPNAME", "L_Morrowmere_v2")      # a Test/ copy for SPRINT-022 test builds
T0 = time.time()

# Folders that hold architecture / systems, not props (their own checks live in verify_v2_build.py).
SKIP_FOLDERS = ("V2/Houses", "V2/Shells", "V2/Civic", "V2/Towers", "V2/Walls", "V2/Parapets", "V2/Stairs",
                "V2/StairRamps", "V2/StairWalls", "V2/RampWalls", "V2/Quay", "V2/Terrain", "V2/Ladders", "V2/Lights",
                "V2/Sky", "V2/Sea", "V2/Audio", "V2/Doors", "V2/Arches", "V2/GardenWalls", "V2/Mole",
                "V2/Jetty", "V2/JettyDeck", "V2/FishPier", "V2/FishPierDeck", "V2/Bridges", "V2/Brook", "V2/Water", "V2/KoiSpill",
                "V2/Coast", "V2/Gallows", "V2/Waterwheel", "V2/Facade", "V2/Towers", "V2/Landmarks/Arch", "Gameplay/Dev", "Village/Interior",
                "Lighting", "Underground", "V2/Underground", "Dig")
SAMPLES = ((0.5, 0.5), (0.2, 0.2), (0.8, 0.2), (0.2, 0.8), (0.8, 0.8))
UP = 100.0          # trace start above the bottom face (cm)
DOWN = 300.0        # trace end below it


def log(m):
    unreal.log(f"KG_PLACECHECK {m}")
    print(f"KG_PLACECHECK {m}")


def folder_of(a):
    try:
        return str(a.get_folder_path())
    except Exception:
        return ""


def skip_folder(f):
    return any(f == s or f.startswith(s + "/") for s in SKIP_FOLDERS) or "/Interior" in f


def mesh_name(m):
    return m.get_name() if m else ""


def hit_tuple(h):
    t = h.to_tuple()
    # (blocking, initial_overlap, time, distance, location, impact_point, normal, impact_normal, phys_mat, actor,
    #  component, hit_bone, bone, item, element, face, start, end)
    return t


class Sampler:
    def __init__(self, world):
        self.world = world
        self.n_traces = 0
        self.terrain = [a.static_mesh_component for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
                        if folder_of(a) == "V2/Terrain" and a.static_mesh_component.static_mesh
                        and "Terrain" in a.static_mesh_component.static_mesh.get_name()]

    def surfaces(self, x, y, z0, self_actor, self_comp, self_item, z_top=None):
        """Blocking surfaces under (x, y) from above the prop (its top + 20 cm, at least z0 + UP) down to z0 - DOWN
        (top first), skipping the prop itself: a sunk prop still sees the ground above its bottom."""
        out = []
        top = z0 + UP
        ignore = [self_actor] if self_actor is not None and self_item is None else []
        for _ in range(40):
            if top <= z0 - DOWN:
                break
            self.n_traces += 1
            hit = unreal.SystemLibrary.line_trace_single(
                self.world, unreal.Vector(x, y, top), unreal.Vector(x, y, z0 - DOWN),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
            if hit is None:
                break
            t = hit_tuple(hit)
            if not t[0]:
                break
            p, nrm, actor, comp, item = t[5], t[7], t[9], t[10], t[13]
            zc = p.z
            if comp is not None and self_comp is not None and comp == self_comp and (self_item is None or item == self_item):
                top = zc - 8.0          # our own instance: continue below it
                continue
            name, kind = "", ""
            if actor is not None:
                kind = actor.get_class().get_name()
                name = actor.get_actor_label()
                f = folder_of(actor)
            else:
                f = ""
            mname = ""
            try:
                if isinstance(comp, unreal.StaticMeshComponent):
                    mname = mesh_name(comp.static_mesh)
            except Exception:
                pass
            out.append({"z": round(zc, 1), "nz": round(nrm.z, 2), "a": name, "f": f, "m": mname, "k": kind})
            top = zc - 0.5
            if len(out) >= 6:
                break
        # the ground itself, straight from the terrain tiles (also above a sunk prop's bottom)
        for tc in self.terrain:
            self.n_traces += 1
            r = tc.line_trace_component(unreal.Vector(x, y, z_top + 2000.0), unreal.Vector(x, y, z0 - 5000.0), True, False, False)
            vec = [v for v in (r if isinstance(r, tuple) else ()) if isinstance(v, unreal.Vector)]
            if len(vec) >= 2 and (r[0] is True or not isinstance(r[0], bool)):
                out.append({"z": round(vec[0].z, 1), "nz": round(vec[1].z, 2), "a": "Terrain", "f": "V2/Terrain", "m": "", "k": "terrain"})
                break
        return out

    def item(self, rec, m, xf, self_actor, self_comp, self_item):
        bb = m.get_bounding_box()
        lo, hi = bb.min, bb.max
        corners = [xf.transform_location(unreal.Vector(x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
        zs = [c.z for c in corners]
        rec["h"] = round(max(zs) - min(zs), 1)
        rec["top"] = round(max(zs), 1)
        foot = [xf.transform_location(unreal.Vector(x, y, lo.z)) for x, y in ((lo.x, lo.y), (hi.x, lo.y), (hi.x, hi.y), (lo.x, hi.y))]
        rec["foot"] = [[round(p.x, 1), round(p.y, 1)] for p in foot]
        rec["sx"] = round(abs(hi.x - lo.x) * abs(xf.scale3d.x), 1)
        rec["sy"] = round(abs(hi.y - lo.y) * abs(xf.scale3d.y), 1)
        tilt = xf.rotation.rotator()
        rec["tilt"] = round(max(abs(tilt.pitch), abs(tilt.roll)), 1)
        smp = []
        for u, v in SAMPLES:
            lp = unreal.Vector(lo.x + (hi.x - lo.x) * u, lo.y + (hi.y - lo.y) * v, lo.z)
            wp = xf.transform_location(lp)
            smp.append({"x": round(wp.x, 1), "y": round(wp.y, 1), "bz": round(wp.z, 1),
                        "s": self.surfaces(wp.x, wp.y, wp.z, self_actor, self_comp, self_item, max(zs))})
        rec["smp"] = smp
        return rec


def game_world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if w.get_name() == MAPNAME and unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            continue
    return None


def main(world):
    S = Sampler(world)
    items, classified = [], {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        cls = a.get_class().get_name()
        f = folder_of(a)
        if f in ("", "None"):
            # spawned at runtime by game systems (chore spots, pickups, pawns): not level content
            classified["runtime:" + cls] = classified.get("runtime:" + cls, 0) + 1
            continue
        if cls in ("KGGrassField", "KGDoor", "KGLadder", "PointLight", "SpotLight", "AmbientSound", "PlayerStart",
                   "TargetPoint", "CameraActor", "KGTaskStation", "KGFishSchool", "NavMeshBoundsVolume", "RecastNavMesh"):
            classified[cls] = classified.get(cls, 0) + 1
            continue
        if skip_folder(f):
            key = "skip:" + "/".join(f.split("/")[:2])
            classified[key] = classified.get(key, 0) + 1
            continue
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
        if cls == "KGFoliageField":
            for c in comps:
                if not isinstance(c, unreal.InstancedStaticMeshComponent) or not c.static_mesh:
                    continue
                n = c.get_instance_count()
                col = c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
                for i in range(n):
                    xf = c.get_instance_transform(i, True)
                    rec = {"src": "inst", "a": a.get_actor_label(), "f": f, "m": mesh_name(c.static_mesh), "i": i,
                           "c": bool(col), "k": cls}
                    items.append(S.item(rec, c.static_mesh, xf, a, c, i))
            continue
        comp = next((c for c in comps if c.static_mesh), None)
        if comp is None:
            classified["no mesh:" + cls] = classified.get("no mesh:" + cls, 0) + 1
            continue
        col = a.get_actor_enable_collision() and comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
        rec = {"src": "actor", "a": a.get_actor_label(), "f": f, "m": mesh_name(comp.static_mesh), "c": bool(col),
               "k": cls, "hid": bool(a.is_hidden_ed()) or bool(getattr(a, "hidden", False))}
        try:
            rec["hid"] = rec["hid"] or bool(a.get_editor_property("hidden"))
        except Exception:
            pass
        items.append(S.item(rec, comp.static_mesh, comp.get_world_transform(), a, comp, None))
    json.dump({"level": LEVEL, "secs": round(time.time() - T0, 1), "traces": S.n_traces, "classified": classified,
               "items": items}, open(OUT, "w"))
    log(f"{len(items)} items, {S.n_traces} traces, classified {json.dumps(classified)} -> {OUT} in {time.time() - T0:.0f}s")


class Runner:
    """Waits for the game world (+ a few seconds of streaming), samples, quits."""

    def __init__(self):
        self.t0 = time.time()
        self.world = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, dt):
        now = time.time()
        if now - self.t0 < 6.0:
            return
        self.world = self.world or game_world()
        if not self.world:
            if now - self.t0 > 180:
                log("no game world - giving up")
                self.finish()
            return
        if now - self.t0 < 12.0:
            return
        try:
            main(self.world)
        except Exception:
            import traceback
            log("error " + traceback.format_exc())
        self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        unreal.SystemLibrary.quit_game(self.world, None, unreal.QuitPreference.QUIT, False)


RUNNER = Runner()
