"""Author the third-person EMOTE clips for the Quaternius villagers (UAL1/UBC skeleton) and export one FBX per clip.

  blender --background --factory-startup --python Tools/Blender/kg_make_emotes.py -- [<out_dir>] [--only Wave,Point]
          [--preview]

Default out dir: Art/Export/Villagers/Emotes -> A_KG_Emote_<Name>.fbx (30 fps, in place, root bone static).
--preview also renders a contact sheet per clip (front row + 3/4 row, skinned SK_KG_Villager_M) to
Art/Concept/Emotes/<Name>_sheet.png.

Pipeline: the UAL1 armature ("Armature", meshes removed) and the export_fbx() settings are taken verbatim from
Tools/Blender/kg_build_villagers.py, so the clips land on /Game/KillGodot/Characters/Villager/SK_KG_Villager_M's
skeleton exactly like the UAL1 clips (UE side: Tools/Unreal/kg_import_emotes.py, import_uniform_scale 100).

Conventions (Blender armature space == world, armature object at identity):
  the character faces -Y (foot -> ball points -Y), its LEFT is +X, its RIGHT is -X, up is +Z.
  ch(out, fwd, up, side) converts character-relative offsets to world ("out" = away from the centre line on
  that side). Rotations: +X bends forward, +Z turns to the character's left, +Y leans the top to the left.

Authoring model: every clip is a set of eased scalar tracks (monotone cubic Hermite = Blender auto-clamped,
cyclic for loops) + a pose function. Each frame: reset to the UAL1 Idle_Loop frame-0 pose, apply the pose function
(additive spine/head rotations, two-bone hinge IK for arms and legs, finger presets) and key every bone. Solving
per frame keeps planted feet and hand contacts exact. All arm/leg parameters blend from the exact idle values, so
one-shots start and end in the idle pose.
"""
import math
import os
import sys
import tempfile

import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = "D:/Kill Godot"
DEFAULT_OUT = f"{ROOT}/Art/Export/Villagers/Emotes"
CONCEPT = f"{ROOT}/Art/Concept/Emotes"
VILLAGERS_PY = f"{ROOT}/Tools/Blender/kg_build_villagers.py"
UAL2 = (f"{ROOT}/Art/Source/Quaternius_UniversalAnimationLibrary2/Universal Animation Library 2[Standard]/"
        "Universal Animation Library 2[Standard]/Unreal-Godot/UAL2_Standard.glb")
FPS = 30

# Reuse compose()/export_fbx()/import_gltf()/UAL1 from the villager builder (module body without its main() call).
_src = open(VILLAGERS_PY, encoding="utf-8").read().rsplit("\nmain()", 1)[0]
KGV = {"__name__": "kg_build_villagers"}
exec(compile(_src, VILLAGERS_PY, "exec"), KGV)

UP = Vector((0, 0, 1))
FWD = Vector((0, -1, 0))
AX_X, AX_Y, AX_Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
FINGERS = ("index", "middle", "ring", "pinky")


def sgn(s):
    return 1.0 if s == "l" else -1.0


def other(s):
    return "l" if s == "r" else "r"


def ch(out, fwd, up, s="r"):
    """Character-relative vector: out = towards side s, fwd = facing direction, up."""
    return Vector((sgn(s) * out, -fwd, up))


def orth(v, axis):
    a = axis.normalized()
    return v - a * v.dot(a)


def signed_angle(a, b, axis):
    return math.atan2(axis.normalized().dot(a.cross(b)), a.dot(b))


def lerp(a, b, w):
    return a + (b - a) * w


def nlerp(a, b, w):
    v = a.normalized() * (1 - w) + b.normalized() * w
    return v.normalized() if v.length > 1e-6 else b.normalized()


def clamp01(x):
    return max(0.0, min(1.0, x))


# ----------------------------------------------------------------------------------------------------------------
# Eased tracks
# ----------------------------------------------------------------------------------------------------------------
class Track:
    """Scalar keys [(frame, value)] with monotone cubic Hermite interpolation (flat at extremes, no overshoot beyond
    the keys - overshoot is authored explicitly). cyclic=True wraps tangents (first and last value must match)."""

    def __init__(self, keys, cyclic=False):
        self.k = sorted(keys)
        self.cyclic = cyclic
        n = len(self.k)
        self.m = [0.0] * n
        for i in range(n):
            if 0 < i < n - 1:
                p, c, nx = self.k[i - 1], self.k[i], self.k[i + 1]
            elif cyclic and n > 2:
                per = self.k[-1][0] - self.k[0][0]
                if i == 0:
                    p = (self.k[-2][0] - per, self.k[-2][1])
                    c, nx = self.k[0], self.k[1]
                else:
                    p, c = self.k[-2], self.k[-1]
                    nx = (self.k[1][0] + per, self.k[1][1])
            else:
                continue
            d0 = (c[1] - p[1]) / max(c[0] - p[0], 1e-6)
            d1 = (nx[1] - c[1]) / max(nx[0] - c[0], 1e-6)
            if d0 * d1 <= 0:
                self.m[i] = 0.0
            else:
                m = (nx[1] - p[1]) / max(nx[0] - p[0], 1e-6)
                lim = 3.0 * min(abs(d0), abs(d1))
                self.m[i] = max(-lim, min(lim, m))

    def __call__(self, t):
        k = self.k
        if t <= k[0][0]:
            return k[0][1]
        if t >= k[-1][0]:
            return k[-1][1]
        for i in range(len(k) - 1):
            t0, v0 = k[i]
            t1, v1 = k[i + 1]
            if t0 <= t <= t1:
                h = t1 - t0
                u = (t - t0) / h
                h00 = 2 * u ** 3 - 3 * u ** 2 + 1
                h10 = u ** 3 - 2 * u ** 2 + u
                h01 = -2 * u ** 3 + 3 * u ** 2
                h11 = u ** 3 - u ** 2
                return h00 * v0 + h10 * h * self.m[i] + h01 * v1 + h11 * h * self.m[i + 1]
        return k[-1][1]


def K(*keys, cyclic=False):
    return Track(list(keys), cyclic)


# ----------------------------------------------------------------------------------------------------------------
# Rig helpers
# ----------------------------------------------------------------------------------------------------------------
HAND_SHAPES = {
    # finger: (curl_01, curl_02, curl_03, spread_01) degrees; thumb: (opposition, flex_01, flex_02, flex_03)
    "open": {"index": (4, 4, 2, 7), "middle": (4, 4, 2, 0), "ring": (4, 4, 2, -6), "pinky": (5, 4, 2, -12),
             "thumb": (0, 0, 5, 5)},
    "flat": {"index": (0, 0, 0, 1), "middle": (0, 0, 0, 0), "ring": (0, 0, 0, -1), "pinky": (0, 0, 0, -2),
             "thumb": (22, 0, 5, 5)},
    "cup": {"index": (12, 12, 8, 3), "middle": (12, 12, 8, 0), "ring": (14, 12, 8, -3), "pinky": (16, 12, 8, -6),
            "thumb": (15, 0, 8, 8)},
    "fist": {"index": (78, 95, 70, 0), "middle": (82, 95, 70, 0), "ring": (86, 95, 70, 0),
             "pinky": (90, 95, 70, 0), "thumb": (45, 10, 30, 40)},
    "point": {"index": (0, 0, 0, 2), "middle": (82, 95, 70, 0), "ring": (86, 95, 70, 0),
              "pinky": (90, 95, 70, 0), "thumb": (50, 10, 35, 40)},
    "thumbout": {"index": (78, 95, 70, 0), "middle": (82, 95, 70, 0), "ring": (86, 95, 70, 0),
                 "pinky": (90, 95, 70, 0), "thumb": (-5, -35, -5, 0)},
    "grip": {"index": (60, 70, 45, 0), "middle": (64, 70, 45, 0), "ring": (68, 70, 45, 0),
             "pinky": (72, 70, 45, 0), "thumb": (35, 5, 20, 20)},
    "claw": {"index": (25, 40, 30, 6), "middle": (25, 40, 30, 0), "ring": (28, 40, 30, -5),
             "pinky": (30, 40, 30, -10), "thumb": (20, 0, 20, 20)},
}


class Rig:
    def __init__(self, arm):
        self.obj = arm
        self.pbs = arm.pose.bones
        self.upd()
        self.base = {pb.name: (pb.location.copy(), pb.rotation_quaternion.copy(), pb.scale.copy()) for pb in self.pbs}
        self.base_m = {pb.name: pb.matrix.copy() for pb in self.pbs}
        self.rest_m = {b.name: b.matrix_local.copy() for b in arm.data.bones}
        self.hinge, self.pole0, self.len, self.twist0 = {}, {}, {}, {}
        self.palm_lo, self.palm_ha = {}, {}
        palm_rest = Vector((0, 0, -1))  # T-pose rest: palms face down, thumbs forward (-Y)
        for s in "lr":
            for a, b, c in ((f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"), (f"thigh_{s}", f"calf_{s}", f"foot_{s}")):
                pa, pb_, pc = (self.base_m[x].translation for x in (a, b, c))
                n = (pb_ - pa).normalized().cross((pc - pb_).normalized()).normalized()
                self.hinge[a] = self.base_m[a].to_3x3().inverted() @ n
                self.hinge[b] = self.base_m[b].to_3x3().inverted() @ n
                self.len[a], self.len[b] = (pb_ - pa).length, (pc - pb_).length
                u = (pc - pa).normalized()
                self.pole0[a] = orth(pb_ - pa, u).normalized()
            lo, ha = f"lowerarm_{s}", f"hand_{s}"
            self.palm_lo[s] = self.rest_m[lo].to_3x3().inverted() @ palm_rest
            self.palm_ha[s] = self.rest_m[ha].to_3x3().inverted() @ palm_rest
            ld = (self.base_m[ha].translation - self.base_m[lo].translation).normalized()
            nat = orth(self.base_m[lo].to_3x3() @ self.palm_lo[s], ld)
            want = orth(self.base_m[ha].to_3x3() @ self.palm_ha[s], ld)
            self.twist0[s] = signed_angle(nat, want, ld)
        self._finger_axes()
        self.shape_cache = {}

    # --- basics -------------------------------------------------------------------------------------------------
    def upd(self):
        bpy.context.view_layer.update()

    def reset(self):
        for pb in self.pbs:
            loc, q, sc = self.base[pb.name]
            pb.location, pb.rotation_quaternion, pb.scale = loc, q, sc
        self.upd()

    def snap(self):
        return {pb.name: (pb.location.copy(), pb.rotation_quaternion.normalized(), pb.scale.copy()) for pb in self.pbs}

    def head(self, name):
        return self.pbs[name].matrix.translation.copy()

    def m3(self, name):
        return self.pbs[name].matrix.to_3x3()

    def ydir(self, name):
        return (self.m3(name) @ AX_Y).normalized()

    def set_m(self, name, m):
        self.pbs[name].matrix = m
        self.upd()

    def rot(self, name, axis, deg, pivot=None):
        if abs(deg) < 1e-6:
            return
        if axis.length < 1e-8:
            return
        m = self.pbs[name].matrix.copy()
        p = m.translation.copy() if pivot is None else pivot
        r = Matrix.Rotation(math.radians(deg), 4, axis.normalized())
        self.set_m(name, Matrix.Translation(p) @ r @ Matrix.Translation(-p) @ m)

    def set_frame(self, name, ydir, ref_local, ref_world, pos=None):
        a1 = AX_Y.copy()
        b1 = orth(ref_local, a1).normalized()
        c1 = a1.cross(b1)
        a2 = ydir.normalized()
        b2 = orth(ref_world, a2)
        if b2.length < 1e-6:
            b2 = orth(self.m3(name) @ ref_local, a2)
        b2.normalize()
        c2 = a2.cross(b2)
        loc = Matrix((a1, b1, c1)).transposed()
        wld = Matrix((a2, b2, c2)).transposed()
        m = (wld @ loc.transposed()).to_4x4()
        m.translation = self.head(name) if pos is None else pos
        self.set_m(name, m)

    def pt(self, bone, base_point):
        """World position of a point rigidly attached to `bone` (given where it is in the idle pose)."""
        return self.pbs[bone].matrix @ (self.base_m[bone].inverted() @ base_point)

    def vec(self, bone, base_vec):
        return self.m3(bone) @ (self.base_m[bone].to_3x3().inverted() @ base_vec)

    # --- body ---------------------------------------------------------------------------------------------------
    def hips(self, fwd=0.0, up=0.0, left=0.0, pitch=0.0, yaw=0.0, roll=0.0):
        """Pelvis offset (m, character axes) + rotation (deg): pitch>0 tips the top forward, yaw>0 turns left,
        roll>0 tips the top to the left. Call plant() afterwards for full-body clips."""
        pb = self.pbs["pelvis"]
        m = pb.matrix.copy()
        p = m.translation.copy()
        r = (Matrix.Rotation(math.radians(yaw), 4, AX_Z) @ Matrix.Rotation(math.radians(roll), 4, AX_Y)
             @ Matrix.Rotation(math.radians(pitch), 4, AX_X))
        m = Matrix.Translation(p + Vector((left, -fwd, up))) @ r @ Matrix.Translation(-p) @ m
        self.set_m("pelvis", m)

    def spine(self, fwd=0.0, twist=0.0, lean=0.0, w=(0.3, 0.35, 0.35)):
        for name, k in zip(("spine_01", "spine_02", "spine_03"), w):
            self.rot(name, AX_X, fwd * k)
            self.rot(name, AX_Z, twist * k)
            self.rot(name, AX_Y, lean * k)

    def look(self, yaw=0.0, pitch=0.0, roll=0.0, neck=0.4):
        for name, k in (("neck_01", neck), ("Head", 1.0 - neck)):
            self.rot(name, AX_X, pitch * k)
            self.rot(name, AX_Z, yaw * k)
            self.rot(name, AX_Y, roll * k)

    def clav(self, s, up=0.0, fwd=0.0):
        name = f"clavicle_{s}"
        d = self.ydir(name)
        if up:
            self.rot(name, d.cross(UP), up)
        if fwd:
            self.rot(name, self.ydir(name).cross(FWD), fwd)

    def shoulder(self, s):
        return self.head(f"upperarm_{s}")

    # --- two-bone hinge IK --------------------------------------------------------------------------------------
    def _two_bone(self, a, b, S, H, pole):
        l1, l2 = self.len[a], self.len[b]
        d = H - S
        dist = max(0.04, min(d.length, (l1 + l2) * 0.9995))
        u = d.normalized()
        v = orth(pole, u)
        if v.length < 1e-5:
            v = orth(UP if abs(u.z) < 0.9 else FWD, u)
        v.normalize()
        x = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
        h = math.sqrt(max(l1 * l1 - x * x, 0.0))
        E = S + u * x + v * h
        Hn = S + u * dist
        n = v.cross(u).normalized()
        self.set_frame(a, E - S, self.hinge[a], n)
        self.set_frame(b, Hn - E, self.hinge[b], n)
        return E, Hn

    def idle_arm(self, s):
        up, lo, ha = f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"
        return {"H": self.base_m[ha].translation.copy(), "pole": self.pole0[up].copy(),
                "fingers": (self.base_m[ha].to_3x3() @ AX_Y).normalized(),
                "palm": (self.base_m[ha].to_3x3() @ self.palm_ha[s]).normalized()}

    def arm(self, s, w, H, pole, fingers=None, palm=None):
        """Blend the arm from idle (w=0, exact) to a wrist target H with elbow pole direction, hand orientation given
        by finger direction + palm normal (world). w may overshoot 1 slightly. Idle values follow the torso."""
        up, lo, ha = f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"
        idle = self.idle_arm(s)
        # idle values ride on the (possibly bent) chest so a half-raised arm follows the torso
        ref = "spine_03"
        iH = self.pt(ref, idle["H"])
        ipole, ifing, ipalm = (self.vec(ref, idle[k]) for k in ("pole", "fingers", "palm"))
        if w <= 1e-4:
            return  # untouched locals: the idle arm rides on the torso
        tH = lerp(iH, H, w)
        tpole = nlerp(ipole, pole, clamp01(w))
        tf = nlerp(ifing, fingers, clamp01(w)) if fingers is not None else None
        tp = nlerp(ipalm, palm, clamp01(w)) if palm is not None else None
        E, Hn = self._two_bone(up, lo, self.shoulder(s), tH, tpole)
        ld = (Hn - E).normalized()
        if tf is None:
            tf = ld
        if tp is None:
            tp = self.m3(lo) @ self.palm_lo[s]
        nat = orth(self.m3(lo) @ self.palm_lo[s], ld)
        want = orth(tp, ld)
        if nat.length > 1e-5 and want.length > 1e-5:
            th = signed_angle(nat, want, ld) - self.twist0[s]
            th = (th + math.pi) % (2 * math.pi) - math.pi
            th = max(-math.radians(150), min(math.radians(150), th))
            self.rot(lo, ld, math.degrees(th))
        self.set_frame(ha, tf, self.palm_ha[s], tp)

    def leg(self, s, A=None, pole=None, foot=None):
        """Leg IK to ankle A (default: idle, i.e. planted) with knee pole and foot world rotation (3x3)."""
        th, ca, fo = f"thigh_{s}", f"calf_{s}", f"foot_{s}"
        A = self.base_m[fo].translation.copy() if A is None else A
        if pole is None:
            pole = self.vec("pelvis", self.pole0[th])
        self._two_bone(th, ca, self.head(th), A, pole)
        rot = self.base_m[fo].to_3x3() if foot is None else foot
        m = rot.to_4x4()
        m.translation = self.head(fo)
        self.set_m(fo, m)

    def plant(self):
        self.leg("l")
        self.leg("r")

    # --- fingers ------------------------------------------------------------------------------------------------
    def _finger_axes(self):
        self.fax = {}
        palm = Vector((0, 0, -1))
        thumb_side = Vector((0, -1, 0))
        for s in "lr":
            hand_r = self.rest_m[f"hand_{s}"]
            hdir = (hand_r.to_3x3() @ AX_Y).normalized()
            pc = hand_r.translation + hdir * 0.075 + palm * 0.03
            for f in FINGERS + ("thumb",):
                for i in (1, 2, 3):
                    name = f"{f}_0{i}_{s}"
                    r = self.rest_m[name]
                    inv = r.to_3x3().inverted()
                    d = (r.to_3x3() @ AX_Y).normalized()
                    if f == "thumb":
                        flex = d.cross(pc - r.translation).normalized()
                        tgt = self.rest_m[f"middle_01_{s}"].translation + palm * 0.02 - r.translation
                        opp = d.cross(tgt).normalized()
                        self.fax[name] = (inv @ flex, inv @ opp)
                    else:
                        curl = d.cross(palm).normalized()
                        spread = d.cross(thumb_side).normalized()
                        self.fax[name] = (inv @ curl, inv @ spread)

    def shape_quats(self, s, shape):
        key = (s, shape)
        if key in self.shape_cache:
            return self.shape_cache[key]
        out = {}
        if shape == "relaxed":
            for f in FINGERS + ("thumb",):
                for i in (1, 2, 3):
                    n = f"{f}_0{i}_{s}"
                    out[n] = self.base[n][1].copy()
        else:
            spec = HAND_SHAPES[shape]
            for f in FINGERS:
                c1, c2, c3, sp = spec[f]
                for i, c in zip((1, 2, 3), (c1, c2, c3)):
                    n = f"{f}_0{i}_{s}"
                    ca, sa = self.fax[n]
                    q = Quaternion(ca, math.radians(c))
                    if i == 1:
                        q = Quaternion(sa, math.radians(sp)) @ q
                    out[n] = q
            opp, f1, f2, f3 = spec["thumb"]
            for i, fl in zip((1, 2, 3), (f1, f2, f3)):
                n = f"thumb_0{i}_{s}"
                fa, oa = self.fax[n]
                q = Quaternion(fa, math.radians(fl))
                if i == 1:
                    q = Quaternion(oa, math.radians(opp)) @ q
                out[n] = q
        self.shape_cache[key] = out
        return out

    def hand(self, s, shape, w=1.0, frm="relaxed"):
        a = self.shape_quats(s, frm)
        b = self.shape_quats(s, shape)
        w = clamp01(w)
        for n, qa in a.items():
            qb = b[n]
            if qa.dot(qb) < 0:
                qb = -qb
            self.pbs[n].rotation_quaternion = qa.slerp(qb, w)
        self.upd()

    # --- hand placement helpers ---------------------------------------------------------------------------------
    @staticmethod
    def wrist_for_palm(P, fingers, palm_n, reach=0.075, thick=0.02):
        """Wrist position so that the palm centre touches P (palm_n = direction the palm faces)."""
        return P - fingers.normalized() * reach - palm_n.normalized() * thick


# ----------------------------------------------------------------------------------------------------------------
# Landmarks on the villager in the idle pose (world, m) - measured from SK_KG_Villager_M
# ----------------------------------------------------------------------------------------------------------------
FACE = Vector((-0.02, -0.15, 1.60))
FOREHEAD = Vector((-0.02, -0.145, 1.67))
BROW_R = Vector((-0.085, -0.135, 1.655))
THROAT = Vector((-0.02, -0.095, 1.46))
HEART = Vector((0.06, -0.10, 1.30))
BELLY = Vector((0.0, -0.08, 1.06))
LOWBACK = Vector((0.0, 0.17, 1.03))


# ----------------------------------------------------------------------------------------------------------------
# Clips. Each returns (frames, cyclic, pose_fn(rig, t), preview_frames)
# ----------------------------------------------------------------------------------------------------------------
def clip_wave():
    s = "r"
    N = 72
    up = K((0, 0), (3, -0.05), (11, 1.07), (15, 1), (46, 1), (56, 0.35), (64, 0), (72, 0))
    wav = K((0, 0), (12, 0), (17, -1), (23, 1), (29, -1), (35, 1), (41, -1), (47, 0.2), (54, 0), (72, 0))
    body = K((0, 0), (12, 1), (48, 1), (62, 0), (72, 0))

    def pose(r, t):
        b, u = body(t), up(t)
        r.spine(fwd=-3 * b, lean=5 * b, twist=-4 * b)
        r.look(pitch=-5 * b, roll=-9 * b, yaw=-4 * b)
        r.clav(s, up=16 * clamp01(u))
        S = r.shoulder(s)
        a = math.radians(24) * wav(t)
        fdir = ch(math.sin(a), 0.10, math.cos(a), s).normalized()
        E = S + ch(0.25, 0.08, -0.06, s)
        H = E + fdir * r.len[f"lowerarm_{s}"]
        fing = ch(math.sin(a * 1.4) + 0.05, 0.05, math.cos(a * 1.4), s).normalized()
        r.arm(s, u, H, pole=E - S, fingers=fing, palm=FWD)
        r.hand(s, "open", clamp01(u * 1.3))
    return N, False, pose, (0, 11, 17, 23, 35, 56)


def clip_point():
    s = "r"
    N = 60
    up = K((0, 0), (4, 0.35), (11, 1.08), (15, 1), (46, 1), (54, 0.2), (58, 0), (60, 0))
    cock = K((0, 0), (4, 1), (9, 0), (60, 0))       # anticipation: hand pulled back to the shoulder
    body = K((0, 0), (5, -0.4), (11, 1.1), (15, 1), (46, 1), (56, 0), (60, 0))

    def pose(r, t):
        b, u, c = body(t), up(t), cock(t)
        r.spine(fwd=5 * b, twist=10 * b)
        r.look(pitch=-3 * b, yaw=-4 * b)
        r.clav(s, up=4 * clamp01(u), fwd=12 * clamp01(u))
        S = r.shoulder(s)
        H = S + ch(0.03, 0.52, 0.03, s)
        H = lerp(H, S + ch(0.12, 0.18, -0.12, s), c)
        pole = lerp(ch(0.35, 0.0, -1.0, s), ch(1.0, -0.4, -0.6, s), c)
        r.arm(s, u, H, pole=pole, fingers=FWD + ch(0, 0, 0.05, s), palm=ch(-0.7, 0, -0.7, s))
        r.hand(s, "point", clamp01(u * 1.4))
    return N, False, pose, (0, 4, 11, 30, 50, 60)


def clip_clap():
    N = 72
    up = K((0, 0), (8, 1), (50, 1), (62, 0), (72, 0))
    claps = (12, 19, 26, 33, 40)
    keys = [(0, 1.0), (8, 1.0)]
    for i, c in enumerate(claps):
        keys += [(c, 0.0), (c + 3.5 if i < len(claps) - 1 else c + 5, 1.0)]
    keys += [(72, 1.0)]
    gap = K(*keys)
    body = K((0, 0), (10, 1), (46, 1), (60, 0), (72, 0))

    def pose(r, t):
        b, u, g = body(t), up(t), gap(t)
        bounce = (1 - g) * clamp01(u)
        r.spine(fwd=-3 * b + 2 * bounce, lean=0)
        r.look(pitch=-4 * b + 3 * bounce)
        for s in "lr":
            r.clav(s, up=6 * clamp01(u) - 3 * bounce, fwd=6 * clamp01(u))
            M = Vector((0.0, -0.33, 1.20 - 0.03 * bounce))
            fing = (ch(-0.12 - 0.35 * g, 0.55, 0.8, s)).normalized()
            palm = ch(-1.0, 0.25 * g, 0, s).normalized()
            P = M + ch(0.012 + 0.16 * g, 0.02 * g, 0.02 * g, s)
            H = Rig.wrist_for_palm(P, fing, palm, reach=0.07, thick=0.018)
            r.arm(s, u, H, pole=ch(1.0, -0.2, -0.8 + 0.2 * bounce, s), fingers=fing, palm=palm)
            r.hand(s, "cup", clamp01(u * 1.3))
    return N, False, pose, (0, 8, 12, 16, 26, 62)


def clip_cheer():
    N = 66
    # 0 = idle, 1 = fists overhead (arms straight), mid 0.5 = fists at head height (pump down)
    lift = K((0, 0), (5, 0.18), (12, 1.0), (18, 0.55), (25, 1.0), (40, 1.0), (47, 0.62), (58, 0), (66, 0))
    act = K((0, 0), (5, 0.5), (11, 1), (48, 1), (58, 0), (66, 0))
    body = K((0, 0), (5, -0.5), (12, 1.1), (18, 0.7), (25, 1.1), (40, 1), (54, 0), (66, 0))

    def pose(r, t):
        l, a, b = lift(t), act(t), body(t)
        r.spine(fwd=-9 * b)
        r.look(pitch=-16 * b)
        for s in "lr":
            r.clav(s, up=22 * clamp01(l))
            S = r.shoulder(s)
            top = S + ch(0.15, 0.07, 0.51, s)
            mid = S + ch(0.24, 0.10, 0.20, s)
            low = S + ch(0.10, 0.22, -0.18, s)
            if l >= 0.55:
                H = lerp(mid, top, (l - 0.55) / 0.45)
            else:
                H = lerp(low, mid, clamp01(l / 0.55))
            fing = nlerp(ch(0.05, 0.4, 1.0, s), ch(0.08, 0.05, 1.0, s), clamp01(l))
            r.arm(s, a, H, pole=ch(1.0, -0.15, -0.35, s), fingers=fing, palm=ch(-0.3, 1.0, 0.0, s))
            r.hand(s, "fist", clamp01(a * 1.5))
    return N, False, pose, (0, 5, 12, 18, 25, 47)


def clip_shrug():
    N = 54
    up = K((0, 0), (8, 1.1), (12, 1), (32, 1), (44, 0), (54, 0))
    body = K((0, 0), (9, 1.05), (13, 1), (32, 1), (46, 0), (54, 0))

    def pose(r, t):
        u, b = up(t), body(t)
        r.spine(fwd=-3 * b, lean=2 * b)
        r.look(pitch=-6 * b, roll=13 * b, yaw=4 * b)
        for s in "lr":
            r.clav(s, up=24 * u, fwd=5 * u)
            S = r.shoulder(s)
            E = S + ch(0.05, 0.04, -0.265, s)
            fore = ch(0.55, 0.80, 0.12, s).normalized()
            H = E + fore * r.len[f"lowerarm_{s}"]
            r.arm(s, clamp01(u), H, pole=ch(0.3, -0.35, -1.0, s), fingers=ch(0.75, 0.6, 0.08, s), palm=UP)
            r.hand(s, "open", clamp01(u * 1.3))
    return N, False, pose, (0, 8, 12, 24, 40, 54)


def clip_laugh():
    N = 78
    back = K((0, 0), (9, 1.0), (15, 0.85), (22, 0.0), (58, 0), (64, 0.5), (72, 0), (78, 0))
    fold = K((0, 0), (15, 0), (24, 1.0), (50, 0.9), (60, 0.0), (78, 0))
    act = K((0, 0), (7, 1), (64, 1), (74, 0), (78, 0))
    # slap: 0 = hand raised, 1 = on the thigh
    slap = K((0, 0), (22, 0), (25, 1), (29, 0), (33, 1), (37, 0), (41, 1), (45, 0), (49, 1), (54, 0), (78, 0))
    bounce_keys = [(0, 0)] + [(f, (1 if i % 2 == 0 else -1)) for i, f in enumerate(range(6, 60, 4))] + [(64, 0), (78, 0)]
    shake = K(*bounce_keys)

    def pose(r, t):
        bk, fd, a, sh = back(t), fold(t), act(t), shake(t) * act(t)
        r.hips(fwd=-0.05 * fd + 0.015 * bk, up=-0.06 * fd - 0.01 * bk, pitch=14 * fd - 5 * bk)
        r.plant()
        r.spine(fwd=26 * fd - 22 * bk + 1.5 * sh)
        r.look(pitch=14 * fd - 30 * bk - 4 * sh, roll=-4 * fd)
        for s in "lr":
            r.clav(s, up=5 * a + 5 * sh)
        # left hand on the belly
        s = "l"
        P = r.pt("spine_01", BELLY + ch(0.02, 0, 0, "l"))
        fing = r.vec("spine_01", ch(-0.85, 0.0, -0.35, "l"))
        palm = r.vec("spine_01", Vector((0, 1, 0)))
        r.arm(s, a, Rig.wrist_for_palm(P, fing, palm), pole=r.vec("spine_01", ch(1.0, -0.2, -0.5, s)),
              fingers=fing, palm=palm)
        r.hand(s, "cup", a)
        # right hand slaps the right thigh
        s = "r"
        th = "thigh_r"
        P0 = r.head(th).lerp(r.head("calf_r"), 0.55)
        thigh_front = r.vec(th, Vector((0, -1, 0)))
        on = P0 + thigh_front * 0.085
        sl = slap(t)
        P = lerp(on + thigh_front * 0.10 + UP * 0.14, on, sl)
        fing_s, palm_s = ch(-0.1, 0.2, -1.0, s).normalized(), -thigh_front
        Hs = Rig.wrist_for_palm(P, fing_s, palm_s)
        # while leaning back the right hand also holds the belly
        Pb = r.pt("spine_01", BELLY + ch(0.07, 0.0, 0.07, s))
        fing_b = r.vec("spine_01", ch(-0.85, 0.0, -0.3, s)).normalized()
        palm_b = r.vec("spine_01", Vector((0, 1, 0)))
        Hb = Rig.wrist_for_palm(Pb, fing_b, palm_b)
        r.arm(s, a, lerp(Hs, Hb, bk), pole=ch(1.0, -0.3, -0.5, s), fingers=nlerp(fing_s, fing_b, bk),
              palm=nlerp(palm_s, palm_b, bk))
        r.hand(s, "open", a)
    return N, False, pose, (0, 9, 25, 29, 41, 64)


def clip_facepalm():
    s = "r"
    N = 66
    up = K((0, 0), (5, 0.4), (11, 1.04), (14, 1), (46, 1), (56, 0.15), (62, 0), (66, 0))
    dip = K((0, 0), (10, 0), (15, 1.1), (18, 1), (46, 1), (56, 0), (66, 0))
    shake = K((0, 0), (18, 0), (24, 1), (30, -1), (36, 1), (42, -0.5), (46, 0), (66, 0))

    def pose(r, t):
        u, d, sh = up(t), dip(t), shake(t)
        r.spine(fwd=8 * d)
        r.look(pitch=16 * d, yaw=7 * sh, roll=-3 * d)
        r.clav(s, up=6 * clamp01(u), fwd=10 * clamp01(u))
        fing = r.vec("Head", ch(-0.35, 0.0, 1.0, s).normalized())
        palm = r.vec("Head", Vector((0, 1, 0)))
        P = r.pt("Head", FACE + Vector((0.0, 0, 0.035)))
        H = Rig.wrist_for_palm(P, fing, palm, reach=0.07, thick=0.02)
        r.arm(s, u, H, pole=ch(0.6, 0.1, -1.0, s), fingers=fing, palm=palm)
        r.hand(s, "flat", clamp01(u * 1.2))
    return N, False, pose, (0, 5, 11, 18, 30, 56)


def clip_reel():
    N = 60
    beat = K(*[(f, (0.0 if i % 2 == 0 else 1.0)) for i, f in enumerate((0, 7.5, 15, 22.5, 30, 37.5, 45, 52.5, 60))],
             cyclic=True)  # 1 = knees bent (down beat)
    # phase: 0 reel pose, 1 wind-up overhead, 2 cast forward, 3 follow-through, back to 0 (reel) at 30
    cast = K((0, 0), (8, 1), (14, 2), (21, 3), (30, 4), (60, 4), cyclic=False)
    crank = K((30, 0), (60, 2), cyclic=False)
    sway = K((0, 0), (15, 1), (30, 0), (45, -1), (60, 0), cyclic=True)

    def arm_targets(r, ph, t):
        # returns per phase: (H_r, H_l, torso twist, torso fwd)
        c = math.tau * crank(t) if t >= 30 else 0.0
        K_r = Vector((-0.24, -0.25, 1.02))
        reel_r = K_r + (UP * math.cos(c) + FWD * math.sin(c)) * 0.10
        poses = [
            (reel_r, Vector((0.02, -0.28, 1.06)), 0.0, 6.0),                 # 0 reeling at the hip
            (Vector((-0.36, 0.04, 1.80)), Vector((-0.30, -0.10, 1.58)), -18.0, -6.0),  # 1 wind-up over right shoulder
            (Vector((-0.12, -0.52, 1.52)), Vector((-0.02, -0.40, 1.40)), 6.0, 12.0),   # 2 cast forward
            (Vector((-0.12, -0.46, 1.25)), Vector((-0.01, -0.36, 1.16)), 4.0, 10.0),   # 3 follow-through
            (reel_r, Vector((0.02, -0.28, 1.06)), 0.0, 6.0),                 # 4 == 0
        ]
        i = min(int(ph), 3)
        f = ph - i
        a, b = poses[i], poses[i + 1]
        return lerp(a[0], b[0], f), lerp(a[1], b[1], f), lerp(a[2], b[2], f), lerp(a[3], b[3], f)

    def pose(r, t):
        bt, sw = beat(t), sway(t)
        r.hips(up=-0.07 * bt, left=0.03 * sw, roll=-3 * sw, pitch=6 * bt, yaw=4 * sw)
        r.plant()
        Hr, Hl, tw, fw = arm_targets(r, cast(t), t)
        r.spine(fwd=fw - 4 * bt, twist=tw, lean=2 * sw)
        r.look(pitch=-6 + 8 * bt, yaw=-0.3 * tw, roll=4 * sw)
        for s, H in (("r", Hr), ("l", Hl)):
            r.clav(s, up=6 + 6 * max(0.0, (H.z - 1.3) / 0.4))
            pole = ch(1.0, -0.3, -0.6, s)
            if s == "r":
                palm_v = ch(-1.0, 0.0, -0.2, s)
                fing = ch(-0.3, 0.9, -0.2, s)
            else:
                palm_v = ch(-1.0, 0.0, 0.2, s)
                fing = ch(-0.2, 0.9, 0.2, s)
            r.arm(s, 1.0, H, pole=pole, fingers=fing.normalized(), palm=palm_v)
            r.hand(s, "grip", 1.0)
    return N, True, pose, (0, 8, 14, 21, 38, 52)


# Fishing (Source/KillGodot/Fishing): upper-body layers the game plays while a villager holds / casts / reels a rod.
# The rod itself is placed by the game at hand_r and aimed from the fishing state; these clips only pose the arms.
FISH_R_HOLD = Vector((-0.20, -0.32, 1.10))
FISH_L_HOLD = Vector((0.00, -0.30, 1.02))


def fish_arms(r, Hr, Hl, wl=1.0):
    for s, H in (("r", Hr), ("l", Hl)):
        r.clav(s, up=6 + 6 * max(0.0, (H.z - 1.3) / 0.4))
        if s == "r":
            palm_v, fing = ch(-1.0, 0.0, -0.2, s), ch(-0.3, 0.9, -0.2, s)
        else:
            palm_v, fing = ch(-1.0, 0.0, 0.2, s), ch(-0.2, 0.9, 0.2, s)
        r.arm(s, 1.0 if s == "r" else wl, H, pole=ch(1.0, -0.3, -0.6, s), fingers=fing.normalized(), palm=palm_v)
        r.hand(s, "grip", 1.0 if s == "r" else wl)


def clip_fish_hold():
    """Loop (3 s): rod held out in front, left hand on the butt, watching the float, breathing."""
    N = 90
    br = K((0, 0), (22.5, 1), (45, 0), (67.5, -1), (90, 0), cyclic=True)

    def pose(r, t):
        b = br(t)
        r.spine(fwd=6 + 1.5 * b, twist=-4)
        r.look(pitch=8 + 1.5 * b, yaw=3)
        fish_arms(r, FISH_R_HOLD + UP * (0.008 * b), FISH_L_HOLD + UP * (0.006 * b))
    return N, True, pose, (0, 45)


def clip_fish_cast():
    """One-shot (0.7 s): wind up over the right shoulder, whip forward, follow through, back to the hold."""
    N = 21
    ph = K((0, 0), (7, 1), (11, 2), (15, 3), (21, 4), cyclic=False)
    poses = [
        (FISH_R_HOLD, FISH_L_HOLD, -4.0, 6.0),
        (Vector((-0.36, 0.04, 1.80)), Vector((-0.30, -0.10, 1.58)), -18.0, -6.0),
        (Vector((-0.12, -0.52, 1.52)), Vector((-0.02, -0.40, 1.40)), 6.0, 12.0),
        (Vector((-0.12, -0.46, 1.25)), Vector((-0.01, -0.36, 1.16)), 4.0, 10.0),
        (FISH_R_HOLD, FISH_L_HOLD, -4.0, 6.0),
    ]

    def pose(r, t):
        p = min(max(ph(t), 0.0), 4.0)
        i = min(int(p), 3)
        f = p - i
        a, b = poses[i], poses[i + 1]
        r.spine(fwd=lerp(a[3], b[3], f), twist=lerp(a[2], b[2], f))
        r.look(pitch=6.0 - 4.0 * math.sin(math.pi * min(p, 2.0) / 2.0))
        fish_arms(r, lerp(a[0], b[0], f), lerp(a[1], b[1], f))
    return N, False, pose, (0, 7, 11, 15, 21)


def clip_fish_reel():
    """Loop (0.6 s): rod up and steady-ish, the left hand cranks the reel in circles."""
    N = 18
    crank = K((0, 0), (18, 1), cyclic=False)

    def pose(r, t):
        c = math.tau * crank(t)
        r.spine(fwd=9, twist=-3)
        r.look(pitch=5)
        Hr = Vector((-0.20, -0.36, 1.16)) + UP * (0.006 * math.sin(2 * c))
        Hl = Vector((-0.08, -0.30, 1.05)) + (UP * math.cos(c) + FWD * math.sin(c)) * 0.06
        fish_arms(r, Hr, Hl)
    return N, True, pose, (0, 4, 9, 13)


# Sit: pelvis head ~0.14 m above the floor, knees up, forearms resting on the knees.


SIT_Z = 0.14            # seated pelvis head height (m)
SIT_BACK = 0.08         # pelvis moves back (m) so the knees/feet stay around the root
SIT_PITCH = -14.0       # pelvis rolls back onto the buttocks
SIT_TORSO = 22.0        # spine leans forward over the knees


def sit_pose(r, drop=1.0, back=SIT_BACK, feet=1.0, pitch=SIT_PITCH, torso=SIT_TORSO, breathe=0.0, look=0.0,
             nod=0.0):
    """drop 0..1 lowers the pelvis from idle to SIT_Z, feet 0..1 moves the ankles from idle to the seated spot."""
    pel0 = r.base_m["pelvis"].translation
    r.hips(fwd=-back, up=(SIT_Z - pel0.z) * drop, pitch=pitch)
    for s in "lr":
        fo = f"foot_{s}"
        A0 = r.base_m[fo].translation
        A1 = Vector((sgn(s) * 0.19, -0.40, 0.105))
        A = lerp(A0, A1, feet)
        # knees: forward while crouching, up/out when seated
        pole = nlerp(r.vec("pelvis", r.pole0[f"thigh_{s}"]), ch(0.35, 0.35, 1.0, s), clamp01(max(feet, drop * 0.6)))
        base = r.base_m[fo].to_3x3()
        by = base @ AX_Y
        face = signed_angle(Vector((by.x, by.y, 0)).normalized(), FWD, AX_Z) + math.radians(sgn(s) * 10)
        seated = Matrix.Rotation(face, 3, AX_Z) @ base
        q = base.to_quaternion().slerp(seated.to_quaternion(), clamp01(feet))
        r.leg(s, A, pole=pole, foot=q.to_matrix())
    r.spine(fwd=torso + 1.5 * breathe)
    r.look(pitch=-10 * clamp01(feet) + nod, yaw=look)
    for s in "lr":
        r.clav(s, up=2.5 * breathe)


def sit_arms(r, amount):
    """Forearms draped over the knees, hands hanging in front of them."""
    for s in "lr":
        knee = r.head(f"calf_{s}")
        H = knee + ch(-0.02, 0.13, -0.05, s)
        fing = ch(-0.15, 0.35, -1.0, s).normalized()
        palm = ch(-1.0, 0.0, 0.0, s)
        r.arm(s, amount, H, pole=ch(0.5, -0.4, -1.0, s), fingers=fing, palm=palm)


def clip_sit_enter():
    N = 27
    drop = K((0, 0), (7, 0.42), (15, 1.03), (19, 0.98), (27, 1.0))
    back = K((0, 0), (7, 0.13), (15, SIT_BACK + 0.01), (27, SIT_BACK))
    feet = K((0, 0), (9, 0.0), (15, 1.0), (27, 1.0))
    pitch = K((0, 0), (7, 6.0), (15, SIT_PITCH - 2), (19, SIT_PITCH + 1), (27, SIT_PITCH))
    torso = K((0, 0), (7, 30.0), (15, 12.0), (20, SIT_TORSO + 3), (27, SIT_TORSO))
    arms = K((0, 0), (7, 0.55), (15, 0.85), (22, 1.0), (27, 1.0))

    def pose(r, t):
        sit_pose(r, drop(t), back(t), feet(t), pitch(t), torso(t))
        sit_arms(r, arms(t))
    return N, False, pose, (0, 5, 9, 13, 18, 27)


def clip_sit_loop():
    N = 90
    br = K((0, 0), (22, 1), (45, 0), (67, 1), (90, 0), cyclic=True)
    lk = K((0, 0), (15, 0), (32, 28), (50, 28), (65, -20), (80, -20), (90, 0), cyclic=True)
    nd = K((0, 0), (32, 3), (65, -2), (90, 0), cyclic=True)

    def pose(r, t):
        sit_pose(r, breathe=br(t), look=lk(t), nod=nd(t))
        sit_arms(r, 1.0)
    return N, True, pose, (0, 22, 40, 58, 72, 90)


def clip_bow():
    N = 60
    hands = K((0, 0), (9, 1), (50, 1), (58, 0), (60, 0))
    bow = K((0, 0), (8, 0), (20, 1.1), (24, 1.0), (38, 1.0), (48, -0.05), (52, 0), (60, 0))

    def pose(r, t):
        h, b = hands(t), bow(t)
        r.hips(fwd=-0.09 * b, up=-0.015 * b, pitch=20 * b)
        r.plant()
        r.spine(fwd=25 * b)
        r.look(pitch=8 * b)
        # right palm on the heart
        s = "r"
        P = r.pt("spine_03", HEART)
        fing = r.vec("spine_03", Vector((1.0, 0.0, 0.25)).normalized())
        palm = r.vec("spine_03", Vector((0, 1, 0)))
        r.clav(s, fwd=6 * h)
        r.arm(s, h, Rig.wrist_for_palm(P, fing, palm, reach=0.07, thick=0.02), pole=r.vec("spine_03", ch(1, 0.1, -0.8, s)),
              fingers=fing, palm=palm)
        r.hand(s, "flat", h)
        # left hand behind the back (back of the hand on the lower back)
        s = "l"
        P = r.pt("spine_01", LOWBACK + Vector((0.02, 0, 0)))
        fing = r.vec("spine_01", Vector((-1.0, 0.0, 0.1)).normalized())
        palm = r.vec("spine_01", Vector((0, 1, 0)))
        H = P - fing * 0.07 + palm * 0.025
        r.arm(s, h, H, pole=r.vec("spine_01", ch(1.0, -0.6, -0.4, s)), fingers=fing, palm=palm)
        r.hand(s, "cup", h)
    return N, False, pose, (0, 9, 20, 30, 48, 56)


def clip_salute():
    s = "r"
    N = 60
    up = K((0, 0), (3, -0.05), (8, 1.05), (11, 1), (40, 1), (45, -0.06), (49, 0.02), (55, 0), (60, 0))
    body = K((0, 0), (8, 1), (42, 1), (52, 0), (60, 0))

    def pose(r, t):
        u, b = up(t), body(t)
        r.spine(fwd=-4 * b)
        r.look(pitch=-5 * b)
        r.clav(s, up=10 * clamp01(u), fwd=6 * clamp01(u))
        B = r.pt("Head", BROW_R)
        fing = ch(-0.8, 0.12, 0.55, s).normalized()
        palm = orth(ch(0.15, 0.35, -1.0, s), fing).normalized()
        H = B - fing * 0.19 - palm * 0.01
        r.arm(s, u, H, pole=ch(1.0, 0.1, -0.35, s), fingers=fing, palm=palm)
        r.hand(s, "flat", clamp01(u * 1.3))
    return N, False, pose, (0, 5, 8, 25, 45, 60)


def clip_sus(r, ual2_action):
    """UAL2 Idle_FoldArms_Loop + slow suspicious head tilt / look-around (seamless: the extra is periodic)."""
    ad = r.obj.animation_data
    N = int(round(ual2_action.frame_range[1] - ual2_action.frame_range[0]))
    f0 = int(round(ual2_action.frame_range[0]))
    keys = []
    for f in range(N + 1):
        ad.action = ual2_action
        if hasattr(ad, "action_slot") and ual2_action.slots:
            ad.action_slot = ual2_action.slots[0]
        bpy.context.scene.frame_set(f0 + f)
        r.upd()
        loc = {pb.name: (pb.location.copy(), pb.rotation_quaternion.copy(), pb.scale.copy()) for pb in r.pbs}
        ad.action = None
        for pb in r.pbs:
            pb.location, pb.rotation_quaternion, pb.scale = loc[pb.name]
        r.upd()
        ph = math.tau * f / N
        r.look(yaw=14 * math.sin(ph), pitch=4 + 3 * math.sin(2 * ph), roll=7 + 3 * math.cos(ph))
        keys.append((f, r.snap()))
    keys[-1] = (N, keys[0][1])  # the source loop's last frame is ~0.5 mm off its first: make it exact
    return N, keys


def clip_cry():
    N = 90
    up = K((0, 0), (5, 0.5), (11, 1.03), (14, 1), (70, 1), (80, 0.15), (86, 0), (90, 0))
    body = K((0, 0), (10, 0.3), (16, 1.0), (70, 1), (82, 0), (90, 0))
    sob_keys = [(0, 0), (14, 0)] + [(f, (1 if i % 2 == 0 else -0.6)) for i, f in enumerate(range(17, 70, 3))] + [(72, 0), (90, 0)]
    sob = K(*sob_keys)

    def pose(r, t):
        u, b, sb = up(t), body(t), sob(t) * body(t)
        r.hips(fwd=-0.03 * b, up=-0.05 * b, pitch=6 * b)
        r.plant()
        r.spine(fwd=16 * b + 2.5 * sb)
        r.look(pitch=22 * b + 3 * sb)
        for s in "lr":
            r.clav(s, up=5 * b + 6 * sb, fwd=6 * b)
        for s in "lr":
            fing = r.vec("Head", ch(-0.25, 0.0, 1.0, s).normalized())
            palm = r.vec("Head", Vector((0, 1, 0)))
            P = r.pt("Head", FACE + Vector((0.0, 0.0, 0.0)) + ch(0.045, 0, 0.0, s))
            H = Rig.wrist_for_palm(P, fing, palm, reach=0.07, thick=0.022)
            r.arm(s, u, H, pole=ch(0.4, 0.1, -1.0, s), fingers=fing, palm=palm)
            r.hand(s, "cup", clamp01(u * 1.2))
    return N, False, pose, (0, 11, 20, 40, 60, 82)


def clip_threaten():
    s = "r"
    N = 60
    act = K((0, 0), (8, 1), (50, 1), (58, 0), (60, 0))
    slash = K((0, 0), (10, 0), (13, -0.08), (25, 1.0), (28, 1.05), (32, 1.0), (60, 1.0))  # thumb left->right across
    point = K((0, 0), (28, 0), (35, 1.08), (38, 1), (60, 1))                              # switch to pointing
    wag = K((0, 0), (37, 0), (40, 1), (43, -1), (46, 1), (49, -1), (52, 0), (60, 0))
    body = K((0, 0), (10, 1), (30, 1), (36, 1.2), (50, 1), (58, 0), (60, 0))

    def pose(r, t):
        a, sl, pt_, wg, b = act(t), slash(t), point(t), wag(t), body(t)
        r.spine(fwd=5 * b + 4 * pt_, twist=4 * pt_)
        r.look(pitch=8 * b - 6 * pt_, roll=5 * b * (1 - pt_), yaw=-6 * (sl - 0.5) * (1 - pt_))
        r.clav(s, up=6 * a, fwd=10 * a)
        # throat slash: fist palm down, knuckles pointing to the left, thumb (pointing back) drawn across the throat
        T = r.pt("neck_01", THROAT)
        tip = T + Vector((0.07 - 0.16 * sl, -0.02, 0.0))
        fing_s = ch(-1.0, 0.15, 0.0, s).normalized()
        palm_s = ch(0.0, 0.0, -1.0, s)
        Hs = tip + ch(0.06, 0.10, 0.02, s)
        S = r.shoulder(s)
        Hp = S + ch(0.05, 0.51, 0.07, s) + ch(0.0, -0.05 * abs(wg), 0.0, s)
        fing_p = (FWD + ch(0.35 * wg, 0, 0.12, s)).normalized()
        palm_p = ch(-0.8, 0.0, -0.6, s)
        H = lerp(Hs, Hp, pt_)
        fing = nlerp(fing_s, fing_p, pt_)
        palm = nlerp(palm_s, palm_p, pt_)
        pole = nlerp(ch(1.0, 0.1, -0.7, s), ch(0.4, 0.0, -1.0, s), pt_)
        r.arm(s, a, H, pole=pole, fingers=fing, palm=palm)
        if pt_ > 0:
            r.hand(s, "point", clamp01(pt_ * 1.2), frm="thumbout")
        else:
            r.hand(s, "thumbout", clamp01(a * 1.3))
    return N, False, pose, (0, 10, 18, 25, 37, 46)


def clip_accuse():
    N = 66
    act = K((0, 0), (5, 1), (56, 1), (64, 0), (66, 0))
    ext = K((0, 0), (5, 0.0), (10, 1.1), (13, 1), (20, 0.35), (24, 1.1), (27, 1), (31, 0.35), (35, 1.12), (38, 1),
            (50, 1), (60, 0), (66, 0))
    lean = K((0, 0), (5, -0.5), (10, 1.1), (13, 1), (20, 0.6), (24, 1.15), (31, 0.6), (35, 1.2), (50, 1), (60, 0),
             (66, 0))
    fist = K((0, 0), (6, 1), (56, 1), (64, 0), (66, 0))

    def pose(r, t):
        a, e, ln, fs = act(t), ext(t), lean(t), fist(t)
        r.spine(fwd=12 * ln, twist=10 * clamp01(e))
        r.look(pitch=-8 * ln, yaw=-4 * clamp01(e))
        s = "r"
        r.clav(s, up=5 * a, fwd=14 * clamp01(e))
        S = r.shoulder(s)
        Hfar = S + ch(0.03, 0.53, 0.06, s)
        Hcock = S + ch(0.10, 0.12, -0.06, s)
        H = lerp(Hcock, Hfar, e)
        pole = nlerp(ch(1.0, -0.3, -0.6, s), ch(0.3, 0.0, -1.0, s), clamp01(e))
        r.arm(s, a, H, pole=pole, fingers=(FWD + ch(0, 0, 0.06, s)).normalized(), palm=ch(-0.7, 0, -0.7, s))
        r.hand(s, "point", clamp01(a * 1.4))
        s = "l"
        r.clav(s, up=6 * fs)
        H = r.pt("spine_01", Vector((0.27, -0.02, 0.90)))
        r.arm(s, fs, H, pole=ch(1.0, -0.5, -0.3, s), fingers=ch(-0.15, 0.2, -1.0, s).normalized(), palm=ch(-1, 0, 0, s))
        r.hand(s, "fist", fs)
    return N, False, pose, (0, 5, 10, 20, 24, 35)


CLIPS = {
    # name: (builder, loop, layer)
    "Wave": (clip_wave, False, "upper"),
    "Point": (clip_point, False, "upper"),
    "Clap": (clip_clap, False, "upper"),
    "Cheer": (clip_cheer, False, "upper"),
    "Shrug": (clip_shrug, False, "upper"),
    "Laugh": (clip_laugh, False, "full"),
    "Facepalm": (clip_facepalm, False, "upper"),
    "Reel": (clip_reel, True, "full"),
    "Sit_Enter": (clip_sit_enter, False, "full"),
    "Sit_Loop": (clip_sit_loop, True, "full"),
    "Bow": (clip_bow, False, "full"),
    "Salute": (clip_salute, False, "upper"),
    "Sus": (None, True, "upper"),
    "Cry": (clip_cry, False, "full"),
    "Threaten": (clip_threaten, False, "upper"),
    "Accuse": (clip_accuse, False, "upper"),
    "Fish_Hold": (clip_fish_hold, True, "upper"),
    "Fish_Cast": (clip_fish_cast, False, "upper"),
    "Fish_Reel": (clip_fish_reel, True, "upper"),
}


# ----------------------------------------------------------------------------------------------------------------
# Actions + export
# ----------------------------------------------------------------------------------------------------------------
def action_fcurves(action):
    out = []
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                cb = strip.channelbag(slot)
                if cb:
                    out.extend(cb.fcurves)
    return out


def write_action(arm, name, keys, loop):
    if name in bpy.data.actions:
        bpy.data.actions.remove(bpy.data.actions[name])
    act = bpy.data.actions.new(name)
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = act
    prev = {}
    for f, snap in keys:
        for pb in arm.pose.bones:
            loc, q, sc = snap[pb.name]
            q = q.copy()
            if pb.name in prev and prev[pb.name].dot(q) < 0:
                q.negate()
            prev[pb.name] = q
            pb.location, pb.rotation_quaternion, pb.scale = loc, q, sc
            for path in ("location", "rotation_quaternion", "scale"):
                pb.keyframe_insert(path, frame=f, group=pb.name)
    for fc in action_fcurves(act):
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
        fc.update()
    if loop:
        # seamless: last frame == first frame (quaternion sign included)
        first, last = keys[0][1], keys[-1][1]
        err = max(max((first[n][0] - last[n][0]).length, min((first[n][1] - last[n][1]).magnitude,
                                                             (first[n][1] + last[n][1]).magnitude))
                  for n in first)
        print(f"KG_EMOTE_LOOPCHECK {name} seam_err={err:.6f}")
    return act


def build_keys(rig, builder):
    n, cyclic, pose, show = builder()
    keys = []
    for f in range(n + 1):
        rig.reset()
        pose(rig, float(f))
        keys.append((f, rig.snap()))
    if cyclic:
        keys[-1] = (n, keys[0][1])
    return n, keys, show


def check_feet(rig, name, keys):
    rig.reset()
    ref = {b: rig.head(b) for b in ("foot_l", "foot_r", "ball_l", "ball_r")}
    worst = 0.0
    for f, snap in keys:
        for pb in rig.pbs:
            pb.location, pb.rotation_quaternion, pb.scale = snap[pb.name]
        rig.upd()
        for b, p in ref.items():
            worst = max(worst, (rig.head(b) - p).length)
    print(f"KG_EMOTE_FEETCHECK {name} max_foot_drift={worst * 100:.2f}cm")
    return worst


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = []
    preview = "--preview" in argv
    if "--only" in argv:
        only = [x for x in argv[argv.index("--only") + 1].split(",") if x]
    pos = [a for i, a in enumerate(argv) if not a.startswith("--") and (i == 0 or argv[i - 1] != "--only")]
    out_dir = os.path.abspath(pos[0]) if pos else DEFAULT_OUT
    os.makedirs(out_dir, exist_ok=True)
    names = [n for n in CLIPS if not only or n in only]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS          # before the glTF imports so their actions land on 30 fps frames
    scene.render.fps_base = 1.0
    objs = KGV["import_gltf"](KGV["UAL1"])
    arm = next(o for o in objs if o.type == "ARMATURE")
    arm.name = "Armature"
    for o in objs:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    ad = arm.animation_data or arm.animation_data_create()
    idle = bpy.data.actions["Idle_Loop"]
    ad.action = idle
    if hasattr(ad, "action_slot") and idle.slots:
        ad.action_slot = idle.slots[0]
    scene.frame_set(int(idle.frame_range[0]))
    bpy.context.view_layer.update()
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    # freeze the idle frame-0 pose, then detach the action
    frozen = {pb.name: (pb.location.copy(), pb.rotation_quaternion.copy(), pb.scale.copy()) for pb in arm.pose.bones}
    ad.action = None
    for pb in arm.pose.bones:
        pb.location, pb.rotation_quaternion, pb.scale = frozen[pb.name]
    rig = Rig(arm)

    ual2 = None
    if "Sus" in names:
        before = set(bpy.data.objects)
        acts_before = set(bpy.data.actions)
        bpy.ops.import_scene.gltf(filepath=UAL2)
        for o in [o for o in bpy.data.objects if o not in before]:
            bpy.data.objects.remove(o, do_unlink=True)
        ual2 = next(a for a in bpy.data.actions if a not in acts_before and a.name.startswith("Idle_FoldArms_Loop"))

    results = []
    for name in names:
        builder, loop, layer = CLIPS[name]
        if name == "Sus":
            n, keys = clip_sus(rig, ual2)
            show = (0, 15, 30, 45, 60, 75)
        else:
            n, keys, show = build_keys(rig, builder)
        act = write_action(arm, f"A_KG_Emote_{name}", keys, loop)
        if layer == "full" and not name.startswith("Sit"):
            check_feet(rig, name, keys)
        ad.action = act
        if hasattr(ad, "action_slot") and act.slots:
            ad.action_slot = act.slots[0]
        scene.frame_start, scene.frame_end = 0, n
        KGV["export_fbx"](os.path.join(out_dir, f"A_KG_Emote_{name}.fbx"), [arm], anim=True)
        results.append((name, n, loop, layer, show))
        print(f"KG_EMOTE {name} frames={n + 1} length={n / FPS:.3f}s loop={loop} layer={layer}")
        rig.reset()

    if preview:
        render_previews(arm, results)
    print("KG_EMOTES_DONE " + " ".join(f"{n}:{f / FPS:.2f}s{'(loop)' if lp else ''}" for n, f, lp, _, _ in results))


# ----------------------------------------------------------------------------------------------------------------
# Preview contact sheets
# ----------------------------------------------------------------------------------------------------------------
def render_previews(arm, results, tile=480, tmp_dir=None):
    import numpy as np
    scene = bpy.context.scene
    os.makedirs(CONCEPT, exist_ok=True)
    tmp_dir = tmp_dir or os.path.join(tempfile.gettempdir(), "kg_emote_preview")
    os.makedirs(tmp_dir, exist_ok=True)
    varm, _mesh = KGV["compose"]("SK_KG_Villager_M", KGV["VILLAGERS"]["SK_KG_Villager_M"])
    arm.name = "Armature"
    # UE plays the clip's local transforms on the villager skeleton, so the villager bones take the UAL bone
    # matrices verbatim (component space) - reproduce that with world-space copy-transforms.
    for pb in varm.pose.bones:
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target = arm
        c.subtarget = pb.name
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    try:
        scene.eevee.taa_render_samples = 8
    except AttributeError:
        pass
    scene.render.resolution_x = scene.render.resolution_y = tile
    scene.render.image_settings.file_format = "PNG"
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs["Color"].default_value = (0.42, 0.55, 0.72, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    bpy.ops.mesh.primitive_plane_add(size=4, location=(0, 0, 0))
    floor = bpy.context.active_object
    mat = bpy.data.materials.new("Floor")
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (0.25, 0.3, 0.22, 1)
    floor.data.materials.append(mat)
    cams = []
    for label, ang in (("front", 0.0), ("q34", 40.0)):
        cam = bpy.data.objects.new(f"Cam_{label}", bpy.data.cameras.new(label))
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = 2.35
        a = math.radians(ang)
        cam.location = (-6 * math.sin(a), -6 * math.cos(a), 2.2)
        cam.rotation_euler = (Vector((0, 0, 0.98)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(cam)
        cams.append(cam)
    ad = arm.animation_data
    for name, n, loop, layer, show in results:
        act = bpy.data.actions[f"A_KG_Emote_{name}"]
        ad.action = act
        if hasattr(ad, "action_slot") and act.slots:
            ad.action_slot = act.slots[0]
        rows = []
        for cam in cams:
            scene.camera = cam
            row = []
            for f in show:
                scene.frame_set(f)
                p = os.path.join(tmp_dir, f"{name}_{cam.name}_{f:03d}.png")
                scene.render.filepath = p
                bpy.ops.render.render(write_still=True)
                img = bpy.data.images.load(p)
                px = np.empty(tile * tile * 4, np.float32)
                img.pixels.foreach_get(px)
                row.append(px.reshape(tile, tile, 4))
                bpy.data.images.remove(img)
            rows.append(np.concatenate(row, axis=1))
        sheet = np.concatenate(rows[::-1], axis=0)  # Blender images are bottom-up: first row on top
        h, w = sheet.shape[:2]
        # thin separators between tiles
        for i in range(1, len(show)):
            sheet[:, i * tile - 1:i * tile + 1, :3] = 0.05
        sheet[h // 2 - 1:h // 2 + 1, :, :3] = 0.05
        out = bpy.data.images.new(f"{name}_sheet", w, h, alpha=False)
        out.pixels.foreach_set(sheet.ravel())
        out.filepath_raw = os.path.join(CONCEPT, f"{name}_sheet.png")
        out.file_format = "PNG"
        out.save()
        bpy.data.images.remove(out)
        print(f"KG_EMOTE_SHEET {name} frames={list(show)} -> {os.path.join(CONCEPT, name + '_sheet.png')}")


if __name__ == "__main__":
    main()
