"""SPRINT-027a: silhouette gear worn over the Quaternius villagers (accessories, not characters):
witch hat, straw hat, chef's toque, crown, cape, apron. Authored in villager component space (metres, Z up, feet
at 0, Head bone at z 1.60, head top ~1.84; the glTF export flips Y, so Blender -Y here = the villager's front +Y
in UE, checked against the imported bounds), white vertex colour; UKGAppearanceComponent
attaches them to Head / spine_03 / spine_02 with an identity placement and tints M_KG_RoleBand's "Tint".

  blender --background --factory-startup --python Tools/Blender/kg_make_villager_gear.py -- Art/Packed/KG_VillagerGear.glb
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
HEAD = Vector((0.0, 0.015, 1.68))        # head centre (UBC hair bounds; Y already flipped for UE)


def finish(name, bm, smooth=True):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    for c in col.data:
        c.color = (1.0, 1.0, 1.0, 1.0)
    me.color_attributes.active_color = col
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.new("M_KG_Gear"))
    return obj


def ring(bm, center, r, z, seg, squash=1.0):
    return [bm.verts.new(Vector((center.x + r * math.cos(2 * math.pi * i / seg),
                                 center.y + r * squash * math.sin(2 * math.pi * i / seg), z))) for i in range(seg)]


def skin(bm, rings, close_top=None, close_bottom=None):
    for a, b in zip(rings, rings[1:]):
        for i in range(len(a)):
            bm.faces.new((a[i], a[(i + 1) % len(a)], b[(i + 1) % len(b)], b[i]))
    if close_top:
        bm.faces.new(list(reversed(rings[-1])))
    if close_bottom:
        bm.faces.new(rings[0])


def lathe(name, profile, center=HEAD, seg=24, squash=0.95, close_top=True, close_bottom=True):
    """profile: list of (radius, z) from bottom to top; a rim of near-zero radius closes with a fan."""
    bm = bmesh.new()
    rings = [ring(bm, center, max(r, 0.002), z, seg, squash) for r, z in profile]
    skin(bm, rings, close_top, close_bottom)
    return finish(name, bm)


# --- hats (Head bone) ----------------------------------------------------------------------------------------------
# Witch: brim disc, tall cone with a bent tip.
lathe("SM_KG_Gear_WitchHat",
      [(0.255, 1.775), (0.255, 1.785), (0.135, 1.795), (0.125, 1.82), (0.10, 1.90), (0.07, 1.98), (0.04, 2.06),
       (0.018, 2.12), (0.002, 2.15)])
# Straw hat: a low dome with a wide, gently drooping brim.
lathe("SM_KG_Gear_StrawHat",
      [(0.27, 1.745), (0.27, 1.755), (0.20, 1.775), (0.135, 1.79), (0.13, 1.80), (0.115, 1.85), (0.07, 1.88),
       (0.002, 1.895)])
# Chef's toque: a straight band with a puffed crown.
lathe("SM_KG_Gear_Toque",
      [(0.125, 1.755), (0.122, 1.86), (0.14, 1.90), (0.15, 1.95), (0.135, 2.0), (0.08, 2.04), (0.002, 2.05)])
# Crown: a band with eight points.
bm = bmesh.new()
seg = 32
inner = ring(bm, HEAD, 0.118, 1.76, seg)
outer_lo = ring(bm, HEAD, 0.128, 1.76, seg)
outer_hi = [bm.verts.new(Vector((HEAD.x + 0.128 * math.cos(2 * math.pi * i / seg),
                                 HEAD.y + 0.128 * 0.95 * math.sin(2 * math.pi * i / seg),
                                 1.80 + (0.08 if i % 4 == 0 else 0.0)))) for i in range(seg)]
inner_hi = [bm.verts.new(Vector((HEAD.x + 0.118 * math.cos(2 * math.pi * i / seg),
                                 HEAD.y + 0.118 * 0.95 * math.sin(2 * math.pi * i / seg),
                                 1.80 + (0.08 if i % 4 == 0 else 0.0)))) for i in range(seg)]
skin(bm, [inner, outer_lo, outer_hi, inner_hi, inner])
finish("SM_KG_Gear_Crown", bm, smooth=False)

# --- cape (spine_03): hangs from the shoulders down the back, flaring out; a thin slab --------------------------------
bm = bmesh.new()
rows = []
for t in (0.0, 0.25, 0.5, 0.75, 1.0):
    z = 1.47 - t * 0.92
    half = 0.15 + t * 0.09
    y = 0.10 + t * 0.09 + 0.02 * math.sin(t * math.pi)      # behind the back (UE -Y)
    front = [bm.verts.new(Vector((-half + 2 * half * k / 4, y, z))) for k in range(5)]
    back = [bm.verts.new(Vector((-half + 2 * half * k / 4, y + 0.012, z))) for k in range(5)]
    rows.append((front, back))
for (fa, ba), (fb, bb) in zip(rows, rows[1:]):
    for k in range(4):
        bm.faces.new((fa[k], fa[k + 1], fb[k + 1], fb[k]))
        bm.faces.new((bb[k], bb[k + 1], ba[k + 1], ba[k]))
    bm.faces.new((fa[0], fb[0], bb[0], ba[0]))
    bm.faces.new((ba[4], bb[4], fb[4], fa[4]))
top_f, top_b = rows[0]
bot_f, bot_b = rows[-1]
for k in range(4):
    bm.faces.new((top_b[k], top_b[k + 1], top_f[k + 1], top_f[k]))
    bm.faces.new((bot_f[k], bot_f[k + 1], bot_b[k + 1], bot_b[k]))
finish("SM_KG_Gear_Cape", bm)

# --- apron (spine_02): bib over the chest, skirt to the knees, on the front (+Y) ------------------------------------
bm = bmesh.new()
profile = [(1.36, 0.095, 0.125), (1.22, 0.11, 0.135), (1.08, 0.165, 0.145), (0.90, 0.19, 0.15), (0.66, 0.21, 0.16)]
rows = []
for z, half, y in profile:
    front = [bm.verts.new(Vector((-half + 2 * half * k / 4, -y - 0.012, z))) for k in range(5)]   # UE +Y front
    back = [bm.verts.new(Vector((-half + 2 * half * k / 4, -y, z))) for k in range(5)]
    rows.append((front, back))
for (fa, ba), (fb, bb) in zip(rows, rows[1:]):
    for k in range(4):
        bm.faces.new((fa[k], fb[k], fb[k + 1], fa[k + 1]))
        bm.faces.new((ba[k], ba[k + 1], bb[k + 1], bb[k]))
    bm.faces.new((fa[0], ba[0], bb[0], fb[0]))
    bm.faces.new((fa[4], fb[4], bb[4], ba[4]))
top_f, top_b = rows[0]
bot_f, bot_b = rows[-1]
for k in range(4):
    bm.faces.new((top_f[k], top_f[k + 1], top_b[k + 1], top_b[k]))
    bm.faces.new((bot_f[k], bot_b[k], bot_b[k + 1], bot_f[k + 1]))
finish("SM_KG_Gear_Apron", bm)

bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE")
print("KG_VILLAGER_GEAR", out)
