"""Sea-of-Thieves-style ocean + fish materials, and the sea mesh import. Live-safe: creates missing assets only.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_make_ocean.py --timeout 600
Headless REBUILD of M_KG_Ocean (the graph is wiped and rebuilt; never do this in a live editor, it crashes there):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_make_ocean.py rebuild" -unattended -nosplash -nopause -nullrhi

M_KG_Ocean: translucent, forward-shaded; waves in WPO (KEEP IN SYNC with Source/KillGodot/World/KGWaves.h),
analytic world-space normals, colour by water thickness (turquoise shallows -> deep blue), glowing crests
(fake subsurface), shore + crest foam.
Calm harbour basin: MPC_KG_Water holds CalmZone = (centre x, centre y, radius, fade) cm,
CalmParams = (swell scale, ripple amplitude cm, tint amount, 0) and CalmTint. Radius 0 (the default) = open sea
everywhere, so the v1 map is unchanged. AKGMapInfo (bCalmWater, CalmCentre, ...) pushes the level's values into both
this collection and FKGWaves at load, so the rendered surface and swimming / buoyancy agree.
M_KG_Fish: vertex colour + tail wiggle driven by vertex alpha (0 head .. 1 tail).
"""
import sys

import unreal

MAT_DIR = "/Game/KillGodot/Materials"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

WAVES = "float4 W[3] = { float4(0.78, 0.62, 26.0, 2600.0), float4(-0.35, 0.94, 13.0, 1300.0), float4(0.95, -0.31, 6.0, 650.0) };\n"

# Calm weight: 0 open sea .. 1 sheltered basin (smoothstep over Fade inside Radius). KEEP IN SYNC with FKGWaves::CalmWeight.
CALM_HLSL = r"""
if (CZ.z <= 0.0) return 0.0;
float f = max(CZ.w, 1.0);
float t = saturate((distance(P.xy, CZ.xy) - (CZ.z - f)) / f);
return 1.0 - t * t * (3.0 - 2.0 * t);
"""

HEIGHT_HLSL = WAVES + r"""
float h = 0;
for (int i = 0; i < 3; i++)
{
    float k = 6.2831853 / W[i].w;
    float c = sqrt(980.0 / k);
    h += W[i].z * sin(k * (W[i].x * P.x + W[i].y * P.y) - k * c * Time);
}
// Sheltered basin: the swell shrinks to CP.x and small wind ripples (CP.y cm) take over. Same maths as FKGWaves::HeightAt.
float rip = 0.6 * sin(0.021 * (0.6 * P.x + 0.8 * P.y) - 1.9 * Time) + 0.4 * sin(0.029 * (-0.7 * P.x + 0.71 * P.y) - 2.3 * Time);
return h * lerp(1.0, CP.x, Wc) + Wc * CP.y * rip;
"""

NORMAL_HLSL = WAVES + r"""
float dx = 0, dy = 0;
for (int i = 0; i < 3; i++)
{
    float k = 6.2831853 / W[i].w;
    float c = sqrt(980.0 / k);
    float d = W[i].z * k * cos(k * (W[i].x * P.x + W[i].y * P.y) - k * c * Time);
    dx += d * W[i].x;
    dy += d * W[i].y;
}
float s = lerp(1.0, CP.x, Wc);
dx *= s;
dy *= s;
// Basin ripples (x1.6 in the normal so sheltered water still sparkles) plus irregular cat's-paws: the two ripple
// trains are broken up by a slow low-frequency gust mask so the basin never reads as a regular grid.
float a1 = 0.021 * (0.6 * P.x + 0.8 * P.y) - 1.9 * Time;
float a2 = 0.029 * (-0.7 * P.x + 0.71 * P.y) - 2.3 * Time;
float gust = 0.55 + 0.45 * sin(P.x * 0.0061 + P.y * 0.0047 + Time * 0.35) * sin(P.y * 0.0083 - P.x * 0.0029 - Time * 0.27);
float ra = Wc * CP.y * 1.6 * gust;
dx += ra * (0.6 * 0.021 * 0.6 * cos(a1) - 0.4 * 0.029 * 0.7 * cos(a2));
dy += ra * (0.6 * 0.021 * 0.8 * cos(a1) + 0.4 * 0.029 * 0.71 * cos(a2));
dx += Wc * 0.035 * (1.2 - gust) * sin(P.x * 0.09 + P.y * 0.05 - Time * 2.6 + 2.0 * sin(P.y * 0.013));
dy += Wc * 0.035 * (1.2 - gust) * cos(P.y * 0.08 - P.x * 0.04 - Time * 2.2 + 2.0 * sin(P.x * 0.011));
// Small capillary ripples for sun glints, faded out with distance (they alias far away).
float fade = saturate(1.0 - distance(P, Cam) / 4500.0);
dx += fade * 0.035 * sin(P.x * 0.045 + P.y * 0.021 + Time * 2.3) + fade * 0.02 * sin(P.x * 0.11 - Time * 3.1);
dy += fade * 0.035 * cos(P.y * 0.051 - P.x * 0.017 + Time * 1.9) + fade * 0.02 * cos(P.y * 0.13 + Time * 2.7);
return normalize(float3(-dx, -dy, 1.0));
"""

COLOR_HLSL = r"""
float depthT = saturate(Thick / 450.0);
float3 c = lerp(Shallow, Deep, depthT * depthT * (3 - 2 * depthT));
c = lerp(c, Tint, Wc * CP.z);   // sheltered water: a calmer green-teal
float crest = saturate(H / 32.0 + 0.25);
c = lerp(c, Crest, crest * crest * 0.55 * (1.0 - 0.7 * Wc));
float shoreFoam = saturate(1.0 - Thick / 40.0) * (0.55 + 0.45 * sin(P.x * 0.03 + P.y * 0.02 + Time * 1.4)) * (1.0 - 0.6 * Wc);
float crestFoam = saturate((H - 26.0) / 10.0) * (0.6 + 0.4 * sin(P.x * 0.09 + Time * 3.0));
return lerp(c, float3(0.95, 0.98, 1.0), saturate(shoreFoam + crestFoam));
"""

EMISSIVE_HLSL = r"""
// Fake subsurface: light shining through the wave crests (the Sea of Thieves glow).
float crest = saturate(H / 30.0 + 0.1);
return Crest * crest * crest * Glow;
"""

OPACITY_HLSL = r"""
float shoreFoam = saturate(1.0 - Thick / 40.0);
return saturate(lerp(0.35, 0.94, saturate(Thick / 260.0)) + shoreFoam * 0.5);
"""

MPC_PATH = f"{MAT_DIR}/MPC_KG_Water"
MPC_VECTORS = {"CalmZone": (0.0, 0.0, 0.0, 1000.0), "CalmParams": (0.25, 2.5, 0.0, 0.0), "CalmTint": (0.02, 0.30, 0.27, 1.0)}


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


def vec(m, name, rgb, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(r=rgb[0], g=rgb[1], b=rgb[2], a=1.0))
    return e


def scalar(m, name, v, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", v)
    return e


def water_mpc():
    """MPC_KG_Water with the calm-basin vectors (defaults = off). Created once; missing vectors are appended."""
    if eal.does_asset_exist(MPC_PATH):
        mpc = unreal.load_asset(MPC_PATH)
    else:
        mpc = tools.create_asset("MPC_KG_Water", MAT_DIR, unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    vecs = list(mpc.get_editor_property("vector_parameters"))
    have = {str(v.get_editor_property("parameter_name")) for v in vecs}
    for name, d in MPC_VECTORS.items():
        if name in have:
            continue
        v = unreal.CollectionVectorParameter()
        v.set_editor_property("parameter_name", name)
        v.set_editor_property("default_value", unreal.LinearColor(r=d[0], g=d[1], b=d[2], a=d[3]))
        vecs.append(v)
    mpc.set_editor_property("vector_parameters", vecs)
    eal.save_loaded_asset(mpc)
    return mpc


def mpc_param(m, mpc, name, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, x, y)
    e.set_editor_property("collection", mpc)
    e.set_editor_property("parameter_name", name)
    return e


def new_material(name):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        return None
    return tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def ocean(rebuild=False):
    m = new_material("M_KG_Ocean")
    if not m:
        m = unreal.load_asset(f"{MAT_DIR}/M_KG_Ocean")
        if not rebuild:
            return m
        mel.delete_all_material_expressions(m)   # headless only (see the docstring)
    build_ocean(m)
    return m


def build_ocean(m):
    mpc = water_mpc()
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    m.set_editor_property("tangent_space_normal", False)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1800, 0)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1800, 150)
    cz = mpc_param(m, mpc, "CalmZone", -2100, -250)
    cp = mpc_param(m, mpc, "CalmParams", -2100, -120)
    ctint = mpc_param(m, mpc, "CalmTint", -1500, 1330)
    calm = custom(m, CALM_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["P", "CZ"], -1650, -200, "calm weight")
    mel.connect_material_expressions(wp, "", calm, "P")
    mel.connect_material_expressions(cz, "", calm, "CZ")
    height = custom(m, HEIGHT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["P", "Time", "Wc", "CP"], -1400, 0,
                    "wave height")
    mel.connect_material_expressions(wp, "", height, "P")
    mel.connect_material_expressions(tm, "", height, "Time")
    mel.connect_material_expressions(calm, "", height, "Wc")
    mel.connect_material_expressions(cp, "", height, "CP")
    # WPO = (0, 0, h)
    zero = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -1200, 250)
    app1 = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -1000, 250)
    mel.connect_material_expressions(zero, "", app1, "A")
    mel.connect_material_expressions(zero, "", app1, "B")
    app2 = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -850, 250)
    mel.connect_material_expressions(app1, "", app2, "A")
    mel.connect_material_expressions(height, "", app2, "B")
    mel.connect_material_property(app2, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    normal = custom(m, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["P", "Time", "Cam", "Wc", "CP"],
                    -1400, 400, "wave normal")
    mel.connect_material_expressions(wp, "", normal, "P")
    mel.connect_material_expressions(tm, "", normal, "Time")
    mel.connect_material_expressions(calm, "", normal, "Wc")
    mel.connect_material_expressions(cp, "", normal, "CP")
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, -1800, 500)
    mel.connect_material_expressions(cam, "", normal, "Cam")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    sd = mel.create_material_expression(m, unreal.MaterialExpressionSceneDepth, -1800, 700)
    pd = mel.create_material_expression(m, unreal.MaterialExpressionPixelDepth, -1800, 820)
    thick = mel.create_material_expression(m, unreal.MaterialExpressionSubtract, -1500, 750)
    mel.connect_material_expressions(sd, "", thick, "A")
    mel.connect_material_expressions(pd, "", thick, "B")

    shallow = vec(m, "Shallow", (0.05, 0.62, 0.58), -1500, 950)
    deep = vec(m, "Deep", (0.006, 0.09, 0.2), -1500, 1080)
    crest = vec(m, "Crest", (0.12, 0.85, 0.62), -1500, 1210)
    color = custom(m, COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["Thick", "H", "Shallow", "Deep", "Crest", "P", "Time", "Wc", "CP", "Tint"], -1000, 800, "ocean colour")
    for src, pin in ((thick, "Thick"), (height, "H"), (shallow, "Shallow"), (deep, "Deep"), (crest, "Crest"),
                     (wp, "P"), (tm, "Time"), (calm, "Wc"), (cp, "CP"), (ctint, "Tint")):
        mel.connect_material_expressions(src, "", color, pin)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    emis = custom(m, EMISSIVE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["H", "Crest", "Glow"], -1000, 1150,
                  "crest glow")
    mel.connect_material_expressions(height, "", emis, "H")
    mel.connect_material_expressions(crest, "", emis, "Crest")
    mel.connect_material_expressions(scalar(m, "CrestGlow", 0.35, -1300, 1350), "", emis, "Glow")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opac = custom(m, OPACITY_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Thick"], -1000, 1400, "ocean opacity")
    mel.connect_material_expressions(thick, "", opac, "Thick")
    mel.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(scalar(m, "Roughness", 0.04, -700, 1500), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 1.0, -700, 1600), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


FISH_HLSL = r"""
// Tail wiggle in local Y, stronger towards the tail (vertex alpha 0 head .. 1 tail).
float a = Along;
float w = sin(Time * 9.0 - a * 5.5 + Phase) * pow(a, 1.6) * Amp;
return float3(0, w, 0);
"""


def fish():
    m = new_material("M_KG_Fish")
    if not m:
        return unreal.load_asset(f"{MAT_DIR}/M_KG_Fish")
    m.set_editor_property("used_with_instanced_static_meshes", True)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.35, -600, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 0.7, -600, 280), "", unreal.MaterialProperty.MP_SPECULAR)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1100, 400)
    ppos = mel.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, -1100, 500)
    phase = mel.create_material_expression(m, unreal.MaterialExpressionDotProduct, -900, 500)
    mel.connect_material_expressions(ppos, "", phase, "A")
    ph_const = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -1100, 600)
    ph_const.set_editor_property("constant", unreal.LinearColor(r=0.013, g=0.017, b=0.0, a=0.0))
    mel.connect_material_expressions(ph_const, "", phase, "B")
    wig = custom(m, FISH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["Along", "Time", "Phase", "Amp"], -700, 400,
                 "tail wiggle")
    mel.connect_material_expressions(vc, "A", wig, "Along")
    mel.connect_material_expressions(tm, "", wig, "Time")
    mel.connect_material_expressions(phase, "", wig, "Phase")
    mel.connect_material_expressions(scalar(m, "WiggleCm", 4.0, -900, 700), "", wig, "Amp")
    tv = mel.create_material_expression(m, unreal.MaterialExpressionTransform, -450, 400)
    tv.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    tv.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    mel.connect_material_expressions(wig, "", tv, "")
    mel.connect_material_property(tv, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def import_glb(path, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


REBUILD = any(a.lower() == "rebuild" for a in sys.argv[1:])
om = ocean(rebuild=REBUILD)
fm = fish()
sea_path = "/Game/KillGodot/Env/Sea/KG_Sea/StaticMeshes/KG_Sea"   # single-mesh glb: named after the file
if not eal.does_asset_exist(sea_path):
    import_glb("D:/Kill Godot/Art/Packed/KG_Sea.glb", "/Game/KillGodot/Env/Sea")
sea = unreal.load_asset(sea_path)
if sea:
    sea.set_material(0, om)
    unreal.EditorStaticMeshLibrary.remove_collisions(sea)
    ns = sea.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    sea.set_editor_property("nanite_settings", ns)
    eal.save_loaded_asset(sea)
print("KG_OCEAN ok", bool(om), bool(fm), bool(sea), "rebuilt" if REBUILD else "")
