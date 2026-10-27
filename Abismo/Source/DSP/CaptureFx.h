#pragma once
// INVERSO CAPTURE, the editing: time stretch that keeps the pitch (three ways), pitch that keeps the time,
// tape-style speed, tape stop, reverse, and the effects (filter sweep, drive, crush, ring, chop, echo,
// reverb). All offline: the clip goes in, the edited clip comes out, and the audition and the file are
// the same sound. Portable (no JUCE).
#include "CaptureDsp.h"
#include "ChainDsp.h"
#include <complex>
#include <functional>
#include <random>

namespace inverso_abismo::capture {

using Cancel = std::function<bool()>;
constexpr double maxResultSeconds = 120.0;   // the longest result of a stretch (effect tails come on top)
using inverso_abismo::chain::Biquad;
using inverso_abismo::chain::Saturator;
using inverso_abismo::chain::SatControls;
using inverso_abismo::chain::SatType;

// ---------------------------------------------------------------- the whole edit
struct RenderParams {
 double start = 0.0, end = 1.0;      // region, 0..1
 // time and pitch
 int stretchMode = 0;                // 0 beats (WSOLA), 1 tones (phase vocoder), 2 texture (random phase)
 float stretchPct = 100.f;           // 100 = as it is, 200 = twice as long
 float pitchSt = 0.f;
 bool tape = false;                  // pitch and speed together, as a tape
 bool reverse = false;
 float warp = 0.f;                   // 0 off .. 100: how far the tape stops (or starts)
 int warpDir = 0;                    // 0 stops, 1 starts
 // tone
 bool filterOn = false; int filterType = 0; float filterFrom = 20000.f, filterTo = 20000.f, filterReso = 20.f;   // low-pass, high-pass, band-pass
 bool driveOn = false; int driveType = 0; float driveAmount = 0.f, driveMix = 100.f;
 bool crushOn = false; float crushBits = 16.f, crushRate = 48000.f, crushMix = 100.f;
 bool ringOn = false; float ringFreq = 200.f, ringMix = 50.f;
 // space and rhythm
 bool chopOn = false; int chopMode = 0; float chopSliceMs = 125.f, chopDepth = 50.f;   // gate, stutter
 bool echoOn = false; float echoMs = 300.f, echoFeedback = 40.f, echoDark = 30.f, echoMix = 30.f;
 bool reverbOn = false; float reverbSize = 50.f, reverbDamp = 40.f, reverbMix = 30.f;
 // end
 float fadeInMs = 5.f, fadeOutMs = 20.f, gainDb = 0.f;
};

namespace detail {
inline bool stopped(const Cancel& c) { return c && c(); }

struct Fft {
 explicit Fft(std::size_t size) : n(size), rev(size), tw(size / 2) {
  std::size_t bits = 0; while ((std::size_t{1} << bits) < n) ++bits;
  for (std::size_t i = 0; i < n; ++i) { std::size_t r = 0; for (std::size_t b = 0; b < bits; ++b) if (i & (std::size_t{1} << b)) r |= std::size_t{1} << (bits - 1 - b); rev[i] = r; }
  for (std::size_t i = 0; i < n / 2; ++i) tw[i] = std::polar(1.0, -2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n));
 }
 void run(std::vector<std::complex<double>>& x, bool inverse) const {
  for (std::size_t i = 0; i < n; ++i) if (rev[i] > i) std::swap(x[i], x[rev[i]]);
  for (std::size_t len = 2; len <= n; len <<= 1) {
   const std::size_t half = len / 2, step = n / len;
   for (std::size_t i = 0; i < n; i += len)
    for (std::size_t k = 0; k < half; ++k) {
     const auto w = inverse ? std::conj(tw[k * step]) : tw[k * step];
     const auto a = x[i + k], b = x[i + k + half] * w;
     x[i + k] = a + b; x[i + k + half] = a - b;
    }
  }
  if (inverse) for (auto& v : x) v /= static_cast<double>(n);
 }
 std::size_t n;
 std::vector<std::size_t> rev;
 std::vector<std::complex<double>> tw;
};

inline std::vector<double> hann(std::size_t n) {
 std::vector<double> w(n);
 for (std::size_t i = 0; i < n; ++i) w[i] = .5 - .5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n));
 return w;
}
inline std::size_t pow2AtLeast(double v) { std::size_t p = 256; while (static_cast<double>(p) < v) p <<= 1; return p; }
inline double sampleAtClamped(const std::vector<float>& x, long i) { return i < 0 || i >= static_cast<long>(x.size()) ? 0.0 : static_cast<double>(x[static_cast<std::size_t>(i)]); }
} // namespace detail

// ---------------------------------------------------------------- resampling
// factor 2: twice as fast, an octave up, half as long.
inline Clip resample(const Clip& in0, double factor, const Cancel& cancel = {}) {
 Clip out; out.sampleRate = in0.sampleRate;
 if (in0.empty() || factor <= 1e-4) return out;
 Clip filtered;
 const Clip* src = &in0;
 if (factor > 1.001) {   // reading faster than the clip was made folds what is above half the new rate back down: take it away first
  filtered = in0;
  const double fc = .42 * in0.sampleRate / factor;
  static const double qs[4] = {.5098, .6013, .9000, 2.5629};   // eighth-order Butterworth
  Biquad fl[4], fr[4];
  for (int k = 0; k < 4; ++k) { fl[k].set(inverso_abismo::chain::lowpass(fc, qs[k], in0.sampleRate)); fr[k].set(inverso_abismo::chain::lowpass(fc, qs[k], in0.sampleRate)); }
  for (std::size_t i = 0; i < filtered.frames(); ++i) {
   double l = filtered.l[i], r = filtered.r[i];
   for (int k = 0; k < 4; ++k) { l = fl[k].process(l); r = fr[k].process(r); }
   filtered.l[i] = static_cast<float>(l); filtered.r[i] = static_cast<float>(r);
  }
  src = &filtered;
 }
 const Clip& in = *src;
 const auto n = static_cast<std::size_t>(std::ceil(static_cast<double>(in.frames()) / factor));
 out.l.resize(n); out.r.resize(n);
 for (std::size_t i = 0; i < n; ++i) {
  if ((i & 0x3fff) == 0 && detail::stopped(cancel)) return Clip{};
  const double pos = static_cast<double>(i) * factor;
  out.l[i] = static_cast<float>(cubic(in.l, pos)); out.r[i] = static_cast<float>(cubic(in.r, pos));
 }
 return out;
}

// ---------------------------------------------------------------- time stretch, the pitch stays
// ratio 2: twice as long.
inline std::vector<float> stretchChannelWsola(const std::vector<float>& x, const std::vector<float>& mono, double ratio, double sr, const Cancel& cancel) {
 const std::size_t N = detail::pow2AtLeast(2048.0 * sr / 48000.0 * .9), Hs = N / 2;
 const long tol = static_cast<long>(N / 4);
 const auto win = detail::hann(N);
 const auto outLen = static_cast<std::size_t>(static_cast<double>(x.size()) * ratio);
 std::vector<double> out(outLen + 2 * N, 0.0), norm(outLen + 2 * N, 0.0);
 long prev = 0;
 const std::size_t frames = outLen / Hs + 2;
 for (std::size_t k = 0; k < frames; ++k) {
  if ((k & 31) == 0 && detail::stopped(cancel)) return {};
  const long nominal = static_cast<long>(static_cast<double>(k * Hs) / ratio);
  long s = nominal;
  if (k > 0) {
   // the natural continuation of the frame before is what the next frame should resemble
   const long tmpl = prev + static_cast<long>(Hs);
   double best = -1e30; long bestS = nominal;
   const long lo = std::max(0L, nominal - tol), hi = nominal + tol;
   const long len = static_cast<long>(N), dec = 4;
   for (long c = lo; c <= hi; c += dec) {
    double dot = 0, e = 1e-9;
    for (long i = 0; i < len; i += dec) { const double a = detail::sampleAtClamped(mono, tmpl + i), b = detail::sampleAtClamped(mono, c + i); dot += a * b; e += b * b; }
    const double score = dot / std::sqrt(e);
    if (score > best) { best = score; bestS = c; }
   }
   // refine around the coarse winner
   double best2 = -1e30; long fine = bestS;
   for (long c = std::max(0L, bestS - dec); c <= bestS + dec; ++c) {
    double dot = 0, e = 1e-9;
    for (long i = 0; i < len; i += 2) { const double a = detail::sampleAtClamped(mono, tmpl + i), b = detail::sampleAtClamped(mono, c + i); dot += a * b; e += b * b; }
    const double score = dot / std::sqrt(e);
    if (score > best2) { best2 = score; fine = c; }
   }
   s = fine;
  }
  const std::size_t o = k * Hs;
  for (std::size_t i = 0; i < N; ++i) { out[o + i] += detail::sampleAtClamped(x, s + static_cast<long>(i)) * win[i]; norm[o + i] += win[i]; }
  prev = s;
 }
 std::vector<float> r(outLen);
 for (std::size_t i = 0; i < outLen; ++i) r[i] = static_cast<float>(norm[i] > 1e-3 ? out[i] / norm[i] : 0.0);
 return r;
}

inline std::vector<float> stretchChannelVocoder(const std::vector<float>& x, double ratio, double sr, const Cancel& cancel) {
 const std::size_t N = detail::pow2AtLeast(4096.0 * sr / 48000.0 * .9), Hs = N / 4;
 const detail::Fft fft(N);
 const auto win = detail::hann(N);
 const auto outLen = static_cast<std::size_t>(static_cast<double>(x.size()) * ratio);
 std::vector<double> out(outLen + 2 * N, 0.0), norm(outLen + 2 * N, 0.0);
 std::vector<double> prevPhase(N / 2 + 1, 0.0), outPhase(N / 2 + 1, 0.0), newOut(N / 2 + 1, 0.0), mags(N / 2 + 1, 0.0), phs(N / 2 + 1, 0.0);
 std::vector<std::size_t> peakOfBin(N / 2 + 1, 0), peaks;
 std::vector<std::complex<double>> buf(N);
 long lastPos = 0;
 const std::size_t frames = outLen / Hs + 2;
 for (std::size_t k = 0; k < frames; ++k) {
  if ((k & 15) == 0 && detail::stopped(cancel)) return {};
  const long pos = static_cast<long>(std::llround(static_cast<double>(k * Hs) / ratio));
  const double ha = k == 0 ? static_cast<double>(Hs) : static_cast<double>(std::max(1L, pos - lastPos));
  lastPos = pos;
  for (std::size_t i = 0; i < N; ++i) buf[i] = detail::sampleAtClamped(x, pos + static_cast<long>(i) - static_cast<long>(N / 2)) * win[i];
  fft.run(buf, false);
  const std::size_t H = N / 2;
  for (std::size_t b = 0; b <= H; ++b) { mags[b] = std::abs(buf[b]); phs[b] = std::arg(buf[b]); }
  // the peaks (sinusoids): each one's phase advances by its own true frequency; the bins around it keep
  // their phase relation to it (identity phase locking, Laroche and Dolson), so a sinusoid stays one piece
  std::size_t lastPeak = 0;
  std::vector<std::size_t>& peakOf = peakOfBin; peaks.clear();
  for (std::size_t b = 0; b <= H; ++b) {
   const bool isPeak = (b < 2 || mags[b] >= mags[b - 2]) && (b < 1 || mags[b] >= mags[b - 1]) && (b + 1 > H || mags[b] >= mags[b + 1]) && (b + 2 > H || mags[b] >= mags[b + 2]);
   if (isPeak && (peaks.empty() || b > peaks.back() + 1 || mags[b] > mags[peaks.back()])) { if (!peaks.empty() && b <= peaks.back() + 1) peaks.back() = b; else peaks.push_back(b); }
  }
  if (peaks.empty()) peaks.push_back(0);
  (void)lastPeak;
  for (std::size_t pi = 0, b = 0; b <= H; ++b) { while (pi + 1 < peaks.size() && b > (peaks[pi] + peaks[pi + 1]) / 2) ++pi; peakOf[b] = peaks[pi]; }
  for (std::size_t b : peaks) {
   const double omega = 2.0 * M_PI * static_cast<double>(b) / static_cast<double>(N);
   double delta = phs[b] - prevPhase[b] - omega * ha;
   delta -= 2.0 * M_PI * std::round(delta / (2.0 * M_PI));
   const double trueFreq = omega + delta / ha;
   newOut[b] = k == 0 ? phs[b] : outPhase[b] + trueFreq * static_cast<double>(Hs);
  }
  for (std::size_t b = 0; b <= H; ++b) {
   if (peakOf[b] != b) newOut[b] = newOut[peakOf[b]] + phs[b] - phs[peakOf[b]];
   prevPhase[b] = phs[b];
  }
  for (std::size_t b = 0; b <= H; ++b) {
   outPhase[b] = newOut[b];
   buf[b] = std::polar(mags[b], outPhase[b]);
   if (b > 0 && b < H) buf[N - b] = std::conj(buf[b]);
  }
  buf[N / 2] = std::complex<double>(buf[N / 2].real(), 0.0);
  fft.run(buf, true);
  const std::size_t o = k * Hs;
  for (std::size_t i = 0; i < N; ++i) { out[o + i] += buf[i].real() * win[i]; norm[o + i] += win[i] * win[i]; }
 }
 const std::size_t shift = N / 2;
 std::vector<float> r(outLen);
 for (std::size_t i = 0; i < outLen; ++i) { const std::size_t j = i + shift; r[i] = static_cast<float>(norm[j] > 1e-3 ? out[j] / norm[j] : 0.0); }
 return r;
}

// Random phases: the sound becomes a texture, however far it is stretched (the "Paulstretch" idea).
inline std::vector<float> stretchChannelTexture(const std::vector<float>& x, double ratio, double sr, std::uint32_t seed, const Cancel& cancel) {
 const double want = std::clamp(.17 * sr * std::sqrt(std::max(1.0, ratio) / 2.0), 4096.0, 65536.0);
 const std::size_t N = detail::pow2AtLeast(want), Hs = N / 4;
 const detail::Fft fft(N);
 const auto win = detail::hann(N);
 const auto outLen = static_cast<std::size_t>(static_cast<double>(x.size()) * ratio);
 std::vector<double> out(outLen + 2 * N, 0.0), norm(outLen + 2 * N, 0.0);
 std::vector<std::complex<double>> buf(N);
 std::mt19937 rng(seed);
 std::uniform_real_distribution<double> phase(0.0, 2.0 * M_PI);
 const std::size_t frames = outLen / Hs + 2;
 for (std::size_t k = 0; k < frames; ++k) {
  if ((k & 7) == 0 && detail::stopped(cancel)) return {};
  const long pos = static_cast<long>(std::llround(static_cast<double>(k * Hs) / ratio));
  for (std::size_t i = 0; i < N; ++i) buf[i] = detail::sampleAtClamped(x, pos + static_cast<long>(i) - static_cast<long>(N / 2)) * win[i];
  fft.run(buf, false);
  for (std::size_t b = 0; b <= N / 2; ++b) {
   const double mag = std::abs(buf[b]);
   buf[b] = (b == 0 || b == N / 2) ? std::complex<double>(mag, 0.0) : std::polar(mag, phase(rng));
   if (b > 0 && b < N / 2) buf[N - b] = std::conj(buf[b]);
  }
  fft.run(buf, true);
  const std::size_t o = k * Hs;
  for (std::size_t i = 0; i < N; ++i) { out[o + i] += buf[i].real() * win[i]; norm[o + i] += win[i] * win[i]; }
 }
 const std::size_t shift = N / 2;
 std::vector<float> r(outLen);
 for (std::size_t i = 0; i < outLen; ++i) { const std::size_t j = i + shift; r[i] = static_cast<float>(norm[j] > 1e-3 ? out[j] / norm[j] : 0.0); }
 return r;
}

inline Clip timeStretch(const Clip& in, double ratio, int mode, const Cancel& cancel = {}) {
 Clip out; out.sampleRate = in.sampleRate;
 if (in.empty() || ratio <= 1e-3) return out;
 if (std::abs(ratio - 1.0) < 1e-4) return in;
 std::vector<float> mono(in.frames());
 for (std::size_t i = 0; i < mono.size(); ++i) mono[i] = .5f * (in.l[i] + in.r[i]);
 if (mode == 0) { out.l = stretchChannelWsola(in.l, mono, ratio, in.sampleRate, cancel); out.r = stretchChannelWsola(in.r, mono, ratio, in.sampleRate, cancel); }
 else if (mode == 1) { out.l = stretchChannelVocoder(in.l, ratio, in.sampleRate, cancel); out.r = stretchChannelVocoder(in.r, ratio, in.sampleRate, cancel); }
 else { out.l = stretchChannelTexture(in.l, ratio, in.sampleRate, 1u, cancel); out.r = stretchChannelTexture(in.r, ratio, in.sampleRate, 2u, cancel); }
 if (out.l.empty() || out.r.empty()) return Clip{};
 return out;
}

// ---------------------------------------------------------------- tape stop / start
inline Clip tapeWarp(const Clip& in, float amount, int direction, const Cancel& cancel = {}) {
 if (in.empty() || amount <= .01f) return in;
 Clip out; out.sampleRate = in.sampleRate;
 const double vEnd = 1.0 - .98 * static_cast<double>(amount) * .01;
 const double n = static_cast<double>(in.frames());
 const std::size_t cap = static_cast<std::size_t>(n * 3.0);
 double pos = 0.0;
 while (pos < n - 1.0 && out.l.size() < cap) {
  if ((out.l.size() & 0x3fff) == 0 && detail::stopped(cancel)) return Clip{};
  const double u = pos / n;
  const double v = direction == 0 ? 1.0 + (vEnd - 1.0) * u * u : vEnd + (1.0 - vEnd) * (1.0 - (1.0 - u) * (1.0 - u));
  out.l.push_back(static_cast<float>(cubic(in.l, pos))); out.r.push_back(static_cast<float>(cubic(in.r, pos)));
  pos += std::max(.002, v);
 }
 // a stopping tape ends in silence, a starting one comes out of it
 const std::size_t f = std::min<std::size_t>(out.l.size() / 10, 4096);
 for (std::size_t i = 0; i < f; ++i) {
  const float g = static_cast<float>(i) / static_cast<float>(f);
  const std::size_t j = direction == 0 ? out.l.size() - 1 - i : i;
  out.l[j] *= g; out.r[j] *= g;
 }
 return out;
}

inline void reverseClip(Clip& c) { std::reverse(c.l.begin(), c.l.end()); std::reverse(c.r.begin(), c.r.end()); }

// ---------------------------------------------------------------- effects (each in place, or longer if it has a tail)
inline void filterSweep(Clip& c, int type, float from, float to, float reso, const Cancel& cancel = {}) {
 const std::size_t n = c.frames();
 if (n == 0) return;
 const double q = .5 + 11.5 * static_cast<double>(reso) * .01;
 Biquad bl, br;
 const double a = std::clamp(static_cast<double>(from), 20.0, c.sampleRate * .45), b = std::clamp(static_cast<double>(to), 20.0, c.sampleRate * .45);
 for (std::size_t i = 0; i < n; ++i) {
  if ((i & 15) == 0) {
   if ((i & 0xffff) == 0 && detail::stopped(cancel)) return;
   const double f = a * std::pow(b / a, static_cast<double>(i) / static_cast<double>(n));
   const auto co = type == 0 ? inverso_abismo::chain::lowpass(f, q, c.sampleRate) : type == 1 ? inverso_abismo::chain::highpass(f, q, c.sampleRate) : inverso_abismo::chain::bandpass(f, q, c.sampleRate);
   bl.set(co); br.set(co);
  }
  c.l[i] = static_cast<float>(bl.process(c.l[i])); c.r[i] = static_cast<float>(br.process(c.r[i]));
 }
}

inline void drive(Clip& c, int type, float amount, float mix) {
 if (c.empty() || amount <= 0.f) return;
 Saturator sat; sat.prepare(c.sampleRate);
 SatControls k; k.on = true; k.type = static_cast<SatType>(std::clamp(type, 0, 2)); k.drive = amount; k.tone = 0.f; k.mix = mix;
 sat.setControls(k);
 for (std::size_t i = 0; i < c.frames(); ++i) { c.l[i] = static_cast<float>(sat.process(0, c.l[i])); c.r[i] = static_cast<float>(sat.process(1, c.r[i])); }
}

inline void crush(Clip& c, float bits, float rate, float mix) {
 if (c.empty()) return;
 const double levels = std::pow(2.0, std::clamp(static_cast<double>(bits), 1.0, 16.0) - 1.0);
 const std::size_t hold = std::max<std::size_t>(1, static_cast<std::size_t>(std::round(c.sampleRate / std::clamp(static_cast<double>(rate), 500.0, c.sampleRate))));
 const float m = std::clamp(mix, 0.f, 100.f) * .01f;
 float hl = 0.f, hr = 0.f;
 for (std::size_t i = 0; i < c.frames(); ++i) {
  if (i % hold == 0) { hl = static_cast<float>(std::round(static_cast<double>(c.l[i]) * levels) / levels); hr = static_cast<float>(std::round(static_cast<double>(c.r[i]) * levels) / levels); }
  c.l[i] += (hl - c.l[i]) * m; c.r[i] += (hr - c.r[i]) * m;
 }
}

inline void ring(Clip& c, float freq, float mix) {
 const double m = std::clamp(static_cast<double>(mix), 0.0, 100.0) * .01, w = 2.0 * M_PI * static_cast<double>(freq) / c.sampleRate;
 for (std::size_t i = 0; i < c.frames(); ++i) {
  const double s = std::sin(w * static_cast<double>(i));
  c.l[i] = static_cast<float>(c.l[i] * (1.0 - m + m * s)); c.r[i] = static_cast<float>(c.r[i] * (1.0 - m + m * s));
 }
}

// mode 0: the end of each slice is cut away; mode 1: the start of each slice repeats to fill it
inline void chop(Clip& c, int mode, float sliceMs, float depth) {
 const double d = std::clamp(static_cast<double>(depth), 0.0, 100.0) * .01;
 if (d < .005) return;   // no depth, no chop
 const std::size_t slice = std::max<std::size_t>(64, static_cast<std::size_t>(sliceMs * .001 * c.sampleRate));
 const std::size_t ramp = std::min<std::size_t>(slice / 4, static_cast<std::size_t>(.002 * c.sampleRate) + 1);
 std::vector<float> tl, tr;
 for (std::size_t s = 0; s < c.frames(); s += slice) {
  const std::size_t len = std::min(slice, c.frames() - s);
  if (mode == 0) {
   const auto open = static_cast<std::size_t>(static_cast<double>(len) * (1.0 - .9 * d));
   for (std::size_t i = open; i < len; ++i) {
    const double g = i - open < ramp ? 1.0 - static_cast<double>(i - open) / static_cast<double>(ramp) : 0.0;
    c.l[s + i] = static_cast<float>(c.l[s + i] * g); c.r[s + i] = static_cast<float>(c.r[s + i] * g);
   }
  } else {
   const std::size_t piece = std::max<std::size_t>(32, static_cast<std::size_t>(static_cast<double>(len) / (1.0 + 7.0 * d)));
   if (piece >= len) continue;   // nothing repeats in this slice
   tl.assign(c.l.begin() + static_cast<long>(s), c.l.begin() + static_cast<long>(s + piece)); tr.assign(c.r.begin() + static_cast<long>(s), c.r.begin() + static_cast<long>(s + piece));
   const std::size_t edge = std::min(ramp, piece / 2) + 1;
   for (std::size_t i = 0; i < len; ++i) {
    const std::size_t j = i % piece;
    const double g = std::min({1.0, static_cast<double>(j + 1) / static_cast<double>(edge), static_cast<double>(piece - j) / static_cast<double>(edge)});
    c.l[s + i] = static_cast<float>(tl[j] * g); c.r[s + i] = static_cast<float>(tr[j] * g);
   }
  }
 }
}

// The tail is kept: the clip grows until the echoes have gone (at most 8 s).
inline void echo(Clip& c, float ms, float feedback, float dark, float mix) {
 const std::size_t d = std::max<std::size_t>(16, static_cast<std::size_t>(ms * .001 * c.sampleRate));
 const double fb = std::clamp(static_cast<double>(feedback), 0.0, 95.0) * .01, m = std::clamp(static_cast<double>(mix), 0.0, 100.0) * .01;
 const double tail = fb < .01 ? static_cast<double>(d) : std::min(8.0 * c.sampleRate, static_cast<double>(d) * std::log(.001) / std::log(fb));
 const std::size_t total = c.frames() + static_cast<std::size_t>(tail) + 1;
 const double lp = 1.0 - std::clamp(static_cast<double>(dark), 0.0, 100.0) * .0095;   // each repeat loses some highs
 const std::size_t n = c.frames();
 c.l.resize(total, 0.f); c.r.resize(total, 0.f);
 for (int side = 0; side < 2; ++side) {
  auto& ch = side ? c.r : c.l;
  std::vector<double> e(total, 0.0);
  double state = 0.0;
  for (std::size_t i = d; i < total; ++i) {
   const double x = i - d < n ? static_cast<double>(ch[i - d]) : 0.0;
   state += lp * (e[i - d] - state);
   e[i] = x + fb * state;
  }
  for (std::size_t i = 0; i < total; ++i) ch[i] = static_cast<float>(static_cast<double>(ch[i]) + m * e[i]);
 }
}

// Freeverb's layout: eight combs and four all-passes a side.
inline void reverb(Clip& c, float size, float damp, float mix) {
 static const int combs[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
 static const int allp[4] = {556, 441, 341, 225};
 const double scale = c.sampleRate / 44100.0;
 const double room = .7 + .28 * std::clamp(static_cast<double>(size), 0.0, 100.0) * .01;
 const double dp = std::clamp(static_cast<double>(damp), 0.0, 100.0) * .01 * .8;
 const double m = std::clamp(static_cast<double>(mix), 0.0, 100.0) * .01;
 const double rt60 = 3.0 * .0366 / -std::log10(room);   // the combs' average delay and their feedback give the decay time
 const std::size_t tailFrames = static_cast<std::size_t>(std::clamp(rt60, .4, 14.0) * c.sampleRate);
 const std::size_t total = c.frames() + tailFrames;
 c.l.resize(total, 0.f); c.r.resize(total, 0.f);
 struct Comb { std::vector<double> b; std::size_t i = 0; double store = 0; };
 struct All { std::vector<double> b; std::size_t i = 0; };
 for (int side = 0; side < 2; ++side) {
  std::vector<Comb> cs(8); std::vector<All> as(4);
  for (int k = 0; k < 8; ++k) cs[static_cast<std::size_t>(k)].b.assign(static_cast<std::size_t>((combs[k] + (side ? 23 : 0)) * scale), 0.0);
  for (int k = 0; k < 4; ++k) as[static_cast<std::size_t>(k)].b.assign(static_cast<std::size_t>((allp[k] + (side ? 23 : 0)) * scale), 0.0);
  auto& ch = side ? c.r : c.l;
  for (std::size_t i = 0; i < total; ++i) {
   const double in = static_cast<double>(ch[i]) * .015;
   double sum = 0;
   for (auto& cb : cs) { const double o = cb.b[cb.i]; cb.store = o * (1.0 - dp) + cb.store * dp; cb.b[cb.i] = in + cb.store * room; if (++cb.i >= cb.b.size()) cb.i = 0; sum += o; }
   for (auto& a : as) { const double o = a.b[a.i]; const double y = -sum + o; a.b[a.i] = sum + o * .5; if (++a.i >= a.b.size()) a.i = 0; sum = y; }
   ch[i] = static_cast<float>(static_cast<double>(ch[i]) * (1.0 - m * .5) + sum * 3.0 * m);
  }
 }
}

inline void trimSilence(Clip& c, double floorDb = -75.0) {
 const double th = std::pow(10.0, floorDb / 20.0);
 std::size_t last = c.frames();
 while (last > 1 && std::abs(c.l[last - 1]) < th && std::abs(c.r[last - 1]) < th) --last;
 last = std::min(c.frames(), last + static_cast<std::size_t>(.05 * c.sampleRate));
 c.l.resize(last); c.r.resize(last);
}

// ---------------------------------------------------------------- the pipeline
// Returns an empty clip if the edit was cancelled.
inline Clip render(const Clip& source, const RenderParams& p, const Cancel& cancel = {}) {
 Clip c; c.sampleRate = source.sampleRate;
 if (source.empty()) return c;
 const double a = std::clamp(std::min(p.start, p.end), 0.0, 1.0), b = std::clamp(std::max(p.start, p.end), 0.0, 1.0);
 const auto from = static_cast<std::size_t>(a * static_cast<double>(source.frames()));
 auto to = static_cast<std::size_t>(b * static_cast<double>(source.frames()));
 to = std::max(to, std::min(source.frames(), from + 64));
 c.l.assign(source.l.begin() + static_cast<long>(from), source.l.begin() + static_cast<long>(to));
 c.r.assign(source.r.begin() + static_cast<long>(from), source.r.begin() + static_cast<long>(to));
 if (c.frames() < 64) return c;

 // time and pitch
 double T = std::clamp(static_cast<double>(p.stretchPct) * .01, .05, 20.0);
 T = std::min(T, maxResultSeconds * c.sampleRate / static_cast<double>(c.frames()));   // a long clip cannot be stretched past what fits in memory
 const double f = std::exp2(static_cast<double>(p.pitchSt) / 12.0);
 if (p.tape) { const double speed = f / T; if (std::abs(speed - 1.0) > 1e-4) c = resample(c, speed, cancel); }   // pitch and speed together; PITCH adds on top
 else {
  if (std::abs(f - 1.0) > 1e-4) c = resample(c, f, cancel);          // the pitch moves, the length with it...
  if (!c.empty()) c = timeStretch(c, T * f, p.stretchMode, cancel);   // ...and the stretch gives the length back
 }
 if (c.empty() || detail::stopped(cancel)) return Clip{};
 if (p.warp > .01f) c = tapeWarp(c, p.warp, p.warpDir, cancel);
 if (c.empty()) return Clip{};
 if (p.reverse) reverseClip(c);

 // tone
 if (p.filterOn) filterSweep(c, p.filterType, p.filterFrom, p.filterTo, p.filterReso, cancel);
 if (p.driveOn) drive(c, p.driveType, p.driveAmount, p.driveMix);
 if (p.crushOn) crush(c, p.crushBits, p.crushRate, p.crushMix);
 if (p.ringOn) ring(c, p.ringFreq, p.ringMix);
 if (detail::stopped(cancel)) return Clip{};
 // space and rhythm
 if (p.chopOn) chop(c, p.chopMode, p.chopSliceMs, p.chopDepth);
 if (p.echoOn) echo(c, p.echoMs, p.echoFeedback, p.echoDark, p.echoMix);
 if (p.reverbOn) reverb(c, p.reverbSize, p.reverbDamp, p.reverbMix);
 if (p.echoOn || p.reverbOn) trimSilence(c);
 if (detail::stopped(cancel)) return Clip{};

 // the end of it
 const double g = std::pow(10.0, static_cast<double>(p.gainDb) / 20.0);
 const auto n = c.frames();
 const double fin = std::max(1.0, p.fadeInMs * .001 * c.sampleRate), fout = std::max(1.0, p.fadeOutMs * .001 * c.sampleRate);
 for (std::size_t i = 0; i < n; ++i) {
  const double e = g * std::min({1.0, (static_cast<double>(i) + 1.0) / fin, (static_cast<double>(n - i)) / fout});
  float l = static_cast<float>(c.l[i] * e), r = static_cast<float>(c.r[i] * e);
  if (!std::isfinite(l)) l = 0.f;
  if (!std::isfinite(r)) r = 0.f;
  c.l[i] = std::clamp(l, -8.f, 8.f); c.r[i] = std::clamp(r, -8.f, 8.f);
 }
 return c;
}

} // namespace inverso_abismo::capture
