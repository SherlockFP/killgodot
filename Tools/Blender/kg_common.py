"""Shared helpers for Kill Godot Blender tools (Blender 5.2, headless-safe).

- PALETTE: the 8x8 master palette (see Docs/06_Art_Direction.md)
- toon_material / outline_material: Among-Us-3D-style cel shading + inverted-hull outline (EEVEE)
- PartBuilder: builds one skinned mesh out of rigid parts (every part 100% weighted to one bone)
"""
import bpy
import bmesh
import colorsys
import math
from mathutils import Euler, Matrix, Vector

# 8 rows x 8 cols. Row/col order is the texel layout of T_KG_Palette.
PALETTE = [
    [("sea_turquoise", "1FB5C4"), ("deep_sea", "0E6E8C"), ("sky_blue", "7FD3F0"), ("ghost_cyan", "9FF3FF"),
     ("night_indigo", "26235C"), ("dusk_pink", "E0529C"), ("fog_lilac", "C9C3E6"), ("plum", "7B3F9E")],
    [("meadow", "7ACC3D"), ("forest", "2E8B57"), ("moss", "5A7D2A"), ("lime", "B8E04A"),
     ("sand", "E8C98A"), ("dirt", "9C6B3F"), ("stone_light", "B9B2A8"), ("stone_dark", "6E6A67")],
    [("roof_red", "E0413A"), ("orange", "F28C28"), ("mustard", "F2C230"), ("lantern", "FFB347"),
     ("crimson", "C8102E"), ("coral", "FF6F61"), ("peach", "FFB38A"), ("brick", "A8432E")],
    [("cream", "FFF1D6"), ("white", "FAFAFA"), ("wood", "A0612B"), ("dark_wood", "5A3A22"),
     ("light_wood", "D9A066"), ("charcoal", "2B2B2B"), ("black", "111111"), ("gray", "8C8C8C")],
    [("skin1", "F6D3B3"), ("skin2", "E8B48A"), ("skin3", "C98B5E"), ("skin4", "8D5A3B"),
     ("skin5", "5C3A24"), ("blush", "F28AA0"), ("gold", "D4A017"), ("silver", "C0C6CC")],
    [("royal_blue", "2F5BD3"), ("teal", "1A9E8F"), ("violet", "8E44AD"), ("magenta", "D6337F"),
     ("emerald", "1FAA59"), ("navy", "1F2A5A"), ("burgundy", "7A1F3D"), ("sunflower", "FFD23F")],
    [("pastel_pink", "F7C6D9"), ("pastel_blue", "BFE3F5"), ("pastel_green", "CDEFC0"), ("pastel_yellow", "FFF3B0"),
     ("pastel_purple", "D9C6F2"), ("pastel_orange", "FFD8B0"), ("mint", "A8E6CF"), ("lavender", "B8A9E3")],
    [("ember", "FF5A1F"), ("poison", "7CFC00"), ("paint_red", "B0132E"), ("sapphire", "0F52BA"),
     ("rust", "B7410E"), ("bone", "EDE3C8"), ("ink", "1B1B3A"), ("glow_white", "FFFDE7")],
]
PALETTE_HEX = {name: hexv for row in PALETTE for name, hexv in row}


def hex_to_srgb(hexv):
    hexv = PALETTE_HEX.get(hexv, hexv).lstrip("#")
    return tuple(int(hexv[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_rgba(hexv, v_mul=1.0, s_mul=1.0, h_shift=0.0):
    """Palette name or hex -> linear RGBA, optionally shifted in HSV (for toon shadow/highlight bands)."""
    r, g, b = hex_to_srgb(hexv)
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    r, g, b = colorsys.hsv_to_rgb((h + h_shift) % 1.0, min(1.0, s * s_mul), min(1.0, v * v_mul))
    return (srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), 1.0)


_MAT_CACHE = {}
OUTLINE_GROUP = "KG_Outline"


def toon_material(color, emissive=False):
    """3-band cel material. Shadow band is a darker, more saturated, slightly cooler version of the colour."""
    key = ("toon", color, emissive)
    if key in _MAT_CACHE:
        return _MAT_CACHE[key]
    mat = bpy.data.materials.new(f"M_KG_Toon_{color}")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    if emissive:
        emit.inputs["Color"].default_value = linear_rgba(color)
        emit.inputs["Strength"].default_value = 2.5
    else:
        diffuse = nt.nodes.new("ShaderNodeBsdfDiffuse")
        to_rgb = nt.nodes.new("ShaderNodeShaderToRGB")
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.interpolation = "CONSTANT"
        els = ramp.color_ramp.elements
        els[0].position = 0.0
        els[0].color = linear_rgba(color, 0.58, 1.15, -0.02)
        els[1].position = 0.1
        els[1].color = linear_rgba(color)
        hi = els.new(0.78)
        hi.color = linear_rgba(color, 1.1, 0.9)
        nt.links.new(diffuse.outputs[0], to_rgb.inputs[0])
        nt.links.new(to_rgb.outputs["Color"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], emit.inputs["Color"])
    mat.diffuse_color = linear_rgba(color)
    mat["kg_color"] = color
    _MAT_CACHE[key] = mat
    return mat


def bake_to_vertex_colors(obj, body_name="M_KG_PuppetBody", outline_name="M_KG_Outline"):
    """For UE export: palette materials -> sRGB vertex colours; mesh ends up with 2 slots (body, outline hull)."""
    me = obj.data
    attr = me.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for poly in me.polygons:
        mat = me.materials[poly.material_index] if poly.material_index < len(me.materials) else None
        color = mat.get("kg_color") if mat else None
        r, g, b = hex_to_srgb(color) if color else (1.0, 0.0, 1.0)
        for li in poly.loop_indices:
            attr.data[li].color_srgb = (r, g, b, 1.0)
        poly.material_index = 0
    me.materials.clear()
    body = bpy.data.materials.get(body_name) or bpy.data.materials.new(body_name)
    outline = bpy.data.materials.get(outline_name) or bpy.data.materials.new(outline_name)
    me.materials.append(body)
    me.materials.append(outline)
    for mod in obj.modifiers:
        if mod.type == "SOLIDIFY":
            mod.material_offset = 1
    return obj


def outline_material():
    key = ("outline",)
    if key in _MAT_CACHE:
        return _MAT_CACHE[key]
    mat = bpy.data.materials.new("M_KG_Outline")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (0.012, 0.008, 0.025, 1.0)
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    mat.use_backface_culling = True
    for attr in ("use_backface_culling_shadow",):
        if hasattr(mat, attr):
            setattr(mat, attr, True)
    _MAT_CACHE[key] = mat
    return mat


def _torus(bm, major, minor, seg=16, ring=6):
    verts = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        for j in range(ring):
            b = 2 * math.pi * j / ring
            r = major + minor * math.cos(b)
            verts.append(bm.verts.new((r * math.cos(a), r * math.sin(a), minor * math.sin(b))))
    for i in range(seg):
        for j in range(ring):
            a = verts[i * ring + j]
            b = verts[((i + 1) % seg) * ring + j]
            c = verts[((i + 1) % seg) * ring + (j + 1) % ring]
            d = verts[i * ring + (j + 1) % ring]
            bm.faces.new((a, b, c, d))


class PartBuilder:
    """Accumulates rigid parts into one mesh; each part gets a material and a 100% weight to one bone."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.materials = []
        self.groups = []
        self.offset = Vector((0.0, 0.0, 0.0))

    def _index(self, lst, item):
        if item not in lst:
            lst.append(item)
        return lst.index(item)

    def _append(self, tmp, color, bone, xform, smooth, bevel, emissive=False, outline=True):
        bmesh.ops.transform(tmp, matrix=xform, verts=tmp.verts)
        if bevel > 0:
            bmesh.ops.bevel(tmp, geom=list(tmp.edges), offset=bevel, segments=2, profile=0.5, affect="EDGES",
                            clamp_overlap=True)
        me = bpy.data.meshes.new("_tmp")
        tmp.to_mesh(me)
        tmp.free()
        start = len(self.bm.verts)
        start_f = len(self.bm.faces)
        self.bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        self.bm.verts.ensure_lookup_table()
        self.bm.faces.ensure_lookup_table()
        mi = self._index(self.materials, toon_material(color, emissive))
        gi = self._index(self.groups, bone)
        oi = self._index(self.groups, OUTLINE_GROUP) if outline else None
        for v in self.bm.verts[start:]:
            v[self.deform][gi] = 1.0
            if oi is not None:
                v[self.deform][oi] = 1.0
        for f in self.bm.faces[start_f:]:
            f.material_index = mi
            f.smooth = smooth

    def _matrix(self, center, rot, scale):
        return (Matrix.Translation(Vector(center) + self.offset) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
                @ Matrix.Diagonal((*scale, 1.0)))

    def box(self, size, center, color, bone="root", rot=(0, 0, 0), bevel=0.0, smooth=False, emissive=False,
            outline=True):
        tmp = bmesh.new()
        bmesh.ops.create_cube(tmp, size=1.0)
        self._append(tmp, color, bone, self._matrix(center, rot, size), smooth, bevel, emissive, outline)

    def sphere(self, radius, center, color, bone="root", scale=(1, 1, 1), rot=(0, 0, 0), segs=14, rings=9,
               emissive=False, outline=True):
        tmp = bmesh.new()
        bmesh.ops.create_uvsphere(tmp, u_segments=segs, v_segments=rings, radius=radius)
        self._append(tmp, color, bone, self._matrix(center, rot, scale), True, 0.0, emissive, outline)

    def cylinder(self, r1, r2, depth, center, color, bone="root", rot=(0, 0, 0), segs=14, scale=(1, 1, 1),
                 bevel=0.0, smooth=True, outline=True):
        tmp = bmesh.new()
        bmesh.ops.create_cone(tmp, cap_ends=True, cap_tris=False, segments=segs, radius1=r1, radius2=r2,
                              depth=depth)
        self._append(tmp, color, bone, self._matrix(center, rot, scale), smooth, bevel, False, outline)

    def torus(self, major, minor, center, color, bone="root", rot=(0, 0, 0), scale=(1, 1, 1), outline=True):
        tmp = bmesh.new()
        _torus(tmp, major, minor)
        self._append(tmp, color, bone, self._matrix(center, rot, scale), True, 0.0, False, outline)

    def build(self, collection, outline=0.0, armature=None):
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        obj = bpy.data.objects.new(self.name, me)
        collection.objects.link(obj)
        for m in self.materials:
            me.materials.append(m)
        for g in self.groups:
            obj.vertex_groups.new(name=g)
        if hasattr(me, "set_sharp_from_angle"):
            me.set_sharp_from_angle(angle=math.radians(40))
        if armature is not None:
            obj.parent = armature
            mod = obj.modifiers.new("Armature", "ARMATURE")
            mod.object = armature
        if outline > 0:
            n = len(self.materials)
            for _ in range(n):
                me.materials.append(outline_material())
            sol = obj.modifiers.new("Outline", "SOLIDIFY")
            sol.thickness = outline
            sol.offset = 1.0
            sol.use_flip_normals = True
            sol.use_rim = False
            sol.material_offset = n
            if OUTLINE_GROUP in self.groups:
                sol.vertex_group = OUTLINE_GROUP
                sol.thickness_vertex_group = 0.0
        return obj


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _MAT_CACHE.clear()


def setup_toon_render(scene, sky="sky_blue", ambient=0.35, res=(1920, 1080), samples=32):
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    try:
        scene.eevee.taa_render_samples = samples
    except AttributeError:
        pass
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    world = bpy.data.worlds.new("KG_World")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputWorld")
    lp = nt.nodes.new("ShaderNodeLightPath")
    mix = nt.nodes.new("ShaderNodeMixShader")
    bg_light = nt.nodes.new("ShaderNodeBackground")
    bg_cam = nt.nodes.new("ShaderNodeBackground")
    bg_light.inputs["Color"].default_value = linear_rgba("pastel_blue")
    bg_light.inputs["Strength"].default_value = ambient
    bg_cam.inputs["Color"].default_value = linear_rgba(sky)
    bg_cam.inputs["Strength"].default_value = 1.0
    nt.links.new(lp.outputs["Is Camera Ray"], mix.inputs["Fac"])
    nt.links.new(bg_light.outputs[0], mix.inputs[1])
    nt.links.new(bg_cam.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])


def add_sun(collection, rot_deg=(48, 0, -38), strength=3.2, angle_deg=0.0):
    light = bpy.data.lights.new("KG_Sun", "SUN")
    light.energy = strength
    light.angle = math.radians(angle_deg)
    if hasattr(light, "use_shadow_jitter"):
        light.use_shadow_jitter = False
    light.color = linear_rgba("pastel_yellow")[:3]
    obj = bpy.data.objects.new("KG_Sun", light)
    obj.rotation_euler = Euler([math.radians(a) for a in rot_deg])
    collection.objects.link(obj)
    return obj


def add_camera(collection, location, target, lens=40.0):
    cam = bpy.data.cameras.new("KG_Cam")
    cam.lens = lens
    obj = bpy.data.objects.new("KG_Cam", cam)
    obj.location = Vector(location)
    direction = Vector(target) - Vector(location)
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    collection.objects.link(obj)
    return obj
