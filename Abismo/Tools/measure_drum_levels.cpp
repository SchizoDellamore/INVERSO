// Measures how loud each object (mode "objects") and each pad of each kit (mode "kits") plays, to aim every hit at
// the same K-weighted momentary level; Tools/make_drum_trim.py turns the output into Source/DSP/DrumsTrim.h.
// Build it against the source tree (header only: g++ -std=c++20 -O2 -I<Abismo> measure_drum_levels.cpp), run it
// from Abismo/ with DrumsTrim.h holding zeros for what is being measured:
//   objects: prints "object momentaryLufs peakDb trimDb" (all object trims zero)
//   kits:    prints "kit pad trimDb level"                (object trims final, kit trims zero; the script keeps LEVEL in range)
#include "../Source/DSP/DrumsEngine.h"
#include "../Source/DSP/ChainDyn.h"
#include <cstdio>
#include <cstring>
#include <fstream>
namespace D = inverso_abismo::drums;
namespace C = inverso_abismo::chain;

struct Measure { double lufs, peakDb; };
static Measure hit(const D::Values& v, const D::SampleLibrary& lib, int note, double sr) {
 const int n = static_cast<int>(sr * 2.5);
 D::Engine e; e.setValues(v); e.prepare(sr); e.setLibrary(&lib);
 std::vector<float> l(static_cast<std::size_t>(n), 0.f), r(static_cast<std::size_t>(n), 0.f);
 e.noteOn(note, .8f);
 for (int i = 0; i < n; i += 256) e.render(l.data() + i, r.data() + i, std::min(256, n - i));
 C::Loudness m; m.prepare(sr);
 double loud = -120, peak = 0;
 for (int i = 0; i < n; ++i) {
  const auto u = static_cast<std::size_t>(i);
  m.process(l[u], r[u]);
  loud = std::max(loud, m.momentaryLufs());
  peak = std::max(peak, static_cast<double>(std::max(std::abs(l[u]), std::abs(r[u]))));
 }
 return {loud, 20 * std::log10(peak + 1e-9)};
}

int main(int argc, char** argv) {
 const bool kits = argc > 1 && std::strcmp(argv[1], "kits") == 0;
 D::SampleLibrary lib;
 std::ifstream f("Library/drums.bin", std::ios::binary);
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
 if (!D::SampleLibrary::parse(bytes.data(), bytes.size(), lib)) { std::fprintf(stderr, "no library\n"); return 1; }
 lib.equaliseLoudness();
 const double sr = 48000;
 if (!kits) {
  for (int o = 0; o < D::numObjects; ++o) {
   auto v = D::defaults();
   v[static_cast<std::size_t>(D::P::padBase(0) + D::P::Object)] = static_cast<float>(o);
   const auto m = hit(v, lib, D::firstPadNote, sr);
   double want = -20.0 - m.lufs;
   if (m.peakDb + want > -3.0) want = -3.0 - m.peakDb;
   std::printf("%d %.1f %.1f %.2f\n", o, m.lufs, m.peakDb, std::max(-24.0, std::min(18.0, want)));
  }
  return 0;
 }
 for (std::size_t k = 0; k < D::kits().size(); ++k) {
  const auto v = D::kitValues(k);
  for (int pad = 0; pad < D::numPads; ++pad) {
   const auto m = hit(v, lib, D::firstPadNote + pad, sr);
   double want = -21.0 - m.lufs;
   if (m.peakDb + want > -3.0) want = -3.0 - m.peakDb;
   const double level = static_cast<double>(v[static_cast<std::size_t>(D::P::padBase(pad) + D::P::Level)]);
   std::printf("%zu %d %.2f %.2f\n", k, pad, want, level);
  }
 }
}
