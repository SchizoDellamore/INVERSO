#pragma once
#include "../Source/DSP/AbismoEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace test {
using namespace inverso_abismo;

inline void verify(bool ok, const char* message) {
 if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); }
}

struct Stereo { std::vector<float> l, r; };

inline Stereo render(Engine& e, int samples, int block = 256) {
 Stereo out{std::vector<float>(static_cast<std::size_t>(samples)), std::vector<float>(static_cast<std::size_t>(samples))};
 for (int i = 0; i < samples; i += block) {
  const int n = std::min(block, samples - i);
  e.render(out.l.data() + i, out.r.data() + i, n);
 }
 return out;
}

inline float peak(const Stereo& s) {
 float p = 0.f;
 for (std::size_t i = 0; i < s.l.size(); ++i) p = std::max({p, std::abs(s.l[i]), std::abs(s.r[i])});
 return p;
}
inline float rms(const std::vector<float>& x, std::size_t from = 0, std::size_t to = 0) {
 if (to == 0 || to > x.size()) to = x.size();
 double sum = 0.0;
 for (std::size_t i = from; i < to; ++i) sum += static_cast<double>(x[i]) * x[i];
 return to > from ? static_cast<float>(std::sqrt(sum / static_cast<double>(to - from))) : 0.f;
}
inline bool finite(const Stereo& s) {
 for (std::size_t i = 0; i < s.l.size(); ++i) if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
 return true;
}
// Frequency from rising zero crossings with linear interpolation.
inline double frequency(const std::vector<float>& x, double sr, std::size_t from, std::size_t to) {
 double first = -1, last = -1; int count = 0;
 for (std::size_t i = from + 1; i < to; ++i) {
  if (x[i - 1] < 0.f && x[i] >= 0.f) {
   const double t = static_cast<double>(i - 1) + x[i - 1] / static_cast<double>(x[i - 1] - x[i]);
   if (first < 0) first = t; else ++count;
   last = t;
  }
 }
 return count > 0 ? sr * count / (last - first) : 0.0;
}
inline double goertzel(const std::vector<float>& x, double sr, double f, std::size_t from, std::size_t to) {
 const double w = 2.0 * 3.14159265358979 * f / sr, c = 2.0 * std::cos(w);
 double s1 = 0, s2 = 0;
 for (std::size_t i = from; i < to; ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
 return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / static_cast<double>(to - from);
}

// A plain, deterministic test patch: sine, one voice, no drift/LFO/reverb.
inline Values pureSine() {
 Values v = defaults();
 v[P::OscShape] = 0; v[P::OscLevel] = 100; v[P::Unison] = 0; v[P::Detune] = 0; v[P::Drift] = 0;
 v[P::Cutoff] = 20000; v[P::FilterEnv] = 0; v[P::Keytrack] = 0; v[P::Resonance] = 0;
 v[P::RevMix] = 0; v[P::Attack] = 5; v[P::Sustain] = 100; v[P::Release] = 50; v[P::Output] = 0;
 return v;
}

inline void writeWav(const std::string& path, const Stereo& s, int sr) {
 std::ofstream f(path, std::ios::binary);
 auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
 auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
 const auto frames = static_cast<std::uint32_t>(s.l.size());
 f.write("RIFF", 4); u32(36 + frames * 4); f.write("WAVEfmt ", 8);
 u32(16); u16(1); u16(2); u32(static_cast<std::uint32_t>(sr)); u32(static_cast<std::uint32_t>(sr) * 4); u16(4); u16(16);
 f.write("data", 4); u32(frames * 4);
 for (std::uint32_t i = 0; i < frames; ++i) {
  for (float x : {s.l[i], s.r[i]}) {
   const auto v = static_cast<std::int16_t>(std::lround(std::clamp(x, -1.f, 1.f) * 32767.f));
   f.write(reinterpret_cast<const char*>(&v), 2);
  }
 }
}
} // namespace test
