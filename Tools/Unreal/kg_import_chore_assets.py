"""Chore minigame assets (headless, editor closed):
  - imports the synthesised minigame sounds Art/Audio/S_Chore_*.wav + S_UI_*.wav into /Game/KillGodot/Audio
    (make them first: python Tools/Audio/kg_synth_sfx.py Art/Audio "S_Chore_,S_UI_");
  - creates the two materials the visual-chore effects use (AKGChoreFx, Source/KillGodot/Chores/KGChoreFx.cpp):
      M_KG_ChoreGlow  unlit emissive, params Color (vector) x Intensity (scalar)       - lighthouse / lamp / candle flames
      M_KG_ChoreSmoke unlit translucent, params Color, Opacity, soft Fresnel edge       - bakery chimney smoke
    Existing materials are left alone (pass --rebuild-materials to recreate them).

  "D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/Kill Godot/KillGodot.uproject"
      -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_chore_assets.py" -unattended -nullrhi -nosplash -stdout
"""
import glob
import sys

import unreal

AUDIO = "/Game/KillGodot/Audio"
MATS = "/Game/KillGodot/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
rebuild = any("--rebuild-materials" in a for a in sys.argv) or "--rebuild-materials" in unreal.SystemLibrary.get_command_line()

# ---- sounds -------------------------------------------------------------------------------------------------------
tasks = []
for path in sorted(glob.glob("D:/Kill Godot/Art/Audio/S_Chore_*.wav") + glob.glob("D:/Kill Godot/Art/Audio/S_UI_*.wav")):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", AUDIO)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
tools.import_asset_tasks(tasks)
sounds = 0
for t in tasks:
    for obj_path in t.get_editor_property("imported_object_paths") or []:
        w = unreal.load_asset(obj_path)
        if isinstance(w, unreal.SoundWave):
            w.set_editor_property("looping", False)
            eal.save_loaded_asset(w)
            sounds += 1
print(f"KG_CHORE_ASSETS sounds imported: {sounds} / {len(tasks)}")


# ---- materials ----------------------------------------------------------------------------------------------------
def make_material(name):
    path = f"{MATS}/{name}"
    if eal.does_asset_exist(path):
        if not rebuild:
            print(f"KG_CHORE_ASSETS {name} exists (kept)")
            return None
        eal.delete_asset(path)
    return tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())


def param_vector(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def param_scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


glow = make_material("M_KG_ChoreGlow")
if glow:
    glow.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = param_vector(glow, "Color", unreal.LinearColor(r=1.0, g=0.7, b=0.3, a=1.0), -600, 0)
    intensity = param_scalar(glow, "Intensity", 10.0, -600, 220)
    mul = mel.create_material_expression(glow, unreal.MaterialExpressionMultiply, -300, 80)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(intensity, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(glow)
    eal.save_loaded_asset(glow)
    print("KG_CHORE_ASSETS M_KG_ChoreGlow created")

smoke = make_material("M_KG_ChoreSmoke")
if smoke:
    smoke.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    smoke.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    color = param_vector(smoke, "Color", unreal.LinearColor(r=0.85, g=0.83, b=0.8, a=1.0), -700, -100)
    opacity = param_scalar(smoke, "Opacity", 0.6, -700, 150)
    fresnel = mel.create_material_expression(smoke, unreal.MaterialExpressionFresnel, -700, 300)
    fresnel.set_editor_property("exponent", 1.6)
    soft = mel.create_material_expression(smoke, unreal.MaterialExpressionOneMinus, -450, 300)
    mel.connect_material_expressions(fresnel, "", soft, "")
    mul = mel.create_material_expression(smoke, unreal.MaterialExpressionMultiply, -250, 200)
    mel.connect_material_expressions(opacity, "", mul, "A")
    mel.connect_material_expressions(soft, "", mul, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(smoke)
    eal.save_loaded_asset(smoke)
    print("KG_CHORE_ASSETS M_KG_ChoreSmoke created")

print("KG_CHORE_ASSETS done")
