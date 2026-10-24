#pragma once
// FURNACE amplifier: noise gate, tight filter, preamp gain stages, the classic
// treble/bass/middle tone stack (solved from the circuit, see
// Tools/derive_tonestack.py), power amp with sag and presence, output
// transformer, spring reverb. The nonlinear part runs 8x oversampled.
// The cabinet comes after this (FurnaceCab.h, convolved by the plugin).
// Portable (no JUCE).

#include "FurnaceParams.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace inverso_abismo::furnace {

namespace amp {
constexpr double pi = 3.14159265358979323846;

inline double besselI0(double x) {
 double sum = 1, term = 1;
 for (int k = 1; k < 40; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); sum += term; }
 return sum;
}
// Half-band low-pass (cutoff at a quarter of the rate), Kaiser window.
inline std::vector<float> halfband(int taps, double beta) {
 std::vector<float> h(static_cast<std::size_t>(taps));
 const double m = (taps - 1) / 2.0;
 double sum = 0;
 for (int i = 0; i < taps; ++i) {
  const double x = i - m;
  const double sinc = std::abs(x) < 1e-9 ? .5 : std::sin(pi * .5 * x) / (pi * x);
  const double r = x / m;
  const double w = besselI0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / besselI0(beta);
  h[static_cast<std::size_t>(i)] = static_cast<float>(sinc * w);
  sum += sinc * w;
 }
 for (auto& v : h) v = static_cast<float>(v / sum);
 return h;
}

// One 2x stage: up() turns 1 sample into 2, down() turns 2 into 1. Half of a
// half-band filter's taps are zero; only the others are computed.
class TwoX {
public:
 explicit TwoX(int taps) : h(halfband(taps, 7.86)) {
  for (std::size_t i = 0; i < h.size(); ++i) {
   if (std::abs(h[i]) < 1e-12f) continue;
   taps_.push_back({i, h[i]});
   (i % 2 == 0 ? evenTaps : oddTaps).push_back({i / 2, 2 * h[i]});
  }
  reset();
 }
 void reset() { upHist.assign(2 * h.size(), 0.f); downHist.assign(2 * h.size(), 0.f); pos = 0; dpos = 0; }
 void up(float x, float& a, float& b) {
  // zero-stuffed input filtered at the high rate (gain 2 for the stuffed zeros);
  // the history is mirrored so the taps read it without wrapping
  const std::size_t n = h.size();
  pos = (pos + n - 1) % n;
  upHist[pos] = x;
  upHist[pos + n] = x;
  const float* hist = &upHist[pos];
  float even = 0, odd = 0;
  for (const auto& t : evenTaps) even += t.c * hist[t.i];
  for (const auto& t : oddTaps) odd += t.c * hist[t.i];
  a = even;
  b = odd;
 }
 float down(float a, float b) {
  push(a);
  push(b);
  float y = 0;
  const float* hist = &downHist[dpos];
  for (const auto& t : taps_) y += t.c * hist[t.i];
  return y;
 }
private:
 struct Tap { std::size_t i; float c; };
 void push(float v) {
  const std::size_t n = h.size();
  dpos = (dpos + n - 1) % n;
  downHist[dpos] = v;
  downHist[dpos + n] = v; // mirror: hist[0..n) is always contiguous from dpos
 }
 std::vector<float> h, upHist, downHist;
 std::vector<Tap> taps_, evenTaps, oddTaps;
 std::size_t pos = 0, dpos = 0;
};

// tanh by a continued fraction, clamped at |x| = 4.97: within 1e-4 of std::tanh.
inline float fastTanh(float x) {
 if (x > 4.97f) return 1.f;
 if (x < -4.97f) return -1.f;
 const float x2 = x * x;
 return x * (135135.f + x2 * (17325.f + x2 * (378.f + x2))) / (135135.f + x2 * (62370.f + x2 * (3150.f + 28.f * x2)));
}

// One-pole filters by coefficient.
struct OnePole {
 float a = 0, z = 0;
 void setLow(double hz, double fs) { a = static_cast<float>(std::exp(-2 * pi * hz / fs)); }
 float low(float x) { z = x + a * (z - x); return z; }
 float high(float x) { return x - low(x); }
};

// RBJ biquads.
struct Biquad {
 float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
 float process(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
 void reset() { z1 = z2 = 0; }
 void set(double B0, double B1, double B2, double A0, double A1, double A2) {
  b0 = static_cast<float>(B0 / A0); b1 = static_cast<float>(B1 / A0); b2 = static_cast<float>(B2 / A0);
  a1 = static_cast<float>(A1 / A0); a2 = static_cast<float>(A2 / A0);
 }
 void highpass(double f, double q, double fs) {
  const double w = 2 * pi * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
  set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
 }
 void lowpass(double f, double q, double fs) {
  const double w = 2 * pi * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
  set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
 }
 void peak(double f, double db, double q, double fs) {
  const double A = std::pow(10.0, db / 40), w = 2 * pi * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
  set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
 }
 void highShelf(double f, double db, double fs) {
  const double A = std::pow(10.0, db / 40), w = 2 * pi * f / fs, c = std::cos(w), s = std::sin(w);
  const double al = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * al;
  set(A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
      (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
 }
 void lowShelf(double f, double db, double fs) {
  const double A = std::pow(10.0, db / 40), w = 2 * pi * f / fs, c = std::cos(w), s = std::sin(w);
  const double al = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * al;
  set(A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
      (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
 }
};

// Tone stack parts (ohms, farads). Pots: R1 treble, R2 bass, R3 middle; R4 slope.
struct StackParts { double R1, R2, R3, R4, C1, C2, C3; };
constexpr StackParts blackface{250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9};
constexpr StackParts tweed{250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9};
constexpr StackParts british{220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9};
constexpr StackParts modern{250e3, 1e6, 25e3, 47e3, 330e-12, 22e-9, 22e-9};
constexpr StackParts chime{1e6, 1e6, 10e3, 100e3, 50e-12, 22e-9, 22e-9};

// The analog transfer function of the stack for pot settings t (treble), l
// (bass), m (middle) in 0..1, generated by Tools/derive_tonestack.py.
inline void stackAnalog(const StackParts& p, double t, double l, double m, double b[4], double a[4]) {
 const double R1 = p.R1, R2 = p.R2, R3 = p.R3, R4 = p.R4, RL = 1e6, C1 = p.C1, C2 = p.C2, C3 = p.C3;
 const double D = R1 * t + R2 * l + R3 * m + RL;
 b[0] = 0;
 b[1] = RL * (C1 * R1 * t + C1 * R2 * l + C1 * R3 * m + C2 * R2 * l + C2 * R3 * m + C3 * R3 * m) / D;
 b[2] = RL * (C1 * C2 * R1 * R2 * l + C1 * C2 * R1 * R3 * m + C1 * C2 * R1 * R4 * t + C1 * C2 * R2 * R4 * l + C1 * C2 * R3 * R4 * m
              + C1 * C3 * R1 * R3 * m + C1 * C3 * R1 * R4 * t + C1 * C3 * R2 * R3 * l * m + C1 * C3 * R2 * R4 * l + C1 * C3 * R3 * R4 * m
              + C2 * C3 * R2 * R3 * l * m) / D;
 b[3] = C1 * C2 * C3 * R2 * RL * l * (R1 * R3 * m + R1 * R4 * t + R3 * R4 * m) / D;
 a[0] = 1;
 a[1] = (-C1 * R1 * R1 * t * t + C1 * R1 * R1 * t - C1 * R1 * R2 * l * t + C1 * R1 * R2 * l - C1 * R1 * R3 * m * t + C1 * R1 * R3 * m + C1 * R1 * RL
         + C1 * R2 * RL * l + C1 * R3 * RL * m + C2 * R1 * R2 * l * t + C2 * R1 * R3 * m * t + C2 * R1 * R4 * t + C2 * R2 * R4 * l + C2 * R2 * RL * l
         + C2 * R3 * R4 * m + C2 * R3 * RL * m + C2 * R4 * RL + C3 * R1 * R3 * m * t + C3 * R1 * R4 * t + C3 * R2 * R3 * l * m + C3 * R2 * R4 * l
         + C3 * R3 * R4 * m + C3 * R3 * RL * m + C3 * R4 * RL) / D;
 a[2] = (-C1 * C2 * R1 * R1 * R2 * l * t * t + C1 * C2 * R1 * R1 * R2 * l * t - C1 * C2 * R1 * R1 * R3 * m * t * t + C1 * C2 * R1 * R1 * R3 * m * t
         - C1 * C2 * R1 * R1 * R4 * t * t + C1 * C2 * R1 * R1 * R4 * t - C1 * C2 * R1 * R2 * R4 * l * t + C1 * C2 * R1 * R2 * R4 * l
         + C1 * C2 * R1 * R2 * RL * l - C1 * C2 * R1 * R3 * R4 * m * t + C1 * C2 * R1 * R3 * R4 * m + C1 * C2 * R1 * R3 * RL * m
         + C1 * C2 * R1 * R4 * RL + C1 * C2 * R2 * R4 * RL * l + C1 * C2 * R3 * R4 * RL * m - C1 * C3 * R1 * R1 * R3 * m * t * t
         + C1 * C3 * R1 * R1 * R3 * m * t - C1 * C3 * R1 * R1 * R4 * t * t + C1 * C3 * R1 * R1 * R4 * t - C1 * C3 * R1 * R2 * R3 * l * m * t
         + C1 * C3 * R1 * R2 * R3 * l * m - C1 * C3 * R1 * R2 * R4 * l * t + C1 * C3 * R1 * R2 * R4 * l - C1 * C3 * R1 * R3 * R4 * m * t
         + C1 * C3 * R1 * R3 * R4 * m + C1 * C3 * R1 * R3 * RL * m + C1 * C3 * R1 * R4 * RL + C1 * C3 * R2 * R3 * RL * l * m
         + C1 * C3 * R2 * R4 * RL * l + C1 * C3 * R3 * R4 * RL * m + C2 * C3 * R1 * R2 * R3 * l * m * t + C2 * C3 * R1 * R2 * R4 * l * t
         + C2 * C3 * R2 * R3 * R4 * l * m + C2 * C3 * R2 * R3 * RL * l * m + C2 * C3 * R2 * R4 * RL * l) / D;
 a[3] = C1 * C2 * C3 * R2 * l * (-R1 * R1 * R3 * m * t * t + R1 * R1 * R3 * m * t - R1 * R1 * R4 * t * t + R1 * R1 * R4 * t - R1 * R3 * R4 * m * t
                                 + R1 * R3 * R4 * m + R1 * R3 * RL * m + R1 * R4 * RL + R3 * R4 * RL * m) / D;
}

// Third-order IIR from the analog stack by the bilinear transform.
class ToneStack {
public:
 void design(const StackParts& p, double t, double l, double m, double fs) {
  double b[4], a[4];
  stackAnalog(p, t, l, m, b, a);
  const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
  const double B[4] = {b[0] + b[1] * c + b[2] * c2 + b[3] * c3, 3 * b[0] + b[1] * c - b[2] * c2 - 3 * b[3] * c3,
                       3 * b[0] - b[1] * c - b[2] * c2 + 3 * b[3] * c3, b[0] - b[1] * c + b[2] * c2 - b[3] * c3};
  const double A[4] = {a[0] + a[1] * c + a[2] * c2 + a[3] * c3, 3 * a[0] + a[1] * c - a[2] * c2 - 3 * a[3] * c3,
                       3 * a[0] - a[1] * c - a[2] * c2 + 3 * a[3] * c3, a[0] - a[1] * c + a[2] * c2 - a[3] * c3};
  for (int i = 0; i < 4; ++i) { nb[i] = B[i] / A[0]; na[i] = A[i] / A[0]; }
 }
 // Transposed direct form II in double: the poles sit close to z = 1 at 4x rates.
 float process(float xf) {
  const double x = xf;
  const double y = nb[0] * x + s[0];
  s[0] = nb[1] * x - na[1] * y + s[1];
  s[1] = nb[2] * x - na[2] * y + s[2];
  s[2] = nb[3] * x - na[3] * y;
  return static_cast<float>(y);
 }
 void reset() { s[0] = s[1] = s[2] = 0; }
 // Magnitude of the analog stack at f (for the editor's screen and tests).
 static double magnitude(const StackParts& p, double t, double l, double m, double f) {
  double b[4], a[4];
  stackAnalog(p, t, l, m, b, a);
  const double w = 2 * pi * f;
  const double nr = b[0] - b[2] * w * w, ni = b[1] * w - b[3] * w * w * w;
  const double dr = a[0] - a[2] * w * w, di = a[1] * w - a[3] * w * w * w;
  return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
 }
private:
 double nb[4]{1, 0, 0, 0}, na[4]{1, 0, 0, 0}, s[3]{0, 0, 0};
};

// Pot tapers: bass is an audio (log) pot, treble mildly, middle almost linear.
inline double taperTreble(double k) { return std::clamp(std::pow(k, 1.6), .002, .998); }
inline double taperBass(double k) { return std::clamp(k * k * k, .002, 1.0); }
inline double taperMid(double k) { return std::clamp(std::pow(k, 1.3), .002, 1.0); }

struct Rng {
 std::uint32_t s = 0x9E3779B9u;
 float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return static_cast<float>(s) * 2.3283064e-10f; } // 0..1
};

// Spring reverb: a loop of stretched all-passes (the chirp of a spring) and a delay.
class Spring {
public:
 void prepare(double fs) {
  sr = fs;
  for (int k = 0; k < 2; ++k) {
   auto& t = tanks[static_cast<std::size_t>(k)];
   t.delay.assign(static_cast<std::size_t>(fs * .2), 0.f);
   t.length = fs * (k == 0 ? .0371 : .0443);
   t.stretch = std::max(2, static_cast<int>(std::lround(fs / 48000.0 * 11)));
   t.ap.assign(static_cast<std::size_t>(numAllpass * t.stretch), 0.f);
   t.damp.setLow(4200, fs);
   t.dc.setLow(110, fs);
   t.pos = 0; t.apPos = 0;
  }
 }
 float process(float x, float amount) {
  float out = 0;
  for (auto& t : tanks) {
   const std::size_t n = t.delay.size();
   const double readAt = static_cast<double>(t.pos) + static_cast<double>(n) - t.length;
   const auto i0 = static_cast<std::size_t>(readAt) % n;
   const float fr = static_cast<float>(readAt - std::floor(readAt));
   const float delayed = t.delay[i0] + fr * (t.delay[(i0 + 1) % n] - t.delay[i0]);
   // dispersion: numAllpass first-order all-passes on z^-stretch
   float v = delayed;
   const std::size_t S = static_cast<std::size_t>(t.stretch);
   for (int k = 0; k < numAllpass; ++k) {
    float* line = &t.ap[static_cast<std::size_t>(k) * S];
    const float old = line[t.apPos];
    const float y = -apCoef * v + old;
    line[t.apPos] = v + apCoef * y;
    v = y;
   }
   t.apPos = (t.apPos + 1) % S;
   v = t.damp.low(v);
   v = t.dc.high(v);
   t.delay[t.pos] = x * .5f + .85f * v;
   t.pos = (t.pos + 1) % n;
   out += v;
  }
  return x + amount * 1.8f * out;
 }
 void reset() { for (auto& t : tanks) { std::fill(t.delay.begin(), t.delay.end(), 0.f); std::fill(t.ap.begin(), t.ap.end(), 0.f); t.damp.z = t.dc.z = 0; } }
private:
 static constexpr int numAllpass = 60;
 static constexpr float apCoef = .62f;
 struct Tank { std::vector<float> delay, ap; std::size_t pos = 0, apPos = 0; double length = 0; int stretch = 2; OnePole damp, dc; };
 std::array<Tank, 2> tanks;
 double sr = 48000;
};
} // namespace amp

// What makes each amp model itself.
struct ModelSpec {
 amp::StackParts stack;
 int stages;              // preamp triode stages
 int stackAfter;          // the tone stack comes after this stage (1-based)
 float gainMinDb, gainRangeDb;
 std::array<float, 4> stageGainDb; // fixed gain of stages 2..4 (index 1..3)
 std::array<float, 4> stageHp;     // coupling capacitor high-pass per stage (Hz)
 std::array<float, 4> stageLp;     // Miller low-pass per stage (Hz)
 std::array<float, 4> bias;        // asymmetry per stage
 float brightDb;          // bright cap: treble boost at low gain
 float stackMakeupDb;
 float powerDb;           // power amp drive at master 5
 float headroom;          // power amp clip level
 float sag;               // how much the supply droops
 float outHp, outLp;      // output transformer
 float presenceHz;
 float trimDb;            // matches the models' loudness
 bool bass, fuzz, radio;
};

inline const std::array<ModelSpec, numModels>& modelSpecs() {
 using namespace amp;
 static const std::array<ModelSpec, numModels> table{{
  // Clean US: two stages, stack after the first, lots of power amp headroom.
  {blackface, 2, 1, -8, 26, {{0, 14, 0, 0}}, {{20, 30, 0, 0}}, {{9000, 12000, 0, 0}}, {{.25f, .3f, 0, 0}}, 5, 22, -6, 1.1f, .5f, 45, 11000, 4000, 6, false, false, false},
  // Chime UK: top boost, class A power amp that breaks up early.
  {chime, 2, 2, -4, 28, {{0, 18, 0, 0}}, {{30, 60, 0, 0}}, {{9000, 10000, 0, 0}}, {{.3f, .35f, 0, 0}}, 3, 20, 0, .8f, .8f, 60, 10000, 3500, 0, false, false, false},
  // Crunch UK: three stages, stack at the end (cathode follower).
  {british, 3, 3, -2, 32, {{0, 20, 16, 0}}, {{25, 80, 30, 0}}, {{10000, 9000, 9000, 0}}, {{.2f, .35f, .3f, 0}}, 4, 24, -2, 1.f, .6f, 55, 10000, 3800, -5, false, false, false},
  // Lead UK: four stages.
  {british, 4, 4, 2, 32, {{0, 22, 20, 14}}, {{30, 120, 60, 30}}, {{9000, 8000, 7500, 8000}}, {{.2f, .4f, .3f, .35f}}, 3, 24, -3, 1.f, .55f, 60, 9500, 3600, -5, false, false, false},
  // Modern: tight, very high gain, scooped.
  {modern, 4, 4, 4, 34, {{0, 24, 22, 16}}, {{40, 300, 180, 40}}, {{9000, 7500, 7000, 7500}}, {{.15f, .4f, .35f, .3f}}, 2, 24, -4, 1.f, .35f, 70, 9000, 3800, -5, false, false, false},
  // Fuzz: a two-transistor fuzz into a clean British amp.
  {british, 2, 2, -14, 8, {{0, 12, 0, 0}}, {{30, 40, 0, 0}}, {{7000, 9000, 0, 0}}, {{.2f, .3f, 0, 0}}, 0, 22, -4, 1.f, .6f, 55, 9000, 3600, -4, false, true, false},
  // Bass Clean: huge headroom, deep transformer.
  {tweed, 2, 1, -10, 22, {{0, 12, 0, 0}}, {{12, 20, 0, 0}}, {{12000, 14000, 0, 0}}, {{.2f, .25f, 0, 0}}, 2, 20, -8, 1.3f, .4f, 25, 13000, 3500, 12, true, false, false},
  // Bass Grind: drive on the mids and highs, the lows stay clean.
  {tweed, 3, 3, -2, 30, {{0, 18, 12, 0}}, {{12, 60, 40, 0}}, {{9000, 8000, 9000, 0}}, {{.25f, .35f, .3f, 0}}, 2, 22, -4, 1.1f, .5f, 25, 11000, 3500, 0, true, false, false},
  // Dying Radio: a tired little valve radio, crossover crackle, sputter.
  {tweed, 2, 1, 0, 30, {{0, 16, 0, 0}}, {{200, 300, 0, 0}}, {{4000, 3500, 0, 0}}, {{.45f, .5f, 0, 0}}, 0, 22, 0, .6f, 1.f, 250, 3200, 2500, 2, false, false, true},
 }};
 return table;
}

class Amp {
public:
 static constexpr int osFactor = 8;
 // Group delay of the three half-band stages, up and down: 37.5 base-rate
 // samples (31/2 + 10/4 + 6/8, twice). The DI path is delayed by 37.
 static constexpr int latency = 37;

 void prepare(double sampleRate) {
  fs = sampleRate;
  osFs = fs * osFactor;
  spring.prepare(fs);
  diDelay.assign(64, 0.f);
  reset();
  controlCountdown = 0;
  smoothedReady = false;
 }
 void reset() {
  stage1Up.reset(); stage1Down.reset(); stage2Up.reset(); stage2Down.reset(); stage3Up.reset(); stage3Down.reset();
  for (auto& s : stages) s = {};
  stack.reset(); tight.reset(); presence.reset(); outLp.reset(); outHp.z = 0; bright.reset(); lowSplit.z = 0;
  spring.reset();
  gateGain = 0; gateEnv = 0; gateOpen = false; gateHold = 0;
  sagEnv = 0; blockEnv = 0; fuzzZ = 0; radioCrackle = 0; radioDrop = 1;
  std::fill(diDelay.begin(), diDelay.end(), 0.f);
  diPos = 0;
 }
 void setValues(const Values& v) { target = v; }

 // In: mono input (the guitar). Out: the amp before the cabinet, and the
 // input delayed by the amp's latency (for the DI blend).
 void process(const float* in, float* out, float* di, int n) {
  for (int i = 0; i < n; ++i) {
   if (controlCountdown <= 0) { control(); controlCountdown = controlInterval; }
   --controlCountdown;
   float x = in[i];
   // noise gate on the input, with hysteresis and hold
   gateEnv = std::max(std::abs(x), gateEnv * gateEnvFall);
   if (gateThreshold > 0) {
    if (gateEnv > gateThreshold) { gateOpen = true; gateHold = gateHoldSamples; }
    else if (gateEnv < gateThreshold * .5f) { if (gateHold > 0) --gateHold; else gateOpen = false; }
    gateGain += (gateOpen ? gateAttack : gateRelease) * ((gateOpen ? 1.f : 0.f) - gateGain);
    x *= gateGain;
   }
   diDelay[diPos] = x;
   di[i] = diDelay[(diPos + diDelay.size() - static_cast<std::size_t>(latency)) % diDelay.size()];
   diPos = (diPos + 1) % diDelay.size();
   x = tight.process(x);
   float a, b;
   float s2[4], s3[8];
   stage1Up.up(x, a, b);
   stage2Up.up(a, s2[0], s2[1]);
   stage2Up.up(b, s2[2], s2[3]);
   for (int k = 0; k < 4; ++k) stage3Up.up(s2[k], s3[2 * k], s3[2 * k + 1]);
   for (float& u : s3) u = oversampled(u);
   for (int k = 0; k < 4; ++k) s2[k] = stage3Down.down(s3[2 * k], s3[2 * k + 1]);
   a = stage2Down.down(s2[0], s2[1]);
   b = stage2Down.down(s2[2], s2[3]);
   float y = stage1Down.down(a, b);
   y = spring.process(y, springAmount);
   out[i] = y * outGain;
  }
 }

 const ModelSpec& model() const { return modelSpecs()[static_cast<std::size_t>(modelIndex)]; }
 int modelNumber() const { return modelIndex; }
 float gateReduction() const { return gateGain; }

private:
 struct StageState { amp::OnePole hp, lp; };

 float oversampled(float x) {
  const auto& m = model();
  if (m.fuzz) x = fuzz(x);
  // Bass Grind: the lows go around the drive and come back after it.
  float lows = 0;
  if (m.bass && m.stages >= 3) { lows = lowSplit.low(x); x -= lows; }
  x = bright.process(x * inGain);
  for (int k = 0; k < m.stages; ++k) {
   auto& st = stages[static_cast<std::size_t>(k)];
   if (k > 0) x *= stageGain[static_cast<std::size_t>(k)];
   x = st.hp.high(x);
   // grid current shifts the bias of a hard-driven stage (blocking)
   const float bias = m.bias[static_cast<std::size_t>(k)] - .5f * blockEnv;
   x = -(amp::fastTanh(x + bias) - amp::fastTanh(bias)); // a triode inverts
   x = st.lp.low(x);
   if (k + 1 == m.stackAfter) x = stack.process(x) * stackMakeup;
  }
  if (m.radio) x = radio(x);
  blockEnv += (std::max(0.f, std::abs(x) - .6f) - blockEnv) * blockCoef;
  if (m.bass && m.stages >= 3) x = x * .55f + lows * inGain * stackMakeup * .12f;
  // power amp: push-pull, the supply sags under load
  const float head = m.headroom * (1.f - sagAmount * sagEnv);
  const float driven = x * powerGain / head;
  float p = driven / std::sqrt(1.f + driven * driven);
  p *= head;
  sagEnv += ((std::abs(p) > sagEnv ? sagAttack : sagRelease)) * (std::abs(p) - sagEnv);
  p = presence.process(p);
  p = outLp.process(p);
  return outHp.high(p);
 }

 // Fuzz: germanium-like asymmetric clipping with a starved bias that sputters.
 float fuzz(float x) {
  x = fuzzIn.low(x) * fuzzGain;  // the input capacitor and the pickup's own top end
  const float y = x > 0 ? amp::fastTanh(1.4f * x) : -.75f * amp::fastTanh(-2.f * x / .75f);
  fuzzZ += .002f * (y - fuzzZ);
  return (y - fuzzZ) * .8f;
 }

 // The tired radio: crossover dead zone, crackle and the volume dropping out.
 float radio(float x) {
  const float dead = .04f + .06f * sagEnv;
  x = x > dead ? x - dead : x < -dead ? x + dead : 0.f;
  if (rng.next() < 3.f / static_cast<float>(osFs)) radioCrackle = (rng.next() * 2 - 1) * .5f;
  radioCrackle *= .995f;
  if (rng.next() < .4f / static_cast<float>(osFs)) radioDropTarget = .2f + .5f * rng.next();
  if (rng.next() < 2.f / static_cast<float>(osFs)) radioDropTarget = 1.f;
  radioDrop += .0002f * (radioDropTarget - radioDrop);
  return x * radioDrop + radioCrackle * .3f;
 }

 void control() {
  if (!smoothedReady) { values = target; smoothedReady = true; }
  for (std::size_t i = 0; i < values.size(); ++i) {
   if (specs()[i].kind == Kind::Float) values[i] += .2f * (target[i] - values[i]);
   else values[i] = target[i];
  }
  const int newModel = std::clamp(static_cast<int>(std::lround(values[P::Model])), 0, numModels - 1);
  if (newModel != modelIndex) {
   modelIndex = newModel;
   for (auto& s : stages) s = {};
   stack.reset();
  }
  const auto& m = model();
  const double gain = values[P::Gain] / 10.0;
  inGain = std::pow(10.f, static_cast<float>((m.gainMinDb + m.gainRangeDb * gain) / 20.0));
  fuzzGain = std::pow(10.f, static_cast<float>((6.0 + 24.0 * gain) / 20.0));
  fuzzIn.setLow(4500, osFs);
  for (int k = 0; k < 4; ++k) {
   auto& st = stages[static_cast<std::size_t>(k)];
   stageGain[static_cast<std::size_t>(k)] = std::pow(10.f, m.stageGainDb[static_cast<std::size_t>(k)] / 20.f);
   st.hp.setLow(std::max(5.f, m.stageHp[static_cast<std::size_t>(k)]), osFs);
   st.lp.setLow(std::max(1000.f, m.stageLp[static_cast<std::size_t>(k)]), osFs);
  }
  bright.highShelf(1800, m.brightDb * (1 - gain), osFs);
  stack.design(m.stack, amp::taperTreble(values[P::Treble] / 10.0), amp::taperBass(values[P::Bass] / 10.0), amp::taperMid(values[P::Mid] / 10.0), osFs);
  stackMakeup = std::pow(10.f, m.stackMakeupDb / 20.f);
  const float masterDb = m.powerDb + (values[P::Master] - 5.f) * 2.4f;
  powerGain = std::pow(10.f, masterDb / 20.f);
  sagAmount = m.sag * values[P::Sag] / 100.f * .6f;
  presence.highShelf(m.presenceHz, (values[P::Presence] - 5.f) * 1.4f, osFs);
  outLp.lowpass(m.outLp, .6, osFs);
  outHp.setLow(m.outHp, osFs);
  lowSplit.setLow(250, osFs);
  tight.highpass(40.0 * std::pow(10.0, values[P::Tight] / 100.0), .7, fs);
  springAmount = values[P::Spring] / 100.f;
  // Louder with more master, but not by as much as the drive rises.
  outGain = std::pow(10.f, (m.trimDb - .7f * (masterDb - m.powerDb) + values[P::Output]) / 20.f);
  const float gateDb = values[P::Gate];
  gateThreshold = gateDb <= -89.9f ? 0.f : std::pow(10.f, gateDb / 20.f);
  if (gateThreshold <= 0) gateGain = 1.f;
 }

 double fs = 48000, osFs = 384000;
 amp::TwoX stage1Up{63}, stage1Down{63}, stage2Up{21}, stage2Down{21}, stage3Up{13}, stage3Down{13};
 std::array<StageState, 4> stages{};
 std::array<float, 4> stageGain{};
 amp::ToneStack stack;
 amp::Biquad tight, presence, outLp, bright;
 amp::OnePole outHp, lowSplit, fuzzIn;
 amp::Spring spring;
 amp::Rng rng;
 Values target = defaults(), values = defaults();
 bool smoothedReady = false;
 int modelIndex = -1;
 int controlCountdown = 0;
 static constexpr int controlInterval = 32;
 float inGain = 1, fuzzGain = 1, stackMakeup = 1, powerGain = 1, sagAmount = 0, springAmount = 0, outGain = 1;
 float sagEnv = 0, blockEnv = 0, fuzzZ = 0;
 const float sagAttack = .002f, sagRelease = .00005f, blockCoef = .0005f;
 float radioCrackle = 0, radioDrop = 1, radioDropTarget = 1;
 // gate
 float gateThreshold = 0, gateGain = 1, gateEnv = 0;
 bool gateOpen = false;
 int gateHold = 0;
 const float gateEnvFall = .9995f, gateAttack = .02f, gateRelease = .0015f;
 const int gateHoldSamples = 2400;
 std::vector<float> diDelay;
 std::size_t diPos = 0;
};

} // namespace inverso_abismo::furnace
