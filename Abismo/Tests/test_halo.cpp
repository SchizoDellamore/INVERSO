// HALO: dry when the mix is at zero, a reverb tail when not, the gated room
// stops dead after a hit and opens again on the next, every preset sane, a
// parameter storm.
#include "TestUtil.h"
#include "../Source/DSP/HaloEngine.h"
#include <cstdio>
#include <random>

namespace H = inverso_abismo::halo;
using test::verify;
using namespace H::P;
constexpr int sr = 48000;

// a snare-ish hit every `period` samples
static std::vector<float> hits(int n, int period) {
 std::vector<float> x(static_cast<std::size_t>(n), 0.f);
 std::mt19937 rng(9); std::normal_distribution<float> g(0, .3f);
 for (int start = 0; start < n; start += period)
  for (int i = 0; i < 4800 && start + i < n; ++i) x[static_cast<std::size_t>(start + i)] = g(rng) * std::exp(-i / 700.f);
 return x;
}
static std::vector<float> run(H::Values v, std::vector<float> x) {
 H::Engine e; e.setValues(v); e.prepare(sr);
 for (std::size_t i = 0; i < x.size(); i += 256) e.process(x.data() + i, nullptr, static_cast<int>(std::min<std::size_t>(256, x.size() - i)));
 return x;
}

int main() {
 // 1. mix at zero: the sound passes untouched
 {
  auto v = H::defaults(); v[Mix] = 0; v[Output] = 0;
  const auto in = hits(sr, sr);
  const auto out = run(v, in);
  float err = 0; for (std::size_t i = 0; i < in.size(); ++i) err = std::max(err, std::abs(out[i] - in[i]));
  std::printf("mix 0: worst difference %.2e\n", err);
  verify(err < 1e-5f, "mix 0 changes the sound");
 }
 // 2. gated vs hall: the tail after a hit
 auto tail = [&](int mode) {
  auto v = H::defaults(); v[Mode] = static_cast<float>(mode); v[Mix] = 100; v[Decay] = 5; v[GateHold] = 200; v[GateRelease] = 30; v[GateThresh] = -30;
  const auto out = run(v, hits(sr * 2, sr * 4)); // one hit
  return std::pair<float, float>{test::rms(out, 2400, 9600), test::rms(out, sr / 2, sr)};
 };
 const auto hall = tail(1), gated = tail(3);
 std::printf("hall: body %.4f, 0.5-1 s %.4f;  gated: body %.4f, 0.5-1 s %.6f\n", hall.first, hall.second, gated.first, gated.second);
 verify(gated.first > hall.first * .3f, "the gated room has no body");
 verify(gated.second < hall.second * .02f, "the gated room is not cut after the hit");
 // 3. the gate opens again on the next hit
 {
  auto v = H::defaults(); v[Mode] = 3; v[Mix] = 100;
  const auto out = run(v, hits(sr * 2, sr / 2));
  const float second = test::rms(out, sr / 2 + 1200, sr / 2 + 6000), gap = test::rms(out, sr / 2 - 4800, sr / 2 - 200);
  std::printf("gated: second hit %.4f, just before it %.6f\n", second, gap);
  verify(second > gap * 20.f, "the gate does not open again on the next hit");
 }
 // 4. every preset finite and sensible
 for (std::size_t p = 0; p < H::presets().size(); ++p) {
  const auto in = hits(sr * 2, sr / 2);
  const auto out = run(H::presetValues(p), in);
  const float ratio = test::rms(out) / test::rms(in);
  for (float x : out) verify(std::isfinite(x) && std::abs(x) < 4.f, "preset output invalid");
  std::printf("%-24s level %+.1f dB\n", H::presets()[p].name, 20 * std::log10(ratio));
  verify(std::abs(20 * std::log10(ratio)) < 8, "a preset changes the level too much");
 }
 // 5. stereo stays stereo, mono works
 {
  H::Engine e; e.setValues(H::presetValues(8)); e.prepare(sr);
  auto l = hits(sr, sr / 4), r = l;
  for (std::size_t i = 0; i < l.size(); i += 256) e.process(l.data() + i, r.data() + i, static_cast<int>(std::min<std::size_t>(256, l.size() - i)));
  double diff = 0; for (std::size_t i = 0; i < l.size(); ++i) diff += std::pow(l[i] - r[i], 2);
  verify(diff > 1e-3, "the reverb has no width");
 }
 // 5b. turning the reverb type, pre-delay and mix while it plays does not click. Glides bend the
 // pitch of a 220 Hz tone (that is fine); a click is broadband, so measure what passes above 8 kHz.
 {
  H::Engine e; auto v = H::presetValues(8); e.setValues(v); e.prepare(sr);
  std::vector<float> l(256), r(256);
  float worst = 0, steady = 0;
  double h1 = 0, h2 = 0, h3 = 0, h4 = 0, x1 = 0, x2 = 0, x3 = 0, x4 = 0;
  const double rc = 1.0 / (1.0 + 2 * 3.14159265 * 8000.0 / sr);
  for (int b = 0; b < sr * 6 / 256; ++b) {
   if (b > sr * 3 / 256) { v[Mode] = static_cast<float>((b / 40) % 4); v[PreDelay] = (b / 25) % 2 ? 10.f : 180.f; v[Mix] = (b / 33) % 2 ? 20.f : 60.f; e.setValues(v); }
   for (int i = 0; i < 256; ++i) l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = .3f * std::sin(6.2831853f * 220.f * static_cast<float>(b * 256 + i) / sr);
   e.process(l.data(), r.data(), 256);
   for (float x : l) {
    // four one-pole high-passes at 8 kHz
    h1 = rc * (h1 + x - x1); x1 = x; h2 = rc * (h2 + h1 - x2); x2 = h1; h3 = rc * (h3 + h2 - x3); x3 = h2; h4 = rc * (h4 + h3 - x4); x4 = h3;
    const float a = static_cast<float>(std::abs(h4));
    if (b > 20 && b <= sr * 3 / 256) steady = std::max(steady, a);
    if (b > sr * 3 / 256) worst = std::max(worst, a);
   }
  }
  std::printf("halo: above 8 kHz %.5f while turning, %.5f held\n", worst, steady);
  verify(worst < steady * 3.f + .001f, "turning HALO's controls clicks");
 }
 // 6. parameter storm
 {
  H::Engine e; e.prepare(44100);
  std::mt19937 rng(4); std::uniform_real_distribution<float> u(0, 1);
  std::vector<float> l(256), r(256);
  for (int b = 0; b < 3000; ++b) {
   H::Values v;
   for (std::size_t i = 0; i < v.size(); ++i) v[i] = H::specs()[i].min + u(rng) * (H::specs()[i].max - H::specs()[i].min);
   e.setValues(v);
   for (std::size_t i = 0; i < l.size(); ++i) { l[i] = (u(rng) - .5f); r[i] = (u(rng) - .5f); }
   e.process(l.data(), r.data(), 256);
   for (std::size_t i = 0; i < l.size(); ++i) verify(std::isfinite(l[i]) && std::abs(l[i]) < 20 && std::isfinite(r[i]), "unstable under a parameter storm");
  }
 }
 std::puts("halo: all good");
 return 0;
}
