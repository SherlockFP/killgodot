"""Procedural art for the role reveal "moment" (SPRINT-037, Source/KillGodot/UI/Reveal/SKGRoleReveal.cpp).

Same sticker style as the chat emoji (Tools/UI/kg_make_emoji.py: bold flat shapes, one shade, gloss, uniform ink
outline, soft drop shadow), drawn at 1024 px and downsampled. Everything is synthesised: CC0 by construction.

Outputs (default Art/UI/Reveal), 512 x 512 RGBA:
  T_KG_Reveal_Town.png        a lit harbour lantern      (Town: the ones who wait, the light that stays on)
  T_KG_Reveal_Impatient.png   a dagger through a cracked pocket watch (the Clockbreakers: time is up)
  T_KG_Reveal_Neutral.png     a two-faced carnival mask   (plays its own game)
  T_KG_Reveal_Mate0..2.png    villager busts (cap, wide hat, hood) in greys, tinted per accomplice in Slate
  _preview.png                contact sheet on the three alignment colours

  python Tools/UI/kg_make_reveal_art.py [out_dir]
then import (headless commandlet or live): Tools/Unreal/kg_import_reveal_art.py
"""
import math
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kg_make_emoji as E  # noqa: E402  (Canvas, palette, helpers)
from kg_make_emoji import Canvas, arc_pts, flame, hexc, rot_pts  # noqa: E402

S = E.S
ROOT = E.ROOT
OUT = 512

BRASS = hexc("F2C230")
BRASS_SHADE = hexc("B8861B")
IRON = hexc("4A4458")
IRON_SHADE = hexc("2B2633")
GLASS = hexc("FFE08A")
GLASS_SHADE = hexc("F28C28")
VIOLET = hexc("8E6AD8")
VIOLET_SHADE = hexc("5B3FA0")
BLOOD = hexc("E0213F")
GREY = hexc("EDEDED")
GREY_SHADE = hexc("A9A9B2")
GREY_DARK = hexc("C9C9D0")
GREY_DEEP = hexc("7E7E8A")


def rotated(mask, ang, cx=0.5, cy=0.5):
    img = Image.fromarray((mask * 255).astype(np.uint8)).rotate(ang, resample=Image.BICUBIC, center=(cx * S, cy * S))
    return np.asarray(img, np.float32) / 255.0


# ------------------------------------------------------------------------------------------------------------------
# Alignment illustrations
# ------------------------------------------------------------------------------------------------------------------

def town(c):
    # Warm halo, a hanging ring, iron cap, four glass panes with a big flame, iron base.
    glow = ndimage.gaussian_filter(c.circle(0.5, 0.55, 0.36), 46)
    c.paint(glow, hexc("FFC34D", 0.75), outline=False)
    c.stroke(arc_pts(0.5, 0.13, 0.075, 0.075, 180, 360), 0.038, IRON, outline=True)
    c.paint(c.poly([(0.27, 0.27), (0.73, 0.27), (0.64, 0.15), (0.36, 0.15)]), IRON, shade=IRON_SHADE)
    c.paint(c.rrect(0.44, 0.1, 0.56, 0.17, 0.02), IRON, shade=IRON_SHADE)
    glass = c.rrect(0.29, 0.26, 0.71, 0.79, 0.05)
    c.paint(glass, GLASS, shade=GLASS_SHADE, shade_offset=(0.07, 0.0))
    inner = ndimage.gaussian_filter(c.ellipse(0.5, 0.58, 0.16, 0.2), 18)
    c.paint(inner, hexc("FFF6D8", 0.9), outline=False, clip=glass)
    flame(c, 0.5, 0.52, 0.3, colors=(hexc("FF9A2E"), hexc("FFD27A"), hexc("FFFBEA")), outline=False)
    c.paint(c.rrect(0.44, 0.66, 0.56, 0.74, 0.02), hexc("F6E7C8"), outline=False)   # candle stub
    for x in (0.29, 0.5, 0.71):
        c.stroke([(x, 0.27), (x, 0.78)], 0.032, IRON)
    c.stroke([(0.29, 0.27), (0.71, 0.27)], 0.03, IRON)
    c.paint(c.rrect(0.22, 0.77, 0.78, 0.88, 0.035), IRON, shade=IRON_SHADE)
    c.paint(c.rrect(0.3, 0.87, 0.7, 0.92, 0.02), IRON, shade=IRON_SHADE)
    c.gloss(0.37, 0.42, 0.022, 0.12, glass, 0.75, rot=0)
    c.gloss(0.4, 0.2, 0.08, 0.02, c.poly([(0.27, 0.27), (0.73, 0.27), (0.64, 0.15), (0.36, 0.15)]), 0.35, rot=0)


def impatient(c):
    # A pocket watch, cracked, with a dagger driven through it from the top right.
    cx, cy = 0.44, 0.56
    c.paint(c.circle(cx, cy - 0.37, 0.05), BRASS, shade=BRASS_SHADE)       # crown
    c.stroke(arc_pts(cx, cy - 0.44, 0.06, 0.05, 180, 360), 0.03, BRASS, outline=True)
    rim = c.circle(cx, cy, 0.34)
    c.paint(rim, BRASS, shade=BRASS_SHADE)
    face = c.circle(cx, cy, 0.27)
    c.paint(face, hexc("FFF1D6"), shade=hexc("E0C79A"), shade_offset=(0.03, 0.03))
    for i in range(12):
        a = math.radians(i * 30)
        r0, r1 = (0.2, 0.25) if i % 3 else (0.17, 0.25)
        c.stroke([(cx + r0 * math.sin(a), cy - r0 * math.cos(a)), (cx + r1 * math.sin(a), cy - r1 * math.cos(a))],
                 0.018 if i % 3 else 0.03)
    # hands at five to midnight
    c.stroke([(cx, cy), (cx - 0.05, cy - 0.19)], 0.034)
    c.stroke([(cx, cy), (cx - 0.012, cy - 0.13)], 0.042, BLOOD)
    c.paint(c.circle(cx, cy, 0.03), E.INK, outline=False)
    # the crack
    c.stroke([(cx - 0.25, cy + 0.02), (cx - 0.15, cy + 0.06), (cx - 0.17, cy + 0.13), (cx - 0.06, cy + 0.17),
              (cx - 0.02, cy + 0.26)], 0.02)
    c.stroke([(cx - 0.15, cy + 0.06), (cx - 0.08, cy + 0.03)], 0.014)
    c.gloss(cx - 0.12, cy - 0.24, 0.12, 0.04, rim, 0.6)
    # dagger: blade enters the face, handle out to the top right
    ang = -38
    px, py = 0.6, 0.5   # pivot on the watch face
    blade = c.poly(rot_pts([(px - 0.045, py - 0.02), (px - 0.045, py + 0.36), (px, py + 0.47), (px + 0.045, py + 0.36),
                            (px + 0.045, py - 0.02)], ang, px, py))
    hidden = c.circle(cx, cy, 0.27)   # the part of the blade inside the watch is hidden (it went through)
    blade_vis = np.clip(blade - hidden * c.poly(rot_pts([(px - 0.2, py + 0.05), (px + 0.2, py + 0.05),
                                                         (px + 0.2, py + 0.6), (px - 0.2, py + 0.6)], ang, px, py)), 0, 1)
    c.paint(blade_vis, hexc("E6ECF2"), shade=hexc("8E9BA8"), shade_offset=(0.03, 0.0))
    c.paint(np.clip(blade_vis * c.poly(rot_pts([(px - 0.045, py - 0.02), (px - 0.045, py + 0.5), (px, py + 0.5),
                                                  (px, py - 0.02)], ang, px, py)), 0, 1), hexc("FFFFFF", 0.55), outline=False)
    # blood on the blade tip that shows below the watch
    tip = c.poly(rot_pts([(px - 0.045, py + 0.3), (px, py + 0.47), (px + 0.045, py + 0.3)], ang, px, py))
    c.paint(np.clip(tip - hidden, 0, 1), BLOOD, outline=False)
    guard = rotated(c.rrect(px - 0.13, py - 0.06, px + 0.13, py - 0.01, 0.02), -ang, px, py)
    handle = c.poly(rot_pts([(px - 0.04, py - 0.06), (px + 0.04, py - 0.06), (px + 0.045, py - 0.3), (px - 0.045, py - 0.3)],
                            ang, px, py))
    c.paint(handle, hexc("C8102E"), shade=hexc("7A0A1E"), shade_offset=(0.03, 0.0))
    for d in (0.12, 0.2):
        p = rot_pts([(px, py - d)], ang, px, py)[0]
        c.stroke(rot_pts([(px - 0.045, py - d), (px + 0.045, py - d)], ang, px, py), 0.014, BRASS)
        del p
    c.paint(guard, BRASS, shade=BRASS_SHADE, shade_offset=(0.01, 0.015))
    pom = rot_pts([(px, py - 0.33)], ang, px, py)[0]
    c.paint(c.circle(pom[0], pom[1], 0.045), BRASS, shade=BRASS_SHADE)
    # a few blood drops falling
    for (x, y, r) in ((0.28, 0.93, 0.022), (0.36, 0.97, 0.014)):
        c.paint(c.circle(x, y, r), BLOOD)


def neutral(c):
    # Carnival mask, split down the middle: cream smiling half, violet sneering half. Ribbons.
    pts = [(0.08, 0.36), (0.3, 0.26), (0.5, 0.33), (0.7, 0.26), (0.92, 0.36), (0.88, 0.62), (0.68, 0.78),
           (0.55, 0.7), (0.5, 0.73), (0.45, 0.7), (0.32, 0.78), (0.12, 0.62)]
    m = c.poly(pts)
    holes = np.maximum(c.ellipse(0.31, 0.47, 0.1, 0.065), rotated(c.ellipse(0.69, 0.48, 0.1, 0.05), -12, 0.69, 0.48))
    m = np.clip(m - holes, 0, 1)
    left = c.poly([(0.0, 0.0), (0.5, 0.0), (0.5, 1.0), (0.0, 1.0)])
    c.paint(m * left, hexc("FFF1D6"), shade=hexc("E0C79A"), shade_offset=(0.0, 0.06))
    c.paint(m * (1 - left), VIOLET, shade=VIOLET_SHADE, shade_offset=(0.0, 0.06))
    for x, rot in ((0.31, 0), (0.69, -12)):
        ring = arc_pts(x, 0.47, 0.115, 0.08 if x < 0.5 else 0.065, 0, 360, 40)
        c.stroke(rot_pts(ring, rot, x, 0.48) if rot else ring, 0.022, BRASS)
    c.stroke([(0.5, 0.33), (0.5, 0.72)], 0.018, BRASS)
    # smile (left) and a crooked sneer (right)
    c.stroke(arc_pts(0.38, 0.6, 0.08, 0.05, 20, 160), 0.022)
    c.stroke([(0.54, 0.64), (0.6, 0.62), (0.68, 0.64)], 0.022)
    # diamond on the brow
    c.paint(c.poly([(0.5, 0.36), (0.535, 0.4), (0.5, 0.44), (0.465, 0.4)]), BLOOD, outline=False)
    c.gloss(0.22, 0.36, 0.08, 0.025, m, 0.6, rot=-15)
    # ribbons
    for side in (-1, 1):
        x0 = 0.5 + side * 0.42
        c.stroke([(x0, 0.4), (x0 + side * 0.04, 0.55), (x0 + side * 0.01, 0.7), (x0 + side * 0.05, 0.86)], 0.04,
                 VIOLET if side > 0 else hexc("FFD23F"), outline=True)
    # a playing-card suit hovering above: the "own game"
    c.paint(c.poly([(0.5, 0.06), (0.56, 0.14), (0.5, 0.22), (0.44, 0.14)]), hexc("FFD23F"), shade=hexc("D98A1E"))


# ------------------------------------------------------------------------------------------------------------------
# Accomplice busts (grey, tinted in Slate)
# ------------------------------------------------------------------------------------------------------------------

def bust(c, hat):
    body = np.maximum(c.rrect(0.16, 0.62, 0.84, 1.3, 0.2), c.ellipse(0.5, 0.66, 0.3, 0.08))
    c.paint(body, GREY_DARK, shade=GREY_DEEP, shade_offset=(0.07, 0.0))
    c.paint(c.rrect(0.42, 0.52, 0.58, 0.66, 0.03), GREY_SHADE, outline=True)   # neck
    head = c.ellipse(0.5, 0.42, 0.17, 0.19)
    c.paint(head, GREY, shade=GREY_SHADE, shade_offset=(0.05, 0.02))
    c.gloss(0.44, 0.33, 0.05, 0.03, head, 0.5)
    # collar / scarf line
    c.stroke(arc_pts(0.5, 0.62, 0.14, 0.06, 20, 160), 0.022, GREY_DEEP)
    if hat == 0:   # fisherman's cap with a bill
        cap = np.maximum(c.ellipse(0.5, 0.29, 0.19, 0.12), c.rrect(0.31, 0.27, 0.69, 0.34, 0.02))
        c.paint(cap, GREY_DARK, shade=GREY_DEEP, shade_offset=(0.05, 0.0))
        c.paint(c.ellipse(0.58, 0.345, 0.2, 0.035), GREY_DEEP)
        c.paint(c.circle(0.5, 0.17, 0.03), GREY_DARK)
    elif hat == 1:  # wide-brimmed hat
        c.paint(c.ellipse(0.5, 0.31, 0.33, 0.055), GREY_DEEP, shade=hexc("5A5A66"), shade_offset=(0.0, 0.02))
        crown = c.rrect(0.35, 0.12, 0.65, 0.33, 0.06)
        c.paint(crown, GREY_DARK, shade=GREY_DEEP, shade_offset=(0.05, 0.0))
        c.paint(c.rrect(0.35, 0.26, 0.65, 0.3, 0.0), GREY_SHADE, outline=False, clip=crown)
    else:           # hood over the head and shoulders
        hood = np.maximum(c.ellipse(0.5, 0.4, 0.25, 0.28), c.poly([(0.25, 0.42), (0.75, 0.42), (0.86, 0.7), (0.14, 0.7)]))
        hood = np.clip(hood - c.ellipse(0.5, 0.45, 0.15, 0.17), 0, 1)
        c.paint(hood, GREY_DARK, shade=GREY_DEEP, shade_offset=(0.06, 0.0))
        c.paint(c.ellipse(0.5, 0.45, 0.15, 0.17), hexc("3A3A44"), outline=False, clip=head)   # shadowed face


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Art", "UI", "Reveal")
    os.makedirs(out_dir, exist_ok=True)
    jobs = [("T_KG_Reveal_Town", town), ("T_KG_Reveal_Impatient", impatient), ("T_KG_Reveal_Neutral", neutral)]
    for hat in range(3):
        jobs.append((f"T_KG_Reveal_Mate{hat}", lambda c, h=hat: bust(c, h)))
    images = []
    for name, fn in jobs:
        c = Canvas()
        fn(c)
        img = c.render(OUT, 7)
        img.save(os.path.join(out_dir, name + ".png"))
        images.append(img)
        print(f"KG_REVEAL_ART {name} {img.size[0]}x{img.size[1]}")
    # Contact sheet: each illustration on its alignment flood, busts tinted crimson.
    cell = 256
    sheet = Image.new("RGBA", (cell * len(images), cell), (0, 0, 0, 255))
    backs = [(18, 60, 40), (70, 8, 20), (40, 22, 70), (70, 8, 20), (70, 8, 20), (70, 8, 20)]
    for i, img in enumerate(images):
        bg = Image.new("RGBA", (cell, cell), backs[i] + (255,))
        im = img.resize((cell, cell), Image.LANCZOS)
        if i >= 3:
            arr = np.asarray(im, np.float32) / 255.0
            arr[..., :3] *= np.array([1.0, 0.28, 0.37], np.float32)
            im = Image.fromarray((arr * 255).astype(np.uint8), "RGBA")
        bg.alpha_composite(im)
        sheet.paste(bg, (i * cell, 0))
    sheet.save(os.path.join(out_dir, "_preview.png"))


if __name__ == "__main__":
    main()
