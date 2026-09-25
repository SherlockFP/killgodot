"""Write the dressing notes for the external kits from the packed manifests (Art/Packed/Ext/KG_Ext_<Tag>.json):
Tools/Unreal/dressing/pack_ext_<Tag>.md (what each mesh is, size, collision, where it fits) + pack_ext_index.md.
Also appends the credit rows (Docs/Credits.md) and approved sources (Tools/Assets/approved_sources.json) once.

  python Tools/Assets/kg_ext_notes.py [--credits]
"""
import glob
import json
import os
import sys

ROOT = "D:/Kill Godot"
PACKED = ROOT + "/Art/Packed/Ext"
OUT = ROOT + "/Tools/Unreal/dressing"
DATE = "2026-09-25"

# tag -> pack facts. fit: v2 districts (square, harbour, streets, church, japan, countryside, coast, wilds, beckside)
# and Storm Manor rooms by number (1 Büyük Salon, 2 Giriş Holü, 3 Yemek Salonu, 4 Mutfak, 5 Kiler, 6 Çamaşırhane,
# 7 Şapel, 8 Kütüphane, 9 Balo Salonu, 10 Şarap Mahzeni, 11 Kıvılcım Odası, 12 Sarnıç, 13 Portre Galerisi,
# 14/16 Misafir odaları, 15 Hamam, 17 Atölye, 18 Çocuk Odası, 19 Çalışma, 20 Efendi Yatak, 21 Bilardo, 22 Tavan Arası,
# 23 Saat Odası, 24 Kule, + mezarlık / avlu / kayıkhane outside).
P = {
 "KPirate":     dict(title="Kenney Pirate Kit", folder="Kenney_PirateKit", author="Kenney", url="https://kenney.nl/assets/pirate-kit", lic="CC0", risk="OK",
                     scale="x0.7", fit="harbour + coast: row boats, ship hulls/parts, cannons, crates/barrels, palisade & fort walls, palm trees, treasure chests, buoys/flags. Manor: kayıkhane (boat house), cellar barrels (10)."),
 "KSurvival":   dict(title="Kenney Survival Kit", folder="Kenney_SurvivalKit", author="Kenney", url="https://kenney.nl/assets/survival-kit", lic="CC0", risk="OK",
                     scale="x3", fit="wilds + countryside: tents, campfire, bedrolls, fishing rod, tools, logs, wooden signs, workbench, canoe, bows/arrows. Beckside woodcutter camp."),
 "KFurniture":  dict(title="Kenney Furniture Kit", folder="Kenney_FurnitureKit", author="Kenney", url="https://kenney.nl/assets/furniture-kit", lic="CC0", risk="OK",
                     scale="x1", fit="Storm Manor interiors: beds, wardrobes, desks, bookcases, sofas, armchairs, tables, chairs, bathtub/sink/toilet (15 Hamam), lamps, plants, TV/modern items to skip. Rooms 14/16/18/19/20 (bedrooms, study), 8 (library), 9 (ballroom chairs)."),
 "KGraveyard":  dict(title="Kenney Graveyard Kit", folder="Kenney_GraveyardKit", author="Kenney", url="https://kenney.nl/assets/graveyard-kit", lic="CC0", risk="OK",
                     scale="x2", fit="church (v2 churchyard) + Manor mezarlık / 7 Şapel / crypt & catacombs under 12 Sarnıç: gravestones, crypts, coffins, fences, iron gates, lanterns, urns, dead trees, pumpkins, skeleton bits, altar, pillars, brick walls."),
 "KFood":       dict(title="Kenney Food Kit", folder="Kenney_FoodKit", author="Kenney", url="https://kenney.nl/assets/food-kit", lic="CC0", risk="OK",
                     scale="x1", fit="square market stalls + harbour fish market + 4 Mutfak / 3 Yemek Salonu / 5 Kiler: fruit, vegetables, bread, cheese, fish, meat, bottles, plates, bowls, pots, barrels, cutting boards. Skip burgers/fries/sushi-modern bits."),
 "KTown":       dict(title="Kenney Fantasy Town Kit 2.0", folder="Kenney_FantasyTownKit", author="Kenney", url="https://kenney.nl/assets/fantasy-town-kit", lic="CC0", risk="OK",
                     scale="x1 (buildings ~3 m tall: background/toy scale, use as sheds & stalls, or scale 1.3-1.5)", fit="streets + square + countryside: modular walls/roofs/windows/doors, chimneys, lanterns, fences, wells, carts, crates, market awnings, trees, planters, cobble roads."),
 "KDungeon":    dict(title="Kenney Modular Dungeon Kit", folder="Kenney_ModularDungeonKit", author="Kenney", url="https://kenney.nl/assets/modular-dungeon-kit", lic="CC0", risk="OK",
                     scale="x0.7", fit="Manor cellar: 10 Şarap Mahzeni vault walls/arches, 12 Sarnıç columns, catacomb corridors, iron gates/doors, stairs, torches, chests, barrels. Also the v2 mine/cave mouth."),
 "KMiniForest": dict(title="Kenney Mini Forest", folder="Kenney_MiniForest", author="Kenney", url="https://kenney.nl/assets/mini-forest", lic="CC0", risk="OK",
                     scale="x2.5", fit="wilds + beckside: trees, tents, archer targets, camp base bits, rocks, logs (small mini-style set, background fill)."),
 "KNature":     dict(title="Kenney Nature Kit", folder="Kenney_NatureKit", author="Kenney", url="https://kenney.nl/assets/nature-kit", lic="CC0", risk="OK",
                     scale="x1", fit="countryside + wilds + coast + japan: 329 pieces of trees (many variants), bushes, rocks, cliffs, ground tiles, paths, fences, flowers, mushrooms, logs, stumps, campfire, tents, bridges, cactus/palms (skip)."),
 "KCastle":     dict(title="Kenney Castle Kit", folder="Kenney_CastleKit", author="Kenney", url="https://kenney.nl/assets/castle-kit", lic="CC0", risk="OK",
                     scale="x3", fit="church + wilds ruins + Manor 24 Fırtına Kulesi / outer walls: towers, wall segments, gates, doors, stairs, flags, siege bits, trees, wooden bridges."),
 "KKForest":    dict(title="KayKit Forest Nature Pack (free)", folder="KayKit_Forest", author="Kay Lousberg", url="https://kaylousberg.itch.io/kaykit-forest", lic="CC0", risk="OK",
                     scale="x1", fit="wilds + countryside + beckside + japan: 105 trees/bushes/rocks/grass/flowers/mushrooms/logs/stumps in 3 colour sets (Color1 = summer green, Color2 = autumn, Color3 = pale)."),
 "KKDungeon":   dict(title="KayKit Dungeon Pack 1.1 (free)", folder="KayKit_DungeonPack", author="Kay Lousberg", url="https://kaylousberg.itch.io/kaykit-dungeon-pack", lic="CC0", risk="OK",
                     scale="x1 (chunky: walls 2 m grid)", fit="Manor cellar + catacombs (10, 12, crypt): modular walls, floors, stairs, doors, gates, pillars, torches, braziers, chests, barrels, tables, chairs, bookshelves, banners, cobwebs, coffins, traps, keys, potions."),
 "KKFurniture": dict(title="KayKit Furniture Bits (free)", folder="KayKit_FurnitureBits", author="Kay Lousberg", url="https://kaylousberg.itch.io/furniture-bits", lic="CC0", risk="OK",
                     scale="x0.7", fit="Manor bedrooms 14/16/20, study 19, nursery 18, library 8: beds, armchairs, sofas, wardrobes, desks, dressers, bookcases, mirrors, lamps, rugs, plants, TV (skip)."),
 "KKHalloween": dict(title="KayKit Halloween Bits (free)", folder="KayKit_HalloweenBits", author="Kay Lousberg", url="https://kaylousberg.itch.io/halloween-bits", lic="CC0", risk="OK",
                     scale="x0.6", fit="Manor mezarlık + 7 Şapel + attic 22 + catacombs: gravestones, coffins, cauldrons, candles, hanging/standing/post lanterns, pumpkins, spider webs, dead trees, fences, gates, crates, skulls/bones, scarecrow, witch hat/broom, ghosts (skip)."),
 "KKHex":       dict(title="KayKit Medieval Hexagon Pack (free)", folder="KayKit_MedievalHexagon", author="Kay Lousberg", url="https://kaylousberg.itch.io/kaykit-medieval-hexagon", lic="CC0", risk="OK",
                     scale="x3 (board-game hex tiles scaled so the little buildings are ~3 m)", fit="countryside + coast background: mini buildings (mill, tavern, barracks, church, market, houses, castle, watchtower), boats, trees, hills, mountains, rivers, roads, walls, bridges. Use the buildings/units as distant filler or toy models (18 Çocuk Odası)."),
 "KKRestaurant": dict(title="KayKit Restaurant Bits (free)", folder="KayKit_RestaurantBits", author="Kay Lousberg", url="https://kaylousberg.itch.io/restaurant-bits", lic="CC0", risk="OK",
                     scale="x0.75", fit="4 Mutfak + 3 Yemek Salonu + village tavern/inn + market: stoves, counters, fridges (skip modern), tables, chairs, stools, plates, cups, bottles, food (bread, pizza, soup, cake), crates, shelves, cash register (skip)."),
 "QFarm":       dict(title="Quaternius LowPoly Farm Buildings", folder="Quaternius_FarmBuildings", author="Quaternius", url="https://quaternius.itch.io/lowpoly-farm-buildings", lic="CC0", risk="OK",
                     scale="x1 (buildings 5-11 m)", fit="countryside + beckside farm: barns, silo house, windmill, stable, chicken coop, sheds, fences, wells."),
 "QDungeon":    dict(title="Quaternius LowPoly Modular Dungeon", folder="Quaternius_ModularDungeon", author="Quaternius", url="https://quaternius.itch.io/lowpoly-modular-dungeon-pack", lic="CC0", risk="OK",
                     scale="x0.8", fit="Manor cellar 10/11/12 + catacombs + v2 mine: stone walls, arches, doors, pillars, statues, banners, cobwebs, chairs, tables, barrels, torches, stairs, floors."),
 "QNature":     dict(title="Quaternius 150+ LowPoly Nature Models (Ultimate Nature Pack)", folder="Quaternius_UltimateNature", author="Quaternius", url="https://quaternius.itch.io/150-lowpoly-nature-models", lic="CC0", risk="OK",
                     scale="x1", fit="wilds + countryside + coast + beckside: birch/pine/palm/dead/autumn/snow trees, bushes, rocks, logs, plants, flowers, mushrooms, cactus (skip), grass clumps."),
 "FSVillage":   dict(title="Fertile Soil Productions Modular Village Pack", folder="FertileSoil_ModularVillage", author="Fertile Soil Productions", url="https://fertile-soil-productions.itch.io/modular-village-pack", lic="CC0", risk="OK",
                     scale="x1 (real scale, flat MTL colours)", fit="streets + square: 155 modular house pieces (walls, roofs, doors, windows, stairs, balconies), props (barrels, carts, crates, market bits, fences, lanterns, wells). 2nd facade style beside KG_Village."),
 "SPDocks":     dict(title="SimplePolygon Wooden Docks", folder="SimplePolygon_WoodenDocks", author="SimplePolygon", url="https://simplepolygon.itch.io/wooden-docks", lic="Custom: free personal+commercial, edits OK, CREDIT REQUIRED, no resale", risk="CC-BY (credit)",
                     scale="x10 (FBX shipped at 1/10)", fit="harbour + coast + Manor kayıkhane: modular pier planks, posts, ramps, stairs, railings, ropes, crates, lanterns, boat bumpers."),
 "VCTrees":     dict(title="vertexcat Vertex color trees set", folder="Vertexcat_Trees", author="vertexcat", url="https://vertexcat.itch.io/vertex-color-trees-set", lic="CC0", risk="OK",
                     scale="x1", fit="countryside + wilds background: 6 stylised trees (bell, diamond, pine, pink/blossom for japan, round, tall) 3.5-4.5 m, 116-600 tris."),
 "EmaceHouses": dict(title="EmacEArt Free Fantasy Medieval Houses & Props (Slavic world free)", folder="EmaceArt_MedievalHouses", author="EmacEArt", url="https://emaceart.itch.io/free-fantasy-medieval-houses-and-props-pack", lic="CC0 badge + EmacEArt note: no standalone redistribution/resale", risk="OK (no resale)",
                     scale="x1 (check: some hero buildings 10-17 m)", fit="streets + countryside + coast: 212 pieces incl. whole houses/inns/towers, roofs, walls, fences, wells, carts, barrels, crates, rocks, mud/road tiles, trees (EA03_Env_* environment, EA03_Prop_* props, EA03_Building_*). Textured PBR baked to face colours; heaviest pieces skipped >20k tris."),
 "EmaceHaunted": dict(title="EmacEArt FREE Haunted Grounds (Necropoly lite)", folder="EmaceArt_HauntedGrounds", author="EmacEArt", url="https://emaceart.itch.io/freenecropoly", lic="EmacEArt Asset License: free personal+commercial, no credit, no resale", risk="OK (no resale)",
                     scale="x0.6", fit="Manor mezarlık + 7 Şapel + crypt + wilds: gravestones, tombs, coffins, candles, lanterns, signs, planks, cobwebs, pumpkins, dead/crow trees, fences, gates, statues, urns."),
 "Goblin":      dict(title="ByndWalker Lowpoly Goblin Camp (free)", folder="ByndWalker_GoblinCamp", author="ByndWalker", url="https://byndwalker.itch.io/lowpoly-goblin-camp", lic="CC0", risk="OK",
                     scale="x1", fit="wilds + beckside woodcutter/bandit camp: 2 cabins, stone house, 5 tents, branch fences, branch cage, rock scatter."),
 "Cafe":        dict(title="Syntaxes of Play Medieval Cafe Asset Pack (free)", folder="SynOfPlay_MedievalCafe", author="Syntaxes of Play", url="https://synofplay.itch.io/medieval-caf-asset-pack-free", lic="CC0 1.0", risk="OK",
                     scale="x1", fit="village tavern/inn + square market + 3 Yemek Salonu / 4 Mutfak: wooden chairs/stools/tables, shelves, crates, tapestries, menu boards, baskets of pastries/jars, bread, cheese, pies, mugs, bottles, candles, balance scale."),
 "Yume":        dict(title="YumeForge Japanese Rural Village (free tier)", folder="YumeForge_JapanVillageFree", author="YumeForge", url="https://yumeforge.itch.io/japanese-rural-village", lic="Custom: personal+commercial, no resale/redistribution of raw assets", risk="OK (no resale)",
                     scale="x1", fit="japan garden + island: torii gate, stone lantern, preset house, 3 road segments, fences, props, food items (35 pieces)."),
 "KarLove":     dict(title="KarLove Games Cemetery Environment (free tier, no materials)", folder="KarLove_Cemetery", author="KarLove Games", url="https://karlove-games.itch.io/cemetery-environment-asset-pack", lic="Custom: personal+commercial, no redistribution", risk="OK (no resale)",
                     scale="x1", fit="church yard + Manor mezarlık: 20 graves/crosses/tombs/fences. Free tier has NO materials: everything is flat grey, needs a palette recolour before use."),
 "KFFood":      dict(title="Kyle Fuji Low Poly Food Asset Pack", folder="KyleFuji_Food", author="Kyle Fuji", url="https://kyle-fuji.itch.io/low-poly-food-asset-pack", lic="CC0 1.0", risk="OK",
                     scale="x1", fit="square market + harbour fish stalls + 4 Mutfak: fruit, vegetables, baked goods, sushi/junk food (skip modern), 47 small props."),
 "PPManor":     dict(title="Poly Pizza manor gap fills (chandeliers, bathtubs, fireplace, armchair, couch, grandfather clock, coffin, bookcase, globe, telescope, armour, rug, cauldron, statues, lute, mannequin, painting, ceiling lamp)", folder="PolyPizza_Manor", author="CreativeTrio, Quaternius, Kenney, Kay Lousberg, reyshapes (CC0); Nick Slough (Painting), Zsky (Statue, AngelStatue, Ceiling Lamp) (CC-BY)", url="https://poly.pizza (ids in file names)", lic="CC0 except Painting_rsZqX75a8x, Statue_gieXYyUTYr, AngelStatue_6v4CL0nKfT, Ceiling_Lamp_OTJPOdps4p = CC BY", risk="OK / CC-BY (credit) for the 4 named files",
                     scale="x1 (check each: Poly Pizza scales vary)", fit="1 Büyük Salon chandelier (static stand-in; the gameplay chandelier stays custom), 15 Hamam bathtubs, 8 Kütüphane bookcase/globe/telescope, 19 Çalışma, 13 Galeri painting/statues/busts, 7 Şapel coffin, 4 Mutfak cauldron, 17 Atölye mannequin, 9 Balo lute."),
 "PPHarbour":   dict(title="Poly Pizza harbour gap fills (anchor, boat, bucket of fish, cart, gallows, lanterns, fishing rod, fishing net, crane)", folder="PolyPizza_Harbour", author="Quaternius, Kay Lousberg, iPoly3D (CC0); J-Toastie (Fishing Net, Crane) (CC-BY)", url="https://poly.pizza (ids in file names)", lic="CC0 except Fishing_Net_Z11I3AUBO9 and Crane_gCcpjaxFdv = CC BY", risk="OK / CC-BY (credit) for the 2 named files",
                     scale="x1 (check each)", fit="harbour: crane, nets, anchor, fish bucket, rowing boat, cart; square gallows (2 variants); hanging/post/hand lanterns for streets, japan and the Manor entrance 2."),
}


def size_str(s):
    return f"{s[0]}x{s[1]}x{s[2]}"


def write_pack(tag, info, man):
    props, skipped = man["props"], man.get("skipped", [])
    lines = [f"# pack_ext_{tag}: {info['title']}", "",
             f"- Source: `Art/Source/{info['folder']}` — {info['author']} — {info['url']}",
             f"- License: {info['lic']}  |  Licence risk: **{info['risk']}**",
             f"- Imported: `/Game/KillGodot/Env/Ext/KG_Ext_{tag}/StaticMeshes/SM_KG_{tag}_<piece>` (material M_KG_PropVCLinear, baked face colours, Nanite off)",
             f"- Scale applied at pack time: {info['scale']}. Pivot: bottom centre (minZ = 0, X/Y centred).",
             f"- Fits: {info['fit']}", "",
             f"{len(props)} meshes, {sum(p['tris'] for p in props.values())} tris total. Collision: complex = walkable/large (walls, floors, piers), box = one box, none = small clutter.", "",
             "| mesh | size X x Y x Z cm | tris | collision | source file |", "|---|---|---|---|---|"]
    for name, p in props.items():
        lines.append(f"| SM_KG_{name} | {size_str(p['size'])} | {p['tris']} | {p['collision']} | {p['file']} |")
    if skipped:
        lines += ["", "Skipped at pack time:"] + [f"- {s.get('file')} : {s.get('reason')}" for s in skipped]
    open(f"{OUT}/pack_ext_{tag}.md", "w", encoding="utf-8").write("\n".join(lines) + "\n")
    return len(props), sum(p["tris"] for p in props.values())


index = ["# External kits (pack_ext_*)", "",
         f"Downloaded {DATE} (user approval in chat), packed with `Tools/Blender/kg_ext_pack.py` via `Tools/Assets/kg_ext_build.py`,",
         "imported with `Tools/Unreal/kg_import_ext_pack.py` (static) and `kg_import_ext_chars.py` (skeletal). Nothing is placed in a map;",
         "the v2 finisher and the Storm Manor builder pick from these notes. Contact sheet: `Docs/Research/Ext_Assets_Sheet.png`.", "",
         "| tag | pack | meshes | tris | licence risk | fits |", "|---|---|---|---|---|---|"]
for tag, info in P.items():
    mp = f"{PACKED}/KG_Ext_{tag}.json"
    if not os.path.exists(mp):
        index.append(f"| {tag} | {info['title']} | - | - | {info['risk']} | NOT PACKED |")
        continue
    n, tris = write_pack(tag, info, json.load(open(mp)))
    index.append(f"| {tag} | {info['title']} | {n} | {tris} | {info['risk']} | {info['fit'][:110]} |")
index += ["", "Characters (skeletal): see `pack_ext_characters.md`."]
open(f"{OUT}/pack_ext_index.md", "w", encoding="utf-8").write("\n".join(index) + "\n")
print("notes written for", len(P), "packs")

if "--credits" in sys.argv:
    # one credit row per source folder (the two Poly Pizza folders list their authors), sizes from disk
    rows, seen = [], set()
    for tag, info in P.items():
        f = info["folder"]
        if f in seen:
            continue
        seen.add(f)
        d = f"{ROOT}/Art/Source/{f}"
        mb = sum(os.path.getsize(os.path.join(dp, x)) for dp, dn, fn in os.walk(d) for x in fn) / 1e6 if os.path.isdir(d) else 0
        rows.append(f"| `Art/Source/{f}` | {info['author']} | \"{info['title']}\" [{info['lic']}] ({info['url']}) | {info['lic'].split(':')[0]} | {info['risk']} | {mb:.1f} MB |")
    print("\n".join(rows))
