// Renders CLEAR's demos: a harsh distorted guitar and a muddy guitar + bass mix,
// each before, after and "what is taken away" (delta).  clear_demos <folder>
#include "TestUtil.h"
#include "FurnaceDI.h"
#include "../Source/DSP/ClearEngine.h"
#include "../Source/DSP/FurnaceAmp.h"
#include "../Source/DSP/FurnaceCab.h"
#include <complex>

namespace fu = inverso_abismo::furnace;
namespace cl = inverso_abismo::clear;

static std::vector<float> convolve(const std::vector<float>& x, const std::vector<float>& h) {
 std::size_t n = 1;
 while (n < x.size() + h.size()) n <<= 1;
 std::vector<std::complex<double>> X(n), H(n);
 for (std::size_t i = 0; i < x.size(); ++i) X[i] = x[i];
 for (std::size_t i = 0; i < h.size(); ++i) H[i] = h[i];
 fu::cabdetail::fft(X, false); fu::cabdetail::fft(H, false);
 for (std::size_t i = 0; i < n; ++i) X[i] *= H[i];
 fu::cabdetail::fft(X, true);
 std::vector<float> y(x.size());
 for (std::size_t i = 0; i < y.size(); ++i) y[i] = static_cast<float>(X[i].real());
 return y;
}
static std::vector<float> amp(const std::vector<float>& di, fu::Values v, double fs) {
 fu::Amp a; a.prepare(fs); a.setValues(v);
 std::vector<float> out(di.size()), d(di.size());
 for (std::size_t i = 0; i < di.size(); i += 256) {
  const int n = static_cast<int>(std::min<std::size_t>(256, di.size() - i));
  a.process(di.data() + i, out.data() + i, d.data() + i, n);
 }
 const int cab = static_cast<int>(v[fu::P::Cab]);
 auto wet = convolve(out, fu::designCab(cab - 1, static_cast<int>(v[fu::P::Mic]), fu::fromKnobs(v[fu::P::Position] / 100.0, v[fu::P::Axis] / 100.0, v[fu::P::Distance] / 100.0), fs));
 const float blend = v[fu::P::DIBlend] / 100.f;
 for (std::size_t i = 0; i < wet.size(); ++i) wet[i] = wet[i] * 4.f * (1 - blend) + d[i] * blend;
 return wet;
}
static void bell(std::vector<float>& x, double f, double db, double q, double fs) {
 const double A = std::pow(10.0, db / 40), w = 2 * 3.14159265358979 * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
 const double b0 = 1 + al * A, b1 = -2 * c, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * c, a2 = 1 - al / A;
 double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
 for (auto& v : x) { const double y = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0; x2 = x1; x1 = v; y2 = y1; y1 = y; v = static_cast<float>(y); }
}
static std::vector<float> clear(const std::vector<float>& in, cl::Values v, double fs) {
 cl::Engine e; e.prepare(fs); e.setValues(v);
 std::vector<float> x(in);
 x.resize(in.size() + static_cast<std::size_t>(e.latency()), 0.f);
 double mud = 0, harsh = 0; int frames = 0;
 for (std::size_t i = 0; i < x.size(); i += 512) {
  e.process(x.data() + i, nullptr, static_cast<int>(std::min<std::size_t>(512, x.size() - i)));
  mud += e.mudNow.load(); harsh += e.harshNow.load(); ++frames;
 }
 if (v[cl::P::Delta] < .5f) std::printf("  mean cut: mud %.1f dB, harshness %.1f dB\n", mud / frames, harsh / frames);
 x.erase(x.begin(), x.begin() + e.latency());
 return x;
}
static void save(const std::string& path, std::vector<float> x, float gain) {
 for (auto& v : x) v = std::clamp(v * gain, -1.f, 1.f);
 test::writeWav(path, test::Stereo{x, x}, 48000);
}
static float peakOf(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max(p, std::abs(v)); return p; }

int main(int argc, char** argv) {
 const std::string dir = argc > 1 ? argv[1] : ".";
 const double fs = 48000;
 // a harsh guitar: high gain, presence and treble up, the mic on the centre of the cone
 auto gv = fu::presetValues(8);
 gv[fu::P::Presence] = 9.5f; gv[fu::P::Treble] = 8.5f; gv[fu::P::Position] = 0; gv[fu::P::Cab] = 1;
 auto guitar = amp(furnace_di::riff(fs, 4), gv, fs);
 bell(guitar, 3400, 7, 6, fs); // and a ringing cabinet resonance
 // a muddy mix: clean guitar and bass, plus a low-mid build-up
 auto cleanV = fu::presetValues(1);
 auto bassV = fu::presetValues(12);
 auto g2 = amp(furnace_di::clean(fs, 5), cleanV, fs);
 auto b2 = amp(furnace_di::bassLine(fs, 6), bassV, fs);
 std::vector<float> mix(std::max(g2.size(), b2.size()), 0.f);
 for (std::size_t i = 0; i < g2.size(); ++i) mix[i] += g2[i];
 for (std::size_t i = 0; i < b2.size(); ++i) mix[i] += .9f * b2[i];
 bell(mix, 280, 8, .9, fs);
 struct Job { const char* name; std::vector<float>* in; int preset; } jobs[] = {{"harsh", &guitar, 1}, {"muddy", &mix, 2}};
 for (auto& j : jobs) {
  const float g = .7f / peakOf(*j.in);
  auto v = cl::presetValues(static_cast<std::size_t>(j.preset));
  const auto after = clear(*j.in, v, fs);
  v[cl::P::Delta] = 1;
  const auto delta = clear(*j.in, v, fs);
  save(dir + "/clear_" + j.name + "_before.wav", *j.in, g);
  save(dir + "/clear_" + j.name + "_after.wav", after, g);
  save(dir + "/clear_" + j.name + "_delta.wav", delta, g);
  std::printf("%s: rendered\n", j.name);
 }
}
