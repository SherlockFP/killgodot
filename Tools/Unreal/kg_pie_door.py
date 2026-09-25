"""In PIE: stand the player 2 m in front of the nearest KGDoor to the plaza, facing it; STEP=face|open|shot."""
import unreal

STEP = globals().get("STEP", "face")
w = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
pc = unreal.GameplayStatics.get_player_controller(w, 0)
pawn = pc.get_controlled_pawn()
doors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_class().get_name() == "KGDoor"]
door = min(doors, key=lambda d: (d.get_actor_location().x) ** 2 + (d.get_actor_location().y + 800) ** 2)
if STEP == "face":
    loc = door.get_actor_location()
    fwd = door.get_actor_forward_vector()     # door local +X points out of the house
    right = door.get_actor_right_vector()     # the leaf runs along +Y from the hinge
    stand = unreal.Vector(loc.x + fwd.x * 220 + right.x * 55, loc.y + fwd.y * 220 + right.y * 55, loc.z + 100)
    pawn.set_actor_location(stand, False, True)
    target = unreal.Vector(loc.x + right.x * 55, loc.y + right.y * 55, loc.z + 150)
    look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(stand.x, stand.y, stand.z + 64), target)
    pc.set_control_rotation(look)
    print("KG_DOOR facing", door.get_name(), "open", door.is_open())
elif STEP == "open":
    unreal.SystemLibrary.execute_console_command(w, "kg.Act interact")
    print("KG_DOOR interact sent")
elif STEP == "shot":
    print("KG_DOOR state open", door.is_open())
    unreal.SystemLibrary.execute_console_command(w, f"HighResShot 1280x720 filename=KG_Door_{'Open' if door.is_open() else 'Closed'}")
