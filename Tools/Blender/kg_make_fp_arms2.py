"""First-person arms v2: stylised villager arms (cream linen sleeves, warm-tan hands) + CS2-style viewmodel clips.

  blender --background --factory-startup --python Tools/Blender/kg_make_fp_arms2.py -- \
      --packed Art/Packed/FPArms2 --concept Art/Concept/FPArms2 [--stage rest|poses|all] [--only clip,clip] [--no-export]

Everything is authored in CAMERA SPACE (metres): the eye is the `camera` bone at the rig origin, the view looks along
+X, +Z is up, +Y is left. Blender -> glTF -> UE Interchange maps (x, y, z) -> (x, -y, z) * 100 cm, so the rig can be
attached to the UE first-person camera with an identity transform (camera bone == rig root == eye).

Bone local axes are chosen so that, after that conversion, UE bone axes are meaningful where it matters:
UE local X = Blender local X, UE local Y = Blender local Z, UE local Z = Blender local Y (the bone direction).
  camera / root : UE X = forward, UE Z = up (identity in UE).
  weapon_r/_l   : UE X = blade direction (tip), UE Z = blade spine (up in the idle grip), origin = handle grip point.

Mesh: signed-distance-field modelling (tapered round cones, ellipsoids, rounded boxes, smooth-min blends) -> surface
nets -> smoothing -> quadric decimation -> SDF-gradient normals. Skin weights come from the same primitives (nearest
bone chain + soft blend + topological smoothing); forearm roll is spread over two twist bones.
Vertex colour "Col" holds sRGB (the UE material squares it); alpha = roughness.
Clips are baked every frame from camera-space targets (2-bone IK, knife-driven hand placement, grip wrap solver).
One clip per glb (Blender 5 merges slotted actions otherwise).
"""
import math
import os
import sys
import time

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

T0 = time.time()
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default=None):
    if name in ARGV:
        i = ARGV.index(name)
        return ARGV[i + 1] if i + 1 < len(ARGV) and not ARGV[i + 1].startswith("--") else True
    return default


ROOT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PACKED = os.path.join(ROOT_DIR, arg("--packed", "Art/Packed/FPArms2"))
CONCEPT = os.path.join(ROOT_DIR, arg("--concept", "Art/Concept/FPArms2"))
STAGE = arg("--stage", "all")
ONLY = [c for c in str(arg("--only", "")).split(",") if c]
DO_EXPORT = not arg("--no-export", False)
KNIFE_GLB = os.path.join(ROOT_DIR, "Art/Source/PolyPizza_Melee/Hunters_Knife_a2avVUVeYD.glb")
KNIFE_SCALE = 0.6
# Fishing rod (SM_KG_FishingRod: 1.9 m along +X, butt at X=0, reel hanging to -Z): KEEP IN SYNC with the
# SM_KG_FishingRod grip in Source/KillGodot/Character/KGCharacter.cpp (tip +X, spine +Z, grip (20,0,0) cm, scale 0.55).
WATER_PROPS_GLB = os.path.join(ROOT_DIR, "Art/Packed/KG_WaterProps.glb")
ROD_SCALE = 0.55
ROD_GRIP = 0.20          # m along the rod (the cork)
ROD_LEN = 1.9
FPS = 30
FOV_DEG = 72.0


def log(*a):
    print(f"KG_ARMS2 [{time.time() - T0:6.1f}s]", *a, flush=True)


# ================================================================================================================
# small linear algebra
# ================================================================================================================
def nrm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def rot(axis, deg):
    a = nrm(axis)
    t = math.radians(deg)
    c, s = math.cos(t), math.sin(t)
    x, y, z = a
    return np.array([[c + x * x * (1 - c), x * y * (1 - c) - z * s, x * z * (1 - c) + y * s],
                     [y * x * (1 - c) + z * s, c + y * y * (1 - c), y * z * (1 - c) - x * s],
                     [z * x * (1 - c) - y * s, z * y * (1 - c) + x * s, c + z * z * (1 - c)]])


def RX(d):
    return rot((1, 0, 0), d)


def RY(d):
    return rot((0, 1, 0), d)


def RZ(d):
    return rot((0, 0, 1), d)


def m4(R, t):
    M = np.eye(4)
    M[:3, :3] = R
    M[:3, 3] = t
    return M


def inv4(M):
    R = M[:3, :3]
    t = M[:3, 3]
    return m4(R.T, -R.T @ t)


def frame_yz(y, zhint):
    """Right-handed frame (columns X, Y, Z) with Y along y and Z as close as possible to zhint."""
    y = nrm(y)
    x = nrm(np.cross(y, zhint))
    z = np.cross(x, y)
    return np.column_stack([x, y, z])


def frame_xy(x, yhint):
    x = nrm(x)
    z = nrm(np.cross(x, yhint))
    y = np.cross(z, x)
    return np.column_stack([x, y, z])


def quat_from_R(R):
    return np.array(Matrix(R.tolist()).to_quaternion())  # w, x, y, z


def R_from_quat(q):
    from mathutils import Quaternion
    return np.array(Quaternion(q).to_matrix())


def slerp(q0, q1, t):
    q0 = np.asarray(q0, float)
    q1 = np.asarray(q1, float)
    d = float(np.dot(q0, q1))
    if d < 0:
        q1, d = -q1, -d
    if d > 0.9995:
        q = q0 + t * (q1 - q0)
        return q / np.linalg.norm(q)
    th = math.acos(min(1.0, d))
    return (math.sin((1 - t) * th) * q0 + math.sin(t * th) * q1) / math.sin(th)


MIRROR = np.diag([1.0, -1.0, 1.0, 1.0])     # camera-space mirror (right <-> left)
FLIPX = np.diag([-1.0, 1.0, 1.0, 1.0])      # keep bone frames right-handed after mirroring (Blender X-mirror)


def mirror_m4(M):
    return MIRROR @ M @ FLIPX


# ================================================================================================================
# skeleton (right side authored, left = mirror)
# ================================================================================================================
SHOULDER = np.array([-0.10, -0.19, -0.27])
L_UP, L_FORE = 0.29, 0.26
HAND_LEN = 0.093
ELBOW = SHOULDER + np.array([L_UP, 0, 0])
WRIST = ELBOW + np.array([L_FORE, 0, 0])
R_ARM = np.column_stack([(0, 1, 0), (1, 0, 0), (0, 0, -1)])        # X = elbow hinge, Y = along the arm
HAND_REST_ROLL = 100.0                                              # bind pose: thumb up, slightly supinated (mid twist range)
R_PALMDOWN = np.column_stack([(0, -1, 0), (1, 0, 0), (0, 0, 1)])    # hand: X ulnar, Y distal, Z dorsal
R_HAND_REST = RX(HAND_REST_ROLL) @ R_PALMDOWN
TWISTS = (("lowerarm_twist_01", 0.5, 0.5), ("lowerarm_twist_02", 0.85, 1.0))   # name, position t, roll share

# fingers in hand-local coords (x ulnar, y distal, z dorsal); spread + = toward the thumb
FINGERS = {
    #          MCP                      lengths (prox, mid, dist)   radii (mcp, pip, dip, tip)          spread  rest curl
    "index":  ((-0.0335, 0.0870, 0.000), (0.044, 0.026, 0.0175), (0.0099, 0.0090, 0.0083, 0.0076), 8.0, (8, 12, 6)),
    "middle": ((-0.0110, 0.0925, 0.001), (0.048, 0.030, 0.0185), (0.0103, 0.0094, 0.0087, 0.0079), 1.5, (8, 12, 6)),
    "ring":   ((0.0115, 0.0885, 0.000), (0.045, 0.028, 0.0175), (0.0097, 0.0089, 0.0082, 0.0075), -6.0, (9, 13, 6)),
    "pinky":  ((0.0325, 0.0780, -0.003), (0.036, 0.0215, 0.0155), (0.0086, 0.0079, 0.0073, 0.0067), -14.0, (10, 14, 7)),
}
THUMB_CMC = np.array([-0.0215, 0.0235, -0.0095])
THUMB_DIR = nrm((-0.56, 0.74, -0.37))
THUMB_NAIL = nrm((-0.62, 0.08, 0.78))
THUMB_LEN = (0.043, 0.0325, 0.0215)
THUMB_R = (0.0140, 0.0121, 0.0108, 0.0098)
THUMB_REST_CURL = (0.0, 10.0, 8.0)
FINGER_NAMES = ("thumb", "index", "middle", "ring", "pinky")

# knife grip: handle runs diagonally across the palm (index MCP -> hypothenar heel), blade exits past the index knuckle
GRIP_POINT = np.array([-0.0040, 0.0610, -0.0325])
GRIP_BLADE = nrm((-0.68, 0.72, 0.10))          # saber grip: ~47 deg diagonal from the heel of the hand to the index
GRIP_SPINE = nrm((-0.72, -0.68, 0.0))           # spine faces the thumb (which rests on it)
HANDLE_R = 0.0142          # knife handle radius at KNIFE_SCALE (the grip hole the fingers wrap)


def grip_frame():
    x = GRIP_BLADE
    y = nrm(GRIP_SPINE - np.dot(GRIP_SPINE, x) * x)
    return np.column_stack([x, y, np.cross(x, y)])


def build_rest():
    """World (camera-space) rest matrices for the right side + centre bones. Returns {name: (M4, parent, length)}."""
    B = {}
    Rup = np.column_stack([(1, 0, 0), (0, 0, 1), (0, -1, 0)])   # X fwd, Y(bone) up -> UE: X fwd, Z up
    B["root"] = (m4(Rup, (0, 0, 0)), None, 0.12)
    B["camera"] = (m4(Rup, (0, 0, 0)), "root", 0.08)
    B["upperarm_r"] = (m4(R_ARM, SHOULDER), "root", L_UP)
    B["lowerarm_r"] = (m4(R_ARM, ELBOW), "upperarm_r", L_FORE)
    for name, t, _ in TWISTS:
        B[name + "_r"] = (m4(R_ARM, ELBOW + np.array([L_FORE * t, 0, 0])), "lowerarm_r", 0.05)
    H = m4(R_HAND_REST, WRIST)
    B["hand_r"] = (H, "lowerarm_r", HAND_LEN)
    for f, (mcp, lens, radii, spread, curl) in FINGERS.items():
        F = RZ(spread)
        p = np.array(mcp, float)
        parent = "hand_r"
        for j in range(3):
            F = F @ RX(-curl[j])
            name = f"{f}_0{j + 1}_r"
            B[name] = (H @ m4(F, p), parent, lens[j])
            p = p + F[:, 1] * lens[j]
            parent = name
    F = frame_yz(THUMB_DIR, THUMB_NAIL)
    p = THUMB_CMC.copy()
    parent = "hand_r"
    for j in range(3):
        F = F @ RX(-THUMB_REST_CURL[j])
        name = f"thumb_0{j + 1}_r"
        B[name] = (H @ m4(F, p), parent, THUMB_LEN[j])
        p = p + F[:, 1] * THUMB_LEN[j]
        parent = name
    B["weapon_r"] = (H @ m4(grip_frame(), GRIP_POINT), "hand_r", 0.05)
    # left side
    for name in [n for n in B if n.endswith("_r")]:
        M, parent, L = B[name]
        lp = parent[:-2] + "_l" if parent and parent.endswith("_r") else parent
        B[name[:-2] + "_l"] = (mirror_m4(M), lp, L)
    return B


REST = build_rest()
BONE_ORDER = list(REST.keys())


def rest_rel(name):
    M, parent, _ = REST[name]
    return M if parent is None else inv4(REST[parent][0]) @ M


# ================================================================================================================
# hand shapes (finger curls) + pose solver
# ================================================================================================================
def finger_chain(Hw, side_rest_hand, f, curls, spread=0.0, extra_rot=None):
    """World matrices of a finger's 3 bones for hand world matrix Hw (right-side conventions)."""
    out = []
    Tp = Hw
    for j in range(3):
        name = f"{f}_0{j + 1}_r"
        rest_c = FINGERS[f][4][j] if f != "thumb" else THUMB_REST_CURL[j]
        basis = np.eye(4)
        Rb = RX(-(curls[j] - rest_c))
        if j == 0:
            Rb = RZ(spread) @ Rb
            if extra_rot is not None:
                Rb = extra_rot @ Rb
        basis[:3, :3] = Rb
        Tj = Tp @ rest_rel(name) @ basis
        out.append((name, Tj))
        Tp = Tj
    return out


def thumb_chain(Hw, th):
    """th = (flex1, abduct1, roll1, curl2, curl3) relative deltas for thumb_01 + absolute curls for 02/03."""
    flex1, abd1, roll1, c2, c3 = th
    R1 = RZ(abd1) @ RX(-flex1) @ RY(roll1)
    return finger_chain(Hw, None, "thumb", (THUMB_REST_CURL[0], c2, c3), 0.0, extra_rot=R1)


def seg_capsule_dist(p, a, b):
    ab = b - a
    t = np.clip(np.dot(p - a, ab) / np.dot(ab, ab), 0, 1)
    return np.linalg.norm(p - (a + t * ab))


def seg_line_dist(a, b, c, d, n=6):
    """Min distance between segment ab and infinite line (c, dir d) by sampling the segment."""
    best = 1e9
    for i in range(n + 1):
        p = a + (b - a) * (i / n)
        v = p - c
        best = min(best, np.linalg.norm(v - np.dot(v, d) * d))
    return best


def tip_of(T, name):
    return T[:3, 3] + T[:3, 1] * REST[name][2]


def solve_grip_curls(margin=0.0005):
    """Close each finger around the knife handle (a cylinder along weapon_r X through the grip point): all joints
    flex together; when a segment hits the handle, it and every joint proximal to it stop (like a real hand closing)."""
    Hw = REST["hand_r"][0]
    Wk = REST["weapon_r"][0]
    c, d = Wk[:3, 3], Wk[:3, 0]
    res = {}
    for f in ("index", "middle", "ring", "pinky"):
        radii = FINGERS[f][2]
        limits = (96.0, 110.0, 88.0)
        rates = (1.0, 1.15, 0.8)
        curls = [0.0, 0.0, 0.0]
        active = [True, True, True]

        def hits(cs):
            ch = finger_chain(Hw, None, f, cs, 0.0)
            out = []
            for j in range(3):
                name, T = ch[j]
                r = 0.5 * (radii[j] + radii[j + 1])
                out.append(seg_line_dist(T[:3, 3], tip_of(T, name), c, d) < HANDLE_R + r + margin)
            return out
        for _ in range(400):
            if not any(active):
                break
            prev = list(curls)
            for j in range(3):
                if active[j]:
                    curls[j] = min(limits[j], curls[j] + 0.5 * rates[j])
                    if curls[j] >= limits[j]:
                        active[j] = False
            h = hits(curls)
            for j in range(2, -1, -1):
                if h[j]:
                    for i in range(j + 1):
                        if active[i] or curls[i] != prev[i]:
                            curls[i] = prev[i]
                            active[i] = False
                    break
        res[f] = tuple(round(v, 1) for v in curls)
    return res


def solve_thumb_to(Hw_fingers_fn, target_fn, init=(20, 0, 0, 20, 20), avoid=None):
    """Pattern search on thumb params so the thumb pad lands on target (world point), avoiding a handle cylinder."""
    Hw = REST["hand_r"][0]
    fing = Hw_fingers_fn(Hw)
    target = target_fn(fing)

    def cost(th):
        ch = thumb_chain(Hw, th)
        name, T = ch[2]
        pad = tip_of(T, name) - T[:3, 2] * THUMB_R[3] * 0.4      # pad = palmar side of the thumb tip
        e = np.linalg.norm(pad - target) ** 2 * 1e4
        if avoid is not None:
            c, d, r = avoid
            for j, (nm, TT) in enumerate(ch):
                dist = seg_line_dist(TT[:3, 3], tip_of(TT, nm), c, d)
                pen = r + THUMB_R[j + 1] - dist
                if pen > 0:
                    e += (pen * 1000) ** 2
        # stay natural: penalise extreme angles
        e += 1e-5 * (th[0] ** 2 + th[1] ** 2 + th[2] ** 2) + 1e-6 * (th[3] ** 2 + th[4] ** 2)
        e += 1e-3 * max(0, abs(th[3]) - 70) ** 2 + 1e-3 * max(0, abs(th[4]) - 85) ** 2
        return e
    starts = [init, (20, 0, 0, 20, 20), (40, 20, 30, 30, 30), (30, -20, -30, 40, 30), (55, 5, 40, 20, 40),
              (45, 25, -20, 45, 50), (15, -10, 20, 50, 60)]
    best_x, best_c = None, 1e18
    for st in starts:
        x = np.array(st, float)
        best = cost(x)
        step = 16.0
        while step > 0.25:
            improved = False
            for i in range(5):
                for s in (step, -step):
                    y = x.copy()
                    y[i] += s
                    cy = cost(y)
                    if cy < best:
                        x, best, improved = y, cy, True
            if not improved:
                step *= 0.5
        if best < best_c:
            best_x, best_c = x, best
    return tuple(float(v) for v in best_x), math.sqrt(best_c / 1e4)


HAND_SHAPES = {}


def build_hand_shapes():
    Hw = REST["hand_r"][0]
    grip = solve_grip_curls()
    Wk = REST["weapon_r"][0]
    handle = (Wk[:3, 3], Wk[:3, 0], HANDLE_R)

    def fingers_of(curls):
        def fn(H):
            out = {}
            for f in ("index", "middle", "ring", "pinky"):
                for name, T in finger_chain(H, None, f, curls[f], 0.0):
                    out[name] = T
            return out
        return fn

    def on_spine(fing):
        # saber grip: the thumb pad rests on the handle's spine side, just behind the guard
        return Wk[:3, 3] + Wk[:3, 0] * 0.026 + Wk[:3, 1] * (HANDLE_R + THUMB_R[3] * 0.75)

    th_grip, err_g = solve_thumb_to(fingers_of(grip), on_spine, init=(25, 10, 20, 25, 20), avoid=handle)
    fist = {"index": (88, 100, 62), "middle": (92, 102, 62), "ring": (95, 102, 60), "pinky": (98, 100, 58)}

    def fist_target(fing):
        a = fing["index_02_r"]
        b = fing["middle_02_r"]
        p = 0.6 * (a[:3, 3] + a[:3, 1] * 0.5 * REST["index_02_r"][2]) + 0.4 * (b[:3, 3] + b[:3, 1] * 0.5 * REST["middle_02_r"][2])
        return p + a[:3, 2] * (FINGERS["index"][2][1] + THUMB_R[3] * 0.85)
    th_fist, err_f = solve_thumb_to(fingers_of(fist), fist_target, init=(30, 10, 25, 30, 20))
    log(f"grip curls {grip} thumb {tuple(round(v, 1) for v in th_grip)} err {err_g * 1000:.1f}mm; "
        f"fist thumb {tuple(round(v, 1) for v in th_fist)} err {err_f * 1000:.1f}mm")
    HAND_SHAPES["grip"] = dict(fingers=grip, spread={}, thumb=th_grip)
    HAND_SHAPES["fist"] = dict(fingers=fist, spread={}, thumb=th_fist)
    HAND_SHAPES["open"] = dict(fingers={"index": (6, 10, 6), "middle": (7, 11, 6), "ring": (9, 13, 7), "pinky": (12, 15, 8)},
                               spread={"index": 9, "middle": 2, "ring": -7, "pinky": -16}, thumb=(-14, 22, -10, 6, 8))
    HAND_SHAPES["relax"] = dict(fingers={"index": (14, 22, 10), "middle": (19, 28, 12), "ring": (25, 34, 15), "pinky": (31, 40, 18)},
                                spread={"index": 1, "middle": 0, "ring": -1, "pinky": -2}, thumb=(8, 4, 10, 16, 14))
    HAND_SHAPES["loose"] = dict(fingers={k: (v[0] * 0.55, v[1] * 0.5, v[2] * 0.5) for k, v in grip.items()},
                                spread={}, thumb=(th_grip[0] * 0.5, th_grip[1], th_grip[2] * 0.5, th_grip[3] * 0.4, th_grip[4] * 0.4))
    HAND_SHAPES["carry"] = dict(fingers={"index": (22, 24, 12), "middle": (24, 26, 12), "ring": (26, 28, 13), "pinky": (28, 30, 14)},
                                spread={"index": 3, "middle": 0, "ring": -2, "pinky": -5}, thumb=(-4, 10, 5, 12, 10))
    HAND_SHAPES["grab"] = dict(fingers={"index": (62, 78, 45), "middle": (66, 80, 46), "ring": (70, 82, 46), "pinky": (74, 82, 45)},
                               spread={}, thumb=(th_fist[0] * 0.8, th_fist[1], th_fist[2] * 0.8, th_fist[3] * 0.7, th_fist[4] * 0.7))
    # emote shapes -------------------------------------------------------------------------------------------------
    # flat: open hand with the fingers together and the thumb along the index (salute / clap)
    HAND_SHAPES["flat"] = dict(fingers={"index": (4, 6, 3), "middle": (4, 6, 3), "ring": (5, 7, 4), "pinky": (6, 9, 5)},
                               spread={"index": -5, "middle": -1, "ring": 4, "pinky": 9}, thumb=(-18, -20, 0, 6, 6))
    # point: index nearly straight, middle/ring/pinky curled like the fist, thumb tucked over the middle finger
    point = {"index": (4, 6, 3), "middle": fist["middle"], "ring": fist["ring"], "pinky": fist["pinky"]}

    def point_target(fing):
        b = fing["middle_02_r"]
        p = b[:3, 3] + b[:3, 1] * 0.5 * REST["middle_02_r"][2]
        return p + b[:3, 2] * (FINGERS["middle"][2][1] + THUMB_R[3] * 0.85)
    th_point, err_p = solve_thumb_to(fingers_of(point), point_target, init=th_fist)
    log(f"point thumb {tuple(round(v, 1) for v in th_point)} err {err_p * 1000:.1f}mm")
    HAND_SHAPES["point"] = dict(fingers=point, spread={"index": -2}, thumb=th_point)


def shape_params(key):
    """Finger curls / spreads / thumb params of an ArmKey (a shape blend, or a mix of two keys' shapes)."""
    if hasattr(key, "_mix"):
        s0, s1, t = key._mix
        f0, sp0, th0 = blend_shape(*s0)
        f1, sp1, th1 = blend_shape(*s1)
        fingers = {f: tuple(np.array(f0[f]) + (np.array(f1[f]) - np.array(f0[f])) * t) for f in f0}
        spread = {f: sp0.get(f, 0) + (sp1.get(f, 0) - sp0.get(f, 0)) * t for f in f0}
        return fingers, spread, tuple(np.array(th0) + (np.array(th1) - np.array(th0)) * t)
    return blend_shape(*key.shape)


def blend_shape(a, b, w):
    A, Bs = HAND_SHAPES[a], HAND_SHAPES[b]
    fingers, spread = {}, {}
    for f in ("index", "middle", "ring", "pinky"):
        fa, fb = np.array(A["fingers"][f]), np.array(Bs["fingers"][f])
        fingers[f] = tuple(fa + (fb - fa) * w)
        sa, sb = A["spread"].get(f, 0.0), Bs["spread"].get(f, 0.0)
        spread[f] = sa + (sb - sa) * w
    ta, tb = np.array(A["thumb"]), np.array(Bs["thumb"])
    return fingers, spread, tuple(ta + (tb - ta) * w)


class ArmKey:
    """One keyframe for one arm, animator-style FK (right-arm conventions; the left arm is solved mirrored).
    W: wrist position (camera space); pole: elbow direction hint; shoulder: shoulder offset.
    roll: forearm roll referenced to world up (0 = palm down, 90 = thumb up / handshake, 180 = palm up);
    flex: palmar wrist flexion (+) / extension (-); dev: ulnar (+) / radial (-) deviation.
    shape: (hand shape a, hand shape b, weight); wspin/woff: weapon_r spin (deg about its Z) / offset (m);
    wroll: weapon_r roll (deg) about its own X = the blade axis (finger flips that never swing the tip at the eye)."""
    NUM = ("W", "pole", "shoulder", "roll", "flex", "dev", "wspin", "woff", "wroll")

    def __init__(self, W, roll=90.0, flex=0.0, dev=0.0, pole=(-0.25, -0.55, -1.0), shoulder=(0, 0, 0),
                 shape=("grip", "grip", 0.0), wspin=0.0, woff=(0, 0, 0), wroll=0.0):
        self.W = np.array(W, float)
        self.roll, self.flex, self.dev = float(roll), float(flex), float(dev)
        self.pole = np.array(pole, float)
        self.shoulder = np.array(shoulder, float)
        self.shape = shape
        self.wspin = float(wspin)
        self.woff = np.array(woff, float)
        self.wroll = float(wroll)

    def but(self, dW=(0, 0, 0), droll=0.0, dflex=0.0, ddev=0.0, **kw):
        k = ArmKey(self.W + np.array(dW, float), self.roll + droll, self.flex + dflex, self.dev + ddev, self.pole,
                   self.shoulder, self.shape, self.wspin, self.woff, self.wroll)
        for n, v in kw.items():
            setattr(k, n, np.array(v, float) if n in ("W", "pole", "shoulder", "woff") else v)
        return k


def weapon_rel():
    return rest_rel("weapon_r")


A_PD = R_ARM.T @ R_PALMDOWN          # palm-down hand relative to the lowerarm hinge frame (rest relation)


def ik(key):
    S = SHOULDER + key.shoulder
    W = key.W
    d = W - S
    dist = float(np.linalg.norm(d))
    reach = (L_UP + L_FORE) * 0.9985
    pulled = 0.0
    if dist > reach:                       # protract the shoulder rather than detach the hand
        pulled = dist - reach
        S = S + d / dist * pulled
        d = W - S
        dist = reach
    u = d / dist
    v = nrm(key.pole - np.dot(key.pole, u) * u)
    a = (L_UP ** 2 - L_FORE ** 2 + dist ** 2) / (2 * dist)
    h = math.sqrt(max(L_UP ** 2 - a * a, 0.0))
    E = S + a * u + h * v
    n = nrm(np.cross(u, v))
    yu, yl = nrm(E - S), nrm(W - E)
    return S, E, np.column_stack([n, yu, np.cross(n, yu)]), np.column_stack([n, yl, np.cross(n, yl)]), pulled


def hand_rot(yl, roll, flex, dev):
    hint = np.array([0, 0, 1.0]) if abs(yl[2]) < 0.93 else np.array([-1.0, 0, 0])
    return frame_yz(yl, hint) @ RY(roll) @ RZ(-dev) @ RX(-flex)


def blade_of(key):
    _, _, _, Rl, _ = ik(key)
    H = m4(hand_rot(Rl[:, 1], key.roll, key.flex, key.dev), key.W)
    Wm = H @ weapon_rel()
    return Wm[:3, 0], Wm[:3, 1]


def twist_angle(Q):
    q = quat_from_R(Q)
    return math.degrees(2 * math.atan2(q[2], q[0]))


TAN_H = math.tan(math.radians(FOV_DEG * 0.5))
TAN_V = TAN_H * 9.0 / 16.0
POLES = ((-0.35, -0.85, -0.2), (-0.2, -0.6, -0.8), (-0.5, -0.5, -0.7), (-0.1, -0.95, 0.2), (-0.6, -0.8, 0.0),
         (0.15, -0.7, -0.7), (-0.2, -0.3, -0.95))
KNUCKLE = m4(np.eye(3), (-0.004, 0.090, 0.004)) @ m4(np.column_stack([(0, 1, 0), (0, 0, 1), (1, 0, 0)]), (0, 0, 0))


def screen_pt(sx, sy, depth):
    return depth * np.array([1.0, -sx * TAN_H, sy * TAN_V])


def dir_of(yaw, pitch):
    y, p = math.radians(yaw), math.radians(pitch)
    return np.array([math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p)])


def place(local, sx, sy, depth, yaw, pitch, roll_about=0.0, poles=POLES, dev_pref=15.0, flex_pref=0.0, w_dir=40.0,
          w_roll=6.0, verbose=False, **kw):
    """Arm key that puts `local` (a frame in hand coords: X = aim axis, Y = 'up' axis) at screen (sx, sy) / depth,
    with X along (yaw, pitch) and Y rolled `roll_about` deg (left-positive) from world up about X. The most natural
    arm wins: forearm twist inside its range, wrist near neutral, elbow off-screen, no over-reach."""
    G = screen_pt(sx, sy, depth)
    tgt = dir_of(yaw, pitch)
    up = np.array([0, 0, 1.0]) - np.dot([0, 0, 1.0], tgt) * tgt
    up = nrm(up) if np.linalg.norm(up) > 1e-3 else np.array([-1.0, 0, 0])
    ytgt = rot(tgt, roll_about) @ up
    A0 = R_ARM.T @ R_HAND_REST
    gl = local[:3, 3]

    def build(x, pole):
        W = G - np.array([0.0, 0.0, 0.0])
        k = None
        for _ in range(7):
            k = ArmKey(W, x[0], x[1], x[2], pole, **kw)
            _, E, _, Rl, pulled = ik(k)
            Rh = hand_rot(Rl[:, 1], x[0], x[1], x[2])
            W = G - Rh @ gl
        k.W = W
        return k, E, Rl, Rh, pulled

    def cost(x, pole):
        k, E, Rl, Rh, pulled = build(x, pole)
        F = Rh @ local[:3, :3]
        e = (1 - np.dot(F[:, 0], tgt)) * w_dir + (1 - np.dot(F[:, 1], ytgt)) * w_roll
        tw = twist_angle((Rl.T @ Rh) @ A0.T)
        e += 0.02 * max(0, tw - 88) ** 2 + 0.02 * max(0, -108 - tw) ** 2 + 0.00001 * tw ** 2
        e += 0.00008 * (x[1] - flex_pref) ** 2 + 0.00008 * (x[2] - dev_pref) ** 2
        e += 0.02 * max(0, x[1] - 55) ** 2 + 0.02 * max(0, -x[1] - 45) ** 2
        e += 0.03 * max(0, x[2] - 33) ** 2 + 0.03 * max(0, -x[2] - 15) ** 2
        e += 400 * pulled
        if E[0] > 0.03:                               # elbow should stay out of the picture
            ex, ey = -E[1] / E[0] / TAN_H, E[2] / E[0] / TAN_V
            if abs(ex) < 1.0 and ey > -1.0:
                e += 0.6 * min(1.0 - abs(ex), ey + 1.0)
        return e, tw
    best = (1e18, None, None)
    for pole in poles:
        for r0 in (-60, 0, 60, 120, 170):
            x = np.array([r0, flex_pref, dev_pref], float)
            c, _ = cost(x, pole)
            step = 24.0
            while step > 0.4:
                imp = False
                for i in range(3):
                    for sgn in (1, -1):
                        y = x.copy()
                        y[i] += sgn * step
                        cy, _ = cost(y, pole)
                        if cy < c:
                            x, c, imp = y, cy, True
                if not imp:
                    step *= 0.5
            if c < best[0]:
                best = (c, x.copy(), pole)
    c, x, pole = best
    k, E, Rl, Rh, pulled = build(x, pole)
    k.roll = float((x[0] + 180.0) % 360.0 - 180.0)
    if verbose:
        log(f"place({sx},{sy},{depth}) -> cost {c:.3f} pole {pole} roll {k.roll:.0f} flex {x[1]:.0f} dev {x[2]:.0f} "
            f"W {tuple(round(v, 3) for v in k.W)}")
    return k


def knife_at(sx, sy, depth, yaw, pitch, spine=0.0, **kw):
    kw.setdefault("dev_pref", 25.0)
    return place(weapon_rel(), sx, sy, depth, yaw, pitch, spine, **kw)


def hand_at(sx, sy, depth, yaw, pitch, back=0.0, **kw):
    """Knuckles at screen (sx, sy): fingers (hand Y) along (yaw, pitch), back of the hand rolled `back` from up."""
    kw.setdefault("shape", ("relax", "relax", 0))
    return place(KNUCKLE, sx, sy, depth, yaw, pitch, back, **kw)


def hand_vec(sx, sy, depth, fingers, dorsal, **kw):
    """hand_at() from camera-space vectors: `fingers` = hand distal axis, `dorsal` = back-of-hand normal (the palm
    faces -dorsal). Converts them to hand_at's yaw / pitch / back (back = signed angle, right-hand rule about the
    finger axis, from world-up-projected to dorsal)."""
    f = nrm(fingers)
    yaw, pitch = math.degrees(math.atan2(f[1], f[0])), math.degrees(math.asin(np.clip(f[2], -1, 1)))
    tgt = dir_of(yaw, pitch)
    up = np.array([0, 0, 1.0]) - tgt[2] * tgt
    up = nrm(up) if np.linalg.norm(up) > 1e-3 else np.array([-1.0, 0, 0])
    d = np.asarray(dorsal, float)
    d = nrm(d - np.dot(d, tgt) * tgt)
    back = math.degrees(math.atan2(np.dot(np.cross(up, d), tgt), np.dot(up, d)))
    return hand_at(sx, sy, depth, yaw, pitch, back, **kw)


def unwrap(k, ref):
    """Shift k.roll by whole turns so interpolating from `ref` takes the short way round."""
    k.roll += 360.0 * round((ref.roll - k.roll) / 360.0)
    return k


def solve_arm(key, report=None):
    """World matrices for all right-side bones for one ArmKey."""
    out = {}
    S, E, Ru, Rl, pulled = ik(key)
    W = key.W
    H = m4(hand_rot(Rl[:, 1], key.roll, key.flex, key.dev), W)
    out["upperarm_r"] = m4(Ru, S)
    out["lowerarm_r"] = m4(Rl, E)
    A0 = R_ARM.T @ R_HAND_REST
    Q = (Rl.T @ H[:3, :3]) @ A0.T
    rho = twist_angle(Q)
    for name, t, share in TWISTS:
        out[name + "_r"] = m4(Rl @ RY(rho * share), E + Rl[:, 1] * (L_FORE * t))
    out["hand_r"] = H
    fingers, spread, th = shape_params(key)
    for f in ("index", "middle", "ring", "pinky"):
        for name, T in finger_chain(H, None, f, fingers[f], spread.get(f, 0.0)):
            out[name] = T
    for name, T in thumb_chain(H, th):
        out[name] = T
    basis = m4(RZ(key.wspin) @ RX(key.wroll), key.woff)
    out["weapon_r"] = H @ weapon_rel() @ basis
    if report is not None:
        bl = out["weapon_r"][:3, 0]
        sp = out["weapon_r"][:3, 1]
        el = 180 - math.degrees(math.acos(np.clip(np.dot(-Ru[:, 1], Rl[:, 1]), -1, 1)))
        fy = math.degrees(math.atan2(Rl[1, 1], Rl[0, 1]))
        fp = math.degrees(math.asin(np.clip(Rl[2, 1], -1, 1)))
        report.append(f"twist {rho:+.0f} roll {key.roll:.0f} flex {key.flex:+.0f} dev {key.dev:+.0f} elbow {el:.0f} "
                      f"forearm yaw {fy:+.0f} pitch {fp:+.0f} blade yaw {math.degrees(math.atan2(bl[1], bl[0])):+.0f} "
                      f"pitch {math.degrees(math.asin(np.clip(bl[2], -1, 1))):+.0f} spine z {sp[2]:+.2f}"
                      + (f" PULLED {pulled * 100:.1f}cm" if pulled > 0.002 else ""))
    return out


def solve_pose(kr, kl, report=None):
    pose = {"root": REST["root"][0], "camera": REST["camera"][0]}
    rr = [] if report is not None else None
    pose.update(solve_arm(kr, rr))
    rl = [] if report is not None else None
    for name, M in solve_arm(kl, rl).items():
        pose[name[:-2] + "_l"] = mirror_m4(M)
    if report is not None:
        report.append("R: " + "; ".join(rr) + " | L: " + "; ".join(rl))
    return pose


# ================================================================================================================
# SDF primitives (numpy, float64, points (N,3) in camera space)
# ================================================================================================================
def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


class Prim:
    """Tapered round cone / ellipsoid / rounded box / torus in a local frame (world rotation R, origin o)."""

    def __init__(self, kind, bone, o, R, **kw):
        self.kind, self.bone, self.o, self.R = kind, bone, np.asarray(o, float), np.asarray(R, float)
        self.kw = kw

    def local(self, P):
        return (P - self.o) @ self.R

    def eval(self, P):
        q = self.local(P)
        k = self.kw
        if self.kind == "cone":        # along local +Y from 0 to L, radii r1 -> r2, cross-section scaled sx (X), sz (Z)
            sx, sz = k.get("sx", 1.0), k.get("sz", 1.0)
            q = q / np.array([sx, 1.0, sz])
            L, r1, r2 = k["L"], k["r1"], k["r2"]
            l2 = L * L
            rr = r1 - r2
            a2 = l2 - rr * rr
            il2 = 1.0 / l2
            y = q[:, 1] * L
            z = y - l2
            x2 = (q[:, 0] ** 2 + q[:, 2] ** 2) * l2 * l2
            y2 = y * y * l2
            z2 = z * z * l2
            kk = np.sign(rr) * rr * rr * x2
            d1 = np.sqrt(x2 + z2) * il2 - r2
            d2 = np.sqrt(x2 + y2) * il2 - r1
            d3 = (np.sqrt(np.maximum(x2 * a2 * il2, 0)) + y * rr) * il2 - r1
            d = np.where(np.sign(z) * a2 * z2 > kk, d1, np.where(np.sign(y) * a2 * y2 < kk, d2, d3))
            return d * min(sx, sz)
        if self.kind == "ellip":
            r = np.asarray(k["r"], float)
            k0 = np.linalg.norm(q / r, axis=1)
            k1 = np.linalg.norm(q / (r * r), axis=1) + 1e-9
            return k0 * (k0 - 1.0) / k1
        if self.kind == "box":
            h = np.asarray(k["h"], float)
            d = np.abs(q) - h
            return np.linalg.norm(np.maximum(d, 0), axis=1) + np.minimum(d.max(axis=1), 0) - k["r"]
        if self.kind == "torus":      # ring around local Y
            rq = np.sqrt(q[:, 0] ** 2 + q[:, 2] ** 2) - k["major"]
            return np.sqrt(rq * rq + q[:, 1] ** 2) - k["minor"]
        raise ValueError(self.kind)

    def bounds(self):
        k = self.kw
        if self.kind == "cone":
            ext = max(k["r1"], k["r2"]) * max(k.get("sx", 1), k.get("sz", 1))
            pts = [self.o, self.o + self.R[:, 1] * k["L"]]
            return np.min(pts, 0) - ext, np.max(pts, 0) + ext
        if self.kind == "ellip":
            e = max(k["r"])
        elif self.kind == "box":
            e = np.linalg.norm(k["h"]) + k["r"]
        else:
            e = k["major"] + k["minor"]
        return self.o - e, self.o + e


def cone_between(bone, a, b, r1, r2, zhint, sx=1.0, sz=1.0):
    a, b = np.asarray(a, float), np.asarray(b, float)
    R = frame_yz(b - a, zhint)
    return Prim("cone", bone, a, R, L=float(np.linalg.norm(b - a)), r1=r1, r2=r2, sx=sx, sz=sz)


# ---------------------------------------------------------------------------------------------------------------
# the right arm, built from the rest skeleton
# ---------------------------------------------------------------------------------------------------------------
def hand_pt(p):
    H = REST["hand_r"][0]
    return H[:3, :3] @ np.asarray(p, float) + H[:3, 3]


def hand_dir(v):
    return REST["hand_r"][0][:3, :3] @ np.asarray(v, float)


class ArmSDF:
    def __init__(self):
        H = REST["hand_r"][0]
        Rh = H[:3, :3]
        zc = Rh[:, 2]
        self.palm = []
        self.palm.append(Prim("box", "hand_r", hand_pt((0.0010, 0.0530, -0.0045)), Rh, h=(0.0285, 0.029, 0.0045), r=0.0115))
        thenar_R = Rh @ frame_yz(THUMB_DIR, (0.3, 0.0, 1.0))
        self.palm.append(Prim("ellip", "hand_r", hand_pt((-0.0215, 0.0345, -0.0115)), thenar_R, r=(0.0155, 0.0265, 0.0125)))
        self.palm.append(Prim("ellip", "hand_r", hand_pt((0.0235, 0.0400, -0.0085)), Rh, r=(0.0125, 0.0300, 0.0115)))
        bases = {"index": (-0.0180, 0.012, 0.0015), "middle": (-0.0060, 0.011, 0.0025),
                 "ring": (0.0065, 0.012, 0.0015), "pinky": (0.0180, 0.015, -0.0005)}
        for f, (mcp, lens, radii, spread, curl) in FINGERS.items():
            self.palm.append(cone_between("hand_r", hand_pt(bases[f]), hand_pt(mcp), 0.0098, radii[0] + 0.0004, zc, sx=1.05, sz=0.9))
        # fingers: 3 tapered segments each (slightly wide, flat cross-section)
        self.fingers = {}
        for f in FINGERS:
            radii = FINGERS[f][2]
            segs = []
            for j in range(3):
                name = f"{f}_0{j + 1}_r"
                T = REST[name][0]
                a = T[:3, 3]
                b = a + T[:3, 1] * REST[name][2]
                segs.append(Prim("cone", name, a, T[:3, :3], L=REST[name][2], r1=radii[j], r2=radii[j + 1], sx=1.08, sz=0.94))
            self.fingers[f] = segs
        segs = []
        for j in range(3):
            name = f"thumb_0{j + 1}_r"
            T = REST[name][0]
            segs.append(Prim("cone", name, T[:3, 3], T[:3, :3], L=REST[name][2], r1=THUMB_R[j], r2=THUMB_R[j + 1],
                             sx=1.1, sz=0.92))
        self.fingers["thumb"] = segs
        # forearm: three elliptic segments, the wide axis turning from the elbow towards the wrist's radial-ulnar axis
        xw = Rh[:, 0]
        prof = [(0.26, 0.0385, 1.12, 0.92), (0.46, 0.0366, 1.17, 0.90), (0.76, 0.0322, 1.24, 0.86), (1.0, 0.0262, 1.33, 0.80)]
        self.forearm = []
        for i in range(3):
            t0, r0, sx0, sz0 = prof[i]
            t1, r1, sx1, sz1 = prof[i + 1]
            a = ELBOW + np.array([L_FORE * t0, 0, 0])
            b = ELBOW + np.array([L_FORE * t1, 0, 0])
            tm = 0.5 * (t0 + t1)
            wide = rot((1, 0, 0), -HAND_REST_ROLL * (1 - tm)) @ xw
            R = frame_yz(b - a, np.cross(wide, b - a))
            self.forearm.append(Prim("cone", "forearm", a, R, L=float(np.linalg.norm(b - a)), r1=r0, r2=r1,
                                     sx=0.5 * (sx0 + sx1), sz=0.5 * (sz0 + sz1)))
        # sleeve (loose linen) + rolled cuff
        up0 = SHOULDER + np.array([L_UP * 0.50, 0, 0])
        cuff_t = 0.52
        self.cuff_x = ELBOW[0] + L_FORE * cuff_t
        zh = np.array([0, 0, 1.0])
        self.sleeve = [
            cone_between("upperarm_r", up0, ELBOW + np.array([0.01, 0, 0]), 0.0545, 0.0520, zh, sx=1.0, sz=1.0),
            cone_between("lowerarm_r", ELBOW - np.array([0.01, 0, 0]), np.array([self.cuff_x, ELBOW[1], ELBOW[2]]), 0.0520, 0.0475, zh, sx=1.0, sz=1.0),
        ]
        R_ax = frame_yz((1, 0, 0), zh)
        c0 = np.array([self.cuff_x - 0.004, ELBOW[1], ELBOW[2]])
        self.cuff = [
            Prim("torus", "lowerarm_r", c0, R_ax, major=0.0420, minor=0.0105),
            Prim("torus", "lowerarm_r", c0 + np.array([0.0155, 0, 0]), R_ax, major=0.0385, minor=0.0088),
            Prim("cone", "lowerarm_r", c0 - np.array([0.004, 0, 0]), R_ax, L=0.018, r1=0.0435, r2=0.0395),
        ]
        self.sleeve_x0 = up0[0]

    # --- combined fields --------------------------------------------------------------------------------------
    def skin(self, P):
        palm = self.palm[0].eval(P)
        for p in self.palm[1:]:
            palm = smin(palm, p.eval(P), 0.0105)
        hand = None
        for f, segs in self.fingers.items():
            d = segs[0].eval(P)
            for s in segs[1:]:
                d = smin(d, s.eval(P), 0.0035)
            k = 0.0135 if f == "thumb" else 0.0072
            dd = smin(palm, d, k)
            hand = dd if hand is None else np.minimum(hand, dd)
        fa = self.forearm[0].eval(P)
        for p in self.forearm[1:]:
            fa = smin(fa, p.eval(P), 0.03)
        # thinner under the sleeve so the skin never pokes through the linen when the forearm twists
        fa = fa + 0.30 * np.clip(self.cuff_x - 0.004 - P[:, 0], 0.0, 0.2)
        return smin(fa, hand, 0.022)

    def sleeve_field(self, P):
        d = smin(self.sleeve[0].eval(P), self.sleeve[1].eval(P), 0.03)
        # folds: soft diagonal ridges + bunching rings above the inner elbow
        ax = P - np.array([0, ELBOW[1], ELBOW[2]])
        th = np.arctan2(ax[:, 2], ax[:, 1])
        s = P[:, 0] - self.sleeve_x0
        env = np.clip(s / 0.04, 0, 1) * np.clip((self.cuff_x - 0.02 - P[:, 0]) / 0.03, 0, 1)
        fold = 0.55 * np.sin(3 * th + 34 * s + 0.7) + 0.45 * np.sin(5 * th - 26 * s + 2.1) + 0.25 * np.sin(2 * th + 61 * s)
        ring = np.sin(95 * (P[:, 0] - ELBOW[0])) * np.exp(-((P[:, 0] - ELBOW[0] - 0.01) / 0.045) ** 2) * np.maximum(0, np.sin(th)) ** 2
        self._fold = fold * env + 0.8 * ring
        d = d - 0.0021 * fold * env - 0.0024 * ring
        # rolled cuff
        cf = self.cuff[0].eval(P)
        for p in self.cuff[1:]:
            cf = smin(cf, p.eval(P), 0.004)
        d = smin(d, cf, 0.006)
        return d

    def all_field(self, P):
        return np.minimum(self.skin(P), self.sleeve_field(P))


# ================================================================================================================
# surface extraction (surface nets) + cleanup
# ================================================================================================================
EDGES = [((0, 0, 0), (1, 0, 0)), ((0, 1, 0), (1, 1, 0)), ((0, 0, 1), (1, 0, 1)), ((0, 1, 1), (1, 1, 1)),
         ((0, 0, 0), (0, 1, 0)), ((1, 0, 0), (1, 1, 0)), ((0, 0, 1), (0, 1, 1)), ((1, 0, 1), (1, 1, 1)),
         ((0, 0, 0), (0, 0, 1)), ((1, 0, 0), (1, 0, 1)), ((0, 1, 0), (0, 1, 1)), ((1, 1, 0), (1, 1, 1))]


def sample_grid(field, lo, hi, h):
    n = np.ceil((hi - lo) / h).astype(int) + 1
    xs = lo[0] + np.arange(n[0]) * h
    ys = lo[1] + np.arange(n[1]) * h
    zs = lo[2] + np.arange(n[2]) * h
    F = np.empty(n, np.float32)
    Y, Z = np.meshgrid(ys, zs, indexing="ij")
    yz = np.column_stack([Y.ravel(), Z.ravel()])
    for i, x in enumerate(xs):
        P = np.column_stack([np.full(len(yz), x), yz])
        F[i] = field(P).reshape(n[1], n[2])
    return F


def surface_nets(F, lo, h):
    inside = F < 0
    nx, ny, nz = F.shape
    cs = (slice(0, nx - 1), slice(0, ny - 1), slice(0, nz - 1))

    def corner(o):
        return tuple(slice(o[i], F.shape[i] - 1 + o[i]) for i in range(3))
    cnt_in = np.zeros((nx - 1, ny - 1, nz - 1), np.int8)
    for o in [(i, j, k) for i in (0, 1) for j in (0, 1) for k in (0, 1)]:
        cnt_in += inside[corner(o)]
    active = (cnt_in > 0) & (cnt_in < 8)
    idx = -np.ones(active.shape, np.int64)
    ai = np.nonzero(active)
    nv = len(ai[0])
    idx[ai] = np.arange(nv)
    acc = np.zeros((nv, 3))
    cnt = np.zeros(nv)
    for oa, ob in EDGES:
        fa = F[corner(oa)]
        fb = F[corner(ob)]
        m = active & ((fa < 0) != (fb < 0))
        mi = np.nonzero(m)
        t = (fa[mi] / (fa[mi] - fb[mi]))[:, None]
        base = np.column_stack(mi).astype(float)
        pos = base + np.array(oa) + t * (np.array(ob) - np.array(oa))
        vi = idx[mi]
        acc[vi] += pos
        cnt[vi] += 1
    verts = lo + h * acc / cnt[:, None]
    faces = []
    # x-edges
    a = inside[:-1, 1:-1, 1:-1]
    b = inside[1:, 1:-1, 1:-1]
    m = a != b
    i, j, k = np.nonzero(m)
    j, k = j + 1, k + 1
    q = np.column_stack([idx[i, j - 1, k - 1], idx[i, j, k - 1], idx[i, j, k], idx[i, j - 1, k]])
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    faces.append(q)
    # y-edges
    a = inside[1:-1, :-1, 1:-1]
    b = inside[1:-1, 1:, 1:-1]
    m = a != b
    i, j, k = np.nonzero(m)
    i, k = i + 1, k + 1
    q = np.column_stack([idx[i - 1, j, k - 1], idx[i - 1, j, k], idx[i, j, k], idx[i, j, k - 1]])
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    faces.append(q)
    # z-edges
    a = inside[1:-1, 1:-1, :-1]
    b = inside[1:-1, 1:-1, 1:]
    m = a != b
    i, j, k = np.nonzero(m)
    i, j = i + 1, j + 1
    q = np.column_stack([idx[i - 1, j - 1, k], idx[i, j - 1, k], idx[i, j, k], idx[i - 1, j, k]])
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    faces.append(q)
    faces = np.concatenate(faces)
    faces = faces[(faces >= 0).all(axis=1)]
    return verts, faces


def grad(field, P, e=2e-4):
    g = np.zeros_like(P)
    for i in range(3):
        d = np.zeros(3)
        d[i] = e
        g[:, i] = (field(P + d) - field(P - d)) / (2 * e)
    return g


def project(field, P, iters=3):
    for _ in range(iters):
        d = field(P)
        g = grad(field, P)
        gg = np.maximum((g * g).sum(1), 1e-12)
        P = P - (d / gg)[:, None] * g
    return P


def smooth_mesh(V, F, iters=3, lam=0.55, field=None):
    n = len(V)
    # adjacency via face edges
    e = np.concatenate([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 3]], F[:, [3, 0]]])
    e = np.concatenate([e, e[:, ::-1]])
    deg = np.bincount(e[:, 0], minlength=n).astype(float)
    for _ in range(iters):
        acc = np.zeros_like(V)
        np.add.at(acc, e[:, 0], V[e[:, 1]])
        avg = acc / np.maximum(deg, 1)[:, None]
        V = V + lam * (avg - V)
        if field is not None:
            V = project(field, V, 2)
    return V


def mesh_from(field, lo, hi, h, target_tris, name):
    t = time.time()
    F = sample_grid(field, lo, hi, h)
    V, Q = surface_nets(F, lo, h)
    V = project(field, V, 2)
    V = smooth_mesh(V, Q, iters=3, field=field)
    me = bpy.data.meshes.new(name + "_dense")
    me.from_pydata(V.tolist(), [], Q.tolist())
    me.validate()
    ob = bpy.data.objects.new(name + "_dense", me)
    bpy.context.scene.collection.objects.link(ob)
    ntri = len(Q) * 2
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = min(1.0, target_tris / ntri)
    dg = bpy.context.evaluated_depsgraph_get()
    me2 = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    bm = bmesh.new()
    bm.from_mesh(me2)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_degenerate(bm, dist=1e-6, edges=bm.edges[:])
    bm.to_mesh(me2)
    bm.free()
    V2 = np.array([v.co[:] for v in me2.vertices])
    V2 = project(field, V2, 3)
    for v, p in zip(me2.vertices, V2):
        v.co = p
    log(f"{name}: grid {F.shape} dense {len(V)}v/{ntri}t -> {len(me2.polygons)} tris ({time.time() - t:.1f}s)")
    return me2


# ================================================================================================================
# weights + colours
# ================================================================================================================
CHAIN_PARENT = {}
for _f in FINGER_NAMES:
    CHAIN_PARENT[f"{_f}_01_r"] = "hand_r"
    CHAIN_PARENT[f"{_f}_02_r"] = f"{_f}_01_r"
    CHAIN_PARENT[f"{_f}_03_r"] = f"{_f}_02_r"


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def forearm_weights(tpar):
    """lowerarm / twist_01 / twist_02 shares along the forearm (roll spreads linearly)."""
    t = np.clip(tpar, 0, 1)
    w_lo = np.clip(1 - t / 0.5, 0, 1)
    w_t1 = np.where(t < 0.5, t / 0.5, np.clip(1 - (t - 0.5) / 0.35, 0, 1))
    w_t2 = np.clip((t - 0.5) / 0.35, 0, 1)
    return {"lowerarm_r": w_lo, "lowerarm_twist_01_r": w_t1, "lowerarm_twist_02_r": w_t2}


def skin_weights(sdf, V, F):
    n = len(V)
    H = REST["hand_r"][0]
    hl = (V - H[:3, 3]) @ H[:3, :3]                    # hand-local coords
    tpar = (V[:, 0] - ELBOW[0]) / L_FORE
    # hand chain distances
    names = ["hand_r"]
    dists = [np.min([p.eval(V) for p in sdf.palm], axis=0)]
    for f, segs in sdf.fingers.items():
        for s in segs:
            names.append(s.bone)
            dists.append(s.eval(V))
    D = np.array(dists)                                   # (B, n)
    near = np.argmin(D, axis=0)
    W = np.zeros((len(names), n))
    temp = 0.0032
    for bi, bname in enumerate(names):
        sel = near == bi
        if not sel.any():
            continue
        cand = {bi}
        par = CHAIN_PARENT.get(bname)
        if par:
            cand.add(names.index(par))
        for ci, cn in enumerate(names):
            if CHAIN_PARENT.get(cn) == bname:
                cand.add(ci)
        cand = sorted(cand)
        sub = D[cand][:, sel]
        e = np.exp(-(sub - sub.min(axis=0)) / temp)
        e /= e.sum(axis=0)
        for k, ci in enumerate(cand):
            W[ci, sel] = e[k]
    # topological smoothing (does not bleed across the air gaps between fingers)
    e = np.concatenate([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]])
    e = np.concatenate([e, e[:, ::-1]])
    deg = np.bincount(e[:, 0], minlength=n).astype(float)
    for _ in range(3):
        acc = np.zeros_like(W)
        for bi in range(len(names)):
            acc[bi] = np.bincount(e[:, 0], weights=W[bi][e[:, 1]], minlength=n)
        W = 0.5 * W + 0.5 * acc / np.maximum(deg, 1)
    W /= W.sum(axis=0, keepdims=True)
    # wrist: blend hand chain -> forearm chain
    share = smoothstep(-0.014, 0.020, hl[:, 1])
    out = {names[i]: W[i] * share for i in range(len(names))}
    for k, v in forearm_weights(tpar).items():
        out[k] = out.get(k, 0) + v * (1 - share)
    return out


def sleeve_weights(V):
    x = V[:, 0]
    w_lo = smoothstep(ELBOW[0] - 0.05, ELBOW[0] + 0.045, x)
    out = {"upperarm_r": 1 - w_lo}
    tpar = (x - ELBOW[0]) / L_FORE
    for k, v in forearm_weights(tpar).items():
        out[k] = out.get(k, 0) + v * w_lo
    return out


def srgb(r, g, b):
    return np.array([r, g, b], float) / 255.0


SKIN = srgb(232, 160, 110)
SKIN_KNUCKLE = srgb(214, 128, 92)
SKIN_PALM = srgb(242, 184, 140)
SKIN_TIP = srgb(236, 146, 112)
LINEN = srgb(238, 224, 190)
LINEN_SHADE = srgb(206, 186, 146)
LINEN_CUFF = srgb(246, 236, 208)
NAIL = srgb(238, 178, 150)


def sdf_ao(field, P, N):
    ao = np.zeros(len(P))
    w = 1.0
    for i in range(1, 6):
        dlt = 0.006 * i
        ao += w * (dlt - field(P + N * dlt))
        w *= 0.55
    return np.clip(1 - 18.0 * ao, 0, 1)


def skin_colors(sdf, V, N, all_field):
    H = REST["hand_r"][0]
    hl = (V - H[:3, 3]) @ H[:3, :3]
    c = np.tile(SKIN, (len(V), 1))
    # palm side lighter
    palm = smoothstep(0.004, -0.016, hl[:, 2]) * smoothstep(-0.02, 0.02, hl[:, 1])
    c = c + (SKIN_PALM - SKIN) * palm[:, None] * 0.8
    # knuckles (dorsal MCP / PIP) and fingertips warmer
    kn = np.zeros(len(V))
    tip = np.zeros(len(V))
    for f in ("index", "middle", "ring", "pinky"):
        for j, rad in ((0, 0.013), (1, 0.010), (2, 0.009)):
            T = REST[f"{f}_0{j + 1}_r"][0]
            ctr = T[:3, 3] + T[:3, 2] * FINGERS[f][2][j] * 0.6
            kn = np.maximum(kn, np.exp(-(np.linalg.norm(V - ctr, axis=1) / rad) ** 2))
        T = REST[f"{f}_03_r"][0]
        tp = tip_of(T, f"{f}_03_r")
        tip = np.maximum(tip, np.exp(-(np.linalg.norm(V - tp, axis=1) / 0.012) ** 2))
    T = REST["thumb_03_r"][0]
    tip = np.maximum(tip, np.exp(-(np.linalg.norm(V - tip_of(T, "thumb_03_r"), axis=1) / 0.013) ** 2))
    c = c + (SKIN_KNUCKLE - c) * (0.55 * kn)[:, None]
    c = c + (SKIN_TIP - c) * (0.45 * tip)[:, None]
    ao = sdf_ao(all_field, V, N)
    c = c * (0.72 + 0.28 * ao)[:, None]
    return np.column_stack([np.clip(c, 0, 1), np.full(len(V), 0.55)])


def sleeve_colors(sdf, V, N, all_field):
    sdf.sleeve_field(V)
    fold = sdf._fold
    c = np.tile(LINEN, (len(V), 1))
    c = c + (LINEN_SHADE - LINEN) * np.clip(-fold * 0.6, 0, 1)[:, None] * 0.7
    cuff = smoothstep(sdf.cuff_x - 0.022, sdf.cuff_x - 0.012, V[:, 0])
    c = c + (LINEN_CUFF - c) * cuff[:, None]
    # stitched hem line on the roll + a warm stripe (villager tunic trim)
    stripe = np.exp(-((V[:, 0] - (sdf.cuff_x + 0.0015)) / 0.0026) ** 2) * cuff
    ao = sdf_ao(all_field, V, N)
    c = c * (0.70 + 0.30 * ao)[:, None]
    return np.column_stack([np.clip(c, 0, 1), np.full(len(V), 0.9)])


# ================================================================================================================
# nails: thin curved plates on the distal phalanges
# ================================================================================================================
def build_nails(skin_field):
    verts, faces, bones = [], [], []
    for f in FINGER_NAMES:
        name = f"{f}_03_r"
        T = REST[name][0]
        L = REST[name][2]
        if f == "thumb":
            r_tip = THUMB_R[3]
            y0, y1, wdt = 0.05 * L, 0.95 * L + 0.2 * r_tip, 0.78
        else:
            r_tip = FINGERS[f][2][3]
            y0, y1, wdt = 0.12 * L, 0.93 * L + 0.25 * r_tip, 0.74
        nu, nv = 5, 4
        grid_top, grid_bot = [], []
        for iv in range(nv):
            v = iv / (nv - 1)
            y = y0 + (y1 - y0) * v
            for iu in range(nu):
                u = (iu / (nu - 1)) * 2 - 1
                # rounded-rectangle outline: corners pulled in at the root
                ang = u * math.radians(58) * wdt * (0.82 + 0.18 * math.sin(math.pi * min(1, v * 1.4)))
                dirl = np.array([math.sin(ang), 0.0, math.cos(ang)])
                o = T[:3, 3] + T[:3, :3] @ np.array([0, y, 0])
                dw = T[:3, :3] @ dirl
                # march out to the skin surface along dw
                p = o + dw * 0.004
                for _ in range(8):
                    p = p + dw * float(skin_field(p[None])[0]) * -1.0
                grid_top.append(p + dw * 0.0011)
                grid_bot.append(p - dw * 0.0009)
        base = len(verts)
        verts += grid_top + grid_bot
        bones += [name] * (2 * nu * nv)
        nt = nu * nv
        for iv in range(nv - 1):
            for iu in range(nu - 1):
                a = base + iv * nu + iu
                faces.append((a, a + 1, a + nu + 1, a + nu))
                b = a + nt
                faces.append((b, b + nu, b + nu + 1, b + 1))
        ring = [iu for iu in range(nu)] + [iv * nu + nu - 1 for iv in range(1, nv)] + \
               [(nv - 1) * nu + iu for iu in range(nu - 2, -1, -1)] + [iv * nu for iv in range(nv - 2, 0, -1)]
        for k in range(len(ring)):
            a, b = base + ring[k], base + ring[(k + 1) % len(ring)]
            faces.append((a, a + nt, b + nt, b))
    return np.array(verts), faces, bones


# ================================================================================================================
# Blender assembly
# ================================================================================================================
def build_armature():
    arm = bpy.data.armatures.new("KG_FPArms2_Rig")
    ob = bpy.data.objects.new("KG_FPArms2_Rig", arm)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for name in BONE_ORDER:
        M, parent, L = REST[name]
        eb = arm.edit_bones.new(name)
        eb.head = (0, 0, 0)
        eb.tail = (0, L, 0)
        eb.matrix = Matrix(M.tolist())
        eb.length = L
        eb.use_deform = not (name in ("root", "camera") or name.startswith("weapon"))
    for name in BONE_ORDER:
        parent = REST[name][1]
        if parent:
            arm.edit_bones[name].parent = arm.edit_bones[parent]
            arm.edit_bones[name].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in ob.pose.bones:
        pb.rotation_mode = "QUATERNION"
    # check that edit-bone matrices round-tripped
    err = max(np.abs(np.array(ob.data.bones[n].matrix_local) - REST[n][0]).max() for n in BONE_ORDER)
    log(f"armature: {len(BONE_ORDER)} bones, rest round-trip err {err:.2e}")
    return ob


def vc_material():
    m = bpy.data.materials.new("M_KG_FPArms2")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    att = nt.nodes.new("ShaderNodeVertexColor")
    att.layer_name = "Col"
    sq = nt.nodes.new("ShaderNodeVectorMath")
    sq.operation = "MULTIPLY"
    nt.links.new(att.outputs["Color"], sq.inputs[0])
    nt.links.new(att.outputs["Color"], sq.inputs[1])
    nt.links.new(sq.outputs[0], bsdf.inputs["Base Color"])
    nt.links.new(att.outputs["Alpha"], bsdf.inputs["Roughness"])
    try:
        bsdf.inputs["Specular IOR Level"].default_value = 0.35
    except KeyError:
        pass
    return m


def build_mesh(rig):
    sdf = ArmSDF()
    all_f = sdf.all_field
    # skin: from inside the sleeve to the finger tips
    prims = sdf.palm + [s for segs in sdf.fingers.values() for s in segs] + sdf.forearm
    lo = np.min([p.bounds()[0] for p in prims], axis=0) - 0.014
    hi = np.max([p.bounds()[1] for p in prims], axis=0) + 0.014
    me_skin = mesh_from(sdf.skin, lo, hi, float(arg("--vox", 0.0018)), int(arg("--skin-tris", 4100)), "skin")
    sp = sdf.sleeve + sdf.cuff
    lo = np.min([p.bounds()[0] for p in sp], axis=0) - 0.022
    hi = np.max([p.bounds()[1] for p in sp], axis=0) + 0.022
    me_sleeve = mesh_from(sdf.sleeve_field, lo, hi, 0.0028, int(arg("--sleeve-tris", 1300)), "sleeve")

    parts = []
    for me, kind in ((me_skin, "skin"), (me_sleeve, "sleeve")):
        V = np.array([v.co[:] for v in me.vertices])
        Fc = np.array([p.vertices[:] for p in me.polygons])
        field = sdf.skin if kind == "skin" else sdf.sleeve_field
        N = grad(field, V)
        N /= np.linalg.norm(N, axis=1, keepdims=True)
        if kind == "skin":
            W = skin_weights(sdf, V, Fc)
            C = skin_colors(sdf, V, N, all_f)
        else:
            W = sleeve_weights(V)
            C = sleeve_colors(sdf, V, N, all_f)
        parts.append((V, Fc, N, W, C))
        bpy.data.meshes.remove(me)
    NV, NF, NB = build_nails(sdf.skin)
    NN = np.zeros_like(NV)
    nf = np.array(NF)
    # nail normals from the plate faces (per-vertex average)
    for q in nf:
        n = np.cross(NV[q[1]] - NV[q[0]], NV[q[3]] - NV[q[0]])
        for i in q:
            NN[i] += n
    NN /= np.maximum(np.linalg.norm(NN, axis=1, keepdims=True), 1e-12)
    NW = {}
    for i, b in enumerate(NB):
        NW.setdefault(b, np.zeros(len(NV)))[i] = 1.0
    NC = np.column_stack([np.tile(NAIL, (len(NV), 1)), np.full(len(NV), 0.42)])
    parts.append((NV, nf, NN, NW, NC))

    # assemble right + mirrored left into one mesh
    allV, allF, allN, allC, groups = [], [], [], [], {}
    off = 0
    for side in ("r", "l"):
        for V, Fc, N, W, C in parts:
            if side == "l":
                V = V * np.array([1, -1, 1])
                N = N * np.array([1, -1, 1])
                Fc = [tuple(reversed(f)) for f in Fc]
            allV.append(V)
            allN.append(N)
            allC.append(C)
            allF += [tuple(int(i) + off for i in f) for f in Fc]
            for b, w in W.items():
                bn = b if side == "r" else b[:-2] + "_l"
                g = groups.setdefault(bn, {})
                for vi in np.nonzero(w > 1e-4)[0]:
                    g[int(vi) + off] = float(w[vi])
            off += len(V)
    V = np.concatenate(allV)
    N = np.concatenate(allN)
    C = np.concatenate(allC)
    me = bpy.data.meshes.new("SK_KG_FPArms2")
    me.from_pydata(V.tolist(), [], allF)
    me.validate()
    for p in me.polygons:
        p.use_smooth = True
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    col.data.foreach_set("color", C.astype(np.float32).ravel())
    me.color_attributes.active_color = col
    try:
        me.color_attributes.render_color_index = me.color_attributes.active_color_index
    except Exception:
        pass
    me.normals_split_custom_set_from_vertices(N.tolist())
    me.materials.append(vc_material())
    ob = bpy.data.objects.new("SK_KG_FPArms2", me)
    bpy.context.scene.collection.objects.link(ob)
    # limit to 4 influences, renormalise
    per_v = {}
    for b, g in groups.items():
        for vi, w in g.items():
            per_v.setdefault(vi, []).append((w, b))
    vg = {b: ob.vertex_groups.new(name=b) for b in groups}
    for vi, lst in per_v.items():
        lst.sort(reverse=True)
        lst = lst[:4]
        s = sum(w for w, _ in lst)
        for w, b in lst:
            if w / s > 0.004:
                vg[b].add([vi], w / s, "REPLACE")
    ob.parent = rig
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = rig
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    log(f"mesh: {len(me.vertices)} verts, {tris} tris, groups {len(vg)}")
    return ob, sdf


# ================================================================================================================
# clips
# ================================================================================================================
def ease(kind, t):
    if kind == "lin":
        return t
    if kind == "in":
        return t * t * t
    if kind == "out":
        return 1 - (1 - t) ** 3
    if kind == "in2":
        return t * t
    if kind == "out2":
        return 1 - (1 - t) ** 2
    return t * t * (3 - 2 * t)      # "io"


class Track:
    """Keys: list of (frame, ArmKey, ease-into-this-key)."""

    def __init__(self, keys):
        self.keys = keys

    def sample(self, fr):
        ks = self.keys
        if fr <= ks[0][0]:
            return ks[0][1], ks[0][1], 0.0
        for i in range(1, len(ks)):
            if fr <= ks[i][0]:
                f0, k0, _ = ks[i - 1]
                f1, k1, e = ks[i]
                t = (fr - f0) / max(1e-6, f1 - f0)
                return k0, k1, ease(e, t)
        return ks[-1][1], ks[-1][1], 0.0


def lerp_key(k0, k1, t):
    k = ArmKey(k0.W + (k1.W - k0.W) * t, k0.roll + (k1.roll - k0.roll) * t, k0.flex + (k1.flex - k0.flex) * t,
               k0.dev + (k1.dev - k0.dev) * t, k0.pole + (k1.pole - k0.pole) * t,
               k0.shoulder + (k1.shoulder - k0.shoulder) * t, k0.shape, k0.wspin + (k1.wspin - k0.wspin) * t,
               k0.woff + (k1.woff - k0.woff) * t, k0.wroll + (k1.wroll - k0.wroll) * t)
    k._mix = (k0.shape, k1.shape, t)
    return k


def smooth_keys(seq, loop, sigma):
    """Gaussian smoothing of every numeric FK channel (C1-ish motion through the keys)."""
    if sigma <= 0 or len(seq) < 3:
        return seq
    n = len(seq)
    rad = int(math.ceil(sigma * 2.5))
    w = np.array([math.exp(-0.5 * (i / sigma) ** 2) for i in range(-rad, rad + 1)])
    chans = {}
    for nm in ArmKey.NUM:
        chans[nm] = np.array([np.atleast_1d(getattr(k, nm)).astype(float) for k in seq])
    out = {nm: np.zeros_like(v) for nm, v in chans.items()}
    for i in range(n):
        idx = []
        for kk in range(-rad, rad + 1):
            j = i + kk
            j = j % (n - 1) if loop else min(max(j, 0), n - 1)
            idx.append(j)
        for nm, v in chans.items():
            out[nm][i] = (w[:, None] * v[idx]).sum(0) / w.sum()
    for i, k in enumerate(seq):
        for nm in ArmKey.NUM:
            val = out[nm][i]
            setattr(k, nm, float(val[0]) if val.shape == (1,) else val)
    return seq


class Clip:
    def __init__(self, name, frames, loop, right, left, sigma=0.0, extra=None, preview=()):
        self.name, self.frames, self.loop = name, frames, loop
        self.right, self.left = right, left
        self.sigma, self.extra = sigma, extra
        self.preview = preview

    def keys_at(self):
        out = []
        for side, tr in (("r", self.right), ("l", self.left)):
            seq = []
            for fr in range(self.frames + 1):
                k0, k1, t = tr.sample(fr)
                seq.append(lerp_key(k0, k1, t))
            seq = smooth_keys(seq, self.loop, self.sigma)
            if self.extra:
                for fr, k in enumerate(seq):
                    self.extra(side, fr, k)
            out.append(seq)
        return list(zip(out[0], out[1]))


def breathe(amp, period):
    def fn(side, fr, k):
        ph = 2 * math.pi * fr / period
        k.W = k.W + np.array([0.0016 * math.sin(ph + 0.6), 0.0, 0.0034 * math.sin(ph)]) * amp
        k.roll += 0.9 * amp * math.sin(ph + 1.1)
        k.flex += 1.3 * amp * math.sin(ph + 0.3)
    return fn


HIDDEN = ArmKey((0.05, -0.30, -0.62), roll=60, flex=10, pole=(-0.2, -0.6, -1.0), shape=("relax", "relax", 0))


def build_clips():
    """Key poses are placed in SCREEN space (sx right, sy up in [-1, 1] at FOV 72 / 16:9, depth in metres) and
    solved for the most natural arm; clips interpolate the resulting FK channels."""
    C = {}
    V = arg("--verbose-keys", False)
    L_hidden = Track([(0, HIDDEN, "lin")])
    idle = knife_at(0.56, -0.80, 0.37, 30, 4, 45, dev_pref=28, flex_pref=-5, verbose=V)
    # --- knife idle (loop, 3 s breathing) ---------------------------------------------------------------------
    C["knife_idle"] = Clip("knife_idle", 90, True, Track([(0, idle, "lin")]), L_hidden, extra=breathe(1.0, 90),
                           preview=(0, 45))
    # --- knife draw (0.6 s): up from below the frame, blade flicks forward, settles -----------------------------------
    d0 = knife_at(0.62, -1.75, 0.26, 40, 70, -60, shape=("loose", "loose", 0), verbose=V)
    d1 = knife_at(0.58, -0.95, 0.32, 36, 45, -10, shape=("loose", "grip", 0.6), verbose=V)
    d2 = knife_at(0.52, -0.68, 0.36, 24, -8, 52, verbose=V)
    d3 = knife_at(0.57, -0.83, 0.37, 31, 8, 42, verbose=V)
    C["knife_draw"] = Clip("knife_draw", 18, False,
                           Track([(0, d0, "lin"), (6, d1, "out2"), (11, d2, "out"), (15, d3, "io"), (18, idle, "io")]),
                           L_hidden, sigma=0.8, preview=(0, 6, 11, 18))
    # --- slash A (0.45 s): right -> left diagonal, top-right to bottom-left, edge leading ------------------------------
    a1 = knife_at(0.78, -0.05, 0.30, -30, 45, -80, verbose=V)
    a2 = knife_at(0.05, -0.35, 0.36, 45, 0, -95, verbose=V)
    a3 = knife_at(-0.55, -0.78, 0.32, 90, -30, -110, verbose=V)
    a4 = knife_at(-0.15, -0.98, 0.30, 60, -10, -40, verbose=V)
    C["knife_slash_a"] = Clip("knife_slash_a", 14, False, Track([
        (0, idle, "lin"), (3, a1, "out2"), (5, a2, "in2"), (7, a3, "out"), (9, a4, "io"), (14, idle, "io")]),
        L_hidden, sigma=0.55, preview=(3, 5, 7))
    # --- slash B (0.45 s): backhand left -> right --------------------------------------------------------------------
    b1 = knife_at(-0.30, -0.35, 0.36, 75, 25, 80, verbose=V)
    b2 = knife_at(0.25, -0.55, 0.38, -10, 0, 70, w_roll=20, verbose=V)
    b3 = knife_at(0.80, -0.78, 0.32, -50, -20, 100, verbose=V)
    C["knife_slash_b"] = Clip("knife_slash_b", 14, False, Track([
        (0, idle, "lin"), (3, b1, "out2"), (5, b2, "in2"), (7, b3, "out"), (14, idle, "io")]),
        L_hidden, sigma=0.55, preview=(3, 5, 7))
    # --- stab (0.6 s): wind up, heavy forward thrust to the centre (the backstab), recover ------------------------------
    s1 = knife_at(0.54, -0.64, 0.33, 20, 10, 35, verbose=V)
    s2 = knife_at(0.10, -0.30, 0.52, 5, -8, 12, shoulder=(0.06, 0.02, 0.0), verbose=V)
    s3 = s2.but(dW=(-0.012, 0, -0.005))
    C["knife_stab"] = Clip("knife_stab", 18, False,
                           Track([(0, idle, "lin"), (5, s1, "io"), (8, s2, "in"), (10, s3, "out"), (18, idle, "io")]),
                           L_hidden, sigma=0.5, preview=(5, 8, 10))
    # --- inspect (3.2 s), CS2-style and weighty: raise the knife to the lower centre-right with the flat of the blade
    # turned to the eye (slight overshoot, settle), supinate so the blade rolls 180 about its own axis to show the other
    # face (the tip stays put: no whip over the top), dip, toss-flip it 360 about the blade axis in loosened fingers,
    # catch, hold, back to idle. Every rotation is about the blade axis or the forearm, so the tip never swings at the
    # eye (min depth ~39 cm) and stays below the crosshair (Tools/Blender/kg_fp2_metrics.py checks it per frame).
    p1 = knife_at(0.34, -0.40, 0.38, 72, 6, 45, verbose=V)
    p1o = p1.but(dW=(0.0, 0.0, 0.005), droll=-3)
    p1b = p1.but(dW=(0.003, 0.003, 0.002), droll=-5)
    p2 = knife_at(0.36, -0.40, 0.38, 76, 4, 180, verbose=V)
    p2b = p2.but(dW=(-0.002, -0.003, 0.002), droll=4)
    dip = p2b.but(dW=(0.0, 0.0, -0.006), dflex=-6)
    flip = p2b.but(dW=(0.0, 0.0, 0.010), dflex=8, wroll=360.0, shape=("grip", "loose", 1.0))
    catch = p2b.but(dW=(0.0, 0.0, -0.004), wroll=360.0, shape=("loose", "grip", 1.0))
    hold = p2b.but(dW=(0.002, 0.0, 0.0), droll=-6, wroll=360.0)
    C["knife_inspect"] = Clip("knife_inspect", 96, False, Track([
        (0, idle, "lin"), (13, p1o, "out2"), (18, p1, "io"), (31, p1b, "io"),
        (45, p2, "io"), (52, p2b, "io"), (57, dip, "io"), (67, flip, "io"), (72, catch, "out2"),
        (82, hold, "io"), (96, idle.but(wroll=360.0), "io")]), L_hidden, sigma=1.2,
        preview=(13, 24, 38, 48, 60, 63, 66, 76, 88))
    # --- fists idle (loop): both fists low, only the knuckles in the frame ----------------------------------------------
    fi = hand_at(0.48, -0.84, 0.36, 18, 38, -35, shape=("fist", "fist", 0), dev_pref=5, verbose=V)
    C["fists_idle"] = Clip("fists_idle", 90, True, Track([(0, fi, "lin")]), Track([(0, fi, "lin")]),
                           extra=breathe(1.0, 90), preview=(0,))
    # --- punches (0.35 s) ---------------------------------------------------------------------------------------------
    p1 = fi.but(dW=(-0.03, 0.0, 0.008), droll=4)
    p2 = hand_at(0.24, -0.32, 0.50, 4, 4, -15, shape=("fist", "fist", 0), shoulder=(0.07, 0.03, 0.02), dev_pref=5,
                 verbose=V)
    p3 = p2.but(dW=(-0.02, -0.004, -0.004))
    guard = fi.but(dW=(-0.02, 0.01, 0.015), droll=5)
    punch = Track([(0, fi, "lin"), (2, p1, "io"), (5, p2, "out2"), (6, p3, "lin"), (11, fi, "io")])
    C["punch_r"] = Clip("punch_r", 11, False, punch, Track([(0, fi, "lin"), (4, guard, "io"), (11, fi, "io")]),
                        sigma=0.5, preview=(5,))
    C["punch_l"] = Clip("punch_l", 11, False, Track([(0, fi, "lin"), (4, guard, "io"), (11, fi, "io")]), punch,
                        sigma=0.5, preview=(5,))
    # --- reach + grab (0.5 s): open hand reaches forward, then closes ------------------------------------------------
    r0 = hand_at(0.45, -1.55, 0.26, 20, 35, 10, shape=("relax", "relax", 0), verbose=V)
    r1 = hand_at(0.20, -0.48, 0.42, 10, -14, -5, shape=("open", "open", 0), dev_pref=0, w_roll=30, verbose=V)
    r2 = r1.but(dW=(0.025, 0.003, -0.003))
    r3 = r2.but(dW=(-0.012, 0.0, 0.0), shape=("grab", "grab", 0), dflex=6)
    C["reach_grab"] = Clip("reach_grab", 15, False, Track([(0, r0, "lin"), (7, r1, "out2"), (10, r2, "io"), (15, r3, "io")]),
                           L_hidden, sigma=0.6, preview=(7, 10, 15))
    # --- carry idle (loop): both hands forward, palms facing each other around a box ----------------------------------
    ca = hand_at(0.46, -0.70, 0.40, 4, 6, -80, shape=("carry", "carry", 0), dev_pref=0, verbose=V)
    C["carry_idle"] = Clip("carry_idle", 90, True, Track([(0, ca, "lin")]), Track([(0, ca, "lin")]),
                           extra=breathe(0.8, 90), preview=(0,))
    # --- open hands idle (loop): relaxed open hands low in the corners (CS2 empty-hand look) ----------------------------
    oh = hand_at(0.62, -0.92, 0.40, 22, 22, -60, shape=("relax", "open", 0.3), dev_pref=5, verbose=V)
    C["hands_idle"] = Clip("hands_idle", 90, True, Track([(0, oh, "lin")]), Track([(0, oh, "lin")]),
                           extra=breathe(1.0, 90), preview=(0,))
    build_rod_clips(C, L_hidden, V)
    # --- hidden (1-frame pose, exported as 2 identical frames) ----------------------------------------------------------
    C["empty_hidden"] = Clip("empty_hidden", 1, False, Track([(0, HIDDEN, "lin")]), L_hidden, preview=(0,))
    build_emote_clips(C, L_hidden, V)
    return C


def rod_reel_knob(kr):
    """Camera-space point of the reel's crank knob for a right-arm key holding the rod (weapon frame: X = rod,
    bone Y = rod up / spine, bone Z = rod -Y)."""
    Wm = solve_arm(kr)["weapon_r"]
    # rod-local knob (0.337, 0.04, -0.072) m minus the grip, scaled; rod (x, y, z) -> bone (x, z, -y) (see add_rod).
    rx, ry, rz = (0.337 - ROD_GRIP) * ROD_SCALE, 0.040 * ROD_SCALE, -0.072 * ROD_SCALE
    local = np.array([rx, rz, -ry, 1.0])
    return (Wm @ local)[:3], Wm[:3, 0]


def left_hand_at(P, fingers_yaw=35.0, fingers_pitch=-15.0, back=0.0, **kw):
    """Left-hand key whose knuckles sit at camera point P (the left arm is solved in mirrored space)."""
    m = np.array([P[0], -P[1], P[2]])
    sx = -m[1] / (m[0] * TAN_H)
    sy = m[2] / (m[0] * TAN_V)
    return hand_at(sx, sy, float(m[0]), fingers_yaw, fingers_pitch, back, **kw)


def build_rod_clips(C, L_hidden, V):
    """Fishing rod on weapon_r (Source/KillGodot/Fishing): hold, wind-up (charge), cast, reel (left hand cranks),
    hook-set. The rod tip must stay on screen in the hold / reel poses (the line starts there)."""
    idle = knife_at(0.52, -0.78, 0.38, 6, 24, 0, dev_pref=10, verbose=V)
    # --- idle hold (loop, 3 s breathing): cork low right, rod up-forward, tip a little right of centre ---------------
    C["rod_idle"] = Clip("rod_idle", 90, True, Track([(0, idle, "lin")]), L_hidden, extra=breathe(0.8, 90), preview=(0,))
    # --- wind-up (the charge): rod back over the right shoulder, trembling a little -------------------------------
    back = knife_at(0.72, -0.52, 0.34, -25, 70, 0, dev_pref=10, verbose=V)

    def tremble(side, fr, k):
        if side == "r":
            ph = 2 * math.pi * fr / 30.0
            k.W = k.W + np.array([0.0, 0.0012 * math.sin(ph * 3.0), 0.0016 * math.sin(ph * 2.0 + 0.4)])
            k.flex += 1.2 * math.sin(ph * 4.0)
    C["rod_windup"] = Clip("rod_windup", 30, True, Track([(0, back, "lin")]), L_hidden, extra=tremble, preview=(0,))
    mid = knife_at(0.62, -0.62, 0.36, -8, 50, 0, dev_pref=10, verbose=V)
    C["rod_windup_in"] = Clip("rod_windup_in", 9, False, Track([(0, idle, "lin"), (4, mid, "io"), (9, back, "out2")]),
                              L_hidden, sigma=0.6, preview=(4, 9))
    # --- cast (0.6 s): whip forward from the wind-up, overshoot low, settle into the hold ---------------------------
    snap = knife_at(0.42, -0.50, 0.46, 8, 8, 0, shoulder=(0.04, 0.01, 0.0), dev_pref=10, verbose=V)
    over = snap.but(dW=(0.01, 0.0, -0.03), dflex=-8)
    C["rod_cast"] = Clip("rod_cast", 18, False, Track([
        (0, back, "lin"), (3, mid, "in2"), (6, snap, "out"), (9, over, "io"), (18, idle, "io")]),
        L_hidden, sigma=0.5, preview=(3, 6, 9))
    # --- reel (loop 0.5 s): rod raised, the left hand cranks the reel in circles --------------------------------------
    reel = knife_at(0.50, -0.70, 0.42, 4, 27, 0, dev_pref=10, verbose=V)
    knob, rod_dir = rod_reel_knob(reel)
    crank = left_hand_at(knob + np.array([-0.01, 0.0, 0.0]), shape=("fist", "fist", 0), verbose=V)

    def cranking(side, fr, k):
        ph = 2 * math.pi * fr / 15.0
        if side == "l":
            # circles in the (forward, up) plane: a centre-pin reel spins about the rod's lateral axis
            k.W = k.W + np.array([0.028 * math.cos(ph), 0.0, 0.028 * math.sin(ph)])
            k.roll += 6.0 * math.sin(ph)
        else:
            k.W = k.W + np.array([0.0, 0.002 * math.sin(ph * 2.0), 0.004 * math.sin(ph)])
    C["rod_reel"] = Clip("rod_reel", 15, True, Track([(0, reel, "lin")]), Track([(0, crank, "lin")]), extra=cranking,
                         preview=(0, 4, 8, 11))
    # --- hook-set (0.45 s): sharp upward jerk, settle into the reel pose -----------------------------------------------
    jerk = knife_at(0.50, -0.50, 0.33, 2, 62, 0, dev_pref=10, verbose=V)
    C["rod_hookset"] = Clip("rod_hookset", 14, False, Track([
        (0, idle, "lin"), (3, jerk, "out"), (8, jerk.but(dW=(0.0, 0.0, -0.01)), "io"), (14, reel, "io")]),
        Track([(0, HIDDEN, "lin"), (8, HIDDEN, "lin"), (14, crank, "io")]),
        sigma=0.5, preview=(3, 8, 14))


def build_emote_clips(C, L_hidden, V):
    """First-person halves of the social emotes (the third-person body plays the full emote). Every clip starts and
    ends with both arms below the frame: the game only shows the arms during the gesture."""
    relax = ("relax", "relax", 0)
    # --- wave (2.4 s): open palm up in the upper right, facing away, 3 side-to-side waves (wrist deviation + a little
    # forearm sway), lowers out --------------------------------------------------------------------------------------
    wc = hand_vec(0.50, 0.20, 0.38, (0.18, -0.10, 1.0), (-1.0, -0.12, 0.05), shape=("open", "open", 0), dev_pref=5,
                  w_roll=30, verbose=V)
    w_low = wc.but(dW=(-0.06, -0.03, -0.34), dflex=20, shape=relax)
    w_over = wc.but(dW=(0.0, 0.0, 0.018), dflex=-6)
    wl = wc.but(dW=(0.0, 0.012, 0.004), ddev=-24, droll=-4)
    wr = wc.but(dW=(0.0, -0.012, 0.0), ddev=11, droll=4)
    w_up = wc.but(dW=(0.0, 0.0, 0.010), dflex=-4)
    C["emote_wave"] = Clip("emote_wave", 72, False, Track([
        (0, w_low, "lin"), (10, w_over, "out2"), (15, wc, "io"), (20, wl, "io"), (26, wr, "io"), (32, wl, "io"),
        (38, wr, "io"), (44, wl, "io"), (50, wr, "io"), (55, wc, "io"), (59, w_up, "io"), (72, w_low, "in2")]),
        L_hidden, sigma=1.0, preview=(10, 20, 26, 64))
    # --- point (2.0 s): arm extends toward the centre-right, index forward, one emphatic jab, hold, lower out -----------
    pc = hand_vec(0.22, -0.22, 0.46, (1.0, 0.22, 0.12), (0.0, -0.30, 0.95), shape=("point", "point", 0),
                  shoulder=(0.05, 0.02, 0.01), dev_pref=0, w_roll=20, verbose=V)
    p_low = pc.but(dW=(-0.16, -0.02, -0.30), dflex=15, shape=relax)
    p_over = pc.but(dW=(0.012, 0.0, 0.010))
    p_cock = pc.but(dW=(-0.022, 0.0, 0.008), dflex=-6)
    p_jab = pc.but(dW=(0.030, 0.002, -0.006), dflex=7)
    p_hold = pc.but(dW=(0.004, 0.0, -0.003))
    C["emote_point"] = Clip("emote_point", 60, False, Track([
        (0, p_low, "lin"), (10, p_over, "out2"), (14, pc, "io"), (17, p_cock, "io"), (20, p_jab, "out"), (25, pc, "io"),
        (50, p_hold, "lin"), (60, p_low, "in2")]), L_hidden, sigma=0.8, preview=(10, 20, 36, 55))
    # --- clap (2.4 s): both flat hands up in the lower centre, 5 claps (palms meet on the centre line), lower out -------
    c_open = hand_vec(0.30, -0.55, 0.36, (0.70, 0.25, 0.65), (0.10, -1.0, 0.15), shape=("flat", "flat", 0),
                      dev_pref=0, w_roll=30, verbose=V)
    c_apart = unwrap(hand_vec(0.20, -0.50, 0.35, (0.72, 0.14, 0.65), (0.05, -1.0, 0.10), shape=("flat", "flat", 0),
                              dev_pref=0, w_roll=30, verbose=V), c_open)
    c_meet = unwrap(hand_vec(0.095, -0.45, 0.34, (0.75, 0.0, 0.65), (0.0, -1.0, 0.0), shape=("flat", "flat", 0),
                             dev_pref=0, w_roll=30, verbose=V), c_open)
    c_low = c_open.but(dW=(-0.06, -0.04, -0.30), dflex=15, shape=relax)
    c_up = c_open.but(dW=(0.0, 0.0, 0.008))
    ck = [(0, c_low, "lin"), (11, c_open, "out2"), (15, c_meet, "in2")]
    for fr in (22, 29, 36, 43):
        ck += [(fr - 3, c_apart, "out2"), (fr, c_meet, "in2")]
    ck += [(49, c_open, "out2"), (55, c_up, "io"), (72, c_low, "in2")]
    C["emote_clap"] = Clip("emote_clap", 72, False, Track(ck), Track(ck), sigma=0.45, preview=(13, 22, 26, 62))
    # --- salute (2.0 s): flat hand to the brow at the top-right edge (fingers leave the top of the frame, palm down,
    # hand >= 12 cm from the eye), hold, snap down out -------------------------------------------------------------
    sp = ((-0.1, -0.95, 0.2), (-0.3, -0.9, 0.3), (-0.2, -0.8, -0.5), (-0.35, -0.85, -0.2))
    sa = hand_vec(0.45, 0.80, 0.20, (-0.15, 0.90, 0.42), (-0.30, -0.50, 0.80), shape=("flat", "flat", 0), poles=sp,
                  dev_pref=0, w_roll=20, verbose=V)
    s_low = sa.but(dW=(-0.02, -0.10, -0.46), dflex=15, shape=relax)
    s_side = sa.but(dW=(-0.02, -0.22, -0.14), dflex=8)          # rises along (outside) the right edge of the frame
    s_over = sa.but(dW=(0.0, 0.004, 0.010))
    s_hold = sa.but(dW=(0.002, 0.0, -0.002))
    s_pre = sa.but(dW=(0.006, -0.004, 0.008))
    s_down = s_side.but(dW=(0.04, 0.0, -0.30), shape=relax)
    C["emote_salute"] = Clip("emote_salute", 60, False, Track([
        (0, s_low, "lin"), (7, s_side, "in2"), (12, s_over, "out2"), (16, sa, "io"), (40, s_hold, "lin"),
        (43, s_pre, "io"), (47, s_side, "in2"), (52, s_down, "out2"), (60, s_down, "lin")]), L_hidden, sigma=0.8,
        preview=(9, 16, 30, 45))


# ================================================================================================================
# baking
# ================================================================================================================
def pose_to_basis(rig, pose):
    out = {}
    for name in BONE_ORDER:
        b = rig.data.bones[name]
        rest = np.array(b.matrix_local)
        if b.parent:
            prest = np.array(b.parent.matrix_local)
            local_rest = inv4(prest) @ rest
            local_now = inv4(pose[b.parent.name]) @ pose[name]
        else:
            local_rest = rest
            local_now = pose[name]
        out[name] = inv4(local_rest) @ local_now
    return out


def bake_clip(rig, clip):
    keys = clip.keys_at()
    act = bpy.data.actions.new(clip.name)
    act.use_fake_user = True
    rig.animation_data_create()
    rig.animation_data.action = act
    nfr = clip.frames + 1
    locs = {n: np.zeros((nfr, 3)) for n in BONE_ORDER}
    quats = {n: np.zeros((nfr, 4)) for n in BONE_ORDER}
    reports = []
    for fr, (kr, kl) in enumerate(keys):
        rep = [] if fr in clip.preview else None
        pose = solve_pose(kr, kl, rep)
        if rep:
            reports.append(f"f{fr}: {rep[-1]}")
        basis = pose_to_basis(rig, pose)
        for n, B in basis.items():
            locs[n][fr] = B[:3, 3]
            q = quat_from_R(B[:3, :3])
            if fr > 0 and np.dot(q, quats[n][fr - 1]) < 0:
                q = -q
            quats[n][fr] = q
    if clip.frames == 1:
        pass
    for n in BONE_ORDER:
        for prop, arr, ncomp in (("location", locs[n], 3), ("rotation_quaternion", quats[n], 4)):
            dp = f'pose.bones["{n}"].{prop}'
            for c in range(ncomp):
                fc = act.fcurve_ensure_for_datablock(rig, dp, index=c) if hasattr(act, "fcurve_ensure_for_datablock") \
                    else act.fcurves.new(dp, index=c, action_group=n)
                fc.keyframe_points.add(nfr)
                co = np.column_stack([np.arange(nfr, dtype=float), arr[:, c]]).ravel()
                fc.keyframe_points.foreach_set("co", co)
                fc.keyframe_points.foreach_set("interpolation", [1] * nfr)   # LINEAR
                fc.update()
    for r in reports:
        log(f"  {clip.name} {r}")
    return act


# ================================================================================================================
# export
# ================================================================================================================
def export_glb(path, rig, mesh, action=None, frames=0):
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = rig
    sc = bpy.context.scene
    if action is not None:
        rig.animation_data.action = action
        sc.frame_start, sc.frame_end = 0, frames
    else:
        if rig.animation_data:
            rig.animation_data.action = None
        for pb in rig.pose.bones:
            pb.location = (0, 0, 0)
            pb.rotation_quaternion = (1, 0, 0, 0)
    sc.render.fps = FPS
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLB", use_selection=True, export_apply=False,
        export_vertex_color="ACTIVE", export_all_vertex_colors=False, export_normals=True, export_tangents=False,
        export_skins=True, export_def_bones=False, export_armature_object_remove=True, export_influence_nb=4,
        export_animations=action is not None, export_animation_mode="ACTIVE_ACTIONS", export_frame_range=True,
        export_force_sampling=True, export_optimize_animation_size=False, export_anim_slide_to_zero=False,
        export_reset_pose_bones=True, export_rest_position_armature=True, export_yup=True, export_materials="EXPORT")


def check_glb(path):
    import json
    import struct
    with open(path, "rb") as fh:
        data = fh.read()
    ln = struct.unpack_from("<I", data, 12)[0]
    js = json.loads(data[20:20 + ln])
    anims = js.get("animations", [])
    info = []
    for a in anims:
        mx = 0.0
        for s in a["samplers"]:
            acc = js["accessors"][s["input"]]
            mx = max(mx, acc.get("max", [0])[0])
        info.append(f"{a['name']}:{len(a['channels'])}ch {mx:.3f}s")
    prim = js["meshes"][0]["primitives"][0]
    col = prim["attributes"].get("COLOR_0")
    ctype = js["accessors"][col]["type"] if col is not None else None
    return f"nodes {len(js['nodes'])} skins {len(js.get('skins', []))} anims {info} COLOR_0 {ctype}"


# ================================================================================================================
# preview rendering
# ================================================================================================================
def setup_render(rig):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    try:
        sc.eevee.taa_render_samples = 32
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    world = bpy.data.worlds.new("KG_Prev")
    sc.world = world
    nt = world.node_tree
    bg = nt.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.42, 0.55, 0.72, 1.0)
    bg.inputs["Strength"].default_value = 0.9
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.2
    sun.data.angle = math.radians(3)
    sun.data.color = (1.0, 0.95, 0.86)
    sun.matrix_world = Matrix(m4(look_frame(nrm((0.55, -0.35, -0.75))), (0, 0, 2)).tolist())
    sc.collection.objects.link(sun)
    # backdrop: ground + far wall so the viewmodel reads against a scene
    gm = bpy.data.meshes.new("Ground")
    gm.from_pydata([(-5, -30, -1.7), (60, -30, -1.7), (60, 30, -1.7), (-5, 30, -1.7)], [], [(0, 1, 2, 3)])
    g = bpy.data.objects.new("Ground", gm)
    sc.collection.objects.link(g)
    gmat = bpy.data.materials.new("GroundM")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.34, 0.33, 0.29, 1)
    gm.materials.append(gmat)
    cam = bpy.data.objects.new("FPCam", bpy.data.cameras.new("FPCam"))
    cam.data.sensor_fit = "HORIZONTAL"
    cam.data.angle = math.radians(FOV_DEG)
    cam.data.clip_start = 0.01
    cam.data.clip_end = 200
    sc.collection.objects.link(cam)
    cam.parent = rig
    cam.parent_type = "BONE"
    cam.parent_bone = "camera"
    bpy.context.view_layer.update()
    # place at the eye looking +X (world), whatever the bone frame is
    cam.matrix_world = Matrix.Translation((0, 0, 0)) @ Matrix.Rotation(math.radians(-90), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")
    sc.camera = cam
    # side camera for grip checks
    side = bpy.data.objects.new("SideCam", bpy.data.cameras.new("SideCam"))
    side.data.lens = 50
    side.data.clip_start = 0.01
    sc.collection.objects.link(side)
    return cam, side


def add_knife(rig, side="r"):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=KNIFE_GLB)
    new = [o for o in bpy.data.objects if o not in before]
    mesh = [o for o in new if o.type == "MESH"][0]
    bpy.context.view_layer.update()
    mw = mesh.matrix_world.copy()
    mesh.data.transform(mw)                      # bake the glTF node transform (the knife comes in scaled)
    for o in new:
        if o is not mesh:
            bpy.data.objects.remove(o)
    mesh.parent = None
    mesh.matrix_world = Matrix.Identity(4)
    # knife mesh axes (Blender import): +Y blade, +X spine, Z flat normal; grip point 3.5 cm (unscaled) behind the guard
    Kr = np.column_stack([(0, 1, 0), (1, 0, 0), (0, 0, -1)])          # knife X -> bone Y (spine), knife Y -> bone X
    grip = np.array([0.0, -0.035, 0.0]) * KNIFE_SCALE
    Mk = m4(Kr * KNIFE_SCALE, -(Kr @ grip))
    mesh.parent = rig
    mesh.parent_type = "BONE"
    mesh.parent_bone = f"weapon_{side}"
    mesh["kg_local"] = Mk.ravel().tolist()
    return mesh


def add_rod(rig, side="r"):
    """SM_KG_FishingRod from the water-props pack on weapon_r (rod X -> bone X, rod Z (up) -> bone Y = spine)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=WATER_PROPS_GLB)
    new = [o for o in bpy.data.objects if o not in before]
    rod = next((o for o in new if o.type == "MESH" and "FishingRod" in o.name), None)
    if rod is None:
        for o in new:
            bpy.data.objects.remove(o)
        return None
    bpy.context.view_layer.update()
    mw = rod.matrix_world.copy()
    rod.data = rod.data.copy()
    rod.data.transform(mw)
    for o in new:
        if o is not rod:
            bpy.data.objects.remove(o)
    rod.parent = None
    rod.matrix_world = Matrix.Identity(4)
    Rr = np.column_stack([(1, 0, 0), (0, 0, -1), (0, 1, 0)])
    grip = np.array([ROD_GRIP, 0.0, 0.0]) * ROD_SCALE
    rod["kg_local"] = m4(Rr * ROD_SCALE, -(Rr @ grip)).ravel().tolist()
    rod.parent = rig
    rod.parent_type = "BONE"
    rod.parent_bone = f"weapon_{side}"
    rod.hide_render = True
    return rod


def place_on_bone(obj, rig, bone):
    pb = rig.pose.bones[bone]
    Mk = np.array(obj["kg_local"]).reshape(4, 4)
    obj.matrix_world = rig.matrix_world @ pb.matrix @ Matrix(Mk.tolist())


def render(path, cam=None):
    sc = bpy.context.scene
    if cam is not None:
        sc.camera = cam
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def preview_clip(rig, clip, act, knife, fpcam, sidecam, tag="", rod=None):
    rig.animation_data.action = act
    sc = bpy.context.scene
    for fr in clip.preview:
        sc.frame_set(fr)
        bpy.context.view_layer.update()
        if rod is not None:
            place_on_bone(rod, rig, "weapon_r")
            rod.hide_render = not clip.name.startswith("rod")
            bpy.context.view_layer.update()
            if clip.name.startswith("rod") and fr == clip.preview[0]:
                tip = np.array(rod.matrix_world) @ np.array([ROD_LEN, 0.0, 0.0, 1.0])
                log(f"  rod tip camera-space {tuple(round(v, 3) for v in tip[:3])} screen "
                    f"({-tip[1] / tip[0] / TAN_H:+.2f}, {tip[2] / tip[0] / TAN_V:+.2f})")
        if knife is not None:
            place_on_bone(knife, rig, "weapon_r")
            knife.hide_render = not clip.name.startswith("knife")
            bpy.context.view_layer.update()
            if fr == clip.preview[0]:
                Hm = np.array(rig.pose.bones["hand_r"].matrix)
                Km = np.array(knife.matrix_world)
                for kp in ((0, -0.20, 0), (0, -0.09, 0), (0, 0.02, 0), (0, 0.30, 0)):
                    wp = Km @ np.array([*kp, 1.0])
                    hl = (np.linalg.inv(Hm) @ wp)[:3]
                    log(f"  knife pt {kp} -> hand-local {tuple(round(v, 3) for v in hl)}")
                kc = (Km @ np.array([0, -0.09, 0, 1.0]))[:3]
                kd = nrm(Km[:3, :3] @ np.array([0, 1.0, 0]))
                for f in ("thumb", "index", "middle", "ring", "pinky"):
                    ds = []
                    for j in (1, 2, 3):
                        pb = rig.pose.bones[f"{f}_0{j}_r"]
                        a = np.array(pb.head)
                        b = np.array(pb.tail)
                        ds.append(round(seg_line_dist(a, b, kc, kd) * 100, 1))
                    log(f"  {f} seg->handle-axis cm {ds}")
                Wb = np.array(rig.pose.bones["weapon_r"].matrix)
                log(f"  weapon_r hand-local {tuple(round(v, 3) for v in (np.linalg.inv(Hm) @ Wb)[:3, 3])} X {tuple(round(v, 2) for v in (np.linalg.inv(Hm) @ Wb)[:3, 0])}")
                log(f"knife at {tuple(round(v, 3) for v in knife.matrix_world.translation)} dims {tuple(round(v, 3) for v in knife.dimensions)} "
                    f"hidden {knife.hide_render} bone {tuple(round(v, 3) for v in rig.pose.bones['weapon_r'].matrix.translation)}")
        render(os.path.join(CONCEPT, f"FP2_{clip.name}_f{fr:02d}{tag}.png"), fpcam)


def side_views(rig, knife, sidecam, tag):
    """Close-up of the right hand from three angles (grip / finger checks)."""
    sc = bpy.context.scene
    bpy.context.view_layer.update()
    H = np.array(rig.pose.bones["hand_r"].matrix)
    c = H[:3, 3] + H[:3, 1] * 0.07
    views = {"front": np.array([0.35, 0.05, 0.08]), "top": np.array([-0.05, 0.12, 0.33]), "out": np.array([0.02, -0.33, 0.05]),
             "under": np.array([0.05, 0.10, -0.30])}
    sidecam.data.lens = 50
    for nm, off in views.items():
        pos = c + off
        sidecam.matrix_world = Matrix(m4(look_frame(nrm(c - pos)), pos).tolist())
        render(os.path.join(CONCEPT, f"FP2_hand_{tag}_{nm}.png"), sidecam)
    # zoom from the eye: exactly what the player sees, magnified
    sidecam.data.lens = 120
    sidecam.matrix_world = Matrix(m4(look_frame(nrm(c)), np.zeros(3)).tolist())
    render(os.path.join(CONCEPT, f"FP2_hand_{tag}_eyezoom.png"), sidecam)
    sidecam.data.lens = 50


def look_frame(fwd, up=(0, 0, 1)):
    """Rotation for a Blender camera/light whose -Z looks along fwd."""
    z = -np.asarray(fwd, float)
    x = np.cross(up, z)
    x = nrm(x) if np.linalg.norm(x) > 1e-6 else np.array([1.0, 0, 0])
    return np.column_stack([x, np.cross(z, x), z])


# ================================================================================================================
def analyze():
    """Search natural knife-idle arm configurations: blade forward-left-up, wrist near neutral, forearm visible."""
    global SHOULDER
    rows = []
    tgt_y, tgt_p = float(arg("--ty", 18)), float(arg("--tp", 14))
    import itertools
    for shx, wx, wy, wz, pole in itertools.product(
            (-0.10, -0.18), (0.32, 0.38, 0.44), (-0.10, -0.14, -0.18), (-0.11, -0.15),
            ((-0.2, -0.3, -1.0), (-0.3, -0.7, -0.6), (-0.6, -0.4, -0.7), (-0.1, -0.9, -0.3), (0.2, -0.6, -0.8),
             (-0.5, -0.8, 0.0))):
        SHOULDER = np.array([shx, -0.19, -0.27])
        if True:
            if True:
                if True:
                    for roll in range(0, 181, 15):
                        for dev in (15, 25, 32):
                            for flex in (-25, -10, 5, 20):
                                k = ArmKey((wx, wy, wz), roll=roll, flex=flex, dev=dev, pole=pole)
                                S, E, Ru, Rl, pulled = ik(k)
                                bl, sp = blade_of(k)
                                by = math.degrees(math.atan2(bl[1], bl[0]))
                                bp = math.degrees(math.asin(bl[2]))
                                fy = math.degrees(math.atan2(Rl[1, 1], Rl[0, 1]))
                                fp = math.degrees(math.asin(Rl[2, 1]))
                                # screen position of the elbow (want it off-screen low/right) and the wrist
                                sc = lambda p: (-p[1] / p[0] / 0.7265, p[2] / p[0] / 0.4087) if p[0] > 0.02 else (9, -9)
                                ex, ey = sc(E)
                                wxs, wys = sc(np.array((wx, wy, wz)))
                                A0 = R_ARM.T @ R_HAND_REST
                                H = hand_rot(Rl[:, 1], roll, flex, dev)
                                tw = twist_angle((Rl.T @ H) @ A0.T)
                                err = abs(by - tgt_y) + abs(bp - tgt_p) + 0.15 * abs(flex) + 0.3 * abs(dev - 22) \
                                    + 25 * (sp[2] < 0.3) + 2.0 * max(0, tw - 115) + 2.0 * max(0, -35 - tw) + 30 * (pulled > 0.003) \
                                    + 20 * (abs(wxs) > 0.95 or wys < -1.1)
                                rows.append((err, shx, wx, wy, wz, pole, roll, flex, dev, tw, by, bp, sp[2], fy, fp, ex, ey, wxs, wys))
    SHOULDER = np.array([-0.10, -0.19, -0.27])
    rows.sort(key=lambda r: r[0])
    for r in rows[:30]:
        log("err %.1f sh %.2f W(%.2f %.2f %.2f) pole %s roll %d flex %d dev %d tw %.0f | blade yaw %.0f pitch %.0f spz %.2f | forearm yaw %.0f pitch %.0f | elbow scr (%.2f %.2f) wrist scr (%.2f %.2f)" % r)


def main():
    os.makedirs(PACKED, exist_ok=True)
    os.makedirs(CONCEPT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if STAGE == "analyze":
        analyze()
        return
    build_hand_shapes()
    rig = build_armature()
    mesh, sdf = build_mesh(rig)
    fpcam, sidecam = setup_render(rig)
    knife = add_knife(rig) if os.path.exists(KNIFE_GLB) else None
    rod = add_rod(rig) if os.path.exists(WATER_PROPS_GLB) else None
    clips = build_clips()
    if STAGE == "rest":
        rig.animation_data_create()
        bpy.context.scene.frame_set(0)
        if knife:
            place_on_bone(knife, rig, "weapon_r")
        render(os.path.join(CONCEPT, "FP2_rest_fp.png"), fpcam)
        side_views(rig, knife, sidecam, "rest")
        return
    names = ONLY or list(clips.keys())
    actions = {}
    for nm in names:
        clip = clips[nm]
        actions[nm] = bake_clip(rig, clip)
    log(f"baked {len(actions)} clips")
    if STAGE in ("poses", "all"):
        for nm in names:
            preview_clip(rig, clips[nm], actions[nm], knife, fpcam, sidecam, rod=rod)
        if "knife_idle" in actions:
            rig.animation_data.action = actions["knife_idle"]
            bpy.context.scene.frame_set(0)
            if knife:
                place_on_bone(knife, rig, "weapon_r")
            side_views(rig, knife, sidecam, "idle")
    if DO_EXPORT and STAGE == "all":
        if knife:
            bpy.data.objects.remove(knife)
        if rod:
            bpy.data.objects.remove(rod)
        if not ONLY:   # --only re-exports just those clips: the rig glb and the .blend stay as they are
            p = os.path.join(PACKED, "SK_KG_FPArms2.glb")
            export_glb(p, rig, mesh)
            log("rig:", check_glb(p))
        for nm in names:
            clip = clips[nm]
            p = os.path.join(PACKED, f"fp_arms_{nm}.glb")
            export_glb(p, rig, mesh, actions[nm], clip.frames)
            log(f"{nm}:", check_glb(p))
        if not ONLY:
            bpy.ops.wm.save_as_mainfile(filepath=os.path.join(PACKED, "KG_FPArms2.blend"))
    log("done")


main()
