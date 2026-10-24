// RELICARIO engine checks: each machine plays in tune, behaves like its
// mechanism (tape loops drift, bellows sag under chords, the fork sustains while
// driven, the organ's registers lock to octaves, the broken speaker buzzes),
// every preset stays below 0 dBFS and the engine survives random automation.
// With a directory argument it writes one demo WAV per preset there.
#include "../Source/DSP/RelicarioEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace R = inverso_abismo::relicario;
using R::P::Count;

struct Stereo { std::vector<float> l, r; };

static void verify(bool ok, const char* message) {
 if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::exit(1); }
}
static Stereo render(R::Engine& e, int samples) {
 Stereo out{std::vector<float>(static_cast<std::size_t>(samples)), std::vector<float>(static_cast<std::size_t>(samples))};
 for (int i = 0; i < samples; i += 256) e.render(out.l.data() + i, out.r.data() + i, std::min(256, samples - i));
 return out;
}
static float peak(const Stereo& s) { float p = 0.f; for (std::size_t i = 0; i < s.l.size(); ++i) p = std::max({p, std::abs(s.l[i]), std::abs(s.r[i])}); return p; }
static bool finite(const Stereo& s) { for (std::size_t i = 0; i < s.l.size(); ++i) if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false; return true; }
static float rms(const std::vector<float>& x, std::size_t from, std::size_t to) {
 double sum = 0; to = std::min(to, x.size());
 for (std::size_t i = from; i < to; ++i) sum += static_cast<double>(x[i]) * x[i];
 return to > from ? static_cast<float>(std::sqrt(sum / static_cast<double>(to - from))) : 0.f;
}
static double goertzel(const std::vector<float>& x, double sr, double f, std::size_t from, std::size_t to) {
 const double w = 2.0 * 3.14159265358979 * f / sr, c = 2.0 * std::cos(w);
 double s1 = 0, s2 = 0;
 for (std::size_t i = from; i < to && i < x.size(); ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
 return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / static_cast<double>(to - from);
}
// Strongest frequency near `around` (fine search), for tuning checks.
static double peakFrequency(const std::vector<float>& x, double sr, double around, std::size_t from, std::size_t to) {
 double best = 0, bestF = around;
 for (double f = around * .96; f < around * 1.04; f += around * .0004) {
  const double m = goertzel(x, sr, f, from, to);
  if (m > best) { best = m; bestF = f; }
 }
 return bestF;
}
static void writeWav(const std::string& path, const Stereo& s, int sr) {
 std::ofstream f(path, std::ios::binary);
 auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
 auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
 const auto frames = static_cast<std::uint32_t>(s.l.size());
 f.write("RIFF", 4); u32(36 + frames * 4); f.write("WAVEfmt ", 8);
 u32(16); u16(1); u16(2); u32(static_cast<std::uint32_t>(sr)); u32(static_cast<std::uint32_t>(sr) * 4); u16(4); u16(16);
 f.write("data", 4); u32(frames * 4);
 for (std::size_t i = 0; i < s.l.size(); ++i)
  for (float x : {s.l[i], s.r[i]}) { const auto v = static_cast<std::int16_t>(std::lround(std::clamp(x, -1.f, 1.f) * 32767.f)); f.write(reinterpret_cast<const char*>(&v), 2); }
}

// One machine alone, everything that moves pitch switched off.
static R::Values only(int machineLevel) {
 using namespace R::P;
 R::Values v = R::defaults();
 v[FitaLevel] = v[HarmLevel] = v[ForkLevel] = v[OrganLevel] = 0;
 v[machineLevel] = 100;
 v[Wow] = 0; v[Wear] = 0; v[TremDepth] = 0; v[Beat] = 0; v[Clang] = 0; v[Sag] = 0; v[Motor] = 0; v[Hum] = 0;
 v[RevMix] = 0; v[Dust] = 0; v[Output] = 0; v[Loop1Wave] = 4; v[LoopMix] = 0; v[Breath] = 0; v[Spark] = 0;
 return v;
}

static void tuning() {
 using namespace R::P;
 for (int sr : {44100, 48000, 96000}) {
  for (int machine : {FitaLevel, HarmLevel, ForkLevel, OrganLevel}) {
   R::Values v = only(machine); v[Age] = 0; // a factory-new instrument is exactly in tune
   R::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(69, 1.f);
   auto s = render(e, sr);
   const double f = peakFrequency(s.l, sr, 440.0, static_cast<std::size_t>(sr / 2), static_cast<std::size_t>(sr));
   const double cents = 1200.0 * std::log2(f / 440.0);
   if (sr == 48000) std::printf("machine %d: A4 at %.2f Hz (%+.1f ct)\n", machine, f, cents);
   verify(std::abs(cents) < 3.0, "machine out of tune");
   verify(finite(s) && peak(s) <= 1.f, "machine output invalid");
  }
 }
}

static void mechanisms() {
 using namespace R::P;
 const int sr = 48000;
 // Tape: the loops change the level along their length (wear), so a held note breathes.
 {
  R::Values v = only(FitaLevel); v[Wear] = 100; v[LoopLength] = 2;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, 1.f);
  auto s = render(e, sr * 4);
  float lo = 1e9f, hi = 0.f;
  for (int k = 2; k < 40; ++k) { const float r = rms(s.l, static_cast<std::size_t>(k * sr / 10), static_cast<std::size_t>((k + 1) * sr / 10)); lo = std::min(lo, r); hi = std::max(hi, r); }
  std::printf("tape wear: level wanders %.4f - %.4f\n", lo, hi);
  verify(hi > lo * 1.25f, "worn tape must vary along the loop");
 }
 // Bellows: a big chord drops the pressure; each note gets weaker and flatter.
 {
  R::Values v = only(HarmLevel); v[Sag] = 100;
  auto level = [&](int notes) {
   R::Engine e; e.setValues(v); e.prepare(sr);
   for (int n = 0; n < notes; ++n) e.noteOn(57 + 12 * n, 1.f);
   auto s = render(e, sr * 2);
   return goertzel(s.l, sr, 220.0, sr, sr * 2);
  };
  const double alone = level(1), chord = level(5);
  std::printf("bellows: A3 alone %.4f, inside a 5-note chord %.4f\n", alone, chord);
  verify(chord < alone * .85, "bellows must sag under a chord");
 }
 // Fork: driven, it keeps ringing; undriven it dies away.
 {
  auto tail = [&](float maintain) {
   R::Values v = only(ForkLevel); v[Maintain] = maintain;
   R::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(69, 1.f);
   auto s = render(e, sr * 6);
   return rms(s.l, static_cast<std::size_t>(sr * 5), static_cast<std::size_t>(sr * 6)) / rms(s.l, sr / 2, sr);
  };
  const float driven = tail(100), free = tail(0);
  std::printf("fork after 5 s: driven %.2f, free %.2f of its start\n", driven, free);
  verify(driven > .8f && free < .4f, "electric drive must sustain the fork");
 }
 // Organ: registers are octaves of one oscillator; the fifth adds 3x.
 {
  R::Values v = only(OrganLevel); v[Reg8] = 100; v[Reg4] = 0; v[Upper] = 100; v[UpperInterval] = 1; v[OrganTone] = 100;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, 1.f);
  auto s = render(e, sr);
  const double fifth = goertzel(s.l, sr, 660.0, sr / 4, sr), between = goertzel(s.l, sr, 550.0, sr / 4, sr);
  verify(fifth > between * 10, "upper register at the fifth must add 3x the fundamental");
 }
 // Broken speaker: buzz above 3 kHz grows.
 {
  auto buzz = [&](float broken) {
   R::Values v = only(OrganLevel); v[SpeakerBreak] = broken; v[OrganTone] = 30;
   R::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(45, 1.f);
   auto s = render(e, sr);
   double h = 0; for (double f = 3000; f < 8000; f += 250) h += goertzel(s.l, sr, f, sr / 4, sr);
   return h;
  };
  const double cab = buzz(0), broken = buzz(100);
  std::printf("speaker: high band cabinet %.5f, broken %.5f\n", cab, broken);
  verify(broken > cab * 2, "the broken speaker must buzz");
 }
 // Release: every machine goes silent, hum and hiss fade after playing.
 {
  R::Values v = R::defaults(); v[FitaLevel] = v[HarmLevel] = v[ForkLevel] = v[OrganLevel] = 100; v[Hum] = 100; v[Motor] = 100; v[Dust] = 100; v[RevMix] = 0;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(60, 1.f); render(e, sr); e.noteOff(60);
  render(e, sr * 8);
  auto quiet = render(e, sr);
  std::printf("after release: %d voices, rms %.6f\n", e.activeVoices(), rms(quiet.l, 0, quiet.l.size()));
  verify(e.activeVoices() == 0 && rms(quiet.l, 0, quiet.l.size()) < 2e-4f, "silence after release");
 }
}

// The effects rack: every effect does something audible and stays safe.
static Stereo fxRender(R::Values v, int sr, int seconds = 2) {
 R::Engine e; e.setValues(v); e.prepare(sr);
 e.setTransport(120.0, 0.0, true);
 e.noteOn(57, .9f);
 return render(e, sr * seconds);
}
static float difference(const Stereo& a, const Stereo& b) {
 double sum = 0; for (std::size_t i = 0; i < a.l.size(); ++i) { const double d = a.l[i] - b.l[i]; sum += d * d; }
 return static_cast<float>(std::sqrt(sum / static_cast<double>(a.l.size())));
}
static void effects() {
 using namespace R::P;
 const int sr = 48000;
 R::Values dry = R::defaults();
 dry[RevMix] = 0; dry[Dust] = 0; dry[Wow] = 0; dry[Wear] = 0;
 const auto base = fxRender(dry, sr);
 const float level = rms(base.l, 0, base.l.size());
 struct Case { const char* name; std::vector<std::pair<int, float>> set; };
 const std::vector<Case> cases = {
  {"drive tape", {{Drive, 100}}}, {"drive fuzz", {{Drive, 100}, {DriveType, 1}}}, {"drive fold", {{Drive, 100}, {DriveType, 2}}},
  {"drive rectify", {{Drive, 100}, {DriveType, 3}}}, {"drive shatter", {{Drive, 100}, {DriveType, 4}}},
  {"resample 5000", {{Resample, 4}}}, {"resample 1000 saturated", {{Resample, 6}, {ResampleSat, 100}}},
  {"chop", {{ChopDiv, 3}, {ChopDepth, 100}}}, {"chop free", {{ChopDiv, 3}, {ChopSync, 1}, {ChopMs, 50}, {ChopDepth, 100}}},
  {"chop reverse", {{ChopReverse, 100}}},
  {"low pass", {{FilterMode, 1}, {Cutoff, 400}}}, {"band pass", {{FilterMode, 2}, {Cutoff, 1000}, {Resonance, 60}}},
  {"high pass", {{FilterMode, 3}, {Cutoff, 2000}}}, {"filter env", {{FilterMode, 1}, {Cutoff, 200}, {FilterEnv, 100}, {Resonance, 80}}},
  {"filter lfo", {{FilterMode, 1}, {Cutoff, 500}, {FilterLfo, 100}, {FilterLfoRate, 3}}},
  {"ring", {{RingMix, 100}, {RingFreq, 330}}},
  {"chorus", {{Ensemble, 100}, {ChorusRate, 2}, {ChorusDepth, 100}}},
  {"flanger", {{FlangerMix, 100}, {FlangerFeedback, 90}}}, {"flanger negative", {{FlangerMix, 100}, {FlangerFeedback, -90}}},
  {"phaser", {{PhaserMix, 100}, {PhaserFeedback, 90}}},
  {"tremolo", {{TremoloDepth, 100}}}, {"tremolo square", {{TremoloDepth, 100}, {TremoloShape, 1}}}, {"tremolo pan", {{TremoloDepth, 100}, {TremoloShape, 2}}},
  {"vibrato", {{VibratoDepth, 100}}},
  {"echo synced", {{EchoMix, 60}, {EchoSync, 1}}}, {"echo dark", {{EchoMix, 60}, {EchoTone, 0}}},
  {"reverb dark", {{RevMix, 50}, {RevDamp, 100}}},
 };
 for (const auto& c : cases) {
  auto v = dry;
  for (const auto& [p, value] : c.set) v[static_cast<std::size_t>(p)] = value;
  const auto out = fxRender(v, sr);
  const float d = difference(out, base);
  verify(finite(out) && peak(out) <= 1.f, "effect output invalid");
  if (d < level * .05f) { std::fprintf(stderr, "%s changed nothing (%.5f vs %.5f)\n", c.name, d, level); std::exit(1); }
 }
 // Resonant extremes at every cutoff stay finite and under the ceiling.
 for (float hz : {20.f, 200.f, 2000.f, 12000.f, 20000.f})
  for (int mode : {1, 2, 3}) {
   auto v = dry; v[FilterMode] = static_cast<float>(mode); v[Cutoff] = hz; v[Resonance] = 100; v[FilterEnv] = 100; v[Drive] = 100;
   const auto out = fxRender(v, sr, 1);
   verify(finite(out) && peak(out) <= 1.f, "resonant filter blew up");
  }
 // The low pass really darkens: 3.5 kHz band drops by more than 20 dB.
 {
  auto v = dry; v[FilterMode] = 1; v[Cutoff] = 300;
  const auto out = fxRender(v, sr);
  const double open = goertzel(base.l, sr, 3520.0, static_cast<std::size_t>(sr / 2), static_cast<std::size_t>(sr * 3 / 2));
  const double shut = goertzel(out.l, sr, 3520.0, static_cast<std::size_t>(sr / 2), static_cast<std::size_t>(sr * 3 / 2));
  verify(shut < open * .1, "low pass does not cut");
 }
 // Chop at 1/16 and 120 BPM (125 ms steps) gates: many quiet stretches.
 {
  auto v = dry; v[ChopDiv] = 3; v[ChopDepth] = 100;
  const auto out = fxRender(v, sr);
  int quiet = 0, windows = 0;
  for (std::size_t i = static_cast<std::size_t>(sr / 2); i + 96 < out.l.size(); i += 96, ++windows)
   if (rms(out.l, i, i + 96) < level * .1f) ++quiet;
  verify(quiet > windows / 4, "chop does not gate");
 }
 // Synced echo at 120 BPM, 1/4: the repeat lands 500 ms after a short note.
 {
  auto v = dry; v[EchoMix] = 80; v[EchoSync] = 1; v[EchoFeedback] = 0; v[Release] = 5; v[Attack] = 1;
  R::Engine e; e.setValues(v); e.prepare(sr); e.setTransport(120.0, 0.0, true);
  e.noteOn(57, 1.f); auto a = render(e, sr / 20); e.noteOff(57); auto b = render(e, sr);
  const float early = rms(b.l, static_cast<std::size_t>(sr * .2), static_cast<std::size_t>(sr * .4));
  const float echo = rms(b.l, static_cast<std::size_t>(sr * .45), static_cast<std::size_t>(sr * .5));
  verify(echo > early * 5.f && echo > 1e-3f, "synced echo not at 1/4");
 }
 std::puts("effects rack: pass");
}

static void stress() {
 for (int sr : {44100, 96000}) {
  R::Engine e; e.setValues(R::defaults()); e.prepare(sr);
  std::srand(5);
  std::vector<float> l(512), r(512);
  for (int block = 0; block < 1500; ++block) {
   if (block % 7 == 0) {
    R::Values v{};
    for (int p = 0; p < Count; ++p) {
     const auto& s = R::specs()[static_cast<std::size_t>(p)];
     v[static_cast<std::size_t>(p)] = s.min + (s.max - s.min) * static_cast<float>(std::rand()) / RAND_MAX;
     if (s.kind != inverso_abismo::Kind::Float) v[static_cast<std::size_t>(p)] = std::round(v[static_cast<std::size_t>(p)]);
    }
    v[R::P::Output] = 0;
    e.setValues(v);
   }
   if (block % 5 == 0) e.noteOn(24 + std::rand() % 80, .2f + .8f * static_cast<float>(std::rand()) / RAND_MAX);
   if (block % 9 == 0) e.noteOff(24 + std::rand() % 80);
   if (block % 13 == 0) e.setPitchBend(static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f);
   e.render(l.data(), r.data(), 512);
   for (int i = 0; i < 512; ++i) verify(std::isfinite(l[static_cast<std::size_t>(i)]) && std::abs(l[static_cast<std::size_t>(i)]) <= 1.f, "automation output invalid");
  }
  std::printf("%d Hz randomised automation: pass\n", sr);
 }
}

static void presetDemos(const std::string& dir) {
 const int sr = 48000;
 for (std::size_t i = 0; i < R::presets().size(); ++i) {
  R::Engine e; e.setValues(R::presetValues(i)); e.prepare(sr);
  Stereo all;
  auto append = [&](const Stereo& s) { all.l.insert(all.l.end(), s.l.begin(), s.l.end()); all.r.insert(all.r.end(), s.r.begin(), s.r.end()); };
  for (int n : {48, 55, 60, 63}) e.noteOn(n, .85f);
  append(render(e, sr * 4));
  for (int n : {48, 55, 60, 63}) e.noteOff(n);
  append(render(e, sr));
  const int melody[] = {67, 70, 72, 75, 74, 70};
  for (int n : melody) { e.noteOn(n, .8f); append(render(e, sr * 2 / 3)); e.noteOff(n); }
  append(render(e, sr * 4));
  const float p = peak(all), level = rms(all.l, 0, static_cast<std::size_t>(sr * 4));
  std::printf("%-28s peak %.3f  rms %.4f\n", R::presets()[i].name, p, level);
  verify(finite(all) && p <= 1.f, "preset output invalid");
  verify(level > 2e-3f, "preset too quiet");
  if (!dir.empty()) {
   std::string name = std::to_string(i) + "_" + R::presets()[i].name + ".wav";
   for (auto& ch : name) if (ch == ' ') ch = '_';
   writeWav(dir + "/relicario_" + name, all, sr);
  }
 }
}

// 0.14: what the player heard and reported, each checked.
static double harmonicShare(const std::vector<float>& x, double sr, double f0, std::size_t from, std::size_t to) {
 // energy at the harmonics of f0 against energy halfway between them (up to 5 kHz)
 double harm = 0, between = 0;
 for (int k = 1; f0 * (k + .5) < 5000; ++k) { harm += goertzel(x, sr, f0 * k, from, to); between += goertzel(x, sr, f0 * (k + .5), from, to); }
 return harm / (harm + between + 1e-30);
}
static float clickiness(const std::vector<float>& x, std::size_t from, std::size_t to) {
 // the largest second difference against the signal's typical one
 double typical = 0; float worst = 0;
 for (std::size_t i = from + 2; i < to; ++i) {
  const float d = std::abs(x[i] - 2 * x[i - 1] + x[i - 2]);
  typical += d; worst = std::max(worst, d);
 }
 typical /= static_cast<double>(to - from - 2);
 return static_cast<float>(worst / (typical + 1e-9));
}
static void feedback014() {
 using namespace R::P;
 const int sr = 48000;
 // Chop: switching its settings while it plays clicks no more than the chop
 // already does with its settings left alone (its gate opens in 1.5 ms).
 {
  auto run = [&](bool changing) {
   R::Values v = only(FitaLevel); v[Loop1Wave] = 4; v[ChopDiv] = 3; v[ChopDepth] = 40; v[ChopChaos] = 80; v[ChopReverse] = 50;
   R::Engine e; e.setValues(v); e.prepare(sr); e.setTransport(120.0, 0.0, true);
   e.noteOn(57, .9f);
   std::vector<float> out;
   for (int k = 0; k < 40; ++k) {
    if (changing) { v[ChopDiv] = static_cast<float>(1 + k % 6); v[ChopSync] = static_cast<float>(k % 3 == 0); v[ChopMs] = 60.f + 37.f * static_cast<float>(k % 7); }
    e.setValues(v);
    auto s = render(e, sr / 10);
    out.insert(out.end(), s.l.begin(), s.l.end());
   }
   float worst = 0;
   for (std::size_t i = static_cast<std::size_t>(sr / 5) + 2; i < out.size(); ++i) worst = std::max(worst, std::abs(out[i] - 2 * out[i - 1] + out[i - 2]));
   return worst;
  };
  const float still = run(false), moving = run(true);
  std::printf("chop: worst jump %.5f changing settings, %.5f left alone\n", moving, still);
  verify(moving < still * 2.f, "the chop clicks when its settings change");
 }
 // Drive: switching the type and easing it in from zero click no more than
 // the sharpest type does on its own.
 {
  auto worstJump = [&](const std::vector<float>& out) {
   float worst = 0;
   for (std::size_t i = static_cast<std::size_t>(sr / 5) + 2; i < out.size(); ++i) worst = std::max(worst, std::abs(out[i] - 2 * out[i - 1] + out[i - 2]));
   return worst;
  };
  auto run = [&](int fixedType) {
   R::Values v = only(FitaLevel); v[Loop1Wave] = 4; v[Drive] = 0;
   R::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(57, .9f);
   std::vector<float> out;
   for (int k = 0; k < 30; ++k) {
    v[Drive] = fixedType >= 0 ? 60.f : k < 5 ? static_cast<float>(k) : 60.f;
    v[DriveType] = static_cast<float>(fixedType >= 0 ? fixedType : k % 5);
    e.setValues(v);
    auto s = render(e, sr / 10);
    out.insert(out.end(), s.l.begin(), s.l.end());
   }
   return worstJump(out);
  };
  float sharpest = 0;
  for (int t = 0; t < 5; ++t) sharpest = std::max(sharpest, run(t));
  const float moving = run(-1);
  std::printf("drive: worst jump %.5f changing type, %.5f sharpest type held\n", moving, sharpest);
  verify(moving < sharpest * 1.5f, "the drive clicks when its type changes");
 }
 // Tape drive warms instead of raising the hiss: hiss over the note grows little.
 {
  auto hissRatio = [&](float drive) {
   R::Values v = only(FitaLevel); v[Loop1Wave] = 4; v[Wear] = 100; v[DriveType] = 0; v[Drive] = drive;
   R::Engine e; e.setValues(v); e.prepare(sr);
   e.noteOn(45, .5f);
   auto s = render(e, sr * 2);
   double hiss = 0; for (double f = 6000; f < 12000; f += 500) hiss += goertzel(s.l, sr, f, sr / 2, sr * 2);
   return hiss / goertzel(s.l, sr, 110, sr / 2, sr * 2);
  };
  const double clean = hissRatio(0), driven = hissRatio(100);
  std::printf("tape drive: hiss against the note x%.2f\n", driven / clean);
  verify(driven < clean * 1.5, "tape drive brings the hiss up");
 }
 // Rectify keeps the note; shatter never drops it to silence.
 {
  R::Values v = only(FitaLevel); v[Loop1Wave] = 4; v[DriveType] = 3; v[Drive] = 60;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, .9f);
  auto s = render(e, sr);
  const double fund = goertzel(s.l, sr, 220, sr / 4, sr), oct = goertzel(s.l, sr, 440, sr / 4, sr);
  std::printf("rectify: fundamental %.2f of the octave\n", fund / oct);
  verify(fund > oct * .25, "rectify loses the note");
  v[DriveType] = 4; v[Drive] = 100;
  R::Engine e2; e2.setValues(v); e2.prepare(sr);
  e2.noteOn(57, .9f);
  auto t = render(e2, sr * 3);
  const float whole = rms(t.l, sr / 2, sr * 3);
  float least = 1e9f;
  for (int w = sr / 2; w + sr / 50 < sr * 3; w += sr / 100) least = std::min(least, rms(t.l, static_cast<std::size_t>(w), static_cast<std::size_t>(w + sr / 50)));
  std::printf("shatter: quietest 20 ms at %.2f of the average\n", least / whole);
  verify(least > whole * .15f, "shatter makes the note disappear");
 }
 // Bit crush: quiet passages do not turn into hiss.
 {
  R::Values v = only(FitaLevel); v[Loop1Wave] = 4; v[Resample] = 5; v[ResampleSat] = 100; v[FitaRelease] = 3000;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, .9f); render(e, sr / 2); e.noteOff(57);
  auto s = render(e, sr * 2);
  const double share = harmonicShare(s.l, sr, 220, sr, sr * 2); // the quiet tail of the note
  std::printf("resample: tail %.0f%% harmonic\n", share * 100);
  verify(share > .7, "resample turns quiet sound into noise");
 }
 // Broken speaker: faults that follow the note (harmonic), not hiss.
 {
  R::Values v = only(OrganLevel); v[SpeakerBreak] = 100; v[OrganTone] = 50; v[Reg16] = 0; v[Reg8] = 100; v[Reg4] = 50;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(45, 1.f);
  auto s = render(e, sr);
  const double share = harmonicShare(s.l, sr, 110, sr / 4, sr);
  std::printf("broken speaker: %.0f%% harmonic\n", share * 100);
  verify(share > .85, "the broken speaker is mostly noise");
 }
 // Fork spark: a buzz at the fork's pitch, not noise; clang: the bar's metal modes.
 {
  R::Values v = only(ForkLevel); v[Spark] = 100; v[Maintain] = 100;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, 1.f);
  auto s = render(e, sr);
  const double share = harmonicShare(s.l, sr, 220, sr / 4, sr);
  std::printf("spark: %.0f%% harmonic\n", share * 100);
  verify(share > .85, "the spark is mostly noise");
  v[Spark] = 0; v[Clang] = 100;
  R::Engine e2; e2.setValues(v); e2.prepare(sr);
  e2.noteOn(57, 1.f);
  auto c = render(e2, sr / 4);
  const double mode = goertzel(c.l, sr, 220 * 6.267, 0, sr / 8), mode3 = goertzel(c.l, sr, 220 * 17.55, 0, sr / 16);
  const double harmonic6 = goertzel(c.l, sr, 220 * 6, 0, sr / 8);
  std::printf("clang: 6.27x %.4f, 6x %.4f, 17.55x %.4f\n", mode, harmonic6, mode3);
  verify(mode > harmonic6 * 4 && mode3 > 0, "the clang is not the metal's own modes");
 }
 // Machine envelopes, on/off and the loop levels do what they say.
 {
  R::Values v = only(FitaLevel); v[FitaAttack] = 2000;
  R::Engine e; e.setValues(v); e.prepare(sr);
  e.noteOn(57, 1.f);
  auto s = render(e, sr * 3);
  const float early = rms(s.l, sr / 20, sr / 10), late = rms(s.l, sr * 5 / 2, sr * 3);
  verify(late > early * 5, "the tape's own attack does nothing");
  // a machine's release is heard as set, past the old shared envelope
  {
   R::Values r = only(FitaLevel); r[FitaRelease] = 4000;
   R::Engine er; er.setValues(r); er.prepare(sr);
   er.noteOn(57, 1.f); render(er, sr / 2); er.noteOff(57);
   auto tail = render(er, sr * 2);
   const float held = rms(tail.l, sr, sr * 5 / 4);
   r[Release] = 50; // the master ENVELOPE closes everything
   R::Engine em; em.setValues(r); em.prepare(sr);
   em.noteOn(57, 1.f); render(em, sr / 2); em.noteOff(57);
   auto cut = render(em, sr * 2);
   std::printf("tape release 4 s: %.4f after 1 s; with the master release at 50 ms: %.6f\n", held, rms(cut.l, sr, sr * 5 / 4));
   verify(held > 1e-3f, "the tape's own release does nothing");
   verify(rms(cut.l, sr, sr * 5 / 4) < held * .01f, "the master envelope does not close the machines");
  }
  v[FitaAttack] = 1; v[FitaOn] = 0;
  R::Engine e2; e2.setValues(v); e2.prepare(sr);
  e2.noteOn(57, 1.f);
  auto off = render(e2, sr);
  verify(rms(off.l, sr / 2, sr) < 1e-5f, "a machine switched off still plays");
  v[FitaOn] = 1; v[LoopMix] = 50; v[Loop2Wave] = 4; v[Loop1Level] = 0; v[Loop2Level] = 0;
  R::Engine e3; e3.setValues(v); e3.prepare(sr);
  e3.noteOn(57, 1.f);
  auto none = render(e3, sr);
  verify(rms(none.l, sr / 2, sr) < 1e-4f, "loop levels at zero still play");
  std::puts("machine envelopes, on/off, loop levels: fine");
 }
}

int main(int argc, char** argv) {
 tuning();
 mechanisms();
 effects();
 feedback014();
 presetDemos(argc > 1 ? argv[1] : "");
 stress();
 std::puts("Relicario tests passed");
}
