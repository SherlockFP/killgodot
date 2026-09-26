"""Project-owned glTF base materials with the usage flags instanced/Nanite props need, plus a usage check of the v2 map.

Why: every kit material imported through Interchange (MI_Brick, Leaves, MI_Trim_Props_Vertex, ...) is a child of
Unreal's built-in glTF material (/InterchangeAssets/gltf/M_Default via MI_Default_Opaque_DS etc.). Usage flags live on
the BASE material, and engine plugin content cannot be saved from the project, so HISM props (KGFoliageField, the dressing
C.instanced) drew the default grey checker in -game and in cooked builds ("missing usage flag InstancedStaticMeshes!
Default Material will be used in game").

Fix (idempotent):
  1. copy the engine glTF base material(s) and the intermediate glTF MIs actually used into
     /Game/KillGodot/Materials/glTF/ (M_KG_glTF_Default, MI_KG_glTF_Default_Opaque_DS, ...), set
     bUsedWithInstancedStaticMeshes + bUsedWithNanite on the base copy;
  2. reparent every /Game material instance whose parent is an /InterchangeAssets/ asset onto the copy (parameters and
     static switches are kept: same names, same graph);
  3. set the same flags on /Game base materials that the v2 HISMs / Nanite meshes use;
  4. check: every material on every (H)ISM component of L_Morrowmere_v2 has an instancing-ready base, every Nanite mesh a
     Nanite-ready base -> Saved/KG_V2_MaterialCheck.json (and "KG_MATFIX check" in the log).

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_fix_gltf_materials.py [check]" -unattended -nosplash -nopause -nullrhi
  ("check" = step 4 only)
"""
import json
import sys

import unreal

ROOT = "D:/Kill Godot"
LEVEL = "/Game/KillGodot/Maps/L_Morrowmere_v2"
DEST = "/Game/KillGodot/Materials/glTF"
REPORT = f"{ROOT}/Saved/KG_V2_MaterialCheck.json"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def log(m):
    unreal.log(f"KG_MATFIX {m}")


def pkg(obj):
    return obj.get_path_name().split(".")[0] if obj else ""


def copy_name(engine_path):
    n = engine_path.rsplit("/", 1)[1]
    return f"{DEST}/{n.replace('M_', 'M_KG_glTF_', 1) if n.startswith('M_') else n.replace('MI_', 'MI_KG_glTF_', 1)}"


def set_flags(m):
    """Usage flags on a base UMaterial; returns True if something changed."""
    changed = False
    for prop in ("used_with_instanced_static_meshes", "used_with_nanite"):
        if not m.get_editor_property(prop):
            m.set_editor_property(prop, True)
            changed = True
    return changed


_copies = {}


def project_copy(engine_asset):
    """Project-owned copy of an engine glTF material / MI (recursive for the parent chain)."""
    src = pkg(engine_asset)
    if src in _copies:
        return _copies[src]
    dst = copy_name(src)
    if eal.does_asset_exist(dst):
        obj = unreal.load_asset(dst)
    else:
        obj = eal.duplicate_asset(src, dst)
        log(f"copied {src} -> {dst}")
    if isinstance(obj, unreal.MaterialInstanceConstant):
        parent = engine_asset.get_editor_property("parent")
        if parent and pkg(parent).startswith("/InterchangeAssets/"):
            mel.set_material_instance_parent(obj, project_copy(parent))
            mel.update_material_instance(obj)
    elif isinstance(obj, unreal.Material):
        if set_flags(obj):
            mel.recompile_material(obj)
    eal.save_loaded_asset(obj)
    _copies[src] = obj
    return obj


def reparent_project_instances():
    ar_filter = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")],
                                package_paths=["/Game"], recursive_paths=True)
    moved = []
    for ad in reg.get_assets(ar_filter):
        path = str(ad.package_name)
        if path.startswith(DEST):
            continue
        mi = unreal.load_asset(path)
        parent = mi.get_editor_property("parent") if mi else None
        if not parent or not pkg(parent).startswith("/InterchangeAssets/"):
            continue
        mel.set_material_instance_parent(mi, project_copy(parent))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        moved.append(path)
    log(f"reparented {len(moved)} project MIs onto {DEST}")
    return moved


def level_components():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for a in actors:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            yield a, c


def base_of(mi):
    try:
        return mi.get_base_material()
    except Exception:
        return None


def audit(fix):
    """Every (H)ISM material needs an instancing base; every Nanite mesh a Nanite base. fix=True sets /Game flags."""
    ism, nanite, bad, fixed = {}, {}, [], []
    n_comp = n_inst = 0
    for a, c in level_components():
        mesh = c.get_editor_property("static_mesh")
        if not mesh:
            continue
        is_ism = isinstance(c, unreal.InstancedStaticMeshComponent)
        nan = bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled"))
        if not is_ism and not nan:
            continue
        if is_ism and c.get_instance_count() == 0:
            continue   # nothing is drawn with it (e.g. board tables fill their ISMs at runtime, with their own MIDs)
        if is_ism:
            n_comp += 1
            n_inst += c.get_instance_count()
        for i in range(c.get_num_materials()):
            mi = c.get_material(i)
            base = base_of(mi) if mi else None
            if not base:
                continue
            for need, flag, book in ((is_ism, "used_with_instanced_static_meshes", ism), (nan, "used_with_nanite", nanite)):
                if not need:
                    continue
                key = pkg(base)
                ok = bool(base.get_editor_property(flag))
                if not ok and fix and key.startswith("/Game/"):
                    base.set_editor_property(flag, True)
                    mel.recompile_material(base)
                    eal.save_loaded_asset(base)
                    fixed.append(f"{key}:{flag}")
                    ok = True
                rec = book.setdefault(pkg(mi), {"base": key, "ok": ok, "meshes": set()})
                rec["ok"] = rec["ok"] and ok
                rec["meshes"].add(pkg(mesh).rsplit("/", 1)[1])
                if not ok:
                    bad.append(f"{pkg(mi)} (base {key}) lacks {flag} [{pkg(mesh)} in {a.get_actor_label()}]")
    out = {"ism_components": n_comp, "ism_instances": n_inst,
           "ism_materials": {k: {**v, "meshes": sorted(v["meshes"])[:8]} for k, v in sorted(ism.items())},
           "nanite_materials": {k: {**v, "meshes": sorted(v["meshes"])[:8]} for k, v in sorted(nanite.items())},
           "fixed_flags": fixed, "problems": sorted(set(bad))}
    return out


def main():
    check_only = any(a.lower() == "check" for a in sys.argv[1:])
    moved = [] if check_only else reparent_project_instances()
    out = audit(fix=not check_only)
    out["reparented"] = moved
    out["copies"] = sorted(_copies)
    out["pass"] = not out["problems"]
    json.dump(out, open(REPORT, "w"), indent=1)
    log(f"check: {out['ism_components']} ISM components / {out['ism_instances']} instances, "
        f"{len(out['ism_materials'])} ISM materials, {len(out['nanite_materials'])} Nanite materials, "
        f"{len(out['problems'])} problems, fixed {len(out['fixed_flags'])} flags -> {'PASS' if out['pass'] else 'FAIL'}")
    for p in out["problems"][:40]:
        log(f"  problem: {p}")


main()
