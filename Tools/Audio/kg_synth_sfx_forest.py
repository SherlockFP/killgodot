"""SPRINT-033 forest sounds (Source/KillGodot/Forest). All synthesised, CC0 by construction.

  python Tools/Audio/kg_synth_sfx_forest.py [Art/Audio]
  then (editor closed): UnrealEditor-Cmd ... -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_audio.py --only S_Forest_"

Writes 16-bit 48 kHz mono WAVs:
  S_Forest_Howl     a long wolf howl: a voiced glide up, a wavering hold, a falling tail, forest reverb (heard 160 m)
  S_Forest_Growl    a low throaty growl from the dark (the eyes stage, 20 m)
  S_Forest_Snarl    the lunge: snarl + snap of the jaws
  S_Forest_Whisper  the Mist: breathy, pitchless whispers and a cold hiss
"""
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter

SR = 48000
rng = np.random.default_rng(3303)
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


def reverb(x, secs=1.6, mix=0.35):
    n = int(SR * secs)
    ir = rng.standard_normal(n) * np.exp(-np.linspace(0, 7, n))
    ir = lp(ir, 3500)
    wet = np.convolve(x, ir)[: len(x) + n]
    wet = np.pad(wet, (0, len(x) + n - len(wet)))
    out = np.concatenate([x, np.zeros(n)])
    return out * (1 - mix) + wet / (np.max(np.abs(wet)) + 1e-9) * np.max(np.abs(x)) * mix


def write(name, x, peak=0.8):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    fade = int(SR * 0.01)
    x[:fade] *= np.linspace(0, 1, fade)
    x[-fade:] *= np.linspace(1, 0, fade)
    wavfile.write(os.path.join(out_dir, name + ".wav"), SR, (x * 32767).astype(np.int16))
    print("wrote", name, f"{len(x) / SR:.2f}s")


def voiced(f0, dur, harm=(1.0, 0.45, 0.22, 0.1, 0.05)):
    t = t_axis(dur)
    ph = 2 * np.pi * np.cumsum(f0) / SR
    return sum(a * np.sin((k + 1) * ph) for k, a in enumerate(harm)) * np.ones_like(t)


# ---- howl: 3.6 s, 380 -> 720 Hz glide, vibrato hold, fall to 430 -------------------------------------------------
dur = 3.6
t = t_axis(dur)
f = np.interp(t, [0, 0.5, 1.2, 2.6, 3.6], [380, 690, 720, 700, 430])
f = f * (1 + 0.012 * np.sin(2 * np.pi * 5.2 * t) * np.clip((t - 0.8) / 0.6, 0, 1))
env = np.interp(t, [0, 0.35, 2.8, 3.6], [0, 1, 0.85, 0])
howl = voiced(f, dur) * env
breath = bp(rng.standard_normal(len(t)), 900, 3200) * env * 0.08
write("S_Forest_Howl", reverb(howl + breath, 2.4, 0.45), 0.85)

# ---- growl: 1.8 s, 70-95 Hz rough pulse train through a throat formant ----------------------------------------------
dur = 1.8
t = t_axis(dur)
f = 82 + 12 * np.sin(2 * np.pi * 1.3 * t) + 6 * rng.standard_normal(len(t)).cumsum() / SR * 40
ph = 2 * np.pi * np.cumsum(f) / SR
pulse = (np.sin(ph) > 0.6).astype(float) - 0.2
rough = pulse * (1 + 0.5 * lp(rng.standard_normal(len(t)), 60))
growl = bp(rough, 90, 900) + 0.3 * bp(rng.standard_normal(len(t)), 200, 700)
env = np.interp(t, [0, 0.25, 1.4, 1.8], [0, 1, 0.9, 0])
write("S_Forest_Growl", reverb(growl * env, 0.8, 0.25), 0.7)

# ---- snarl + snap: 0.9 s ----------------------------------------------------------------------------------------------
dur = 0.9
t = t_axis(dur)
f = np.interp(t, [0, 0.3, 0.9], [160, 240, 120])
ph = 2 * np.pi * np.cumsum(f) / SR
snarl = np.sign(np.sin(ph)) * 0.6 + bp(rng.standard_normal(len(t)), 400, 2600) * 0.9
env = np.interp(t, [0, 0.05, 0.55, 0.9], [0, 1, 0.8, 0])
snap = np.zeros(len(t))
i0 = int(SR * 0.62)
click = hp(rng.standard_normal(int(SR * 0.03)), 1500) * np.exp(-np.linspace(0, 9, int(SR * 0.03)))
snap[i0:i0 + len(click)] += click * 3.0
write("S_Forest_Snarl", bp(snarl * env, 120, 5000) + snap, 0.9)

# ---- whisper: 4 s of breathy formant noise bursts + a cold hiss ------------------------------------------------------
dur = 4.0
t = t_axis(dur)
x = np.zeros(len(t))
for k in range(9):
    at = rng.uniform(0.1, 3.4)
    ln = rng.uniform(0.25, 0.6)
    n = int(SR * ln)
    i = int(SR * at)
    burst = bp(rng.standard_normal(n), rng.uniform(900, 1400), rng.uniform(2400, 3800))
    burst *= np.sin(np.linspace(0, np.pi, n)) ** 2
    x[i:i + n] += burst[: len(x) - i] * rng.uniform(0.4, 1.0)
hiss = hp(rng.standard_normal(len(t)), 5000) * 0.12 * np.interp(t, [0, 1, 3, 4], [0, 1, 1, 0])
write("S_Forest_Whisper", reverb(x + hiss, 1.8, 0.5), 0.6)
