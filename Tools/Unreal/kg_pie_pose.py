"""In PIE: preview an FP arms clip (POSE, frame POS) with the held item hidden, then shoot KG_Pose_<POSE>.png."""
import unreal
POSE = globals().get("POSE", "relax")
POS = globals().get("POS", 0.3)
w = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
arms = [c for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "ArmsMesh"][0]
item = [c for c in pawn.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "HeldItem"][0]
item.set_visibility(False)
arms.play_animation(unreal.load_asset(f"/Game/KillGodot/Characters/FPArms/arms_rig/SkeletalMeshes/arms_rig{POSE}"), False)
arms.set_position(POS, False)
arms.set_play_rate(0.0)
unreal.SystemLibrary.execute_console_command(w, f"HighResShot 960x540 filename=KG_Pose_{POSE}")
