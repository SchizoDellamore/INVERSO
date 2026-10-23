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

int main(int argc, char** argv) {
 classic();
 granular();
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
