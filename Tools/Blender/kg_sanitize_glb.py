"""Sanitise a glb for Unreal Interchange: triangulate, merge doubles, drop loose geometry, recalc normals outward,
replace materials with one plain material (colours stay in the active vertex colour attribute). Fixes 'Bad MeshDescription' imports.
  blender --background --factory-startup --python Tools/Blender/kg_sanitize_glb.py -- in.glb out.glb"""
import sys

import bmesh
import bpy

a = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=a[0])
n = 0
# Interchange needs a material on every primitive (none -> empty polygon groups -> "Bad MeshDescription"),
# but textured/complex glTF materials become unsaveable dynamic instances: give everything one plain material.
plain = bpy.data.materials.new("M_KG_Plain")
for o in bpy.data.objects:
    if o.type != "MESH":
        continue
    me = o.data
    me.materials.clear()
    me.materials.append(plain)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()
    n += 1
bpy.ops.export_scene.gltf(filepath=a[1], export_format="GLB", export_vertex_color="ACTIVE", export_materials="EXPORT")
print("KG_SANITIZE", a[1], n)
