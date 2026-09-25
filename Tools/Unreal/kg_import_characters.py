"""Import the puppet FBX exports (Tools/Blender/kg_export_characters.py) and wire their two material slots.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_characters.py"
"""
import os

import unreal

EXPORT_DIR = "D:/Kill Godot/Art/Export"
DEST = "/Game/KillGodot/Characters/Puppet"
MAT_DIR = "/Game/KillGodot/Materials"

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"KG_IMPORT: {msg}")


def new_material(name):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)
    return mat


def build_materials():
    # Body: palette baked into sRGB vertex colours -> linearise (pow 2.2) -> base colour. Toon ramp comes in M9.
    body = new_material("M_KG_PuppetBody")
    vc = mel.create_material_expression(body, unreal.MaterialExpressionVertexColor, -500, 0)
    pw = mel.create_material_expression(body, unreal.MaterialExpressionPower, -250, 0)
    pw.set_editor_property("const_exponent", 2.2)
    mel.connect_material_expressions(vc, "", pw, "Base")
    mel.connect_material_property(pw, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(body, unreal.MaterialExpressionConstant, -250, 200)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(body)

    # Outline: inverted hull, unlit ink colour, single-sided so the flipped hull only shows at silhouettes.
    outline = new_material("M_KG_Outline")
    outline.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    ink = mel.create_material_expression(outline, unreal.MaterialExpressionConstant3Vector, -300, 0)
    ink.set_editor_property("constant", unreal.LinearColor(0.004, 0.003, 0.01, 1.0))
    mel.connect_material_property(ink, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(outline)
    for m in (body, outline):
        eal.save_loaded_asset(m)
    return {"M_KG_PuppetBody": body, "M_KG_Outline": outline}


def import_skeletal(fbx, name, physics):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(EXPORT_DIR, fbx))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", physics)
    data = ui.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property("import_morph_targets", False)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    path = f"{DEST}/{name}"
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"{name}: expected SkeletalMesh, got {type(mesh)}")
    return mesh


def assign(mesh, mats):
    slots = []
    for slot in mesh.get_editor_property("materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        if name in mats:
            slot.set_editor_property("material_interface", mats[name])
        slots.append(slot)
    mesh.set_editor_property("materials", slots)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    log(f"{mesh.get_name()} slots={[str(s.get_editor_property('material_slot_name')) for s in slots]} "
        f"extent={b.box_extent} origin={b.origin}")


def main():
    # Use the legacy FBX importer so FbxImportUI options are honoured.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    mats = build_materials()
    assign(import_skeletal("SK_KG_FP_Arms.fbx", "SK_KG_FP_Arms", False), mats)
    assign(import_skeletal("SK_KG_Puppet_Tilly.fbx", "SK_KG_Puppet_Tilly", True), mats)
    log("done")


main()
