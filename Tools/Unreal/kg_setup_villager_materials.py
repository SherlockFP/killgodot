"""Villager materials: import Quaternius base-colour textures, build M_KG_Character (tint + saturation boost for the
vibrant look), make one instance per mesh slot and assign it.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_setup_villager_materials.py"
"""
import glob
import os

import unreal

SRC = "D:/Kill Godot/Art/Source"
DEST = "/Game/KillGodot/Characters/Villager"
TEX = f"{DEST}/Textures"
MAT = f"{DEST}/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

WANTED = ["T_Hair_1_BaseColor", "T_Hair_2_BaseColor", "T_Eye_Brown", "T_Peasant_BaseColor", "T_Peasant_2_BaseColor",
          "T_Superhero_Male_Ligh", "T_Superhero_Female_Light_BaseColor", "T_Regular_Male_Dark_BaseColor",
          "T_Regular_Female_Dark_BaseColor"]

# slot-name keyword -> (texture, tint (linear), saturation boost)
MALE_RULES = [("Hair", "T_Hair_1_BaseColor", (0.55, 0.26, 0.12)), ("Eyes", "T_Eye_Brown", (1, 1, 1)),
              ("Superhero_Male", "T_Superhero_Male_Ligh", (1, 1, 1)), ("Peasant", "T_Peasant_BaseColor", (1, 1, 1)),
              ("Regular_Male", "T_Superhero_Male_Ligh", (1, 1, 1))]
FEMALE_RULES = [("Hair", "T_Hair_2_BaseColor", (0.95, 0.35, 0.12)), ("Eyes", "T_Eye_Brown", (1, 1, 1)),
                ("Superhero_Female", "T_Superhero_Female_Light_BaseColor", (1, 1, 1)),
                ("Peasant", "T_Peasant_2_BaseColor", (1, 1, 1)),
                ("Regular_Female", "T_Superhero_Female_Light_BaseColor", (1, 1, 1))]


def log(msg):
    unreal.log(f"KG_MAT: {msg}")


def import_textures():
    found = {}
    for path in glob.glob(f"{SRC}/Quaternius_*/**/*.png", recursive=True):
        name = os.path.splitext(os.path.basename(path))[0]
        if name in WANTED and name not in found:
            found[name] = path
    for name, path in found.items():
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", path)
        t.set_editor_property("destination_path", TEX)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tools.import_asset_tasks([t])
    log(f"textures: {sorted(found)}")
    return {n: unreal.load_asset(f"{TEX}/{n}") for n in found}


def build_master():
    path = f"{MAT}/M_KG_Character"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_KG_Character", MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
    tex.set_editor_property("parameter_name", "BaseColor")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse"))
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 250)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 100)
    mel.connect_material_expressions(tex, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    sat = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 300)
    sat.set_editor_property("parameter_name", "Desaturation")
    sat.set_editor_property("default_value", -0.3)  # negative = more saturated (vibrant art direction)
    desat = mel.create_material_expression(m, unreal.MaterialExpressionDesaturation, -350, 100)
    mel.connect_material_expressions(mul, "", desat, "")
    mel.connect_material_expressions(sat, "", desat, "Fraction")
    mel.connect_material_property(desat, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -350, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.75)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def make_instance(master, name, texture, tint):
    path = f"{MAT}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    if texture:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColor", texture)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    eal.save_loaded_asset(mi)
    return mi


def apply(mesh_name, rules, master, textures):
    mesh = unreal.load_asset(f"{DEST}/{mesh_name}")
    slots = []
    report = []
    for slot in mesh.get_editor_property("materials"):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        rule = next((r for r in rules if r[0] in slot_name), None)
        if rule:
            key, tex_name, tint = rule
            mi = make_instance(master, f"MI_KG_{mesh_name[3:]}_{slot_name.replace('MI_', '')}", textures.get(tex_name), tint)
            slot.set_editor_property("material_interface", mi)
            report.append(f"{slot_name}->{tex_name}")
        else:
            report.append(f"{slot_name}->(unchanged)")
        slots.append(slot)
    mesh.set_editor_property("materials", slots)
    eal.save_loaded_asset(mesh)
    log(f"{mesh_name}: {report}")


def main():
    textures = import_textures()
    master = build_master()
    apply("SK_KG_Villager_M", MALE_RULES, master, textures)
    apply("SK_KG_Villager_F", FEMALE_RULES, master, textures)
    apply("SK_KG_FPArms_M", MALE_RULES, master, textures)
    apply("SK_KG_FPArms_F", FEMALE_RULES, master, textures)


main()
