// INVERSO CAPTURE: the ring keeps what passed, the player and the render agree, edits do what they say.
#include "../Source/DSP/CaptureDsp.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace inverso_abismo::capture;
static void verify(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); } }
static const double SR = 48000.0;

static Clip ramp(std::size_t n) { Clip c; c.sampleRate = SR; c.l.resize(n); c.r.resize(n); for (std::size_t i = 0; i < n; ++i) { c.l[i] = static_cast<float>(i) / static_cast<float>(n); c.r[i] = -c.l[i]; } return c; }
static Clip sine(double f, double seconds, double amp = .5) { Clip c; c.sampleRate = SR; const auto n = static_cast<std::size_t>(seconds * SR); c.l.resize(n); c.r.resize(n); for (std::size_t i = 0; i < n; ++i) c.l[i] = c.r[i] = static_cast<float>(amp * std::sin(2 * M_PI * f * static_cast<double>(i) / SR)); return c; }

static void ringTest() {
 Ring ring; ring.prepare(SR);
 std::vector<float> a(1000), b(1000);
 for (int round = 0; round < 3; ++round) { for (int i = 0; i < 1000; ++i) { a[static_cast<std::size_t>(i)] = static_cast<float>(round * 1000 + i); b[static_cast<std::size_t>(i)] = -a[static_cast<std::size_t>(i)]; } ring.write(a.data(), b.data(), 1000); }
 const auto c = ring.last(1500);
 verify(c.frames() == 1500 && c.l.front() == 1500.f && c.l.back() == 2999.f && c.r.back() == -2999.f, "ring: the last frames, in order");
 verify(ring.last(99999).frames() == 3000, "ring: asking for more than was written gives what there is");
 // mono input
 Ring m; m.prepare(SR); m.write(a.data(), nullptr, 1000);
 verify(m.last(10).r[3] == m.last(10).l[3], "ring: mono is copied to both sides");
 // wrap: write more than the ring holds; the last seconds stay right
 Ring w; w.prepare(8000.0);
 const std::size_t total = static_cast<std::size_t>(8000.0 * 130.0);
 std::vector<float> chunk(5000);
 std::uint64_t counter = 0;
 for (std::size_t done = 0; done < total; done += chunk.size()) { for (auto& v : chunk) v = static_cast<float>(counter++ % 100000); w.write(chunk.data(), chunk.data(), static_cast<int>(chunk.size())); }
 const auto cw = w.last(static_cast<std::size_t>(8000.0 * 10.0));
 verify(cw.frames() == 80000, "ring: ten seconds after wrapping twice");
 bool ok = true; for (std::size_t i = 1; i < cw.frames(); ++i) { const float d = cw.l[i] - cw.l[i - 1]; if (d != 1.f && d != -99999.f) ok = false; }
 verify(ok, "ring: continuous after wrapping");
 // the longest clip is capped
 verify(w.last(static_cast<std::size_t>(8000.0 * 500.0)).frames() == static_cast<std::size_t>(8000.0 * maxClipSeconds), "ring: a clip is at most 60 seconds");
 // a reader running while the writer writes sees only whole, ordered data
 Ring t; t.prepare(SR);
 std::atomic<bool> stop{false};
 std::thread writer([&] { std::vector<float> x(256); std::uint64_t k = 0; while (!stop.load()) { for (auto& v : x) v = static_cast<float>(k++ % 1000000); t.write(x.data(), x.data(), 256); } });
 bool ordered = true;
 for (int i = 0; i < 200; ++i) { const auto cc = t.last(static_cast<std::size_t>(SR * 5)); for (std::size_t j = 1; j < cc.frames(); ++j) { const float d = cc.l[j] - cc.l[j - 1]; if (d != 1.f && d != -999999.f) { ordered = false; break; } } }
 stop = true; writer.join();
 verify(ordered, "ring: reading while writing gives ordered data");
 std::puts("ring: ok");
}

static void renderTest() {
 // the region and the length
 { Clip c = ramp(48000); Edit e; e.start = .25; e.end = .75; e.fadeInMs = 0; e.fadeOutMs = 0; const auto o = renderEdit(c, e);
   verify(std::abs(static_cast<double>(o.frames()) - 24000.0) < 3, "render: the region's length");
   verify(std::abs(o.l[100] - (.25f + 100.f / 48000.f)) < 1e-3f, "render: the region starts where it says"); }
 // reverse
 { Clip c = ramp(48000); Edit e; e.reverse = true; e.fadeInMs = 0; e.fadeOutMs = 0; const auto o = renderEdit(c, e);
   verify(o.l[100] > .99f && o.l[o.frames() - 100] < .01f && o.l[2000] > o.l[3000], "render: reverse plays backwards"); }
 // pitch: +12 st halves the length, -12 doubles it
 { Clip c = sine(440, 1.0); Edit e; e.pitchSt = 12; const auto up = renderEdit(c, e); e.pitchSt = -12; const auto down = renderEdit(c, e);
   verify(std::abs(static_cast<double>(up.frames()) - 24000.0) < 4 && std::abs(static_cast<double>(down.frames()) - 96000.0) < 4, "render: octave up halves, octave down doubles the length");
   // the pitch really moves: count zero crossings
   auto crossings = [](const Clip& x) { int n = 0; for (std::size_t i = 1; i < x.frames(); ++i) if ((x.l[i - 1] < 0) != (x.l[i] < 0)) ++n; return n; };
   verify(std::abs(crossings(up) - 880) < 6, "render: +12 st doubles the frequency"); }
 // fades and gain
 { Clip c = sine(220, 1.0, .5); Edit e; e.fadeInMs = 100; e.fadeOutMs = 100; e.gainDb = 6; const auto o = renderEdit(c, e);
   auto peakIn = [&](std::size_t a, std::size_t b) { double p = 0; for (std::size_t i = a; i < b; ++i) p = std::max(p, static_cast<double>(std::abs(o.l[i]))); return p; };
   verify(peakIn(0, 200) < .05, "render: fade in starts from silence");
   verify(peakIn(o.frames() - 200, o.frames()) < .05, "render: fade out ends in silence");
   verify(std::abs(peakIn(20000, 30000) - .5 * 1.995) < .02, "render: +6 dB doubles the level"); }
 // no edit is a copy
 { Clip c = sine(300, .5); Edit e; e.fadeInMs = 0; e.fadeOutMs = 0; const auto o = renderEdit(c, e);
   double worst = 0; for (std::size_t i = 4; i + 4 < o.frames() && i < c.frames(); ++i) worst = std::max(worst, static_cast<double>(std::abs(o.l[i] - c.l[i])));
   verify(o.frames() >= c.frames() - 2 && worst < 1e-5, "render: no edit gives the clip back"); }
 // normalize
 { Clip c = sine(300, .2, .1); normalize(c, -1.0); verify(std::abs(peakOf(c) - std::pow(10.0, -1.0 / 20.0)) < 1e-3, "normalize: the peak lands on the target"); Clip z; z.l.assign(100, 0.f); z.r = z.l; normalize(z, -1.0); verify(peakOf(z) == 0.0, "normalize: silence stays silence"); }
 // odd regions do not crash or leave the clip
 { Clip c = sine(300, .3); Edit e; e.start = .9; e.end = .1; auto o = renderEdit(c, e); (void)o; e.start = -1; e.end = 5; o = renderEdit(c, e); verify(o.frames() > 0, "render: out-of-range region is clamped"); Clip empty; verify(renderEdit(empty, Edit{}).empty(), "render: an empty clip stays empty"); }
 std::puts("render: ok");
}

static void playerTest() {
 // live playback at another host rate: the same sound, the length scales with the rates
 Clip c = sine(440, 1.0); Edit e; e.fadeInMs = 0; e.fadeOutMs = 0;
 for (double host : {44100.0, 48000.0, 96000.0}) {
  Player p; p.play(&c, e, host);
  std::vector<float> l(static_cast<std::size_t>(host * 1.2)), r = l; bool ended = false; int n = 0;
  for (std::size_t i = 0; i < l.size() && !ended; i += 512) { const int b = static_cast<int>(std::min<std::size_t>(512, l.size() - i)); ended = p.render(l.data() + i, r.data() + i, b); n = static_cast<int>(i) + b; }
  verify(ended && std::abs(static_cast<double>(n) - host) < 1030, "player: one pass lasts the clip's time at any host rate");
 }
 // a loop is seamless: no frame is skipped at the wrap
 { Clip dc; dc.sampleRate = SR; dc.l.assign(1000, .5f); dc.r = dc.l; Edit le; le.loop = true; le.fadeInMs = 0; le.fadeOutMs = 0; Player p; p.play(&dc, le, SR);
   std::vector<float> l(5000), r(5000); p.render(l.data(), r.data(), 5000); int holes = 0; for (int i = 0; i < 5000; ++i) if (std::abs(l[static_cast<std::size_t>(i)] - .5f) > 1e-4f) ++holes;
   verify(holes == 0, "player: a loop leaves no empty frame at the wrap"); }
 // looping does not end; stop fades out without a click
 { Player p; e.loop = true; p.play(&c, e, SR); std::vector<float> l(48000 * 3), r = l; verify(!p.render(l.data(), r.data(), static_cast<int>(l.size())), "player: a loop does not end");
   p.stop(); std::vector<float> a(512), b(512); p.render(a.data(), b.data(), 512); double jump = 0; for (int i = 1; i < 512; ++i) jump = std::max(jump, static_cast<double>(std::abs(a[static_cast<std::size_t>(i)] - a[static_cast<std::size_t>(i) - 1])));
   verify(!p.active() && jump < .05, "player: stop fades out smoothly"); }
 // the live sound is the rendered sound
 { Clip x = sine(330, .5); Edit f; f.pitchSt = 3.f; f.reverse = true; f.gainDb = -4.f; const auto o = renderEdit(x, f);
   Player p; p.play(&x, f, SR); std::vector<float> l(o.frames()), r = l; p.render(l.data(), r.data(), static_cast<int>(l.size()));
   double worst = 0; for (std::size_t i = 0; i < l.size(); ++i) worst = std::max(worst, static_cast<double>(std::abs(l[i] - o.l[i])));
   verify(worst < 1e-6, "player: the audition is the same sound as the bake"); }
 // no clip: nothing, no crash
 { Player p; std::vector<float> l(64), r = l; verify(!p.render(l.data(), r.data(), 64) && !p.active(), "player: nothing to play"); p.play(nullptr, e, SR); }
 std::puts("player: ok");
}

int main() { ringTest(); renderTest(); playerTest(); std::puts("capture dsp: all good"); return 0; }
