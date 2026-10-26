#pragma once
#include <cmath>
#include <algorithm>
// ABISMO DRUMS engine. Portable C++17, no JUCE, no allocation after prepare().
// Every hit is synthesised: a strike / click-train exciter drives a modal body
// (up to 8 modes), an optional pitch-dropping membrane, a noise layer and a
// "rattle" of loose parts that follows the body's loudness. Each hit is varied
// (tune, decay, level, mode spacing) so no two hits are identical.
// Notes >= 60 play a monophonic musical saw with bowing, vibrato and glide.
// Global: stumble (random late hits), megaphone, gramophone, stutter / crush,
// small-room FDN reverb, output and soft ceiling.
#include "AbismoCommon.h"
#include "AbismoMutilator.h"
#include "AbismoReverb.h"
#include "DrumsParams.h"
#include "DrumsTrim.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace inverso_abismo::drums {

constexpr int maxHits = 32;
constexpr int maxPending = 64;

struct PadSettings {
 int object = 0;
 float tune = 0.f, decay = 1.f, tone = .5f, rattle = 0.f, rust = .2f, variation = .35f, gain = 1.f, pan = 0.f;
};

constexpr int maxModes = 48;
constexpr int maxLoose = 10;
constexpr int maxEvents = 16;

// Mode-frequency ratio k (0-based) for a shape: the physics of plates, bars,
// shells, heads and wood, with a little of the object's own irregularity.
inline float shapeRatio(Shape shape, int k, detail::Random& r) {
 static const float bessel[16] = {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f,
                                  3.600f, 3.652f, 4.060f, 4.154f, 4.601f, 4.832f, 4.903f, 5.131f};
 static const float freeBar[8] = {1.f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.81f, 31.87f};
 static const float ring[6] = {1.f, 2.83f, 5.43f, 8.78f, 12.9f, 17.8f};
 static const float axial[5] = {1.f, 1.13f, 1.34f, 1.62f, 1.95f};
 const float u = r.next();
 switch (shape) {
  case Shape::Membrane: return (k < 16 ? bessel[k] : bessel[15] + .32f * static_cast<float>(k - 15)) * (k ? 1.f + .015f * u : 1.f);
  case Shape::Plate: return k ? (1.f + .55f * std::pow(static_cast<float>(k), 1.35f)) * (1.f + .04f * u) : 1.f;
  case Shape::Bar: {
   // Transverse bending modes, with torsional modes between them.
   const float base = freeBar[std::min(k / 2, 7)] * (k / 2 >= 8 ? 1.f + .5f * static_cast<float>(k / 2 - 7) : 1.f);
   return (k % 2 ? base * 1.47f : base) * (k ? 1.f + .01f * u : 1.f);
  }
  case Shape::Shell: return k ? ring[k % 6] * axial[(k / 6) % 5] * (1.f + .02f * u) : 1.f;
  case Shape::Wood: return k ? (1.f + .9f * std::pow(static_cast<float>(k), 1.1f)) * (1.f + .08f * u) : 1.f;
  case Shape::Irregular: return k ? std::exp(.5f * (u + 1.f) * std::log(25.f)) : 1.f;
 }
 return 1.f;
}

// A set of recordings for one object, ordered soft -> loud.
struct SampleSet {
 std::string name;
 double sampleRate = 44100.0;
 std::vector<std::vector<float>> layers;
};

// The embedded library (Library/drums.bin). Parsed once; never touched by the audio thread.
struct SampleLibrary {
 std::vector<SampleSet> objects;
 static bool parse(const unsigned char* data, std::size_t size, SampleLibrary& out) {
  std::size_t o = 0;
  auto need = [&](std::size_t n) { return o + n <= size; };
  auto u16 = [&] { const unsigned v = data[o] | (data[o + 1] << 8); o += 2; return v; };
  auto u32 = [&] {
   const std::uint32_t v = static_cast<std::uint32_t>(data[o]) | (static_cast<std::uint32_t>(data[o + 1]) << 8) |
                           (static_cast<std::uint32_t>(data[o + 2]) << 16) | (static_cast<std::uint32_t>(data[o + 3]) << 24);
   o += 4;
   return v;
  };
  if (!data || !need(16) || std::memcmp(data, "ABDL", 4) != 0) return false;
  o = 4;
  if (u32() != 1) return false;
  const double rate = u32();
  const std::uint32_t count = u32();
  out.objects.clear();
  for (std::uint32_t i = 0; i < count; ++i) {
   if (!need(2)) return false;
   const unsigned nameLen = u16();
   if (!need(nameLen + 2)) return false;
   SampleSet set;
   set.name.assign(reinterpret_cast<const char*>(data + o), nameLen);
   o += nameLen;
   set.sampleRate = rate;
   const unsigned layers = u16();
   for (unsigned l = 0; l < layers; ++l) {
    if (!need(4)) return false;
    const std::uint32_t frames = u32();
    if (!need(static_cast<std::size_t>(frames) * 2)) return false;
    std::vector<float> pcm(frames);
    for (std::uint32_t k = 0; k < frames; ++k) {
     const auto v = static_cast<std::int16_t>(data[o] | (data[o + 1] << 8));
     pcm[k] = static_cast<float>(v) / 32768.f;
     o += 2;
    }
    set.layers.push_back(std::move(pcm));
   }
   out.objects.push_back(std::move(set));
  }
  return true;
 }

 // Perceived loudness of a hit: K-weighted (a high pass under the low bass, a
 // lift above 1.5 kHz, as in ITU-R BS.1770) and the loudest 100 ms of it.
 static double loudness(const std::vector<float>& x, double sr) {
  struct Biquad { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
   double run(double v) { const double y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; return y; } };
  const double pi = 3.14159265358979323846;
  auto shelf = [&] { // RBJ high shelf, +4 dB at 1681 Hz
   const double A = std::pow(10.0, 4.0 / 40.0), w = 2 * pi * 1681.0 / sr, cs = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0);
   const double a0 = (A + 1) - (A - 1) * cs + 2 * std::sqrt(A) * al;
   return Biquad{A * ((A + 1) + (A - 1) * cs + 2 * std::sqrt(A) * al) / a0, -2 * A * ((A - 1) + (A + 1) * cs) / a0,
                 A * ((A + 1) + (A - 1) * cs - 2 * std::sqrt(A) * al) / a0, 2 * ((A - 1) - (A + 1) * cs) / a0,
                 ((A + 1) - (A - 1) * cs - 2 * std::sqrt(A) * al) / a0};
  };
  auto highPass = [&] { // RBJ high pass at 38 Hz
   const double w = 2 * pi * 38.0 / sr, cs = std::cos(w), al = std::sin(w) / (2 * .5), a0 = 1 + al;
   return Biquad{(1 + cs) / 2 / a0, -(1 + cs) / a0, (1 + cs) / 2 / a0, -2 * cs / a0, (1 - al) / a0};
  };
  Biquad k1 = shelf(), k2 = highPass();
  const auto win = std::max<std::size_t>(1, static_cast<std::size_t>(.1 * sr));
  std::vector<double> sq(x.size());
  for (std::size_t i = 0; i < x.size(); ++i) { const double y = k2.run(k1.run(x[i])); sq[i] = y * y; }
  double sum = 0, best = 0;
  for (std::size_t i = 0; i < sq.size(); ++i) {
   sum += sq[i];
   if (i >= win) sum -= sq[i - win];
   best = std::max(best, sum / static_cast<double>(std::min(win, i + 1)));
  }
  return best;
 }

 // Brings every object to the same perceived loudness (the median of the set),
 // the same gain on all its velocity layers so their relation stays; a hit
 // never goes past -0.5 dBFS.
 void equaliseLoudness() {
  std::vector<double> level(objects.size());
  for (std::size_t i = 0; i < objects.size(); ++i)
   level[i] = objects[i].layers.empty() ? 0.0 : loudness(objects[i].layers.back(), objects[i].sampleRate);
  std::vector<double> sorted;
  for (double l : level) if (l > 0) sorted.push_back(l);
  if (sorted.empty()) return;
  std::sort(sorted.begin(), sorted.end());
  const double target = sorted[sorted.size() / 2];
  for (std::size_t i = 0; i < objects.size(); ++i) {
   if (level[i] <= 0) continue;
   float peak = 0.f;
   for (const auto& layer : objects[i].layers) for (float v : layer) peak = std::max(peak, std::abs(v));
   double gain = std::clamp(std::sqrt(target / level[i]), .125, 8.0);
   if (peak > 0.f) gain = std::min(gain, .944 / peak);
   for (auto& layer : objects[i].layers) for (auto& v : layer) v = static_cast<float>(v * gain);
  }
 }
};

class Hit {
public:
 void seed(std::uint32_t s) { rng.state = s * 2654435761u + 99u; }
 bool active() const { return alive; }
 long long age = 0;
 int pad = -1;

 void start(int padIndex, const PadSettings& p, const std::array<float, 8>& rustOffsets, float velocity,
            float velSens, double sampleRate) {
  sr = sampleRate;
  pad = padIndex;
  alive = true;
  age = 0;
  elapsed = 0;
  quiet = 0;
  sample = nullptr;
  const int objectIndex = isSampleObject(p.object) ? synthStandIn(p.object) : synthObject(p.object);
  const auto& m = objects()[static_cast<std::size_t>(objectIndex)];
  const float v = p.variation;
  // The pad's own random identity (fixed) and this hit's randomness.
  detail::Random fixed;
  fixed.state = 0x5EED0000u + static_cast<std::uint32_t>(padIndex * 131 + objectIndex * 7);
  const float tuneRatio = detail::semitoneRatio(p.tune + v * .4f * rng.next()); // +-0.4 st
  const float decayMul = p.decay * (1.f + .2f * v * rng.next());
  const float velScale = 1.f - velSens + velSens * velocity;
  gain = p.gain * std::pow(10.f, 2.f * v * rng.next() / 20.f) * velScale * velScale * m.gain;
  const float rad = (std::clamp(p.pan, -1.f, 1.f) + 1.f) * .785398f;
  panL = std::cos(rad) * 1.4142f;
  panR = std::sin(rad) * 1.4142f;
  const float hardness = std::clamp(.6f * p.tone + .4f * velocity, 0.f, 1.f);
  static const float pieceMul[3] = {1.f, 1.37f, .79f};
  const float nyquist = std::min(14000.f, .45f * static_cast<float>(sr));
  modeCount = 0;
  float longest = .05f;
  for (int k = 0; k < std::min(m.modes, maxModes); ++k) {
   const int piece = k % std::max(1, m.pieces);
   float ratio = shapeRatio(m.shape, k / std::max(1, m.pieces), fixed) * pieceMul[piece];
   if (k) ratio *= 1.f + p.rust * .06f * rustOffsets[static_cast<std::size_t>(k % 8)];
   ratio *= 1.f + v * .006f * rng.next();
   const float f = m.f0 * tuneRatio * ratio;
   if (f >= nyquist) continue;
   const float t60 = std::max(.008f, m.t60 * decayMul * std::pow(std::max(1.f, ratio), -m.dampPow) * (1.f + .2f * fixed.next()));
   const float fixedAmp = .35f + .65f * std::abs(fixed.next());
   const float hitAmp = .35f + .65f * std::abs(rng.next());
   float amp = (fixedAmp * (1.f - .6f * v) + hitAmp * .6f * v) * std::pow(std::max(1.f, ratio), -m.tilt);
   if (m.shape == Shape::Bar && (k / std::max(1, m.pieces)) % 2) amp *= .4f; // torsional modes are weaker
   auto& mode = modes[static_cast<std::size_t>(modeCount++)];
   mode.res.reset();
   mode.res.set(f, t60, sr);
   mode.amp = amp;
   mode.piece = piece;
   longest = std::max(longest, t60);
  }
  // Loose part: a small rattling piece (rivets, snare wires, a crack).
  rattle = p.rattle;
  for (int k = 0; k < maxLoose; ++k) {
   const float ratio = shapeRatio(Shape::Plate, k, fixed);
   const float f = std::min(m.looseF * tuneRatio * ratio, nyquist - 1.f);
   loose[static_cast<std::size_t>(k)].reset();
   loose[static_cast<std::size_t>(k)].set(f, .03f / std::sqrt(ratio), sr);
   looseAmp[static_cast<std::size_t>(k)] = (.4f + .6f * std::abs(fixed.next())) / (1.f + .3f * static_cast<float>(k));
  }
  follower = 0.f;
  // Contact events: the strike, its bounces, and for bundles a shaken train.
  eventCount = 0;
  pulseLen = std::max(1, static_cast<int>(m.contactMs * (1.4f - .7f * hardness) * (1.f + .25f * v * rng.next()) * .001f * static_cast<float>(sr)));
  auto addEvent = [&](double t, float a, int piece) {
   if (eventCount >= maxEvents) return;
   events[static_cast<std::size_t>(eventCount++)] = {static_cast<long long>(t * sr), a, piece};
  };
  addEvent(0.0, 1.f, 0);
  double t = 0.0, interval = m.impactRate > 0.f ? 1.0 / m.impactRate : 0.0;
  for (int i = 1; i < m.impacts; ++i) {
   const double u = .5 * (rng.next() + 1.f);
   t += interval * (.4 + 1.2 * u);
   interval *= m.impactDecel;
   addEvent(t, .3f + .7f * std::abs(rng.next()), static_cast<int>(.5f * (rng.next() + 1.f) * static_cast<float>(m.pieces)) % std::max(1, m.pieces));
  }
  if (m.bounces > 0) {
   double bt = m.bounceMs * .001 * (.7 + .3 * (rng.next() + 1.f));
   addEvent(bt, .35f, 0);
   if (m.bounces > 1) addEvent(bt * 1.6, .12f, 0);
  }
  for (auto& pc : piecePulse) pc = 0.f;
  memLevel = m.membrane;
  memFreq = m.memBase * tuneRatio;
  memStart = m.memStart;
  memDrop = static_cast<float>(std::exp(-1.0 / std::max(1.0, m.memDropMs * .001 * sr)));
  memDecay = static_cast<float>(std::exp(-6.9078 / std::max(1.0, m.memDecay * decayMul * sr)));
  memEnv = 1.f; memPhase = 0.f; memDropState = 1.f;
  if (memLevel > 0.f) longest = std::max(longest, m.memDecay * decayMul);
  lifetime = static_cast<long long>((longest * 1.2f + static_cast<float>(t) + .05f) * static_cast<float>(sr));
  const float cutoff = std::clamp(2000.f * std::exp2(p.tone * 3.f), 200.f, .45f * static_cast<float>(sr));
  const float g = std::tan(3.14159265f * cutoff / static_cast<float>(sr));
  toneA1 = 1.f / (1.f + g * (g + 1.2f)); toneA2 = g * toneA1; toneA3 = g * toneA2;
  toneFilter.reset();
 }

 // A recording: velocity picks the dynamic layer, never the same take twice in a row.
 void startSample(int padIndex, const PadSettings& p, const SampleSet& set, int& lastLayer, float velocity,
                  float velSens, double sampleRate) {
  sr = sampleRate;
  pad = padIndex;
  age = 0;
  elapsed = 0;
  quiet = 0;
  const int n = static_cast<int>(set.layers.size());
  if (n == 0) { alive = false; return; }
  const float v = p.variation;
  const float shaped = std::pow(std::clamp(velocity, 0.f, 1.f), .8f) * static_cast<float>(n - 1);
  int layer = std::clamp(static_cast<int>(std::lround(shaped + v * .25f * static_cast<float>(n) * rng.next())), 0, n - 1);
  if (layer == lastLayer && n > 1) layer = layer > 0 && (rng.next() < 0.f || layer == n - 1) ? layer - 1 : layer + 1;
  lastLayer = layer;
  sample = &set.layers[static_cast<std::size_t>(layer)];
  alive = true;
  pos = 0.0;
  rate = static_cast<double>(detail::semitoneRatio(p.tune + v * .3f * rng.next())) * set.sampleRate / sr;
  // Layers carry the dynamics, so velocity scales level only gently on top.
  const float velScale = 1.f - velSens * .45f * (1.f - velocity);
  gain = p.gain * std::pow(10.f, 2.f * v * rng.next() / 20.f) * velScale;
  const float rad = (std::clamp(p.pan, -1.f, 1.f) + 1.f) * .785398f;
  panL = std::cos(rad) * 1.4142f;
  panR = std::sin(rad) * 1.4142f;
  // DECAY below 100% fades the recording out sooner; above it keeps its natural length.
  const double seconds = static_cast<double>(sample->size()) / set.sampleRate / std::max(1e-6, rate * sr / set.sampleRate);
  envMul = p.decay < .999f ? static_cast<float>(std::exp(-1.0 / std::max(1.0, seconds * p.decay * .35 * sr))) : 1.f;
  env = 1.f;
  // TONE: below 50% a low-pass closes in, above 50% a high-pass thins it out.
  toneMode = p.tone < .5f ? 0 : 2;
  const float cutoff = p.tone < .5f ? 1200.f * std::exp2(p.tone * 2.f * 3.9f) : 20.f * std::exp2((p.tone - .5f) * 2.f * 6.f);
  const float g = std::tan(3.14159265f * std::clamp(cutoff, 20.f, .45f * static_cast<float>(sr)) / static_cast<float>(sr));
  toneA1 = 1.f / (1.f + g * (g + 1.2f)); toneA2 = g * toneA1; toneA3 = g * toneA2;
  toneFilter.reset();
  rust = p.rust;
  rattle = p.rattle;
  detail::Random fixed;
  fixed.state = 0x5EED0000u + static_cast<std::uint32_t>(padIndex * 131 + 977);
  for (int k = 0; k < maxLoose; ++k) {
   const float ratio = shapeRatio(Shape::Plate, k, fixed);
   loose[static_cast<std::size_t>(k)].reset();
   loose[static_cast<std::size_t>(k)].set(std::min(2400.f * ratio, .45f * static_cast<float>(sr)), .03f / std::sqrt(ratio), sr);
   looseAmp[static_cast<std::size_t>(k)] = (.4f + .6f * std::abs(fixed.next())) / (1.f + .3f * static_cast<float>(k));
  }
  follower = 0.f;
 }

 void render(float* outL, float* outR, int n) {
  if (!alive) return;
  if (sample) { renderSample(outL, outR, n); return; }
  const float pulseAmp = 3.14159265f / (2.f * static_cast<float>(pulseLen)); // unit impulse area
  for (int i = 0; i < n; ++i) {
   // Half-sine contact force for every event that is touching right now.
   piecePulse[0] = piecePulse[1] = piecePulse[2] = 0.f;
   for (int e = 0; e < eventCount; ++e) {
    const auto& ev = events[static_cast<std::size_t>(e)];
    const long long d = elapsed - ev.at;
    if (d >= 0 && d < pulseLen)
     piecePulse[static_cast<std::size_t>(ev.piece)] += ev.amp * pulseAmp * detail::sine(.5f * static_cast<float>(d) / static_cast<float>(pulseLen));
   }
   float body = 0.f;
   for (int k = 0; k < modeCount; ++k) {
    auto& mode = modes[static_cast<std::size_t>(k)];
    body += mode.res.process(piecePulse[static_cast<std::size_t>(mode.piece)]) * mode.amp;
   }
   body *= 2.f;
   float mem = 0.f;
   if (memLevel > 0.f) {
    memDropState *= memDrop;
    memPhase += memFreq * (1.f + (memStart - 1.f) * memDropState) / static_cast<float>(sr);
    if (memPhase >= 1.f) memPhase -= 1.f;
    mem = detail::sine(memPhase) * memEnv * memLevel;
    memEnv *= memDecay;
   }
   // Loose part: sparse collisions whose rate follows how hard the body moves.
   float rat = 0.f;
   if (rattle > 0.f) {
    const float a = std::abs(body) + std::abs(mem);
    follower += (a > follower ? .02f : .002f) * (a - follower);
    float hit = 0.f;
    const float p = rattle * std::min(1.f, follower * 30.f) * .2f;
    if (.5f * (rng.next() + 1.f) < p) hit = follower * (.5f + std::abs(rng.next())) * 6.f;
    for (int k = 0; k < maxLoose; ++k) rat += loose[static_cast<std::size_t>(k)].process(hit) * looseAmp[static_cast<std::size_t>(k)];
    rat *= rattle;
   }
   const float x = toneFilter.process(body + mem + rat, toneA1, toneA2, toneA3, 1.2f, 0) * gain;
   outL[i] += x * panL;
   outR[i] += x * panR;
   ++elapsed;
   quiet = std::abs(x) < 2e-6f ? quiet + 1 : 0;
  }
  age += n;
  if (elapsed > lifetime || (elapsed > sr * .1 && quiet > static_cast<long long>(sr * .05))) alive = false;
 }
 void stop() { alive = false; }

private:
 void renderSample(float* outL, float* outR, int n) {
  const auto& buf = *sample;
  const auto len = static_cast<double>(buf.size());
  for (int i = 0; i < n; ++i) {
   float x = 0.f;
   if (pos < len - 1.0) {
    // 4-point Hermite read; tape-like varispeed for TUNE.
    const auto k = static_cast<std::size_t>(pos);
    const float t = static_cast<float>(pos - static_cast<double>(k));
    const float xm1 = k > 0 ? buf[k - 1] : buf[k], x0 = buf[k], x1 = buf[k + 1], x2 = k + 2 < buf.size() ? buf[k + 2] : x1;
    const float c1 = .5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - .5f * x2, c3 = .5f * (x2 - xm1) + 1.5f * (x0 - x1);
    x = (((c3 * t + c2) * t + c1) * t + x0) * env;
    env *= envMul;
    pos += rate;
   }
   if (rust > 0.f) x += rust * (detail::softTanh(x * 6.f) * .3f - x * .5f); // corroded, gritty
   if (rattle > 0.f) {
    const float a = std::abs(x);
    follower += (a > follower ? .02f : .002f) * (a - follower);
    float hit = 0.f;
    if (.5f * (rng.next() + 1.f) < rattle * std::min(1.f, follower * 30.f) * .2f) hit = follower * (.5f + std::abs(rng.next())) * 6.f;
    float rat = 0.f;
    for (int k = 0; k < maxLoose; ++k) rat += loose[static_cast<std::size_t>(k)].process(hit) * looseAmp[static_cast<std::size_t>(k)];
    x += rat * rattle;
   }
   const float y = toneFilter.process(x, toneA1, toneA2, toneA3, 1.2f, toneMode) * gain;
   outL[i] += y * panL;
   outR[i] += y * panR;
   ++elapsed;
   quiet = std::abs(y) < 2e-6f ? quiet + 1 : 0;
  }
  age += n;
  if (pos >= len - 1.0 && quiet > static_cast<long long>(sr * .03)) alive = false;
 }

 struct ModeState { Resonator res; float amp = 0.f; int piece = 0; };
 struct Event { long long at = 0; float amp = 0.f; int piece = 0; };
 detail::Random rng;
 std::array<ModeState, maxModes> modes{};
 std::array<Resonator, maxLoose> loose{};
 std::array<float, maxLoose> looseAmp{};
 std::array<Event, maxEvents> events{};
 std::array<float, 3> piecePulse{};
 SVF toneFilter;
 double sr = 48000.0;
 long long elapsed = 0, lifetime = 0, quiet = 0;
 int modeCount = 0, eventCount = 0, pulseLen = 1;
 bool alive = false;
 float gain = 1.f, panL = 1.f, panR = 1.f, rattle = 0.f, follower = 0.f;
 float memLevel = 0.f, memFreq = 100.f, memStart = 1.f, memDrop = .99f, memDecay = .999f, memEnv = 0.f;
 float memPhase = 0.f, memDropState = 1.f;
 float toneA1 = 1.f, toneA2 = 0.f, toneA3 = 0.f;
 const std::vector<float>* sample = nullptr;
 double pos = 0.0, rate = 1.0;
 float env = 1.f, envMul = 1.f, rust = 0.f;
 int toneMode = 0;
};

// Monophonic musical saw: a near-sine "singing" tone with bow noise, vibrato,
// slow wobble (the blade flexing) and glide between notes.
class Saw {
public:
 struct Settings { float level = .7f, attackMs = 300.f, releaseMs = 800.f, vibrato = .35f, vibRate = 5.5f, glideMs = 180.f, tone = .3f, wobble = .3f; };
 void prepare(double sampleRate) { sr = sampleRate; env.reset(); heldCount = 0; bowState = 0.f; }
 void noteOn(int note, float velocity, const Settings& s) {
  for (int i = 0; i < heldCount; ++i) if (heldNotes[static_cast<std::size_t>(i)] == note) return;
  if (heldCount < 16) heldNotes[static_cast<std::size_t>(heldCount++)] = note;
  target = static_cast<float>(note);
  if (!env.active()) { current = target; vibratoPhase = 0.f; }
  glideStep = s.glideMs > 0.f ? std::abs(target - current) / std::max(1.f, s.glideMs * .001f * static_cast<float>(sr)) : 1e9f;
  vel = std::clamp(velocity, .05f, 1.f);
  env.gateOn();
 }
 void noteOff(int note, bool sustain) {
  int w = 0;
  for (int i = 0; i < heldCount; ++i) if (heldNotes[static_cast<std::size_t>(i)] != note) heldNotes[static_cast<std::size_t>(w++)] = heldNotes[static_cast<std::size_t>(i)];
  heldCount = w;
  if (heldCount > 0) { // legato back to the last held note
   target = static_cast<float>(heldNotes[static_cast<std::size_t>(heldCount - 1)]);
   glideStep = std::max(glideStep, std::abs(target - current) / std::max(1.f, .08f * static_cast<float>(sr)));
  } else if (!sustain) env.gateOff();
  else pendingRelease = true;
 }
 void sustainUp() { if (pendingRelease && heldCount == 0) env.gateOff(); pendingRelease = false; }
 void allOff() { heldCount = 0; env.gateOff(); }
 void kill() { heldCount = 0; env.reset(); }
 bool active() const { return env.active(); }
 float currentNote() const { return current; }

 void render(float* outL, float* outR, int n, const Settings& s, float bendSt) {
  if (!env.active()) return;
  env.set(s.attackMs, 100.f, 1.f, s.releaseMs, sr);
  const float vibInc = s.vibRate / static_cast<float>(sr);
  const float bowCoef = .02f + .1f * s.tone;
  for (int i = 0; i < n; ++i) {
   if (current < target) current = std::min(target, current + glideStep);
   else if (current > target) current = std::max(target, current - glideStep);
   vibratoPhase += vibInc; if (vibratoPhase >= 1.f) vibratoPhase -= 1.f;
   if (--wobbleHold <= 0) { wobbleTarget = rng.next(); wobbleHold = static_cast<int>(sr * (.3 + .4 * (rng.next() + 1.f))); }
   wobble += .00005f * (wobbleTarget - wobble);
   // Vibrato deepens as the bow settles in, like a player's hand on the blade.
   const float e = env.next();
   const float vib = detail::sine(vibratoPhase) * s.vibrato * .45f * std::min(1.f, e * 1.4f);
   const float pitch = current + bendSt + vib + wobble * s.wobble * .35f;
   phase += 440.f * detail::semitoneRatio(pitch - 69.f) / static_cast<float>(sr);
   if (phase >= 1.f) phase -= 1.f;
   const float fund = detail::sine(phase);
   float ph2 = phase * 2.f; if (ph2 >= 1.f) ph2 -= 1.f;
   bowState += bowCoef * (rng.next() - bowState);
   const float bow = bowState * s.tone * 2.f * (1.f + .6f * std::abs(fund));
   const float tremolo = 1.f + .25f * s.wobble * wobble;
   const float x = (fund + .08f * s.tone * detail::sine(ph2) + bow) * e * vel * s.level * tremolo * .45f;
   outL[i] += x;
   outR[i] += x;
  }
 }

private:
 Envelope env;
 detail::Random rng;
 double sr = 48000.0;
 std::array<int, 16> heldNotes{};
 int heldCount = 0, wobbleHold = 0;
 bool pendingRelease = false;
 float target = 60.f, current = 60.f, glideStep = 1e9f, vel = 1.f, phase = 0.f, vibratoPhase = 0.f;
 float wobble = 0.f, wobbleTarget = 0.f, bowState = 0.f;
};

// Megaphone: narrow band, overdriven horn. Gramophone: band-limited, wow and
// flutter through a short modulated delay, crackle and hiss.
class Character {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  delay.assign(static_cast<std::size_t>(sr * .02) + 4, 0.f);
  write = 0; wowPhase = flutterPhase = 0.f;
  for (auto* f : {&megaL, &megaR, &megaHL, &megaHR, &gramL, &gramR}) f->reset();
  hpL = hpR = gHpL = gHpR = 0.f;
  setFilter(megaBp, 1500.f, 1.4f);
  setFilter(megaLp, 3400.f, 1.2f);
  setFilter(gramLp, 3800.f, 1.3f);
 }
 // Silence the delay line and filters without reallocating (audio thread safe).
 void clear() {
  std::fill(delay.begin(), delay.end(), 0.f);
  surfaceEnv = 0.f; crackle = 0.f;
  for (auto* f : {&megaL, &megaR, &megaHL, &megaHR, &gramL, &gramR}) f->reset();
  hpL = hpR = gHpL = gHpR = 0.f;
 }
 void process(float& l, float& r, float megaphone, float gramophone) {
  if (megaphone > 0.f) {
   float ml = l, mr = r;
   hpL += .09f * (ml - hpL); ml -= hpL; // ~700 Hz high-pass
   hpR += .09f * (mr - hpR); mr -= hpR;
   ml = megaL.process(ml, megaBp.a1, megaBp.a2, megaBp.a3, megaBp.k, 1) * 1.5f + ml * .5f;
   mr = megaR.process(mr, megaBp.a1, megaBp.a2, megaBp.a3, megaBp.k, 1) * 1.5f + mr * .5f;
   const float drive = 2.f + 10.f * megaphone;
   ml = detail::softTanh(ml * drive) / std::sqrt(drive);
   mr = detail::softTanh(mr * drive) / std::sqrt(drive);
   ml = megaHL.process(ml, megaLp.a1, megaLp.a2, megaLp.a3, megaLp.k, 0) * 1.6f;
   mr = megaHR.process(mr, megaLp.a1, megaLp.a2, megaLp.a3, megaLp.k, 0) * 1.6f;
   l += megaphone * (ml - l);
   r += megaphone * (mr - r);
  }
  if (gramophone > 0.f && !delay.empty()) {
   const std::size_t n = delay.size() / 2;
   delay[write * 2] = l; delay[write * 2 + 1] = r;
   wowPhase += .55f / static_cast<float>(sr); if (wowPhase >= 1.f) wowPhase -= 1.f;
   flutterPhase += 6.3f / static_cast<float>(sr); if (flutterPhase >= 1.f) flutterPhase -= 1.f;
   const float ms = 4.f + gramophone * (2.2f * detail::sine(wowPhase) + .35f * detail::sine(flutterPhase));
   const float d = ms * .001f * static_cast<float>(sr);
   const float pos = static_cast<float>(write) + static_cast<float>(n) - d;
   const auto i0 = static_cast<std::size_t>(pos) % n;
   const auto i1 = (i0 + 1) % n;
   const float frac = pos - std::floor(pos);
   float gl = delay[i0 * 2] + frac * (delay[i1 * 2] - delay[i0 * 2]);
   float gr = delay[i0 * 2 + 1] + frac * (delay[i1 * 2 + 1] - delay[i0 * 2 + 1]);
   write = (write + 1) % n;
   gHpL += .04f * (gl - gHpL); gl -= gHpL; // ~300 Hz high-pass
   gHpR += .04f * (gr - gHpR); gr -= gHpR;
   gl = gramL.process(gl, gramLp.a1, gramLp.a2, gramLp.a3, gramLp.k, 0);
   gr = gramR.process(gr, gramLp.a1, gramLp.a2, gramLp.a3, gramLp.k, 0);
   // Surface noise follows the playing, like a record that only crackles
   // while it turns under the needle: it fades a couple of seconds after the last hit.
   const float level = std::min(1.f, (std::abs(l) + std::abs(r)) * 25.f);
   surfaceEnv += (level > surfaceEnv ? .01f : 1.f / (1.2f * static_cast<float>(sr))) * (level - surfaceEnv);
   float surface = rng.next() * .003f; // hiss
   if ((rng.next() + 1.f) * .5f < 6.f / static_cast<float>(sr)) crackle = rng.next() * .1f;
   surface += crackle; crackle *= .6f;
   surface *= surfaceEnv;
   gl += surface; gr += surface;
   l += gramophone * (gl * 1.3f - l);
   r += gramophone * (gr * 1.3f - r);
  }
 }
private:
 struct Coeffs { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f; };
 float surfaceEnv = 0.f;
 void setFilter(Coeffs& c, float f, float k) {
  const float g = std::tan(3.14159265f * std::min(f, .45f * static_cast<float>(sr)) / static_cast<float>(sr));
  c.k = k; c.a1 = 1.f / (1.f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
 }
 double sr = 48000.0;
 Coeffs megaBp, megaLp, gramLp;
 SVF megaL, megaR, megaHL, megaHR, gramL, gramR;
 float hpL = 0.f, hpR = 0.f, gHpL = 0.f, gHpR = 0.f, wowPhase = 0.f, flutterPhase = 0.f, crackle = 0.f;
 std::vector<float> delay;
 std::size_t write = 0;
 detail::Random rng;
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  for (int i = 0; i < maxHits; ++i) { hits[static_cast<std::size_t>(i)].seed(static_cast<std::uint32_t>(i + 7)); hits[static_cast<std::size_t>(i)].stop(); }
  saw.prepare(sr);
  character.prepare(sr);
  mutilator.prepare(sr);
  mutilator.setLayeredRepeats(true);
  reverb.prepare(sr);
  airL.reset(); airR.reset();
  {
   const float g = std::tan(3.14159265f * std::min(9500.f, .45f * static_cast<float>(sr)) / static_cast<float>(sr));
   airA1 = 1.f / (1.f + g * (g + 1.1f)); airA2 = g * airA1; airA3 = g * airA2;
  }
  pendingCount = 0;
  // Fixed per-pad "rust" offsets: each pad's object is bent its own way.
  detail::Random r; r.state = 0xBADC0DEu;
  for (auto& pad : rust) for (auto& o : pad) o = r.next();
  smoothed = target;
  derive();
 }
 void setValues(const Values& v) { target = v; }
 // A new kit: drop what is sounding and take the new values at once, so the old
 // kit's tails and gliding knobs (room size, effects) do not smear into it.
 void switchKit(const Values& v) {
  allSoundOff();
  character.clear();
  mutilator.clear();
  airL.reset(); airR.reset();
  target = v; smoothed = v; derive();
 }
 // The library and pad samples are owned by the caller and must outlive their use.
 // Changing them silences the hits that may be reading the old memory.
 void setLibrary(const SampleLibrary* lib) { if (lib != library) { for (auto& h : hits) h.stop(); library = lib; } }
 void setPadSample(int pad, const SampleSet* set) {
  if (pad < 0 || pad >= numPads || padSample[static_cast<std::size_t>(pad)] == set) return;
  for (auto& h : hits) if (h.active() && h.pad == pad) h.stop();
  padSample[static_cast<std::size_t>(pad)] = set;
 }
 bool padUsesSample(int pad) const {
  const auto& p = pads[static_cast<std::size_t>(std::clamp(pad, 0, numPads - 1))];
  return padSample[static_cast<std::size_t>(std::clamp(pad, 0, numPads - 1))] ||
         (library && isSampleObject(p.object) && static_cast<std::size_t>(p.object) < library->objects.size());
 }
 void setTransport(double bpm, double ppq, bool playing) { mutilator.setTransport(bpm, ppq, playing); }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f) * 2.f; }
 void setSustain(bool down) { sustainPedal = down; if (!down) saw.sustainUp(); }
 bool sustainDown() const { return sustainPedal; }

 void noteOn(int note, float velocity) {
  if (velocity <= 0.f) { noteOff(note); return; }
  if (note >= firstSawNote) { saw.noteOn(note, velocity, sawSettings); return; }
  const int pad = note - firstPadNote;
  if (pad < 0 || pad >= numPads) return;
  // STUMBLE: some hits land late, like a drummer (or a machine) losing its footing.
  const float stumble = smoothed[P::Stumble] * .01f;
  int delaySamples = 0;
  if (stumble > 0.f) {
   const float u = .5f * (rng.next() + 1.f);
   delaySamples = static_cast<int>(u * u * stumble * .08 * sr);
  }
  if (delaySamples <= 0 || pendingCount >= maxPending) { trigger(pad, velocity); return; }
  pending[static_cast<std::size_t>(pendingCount++)] = {pad, velocity, delaySamples};
 }
 void noteOff(int note) { if (note >= firstSawNote) saw.noteOff(note, sustainPedal); } // hits ring out by themselves
 void allNotesOff() { saw.allOff(); }
 void allSoundOff() { for (auto& h : hits) h.stop(); saw.kill(); pendingCount = 0; reverb.clear(); }

 int activeHits() const { int n = 0; for (const auto& h : hits) n += h.active() ? 1 : 0; return n; }
 bool sawActive() const { return saw.active(); }
 // Bit i set = pad i was triggered since the last call (for the editor's pad lights).
 std::uint32_t takeTriggeredPads() { const auto t = triggered; triggered = 0; return t; }

 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   float* l = left + done;
   float* r = right + done;
   std::fill(l, l + n, 0.f);
   std::fill(r, r + n, 0.f);
   for (int i = 0; i < n; ++i) {
    // Release stumbling hits exactly on their late sample.
    for (int p = 0; p < pendingCount;) {
     auto& h = pending[static_cast<std::size_t>(p)];
     if (--h.delay <= 0) { trigger(h.pad, h.velocity); pending[static_cast<std::size_t>(p)] = pending[static_cast<std::size_t>(--pendingCount)]; }
     else ++p;
    }
    for (auto& h : hits) if (h.active()) h.render(l + i, r + i, 1);
   }
   saw.render(l, r, n, sawSettings, bend);
   const float mix = smoothed[P::RoomMix] * .01f;
   const float dryGain = std::cos(mix * 1.5707963f), wetGain = std::sin(mix * 1.5707963f) * 1.4f;
   const float mega = smoothed[P::Megaphone] * .01f, gram = smoothed[P::Gramophone] * .01f;
   for (int i = 0; i < n; ++i) {
    // Like a recording, not a synth: air absorbs the top, the tape rounds peaks.
    l[i] = airL.process(detail::softTanh(l[i] * 1.2f) / 1.2f, airA1, airA2, airA3, 1.1f, 0);
    r[i] = airR.process(detail::softTanh(r[i] * 1.2f) / 1.2f, airA1, airA2, airA3, 1.1f, 0);
    character.process(l[i], r[i], mega, gram);
    mutilator.process(l[i], r[i]);
    float wl = 0.f, wr = 0.f;
    reverb.process(l[i], r[i], wl, wr);
    l[i] = softCeiling((l[i] * dryGain + wl * wetGain) * outGain);
    r[i] = softCeiling((r[i] * dryGain + wr * wetGain) * outGain);
   }
   done += n;
   controlCountdown -= n;
  }
 }

private:
 struct Pending { int pad = 0; float velocity = 0.f; int delay = 0; };

 void trigger(int pad, float velocity) {
  Hit* chosen = nullptr;
  for (auto& h : hits) if (!h.active()) { chosen = &h; break; }
  if (!chosen) for (auto& h : hits) if (!chosen || h.age > chosen->age) chosen = &h;
  const auto u = static_cast<std::size_t>(pad);
  const auto& settings = pads[u];
  if (padSample[u]) chosen->startSample(pad, settings, *padSample[u], lastLayer[u], velocity, velSens, sr);
  else if (library && isSampleObject(settings.object) && static_cast<std::size_t>(settings.object) < library->objects.size())
   chosen->startSample(pad, settings, library->objects[static_cast<std::size_t>(settings.object)], lastLayer[u], velocity, velSens, sr);
  else chosen->start(pad, settings, rust[u], velocity, velSens, sr);
  triggered |= 1u << pad;
 }

 void controlTick() {
  const auto& table = specs();
  for (std::size_t i = 0; i < target.size(); ++i) {
   if (table[i].kind == Kind::Float) smoothed[i] += .15f * (target[i] - smoothed[i]);
   else smoothed[i] = target[i];
  }
  derive();
 }

 void derive() {
  const auto& s = smoothed;
  for (int pad = 0; pad < numPads; ++pad) {
   const int b = P::padBase(pad);
   auto& p = pads[static_cast<std::size_t>(pad)];
   p.object = static_cast<int>(std::lround(s[static_cast<std::size_t>(b + P::Object)]));
   p.tune = s[static_cast<std::size_t>(b + P::Tune)];
   p.decay = s[static_cast<std::size_t>(b + P::Decay)] * .01f;
   p.tone = s[static_cast<std::size_t>(b + P::Tone)] * .01f;
   p.rattle = s[static_cast<std::size_t>(b + P::Rattle)] * .01f;
   p.rust = s[static_cast<std::size_t>(b + P::Rust)] * .01f;
   p.variation = s[static_cast<std::size_t>(b + P::Variation)] * .01f;
   p.gain = std::pow(10.f, (s[static_cast<std::size_t>(b + P::Level)] + objectTrimDb(p.object)) / 20.f);
   p.pan = s[static_cast<std::size_t>(b + P::Pan)] * .01f;
  }
  velSens = s[P::VelSens] * .01f;
  sawSettings = {s[P::SawLevel] * .01f, s[P::SawAttack], s[P::SawRelease], s[P::SawVibrato] * .01f, s[P::SawVibRate],
                 s[P::SawGlide], s[P::SawTone] * .01f, s[P::SawWobble] * .01f};
  static const double divisions[7] = {0.0, 1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
  mutilator.configure(divisions[std::clamp(static_cast<int>(std::lround(s[P::StutterDiv])), 0, 6)], s[P::StutterDepth] * .01f,
                      s[P::Chaos] * .01f, s[P::Crush] * .01f, 0.f);
  mutilator.setFreeTime(std::lround(s[P::StutterSync]) == 1, s[P::StutterMs]);
  reverb.configure(s[P::RoomSize] * .01f, s[P::RoomDecay], s[P::RoomDamp] * .01f, false);
  outGain = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 std::array<PadSettings, numPads> pads{};
 std::array<std::array<float, 8>, numPads> rust{};
 std::array<Hit, maxHits> hits{};
 std::array<Pending, maxPending> pending{};
 const SampleLibrary* library = nullptr;
 std::array<const SampleSet*, numPads> padSample{};
 std::array<int, numPads> lastLayer{};
 int pendingCount = 0, controlCountdown = 0;
 Saw saw;
 Saw::Settings sawSettings;
 Character character;
 Mutilator mutilator;
 Reverb reverb;
 SVF airL, airR;
 float airA1 = 1.f, airA2 = 0.f, airA3 = 0.f;
 detail::Random rng;
 float bend = 0.f, velSens = .7f, outGain = .7f;
 bool sustainPedal = false;
 std::uint32_t triggered = 0;
};

} // namespace inverso_abismo::drums
