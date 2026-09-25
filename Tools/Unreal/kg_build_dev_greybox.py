"""Build /Game/KillGodot/Maps/L_Dev_Greybox: a small test yard for M1/M2 (first-person, physics, backstab).

Run headless:
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_build_dev_greybox.py"
"""
import unreal

MAP_PATH = "/Game/KillGodot/Maps/L_Dev_Greybox"
CUBE = "/Engine/BasicShapes/Cube.Cube"
PLANE = "/Engine/BasicShapes/Plane.Plane"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"KG_GREYBOX: {msg}")


def spawn(cls, location=(0, 0, 0), rotation=(0, 0, 0), label=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(*rotation))
    if label:
        actor.set_actor_label(label)
    return actor


def box(label, location, scale, mesh=CUBE, simulate=False):
    actor = spawn(unreal.StaticMeshActor, location, label=label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset(mesh))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if simulate:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_simulate_physics(True)
    return actor


def main():
    if assets.does_asset_exist(MAP_PATH):
        assets.delete_asset(MAP_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        raise RuntimeError("could not create a blank map")

    # Lighting + atmosphere (dynamic, no static lighting; see Docs/05 §8)
    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (-50, -35, 0), "Sun")
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog, label="HeightFog")
    spawn(unreal.VolumetricCloud, label="Clouds")

    # Yard: 60 m x 60 m floor, 4 house-sized blocks (2 floors = 6 m), a doorway gap, crates to push around.
    box("Floor", (0, 0, 0), (60, 60, 1), mesh=PLANE)
    for i, (x, y) in enumerate(((-1500, -1500), (1500, -1500), (-1500, 1500), (1500, 1500))):
        box(f"House_{i}_Shell", (x, y, 300), (8, 6, 6))
    box("Wall_Left", (0, -700, 150), (0.2, 6, 3))
    box("Wall_Right", (0, 700, 150), (0.2, 6, 3))
    for i in range(6):
        box(f"Crate_{i}", (400 + (i % 3) * 110, -200 + (i // 3) * 110, 60), (1, 1, 1), simulate=True)
    box("Ramp", (-600, 0, 60), (4, 2, 0.2))
    spawn(unreal.PlayerStart, (-900, 0, 120), (0, 0, 0), "PlayerStart")

    # M2 test props: a door you can open / lock / break, and a puppet dummy facing away (backstab target).
    door_cls = unreal.load_class(None, "/Script/KillGodot.KGDoor")
    char_cls = unreal.load_class(None, "/Script/KillGodot.KGCharacter")
    if door_cls:
        spawn(door_cls, (-300, 350, 0), (0, 0, 0), "Door_Test")
    if char_cls:
        spawn(char_cls, (-450, -120, 92), (0, 0, 0), "Dummy_Villager")

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        raise RuntimeError("save_map failed")
    log(f"saved {MAP_PATH}")


main()
