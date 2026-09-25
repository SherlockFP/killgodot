"""Import the procedural grass clumps and build M_KG_Grass (two-sided foliage, root->tip gradient, wind WPO).
Safe in the live editor on first run (creates new assets only). Re-running rebuilds nothing that exists.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_grass.py --timeout 300
"""
import unreal

DEST = "/Game/KillGodot/Env/Grass"
MAT = "/Game/KillGodot/Materials/M_KG_Grass"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

WIND_HLSL = r"""
// Height: 0 root .. 1 tip (vertex colour R). Big slow gusts roll across the field + small flutter per blade.
float h = Height * Height;
float2 wp = WorldPos.xy;
float gust = sin(dot(wp, float2(0.0021, 0.0013)) - Time * 1.25) * 0.5 + 0.5;
gust = gust * gust;
float flutter = sin(dot(wp, float2(0.043, 0.031)) + Time * 3.7 + Rand * 6.2831);
float bend = (0.25 + 0.95 * gust + 0.18 * flutter) * h * Amp;
float2 dir = normalize(WindDir);
return float3(dir * bend, -0.35 * abs(bend) * Height);
"""


def scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def color(m, name, rgb, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(r=rgb[0], g=rgb[1], b=rgb[2], a=1.0))
    return e


def build_material():
    if eal.does_asset_exist(MAT):
        return unreal.load_asset(MAT)
    m = tools.create_asset("M_KG_Grass", "/Game/KillGodot/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -1400, 0)
    root = color(m, "RootColor", (0.035, 0.09, 0.02), -1400, 200)
    tip = color(m, "TipColor", (0.34, 0.55, 0.12), -1400, 350)
    petal = color(m, "PetalColor", (0.95, 0.85, 0.35), -1400, 500)
    grad = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -1000, 200)
    mel.connect_material_expressions(root, "", grad, "A")
    mel.connect_material_expressions(tip, "", grad, "B")
    mel.connect_material_expressions(vc, "R", grad, "Alpha")
    # Per-blade variation: 0.8..1.15 brightness from vertex colour G.
    var_mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 250)
    var_add = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -1000, 450)
    mel.connect_material_expressions(vc, "G", var_add, "A")
    var_add.set_editor_property("const_b", 0.0)
    var_scale = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -900, 450)
    mel.connect_material_expressions(vc, "G", var_scale, "A")
    var_scale.set_editor_property("const_b", 0.35)
    var_bias = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -800, 450)
    mel.connect_material_expressions(var_scale, "", var_bias, "A")
    var_bias.set_editor_property("const_b", 0.8)
    mel.connect_material_expressions(grad, "", var_mul, "A")
    mel.connect_material_expressions(var_bias, "", var_mul, "B")
    flower = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -600, 300)
    mel.connect_material_expressions(var_mul, "", flower, "A")
    mel.connect_material_expressions(petal, "", flower, "B")
    mel.connect_material_expressions(vc, "B", flower, "Alpha")
    mel.connect_material_property(flower, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Light through the blades (warm-green translucency, the backlit Tsushima look).
    sss = color(m, "Subsurface", (0.25, 0.4, 0.05), -600, 550)
    mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.75, -600, 650), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 0.25, -600, 720), "", unreal.MaterialProperty.MP_SPECULAR)

    wind = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -700, 900)
    wind.set_editor_property("code", WIND_HLSL)
    wind.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    wind.set_editor_property("description", "KG grass wind")
    ins = []
    for n in ["Height", "WorldPos", "Time", "Rand", "Amp", "WindDir"]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    wind.set_editor_property("inputs", ins)
    mel.connect_material_expressions(vc, "R", wind, "Height")
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1100, 950)
    mel.connect_material_expressions(wp, "", wind, "WorldPos")
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1100, 1050)
    mel.connect_material_expressions(tm, "", wind, "Time")
    mel.connect_material_expressions(vc, "G", wind, "Rand")
    mel.connect_material_expressions(scalar(m, "WindStrength", 22.0, -1100, 1150), "", wind, "Amp")
    wd = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -1100, 1250)
    wd.set_editor_property("r", 0.8)
    wd.set_editor_property("g", 0.45)
    mel.connect_material_expressions(wd, "", wind, "WindDir")
    mel.connect_material_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


mat = build_material()
if not eal.does_asset_exist(f"{DEST}/KG_Grass/StaticMeshes/SM_KG_GrassMid"):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_Grass.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
for name in ("SM_KG_GrassShort", "SM_KG_GrassMid", "SM_KG_GrassTall", "SM_KG_GrassFlowers"):
    m = unreal.load_asset(f"{DEST}/KG_Grass/StaticMeshes/{name}")
    if not m:
        print("KG_GRASS missing", name)
        continue
    m.set_material(0, mat)
    unreal.EditorStaticMeshLibrary.remove_collisions(m)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    m.set_editor_property("nanite_settings", ns)
    eal.save_loaded_asset(m)
print("KG_GRASS ok")
