"""A stylised seagull for the harbour sky (flocks reuse AKGFishSchool with negative depth).
+X = beak, wings along +-Y. Vertex colour RGB = albedo (linear), ALPHA = normalised distance from the body axis
(0 at the body, 1 at the wing tips) so M_KG_Bird can flap the wings in the vertex shader.

  blender --background --factory-startup --python Tools/Blender/kg_make_gull.py -- Art/Packed/KG_Gull.glb
"""
import sys

import bmesh
import bpy

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bm = bmesh.new()
WHITE, GREY, BLACK, YELLOW = (0.9, 0.9, 0.88), (0.55, 0.58, 0.62), (0.05, 0.05, 0.05), (0.95, 0.75, 0.1)
colours = {}


def tri_fan(pts, col):
    vs = [bm.verts.new(p) for p in pts]
    f = bm.faces.new(vs)
    colours[f] = col
    return f


# Body: an elongated diamond (8 faces).
nose, tail = (0.22, 0, 0.0), (-0.24, 0, 0.01)
top, bottom, left, right = (0.0, 0, 0.07), (0.0, 0, -0.06), (0.0, -0.06, 0), (0.0, 0.06, 0)
for a, b in ((top, right), (right, bottom), (bottom, left), (left, top)):
    tri_fan([nose, a, b], WHITE)
    tri_fan([tail, b, a], WHITE)
tri_fan([(0.22, 0, 0.0), (0.30, 0.012, -0.01), (0.30, -0.012, -0.01)], YELLOW)   # beak
tri_fan([(-0.24, 0, 0.01), (-0.34, 0.07, 0.02), (-0.34, -0.07, 0.02)], WHITE)    # tail fan
# Wings: two segments each side, grey with black tips (gull silhouette with a slight gull-wing bend).
for s in (-1, 1):
    tri_fan([(0.06, 0.04 * s, 0.03), (0.08, 0.36 * s, 0.07), (-0.08, 0.36 * s, 0.07), (-0.06, 0.04 * s, 0.03)], GREY)
    tri_fan([(0.08, 0.36 * s, 0.07), (0.02, 0.62 * s, 0.02), (-0.06, 0.60 * s, 0.02), (-0.08, 0.36 * s, 0.07)], BLACK)

me = bpy.data.meshes.new("SM_KG_Gull")
bm.to_mesh(me)
col = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
face_cols = list(colours.values())
for poly, c in zip(me.polygons, face_cols):
    for li in poly.loop_indices:
        v = me.vertices[me.loops[li].vertex_index].co
        span = min(1.0, abs(v.y) / 0.62)
        col.data[li].color = (c[0], c[1], c[2], span)
me.color_attributes.active_color = col
obj = bpy.data.objects.new("SM_KG_Gull", me)
bpy.context.scene.collection.objects.link(obj)
obj.data.materials.append(bpy.data.materials.new("M_KG_Plain"))
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE")
print("KG_GULL", out)
