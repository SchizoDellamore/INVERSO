#pragma once
// CLEAR engine. A short-time Fourier transform (2048 points at 44.1/48 kHz,
// 4096 above, 75% overlap, square-root Hann windows: perfect reconstruction
// when nothing is taken away) and, per frame:
//  - HARSHNESS: in its range, every frequency is compared with its own
//    neighbourhood (an average over a fraction of an octave, set by SHARPNESS);
//    what sticks out more than the sensitivity allows is turned down, by up to
//    DEPTH. Resonances, whistles and ringing go; a flat bright sound stays.
//  - MUD: the low-mid band around FOCUS is weighed against the rest of the
//    spectrum (both pink-corrected, so a normal mix reads level); when it
//    builds up past the sensitivity, a broad dip takes it back, up to DEPTH.
// Both channels share the same gains (from their summed power), so the stereo
// image does not move. Reductions attack within a frame and release at SPEED.
// Portable (no JUCE).

#include "ClearParams.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace inverso_abismo::clear {

namespace dsp {
constexpr double pi = 3.14159265358979323846;

// Radix-2 complex FFT with precomputed twiddles.
class FFT {
public:
 void prepare(std::size_t n) {
  size = n;
  rev.assign(n, 0);
  for (std::size_t i = 0, j = 0; i < n; ++i) {
   rev[i] = j;
   std::size_t bit = n >> 1;
   for (; j & bit; bit >>= 1) j ^= bit;
   j ^= bit;
  }
  tw.resize(n / 2);
  for (std::size_t i = 0; i < n / 2; ++i) tw[i] = std::polar(1.f, static_cast<float>(-2 * pi * static_cast<double>(i) / static_cast<double>(n)));
 }
 void forward(std::vector<std::complex<float>>& a) const { run(a, false); }
 void inverse(std::vector<std::complex<float>>& a) const {
  run(a, true);
  const float k = 1.f / static_cast<float>(size);
  for (auto& x : a) x *= k;
 }
private:
 void run(std::vector<std::complex<float>>& a, bool inv) const {
  for (std::size_t i = 0; i < size; ++i) if (i < rev[i]) std::swap(a[i], a[rev[i]]);
  for (std::size_t len = 2; len <= size; len <<= 1) {
   const std::size_t step = size / len, half = len / 2;
   for (std::size_t i = 0; i < size; i += len)
    for (std::size_t j = 0; j < half; ++j) {
     const auto w = inv ? std::conj(tw[j * step]) : tw[j * step];
     const auto u = a[i + j], v = a[i + j + half] * w;
     a[i + j] = u + v;
     a[i + j + half] = u - v;
    }
  }
 }
 std::size_t size = 0;
 std::vector<std::size_t> rev;
 std::vector<std::complex<float>> tw;
};
} // namespace dsp

class Engine {
public:
 static constexpr int displayBins = 128;
 static constexpr float displayLow = 30.f, displayHigh = 18000.f;

 void prepare(double sampleRate) {
  sr = sampleRate;
  N = sampleRate > 70000 ? 4096 : 2048;
  hop = N / 4;
  fft.prepare(N);
  window.resize(N);
  for (std::size_t n = 0; n < N; ++n) window[n] = static_cast<float>(std::sqrt(.5 - .5 * std::cos(2 * dsp::pi * static_cast<double>(n) / static_cast<double>(N))));
  for (auto& ch : channels) { ch.in.assign(N, 0.f); ch.out.assign(N, 0.f); ch.spec.assign(N, {}); }
  const std::size_t bins = N / 2 + 1;
  power.assign(bins, 0.f); detect.assign(bins, 0.f); db.assign(bins, 0.f);
  prefix.assign(bins + 1, 0.0);
  detectCoef = static_cast<float>(1.0 - std::exp(-static_cast<double>(hop) / sampleRate / .03)); // ~30 ms
  target.assign(bins, 0.f); reduction.assign(bins, 0.f); smoothTmp.assign(bins, 0.f); gain.assign(bins, 1.f);
  freq.resize(bins);
  for (std::size_t k = 0; k < bins; ++k) freq[k] = static_cast<float>(static_cast<double>(k) * sr / static_cast<double>(N));
  displayLevel.fill(-120.f);
  pos = 0; hopCount = 0;
 }
 int latency() const { return static_cast<int>(N); }
 void setValues(const Values& v) { values = v; }
 void reset() {
  for (auto& ch : channels) { std::fill(ch.in.begin(), ch.in.end(), 0.f); std::fill(ch.out.begin(), ch.out.end(), 0.f); }
  std::fill(reduction.begin(), reduction.end(), 0.f);
  std::fill(detect.begin(), detect.end(), 0.f);
  pos = 0; hopCount = 0;
 }

 // In place; right may equal left (mono) or be null.
 void process(float* left, float* right, int n) {
  const bool stereo = right != nullptr && right != left;
  for (int i = 0; i < n; ++i) {
   auto& L = channels[0];
   auto& R = channels[1];
   L.in[pos] = left[i];
   R.in[pos] = stereo ? right[i] : left[i];
   const float yl = L.out[pos], yr = R.out[pos];
   L.out[pos] = 0.f; R.out[pos] = 0.f;
   left[i] = yl;
   if (stereo) right[i] = yr;
   pos = (pos + 1) % N;
   if (++hopCount == hop) { hopCount = 0; frame(stereo); }
  }
 }

 // For the editor: input level (dB) and reduction (dB, positive) on a log axis.
 std::array<std::atomic<float>, displayBins> displayInput{}, displayReduction{};
 std::atomic<float> mudNow{0.f}, harshNow{0.f}; // the largest reduction this frame (dB)

private:
 struct Channel { std::vector<float> in, out; std::vector<std::complex<float>> spec; };

 void frame(bool stereo) {
  const std::size_t bins = N / 2 + 1;
  const int used = stereo ? 2 : 1;
  for (int c = 0; c < used; ++c) {
   auto& ch = channels[static_cast<std::size_t>(c)];
   for (std::size_t k = 0; k < N; ++k) ch.spec[k] = {ch.in[(pos + k) % N] * window[k], 0.f};
   fft.forward(ch.spec);
  }
  for (std::size_t k = 0; k < bins; ++k) {
   float p = std::norm(channels[0].spec[k]);
   if (stereo) p += std::norm(channels[1].spec[k]);
   power[k] = p;
  }
  analyse();
  // apply: the wet/dry mix and the delta (what is taken away) live in the gain
  const float mix = values[P::Mix] * .01f;
  const bool delta = values[P::Delta] >= .5f;
  const float out = std::pow(10.f, values[P::Output] / 20.f) * .5f; // .5: sqrt-Hann at 75% overlap sums to 2
  for (std::size_t k = 0; k < bins; ++k) {
   const float g = std::pow(10.f, -reduction[k] / 20.f);
   const float wet = 1.f - mix * (1.f - g);
   gain[k] = (delta ? 1.f - wet : wet) * out;
  }
  for (int c = 0; c < used; ++c) {
   auto& ch = channels[static_cast<std::size_t>(c)];
   for (std::size_t k = 0; k < bins; ++k) ch.spec[k] *= gain[k];
   for (std::size_t k = 1; k < N / 2; ++k) ch.spec[N - k] = std::conj(ch.spec[k]);
   fft.inverse(ch.spec);
   for (std::size_t k = 0; k < N; ++k) ch.out[(pos + k) % N] += ch.spec[k].real() * window[k];
  }
 }

 void analyse() {
  const std::size_t bins = N / 2 + 1;
  // Detection works on power averaged over neighbouring bins and a few frames:
  // single bins of a noisy sound jump around and must not read as resonances.
  float maxDb = -200.f;
  for (std::size_t k = 0; k < bins; ++k) {
   const float local = (power[k > 0 ? k - 1 : k] + 2.f * power[k] + power[k + 1 < bins ? k + 1 : k]) * .25f;
   detect[k] += detectCoef * (local - detect[k]);
   db[k] = 10.f * std::log10(detect[k] + 1e-20f);
   maxDb = std::max(maxDb, db[k]);
  }
  std::fill(target.begin(), target.end(), 0.f);
  const float floorDb = maxDb - 70.f; // never chase what can barely be heard
  const bool silent = maxDb < -110.f;
  // ---- harshness: what sticks out of its own neighbourhood
  const float harshDepth = values[P::HarshDepth];
  float harshMax = 0.f;
  if (harshDepth > 0.f && !silent) {
   prefix[0] = 0;
   for (std::size_t k = 0; k < bins; ++k) prefix[k + 1] = prefix[k] + db[k];
   const double half = .5 + (1.0 / 8.0 - .5) * values[P::Sharpness] / 100.0; // octaves each side
   const double up = std::pow(2.0, half), down = 1.0 / up;
   const double innerUp = std::pow(2.0, half * .4), innerDown = 1.0 / innerUp;
   const float threshold = 7.f - 5.5f * values[P::HarshSense] / 100.f; // dB above the neighbourhood
   const float lo = std::min(values[P::HarshLow], values[P::HarshHigh] * .9f), hi = values[P::HarshHigh];
   for (std::size_t k = 2; k < bins - 1; ++k) {
    const float f = freq[k];
    const float w = rangeWeight(f, lo, hi);
    if (w <= 0.f || db[k] < floorDb) continue;
    // the neighbourhood leaves out the centre, so a peak does not raise its own reference
    const auto a = std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<double>(k) * down));
    const auto b = std::min(bins - 1, static_cast<std::size_t>(std::ceil(static_cast<double>(k) * up)));
    const auto ca = std::max(a, static_cast<std::size_t>(static_cast<double>(k) * innerDown));
    const auto cb = std::min(b, static_cast<std::size_t>(std::ceil(static_cast<double>(k) * innerUp)));
    const double count = static_cast<double>((b + 1 - a) - (cb + 1 - ca));
    if (count < 2) continue;
    const float env = static_cast<float>(((prefix[b + 1] - prefix[a]) - (prefix[cb + 1] - prefix[ca])) / count);
    const float excess = db[k] - env - threshold;
    if (excess > 0.f) target[k] = std::min(harshDepth, excess) * w;
   }
   // the whole range too bright: its level against the body of the sound (both
   // pink-corrected; pink noise is the line, most music sits below it)
   double band = 0, bandN = 0, ref = 0, refN = 0;
   for (std::size_t k = 1; k < bins; ++k) {
    const float f = freq[k];
    // measured after the resonances come out: a single peak is not a bright sound
    const double pinked = static_cast<double>(detect[k]) * std::pow(10.0, -target[k] / 10.0) * f;
    if (f >= lo && f <= hi) { band += pinked * rangeWeight(f, lo, hi); bandN += rangeWeight(f, lo, hi); }
    else if (f >= 200.f && f <= 1500.f) { ref += pinked; refN += 1; }
   }
   if (bandN > 0 && refN > 0 && ref > 0) {
    const float excess = static_cast<float>(10 * std::log10((band / bandN) / (ref / refN) + 1e-20));
    const float allowance = 4.f - 7.f * values[P::HarshSense] / 100.f;
    broadHarsh = std::clamp((excess - allowance) * .8f, 0.f, harshDepth);
   } else broadHarsh = 0.f;
   for (std::size_t k = 1; k < bins; ++k) {
    if (broadHarsh > 0.f) target[k] = std::min(harshDepth, target[k] + broadHarsh * rangeWeight(freq[k], lo, hi));
    harshMax = std::max(harshMax, target[k]);
   }
  }
  // ---- mud: the low-mid band against the rest, both pink-corrected
  const float mudDepth = values[P::MudDepth];
  float mudCut = 0.f;
  const float focus = values[P::MudFocus];
  if (mudDepth > 0.f && !silent) {
   // a bump: the band against the line from the bass below it to the mids
   // above it (pink-corrected), so a mix's natural tilt reads level
   auto level = [&](float f0, float f1) {
    double sum = 0, n = 0;
    for (std::size_t k = 1; k < bins; ++k) if (freq[k] >= f0 && freq[k] <= f1) { sum += static_cast<double>(detect[k]) * freq[k]; n += 1; }
    return n > 0 && sum > 0 ? 10 * std::log10(sum / n) : -300.0;
   };
   const double below = level(focus / 3.2f, focus / 1.8f), band = level(focus / 1.5f, focus * 1.5f), above = level(focus * 1.8f, focus * 3.2f);
   if (below > -290 && band > -290 && above > -290) {
    const float excess = static_cast<float>(band - .5 * (below + above));
    const float allowance = 5.f - 4.5f * values[P::MudSense] / 100.f; // how big a bump is still normal
    mudCut = std::clamp((excess - allowance) * 1.3f, 0.f, mudDepth);
   }
   for (std::size_t k = 1; k < bins; ++k) {
    const float o = std::log2(freq[k] / focus) / .7f;
    target[k] += mudCut * std::exp(-.5f * o * o);
   }
  }
  // ---- time: attack within a frame, release at SPEED
  const double hopSeconds = static_cast<double>(hop) / sr;
  const double release = .4 * std::pow(.1, values[P::Speed] / 100.0); // 400 ms .. 40 ms
  const float rel = static_cast<float>(std::exp(-hopSeconds / release));
  const float att = static_cast<float>(std::exp(-hopSeconds / .004));
  for (std::size_t k = 0; k < bins; ++k) {
   const float t = target[k], r = reduction[k];
   reduction[k] = t > r ? t + att * (r - t) : t + rel * (r - t);
  }
  // ---- frequency: a little smoothing so neighbouring bins move together
  for (int pass = 0; pass < 2; ++pass) {
   smoothTmp = reduction;
   for (std::size_t k = 1; k + 1 < bins; ++k) reduction[k] = .25f * smoothTmp[k - 1] + .5f * smoothTmp[k] + .25f * smoothTmp[k + 1];
  }
  reduction[0] = 0.f;
  reduction[bins - 1] = 0.f;
  publish();
  mudNow.store(mudCut, std::memory_order_relaxed);
  harshNow.store(harshMax, std::memory_order_relaxed);
 }

 static float rangeWeight(float f, float lo, float hi) {
  // soft edges, a third of an octave each side
  const float a = std::log2(f / lo) * 3.f + .5f, b = std::log2(hi / f) * 3.f + .5f;
  return std::clamp(std::min(a, b), 0.f, 1.f);
 }

 void publish() {
  const std::size_t bins = N / 2 + 1;
  for (int i = 0; i < displayBins; ++i) {
   const float f0 = displayLow * std::pow(displayHigh / displayLow, static_cast<float>(i) / displayBins);
   const float f1 = displayLow * std::pow(displayHigh / displayLow, static_cast<float>(i + 1) / displayBins);
   auto k0 = static_cast<std::size_t>(f0 / static_cast<float>(sr) * static_cast<float>(N));
   auto k1 = std::max(k0 + 1, static_cast<std::size_t>(f1 / static_cast<float>(sr) * static_cast<float>(N)));
   k1 = std::min(k1, bins);
   float p = 0, r = 0;
   for (std::size_t k = k0; k < k1; ++k) { p = std::max(p, power[k]); r = std::max(r, reduction[k]); }
   const float level = 10.f * std::log10(p + 1e-20f) - 10.f * std::log10(static_cast<float>(N) * static_cast<float>(N) / 8.f);
   auto& smooth = displayLevel[static_cast<std::size_t>(i)];
   smooth = level > smooth ? level : smooth + .3f * (level - smooth);
   displayInput[static_cast<std::size_t>(i)].store(smooth, std::memory_order_relaxed);
   displayReduction[static_cast<std::size_t>(i)].store(r, std::memory_order_relaxed);
  }
 }

 double sr = 48000;
 std::size_t N = 2048, hop = 512, pos = 0, hopCount = 0;
 dsp::FFT fft;
 float detectCoef = .3f, broadHarsh = 0.f;
 std::vector<float> window, power, detect, db, target, reduction, smoothTmp, gain, freq;
 std::vector<double> prefix;
 std::array<Channel, 2> channels;
 std::array<float, displayBins> displayLevel{};
 Values values = defaults();
};

} // namespace inverso_abismo::clear
