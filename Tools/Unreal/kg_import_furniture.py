"""Import the house-interior furniture: JayBee Kitchen + Bedroom (CC0, packed by Tools/Blender/kg_pack_kit.py into
Art/Packed/KG_Kitchen.glb / KG_Bedroom.glb) and our procedural Art/Packed/KG_Interior.glb (hearth, rugs, herbs,
railings, firewood, food; Tools/Blender/kg_make_interior_props.py). Creates NEW assets only, so it is live-safe:

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_furniture.py --timeout 900      (PIE stopped)

-> /Game/KillGodot/Env/Furniture/{KG_Kitchen,KG_Bedroom,KG_Interior}/StaticMeshes/...
A kit is only imported when its meshes are missing (Interchange RE-imports keep stale vertex colours and rebind
materials). Delete a kit's folder by hand first to force a fresh import. The setup pass after it is idempotent and
runs every time (run the script a second time after a fresh import: Interchange may bind its own materials late):
  - JayBee meshes -> MI_KG_Palette_{Kitchen,Bedroom} (plain lit palette material; the glTF default instance rendered
    them near-black), procedural props -> M_KG_PropVC (vertex colour) / M_KG_Embers (emissive fire),
  - collision: a simple box on furniture you bump into, none on clutter/rugs/herbs, per-poly on the stair rail,
  - Nanite off (tiny low-poly meshes); palette atlases point-sampled without mips so swatches never bleed.
"""
import json
import struct

import unreal

PACKED = "D:/Kill Godot/Art/Packed"
DEST = "/Game/KillGodot/Env/Furniture"
KITS = ["KG_Kitchen", "KG_Bedroom", "KG_Interior"]
MATS = "/Game/KillGodot/Materials"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

# Walk-through decoration (no collision at all).
NO_COLLISION = ("Kitchen_Book", "Kitchen_Bottle", "Kitchen_Bowl", "Kitchen_Cup", "Kitchen_Jar", "Kitchen_KitchenKnife",
                "Kitchen_Laddle", "Kitchen_PepperShaker", "Kitchen_Plate", "Kitchen_SmallPan", "Kitchen_SmallPot",
                "Kitchen_Spoon", "Kitchen_YellowBottle", "Kitchen_YellowJar", "Bedroom_WallShelf",
                "SM_KG_Rug_", "SM_KG_Herb", "SM_KG_Bread", "SM_KG_Cheese")
PER_POLY = ("SM_KG_StairRail",)


def glb_mesh_names(kit):
    """Mesh object names inside a packed .glb (= the static mesh asset names Interchange creates)."""
    data = open(f"{PACKED}/{kit}.glb", "rb").read()
    length = struct.unpack("<I", data[12:16])[0]
    doc = json.loads(data[20:20 + length])
    return sorted({n["name"] for n in doc.get("nodes", []) if "mesh" in n})


def import_glb(kit):
    """Default Interchange glTF pipeline (it lays assets out as <kit>/StaticMeshes|Materials|Textures and imports
    COLOR_0 as vertex colour); fresh imports only."""
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f"{PACKED}/{kit}.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", False)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


def vc_material():
    """M_KG_PropVC: vertex colour (sRGB, squared ~ linear) -> base colour. Shared with the water props."""
    m = unreal.load_asset(f"{MATS}/M_KG_PropVC")
    if m:
        return m
    m = tools.create_asset("M_KG_PropVC", MATS, unreal.Material, unreal.MaterialFactoryNew())
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -700, 0)
    sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 0)
    mel.connect_material_expressions(vc, "", sq, "A")
    mel.connect_material_expressions(vc, "", sq, "B")
    mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def embers_material():
    """M_KG_Embers: emissive vertex colour for the hearth fire (NEW asset, never edited once it exists)."""
    m = unreal.load_asset(f"{MATS}/M_KG_Embers")
    if m:
        return m
    m = tools.create_asset("M_KG_Embers", MATS, unreal.Material, unreal.MaterialFactoryNew())
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, 0)
    sq = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -650, 0)
    mel.connect_material_expressions(vc, "", sq, "A")
    mel.connect_material_expressions(vc, "", sq, "B")
    k = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -650, 200)
    k.set_editor_property("r", 6.0)
    em = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 100)
    mel.connect_material_expressions(sq, "", em, "A")
    mel.connect_material_expressions(k, "", em, "B")
    mel.connect_material_property(sq, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def palette_materials():
    """M_KG_FurniturePalette (texture param 'Palette' -> base colour, roughness 0.8) + one instance per JayBee kit."""
    m = unreal.load_asset(f"{MATS}/M_KG_FurniturePalette")
    if not m:
        m = tools.create_asset("M_KG_FurniturePalette", MATS, unreal.Material, unreal.MaterialFactoryNew())
        tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
        tex.set_editor_property("parameter_name", "Palette")
        tex.set_editor_property("texture", unreal.load_asset(f"{DEST}/KG_Kitchen/Textures/Texture"))
        mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200)
        r.set_editor_property("r", 0.8)
        mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    out = {}
    for kit, tex_name in (("Kitchen", "Texture"), ("Bedroom", "Texture2")):
        tex = unreal.load_asset(f"{DEST}/KG_{kit}/Textures/{tex_name}")
        if tex:
            tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            eal.save_loaded_asset(tex)
        mi = unreal.load_asset(f"{MATS}/MI_KG_Palette_{kit}")
        if not mi:
            mi = tools.create_asset(f"MI_KG_Palette_{kit}", MATS, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property("parent", m)
            mel.set_material_instance_texture_parameter_value(mi, "Palette", tex)
            eal.save_loaded_asset(mi)
        out[kit] = mi
    return out


def setup_mesh(mesh, vc, emb, pal):
    name = mesh.get_name()
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        if name.startswith("SM_KG_"):
            mesh.set_material(i, emb if "Embers" in slot_name else vc)
        elif name.startswith("Kitchen_"):
            mesh.set_material(i, unreal.load_asset(f"{DEST}/KG_Kitchen/Materials/Heat") if "Heat" in slot_name
                              else pal["Kitchen"])
        elif name.startswith("Bedroom_"):
            mesh.set_material(i, pal["Bedroom"])
    ns = mesh.get_editor_property("nanite_settings")
    if ns.get_editor_property("enabled"):
        ns.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", ns)
    sms.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if name.startswith(PER_POLY):
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        for i in range(mesh.get_num_sections(0)):
            sms.enable_section_collision(mesh, True, 0, i)
    elif name.startswith(NO_COLLISION):
        for i in range(mesh.get_num_sections(0)):
            sms.enable_section_collision(mesh, False, 0, i)
    else:
        sms.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    eal.save_loaded_asset(mesh)
    bb = mesh.get_bounding_box()
    return [round(v, 1) for v in (bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z)]


def main():
    for kit in KITS:
        names = glb_mesh_names(kit)
        if not unreal.load_asset(f"{DEST}/{kit}/StaticMeshes/{names[0]}"):
            paths = import_glb(kit)
            unreal.log(f"KG_FURNITURE: imported {kit} -> {len(paths)} assets under {DEST}/{kit}")
    vc_mat, emb_mat, pal = vc_material(), embers_material(), palette_materials()
    done = 0
    for kit in KITS:
        for n in glb_mesh_names(kit):
            m = unreal.load_asset(f"{DEST}/{kit}/StaticMeshes/{n}")
            if not isinstance(m, unreal.StaticMesh):
                unreal.log_warning(f"KG_FURNITURE missing {kit}/{n}")
                continue
            print("KG_FURNITURE", n, setup_mesh(m, vc_mat, emb_mat, pal), "vc" if sms.has_vertex_colors(m) else "")
            done += 1
    print(f"KG_FURNITURE set up {done} static meshes")


main()
