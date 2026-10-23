// ENKI: every source type sounds, the generator stays in range and keeps locked
// sections, mutation moves by its amount, generated sounds are levelled, finite
// and below 0 dBFS. With a directory argument it writes demo WAVs of generated sounds.
#include "../Source/DSP/DrumsEngine.h"
#include "../Source/DSP/EnkiGenerator.h"
#include "TestUtil.h"
#include <fstream>
#include <string>
using namespace test;
namespace E = inverso_abismo::enki;
namespace D = inverso_abismo::drums;
using namespace E::P;

#ifndef ABISMO_DRUMS_LIBRARY
#define ABISMO_DRUMS_LIBRARY "Library/drums.bin"
#endif

static std::vector<E::Sample> samples;
static E::SamplePool pool;

static void loadPool() {
 std::ifstream f(ABISMO_DRUMS_LIBRARY, std::ios::binary);
 verify(f.good(), "cannot open Library/drums.bin");
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), {});
 static D::SampleLibrary lib;
 verify(D::SampleLibrary::parse(bytes.data(), bytes.size(), lib), "library does not parse");
 samples.resize(lib.objects.size());
 pool.fill(nullptr);
 for (std::size_t i = 0; i < lib.objects.size(); ++i) {
  samples[i].data = lib.objects[i].layers.back(); // the loudest take
  samples[i].sampleRate = lib.objects[i].sampleRate;
  pool[i] = &samples[i];
 }
}

static Stereo play(E::Engine& e, int n) {
 Stereo out{std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n))};
 for (int i = 0; i < n; i += 256) e.render(out.l.data() + i, out.r.data() + i, std::min(256, n - i));
 return out;
}
static E::Engine make(const E::Values& v, int sr) {
 E::Engine e;
 for (int i = 0; i < E::numSamples; ++i) e.setSample(i, pool[static_cast<std::size_t>(i)]);
 e.setValues(v); e.prepare(sr); e.setTransport(110, 0, true);
 return e;
}

static void sources() {
 const int sr = 48000;
 const char* names[] = {"wave", "fm", "noise", "sample", "grains", "resonator", "reed"};
 for (int t = 0; t < E::numSourceTypes; ++t)
  for (float motion : {0.f, 60.f}) {
   E::Values v = E::defaults();
   v[AType] = static_cast<float>(t); v[AMotion] = motion; v[ALevel] = 90; v[BLevel] = 0; v[RevMix] = 0;
   auto e = make(v, sr);
   e.noteOn(60, .9f);
   const auto s = play(e, sr);
   const float level = rms(s.l, static_cast<std::size_t>(sr / 10), s.l.size());
   std::printf("source %-9s motion %3.0f: rms %.4f\n", names[t], motion, level);
   verify(finite(s) && peak(s) <= 1.f && level > 1e-3f, "a source type is silent or invalid");
  }
}

static void generator() {
 E::Generator g(5);
 E::Values cur = E::defaults();
 // Ranges hold for many draws in every category, and locks keep their sections.
 for (int cat = 0; cat < 5; ++cat) {
  cur[Category] = static_cast<float>(cat);
  for (int i = 0; i < 200; ++i) {
   auto v = g.generate(cur, 0);
   for (int p = 0; p < Count; ++p) {
    const auto& s = E::specs()[static_cast<std::size_t>(p)];
    verify(v[static_cast<std::size_t>(p)] >= s.min - 1e-3f && v[static_cast<std::size_t>(p)] <= s.max + 1e-3f, "generated value out of range");
   }
   verify(v[Category] == cur[Category], "generator changed its own category");
  }
 }
 cur = g.generate(cur, 0);
 cur[LockA] = 1; cur[LockSpace] = 1;
 const auto next = g.generate(cur, 0);
 for (int p = AType; p <= ASample; ++p) verify(next[static_cast<std::size_t>(p)] == cur[static_cast<std::size_t>(p)], "locked source A changed");
 for (int p = EchoMix; p <= RevDecay; ++p) verify(next[static_cast<std::size_t>(p)] == cur[static_cast<std::size_t>(p)], "locked space changed");
 // Mutation distance grows with the amount.
 cur[LockA] = 0; cur[LockSpace] = 0;
 auto distance = [&](const E::Values& a, const E::Values& b) {
  double d = 0;
  for (int p = 0; p < Category; ++p) {
   const auto& s = E::specs()[static_cast<std::size_t>(p)];
   d += std::abs(a[static_cast<std::size_t>(p)] - b[static_cast<std::size_t>(p)]) / (s.max - s.min);
  }
  return d;
 };
 double small = 0, large = 0;
 for (int i = 0; i < 50; ++i) { small += distance(cur, g.mutate(cur, .1f, 0)); large += distance(cur, g.mutate(cur, .8f, 0)); }
 std::printf("mutation distance: 10%% -> %.2f, 80%% -> %.2f\n", small / 50, large / 50);
 verify(large > small * 3, "mutate amount does not scale");
}

static void demos(const std::string& dir) {
 const int sr = 48000;
 const char* cats[] = {"Musical", "Textures", "Hits", "Rhythmic", "Weird"};
 E::Generator g(2026);
 for (int cat = 0; cat < 5; ++cat)
  for (int i = 0; i < 6; ++i) {
   E::Values cur = E::defaults();
   cur[Category] = static_cast<float>(cat);
   auto v = g.generateLevelled(cur, 0, pool);
   auto e = make(v, sr);
   Stereo all;
   auto append = [&](const Stereo& s) { all.l.insert(all.l.end(), s.l.begin(), s.l.end()); all.r.insert(all.r.end(), s.r.begin(), s.r.end()); };
   if (cat == 2) { // hits: four, at different pitches
    for (int n : {48, 43, 55, 36}) { e.noteOn(n, .9f); append(play(e, sr * 3 / 2)); e.noteOff(n); }
    append(play(e, sr * 2));
   } else {
    for (int n : {48, 55, 60, 63}) e.noteOn(n, .8f);
    append(play(e, cat == 1 ? sr * 6 : sr * 3));
    for (int n : {48, 55, 60, 63}) e.noteOff(n);
    append(play(e, sr));
    for (int n : {67, 70, 72, 75, 74, 70}) { e.noteOn(n, .8f); append(play(e, sr / 2)); e.noteOff(n); }
    append(play(e, sr * 3));
   }
   const float p = peak(all), level = rms(all.l, 0, all.l.size());
   std::printf("%-9s %d: A %d B %d  peak %.3f  rms %.4f  output %+.1f dB\n", cats[cat], i, static_cast<int>(v[AType]), static_cast<int>(v[BType]), p, level, v[Output]);
   verify(finite(all) && p <= 1.f, "generated sound invalid");
   verify(level > 1e-3f, "generated sound is silent");
   if (!dir.empty()) writeWav(dir + "/enki_" + std::to_string(cat) + "_" + std::to_string(i) + ".wav", all, sr);
  }
}

static void stress() {
 E::Generator g(9);
 E::Values cur = E::defaults();
 cur[Category] = 4;
 auto e = make(cur, 44100);
 std::vector<float> l(512), r(512);
 std::srand(3);
 for (int block = 0; block < 1200; ++block) {
  if (block % 25 == 0) { cur = g.generate(cur, 0); e.setValues(cur); }
  if (block % 5 == 0) e.noteOn(30 + std::rand() % 70, .3f + .7f * static_cast<float>(std::rand()) / RAND_MAX);
  if (block % 7 == 0) e.noteOff(30 + std::rand() % 70);
  if (block % 11 == 0) e.setPitchBend(static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f);
  e.render(l.data(), r.data(), 512);
  for (int i = 0; i < 512; ++i) verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::abs(l[static_cast<std::size_t>(i)]) <= 1.f, "stress output invalid");
 }
 std::puts("random generations under MIDI: pass");
}

int main(int argc, char** argv) {
 loadPool();
 sources();
 generator();
 demos(argc > 1 ? argv[1] : "");
 stress();
 std::puts("Enki tests passed");
}
