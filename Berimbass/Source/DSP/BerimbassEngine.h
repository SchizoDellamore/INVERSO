#pragma once
// INVERSO BERIMBASS engine: a physically modelled bass string (digital
// waveguide) with four ways to play it and three bodies to sound through.
// Portable C++17, no JUCE, no allocation after prepare().
//   Technique: Pizzicato (finger), Bambu (berimbau stick + its click),
//              Arco (bow), Pedra (stone / dobrao: buzz and a pitch scoop).
//   Body:      Cabaca (gourd "wah", mod wheel opens it), Banjo (skin head),
//              Balde (washtub), Nenhum (dry string).
// The loop: fractional delay -> one-pole damping -> two dispersion all-passes
// (stiffness) -> loss -> buzz (stone touching the string) -> back in.
// Loop delay is corrected by the filters' phase delay so notes stay in tune.
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

struct Controls {
 int technique = 1, body = 0;
 bool mono = true;
 float decay = 3.f, brightness = .6f, position = .15f, stiffness = .3f, buzz = .2f, pressure = .6f;
 float attackMs = 150.f, releaseMs = 250.f, stoneBend = 1.f, octaveSt = 0.f, tuneSt = 0.f, glideMs = 60.f;
 float bendRange = 2.f, vibrato = 0.f, vibRate = 5.f, sub = 0.f, velSens = .7f;
};

inline float phaseDelayLowpass(float a, float w) { return std::atan2(a * std::sin(w), 1.f - a * std::cos(w)) / w; }
inline float phaseDelayAllpass(float c, float w) {
 const float argN = std::atan2(-std::sin(w), c + std::cos(w));
 const float argD = std::atan2(-c * std::sin(w), 1.f + c * std::cos(w));
 return -(argN - argD) / w;
}

class Voice {
public:
 void prepare(double sampleRate, std::uint32_t seed) {
  sr = sampleRate;
  const auto len = static_cast<std::size_t>(sr / minFreq) + 8;
  line.assign(len, 0.f);
  excitation.assign(len, 0.f);
  rng.state = seed * 2654435761u + 31u;
  kill();
 }
 void kill() {
  std::fill(line.begin(), line.end(), 0.f);
  lp = ap1x = ap1y = ap2x = ap2y = 0.f;
  exciteLeft = 0; clickLeft = 0; active = false; gate = false; note = -1; follower = 0.f; silentFor = 0;
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
  if (c.technique == 2) { bowEnv.gateOn(); return; }
  bowEnv.reset();
  // Prepare one period of excitation, shaped by technique and strike position.
  updatePitch(c, 0.f, 0.f, 0.f);
  const int n = std::clamp(static_cast<int>(period), 8, static_cast<int>(excitation.size()) - 1);
  const bool stick = c.technique == 1 || c.technique == 3;
  const float hardness = std::clamp((stick ? .55f : .15f) + .45f * c.pressure * (.5f + .5f * vel), .05f, .98f);
  const int pulse = stick ? std::max(2, static_cast<int>(n * (.08f - .06f * hardness))) : n;
  float state = 0.f;
  for (int i = 0; i < n; ++i) {
   float x = 0.f;
   if (i < pulse) {
    const float w = detail::sine(.5f * static_cast<float>(i) / static_cast<float>(pulse)); // half-sine window
    state += hardness * (rng.next() - state);
    x = (stick ? (rng.next() * .4f + 1.f) : state * 2.5f) * w;
   }
   excitation[static_cast<std::size_t>(i)] = x;
  }
  // Comb at the strike position: removes harmonics with a node there.
  const int pos = std::clamp(static_cast<int>(c.position * static_cast<float>(n)), 1, n - 1);
  for (int i = n - 1; i >= pos; --i) excitation[static_cast<std::size_t>(i)] -= excitation[static_cast<std::size_t>(i - pos)];
  float peak = 1e-6f;
  for (int i = 0; i < n; ++i) peak = std::max(peak, std::abs(excitation[static_cast<std::size_t>(i)]));
  const float norm = .7f * amp / peak;
  for (int i = 0; i < n; ++i) excitation[static_cast<std::size_t>(i)] *= norm;
  exciteLength = n;
  exciteLeft = n;
  clickLeft = stick ? static_cast<int>(.004 * sr) : 0;
  clickGain = (c.technique == 1 ? .5f : .3f) * amp;
 }
 void release() { gate = false; bowEnv.gateOff(); }

 // Control rate: pitch, glide, vibrato, loop filters and loss.
 void control(const Controls& c, float bendSt, float pressure, float lfoVib) {
  const float dtMs = static_cast<float>(controlInterval * 1000.0 / sr);
  if (c.glideMs > 0.f && std::abs(glideOffset) > 1e-6f) {
   const float step = std::abs(glideStart) * dtMs / c.glideMs;
   glideOffset = glideOffset > 0.f ? std::max(0.f, glideOffset - step) : std::min(0.f, glideOffset + step);
  } else glideOffset = 0.f;
  if (scoop < 0.f) { scoopTime += dtMs; scoop = -c.stoneBend * std::max(0.f, 1.f - scoopTime / 80.f); }
  vibTime += dtMs;
  const float vib = lfoVib * c.vibrato * .5f * std::min(1.f, vibTime / 400.f);
  updatePitch(c, bendSt + glideOffset + scoop + pressure * c.stoneBend + vib, pressure, 0.f);
  bowEnv.set(c.attackMs, 100.f, 1.f, c.releaseMs, sr);
 }

 void render(const Controls& c, float* out, int n, float pressure) {
  if (!active) return;
  const int len = static_cast<int>(line.size());
  const float buzz = std::clamp(c.buzz + (c.technique == 3 ? .25f : 0.f) + pressure * .3f, 0.f, 1.f);
  const float threshold = .6f - .55f * buzz;
  for (int i = 0; i < n; ++i) {
   // Read the loop output with linear fractional delay.
   float pos = static_cast<float>(write) - delaySamples;
   while (pos < 0.f) pos += static_cast<float>(len);
   const int i0 = static_cast<int>(pos) % len, i1 = (i0 + 1) % len;
   const float frac = pos - std::floor(pos);
   float y = line[static_cast<std::size_t>(i0)] + frac * (line[static_cast<std::size_t>(i1)] - line[static_cast<std::size_t>(i0)]);
   const float outSample = y;
   lp = (1.f - lpCoef) * y + lpCoef * lp;
   y = lp;
   const float a1 = apCoef * y + ap1x - apCoef * ap1y; ap1x = y; ap1y = a1;
   const float a2 = apCoef * a1 + ap2x - apCoef * ap2y; ap2x = a1; ap2y = a2;
   y = a2 * loopGain;
   if (buzz > .01f && std::abs(y) > threshold) { // string bouncing off the stone
    const float excess = std::abs(y) - threshold;
    y = (y > 0.f ? 1.f : -1.f) * (threshold - excess * .6f) + rng.next() * excess * .1f;
   }
   float in = 0.f;
   if (exciteLeft > 0) { in += excitation[static_cast<std::size_t>(exciteLength - exciteLeft)]; --exciteLeft; }
   if (c.technique == 2) { // bow: steady rosin noise, shaped by the bow envelope
    const float e = bowEnv.next();
    bowNoise += (.08f + .25f * c.pressure) * (rng.next() - bowNoise);
    in += bowNoise * e * (.08f + .12f * c.pressure) * amp;
    if (!bowEnv.active() && !gate) bowNoise = 0.f;
   }
   line[static_cast<std::size_t>(write)] = y + in;
   write = (write + 1) % len;
   float o = outSample;
   if (clickLeft > 0) { // the stick hitting the string: a short bright "tch"
    clickHp += .5f * (rng.next() - clickHp);
    o += (rng.next() - clickHp) * clickGain * static_cast<float>(clickLeft) / static_cast<float>(.004 * sr);
    --clickLeft;
   }
   if (c.sub > 0.f) {
    follower += .001f * (std::abs(outSample) - follower);
    subPhase += subInc; if (subPhase >= 1.f) subPhase -= 1.f;
    o += detail::sine(subPhase) * follower * 2.2f * c.sub;
   }
   out[i] += o;
   silentFor = std::abs(o) < 1e-5f ? silentFor + 1 : 0;
  }
  age += n;
  if (!gate && !bowEnv.active() && silentFor > static_cast<int>(sr * .05)) active = false;
 }

private:
 void updatePitch(const Controls& c, float offsetSt, float, float) {
  const float semis = static_cast<float>(note) + c.octaveSt + c.tuneSt + offsetSt;
  const float f = std::clamp(440.f * detail::semitoneRatio(semis - 69.f), minFreq, .2f * static_cast<float>(sr));
  const float w = twoPi * f / static_cast<float>(sr);
  period = static_cast<float>(sr) / f;
  lpCoef = .02f + (1.f - c.brightness) * .8f;
  apCoef = -.6f * c.stiffness;
  // Loss per round trip for the requested decay (release while the key is up),
  // compensating the damping filter's own loss at the fundamental.
  const float t60 = gate || (c.technique == 2 && bowEnv.active()) ? c.decay : std::min(c.decay, c.releaseMs * .001f);
  const float lpMag = (1.f - lpCoef) / std::sqrt(1.f - 2.f * lpCoef * std::cos(w) + lpCoef * lpCoef);
  loopGain = std::min(.99995f, std::pow(10.f, -3.f / (std::max(.02f, t60) * f)) / lpMag);
  const float filterDelay = phaseDelayLowpass(lpCoef, w) + 2.f * phaseDelayAllpass(apCoef, w);
  delaySamples = std::clamp(period - filterDelay, 2.f, static_cast<float>(line.size() - 3));
  subInc = f * .5f / static_cast<float>(sr);
 }

 double sr = 48000.0;
 std::vector<float> line, excitation;
 detail::Random rng;
 Envelope bowEnv;
 int write = 0, exciteLeft = 0, exciteLength = 0, clickLeft = 0, silentFor = 0;
 bool active = false;
 float vel = 1.f, amp = 1.f, period = 100.f, delaySamples = 100.f, lpCoef = .3f, apCoef = 0.f, loopGain = .99f;
 float lp = 0.f, ap1x = 0.f, ap1y = 0.f, ap2x = 0.f, ap2y = 0.f, bowNoise = 0.f, clickHp = 0.f, clickGain = .4f;
 float glideOffset = 0.f, glideStart = 0.f, scoop = 0.f, scoopTime = 0.f, vibTime = 0.f;
 float follower = 0.f, subPhase = 0.f, subInc = 0.f;
};

// The three bodies, applied to the summed strings (they are linear).
class Body {
public:
 void prepare(double sampleRate) { sr = sampleRate; for (auto* f : {&bp, &lpf, &twang}) f->reset(); for (auto& m : modes) m.reset(); }
 void configure(int type, float gourdOpen, float tuneSt) {
  body = type;
  const float t = detail::semitoneRatio(tuneSt);
  if (type == 0) {
   setFilter(bpCo, 140.f * t * std::exp2(gourdOpen * 2.6f), .35f);
   setFilter(lpCo, 350.f * std::exp2(gourdOpen * 4.5f), 1.f);
   gourd = gourdOpen;
  } else if (type == 1) {
   static const float r[4] = {1.f, 1.59f, 2.14f, 2.65f};
   for (int i = 0; i < 4; ++i) modes[static_cast<std::size_t>(i)].set(330.f * t * r[i], .1f - .015f * static_cast<float>(i), sr);
   setFilter(lpCo, 1500.f, 1.f);
  } else if (type == 2) {
   static const float r[5] = {1.f, 1.5f, 2.27f, 3.1f, 4.2f};
   for (int i = 0; i < 5; ++i) modes[static_cast<std::size_t>(i)].set(80.f * t * r[i], .35f - .05f * static_cast<float>(i), sr);
   setFilter(lpCo, 1800.f, 1.f);
  }
 }
 float process(float x, float mix) {
  float y = x;
  switch (body) {
   case 0: { // gourd: the wah of opening and closing it against the belly
    const float low = lpf.process(x, lpCo.a1, lpCo.a2, lpCo.a3, lpCo.k, 0);
    const float peak = bp.process(x, bpCo.a1, bpCo.a2, bpCo.a3, bpCo.k, 1);
    y = low * .8f + peak * (.3f + 1.4f * gourd);
    break;
   }
   case 1: { // banjo head: short bright modes plus string twang
    float r = 0.f;
    for (int i = 0; i < 4; ++i) r += modes[static_cast<std::size_t>(i)].process(x) / (1.f + .5f * static_cast<float>(i));
    const float high = x - twang.process(x, lpCo.a1, lpCo.a2, lpCo.a3, lpCo.k, 0);
    y = (x * .55f + r * .9f + high * .7f) * .3f; // level-matched to the gourd
    break;
   }
   case 2: { // washtub: dark string into a hollow metal tub
    float r = 0.f;
    for (int i = 0; i < 5; ++i) r += modes[static_cast<std::size_t>(i)].process(x) / (1.f + .6f * static_cast<float>(i));
    y = (lpf.process(x, lpCo.a1, lpCo.a2, lpCo.a3, lpCo.k, 0) * .7f + r * 1.1f) * .2f; // level-matched to the gourd
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
 int body = 0;
 float gourd = .5f;
 Co bpCo, lpCo;
 SVF bp, lpf, twang;
 std::array<Resonator, 5> modes{};
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
   // Legato (another key still held): glide without re-plucking; otherwise strike again.
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
    // DC blocker (~10 Hz): injected bow / pluck noise must not build an offset.
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
