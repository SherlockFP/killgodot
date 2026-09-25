"""Compose Quaternius villagers (UBC head + Modular Outfit + hair + brows) on one armature and export for UE.

Outputs to <out_dir>:
  SK_KG_Villager_M.fbx, SK_KG_Villager_F.fbx   skinned, textures embedded, armature object "Armature"
  A_KG_UAL1.fbx                                  UAL1 animations (armature only) for the same skeleton
  KG_Villagers_Preview.png                       quick lineup render

Usage: blender --background --factory-startup --python kg_build_villagers.py -- <out_dir>
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

SRC = "D:/Kill Godot/Art/Source"
UBC = f"{SRC}/Quaternius_UniversalBaseCharacters/Universal Base Characters[Standard]/Universal Base Characters[Standard]"
OUTFITS = (f"{SRC}/Quaternius_ModularOutfitsFantasy/Modular Character Outfits - Fantasy[Standard]/"
           "Modular Character Outfits - Fantasy[Standard]/Exports/glTF (Godot-Unreal)/Outfits")
UAL1 = (f"{SRC}/Quaternius_UniversalAnimationLibrary/Universal Animation Library[Standard]/"
        "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb")

VILLAGERS = {
    # name: (base body, outfit, hair, brows, hair colour (linear))
    "SK_KG_Villager_M": ("Superhero_Male_FullBody.gltf", "Male_Peasant.gltf", "Hair_SimpleParted.gltf",
                         "Eyebrows_Regular.gltf", (0.30, 0.09, 0.02, 1.0)),
    "SK_KG_Villager_F": ("Superhero_Female_FullBody.gltf", "Female_Peasant.gltf", "Hair_Long.gltf",
                         "Eyebrows_Female.gltf", (0.55, 0.12, 0.02, 1.0)),
}


def find(root, name):
    for dirpath, _, files in os.walk(root):
        if name in files and ("Godot" in dirpath or "gltf" in dirpath.lower()):
            return os.path.join(dirpath, name)
    raise FileNotFoundError(name)


def import_gltf(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def keep_bones(obj, keywords):
    """Delete vertices whose dominant bone does not contain any keyword."""
    names = {g.index: g.name.lower() for g in obj.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    dl = bm.verts.layers.deform.active
    doomed = []
    for v in bm.verts:
        w = v[dl] if dl else {}
        if w:
            gi = max(w.items(), key=lambda kv: kv[1])[0]
            if not any(k in names.get(gi, "") for k in keywords):
                doomed.append(v)
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()


def tint_hair(objs, color):
    for o in objs:
        if o.type != "MESH":
            continue
        for mat in o.data.materials:
            if mat and mat.use_nodes:
                bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
                if bsdf and not bsdf.inputs["Base Color"].is_linked:
                    bsdf.inputs["Base Color"].default_value = color
                elif bsdf:
                    # Multiply the texture with the hair colour so it stays textured but not white.
                    link = bsdf.inputs["Base Color"].links[0]
                    mix = mat.node_tree.nodes.new("ShaderNodeMix")
                    mix.data_type = "RGBA"
                    mix.blend_type = "MULTIPLY"
                    mix.inputs["Factor"].default_value = 1.0
                    mix.inputs[7].default_value = color
                    mat.node_tree.links.new(link.from_socket, mix.inputs[6])
                    mat.node_tree.links.new(mix.outputs[2], bsdf.inputs["Base Color"])


def compose(name, spec):
    body, outfit, hair, brows, hair_color = spec
    groups = []
    main_arm = None
    # Brows ship inside the base body already (a separate brows file only duplicated them, untinted/white).
    for path, is_body, is_hair in ((find(UBC, body), True, False), (os.path.join(OUTFITS, outfit), False, False),
                                   (find(UBC, hair), False, True)):
        objs = import_gltf(path)
        arm = next((o for o in objs if o.type == "ARMATURE"), None)
        meshes = [o for o in objs if o.type == "MESH"]
        if is_body:
            for m in meshes:
                keep_bones(m, ("head", "neck", "eye", "jaw"))
        if is_hair:
            tint_hair(meshes, hair_color)
            # UBC hair is an unskinned static mesh: bind it 100% to the head bone.
            for m in meshes:
                vg = m.vertex_groups.new(name="Head")
                vg.add(list(range(len(m.data.vertices))), 1.0, "REPLACE")
                if not any(mod.type == "ARMATURE" for mod in m.modifiers):
                    m.modifiers.new("Armature", "ARMATURE")
        if main_arm is None:
            main_arm = arm
        groups.append((arm, meshes))

    # Re-parent every mesh to the first armature, drop the duplicates.
    for arm, meshes in groups:
        for m in meshes:
            mw = m.matrix_world.copy()
            m.parent = main_arm
            m.matrix_world = mw
            for mod in m.modifiers:
                if mod.type == "ARMATURE":
                    mod.object = main_arm
        if arm is not main_arm and arm is not None:
            bpy.data.objects.remove(arm, do_unlink=True)
    for o in list(bpy.data.objects):
        if o.type == "EMPTY" and not o.children:
            bpy.data.objects.remove(o, do_unlink=True)

    meshes = [c for c in main_arm.children if c.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    main_arm.name = "Armature"
    return main_arm, joined


def export_fbx(path, objs, anim=False, nla=False):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
        bake_anim=anim, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, path_mode="COPY", embed_textures=not anim,
        mesh_smooth_type="FACE", use_armature_deform_only=False, armature_nodetype="NULL",
        apply_scale_options="FBX_SCALE_NONE", primary_bone_axis="Y", secondary_bone_axis="X")
    print(f"KG_EXPORT_OK {path}")


def render_preview(path, rigs):
    scene = bpy.context.scene
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.62, 0.8, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.location = (0, -4.6, 1.1)
    cam.rotation_euler = (Vector((0, 0, 0.95)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    out_dir = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
    os.makedirs(out_dir, exist_ok=True)

    # Characters (one scene each so names/armatures never collide).
    for name, spec in VILLAGERS.items():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        arm, mesh = compose(name, spec)
        export_fbx(os.path.join(out_dir, f"{name}.fbx"), [arm, mesh])
        # First-person arms: same skeleton, only shoulder/arm/hand/finger vertices (no torso in front of the camera).
        keep_bones(mesh, ("clavicle", "upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky"))
        fp_name = name.replace("SK_KG_Villager", "SK_KG_FPArms")
        mesh.name = fp_name
        export_fbx(os.path.join(out_dir, f"{fp_name}.fbx"), [arm, mesh])

    # Animations: UAL1 armature + all actions.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs = import_gltf(UAL1)
    arm = next(o for o in objs if o.type == "ARMATURE")
    arm.name = "Armature"
    for o in objs:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    # Blender 5 FBX export merges NLA/all-actions into one stack, so export one file per clip.
    anim_dir = os.path.join(out_dir, "Anims")
    os.makedirs(anim_dir, exist_ok=True)
    ad = arm.animation_data or arm.animation_data_create()
    seen = set()
    for action in sorted(bpy.data.actions, key=lambda a: a.name):
        base = action.name.split(".")[0]
        if base in seen or base == "A_TPose":
            continue
        seen.add(base)
        ad.action = action
        if hasattr(ad, "action_slot") and getattr(action, "slots", None):
            ad.action_slot = action.slots[0]
        bpy.context.scene.frame_start, bpy.context.scene.frame_end = (int(action.frame_range[0]),
                                                                      int(action.frame_range[1]))
        export_fbx(os.path.join(anim_dir, f"A_KG_{base}.fbx"), [arm], anim=True)
    print(f"KG_ANIMS {len(seen)}")

    # Preview both villagers side by side.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for i, (name, spec) in enumerate(VILLAGERS.items()):
        arm, _ = compose(name, spec)
        arm.location.x = (i - 0.5) * 1.2
    render_preview(os.path.join(out_dir, "KG_Villagers_Preview.png"), None)
    print("KG_VILLAGERS_DONE")


main()
