#!/usr/bin/env python3
"""Sets OUTPUT of the NEON presets (from index FIRST on) so each plays at -14 dBFS.
Usage: calibrate_neon_presets.py <Abismo folder> <first index>   (builds Tools/measure_neon_levels.cpp itself; repeats until settled)"""
import re, subprocess, sys, tempfile, os

root, first = sys.argv[1], int(sys.argv[2])
header = os.path.join(root, "Source", "DSP", "NeonParams.h")
exe = os.path.join(tempfile.mkdtemp(), "neonlevels")
for round_ in range(4):
    subprocess.run(["g++", "-std=c++20", "-O2", "-I" + root, os.path.join(root, "Tools", "measure_neon_levels.cpp"), "-o", exe], check=True)
    rows = [l.split("|") for l in subprocess.run([exe], capture_output=True, text=True, check=True).stdout.splitlines()]
    text = open(header, encoding="utf-8").read()
    worst = 0.0
    for idx, name, db, out in rows:
        if int(idx) < first:
            continue
        delta = -14.0 - float(db)
        worst = max(worst, abs(delta))
        new = max(-36.0, min(6.0, float(out) + delta))
        m = re.search(r'\{"%s",.*?\{Output, (-?[0-9.]+f?)\}' % re.escape(name), text, re.S)
        text = text[:m.start(1)] + "%.1ff" % new + text[m.end(1):]
    open(header, "w", encoding="utf-8").write(text)
    print("round", round_, "largest correction %.1f dB" % worst)
    if worst < 0.3:
        break
