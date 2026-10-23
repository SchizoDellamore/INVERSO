// INVERSO BERIMBASS engine: tuning of every technique across the bass range,
// decay to silence, mono glide, gourd wah on the mod wheel, stone pressure
// raising the pitch, presets below 0 dBFS, randomised stress.
// With a directory argument it writes demo WAVs there.
#include "../Source/DSP/BerimbassEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace B = inverso_berimbass;
namespace detail = inverso_abismo::detail;

static void verify(bool ok, const char* message) {
 if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); }
}
static std::vector<float> play(B::Engine& e, int samples, std::vector<float>* right = nullptr) {
 std::vector<float> l(static_cast<std::size_t>(samples)), r(static_cast<std::size_t>(samples));
 for (int i = 0; i < samples; i += 256) e.render(l.data() + i, r.data() + i, std::min(256, samples - i));
 if (right) *right = r;
 return l;
}
static float rms(const std::vector<float>& x, std::size_t a = 0, std::size_t b = 0) {
 if (b == 0 || b > x.size()) b = x.size();
 double s = 0; for (std::size_t i = a; i < b; ++i) s += static_cast<double>(x[i]) * x[i];
 return b > a ? static_cast<float>(std::sqrt(s / static_cast<double>(b - a))) : 0.f;
}
// Fundamental by normalised autocorrelation with parabolic peak refinement.
static double pitch(const std::vector<float>& x, double sr, std::size_t from, std::size_t len, double lo = 25.0, double hi = 1000.0) {
 const int minLag = static_cast<int>(sr / hi), maxLag = static_cast<int>(sr / lo);
 std::vector<double> c(static_cast<std::size_t>(maxLag + 2), 0.0);
 for (int lag = minLag; lag <= maxLag + 1; ++lag) {
  double num = 0, e1 = 0, e2 = 0;
  for (std::size_t i = from; i < from + len; ++i) {
   const double a = x[i], b = x[i + static_cast<std::size_t>(lag)];
   num += a * b; e1 += a * a; e2 += b * b;
  }
  c[static_cast<std::size_t>(lag)] = num / std::sqrt(e1 * e2 + 1e-20);
 }
 // First lag that reaches 90% of the global maximum: avoids octave errors.
 double best = 0; for (int lag = minLag; lag <= maxLag; ++lag) best = std::max(best, c[static_cast<std::size_t>(lag)]);
 int lag = minLag;
 for (int k = minLag + 1; k < maxLag; ++k) {
  const auto u = static_cast<std::size_t>(k);
  if (c[u] >= .9 * best && c[u] >= c[u - 1] && c[u] >= c[u + 1]) { lag = k; break; }
 }
 const auto u = static_cast<std::size_t>(lag);
 const double a = c[u - 1], b = c[u], d = c[u + 1];
 const double shift = (a - d) / (2.0 * (a - 2.0 * b + d));
 return sr / (lag + (std::isfinite(shift) ? shift : 0.0));
}
static double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }

static B::Values plain(int technique, int body) {
 B::Values v = B::defaults();
 v[B::P::Technique] = static_cast<float>(technique); v[B::P::Body] = static_cast<float>(body);
 v[B::P::Stiffness] = 0; v[B::P::Buzz] = 0; v[B::P::RevMix] = 0; v[B::P::Drive] = 0; v[B::P::StoneBend] = 1;
 v[B::P::Decay] = 6; v[B::P::GourdRate] = 0;
 return v;
}

static void tuning() {
 const char* names[4] = {"pizzicato", "bambu", "arco", "pedra"};
 for (int sr : {44100, 48000, 96000}) {
  for (int tech = 0; tech < 4; ++tech) {
   for (int note : {28, 40, 52}) { // E1, E2, E3
    B::Engine e; e.setValues(plain(tech, 3)); e.prepare(sr);
    e.noteOn(note, .9f);
    const auto x = play(e, sr);
    const double ref = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    const double f = pitch(x, sr, static_cast<std::size_t>(sr * .3), static_cast<std::size_t>(sr * .4));
    const double err = cents(f, ref);
    if (sr == 48000) std::printf("%-9s note %d: %.2f Hz (%+.1f cents)\n", names[tech], note, f, err);
    verify(std::abs(err) < 6.0, "string out of tune");
   }
  }
 }
}

static void decayAndBodies() {
 const int sr = 48000;
 for (int body = 0; body < 4; ++body) {
  for (int tech = 0; tech < 4; ++tech) {
   B::Values v = plain(tech, body); v[B::P::Decay] = 1; v[B::P::Release] = 200; v[B::P::Buzz] = 40;
   B::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(36, 1.f);
   const auto on = play(e, sr / 2);
   e.noteOff(36);
   const auto off = play(e, sr * 3);
   float p = 0; for (float s : on) p = std::max(p, std::abs(s));
   verify(rms(on) > 5e-3f, "note must be audible");
   verify(p <= 1.f, "output above 0 dBFS");
   verify(e.activeVoices() == 0, "voice must free itself after release");
   verify(rms(off, sr * 2) < 1e-4f, "string must fall silent after release");
  }
 }
 // Washtub with stiffness: still near pitch (inharmonic but recognisable).
 B::Values v = plain(0, 2); v[B::P::Stiffness] = 60;
 B::Engine e; e.setValues(v); e.prepare(sr);
 e.noteOn(33, 1.f);
 const auto x = play(e, sr);
 verify(std::abs(cents(pitch(x, sr, sr / 4, sr / 3), 55.0)) < 40.0, "stiff string drifted too far");
}

static void expression() {
 const int sr = 48000;
 // Mono legato glide: second key held while first still down slides, without re-plucking.
 B::Values v = plain(0, 3); v[B::P::Glide] = 300; v[B::P::VoiceMode] = 1;
 B::Engine e; e.setValues(v); e.prepare(sr);
 e.noteOn(40, 1.f); play(e, sr / 2);
 e.noteOn(47, 1.f);
 const auto g = play(e, sr);
 const double mid = pitch(g, sr, static_cast<std::size_t>(sr * .1), static_cast<std::size_t>(sr * .06));
 const double end = pitch(g, sr, static_cast<std::size_t>(sr * .6), static_cast<std::size_t>(sr * .3));
 std::printf("glide E2->B2: mid %.1f Hz, end %.1f Hz\n", mid, end);
 verify(mid > 85.0 && mid < 120.0 && std::abs(cents(end, 123.47)) < 8.0, "mono glide failed");
 verify(e.activeVoices() == 1, "mono mode must use one voice");

 // Aftertouch presses the stone: pitch rises by STONE_BEND.
 v = plain(1, 3); v[B::P::StoneBend] = 2;
 B::Engine s; s.setValues(v); s.prepare(sr);
 s.noteOn(40, 1.f); play(s, sr / 4);
 s.setPressure(1.f);
 const auto pressed = play(s, sr / 2);
 const double fp = pitch(pressed, sr, sr / 8, sr / 4);
 std::printf("stone pressed: %.2f Hz (%+.1f cents)\n", fp, cents(fp, 82.41));
 verify(std::abs(cents(fp, 82.41 * std::pow(2.0, 2.0 / 12.0))) < 10.0, "stone pressure must raise pitch by STONE_BEND");

 // Gourd: opening it (mod wheel) moves energy up into the wah band.
 auto bandEnergy = [&](float wheel) {
  B::Values gv = plain(1, 0); gv[B::P::Gourd] = 0;
  B::Engine ge; ge.setValues(gv); ge.prepare(sr);
  ge.setModWheel(wheel);
  ge.noteOn(36, 1.f);
  const auto x = play(ge, sr / 2);
  // crude high-band energy: first difference emphasises upper partials
  double hiE = 0, all = 0;
  for (std::size_t i = 1; i < x.size(); ++i) { const double d = x[i] - x[i - 1]; hiE += d * d; all += static_cast<double>(x[i]) * x[i]; }
  return hiE / (all + 1e-12);
 };
 const double closed = bandEnergy(0.f), open = bandEnergy(1.f);
 std::printf("gourd closed %.5f, open %.5f\n", closed, open);
 verify(open > closed * 3.0, "opening the gourd must brighten the sound");

 // Poly mode holds a chord.
 v = plain(0, 3); v[B::P::VoiceMode] = 0;
 B::Engine p; p.setValues(v); p.prepare(sr);
 p.noteOn(40, 1.f); p.noteOn(47, 1.f); p.noteOn(52, 1.f);
 play(p, 1024);
 verify(p.activeVoices() == 3, "poly mode must hold three notes");
}

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
 std::ofstream f(path, std::ios::binary);
 auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
 auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
 const auto frames = static_cast<std::uint32_t>(l.size());
 f.write("RIFF", 4); u32(36 + frames * 4); f.write("WAVEfmt ", 8);
 u32(16); u16(1); u16(2); u32(static_cast<std::uint32_t>(sr)); u32(static_cast<std::uint32_t>(sr) * 4); u16(4); u16(16);
 f.write("data", 4); u32(frames * 4);
 for (std::uint32_t i = 0; i < frames; ++i)
  for (float x : {l[i], r[i]}) {
   const auto v = static_cast<std::int16_t>(std::lround(std::clamp(x, -1.f, 1.f) * 32767.f));
   f.write(reinterpret_cast<const char*>(&v), 2);
  }
}

int main(int argc, char** argv) {
 tuning();
 decayAndBodies();
 expression();
 const int sr = 48000;
 // A capoeira-ish bass line for each preset: toque de angola feel at 80 BPM.
 const double eighth = 60.0 / 80.0 / 2.0;
 const int line[16] = {28, -1, 28, 31, -1, 33, 28, -1, 35, -1, 33, 31, 28, -1, 26, 28};
 for (std::size_t k = 0; k < B::presets().size(); ++k) {
  B::Engine e; e.setValues(B::presetValues(k)); e.prepare(sr);
  std::vector<float> L, R;
  float wheel = 0.f;
  for (int bar = 0; bar < 2; ++bar)
   for (int step = 0; step < 16; ++step) {
    const int n = line[step];
    if (n > 0) { e.noteOn(n + 12, .7f + .25f * (step % 4 == 0)); }
    if (k == 0 || k == 1) { wheel = .5f + .5f * std::sin(static_cast<float>(bar * 16 + step) * .8f); e.setModWheel(wheel); }
    std::vector<float> r;
    const auto l = play(e, static_cast<int>(eighth * sr * .8), &r);
    L.insert(L.end(), l.begin(), l.end()); R.insert(R.end(), r.begin(), r.end());
    if (n > 0) e.noteOff(n + 12);
    std::vector<float> r2;
    const auto l2 = play(e, static_cast<int>(eighth * sr * .2), &r2);
    L.insert(L.end(), l2.begin(), l2.end()); R.insert(R.end(), r2.begin(), r2.end());
   }
  // Final long note to hear the body and the tail.
  e.noteOn(40, .9f);
  std::vector<float> r3; auto l3 = play(e, sr * 2, &r3);
  L.insert(L.end(), l3.begin(), l3.end()); R.insert(R.end(), r3.begin(), r3.end());
  e.noteOff(40);
  l3 = play(e, sr, &r3);
  L.insert(L.end(), l3.begin(), l3.end()); R.insert(R.end(), r3.begin(), r3.end());
  float p = 0; bool finite = true;
  for (std::size_t i = 0; i < L.size(); ++i) { p = std::max({p, std::abs(L[i]), std::abs(R[i])}); finite = finite && std::isfinite(L[i]) && std::isfinite(R[i]); }
  std::printf("%-20s peak %.3f  rms %.4f\n", B::presets()[k].name, p, rms(L));
  verify(finite && p <= 1.f, "preset output invalid");
  verify(rms(L) > 5e-3f, "preset too quiet");
  if (argc > 1) {
   std::string name = std::string(argv[1]) + "/berimbass_" + std::to_string(k) + "_" + B::presets()[k].name + ".wav";
   for (auto& ch : name) if (ch == ' ') ch = '_';
   writeWav(name, L, R, sr);
  }
 }
 detail::Random rnd;
 for (int hostRate : {44100, 96000}) {
  B::Engine e; e.prepare(hostRate);
  std::vector<float> l(4096), r(4096);
  for (int b = 0; b < 800; ++b) {
   B::Values v = B::defaults();
   for (int p = 0; p < B::P::Count; ++p) {
    const auto& s = B::specs()[static_cast<std::size_t>(p)];
    const float u = .5f * (rnd.next() + 1.f);
    v[static_cast<std::size_t>(p)] = s.kind == inverso_abismo::Kind::Float ? s.min + u * (s.max - s.min) : std::round(s.min + u * (s.max - s.min));
   }
   e.setValues(v);
   if (b % 3 == 0) e.noteOn(20 + static_cast<int>((rnd.next() + 1.f) * 30.f), .5f * (rnd.next() + 1.f));
   if (b % 4 == 0) e.noteOff(20 + static_cast<int>((rnd.next() + 1.f) * 30.f));
   if (b % 7 == 0) e.setPitchBend(rnd.next());
   if (b % 5 == 0) e.setModWheel(.5f * (rnd.next() + 1.f));
   if (b % 6 == 0) e.setPressure(.5f * (rnd.next() + 1.f));
   if (b % 11 == 0) e.setSustain(rnd.next() > 0.f);
   if (b % 97 == 0) e.allNotesOff();
   const int n = 1 + static_cast<int>((rnd.next() + 1.f) * 1000.f);
   e.render(l.data(), r.data(), n);
   for (int i = 0; i < n; ++i)
    verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::abs(l[static_cast<std::size_t>(i)]) <= 1.f, "stress output invalid");
  }
  std::printf("%d Hz randomised berimbass automation: pass\n", hostRate);
 }
 std::puts("Berimbass tests passed");
}
