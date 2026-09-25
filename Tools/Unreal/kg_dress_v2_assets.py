"""v2 dressing helper assets + level fix-ups (UE, headless-safe; called by Tools/Unreal/kg_dress_v2.py).

ensure()  creates once, under /Game/KillGodot/Env/Dress/KG_DressV2/ (re-run safe, never edits existing graphs):
  M_KG_WindowGlow_v2  unlit warm interior glow. Brightness follows the sun: SkyAtmosphereLightIlluminance drives a
                      night factor, so windows are a faint warm tint at golden hour and glow at night (the game's
                      Night phase drops the sun to 0.9 lux, see AKGGameState::UpdatePhaseLighting). Per-instance
                      random dims some panes (HISM), so a street never lights up uniformly.
  SM_KG_GlowCard      /Engine/BasicShapes/Plane copy with M_KG_WindowGlow: the "lightbox" behind shell windows.
  M_KG_LighthouseBeam_v2 unlit additive two-sided cone, bright at the apex, soft edges; strong at night only.
  SM_KG_BeamCone      /Engine/BasicShapes/Cone copy with M_KG_LighthouseBeam (spun by an AKGSpinner in dress_coast).
  M_KG_BrookWater_v2  calm dark teal water with a slow world-space ripple tint (replaces the pale M_KG_PondWater on
                      the brook ribbons and the mill pond).
  T_KG_RoundTiles_Slate  slate version of the kit roof texture (Art/Textures/T_KG_RoundTiles_Slate_BaseColor.png,
                      made by Tools/Level/make_slate_texture.py) -> MI_KG_crown_hill_RoundTiles, so the Crown roofs read
                      blue-grey slate instead of dark brown (a multiply alone cannot desaturate the orange tiles).
level_fixups()  per build: brook water material, crown slate MI (the builder resets its BaseColorFactor each build).
"""
import os

import unreal

DIR = "/Game/KillGodot/Env/Dress/KG_DressV2"
ROOT = "D:/Kill Godot"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def _exists(name):
    return eal.does_asset_exist(f"{DIR}/{name}")


def _new_material(name):
    return tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())


def _expr(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def _const(m, v, x, y):
    c = _expr(m, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c


def _const3(m, rgb, x, y):
    c = _expr(m, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(r=rgb[0], g=rgb[1], b=rgb[2], a=1.0))
    return c


def _bin(m, cls, a, b, x, y):
    n = _expr(m, cls, x, y)
    mel.connect_material_expressions(a, "", n, "A")
    mel.connect_material_expressions(b, "", n, "B")
    return n


def _night(m, x=-1400, y=0):
    """0 at golden hour/day, 1 at the game's Night phase: from the atmosphere sun illuminance at the pixel."""
    try:
        il = _expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, x, y)
        il.set_editor_property("light_index", 0)
    except Exception:
        return _const(m, 0.0, x, y)
    lum = _bin(m, unreal.MaterialExpressionDotProduct, il, _const3(m, (0.3, 0.59, 0.11), x, y + 120), x + 200, y)
    d = _bin(m, unreal.MaterialExpressionSubtract, _const(m, 2.2, x + 200, y + 150), lum, x + 400, y)
    s = _bin(m, unreal.MaterialExpressionMultiply, d, _const(m, 0.6, x + 400, y + 150), x + 600, y)
    sat = _expr(m, unreal.MaterialExpressionSaturate, x + 800, y)
    mel.connect_material_expressions(s, "", sat, "")
    return sat


def _lerp(m, a, b, alpha, x, y):
    n = _expr(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a, "", n, "A")
    mel.connect_material_expressions(b, "", n, "B")
    mel.connect_material_expressions(alpha, "", n, "Alpha")
    return n


WINDOW_MAT, BEAM_MAT, WATER_MAT = "M_KG_WindowGlow_v2", "M_KG_LighthouseBeam_v2", "M_KG_BrookWater_v2"


def window_glow():
    if _exists(WINDOW_MAT):
        return 0
    m = _new_material(WINDOW_MAT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("two_sided", True)
    col = _expr(m, unreal.MaterialExpressionVectorParameter, -900, -300)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(r=1.0, g=0.52, b=0.2, a=1.0))
    night = _night(m, -1600, 200)
    k = _bin(m, unreal.MaterialExpressionAdd, _const(m, 0.35, -700, 100),
             _bin(m, unreal.MaterialExpressionMultiply, night, _const(m, 6.0, -800, 300), -650, 250), -500, 150)
    rnd = _expr(m, unreal.MaterialExpressionPerInstanceRandom, -900, 450)
    var = _lerp(m, _const(m, 0.25, -700, 420), _const(m, 1.0, -700, 480), rnd, -500, 450)
    # a soft vertical gradient: warmer/brighter low (lamp light), a bit dimmer at the top
    uv = _expr(m, unreal.MaterialExpressionTextureCoordinate, -1100, 650)
    v = _expr(m, unreal.MaterialExpressionComponentMask, -900, 650)
    v.set_editor_property("r", False)
    v.set_editor_property("g", True)
    mel.connect_material_expressions(uv, "", v, "")
    grad = _lerp(m, _const(m, 0.55, -700, 620), _const(m, 1.1, -700, 700), v, -500, 650)
    e1 = _bin(m, unreal.MaterialExpressionMultiply, col, k, -300, -100)
    e2 = _bin(m, unreal.MaterialExpressionMultiply, e1, var, -150, 100)
    e3 = _bin(m, unreal.MaterialExpressionMultiply, e2, grad, 0, 200)
    mel.connect_material_property(e3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return 1


def beam():
    if _exists(BEAM_MAT):
        return 0
    m = _new_material(BEAM_MAT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    lp = _expr(m, unreal.MaterialExpressionLocalPosition, -1300, 0)
    z = _expr(m, unreal.MaterialExpressionComponentMask, -1100, 0)
    for ch, on in (("r", False), ("g", False), ("b", True)):
        z.set_editor_property(ch, on)
    mel.connect_material_expressions(lp, "", z, "")
    t = _bin(m, unreal.MaterialExpressionAdd, z, _const(m, 50.0, -1100, 120), -900, 0)
    t = _bin(m, unreal.MaterialExpressionMultiply, t, _const(m, 0.01, -900, 120), -700, 0)
    pw = _expr(m, unreal.MaterialExpressionPower, -500, 0)
    mel.connect_material_expressions(t, "", pw, "Base")
    mel.connect_material_expressions(_const(m, 2.2, -700, 120), "", pw, "Exponent")
    t = pw
    fres = _expr(m, unreal.MaterialExpressionFresnel, -900, 300)
    fres.set_editor_property("exponent", 1.6)
    edge = _expr(m, unreal.MaterialExpressionOneMinus, -700, 300)
    mel.connect_material_expressions(fres, "", edge, "")
    night = _night(m, -1600, 500)
    k = _bin(m, unreal.MaterialExpressionAdd, _const(m, 0.05, -700, 500),
             _bin(m, unreal.MaterialExpressionMultiply, night, _const(m, 0.75, -800, 650), -650, 600), -500, 550)
    col = _const3(m, (1.0, 0.86, 0.55), -500, -200)
    e = _bin(m, unreal.MaterialExpressionMultiply, col, t, -300, -100)
    e = _bin(m, unreal.MaterialExpressionMultiply, e, edge, -150, 100)
    e = _bin(m, unreal.MaterialExpressionMultiply, e, k, 0, 200)
    mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return 1


def brook_water():
    """Calm dark teal water; a slow world-space ripple (two crossed sines, no texture) shifts the tint."""
    if _exists(WATER_MAT):
        return 0
    m = _new_material(WATER_MAT)
    wp = _expr(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tm = _expr(m, unreal.MaterialExpressionTime, -1600, 300)
    waves = []
    for k, (ch, div, speed) in enumerate((("r", 260.0, 0.6), ("g", 340.0, -0.45))):
        c = _expr(m, unreal.MaterialExpressionComponentMask, -1400, k * 200)
        for cc in ("r", "g", "b", "a"):
            c.set_editor_property(cc, cc == ch)
        mel.connect_material_expressions(wp, "", c, "")
        d = _bin(m, unreal.MaterialExpressionDivide, c, _const(m, div, -1400, k * 200 + 100), -1200, k * 200)
        ts = _bin(m, unreal.MaterialExpressionMultiply, tm, _const(m, speed, -1300, k * 200 + 150), -1200, k * 200 + 100)
        a = _bin(m, unreal.MaterialExpressionAdd, d, ts, -1000, k * 200)
        sn = _expr(m, unreal.MaterialExpressionSine, -800, k * 200)
        mel.connect_material_expressions(a, "", sn, "")
        waves.append(sn)
    prod = _bin(m, unreal.MaterialExpressionMultiply, waves[0], waves[1], -600, 100)
    alpha = _bin(m, unreal.MaterialExpressionAdd, _bin(m, unreal.MaterialExpressionMultiply, prod, _const(m, 0.35, -600, 250),
                                                        -450, 150), _const(m, 0.4, -450, 300), -300, 150)
    deep = _const3(m, (0.010, 0.050, 0.058), -400, -150)
    shallow = _const3(m, (0.030, 0.135, 0.130), -400, -50)
    base = _lerp(m, deep, shallow, alpha, -100, 0)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(m, 0.06, -100, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(m, 0.8, -100, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return 1


def mesh_copy(src, name, mat_name):
    """A copy of an engine basic shape carrying one of our materials (re-pointed every run, so a new material version
    takes over without deleting assets)."""
    made = 0
    if _exists(name):
        m = unreal.load_asset(f"{DIR}/{name}")
    else:
        m = eal.duplicate_asset(src, f"{DIR}/{name}")
        made = 1
    if not m:
        return 0
    mat = unreal.load_asset(f"{DIR}/{mat_name}")
    if m.get_material(0) != mat:
        m.set_material(0, mat)
        eal.save_loaded_asset(m)
        made = max(made, 1)
    return made


SLATE_PNG = f"{ROOT}/Art/Textures/T_KG_RoundTiles_Slate_BaseColor.png"
SLATE = f"{DIR}/T_KG_RoundTiles_Slate_BaseColor"


def slate_texture():
    if _exists("T_KG_RoundTiles_Slate_BaseColor") or not os.path.exists(SLATE_PNG):
        return 0
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", SLATE_PNG)
    t.set_editor_property("destination_path", DIR)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    return 1


def ensure():
    if not eal.does_directory_exist(DIR):
        eal.make_directory(DIR)
    made = {"window_glow": window_glow(), "beam": beam(), "brook_water": brook_water(), "slate_tex": slate_texture()}
    made["glow_card"] = mesh_copy("/Engine/BasicShapes/Plane", "SM_KG_GlowCard", WINDOW_MAT)
    made["beam_cone"] = mesh_copy("/Engine/BasicShapes/Cone", "SM_KG_BeamCone", BEAM_MAT)
    return made


def level_fixups():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = {"brook": 0, "slate": False}
    water = unreal.load_asset(f"{DIR}/{WATER_MAT}")
    if water:
        for a in actors.get_all_level_actors():
            if str(a.get_folder_path()) == "V2/Brook":
                comp = a.get_component_by_class(unreal.StaticMeshComponent)
                if comp:
                    comp.set_material(0, water)
                    out["brook"] += 1
    mi = unreal.load_asset("/Game/KillGodot/Env/KG_Village/Materials/District/MI_KG_crown_hill_RoundTiles")
    tex = unreal.load_asset(SLATE)
    if mi and tex:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", tex)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColorFactor", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out["slate"] = True
    return out


def fix_ism_usage(mesh_paths):
    """Meshes drawn through HISM need bUsedWithInstancedStaticMeshes on their base material, or a -game/cooked run
    falls back to the default material (grey checker). Sets the flag on project (/Game) base materials of the given
    meshes (+ the builder's forest meshes) and saves them. Returns the fixed material paths."""
    fixed = []
    seen = set()
    for p in sorted(set(mesh_paths)):
        m = unreal.load_asset(p)
        if not isinstance(m, unreal.StaticMesh):
            continue
        for sm in m.get_editor_property("static_materials"):
            mi = sm.get_editor_property("material_interface")
            if not mi:
                continue
            base = mi.get_base_material() if hasattr(mi, "get_base_material") else mi
            if not base:
                continue
            bp = base.get_path_name().split(".")[0]
            if bp in seen or not bp.startswith("/Game/"):
                continue
            seen.add(bp)
            if not base.get_editor_property("used_with_instanced_static_meshes"):
                base.set_editor_property("used_with_instanced_static_meshes", True)
                mel.recompile_material(base)
                eal.save_loaded_asset(base)
                fixed.append(bp)
    return fixed
