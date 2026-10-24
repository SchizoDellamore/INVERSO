"""INVERSO CABS: synthetic guitar and bass speaker cabinet impulse responses.

Each IR is designed as a magnitude response (speaker, cabinet, microphone and
its position), made minimum phase like a real close-miked speaker, then given
the time-domain parts a magnitude cannot hold: the back wave of open cabinets,
floor and room reflections for distant mics. Deterministic: run it again and
the same files come out.

  python3 make_cab_irs.py <output folder>
"""
import os
import zlib
import sys
import numpy as np
import soundfile as sf

NFFT = 1 << 16


def grid(fs):
    f = np.fft.rfftfreq(NFFT, 1.0 / fs)
    f[0] = f[1] * .5
    return f


# ---- building blocks, all in dB over the frequency grid
def hp2(f, fc, q):
    s = 1j * f / fc
    return 20 * np.log10(np.abs(s * s / (s * s + s / q + 1)))


def lp2(f, fc, q):
    s = 1j * f / fc
    return 20 * np.log10(np.abs(1 / (s * s + s / q + 1)))


def hp1(f, fc):
    s = 1j * f / fc
    return 20 * np.log10(np.abs(s / (s + 1)))


def lp1(f, fc):
    s = 1j * f / fc
    return 20 * np.log10(np.abs(1 / (s + 1)))


def peak(f, fc, gain, q):
    # bell on a log axis: gain at fc, width set by q (bandwidth in octaves ~ 1.4/q)
    oct_ = np.log2(f / fc)
    bw = 1.4 / q
    return gain * np.exp(-.5 * (oct_ / (bw * .5)) ** 2)


def shelf(f, fc, gain, slope_oct=1.0):
    return gain / (1 + np.exp(-np.log2(f / fc) * 4.0 / slope_oct))


class Rng:
    """splitmix64 + Box-Muller: the same numbers in Python and in the C++ port."""
    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFFFFFFFFFF

    def next_u64(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
        return z ^ (z >> 31)

    def uniform(self):
        return ((self.next_u64() >> 11) + .5) / float(1 << 53)

    def normal(self, n):
        out = np.empty(n)
        for i in range(n):
            u1, u2 = self.uniform(), self.uniform()
            out[i] = np.sqrt(-2 * np.log(u1)) * np.cos(2 * np.pi * u2)
        return out


def smooth_noise_db(f, seed, depth, start, width_oct=1 / 10):
    """Cone breakup: a fixed, speaker-specific ripple that grows above `start`."""
    lo, hi = np.log2(20), np.log2(24000)
    knots = lo + width_oct * np.arange(int(np.ceil((hi - lo) / width_oct)) + 2)
    vals = Rng(seed).normal(len(knots))
    ripple = np.interp(np.log2(f), knots, vals)
    weight = np.clip(np.log2(f / start) / 2.0, 0, 1)
    return depth * ripple * weight


# ---- speakers (in the spirit of well-known types; names are descriptive)
SPEAKERS = {
    # name: low resonance (fc, q), list of bells, lowpass (fc, q), extra lowpass fc, breakup (depth, start, seed)
    'Brit 4x12 V':   dict(low=(85, 1.5), bells=[(110, 2.0, 2.2), (500, -3.5, 1.2), (1400, 1.5, 1.5), (2600, 4.0, 2.2), (4200, 5.0, 2.5)], lp=(5200, 1.1), lp2=7500, brk=(2.4, 900, 11)),
    'Brit 4x12 G':   dict(low=(80, 1.3), bells=[(105, 2.0, 2.0), (650, -2.0, 1.0), (1900, 3.0, 1.4), (3200, 1.5, 2.0)], lp=(4600, .9), lp2=6800, brk=(1.8, 1000, 23)),
    'Brit 2x12 Blue': dict(low=(95, 1.2), bells=[(120, 1.0, 2.0), (800, -1.5, 1.0), (2300, 2.0, 1.6), (3700, 4.5, 2.2), (5300, 2.0, 3.0)], lp=(6200, 1.0), lp2=9000, brk=(2.2, 900, 37)),
    'Brit 1x12 Open': dict(low=(105, .9), bells=[(160, 1.0, 1.5), (700, -1.0, 1.0), (2800, 3.0, 1.8), (4500, 2.0, 2.5)], lp=(5600, .9), lp2=8500, brk=(2.0, 1000, 41), open_back=True),
    'Amer 2x12 Open': dict(low=(100, .85), bells=[(130, 1.0, 1.8), (450, -2.5, 1.0), (1700, 1.0, 1.4), (3300, 3.5, 1.6), (5600, 2.0, 2.5)], lp=(6600, .9), lp2=9500, brk=(2.0, 1100, 53), open_back=True),
    'Amer 1x12 Open': dict(low=(110, .8), bells=[(180, 1.5, 1.5), (600, -1.5, 1.0), (2500, 2.0, 1.4), (4400, 3.0, 2.0)], lp=(6000, .85), lp2=9000, brk=(2.2, 1000, 67), open_back=True),
    'Amer 4x10 Open': dict(low=(115, .9), bells=[(160, 2.0, 1.6), (500, -1.5, 1.2), (1600, 2.0, 1.4), (3600, 3.0, 2.0)], lp=(5800, .9), lp2=8500, brk=(2.6, 900, 71), open_back=True),
    'Modern 4x12':   dict(low=(75, 1.6), bells=[(95, 3.0, 2.2), (420, -4.5, 1.1), (1300, -1.0, 1.5), (2500, 3.5, 2.0), (4400, 5.0, 2.8)], lp=(5400, 1.2), lp2=7400, brk=(2.0, 1000, 83)),
    # bass
    'Bass 8x10':      dict(low=(58, 1.2), bells=[(75, 2.5, 1.6), (300, -1.5, 1.2), (800, 2.5, 1.2), (1800, 2.0, 1.5)], lp=(4200, .8), lp2=6500, brk=(1.6, 900, 101), bass=True),
    'Bass 1x15 Flip': dict(low=(48, 1.3), bells=[(60, 3.0, 1.6), (450, -2.0, 1.0), (1100, 1.5, 1.4)], lp=(2800, .8), lp2=4500, brk=(1.4, 700, 113), bass=True),
    'Bass 4x10 Horn': dict(low=(62, 1.0), bells=[(85, 1.5, 1.5), (500, -2.0, 1.0), (1500, 2.0, 1.5), (2600, 1.5, 2.0)], lp=(3800, .8), lp2=6000, brk=(1.8, 900, 127), bass=True, horn=(3800, 12000, -9.0)),
    'Bass 2x12 Modern': dict(low=(52, 1.1), bells=[(70, 2.0, 1.4), (380, -3.0, 1.0), (1000, 1.0, 1.4), (2200, 2.0, 1.8)], lp=(3400, .85), lp2=5200, brk=(1.5, 800, 131), bass=True, horn=(3600, 15000, -7.0)),
    'Bass 1x18 Sub':  dict(low=(38, 1.0), bells=[(48, 2.0, 1.4), (300, -1.0, 1.0)], lp=(1400, .7), lp2=2600, brk=(1.0, 500, 149), bass=True),
}

# ---- microphones: tonal character; directional mics get proximity bass when close
MICS = {
    'Dyn57':  dict(bells=[(5800, 3.0, 1.6), (2500, 1.0, 1.4)], hp=150, lp=14000, directional=True),
    'Dyn421': dict(bells=[(4500, 3.0, 1.4), (900, -1.0, 1.0)], hp=70, lp=16000, directional=True),
    'Cond87': dict(bells=[(9000, 2.0, 1.0)], hp=30, lp=20000, directional=True, gentle=True),
    'Rib121': dict(bells=[(120, 1.0, 1.0), (3500, -1.5, 1.2)], hp=25, lp=8500, directional=True, ribbon=True),
}

# ---- positions: on-axis brightness and distance
POSITIONS = {
    'Cap':      dict(bright=2.0, axis=0.0, dist=.025),
    'CapEdge':  dict(bright=0.0, axis=0.0, dist=.025),
    'Cone':     dict(bright=-3.5, axis=0.0, dist=.03),
    'OffAxis':  dict(bright=-1.0, axis=1.0, dist=.04),
    'Back':     dict(bright=-4.0, axis=.3, dist=.15, back=True),   # behind an open cabinet
    'Room':     dict(bright=-1.5, axis=.3, dist=1.2, room=True),
}

C = 343.0


def magnitude(fs, spk, mic, pos):
    f = grid(fs)
    s = SPEAKERS[spk]
    m = MICS[mic]
    p = POSITIONS[pos]
    db = hp2(f, *s['low']) + lp2(f, *s['lp']) + 2 * lp2(f, s['lp2'], .7)
    for fc, g, q in s['bells']:
        db += peak(f, fc, g * (.55 if fc > 1500 and not s.get('bass') else 1.0), q)
    if not s.get('bass'):
        db += peak(f, 220, 2.5, .6)            # the body a guitar cone has in the low mids
    if s.get('open_back'):
        db += hp1(f, 70)                      # less low end with an open back
    if 'horn' in s:
        lo, hi, lvl = s['horn']
        hf = hp2(f, lo, .7) + lp2(f, hi, .7) + lvl
        db = 20 * np.log10(10 ** (db / 20) + 10 ** (hf / 20))
    depth, start, seed = s['brk']
    db += smooth_noise_db(f, seed, depth, start)
    # position on the cone
    db += shelf(f, 3000, p['bright'], 1.2)
    if p['axis'] > 0:
        db += p['axis'] * (lp2(f, 3600, .6) + 2.0)
    # microphone
    for fc, g, q in m['bells']:
        db += peak(f, fc, g, q)
    db += hp2(f, m['hp'], .7) + lp2(f, m['lp'], .7)
    if m.get('ribbon'):
        db += lp1(f, 9000)
    # proximity effect of a directional mic, strongest when close
    if m.get('directional') and p['dist'] < .3:
        prox = 6.0 * (1 - p['dist'] / .3) * (.5 if m.get('gentle') else 1.0)
        db += prox * (1 - 1 / (1 + (180 / f) ** 2))
    return f, db


def minimum_phase(db):
    logmag = db / 20 * np.log(10)
    full = np.concatenate([logmag, logmag[-2:0:-1]])
    cep = np.fft.ifft(full).real
    n = len(full)
    fold = np.zeros(n)
    fold[0] = cep[0]
    fold[1:n // 2] = 2 * cep[1:n // 2]
    fold[n // 2] = cep[n // 2]
    return np.fft.ifft(np.exp(np.fft.fft(fold))).real


def delayed(x, seconds, fs, gain, lowpass_hz=None):
    d = seconds * fs
    i = int(d)
    frac = d - i
    y = np.zeros_like(x)
    if i + 1 >= len(x):
        return y
    y[i:] += (1 - frac) * x[:len(x) - i]
    y[i + 1:] += frac * x[:len(x) - i - 1]
    if lowpass_hz:
        a = np.exp(-2 * np.pi * lowpass_hz / fs)
        out = np.zeros_like(y)
        acc = 0.0
        for k in range(len(y)):
            acc = (1 - a) * y[k] + a * acc
            out[k] = acc
        y = out
    return gain * y


def make_ir(fs, spk, mic, pos, length):
    f, db = magnitude(fs, spk, mic, pos)
    h = minimum_phase(db)[:length].copy()
    s = SPEAKERS[spk]
    p = POSITIONS[pos]
    if s.get('open_back'):
        # the back wave bounces off the wall behind the cabinet and returns inverted
        h += delayed(h, .0045 if p.get('back') else .0062, fs, -(.55 if p.get('back') else .28), 2500)
    if p.get('back'):
        h = -h                                  # the back of the cone moves the other way
    if p.get('room'):
        # floor bounce, two walls, the ceiling, then a short diffuse tail
        direct = p['dist']
        for extra, g, lp in [(.9, .5, 6000), (2.6, .32, 4500), (3.4, .28, 4000), (4.1, .22, 3500), (6.0, .16, 3000)]:
            h += delayed(h, extra / C, fs, g * direct / (direct + extra), lp)
        n = np.arange(length)
        tail = Rng(zlib.crc32(f"{spk}|{mic}".encode())).normal(length) * np.exp(-n / (fs * .09))
        tail[:int(.012 * fs)] = 0
        a = np.exp(-2 * np.pi * 3000 / fs)
        from scipy.signal import lfilter
        tail = lfilter([1 - a], [1, -a], tail)
        tail = np.convolve(tail, h[:int(.02 * fs)])[:length]
        h += .035 * tail / (np.max(np.abs(tail)) + 1e-12) * np.max(np.abs(h))
        h = np.concatenate([np.zeros(int(direct / C * fs * .25)), h])[:length]
    # fade the end
    fade = int(length * .25)
    h[-fade:] *= np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    return h


def loudness(h, fs):
    """Energy through a pink-ish spectrum, 60 Hz - 8 kHz: equal loudness across the set."""
    H = np.abs(np.fft.rfft(h, NFFT)) ** 2
    f = grid(fs)
    w = np.where((f > 60) & (f < 8000), 1 / f, 0)
    return np.sqrt(np.sum(H * w) / np.sum(w))


def plan():
    out = []
    guitar = [k for k, v in SPEAKERS.items() if not v.get('bass')]
    bass = [k for k, v in SPEAKERS.items() if v.get('bass')]
    for spk in guitar:
        positions = ['Cap', 'CapEdge', 'Cone', 'OffAxis', 'Room']
        if SPEAKERS[spk].get('open_back'):
            positions.append('Back')
        for pos in positions:
            out.append(('Guitar', spk, 'Dyn57', pos))
        out.append(('Guitar', spk, 'Rib121', 'CapEdge'))
        out.append(('Guitar', spk, 'Cond87', 'Cone'))
        out.append(('Guitar', spk, 'Dyn421', 'CapEdge'))
    for spk in bass:
        for mic, pos in [('Dyn421', 'Cap'), ('Dyn421', 'CapEdge'), ('Cond87', 'Cone'), ('Rib121', 'CapEdge'), ('Dyn57', 'OffAxis'), ('Cond87', 'Room')]:
            out.append(('Bass', spk, mic, pos))
    return out


def main(root):
    items = plan()
    for fs in (48000, 44100):
        length = int(.17 * fs)
        made = []
        for group, spk, mic, pos in items:
            h = make_ir(fs, spk, mic, pos, int(.35 * fs) if pos == 'Room' else length)
            made.append((group, spk, mic, pos, h / loudness(h, fs)))
        top = max(np.max(np.abs(h)) for *_, h in made)
        for group, spk, mic, pos, h in made:
            folder = os.path.join(root, f'{fs // 1000 if fs == 48000 else 44.1} kHz'.replace('.0', ''), group, spk)
            os.makedirs(folder, exist_ok=True)
            name = f'{spk} - {mic} {pos}.wav'
            sf.write(os.path.join(folder, name), (h / top * .89).astype(np.float32), fs, subtype='PCM_24')
    print(len(items), 'IRs per sample rate')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'INVERSO CABS')
