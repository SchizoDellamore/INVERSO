// INVERSO CAPTURE's editing, measured: stretch keeps the pitch and the level, pitch keeps the length, the
// effects do what their names say, the pipeline cancels and never produces garbage.
#include "../Source/DSP/CaptureFx.h"
#include <cstdio>
#include <cstdlib>

using namespace inverso_abismo::capture;
static void verify(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); } }
static const double SR = 48000.0;

static Clip sine(double f, double seconds, double amp = .5) { Clip c; c.sampleRate = SR; const auto n = static_cast<std::size_t>(seconds * SR); c.l.resize(n); c.r.resize(n); for (std::size_t i = 0; i < n; ++i) c.l[i] = c.r[i] = static_cast<float>(amp * std::sin(2 * M_PI * f * static_cast<double>(i) / SR)); return c; }
// a "musical" test sound: a chord with a slow tremolo, so stretching has something to get wrong
static Clip chord(double seconds) { Clip c = sine(220, seconds, .2); const Clip b = sine(277.2, seconds, .15), d = sine(330, seconds, .15); for (std::size_t i = 0; i < c.frames(); ++i) { const float v = c.l[i] + b.l[i] + d.l[i]; c.l[i] = c.r[i] = v; } return c; }
static Clip clicks(double seconds) { Clip c; c.sampleRate = SR; const auto n = static_cast<std::size_t>(seconds * SR); c.l.assign(n, 0.f); c.r = c.l; for (double t = .1; t < seconds - .2; t += .5) { const auto at = static_cast<std::size_t>(t * SR); for (std::size_t k = 0; k < 4000 && at + k < n; ++k) c.l[at + k] = c.r[at + k] = static_cast<float>(.8 * std::exp(-static_cast<double>(k) / 600.0) * std::sin(2 * M_PI * 180.0 * static_cast<double>(k) / SR)); } return c; }

// the strongest frequency (Hz) in a stretch of the clip, by FFT with parabolic refinement
static double peakFreq(const Clip& c, std::size_t from, std::size_t n = 16384) {
 detail::Fft fft(n); const auto w = detail::hann(n); std::vector<std::complex<double>> b(n);
 for (std::size_t i = 0; i < n; ++i) b[i] = (from + i < c.frames() ? c.l[from + i] : 0.f) * w[i];
 fft.run(b, false);
 std::size_t best = 1; for (std::size_t k = 1; k < n / 2; ++k) if (std::abs(b[k]) > std::abs(b[best])) best = k;
 const double a = std::abs(b[best - 1]), m = std::abs(b[best]), z = std::abs(b[best + 1]);
 const double off = .5 * (a - z) / (a - 2 * m + z + 1e-30);
 return (static_cast<double>(best) + off) * c.sampleRate / static_cast<double>(n);
}
static double rms(const Clip& c, std::size_t a, std::size_t b) { double s = 0; b = std::min(b, c.frames()); for (std::size_t i = a; i < b; ++i) s += static_cast<double>(c.l[i]) * c.l[i]; return std::sqrt(s / static_cast<double>(std::max<std::size_t>(1, b - a))); }
static double db(double x) { return 20 * std::log10(x + 1e-12); }
static bool clean(const Clip& c) { for (std::size_t i = 0; i < c.frames(); ++i) if (!std::isfinite(c.l[i]) || !std::isfinite(c.r[i])) return false; return true; }

static void stretchTest() {
 const char* names[3] = {"beats", "tones", "texture"};
 for (int mode = 0; mode < 3; ++mode) for (double ratio : {.5, 2.0, 4.0}) {
  const Clip in = chord(3.0);
  const Clip out = timeStretch(in, ratio, mode);
  const double wantLen = static_cast<double>(in.frames()) * ratio;
  const double f0 = peakFreq(in, 20000), f1 = peakFreq(out, static_cast<std::size_t>(wantLen * .4));
  const double r = rms(out, static_cast<std::size_t>(wantLen * .2), static_cast<std::size_t>(wantLen * .8)), r0 = rms(in, 20000, 100000);
  std::printf("stretch %-7s x%.1f: length %.3f of %.3f, strongest frequency %.1f Hz (was %.1f), level %+.1f dB\n", names[mode], ratio, static_cast<double>(out.frames()) / wantLen, 1.0, f1, f0, db(r) - db(r0));
  verify(out.frames() > 0 && std::abs(static_cast<double>(out.frames()) / wantLen - 1.0) < .02, "stretch: the length");
  if (mode < 2) verify(std::abs(f1 - f0) < 6.0, "stretch: the pitch must stay");
  else verify(std::abs(f1 - 220.0) < 8.0 || std::abs(f1 - 277.2) < 8.0 || std::abs(f1 - 330.0) < 8.0, "stretch (texture): what is heard is still the chord");
  verify(std::abs(db(r) - db(r0)) < (mode == 2 ? 9.0 : 2.5), "stretch: the level must stay");
  verify(clean(out), "stretch: no garbage");
 }
 // beats keeps the clicks sharp: the attack is not smeared into the gaps
 { const Clip in = clicks(3.0); const Clip out = timeStretch(in, 2.0, 0);
   const double peak = peakOf(out); verify(peak > .5, "stretch (beats): transients keep their weight");
   std::size_t quiet = 0, total = 0; for (std::size_t i = 0; i < out.frames(); i += 100) { ++total; if (std::abs(out.l[i]) < .02f) ++quiet; }
   verify(static_cast<double>(quiet) / static_cast<double>(total) > .6, "stretch (beats): the gaps between the hits stay gaps"); }
 // no change is no work
 { const Clip in = chord(1.0); const Clip out = timeStretch(in, 1.0, 1); verify(out.frames() == in.frames() && out.l[1234] == in.l[1234], "stretch: x1 returns the clip"); }
 // cancel
 { const Clip in = chord(10.0); int calls = 0; const Clip out = timeStretch(in, 3.0, 1, [&] { return ++calls > 3; }); verify(out.empty(), "stretch: cancelling gives nothing"); }
 std::puts("stretch: ok");
}

static void pitchTest() {
 const Clip in = chord(2.0);
 RenderParams p; p.fadeInMs = 0; p.fadeOutMs = 0;
 p.pitchSt = 7;
 const Clip up = render(in, p);
 std::printf("pitch +7 st: length %.3f, frequency %.1f -> %.1f Hz\n", static_cast<double>(up.frames()) / static_cast<double>(in.frames()), peakFreq(in, 20000), peakFreq(up, 20000));
 verify(std::abs(static_cast<double>(up.frames()) / static_cast<double>(in.frames()) - 1.0) < .02, "pitch: the length stays");
 verify(std::abs(peakFreq(up, 20000) / peakFreq(in, 20000) - std::exp2(7.0 / 12.0)) < .01, "pitch: +7 semitones");
 p.pitchSt = -12; const Clip down = render(in, p);
 verify(std::abs(peakFreq(down, 20000) / peakFreq(in, 20000) - .5) < .01 && std::abs(static_cast<double>(down.frames()) / static_cast<double>(in.frames()) - 1.0) < .02, "pitch: an octave down");
 // tape: the speed and the pitch together
 p.pitchSt = 0; p.tape = true; p.stretchPct = 200; const Clip tape = render(in, p);
 verify(std::abs(static_cast<double>(tape.frames()) / static_cast<double>(in.frames()) - 2.0) < .01 && std::abs(peakFreq(tape, 20000) / peakFreq(in, 20000) - .5) < .01, "tape: twice as long is an octave down");
 // stretch and pitch together, in the pipeline
 p = RenderParams{}; p.fadeInMs = 0; p.fadeOutMs = 0; p.stretchPct = 150; p.pitchSt = 5; p.stretchMode = 1; const Clip both = render(in, p);
 verify(std::abs(static_cast<double>(both.frames()) / static_cast<double>(in.frames()) - 1.5) < .03 && std::abs(peakFreq(both, 40000) / peakFreq(in, 20000) - std::exp2(5.0 / 12.0)) < .015, "stretch and pitch are independent");
 std::puts("pitch: ok");
}

static void effectsTest() {
 // filter
 { Clip a = sine(200, 1.0, .5), b = sine(8000, 1.0, .5); filterSweep(a, 0, 1000, 1000, 20); filterSweep(b, 0, 1000, 1000, 20);
   verify(db(rms(a, 20000, 40000)) > db(.5 / std::sqrt(2.0)) - 1.0 && db(rms(b, 20000, 40000)) < db(.5 / std::sqrt(2.0)) - 25.0, "filter: low-pass passes the low and removes the high");
   Clip h = sine(200, 1.0, .5); filterSweep(h, 1, 1000, 1000, 20); verify(db(rms(h, 20000, 40000)) < -30.0, "filter: high-pass removes the low");
   Clip s = sine(2000, 2.0, .5); filterSweep(s, 0, 20000, 200, 20); verify(rms(s, 1000, 5000) > .3 && rms(s, s.frames() - 5000, s.frames()) < .02, "filter: the sweep closes over time"); }
 // drive: more harmonics
 { Clip a = sine(500, .5, .3), b = a; drive(b, 0, 80, 100);
   const double h1 = std::abs(peakFreq(b, 8000) - 500.0); (void)h1;
   double r0 = rms(a, 8000, 20000), r1 = rms(b, 8000, 20000); verify(std::abs(db(r1) - db(r0)) < 3.0, "drive: the level stays about the same"); 
   // the wave is no longer a sine: its crest factor drops
   double pk0 = peakOf(a), pk1 = peakOf(b); verify(pk1 / r1 < pk0 / r0, "drive: the wave is flattened"); }
 // crush
 { Clip a = sine(300, .3, .9); crush(a, 3, 48000, 100); std::vector<float> seen; for (std::size_t i = 0; i < a.frames(); ++i) { bool f = false; for (float v : seen) if (std::abs(v - a.l[i]) < 1e-6f) { f = true; break; } if (!f) seen.push_back(a.l[i]); }
   verify(seen.size() <= 9, "crush: 3 bits leave at most 9 levels (the 8 steps and the top)");
   Clip r = sine(300, .3, .5); crush(r, 16, 4000, 100); int changes = 0; for (std::size_t i = 1; i < r.frames(); ++i) if (r.l[i] != r.l[i - 1]) ++changes; verify(changes < static_cast<int>(r.frames() / 8), "crush: a low rate holds the samples"); }
 // ring: sidebands
 { Clip a = sine(1000, 1.0, .5); ring(a, 300, 100); const double f = peakFreq(a, 10000); verify(std::abs(f - 700.0) < 8.0 || std::abs(f - 1300.0) < 8.0, "ring: the sidebands, not the tone"); }
 // chop
 { Clip a = sine(400, 2.0, .5); chop(a, 0, 100, 80); int silent = 0, total = 0; for (std::size_t i = 4800; i < 40000; i += 20) { ++total; if (std::abs(a.l[i]) < .001f) ++silent; }
   verify(silent > total / 4 && silent < total * 9 / 10, "chop: part of each slice is shut");
   Clip b = sine(400, 2.0, .5); chop(b, 1, 100, 60); verify(clean(b) && peakOf(b) > .4, "chop: stutter keeps the sound"); }
 // echo: a repeat where it should be, and a tail
 { Clip a; a.sampleRate = SR; a.l.assign(48000, 0.f); a.r = a.l; a.l[100] = a.r[100] = 1.f; echo(a, 250, 50, 0, 100);
   const std::size_t at = 100 + 12000; verify(a.l[at] > .4f && a.l[at] < 1.01f, "echo: the first repeat is at the time set"); verify(a.l[at + 12000] > .15f && a.l[at + 12000] < a.l[at], "echo: the repeats fade");
   verify(a.frames() > 48000 + 12000, "echo: the tail is kept"); }
 // reverb: a tail after the sound ends, and transparent at mix 0
 { Clip a = sine(300, .3, .5); const Clip d = a; reverb(a, 70, 30, 40); verify(a.frames() > d.frames() + 24000 && rms(a, d.frames() + 2000, d.frames() + 8000) > .003, "reverb: it rings on after the sound");
   Clip z = d; reverb(z, 70, 30, 0); double worst = 0; for (std::size_t i = 0; i < d.frames(); ++i) worst = std::max(worst, static_cast<double>(std::abs(z.l[i] - d.l[i]))); verify(worst < 1e-5, "reverb: mix 0 is the dry sound"); verify(clean(a), "reverb: no garbage"); }
 // tape stop: shorter pitch falling, ends quiet
 { const Clip in = sine(1000, 2.0, .5); const Clip o = tapeWarp(in, 100, 0); verify(o.frames() > in.frames() && peakFreq(o, 2000, 8192) > peakFreq(o, o.frames() - 30000, 8192) * 1.3, "tape stop: the pitch falls"); verify(std::abs(o.l[o.frames() - 1]) < .01f, "tape stop: ends in silence"); 
   const Clip st = tapeWarp(in, 100, 1); verify(peakFreq(st, 2000, 8192) < peakFreq(st, st.frames() - 20000, 8192), "tape start: the pitch rises"); }

 // a starting tape begins in silence and ends at full level; a stopping one is the other way round
 { const Clip in = sine(1000, 2.0, .5); const Clip st = tapeWarp(in, 100, 1); verify(std::abs(st.l[2]) < .01f && peakOf(Clip{{st.l.end() - 2000, st.l.end()}, {st.l.end() - 2000, st.l.end()}, SR}) > .3, "tape start: in from silence, out at full level"); }
 // pitching up does not fold the highs back: 15 kHz one octave up has nothing left to fold
 { Clip hi = sine(15000, .5, .5); const Clip up = resample(hi, 2.0); verify(rms(up, 2000, 8000) < .02, "resample: no aliasing when reading faster"); const Clip same = resample(sine(500, .5, .5), 2.0); verify(rms(same, 2000, 8000) > .3, "resample: the low stays"); }
 // a big reverb rings for seconds and dies away before it is cut
 { Clip a = sine(300, .3, .5); reverb(a, 100, 30, 40); trimSilence(a); const double seconds = static_cast<double>(a.frames()) / SR; verify(seconds > 6.0, "reverb: a big room rings for seconds"); verify(rms(a, a.frames() - 4000, a.frames()) < .003, "reverb: and it has died away at the end"); }
 // chop with no depth leaves the clip alone
 { const Clip a = sine(400, 1.0, .5); Clip b = a, c2 = a; chop(b, 1, 125, 0); chop(c2, 0, 125, 0); double worst = 0; for (std::size_t i = 0; i < a.frames(); ++i) worst = std::max({worst, static_cast<double>(std::abs(a.l[i] - b.l[i])), static_cast<double>(std::abs(a.l[i] - c2.l[i]))}); verify(worst == 0.0, "chop: no depth, no change"); }
 std::puts("effects: ok");
}

static void pipelineTest() {
 const Clip in = chord(2.0);
 // all off is the region, with the fades
 { RenderParams p; p.fadeInMs = 0; p.fadeOutMs = 0; const Clip o = render(in, p); double worst = 0; for (std::size_t i = 10; i + 10 < o.frames(); ++i) worst = std::max(worst, static_cast<double>(std::abs(o.l[i] - in.l[i])));
   verify(o.frames() == in.frames() && worst < 1e-6, "pipeline: nothing set returns the clip"); }
 { RenderParams p; p.start = .25; p.end = .75; p.fadeInMs = 0; p.fadeOutMs = 0; p.reverse = true; const Clip o = render(in, p); verify(std::abs(static_cast<double>(o.frames()) - static_cast<double>(in.frames()) * .5) < 3 && std::abs(o.l[200] - in.l[in.frames() * 3 / 4 - 201]) < 1e-5f, "pipeline: region and reverse"); }
 // every effect on at once is finite and bounded
 { RenderParams p; p.stretchPct = 130; p.stretchMode = 0; p.pitchSt = -3; p.warp = 40; p.reverse = true; p.filterOn = true; p.filterFrom = 8000; p.filterTo = 400; p.driveOn = true; p.driveAmount = 70; p.crushOn = true; p.crushBits = 8; p.crushRate = 16000; p.ringOn = true; p.chopOn = true; p.chopMode = 1; p.echoOn = true; p.reverbOn = true; p.gainDb = 6;
   const Clip o = render(in, p); verify(!o.empty() && clean(o) && peakOf(o) <= 8.01, "pipeline: everything at once stays finite"); }
 // odd regions
 { RenderParams p; p.start = .9; p.end = .1; verify(!render(in, p).empty(), "pipeline: a reversed region is swapped"); p.start = p.end = .5; verify(render(in, p).frames() >= 64, "pipeline: an empty region still gives a little"); Clip e; verify(render(e, RenderParams{}).empty(), "pipeline: an empty clip"); }
 // tape and pitch together: the pitch counts
 { RenderParams p; p.fadeInMs = 0; p.fadeOutMs = 0; p.tape = true; p.pitchSt = 12; const Clip o = render(in, p); verify(std::abs(peakFreq(o, 20000) / peakFreq(in, 20000) - 2.0) < .02, "tape: pitch adds to the speed"); }
 // a stretch that would not fit is held at the limit
 { Clip small = sine(300, 10.0); small.sampleRate = 8000.0; small.l.resize(80000); small.r.resize(80000); RenderParams p; p.stretchPct = 1600; p.stretchMode = 0; const Clip o = render(small, p); verify(!o.empty() && static_cast<double>(o.frames()) <= 121.0 * 8000.0, "pipeline: the result is at most about two minutes"); }
 // cancel
 { RenderParams p; p.stretchPct = 300; p.stretchMode = 1; int n = 0; verify(render(in, p, [&] { return ++n > 2; }).empty(), "pipeline: cancelled"); }
 // limits of the knobs
 { RenderParams p; p.stretchPct = 10; verify(!render(in, p).empty(), "pipeline: 10%"); p.stretchPct = 1600; p.stretchMode = 2; Clip small = sine(300, .5); verify(!render(small, p).empty(), "pipeline: 1600% texture"); }
 std::puts("pipeline: ok");
}

int main() { stretchTest(); pitchTest(); effectsTest(); pipelineTest(); std::puts("capture fx: all good"); return 0; }
