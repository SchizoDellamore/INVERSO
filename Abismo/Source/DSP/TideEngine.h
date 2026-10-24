#pragma once
// TIDE engine: five state-variable EQ bands (Cytomic's trapezoidal SVF, safe
// to modulate every sample), each with its own detector. A dynamic band
// listens to its own part of the spectrum (the band itself; below or above a
// shelf) and, past the threshold, moves its gain by up to its range: a cut
// that only happens when that frequency gets too loud, or a lift that only
// happens on the loud moments. Both channels share the detection, so the
// stereo image stays put. No latency. Portable (no JUCE).

#include "TideParams.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace inverso_abismo::tide {

namespace eq {
constexpr double pi = 3.14159265358979323846;

struct Svf {
 float ic1 = 0.f, ic2 = 0.f;
 float process(float v0, float a1, float a2, float a3, float m0, float m1, float m2) {
  const float v3 = v0 - ic2;
  const float v1 = a1 * ic1 + a2 * v3;
  const float v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.f * v1 - ic1;
  ic2 = 2.f * v2 - ic2;
  return m0 * v0 + m1 * v1 + m2 * v2;
 }
 void reset() { ic1 = ic2 = 0.f; }
};

struct Coefs { float a1 = 1.f, a2 = 0.f, a3 = 0.f, m0 = 1.f, m1 = 0.f, m2 = 0.f; };

// g0 = tan(pi f / fs), computed when the frequency changes.
inline Coefs design(BandShape shape, float g0, float q, float gainDb) {
 Coefs c;
 const float A = std::pow(10.f, gainDb / 40.f);
 float g = g0, k = 1.f / q;
 switch (shape) {
  case BandShape::Bell: k = 1.f / (q * A); c.m0 = 1.f; c.m1 = k * (A * A - 1.f); c.m2 = 0.f; break;
  case BandShape::LowShelf: g = g0 / std::sqrt(A); c.m0 = 1.f; c.m1 = k * (A - 1.f); c.m2 = A * A - 1.f; break;
  case BandShape::HighShelf: g = g0 * std::sqrt(A); c.m0 = A * A; c.m1 = k * (1.f - A) * A; c.m2 = 1.f - A * A; break;
 }
 c.a1 = 1.f / (1.f + g * (g + k));
 c.a2 = g * c.a1;
 c.a3 = g * c.a2;
 return c;
}
inline Coefs cut(bool high, float g0) {
 Coefs c;
 const float k = 1.41421356f; // Butterworth
 c.a1 = 1.f / (1.f + g0 * (g0 + k)); c.a2 = g0 * c.a1; c.a3 = g0 * c.a2;
 if (high) { c.m0 = 0.f; c.m1 = 0.f; c.m2 = 1.f; }  // a low pass removes the highs
 else { c.m0 = 1.f; c.m1 = -k; c.m2 = -1.f; }       // a high pass removes the lows
 return c;
}

// Magnitude (dB) of one band at f, from the analog prototype the SVF is warped from.
inline double bandDb(BandShape shape, double f0, double q, double gainDb, double f) {
 const double A = std::pow(10.0, gainDb / 40.0), sa = std::sqrt(A);
 const std::complex<double> s(0.0, f / f0);
 std::complex<double> h;
 switch (shape) {
  case BandShape::Bell: h = (s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0); break;
  case BandShape::LowShelf: h = A * (s * s + s * (sa / q) + A) / (A * s * s + s * (sa / q) + 1.0); break;
  case BandShape::HighShelf: h = A * (A * s * s + s * (sa / q) + 1.0) / (s * s + s * (sa / q) + A); break;
 }
 return 20.0 * std::log10(std::abs(h));
}
inline double cutDb(bool high, double f0, double f) {
 const std::complex<double> s(0.0, f / f0);
 const std::complex<double> den = s * s + s * 1.41421356 + 1.0;
 return 20.0 * std::log10(std::abs(high ? 1.0 / den : s * s / den));
}
} // namespace eq

class Engine {
public:
 static constexpr int scopeSize = 8192;

 void prepare(double sampleRate) {
  sr = sampleRate > 0 ? sampleRate : 48000.0;
  for (auto& b : bands) { b.l.reset(); b.r.reset(); b.det.reset(); b.env = 0.f; b.lastFreq = -1.f; }
  lowL.reset(); lowR.reset(); highL.reset(); highR.reset();
  scope.assign(scopeSize, 0.f);
  scopeWrite.store(0);
  smoothed = target;
  ready = true;
  // start settled: no fade-in on the first block
  for (int k = 0; k < numBands; ++k) bands[static_cast<std::size_t>(k)].onGain = target[static_cast<std::size_t>(P::band(k, P::On))] >= .5f ? 1.f : 0.f;
  control();
  for (auto& b : bands) b.gainNow = b.gain;
  lowMix = lowTarget; highMix = highTarget;
  countdown = 0;
 }
 void setValues(const Values& v) { target = v; }
 void reset() { for (auto& b : bands) { b.l.reset(); b.r.reset(); b.det.reset(); b.env = 0.f; } lowL.reset(); lowR.reset(); highL.reset(); highR.reset(); }

 void process(float* left, float* right, int n) {
  const bool stereo = right != nullptr && right != left;
  for (int i = 0; i < n; ++i) {
   if (countdown <= 0) { control(); countdown = 16; }
   --countdown;
   float l = left[i], r = stereo ? right[i] : left[i];
   const float mono = .5f * (l + r);
   for (int k = 0; k < numBands; ++k) {
    auto& b = bands[static_cast<std::size_t>(k)];
    float dyn = 0.f;
    if (b.dynamic) {
     // the band's own part of the spectrum, followed peak by peak
     const float heard = b.det.process(mono, b.dc.a1, b.dc.a2, b.dc.a3, b.dc.m0, b.dc.m1, b.dc.m2);
     const float a = std::abs(heard);
     b.env += (a > b.env ? attackCoef : releaseCoef) * (a - b.env);
     const float level = 20.f * std::log10(b.env + 1e-9f);
     const float over = level - b.thresh;
     if (over > 0.f) dyn = b.range < 0.f ? std::max(b.range, -.8f * over) : std::min(b.range, .8f * over);
    } else b.env = 0.f;
    b.dynNow += .2f * (dyn - b.dynNow);
    b.gainNow += glide * (b.gain - b.gainNow); // the control rate never shows as steps
    const float gainDb = b.gainNow + b.dynNow;
    if (std::abs(gainDb - b.lastGain) > 1e-4f || b.dirty) { b.c = eq::design(b.shape, b.g0, b.q, gainDb); b.lastGain = gainDb; b.dirty = false; }
    const auto& c = b.c;
    l = b.l.process(l, c.a1, c.a2, c.a3, c.m0, c.m1, c.m2);
    if (stereo) r = b.r.process(r, c.a1, c.a2, c.a3, c.m0, c.m1, c.m2);
   }
   // the cuts always run, so switching one in crossfades from a filter that is already settled
   lowMix += cutStep * (lowTarget - lowMix);
   highMix += cutStep * (highTarget - highMix);
   {
    const float hl = lowL.process(l, lowC.a1, lowC.a2, lowC.a3, lowC.m0, lowC.m1, lowC.m2);
    const float hr = stereo ? lowR.process(r, lowC.a1, lowC.a2, lowC.a3, lowC.m0, lowC.m1, lowC.m2) : hl;
    l += lowMix * (hl - l); r += lowMix * (hr - r);
   }
   {
    const float hl = highL.process(l, highC.a1, highC.a2, highC.a3, highC.m0, highC.m1, highC.m2);
    const float hr = stereo ? highR.process(r, highC.a1, highC.a2, highC.a3, highC.m0, highC.m1, highC.m2) : hl;
    l += highMix * (hl - l); r += highMix * (hr - r);
   }
   l *= outGain; r *= outGain;
   left[i] = l;
   if (stereo) right[i] = r;
   // for the analyser
   const int w = scopeWrite.load(std::memory_order_relaxed);
   scope[static_cast<std::size_t>(w)] = .5f * (l + r);
   scopeWrite.store((w + 1) % scopeSize, std::memory_order_relaxed);
  }
  for (int k = 0; k < numBands; ++k) {
   const auto& b = bands[static_cast<std::size_t>(k)];
   dynamicNow[static_cast<std::size_t>(k)].store(b.dynNow, std::memory_order_relaxed);
   levelNow[static_cast<std::size_t>(k)].store(b.dynamic ? 20.f * std::log10(b.env + 1e-9f) : -120.f, std::memory_order_relaxed);
  }
 }

 // For the editor.
 std::array<std::atomic<float>, numBands> dynamicNow{}, levelNow{};
 std::vector<float> scope;
 std::atomic<int> scopeWrite{0};

 // The whole curve at f (dB), static part plus the given dynamic gains.
 static double responseDb(const Values& v, const std::array<float, numBands>& dynamic, double f) {
  double db = 0;
  for (int k = 0; k < numBands; ++k) {
   if (v[static_cast<std::size_t>(P::band(k, P::On))] < .5f) continue;
   const double g = v[static_cast<std::size_t>(P::band(k, P::Gain))] + dynamic[static_cast<std::size_t>(k)];
   if (std::abs(g) < 1e-4) continue;
   db += eq::bandDb(shapeOf(k), v[static_cast<std::size_t>(P::band(k, P::Freq))], v[static_cast<std::size_t>(P::band(k, P::Q))], g, f);
  }
  if (v[P::LowCutOn] >= .5f) db += eq::cutDb(false, v[P::LowCut], f);
  if (v[P::HighCutOn] >= .5f) db += eq::cutDb(true, v[P::HighCut], f);
  return db + v[P::Output];
 }

private:
 struct Band {
  eq::Svf l, r, det;
  eq::Coefs dc, c;
  BandShape shape = BandShape::Bell;
  float g0 = .01f, q = 1.f, gain = 0.f, thresh = -60.f, range = 0.f, env = 0.f, dynNow = 0.f, lastFreq = -1.f, lastGain = 0.f, gainNow = 0.f;
  bool dynamic = false, dirty = true;
 };

 void control() {
  if (!ready) return;
  for (std::size_t i = 0; i < smoothed.size(); ++i) {
   if (specs()[i].kind == Kind::Float) smoothed[i] += .25f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  const auto& s = smoothed;
  const float nyq = static_cast<float>(sr) * .45f;
  const float onStep = 1.f - std::exp(-16.f / (.015f * static_cast<float>(sr)));
  for (int k = 0; k < numBands; ++k) {
   auto& b = bands[static_cast<std::size_t>(k)];
   const bool on = s[static_cast<std::size_t>(P::band(k, P::On))] >= .5f;
   b.shape = shapeOf(k);
   const float f = std::min(s[static_cast<std::size_t>(P::band(k, P::Freq))], nyq);
   if (std::abs(f - b.lastFreq) > 1e-3f) { b.g0 = static_cast<float>(std::tan(eq::pi * f / sr)); b.lastFreq = f; b.dirty = true; }
   const float q = s[static_cast<std::size_t>(P::band(k, P::Q))];
   if (std::abs(q - b.q) > 1e-6f) b.dirty = true;
   b.q = q;
   b.onGain += onStep * ((on ? 1.f : 0.f) - b.onGain); // ~15 ms, a band fades in and out
   b.gain = s[static_cast<std::size_t>(P::band(k, P::Gain))] * b.onGain;
   b.thresh = s[static_cast<std::size_t>(P::band(k, P::Thresh))];
   b.range = s[static_cast<std::size_t>(P::band(k, P::Range))] * b.onGain;
   b.dynamic = std::abs(b.range) > .01f;
   // the detector: the band itself for a bell (a little wider than it), below
   // a low shelf, above a high shelf
   const float dq = std::max(.7f, b.q * .8f), dk = 1.f / dq;
   b.dc.a1 = 1.f / (1.f + b.g0 * (b.g0 + dk)); b.dc.a2 = b.g0 * b.dc.a1; b.dc.a3 = b.g0 * b.dc.a2;
   switch (b.shape) {
    case BandShape::Bell: b.dc.m0 = 0.f; b.dc.m1 = dk; b.dc.m2 = 0.f; break;      // band pass, unity peak
    case BandShape::LowShelf: b.dc.m0 = 0.f; b.dc.m1 = 0.f; b.dc.m2 = 1.f; break; // low pass
    case BandShape::HighShelf: b.dc.m0 = 1.f; b.dc.m1 = -dk; b.dc.m2 = -1.f; break; // high pass
   }
  }
  lowTarget = s[P::LowCutOn] >= .5f ? 1.f : 0.f;
  highTarget = s[P::HighCutOn] >= .5f ? 1.f : 0.f;
  cutStep = 1.f - std::exp(-1.f / (.012f * static_cast<float>(sr)));
  lowC = eq::cut(false, static_cast<float>(std::tan(eq::pi * std::min(s[P::LowCut], nyq) / sr)));
  highC = eq::cut(true, static_cast<float>(std::tan(eq::pi * std::min(s[P::HighCut], nyq) / sr)));
  attackCoef = 1.f - std::exp(-1.f / (s[P::Attack] * .001f * static_cast<float>(sr)));
  releaseCoef = 1.f - std::exp(-1.f / (s[P::Release] * .001f * static_cast<float>(sr)));
  outGain = std::pow(10.f, s[P::Output] / 20.f);
  glide = 1.f - std::exp(-1.f / (.003f * static_cast<float>(sr)));
 }

 struct BandState : Band { float onGain = 1.f; };
 double sr = 48000;
 std::array<BandState, numBands> bands{};
 eq::Svf lowL, lowR, highL, highR;
 eq::Coefs lowC, highC;
 float lowMix = 0.f, highMix = 0.f, lowTarget = 0.f, highTarget = 0.f, cutStep = .002f, glide = .007f, attackCoef = .01f, releaseCoef = .001f, outGain = 1.f;
 Values target = defaults(), smoothed = defaults();
 int countdown = 0;
 bool ready = false;
};

} // namespace inverso_abismo::tide
