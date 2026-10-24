#pragma once
// FURNACE cabinets: the INVERSO CABS designer (IRs/tools/make_cab_irs.py) in
// C++, so the microphone can move continuously. A speaker, its cabinet, a
// microphone and its place become a magnitude response, made minimum phase
// like a close-miked speaker; open backs, the floor and the room are added in
// time. The randomness (cone breakup, room tail) uses the same generator as the
// Python tool, so the same settings give the same cabinets as the IR pack.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace inverso_abismo::furnace {

struct Bell { double f, gain, q; };
struct Speaker {
 const char* name;
 double lowF, lowQ;
 std::array<Bell, 5> bells;
 int numBells;
 double lpF, lpQ, lp2F;
 double breakupDepth, breakupStart;
 std::uint32_t seed;
 bool openBack, bass;
 bool horn; double hornLo, hornHi, hornLevel;
};

inline const std::array<Speaker, 13>& speakers() {
 static const std::array<Speaker, 13> table{{
  {"Brit 4x12 V", 85, 1.5, {{{110, 2.0, 2.2}, {500, -3.5, 1.2}, {1400, 1.5, 1.5}, {2600, 4.0, 2.2}, {4200, 5.0, 2.5}}}, 5, 5200, 1.1, 7500, 2.4, 900, 11, false, false, false, 0, 0, 0},
  {"Brit 4x12 G", 80, 1.3, {{{105, 2.0, 2.0}, {650, -2.0, 1.0}, {1900, 3.0, 1.4}, {3200, 1.5, 2.0}}}, 4, 4600, .9, 6800, 1.8, 1000, 23, false, false, false, 0, 0, 0},
  {"Brit 2x12 Blue", 95, 1.2, {{{120, 1.0, 2.0}, {800, -1.5, 1.0}, {2300, 2.0, 1.6}, {3700, 4.5, 2.2}, {5300, 2.0, 3.0}}}, 5, 6200, 1.0, 9000, 2.2, 900, 37, false, false, false, 0, 0, 0},
  {"Brit 1x12 Open", 105, .9, {{{160, 1.0, 1.5}, {700, -1.0, 1.0}, {2800, 3.0, 1.8}, {4500, 2.0, 2.5}}}, 4, 5600, .9, 8500, 2.0, 1000, 41, true, false, false, 0, 0, 0},
  {"Amer 2x12 Open", 100, .85, {{{130, 1.0, 1.8}, {450, -2.5, 1.0}, {1700, 1.0, 1.4}, {3300, 3.5, 1.6}, {5600, 2.0, 2.5}}}, 5, 6600, .9, 9500, 2.0, 1100, 53, true, false, false, 0, 0, 0},
  {"Amer 1x12 Open", 110, .8, {{{180, 1.5, 1.5}, {600, -1.5, 1.0}, {2500, 2.0, 1.4}, {4400, 3.0, 2.0}}}, 4, 6000, .85, 9000, 2.2, 1000, 67, true, false, false, 0, 0, 0},
  {"Amer 4x10 Open", 115, .9, {{{160, 2.0, 1.6}, {500, -1.5, 1.2}, {1600, 2.0, 1.4}, {3600, 3.0, 2.0}}}, 4, 5800, .9, 8500, 2.6, 900, 71, true, false, false, 0, 0, 0},
  {"Modern 4x12", 75, 1.6, {{{95, 3.0, 2.2}, {420, -4.5, 1.1}, {1300, -1.0, 1.5}, {2500, 3.5, 2.0}, {4400, 5.0, 2.8}}}, 5, 5400, 1.2, 7400, 2.0, 1000, 83, false, false, false, 0, 0, 0},
  {"Bass 8x10", 58, 1.2, {{{75, 2.5, 1.6}, {300, -1.5, 1.2}, {800, 2.5, 1.2}, {1800, 2.0, 1.5}}}, 4, 4200, .8, 6500, 1.6, 900, 101, false, true, false, 0, 0, 0},
  {"Bass 1x15 Flip", 48, 1.3, {{{60, 3.0, 1.6}, {450, -2.0, 1.0}, {1100, 1.5, 1.4}}}, 3, 2800, .8, 4500, 1.4, 700, 113, false, true, false, 0, 0, 0},
  {"Bass 4x10 Horn", 62, 1.0, {{{85, 1.5, 1.5}, {500, -2.0, 1.0}, {1500, 2.0, 1.5}, {2600, 1.5, 2.0}}}, 4, 3800, .8, 6000, 1.8, 900, 127, false, true, true, 3800, 12000, -9.0},
  {"Bass 2x12 Modern", 52, 1.1, {{{70, 2.0, 1.4}, {380, -3.0, 1.0}, {1000, 1.0, 1.4}, {2200, 2.0, 1.8}}}, 4, 3400, .85, 5200, 1.5, 800, 131, false, true, true, 3600, 15000, -7.0},
  {"Bass 1x18 Sub", 38, 1.0, {{{48, 2.0, 1.4}, {300, -1.0, 1.0}}}, 2, 1400, .7, 2600, 1.0, 500, 149, false, true, false, 0, 0, 0},
 }};
 return table;
}
constexpr int numSpeakers = 13;

struct Mic {
 const char* name;
 std::array<Bell, 2> bells;
 int numBells;
 double hp, lp;
 bool gentle, ribbon;
};
inline const std::array<Mic, 4>& mics() {
 static const std::array<Mic, 4> table{{
  {"Dyn57", {{{5800, 3.0, 1.6}, {2500, 1.0, 1.4}}}, 2, 150, 14000, false, false},
  {"Dyn421", {{{4500, 3.0, 1.4}, {900, -1.0, 1.0}}}, 2, 70, 16000, false, false},
  {"Cond87", {{{9000, 2.0, 1.0}}}, 1, 30, 20000, true, false},
  {"Rib121", {{{120, 1.0, 1.0}, {3500, -1.5, 1.2}}}, 2, 25, 8500, false, true},
 }};
 return table;
}
constexpr int numMics = 4;

// Where the microphone is. The named places of the IR pack are special cases.
struct Placement {
 double bright = 0.0; // dB above 3 kHz: centre of the cone > 0, outer cone < 0
 double axis = 0.0;   // 0 pointing at the cone, 1 angled away
 double dist = .025;  // metres from the grille
 bool back = false;   // behind an open cabinet
 bool room = false;   // the pack's room position (the plugin blends it by distance)
};
inline Placement named(const char* p) {
 const std::string_view n(p);
 if (n == "Cap") return {2.0, 0.0, .025};
 if (n == "CapEdge") return {0.0, 0.0, .025};
 if (n == "Cone") return {-3.5, 0.0, .03};
 if (n == "OffAxis") return {-1.0, 1.0, .04};
 if (n == "Back") return {-4.0, .3, .15, true};
 return {-1.5, .3, 1.2, false, true}; // Room
}
// The plugin's knobs: position 0 (centre) .. 1 (outer cone), axis 0..1,
// distance 0 (touching) .. 1 (1.2 m, with the room).
inline Placement fromKnobs(double position, double axis, double distance) {
 Placement p;
 position = std::clamp(position, 0.0, 1.0);
 p.bright = position < 1.0 / 3.0 ? 2.0 - 6.0 * position : -3.5 * (position - 1.0 / 3.0) / (2.0 / 3.0);
 p.axis = std::clamp(axis, 0.0, 1.0);
 p.dist = .025 * std::pow(1.2 / .025, std::clamp(distance, 0.0, 1.0));
 return p;
}

namespace cabdetail {
constexpr double pi = 3.14159265358979323846;
using cd = std::complex<double>;

inline double hp2(double f, double fc, double q) { const cd s(0, f / fc); return 20 * std::log10(std::abs(s * s / (s * s + s / q + 1.0))); }
inline double lp2(double f, double fc, double q) { const cd s(0, f / fc); return 20 * std::log10(std::abs(1.0 / (s * s + s / q + 1.0))); }
inline double hp1(double f, double fc) { const cd s(0, f / fc); return 20 * std::log10(std::abs(s / (s + 1.0))); }
inline double lp1(double f, double fc) { const cd s(0, f / fc); return 20 * std::log10(std::abs(1.0 / (s + 1.0))); }
inline double peak(double f, double fc, double gain, double q) {
 const double o = std::log2(f / fc), bw = 1.4 / q;
 return gain * std::exp(-.5 * (o / (bw * .5)) * (o / (bw * .5)));
}
inline double shelf(double f, double fc, double gain, double slopeOct) { return gain / (1 + std::exp(-std::log2(f / fc) * 4.0 / slopeOct)); }

// splitmix64 + Box-Muller, identical to the Python tool.
struct Rng {
 std::uint64_t state;
 explicit Rng(std::uint64_t seed) : state(seed) {}
 std::uint64_t next() {
  state += 0x9E3779B97F4A7C15ull;
  std::uint64_t z = state;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
 }
 double uniform() { return (static_cast<double>(next() >> 11) + .5) / 9007199254740992.0; }
 double normal() { const double u1 = uniform(), u2 = uniform(); return std::sqrt(-2 * std::log(u1)) * std::cos(2 * pi * u2); }
};

inline std::uint32_t crc32(const std::string& s) {
 std::uint32_t c = 0xFFFFFFFFu;
 for (char ch : s) {
  c ^= static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
  for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
 }
 return ~c;
}

inline void fft(std::vector<cd>& a, bool inverse) {
 const std::size_t n = a.size();
 for (std::size_t i = 1, j = 0; i < n; ++i) {
  std::size_t bit = n >> 1;
  for (; j & bit; bit >>= 1) j ^= bit;
  j ^= bit;
  if (i < j) std::swap(a[i], a[j]);
 }
 for (std::size_t len = 2; len <= n; len <<= 1) {
  const double ang = 2 * pi / static_cast<double>(len) * (inverse ? 1 : -1);
  const cd wl(std::cos(ang), std::sin(ang));
  for (std::size_t i = 0; i < n; i += len) {
   cd w(1);
   for (std::size_t j = 0; j < len / 2; ++j) {
    const cd u = a[i + j], v = a[i + j + len / 2] * w;
    a[i + j] = u + v;
    a[i + j + len / 2] = u - v;
    w *= wl;
   }
  }
 }
 if (inverse) for (auto& x : a) x /= static_cast<double>(n);
}

// Adds a delayed (fractional), optionally low-passed, scaled copy of x to x.
inline void addDelayed(std::vector<double>& x, double seconds, double fs, double gain, double lowpassHz) {
 const double d = seconds * fs;
 const std::size_t i = static_cast<std::size_t>(d);
 const double frac = d - static_cast<double>(i);
 const std::size_t n = x.size();
 if (i + 1 >= n) return;
 std::vector<double> y(n, 0.0);
 for (std::size_t k = 0; k + i < n; ++k) y[k + i] += (1 - frac) * x[k];
 for (std::size_t k = 0; k + i + 1 < n; ++k) y[k + i + 1] += frac * x[k];
 if (lowpassHz > 0) {
  const double a = std::exp(-2 * pi * lowpassHz / fs);
  double acc = 0;
  for (auto& v : y) { acc = (1 - a) * v + a * acc; v = acc; }
 }
 for (std::size_t k = 0; k < n; ++k) x[k] += gain * y[k];
}
} // namespace cabdetail

// Magnitude in dB at frequency f (Hz).
inline double cabMagnitudeDb(double f, int speaker, int mic, const Placement& p) {
 using namespace cabdetail;
 const auto& s = speakers()[static_cast<std::size_t>(std::clamp(speaker, 0, numSpeakers - 1))];
 const auto& m = mics()[static_cast<std::size_t>(std::clamp(mic, 0, numMics - 1))];
 double db = hp2(f, s.lowF, s.lowQ) + lp2(f, s.lpF, s.lpQ) + 2 * lp2(f, s.lp2F, .7);
 for (int i = 0; i < s.numBells; ++i) {
  const auto& b = s.bells[static_cast<std::size_t>(i)];
  db += peak(f, b.f, b.gain * (b.f > 1500 && !s.bass ? .55 : 1.0), b.q);
 }
 if (!s.bass) db += peak(f, 220, 2.5, .6);
 if (s.openBack) db += hp1(f, 70);
 if (s.horn) {
  const double hf = hp2(f, s.hornLo, .7) + lp2(f, s.hornHi, .7) + s.hornLevel;
  db = 20 * std::log10(std::pow(10.0, db / 20) + std::pow(10.0, hf / 20));
 }
 db += shelf(f, 3000, p.bright, 1.2);
 if (p.axis > 0) db += p.axis * (lp2(f, 3600, .6) + 2.0);
 for (int i = 0; i < m.numBells; ++i) {
  const auto& b = m.bells[static_cast<std::size_t>(i)];
  db += peak(f, b.f, b.gain, b.q);
 }
 db += hp2(f, m.hp, .7) + lp2(f, m.lp, .7);
 if (m.ribbon) db += lp1(f, 9000);
 if (p.dist < .3) {
  const double prox = 6.0 * (1 - p.dist / .3) * (m.gentle ? .5 : 1.0);
  db += prox * (1 - 1 / (1 + (180 / f) * (180 / f)));
 }
 return db;
}

// Cone breakup ripple: knots every 1/10 octave from 20 Hz, interpolated on log f.
struct Breakup {
 std::vector<double> knots;
 double depth = 0, start = 1000;
 Breakup(const Speaker& s) : depth(s.breakupDepth), start(s.breakupStart) {
  const double lo = std::log2(20.0), hi = std::log2(24000.0), w = .1;
  const int n = static_cast<int>(std::ceil((hi - lo) / w)) + 2;
  cabdetail::Rng rng(s.seed);
  knots.resize(static_cast<std::size_t>(n));
  for (auto& k : knots) k = rng.normal();
 }
 double operator()(double f) const {
  const double x = (std::log2(f) - std::log2(20.0)) / .1;
  double r;
  if (x <= 0) r = knots.front();
  else if (x >= static_cast<double>(knots.size() - 1)) r = knots.back();
  else { const auto i = static_cast<std::size_t>(x); const double fr = x - static_cast<double>(i); r = knots[i] + fr * (knots[i + 1] - knots[i]); }
  const double weight = std::clamp(std::log2(f / start) / 2.0, 0.0, 1.0);
  return depth * r * weight;
 }
};

// Pink-weighted loudness, 60 Hz - 8 kHz, as the Python tool measures it.
inline double cabLoudness(const std::vector<double>& h, double fs, std::size_t nfft) {
 using namespace cabdetail;
 std::vector<cd> a(nfft, 0.0);
 for (std::size_t i = 0; i < std::min(h.size(), nfft); ++i) a[i] = h[i];
 fft(a, false);
 double num = 0, den = 0;
 for (std::size_t k = 1; k <= nfft / 2; ++k) {
  const double f = fs * static_cast<double>(k) / static_cast<double>(nfft);
  if (f > 60 && f < 8000) { num += std::norm(a[k]) / f; den += 1 / f; }
 }
 return std::sqrt(num / std::max(den, 1e-30));
}

// The impulse response, loudness-normalised (equal perceived level for every cabinet).
// `roomAmount` 0..1 blends the room (reflections and tail); the pack uses 1 at 1.2 m.
inline std::vector<float> designCab(int speaker, int mic, Placement p, double fs, double roomAmount = -1.0, std::size_t nfft = 1 << 15) {
 using namespace cabdetail;
 const auto& s = speakers()[static_cast<std::size_t>(std::clamp(speaker, 0, numSpeakers - 1))];
 if (roomAmount < 0) roomAmount = p.room ? 1.0 : std::clamp((p.dist - .3) / .9, 0.0, 1.0);
 const std::size_t length = static_cast<std::size_t>((roomAmount > 0 ? .35 : .17) * fs);
 const Breakup breakup(s);
 // log magnitude on the FFT grid, then the real cepstrum folded for minimum phase
 const std::size_t bins = nfft / 2 + 1;
 std::vector<cd> spec(nfft);
 for (std::size_t k = 0; k < bins; ++k) {
  double f = fs * static_cast<double>(k) / static_cast<double>(nfft);
  if (k == 0) f = fs / static_cast<double>(nfft) * .5;
  const double db = cabMagnitudeDb(f, speaker, mic, p) + breakup(f);
  spec[k] = db / 20 * std::log(10.0);
  if (k > 0 && k < nfft / 2) spec[nfft - k] = spec[k];
 }
 fft(spec, true);
 std::vector<cd> fold(nfft, 0.0);
 fold[0] = spec[0].real();
 for (std::size_t k = 1; k < nfft / 2; ++k) fold[k] = 2 * spec[k].real();
 fold[nfft / 2] = spec[nfft / 2].real();
 fft(fold, false);
 for (auto& v : fold) v = std::exp(v);
 fft(fold, true);
 std::vector<double> h(length, 0.0);
 for (std::size_t i = 0; i < std::min(length, nfft); ++i) h[i] = fold[i].real();
 if (s.openBack) addDelayed(h, p.back ? .0045 : .0062, fs, -(p.back ? .55 : .28), 2500);
 if (p.back) for (auto& v : h) v = -v;
 if (roomAmount > 0) {
  const double direct = std::max(p.dist, .3);
  const double refl[5][3] = {{.9, .5, 6000}, {2.6, .32, 4500}, {3.4, .28, 4000}, {4.1, .22, 3500}, {6.0, .16, 3000}};
  for (const auto& r : refl) addDelayed(h, r[0] / 343.0, fs, roomAmount * r[1] * direct / (direct + r[0]), r[2]);
  Rng rng(crc32(std::string(s.name) + "|" + mics()[static_cast<std::size_t>(std::clamp(mic, 0, numMics - 1))].name));
  std::vector<double> tail(length);
  for (std::size_t i = 0; i < length; ++i) tail[i] = rng.normal() * std::exp(-static_cast<double>(i) / (fs * .09));
  std::fill(tail.begin(), tail.begin() + static_cast<std::ptrdiff_t>(std::min(length, static_cast<std::size_t>(.012 * fs))), 0.0);
  const double a = std::exp(-2 * pi * 3000 / fs);
  double acc = 0;
  for (auto& v : tail) { acc = (1 - a) * v + a * acc; v = acc; }
  const std::size_t head = static_cast<std::size_t>(.02 * fs);
  std::vector<double> conv(length, 0.0);
  for (std::size_t j = 0; j < head && j < length; ++j) {
   if (std::abs(h[j]) < 1e-30) continue;
   for (std::size_t i = 0; i + j < length; ++i) conv[i + j] += tail[i] * h[j];
  }
  double tailPeak = 1e-12, hPeak = 0;
  for (double v : conv) tailPeak = std::max(tailPeak, std::abs(v));
  for (double v : h) hPeak = std::max(hPeak, std::abs(v));
  for (std::size_t i = 0; i < length; ++i) h[i] += roomAmount * .035 * conv[i] / tailPeak * hPeak;
  const auto pre = static_cast<std::size_t>(direct / 343.0 * fs * .25 * roomAmount);
  if (pre > 0) { h.insert(h.begin(), pre, 0.0); h.resize(length); }
 }
 const std::size_t fade = length / 4;
 for (std::size_t i = 0; i < fade; ++i) {
  const double c = std::cos(static_cast<double>(i) / static_cast<double>(fade - 1) * pi / 2);
  h[length - fade + i] *= c * c;
 }
 const double loud = cabLoudness(h, fs, nfft);
 std::vector<float> out(length);
 for (std::size_t i = 0; i < length; ++i) out[i] = static_cast<float>(h[i] / loud * .089); // ~ -21 dB: headroom for the convolver
 return out;
}

} // namespace inverso_abismo::furnace
