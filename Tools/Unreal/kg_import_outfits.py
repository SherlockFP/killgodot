"""Cosmetics: import the Quaternius head/shoulder accessories as STATIC meshes into NEW asset paths and give them
vibrant M_KG_Character instances. Headless (safe while the user's editor is open; never touches maps or existing
assets, only /Game/KillGodot/Cosmetics/...):

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_outfits.py"
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_outfits.py --bones"

--bones is read-only: prints the villager skeleton (bone names + component-space reference positions) and the bounds of
the props used as novelty hats, which is what UKGCosmeticCatalog's attach offsets are derived from.

The free "Modular Character Outfits - Fantasy [Standard]" pack only ships two hoods and two pauldron sets as separate
parts (no hats/capes), so the Universal Base Characters beard is imported as well. The skinned parts are baked in
their bind pose (Interchange ForceAllMeshAsType = static); UKGCosmeticsComponent attaches them to the head/spine bone
with the inverse of that bone's reference pose, so they sit exactly where the artist modelled them.
Folder names contain "[Standard]" brackets: paths are built with os.listdir/os.path, never glob.
"""
import os
import sys

import unreal

SRC = "D:/Kill Godot/Art/Source"
OUTFITS = os.path.join(SRC, "Quaternius_ModularOutfitsFantasy", "Modular Character Outfits - Fantasy[Standard]",
                       "Modular Character Outfits - Fantasy[Standard]")
PARTS = os.path.join(OUTFITS, "Exports", "glTF (Godot-Unreal)", "Modular Parts")
OUTFIT_TEX = os.path.join(OUTFITS, "Textures")
UBC = os.path.join(SRC, "Quaternius_UniversalBaseCharacters", "Universal Base Characters[Standard]",
                   "Universal Base Characters[Standard]")
HAIR = os.path.join(UBC, "Hairstyles", "Origin at 0", "glTF (Godot)")

DEST = "/Game/KillGodot/Cosmetics"
MESH_DEST = f"{DEST}/Outfits"
TEX_DEST = f"{DEST}/Textures"
MAT_DEST = f"{DEST}/Materials"
MASTER = "/Game/KillGodot/Characters/Villager/Materials/M_KG_Character"
HAIR_TEX = "/Game/KillGodot/Characters/Villager/Textures/T_Hair_1_BaseColor"
VILLAGER = "/Game/KillGodot/Characters/Villager/SK_KG_Villager_M"

# asset name -> (source dir, source file, texture key)
PIECES = {
    "SM_KG_Cos_Hood_M": (PARTS, "Male_Ranger_Head_Hood.gltf", "Ranger"),
    "SM_KG_Cos_Hood_F": (PARTS, "Female_Ranger_Head_Hood.gltf", "Ranger"),
    "SM_KG_Cos_Pauldron_M": (PARTS, "Male_Ranger_Acc_Pauldron.gltf", "Ranger"),
    "SM_KG_Cos_Pauldrons_F": (PARTS, "Female_Ranger_Acc_Pauldrons.gltf", "Ranger"),
    "SM_KG_Cos_Beard": (HAIR, "Hair_Beard.gltf", "Hair"),
}
# Novelty hats reuse existing props (read-only here; the catalog references them directly).
PROP_HATS = ["/Game/KillGodot/Env/KG_Props/StaticMeshes/Pot_1", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Bucket_Wooden_1",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Candle_1", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Mug",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Chalice", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Pot_1_Lid",
             "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Fish_GoldenCarp",
             "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Fish_Mackerel",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Chair_1", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Bench",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Stool", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Bag",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Pouch_Large",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Shield_Wooden",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Coin", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Key_Metal",
             "/Game/KillGodot/Env/KG_Props/StaticMeshes/Scroll_1", "/Game/KillGodot/Env/KG_Props/StaticMeshes/Rope_1"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"KG_OUTFITS: {msg}")


def fmt(v):
    return f"({v.x:.1f}, {v.y:.1f}, {v.z:.1f})"


def dump_bones():
    mesh = unreal.load_asset(VILLAGER)
    if not mesh:
        log(f"missing {VILLAGER}")
        return
    skeleton = mesh.get_editor_property("skeleton")
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
    log(f"skeleton {skeleton.get_path_name()} bones={len(names)}")
    for n in names:
        t = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD)
        r = t.rotation.rotator()
        log(f"  bone {n}: loc={fmt(t.translation)} rot(p,y,r)=({r.pitch:.1f}, {r.yaw:.1f}, {r.roll:.1f}) "
            f"scale={fmt(t.scale3d)}")
    try:
        sockets = [str(mesh.get_socket_by_index(i).get_editor_property("socket_name")) for i in range(mesh.num_sockets())]
    except Exception as exc:  # noqa: BLE001 - diagnostics only
        sockets = [f"<unreadable: {exc}>"]
    log(f"mesh sockets: {sockets}")
    b = mesh.get_bounds()
    log(f"villager bounds origin={fmt(b.origin)} extent={fmt(b.box_extent)}")
    # Seat tuning: where the pelvis/head sit in the sitting clip vs the idle clip (component space, cm).
    for anim_name in ("A_KG_Idle_Loop", "A_KG_Sitting_Idle_Loop", "A_KG_Sitting_Enter"):
        anim = unreal.load_asset(f"/Game/KillGodot/Characters/Villager/Anims/{anim_name}")
        if not anim:
            log(f"anim {anim_name}: MISSING")
            continue
        try:
            opts = unreal.AnimPoseEvaluationOptions()
            opts.set_editor_property("optional_skeletal_mesh", mesh)
            t = min(0.5, anim.get_play_length() * 0.5)
            pose_a = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, opts)
            parts = []
            for bone in ("root", "pelvis", "Head", "foot_l", "ball_l"):
                bt = unreal.AnimPoseExtensions.get_bone_pose(pose_a, bone, unreal.AnimPoseSpaces.WORLD)
                parts.append(f"{bone}={fmt(bt.translation)}")
            log(f"anim {anim_name} t={t:.2f}s len={anim.get_play_length():.2f}: " + " ".join(parts))
        except Exception as exc:  # noqa: BLE001 - diagnostics only
            log(f"anim {anim_name}: pose eval failed {exc}")
    for path in PROP_HATS:
        m = unreal.load_asset(path)
        if m:
            bb = m.get_bounding_box()
            log(f"prop {path}: min={fmt(bb.min)} max={fmt(bb.max)}")
        else:
            log(f"prop {path}: MISSING")


def import_texture(src, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", TEX_DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(f"{TEX_DEST}/{name}")
    if tex:
        tex.set_editor_property("max_texture_size", 1024)   # 4K atlas is overkill for a hood
        eal.save_loaded_asset(tex)
    return tex


def make_instance(name, texture, tint):
    path = f"{MAT_DEST}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, MAT_DEST, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MASTER))
    if texture:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColor", texture)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(r=tint[0], g=tint[1], b=tint[2], a=1.0))
    eal.save_loaded_asset(mi)
    return mi


def static_pipeline():
    pipe = unreal.InterchangeGenericAssetsPipeline()
    pipe.get_editor_property("common_meshes_properties").set_editor_property(
        "force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    mesh = pipe.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", True)
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
    mesh.set_editor_property("collision", False)
    mesh.set_editor_property("build_nanite", False)
    pipe.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
    mat = pipe.get_editor_property("material_pipeline")
    mat.set_editor_property("import_materials", False)
    mat.get_editor_property("texture_pipeline").set_editor_property("import_textures", False)
    return pipe


def import_piece(name, src_dir, src_file):
    src = os.path.join(src_dir, src_file)
    if not os.path.isfile(src):
        log(f"{name}: source missing {src}")
        return None
    folder = f"{MESH_DEST}/{name}"
    pipe = static_pipeline()
    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(pipe)
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src.replace("\\", "/"))
    t.set_editor_property("destination_path", folder)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", stack)
    tools.import_asset_tasks([t])
    paths = [str(p) for p in t.get_editor_property("imported_object_paths")]
    meshes = [p for p in paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        # Some translators report nothing through the task; fall back to listing the folder.
        meshes = [p for p in eal.list_assets(folder, recursive=True, include_folder=False)
                  if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    log(f"{name}: imported {paths} -> static meshes {meshes}")
    return unreal.load_asset(meshes[0]) if meshes else None


def main_import():
    ranger_png = os.path.join(OUTFIT_TEX, "Ranger", "T_Ranger_BaseColor.png")
    textures = {"Ranger": import_texture(ranger_png, "T_KG_Cos_Ranger_BaseColor") if os.path.isfile(ranger_png) else None,
                "Hair": unreal.load_asset(HAIR_TEX)}
    instances = {"Ranger": make_instance("MI_KG_Cos_Ranger", textures["Ranger"], (1.0, 1.0, 1.0)),
                 "Hair": make_instance("MI_KG_Cos_Beard", textures["Hair"], (0.55, 0.26, 0.12))}
    for name, (src_dir, src_file, tex_key) in PIECES.items():
        mesh = import_piece(name, src_dir, src_file)
        if not mesh:
            continue
        mi = instances[tex_key]
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mi)
        eal.save_loaded_asset(mesh)
        bb = mesh.get_bounding_box()
        log(f"RESULT {name} path={mesh.get_path_name()} min={fmt(bb.min)} max={fmt(bb.max)} "
            f"slots={len(mesh.get_editor_property('static_materials'))}")
    dump_bones()


if "--bones" in sys.argv or any("--bones" in a for a in sys.argv):
    dump_bones()
else:
    main_import()
