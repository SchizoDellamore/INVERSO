#!/usr/bin/env python3
"""Builds Abismo/Library/drums.bin, the ABISMO DRUMS sample library.

Every source recording is CC0 (public domain):
  - Versilian Community Sample Library (VCSL), https://github.com/sgossner/VCSL
  - sfzinstruments body_percussion, https://github.com/sfzinstruments/body_percussion

Usage:
  build_drum_library.py <VCSL checkout> <body_percussion checkout> <out.bin> <SOURCES.md>

Some objects are recordings as they are (trimmed, mono, levelled); others are
built from several recordings (layered, pitched, choked, saturated, cracked).
Output format (little endian):
  "ABDL" u32 version=1, u32 sampleRate, u32 objectCount
  per object: u16 nameLength, name (UTF-8), u16 sampleCount
  per sample (ordered soft -> loud): u32 frames, int16 frames
"""
import glob
import os
import struct
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, resample_poly, sosfilt

SR = 44100


def load(path):
    sr, x = wavfile.read(path)
    if x.dtype == np.int16:
        x = x.astype(np.float64) / 32768.0
    elif x.dtype == np.int32:
        x = x.astype(np.float64) / 2147483648.0
    else:
        x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if sr != SR:
        x = resample_poly(x, SR, sr)
    return x - np.mean(x[: min(len(x), 256)])


def trim(x, cap, floor_db=-60.0):
    peak = np.max(np.abs(x)) + 1e-12
    above = np.nonzero(np.abs(x) > peak * 10 ** (-45 / 20))[0]
    start = max(0, above[0] - 32) if len(above) else 0
    x = x[start:]
    env = np.abs(x)
    above = np.nonzero(env > peak * 10 ** (floor_db / 20))[0]
    end = above[-1] + 1 if len(above) else len(x)
    x = x[: min(end, int(cap * SR))]
    fade = min(len(x) // 4, int(0.03 * SR))
    if fade > 0:
        x[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    x[:16] *= np.linspace(0.0, 1.0, 16)
    return x


def lowpass(x, f, order=2):
    return sosfilt(butter(order, f, "low", fs=SR, output="sos"), x)


def highpass(x, f, order=2):
    return sosfilt(butter(order, f, "high", fs=SR, output="sos"), x)


def pitch(x, semitones):
    if semitones == 0:
        return x
    ratio = 2 ** (semitones / 12.0)
    # Varispeed like tape: down = longer and darker, up = shorter and brighter.
    up, down = int(round(1000 / ratio)), 1000
    return resample_poly(x, up, down)


def saturate(x, drive):
    if drive <= 1.0:
        return x
    peak = np.max(np.abs(x)) + 1e-12
    y = np.tanh(x / peak * drive) / np.tanh(drive)
    return y * peak


def choke(x, ms):
    n = int(ms * 0.001 * SR)
    if n >= len(x):
        return x
    y = x[: n + int(0.02 * SR)].copy()
    y[n:] *= np.linspace(1.0, 0.0, len(y) - n) ** 2
    return y


def crack(x, amount, rng):
    # A crack makes the two lips of a cymbal slap together: bursts of buzz
    # whose strength follows the cymbal's own loudness.
    env = lowpass(np.abs(x), 30.0, 1)
    gate = (rng.random(len(x)) < 0.35).astype(np.float64)
    gate = lowpass(gate, 900.0, 1)
    buzz = highpass(rng.standard_normal(len(x)), 2500.0) * gate * env * 3.0
    return x * (1.0 + amount * 0.6 * (gate - 0.35)) + amount * buzz


def mix(a, b, gain_db):
    n = max(len(a), len(b))
    y = np.zeros(n)
    y[: len(a)] += a
    y[: len(b)] += b * 10 ** (gain_db / 20)
    return y


def files(root, pattern):
    found = sorted(glob.glob(os.path.join(root, pattern)))
    if not found:
        raise SystemExit("no files for " + pattern)
    return found


def build(vcsl, body):
    I = os.path.join(vcsl, "Idiophones", "Struck Idiophones")
    M = os.path.join(vcsl, "Membranophones", "Struck Membranophones")
    B = os.path.join(body, "Samples", "body")
    rng = np.random.default_rng(7)
    objects = []

    def plain(name, paths, cap, lp=None, hp=None, semis=0, drive=1.0, limit=12):
        out = []
        for p in paths[:limit]:
            x = pitch(load(p), semis)
            if hp:
                x = highpass(x, hp)
            if lp:
                x = lowpass(x, lp)
            out.append(trim(saturate(x, drive), cap))
        objects.append((name, out))

    # 0 Oil drum: concert bass drum + a brake drum an octave down, saturated.
    kicks = files(M, "Bass Drum 2/bassdrum_hit_*.wav")
    brakes = files(I, "Brake Drum/BrakeDrum1_Hammer_*.wav")
    out = []
    for i, k in enumerate(kicks):
        metal = lowpass(pitch(load(brakes[i % len(brakes)]), -12), 2500)
        out.append(trim(saturate(lowpass(mix(load(k), metal, -10), 6000), 1.8), 1.2))
    objects.append(("Tambor de Oleo", out))
    # 1 Bones / claves, darker and lower.
    plain("Ossos", files(I, "Claves/Claves*_Hit*.wav") + files(I, "Claves/Legacy/claves_*.wav"), 0.3, lp=7000, semis=-3)
    # 2 Parade snare: the rope-tension snare with its snares on.
    plain("Caixa de Fanfarra", files(M, "Snare Drum, Rope Tension/Low/RopeSnare_low_sn_*.wav")
          + files(M, "Snare Drum, Rope Tension/Hi/RopeSnare_hi_sn_*.wav"), 1.0, lp=9000)
    plain("Palma", files(B, "handclap_*.wav"), 0.5)
    # 4 Bin lid: clash cymbal choked short, lower, dirty, no air on top.
    lids = files(I, "Clash Cymbals 1/cymbal_crash1_*.wav") + files(I, "Clash Cymbals 2/cymbal_crash2_*.wav")
    out = []
    for p in lids[:12]:
        x = choke(pitch(load(p), -5), 280)
        out.append(trim(saturate(lowpass(highpass(x, 250), 5000), 2.5), 0.5))
    objects.append(("Tampa de Lixo", out))
    plain("Caixote", files(I, "Cajon/Cajon_*.wav"), 0.6, lp=8000)
    plain("Chimbal Velho", files(I, "Hi-Hat Cymbal/HiHat_Close_*.wav") + files(I, "Hi-Hat Cymbal/HiHat_HitC_*.wav"),
          0.35, lp=7000, semis=-2, drive=1.3)
    # 7 Trash can: a tom with a choked, lowered cymbal rattling inside it.
    toms = files(M, "Tom 1/Stick/TomH_HitS_*.wav")
    out = []
    for i, t in enumerate(toms):
        tin = lowpass(highpass(choke(pitch(load(lids[i % len(lids)]), -7), 150), 400), 4000)
        out.append(trim(saturate(mix(load(t), tin, -9), 2.0), 0.8))
    objects.append(("Lata de Lixo", out))
    plain("Pisao", files(B, "stomp_*.wav") + files(B, "halfstomp_*.wav"), 0.5, limit=12)
    plain("Tronco", files(I, "Slit Drum/LogDrum*.wav"), 0.8)
    plain("Matraca", files(I, "Ratchet/Ratchet*.wav"), 1.5)
    plain("Freio de Carro", files(I, "Brake Drum/BrakeDrum*_Hammer*.wav") + files(I, "Brake Drum/BrakeDrum*_YarnM*.wav"),
          1.5, lp=8000)
    plain("Chapa de Metal", [p for p in files(I, "Gong 1/gong_*.wav") if "scrape" not in p], 3.0, lp=6000, semis=5,
          drive=1.4)
    # 13 Cracked cymbal: suspended cymbal hits with the crack buzzing.
    cym = [p for p in files(I, "Suspended Cymbal 1/susCymb1_hit_*.wav") if "bell" not in p]
    cym += [p for p in files(I, "Suspended Cymbal 2/susCymb2_hit_*.wav") if "bell" not in p]
    out = []
    for p in cym[:10]:
        out.append(trim(lowpass(crack(load(p), 0.6, rng), 8000), 2.5))
    objects.append(("Prato Rachado", out))
    plain("Bigorna", files(I, "Anvil/Anvil_*.wav"), 1.2, lp=6000)
    plain("Mola", files(I, "Flexatone/flexatone_slap*.wav") + files(I, "Flexatone/flexatone[12].wav"), 1.2)
    plain("Raspador", files(I, "Guiro/Guiro_Hit*.wav") + files(I, "Guiro/Legacy/guiro_short*.wav") + files(I, "Guiro/Guiro_Fast*.wav"), 0.8)
    plain("Chocalho de Mola", files(I, "Vibraslap/Legacy/vibraslap_*.wav"), 1.2)
    plain("Sino Enferrujado", files(I, "Cowbells/Cowbell*_Muted_*.wav") + files(I, "Cowbells/Cowbell*_Hit_*.wav"),
          0.6, lp=4000, semis=-7, drive=2.0)
    plain("Pandeiro de Lata", files(I, "Tambourine 1/Tamb1_Hit*.wav"), 0.5, lp=8000)
    plain("Tabua Batendo", files(I, "Slapstick/slapstick*.wav"), 0.5, lp=7000)
    plain("Peito", files(B, "chestthump_*.wav") + files(B, "bellycenter_*.wav"), 0.4)
    plain("Estalo", files(B, "snap_*.wav") + files(B, "fingerclap_*.wav"), 0.3)
    plain("Calcanhar", files(B, "heel_*.wav"), 0.3)
    return objects


def main():
    vcsl, body, out_bin, out_md = sys.argv[1:5]
    objects = build(vcsl, body)
    with open(out_bin, "wb") as f:
        f.write(b"ABDL")
        f.write(struct.pack("<III", 1, SR, len(objects)))
        for name, samples in objects:
            # Order by loudness so the engine can map velocity to dynamics.
            samples = sorted(samples, key=lambda s: float(np.sqrt(np.mean(s[: int(0.05 * SR)] ** 2))))
            peak = max(np.max(np.abs(s)) for s in samples) + 1e-12
            encoded = name.encode("utf-8")
            f.write(struct.pack("<H", len(encoded)))
            f.write(encoded)
            f.write(struct.pack("<H", len(samples)))
            for s in samples:
                pcm = np.clip(s / peak * 0.5 * 32767.0, -32768, 32767).astype("<i2")  # loudest at -6 dBFS
                f.write(struct.pack("<I", len(pcm)))
                f.write(pcm.tobytes())
    total = sum(len(s) for _, ss in objects for s in ss)
    with open(out_md, "w", encoding="utf-8") as f:
        f.write("# ABISMO DRUMS sample library: sources\n\n")
        f.write("All recordings are CC0 1.0 (public domain dedication):\n\n")
        f.write("- Versilian Community Sample Library (VCSL), https://github.com/sgossner/VCSL\n")
        f.write("- sfzinstruments body_percussion, https://github.com/sfzinstruments/body_percussion\n\n")
        f.write("Built by `Tools/build_drum_library.py` (trim, mono, level; some objects layered, pitched,\n")
        f.write("choked, saturated or cracked). %d objects, %d samples, %.1f s of audio at %d Hz.\n\n"
                % (len(objects), sum(len(s) for _, s in objects), total / SR, SR))
        for name, samples in objects:
            f.write("- %s: %d samples\n" % (name, len(samples)))
    print("%d objects, %d samples, %.1f s, %.1f MB" % (len(objects), sum(len(s) for _, s in objects), total / SR,
                                                       os.path.getsize(out_bin) / 1e6))


if __name__ == "__main__":
    main()
