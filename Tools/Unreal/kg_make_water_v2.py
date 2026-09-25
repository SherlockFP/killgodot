"""SPRINT-022 water family ("the water looks bad, make it realistic"), new assets under /Game/KillGodot/Materials/S22/.

  M_KG_WaterV2_Sea    the sea + the calm harbour basin (the KG_Sea mesh): the SAME wave WPO as M_KG_Ocean /
                      FKGWaves (HEIGHT_HLSL / CALM_HLSL are read from Tools/Unreal/kg_make_ocean.py, so there is one
                      source for the maths; Source/KillGodot/World/KGWaves.h mirrors it), analytic wave normals plus
                      two-scale ripple normals, depth colour + absorption, shoreline depth fade, fresnel sky
                      reflection, refraction of what is under the surface, soft foam at contact lines (shore, piers,
                      mole), a sharp sun glint.
  M_KG_WaterV2_Still  ponds and brooks (horizontal planes, no WPO): the same shading; ripples pan along the plane's
                      local X at FlowSpeed (brook ribbons are laid along the stream) or just drift (ponds).
  MI_KG_Water_Sea, MI_KG_Water_Koi, MI_KG_Water_Brook, MI_KG_Water_Pond   the family's instances
  M_KG_PondBed        a dark mossy bed card for the raised koi pond
  T_KG_WaterRipple_N  tileable ripple normal map (Tools/Level/make_water_normals.py -> Art/Textures)
  MPC_KG_Weather      scalars for the later WEATHER sprint, all neutral by default (documented in v2_Build_Report §8):
                        WindStrength (0.35)  ripple strength and drift speed (0 = glassy, 1 = gusty)
                        RainRipple   (0)     rain-drop rings on every water surface (0 = none, 1 = downpour)
                        Wetness      (0)     reserved for wet stone / roofs (not read by the water materials)

Headless (new assets only: safe while the editor is open):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_make_water_v2.py [rebuild]" -unattended -nosplash -nopause -nullrhi
The v2 builder assigns them (kg_build_village_v2: sea override, pond / brook planes) when they exist.
"""
import sys

import unreal

DIR = "/Game/KillGodot/Materials/S22"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
TEX_SRC = "D:/Kill Godot/Art/Textures/T_KG_WaterRipple_N.png"

# one source for the wave maths: the string constants at the top of kg_make_ocean.py (not executed past them)
_oc = open("D:/Kill Godot/Tools/Unreal/kg_make_ocean.py", encoding="utf-8").read()
_ns = {}
exec(_oc[_oc.index("WAVES = "):_oc.index("COLOR_HLSL = ")], _ns)
HEIGHT_HLSL, CALM_HLSL, NORMAL_HLSL = _ns["HEIGHT_HLSL"], _ns["CALM_HLSL"], _ns["NORMAL_HLSL"]

RIPPLE_HLSL = r"""
// Two scales of tiled ripple normals, drifting in different directions; wind scales strength and speed.
float wind = saturate(Wind);
float2 flow = Flow.xy * FlowSpeed;
float2 uv1 = P.xy / S.x + float2(0.021, 0.013) * Time * (0.4 + wind) + flow * Time / S.x;
float2 uv2 = P.xy / S.y + float2(-0.011, 0.027) * Time * (0.4 + wind) + flow * Time / S.y;
float3 a = Tex.SampleLevel(TexSampler, uv1, 0).xyz * 2.0 - 1.0;
float3 b = Tex.SampleLevel(TexSampler, uv2, 0).xyz * 2.0 - 1.0;
float dist = distance(P, Cam);
float fade = saturate(1.0 - dist / 5000.0);
float far = saturate(1.0 - dist / 20000.0);                    // the tiling would show far out: fade the ripples away
float2 d = (a.xy * Amp.x * far + b.xy * Amp.y * fade) * (0.35 + 0.9 * wind);
// rain rings (weather sprint): expanding rings in a jittered 60 cm grid, only when RainRipple > 0
if (Rain > 0.001)
{
    float2 c = P.xy / 60.0;
    float2 id = floor(c);
    float2 f = frac(c) - 0.5;
    float h = frac(sin(dot(id, float2(12.9898, 78.233))) * 43758.5453);
    float t = frac(Time * 0.9 + h);
    float r = length(f - (h - 0.5) * 0.4);
    float ring = sin((r - t * 0.45) * 60.0) * saturate(1.0 - abs(r - t * 0.45) * 12.0) * (1.0 - t);
    d += normalize(f + 1e-4) * ring * 0.35 * Rain;
}
return float3(d, 0.0);
"""

SHADE_HLSL = r"""
// Colour of the water body: depth-based scattering colour with Beer-Lambert style absorption per channel.
float th = max(Thick, 0.0);
float3 trans = exp(-th * Absorb.rgb / 100.0);                 // transmittance per channel (Absorb per metre)
float depthT = 1.0 - dot(trans, float3(0.33, 0.34, 0.33));
float3 col = lerp(Shallow.rgb, Deep.rgb, saturate(depthT * 1.15));
col = lerp(col, Tint.rgb, TintAmt);
// soft foam at the contact lines (shore, piers, the mole, the pond rim): a depth band broken up by the ripples
float band = saturate(1.0 - th / FoamDist);
float foam = band * band * saturate(0.55 + 2.2 * Rip.x + 1.4 * Rip.y) * FoamAmt;
return lerp(col, float3(0.92, 0.96, 0.98), saturate(foam));
"""

OPACITY_HLSL = r"""
float th = max(Thick, 0.0);
float body = 1.0 - exp(-th / OpacityDepth);
float fres = pow(1.0 - saturate(dot(N, V)), 5.0);
float band = saturate(1.0 - th / FoamDist);
float edge = saturate(th / EdgeFade);                         // soft shoreline: fades out where the water meets the ground
return saturate((lerp(MinOpacity, 1.0, body) + fres * 0.5 + band * band * 0.4 * FoamAmt) * edge);
"""

EMISSIVE_HLSL = r"""
// Fresnel sky reflection and a sharp sun glint, both following the sky light / sun (dark at night).
float3 n = normalize(N);
float3 v = normalize(V);
float3 r = reflect(-v, n);
float fres = 0.02 + 0.98 * pow(1.0 - saturate(dot(n, v)), 5.0);
float3 sky = Sky.rgb;
float3 refl = sky * fres * ReflAmt;
float g = pow(saturate(dot(r, normalize(SunDir))), GlintPow);
float3 glint = SunCol.rgb * g * GlintAmt;
return (refl + glint) * saturate(Edge);
"""

REFRACT_HLSL = r"""
float th = max(Thick, 0.0);
return 1.0 + Refract * saturate(th / 60.0);
"""


# ============================================================================================ helpers
def custom(m, code, out_type, inputs, x, y, desc):
    e = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", out_type)
    e.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


def scalar(m, name, v, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", v)
    return e


def vec(m, name, rgba, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*rgba))
    return e


def expr(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def link(a, apin, b, bpin):
    mel.connect_material_expressions(a, apin, b, bpin)


def mpc_param(m, mpc, name, x, y, scalar_=False):
    e = expr(m, unreal.MaterialExpressionCollectionParameter, x, y)
    e.set_editor_property("collection", mpc)
    e.set_editor_property("parameter_name", name)
    return e


# ============================================================================================ assets
def weather_mpc():
    path = f"{DIR}/MPC_KG_Weather"
    if eal.does_asset_exist(path):
        mpc = unreal.load_asset(path)
    else:
        mpc = tools.create_asset("MPC_KG_Weather", DIR, unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    sc = list(mpc.get_editor_property("scalar_parameters"))
    have = {str(p.get_editor_property("parameter_name")) for p in sc}
    for name, d in (("WindStrength", 0.35), ("RainRipple", 0.0), ("Wetness", 0.0)):
        if name not in have:
            p = unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", d)
            sc.append(p)
    mpc.set_editor_property("scalar_parameters", sc)
    eal.save_loaded_asset(mpc)
    return mpc


def ripple_texture():
    path = f"{DIR}/T_KG_WaterRipple_N"
    if not eal.does_asset_exist(path):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", TEX_SRC)
        t.set_editor_property("destination_path", DIR)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tools.import_asset_tasks([t])
    tex = unreal.load_asset(path)
    if tex:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex)
    return tex


def water_mpc():
    return unreal.load_asset("/Game/KillGodot/Materials/MPC_KG_Water")


def new_or_wipe(name, rebuild):
    path = f"{DIR}/{name}"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not rebuild:
            return m, False
        mel.delete_all_material_expressions(m)          # headless only (a live editor crashes on this)
        return m, True
    return tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew()), True


def build_water(m, sea, tex, wmpc, cmpc):
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("tangent_space_normal", False)
    try:
        m.set_editor_property("refraction_method", unreal.RefractionMode.RM_PIXEL_NORMAL_OFFSET)
    except Exception:
        pass
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -2600, 0)
    tm = expr(m, unreal.MaterialExpressionTime, -2600, 120)
    cam = expr(m, unreal.MaterialExpressionCameraPositionWS, -2600, 240)
    wind = mpc_param(m, wmpc, "WindStrength", -2600, 360)
    rain = mpc_param(m, wmpc, "RainRipple", -2600, 480)

    # --- surface: sea WPO + analytic normal (same maths as M_KG_Ocean / FKGWaves), or a flat plane
    if sea:
        cz = mpc_param(m, cmpc, "CalmZone", -2600, -400)
        cp = mpc_param(m, cmpc, "CalmParams", -2600, -280)
        calm = custom(m, CALM_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["P", "CZ"], -2300, -360, "calm weight")
        link(wp, "", calm, "P")
        link(cz, "", calm, "CZ")
        height = custom(m, HEIGHT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["P", "Time", "Wc", "CP"], -2000, -300,
                        "wave height (KEEP IN SYNC: FKGWaves)")
        for s_, p_ in ((wp, "P"), (tm, "Time"), (calm, "Wc"), (cp, "CP")):
            link(s_, "", height, p_)
        zero = expr(m, unreal.MaterialExpressionConstant, -1800, -200)
        a1 = expr(m, unreal.MaterialExpressionAppendVector, -1650, -200)
        link(zero, "", a1, "A")
        link(zero, "", a1, "B")
        a2 = expr(m, unreal.MaterialExpressionAppendVector, -1500, -200)
        link(a1, "", a2, "A")
        link(height, "", a2, "B")
        mel.connect_material_property(a2, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        base_n = custom(m, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["P", "Time", "Cam", "Wc", "CP"], -2000, -100,
                        "wave normal")
        for s_, p_ in ((wp, "P"), (tm, "Time"), (cam, "Cam"), (calm, "Wc"), (cp, "CP")):
            link(s_, "", base_n, p_)
        flow = expr(m, unreal.MaterialExpressionConstant3Vector, -2300, 600)
    else:
        base_n = expr(m, unreal.MaterialExpressionVertexNormalWS, -2000, -100)
        one = expr(m, unreal.MaterialExpressionConstant3Vector, -2400, 600)
        one.set_editor_property("constant", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
        flow = expr(m, unreal.MaterialExpressionTransform, -2200, 600)
        flow.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
        flow.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        link(one, "", flow, "")

    # --- ripples
    tob = expr(m, unreal.MaterialExpressionTextureObject, -2400, 760)
    tob.set_editor_property("texture", tex)
    try:
        tob.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    except Exception:
        pass
    scales = vec(m, "RippleScale", (900.0, 260.0, 0.0, 0.0), -2400, 880)
    amps = vec(m, "RippleAmp", (0.55, 0.35, 0.0, 0.0), -2400, 1000)
    fspd = scalar(m, "FlowSpeed", 0.0, -2400, 1120)
    rip = custom(m, RIPPLE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                 ["P", "Time", "Cam", "Tex", "S", "Amp", "Wind", "Rain", "Flow", "FlowSpeed"], -1900, 700, "ripples")
    for s_, p_ in ((wp, "P"), (tm, "Time"), (cam, "Cam"), (tob, "Tex"), (scales, "S"), (amps, "Amp"), (wind, "Wind"),
                   (rain, "Rain"), (flow, "Flow"), (fspd, "FlowSpeed")):
        link(s_, "", rip, p_)
    nrm = custom(m, "return normalize(B + R);", unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["B", "R"], -1600, 300, "normal")
    link(base_n, "", nrm, "B")
    link(rip, "", nrm, "R")
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)

    # --- depth
    sd = expr(m, unreal.MaterialExpressionSceneDepth, -2600, 1300)
    pd = expr(m, unreal.MaterialExpressionPixelDepth, -2600, 1420)
    thick = expr(m, unreal.MaterialExpressionSubtract, -2400, 1360)
    link(sd, "", thick, "A")
    link(pd, "", thick, "B")

    shallow = vec(m, "Shallow", (0.06, 0.42, 0.40, 1.0) if sea else (0.10, 0.36, 0.30, 1.0), -2000, 1500)
    deep = vec(m, "Deep", (0.004, 0.05, 0.10, 1.0) if sea else (0.01, 0.07, 0.06, 1.0), -2000, 1620)
    absorb = vec(m, "Absorb", (0.45, 0.09, 0.06, 0.0) if sea else (0.9, 0.35, 0.45, 0.0), -2000, 1740)
    foam_d = scalar(m, "FoamDist", 38.0 if sea else 14.0, -2000, 1860)
    foam_a = scalar(m, "FoamAmt", 0.85 if sea else 0.35, -2000, 1980)
    if sea:
        ctint = mpc_param(m, cmpc, "CalmTint", -2000, 2100)
        tint_amt = custom(m, "return Wc * CP.z;", unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Wc", "CP"], -1800, 2200, "calm tint")
        link(calm, "", tint_amt, "Wc")
        link(cp, "", tint_amt, "CP")
    else:
        ctint = vec(m, "Tint", (0.1, 0.3, 0.25, 1.0), -2000, 2100)
        tint_amt = scalar(m, "TintAmount", 0.0, -1800, 2200)
    col = custom(m, SHADE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                 ["Thick", "Shallow", "Deep", "Absorb", "Tint", "TintAmt", "FoamDist", "FoamAmt", "Rip"], -1400, 1500, "water colour")
    for s_, p_ in ((thick, "Thick"), (shallow, "Shallow"), (deep, "Deep"), (absorb, "Absorb"), (ctint, "Tint"),
                   (tint_amt, "TintAmt"), (foam_d, "FoamDist"), (foam_a, "FoamAmt"), (rip, "Rip")):
        link(s_, "", col, p_)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    camv = expr(m, unreal.MaterialExpressionCameraVectorWS, -2000, 2400)
    op = custom(m, OPACITY_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                ["Thick", "N", "V", "OpacityDepth", "MinOpacity", "FoamDist", "FoamAmt", "EdgeFade"], -1400, 2300, "opacity")
    for s_, p_ in ((thick, "Thick"), (nrm, "N"), (camv, "V"), (scalar(m, "OpacityDepth", 260.0 if sea else 70.0, -2000, 2520), "OpacityDepth"),
                   (scalar(m, "MinOpacity", 0.18 if sea else 0.12, -2000, 2640), "MinOpacity"), (foam_d, "FoamDist"),
                   (foam_a, "FoamAmt"), (scalar(m, "EdgeFade", 6.0, -2000, 2760), "EdgeFade")):
        link(s_, "", op, p_)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    # --- sky reflection + sun glint (emissive, follows the sky light and the sun, so it dims at night)
    sky = None
    try:
        sky = expr(m, unreal.MaterialExpressionSkyLightEnvMapSample, -1800, 2900)
        rv = expr(m, unreal.MaterialExpressionReflectionVectorWS, -2000, 2900)
        link(rv, "", sky, "Direction")
        link(scalar(m, "SkyRoughness", 0.08, -2000, 3000), "", sky, "Roughness")
    except Exception:
        sky = vec(m, "SkyColor", (0.35, 0.45, 0.55, 1.0), -1800, 2900)
    try:
        sun_dir = expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1800, 3100)
        sun_col = expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1800, 3220)
        link(wp, "", sun_col, "WorldPosition")
    except Exception:
        sun_dir = vec(m, "SunDir", (0.3, 0.3, 0.9, 0.0), -1800, 3100)
        sun_col = vec(m, "SunColor", (3.0, 2.8, 2.4, 0.0), -1800, 3220)
    edge = custom(m, "return saturate(max(Thick, 0.0) / 6.0);", unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Thick"], -1800, 3340, "edge")
    link(thick, "", edge, "Thick")
    em = custom(m, EMISSIVE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                ["N", "V", "Sky", "SunDir", "SunCol", "ReflAmt", "GlintPow", "GlintAmt", "Edge"], -1400, 3000, "reflection + glint")
    for s_, p_ in ((nrm, "N"), (camv, "V"), (sky, "Sky"), (sun_dir, "SunDir"), (sun_col, "SunCol"),
                   (scalar(m, "ReflAmt", 0.55, -1800, 3460), "ReflAmt"), (scalar(m, "GlintPow", 700.0, -1800, 3580), "GlintPow"),
                   (scalar(m, "GlintAmt", 0.02, -1800, 3700), "GlintAmt"), (edge, "Edge")):
        link(s_, "", em, p_)
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rf = custom(m, REFRACT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Thick", "Refract"], -1400, 3900, "refraction")
    link(thick, "", rf, "Thick")
    link(scalar(m, "Refract", 0.22 if sea else 0.12, -1800, 3900), "", rf, "Refract")
    mel.connect_material_property(rf, "", unreal.MaterialProperty.MP_REFRACTION)
    mel.connect_material_property(scalar(m, "Roughness", 0.035, -1400, 4100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 0.9, -1400, 4200), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def pond_bed(rebuild):
    m, fresh = new_or_wipe("M_KG_PondBed", rebuild)
    if fresh:
        c = vec(m, "Color", (0.035, 0.06, 0.035, 1.0), -600, 0)
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
        r = scalar(m, "Roughness", 0.9, -600, 150)
        mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    return m


INSTANCES = {
    "MI_KG_Water_Sea": ("M_KG_WaterV2_Sea", {"RippleScale": (1600.0, 450.0, 0, 0), "RippleAmp": (0.22, 0.14, 0, 0),
                                             "ReflAmt": 0.4}),
    "MI_KG_Water_Koi": ("M_KG_WaterV2_Still", {"Shallow": (0.16, 0.40, 0.26, 1), "Deep": (0.02, 0.10, 0.06, 1),
                                               "Absorb": (1.4, 0.55, 0.8, 0), "OpacityDepth": 45.0, "MinOpacity": 0.1,
                                               "RippleScale": (420.0, 140.0, 0, 0), "RippleAmp": (0.25, 0.18, 0, 0),
                                               "FoamAmt": 0.25, "Refract": 0.1}),
    "MI_KG_Water_Brook": ("M_KG_WaterV2_Still", {"Shallow": (0.13, 0.35, 0.31, 1), "Deep": (0.02, 0.08, 0.09, 1),
                                                 "Absorb": (1.1, 0.42, 0.36, 0),
                                                 "FlowSpeed": 60.0, "RippleScale": (300.0, 110.0, 0, 0),
                                                 "RippleAmp": (0.7, 0.45, 0, 0), "FoamAmt": 0.3, "FoamDist": 5.0,
                                                 "OpacityDepth": 55.0, "ReflAmt": 0.25}),
    "MI_KG_Water_Pond": ("M_KG_WaterV2_Still", {"Shallow": (0.12, 0.33, 0.30, 1), "Deep": (0.02, 0.07, 0.08, 1),
                                                "RippleAmp": (0.35, 0.25, 0, 0)}),
}


def instances():
    out = {}
    for name, (parent, params) in INSTANCES.items():
        path = f"{DIR}/{name}"
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
        if not mi:
            mi = tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset(f"{DIR}/{parent}"))
        for k, v in params.items():
            if isinstance(v, tuple):
                mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
            else:
                mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[name] = path
    return out


def ensure(rebuild=False):
    wmpc = weather_mpc()
    cmpc = water_mpc()
    tex = ripple_texture()
    for name, sea in (("M_KG_WaterV2_Sea", True), ("M_KG_WaterV2_Still", False)):
        m, fresh = new_or_wipe(name, rebuild)
        if fresh:
            build_water(m, sea, tex, wmpc, cmpc)
    pond_bed(rebuild)
    return instances()


if __name__ != "kg_make_water_v2" or "--run" in sys.argv:
    made = ensure(rebuild=any(a.lower() == "rebuild" for a in sys.argv[1:]))
    print(f"KG_WATER_V2 ok: {sorted(made)}")
