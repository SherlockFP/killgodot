"""Wind for tree leaves and flowers: M_KG_Foliage (masked, two-sided foliage, same "BaseColorTexture" parameter as
the Interchange glTF instances) + re-parent the nature kit's leaf/flower instances onto it. Live-safe.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_make_foliage_wind.py --timeout 600
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/KillGodot/Materials/M_KG_Foliage"

SWAY = r"""
// Height above the tree's pivot drives the sway (trunk base still, crown moving). Big slow sway per tree +
// fast leaf flutter per position. Same wind direction as the meadow (M_KG_Grass).
float h = saturate((P.z - O.z) / 700.0);
h *= h;
float2 dir = normalize(float2(0.8, 0.45));
float big = sin(Time * 1.1 + dot(O.xy, float2(0.0013, 0.0009))) * 0.6 + sin(Time * 0.47 + O.x * 0.0007) * 0.4;
float flutter = sin(Time * 5.3 + dot(P.xyz, float3(0.031, 0.027, 0.043))) * 0.35;
float s = (big * 10.0 + flutter * 4.0 + 5.0) * h * Amp;
return float3(dir * s, flutter * 2.0 * h);
"""

if not eal.does_asset_exist(PATH):
    m = tools.create_asset("M_KG_Foliage", "/Game/KillGodot/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
    tex.set_editor_property("parameter_name", "BaseColorTexture")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse"))
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    sss = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 150)
    mel.connect_material_expressions(tex, "RGB", sss, "A")
    sss.set_editor_property("const_b", 0.6)
    mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 300)
    rough.set_editor_property("r", 0.8)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, 500)
    c.set_editor_property("code", SWAY)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("P", "O", "Time", "Amp"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, 450), "", c, "P")
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, -900, 550), "", c, "O")
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 650), "", c, "Time")
    amp = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 750)
    amp.set_editor_property("parameter_name", "WindStrength")
    amp.set_editor_property("default_value", 1.0)
    mel.connect_material_expressions(amp, "", c, "Amp")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

master = unreal.load_asset(PATH)
done = []
for name in ("Leaves", "Leaves_NormalTree", "Leaves_Pine", "Leaves_TwistedTree", "Flowers"):
    mi = unreal.load_asset(f"/Game/KillGodot/Env/KG_Nature/Materials/{name}")
    if isinstance(mi, unreal.MaterialInstanceConstant) and mi.get_editor_property("parent") != master:
        mel.set_material_instance_parent(mi, master)
        if name == "Leaves_Pine":
            mel.set_material_instance_scalar_parameter_value(mi, "WindStrength", 0.5)   # stiffer needles
        eal.save_loaded_asset(mi)
        done.append(name)
print("KG_FOLIAGE", done)
