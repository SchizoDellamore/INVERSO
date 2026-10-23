#pragma once
// ENKI generator: invents whole sounds. GENERATE draws a new sound for the
// chosen category; MUTATE moves the current one by an amount; locked sections
// are left alone. Every result is levelled by rendering a short test chord.
#include "EnkiEngine.h"
#include <cmath>
#include <cstdint>

namespace inverso_abismo::enki {

enum Category { Musical, Textures, Hits, Rhythmic, Weird };
enum SourceType { Wave, FM, Noise, SampleSrc, Grains, ResonatorSrc, Reed };

class Generator {
public:
 explicit Generator(std::uint32_t seed = 1u) { rng.state = seed * 2654435761u + 7u; }
 void reseed(std::uint32_t seed) { rng.state = seed * 2654435761u + 7u; }

 // A brand new sound. `userSlots` is a bit mask of the user sample slots that hold a recording.
 Values generate(const Values& current, std::uint32_t userSlots) {
  const int category = std::clamp(static_cast<int>(std::lround(current[P::Category])), 0, 4);
  Values v = defaults();
  for (int p = P::Category; p < P::Count; ++p) v[static_cast<std::size_t>(p)] = current[static_cast<std::size_t>(p)];
  v[P::PressureCC] = current[P::PressureCC];
  slots = userSlots;
  switch (category) {
   case Musical: musical(v); break;
   case Textures: textures(v); break;
   case Hits: hits(v); break;
   case Rhythmic: rhythmic(v); break;
   default: weird(v); break;
  }
  keepLocked(v, current);
  return v;
 }

 // A variation: every unlocked parameter moves by up to `amount` (0..1) of its range.
 Values mutate(const Values& current, float amount, std::uint32_t userSlots) {
  slots = userSlots;
  Values v = current;
  const auto& table = specs();
  for (int p = 0; p < P::Category; ++p) {
   if (p == P::PressureCC || p == P::Output || p == P::BendRange || p == P::VelSens || p == P::Width) continue;
   const auto& s = table[static_cast<std::size_t>(p)];
   auto& x = v[static_cast<std::size_t>(p)];
   if (s.kind == Kind::Float) {
    // Times and frequencies move on a log scale, the rest linearly.
    if (s.centre > 0.f && s.min > 0.f) x = std::exp(std::clamp(std::log(x) + amount * 1.5f * rng.next(), std::log(s.min), std::log(s.max)));
    else x = std::clamp(x + amount * .5f * (s.max - s.min) * rng.next(), s.min, s.max);
   } else if (u() < amount * .35f) {
    if (p == P::ASample || p == P::BSample) x = static_cast<float>(pickSample());
    else if (s.kind == Kind::Int) x = std::clamp(std::round(x + amount * .3f * (s.max - s.min) * rng.next()), s.min, s.max);
    else x = std::round(s.min + u() * (s.max - s.min));
   }
  }
  keepLocked(v, current);
  return v;
 }

 // Sets OUTPUT so a test chord sits near -20 dBFS RMS with peaks under -3 dBFS
 // (generated sounds differ by 40 dB otherwise). The chord plays long enough
 // for slow attacks; loudness is measured once the sound has arrived. Returns
 // false if the sound is too quiet to bring up (the caller draws again).
 static bool level(Values& v, const SamplePool& pool, double sr = 24000.0) {
  Engine e;
  for (int i = 0; i < numSamples; ++i) e.setSample(i, pool[static_cast<std::size_t>(i)]);
  Values test = v;
  test[P::Output] = 0.f;
  e.setValues(test);
  e.prepare(sr);
  e.setTransport(120.0, 0.0, true);
  for (int n : {48, 55, 60}) e.noteOn(n, .8f);
  const double attack = v[P::Attack] * .001;
  const double seconds = std::clamp(attack * 1.2 + 1.2, 1.6, 6.0);
  const double from = v[P::Hold] >= .5f ? .0 : std::min(seconds * .6, std::max(.05, attack * .8));
  const int total = static_cast<int>(sr * seconds);
  std::vector<float> l(256), r(256);
  double sum = 0.0, peakAll = 0.0;
  long count = 0;
  for (int done = 0; done < total; done += 256) {
   e.render(l.data(), r.data(), 256);
   for (int i = 0; i < 256; ++i) {
    const double x = std::max(std::abs(l[static_cast<std::size_t>(i)]), std::abs(r[static_cast<std::size_t>(i)]));
    peakAll = std::max(peakAll, x);
    if (done + i > sr * from) { sum += x * x; ++count; }
   }
  }
  const double rms = std::sqrt(sum / std::max(1L, count));
  const double rmsDb = rms > 1e-7 ? 20.0 * std::log10(rms) : -140.0;
  const double peakDb = peakAll > 1e-7 ? 20.0 * std::log10(peakAll) : -140.0;
  const double gain = std::clamp(std::min(-20.0 - rmsDb, -3.0 - peakDb), -36.0, 6.0);
  v[P::Output] = static_cast<float>(gain);
  return rmsDb + gain > -32.0;
 }

 // GENERATE as the plugin uses it: drawn, levelled, and drawn again if inaudible.
 Values generateLevelled(const Values& current, std::uint32_t userSlots, const SamplePool& pool) {
  Values v = generate(current, userSlots);
  for (int tries = 0; tries < 8 && !level(v, pool); ++tries) v = generate(current, userSlots);
  return v;
 }
 Values mutateLevelled(const Values& current, float amount, std::uint32_t userSlots, const SamplePool& pool) {
  Values v = mutate(current, amount, userSlots);
  for (int tries = 0; tries < 8 && !level(v, pool); ++tries) v = mutate(current, amount, userSlots);
  return v;
 }

private:
 float u() { return .5f * (rng.next() + 1.f); }                 // 0..1
 float range(float a, float b) { return a + (b - a) * u(); }
 float logRange(float a, float b) { return std::exp(range(std::log(a), std::log(b))); }
 template <int N> int pick(const int (&items)[N]) { return items[std::min(N - 1, static_cast<int>(u() * N))]; }
 template <int N> float pickf(const float (&items)[N]) { return items[std::min(N - 1, static_cast<int>(u() * N))]; }
 bool chance(float p) { return u() < p; }

 // Library recordings, or the user's own (loaded or downloaded) about a third of the time.
 int pickSample() {
  int used[numUserSlots]; int n = 0;
  for (int i = 0; i < numUserSlots; ++i) if (slots & (1u << i)) used[n++] = numLibrarySamples + i;
  if (n > 0 && chance(.4f)) return used[std::min(n - 1, static_cast<int>(u() * static_cast<float>(n)))];
  return std::min(numLibrarySamples - 1, static_cast<int>(u() * numLibrarySamples));
 }

 void source(Values& v, int k, int type, float level, float pitch) {
  const int b = k * P::SourceStride;
  v[static_cast<std::size_t>(P::AType + b)] = static_cast<float>(type);
  v[static_cast<std::size_t>(P::ALevel + b)] = level;
  v[static_cast<std::size_t>(P::APitch + b)] = pitch;
  v[static_cast<std::size_t>(P::AFine + b)] = chance(.5f) ? range(-12.f, 12.f) : 0.f;
  v[static_cast<std::size_t>(P::AChar + b)] = range(0.f, 100.f);
  v[static_cast<std::size_t>(P::AMotion + b)] = range(0.f, 100.f);
  v[static_cast<std::size_t>(P::ASample + b)] = static_cast<float>(pickSample());
 }
 void set(Values& v, int p, float x) { v[static_cast<std::size_t>(p)] = x; }

 void musical(Values& v) {
  static const int typesA[] = {Wave, Wave, FM, FM, Reed, ResonatorSrc, SampleSrc};
  static const int typesB[] = {Wave, FM, Noise, Grains, ResonatorSrc, Reed};
  static const float intervals[] = {0.f, 0.f, 0.f, 12.f, -12.f, 7.f, 19.f, 24.f};
  const int a = pick(typesA);
  source(v, 0, a, range(65.f, 90.f), chance(.8f) ? 0.f : -12.f);
  source(v, 1, pick(typesB), chance(.7f) ? range(15.f, 55.f) : 0.f, pickf(intervals));
  if (a == Wave) set(v, P::AMotion, range(0.f, 45.f));
  if (a == FM) set(v, P::AMotion, range(10.f, 55.f));
  if (a == ResonatorSrc) set(v, P::AMotion, range(0.f, 35.f));
  set(v, P::BMotion, range(0.f, 50.f));
  set(v, P::FilterMode, chance(.8f) ? 0.f : 1.f);
  set(v, P::Cutoff, logRange(700.f, 9000.f));
  set(v, P::Resonance, range(0.f, 45.f));
  set(v, P::FilterEnv, chance(.6f) ? range(-10.f, 50.f) : 0.f);
  set(v, P::Drive, chance(.35f) ? range(5.f, 35.f) : 0.f);
  set(v, P::DriveType, chance(.8f) ? 0.f : static_cast<float>(pick({1, 2})));
  set(v, P::Attack, logRange(2.f, 400.f));
  set(v, P::Decay, logRange(150.f, 2500.f));
  set(v, P::Sustain, range(25.f, 90.f));
  set(v, P::Release, logRange(150.f, 3000.f));
  set(v, P::ModAttack, logRange(1.f, 300.f));
  set(v, P::ModDecay, logRange(100.f, 2500.f));
  set(v, P::LfoShape, chance(.7f) ? 0.f : 4.f);
  set(v, P::LfoRate, logRange(.1f, 6.f));
  set(v, P::LfoPitch, chance(.5f) ? range(0.f, 18.f) : 0.f);
  set(v, P::LfoFilter, chance(.4f) ? range(0.f, 30.f) : 0.f);
  set(v, P::LfoChar, chance(.4f) ? range(0.f, 40.f) : 0.f);
  set(v, P::EchoMix, chance(.4f) ? range(5.f, 30.f) : 0.f);
  set(v, P::EchoSync, static_cast<float>(pick({0, 2, 3})));
  set(v, P::RevMix, range(12.f, 45.f));
  set(v, P::RevSize, range(40.f, 85.f));
  set(v, P::RevDecay, logRange(1.f, 6.f));
 }

 void textures(Values& v) {
  static const int typesA[] = {Grains, Grains, Noise, Wave, ResonatorSrc, FM};
  static const int typesB[] = {Grains, Noise, Wave, FM, ResonatorSrc};
  static const float intervals[] = {0.f, 12.f, 19.f, 24.f, -12.f, 7.f, 5.f};
  source(v, 0, pick(typesA), range(60.f, 90.f), chance(.7f) ? 0.f : -12.f);
  source(v, 1, pick(typesB), range(20.f, 70.f), pickf(intervals));
  if (v[P::AType] == ResonatorSrc) set(v, P::AMotion, range(60.f, 100.f)); // bowed
  if (v[P::AType] == Noise) set(v, P::AMotion, range(40.f, 95.f));          // pitched air
  set(v, P::FilterMode, static_cast<float>(pick({0, 0, 1, 3})));
  set(v, P::Cutoff, logRange(300.f, 7000.f));
  set(v, P::Resonance, range(10.f, 70.f));
  set(v, P::FilterEnv, range(-20.f, 40.f));
  set(v, P::Attack, logRange(300.f, 5000.f));
  set(v, P::Decay, logRange(1000.f, 8000.f));
  set(v, P::Sustain, range(60.f, 100.f));
  set(v, P::Release, logRange(2000.f, 10000.f));
  set(v, P::ModAttack, logRange(500.f, 6000.f));
  set(v, P::ModDecay, logRange(2000.f, 12000.f));
  set(v, P::LfoShape, static_cast<float>(pick({0, 4, 4, 3})));
  set(v, P::LfoRate, logRange(.03f, .6f));
  set(v, P::LfoFilter, range(10.f, 60.f));
  set(v, P::LfoChar, range(20.f, 90.f));
  set(v, P::LfoPan, range(10.f, 70.f));
  set(v, P::LfoPitch, chance(.3f) ? range(5.f, 40.f) : 0.f);
  set(v, P::Ring, chance(.2f) ? range(10.f, 50.f) : 0.f);
  set(v, P::Reverse, chance(.25f) ? range(10.f, 50.f) : 0.f);
  set(v, P::EchoMix, chance(.6f) ? range(10.f, 45.f) : 0.f);
  set(v, P::EchoTime, logRange(250.f, 1100.f));
  set(v, P::EchoFeedback, range(30.f, 70.f));
  set(v, P::RevMix, range(40.f, 80.f));
  set(v, P::RevSize, range(70.f, 100.f));
  set(v, P::RevDecay, logRange(4.f, 15.f));
  set(v, P::Octave, static_cast<float>(pick({1, 2, 2})));
 }

 void hits(Values& v) {
  static const int typesA[] = {SampleSrc, SampleSrc, ResonatorSrc, Noise, FM};
  static const int typesB[] = {Wave, Wave, FM, SampleSrc, Noise, ResonatorSrc};
  source(v, 0, pick(typesA), range(70.f, 95.f), static_cast<float>(pick({0, 0, -12, -5, 7})));
  source(v, 1, pick(typesB), range(40.f, 85.f), static_cast<float>(pick({-12, -24, 0, -12})));
  if (v[P::AType] == SampleSrc) { set(v, P::AChar, range(0.f, 10.f)); set(v, P::AMotion, 0.f); }
  if (v[P::AType] == ResonatorSrc) set(v, P::AMotion, range(0.f, 20.f)); // struck
  if (v[P::BType] == Wave) { set(v, P::BChar, range(0.f, 15.f)); set(v, P::BMotion, 0.f); } // a sub thump
  if (v[P::BType] == SampleSrc) { set(v, P::BChar, range(0.f, 10.f)); set(v, P::BMotion, 0.f); }
  set(v, P::Hold, 1.f);
  set(v, P::FilterMode, chance(.75f) ? 0.f : 1.f);
  set(v, P::Cutoff, logRange(1500.f, 16000.f));
  set(v, P::Resonance, range(0.f, 40.f));
  set(v, P::FilterEnv, range(0.f, 60.f));
  set(v, P::Keytrack, range(0.f, 50.f));
  set(v, P::Drive, range(15.f, 70.f));
  set(v, P::DriveType, static_cast<float>(pick({0, 0, 1, 2, 4})));
  set(v, P::Attack, logRange(1.f, 8.f));
  set(v, P::Decay, logRange(200.f, 2500.f));
  set(v, P::Sustain, 0.f);
  set(v, P::Release, logRange(300.f, 2000.f));
  set(v, P::ModAttack, 1.f);
  set(v, P::ModDecay, logRange(30.f, 600.f));
  set(v, P::PitchEnv, chance(.7f) ? range(-24.f, 14.f) : 0.f);
  set(v, P::Reverse, chance(.15f) ? range(30.f, 80.f) : 0.f);
  set(v, P::EchoMix, chance(.3f) ? range(5.f, 25.f) : 0.f);
  set(v, P::RevMix, range(20.f, 60.f));
  set(v, P::RevSize, range(50.f, 100.f));
  set(v, P::RevDecay, logRange(1.5f, 8.f));
 }

 void rhythmic(Values& v) {
  musical(v);
  static const int typesA[] = {Wave, FM, SampleSrc, Grains, Reed, Noise};
  source(v, 0, pick(typesA), range(70.f, 90.f), 0.f);
  set(v, P::Gate, range(60.f, 100.f));
  set(v, P::GateDiv, static_cast<float>(pick({0, 1, 1, 2})));
  set(v, P::GatePattern, std::round(range(1.f, 255.f)));
  set(v, P::Sustain, range(60.f, 100.f));
  set(v, P::LfoSync, static_cast<float>(pick({3, 4, 5, 6})));
  set(v, P::LfoShape, static_cast<float>(pick({2, 3, 0})));
  set(v, P::LfoFilter, range(20.f, 70.f));
  set(v, P::Glitch, chance(.35f) ? range(10.f, 60.f) : 0.f);
  set(v, P::EchoMix, range(10.f, 40.f));
  set(v, P::EchoSync, static_cast<float>(pick({2, 3, 4, 6})));
 }

 void weird(Values& v) {
  for (int k = 0; k < 2; ++k)
   source(v, k, static_cast<int>(u() * numSourceTypes) % numSourceTypes, range(40.f, 95.f), std::round(range(-24.f, 24.f)));
  const auto& table = specs();
  for (int p = P::Ring; p < P::EchoMix; ++p) {
   const auto& s = table[static_cast<std::size_t>(p)];
   if (s.kind == Kind::Float) v[static_cast<std::size_t>(p)] = s.centre > 0.f && s.min > 0.f ? logRange(s.min, s.max) : range(s.min, s.max);
   else v[static_cast<std::size_t>(p)] = std::round(range(s.min, s.max));
  }
  set(v, P::Hold, chance(.4f) ? 1.f : 0.f);
  set(v, P::Gate, chance(.4f) ? range(30.f, 100.f) : 0.f);
  set(v, P::Glitch, chance(.4f) ? range(10.f, 80.f) : 0.f);
  set(v, P::Crush, chance(.3f) ? range(20.f, 70.f) : 0.f);
  set(v, P::EchoMix, range(0.f, 50.f));
  set(v, P::EchoFeedback, range(20.f, 80.f));
  set(v, P::RevMix, range(10.f, 70.f));
  set(v, P::RevSize, range(30.f, 100.f));
  set(v, P::RevDecay, logRange(1.f, 12.f));
 }

 static void keepLocked(Values& v, const Values& current) {
  const bool lock[5] = {current[P::LockA] >= .5f, current[P::LockB] >= .5f, current[P::LockShape] >= .5f,
                        current[P::LockMotion] >= .5f, current[P::LockSpace] >= .5f};
  for (int p = 0; p < P::Count; ++p) {
   const auto sec = sectionOf(p);
   const bool keep = (sec == Section::SourceA && lock[0]) || (sec == Section::SourceB && lock[1]) || (sec == Section::Shape && lock[2])
                     || (sec == Section::Motion && lock[3]) || (sec == Section::Space && lock[4]) || sec == Section::Generator;
   if (keep) v[static_cast<std::size_t>(p)] = current[static_cast<std::size_t>(p)];
  }
 }

 Random rng;
 std::uint32_t slots = 0;
};

} // namespace inverso_abismo::enki
