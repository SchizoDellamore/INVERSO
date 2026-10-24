#pragma once
// Building blocks shared by INVERSO ABISMO (synth) and ABISMO SAMPLER.
// Portable C++17, no JUCE, nothing here allocates.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace inverso_abismo {

constexpr float twoPi = 6.28318530718f;
constexpr int controlInterval = 16;

namespace detail {
inline float polyBlep(float t, float dt) {
 if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
 if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
 return 0.f;
}
inline float sine(float phase) {
 static const auto table = [] {
  std::array<float, 2049> t{};
  for (std::size_t i = 0; i < t.size(); ++i) t[i] = std::sin(twoPi * static_cast<float>(i) / 2048.f);
  return t;
 }();
 const float pos = phase * 2048.f;
 const auto i = std::min(static_cast<std::size_t>(pos), std::size_t{2047});
 const float frac = pos - static_cast<float>(i);
 return table[i] + frac * (table[i + 1] - table[i]);
}
inline float softTanh(float x) {
 x = std::clamp(x, -3.f, 3.f);
 return x * (27.f + x * x) / (27.f + 9.f * x * x);
}
inline float semitoneRatio(float st) { return std::exp2(st / 12.f); }
inline float msToCoef(float ms, double sr, double timeConstants = 4.6) {
 // One-pole coefficient: 4.6 time constants = 99% of the way after `ms`.
 const double samples = std::max(1.0, static_cast<double>(ms) * .001 * sr);
 return static_cast<float>(1.0 - std::exp(-timeConstants / samples));
}
struct Random {
 std::uint32_t state = 0x9E3779B9u;
 float next() { // uniform [-1, 1)
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return static_cast<float>(state) * (2.f / 4294967296.f) - 1.f;
 }
};
} // namespace detail

class Envelope {
public:
 enum class Stage { Idle, Attack, Decay, Sustain, Release };
 void set(float a, float d, float s, float r, double sr) {
  attackStep = static_cast<float>(1.0 / std::max(1.0, static_cast<double>(a) * .001 * sr));
  decayCoef = detail::msToCoef(d, sr);
  releaseCoef = detail::msToCoef(r, sr, 9.2); // -80 dB at the release time, then the voice frees
  sustain = std::clamp(s, 0.f, 1.f);
 }
 void copySettings(const Envelope& o) {
  attackStep = o.attackStep; decayCoef = o.decayCoef; releaseCoef = o.releaseCoef; sustain = o.sustain;
 }
 void gateOn() { stage = Stage::Attack; } // restarts from the current level: no click on retrigger
 void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
 void reset() { stage = Stage::Idle; level = 0.f; }
 bool active() const { return stage != Stage::Idle; }
 float next() {
  switch (stage) {
   case Stage::Idle: return 0.f;
   case Stage::Attack:
    level += attackStep;
    if (level >= 1.f) { level = 1.f; stage = Stage::Decay; }
    break;
   case Stage::Decay:
    level += decayCoef * (sustain - level);
    if (std::abs(level - sustain) < 1e-4f) { level = sustain; stage = Stage::Sustain; }
    break;
   case Stage::Sustain: level = sustain; break;
   case Stage::Release:
    level += releaseCoef * (0.f - level);
    if (level < 1e-4f) { level = 0.f; stage = Stage::Idle; }
    break;
  }
  return level;
 }
 float value() const { return level; }
 Stage stage = Stage::Idle;
private:
 float level = 0.f, attackStep = .01f, decayCoef = .01f, releaseCoef = .01f, sustain = 1.f;
};

// Topology-preserving-transform SVF (Zavalishin). Stable under fast modulation.
struct SVF {
 float ic1 = 0.f, ic2 = 0.f;
 float process(float v0, float a1, float a2, float a3, float k, int mode) {
  const float v3 = v0 - ic2;
  const float v1 = a1 * ic1 + a2 * v3;
  const float v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.f * v1 - ic1;
  ic2 = 2.f * v2 - ic2;
  if (mode == 1) return v1 * k;           // band-pass, unity peak
  if (mode == 2) return v0 - k * v1 - v2; // high-pass
  return v2;                              // low-pass
 }
 // All three outputs from one step (to crossfade between them).
 void processAll(float v0, float a1, float a2, float a3, float k, float& lp, float& bp, float& hp) {
  const float v3 = v0 - ic2;
  const float v1 = a1 * ic1 + a2 * v3;
  const float v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.f * v1 - ic1;
  ic2 = 2.f * v2 - ic2;
  lp = v2; bp = v1 * k; hp = v0 - k * v1 - v2;
 }
 void reset() { ic1 = ic2 = 0.f; }
};

// Two-pole resonator (one vibration mode), energy-normalised for noise or
// impulse excitation.
struct Resonator {
 float y1 = 0.f, y2 = 0.f, a1 = 0.f, a2 = 0.f, gain = 0.f;
 void set(float freq, float t60, double sr) {
  const float w = twoPi * freq / static_cast<float>(sr);
  if (w >= 2.9f || w <= 0.f) { gain = 0.f; return; }
  const float r = static_cast<float>(std::exp(-6.9078 / (std::max(.01, static_cast<double>(t60)) * sr)));
  a1 = 2.f * r * std::cos(w);
  a2 = -r * r;
  // Equal-power normalisation for noise/impulse excitation of an AR(2) system.
  const float r2 = r * r;
  const float variance = (1.f + r2) / ((1.f - r2) * ((1.f + r2) * (1.f + r2) - a1 * a1));
  gain = variance > 0.f ? 1.f / std::sqrt(variance) : 0.f;
 }
 float process(float x) {
  const float y = gain * x + a1 * y1 + a2 * y2;
  y2 = y1; y1 = y;
  return y;
 }
 void reset() { y1 = y2 = 0.f; }
};

// Three band-passes on bass-voice formants, morphed A -> E -> I -> O -> U.
class FormantFilter {
public:
 void set(float vowelPos, float ratio, double sr) {
  static const float freq[5][3] = {{600, 1040, 2250}, {400, 1620, 2400}, {250, 1750, 2600}, {400, 750, 2400}, {350, 600, 2400}};
  static const float amp[5][3] = {{1.f, .45f, .35f}, {1.f, .25f, .35f}, {1.f, .03f, .16f}, {1.f, .28f, .09f}, {1.f, .1f, .025f}};
  static const float bandwidth[3] = {80.f, 90.f, 120.f};
  const float pos = std::clamp(vowelPos, 0.f, 1.f) * 4.f;
  const int a = std::min(static_cast<int>(pos), 3);
  const float t = pos - static_cast<float>(a);
  for (std::size_t i = 0; i < 3; ++i) {
   const float f = std::clamp((freq[a][i] + t * (freq[a + 1][i] - freq[a][i])) * ratio, 60.f, .45f * static_cast<float>(sr));
   const float g = std::tan(3.14159265f * f / static_cast<float>(sr));
   const float kk = std::clamp(bandwidth[i] * ratio / f, .02f, 2.f);
   auto& co = bands[i];
   co.k = kk;
   co.a1 = 1.f / (1.f + g * (g + kk));
   co.a2 = g * co.a1;
   co.a3 = g * co.a2;
   co.gain = amp[a][i] + t * (amp[a + 1][i] - amp[a][i]);
  }
 }
 void process(float& l, float& r, float mix) {
  float fl = 0.f, fr = 0.f;
  for (std::size_t f = 0; f < 3; ++f) {
   const auto& co = bands[f];
   fl += co.gain * left[f].process(l, co.a1, co.a2, co.a3, co.k, 1);
   fr += co.gain * right[f].process(r, co.a1, co.a2, co.a3, co.k, 1);
  }
  // Narrow band-passes remove most of the energy of a broad source: make up for it.
  l += mix * (fl * 3.f - l);
  r += mix * (fr * 3.f - r);
 }
 void reset() { for (auto& f : left) f.reset(); for (auto& f : right) f.reset(); }
private:
 struct Coefficients { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f, gain = 0.f; };
 std::array<Coefficients, 3> bands{};
 std::array<SVF, 3> left{}, right{};
};

// Soft ceiling: linear below 0.9, smoothly saturating to never exceed 1.0.
inline float softCeiling(float x) {
 if (!std::isfinite(x)) return 0.f;
 const float a = std::abs(x);
 if (a <= .9f) return x;
 const float y = .9f + .1f * std::tanh((a - .9f) * 10.f);
 return x < 0.f ? -y : y;
}

} // namespace inverso_abismo
