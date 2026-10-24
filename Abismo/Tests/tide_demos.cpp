// Renders TIDE's demos, each before and after.  tide_demos <folder>
#include "TestUtil.h"
#include "FurnaceDI.h"
#include "../Source/DSP/TideEngine.h"
#include "../Source/DSP/FurnaceAmp.h"
#include "../Source/DSP/FurnaceCab.h"
#include <complex>

namespace fu = inverso_abismo::furnace;
namespace td = inverso_abismo::tide;

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

static std::vector<float> tide(const std::vector<float>& in, td::Values v, double fs, const char* label) {
 td::Engine e; e.setValues(v); e.prepare(fs);
 std::vector<float> x(in);
 std::array<double, td::numBands> sum{}, most{};
 int frames = 0;
 for (std::size_t i = 0; i < x.size(); i += 256) {
  e.process(x.data() + i, nullptr, static_cast<int>(std::min<std::size_t>(256, x.size() - i)));
  for (int k = 0; k < td::numBands; ++k) { const double d = e.dynamicNow[static_cast<std::size_t>(k)].load(); sum[static_cast<std::size_t>(k)] += d; if (std::abs(d) > std::abs(most[static_cast<std::size_t>(k)])) most[static_cast<std::size_t>(k)] = d; }
  ++frames;
 }
 std::printf("%s:", label);
 for (int k = 0; k < td::numBands; ++k) std::printf("  b%d mean %+.1f max %+.1f", k + 1, sum[static_cast<std::size_t>(k)] / frames, most[static_cast<std::size_t>(k)]);
 std::printf("\n");
 return x;
}
static void save(const std::string& path, std::vector<float> x, float gain) {
 for (auto& v : x) v = std::clamp(v * gain, -1.f, 1.f);
 test::writeWav(path, test::Stereo{x, x}, 48000);
}
static float peakOf(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max(p, std::abs(v)); return p; }
static void normalise(std::vector<float>& x, float peak) { const float g = peak / peakOf(x); for (auto& v : x) v *= g; }

int main(int argc, char** argv) {
 const std::string dir = argc > 1 ? argv[1] : ".";
 const double fs = 48000;
 using namespace td::P;
 // 1. a harsh, ringing distorted guitar: the bells only dip when the ring flares
 auto gv = fu::presetValues(8);
 gv[fu::P::Presence] = 9.5f; gv[fu::P::Treble] = 8.5f; gv[fu::P::Position] = 0; gv[fu::P::Cab] = 1;
 auto guitar = amp(furnace_di::riff(fs, 4), gv, fs);
 bell(guitar, 3400, 7, 6, fs);
 normalise(guitar, .5f);
 auto hv = td::presetValues(2);
 hv[band(2, Freq)] = 3400; hv[band(2, Q)] = 4;
 // 2. a bass in a room that booms on one note (G, 98 Hz): only that note is pulled back
 auto bass = amp(furnace_di::bassLine(fs, 6), fu::presetValues(12), fs);
 bell(bass, 98, 9, 5, fs);
 normalise(bass, .5f);
 auto bv = td::presetValues(4);
 bv[band(1, Freq)] = 98; bv[band(1, Q)] = 4; bv[band(1, Thresh)] = -20; bv[band(1, Range)] = -8;
 // 3. a clean guitar whose pick only shines when it digs in
 auto clean = amp(furnace_di::clean(fs, 5), fu::presetValues(1), fs);
 normalise(clean, .5f);
 auto pv = td::presetValues(3);
 struct Job { const char* name; std::vector<float>* in; td::Values v; } jobs[] = {{"harsh_guitar", &guitar, hv}, {"bass_evener", &bass, bv}, {"pick_attack", &clean, pv}};
 for (auto& j : jobs) {
  const auto after = tide(*j.in, j.v, fs, j.name);
  save(dir + "/tide_" + j.name + "_before.wav", *j.in, 1.f);
  save(dir + "/tide_" + j.name + "_after.wav", after, 1.f);
 }
}
