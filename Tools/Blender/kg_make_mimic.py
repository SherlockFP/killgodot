"""SPRINT-041 Trapper pack (KG_Mimic): the mimic overlay for ANY container (teeth ring, tongue, drool, teeth marks) and
the Trapper's ground traps (bear-trap snare open / shut, tripwire). Headless Blender 5.2, vertex colours, no textures.

  blender --background --factory-startup --python Tools/Blender/kg_make_mimic.py -- [out.glb] [preview_dir] [--no-preview]
  then: blender --background --factory-startup --python Tools/Blender/kg_sanitize_glb.py -- \
      Art/Packed/KG_Mimic.glb Art/Packed/KG_Mimic_Clean.glb
  and the UE import (editor closed, headless): UnrealEditor-Cmd.exe KillGodot.uproject -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_import_dress_pack.py KG_Mimic_Clean"
  -> /Game/KillGodot/Env/Dress/KG_Mimic_Clean/StaticMeshes/SM_KG_<Name> (read by Source/KillGodot/Traps/KGMimicTrap.cpp,
     KGFieldTraps.cpp; engine shapes stand in until the import).

Blender axes, metres. The overlay pieces are UNIT pieces the game fits to the host container's mesh bounds at runtime
(AKGMimicTrap::FitOverlays), so one set works on every chest, crate and barrel:
  MimicTeeth : a 1.0 x 1.0 m ring around the lid seam (pivot = seam centre, z = 0): a dark mouth gap band, upper teeth
               pointing down, lower teeth pointing up (+-0.10 m). The game scales z to show tips only (at rest) or bared.
  MimicMarks : the same ring footprint: dark gouges and splinters left after a bite (the "teeth marks" evidence).
  MimicTongue: from the pivot along +X, 0.6 m, drooping tip (the game pitches / stretches it).
  MimicDrool : a drip hanging from the pivot (0 .. -0.09 m).
  SnareOpen / SnareShut : bear trap, pivot bottom centre, ~0.5 m across.
  Tripwire   : two pegs 3 m apart along Y (+-1.5 m, matching AKGTripwireTrap's zone) and the wire at 0.18 m.
"""
import math
import os
import random
import sys

_SRC = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "kg_make_dress_terrace.py"), encoding="utf-8").read()
_SRC = _SRC[:_SRC.rindex("\nmain()")]
exec(compile(_SRC, "kg_make_dress_terrace", "exec"), globals())

OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_Mimic.glb"                       # noqa: F821
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"                                # noqa: F821
MANIFEST = _opt.get("--manifest", f"{ROOT}/Art/Packed/KG_Mimic_Clean.json")                      # noqa: F821
PREFIX = _opt.get("--prefix", "Mimic_preview")                                                   # noqa: F821

TOOTH, TOOTH_DK = C("F4EBD2"), C("CDBF98")                                                       # noqa: F821
GUM, MOUTH = C("8E2233"), C("240A10")                                                            # noqa: F821
TONGUE, TONGUE_DK = C("D9546A"), C("A8364C")                                                     # noqa: F821
DROOL = C("CFE8EE")                                                                              # noqa: F821
GOUGE, SPLINTER = C("3A2414"), C("C79B62")                                                       # noqa: F821
IRON, IRON_DK, RUST_ = C("5B5E63"), C("383A3E"), C("8C4A26")                                     # noqa: F821
PEG, WIRE, BRASS_ = C("8A5E36"), C("B8BEC4"), C("D8AE4A")                                        # noqa: F821


def _ring_points(step):
    """Points along the unit square perimeter (+-0.5), with the outward normal."""
    pts = []
    n = max(2, int(round(1.0 / step)))
    for side in range(4):
        for k in range(n):
            t = -0.5 + (k + 0.5) / n
            if side == 0:
                pts.append(((t, -0.5), (0, -1)))
            elif side == 1:
                pts.append(((0.5, t), (1, 0)))
            elif side == 2:
                pts.append(((-t, 0.5), (0, 1)))
            else:
                pts.append(((-0.5, -t), (-1, 0)))
    return pts


def make_teeth():
    mb = MB("SM_KG_MimicTeeth", angle=30)                                                        # noqa: F821
    rng = random.Random(41)
    # The dark mouth gap: a thin frame just outside the seam, with gum lips above and below.
    for (cx, cy, sx, sy) in ((0, -0.505, 1.03, 0.02), (0, 0.505, 1.03, 0.02), (-0.505, 0, 0.02, 1.03), (0.505, 0, 0.02, 1.03)):
        mb.box((cx, cy, 0.0), (sx, sy, 0.05), MOUTH)
        mb.box((cx * 1.004, cy * 1.004, 0.032), (sx if sx > 0.5 else 0.024, sy if sy > 0.5 else 0.024, 0.016), GUM)
        mb.box((cx * 1.004, cy * 1.004, -0.032), (sx if sx > 0.5 else 0.024, sy if sy > 0.5 else 0.024, 0.016), GUM)
    # Teeth: upper row points down, lower row points up, staggered; a few big fangs.
    for i, ((x, y), (nx, ny)) in enumerate(_ring_points(0.085)):
        out = 0.515
        px, py = x + nx * (out - 0.5), y + ny * (out - 0.5)
        big = i % 7 == 3
        h = (0.15 if big else 0.10) * (0.85 + 0.3 * rng.random())
        r = 0.028 if big else 0.02
        col = mix(TOOTH, TOOTH_DK, 0.25 * rng.random())                                           # noqa: F821
        mb.cyl((px, py, 0.03), (px, py, 0.03 - h), r, 0.002, seg=6, col=col, sh=FLAT)             # noqa: F821
        sh = 0.5 * 0.085
        qx, qy = px + (-ny) * sh * 0.5, py + nx * sh * 0.5
        mb.cyl((qx, qy, -0.03), (qx, qy, -0.03 + h * 0.8), r * 0.9, 0.002, seg=6, col=col, sh=FLAT)  # noqa: F821
    return mb


def make_marks():
    mb = MB("SM_KG_MimicMarks", angle=30)                                                        # noqa: F821
    rng = random.Random(7)
    for (x, y), (nx, ny) in _ring_points(0.07):
        if rng.random() < 0.25:
            continue
        for row, z0 in ((0, 0.05), (1, -0.05)):
            px, py = x + nx * 0.004, y + ny * 0.004
            ang = rng.uniform(-25, 25)
            L = rng.uniform(0.03, 0.06)
            with mb.xf(T(px, py, z0 + rng.uniform(-0.015, 0.015)) @ R(math.degrees(math.atan2(ny, nx)), 'Z') @ R(ang, 'X')):  # noqa: F821
                mb.box((0.0, 0.0, 0.0), (0.008, 0.012, L), GOUGE)
            if rng.random() < 0.3:
                with mb.xf(T(px + nx * 0.01, py + ny * 0.01, z0) @ R(math.degrees(math.atan2(ny, nx)), 'Z') @ R(rng.uniform(40, 70), 'Y')):  # noqa: F821
                    mb.box((0.0, 0.0, 0.0), (0.004, 0.006, 0.03), SPLINTER)
    return mb


def make_tongue():
    mb = MB("SM_KG_MimicTongue", angle=40)                                                       # noqa: F821
    s0 = len(mb.V)
    mb.blob((0.30, 0.0, 0.0), (0.30, 0.075, 0.026), sub=2, amp=0.05, seed=3.0, col=TONGUE)
    # Droop the tip and cut a darker middle groove.
    mb.deform(s0, lambda p: (p.x, p.y, p.z - 0.18 * (max(0.0, p.x - 0.2) / 0.4) ** 2))
    mb.recolor(s0, lambda p, c: mix(TONGUE_DK, c, min(1.0, abs(p.y) / 0.02)) if p.z > -0.001 else c)  # noqa: F821
    return mb


def make_drool():
    mb = MB("SM_KG_MimicDrool", angle=40)                                                        # noqa: F821
    mb.cyl((0, 0, 0.0), (0, 0, -0.07), 0.006, 0.003, seg=6, col=DROOL, sh=SMOOTH)                # noqa: F821
    mb.blob((0, 0, -0.078), (0.012, 0.012, 0.016), sub=1, amp=0.0, col=DROOL)
    return mb


def _jaw(mb, rng, closed):
    """Two semicircular jaws hinged on X at the plate; open = flat, shut = upright and meeting."""
    R_ = 0.22
    for side in (-1, 1):
        rot = 0.0 if not closed else 88.0
        with mb.xf(R(side * rot, 'X')):                                                          # noqa: F821
            seg = 14
            for k in range(seg):
                a0 = math.pi * k / seg
                a1 = math.pi * (k + 1) / seg
                p0 = (R_ * math.cos(a0), side * R_ * math.sin(a0), 0.02)
                p1 = (R_ * math.cos(a1), side * R_ * math.sin(a1), 0.02)
                mb.beam(p0, p1, 0.018, 0.02, mix(IRON, RUST_, 0.25 * rng.random()))                # noqa: F821
                mx_, my_ = (p0[0] + p1[0]) * 0.5, (p0[1] + p1[1]) * 0.5
                if 0 < k < seg - 1:
                    mb.cyl((mx_, my_, 0.03), (mx_ * 0.93, my_ * 0.93, 0.07), 0.012, 0.001, seg=4, col=IRON_DK, sh=FLAT)  # noqa: F821


def make_snare(closed):
    mb = MB("SM_KG_SnareShut" if closed else "SM_KG_SnareOpen", angle=35)                        # noqa: F821
    rng = random.Random(5 if closed else 4)
    mb.lathe([(0.0, 0.0), (0.09, 0.0), (0.09, 0.02), (0.0, 0.02)], 16, col=IRON_DK, sh=FLAT, closed=False)  # noqa: F821
    mb.cyl((-0.02, 0, 0.02), (0.02, 0, 0.02), 0.035, seg=10, col=IRON)                           # noqa: F821  pan
    for sx in (-1, 1):                                                                             # the two springs
        mb.beam((sx * 0.09, -0.03, 0.012), (sx * 0.32, -0.03, 0.012), 0.03, 0.018, IRON_DK)
        mb.beam((sx * 0.09, 0.03, 0.012), (sx * 0.32, 0.03, 0.012), 0.03, 0.018, IRON_DK)
    _jaw(mb, rng, closed)
    for k in range(5):                                                                             # chain + stake
        mb.cbox((0.34 + k * 0.035, 0.0, 0.01), (0.03, 0.012, 0.012), 0.003, IRON_DK)
    mb.cyl((0.53, 0.0, -0.05), (0.53, 0.0, 0.06), 0.012, 0.016, seg=6, col=PEG)                  # noqa: F821
    return mb


def make_tripwire():
    mb = MB("SM_KG_Tripwire", angle=35)                                                          # noqa: F821
    for sy in (-1, 1):
        mb.cyl((0, sy * 1.5, -0.05), (0, sy * 1.5, 0.28), 0.022, 0.016, seg=6, col=PEG)           # noqa: F821
        mb.cyl((0, sy * 1.5, 0.28), (0, sy * 1.5, 0.31), 0.016, 0.004, seg=6, col=PEG)            # noqa: F821
    mb.cyl((0, -1.5, 0.18), (0, 1.5, 0.18), 0.004, seg=5, col=WIRE, caps=False)                   # noqa: F821
    mb.blob((0.0, 1.45, 0.13), (0.022, 0.022, 0.028), sub=1, amp=0.0, col=BRASS_)                 # a little bell
    return mb


PROPS = [
    ("MimicTeeth", make_teeth, "none"),
    ("MimicMarks", make_marks, "none"),
    ("MimicTongue", make_tongue, "none"),
    ("MimicDrool", make_drool, "none"),
    ("SnareOpen", lambda: make_snare(False), "none"),
    ("SnareShut", lambda: make_snare(True), "none"),
    ("Tripwire", make_tripwire, "none"),
]
EXPECT = {}


def preview_tiles(names):
    H = "HUMAN"
    t1 = [dict(label=n, items=[(n, (0, 0, 0), 0), (H, (1.4, -0.5, 0), 0)]) for n, _, _ in PROPS if n in names]
    t1.append(dict(label="Lineup", items="LINEUP", az=-12, el=12))
    t2 = []
    keep = set(names)

    def ok(t):
        return t["items"] == "LINEUP" or all(k in keep or k in ("REF", "HUMAN", "WATER") for k, _, _ in t["items"])
    return [t for t in t1 if ok(t)], [t for t in t2 if ok(t)]


main()                                                                                            # noqa: F821
