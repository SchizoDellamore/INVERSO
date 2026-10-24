#pragma once
// ENKI engine. Each voice plays two sources, mixes them (with optional ring
// modulation), runs them through a filter (low, band, high pass or a comb tuned
// to the note) and a drive. A global LFO, a rhythmic gate, glitch slices, a
// tape echo and a reverb follow. Sources:
//  Wave       sine > triangle > saw > square (CHAR), unison detune (MOTION)
//  FM         two-operator FM: ratio (CHAR, between steps it turns inharmonic), index (MOTION)
//  Noise      colour (CHAR), from broad to a narrow band on the note (MOTION)
//  Sample     a recording played as an instrument: start (CHAR), loop length (MOTION)
//  Grains     a cloud of grains from a recording: position (CHAR), size and scatter (MOTION)
//  Resonator  a modal body (24 materials, CHAR) struck or bowed (MOTION)
//  Reed       a free reed: pulse width (CHAR), breath (MOTION)
#include "AbismoCommon.h"
#include "AbismoEngine.h" // materialRatios
#include "AbismoMutilator.h"
#include "AbismoReverb.h"
#include "EnkiParams.h"
#include "RelicarioFx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace inverso_abismo::enki {


using inverso_abismo::detail::Random;


using inverso_abismo::relicario::DelayLine;
using inverso_abismo::relicario::Drive;

constexpr int maxVoices = 8;
constexpr int maxGrains = 8;
constexpr int resModes = 8;
constexpr float sampleRoot = 261.6256f; // recordings play at their own speed on middle C

// A mono recording the sources can play. Owned by the caller; must outlive its use.
struct Sample {
 std::vector<float> data;
 double sampleRate = 48000.0;
};
using SamplePool = std::array<const Sample*, numSamples>;

struct SourceSettings {
 int type = 0;
 float level = 0.f, pitchSt = 0.f, character = .5f, motion = .2f;
 const Sample* sample = nullptr;
};

inline float fracf(float x) { return x - std::floor(x); }
// 4-point Hermite read; outside the recording reads as silence.
inline float readHermite(const std::vector<float>& d, double index) {
 const int n = static_cast<int>(d.size());
 const auto i = static_cast<int>(std::floor(index));
 const float t = static_cast<float>(index - i);
 auto at = [&](int j) { return (j >= 0 && j < n) ? d[static_cast<std::size_t>(j)] : 0.f; };
 const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
 const float c1 = .5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - .5f * x2, c3 = .5f * (x2 - xm1) + 1.5f * (x0 - x1);
 return ((c3 * t + c2) * t + c1) * t + x0;
}
inline float hann(float t) { return .5f - .5f * std::cos(6.2831853f * std::clamp(t, 0.f, 1.f)); }

class Source {
public:
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 17u; }
 void start(const SourceSettings& s, float freq, double sampleRate) {
  sr = sampleRate;
  for (auto& p : phase) p = .5f * (rng.next() + 1.f);
  modPhase = 0.f;
  noiseLp = noiseLp2 = 0.f; band.reset();
  pos = s.sample && !s.sample->data.empty() ? static_cast<double>(std::clamp(s.character, 0.f, .95f)) * static_cast<double>(s.sample->data.size()) : 0.0;
  loopStart = pos;
  for (auto& g : grains) g.active = false;
  grainClock = 0.f;
  for (auto& m : modes) m.reset();
  strikeLeft = static_cast<int>(.003 * sr);
  reedLp = reedLp2 = 0.f;
  configure(s, freq);
 }
 // Called at control rate with the voice's current frequency.
 void configure(const SourceSettings& s, float freq) {
  settings = s;
  const auto srf = static_cast<float>(sr);
  inc = std::min(freq, .45f * srf) / srf;
  if (s.type == 5) {
   const int material = std::clamp(static_cast<int>(s.character * 23.99f), 0, numMaterials - 1);
   if (material != lastMaterial || std::abs(freq - lastFreq) > freq * .002f) {
    const auto& ratios = materialRatios(material);
    for (int k = 0; k < resModes; ++k) {
     const float f = freq * ratios[static_cast<std::size_t>(k)];
     const float t60 = (1.6f + 2.5f * (1.f - s.motion)) / (1.f + .35f * static_cast<float>(k)) * std::clamp(220.f / std::max(40.f, freq), .4f, 3.f);
     modes[static_cast<std::size_t>(k)].set(std::min(f, .45f * srf), t60, sr);
     modeAmp[static_cast<std::size_t>(k)] = 1.f / (1.f + .45f * static_cast<float>(k));
    }
    lastMaterial = material; lastFreq = freq;
   }
  }
  if (s.type == 2) {
   const float q = .5f + 40.f * s.motion * s.motion;
   const float g = std::tan(3.14159265f * std::min(inc, .45f));
   bandK = 1.f / q; bandA1 = 1.f / (1.f + g * (g + bandK)); bandA2 = g * bandA1; bandA3 = g * bandA2;
   noiseCoef = 1.f - std::exp(-6.2831853f * std::min(150.f * std::pow(120.f, s.character), .45f * srf) / srf);
  }
  if (s.type == 6) reedCoef = 1.f - std::exp(-6.2831853f * std::min(freq * (2.f + 9.f * s.motion), .45f * srf) / srf);
 }

 float next(float charMod) {
  const auto& s = settings;
  float x = 0.f;
  switch (s.type) {
   case 0: { // wave, up to three detuned copies
    const float shape = std::clamp(s.character + charMod, 0.f, 1.f) * 3.f;
    const float det = s.motion * s.motion * .02f; // up to ~35 cents
    const float incs[3] = {inc, inc * (1.f + det), inc * (1.f - det * .93f)};
    const int copies = s.motion > .02f ? 3 : 1;
    for (int u = 0; u < copies; ++u) {
     auto& ph = phase[static_cast<std::size_t>(u)];
     const float dt = incs[u];
     const float sine = detail::sine(ph);
     const float tri = 1.f - 4.f * std::abs(ph - .5f);
     const float saw = 2.f * ph - 1.f - detail::polyBlep(ph, dt);
     float t2 = ph + .5f; if (t2 >= 1.f) t2 -= 1.f;
     const float sq = (ph < .5f ? 1.f : -1.f) + detail::polyBlep(ph, dt) - detail::polyBlep(t2, dt);
     const int a = std::clamp(static_cast<int>(shape), 0, 2);
     const float f = shape - static_cast<float>(a);
     const float w[4] = {sine, tri, saw, sq};
     x += w[a] + f * (w[a + 1] - w[a]);
     ph += dt; if (ph >= 1.f) ph -= 1.f;
    }
    x /= copies > 1 ? 1.9f : 1.f;
    break;
   }
   case 1: { // FM
    static const float ratios[12] = {.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 8.f};
    const float c = std::clamp(s.character + charMod, 0.f, 1.f) * 10.99f;
    const int r = static_cast<int>(c);
    const float frac = c - static_cast<float>(r);
    const float ratio = ratios[r] * (1.f + .08f * frac * frac); // between steps it bends inharmonic
    // Keep the sidebands below Nyquist: high notes get a gentler index.
    const float maxIndex = std::max(0.f, .42f / std::max(1e-5f, inc * std::max(ratio, 1.f)) - 1.f);
    const float index = std::min(s.motion * 7.f * envIndex, maxIndex);
    const float mod = detail::sine(modPhase) * index;
    x = detail::sine(fracf(phase[0] + mod * .159155f));
    modPhase += inc * ratio; modPhase -= std::floor(modPhase);
    phase[0] += inc; if (phase[0] >= 1.f) phase[0] -= 1.f;
    break;
   }
   case 2: { // noise
    const float n = rng.next();
    noiseLp += noiseCoef * (n - noiseLp);
    noiseLp2 += noiseCoef * (noiseLp - noiseLp2);
    const float broad = noiseLp2 * 1.6f;
    const float narrow = band.process(n, bandA1, bandA2, bandA3, bandK, 1) * (1.f + 4.f * s.motion);
    x = (1.f - s.motion) * broad + s.motion * narrow;
    break;
   }
   case 3: { // sample
    if (!s.sample || s.sample->data.size() < 4) break;
    const auto& d = s.sample->data;
    const double rate = static_cast<double>(inc) * sr / sampleRoot * s.sample->sampleRate / sr;
    const double len = static_cast<double>(d.size());
    x = readHermite(d, pos);
    pos += rate;
    if (s.motion > .02f) { // loop a stretch from the start point, with a short fade at the seam
     const double loopLen = std::max(.03 * s.sample->sampleRate, (1.0 - s.motion) * (len - loopStart));
     const double into = pos - loopStart;
     const double fade = std::min(loopLen * .25, .02 * s.sample->sampleRate);
     if (into > loopLen - fade) {
      const float t = static_cast<float>((into - (loopLen - fade)) / fade);
      x = x * (1.f - t) + t * readHermite(d, pos - loopLen);
     }
     if (into >= loopLen) pos -= loopLen;
    }
    break;
   }
   case 4: { // grains
    if (!s.sample || s.sample->data.size() < 4) break;
    const auto& d = s.sample->data;
    const double len = static_cast<double>(d.size());
    const float size = (.02f + .3f * (1.f - s.motion)) * static_cast<float>(s.sample->sampleRate);
    grainClock -= 1.f;
    if (grainClock <= 0.f) {
     grainClock = static_cast<float>(sr) / 28.f * (.7f + .6f * .5f * (rng.next() + 1.f));
     for (auto& g : grains) if (!g.active) {
      const float spray = (.02f + .3f * s.motion) * rng.next();
      const float at = std::clamp(s.character + charMod + spray, 0.f, .98f);
      g.active = true; g.pos = at * len; g.age = 0.f; g.size = size;
      g.rate = static_cast<double>(inc) * sr / sampleRoot * s.sample->sampleRate / sr * (rng.next() < -.7f ? 2.0 : 1.0);
      g.pan = rng.next();
      break;
     }
    }
    for (auto& g : grains) {
     if (!g.active) continue;
     x += readHermite(d, g.pos) * hann(g.age / g.size);
     g.pos += g.rate; g.age += static_cast<float>(g.rate);
     if (g.age >= g.size || g.pos >= len) g.active = false;
    }
    x *= .55f;
    break;
   }
   case 5: { // resonator: struck at the start, bowed by noise as MOTION rises
    // A 3 ms mallet burst at the start; noise bows it as MOTION rises.
    float excite = strikeLeft > 0 ? 6.f * rng.next() * (1.f - s.motion * .7f) : 0.f;
    if (strikeLeft > 0) --strikeLeft;
    excite += s.motion * s.motion * .6f * rng.next();
    for (int k = 0; k < resModes; ++k) x += modes[static_cast<std::size_t>(k)].process(excite) * modeAmp[static_cast<std::size_t>(k)];
    x *= .5f;
    break;
   }
   default: { // reed
    const float duty = .08f + .4f * std::clamp(s.character + charMod, 0.f, 1.f);
    float t2 = phase[0] + 1.f - duty; if (t2 >= 1.f) t2 -= 1.f;
    const float p = (phase[0] < duty ? 1.f : -1.f) + detail::polyBlep(phase[0], inc) - detail::polyBlep(t2, inc) + (1.f - 2.f * duty);
    phase[0] += inc; if (phase[0] >= 1.f) phase[0] -= 1.f;
    reedLp += reedCoef * (p - reedLp);
    reedLp2 += reedCoef * (reedLp - reedLp2);
    x = reedLp2 * 1.4f + s.motion * .15f * rng.next();
    break;
   }
  }
  return x;
 }
 float envIndex = 1.f; // FM index follows the mod envelope

private:
 struct Grain { bool active = false; double pos = 0.0, rate = 1.0; float age = 0.f, size = 1.f, pan = 0.f; };
 SourceSettings settings;
 Random rng;
 double sr = 48000.0, pos = 0.0, loopStart = 0.0;
 std::array<float, 3> phase{};
 float inc = .01f, modPhase = 0.f;
 float noiseLp = 0.f, noiseLp2 = 0.f, noiseCoef = .5f, bandA1 = 1.f, bandA2 = 0.f, bandA3 = 0.f, bandK = 1.f;
 SVF band;
 std::array<Grain, maxGrains> grains{};
 float grainClock = 0.f;
 std::array<Resonator, resModes> modes{};
 std::array<float, resModes> modeAmp{};
 int lastMaterial = -1;
 float lastFreq = 0.f;
 int strikeLeft = 0;
 float reedLp = 0.f, reedLp2 = 0.f, reedCoef = .3f;
};

// What the engine hands every voice at control rate.
struct Controls {
 std::array<SourceSettings, 2> src;
 float ring = 0.f;
 int filterMode = 0, driveType = 0;
 float cutoff = 8000.f, resonance = .15f, filterEnv = 0.f, keytrack = .3f, drive = 0.f;
 float pitchEnv = 0.f, velSens = .5f, octaveSt = 0.f, glideMs = 0.f, bendRange = 2.f;
 bool hold = false;
 // Global modulation, already computed for this tick.
 float lfo = 0.f, lfoPitchCents = 0.f, lfoFilter = 0.f, lfoChar = 0.f;
};

class Voice {
public:
 void seed(std::uint32_t s) { srcs[0].seed(s * 3u + 1u); srcs[1].seed(s * 3u + 2u); rng.state = s * 7919u + 3u; }
 void start(int midiNote, float vel, float glideFrom, const Controls& c, double sampleRate) {
  sr = sampleRate;
  note = midiNote;
  velocity = vel;
  released = false;
  age = 0;
  glide = glideFrom;
  filterL.reset(); comb.assign(comb.size(), 0.f); combAt = 0;
  driveState.clear();
  modEnv.gateOn();
  amp.gateOn();
  pan = std::clamp((static_cast<float>(midiNote) - 60.f) / 48.f, -.5f, .5f) + .15f * rng.next();
  for (int k = 0; k < 2; ++k) srcs[static_cast<std::size_t>(k)].start(c.src[static_cast<std::size_t>(k)], frequency(c, k), sr);
 }
 void release() { if (!released) { released = true; amp.gateOff(); modEnv.gateOff(); } }
 bool active() const { return amp.active(); }
 int note = -1;
 bool released = false;
 long long age = 0;
 Envelope amp, modEnv;

 void control(const Controls& c, float bendSt) {
  bend = bendSt;
  const float dt = static_cast<float>(controlInterval / sr);
  if (c.glideMs > 0.f) {
   const float step = dt * 1000.f / c.glideMs * 12.f;
   glide = glide > 0.f ? std::max(0.f, glide - step) : std::min(0.f, glide + step);
  } else glide = 0.f;
  for (int k = 0; k < 2; ++k) {
   auto& s = srcs[static_cast<std::size_t>(k)];
   s.configure(c.src[static_cast<std::size_t>(k)], frequency(c, k));
   s.envIndex = .35f + .65f * modEnv.value();
  }
  // Filter cutoff: envelope, keytracking and the LFO, in octaves.
  const float oct = c.filterEnv * 6.f * modEnv.value() + c.keytrack * (static_cast<float>(note) - 60.f) / 12.f + c.lfoFilter * 3.f * c.lfo;
  const float hz = std::clamp(c.cutoff * std::exp2(oct), 20.f, static_cast<float>(sr) * .45f);
  if (c.filterMode == 3) {
   combDelay = std::clamp(static_cast<float>(sr) / std::max(30.f, frequency(c, 0)), 2.f, static_cast<float>(comb.size() - 2));
   combFb = .5f + .49f * c.resonance;
   combDamp = 1.f - std::exp(-6.2831853f * hz / static_cast<float>(sr));
  } else {
   const float g = std::tan(3.14159265f * hz / static_cast<float>(sr));
   fk = 2.f - 1.94f * c.resonance;
   fa1 = 1.f / (1.f + g * (g + fk)); fa2 = g * fa1; fa3 = g * fa2;
  }
 }

 // Adds this voice to the stereo mix.
 void render(const Controls& c, float* left, float* right, int n, float panLfo) {
  const float vel = 1.f - c.velSens + c.velSens * velocity;
  const float la = c.src[0].level, lb = c.src[1].level, ringLevel = std::sqrt(la * lb);
  for (int i = 0; i < n; ++i) {
   const float e = amp.next();
   modEnv.next();
   const float a = la > 0.f || c.ring > 0.f ? srcs[0].next(c.lfoChar * c.lfo) : 0.f;
   const float b = lb > 0.f || c.ring > 0.f ? srcs[1].next(c.lfoChar * c.lfo) : 0.f;
   // the ring product follows both levels: a source at zero silences it too
   float x = (1.f - c.ring) * (a * la + b * lb) + c.ring * a * b * 2.f * ringLevel;
   if (c.filterMode == 3) {
    const int size = static_cast<int>(comb.size());
    const float pos = static_cast<float>(combAt) - combDelay + static_cast<float>(size);
    const int i0 = static_cast<int>(pos) % size, i1 = (i0 + 1) % size;
    const float f = pos - std::floor(pos);
    const float y = comb[static_cast<std::size_t>(i0)] + f * (comb[static_cast<std::size_t>(i1)] - comb[static_cast<std::size_t>(i0)]);
    combLp += combDamp * (y - combLp);
    comb[static_cast<std::size_t>(combAt)] = detail::softTanh(x + combFb * combLp);
    combAt = (combAt + 1) % size;
    x = .5f * x + combLp;
   } else {
    x = filterL.process(x, fa1, fa2, fa3, fk, c.filterMode);
    if (c.filterMode != 0) x *= c.filterMode == 1 ? 1.4f : 1.f;
   }
   if (c.drive > 0.f) x = driveState.process(x, c.driveType, c.drive);
   x = detail::softTanh(x * .9f) * e * vel;
   const float p = std::clamp(pan + panLfo, -1.f, 1.f);
   left[i] += x * (1.f - p) * .7071f * 1.2f;
   right[i] += x * (1.f + p) * .7071f * 1.2f;
  }
  age += n;
 }

private:
 float frequency(const Controls& c, int k) const {
  const auto& s = c.src[static_cast<std::size_t>(k)];
  const float st = static_cast<float>(note - 69) + c.octaveSt + s.pitchSt + glide + bend
                   + c.pitchEnv * modEnv.value() + c.lfoPitchCents * c.lfo * .01f;
  return 440.f * detail::semitoneRatio(st);
 }
 std::array<Source, 2> srcs;
 Random rng;
 double sr = 48000.0;
 float velocity = 1.f, glide = 0.f, bend = 0.f, pan = 0.f;
 SVF filterL;
 float fa1 = 1.f, fa2 = 0.f, fa3 = 0.f, fk = 2.f;
 std::vector<float> comb = std::vector<float>(4096, 0.f);
 int combAt = 0;
 float combDelay = 100.f, combFb = .5f, combDamp = .5f, combLp = 0.f;
 Drive driveState;
};

class Engine {
public:
 Engine() { pool.fill(nullptr); }
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  reverb.prepare(sr);
  chop.prepare(sr);
  resampler.prepare(sr);
  chop.setLayeredRepeats(false);
  echoL.prepare(static_cast<int>(sr * 1.4)); echoR.prepare(static_cast<int>(sr * 1.4));
  for (int i = 0; i < maxVoices; ++i) { voices[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 1)); voices[static_cast<std::size_t>(i)].amp.reset(); voices[static_cast<std::size_t>(i)].modEnv.reset(); }
  smoothed = target;
  controlCountdown = 0;
  lfoPhase = 0.f; gateLevel = 1.f; beat = 0.0;
  derive();
 }
 void setValues(const Values& v) { target = v; }
 // Recordings the sources may play (index 0-47 library, 48-63 user slots).
 void setSample(int index, const Sample* s) {
  if (index < 0 || index >= numSamples || pool[static_cast<std::size_t>(index)] == s) return;
  // A source still reading the old memory must stop first.
  for (auto& v : voices) v.amp.reset();
  pool[static_cast<std::size_t>(index)] = s;
 }
 const Sample* sample(int index) const { return index >= 0 && index < numSamples ? pool[static_cast<std::size_t>(index)] : nullptr; }
 void setTransport(double bpm, double ppq, bool playing) {
  chop.setTransport(bpm, ppq, playing);
  if (std::isfinite(bpm) && bpm >= 20.0 && bpm <= 999.0) tempo = bpm;
  if (playing && std::isfinite(ppq)) beat = ppq;
 }
 void setPitchBend(float v) { bendNorm = std::clamp(v, -1.f, 1.f); }
 void setModWheel(float v) { modWheel = std::clamp(v, 0.f, 1.f); }
 void setPressure(float v) { pressure = std::clamp(v, 0.f, 1.f); }
 void setSustain(bool down) {
  sustainPedal = down;
  if (!down) for (std::size_t i = 0; i < voices.size(); ++i) if (sustained[i]) { sustained[i] = false; voices[i].release(); }
 }

 void noteOn(int note, float velocity) {
  if (velocity <= 0.f) { noteOff(note); return; }
  derive();
  Voice* chosen = nullptr;
  for (auto& v : voices) if (!v.active()) { chosen = &v; break; }
  if (!chosen) for (auto& v : voices) if (!chosen || (v.released && !chosen->released) || (v.released == chosen->released && v.age > chosen->age)) chosen = &v;
  const float glideFrom = controls.glideMs > 0.f && lastNote >= 0 ? static_cast<float>(lastNote - note) : 0.f;
  chosen->start(note, std::clamp(velocity, 0.f, 1.f), glideFrom, controls, sr);
  chosen->control(controls, bendNorm * controls.bendRange);
  sustained[static_cast<std::size_t>(chosen - voices.data())] = false;
  lastNote = note;
 }
 void noteOff(int note) {
  if (controls.hold) return; // one-shot sounds play out
  for (std::size_t i = 0; i < voices.size(); ++i) {
   auto& v = voices[i];
   if (!v.active() || v.note != note || v.released) continue;
   if (sustainPedal) sustained[i] = true; else v.release();
  }
 }
 void allNotesOff() { for (auto& v : voices) v.release(); }
 void allSoundOff() {
  for (auto& v : voices) { v.amp.reset(); v.modEnv.reset(); }
  reverb.clear(); echoL.clear(); echoR.clear(); chop.clear();
  echoLpL = echoLpR = echoTail = 0.f;
 }
 int activeVoices() const { int n = 0; for (const auto& v : voices) n += v.active() ? 1 : 0; return n; }
 float lfoValue() const { return controls.lfo; }

 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   float* l = left + done;
   float* r = right + done;
   std::fill(l, l + n, 0.f);
   std::fill(r, r + n, 0.f);
   for (auto& v : voices) if (v.active()) v.render(controls, l, r, n, panLfo);
   const auto& s = smoothed;
   const float echoMix = s[P::EchoMix] * .01f, fb = s[P::EchoFeedback] * .01f;
   const float revMix = s[P::RevMix] * .01f;
   const float dry = std::cos(revMix * 1.5707963f), wet = std::sin(revMix * 1.5707963f) * 1.3f;
   const float width = s[P::Width] * .01f;
   for (int i = 0; i < n; ++i) {
    float a = l[i] * ampLfo * gateLevel, b = r[i] * ampLfo * gateLevel;
    gateLevel += gateCoef * (gateTarget - gateLevel);
    if (resampler.enabled()) resampler.process(a, b);
    chop.process(a, b);
    if (echoMix > 0.f || echoTail > 1e-5f) {
     const float el = echoL.read(echoSamples), er = echoR.read(echoSamples * 1.02f);
     echoLpL += .3f * (el - echoLpL); echoLpR += .3f * (er - echoLpR);
     echoL.push(detail::softTanh(a * echoMix + echoLpR * fb));
     echoR.push(detail::softTanh(b * echoMix + echoLpL * fb));
     a += echoLpL; b += echoLpR;
     echoTail = std::max(std::abs(echoLpL), echoTail * .9999f);
    } else { echoL.push(0.f); echoR.push(0.f); }
    float wl = 0.f, wr = 0.f;
    reverb.process(a, b, wl, wr);
    a = a * dry + wl * wet; b = b * dry + wr * wet;
    const float mid = .5f * (a + b), side = .5f * (a - b) * width;
    l[i] = softCeiling((mid + side) * outGain);
    r[i] = softCeiling((mid - side) * outGain);
   }
   done += n;
   controlCountdown -= n;
  }
 }

private:
 void controlTick() {
  const auto& table = specs();
  for (std::size_t i = 0; i < target.size(); ++i) {
   if (table[i].kind == Kind::Float) smoothed[i] += .15f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  derive();
  const auto& s = smoothed;
  const float dt = static_cast<float>(controlInterval / sr);
  beat += dt * tempo / 60.0;
  // LFO: free in Hz or locked to bars and notes.
  static const float syncBeats[7] = {0.f, 8.f, 4.f, 2.f, 1.f, .5f, .25f};
  const int sync = std::clamp(static_cast<int>(std::lround(s[P::LfoSync])), 0, 6);
  if (sync > 0) lfoPhase = static_cast<float>(std::fmod(beat / syncBeats[sync], 1.0));
  else { lfoPhase += dt * s[P::LfoRate]; lfoPhase -= std::floor(lfoPhase); }
  const int shape = std::clamp(static_cast<int>(std::lround(s[P::LfoShape])), 0, 4);
  if (lfoPhase < lastLfoPhase) { heldRandom = rng.next(); }
  lastLfoPhase = lfoPhase;
  drift += .02f * (heldRandom - drift);
  float lfo = 0.f;
  switch (shape) {
   case 0: lfo = detail::sine(lfoPhase); break;
   case 1: lfo = 1.f - 4.f * std::abs(lfoPhase - .5f); break;
   case 2: lfo = lfoPhase < .5f ? 1.f : -1.f; break;
   case 3: lfo = heldRandom; break;
   default: lfo = drift; break;
  }
  lfoSmooth += (shape == 2 || shape == 3 ? .5f : 1.f) * (lfo - lfoSmooth);
  controls.lfo = lfoSmooth;
  ampLfo = 1.f - s[P::LfoAmp] * .01f * (.5f + .5f * lfoSmooth);
  panLfo = s[P::LfoPan] * .01f * lfoSmooth * .9f;
  // Rhythmic gate: 16 steps from the pattern's 8 bits, played with the host tempo.
  static const double gateBeats[4] = {.5, .25, 1.0 / 3.0, .125};
  const int div = std::clamp(static_cast<int>(std::lround(s[P::GateDiv])), 0, 3);
  const double step = beat / gateBeats[div];
  const int idx = static_cast<int>(std::floor(step)) & 15;
  const int pattern = static_cast<int>(std::lround(s[P::GatePattern])) & 255;
  const bool on = ((pattern >> (idx & 7)) & 1) != 0 || idx == 0;
  const float depth = s[P::Gate] * .01f;
  const float inStep = static_cast<float>(step - std::floor(step));
  gateTarget = 1.f - depth * (on && inStep < .8f ? 0.f : 1.f);
  for (std::size_t i = 0; i < voices.size(); ++i) if (voices[i].active()) voices[i].control(controls, bendNorm * controls.bendRange);
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  Controls& c = controls;
  for (int k = 0; k < 2; ++k) {
   const int b = k * P::SourceStride;
   auto& src = c.src[static_cast<std::size_t>(k)];
   src.type = std::clamp(choice(P::AType + b), 0, numSourceTypes - 1);
   src.level = pct(P::ALevel + b);
   src.pitchSt = s[static_cast<std::size_t>(P::APitch + b)] + s[static_cast<std::size_t>(P::AFine + b)] * .01f;
   src.character = pct(P::AChar + b);
   src.motion = std::min(1.f, pct(P::AMotion + b) + (k == 0 ? modWheel * .5f : 0.f));
   src.sample = pool[static_cast<std::size_t>(std::clamp(choice(P::ASample + b), 0, numSamples - 1))];
   if (!src.sample) src.sample = pool[static_cast<std::size_t>(choice(P::ASample + b) % numLibrarySamples)];
  }
  c.ring = pct(P::Ring);
  c.filterMode = std::clamp(choice(P::FilterMode), 0, 3);
  c.cutoff = s[P::Cutoff] * std::exp2(pressure * 2.f);
  c.resonance = pct(P::Resonance);
  c.filterEnv = pct(P::FilterEnv);
  c.keytrack = pct(P::Keytrack);
  c.drive = pct(P::Drive);
  c.driveType = std::clamp(choice(P::DriveType), 0, 4);
  c.pitchEnv = s[P::PitchEnv];
  c.hold = s[P::Hold] >= .5f;
  c.velSens = pct(P::VelSens);
  c.octaveSt = static_cast<float>((choice(P::Octave) - 2) * 12);
  c.glideMs = s[P::Glide];
  c.bendRange = s[P::BendRange];
  c.lfoPitchCents = s[P::LfoPitch];
  c.lfoFilter = pct(P::LfoFilter);
  c.lfoChar = pct(P::LfoChar) * .5f;
  ampShape.set(s[P::Attack], s[P::Decay], c.hold ? std::min(pct(P::Sustain), .0f) : pct(P::Sustain), s[P::Release], sr);
  modShape.set(s[P::ModAttack], s[P::ModDecay], 0.f, s[P::ModDecay], sr);
  for (auto& v : voices) { v.amp.copySettings(ampShape); v.modEnv.copySettings(modShape); }
  reverb.configure(pct(P::RevSize), s[P::RevDecay], .45f, false);
  static const double echoBeats[7] = {0.0, 1.0, .5, .75, .25, 2.0 / 3.0, 1.0 / 3.0};
  const int es = std::clamp(choice(P::EchoSync), 0, 6);
  echoSamples = static_cast<float>(std::min(es > 0 ? echoBeats[es] * 60.0 / tempo : s[P::EchoTime] * .001, 1.35) * sr);
  const float glitch = pct(P::Glitch);
  chop.setReverse(pct(P::Reverse));
  chop.configure(glitch > 0.f ? (glitch > .6f ? .125 : .25) : 0.0, .35f * glitch, glitch, 0.f, 0.f);
  resampler.set(s[P::ResampleOn] >= .5f ? choice(P::Resample) : 0, s[P::ResampleOn] >= .5f ? pct(P::ResampleSat) : 0.f);
  gateCoef = 1.f - std::exp(-1.f / (.002f * static_cast<float>(sr)));
  outGain = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0, tempo = 120.0, beat = 0.0;
 Values target = defaults(), smoothed = defaults();
 Controls controls;
 Envelope ampShape, modShape;
 std::array<Voice, maxVoices> voices{};
 std::array<bool, maxVoices> sustained{};
 SamplePool pool;
 Reverb reverb;
 Mutilator chop;
 Resampler resampler;
 DelayLine echoL, echoR;
 Random rng;
 float echoSamples = 18000.f, echoLpL = 0.f, echoLpR = 0.f, echoTail = 0.f;
 float lfoPhase = 0.f, lastLfoPhase = 0.f, heldRandom = 0.f, drift = 0.f, lfoSmooth = 0.f, ampLfo = 1.f, panLfo = 0.f;
 float gateLevel = 1.f, gateTarget = 1.f, gateCoef = .01f;
 float outGain = .5f, bendNorm = 0.f, modWheel = 0.f, pressure = 0.f;
 int controlCountdown = 0, lastNote = -1;
 bool sustainPedal = false;
};

} // namespace inverso_abismo::enki
