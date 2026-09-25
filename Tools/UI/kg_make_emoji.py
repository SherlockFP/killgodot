"""Procedural emoji set for the Kill Godot chat (Docs/08_UI_UX.md "Sohbet").

Slate cannot draw colour emoji fonts, so the game ships its own small sticker-style set rendered as images:
bold flat shapes, one lower-right shade, a glossy highlight, a uniform dark-plum outline and a soft drop shadow.
Everything is drawn at 1024 px and downsampled (LANCZOS), so edges stay crisp at every size.

Outputs (default Art/UI/Emoji):
  <id>.png                    160 px stickers (review / reuse)
  T_KG_EmojiAtlas.png         8 x 4 cells of 160 px  (reaction bubbles, picker)
  T_KG_EmojiAtlas_Small.png   8 x 4 cells of 48 px   (inline chat text; thicker outline for small sizes)
  _preview.png                contact sheet on dark + light backgrounds with names

The ORDER below is the emoji index used by the game (Source/KillGodot/Chat/KGEmoji.cpp). Append only.

  python Tools/UI/kg_make_emoji.py [out_dir]
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

S = 1024                      # working resolution
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONT = r"D:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts\Roboto-Black.ttf"

ORDER = [
    "laugh", "sus", "skull", "thumbsup", "thumbsdown", "heart", "wave", "angry",
    "smile", "cry", "shock", "sweat", "zzz", "wink", "clap", "pray",
    "knife", "eyes", "ghost", "candle", "fire", "fish", "bell", "anchor",
    "question", "exclamation", "crown", "clock", "mask", "lantern", "mug", "moon",
]


def hexc(h, a=1.0):
    h = h.lstrip("#")
    return (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0, a)


# Game palette (Tools/Blender/kg_common.py PALETTE + HUD colours).
INK = hexc("2A1833")
FACE = hexc("FFD23F")
FACE_SHADE = hexc("F29E2E")
BLUSH = hexc("F28AA0", 0.55)
WHITE = hexc("FFFDF5")
TEAR = hexc("7FD3F0")
TEAR_SHADE = hexc("3FA6D9")
RED = hexc("E0413A")
RED_SHADE = hexc("A8233A")
TONGUE = hexc("FF6F61")
BONE = hexc("F4ECD6")
BONE_SHADE = hexc("C9B895")
STEEL = hexc("D5DCE3")
STEEL_SHADE = hexc("8E9BA8")
WOOD = hexc("A0612B")
WOOD_SHADE = hexc("6B3D1C")
GOLD = hexc("FFC940")
GOLD_SHADE = hexc("D98A1E")
SLEEVE = hexc("2F5BD3")
SLEEVE_SHADE = hexc("1F3C94")
GHOST = hexc("F2FFFF")
GHOST_SHADE = hexc("8FE3F0")
EMBER = hexc("FF5A1F")
FLAME = hexc("FFB347")
CORE = hexc("FFF3B0")
SEA = hexc("1FB5C4")
SEA_SHADE = hexc("0E6E8C")
CRIMSON = hexc("D8243F")
CRIMSON_SHADE = hexc("8A1030")
CREAM = hexc("FFF1D6")
CREAM_SHADE = hexc("E0C79A")
LILAC = hexc("C9C3E6")
ORANGE = hexc("F28C28")
ORANGE_SHADE = hexc("C0561A")
MOON = hexc("FFE58A")
MOON_SHADE = hexc("E8B24A")
NAVY = hexc("3B63C9")
NAVY_SHADE = hexc("24418F")


class Canvas:
    """Float RGBA painter. Filled shapes join the silhouette that later receives the outline."""

    def __init__(self):
        self.rgb = np.zeros((S, S, 3), np.float32)
        self.alpha = np.zeros((S, S), np.float32)
        self.sil = np.zeros((S, S), np.float32)

    # ---- rasterisers (normalised 0..1 coordinates) ----
    @staticmethod
    def _mask(draw_fn):
        img = Image.new("L", (S, S), 0)
        draw_fn(ImageDraw.Draw(img))
        return np.asarray(img, np.float32) / 255.0

    @staticmethod
    def px(p):
        return (p[0] * S, p[1] * S)

    def ellipse(self, cx, cy, rx, ry):
        return self._mask(lambda d: d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=255))

    def circle(self, cx, cy, r):
        return self.ellipse(cx, cy, r, r)

    def poly(self, pts):
        return self._mask(lambda d: d.polygon([self.px(p) for p in pts], fill=255))

    def rrect(self, x0, y0, x1, y1, r):
        return self._mask(lambda d: d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=255))

    def stroke_mask(self, pts, width):
        def fn(d):
            w = max(1, int(width * S))
            ps = [self.px(p) for p in pts]
            d.line(ps, fill=255, width=w, joint="curve")
            for p in (ps[0], ps[-1]):
                d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=255)
        return self._mask(fn)

    def text_mask(self, text, cx, cy, size, rot=0.0):
        font = ImageFont.truetype(FONT, int(size * S))
        img = Image.new("L", (S, S), 0)
        d = ImageDraw.Draw(img)
        d.text((cx * S, cy * S), text, font=font, fill=255, anchor="mm")
        if rot:
            img = img.rotate(rot, resample=Image.BICUBIC, center=(cx * S, cy * S))
        return np.asarray(img, np.float32) / 255.0

    # ---- painting ----
    def paint(self, mask, color, outline=True, shade=None, shade_offset=(0.045, 0.05), clip=None):
        if clip is not None:
            mask = mask * clip
        r, g, b, a = color
        m = mask * a
        self.rgb = self.rgb * (1 - m[..., None]) + np.array([r, g, b], np.float32) * m[..., None]
        self.alpha = np.maximum(self.alpha, m) if a >= 1.0 else self.alpha + m * (1 - self.alpha)
        if outline:
            self.sil = np.maximum(self.sil, mask)
        if shade is not None:
            dx, dy = int(shade_offset[0] * S), int(shade_offset[1] * S)
            shifted = np.zeros_like(mask)
            shifted[: S - dy, : S - dx] = mask[dy:, dx:]
            crescent = np.clip(mask - shifted, 0, 1)
            crescent = ndimage.gaussian_filter(crescent, 3)
            self.paint(crescent * mask, shade, outline=False)
        return mask

    def stroke(self, pts, width, color=INK, outline=False):
        return self.paint(self.stroke_mask(pts, width), color, outline=outline)

    def gloss(self, cx, cy, rx, ry, clip, alpha=0.55, rot=-25):
        img = Image.new("L", (S, S), 0)
        ImageDraw.Draw(img).ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=255)
        img = img.rotate(rot, resample=Image.BICUBIC, center=(cx * S, cy * S)).filter(ImageFilter.GaussianBlur(4))
        m = np.asarray(img, np.float32) / 255.0
        self.paint(m, (1, 1, 1, alpha), outline=False, clip=clip)

    # ---- output ----
    def render(self, size, outline_px):
        """Composite shadow + outline + art and downsample to size x size."""
        r = outline_px * S / size
        dist = ndimage.distance_transform_edt(self.sil < 0.5)
        ring = np.clip(r + 0.5 - dist, 0, 1).astype(np.float32)
        body = np.maximum(ring, self.sil)
        # soft drop shadow
        shadow = np.zeros_like(body)
        off = int(0.022 * S)
        shadow[off:, :] = body[:-off, :]
        shadow = ndimage.gaussian_filter(shadow, 0.018 * S) * 0.35
        out_a = np.maximum(shadow, body)
        col = np.zeros((S, S, 3), np.float32)
        col[:] = np.array(INK[:3], np.float32)
        a = np.clip(self.alpha, 0, 1)[..., None]
        col = col * (1 - a) + self.rgb * a
        col = np.where((body > 0)[..., None], col, np.array([0.1, 0.06, 0.12], np.float32))
        # un-premultiplied RGBA; shadow pixels take the ink colour
        rgba = np.dstack([col, np.maximum(out_a, np.clip(self.alpha, 0, 1))])
        img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), "RGBA")
        # premultiply for resampling, then un-premultiply
        return downsample(img, size)


def downsample(img, size):
    arr = np.asarray(img, np.float32) / 255.0
    pre = arr.copy()
    pre[..., :3] *= pre[..., 3:4]
    pim = Image.fromarray((pre * 255).astype(np.uint8), "RGBA").resize((size, size), Image.LANCZOS)
    p = np.asarray(pim, np.float32) / 255.0
    a = p[..., 3:4]
    rgb = np.where(a > 1e-4, p[..., :3] / np.maximum(a, 1e-4), 0)
    return Image.fromarray((np.clip(np.dstack([rgb, a]), 0, 1) * 255).astype(np.uint8), "RGBA")


def rot_pts(pts, ang_deg, cx=0.5, cy=0.5):
    a = math.radians(ang_deg)
    ca, sa = math.cos(a), math.sin(a)
    return [(cx + (x - cx) * ca - (y - cy) * sa, cy + (x - cx) * sa + (y - cy) * ca) for x, y in pts]


def arc_pts(cx, cy, rx, ry, a0, a1, n=24):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


# ------------------------------------------------------------------------------------------------------------------
# Faces
# ------------------------------------------------------------------------------------------------------------------

def face(c, fill=FACE, shade=FACE_SHADE, blush=True, cy=0.52):
    m = c.paint(c.circle(0.5, cy, 0.40), fill, shade=shade, shade_offset=(0.05, 0.06))
    c.gloss(0.36, cy - 0.23, 0.13, 0.07, m, 0.6)
    if blush:
        c.paint(c.ellipse(0.27, cy + 0.12, 0.07, 0.045), BLUSH, outline=False, clip=m)
        c.paint(c.ellipse(0.73, cy + 0.12, 0.07, 0.045), BLUSH, outline=False, clip=m)
    return m


def dot_eyes(c, y=0.45, dx=0.13, rx=0.045, ry=0.07):
    for x in (0.5 - dx, 0.5 + dx):
        c.paint(c.ellipse(x, y, rx, ry), INK, outline=False)
        c.paint(c.ellipse(x - rx * 0.3, y - ry * 0.4, rx * 0.35, ry * 0.28), WHITE, outline=False)


def happy_eyes(c, y=0.44, dx=0.14, w=0.05):
    for x in (0.5 - dx, 0.5 + dx):
        c.stroke(arc_pts(x, y + 0.035, 0.075, 0.06, 200, 340), w)


def e_smile(c):
    face(c)
    dot_eyes(c)
    c.stroke(arc_pts(0.5, 0.56, 0.17, 0.13, 20, 160), 0.05)


def e_laugh(c):
    m = face(c, blush=False)
    # squeezed eyes > <
    c.stroke([(0.29, 0.36), (0.39, 0.42), (0.29, 0.47)], 0.05)
    c.stroke([(0.71, 0.36), (0.61, 0.42), (0.71, 0.47)], 0.05)
    mouth = c.poly([(0.27, 0.56)] + arc_pts(0.5, 0.56, 0.23, 0.22, 0, 180)[::-1] + [(0.73, 0.56)])
    mouth = c.poly(arc_pts(0.5, 0.555, 0.23, 0.22, 0, 180))
    c.paint(mouth, INK, outline=False)
    c.paint(c.ellipse(0.5, 0.74, 0.12, 0.07) * mouth, TONGUE, outline=False)
    c.paint(c.rrect(0.31, 0.55, 0.69, 0.60, 0.02) * mouth, WHITE, outline=False)
    # tears of joy
    for side in (-1, 1):
        x = 0.5 + side * 0.36
        pts = [(x, 0.42), (x + side * 0.1, 0.55), (x + side * 0.07, 0.66), (x - side * 0.02, 0.66), (x - side * 0.04, 0.55)]
        tm = c.poly(pts)
        tm = np.maximum(tm, c.ellipse(x + side * 0.03, 0.62, 0.07, 0.065))
        c.paint(tm, TEAR, shade=TEAR_SHADE, shade_offset=(0.02, 0.025))
        c.gloss(x + side * 0.01, 0.57, 0.02, 0.03, tm, 0.8, rot=0)


def e_sus(c):
    face(c, blush=False)
    # half-lidded side-eye
    for x in (0.37, 0.65):
        ew = c.ellipse(x, 0.46, 0.09, 0.075)
        c.paint(ew, WHITE, outline=False)
        c.paint(c.ellipse(x + 0.045, 0.475, 0.042, 0.05) * ew, INK, outline=False)
        lid = c.rrect(x - 0.12, 0.33, x + 0.12, 0.455, 0.0) * ew
        c.paint(lid, FACE_SHADE, outline=False)
        c.stroke([(x - 0.095, 0.455), (x + 0.095, 0.455)], 0.03)
    # raised brow + flat brow
    c.stroke(arc_pts(0.37, 0.33, 0.1, 0.06, 200, 330), 0.04)
    c.stroke([(0.56, 0.35), (0.74, 0.37)], 0.04)
    # smirk
    c.stroke([(0.38, 0.69), (0.5, 0.69), (0.63, 0.65)], 0.045)


def e_angry(c):
    m = c.paint(c.circle(0.5, 0.52, 0.40), hexc("FF7A4D"), shade=RED_SHADE, shade_offset=(0.05, 0.06))
    # red flush from the top
    grad = np.clip((0.75 - np.linspace(0, 1, S)) * 1.6, 0, 1)[:, None] * np.ones((1, S))
    c.paint(grad.astype(np.float32) * m, hexc("E0413A", 0.9), outline=False)
    c.gloss(0.36, 0.29, 0.13, 0.07, m, 0.45)
    c.stroke([(0.24, 0.34), (0.43, 0.43)], 0.055)
    c.stroke([(0.76, 0.34), (0.57, 0.43)], 0.055)
    dot_eyes(c, y=0.5, dx=0.13, rx=0.04, ry=0.055)
    c.stroke(arc_pts(0.5, 0.79, 0.15, 0.1, 205, 335), 0.055)
    # anger vein
    for a in (0, 90, 180, 270):
        p = rot_pts([(0.80, 0.14), (0.86, 0.18)], a, 0.83, 0.16)
        c.stroke(p, 0.035, RED, outline=True)


def e_cry(c):
    m = face(c, blush=False)
    for x in (0.37, 0.63):
        c.stroke(arc_pts(x, 0.43, 0.07, 0.05, 20, 160), 0.045)
    c.stroke(arc_pts(0.5, 0.77, 0.12, 0.08, 200, 340), 0.05)
    # streams
    for x in (0.37, 0.63):
        c.paint(c.rrect(x - 0.035, 0.47, x + 0.035, 0.9, 0.035) * m, TEAR, outline=False)
        c.paint(c.rrect(x - 0.01, 0.5, x + 0.012, 0.86, 0.01) * m, hexc("DDF6FF", 0.9), outline=False)
    drop = c.poly([(0.84, 0.46), (0.9, 0.58), (0.86, 0.63), (0.8, 0.62), (0.78, 0.56)])
    drop = np.maximum(drop, c.circle(0.84, 0.585, 0.055))
    c.paint(drop, TEAR, shade=TEAR_SHADE, shade_offset=(0.015, 0.02))


def e_shock(c):
    face(c)
    for x in (0.37, 0.63):
        c.paint(c.ellipse(x, 0.43, 0.085, 0.1), WHITE, outline=False)
        c.stroke(arc_pts(x, 0.43, 0.085, 0.1, 0, 360, 40), 0.025)
        c.paint(c.circle(x, 0.44, 0.03), INK, outline=False)
    c.stroke(arc_pts(0.37, 0.27, 0.08, 0.04, 200, 340), 0.035)
    c.stroke(arc_pts(0.63, 0.27, 0.08, 0.04, 200, 340), 0.035)
    c.paint(c.ellipse(0.5, 0.71, 0.075, 0.1), INK, outline=False)
    c.paint(c.ellipse(0.5, 0.76, 0.05, 0.04), TONGUE, outline=False, clip=c.ellipse(0.5, 0.71, 0.075, 0.1))


def e_sweat(c):
    face(c)
    happy_eyes(c)
    mouth = c.poly(arc_pts(0.5, 0.58, 0.19, 0.16, 0, 180))
    c.paint(mouth, INK, outline=False)
    c.paint(c.rrect(0.32, 0.575, 0.68, 0.62, 0.02) * mouth, WHITE, outline=False)
    drop = c.poly([(0.80, 0.13), (0.9, 0.29), (0.87, 0.36), (0.79, 0.37), (0.75, 0.3)])
    drop = np.maximum(drop, c.circle(0.825, 0.305, 0.068))
    c.paint(drop, TEAR, shade=TEAR_SHADE, shade_offset=(0.02, 0.02))
    c.gloss(0.81, 0.28, 0.018, 0.03, drop, 0.9, rot=0)


def e_zzz(c):
    face(c, cy=0.56)
    for x in (0.37, 0.63):
        c.stroke(arc_pts(x, 0.5, 0.07, 0.045, 20, 160), 0.045)
    c.paint(c.ellipse(0.52, 0.76, 0.05, 0.045), INK, outline=False)
    # bubble from the nose
    b = c.circle(0.3, 0.7, 0.07)
    c.paint(b, hexc("BFE3F5", 0.85), outline=True)
    c.gloss(0.28, 0.68, 0.02, 0.015, b, 0.9, rot=0)
    for (x, y, s, r) in ((0.72, 0.2, 0.2, 10), (0.87, 0.07, 0.13, 14)):
        c.paint(c.text_mask("Z", x, y, s, r), hexc("B8A9E3"), outline=True)


def e_wink(c):
    face(c)
    c.paint(c.ellipse(0.37, 0.45, 0.045, 0.07), INK, outline=False)
    c.paint(c.ellipse(0.355, 0.42, 0.016, 0.02), WHITE, outline=False)
    c.stroke(arc_pts(0.63, 0.47, 0.07, 0.05, 200, 340), 0.045)
    c.stroke(arc_pts(0.5, 0.58, 0.17, 0.12, 20, 160), 0.05)
    c.paint(c.ellipse(0.58, 0.72, 0.05, 0.05), TONGUE, outline=False)
    c.stroke(arc_pts(0.5, 0.58, 0.17, 0.12, 20, 160), 0.05)


# ------------------------------------------------------------------------------------------------------------------
# Hands (yellow, like the faces, with a blue villager sleeve)
# ------------------------------------------------------------------------------------------------------------------

def thumb_hand(c, flip=False):
    parts = []
    fist = c.rrect(0.33, 0.42, 0.76, 0.86, 0.1)
    thumb = c.poly(rot_pts([(0.36, 0.44), (0.40, 0.12), (0.47, 0.08), (0.54, 0.12), (0.56, 0.44)], -8, 0.47, 0.3))
    thumb = np.maximum(thumb, c.circle(0.465, 0.13, 0.075))
    sleeve = c.rrect(0.14, 0.44, 0.35, 0.9, 0.05)
    if flip:
        fist, thumb, sleeve = (np.flipud(m) for m in (fist, thumb, sleeve))
    c.paint(sleeve, SLEEVE, shade=SLEEVE_SHADE)
    hand = np.maximum(fist, thumb)
    c.paint(hand, FACE, shade=FACE_SHADE)
    c.gloss(0.45, 0.2 if not flip else 0.8, 0.03, 0.06, hand, 0.6, rot=0)
    # finger creases
    ys = (0.53, 0.64, 0.75)
    for y in ys:
        yy = 1 - y if flip else y
        c.stroke([(0.58, yy), (0.74, yy)], 0.028)
    c.stroke([(0.35, 0.46 if not flip else 0.54), (0.56, 0.46 if not flip else 0.54)], 0.028)
    # cuff
    cuff = c.rrect(0.3, 0.44, 0.37, 0.9, 0.02)
    if flip:
        cuff = np.flipud(cuff)
    c.paint(cuff, WHITE, outline=False)


def e_thumbsup(c):
    thumb_hand(c)


def e_thumbsdown(c):
    thumb_hand(c, flip=True)


def open_hand(c, ang, cx=0.5, cy=0.55, scale=1.0, sleeve=True):
    def T(pts):
        return rot_pts([(cx + (x - 0.5) * scale, cy + (y - 0.55) * scale) for x, y in pts], ang, cx, cy)
    palm = c.poly(T([(0.3, 0.5), (0.7, 0.5), (0.72, 0.72), (0.62, 0.86), (0.38, 0.86), (0.28, 0.72)]))
    m = palm
    for fx, top in ((0.34, 0.2), (0.45, 0.13), (0.56, 0.15), (0.66, 0.24)):
        m = np.maximum(m, c.stroke_mask(T([(fx, 0.55), (fx, top)]), 0.1 * scale))
    m = np.maximum(m, c.stroke_mask(T([(0.3, 0.72), (0.2, 0.55)]), 0.1 * scale))
    if sleeve:
        sl = c.poly(T([(0.34, 0.84), (0.66, 0.84), (0.68, 1.02), (0.32, 1.02)]))
        c.paint(sl, SLEEVE, shade=SLEEVE_SHADE)
        c.paint(c.poly(T([(0.34, 0.84), (0.66, 0.84), (0.665, 0.885), (0.335, 0.885)])), WHITE, outline=False)
    c.paint(m, FACE, shade=FACE_SHADE)
    c.stroke(T([(0.4, 0.66), (0.47, 0.72)]), 0.025 * scale)
    return m


def e_wave(c):
    m = open_hand(c, 18, cx=0.52, cy=0.56, scale=0.95)
    c.gloss(0.46, 0.3, 0.03, 0.06, m, 0.5, rot=18)
    for r in (0.3, 0.38):
        c.stroke(arc_pts(0.48, 0.5, r, r, 190, 225, 12), 0.035, hexc("7FD3F0"), outline=True)
        c.stroke(arc_pts(0.56, 0.5, r, r, -45, -10, 12), 0.035, hexc("7FD3F0"), outline=True)


def e_clap(c):
    open_hand(c, -28, cx=0.43, cy=0.6, scale=0.82)
    open_hand(c, 28, cx=0.6, cy=0.6, scale=0.82)
    for a in (-60, -90, -120):
        x0 = 0.52 + 0.3 * math.cos(math.radians(a))
        y0 = 0.42 + 0.3 * math.sin(math.radians(a))
        x1 = 0.52 + 0.42 * math.cos(math.radians(a))
        y1 = 0.42 + 0.42 * math.sin(math.radians(a))
        c.stroke([(x0, y0), (x1, y1)], 0.04, GOLD, outline=True)


def e_pray(c):
    # rays
    for a in range(-150, -20, 26):
        x0, y0 = 0.5 + 0.34 * math.cos(math.radians(a)), 0.45 + 0.34 * math.sin(math.radians(a))
        x1, y1 = 0.5 + 0.45 * math.cos(math.radians(a)), 0.45 + 0.45 * math.sin(math.radians(a))
        c.stroke([(x0, y0), (x1, y1)], 0.035, GOLD, outline=True)
    for side in (-1, 1):
        sl = c.poly([(0.5, 0.74), (0.5 + side * 0.21, 0.72), (0.5 + side * 0.25, 0.95), (0.5, 0.95)])
        c.paint(sl, SLEEVE, shade=SLEEVE_SHADE)
        c.paint(c.poly([(0.5, 0.74), (0.5 + side * 0.21, 0.72), (0.5 + side * 0.215, 0.775), (0.5, 0.79)]), WHITE,
                outline=False)
        pts = [(0.5, 0.14), (0.5 + side * 0.08, 0.17), (0.5 + side * 0.13, 0.3), (0.5 + side * 0.2, 0.56),
               (0.5 + side * 0.21, 0.7), (0.5 + side * 0.16, 0.78), (0.5, 0.8)]
        h = c.poly(pts)
        h = np.maximum(h, c.circle(0.5 + side * 0.045, 0.19, 0.055))
        c.paint(h, FACE, shade=FACE_SHADE if side > 0 else None)
        # fingertips + thumb crease
        c.stroke([(0.5 + side * 0.17, 0.62), (0.5 + side * 0.08, 0.7)], 0.024)
    c.stroke([(0.5, 0.15), (0.5, 0.79)], 0.025)


# ------------------------------------------------------------------------------------------------------------------
# Objects
# ------------------------------------------------------------------------------------------------------------------

def e_skull(c):
    cran = np.maximum(c.ellipse(0.5, 0.43, 0.36, 0.33), c.rrect(0.29, 0.5, 0.71, 0.86, 0.08))
    m = c.paint(cran, BONE, shade=BONE_SHADE)
    c.gloss(0.36, 0.22, 0.1, 0.05, m, 0.7)
    for x in (0.36, 0.64):
        c.paint(c.ellipse(x, 0.5, 0.1, 0.095), INK, outline=False)
        c.paint(c.circle(x + 0.02, 0.51, 0.025), hexc("FF6F61", 0.9), outline=False)
    c.paint(c.poly([(0.5, 0.6), (0.455, 0.69), (0.545, 0.69)]), INK, outline=False)
    for x in (0.39, 0.46, 0.54, 0.61):
        c.stroke([(x, 0.76), (x, 0.86)], 0.022)
    c.stroke([(0.34, 0.76), (0.66, 0.76)], 0.022)
    # crack
    c.stroke([(0.62, 0.12), (0.6, 0.2), (0.65, 0.25), (0.62, 0.3)], 0.02)


def e_knife(c):
    ang = -45
    blade = c.poly(rot_pts([(0.44, 0.58), (0.44, 0.2), (0.5, 0.02), (0.58, 0.22), (0.58, 0.58)], ang))
    c.paint(blade, STEEL, shade=STEEL_SHADE, shade_offset=(0.03, 0.0))
    c.paint(c.poly(rot_pts([(0.44, 0.58), (0.44, 0.2), (0.5, 0.02), (0.49, 0.22), (0.49, 0.58)], ang)),
            hexc("FFFFFF", 0.6), outline=False)
    guard = c.rrect(0.36, 0.56, 0.66, 0.63, 0.03)
    guard = np.asarray(Image.fromarray((guard * 255).astype(np.uint8)).rotate(-ang, resample=Image.BICUBIC,
                                                                            center=(S / 2, S / 2)), np.float32) / 255
    handle = c.poly(rot_pts([(0.45, 0.62), (0.57, 0.62), (0.58, 0.95), (0.44, 0.95)], ang))
    c.paint(handle, WOOD, shade=WOOD_SHADE, shade_offset=(0.03, 0.0))
    c.paint(guard, GOLD, shade=GOLD_SHADE, shade_offset=(0.01, 0.015))
    for y in (0.72, 0.85):
        p = rot_pts([(0.51, y)], ang)[0]
        c.paint(c.circle(p[0], p[1], 0.022), GOLD, outline=False)


def e_heart(c):
    pts = []
    for i in range(80):
        t = i / 80.0 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((0.5 + x * 0.026, 0.47 - y * 0.026))
    m = c.paint(c.poly(pts), RED, shade=RED_SHADE, shade_offset=(0.05, 0.06))
    c.gloss(0.32, 0.3, 0.09, 0.05, m, 0.7, rot=-35)
    c.paint(c.circle(0.7, 0.3, 0.03), hexc("FFFFFF", 0.8), outline=False, clip=m)


def e_eyes(c):
    for x in (0.3, 0.7):
        e = c.ellipse(x, 0.5, 0.18, 0.28)
        c.paint(e, WHITE, shade=hexc("C9C3E6"), shade_offset=(0.03, 0.04))
        c.paint(c.ellipse(x - 0.07, 0.52, 0.075, 0.1) * e, INK, outline=False)
        c.paint(c.circle(x - 0.09, 0.49, 0.022), WHITE, outline=False)


def e_ghost(c):
    top = c.ellipse(0.5, 0.42, 0.32, 0.32)
    body = c.rrect(0.18, 0.42, 0.82, 0.8, 0.0)
    m = np.maximum(top, body)
    for i, x in enumerate((0.26, 0.42, 0.58, 0.74)):
        m = np.maximum(m, c.circle(x, 0.8, 0.08))
    bumps = np.zeros_like(m)
    for x in (0.34, 0.5, 0.66):
        bumps = np.maximum(bumps, c.circle(x, 0.88, 0.06))
    m = np.clip(m - bumps * (c.rrect(0.1, 0.82, 0.9, 1.0, 0) > 0), 0, 1)
    arms = np.maximum(c.ellipse(0.16, 0.56, 0.07, 0.1), c.ellipse(0.84, 0.52, 0.07, 0.1))
    c.paint(arms, GHOST, shade=GHOST_SHADE)
    c.paint(m, GHOST, shade=GHOST_SHADE, shade_offset=(0.06, 0.05))
    c.gloss(0.37, 0.2, 0.1, 0.05, m, 0.8)
    for x in (0.4, 0.6):
        c.paint(c.ellipse(x, 0.43, 0.05, 0.075), INK, outline=False)
    c.paint(c.ellipse(0.5, 0.6, 0.05, 0.06), INK, outline=False)
    c.paint(c.ellipse(0.3, 0.53, 0.05, 0.03), BLUSH, outline=False)
    c.paint(c.ellipse(0.7, 0.53, 0.05, 0.03), BLUSH, outline=False)


def flame(c, cx, cy, s, colors=(EMBER, FLAME, CORE), outline=True):
    def fl(scale, dy):
        pts = []
        for i in range(41):
            t = i / 40.0 * 2 * math.pi
            # teardrop: round bottom, pointy top
            x = math.sin(t) * (1 - math.cos(t) * 0.0) * 0.5 * (0.5 + 0.5 * math.cos(t * 0.5) ** 2)
            y = -math.cos(t) * 0.5
            w = (1 + math.cos(t)) * 0.5
            x = math.sin(t) * 0.42 * (w ** 0.6)
            pts.append((cx + x * s * scale, cy + dy + (y + 0.12) * s * scale * 1.35))
        return c.poly(pts)
    m = c.paint(fl(1.0, 0), colors[0], outline=outline, shade=RED_SHADE, shade_offset=(0.03, 0.03))
    c.paint(fl(0.68, s * 0.12), colors[1], outline=False)
    c.paint(fl(0.38, s * 0.22), colors[2], outline=False)
    return m


def e_fire(c):
    flame(c, 0.5, 0.5, 0.78)
    flame(c, 0.26, 0.66, 0.34)
    flame(c, 0.76, 0.64, 0.38)


def e_candle(c):
    glow = ndimage.gaussian_filter(c.circle(0.5, 0.28, 0.2), 30)
    c.paint(glow, hexc("FFD27A", 0.55), outline=False)
    wax = c.rrect(0.34, 0.42, 0.66, 0.9, 0.05)
    drip = np.maximum(c.rrect(0.52, 0.4, 0.6, 0.6, 0.04), c.circle(0.56, 0.6, 0.04))
    body = np.maximum(wax, drip)
    c.paint(c.ellipse(0.5, 0.9, 0.25, 0.06), hexc("C0C6CC"), shade=STEEL_SHADE, shade_offset=(0.02, 0.02))
    c.paint(body, CREAM, shade=CREAM_SHADE, shade_offset=(0.06, 0.0))
    c.paint(c.ellipse(0.5, 0.42, 0.16, 0.035), hexc("FFFBF0"), outline=False)
    c.stroke([(0.5, 0.42), (0.5, 0.35)], 0.022)
    flame(c, 0.5, 0.25, 0.26)


def e_fish(c):
    tail = c.poly([(0.72, 0.5), (0.95, 0.3), (0.9, 0.5), (0.95, 0.7)])
    c.paint(tail, SEA, shade=SEA_SHADE)
    fin = c.poly([(0.42, 0.3), (0.56, 0.18), (0.62, 0.34)])
    c.paint(fin, SEA, shade=SEA_SHADE)
    body = c.ellipse(0.45, 0.5, 0.36, 0.23)
    m = c.paint(body, hexc("4FD1C5"), shade=SEA_SHADE, shade_offset=(0.0, 0.07))
    c.paint(c.ellipse(0.45, 0.6, 0.3, 0.1) * m, hexc("DFF7F2", 0.85), outline=False)
    for x in (0.46, 0.56):
        c.stroke(arc_pts(x, 0.5, 0.06, 0.16, -60, 60, 12), 0.022, SEA_SHADE)
    c.paint(c.circle(0.24, 0.45, 0.06), WHITE, outline=False)
    c.paint(c.circle(0.225, 0.455, 0.032), INK, outline=False)
    c.stroke(arc_pts(0.12, 0.55, 0.04, 0.03, 300, 390), 0.02)
    c.gloss(0.4, 0.34, 0.1, 0.03, m, 0.6, rot=-5)


def e_bell(c):
    pts = [(0.5, 0.14)]
    for i in range(21):
        t = i / 20.0
        pts.append((0.5 + (0.12 + 0.12 * t + 0.14 * t ** 3) , 0.2 + 0.56 * t))
    right = pts[1:]
    left = [(1 - x, y) for x, y in right][::-1]
    shell = c.poly(left + right + [(0.9, 0.8), (0.1, 0.8)])
    shell = np.maximum(shell, c.ellipse(0.5, 0.26, 0.14, 0.12))
    c.paint(c.circle(0.5, 0.86, 0.075), GOLD_SHADE, shade=WOOD_SHADE, shade_offset=(0.02, 0.02))
    c.stroke(arc_pts(0.5, 0.1, 0.06, 0.06, 180, 360), 0.04, GOLD, outline=True)
    m = c.paint(shell, GOLD, shade=GOLD_SHADE, shade_offset=(0.07, 0.0))
    c.paint(c.rrect(0.08, 0.74, 0.92, 0.82, 0.04), hexc("FFD85C"), shade=GOLD_SHADE, shade_offset=(0.0, 0.03))
    c.gloss(0.38, 0.4, 0.035, 0.14, m, 0.7, rot=12)
    c.stroke([(0.2, 0.66), (0.8, 0.66)], 0.022, GOLD_SHADE)
    for s in (-1, 1):
        c.stroke(arc_pts(0.5 + s * 0.02, 0.5, 0.46, 0.34, (-35 if s > 0 else 180 + 15), (-15 if s > 0 else 180 + 35), 8),
                 0.035, GOLD, outline=True)


def e_anchor(c):
    col, sh = NAVY, NAVY_SHADE
    ring = np.clip(c.circle(0.5, 0.15, 0.085) - c.circle(0.5, 0.15, 0.04), 0, 1)
    shank = c.rrect(0.455, 0.2, 0.545, 0.85, 0.04)
    stock = c.rrect(0.28, 0.28, 0.72, 0.35, 0.035)
    arms = c.stroke_mask(arc_pts(0.5, 0.52, 0.33, 0.33, 20, 160, 30), 0.08)
    fl = c.poly([(0.83, 0.55), (0.92, 0.47), (0.86, 0.66)])
    fr = c.poly([(0.17, 0.55), (0.08, 0.47), (0.14, 0.66)])
    m = np.maximum.reduce([ring, shank, stock, arms, fl, fr])
    c.paint(m, col, shade=sh, shade_offset=(0.025, 0.02))
    c.gloss(0.48, 0.5, 0.012, 0.16, shank, 0.6, rot=0)
    # rope loop through the ring
    c.stroke(arc_pts(0.5, 0.22, 0.2, 0.16, 200, 340, 20), 0.035, hexc("D9A066"), outline=True)


def glyph(c, ch, col, shade):
    m = c.text_mask(ch, 0.5, 0.52, 0.95)
    c.paint(m, col, shade=shade, shade_offset=(0.03, 0.035))
    c.gloss(0.42, 0.25, 0.05, 0.03, m, 0.5)


def e_question(c):
    glyph(c, "?", ORANGE, ORANGE_SHADE)


def e_exclamation(c):
    glyph(c, "!", RED, RED_SHADE)


def e_crown(c):
    pts = [(0.12, 0.3), (0.3, 0.52), (0.5, 0.2), (0.7, 0.52), (0.88, 0.3), (0.82, 0.74), (0.18, 0.74)]
    m = c.poly(pts)
    for x, y in ((0.12, 0.3), (0.5, 0.2), (0.88, 0.3)):
        m = np.maximum(m, c.circle(x, y - 0.03, 0.055))
    c.paint(m, GOLD, shade=GOLD_SHADE, shade_offset=(0.0, 0.07))
    c.paint(c.rrect(0.16, 0.7, 0.84, 0.84, 0.03), hexc("FFD85C"), shade=GOLD_SHADE, shade_offset=(0.0, 0.03))
    c.paint(c.circle(0.5, 0.58, 0.06), CRIMSON, shade=CRIMSON_SHADE, shade_offset=(0.01, 0.015))
    c.paint(c.circle(0.3, 0.63, 0.04), hexc("2F9BD3"), outline=False)
    c.paint(c.circle(0.7, 0.63, 0.04), hexc("1FAA59"), outline=False)
    c.gloss(0.33, 0.45, 0.02, 0.07, m, 0.7, rot=20)


def e_clock(c):
    rim = c.circle(0.5, 0.5, 0.42)
    c.paint(rim, GOLD, shade=GOLD_SHADE)
    f = c.circle(0.5, 0.5, 0.33)
    c.paint(f, CREAM, shade=CREAM_SHADE, shade_offset=(0.03, 0.03))
    for i in range(12):
        a = math.radians(i * 30)
        r0, r1 = (0.25, 0.31) if i % 3 else (0.22, 0.31)
        c.stroke([(0.5 + r0 * math.sin(a), 0.5 - r0 * math.cos(a)), (0.5 + r1 * math.sin(a), 0.5 - r1 * math.cos(a))],
                 0.022 if i % 3 else 0.034)
    c.stroke([(0.5, 0.5), (0.5, 0.26)], 0.04)
    c.stroke([(0.5, 0.5), (0.66, 0.58)], 0.045, CRIMSON)
    c.paint(c.circle(0.5, 0.5, 0.035), INK, outline=False)
    # the crack from the logo
    c.stroke([(0.2, 0.3), (0.3, 0.36), (0.27, 0.44), (0.37, 0.5)], 0.02)
    c.gloss(0.36, 0.2, 0.12, 0.05, rim, 0.6)


def e_mask(c):
    pts = [(0.06, 0.34), (0.3, 0.26), (0.5, 0.34), (0.7, 0.26), (0.94, 0.34), (0.88, 0.58), (0.66, 0.72),
           (0.54, 0.62), (0.5, 0.66), (0.46, 0.62), (0.34, 0.72), (0.12, 0.58)]
    m = c.poly(pts)
    holes = np.maximum(c.ellipse(0.3, 0.47, 0.11, 0.07), c.ellipse(0.7, 0.47, 0.11, 0.07))
    holes = np.maximum(holes, np.asarray(Image.fromarray((holes * 255).astype(np.uint8)), np.float32) / 255)
    m = np.clip(m - holes, 0, 1)
    c.paint(m, CRIMSON, shade=CRIMSON_SHADE, shade_offset=(0.0, 0.06))
    # gold trim around the eyes
    for x in (0.3, 0.7):
        c.stroke(arc_pts(x, 0.47, 0.125, 0.085, 0, 360, 40), 0.025, GOLD)
    c.stroke([(0.5, 0.34), (0.5, 0.58)], 0.02, GOLD)
    c.gloss(0.2, 0.36, 0.08, 0.025, m, 0.6, rot=-15)
    # ribbon
    c.stroke([(0.06, 0.4), (0.0, 0.52)], 0.04, hexc("7B3F9E"), outline=True)
    c.stroke([(0.94, 0.4), (1.0, 0.52)], 0.04, hexc("7B3F9E"), outline=True)


def e_lantern(c):
    glow = ndimage.gaussian_filter(c.circle(0.5, 0.56, 0.3), 40)
    c.paint(glow, hexc("FFC34D", 0.6), outline=False)
    c.stroke(arc_pts(0.5, 0.2, 0.1, 0.1, 180, 360), 0.04, hexc("3A3A44"), outline=True)
    c.paint(c.poly([(0.3, 0.3), (0.7, 0.3), (0.62, 0.2), (0.38, 0.2)]), hexc("4A4A58"), shade=hexc("2B2B33"))
    glass = c.rrect(0.32, 0.3, 0.68, 0.78, 0.05)
    c.paint(glass, hexc("FFD27A"), shade=hexc("F28C28"), shade_offset=(0.06, 0.0))
    flame(c, 0.5, 0.54, 0.2, outline=False)
    for x in (0.32, 0.5, 0.68):
        c.stroke([(x, 0.3), (x, 0.78)], 0.03, hexc("3A3A44"))
    c.paint(c.rrect(0.26, 0.76, 0.74, 0.86, 0.03), hexc("4A4A58"), shade=hexc("2B2B33"))
    c.gloss(0.4, 0.4, 0.02, 0.08, glass, 0.7, rot=0)


def e_mug(c):
    handle = np.clip(c.rrect(0.6, 0.38, 0.9, 0.78, 0.13) - c.rrect(0.67, 0.46, 0.82, 0.7, 0.07), 0, 1)
    c.paint(handle, WOOD, shade=WOOD_SHADE)
    body = c.rrect(0.16, 0.3, 0.7, 0.9, 0.06)
    m = c.paint(body, hexc("D9A066"), shade=WOOD_SHADE, shade_offset=(0.07, 0.0))
    for x in (0.3, 0.44, 0.58):
        c.stroke([(x, 0.4), (x, 0.86)], 0.018, WOOD_SHADE)
    for y in (0.42, 0.8):
        c.paint(c.rrect(0.14, y - 0.035, 0.72, y + 0.035, 0.015), hexc("8C8C9A"), shade=hexc("55556A"), shade_offset=(0.0, 0.02))
    foam = c.rrect(0.14, 0.24, 0.72, 0.36, 0.05)
    for x, y, r in ((0.2, 0.22, 0.08), (0.34, 0.16, 0.1), (0.5, 0.18, 0.09), (0.64, 0.23, 0.08), (0.24, 0.36, 0.05)):
        foam = np.maximum(foam, c.circle(x, y, r))
    foam = np.maximum(foam, c.rrect(0.2, 0.3, 0.28, 0.5, 0.04))
    c.paint(foam, hexc("FFFDF5"), shade=CREAM_SHADE, shade_offset=(0.0, 0.04))
    c.gloss(0.26, 0.55, 0.02, 0.1, m, 0.5, rot=0)


def e_moon(c):
    moon = np.clip(c.circle(0.46, 0.5, 0.38) - c.circle(0.64, 0.4, 0.32), 0, 1)
    m = c.paint(moon, MOON, shade=MOON_SHADE, shade_offset=(0.04, 0.05))
    c.stroke(arc_pts(0.24, 0.5, 0.05, 0.035, 20, 160), 0.03)
    c.paint(c.ellipse(0.22, 0.6, 0.04, 0.025), BLUSH, outline=False)
    for x, y, r in ((0.78, 0.74, 0.07), (0.86, 0.2, 0.05), (0.7, 0.9, 0.035)):
        star = c.poly([(x + r * (1 if i % 2 == 0 else 0.4) * math.cos(math.radians(i * 45 - 90)),
                        y + r * (1 if i % 2 == 0 else 0.4) * math.sin(math.radians(i * 45 - 90))) for i in range(8)])
        c.paint(star, hexc("FFF3B0"))


DRAW = {name: globals()["e_" + name] for name in ORDER}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Art", "UI", "Emoji")
    os.makedirs(out, exist_ok=True)
    big, small = 160, 48
    atlas_big = Image.new("RGBA", (8 * big, 4 * big), (0, 0, 0, 0))
    atlas_small = Image.new("RGBA", (8 * small, 4 * small), (0, 0, 0, 0))
    stickers = []
    for i, name in enumerate(ORDER):
        c = Canvas()
        DRAW[name](c)
        # 4% margin inside each cell so neighbours never bleed under bilinear filtering
        b = c.render(int(big * 0.92), 3.2)
        s = c.render(int(small * 0.92), 1.35)
        cell_b = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        cell_b.paste(b, ((big - b.width) // 2, (big - b.height) // 2))
        cell_s = Image.new("RGBA", (small, small), (0, 0, 0, 0))
        cell_s.paste(s, ((small - s.width) // 2, (small - s.height) // 2))
        cell_b.save(os.path.join(out, f"{name}.png"))
        atlas_big.paste(cell_b, ((i % 8) * big, (i // 8) * big))
        atlas_small.paste(cell_s, ((i % 8) * small, (i // 8) * small))
        stickers.append((name, cell_b, cell_s))
        print(f"{i:2d} {name}")
    atlas_big.save(os.path.join(out, "T_KG_EmojiAtlas.png"))
    atlas_small.save(os.path.join(out, "T_KG_EmojiAtlas_Small.png"))

    # contact sheet: dark panel (chat) and light (bubble), plus the small size at 1:1 and 2x
    cw, ch = 200, 250
    sheet = Image.new("RGBA", (8 * cw, 4 * ch * 2), (0, 0, 0, 255))
    d = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(FONT, 18)
    for half, bg in enumerate(((29, 21, 36, 255), (246, 235, 217, 255))):
        d.rectangle([0, half * 4 * ch, 8 * cw, (half + 1) * 4 * ch], fill=bg)
        for i, (name, cb, cs) in enumerate(stickers):
            x, y = (i % 8) * cw, half * 4 * ch + (i // 8) * ch
            sheet.alpha_composite(cb, (x + 20, y + 10))
            sheet.alpha_composite(cs.resize((22, 22), Image.BILINEAR), (x + 30, y + 190))
            sheet.alpha_composite(cs, (x + 70, y + 178))
            d.text((x + 130, y + 200), name, font=font, fill=(255, 255, 255, 255) if half == 0 else (40, 28, 44, 255),
                   anchor="lm")
    sheet.save(os.path.join(out, "_preview.png"))
    print("wrote", out)


if __name__ == "__main__":
    main()
