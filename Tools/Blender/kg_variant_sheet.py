"""Kill Godot - "generate many, keep the best" contact sheets for puppet Faces.

Every card is a deterministic random puppet built from a seed; pick favourites by their #seed number and they
can be rebuilt exactly (same seed -> same design).

Usage:
  blender --background --factory-startup --python kg_variant_sheet.py -- <out_dir> [first_seed] [sheets]
"""
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import kg_common as kg  # noqa: E402
import kg_puppet_prototype as proto  # noqa: E402

COLS, ROWS = 8, 3
SPACING_X, SPACING_Z = 0.95, 1.25

SKINS = ["skin1", "skin2", "skin3", "skin4", "skin5"]
HAIR_COLORS = ["orange", "ink", "dark_wood", "sunflower", "white", "roof_red", "plum", "black", "light_wood"]
HAIR_STYLES = ["short", "curly", "long", "bun", "tonsure", None]
HATS = [None, None, None, "bowler", "top", "bonnet", "tricorn", "fisher", "chef", "witch"]
TOPS = ["teal", "royal_blue", "crimson", "emerald", "mustard", "plum", "burgundy", "navy", "coral", "orange",
        "violet", "magenta", "sea_turquoise", "wood"]
HAT_COLORS = ["navy", "dark_wood", "plum", "black", "crimson", "emerald", "royal_blue"]
NOSES = ["round", "round", "long", "hook", "button"]


def random_face(seed):
    r = random.Random(seed)
    skin = r.choice(SKINS)
    c = dict(
        name=f"Face{seed:03d}",
        skin=skin,
        hair=r.choice(HAIR_COLORS),
        hair_style=r.choice(HAIR_STYLES),
        hat=r.choice(HATS),
        hat_color=r.choice(HAT_COLORS),
        hat_band=r.choice(["crimson", "gold", "mustard", "teal"]),
        top=r.choice(TOPS),
        pants="charcoal", socks="charcoal", boots="dark_wood", gloves=r.choice(["white", "mustard", "black", "cream"]),
        nose_style=r.choice(NOSES),
        nose=r.choice([skin, "coral", "peach"]) if skin in ("skin1", "skin2") else skin,
        brow=r.randint(-18, 18),
        brow_lift=r.uniform(-0.01, 0.015),
        eye_scale=r.uniform(0.85, 1.25),
        lids=r.random() < 0.2,
        head_size=(r.uniform(0.36, 0.44), r.uniform(0.28, 0.36)),
        long_chin=r.random() < 0.2,
        jaw=r.uniform(4, 24),
        mustache=r.random() < 0.2,
        beard=r.random() < 0.14,
        glasses=r.random() < 0.12,
        monocle=r.random() < 0.06,
        eyepatch=r.random() < 0.05,
        freckles=r.random() < 0.2,
        gap_tooth=r.random() < 0.15,
        gold_tooth=r.random() < 0.1,
        extras=[e for e, p in (("scarf", 0.2), ("cape", 0.12)) if r.random() < p],
        cape=r.choice(["plum", "crimson", "navy"]),
    )
    if r.random() < 0.3:
        c["brow_r"] = r.randint(-18, 18)  # asymmetric brows = instant character
    if c["glasses"] and c["monocle"]:
        c["monocle"] = False
    return c


def build_face(collection, c, location):
    rig = proto.make_armature(collection, c["name"])
    b = kg.PartBuilder(f"SKM_KG_{c['name']}")
    # Bust only: shoulders + neck + head, so cards never overlap.
    b.sphere(0.215, (0, 0, 1.2), c["top"], "spine_02", scale=(1, 0.9, 0.5))
    b.cylinder(0.066, 0.066, 0.12 + proto.HEAD_UP, (0, 0, 1.28 + proto.HEAD_UP / 2), proto.JOINT, "neck_01")
    proto.build_head(b, c)
    proto.build_hair_and_hat(b, c)
    c = dict(c, extras=[e for e in c.get("extras", []) if e == "scarf"])
    proto.build_extras(b, c)
    b.build(collection, outline=0.014, armature=rig)
    proto.rot_bone(rig, "jaw", (1, 0, 0), c["jaw"])
    rig.location = location
    return rig


def add_label(collection, text, location):
    plate = kg.PartBuilder(f"Label_{text}")
    plate.box((0.26, 0.02, 0.09), (0, 0, 0), "cream", bevel=0.02)
    obj = plate.build(collection, outline=0.006)
    obj.location = location
    curve = bpy.data.curves.new(f"LabelText_{text}", "FONT")
    curve.body = text
    curve.size = 0.07
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    txt = bpy.data.objects.new(f"LabelText_{text}", curve)
    txt.location = Vector(location) + Vector((0, -0.02, 0))
    txt.rotation_euler = (math.radians(90), 0, 0)
    txt.data.materials.append(kg.toon_material("ink", emissive=True))
    collection.objects.link(txt)


def render_sheet(out_dir, first_seed):
    kg.reset_scene()
    scene = bpy.context.scene
    col = scene.collection
    kg.setup_toon_render(scene, sky="pastel_blue", ambient=0.0, res=(2400, 1350), samples=16)
    kg.add_sun(col, rot_deg=(55, 0, -28), strength=2.8)

    for i in range(COLS * ROWS):
        seed = first_seed + i
        row, colm = divmod(i, COLS)
        row = ROWS - 1 - row  # first seeds on the top row
        x = colm * SPACING_X
        y = 0.0
        z = row * SPACING_Z
        build_face(col, random_face(seed), (x, y, z))
        add_label(col, f"#{seed}", (x, y - 0.3, z + 1.02))

    cam = bpy.data.cameras.new("Sheet")
    cam.type = "ORTHO"
    cam.ortho_scale = COLS * SPACING_X + 0.5
    cam_obj = bpy.data.objects.new("Sheet", cam)
    cx = (COLS - 1) * SPACING_X / 2
    cz = 1.6 + (ROWS - 1) * SPACING_Z / 2
    cam_obj.location = (cx, -12, cz)
    cam_obj.rotation_euler = (math.radians(90), 0, 0)
    col.objects.link(cam_obj)
    scene.camera = cam_obj
    path = os.path.join(out_dir, f"KG_Faces_{first_seed:03d}-{first_seed + COLS * ROWS - 1:03d}.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print(f"KG_SHEET_OK {path}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.abspath(__file__))
    first = int(argv[1]) if len(argv) > 1 else 1
    sheets = int(argv[2]) if len(argv) > 2 else 1
    os.makedirs(out_dir, exist_ok=True)
    for s in range(sheets):
        render_sheet(out_dir, first + s * COLS * ROWS)


if __name__ == "__main__":
    main()
