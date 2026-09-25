"""Import the Drillimpact PSX First Person Arms (CC0) glb (skeletal mesh + all animations) and the CC0 knife.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=PythonScript -Script="D:/Kill Godot/Tools/Unreal/kg_import_fp_arms.py"
"""
import glob
import unreal

ARMS = "D:/Kill Godot/Art/Source/Drillimpact_FPArms/psx-first-person-arms-free-game-assets/arms_rig.glb"
DEST = "/Game/KillGodot/Characters/FPArms"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def imp(path, dest, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]


paths = imp(ARMS, DEST)
eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
report = []
for p in eal.list_assets(DEST, recursive=True):
    a = unreal.load_asset(p)
    extra = ""
    if isinstance(a, unreal.AnimSequence):
        extra = f" {a.get_play_length():.2f}s"
    elif isinstance(a, unreal.SkeletalMesh):
        extra = f" extent={a.get_bounds().box_extent}"
    report.append(f"{a.get_class().get_name()}:{p.split('/')[-1].split('.')[0]}{extra}")
unreal.log(f"KG_ARMS: {report}")

for knife in glob.glob("D:/Kill Godot/Art/Source/PolyPizza_Melee/*a2avVUVeYD*.glb"):
    for p in imp(knife, "/Game/KillGodot/Items/Melee", "SM_KG_Hunters_Knife"):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            s = a.get_editor_property("nanite_settings")
            s.set_editor_property("enabled", False)
            a.set_editor_property("nanite_settings", s)
            eal.save_loaded_asset(a)
            unreal.log(f"KG_KNIFE: {p} extent={a.get_bounds().box_extent}")
