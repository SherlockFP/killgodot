"""SPRINT-022 per-house paint: M_KG_PaintedWood + one MI per colour of Tools/Level/kg_archetypes_v2.PAINTS.

Painted planks for shutters and doors: the kit's wood-trim texture (read from MI_WoodTrim_Wear) is desaturated and
multiplied by the paint colour, its dark grooves stay dark, a little bare wood shows at worn edges (Wear).
New assets only, under /Game/KillGodot/Materials/S22/ (safe while the editor is open: nothing existing is touched).
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_make_paint_v2.py [rebuild]" -unattended -nosplash -nopause -nullrhi
The v2 builder loads MI_KG_Paint_<name> when it exists (kg_build_village_v2.paint_mi) and falls back to plain wood.
"""
import sys

import unreal

sys.path.insert(0, "D:/Kill Godot/Tools/Level")
import kg_archetypes_v2 as ARCH  # noqa: E402

DIR = "/Game/KillGodot/Materials/S22"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
KIT_WOOD = "/Game/KillGodot/Env/KG_Village/Materials/MI_WoodTrim_Wear"


def kit_texture():
    mi = unreal.load_asset(KIT_WOOD)
    if not mi:
        return None
    for n in mel.get_texture_parameter_names(mi):
        t = mel.get_material_instance_texture_parameter_value(mi, n)
        if t and ("base" in str(n).lower() or "color" in str(n).lower() or "diffuse" in str(n).lower()):
            return t
    for n in mel.get_texture_parameter_names(mi):
        t = mel.get_material_instance_texture_parameter_value(mi, n)
        if t:
            return t
    return None


def master(rebuild=False):
    path = f"{DIR}/M_KG_PaintedWood"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not rebuild:
            return m
        mel.delete_all_material_expressions(m)          # headless only
    else:
        m = tools.create_asset("M_KG_PaintedWood", DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 0)
    tex.set_editor_property("parameter_name", "WoodTex")
    kt = kit_texture()
    if kt:
        tex.set_editor_property("texture", kt)
    paint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1100, 300)
    paint.set_editor_property("parameter_name", "Paint")
    paint.set_editor_property("default_value", unreal.LinearColor(0.36, 0.55, 0.36, 1.0))
    wear = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1100, 450)
    wear.set_editor_property("parameter_name", "Wear")
    wear.set_editor_property("default_value", 0.18)
    code = r"""
float l = dot(W.rgb, float3(0.30, 0.59, 0.11));
float grain = saturate(l * 2.4);                       // kit wood is dark: normalise to 0..1
float3 painted = P.rgb * (0.62 + 0.55 * grain);
float bare = saturate((grain - (1.0 - Wear)) * 6.0);   // the brightest (worn) plank edges show wood
return lerp(painted, W.rgb * 1.1, bare);
"""
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -700, 150)
    cu.set_editor_property("code", code)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("W", "P", "Wear"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    mel.connect_material_expressions(tex, "RGB", cu, "W")
    mel.connect_material_expressions(paint, "", cu, "P")
    mel.connect_material_expressions(wear, "", cu, "Wear")
    mel.connect_material_property(cu, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 400)
    r.set_editor_property("r", 0.62)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def instances(m):
    out = {}
    for name, rgb in ARCH.PAINTS.items():
        if rgb is None:
            continue
        path = f"{DIR}/MI_KG_Paint_{name}"
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
        if not mi:
            mi = tools.create_asset(f"MI_KG_Paint_{name}", DIR, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", m)
        mel.set_material_instance_vector_parameter_value(mi, "Paint", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[name] = path
    return out


if __name__ != "kg_make_paint_v2_import":
    M_ = master(rebuild=any(a.lower() == "rebuild" for a in sys.argv[1:]))
    made = instances(M_)
    print(f"KG_PAINT ok: {len(made)} paints in {DIR}; wood texture {kit_texture()}")
