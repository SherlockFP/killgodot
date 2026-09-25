"""Listen-server + 1 client PIE network check (M3). Run steps from outside with a small delay between them:

  python Tools/Unreal/kg_remote.py -c "STEP='setup'; exec(open('D:/Kill Godot/Tools/Unreal/kg_net_test.py').read())"
  STEP in: setup | attack | blade | report | door | shot

PIE must be PIE_ListenServer with 2 players (Editor Preferences > Level Editor > Play, or ConfigSettingsToolset).
Host = server world PC0, Client = client world PC0 (its server-side copy is server world PC1).
"""
import unreal

STEP = globals().get("STEP", "report")
worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = next(w for w in worlds if unreal.SystemLibrary.is_server(w))
client = next(w for w in worlds if not unreal.SystemLibrary.is_server(w))


def pawn(world, idx):
    pc = unreal.GameplayStatics.get_player_controller(world, idx)
    return pc, pc.get_controlled_pawn() if pc else None


def same_player(world, pawn_elsewhere):
    """Actor names differ between PIE worlds; the PlayerState name does not."""
    name = pawn_elsewhere.get_editor_property("player_state").get_player_name()
    return next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character)
                if a.get_editor_property("player_state") and a.get_editor_property("player_state").get_player_name() == name)


def act(action):
    # Direct Python calls run under the editor script guard, which forces RPCs local; queue for the next tick.
    unreal.SystemLibrary.execute_console_command(client, f"kg.Act {action}")


host_pc, host = pawn(server, 0)
remote_pc, remote_on_server = pawn(server, 1)
client_pc, client_pawn = pawn(client, 0)


def place(server_pawn, loc, yaw):
    # Authoritative move; the client's own pawn is moved too so its prediction does not fight the correction.
    server_pawn.set_actor_location_and_rotation(loc, unreal.Rotator(0, yaw, 0), False, True)
    if server_pawn == remote_on_server:
        client_pawn.set_actor_location_and_rotation(loc, unreal.Rotator(0, yaw, 0), False, True)


def aim(yaw, pitch=0.0):
    for pc in (client_pc, remote_pc):
        pc.set_control_rotation(unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0))


if STEP == "setup":
    base = host.get_actor_location()
    place(host, base, 0.0)
    place(remote_on_server, unreal.Vector(base.x - 100.0, base.y, base.z), 0.0)
    host_pc.set_control_rotation(unreal.Rotator(0, 0, 0))
    aim(0.0)
    print("KG_NET setup host", host.get_name(), "client", client_pawn.get_name(), "base", base)
elif STEP == "attack":
    act("attack")
    print("KG_NET client attacked")
elif STEP == "blade":
    act("blade")
    print("KG_NET client blade requested")
elif STEP == "door":
    door = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.Actor)
                if a.get_class().get_name() == "KGDoor")
    d = door.get_actor_location()
    f = door.get_actor_forward_vector()
    stand = unreal.Vector(d.x + f.x * 130.0, d.y + f.y * 130.0, host.get_actor_location().z)
    look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(stand.x, stand.y, stand.z + 64.0), unreal.Vector(d.x, d.y, d.z + 100.0))
    place(remote_on_server, stand, look.yaw)
    aim(look.yaw, look.pitch)
    print("KG_NET door before", door.is_open())
elif STEP == "interact":
    act("interact")
    print("KG_NET client interacted")
elif STEP == "shot":
    unreal.SystemLibrary.execute_console_command(client, "HighResShot 1280x720 filename=KG_Net_Client")

# Always report the authoritative state and what the client believes.
h_srv = host.get_health()
h_cli = same_player(client, host)
doors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.Actor) if a.get_class().get_name() == "KGDoor"]
doors_c = [a for a in unreal.GameplayStatics.get_all_actors_of_class(client, unreal.Actor) if a.get_class().get_name() == "KGDoor"]
print(f"KG_NET host health server={h_srv.get_health():.0f} dead={host.is_dead()} | on client={h_cli.get_health().get_health():.0f} dead={h_cli.is_dead()}")
print(f"KG_NET blade server={remote_on_server.get_editor_property('holding_assassin_blade')} "
      f"door server={[d.is_open() for d in doors]} client={[d.is_open() for d in doors_c]}")
