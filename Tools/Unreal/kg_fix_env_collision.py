"""Interchange glTF imports leave per-section collision OFF, so complex-as-simple meshes had no collision at all
(players walked through walls, interact traces missed doors). Enable it for every environment mesh except tiny
decoration, and make Nanite fallbacks full resolution so collision matches what is rendered.

  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_fix_env_collision.py --timeout 900   (PIE stopped)
"""
import unreal

ROOTS = ["/Game/KillGodot/Env/KG_Village", "/Game/KillGodot/Env/KG_Nature", "/Game/KillGodot/Env/Terrain"]
DECO = ("Grass_", "Flower_", "Clover_", "Petal_", "Fern_", "Plant_", "Mushroom_", "Pebble_", "Prop_Vine", "Bush_")
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
eal = unreal.EditorAssetLibrary
fixed = 0
for root in ROOTS:  # (doors get a box instead: thin complex leaves were missed by traces)
    for path in eal.list_assets(root, True, False):
        m = unreal.load_asset(path)
        if not isinstance(m, unreal.StaticMesh) or m.get_name().startswith(DECO):
            continue
        changed = False
        for i in range(m.get_num_sections(0)):
            if not sub.is_section_collision_enabled(m, 0, i):
                sub.enable_section_collision(m, True, 0, i)
                changed = True
        ns = m.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled") and ns.get_editor_property("fallback_relative_error") != 0.0:
            ns.set_editor_property("fallback_relative_error", 0.0)
            m.set_editor_property("nanite_settings", ns)
            changed = True
        if changed:
            eal.save_loaded_asset(m)
            fixed += 1
unreal.log(f"KG_COLLISION: enabled section collision on {fixed} meshes")
print(f"KG_COLLISION {fixed}")
