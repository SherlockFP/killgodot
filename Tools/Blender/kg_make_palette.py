"""Generate T_KG_Palette.png (256x256, 8x8 swatches, vertical light->dark gradient per swatch) + palette.json.

Usage:
  blender --background --factory-startup --python kg_make_palette.py -- <out_dir>
"""
import json
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from kg_common import PALETTE, hex_to_srgb  # noqa: E402

SIZE = 256
CELL = SIZE // 8


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out_dir, exist_ok=True)

    img = bpy.data.images.new("T_KG_Palette", SIZE, SIZE, alpha=False)
    img.colorspace_settings.name = "sRGB"
    pixels = [0.0] * (SIZE * SIZE * 4)
    meta = {"size": SIZE, "cell": CELL, "swatches": {}}
    for row, entries in enumerate(PALETTE):
        for col, (name, hexv) in enumerate(entries):
            r, g, b = hex_to_srgb(hexv)
            for y in range(CELL):
                # t=0 at swatch top (light), t=1 at swatch bottom (dark)
                t = y / (CELL - 1)
                k = 1.18 - 0.43 * t
                cr, cg, cb = (min(1.0, c * k + (0.06 * (1 - t) if k > 1 else 0)) for c in (r, g, b))
                # Blender images are bottom-up: image row index 0 = bottom
                py = SIZE - 1 - (row * CELL + y)
                for x in range(CELL):
                    px = col * CELL + x
                    i = (py * SIZE + px) * 4
                    pixels[i:i + 4] = (cr, cg, cb, 1.0)
            meta["swatches"][name] = {
                "hex": hexv, "row": row, "col": col,
                # UV (0,0) = bottom-left in Blender/UE-after-import convention; v measured from bottom
                "uv_center": [(col + 0.5) / 8, 1 - (row + 0.5) / 8],
            }
    img.pixels.foreach_set(pixels)
    path = os.path.join(out_dir, "T_KG_Palette.png")
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    with open(os.path.join(out_dir, "palette.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    print(f"KG_PALETTE_OK {path}")


if __name__ == "__main__":
    main()
