// TIDE: flat when flat, static bands that match the drawn curve, a dynamic cut
// that only acts above its threshold, a dynamic lift, no clicks when a band
// switches or moves, the cuts, every preset finite, a parameter storm.
#include "TestUtil.h"
#include "../Source/DSP/TideEngine.h"
#include <cstdio>
#include <random>

using namespace inverso_abismo::tide;
using test::verify;

constexpr double fs = 48000;

static std::vector<float> sine(double f, float amp, std::size_t n) {
 std::vector<float> x(n);
 for (std::size_t i = 0; i < n; ++i) x[i] = amp * static_cast<float>(std::sin(2 * 3.14159265358979 * f * static_cast<double>(i) / fs));
 return x;
}
static void run(Engine& e, std::vector<float>& l, std::vector<float>* r = nullptr, int block = 317) {
 for (std::size_t i = 0; i < l.size(); i += static_cast<std::size_t>(block)) {
  const int n = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(block), l.size() - i));
  e.process(l.data() + i, r ? r->data() + i : nullptr, n);
 }
}
// gain (dB) of the engine for a sine at f, measured on the settled half
static double gainAt(const Values& v, double f, float amp = .1f) {
 Engine e; e.setValues(v); e.prepare(fs);
 const std::size_t n = 48000;
 auto x = sine(f, amp, n);
 const auto in = x;
 run(e, x);
 return 20 * std::log10(test::rms(x, n / 2) / test::rms(in, n / 2));
}
static float maxStep(const std::vector<float>& x, std::size_t from = 1) {
 float m = 0;
 for (std::size_t i = std::max<std::size_t>(from, 2); i < x.size(); ++i) m = std::max(m, std::abs(x[i] - 2 * x[i - 1] + x[i - 2]));
 return m;
}

int main() {
 using namespace P;
 // 1. flat by default
 for (double f : {40.0, 300.0, 2000.0, 12000.0}) verify(std::abs(gainAt(defaults(), f)) < .05, "the default setting is not flat");

 // 2. static bands follow the curve the editor draws
 {
  Values v = defaults();
  v[band(0, Gain)] = 6; v[band(0, Freq)] = 100;
  v[band(2, Gain)] = -9; v[band(2, Freq)] = 1000; v[band(2, Q)] = 2;
  v[band(4, Gain)] = 4; v[band(4, Freq)] = 8000;
  const std::array<float, numBands> none{};
  for (double f : {50.0, 100.0, 400.0, 900.0, 1000.0, 1200.0, 3000.0, 8000.0, 14000.0}) {
   const double got = gainAt(v, f), want = Engine::responseDb(v, none, f);
   if (std::abs(got - want) > .35) std::fprintf(stderr, "f %.0f got %.2f want %.2f\n", f, got, want);
   verify(std::abs(got - want) < .35, "a static band does not match its drawn curve");
  }
  verify(gainAt(v, 1000) < -8.5, "the bell does not reach its gain");
 }

 // 3. a dynamic cut: nothing below the threshold, the cut above it
 {
  Values v = defaults();
  v[band(2, Freq)] = 2500; v[band(2, Q)] = 2; v[band(2, Thresh)] = -30; v[band(2, Range)] = -8;
  const double quiet = gainAt(v, 2500, .005f);   // -46 dBFS peak: under
  const double loud = gainAt(v, 2500, .5f);      // -6 dBFS: well over
  const double other = gainAt(v, 300, .5f);      // loud, but not in the band
  std::printf("dynamic cut: quiet %.2f loud %.2f elsewhere %.2f\n", quiet, loud, other);
  verify(std::abs(quiet) < .1, "the dynamic band acted below its threshold");
  verify(loud < -7.0 && loud > -8.3, "the dynamic band did not cut by its range when loud");
  verify(std::abs(other) < .6, "the dynamic band reacted to another part of the spectrum");
 }

 // 4. a dynamic lift
 {
  Values v = defaults();
  v[band(4, Freq)] = 5000; v[band(4, Thresh)] = -30; v[band(4, Range)] = 6;
  const double quiet = gainAt(v, 10000, .005f), loud = gainAt(v, 10000, .5f);
  std::printf("dynamic lift: quiet %.2f loud %.2f\n", quiet, loud);
  verify(std::abs(quiet) < .1 && loud > 5.0, "the dynamic shelf did not lift on the loud part only");
 }

 // 5. dynamic band follows a level change without clicking
 {
  Values v = defaults();
  v[band(1, Freq)] = 200; v[band(1, Thresh)] = -24; v[band(1, Range)] = -10; v[Attack] = .5f; v[Release] = 10;
  Engine e; e.setValues(v); e.prepare(fs);
  auto x = sine(200, .02f, 96000);
  for (std::size_t i = 24000; i < 72000; ++i) x[i] *= 25.f; // a sudden jump to -6 dBFS
  std::vector<float> ref = x;
  run(e, x);
  // the steepest second difference of the output vs the input's own jump
  const float out = maxStep(x, 100), in = maxStep(ref, 100);
  std::printf("dynamic step: in %.4f out %.4f\n", in, out);
  verify(out < in * 1.1f, "the dynamic band clicks when the level jumps");
 }

 // 6. switching a band on and off, moving it, and the cuts: no clicks
 {
  Engine e; Values v = defaults();
  v[band(2, Gain)] = 12; v[band(2, Freq)] = 400; v[band(2, Q)] = 3;
  e.setValues(v); e.prepare(fs);
  auto x = sine(400, .3f, 4 * 48000);
  const float baseline = maxStep(sine(400, .3f * 4, 4800));
  for (std::size_t i = 0; i < x.size(); i += 256) {
   const int step = static_cast<int>(i / 9600);
   v[band(2, On)] = step % 2 ? 0.f : 1.f;
   v[band(2, Freq)] = step % 3 ? 400.f : 520.f;
   v[LowCutOn] = step % 4 < 2 ? 1.f : 0.f; v[LowCut] = 200;
   v[HighCutOn] = step % 5 < 2 ? 1.f : 0.f; v[HighCut] = 2000;
   e.setValues(v);
   e.process(x.data() + i, nullptr, static_cast<int>(std::min<std::size_t>(256, x.size() - i)));
  }
  const float worst = maxStep(x, 2000);
  std::printf("switching: worst %.4f baseline %.4f\n", worst, baseline);
  verify(worst < baseline * 1.5f, "switching or moving a band clicks");
 }

 // 7. the cuts cut
 {
  Values v = defaults();
  v[LowCutOn] = 1; v[LowCut] = 200; v[HighCutOn] = 1; v[HighCut] = 4000;
  verify(gainAt(v, 50) < -20 && gainAt(v, 16000) < -20, "the cuts do not cut");
  verify(std::abs(gainAt(v, 1000)) < .5, "the cuts hurt the middle");
 }

 // 8. stereo stays linked, mono equals left
 {
  Values v = presetValues(2);
  Engine e; e.setValues(v); e.prepare(fs);
  std::mt19937 rng(3); std::normal_distribution<float> g(0, .2f);
  std::vector<float> l(48000), r(48000);
  for (std::size_t i = 0; i < l.size(); ++i) l[i] = r[i] = g(rng);
  run(e, l, &r);
  for (std::size_t i = 0; i < l.size(); ++i) verify(l[i] == r[i], "identical channels came out different");
 }

 // 9. every preset finite and sensible in level; silence stays silent
 for (std::size_t p = 0; p < presets().size(); ++p) {
  Engine e; e.setValues(presetValues(p)); e.prepare(fs);
  std::mt19937 rng(static_cast<unsigned>(p)); std::normal_distribution<float> g(0, .1f);
  std::vector<float> x(48000);
  for (auto& s : x) s = g(rng);
  const float before = test::rms(x);
  run(e, x);
  const float db = 20 * std::log10(test::rms(x) / before);
  std::printf("%-24s %+.2f dB\n", presets()[p].name, db);
  for (float s : x) verify(std::isfinite(s), "a preset is unstable");
  verify(std::abs(db) < 6, "a preset changes the level too much");
  std::vector<float> quiet(4800, 0.f);
  e.reset(); run(e, quiet);
  for (float s : quiet) verify(std::abs(s) < 1e-6f, "silence in, sound out");
 }

 // 10. parameter storm
 {
  Engine e; e.prepare(44100);
  std::mt19937 rng(11); std::uniform_real_distribution<float> u(0, 1);
  std::vector<float> l(256), r(256);
  for (int b = 0; b < 3000; ++b) {
   Values v;
   for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].min + u(rng) * (specs()[i].max - specs()[i].min);
   e.setValues(v);
   for (std::size_t i = 0; i < l.size(); ++i) { l[i] = (u(rng) - .5f) * 2; r[i] = (u(rng) - .5f) * 2; }
   e.process(l.data(), r.data(), 256);
   for (std::size_t i = 0; i < l.size(); ++i) verify(std::isfinite(l[i]) && std::abs(l[i]) < 100 && std::isfinite(r[i]), "unstable under a parameter storm");
  }
 }
 std::puts("tide: all good");
 return 0;
}
