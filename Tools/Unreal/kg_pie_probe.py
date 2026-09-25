"""Runs inside the editor (via kg_remote.py -f) during PIE: reports FP arms bone positions in camera space and
optionally applies a live ArmsMesh transform override (OVERRIDE; otherwise read-only), then takes a HighResShot.
Edit the OVERRIDE dict and re-run to iterate without rebuilding C++."""
import unreal

OVERRIDE = None
SHOT = "KG_PIE_Tool"

world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
cam = pawn.get_component_by_class(unreal.CameraComponent)
arms = [c for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "ArmsMesh"][0]
if OVERRIDE:
    l, r = OVERRIDE["loc"], OVERRIDE["rot"]
    arms.set_relative_location_and_rotation(unreal.Vector(*l), unreal.Rotator(pitch=r[0], yaw=r[1], roll=r[2]), False, True)
cam_xf = cam.get_world_transform()
names = [str(n) for n in arms.get_all_socket_names()]
for bone in ("hand_R", "hand_L", "forearm_R", "f_middle_01_R", "camera"):
    if bone in names:
        p = cam_xf.inverse_transform_location(arms.get_socket_location(bone))
        print(f"KG_BONE {bone}: fwd={p.x:.1f} right={p.y:.1f} up={p.z:.1f}")
mesh = arms.get_editor_property("skeletal_mesh_asset")
print("KG_ARMS rel", arms.get_editor_property("relative_location"), arms.get_editor_property("relative_rotation"),
      "mesh", mesh.get_name() if mesh else None, "visible", arms.is_visible(), "bones", len(names))
o, e, r = unreal.SystemLibrary.get_component_bounds(arms)
print("KG_ARMS bounds", cam_xf.inverse_transform_location(o), e, "cam", cam_xf.translation)
unreal.SystemLibrary.execute_console_command(world, f"HighResShot 1280x720 filename={SHOT}")
