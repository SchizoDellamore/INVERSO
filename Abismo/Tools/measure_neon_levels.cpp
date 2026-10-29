// Prints, for every NEON preset, the level of a played part (loudest 400 ms, dBFS) and its OUTPUT:
// "index name dB output". Tools/calibrate_neon_presets.py moves OUTPUT so the level lands on -14 dBFS.
// Build: g++ -std=c++20 -O2 -I<Abismo> measure_neon_levels.cpp
#include "../Source/DSP/NeonEngine.h"
#include <cstdio>
#include <vector>
namespace N = inverso_abismo::neon;
int main() {
 const int sr = 48000;
 for (std::size_t p = 0; p < N::presets().size(); ++p) {
  const auto v = N::presetValues(p);
  N::Engine e; e.setValues(v); e.prepare(sr); e.setTransport(120, 0, false);
  const bool mono = v[N::P::VoiceMode] >= 1 || (v[N::P::Arp] >= 1 && v[N::P::ArpMode] < 4.5f);
  if (mono) e.noteOn(45, .8f); else for (int n : {48, 55, 60, 64}) e.noteOn(n, .8f);
  std::vector<float> l(static_cast<std::size_t>(sr * 2)), r(l.size());
  for (int i = 0; i < sr * 2; i += 256) e.render(l.data() + i, r.data() + i, 256);
  double best = 0;
  const int w = sr * 4 / 10;
  for (int s = 0; s + w <= sr * 2; s += sr / 20) {
   double a = 0;
   for (int i = s; i < s + w; ++i) a += static_cast<double>(l[static_cast<std::size_t>(i)]) * l[static_cast<std::size_t>(i)] + static_cast<double>(r[static_cast<std::size_t>(i)]) * r[static_cast<std::size_t>(i)];
   best = std::max(best, a / (2 * w));
  }
  std::printf("%zu|%s|%.2f|%.2f\n", p, N::presets()[p].name, 10 * std::log10(best + 1e-12), static_cast<double>(v[N::P::Output]));
 }
}
