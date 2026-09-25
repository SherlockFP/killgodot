"""Create the front-end map /Game/KillGodot/Maps/L_MainMenu and point its GameMode override at AKGMenuGameMode.

Only ever touches L_MainMenu (creates it when missing). Safe to re-run: when the map already exists it just sets
the GameMode override, which needs the rebuilt editor module (the class must be loadable).

Headless (editor may stay open on another map):
  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_make_main_menu_map.py"
Live (inside the running editor, after a rebuild):
  python Tools/Unreal/kg_remote.py -f Tools/Unreal/kg_make_main_menu_map.py

Even without the override the menu works: KGMenu::LeaveToMainMenu opens the map with ?game=/Script/KillGodot.KGMenuGameMode.
"""
import unreal

MAP_PATH = "/Game/KillGodot/Maps/L_MainMenu"
MODE_CLASS = "/Script/KillGodot.KGMenuGameMode"


def log(msg):
    unreal.log(f"KG_MAINMENU_MAP: {msg}")


def find_world_settings(world):
    try:
        settings = world.get_world_settings()
        if settings:
            return settings
    except Exception:  # noqa: BLE001 - older bindings
        pass
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.WorldSettings):
            return actor
    return None


def set_game_mode(world):
    mode = unreal.load_class(None, MODE_CLASS)
    if mode is None:
        log("KGMenuGameMode is not in the loaded editor module yet: rebuild the editor target and re-run this script "
            "to set the override (the menu still loads through ?game= until then)")
        return False
    settings = find_world_settings(world)
    if settings is None:
        log("could not find the WorldSettings actor")
        return False
    settings.set_editor_property("default_game_mode", mode)
    log(f"GameMode override set to {MODE_CLASS}")
    return True


def main():
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(MAP_PATH):
        log("map exists, only updating the GameMode override")
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    else:
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        if world is None:
            raise RuntimeError("could not create a blank map")
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        if start:
            start.set_actor_label("PlayerStart")
        log("created a blank front-end map (the title screen is painted by Slate, no scene needed)")

    set_game_mode(world)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        raise RuntimeError("save_map failed")
    log(f"saved {MAP_PATH}")


main()
