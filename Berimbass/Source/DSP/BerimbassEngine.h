#pragma once
// INVERSO BERIMBASS engine: a physically modelled bass string and three
// instrument bodies. Portable C++17, no JUCE, no allocation after prepare().
//
// String: two polarisations (two slightly detuned waveguide loops) so the tone
// beats and decays like a real string; tension modulation (the pitch is a bit
// sharp while the string swings wide and settles as it decays: little on the
// berimbau's steel wire, a lot on the washtub's rope); stiffness all-passes;
// the stone (dobrao) bouncing against the wire.
// Techniques: Pizzicato (the finger pulls the string into a triangle and
// lets go), Bambu (a narrow stick strike + a wooden click + caxixi), Arco (the
// STK bowed-string model: real stick-slip friction), Pedra (stick + stone buzz
// + a pitch scoop as the stone presses on).
// Bodies: Cabaca (Helmholtz resonance of the gourd, its shell, and the comb of
// the belly reflection: the real "wah" as it opens), Banjo (a Bessel-mode skin
// head, short and nasal), Balde (washtub: bottom plate, cylinder shell, the
// rope's dull thump), Nenhum (dry).
#include "../../../Abismo/Source/DSP/AbismoCommon.h"
#include "../../../Abismo/Source/DSP/AbismoReverb.h"
#include "BerimbassParams.h"
#include <vector>

namespace inverso_berimbass {

using inverso_abismo::controlInterval;
using inverso_abismo::Envelope;
using inverso_abismo::Resonator;
using inverso_abismo::SVF;
using inverso_abismo::twoPi;
namespace detail = inverso_abismo::detail;

constexpr int maxVoices = 6;
constexpr float minFreq = 20.f;
constexpr int stiffStages = 4;

struct Controls {
 int technique = 1, body = 0;
 bool mono = true;
 float decay = 3.f, brightness = .6f, position = .15f, stiffness = .3f, buzz = .2f, pressure = .6f;
 float attackMs = 150.f, releaseMs = 250.f, stoneBend = 1.f, octaveSt = 0.f, tuneSt = 0.f, glideMs = 60.f;
 float bendRange = 2.f, vibrato = 0.f, vibRate = 5.f, sub = 0.f, velSens = .7f, caxixi = .45f;
 // Material of the string, set by the body: how much the pitch rises when it swings wide.
 float tensionMod = .006f, darkness = 0.f;
};

inline float phaseDelayLowpass(float a, float w) { return std::atan2(a * std::sin(w), 1.f - a * std::cos(w)) / w; }
inline float phaseDelayAllpass(float c, float w) {
 const float argN = std::atan2(-std::sin(w), c + std::cos(w));
 const float argD = std::atan2(-c * std::sin(w), 1.f + c * std::cos(w));
 return -(argN - argD) / w;
}

// Delay line with linear fractional read.
struct Delay {
 std::vector<float> buf;
 int write = 0;
 void prepare(std::size_t n) { buf.assign(n, 0.f); write = 0; }
 void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
 float read(float d) const {
  const int n = static_cast<int>(buf.size());
  float pos = static_cast<float>(write) - d;
  while (pos < 0.f) pos += static_cast<float>(n);
  const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
  const float f = pos - std::floor(pos);
  return buf[static_cast<std::size_t>(i0)] + f * (buf[static_cast<std::size_t>(i1)] - buf[static_cast<std::size_t>(i0)]);
 }
 void push(float x) { buf[static_cast<std::size_t>(write)] = x; write = (write + 1) % static_cast<int>(buf.size()); }
 // Add a displacement shape to the most recent `n` samples (what the loop plays next).
 void addShape(const float* shape, int n) {
  const int len = static_cast<int>(buf.size());
  for (int k = 0; k < n; ++k) {
   const int idx = ((write - n + k) % len + len) % len;
   buf[static_cast<std::size_t>(idx)] += shape[k];
  }
 }
};

// One polarisation of the string: delay -> damping -> stiffness -> loss.
struct Loop {
 Delay line;
 float lp = 0.f, lpCoef = .3f, gain = .99f, delay = 100.f, apCoef = 0.f;
 std::array<float, stiffStages> apX{}, apY{};
 void clear() { line.clear(); lp = 0.f; apX.fill(0.f); apY.fill(0.f); }
 float output(float delayScale) const { return line.read(std::max(2.f, delay * delayScale)); }
 float filter(float y) {
  lp = (1.f - lpCoef) * y + lpCoef * lp;
  y = lp;
  for (int s = 0; s < stiffStages; ++s) {
   const auto i = static_cast<std::size_t>(s);
   const float o = apCoef * y + apX[i] - apCoef * apY[i];
   apX[i] = y; apY[i] = o; y = o;
  }
  return y * gain;
 }
 // Tune for frequency f with a given round-trip decay: corrects the filters' phase delay.
 void tune(float f, double sr, float t60, float brightness, float stiffness) {
  const float w = twoPi * f / static_cast<float>(sr);
  lpCoef = .02f + (1.f - brightness) * .85f;
  apCoef = -.55f * stiffness;
  const float lpMag = (1.f - lpCoef) / std::sqrt(1.f - 2.f * lpCoef * std::cos(w) + lpCoef * lpCoef);
  gain = std::min(.99995f, std::pow(10.f, -3.f / (std::max(.02f, t60) * f)) / lpMag);
  const float filterDelay = phaseDelayLowpass(lpCoef, w) + static_cast<float>(stiffStages) * phaseDelayAllpass(apCoef, w);
  delay = std::clamp(static_cast<float>(sr) / f - filterDelay, 2.f, static_cast<float>(line.buf.size() - 3));
 }
};

// STK-style bowed string: the bow divides the string into a neck and a bridge
// side; a friction curve decides each sample whether the string sticks to the
// bow hair or slips. This is what makes a bowed string sound bowed.
struct Bow {
 Delay neck, bridge;
 float filterState = 0.f, pole = .6f, beta = .127f, lenNeck = 50.f, lenBridge = 10.f;
 void prepare(std::size_t n) { neck.prepare(n); bridge.prepare(n); filterState = 0.f; }
 void clear() { neck.clear(); bridge.clear(); filterState = 0.f; }
 void tune(float f, double sr, float position) {
  pole = .75f - .2f * 22050.f / static_cast<float>(sr);
  const float w = twoPi * f / static_cast<float>(sr);
  const float total = std::max(4.f, static_cast<float>(sr) / f - phaseDelayLowpass(pole, w));
  beta = std::clamp(position, .05f, .35f);
  lenBridge = std::max(1.f, total * beta);
  lenNeck = std::max(1.f, total - lenBridge);
 }
 float tick(float bowVelocity, float pressure, float bridgeLoss) {
  const float b = bridge.read(lenBridge);
  filterState = (1.f - pole) * b + pole * filterState;
  const float bridgeRefl = -filterState * bridgeLoss;
  const float nutRefl = -neck.read(lenNeck);
  const float stringVel = bridgeRefl + nutRefl;
  const float dv = bowVelocity - stringVel;
  const float slope = 5.f - 4.f * pressure;
  float table = std::pow(std::abs((dv + .001f) * slope) + .75f, -4.f);
  table = std::min(table, 1.f);
  const float newVel = dv * table;
  neck.push(bridgeRefl + newVel);
  bridge.push(nutRefl + newVel);
  return b;
 }
};

class Voice {
public:
 void prepare(double sampleRate, std::uint32_t seed) {
  sr = sampleRate;
  const auto len = static_cast<std::size_t>(sr / minFreq) + 8;
  a.line.prepare(len);
  b.line.prepare(len);
  bow.prepare(len);
  shape.assign(len, 0.f);
  rng.state = seed * 2654435761u + 31u;
  static const float woodF[4] = {1150.f, 2350.f, 3900.f, 5300.f};
  for (int i = 0; i < 4; ++i) wood[static_cast<std::size_t>(i)].set(std::min(woodF[i], .45f * static_cast<float>(sr)), .025f - .004f * static_cast<float>(i), sr);
  kill();
 }
 void kill() {
  a.clear(); b.clear(); bow.clear();
  active = false; gate = false; note = -1; silentFor = 0; energy = 0.f;
  for (auto& w : wood) w.reset();
  for (auto& s : seeds) s.reset();
  woodPulse = 0; grainsLeft = 0;
  bowEnv.reset();
 }
 bool isActive() const { return active; }
 int note = -1;
 bool gate = false;
 long long age = 0;

 void start(int midiNote, float velocity, const Controls& c, bool retrigger, float glideFromSt) {
  note = midiNote;
  gate = true;
  age = 0;
  vel = std::clamp(velocity, 0.f, 1.f);
  glideOffset = glideStart = glideFromSt;
  if (!retrigger && active) return; // legato: keep the string ringing, just glide
  active = true;
  silentFor = 0;
  scoop = c.technique == 3 ? -c.stoneBend : 0.f;
  scoopTime = 0.f;
  vibTime = 0.f;
  const float velScale = 1.f - c.velSens + c.velSens * vel;
  amp = velScale;
  updatePitch(c, 0.f);
  if (c.technique == 2) { bowEnv.gateOn(); return; }
  bowEnv.reset();
  // Initial displacement of the string, written into both polarisations.
  const int n = std::clamp(static_cast<int>(a.delay), 8, static_cast<int>(shape.size()) - 1);
  const int peak = std::clamp(static_cast<int>(c.position * static_cast<float>(n)), 1, n - 2);
  const bool stick = c.technique == 1 || c.technique == 3;
  if (stick) {
   // A stick hits a short stretch of the string: a narrow raised-cosine bump.
   const float hardness = std::clamp(.4f + .6f * c.pressure * (.5f + .5f * vel), .1f, 1.f);
   const int width = std::max(2, static_cast<int>(static_cast<float>(n) * (.09f - .07f * hardness)));
   for (int k = 0; k < n; ++k) {
    const float d = static_cast<float>(k - peak) / static_cast<float>(width);
    shape[static_cast<std::size_t>(k)] = std::abs(d) < 1.f ? .5f + .5f * std::cos(3.14159265f * d) : 0.f;
   }
  } else {
   // The finger pulls the string into a triangle; its soft pad rounds the corner.
   for (int k = 0; k < n; ++k)
    shape[static_cast<std::size_t>(k)] = k <= peak ? static_cast<float>(k) / static_cast<float>(peak)
                                                   : static_cast<float>(n - k) / static_cast<float>(n - peak);
   const int soft = std::max(1, static_cast<int>(static_cast<float>(n) * (.02f + .08f * (1.f - c.brightness))));
   float acc = 0.f;
   std::vector<float>& s = shape;
   for (int pass = 0; pass < 2; ++pass) { // two box filters ~ a rounded finger
    acc = 0.f;
    for (int k = 0; k < n; ++k) {
     acc += s[static_cast<std::size_t>(k)];
     if (k >= soft) acc -= s[static_cast<std::size_t>(k - soft)];
     s[static_cast<std::size_t>(k)] = acc / static_cast<float>(soft);
    }
   }
  }
  // Remove the shape's average so the string does not start with a DC offset.
  float mean = 0.f;
  for (int k = 0; k < n; ++k) mean += shape[static_cast<std::size_t>(k)];
  mean /= static_cast<float>(n);
  float peakAbs = 1e-6f;
  for (int k = 0; k < n; ++k) { shape[static_cast<std::size_t>(k)] -= mean; peakAbs = std::max(peakAbs, std::abs(shape[static_cast<std::size_t>(k)])); }
  const float level = .55f * amp / peakAbs;
  for (int k = 0; k < n; ++k) shape[static_cast<std::size_t>(k)] *= level;
  a.line.addShape(shape.data(), n);
  for (int k = 0; k < n; ++k) shape[static_cast<std::size_t>(k)] *= .55f;
  b.line.addShape(shape.data(), n);
  if (stick) {
   woodPulse = std::max(1, static_cast<int>(.0004 * sr));
   woodPulseLen = woodPulse;
   woodGain = (c.technique == 1 ? .35f : .2f) * amp;
   if (c.caxixi > 0.f) { grainsLeft = 24 + static_cast<int>(rng.next() * 8.f); nextGrain = 0; caxixiGain = c.caxixi * amp; }
  }
 }
 void release() { gate = false; bowEnv.gateOff(); }

 void control(const Controls& c, float bendSt, float pressure, float lfoVib) {
  const float dtMs = static_cast<float>(controlInterval * 1000.0 / sr);
  if (c.glideMs > 0.f && std::abs(glideOffset) > 1e-6f) {
   const float step = std::abs(glideStart) * dtMs / c.glideMs;
   glideOffset = glideOffset > 0.f ? std::max(0.f, glideOffset - step) : std::min(0.f, glideOffset + step);
  } else glideOffset = 0.f;
  if (scoop < 0.f) { scoopTime += dtMs; scoop = -c.stoneBend * std::max(0.f, 1.f - scoopTime / 80.f); }
  vibTime += dtMs;
  const float vib = lfoVib * c.vibrato * .5f * std::min(1.f, vibTime / 400.f);
  updatePitch(c, bendSt + glideOffset + scoop + pressure * c.stoneBend + vib);
  bowEnv.set(c.attackMs, 100.f, 1.f, c.releaseMs, sr);
 }

 void render(const Controls& c, float* out, int n, float pressure) {
  if (!active) return;
  const float buzz = std::clamp(c.buzz + (c.technique == 3 ? .25f : 0.f) + pressure * .3f, 0.f, 1.f);
  const float threshold = .6f - .55f * buzz;
  for (int i = 0; i < n; ++i) {
   float o;
   if (c.technique == 2) {
    const float e = bowEnv.next();
    const float bowVelocity = (.03f + .2f * amp) * e * (.6f + .8f * c.pressure);
    o = bow.tick(bowVelocity, c.pressure, bowLoss) * 1.2f;
   } else {
    // Tension modulation: while the string swings wide it is a little sharp.
    const float scale = 1.f - c.tensionMod * std::min(1.f, energy * 6.f);
    const float ya = a.output(scale), yb = b.output(scale);
    float fa = a.filter(ya);
    if (buzz > .01f && std::abs(fa) > threshold) { // the wire bouncing off the stone
     const float excess = std::abs(fa) - threshold;
     fa = (fa > 0.f ? 1.f : -1.f) * (threshold - excess * .6f);
    }
    a.line.push(fa);
    b.line.push(b.filter(yb));
    o = ya + yb * .7f;
    energy += .0005f * (o * o - energy);
   }
   if (woodPulse > 0) { // the bamboo stick's own wooden knock
    const float t = 1.f - static_cast<float>(woodPulse) / static_cast<float>(woodPulseLen);
    const float f = detail::sine(.5f * t) * woodGain * 6.f;
    float w = 0.f;
    for (auto& r : wood) w += r.process(f);
    o += w;
    --woodPulse;
   } else {
    float w = 0.f;
    for (auto& r : wood) w += r.process(0.f);
    o += w;
   }
   o += caxixiTick();
   if (c.sub > 0.f) {
    follower += .001f * (std::abs(o) - follower);
    subPhase += subInc; if (subPhase >= 1.f) subPhase -= 1.f;
    o += detail::sine(subPhase) * follower * 2.2f * c.sub;
   }
   out[i] += o * .3f; // level-matched across techniques
   silentFor = std::abs(o) < 1e-5f ? silentFor + 1 : 0;
  }
  age += n;
  if (!gate && !bowEnv.active() && silentFor > static_cast<int>(sr * .05)) active = false;
 }

private:
 // Caxixi: dried seeds hitting a woven basket, a burst of tiny bright impacts.
 float caxixiTick() {
  if (grainsLeft <= 0) {
   float tail = 0.f;
   for (auto& s : seeds) tail += s.process(0.f);
   return tail;
  }
  float hit = 0.f;
  if (--nextGrain <= 0) {
   hit = rng.next() * caxixiGain * 14.f;
   for (std::size_t k = 0; k < seeds.size(); ++k)
    seeds[k].set(std::min(2600.f + 4200.f * std::abs(rng.next()), .45f * static_cast<float>(sr)), .02f, sr);
   nextGrain = static_cast<int>(sr * (.0015 + .0045 * std::abs(rng.next())));
   --grainsLeft;
  }
  float y = 0.f;
  for (auto& s : seeds) y += s.process(hit);
  return y;
 }

 void updatePitch(const Controls& c, float offsetSt) {
  const float semis = static_cast<float>(note) + c.octaveSt + c.tuneSt + offsetSt;
  const float f = std::clamp(440.f * detail::semitoneRatio(semis - 69.f), minFreq, .2f * static_cast<float>(sr));
  const float t60 = gate || (c.technique == 2 && bowEnv.active()) ? c.decay : std::min(c.decay, c.releaseMs * .001f);
  const float bright = std::clamp(c.brightness - c.darkness, 0.f, 1.f);
  a.tune(f, sr, t60, bright, c.stiffness);
  // Second polarisation: ~1.5 cents sharp, a little darker and quicker to fade.
  b.tune(f * 1.00087f, sr, t60 * .7f, bright * .85f, c.stiffness);
  bow.tune(f, sr, c.position);
  bowLoss = std::min(.995f, .95f + .045f * std::min(1.f, c.decay / 6.f)) * (gate || bowEnv.active() ? 1.f : .9f);
  subInc = f * .5f / static_cast<float>(sr);
 }

 double sr = 48000.0;
 Loop a, b;
 Bow bow;
 std::vector<float> shape;
 detail::Random rng;
 Envelope bowEnv;
 std::array<Resonator, 4> wood{};
 std::array<Resonator, 3> seeds{};
 int silentFor = 0, woodPulse = 0, woodPulseLen = 1, grainsLeft = 0, nextGrain = 0;
 bool active = false;
 float vel = 1.f, amp = 1.f, energy = 0.f, bowLoss = .95f, woodGain = .3f, caxixiGain = .4f;
 float glideOffset = 0.f, glideStart = 0.f, scoop = 0.f, scoopTime = 0.f, vibTime = 0.f;
 float follower = 0.f, subPhase = 0.f, subInc = 0.f;
};

// The three bodies, applied to the summed strings (they are linear).
class Body {
public:
 void prepare(double sampleRate) {
  sr = sampleRate;
  comb.prepare(static_cast<std::size_t>(sr * .005) + 4);
  for (auto* f : {&lpf, &nasal, &mouthF}) f->reset();
  for (auto& m : modes) m.reset();
  helm.reset();
 }
 void configure(int type, float gourdOpen, float tuneSt) {
  body = type;
  gourd = gourdOpen;
  const float t = detail::semitoneRatio(tuneSt);
  const float ny = .45f * static_cast<float>(sr);
  auto setModes = [&](const float* ratios, int count, float f0, float t60, float damp) {
   modeCount = count;
   for (int i = 0; i < count; ++i)
    modes[static_cast<std::size_t>(i)].set(std::min(f0 * ratios[i], ny), t60 * std::pow(ratios[i], -damp), sr);
  };
  if (type == 0) {
   // Gourd: Helmholtz resonance (air in the gourd) + the shell + the belly comb.
   helm.set(115.f * t, .25f, sr);
   // The open mouth of the gourd: a vowel-like resonance that rises from "u" to "a".
   setFilter(mouthCo, 320.f * t * std::exp2(gourdOpen * 2.f), .6f);
   static const float shell[8] = {4.2f, 5.6f, 7.9f, 9.4f, 12.1f, 15.8f, 19.7f, 24.3f};
   setModes(shell, 8, 115.f * t, .05f, .4f);
   setFilter(lpCo, 450.f * std::exp2(gourdOpen * 4.f), .9f);
   combDelay = static_cast<float>(sr) * (.0008f + .0022f * gourdOpen);
   combGain = .65f * (1.f - gourdOpen);
  } else if (type == 1) {
   // Banjo head: circular membrane modes (Bessel), short; plus its nasal peak.
   static const float bessel[16] = {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f,
                                    3.600f, 3.652f, 4.060f, 4.154f, 4.601f, 4.832f, 4.903f, 5.131f};
   setModes(bessel, 16, 180.f * t, .14f, .6f);
   setFilter(nasalCo, 1250.f * t, .7f);
  } else if (type == 2) {
   // Washtub: the tub's bottom plate (membrane-like) and its cylinder wall.
   static const float tub[14] = {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.9f, 5.2f, 6.1f, 7.4f, 9.3f, 11.7f, 13.2f, 16.f};
   setModes(tub, 14, 78.f * t, .45f, .5f);
   helm.set(62.f * t, .3f, sr);
   setFilter(lpCo, 1400.f, .9f);
  }
 }
 float process(float x, float mix) {
  float y = x;
  switch (body) {
   case 0: {
    float shell = 0.f;
    for (int i = 0; i < modeCount; ++i) shell += modes[static_cast<std::size_t>(i)].process(x) / (1.f + .4f * static_cast<float>(i));
    const float air = helm.process(x);
    const float mouth = mouthF.process(x, mouthCo.a1, mouthCo.a2, mouthCo.a3, mouthCo.k, 1);
    float g = lpf.process(x, lpCo.a1, lpCo.a2, lpCo.a3, lpCo.k, 0) * .7f + air * .8f + mouth * 1.6f * gourd + shell * .25f;
    // Reflection off the player's belly: comb filtering that sweeps as the gourd opens.
    const float back = comb.read(std::max(1.f, combDelay));
    comb.push(g);
    y = (g + combGain * back) * .7f;
    break;
   }
   case 1: {
    float head = 0.f;
    for (int i = 0; i < modeCount; ++i) head += modes[static_cast<std::size_t>(i)].process(x) / (1.f + .25f * static_cast<float>(i));
    const float nose = nasal.process(x, nasalCo.a1, nasalCo.a2, nasalCo.a3, nasalCo.k, 1);
    y = (x * .45f + head * .4f + nose * 1.6f) * .5f; // level-matched to the gourd
    break;
   }
   case 2: {
    float tub = 0.f;
    for (int i = 0; i < modeCount; ++i) tub += modes[static_cast<std::size_t>(i)].process(x) / (1.f + .5f * static_cast<float>(i));
    y = (lpf.process(x, lpCo.a1, lpCo.a2, lpCo.a3, lpCo.k, 0) * .5f + helm.process(x) * 1.2f + tub * .6f) * .3f; // level-matched
    break;
   }
   default: break;
  }
  return x + mix * (y - x);
 }
private:
 struct Co { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f; };
 void setFilter(Co& c, float f, float k) {
  const float g = std::tan(3.14159265f * std::clamp(f, 20.f, .45f * static_cast<float>(sr)) / static_cast<float>(sr));
  c.k = k; c.a1 = 1.f / (1.f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
 }
 double sr = 48000.0;
 int body = 0, modeCount = 0;
 float gourd = .5f, combDelay = 50.f, combGain = 0.f;
 Co lpCo, nasalCo, mouthCo;
 SVF lpf, nasal, mouthF;
 Resonator helm;
 Delay comb;
 std::array<Resonator, 16> modes{};
};

class Engine {
public:
 void prepare(double sampleRate) {
  sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
  for (int i = 0; i < maxVoices; ++i) voices[static_cast<std::size_t>(i)].prepare(sr, static_cast<std::uint32_t>(i + 3));
  body.prepare(sr);
  reverb.prepare(sr);
  toneFilter.reset();
  dcIn = dcOut = 0.f;
  dcR = 1.f - twoPi * 10.f / static_cast<float>(sr);
  heldCount = 0;
  smoothed = target;
  controlCountdown = 0;
  derive();
 }
 void setValues(const Values& v) { target = v; }

 void noteOn(int note, float velocity) {
  if (velocity <= 0.f) { noteOff(note); return; }
  if (heldCount < 16) held[static_cast<std::size_t>(heldCount++)] = note;
  const float from = (c.glideMs > 0.f && lastNote >= 0) ? static_cast<float>(lastNote - note) : 0.f;
  if (c.mono) {
   auto& v = voices[0];
   // Legato (another key still held): glide without re-striking; otherwise strike again.
   const bool legato = heldCount > 1 && v.isActive() && v.gate;
   v.start(note, velocity, c, !legato, legato || c.technique == 2 ? from : 0.f);
  } else {
   Voice* chosen = nullptr;
   for (auto& v : voices) if (v.isActive() && v.note == note) { chosen = &v; break; }
   if (!chosen) for (auto& v : voices) if (!v.isActive()) { chosen = &v; break; }
   if (!chosen) for (auto& v : voices) if (!chosen || v.age > chosen->age) chosen = &v;
   chosen->start(note, velocity, c, true, 0.f);
  }
  lastNote = note;
 }
 void noteOff(int note) {
  int w = 0;
  for (int i = 0; i < heldCount; ++i) if (held[static_cast<std::size_t>(i)] != note) held[static_cast<std::size_t>(w++)] = held[static_cast<std::size_t>(i)];
  heldCount = w;
  if (c.mono) {
   auto& v = voices[0];
   if (heldCount > 0 && v.isActive()) { // fall back to the previous held key
    const int back = held[static_cast<std::size_t>(heldCount - 1)];
    v.start(back, .7f, c, false, static_cast<float>(v.note - back));
    lastNote = back;
   } else if (v.note == note) {
    if (sustainPedal) pendingRelease = true; else v.release();
   }
  } else {
   for (auto& v : voices) {
    if (!v.isActive() || v.note != note || !v.gate) continue;
    if (sustainPedal) pendingRelease = true; else v.release();
   }
  }
 }
 void setSustain(bool down) {
  sustainPedal = down;
  if (!down && pendingRelease) {
   pendingRelease = false;
   for (auto& v : voices) {
    bool stillHeld = false;
    for (int i = 0; i < heldCount; ++i) stillHeld = stillHeld || held[static_cast<std::size_t>(i)] == v.note;
    if (v.isActive() && !stillHeld) v.release();
   }
  }
 }
 void allNotesOff() { heldCount = 0; for (auto& v : voices) v.release(); }
 void allSoundOff() { heldCount = 0; for (auto& v : voices) v.kill(); reverb.clear(); }
 void setPitchBend(float normalised) { bend = std::clamp(normalised, -1.f, 1.f); }
 void setModWheel(float value) { modWheel = std::clamp(value, 0.f, 1.f); }
 void setPressure(float value) { pressure = std::clamp(value, 0.f, 1.f); }
 int activeVoices() const { int n = 0; for (const auto& v : voices) n += v.isActive() ? 1 : 0; return n; }

 void render(float* left, float* right, int numSamples) {
  int done = 0;
  while (done < numSamples) {
   if (controlCountdown <= 0) { controlTick(); controlCountdown = controlInterval; }
   const int n = std::min(controlCountdown, numSamples - done);
   float* l = left + done;
   float* r = right + done;
   std::fill(l, l + n, 0.f);
   for (auto& v : voices) v.render(c, l, n, pressure);
   const float mix = smoothed[P::RevMix] * .01f;
   const float dryGain = std::cos(mix * 1.5707963f), wetGain = std::sin(mix * 1.5707963f) * 1.4f;
   const float drive = smoothed[P::Drive] * .01f;
   for (int i = 0; i < n; ++i) {
    // DC blocker (~10 Hz): the bow and the stone must not build an offset.
    const float in = l[i];
    dcOut = in - dcIn + dcR * dcOut;
    dcIn = in;
    float x = body.process(dcOut, bodyMix);
    if (drive > 0.f) x = detail::softTanh(x * (1.f + drive * 5.f)) / (1.f + drive * 1.5f) * 1.3f;
    x = toneFilter.process(x, toneCo[0], toneCo[1], toneCo[2], .9f, 0);
    float wl = 0.f, wr = 0.f;
    reverb.process(x, x, wl, wr);
    l[i] = inverso_abismo::softCeiling((x * dryGain + wl * wetGain) * outGain);
    r[i] = inverso_abismo::softCeiling((x * dryGain + wr * wetGain) * outGain);
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
  const float dt = static_cast<float>(controlInterval / sr);
  gourdPhase += smoothed[P::GourdRate] * dt; if (gourdPhase >= 1.f) gourdPhase -= 1.f;
  vibPhase += smoothed[P::VibRate] * dt; if (vibPhase >= 1.f) vibPhase -= 1.f;
  const float gourdLfo = smoothed[P::GourdRate] > 0.f ? detail::sine(gourdPhase) * smoothed[P::GourdDepth] * .005f : 0.f;
  const float gourd = std::clamp(smoothed[P::Gourd] * .01f + modWheel + gourdLfo, 0.f, 1.f);
  body.configure(c.body, gourd, smoothed[P::BodyTune]);
  for (auto& v : voices) if (v.isActive()) v.control(c, bend * c.bendRange, pressure, detail::sine(vibPhase));
 }

 void derive() {
  const auto& s = smoothed;
  auto pct = [&](int p) { return s[static_cast<std::size_t>(p)] * .01f; };
  auto choice = [&](int p) { return static_cast<int>(std::lround(s[static_cast<std::size_t>(p)])); };
  c.technique = std::clamp(choice(P::Technique), 0, 3);
  c.body = std::clamp(choice(P::Body), 0, 3);
  c.mono = choice(P::VoiceMode) == 1;
  c.decay = s[P::Decay];
  c.brightness = pct(P::Brightness);
  c.position = pct(P::Position);
  c.stiffness = pct(P::Stiffness);
  c.buzz = pct(P::Buzz);
  c.pressure = pct(P::Pressure);
  c.attackMs = s[P::Attack];
  c.releaseMs = s[P::Release];
  c.stoneBend = s[P::StoneBend];
  c.octaveSt = static_cast<float>((choice(P::Octave) - 2) * 12);
  c.tuneSt = s[P::Tune] * .01f;
  c.glideMs = s[P::Glide];
  c.bendRange = s[P::BendRange];
  c.vibrato = pct(P::Vibrato);
  c.vibRate = s[P::VibRate];
  c.sub = pct(P::Sub);
  c.velSens = pct(P::VelSens);
  c.caxixi = c.body == 0 ? pct(P::Caxixi) : 0.f; // the caxixi belongs to the berimbau
  // String material follows the instrument: steel wire, banjo string, washtub rope.
  static const float tensionMod[4] = {.006f, .012f, .04f, .008f};
  static const float darkness[4] = {0.f, 0.f, .35f, 0.f};
  c.tensionMod = tensionMod[c.body];
  c.darkness = darkness[c.body];
  bodyMix = c.body == 3 ? 0.f : pct(P::BodyMix);
  const float g = std::tan(3.14159265f * std::min(s[P::Tone], .45f * static_cast<float>(sr)) / static_cast<float>(sr));
  toneCo[0] = 1.f / (1.f + g * (g + .9f)); toneCo[1] = g * toneCo[0]; toneCo[2] = g * toneCo[1];
  reverb.configure(pct(P::RevSize), s[P::RevDecay], .5f, false);
  outGain = std::pow(10.f, s[P::Output] / 20.f);
 }

 double sr = 48000.0;
 Values target = defaults(), smoothed = defaults();
 Controls c;
 std::array<Voice, maxVoices> voices{};
 std::array<int, 16> held{};
 Body body;
 SVF toneFilter;
 inverso_abismo::Reverb reverb;
 int heldCount = 0, lastNote = -1, controlCountdown = 0;
 bool sustainPedal = false, pendingRelease = false;
 float bend = 0.f, modWheel = 0.f, pressure = 0.f, bodyMix = .8f, outGain = 1.f, gourdPhase = 0.f, vibPhase = 0.f;
 float toneCo[3] = {1.f, 0.f, 0.f};
 float dcIn = 0.f, dcOut = 0.f, dcR = .9987f;
};

} // namespace inverso_berimbass
