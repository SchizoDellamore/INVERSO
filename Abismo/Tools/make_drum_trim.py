#!/usr/bin/env python3
"""Writes Source/DSP/DrumsTrim.h from the output of measure_drum_levels.
Usage: make_drum_trim.py zero <folder of the headers>             (all zeros, to measure against)
       make_drum_trim.py objects <objects.txt> <DrumsTrim.h>      (kit trims left at zero)
       make_drum_trim.py kits <objects.txt> <kits.txt> <DrumsTrim.h>
Procedure (from Abismo/): regenerate drums.bin and the headers, run `make_drum_trim.py zero` for a header of zeros,
measure objects, write them, measure kits, write both."""
import sys


def write(path, objects, kits, kit_count):
    with open(path, "w") as f:
        f.write("#pragma once\n// Level of each object (recording or physical model) so that all of them hit at about the same loudness\n")
        f.write("// at their default settings, and of each pad of each kit. Measured by Tools/measure_drum_levels (K-weighted\n")
        f.write("// momentary maximum of one hit at velocity .8, aimed at -20 LUFS, peak kept under -3 dBFS); written by\n")
        f.write("// Tools/make_drum_trim.py: regenerate it when the library or the kits change.\n#include <array>\n")
        f.write("namespace inverso_abismo::drums {\nconstexpr int trimObjects = %d;\nconstexpr int trimKits = %d;\n" % (len(objects), kit_count))
        f.write("inline float objectTrimDb(int object) {\n static const float trim[trimObjects] = {")
        for i, v in enumerate(objects):
            f.write("%s%.2ff," % ("\n  " if i % 8 == 0 else " ", v))
        f.write("\n };\n return trim[object < 0 ? 0 : (object >= trimObjects ? trimObjects - 1 : object)];\n}\n\n")
        f.write("// The same, for each pad of each kit (its decay, tone, rattle and character change the level): added to the pad's LEVEL.\n")
        f.write("inline const std::array<std::array<float, 16>, trimKits>& kitPadTrimDb() {\n static const std::array<std::array<float, 16>, trimKits> trim{{\n")
        for k in range(kit_count):
            f.write("  {%s},\n" % ", ".join("%.2ff" % kits[k][p] for p in range(16)))
        f.write(" }};\n return trim;\n}\n}\n")


def main():
    mode = sys.argv[1]
    if mode == "zero":   # sizes from DrumsObjects.h / DrumsKitsGenerated.h
        import re
        folder = sys.argv[2]
        extra = int(re.search(r"numExtraObjects = (\d+)", open(folder + "/DrumsObjects.h").read()).group(1))
        gen = int(re.search(r"numGeneratedKits = (\d+)", open(folder + "/DrumsKitsGenerated.h").read()).group(1))
        count = 64 + extra
        write(folder + "/DrumsTrim.h", [0.0] * count, [[0.0] * 16 for _ in range(11 + gen)], 11 + gen)
        return
    objects = {}
    for line in open(sys.argv[2]):
        p = line.split()
        if len(p) == 4:
            objects[int(p[0])] = float(p[3])
    obj = [objects[i] for i in range(len(objects))]
    if mode == "objects":
        import re
        text = open(sys.argv[3]).read()
        kit_count = int(re.search(r"trimKits = (\d+)", text).group(1))
        write(sys.argv[3], obj, [[0.0] * 16 for _ in range(kit_count)], kit_count)
        return
    kits = {}
    for line in open(sys.argv[3]):
        p = line.split()
        if len(p) == 4:
            kits.setdefault(int(p[0]), {})[int(p[1])] = (float(p[2]), float(p[3]))
    rows = []
    for k in range(len(kits)):
        # LEVEL runs from -36 to +6 dB: if a pad would need more than that, the whole kit comes down by the shortfall
        # (every pad stays at the same loudness as the others) instead of that pad staying quiet.
        shortfall = max(0.0, max(w + lv - 6.0 for w, lv in kits[k].values()))
        rows.append([max(-36.0 - kits[k][p][1], kits[k][p][0] - shortfall) for p in range(16)])
    write(sys.argv[4], obj, rows, len(kits))


main()
