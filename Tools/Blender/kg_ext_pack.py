"""Pack an EXTERNAL low-poly kit (Kenney / KayKit / Quaternius / itch packs) into ONE sanitised glb + manifest that
Tools/Unreal/kg_import_ext_pack.py imports as static meshes with the house vertex-colour material.

Per model file: import (glTF/GLB/FBX/OBJ), drop rigs/empties, apply transforms, join into one object, BAKE the
material look into a per-corner colour attribute (texture sampled at the face's UV centre, or the material's base
colour, or existing vertex colours), move the pivot to the bottom centre, sanitise (one plain material, merge doubles,
triangulate) and name it SM_KG_<tag>_<file>. Colours are stored LINEAR (what M_KG_PropVCLinear expects).

  blender --background --factory-startup --python Tools/Blender/kg_ext_pack.py -- <src_dir> <out.glb> --tag KPirate
        [--ext glb,gltf,fbx,obj] [--scale 1.0] [--include RX] [--exclude RX] [--maxtris 30000] [--flat] [--split]
--flat: do not recurse into sub folders.  --split: one piece per mesh OBJECT (multi-object files such as a single
FBX holding a whole kit) instead of one piece per file. The manifest (<out>.json) gets material/collision/size/tris.
"""
import json
import math
import os
import re
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src_dir, out_path = argv[0], argv[1]


def opt(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


TAG = opt("--tag", "Ext")
EXTS = tuple("." + e.strip().lower() for e in opt("--ext", "glb,gltf,fbx,obj").split(","))
SCALE = float(opt("--scale", "1.0"))
INCLUDE = re.compile(opt("--include", "."), re.I)
EXCLUDE = re.compile(opt("--exclude", "(?!)"), re.I)
MAXTRIS = int(opt("--maxtris", "30000"))
FLAT = "--flat" in argv
SPLIT = "--split" in argv
COMPLEX_RX = re.compile(r"floor|wall|stair|ramp|pier|dock|bridge|road|path|terrain|hex|tile|ground|platform|roof|"
                        r"ship|boat|cliff|rock_?large|fence|gate|arch|column|pillar|tower|house|building|cabin|tent|"
                        r"hut|ruin|coffin|grave|crypt|foundation|base|deck", re.I)

bpy.ops.wm.read_factory_settings(use_empty=True)
plain = bpy.data.materials.new("M_KG_Plain")
_img_cache = {}


def image_array(img):
    key = img.name
    if key not in _img_cache:
        w, h = img.size
        if w == 0 or h == 0:
            _img_cache[key] = None
        else:
            arr = np.empty(w * h * img.channels, dtype=np.float32)
            img.pixels.foreach_get(arr)
            arr = arr.reshape(h, w, img.channels)
            if img.channels < 3:
                arr = np.repeat(arr[:, :, :1], 3, axis=2)
            # byte images arrive as their stored (sRGB) values: linearise; float images are already linear
            if not img.is_float:
                c = arr[:, :, :3]
                arr[:, :, :3] = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
            _img_cache[key] = arr
    return _img_cache[key]


def base_colour_source(mat):
    """-> ('image', Image, factor) | ('color', (r,g,b), None) | ('vertex', None, factor)"""
    if not mat:
        return ("color", (0.6, 0.6, 0.6), None)
    if not mat.use_nodes or not mat.node_tree:
        c = mat.diffuse_color
        return ("color", (c[0], c[1], c[2]), None)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if not bsdf:
        c = mat.diffuse_color
        return ("color", (c[0], c[1], c[2]), None)
    inp = bsdf.inputs.get("Base Color")
    fac = tuple(inp.default_value[:3])
    if inp.is_linked:
        seen, stack = set(), [inp.links[0].from_node]
        while stack:
            n = stack.pop()
            if n in seen:
                continue
            seen.add(n)
            if n.type == "TEX_IMAGE" and n.image:
                return ("image", n.image, (1.0, 1.0, 1.0))
            if n.type in ("VERTEX_COLOR", "ATTRIBUTE"):
                return ("vertex", None, fac)
            for i in n.inputs:
                for l in i.links:
                    stack.append(l.from_node)
    return ("color", fac, None)


def bake_colours(obj):
    me = obj.data
    existing = me.color_attributes.active_color if me.color_attributes else None
    uv = me.uv_layers.active
    n_loops = len(me.loops)
    out = np.empty((n_loops, 4), dtype=np.float32)
    out[:, 3] = 1.0
    old = None
    if existing:
        old = np.empty(len(existing.data) * 4, dtype=np.float32)
        existing.data.foreach_get("color", old)
        old = old.reshape(-1, 4)
        if existing.domain == "POINT":
            idx = np.empty(n_loops, dtype=np.int32)
            me.loops.foreach_get("vertex_index", idx)
            old = old[idx]
    uvs = None
    if uv:
        uvs = np.empty(n_loops * 2, dtype=np.float32)
        uv.data.foreach_get("uv", uvs)
        uvs = uvs.reshape(-1, 2)
    sources = [base_colour_source(s.material) for s in obj.material_slots] or [("color", (0.6, 0.6, 0.6), None)]
    for poly in me.polygons:
        src = sources[min(poly.material_index, len(sources) - 1)]
        ls = poly.loop_start
        le = ls + poly.loop_total
        if src[0] == "image" and uvs is not None:
            arr = image_array(src[1])
            if arr is None:
                out[ls:le, :3] = (0.6, 0.6, 0.6)
                continue
            h, w = arr.shape[:2]
            c = uvs[ls:le].mean(axis=0)
            px = int(math.floor(c[0] * w)) % w
            py = int(math.floor(c[1] * h)) % h
            out[ls:le, :3] = arr[py, px, :3]
        elif old is not None and src[0] != "image":   # untextured material + existing vertex colours: keep them
            fac = np.array(src[2] if src[0] == "vertex" else (1.0, 1.0, 1.0), dtype=np.float32)
            out[ls:le, :3] = old[ls:le, :3] * fac
        else:
            out[ls:le, :3] = src[1] if src[0] == "color" else (0.6, 0.6, 0.6)
    for a in list(me.color_attributes):
        me.color_attributes.remove(a)
    attr = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    attr.data.foreach_set("color", out.ravel())
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = 0


def import_file(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path, import_shading="FLAT")
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=True)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)


def clean_name(stem):
    s = re.sub(r"[^A-Za-z0-9]+", "_", stem).strip("_")
    return s or "Piece"


def select_only(objs):
    for o in bpy.data.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def remove(objs):
    for o in objs:
        try:
            if o.name in bpy.data.objects:
                bpy.data.objects.remove(o, do_unlink=True)
        except ReferenceError:
            pass


files = []
if FLAT:
    files = [os.path.join(src_dir, f) for f in sorted(os.listdir(src_dir)) if f.lower().endswith(EXTS)]
else:
    for dp, dn, fn in os.walk(src_dir):
        dn.sort()
        for f in sorted(fn):
            if f.lower().endswith(EXTS):
                files.append(os.path.join(dp, f))
files = [f for f in files if INCLUDE.search(os.path.relpath(f, src_dir)) and not EXCLUDE.search(os.path.relpath(f, src_dir))]

manifest, skipped, used = {}, [], set()


def finish_piece(obj, stem_src, rel):
    me = obj.data
    me.materials.clear()
    me.materials.append(plain)
    if SCALE != 1.0:
        for v in me.vertices:
            v.co *= SCALE
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    off = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, min(zs)))
    for v in me.vertices:
        v.co -= off
    obj.location = (0, 0, 0)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()
    tris = len(me.polygons)
    size = [round((max(xs) - min(xs)) * 100), round((max(ys) - min(ys)) * 100), round((max(zs) - min(zs)) * 100)]
    stem = clean_name(stem_src)
    name = f"{TAG}_{stem}"
    i = 2
    while name in used:
        name = f"{TAG}_{stem}_{i}"
        i += 1
    if tris > MAXTRIS:
        skipped.append({"file": rel, "piece": stem, "reason": f"{tris} tris > {MAXTRIS}"})
        bpy.data.objects.remove(obj, do_unlink=True)
        return
    used.add(name)
    obj.name = "SM_KG_" + name
    me.name = obj.name
    if COMPLEX_RX.search(stem) and max(size) >= 150:
        col = "complex"
    elif max(size) < 45:
        col = "none"
    else:
        col = "box"
    manifest[name] = {"material": "VC", "collision": col, "size": size, "tris": tris, "file": rel}


for path in files:
    rel = os.path.relpath(path, src_dir).replace("\\", "/")
    before = set(bpy.data.objects)
    try:
        import_file(path)
    except Exception as e:  # noqa
        skipped.append({"file": rel, "reason": f"import failed: {e}"[:200]})
        continue
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH" and len(o.data.polygons) > 0]
    if not meshes:
        remove(new)
        skipped.append({"file": rel, "reason": "no mesh"})
        continue
    select_only(meshes)
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    for o in meshes:   # instanced mesh data (kit files with linked duplicates) cannot take transform_apply
        if o.data.users > 1:
            o.data = o.data.copy()
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    remove([o for o in new if o not in meshes])
    for o in meshes:
        bake_colours(o)   # per mesh, BEFORE any join, so each part keeps its own material look
    if SPLIT:
        for o in meshes:
            finish_piece(o, re.sub(r"\.\d{3}$", "", o.name), rel)
    else:
        if len(meshes) > 1:
            select_only(meshes)
            bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        remove([o for o in meshes if o != obj])
        finish_piece(obj, os.path.splitext(os.path.basename(path))[0], rel)

for block in (bpy.data.materials, bpy.data.images, bpy.data.meshes, bpy.data.armatures, bpy.data.actions):
    for item in list(block):
        if item.users == 0:
            block.remove(item)
os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=out_path, export_format="GLB", export_vertex_color="ACTIVE", export_materials="EXPORT",
                          export_apply=True, export_animations=False, export_skins=False, export_yup=True)
json.dump({"tag": TAG, "source": src_dir, "props": manifest, "skipped": skipped},
          open(os.path.splitext(out_path)[0] + ".json", "w"), indent=1)
print(f"KG_EXT_PACK {TAG}: {len(manifest)} pieces, {sum(m['tris'] for m in manifest.values())} tris, skipped {len(skipped)} -> {out_path}")
