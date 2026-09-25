"""SPRINT-027a: extra villager BODY variants on the shared UBC skeleton (same recipe as kg_build_villagers.py, no
hair baked in: hairstyles are runtime head attachments, so one body serves many archetypes).

Outputs to <out_dir> (FBX, textures embedded, armature object "Armature"):
  SK_KG_Villager_MR / FR   UBC head + Ranger modular parts, no hood/pauldron (hunter, guard, pirate, ranger)
  (the UBC "Superhero" full body is the underwear base mesh: never shipped as a variant)
  SK_KG_Villager_MX / FX   mixed modular parts (ranger body over peasant legs / peasant body over ranger legs)
  SK_KG_Villager_MB / FB   bald peasant (the shipped SK_KG_Villager_M/F keep their baked hair; these take runtime hair)
  KG_VillagerVariants_Preview.png

Usage: blender --background --factory-startup --python Tools/Blender/kg_build_villager_variants.py -- <out_dir>
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

SRC = "D:/Kill Godot/Art/Source"
UBC = f"{SRC}/Quaternius_UniversalBaseCharacters/Universal Base Characters[Standard]/Universal Base Characters[Standard]"
OUTFIT_ROOT = (f"{SRC}/Quaternius_ModularOutfitsFantasy/Modular Character Outfits - Fantasy[Standard]/"
               "Modular Character Outfits - Fantasy[Standard]/Exports/glTF (Godot-Unreal)")
OUTFITS = f"{OUTFIT_ROOT}/Outfits"
PARTS = f"{OUTFIT_ROOT}/Modular Parts"

# name: (base body, [outfit files], keep head only)
VARIANTS = {
    "SK_KG_Villager_MR": ("Superhero_Male_FullBody.gltf",
                          [f"{PARTS}/Male_Ranger_Body.gltf", f"{PARTS}/Male_Ranger_Arms.gltf",
                           f"{PARTS}/Male_Ranger_Legs.gltf", f"{PARTS}/Male_Ranger_Feet_Boots.gltf"], True),
    "SK_KG_Villager_FR": ("Superhero_Female_FullBody.gltf",
                          [f"{PARTS}/Female_Ranger_Body.gltf", f"{PARTS}/Female_Ranger_Arms.gltf",
                           f"{PARTS}/Female_Ranger_Legs.gltf", f"{PARTS}/Female_Ranger_Feet.gltf"], True),
    "SK_KG_Villager_MX": ("Superhero_Male_FullBody.gltf",
                          [f"{PARTS}/Male_Ranger_Body.gltf", f"{PARTS}/Male_Ranger_Arms.gltf",
                           f"{PARTS}/Male_Peasant_Legs.gltf", f"{PARTS}/Male_Peasant_Feet.gltf"], True),
    "SK_KG_Villager_FX": ("Superhero_Female_FullBody.gltf",
                          [f"{PARTS}/Female_Peasant_Body.gltf", f"{PARTS}/Female_Peasant_Arms.gltf",
                           f"{PARTS}/Female_Ranger_Legs.gltf", f"{PARTS}/Female_Ranger_Feet.gltf"], True),
    "SK_KG_Villager_MB": ("Superhero_Male_FullBody.gltf", [f"{OUTFITS}/Male_Peasant.gltf"], True),
    "SK_KG_Villager_FB": ("Superhero_Female_FullBody.gltf", [f"{OUTFITS}/Female_Peasant.gltf"], True),
}


def find(root, name):
    for dirpath, _, files in os.walk(root):
        if name in files and ("Godot" in dirpath or "gltf" in dirpath.lower()):
            return os.path.join(dirpath, name)
    raise FileNotFoundError(name)


def import_gltf(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def keep_bones(obj, keywords):
    names = {g.index: g.name.lower() for g in obj.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    dl = bm.verts.layers.deform.active
    doomed = []
    for v in bm.verts:
        w = v[dl] if dl else {}
        if w:
            gi = max(w.items(), key=lambda kv: kv[1])[0]
            if not any(k in names.get(gi, "") for k in keywords):
                doomed.append(v)
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()


def compose(name, spec):
    body, outfits, head_only = spec
    groups = []
    main_arm = None
    for path, is_body in [(find(UBC, body), True)] + [(p, False) for p in outfits]:
        objs = import_gltf(path)
        arm = next((o for o in objs if o.type == "ARMATURE"), None)
        meshes = [o for o in objs if o.type == "MESH"]
        if is_body and head_only:
            for m in meshes:
                keep_bones(m, ("head", "neck", "eye", "jaw"))
        if main_arm is None:
            main_arm = arm
        groups.append((arm, meshes))
    for arm, meshes in groups:
        for m in meshes:
            mw = m.matrix_world.copy()
            m.parent = main_arm
            m.matrix_world = mw
            for mod in m.modifiers:
                if mod.type == "ARMATURE":
                    mod.object = main_arm
        if arm is not main_arm and arm is not None:
            bpy.data.objects.remove(arm, do_unlink=True)
    for o in list(bpy.data.objects):
        if o.type == "EMPTY" and not o.children:
            bpy.data.objects.remove(o, do_unlink=True)
    meshes = [c for c in main_arm.children if c.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    main_arm.name = "Armature"
    return main_arm, joined


def export_fbx(path, objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
        bake_anim=False, path_mode="COPY", embed_textures=True, mesh_smooth_type="FACE",
        use_armature_deform_only=False, armature_nodetype="NULL", apply_scale_options="FBX_SCALE_NONE",
        primary_bone_axis="Y", secondary_bone_axis="X")
    print(f"KG_EXPORT_OK {path}")


def render_preview(path):
    scene = bpy.context.scene
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.resolution_x, scene.render.resolution_y = 2400, 900
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.62, 0.8, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.location = (0, -9.5, 1.3)
    cam.rotation_euler = (Vector((0, 0, 0.95)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    out_dir = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
    os.makedirs(out_dir, exist_ok=True)
    for name, spec in VARIANTS.items():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        arm, mesh = compose(name, spec)
        export_fbx(os.path.join(out_dir, f"{name}.fbx"), [arm, mesh])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for i, (name, spec) in enumerate(VARIANTS.items()):
        arm, _ = compose(name, spec)
        arm.location.x = (i - (len(VARIANTS) - 1) * 0.5) * 1.1
    render_preview(os.path.join(out_dir, "KG_VillagerVariants_Preview.png"))
    print("KG_VARIANTS_DONE")


main()
