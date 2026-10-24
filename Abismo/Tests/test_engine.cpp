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

static void voiceAndMutilation() {
 const int sr = 48000;
 // Vowel A on a low saw: energy near F1 (600 Hz) must dominate energy near 1700 Hz;
 // vowel I reverses that relation (F1 250 Hz, F2 1750 Hz).
 auto vowelBalance = [&](float vowel) {
  Values v = pureSine(); v[P::OscShape] = 66; v[P::VoiceMix] = 100; v[P::Vowel] = vowel;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(36, 1.f);
  auto s = render(e, sr);
  double low = 0, high = 0;
  for (int h = 1; h < 60; ++h) {
   const double f = 65.406 * h;
   const double m = goertzel(s.l, sr, f, sr / 4, sr);
   if (f > 450 && f < 750) low += m;
   if (f > 1550 && f < 1950) high += m;
  }
  return low / high;
 };
 const double a = vowelBalance(0), i = vowelBalance(50);
 std::printf("formant F1/F2 energy ratio: A %.2f, I %.2f\n", a, i);
 verify(a > 2.0 * i, "vowel morph must move the formants");

 // 1/16 stutter at 120 BPM, full depth, no chaos: the end of each 125 ms step is gated.
 Values v = pureSine(); v[P::StutterDiv] = 3; v[P::StutterDepth] = 100; v[P::Chaos] = 0;
 Engine e; e.setValues(v); e.prepare(sr);
 e.setTransport(120.0, 0.0, true);
 e.noteOn(69, 1.f);
 auto s = render(e, sr * 2);
 const auto step = static_cast<std::size_t>(sr * .125);
 double open = 0, closed = 0;
 for (std::size_t k = 4; k < 14; ++k) {
  open += rms(s.l, k * step + step / 20, k * step + step / 8);
  closed += rms(s.l, k * step + step / 2, k * step + step - step / 10);
 }
 std::printf("stutter open %.4f closed %.6f\n", open, closed);
 verify(open > .1 && closed < open * .01, "stutter gate must follow the 1/16 grid");

 // Chaos must change the pattern between steps without breaking the output.
 v[P::Chaos] = 100; e.setValues(v);
 auto chaotic = render(e, sr * 2);
 verify(finite(chaotic) && peak(chaotic) <= 1.f, "chaotic stutter must stay finite");

 // Stutter off again: gain must recover fully.
 v[P::StutterDiv] = 0; e.setValues(v);
 render(e, sr / 2);
 auto recovered = render(e, sr / 2);
 verify(rms(recovered.l) > .5f * rms(s.l, 0, step / 8), "audio must return when stutter is switched off");

 // Crush at maximum quantises to very few steps (counted against the sound's
 // own level since 0.14): far from the clean sine, and silent in silence.
 v = pureSine(); v[P::Crush] = 100;
 Engine c; c.setValues(v); c.prepare(sr);
 c.noteOn(69, 1.f);
 auto crushed = render(c, sr / 2);
 v[P::Crush] = 0;
 Engine clean; clean.setValues(v); clean.prepare(sr);
 clean.noteOn(69, 1.f);
 auto smooth = render(clean, sr / 2);
 double err = 0, sig = 0;
 for (std::size_t n = sr / 4; n < crushed.l.size(); ++n) { const double d = crushed.l[n] - smooth.l[n]; err += d * d; sig += static_cast<double>(smooth.l[n]) * smooth.l[n]; }
 std::printf("crush error: %.1f dB below the signal\n", 10 * std::log10(err / sig));
 verify(err > sig * .004, "crush must quantise the signal"); // 3 bits: about 20 dB
}

// A struck resonator alone (no oscillator, no reverb): its spectrum is the material.
static Values resonatorOnly(int material) {
 Values v = pureSine(); v[P::OscLevel] = 0; v[P::ResLevel] = 100; v[P::ResMaterial] = static_cast<float>(material);
 v[P::Bow] = 0; v[P::Strike] = 100; v[P::ResDecay] = 3; v[P::Sustain] = 100; v[P::Cutoff] = 20000; v[P::FilterEnv] = 0;
 v[P::ResStereo] = 0;
 return v;
}

static void deeperResonator() {
 const int sr = 48000;
 // Every material, alone and morphed, struck or fed by the oscillators at full
 // input with a long decay: audible, finite, below 0 dBFS.
 for (int m = 0; m < numMaterials; ++m) {
  for (int mode = 0; mode < 3; ++mode) {
   Values v = resonatorOnly(m);
   if (mode == 1) { v[P::ResMaterialB] = static_cast<float>((m + 7) % numMaterials); v[P::ResMorph] = 50; v[P::ResStretch] = 60; }
   if (mode == 2) { v[P::OscLevel] = 70; v[P::ResInput] = 100; v[P::ResDecay] = 15; v[P::ResRattle] = 100; v[P::Bow] = 100; }
   Engine e; e.setValues(v); e.prepare(sr);
   for (int n : {36, 60, 84}) e.noteOn(n, 1.f);
   auto s = render(e, sr * 2);
   verify(finite(s) && peak(s) <= 1.f, "resonator material must stay finite and below 0 dBFS");
   verify(rms(s.l, sr / 10, sr) > 1e-3f, "resonator material must sound");
  }
 }
 // Pipe: odd harmonics only. A4 struck: 1320 Hz strong, 880 Hz absent.
 {
  Values v = resonatorOnly(7); v[P::ResStretch] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(69, 1.f);
  auto s = render(e, sr);
  const double third = goertzel(s.l, sr, 1320.0, sr / 20, sr / 2), second = goertzel(s.l, sr, 880.0, sr / 20, sr / 2);
  std::printf("pipe: 3rd harmonic %.5f, 2nd %.6f\n", third, second);
  verify(third > second * 10, "pipe material must ring on odd harmonics");
 }
 // Tune +12 doubles the resonator; keytrack 0 pins it near middle C whatever the note.
 auto lowestPeak = [&](Values v, int note) {
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(note, 1.f);
  auto s = render(e, sr);
  double best = 0, bestF = 0;
  for (double f = 60; f < 2000; f *= 1.005) {
   const double m = goertzel(s.l, sr, f, sr / 20, sr / 2);
   if (m > best) { best = m; bestF = f; }
  }
  return bestF;
 };
 Values str = resonatorOnly(0);
 const double base = lowestPeak(str, 57);
 str[P::ResTune] = 12;
 const double up = lowestPeak(str, 57);
 std::printf("resonator A3 %.1f Hz, tuned +12 %.1f Hz\n", base, up);
 verify(std::abs(base - 220.0) < 4 && std::abs(up - 440.0) < 8, "resonator tune must move the partials");
 str = resonatorOnly(0); str[P::ResKeytrack] = 0;
 const double fixedLow = lowestPeak(str, 40), fixedHigh = lowestPeak(str, 76);
 std::printf("keytrack 0: note E2 %.1f Hz, note E5 %.1f Hz\n", fixedLow, fixedHigh);
 verify(std::abs(fixedLow - 261.6) < 5 && std::abs(fixedHigh - 261.6) < 5, "keytrack 0 must pin the resonator");
 // Rattle adds a buzz above the body.
 auto hiss = [&](float rattle) {
  Values v = resonatorOnly(2); v[P::ResRattle] = rattle; v[P::Strike] = 100;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(45, 1.f);
  auto s = render(e, sr);
  return goertzel(s.l, sr, 7900.0, sr / 50, sr / 3) + goertzel(s.l, sr, 9100.0, sr / 50, sr / 3);
 };
 const double dry = hiss(0), buzzing = hiss(100);
 std::printf("rattle: high band %.6f -> %.6f\n", dry, buzzing);
 verify(buzzing > dry * 3, "rattle must buzz");
}

static void destruction() {
 const int sr = 48000;
 // Every drive type at full drive: finite, below 0 dBFS, and not all alike.
 std::vector<double> thirds;
 for (int mode = 0; mode < 5; ++mode) {
  Values v = pureSine(); v[P::Drive] = 100; v[P::DriveMode] = static_cast<float>(mode); v[P::Cutoff] = 20000; v[P::FilterEnv] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, 1.f);
  auto s = render(e, sr);
  verify(finite(s) && peak(s) <= 1.f, "drive type must stay finite");
  verify(rms(s.l, sr / 4, sr) > 1e-2f, "drive type must sound");
  thirds.push_back(goertzel(s.l, sr, 440.0, sr / 4, sr) / (goertzel(s.l, sr, 220.0, sr / 4, sr) + 1e-9));
 }
 std::printf("octave/fundamental by drive type: %.2f %.2f %.2f %.2f %.2f\n", thirds[0], thirds[1], thirds[2], thirds[3], thirds[4]);
 verify(thirds[3] > 3 * thirds[0], "rectifier must jump an octave");
 // Ring modulator: A4 against a carrier a fifth up gives 219 Hz and 1099 Hz sidebands.
 {
  Values v = pureSine(); v[P::RingMix] = 100; v[P::RingPitch] = 7; v[P::Cutoff] = 20000; v[P::FilterEnv] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(69, 1.f);
  auto s = render(e, sr);
  const double lo = goertzel(s.l, sr, 219.3, sr / 4, sr), hi = goertzel(s.l, sr, 1099.3, sr / 4, sr), dry = goertzel(s.l, sr, 440.0, sr / 4, sr);
  std::printf("ring: 219 %.4f 1099 %.4f 440 %.5f\n", lo, hi, dry);
  verify(lo > dry * 10 && hi > dry * 10, "ring modulator must make sidebands");
 }
 // Reverse slices: a rising tone played back falls. Glissando up over 2 s on a
 // 1/8 grid at 120 BPM, every step reversed: the pitch inside a step goes down.
 {
  Values v = pureSine(); v[P::Reverse] = 100; v[P::PitchEnv] = 12; v[P::PitchTime] = 2000; v[P::Cutoff] = 20000; v[P::FilterEnv] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.setTransport(120.0, 0.0, true);
  e.noteOn(57, 1.f);
  auto s = render(e, sr * 2);
  verify(finite(s) && peak(s) <= 1.f, "reverse must stay finite");
  const auto step = static_cast<std::size_t>(sr / 4);
  int falling = 0;
  for (std::size_t k = 1; k < 7; ++k) {
   const double a = frequency(s.l, sr, k * step + step / 10, k * step + step / 2);
   const double b = frequency(s.l, sr, k * step + step / 2, k * step + step - step / 10);
   if (b < a) ++falling;
  }
  std::printf("reverse: %d of 6 steps fall in pitch\n", falling);
  verify(falling >= 5, "reversed steps must play backwards");
 }
 // Free stutter time: a 200 ms gate is the same at 60 and at 180 BPM.
 for (double bpm : {60.0, 180.0}) {
  Values v = pureSine(); v[P::StutterDiv] = 1; v[P::StutterSync] = 1; v[P::StutterMs] = 200; v[P::StutterDepth] = 100; v[P::Chaos] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.setTransport(bpm, 0.0, true);
  e.noteOn(69, 1.f);
  auto s = render(e, sr * 2);
  const auto step = static_cast<std::size_t>(sr / 5);
  double open = 0, closed = 0;
  for (std::size_t k = 1; k < 9; ++k) {
   open += rms(s.l, k * step + step / 20, k * step + step / 8);
   closed += rms(s.l, k * step + step / 2, k * step + step - step / 10);
  }
  std::printf("free stutter 200 ms at %.0f BPM: open %.3f closed %.5f\n", bpm, open, closed);
  verify(open > .1 && closed < open * .01, "free stutter must ignore the tempo");
 }
 // Reverse reverb: a short note's tail swells instead of decaying.
 {
  Values v = pureSine(); v[P::RevMix] = 100; v[P::RevReverse] = 100; v[P::RevDecay] = 1.5f; v[P::Release] = 30; v[P::Decay] = 60; v[P::Sustain] = 0;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(69, 1.f);
  auto s = render(e, sr);
  std::size_t loudest = 0; float best = 0;
  for (std::size_t i = 0; i + 480 < s.l.size(); i += 480) {
   const float r = rms(s.l, i, i + 480);
   if (r > best) { best = r; loudest = i; }
  }
  std::printf("reverse reverb: loudest at %.0f ms\n", 1000.0 * static_cast<double>(loudest) / sr);
  verify(finite(s) && loudest > static_cast<std::size_t>(sr / 10), "reverse reverb must swell after the note");
 }
 // Tape warp: the pitch of a steady A4 wanders.
 {
  Values v = pureSine(); v[P::Warp] = 100;
  Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(69, 1.f);
  auto s = render(e, sr * 3);
  verify(finite(s) && peak(s) <= 1.f, "warp must stay finite");
  double lo = 1e9, hi = 0;
  for (std::size_t k = 2; k < 30; ++k) {
   const double f = frequency(s.l, sr, k * sr / 10, (k + 1) * sr / 10);
   lo = std::min(lo, f); hi = std::max(hi, f);
  }
  std::printf("warp: A4 wanders %.1f - %.1f Hz\n", lo, hi);
  verify(hi - lo > 4, "warp must bend the pitch");
 }
}

int main() {
 tuning();
 releaseAndPedal();
 polyphony();
 clusterAndGlissando();
 resonatorAndFreeze();
 voiceAndMutilation();
 deeperResonator();
 destruction();
 std::puts("Abismo engine tests passed");
}
