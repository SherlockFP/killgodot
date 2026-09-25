"""Import Art/Packed/KG_WaterProps.glb (fish, rowboat, oar, rod, bobber, shovel, dig mound/hole, doorbell) and give
them vertex-colour materials. Live-safe (new assets; only adds nodes to M_KG_Fish).

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_water_props.py --timeout 600

Vertex colours are sRGB (like the terrain), so materials square them for an approximate linear conversion.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/KillGodot/Env/WaterProps"
MESHES = f"{DEST}/KG_WaterProps/StaticMeshes"


def prop_vc_material():
    path = "/Game/KillGodot/Materials/M_KG_PropVC"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_KG_PropVC", "/Game/KillGodot/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_instanced_static_meshes", True)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -700, 0)
    sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 0)
    mel.connect_material_expressions(vc, "", sq, "A")
    mel.connect_material_expressions(vc, "", sq, "B")
    mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -450, 200)
    r.set_editor_property("r", 0.6)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def linearise_fish():
    m = unreal.load_asset("/Game/KillGodot/Materials/M_KG_Fish")
    src = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR)
    if src and src.get_class().get_name() == "MaterialExpressionVertexColor":
        sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -700, -100)
        mel.connect_material_expressions(src, "", sq, "A")
        mel.connect_material_expressions(src, "", sq, "B")
        mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    return m


if not eal.does_asset_exist(f"{MESHES}/SM_KG_Rowboat"):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_WaterProps.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])

vc_mat = prop_vc_material()
fish_mat = linearise_fish()
report = []
for path in eal.list_assets(MESHES, False, False):
    m = unreal.load_asset(path)
    if not isinstance(m, unreal.StaticMesh):
        continue
    name = m.get_name()
    is_fish = name.startswith("SM_KG_Fish_")
    for i in range(m.get_num_sections(0)):
        m.set_material(i, fish_mat if is_fish else vc_mat)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    m.set_editor_property("nanite_settings", ns)
    unreal.EditorStaticMeshLibrary.remove_collisions(m)
    if name in ("SM_KG_Rowboat", "SM_KG_DigMound", "SM_KG_Doorbell"):
        # Boats float as physics bodies: a convex hull keeps them stable on the waves.
        unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
    eal.save_loaded_asset(m)
    report.append(name)
print("KG_WATERPROPS", report)
