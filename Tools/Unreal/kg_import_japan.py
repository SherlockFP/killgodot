"""Import Art/Packed/KG_JapanProps_NoMat.glb (torii, pagoda, lanterns, sakura/maple/pine/bamboo, koi pond, bridges, koi,
yatai) into /Game/KillGodot/Env/Japan with our materials, chosen by the glTF material name the Blender script set:
  *Glow*    -> M_KG_JapanGlow (vertex colour, alpha = emissive mask: lanterns/lamps glow warm)
  *Foliage* -> M_KG_JapanFoliage (vertex colour, alpha = wind mask; same sway as M_KG_Foliage)
  *Koi*     -> M_KG_Fish (tail wiggle from alpha)
  else      -> M_KG_PropVC
Vertex RGB here is LINEAR (unlike the sRGB water props), so the Japan materials do not square it.
Live-safe: new assets only.

Source glb is sanitised first: Tools/Blender/kg_sanitize_glb.py (triangulate/merge; avoids "Bad MeshDescription").
  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_japan.py --timeout 900
"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/KillGodot/Env/JapanC3"
MESHES = f"{DEST}/KG_JapanProps_Clean2/StaticMeshes"
MAT = "/Game/KillGodot/Materials"
NAMES = ["Torii", "ToroLantern", "GardenLamp", "Sakura_A", "Sakura_B", "Maple", "BonsaiPine", "Bamboo", "KoiPondRim",
         "WoodWalk", "ArchBridge", "SteppingStone", "GardenRock", "Koi", "Pagoda", "Noren_Stall"]

SWAY = r"""
float h = saturate((P.z - O.z) / 600.0) * Mask;
float2 dir = normalize(float2(0.8, 0.45));
float big = sin(Time * 1.1 + dot(O.xy, float2(0.0013, 0.0009))) * 0.6 + sin(Time * 0.47 + O.x * 0.0007) * 0.4;
float flutter = sin(Time * 5.3 + dot(P.xyz, float3(0.031, 0.027, 0.043))) * 0.35;
float s = (big * 9.0 + flutter * 4.0 + 4.0) * h;
return float3(dir * s, flutter * 2.0 * h);
"""


def new_mat(name):
    path = f"{MAT}/{name}"
    if eal.does_asset_exist(path):
        return None, unreal.load_asset(path)
    return tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew()), None


def glow_material():
    m, existing = new_mat("M_KG_JapanGlow")
    if existing:
        return existing
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -800, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -800, 250)
    glow.set_editor_property("constant", unreal.LinearColor(r=6.0, g=3.2, b=1.1, a=1.0))
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 200)
    mel.connect_material_expressions(vc, "A", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def foliage_material():
    m, existing = new_mat("M_KG_JapanFoliage")
    if existing:
        return existing
    m.set_editor_property("two_sided", True)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sss = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 150)
    mel.connect_material_expressions(vc, "", sss, "A")
    sss.set_editor_property("const_b", 0.5)
    mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, 400)
    c.set_editor_property("code", SWAY)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("P", "O", "Time", "Mask"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, 350), "", c, "P")
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, -900, 450), "", c, "O")
    mel.connect_material_expressions(mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 550), "", c, "Time")
    mel.connect_material_expressions(vc, "A", c, "Mask")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def plain_vc_linear():
    m, existing = new_mat("M_KG_PropVCLinear")
    if existing:
        return existing
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -600, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200)
    r.set_editor_property("r", 0.7)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


if not eal.does_asset_exist(f"{MESHES}/SM_KG_Torii"):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_JapanProps_Clean2.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])

mats = {"Glow": glow_material(), "Foliage": foliage_material(), "Koi": unreal.load_asset(f"{MAT}/M_KG_Fish"),
        "VC": plain_vc_linear()}
kind_of = {"ToroLantern": "Glow", "GardenLamp": "Glow", "Noren_Stall": "Glow", "Sakura_A": "Foliage", "Sakura_B": "Foliage",
           "Maple": "Foliage", "BonsaiPine": "Foliage", "Bamboo": "Foliage", "Koi": "Koi"}
out = []
for n in NAMES:
    m = unreal.load_asset(f"{MESHES}/SM_KG_{n}")
    if not m:
        out.append(f"{n} MISSING")
        continue
    mat = mats[kind_of.get(n, "VC")]
    for i in range(max(1, len(m.get_editor_property("static_materials")))):
        m.set_material(i, mat)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    m.set_editor_property("nanite_settings", ns)
    if n in ("Koi",):
        unreal.EditorStaticMeshLibrary.remove_collisions(m)
    elif n in ("WoodWalk", "ArchBridge", "KoiPondRim", "Pagoda", "Torii", "Noren_Stall"):
        m.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(m)
    out.append(n)
print("KG_JAPAN", out)
