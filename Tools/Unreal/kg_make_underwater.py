"""M_KG_Underwater post-process (live-safe: creates if missing): murky blue-green fog that thickens with distance,
plus gentle caustic-like brightness wobble. Used by AKGCharacter's UnderwaterPP (enabled only below the waves).
Also tunes M_KG_Ocean for close-up swimming: two-sided (visible from below), softer sky reflection."""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/KillGodot/Materials/M_KG_Underwater"

UW_HLSL = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 wob = float2(sin(uv.y * 40.0 + Time * 1.7), cos(uv.x * 34.0 + Time * 1.3)) * 0.0025;
float3 scene = SceneTextureLookup(uv + wob, 14, false).rgb;
float d = SceneTextureLookup(uv, 1, false).r;
float fog = 1.0 - exp(-d / 1400.0);
float3 water = float3(0.03, 0.26, 0.30);
float caustic = 0.9 + 0.1 * sin(uv.x * 60.0 + Time * 2.0) * sin(uv.y * 55.0 - Time * 1.6);
return lerp(scene * float3(0.55, 0.85, 0.9) * caustic, water, saturate(fog));
"""

if not eal.does_asset_exist(PATH):
    m = tools.create_asset("M_KG_Underwater", "/Game/KillGodot/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    st = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 0)
    st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    sd = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 200)
    sd.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 400)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 100)
    c.set_editor_property("code", UW_HLSL)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("Time", "Unused0", "Unused1"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    mel.connect_material_expressions(tm, "", c, "Time")
    mel.connect_material_expressions(st, "Color", c, "Unused0")
    mel.connect_material_expressions(sd, "Color", c, "Unused1")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

ocean = unreal.load_asset("/Game/KillGodot/Materials/M_KG_Ocean")
if ocean and not ocean.get_editor_property("two_sided"):
    ocean.set_editor_property("two_sided", True)
    spec = mel.get_material_property_input_node(ocean, unreal.MaterialProperty.MP_SPECULAR)
    if spec:
        spec.set_editor_property("default_value", 0.5)
    mel.recompile_material(ocean)
    eal.save_loaded_asset(ocean)
print("KG_UNDERWATER ok")
