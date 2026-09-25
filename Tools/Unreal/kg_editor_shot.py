"""Editor-viewport screenshots from fixed camera views (no PIE needed).

  python Tools/Unreal/kg_remote.py -c "VIEWS=['aerial','plaza']; exec(open('D:/Kill Godot/Tools/Unreal/kg_editor_shot.py').read())"
Files: Saved/Screenshots/WindowsEditor/KG_View_<name>.png (taken on the next editor frames).
"""
import unreal

CAMS = {
    # name: (location cm, rotation pitch/yaw)
    "aerial": ((-9000, 9000, 6500), (-28, -45)),
    "harbour": ((1500, 11000, 1200), (-10, -105)),
    "plaza": ((0, 1600, 620), (-6, -90)),
    "street": ((250, 4800, 560), (-3, -92)),
    "house": ((-2300, 1300, 560), (-4, -60)),
    "hills": ((0, -2500, 900), (2, -90)),
}
views = globals().get("VIEWS", list(CAMS))
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()
for name in views:
    (x, y, z), (p, yw) = CAMS[name]
    ues.set_level_viewport_camera_info(unreal.Vector(x, y, z), unreal.Rotator(pitch=p, yaw=yw, roll=0))
    unreal.SystemLibrary.execute_console_command(world, f"HighResShot 1600x900 filename=KG_View_{name}")
    print("KG_SHOT", name)
