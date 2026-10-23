#pragma once
// INVERSO ABISMO synthesis engine. Portable C++17, no JUCE, no allocation after
// prepare(). Per voice: morphing PolyBLEP oscillator stacks (unison + dissonant
// cluster intervals), sub sine, breath noise, a 6-mode resonator excited by
// bow noise / scrape impulses / strike burst, stereo TPT state-variable filter.
// Global: LFO, FDN reverb, width, output gain and a soft safety ceiling.
#include "AbismoParams.h"
#include "AbismoReverb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace inverso_abismo {

constexpr float twoPi = 6.28318530718f;
constexpr int maxVoices = 16;
constexpr int maxUnison = 7;
constexpr int maxStacks = 3;  // root + two cluster intervals
constexpr int numModes = 6;
constexpr int controlInterval = 16;

namespace detail {
inline float polyBlep(float t, float dt) {
 if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
 if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
 return 0.f;
}
inline float sine(float phase) {
 static const auto table = [] {
  std::array<float, 2049> t{};
  for (std::size_t i = 0; i < t.size(); ++i) t[i] = std::sin(twoPi * static_cast<float>(i) / 2048.f);
  return t;
 }();
 const float pos = phase * 2048.f;
 const auto i = std::min(static_cast<std::size_t>(pos), std::size_t{2047});
 const float frac = pos - static_cast<float>(i);
 return table[i] + frac * (table[i + 1] - table[i]);
}
inline float softTanh(float x) {
 x = std::clamp(x, -3.f, 3.f);
 return x * (27.f + x * x) / (27.f + 9.f * x * x);
}
inline float semitoneRatio(float st) { return std::exp2(st / 12.f); }
inline float msToCoef(float ms, double sr, double timeConstants = 4.6) {
 // One-pole coefficient: 4.6 time constants = 99% of the way after `ms`.
 const double samples = std::max(1.0, static_cast<double>(ms) * .001 * sr);
 return static_cast<float>(1.0 - std::exp(-timeConstants / samples));
}
struct Random {
 std::uint32_t state = 0x9E3779B9u;
 float next() { // uniform [-1, 1)
  state ^= state << 13; state ^= state >> 17; state ^= state << 5;
  return static_cast<float>(state) * (2.f / 4294967296.f) - 1.f;
 }
};
} // namespace detail

class Envelope {
public:
 enum class Stage { Idle, Attack, Decay, Sustain, Release };
 void set(float a, float d, float s, float r, double sr) {
  attackStep = static_cast<float>(1.0 / std::max(1.0, static_cast<double>(a) * .001 * sr));
  decayCoef = detail::msToCoef(d, sr);
  releaseCoef = detail::msToCoef(r, sr, 9.2); // -80 dB at the release time, then the voice frees
  sustain = std::clamp(s, 0.f, 1.f);
 }
 void copySettings(const Envelope& o) {
  attackStep = o.attackStep; decayCoef = o.decayCoef; releaseCoef = o.releaseCoef; sustain = o.sustain;
 }
 void gateOn() { stage = Stage::Attack; } // restarts from the current level: no click on retrigger
 void gateOff() { if (stage != Stage::Idle) stage = Stage::Release; }
 void reset() { stage = Stage::Idle; level = 0.f; }
 bool active() const { return stage != Stage::Idle; }
 float next() {
  switch (stage) {
   case Stage::Idle: return 0.f;
   case Stage::Attack:
    level += attackStep;
    if (level >= 1.f) { level = 1.f; stage = Stage::Decay; }
    break;
   case Stage::Decay:
    level += decayCoef * (sustain - level);
    if (std::abs(level - sustain) < 1e-4f) { level = sustain; stage = Stage::Sustain; }
    break;
   case Stage::Sustain: level = sustain; break;
   case Stage::Release:
    level += releaseCoef * (0.f - level);
    if (level < 1e-4f) { level = 0.f; stage = Stage::Idle; }
    break;
  }
  return level;
 }
 float value() const { return level; }
 Stage stage = Stage::Idle;
private:
 float level = 0.f, attackStep = .01f, decayCoef = .01f, releaseCoef = .01f, sustain = 1.f;
};

// Topology-preserving-transform SVF (Zavalishin). Stable under fast modulation.
struct SVF {
 float ic1 = 0.f, ic2 = 0.f;
 float process(float v0, float a1, float a2, float a3, float k, int mode) {
  const float v3 = v0 - ic2;
  const float v1 = a1 * ic1 + a2 * v3;
  const float v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.f * v1 - ic1;
  ic2 = 2.f * v2 - ic2;
  if (mode == 1) return v1 * k;           // band-pass, unity peak
  if (mode == 2) return v0 - k * v1 - v2; // high-pass
  return v2;                              // low-pass
 }
 void reset() { ic1 = ic2 = 0.f; }
};

struct Resonator {
 float y1 = 0.f, y2 = 0.f, a1 = 0.f, a2 = 0.f, gain = 0.f;
 void set(float freq, float t60, double sr) {
  const float w = twoPi * freq / static_cast<float>(sr);
  if (w >= 2.9f || w <= 0.f) { gain = 0.f; return; }
  const float r = static_cast<float>(std::exp(-6.9078 / (std::max(.01, static_cast<double>(t60)) * sr)));
  a1 = 2.f * r * std::cos(w);
  a2 = -r * r;
  // Equal-power normalisation for noise/impulse excitation of an AR(2) system.
  const float r2 = r * r;
  const float variance = (1.f + r2) / ((1.f - r2) * ((1.f + r2) * (1.f + r2) - a1 * a1));
  gain = variance > 0.f ? 1.f / std::sqrt(variance) : 0.f;
 }
 float process(float x) {
  const float y = gain * x + a1 * y1 + a2 * y2;
  y2 = y1; y1 = y;
  return y;
 }
 void reset() { y1 = y2 = 0.f; }
};

struct Oscillator {
 float phase = 0.f, inc = 0.f;
 float next(float shape) {
  const float t = phase;
  phase += inc;
  if (phase >= 1.f) phase -= 1.f;
  const float seg = shape * 3.f;
  auto wave = [&](int w) {
   switch (w) {
    case 0: return detail::sine(t);
    case 1: return 1.f - 4.f * std::abs(t - .5f);
    case 2: return 2.f * t - 1.f - detail::polyBlep(t, inc);
    default: {
     float sq = t < .5f ? 1.f : -1.f;
     float t2 = t + .5f; if (t2 >= 1.f) t2 -= 1.f;
     return sq + detail::polyBlep(t, inc) - detail::polyBlep(t2, inc);
    }
   }
  };
  const int a = std::clamp(static_cast<int>(seg), 0, 2);
  const float frac = std::clamp(seg - static_cast<float>(a), 0.f, 1.f);
  if (frac <= 0.f) return wave(a);
  if (frac >= 1.f) return wave(a + 1);
  return wave(a) + frac * (wave(a + 1) - wave(a));
 }
};

struct Controls {
 // Values derived from the parameter table once per control tick.
 float shape = .66f, oscLevel = .7f, subLevel = 0.f, noiseLevel = 0.f;
 float clusterLevel = 0.f, detuneCents = 10.f, spread = .6f;
 float resLevel = 0.f, resDecay = 2.f, bow = .4f, scrapeRate = 0.f, strike = .3f, exciterCoef = .5f;
 float cutoff = 3500.f, k = 1.7f, filterEnv = .2f, keytrack = .3f, drive = 0.f;
 float pitchEnv = 0.f, pitchTimeMs = 1500.f, driftCents = 5.f, glideMs = 0.f;
 float lfoPitchCents = 0.f, lfoFilterOct = 0.f, lfoAmp = 0.f, velSens = .6f;
 float octaveSt = 0.f, bendRange = 2.f;
 int filterMode = 0, unison = 3, material = 2;
 std::array<float, 2> clusterSt{0.f, 0.f};
 int clusterCount = 0;
};

inline const std::array<float, numModes>& materialRatios(int material) {
 static const std::array<std::array<float, numModes>, 6> ratios{{
  {1.f, 2.f, 3.f, 4.f, 5.f, 6.f},                   // string: harmonic
  {1.f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f},       // wood: free bar
  {1.f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f},         // metal: plate / bowed cymbal
  {1.f, 2.32f, 4.25f, 6.63f, 9.38f, 12.13f},        // glass
  {.5f, 1.f, 1.19f, 1.5f, 2.f, 2.66f},              // bell: hum, prime, minor third...
  {1.f, 1.06f, 1.414f, 1.5f, 2.83f, 3.17f},         // abyss: semitone and tritone beating
 }};
 return ratios[static_cast<std::size_t>(std::clamp(material, 0, 5))];
}

class Voice {
public:
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 12345u; }

 void start(int midiNote, float vel, float glideFrom, const Controls& c, bool stolen) {
  note = midiNote;
  velocity = std::clamp(vel, 0.f, 1.f);
  released = sustained = false;
  age = 0;
  if (!stolen) {
   for (auto& stack : osc) for (auto& o : stack) o.phase = .5f * (rng.next() + 1.f);
   sub.phase = 0.f;
   for (auto& m : modes) m.reset();
   filterL.reset(); filterR.reset();
   driftValue = driftTarget = 0.f;
   exciterState = 0.f;
  }
  unison = std::clamp(c.unison, 1, maxUnison);
  for (int u = 0; u < unison; ++u) {
   const float pos = unison == 1 ? 0.f : -1.f + 2.f * static_cast<float>(u) / static_cast<float>(unison - 1);
   unisonPos[static_cast<std::size_t>(u)] = pos;
   // Small random offset so stacked unison never phase-locks into a static comb.
   unisonJitter[static_cast<std::size_t>(u)] = .15f * rng.next();
  }
  pitchEnvTime = 0.f;
  glideOffset = glideStart = glideFrom;
  strikeSamples = 0;
  amp.gateOn();
  fenv.gateOn();
 }
 void release() { released = true; amp.gateOff(); fenv.gateOff(); }
 bool active() const { return amp.active(); }

 void control(const Controls& c, float bendSt, float lfo, float pressure, double sr, const Envelope& ampShape,
              const Envelope& filterShape) {
  amp.copySettings(ampShape);
  fenv.copySettings(filterShape);
  const float dtMs = static_cast<float>(controlInterval * 1000.0 / sr);
  pitchEnvTime += dtMs;
  const float gl = std::clamp(pitchEnvTime / std::max(10.f, c.pitchTimeMs), 0.f, 1.f);
  if (c.glideMs > 0.f) {
   const float step = std::abs(glideStart) * dtMs / c.glideMs; // linear portamento
   glideOffset = glideOffset > 0.f ? std::max(0.f, glideOffset - step) : std::min(0.f, glideOffset + step);
  } else glideOffset = 0.f;
  // Drift: smoothed random walk; a new target roughly every 0.25-1 s.
  if (--driftHold <= 0) {
   driftTarget = rng.next();
   driftHold = static_cast<int>((.25 + .375 * (rng.next() + 1.f)) * sr / controlInterval);
  }
  driftValue += .02f * (driftTarget - driftValue);
  const float pitch = static_cast<float>(note - 69) + c.octaveSt + bendSt + c.pitchEnv * gl + glideOffset +
                      driftValue * c.driftCents * .01f + lfo * c.lfoPitchCents * .01f;
  baseFreq = 440.f * detail::semitoneRatio(pitch);
  const float nyquistGuard = .45f * static_cast<float>(sr);
  for (int s = 0; s < maxStacks; ++s) {
   const float interval = s == 0 ? 0.f : c.clusterSt[static_cast<std::size_t>(s - 1)];
   const float stackFreq = baseFreq * detail::semitoneRatio(interval);
   for (int u = 0; u < unison; ++u) {
    const float cents = c.detuneCents * (unisonPos[static_cast<std::size_t>(u)] + unisonJitter[static_cast<std::size_t>(u)] * (unison > 1 ? 1.f : 0.f));
    const float f = std::min(stackFreq * detail::semitoneRatio(cents * .01f), nyquistGuard);
    osc[static_cast<std::size_t>(s)][static_cast<std::size_t>(u)].inc = f / static_cast<float>(sr);
   }
  }
  sub.inc = std::min(baseFreq * .5f, nyquistGuard) / static_cast<float>(sr);

  const auto& ratios = materialRatios(c.material);
  for (int m = 0; m < numModes; ++m)
   modes[static_cast<std::size_t>(m)].set(baseFreq * ratios[static_cast<std::size_t>(m)],
                                          c.resDecay / (1.f + .35f * static_cast<float>(m)), sr);

  const float velScale = 1.f - c.velSens + c.velSens * velocity;
  const float octaves = c.filterEnv * 5.f * fenv.value() * velScale + c.keytrack * static_cast<float>(note - 60) / 12.f +
                        c.lfoFilterOct * lfo + pressure * 1.5f;
  const float fc = std::clamp(c.cutoff * std::exp2(octaves), 20.f, .45f * static_cast<float>(sr));
  const float g = std::tan(3.14159265f * fc / static_cast<float>(sr));
  k = c.k;
  a1 = 1.f / (1.f + g * (g + k));
  a2 = g * a1;
  a3 = g * a2;
  ampVel = std::pow(velScale, 1.5f);
 }

 void render(const Controls& c, float lfo, float* outL, float* outR, int n, double sr) {
  const float stackGain = 1.f / std::sqrt(static_cast<float>(unison));
  std::array<float, maxUnison> panL{}, panR{};
  for (int u = 0; u < unison; ++u) {
   const float pan = .5f + .5f * unisonPos[static_cast<std::size_t>(u)] * c.spread;
   panL[static_cast<std::size_t>(u)] = std::cos(pan * 1.5707963f) * 1.4142f;
   panR[static_cast<std::size_t>(u)] = std::sin(pan * 1.5707963f) * 1.4142f;
  }
  const int stacks = 1 + std::min(c.clusterCount, 2);
  const float scrapeProb = static_cast<float>(c.scrapeRate / sr);
  const float trem = 1.f - c.lfoAmp * (.5f - .5f * lfo);
  const auto strikeLength = static_cast<int>(.004 * sr);
  for (int i = 0; i < n; ++i) {
   float l = 0.f, r = 0.f;
   if (c.oscLevel > 0.f) {
    for (int s = 0; s < stacks; ++s) {
     const float level = (s == 0 ? 1.f : c.clusterLevel * .8f) * stackGain * c.oscLevel;
     if (level <= 0.f) continue;
     for (int u = 0; u < unison; ++u) {
      const float x = osc[static_cast<std::size_t>(s)][static_cast<std::size_t>(u)].next(c.shape) * level;
      l += x * panL[static_cast<std::size_t>(u)];
      r += x * panR[static_cast<std::size_t>(u)];
     }
    }
   }
   if (c.subLevel > 0.f) { const float x = detail::sine(sub.phase) * c.subLevel; l += x; r += x; }
   sub.phase += sub.inc; if (sub.phase >= 1.f) sub.phase -= 1.f;
   if (c.noiseLevel > 0.f) { l += rng.next() * c.noiseLevel * .5f; r += rng.next() * c.noiseLevel * .5f; }
   if (c.resLevel > 0.f) {
    exciterState += c.exciterCoef * (rng.next() - exciterState);
    float exc = exciterState * c.bow * 2.f;
    if (scrapeProb > 0.f && (rng.next() + 1.f) * .5f < scrapeProb) exc += rng.next() * 6.f;
    if (strikeSamples < strikeLength) { exc += rng.next() * c.strike * velocity * 8.f; ++strikeSamples; }
    float res = 0.f;
    for (int m = 0; m < numModes; ++m)
     res += modes[static_cast<std::size_t>(m)].process(exc) / (1.f + .5f * static_cast<float>(m));
    res *= c.resLevel * .5f;
    l += res; r += res;
   } else if (strikeSamples < strikeLength) ++strikeSamples;
   if (c.drive > 0.f) {
    const float pre = 1.f + c.drive * 6.f, post = 1.f / (1.f + c.drive * 2.f);
    l = detail::softTanh(l * pre) * post * 1.8f;
    r = detail::softTanh(r * pre) * post * 1.8f;
   }
   l = filterL.process(l, a1, a2, a3, k, c.filterMode);
   r = filterR.process(r, a1, a2, a3, k, c.filterMode);
   const float e = amp.next() * ampVel * trem * .25f;
   fenv.next();
   outL[i] += l * e;
   outR[i] += r * e;
  }
  age += n;
 }

 float level() const { return amp.value(); }
 int note = -1;
 bool released = false, sustained = false;
 long long age = 0;
 Envelope amp, fenv;

private:
 std::array<std::array<Oscillator, maxUnison>, maxStacks> osc{};
 Oscillator sub;
 std::array<Resonator, numModes> modes{};
 std::array<float, maxUnison> unisonPos{}, unisonJitter{};
 SVF filterL, filterR;
 detail::Random rng;
 int unison = 1, driftHold = 0, strikeSamples = 0;
 float velocity = 1.f, baseFreq = 440.f, pitchEnvTime = 0.f, glideOffset = 0.f, glideStart = 0.f;
 float driftValue = 0.f, driftTarget = 0.f, exciterState = 0.f, ampVel = 1.f;
 float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f;
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  for (int i = 0; i < maxVoices; ++i) {
   voices[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 1));
   voices[static_cast<std::size_t>(i)].amp.reset();
   voices[static_cast<std::size_t>(i)].fenv.reset();
  }
  smoothed = target;
  controlCountdown = 0;
  lfoPhase = 0.f;
  outGain = std::pow(10.f, smoothed[P::Output] / 20.f);
  derive();
 }
 void setValues(const Values& v) { target = v; }
 const Values& values() const { return target; }

 void noteOn(int note, float velocity) {
  if (velocity <= 0.f) { noteOff(note); return; }
  for (auto& v : voices)
   if (v.active() && v.note == note && !v.released) v.release();
  Voice* chosen = nullptr;
  bool stolen = false;
  for (auto& v : voices) if (!v.active()) { chosen = &v; break; }
  if (!chosen) {
   stolen = true;
   // Steal the oldest released voice, otherwise the oldest voice.
   for (auto& v : voices) if (v.released && (!chosen || v.age > chosen->age)) chosen = &v;
   if (!chosen) for (auto& v : voices) if (!chosen || v.age > chosen->age) chosen = &v;
  }
  const float glideFrom = (controls.glideMs > 0.f && lastNote >= 0) ? static_cast<float>(lastNote - note) : 0.f;
  chosen->start(note, velocity, glideFrom, controls, stolen);
  chosen->control(controls, bend * controls.bendRange, lfoValue, pressure, sr, ampShape, filterShape);
  lastNote = note;
 }
 void noteOff(int note) {
  for (auto& v : voices) {
   if (!v.active() || v.note != note || v.released) continue;
   if (sustainPedal) v.sustained = true;
   else v.release();
  }
 }
 void setSustain(bool down) {
  sustainPedal = down;
  if (!down)
   for (auto& v : voices) if (v.sustained && !v.released) { v.sustained = false; v.release(); }
 }
 void allNotesOff() { sustainPedal = false; for (auto& v : voices) if (v.active()) v.release(); }
 void allSoundOff() { for (auto& v : voices) { v.amp.reset(); v.fenv.reset(); } reverb.clear(); }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f); }
 void setModWheel(float value) { modWheel = std::clamp(value, 0.f, 1.f); }
 void setPressure(float value) { pressure = std::clamp(value, 0.f, 1.f); }

 int activeVoices() const {
  int n = 0;
  for (const auto& v : voices) n += v.active() ? 1 : 0;
  return n;
 }

 // Adds nothing to the buffers' previous content: output is written, not mixed.
 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   float* l = left + done;
   float* r = right + done;
   std::fill(l, l + n, 0.f);
   std::fill(r, r + n, 0.f);
   for (auto& v : voices) if (v.active()) v.render(controls, lfoValue, l, r, n, sr);
   const float mix = smoothed[P::RevMix] * .01f;
   const float dryGain = std::cos(mix * 1.5707963f), wetGain = std::sin(mix * 1.5707963f) * 1.4f;
   const float width = smoothed[P::Width] * .01f;
   for (int i = 0; i < n; ++i) {
    float wl = 0.f, wr = 0.f;
    reverb.process(l[i], r[i], wl, wr);
    float ol = l[i] * dryGain + wl * wetGain;
    float orr = r[i] * dryGain + wr * wetGain;
    const float mid = .5f * (ol + orr), side = .5f * (ol - orr) * width;
    ol = (mid + side) * outGain;
    orr = (mid - side) * outGain;
    l[i] = safety(ol);
    r[i] = safety(orr);
   }
   done += n;
   controlCountdown -= n;
  }
 }

 // Soft ceiling: linear below 0.9, smoothly saturating to never exceed 1.0.
 static float safety(float x) {
  if (!std::isfinite(x)) return 0.f;
  const float a = std::abs(x);
  if (a <= .9f) return x;
  const float y = .9f + .1f * std::tanh((a - .9f) * 10.f);
  return x < 0.f ? -y : y;
 }

private:
 void controlTick() {
  const auto& table = specs();
  for (std::size_t i = 0; i < target.size(); ++i) {
   if (table[i].kind == Kind::Float) smoothed[i] += .15f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  outGain = std::pow(10.f, smoothed[P::Output] / 20.f);
  derive();
  lfoPhase += static_cast<float>(smoothed[P::LfoRate] * controlInterval / sr);
  while (lfoPhase >= 1.f) lfoPhase -= 1.f;
  lfoValue = detail::sine(lfoPhase);
  for (auto& v : voices)
   if (v.active()) v.control(controls, bend * controls.bendRange, lfoValue, pressure, sr, ampShape, filterShape);
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  const float tension = std::clamp(pct(P::Tension) + modWheel, 0.f, 1.f);
  Controls& c = controls;
  c.shape = pct(P::OscShape);
  c.oscLevel = pct(P::OscLevel);
  c.octaveSt = static_cast<float>((choice(P::Octave) - 2) * 12);
  c.unison = choice(P::Unison) + 1;
  const float det = pct(P::Detune);
  c.detuneCents = det * det * 60.f + tension * 10.f;
  c.spread = pct(P::Spread);
  c.subLevel = pct(P::SubLevel);
  static const float clusterTable[7][2] = {{0, 0}, {1, 0}, {6, 0}, {1, 6}, {-.5f, .5f}, {6, 13}, {1, 11}};
  static const int clusterCounts[7] = {0, 1, 1, 2, 2, 2, 2};
  const int mode = std::clamp(choice(P::ClusterMode), 0, 6);
  c.clusterSt = {clusterTable[mode][0], clusterTable[mode][1]};
  c.clusterCount = clusterCounts[mode];
  c.clusterLevel = mode ? std::min(1.f, pct(P::ClusterLevel) + tension * (1.f - pct(P::ClusterLevel)) * .8f) : 0.f;
  c.noiseLevel = pct(P::NoiseLevel);
  c.material = choice(P::ResMaterial);
  c.resLevel = pct(P::ResLevel);
  c.resDecay = s[P::ResDecay];
  c.bow = std::min(1.f, pct(P::Bow) + tension * .4f + pressure * .5f);
  const float scrape = pct(P::Scrape);
  c.scrapeRate = scrape * scrape * 400.f;
  c.strike = pct(P::Strike);
  c.exciterCoef = .02f + .98f * pct(P::NoiseTone) * pct(P::NoiseTone);
  c.filterMode = choice(P::FilterMode);
  c.cutoff = s[P::Cutoff];
  c.k = 2.f * (1.f - .97f * std::min(1.f, pct(P::Resonance) + tension * .25f));
  c.filterEnv = pct(P::FilterEnv);
  c.keytrack = pct(P::Keytrack);
  c.drive = pct(P::Drive);
  c.velSens = pct(P::VelSens);
  c.pitchEnv = s[P::PitchEnv];
  c.pitchTimeMs = s[P::PitchTime];
  const float drift = std::min(1.f, pct(P::Drift) + tension * .6f);
  c.driftCents = drift * drift * 60.f; // up to 60 cents of wandering pitch
  c.glideMs = s[P::Glide];
  c.bendRange = s[P::BendRange];
  const float lp = std::min(1.f, pct(P::LfoPitch) + tension * .15f);
  c.lfoPitchCents = lp * lp * 200.f;
  c.lfoFilterOct = pct(P::LfoFilter) * 3.f;
  c.lfoAmp = pct(P::LfoAmp);
  ampShape.set(s[P::Attack], s[P::Decay], pct(P::Sustain), s[P::Release], sr);
  filterShape.set(s[P::FAttack], s[P::FDecay], pct(P::FSustain), s[P::FRelease], sr);
  reverb.configure(pct(P::RevSize), s[P::RevDecay], pct(P::RevDamp), s[P::Freeze] >= .5f);
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 Controls controls;
 Envelope ampShape, filterShape;
 std::array<Voice, maxVoices> voices{};
 Reverb reverb;
 int controlCountdown = 0, lastNote = -1;
 float lfoPhase = 0.f, lfoValue = 0.f, bend = 0.f, modWheel = 0.f, pressure = 0.f, outGain = .5f;
 bool sustainPedal = false;
};

} // namespace inverso_abismo
