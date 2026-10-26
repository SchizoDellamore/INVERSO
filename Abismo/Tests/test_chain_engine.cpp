// Every strip and every preset: values inside their ranges, stable output, the master's ceiling holds,
// parameters a plugin does not have stay out of its signal path, the latency matches the limiter.
#include "../Source/DSP/ChainEngine.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace inverso_abismo::chain;

static void verify(bool ok, const std::string& message) {
 if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message.c_str()); std::exit(1); }
}

static void table() {
 std::set<std::string> ids;
 for (const auto& sp : allSpecs()) {
  verify(ids.insert(sp.id).second, std::string("duplicate id ") + sp.id);
  verify(sp.min < sp.max && sp.def >= sp.min && sp.def <= sp.max, std::string("default out of range: ") + sp.id);
  if (sp.centre > 0) verify(sp.centre > sp.min && sp.centre < sp.max, std::string("centre out of range: ") + sp.id);
 }
 for (int k = 0; k < numStrips; ++k) {
  const auto s = static_cast<Strip>(k);
  const auto& l = layout(s);
  verify(l.specs.size() == paramsOf(s).size(), "layout size");
  std::set<int> seen;
  for (int g : paramsOf(s)) verify(seen.insert(g).second, std::string(stripName(s)) + ": parameter listed twice");
  std::set<std::string> names;
  for (const auto& p : presets(s)) verify(names.insert(p.name).second, std::string(stripName(s)) + ": duplicate preset name " + p.name);
  for (std::size_t i = 0; i < presets(s).size(); ++i) {
   const auto v = presetValues(s, i);
   for (std::size_t g = 0; g < v.size(); ++g) {
    const auto& sp = allSpecs()[g];
    verify(v[g] >= sp.min - 1e-4f && v[g] <= sp.max + 1e-4f, std::string(stripName(s)) + "/" + presets(s)[i].name + ": " + sp.id + " out of range");
   }
   for (const auto& pv : presets(s)[i].values)
    verify(l.local[static_cast<std::size_t>(pv.param)] >= 0, std::string(stripName(s)) + "/" + presets(s)[i].name + ": sets " + allSpecs()[static_cast<std::size_t>(pv.param)].id + " which the plugin does not have");
  }
  std::printf("%-10s %2zu parameters, %2zu presets\n", stripName(s), l.specs.size(), presets(s).size());
 }
}

static void signalFor(std::vector<float>& l, std::vector<float>& r, int n, double sr) {
 std::uint32_t seed = 4242;
 l.assign(static_cast<std::size_t>(n), 0.f); r = l;
 double lp = 0, lp2 = 0;
 for (int i = 0; i < n; ++i) {
  seed = seed * 1664525u + 1013904223u; const double w = static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
  lp += .15 * (w - lp); lp2 += .03 * (lp - lp2);
  const double t = std::fmod(static_cast<double>(i) / sr, .5);
  const double hit = std::exp(-t / .08) * std::sin(2 * pi * (60 + 120 * std::exp(-t / .02)) * t);   // a kick every half second
  const double v = .5 * hit + .12 * lp * 2 + .2 * lp2 * 3;
  l[static_cast<std::size_t>(i)] = static_cast<float>(v);
  r[static_cast<std::size_t>(i)] = static_cast<float>(.9 * v + .05 * lp);
 }
}

// independent true-peak estimate (8x windowed sinc) on the right channel's samples
static double truePeak(const std::vector<float>& x) {
 const int K = 24, R = 8; double peak = 0;
 for (std::size_t n = K; n + K < x.size(); ++n) {
  peak = std::max(peak, static_cast<double>(std::abs(x[n])));
  for (int p = 1; p < R; ++p) {
   double a = 0;
   for (int k = -K + 1; k <= K; ++k) { const double t = k - static_cast<double>(p) / R; a += x[static_cast<std::size_t>(static_cast<long>(n) + k)] * (std::sin(pi * t) / (pi * t)) * (.5 + .5 * std::cos(pi * t / (K + .5))); }
   peak = std::max(peak, std::abs(a));
  }
 }
 return gainToDb(peak);
}

static void presetsRun() {
 const double sr = 48000.0; const int n = 48000 * 3;
 std::vector<float> sl, sr_; signalFor(sl, sr_, n, sr);
 for (int k = 0; k < numStrips; ++k) {
  const auto s = static_cast<Strip>(k);
  for (std::size_t p = 0; p < presets(s).size(); ++p) {
   Engine e(s);
   e.setValues(presetValues(s, p));
   e.prepare(sr);
   auto l = sl, r = sr_;
   // odd block sizes on purpose
   for (int pos = 0; pos < n;) { const int b = std::min(n - pos, 37 + (pos / 997) % 300); e.process(l.data() + pos, r.data() + pos, b); pos += b; }
   double top = 0;
   for (int i = 0; i < n; ++i) { verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::isfinite(r[static_cast<std::size_t>(i)]), std::string(stripName(s)) + "/" + presets(s)[p].name + ": not finite"); top = std::max(top, static_cast<double>(std::max(std::abs(l[static_cast<std::size_t>(i)]), std::abs(r[static_cast<std::size_t>(i)])))); }
   verify(top < 4.0, std::string(stripName(s)) + "/" + presets(s)[p].name + ": runaway level");
   if (s == Strip::Master) {
    const auto v = presetValues(s, p);
    if (v[P::LimOn] > .5f && v[P::OutGain] == 0.f) {
     std::vector<float> tail(l.begin() + 24000, l.end());
     const double tp = truePeak(tail), ceil = v[P::LimCeil];
     verify(tp < ceil + .15, std::string("MASTER/") + presets(s)[p].name + ": true peak over the ceiling");
    }
    std::printf("MASTER/%-28s short-term %6.1f LUFS, peak %6.1f dBFS, GR %.1f/%.1f/%.1f dB\n", presets(s)[p].name, e.shortTerm.load(), e.peakDb.load(), e.gr1.load(), e.gr2.load(), e.grLim.load());
   }
  }
 }
}

static void latencyAndBypass() {
 verify(layout(Strip::Master).specs[static_cast<std::size_t>(layout(Strip::Master).local[P::OutGain])].max <= 0.f, "the master's output trim must not raise the signal over the limiter's ceiling");
 verify(Engine(Strip::Master).latency() == 107, "master latency");
 verify(Engine(Strip::Kick).latency() == 0, "a strip without a limiter has no latency");
 // a strip with everything off passes the signal through untouched
 Engine e(Strip::Snare);
 auto v = presetValues(Strip::Snare, 0);
 v[P::EqOn] = 0; v[P::GateOn] = 0; v[P::TrOn] = 0; v[P::TameOn] = 0; v[P::C1On] = 0; v[P::SatOn] = 0;
 e.setValues(v); e.prepare(48000);
 std::vector<float> l, r; signalFor(l, r, 9600, 48000);
 const auto l0 = l, r0 = r;
 e.process(l.data(), r.data(), 9600);
 double worst = 0; for (std::size_t i = 0; i < l.size(); ++i) worst = std::max(worst, std::abs(static_cast<double>(l[i] - l0[i])));
 for (std::size_t i = 0; i < r.size(); ++i) worst = std::max(worst, std::abs(static_cast<double>(r[i] - r0[i])));
 std::printf("everything off: largest difference %.2e\n", worst);
 verify(worst < 1e-6, "everything off must be transparent");
 // mono in, mono out
 Engine m(Strip::Kick); m.prepare(48000);
 std::vector<float> a(4800, .1f); m.process(a.data(), nullptr, 4800);
 for (float x : a) verify(std::isfinite(x), "mono");
}

// The master's macros: each one does what it says, on top of the preset, and the ceiling still holds.
static void macros() {
 const double sr = 48000.0; const int n = 48000 * 4;
 std::vector<float> sl, sr_; signalFor(sl, sr_, n, sr);
 auto run = [&](int macro, float amount) {
  auto v = presetValues(Strip::Master, 0);
  if (macro >= 0) v[static_cast<std::size_t>(macro)] = amount;
  Engine e(Strip::Master); e.setValues(v); e.prepare(sr);
  auto l = sl, r = sr_;
  double maxGr = 0;
  for (int i = 0; i < n; i += 256) { e.process(l.data() + i, r.data() + i, std::min(256, n - i)); maxGr = std::max(maxGr, static_cast<double>(e.gr1.load() + e.gr2.load())); }
  struct R { double lufs, peak, gr, hi, lo; } out{e.shortTerm.load(), e.peakDb.load(), maxGr, 0, 0};
  Biquad hp, lp; hp.set(highpass(6000, .707, sr)); lp.set(lowpass(150, .707, sr));
  double a = 0, b = 0; for (int i = 24000; i < n; ++i) { const double x = hp.process(l[static_cast<std::size_t>(i)]), y = lp.process(l[static_cast<std::size_t>(i)]); a += x * x; b += y * y; }
  out.hi = 10 * std::log10(a / (n - 24000) + 1e-20); out.lo = 10 * std::log10(b / (n - 24000) + 1e-20);
  return out;
 };
 const auto base = run(-1, 0);
 const auto loud = run(P::MLoud, 100), glue = run(P::MGlue, 100), warm = run(P::MWarmth, 100), bright = run(P::MTone, 100), dark = run(P::MTone, -100);
 std::printf("macros: loudness +%.1f LUFS (peak %.1f), glue +%.1f dB reduction, tone high %+.1f / %+.1f dB, warmth low %+.1f dB\n",
             loud.lufs - base.lufs, loud.peak, glue.gr - base.gr, bright.hi - base.hi, dark.hi - base.hi, warm.lo - base.lo);
 verify(loud.lufs - base.lufs > 4.0 && loud.peak < -0.8, "LOUDNESS must raise the loudness and keep the ceiling");
 verify(glue.gr - base.gr > 2.0, "GLUE must compress more");
 verify(bright.hi - base.hi > 1.5 && dark.hi - base.hi < -1.5, "TONE must tilt the highs both ways");
 verify(warm.lo - base.lo > .3, "WARMTH must add weight");
}

int main() {
 table(); presetsRun(); latencyAndBypass(); macros();
 std::puts("chain engine: all good");
 return 0;
}
