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
    # Many close-mic recordings carry rumble and offset below 25 Hz: inaudible, but it eats the peak headroom and
    # the level of everything that can be heard, so it goes.
    x = highpass(x, 28.0, 4)
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
        if os.environ.get("DRUMLIB_LIST_MISSING"):
            print("MISSING", root, pattern)
            return []
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
    assert len(objects) == 48, len(objects)   # ENKI and saved sessions rely on these 48 staying as they are
    objects += more_objects(vcsl, body, rusty)
    return objects



def one_note(paths, which=0):
    """The files of the lowest pitch (and the next ones, if it has too few takes)."""
    import re
    groups = {}
    for p in paths:
        m = re.search(r"(?:^|_)([A-G]#?)(\d)_", os.path.basename(p))
        if m:
            groups.setdefault((int(m.group(2)), "CDEFGAB".index(m.group(1)[0]), len(m.group(1))), []).append(p)
    keys = sorted(groups)[which:]
    taken = []
    for k in keys:   # the lowest pitch, plus the next ones until there are enough takes
        taken += groups[k]
        if len(taken) >= 4:
            break
    return taken


# Stand-in synthesised model for each category, used only if the library file is missing.
STAND_IN = {"Kick": 0, "Snare": 2, "Tom": 11, "Hat": 6, "Cymbal": 13, "Perc": 5, "Metal": 14, "Wood": 5, "Body": 15,
            "Tonal": 15, "Ruin": 12}


def more_objects(vcsl, body, rusty):
    """Objects 48 and on of the library: each is a different real source or articulation."""
    I = os.path.join(vcsl, "Idiophones", "Struck Idiophones")
    PI = os.path.join(vcsl, "Idiophones", "Plucked Idiophones")
    M = os.path.join(vcsl, "Membranophones", "Struck Membranophones")
    CH = os.path.join(vcsl, "Chordophones")
    B = os.path.join(body, "Samples", "body")
    R = os.path.join(rusty, "Samples")
    out = []

    def rus(art, mic):
        return by_layer(files(R, "%s/%s/*" % (art, mic)))

    def add(cat, name, paths, cap, n=5, **kw):
        kw.setdefault("limit", n)
        label, takes = worn("%s: %s" % (cat, name), spread(paths, n), max(0.3, cap * 0.55), **kw)   # tails kept short: the library ships inside the plugin
        if not os.environ.get("DRUMLIB_LIST_MISSING"):
            assert len(takes) >= 2, label
        out.append((label, takes))

    # ---- KICK: the rusty kit's mics and pedal, concert and legacy bass drums, and low drums pitched down.
    add("Kick", "Beater Close", rus("kick_24/kick", "kick"), 0.9, 6)
    add("Kick", "Room Bleed", rus("kick_24/kick", "sn"), 0.7, 6)
    add("Kick", "Open Close-Mic", rus("kick_24_nodamp/kick", "kick"), 1.3, 5)
    add("Kick", "Shell Knock", rus("kick_24/sc", "cl"), 0.5, 5)
    add("Kick", "Hoop Click", rus("kick_24/rc", "cl"), 0.4, 5)
    add("Kick", "Pedal Thump", rus("kick_24/phit", "cl") + rus("kick_24/preturn", "cl"), 0.6, 6)
    add("Kick", "Concert Bass Drum", files(M, "Bass Drum 1/*.wav"), 1.6, 6, lp=6000)
    add("Kick", "Legacy Bass Drum", [p for p in files(M, "Bass Drum 3 - Legacy/bdrum*.wav") if "muted" not in p], 1.5, 6, lp=6000)
    add("Kick", "Muted Bass Drum", files(M, "Bass Drum 3 - Legacy/bdrum_muted_*.wav"), 0.9, 6, lp=6000)
    add("Kick", "Timpani Thud", files(M, "Timpani 1/Hit/Timpani*_Hit_*.wav"), 0.9, 6, semis=-14, lp=2500, drive=1.5, hold=350)
    add("Kick", "Frame Drum Boom", files(M, "Frame Drum/HDrumL_Hit_*.wav"), 1.0, 4, semis=-9, lp=3000)
    add("Kick", "Tumba Boom", files(M, "Conga/Tumba_HitN_*.wav"), 0.9, 6, semis=-14, lp=2500)
    add("Kick", "Floor Tom Thud", files(M, "Legacy Toms/tenor_lower/*.wav"), 0.8, 6, semis=-9, lp=2500, hold=500)
    add("Kick", "Cajon Sub", files(I, "Cajon/Cajon_*.wav"), 0.7, 6, semis=-10, lp=1800, drive=1.6)
    add("Kick", "Bitten Bass Drum", files(M, "Bass Drum 3 - Legacy/bdrum_*.wav"), 0.9, 6, lp=4000, drive=2.0, rate=9000, bits=8)

    # ---- SNARE: one wooden snare in many ways, then four other snares.
    add("Snare", "Top Mic", rus("snare_14/center", "top"), 0.8, 7)
    add("Snare", "Wires Mic", rus("snare_14/center", "btm"), 0.9, 6)
    add("Snare", "Edge", rus("snare_14/edge", "top"), 0.8, 6)
    add("Snare", "Rimshot", rus("snare_14/rimshot", "top"), 0.8, 6)
    add("Snare", "Rimshot Wires", rus("snare_14/rimshot", "btm"), 0.8, 6)
    add("Snare", "Side Stick", rus("snare_14/sidestick", "top"), 0.5, 6)
    add("Snare", "Brush", rus("snare_14/brush", "top"), 0.8, 5)
    add("Snare", "Mallet", rus("snare_14/mallet", "top"), 0.9, 5)
    add("Snare", "Tom On Top", rus("snare_14/pcenter", "top"), 0.9, 6)
    add("Snare", "Tom On Top Rimshot", rus("snare_14/primshot", "top"), 0.9, 6)
    add("Snare", "Tambourine On Top", rus("snare_14/tcenter", "top"), 0.9, 6)
    add("Snare", "Tambourine Rimshot", rus("snare_14/trimshot", "top"), 0.9, 6)
    add("Snare", "Brush Dig", rus("snare_14/bdig", "top"), 0.7, 5)
    add("Snare", "Brush Release", rus("snare_14/brelease", "btm"), 0.8, 5)
    add("Snare", "Old Snare", files(M, "Legacy Snares/OldSnare/snare_*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Legacy Drum One", files(M, "Legacy Snares/drum1/*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Legacy Drum Two", files(M, "Legacy Snares/drum2/*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Marching Legacy", files(M, "Legacy Snares/drum3_marching/*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Modern One", files(M, "Snare Drum, Modern 1/Snare*_Hit*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Modern Two", files(M, "Snare Drum, Modern 2/Snare*_Hit*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Modern Three", files(M, "Snare Drum, Modern 3/Snare*_HitSN*.wav"), 0.9, 7, lp=9000)
    add("Snare", "Modern Rimshot", files(M, "Snare Drum, Modern 3/Snare*_rimshot*.wav"), 0.8, 4, lp=9000)
    add("Snare", "Cross Stick", files(M, "Snare Drum, Modern 2/Snare*_Xstick*.wav") + files(M, "Snare Drum, Modern 3/Snare*_Xstick*.wav"), 0.5, 4)
    add("Snare", "Stick Taps", files(M, "Snare Drum, Modern 1/Snare*_taps*.wav") + files(M, "Snare Drum, Modern 2/Snare*_taps*.wav"), 0.5, 4)

    # ---- TOM: four rusty toms, two modern toms, legacy tenor toms.
    add("Tom", "Fourteen", rus("tom_14/center", "cl"), 1.0, 6)
    add("Tom", "Fourteen Mallet", rus("tom_14/mallet", "cl"), 1.0, 5)
    add("Tom", "Fifteen", rus("tom_15/center", "cl"), 1.1, 6)
    add("Tom", "Fifteen Brush", rus("tom_15/brush", "cl"), 1.0, 5)
    add("Tom", "Eighteen", rus("tom_18/center", "cl"), 1.4, 6)
    add("Tom", "Eighteen Mallet", rus("tom_18/mallet", "cl"), 1.4, 5)
    add("Tom", "Twenty-Two", rus("tom_22/center", "cl"), 1.6, 6)
    add("Tom", "Twenty-Two Brush", rus("tom_22/brush", "cl"), 1.4, 5)
    add("Tom", "Concert High", files(M, "Tom 1/Stick/TomH_HitS_*.wav"), 1.0, 6)
    add("Tom", "Concert High Mallet", files(M, "Tom 1/Mallet/TomH_HitM_*.wav"), 1.1, 6)
    add("Tom", "Concert Low", files(M, "Tom 2/Stick/TomL_HitS_*.wav"), 1.2, 6)
    add("Tom", "Concert Low Mallet", files(M, "Tom 2/Mallet/TomL_HitM_*.wav"), 1.3, 6)
    add("Tom", "Concert Rim", files(M, "Tom 1/TomH_rimS_*.wav") + files(M, "Tom 2/TomL_rimS_*.wav"), 0.6, 5)
    add("Tom", "Tenor High", files(M, "Legacy Toms/tenor_higher/*.wav"), 1.0, 7, lp=9000)
    add("Tom", "Tenor Low", files(M, "Legacy Toms/tenor_lower/*.wav"), 1.1, 7, lp=9000)
    add("Tom", "Timpani", files(M, "Timpani 1/Hit/Timpani*_Hit_*.wav"), 1.5, 6, semis=-3, lp=7000)

    # ---- HAT: the rusty hi-hat by articulation, kept dark.
    add("Hat", "Closed", rus("hihat_14/cl", "cl"), 0.35, 6, lp=9000)
    add("Hat", "Tight", rus("hihat_14/tc", "cl"), 0.3, 6, lp=9000)
    add("Hat", "Loose", rus("hihat_14/lc", "cl"), 0.5, 6, lp=9000)
    add("Hat", "Quarter Open", rus("hihat_14/qo", "cl"), 0.8, 6, lp=9000)
    add("Hat", "Half Open", rus("hihat_14/ho", "cl"), 1.0, 6, lp=9000)
    add("Hat", "Open", rus("hihat_14/open", "cl"), 1.6, 5, lp=9000)
    add("Hat", "Foot Chick", rus("hihat_14/chik", "cl"), 0.25, 5)
    add("Hat", "Foot Splash", rus("hihat_14/footspl", "cl"), 0.8, 5, lp=9000)
    add("Hat", "Brush Closed", rus("hihat_14/cl_brush", "cl"), 0.4, 5, lp=9000)
    add("Hat", "Brush Open", rus("hihat_14/open_brush", "cl"), 1.0, 5, lp=9000)
    add("Hat", "Pedal Noise", rus("hihat_14/perc", "cl") + rus("hihat_14/return", "cl"), 0.3, 5)

    # ---- CYMBAL: few, soft and dark.
    add("Cymbal", "Crash Mallet", rus("crash_17/mallet", "oh"), 2.2, 5, lp=8000)
    add("Cymbal", "Crash Brush", rus("crash_17/brush", "oh"), 2.0, 4, lp=8000)
    add("Cymbal", "Crash Choke", rus("crash_17/choke", "oh"), 0.7, 5, lp=8000)
    add("Cymbal", "Ride Mallet", rus("ride_22/mallet", "oh"), 2.2, 5, lp=8000)
    add("Cymbal", "Ride Brush", rus("ride_22/brush", "oh"), 2.0, 4, lp=8000)
    add("Cymbal", "Ride Bell", rus("ride_22/bl", "oh"), 1.8, 5, lp=6500)
    add("Cymbal", "Ride Choke", rus("ride_22/choke", "oh"), 0.7, 5, lp=8000)
    add("Cymbal", "Clash Dull", files(I, "Clash Cymbals 2/cymbal_crash2_*.wav"), 1.6, 5, semis=-3, lp=5000)

    # ---- PERC: hand drums and small instruments.
    add("Perc", "Bongo High", files(M, "Bongos/BongoH_Hit[0-9]*.wav"), 0.5, 6)
    add("Perc", "Bongo High Muted", files(M, "Bongos/BongoH_HitMuted*.wav"), 0.3, 5)
    add("Perc", "Bongo Low", files(M, "Bongos/BongoL_Hit[0-9]*.wav"), 0.6, 6)
    add("Perc", "Bongo Low Muted", files(M, "Bongos/BongoL_HitMuted*.wav"), 0.3, 5)
    add("Perc", "Conga Open", files(M, "Conga/Conga_HitN_*.wav"), 0.9, 6)
    add("Perc", "Conga Slap", files(M, "Conga/Conga_HitFM*.wav"), 0.5, 5)
    add("Perc", "Quinto", files(M, "Conga/Quinto_HitN_*.wav"), 0.7, 6)
    add("Perc", "Quinto Slap", files(M, "Conga/Quinto_HitFM*.wav"), 0.5, 5)
    add("Perc", "Tumba", files(M, "Conga/Tumba_HitN_*.wav"), 1.0, 6)
    add("Perc", "Tumba Slap", files(M, "Conga/Tumba_HitFM*.wav"), 0.6, 5)
    add("Perc", "Frame Drum Large", files(M, "Frame Drum/HDrumL_Hit_*.wav"), 1.0, 4)
    add("Perc", "Frame Drum Muted", files(M, "Frame Drum/HDrumL_HitMuted*.wav") + files(M, "Frame Drum/HDrumS_HitMuted*.wav"), 0.4, 5)
    add("Perc", "Frame Drum Small", files(M, "Frame Drum/HDrumS_Hit_*.wav"), 0.7, 4)
    add("Perc", "Hand Clap Group", files(I, "Claps/Clap_rr*.wav"), 0.8, 5)
    add("Perc", "Hand Clap Solo", files(I, "Claps/SoloClap_vl*.wav"), 0.5, 4)
    add("Perc", "Darbuka One", files(M, "Darbuka/Darbuka_1_hit_*.wav") + files(M, "Darbuka/Darbuka_2_hit_*.wav"), 0.6, 4)
    add("Perc", "Darbuka Two", files(M, "Darbuka/Darbuka_4_hit_*.wav") + files(M, "Darbuka/Darbuka_5_hit_*.wav"), 0.6, 4)
    add("Perc", "Cowbell", files(I, "Cowbells/Cowbell*_Normal_*.wav"), 0.8, 6, lp=6500)
    add("Perc", "Cowbell Muted", files(I, "Cowbells/Cowbell*_Muted_*.wav"), 0.3, 6, lp=6500)
    add("Perc", "Large Shaker", files(I, "Shaker, Large/LShaker_Hit*.wav") + files(I, "Shaker, Large/LShaker_Shake*.wav"), 0.9, 6, lp=8000)
    add("Perc", "Small Shaker", files(I, "Shaker, Small/*.wav"), 0.8, 6, lp=8000)
    add("Perc", "Tambourine Down", files(I, "Tambourine [345] - Legacy/tambourine_down*.wav"), 0.9, 6, lp=8000)
    add("Perc", "Tambourine Up", files(I, "Tambourine [345] - Legacy/tambourine_up*.wav"), 0.9, 6, lp=8000)
    add("Perc", "Cabasa", files(I, "Cabasa/Cabasa*_Hit*.wav"), 0.5, 4, lp=8000)

    # ---- WOOD
    add("Wood", "Woodblock", files(I, "Woodblock/wood_click*.wav"), 0.4, 6, lp=8000)
    add("Wood", "Claves", files(I, "Claves/Claves*_Hit_*.wav"), 0.4, 6, lp=8000)
    add("Wood", "Slapstick", files(I, "Slapstick/*.wav"), 0.5, 5, lp=9000)
    add("Wood", "Cajon Slap", files(I, "Cajon/Cajon_hit*_fff_*.wav") + files(I, "Cajon/Cajon_hit*_f_*.wav"), 0.5, 5, lp=9000)
    add("Wood", "Log Drum Low", files(I, "Slit Drum/LogDrum*.wav"), 0.9, 6, semis=-7, lp=5000)

    # ---- METAL
    add("Metal", "Tubular Clang", files(I, "Tubular Bells 1/chimes_*.wav") + files(I, "Tubular Bells 2/*.wav"), 0.7, 6, semis=-5, hold=500, lp=6000)
    add("Metal", "Bell Tree Fall", files(I, "Bell Tree/Stroke/*.wav"), 1.0, 4, semis=-12, hold=700, lp=5000)
    add("Metal", "Dull Anvil", files(I, "Anvil/Anvil_*.wav"), 0.6, 6, semis=-5, hold=300, lp=3500, drive=1.5)
    add("Metal", "Brake Drum Low", files(I, "Brake Drum/BrakeDrum*_Hammer*.wav"), 1.0, 6, semis=-7, lp=4500)
    add("Metal", "Gong Low", [p for p in files(I, "Gong 1/gong_*.wav") if "scrape" not in p], 2.0, 4, semis=-9, hold=1200, lp=3500)
    add("Metal", "Clash Choked", files(I, "Clash Cymbals 1/cymbal_crash1_*.wav"), 0.4, 6, hold=200, lp=5000, semis=-2)

    # ---- BODY (body_percussion has more than the earlier objects used)
    add("Body", "Cheek Pop", files(B, "cheek_*.wav"), 0.3, 6)
    add("Body", "Head Knock", files(B, "head_*.wav"), 0.4, 6)
    add("Body", "Belly Slap", files(B, "bellyside_*.wav"), 0.4, 6)
    add("Body", "Shoulder", files(B, "shoulderout_*.wav"), 0.4, 6)
    add("Body", "Chest Slap", files(B, "chestside_*.wav"), 0.4, 6)
    add("Body", "Pinky Snap", files(B, "pinkysnap_*.wav"), 0.3, 6)

    # ---- TONAL: plucked and struck pitched sources, one pitch each, played as hits.
    add("Tonal", "Kalimba Kenya", one_note(files(PI, "Kalimba, Kenya/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Kalimba Tanzania", one_note(files(PI, "Kalimba, Tanzania/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Mbira Gandanga", one_note(files(PI, "Mbira Mavembe*/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Mbira Nyamaropa", one_note(files(PI, "Mbira dzaVadzimu*/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Nyunga Nyunga", one_note(files(PI, "Nyunga Nyunga*/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Balafon Soft", one_note(files(I, "Balafon/Soft Mallet/*.wav")), 1.0, 5, semis=-12, lp=5000)
    add("Tonal", "Balafon Hard", one_note(files(I, "Balafon/Hard Mallet/*.wav")), 1.0, 5, semis=-12, lp=5000)
    add("Tonal", "Marimba", one_note(files(I, "Marimba/*.wav")), 1.2, 5, semis=-12, hold=800, lp=5000)
    add("Tonal", "Xylophone", one_note(files(I, "Xylophone/Medium Mallets/*.wav")), 0.8, 5, semis=-12, lp=5000)
    add("Tonal", "Vibraphone", one_note(files(I, "Vibraphone/Hard Mallets/*.wav")), 1.5, 5, semis=-12, hold=800, lp=5000)
    add("Tonal", "Hand Chimes", one_note(files(I, "Hand Chimes/*.wav")), 1.5, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Harp Pluck", one_note(files(CH, "Composite Chordophones/Folk Harp/*.wav")), 1.2, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Dan Tranh Pluck", one_note(files(CH, "Zithers/Dan Tranh/Normal/*.wav")), 1.2, 5, semis=-12, hold=900, lp=5000)
    add("Tonal", "Harpsichord Pluck", one_note(files(CH, "Zithers/Harpsichord, English/Sustains/Normal/*.wav")), 1.0, 5, semis=-12, hold=700, lp=5000)

    # ---- RUIN: real recordings taken down, cut and crushed (the new sources, not the earlier ones).
    add("Ruin", "Cardboard Snare", files(M, "Snare Drum, Modern 1/Snare*_Hit*.wav"), 0.4, 6, semis=-2, lp=3500, rate=9000, bits=6, drive=1.5, hold=220)
    add("Ruin", "Tin Bongo", files(M, "Bongos/BongoH_Hit[0-9]*.wav"), 0.4, 6, semis=-5, hp=400, rate=8000, bits=7)
    add("Ruin", "Drowned Tom", files(M, "Tom 1/Stick/TomH_HitS_*.wav"), 0.9, 6, semis=-7, lp=700, hold=600)
    add("Ruin", "Melted Conga", files(M, "Conga/Conga_HitN_*.wav"), 0.8, 6, semis=-10, rate=7000, bits=6, drive=2.5)
    add("Ruin", "Radio Snare", files(M, "Legacy Snares/drum1/*.wav"), 0.5, 6, hp=700, lp=3000, rate=6000, bits=5, drive=3.0)
    add("Ruin", "Gutted Kick", rus("kick_24/kick", "oh"), 0.8, 6, semis=-5, rate=5500, bits=6, drive=3.0, hold=450)
    add("Ruin", "Wet Clap", files(I, "Claps/Clap_rr*.wav"), 0.4, 5, semis=-4, lp=1500, hold=120, drive=2.0)
    add("Ruin", "Broken Cowbell", files(I, "Cowbells/Cowbell*_Normal_*.wav"), 0.7, 6, semis=-15, rate=7000, bits=5, lp=3000)
    add("Ruin", "Slowed Kalimba", one_note(files(PI, "Kalimba, Kenya/*.wav")), 0.9, 5, semis=-19, hold=350, lp=1200, drive=1.5)
    add("Ruin", "Bitten Hat", rus("hihat_14/tc", "cl"), 0.3, 6, semis=-4, rate=5000, bits=4, hold=150, lp=3500)
    add("Ruin", "Choked Timpani", files(M, "Timpani 1/Hit/Timpani*_Hit_*.wav"), 0.6, 6, semis=-4, hold=200, drive=2.0)
    add("Ruin", "Crushed Rimshot", rus("snare_14/rimshot", "top"), 0.5, 6, rate=7000, bits=5, lp=4500)
    add("Ruin", "Splintered Wood", files(I, "Claves/Claves*_Hit_*.wav") + files(I, "Woodblock/wood_click*.wav"), 0.4, 6, semis=-10, lp=2500, drive=2.0)
    add("Ruin", "Sunk Tambourine", files(I, "Tambourine [345] - Legacy/tambourine_*.wav"), 0.5, 6, semis=-14, hold=250, lp=2500, drive=1.8, rate=9000, bits=7)
    add("Ruin", "Dead Frame Drum", files(M, "Frame Drum/HDrumL_Hit_*.wav"), 0.5, 4, semis=-6, lp=1400, rate=8000, bits=6, drive=2.2)
    add("Ruin", "Rotten Harp", one_note(files(CH, "Composite Chordophones/Folk Harp/*.wav")), 0.7, 5, semis=-24, hold=250, lp=1500, drive=2.5, rate=9000, bits=6)
    add("Ruin", "Scorched Floor Tom", rus("tom_22/center", "cl"), 0.7, 6, semis=-3, hold=350, rate=10000, bits=7, drive=2.8)
    add("Ruin", "Dust Brush", rus("snare_14/bstir", "top"), 1.0, 4, hp=500, lp=3000, rate=6000, bits=6, drive=2.0)
    return out


# ---- Kits: 16 pads in a General-MIDI-like layout (kick, rim, snare, clap, alt snare, low tom, closed hat, mid tom,
# pedal hat, high tom, open hat, tom/perc, crash, perc, ride, perc), each kit built from its own sources.
# Items are "Category: Name" of the new objects, or the name of one of the first 48 objects.
KITS = [
    ("Rusty Close Mics",
     ["Kick: Beater Close", "Snare: Side Stick", "Snare: Top Mic", "Perc: Hand Clap Solo", "Snare: Edge", "Tom: Eighteen",
      "Hat: Closed", "Tom: Fifteen", "Hat: Foot Chick", "Tom: Fourteen", "Hat: Quarter Open", "Tom: Twenty-Two",
      "Cymbal: Crash Mallet", "Perc: Cowbell Muted", "Cymbal: Ride Brush", "Wood: Claves"],
     {"Room": (22, 40)}),
    ("Brush Session",
     ["Kick: Muted Bass Drum", "Snare: Brush Dig", "Snare: Brush", "Body: Cheek Pop", "Snare: Brush Release", "Tom: Twenty-Two Brush",
      "Hat: Brush Closed", "Tom: Fifteen Brush", "Hat: Foot Chick", "Tom: Eighteen Mallet", "Hat: Brush Open", "Perc: Frame Drum Muted",
      "Cymbal: Crash Brush", "Perc: Small Shaker", "Cymbal: Ride Brush", "Wood: Woodblock"],
     {"Room": (30, 50), "Stumble": 20}),
    ("Concert Hall",
     ["Kick: Concert Bass Drum", "Tom: Concert Rim", "Snare: Old Snare", "Perc: Hand Clap Group", "Snare: Marching Legacy", "Tom: Timpani",
      "Perc: Cowbell Muted", "Tom: Concert Low Mallet", "Metal: Dull Anvil", "Tom: Concert High Mallet", "Metal: Bell Tree Fall", "Tom: Concert Low",
      "Cymbal: Clash Dull", "Metal: Tubular Clang", "Metal: Gong Low", "Perc: Tambourine Down"],
     {"Room": (50, 90), "Decay": 3.2}),
    ("Conga Circle",
     ["Kick: Tumba Boom", "Perc: Conga Slap", "Perc: Conga Open", "Body: Pinky Snap", "Perc: Quinto", "Perc: Tumba",
      "Perc: Bongo Low", "Perc: Quinto Slap", "Perc: Bongo Low Muted", "Perc: Bongo High", "Perc: Bongo High Muted", "Perc: Tumba Slap",
      "Perc: Large Shaker", "Perc: Cowbell", "Perc: Tambourine Up", "Perc: Darbuka One"],
     {"Room": (18, 35)}),
    ("Kalimba Keys", ["Tonal: Kalimba Kenya"] * 16, {"Scale": 1, "Room": (35, 60)}),
    ("Balafon Keys", ["Tonal: Balafon Soft"] * 16, {"Scale": 2, "Room": (25, 45)}),
    ("Harp Keys", ["Tonal: Harp Pluck"] * 16, {"Scale": 3, "Room": (45, 80), "Decay": 2.4}),
    ("Body Beat",
     ["Body: Head Knock", "Body: Pinky Snap", "Body: Chest Slap", "Perc: Hand Clap Solo", "Body: Belly Slap", "Stomp",
      "Body: Cheek Pop", "Heel", "Snap", "Body: Shoulder", "Perc: Hand Clap Group", "Chest",
      "Clap", "Body: Cheek Pop", "Stomp", "Snap"],
     {"Room": (15, 30)}),
    ("Wood Shop",
     ["Wood: Log Drum Low", "Wood: Slapstick", "Wood: Cajon Slap", "Wood: Woodblock", "Kick: Cajon Sub", "Log",
      "Wood: Claves", "Crate", "Wood: Woodblock", "Wood: Log Drum Low", "Wood: Claves", "Kick: Cajon Sub",
      "Wood: Slapstick", "Wood: Cajon Slap", "Ratchet", "Wood: Claves"],
     {"Room": (20, 35)}),
    ("Ruin Machine",
     ["Ruin: Gutted Kick", "Ruin: Crushed Rimshot", "Ruin: Radio Snare", "Ruin: Wet Clap", "Ruin: Cardboard Snare", "Ruin: Scorched Floor Tom",
      "Ruin: Bitten Hat", "Ruin: Drowned Tom", "Ruin: Splintered Wood", "Ruin: Dead Frame Drum", "Ruin: Bitten Hat", "Ruin: Melted Conga",
      "Ruin: Sunk Tambourine", "Ruin: Broken Cowbell", "Ruin: Dust Brush", "Ruin: Tin Bongo"],
     {"Crush": 30, "Room": (15, 30)}),
    ("Slowed Tape",
     ["Ruin: Choked Timpani", "Ruin: Slowed Kalimba", "Ruin: Radio Snare", "Ruin: Wet Clap", "Ruin: Rotten Harp", "Ruin: Drowned Tom",
      "Ruin: Bitten Hat", "Ruin: Scorched Floor Tom", "Ruin: Bitten Hat", "Ruin: Melted Conga", "Ruin: Splintered Wood", "Ruin: Dead Frame Drum",
      "Ruin: Sunk Tambourine", "Ruin: Slowed Kalimba", "Ruin: Dust Brush", "Ruin: Rotten Harp"],
     {"Gramophone": 55, "Room": (30, 60), "Stumble": 30}),
    ("Legacy Drums",
     ["Kick: Legacy Bass Drum", "Snare: Cross Stick", "Snare: Legacy Drum One", "Perc: Hand Clap Group", "Snare: Legacy Drum Two", "Tom: Tenor Low",
      "Hat: Tight", "Tom: Tenor High", "Hat: Foot Chick", "Tom: Tenor High", "Hat: Half Open", "Tom: Tenor Low",
      "Cymbal: Crash Choke", "Perc: Tambourine Down", "Cymbal: Ride Bell", "Perc: Cowbell"],
     {"Room": (28, 50)}),
    ("Snare Garden",
     ["Snare: Top Mic", "Snare: Side Stick", "Snare: Rimshot", "Snare: Wires Mic", "Snare: Edge", "Snare: Mallet",
      "Snare: Modern One", "Snare: Modern Two", "Snare: Modern Three", "Snare: Tom On Top", "Snare: Tambourine On Top", "Snare: Modern Rimshot",
      "Snare: Rimshot Wires", "Snare: Stick Taps", "Snare: Tambourine Rimshot", "Snare: Tom On Top Rimshot"],
     {"Room": (25, 45)}),
    ("Tom Garden",
     ["Tom: Twenty-Two", "Tom: Concert Rim", "Tom: Eighteen", "Tom: Fifteen", "Tom: Fourteen", "Tom: Twenty-Two Brush",
      "Tom: Eighteen Mallet", "Tom: Fifteen Brush", "Tom: Concert Low", "Tom: Concert High", "Tom: Fourteen Mallet", "Tom: Tenor Low",
      "Tom: Tenor High", "Tom: Timpani", "Tom: Concert Low Mallet", "Tom: Concert High Mallet"],
     {"Room": (30, 55)}),
    ("Hat Lab",
     ["Hat: Pedal Noise", "Hat: Foot Chick", "Hat: Closed", "Hat: Tight", "Hat: Loose", "Hat: Foot Splash",
      "Hat: Quarter Open", "Hat: Half Open", "Hat: Open", "Hat: Brush Closed", "Hat: Brush Open", "Ruin: Bitten Hat",
      "Hat: Tight", "Hat: Loose", "Hat: Closed", "Hat: Quarter Open"],
     {"Room": (15, 30)}),
    ("Kick Lab",
     ["Kick: Beater Close", "Kick: Room Bleed", "Kick: Open Close-Mic", "Kick: Shell Knock", "Kick: Hoop Click", "Kick: Pedal Thump",
      "Kick: Concert Bass Drum", "Kick: Legacy Bass Drum", "Kick: Muted Bass Drum", "Kick: Timpani Thud", "Kick: Frame Drum Boom", "Kick: Tumba Boom",
      "Kick: Floor Tom Thud", "Kick: Cajon Sub", "Kick: Bitten Bass Drum", "Ruin: Gutted Kick"],
     {"Room": (20, 40)}),
    ("Metal Shop",
     ["Metal: Brake Drum Low", "Metal: Dull Anvil", "Metal: Tubular Clang", "Metal: Clash Choked", "Metal: Gong Low", "Brake Drum",
      "Anvil", "Metal: Bell Tree Fall", "Trash Can Lid", "Metal: Dull Anvil", "Metal Sheet", "Metal: Brake Drum Low",
      "Metal: Gong Low", "Metal: Tubular Clang", "Metal: Clash Choked", "Ruin: Broken Cowbell"],
     {"Room": (35, 70), "StutterDiv": 3, "StutterDepth": 30}),
    ("Desert Caravan",
     ["Perc: Frame Drum Large", "Perc: Frame Drum Muted", "Perc: Darbuka Two", "Perc: Hand Clap Group", "Perc: Darbuka One", "Perc: Frame Drum Small",
      "Perc: Tambourine Down", "Perc: Conga Open", "Perc: Small Shaker", "Perc: Tumba", "Perc: Tambourine Up", "Perc: Quinto",
      "Perc: Large Shaker", "Perc: Cowbell Muted", "Perc: Cabasa", "Wood: Claves"],
     {"Room": (30, 55), "Stumble": 25}),
    ("Wire and Dust",
     ["Kick: Pedal Thump", "Snare: Cross Stick", "Snare: Wires Mic", "Body: Pinky Snap", "Snare: Brush Release", "Kick: Hoop Click",
      "Hat: Pedal Noise", "Snare: Stick Taps", "Hat: Foot Chick", "Snare: Brush Dig", "Hat: Tight", "Kick: Shell Knock",
      "Ruin: Dust Brush", "Perc: Cabasa", "Metal: Clash Choked", "Perc: Small Shaker"],
     {"Room": (20, 40), "Stumble": 15}),
    ("Funeral March",
     ["Kick: Concert Bass Drum", "Snare: Marching Legacy", "Snare: Legacy Drum One", "Metal: Gong Low", "Snare: Legacy Drum Two", "Tom: Timpani",
      "Metal: Tubular Clang", "Tom: Tenor Low", "Metal: Bell Tree Fall", "Tom: Concert Low Mallet", "Metal: Dull Anvil", "Kick: Timpani Thud",
      "Cymbal: Clash Dull", "Tonal: Hand Chimes", "Cymbal: Crash Mallet", "Tonal: Vibraphone"],
     {"Room": (55, 95), "Decay": 4.5, "Stumble": 35}),
    ("Radio Static",
     ["Ruin: Gutted Kick", "Snare: Old Snare", "Ruin: Radio Snare", "Ruin: Wet Clap", "Snare: Cross Stick", "Ruin: Drowned Tom",
      "Ruin: Bitten Hat", "Tom: Tenor High", "Hat: Foot Chick", "Ruin: Dead Frame Drum", "Ruin: Bitten Hat", "Ruin: Melted Conga",
      "Ruin: Sunk Tambourine", "Ruin: Broken Cowbell", "Ruin: Dust Brush", "Ruin: Tin Bongo"],
     {"Megaphone": 60, "Gramophone": 50, "Room": (12, 20)}),
    ("Stutter Clinic",
     ["Kick: Pedal Thump", "Snare: Side Stick", "Snare: Rimshot", "Perc: Hand Clap Solo", "Snare: Tambourine On Top", "Tom: Fifteen",
      "Hat: Tight", "Tom: Fourteen", "Hat: Foot Chick", "Perc: Bongo Low", "Hat: Quarter Open", "Perc: Conga Slap",
      "Cymbal: Crash Choke", "Wood: Woodblock", "Cymbal: Ride Choke", "Perc: Cowbell Muted"],
     {"StutterDiv": 4, "StutterDepth": 55, "Chaos": 55, "Room": (20, 35)}),
    ("Marimba Rain",
     ["Tonal: Marimba", "Tonal: Xylophone", "Tonal: Vibraphone", "Tonal: Hand Chimes", "Tonal: Balafon Hard", "Tonal: Dan Tranh Pluck",
      "Tonal: Mbira Gandanga", "Tonal: Mbira Nyamaropa", "Tonal: Nyunga Nyunga", "Tonal: Kalimba Tanzania", "Tonal: Harpsichord Pluck", "Tonal: Harp Pluck",
      "Tonal: Balafon Soft", "Tonal: Kalimba Kenya", "Tonal: Marimba", "Tonal: Xylophone"],
     {"Room": (40, 70), "Decay": 2.8}),
    ("Cheap Orchestra",
     ["Kick: Frame Drum Boom", "Perc: Frame Drum Muted", "Snare: Marching Legacy", "Perc: Hand Clap Group", "Snare: Legacy Drum One", "Tom: Timpani",
      "Perc: Small Shaker", "Tom: Tenor Low", "Perc: Tambourine Down", "Tom: Concert High", "Metal: Bell Tree Fall", "Tom: Concert Low",
      "Cymbal: Clash Dull", "Tonal: Hand Chimes", "Cymbal: Ride Mallet", "Wood: Slapstick"],
     {"Room": (40, 75), "Stumble": 30, "Gramophone": 25}),
    ("Drowned Kit",
     ["Ruin: Drowned Tom", "Kick: Floor Tom Thud", "Ruin: Cardboard Snare", "Ruin: Wet Clap", "Ruin: Crushed Rimshot", "Ruin: Scorched Floor Tom",
      "Hat: Closed", "Ruin: Choked Timpani", "Hat: Foot Chick", "Ruin: Melted Conga", "Hat: Open", "Ruin: Dead Frame Drum",
      "Cymbal: Ride Mallet", "Ruin: Slowed Kalimba", "Cymbal: Crash Mallet", "Ruin: Rotten Harp"],
     {"Room": (55, 90), "Decay": 5.0, "Megaphone": 0, "Gramophone": 30}),
    ("Small Room Rock",
     ["Kick: Open Close-Mic", "Snare: Rimshot", "Snare: Top Mic", "Perc: Hand Clap Group", "Snare: Rimshot Wires", "Tom: Twenty-Two",
      "Hat: Tight", "Tom: Eighteen", "Hat: Foot Chick", "Tom: Fifteen", "Hat: Open", "Tom: Fourteen",
      "Cymbal: Crash Choke", "Perc: Cowbell", "Cymbal: Ride Bell", "Perc: Tambourine Down"],
     {"Room": (18, 25), "Output": -3}),
    ("Street Band",
     ["Kick: Legacy Bass Drum", "Snare: Cross Stick", "Snare: Marching Legacy", "Clap", "Snare: Modern Rimshot", "Tom: Tenor Low",
      "Perc: Tambourine Down", "Tom: Tenor High", "Perc: Cabasa", "Tom: Tenor High", "Perc: Tambourine Up", "Tom: Tenor Low",
      "Cymbal: Clash Dull", "Perc: Cowbell", "Cymbal: Crash Choke", "Wood: Slapstick"],
     {"Room": (25, 40), "Stumble": 45}),
]


SCALES = {
    1: [-12, -9, -7, -5, -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24],
    2: [-12, -10, -7, -5, -3, 0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24],
    3: [-12, -10, -9, -7, -5, -4, -1, 0, 2, 3, 5, 7, 8, 11, 12, 15],
}
FIRST_EXTRA = 64   # object numbers 48-63 are the synthesised models; the library's later objects start here


def old_names(params_h):
    import re
    text = open(params_h, encoding="utf-8").read()
    block = re.search(r"sampleObjectName\(int object\) \{.*?static const char\* names\[numSampleObjects\] = \{(.*?)\};", text, re.S).group(1)
    return re.findall(r'"([^"]+)"', block)


def write_headers(objects, folder, params_h):
    """DrumsObjects.h (names of the objects after the first 48) and DrumsKitsGenerated.h, from the same list."""
    old = old_names(params_h)
    assert len(old) == 48
    extra = [name for name, _ in objects[48:]]
    index = {n: FIRST_EXTRA + k for k, n in enumerate(extra)}
    index.update({n: i for i, n in enumerate(old)})
    with open(os.path.join(folder, "DrumsObjects.h"), "w", encoding="utf-8") as f:
        f.write("#pragma once\n// Generated by Tools/build_drum_library.py together with Library/drums.bin: do not edit.\n")
        f.write("#include <algorithm>\nnamespace inverso_abismo::drums {\n")
        f.write("constexpr int numExtraObjects = %d;   // library objects after the first 48; OBJECT numbers %d and up\n" % (len(extra), FIRST_EXTRA))
        f.write("inline const char* extraObjectName(int i) {\n static const char* names[numExtraObjects] = {\n")
        for n in extra:
            f.write('  "%s",\n' % n)
        f.write(" };\n return names[std::clamp(i, 0, numExtraObjects - 1)];\n}\n")
        f.write("inline int extraObjectStandIn(int i) {\n static const int map[numExtraObjects] = {")
        for k, n in enumerate(extra):
            f.write("%s%d," % ("\n  " if k % 24 == 0 else "", STAND_IN[n.split(":")[0]]))
        f.write("\n };\n return map[std::clamp(i, 0, numExtraObjects - 1)];\n}\n}\n")
    glob_params = {"Stumble": "Stumble", "Crush": "Crush", "Gramophone": "Gramophone", "Megaphone": "Megaphone",
                   "StutterDiv": "StutterDiv", "StutterDepth": "StutterDepth", "Chaos": "Chaos", "Output": "Output",
                   "Decay": "RoomDecay"}
    with open(os.path.join(folder, "DrumsKitsGenerated.h"), "w", encoding="utf-8") as f:
        f.write("#pragma once\n// Generated by Tools/build_drum_library.py: do not edit. Included by DrumsParams.h after Kit.\n")
        f.write("constexpr int numGeneratedKits = %d;\n" % len(KITS))
        f.write("inline const Kit* generatedKits() {\n using namespace P;\n static const Kit table[numGeneratedKits] = {\n")
        for name, items, extras in KITS:
            assert len(items) == 16, name
            rows = []
            scale = SCALES.get(extras.get("Scale"))
            for pad, item in enumerate(items):
                assert item in index, (name, item)
                rows.append("{%d,Object,%d}" % (pad, index[item]))
                if scale:
                    rows.append("{%d,Tune,%d}" % (pad, scale[pad]))
            for key, val in extras.items():
                if key == "Scale":
                    continue
                if key == "Room":
                    rows.append("{-1,RoomMix,%d}" % val[0])
                    rows.append("{-1,RoomSize,%d}" % val[1])
                elif key == "Decay":
                    rows.append("{-1,RoomDecay,%.2ff}" % val)
                else:
                    rows.append("{-1,%s,%d}" % (glob_params[key], val))
            rows.append("{-1,-1,0}")
            assert len(rows) <= 64, name
            f.write('  {"%s", {%s}},\n' % (name, ",".join(rows)))
        f.write(" };\n return table;\n}\n")
    print("headers: %d extra objects, %d kits" % (len(extra), len(KITS)))


def main():
    vcsl, body, rusty, out_bin, out_md = sys.argv[1:6]
    wav_dir = sys.argv[6] if len(sys.argv) > 6 else None
    objects = build(vcsl, body, rusty)
    if os.environ.get("DRUMLIB_HEADERS"):
        write_headers(objects, os.environ["DRUMLIB_HEADERS"], os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "DSP", "DrumsParams.h"))
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
