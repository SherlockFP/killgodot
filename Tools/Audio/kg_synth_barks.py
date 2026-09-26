"""SPRINT-023 placeholder bark voices for the Z/X/C voice commands (no downloads, CC0 by construction).

  python Tools/Audio/kg_synth_barks.py Art/Audio/Barks [MaleLow,Female]

Writes one 16-bit 48 kHz mono WAV per (voice, line): S_Bark_<Voice>_<LineId>.wav. Lines and syllable counts mirror
FKGVoiceCommandCatalog (Source/KillGodot/Voice/KGVoiceCommands.cpp); the bark length is the same formula the mouth
uses (0.16 s per syllable + 0.25 s), so the jaw and the sound end together on every machine.

Voice = a glottal pulse train (sawtooth with jitter and vibrato) through three formant band-passes that glide between
vowels per syllable, gated by syllable envelopes, with a short noise burst at each onset (the "consonant"). Four voice
types differ in f0, formant scale, breathiness and rasp. Import: Tools/Unreal/kg_import_barks.py (commandlet).
"""
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter

SR = 48000
out_dir = sys.argv[1] if len(sys.argv) > 1 else "Art/Audio/Barks"
ONLY = [p for p in (sys.argv[2].split(",") if len(sys.argv) > 2 else []) if p]
os.makedirs(out_dir, exist_ok=True)

# id, syllables, contour: 'call' rises and holds, 'alarm' loud and sharp, 'question' rises at the end,
# 'statement' falls, 'short' one clipped syllable, 'cheer' bright and rising
LINES = [
    ("overhere", 3, "call"), ("help", 1, "alarm"), ("followme", 3, "call"), ("meeting", 6, "call"),
    ("whosthere", 2, "question"), ("wait", 1, "alarm"), ("gogogo", 3, "alarm"), ("run", 1, "alarm"),
    ("sawsomething", 4, "statement"), ("suspicious", 3, "statement"), ("iwasat", 4, "statement"), ("trustme", 2, "statement"),
    ("notme", 4, "question"), ("votethem", 2, "alarm"), ("mychore", 5, "statement"), ("watchthem", 2, "statement"),
    ("yes", 1, "short"), ("no", 1, "short"), ("thanks", 1, "cheer"), ("sorry", 2, "statement"),
    ("goodgame", 2, "cheer"), ("nice", 1, "cheer"), ("hello", 2, "cheer"), ("bye", 1, "cheer"),
]

# f0 Hz, formant scale (1 = adult male), breath, rasp (30 Hz amplitude modulation depth), brightness
VOICES = {
    "MaleLow": dict(f0=98.0, fscale=0.95, breath=0.06, rasp=0.0, bright=0.8),
    "MaleYoung": dict(f0=135.0, fscale=1.05, breath=0.05, rasp=0.0, bright=1.0),
    "Female": dict(f0=215.0, fscale=1.18, breath=0.08, rasp=0.0, bright=1.15),
    "Old": dict(f0=112.0, fscale=0.98, breath=0.14, rasp=0.45, bright=0.7),
}

# vowel formants (F1, F2, F3) in Hz for an adult male
VOWELS = {"a": (730, 1090, 2440), "e": (530, 1840, 2480), "i": (390, 1990, 2550), "o": (570, 840, 2410), "u": (440, 1020, 2240)}
VOWEL_ORDER = "aoeiu"


def duration(syllables):
    return 0.16 * syllables + 0.25


def bp(x, lo, hi):
    lo = max(40.0, lo)
    hi = min(SR / 2 - 100.0, hi)
    b, a = butter(2, [lo / (SR / 2), hi / (SR / 2)], "band")
    return lfilter(b, a, x)


def lp(x, hz):
    b, a = butter(2, min(hz, SR / 2 - 100) / (SR / 2), "low")
    return lfilter(b, a, x)


def hp(x, hz):
    b, a = butter(2, max(hz, 20) / (SR / 2), "high")
    return lfilter(b, a, x)


def pitch_contour(n, f0, contour, syll, rng):
    """Per-sample f0 multiplier: a musical phrase shape per contour type plus per-syllable accents."""
    t = np.linspace(0.0, 1.0, n)
    if contour == "call":
        base = 1.0 + 0.10 * np.sin(t * np.pi) + 0.12 * (t > 0.7)
    elif contour == "alarm":
        base = 1.22 - 0.10 * t
    elif contour == "question":
        base = 0.96 + 0.28 * t ** 2
    elif contour == "statement":
        base = 1.08 - 0.20 * t
    elif contour == "short":
        base = 1.05 - 0.08 * t
    else:  # cheer
        base = 1.0 + 0.18 * t + 0.05 * np.sin(t * 6.0)
    # small accent per syllable
    acc = np.ones(n)
    for k in range(syll):
        a, b = int(n * k / syll), int(n * (k + 1) / syll)
        acc[a:b] *= 1.0 + rng.uniform(-0.04, 0.06)
    vib = 1.0 + 0.012 * np.sin(2 * np.pi * 5.5 * t * (n / SR))
    return base * acc * vib


def syllable_env(n, syll, contour):
    """The same 60 % duty sine humps as FKGVoiceCommandCatalog::MouthEnvelope (accents differ, that is fine)."""
    env = np.zeros(n)
    slot = n / syll
    for k in range(syll):
        a = int(k * slot)
        w = int(slot * 0.6)
        hump = np.sin(np.linspace(0, np.pi, w)) ** 0.8
        peak = 1.0
        if contour == "call" and k == syll - 1:
            peak = 1.1
        if contour == "statement":
            peak = 1.0 - 0.08 * k
        env[a:a + w] = hump[: max(0, min(w, n - a))] * peak
    return env


def vowel_track(n, syll, rng):
    """Formant targets glide between a vowel per syllable."""
    pts = []
    for k in range(syll):
        v = VOWELS[VOWEL_ORDER[(k * 2 + rng.integers(0, 2)) % 5]]
        pts.append(v)
    pts = np.array(pts, dtype=float)
    if syll == 1:
        return np.repeat(pts, n, axis=0)
    xs = np.linspace(0, syll - 1, n)
    return np.stack([np.interp(xs, np.arange(syll), pts[:, i]) for i in range(3)], axis=1)


def synth(line_id, syll, contour, voice, rng):
    v = VOICES[voice]
    dur = duration(syll)
    n = int(SR * dur)
    t = np.arange(n) / SR
    f0 = v["f0"] * pitch_contour(n, v["f0"], contour, syll, rng)
    jitter = 1.0 + 0.006 * lp(rng.standard_normal(n), 40.0)
    phase = np.cumsum(f0 * jitter / SR)
    # glottal source: sawtooth softened by a low-pass, plus breath noise
    src = 2.0 * (phase % 1.0) - 1.0
    src = lp(src, 3500.0 * v["bright"])
    breath = hp(rng.standard_normal(n), 1500.0) * v["breath"]
    src = src + breath
    if v["rasp"] > 0:
        src *= 1.0 - v["rasp"] * 0.5 * (1 + np.sin(2 * np.pi * 31.0 * t))
    # formants: three band-passes tracking the vowel per syllable (piecewise: 12 segments for speed)
    form = vowel_track(n, syll, rng) * v["fscale"]
    out = np.zeros(n)
    seg = max(1, n // 12)
    for s in range(0, n, seg):
        e = min(n, s + seg)
        f1, f2, f3 = form[(s + e) // 2]
        chunk = src[max(0, s - 400):e]
        y = 1.0 * bp(chunk, f1 * 0.8, f1 * 1.25) + 0.55 * bp(chunk, f2 * 0.88, f2 * 1.14) + 0.25 * bp(chunk, f3 * 0.92, f3 * 1.09)
        out[s:e] += y[-(e - s):]
    env = syllable_env(n, syll, contour)
    out *= env
    # consonant bursts at syllable onsets
    slot = n / syll
    for k in range(syll):
        a = int(k * slot)
        w = int(0.018 * SR)
        if a + w < n:
            burst = bp(rng.standard_normal(w), 1800.0, 6500.0) * np.linspace(1, 0, w) ** 2 * 0.35
            out[a:a + w] += burst
    # loudness per contour, then a gentle limiter
    gain = {"alarm": 1.15, "call": 1.05, "cheer": 1.0, "question": 0.95, "statement": 0.9, "short": 0.95}[contour]
    out = out / (np.max(np.abs(out)) + 1e-6) * 0.85 * gain
    out = np.tanh(out * 1.3) / np.tanh(1.3)
    # 5 ms fade in/out
    f = int(0.005 * SR)
    out[:f] *= np.linspace(0, 1, f)
    out[-f:] *= np.linspace(1, 0, f)
    return out


count = 0
for voice in VOICES:
    if ONLY and voice not in ONLY:
        continue
    for i, (line_id, syll, contour) in enumerate(LINES):
        rng = np.random.default_rng(1848 + i * 17 + list(VOICES).index(voice) * 1009)
        x = synth(line_id, syll, contour, voice, rng)
        name = f"S_Bark_{voice}_{line_id}.wav"
        wavfile.write(os.path.join(out_dir, name), SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
        count += 1
print(f"wrote {count} barks to {out_dir}")
