"""In PIE: put the player 2.2 m in front of the dummy villager, looking at its face, and take KG_Face.png."""
import unreal
w = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
pc = unreal.GameplayStatics.get_player_controller(w, 0)
pawn = pc.get_controlled_pawn()
dummy = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Character) if a != pawn)
d = dummy.get_actor_location()
fwd = dummy.get_actor_forward_vector()
pos = unreal.Vector(d.x + fwd.x * 220, d.y + fwd.y * 220, d.z)
pawn.set_actor_location(pos, False, True)
look = unreal.MathLibrary.find_look_at_rotation(pawn.get_component_by_class(unreal.CameraComponent).get_world_location(), unreal.Vector(d.x, d.y, d.z + 60))
pc.set_control_rotation(look)
unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=KG_Face")
