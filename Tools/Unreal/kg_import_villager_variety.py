"""SPRINT-027a villager variety: NEW asset paths under /Game/KillGodot/Characters/VillagerVariety (headless, live-safe,
never touches maps or the shipped villager meshes):

  Bodies/SK_KG_Villager_{MR,FR,MX,FX,MB,FB}   Tools/Blender/kg_build_villager_variants.py, on the shipped villager skeleton
  SM_KG_Hair_*                                UBC hairstyles as static head attachments (bind pose, identity placement)
  Gear/.../SM_KG_Gear_*                       Tools/Blender/kg_make_villager_gear.py (hats, cape, apron)
  Bands/.../SM_KG_Role{Sash,Cuff}             Tools/Blender/kg_make_role_bands.py
  Materials/MI_KG_Var_*                       M_KG_Character instances (runtime MIDs tint "Tint" per player)
  Materials/M_KG_RoleBand                     flat "Tint" (+ "Glow") for the gear and bands

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_villager_variety.py"

The script ends with a read-only dump of the villager material slots (what the runtime keyword rules match).
"""
import os
import sys

import unreal

SRC = "D:/Kill Godot/Art/Source"
UBC = os.path.join(SRC, "Quaternius_UniversalBaseCharacters", "Universal Base Characters[Standard]",
                   "Universal Base Characters[Standard]")
HAIR = os.path.join(UBC, "Hairstyles", "Origin at 0", "glTF (Godot)")
BANDS_GLB = "D:/Kill Godot/Art/Packed/KG_RoleBands.glb"
GEAR_GLB = "D:/Kill Godot/Art/Packed/KG_VillagerGear.glb"
BODIES_DIR = "D:/Kill Godot/Art/Export/VillagerVariety"
BODIES = ["SK_KG_Villager_MR", "SK_KG_Villager_FR", "SK_KG_Villager_MX", "SK_KG_Villager_FX",
          "SK_KG_Villager_MB", "SK_KG_Villager_FB"]

DEST = "/Game/KillGodot/Characters/VillagerVariety"
MAT_DEST = f"{DEST}/Materials"
TEX_DEST = f"{DEST}/Textures"
MASTER = "/Game/KillGodot/Characters/Villager/Materials/M_KG_Character"
VTEX = "/Game/KillGodot/Characters/Villager/Textures"
HAIR_TEX = f"{VTEX}/T_Hair_1_BaseColor"
SKELETON = "/Game/KillGodot/Characters/Villager/SK_KG_Villager_M_Skeleton"
RANGER_PNG = os.path.join(SRC, "Quaternius_ModularOutfitsFantasy", "Modular Character Outfits - Fantasy[Standard]",
                          "Modular Character Outfits - Fantasy[Standard]", "Textures", "Ranger", "T_Ranger_BaseColor.png")

HAIRSTYLES = {
    "SM_KG_Hair_Buns": "Hair_Buns.gltf",
    "SM_KG_Hair_Buzzed": "Hair_Buzzed.gltf",
    "SM_KG_Hair_BuzzedFemale": "Hair_BuzzedFemale.gltf",
    "SM_KG_Hair_Long": "Hair_Long.gltf",
    "SM_KG_Hair_SimpleParted": "Hair_SimpleParted.gltf",
}
# slot keyword -> texture (male, female). Peasant/Ranger/skin slots are tinted per player at runtime.
SLOT_RULES = [("Hair", (f"{VTEX}/T_Hair_1_BaseColor", f"{VTEX}/T_Hair_2_BaseColor")),
              ("Eyes", (f"{VTEX}/T_Eye_Brown", f"{VTEX}/T_Eye_Brown")),
              ("Ranger", (f"{TEX_DEST}/T_Ranger_BaseColor", f"{TEX_DEST}/T_Ranger_BaseColor")),
              ("Peasant", (f"{VTEX}/T_Peasant_BaseColor", f"{VTEX}/T_Peasant_2_BaseColor")),
              ("Superhero", (f"{VTEX}/T_Superhero_Male_Ligh", f"{VTEX}/T_Superhero_Female_Light_BaseColor")),
              ("Regular", (f"{VTEX}/T_Superhero_Male_Ligh", f"{VTEX}/T_Superhero_Female_Light_BaseColor"))]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"KG_VARIETY: {msg}")


def fmt(v):
    return f"({v.x:.1f}, {v.y:.1f}, {v.z:.1f})"


def import_task(src, folder, name=None, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src.replace("\\", "/"))
    t.set_editor_property("destination_path", folder)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    if options is not None:
        t.set_editor_property("options", options)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


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


def import_static(src, folder, combine=True):
    """combine: one static mesh (hair gltf). Otherwise the default pipeline: one mesh per object (gear/band GLBs)."""
    if not os.path.isfile(src):
        log(f"source missing {src}")
        return []
    options = None
    if combine:
        options = unreal.InterchangePipelineStackOverride()
        options.add_pipeline(static_pipeline())
    paths = import_task(src, folder, options=options)
    meshes = [p for p in paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        meshes = [p for p in eal.list_assets(folder, recursive=True, include_folder=False)
                  if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    log(f"imported {os.path.basename(src)} -> {meshes}")
    return [unreal.load_asset(p) for p in meshes]


def make_instance(name, texture_path):
    path = f"{MAT_DEST}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, MAT_DEST, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(MASTER))
    tex = unreal.load_asset(texture_path)
    if tex:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColor", tex)
    else:
        log(f"texture missing {texture_path}")
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(r=1.0, g=1.0, b=1.0, a=1.0))
    eal.save_loaded_asset(mi)
    return mi


def band_material():
    path = f"{MAT_DEST}/M_KG_RoleBand"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_KG_RoleBand", MAT_DEST, unreal.Material, unreal.MaterialFactoryNew())
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1.0))
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 200)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 0.0)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 120)
    mel.connect_material_expressions(tint, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 300)
    rough.set_editor_property("r", 0.7)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def import_bodies():
    if os.path.isfile(RANGER_PNG) and not eal.does_asset_exist(f"{TEX_DEST}/T_Ranger_BaseColor"):
        import_task(RANGER_PNG, TEX_DEST, "T_Ranger_BaseColor")
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    skeleton = unreal.load_asset(SKELETON)
    folder = f"{DEST}/Bodies"
    for name in BODIES:
        fbx = os.path.join(BODIES_DIR, f"{name}.fbx")
        if not os.path.isfile(fbx):
            log(f"{name}: fbx missing")
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("create_physics_asset", False)
        ui.set_editor_property("skeleton", skeleton)
        import_task(fbx, folder, name, ui)
        mesh = unreal.load_asset(f"{folder}/{name}")
        if not mesh:
            log(f"{name}: IMPORT FAILED")
            continue
        female = name[-2] == "F"
        sex = "F" if female else "M"
        slots = []
        report = []
        for slot in mesh.get_editor_property("materials"):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            rule = next((r for r in SLOT_RULES if r[0].lower() in slot_name.lower()), None)
            if rule:
                key, (tex_m, tex_f) = rule
                slot.set_editor_property("material_interface", make_instance(f"MI_KG_Var_{key}_{sex}", tex_f if female else tex_m))
                report.append(f"{slot_name}->{key}")
            else:
                report.append(f"{slot_name}->(unchanged)")
            slots.append(slot)
        mesh.set_editor_property("materials", slots)
        eal.save_loaded_asset(mesh)
        b = mesh.get_bounds()
        skel = mesh.get_editor_property("skeleton")
        log(f"BODY {name}: {report} skeleton={skel.get_name() if skel else None} extent={fmt(b.box_extent)}")


def dump_villagers():
    for name in ("SK_KG_Villager_M", "SK_KG_Villager_F"):
        mesh = unreal.load_asset(f"/Game/KillGodot/Characters/Villager/{name}")
        if not mesh:
            log(f"{name}: MISSING")
            continue
        slots = [str(s.get_editor_property("material_slot_name")) for s in mesh.get_editor_property("materials")]
        b = mesh.get_bounds()
        log(f"{name}: slots={slots} bounds origin={fmt(b.origin)} extent={fmt(b.box_extent)}")


def main():
    if "--gear" in " ".join(sys.argv):      # quick re-import of the Blender gear/bands only
        band_mat = band_material()
        for mesh in (import_static(BANDS_GLB, f"{DEST}/Bands", combine=False) +
                     import_static(GEAR_GLB, f"{DEST}/Gear", combine=False)):
            for i in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(i, band_mat)
            eal.save_loaded_asset(mesh)
            bb = mesh.get_bounding_box()
            log(f"RESULT gear path={mesh.get_path_name()} min={fmt(bb.min)} max={fmt(bb.max)}")
        log("DONE")
        return
    hair_mi = make_instance("MI_KG_Var_Hair", HAIR_TEX)
    for name, gltf in HAIRSTYLES.items():
        for mesh in import_static(os.path.join(HAIR, gltf), f"{DEST}/{name}"):
            for i in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(i, hair_mi)
            eal.save_loaded_asset(mesh)
            bb = mesh.get_bounding_box()
            log(f"RESULT {name} path={mesh.get_path_name()} min={fmt(bb.min)} max={fmt(bb.max)}")
    import_bodies()
    band_mat = band_material()
    for mesh in (import_static(BANDS_GLB, f"{DEST}/Bands", combine=False) +
                 import_static(GEAR_GLB, f"{DEST}/Gear", combine=False)):
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, band_mat)
        ns = mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", ns)
        eal.save_loaded_asset(mesh)
        bb = mesh.get_bounding_box()
        log(f"RESULT gear path={mesh.get_path_name()} min={fmt(bb.min)} max={fmt(bb.max)}")
    eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    dump_villagers()
    log("DONE")


main()
