#!/usr/bin/env python3
"""Builds Abismo/Library/drums.bin, the ABISMO DRUMS sample library.

Every source recording is CC0 (public domain):
  - Versilian Community Sample Library (VCSL), https://github.com/sgossner/VCSL
  - sfzinstruments body_percussion, https://github.com/sfzinstruments/body_percussion
  - Karoryfer Big Rusty Drums, https://github.com/sfzinstruments/karoryfer.big-rusty-drums

Usage:
  build_drum_library.py <VCSL> <body_percussion> <big-rusty-drums> <out.bin> <SOURCES.md> [wav folder]

With a wav folder, every take is also written there as a WAV file, one folder per object.

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
    if not path.lower().endswith(".wav"):
        import soundfile
        x, sr = soundfile.read(path, dtype="float64", always_2d=False)
        if x.ndim > 1:
            x = x.mean(axis=1)
        if sr != SR:
            x = resample_poly(x, SR, sr)
        return x - np.mean(x[: min(len(x), 256)])
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


def degrade(x, rate, bits):
    """Sample-and-hold at `rate` Hz, quantised to `bits`: a worn converter."""
    step = max(1, int(round(SR / rate)))
    y = np.repeat(x[::step], step)[: len(x)]
    peak = np.max(np.abs(y)) + 1e-12
    q = 2 ** (bits - 1)
    return np.round(y / peak * q) / q * peak


def worn(name, paths, cap, semis=0, lp=None, hp=None, drive=1.0, hold=None, rate=None, bits=8, limit=12, rng_=None):
    """A recording taken down, choked and degraded: the same real sound, ruined."""
    out = []
    for p in paths[:limit]:
        x = pitch(load(p), semis)
        if hold:
            x = choke(x, hold)
        if hp:
            x = highpass(x, hp)
        if rate:
            x = degrade(x, rate, bits)
        if lp:
            x = lowpass(x, lp)
        out.append(trim(saturate(x, drive), cap))
    return (name, out)


def mix(a, b, gain_db):
    n = max(len(a), len(b))
    y = np.zeros(n)
    y[: len(a)] += a
    y[: len(b)] += b * 10 ** (gain_db / 20)
    return y


def spread(paths, n):
    """n takes spread evenly over a list ordered soft -> loud."""
    if len(paths) <= n:
        return list(paths)
    idx = np.round(np.linspace(0, len(paths) - 1, n)).astype(int)
    return [paths[i] for i in idx]


def by_layer(paths):
    """Orders Karoryfer names (..._vl3_rr2) by velocity layer, then round robin."""
    import re

    def key(p):
        m = re.search(r"vl(\d+)_rr(\d+)", os.path.basename(p))
        return (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    return sorted(paths, key=key)


def train(grains, rng, count, gap_ms, decay):
    """A short run of small impacts, like a chain dropped or cutlery shaken."""
    n = count
    pos, out, gain = 0, np.zeros(int(1.0 * SR)), 1.0
    for _ in range(n):
        g = grains[rng.integers(len(grains))]
        if pos + len(g) > len(out):
            break
        out[pos: pos + len(g)] += g * gain * (1.0 if pos == 0 else rng.uniform(0.6, 1.0))
        pos += int(rng.uniform(*gap_ms) * 0.001 * SR)
        gain *= decay
    return out


def files(root, pattern):
    found = sorted(glob.glob(os.path.join(root, pattern)))
    if not found:
        raise SystemExit("no files for " + pattern)
    return found


def build(vcsl, body, rusty):
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
    objects.append(worn("Motor Morto", files(I, "Ratchet/Ratchet*.wav"), 1.8, semis=-24, lp=1800, drive=2.2, hold=1400))
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
    objects.append(worn("Gongo Podre", cym, 2.5, semis=-17, lp=2400, drive=2.0, hold=1500, limit=10))
    plain("Bigorna", files(I, "Anvil/Anvil_*.wav"), 1.2, lp=6000)
    objects.append(worn("Mola Morta", files(I, "Flexatone/flexatone_slap*.wav") + files(I, "Flexatone/flexatone[12].wav"), 1.6, semis=-19, lp=1300, drive=1.8))
    plain("Raspador", files(I, "Guiro/Guiro_Hit*.wav") + files(I, "Guiro/Legacy/guiro_short*.wav") + files(I, "Guiro/Guiro_Fast*.wav"), 0.8)
    objects.append(worn("Engrenagem", files(I, "Vibraslap/Legacy/vibraslap_*.wav"), 1.4, semis=-14, lp=2200, drive=2.6, hold=600, rate=9000, bits=9))
    plain("Sino Enferrujado", files(I, "Cowbells/Cowbell*_Muted_*.wav") + files(I, "Cowbells/Cowbell*_Hit_*.wav"),
          0.6, lp=4000, semis=-7, drive=2.0)
    objects.append(worn("Lata Amassada", files(I, "Tambourine 1/Tamb1_Hit*.wav"), 0.8, semis=-12, lp=2600, drive=2.0, hold=300, rate=11000, bits=8))
    objects.append(worn("Porta de Ferro", [p for p in files(I, "Gong 1/gong_*.wav") if "scrape" not in p], 1.0, semis=-12, lp=1900, drive=2.4, hold=450))
    plain("Peito", files(B, "chestthump_*.wav") + files(B, "bellycenter_*.wav"), 0.4)
    plain("Estalo", files(B, "snap_*.wav") + files(B, "fingerclap_*.wav"), 0.3)
    plain("Calcanhar", files(B, "heel_*.wav"), 0.3)

    # ---- 0.7: a rusty drum kit (Big Rusty Drums, overhead mics) and more junk.
    R = os.path.join(rusty, "Samples")

    def rus(folder, pattern="*"):
        return by_layer(files(R, folder + "/oh/" + pattern))
    plain("Bumbo Enferrujado", spread(rus("kick_24/kick"), 12), 1.0)
    plain("Bumbo Oco", spread(rus("kick_24_nodamp/kick"), 10), 1.4)
    plain("Caixa Enferrujada", spread(rus("snare_14/center"), 12), 0.9)
    plain("Aro", spread(rus("snare_14/rc"), 6) + spread(rus("snare_14/sidestick"), 6), 0.5)
    plain("Casco", spread(rus("snare_14/sc"), 4) + spread(rus("kick_24/sc"), 4) + spread(rus("kick_24/rc"), 4), 0.5)
    plain("Esteira Solta", rus("snare_14/bflutter") + rus("snare_14/brelease"), 1.2)
    objects.append(worn("Pilha Afundada", spread(rus("stack_3_layer/ed"), 6) + spread(rus("stack_3_layer/mid"), 6), 1.2, semis=-12, lp=2600, drive=2.0, hold=700))
    plain("China Velho", spread(rus("china_18/cn"), 8) + spread(rus("china_18/choke"), 2), 2.5, lp=9000)
    objects.append(worn("Prato Derretido", spread(rus("crash_sizzle_17/cr"), 10), 2.5, semis=-19, lp=2200, drive=1.8, rate=8000, bits=9, limit=10))
    plain("Conducao Rebitada", spread(rus("crash_sizzle_17/rd"), 8) + spread(rus("ride_sizzle_19/bl"), 4), 1.8, lp=9000)
    objects.append(worn("Chimbal Afundado", spread(rus("hihat_14/tc"), 6) + spread(rus("hihat_14/chik"), 4)
          + spread(rus("hihat_14/footspl"), 2), 0.7, semis=-10, lp=1700, drive=2.2, hold=400))
    plain("Pedal Rangendo", rus("kick_24/phit") + rus("kick_24/preturn") + rus("hihat_14/phit"), 0.9)
    plain("Surdo Velho", spread(rus("tom_22/center"), 8) + spread(rus("tom_22/mallet"), 4), 1.8)

    # Industrial tank: timpani hits far down, choked by the hand, dirty.
    timp = files(M, "Timpani 1/Hit/Timpani*_Hit_*.wav")
    out = []
    for p in spread(timp, 10):
        x = choke(pitch(load(p), -9), 900)
        out.append(trim(saturate(lowpass(x, 3500), 2.2), 1.2))
    objects.append(("Tanque", out))
    # Iron pipes: tubular bells an octave down, the hum choked, the clang kept.
    tubes = files(I, "Tubular Bells 1/chimes_*.wav") + files(I, "Tubular Bells 2/*.wav")
    out = []
    for p in spread(tubes, 10):
        x = choke(pitch(load(p), -12), 700)
        out.append(trim(saturate(lowpass(x, 5000), 1.6), 0.9))
    objects.append(("Cano de Ferro", out))
    # Cutlery: two to four small metal pieces (triangle, finger cymbal, agogo) knocking together.
    small = files(I, "Triangles/Triangle*_Hit*.wav") + files(I, "Finger Cymbals/*.wav") + files(I, "Agogo Bells/Agogo_High_*.wav")
    grains = [lowpass(highpass(choke(pitch(load(p), s), 140), 600), 6500) for p in small for s in (-9, -5, -2)]
    out = [trim(train(grains, rng, int(rng.integers(3, 7)), (18, 70), 0.8), 0.6) for _ in range(12)]
    objects.append(("Talheres", out))
    # Chain: a short run of jingles and small bells, low and dark, never a long ring.
    jingles = files(I, "Sleigh Bells/*.wav") + files(I, "Tambourine 2/*.wav")
    grains = [lowpass(choke(pitch(load(p), s), 90), 4500) for p in jingles for s in (-14, -10, -7)]
    out = [trim(train(grains, rng, int(rng.integers(8, 16)), (12, 38), 0.9), 0.7) for _ in range(12)]
    objects.append(("Corrente", out))
    plain("Agogo Torto", files(I, "Agogo Bells/Agogo_*.wav"), 0.6, lp=5000, semis=-4, drive=1.8)
    plain("Bilha de Barro", spread(files(M, "Darbuka/Darbuka_*_hit_*.wav"), 12), 0.6, lp=8000, semis=-3)
    objects.append(worn("Cascalho", files(I, "Shaker, Small/*.wav") + files(I, "Cabasa/Cabasa1_Hit*.wav"), 0.9, semis=-14, lp=2300, drive=1.6, rate=12000, bits=10))
    # Sand: a handful thrown on a drum head, cut from the ocean drum's loudest moments.
    out = []
    for p in files(M, "../Other Membranophones/Ocean Drum/*.wav"):
        x = lowpass(load(p), 6000)
        env = lowpass(np.abs(x), 8.0, 1)
        taken = []
        for c in np.argsort(env[:: SR // 10])[::-1]:
            start = int(c) * (SR // 10)
            seg = x[start: start + int(0.7 * SR)].copy()
            if len(seg) < int(0.3 * SR) or any(abs(start - o) < int(0.8 * SR) for o in taken):
                continue
            taken.append(start)
            t = np.arange(len(seg)) / SR
            seg *= np.minimum(1.0, t / 0.008) * np.exp(-t / 0.18)
            out.append(trim(seg, 0.7))
            if len(taken) == 4:
                break
    objects.append(("Areia", out))
    plain("Vidro Gemendo", files(os.path.join(vcsl, "Idiophones", "Friction Idiophones"), "Wine Glasses/Sustains/*/*.wav")[:10],
          2.5, lp=3500, semis=-12)
    plain("Gongo Raspado", [p for p in files(I, "Gong 1/*.wav") if "scrape" in p] + files(I, "Gong 2/scrape*.mp3"),
          3.0, lp=6000, semis=-5)
    plain("Sino de Mao", files(I, "Hand Bells, Nepalese/*.wav"), 1.5, lp=6000, semis=-5, drive=1.3)
    return objects


def main():
    vcsl, body, rusty, out_bin, out_md = sys.argv[1:6]
    wav_dir = sys.argv[6] if len(sys.argv) > 6 else None
    objects = build(vcsl, body, rusty)
    with open(out_bin, "wb") as f:
        f.write(b"ABDL")
        f.write(struct.pack("<III", 1, SR, len(objects)))
        for index, (name, samples) in enumerate(objects):
            # Order by loudness so the engine can map velocity to dynamics.
            samples = sorted(samples, key=lambda s: float(np.sqrt(np.mean(s[: int(0.05 * SR)] ** 2))))
            peak = max(np.max(np.abs(s)) for s in samples) + 1e-12
            encoded = name.encode("utf-8")
            f.write(struct.pack("<H", len(encoded)))
            f.write(encoded)
            f.write(struct.pack("<H", len(samples)))
            for k, s in enumerate(samples):
                pcm = np.clip(s / peak * 0.5 * 32767.0, -32768, 32767).astype("<i2")  # loudest at -6 dBFS
                f.write(struct.pack("<I", len(pcm)))
                f.write(pcm.tobytes())
                if wav_dir:
                    # Takes numbered soft -> loud; the loudest of each object peaks at -0.2 dBFS.
                    folder = os.path.join(wav_dir, "%02d %s" % (index + 1, name))
                    os.makedirs(folder, exist_ok=True)
                    wavfile.write(os.path.join(folder, "%s %02d.wav" % (name, k + 1)), SR,
                                  np.clip(s / peak * 0.977 * 32767.0, -32768, 32767).astype(np.int16))
    total = sum(len(s) for _, ss in objects for s in ss)
    with open(out_md, "w", encoding="utf-8") as f:
        f.write("# ABISMO DRUMS sample library: sources\n\n")
        f.write("All recordings are CC0 1.0 (public domain dedication):\n\n")
        f.write("- Versilian Community Sample Library (VCSL), https://github.com/sgossner/VCSL\n")
        f.write("- sfzinstruments body_percussion, https://github.com/sfzinstruments/body_percussion\n")
        f.write("- Karoryfer Big Rusty Drums, https://github.com/sfzinstruments/karoryfer.big-rusty-drums\n\n")
        f.write("Built by `Tools/build_drum_library.py` (trim, mono, level; some objects layered, pitched,\n")
        f.write("choked, saturated or cracked). %d objects, %d samples, %.1f s of audio at %d Hz.\n\n"
                % (len(objects), sum(len(s) for _, s in objects), total / SR, SR))
        for name, samples in objects:
            f.write("- %s: %d samples\n" % (name, len(samples)))
    print("%d objects, %d samples, %.1f s, %.1f MB" % (len(objects), sum(len(s) for _, s in objects), total / SR,
                                                       os.path.getsize(out_bin) / 1e6))


if __name__ == "__main__":
    main()
