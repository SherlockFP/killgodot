"""Pack every external kit in Art/Source into Art/Packed/Ext/KG_Ext_<Tag>.glb (+ .json manifest) with Blender headless.
  python Tools/Assets/kg_ext_build.py [Tag ...]        (no args = all)   log: Saved/Logs/kg_ext_build.log"""
import json, os, subprocess, sys, time
ROOT = "D:/Kill Godot"
BL = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
S = ROOT + "/Art/Source/"
# tag, source dir, extra args
PACKS = [
 ("KPirate",     S + "Kenney_PirateKit/Models/GLB format", ["--ext", "glb", "--scale", "0.7"]),
 ("KSurvival",   S + "Kenney_SurvivalKit/Models/GLB format", ["--ext", "glb", "--scale", "3"]),
 ("KFurniture",  S + "Kenney_FurnitureKit/Models/GLTF format", ["--ext", "glb,gltf"]),
 ("KGraveyard",  S + "Kenney_GraveyardKit/Models/GLB format", ["--ext", "glb", "--scale", "2"]),
 ("KFood",       S + "Kenney_FoodKit/Models/GLB format", ["--ext", "glb"]),
 ("KTown",       S + "Kenney_FantasyTownKit/Models/GLB format", ["--ext", "glb"]),
 ("KDungeon",    S + "Kenney_ModularDungeonKit/Models/GLB format", ["--ext", "glb", "--scale", "0.7"]),
 ("KMiniForest", S + "Kenney_MiniForest/Models/GLB format", ["--ext", "glb", "--scale", "2.5"]),
 ("KNature",     S + "Kenney_NatureKit/Models/GLTF format", ["--ext", "glb,gltf"]),
 ("KCastle",     S + "Kenney_CastleKit/Models/GLB format", ["--ext", "glb", "--scale", "3"]),
 ("KKForest",    S + "KayKit_Forest/KayKit_Forest_Nature_Pack_1.0_FREE/KayKit_Forest_Nature_Pack_1.0_FREE/Assets/gltf", ["--ext", "gltf"]),
 ("KKDungeon",   S + "KayKit_DungeonPack/KayKit_Dungeon_Pack_1.1_FREE/KayKit_Dungeon_Pack_1.1_FREE/Assets/gltf", ["--ext", "gltf"]),
 ("KKFurniture", S + "KayKit_FurnitureBits/KayKit_Furniture_Bits_1.0_FREE/KayKit_Furniture_Bits_1.0_FREE/Assets/gltf", ["--ext", "gltf", "--scale", "0.7"]),
 ("KKHalloween", S + "KayKit_HalloweenBits/KayKit_HalloweenBits_1.0_FREE/KayKit_HalloweenBits_1.0_FREE/Assets/gltf", ["--ext", "gltf", "--scale", "0.6"]),
 ("KKHex",       S + "KayKit_MedievalHexagon/KayKit_Medieval_Hexagon_Pack_1.0_FREE/KayKit_Medieval_Hexagon_Pack_1.0_FREE/Assets/gltf", ["--ext", "gltf", "--scale", "3"]),
 ("KKRestaurant", S + "KayKit_RestaurantBits/KayKit_Restaurant_Bits_1.0_FREE", ["--ext", "gltf,glb", "--exclude", "unity", "--scale", "0.75"]),
 ("QFarm",       S + "Quaternius_FarmBuildings", ["--ext", "fbx"]),
 ("QDungeon",    S + "Quaternius_ModularDungeon", ["--ext", "fbx", "--scale", "0.8"]),
 ("QNature",     S + "Quaternius_UltimateNature", ["--ext", "fbx"]),
 ("FSVillage",   S + "FertileSoil_ModularVillage", ["--ext", "obj"]),
 ("SPDocks",     S + "SimplePolygon_WoodenDocks", ["--ext", "fbx", "--split", "--scale", "10"]),
 ("VCTrees",     S + "Vertexcat_Trees", ["--ext", "fbx"]),
 ("EmaceHouses", S + "EmaceArt_MedievalHouses/FBX", ["--ext", "fbx", "--exclude", r"(^|/)EA03\.fbx$", "--maxtris", "20000"]),
 ("EmaceHaunted", S + "EmaceArt_HauntedGrounds/glTF", ["--ext", "glb", "--scale", "0.6"]),
 ("Goblin",      S + "ByndWalker_GoblinCamp/byndwalker_goblin_camp_free/Free/GLB", ["--ext", "glb"]),
 ("Cafe",        S + "SynOfPlay_MedievalCafe/medv-cafe/fbx", ["--ext", "fbx"]),
 ("Yume",        S + "YumeForge_JapanVillageFree/Free_JapanVillage/FreePack/GLTF", ["--ext", "gltf"]),
 ("KarLove",     S + "KarLove_Cemetery", ["--ext", "obj"]),
 ("KFFood",      S + "KyleFuji_Food/Low Poly Food Asset Pack (Godot)/Kyle Fuji/Models", ["--ext", "glb"]),
 ("PPManor",     S + "PolyPizza_Manor", ["--ext", "glb"]),
 ("PPHarbour",   S + "PolyPizza_Harbour", ["--ext", "glb"]),
]
want = set(sys.argv[1:])
os.makedirs(ROOT + "/Saved/Logs", exist_ok=True)
os.makedirs(ROOT + "/Art/Packed/Ext", exist_ok=True)
summary = {}
for tag, src, extra in PACKS:
    if want and tag not in want:
        continue
    if not os.path.isdir(src):
        summary[tag] = {"error": "source dir missing", "src": src}; print(tag, "MISSING", src, flush=True); continue
    out = f"{ROOT}/Art/Packed/Ext/KG_Ext_{tag}.glb"
    log = f"{ROOT}/Saved/Logs/kg_ext_pack_{tag}.log"
    t0 = time.time()
    with open(log, "w") as lf:
        r = subprocess.run([BL, "--background", "--factory-startup", "--python", ROOT + "/Tools/Blender/kg_ext_pack.py", "--", src, out, "--tag", tag] + extra,
                           stdout=lf, stderr=subprocess.STDOUT, timeout=3600)
    line = [l for l in open(log, errors="replace") if l.startswith("KG_EXT_PACK")]
    ok = r.returncode == 0 and os.path.exists(out) and line
    summary[tag] = {"ok": bool(ok), "secs": round(time.time() - t0), "line": line[-1].strip() if line else "", "rc": r.returncode}
    print(tag, summary[tag], flush=True)
json.dump(summary, open(ROOT + "/Saved/Logs/kg_ext_build_summary.json", "w"), indent=1)
