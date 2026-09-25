"""Numeric viewmodel check for FPArms2 (no Unreal needed): projects the skinned arms + knife per clip frame.

  blender --background --factory-startup --python Tools/Blender/kg_fp2_metrics.py -- \
      --blend Art/Packed/FPArms2/KG_FPArms2.blend --fov 72 --offset 0,0,0 [--clips knife_idle:0,knife_inspect:0-96/6]

--offset is the viewmodel pivot offset in UE camera space, cm (forward, right, up) - what UKGViewmodelComponent
applies (preset Offset converted) plus any per-state lift. --fov is the first-person (viewmodel) horizontal FOV at 16:9.

Per frame it prints screen coordinates in [-1, 1] (u right, v up; the crosshair is 0,0) for the right hand and the
knife tip/grip, the fraction of the 16:9 frame covered by arms and by the knife (triangle raster, 256x144), the nearest
on-screen depth (UE FirstPersonScale 0.6 x depth must stay > 10 cm near plane, i.e. > 16.7 cm), and per-frame knife
tip speed on screen (jitter check).
"""
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default=None):
    if name in ARGV:
        i = ARGV.index(name)
        return ARGV[i + 1] if i + 1 < len(ARGV) else True
    return default


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLEND = os.path.join(ROOT, arg("--blend", "Art/Packed/FPArms2/KG_FPArms2.blend"))
KNIFE_GLB = os.path.join(ROOT, "Art/Source/PolyPizza_Melee/Hunters_Knife_a2avVUVeYD.glb")
KNIFE_SCALE = 0.6
KNIFE_TIP = 0.215            # m from the grip along weapon_r X (mesh -Y 32.4 + 2.1 cm grip offset, at 0.6)
FOV = float(arg("--fov", 72.0))
OFF_UE = [float(v) for v in str(arg("--offset", "0,0,0")).split(",")]
OFF = np.array([OFF_UE[0], -OFF_UE[1], OFF_UE[2]]) / 100.0     # UE cm (fwd, right, up) -> Blender m (fwd, left, up)
TAN_H = math.tan(math.radians(FOV * 0.5))
TAN_V = TAN_H * 9.0 / 16.0
RW, RH = 256, 144
FP_SCALE, NEAR = 0.6, 0.10
CLIPS = arg("--clips", "knife_idle:0,knife_inspect:0-96/4,fists_idle:0,hands_idle:0,carry_idle:0")


def proj(P):
    """(N,3) camera-space metres -> (N,2) screen [-1,1] (u right, v up)."""
    x = np.maximum(P[:, 0], 1e-4)
    return np.column_stack([-P[:, 1] / x / TAN_H, P[:, 2] / x / TAN_V])


def coverage(P, tris):
    """Fraction of the frame covered by the triangles (pixel centres, 256x144)."""
    S = proj(P)
    front = P[:, 0] > 0.02
    px = (S[:, 0] * 0.5 + 0.5) * RW
    py = (0.5 - S[:, 1] * 0.5) * RH
    img = np.zeros((RH, RW), bool)
    for a, b, c in tris:
        if not (front[a] and front[b] and front[c]):
            continue
        xs, ys = (px[a], px[b], px[c]), (py[a], py[b], py[c])
        x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, RW)
        y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, RH)
        if x0 >= x1 or y0 >= y1:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-9:
            continue
        l1 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        l2 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        inside = (l1 >= 0) & (l2 >= 0) & (l1 + l2 <= 1)
        img[y0:y1, x0:x1] |= inside
    return img


def eval_mesh(ob, dg):
    ev = ob.evaluated_get(dg)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    M = np.array(ob.matrix_world)
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    P = co @ M[:3, :3].T + M[:3, 3]
    tris = np.zeros(len(me.loop_triangles) * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("vertices", tris)
    ev.to_mesh_clear()
    return P, tris.reshape(-1, 3)


def add_knife(rig):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=KNIFE_GLB)
    new = [o for o in bpy.data.objects if o not in before]
    mesh = [o for o in new if o.type == "MESH"][0]
    bpy.context.view_layer.update()
    mesh.data.transform(mesh.matrix_world.copy())
    for o in new:
        if o is not mesh:
            bpy.data.objects.remove(o)
    mesh.parent = None
    mesh.matrix_world = Matrix.Identity(4)
    Kr = np.column_stack([(0, 1, 0), (1, 0, 0), (0, 0, -1)])
    grip = np.array([0.0, -0.035, 0.0]) * KNIFE_SCALE
    Mk = np.eye(4)
    Mk[:3, :3] = Kr * KNIFE_SCALE
    Mk[:3, 3] = -(Kr @ grip)
    return mesh, Mk


def parse_clips(spec):
    out = []
    for part in spec.split(","):
        name, _, fr = part.partition(":")
        if "-" in fr:
            rng, _, step = fr.partition("/")
            a, b = rng.split("-")
            out.append((name, list(range(int(a), int(b) + 1, int(step or 1)))))
        else:
            out.append((name, [int(f) for f in fr.split("+")] if fr else [0]))
    return out


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    arms = next(o for o in bpy.data.objects if o.type == "MESH" and o.find_armature() == rig)
    knife, Mk = add_knife(rig)
    kco = np.array([v.co[:] for v in knife.data.vertices])
    knife.data.calc_loop_triangles()
    ktris = np.array([t.vertices[:] for t in knife.data.loop_triangles])
    sc = bpy.context.scene
    print(f"KG_FP2M config fov {FOV} offset(UE cm fwd,right,up) {OFF_UE} | arms verts {len(arms.data.vertices)}")
    for name, frames in parse_clips(CLIPS):
        act = bpy.data.actions.get(name)
        if not act:
            print(f"KG_FP2M {name}: no action")
            continue
        rig.animation_data.action = act
        prev = None
        for fr in frames:
            sc.frame_set(fr)
            dg = bpy.context.evaluated_depsgraph_get()
            P, tris = eval_mesh(arms, dg)
            P = P + OFF
            wr = np.array(rig.matrix_world) @ np.array(rig.pose.bones["weapon_r"].matrix)
            hr = np.array(rig.matrix_world) @ np.array(rig.pose.bones["hand_r"].matrix)
            KP = (kco @ (wr @ Mk)[:3, :3].T + (wr @ Mk)[:3, 3]) + OFF
            grip = wr[:3, 3] + OFF
            tip = grip + wr[:3, 0] * KNIFE_TIP
            palm = hr[:3, 3] + hr[:3, 1] * 0.05 + OFF          # middle of the palm (bone Y = distal)
            s = proj(np.array([palm, grip, tip]))
            ia = coverage(P, tris)
            knife_on = name.startswith("knife")
            ik = coverage(KP, ktris) if knife_on else np.zeros_like(ia)
            allp = np.vstack([P, KP]) if knife_on else P
            S = proj(allp)
            onscr = (np.abs(S[:, 0]) <= 1) & (np.abs(S[:, 1]) <= 1) & (allp[:, 0] > 0)
            near = allp[onscr, 0].min() * 100 if onscr.any() else float("nan")
            spd = float(np.linalg.norm(s[2] - prev)) if prev is not None else 0.0
            prev = s[2]
            ys, xs = np.nonzero(ia | ik)
            bbox = ((xs.max() - xs.min() + 1) * (ys.max() - ys.min() + 1)) / (RW * RH) * 100 if len(xs) else 0.0
            print(f"KG_FP2M {name} f{fr:03d} palm({s[0][0]:+.2f},{s[0][1]:+.2f}) grip({s[1][0]:+.2f},{s[1][1]:+.2f}) "
                  f"tip({s[2][0]:+.2f},{s[2][1]:+.2f}) tipdepth {tip[0]*100:.1f}cm arms {ia.mean()*100:.1f}% "
                  f"knife {ik.mean()*100:.1f}% total {(ia | ik).mean()*100:.1f}% bbox {bbox:.1f}% near {near:.1f}cm"
                  f"{' CLIP' if near * FP_SCALE < NEAR * 100 else ''} tipstep {spd:.3f}")


main()
