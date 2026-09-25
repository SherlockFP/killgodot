"""SPRINT-022 landmark / tower / cliff props (pack KG_DressLandmarks). Headless Blender 5.2, vertex colours, no textures.

  blender --background --factory-startup --python Tools/Blender/kg_make_dress_landmarks.py -- \
      [out.glb] [preview_dir] [--only A,B] [--no-preview] [--no-verify] [--no-export]
  then: blender --background --factory-startup --python Tools/Blender/kg_sanitize_glb.py -- \
      Art/Packed/KG_DressLandmarks.glb Art/Packed/KG_DressLandmarks_Clean.glb
  and the UE import: kg_import_dress_pack.py KG_DressLandmarks_Clean  (build step 2)

The mesh builder, palette, finalize/export/manifest/preview/verify machinery is the KG_DressTerrace pack's
(Tools/Blender/kg_make_dress_terrace.py, executed up to its main()); this file only adds the props.
Blender axes, metres. "Front" = -Y in Blender = +Y in UE (the builder's +Y-front prop convention, like ClockFace).
Pivots:
  ClockStage, Belfry, Spire, LighthouseGallery, LampRoom, Telescope, LanternPlinth, Buttress, WallFountain,
  ArchPortal : bottom centre (Buttress / WallFountain / ArchPortal: back face on Y = 0, body towards -Y)
  ClockDial  : dial centre, flat back at Y = 0, face towards -Y
  ClockHand_Hour / ClockHand_Minute : the arbor (rotation centre); hand points +Z (12 o'clock), proud towards -Y
  Bell       : the headstock axle centre (the bell hangs below it, the yoke runs along X)
  BellBeam   : centre of the beam (runs along X)
  Waterwheel : axle centre; wheel plane XZ, axle along Y (spins about Y)
  Cliff_A/B/C, CliffTalus : bottom of the back face, centre (back on Y = 0 against the cliff, rock towards -Y)
"""
import math
import os
import random
import sys

_SRC = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "kg_make_dress_terrace.py"), encoding="utf-8").read()
_SRC = _SRC[:_SRC.rindex("\nmain()")]
exec(compile(_SRC, "kg_make_dress_terrace", "exec"), globals())

OUT_GLB = _pos[0] if len(_pos) > 0 else f"{ROOT}/Art/Packed/KG_DressLandmarks.glb"            # noqa: F821
PREVIEW_DIR = _pos[1] if len(_pos) > 1 else f"{ROOT}/Art/Concept"                                # noqa: F821
MANIFEST = _opt.get("--manifest", f"{ROOT}/Art/Packed/KG_DressLandmarks_Clean.json")             # noqa: F821
PREFIX = _opt.get("--prefix", "DressLandmarks_preview")                                          # noqa: F821

WOOD, WOOD_DK, WOOD_LT = C("8A5E36"), C("5E3E22"), C("A87B4E")                                  # noqa: F821
BRONZE, BRONZE_DK = C("B98A3E"), C("7E5A28")                                                      # noqa: F821
SLATE, SLATE_DK = C("5C6878"), C("46505E")                                                        # noqa: F821
BRASS = C("D8AE4A")                                                                               # noqa: F821
ROCKS = [C("9A8F80"), C("A69A88"), C("8E8375"), C("AFA391"), C("978A78"), C("B4A994")]            # noqa: F821
ROCK_DK = C("5E5850")                                                                             # noqa: F821
GRASS_T, GRASS_T2 = C("7FA24A"), C("6B8E3E")                                                      # noqa: F821
WATERC = C("3F8FA0")                                                                              # noqa: F821


def octo(r, n=8, phase=math.pi / 8):
    return circle(n, r, phase)                                                                    # noqa: F821


# ================================================================================================ clock tower
def make_clock_stage():
    """Stone clock stage on top of the 4 x 4 m kit shaft (walls reach +-2.31 m): corbel table, pilasters, four round
    dial niches, a moulded cornice. 5.0 x 5.0 x 3.4 m."""
    mb = MB("SM_KG_ClockStage", angle=30)                                                         # noqa: F821
    rng = random.Random(11)
    s0 = mb.cbox((0, 0, 0.18), (4.9, 4.9, 0.36), 0.05, pick(rng, TERRACE))                        # noqa: F821
    mb.recolor(s0, stone_noise(1.0))                                                              # noqa: F821
    for k in range(10):                                                                           # corbels
        for sgn, ax in ((1, 0), (-1, 0), (1, 1), (-1, 1)):
            t = -2.1 + k * 0.466
            c = (t, sgn * 2.47, 0.44) if ax == 0 else (sgn * 2.47, t, 0.44)
            mb.cbox(c, (0.22, 0.22, 0.2) if ax == 0 else (0.22, 0.22, 0.2), 0.03, mul(STONE, 0.95))  # noqa: F821
    s0 = mb.cbox((0, 0, 0.62), (5.1, 5.1, 0.16), 0.03, pick(rng, VOUSS))                           # noqa: F821
    s0 = mb.cbox((0, 0, 1.98), (4.7, 4.7, 2.6), 0.04, pick(rng, TERRACE))                          # noqa: F821
    mb.recolor(s0, stone_noise(3.0, 0.05))                                                        # noqa: F821
    for sx in (-1, 1):                                                                            # corner pilasters
        for sy in (-1, 1):
            mb.cbox((sx * 2.3, sy * 2.3, 1.98), (0.36, 0.36, 2.64), 0.04, pick(rng, VOUSS))       # noqa: F821
    for rot in (0, 90, 180, 270):                                                                 # dial niches + rings
        with mb.xf(R(rot, 'Z') @ T(0, -2.36, 1.98) @ XZ_FRONT_NEG_Y):                             # noqa: F821
            mb.lathe([(0.98, 0.0), (1.08, 0.0), (1.08, 0.07), (0.98, 0.07)], 28, col=mix(STONE_LT, WHITE, 0.3), sh=FLAT,  # noqa: F821
                     closed=True)
    s0 = mb.cbox((0, 0, 3.32), (5.2, 5.2, 0.18), 0.04, pick(rng, VOUSS))                           # noqa: F821
    mb.cbox((0, 0, 3.44), (4.9, 4.9, 0.08), 0.02, mul(STONE, 0.9))                                # noqa: F821
    return mb


def make_belfry():
    """Open belfry over the clock stage: four piers, a round arch on every side, balustrade, entablature, a small
    bell inside. 4.6 x 4.6 x 3.8 m."""
    mb = MB("SM_KG_Belfry", angle=30)                                                             # noqa: F821
    rng = random.Random(12)
    mb.cbox((0, 0, 0.08), (4.6, 4.6, 0.16), 0.03, pick(rng, VOUSS))                                # noqa: F821
    for sx in (-1, 1):
        for sy in (-1, 1):
            s0 = mb.cbox((sx * 1.95, sy * 1.95, 1.55), (0.7, 0.7, 2.8), 0.05, pick(rng, TERRACE))  # noqa: F821
            mb.recolor(s0, stone_noise(sx + 2 * sy))                                              # noqa: F821
    for rot in (0, 90, 180, 270):
        with mb.xf(R(rot, 'Z')):                                                                  # noqa: F821
            # arch: voussoirs on a semicircle spanning the 3.2 m opening between the piers, springing at z 2.2
            n = 11
            for k in range(n):
                a0, a1 = math.pi * k / n, math.pi * (k + 1) / n
                am = (a0 + a1) / 2
                r = 1.3
                c = (math.cos(am) * (r + 0.14), -1.95, 2.2 + math.sin(am) * (r + 0.14))
                with mb.xf(T(*c) @ R(-math.degrees(am) + 90, 'Y')):                                # noqa: F821
                    mb.cbox((0, 0, 0), (0.28, 0.5, 0.36), 0.03, pick(rng, VOUSS) if k != n // 2 else mix(WHITE, GOLD, 0.2))  # noqa: F821
            s0 = mb.cbox((0, -1.95, 3.35), (4.6, 0.62, 0.5), 0.04, pick(rng, TERRACE))            # noqa: F821
            for k in range(7):                                                                    # balustrade
                x = -1.2 + k * 0.4
                mb.cyl((x, -1.95, 0.16), (x, -1.95, 0.8), 0.07, 0.05, 8, col=STONE_LT)             # noqa: F821
            mb.cbox((0, -1.95, 0.86), (3.3, 0.3, 0.12), 0.03, pick(rng, VOUSS))                    # noqa: F821
    mb.cbox((0, 0, 3.68), (4.9, 4.9, 0.18), 0.04, pick(rng, VOUSS))                                # noqa: F821
    # small bell under a beam
    mb.beam((-2.0, 0, 3.0), (2.0, 0, 3.0), 0.18, 0.22, WOOD_DK)
    mb.lathe([(0, 2.97), (0.16, 2.95), (0.3, 2.82), (0.33, 2.5), (0.42, 2.2), (0.46, 2.12), (0, 2.12)], 16,
             col=BRONZE, sh=AUTO)
    return mb


def make_spire():
    """Octagonal drum with lucarnes and a tall verdigris spire, gilded ball and a weathervane. 3.8 m wide, 11.5 m."""
    mb = MB("SM_KG_Spire", angle=28)                                                              # noqa: F821
    rng = random.Random(13)
    drum = octo(1.9)
    s0 = mb.prism(drum, 0.0, 1.1, pick(rng, TERRACE), b1=0.04)                                    # noqa: F821
    mb.recolor(s0, stone_noise(4.0))                                                              # noqa: F821
    mb.prism(octo(2.0), 1.1, 1.3, pick(rng, VOUSS), b0=0.03, b1=0.03)                              # noqa: F821
    # spire: 8-sided cone with slight concave profile, verdigris with darker bands
    prof = [(0, 1.3), (1.85, 1.3), (1.45, 2.6), (1.0, 4.4), (0.6, 6.6), (0.28, 8.6), (0.08, 9.9), (0, 10.0)]
    band = [None, rgb3(VERD), rgb3(VERD_DK), rgb3(VERD), rgb3(VERD_LT), rgb3(VERD), rgb3(VERD_DK)]  # noqa: F821
    mb.lathe(prof, 8, col=VERD, segcol=band, sh=FLAT, phase=math.pi / 8)                          # noqa: F821
    for k in range(4):                                                                            # lucarnes
        a = math.pi / 2 * k
        with mb.xf(R(math.degrees(a), 'Z') @ T(0, -1.55, 2.3)):                                   # noqa: F821
            mb.cbox((0, 0, 0), (0.7, 0.5, 0.9), 0.03, VERD_LT)                                    # noqa: F821
            mb.cbox((0, -0.26, -0.05), (0.36, 0.04, 0.5), 0.01, C("2A3440", a=0.0))               # noqa: F821
    mb.cyl((0, 0, 9.9), (0, 0, 11.6), 0.035, 0.025, 6, col=IRON)                                  # noqa: F821
    mb.blob((0, 0, 10.15), 0.2, sub=2, amp=0.0, col=GOLD)                                         # noqa: F821
    # weathervane: arrow + a cockerel plate at 11.1 m
    mb.beam((-0.75, 0, 11.1), (0.75, 0, 11.1), 0.04, 0.04, IRON)                                  # noqa: F821
    with mb.xf(T(0.62, 0, 11.1)):                                                                 # noqa: F821
        mb.ext2d([(0, -0.14), (0.3, 0), (0, 0.14)], -0.015, 0.015, GOLD)                          # noqa: F821
    with mb.xf(T(-0.25, 0, 11.1) @ R(90, 'X')):                                                  # noqa: F821
        mb.ext2d([(-0.35, 0.0), (-0.1, 0.05), (0.05, 0.32), (0.18, 0.28), (0.12, 0.12), (0.3, 0.05), (0.2, -0.1),
                  (-0.2, -0.12)], -0.015, 0.015, GOLD)                                           # noqa: F821
    for k, (dx, dy) in enumerate(((0.4, 0), (-0.4, 0), (0, 0.4), (0, -0.4))):                     # N E S W
        mb.beam((0, 0, 10.75), (dx, dy, 10.75), 0.025, 0.025, IRON)                               # noqa: F821
    return mb


def make_clock_dial():
    """1.9 m dial without hands (the hands are separate, spinning): stone ring, cream face, gilded marks."""
    mb = MB("SM_KG_ClockDial", angle=30)                                                          # noqa: F821
    with mb.xf(DIAL):                                                                             # noqa: F821
        mb.lathe([(0, 0.0), (0.84, 0.0), (0.84, 0.1), (0, 0.1)], 32, col=CREAM, sh=FLAT,          # noqa: F821
                 segcol=[None, rgb3(CREAM), rgb3(C("F6EDD5"))])                                   # noqa: F821
        prof = [(0.82, 0.0), (0.95, 0.0), (0.95, 0.11), (0.93, 0.15), (0.88, 0.17), (0.84, 0.14)]
        mb.lathe(prof, 32, col=GOLD, sh=AUTO, closed=True)                                        # noqa: F821
        mb.lathe([(0.7, 0.101), (0.72, 0.101), (0.72, 0.104), (0.7, 0.104)], 32, col=C("3A3530"), sh=FLAT, closed=True)  # noqa: F821
        for k in range(12):
            th = math.radians(30 * k)
            d = Vector((math.sin(th), math.cos(th), 0))                                           # noqa: F821
            lng = k % 3 == 0
            r0, r1, w = (0.52, 0.78, 0.07) if lng else (0.62, 0.78, 0.04)
            mb.beam(d * r0 + Vector((0, 0, 0.108)), d * r1 + Vector((0, 0, 0.108)), w, 0.016, C("2A2830"), up=(0, 0, 1))  # noqa: F821
        mb.lathe([(0, 0.1), (0.07, 0.1), (0.07, 0.14), (0, 0.15)], 12, col=C("1E1D24"), sh=AUTO)  # noqa: F821
    return mb


def _hand(name, pts, zoff):
    mb = MB(name, angle=30)                                                                       # noqa: F821
    # drawn in the dial frame (local x = X, local y = Z, proud towards -Y), arbor at the origin
    with mb.xf(DIAL):                                                                             # noqa: F821
        mb.ext2d(pts, zoff, zoff + 0.022, C("1E1D24"))                                            # noqa: F821
        mb.lathe([(0, zoff - 0.01), (0.05, zoff - 0.01), (0.05, zoff + 0.03), (0, zoff + 0.035)], 10, col=GOLD, sh=AUTO)  # noqa: F821
    return mb


def make_hand_hour():
    return _hand("SM_KG_ClockHand_Hour", [(-0.04, -0.12), (0.04, -0.12), (0.03, 0.26), (0.09, 0.34), (0.0, 0.48),
                                          (-0.09, 0.34), (-0.03, 0.26)], 0.02)


def make_hand_minute():
    return _hand("SM_KG_ClockHand_Minute", [(-0.028, -0.16), (0.028, -0.16), (0.02, 0.62), (0.045, 0.66), (0.0, 0.76),
                                            (-0.045, 0.66), (-0.02, 0.62)], 0.05)


# ================================================================================================ bell tower
def make_bell():
    """Bronze bell (0.85 m) hanging from a timber headstock; pivot = the axle centre (bell below, yoke along X)."""
    mb = MB("SM_KG_Bell", angle=30)                                                               # noqa: F821
    mb.cbox((0, 0, 0.0), (1.3, 0.26, 0.26), 0.03, WOOD_DK)                                        # noqa: F821
    for sx in (-1, 1):
        mb.cyl((sx * 0.65, 0, 0), (sx * 0.85, 0, 0), 0.06, 0.06, 10, col=IRON)                    # noqa: F821
        mb.beam((sx * 0.25, 0, -0.12), (sx * 0.2, 0, -0.32), 0.05, 0.08, IRON)                    # noqa: F821
    prof = [(0, -0.12), (0.2, -0.13), (0.26, -0.2), (0.28, -0.42), (0.33, -0.66), (0.43, -0.84), (0.46, -0.9),
            (0.44, -0.93), (0.38, -0.9), (0.3, -0.85)]
    mb.lathe(prof, 20, col=BRONZE, sh=SMOOTH, segcol=[None, None, None, None, rgb3(BRONZE_DK), None, rgb3(GOLD), None, None])  # noqa: F821
    mb.lathe([(0.29, -0.84), (0.2, -0.3), (0.0, -0.2)], 16, col=BRONZE_DK, sh=SMOOTH)            # noqa: F821
    mb.cyl((0, 0, -0.2), (0, 0, -0.8), 0.018, 0.018, 6, col=IRON)                                 # noqa: F821
    mb.blob((0, 0, -0.85), 0.07, sub=1, amp=0.0, col=IRON)                                        # noqa: F821
    return mb


def make_bell_beam():
    """Oak beam spanning the 4 m tower top (4.7 m long) with iron straps, the bell's headstock rests on it."""
    mb = MB("SM_KG_BellBeam", angle=30)                                                           # noqa: F821
    mb.cbox((0, 0, 0), (4.7, 0.3, 0.32), 0.04, WOOD)                                              # noqa: F821
    for x in (-1.9, -0.7, 0.7, 1.9):
        mb.cbox((x, 0, 0), (0.08, 0.34, 0.36), 0.01, IRON)                                        # noqa: F821
    return mb


# ================================================================================================ lighthouse
def make_gallery():
    """Corbelled gallery on top of the 4 x 4 m shaft: 5.8 m octagonal platform with an iron railing."""
    mb = MB("SM_KG_LighthouseGallery", angle=30)                                                  # noqa: F821
    rng = random.Random(21)
    s0 = mb.prism(octo(2.55), 0.0, 0.3, pick(rng, GRANITE), b1=0.03)                               # noqa: F821
    mb.recolor(s0, stone_noise(2.0))                                                              # noqa: F821
    for k in range(16):                                                                           # corbels
        a = 2 * math.pi * k / 16
        with mb.xf(T(math.cos(a) * 2.45, math.sin(a) * 2.45, -0.2) @ R(math.degrees(a), 'Z')):    # noqa: F821
            mb.cbox((0, 0, 0), (0.45, 0.22, 0.4), 0.04, mul(STONE, 0.92))                         # noqa: F821
    s0 = mb.prism(octo(2.95), 0.3, 0.5, pick(rng, GRANITE), b0=0.03, b1=0.03)                      # noqa: F821
    pts = octo(2.85)
    for k in range(8):                                                                            # railing
        a, b = Vector((*pts[k], 0.5)), Vector((*pts[(k + 1) % 8], 0.5))                            # noqa: F821
        mb.cyl(a, a + Vector((0, 0, 1.05)), 0.04, 0.04, 6, col=IRON)                              # noqa: F821
        for h in (0.55, 1.05):
            mb.beam(a + Vector((0, 0, h)), b + Vector((0, 0, h)), 0.045, 0.045, IRON)             # noqa: F821
        for t in (0.25, 0.5, 0.75):
            p = a.lerp(b, t)
            mb.cyl(p, p + Vector((0, 0, 1.05)), 0.018, 0.018, 5, col=IRON_LT)                     # noqa: F821
    return mb


def make_lamp_room():
    """Octagonal lantern: stone sill, glazing (glowing glass), iron mullions, copper dome, ventilator ball."""
    mb = MB("SM_KG_LampRoom", angle=30, material="Glow")                                          # noqa: F821
    rng = random.Random(22)
    mb.prism(octo(1.45), 0.0, 0.75, with_a(mul(WHITE, 0.95), 0.0), b1=0.03)                        # noqa: F821
    mb.prism(octo(1.52), 0.75, 0.85, with_a(C("C43A2E"), 0.0), b0=0.02, b1=0.02)                   # noqa: F821
    mb.prism(octo(1.3), 0.85, 2.55, GLASS)                                                        # noqa: F821
    pts = octo(1.34)
    for k in range(8):
        p = Vector((*pts[k], 0.85))                                                               # noqa: F821
        mb.cyl(p, p + Vector((0, 0, 1.7)), 0.05, 0.05, 6, col=with_a(IRON, 0.0))                  # noqa: F821
    mb.prism(octo(1.5), 2.55, 2.7, with_a(C("C43A2E"), 0.0), b0=0.02, b1=0.02)                     # noqa: F821
    prof = [(0, 2.7), (1.55, 2.7), (1.45, 2.95), (1.2, 3.35), (0.8, 3.7), (0.3, 3.9), (0, 3.93)]
    mb.lathe(prof, 8, col=with_a(VERD, 0.0), sh=FLAT, phase=math.pi / 8)                          # noqa: F821
    mb.cyl((0, 0, 3.9), (0, 0, 4.15), 0.12, 0.08, 8, col=with_a(VERD_DK, 0.0))                    # noqa: F821
    mb.blob((0, 0, 4.3), 0.2, sub=2, amp=0.0, col=with_a(VERD_LT, 0.0))                           # noqa: F821
    mb.cyl((0, 0, 4.45), (0, 0, 5.0), 0.03, 0.015, 6, col=with_a(IRON, 0.0))                      # noqa: F821
    # the lamp inside (glows through the glass)
    mb.lathe([(0, 1.1), (0.35, 1.1), (0.42, 1.5), (0.3, 1.95), (0, 2.05)], 12, col=C("FFE7A8", a=1.0), sh=SMOOTH)  # noqa: F821
    return mb


# ================================================================================================ small landmarks
def make_waterwheel():
    """Undershot waterwheel, 4.3 m across, 1.0 m wide: two rims, 8 spokes a side, 16 paddles, hub and axle."""
    mb = MB("SM_KG_Waterwheel", angle=30)                                                         # noqa: F821
    R_ = 2.15
    for y in (-0.48, 0.48):
        with mb.xf(T(0, y, 0) @ R(90, 'X')):                                                      # noqa: F821
            mb.torus(R_ - 0.1, 0.09, 24, 6, col=WOOD_DK, sh=AUTO)                                 # noqa: F821
            mb.torus(1.2, 0.06, 20, 6, col=WOOD, sh=AUTO)                                         # noqa: F821
        for k in range(8):
            a = 2 * math.pi * k / 8
            mb.beam((0, y, 0), (math.cos(a) * (R_ - 0.08), y, math.sin(a) * (R_ - 0.08)), 0.12, 0.1, WOOD, up=(0, 1, 0))  # noqa: F821
    for k in range(16):
        a = 2 * math.pi * (k + 0.5) / 16
        c, s_ = math.cos(a), math.sin(a)
        mb.beam((c * (R_ - 0.55), 0, s_ * (R_ - 0.55)), (c * (R_ + 0.08), 0, s_ * (R_ + 0.08)), 1.08, 0.07,
                WOOD_LT if k % 2 else WOOD, up=(0, 1, 0))
    mb.cyl((0, -0.7, 0), (0, 0.7, 0), 0.24, 0.24, 10, col=WOOD_DK)                                # noqa: F821
    mb.cyl((0, -1.1, 0), (0, 1.1, 0), 0.09, 0.09, 8, col=IRON)                                    # noqa: F821
    for y in (-0.62, 0.62):
        mb.cyl((0, y - 0.04, 0), (0, y + 0.04, 0), 0.3, 0.3, 10, col=IRON)                        # noqa: F821
    return mb


def make_arch_portal():
    """Stone portal for the sottoportego arches over the opes: jambs, a round arch of voussoirs with a keystone,
    a cornice and a lantern bracket. Opening 2.6 m wide, 3.1 m to the crown; 3.6 wide x 3.9 m, 0.35 m deep."""
    mb = MB("SM_KG_ArchPortal", angle=30)                                                         # noqa: F821
    rng = random.Random(31)
    for sx in (-1, 1):
        z = 0.0
        while z < 1.8:
            h = 0.36
            s0 = mb.cbox((sx * 1.55, -0.18, z + h / 2), (0.5 if int(z / h) % 2 else 0.42, 0.36, h - 0.01), 0.03,
                         pick(rng, GRANITE))                                                      # noqa: F821
            z += h
        mb.cbox((sx * 1.55, -0.2, 1.87), (0.56, 0.4, 0.12), 0.02, pick(rng, VOUSS))               # noqa: F821
    n = 13
    for k in range(n):
        a0, a1 = math.pi * k / n, math.pi * (k + 1) / n
        am = (a0 + a1) / 2
        r = 1.3 + 0.2
        c = (math.cos(am) * r, -0.18, 1.93 + math.sin(am) * r)
        with mb.xf(T(*c) @ R(-math.degrees(am) + 90, 'Y')):                                       # noqa: F821
            key = k == n // 2
            mb.cbox((0, 0, 0), (0.36 if not key else 0.46, 0.36, 0.4 if not key else 0.52), 0.03,
                    pick(rng, VOUSS) if not key else mix(WHITE, STONE_LT, 0.4))                   # noqa: F821
    mb.cbox((0, -0.2, 3.72), (3.7, 0.42, 0.16), 0.03, pick(rng, VOUSS))                           # noqa: F821
    for sx in (-1, 1):                                                                            # spandrel blocks
        mb.cbox((sx * 1.55, -0.16, 2.95), (0.56, 0.3, 1.45), 0.03, pick(rng, GRANITE))            # noqa: F821
    mb.beam((1.62, -0.35, 2.55), (1.62, -0.75, 2.55), 0.04, 0.04, IRON)                           # noqa: F821
    return mb


def make_buttress():
    """Battered stone buttress for the retaining walls: 0.9 m wide, 3.0 m high, 0.95 m deep at the foot, 0.35 at the top,
    with a sloped coping. Back face on Y = 0 (against the wall), body towards -Y."""
    mb = MB("SM_KG_Buttress", angle=30)                                                           # noqa: F821
    rng = random.Random(41)
    z, depth = 0.0, 0.95
    for k in range(6):
        h = 0.48
        d1 = 0.95 - (0.6 * (k + 1) / 6)
        s0 = mb.prism([(-0.45, 0.0), (0.45, 0.0), (0.45, -depth), (-0.45, -depth)], z, z + h - 0.01, pick(rng, GRANITE), b1=0.03)  # noqa: F821
        mb.recolor(s0, stone_noise(k))                                                            # noqa: F821
        z += h
        depth = d1 + 0.02
    with mb.xf(T(0, 0, 2.88)):                                                                    # noqa: F821
        mb.prism([(-0.5, 0.0), (0.5, 0.0), (0.5, -0.42), (-0.5, -0.42)], 0.0, 0.14, pick(rng, VOUSS), b1=0.03)  # noqa: F821
    mb.recolor(0, lambda p, c: mix(c, MOSS, 0.35) if p.z < 0.35 and nz(p, 3.0) > 0.2 else c)      # noqa: F821
    return mb


def make_wall_fountain():
    """Wall fountain for a retaining wall: an arched stone panel, a lion-mask spout, a half-round basin with water.
    1.8 m wide, 2.5 m high, basin 0.75 m deep (towards -Y); back on Y = 0."""
    mb = MB("SM_KG_WallFountain", angle=30)                                                       # noqa: F821
    rng = random.Random(51)
    s0 = mb.cbox((0, -0.1, 1.2), (1.8, 0.2, 2.4), 0.04, pick(rng, TERRACE))                        # noqa: F821
    with mb.xf(T(0, -0.2, 2.25) @ XZ_FRONT_NEG_Y):                                               # noqa: F821
        mb.lathe([(0.0, 0.0), (0.62, 0.0), (0.62, 0.08), (0.0, 0.08)], 18, col=pick(rng, VOUSS), sh=FLAT)  # noqa: F821
    mb.cbox((0, -0.22, 2.5), (1.9, 0.26, 0.14), 0.03, pick(rng, VOUSS))                           # noqa: F821
    mb.blob((0, -0.3, 1.45), (0.2, 0.14, 0.22), sub=2, amp=0.15, col=BRONZE)                      # noqa: F821
    mb.cyl((0, -0.38, 1.36), (0, -0.52, 1.3), 0.035, 0.03, 8, col=BRONZE_DK)                      # noqa: F821
    # basin: half round, rim 0.62 high
    half = [(math.cos(a) * 0.72, -math.sin(a) * 0.72 - 0.2) for a in [math.pi * k / 12 for k in range(13)]]
    s0 = mb.prism(half, 0.0, 0.62, pick(rng, VOUSS), b1=0.03)                                    # noqa: F821
    inner = [(math.cos(a) * 0.58, -math.sin(a) * 0.58 - 0.22) for a in [math.pi * k / 12 for k in range(13)]]
    mb.prism(inner, 0.0, 0.54, WATERC)                                                            # noqa: F821
    mb.cbox((0, -0.36, 0.84), (0.05, 0.05, 0.5), 0.01, C("BFE6F0"))                              # noqa: F821
    return mb


def make_telescope():
    """Belvedere telescope: a stone pedestal with a brass swivel and a brass tube aimed at the sea (front -Y)."""
    mb = MB("SM_KG_Telescope", angle=30)                                                          # noqa: F821
    rng = random.Random(61)
    mb.lathe([(0, 0), (0.34, 0), (0.34, 0.12), (0.22, 0.2), (0.18, 0.9), (0.26, 1.0), (0.26, 1.06), (0, 1.06)], 10,
             col=pick(rng, VOUSS), sh=AUTO)                                                       # noqa: F821
    mb.cyl((0, 0, 1.06), (0, 0, 1.2), 0.08, 0.08, 10, col=BRASS)                                  # noqa: F821
    with mb.xf(T(0, 0, 1.3) @ R(-15, 'X')):                                                       # noqa: F821
        mb.cyl((0, 0.45, 0), (0, -0.85, 0), 0.065, 0.1, 14, col=BRASS)                            # noqa: F821
        mb.cyl((0, -0.85, 0), (0, -0.92, 0), 0.115, 0.115, 14, col=BRONZE_DK)                     # noqa: F821
        mb.cyl((0, 0.45, 0), (0, 0.55, 0), 0.04, 0.03, 8, col=BRONZE_DK)                          # noqa: F821
    mb.cbox((0, 0, 1.25), (0.08, 0.2, 0.1), 0.01, BRASS)                                          # noqa: F821
    return mb


def make_lantern_plinth():
    mb = MB("SM_KG_LanternPlinth", angle=30)                                                      # noqa: F821
    rng = random.Random(71)
    s0 = mb.prism([(-0.55, -0.55), (0.55, -0.55), (0.55, 0.55), (-0.55, 0.55)], 0.0, 0.2, pick(rng, GRANITE), b1=0.03)  # noqa: F821
    mb.recolor(s0, stone_noise(2.0))                                                              # noqa: F821
    mb.prism([(-0.42, -0.42), (0.42, -0.42), (0.42, 0.42), (-0.42, 0.42)], 0.2, 0.36, pick(rng, VOUSS), b1=0.03)  # noqa: F821
    return mb


# ================================================================================================ cliffs
def _strata(mb, rng, w, h, d, seed, ledges=3, grass=True):
    """A layered rock mass w wide (X), h high, d deep (towards -Y), back on Y = 0: stacked slabs of varying depth
    and height (the strata), each slightly offset, rounded fronts, green tufts on the ledges."""
    z = 0.0
    k = 0
    heights = []
    while z < h - 0.3:
        hh = min(h - z, rng.uniform(0.7, 1.5))
        heights.append((z, hh))
        z += hh
    for i, (z0, hh) in enumerate(heights):
        # deeper at the bottom (a stepped, battered face), strongly irregular in plan (jutting and receding blocks)
        frac = i / max(1, len(heights) - 1)
        dd = d * (1.0 - 0.45 * frac) * rng.uniform(0.85, 1.05)
        n = 13
        xs = [-w / 2 + w * j / (n - 1) for j in range(n)]
        front = [(x + rng.uniform(-0.15, 0.15), -dd * (0.62 + 0.3 * math.sin(j * 1.3 + seed + i * 2.1)) - rng.uniform(0, 0.55))
                 for j, x in enumerate(xs)]
        wl = w * rng.uniform(0.86, 1.0)
        poly = [(-wl / 2, 0.0)] + [(max(-wl / 2, min(wl / 2, x)), y) for x, y in front] + [(wl / 2, 0.0)]
        # weathered colour: darker and cooler towards the sea, warm grey above, moss on the ledge tops
        base = mix(pick(rng, ROCKS, 0.12), ROCK_DK, 0.45 * (1.0 - frac) + rng.uniform(0.0, 0.2))   # noqa: F821
        s0 = mb.prism(poly, z0, z0 + hh, base, b1=0.18, b0=0.06)                                    # noqa: F821
        mb.recolor(s0, lambda p, c, s=seed + i, top=z0 + hh: mix(mul(c, 1.0 + 0.16 * nz(p, 1.3, (s, 2.0, 1.0))),
                                                                  GRASS_T2, 0.6 if (grass and p.z > top - 0.05 and nz(p, 0.8, (s, 5.0, 0.0)) > -0.2) else 0.0))  # noqa: F821
        k += 1
    return mb


def _cliff(name, w, h, d, seed):
    mb = MB(name, angle=40)                                                                       # noqa: F821
    rng = random.Random(seed)
    _strata(mb, rng, w, h, d, seed)
    return mb


def make_cliff_a():
    return _cliff("SM_KG_Cliff_A", 9.0, 5.5, 3.0, 101)


def make_cliff_b():
    return _cliff("SM_KG_Cliff_B", 7.0, 4.5, 2.6, 202)


def make_cliff_c():
    return _cliff("SM_KG_Cliff_C", 6.0, 6.5, 3.2, 303)


def make_talus():
    """Boulder apron for the waterline: 8 m wide, 2.8 m high, 4 m deep."""
    mb = MB("SM_KG_CliffTalus", angle=40)                                                         # noqa: F821
    rng = random.Random(404)
    for j in range(14):
        x = rng.uniform(-3.6, 3.6)
        dep = rng.uniform(0.4, 3.4)
        r = rng.uniform(0.6, 1.3) * (1.2 - dep / 5.0)
        z = max(r * 0.55, 2.4 - dep * 0.65)
        mb.blob((x, -dep, z * 0.8), (r * 1.25, r, r * 0.85), sub=1, amp=0.22, seed=j * 3.1, col=pick(rng, ROCKS), sh=FLAT)  # noqa: F821
    mb.recolor(0, lambda p, c: mix(c, ALGAE_DK, 0.55) if p.z < 0.45 else c)                       # noqa: F821
    return mb


# ================================================================================================ registry
PROPS = [
    ("ClockStage", make_clock_stage, "box"),
    ("Belfry", make_belfry, "complex"),
    ("Spire", make_spire, "complex"),
    ("ClockDial", make_clock_dial, "none"),
    ("ClockHand_Hour", make_hand_hour, "none"),
    ("ClockHand_Minute", make_hand_minute, "none"),
    ("Bell", make_bell, "none"),
    ("BellBeam", make_bell_beam, "none"),
    ("LighthouseGallery", make_gallery, "complex"),
    ("LampRoom", make_lamp_room, "complex"),
    ("Waterwheel", make_waterwheel, "none"),
    ("ArchPortal", make_arch_portal, "complex"),
    ("Buttress", make_buttress, "box"),
    ("WallFountain", make_wall_fountain, "complex"),
    ("Telescope", make_telescope, "box"),
    ("LanternPlinth", make_lantern_plinth, "box"),
    ("Cliff_A", make_cliff_a, "complex"),
    ("Cliff_B", make_cliff_b, "complex"),
    ("Cliff_C", make_cliff_c, "complex"),
    ("CliffTalus", make_talus, "complex"),
]
EXPECT = {}


def preview_tiles(names):
    H = "HUMAN"
    t1 = [dict(label=n, items=[(n, (0, 0, 0), 0), (H, (1.6, -0.5, 0), 0)]) for n, _, _ in PROPS if n in names]
    t1.append(dict(label="Lineup", items="LINEUP", az=-12, el=12))
    t2 = []
    if all(k in names for k in ("ClockStage", "Belfry", "Spire", "ClockDial")):
        t2.append(dict(label="Clock tower top (stage + belfry + spire)",
                       items=[("ClockStage", (0, 0, 0), 0), ("Belfry", (0, 0, 3.52), 0), ("Spire", (0, 0, 7.4), 0),
                              ("ClockDial", (0, -2.43, 1.98), 0), ("ClockHand_Hour", (0, -2.55, 1.98), 0),
                              ("ClockHand_Minute", (0, -2.55, 1.98), 0), (H, (3.2, -1.0, 0), 0)], az=-25, el=10))
    if all(k in names for k in ("LighthouseGallery", "LampRoom")):
        t2.append(dict(label="Lighthouse top", items=[("LighthouseGallery", (0, 0, 0), 0), ("LampRoom", (0, 0, 0.5), 0),
                                                      (H, (1.8, -0.4, 0.5), 0)], az=-25, el=12))
    if "Cliff_A" in names:
        t2.append(dict(label="Cliff cladding run", items=[("CliffTalus", (-4, -2.5, 0), 0), ("Cliff_A", (-4, 0, 0), 0),
                                                          ("Cliff_B", (3.8, 0.4, 0), 0), ("Cliff_C", (0, 1.4, 5.0), 0),
                                                          (H, (0, -5.5, 0), 0)], az=-20, el=8))
    keep = set(names)

    def ok(t):
        return t["items"] == "LINEUP" or all(k in keep or k in ("REF", "HUMAN", "WATER") for k, _, _ in t["items"])
    return [t for t in t1 if ok(t)], [t for t in t2 if ok(t)]


main()                                                                                            # noqa: F821
