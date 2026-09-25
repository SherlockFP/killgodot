"""Import Art/Packed/KG_PirateProps.glb (piers, planks, barrels, treasure chests, crates, flags, torches, cannon) into
/Game/KillGodot/Env/Pirate. The Blender export bakes the pack's palette texture into vertex colours (no materials),
so every mesh uses M_KG_PropVC. Live-safe (new assets only). Piers/planks collide with their triangles; the rest
get boxes.

Source glb is sanitised first: Tools/Blender/kg_sanitize_glb.py (triangulate/merge; avoids "Bad MeshDescription").
  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_import_pirate.py --timeout 600
"""
import unreal

eal = unreal.EditorAssetLibrary
DEST = "/Game/KillGodot/Env/PirateC3"   # the first (material-broken) import left unsaved ghosts under /Env/Pirate
MESHES = f"{DEST}/KG_PirateProps_Clean2/StaticMeshes"
NAMES = (["pier_0", "pier_1", "pier_2", "Planks_0", "Planks_1", "Planks_2", "Planks_3", "Broken_Flag_0", "Broken_Flag_1",
          "cannon_0", "cannon_0_001", "Cannon_Ball_0", "Cannon_Ball_1", "chest_common_0", "chest_diamond_0",
          "chest_gold_0", "chest_silver_0", "crates_0", "crates_1", "FlagLow_0", "FlagLow_1", "FlagTall_0", "FlagTall_1",
          "Torch_0", "Torch_1", "Torch_2", "Torch_3"] + [f"Barrel_{i}" for i in range(14)])

if not eal.does_asset_exist(f"{MESHES}/SM_KG_Pirate_pier_0"):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", "D:/Kill Godot/Art/Packed/KG_PirateProps_Clean2.glb")
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])

vc = unreal.load_asset("/Game/KillGodot/Materials/M_KG_PropVC")
out = []
for name in NAMES:
    m = unreal.load_asset(f"{MESHES}/SM_KG_Pirate_{name}")
    if not m:
        out.append(f"{name} MISSING")
        continue
    for i in range(max(1, len(m.get_editor_property("static_materials")))):
        m.set_material(i, vc)
    if "pier" in name or "Planks" in name:
        m.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        for s in range(m.get_num_sections(0)):
            sub.enable_section_collision(m, True, 0, s)
    else:
        unreal.EditorStaticMeshLibrary.remove_collisions(m)
        unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    m.set_editor_property("nanite_settings", ns)
    eal.save_loaded_asset(m)
    b = m.get_bounds()
    out.append(f"{name} {round(b.box_extent.x * 2)}x{round(b.box_extent.y * 2)} z {round(b.origin.z - b.box_extent.z)}..{round(b.origin.z + b.box_extent.z)}")
print("KG_PIRATE_IMPORT\n" + "\n".join(out))
