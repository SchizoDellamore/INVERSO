"""Plays a guitar riff, a clean part and a bass line through amps and the cab IRs.

The instruments are Karplus-Strong strings with a pickup; the amps are simple
gain stages with a tone stack. They exist to hear the IRs, nothing more.

  python3 demo_cabs.py <IR folder (48 kHz)> <output folder>
"""
import os
import sys
import numpy as np
import soundfile as sf
from scipy.signal import lfilter, butter, sosfilt, fftconvolve

FS = 48000
rng = np.random.default_rng(7)


def string(freq, dur, vel=1.0, mute=0.0, pick=.15, seed=0, bass=False):
    """Karplus-Strong string, computed one period at a time."""
    n = int(dur * FS)
    D = FS / freq - .5                      # the averaging filter adds half a sample
    N = int(D)
    f = D - N
    r = np.random.default_rng(seed)
    exc = r.uniform(-1, 1, N) * vel
    exc = lfilter([.55 + .45 * vel, -.25 - .2 * mute], [1], exc)
    k = max(1, int(pick * N))
    exc = exc - np.roll(exc, k)              # pick position
    # loop gain per sample: long sustain open, short when palm muted
    t60 = (4.0 if not bass else 3.0) * (1 - .93 * mute) * (110 / freq) ** .35
    g = 10 ** (-3 / (t60 * FS))
    y = np.zeros(n + N + 2)
    y[2:N + 2] = exc
    pos = N + 2
    while pos < len(y):
        end = min(len(y), pos + N)
        idx = np.arange(pos, end)
        a1 = .5 * (y[idx - N] + y[idx - N - 1])
        a2 = .5 * (y[idx - N - 1] + y[idx - N - 2])
        y[idx] = g * ((1 - f) * a1 + f * a2)
        if mute > 0:
            y[idx] = .7 * y[idx] + .3 * .5 * (y[idx - N] + y[idx - N - 2]) * g
        pos = end
    out = y[2:n + 2]
    pu = max(1, int(.13 * FS / freq))        # pickup position
    out = out - .6 * np.concatenate([np.zeros(pu), out[:-pu]])
    rel = int(.015 * FS)
    out[-rel:] *= np.linspace(1, 0, rel)
    return out


def place(track, sig, t):
    i = max(0, int(t * FS))
    j = min(len(track), i + len(sig))
    track[i:j] += sig[:j - i]


def note_hz(midi):
    return 440 * 2 ** ((midi - 69) / 12)


def guitar_riff(seed, timing=0.0):
    bpm = 132
    e = 60 / bpm / 2
    total = 16 * e * 4 + 2.5
    t = np.zeros(int(total * FS))
    s = seed * 1000
    def chord(root, t0, dur, mute=0.0, vel=1.0, fifth=True):
        notes = [root, root + 7, root + 12] if fifth else [root]
        for j, m in enumerate(notes):
            place(t, string(note_hz(m), dur + .05, vel * (1 - .1 * j), mute, seed=s + int(t0 * 100) + j), t0 + j * .004 + timing * rng.uniform(-.006, .006))
    bar = 0.0
    for rep in range(2):
        for k in range(6):
            chord(40, bar + k * e, e * .95, mute=.85, vel=.9)          # E5 chugs
        chord(43, bar + 6 * e, e * 2, vel=1.0)                          # G5
        for k in range(6):
            chord(40, bar + 8 * e + k * e, e * .95, mute=.85, vel=.9)
        chord(46, bar + 14 * e, e, vel=1.0)                              # A#5
        chord(45, bar + 15 * e, e, vel=1.0)                              # A5
        bar += 16 * e
    chord(40, bar, 2.4, vel=1.0)                                        # ring out
    chord(47, bar + .004, 2.4, vel=.8, fifth=False)
    return t[:int((bar + 2.5) * FS)]


def clean_part(seed):
    bpm = 90
    e = 60 / bpm / 2
    seq = [(52, 59, 64, 67, 71, 67, 64, 59), (48, 55, 60, 64, 67, 64, 60, 55), (45, 52, 57, 60, 64, 60, 57, 52), (50, 57, 62, 66, 69, 66, 62, 57)]
    total = len(seq) * 8 * e + 3
    t = np.zeros(int(total * FS))
    time = 0.0
    for bar in seq:
        for k, m in enumerate(bar):
            place(t, string(note_hz(m), 3.0, .55 + .1 * (k == 0), seed=seed * 999 + int(time * 50), pick=.2), time)
            time += e
    return t


def bass_line(seed):
    bpm = 104
    e = 60 / bpm / 2
    line = [28, 28, 40, 28, 31, 31, 33, 35, 28, 28, 40, 28, 36, 36, 35, 33]
    t = np.zeros(int((len(line) * 2 * e + 2) * FS))
    time = 0.0
    for rep in range(2):
        for k, m in enumerate(line):
            place(t, string(note_hz(m), e * 1.9 if k % 4 == 3 else e * .98, .8 + .2 * (k % 4 == 0), mute=.25, pick=.22, seed=seed * 77 + k + rep * 31, bass=True), time)
            time += e
    return t


def hp(x, fc, order=2):
    return sosfilt(butter(order, fc, 'hp', fs=FS, output='sos'), x)


def lp(x, fc, order=2):
    return sosfilt(butter(order, fc, 'lp', fs=FS, output='sos'), x)


def bell(x, fc, gain_db, q):
    A = 10 ** (gain_db / 40)
    w = 2 * np.pi * fc / FS
    al = np.sin(w) / (2 * q)
    b = [1 + al * A, -2 * np.cos(w), 1 - al * A]
    a = [1 + al / A, -2 * np.cos(w), 1 - al / A]
    return lfilter(b, a, x)


def amp_high_gain(x):
    x = hp(x / (np.max(np.abs(x)) + 1e-9), 110)
    x = np.tanh(25 * x + .1) - np.tanh(.1)
    x = lp(hp(x, 60), 6500)
    x = np.tanh(6 * x - .2) + np.tanh(.2)
    x = bell(x, 650, -6, .8)
    x = bell(x, 120, 3, .8)
    x = np.tanh(1.5 * x)
    return hp(x, 40)


def amp_clean(x):
    x = hp(x / (np.max(np.abs(x)) + 1e-9), 70)
    x = np.tanh(1.6 * x) / np.tanh(1.6)
    x = bell(x, 500, -2, .7)
    return x


def amp_bass(x):
    x = hp(x / (np.max(np.abs(x)) + 1e-9), 35)
    d = np.tanh(3 * x) / np.tanh(3)
    x = .7 * x + .3 * d
    x = bell(x, 800, 2, .8)
    return x


def cab(x, ir_path):
    h, fs = sf.read(ir_path)
    assert fs == FS
    return fftconvolve(x, h)[:len(x)]


def finish(x, peak=.8):
    return x / (np.max(np.abs(x)) + 1e-9) * peak


def main(ir_root, out):
    os.makedirs(out, exist_ok=True)
    gl, gr = amp_high_gain(guitar_riff(1, 1)), amp_high_gain(guitar_riff(2, 1))
    cl, cr = amp_clean(clean_part(1)), amp_clean(clean_part(2))
    bass = amp_bass(bass_line(3))
    takes = []
    for name in sorted(os.listdir(os.path.join(ir_root, 'Guitar'))):
        takes.append(('riff', f'Guitar/{name}/{name} - Dyn57 Cap.wav'))
    for mp in ['Dyn57 CapEdge', 'Dyn57 Cone', 'Dyn57 OffAxis', 'Dyn57 Room', 'Rib121 CapEdge', 'Cond87 Cone', 'Dyn421 CapEdge']:
        takes.append(('riff', f'Guitar/Brit 4x12 V/Brit 4x12 V - {mp}.wav'))
    for name in ['Amer 2x12 Open', 'Brit 1x12 Open']:
        for mp in ['Dyn57 Cap', 'Dyn57 Back', 'Dyn57 Room']:
            takes.append(('clean', f'Guitar/{name}/{name} - {mp}.wav'))
    for name in sorted(os.listdir(os.path.join(ir_root, 'Bass'))):
        takes.append(('bass', f'Bass/{name}/{name} - Dyn421 Cap.wav'))
    takes.append(('bass', 'Bass/Bass 8x10/Bass 8x10 - Cond87 Room.wav'))
    for i, (kind, rel) in enumerate(takes):
        path = os.path.join(ir_root, rel)
        if kind == 'riff':
            y = np.stack([cab(gl, path), cab(gr, path)], 1)
        elif kind == 'clean':
            a, b = cab(cl, path), cab(cr, path)
            y = np.stack([.8 * a + .2 * b, .2 * a + .8 * b], 1)
        else:
            m = cab(bass, path)
            y = np.stack([m, m], 1)
        sf.write(os.path.join(out, f'cab_{i:02d}.wav'), finish(y), FS, subtype='PCM_16')
        print(f'cab_{i:02d}', kind, rel)
    # without a cab, for comparison
    sf.write(os.path.join(out, 'nocab_riff.wav'), finish(np.stack([gl, gr], 1), .5), FS, subtype='PCM_16')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
