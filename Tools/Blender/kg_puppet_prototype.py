"""Kill Godot - painted wooden puppet prototype (art-direction preview).

Builds 5 rigged puppet villagers (rigid parts + hinged jaw, UE-Mannequin-style bone names), a small village
backdrop, and renders 3 previews in the Among-Us-3D toon style:
  1) lineup, 2) talking close-up, 3) CS2-style first-person viewmodel mock.

Usage:
  blender --background --factory-startup --python kg_puppet_prototype.py -- <out_dir>
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import kg_common as kg  # noqa: E402

# ----------------------------------------------------------------------------------------------------------------
# Skeleton (character faces -Y, Z up, metres). _l = character left = +X.
# ----------------------------------------------------------------------------------------------------------------
def bone_table():
    bones = [
        ("root", (0, 0, 0), (0, 0, 0.12), None),
        ("pelvis", (0, 0, 0.74), (0, 0, 0.88), "root"),
        ("spine_01", (0, 0, 0.88), (0, 0, 1.05), "pelvis"),
        ("spine_02", (0, 0, 1.05), (0, 0, 1.22), "spine_01"),
        ("neck_01", (0, 0, 1.22), (0, 0, 1.32 + HEAD_UP), "spine_02"),
        ("head", (0, 0, 1.32 + HEAD_UP), (0, 0, 1.64 + HEAD_UP), "neck_01"),
        ("jaw", (0, 0.08, 1.335 + HEAD_UP), (0, -0.13, 1.30 + HEAD_UP), "head"),
    ]
    for s, sx in (("l", 1), ("r", -1)):
        bones += [
            (f"clavicle_{s}", (0.05 * sx, 0, 1.19), (0.2 * sx, 0, 1.19), "spine_02"),
            (f"upperarm_{s}", (0.225 * sx, 0, 1.19), (0.24 * sx, 0, 0.94), f"clavicle_{s}"),
            (f"lowerarm_{s}", (0.24 * sx, 0, 0.94), (0.25 * sx, 0, 0.73), f"upperarm_{s}"),
            (f"hand_{s}", (0.25 * sx, 0, 0.73), (0.25 * sx, 0, 0.60), f"lowerarm_{s}"),
            (f"thigh_{s}", (0.1 * sx, 0, 0.78), (0.1 * sx, 0, 0.43), "pelvis"),
            (f"calf_{s}", (0.1 * sx, 0, 0.43), (0.1 * sx, 0, 0.08), f"thigh_{s}"),
            (f"foot_{s}", (0.1 * sx, 0, 0.08), (0.1 * sx, -0.13, 0.03), f"calf_{s}"),
        ]
    return bones


def make_armature(collection, name):
    arm = bpy.data.armatures.new(f"{name}_Rig")
    obj = bpy.data.objects.new(f"SK_KG_{name}", arm)
    collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, head, tail, parent in bone_table():
        eb = arm.edit_bones.new(bname)
        eb.head, eb.tail = head, tail
        if parent:
            eb.parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.show_in_front = False
    arm.display_type = "STICK"
    return obj


def rot_bone(arm_obj, name, axis, deg):
    """Rotate a pose bone around its own head, around an armature-space axis."""
    bpy.context.view_layer.update()
    pb = arm_obj.pose.bones[name]
    head = pb.head.copy()
    r = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, Vector(axis)) @ Matrix.Translation(-head)
    pb.matrix = r @ pb.matrix
    bpy.context.view_layer.update()


# ----------------------------------------------------------------------------------------------------------------
# Character parts
# ----------------------------------------------------------------------------------------------------------------
JOINT = "light_wood"
HEAD_UP = 0.07  # neck length; keeps the hinged jaw clear of the shoulders


def build_body(b: kg.PartBuilder, c):
    # legs
    for s, sx in (("l", 1), ("r", -1)):
        b.box((0.15, 0.25, 0.1), (0.1 * sx, -0.04, 0.05), c["boots"], f"foot_{s}", bevel=0.04, smooth=True)
        b.cylinder(0.06, 0.066, 0.30, (0.1 * sx, 0, 0.25), c["socks"], f"calf_{s}")
        b.sphere(0.07, (0.1 * sx, 0, 0.43), JOINT, f"calf_{s}")
        b.cylinder(0.078, 0.084, 0.30, (0.1 * sx, 0, 0.61), c["pants"], f"thigh_{s}")
    # hips, belt, torso
    b.cylinder(0.178, 0.172, 0.17, (0, 0, 0.79), c["pants"], "pelvis")
    b.cylinder(0.186, 0.186, 0.055, (0, 0, 0.885), "dark_wood", "spine_01")
    b.box((0.075, 0.025, 0.055), (0, -0.188, 0.885), "gold", "spine_01", bevel=0.008)
    b.cylinder(0.18, 0.208, 0.34, (0, 0, 1.06), c["top"], "spine_01")
    b.sphere(0.208, (0, 0, 1.225), c["top"], "spine_02", scale=(1, 0.92, 0.42))
    if c.get("vest"):
        b.box((0.2, 0.03, 0.3), (0, -0.19, 1.07), c["vest"], "spine_01", bevel=0.012)
    for z in (1.15, 1.07, 0.99):
        b.sphere(0.017, (0, -0.205 if c.get("vest") else -0.2, z), c.get("buttons", "cream"), "spine_01")
    b.cylinder(0.066, 0.066, 0.12 + HEAD_UP, (0, 0, 1.28 + HEAD_UP / 2), JOINT, "neck_01")
    # arms
    for s, sx in (("l", 1), ("r", -1)):
        b.sphere(0.08, (0.225 * sx, 0, 1.19), c["top"], f"upperarm_{s}")
        b.cylinder(0.066, 0.062, 0.22, (0.232 * sx, 0, 1.065), c.get("sleeve", c["top"]), f"upperarm_{s}")
        b.sphere(0.06, (0.24 * sx, 0, 0.94), JOINT, f"lowerarm_{s}")
        b.cylinder(0.058, 0.053, 0.19, (0.245 * sx, 0, 0.835), c.get("sleeve", c["top"]), f"lowerarm_{s}")
        b.cylinder(0.074, 0.07, 0.055, (0.25 * sx, 0, 0.735), c["gloves"], f"hand_{s}")
        b.sphere(0.085, (0.25 * sx, -0.005, 0.66), c["gloves"], f"hand_{s}", scale=(0.85, 0.72, 1.05))
        b.sphere(0.036, (0.25 * sx - 0.012 * sx, -0.068, 0.69), c["gloves"], f"hand_{s}", scale=(1, 1, 1.4))


def build_head(b: kg.PartBuilder, c):
    b.offset = Vector((0, 0, HEAD_UP))
    skin = c["skin"]
    hw, hh = c.get("head_size", (0.40, 0.32))
    b.box((hw, 0.36, hh), (0, 0, 1.33 + hh / 2), skin, "head", bevel=0.08, smooth=True)
    b.sphere(0.047, (hw / 2 + 0.003, 0.0, 1.47), skin, "head", scale=(0.5, 0.9, 1.1))
    b.sphere(0.047, (-hw / 2 - 0.003, 0.0, 1.47), skin, "head", scale=(0.5, 0.9, 1.1))
    # mouth cavity + teeth (visible when the jaw hinge opens)
    b.box((0.28, 0.2, 0.05), (0, -0.05, 1.33), "burgundy", "head")
    if c.get("gap_tooth"):
        b.box((0.09, 0.03, 0.036), (-0.068, -0.148, 1.318), "white", "head", bevel=0.006)
        b.box((0.09, 0.03, 0.036), (0.068, -0.148, 1.318), "white", "head", bevel=0.006)
    else:
        b.box((0.22, 0.03, 0.036), (0, -0.148, 1.318), "white", "head", bevel=0.006)
    if c.get("gold_tooth"):
        b.box((0.035, 0.032, 0.037), (0.06, -0.15, 1.318), "gold", "head", bevel=0.005)
    # hinged jaw
    b.box((0.34, 0.3, 0.09) if not c.get("long_chin") else (0.3, 0.3, 0.13),
          (0, -0.025, 1.286 if not c.get("long_chin") else 1.27), skin, "jaw", bevel=0.038, smooth=True)
    b.box((0.2, 0.025, 0.028), (0, -0.155, 1.334), "white", "jaw", bevel=0.005)
    if c.get("beard"):
        b.sphere(0.17, (0, -0.07, 1.25), c["hair"], "jaw", scale=(1.05, 0.75, 0.9))
    # nose, eyes, brows, cheeks
    nose_style = c.get("nose_style", "round")
    if nose_style == "long":
        b.cylinder(0.045, 0.012, 0.2, (0, -0.27, 1.45), c.get("nose", "peach"), "head", rot=(90, 0, 0), segs=10)
    elif nose_style == "hook":
        b.sphere(0.06, (0, -0.21, 1.44), c.get("nose", "peach"), "head", scale=(0.8, 1.5, 1.15), rot=(-25, 0, 0))
    elif nose_style == "button":
        b.sphere(0.035, (0, -0.195, 1.45), c.get("nose", "peach"), "head")
    else:
        b.sphere(0.056, (0, -0.21, 1.45), c.get("nose", "peach"), "head",
                 scale=c.get("nose_scale", (1, 1.35, 0.95)))
    for sx in (1, -1):
        es = c.get("eye_scale", 1.0)
        if c.get("eyepatch") and sx == -1:
            b.cylinder(0.06, 0.06, 0.02, (0.082 * sx, -0.185, 1.525), "black", "head", rot=(90, 0, 0), segs=14)
            b.box((hw + 0.02, 0.37, 0.018), (0, 0, 1.56), "black", "head", rot=(0, -20, 0))
            continue
        b.sphere(0.054 * es, (0.082 * sx, -0.172, 1.527), "white", "head", scale=(1, 0.42, 1.2))
        b.sphere(0.028 * es, (0.078 * sx, -0.192, 1.52), "ink", "head", scale=(1, 0.45, 1.05))
        if c.get("lids"):
            b.box((0.12 * es, 0.03, 0.05 * es), (0.082 * sx, -0.19, 1.555 + 0.01 * es), skin, "head",
                  rot=(0, -8 * sx, 0), bevel=0.01)
        b.sphere(0.009, (0.068 * sx, -0.203, 1.537), "white", "head", outline=False)
        brow = c.get("brow", 0) if sx == 1 else c.get("brow_r", c.get("brow", 0))
        b.box((0.1, 0.028, 0.026), (0.082 * sx, -0.182, 1.6 + c.get("brow_lift", 0)), c["hair"], "head",
              rot=(0, -brow * sx, 0), bevel=0.008)
        b.sphere(0.036, (0.145 * sx, -0.17, 1.43), "blush", "head", scale=(1, 0.3, 0.75), outline=False)
        if c.get("freckles"):
            for dx, dz in ((0.0, 0.0), (0.03, 0.012), (0.018, -0.02)):
                b.sphere(0.007, ((0.12 + dx) * sx, -0.186, 1.455 + dz), "rust", "head", outline=False)


def build_hair_and_hat(b: kg.PartBuilder, c):
    hw, hh = c.get("head_size", (0.40, 0.32))
    b.offset = Vector((0, 0, HEAD_UP + hh - 0.32))
    style = c.get("hair_style")
    hair = c["hair"]
    if style == "short":
        b.box((0.43, 0.39, 0.1), (0, 0.01, 1.648), hair, "head", bevel=0.045, smooth=True)
        b.box((0.42, 0.12, 0.24), (0, 0.145, 1.53), hair, "head", bevel=0.045, smooth=True)
        for x, z in ((-0.12, 1.625), (0.0, 1.635), (0.12, 1.62)):
            b.sphere(0.066, (x, -0.155, z), hair, "head", scale=(1.1, 0.8, 0.9))
    elif style == "tonsure":
        b.torus(0.2, 0.05, (0, 0.0, 1.545), hair, "head", scale=(1.02, 0.92, 1.0))
    elif style == "curly":
        for i in range(9):
            a = i * 2 * math.pi / 9
            b.sphere(0.09, (0.16 * math.cos(a), 0.14 * math.sin(a) + 0.02, 1.64), hair, "head")
        b.sphere(0.12, (0, 0.02, 1.68), hair, "head")
    elif style == "long":
        b.box((0.43, 0.39, 0.1), (0, 0.01, 1.648), hair, "head", bevel=0.045, smooth=True)
        b.box((0.44, 0.14, 0.42), (0, 0.15, 1.44), hair, "head", bevel=0.05, smooth=True)
    elif style == "bun":
        b.box((0.43, 0.39, 0.09), (0, 0.01, 1.646), hair, "head", bevel=0.04, smooth=True)
        b.sphere(0.12, (0, 0.12, 1.72), hair, "head")
    hat = c.get("hat")
    if hat == "bowler":
        b.cylinder(0.25, 0.25, 0.022, (0, 0, 1.655), "black", "head")
        b.sphere(0.19, (0, 0, 1.665), "black", "head", scale=(1, 1, 0.78))
        b.cylinder(0.193, 0.193, 0.045, (0, 0, 1.69), c.get("hat_band", "burgundy"), "head")
    elif hat == "top":
        b.cylinder(0.26, 0.26, 0.022, (0, 0, 1.655), c.get("hat_color", "navy"), "head")
        b.cylinder(0.165, 0.18, 0.32, (0, 0, 1.82), c.get("hat_color", "navy"), "head")
        b.cylinder(0.172, 0.182, 0.055, (0, 0, 1.7), c.get("hat_band", "crimson"), "head")
    elif hat == "tricorn":
        b.sphere(0.2, (0, 0, 1.66), c.get("hat_color", "dark_wood"), "head", scale=(1, 1, 0.55))
        for i in range(3):
            a = math.radians(90 + i * 120)
            b.box((0.3, 0.05, 0.1), (0.17 * math.cos(a), 0.17 * math.sin(a), 1.7),
                  c.get("hat_color", "dark_wood"), "head", rot=(0, 0, math.degrees(a) + 90), bevel=0.02)
        b.sphere(0.03, (0, -0.24, 1.72), "gold", "head")
    elif hat == "fisher":
        b.cylinder(0.215, 0.2, 0.12, (0, 0, 1.69), c.get("hat_color", "navy"), "head")
        b.box((0.26, 0.14, 0.025), (0, -0.2, 1.64), c.get("hat_color", "navy"), "head", rot=(-10, 0, 0),
              bevel=0.01)
    elif hat == "chef":
        b.cylinder(0.2, 0.2, 0.14, (0, 0, 1.7), "white", "head")
        b.sphere(0.22, (0, 0, 1.84), "white", "head", scale=(1.1, 1.1, 0.8))
    elif hat == "witch":
        b.cylinder(0.3, 0.3, 0.02, (0, 0, 1.655), c.get("hat_color", "plum"), "head", segs=18)
        b.cylinder(0.18, 0.01, 0.5, (0, 0.03, 1.91), c.get("hat_color", "plum"), "head", rot=(-12, 0, 0), segs=12)
    elif hat == "bonnet":
        b.sphere(0.235, (0, 0.045, 1.56), "ink", "head", scale=(1.05, 1.0, 0.98))
        b.box((0.06, 0.02, 0.14), (0.16, -0.12, 1.33), "plum", "head", rot=(0, 20, 0), bevel=0.01)
        b.box((0.06, 0.02, 0.14), (-0.16, -0.12, 1.33), "plum", "head", rot=(0, -20, 0), bevel=0.01)
    # facial hair / eyewear
    if c.get("mustache"):
        for sx in (1, -1):
            b.sphere(0.06, (0.075 * sx, -0.198, 1.385), c["hair"], "head", scale=(1.7, 0.6, 0.55),
                     rot=(0, 18 * sx, 0))
    if c.get("glasses"):
        for sx in (1, -1):
            b.torus(0.05, 0.009, (0.082 * sx, -0.2, 1.525), "charcoal", "head", rot=(90, 0, 0))
        b.box((0.05, 0.012, 0.012), (0, -0.205, 1.53), "charcoal", "head")
    if c.get("monocle"):
        b.torus(0.052, 0.01, (-0.082, -0.205, 1.525), "gold", "head", rot=(90, 0, 0))
        b.cylinder(0.005, 0.005, 0.2, (-0.12, -0.205, 1.42), "gold", "head", rot=(0, 20, 0))
    b.offset = Vector((0, 0, 0))


def build_extras(b: kg.PartBuilder, c):
    for extra in c.get("extras", []):
        if extra == "pan_r":
            b.box((0.032, 0.032, 0.24), (-0.25, -0.02, 0.54), "dark_wood", "hand_r", bevel=0.008)
            b.cylinder(0.14, 0.12, 0.04, (-0.25, -0.03, 0.34), "charcoal", "hand_r", rot=(90, 0, 0), segs=20)
            b.cylinder(0.118, 0.118, 0.012, (-0.25, -0.052, 0.34), "stone_dark", "hand_r", rot=(90, 0, 0), segs=20)
        elif extra == "umbrella_l":
            b.cylinder(0.013, 0.013, 0.66, (0.25, -0.02, 0.35), "charcoal", "hand_l")
            b.cylinder(0.02, 0.075, 0.42, (0.25, -0.02, 0.24), "crimson", "hand_l", segs=8)
            b.sphere(0.03, (0.25, -0.02, 0.705), "gold", "hand_l")
        elif extra == "bell_r":
            b.cylinder(0.02, 0.02, 0.12, (-0.25, -0.02, 0.58), "dark_wood", "hand_r")
            b.cylinder(0.1, 0.045, 0.14, (-0.25, -0.02, 0.46), "gold", "hand_r", segs=16)
            b.sphere(0.028, (-0.25, -0.02, 0.385), "gold", "hand_r")
        elif extra == "cape":
            b.box((0.46, 0.035, 0.66), (0, 0.15, 0.92), c.get("cape", "plum"), "spine_02", rot=(8, 0, 0),
                  bevel=0.012)
            for sx in (1, -1):
                b.sphere(0.06, (0.15 * sx, -0.06, 1.23), c.get("cape", "plum"), "spine_02", scale=(1.2, 1.2, 0.6))
        elif extra == "scarf":
            b.torus(0.085, 0.042, (0, -0.01, 1.27), "crimson", "neck_01", scale=(1, 1, 0.8))
            b.box((0.07, 0.03, 0.22), (0.07, -0.13, 1.13), "crimson", "spine_02", rot=(-8, 0, -10), bevel=0.012)
        elif extra == "watch_chain":
            b.torus(0.03, 0.007, (0.1, -0.2, 1.02), "gold", "spine_01", rot=(90, 0, 0))


CAST = [
    dict(name="DulMarrow", x=-1.95, skin="skin1", hair="ink", hair_style="bun", hat="bonnet", top="ink",
         sleeve="ink", pants="ink", socks="plum", boots="black", gloves="violet", long_chin=True, brow=10,
         brow_r=-18, brow_lift=0.01, nose="skin2", extras=["cape"], cape="plum", jaw=14,
         pose=[("upperarm_l", (1, 0, 0), -78), ("lowerarm_l", (1, 0, 0), -8), ("head", (0, 1, 0), -6)]),
    dict(name="BayPozzo", x=-0.97, skin="skin2", hair="dark_wood", hat="bowler", top="burgundy", vest="mustard",
         sleeve="burgundy", pants="charcoal", socks="charcoal", boots="dark_wood", gloves="white", mustache=True,
         nose="coral", nose_scale=(1.15, 1.45, 1.0), brow=-8, jaw=9, gold_tooth=True, extras=["watch_chain"],
         pose=[("upperarm_l", (0, 1, 0), -22), ("upperarm_r", (0, 1, 0), 22), ("lowerarm_l", (1, 0, 0), -70),
               ("lowerarm_r", (1, 0, 0), -70), ("spine_02", (1, 0, 0), 6)]),
    dict(name="TillyCil", x=0.0, skin="skin1", hair="orange", hair_style="short", top="teal", sleeve="teal",
         pants="navy", socks="mustard", boots="dark_wood", gloves="mustard", gap_tooth=True, freckles=True,
         brow=-14, brow_lift=0.012, jaw=24, extras=["pan_r"], buttons="mustard",
         pose=[("upperarm_r", (0, 1, 0), 145), ("lowerarm_r", (0, 1, 0), 25), ("upperarm_l", (0, 1, 0), -30),
               ("lowerarm_l", (1, 0, 0), -40), ("head", (0, 1, 0), 5)]),
    dict(name="PederBellweather", x=0.97, skin="skin3", hair="dark_wood", hair_style="tonsure", top="wood",
         sleeve="wood", pants="wood", socks="dark_wood", boots="black", gloves="cream", glasses=True, brow=-6,
         nose="skin4", jaw=18, extras=["bell_r"], buttons="gold",
         pose=[("upperarm_r", (1, 0, 0), -55), ("lowerarm_r", (1, 0, 0), -45), ("head", (1, 0, 0), -6)]),
    dict(name="Tick", x=1.95, skin="skin4", hair="black", hat="top", hat_color="navy", hat_band="crimson",
         top="navy", sleeve="navy", pants="charcoal", socks="crimson", boots="black", gloves="black",
         monocle=True, brow=16, brow_r=4, nose="skin5", jaw=3, extras=["umbrella_l", "scarf"], buttons="gold",
         pose=[("upperarm_l", (0, 1, 0), -12), ("head", (0, 1, 0), 9), ("upperarm_r", (1, 0, 0), -20),
               ("lowerarm_r", (1, 0, 0), -60)]),
]


def build_character(collection, c):
    rig = make_armature(collection, c["name"])
    b = kg.PartBuilder(f"SKM_KG_{c['name']}")
    build_body(b, c)
    build_head(b, c)
    build_hair_and_hat(b, c)
    build_extras(b, c)
    b.build(collection, outline=0.014, armature=rig)
    rot_bone(rig, "jaw", (1, 0, 0), c.get("jaw", 0))
    for bone, axis, deg in c.get("pose", []):
        rot_bone(rig, bone, axis, deg)
    rig.location = (c["x"], 0.0, 0.0)
    rig.rotation_euler = (0, 0, -math.atan2(c["x"], 6.3) * 0.9)
    return rig


# ----------------------------------------------------------------------------------------------------------------
# Village backdrop
# ----------------------------------------------------------------------------------------------------------------
def house(b, cx, cy, w, d, h, wall, roof, trim="dark_wood", door="mustard"):
    b.box((w, d, h), (cx, cy, h / 2), wall, bevel=0.05)
    front = cy - d / 2
    for x in (cx - w / 2 + 0.08, cx + w / 2 - 0.08, cx):
        b.box((0.16, 0.12, h), (x, front - 0.03, h / 2), trim)
    b.box((w, 0.12, 0.16), (cx, front - 0.03, h * 0.52), trim)
    b.box((w + 0.1, 0.14, 0.18), (cx, front - 0.03, h - 0.05), trim)
    # roof: triangular prism along X, apex up
    b.cylinder(d * 0.72, d * 0.72, w + 0.5, (cx, cy, h + d * 0.2), roof, rot=(0, -90, 0), segs=3,
               scale=(0.75, 1.12, 1.0), smooth=False, bevel=0.03)
    b.box((0.42, 0.42, 1.2), (cx + w * 0.28, cy + 0.2, h + 0.9), "brick", bevel=0.03)
    # door + windows + flower boxes
    b.box((0.9, 0.14, 1.7), (cx - w * 0.18, front - 0.04, 0.85), door, bevel=0.03)
    b.sphere(0.05, (cx - w * 0.18 + 0.3, front - 0.13, 0.85), "gold")
    for wx, wz in ((cx + w * 0.22, 1.1), (cx - w * 0.3, h * 0.75), (cx + w * 0.22, h * 0.75)):
        b.box((0.82, 0.1, 0.82), (wx, front - 0.03, wz), "white", bevel=0.02)
        b.box((0.64, 0.12, 0.64), (wx, front - 0.04, wz), "sky_blue", emissive=False)
        b.box((0.08, 0.13, 0.64), (wx, front - 0.05, wz), "white")
        b.box((0.9, 0.3, 0.18), (wx, front - 0.18, wz - 0.5), "wood", bevel=0.02)
        for i, col in enumerate(("roof_red", "magenta", "sunflower", "coral")):
            b.sphere(0.075, (wx - 0.3 + i * 0.2, front - 0.2, wz - 0.36), col)


def build_village(collection):
    env = kg.PartBuilder("KG_Env")
    env.box((80, 80, 0.2), (0, 20, -0.1), "meadow")
    env.cylinder(5.2, 5.2, 0.06, (0, 0.6, 0.0), "sand", segs=40)
    for i in range(10):
        a = i * 2 * math.pi / 10
        env.box((0.7, 0.5, 0.03), (4.4 * math.cos(a), 0.6 + 4.4 * math.sin(a), 0.035), "stone_dark", bevel=0.01,
                rot=(0, 0, math.degrees(a)))
    house(env, -3.1, 4.0, 3.8, 2.6, 3.0, "cream", "roof_red")
    house(env, 3.2, 4.3, 3.4, 2.6, 2.8, "pastel_orange", "teal", trim="wood", door="royal_blue")
    house(env, 0.1, 7.2, 3.0, 2.4, 3.6, "pastel_yellow", "orange", door="emerald")
    # sea, cliffs, lighthouse, hills, clouds
    env.box((120, 60, 0.1), (0, 52, -0.05), "sea_turquoise")
    env.sphere(9, (-18, 30, -3), "forest", scale=(1.4, 1, 0.9))
    env.sphere(12, (22, 34, -4), "moss", scale=(1.5, 1, 0.9))
    env.cylinder(0.9, 0.7, 7, (-14, 26, 5.5), "white", segs=16)
    for z in (3.2, 5.6, 8.0):
        env.cylinder(0.86, 0.8, 0.8, (-14, 26, z), "roof_red", segs=16)
    env.cylinder(0.6, 0.6, 1.0, (-14, 26, 9.5), "lantern", segs=12)
    env.cylinder(0.9, 0.05, 1.0, (-14, 26, 10.5), "crimson", segs=12)
    for cx, cz, s in ((-9, 13, 1.0), (7, 15, 1.3), (20, 11, 0.9)):
        for dx, dz, r in ((0, 0, 2.2), (2.3, 0.4, 1.8), (-2.2, -0.2, 1.6), (0.8, 1.4, 1.7)):
            env.sphere(r * s, (cx + dx * s, 42, cz + dz * s), "white", scale=(1.2, 0.8, 0.8))
    # props: barrels, crates, lamp post, dead tree
    for x, y in ((2.2, 2.0), (2.75, 2.25), (-4.6, 1.8)):
        env.cylinder(0.3, 0.3, 0.7, (x, y, 0.35), "wood", segs=14)
        for z in (0.12, 0.58):
            env.cylinder(0.315, 0.315, 0.05, (x, y, z), "dark_wood", segs=14)
    for x, y, z in ((-3.9, 2.0, 0.3), (-3.35, 2.1, 0.3), (-3.62, 2.05, 0.88)):
        env.box((0.56, 0.56, 0.56), (x, y, z), "light_wood", bevel=0.03)
    env.cylinder(0.05, 0.06, 3.0, (-2.3, 1.2, 1.5), "charcoal", segs=10)
    env.box((0.28, 0.28, 0.36), (-2.3, 1.2, 3.1), "lantern", emissive=True)
    env.cylinder(0.24, 0.02, 0.2, (-2.3, 1.2, 3.38), "charcoal", segs=4)
    env.cylinder(0.14, 0.2, 2.6, (4.9, 1.2, 1.3), "dark_wood", segs=9)
    for rot, cz in (((0, 35, 20), 2.3), ((0, -40, -30), 2.0), ((25, 20, 140), 2.6)):
        env.cylinder(0.05, 0.08, 1.3, (4.9, 1.2, cz), "dark_wood", rot=rot, segs=6)
    env.build(collection, outline=0.025)


# ----------------------------------------------------------------------------------------------------------------
# First-person viewmodel mock (CS2 composition: right hand, lower-right, item pointing into the scene)
# ----------------------------------------------------------------------------------------------------------------
def cylinder_between(b, p1, p2, r1, r2, color, segs=14):
    p1, p2 = Vector(p1), Vector(p2)
    d = p2 - p1
    q = d.to_track_quat("Z", "Y")
    b.cylinder(r1, r2, d.length, (p1 + p2) / 2, color, rot=[math.degrees(a) for a in q.to_euler()], segs=segs)


def build_viewmodel(collection, cam_obj):
    vm = kg.PartBuilder("SKM_KG_FP_Arms")
    # local frame: X right, Y forward, Z up (metres, camera at origin)
    cylinder_between(vm, (0.40, 0.22, -0.34), (0.235, 0.455, -0.175), 0.062, 0.053, "teal")
    cylinder_between(vm, (0.24, 0.45, -0.178), (0.222, 0.49, -0.158), 0.071, 0.068, "mustard")
    vm.sphere(0.07, (0.205, 0.525, -0.14), "mustard", scale=(0.85, 1.15, 0.8))
    vm.sphere(0.028, (0.172, 0.53, -0.115), "mustard", scale=(1, 1.5, 1))
    # frying pan held like a blade: handle forward/up, pan face turned towards the player's left
    cylinder_between(vm, (0.198, 0.53, -0.135), (0.155, 0.70, -0.045), 0.014, 0.016, "dark_wood", segs=8)
    vm.cylinder(0.092, 0.08, 0.026, (0.132, 0.795, 0.0), "charcoal", rot=(0, 75, 10), segs=24)
    vm.cylinder(0.078, 0.078, 0.009, (0.119, 0.797, 0.0), "stone_dark", rot=(0, 75, 10), segs=24)
    obj = vm.build(collection, outline=0.0028)
    conv = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1)))
    obj.matrix_world = cam_obj.matrix_world @ conv
    return obj


# ----------------------------------------------------------------------------------------------------------------
def render(scene, cam, path):
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print(f"KG_RENDER_OK {path}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out_dir, exist_ok=True)

    kg.reset_scene()
    scene = bpy.context.scene
    col = scene.collection
    kg.setup_toon_render(scene, sky="sky_blue", ambient=0.0)
    kg.add_sun(col, rot_deg=(50, 0, -32), strength=2.8)

    build_village(col)
    for c in CAST:
        build_character(col, c)

    lineup = kg.add_camera(col, (0, -6.2, 1.3), (0, 0.4, 1.12), lens=38)
    closeup = kg.add_camera(col, (0.36, -1.55, 1.58), (0.0, 0.0, 1.47), lens=55)
    fp = kg.add_camera(col, (1.35, -4.2, 1.55), (-0.5, 0.6, 1.3), lens=26)
    fp.data.clip_start = 0.01
    bpy.context.view_layer.update()
    vm = build_viewmodel(col, fp)

    vm.hide_render = True
    render(scene, lineup, os.path.join(out_dir, "KG_Proto_Lineup.png"))
    render(scene, closeup, os.path.join(out_dir, "KG_Proto_Talking.png"))
    vm.hide_render = False
    render(scene, fp, os.path.join(out_dir, "KG_Proto_Viewmodel.png"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "KG_PuppetPrototype.blend"))
    print("KG_PROTO_DONE")


if __name__ == "__main__":
    main()
