// ABISMO SAMPLER engine: pitch tracking, reverse, loops, granular freeze and
// time-stretch, sample swapping, presets and randomised stress.
// With a directory argument it writes one demo WAV per preset there.
#include "../Source/DSP/SamplerDefault.h"
#include "TestUtil.h"
using namespace test;
namespace S = inverso_abismo::sampler;

static S::SampleData sineSample(double freq, double seconds, int sr = 48000) {
 S::SampleData d;
 d.sampleRate = sr;
 d.name = "sine";
 const auto n = static_cast<std::size_t>(seconds * sr);
 d.left.resize(n);
 for (std::size_t i = 0; i < n; ++i) d.left[i] = .5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * freq * static_cast<double>(i) / sr));
 return d;
}
static S::Values plain() {
 S::Values v = S::defaults();
 v[S::P::RootNote] = 69; v[S::P::RevMix] = 0; v[S::P::Attack] = 2; v[S::P::Release] = 30;
 return v;
}
static Stereo play(S::Engine& e, int samples) {
 Stereo out{std::vector<float>(static_cast<std::size_t>(samples)), std::vector<float>(static_cast<std::size_t>(samples))};
 for (int i = 0; i < samples; i += 256) e.render(out.l.data() + i, out.r.data() + i, std::min(256, samples - i));
 return out;
}

static void classic() {
 const int sr = 48000;
 auto sine = sineSample(440.0, 2.0);
 for (int hostRate : {44100, 48000, 96000}) {
  S::Engine e; e.setValues(plain()); e.prepare(hostRate); e.setSample(&sine);
  e.noteOn(81, 1.f); // one octave above the root
  auto s = play(e, hostRate / 2);
  const double f = frequency(s.l, hostRate, static_cast<std::size_t>(hostRate / 10), s.l.size());
  std::printf("classic %d Hz host: note 81 from 440 Hz root -> %.2f Hz\n", hostRate, f);
  verify(std::abs(f - 880.0) < 1.0, "classic playback must track pitch and sample rate");
 }
 // One-shot ends by itself, even with the key held.
 S::Engine e; e.setValues(plain()); e.prepare(sr); e.setSample(&sine);
 e.noteOn(69, 1.f);
 play(e, sr * 3);
 verify(e.activeVoices() == 0, "one-shot voice must end with the sample");
 // The region: START 25%, END 50% of a 2 s sample plays half a second, then stops.
 {
  auto rv = plain(); rv[S::P::Start] = 25; rv[S::P::End] = 50;
  S::Engine er; er.setValues(rv); er.prepare(sr); er.setSample(&sine);
  er.noteOn(69, 1.f);
  auto out = play(er, sr);
  const float inside = rms(out.l, static_cast<std::size_t>(sr / 10), static_cast<std::size_t>(sr * 4 / 10));
  const float after = rms(out.l, static_cast<std::size_t>(sr * 6 / 10), static_cast<std::size_t>(sr));
  std::printf("region 25-50%%: %.3f inside, %.6f after END\n", inside, after);
  verify(inside > .1f && after < 1e-4f && er.activeVoices() == 0, "a one-shot does not stop at END");
 }
 // Forward loop sustains well past the end of the sample.
 auto v = plain(); v[S::P::LoopMode] = 1;
 e.setValues(v); e.noteOn(69, 1.f);
 auto looped = play(e, sr * 6);
 verify(rms(looped.l, static_cast<std::size_t>(sr * 5), looped.l.size()) > .1f, "forward loop must sustain");
 e.noteOff(69); play(e, sr);
 v[S::P::LoopMode] = 2; e.setValues(v); e.noteOn(69, 1.f);
 auto pingpong = play(e, sr * 6);
 verify(rms(pingpong.l, static_cast<std::size_t>(sr * 5), pingpong.l.size()) > .1f, "ping-pong loop must sustain");
 e.allSoundOff();

 // Reverse: a rising ramp sample must come out falling.
 S::SampleData ramp; ramp.sampleRate = sr; ramp.left.resize(sr);
 for (int i = 0; i < sr; ++i) ramp.left[static_cast<std::size_t>(i)] = static_cast<float>(i) / sr * .8f;
 v = plain(); v[S::P::Reverse] = 1;
 S::Engine rev; rev.setValues(v); rev.prepare(sr); rev.setSample(&ramp);
 rev.noteOn(69, 1.f);
 auto back = play(rev, sr / 2);
 verify(back.l[1000] > back.l[20000] && back.l[20000] > .05f, "reverse must play the sample backwards");
}

static void granular() {
 const int sr = 48000;
 auto sine = sineSample(440.0, 3.0);
 // Freeze (scan 0): steady output long after the source would have ended.
 auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 0; v[S::P::Spray] = 0; v[S::P::GrainSize] = 100; v[S::P::Density] = 40;
 S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&sine);
 e.noteOn(69, 1.f);
 auto frozen = play(e, sr * 6);
 const float early = rms(frozen.l, sr, sr * 2), late = rms(frozen.l, sr * 5, sr * 6);
 std::printf("granular freeze rms %.3f -> %.3f\n", early, late);
 verify(late > .05f && std::abs(late - early) < early * .3f, "granular freeze must sustain at a steady level");
 const double f = goertzel(frozen.l, sr, 440.0, sr * 4, sr * 5), off = goertzel(frozen.l, sr, 520.0, sr * 4, sr * 5);
 verify(f > off * 10.0, "grains must keep the source pitch at the root note");
 // Time-stretch: scan 25% with transposition +12 -> 880 Hz independent of speed.
 v[S::P::Scan] = 25; e.setValues(v); e.allSoundOff();
 e.noteOn(81, 1.f);
 auto stretched = play(e, sr * 2);
 verify(goertzel(stretched.l, sr, 880.0, sr / 2, sr * 2) > 10.0 * goertzel(stretched.l, sr, 440.0, sr / 2, sr * 2),
        "granular transposition must not depend on scan speed");
}

// Granular: LOOP decides how SCAN travels. Off: round the whole region; Forward: round the loop;
// Ping-pong: back and forth inside the loop. Grains come from the region only.
static void granularLoops() {
 const int sr = 48000;
 auto sine = sineSample(440.0, 4.0);
 for (int mode : {0, 1, 2}) {
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 100; v[S::P::Spray] = 0; v[S::P::Position] = 45;
  v[S::P::LoopMode] = static_cast<float>(mode); v[S::P::LoopStart] = 40; v[S::P::LoopEnd] = 60;
  v[S::P::Start] = 10; v[S::P::End] = 90;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&sine);
  e.noteOn(69, 1.f);
  float lo = 1.f, hi = 0.f, prev = -1.f; int wraps = 0, turns = 0; float lastStep = 0.f;
  std::vector<float> l(256), r(256);
  for (int b = 0; b < sr * 12 / 256; ++b) {
   e.render(l.data(), r.data(), 256);
   const float at = e.playheadPosition();
   if (b > 10) { lo = std::min(lo, at); hi = std::max(hi, at); }
   if (prev >= 0.f) {
    const float step = at - prev;
    if (step < -.1f) ++wraps;
    else if (std::abs(step) > 1e-6f && std::abs(lastStep) > 1e-6f && (step > 0.f) != (lastStep > 0.f)) ++turns;
    if (std::abs(step) > 1e-6f && std::abs(step) < .1f) lastStep = step;
   }
   prev = at;
  }
  std::printf("granular loop %s: cloud %.2f..%.2f, %d wraps, %d turns\n", mode == 0 ? "off" : mode == 1 ? "forward" : "ping-pong", lo, hi, wraps, turns);
  if (mode == 0) verify(lo < .15f && hi > .85f && lo >= .099f && hi <= .901f && wraps >= 2, "granular, loop off: the cloud must travel the whole region (START..END), round and round");
  if (mode == 1) verify(lo >= .399f && hi <= .601f && wraps >= 5 && turns == 0, "granular, loop forward: the cloud must go round the loop");
  if (mode == 2) verify(lo >= .399f && hi <= .601f && hi > .58f && lo < .42f && turns >= 5 && wraps == 0, "granular, ping-pong: the cloud must go back and forth inside the loop");
 }
}

// Review findings, each one a test: the cloud must not stick at the end of a region ending at
// 100% (also with a 44.1 kHz file at 48 kHz); a POSITION outside START..END starts at the
// region's edge; grains never play what lies outside START..END.
static void granularEdges() {
 const int sr = 48000;
 {
  auto file = sineSample(440.0, 4.0, 44100);
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 100; v[S::P::Spray] = 0; v[S::P::LoopMode] = 0; v[S::P::Start] = 0; v[S::P::End] = 100;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&file);
  e.noteOn(69, 1.f);
  std::vector<float> l(256), r(256);
  float prev = -1.f; int wraps = 0, still = 0;
  for (int b = 0; b < sr * 12 / 256; ++b) {
   e.render(l.data(), r.data(), 256);
   const float at = e.playheadPosition();
   if (prev >= 0.f && at < prev - .5f) ++wraps;
   if (prev >= 0.f && std::abs(at - prev) < 1e-7f) ++still;
   prev = at;
  }
  std::printf("granular, END 100%%, 44.1k file: %d wraps, %d still blocks\n", wraps, still);
  verify(wraps >= 2 && still < 3, "granular cloud stuck at the end of a region ending at 100%");
 }
 {
  auto sine = sineSample(440.0, 4.0);
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 0; v[S::P::Position] = 5; v[S::P::Start] = 10; v[S::P::End] = 90;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&sine);
  e.noteOn(69, 1.f);
  std::vector<float> l(256), r(256);
  for (int b = 0; b < 20; ++b) e.render(l.data(), r.data(), 256);
  std::printf("granular, POSITION 5%% before START 10%%: cloud at %.3f\n", e.playheadPosition());
  verify(std::abs(e.playheadPosition() - .10f) < .002f, "a POSITION before START must start the cloud at START");
  for (float loopMode : {0.f, 1.f}) {
   v[S::P::Position] = 95; v[S::P::LoopMode] = loopMode; v[S::P::LoopStart] = 30; v[S::P::LoopEnd] = 60;
   S::Engine e2; e2.setValues(v); e2.prepare(sr); e2.setSample(&sine);
   e2.noteOn(69, 1.f);
   for (int b = 0; b < 20; ++b) e2.render(l.data(), r.data(), 256);
   std::printf("granular, POSITION 95%% after END 90%% (loop %s): cloud at %.3f\n", loopMode > 0.f ? "forward" : "off", e2.playheadPosition());
   verify(std::abs(e2.playheadPosition() - .90f) < .002f, "a POSITION after END must start the cloud at END");
  }
 }
 for (bool reversed : {false, true}) {
  // 440 Hz inside the region (30..60%), 1760 Hz outside; big grains near the region's end
  S::SampleData d; d.sampleRate = sr; d.name = "two tones";
  const std::size_t n = static_cast<std::size_t>(sr) * 4;
  d.left.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
   const double x = static_cast<double>(i) / static_cast<double>(n);
   const double f = (x >= .3 && x <= .6) ? 440.0 : 1760.0;
   d.left[i] = .5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * f * static_cast<double>(i) / sr));
  }
  // REVERSE: START..END refer to the reversed sound, so the file's 30..60% is the reversed 40..70%
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 0; v[S::P::Spray] = 30; v[S::P::GrainSize] = 400;
  v[S::P::Position] = reversed ? 68.f : 58.f; v[S::P::Start] = reversed ? 40.f : 30.f; v[S::P::End] = reversed ? 70.f : 60.f;
  v[S::P::Reverse] = reversed ? 1.f : 0.f;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&d);
  e.noteOn(69, 1.f);
  auto out = play(e, sr * 3);
  const double inside = goertzel(out.l, sr, 440.0, sr / 2, sr * 3), outside = goertzel(out.l, sr, 1760.0, sr / 2, sr * 3);
  std::printf("granular region %s: inside %.3f, outside %.5f\n", reversed ? "reversed" : "forward", inside, outside);
  verify(inside > .01 && outside < inside * .01, "grains must not play what lies outside START..END");
 }
}

// What is heard, not only where the cloud is drawn: a cloud parked at END plays from END (also
// with spray), and a region much shorter than a grain gives a short grain, not a buzz.
static void granularHeard() {
 const int sr = 48000;
 S::SampleData d; d.sampleRate = sr; d.name = "two tones";
 const std::size_t n = static_cast<std::size_t>(sr) * 4;
 d.left.resize(n);
 for (std::size_t i = 0; i < n; ++i) {
  const double x = static_cast<double>(i) / static_cast<double>(n);
  const double f = x >= .8 ? 440.0 : 1760.0; // the last 20% sounds different
  d.left[i] = .5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * f * static_cast<double>(i) / sr));
 }
 for (float spray : {0.f, 30.f}) {
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 0; v[S::P::Spray] = spray; v[S::P::Position] = 95; v[S::P::Start] = 10; v[S::P::End] = 90;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&d);
  e.noteOn(69, 1.f);
  auto out = play(e, sr * 2);
  const double nearEnd = goertzel(out.l, sr, 440.0, sr / 2, sr * 2), start = goertzel(out.l, sr, 1760.0, sr / 2, sr * 2);
  std::printf("granular cloud parked at END, spray %.0f%%: END tone %.3f, START tone %.4f\n", spray, nearEnd, start);
  verify(nearEnd > .01 && start < nearEnd * .2, "a cloud parked at END must play from END, not from START");
 }
 {
  auto sine = sineSample(440.0, 4.0);
  auto v = plain(); v[S::P::Mode] = 1; v[S::P::Scan] = 0; v[S::P::Spray] = 0; v[S::P::GrainSize] = 400; v[S::P::Start] = 50; v[S::P::End] = 50.01f;
  S::Engine e; e.setValues(v); e.prepare(sr); e.setSample(&sine);
  e.noteOn(69, 1.f);
  auto out = play(e, sr * 2);
  float hf = 0.f, prev = 0.f;
  for (std::size_t i = static_cast<std::size_t>(sr) / 2; i < out.l.size(); ++i) { hf = std::max(hf, std::abs(out.l[i] - prev)); prev = out.l[i]; }
  std::printf("granular, region of a few samples: largest step %.4f, rms %.4f\n", hf, rms(out.l, sr / 2, sr * 2));
  verify(finite(out) && hf < .05f, "a tiny region must not turn grains into a buzz");
 }
}

static void swapping() {
 const int sr = 48000;
 auto a = sineSample(440.0, 1.0), b = sineSample(220.0, 1.0);
 S::Engine e; auto v = plain(); v[S::P::LoopMode] = 1; e.setValues(v); e.prepare(sr);
 play(e, 512); // no sample yet: silent, no crash
 e.noteOn(69, 1.f);
 verify(peak(play(e, 512)) == 0.f, "no sample must be silent");
 e.setSample(&a); e.noteOn(69, 1.f); play(e, sr / 4);
 e.setSample(&b);
 verify(e.activeVoices() == 0, "sample swap must cut voices that read the old sample");
 e.noteOn(69, 1.f);
 auto s = play(e, sr / 2);
 verify(std::abs(frequency(s.l, sr, sr / 10, s.l.size()) - 220.0) < 1.0, "new sample must play after swap");
}

// RESAMPLE: off is untouched; a rate keeps what is under its Nyquist and takes
// away what is above; changing the rate does not click.
static void resampler() {
 const double sr = 48000;
 auto tone = [&](double f, int choice, float sat) {
  inverso_abismo::Resampler rs; rs.prepare(sr); rs.set(choice, sat);
  double in = 0, out = 0;
  for (int i = 0; i < 48000; ++i) {
   float l = .3f * static_cast<float>(std::sin(2 * 3.14159265358979 * f * i / sr)), r = l;
   const float x = l;
   rs.process(l, r);
   verify(std::isfinite(l) && std::isfinite(r), "resample: not finite");
   if (i > 9600) { in += static_cast<double>(x) * x; out += static_cast<double>(l) * l; }
  }
  return 10 * std::log10(out / in);
 };
 verify(std::abs(tone(1000, 0, 0.f)) < 1e-4, "resample off changes the sound");
 const double pass = tone(1000, 4, 0.f), stop = tone(4000, 4, 0.f), stop2 = tone(9000, 2, 0.f), pass2 = tone(3000, 2, 0.f);
 std::printf("resample 5000 Hz: 1 kHz %+.2f dB, 4 kHz %+.1f dB; 10000 Hz: 3 kHz %+.2f dB, 9 kHz %+.1f dB\n", pass, stop, pass2, stop2);
 verify(std::abs(pass) < 1.0 && std::abs(pass2) < 1.0, "resample loses what is under its Nyquist");
 verify(stop < -30 && stop2 < -30, "resample keeps what is above its Nyquist");
 const double sat = tone(200, 4, 1.f);
 std::printf("resample saturation at full: %+.2f dB on a -10 dBFS tone\n", sat);
 verify(std::abs(sat) < 3.0, "resample saturation changes the level too much");
 // rate changes while a tone plays: no step bigger than a held rate's own
 // (the held rates themselves leave a little of the steps' top end)
 auto worstStep = [&](bool switching, int fixed) {
  inverso_abismo::Resampler rs; rs.prepare(sr); rs.set(fixed, 0.f);
  float prev = 0, prev2 = 0, worst = 0;
  for (int i = 0; i < 96000; ++i) {
   if (switching && i % 9600 == 0) rs.set((i / 9600) % 8, 0.f);
   float l = .3f * static_cast<float>(std::sin(2 * 3.14159265358979 * 300 * i / sr)), r = l;
   rs.process(l, r);
   if (i > 4800) worst = std::max(worst, std::abs(l - 2 * prev + prev2));
   prev2 = prev; prev = l;
  }
  return worst;
 };
 float steady = 0;
 for (int k = 0; k < 8; ++k) steady = std::max(steady, worstStep(false, k));
 const float moving = worstStep(true, 0);
 std::printf("resample rate changes: worst step %.5f, steadiest held rate %.5f\n", moving, steady);
 verify(moving < steady * 1.5f, "changing the resample rate clicks");
}

int main(int argc, char** argv) {
 resampler();
 classic();
 granular();
 granularLoops();
 granularEdges();
 granularHeard();
 swapping();
 const int sr = 48000;
 const auto voice = S::makeDefaultSample(sr);
 std::printf("default sample: %d frames, rms %.3f\n", voice.length(), rms(voice.left));
 verify(voice.length() == sr * 5 && rms(voice.left) > .02f, "default sample must be audible");

 detail::Random rnd;
 for (std::size_t i = 0; i < S::presets().size(); ++i) {
  S::Engine e; e.setValues(S::presetValues(i)); e.prepare(sr); e.setSample(&voice);
  e.setTransport(100.0, 0.0, true);
  Stereo all;
  auto append = [&](const Stereo& s) {
   all.l.insert(all.l.end(), s.l.begin(), s.l.end());
   all.r.insert(all.r.end(), s.r.begin(), s.r.end());
  };
  e.noteOn(60, .9f); append(play(e, sr * 4)); e.noteOff(60);
  e.noteOn(53, .8f); e.noteOn(60, .8f); append(play(e, sr * 3)); e.noteOff(53); e.noteOff(60);
  e.noteOn(67, .7f); append(play(e, sr * 2)); e.noteOff(67);
  append(play(e, sr * 4));
  const float p = peak(all), level = rms(all.l, 0, static_cast<std::size_t>(sr * 4));
  std::printf("%-22s peak %.3f  rms %.4f\n", S::presets()[i].name, p, level);
  verify(finite(all) && p <= 1.f, "sampler preset output invalid");
  verify(level > 2e-3f, "sampler preset silent");
  if (argc > 1) {
   std::string name = std::string(argv[1]) + "/sampler_" + std::to_string(i) + "_" + S::presets()[i].name + ".wav";
   for (auto& ch : name) if (ch == ' ') ch = '_';
   writeWav(name, all, sr);
  }
 }

 for (int hostRate : {44100, 96000}) {
  S::Engine e; e.prepare(hostRate); e.setSample(&voice);
  std::vector<float> l(4096), r(4096);
  for (int b = 0; b < 500; ++b) {
   S::Values v = S::defaults();
   for (int p = 0; p < S::P::Count; ++p) {
    const auto& s = S::specs()[static_cast<std::size_t>(p)];
    const float u = .5f * (rnd.next() + 1.f);
    v[static_cast<std::size_t>(p)] = s.kind == Kind::Float ? s.min + u * (s.max - s.min) : std::round(s.min + u * (s.max - s.min));
   }
   e.setValues(v);
   if (b % 3 == 0) e.noteOn(24 + static_cast<int>((rnd.next() + 1.f) * 40.f), .5f * (rnd.next() + 1.f));
   if (b % 5 == 0) e.noteOff(24 + static_cast<int>((rnd.next() + 1.f) * 40.f));
   if (b % 7 == 0) e.setPitchBend(rnd.next());
   if (b % 11 == 0) e.setModWheel(.5f * (rnd.next() + 1.f));
   if (b % 41 == 0) e.setSample(b % 82 == 0 ? nullptr : &voice);
   const int n = 1 + static_cast<int>((rnd.next() + 1.f) * 1000.f);
   e.render(l.data(), r.data(), n);
   for (int i = 0; i < n; ++i)
    verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::abs(l[static_cast<std::size_t>(i)]) <= 1.f &&
           std::isfinite(r[static_cast<std::size_t>(i)]) && std::abs(r[static_cast<std::size_t>(i)]) <= 1.f, "stress output invalid");
  }
  std::printf("%d Hz randomised sampler automation: pass\n", hostRate);
 }
 std::puts("Abismo sampler tests passed");
}
