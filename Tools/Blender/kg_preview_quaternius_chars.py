"""Preview: Quaternius Universal Base Characters + Modular Outfits (Fantasy) as a character-direction candidate.

  blender --background --factory-startup --python kg_preview_quaternius_chars.py -- <out_png>
"""
import math
import os
import sys

import bpy
from mathutils import Vector

SRC = "D:/Kill Godot/Art/Source"
UBC = f"{SRC}/Quaternius_UniversalBaseCharacters/Universal Base Characters[Standard]/Universal Base Characters[Standard]"
OUT = f"{SRC}/Quaternius_ModularOutfitsFantasy/Modular Character Outfits - Fantasy[Standard]/Modular Character Outfits - Fantasy[Standard]/Exports/glTF (Godot-Unreal)/Outfits"


def find(root, name):
    for dirpath, _, files in os.walk(root):
        if name in files and "gltf" in dirpath.lower() or name in files and "Godot" in dirpath:
            return os.path.join(dirpath, name)
    return None


def import_gltf(path, offset):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location += Vector(offset)
    return new


def keep_head_only(objs):
    """Outfits replace the body; keep only vertices mostly weighted to head/neck bones."""
    import bmesh
    for o in objs:
        if o.type != "MESH" or not o.vertex_groups:
            continue
        names = {g.index: g.name.lower() for g in o.vertex_groups}
        bm = bmesh.new()
        bm.from_mesh(o.data)
        dl = bm.verts.layers.deform.active
        doomed = []
        for v in bm.verts:
            w = v[dl] if dl else {}
            if not w:
                continue
            gi = max(w.items(), key=lambda kv: kv[1])[0]
            if not any(k in names.get(gi, "") for k in ("head", "neck", "eye", "jaw")):
                doomed.append(v)
        bmesh.ops.delete(bm, geom=doomed, context="VERTS")
        bm.to_mesh(o.data)
        bm.free()


CHARACTERS = [
    ("Superhero_Male_FullBody.gltf", "Male_Peasant.gltf", "Hair_SimpleParted.gltf", "Eyebrows_Regular.gltf"),
    ("Superhero_Female_FullBody.gltf", "Female_Peasant.gltf", "Hair_Long.gltf", "Eyebrows_Female.gltf"),
    ("Superhero_Male_FullBody.gltf", "Male_Ranger.gltf", "Hair_Beard.gltf", "Eyebrows_Regular.gltf"),
    ("Superhero_Female_FullBody.gltf", "Female_Ranger.gltf", "Hair_Buns.gltf", "Eyebrows_Female.gltf"),
]


def main():
    out_png = sys.argv[sys.argv.index("--") + 1]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    for i, (body, outfit, hair, brows) in enumerate(CHARACTERS):
        off = ((i - 1.5) * 1.1, 0, 0)
        for name in (body, hair, brows):
            p = find(UBC, name)
            if p:
                objs = import_gltf(p, off)
                if name == body:
                    keep_head_only(objs)
        p = os.path.join(OUT, outfit)
        if os.path.exists(p):
            import_gltf(p, off)

    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.view_settings.view_transform = "AgX"
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.62, 0.8, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.9
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    ground = bpy.data.meshes.new("G")
    bpy.ops.mesh.primitive_plane_add(size=30)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 50
    cam.location = (0, -6.2, 1.2)
    cam.rotation_euler = (Vector((0, 0, 0.95)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    print(f"KG_PREVIEW_OK {out_png}")


main()
