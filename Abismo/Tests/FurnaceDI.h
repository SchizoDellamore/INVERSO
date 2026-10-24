#pragma once
// Synthetic DI recordings for FURNACE's tests and demos: Karplus-Strong strings
// with a pick and a pickup, played as a rock riff, a clean arpeggio and a bass
// line (the same parts as IRs/tools/demo_cabs.py). Peaks sit near -6 dBFS,
// like a guitar recorded straight into an interface.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace furnace_di {

struct Rand {
 std::uint64_t s;
 explicit Rand(std::uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 1) {}
 double uniform() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<double>(s >> 11) / 9007199254740992.0; }
};

inline double hz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

// One plucked string. mute 0 (open) .. 1 (palm muted).
inline std::vector<float> string(double fs, double freq, double dur, double vel, double mute, double pick, std::uint64_t seed, bool bass = false) {
 const auto n = static_cast<std::size_t>(dur * fs);
 const double D = fs / freq - .5;
 const auto N = static_cast<std::size_t>(D);
 const double f = D - static_cast<double>(N);
 Rand r(seed);
 std::vector<double> exc(N);
 for (auto& e : exc) e = (r.uniform() * 2 - 1) * vel;
 // brighter for harder picks
 double prev = 0;
 for (auto& e : exc) { const double x = e; e = (.55 + .45 * vel) * x - (.25 + .2 * mute) * prev; prev = x; }
 const std::size_t k = std::max<std::size_t>(1, static_cast<std::size_t>(pick * static_cast<double>(N)));
 std::vector<double> picked(N);
 for (std::size_t i = 0; i < N; ++i) picked[i] = exc[i] - exc[(i + N - k) % N];
 const double t60 = (bass ? 3.0 : 4.0) * (1 - .93 * mute) * std::pow(110.0 / freq, .35);
 const double g = std::pow(10.0, -3.0 / (t60 * fs));
 std::vector<double> y(n + N + 2, 0.0);
 for (std::size_t i = 0; i < N; ++i) y[i + 2] = picked[i];
 for (std::size_t i = N + 2; i < y.size(); ++i) {
  const double a1 = .5 * (y[i - N] + y[i - N - 1]);
  const double a2 = .5 * (y[i - N - 1] + y[i - N - 2]);
  double v = g * ((1 - f) * a1 + f * a2);
  if (mute > 0) v = .7 * v + .3 * .5 * (y[i - N] + y[i - N - 2]) * g;
  y[i] = v;
 }
 std::vector<float> out(n);
 const auto pu = std::max<std::size_t>(1, static_cast<std::size_t>(.13 * fs / freq)); // pickup position
 for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<float>(y[i + 2] - (i >= pu ? .6 * y[i + 2 - pu] : 0.0));
 const auto rel = std::min(n, static_cast<std::size_t>(.015 * fs));
 for (std::size_t i = 0; i < rel; ++i) out[n - rel + i] *= 1.f - static_cast<float>(i) / static_cast<float>(rel);
 return out;
}

inline void place(std::vector<float>& track, const std::vector<float>& s, double t, double fs) {
 const auto i = static_cast<std::size_t>(std::max(0.0, t * fs));
 for (std::size_t k = 0; k < s.size() && i + k < track.size(); ++k) track[i + k] += s[k];
}

inline void normalise(std::vector<float>& x, float peak) {
 float p = 1e-9f;
 for (float v : x) p = std::max(p, std::abs(v));
 for (float& v : x) v *= peak / p;
}

// E5 chugs, G5, A#5-A5, twice, then a ringing chord: about 10 s.
inline std::vector<float> riff(double fs, std::uint64_t seed = 1) {
 const double e = 60.0 / 132 / 2;
 std::vector<float> t(static_cast<std::size_t>((32 * e + 2.6) * fs), 0.f);
 Rand jitter(seed + 99);
 auto chord = [&](int root, double t0, double dur, double mute, double vel, bool fifth = true) {
  const int notes[3] = {root, root + 7, root + 12};
  for (int j = 0; j < (fifth ? 3 : 1); ++j)
   place(t, string(fs, hz(notes[j]), dur + .05, vel * (1 - .1 * j), mute, .15, seed * 1000 + static_cast<std::uint64_t>(t0 * 100) + static_cast<std::uint64_t>(j)),
         t0 + j * .004 + (jitter.uniform() - .5) * .012, fs);
 };
 double bar = 0;
 for (int rep = 0; rep < 2; ++rep) {
  for (int k = 0; k < 6; ++k) chord(40, bar + k * e, e * .95, .85, .9);
  chord(43, bar + 6 * e, e * 2, 0, 1);
  for (int k = 0; k < 6; ++k) chord(40, bar + 8 * e + k * e, e * .95, .85, .9);
  chord(46, bar + 14 * e, e, 0, 1);
  chord(45, bar + 15 * e, e, 0, 1);
  bar += 16 * e;
 }
 chord(40, bar, 2.4, 0, 1);
 chord(47, bar + .004, 2.4, 0, .8, false);
 normalise(t, .5f);
 return t;
}

// Em - C - Am - D arpeggios, about 13 s.
inline std::vector<float> clean(double fs, std::uint64_t seed = 1) {
 const double e = 60.0 / 90 / 2;
 const int seq[4][8] = {{52, 59, 64, 67, 71, 67, 64, 59}, {48, 55, 60, 64, 67, 64, 60, 55}, {45, 52, 57, 60, 64, 60, 57, 52}, {50, 57, 62, 66, 69, 66, 62, 57}};
 std::vector<float> t(static_cast<std::size_t>((32 * e + 3) * fs), 0.f);
 double time = 0;
 for (const auto& bar : seq)
  for (int k = 0; k < 8; ++k) {
   place(t, string(fs, hz(bar[k]), 3.0, k == 0 ? .65 : .55, 0, .2, seed * 999 + static_cast<std::uint64_t>(time * 50)), time, fs);
   time += e;
  }
 normalise(t, .4f);
 return t;
}

// A bass line in E, twice, about 10 s.
inline std::vector<float> bassLine(double fs, std::uint64_t seed = 3) {
 const double e = 60.0 / 104 / 2;
 const int line[16] = {28, 28, 40, 28, 31, 31, 33, 35, 28, 28, 40, 28, 36, 36, 35, 33};
 std::vector<float> t(static_cast<std::size_t>((32 * e + 2) * fs), 0.f);
 double time = 0;
 for (int rep = 0; rep < 2; ++rep)
  for (int k = 0; k < 16; ++k) {
   place(t, string(fs, hz(line[k]), k % 4 == 3 ? e * 1.9 : e * .98, k % 4 == 0 ? 1.0 : .8, .25, .22, seed * 77 + static_cast<std::uint64_t>(k + rep * 31), true), time, fs);
   time += e;
  }
 normalise(t, .5f);
 return t;
}

} // namespace furnace_di
