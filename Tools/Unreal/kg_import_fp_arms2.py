"""Import the stylised first-person arms v2 (Tools/Blender/kg_make_fp_arms2.py) - live-safe, NEW assets only.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_fp_arms2.py --timeout 900
  # editor closed, re-import some clips only (mesh + skeleton untouched):
  UnrealEditor-Cmd KillGodot.uproject -run=PythonScript -Script="Tools/Unreal/kg_import_fp_arms2.py --only knife_inspect"

Source: Art/Packed/FPArms2/SK_KG_FPArms2.glb (mesh + rig, bind pose) and one glb per clip (fp_arms_<clip>.glb,
Blender 5 merges slotted actions when several are exported together). Everything lands in
/Game/KillGodot/Characters/FPArms2/: SK_KG_FPArms2 (+ SK_KG_FPArms2_Skeleton), M_KG_FPArms2 and
Anims/A_FP2_<clip>, all clips on the ONE skeleton. Interchange glTF with an explicit pipeline (no scene/type
subfolders, keep the authored normals, vertex colours replace, no physics asset, no glTF materials).

Rig contract (camera space, the rig root IS the eye): attach the arms mesh to the first-person camera with an
identity transform. `camera` bone = root = (0,0,0), X forward / Z up. `weapon_r`/`weapon_l`: X = blade tip
direction, Z = blade spine, origin = handle grip point. Hunters knife (SM_KG_Hunters_Knife) on weapon_r:
see the KG_FP2 log lines printed at the end (relative transform + scale).
"""
import sys

import unreal

SRC = "D:/Kill Godot/Art/Packed/FPArms2"
DEST = "/Game/KillGodot/Characters/FPArms2"
ANIMS = DEST + "/Anims"
MESH = "SK_KG_FPArms2"
CLIPS = {  # clip: loops
    "knife_idle": True, "knife_draw": False, "knife_slash_a": False, "knife_slash_b": False, "knife_stab": False,
    "knife_inspect": False, "fists_idle": True, "punch_r": False, "punch_l": False, "reach_grab": False,
    "carry_idle": True, "hands_idle": True, "empty_hidden": False,
    "emote_wave": False, "emote_point": False, "emote_clap": False, "emote_salute": False,
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"KG_FP2: {msg}")
    print(f"KG_FP2: {msg}")


def pipeline(asset_name, skeleton=None):
    p = unreal.InterchangeGenericAssetsPipeline()
    p.set_editor_property("use_source_name_for_asset", True)
    p.set_editor_property("asset_name", asset_name)
    for prop, val in (("scene_name_sub_folder", False), ("asset_type_sub_folders", False)):
        try:
            p.set_editor_property(prop, val)
        except Exception as e:  # noqa: BLE001
            log(f"pipeline prop {prop}: {e}")
    cm = p.get_editor_property("common_meshes_properties")
    cm.set_editor_property("recompute_normals", False)       # keep the SDF-gradient normals
    cm.set_editor_property("recompute_tangents", True)
    cm.set_editor_property("vertex_color_import_option", unreal.InterchangeVertexColorImportOption.IVCIO_REPLACE)
    cs = p.get_editor_property("common_skeletal_meshes_and_animations_properties")
    mp = p.get_editor_property("mesh_pipeline")
    mp.set_editor_property("import_static_meshes", False)
    mp.set_editor_property("create_physics_asset", False)
    mp.set_editor_property("import_morph_targets", False)
    p.get_editor_property("material_pipeline").set_editor_property("import_materials", False)
    ap = p.get_editor_property("animation_pipeline")
    if skeleton is None:
        mp.set_editor_property("import_skeletal_meshes", True)
        ap.set_editor_property("import_animations", False)
    else:
        cs.set_editor_property("import_only_animations", True)
        cs.set_editor_property("skeleton", skeleton)
        ap.set_editor_property("import_animations", True)
        ap.set_editor_property("custom_bone_animation_sample_rate", 30)
    return p


def run_import(glb, dest, pipe):
    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(pipe)
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", glb)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", stack)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


def material():
    path = f"{DEST}/M_KG_FPArms2"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_KG_FPArms2", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -700, 0)
    sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, -40)
    mel.connect_material_expressions(vc, "", sq, "A")         # sRGB vertex colour -> ~linear (x^2)
    mel.connect_material_expressions(vc, "", sq, "B")
    mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(vc, "A", unreal.MaterialProperty.MP_ROUGHNESS)   # alpha = roughness
    spec = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -450, 220)
    spec.set_editor_property("r", 0.35)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def find(folder, cls):
    out = []
    for p in eal.list_assets(folder, recursive=False, include_folder=False):
        a = unreal.load_asset(p)
        if isinstance(a, cls):
            out.append(a)
    return out


def only_clips():
    """--only a,b (commandlet -Script args): re-import just these clips onto the existing skeleton."""
    argv = list(sys.argv)
    if "--only" in argv and argv.index("--only") + 1 < len(argv):
        return [c for c in argv[argv.index("--only") + 1].split(",") if c]
    return []


def reimport_clips(names):
    mesh = unreal.load_asset(f"{DEST}/{MESH}")
    if not mesh:
        raise RuntimeError("KG_FP2: no existing mesh; run the full import first")
    skeleton = mesh.get_editor_property("skeleton")
    report = []
    for clip in names:
        name = f"A_FP2_{clip}"
        run_import(f"{SRC}/fp_arms_{clip}.glb", ANIMS, pipeline(name, skeleton))
        seq = unreal.load_asset(f"{ANIMS}/{name}")
        if not isinstance(seq, unreal.AnimSequence):
            report.append(f"{clip}:MISSING")
            continue
        eal.save_loaded_asset(seq)
        report.append(f"{name}:{seq.get_play_length():.3f}s skeleton {seq.get_editor_property('skeleton').get_name()}")
    log("reimported " + ", ".join(report))


def main():
    if only_clips():
        reimport_clips(only_clips())
        return
    # 1) mesh + skeleton
    run_import(f"{SRC}/{MESH}.glb", DEST, pipeline(MESH))
    meshes = [m for m in find(DEST, unreal.SkeletalMesh) if m.get_name() == MESH] or find(DEST, unreal.SkeletalMesh)
    if not meshes:
        raise RuntimeError("KG_FP2: skeletal mesh import failed")
    mesh = meshes[0]
    skeleton = mesh.get_editor_property("skeleton")
    log(f"mesh {mesh.get_path_name()} skeleton {skeleton.get_path_name()} bounds {mesh.get_bounds().box_extent}")
    # 2) material
    mat = material()
    slots = []
    for s in mesh.get_editor_property("materials"):
        s.set_editor_property("material_interface", mat)
        slots.append(s)
    mesh.set_editor_property("materials", slots)
    eal.save_loaded_asset(mesh)
    # 3) clips on the same skeleton
    report = []
    for clip, loop in CLIPS.items():
        name = f"A_FP2_{clip}"
        run_import(f"{SRC}/fp_arms_{clip}.glb", ANIMS, pipeline(name, skeleton))
        seq = unreal.load_asset(f"{ANIMS}/{name}")
        if not isinstance(seq, unreal.AnimSequence):
            stray = [a.get_name() for a in find(ANIMS, unreal.AnimSequence)]
            report.append(f"{clip}:MISSING (anims in folder: {stray})")
            continue
        try:
            seq.set_editor_property("b_loop", loop)
        except Exception:  # noqa: BLE001
            pass
        eal.save_loaded_asset(seq)
        report.append(f"{name}:{seq.get_play_length():.3f}s{' loop' if loop else ''}")
    log("anims " + ", ".join(report))
    eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    verify(skeleton)


def fmt(v):
    return f"({v.x:.2f}, {v.y:.2f}, {v.z:.2f})"


def verify(skeleton):
    """Bone names + the transforms the director needs (component space, cm)."""
    ape = unreal.AnimPoseExtensions
    ref = ape.get_ref_pose(skeleton) if hasattr(ape, "get_ref_pose") else ape.get_reference_pose(skeleton)
    names = [str(n) for n in ape.get_bone_names(ref)]
    log(f"bones ({len(names)}): {names}")
    for b in ("root", "camera", "hand_r", "weapon_r", "weapon_l"):
        if b in names:
            t = ape.get_bone_pose(ref, b, unreal.AnimPoseSpaces.WORLD)
            r = t.rotation
            log(f"ref {b}: loc {fmt(t.translation)} X {fmt(r.get_axis_x())} Z {fmt(r.get_axis_z())}")
    opts = unreal.AnimPoseEvaluationOptions()
    for clip, times in (("knife_idle", (0.0,)), ("knife_stab", (0.27,)), ("fists_idle", (0.0,))):
        seq = unreal.load_asset(f"{ANIMS}/A_FP2_{clip}")
        if not seq:
            continue
        for tm in times:
            pose = ape.get_anim_pose_at_time(seq, tm, opts)
            for b in ("hand_r", "weapon_r", "camera"):
                t = ape.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD)
                r = t.rotation
                log(f"{clip}@{tm:.2f} {b}: loc {fmt(t.translation)} X(blade) {fmt(r.get_axis_x())} "
                    f"Z(spine) {fmt(r.get_axis_z())}")
    knife = unreal.load_asset("/Game/KillGodot/Items/Melee/Hunters_Knife_a2avVUVeYD/StaticMeshes/SM_KG_Hunters_Knife")
    if knife:
        log(f"knife bounds origin {fmt(knife.get_bounds().origin)} extent {fmt(knife.get_bounds().box_extent)}")


main()
