"""Create small FX materials if missing (safe live: only creates new assets, never edits/deletes graphs).
  M_KG_TaskMarker    unlit glowing yellow (chore markers)
  M_KG_RevealOverlay translucent additive red fresnel shell (Lighthouse Illumination overlay)
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/KillGodot/Materials"


def make(name):
    if eal.does_asset_exist(f"{DIR}/{name}"):
        return None
    return tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())


m = make("M_KG_TaskMarker")
if m:
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    c.set_editor_property("constant", unreal.LinearColor(r=4.0, g=2.6, b=0.3, a=1.0))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

m = make("M_KG_RevealOverlay")
if m:
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -600, 0)
    fres.set_editor_property("exponent", 1.5)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -600, 200)
    c.set_editor_property("constant", unreal.LinearColor(r=6.0, g=0.15, b=0.2, a=1.0))
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(fres, "", mul, "A")
    mel.connect_material_expressions(c, "", mul, "B")
    add = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -150, 100)
    base = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 300)
    base.set_editor_property("constant", unreal.LinearColor(r=0.6, g=0.02, b=0.03, a=1.0))
    mel.connect_material_expressions(mul, "", add, "A")
    mel.connect_material_expressions(base, "", add, "B")
    mel.connect_material_property(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
print("KG_FX ok")
