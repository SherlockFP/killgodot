"""Role bands (SPRINT-027a, acceptance 2): two plain cloth bands, tinted at runtime by UKGAppearanceComponent.

  SM_KG_RoleSash  diagonal sash across the villager torso (public "revealed role" marker: corpse / trial / epilogue).
                  Authored in villager component space (metres, Z up, feet at 0, Head bone at z=1.60), symmetric in
                  Y so the Blender->glTF->UE axis flip does not matter. Attached to spine_02 with an identity
                  placement (same convention as the Quaternius outfit parts, UKGCosmeticsComponent::ComputeBoneRelative).
  SM_KG_RoleCuff  thin ring at the wrist of the first-person arms (owner-only alignment tint). Axis = Z; attached to
                  the FPArms2 hand_r bone rolled 90 deg so the ring axis follows the forearm.

  blender --background --factory-startup --python Tools/Blender/kg_make_role_bands.py -- Art/Packed/KG_RoleBands.glb
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)


def band(name, center, u, v, ra, rb, width, thick, seg=64):
    """Closed rectangular tube following the ellipse center + ra*cos(t)*u + rb*sin(t)*v (u, v unit, orthogonal).
    The band's flat side is width wide along n = u x v; thick is the cloth thickness (radial)."""
    n = u.cross(v).normalized()
    bm = bmesh.new()
    rings = []
    for i in range(seg):
        t = 2.0 * math.pi * i / seg
        p = center + u * (ra * math.cos(t)) + v * (rb * math.sin(t))
        o = (u * (rb * math.cos(t)) + v * (ra * math.sin(t))).normalized()   # ellipse outward normal
        rings.append([bm.verts.new(p + o * (thick * 0.5) + n * (width * 0.5)),
                      bm.verts.new(p + o * (thick * 0.5) - n * (width * 0.5)),
                      bm.verts.new(p - o * (thick * 0.5) - n * (width * 0.5)),
                      bm.verts.new(p - o * (thick * 0.5) + n * (width * 0.5))])
    for i in range(seg):
        a, b = rings[i], rings[(i + 1) % seg]
        for k in range(4):
            bm.faces.new((a[k], b[k], b[(k + 1) % 4], a[(k + 1) % 4]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    for c in col.data:
        c.color = (1.0, 1.0, 1.0, 1.0)
    me.color_attributes.active_color = col
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.new("M_KG_RoleBand"))
    return obj


# Sash: right shoulder (x +0.16, z 1.41) over the chest centre (z 1.18) to the opposite hip (x -0.16, z 0.95).
tilt = math.radians(55.0)
band("SM_KG_RoleSash", Vector((0.0, 0.0, 1.18)), Vector((math.cos(tilt), 0.0, math.sin(tilt))), Vector((0.0, 1.0, 0.0)),
     ra=0.285, rb=0.150, width=0.065, thick=0.012)
# Cuff: a bracelet around the wrist, axis Z.
band("SM_KG_RoleCuff", Vector((0.0, 0.0, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)),
     ra=0.040, rb=0.036, width=0.018, thick=0.006, seg=32)

bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE")
print("KG_ROLE_BANDS", out)
