"""Procedural sounds for digging + the underground (Source/KillGodot/Dig). All synthesised, CC0 by construction.

  python Tools/Audio/kg_synth_sfx_dig.py [Art/Audio]
  then (editor closed): UnrealEditor-Cmd ... -run=pythonscript -script="D:/Kill Godot/Tools/Unreal/kg_import_dig_assets.py"
  (or Tools/Unreal/kg_import_audio.py --only S_Dig_,S_Cave_,S_Gate_,S_Passage_)

Writes 16-bit 48 kHz mono WAVs (loops seamless, named *_Loop):
  S_Dig_Stab       spade bites into soil (every stroke; graves play it louder and far)
  S_Dig_Toss       a spadeful of earth thrown aside (every finished stage)
  S_Dig_Clank      spade on coffin wood + stone: the loud grave sound heard 40 m away
  S_Dig_Chest      a buried chest opens: creak, coins, a little sparkle
  S_Cave_Drip      one water drop with a stone echo (positional, around the listener underground)
  S_Cave_Amb_Loop  24 s bed: low rumble, a slow draught moaning in the tunnels, far drips
  S_Gate_Open      the iron vault gate swings (creak + clank)
  S_Gate_Locked    rattling a locked iron gate
  S_Passage_Door   a heavy stone door grinding (the mausoleum / crypt door)
  S_Passage_Ladder wooden rungs and a creak (the well)
Helpers copied from Tools/Audio/kg_synth_sfx.py (that script synthesises everything on import).
"""
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter

SR = 48000
rng = np.random.default_rng(1916)
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


def env(n, attack, release, curve=3.0):
    e = np.ones(n)
    a = max(1, int(attack * SR))
    r = max(1, int(release * SR))
    e[:a] = np.linspace(0, 1, a)
    e[-r:] *= np.linspace(1, 0, r) ** curve
    return e


def smooth_noise(sec, rate_hz):
    n = int(sec * SR)
    k = max(2, int(sec * rate_hz) + 2)
    pts = rng.random(k)
    x = np.interp(np.linspace(0, k - 1, n), np.arange(k), pts)
    return lp(x, rate_hz * 2 + 0.1, 1)


def loopify(x, fade=1.5):
    f = int(fade * SR)
    head, tail = x[:f], x[-f:]
    w = np.linspace(0, 1, f)
    x = x[:-f].copy()
    x[:f] = head * w + tail * (1 - w)
    return x


def save(name, x, gain=0.8):
    x = np.asarray(x, dtype=np.float64)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * gain
    wavfile.write(os.path.join(out_dir, f"{name}.wav"), SR, (x * 32767).astype(np.int16))
    print(f"KG_SFX {name}.wav {len(x) / SR:.1f}s")


def tone(f, dur, decay, harmonics=((1.0, 1.0),), attack=0.002):
    t = t_axis(dur)
    x = sum(a * np.sin(2 * np.pi * f * r * t) for r, a in harmonics)
    return x * np.exp(-t / decay) * env(len(t), attack, min(0.02, dur * 0.3), 1.0)


def noise_burst(dur, lo, hi, attack=0.001, release=None):
    n = rng.standard_normal(int(dur * SR))
    return bp(n, lo, hi) * env(len(n), attack, release if release is not None else dur * 0.9, 2.5)


def seq(parts):
    total = max(int(off * SR) + len(sig) for off, sig in parts)
    x = np.zeros(total)
    for off, sig in parts:
        i0 = int(off * SR)
        x[i0:i0 + len(sig)] += sig
    return x


def echo(x, delays=((0.11, 0.35), (0.23, 0.2), (0.41, 0.1)), tail=0.0):
    """Cheap stone-room echo: a few taps + an optional diffuse tail."""
    n = len(x) + int((max(d for d, _ in delays) + tail) * SR)
    out = np.zeros(n)
    out[:len(x)] += x
    for d, g in delays:
        i = int(d * SR)
        out[i:i + len(x)] += lp(x, 3000) * g
    if tail > 0:
        tt = t_axis(tail)
        ir = rng.standard_normal(len(tt)) * np.exp(-tt / (tail * 0.3))
        wet = np.convolve(lp(x, 2500), ir)[:n] * 0.02
        out[:len(wet)] += wet
    return out


# ------------------------------------------------------------------------------------------------ digging
def dig_stab():
    thud = tone(95, 0.25, 0.06, ((1, 1), (2.1, 0.3)))
    shink = noise_burst(0.07, 2200, 6000, 0.0005, 0.06) * 0.6
    grit = lp(rng.standard_normal(int(0.22 * SR)), 1800) * env(int(0.22 * SR), 0.004, 0.2, 2.0) * 0.8
    return seq([(0.0, shink), (0.004, thud), (0.01, grit)])


def dig_toss():
    whoosh = bp(rng.standard_normal(int(0.35 * SR)), 300, 1600) * env(int(0.35 * SR), 0.12, 0.2, 2.0) * 0.4
    patter = np.zeros(int(0.8 * SR))
    for _ in range(40):
        i = int(rng.uniform(0.18, 0.62) * SR)
        click = bp(rng.standard_normal(int(0.012 * SR)), 1200, 5000) * np.exp(-t_axis(0.012) / 0.003)
        patter[i:i + len(click)] += click * rng.uniform(0.2, 1.0)
    thump = tone(70, 0.3, 0.07) * 0.8
    return seq([(0.0, whoosh), (0.0, patter), (0.2, thump)])


def dig_clank():
    wood = tone(180, 0.6, 0.12, ((1, 1), (2.02, 0.5), (3.1, 0.2)))
    ring = tone(1180, 1.2, 0.35, ((1, 0.5), (1.93, 0.35), (2.71, 0.2)))
    scrape = bp(rng.standard_normal(int(0.3 * SR)), 900, 3500) * env(int(0.3 * SR), 0.01, 0.25, 2.0) * 0.4
    return echo(seq([(0.0, scrape), (0.03, wood), (0.03, ring * 0.7)]), ((0.16, 0.3), (0.34, 0.15)), tail=0.8)


def dig_chest():
    creak = np.sin(2 * np.pi * np.cumsum(220 + 60 * np.sin(2 * np.pi * 3.0 * t_axis(0.5)) + 30 * rng.standard_normal(int(0.5 * SR))) / SR)
    creak = bp(creak * env(len(creak), 0.05, 0.2), 150, 1500) * 0.5
    coins = np.zeros(int(1.1 * SR))
    for _ in range(12):
        f = rng.uniform(2400, 4400)
        tt = t_axis(0.25)
        ping = np.sin(2 * np.pi * f * tt) * np.exp(-tt / 0.06) + 0.4 * np.sin(2 * np.pi * f * 1.51 * tt) * np.exp(-tt / 0.04)
        i = int(rng.uniform(0.35, 0.8) * SR)
        coins[i:i + len(ping)] += ping * rng.uniform(0.3, 0.8)
    sparkle = seq([(0.0, tone(880, 0.4, 0.15, ((1, 1), (2, 0.2)))), (0.09, tone(1320, 0.4, 0.15, ((1, 1), (2, 0.2)))),
                   (0.18, tone(1760, 0.6, 0.25, ((1, 1), (2, 0.2))))]) * 0.5
    return seq([(0.0, creak), (0.3, coins), (0.45, sparkle)])


# ------------------------------------------------------------------------------------------------ the underground
def cave_drip():
    tt = t_axis(0.18)
    f = 1100 + 1300 * (1 - np.exp(-tt / 0.02))
    plink = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt / 0.04)
    return echo(plink, ((0.09, 0.4), (0.19, 0.25), (0.33, 0.14), (0.52, 0.07)), tail=0.7)


def cave_amb(sec=24.0):
    n = int(sec * SR)
    brown = np.cumsum(rng.standard_normal(n))
    brown = hp(brown - np.mean(brown), 20)
    rumble = lp(brown, 110, 2)
    rumble /= np.max(np.abs(rumble)) or 1.0
    draught = bp(rng.standard_normal(n), 180, 520) * (0.35 + 0.65 * smooth_noise(sec, 0.12)) * 0.5
    x = rumble * 0.7 + draught
    for _ in range(9):   # far drips, low and dull
        d = lp(cave_drip(), 1600) * rng.uniform(0.05, 0.14)
        i = int(rng.uniform(0.5, sec - 2.0) * SR)
        x[i:i + len(d)] += d[:max(0, min(len(d), n - i))]
    return x


def gate_open():
    tt = t_axis(1.1)
    f = 160 + 120 * np.sin(np.pi * tt / 1.1) + 25 * smooth_noise(1.1, 18) - 12
    saw = 2 * ((np.cumsum(f) / SR) % 1.0) - 1
    creak = bp(saw, 200, 2400) * env(len(tt), 0.08, 0.3) * 0.6
    clank = tone(420, 0.8, 0.2, ((1, 1), (2.76, 0.5), (5.4, 0.25))) * 0.9
    return echo(seq([(0.0, creak), (1.0, clank)]), tail=0.6)


def gate_locked():
    parts = []
    for k in range(4):
        parts.append((k * 0.11 + rng.uniform(0, 0.02), tone(rng.uniform(380, 520), 0.25, 0.07, ((1, 1), (2.7, 0.6), (5.1, 0.3)))
                      * rng.uniform(0.6, 1.0)))
        parts.append((k * 0.11, noise_burst(0.03, 2000, 7000) * 0.3))
    return echo(seq(parts), ((0.12, 0.25), (0.26, 0.12)))


def passage_door():
    n = int(1.2 * SR)
    brown = np.cumsum(rng.standard_normal(n))
    brown = hp(brown - np.mean(brown), 30)
    grind = bp(brown, 90, 420) * (0.6 + 0.4 * smooth_noise(1.2, 9)) * env(n, 0.15, 0.4)
    grind /= np.max(np.abs(grind)) or 1.0
    thump = tone(62, 0.5, 0.12, ((1, 1), (1.5, 0.4)))
    return echo(seq([(0.0, grind * 0.8), (1.05, thump)]), tail=0.5)


def passage_ladder():
    parts = []
    for k in range(4):
        parts.append((k * 0.32, tone(rng.uniform(170, 240), 0.2, 0.05, ((1, 1), (2.3, 0.4))) * rng.uniform(0.6, 1.0)))
    tt = t_axis(0.45)
    f = 300 + 80 * np.sin(2 * np.pi * 2.2 * tt)
    creak = bp(np.sin(2 * np.pi * np.cumsum(f) / SR) + 0.3 * rng.standard_normal(len(tt)), 250, 1800) * env(len(tt), 0.05, 0.2) * 0.4
    parts.append((0.5, creak))
    return seq(parts)


save("S_Dig_Stab", dig_stab(), 0.75)
save("S_Dig_Toss", dig_toss(), 0.65)
save("S_Dig_Clank", dig_clank(), 0.9)
save("S_Dig_Chest", dig_chest(), 0.75)
save("S_Cave_Drip", cave_drip(), 0.55)
save("S_Cave_Amb_Loop", loopify(cave_amb(24.0), 1.5), 0.45)
save("S_Gate_Open", gate_open(), 0.8)
save("S_Gate_Locked", gate_locked(), 0.7)
save("S_Passage_Door", passage_door(), 0.75)
save("S_Passage_Ladder", passage_ladder(), 0.6)
