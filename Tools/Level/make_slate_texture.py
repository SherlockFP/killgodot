"""Slate roof texture for the Crown Hill district: the kit's orange round-tile base colour remapped to blue-grey slate
by luminance (a material multiply cannot desaturate the orange). -> Art/Textures/T_KG_RoundTiles_Slate_BaseColor.png
(imported by Tools/Unreal/kg_dress_v2_assets.py into /Game/KillGodot/Env/Dress/KG_DressV2/)."""
import numpy as np
from PIL import Image

SRC = ("D:/Kill Godot/Art/Source/Quaternius_MedievalVillageMegaKit/Medieval Village MegaKit[Standard]/"
       "Medieval Village MegaKit[Standard]/glTF/T_RoundTiles_BaseColor.png")
OUT = "D:/Kill Godot/Art/Textures/T_KG_RoundTiles_Slate_BaseColor.png"

im = np.asarray(Image.open(SRC).convert("RGB")).astype(np.float32) / 255.0
lum = im @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
lo, hi = np.percentile(lum, 2), np.percentile(lum, 98)
t = np.clip((lum - lo) / max(1e-6, hi - lo), 0, 1) ** 0.9
dark = np.array([0.16, 0.19, 0.25], dtype=np.float32)
light = np.array([0.56, 0.62, 0.72], dtype=np.float32)
out = dark[None, None, :] * (1 - t[..., None]) + light[None, None, :] * t[..., None]
Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8)).save(OUT)
print("slate ->", OUT, im.shape)
