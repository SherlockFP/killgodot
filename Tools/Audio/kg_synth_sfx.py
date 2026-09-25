"""Procedural sound design for Morrowmere (no downloads, all synthesised, CC0 by construction).

  python Tools/Audio/kg_synth_sfx.py Art/Audio [prefixes]

prefixes (optional, comma separated): only write sounds whose name starts with one of them, e.g. "S_Chore_,S_UI_"
for the chore minigame set. Everything is still synthesised in the same order, so the other sounds stay identical.

Writes 16-bit 48 kHz mono WAVs. Loops are written seamless (crossfaded) and named *_Loop.
Ambience: wind, sea waves, harbour water lapping, birds (day), seagulls, crickets (night), fire crackle.
One-shots: church bell, door bell, footsteps (grass/wood/stone), splash, crate break, punch, knife stab,
doors, task complete chime, meeting gong, bite "plop", reel click.
Chore minigames (S_UI_*, S_Chore_*): UI blips, stage chime, crank ratchet, splash, chop, rope, whoosh, paper, pin,
stamp, anvil, bellows, whetstone, pour, dough, coin, pluck, match, clock tick, millstone, thud, tar brush, flame.
Import them with Tools/Unreal/kg_import_chore_assets.py (commandlet, editor closed).
Fishing (S_Fish_*, S_FishReel_Loop; Source/KillGodot/Fishing): cast whoosh, nibble, bite, hook zip, reel ratchet loop,
line snap, catch fanfare, escape plonk, coin jingle. "python Tools/Audio/kg_synth_sfx.py Art/Audio S_Fish_,S_FishReel",
then Tools/Unreal/kg_import_audio.py --only S_Fish_,S_FishReel (commandlet).
"""
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter

SR = 48000
rng = np.random.default_rng(1848)
out_dir = sys.argv[1] if len(sys.argv) > 1 else "Art/Audio"
ONLY = [p for p in (sys.argv[2].split(",") if len(sys.argv) > 2 else []) if p]
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
    """Slow random control signal 0..1."""
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
    if ONLY and not any(name.startswith(p) for p in ONLY):
        return
    x = np.asarray(x, dtype=np.float64)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * gain
    wavfile.write(os.path.join(out_dir, f"{name}.wav"), SR, (x * 32767).astype(np.int16))
    print(f"KG_SFX {name}.wav {len(x) / SR:.1f}s")


# ------------------------------------------------------------------------------------------------ ambience loops
def wind_loop(sec=24.0):
    n = rng.standard_normal(int(SR * sec))
    gust = 0.35 + 0.65 * smooth_noise(sec, 0.25)
    body = bp(n, 150, 700) * gust
    whistle = bp(n, 900 + 0.0, 1600) * (gust ** 3) * 0.35
    return loopify(body + whistle)


def sea_loop(sec=24.0):
    t = t_axis(sec)
    n = rng.standard_normal(len(t))
    swell = np.zeros(len(t))
    # Individual waves arriving every 4-7 s: roar up, hiss down.
    pos = 0.0
    while pos < sec:
        dur = rng.uniform(3.5, 6.5)
        i0, i1 = int(pos * SR), min(len(t), int((pos + dur) * SR))
        seg = np.linspace(0, 1, i1 - i0)
        swell[i0:i1] += np.sin(np.pi * seg) ** 1.5 * (0.6 + 0.4 * rng.random())
        pos += rng.uniform(3.0, 5.5)
    roar = lp(n, 500) * swell
    hiss = bp(n, 2000, 7000) * np.roll(swell, int(0.8 * SR)) ** 2 * 0.25
    return loopify(roar + hiss)


def lapping_loop(sec=16.0):
    """Water slapping against dock posts: short gurgles."""
    x = np.zeros(int(SR * sec))
    pos = 0.0
    while pos < sec - 0.5:
        L = rng.uniform(0.15, 0.4)
        seg = bp(rng.standard_normal(int(L * SR)), 200, rng.uniform(900, 1600)) * env(int(L * SR), 0.01, L * 0.8)
        i0 = int(pos * SR)
        x[i0:i0 + len(seg)] += seg * rng.uniform(0.3, 1.0)
        pos += rng.uniform(0.25, 0.9)
    return loopify(x, 0.5)


def chirp(f0, f1, dur, vib=0.0):
    t = t_axis(dur)
    f = np.linspace(f0, f1, len(t)) * (1 + vib * np.sin(2 * np.pi * 30 * t))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph) * env(len(t), 0.005, dur * 0.6, 2)


def birds_loop(sec=30.0):
    x = np.zeros(int(SR * sec))
    pos = 0.3
    while pos < sec - 1.5:
        call = np.concatenate([chirp(rng.uniform(3000, 4200), rng.uniform(4500, 6000), rng.uniform(0.05, 0.12), 0.02)
                               for _ in range(rng.integers(2, 6))])
        i0 = int(pos * SR)
        x[i0:i0 + len(call)] += call * rng.uniform(0.2, 0.7)
        pos += rng.uniform(0.6, 3.5)
    return loopify(x, 0.8)


def gull_cry():
    parts = []
    for _ in range(rng.integers(2, 4)):
        d = rng.uniform(0.25, 0.45)
        t = t_axis(d)
        f = 1100 + 700 * np.sin(np.pi * t / d) - 300 * t / d
        ph = 2 * np.pi * np.cumsum(f) / SR
        tone = np.sin(ph) + 0.5 * np.sin(2 * ph) + 0.3 * np.sin(3 * ph)
        rough = 1 + 0.4 * np.sin(2 * np.pi * 70 * t)
        parts.append(tone * rough * env(len(t), 0.02, d * 0.5) )
        parts.append(np.zeros(int(0.08 * SR)))
    return bp(np.concatenate(parts), 600, 5000)


def gulls_loop(sec=30.0):
    x = np.zeros(int(SR * sec))
    pos = 1.0
    while pos < sec - 2.0:
        c = gull_cry()
        i0 = int(pos * SR)
        x[i0:i0 + len(c)] += c * rng.uniform(0.3, 1.0)
        pos += rng.uniform(3.0, 8.0)
    return loopify(x, 0.8)


def crickets_loop(sec=20.0):
    t = t_axis(sec)
    x = np.zeros(len(t))
    for k in range(5):
        f = rng.uniform(4200, 5200)
        rate = rng.uniform(12, 18)
        gate = (np.sin(2 * np.pi * rate * t + rng.random() * 6) > 0.6).astype(float)
        pulse = (smooth_noise(sec, 0.3) > 0.35).astype(float)
        x += np.sin(2 * np.pi * f * t) * lp(gate, 200, 1) * lp(pulse, 2, 1) * rng.uniform(0.3, 0.8)
    return loopify(x)


def fire_loop(sec=12.0):
    x = lp(rng.standard_normal(int(SR * sec)), 300) * 0.4
    pos = 0.0
    while pos < sec - 0.1:
        L = rng.uniform(0.003, 0.02)
        crack = hp(rng.standard_normal(int(L * SR)), 1500) * env(int(L * SR), 0.0005, L * 0.9)
        i0 = int(pos * SR)
        x[i0:i0 + len(crack)] += crack * rng.uniform(0.5, 2.0)
        pos += rng.exponential(0.12)
    return loopify(x, 0.5)


# ------------------------------------------------------------------------------------------------ one-shots
def bell(f0=220.0, dur=6.0):
    """Church bell: inharmonic partials (hum, prime, tierce, quint, nominal...) with individual decays."""
    t = t_axis(dur)
    partials = [(0.5, 1.0, 4.0), (1.0, 0.8, 3.0), (1.19, 0.6, 2.2), (1.5, 0.45, 1.8), (2.0, 0.7, 1.5),
                (2.51, 0.35, 1.0), (2.66, 0.3, 0.9), (3.01, 0.25, 0.7), (4.1, 0.18, 0.5)]
    x = np.zeros(len(t))
    for ratio, amp, decay in partials:
        beat = 1 + 0.002 * np.sin(2 * np.pi * rng.uniform(0.5, 2) * t)
        x += amp * np.sin(2 * np.pi * f0 * ratio * beat * t) * np.exp(-t / decay)
    strike = hp(rng.standard_normal(int(0.02 * SR)), 2000) * env(int(0.02 * SR), 0.0005, 0.018)
    x[:len(strike)] += strike * 0.5
    return x


def doorbell():
    t = t_axis(1.6)
    x = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, a, d in
            [(1320, 1.0, 0.5), (2640, 0.4, 0.3), (3960, 0.25, 0.2), (1760, 0.6, 0.45)])
    return x * env(len(t), 0.001, 0.3)


def footstep(kind):
    L = 0.18
    n = rng.standard_normal(int(L * SR))
    if kind == "Grass":
        x = bp(n, 800, 6000) * env(len(n), 0.01, 0.15)
    elif kind == "Wood":
        thump = np.sin(2 * np.pi * 110 * t_axis(L)) * np.exp(-t_axis(L) / 0.04)
        x = thump + bp(n, 300, 1500) * env(len(n), 0.002, 0.08) * 0.6
    else:  # Stone
        x = bp(n, 1500, 7000) * env(len(n), 0.001, 0.06) + lp(n, 300) * env(len(n), 0.002, 0.05) * 0.5
    return x


def splash(big=False):
    L = 1.2 if big else 0.6
    n = rng.standard_normal(int(L * SR))
    x = bp(n, 300, 6000) * env(len(n), 0.005, L * 0.9, 2.0)
    bubbles = sum(chirp(rng.uniform(400, 900), rng.uniform(900, 1600), 0.05) * 0.2 for _ in range(1))
    x[: len(bubbles)] += bubbles
    return x


def crate_break():
    L = 0.9
    n = rng.standard_normal(int(L * SR))
    crack = hp(n, 800) * env(len(n), 0.001, 0.15, 4) * 1.5
    t = t_axis(L)
    thud = np.sin(2 * np.pi * 90 * t) * np.exp(-t / 0.08)
    rattle = np.zeros(len(n))
    for _ in range(8):
        i0 = int(rng.uniform(0.08, 0.7) * SR)
        k = bp(rng.standard_normal(int(0.03 * SR)), 600, 3000) * env(int(0.03 * SR), 0.001, 0.025)
        rattle[i0:i0 + len(k)] += k * rng.uniform(0.2, 0.6)
    return crack + thud + rattle


def punch():
    L = 0.25
    t = t_axis(L)
    thump = np.sin(2 * np.pi * (140 - 200 * t) * t) * np.exp(-t / 0.05)
    slap = bp(rng.standard_normal(len(t)), 1000, 5000) * env(len(t), 0.001, 0.05)
    return thump + slap * 0.6


def stab():
    L = 0.4
    t = t_axis(L)
    swish = bp(rng.standard_normal(len(t)), 2000, 8000) * np.sin(np.pi * t / L) ** 2 * 0.6
    squelch = lp(rng.standard_normal(len(t)), 500) * env(len(t), 0.1, 0.25) * 0.8
    return swish + squelch


def door(opening=True):
    L = 1.0
    t = t_axis(L)
    f = (380 if opening else 320) + 120 * np.sin(2 * np.pi * 1.3 * t)
    creak = np.sign(np.sin(2 * np.pi * np.cumsum(f) / SR)) * (0.3 + 0.7 * smooth_noise(L, 12))
    creak = bp(creak, 300, 2500) * env(len(t), 0.05, 0.3)
    if not opening:
        thud = np.sin(2 * np.pi * 70 * t) * np.exp(-(t - 0.75).clip(0) / 0.08) * (t > 0.75)
        creak = creak * 0.6 + thud
    return creak


def chime():
    t = t_axis(1.2)
    notes = [(784, 0.0), (988, 0.09), (1175, 0.18), (1568, 0.27)]
    x = np.zeros(len(t))
    for f, start in notes:
        tt = (t - start).clip(0)
        x += np.sin(2 * np.pi * f * tt) * np.exp(-tt / 0.35) * (t >= start)
    return x


def gong():
    t = t_axis(5.0)
    x = sum(a * np.sin(2 * np.pi * f * t + 3 * np.sin(2 * np.pi * 0.7 * t)) * np.exp(-t / d)
            for f, a, d in [(98, 1.0, 3.5), (196.5, 0.5, 2.5), (287, 0.4, 2.0), (401, 0.3, 1.2)])
    return x


def plop():
    return chirp(900, 300, 0.12) + bp(rng.standard_normal(int(0.12 * SR)), 500, 3000) * env(int(0.12 * SR), 0.002, 0.1) * 0.4


def reel_click():
    x = np.zeros(int(0.6 * SR))
    for k in range(12):
        i0 = int(k * 0.045 * SR)
        c = hp(rng.standard_normal(int(0.006 * SR)), 3000) * env(int(0.006 * SR), 0.0002, 0.005)
        x[i0:i0 + len(c)] += c
    return x


save("A_Wind_Loop", wind_loop(), 0.5)
save("A_Sea_Loop", sea_loop(), 0.7)
save("A_Lapping_Loop", lapping_loop(), 0.5)
save("A_Birds_Loop", birds_loop(), 0.45)
save("A_Gulls_Loop", gulls_loop(), 0.6)
save("A_Crickets_Loop", crickets_loop(), 0.35)
save("A_Fire_Loop", fire_loop(), 0.5)
save("S_ChurchBell", bell(196.0), 0.9)
save("S_Doorbell", doorbell(), 0.7)
for k in ("Grass", "Wood", "Stone"):
    for v in range(3):
        save(f"S_Step_{k}_{v}", footstep(k), 0.5)
save("S_Splash", splash(), 0.7)
save("S_Splash_Big", splash(True), 0.8)
save("S_CrateBreak", crate_break(), 0.85)
save("S_Punch", punch(), 0.8)
save("S_Stab", stab(), 0.8)
save("S_DoorOpen", door(True), 0.6)
save("S_DoorClose", door(False), 0.7)
save("S_TaskDone", chime(), 0.6)
save("S_MeetingGong", gong(), 0.85)
save("S_FishBite", plop(), 0.7)
save("S_ReelClick", reel_click(), 0.5)


# ------------------------------------------------------------------------------------------------ chore minigames
# Short, dry UI-scale sounds (played 2D by SKGChorePanel). Appended after the originals so their rng stream is unchanged.
def tone(f, dur, decay, harmonics=((1.0, 1.0),), attack=0.002):
    t = t_axis(dur)
    x = sum(a * np.sin(2 * np.pi * f * r * t) for r, a in harmonics)
    return x * np.exp(-t / decay) * env(len(t), attack, min(0.02, dur * 0.3), 1.0)


def noise_burst(dur, lo, hi, attack=0.001, release=None):
    n = rng.standard_normal(int(dur * SR))
    return bp(n, lo, hi) * env(len(n), attack, release if release is not None else dur * 0.9, 2.5)


def mix(*xs):
    """Sum signals of different lengths (padded with silence)."""
    n = max(len(x) for x in xs)
    out = np.zeros(n)
    for x in xs:
        out[:len(x)] += x
    return out


def seq(parts, gap_s=0.0):
    """Concatenate (offset_s, signal) pairs into one buffer."""
    total = max(int((off + gap_s) * SR) + len(sig) for off, sig in parts)
    x = np.zeros(total)
    for off, sig in parts:
        i0 = int(off * SR)
        x[i0:i0 + len(sig)] += sig
    return x


def ui_good():
    return seq([(0.0, tone(988, 0.16, 0.06, ((1, 1), (2, 0.25)))), (0.06, tone(1480, 0.2, 0.08, ((1, 1), (2, 0.2))))])


def ui_bad():
    t = t_axis(0.28)
    f = 170 - 60 * t / 0.28
    x = np.sign(np.sin(2 * np.pi * np.cumsum(f) / SR)) * 0.5 + np.sin(2 * np.pi * np.cumsum(f * 0.5) / SR)
    return lp(x, 1200) * env(len(t), 0.004, 0.2, 2.0)


def ui_click():
    return mix(tone(2400, 0.035, 0.008), noise_burst(0.02, 2000, 7000) * 0.3)


def stage_chime():
    marimba = ((1, 1.0), (4.0, 0.25), (9.9, 0.08))
    return seq([(0.0, tone(659, 0.35, 0.12, marimba)), (0.09, tone(988, 0.45, 0.16, marimba)), (0.18, tone(1319, 0.5, 0.2, marimba))])


def crank():
    parts = [(k * 0.028, noise_burst(0.012, 1500, 6000) * (1.0 - 0.2 * k)) for k in range(3)]
    parts.append((0.0, tone(180, 0.1, 0.03) * 0.6))
    return seq(parts)


def small_splash():
    x = noise_burst(0.45, 400, 5000, 0.004, 0.4)
    for k in range(4):
        i0 = int(rng.uniform(0.02, 0.2) * SR)
        c = chirp(rng.uniform(500, 900), rng.uniform(1100, 1800), 0.04) * 0.3
        x[i0:i0 + len(c)] += c
    return x


def chop():
    t = t_axis(0.35)
    thunk = np.sin(2 * np.pi * (220 - 300 * t) * t) * np.exp(-t / 0.05)
    crack = noise_burst(0.35, 1200, 6000, 0.0005, 0.08) * 1.2
    return mix(thunk, crack)


def rope_knot():
    creak = door(True)[: int(0.22 * SR)] * env(int(0.22 * SR), 0.01, 0.15)
    return mix(creak * 0.7, tone(120, 0.18, 0.05) * 0.8)


def whoosh():
    t = t_axis(0.32)
    n = rng.standard_normal(len(t))
    sweep = np.zeros(len(t))
    for i0 in range(0, len(t), 480):
        f = 600 + 2400 * np.sin(np.pi * i0 / len(t))
        seg = bp(n[i0:i0 + 480], f * 0.7, min(f * 1.4, 20000))
        sweep[i0:i0 + len(seg)] = seg
    return sweep * np.sin(np.pi * t / 0.32) ** 2


def paper():
    x = np.zeros(int(0.38 * SR))
    for k in range(9):
        i0 = int(rng.uniform(0.0, 0.3) * SR)
        c = noise_burst(rng.uniform(0.02, 0.06), 2500, 9000) * rng.uniform(0.3, 1.0)
        x[i0:i0 + len(c)] += c[: len(x) - i0]
    return x


def pin():
    return mix(tone(3200, 0.12, 0.03, ((1, 1), (2.76, 0.5))), noise_burst(0.01, 3000, 9000) * 0.5)


def stamp():
    return mix(tone(95, 0.22, 0.05) * 1.2, noise_burst(0.08, 400, 3000) * 0.6)


def anvil():
    t = t_axis(0.9)
    x = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, a, d in
            [(1180, 1.0, 0.35), (2630, 0.6, 0.25), (3910, 0.4, 0.18), (5230, 0.25, 0.1)])
    return mix(x, noise_burst(0.02, 2000, 9000) * 0.8)


def bellows():
    n = rng.standard_normal(int(0.5 * SR))
    return lp(n, 900) * np.sin(np.pi * t_axis(0.5) / 0.5) ** 1.5


def scrape():
    t = t_axis(0.42)
    n = bp(rng.standard_normal(len(t)), 2200, 7000)
    return n * (0.6 + 0.4 * np.sin(2 * np.pi * 38 * t)) * np.sin(np.pi * t / 0.42)


def pour():
    x = lp(rng.standard_normal(int(0.8 * SR)), 900) * 0.4
    for k in range(14):
        i0 = int(rng.uniform(0.0, 0.7) * SR)
        c = chirp(rng.uniform(300, 600), rng.uniform(700, 1300), rng.uniform(0.03, 0.07)) * rng.uniform(0.3, 0.8)
        x[i0:i0 + len(c)] += c[: len(x) - i0]
    return x * env(len(x), 0.03, 0.2)


def squish():
    t = t_axis(0.26)
    body = lp(rng.standard_normal(len(t)), 500) * np.exp(-t / 0.09)
    return body + np.sin(2 * np.pi * (160 - 200 * t) * t) * np.exp(-t / 0.06) * 0.6


def coin():
    return seq([(0.0, tone(2900, 0.35, 0.12, ((1, 1), (2.4, 0.5)))), (0.07, tone(3400, 0.35, 0.1, ((1, 1), (2.4, 0.4))))])


def pluck():
    t = t_axis(0.16)
    pop = np.sin(2 * np.pi * (700 - 2600 * t) * t) * np.exp(-t / 0.04)
    return mix(pop, noise_burst(0.16, 300, 2500, 0.001, 0.12) * 0.5)


def match_strike():
    scratch = noise_burst(0.12, 1500, 8000) * 1.2
    flare = lp(rng.standard_normal(int(0.45 * SR)), 1500) * env(int(0.45 * SR), 0.03, 0.35)
    return seq([(0.0, scratch), (0.08, flare * 0.8)])


def clock_tick():
    return mix(tone(1800, 0.05, 0.01), noise_burst(0.015, 3000, 8000) * 0.5)


def grind():
    t = t_axis(0.6)
    rumble = lp(rng.standard_normal(len(t)), 250) * (0.7 + 0.3 * np.sin(2 * np.pi * 9 * t))
    grit = bp(rng.standard_normal(len(t)), 900, 3000) * 0.25
    return (rumble + grit) * env(len(t), 0.04, 0.2)


def thud():
    t = t_axis(0.2)
    return mix(np.sin(2 * np.pi * (110 - 150 * t) * t) * np.exp(-t / 0.05), noise_burst(0.05, 200, 1500) * 0.3)


def brush():
    t = t_axis(0.32)
    swish = bp(rng.standard_normal(len(t)), 800, 4000) * np.sin(np.pi * t / 0.32) ** 1.5
    sticky = lp(rng.standard_normal(len(t)), 400) * (np.sin(2 * np.pi * 22 * t) > 0.6) * 0.4
    return swish + sticky


def flame():
    t = t_axis(0.55)
    whoomp = lp(rng.standard_normal(len(t)), 600) * env(len(t), 0.05, 0.4)
    return whoomp + np.sin(2 * np.pi * (70 + 60 * t) * t) * np.exp(-t / 0.15) * 0.5


save("S_UI_Good", ui_good(), 0.55)
save("S_UI_Bad", ui_bad(), 0.5)
save("S_UI_Click", ui_click(), 0.4)
save("S_Chore_Stage", stage_chime(), 0.6)
save("S_Chore_Crank", crank(), 0.5)
save("S_Chore_Splash", small_splash(), 0.6)
save("S_Chore_Chop", chop(), 0.8)
save("S_Chore_Knot", rope_knot(), 0.6)
save("S_Chore_Whoosh", whoosh(), 0.5)
save("S_Chore_Paper", paper(), 0.5)
save("S_Chore_Pin", pin(), 0.5)
save("S_Chore_Stamp", stamp(), 0.7)
save("S_Chore_Anvil", anvil(), 0.7)
save("S_Chore_Bellows", bellows(), 0.55)
save("S_Chore_Scrape", scrape(), 0.5)
save("S_Chore_Pour", pour(), 0.55)
save("S_Chore_Squish", squish(), 0.6)
save("S_Chore_Coin", coin(), 0.5)
save("S_Chore_Pluck", pluck(), 0.6)
save("S_Chore_Match", match_strike(), 0.6)
save("S_Chore_Tick", clock_tick(), 0.5)
save("S_Chore_Grind", grind(), 0.6)
save("S_Chore_Thud", thud(), 0.7)
save("S_Chore_Brush", brush(), 0.5)
save("S_Chore_Flame", flame(), 0.6)


# ------------------------------------------------------------------------------------------------ fishing
# Appended after every older sound so the shared random stream above stays unchanged.
def fish_cast():
    L = 0.55
    t = t_axis(L)
    n = rng.standard_normal(len(t))
    sweep = np.sin(np.pi * np.clip(t / L, 0, 1)) ** 1.5
    x = bp(n, 700, 5200) * sweep
    zip_ = chirp(1800, 5200, 0.30) * 0.10
    i0 = int(0.12 * SR)
    x[i0:i0 + len(zip_)] += zip_
    return x


def fish_nibble():
    x = np.zeros(int(0.32 * SR))
    for start, f0 in ((0.0, 1400), (0.13, 1250)):
        c = chirp(f0, f0 * 0.55, 0.05) * env(int(0.05 * SR), 0.002, 0.04) * 0.5
        i0 = int(start * SR)
        x[i0:i0 + len(c)] += c
    return x


def fish_bite():
    L = 0.7
    blip = chirp(760, 170, 0.16)
    n = rng.standard_normal(int(L * SR))
    x = bp(n, 250, 4000) * env(len(n), 0.004, L * 0.85, 2.5) * 0.7
    x[:len(blip)] += blip * 1.2
    return x


def fish_hook():
    L = 0.28
    t = t_axis(L)
    f = 500 + 5200 * (t / L) ** 1.6
    ph = 2 * np.pi * np.cumsum(f) / SR
    zz = np.sign(np.sin(ph)) * 0.35 + np.sin(ph) * 0.65
    return bp(zz, 300, 7000) * env(len(t), 0.005, 0.08)


def fish_reel_loop(sec=1.0):
    x = np.zeros(int(sec * SR))
    for k in range(int(sec * 24)):
        i0 = int(k * SR / 24)
        c = hp(rng.standard_normal(int(0.005 * SR)), 2500) * env(int(0.005 * SR), 0.0002, 0.004)
        x[i0:i0 + len(c)] += c * (0.8 + 0.4 * rng.random())
    return x + bp(rng.standard_normal(len(x)), 400, 1400) * 0.05


def fish_snap():
    L = 0.8
    N = int(SR / 196.0)
    buf = rng.uniform(-1, 1, N)
    out = np.zeros(int(L * SR))
    for i in range(len(out)):          # Karplus-Strong twang of the line
        out[i] = buf[i % N]
        buf[i % N] = 0.996 * 0.5 * (buf[i % N] + buf[(i + 1) % N])
    crack = hp(rng.standard_normal(int(0.03 * SR)), 2000) * env(int(0.03 * SR), 0.0005, 0.025) * 1.5
    out = out * env(len(out), 0.001, 0.6, 2.0)
    out[:len(crack)] += crack
    return out


def fish_fanfare():
    t = t_axis(1.5)
    notes = [(523.25, 0.00), (659.25, 0.09), (783.99, 0.18), (1046.5, 0.30), (783.99, 0.52), (1046.5, 0.62)]
    x = np.zeros(len(t))
    for f, start in notes:
        tt = (t - start).clip(0)
        x += (np.sin(2 * np.pi * f * tt) + 0.35 * np.sin(4 * np.pi * f * tt)) * np.exp(-tt / 0.42) * (t >= start)
    return x + bp(rng.standard_normal(len(t)), 5000, 11000) * np.exp(-t / 0.5) * 0.08


def fish_escape():
    plonk = chirp(420, 120, 0.35) * env(int(0.35 * SR), 0.004, 0.3)
    n = rng.standard_normal(int(0.6 * SR))
    x = bp(n, 300, 3000) * env(len(n), 0.01, 0.5, 2.0) * 0.45
    x[:len(plonk)] += plonk
    return x


def fish_coins():
    x = np.zeros(int(0.7 * SR))
    for _ in range(7):
        f = rng.uniform(2400, 4200)
        tt = t_axis(0.25)
        ping = np.sin(2 * np.pi * f * tt) * np.exp(-tt / 0.06) + 0.4 * np.sin(2 * np.pi * f * 1.51 * tt) * np.exp(-tt / 0.04)
        i0 = int(rng.uniform(0.0, 0.4) * SR)
        x[i0:i0 + len(ping)] += ping * rng.uniform(0.4, 0.9)
    return x


save("S_Fish_Cast", fish_cast(), 0.7)
save("S_Fish_Nibble", fish_nibble(), 0.45)
save("S_Fish_Bite", fish_bite(), 0.8)
save("S_Fish_Hook", fish_hook(), 0.6)
save("S_FishReel_Loop", loopify(fish_reel_loop(1.2), 0.2), 0.5)
save("S_Fish_Snap", fish_snap(), 0.85)
save("S_Fish_Fanfare", fish_fanfare(), 0.7)
save("S_Fish_Escape", fish_escape(), 0.7)
save("S_Fish_Coins", fish_coins(), 0.6)


# ---- World chores (SPRINT-016, S_WC_*): door knock, water slosh -------------------------------------------------------
# Appended last so every sound above keeps its exact random stream.
def knock():
    """Three knuckle raps on a wooden door: a short hollow body resonance plus a click."""
    x = np.zeros(int(0.62 * SR))
    for k, at in enumerate((0.0, 0.2, 0.37)):
        t = t_axis(0.12)
        body = np.sin(2 * np.pi * (190 - 60 * t) * t) * np.exp(-t / 0.028) + 0.5 * np.sin(2 * np.pi * 410 * t) * np.exp(-t / 0.015)
        click = noise_burst(0.12, 1500, 5000, 0.0003, 0.006) * 0.5
        rap = mix(body, click) * (1.0 if k < 2 else 1.15)
        i0 = int(at * SR)
        x[i0:i0 + len(rap)] += rap[: len(x) - i0]
    return x


def slosh():
    """Water lurching in a bucket: a low swell of filtered noise with a couple of drips."""
    t = t_axis(0.55)
    swell = bp(rng.standard_normal(len(t)), 250, 1400) * np.sin(np.pi * t / 0.55) ** 2
    drip = np.zeros(len(t))
    for at in (0.28, 0.41):
        tt = t_axis(0.06)
        d = np.sin(2 * np.pi * (900 + 1400 * tt) * tt) * np.exp(-tt / 0.012)
        i0 = int(at * SR)
        drip[i0:i0 + len(d)] += d * 0.5
    return swell * 0.9 + drip


save("S_WC_Knock", knock(), 0.8)
save("S_WC_Slosh", slosh(), 0.55)
