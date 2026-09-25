"""Procedural stings for the role reveal moment (SPRINT-037, Source/KillGodot/UI/Reveal). CC0 by construction.

  python Tools/Audio/kg_synth_sfx_reveal.py [Art/Audio]
  then import headless (editor open is fine, new assets only):
  UnrealEditor-Cmd "D:/Kill Godot/KillGodot.uproject" -run=pythonscript
      -script="D:/Kill Godot/Tools/Unreal/kg_import_audio.py --only S_Reveal_" -unattended -nullrhi -nosplash

Writes 16-bit 48 kHz mono WAVs:
  S_Reveal_Riser      ~1.3 s  a pocket watch ticking faster and faster over a rising swell (the card spins in)
  S_Reveal_Town       ~3 s    warm thump + a bright major bell chord and shimmer (YOU WAIT WITH THE TOWN)
  S_Reveal_Impatient  ~3 s    sub boom + a dissonant brass/string stab + a metal hit (YOU ARE IMPATIENT)
  S_Reveal_Neutral    ~3 s    soft thump + a whole-tone celesta run, detuned shimmer (YOUR OWN GAME)
The sting starts on the flip (KGReveal::FlipAt); the riser ends on it.
Helpers follow Tools/Audio/kg_synth_sfx.py (copied, that script synthesises everything on import).
"""
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, fftconvolve, lfilter

SR = 48000
rng = np.random.default_rng(3701)
out_dir = sys.argv[1] if len(sys.argv) > 1 else "Art/Audio"
os.makedirs(out_dir, exist_ok=True)


def t_axis(sec):
    return np.arange(int(SR * sec)) / SR


def lp(x, hz, order=2):
    b, a = butter(order, hz / (SR / 2), "low")
    return lfilter(b, a, x)


def hp(x, hz, order=2):
    b, a = butter(order, hz / (SR / 2), "high")
    return lfilter(b, a, x)


def bp(x, lo, hi, order=2):
    b, a = butter(order, [lo / (SR / 2), hi / (SR / 2)], "band")
    return lfilter(b, a, x)


def pad(x, n):
    return np.pad(x, (0, max(0, n - len(x))))[:n]


def mix(*xs):
    n = max(len(x) for x in xs)
    return sum(pad(x, n) for x in xs)


def at(x, offset_s, total_s):
    n = int(total_s * SR)
    o = int(offset_s * SR)
    out = np.zeros(n)
    seg = x[: max(0, n - o)]
    out[o:o + len(seg)] = seg
    return out


def reverb(x, sec=1.4, wet=0.35, tone=5000.0):
    n = int(sec * SR)
    ir = rng.standard_normal(n) * np.exp(-np.linspace(0, 7, n))
    ir = lp(ir, tone)
    ir[: int(0.012 * SR)] = 0.0
    tail = fftconvolve(x, ir)[: len(x) + n]
    tail /= max(1e-9, np.max(np.abs(tail)))
    return mix(x * (1 - wet * 0.3), tail * wet * np.max(np.abs(x)))


def save(name, x, gain=0.85, max_s=3.6, fade_s=0.4):
    x = x[: int(max_s * SR)]
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * gain
    fade = int(fade_s * SR)
    x[-fade:] *= np.linspace(1, 0, fade)
    wavfile.write(os.path.join(out_dir, name + ".wav"), SR, (x * 32767).astype(np.int16))
    print(f"KG_SFX {name} {len(x) / SR:.2f}s")


def tone(f, dur, decay, partials=((1.0, 1.0),), attack=0.004, detune=0.0):
    t = t_axis(dur)
    x = np.zeros_like(t)
    for ratio, amp in partials:
        for d in ((0.0,) if detune == 0 else (-detune, detune)):
            x += amp * np.sin(2 * np.pi * f * ratio * (1 + d) * t + rng.random() * 6.28)
    e = np.exp(-t / decay)
    a = int(attack * SR)
    e[:a] *= np.linspace(0, 1, a)
    return x * e


def thump(f0=90.0, f1=38.0, dur=0.9, decay=0.25):
    t = t_axis(dur)
    f = f1 + (f0 - f1) * np.exp(-t / 0.08)
    ph = 2 * np.pi * np.cumsum(f) / SR
    x = np.sin(ph) * np.exp(-t / decay)
    click = hp(rng.standard_normal(len(t)), 1500) * np.exp(-t / 0.006) * 0.4
    return x + click


def noise_swell(dur, lo, hi, curve=2.5):
    t = t_axis(dur)
    x = bp(rng.standard_normal(len(t)), lo, hi)
    return x * (t / dur) ** curve


def tick(f=3200.0, dur=0.05):
    t = t_axis(dur)
    x = bp(rng.standard_normal(len(t)), f * 0.7, f * 1.3) * np.exp(-t / 0.006)
    return x + 0.5 * np.sin(2 * np.pi * f * 0.5 * t) * np.exp(-t / 0.01)


def riser():
    total = 1.3
    x = np.zeros(int(total * SR))
    # accelerating ticks: gaps shrink from 0.22 s to 0.035 s
    t = 0.0
    gap = 0.22
    k = 0
    while t < total - 0.05:
        x += at(tick(2600.0 if k % 2 else 3300.0) * (0.35 + 0.65 * t / total), t, total)
        t += gap
        gap = max(0.035, gap * 0.8)
        k += 1
    swell = noise_swell(total, 300, 6000, 2.8) * 0.5
    tt = t_axis(total)
    sweep = np.sin(2 * np.pi * np.cumsum(180 + 700 * (tt / total) ** 2) / SR) * (tt / total) ** 3 * 0.25
    return mix(x, swell, sweep)


def sting_town():
    total = 3.2
    root = 261.63   # C4
    bell = ((1.0, 1.0), (2.0, 0.45), (3.0, 0.22), (4.2, 0.12))
    chord = mix(*(at(tone(root * r, 2.8, 1.1, bell), i * 0.035, 2.9) for i, r in enumerate((1.0, 1.25, 1.5, 2.0))))
    high = mix(*(at(tone(root * 4 * r, 1.2, 0.35, ((1.0, 1.0),)) * 0.35, 0.12 + i * 0.07, 2.0)
                 for i, r in enumerate((1.0, 1.25, 1.5, 2.0))))
    shimmer = hp(rng.standard_normal(int(2.0 * SR)), 7000) * np.exp(-t_axis(2.0) / 0.5) * 0.12
    x = mix(thump(110, 55, 0.8, 0.18) * 0.9, chord * 0.55, high, shimmer)
    return reverb(pad(x, int(total * SR)), 1.6, 0.4)


def sting_impatient():
    total = 3.2
    boom = thump(70, 28, 1.6, 0.55) * 1.2
    t = t_axis(2.4)
    # dissonant stab: A2 + Bb2 + E3 + F3 sawtooth-ish, low-passed, fast attack, swelling decay
    stab = np.zeros_like(t)
    for f in (110.0, 116.54, 164.81, 174.61, 220.0):
        ph = 2 * np.pi * f * t + rng.random() * 6.28
        saw = sum(np.sin(k * ph) / k for k in range(1, 9))
        stab += saw
    stab = lp(stab, 1800) * np.exp(-t / 0.7) * 0.35
    metal = mix(*(tone(f, 1.6, 0.5, ((1.0, 1.0),)) * a for f, a in ((523.0, 0.3), (1247.0, 0.25), (1893.0, 0.2), (2941.0, 0.12))))
    hit = hp(rng.standard_normal(int(0.3 * SR)), 2500) * np.exp(-t_axis(0.3) / 0.03) * 0.5
    x = mix(boom, stab, metal * 0.6, hit)
    return reverb(pad(x, int(total * SR)), 1.8, 0.45, 3500)


def sting_neutral():
    total = 3.2
    root = 293.66   # D4, whole-tone run up
    notes = [root * 2 ** (k * 2 / 12) for k in range(7)]
    celesta = ((1.0, 1.0), (4.0, 0.3), (8.0, 0.08))
    run = mix(*(at(tone(f * 2, 1.4, 0.45, celesta), 0.06 + i * 0.075, 2.6) for i, f in enumerate(notes)))
    pad_ = mix(*(tone(f, 2.6, 1.3, ((1.0, 1.0), (2.0, 0.3)), attack=0.08, detune=0.004) for f in (root, root * 1.26, root * 1.587)))
    x = mix(thump(95, 48, 0.8, 0.16) * 0.7, run * 0.4, pad_ * 0.3)
    return reverb(pad(x, int(total * SR)), 1.8, 0.5, 6000)


if __name__ == "__main__":
    save("S_Reveal_Riser", riser(), 0.7, fade_s=0.015)
    save("S_Reveal_Town", sting_town())
    save("S_Reveal_Impatient", sting_impatient())
    save("S_Reveal_Neutral", sting_neutral())
