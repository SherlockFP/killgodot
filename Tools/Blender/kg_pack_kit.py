"""Pack a folder of single-piece glTFs (Quaternius kits) into ONE .glb: one object per piece (joined, at origin,
named after the file), shared materials/images deduplicated. Unreal then imports every piece as its own static
mesh while textures/materials exist once.

  blender --background --factory-startup --python Tools/Blender/kg_pack_kit.py -- <gltf_dir> <out.glb> [prefix]
"""
import os
import re
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
src_dir, out_path = argv[0], argv[1]
prefix = argv[2] if len(argv) > 2 else ""

bpy.ops.wm.read_factory_settings(use_empty=True)
materials = {}
images = {}


def base_name(name):
    return re.sub(r"\.\d{3}$", "", name)


# listdir, not glob: kit folders are named like "MegaKit[Standard]" and brackets are glob syntax.
files = sorted(os.path.join(src_dir, f) for f in os.listdir(src_dir) if f.lower().endswith((".gltf", ".glb")))
for path in files:
    piece = prefix + os.path.splitext(os.path.basename(path))[0]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        continue
    # Bake node transforms, drop empties/armatures, join into a single piece at the origin.
    for o in new:
        o.select_set(False)
    for o in meshes:
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = piece
    obj.data.name = piece
    for o in new:
        try:
            if o != obj and o.name in bpy.data.objects:
                bpy.data.objects.remove(o, do_unlink=True)
        except ReferenceError:   # already consumed by the join (multi-mesh files, e.g. JayBee furniture)
            pass
    # Deduplicate materials and images by base name / file path.
    for slot in obj.material_slots:
        m = slot.material
        if not m:
            continue
        key = base_name(m.name)
        if key in materials and materials[key] != m:
            slot.material = materials[key]
        else:
            materials[key] = m
            m.name = key
            if m.use_nodes:
                for n in m.node_tree.nodes:
                    if n.type == "TEX_IMAGE" and n.image:
                        ikey = os.path.basename(n.image.filepath) or base_name(n.image.name)
                        if ikey in images and images[ikey] != n.image:
                            n.image = images[ikey]
                        else:
                            images[ikey] = n.image

for block in (bpy.data.materials, bpy.data.images, bpy.data.meshes):
    for item in list(block):
        if item.users == 0:
            block.remove(item)

pieces = [o for o in bpy.data.objects if o.type == "MESH"]
bpy.ops.export_scene.gltf(filepath=out_path, export_format="GLB", use_selection=False)
print(f"KG_PACK: {len(pieces)} pieces, {len(bpy.data.materials)} materials, {len(bpy.data.images)} images -> {out_path}")
