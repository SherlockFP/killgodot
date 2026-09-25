"""Export the zisongbr LowPoly Pirate Props (.blend, CC0) to one glb: piers, planks, barrels, chests, crates, flags,
torches, cannon. The pack is modelled ~3x too large; everything is scaled by SCALE and each object is re-centred
on its base (pivot at the bottom centre) so Unreal can place them on the ground/deck.

  blender --background Art/Source/zisongbr_PirateProps/lowpoly_caves_and_props/LowPoly/Lowpoly_Props.blend \
          --python Tools/Blender/kg_export_pirate_props.py -- Art/Packed/KG_PirateProps.glb
"""
import sys

import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
SCALE = 0.35
KEEP = ("pier_", "Planks_", "Barrel_", "chest_", "crates_", "Flag", "Torch_", "cannon", "Cannon_Ball", "Broken_Flag")

# Remove everything that is not a prop mesh we want.
for o in list(bpy.data.objects):
    if o.type != "MESH" or not o.name.startswith(KEEP):
        bpy.data.objects.remove(o, do_unlink=True)

objs = list(bpy.data.objects)   # snapshot: renaming re-sorts bpy.data.objects
for o in objs:
    o.name = "SM_KG_Pirate_" + o.name.replace(".", "_")
for o in objs:
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    o.select_set(False)

for o in objs:
    # Apply the transform, scale down, move the pivot to the bottom centre, place at the origin.
    for other in bpy.data.objects:
        other.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = o.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    offset = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, min(zs)))
    for v in me.vertices:
        v.co = (v.co - offset) * SCALE
    o.location = (0, 0, 0)
    me.update()
    bpy.context.view_layer.update()
    d = o.dimensions
    print(f"KG_PIRATE {o.name}: {d.x:.2f} x {d.y:.2f} x {d.z:.2f} m")

# Bake the palette texture into per-corner vertex colours (the pack is flat palette-mapped low poly), then drop
# the materials: Unreal's glTF import turned the PSD palette materials into unsaveable dynamic instances.
def image_of(mat):
    if mat and mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == "TEX_IMAGE" and n.image:
                return n.image
    return None


cache = {}
for o in objs:
    me = o.data
    uv = me.uv_layers.active
    col = me.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for poly in me.polygons:
        mat = o.material_slots[poly.material_index].material if o.material_slots else None
        img = image_of(mat)
        for li in poly.loop_indices:
            c = (0.8, 0.8, 0.8, 1.0)
            if img and uv:
                if img.name not in cache:
                    cache[img.name] = (img.size[0], img.size[1], list(img.pixels[:]))
                w, h, px = cache[img.name]
                u, v = uv.data[li].uv
                x = min(w - 1, max(0, int((u % 1.0) * w)))
                y = min(h - 1, max(0, int((v % 1.0) * h)))
                i = (y * w + x) * 4
                c = (px[i], px[i + 1], px[i + 2], 1.0)
            elif mat:
                c = tuple(mat.diffuse_color)
            col.data[li].color = c
    me.color_attributes.active_color = col
    me.materials.clear()

bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=False, export_vertex_color="ACTIVE",
                          export_materials="NONE")
print("KG_PIRATE exported", out)
