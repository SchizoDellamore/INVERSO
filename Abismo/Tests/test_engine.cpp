#include "TestUtil.h"
using namespace test;

static void tuning() {
 for (int sr : {44100, 48000, 96000}) {
  Engine e; e.setValues(pureSine()); e.prepare(sr);
  auto silent = render(e, 1024);
  verify(peak(silent) == 0.f, "engine must be silent without notes");
  e.noteOn(69, 1.f);
  auto a = render(e, sr);
  const double f = frequency(a.l, sr, static_cast<std::size_t>(sr / 4), a.l.size());
  std::printf("%d Hz: A4 measured %.3f Hz\n", sr, f);
  verify(std::abs(f - 440.0) < .5, "A4 must play at 440 Hz");
  e.setPitchBend(1.f);
  auto bent = render(e, sr / 2);
  const double fb = frequency(bent.l, sr, static_cast<std::size_t>(sr / 8), bent.l.size());
  verify(std::abs(fb - 493.883) < 1.0, "+2 semitone pitch bend");
  Values v = pureSine(); v[P::Octave] = 3; e.setValues(v); e.setPitchBend(0.f);
  auto up = render(e, sr / 2);
  verify(std::abs(frequency(up.l, sr, static_cast<std::size_t>(sr / 8), up.l.size()) - 880.0) < 1.0, "+1 octave switch");
 }
}

static void releaseAndPedal() {
 Engine e; e.setValues(pureSine()); e.prepare(48000);
 e.noteOn(60, .8f); render(e, 4800);
 e.noteOff(60);
 auto tail = render(e, 48000);
 verify(e.activeVoices() == 0, "voice must free itself after release");
 verify(rms(tail.l, 24000) < 1e-5f, "release must reach silence");

 e.setSustain(true);
 e.noteOn(64, .8f); render(e, 4800); e.noteOff(64);
 render(e, 48000);
 verify(e.activeVoices() == 1, "sustain pedal must hold the note");
 e.setSustain(false);
 render(e, 48000);
 verify(e.activeVoices() == 0, "pedal up must release held notes");
}

static void polyphony() {
 Engine e; Values v = presetValues(1); e.setValues(v); e.prepare(48000);
 for (int n = 30; n < 80; ++n) { e.noteOn(n, .9f); render(e, 64); }
 verify(e.activeVoices() <= maxVoices, "voice limit");
 auto s = render(e, 48000);
 verify(finite(s) && peak(s) <= 1.f, "voice stealing must stay finite and below 0 dBFS");
 e.allNotesOff();
 render(e, 48000 * 8);
 verify(e.activeVoices() == 0, "all notes off releases everything");
}

static void clusterAndGlissando() {
 const int sr = 48000;
 Engine e; Values v = pureSine(); v[P::ClusterMode] = 2; v[P::ClusterLevel] = 100; e.setValues(v); e.prepare(sr);
 e.noteOn(69, 1.f);
 auto s = render(e, sr);
 const double root = goertzel(s.l, sr, 440.0, sr / 4, sr), tritone = goertzel(s.l, sr, 622.254, sr / 4, sr);
 const double between = goertzel(s.l, sr, 530.0, sr / 4, sr);
 verify(tritone > root * .5 && tritone > between * 20, "tritone cluster must add 622 Hz");

 v = pureSine(); v[P::PitchEnv] = -12; v[P::PitchTime] = 1000;
 Engine g; g.setValues(v); g.prepare(sr);
 g.noteOn(69, 1.f);
 auto fall = render(g, sr * 2);
 const double end = frequency(fall.l, sr, static_cast<std::size_t>(sr * 1.3), fall.l.size());
 std::printf("glissando end %.2f Hz\n", end);
 verify(std::abs(end - 220.0) < 1.0, "-12 st glissando must land an octave down");

 v = pureSine(); v[P::Glide] = 200;
 Engine h; h.setValues(v); h.prepare(sr);
 h.noteOn(57, 1.f); render(h, sr / 2); h.noteOff(57);
 h.noteOn(69, 1.f);
 auto glide = render(h, sr);
 const double early = frequency(glide.l, sr, 0, static_cast<std::size_t>(sr * .03));
 const double late = frequency(glide.l, sr, sr / 2, glide.l.size());
 std::printf("glide early %.1f late %.1f\n", early, late);
 verify(early < 300.0 && std::abs(late - 440.0) < 1.0, "glide must start from the previous note");
}

static void resonatorAndFreeze() {
 const int sr = 48000;
 Engine e; Values v = presetValues(2); v[P::RevMix] = 0; e.setValues(v); e.prepare(sr);
 e.noteOn(48, 1.f);
 auto bowed = render(e, sr * 2);
 verify(rms(bowed.l, sr, bowed.l.size()) > 1e-3f, "bowed resonator must sound without oscillators");

 v = pureSine(); v[P::RevMix] = 100; v[P::RevDecay] = 2; v[P::Release] = 10;
 Engine f; f.setValues(v); f.prepare(sr);
 f.noteOn(60, 1.f); render(f, sr / 2); f.noteOff(60); render(f, sr / 10);
 v[P::Freeze] = 1; f.setValues(v);
 render(f, sr * 4);
 auto frozen = render(f, sr);
 verify(rms(frozen.l) > 1e-3f, "frozen reverb must keep ringing");
 v[P::Freeze] = 0; v[P::RevDecay] = .5f; f.setValues(v);
 render(f, sr * 4);
 auto gone = render(f, sr);
 verify(rms(gone.l) < 1e-4f, "unfrozen reverb must decay");
}

int main() {
 tuning();
 releaseAndPedal();
 polyphony();
 clusterAndGlissando();
 resonatorAndFreeze();
 std::puts("Abismo engine tests passed");
}
