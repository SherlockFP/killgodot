"""KillGo key art + loading screens, composed from real L_Morrowmere_v2 renders (PIL + numpy, no engine).

Inputs: Saved/Screenshots/Brand/<shot>.png (2560x1440, from Tools/Brand/kg_brand_capture.ps1)
Logo:   Tools/Brand/kg_brand_logo.py (the in-game wordmark)
Text:   Docs/Lore/KillGo_Lore.md section 5 (the loading lines are copied below; keep them in sync)

    python Tools/Brand/kg_brand_keyart.py

Outputs:
  Art/Brand/KeyArt/KillGo_Hero_1920x1080.png            dusk harbour from the jetty head (br_dusk_harbour_b)
  Art/Brand/KeyArt/KillGo_Capsule_616x353.png           silhouette on the Belvedere at sunset (br_belvedere_b)
  Art/Brand/KeyArt/KillGo_Capsule_Vertical_600x900.png  the lighthouse at night (br_lighthouse_night_b)
  Art/Brand/Loading/T_KG_Load_<District>.png            1920x1080, no text (imported as UI textures)
  Art/Brand/Loading/T_KG_Load_<District>_Preview.png    same with the EN + TR lore line printed
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kg_brand_logo as logo  # noqa: E402

ROOT = logo.ROOT
SRC = os.path.join(ROOT, "Saved", "Screenshots", "Brand")
KEY = os.path.join(ROOT, "Art", "Brand", "KeyArt")
LOAD = os.path.join(ROOT, "Art", "Brand", "Loading")
FONTS = logo.FONTS
BOLD = os.path.join(FONTS, "Roboto-Bold.ttf")
MEDIUM = os.path.join(FONTS, "Roboto-Medium.ttf")
BLACK = os.path.join(FONTS, "Roboto-Black.ttf")
TAGLINE = logo.TAGLINE.upper()

LOADING = [
    # (texture name, shot, district EN, district TR, line key, EN, TR)
    ("T_KG_Load_Harbour", "ld_harbour", "HARBOUR ROW", "LİMAN SIRASI", "Lore.Load.01",
     "Every evening the boy's boat touches the jetty. Every evening: \"Not tonight. Tomorrow, surely.\"",
     "Her akşam çocuğun kayığı iskeleye değer. Her akşam: \"Bu akşam değil. Yarın kesin.\""),
    ("T_KG_Load_Heart", "br_square", "FOUNTAIN SQUARE", "ÇEŞME MEYDANI", "Lore.Load.02",
     "The fountain has run since 1693. Nobody has ever seen anyone refill it. Nobody asks.",
     "Çeşme 1693'ten beri akıyor. Dolduran kimseyi gören olmadı. Soran da olmadı."),
    ("T_KG_Load_Crown", "ld_crown", "CROWN HILL", "TAÇ TEPESİ", "Lore.Load.03",
     "Every date on every tombstone says \"Tomorrow\". The gravedigger calls it optimism.",
     "Her mezar taşındaki tarih \"Yarın\" der. Mezarcı buna iyimserlik diyor."),
    ("T_KG_Load_Sakura", "ld_sakura_b", "SAKURA GARDEN", "SAKURA BAHÇESİ", "Lore.Load.04",
     "The sakura never drop their petals and the koi never let themselves be caught. Some things here refuse to end.",
     "Sakuralar yaprak dökmez, koiler yakalanmaz. Burada bazı şeyler bitmeyi reddeder."),
]

# grade presets: exposure, contrast, saturation, shadow tint, highlight tint, split amount, vignette
GRADES = {
    "dusk": dict(exp=1.04, con=1.12, sat=1.12, sh=(-0.02, -0.01, 0.05), hi=(0.06, 0.02, -0.04), split=1.0, vig=0.42),
    "sunset": dict(exp=1.0, con=1.14, sat=1.15, sh=(-0.01, -0.02, 0.04), hi=(0.07, 0.02, -0.03), split=1.0, vig=0.45),
    "night": dict(exp=0.80, con=1.18, sat=1.05, sh=(-0.03, 0.0, 0.07), hi=(0.05, 0.03, -0.03), split=1.0, vig=0.50),
    "golden": dict(exp=1.02, con=1.10, sat=1.12, sh=(-0.015, 0.0, 0.035), hi=(0.04, 0.02, -0.02), split=1.0, vig=0.35),
}


def load(shot):
    path = os.path.join(SRC, shot + ".png")
    if not os.path.exists(path):
        raise SystemExit(f"missing render {path} (run Tools/Brand/kg_brand_capture.ps1 {shot})")
    return Image.open(path).convert("RGB")


def grade(im, g):
    a = np.asarray(im).astype(np.float32) / 255.0
    a = a * g["exp"]
    lum = (a * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(-1, keepdims=True)
    a = lum + (a - lum) * g["sat"]
    # soft S-curve around mid grey
    a = np.clip(a, 0.0, 1.0)
    s = a * a * (3 - 2 * a)
    a = a + (s - a) * (g["con"] - 1.0) * 2.0
    lum = (np.clip(a, 0, 1) * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(-1, keepdims=True)
    a = a + (1 - lum) * np.array(g["sh"], np.float32) * g["split"] + lum * np.array(g["hi"], np.float32) * g["split"]
    a = np.clip(a, 0.0, 1.0)
    return Image.fromarray((a * 255.0 + 0.5).astype(np.uint8))


def vignette(im, strength, cx=0.5, cy=0.5):
    w, h = im.size
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    dx = (x / w - cx) / 0.62
    dy = (y / h - cy) / 0.62
    r = np.clip(np.sqrt(dx * dx + dy * dy), 0, 1.6)
    m = 1.0 - strength * np.clip((r - 0.35) / 1.0, 0, 1) ** 1.6
    a = np.asarray(im).astype(np.float32) * m[..., None]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def gradient(im, top=0.0, bottom=0.0, top_h=0.3, bottom_h=0.35, color=logo.INK):
    """Darken the top/bottom bands (text and logo room)."""
    w, h = im.size
    y = np.linspace(0, 1, h, dtype=np.float32)[:, None]
    t = np.clip(1 - y / top_h, 0, 1) ** 1.5 * top if top else 0
    b = np.clip((y - (1 - bottom_h)) / bottom_h, 0, 1) ** 1.3 * bottom if bottom else 0
    k = np.maximum(t, b)[..., None] if not np.isscalar(t) or not np.isscalar(b) else None
    if k is None:
        return im
    k = np.broadcast_to(k, (h, w, 1))
    a = np.asarray(im).astype(np.float32)
    c = np.array(color, np.float32)
    a = a * (1 - k) + c * k
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def crop_resize(im, box, size):
    return im.crop(box).resize(size, Image.LANCZOS)


def tracked(draw, xy, text, font, fill, tracking=0.0, anchor="ls"):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill, anchor=anchor)
        x += font.getlength(ch) + tracking
    return x


def tracked_width(text, font, tracking):
    return sum(font.getlength(ch) for ch in text) + tracking * (len(text) - 1)


def text_layer(size, draw_fn, shadow=(0, 3), blur=4, shadow_alpha=200):
    """Draw text via draw_fn(draw) on a layer and add a soft dark drop shadow under it."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    sh = Image.new("RGBA", size, logo.INK + (0,))
    a = layer.getchannel("A").point(lambda v: v * shadow_alpha // 255)
    sh.putalpha(a)
    sh = sh.transform(size, Image.AFFINE, (1, 0, -shadow[0], 0, 1, -shadow[1])).filter(ImageFilter.GaussianBlur(blur))
    out = Image.alpha_composite(sh, layer)
    return out


def paste_logo(base, variant, width, x, y):
    lg = logo.wordmark(width, variant)
    base.alpha_composite(lg, (int(x), int(y)))
    return lg.size


def tagline(base, text, cx, y, size, color=logo.LANTERN, track_em=0.17, align="center"):
    font = ImageFont.truetype(BOLD, size)
    tr = size * track_em
    w = tracked_width(text, font, tr)
    x0 = cx - w / 2 if align == "center" else cx
    lay = text_layer(base.size, lambda d: tracked(d, (x0, y), text, font, color + (255,), tr), shadow=(0, 2),
                     blur=max(2, size // 8), shadow_alpha=230)
    base.alpha_composite(lay)


# ------------------------------------------------------------------------------------------------------------------
def hero():
    im = grade(load("br_dusk_harbour_b"), GRADES["dusk"]).resize((1920, 1080), Image.LANCZOS)
    im = gradient(im, top=0.62, top_h=0.34, bottom=0.35, bottom_h=0.22)
    im = vignette(im, GRADES["dusk"]["vig"], cy=0.55).convert("RGBA")
    lw = 800
    lg = logo.wordmark(lw, "dark")
    im.alpha_composite(lg, ((1920 - lw) // 2, 4))
    tagline(im, TAGLINE, 960, 4 + lg.size[1] - 44, 27)
    return im.convert("RGB")


def capsule():
    src = grade(load("br_belvedere_b"), GRADES["sunset"])
    # 616x353 (1.745): keep the silhouette on the left third and the axis (torii + pagoda) centred
    im = crop_resize(src, (150, 40, 150 + 2100, 40 + 1203), (616, 353))
    im = gradient(im, top=0.35, top_h=0.35)
    im = vignette(im, 0.38, cx=0.52, cy=0.5).convert("RGBA")
    lw = 318
    lg = logo.wordmark(lw, "dark")
    im.alpha_composite(lg, (616 - lw - 14, 6))
    return im.convert("RGB")


def vertical():
    src = grade(load("br_lighthouse_night_b"), GRADES["night"])
    im = crop_resize(src, (800, 0, 1760, 1440), (600, 900))
    # the rendered night sky is a flat grey haze: pull it toward deep moonlit blue (multiply, strongest at the top,
    # fading out above the cliff) so the lamp, beams and moon carry the frame
    a = np.asarray(im).astype(np.float32) / 255.0
    y = np.linspace(0, 1, 900, dtype=np.float32)[:, None, None]
    k = np.clip(1 - y / 0.66, 0, 1) ** 0.8
    tint = np.array([0.52, 0.62, 0.95], np.float32)
    lum = (a * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(-1, keepdims=True)
    keep = np.clip((lum - 0.62) / 0.3, 0, 1)            # keep the moon, the lamp and the beams bright
    a = a * (1 - k * (1 - keep)) + a * tint * k * (1 - keep)
    im = Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8))
    im = gradient(im, top=0.25, top_h=0.2, bottom=0.55, bottom_h=0.34)
    im = vignette(im, GRADES["night"]["vig"], cy=0.45).convert("RGBA")
    lw = 540
    lg = logo.wordmark(lw, "dark")
    y = 900 - lg.size[1] - 58
    im.alpha_composite(lg, ((600 - lw) // 2, y))
    tagline(im, "EVERYONE'S WAITING.", 300, y + lg.size[1] - 8, 21)
    tagline(im, "SOMEONE'S IMPATIENT.", 300, y + lg.size[1] + 22, 21)
    return im.convert("RGB")


def loading(entry, preview):
    name, shot, dist_en, dist_tr, key, en, tr = entry
    im = grade(load(shot), GRADES["golden"]).resize((1920, 1080), Image.LANCZOS)
    im = gradient(im, bottom=0.82, bottom_h=0.34, top=0.25, top_h=0.14)
    im = vignette(im, GRADES["golden"]["vig"], cy=0.42).convert("RGBA")
    lw = 300
    lg = logo.wordmark(lw, "dark")
    im.alpha_composite(lg, (1920 - lw - 48, 1080 - lg.size[1] - 40))
    if preview:
        cap = ImageFont.truetype(BOLD, 20)
        f_en = ImageFont.truetype(BOLD, 38)
        f_tr = ImageFont.truetype(MEDIUM, 27)
        x = 96

        def draw(d):
            tracked(d, (x, 868), f"{dist_en}  ·  {dist_tr}", cap, logo.LANTERN + (255,), 20 * 0.22)
            wrap = wrap_text(en, f_en, 1320)
            yy = 918
            for ln in wrap:
                d.text((x, yy), ln, font=f_en, fill=logo.CREAM + (255,), anchor="ls")
                yy += 48
            for ln in wrap_text(tr, f_tr, 1320):
                d.text((x, yy + 6), ln, font=f_tr, fill=(0xCD, 0xBB, 0x98, 255), anchor="ls")
                yy += 36
        im.alpha_composite(text_layer(im.size, draw, shadow=(0, 3), blur=5))
    return im.convert("RGB")


def wrap_text(text, font, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if font.getlength(t) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines


def main():
    os.makedirs(KEY, exist_ok=True)
    os.makedirs(LOAD, exist_ok=True)
    outs = {
        os.path.join(KEY, "KillGo_Hero_1920x1080.png"): hero(),
        os.path.join(KEY, "KillGo_Capsule_616x353.png"): capsule(),
        os.path.join(KEY, "KillGo_Capsule_Vertical_600x900.png"): vertical(),
    }
    for e in LOADING:
        outs[os.path.join(LOAD, e[0] + ".png")] = loading(e, False)
        outs[os.path.join(LOAD, e[0] + "_Preview.png")] = loading(e, True)
    for p, im in outs.items():
        im.save(p)
        print(f"KG_BRAND {os.path.relpath(p, ROOT)} {im.size}")


if __name__ == "__main__":
    main()
