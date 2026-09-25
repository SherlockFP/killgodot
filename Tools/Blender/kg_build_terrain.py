"""Morrowmere terrain: a sheltered cove. Village plateau at the origin, beach and sea to the south (-Y), a ring of
green hills and rocky mountains around the rest. Vertex-coloured (toon palette), no textures.

  blender --background --factory-startup --python Tools/Blender/kg_build_terrain.py -- Art/Packed/KG_Terrain.glb

Coordinates in metres (Blender): +X east, +Y north. Sea level is Z=0. The plaza sits at ~Z=4.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector, noise

out = sys.argv[sys.argv.index("--") + 1]
SIZE = 640.0      # metres per side
STEP = 2.5        # grid resolution
N = int(SIZE / STEP)

PLATEAU = 4.0
LAYOUT = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Level", "morrowmere_layout.json")))
# Layout is in the UE frame (metres, +y = sea); Blender uses (x, -y).
LANES = [(l["width"], [(p[0], -p[1]) for p in l["points"]]) for l in LAYOUT["lanes"]]
AREAS = [((a["center"][0], -a["center"][1]), a["radius"]) for a in LAYOUT["areas"]]
PADS = [((h["at"][0], -h["at"][1]), max(h["size"]) * 0.5 + 2.0) for h in LAYOUT["houses"]]
for name, lm in LAYOUT["landmarks"].items():
    PADS.append(((lm["at"][0], -lm["at"][1]), (max(lm["size"]) * 0.5 + 2.5) if "size" in lm else 5.0))
HILLS = [((70.0, -57.0), 22.0, 9.0), ((-28.0, 48.0), 20.0, 2.5)]   # lighthouse headland, church knoll
COBBLE = (0.58, 0.55, 0.50)
SAND = (0.93, 0.80, 0.52)
WET_SAND = (0.70, 0.60, 0.42)
GRASS_A = (0.33, 0.62, 0.22)
GRASS_B = (0.46, 0.70, 0.26)
PATH = (0.62, 0.47, 0.30)
ROCK = (0.52, 0.49, 0.46)
ROCK_DARK = (0.38, 0.36, 0.35)


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def lerp3(a, b, t):
    return tuple(lerp(a[i], b[i], t) for i in range(3))


def fbm(x, y, scale, octaves=4):
    return noise.fractal(Vector((x / scale, y / scale, 0.37)), 0.5, 2.0, octaves)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def lane_factor(x, y):
    """1 on a lane/area, fading to 0 over 1.5 m at the edge."""
    best = 0.0
    for width, pts in LANES:
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            d = seg_dist(x, y, ax, ay, bx, by)
            best = max(best, 1.0 - smooth(width * 0.5, width * 0.5 + 1.5, d))
    return best


def area_factor(x, y):
    best = 0.0
    for (cx, cy), r in AREAS:
        best = max(best, 1.0 - smooth(r, r + 2.0, math.hypot(x - cx, y - cy)))
    return best


def on_street(x, y):
    return lane_factor(x, y) > 0.5 or area_factor(x, y) > 0.5


def height(x, y):
    r = math.hypot(x, y)
    h = PLATEAU + 0.5 * fbm(x, y, 40.0)
    # Rolling meadow outside the village core.
    h += smooth(45, 110, r) * (3.0 + 5.0 * fbm(x + 300, y, 60.0))
    # Mountains around, open to the south except for two rocky headlands.
    open_south = smooth(50, 110, -y) * (1.0 - smooth(90, 160, abs(x)))
    ring = smooth(120, 210, r) * (1.0 - open_south)
    h += ring * (38.0 + 34.0 * fbm(x, y + 500, 90.0, 5))
    # Beach and sea floor to the south.
    coast = smooth(-34.0, -70.0, y) * (1.0 - ring)
    h = lerp(h, -2.5 - 10.0 * smooth(-70, -200, y), coast)
    for (cx, cy), r, hh in HILLS:
        h += hh * (1.0 - smooth(r * 0.4, r, math.hypot(x - cx, y - cy)))
    # Inland lanes and yards are smoothed towards the plateau (not on the beach).
    inland = 1.0 - smooth(-26.0, -40.0, y)
    h = lerp(h, PLATEAU + 0.25 * (h - PLATEAU), 0.7 * max(lane_factor(x, y), area_factor(x, y)) * inland)
    return h


def pad_height(x, y):
    """Buildings get flat pads at the (pre-pad) terrain height of their centre."""
    h = height(x, y)
    for (cx, cy), r in PADS:
        k = 1.0 - smooth(r, r + 3.0, math.hypot(x - cx, y - cy))
        if k > 0:
            h = lerp(h, height(cx, cy), k)
    return h


verts, faces, heights = [], [], []
half = SIZE / 2
for j in range(N + 1):
    for i in range(N + 1):
        x = -half + i * STEP
        y = -half + j * STEP
        z = pad_height(x, y)
        verts.append((x, y, z))
        heights.append(z)
for j in range(N):
    for i in range(N):
        a = j * (N + 1) + i
        faces.append((a, a + 1, a + N + 2, a + N + 1))

mesh = bpy.data.meshes.new("SM_KG_Terrain")
mesh.from_pydata(verts, [], faces)
mesh.update()
obj = bpy.data.objects.new("SM_KG_Terrain", mesh)
bpy.context.scene.collection.objects.link(obj)

col = mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
for idx, (x, y, z) in enumerate(verts):
    # Slope from the neighbouring grid heights.
    i, j = idx % (N + 1), idx // (N + 1)
    zx = heights[j * (N + 1) + min(i + 1, N)] - heights[j * (N + 1) + max(i - 1, 0)]
    zy = heights[min(j + 1, N) * (N + 1) + i] - heights[max(j - 1, 0) * (N + 1) + i]
    slope = math.hypot(zx, zy) / (2 * STEP)
    g = 0.5 + 0.5 * fbm(x + 90, y - 40, 18.0)
    c = lerp3(GRASS_A, GRASS_B, g)
    if z < 1.4:
        c = lerp3(WET_SAND, SAND, smooth(-1.0, 1.0, z))
    elif z < 2.6 and y < -20:
        c = lerp3(SAND, c, smooth(1.4, 2.6, z))
    lf, af = lane_factor(x, y), area_factor(x, y)
    if z > 1.6:
        c = lerp3(c, PATH, 0.85 * lf)
        c = lerp3(c, COBBLE if math.hypot(x, y - 8.0) < 14.0 else PATH, 0.8 * af)
    rock = smooth(0.55, 0.9, slope) + smooth(40.0, 60.0, z)
    if rock > 0:
        c = lerp3(c, lerp3(ROCK, ROCK_DARK, g), min(1.0, rock))
    col.data[idx].color = (c[0], c[1], c[2], 1.0)
mesh.color_attributes.active_color = col

mat = bpy.data.materials.new("M_KG_TerrainVC")
mat.use_nodes = True
nt = mat.node_tree
attr = nt.nodes.new("ShaderNodeVertexColor")
attr.layer_name = "Col"
nt.links.new(attr.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
obj.data.materials.append(mat)

for p in mesh.polygons:
    p.use_smooth = True

# Heightmap for the Unreal village builder (UE: x_cm = X*100, y_cm = -Y*100).
import json
with open(out.replace(".glb", "_heights.json"), "w") as f:
    json.dump({"size": SIZE, "step": STEP, "n": N, "heights": [round(h, 3) for h in heights]}, f)

bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_vertex_color="ACTIVE", export_normals=True)
print(f"KG_TERRAIN: {len(verts)} verts, height {min(heights):.1f}..{max(heights):.1f} m -> {out}")
