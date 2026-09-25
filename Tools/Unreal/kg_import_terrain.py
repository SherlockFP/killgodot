"""Import the Morrowmere terrain (Art/Packed/KG_Terrain.glb) and build the terrain + water materials. Headless:

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_terrain.py"
"""
import unreal

DEST = "/Game/KillGodot/Env/Terrain"
MAT = "/Game/KillGodot/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def fresh_material(folder, name):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def color(m, name, rgb, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def terrain_material():
    m = fresh_material(MAT, "M_KG_Terrain")
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -600, 0)
    # Vertex colours arrive in sRGB bytes; square them for an approximate linear conversion.
    sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(vc, "", sq, "A")
    mel.connect_material_expressions(vc, "", sq, "B")
    mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.92, -350, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def water_material():
    """Stylised sea: fresnel from deep teal to bright shallow colour, glossy, gentle panning ripples."""
    m = fresh_material(MAT, "M_KG_Water")
    deep = color(m, "Deep", (0.01, 0.12, 0.22), -900, -100)
    light = color(m, "Shallow", (0.05, 0.42, 0.52), -900, 100)
    fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 300)
    fres.set_editor_property("exponent", 2.5)
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -600, 0)
    mel.connect_material_expressions(light, "", lerp, "A")
    mel.connect_material_expressions(deep, "", lerp, "B")
    mel.connect_material_expressions(fres, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.06, -600, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 0.9, -600, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    # Ripples: engine noise normal texture panned in world space.
    tex = unreal.load_asset("/Engine/EngineMaterials/Water_Normal") or unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1300, 500)
    mask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1150, 500)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mel.connect_material_expressions(wp, "", mask, "")
    div = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -1000, 500)
    mel.connect_material_expressions(mask, "", div, "A")
    mel.connect_material_expressions(scalar(m, "RippleScale", 900.0, -1150, 650), "", div, "B")
    pan = mel.create_material_expression(m, unreal.MaterialExpressionPanner, -850, 500)
    pan.set_editor_property("speed_x", 0.02)
    pan.set_editor_property("speed_y", 0.035)
    mel.connect_material_expressions(div, "", pan, "Coordinate")
    samp = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -650, 500)
    samp.set_editor_property("texture", tex)
    samp.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(pan, "", samp, "UVs")
    mel.connect_material_property(samp, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


t = unreal.AssetImportTask()
t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_Terrain.glb")
t.set_editor_property("destination_path", DEST)
t.set_editor_property("automated", True)
t.set_editor_property("replace_existing", True)
t.set_editor_property("save", True)
tools.import_asset_tasks([t])
# Re-imports keep the tuned materials (MI_KG_Terrain tint etc.); build them only the first time.
tm = unreal.load_asset(f"{MAT}/MI_KG_Terrain") or (unreal.load_asset(f"{MAT}/M_KG_Terrain") if eal.does_asset_exist(f"{MAT}/M_KG_Terrain") else terrain_material())
if not eal.does_asset_exist(f"{MAT}/M_KG_Water"):
    water_material()
for p in t.get_editor_property("imported_object_paths"):
    mesh = unreal.load_asset(str(p))
    if isinstance(mesh, unreal.StaticMesh):
        mesh.set_material(0, tm)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        ns = mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)   # full-res render + collision (Nanite would collide with the fallback)
        mesh.set_editor_property("nanite_settings", ns)
        eal.save_loaded_asset(mesh)
        unreal.log(f"KG_TERRAIN: {mesh.get_path_name()} bounds {mesh.get_bounds().box_extent}")
