"""KillGo wordmark, variants and app icon (PIL, no engine needed).

The wordmark mirrors the in-game logo exactly (Source/KillGodot/UI/Menu/SKGMenuArt.cpp, SKGLogo + SKGClockGlyph):
"KILLG" in Roboto Black (the engine font), cream #F6E7C8 with a rust drop shadow #7A2A12, followed by the clock-face
"O": cream ring, dusk-plum face, 12 ticks, cream hour hand, lantern-orange minute hand, an ink crack through the
top-right of the ring and a brass-gold hub. Proportions are the Slate ones in em units (Em = font px size).

    python Tools/Brand/kg_brand_logo.py            # -> Art/Brand/*.png

Outputs (Art/Brand/):
  KillGo_Logo.png           2048 px wide, transparent, the in-game look (for dusk/dark UI)
  KillGo_Logo_Dark.png      2048 px wide, transparent, for busy or dark backgrounds (ink outline + lantern glow)
  KillGo_Logo_Light.png     2048 px wide, transparent, for light backgrounds (ink letters, lantern shadow)
  KillGo_Logo_Tagline.png   2048 px wide, transparent, wordmark + tagline lockup
  KillGo_AppIcon_1024.png   1024x1024 square app icon (the cracked clock on a dusk sky)
  KillGo_Logo_Sheet.png     preview sheet of all of the above on light and dark grounds
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "Art", "Brand")
FONTS = r"D:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts"
BLACK = os.path.join(FONTS, "Roboto-Black.ttf")
BOLD = os.path.join(FONTS, "Roboto-Bold.ttf")

# FKGMenuStyle palette
INK = (0x12, 0x0D, 0x17)
NIGHT = (0x1D, 0x15, 0x24)
PANEL = (0x2A, 0x1F, 0x31)
CREAM = (0xF6, 0xE7, 0xC8)
GOLD = (0xF2, 0xC2, 0x30)
LANTERN = (0xF2, 0x8C, 0x28)
CRIMSON = (0xC8, 0x10, 0x2E)
RUST = (0x7A, 0x2A, 0x12)
FACE_IN = (0x2E, 0x1E, 0x38)
FACE_OUT = (0x1A, 0x11, 0x20)

TAGLINE = "Everyone's waiting. Someone's impatient."
SS = 3   # supersampling

# In-game static frame: SKGClockGlyph at T=0 -> minute 150 deg, hour 300 + 150/12 = 312.5 deg (10:25).
MINUTE_DEG = 150.0
HOUR_DEG = 312.5

STYLES = {
    "game": {"text": CREAM, "shadow": RUST + (242,), "ring": CREAM, "face": (FACE_IN, FACE_OUT), "tick": CREAM,
             "hour": CREAM, "minute": LANTERN, "crack": INK, "hub": GOLD, "outline": None, "glow": None},
    "dark": {"text": CREAM, "shadow": RUST + (242,), "ring": CREAM, "face": (FACE_IN, FACE_OUT), "tick": CREAM,
             "hour": CREAM, "minute": LANTERN, "crack": INK, "hub": GOLD, "outline": INK, "glow": LANTERN},
    "light": {"text": PANEL, "shadow": LANTERN + (255,), "ring": PANEL, "face": ((0xFF, 0xF4, 0xDC), (0xF0, 0xDC, 0xB4)),
              "tick": PANEL, "hour": PANEL, "minute": CRIMSON, "crack": CREAM, "hub": LANTERN, "outline": None,
              "glow": None},
}


def clock_dir(deg):
    r = math.radians(deg)
    return math.sin(r), -math.cos(r)


def line(d, a, b, color, width):
    d.line([a, b], fill=color, width=max(1, int(round(width))))


def tapered(d, pts, widths, color):
    """A polyline whose width tapers point by point (a crack), drawn as one polygon."""
    left, right = [], []
    n = len(pts)
    for i, (p, w) in enumerate(zip(pts, widths)):
        a = pts[max(0, i - 1)]
        b = pts[min(n - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / L, dx / L
        left.append((p[0] + nx * w / 2, p[1] + ny * w / 2))
        right.append((p[0] - nx * w / 2, p[1] - ny * w / 2))
    d.polygon(left + right[::-1], fill=color)


def draw_clock(img, cx, cy, em, st, shadow=None, round_caps=False):
    """The SKGClockGlyph face at centre (cx, cy) for font size em (px)."""
    radius = em * 0.37
    ring = em * 0.155
    face = radius - ring
    d = ImageDraw.Draw(img)

    def line(d, a, b, color, width):
        d.line([a, b], fill=color, width=max(1, int(round(width))))
        if round_caps:
            r = width / 2
            for p in (a, b):
                d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=color)
    if shadow:
        sx, sy, scol = shadow
        d.ellipse([cx + sx - radius, cy + sy - radius, cx + sx + radius, cy + sy + radius], fill=scol)
    d.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=st["ring"] + (255,))
    # radial face gradient (centre -> edge)
    c0, c1 = st["face"]
    steps = 48
    for i in range(steps):
        t = i / (steps - 1)
        r = face * (1.0 - t) + 0.5
        col = tuple(int(c1[k] + (c0[k] - c1[k]) * t) for k in range(3))
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col + (255,))
    for mark in range(12):
        major = mark % 3 == 0
        dx, dy = clock_dir(mark * 30.0)
        a = (cx + dx * (face - em * (0.075 if major else 0.045)), cy + dy * (face - em * (0.075 if major else 0.045)))
        b = (cx + dx * (face - em * 0.018), cy + dy * (face - em * 0.018))
        line(d, a, b, st["tick"] + (217,), em * (0.026 if major else 0.016))
    hx, hy = clock_dir(HOUR_DEG)
    line(d, (cx - hx * em * 0.03, cy - hy * em * 0.03), (cx + hx * face * 0.52, cy + hy * face * 0.52),
         st["hour"] + (255,), em * 0.05)
    mx, my = clock_dir(MINUTE_DEG)
    line(d, (cx - mx * em * 0.04, cy - my * em * 0.04), (cx + mx * face * 0.84, cy + my * face * 0.84),
         st["minute"] + (255,), em * 0.034)
    # Crack: the in-game path (38/1.02 -> 46/0.80 -> 33/0.63 -> 44/0.44, branch 46/0.80 -> 62/0.93), tapered from
    # the rim inward and clipped to the ring so it reads as a crack, not a stick.
    main = [(38.0, 1.04), (46.0, 0.80), (33.0, 0.63), (44.0, 0.44)]
    pts = [(cx + clock_dir(a)[0] * radius * f, cy + clock_dir(a)[1] * radius * f) for a, f in main]
    b1 = (cx + clock_dir(62.0)[0] * radius * 0.95, cy + clock_dir(62.0)[1] * radius * 0.95)
    crack = Image.new("RGBA", img.size, (0, 0, 0, 0))
    cd = ImageDraw.Draw(crack)
    tapered(cd, pts, [em * 0.040, em * 0.030, em * 0.018, em * 0.002], st["crack"] + (255,))
    tapered(cd, [pts[1], b1], [em * 0.022, em * 0.002], st["crack"] + (255,))
    disc = Image.new("L", img.size, 0)
    ImageDraw.Draw(disc).ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=255)
    crack.putalpha(ImageChops.multiply(crack.getchannel("A"), disc))
    img.alpha_composite(crack)
    d = ImageDraw.Draw(img)
    hub = em * 0.045
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=st["hub"] + (255,))


def wordmark(width, style="game", tagline=False, pad=0.06):
    """Render the wordmark so the final image is `width` px wide (transparent RGBA)."""
    st = STYLES[style]
    probe = ImageFont.truetype(BLACK, 1000)
    text_w = probe.getlength("KILLG") / 1000.0          # in em
    spacing = 0.010 * 4                                  # LetterSpacing 10 (1/1000 em) x 4 gaps -> em
    glyph_w = 0.80
    shadow_x, shadow_y = 0.0225, 0.04125                 # Size*(0.03, 0.055) in Slate px, Size = em*0.75
    total_em = text_w + spacing + glyph_w + shadow_x
    W = width * SS
    em = W * (1.0 - 2 * pad) / total_em
    font = ImageFont.truetype(BLACK, int(round(em)))
    asc, desc = font.getmetrics()
    line_h = em * 1.17
    tag_h = 0.0
    if tagline:
        tag_em_ratio = 0.15
        tag_font_px = em * tag_em_ratio
        tag_font = ImageFont.truetype(BOLD, int(round(tag_font_px)))
        tag = TAGLINE.upper()
        track = tag_font_px * 0.17
        tag_w = sum(tag_font.getlength(ch) for ch in tag) + track * (len(tag) - 1)
        word_w = em * (text_w + spacing + glyph_w)
        if tag_w > word_w:   # scale the tagline to the wordmark width for the lockup
            k = word_w / tag_w
            tag_font_px *= k
            track *= k
            tag_font = ImageFont.truetype(BOLD, int(round(tag_font_px)))
            tag_w = sum(tag_font.getlength(ch) for ch in tag) + track * (len(tag) - 1)
        tag_h = tag_font_px * 1.6
    H = int(line_h + tag_h + em * pad * 2 * 1.2 + em * 0.2)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x0 = W * pad
    base = em * pad * 1.2 + em * 0.98
    sh = (em * shadow_x, em * shadow_y, st["shadow"])

    def paint(target, ox=0.0, oy=0.0, color=None, with_clock=True):
        d = ImageDraw.Draw(target)
        x = x0 + ox
        for ch in "KILLG":
            d.text((x, base + oy), ch, font=font, fill=color or (st["text"] + (255,)), anchor="ls")
            x += font.getlength(ch) + em * 0.010
        return x

    # shadow layer
    shadow_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gx = paint(shadow_layer, sh[0], sh[1], color=sh[2])
    glyph_cx = gx + em * glyph_w * 0.5
    glyph_cy = base - em * 0.36
    sd = ImageDraw.Draw(shadow_layer)
    r = em * 0.37
    sd.ellipse([glyph_cx + sh[0] - r, glyph_cy + sh[1] - r, glyph_cx + sh[0] + r, glyph_cy + sh[1] + r], fill=sh[2])

    main = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    paint(main)
    draw_clock(main, glyph_cx, glyph_cy, em, st)

    if tagline:
        td = ImageDraw.Draw(main)
        tx = x0 + em * 0.03
        ty = base + em * 0.30
        for ch in tag:
            td.text((tx, ty), ch, font=tag_font, fill=LANTERN + (255,), anchor="ls")
            tx += tag_font.getlength(ch) + track
        ImageDraw.Draw(shadow_layer)

    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    if st["glow"]:
        alpha = Image.alpha_composite(shadow_layer, main).getchannel("A")
        glow = Image.new("RGBA", (W, H), st["glow"] + (0,))
        glow.putalpha(alpha.filter(ImageFilter.GaussianBlur(em * 0.16)).point(lambda v: int(v * 0.55)))
        out = Image.alpha_composite(out, glow)
    if st["outline"]:
        alpha = Image.alpha_composite(shadow_layer, main).getchannel("A")
        k = int(em * 0.035) * 2 + 1
        ol = Image.new("RGBA", (W, H), st["outline"] + (0,))
        ol.putalpha(alpha.filter(ImageFilter.MaxFilter(min(k, 61))).filter(ImageFilter.GaussianBlur(em * 0.006)))
        out = Image.alpha_composite(out, ol)
    out = Image.alpha_composite(out, shadow_layer)
    out = Image.alpha_composite(out, main)
    bbox = out.getchannel("A").point(lambda v: 255 if v > 2 else 0).getbbox()
    margin = int(em * pad)
    out = out.crop((0, max(0, bbox[1] - margin), W, min(H, bbox[3] + margin)))
    return out.resize((width, max(1, out.height // SS)), Image.LANCZOS)


def app_icon(size=1024):
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 255))
    d = ImageDraw.Draw(img)
    horizon = S * 0.70
    bands = [(0.0, (0x0B, 0x08, 0x16)), (0.45, (0x1E, 0x13, 0x30)), (0.78, (0x4A, 0x22, 0x40)), (1.0, (0xB9, 0x55, 0x3A))]
    for y in range(S):
        t = min(1.0, y / horizon)
        for i in range(len(bands) - 1):
            if bands[i][0] <= t <= bands[i + 1][0]:
                u = (t - bands[i][0]) / (bands[i + 1][0] - bands[i][0])
                c = tuple(int(bands[i][1][k] + (bands[i + 1][1][k] - bands[i][1][k]) * u) for k in range(3))
                break
        if y > horizon:   # sea
            u = (y - horizon) / (S - horizon)
            c = tuple(int(a + (b - a) * u) for a, b in zip((0x3A, 0x21, 0x40), (0x09, 0x0A, 0x13)))
        d.line([(0, y), (S, y)], fill=c + (255,))
    # sun glow on the horizon
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([S * 0.05, horizon - S * 0.30, S * 0.95, horizon + S * 0.30], fill=LANTERN + (120,))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.09))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    # soft glints on the sea
    gl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd2 = ImageDraw.Draw(gl)
    for i in range(12):
        y = horizon + 10 * SS + (i ** 1.4) * 9 * SS
        if y > S:
            break
        half = (120 + 90 * ((i * 37) % 7) / 7) * SS * (1 + i * 0.06)
        a = int(150 * (1 - i / 12))
        h = 3.5 * SS * (1 + i * 0.08)
        gd2.ellipse([S * 0.5 - half, y - h, S * 0.5 + half, y + h], fill=(0xF7, 0xA4, 0x5A, a))
    img = Image.alpha_composite(img, gl.filter(ImageFilter.GaussianBlur(2 * SS)))
    # the clock: em chosen so the ring fills ~64 % of the icon
    em = S * 0.64 / (2 * 0.37)
    cx, cy = S * 0.5, S * 0.46
    shadow = (em * 0.0225, em * 0.04125, RUST + (242,))
    halo = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    r = em * 0.37 * 1.08
    hd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(0xFF, 0xC8, 0x70, 150))
    img = Image.alpha_composite(img, halo.filter(ImageFilter.GaussianBlur(S * 0.05)))
    clock = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    draw_clock(clock, cx, cy, em, STYLES["game"], shadow=shadow, round_caps=True)
    img = Image.alpha_composite(img, clock)
    return img.resize((size, size), Image.LANCZOS).convert("RGB")


def checker(w, h, a=(236, 228, 214), b=(222, 212, 196), cell=32):
    im = Image.new("RGB", (w, h), a)
    d = ImageDraw.Draw(im)
    for y in range(0, h, cell):
        for x in range(0, w, cell):
            if (x // cell + y // cell) % 2:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=b)
    return im


def sheet(logos, icon):
    W, pad = 2400, 60
    rows = []
    for name, im, bg in logos:
        scale = (W // 2 - 2 * pad) / im.width
        small = im.resize((int(im.width * scale), int(im.height * scale)), Image.LANCZOS)
        rows.append((name, small, bg))
    H = pad + sum(max(r[1].height for r in rows[i:i + 2]) + 2 * pad for i in range(0, len(rows), 2)) + 720
    out = Image.new("RGB", (W, H), (40, 32, 46))
    y = pad
    font = ImageFont.truetype(BOLD, 28)
    for i in range(0, len(rows), 2):
        rh = max(r[1].height for r in rows[i:i + 2]) + 2 * pad
        for j, (name, small, bg) in enumerate(rows[i:i + 2]):
            x = j * W // 2
            tile = bg(W // 2, rh) if callable(bg) else Image.new("RGB", (W // 2, rh), bg)
            tile.paste(small, (pad, (rh - small.height) // 2), small)
            ImageDraw.Draw(tile).text((16, 10), name, font=font, fill=(128, 110, 120))
            out.paste(tile, (x, y))
        y += rh
    ic = icon.resize((560, 560), Image.LANCZOS)
    out.paste(ic, (pad, y + 70))
    ImageDraw.Draw(out).text((pad, y + 20), "KillGo_AppIcon_1024.png", font=font, fill=(200, 190, 180))
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    game = wordmark(2048, "game")
    dark = wordmark(2048, "dark")
    light = wordmark(2048, "light")
    tag = wordmark(2048, "game", tagline=True)
    game.save(os.path.join(OUT, "KillGo_Logo.png"))
    dark.save(os.path.join(OUT, "KillGo_Logo_Dark.png"))
    light.save(os.path.join(OUT, "KillGo_Logo_Light.png"))
    tag.save(os.path.join(OUT, "KillGo_Logo_Tagline.png"))
    icon = app_icon(1024)
    icon.save(os.path.join(OUT, "KillGo_AppIcon_1024.png"))
    dusk = (0x1D, 0x15, 0x24)
    sheet([("KillGo_Logo.png", game, dusk), ("KillGo_Logo_Dark.png", dark, (0x3A, 0x5A, 0x4A)),
           ("KillGo_Logo_Light.png", light, checker), ("KillGo_Logo_Tagline.png", tag, dusk)],
          icon).save(os.path.join(OUT, "KillGo_Logo_Sheet.png"))
    for n in ("KillGo_Logo", "KillGo_Logo_Dark", "KillGo_Logo_Light", "KillGo_Logo_Tagline", "KillGo_AppIcon_1024"):
        im = Image.open(os.path.join(OUT, n + ".png"))
        print(f"KG_BRAND {n}.png {im.size} {im.mode}")


if __name__ == "__main__":
    sys.exit(main())
