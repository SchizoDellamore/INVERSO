"""Prints each IR's level at a few frequencies relative to 1 kHz (1/3 octave averages)."""
import sys, glob, numpy as np, soundfile as sf
pts = [60, 100, 200, 400, 700, 2000, 3000, 4500, 6000, 8000, 10000, 14000]
print(f"{'':44s}" + ''.join(f'{p:>7d}' for p in pts))
for path in sorted(glob.glob(sys.argv[1])):
    h, fs = sf.read(path)
    H = np.abs(np.fft.rfft(h, 1 << 15)) ** 2
    f = np.fft.rfftfreq(1 << 15, 1 / fs)
    band = lambda c: 10 * np.log10(np.mean(H[(f > c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))]))
    ref = band(1000)
    print(f"{path.split('/')[-1][:-4]:44s}" + ''.join(f'{band(p) - ref:7.1f}' for p in pts))
