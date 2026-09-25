"""SPRINT-022: tileable water ripple normal map (Art/Textures/T_KG_WaterRipple_N.png, 512 x 512, tangent space).
A sum of travelling-wave-like sines with integer frequencies (so it tiles), gently domain-warped, differentiated into
normals. The water materials sample it at two scales and two pan directions (Tools/Unreal/kg_make_water_v2.py).
    python Tools/Level/make_water_normals.py
"""
import os

import numpy as np
from PIL import Image

N = 512
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "Textures", "T_KG_WaterRipple_N.png")
rng = np.random.default_rng(22)
u = np.arange(N) / N
X, Y = np.meshgrid(u, u)
h = np.zeros((N, N))
warp_x = 0.05 * np.sin(2 * np.pi * (2 * Y + 1 * X)) + 0.03 * np.sin(2 * np.pi * (3 * X - 2 * Y))
warp_y = 0.05 * np.sin(2 * np.pi * (2 * X - 1 * Y)) + 0.03 * np.sin(2 * np.pi * (1 * X + 3 * Y))
for k in range(28):
    fx, fy = rng.integers(-9, 10), rng.integers(-9, 10)
    f = np.hypot(fx, fy)
    if f < 2:
        continue
    amp = 1.0 / f ** 1.35
    ph = rng.uniform(0, 2 * np.pi)
    h += amp * np.sin(2 * np.pi * (fx * (X + warp_x) + fy * (Y + warp_y)) + ph)
h = (h - h.mean()) / h.std()
gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
s = 6.0
n = np.dstack([-gx * s, -gy * s, np.ones_like(h)])
n /= np.linalg.norm(n, axis=2, keepdims=True)
img = ((n * 0.5 + 0.5) * 255).clip(0, 255).astype(np.uint8)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
Image.fromarray(img, "RGB").save(OUT)
print("wrote", os.path.normpath(OUT))
