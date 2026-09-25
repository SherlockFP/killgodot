"""Character toon look (QA-002-03): rim light + shadow lift on M_KG_Character, and a custom-depth ink outline
post-process (M_KG_PP_Outline) placed in the level via an unbound PostProcessVolume.

Materials (editor CLOSED): UnrealEditor-Cmd.exe KillGodot.uproject -run=PythonScript -Script="...kg_toon_look.py materials"
Volume (live editor):      python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_toon_look.py
Idempotent: recreates both materials and re-points every MI_KG_* instance, reuses the volume if present.
NEVER use MaterialEditingLibrary.delete_all_material_expressions on a loaded material: it asserts (!IsRooted)
and takes the whole editor down (crashed 2026-09-24).
Characters opt in with SetRenderCustomDepth(true) (AKGCharacter does this for its body mesh).
"""
import unreal

MAT_CHAR = "/Game/KillGodot/Characters/Villager/Materials/M_KG_Character"
PP_DIR = "/Game/KillGodot/Materials"
PP_NAME = "M_KG_PP_Outline"
VOLUME_LABEL = "KG_PP_Look"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector(m, name, rgb, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def fresh_material(path):
    folder, name = path.rsplit("/", 1)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def build_character_graph(m):
    """Vibrant base colour + warm fresnel rim + shadow lift (keeps faces readable in shade)."""
    m.set_editor_property("used_with_skeletal_mesh", True)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 0)
    tex.set_editor_property("parameter_name", "BaseColor")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse"))
    tint = vector(m, "Tint", (1, 1, 1), -1100, 250)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 100)
    mel.connect_material_expressions(tex, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    sat = scalar(m, "Desaturation", -0.3, -800, 300)  # negative = more saturated (vibrant art direction)
    desat = mel.create_material_expression(m, unreal.MaterialExpressionDesaturation, -550, 100)
    mel.connect_material_expressions(mul, "", desat, "")
    mel.connect_material_expressions(sat, "", desat, "Fraction")
    mel.connect_material_property(desat, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = scalar(m, "Roughness", 0.75, -550, 350)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Shadow lift: a little self-colour so shaded faces read as the character's colour, not mud.
    lift = scalar(m, "ShadowLift", 0.14, -550, 500)
    lifted = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 450)
    mel.connect_material_expressions(desat, "", lifted, "A")
    mel.connect_material_expressions(lift, "", lifted, "B")

    # Rim: fresnel x warm colour x base colour so the rim stays in the character's palette.
    fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -550, 650)
    fres.set_editor_property("exponent", 3.0)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    rim_col = vector(m, "RimColor", (1.0, 0.86, 0.7), -550, 800)
    rim_str = scalar(m, "RimStrength", 0.45, -550, 950)
    rim_a = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 700)
    mel.connect_material_expressions(fres, "", rim_a, "A")
    mel.connect_material_expressions(rim_col, "", rim_a, "B")
    rim_b = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 750)
    mel.connect_material_expressions(rim_a, "", rim_b, "A")
    mel.connect_material_expressions(rim_str, "", rim_b, "B")
    rim_c = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 0, 700)
    mel.connect_material_expressions(rim_b, "", rim_c, "A")
    mel.connect_material_expressions(desat, "", rim_c, "B")
    emis = mel.create_material_expression(m, unreal.MaterialExpressionAdd, 150, 550)
    mel.connect_material_expressions(lifted, "", emis, "A")
    mel.connect_material_expressions(rim_c, "", emis, "B")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)


OUTLINE_HLSL = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 px = View.BufferSizeAndInvSize.zw * Thickness * (View.ViewSizeAndInvSize.y / 1080.0);
float3 scene = SceneTextureLookup(uv, 14, false).rgb;
float sd = SceneTextureLookup(uv, 1, false).r;
float c = SceneTextureLookup(uv, 13, false).r;
float nearest = c;
float edge = 0;
float2 o[8] = { float2(1,0), float2(-1,0), float2(0,1), float2(0,-1),
                float2(0.7,0.7), float2(-0.7,0.7), float2(0.7,-0.7), float2(-0.7,-0.7) };
for (int i = 0; i < 8; i++)
{
    float n = SceneTextureLookup(uv + o[i] * px, 13, false).r;
    nearest = min(nearest, n);
    float m = min(n, c);
    edge = max(edge, saturate((abs(n - c) - Threshold * m) / (0.05 * m + 1.0)));
}
// Never draw through walls: the outlined surface must be the visible one (no wallhack in a deduction game).
float visible = step(nearest, sd + 2.0);
float fade = saturate(1.0 - (nearest - FadeStart) / FadeRange);
return lerp(scene, Ink.rgb, edge * visible * fade);
"""


def build_outline():
    m = fresh_material(f"{PP_DIR}/{PP_NAME}")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    for loc in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
        if hasattr(unreal.BlendableLocation, loc):
            m.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, loc))
            break
    # Referenced so the scene-texture lookups in the custom node are compiled in.
    st_col = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 0)
    st_col.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    st_cd = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 200)
    st_cd.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    st_sd = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 400)
    st_sd.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)

    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 100)
    custom.set_editor_property("code", OUTLINE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "KG ink outline")
    names = ["Thickness", "Threshold", "FadeStart", "FadeRange", "Ink", "Unused0", "Unused1", "Unused2"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(scalar(m, "Thickness", 1.6, -700, 600), "", custom, "Thickness")
    mel.connect_material_expressions(scalar(m, "Threshold", 0.08, -700, 700), "", custom, "Threshold")
    mel.connect_material_expressions(scalar(m, "FadeStart", 2500.0, -700, 800), "", custom, "FadeStart")
    mel.connect_material_expressions(scalar(m, "FadeRange", 2500.0, -700, 900), "", custom, "FadeRange")
    ink = vector(m, "Ink", (0.025, 0.018, 0.02), -700, 1000)
    mel.connect_material_expressions(ink, "", custom, "Ink")
    mel.connect_material_expressions(st_col, "Color", custom, "Unused0")
    mel.connect_material_expressions(st_cd, "Color", custom, "Unused1")
    mel.connect_material_expressions(st_sd, "Color", custom, "Unused2")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def place_volume(outline):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    vol = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == VOLUME_LABEL), None)
    if not vol:
        vol = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
        vol.set_actor_label(VOLUME_LABEL)
    vol.set_editor_property("unbound", True)
    settings = vol.get_editor_property("settings")
    wb = unreal.WeightedBlendables()
    wb.set_editor_property("array", [unreal.WeightedBlendable(1.0, outline)])
    settings.set_editor_property("weighted_blendables", wb)
    vol.set_editor_property("settings", settings)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return vol


def reparent_instances(master):
    folder = MAT_CHAR.rsplit("/", 1)[0]
    n = 0
    for path in eal.list_assets(folder, False, False):
        mi = unreal.load_asset(path)
        if isinstance(mi, unreal.MaterialInstanceConstant):
            mel.set_material_instance_parent(mi, master)
            eal.save_loaded_asset(mi)
            n += 1
    return n


# Runs on exec (commandlet and remote exec do not set __name__ == "__main__").
import sys
# "materials" (safe headless: -run=PythonScript -Script="...kg_toon_look.py materials"), "volume" (live editor),
# or "all". Default is "volume" so a remote exec never rebuilds materials. Recreating materials in a live editor can fail while assets are in use; prefer headless.
mode = next((a for a in sys.argv[1:] if a in ("materials", "volume", "all")), "volume")
if mode in ("materials", "all"):
    master = fresh_material(MAT_CHAR)
    build_character_graph(master)
    unreal.log(f"KG_TOON: reparented {reparent_instances(master)} instances")
    build_outline()
if mode in ("volume", "all"):
    vol = place_volume(unreal.load_asset(f"{PP_DIR}/{PP_NAME}"))
    unreal.log(f"KG_TOON: volume {vol.get_actor_label()}")
unreal.log(f"KG_TOON ok ({mode})")
print("KG_TOON ok")
