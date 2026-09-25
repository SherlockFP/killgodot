"""Export the puppet characters for Unreal (rigid-skinned, vertex-coloured, inverted-hull outline slot).

Outputs (FBX, metres, UE-Mannequin-style bone names, armature object named "Armature" so UE adds no extra root):
  SK_KG_FP_Arms.fbx        first-person right arm + glove + frying pan (bones: root, lowerarm_r, hand_r, weapon_r)
  SK_KG_Puppet_Tilly.fbx   third-person puppet body with hinged jaw (full prototype skeleton)

Usage:
  blender --background --factory-startup --python kg_export_characters.py -- <out_dir>
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import kg_common as kg  # noqa: E402
import kg_puppet_prototype as proto  # noqa: E402


def ue(x, y, z):
    """UE camera/mesh space in cm (X fwd, Y right, Z up) -> Blender metres (FBX import flips Y)."""
    return Vector((x / 100.0, -y / 100.0, z / 100.0))


def armature_from_table(collection, table):
    arm = bpy.data.armatures.new("KG_Rig")
    obj = bpy.data.objects.new("Armature", arm)
    collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for name, head, tail, parent in table:
        eb = arm.edit_bones.new(name)
        eb.head, eb.tail = head, tail
        if parent:
            eb.parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


def cyl_between(b, p1, p2, r1, r2, color, bone, segs=14):
    d = p2 - p1
    q = d.to_track_quat("Z", "Y")
    b.cylinder(r1, r2, d.length, (p1 + p2) / 2, color, bone, rot=[math.degrees(a) for a in q.to_euler()], segs=segs)


def export_selected(path, objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
        bake_anim=False, use_mesh_modifiers=True, mesh_smooth_type="FACE", colors_type="SRGB",
        primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
        apply_scale_options="FBX_SCALE_NONE", use_armature_deform_only=True)
    print(f"KG_EXPORT_OK {path}")


def build_fp_arms(out_dir):
    kg.reset_scene()
    col = bpy.context.scene.collection
    elbow, wrist, palm = ue(0, 30, -38), ue(30, 16, -20), ue(35, 14, -17)
    grip_end, pan = ue(52, 10, -7), ue(62, 7, -2)
    rig = armature_from_table(col, [
        ("root", Vector((0, 0, 0)), ue(10, 0, 0), None),
        ("lowerarm_r", elbow, wrist, "root"),
        ("hand_r", wrist, palm, "lowerarm_r"),
        ("weapon_r", palm, grip_end, "hand_r"),
    ])
    b = kg.PartBuilder("SKM_KG_FP_Arms")
    cyl_between(b, elbow, wrist, 0.056, 0.05, "teal", "lowerarm_r")
    wdir = (palm - wrist).normalized()
    cyl_between(b, wrist - wdir * 0.01, wrist + wdir * 0.035, 0.068, 0.066, "mustard", "hand_r")
    b.sphere(0.07, tuple(palm), "mustard", "hand_r", scale=(1.1, 0.85, 0.8))
    b.sphere(0.028, tuple(palm + ue(2, -4, 4)), "mustard", "hand_r", scale=(1, 1, 1.4))
    cyl_between(b, palm, grip_end, 0.015, 0.017, "dark_wood", "weapon_r", segs=8)
    axis = Vector((0, 1, 0.3)).normalized()  # pan face turned to the player's left, tilted up (Blender space)
    rot = [math.degrees(a) for a in axis.to_track_quat("Z", "Y").to_euler()]
    b.cylinder(0.09, 0.078, 0.026, tuple(pan), "charcoal", "weapon_r", rot=rot, segs=24)
    b.cylinder(0.076, 0.076, 0.008, tuple(pan + axis * 0.012), "stone_dark", "weapon_r", rot=rot, segs=24,
               outline=False)
    mesh = b.build(col, outline=0.0035, armature=rig)
    kg.bake_to_vertex_colors(mesh)
    export_selected(os.path.join(out_dir, "SK_KG_FP_Arms.fbx"), [rig, mesh])


def build_tilly(out_dir):
    kg.reset_scene()
    col = bpy.context.scene.collection
    c = dict(proto.CAST[2])  # Tilly: teal sweater, ginger hair, gap tooth
    c["extras"] = []
    rig = armature_from_table(col, [(n, Vector(h), Vector(t), p) for n, h, t, p in proto.bone_table()])
    b = kg.PartBuilder("SKM_KG_Puppet_Tilly")
    proto.build_body(b, c)
    proto.build_head(b, c)
    proto.build_hair_and_hat(b, c)
    mesh = b.build(col, outline=0.012, armature=rig)
    kg.bake_to_vertex_colors(mesh)
    export_selected(os.path.join(out_dir, "SK_KG_Puppet_Tilly.fbx"), [rig, mesh])


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out_dir, exist_ok=True)
    build_fp_arms(out_dir)
    build_tilly(out_dir)


if __name__ == "__main__":
    main()
