#pragma once
// INVERSO channel-strip and mastering building blocks (no JUCE): filters, the
// parametric EQ, the saturator and the stereo width. Everything runs in double
// precision per sample; the engine sets controls every few samples.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace inverso_abismo::chain {

constexpr double pi = 3.14159265358979323846;
inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }
inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
// One-pole smoothing coefficient for a time constant in seconds at `sr`.
// Exact comparison without -Wfloat-equal: settings are compared to see whether they changed at all.
inline bool same(double a, double b) { return !(a < b) && !(b < a); }
inline double timeCoef(double seconds, double sr) { return seconds <= 1e-9 ? 1.0 : 1.0 - std::exp(-1.0 / (seconds * sr)); }

// ---------------------------------------------------------------- biquads (RBJ cookbook)

struct Coeffs {
 double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

namespace detail {
inline Coeffs normalise(double b0, double b1, double b2, double a0, double a1, double a2) {
 return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
inline double safeF(double f, double sr) { return clampd(f, 5.0, 0.47 * sr); }
}

inline Coeffs peaking(double f, double q, double gainDb, double sr) {
 f = detail::safeF(f, sr);
 const double A = std::pow(10.0, gainDb / 40.0), w = 2 * pi * f / sr, al = std::sin(w) / (2 * std::max(q, .05));
 return detail::normalise(1 + al * A, -2 * std::cos(w), 1 - al * A, 1 + al / A, -2 * std::cos(w), 1 - al / A);
}
inline Coeffs lowShelf(double f, double q, double gainDb, double sr) {
 f = detail::safeF(f, sr);
 const double A = std::pow(10.0, gainDb / 40.0), w = 2 * pi * f / sr, cw = std::cos(w), al = std::sin(w) / (2 * std::max(q, .05));
 const double s = 2 * std::sqrt(A) * al;
 return detail::normalise(A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s),
                          (A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s);
}
inline Coeffs highShelf(double f, double q, double gainDb, double sr) {
 f = detail::safeF(f, sr);
 const double A = std::pow(10.0, gainDb / 40.0), w = 2 * pi * f / sr, cw = std::cos(w), al = std::sin(w) / (2 * std::max(q, .05));
 const double s = 2 * std::sqrt(A) * al;
 return detail::normalise(A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s),
                          (A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s);
}
inline Coeffs highpass(double f, double q, double sr) {
 f = detail::safeF(f, sr);
 const double w = 2 * pi * f / sr, cw = std::cos(w), al = std::sin(w) / (2 * q);
 return detail::normalise((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Coeffs lowpass(double f, double q, double sr) {
 f = detail::safeF(f, sr);
 const double w = 2 * pi * f / sr, cw = std::cos(w), al = std::sin(w) / (2 * q);
 return detail::normalise((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}
// Band-pass with 0 dB gain at the centre.
inline Coeffs bandpass(double f, double q, double sr) {
 f = detail::safeF(f, sr);
 const double w = 2 * pi * f / sr, al = std::sin(w) / (2 * std::max(q, .05));
 return detail::normalise(al, 0, -al, 1 + al, -2 * std::cos(w), 1 - al);
}

// Magnitude of a biquad at `f` (for the editor's curves and the tests).
inline double magnitude(const Coeffs& c, double f, double sr) {
 const double w = 2 * pi * f / sr, c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
 const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = -(c.b1 * s1 + c.b2 * s2);
 const double dr = 1 + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
 return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
}

// Transposed direct form II: exact state, no gain at DC surprises.
struct Biquad {
 Coeffs c;
 double z1 = 0, z2 = 0;
 void set(const Coeffs& n) { c = n; }
 void reset() { z1 = z2 = 0; }
 double process(double x) {
  const double y = c.b0 * x + z1;
  z1 = c.b1 * x - c.a1 * y + z2;
  z2 = c.b2 * x - c.a2 * y;
  return y;
 }
};

// A 4th-order Butterworth high-pass (two sections), for sidechains and the strip's HPF.
struct HighPass4 {
 Biquad a, b;
 void set(double f, double sr) { a.set(highpass(f, .5412, sr)); b.set(highpass(f, 1.3066, sr)); }
 void reset() { a.reset(); b.reset(); }
 double process(double x) { return b.process(a.process(x)); }
};
struct LowPass4 {
 Biquad a, b;
 void set(double f, double sr) { a.set(lowpass(f, .5412, sr)); b.set(lowpass(f, 1.3066, sr)); }
 void reset() { a.reset(); b.reset(); }
 double process(double x) { return b.process(a.process(x)); }
};

// ---------------------------------------------------------------- parametric EQ

struct EqControls {
 bool on = true;
 float hpf = 10.f, lpf = 22000.f;                 // 10 Hz and 22 kHz: off
 float lsF = 100.f, lsG = 0.f;                    // low shelf
 float b1F = 300.f, b1G = 0.f, b1Q = 1.f;         // bell 1
 float b2F = 3000.f, b2G = 0.f, b2Q = 1.f;       // bell 2
 float hsF = 10000.f, hsG = 0.f;                  // high shelf
 int ms = 0;                                      // 0 stereo, 1 mid only, 2 side only
};

class Eq {
public:
 void prepare(double sampleRate) { sr = sampleRate; for (auto& ch : state) for (auto& b : ch.bands) b.reset(); for (auto& ch : state) { ch.hp.reset(); ch.lp.reset(); } setControls(c, true); }
 void setControls(const EqControls& n, bool force = false) {
  if (!force && sameAs(n)) return;
  c = n;
  hpOn = c.hpf > 10.5f; lpOn = c.lpf < 21900.f;
  const double q = .6; // broad, musical shelves
  Coeffs bands[4] = {lowShelf(c.lsF, q, c.lsG, sr), peaking(c.b1F, c.b1Q, c.b1G, sr), peaking(c.b2F, c.b2Q, c.b2G, sr), highShelf(c.hsF, q, c.hsG, sr)};
  active[0] = std::abs(c.lsG) > .005f; active[1] = std::abs(c.b1G) > .005f; active[2] = std::abs(c.b2G) > .005f; active[3] = std::abs(c.hsG) > .005f;
  for (auto& ch : state) {
   for (int i = 0; i < 4; ++i) ch.bands[static_cast<std::size_t>(i)].set(bands[i]);
   if (hpOn) ch.hp.set(c.hpf, sr);
   if (lpOn) ch.lp.set(c.lpf, sr);
  }
 }
 void process(double& l, double& r) {
  if (!c.on) return;
  if (c.ms == 0) { l = run(0, l); r = run(1, r); return; }
  double m = .5 * (l + r), s = .5 * (l - r);
  if (c.ms == 1) m = run(0, m); else s = run(1, s);
  l = m + s; r = m - s;
 }
 // Combined magnitude in dB at f (HPF, LPF and the four bands).
 double responseDb(double f) const {
  double g = 1.0;
  if (hpOn) { HighPass4 h; h.set(c.hpf, sr); g *= magnitude(h.a.c, f, sr) * magnitude(h.b.c, f, sr); }
  if (lpOn) { LowPass4 h; h.set(c.lpf, sr); g *= magnitude(h.a.c, f, sr) * magnitude(h.b.c, f, sr); }
  const Coeffs bands[4] = {lowShelf(c.lsF, .6, c.lsG, sr), peaking(c.b1F, c.b1Q, c.b1G, sr), peaking(c.b2F, c.b2Q, c.b2G, sr), highShelf(c.hsF, .6, c.hsG, sr)};
  for (int i = 0; i < 4; ++i) if (active[i]) g *= magnitude(bands[i], f, sr);
  return gainToDb(g);
 }
private:
 bool sameAs(const EqControls& n) const {
  return same(n.on, c.on) && same(n.hpf, c.hpf) && same(n.lpf, c.lpf) && same(n.lsF, c.lsF) && same(n.lsG, c.lsG) && same(n.b1F, c.b1F) && same(n.b1G, c.b1G) && same(n.b1Q, c.b1Q) &&
         same(n.b2F, c.b2F) && same(n.b2G, c.b2G) && same(n.b2Q, c.b2Q) && same(n.hsF, c.hsF) && same(n.hsG, c.hsG) && same(n.ms, c.ms);
 }
 double run(int ch, double x) {
  auto& s = state[static_cast<std::size_t>(ch)];
  if (hpOn) x = s.hp.process(x);
  for (int i = 0; i < 4; ++i) if (active[i]) x = s.bands[static_cast<std::size_t>(i)].process(x);
  if (lpOn) x = s.lp.process(x);
  return x;
 }
 struct Channel { std::array<Biquad, 4> bands; HighPass4 hp; LowPass4 lp; };
 double sr = 48000.0;
 EqControls c;
 std::array<Channel, 2> state;
 bool hpOn = false, lpOn = false;
 bool active[4] = {false, false, false, false};
};

// ---------------------------------------------------------------- saturator

namespace detail {
// ln(cosh(x)), stable for large |x|: the antiderivative of tanh.
inline double lnCosh(double x) { const double a = std::abs(x); return a + std::log1p(std::exp(-2 * a)) - 0.6931471805599453; }
// First-order antiderivative anti-aliasing for y = tanh(x).
struct AdaaTanh {
 double x1 = 0;
 double process(double x) {
  const double dx = x - x1;
  const double y = std::abs(dx) < 1e-6 ? std::tanh(.5 * (x + x1)) : (lnCosh(x) - lnCosh(x1)) / dx;
  x1 = x;
  return y;
 }
};
}

enum class SatType { Tape = 0, Tube = 1, Transformer = 2 };

struct SatControls {
 bool on = true;
 SatType type = SatType::Tape;
 float drive = 0.f;   // 0..100 %
 float tone = 0.f;    // -100..100: tilt, dark to bright
 float mix = 100.f;   // %
};

// Tape (soft, a little dull, a head bump), tube (even harmonics from a bias) and
// transformer (the lows saturate first). The drive is loudness-compensated: at
// -18 dBFS RMS the level stays within about a dB at any drive, so it can be set
// by ear without touching the output.
class Saturator {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  buildCompensation();
  for (auto& ch : state) ch = Channel{};
  setControls(c, true);
 }
 void setControls(const SatControls& n, bool force = false) {
  if (!force && same(n.on, c.on) && n.type == c.type && same(n.drive, c.drive) && same(n.tone, c.tone) && same(n.mix, c.mix)) return;
  c = n;
  const double d = clampd(c.drive, 0, 100) / 100.0;
  g = dbToGain(24.0 * d);
  compensation = compFor(c.type, d);
  mixAmt = clampd(c.mix, 0, 100) / 100.0;
  const double t = clampd(c.tone, -100, 100) / 100.0;
  const Coeffs tilt = highShelf(1800.0, .5, 4.0 * t, sr), tiltLow = lowShelf(1800.0, .5, -4.0 * t, sr);
  const Coeffs dull = lowpass(clampd(18000.0 - 9000.0 * d, 6000.0, 18000.0), .6, sr); // tape's head loses highs as it is pushed
  const Coeffs bump = lowShelf(75.0, .6, 1.8 * d, sr);
  lowSplit = timeCoef(1.0 / (2 * pi * 160.0), sr);
  for (auto& ch : state) { ch.tilt.set(tilt); ch.tiltLow.set(tiltLow); ch.dull.set(dull); ch.bump.set(bump); }
 }
 double process(int ch, double in) {
  if (!c.on || c.drive <= 0.001f) return in;
  auto& s = state[static_cast<std::size_t>(ch)];
  double wet = shape(s, c.type, g, lowSplit, in);
  wet = s.tiltLow.process(s.tilt.process(wet)) * compensation;
  return in + (wet - in) * mixAmt;
 }
private:
 struct Channel {
  detail::AdaaTanh a, b;
  Biquad tilt, tiltLow, dull, bump;
  double low = 0, dc = 0;
 };
 // Unity gain for small signals; what differs is how each one bends the big ones.
 static double shape(Channel& s, SatType type, double g, double lowSplit, double x) {
  switch (type) {
   case SatType::Tape:
    return s.bump.process(s.dull.process(s.a.process(g * x) / g));
   case SatType::Tube: {
    const double bias = .30 * std::min(1.0, .25 + (g - 1.0) / 8.0);
    const double slope = 1.0 - std::tanh(bias) * std::tanh(bias);          // small-signal gain of tanh at the bias
    const double y = (s.a.process(g * x + bias) - std::tanh(bias)) / (g * slope);
    s.dc += 0.0004 * (y - s.dc);                                             // the bias makes DC: take it out
    return y - s.dc;
   }
   case SatType::Transformer: {
    s.low += lowSplit * (x - s.low);
    const double high = x - s.low, gl = g * 1.5, gh = std::sqrt(g);
    return s.a.process(gl * s.low) / gl + s.b.process(gh * high) / gh;      // the lows saturate first
   }
  }
  return x;
 }
 // The level each type and drive adds on a reference program, measured once: compensation is exact for it.
 void buildCompensation() {
  for (int t = 0; t < 3; ++t) {
   for (int i = 0; i <= 32; ++i) {
    const double d = static_cast<double>(i) / 32.0, gg = dbToGain(24.0 * d), split = timeCoef(1.0 / (2 * pi * 160.0), sr);
    Channel ch;
    ch.dull.set(lowpass(clampd(18000.0 - 9000.0 * d, 6000.0, 18000.0), .6, sr));
    ch.bump.set(lowShelf(75.0, .6, 1.8 * d, sr));
    double in2 = 0, out2 = 0;
    const int n = static_cast<int>(sr * .6);
    std::vector<double> ref(static_cast<std::size_t>(n));
    {
     double lp = 0, lp2 = 0, acc = 0; std::uint32_t seed = 12345u;
     for (int k = 0; k < n; ++k) {
      seed = seed * 1664525u + 1013904223u;
      lp += .12 * ((static_cast<double>(seed >> 8) / 8388608.0 - 1.0) - lp);
      lp2 += .05 * (lp - lp2);
      ref[static_cast<std::size_t>(k)] = lp * .7 + lp2 * 2.2; acc += ref[static_cast<std::size_t>(k)] * ref[static_cast<std::size_t>(k)];
     }
     const double scale = dbToGain(-18.0) / std::sqrt(acc / n);   // dark noise at -18 dBFS RMS
     for (auto& v : ref) v *= scale;
    }
    for (int k = 0; k < n; ++k) {
     const double x = ref[static_cast<std::size_t>(k)];
     const double y = shape(ch, static_cast<SatType>(t), gg, split, x);
     if (k > n / 5) { in2 += x * x; out2 += y * y; }
    }
    comp[static_cast<std::size_t>(t)][static_cast<std::size_t>(i)] = in2 > 0 && out2 > 0 ? std::sqrt(in2 / out2) : 1.0;
   }
  }
 }
 double compFor(SatType t, double d) const {
  const double p = clampd(d, 0, 1) * 32.0;
  const std::size_t i = std::min<std::size_t>(31, static_cast<std::size_t>(p));
  const double f = p - static_cast<double>(i);
  const auto& row = comp[static_cast<std::size_t>(t)];
  return row[i] + f * (row[i + 1] - row[i]);
 }
 double sr = 48000.0, g = 1.0, compensation = 1.0, mixAmt = 1.0, lowSplit = 0.0;
 SatControls c;
 std::array<Channel, 2> state;
 std::array<std::array<double, 33>, 3> comp{};
};

// ---------------------------------------------------------------- stereo width

struct WidthControls {
 float width = 100.f;  // %
 float bassMono = 20.f; // Hz; 20 or less: off
};

class Width {
public:
 void prepare(double sampleRate) { sr = sampleRate; hp.reset(); setControls(c, true); }
 void setControls(const WidthControls& n, bool force = false) {
  if (!force && same(n.width, c.width) && same(n.bassMono, c.bassMono)) return;
  c = n;
  w = clampd(c.width, 0, 200) / 100.0;
  mono = c.bassMono > 20.5f;
  if (mono) hp.set(c.bassMono, sr);
 }
 void process(double& l, double& r) {
  if (!mono && std::abs(w - 1.0) < 1e-4) return;
  const double m = .5 * (l + r);
  double s = .5 * (l - r);
  if (mono) s = hp.process(s);     // below the corner the sides are removed: the lows are mono
  s *= w;
  l = m + s; r = m - s;
 }
private:
 double sr = 48000.0, w = 1.0;
 bool mono = false;
 WidthControls c;
 HighPass4 hp;
};

} // namespace inverso_abismo::chain
