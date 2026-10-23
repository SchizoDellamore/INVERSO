// Randomised automation, MIDI and sample rates; plus a worst-case CPU estimate
// (16 voices x 7 unison x 3 cluster stacks + resonator + reverb).
#include "TestUtil.h"
#include <chrono>
using namespace test;

int main() {
 detail::Random rnd;
 for (int sr : {44100, 48000, 96000}) {
  Engine e; e.prepare(sr);
  std::vector<float> l(4096), r(4096);
  for (int blockIndex = 0; blockIndex < 600; ++blockIndex) {
   Values v = defaults();
   for (int p = 0; p < P::Count; ++p) {
    const auto& s = specs()[static_cast<std::size_t>(p)];
    const float u = .5f * (rnd.next() + 1.f);
    v[static_cast<std::size_t>(p)] = s.kind == Kind::Float ? s.min + u * (s.max - s.min) : std::round(s.min + u * (s.max - s.min));
   }
   v[P::Output] = std::min(v[P::Output], 6.f);
   e.setValues(v);
   if (blockIndex % 3 == 0) e.noteOn(24 + static_cast<int>((rnd.next() + 1.f) * 40.f), .5f * (rnd.next() + 1.f));
   if (blockIndex % 5 == 0) e.noteOff(24 + static_cast<int>((rnd.next() + 1.f) * 40.f));
   if (blockIndex % 7 == 0) e.setPitchBend(rnd.next());
   if (blockIndex % 11 == 0) e.setModWheel(.5f * (rnd.next() + 1.f));
   if (blockIndex % 13 == 0) e.setPressure(.5f * (rnd.next() + 1.f));
   if (blockIndex % 17 == 0) e.setSustain(rnd.next() > 0.f);
   if (blockIndex % 97 == 0) e.allNotesOff();
   const int n = 1 + static_cast<int>((rnd.next() + 1.f) * 1000.f);
   e.render(l.data(), r.data(), n);
   for (int i = 0; i < n; ++i) {
    verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::isfinite(r[static_cast<std::size_t>(i)]), "non-finite output");
    verify(std::abs(l[static_cast<std::size_t>(i)]) <= 1.f && std::abs(r[static_cast<std::size_t>(i)]) <= 1.f, "output above ceiling");
   }
  }
  std::printf("%d Hz randomised automation: pass\n", sr);
 }

 Values worst = defaults();
 worst[P::Unison] = 6; worst[P::ClusterMode] = 3; worst[P::ClusterLevel] = 100; worst[P::SubLevel] = 50;
 worst[P::NoiseLevel] = 30; worst[P::ResLevel] = 60; worst[P::Scrape] = 50; worst[P::Drive] = 40;
 worst[P::Sustain] = 100; worst[P::RevMix] = 50;
 Engine e; e.setValues(worst); e.prepare(48000);
 for (int n = 0; n < maxVoices; ++n) e.noteOn(36 + n * 2, .8f);
 std::vector<float> l(512), r(512);
 const int blocks = 48000 * 5 / 512;
 const auto t0 = std::chrono::steady_clock::now();
 for (int b = 0; b < blocks; ++b) e.render(l.data(), r.data(), 512);
 const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
 std::printf("worst case: %.1f%% of one core for realtime at 48 kHz (this machine, single instance)\n", 100.0 * seconds / 5.0);
 verify(e.activeVoices() == maxVoices, "all voices sounding in CPU test");
}
