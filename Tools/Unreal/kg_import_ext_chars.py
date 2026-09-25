"""Import the EXTERNAL rigged character/animal packs as skeletal meshes (+ their own skeletons and animations) under
/Game/KillGodot/Env/Ext/Characters/<Pack>/. Interchange for glTF/GLB (KayKit, Quaternius modular), the legacy FBX
importer for Quaternius' older FBX packs (rig + all clips in one file). Materials/textures come in as the packs ship
them (the villager-variety agent decides the final look). New folders only.

  UnrealEditor-Cmd.exe "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_import_ext_chars.py [Pack ...]" -unattended -nosplash -nopause -nullrhi
Result summary: Saved/Logs/kg_import_ext_chars_<stamp>.json
"""
import glob
import json
import os
import sys
import time

import unreal

S = "D:/Kill Godot/Art/Source"
DEST = "/Game/KillGodot/Env/Ext/Characters"
PACKS = {
    "KayKit_Adventurers": [f"{S}/KayKit_Adventurers/KayKit_Adventurers_2.0_FREE/KayKit_Adventurers_2.0_FREE/Characters/gltf/*.glb",
                           f"{S}/KayKit_Adventurers/KayKit_Adventurers_2.0_FREE/KayKit_Adventurers_2.0_FREE/Animations/gltf/Rig_Medium/*.glb"],
    "KayKit_Skeletons": [f"{S}/KayKit_Skeletons/KayKit_Skeletons_1.1_FREE/KayKit_Skeletons_1.1_FREE/characters/gltf/*.glb",
                         f"{S}/KayKit_Skeletons/KayKit_Skeletons_1.1_FREE/KayKit_Skeletons_1.1_FREE/Animations/gltf/Rig_Medium/*.glb"],
    "Quaternius_UltimateModularWomen": [f"{S}/Quaternius_UltimateModularWomen/Individual Characters/glTF/*.gltf"],
    "Quaternius_UltimateModularMen": [f"{S}/Quaternius_UltimateModularMen/Individual Characters/glTF/*.gltf"],
    "Quaternius_AnimatedCharacters": [f"{S}/Quaternius_AnimatedCharacters/ultimate_animated_character_pack_by_quaternius/Ultimate Animated Character Pack - Nov 2019/FBX/*.fbx"],
    "Quaternius_RPGCharacters": [f"{S}/Quaternius_RPGCharacters/rpg_characters_-_nov_2020/RPG Characters - Nov 2020/FBX/*.fbx"],
    "Quaternius_AnimatedKnight": [f"{S}/Quaternius_AnimatedKnight/Knight Character by @Quaternius/Knight Character by @Quaternius/FBX/KnightCharacter.fbx"],
    "Animals_Quaternius": [f"{S}/Quaternius_AnimatedAnimals/Animal Pack Vol.2 by @Quaternius/Animal Pack Vol.2 by @Quaternius/FBX/*.fbx",
                           f"{S}/Quaternius_FarmAnimals/Farm Animals Animated  by Quaternius/FBX/*.fbx"],
    "Fish_Quaternius": [f"{S}/Quaternius_AnimatedFish/Fish Pack Animated by Quaternius/FBX/*.fbx"],
    "Monsters_Quaternius": [f"{S}/Quaternius_AnimatedMonsters/Monster Pack Animated by Quaternius/FBX/*.fbx"],
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
want = [a for a in sys.argv[1:] if a and not a.startswith("-")] or list(PACKS)


def fbx_ui():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("automated_import_should_detect_type", False)
    return ui


results = {}
for pack in want:
    files = sorted(f for g in PACKS[pack] for f in glob.glob(g))
    dest = f"{DEST}/{pack}"
    out = {"files": len(files), "skeletal": [], "failed": [], "secs": 0}
    t0 = time.time()
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0].replace(" ", "_")
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", f"{dest}/{name}")
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        if f.lower().endswith(".fbx"):
            t.set_editor_property("options", fbx_ui())
        try:
            tools.import_asset_tasks([t])
            paths = [str(p) for p in t.get_editor_property("imported_object_paths")]
            sk = [p for p in paths if isinstance(unreal.load_asset(p), unreal.SkeletalMesh)]
            if sk:
                out["skeletal"].append({"file": os.path.basename(f), "mesh": sk[0], "assets": len(paths)})
            else:
                out["failed"].append({"file": os.path.basename(f), "assets": paths[:4]})
        except Exception as e:  # noqa
            out["failed"].append({"file": os.path.basename(f), "error": str(e)[:200]})
    out["secs"] = round(time.time() - t0)
    results[pack] = out
    unreal.log(f"KG_EXT_CHARS {pack}: {len(out['skeletal'])} skeletal meshes of {len(files)} files, failed {len(out['failed'])}")
stamp = time.strftime("%Y%m%d_%H%M%S")
json.dump(results, open(f"D:/Kill Godot/Saved/Logs/kg_import_ext_chars_{stamp}.json", "w"), indent=1)
unreal.log("KG_EXT_CHARS done")
