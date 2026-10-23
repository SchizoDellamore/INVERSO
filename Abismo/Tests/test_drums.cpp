// ABISMO DRUMS engine: every object sounds and dies away, hits vary, stumble
// delays hits, the musical saw is in tune and glides, kits stay below 0 dBFS,
// randomised stress. With a directory argument it writes demo WAVs there.
#include "../Source/DSP/DrumsEngine.h"
#include "TestUtil.h"
using namespace test;
namespace D = inverso_abismo::drums;

static D::Values dry() {
 D::Values v = D::defaults();
 v[D::P::RoomMix] = 0; v[D::P::Stumble] = 0; v[D::P::Output] = 0;
 for (int p = 0; p < D::numPads; ++p) v[static_cast<std::size_t>(D::P::padBase(p) + D::P::Pan)] = 0;
 return v;
}
static Stereo play(D::Engine& e, int samples) {
 Stereo out{std::vector<float>(static_cast<std::size_t>(samples)), std::vector<float>(static_cast<std::size_t>(samples))};
 for (int i = 0; i < samples; i += 256) e.render(out.l.data() + i, out.r.data() + i, std::min(256, samples - i));
 return out;
}
static std::size_t onset(const std::vector<float>& x, float threshold = 1e-3f) {
 for (std::size_t i = 0; i < x.size(); ++i) if (std::abs(x[i]) > threshold) return i;
 return x.size();
}

static void objects() {
 const int sr = 48000;
 for (int obj = 0; obj < 16; ++obj) {
  D::Values v = dry();
  v[static_cast<std::size_t>(D::P::padBase(0) + D::P::Object)] = static_cast<float>(obj);
  v[static_cast<std::size_t>(D::P::padBase(0) + D::P::Variation)] = 0;
  D::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(D::firstPadNote, 1.f);
  auto hit = play(e, sr * 8);
  const float early = rms(hit.l, 0, static_cast<std::size_t>(sr / 5)), p = peak(hit);
  std::printf("%-24s peak %.3f  rms(200ms) %.4f\n", D::objects()[static_cast<std::size_t>(obj)].name, p, early);
  verify(finite(hit) && p <= 1.f, "object output invalid");
  verify(early > 3e-3f, "object must be audible");
  verify(e.activeHits() == 0, "hit must free itself after ringing out");
  verify(rms(hit.l, static_cast<std::size_t>(sr * 7), hit.l.size()) < 1e-5f, "object must decay to silence");
 }
}

static void variationAndStumble() {
 const int sr = 48000;
 D::Engine e; e.setValues(dry()); e.prepare(sr);
 e.noteOn(D::firstPadNote + 3, 1.f);
 auto a = play(e, sr / 2);
 e.noteOn(D::firstPadNote + 3, 1.f);
 auto b = play(e, sr / 2);
 double diff = 0;
 for (std::size_t i = 0; i < a.l.size(); ++i) diff += std::abs(a.l[i] - b.l[i]);
 verify(diff > 1.0, "variation must make consecutive hits differ");

 D::Values v = dry();
 v[static_cast<std::size_t>(D::P::padBase(3) + D::P::Variation)] = 0;
 D::Engine still; still.setValues(v); still.prepare(sr);
 still.noteOn(D::firstPadNote + 3, 1.f);
 verify(onset(play(still, 4800).l) < 5, "no stumble: hit starts immediately");

 v[D::P::Stumble] = 100;
 D::Engine drunk; drunk.setValues(v); drunk.prepare(sr);
 std::size_t late = 0, maxDelay = 0;
 for (int k = 0; k < 40; ++k) {
  drunk.noteOn(D::firstPadNote + 3, 1.f);
  const auto at = onset(play(drunk, 6000).l, 2e-3f);
  if (at > 48) ++late;
  maxDelay = std::max(maxDelay, at);
  drunk.allSoundOff();
 }
 std::printf("stumble: %zu of 40 hits late, max %.1f ms\n", late, maxDelay * 1000.0 / sr);
 verify(late >= 10 && maxDelay <= static_cast<std::size_t>(.081 * sr), "stumble must delay hits by up to 80 ms");
}

static void saw() {
 const int sr = 48000;
 D::Values v = dry();
 v[D::P::SawVibrato] = 0; v[D::P::SawWobble] = 0; v[D::P::SawTone] = 0; v[D::P::SawAttack] = 20; v[D::P::SawGlide] = 300;
 D::Engine e; e.setValues(v); e.prepare(sr);
 e.noteOn(69, 1.f);
 auto a = play(e, sr);
 const double f = frequency(a.l, sr, sr / 4, a.l.size());
 std::printf("saw A4 %.2f Hz\n", f);
 verify(std::abs(f - 440.0) < .5, "saw must be in tune");
 e.noteOn(81, 1.f); // legato: glide up an octave
 auto g = play(e, sr);
 const double mid = frequency(g.l, sr, static_cast<std::size_t>(sr * .1), static_cast<std::size_t>(sr * .2));
 const double end = frequency(g.l, sr, sr / 2, g.l.size());
 verify(mid > 470.0 && mid < 860.0 && std::abs(end - 880.0) < 1.0, "saw must glide between notes");
 e.noteOff(81); e.noteOff(69);
 play(e, sr * 3);
 verify(!e.sawActive(), "saw must stop after release");
 verify(e.activeHits() == 0, "saw notes must not trigger pads");
}

static void groove(D::Engine& e, Stereo& all, int sr, int bars, int stumbleSeed) {
 // 6/8 junkyard waltz at 92 BPM, eighth = 0.326 s: kick, rim, snare, chain, lid.
 const double eighth = 60.0 / 92.0 / 2.0;
 const int pattern[6][4] = {{36, 44, -1, -1}, {42, -1, -1, -1}, {38, 42, -1, -1}, {36, 44, -1, -1}, {37, 46, -1, -1}, {38, 48, -1, -1}};
 detail::Random r; r.state = static_cast<std::uint32_t>(stumbleSeed);
 for (int bar = 0; bar < bars; ++bar)
  for (int step = 0; step < 6; ++step) {
   for (int n : pattern[step]) if (n > 0) e.noteOn(n, .65f + .3f * (r.next() + 1.f) * .5f);
   if (bar % 2 == 1 && step == 5) e.noteOn(49, .8f);
   if (bar == bars - 1 && step == 0) e.noteOn(50, 1.f);
   auto s = play(e, static_cast<int>(eighth * sr));
   all.l.insert(all.l.end(), s.l.begin(), s.l.end());
   all.r.insert(all.r.end(), s.r.begin(), s.r.end());
  }
}

int main(int argc, char** argv) {
 objects();
 variationAndStumble();
 saw();
 const int sr = 48000;
 for (std::size_t k = 0; k < D::kits().size(); ++k) {
  D::Engine e; e.setValues(D::kitValues(k)); e.prepare(sr);
  e.setTransport(92.0, 0.0, true);
  Stereo all;
  groove(e, all, sr, 4, 11);
  // Saw melody over the last bars.
  e.noteOn(64, .8f);
  auto s1 = play(e, sr * 2); all.l.insert(all.l.end(), s1.l.begin(), s1.l.end()); all.r.insert(all.r.end(), s1.r.begin(), s1.r.end());
  e.noteOn(67, .8f); e.noteOff(64);
  auto s2 = play(e, sr * 2); all.l.insert(all.l.end(), s2.l.begin(), s2.l.end()); all.r.insert(all.r.end(), s2.r.begin(), s2.r.end());
  e.noteOff(67);
  auto tail = play(e, sr * 2); all.l.insert(all.l.end(), tail.l.begin(), tail.l.end()); all.r.insert(all.r.end(), tail.r.begin(), tail.r.end());
  const float p = peak(all), level = rms(all.l);
  std::printf("kit %-20s peak %.3f  rms %.4f\n", D::kits()[k].name, p, level);
  verify(finite(all) && p <= 1.f, "kit output invalid");
  verify(level > 5e-3f, "kit too quiet");
  if (argc > 1) {
   std::string name = std::string(argv[1]) + "/drums_" + std::to_string(k) + "_" + D::kits()[k].name + ".wav";
   for (auto& ch : name) if (ch == ' ') ch = '_';
   writeWav(name, all, sr);
  }
 }
 detail::Random rnd;
 for (int hostRate : {44100, 96000}) {
  D::Engine e; e.prepare(hostRate);
  std::vector<float> l(4096), r(4096);
  for (int b = 0; b < 600; ++b) {
   D::Values v = D::defaults();
   for (int p = 0; p < D::P::Count; ++p) {
    const auto& s = D::specs()[static_cast<std::size_t>(p)];
    const float u = .5f * (rnd.next() + 1.f);
    v[static_cast<std::size_t>(p)] = s.kind == Kind::Float ? s.min + u * (s.max - s.min) : std::round(s.min + u * (s.max - s.min));
   }
   e.setValues(v);
   for (int k = 0; k < 3; ++k) e.noteOn(36 + static_cast<int>((rnd.next() + 1.f) * 30.f), .5f * (rnd.next() + 1.f));
   if (b % 4 == 0) e.noteOff(60 + static_cast<int>((rnd.next() + 1.f) * 6.f));
   if (b % 9 == 0) e.setPitchBend(rnd.next());
   if (b % 13 == 0) e.setSustain(rnd.next() > 0.f);
   const int n = 1 + static_cast<int>((rnd.next() + 1.f) * 1000.f);
   e.render(l.data(), r.data(), n);
   for (int i = 0; i < n; ++i)
    verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::abs(l[static_cast<std::size_t>(i)]) <= 1.f &&
           std::isfinite(r[static_cast<std::size_t>(i)]) && std::abs(r[static_cast<std::size_t>(i)]) <= 1.f, "stress output invalid");
  }
  std::printf("%d Hz randomised drums automation: pass\n", hostRate);
 }
 std::puts("Abismo drums tests passed");
}
