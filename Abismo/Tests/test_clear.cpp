// CLEAR: perfect reconstruction, transparency on a balanced sound, a resonance
// and a low-mid build-up taken down, delta, mono, a parameter storm.
#include "TestUtil.h"
#include "../Source/DSP/ClearEngine.h"
#include <complex>
#include <random>

using namespace inverso_abismo::clear;
using test::verify;

static std::vector<float> pink(std::size_t n, unsigned seed, float level = .1f) {
 std::mt19937 rng(seed);
 std::normal_distribution<float> g(0.f, 1.f);
 std::vector<float> x(n);
 float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0; // Paul Kellet's filter
 for (auto& v : x) {
  const float w = g(rng);
  b0 = .99886f * b0 + w * .0555179f; b1 = .99332f * b1 + w * .0750759f; b2 = .96900f * b2 + w * .1538520f;
  b3 = .86650f * b3 + w * .3104856f; b4 = .55000f * b4 + w * .5329522f; b5 = -.7616f * b5 - w * .0168980f;
  v = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * .5362f) * level * .11f;
  b6 = w * .115926f;
 }
 return x;
}
// RBJ peaking filter applied in place.
static void bell(std::vector<float>& x, double f, double db, double q, double fs) {
 const double A = std::pow(10.0, db / 40), w = 2 * 3.14159265358979 * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
 const double b0 = 1 + al * A, b1 = -2 * c, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * c, a2 = 1 - al / A;
 double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
 for (auto& v : x) {
  const double y = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
  x2 = x1; x1 = v; y2 = y1; y1 = y; v = static_cast<float>(y);
 }
}
static std::vector<float> run(Engine& e, std::vector<float> x, int block = 333) {
 for (std::size_t i = 0; i < x.size(); i += static_cast<std::size_t>(block)) {
  const int n = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(block), x.size() - i));
  e.process(x.data() + i, nullptr, n);
 }
 return x;
}
// Level of the output relative to the (delayed) input in a third of an octave around f.
static float bandChange(const std::vector<float>& in, const std::vector<float>& out, int latency, double f, double fs) {
 const std::size_t from = 48000, n = 1 << 17;
 std::vector<std::complex<double>> A(n), B(n);
 for (std::size_t i = 0; i < n; ++i) {
  const double w = .5 - .5 * std::cos(2 * 3.14159265358979 * static_cast<double>(i) / static_cast<double>(n));
  A[i] = in[from + i] * w;
  B[i] = out[from + i + static_cast<std::size_t>(latency)] * w;
 }
 inverso_abismo::clear::dsp::FFT unused; (void)unused;
 auto fft = [](std::vector<std::complex<double>>& a) {
  const std::size_t N = a.size();
  for (std::size_t i = 1, j = 0; i < N; ++i) { std::size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
  for (std::size_t len = 2; len <= N; len <<= 1) {
   const auto wl = std::polar(1.0, -2 * 3.14159265358979 / static_cast<double>(len));
   for (std::size_t i = 0; i < N; i += len) { std::complex<double> w(1); for (std::size_t j = 0; j < len / 2; ++j) { const auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
  }
 };
 fft(A); fft(B);
 double pa = 0, pb = 0;
 for (std::size_t k = 1; k < n / 2; ++k) {
  const double fk = fs * static_cast<double>(k) / static_cast<double>(n);
  if (fk < f / std::pow(2.0, 1.0 / 6) || fk > f * std::pow(2.0, 1.0 / 6)) continue;
  pa += std::norm(A[k]); pb += std::norm(B[k]);
 }
 return static_cast<float>(10 * std::log10(pb / pa));
}

int main() {
 const double fs = 48000;
 const std::size_t len = static_cast<std::size_t>(fs * 6);
 const double bands[] = {63, 125, 250, 500, 1000, 2000, 3000, 4000, 6000, 8000, 12000};
 // 1. Nothing to take away: the output is the input, delayed by the latency.
 {
  Engine e;
  e.prepare(fs);
  auto v = defaults();
  v[P::MudDepth] = 0; v[P::HarshDepth] = 0;
  e.setValues(v);
  const auto in = pink(len, 1);
  const auto out = run(e, in);
  double err = 0, sig = 0;
  for (std::size_t i = 10000; i < len; ++i) { const double d = out[i] - in[i - static_cast<std::size_t>(e.latency())]; err += d * d; sig += static_cast<double>(in[i]) * in[i]; }
  std::printf("reconstruction error: %.1f dB\n", 10 * std::log10(err / sig));
  verify(10 * std::log10(err / sig) < -100, "the transform does not reconstruct the input");
 }
 // 2. A balanced sound (pink noise) passes almost untouched with the defaults.
 {
  Engine e;
  e.prepare(fs);
  e.setValues(defaults());
  const auto in = pink(len, 2);
  const auto out = run(e, in);
  float worst = 0;
  for (double f : bands) worst = std::min(worst, bandChange(in, out, e.latency(), f, fs));
  std::printf("pink noise, defaults: largest cut %.2f dB\n", -worst);
  verify(worst > -1.0f, "CLEAR colours a balanced sound");
 }
 // 3. A ringing resonance at 3.2 kHz (+15 dB, narrow) is taken down; the rest stays.
 {
  Engine e;
  e.prepare(fs);
  e.setValues(defaults());
  auto in = pink(len, 3);
  bell(in, 3200, 15, 12, fs);
  const auto out = run(e, in);
  const float at = bandChange(in, out, e.latency(), 3200, fs);
  float elsewhere = 0;
  for (double f : {250.0, 1000.0, 6000.0, 10000.0}) elsewhere = std::min(elsewhere, bandChange(in, out, e.latency(), f, fs));
  std::printf("resonance: %.2f dB at 3.2 kHz, at most %.2f dB elsewhere\n", at, elsewhere);
  verify(at < -2.5f, "the resonance was not taken down");
  verify(elsewhere > -1.f, "taking the resonance down hurt the rest");
 }
 // 4. A low-mid build-up (+9 dB around 250 Hz) is taken back; a clean low end is not.
 {
  Engine e;
  e.prepare(fs);
  e.setValues(defaults());
  auto in = pink(len, 4);
  bell(in, 250, 9, 1.0, fs);
  const auto out = run(e, in);
  const float at = bandChange(in, out, e.latency(), 250, fs);
  const float high = bandChange(in, out, e.latency(), 4000, fs);
  std::printf("mud: %.2f dB at 250 Hz, %.2f dB at 4 kHz\n", at, high);
  verify(at < -2.f, "the mud was not taken back");
  verify(high > -1.f, "taking the mud back hurt the highs");
 }
 // 5. Delta is what the normal output takes away: the two add back to the input.
 {
  auto in = pink(len, 5);
  bell(in, 3200, 15, 12, fs);
  Engine a, b;
  a.prepare(fs); b.prepare(fs);
  auto v = defaults();
  a.setValues(v);
  v[P::Delta] = 1;
  b.setValues(v);
  const auto wet = run(a, in), delta = run(b, in);
  double err = 0, sig = 0;
  for (std::size_t i = 10000; i < len; ++i) { const double d = wet[i] + delta[i] - in[i - static_cast<std::size_t>(a.latency())]; err += d * d; sig += static_cast<double>(in[i]) * in[i]; }
  verify(10 * std::log10(err / sig) < -90, "wet + delta does not give the input back");
  std::puts("delta: wet + delta = input");
 }
 // 6. Stereo with identical channels stays identical; silence stays silent.
 {
  Engine e;
  e.prepare(44100);
  e.setValues(defaults());
  auto l = pink(88200, 6), r = l;
  for (std::size_t i = 0; i < l.size(); i += 512) {
   const int n = static_cast<int>(std::min<std::size_t>(512, l.size() - i));
   e.process(l.data() + i, r.data() + i, n);
  }
  for (std::size_t i = 0; i < l.size(); ++i) verify(std::abs(l[i] - r[i]) < 1e-6f, "identical channels came out different");
  std::vector<float> s(88200, 0.f);
  s = run(e, s);
  std::vector<float> zero(88200, 0.f);
  const auto quiet = run(e, zero);
  for (float x : quiet) verify(std::abs(x) < 1e-7f, "silence in, sound out");
  std::puts("stereo link and silence: fine");
 }
 // 7. Parameter storm at 96 kHz (the 4096-point transform).
 {
  Engine e;
  e.prepare(96000);
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> u(0.f, 1.f);
  auto in = pink(96000 * 4, 8, .3f);
  for (std::size_t i = 0; i < in.size(); i += 256) {
   if ((i / 256) % 10 == 0) {
    Values v = defaults();
    for (std::size_t k = 0; k < v.size(); ++k) v[k] = specs()[k].min + u(rng) * (specs()[k].max - specs()[k].min);
    e.setValues(v);
   }
   e.process(in.data() + i, nullptr, 256);
  }
  for (float x : in) verify(std::isfinite(x) && std::abs(x) < 10.f, "unstable output");
  std::puts("parameter storm: stable");
 }
 std::puts("CLEAR tests passed");
}
