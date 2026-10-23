#pragma once
// INVERSO ABISMO synthesis engine. Portable C++17, no JUCE, no allocation after
// prepare(). Per voice: morphing PolyBLEP oscillator stacks (unison + dissonant
// cluster intervals), sub sine, breath noise, a 12-mode resonator (24 materials,
// morph between two, own tuning, stretch, rattle) excited by bow noise / scrape
// impulses / strike burst / the oscillators, five drive types, ring modulator,
// stereo TPT state-variable filter.
// Optional 3-formant vowel filter after the SVF for processed-voice timbres.
// Global: LFO, stutter / reverse / crush stage, FDN reverb with a reversed tail,
// tape warp, width, output gain and a soft safety ceiling.
#include "AbismoCommon.h"
#include "AbismoParams.h"
#include "AbismoMutilator.h"
#include "AbismoReverb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace inverso_abismo {

constexpr int maxVoices = 16;
constexpr int maxUnison = 7;
constexpr int maxStacks = 3;  // root + two cluster intervals
constexpr int numModes = 12;

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
 float voiceMix = 0.f, vowel = 0.f, formantRatio = 1.f;
 float morph = 0.f, resTuneSt = 0.f, resKeytrack = 1.f, stretch = 0.f, bright = .5f, resStereo = .4f;
 float resInput = 0.f, rattle = 0.f, ringMix = 0.f, ringSt = 7.f;
 int filterMode = 0, unison = 3, material = 2, materialB = 5, driveMode = 0;
 std::array<float, 2> clusterSt{0.f, 0.f};
 int clusterCount = 0;
};

// Mode frequency ratios for each material, lowest first. Some come from physics
// (bars, membranes, stiff strings), some are composed (chords, clusters).
inline const std::array<float, numModes>& materialRatios(int material) {
 static const std::array<std::array<float, numModes>, numMaterials> ratios{{
  {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f},                        // string: harmonic
  {1.f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.8f, 31.9f, 39.9f, 48.8f, 58.6f, 69.3f}, // wood: free bar
  {1.f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.16f, 3.50f, 3.91f, 4.22f, 4.65f, 5.10f},   // metal: plate / bowed cymbal
  {1.f, 2.32f, 4.25f, 6.63f, 9.38f, 12.13f, 15.1f, 18.4f, 21.9f, 25.6f, 29.5f, 33.6f},  // glass
  {.5f, 1.f, 1.19f, 1.5f, 2.f, 2.66f, 3.01f, 3.25f, 4.06f, 4.62f, 5.33f, 6.1f},        // bell: hum, prime, tierce...
  {1.f, 1.06f, 1.414f, 1.5f, 2.83f, 3.17f, 4.24f, 4.49f, 5.66f, 6.f, 7.07f, 8.48f},    // abyss: semitones and tritones
  {1.f, 3.98f, 9.95f, 15.4f, 20.8f, 27.3f, 34.6f, 42.9f, 52.f, 62.f, 73.f, 85.f},      // marimba: tuned bar
  {1.f, 3.f, 5.f, 7.f, 9.f, 11.f, 13.f, 15.f, 17.f, 19.f, 21.f, 23.f},                 // pipe: closed tube, odd harmonics
  {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.6f, 3.652f, 4.06f, 4.154f}, // membrane: Bessel
  {1.f, 1.12f, 1.33f, 1.51f, 1.68f, 1.89f, 2.03f, 2.29f, 2.47f, 2.75f, 3.01f, 3.38f},  // gong: dense, low
  {1.f, 1.004f, 2.71f, 2.718f, 5.06f, 5.071f, 7.98f, 7.995f, 11.5f, 11.52f, 15.6f, 15.63f}, // singing bowl: beating pairs
  {1.f, 2.83f, 5.2f, 8.f, 11.18f, 14.7f, 18.5f, 22.6f, 27.f, 31.6f, 36.5f, 41.6f},     // spring
  {1.f, 2.003f, 3.011f, 4.026f, 5.05f, 6.086f, 7.136f, 8.202f, 9.286f, 10.39f, 11.52f, 12.67f}, // piano wire: stiff string
  {1.f, 1.52f, 2.04f, 2.67f, 3.92f, 4.56f, 5.28f, 6.f, 7.1f, 8.3f, 9.1f, 10.5f},       // gamelan: bonang kettle
  {1.f, 1.47f, 2.09f, 2.56f, 3.39f, 4.12f, 4.79f, 5.61f, 6.27f, 7.04f, 8.23f, 9.48f},  // cymbal
  {1.f, 2.9f, 3.1f, 5.8f, 6.3f, 9.6f, 10.1f, 13.7f, 14.5f, 18.9f, 19.6f, 24.8f},       // bone: harsh close pairs
  {1.f, 1.189f, 1.498f, 2.f, 2.378f, 2.997f, 4.f, 4.757f, 5.993f, 8.f, 9.514f, 11.99f}, // minor chord
  {1.f, 1.059f, 1.122f, 1.189f, 1.26f, 1.335f, 2.f, 2.119f, 2.245f, 2.378f, 2.52f, 2.67f}, // cluster: semitone stack
  {1.f, 1.01f, 1.414f, 1.43f, 2.f, 2.02f, 2.828f, 2.86f, 4.f, 4.04f, 5.657f, 5.72f},    // tritone, each beating
  {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f},                     // rust: harmonic, bent per note
  {.47f, .91f, 1.f, 1.35f, 1.94f, 2.5f, 3.1f, 3.85f, 4.7f, 5.5f, 6.6f, 7.8f},          // wooden body: low resonances
  {1.f, 1.03f, 1.08f, 1.15f, 1.21f, 1.3f, 1.37f, 1.46f, 1.55f, 1.63f, 1.74f, 1.86f},   // thunder sheet: dense wobble
  {.25f, .5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 5.f, 6.f},                  // underground: sub-octaves
  {.5f, .51f, 1.f, 1.02f, 1.19f, 1.21f, 1.5f, 1.53f, 2.f, 2.04f, 2.66f, 2.71f},        // cracked bell: split partials
 }};
 return ratios[static_cast<std::size_t>(std::clamp(material, 0, numMaterials - 1))];
}

// How far (cents) each mode is bent at random per note: a little everywhere, a lot in Rust.
inline float materialJitterCents(int material) { return material == 19 ? 45.f : 3.f; }

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
   formant.reset();
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
  // Each note bends the resonator's partials its own way (strongly in Rust).
  for (auto& j : modeJitter) j = rng.next();
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

  // Resonator: follows the note (keytrack 100%) or sits fixed around middle C,
  // shifted by its own tuning; partials morph from material A to B, stretch, bend.
  const float resPitch = c.resKeytrack * (pitch - c.octaveSt) + (1.f - c.resKeytrack) * -9.f + c.octaveSt + c.resTuneSt;
  const float resBase = 440.f * detail::semitoneRatio(resPitch);
  const auto& ra = materialRatios(c.material);
  const auto& rb = materialRatios(c.materialB);
  const float jitter = (1.f - c.morph) * materialJitterCents(c.material) + c.morph * materialJitterCents(c.materialB);
  const float stretchPow = 1.f + .3f * c.stretch;
  const float decayTilt = .05f + .6f * (1.f - c.bright), ampTilt = .1f + .8f * (1.f - c.bright);
  float ampSum = 0.f;
  for (int m = 0; m < numModes; ++m) {
   const auto i = static_cast<std::size_t>(m);
   float ratio = c.morph <= 0.f ? ra[i] : c.morph >= 1.f ? rb[i] : std::exp((1.f - c.morph) * std::log(ra[i]) + c.morph * std::log(rb[i]));
   ratio = std::pow(ratio, stretchPow) * detail::semitoneRatio(modeJitter[i] * jitter * .01f);
   modes[i].set(std::min(resBase * ratio, nyquistGuard), c.resDecay / (1.f + decayTilt * static_cast<float>(m)), sr);
   modeAmp[i] = 1.f / (1.f + ampTilt * static_cast<float>(m));
   ampSum += modeAmp[i] * modeAmp[i];
  }
  // Same power as the old 6-mode resonator whatever the tilt (the modes ring independently).
  for (auto& a : modeAmp) a *= 1.431f / std::sqrt(ampSum);
  ringInc = std::min(baseFreq * detail::semitoneRatio(c.ringSt), nyquistGuard) / static_cast<float>(sr);

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
  if (c.voiceMix > 0.f) formant.set(std::clamp(c.vowel + c.lfoFilterOct * lfo * .1f, 0.f, 1.f), c.formantRatio, sr);
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
   const float oscMono = .5f * (l + r);
   if (c.subLevel > 0.f) { const float x = detail::sine(sub.phase) * c.subLevel; l += x; r += x; }
   sub.phase += sub.inc; if (sub.phase >= 1.f) sub.phase -= 1.f;
   if (c.noiseLevel > 0.f) { l += rng.next() * c.noiseLevel * .5f; r += rng.next() * c.noiseLevel * .5f; }
   if (c.resLevel > 0.f) {
    exciterState += c.exciterCoef * (rng.next() - exciterState);
    float exc = exciterState * c.bow * 2.f;
    if (scrapeProb > 0.f && (rng.next() + 1.f) * .5f < scrapeProb) exc += rng.next() * 6.f;
    if (strikeSamples < strikeLength) { exc += rng.next() * c.strike * velocity * 8.f; ++strikeSamples; }
    // The oscillators can drive the resonator too, like a string through a body.
    exc += c.resInput * oscMono * .35f + rattleFeedback;
    float resL = 0.f, resR = 0.f;
    for (int m = 0; m < numModes; ++m) {
     const auto mi = static_cast<std::size_t>(m);
     const float y = modes[mi].process(exc) * modeAmp[mi];
     const float side = (m & 1) ? c.resStereo : -c.resStereo; // alternate partials left and right
     resL += y * (1.f + side);
     resR += y * (1.f - side);
    }
    resL *= c.resLevel * .5f; resR *= c.resLevel * .5f;
    if (c.rattle > 0.f) {
     // A loose part touching the body: past a small swing it buzzes, and the buzz rings the body again.
     const float swing = .5f * (std::abs(resL) + std::abs(resR));
     const float excess = std::max(0.f, swing - .04f);
     // Contact clicks, darkened: a metal buzz, not white static.
     rattleTone += .25f * (excess * rng.next() * c.rattle * 14.f - rattleTone);
     const float buzz = rattleTone;
     rattleFeedback = buzz * .3f;
     resL += buzz; resR += buzz;
    } else rattleFeedback = 0.f;
    // Fed by the oscillators a long resonance can pile up: keep it in a musical range.
    l += 3.f * detail::softTanh(resL * (1.f / 3.f));
    r += 3.f * detail::softTanh(resR * (1.f / 3.f));
   } else if (strikeSamples < strikeLength) ++strikeSamples;
   if (c.drive > 0.f) {
    l = shape(l, c, dcL);
    r = shape(r, c, dcR);
   }
   if (c.ringMix > 0.f) {
    const float carrier = detail::sine(ringPhase);
    l *= 1.f - c.ringMix + c.ringMix * carrier;
    r *= 1.f - c.ringMix + c.ringMix * carrier;
   }
   ringPhase += ringInc; if (ringPhase >= 1.f) ringPhase -= 1.f;
   l = filterL.process(l, a1, a2, a3, k, c.filterMode);
   r = filterR.process(r, a1, a2, a3, k, c.filterMode);
   if (c.voiceMix > 0.f) formant.process(l, r, c.voiceMix);
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
 // The five drive types. All keep roughly the same loudness as the tape drive.
 float shape(float x, const Controls& c, float& dc) {
  const float pre = 1.f + c.drive * 6.f, post = 1.f / (1.f + c.drive * 2.f);
  switch (c.driveMode) {
   case 1: { // fuzz: hard, asymmetric clipping
    float y = x * pre * 1.5f;
    y = y > 0.f ? std::min(y, 1.f) : std::max(y, -.55f) * 1.6f;
    return y * post * 1.6f;
   }
   case 2: // fold: the wave folds back on itself, more folds as drive rises
    return std::sin(x * pre * 1.3f) * (.55f + .45f * post);
   case 3: { // rectifier: full-wave, an octave up and buzzing; DC removed
    const float y = detail::softTanh(std::abs(x) * pre) * 1.6f;
    dc += .0015f * (y - dc);
    return (y - dc) * (.5f + post);
   }
   case 4: { // shatter: crushed curve with random dropouts that crackle
    if (dropHold > 0) { --dropHold; return 0.f; }
    if ((rng.next() + 1.f) * .5f < c.drive * .0015f) dropHold = 8 + static_cast<int>((rng.next() + 1.f) * 120.f);
    const float y = x * pre;
    const float a = std::min(1.f, std::abs(y));
    return (y < 0.f ? -1.f : 1.f) * std::sqrt(std::sqrt(a)) * (.45f + .4f * post);
   }
   default:
    return detail::softTanh(x * pre) * post * 1.8f;
  }
 }

 std::array<std::array<Oscillator, maxUnison>, maxStacks> osc{};
 Oscillator sub;
 std::array<Resonator, numModes> modes{};
 std::array<float, numModes> modeAmp{}, modeJitter{};
 std::array<float, maxUnison> unisonPos{}, unisonJitter{};
 float rattleFeedback = 0.f, rattleTone = 0.f, ringPhase = 0.f, ringInc = 0.f, dcL = 0.f, dcR = 0.f;
 int dropHold = 0;
 SVF filterL, filterR;
 FormantFilter formant;
 detail::Random rng;
 int unison = 1, driftHold = 0, strikeSamples = 0;
 float velocity = 1.f, baseFreq = 440.f, pitchEnvTime = 0.f, glideOffset = 0.f, glideStart = 0.f;
 float driftValue = 0.f, driftTarget = 0.f, exciterState = 0.f, ampVel = 1.f;
 float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f;
};

// Reversed reverb tail: two overlapping grains each play the last 350 ms of the
// wet signal backwards, so every tail swells up into its source.
class ReverseGrains {
public:
 void prepare(double sr) {
  window = std::max(64, static_cast<int>(.35 * sr));
  buffer.assign(static_cast<std::size_t>(window) * 4 + 8, 0.f);
  write = 0; phase = 0;
 }
 void clear() { std::fill(buffer.begin(), buffer.end(), 0.f); }
 void process(float inL, float inR, float& outL, float& outR) {
  const int frames = static_cast<int>(buffer.size() / 2);
  buffer[static_cast<std::size_t>(write) * 2] = inL;
  buffer[static_cast<std::size_t>(write) * 2 + 1] = inR;
  outL = outR = 0.f;
  for (int g = 0; g < 2; ++g) {
   const int p = (phase + g * window / 2) % window;           // samples into this grain
   const int start = (write - p + frames) % frames;           // where this grain began
   const int read = (start - 1 - p + 2 * frames) % frames;    // walking backwards from there
   const float w = std::sin(3.14159265f * static_cast<float>(p) / static_cast<float>(window));
   const float gain = w * w;                                  // the two Hann grains sum to one
   outL += buffer[static_cast<std::size_t>(read) * 2] * gain;
   outR += buffer[static_cast<std::size_t>(read) * 2 + 1] * gain;
  }
  write = (write + 1) % frames;
  phase = (phase + 1) % window;
 }
private:
 std::vector<float> buffer;
 int window = 1, write = 0, phase = 0;
};

// Tape warp: a slowly wandering delay (wow), fast flutter, and now and then the
// tape drags and drops in pitch before it recovers.
class TapeWarp {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  buffer.assign(static_cast<std::size_t>(sr * .12) * 2 + 8, 0.f);
  write = 0; wow = flutter = 0.f; drag = dragTarget = 0.f; dragHold = 0;
 }
 void clear() { std::fill(buffer.begin(), buffer.end(), 0.f); }
 void process(float& l, float& r, float amount) {
  const int frames = static_cast<int>(buffer.size() / 2);
  buffer[static_cast<std::size_t>(write) * 2] = l;
  buffer[static_cast<std::size_t>(write) * 2 + 1] = r;
  if (amount > 0.f) {
   wow += static_cast<float>(.55 / sr); if (wow >= 1.f) wow -= 1.f;
   flutter += static_cast<float>(6.3 / sr); if (flutter >= 1.f) flutter -= 1.f;
   if (--dragHold <= 0) {
    // Mostly steady; sometimes a drag that pulls the pitch down for a moment.
    dragTarget = (rng.next() + 1.f) * .5f < .25f * amount ? (rng.next() + 1.f) * .5f : 0.f;
    dragHold = static_cast<int>(sr * (.15 + .5 * (rng.next() + 1.f)));
   }
   drag += static_cast<float>(4.0 / sr) * (dragTarget - drag);
   const float ms = 2.f + amount * (14.f * (.5f + .5f * detail::sine(wow)) + .9f * detail::sine(flutter) + 40.f * drag);
   const float d = std::clamp(ms * .001f * static_cast<float>(sr), 1.f, static_cast<float>(frames - 2));
   const float pos = static_cast<float>(write) - d + static_cast<float>(frames);
   const int i0 = static_cast<int>(pos) % frames, i1 = (i0 + 1) % frames;
   const float f = pos - std::floor(pos);
   const float wl = buffer[static_cast<std::size_t>(i0) * 2] + f * (buffer[static_cast<std::size_t>(i1) * 2] - buffer[static_cast<std::size_t>(i0) * 2]);
   const float wr = buffer[static_cast<std::size_t>(i0) * 2 + 1] + f * (buffer[static_cast<std::size_t>(i1) * 2 + 1] - buffer[static_cast<std::size_t>(i0) * 2 + 1]);
   const float mix = std::min(1.f, amount * 2.f);
   l += mix * (wl - l);
   r += mix * (wr - r);
  }
  write = (write + 1) % frames;
 }
private:
 std::vector<float> buffer;
 double sr = 48000.0;
 int write = 0, dragHold = 0;
 float wow = 0.f, flutter = 0.f, drag = 0.f, dragTarget = 0.f;
 detail::Random rng;
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  mutilator.prepare(sr);
  reverseTail.prepare(sr);
  warp.prepare(sr);
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
 void allSoundOff() {
  for (auto& v : voices) { v.amp.reset(); v.fenv.reset(); }
  reverb.clear(); reverseTail.clear(); warp.clear();
 }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f); }
 void setModWheel(float value) { modWheel = std::clamp(value, 0.f, 1.f); }
 void setPressure(float value) { pressure = std::clamp(value, 0.f, 1.f); }
 // Host tempo and musical position at the start of the next render() call.
 void setTransport(double bpm, double ppqPosition, bool playing) { mutilator.setTransport(bpm, ppqPosition, playing); }

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
   const float revReverse = smoothed[P::RevReverse] * .01f, warpAmount = smoothed[P::Warp] * .01f;
   for (int i = 0; i < n; ++i) {
    float wl = 0.f, wr = 0.f;
    mutilator.process(l[i], r[i]);
    reverb.process(l[i], r[i], wl, wr);
    if (revReverse > 0.f) {
     float bl = 0.f, br = 0.f;
     reverseTail.process(wl, wr, bl, br);
     wl += revReverse * (bl * 1.2f - wl);
     wr += revReverse * (br * 1.2f - wr);
    }
    float ol = l[i] * dryGain + wl * wetGain;
    float orr = r[i] * dryGain + wr * wetGain;
    const float mid = .5f * (ol + orr), side = .5f * (ol - orr) * width;
    ol = (mid + side) * outGain;
    orr = (mid - side) * outGain;
    warp.process(ol, orr, warpAmount);
    l[i] = safety(ol);
    r[i] = safety(orr);
   }
   done += n;
   controlCountdown -= n;
  }
 }

 static float safety(float x) { return softCeiling(x); }

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
  c.voiceMix = pct(P::VoiceMix);
  c.vowel = pct(P::Vowel);
  c.formantRatio = detail::semitoneRatio(s[P::FormantShift]);
  c.materialB = choice(P::ResMaterialB);
  c.morph = pct(P::ResMorph);
  c.resTuneSt = s[P::ResTune];
  c.resKeytrack = pct(P::ResKeytrack);
  c.stretch = pct(P::ResStretch);
  c.bright = pct(P::ResBright);
  c.resStereo = pct(P::ResStereo) * .9f;
  c.resInput = pct(P::ResInput);
  c.rattle = pct(P::ResRattle);
  c.driveMode = choice(P::DriveMode);
  c.ringMix = pct(P::RingMix);
  c.ringSt = s[P::RingPitch];
  mutilator.setReverse(pct(P::Reverse));
  static const double divisions[7] = {0.0, 1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
  const int division = std::clamp(choice(P::StutterDiv), 0, 6);
  mutilator.configure(divisions[division], pct(P::StutterDepth), std::min(1.f, pct(P::Chaos) + tension * .3f),
                      pct(P::Crush), pct(P::Downsample));
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
 Mutilator mutilator;
 ReverseGrains reverseTail;
 TapeWarp warp;
 int controlCountdown = 0, lastNote = -1;
 float lfoPhase = 0.f, lfoValue = 0.f, bend = 0.f, modWheel = 0.f, pressure = 0.f, outGain = .5f;
 bool sustainPedal = false;
};

} // namespace inverso_abismo
