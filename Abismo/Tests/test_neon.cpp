// NEON: tuning, unison, the arpeggiator on the clock, CPU, every preset sane,
// voice stealing, a parameter storm.
#include "TestUtil.h"
#include "../Source/DSP/NeonEngine.h"
#include <cstdio>
#include <chrono>
#include <random>

namespace N = inverso_abismo::neon;
using test::verify;
using namespace N::P;

constexpr int sr = 48000;
static test::Stereo play(N::Engine& e, int n) {
 test::Stereo s{std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n))};
 for (int i = 0; i < n; i += 256) e.render(s.l.data() + i, s.r.data() + i, std::min(256, n - i));
 return s;
}
static N::Values plain() {
 auto v = N::defaults();
 v[Drift] = 0; v[RevMix] = 0; v[ChorusMode] = 0; v[FilterType] = 1; v[Cutoff] = 20000; v[FilterEnv] = 0; v[Keytrack] = 0; v[FilterVel] = 0; v[FilterDrive] = 0;
 v[Attack] = 1; v[Sustain] = 100; v[Release] = 20; v[Osc1Wave] = 3; v[Output] = 0;
 return v;
}
static double frequency(const std::vector<float>& x, std::size_t from, std::size_t to) {
 int crossings = 0; double first = -1, last = 0;
 for (std::size_t i = from + 1; i < to; ++i)
  if (x[i - 1] <= 0.f && x[i] > 0.f) { const double t = static_cast<double>(i - 1) + x[i - 1] / (x[i - 1] - x[i]); if (first < 0) first = t; last = t; ++crossings; }
 return crossings > 1 ? (crossings - 1) * sr / (last - first) : 0.0;
}
// onsets: where the short-term level jumps up
static std::vector<int> onsets(const std::vector<float>& x) {
 std::vector<int> at;
 const int win = 240;
 float prev = 0;
 for (std::size_t i = 0; i + win < x.size(); i += win) {
  float e = 0; for (int k = 0; k < win; ++k) e = std::max(e, std::abs(x[i + static_cast<std::size_t>(k)]));
  if (e > prev * 2.5f && e > .01f && (at.empty() || static_cast<int>(i) - at.back() > 2000)) at.push_back(static_cast<int>(i));
  prev = e;
 }
 return at;
}

int main() {
 // 1. tuning, both oscillators and the FM carrier
 {
  auto v = plain();
  N::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(69, 1.f);
  auto s = play(e, sr);
  const double f = frequency(s.l, sr / 4, sr);
  std::printf("A4 sine: %.2f Hz\n", f);
  verify(std::abs(f - 440.0) < .5, "oscillator 1 out of tune");
  v[Osc1Level] = 0; v[Osc2Level] = 80; v[Osc2Wave] = 3; v[Osc2Semi] = 7; v[Osc2Fine] = 0;
  N::Engine e2; e2.setValues(v); e2.prepare(sr); e2.noteOn(69, 1.f);
  auto s2 = play(e2, sr);
  const double f2 = frequency(s2.l, sr / 4, sr);
  std::printf("oscillator 2 +7 st: %.2f Hz\n", f2);
  verify(std::abs(f2 - 440.0 * std::pow(2.0, 7.0 / 12.0)) < .8, "oscillator 2 out of tune");
 }
 // 2. unison: wider, about the same level
 {
  auto v = plain(); v[Osc1Wave] = 0;
  N::Engine a; a.setValues(v); a.prepare(sr); a.noteOn(57, 1.f);
  auto one = play(a, sr);
  v[Unison] = 8; v[Detune] = 60; v[Spread] = 100;
  N::Engine b; b.setValues(v); b.prepare(sr); b.noteOn(57, 1.f);
  auto many = play(b, sr);
  double side = 0, mid = 0;
  for (std::size_t i = sr / 4; i < many.l.size(); ++i) { side += std::pow(many.l[i] - many.r[i], 2); mid += std::pow(many.l[i] + many.r[i], 2); }
  const float l1 = test::rms(one.l, sr / 4), l8 = test::rms(many.l, sr / 4);
  std::printf("unison x8: level %.2f dB vs one voice, side/mid %.2f, voices %d\n", 20 * std::log10(l8 / l1), std::sqrt(side / mid), b.activeVoices());
  verify(b.activeVoices() == 8, "unison does not start 8 voices");
  verify(std::abs(20 * std::log10(l8 / l1)) < 4, "unison changes the level too much");
  verify(side / mid > .05, "unison spread is not stereo");
 }
 // 3. arpeggiator: C E G up, 1/16 at 120 BPM = a note every 125 ms, rising
 {
  auto v = plain(); v[Arp] = 1; v[ArpRate] = 3; v[ArpMode] = 0; v[ArpOctaves] = 1; v[Gate] = 50; v[Release] = 5;
  N::Engine e; e.setValues(v); e.prepare(sr); e.setTransport(120, 0, false);
  e.noteOn(60, 1.f); e.noteOn(64, 1.f); e.noteOn(67, 1.f);
  auto s = play(e, sr);
  const auto on = onsets(s.l);
  std::printf("arp: %zu notes in 1 s\n", on.size());
  verify(on.size() >= 7 && on.size() <= 9, "arpeggiator rate is wrong");
  for (std::size_t k = 1; k < on.size(); ++k) verify(std::abs(on[k] - on[k - 1] - 6000) < 400, "arpeggiator steps are uneven");
  const double f0 = frequency(s.l, static_cast<std::size_t>(on[0] + 300), static_cast<std::size_t>(on[0] + 2700));
  const double f1 = frequency(s.l, static_cast<std::size_t>(on[1] + 300), static_cast<std::size_t>(on[1] + 2700));
  const double f2 = frequency(s.l, static_cast<std::size_t>(on[2] + 300), static_cast<std::size_t>(on[2] + 2700));
  std::printf("arp notes: %.1f %.1f %.1f Hz\n", f0, f1, f2);
  verify(std::abs(f0 - 261.6) < 3 && std::abs(f1 - 329.6) < 3 && std::abs(f2 - 392.0) < 3, "arpeggiator does not play C E G up");
  e.noteOff(60); e.noteOff(64); e.noteOff(67);
  play(e, sr / 2);
  auto quiet = play(e, sr / 2);
  verify(test::rms(quiet.l) < 1e-4f, "arpeggiator keeps playing after the keys are released");
 }
 // 4. lightness: an 8-note chord, one voice each, must run far faster than real time
 {
  auto v = N::presetValues(6); // Polysynth/Juno Poly
  N::Engine e; e.setValues(v); e.prepare(sr);
  for (int n : {48, 52, 55, 59, 60, 64, 67, 71}) e.noteOn(n, .8f);
  std::vector<float> l(256), r(256);
  const auto t0 = std::chrono::steady_clock::now();
  for (int b = 0; b < sr * 10 / 256; ++b) e.render(l.data(), r.data(), 256);
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("CPU: 8 voices + effects, 10 s of audio in %.3f s (%.1f%% of one core)\n", seconds, seconds * 10.0);
  verify(seconds < 1.0, "NEON is too heavy");
 }
 // 6. every preset: plays, finite, under the ceiling, sensible level
 for (std::size_t p = 0; p < N::presets().size(); ++p) {
  N::Engine e; e.setValues(N::presetValues(p)); e.prepare(sr); e.setTransport(120, 0, false);
  for (int n : {48, 55, 60, 64}) e.noteOn(n, .8f);
  auto s = play(e, sr * 2);
  for (int n : {48, 55, 60, 64}) e.noteOff(n);
  auto tail = play(e, sr / 2);
  const float level = test::rms(s.l);
  float pk = 0; for (float x : s.l) { verify(std::isfinite(x), "preset output not finite"); pk = std::max(pk, std::abs(x)); }
  std::printf("%-20s rms %.3f (%.1f dBFS) peak %.2f\n", N::presets()[p].name, level, 20 * std::log10(level + 1e-9f), pk);
  verify(level > .01f, "a preset is (almost) silent");
  verify(pk <= 1.001f, "a preset clips past the ceiling");
 }
 // 7. voice stealing: 40 notes at once never exceed the pool
 {
  auto v = plain(); v[Unison] = 4;
  N::Engine e; e.setValues(v); e.prepare(sr);
  for (int n = 30; n < 70; ++n) e.noteOn(n, .5f);
  auto s = play(e, sr / 4);
  verify(e.activeVoices() <= N::Engine::maxVoices, "too many voices");
  for (float x : s.l) verify(std::isfinite(x), "stealing produced invalid output");
 }
 // 8. parameter storm with notes and pattern changes
 {
  N::Engine e; e.prepare(44100); e.setTransport(128, 0, true);
  std::mt19937 rng(5); std::uniform_real_distribution<float> u(0, 1);
  std::vector<float> l(256), r(256);
  for (int b = 0; b < 3000; ++b) {
   N::Values v;
   for (std::size_t i = 0; i < v.size(); ++i) v[i] = N::specs()[i].min + u(rng) * (N::specs()[i].max - N::specs()[i].min);
   v[Output] = -6;
   e.setValues(v);
   if (b % 7 == 0) e.noteOn(36 + static_cast<int>(u(rng) * 48), u(rng));
   if (b % 11 == 0) e.noteOff(36 + static_cast<int>(u(rng) * 48));
   e.setTransport(128, b * 256.0 / 44100 * 128 / 60, true);
   e.render(l.data(), r.data(), 256);
   for (std::size_t i = 0; i < l.size(); ++i) verify(std::isfinite(l[i]) && std::abs(l[i]) <= 1.01f && std::isfinite(r[i]), "unstable under a parameter storm");
  }
 }
 std::puts("neon: all good");
 return 0;
}
