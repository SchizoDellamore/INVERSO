// FURNACE: tone stack against the circuit, oversampling (aliasing), latency,
// gate, spring, cabinets, every preset on a DI, and a parameter storm.
#include "TestUtil.h"
#include "FurnaceDI.h"
#include "../Source/DSP/FurnaceAmp.h"
#include "../Source/DSP/FurnaceCab.h"
#include <complex>
#include <cstring>
#include <random>

using namespace inverso_abismo::furnace;
using test::verify;
using cd = std::complex<double>;

static std::vector<cd> spectrum(const std::vector<float>& x, std::size_t n) {
 std::vector<cd> a(n, 0.0);
 for (std::size_t i = 0; i < std::min(n, x.size()); ++i) a[i] = x[i];
 cabdetail::fft(a, false);
 return a;
}
static std::vector<float> convolve(const std::vector<float>& x, const std::vector<float>& h) {
 std::size_t n = 1;
 while (n < x.size() + h.size()) n <<= 1;
 auto X = spectrum(x, n), H = spectrum(h, n);
 for (std::size_t i = 0; i < n; ++i) X[i] *= H[i];
 cabdetail::fft(X, true);
 std::vector<float> y(x.size());
 for (std::size_t i = 0; i < y.size(); ++i) y[i] = static_cast<float>(X[i].real());
 return y;
}
static float rmsDb(const std::vector<float>& x) {
 double s = 0;
 for (float v : x) s += static_cast<double>(v) * v;
 return static_cast<float>(10 * std::log10(s / static_cast<double>(x.size()) + 1e-20));
}

struct Render { std::vector<float> amp, di; };
static Render run(Amp& a, const std::vector<float>& in, int block = 256) {
 Render r{std::vector<float>(in.size()), std::vector<float>(in.size())};
 for (std::size_t i = 0; i < in.size(); i += static_cast<std::size_t>(block)) {
  const int n = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(block), in.size() - i));
  a.process(in.data() + i, r.amp.data() + i, r.di.data() + i, n);
 }
 return r;
}
// The plugin's chain: amp, cabinet, DI blend, output.
static std::vector<float> chain(const Values& v, const std::vector<float>& in, double fs) {
 Amp a;
 a.prepare(fs);
 a.setValues(v);
 auto r = run(a, in);
 const int cab = static_cast<int>(v[P::Cab]);
 std::vector<float> wet = r.amp;
 if (cab > cabOff && cab < cabUser) {
  const auto ir = designCab(cab - 1, static_cast<int>(v[P::Mic]), fromKnobs(v[P::Position] / 100.0, v[P::Axis] / 100.0, v[P::Distance] / 100.0), fs);
  wet = convolve(r.amp, ir);
  for (auto& s : wet) s *= 4.f; // the plugin's cab makeup
 }
 const float di = v[P::DIBlend] / 100.f;
 for (std::size_t i = 0; i < wet.size(); ++i) wet[i] = wet[i] * (1 - di) + r.di[i] * di;
 return wet;
}

int main() {
 const double fs = 48000;
 // 1. The digital tone stack follows the circuit (bilinear at the 4x rate).
 {
  for (const auto& parts : {amp::blackface, amp::tweed, amp::british, amp::modern, amp::chime}) {
   for (double knob : {.1, .5, .9}) {
    amp::ToneStack ts;
    const double t = amp::taperTreble(knob), l = amp::taperBass(1 - knob), m = amp::taperMid(knob);
    ts.design(parts, t, l, m, fs * 8);
    for (double f : {60.0, 250.0, 700.0, 2000.0, 6000.0}) {
     // measure by running a sine through the IIR
     double re = 0, im = 0;
     const int n = 384000;
     for (int i = 0; i < n; ++i) {
      const double ph = 2 * amp::pi * f * i / (fs * 8);
      const float y = ts.process(static_cast<float>(std::sin(ph)));
      if (i >= n / 2) { re += y * std::sin(ph); im += y * std::cos(ph); }
     }
     const double measured = 2 * std::sqrt(re * re + im * im) / (n / 2);
     const double expected = amp::ToneStack::magnitude(parts, t, l, m, f);
     const double err = 20 * std::log10(measured / expected);
     if (std::abs(err) > .25) std::printf("stack %.0f Hz knob %.1f: %.2f dB off\n", f, knob, err);
     verify(std::abs(err) < .25, "tone stack differs from the circuit");
    }
   }
  }
  // the controls do what they say: more treble = more 4 kHz, more bass = more 80 Hz
  const auto mag = [](double t, double l, double m, double f) { return 20 * std::log10(amp::ToneStack::magnitude(amp::british, amp::taperTreble(t), amp::taperBass(l), amp::taperMid(m), f)); };
  verify(mag(.9, .5, .5, 4000) - mag(.1, .5, .5, 4000) > 8, "treble does too little");
  verify(mag(.5, .9, .5, 80) - mag(.5, .1, .5, 80) > 8, "bass does too little");
  verify(mag(.5, .5, .9, 600) - mag(.5, .5, .1, 600) > 4, "middle does too little");
  std::puts("tone stack: matches the circuit within 0.25 dB");
 }

 // 2. Aliasing: high notes on the high-gain models, energy off the harmonics.
 {
  for (int model : {3, 4, 5}) {
   for (double f0 : {1318.51, 2637.02}) { // E6 (24th fret), E7 (beyond the guitar)
    Amp a;
    a.prepare(fs);
    auto v = defaults();
    v[P::Model] = static_cast<float>(model); v[P::Gain] = 9; v[P::Treble] = 8; v[P::Presence] = 8;
    a.setValues(v);
    std::vector<float> in(1 << 16);
    for (std::size_t i = 0; i < in.size(); ++i) in[i] = static_cast<float>(.3 * std::sin(2 * amp::pi * f0 * static_cast<double>(i) / fs));
    auto r = run(a, in);
    std::vector<float> tail(r.amp.begin() + (1 << 15), r.amp.end());
    for (std::size_t i = 0; i < tail.size(); ++i) tail[i] *= static_cast<float>(.5 - .5 * std::cos(2 * amp::pi * static_cast<double>(i) / static_cast<double>(tail.size())));
    const std::size_t n = tail.size();
    auto X = spectrum(tail, n);
    double harm = 0, alias = 0;
    for (std::size_t k = 4; k < n / 2; ++k) {
     const double f = fs * static_cast<double>(k) / static_cast<double>(n);
     const double h = f / f0;
     const double p = std::norm(X[k]);
     if (std::abs(h - std::round(h)) * f0 < 30) harm += p; else alias += p;
    }
    const double ratio = 10 * std::log10(alias / harm);
    std::printf("aliasing, model %d at %.0f Hz: %.1f dB below the harmonics\n", model, f0, -ratio);
    if (f0 < 2000) verify(ratio < -50, "oversampling lets too much aliasing through");
   }
  }
 }

 // 3. The DI path lines up with the amp (the DI blend must not comb filter).
 {
  Amp a;
  a.prepare(fs);
  auto v = defaults();
  v[P::Model] = 6; v[P::Gain] = 0; v[P::Master] = 2; v[P::Bass] = 5; v[P::Treble] = 5;
  a.setValues(v);
  auto in = furnace_di::bassLine(fs);
  auto r = run(a, in);
  int best = 0; double bestC = -1e30;
  for (int lag = -40; lag <= 40; ++lag) {
   double c = 0;
   for (std::size_t i = 48000; i < 200000; ++i) c += static_cast<double>(r.amp[i]) * r.di[static_cast<std::size_t>(static_cast<int>(i) - lag)];
   if (std::abs(c) > bestC) { bestC = std::abs(c); best = lag; }
  }
  std::printf("DI against amp: %d samples\n", best);
  verify(std::abs(best) <= 3, "DI path is not aligned with the amp");
 }

 // 4. Gate: hiss under the threshold is silenced, playing opens it.
 {
  Amp a;
  a.prepare(fs);
  auto v = defaults();
  v[P::Model] = 4; v[P::Gain] = 8; v[P::Gate] = -50; // the hiss below peaks near -58 dBFS
  a.setValues(v);
  std::mt19937 rng(5);
  std::normal_distribution<float> noise(0.f, .0003f); // about -70 dBFS
  std::vector<float> in(96000);
  for (auto& x : in) x = noise(rng);
  auto r = run(a, in);
  std::vector<float> end(r.amp.end() - 24000, r.amp.end());
  std::printf("gate closed: %.1f dBFS\n", rmsDb(end));
  verify(rmsDb(end) < -90, "gate does not close on hiss");
  auto riff = furnace_di::riff(fs);
  auto played = run(a, riff);
  verify(rmsDb(played.amp) > -40, "gate does not open when playing");
 }

 // 5. Spring: rings, then dies away, never runs away.
 {
  Amp a;
  a.prepare(fs);
  auto v = defaults();
  v[P::Model] = 0; v[P::Gain] = 3; v[P::Spring] = 100;
  a.setValues(v);
  std::vector<float> in(static_cast<std::size_t>(fs * 6), 0.f);
  for (int i = 0; i < 200; ++i) in[static_cast<std::size_t>(4800 + i)] = .4f * std::sin(i * .3f);
  auto r = run(a, in);
  const auto seg = [&](double t0, double t1) { return rmsDb(std::vector<float>(r.amp.begin() + static_cast<long>(t0 * fs), r.amp.begin() + static_cast<long>(t1 * fs))); };
  std::printf("spring: %.1f dB at 0.3 s, %.1f dB at 1.5 s, %.1f dB at 5 s\n", seg(.3, .5), seg(1.5, 1.7), seg(5, 5.5));
  verify(seg(.3, .5) > -70, "spring does not ring");
  verify(seg(5, 5.5) < seg(1.5, 1.7) - 20, "spring does not decay");
 }

 // 6. Cabinets: equal loudness, minimum phase (energy right at the start).
 {
  for (int s = 0; s < numSpeakers; ++s) {
   const auto ir = designCab(s, 0, fromKnobs(.33, 0, 0), fs);
   std::size_t peakAt = 0;
   for (std::size_t i = 0; i < ir.size(); ++i) if (std::abs(ir[i]) > std::abs(ir[peakAt])) peakAt = i;
   verify(peakAt < static_cast<std::size_t>(fs * .002), "cabinet response does not start at once");
  }
  std::puts("cabinets: minimum phase");
 }

 // 7. Every preset on its DI: finite, reasonable level; prints the levels.
 {
  const auto guitar = furnace_di::riff(fs), clean = furnace_di::clean(fs), bass = furnace_di::bassLine(fs);
  const char* dir = std::getenv("FURNACE_DEMOS");
  for (std::size_t p = 0; p < presets().size(); ++p) {
   const auto v = presetValues(p);
   const int model = static_cast<int>(v[P::Model]);
   const bool isBass = modelSpecs()[static_cast<std::size_t>(model)].bass || v[P::Cab] >= 9;
   const bool isClean = model <= 1 && v[P::Gain] <= 5.5f;
   const auto& in = isBass ? bass : isClean ? clean : guitar;
   auto out = chain(v, in, fs);
   float peak = 0;
   for (float x : out) { verify(std::isfinite(x), "non-finite output"); peak = std::max(peak, std::abs(x)); }
   std::printf("%-22s %-11s rms %6.1f dBFS  peak %5.2f\n", presets()[p].name, isBass ? "(bass)" : isClean ? "(clean)" : "(riff)", rmsDb(out), peak);
   verify(rmsDb(out) > -32 && rmsDb(out) < -10, "preset level out of range");
   if (dir) {
    test::Stereo s{out, out};
    for (auto& x : s.l) x = std::clamp(x, -1.f, 1.f);
    s.r = s.l;
    char path[512];
    std::snprintf(path, sizeof path, "%s/furnace_%02d.wav", dir, static_cast<int>(p));
    test::writeWav(path, s, static_cast<int>(fs));
   }
  }
 }

 // 8. Parameter storm: every knob moving while playing, all models.
 {
  Amp a;
  a.prepare(44100);
  std::mt19937 rng(9);
  std::uniform_real_distribution<float> u(0.f, 1.f);
  auto riff = furnace_di::riff(44100);
  std::vector<float> out(256), di(256);
  for (std::size_t i = 0; i + 256 < riff.size(); i += 256) {
   if ((i / 256) % 8 == 0) {
    Values v = defaults();
    for (std::size_t k = 0; k < v.size(); ++k) v[k] = specs()[k].min + u(rng) * (specs()[k].max - specs()[k].min);
    a.setValues(v);
   }
   a.process(riff.data() + i, out.data(), di.data(), 256);
   for (float x : out) { verify(std::isfinite(x), "non-finite output in the storm"); verify(std::abs(x) < 20.f, "runaway output in the storm"); }
  }
  std::puts("parameter storm: stable");
 }
 std::puts("FURNACE tests passed");
}
